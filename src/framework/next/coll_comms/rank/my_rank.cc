/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "my_rank.h"
#include "hcomm_c_adpt.h"
#include "hcomm_res.h"
#include "channel.h"
#include "endpoint_pair.h"
#include "hccl_res.h"
#include "../common/loggers/channel_logger.h"  // 日志记录器
#include "hcclCommDfx.h"
#include "env_config/env_config.h"

using namespace hcomm;

namespace MyRankUtils {

HcommChannelDesc ChannelDescHccl2Hcomm(const HcclChannelDesc &hcclDesc)
{
    HcommChannelDesc hcommDesc;
    hcommDesc.remoteEndpoint = hcclDesc.remoteEndpoint;
    hcommDesc.notifyNum = hcclDesc.notifyNum;
    hcommDesc.memHandles = hcclDesc.memHandles;
    hcommDesc.memHandleNum = hcclDesc.memHandleNum;
    return hcommDesc;
}

} // namespace MyRankUtils

namespace hccl {

MyRank::MyRank(aclrtBinHandle binHandle, uint32_t rankId, const CommConfig& config, const ManagerCallbacks& callbacks)
    : binHandle_(binHandle), rankId_(rankId), config_(config), callbacks_(callbacks)
{
}

MyRank::~MyRank()
{
    HCCL_INFO("[MyRank][~MyRank] MyRank deinit");
    // 析构有时序要求
    rankPairMgr_ = nullptr; // 内部会销毁channel，可能需要返还endpoint与ccu资源
    endpointMgr_ = nullptr; // 内部会销毁endpoint，可能需要返回ccu资源
    ccuResContainer_ = nullptr;  // 内部清理CCU资源，关闭CCU通道
    commMems_ = nullptr;
}

HcclResult MyRank::Init(HcclMem cclBuffer, const uint32_t opExpansionMode, uint32_t rankNum)
{
    // EXCEPTION_HANDLE_BEGIN
    // 创建通信内存管理器
    EXECEPTION_CATCH(commMems_ = std::make_unique<CommMems>(config_.GetConfigBufferSize()), return HCCL_E_PTR);

    // 初始化通信内存
    CHK_RET(commMems_->Init(cclBuffer));

    EXECEPTION_CATCH(engineCtxs_ = std::make_unique<EngineCtxs>(), return HCCL_E_PTR);

    opExpansionMode_ = opExpansionMode;
    if (!ccuResContainer_ && rankNum != 1) {
        ccuResContainer_.reset(new (std::nothrow)CcuResContainer(opExpansionMode_));
        CHK_PTR_NULL(ccuResContainer_);
        CHK_RET(ccuResContainer_->Init());
    }

    // 创建端点管理器
    EXECEPTION_CATCH(endpointMgr_ = std::make_unique<hcomm::EndpointMgr>(), return HCCL_E_PTR);

    // rankPairMgr_初始化
    EXECEPTION_CATCH(rankPairMgr_ = std::make_unique<RankPairMgr>(), return HCCL_E_PTR);
    // EXCEPTION_HANDLE_END
    return HCCL_SUCCESS;
}


HcclResult MyRank::BatchCreateSockets(CommEngine engine, const HcclChannelDesc* channelDescs, uint32_t channelNum,
        const std::string &commTag, std::vector<HcommChannelDesc> &hcommDescs)
{
    CHK_PTR_NULL(channelDescs);
    CHK_PRT_RET(channelNum == 0,
        HCCL_ERROR("[%s] invalid param: channelNum is zero", __func__), HCCL_E_PARA);

    uint32_t localRank = rankId_;
    for (uint32_t i = 0; i < channelNum; ++i) {
        const EndpointDesc &localEndpointDesc = channelDescs[i].localEndpoint;
        const EndpointDesc &remoteEndpointDesc = channelDescs[i].remoteEndpoint;
        uint32_t remoteRank = channelDescs[i].remoteRank;
        HCCL_INFO("[%s][%u/%u] remoteRank[%u] localProtocol[%d] remoteProtocol[%d]",
            __func__, i + 1, channelNum, remoteRank, localEndpointDesc.protocol, remoteEndpointDesc.protocol
        );

        hcomm::EndpointPair* endpointPair = nullptr;
        RankIdPair rankIdPair = std::make_pair(localRank, remoteRank);
        EndpointDescPair endpointDescPair = std::make_pair(localEndpointDesc, remoteEndpointDesc);
        RankPair* rankPair = nullptr;
        CHK_RET(rankPairMgr_->Get(rankIdPair, rankPair));
        CHK_PTR_NULL(rankPair);
        CHK_RET(rankPair->GetEndpointPair(engine, endpointDescPair, endpointPair));
        CHK_PTR_NULL(endpointPair);

        Hccl::Socket* socket = nullptr;
        auto ret = endpointPair->GetSocket(commTag, socket);
        CHK_PRT_RET(ret != HCCL_SUCCESS,
            HCCL_ERROR("[%s] failed to get socket, channelIndex[%u], remoteRank[%u], protocol[%d]",
                __func__, i, remoteRank, localEndpointDesc.protocol),
            ret);
        CHK_PTR_NULL(socket);

        hcommDescs[i]  = MyRankUtils::ChannelDescHccl2Hcomm(channelDescs[i]);
        hcommDescs[i].socket = reinterpret_cast<HcommSocket>(socket);
        HCCL_INFO("[%s][%u/%u] socket created successfully, remoteRank[%u], socket[%p]",
            __func__, i + 1, channelNum, remoteRank, socket);
    }
    return HCCL_SUCCESS;
}

constexpr uint32_t MEM_HANDLE_NUM_MAX = 256;  // memHandleNum的默认限制最大为256

HcclResult MyRank::CheckChannelParam(CommEngine engine, const HcclChannelDesc &channelDesc, 
    uint32_t index)
{
    if (engine == COMM_ENGINE_AIV) {
        CHK_PRT_RET(
            (channelDesc.memHandleNum > MEM_HANDLE_NUM_MAX), 
            HCCL_ERROR("[%s]Channeldesc[%u] invalid memHandleNum, memHandleNum[%u], max channel num[%u]",
            __func__, index, channelDesc.memHandleNum, MEM_HANDLE_NUM_MAX), HCCL_E_PARA
        );
        CHK_PRT_RET(
            (channelDesc.memHandleNum != 0 && channelDesc.memHandles == nullptr), 
            HCCL_ERROR("[%s]Channeldesc[%u] invalid memHandles, memHandles is null", 
            __func__, index), HCCL_E_PARA
        );
    } else {
        if (channelDesc.memHandleNum != 0) {
            HCCL_WARNING("[%s]Channeldesc[%u] memHandleNum[%u] is non-zero, memHandle exchange is not supported.", 
                __func__, index, channelDesc.memHandleNum);
        }
    }
    return HCCL_SUCCESS;
}

HcclResult MyRank::BatchCreateChannels(CommEngine engine, const HcclChannelDesc* channelDescs, uint32_t channelNum,
        std::vector<HcommChannelDesc> &hcommDescs, ChannelHandle *channelHandles)
{
    CHK_PTR_NULL(channelDescs);
    CHK_PTR_NULL(channelHandles);
    CHK_PRT_RET(channelNum == 0,
        HCCL_ERROR("[%s] invalid param: channelNum is zero", __func__), HCCL_E_PARA);

    uint32_t localRank = rankId_;
    std::vector<HcclMem> memVec;
    CHK_SMART_PTR_NULL(commMems_);
    CHK_RET(commMems_->GetMemoryHandles(memVec));
    std::unordered_map<RankPair*, std::unordered_map<hcomm::EndpointPair*, u32>> reuseChannelIdxMap{};

    for (uint32_t i = 0; i < channelNum; ++i) {
        // 参数检查
        CHK_RET(CheckChannelParam(engine, channelDescs[i], i));

        const EndpointDesc &localEndpointDesc = channelDescs[i].localEndpoint;
        const EndpointDesc &remoteEndpointDesc = channelDescs[i].remoteEndpoint;
        uint32_t remoteRank = channelDescs[i].remoteRank;

        HCCL_INFO("[%s][%u/%u] remoteRank[%u] localProtocol[%d] remoteProtocol[%d] engine[%d]",
            __func__, i + 1, channelNum, remoteRank, localEndpointDesc.protocol, remoteEndpointDesc.protocol, engine
        );

        EndpointHandle epHandle = nullptr;
        CHK_PTR_NULL(endpointMgr_);
        auto ret = endpointMgr_->Get(localEndpointDesc, epHandle);
        CHK_PRT_RET(ret != HCCL_SUCCESS,
            HCCL_ERROR("[%s] failed to get endpoint, channelIndex[%u], remoteRank[%u], protocol[%d]",
                __func__, i, remoteRank, localEndpointDesc.protocol),
            ret);
        CHK_PTR_NULL(epHandle);

        HCCL_INFO("[%s][%u/%u] remoteRank[%u] epHandle[%p] protocol[%d]",
            __func__, i + 1, channelNum, remoteRank,
            epHandle, localEndpointDesc.protocol);

        // 注册内存
        std::vector<MemHandle> memHandleVec;
        std::vector<std::string> memTag;
        if (engine == COMM_ENGINE_AIV) {
            memVec.clear();
            CHK_RET(commMems_->GetTagMemoryHandles(channelDescs[i].memHandles, channelDescs[i].memHandleNum, memVec, memTag));
            HCCL_INFO("[%s][%u/%u] remoteRank[%u] got %zu user memory handles",
                __func__, i + 1, channelNum, remoteRank, memVec.size());
        } else {
            memTag.push_back("HcclBuffer");
        }

        ret = endpointMgr_->RegisterMemory(epHandle, memTag, memVec, memHandleVec);
        CHK_PRT_RET(ret != HCCL_SUCCESS,
            HCCL_ERROR("[%s] failed to register memory, channelIndex[%u], remoteRank[%u], memTagNum[%zu]",
                __func__, i, remoteRank, memTag.size()),
            ret);

        hcommDescs[i].exchangeAllMems = false;
        hcommDescs[i].memHandles = memHandleVec.data();
        hcommDescs[i].memHandleNum = memHandleVec.size();

        hcomm::EndpointPair* endpointPair = nullptr;
        RankIdPair rankIdPair = std::make_pair(localRank, remoteRank);
        EndpointDescPair endpointDescPair = std::make_pair(localEndpointDesc, remoteEndpointDesc);
        RankPair* rankPair = nullptr;
        CHK_RET(rankPairMgr_->Get(rankIdPair, rankPair));
        CHK_PTR_NULL(rankPair);
        CHK_RET(rankPair->GetEndpointPair(engine, endpointDescPair, endpointPair));
        CHK_PTR_NULL(endpointPair);

        if (reuseChannelIdxMap.find(rankPair) == reuseChannelIdxMap.end()) {
            std::unordered_map<hcomm::EndpointPair*, u32> endpointPair2Idx{};
            endpointPair2Idx.emplace(endpointPair, 0);
            reuseChannelIdxMap.emplace(rankPair, endpointPair2Idx);
        } else if (reuseChannelIdxMap[rankPair].find(endpointPair) == reuseChannelIdxMap[rankPair].end()) {
            reuseChannelIdxMap[rankPair].emplace(endpointPair, 0);
        }
        u32& reuseIdx = reuseChannelIdxMap[rankPair][endpointPair];
        ret = endpointPair->CreateChannel(epHandle, engine, reuseIdx, &hcommDescs[i], channelHandles + i);
        CHK_PRT_RET(ret != HCCL_SUCCESS,
            HCCL_ERROR("[%s] failed to create channel, channelIndex[%u], remoteRank[%u], engine[%d], reuseIndex[%u]",
                __func__, i + 1, remoteRank, engine, reuseIdx),
            ret);
        reuseIdx++;

        HCCL_INFO("[%s][%u/%u] channel created successfully, remoteRank[%u], channelHandle[%p]",
            __func__, i + 1, channelNum, remoteRank, channelHandles[i]);
    }

    return HCCL_SUCCESS;
}

HcclResult MyRank::BatchConnectChannels(const HcclChannelDesc* channelDescs, ChannelHandle *channelHandles, uint32_t channelNum)
{
    auto timeout = std::chrono::seconds(Hccl::EnvConfig::GetInstance().GetSocketConfig().GetLinkTimeOut());
    auto startTime = std::chrono::steady_clock::now();

    HCCL_INFO("[%s] start connecting channels, channelNum[%u], timeout[%lld]sec",
        __func__, channelNum, timeout);

    std::vector<int32_t> statusVec(channelNum, 0);
    int32_t* statusList = statusVec.data();
    uint32_t retryCount = 0;
    for (uint32_t i = 0; i < channelNum; ++i) {
        while (true) {
            HcclResult ret = HcommChannelGetStatus(channelHandles + i, 1, statusList + i);

            // 卫语句：先处理异常情况

            // 1. 检查超时
            if ((std::chrono::steady_clock::now() - startTime) >= timeout) {
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - startTime).count();
                HCCL_ERROR("[%s] channel connect timeout after %lld sec, channelNum[%u], elapsed[%lld]ms, retryCount[%u]",
                    __func__, timeout, channelNum, elapsed, retryCount);
                logger::ChannelLogger::PrintChannelErrorDetails(rankId_, channelNum, channelDescs, channelHandles, statusList, elapsed);
                return HCCL_E_TIMEOUT;
            }

            // 2. 处理重试（去除频繁的重试日志，一秒可能重试上千次）
            if (ret == HCCL_E_AGAIN) {
                retryCount++;
                continue;
            }

            // 3. 处理失败
            if (ret != HCCL_SUCCESS) {
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - startTime).count();
                HCCL_ERROR("[%s] channel connect failed, channelNum[%u], ret[%d], elapsed[%lld]ms, retryCount[%u]",
                    __func__, channelNum, ret, elapsed, retryCount);
                logger::ChannelLogger::PrintChannelErrorDetails(rankId_, channelNum, channelDescs, channelHandles, statusList, elapsed);
                return ret;
            }

            // 4. 正常情况：所有通道连接成功
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - startTime).count();
            HCCL_INFO("[%s] all channels connected successfully, channelNum[%u], elapsed[%lld]ms, retryCount[%u]",
                __func__, channelNum, elapsed, retryCount);
            break;
        }
    }
    return HCCL_SUCCESS;
}

HcclResult MyRank::CreateChannels(CommEngine engine, const std::string &commTag,
        const HcclChannelDesc* channelDescs, uint32_t channelNum, ChannelHandle *channelHandles)
{
    CHK_PTR_NULL(channelDescs);
    CHK_PTR_NULL(channelHandles);
    CHK_PRT_RET(channelNum == 0,
        HCCL_ERROR("[%s] invalid param: channelNum is zero", __func__), HCCL_E_PARA);

    HCCL_INFO("[CreateChannels][Enter] engine[%d] commTag[%s] channelNum[%u] rankId[%u]",
        engine, commTag.c_str(), channelNum, rankId_);

    std::vector<ChannelHandle> hostChannelHandles(channelNum);
    ChannelHandle* hostChannelHandleList = hostChannelHandles.data();

    std::vector<HcommChannelDesc> hcommDescs(channelNum);

    CHK_RET(BatchCreateSockets(engine, channelDescs, channelNum, commTag, hcommDescs));
    CHK_RET(BatchCreateChannels(engine, channelDescs, channelNum, hcommDescs, hostChannelHandleList));
    CHK_RET(BatchConnectChannels(channelDescs, hostChannelHandleList, channelNum));
    // 添加初始化时进行填表
    for (u32 i = 0; i < channelNum; ++i) {
        CHK_RET(CheckChannelParam(engine,channelDescs[i],i));
        u32 remoteRank = channelDescs[i].remoteRank;
        HcclCommDfx::AddChannelRemoteRankId(commTag, hostChannelHandleList[i], remoteRank);
    }

    if (engine == COMM_ENGINE_AICPU || engine == COMM_ENGINE_AICPU_TS) {
        // 新增：添加 kernelLaunchAicpuCommInit 调用
        if (!callbacks_.getAicpuCommState()) {
            HCCL_INFO("MyRank::%s kernelLaunchAicpuCommInit start.", __func__);
            HcclResult ret = callbacks_.kernelLaunchAicpuCommInit();
            CHK_PRT_RET(ret != HCCL_SUCCESS,
                HCCL_ERROR("[%s] kernelLaunchAicpuCommInit failed, return [%d].", __func__, ret), ret);
            callbacks_.setAicpuCommState(true);
        }

        CHK_RET(HcommChannelKernelLaunch(channelHandles, hostChannelHandleList, channelNum, commTag, binHandle_));
        return HCCL_SUCCESS;
    }

    if (engine == COMM_ENGINE_CPU || engine == COMM_ENGINE_CCU
        || engine == COMM_ENGINE_AIV) {
        // TODO: Host侧 Channel 赋值到 channelHandles
        CHK_SAFETY_FUNC_RET(memcpy_s(channelHandles,
            channelNum * sizeof(ChannelHandle),
            hostChannelHandleList,
            channelNum * sizeof(ChannelHandle)));
        return HCCL_SUCCESS;
    }

    HCCL_ERROR("[MyRank][%s] unsupported comm engine[%d].", __func__, engine);
    return HCCL_E_NOT_SUPPORT;
}

HcclResult MyRank::ChannelGetHcclBuffer(ChannelHandle channel, void **buffer, uint64_t *size)
{
    CHK_PTR_NULL(buffer);
    CHK_PTR_NULL(size);

    u32 memNum = 0;  // 接收内存块数量
    /* 实现获取buffer Num的接口，此处Size为10的vector暂存 */
    std::vector<HcommMem *> remoteMemList(10);
    std::vector<char *> memTags(10);
    CHK_RET(HcommChannelGetRemoteMem(channel, remoteMemList.data(), &memNum, memTags.data()));

    for (u32 i = 0; i < memNum; i++) {
        HCCL_INFO("%s memNum[%u] memTags[%s] size[%llu]", __func__, memNum, memTags[i], *size);
        if (strcmp(memTags[i], "HcclBuffer") == 0) {
            *buffer = remoteMemList[i]->addr;
            *size = remoteMemList[i]->size;
            HCCL_INFO("[%s] Found Hccl buffer memNum is %u at index %u: addr=%p, size=%llu",
                __func__,
                memNum,
                i,
                remoteMemList[i]->addr,
                remoteMemList[i]->size);
            break;  // 找到后立即退出循环
        }
    }
    return HCCL_SUCCESS;
}

HcclResult MyRank::ChannelGetRemoteMem(ChannelHandle channel, CommMem **remoteMem, char ***memTag, uint32_t *memNum)
{
    CHK_PTR_NULL(remoteMem);
    CHK_PTR_NULL(memTag);
    CHK_PTR_NULL(memNum);

    CHK_RET(HcommChannelGetUserRemoteMem(channel, remoteMem, memTag, memNum));
    // 添加空指针检查，防止返回的指针为空
    if (*memNum > 0) {
        CHK_PTR_NULL(*remoteMem);
        CHK_PTR_NULL(*memTag);
    }
    return HCCL_SUCCESS;
}
} // namespace hccl

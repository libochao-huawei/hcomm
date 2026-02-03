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

MyRank::MyRank(aclrtBinHandle binHandle, uint32_t rankId, const CommConfig& config)
    : binHandle_(binHandle), rankId_(rankId), config_(config)
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

HcclResult MyRank::Init(HcclMem cclBuffer, const uint32_t opExpansionMode)
{
    // EXCEPTION_HANDLE_BEGIN
    // 创建通信内存管理器
    EXECEPTION_CATCH(commMems_ = std::make_unique<CommMems>(config_.GetConfigBufferSize()), return HCCL_E_PTR);

    // 初始化通信内存
    CHK_RET(commMems_->Init(cclBuffer));

    opExpansionMode_ = opExpansionMode;
    if (!ccuResContainer_) {
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


HcclResult MyRank::BatchCreateSockets(const HcclChannelDesc* channelDescs, uint32_t channelNum,
        const std::string &commTag, std::vector<HcommChannelDesc> &hcommDescs)
{
    uint32_t localRank = rankId_;
    for (uint32_t i = 0; i < channelNum; ++i) {
        const EndpointDesc &localEndpointDesc = channelDescs[i].localEndpoint;
        const EndpointDesc &remoteEndpointDesc = channelDescs[i].remoteEndpoint;
        uint32_t remoteRank = channelDescs[i].remoteRank;
        HCCL_INFO("[%s] localProtocol[%d] remoteProtocol[%d]",
            __func__, localEndpointDesc.protocol, remoteEndpointDesc.protocol
        );
        
        hcomm::EndpointPair* endpointPair = nullptr;
        RankIdPair rankIdPair = std::make_pair(localRank, remoteRank);
        EndpointDescPair endpointDescPair = std::make_pair(localEndpointDesc, remoteEndpointDesc);
        RankPair* rankPair = nullptr;
        CHK_RET(rankPairMgr_->Get(rankIdPair, rankPair));
        CHK_PTR_NULL(rankPair);
        CHK_RET(rankPair->GetEndpointPair(endpointDescPair, endpointPair));

        Hccl::Socket* socket = nullptr;
        CHK_PTR_NULL(endpointPair);
        CHK_RET(endpointPair->GetSocket(commTag, socket));
        CHK_PTR_NULL(socket);
        hcommDescs[i]  = MyRankUtils::ChannelDescHccl2Hcomm(channelDescs[i]);
        hcommDescs[i].socket = reinterpret_cast<HcommSocket>(socket);
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
    uint32_t localRank = rankId_;
    std::vector<HcclMem> memVec;
    CHK_SMART_PTR_NULL(commMems_);
    CHK_RET(commMems_->GetMemoryHandles(memVec));

    for (uint32_t i = 0; i < channelNum; ++i) {
        // 参数检查
        CHK_RET(CheckChannelParam(engine, channelDescs[i], i));

        const EndpointDesc &localEndpointDesc = channelDescs[i].localEndpoint;
        const EndpointDesc &remoteEndpointDesc = channelDescs[i].remoteEndpoint;
        uint32_t remoteRank = channelDescs[i].remoteRank;

        HCCL_INFO("[%s] remoteRank[%d] localProtocol[%d] remoteProtocol[%d]",
            __func__, remoteRank, localEndpointDesc.protocol, remoteEndpointDesc.protocol
        );
        EndpointHandle epHandle = nullptr;
        CHK_PTR_NULL(endpointMgr_);
        CHK_RET(endpointMgr_->Get(localEndpointDesc, epHandle));
        CHK_PTR_NULL(epHandle);

        Endpoint* localEpPtr = reinterpret_cast<Endpoint*>(epHandle);
        HCCL_INFO("[%s] remoteRank[%d] epHandle->GetEndpointDesc() protocol[%d]",
            __func__, remoteRank, localEpPtr->GetEndpointDesc().protocol
        );
        
        // 注册内存
        std::vector<MemHandle> memHandleVec;
        std::vector<std::string> memTag;
        if (engine == COMM_ENGINE_AIV) {
            memVec.clear();
            CHK_RET(commMems_->GetTagMemoryHandles(channelDescs[i].memHandles, channelDescs[i].memHandleNum, memVec, memTag));
        } else {
            memTag.push_back("HcclBuffer");
        }
        CHK_RET(endpointMgr_->RegisterMemory(epHandle, memTag, memVec, memHandleVec));
        hcommDescs[i].exchangeAllMems = false;
        hcommDescs[i].memHandles = memHandleVec.data();
        hcommDescs[i].memHandleNum = memHandleVec.size();

        hcomm::EndpointPair* endpointPair = nullptr;
        RankIdPair rankIdPair = std::make_pair(localRank, remoteRank);
        EndpointDescPair endpointDescPair = std::make_pair(localEndpointDesc, remoteEndpointDesc);
        RankPair* rankPair = nullptr;
        CHK_RET(rankPairMgr_->Get(rankIdPair, rankPair));
        CHK_PTR_NULL(rankPair);
        CHK_RET(rankPair->GetEndpointPair(endpointDescPair, endpointPair));
        CHK_RET(endpointPair->CreateChannel(epHandle, engine, &hcommDescs[i], channelHandles + i));
    }

    return HCCL_SUCCESS;
}

HcclResult MyRank::BatchConnectChannels(ChannelHandle *channelHandles, uint32_t channelNum)
{
    // TODO: 从环境变量里面拿 timeoutSec
    constexpr uint32_t timeoutSec = 120;
    constexpr auto timeout = std::chrono::seconds(timeoutSec);
    auto startTime = std::chrono::steady_clock::now();

    std::vector<int32_t> statusVec(channelNum, 0);
    int32_t* statusList = statusVec.data();
    while (true) {
        // 1. HCCL_SUCCESS: 退出循环
        // 2. HCCL_E_AGAIN: 继续在 while true 内重试
        // 3. Others: 错误状态
        HcclResult ret = HcommChannelGetStatus(channelHandles, channelNum, statusList);
 
        if (ret == HCCL_E_AGAIN) {
            continue;
        }
        if ((std::chrono::steady_clock::now() - startTime) >= timeout) {
            HCCL_ERROR("[%s] channel connect timeout", __func__);
            return HCCL_E_TIMEOUT;
        }
        if (ret == HCCL_SUCCESS) {
            break;
        } else {
            HCCL_ERROR("[%s] Invalid Status[%d]", __func__, ret);
            return ret;
        }
    }
    return HCCL_SUCCESS;
}

HcclResult MyRank::CreateChannels(CommEngine engine, const std::string &commTag,
        const HcclChannelDesc* channelDescs, uint32_t channelNum, ChannelHandle *channelHandles)
{
    std::vector<ChannelHandle> hostChannelHandles(channelNum);
    ChannelHandle* hostChannelHandleList = hostChannelHandles.data();
 
    std::vector<HcommChannelDesc> hcommDescs(channelNum);
 
    // 1. 创建 Socket
    CHK_RET(BatchCreateSockets(channelDescs, channelNum, commTag, hcommDescs));
    
    // 2. 创建 Channel
    CHK_RET(BatchCreateChannels(engine, channelDescs, channelNum, hcommDescs, hostChannelHandleList));
 
    // 3. 连接 Channel
    CHK_RET(BatchConnectChannels(hostChannelHandleList, channelNum));
 
    // 4. channel kernel launch
    if (engine == COMM_ENGINE_AICPU || engine == COMM_ENGINE_AICPU_TS) {
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

HcclResult MyRank::ChannelGetRemoteMem(ChannelHandle channel, CommMem **remoteMem, char **memTag, uint32_t *memNum)
{
    CHK_PTR_NULL(remoteMem);
    CHK_PTR_NULL(memTag);
    CHK_PTR_NULL(memNum);
    CHK_PRT_RET(
        (*memNum == 0), HCCL_INFO("[%s] memNum is equal to 0, no need to get remoteMem",
        __func__, *memNum), HCCL_SUCCESS);

    uint32_t cclbufferNum = 1; // HcommChannelGetRemoteMem 会将cclbuffer也回传
    uint32_t totalMem = *memNum + cclbufferNum;
    std::vector<HcommMem *> remoteMemList(totalMem); 
    std::vector<char *> tmpMemTags(totalMem);
    CHK_RET(HcommChannelGetRemoteMem(channel, remoteMemList.data(), &totalMem, tmpMemTags.data()));
    *memNum = totalMem - cclbufferNum;

    uint32_t index = 0;
    for (u32 i = 0; i < totalMem; i++) {
        HCCL_INFO("%s memNum[%u] memTags[%s]", __func__, totalMem, tmpMemTags[i]);
        if (strcmp(tmpMemTags[i], "HcclBuffer") != 0) {
            (*remoteMem)[index].addr = remoteMemList[i]->addr;
            (*remoteMem)[index].size = remoteMemList[i]->size;
            (*remoteMem)[index].type = ConvertHcclToCommMemType(remoteMemList[i]->type);
            HCCL_INFO("[%s] Found memNum at index %u: addr=%p, size=%llu",
                __func__,
                index,
                remoteMemList[i]->addr,
                remoteMemList[i]->size);
            index++;
        }
    }
    return HCCL_SUCCESS;
}
} // namespace hccl

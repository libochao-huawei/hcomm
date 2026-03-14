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
#include "kfc.h"
#include "../common/loggers/channel_logger.h"  // 日志记录器
<<<<<<< HEAD
#include "../comms/api_c_adpt/hcomm_c_adpt.h"
=======
#include "env_config/env_config.h"
>>>>>>> fb75bdfa7380f4804c268281998cb59e2ee22435

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

        NsRecoveryData data{channelHandles, hostChannelHandleList, channelNum, commTag};
        nsRecoveryDatas_[engine].push_back(data);
        
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

void MyRank::SetKfcControlTransfer(std::shared_ptr<HDCommunicate> kfcControlTransferH2D, 
        std::shared_ptr<HDCommunicate> kfcStatusTransferD2H)
{
    kfcControlTransferH2D_ = kfcControlTransferH2D;
    kfcStatusTransferD2H_ = kfcStatusTransferD2H;
}

constexpr u32 WAIT_CMD_TIMEOUT = 10 * 1000; // 最大等待10秒
HcclResult MyRank::StopLaunch()
{
    for (const auto& recoveryData : nsRecoveryDatas_) {
        if (recoveryData.first == COMM_ENGINE_AICPU || recoveryData.first == COMM_ENGINE_AICPU_TS) {
            // Aicpu场景
            Hccl::KfcCommand opCmd = Hccl::KfcCommand::NS_STOP_LAUNCH;
            CHK_RET(kfcControlTransferH2D_->Put(0, sizeof(Hccl::KfcCommand), reinterpret_cast<uint8_t *>(&opCmd)));
            HCCL_INFO("[NsRecovery][Suspend] send KfcCommand[%d] success, which is NS_STOP_LAUNCH.", opCmd);
            Hccl::KfcExecStatus opInfo;
            auto timeout   = std::chrono::milliseconds(WAIT_CMD_TIMEOUT);
            auto startTime = std::chrono::steady_clock::now();
            while (true) {
                CHK_RET(kfcStatusTransferD2H_->Get(0, sizeof(Hccl::KfcExecStatus), reinterpret_cast<uint8_t *>(&opInfo)));
                if (opInfo.kfcStatus == Hccl::KfcStatus::STOP_LAUNCH_DONE) {
                    HCCL_INFO("[NsRecovery][Suspend] received KfcStatus[%d], which is STOP_LAUNCH_DONE", opInfo.kfcStatus);
                    return HcclResult::HCCL_E_SUSPENDING;
                } else if (opInfo.kfcStatus == Hccl::KfcStatus::ERROR){
                    HCCL_ERROR("[NsRecovery][Suspend] received KfcStatus[%d], which is ERROR", opInfo.kfcStatus);
                    return HcclResult::HCCL_E_INTERNAL;
                } else {
                    if((std::chrono::steady_clock::now() - startTime) >= timeout){
                        HCCL_ERROR("[NsRecovery][Suspend] Wait suspend response status timeout[%u ms] and get the opExecStatus is [%u].", WAIT_CMD_TIMEOUT,
                                opInfo.kfcStatus);
                        return HcclResult::HCCL_E_TIMEOUT;
                    }
                    continue;
                }
            }
            return HcclResult::HCCL_SUCCESS;  // todo：多CommEngine的管理存在问题
        } else {
            HCCL_INFO("[NsRecovery][Suspend] Aicpu kernel is not launched yet. Suspend host only.");
        }
    }
    
    return HcclResult::HCCL_SUCCESS;
}

HcclResult MyRank::Clean()
{
    ChannelTable channelTable = rankPairMgr_->GetChannelTable();
    std::vector<ChannelHandle> channelList;
    for (const auto& rankPair : channelTable) {
        for (const auto& endPointPair : rankPair.second) {
            channelList.insert(channelList.end(), endPointPair.second.begin(), endPointPair.second.end());
        }
    }

    if (channelList.empty()) {
        HCCL_INFO("[NsRecovery][Clean] Channel list empty, No need to clean!");
        return HcclResult::HCCL_SUCCESS;
    }

    auto ret = HcommChannelClean(channelList.data(), channelList.size());
    if (ret != HcclResult::HCCL_SUCCESS) {
        HCCL_ERROR("[NsRecovery][Clean] MyRank::Clean failed!");
        return ret;
    }

    for (const auto& recoveryData : nsRecoveryDatas_) {
        if (recoveryData.first == COMM_ENGINE_AICPU || recoveryData.first == COMM_ENGINE_AICPU_TS) {
            // 再清理device，后续优化全用host管理
            HCCL_INFO("[NsRecovery][Clean] start to clean device, waiting for device STOP_LAUNCH_DONE");
            Hccl::KfcExecStatus opInfo;
            CHK_RET(kfcStatusTransferD2H_->Get(0, sizeof(Hccl::KfcExecStatus), reinterpret_cast<uint8_t *>(&opInfo)));
            if (opInfo.kfcStatus == Hccl::KfcStatus::STOP_LAUNCH_DONE) {
                HCCL_INFO("[NsRecovery][Clean] received KfcStatus[%d], which is STOP_LAUNCH_DONE", opInfo.kfcStatus);
                // 通知背景线程清理device侧资源
                Hccl::KfcCommand opCmd = Hccl::KfcCommand::NS_CLEAN;
                CHK_RET(kfcControlTransferH2D_->Put(0, sizeof(Hccl::KfcCommand), reinterpret_cast<uint8_t *>(&opCmd)));
                HCCL_INFO("[NsRecovery][Clean] send KfcCommand [%d] success, which is NS_CLEAN", opCmd);
                // 监听背景线程状态
                auto timeout   = std::chrono::milliseconds(WAIT_CMD_TIMEOUT);
                auto startTime = std::chrono::steady_clock::now();
                while (true) {
                    CHK_RET(kfcStatusTransferD2H_->Get(0, sizeof(Hccl::KfcExecStatus), reinterpret_cast<uint8_t *>(&opInfo)));
                    if (opInfo.kfcStatus == Hccl::KfcStatus::CLEAN_DONE) {
                        HCCL_INFO("[NsRecovery][Clean] received KfcStatus[%d], which is CLEAN_DONE", opInfo.kfcStatus);
                        return HcclResult::HCCL_E_SUSPENDING;
                    } else if (opInfo.kfcStatus == Hccl::KfcStatus::ERROR){
                        HCCL_ERROR("[NsRecovery][Clean] received KfcStatus[%d], which is ERROR", opInfo.kfcStatus);
                        return HcclResult::HCCL_E_INTERNAL;
                    } else {
                        if ((std::chrono::steady_clock::now() - startTime) >= timeout) {
                            HCCL_ERROR("[NsRecovery][Clean] Wait clean response status timeout[%u ms] and get the opExecStatus is [%u].", WAIT_CMD_TIMEOUT,
                                    opInfo.kfcStatus);
                            return HcclResult::HCCL_E_TIMEOUT;
                        }
                        continue;
                    }
                }
            } else {
                HCCL_ERROR("[NsRecovery][Clean] Aicpu kernel is not stopped yet. Cannot clean.");
                return HcclResult::HCCL_E_INTERNAL;
            }
            return HcclResult::HCCL_SUCCESS;
        }
    }

    return HcclResult::HCCL_SUCCESS;
}

HcclResult MyRank::Resume()
{
    ChannelTable channelTable = rankPairMgr_->GetChannelTable();
    std::vector<ChannelHandle> channelList;
    for (const auto& rankPair : channelTable) {
        for (const auto& endPointPair : rankPair.second) {
            channelList.insert(channelList.end(), endPointPair.second.begin(), endPointPair.second.end());
        }
    }

    if (channelList.empty()) {
        HCCL_INFO("[NsRecovery][Clean] Resume list empty, No need to clean!");
        return HcclResult::HCCL_SUCCESS;
    }

    auto ret = HcommChannelResume(channelList.data(), channelList.size());
    if (ret != HcclResult::HCCL_SUCCESS) {
        HCCL_ERROR("[NsRecovery][Clean] MyRank::Clean failed!");
        return ret;
    }

    // 批量建链
    for (const auto& recoveryData : nsRecoveryDatas_) {
        if (recoveryData.first == COMM_ENGINE_AICPU || recoveryData.first == COMM_ENGINE_AICPU_TS) {
            // KernelLaunch时会将CollCommAicpu下的ubTransportMap_链路恢复,只打包transport
            for (const auto& handleData : recoveryData.second) {
                CHK_RET(HcommChannelKernelLaunch(handleData.channelHandles_, handleData.hostChannelHandleList_, 
                handleData.channelNum_, handleData.commTag_, binHandle_));
            }
        }
    }
    return HCCL_SUCCESS;
}

} // namespace hccl

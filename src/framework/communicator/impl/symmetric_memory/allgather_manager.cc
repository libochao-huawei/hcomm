/*
 * Copyright (c) 2024-2025 Huawei Technologies Co., Ltd. All Rights Reserved.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "allgather_manager.h"
#include <chrono>

namespace hccl {
using namespace std;

const string STR_IPC_MEM_EXCHANGE = "Exchange_DATA";
constexpr u32 USLEEP_ONE_THOUSAND = 1000;

AllGatherManager::AllGatherManager(const std::unique_ptr<HcclSocketManager> &socketManager, u32 devicePhyId,
    s32 deviceLogicId, const HcclIpAddress &localVnicIp, const std::vector<RankInfo> &rankInfoList, u32 userRank,
    bool useSuperPodMode, const std::string &identifier)
    : socketManager_(socketManager), devicePhyId_(devicePhyId), deviceLogicId_(deviceLogicId),
      localVnicIp_(localVnicIp), rankInfoList_(rankInfoList), userRank_(userRank), rankSize_(rankInfoList.size()),
      useSuperPodMode_(useSuperPodMode), identifier_(identifier)
{
    leftRank_ = (userRank_ - 1 + rankSize_) % rankSize_;
    rightRank_ = (userRank_ + 1) % rankSize_;
}

AllGatherManager::~AllGatherManager() {
    threadRun_ = false;
    if (recvThread_ && recvThread_->joinable()) {
        recvThread_->join();
        recvThread_ = nullptr;
    }
    if (vnicPortCtx_ != nullptr) {
        HcclNetCloseDev(vnicPortCtx_);
        vnicPortCtx_ = nullptr;
    }
}

HcclResult AllGatherManager::Init() {
    CHK_PRT_RET(rankSize_ < 2, HCCL_ERROR("[AllGatherManager][Init] single rank communicator"), HCCL_E_PARA);
    CHK_RET(EstablishSockets());
    CHK_RET(InitRecvThread());
    return HCCL_SUCCESS;
}

HcclResult AllGatherManager::InitRecvThread() {
    threadRun_ = true;
    recvThread_.reset(new (std::nothrow) std::thread(&AllGatherManager::DealWithRequest, this));
    CHK_SMART_PTR_NULL(recvThread_);
    return HCCL_SUCCESS;
}

HcclResult AllGatherManager::EstablishSockets()
{
    CHK_PRT_RET((vnicPortCtx_ != nullptr),
        HCCL_ERROR("[AllGatherManager][Init] already initd"), HCCL_E_PARA);
    CHK_RET(HcclNetOpenDev(&vnicPortCtx_, NicType::VNIC_TYPE, devicePhyId_, deviceLogicId_, localVnicIp_));
    CHK_PTR_NULL(vnicPortCtx_);

    HCCL_INFO("[AllGatherManager][EstablishSockets] userRank[%u], leftRank_[%u], rightRank_[%u], rankSize_[%u]",
        userRank_, leftRank_, rightRank_, rankSize_);
    for (size_t i = 0; i < rankInfoList_.size(); i++) {
        if (rankInfoList_[i].userRank == leftRank_ || rankInfoList_[i].userRank == rightRank_) {
            HcclRankLinkInfo remoteLinkInfo;
            RankInfo dstRankInfo = rankInfoList_[i];
            remoteLinkInfo.userRank = dstRankInfo.userRank;
            remoteLinkInfo.devicePhyId = dstRankInfo.devicePhyId;
            remoteLinkInfo.ip = HcclIpAddress(dstRankInfo.devicePhyId);
            if (useSuperPodMode_) {
                CHK_RET(hrtRaGetSingleSocketVnicIpInfo(devicePhyId_, DeviceIdType::DEVICE_ID_TYPE_SDID,
                    dstRankInfo.superDeviceId, remoteLinkInfo.ip));
            } else {
                CHK_RET(hrtRaGetSingleSocketVnicIpInfo(devicePhyId_, DeviceIdType::DEVICE_ID_TYPE_PHY_ID,
                    dstRankInfo.devicePhyId, remoteLinkInfo.ip));
            }
            // 通信域未分配端口则使用默认端口
            remoteLinkInfo.port =
                dstRankInfo.deviceVnicPort == HCCL_INVALID_PORT ? HETEROG_CCL_PORT : dstRankInfo.deviceVnicPort;
            remoteLinkInfo.socketsPerLink = 1;
            string newTag = GenerateSocketTag(devicePhyId_, rankInfoList_[i].devicePhyId);
            std::vector<std::shared_ptr<HcclSocket> > tmpSockets;
            HcclResult ret = socketManager_->CreateSingleLinkSocket(
                newTag, vnicPortCtx_, remoteLinkInfo, tmpSockets, false, true);
            CHK_PRT_RET(ret != HCCL_SUCCESS, HCCL_ERROR("[Create][DestSockets]Create single link sockets failed, "
                "local rank[%u], remote rank[%u]", userRank_, rankInfoList_[i].userRank), ret);
            if (tmpSockets.size() != 1) {
                HCCL_ERROR("[AllGatherManager][CreateVnic] socket number[%llu] is not 1 as expected!", tmpSockets.size());
                return HCCL_E_INTERNAL;
            }
            // 设置强制断链为关闭，避免进程退出时recv失败
            tmpSockets[0]->SetForceClose(false);
            mapRankIdconnectedSockets_[remoteLinkInfo.userRank] = (tmpSockets[0]);
            mapRankId2DevPhyId_[remoteLinkInfo.userRank] = remoteLinkInfo.devicePhyId;
        }
    }

    for (const auto& kv : mapRankIdconnectedSockets_) {
        CHK_PRT_RET(socketManager_->WaitLinkEstablish(kv.second) != HCCL_SUCCESS,
            HCCL_ERROR("[AllGatherManager][EstablishSockets] tag[%s] socket establish failed", kv.second->GetTag().c_str()),
            HCCL_E_INTERNAL);
    }
    return HCCL_SUCCESS;
}

std::string AllGatherManager::GenerateSocketTag(u32 localRank, u32 remoteRank)
{
    u32 small = localRank;
    u32 large = remoteRank;

    if (localRank > remoteRank) {
        small = remoteRank;
        large = localRank;
    }

    // Socket构造规则：前缀 + identifier + small + large
    std::string tag = STR_IPC_MEM_EXCHANGE + "_" + identifier_ 
        + "_" + std::to_string(small) + ":" + std::to_string(large);
    return tag;
}

// ---------------------------------------------------------------------
// 核心接口: AllGather
// ---------------------------------------------------------------------
HcclResult AllGatherManager::AllGather(void *inputPtr, void *outputPtr, u64 inputSize)
{
    CHK_PTR_NULL(inputPtr);
    CHK_PTR_NULL(outputPtr);
    CHK_PRT_RET(inputSize == 0, HCCL_ERROR("Input size is 0"), HCCL_E_PARA);
    // 校验 inputSize 是否超过协议载荷上限
    CHK_PRT_RET(inputSize > PACKET_DATA_MAX_LEN, 
        HCCL_ERROR("Input size %lu exceeds max payload %u", inputSize, PACKET_DATA_MAX_LEN), HCCL_E_PARA);
    // 校验是否建链成功
    CHK_PRT_RET(mapRankIdconnectedSockets_.find(rightRank_) == mapRankIdconnectedSockets_.end(),
        HCCL_ERROR("[AllGather] rightRank_%u socket not found in map", rightRank_), HCCL_E_INTERNAL);
    CHK_PRT_RET(mapRankIdconnectedSockets_.find(leftRank_) == mapRankIdconnectedSockets_.end(),
        HCCL_ERROR("[AllGather] leftRank_%u socket not found in map", leftRank_), HCCL_E_INTERNAL);

    HCCL_INFO("[AllGatherManager] start to AllGather, inputPtr[%p], outputPtr[%p], inputSize[%llu]", inputPtr, outputPtr, inputSize);
    
    // 重置本轮状态
    outputDataPtr_ = static_cast<u8*>(outputPtr);
    currentInputSize_ = inputSize; // 记录实际有效长度
    collectedCount_ = 0;
    // 本地数据处理：先把自己的一份拷到 Output 对应位置
    u8* selfDstPtr = outputDataPtr_ + (userRank_ * inputSize);
    CHK_SAFETY_FUNC_RET(memcpy_s(selfDstPtr, inputSize, inputPtr, inputSize));
    collectedCount_++;

    Packet dataPkt;
    dataPkt.type = MsgType::MSG_TYPE_DATA;
    dataPkt.rankId = userRank_;
    // 只拷贝有效部分，Packet 剩余部分由构造函数里的 memset 0 处理
    CHK_SAFETY_FUNC_RET(memcpy_s(dataPkt.data, PACKET_DATA_MAX_LEN, inputPtr, inputSize));
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        requestQueue_.push(dataPkt);
    }
    isProcessingTask_ = true;

    CHK_RET(WaitForCollectionComplete());
    HCCL_INFO("[AllGatherManager] AllGather end");
    return HCCL_SUCCESS;
}

// ---------------------------------------------------------------------
// 独立函数: 等待完成
// ---------------------------------------------------------------------
HcclResult AllGatherManager::WaitForCollectionComplete()
{
    std::unique_lock<std::mutex> lock(completionMutex_);
    
    auto timeout = std::chrono::seconds(GetExternalInputHcclLinkTimeOut());
    completionCv_.wait_for(lock, timeout);
    if (collectedCount_ != rankSize_) {
        HCCL_ERROR("[AllGatherManager] AllGather Timeout! Collected: %u/%u", 
            collectedCount_.load(), rankSize_);
        return HCCL_E_TCP_TRANSFER;
    }

    return HCCL_SUCCESS;
}

void AllGatherManager::DealWithRequest()
{
    if (hrtSetDevice(deviceLogicId_) != HCCL_SUCCESS) {
        return;
    }

    std::vector<u8> leftRecvBuf(PACKET_TOTAL_LEN, 0);
    u32 leftRecvLen = 0;

    while (threadRun_) {
        if (isProcessingTask_) {
            if (collectedCount_ < rankSize_) {
                u64 received = 0;
                std::unique_lock<std::mutex> lock(socketMutex_);
                HcclResult ret = mapRankIdconnectedSockets_[leftRank_]->IRecv(
                    leftRecvBuf.data() + leftRecvLen, PACKET_TOTAL_LEN - leftRecvLen, received);
                
                CHK_PRT_CONT((ret != HCCL_SUCCESS) && (ret != HCCL_E_AGAIN),
                    HCCL_ERROR("[AllGatherManager][DealWithRequest] IRecv failed, ret[%d] remoteRank[%u] receivedSize[%llu]",
                    ret, leftRank_, leftRecvLen));

                leftRecvLen += received;
                if (leftRecvLen == PACKET_TOTAL_LEN) {
                    Packet* pkt = reinterpret_cast<Packet*>(leftRecvBuf.data());
                    ProcessReceivedPacket(*pkt);
                    leftRecvLen = 0;
                }
            }
            std::lock_guard<std::mutex> lock(queueMutex_);
            if (!requestQueue_.empty()) {
                Packet pkt = requestQueue_.front();
                std::unique_lock<std::mutex> sockLock(socketMutex_);
                HcclResult ret = mapRankIdconnectedSockets_[rightRank_]->Send((u8*)&pkt, PACKET_TOTAL_LEN);
                if (ret == HCCL_SUCCESS) {
                    requestQueue_.pop();
                }else {
                    HCCL_ERROR("[AllGatherManager][DealWithRequest] Data(from rank[%u]) Send to rank[%u] failed.", pkt.rankId, rightRank_);
                }
            }
            // 检查是否完全结束, 退出条件: 数据全齐 && 队列空闲
            if (requestQueue_.empty() && collectedCount_ == rankSize_) {
                std::unique_lock<std::mutex> lock(completionMutex_);
                HCCL_INFO("[AllGatherManager] AllGather Complete.");
                isProcessingTask_ = false;
                completionCv_.notify_all();
            }
        }
        SaluSleep(USLEEP_ONE_THOUSAND);
    }
    
    hrtResetDevice(deviceLogicId_);
}

HcclResult AllGatherManager::ProcessReceivedPacket(Packet& pkt) {
    if (pkt.rankId < rankSize_) {
        u8* dest = outputDataPtr_ + (pkt.rankId * currentInputSize_);
        CHK_SAFETY_FUNC_RET(memcpy_s(dest, currentInputSize_, pkt.data, currentInputSize_));
        collectedCount_++;
    }
    HCCL_INFO("[AllGatherManager][ProcessReceivedPacket] Data Recv from rank[%u]. Collected[%u / %u].",
        pkt.rankId, collectedCount_.load(), rankSize_);
    // Ring 转发逻辑：如果数据不是自己的，也不是右边Rank发出的(转了一圈)，则转发给右边
    if (pkt.rankId != userRank_ && pkt.rankId != rightRank_) {
        std::lock_guard<std::mutex> lock(queueMutex_);
        requestQueue_.push(pkt);
    }
    return HCCL_SUCCESS;
}
} // namespace hccl
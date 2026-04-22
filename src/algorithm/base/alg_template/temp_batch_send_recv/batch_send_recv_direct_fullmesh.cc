/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "batch_send_recv_direct_fullmesh.h"
#include "dispatcher_pub.h"

namespace hccl {

BatchSendRecvDirectFullMesh::BatchSendRecvDirectFullMesh(const HcclDispatcher dispatcher)
    : AlgTemplateBase(dispatcher)
{
}

BatchSendRecvDirectFullMesh::~BatchSendRecvDirectFullMesh() {}

HcclResult BatchSendRecvDirectFullMesh::SetBatchSendRecvInfo(const BatchSendRecvInfo* batchSendRecvInfo)
{
    batchSendRecvInfoPtr_ = batchSendRecvInfo;
    return HCCL_SUCCESS;
}

HcclResult BatchSendRecvDirectFullMesh::GenerateSubStreamInfo(const std::vector<Stream> &subStreams,
    const std::vector<std::shared_ptr<LocalNotify>> &meshSignalMainToSub,
    const std::vector<std::shared_ptr<LocalNotify>> &meshSignalSubToMain)
{
    u32 totalSubstreamSize = (totalRdmaRankNum_ > 0) ?
        (sdmaConcurrentNum_ + rdmaConcurrentNum_ + 1) : (sdmaConcurrentNum_);
    if (subStreams.size() < totalSubstreamSize || meshSignalMainToSub.size() < totalSubstreamSize ||
        meshSignalSubToMain.size() < totalSubstreamSize) {
        HCCL_ERROR("[BatchSendRecvDirectFullMesh][GenerateSubStreamInfo]subStreamsSize[%zu], "
            "meshSignalMainToSubSize[%zu] meshSignalSubToMainSize[%zu] is smaller than totalSubstreamSize[%u]",
            subStreams.size(), meshSignalMainToSub.size(), meshSignalSubToMain.size(), totalSubstreamSize);
        return HCCL_E_PARA;
    }
    CHK_PRT_RET(links_.size() < userRankSize_, 
        HCCL_ERROR("[BatchSendRecvDirectFullMesh][GenerateSubStreamInfo]links_.size()[%zu] is smaller than userRankSize_[%u].",
            links_.size(), userRankSize_), HCCL_E_PARA);

    u32 index = 0;
    for (u32 sdmaIndex = 0; sdmaIndex < sdmaConcurrentNum_; sdmaIndex++) {
        sdmaSubStream_.push_back(subStreams[index]);
        sdmaMeshSignalMainToSub_.push_back(meshSignalMainToSub[index]);
        sdmaMeshSignalSubToMain_.push_back(meshSignalSubToMain[index]);
        index++;
    }
    for (u32 localIndex = 0; localIndex < sdmaConcurrentNum_; localIndex++) {
        localSubStream_.push_back(subStreams[index]);
        localSignalMainToSub_.push_back(meshSignalMainToSub[index]);
        localSignalSubToMain_.push_back(meshSignalSubToMain[index]);
        index++;
    }
    if (totalRdmaRankNum_ > 0) {
        rdmaSubStreams_.push_back(subStreams[index]);
        main2RdmaControlStreamNotify_ = meshSignalMainToSub[index];
        rdmaControl2MainStreamNotify_ = meshSignalSubToMain[index];
        index++;
        for (u32 rdmaIndex = 0; rdmaIndex < rdmaConcurrentNum_; rdmaIndex++) {
            rdmaSubStreams_.push_back(subStreams[index]);
            rdmaControl2SubNotifies_.push_back(meshSignalMainToSub[index]);
            rdmaSub2ControlNotifies_.push_back(meshSignalSubToMain[index]);
            index++;
        }
    }
    return HCCL_SUCCESS;
}

HcclResult BatchSendRecvDirectFullMesh::Prepare(PrepareData &param)
{
    mainStream_ = param.stream;
    userRank_ = param.userRank;
    userRankSize_ = param.userRankSize;
    links_ = *param.linksPtr;
    devNumInlocalPod_ = param.devNumInlocalPod;
    rankIdxInPod_ = param.rankIdxInPod;
    isSuPodAsym_ = param.isSuPodAsym;

    podStartRank_ = userRank_ - rankIdxInPod_;
    podEndRank_ = podStartRank_ + devNumInlocalPod_ - 1;
    sdmaConcurrentNum_ = (devNumInlocalPod_ > BATCH_SEND_RECV_DIRECT_FULLMESH_SDMA_CONCURRENT_SIZE) ?
        (BATCH_SEND_RECV_DIRECT_FULLMESH_SDMA_CONCURRENT_SIZE) : (devNumInlocalPod_);

    totalRdmaRankNum_ = userRankSize_ - devNumInlocalPod_;
    rdmaConcurrentNum_ = (totalRdmaRankNum_ > BATCH_SEND_RECV_DIRECT_FULLMESH_RDMA_CONCURRENT_SIZE) ?
        (BATCH_SEND_RECV_DIRECT_FULLMESH_RDMA_CONCURRENT_SIZE) : (totalRdmaRankNum_);

    HCCL_DEBUG("[BatchSendRecvDirectFullMesh]devNumInlocalPod_[%u], userRankSize_[%u] podStartRank_[%u] "
        "podEndRank_[%u], totalRdmaRankNum_[%u], sdmaConcurrentNum_[%u], rdmaConcurrentNum_[%u]",
        devNumInlocalPod_, userRankSize_, podStartRank_, podEndRank_, totalRdmaRankNum_,
        sdmaConcurrentNum_, rdmaConcurrentNum_);

    CHK_PRT_RET(userRankSize_ == 0, HCCL_ERROR("[BatchSendRecvDirectFullMesh][Prepare]userRankSize_ is zero."),
        HCCL_E_PARA);

    cclInMem_ = param.cclInMem;
    cclOutMem_ = param.cclOutMem;
    workMode_ = param.workMode;

    u64 maxSendLen = CalcMaxSendLen();
    isBigCount_ = (maxSendLen > BATCH_SEND_RECV_DIRECT_FULLMESH_BIG_SIZE) ? true : false;
    CHK_RET(GenerateSubStreamInfo(*param.subStreamsPtr, *param.signalPtr, *param.signalAuxPtr));

    u32 blockGroup = isBigCount_ ? 2 : 1;
    sdmaDataBlockSize_ = (cclInMem_.size() / std::max(1u, sdmaConcurrentNum_ * blockGroup));
    if (sdmaDataBlockSize_ > HCCL_MIN_SLICE_ALIGN_910B) {
        sdmaDataBlockSize_ = (sdmaDataBlockSize_ / HCCL_MIN_SLICE_ALIGN_910B) * HCCL_MIN_SLICE_ALIGN_910B;
    }
    CHK_PRT_RET(sdmaDataBlockSize_ == 0, 
        HCCL_ERROR("[BatchSendRecvDirectFullMesh][Prepare]sdmaDataBlockSize_ is zero."), HCCL_E_INTERNAL);

    HCCL_DEBUG("[BatchSendRecvDirectFullMesh][Prepare] userRank [%u] total cclsize[%llu], "
        "sdmaDataBlockSize_[%llu], BigCountFlag[%d]", userRank_, cclInMem_.size(), sdmaDataBlockSize_, isBigCount_);

    rdmaDataBlockSize_ = cclOutMem_.size() / std::max(1u, rdmaConcurrentNum_) / 2;

    return HCCL_SUCCESS;
}

std::string BatchSendRecvDirectFullMesh::GetStreamIndexString()
{
    std::string res = "";
    for (auto& info : subStreamReadInfo_) {
        u32 destRank = info.first;
        u32 streamIndex = destRank % sdmaConcurrentNum_;
        res += std::to_string(streamIndex) + ", ";
    }
    return res;
}

u64 BatchSendRecvDirectFullMesh::CalcMaxSendLen()
{
    u64 maxSendLen = 0;
    const BatchSendRecvInfo& batchSendRecvInfo = *batchSendRecvInfoPtr_;

    for (const auto& item : batchSendRecvInfo.items) {
        maxSendLen = std::max(maxSendLen, item.sendLength);
    }

    HCCL_DEBUG("[BatchSendRecvDirectFullMesh][CalcMaxSendLen] maxSendLen[%llu]", maxSendLen);
    return maxSendLen;
}

HcclResult BatchSendRecvDirectFullMesh::UpdateCurrRankRecvInfo(u32 step, u32 roundIdx, u32 side, u32 destRank,
    std::vector<ReadDataBlock>& readInfo, u32 maxRecvStep)
{
    const BatchSendRecvInfo& batchSendRecvInfo = *batchSendRecvInfoPtr_;
    
    auto it = std::find_if(batchSendRecvInfo.items.begin(), batchSendRecvInfo.items.end(),
        [destRank](const BatchSendRecvItem& item) { return item.remoteRank == destRank; });
    
    if (it == batchSendRecvInfo.items.end()) {
        return HCCL_SUCCESS;
    }

    u64 remainRecvLen = it->recvLength;
    u64 scratchOffset = 0;
    u32 bufferIdx = 0;
    u32 pairNum = sdmaConcurrentNum_ / RANK_SET_COMPUTE_CONST;
    
    if (sdmaConcurrentNum_ == 1) {
        bufferIdx = 0;
    } else if (side == 0) {
        u32 gap = (userRank_ - destRank + devNumInlocalPod_) % devNumInlocalPod_;
        bufferIdx = pairNum - (gap - roundIdx * pairNum);
    } else if (side == 1) {
        u32 gap = (destRank - userRank_ + devNumInlocalPod_) % devNumInlocalPod_;
        bufferIdx = pairNum - 1 + (gap - roundIdx * pairNum);
    } else {
        bufferIdx = 0;
    }

    if (isBigCount_ && (roundIdx % RANK_SET_COMPUTE_CONST != 0)) {
        bufferIdx += sdmaConcurrentNum_;
    }

    scratchOffset = bufferIdx * sdmaDataBlockSize_;

    u32 recvStepIdx = 0;
    u64 dataOffset = 0;
    u64 userOutOffset = 0;

    while (recvStepIdx < maxRecvStep && remainRecvLen > 0) {
        u64 currDataRemainLen = it->recvLength - dataOffset;
        u64 recvLen = std::min(sdmaDataBlockSize_, currDataRemainLen);
        readInfo.push_back({recvLen, scratchOffset, userOutOffset});
        dataOffset += recvLen;
        recvStepIdx++;
        remainRecvLen -= recvLen;
    }

    return HCCL_SUCCESS;
}

HcclResult BatchSendRecvDirectFullMesh::UpdateCurrRankSendInfo(u32 step, u32 roundIdx, u32 side, u32 destRank,
    std::vector<SendDataBlock>& sendInfo, u32 maxSendStep)
{
    const BatchSendRecvInfo& batchSendRecvInfo = *batchSendRecvInfoPtr_;
    
    auto it = std::find_if(batchSendRecvInfo.items.begin(), batchSendRecvInfo.items.end(),
        [destRank](const BatchSendRecvItem& item) { return item.remoteRank == destRank; });
    
    if (it == batchSendRecvInfo.items.end()) {
        return HCCL_SUCCESS;
    }

    u64 remainSendLen = it->sendLength;
    u64 scratchOffset = 0;
    u32 bufferIdx = 0;
    u32 pairNum = sdmaConcurrentNum_ / RANK_SET_COMPUTE_CONST;
    
    if (sdmaConcurrentNum_ == 1) {
        bufferIdx = 0;
    } else if (side == 0) {
        u32 gap = (userRank_ - destRank + devNumInlocalPod_) % devNumInlocalPod_;
        bufferIdx = pairNum - 1 + (gap - roundIdx * pairNum);
    } else if (side == 1) {
        u32 gap = (destRank - userRank_ + devNumInlocalPod_) % devNumInlocalPod_;
        bufferIdx = pairNum - (gap - roundIdx * pairNum);
    } else {
        bufferIdx = 0;
    }

    if (isBigCount_ && (roundIdx % RANK_SET_COMPUTE_CONST != 0)) {
        bufferIdx += sdmaConcurrentNum_;
    }
    scratchOffset = bufferIdx * sdmaDataBlockSize_;

    u32 sendStepIdx = 0;
    u64 dataOffset = 0;
    u64 userInOffset = 0;

    while (sendStepIdx < maxSendStep && remainSendLen > 0) {
        u64 currDataRemainLen = it->sendLength - dataOffset;
        u64 sendLen = std::min(sdmaDataBlockSize_, currDataRemainLen);
        sendInfo.push_back({sendLen, userInOffset, scratchOffset});
        dataOffset += sendLen;
        sendStepIdx++;
        remainSendLen -= sendLen;
    }

    return HCCL_SUCCESS;
}

void BatchSendRecvDirectFullMesh::UpdateSendRecvInfo(u32 step, u32 roundIdx,
    std::unordered_map<u32, std::vector<ReadDataBlock>> &subStreamReadInfo,
    std::unordered_map<u32, std::vector<SendDataBlock>> &subStreamSendInfo,
    const std::vector<std::vector<std::pair<u32, u32>>> &partialCommRankSet)
{
    for (u32 side = 0; side < partialCommRankSet.size(); side++) {
        for (u32 j = 0; j < partialCommRankSet[side].size(); j++) {
            u32 readRemoteRank = partialCommRankSet[side][j].first;
            if (readRemoteRank == userRank_) {
                continue;
            }
            u32 currDestRecvStep = recvNumSubStep_[readRemoteRank];
            std::vector<ReadDataBlock> readInfo;
            UpdateCurrRankRecvInfo(step, roundIdx, side, readRemoteRank, readInfo, currDestRecvStep);
            subStreamReadInfo[readRemoteRank] = readInfo;
        }
    }

    for (u32 side = 0; side < partialCommRankSet.size(); side++) {
        for (u32 j = 0; j < partialCommRankSet[side].size(); j++) {
            u32 sendRemoteRank = partialCommRankSet[side][j].second;
            if (sendRemoteRank == userRank_) {
                continue;
            }
            u32 currDestSendStep = sendNumSubStep_[sendRemoteRank];
            std::vector<SendDataBlock> sendInfo;
            UpdateCurrRankSendInfo(step, roundIdx, side, sendRemoteRank, sendInfo, currDestSendStep);
            subStreamSendInfo[sendRemoteRank] = sendInfo;
        }
    }
}

void BatchSendRecvDirectFullMesh::UpdateOpBaseSubStreamInfo(u32 step, u32 roundIdx)
{
    if (roundIdx == 0 || !isBigCount_) {
        subStreamReadInfo_.clear();
        subStreamSendInfo_.clear();
        UpdateSendRecvInfo(step, roundIdx, subStreamReadInfo_, subStreamSendInfo_, partialCommRankSet_);
    }
    if (isBigCount_ && (roundIdx < commRounds_ - 1)) {
        nextSubStreamReadInfo_.clear();
        nextSubStreamSendInfo_.clear();
        UpdateSendRecvInfo(step, roundIdx + 1, nextSubStreamReadInfo_, nextSubStreamSendInfo_, nextPartialCommRankSet_);
    }
}

HcclResult BatchSendRecvDirectFullMesh::PrepareIntraData(u32 step,
    std::unordered_map<u32, std::vector<SendDataBlock>> &subStreamSendInfo)
{
    const BatchSendRecvInfo& batchSendRecvInfo = *batchSendRecvInfoPtr_;
    u32 sendDataIndex = 0;
    
    for (auto& sdmaInfo : subStreamSendInfo) {
        const std::vector<SendDataBlock>& sendInfo = sdmaInfo.second;
        u32 sendRank = sdmaInfo.first;
        
        auto itemIt = std::find_if(batchSendRecvInfo.items.begin(), batchSendRecvInfo.items.end(),
            [sendRank](const BatchSendRecvItem& item) { return item.remoteRank == sendRank; });
        
        if (itemIt == batchSendRecvInfo.items.end()) {
            sendDataIndex++;
            continue;
        }

        if (step < sendNumSubStep_[sdmaInfo.first]) {
            u8* userInPtr = static_cast<u8*>(itemIt->sendBuf) + sendInfo[step].userInOffset;
            DeviceMem src(userInPtr, sendInfo[step].sendLen);
            DeviceMem dst = cclInMem_.range(sendInfo[step].scratchOffset, sendInfo[step].sendLen);
            
            HCCL_DEBUG("[BatchSendRecvDirectFullMesh][PrepareIntraData]userRank [%u] copy from userInOffset[%llu] "
                "len[%llu] to scratchOffset [%llu]", userRank_, sendInfo[step].userInOffset, 
                sendInfo[step].sendLen, sendInfo[step].scratchOffset);
            
            if (isBigCount_) {
                CHK_RET(HcclD2DMemcpyAsync(dispatcher_, dst, src, localSubStream_[sendDataIndex]));
            } else {
                CHK_RET(HcclD2DMemcpyAsync(dispatcher_, dst, src, mainStream_));
            }
        }
        sendDataIndex++;
    }
    return HCCL_SUCCESS;
}

void BatchSendRecvDirectFullMesh::UpdateRemoteRankSet(u32 roundIdx, u32 groupRankSize)
{
    UpdatePartialCommunicationRankSet(roundIdx, groupRankSize, partialCommRankSet_);
}

void BatchSendRecvDirectFullMesh::UpdatePartialCommunicationRankSet(u32 roundIdx, u32 groupRankSize,
    std::vector<std::vector<std::pair<u32, u32>>> &partialCommRankSet)
{
    partialCommRankSet.clear();
    partialCommRankSet.resize(RANK_SET_COMPUTE_CONST + 1);
    
    const BatchSendRecvInfo& batchSendRecvInfo = *batchSendRecvInfoPtr_;
    std::vector<u32> sortedRanks(batchSendRecvInfo.remoteRanks.begin(), batchSendRecvInfo.remoteRanks.end());
    std::sort(sortedRanks.begin(), sortedRanks.end());
    
    u32 pairNumPerRound = sdmaConcurrentNum_ / RANK_SET_COMPUTE_CONST;
    u32 pairSize = (groupRankSize < sdmaConcurrentNum_) ?
        (groupRankSize + RANK_SET_COMPUTE_CONST - 1) / RANK_SET_COMPUTE_CONST : pairNumPerRound;
    
    u32 startIdx = roundIdx * pairNumPerRound;
    u32 endIdx = std::min(startIdx + pairSize, static_cast<u32>(sortedRanks.size()));
    
    for (u32 i = startIdx; i < endIdx; i++) {
        u32 remoteRank = sortedRanks[i];
        if (remoteRank == userRank_) {
            continue;
        }
        
        u32 gap = (remoteRank > userRank_) ? 
            (remoteRank - userRank_) : (userRank_ - remoteRank + userRankSize_);
        
        if (gap <= userRankSize_ / 2) {
            partialCommRankSet[0].push_back(std::make_pair(remoteRank, remoteRank));
        } else {
            partialCommRankSet[1].push_back(std::make_pair(remoteRank, remoteRank));
        }
    }
}

HcclResult BatchSendRecvDirectFullMesh::NotifySubStreamStart()
{
    for (u32 streamIndex = 0; streamIndex < subStreamReadInfo_.size(); streamIndex++) {
        CHK_RET(LocalNotify::Post(mainStream_, dispatcher_, sdmaMeshSignalSubToMain_[streamIndex], INVALID_VALUE_STAGE));
        CHK_RET(LocalNotify::Wait(sdmaSubStream_[streamIndex], dispatcher_, sdmaMeshSignalSubToMain_[streamIndex],
            INVALID_VALUE_STAGE));
    }
    for (u32 streamIndex = 0; streamIndex < subStreamReadInfo_.size(); streamIndex++) {
        CHK_RET(ExecEmptyTask(cclInMem_, cclOutMem_, sdmaSubStream_[streamIndex], dispatcher_));
    }
    return HCCL_SUCCESS;
}

HcclResult BatchSendRecvDirectFullMesh::WaitSubStreamFinish()
{
    for (u32 streamIndex = 0; streamIndex < subStreamReadInfo_.size(); streamIndex++) {
        CHK_RET(LocalNotify::Post(sdmaSubStream_[streamIndex], dispatcher_, sdmaMeshSignalMainToSub_[streamIndex],
            INVALID_VALUE_STAGE));
        CHK_RET(LocalNotify::Wait(mainStream_, dispatcher_, sdmaMeshSignalMainToSub_[streamIndex],
            INVALID_VALUE_STAGE));
    }
    return HCCL_SUCCESS;
}

HcclResult BatchSendRecvDirectFullMesh::NotifyLocalSubStreamStart()
{
    for (u32 streamIndex = 0; streamIndex < subStreamSendInfo_.size(); streamIndex++) {
        CHK_RET(LocalNotify::Post(mainStream_, dispatcher_, localSignalSubToMain_[streamIndex], INVALID_VALUE_STAGE));
        CHK_RET(LocalNotify::Wait(localSubStream_[streamIndex], dispatcher_, localSignalSubToMain_[streamIndex],
            INVALID_VALUE_STAGE));
    }
    return HCCL_SUCCESS;
}

HcclResult BatchSendRecvDirectFullMesh::WaitLocalSubStreamFinish()
{
    for (u32 streamIndex = 0; streamIndex < subStreamSendInfo_.size(); streamIndex++) {
        CHK_RET(LocalNotify::Post(localSubStream_[streamIndex], dispatcher_, localSignalMainToSub_[streamIndex],
            INVALID_VALUE_STAGE));
        CHK_RET(LocalNotify::Wait(mainStream_, dispatcher_, localSignalMainToSub_[streamIndex],
            INVALID_VALUE_STAGE));
    }
    return HCCL_SUCCESS;
}

u32 BatchSendRecvDirectFullMesh::CalcNumSubStep()
{
    const BatchSendRecvInfo& batchSendRecvInfo = *batchSendRecvInfoPtr_;

    sendNumSubStep_.clear();
    recvNumSubStep_.clear();
    u32 numSubStep = 0;

    for (const auto& item : batchSendRecvInfo.items) {
        if (item.remoteRank == userRank_) {
            continue;
        }

        u32 currRankSendSubStep = ((item.sendLength + sdmaDataBlockSize_ - 1) / sdmaDataBlockSize_);
        sendNumSubStep_[item.remoteRank] = currRankSendSubStep;

        u32 currRankRecvSubStep = ((item.recvLength + sdmaDataBlockSize_ - 1) / sdmaDataBlockSize_);
        recvNumSubStep_[item.remoteRank] = currRankRecvSubStep;
        
        numSubStep = std::max(numSubStep, std::max(currRankSendSubStep, currRankRecvSubStep));
    }
    
    HCCL_DEBUG("[BatchSendRecvDirectFullMesh][CalcNumSubStep] userRank [%u] max communication step[%u]",
        userRank_, numSubStep);
    return numSubStep;
}

HcclResult BatchSendRecvDirectFullMesh::NotifyRemoteRankStart(u32 step)
{
    u32 streamIndex = 0;
    for (auto& sendRecvSide : partialCommRankSet_) {
        for (auto& sendRecvPair : sendRecvSide) {
            u32 recvRank = sendRecvPair.first;
            u32 sendRank = sendRecvPair.second;
            if (sendRank == userRank_) {
                continue;
            }
            const std::vector<ReadDataBlock>& readInfo = subStreamReadInfo_[recvRank];
            const std::vector<SendDataBlock>& sendInfo = subStreamSendInfo_[sendRank];
            Stream& currStream = sdmaSubStream_[streamIndex];
            const LINK& readTransport = links_[recvRank];
            const LINK& sendTransport = links_[sendRank];

            if (step < sendInfo.size()) {
                CHK_RET(sendTransport->TxAck(currStream));
            }

            if (step < readInfo.size()) {
                CHK_RET(readTransport->RxAck(currStream));
            }
            streamIndex++;
        }
    }
    return HCCL_SUCCESS;
}

HcclResult BatchSendRecvDirectFullMesh::SDMAwithRemoteRankAndNotifyEnd(u32 step, u32 roundIdx)
{
    const BatchSendRecvInfo& batchSendRecvInfo = *batchSendRecvInfoPtr_;
    u32 streamIndex = 0;
    
    for (auto& sendRecvSide : partialCommRankSet_) {
        for (auto& sendRecvPair : sendRecvSide) {
            u32 recvRank = sendRecvPair.first;
            u32 sendRank = sendRecvPair.second;
            if (sendRank == userRank_) {
                continue;
            }
            const std::vector<ReadDataBlock>& readInfo = subStreamReadInfo_[recvRank];
            const std::vector<SendDataBlock>& sendInfo = subStreamSendInfo_[sendRank];
            Stream& currStream = sdmaSubStream_[streamIndex];
            const LINK& readTransport = links_[recvRank];
            const LINK& sendTransport = links_[sendRank];

            if (step < readInfo.size()) {
                const LINK& intraNeighboorTransport = links_[recvRank];
                CHK_PTR_NULL(intraNeighboorTransport);
                void* remDMAMemPtr = nullptr;
                CHK_RET(intraNeighboorTransport->GetRemoteMem(UserMemType::INPUT_MEM, &remDMAMemPtr));
                DeviceMem remoteCCLInMem = DeviceMem::create(static_cast<u8*>(remDMAMemPtr), cclInMem_.size());
                DeviceMem srcMem = remoteCCLInMem.range(readInfo[step].remoteOffset, readInfo[step].recvLen);
                
                auto itemIt = std::find_if(batchSendRecvInfo.items.begin(), batchSendRecvInfo.items.end(),
                    [recvRank](const BatchSendRecvItem& item) { return item.remoteRank == recvRank; });
                
                if (itemIt != batchSendRecvInfo.items.end()) {
                    u8* userOutPtr = static_cast<u8*>(itemIt->recvBuf) + readInfo[step].recvOffset;
                    DeviceMem dstMem(userOutPtr, readInfo[step].recvLen);
                    CHK_RET(HcclD2DMemcpyAsync(dispatcher_, dstMem, srcMem, currStream,
                        readTransport->GetRemoteRank(), readTransport->GetLinkType()));
                }
                CHK_RET(readTransport->TxDataSignal(currStream));
            }

            if (step < sendInfo.size()) {
                CHK_RET(sendTransport->RxDataSignal(currStream));
            }
            streamIndex++;
        }
    }
    return HCCL_SUCCESS;
}

HcclResult BatchSendRecvDirectFullMesh::SendRecvData(u32 step, u32 roundIdx)
{
    CHK_RET(NotifyRemoteRankStart(step));
    CHK_RET(WaitSubStreamFinish());
    CHK_RET(ExecEmptyTask(cclInMem_, cclOutMem_, mainStream_, dispatcher_));
    CHK_RET(NotifySubStreamStart());
    
    if (isBigCount_ && (roundIdx < commRounds_ - 1)) {
        CHK_RET(NotifyLocalSubStreamStart());
        CHK_RET(PrepareIntraData(step, nextSubStreamSendInfo_));
    }
    CHK_RET(SDMAwithRemoteRankAndNotifyEnd(step, roundIdx));

    return HCCL_SUCCESS;
}

HcclResult BatchSendRecvDirectFullMesh::LocalCopy()
{
    const BatchSendRecvInfo& batchSendRecvInfo = *batchSendRecvInfoPtr_;
    
    for (const auto& item : batchSendRecvInfo.items) {
        if (item.remoteRank == userRank_ && item.sendLength > 0) {
            DeviceMem src(static_cast<u8*>(item.sendBuf), item.sendLength);
            DeviceMem dst(static_cast<u8*>(item.recvBuf), item.recvLength);
            CHK_RET(HcclD2DMemcpyAsync(dispatcher_, dst, src, mainStream_));
            break;
        }
    }

    return HCCL_SUCCESS;
}

HcclResult BatchSendRecvDirectFullMesh::RunGroupFullMeshBatchSendRecv(u32 roundIdx, u32 step)
{
    UpdateOpBaseSubStreamInfo(step, roundIdx);
    CHK_RET(ExecEmptyTask(cclInMem_, cclOutMem_, mainStream_, dispatcher_));
    
    if (isBigCount_ && (roundIdx == 0)) {
        CHK_RET(NotifyLocalSubStreamStart());
        CHK_RET(PrepareIntraData(step, subStreamSendInfo_));
        CHK_RET(WaitLocalSubStreamFinish());
        CHK_RET(ExecEmptyTask(cclInMem_, cclOutMem_, mainStream_, dispatcher_));
    } else if (!isBigCount_) {
        CHK_RET(PrepareIntraData(step, subStreamSendInfo_));
    }
    
    CHK_RET(NotifySubStreamStart());
    CHK_RET(ExecEmptyTask(cclInMem_, cclOutMem_, mainStream_, dispatcher_));
    CHK_RET(SendRecvData(step, roundIdx));
    
    if (step == 0 && !islocalCpyDone_) {
        CHK_RET(LocalCopy());
        islocalCpyDone_ = true;
    }
    
    CHK_RET(ExecEmptyTask(cclInMem_, cclOutMem_, mainStream_, dispatcher_));
    CHK_RET(WaitSubStreamFinish());
    
    if (isBigCount_ && (roundIdx < commRounds_ - 1)) {
        CHK_RET(WaitLocalSubStreamFinish());
    }
    
    CHK_RET(ExecEmptyTask(cclInMem_, cclOutMem_, mainStream_, dispatcher_));
    return HCCL_SUCCESS;
}

HcclResult BatchSendRecvDirectFullMesh::MainNotifyRdmaControlStart()
{
    CHK_RET(LocalNotify::Post(mainStream_, dispatcher_, rdmaControl2MainStreamNotify_, INVALID_VALUE_STAGE));
    CHK_RET(LocalNotify::Wait(rdmaSubStreams_[0], dispatcher_, rdmaControl2MainStreamNotify_, INVALID_VALUE_STAGE));
    return HCCL_SUCCESS;
}

HcclResult BatchSendRecvDirectFullMesh::RdmaControlNotifyMainFinish()
{
    CHK_RET(LocalNotify::Post(rdmaSubStreams_[0], dispatcher_, main2RdmaControlStreamNotify_, INVALID_VALUE_STAGE));
    CHK_RET(LocalNotify::Wait(mainStream_, dispatcher_, main2RdmaControlStreamNotify_, INVALID_VALUE_STAGE));
    return HCCL_SUCCESS;
}

HcclResult BatchSendRecvDirectFullMesh::RdmaControlNotifySubStart()
{
    for (u32 i = 1; i < rdmaSubStreams_.size(); i++) {
        CHK_RET(LocalNotify::Post(rdmaSubStreams_[0], dispatcher_, rdmaSub2ControlNotifies_[i-1], INVALID_VALUE_STAGE));
        CHK_RET(LocalNotify::Wait(rdmaSubStreams_[i], dispatcher_, rdmaSub2ControlNotifies_[i-1], INVALID_VALUE_STAGE));
    }
    return HCCL_SUCCESS;
}

HcclResult BatchSendRecvDirectFullMesh::SubNotifyRdmaControlFinish()
{
    for (u32 i = 1; i < rdmaSubStreams_.size(); i++) {
        CHK_RET(LocalNotify::Post(rdmaSubStreams_[i], dispatcher_, rdmaControl2SubNotifies_[i-1], INVALID_VALUE_STAGE));
        CHK_RET(LocalNotify::Wait(rdmaSubStreams_[0], dispatcher_, rdmaControl2SubNotifies_[i-1], INVALID_VALUE_STAGE));
    }
    return HCCL_SUCCESS;
}

u32 BatchSendRecvDirectFullMesh::GetNextDstRank(u32& curDstRank)
{
    if (curDstRank >= userRankSize_) {
        curDstRank = curDstRank % userRankSize_;
    }
    if (curDstRank == podStartRank_) {
        curDstRank += devNumInlocalPod_;
    }
    curDstRank = curDstRank % userRankSize_;
    return curDstRank++;
}

u32 BatchSendRecvDirectFullMesh::GetPreSrcRank(u32& curSrcRank)
{
    if (curSrcRank == podStartRank_ + devNumInlocalPod_ - 1) {
        curSrcRank = (curSrcRank + userRankSize_ - devNumInlocalPod_) % userRankSize_;
    }

    if (curSrcRank == 0) {
        curSrcRank = userRankSize_ - 1;
        return 0;
    }
    return curSrcRank--;
}

void BatchSendRecvDirectFullMesh::GenRdmaSendInfo(u32 dstRank, std::vector<SendDataBlock>& sendInfo)
{
    const BatchSendRecvInfo& batchSendRecvInfo = *batchSendRecvInfoPtr_;
    
    auto it = std::find_if(batchSendRecvInfo.items.begin(), batchSendRecvInfo.items.end(),
        [dstRank](const BatchSendRecvItem& item) { return item.remoteRank == dstRank; });
    
    if (it == batchSendRecvInfo.items.end()) {
        return;
    }

    u64 sendOffset = 0;
    u64 sendLength = it->sendLength;
    while (sendLength > 0) {
        u64 curSendLength = std::min(sendLength, rdmaDataBlockSize_);
        SendDataBlock sendData;
        sendData.userInOffset = sendOffset;
        sendData.sendLen = curSendLength;
        u32 index = dstRank % rdmaConcurrentNum_;
        sendData.scratchOffset = rdmaDataBlockSize_ * index;
        sendInfo.push_back(sendData);
        sendOffset += curSendLength;
        sendLength -= curSendLength;
    }
}

void BatchSendRecvDirectFullMesh::GenRdmaRecvInfo(u32 srcRank, std::vector<RecvDataBlock>& recvInfo)
{
    const BatchSendRecvInfo& batchSendRecvInfo = *batchSendRecvInfoPtr_;
    
    auto it = std::find_if(batchSendRecvInfo.items.begin(), batchSendRecvInfo.items.end(),
        [srcRank](const BatchSendRecvItem& item) { return item.remoteRank == srcRank; });
    
    if (it == batchSendRecvInfo.items.end()) {
        return;
    }

    u64 recvOffset = 0;
    u64 recvLength = it->recvLength;
    while (recvLength > 0) {
        u64 curRecvLength = std::min(recvLength, rdmaDataBlockSize_);
        RecvDataBlock recvData;
        recvData.recvOffset = recvOffset;
        recvData.recvLen = curRecvLength;
        u32 index = srcRank % rdmaConcurrentNum_;
        recvData.scratchOffset = rdmaDataBlockSize_ * index + rdmaDataBlockSize_ * rdmaConcurrentNum_;
        recvInfo.push_back(recvData);
        recvOffset += curRecvLength;
        recvLength -= curRecvLength;
    }
}

HcclResult BatchSendRecvDirectFullMesh::CopyDataForSend(u32 dstRank, std::vector<SendDataBlock>& sendInfo, 
    u32 curStep, Stream stream)
{
    if (curStep >= sendInfo.size()) {
        return HCCL_SUCCESS;
    }
    
    const BatchSendRecvInfo& batchSendRecvInfo = *batchSendRecvInfoPtr_;
    
    auto it = std::find_if(batchSendRecvInfo.items.begin(), batchSendRecvInfo.items.end(),
        [dstRank](const BatchSendRecvItem& item) { return item.remoteRank == dstRank; });
    
    if (it == batchSendRecvInfo.items.end()) {
        return HCCL_SUCCESS;
    }

    u8* userInPtr = static_cast<u8*>(it->sendBuf) + sendInfo[curStep].userInOffset;
    DeviceMem src(userInPtr, sendInfo[curStep].sendLen);
    DeviceMem dst = cclOutMem_.range(sendInfo[curStep].scratchOffset, sendInfo[curStep].sendLen);
    CHK_RET(HcclD2DMemcpyAsync(dispatcher_, dst, src, stream));
    return HCCL_SUCCESS;
}

HcclResult BatchSendRecvDirectFullMesh::SendRecvRdmaData(u32 dstRank, u32 srcRank, 
    std::vector<SendDataBlock>& sendInfo, std::vector<RecvDataBlock>& recvInfo, 
    u32 round, u32 index, u32 curStep, Stream stream)
{
    const LINK& sendTransport = links_[dstRank];
    const LINK& recvTransport = links_[srcRank];
    
    CHK_PTR_NULL(sendTransport);
    CHK_PTR_NULL(recvTransport);

    const BatchSendRecvInfo& batchSendRecvInfo = *batchSendRecvInfoPtr_;
    
    auto sendItemIt = std::find_if(batchSendRecvInfo.items.begin(), batchSendRecvInfo.items.end(),
        [dstRank](const BatchSendRecvItem& item) { return item.remoteRank == dstRank; });
    auto recvItemIt = std::find_if(batchSendRecvInfo.items.begin(), batchSendRecvInfo.items.end(),
        [srcRank](const BatchSendRecvItem& item) { return item.remoteRank == srcRank; });

    u32 minStep = std::min(sendInfo.size(), recvInfo.size());
    if (curStep < minStep) {
        CHK_RET(recvTransport->TxAck(stream));
        CHK_RET(sendTransport->RxAck(stream));
        
        u64 sendSrcOffset = (dstRank % rdmaConcurrentNum_) * rdmaDataBlockSize_;
        void* srcPtr = static_cast<u8*>(cclOutMem_.ptr()) + sendSrcOffset;
        u32 dstIndex = userRank_ % rdmaConcurrentNum_;
        u64 sendDstOffset = (dstIndex + rdmaConcurrentNum_) * rdmaDataBlockSize_;
        CHK_RET(sendTransport->TxAsync(UserMemType::OUTPUT_MEM, sendDstOffset, srcPtr,
            sendInfo[curStep].sendLen, stream));

        u64 recvDstOffset = (srcRank % rdmaConcurrentNum_ + rdmaConcurrentNum_) * rdmaDataBlockSize_;
        void* dstPtr = static_cast<u8*>(cclOutMem_.ptr()) + recvDstOffset;
        u64 recvSrcOffset = (userRank_ % rdmaConcurrentNum_) * rdmaDataBlockSize_;
        CHK_RET(recvTransport->RxAsync(UserMemType::OUTPUT_MEM, recvSrcOffset, dstPtr,
            recvInfo[curStep].recvLen, stream));

        CHK_RET(recvTransport->PostFinAck(stream));
        CHK_RET(sendTransport->WaitFinAck(stream));
    } else if (curStep < sendInfo.size()) {
        CHK_RET(sendTransport->RxAck(stream));
        u64 sendSrcOffset = (dstRank % rdmaConcurrentNum_) * rdmaDataBlockSize_;
        void* srcPtr = static_cast<u8*>(cclOutMem_.ptr()) + sendSrcOffset;
        u32 dstIndex = userRank_ % rdmaConcurrentNum_;
        u64 sendDstOffset = (dstIndex + rdmaConcurrentNum_) * rdmaDataBlockSize_;
        CHK_RET(sendTransport->TxAsync(UserMemType::OUTPUT_MEM, sendDstOffset, srcPtr,
            sendInfo[curStep].sendLen, stream));
        CHK_RET(sendTransport->WaitFinAck(stream));
    } else if (curStep < recvInfo.size()) {
        CHK_RET(recvTransport->TxAck(stream));
        u64 recvDstOffset = (srcRank % rdmaConcurrentNum_ + rdmaConcurrentNum_) * rdmaDataBlockSize_;
        void* dstPtr = static_cast<u8*>(cclOutMem_.ptr()) + recvDstOffset;
        u64 recvSrcOffset = (userRank_ % rdmaConcurrentNum_) * rdmaDataBlockSize_;
        CHK_RET(recvTransport->RxAsync(UserMemType::OUTPUT_MEM, recvSrcOffset, dstPtr,
            recvInfo[curStep].recvLen, stream));
        CHK_RET(recvTransport->PostFinAck(stream));
    }
    return HCCL_SUCCESS;
}

HcclResult BatchSendRecvDirectFullMesh::CopyRecvDataToOutput(u32 srcRank, std::vector<RecvDataBlock>& recvInfo,
    u32 curStep, Stream stream)
{
    if (curStep >= recvInfo.size()) {
        return HCCL_SUCCESS;
    }
    
    const BatchSendRecvInfo& batchSendRecvInfo = *batchSendRecvInfoPtr_;
    
    auto it = std::find_if(batchSendRecvInfo.items.begin(), batchSendRecvInfo.items.end(),
        [srcRank](const BatchSendRecvItem& item) { return item.remoteRank == srcRank; });
    
    if (it == batchSendRecvInfo.items.end()) {
        return HCCL_SUCCESS;
    }

    u64 srcOffset = (srcRank % rdmaConcurrentNum_ + rdmaConcurrentNum_) * rdmaDataBlockSize_;
    DeviceMem src = cclOutMem_.range(srcOffset, recvInfo[curStep].recvLen);
    u8* userOutPtr = static_cast<u8*>(it->recvBuf) + recvInfo[curStep].recvOffset;
    DeviceMem dst(userOutPtr, recvInfo[curStep].recvLen);
    CHK_RET(HcclD2DMemcpyAsync(dispatcher_, dst, src, stream));
    return HCCL_SUCCESS;
}

HcclResult BatchSendRecvDirectFullMesh::ProcessSingleGroupRdmaData(std::vector<u32>& dstRanks, 
    std::vector<u32>& srcRanks, u32 round)
{
    for (u32 index = 0; index < dstRanks.size(); index++) {
        u32 dstRank = dstRanks[index];
        u32 srcRank = srcRanks[index];
        Stream stream = rdmaSubStreams_[index + 1];

        std::vector<SendDataBlock> sendInfo;
        std::vector<RecvDataBlock> recvInfo;
        GenRdmaSendInfo(dstRank, sendInfo);
        GenRdmaRecvInfo(srcRank, recvInfo);
        u32 totalStep = std::max(sendInfo.size(), recvInfo.size());
        
        for (u32 curStep = 0; curStep < totalStep; curStep++) {
            CHK_RET(CopyDataForSend(dstRank, sendInfo, curStep, stream));
            CHK_RET(SendRecvRdmaData(dstRank, srcRank, sendInfo, recvInfo, round, index, curStep, stream));
            CHK_RET(CopyRecvDataToOutput(srcRank, recvInfo, curStep, stream));
        }
    }

    return HCCL_SUCCESS;
}

HcclResult BatchSendRecvDirectFullMesh::ProcessRdmaData()
{
    const BatchSendRecvInfo& batchSendRecvInfo = *batchSendRecvInfoPtr_;
    
    std::vector<u32> rdmaRanks;
    for (const auto& item : batchSendRecvInfo.items) {
        if (item.remoteRank < podStartRank_ || item.remoteRank > podEndRank_) {
            rdmaRanks.push_back(item.remoteRank);
        }
    }
    
    if (rdmaRanks.empty()) {
        return HCCL_SUCCESS;
    }

    u32 rdmaRoundNum = (rdmaRanks.size() + rdmaConcurrentNum_ - 1) / rdmaConcurrentNum_;

    for (u32 round = 0; round < rdmaRoundNum; round++) {
        u32 startIdx = round * rdmaConcurrentNum_;
        u32 endIdx = std::min(startIdx + rdmaConcurrentNum_, static_cast<u32>(rdmaRanks.size()));

        std::vector<u32> dstRanks;
        std::vector<u32> srcRanks;
        for (u32 i = startIdx; i < endIdx; i++) {
            dstRanks.push_back(rdmaRanks[i]);
            srcRanks.push_back(rdmaRanks[i]);
        }
        
        CHK_RET(ExecEmptyTask(cclInMem_, cclOutMem_, rdmaSubStreams_[0], dispatcher_));
        CHK_RET(RdmaControlNotifySubStart());
        CHK_RET(ExecEmptyTask(cclInMem_, cclOutMem_, rdmaSubStreams_[0], dispatcher_));
        CHK_RET(ProcessSingleGroupRdmaData(dstRanks, srcRanks, round));
        CHK_RET(SubNotifyRdmaControlFinish());
        CHK_RET(ExecEmptyTask(cclInMem_, cclOutMem_, rdmaSubStreams_[0], dispatcher_));
    }
    
    return HCCL_SUCCESS;
}

HcclResult BatchSendRecvDirectFullMesh::RunRDMA()
{
    CHK_RET(MainNotifyRdmaControlStart());
    CHK_RET(ProcessRdmaData());
    CHK_RET(ExecEmptyTask(cclInMem_, cclOutMem_, mainStream_, dispatcher_));
    CHK_RET(LocalCopy());
    islocalCpyDone_ = true;
    return HCCL_SUCCESS;
}

HcclResult BatchSendRecvDirectFullMesh::RunSDMATasks(u32 roundIdx, u32 step, u32 groupRankSize, u32 leftRankSize)
{
    if (isBigCount_) {
        if (roundIdx == 0) {
            UpdatePartialCommunicationRankSet(roundIdx, groupRankSize, partialCommRankSet_);
        }
        if (roundIdx < commRounds_ - 1) {
            u32 nextgroupRankSize = (leftRankSize - groupRankSize > sdmaConcurrentNum_) ?
                sdmaConcurrentNum_ : leftRankSize - groupRankSize;
            UpdatePartialCommunicationRankSet(roundIdx + 1, nextgroupRankSize, nextPartialCommRankSet_);
        }
        CHK_RET(RunGroupFullMeshBatchSendRecv(roundIdx, step));

        if (roundIdx < commRounds_ - 1) {
            partialCommRankSet_ = nextPartialCommRankSet_;
            subStreamSendInfo_ = nextSubStreamSendInfo_;
            subStreamReadInfo_ = nextSubStreamReadInfo_;
        }
        CHK_RET(LaunchTaskExtend(dispatcher_, mainStream_, localSubStream_));
    } else {
        UpdatePartialCommunicationRankSet(roundIdx, groupRankSize, partialCommRankSet_);
        CHK_RET(RunGroupFullMeshBatchSendRecv(roundIdx, step));
    }
    return HCCL_SUCCESS;
}

HcclResult BatchSendRecvDirectFullMesh::RunSDMA()
{
    u32 totalStep = CalcNumSubStep();
    commRounds_ = (devNumInlocalPod_ + sdmaConcurrentNum_ - 1) / sdmaConcurrentNum_;
    u32 leftRankSize = devNumInlocalPod_ - 1;

    HCCL_DEBUG("[BatchSendRecvDirectFullMesh][RunSDMA] userRank [%u] communication rounds[%llu] totalStep [%u]",
        userRank_, commRounds_, totalStep);

    if (totalStep == 0 && !islocalCpyDone_) {
        CHK_RET(LocalCopy());
        islocalCpyDone_ = true;
        return HCCL_SUCCESS;
    }

    for (u32 step = 0; step < totalStep; step++) {
        u32 currentLeftRankSize = devNumInlocalPod_ - 1;
        for (u32 roundIdx = 0; roundIdx < commRounds_ && currentLeftRankSize > 0; roundIdx++) {
            u32 groupRankSize = (currentLeftRankSize > sdmaConcurrentNum_) ? 
                sdmaConcurrentNum_ : currentLeftRankSize;
            CHK_RET(RunSDMATasks(roundIdx, step, groupRankSize, currentLeftRankSize));
            currentLeftRankSize -= groupRankSize;
            CHK_RET(LaunchTaskExtend(dispatcher_, mainStream_, sdmaSubStream_));
        }
    }
    
    return HCCL_SUCCESS;
}

HcclResult BatchSendRecvDirectFullMesh::RunAsync()
{
    HcclOpMetaInfoDef opMeta = HcclOpMetaInfo::GetOneForBatchSendRecv();
    CHK_RET(InitTask(dispatcher_, mainStream_, opMeta.isEnableCache, opMeta.GetCacheKey()));

    if (userRankSize_ == 1) {
        CHK_RET(LocalCopy());
        return HCCL_SUCCESS;
    }

    CHK_RET(ExecEmptyTask(cclInMem_, cclOutMem_, mainStream_, dispatcher_));
    
    if (totalRdmaRankNum_ > 0) {
        CHK_RET(RunRDMA());
    }

    CHK_RET(LaunchTaskExtend(dispatcher_, mainStream_, rdmaSubStreams_));

    if (devNumInlocalPod_ > 1) {
        CHK_RET(RunSDMA());
    }

    if (totalRdmaRankNum_ > 0) {
        CHK_RET(InitTask(dispatcher_, mainStream_, opMeta.isEnableCache, opMeta.GetCacheKey()));
        CHK_RET(RdmaControlNotifyMainFinish());
        CHK_RET(LaunchTaskExtend(dispatcher_, mainStream_, rdmaSubStreams_));
    }

    return HCCL_SUCCESS;
}

REGISTER_TEMPLATE(TemplateType::TEMPLATE_BATCH_SEND_RECV_DIRECT_FULL_MESH, BatchSendRecvDirectFullMesh);

} // namespace hccl

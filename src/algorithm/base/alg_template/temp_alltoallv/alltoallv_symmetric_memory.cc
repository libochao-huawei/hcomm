/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "alltoallv_symmetric_memory.h"

namespace hccl {
AlltoAllVSymmetricMemory::AlltoAllVDire?ctFullMesh(const HcclDispatcher dispatcher)
    : AlgTemplateBase(dispatcher)
{
}

AlltoAllVSymmetricMemory::~AlltoAllVSymmetricMemory() {}

HcclResult AlltoAllVSymmetricMemory::GenerateSubStreamInfo(const std::vector<Stream> &subStreams,
    const std::vector<std::shared_ptr<LocalNotify>> &meshSignalMainToSub,
    const std::vector<std::shared_ptr<LocalNotify>> &meshSignalSubToMain)
{
    u32 totalSubstreamSize = sdmaConcurrentNum_;
    if (subStreams.size() < totalSubstreamSize || meshSignalMainToSub.size() < totalSubstreamSize ||
        meshSignalSubToMain.size() < totalSubstreamSize) {
        HCCL_ERROR("[AlltoAllVSymmetricMemory][GenerateSubStreamInfo]subStreamsSize[%zu], meshSignalMainToSubSize[%zu]"\
            "meshSignalSubToMainSize[%zu] is smaller than totalSubstreamSize[%u]",subStreams.size(),
            meshSignalMainToSub.size(), meshSignalSubToMain.size(), totalSubstreamSize);
        return HCCL_E_PARA;
    }
    CHK_PRT_RET(links_.size() < userRankSize_, HCCL_ERROR("[AlltoAllVSymmetricMemory][GenerateSubStreamInfo]"\
        "links_.size()[%zu] is smaller than userRankSize_[%u].", links_.size(), userRankSize_),
        HCCL_E_PARA);
    HCCL_DEBUG("subStreams.size[%zu], meshSignalMainToSub.size[%zu], links_.size[%zu]",
        subStreams.size(), meshSignalMainToSub.size(), links_.size());
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
    return HCCL_SUCCESS;
}

HcclResult AlltoAllVSymmetricMemory::Prepare(PrepareData &param)
{
    mainStream_ = param.stream;
    userRank_ = param.userRank;
    userRankSize_ = param.userRankSize;
    links_ = *param.linksPtr;
    localSendRecvInfoPtr_ = param.localSendRecvInfoPtr;
    devNumInlocalPod_ = param.devNumInlocalPod;
    rankIdxInPod_ = param.rankIdxInPod;
    opType_ = param.opType;
    algOpContext_ = param.algOpContext;

    podStartRank_ = userRank_ - rankIdxInPod_;
    podEndRank_ = podStartRank_ + devNumInlocalPod_ - 1;
    sdmaConcurrentNum_ = (devNumInlocalPod_ > ALLTOALLV_DIRECT_FULLMESH_SDMA_CONCURRENT_SIZE) ?
        (ALLTOALLV_DIRECT_FULLMESH_SDMA_CONCURRENT_SIZE) : (devNumInlocalPod_);

    HCCL_DEBUG("[AlltoAllVSymmetricMemory]devNumInlocalPod_[%u], userRankSize_[%u] podStartRank_[%u]" \
        "podEndRank_[%u], sdmaConcurrentNum_[%u]",
        devNumInlocalPod_, userRankSize_, podStartRank_, podEndRank_, sdmaConcurrentNum_);

    CHK_PRT_RET(userRankSize_ == 0, HCCL_ERROR("[AlltoAllVSymmetricMemory][Prepare]userRankSize_ is zero."),
        HCCL_E_PARA);

    userInput_ = param.inputMem;
    userOutput_ = param.outputMem;
    cclInMem_ = param.cclInMem;
    cclOutMem_ = param.cclOutMem;
    workMode_ = param.workMode;
    isSuPodAsym_ = param.isSuPodAsym;

    u64 maxSendLen = CalcMaxSendLen();
    // isBigCount_ = (maxSendLen > ALLTOALLV_DIRECT_FULLMESH_BIG_SIZE) ? true : false;
    CHK_RET(GenerateSubStreamInfo(*param.subStreamsPtr, *param.signalPtr, *param.signalAuxPtr));

    /* 考虑当group0 的rank 跟 group 1的所有rank通信时，每次都要收发，所以取sdmaConcurrentNum_块；
    跟group 0内的rank通信有一块儿浪费 */
    isBigCount_ = false;
    u32 blockGroup = 1;
    sdmaDataBlockSize_= (maxSendLen / std::max(1u, sdmaConcurrentNum_ * blockGroup));
    // 向下对齐到16k Byte
    if (sdmaDataBlockSize_> HCCL_MIN_SLICE_ALIGN_910B) {
        sdmaDataBlockSize_= (sdmaDataBlockSize_/ HCCL_MIN_SLICE_ALIGN_910B) * HCCL_MIN_SLICE_ALIGN_910B;
    }
    CHK_PRT_RET(sdmaDataBlockSize_== 0, HCCL_ERROR("[AlltoAllVSymmetricMemory][Prepare]sdmaDataBlockSize_is zero."),
        HCCL_E_INTERNAL);
    HCCL_DEBUG("[AlltoAllVSymmetricMemory][Prepare] userRank [%u] total cclsize[%llu]," \
        "sdmaDataBlockSize_[%llu], BigCountFlag[%d]", userRank_, cclInMem_.size(), sdmaDataBlockSize_, isBigCount_);

    return HCCL_SUCCESS;
}

std::string AlltoAllVSymmetricMemory::GetStreamIndexString()
{
    std::string res = "";
    for (auto& info : subStreamReadInfo_) {
        u32 destRank = info.first;
        u32 streamIndex = destRank % sdmaConcurrentNum_;
        res += std::to_string(streamIndex) + ", ";
    }
    return res;
}

u64 AlltoAllVSymmetricMemory::CalcMaxSendLen()
{
    u64 maxSendLen = 0;
    const SendRecvInfo& localSendRecvInfo = *localSendRecvInfoPtr_;

    for (u32 dstRank = 0; dstRank < localSendRecvInfo.sendLength.size(); dstRank++) {
        maxSendLen = std::max(maxSendLen, localSendRecvInfo.sendLength[dstRank]);
    }

    HCCL_DEBUG("[AlltoAllVSymmetricMemory][CalcMaxSendLen] maxSendLen[%llu]", maxSendLen);
    return maxSendLen;
}

void AlltoAllVSymmetricMemory::UpdateCurrRankRecvInfo(u32 roundIdx, u32 side, u32 destRank,
    std::vector<ReadDataBlock>& readInfo, u32 maxRecvStep)
{
    const SendRecvInfo& localSendRecvInfo = *localSendRecvInfoPtr_;
    u64 remainRecvLen = localSendRecvInfo.recvLength[destRank];
    u64 scratchOffset = 0;
    u32 bufferIdx = 0;
    u32 pairNum = sdmaConcurrentNum_ / RANK_SET_COMPUTE_CONST;
    if (sdmaConcurrentNum_ == 1) { // 保证和当前rank距离一样时，send/recv用的是同一块buff
        bufferIdx = 0;
    } else if (side == 0) { // 在curRank左边
        u32 gap = (userRank_ - destRank + devNumInlocalPod_) % devNumInlocalPod_;
        bufferIdx = pairNum - (gap - roundIdx * pairNum);
    } else if (side == 1) { // 在curRank右边
        u32 gap = (destRank - userRank_ + devNumInlocalPod_) % devNumInlocalPod_;
        bufferIdx = pairNum - 1 + (gap - roundIdx * pairNum);
    } else { // 最后一个中间位置的rank
        bufferIdx = 0;
    }

    if ((isBigCount_ || opType_ == HcclCMDType::HCCL_CMD_ALLTOALLV || opType_ == HcclCMDType::HCCL_CMD_ALLTOALLVC) &&
        (roundIdx % RANK_SET_COMPUTE_CONST != 0)) { // 奇数轮，用下半Buffer
        bufferIdx += sdmaConcurrentNum_;
    }

    scratchOffset = bufferIdx * sdmaDataBlockSize_;

    u32 recvStepIdx = 0;
    u64 dataOffset = 0;
    HCCL_DEBUG("usrRank[%u] total recv localSendRecvInfo.recvLength[%llu] from dstRank[%u] bufferIdx[%u]",
        userRank_, remainRecvLen, destRank, bufferIdx);

    while(recvStepIdx < maxRecvStep && remainRecvLen > 0) {
        u64 currDataRemainLen = localSendRecvInfo.recvLength[destRank] - dataOffset;
        u64 recvLen = std::min(sdmaDataBlockSize_, currDataRemainLen);
        u64 userOutOffset = localSendRecvInfo.recvOffset[destRank] + dataOffset;
        HCCL_DEBUG("[AlltoAllVSymmetricMemory][UpdateCurrRankRecvInfo] usrRank[%u] recv from destRank [%u]"
            "sendStepIdx[%u] recvLen[%lu] userOutOffset[%llu] scratchOffset[%llu]",
            userRank_, destRank, recvStepIdx, recvLen, userOutOffset, scratchOffset);
        readInfo.push_back({recvLen, scratchOffset, userOutOffset});
        dataOffset += recvLen;
        recvStepIdx++;
        remainRecvLen -= recvLen;
    }
}

void AlltoAllVSymmetricMemory::UpdateCurrRankSendInfo(u32 roundIdx, u32 side, u32 destRank,
    std::vector<SendDataBlock>& sendInfo, u32 maxSendStep)
{
    const SendRecvInfo& localSendRecvInfo = *localSendRecvInfoPtr_;
    u64 remainSendLen = localSendRecvInfo.sendLength[destRank];

    u64 scratchOffset = 0;
    u32 bufferIdx = 0;
    u32 pairNum = sdmaConcurrentNum_ / RANK_SET_COMPUTE_CONST;
    if (sdmaConcurrentNum_ == 1) { // 保证和当前rank距离一样时，send/recv用的是同一块buff
        bufferIdx = 0;
    } else if (side == 0) { // 在curRank左边
        u32 gap = (userRank_ - destRank + devNumInlocalPod_) % devNumInlocalPod_;
        bufferIdx = pairNum - 1 + (gap - roundIdx * pairNum);
    } else if (side == 1) { // 在curRank右边
        u32 gap = (destRank - userRank_ + devNumInlocalPod_) % devNumInlocalPod_;
        bufferIdx = pairNum - (gap - roundIdx * pairNum);
    } else { // 最后一个中间位置的rank
        bufferIdx = 0;
    }

    scratchOffset = bufferIdx * sdmaDataBlockSize_;

    u32 sendStepIdx = 0;
    u64 dataOffset = 0;
    HCCL_DEBUG("usrRank[%u] total send localSendRecvInfo.sendLength[%llu] to dstRank[%u] bufferIdx[%u]",
        userRank_, remainSendLen, destRank, bufferIdx);

    while (sendStepIdx < maxSendStep && remainSendLen > 0) {
        u64 currDataRemainLen = localSendRecvInfo.sendLength[destRank] - dataOffset;
        u64 sendLen = std::min(sdmaDataBlockSize_, currDataRemainLen);
        u64 userInOffset = localSendRecvInfo.sendOffset[destRank] + dataOffset;
        HCCL_DEBUG("[AlltoAllVSymmetricMemory][UpdateCurrRankSendInfo] usrRank[%u] send to destRank [%u]"
            " sendStepIdx[%u] sendLen[%lu] userInOffset[%llu] scratchOffset[%llu]",
            userRank_, destRank, sendStepIdx, sendLen, userInOffset, scratchOffset);
        sendInfo.push_back({sendLen, userInOffset, scratchOffset});
        dataOffset += sendLen;
        sendStepIdx++;
        remainSendLen -= sendLen;
    }
}

void AlltoAllVSymmetricMemory::UpdateSendRecvInfo(u32 roundIdx,
    std::unordered_map<u32, std::vector<ReadDataBlock>> &subStreamReadInfo,
    std::unordered_map<u32, std::vector<SendDataBlock>> &subStreamSendInfo,
    const std::vector<std::vector<std::pair<u32,u32>>> &partialCommRankSet)
{
    for (u32 side = 0; side < partialCommRankSet.size(); side++) {
        for (u32 j = 0; j < partialCommRankSet[side].size(); j++) {
            u32 readRemoteRank = partialCommRankSet[side][j].first;
            if (readRemoteRank == userRank_) {
                continue;
            }
            u32 currDestRecvStep = recvNumSubStep_[readRemoteRank];
            std::vector<ReadDataBlock> readInfo;
            UpdateCurrRankRecvInfo(roundIdx, side, readRemoteRank, readInfo, currDestRecvStep);

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
            UpdateCurrRankSendInfo(roundIdx, side, sendRemoteRank, sendInfo, currDestSendStep);

            subStreamSendInfo[sendRemoteRank] = sendInfo;
        }
    }
}

void AlltoAllVSymmetricMemory::UpdateOpBaseSubStreamInfo(u32 roundIdx)
{
    subStreamReadInfo_.clear();
    subStreamSendInfo_.clear();
    UpdateSendRecvInfo(roundIdx, subStreamReadInfo_, subStreamSendInfo_, partialCommRankSet_);
}

HcclResult AlltoAllVSymmetricMemory::PrepareIntraData(u32 step,
    std::unordered_map<u32,std::vector<SendDataBlock>> &subStreamSendInfo)
{
    u32 sendDataIndex = 0;
    for (auto& sdmaInfo : subStreamSendInfo) {
        const std::vector<SendDataBlock>& sendInfo = sdmaInfo.second;
        if (step < sendNumSubStep_[sdmaInfo.first]) {
            DeviceMem src = userInput_.range(sendInfo[step].userInOffset, sendInfo[step].sendLen);
            DeviceMem dst = cclInMem_.range(sendInfo[step].scratchOffset, sendInfo[step].sendLen);
            HCCL_DEBUG("[AlltoAllVSymmetricMemory][PrepareIntraData]userRank [%u] copy from userInOffset[%llu]"
                "len[%u] to scratchOffset [%llu]", userRank_, sendInfo[step].userInOffset, sendInfo[step].sendLen,
                sendInfo[step].scratchOffset);
                CHK_RET(HcclD2DMemcpyAsync(dispatcher_, dst, src, mainStream_));
        }
        sendDataIndex++;
    }
    return HCCL_SUCCESS;
}

void AlltoAllVSymmetricMemory::UpdateRemoteRankSet(u32 roundIdx, u32 groupRankSize)
{
    if (sdmaConcurrentNum_ == 1) {
        UpdatePartialCommunicationRankSetPairWise(roundIdx, groupRankSize);
    } else {
        UpdatePartialCommunicationRankSet(roundIdx, groupRankSize, partialCommRankSet_);
    }
}

void AlltoAllVSymmetricMemory::UpdatePartialCommunicationRankSetPairWise(u32 roundIdx, u32 groupRankSize)
{
    partialCommRankSet_.clear();
    partialCommRankSet_.resize(1);
    for (u32 i = roundIdx * sdmaConcurrentNum_; i < (roundIdx * sdmaConcurrentNum_ + groupRankSize); i++) {
        u32 readRemoteRank = podStartRank_ + (rankIdxInPod_ + devNumInlocalPod_ - i) % devNumInlocalPod_;
        u32 sendRemoteRank = podStartRank_ + (rankIdxInPod_ + i) % devNumInlocalPod_;
        partialCommRankSet_[0].push_back(std::make_pair(readRemoteRank, sendRemoteRank));
        HCCL_DEBUG("[AlltoAllVSymmetricMemory][UpdatePartialCommunicationRankSetPairWise] userRank [%u] i[%u]" \
            "readRemoteRank[%u] writeRemoteRank[%u]", userRank_, i, readRemoteRank, sendRemoteRank);
    }
    HCCL_DEBUG("[AlltoAllVSymmetricMemory][UpdatePartialCommunicationRankSetPairWise] partialCommRankSet_ size[%zu]",
        partialCommRankSet_[0].size());
}

void AlltoAllVSymmetricMemory::UpdatePartialCommunicationRankSet(u32 roundIdx, u32 groupRankSize,
    std::vector<std::vector<std::pair<u32,u32>>> &partialCommRankSet)
{
    partialCommRankSet.clear();
    partialCommRankSet.resize(RANK_SET_COMPUTE_CONST + 1);
    u32 pairNumPerRound = sdmaConcurrentNum_ / RANK_SET_COMPUTE_CONST;
    u32 pairSize = (groupRankSize < sdmaConcurrentNum_) ?
        (groupRankSize + RANK_SET_COMPUTE_CONST - 1) / RANK_SET_COMPUTE_CONST: pairNumPerRound;
    for (u32 i = roundIdx * pairNumPerRound + 1;
         i < (roundIdx * pairNumPerRound + pairSize + 1); i++) {
        u32 leftRemoteRank = podStartRank_ + (rankIdxInPod_ + devNumInlocalPod_ - i) % devNumInlocalPod_;
        u32 rightRemoteRank = podStartRank_ + (rankIdxInPod_ + i) % devNumInlocalPod_;
        if (leftRemoteRank == rightRemoteRank) {
            partialCommRankSet[2].push_back(std::make_pair(leftRemoteRank, leftRemoteRank));
        } else {
            partialCommRankSet[0].push_back(std::make_pair(leftRemoteRank, leftRemoteRank));
            partialCommRankSet[1].push_back(std::make_pair(rightRemoteRank, rightRemoteRank));
        }
        HCCL_DEBUG("[AlltoAllVSymmetricMemory][UpdatePartialCommunicationRankSet] round[%u] userRank [%u] i[%u]" \
            "read/write leftRemoteRank[%u] rightRemoteRank[%u]", roundIdx, userRank_, i, leftRemoteRank, rightRemoteRank);
    }
    HCCL_DEBUG("[AlltoAllVSymmetricMemory][UpdatePartialCommunicationRankSet] round[%u] partialCommRankSet_ total size[%zu]",
        roundIdx, partialCommRankSet[0].size() + partialCommRankSet[1].size() + partialCommRankSet[2].size());
}

// 主流只需要通知当前子步骤需要收发数据的 SDMA 流，减少同步开销
HcclResult AlltoAllVSymmetricMemory::NotifySubStreamStart()
{
    for (u32 streamIndex = 0; streamIndex < subStreamReadInfo_.size(); streamIndex++) {
        CHK_RET(LocalNotify::Post(mainStream_, dispatcher_, sdmaMeshSignalSubToMain_[streamIndex], INVALID_VALUE_STAGE));
        CHK_RET(LocalNotify::Wait(sdmaSubStream_[streamIndex], dispatcher_, sdmaMeshSignalSubToMain_[streamIndex],
            INVALID_VALUE_STAGE));
    }
    HCCL_DEBUG("[AlltoAllVSymmetricMemory][NotifySubStreamStart] userRank [%u] main stream notify sdma stream [%s]",
        userRank_, GetStreamIndexString().c_str());
    return HCCL_SUCCESS;
}

HcclResult AlltoAllVSymmetricMemory::WaitSubStreamFinish()
{
    for (u32 streamIndex = 0; streamIndex < subStreamReadInfo_.size(); streamIndex++) {
        CHK_RET(LocalNotify::Post(sdmaSubStream_[streamIndex], dispatcher_, sdmaMeshSignalMainToSub_[streamIndex],
            INVALID_VALUE_STAGE));
        CHK_RET(LocalNotify::Wait(mainStream_, dispatcher_, sdmaMeshSignalMainToSub_[streamIndex],
            INVALID_VALUE_STAGE));
    }
    HCCL_DEBUG("[AlltoAllVSymmetricMemory][WaitSubStreamFinish] userRank [%u] main stream wait sdma stream [%s]",
        userRank_, GetStreamIndexString().c_str());
    return HCCL_SUCCESS;
}

HcclResult AlltoAllVSymmetricMemory::NotifyLocalSubStreamStart()
{
    for (u32 streamIndex = 0; streamIndex < subStreamSendInfo_.size(); streamIndex++) {
        CHK_RET(LocalNotify::Post(mainStream_, dispatcher_, localSignalSubToMain_[streamIndex], INVALID_VALUE_STAGE));
        CHK_RET(LocalNotify::Wait(localSubStream_[streamIndex], dispatcher_, localSignalSubToMain_[streamIndex],
            INVALID_VALUE_STAGE));
    }
    return HCCL_SUCCESS;
}

HcclResult AlltoAllVSymmetricMemory::WaitLocalSubStreamFinish()
{
    for (u32 streamIndex = 0; streamIndex < subStreamSendInfo_.size(); streamIndex++) {
        CHK_RET(LocalNotify::Post(localSubStream_[streamIndex], dispatcher_, localSignalMainToSub_[streamIndex],
            INVALID_VALUE_STAGE));
        CHK_RET(LocalNotify::Wait(mainStream_, dispatcher_, localSignalMainToSub_[streamIndex],
            INVALID_VALUE_STAGE));
    }
    return HCCL_SUCCESS;
}

u32 AlltoAllVSymmetricMemory::CalcNumSubStep()
{
    const SendRecvInfo& localSendRecvInfo = *localSendRecvInfoPtr_;

    sendNumSubStep_.clear();
    recvNumSubStep_.clear();
    u32 numSubStep = 0;

    for (u32 destRank = podStartRank_; destRank < podStartRank_ + devNumInlocalPod_; destRank++) {
        if (destRank == userRank_) {
            continue;
        }

        u32 currRankSendSubStep = 1;
        sendNumSubStep_[destRank] = currRankSendSubStep;

        u32 currRankRecvSubStep = 1;
        recvNumSubStep_[destRank] = currRankRecvSubStep;
        HCCL_DEBUG("[AlltoAllVSymmetricMemory][CalcNumSubStep] userRank [%u] currRankSendSubStep[%u]" \
        "currRankRecvSubStep[%u]", userRank_, currRankSendSubStep, currRankRecvSubStep);
        numSubStep = std::max(numSubStep, std::max(currRankSendSubStep, currRankRecvSubStep));
    }
    HCCL_DEBUG("[AlltoAllVSymmetricMemory][CalcNumSubStep] userRank [%u] max communication step[%u]",
        userRank_, numSubStep);
    return numSubStep;
}

HcclResult AlltoAllVSymmetricMemory::NotifyRemoteRankStart(u32 step)
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
            streamIndex ++;
        }
    }
    HCCL_INFO("[AlltoAllVSymmetricMemory][NotifyRemoteRankStart] done");
    return HCCL_SUCCESS;
}

bool AlltoAllVSymmetricMemory::IsPostSyncEnable(u32 step, u32 roundIdx)
{
    bool isPostSyncEnable = false;
    isPostSyncEnable = (step == lastStep_) && (roundIdx == lastRoundIdx_) &&
        algOpContext_.opRetryHandler.retryEnable;
    return isPostSyncEnable;
}

HcclResult AlltoAllVSymmetricMemory::SdmaMainStreamWait(u32 step, u32 roundIdx)
{
    // SDMA wait
    u32 streamIndex = 0;
    for (auto& sendRecvSide : partialCommRankSet_) {
        for (auto& sendRecvPair : sendRecvSide) {
            u32 recvRank = sendRecvPair.first;
            u32 sendRank = sendRecvPair.second;
            if (sendRank == userRank_) {
                continue;
            }
            const std::vector<ReadDataBlock>& readInfo = subStreamReadInfo_[recvRank];
            if (step < readInfo.size()) {
                HCCL_DEBUG("[AlltoAllVSymmetricMemory][SdmaMainStreamWait] userRank [%u], recvRank[%u], "
                    "sendRank[%u], sdma stream [%u], "
                    "post sync info: step[%u], roundIdx[%u], lastStep_[%u], lastRoundIdx_[%u] main stream wait",
                    userRank_,  recvRank, sendRank, streamIndex, step, roundIdx, lastStep_, lastRoundIdx_);
                CHK_RET(LocalNotify::Wait(mainStream_, dispatcher_, sdmaMeshSignalMainToSub_[streamIndex],
                    INVALID_VALUE_STAGE));
            }
            streamIndex ++;
        }
    }
    HCCL_INFO("[AlltoAllVSymmetricMemory][SdmaMainStreamWait] done");
    return HCCL_SUCCESS;
}

HcclResult AlltoAllVSymmetricMemory::SdmaMainStreamPost(u32 step, u32 roundIdx)
{
    // SDMA post
    u32 streamIndex = 0;
    for (auto& sendRecvSide : partialCommRankSet_) {
        for (auto& sendRecvPair : sendRecvSide) {
            u32 recvRank = sendRecvPair.first;
            u32 sendRank = sendRecvPair.second;
            if (sendRank == userRank_) {
                continue;
            }
            const std::vector<ReadDataBlock>& readInfo = subStreamReadInfo_[recvRank];
            if (step < readInfo.size()) {
                HCCL_DEBUG("[AlltoAllVSymmetricMemory][SdmaMainStreamPost] userRank [%u], recvRank[%u], "
                    "sendRank[%u], sdma stream [%u], "
                    "post sync info: step[%u], roundIdx[%u], lastStep_[%u], lastRoundIdx_[%u] main stream post",
                    userRank_,  recvRank, sendRank, streamIndex, step, roundIdx, lastStep_, lastRoundIdx_);
                CHK_RET(LocalNotify::Post(mainStream_, dispatcher_, sdmaMeshSignalSubToMain_[streamIndex],
                    INVALID_VALUE_STAGE));
            }
            streamIndex ++;
        }
    }
    HCCL_INFO("[AlltoAllVSymmetricMemory][SdmaMainStreamPost] done");
    return HCCL_SUCCESS;
}

HcclResult AlltoAllVSymmetricMemory::SetPostSyncTasks(u32 step, u32 roundIdx)
{
    // SDMA wait
    CHK_RET(SdmaMainStreamWait(step, roundIdx));
    // SDMA post
    CHK_RET(SdmaMainStreamPost(step, roundIdx));
    HCCL_DEBUG("[AlltoAllVSymmetricMemory][SetPostSyncTasks] done");
    return HCCL_SUCCESS;
}

HcclResult AlltoAllVSymmetricMemory::SDMAwithRemoteRankAndNotifyEnd(u32 step, u32 roundIdx)
{
    bool isPostSyncEnable = IsPostSyncEnable(step, roundIdx);
    if (isPostSyncEnable) {
        // 下发主流上的后同步wait和post
        CHK_RET(SetPostSyncTasks(step, roundIdx));
    }
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
                DeviceMem remoteUserInMem = DeviceMem::create(static_cast<u8 *>(remDMAMemPtr), userInput_.size());
                DeviceMem srcMem = remoteUserInMem.range(readInfo[step].remoteOffset, readInfo[step].recvLen);
                DeviceMem dstMem = userOutput_.range(readInfo[step].recvOffset, readInfo[step].recvLen);
                CHK_RET(HcclD2DMemcpyAsync(dispatcher_, dstMem, srcMem, currStream,
                    readTransport->GetRemoteRank(), readTransport->GetLinkType()));
                HCCL_DEBUG("[AlltoAllVSymmetricMemory][SendRecvData] userRank [%u], recvRank[%u], sendRank[%u]," \
                    "sdma stream [%u] read data from remote offset [%llu] len [%llu] to local [%llu], "
                    "post sync info: step[%u], roundIdx[%u], lastStep_[%u], lastRoundIdx_[%u]",
                    userRank_,  recvRank, sendRank, streamIndex, readInfo[step].remoteOffset,
                    readInfo[step].recvLen, readInfo[step].recvOffset, step, roundIdx, lastStep_, lastRoundIdx_);
                if (isPostSyncEnable) {
                    HCCL_DEBUG("[AlltoAllVSymmetricMemory][SendRecvData] post sync begins");
                    CHK_RET(LocalNotify::Post(currStream, dispatcher_, sdmaMeshSignalMainToSub_[streamIndex],
                        INVALID_VALUE_STAGE));
                    CHK_RET(LocalNotify::Wait(currStream, dispatcher_, sdmaMeshSignalSubToMain_[streamIndex],
                        INVALID_VALUE_STAGE));
                }
                CHK_RET(readTransport->TxDataSignal(currStream));
            }
            if (step < sendInfo.size()) {
                CHK_RET(sendTransport->RxDataSignal(currStream));
            }
            streamIndex ++;
        }
    }
    HCCL_INFO("[AlltoAllVSymmetricMemory][SDMAwithRemoteRankAndNotifyEnd] done");
    return HCCL_SUCCESS;
}

HcclResult AlltoAllVSymmetricMemory::SendRecvData(u32 step, u32 roundIdx)
{
    HCCL_DEBUG("[AlltoAllVSymmetricMemory][SendRecvData] userRank [%u] sdma stream [%s] wait main stream",
        userRank_, GetStreamIndexString().c_str());
    CHK_RET(NotifyRemoteRankStart(step));
    CHK_RET(WaitSubStreamFinish());
    CHK_RET(NotifySubStreamStart());
    CHK_RET(SDMAwithRemoteRankAndNotifyEnd(step, roundIdx));

    return HCCL_SUCCESS;
}

HcclResult AlltoAllVSymmetricMemory::LocalCopy()
{
    const SendRecvInfo& localSendRecvInfo = *localSendRecvInfoPtr_;
    DeviceMem src = userInput_.range(localSendRecvInfo.sendOffset[userRank_],
        localSendRecvInfo.sendLength[userRank_]);
    DeviceMem dst = userOutput_.range(localSendRecvInfo.recvOffset[userRank_],
        localSendRecvInfo.recvLength[userRank_]);
    HCCL_DEBUG("[AlltoAllVSymmetricMemory][LocalCopy]userRank [%u] copy from userInput [%llu] len [%llu]" \
        "to userOutput [%llu] dstLen[%llu]", userRank_, localSendRecvInfo.sendOffset[userRank_],
        localSendRecvInfo.sendLength[userRank_],
        localSendRecvInfo.recvOffset[userRank_],
        localSendRecvInfo.recvLength[userRank_]);
    CHK_RET(HcclD2DMemcpyAsync(dispatcher_, dst, src, mainStream_));

    return HCCL_SUCCESS;
}

HcclResult AlltoAllVSymmetricMemory::RunGroupFullMeshAlltoall(u32 roundIdx, u32 step)
{
    UpdateOpBaseSubStreamInfo(roundIdx);
    // CHK_RET(PrepareIntraData(step, subStreamSendInfo_));
    CHK_RET(NotifySubStreamStart());
    CHK_RET(SendRecvData(step, roundIdx));
    if (step == 0 && !islocalCpyDone_) {
        CHK_RET(LocalCopy());
        islocalCpyDone_ = true;
    }
    CHK_RET(WaitSubStreamFinish());
    return HCCL_SUCCESS;
}

u32 AlltoAllVSymmetricMemory::GetNextDstRank(u32& curDstRank)
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

u32 AlltoAllVSymmetricMemory::GetPreSrcRank(u32& curDstRank)
{
    if (curDstRank == podStartRank_ + devNumInlocalPod_ - 1) {
        curDstRank = (curDstRank + userRankSize_ - devNumInlocalPod_) % userRankSize_;
    }

    if (curDstRank == 0) {
        curDstRank = userRankSize_ - 1;
        return 0;
    }
    return curDstRank--;
}

// 将数据从userIn拷贝到CCL out
HcclResult AlltoAllVSymmetricMemory::CopyDataForSend(u32 dstRank, std::vector<SendDataBlock>& sendInfo, u32 curStep, Stream stream)
{
    if (curStep >= sendInfo.size()) {
        return HCCL_SUCCESS;
    }
    DeviceMem src = userInput_.range(sendInfo[curStep].userInOffset, sendInfo[curStep].sendLen);
    DeviceMem dst = cclOutMem_.range(sendInfo[curStep].scratchOffset, sendInfo[curStep].sendLen);
    HCCL_DEBUG("[CopyDataForSend] userRank[%u], dstRank[%u], userInOffset[%llu], sendLen[%llu], scratchOffset[%llu]",
        userRank_, dstRank, sendInfo[curStep].userInOffset, sendInfo[curStep].sendLen, sendInfo[curStep].scratchOffset);
    CHK_RET(HcclD2DMemcpyAsync(dispatcher_, dst, src, stream));
    return HCCL_SUCCESS;
}

HcclResult AlltoAllVSymmetricMemory::RdmaPostSync(Stream& stream)
{
    CHK_RET(LocalNotify::Post(stream, dispatcher_, rdmaControl2SubNotifies_[0], INVALID_VALUE_STAGE));
    CHK_RET(LocalNotify::Wait(rdmaSubStreams_[0], dispatcher_, rdmaControl2SubNotifies_[0], INVALID_VALUE_STAGE));

    CHK_RET(LocalNotify::Post(rdmaSubStreams_[0], dispatcher_, rdmaSub2ControlNotifies_[0], INVALID_VALUE_STAGE));
    CHK_RET(LocalNotify::Wait(stream, dispatcher_, rdmaSub2ControlNotifies_[0], INVALID_VALUE_STAGE));
    return HCCL_SUCCESS;
}

HcclResult AlltoAllVSymmetricMemory::RunSDMATasks(u32 roundIdx, u32 step, u32 groupRankSize, u32 leftRankSize)
{
    UpdatePartialCommunicationRankSet(roundIdx, groupRankSize, partialCommRankSet_);
    CHK_RET(RunGroupFullMeshAlltoall(roundIdx, step));
    return HCCL_SUCCESS;
}

HcclResult AlltoAllVSymmetricMemory::RunSDMAFineGrained(u32 totalStep, HcclOpMetaInfoDef &opMeta){
    if (totalStep > 1){
        //细粒度场景不支持切分
        HCCL_ERROR("[AlltoAllVSymmetricMemory][RunSDMAFineGrained] AlltoAllV is not supported when totalStep[%u] > 1, "\
            "HCCL buffer is insufficient. stepSize : %u ", totalStep, algOpContext_.mc2Handler.stepSize);
        return HCCL_E_NOT_SUPPORT;
    } else if (totalStep == 0){
        //totalStep不需要通信，但是需要适配高阶API wait/write
        for (u32 roundIdx = 0; roundIdx < commRounds_; roundIdx++) {
            CHK_RET(mc2HandlerPub.Mc2WaitValue(dispatcher_, mainStream_, &(algOpContext_.mc2Handler), roundIdx));
            CHK_RET(mc2HandlerPub.Mc2WriteValue(dispatcher_, mainStream_, &(algOpContext_.mc2Handler)));
            HCCL_INFO("[AlltoAllVSymmetricMemory][RunSDMAFineGrained] step is 0 finished.");
        }
    } else {
        // totalStep == 1 细粒度修改
        u32 leftRankSize = devNumInlocalPod_; // leftRankSize中去掉本卡
        for (u32 roundIdx = 0; roundIdx < commRounds_ && leftRankSize > 0; roundIdx++) {
            CHK_RET(mc2HandlerPub.Mc2WaitValue(dispatcher_, mainStream_, &(algOpContext_.mc2Handler), roundIdx));
            CHK_RET(InitTask(dispatcher_, mainStream_, opMeta.isEnableCache, opMeta.GetCacheKey()));
            u32 groupRankSize = (leftRankSize > sdmaConcurrentNum_) ? sdmaConcurrentNum_ : leftRankSize;
            UpdateRemoteRankSet(roundIdx, groupRankSize);
            CHK_RET(RunGroupFullMeshAlltoall(roundIdx, 0));
            leftRankSize -= groupRankSize;
            CHK_RET(LaunchTaskExtend(dispatcher_, mainStream_, sdmaSubStream_));
            CHK_RET(mc2HandlerPub.Mc2WriteValue(dispatcher_, mainStream_, &(algOpContext_.mc2Handler)));
        }
        HCCL_INFO("[AlltoAllVSymmetricMemory][RunSDMAFineGrained] fine-grained finished.");
        return HCCL_SUCCESS;
    }

    if (totalStep == 0 && !islocalCpyDone_) {
        CHK_RET(InitTask(dispatcher_, mainStream_, opMeta.isEnableCache, opMeta.GetCacheKey()));
        CHK_RET(LocalCopy());
        islocalCpyDone_ = true;
        CHK_RET(LaunchTaskExtend(dispatcher_, mainStream_, sdmaSubStream_));
        return HCCL_SUCCESS;
    }

    HCCL_INFO("[AlltoAllVSymmetricMemory][RunSDMAFineGrained] finished.");
    return HCCL_SUCCESS;
}

HcclResult AlltoAllVSymmetricMemory::RunSDMA(HcclOpMetaInfoDef &opMeta)
{
    u32 totalStep = CalcNumSubStep();
    lastStep_ = totalStep - 1;
    // 计算每个rank分组fullmesh后需要通信的轮次，向上取整
    commRounds_ = (devNumInlocalPod_ + sdmaConcurrentNum_ - 1) / sdmaConcurrentNum_;
    u32 leftRankSize = devNumInlocalPod_ - 1; // leftRankSize中去掉本卡
    lastRoundIdx_ = std::min((leftRankSize + sdmaConcurrentNum_ - 1) / sdmaConcurrentNum_, static_cast<u32>(commRounds_)) - 1;
    HCCL_DEBUG("[AlltoAllVSymmetricMemory][RunSDMA] userRank [%u] communication rounds[%llu] totalStep [%u] "
        "stepSize [%u], post sync info: lastStep_[%u] lastRoundIdx_[%u] devNumInlocalPod_[%u] sdmaConcurrentNum_[%u]",
        userRank_, commRounds_, totalStep, algOpContext_.mc2Handler.stepSize,
        lastStep_, lastRoundIdx_, devNumInlocalPod_, sdmaConcurrentNum_);

    if (totalStep == 0 && !islocalCpyDone_) {
        CHK_RET(InitTask(dispatcher_, mainStream_, opMeta.isEnableCache, opMeta.GetCacheKey()));
        CHK_RET(LocalCopy());
        islocalCpyDone_ = true;
        CHK_RET(LaunchTaskExtend(dispatcher_, mainStream_, sdmaSubStream_));
        return HCCL_SUCCESS;
    }

    for (u32 step = 0; step < totalStep; step++) {
        u32 currentLeftRankSize = devNumInlocalPod_ - 1; // leftRankSize中去掉本卡
        for (u32 roundIdx = 0; roundIdx < commRounds_ && currentLeftRankSize > 0; roundIdx++) {
            CHK_RET(InitTask(dispatcher_, mainStream_, opMeta.isEnableCache, opMeta.GetCacheKey()));
            u32 groupRankSize = (currentLeftRankSize > sdmaConcurrentNum_) ? sdmaConcurrentNum_ : currentLeftRankSize;
            CHK_RET(RunSDMATasks(roundIdx, step, groupRankSize, currentLeftRankSize));
            currentLeftRankSize -= groupRankSize;
            CHK_RET(LaunchTaskExtend(dispatcher_, mainStream_, sdmaSubStream_));
        }
    }
    
    HCCL_INFO("[AlltoAllVSymmetricMemory][RunSDMA] finished.");
    return HCCL_SUCCESS;
}

HcclResult AlltoAllVFullMeshSymmetricMemory::RunAsync()
{   
    HcclOpMetaInfoDef opMeta = HcclOpMetaInfo::GetOneForAllToAllV(CopyPattern::ZCOPY, cclInMem_.size(), true);
    CHK_RET(InitTask(dispatcher_, mainStream_, opMeta.isEnableCache, opMeta.GetCacheKey()));

    if (userRankSize_ == 1) {
        HCCL_INFO("[AlltoAllVSymmetricMemory][RunAsync] do localcopy with 1 rank");
        CHK_RET(LocalCopy());
        return HCCL_SUCCESS;
    }

    if (devNumInlocalPod_ > 1) {
        CHK_RET(RunSDMA(opMeta));
    }

    HCCL_INFO("[AlltoAllVSymmetricMemory][RunAsync] finished.");
    return HCCL_SUCCESS;
}

HcclResult AlltoAllVSymmetricMemory::GetNslbAdjInfo(const u32 rank, const u32 rankSize,
                                                   const std::vector<LINK> &links, AdjInfo& nslbAdjInfo)
{
    (void) links;
    if (rankSize == 1) {
        return HCCL_SUCCESS;
    }

    u32 devNumInlocalPod = nslbAdjInfo.dstRankNum;
    u32 totalRdmaRankNum = rankSize - devNumInlocalPod;

    u32 rdmaConcurrentNum = (totalRdmaRankNum > ALLTOALLV_DIRECT_FULLMESH_RDMA_CONCURRENT_SIZE) ?
        (ALLTOALLV_DIRECT_FULLMESH_RDMA_CONCURRENT_SIZE) : (totalRdmaRankNum);
    if (rdmaConcurrentNum == 0) {
        return HCCL_SUCCESS;
    }
    // RDMA通信轮次
    u32 rdmaRoundNum = (totalRdmaRankNum + rdmaConcurrentNum - 1) / rdmaConcurrentNum;
    if (rdmaRoundNum == 0) {
        return HCCL_SUCCESS;
    }
    u32 currStage = rank / devNumInlocalPod;

    for (u32 step = 0; step < rdmaRoundNum; step++) {
        u32 sendTo =(rank + devNumInlocalPod + step) % rankSize;
        u32 sendToStag = sendTo / devNumInlocalPod;
        if(currStage == sendToStag) {
            //此时认为时同一个超节点内通讯
            sendTo = (sendTo + devNumInlocalPod) % rankSize;
        }
        NslbDpAdjInfo adjInfoStep = {0};
        adjInfoStep.dstLocalRankId = sendTo;
        adjInfoStep.phaseId = step + 1;
        adjInfoStep.rev = 0;
        nslbAdjInfo.nsAdjInfo.push_back(adjInfoStep);
    }
    nslbAdjInfo.dstRankNum = nslbAdjInfo.nsAdjInfo.size();
    return HCCL_SUCCESS;
}

REGISTER_TEMPLATE(TemplateType::TEMPLATE_ALL_2_ALL_V_DIRECT_FULL_MESH, AlltoAllVSymmetricMemory);
} // namespace hccl
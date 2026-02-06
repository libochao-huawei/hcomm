/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "coll_batch_send_recv_group_executor.h"

namespace hccl {
constexpr u64 BIG_DATA = 128 * 1024;

CollBatchSendRecvGroupExecutor::CollBatchSendRecvGroupExecutor(const HcclDispatcher dispatcher,
    std::unique_ptr<TopoMatcher> &topoMatcher)
    : CollBatchSendRecvExecutor(dispatcher, topoMatcher)
{
}

void CollBatchSendRecvGroupExecutor::ParseParam(const OpParam& param)
{
    tag_ = param.tag;
    HcclSendRecvItem* itemPtr = param.BatchSendRecvDataDes.sendRecvItemsPtr;
    u32 itemNum = param.BatchSendRecvDataDes.itemNum;
    if (itemPtr == nullptr) {
        HCCL_ERROR("[CollBatchSendRecvGroupExecutor][ParseParam] sendRecvInfo is nullptr.");
        return;
    }
    commTargetUserRankSet_.clear();
    for (u32 i = 0; i < itemNum; i++) {
        commTargetUserRankSet_.insert((itemPtr + i)->remoteRank);
        HCCL_INFO("[CollBatchSendRecvGroupExecutor][ParseParam] insert remoteUserRank[%u] to Set ",
            (itemPtr + i)->remoteRank);
    }
    for (u32 i = 0; i < itemNum; i++) {
        if ((itemPtr + i)->sendRecvType == HcclSendRecvType::HCCL_SEND) {
            dstRankSet_.insert((itemPtr + i)->remoteRank);
        }
        else if ((itemPtr + i)->sendRecvType == HcclSendRecvType::HCCL_RECV) {
            srcRankSet_.insert((itemPtr + i)->remoteRank);
        }
    }
    aicpuUnfoldMode_ = param.aicpuUnfoldMode;
}

HcclResult CollBatchSendRecvGroupExecutor::CalcIncreLinkRequest(const OpParam& param, std::set<u32>& ranksLinked,
    AlgResourceRequest& resourceRequest, bool& needIncreLink)
{     
    needIncreLink = false;
    (void)ParseParam(param);
    for (auto& remoteRank : commTargetUserRankSet_) {
        if (ranksLinked.find(remoteRank) == ranksLinked.end()) {
            needIncreLink = true;
            ranksLinked.insert(remoteRank);
            HCCL_INFO("[CollBatchSendRecvGroupExecutor][CalcIncreLinkRequest] Start insert remoteUserRank[%u] to "\
                "ranksLinked Set.", remoteRank);
        }
    }
    CHK_PRT_RET(!needIncreLink, HCCL_INFO("[CollBatchSendRecvGroupExecutor][CalcIncreLinkRequest] It's "\
        "unnecessary to incre alloc link."), HCCL_SUCCESS);

    u64 scratchMemSize = 0U;
    u32 streamNum = 0U;
    u32 notifyNum = 0U;
    u64 aivBufferRequest = 0U;
 
    std::vector<LevelNSubCommTransport> opTransport {
        std::vector<LevelNSubCommTransport>(static_cast<u32>(COMM_LEVEL_RESERVED))
    };
    CHK_RET(CalcCommInfo(opTransport));
    CHK_RET(BuildResourceRequest(scratchMemSize, streamNum, notifyNum, aivBufferRequest, opTransport, resourceRequest));
    return HCCL_SUCCESS;
}

HcclResult CollBatchSendRecvGroupExecutor::GetPairWiseList(HcclSendRecvItem *sendRecvInfo, u32 itemNum)
{
    HCCL_INFO("[CollBatchSendRecvGroupExecutor][GetPairWiseList] Start sort the batchSendRecv tasklist.");
    CHK_PTR_NULL(sendRecvInfo);

    for (u32 i = 0; i < itemNum; i++) {
        HCCL_INFO("[CollBatchSendRecvGroupExecutor][GetPairWiseList] index is %u, itemNum is %u, localRankID is %u, remoteRank is %u, "\
            "sendRecvType is %u, rankSize is %u.", i, itemNum, topoAttr_.userRank, sendRecvInfo->remoteRank,
            static_cast<u32>(sendRecvInfo->sendRecvType), topoAttr_.userRankSize);
        CHK_PTR_NULL(sendRecvInfo->buf);

        if (sendRecvInfo->sendRecvType == HcclSendRecvType::HCCL_SEND) {
            sendDeque_.push_back(sendRecvInfo);
        } else if (sendRecvInfo->sendRecvType == HcclSendRecvType::HCCL_RECV) {
            recvDeque_.push_back(sendRecvInfo);
        } else {
            HCCL_ERROR("[CollBatchSendRecvGroupExecutor][GetPairWiseList] sendRecvType wrong sendrecvType is %d, "\
                "rankID is %u, remoteRank is %u.", sendRecvInfo->sendRecvType, topoAttr_.userRank,
                sendRecvInfo->remoteRank);
            return HCCL_E_PARA;
        }
        sendRecvInfo++;
    }

    /* 此处的排序逻辑(pair-wise算法):
        1.sendDeque元素顺序是:先放remoteRank号小于等于root rank的第一个任务，依次减小(循环索引)直至放完
        2.recvDeque元素顺序是:先放remoteRank号大于等于root rank的第一个任务，依次增大(循环索引)直至放完
    */
    auto sendCompare = [this](HcclSendRecvItem* a, HcclSendRecvItem* b) {
        u32 aFlag = (a->remoteRank <= topoAttr_.userRank) ? (a->remoteRank + topoAttr_.userRankSize) : a->remoteRank;
        u32 bFlag = (b->remoteRank <= topoAttr_.userRank) ? (b->remoteRank + topoAttr_.userRankSize) : b->remoteRank;
        return aFlag > bFlag;
    };

    auto recvCompare = [this](HcclSendRecvItem* a, HcclSendRecvItem* b) {
        u32 aFlag = (a->remoteRank < topoAttr_.userRank) ? (a->remoteRank + topoAttr_.userRankSize) : a->remoteRank;
        u32 bFlag = (b->remoteRank < topoAttr_.userRank) ? (b->remoteRank + topoAttr_.userRankSize) : b->remoteRank;
        return aFlag < bFlag;
    };

    std::stable_sort(sendDeque_.begin(), sendDeque_.end(), sendCompare);
    std::stable_sort(recvDeque_.begin(), recvDeque_.end(), recvCompare);

    while ((!sendDeque_.empty() && sendDeque_.front()->remoteRank == topoAttr_.userRank) &&
        (!recvDeque_.empty() && recvDeque_.front()->remoteRank == topoAttr_.userRank)) {
            sendToSelfDeque_.push_back(sendDeque_.front());
            recvFromSelfDeque_.push_back(recvDeque_.front());
            sendDeque_.pop_front();
            recvDeque_.pop_front();
    }
    // 如果自发自收任务没有完全匹配
    if ((!sendDeque_.empty() && sendDeque_.front()->remoteRank == topoAttr_.userRank) || 
        (!recvDeque_.empty() && recvDeque_.front()->remoteRank == topoAttr_.userRank)) {
            HCCL_ERROR("[CollBatchSendRecvGroupExecutor] SendTask and Recv Task to rank itself do not match,"\
            "please check the task list.");
        return HCCL_E_PARA;
    }
    HCCL_INFO("[CollBatchSendRecvGroupExecutor][GetPairWiseList] End sort the batchSendRecv tasklist.");
    return HCCL_SUCCESS;
}

HcclResult CollBatchSendRecvGroupExecutor::ProcessSelfSendRecvTasks(Stream& stream)
{
    while (!sendToSelfDeque_.empty() && !recvFromSelfDeque_.empty()) {
        if (sendToSelfDeque_.front()->count == recvFromSelfDeque_.front()->count &&
            sendToSelfDeque_.front()->dataType == recvFromSelfDeque_.front()->dataType) {
            u64 dataSize = sendToSelfDeque_.front()->count * SIZE_TABLE[sendToSelfDeque_.front()->dataType];

            DeviceMem inUserMem = DeviceMem::create(static_cast<u8*>(sendToSelfDeque_.front()->buf), dataSize);
            DeviceMem outUserMem = DeviceMem::create(static_cast<u8*>(recvFromSelfDeque_.front()->buf), dataSize);
            CHK_RET(HcclD2DMemcpyAsync(dispatcher_, outUserMem, inUserMem, stream));
            sendToSelfDeque_.pop_front();
            recvFromSelfDeque_.pop_front();
        } else {
            HCCL_ERROR("[HcclBatchSendRecvGroup] Send task and recv task to self : count or dataType do not equal, please"\
                "check the task list.");
            return HCCL_E_PARA;
        }
    }
    HCCL_INFO("ProcessSelfSendRecvTasks Done!");
    return HCCL_SUCCESS;
}

HcclResult CollBatchSendRecvGroupExecutor::CalcBufferSliceSize()
{
    u32 bufferSliceNum = GROUP_MAX_CONCURRENT;
    u32 alignSize = HCCL_MIN_SLICE_ALIGN_910B; // 对齐
    bufferSliceSize_ = algResResp_->cclInputMem.size() / alignSize / bufferSliceNum * alignSize; // cclInp
    HCCL_INFO("[CollBatchSendRecvGroupExecutor][CalcBufferSliceSize] bufferSliceSize_[%llu]", bufferSliceSize_);
    return HCCL_SUCCESS;
}

HcclResult CollBatchSendRecvGroupExecutor::OrganizeSendItemBystream()
{
    sendQueueBySendstream_.resize(sendStreamNum_);
    sendStreamHasTask.resize(sendStreamNum_, false);
    HCCL_INFO("[OrganizeSendItemBystream] sendStreamNum_[%u]", sendStreamNum_);
    while (!sendDeque_.empty()) {
        HcclSendRecvItem* curr = sendDeque_.front();
        CHK_PTR_NULL(curr);
        sendQueueBySendstream_[curr->remoteRank % sendStreamNum_].push_back(curr); // cnt改为remoterank
        sendStreamHasTask[curr->remoteRank % sendStreamNum_] = true;
        sendDeque_.pop_front();
    }
    HCCL_INFO("OrganizeSendItemBystream Done!");
    return HCCL_SUCCESS;
}

HcclResult CollBatchSendRecvGroupExecutor::OrganizeRecvItemBystream()
{
    recvQueueByRecvstream_.resize(recvStreamNum_);
    recvStreamHasTask.resize(recvStreamNum_, false);
    HCCL_INFO("[OrganizeRecvItemBystream] recvStreamNum_[%u]", recvStreamNum_);
    while (!recvDeque_.empty()) {
        HcclSendRecvItem* curr = recvDeque_.front();
        CHK_PTR_NULL(curr);
        recvQueueByRecvstream_[curr->remoteRank % recvStreamNum_].push_back(curr); // cnt改为remoterank
        recvStreamHasTask[curr->remoteRank % recvStreamNum_] = true;
        recvDeque_.pop_front();
    }
    HCCL_INFO("OrganizeRecvItemBystream Done!");
    return HCCL_SUCCESS;
}

HcclResult CollBatchSendRecvGroupExecutor::isGroupBigCount(HcclSendRecvItem *sendRecvInfo, u32 itemNum, bool& isBig) {
    CHK_PTR_NULL(sendRecvInfo);

    for (u32 i = 0; i < itemNum; i++) {
        CHK_PTR_NULL(sendRecvInfo->buf);
        u32 unitSize = SIZE_TABLE[sendRecvInfo->dataType];
        if (sendRecvInfo->count * unitSize > BIG_DATA) { // 只要有一个big，就认为是big
            isBig = true;
            break;
        }
        sendRecvInfo++;
    }
    return HCCL_SUCCESS;
}

HcclResult CollBatchSendRecvGroupExecutor::Orchestrate(OpParam& param, AlgResourceResponse& algResource)
{
    HcclUs startut = TIME_NOW();
    HCCL_CONFIG_INFO(HCCL_ALG, "[CollBatchSendRecvGroupExecutor] groupsendrecv starts.");
    
    sendStreamNum_ = GROUP_MAX_CONCURRENT;
    recvStreamNum_ = GROUP_MAX_CONCURRENT;
    HCCL_INFO("[Orchestrate] sendStreamNum_[%u], recvStreamNum_[%u]", sendStreamNum_, recvStreamNum_);

    algResResp_ = &algResource;
    CHK_RET(CheckCommSize(COMM_COMBINE_ORDER, COMM_SIZE_TWO));
    CHK_RET(GetPairWiseList(param.BatchSendRecvDataDes.sendRecvItemsPtr, param.BatchSendRecvDataDes.itemNum));
    CHK_RET(ProcessSelfSendRecvTasks(param.stream));
    CHK_RET(CalcBufferSliceSize());
    if (topoAttr_.userRankSize == 1) {
        HCCL_INFO("tag[%s] BatchSendRecvGroup Excutor orchestrate success, take time [%lld]us.",
            param.tag.c_str(), DURATION_US(TIME_NOW() - startut));
        return HCCL_SUCCESS;
    }

    bool isBig = false;
    CHK_RET(isGroupBigCount(param.BatchSendRecvDataDes.sendRecvItemsPtr, param.BatchSendRecvDataDes.itemNum, isBig));
    if (isBig) {
        CHK_RET(OrganizeSendItemBystream());
        CHK_RET(OrganizeRecvItemBystream());

        CHK_RET(CalcSendSlices());
        CHK_RET(CalcRecvSlices());

        CHK_RET(RunLoopBig(param));
        
    } else {
        // 不用按照streamId入队
        CHK_RET(CalcSendSlicesSmall());
        CHK_RET(CalcRecvSlicesSmall());
        CHK_RET(RunLoopSmall(param));
    }

    HCCL_INFO("tag[%s] BatchSendRecvGroup Excutor orchestrate success, take time [%lld]us.",
        param.tag.c_str(), DURATION_US(TIME_NOW() - startut));
    return HCCL_SUCCESS;
}

HcclResult CollBatchSendRecvGroupExecutor::GetSendTargetLink(u32 remoteUserRank, LINK& targetLink) {
    u32 commIndex = 0;
    HCCL_INFO("[CollBatchSendRecvGroupExecutor][GetSendTargetLink] remoteUserRank[%u], localUserRank_[%u].",
        remoteUserRank, topoAttr_.userRank);
    if (remoteUserRank < topoAttr_.userRank) {
        HCCL_INFO("[CollBatchSendRecvGroupExecutor][GetSendTargetLink] CommIndex is 0.");
        commIndex = COMM_INDEX_0;
    } else if (remoteUserRank > topoAttr_.userRank) {
        HCCL_INFO("[CollBatchSendRecvGroupExecutor][GetSendTargetLink] CommIndex is 1.");
        commIndex = COMM_INDEX_1;
    } else {
        HCCL_ERROR("[CollBatchSendRecvGroupExecutor][GetSendTargetLink] CommIndex doesn't match.");
        return HCCL_E_PARA;
    }
    CHK_RET(GetTransport(commIndex, remoteUserRank, targetLink));
    CHK_SMART_PTR_NULL(targetLink);

    return HCCL_SUCCESS;
}

HcclResult CollBatchSendRecvGroupExecutor::GetRecvTargetLink(u32 remoteUserRank, LINK& targetLink) {
    u32 commIndex = 0;
    HCCL_INFO("[CollBatchSendRecvGroupExecutor][GetRecvTargetLink] remoteUserRank[%u], localUserRank_[%u].",
        remoteUserRank, topoAttr_.userRank);
    if (remoteUserRank > topoAttr_.userRank) {
        HCCL_INFO("[CollBatchSendRecvGroupExecutor][GetRecvTargetLink] CommIndex is 0.");
        commIndex = COMM_INDEX_0;
    } else if (remoteUserRank < topoAttr_.userRank) {
        HCCL_INFO("[CollBatchSendRecvGroupExecutor][GetRecvTargetLink] CommIndex is 1.");
        commIndex = COMM_INDEX_1;
    } else {
        HCCL_ERROR("[CollBatchSendRecvGroupExecutor][GetRecvTargetLink] CommIndex doesn't match.");
        return HCCL_E_PARA;
    }
    CHK_RET(GetTransport(commIndex, remoteUserRank, targetLink));
    CHK_SMART_PTR_NULL(targetLink);

    return HCCL_SUCCESS;
}

HcclResult CollBatchSendRecvGroupExecutor::MainNotifySub(Stream& mainStream)
{
    // 还有任务的stream，需要跟主流同步，目的是为了使得本地拷贝跟跨卡的拷贝分开
    for (u32 i = 0; i < sendStreamNum_; i++) {
        if (!sendDataSilcesBySendstream_[i].empty()) {
            CHK_RET(LocalNotify::Wait(mainStream, dispatcher_, algResResp_->notifiesMain[i], PROF_STAGE_0));
            HCCL_INFO("MainWait, sendStreamId[%u]", i);
        }
    }
    for (u32 i = 0; i < recvStreamNum_; i++) {
        if (!recvDataSilcesByRecvstream_[i].empty()) {
            CHK_RET(LocalNotify::Wait(mainStream, dispatcher_, algResResp_->notifiesMain[sendStreamNum_ + i], PROF_STAGE_0));
            HCCL_INFO("MainWait, recvStreamId[%u]", i);
        }
    }

    for (u32 i = 0; i < sendStreamNum_; i++) {
        if (!sendDataSilcesBySendstream_[i].empty()) {
            CHK_RET(LocalNotify::Post(mainStream, dispatcher_, algResResp_->notifiesAux[i], PROF_STAGE_0));
            HCCL_INFO("MainPost, sendStreamId[%u]", i);
        }
    }
    for (u32 i = 0; i < recvStreamNum_; i++) {
        if (!recvDataSilcesByRecvstream_[i].empty()) {
            CHK_RET(LocalNotify::Post(mainStream, dispatcher_, algResResp_->notifiesAux[sendStreamNum_ + i], PROF_STAGE_0));
            HCCL_INFO("MainPost, recvStreamId[%u]", i);
        }
    }
    HCCL_INFO("Main Wait Post Done");
    return HCCL_SUCCESS;
}

HcclResult CollBatchSendRecvGroupExecutor::MainPostSubWait(Stream& mainStream)
{
    // 主流通知所有从流开始
    for (u32 i  = 0; i < sendStreamNum_; i++){
        if (!sendDataSilcesBySendstream_[i].empty()){ // 只对有任务的流进行同步，减少开销
            CHK_RET(LocalNotify::Post(mainStream, dispatcher_, algResResp_->notifiesAux[i], PROF_STAGE_0));
            CHK_RET(LocalNotify::Wait(algResResp_->slaveStreams[i], dispatcher_, algResResp_->notifiesAux[i], PROF_STAGE_0));
            HCCL_INFO("MainPost, Send[%u] Wait", i);
        }
    }

    for (u32 i  = 0; i < recvStreamNum_; i++){
        if (!recvDataSilcesByRecvstream_[i].empty()){
            CHK_RET(LocalNotify::Post(mainStream, dispatcher_, algResResp_->notifiesAux[i + sendStreamNum_], PROF_STAGE_0));
            CHK_RET(LocalNotify::Wait(algResResp_->slaveStreams[i + sendStreamNum_], dispatcher_, algResResp_->notifiesAux[i + sendStreamNum_], PROF_STAGE_0));
            HCCL_INFO("MainPost, Recv[%u] Wait", i);
        }
    }

    return HCCL_SUCCESS;
}

HcclResult CollBatchSendRecvGroupExecutor::MainWaitSubPost(Stream& mainStream)
{
    // 最后主流等待所有从流结束
    for (u32 i  = 0; i < sendStreamNum_; i++){
        if (sendStreamHasTask[i]){ // 只对有任务的流进行同步，减少开销
            CHK_RET(LocalNotify::Wait(mainStream, dispatcher_, algResResp_->notifiesMain[i], PROF_STAGE_0));
            CHK_RET(LocalNotify::Post(algResResp_->slaveStreams[i], dispatcher_, algResResp_->notifiesMain[i], PROF_STAGE_0));
            HCCL_INFO("MainWait, Send[%u] Post", i);
        }
    }

    for (u32 i  = 0; i < recvStreamNum_; i++){
        if (recvStreamHasTask[i]){
            CHK_RET(LocalNotify::Post(algResResp_->slaveStreams[i + sendStreamNum_], dispatcher_, algResResp_->notifiesMain[i + sendStreamNum_], PROF_STAGE_0));
            CHK_RET(LocalNotify::Wait(mainStream, dispatcher_, algResResp_->notifiesMain[i + sendStreamNum_], PROF_STAGE_0));
            HCCL_INFO("MainWait, Recv[%u] Post", i);
        }
    }
    return HCCL_SUCCESS;
}

HcclResult CollBatchSendRecvGroupExecutor::CopyLocalDataForARound(Stream& mainStream)
{
    // for (u32 i = 0; i < sendStreamNum_; i++){
    //     if (!sendDataSilcesBySendstream_[i].empty()){ // 只对有任务的流进行同步，减少开销
    //         CHK_RET(LocalNotify::Post(mainStream, dispatcher_, algResResp_->notifiesAux[i], PROF_STAGE_0));
    //         CHK_RET(LocalNotify::Wait(algResResp_->slaveStreams[i], dispatcher_, algResResp_->notifiesAux[i], PROF_STAGE_0));
    //         HCCL_INFO("MainPost, Send[%u] Wait", i);
    //     }
    // }
    for (u32 i = 0; i < sendStreamNum_; i++){
        if (sendDataSilcesBySendstream_[i].empty()) {
            continue;
        }
        SendRecvSlice& slice = sendDataSilcesBySendstream_[i].front();
        DeviceMem inMem(slice.addr, slice.size);
        u64 offset = bufferSliceSize_ * i;
        DeviceMem inCommMem = algResResp_->cclInputMem.range(offset, slice.size);
        HCCL_INFO("[ProcessSendStreamDataSlice] inMem ptr[%p], size[%llu]", inMem.ptr(), inMem.size());
        HCCL_INFO("[ProcessSendStreamDataSlice] inCommMem ptr[%p], size[%llu]", inCommMem.ptr(), inCommMem.size());
        CHK_RET(HcclD2DMemcpyAsync(dispatcher_, inCommMem, inMem, mainStream));
    }
    // for (u32 i = 0; i < sendStreamNum_; i++){
    //     if (!sendDataSilcesBySendstream_[i].empty()){ // 只对有任务的流进行同步，减少开销
    //         CHK_RET(LocalNotify::Wait(mainStream, dispatcher_, algResResp_->notifiesMain[i], PROF_STAGE_0));
    //         CHK_RET(LocalNotify::Post(algResResp_->slaveStreams[i], dispatcher_, algResResp_->notifiesMain[i], PROF_STAGE_0));
    //         HCCL_INFO("MainWait, Send[%u] Post", i);
    //     }
    // }
    return HCCL_SUCCESS;
}

HcclResult CollBatchSendRecvGroupExecutor::RunTasksBig(OpParam& param)
{
    for (u32 i = 0; i < sendStreamNum_; i++) {
        if (!sendDataSilcesBySendstream_[i].empty()) nonEmptySendStream++;
    }
    for (u32 i = 0; i < recvStreamNum_; i++) {
        if (!recvDataSilcesByRecvstream_[i].empty()) nonEmptyRecvStream++;
    }

        // CHK_RET(MainPostSubWait(param.stream));
    while (nonEmptySendStream > 0 || nonEmptyRecvStream > 0) {
        CHK_RET(CopyLocalDataForARound(param.stream));
        CHK_RET(AlgTemplateBase::ExecEmptyTask(algResResp_->cclInputMem, algResResp_->cclOutputMem, param.stream, dispatcher_));
        HCCL_INFO("nonEmptySendStream[%u], nonEmptyRecvStream[%u]", nonEmptySendStream, nonEmptyRecvStream);

        for (u32 i = 0; i < sendStreamNum_; i++){
            if (!sendDataSilcesBySendstream_[i].empty()){ // 只对有任务的流进行同步，减少开销
                CHK_RET(LocalNotify::Post(mainStream, dispatcher_, algResResp_->notifiesAux[i], PROF_STAGE_0));
                CHK_RET(LocalNotify::Wait(algResResp_->slaveStreams[i], dispatcher_, algResResp_->notifiesAux[i], PROF_STAGE_0));
                HCCL_INFO("MainPost, Send[%u] Wait", i);
            }
        }

        for (u32 i = 0; i < sendStreamNum_; i++){
            if (!sendDataSilcesBySendstream_[i].empty()) {
                CHK_RET(ProcessSendStreamDataSlice(algResResp_->slaveStreams[i], i, false, false));
            }
        }
        for (u32 i = 0; i < sendStreamNum_; i++){
            if (!sendDataSilcesBySendstream_[i].empty()){ // 只对有任务的流进行同步，减少开销
                CHK_RET(LocalNotify::Wait(mainStream, dispatcher_, algResResp_->notifiesMain[i], PROF_STAGE_0));
                CHK_RET(LocalNotify::Post(algResResp_->slaveStreams[i], dispatcher_, algResResp_->notifiesMain[i], PROF_STAGE_0));
                HCCL_INFO("MainWait, Send[%u] Post", i);
            }
        }



        for (u32 i  = 0; i < recvStreamNum_; i++){
            if (!recvDataSilcesByRecvstream_[i].empty()){
                CHK_RET(LocalNotify::Post(algResResp_->slaveStreams[sendStreamNum_ + sendStreamNum_], dispatcher_, algResResp_->notifiesAux[i + sendStreamNum_], PROF_STAGE_0));
                CHK_RET(LocalNotify::Wait(algResResp_->slaveStreams[i + sendStreamNum_], dispatcher_, algResResp_->notifiesAux[i + sendStreamNum_], PROF_STAGE_0));
                HCCL_INFO("MainPost, Recv[%u] Wait", i);
            }
        }

        CHK_RET(AlgTemplateBase::ExecEmptyTask(algResResp_->cclInputMem, algResResp_->cclOutputMem, param.stream, dispatcher_));

        for (u32 i = 0; i < recvStreamNum_; i++){
            if (!recvDataSilcesByRecvstream_[i].empty()) {
                CHK_RET(ProcessRecvStreamDataSlice(algResResp_->slaveStreams[i + sendStreamNum_], i, false, false));
            }
        }

        for (u32 i  = 0; i < recvStreamNum_; i++){
            if (recvStreamHasTask[i]){
                CHK_RET(LocalNotify::Post(algResResp_->slaveStreams[i + sendStreamNum_], dispatcher_, algResResp_->notifiesMain[i + sendStreamNum_], PROF_STAGE_0));
                CHK_RET(LocalNotify::Wait(algResResp_->slaveStreams[sendStreamNum_ + sendStreamNum_], dispatcher_, algResResp_->notifiesMain[i + sendStreamNum_], PROF_STAGE_0));
                HCCL_INFO("MainWait, Recv[%u] Post", i);
            }
        }

        // 更新DataSlices
        for (u32 i = 0; i < sendStreamNum_; i++){
            if (!sendDataSilcesBySendstream_[i].empty()) {
                sendDataSilcesBySendstream_[i].pop_front();
                if (sendDataSilcesBySendstream_[i].empty()) {
                    --nonEmptySendStream;
                    HCCL_INFO("[RunLoopBig] nonEmptySendStream[%u]", nonEmptySendStream);
                }
            }
        }
        for (u32 i = 0; i < recvStreamNum_; i++){
            if (!recvDataSilcesByRecvstream_[i].empty()) {
                recvDataSilcesByRecvstream_[i].pop_front();
                if (recvDataSilcesByRecvstream_[i].empty()) {
                    --nonEmptyRecvStream;
                    HCCL_INFO("[RunLoopBig] nonEmptyRecvStream[%u]", nonEmptyRecvStream);
                }
            }
        }
        CHK_RET(AlgTemplateBase::ExecEmptyTask(algResResp_->cclInputMem, algResResp_->cclOutputMem, param.stream, dispatcher_));
        CHK_RET(LaunchTaskExtend(dispatcher_, param.stream, algResResp_->slaveStreams));
    }
        // CHK_RET(MainWaitSubPost(param.stream));
    return HCCL_SUCCESS;
}

HcclResult CollBatchSendRecvGroupExecutor::RunLoopBig(OpParam& param)
{
    if (topoMatcher_->GetExternalInputHcclEnableFfts()) {
        auto meta = HcclOpMetaInfo::GetOneForBatchSendRecv();
        CHK_RET(InitTask(dispatcher_, param.stream, meta.isEnableCache, meta.GetCacheKey()));
        // 多流子图前后需加空拷贝
        CHK_RET(AlgTemplateBase::ExecEmptyTask(algResResp_->cclInputMem, algResResp_->cclOutputMem, param.stream,
            dispatcher_));
    }
    bool IsSetNormalMode = false; // 设置过一次就不需要再设置了
    for (u32 i = 0; i < sendDataSilces_.size(); ++i) {
        SendRecvSlice& slice = sendDataSilces_[i];
        LINK targetLink;
        CHK_RET(GetSendTargetLink(slice.remoteRank, targetLink));
        if (targetLink->GetTransportType() == TransportType::TRANS_TYPE_DEVICE_DIRECT) {
            CHK_RET(SetNormalMode(dispatcher_));
            IsSetNormalMode = true;
            HCCL_INFO("[CollBatchSendRecvGroupExecutor][RunLoopBig]Send Set NormalMode dispatcher");
            break;
        }
    }

    for (u32 i = 0; i < recvDataSilces_.size() && !IsSetNormalMode; ++i) {
        SendRecvSlice& slice = recvDataSilces_[i];
        LINK targetLink;
        CHK_RET(GetRecvTargetLink(slice.remoteRank, targetLink));
        if (targetLink->GetTransportType() == TransportType::TRANS_TYPE_DEVICE_DIRECT) {
            CHK_RET(SetNormalMode(dispatcher_));
            HCCL_INFO("[CollBatchSendRecvGroupExecutor][RunLoopBig]Recv Set NormalMode dispatcher");
            break;
        }
    }
    CHK_RET(RunTasksBig(param));
    if (topoMatcher_->GetExternalInputHcclEnableFfts()) {
        // 多流子图前后需加空拷贝
        CHK_RET(AlgTemplateBase::ExecEmptyTask(algResResp_->cclInputMem, algResResp_->cclOutputMem, param.stream, dispatcher_));
        CHK_RET(LaunchTaskExtend(dispatcher_, param.stream, algResResp_->slaveStreams));
        HCCL_INFO("LaunchTaskExtend!");
    }
    return HCCL_SUCCESS;
}

HcclResult CollBatchSendRecvGroupExecutor::RunLoopSmall(OpParam& param)
{
    if (topoMatcher_->GetExternalInputHcclEnableFfts()) {
        auto meta = HcclOpMetaInfo::GetOneForBatchSendRecv();
        CHK_RET(InitTask(dispatcher_, param.stream, meta.isEnableCache, meta.GetCacheKey()));
        // 多流子图前后需加空拷贝
        CHK_RET(AlgTemplateBase::ExecEmptyTask(algResResp_->cclInputMem, algResResp_->cclOutputMem, param.stream,
            dispatcher_));
    }
    bool IsSetNormalMode = false; // 设置过一次就不需要再设置了
    for (u32 i = 0; i < sendDataSilces_.size(); ++i) {
        SendRecvSlice& slice = sendDataSilces_[i];
        LINK targetLink;
        CHK_RET(GetSendTargetLink(slice.remoteRank, targetLink));
        if (targetLink->GetTransportType() == TransportType::TRANS_TYPE_DEVICE_DIRECT) {
            CHK_RET(SetNormalMode(dispatcher_));
            IsSetNormalMode = true;
            HCCL_INFO("[CollBatchSendRecvGroupExecutor][RunLoopBig]Send Set NormalMode dispatcher");
            break;
        }
    }

    for (u32 i = 0; i < recvDataSilces_.size() && !IsSetNormalMode; ++i) {
        SendRecvSlice& slice = recvDataSilces_[i];
        LINK targetLink;
        CHK_RET(GetRecvTargetLink(slice.remoteRank, targetLink));
        if (targetLink->GetTransportType() == TransportType::TRANS_TYPE_DEVICE_DIRECT) {
            CHK_RET(SetNormalMode(dispatcher_));
            HCCL_INFO("[CollBatchSendRecvGroupExecutor][RunLoopBig]Recv Set NormalMode dispatcher");
            break;
        }
    }

    CHK_RET(ProcessDataSliceSmall(param));

    if (topoMatcher_->GetExternalInputHcclEnableFfts()) {
        // 多流子图前后需加空拷贝
        CHK_RET(AlgTemplateBase::ExecEmptyTask(algResResp_->cclInputMem,
            algResResp_->cclOutputMem, param.stream, dispatcher_));
        CHK_RET(LaunchTaskExtend(dispatcher_, param.stream, algResResp_->slaveStreams));
        HCCL_INFO("LaunchTaskExtend!");
    }
    return HCCL_SUCCESS;
}

HcclResult CollBatchSendRecvGroupExecutor::GetAdjInfo(AlgResourceResponse& algRes, AdjInfo& adjInfo)
{
    algResResp_ = &algRes;
    SubCommInfo level1CommInfo = {0};
    AdjInfo nslbAdjInfo = {0};
    if (Getlevel1CommRank(level1CommInfo) != HCCL_SUCCESS) {
        return HCCL_SUCCESS;
    }
    u32 localRank= level1CommInfo.localRank;
    u32 localRankSize = level1CommInfo.localRankSize;

    std::unique_ptr<AlgTemplateBase> level1TempAlg;
    if (SelectTempAlg(level1TempAlg, localRankSize) != HCCL_SUCCESS) {
        return HCCL_SUCCESS;
    }
    if(level1TempAlg == nullptr) {
        return HCCL_SUCCESS;
    }
    CHK_RET(level1TempAlg->GetNslbAdjInfo(localRank, localRankSize, level1CommInfo.links, nslbAdjInfo));

    adjInfo.dstRankNum = nslbAdjInfo.dstRankNum;
    HCCL_INFO("[nslbdp] adjInfo.dstRankNum[%u].", adjInfo.dstRankNum);
    
    for (size_t i = 0; i < nslbAdjInfo.nsAdjInfo.size(); i++) {
        NslbDpAdjInfo dpAdjInfo = {0};
        dpAdjInfo.dstLocalRankId = nslbAdjInfo.nsAdjInfo[i].dstLocalRankId;
        dpAdjInfo.phaseId = nslbAdjInfo.nsAdjInfo[i].phaseId;
        dpAdjInfo.rev = 0;
        adjInfo.nsAdjInfo.push_back(dpAdjInfo); 
        HCCL_INFO("[nslbdp]GetAdjInfo dstLocalRankId[%u], phaseId[%u].",
                   nslbAdjInfo.nsAdjInfo[i].dstLocalRankId, nslbAdjInfo.nsAdjInfo[i].phaseId);
    }
    return HCCL_SUCCESS;
}

HcclResult CollBatchSendRecvGroupExecutor::CalcSendSlices()
{
    sendDataSilcesBySendstream_.resize(sendStreamNum_);
    for (u32 i = 0; i < sendStreamNum_; i++) { // 按sendStream来放入dataSlice
        auto sendQueueInner = sendQueueBySendstream_[i];
        while (!sendQueueInner.empty()) {
            HcclSendRecvItem* sendRecvItem = sendQueueInner.front();
            HCCL_INFO("[CollBatchSendRecvGroupExecutor][CalcSendSlices] remoteRank[%u], buf[%p], count[%llu],"\
                "dataType[%s], sendRecvType[%d].", sendRecvItem->remoteRank, sendRecvItem->buf,
                sendRecvItem->count, GetDataTypeEnumStr(sendRecvItem->dataType).c_str(), sendRecvItem->sendRecvType);
            u8 *curInputPtr = static_cast<u8 *>(sendRecvItem->buf);
            CHK_PTR_NULL(curInputPtr);
            u32 unitSize = SIZE_TABLE[sendRecvItem->dataType];
            u64 maxCountPerLoop = CalcSendLoopMaxCount(unitSize);

            for (u64 countLeft = sendRecvItem->count, curCount = 0, curOffset = 0; countLeft > 0;
                countLeft -= curCount) {
                curInputPtr += curOffset;
                curCount = (countLeft > maxCountPerLoop) ? maxCountPerLoop : countLeft;
                u64 curSize = curCount * unitSize; // 单位：字节
                sendDataSilcesBySendstream_[i].emplace_back(curInputPtr, curSize, sendRecvItem->remoteRank);
                HCCL_DEBUG("[CollBatchSendRecvGroupExecutor][CalcSendSlices], slice userAddr[%p], slice size[%llu], remoteRank[%u].", curInputPtr, curSize, sendRecvItem->remoteRank);
                curOffset = curSize;
            }
            sendQueueInner.pop_front();
        }
    }
    for (u32 i = 0; i < sendStreamNum_; i++) {
        auto sendQueueInner = sendDataSilcesBySendstream_[i];
        for (auto slice : sendQueueInner){
            HCCL_INFO("[CalcSendSlices] sendstream[%u] slice.addr[%p], slice.size[%llu] slice.remoteRank[%u]", i, slice.addr, slice.size, slice.remoteRank);
        }
    }
    return HCCL_SUCCESS;
}

HcclResult CollBatchSendRecvGroupExecutor::CalcRecvSlices()
{
    recvDataSilcesByRecvstream_.resize(recvStreamNum_);
    for (u32 i = 0; i < recvStreamNum_; i++) { // 按recvStream来放入dataSlice
        auto recvQueueInner = recvQueueByRecvstream_[i];
        while (!recvQueueInner.empty()) {
            HcclSendRecvItem* sendRecvItem = recvQueueInner.front();
            HCCL_INFO("[CollBatchSendRecvGroupExecutor][CalcRecvSlices], remoteRank[%u], buf[%p], count[%llu],"\
                "dataType[%s], recvRecvType[%d].", sendRecvItem->remoteRank, sendRecvItem->buf,
                sendRecvItem->count, GetDataTypeEnumStr(sendRecvItem->dataType).c_str(), sendRecvItem->sendRecvType);
            u8 *curInputPtr = static_cast<u8 *>(sendRecvItem->buf);
            CHK_PTR_NULL(curInputPtr);
            u32 unitSize = SIZE_TABLE[sendRecvItem->dataType];
            u64 maxCountPerLoop = CalcRecvLoopMaxCount(unitSize);

            for (u64 countLeft = sendRecvItem->count, curCount = 0, curOffset = 0; countLeft > 0;
                countLeft -= curCount) {
                curInputPtr += curOffset;
                curCount = (countLeft > maxCountPerLoop) ? maxCountPerLoop : countLeft;
                u64 curSize = curCount * unitSize; // 单位：字节
                recvDataSilcesByRecvstream_[i].emplace_back(curInputPtr, curSize, sendRecvItem->remoteRank);
                HCCL_DEBUG("[CollBatchSendRecvGroupExecutor][CalcRecvSlices] tag[%s], slice userAddr[%p], slice size[%llu].",
                    tag_.c_str(), curInputPtr, curSize);
                curOffset = curSize;
            }
            recvQueueInner.pop_front();
        }
    }
    for (u32 i = 0; i < recvStreamNum_; i++) {
        auto recvQueueInner = recvDataSilcesByRecvstream_[i];
        for (auto slice : recvQueueInner){
            HCCL_INFO("[CalcRecvSlices] recvstream[%u] slice.addr[%p], slice.size[%llu] slice.remoteRank[%u]", i, slice.addr, slice.size, slice.remoteRank);
        }
    }
    return HCCL_SUCCESS;
}

HcclResult CollBatchSendRecvGroupExecutor::CalcSendSlicesSmall()
{
    while (!sendDeque_.empty()) {
        HcclSendRecvItem* sendRecvItem = sendDeque_.front();
        HCCL_INFO("[CollBatchSendRecvGroupExecutor][CalcSendSlicesSmall] tag[%s], remoteRank[%u], buf[%p], count[%llu],"\
            "dataType[%s], sendRecvType[%d].", tag_.c_str(), sendRecvItem->remoteRank, sendRecvItem->buf,
            sendRecvItem->count, GetDataTypeEnumStr(sendRecvItem->dataType).c_str(), sendRecvItem->sendRecvType);
        u8 *curInputPtr = static_cast<u8 *>(sendRecvItem->buf);
        CHK_PTR_NULL(curInputPtr);
        u32 unitSize = SIZE_TABLE[sendRecvItem->dataType];
        u64 maxCountPerLoop = CalcSendLoopMaxCount(unitSize);

        for (u64 countLeft = sendRecvItem->count, curCount = 0, curOffset = 0; countLeft > 0;
            countLeft -= curCount) {
            curInputPtr += curOffset;
            curCount = (countLeft > maxCountPerLoop) ? maxCountPerLoop : countLeft;
            u64 curSize = curCount * unitSize; // 单位：字节
            sendDataSilces_.emplace_back(curInputPtr, curSize, sendRecvItem->remoteRank);
            HCCL_INFO("[CollBatchSendRecvGroupExecutor][CalcSendSlicesSmall] slice userAddr[%p], slice size[%llu], remoteRank[%u]", curInputPtr, curSize, sendRecvItem->remoteRank);
            curOffset = curSize;
        }
        sendDeque_.pop_front();
    }
    return HCCL_SUCCESS;
}

HcclResult CollBatchSendRecvGroupExecutor::CalcRecvSlicesSmall()
{
    while (!recvDeque_.empty()) {
        HcclSendRecvItem* sendRecvItem = recvDeque_.front();
        HCCL_INFO("[CollBatchSendRecvGroupExecutor][CalcRecvSlicesSmall] tag[%s], remoteRank[%u], buf[%p], count[%llu],"\
            "dataType[%s], sendRecvType[%d].", tag_.c_str(), sendRecvItem ->remoteRank, sendRecvItem ->buf, sendRecvItem->count,
            GetDataTypeEnumStr(sendRecvItem->dataType).c_str(), sendRecvItem->sendRecvType);
        u8 *curOutputPtr = static_cast<u8*>(sendRecvItem->buf);
        CHK_PTR_NULL(curOutputPtr);
        u32 unitSize = SIZE_TABLE[sendRecvItem->dataType];
        u64 maxCountPerLoop = CalcRecvLoopMaxCount(unitSize);

        for (u64 countLeft = sendRecvItem->count, curCount = 0, curOffset = 0; countLeft > 0;
            countLeft -= curCount) {
            curOutputPtr += curOffset;
            curCount = (countLeft > maxCountPerLoop) ? maxCountPerLoop : countLeft;
            u64 curSize = curCount * unitSize; // 单位：字节
            recvDataSilces_.emplace_back(curOutputPtr, curSize, sendRecvItem->remoteRank);
            HCCL_INFO("[CollBatchSendRecvGroupExecutor][CalcRecvSlicesSmall] slice userAddr[%p], slice size[%llu], remoteRank[%u]", curOutputPtr, curSize, sendRecvItem->remoteRank);
            curOffset = curSize;
        }
        recvDeque_.pop_front();
    }
    return HCCL_SUCCESS;
}

HcclResult CollBatchSendRecvGroupExecutor::SubNotifyMain(Stream& stream, u32 streamId)
{
    CHK_RET(LocalNotify::Post(stream, dispatcher_, algResResp_->notifiesMain[streamId], PROF_STAGE_0));
    CHK_RET(LocalNotify::Wait(stream, dispatcher_, algResResp_->notifiesAux[streamId], PROF_STAGE_0));
    return HCCL_SUCCESS;
}

HcclResult CollBatchSendRecvGroupExecutor::ProcessSendStreamDataSlice(Stream& stream, u32 sendStreamId, bool needStreamSync, 
    bool retryEnable)
{
    SendRecvSlice& slice = sendDataSilcesBySendstream_[sendStreamId].front();
    
    DeviceMem inMem(slice.addr, slice.size);
    u64 offset = bufferSliceSize_ * sendStreamId;
    DeviceMem inCommMem = algResResp_->cclInputMem.range(offset, slice.size);
    HCCL_INFO("[ProcessSendStreamDataSlice] inMem ptr[%p], size[%llu]", inMem.ptr(), inMem.size());
    HCCL_INFO("[ProcessSendStreamDataSlice] inCommMem ptr[%p], size[%llu]", inCommMem.ptr(), inCommMem.size());

    if (needStreamSync) {
        CHK_RET(SubNotifyMain(stream, sendStreamId));
    }

    ExecMem execMem;
    execMem.inputMem = inCommMem;
    HcclResult ret = SendKernelRun(stream, execMem, slice.remoteRank, retryEnable);
    CHK_PRT_RET(ret != HCCL_SUCCESS,
        HCCL_ERROR("[CollBatchSendRecvGroupExecutor][ProcessSendStreamDataSlice]errNo[0x%016llx]kernel run error, tag[%s], " \
        "input_ptr[%p], size[%llu]", HCCL_ERROR_CODE(ret), tag_.c_str(), execMem.inputMem.ptr(),
        slice.size), ret);
    return HCCL_SUCCESS;
}

HcclResult CollBatchSendRecvGroupExecutor::ProcessRecvStreamDataSlice(Stream& stream, u32 recvStreamId, bool needStreamSync, 
    bool retryEnable)
{
    SendRecvSlice& slice = recvDataSilcesByRecvstream_[recvStreamId].front();
    
    ExecMem execMem;
    if (needStreamSync) {
        CHK_RET(SubNotifyMain(stream, recvStreamId + sendStreamNum_));
    }
    LINK targetLink;
    CHK_RET(GetRecvTargetLink(slice.remoteRank, targetLink));
    if (topoAttr_.isDiffDeviceType || topoAttr_.superPodNum > 1 || 
            (topoAttr_.moduleNum > 1 && topoMatcher_->GetExternalInputInterHccsDisable()) ||
            (topoAttr_.deviceType == DevType::DEV_TYPE_910B && targetLink->GetLinkType() == LinkType::LINK_ROCE)) {
        
        u64 offset = bufferSliceSize_ * recvStreamId;
        execMem.outputMem = algResResp_->cclOutputMem.range(offset, slice.size);
        HcclResult ret = RecvKernelRun(stream, execMem, slice.remoteRank, retryEnable);
        CHK_PRT_RET(ret != HCCL_SUCCESS,
            HCCL_ERROR("[CollBatchSendRecvGroupExecutor][ProcessRecvStreamDataSlice]errNo[0x%016llx]kernel run error, tag[%s], " \
            "output_ptr[%p], size[%llu]", HCCL_ERROR_CODE(ret), tag_.c_str(), execMem.outputMem.ptr(),
            slice.size), ret);

        DeviceMem outMem(slice.addr, slice.size);
        DeviceMem outCommMem = execMem.outputMem;
        HCCL_INFO("[ProcessRecvStreamDataSlice] outMem ptr[%p], size[%llu]", outMem.ptr(), outMem.size());
        HCCL_INFO("[ProcessRecvStreamDataSlice] outCommMem ptr[%p], size[%llu]", outCommMem.ptr(), outCommMem.size());
        CHK_RET(HcclD2DMemcpyAsync(dispatcher_, outMem, outCommMem, stream));
    } else {
        DeviceMem outMem(slice.addr, slice.size);
        execMem.outputMem = outMem;
        HCCL_INFO("[ProcessRecvStreamDataSlice] DMA Reduce, outMem ptr[%p], size[%llu]", outMem.ptr(), outMem.size());
        HcclResult ret = RecvKernelRun(stream, execMem, slice.remoteRank, retryEnable);
        CHK_PRT_RET(ret != HCCL_SUCCESS,
            HCCL_ERROR("[CollBatchSendRecvGroupExecutor][ProcessRecvStreamDataSlice]errNo[0x%016llx]kernel run error, tag[%s], " \
            "input_ptr[%p], size[%llu]", HCCL_ERROR_CODE(ret), tag_.c_str(), execMem.inputMem.ptr(),
            slice.size), ret);
    }
    return HCCL_SUCCESS;
}

HcclResult CollBatchSendRecvGroupExecutor::ProcessDataSliceSmall(OpParam& param)
{
    CHK_RET(MainPostSubWaitSmall(param.stream, algResResp_->slaveStreams[STREAM_INDEX_0]));
    while (!sendDataSilces_.empty() || !recvDataSilces_.empty()) {
        if(!sendDataSilces_.empty()) {
            CHK_RET(ProcessSendDataSliceSmall(param.stream, false, false));
            sendDataSilces_.pop_front();
        }
        if(!recvDataSilces_.empty()) {
            CHK_RET(ProcessRecvDataSliceSmall(algResResp_->slaveStreams[STREAM_INDEX_0], false, false));
            recvDataSilces_.pop_front();
        }
    }
    CHK_RET(MainWaitSubPostSmall(param.stream, algResResp_->slaveStreams[STREAM_INDEX_0]));
    return HCCL_SUCCESS;
}

HcclResult CollBatchSendRecvGroupExecutor::MainPostSubWaitSmall(Stream& mainStream, Stream& subStream)
{
    CHK_RET(LocalNotify::Post(mainStream, dispatcher_, algResResp_->notifiesAux[STREAM_INDEX_0], PROF_STAGE_0));
    CHK_RET(LocalNotify::Wait(subStream, dispatcher_,
        algResResp_->notifiesAux[STREAM_INDEX_0], PROF_STAGE_0));
    return HCCL_SUCCESS;
}

HcclResult CollBatchSendRecvGroupExecutor::MainWaitSubPostSmall(Stream& mainStream, Stream& subStream)
{
    CHK_RET(LocalNotify::Post(subStream, dispatcher_,
        algResResp_->notifiesMain[STREAM_INDEX_0], PROF_STAGE_0));

    CHK_RET(LocalNotify::Wait(mainStream, dispatcher_, algResResp_->notifiesMain[STREAM_INDEX_0],
        PROF_STAGE_0));
    return HCCL_SUCCESS;
}

HcclResult CollBatchSendRecvGroupExecutor::ProcessSendDataSliceSmall(Stream& stream, bool needStreamSync, bool retryEnable)
{
    SendRecvSlice& slice = sendDataSilces_.front();
    
    DeviceMem inMem(slice.addr, slice.size);
    u32 sendStreamId = slice.remoteRank % sendStreamNum_;
    u64 offset = bufferSliceSize_ * sendStreamId;
    DeviceMem inCommMem = algResResp_->cclInputMem.range(offset, slice.size);
    HCCL_INFO("[ProcessSendStreamDataSlice] inMem ptr[%p], size[%llu]", inMem.ptr(), inMem.size());
    HCCL_INFO("[ProcessSendStreamDataSlice] inCommMem ptr[%p], size[%llu]", inCommMem.ptr(), inCommMem.size());
    CHK_RET(HcclD2DMemcpyAsync(dispatcher_, inCommMem, inMem, stream));

    if (needStreamSync) {
        CHK_RET(SubNotifyMain(stream, sendStreamId));
    }

    ExecMem execMem;
    execMem.inputMem = inCommMem;
    HcclResult ret = SendKernelRun(stream, execMem, slice.remoteRank, retryEnable);
    CHK_PRT_RET(ret != HCCL_SUCCESS,
        HCCL_ERROR("[CollBatchSendRecvGroupExecutor][ProcessSendStreamDataSlice]errNo[0x%016llx]kernel run error, tag[%s], " \
        "input_ptr[%p], size[%llu]", HCCL_ERROR_CODE(ret), tag_.c_str(), execMem.inputMem.ptr(),
        slice.size), ret);
    return HCCL_SUCCESS;
}

HcclResult CollBatchSendRecvGroupExecutor::ProcessRecvDataSliceSmall(Stream& stream, bool needStreamSync, bool retryEnable)
{
    SendRecvSlice& slice = recvDataSilces_.front();
    u32 recvStreamId = slice.remoteRank % recvStreamNum_;
    
    ExecMem execMem;
    if (needStreamSync) {
        CHK_RET(SubNotifyMain(stream, recvStreamId + sendStreamNum_));
    }
    LINK targetLink;
    CHK_RET(GetRecvTargetLink(slice.remoteRank, targetLink));
    if (topoAttr_.isDiffDeviceType || topoAttr_.superPodNum > 1 || 
            (topoAttr_.moduleNum > 1 && topoMatcher_->GetExternalInputInterHccsDisable()) ||
            (topoAttr_.deviceType == DevType::DEV_TYPE_910B && targetLink->GetLinkType() == LinkType::LINK_ROCE)) {
        u64 offset = bufferSliceSize_ * recvStreamId;
        execMem.outputMem = algResResp_->cclOutputMem.range(offset, slice.size);
        HcclResult ret = RecvKernelRun(stream, execMem, slice.remoteRank, retryEnable);
        CHK_PRT_RET(ret != HCCL_SUCCESS,
            HCCL_ERROR("[CollBatchSendRecvGroupExecutor][ProcessRecvStreamDataSlice]errNo[0x%016llx]kernel run error, tag[%s], " \
            "output_ptr[%p], size[%llu]", HCCL_ERROR_CODE(ret), tag_.c_str(), execMem.outputMem.ptr(),
            slice.size), ret);

        DeviceMem outMem(slice.addr, slice.size);
        DeviceMem outCommMem = execMem.outputMem;
        HCCL_INFO("[ProcessRecvStreamDataSlice] outMem ptr[%p], size[%llu]", outMem.ptr(), outMem.size());
        HCCL_INFO("[ProcessRecvStreamDataSlice] outCommMem ptr[%p], size[%llu]", outCommMem.ptr(), outCommMem.size());
        CHK_RET(HcclD2DMemcpyAsync(dispatcher_, outMem, outCommMem, stream));
    } else {
        DeviceMem outMem(slice.addr, slice.size);
        execMem.outputMem = outMem;
        HCCL_INFO("[ProcessRecvStreamDataSlice] DMA Reduce, outMem ptr[%p], size[%llu]", outMem.ptr(), outMem.size());
        HcclResult ret = RecvKernelRun(stream, execMem, slice.remoteRank, retryEnable);
        CHK_PRT_RET(ret != HCCL_SUCCESS,
            HCCL_ERROR("[CollBatchSendRecvGroupExecutor][ProcessRecvStreamDataSlice]errNo[0x%016llx]kernel run error, tag[%s], " \
            "input_ptr[%p], size[%llu]", HCCL_ERROR_CODE(ret), tag_.c_str(), execMem.inputMem.ptr(),
            slice.size), ret);
    }
    return HCCL_SUCCESS;
}

HcclResult CollBatchSendRecvGroupExecutor::SendKernelRun(Stream& stream, ExecMem &execMem, u32 remoteUserRank,
    bool retryEnable)
{
    LINK targetLink;
    CHK_RET(GetSendTargetLink(remoteUserRank, targetLink));
    SendReceive executor(dispatcher_, targetLink, INVALID_VALUE_RANKID, HCCL_CHUNK_SIZE, retryEnable);
    CHK_RET(executor.SendPrepare(execMem.inputMem, remoteUserRank, stream));
    CHK_RET(executor.RegisterProfiler(0, PROF_STAGE_0, HCCL_EXEC_STEP_NOT_SET, stream));
    CHK_RET(executor.BatchSendRunAsync());

    return HCCL_SUCCESS;
}

HcclResult CollBatchSendRecvGroupExecutor::RecvKernelRun(Stream& stream, ExecMem &execMem, u32 remoteUserRank,
    bool retryEnable)
{
    LINK targetLink;
    CHK_RET(GetRecvTargetLink(remoteUserRank, targetLink));
    SendReceive executor(dispatcher_, targetLink, INVALID_VALUE_RANKID, HCCL_CHUNK_SIZE, retryEnable);
    CHK_RET(executor.ReceivePrepare(execMem.outputMem, remoteUserRank, stream));
    CHK_RET(executor.RegisterProfiler(0, PROF_STAGE_0, HCCL_EXEC_STEP_NOT_SET, stream));
    CHK_RET(executor.BatchReceiveRunAsync());
    return HCCL_SUCCESS;
}

HcclResult CollBatchSendRecvGroupExecutor::GetTransport(u32 commIndex, u32 remoteUserRank, LINK &targetLink)
{
    CHK_PRT_RET(commIndex >= algResResp_->opTransportResponse[COMM_COMBINE_ORDER].size(),
        HCCL_ERROR("[CollBatchSendRecvGroupExecutor][KernelRun] batchsendrecv op commIndex[%u] is larger than "\
        "opTransportResponse size[%zu]",
        remoteUserRank, algResResp_->opTransportResponse[COMM_COMBINE_ORDER].size()), HCCL_E_PARA);
    SingleSubCommTransport &commCombined =
        const_cast<SingleSubCommTransport&>(algResResp_->opTransportResponse[COMM_COMBINE_ORDER][commIndex]);

    CHK_PRT_RET(remoteUserRank >= commCombined.userRank2subCommRank.size(),
        HCCL_ERROR("[CollBatchSendRecvGroupExecutor][KernelRun] batchsendrecv op remoteUserRank[%u] is larger than "\
        "userRank2subCommRank map size[%zu]",
        remoteUserRank, commCombined.userRank2subCommRank.size()), HCCL_E_PARA);

    u32 remoteRank = commCombined.userRank2subCommRank[remoteUserRank];
    CHK_PRT_RET(remoteRank >= commCombined.links.size(),
        HCCL_ERROR("[CollBatchSendRecvGroupExecutor][KernelRun] batchsendrecv op remoteUserRank[%u], get remoteRank[%u]," \
        "the size of combinedComm links is [%zu]", remoteUserRank, remoteRank, commCombined.links.size()),
        HCCL_E_PARA);
    targetLink = commCombined.links[remoteRank];
    return HCCL_SUCCESS;
}

u64 CollBatchSendRecvGroupExecutor::CalcSendLoopMaxCount(const u32 unitSize)
{
    // 中转内存单次最多能够接受的input count
    u64 maxCountPerLoop = bufferSliceSize_ / unitSize;
    HCCL_WARNING("[CollBatchSendRecvGroupExecutor][CalcSendLoopMaxCount]" \
        "using default maxCountPerLoop[%llu] as CCLBuffSize / unitSize.", maxCountPerLoop);
    return maxCountPerLoop;
}

u64 CollBatchSendRecvGroupExecutor::CalcRecvLoopMaxCount(const u32 unitSize)
{
    // 中转内存单次最多能够接受的output count
    u64 maxCountPerLoop = bufferSliceSize_ / unitSize;
    HCCL_WARNING("[CollBatchSendRecvGroupExecutor][CalcRecvLoopMaxCount]" \
        "using default maxCountPerLoop[%llu] as CCLBuffSize / unitSize.", maxCountPerLoop);
    return maxCountPerLoop;
}

HcclResult CollBatchSendRecvGroupExecutor::CalcStreamNum(u32& streamNum)
{
    sendStreamNum_ = GROUP_MAX_CONCURRENT;
    recvStreamNum_ = GROUP_MAX_CONCURRENT;
    streamNum = sendStreamNum_ + recvStreamNum_ + 1; // 有限度并发， 最后多加一条主流用于recv的同步
    HCCL_INFO("[CollBatchSendRecvGroupExecutor][CalcStreamNum] tag_[%s], streamNum[%u].", tag_.c_str(), streamNum);
    return HCCL_SUCCESS;
}

HcclResult CollBatchSendRecvGroupExecutor::CalcCommInfo(std::vector<LevelNSubCommTransport>& opTransport)
{
    CommParaInfo commParaInfo(COMM_COMBINE_ORDER, CommType::COMM_TAG_PARTIAL_MESH_COMBINED, INVALID_VALUE_RANKID,
    INVALID_VALUE_RANKID, false, false, commTargetUserRankSet_);
    TransportMemType inputType = TransportMemType::CCL_INPUT;
    TransportMemType outputType = TransportMemType::CCL_OUTPUT;

    CHK_RET(CalcCommPlaneInfo(tag_, commParaInfo, opTransport[COMM_COMBINE_ORDER], inputType, outputType));
    return HCCL_SUCCESS;
}

REGISTER_EXEC("BatchSendRecvGroup", BatchSendRecvGroupExecutor, CollBatchSendRecvGroupExecutor);
} // namespace hccl
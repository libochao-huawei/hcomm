/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef COLL_BATCH_SEND_RECV_GROUP_EXECUTOR_H
#define COLL_BATCH_SEND_RECV_GROUP_EXECUTOR_H

#include "coll_comm_executor.h"
#include "coll_batch_send_recv_executor.h"

namespace hccl {
class CollBatchSendRecvGroupExecutor : public CollBatchSendRecvExecutor {
public:
    CollBatchSendRecvGroupExecutor(const HcclDispatcher dispatcher, std::unique_ptr<TopoMatcher> &topoMatcher);
    ~CollBatchSendRecvGroupExecutor() = default;
    HcclResult Orchestrate(OpParam& param, AlgResourceResponse& algResource) override;
    HcclResult GetAdjInfo(AlgResourceResponse& algRes, AdjInfo& adjInfo) override;
    // 增量建链资源计算接口
    HcclResult CalcIncreLinkRequest(const OpParam& param, std::set<u32>& ranksLinked, AlgResourceRequest& resourceRequest, bool& needIncreLink)
        override;
    HcclResult GetSendTargetLink(u32 remoteUserRank, LINK& targetLink);
    HcclResult GetRecvTargetLink(u32 remoteUserRank, LINK& targetLink);
protected:
    /* *************** 资源计算 *************** */
    void ParseParam(const OpParam& param) override;
    HcclResult CalcCommInfo(std::vector<LevelNSubCommTransport>& opTransport) override;

    /* *************** 算法编排 *************** */
    u64 CalcSendLoopMaxCount(const u32 unitSize);
    u64 CalcRecvLoopMaxCount(const u32 unitSize);
    HcclResult ProcessSendStreamDataSlice(Stream& stream, u32 sendStreamId, bool needStreamSync, bool retryEnable);
    HcclResult ProcessRecvStreamDataSlice(Stream& stream, u32 recvStreamId, bool needStreamSync, bool retryEnable);
    HcclResult ProcessSendDataSliceSmall(Stream& stream, bool needStreamSync, bool retryEnable);
    HcclResult ProcessRecvDataSliceSmall(Stream& stream, bool needStreamSync, bool retryEnable);
    HcclResult ProcessDataSliceSmall(OpParam& param);
    HcclResult CalcSendSlices();
    HcclResult CalcRecvSlices();
    HcclResult CalcSendSlicesSmall();
    HcclResult CalcRecvSlicesSmall();
    HcclResult CopyLocalDataForARound(Stream& mainStream);
    HcclResult OrganizeSendSlices();
    HcclResult OrganizeRecvSlices();
    HcclResult OrganizeSendItemBystream();
    HcclResult OrganizeRecvItemBystream();
    struct SendRecvSlice {
        u8* addr;
        u64 size;
        u32 remoteRank;
        SendRecvSlice(u8* addr, u64 size, u32 remoteRank) : addr(addr), size(size), remoteRank(remoteRank) {}
    };

    u32 remoteUserRank_ = 0;
    const u32 MAX_LOOP_IN_ONCE_LAUNCH = 200;
    std::deque<SendRecvSlice> sendDataSilces_;
    std::deque<SendRecvSlice> recvDataSilces_;
    std::vector<std::deque<SendRecvSlice>> sendDataSilcesBySendstream_;
    std::vector<std::deque<SendRecvSlice>> recvDataSilcesByRecvstream_;
    u32 nonEmptySendStream = 0;
    u32 nonEmptyRecvStream = 0;
private:
    HcclResult RunLoopBig(OpParam& param);
    HcclResult RunTasksBig(OpParam& param);
    HcclResult RunLoopSmall(OpParam& param);
    HcclResult CalcStreamNum(u32& streamNum) override;
    HcclResult GetPairWiseList(HcclSendRecvItem *sendRecvInfo, u32 itemNum);
    HcclResult isGroupBigCount(HcclSendRecvItem *sendRecvInfo, u32 itemNum, bool& isSmall);
    HcclResult ProcessSelfSendRecvTasks(Stream& stream);
    HcclResult CalcBufferSliceSize();

    HcclResult MainNotifySub(Stream& mainStream);
    HcclResult MainPostSubWait(Stream& mainStream);
    HcclResult MainWaitSubPost(Stream& mainStream);
    HcclResult SubNotifyMain(Stream& stream, u32 streamId);
    HcclResult MainPostSubWaitSmall(Stream& mainStream, Stream& subStream);
    HcclResult MainWaitSubPostSmall(Stream& mainStream, Stream& subStream);
    
    HcclResult SendKernelRun(Stream& stream, ExecMem &execMem, u32 remoteUserRank, bool retryEnable);
    HcclResult RecvKernelRun(Stream& stream, ExecMem &execMem, u32 remoteUserRank, bool retryEnable);
    HcclResult GetTransport(u32 commIndex, u32 remoteUserRank, LINK &targetLink);

private:

    std::set<u32> commTargetUserRankSet_;
    std::unordered_set<u32> srcRankSet_;
    std::unordered_set<u32> dstRankSet_;
    std::deque<HcclSendRecvItem*> sendToSelfDeque_;
    std::deque<HcclSendRecvItem*> recvFromSelfDeque_;
    std::deque<HcclSendRecvItem*> sendDeque_;
    std::deque<HcclSendRecvItem*> recvDeque_;
    std::vector<std::deque<HcclSendRecvItem*>> sendQueueBySendstream_;
    std::vector<std::deque<HcclSendRecvItem*>> recvQueueByRecvstream_;
    std::vector<bool> sendStreamHasTask;
    std::vector<bool> recvStreamHasTask;
    u32 sendStreamNum_ = 0;
    u32 recvStreamNum_ = 0;
    u64 bufferSliceSize_ = 0;
};
} // namespace hccl

#endif
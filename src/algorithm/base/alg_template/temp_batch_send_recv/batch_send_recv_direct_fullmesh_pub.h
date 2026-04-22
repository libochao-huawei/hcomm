/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef BATCH_SEND_RECV_DIRECT_FULLMESH_PUB_H
#define BATCH_SEND_RECV_DIRECT_FULLMESH_PUB_H

#include "alg_template_base_pub.h"
#include "coll_batch_send_recv_direct_fullmesh_executor.h"

namespace hccl {

class BatchSendRecvDirectFullMesh : public AlgTemplateBase {
public:
    explicit BatchSendRecvDirectFullMesh(const HcclDispatcher dispatcher);
    ~BatchSendRecvDirectFullMesh() override;

    HcclResult RunAsync() override;
    HcclResult Prepare(PrepareData &param) override;
    HcclResult SetBatchSendRecvInfo(const BatchSendRecvInfo* batchSendRecvInfo);

private:
    HcclResult GenerateSubStreamInfo(const std::vector<Stream> &subStreams,
        const std::vector<std::shared_ptr<LocalNotify>> &meshSignalMainToSub,
        const std::vector<std::shared_ptr<LocalNotify>> &meshSignalSubToMain);

    u64 CalcMaxSendLen();
    u32 CalcNumSubStep();

    void UpdateRemoteRankSet(u32 roundIdx, u32 groupRankSize);
    void UpdatePartialCommunicationRankSet(u32 roundIdx, u32 groupRankSize,
        std::vector<std::vector<std::pair<u32, u32>>> &partialCommRankSet);

    HcclResult UpdateCurrRankRecvInfo(u32 step, u32 roundIdx, u32 side, u32 destRank,
        std::vector<ReadDataBlock>& readInfo, u32 maxRecvStep);
    HcclResult UpdateCurrRankSendInfo(u32 step, u32 roundIdx, u32 side, u32 destRank,
        std::vector<SendDataBlock>& sendInfo, u32 maxSendStep);

    void UpdateSendRecvInfo(u32 step, u32 roundIdx,
        std::unordered_map<u32, std::vector<ReadDataBlock>> &subStreamReadInfo,
        std::unordered_map<u32, std::vector<SendDataBlock>> &subStreamSendInfo,
        const std::vector<std::vector<std::pair<u32, u32>>> &partialCommRankSet);

    void UpdateOpBaseSubStreamInfo(u32 step, u32 roundIdx);

    HcclResult PrepareIntraData(u32 step,
        std::unordered_map<u32, std::vector<SendDataBlock>> &subStreamSendInfo);

    HcclResult NotifySubStreamStart();
    HcclResult WaitSubStreamFinish();
    HcclResult NotifyLocalSubStreamStart();
    HcclResult WaitLocalSubStreamFinish();

    HcclResult NotifyRemoteRankStart(u32 step);
    HcclResult SDMAwithRemoteRankAndNotifyEnd(u32 step, u32 roundIdx);
    HcclResult SendRecvData(u32 step, u32 roundIdx);

    HcclResult LocalCopy();

    HcclResult RunGroupFullMeshBatchSendRecv(u32 roundIdx, u32 step);
    HcclResult RunSDMATasks(u32 roundIdx, u32 step, u32 groupRankSize, u32 leftRankSize);
    HcclResult RunSDMA();

    HcclResult MainNotifyRdmaControlStart();
    HcclResult RdmaControlNotifyMainFinish();
    HcclResult RdmaControlNotifySubStart();
    HcclResult SubNotifyRdmaControlFinish();

    u32 GetNextDstRank(u32& curDstRank);
    u32 GetPreSrcRank(u32& curSrcRank);

    void GenRdmaSendInfo(u32 dstRank, std::vector<SendDataBlock>& sendInfo);
    void GenRdmaRecvInfo(u32 srcRank, std::vector<RecvDataBlock>& recvInfo);

    HcclResult CopyDataForSend(u32 dstRank, std::vector<SendDataBlock>& sendInfo, u32 curStep, Stream stream);
    HcclResult SendRecvRdmaData(u32 dstRank, u32 srcRank, std::vector<SendDataBlock>& sendInfo,
        std::vector<RecvDataBlock>& recvInfo, u32 round, u32 index, u32 curStep, Stream stream);
    HcclResult CopyRecvDataToOutput(u32 srcRank, std::vector<RecvDataBlock>& recvInfo, u32 curStep, Stream stream);

    HcclResult ProcessSingleGroupRdmaData(std::vector<u32>& dstRanks, std::vector<u32>& srcRanks, u32 round);
    HcclResult ProcessRdmaData();
    HcclResult RunRDMA();

    std::string GetStreamIndexString();

    Stream mainStream_;
    u32 userRank_ = 0;
    u32 userRankSize_ = 0;
    u32 podStartRank_ = 0;
    u32 podEndRank_ = 0;
    std::vector<LINK> links_;
    const BatchSendRecvInfo* batchSendRecvInfoPtr_ = nullptr;
    u32 devNumInlocalPod_ = 0;
    u32 rankIdxInPod_ = 0;
    u32 totalRdmaRankNum_ = 0;
    bool isSuPodAsym_ = false;
    bool isBigCount_ = false;

    DeviceMem userInput_;
    DeviceMem userOutput_;
    DeviceMem cclInMem_;
    DeviceMem cclOutMem_;
    HcclWorkflowMode workMode_;
    u64 sdmaDataBlockSize_ = 0;

    bool islocalCpyDone_ = false;
    std::unordered_map<u32, std::vector<SendDataBlock>> subStreamSendInfo_;
    std::unordered_map<u32, std::vector<ReadDataBlock>> subStreamReadInfo_;
    std::unordered_map<u32, std::vector<SendDataBlock>> nextSubStreamSendInfo_;
    std::unordered_map<u32, std::vector<ReadDataBlock>> nextSubStreamReadInfo_;
    std::unordered_map<u32, u32> sendNumSubStep_;
    std::unordered_map<u32, u32> recvNumSubStep_;
    u32 sdmaConcurrentNum_ = 0;
    std::vector<std::vector<std::pair<u32, u32>>> partialCommRankSet_;
    std::vector<std::vector<std::pair<u32, u32>>> nextPartialCommRankSet_;
    u64 commRounds_ = 0;

    std::vector<Stream> localSubStream_;
    std::vector<std::shared_ptr<LocalNotify>> localSignalMainToSub_;
    std::vector<std::shared_ptr<LocalNotify>> localSignalSubToMain_;

    std::vector<Stream> sdmaSubStream_;
    std::vector<std::shared_ptr<LocalNotify>> sdmaMeshSignalMainToSub_;
    std::vector<std::shared_ptr<LocalNotify>> sdmaMeshSignalSubToMain_;

    u64 rdmaDataBlockSize_ = 0;
    u32 rdmaConcurrentNum_ = 0;
    std::shared_ptr<LocalNotify> main2RdmaControlStreamNotify_;
    std::shared_ptr<LocalNotify> rdmaControl2MainStreamNotify_;
    std::vector<Stream> rdmaSubStreams_;
    std::vector<std::shared_ptr<LocalNotify>> rdmaControl2SubNotifies_;
    std::vector<std::shared_ptr<LocalNotify>> rdmaSub2ControlNotifies_;
};

} // namespace hccl

#endif /* BATCH_SEND_RECV_DIRECT_FULLMESH_PUB_H */

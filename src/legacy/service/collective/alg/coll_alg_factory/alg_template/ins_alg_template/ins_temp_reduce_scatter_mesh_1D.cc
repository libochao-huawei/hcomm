/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "ins_temp_reduce_scatter_mesh_1D.h"
#include "log.h"
#include "alg_data_trans_wrapper.h"

namespace Hccl {
InsTempReduceScatterMesh1D::InsTempReduceScatterMesh1D(const RankId virtualRank, const u32 tempRankSize,
    const std::vector<std::vector<RankId>> &tempVTopo, const std::map<RankId, u32> &tempVirtRankMap)
    : InsAlgTemplateBase(virtualRank, tempRankSize, tempVTopo, tempVirtRankMap)
{}

InsTempReduceScatterMesh1D::~InsTempReduceScatterMesh1D()
{}

HcclResult InsTempReduceScatterMesh1D::CalcRes(AlgTempResReq &tempResReq)
{
    CHK_PRT_RET(
        CalcResLinksMesh(myRank_, tempRankSize_, tempVTopo_, linkNumBtwPeers_, tempResReq) != HcclResult::HCCL_SUCCESS,
        HCCL_ERROR("[CollAlgFactory] [InsTempReduceScatterMesh1D] Rank [%d], resLinks calculation error!", myRank_),
        HcclResult::HCCL_E_INTERNAL);
    auto &linkReq = tempResReq.links;
    u32 pathNum = 0;
    for (auto resReqIter = linkReq.begin(); resReqIter != linkReq.end(); resReqIter++) {
        auto remoteRank = resReqIter->first;
        if (rank2PathNumMap_.find(remoteRank) == rank2PathNumMap_.end() || rank2PathNumMap_[remoteRank] == 0) {
            HCCL_ERROR("[InsTempReduceScatterMesh1D] No path to remoteRank[%u]", remoteRank);
            return HcclResult::HCCL_E_INTERNAL;
        }
        if (pathNum == 0) {
            pathNum = rank2PathNumMap_[remoteRank];
        } else if (rank2PathNumMap_[remoteRank] != pathNum) {
            HCCL_ERROR("[InsTempReduceScatterMesh1D] Inconsistency pathNum to remoteRanks, Previous consistent pathNum=[%u], mismatched "
                       "remoteRank=[%u], pathNum=[%u]",
                pathNum,
                remoteRank,
                rank2PathNumMap_[remoteRank]);
            return HcclResult::HCCL_E_INTERNAL;
        }
        resReqIter->second = pathNum;
    }

    // Mesh 需要的 que Num 为 tempVTopo_[0].size()-1
    tempResReq.queNum = (tempVTopo_[0].size() > 1) ? (tempVTopo_[0].size() - 1) * pathNum: pathNum;
    tempResReq.streamNum = tempResReq.queNum;
    tempResReq.queNotifys = CreateMasterSlaveQueNotifiesRequest(tempResReq.queNum);
    QId centerQ = 0;
    tempResReq.localWaitGroupCntNotify.emplace_back(centerQ, 0);
    tempResReq.localBcastPostCntNotify.emplace_back(centerQ, 0);

    return HcclResult::HCCL_SUCCESS;
}

u64 InsTempReduceScatterMesh1D::CalcScratchMultiple(const BufferType &inBuffType, const BufferType &outBuffType) const
{
    (void)inBuffType;
    (void)outBuffType;
    u64 scratchMultiple = tempRankSize_;
    return scratchMultiple;
}

HcclResult InsTempReduceScatterMesh1D::GenExtIns(const TempFuncs &tempFuncs, const TemplateDataParams &tempAlgParams,
    const ResLinks &tempLinks, std::vector<InsQuePtr> &tempInsQues)
{
    opMode_ = tempFuncs.opMode;
    enableCounterNotify_ = tempFuncs.enableCounterNotify;
    queNum_ = tempVTopo_[0].size() - 1;
    HCCL_INFO("[InsTempReduceScatterMesh1D] Run Start");
    uint32_t linkNum = tempLinks.begin()->second.size();
    CHK_PRT_RET(linkNum > tempInsQues.size(), HCCL_ERROR("[CollAlgFactory] [InsTempReduceScatterMesh1D] Rank [%d], requiredQue Error.", myRank_),
            HcclResult::HCCL_E_INTERNAL);

    u64 typeSize = DataTypeSizeGet(dataType_);
    u64 dataCnt = tempAlgParams.sliceSize / typeSize;
    std::vector<RankSliceInfo> sliceInfoVecForAllLinks(linkNum);
    std::vector<float> dataSplitRate(linkNum);
    CHK_RET(CalcDataSplitRateForLinks(tempLinks.begin()->second, dataSplitRate));
    u64 linkSliceOffset = 0;
    for (u32 linkIdx = 0; linkIdx < linkNum; linkIdx++) {
        processSize_.emplace_back();
        linkSliceOffsetVec_.emplace_back();
        HCCL_INFO("[InsTempReduceScatterMesh1D] linkIdx =%u",linkIdx);
        if (linkIdx != linkNum - 1) {
            processSize_[linkIdx] = static_cast<u64>(static_cast<float>(dataCnt) * dataSplitRate[linkIdx]) * typeSize;
        } else {
            processSize_[linkIdx] = tempAlgParams.sliceSize - linkSliceOffset;
        }
        linkSliceOffsetVec_[linkIdx] = linkSliceOffset;
        linkSliceOffset += processSize_[linkIdx];
    }

    std::vector<std::vector<InsQuePtr>> tempInsQuesVec;
    u32 queNumPerlink = tempInsQues.size() / linkNum;
    for (uint32_t linkIdx = 0; linkIdx < linkNum; linkIdx++) {
        tempInsQuesVec.emplace_back(
            tempInsQues.begin() + linkIdx * queNumPerlink, tempInsQues.begin() + (linkIdx + 1) * queNumPerlink);
    }
    if (tempInsQues.size() > 1) {
        CHK_RET(PreSyncInterQueues(tempInsQues));
    }
    HCCL_INFO("[InsTempReduceScatterMesh1D][PreCopy] Rank [%d].", myRank_);
    for (uint32_t linkIdx = 0; linkIdx < linkNum; linkIdx++) {
        CHK_RET(RunReduceScatter(tempLinks, tempInsQuesVec[linkIdx], tempAlgParams, linkIdx));
    }
    HCCL_INFO("[InsTempReduceScatterMesh1D][PostCopy] Rank [%d].", myRank_);
    // 流间后同步，从流通知主流
    if (tempInsQues.size() > 1) {
        CHK_RET(PostSyncInterQueues(tempInsQues));
    }
    if (tempInsQues.size() > 1) {
        CHK_RET(PreSyncInterQueues(tempInsQues));
    }
    for (uint32_t linkIdx = 0; linkIdx < linkNum; linkIdx++) {
        PostCopy(tempAlgParams, tempInsQuesVec[linkIdx], linkIdx);
    }
    if (tempInsQues.size() > 1) {
        CHK_RET(PostSyncInterQueues(tempInsQues));
    }
    return HcclResult::HCCL_SUCCESS;
}

HcclResult InsTempReduceScatterMesh1D::PostCopy(
    const TemplateDataParams &tempAlgParams, std::vector<InsQuePtr> &tempInsQues, u32 linkIdx)
{
    // 通信结束之后，数据都在 inbuff 上，需要搬运到对应的输出位置。
    u32 rankIdx = tempVirtRankMap_[myRank_];
    // 如果是单算子模式, 并且是最后一步算子，需要将数据从 inBuff 拷贝到 userOut
    // 是否需要将数据搬运到 OutBuff 上再搬运到 UserOut 上？？
    HCCL_INFO("[InsTempReduceScatterMesh1D][PostCopy], copy from outBuff to userOut");
    // 先把本卡的数据从input搬运到output
    HCCL_INFO("[InsTempReduceScatterMesh1D][PostCopy]tempAlgParams.repeatNum=%llu", tempAlgParams.repeatNum);
    for (u32 repeatIdx = 0; repeatIdx < tempAlgParams.repeatNum; repeatIdx++) {
        DataSlice myRankSlice = DataSlice(tempAlgParams.buffInfo.inBuffType,
            tempAlgParams.buffInfo.inBuffBaseOff + repeatIdx * tempAlgParams.inputRepeatStride +
                rankIdx * tempAlgParams.inputSliceStride + linkSliceOffsetVec_[linkIdx],
            processSize_[linkIdx]);
        DataSlice outputSlice = DataSlice(tempAlgParams.buffInfo.outBuffType,
            tempAlgParams.buffInfo.outBuffBaseOff + repeatIdx * tempAlgParams.outputRepeatStride + linkSliceOffsetVec_[linkIdx],
            processSize_[linkIdx]);
        CHK_RET(LocalCopy(tempInsQues[0], myRankSlice, outputSlice));
        // 把其他卡的数据input累加到output
        for (u32 tmpRank = 0; tmpRank < tempRankSize_; tmpRank++) {
            if (tmpRank != rankIdx) {
                DataSlice srcDataSlice = DataSlice(tempAlgParams.buffInfo.scratBuffType,
                    tempAlgParams.buffInfo.scratchBuffBaseOff + repeatIdx * tempAlgParams.outputRepeatStride +
                        tmpRank * tempAlgParams.outputSliceStride + linkSliceOffsetVec_[linkIdx],
                    processSize_[linkIdx]);
                DataSlice dstDataSlice = DataSlice(tempAlgParams.buffInfo.outBuffType,
                    tempAlgParams.buffInfo.outBuffBaseOff + repeatIdx * tempAlgParams.outputRepeatStride + linkSliceOffsetVec_[linkIdx],
                    processSize_[linkIdx]);  
                CHK_RET(LocalReduce(tempInsQues[0], srcDataSlice, dstDataSlice, dataType_, redOp_));
            }
        }
    }
    return HcclResult::HCCL_SUCCESS;
}

HcclResult InsTempReduceScatterMesh1D::RunReduceScatter(const ResLinks &tempLinks, std::vector<InsQuePtr> &tempInsQues,
    const TemplateDataParams &tempAlgParams, u32 linkIdx)
{
    u32 myAlgRank;
    CHK_RET(GetAlgRank(myRank_, tempVTopo_[0], myAlgRank));
    for (u32 queIdx = 0; queIdx < queNum_; queIdx++) {
        u32 nextRank = (myAlgRank + 1 + queIdx) % tempRankSize_;
        RankId remoteRank = tempVTopo_[0][nextRank];

        HCCL_DEBUG("[InsTempReduceScatterMesh1D][RunReduceScatter] myRank[%d], toRank[%d], fromRank[%d]",
            myRank_,
            remoteRank,
            remoteRank);
        const std::vector<LinkData> &linkRecv = tempLinks.at(remoteRank);
        const std::vector<LinkData> &linkSend = tempLinks.at(remoteRank);
        std::vector<DataSlice> txSrcSlices;
        std::vector<DataSlice> txDstSlices;
        std::vector<DataSlice> rxSrcSlices;
        std::vector<DataSlice> rxDstSlices;

        // 在 inBuff 上进行 ReduceScatter 操作
        // 数据从其他卡，传输到本卡，接收数据
        for (u32 repeatIdx = 0; repeatIdx < tempAlgParams.repeatNum; repeatIdx++) {
            DataSlice rxSrcSlice = DataSlice(tempAlgParams.buffInfo.inBuffType,
                tempAlgParams.buffInfo.inBuffBaseOff + repeatIdx * tempAlgParams.inputRepeatStride +
                    myAlgRank * tempAlgParams.inputSliceStride + linkSliceOffsetVec_[linkIdx],
                processSize_[linkIdx]);  // 接收源
            DataSlice rxDstSlice = DataSlice(tempAlgParams.buffInfo.scratBuffType,
                tempAlgParams.buffInfo.scratchBuffBaseOff + repeatIdx * tempAlgParams.outputRepeatStride +
                    nextRank * tempAlgParams.outputSliceStride + linkSliceOffsetVec_[linkIdx],
                processSize_[linkIdx]);  // 接收目标
            DataSlice txSrcSlice = DataSlice(tempAlgParams.buffInfo.inBuffType,
                tempAlgParams.buffInfo.inBuffBaseOff + repeatIdx * tempAlgParams.inputRepeatStride +
                    nextRank * tempAlgParams.inputSliceStride + linkSliceOffsetVec_[linkIdx],
                processSize_[linkIdx]);  // 发送源
            DataSlice txDstSlice = DataSlice(tempAlgParams.buffInfo.scratBuffType,
                tempAlgParams.buffInfo.scratchBuffBaseOff + repeatIdx * tempAlgParams.outputRepeatStride +
                    myAlgRank * tempAlgParams.outputSliceStride + linkSliceOffsetVec_[linkIdx],
                processSize_[linkIdx]);  // 发送目标

            rxSrcSlices.push_back(rxSrcSlice);
            rxDstSlices.push_back(rxDstSlice);
            txSrcSlices.push_back(txSrcSlice);
            txDstSlices.push_back(txDstSlice);
        }
        SendRecvInfo sendRecvInfo{{linkSend[linkIdx], linkRecv[linkIdx]}, {{txSrcSlices, txDstSlices}, {rxSrcSlices, rxDstSlices}}};

        CHK_PRT_RET(SendRecv(sendRecvInfo, tempInsQues[queIdx], 0, true, DmaMode::PUT),
            HCCL_ERROR("[InsTempReduceScatterMesh1D] RunReduceScatter SendReduce failed"),
            HcclResult::HCCL_E_INTERNAL);
    }
    return HcclResult::HCCL_SUCCESS;
}

RankId InsTempReduceScatterMesh1D::GetRankFromMap(const u32 rankIdx)
{
    RankId rank = -1;
    HCCL_INFO("[InsTempReduceScatterMesh1D] GetRankFromMap");
    for (auto &pair : tempVirtRankMap_) {
        if (pair.second == rankIdx) {
            rank = pair.first;
            break;
        }
    }
    return rank;
}

}  // namespace Hccl

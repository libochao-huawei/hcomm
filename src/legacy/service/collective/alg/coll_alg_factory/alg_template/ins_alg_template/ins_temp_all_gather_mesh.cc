/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "log.h"

#include "alg_data_trans_wrapper.h"
#include "ins_temp_all_gather_mesh.h"

namespace Hccl {
InsTempAllGatherMesh1D::InsTempAllGatherMesh1D(const RankId virtualRank, const u32 tempRankSize,
                                           const std::vector<std::vector<RankId>> &tempVTopo,
                                           const std::map<RankId, u32>            &tempVirtRankMap)
    : InsAlgTemplateBase(virtualRank, tempRankSize, tempVTopo, tempVirtRankMap)
{
}

InsTempAllGatherMesh1D::~InsTempAllGatherMesh1D()
{
}

HcclResult InsTempAllGatherMesh1D::CalcRes(AlgTempResReq &tempResReq)
{
    HCCL_DEBUG("[InsTempAllGatherMesh1D] Enter CalcRes");
    tempResReq.queNum = tempVTopo_[0].size() - 1;
    tempResReq.streamNum = tempResReq.queNum;
    tempResReq.queNotifys = CreateMasterSlaveQueNotifiesRequest(tempResReq.queNum);
    HCCL_DEBUG("[InsTempAllGatherMesh1D] CalcRes queNotifys size[%zu]", tempResReq.queNotifys.size());

    QId centerQ = 0;
    tempResReq.localWaitGroupCntNotify.emplace_back(centerQ, 0);
    tempResReq.localBcastPostCntNotify.emplace_back(centerQ, 0);

    CHK_RET(CalcResLinksMesh(myRank_, tempRankSize_, tempVTopo_, linkNumBtwPeers_, tempResReq));
    HCCL_DEBUG("[InsTempAllGatherMesh1D] CalcRes done");
    return HcclResult::HCCL_SUCCESS;
}

HcclResult InsTempAllGatherMesh1D::CalcSliceInfo(const AllignInfo &allignInfo, const u64 dataSize,
                                               RankSliceInfo &sliceInfoVec)
{
    std::vector<SliceInfo> tmp(1);
    sliceInfoVec.resize(tempRankSize_, tmp);

    CHK_RET(CalcRsAgSliceInfoMesh(myRank_, tempRankSize_, allignInfo, dataSize, sliceInfoVec));

    return HcclResult::HCCL_SUCCESS;
}

HcclResult InsTempAllGatherMesh1D::GenExtIns(const TempFuncs &tempFuncs, const TemplateDataParams &tempAlgParams,
                                           const ResLinks &tempLinks,
                                           std::vector<InsQuePtr> &tempInsQues)
{
    HCCL_INFO("[InsTempAllGatherMesh1D] RunAllGather start");

    opMode_ = tempFuncs.opMode;
    tempAlgParams_ = tempAlgParams;
    tempLinks_ = tempLinks;

    majorQueNum_ = tempVTopo_[0].size() - 1;
    CHK_PRT_RET(majorQueNum_ * queNumPerNeighbor_ != tempInsQues.size(),
        HCCL_ERROR("[InsTempAllGatherMesh1D] RunAllGather Rank [%d], requiredQueNum [%u] not equals to "
                   "templateQueNum [%u].",
                   myRank_, majorQueNum_ * queNumPerNeighbor_, tempInsQues.size()),
        HcclResult::HCCL_E_INTERNAL);

    // queue arrangement
    std::vector<InsQuePtr> mainInsQues;
    for (u32 queIdx = 0; queIdx < majorQueNum_; queIdx++) {
        mainInsQues.push_back(tempInsQues[queIdx * queNumPerNeighbor_]);
    }

    // Local Copy from Input to Output for OPBASE
    CHK_RET(LocalDataCopy(mainInsQues));
    if (tempRankSize_ == 1) {
        return HcclResult::HCCL_SUCCESS;
    }

    // semaphore sync
    if (majorQueNum_ > 1) { // more than one rank
        CHK_RET(PreSyncInterQueues(mainInsQues));
    }

    // locate myRank in tempVTopo -> algRank
    u32 myAlgRank;
    CHK_RET(GetAlgRank(myRank_, tempVTopo_[0], myAlgRank));

    // run Mesh
    CHK_PRT_RET(
        RunMesh(myAlgRank, tempVTopo_[0], tempInsQues) != HcclResult::HCCL_SUCCESS,
        HCCL_ERROR("[InsTempAllGatherMesh1D] Rank [%d], unable to run mesh algorithm.", myRank_),
        HcclResult::HCCL_E_INTERNAL);

    // semaphore sync
    if (majorQueNum_ > 1) { // more than one rank
        CHK_RET(PostSyncInterQueues(mainInsQues));
    }

    // LocalCopy: from scratch to output for opbase
    if ((opMode_ == OpMode::OPBASE) && tempFuncs.isBottom) {
        CHK_RET(PostLocalCopy(mainInsQues));
    }

    return HcclResult::HCCL_SUCCESS;
}

HcclResult InsTempAllGatherMesh1D::LocalDataCopy(std::vector<InsQuePtr> &tempInsQues)
{
    if (tempAlgParams_.buffInfo.inBuffType == tempAlgParams_.buffInfo.outBuffType) {
        return HcclResult::HCCL_SUCCESS;
    }
    u32 myAlgRank;
    CHK_RET(GetAlgRank(myRank_, tempVTopo_[0], myAlgRank));
    for (u32 rpt = 0; rpt < tempAlgParams_.repeatNum; ++rpt) {
        const u64 inBaseOff  = tempAlgParams_.buffInfo.inBuffBaseOff  + rpt * tempAlgParams_.inputRepeatStride;
        const u64 outBaseOff = tempAlgParams_.buffInfo.outBuffBaseOff + rpt * tempAlgParams_.outputRepeatStride;

        const u64 inOff = tempAlgParams_.inputSliceStride * myAlgRank + inBaseOff;
        const u64 outOff = tempAlgParams_.outputSliceStride * myAlgRank + outBaseOff;

        DataSlice src(tempAlgParams_.buffInfo.inBuffType, inOff, tempAlgParams_.sliceSize);
        DataSlice dst(tempAlgParams_.buffInfo.outBuffType, outOff, tempAlgParams_.sliceSize);
        HCCL_INFO("[InsTempAllGatherMesh1D] in:%s -> out:%s", src.Describe().c_str(), dst.Describe().c_str());

        auto ins = std::make_unique<InsLocalCopy>(src, dst);
        tempInsQues[0]->Append(std::move(ins));
    }
    return HcclResult::HCCL_SUCCESS;
}

HcclResult InsTempAllGatherMesh1D::PostLocalCopy(std::vector<InsQuePtr> &tempInsQues)
{
    for (u32 rpt = 0; rpt < tempAlgParams_.repeatNum; ++rpt) {
        const u64 outBaseOff = tempAlgParams_.buffInfo.outBuffBaseOff + rpt * tempAlgParams_.outputRepeatStride;
        const u64 scratchRepeatStride = tempAlgParams_.sliceSize * tempRankSize_;
        const u64 scratchBase = tempAlgParams_.buffInfo.scratchBuffBaseOff + rpt * scratchRepeatStride;

        for (auto rank : tempVTopo_[0]) {
            if (rank == myRank_) {
                continue;
            }
            u32 algRank = 0;
            CHK_RET(GetAlgRank(rank, tempVTopo_[0], algRank));
            // 只拷贝step1的对端
            u64 scratchOffset = scratchBase + tempAlgParams_.sliceSize * algRank;
            u64 outOffset = tempAlgParams_.outputSliceStride * algRank + outBaseOff;

            DataSlice usrInSlice = DataSlice(tempAlgParams_.buffInfo.scratBuffType, scratchOffset,
                tempAlgParams_.sliceSize);
            DataSlice usrOutSlice = DataSlice(tempAlgParams_.buffInfo.outBuffType, outOffset,
                tempAlgParams_.sliceSize);

            std::unique_ptr<Instruction> insLocalCopy = std::make_unique<InsLocalCopy>(usrInSlice, usrOutSlice);
            tempInsQues[0]->Append(std::move(insLocalCopy));
        }
    }
    return HcclResult::HCCL_SUCCESS;
}

HcclResult InsTempAllGatherMesh1D::RunMesh(const u32 myAlgRank, const std::vector<RankId> &vTopo,
                                           std::vector<InsQuePtr> &tempInsQues)
{
    for (u32 rpt = 0; rpt < tempAlgParams_.repeatNum; ++rpt) {
        const u64 outBaseOff = tempAlgParams_.buffInfo.outBuffBaseOff + rpt * tempAlgParams_.outputRepeatStride;
        const u64 scratchRepeatStride = tempAlgParams_.sliceSize * tempRankSize_;
        const u64 scratchBase = tempAlgParams_.buffInfo.scratchBuffBaseOff + rpt * scratchRepeatStride;

        for (u32 queIdx = 0; queIdx < vTopo.size() - 1; queIdx++) {
            RankId connectedRank = vTopo[(myAlgRank + 1 + queIdx) % vTopo.size()];
            
            u32 connectedAlgRank = 0;
            CHK_RET(GetAlgRank(connectedRank, tempVTopo_[0], connectedAlgRank));

            CHK_PRT_RET(queIdx >= tempInsQues.size() || tempLinks_.at(connectedRank).empty(),
                HCCL_ERROR("[InsTempAllGatherMesh1D] queIdx=%u, tempInsQues.size=%u, connectedRank=%d, tempLinks_.size=%u",
                           queIdx, tempInsQues.size(), connectedRank, tempLinks_.size()),
                HcclResult::HCCL_E_INTERNAL);

            InsQuePtr currQue = tempInsQues[queIdx];
            LinkData &neighborLinkData = tempLinks_.at(connectedRank)[0];

            BufferType writeType = (opMode_ == OpMode::OPBASE) ?
                tempAlgParams_.buffInfo.scratBuffType : tempAlgParams_.buffInfo.outBuffType;

            u64 txOutOffset = tempAlgParams_.outputSliceStride * myAlgRank + outBaseOff;
            u64 txScratchOffset = scratchBase + tempAlgParams_.sliceSize * myAlgRank;
            u64 txDstOffset = (opMode_ == OpMode::OPBASE) ? txScratchOffset : txOutOffset;

            u64 rxOutOffset = tempAlgParams_.outputSliceStride * connectedAlgRank + outBaseOff;
            u64 rxScratchOffset = scratchBase + tempAlgParams_.sliceSize * connectedAlgRank;
            u64 rxSrcOffset = (opMode_ == OpMode::OPBASE) ? rxScratchOffset : rxOutOffset;

            vector<DataSlice> txSrcSlices{ DataSlice(tempAlgParams_.buffInfo.outBuffType, txOutOffset,
                                                     tempAlgParams_.sliceSize) };
            vector<DataSlice> txDstSlices{ DataSlice(writeType, txDstOffset, tempAlgParams_.sliceSize) };
            vector<DataSlice> rxSrcSlices{ DataSlice(writeType, rxSrcOffset, tempAlgParams_.sliceSize) };
            vector<DataSlice> rxDstSlices{ DataSlice(tempAlgParams_.buffInfo.outBuffType, rxOutOffset,
                                                     tempAlgParams_.sliceSize) };

            TxRxSlicesList sendRecvSlicesList({txSrcSlices, txDstSlices}, {rxSrcSlices, rxDstSlices});
            TxRxLinks sendRecvLinks(neighborLinkData, neighborLinkData);
            SendRecvInfo sendRecvInfo(sendRecvLinks, sendRecvSlicesList);

            CHK_PRT_RET(SendRecv(sendRecvInfo, currQue, 0, true, DmaMode::PUT),
                HCCL_ERROR("[InsTempAllGatherMesh1D] sendrecv failed (nbr=%d, queIdx=%u, rpt=%u)", connectedRank,
                queIdx, rpt), HcclResult::HCCL_E_INTERNAL);
        }
    }
    return HcclResult::HCCL_SUCCESS;
}

} // namespace Hccl

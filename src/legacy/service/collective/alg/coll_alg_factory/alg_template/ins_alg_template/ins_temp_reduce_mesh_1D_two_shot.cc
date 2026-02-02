/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: 算法模板InsTempReduceMesh1DTwoShot类实现
 * Author: l00929943
 * Create: 2025-06-05
 */

#include "ins_temp_reduce_mesh_1D_two_shot.h"

#include "aicpu_ins.h"
#include "log.h"
#include "alg_data_trans_wrapper.h"

namespace Hccl {

InsTempReduceMesh1DTwoShot::InsTempReduceMesh1DTwoShot(const RankId virtualRank, const u32 tempRankSize,
    const std::vector<std::vector<RankId>> &tempVTopo, const std::map<RankId, u32> &tempVirtRankMap)
    : InsAlgTemplateBase(virtualRank, tempRankSize, tempVTopo, tempVirtRankMap)
{
    HCCL_INFO("[InsTempReduceMesh1DTwoShot] Init.");
}

InsTempReduceMesh1DTwoShot::~InsTempReduceMesh1DTwoShot()
{
    HCCL_INFO("[InsTempReduceMesh1DTwoShot] exit.");
}

HcclResult InsTempReduceMesh1DTwoShot::CalcRes(AlgTempResReq &tempResReq)
{
    tempResReq.queNum = tempRankSize_;
    tempResReq.streamNum = tempResReq.queNum;
    tempResReq.queNotifys = CreateMasterSlaveQueNotifiesRequest(tempResReq.queNum);

    QId centerQ = 0;
    tempResReq.localWaitGroupCntNotify.emplace_back(centerQ, 0);
    tempResReq.localBcastPostCntNotify.emplace_back(centerQ, 0);

    CHK_PRT_RET(CalcResLinksMesh(myRank_, tempRankSize_, tempVTopo_, linkNumBtwPeers_, tempResReq) != HcclResult::HCCL_SUCCESS,
        HCCL_ERROR("[InsTempReduceMesh1DTwoShot] Rank [%d], resLinks calculation error!", myRank_),
        HcclResult::HCCL_E_INTERNAL);

    return HcclResult::HCCL_SUCCESS;
}

u32 InsTempReduceMesh1DTwoShot::CalcScratchMultiple(BufferType inBuffType, BufferType outBuffType) const
{
    (void)input;
    (void)output;
    return tempRankSize_;
}

HcclResult InsTempReduceMesh1DTwoShot::CalcSlice(const u64 dataSize, RankSliceInfo &sliceInfoVec)
{
    std::vector<SliceInfo> tmp(tempVTopo_.size());
    sliceInfoVec.resize(tempRankSize_, tmp);

    u32 unitAllignSize = DataTypeSizeGet(dataType_);

    u64 elementsPerRank = (dataSize / unitAllignSize) / tempRankSize_;
    u64 chunkSize = elementsPerRank * unitAllignSize;
    
    u64 accumOff = 0;
    for (u32 rankIdx = 0; rankIdx < tempRankSize_; rankIdx++) {
        u64 currChunkSize;
        if (rankIdx == tempRankSize_ - 1) {
            currChunkSize = dataSize - accumOff;
        } else {
            currChunkSize = chunkSize;
        }
        
        sliceInfoVec[rankIdx][0] = {accumOff, currChunkSize};
        accumOff += currChunkSize;
    }

    return HcclResult::HCCL_SUCCESS;
}

HcclResult InsTempReduceMesh1DTwoShot::GenExtIns(const TempFuncs &tempFuncs, const TemplateDataParams &tempAlgParams,
    const ResLinks &tempLinks, std::vector<InsQuePtr> &tempInsQues)
{
    if (tempAlgParams.sliceSize == 0) {
        return HcclResult::HCCL_SUCCESS;
    }

    opMode_ = tempFuncs.opMode;
    enableCounterNotify_ = tempFuncs.enableCounterNotify;
    myIdx_ = tempVirtRankMap_.at(myRank_);

    RankSliceInfo sliceInfoVec;
    CHK_RET(CalcSlice(tempAlgParams.sliceSize, sliceInfoVec));

    CHK_RET(RunReduceScatter(sliceInfoVec, tempLinks, tempInsQues, tempAlgParams));

    CHK_RET(RunGatherToRoot(sliceInfoVec, tempLinks, tempInsQues, tempAlgParams));

    return HcclResult::HCCL_SUCCESS;
}

HcclResult InsTempReduceMesh1DTwoShot::RunReduceScatter(const RankSliceInfo &sliceInfoVec, const ResLinks &tempLinks,
    std::vector<InsQuePtr> &tempInsQues, const TemplateDataParams &tempAlgParams)
{
    u64 inOff = tempAlgParams.buffInfo.inBuffBaseOff;
    u64 scOff = tempAlgParams.buffInfo.scratchBuffBaseOff;

    PreSyncInterQueues(tempInsQues);

    for (u32 rankId = 0; rankId < tempRankSize_; rankId++) {
        DataSlice ssrc(tempAlgParams.buffInfo.inBuffType, sliceInfoVec[rankId][0].offset + inOff, sliceInfoVec[rankId][0].size);
        DataSlice sdest(tempAlgParams.buffInfo.scratBuffType, myIdx_ * sliceInfoVec[rankId][0].size + scOff, sliceInfoVec[rankId][0].size);

        if (rankId == myIdx_) {
            if (sliceInfoVec[rankId][0].size != 0) {
                CHK_RET(LocalCopy(tempInsQues[rankId], ssrc, sdest));
            }
        } else {
            DataSlice rsrc(tempAlgParams.buffInfo.inBuffType, sliceInfoVec[myIdx_][0].offset + inOff, sliceInfoVec[myIdx_][0].size);
            DataSlice rdest(tempAlgParams.buffInfo.scratBuffType, rankId * sliceInfoVec[myIdx_][0].size + scOff, sliceInfoVec[myIdx_][0].size);

            const auto &link = tempLinks.at(GetRankFromMap(rankId))[0];
            TxRxLinks links(link, link);
            SlicesList sendSList({ssrc}, {sdest});
            SlicesList recvSList({rsrc}, {rdest});
            TxRxSlicesList txRxSList(sendSList, recvSList);

            CHK_RET(SendRecv(SendRecvInfo(links, txRxSList), tempInsQues[rankId], 0, true, DmaMode::PUT));
        }
    }

    PostSyncInterQueues(tempInsQues);

    if (sliceInfoVec[myIdx_][0].size != 0) {
        DataSlice finalDest(tempAlgParams.buffInfo.scratBuffType, scOff, sliceInfoVec[myIdx_][0].size);
        for (u32 i = 1; i < tempRankSize_; i++) {
            DataSlice currentSrc(tempAlgParams.buffInfo.scratBuffType, i * sliceInfoVec[myIdx_][0].size + scOff, sliceInfoVec[myIdx_][0].size);
            CHK_RET(LocalReduce(tempInsQues[0], currentSrc, finalDest, dataType_, redOp_));
        }
    }

    return HcclResult::HCCL_SUCCESS;
}

HcclResult InsTempReduceMesh1DTwoShot::RunGatherToRoot(const RankSliceInfo &sliceInfoVec, const ResLinks &tempLinks,
    std::vector<InsQuePtr> &tempInsQues, const TemplateDataParams &tempAlgParams)
{
    u64 scOff = tempAlgParams.buffInfo.scratchBuffBaseOff;
    u64 outOff = tempAlgParams.buffInfo.outBuffBaseOff;

    PreSyncInterQueues(tempInsQues);

    if (u32(myRank_) == root_) {
        for (u32 rankIdx = 0; rankIdx < tempRankSize_; rankIdx++) {
            u64 curSize = sliceInfoVec[rankIdx][0].size;
            if (curSize == 0) continue;

            if (rankIdx == myIdx_) {
                DataSlice src(tempAlgParams.buffInfo.scratBuffType, scOff, curSize);
                DataSlice dst(tempAlgParams.buffInfo.outBuffType, sliceInfoVec[rankIdx][0].offset + outOff, curSize);
                CHK_RET(LocalCopy(tempInsQues[rankIdx], src, dst));
            } else {
                DataSlice rsrc(tempAlgParams.buffInfo.scratBuffType, scOff, curSize);
                DataSlice rdest(tempAlgParams.buffInfo.outBuffType, sliceInfoVec[rankIdx][0].offset + outOff, curSize);

                const auto &link = tempLinks.at(GetRankFromMap(rankIdx))[0];
                SlicesList sliceList({rsrc}, {rdest});
                
                CHK_RET(Recv(DataInfo(link, sliceList), tempInsQues[rankIdx], 1, true, DmaMode::GET));
            }
        }
    } else {
        u32 rankIdx = myIdx_;
        u64 curSize = sliceInfoVec[rankIdx][0].size;
        
        if (curSize != 0) {
            DataSlice ssrc(tempAlgParams.buffInfo.scratBuffType, scOff, curSize);
            DataSlice sdest(tempAlgParams.buffInfo.outBuffType, sliceInfoVec[rankIdx][0].offset + outOff, curSize);

            const auto &link = tempLinks.at(root_)[0];
            SlicesList sliceList({ssrc}, {sdest});

            CHK_RET(Send(DataInfo(link, sliceList), tempInsQues[0], 1, true, DmaMode::GET));
        }
    }

    PostSyncInterQueues(tempInsQues);
    return HcclResult::HCCL_SUCCESS;
}

RankId InsTempReduceMesh1DTwoShot::GetRankFromMap(const u32 rankIdx)
{
    RankId rank = -1;
    for (auto &pair : tempVirtRankMap_) {
        if (pair.second == rankIdx) {
            rank = pair.first;
            break;
        }
    }
    return rank;
}
}  // namespace Hccl
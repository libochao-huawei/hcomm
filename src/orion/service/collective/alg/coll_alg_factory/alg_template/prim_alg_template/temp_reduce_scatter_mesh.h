/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: 算法模板TempReduceScatterMesh类头文件
 * Author: shenyutian
 * Create: 2024-05-14
 */

#ifndef HCCLV2_TEMP_REDUCE_SCATTER_MESH
#define HCCLV2_TEMP_REDUCE_SCATTER_MESH

#include "string_util.h"

#include "alg_template_base_v2.h"

namespace Hccl {

class TempReduceScatterMesh : public AlgTemplateBase {
public:
    explicit TempReduceScatterMesh(const RankId virtualRank, const u32 tempRankSize,
                                   const std::vector<std::vector<RankId>> &tempVTopo,
                                   const std::map<RankId, u32>            &tempVirtRankMap);
    ~TempReduceScatterMesh() override;

    std::string Describe() const override
    {
        return StringFormat("Template of reduce scatter mesh with tempRankSize [%u].", tempRankSize_);
    }

    HcclResult GenPrimQue(const TempFuncs &tempFuncs, const RankSliceInfo &sliceInfoVec, const BuffInfo &buffInfo,
                          const ResLinks &tempLinks, std::vector<PrimQuePtr> &tempPrimQues) override;
    using AlgTemplateBase::CalcSliceInfo;
    HcclResult CalcSliceInfo(const AllignInfo &allignInfo, const bool forAllReduce, const u64 dataSize,
                             RankSliceInfo &sliceInfoVec) override;
    HcclResult CalcSliceInfoAllReduce(const AllignInfo &allignInfo, const u64 dataSize, RankSliceInfo &sliceInfoVec) const;
    using AlgTemplateBase::CalcRes;
    HcclResult CalcRes(const bool forAllReduce, AlgTempResReq &tempResReq, u32 &requiredScratchMultiplier) override;
    using AlgTemplateBase::CalcResDetour;
    HcclResult CalcResDetour(const bool forAllReduce, const RankGraph *rankGraph, AlgTempResReq &tempResReq,
                             u32 &requiredScratchMultiplier) override;
    HcclResult CalcResDetour(const bool forAllReduce, ConnectedLinkMgr *linkMgr, AlgTempResReq &tempResReq,
                             u32 &requiredScratchMultiplier) override;

private:
    HcclResult CalcResLinks(AlgTempResReq &tempResReq);

    HcclResult RunMesh(const u32 myAlgRank, const std::vector<RankId> &vTopo, const RankSliceInfo &sliceInfoVec,
                       const ResLinks &tempLinks, std::vector<PrimQuePtr> &tempPrimQues);

    HcclResult RunIndividualPeerDetour(const RankId neighborRank, const SliceInfo &sendReduceSlice,
                                       const SliceInfo &recvReduceSlice, const ResLinks &tempLinks,
                                       std::vector<PrimQuePtr> &detourPrimQues);
    HcclResult RunIndividualPeer(const RankId neighborRank, const LinkData &neighborLinkData,
                                 const SliceInfo &sendReduceSlice, const SliceInfo &recvReduceSlice,
                                 PrimQuePtr currQue) const;
    HcclResult GetSendRecvRedLinks(const RankId neighborRank, const ResLinks &tempLinks,
                                   std::vector<std::vector<LinkDataIterator>> &sendRecvLinks) const;

    std::unique_ptr<PrimGroup> RunSendRecvReduce(const RankId neighborRank, const LinkData &sendLinkData,
                                                 const LinkData &recvLinkData, const SliceInfo &currSendReduceSlice,
                                                 const SliceInfo &currRecvReduceSlice) const;

    HcclResult PostCopyOffload(const RankSliceInfo &sliceInfoVec, std::vector<PrimQuePtr> &tempPrimQues);

    u32 majorQueNum_       = 0;
    u32 queNumPerNeighbor_ = 1;
};

} // namespace Hccl

#endif // !HCCLV2_TEMP_REDUCE_SCATTER_MESH
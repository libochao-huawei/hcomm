/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef HCCLV2_INS_ALL_REDUCE_PARALLEL_EXECUTOR_OPT_H
#define HCCLV2_INS_ALL_REDUCE_PARALLEL_EXECUTOR_OPT_H
#include "ins_coll_alg_base.h"

namespace Hccl {


template <typename AlgTopoMatch, typename InsAlgTemplate0, typename InsAlgTemplate1, typename InsAlgTemplate2, typename InsAlgTemplate3>
class InsAllReduceParallelExecutorV2 : public InsCollAlgBase {
public:
    explicit InsAllReduceParallelExecutorV2();
    ~InsAllReduceParallelExecutorV2() override;

    std::string Describe() const override
    {
        return "Instruction based All Reduce Parallel Executor.";
    }

    // HOST 接口
    HcclResult Orchestrate(const RankGraph *rankGraph, const CollAlgOperator &op, const CollAlgParams &params,
                          InsQuePtr insQue) override;
    // AICPU 接口
    HcclResult Orchestrate(const AlgTopoInfo &topoInfo, const CollAlgOperator &op, const CollAlgParams &params,
                             ConnectedLinkMgr *linkMgr, InsQuePtr insQue) override;

    HcclResult CalcResOffload(const RankGraph *rankGraph, const u64 &dataSize,
                              CollOffloadOpResReq &resReq) override;

    HcclResult CalcRes(const RankGraph *rankGraph, CollAlgResReq &algResReq) override;

private:
    HcclResult CalcLocalRankSize();
    HcclResult GenInsQues(InsAlgTemplate0 &tempAlgIntraRS, InsAlgTemplate1 &tempAlgInterRS, InsAlgTemplate2 &tempAlgIntraAG, InsAlgTemplate3 &tempAlgInterAG);
    void GenTemplateAlgParams0(const u64 dataOffset, const u64 dataCount, const u64 scratchOffset,TemplateDataParams &tempAlgParams) const;
    void GenTemplateAlgParams1(const u64 dataOffset, const u64 dataCount, const u64 scratchOffset,TemplateDataParams &tempAlgParams) const;
    void GenTemplateAlgParams2(const u64 dataOffset, const u64 dataCount, const u64 scratchOffset,TemplateDataParams &tempAlgParams) const;
    HcclResult CalcSendDataSize(u64 &memBlockSize, float &SplitRate, u32 &multipleIntraRS, u32 &multipleInterRS, u32 &multipleIntraAG, u32 &multipleInterAG);
    
    void GetParallelDataSplitRate(std::vector<float> &splitDataSize) const;
    HcclResult PrepareResForTemplate(const RankGraph *rankGraph, InsAlgTemplate0 &tempAlgIntraRS, InsAlgTemplate1 &tempAlgInterRS, InsAlgTemplate2 &tempAlgIntraAG, InsAlgTemplate3 &tempAlgInterAG);
    HcclResult PrepareResForTemplate(ConnectedLinkMgr *linkMgr, InsAlgTemplate0 &tempAlgIntraRS, InsAlgTemplate1 &tempAlgInterRS, InsAlgTemplate2 &tempAlgIntraAG, InsAlgTemplate3 &tempAlgInterAG);

    inline void SetCommonTemplateAlgParams(BufferType inType, BufferType outType, u64 dataOffset, u64 dataCount, u64 scratchOffset, TemplateDataParams& tempAlgParams) const
    {
        auto& bi = tempAlgParams.buffInfo;
        bi.inBuffType = inType;
        bi.outBuffType = outType;
        bi.scratBuffType = BufferType::SCRATCH;
        bi.inBuffBaseOff = bi.outBuffBaseOff = dataOffset;
        bi.scratchBuffBaseOff = scratchOffset;
        
        const u64 totalSize = dataCount * dataTypeSize_;
        tempAlgParams.sliceSize = totalSize;
        tempAlgParams.tailSize = totalSize;
        // 单 slice 场景：stride 固定为 0
        tempAlgParams.inputSliceStride = tempAlgParams.outputSliceStride = 0;
    }

    template <typename InsAlgTemplate0, typename InsAlgTemplate1, typename InsAlgTemplate2, typename InsAlgTemplate3>
    inline void InitAlgCommonParams(
        InsAlgTemplate0& tempAlgIntraRS,
        InsAlgTemplate1& tempAlgInterRS,
        InsAlgTemplate2& tempAlgIntraAG,
        InsAlgTemplate3& tempAlgInterAG,
        const CollAlgOperator& op) const
    {
        tempAlgIntraRS.SetDmaMode(dmaMode_);
        tempAlgIntraRS.InitReduceInfo(redOp_, dataType_);
        tempAlgIntraRS.SetCollOp(op);

        tempAlgInterRS.SetDmaMode(dmaMode_);
        tempAlgInterRS.InitReduceInfo(redOp_, dataType_);
        tempAlgInterRS.SetCollOp(op);

        tempAlgIntraAG.SetDmaMode(dmaMode_);
        tempAlgIntraAG.InitReduceInfo(redOp_, dataType_);
        tempAlgIntraAG.SetCollOp(op);

        tempAlgInterAG.SetDmaMode(dmaMode_);
        tempAlgInterAG.InitReduceInfo(redOp_, dataType_);
        tempAlgInterAG.SetCollOp(op);
    }

    inline void CalcQue(AlgTempResReq &resReqIntraRS, AlgTempResReq &resReqInterRS,
                        AlgTempResReq &resReqIntraAG, AlgTempResReq &resReqInterAG) const
    {
        // 申请算法模板所需资源
        if(!(resReqIntraRS.queNum > 0 && resReqInterRS.queNum > 0 && resReqIntraAG.queNum > 0 && resReqInterAG.queNum > 0)) {
            HCCL_ERROR("resReqIntra.queNum and resReqInter.queNum must larger than 0.");
            return HcclResult::HCCL_E_INTERNAL;
        }
        u32 totalQueueNum = std::max(resReqIntraRS.queNum + resReqInterRS.queNum, resReqIntraAG.queNum + resReqInterAG.queNum);
        CHK_RET(InitQueue(totalQueueNum, requiredQue_));
        for(u32 i = 0 ; i < requiredQue_.size(); i++) {
            unsigned int neededIntraQueNum = ((resReqIntraRS.queNum + resReqInterRS.queNum) > (resReqIntraAG.queNum + resReqInterAG.queNum)) ?
            resReqIntraRS.queNum : resReqIntraAG.queNum;
            if (i < neededIntraQueNum) {
                intraQue_.push_back(requiredQue_[i]);
            } else {
                interQue_.push_back(requiredQue_[i]);
            }
        }
        syncQueues_.emplace_back(intraQue_[0]);
        syncQueues_.emplace_back(interQue_[0]);
    }

    uint64_t rankSizeLevel0_{0};
    uint64_t rankSizeLevel1_{0};

    std::vector<std::vector<RankId>>              virtRanks_;
    std::vector<std::map<RankId, u32>>            virtRankMap_; // map<virtRank, virtRankOrder>
    std::vector<std::vector<std::vector<RankId>>> vTopo_;

    std::vector<InsQuePtr> requiredQue_;
    std::vector<InsQuePtr> intraQue_;
    std::vector<InsQuePtr> interQue_;
    std::vector<InsQuePtr> syncQueues_;

    ResLinks               intraRSLinks_;
    ResLinks               interRSLinks_;
    ResLinks               intraAGLinks_;
    ResLinks               interAGLinks_;

    u64 interScratchOffset0{0};
    u64 interScratchOffset1{0};
    u64 interScratchOffset2{0};
    u64 interScratchOffset3{0};

    u64 Intra0ScratchSize{0};
    u64 Intra1ScratchSize{0};
    u64 Intra2ScratchSize{0};
    u64 Intra3ScratchSize{0};

    u64 Inter0ScratchSize{0};
    u64 Inter1ScratchSize{0};
    u64 Inter2ScratchSize{0};
    u64 Inter3ScratchSize{0};

    float dataSplitRate{0.5};
};

} // namespace Hccl

#endif // HCCLV2_INS_ALL_REDUCE_PARALLEL_EXECUTOR_H

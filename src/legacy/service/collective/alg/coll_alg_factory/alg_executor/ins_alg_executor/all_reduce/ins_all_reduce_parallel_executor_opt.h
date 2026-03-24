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
    void GetParallelDataSplitRate(std::vector<float> &splitDataSize) const;
    HcclResult PrepareResForTemplate(const RankGraph *rankGraph, InsAlgTemplate0 &tempAlgIntraRS, InsAlgTemplate1 &tempAlgInterRS, InsAlgTemplate2 &tempAlgIntraAG, InsAlgTemplate3 &tempAlgInterAG);
    HcclResult PrepareResForTemplate(ConnectedLinkMgr *linkMgr, InsAlgTemplate0 &tempAlgIntraRS, InsAlgTemplate1 &tempAlgInterRS, InsAlgTemplate2 &tempAlgIntraAG, InsAlgTemplate3 &tempAlgInterAG);

    // ===================== RS 阶段的参数生成 =====================
    // Phase 1: 前半数据做框内RS，后半数据做框间RS
    // RS Intra0: 框内 mesh ReduceScatter (前半数据)
    //   INPUT -> SCRATCH, 在共享 INPUT 上读取 M 个intra-rank 的前半数据，reduce后写入 SCRATCH
    //   repeatNum = N (框间rank数)，对每个框间group重复一次
    void GenRSIntraParams0(const u64 dataOffset, const u64 dataCount,
                           const std::vector<u64> &scratchOffVec,
                           TemplateDataParams &params) const;
    // RS Inter1: 框间 NHR ReduceScatter (后半数据)
    //   INPUT -> SCRATCH, 在 INPUT 上读取后半数据，NHR reduce 后写入 SCRATCH
    //   repeatNum = M (框内rank数)，对每个框内rank重复一次
    void GenRSInterParams1(const u64 dataOffset, const u64 dataCount,
                           const std::vector<u64> &scratchOffVec,
                           TemplateDataParams &params) const;

    // Phase 2: 前半数据做框间RS(第二阶段)，后半数据做框内RS(第二阶段)
    // RS Inter0: 框间 NHR ReduceScatter (前半数据，第二阶段)
    //   SCRATCH -> OUTPUT, 从 Phase1 Intra RS 的 scratch 结果中读取，NHR reduce 后写入 OUTPUT
    //   repeatNum = 1
    void GenRSInterParams0(const u64 dataOffset, const u64 dataCount,
                           const std::vector<u64> &scratchOffVec,
                           TemplateDataParams &params) const;
    // RS Intra1: 框内 mesh ReduceScatter (后半数据，第二阶段)
    //   SCRATCH -> OUTPUT, 从 Phase1 Inter RS 的 scratch 结果中读取，mesh reduce 后写入 OUTPUT
    //   repeatNum = 1
    void GenRSIntraParams1(const u64 dataOffset, const u64 dataCount,
                           const std::vector<u64> &scratchOffVec,
                           TemplateDataParams &params) const;

    // ===================== AG 阶段的参数生成 =====================
    // Phase 3: 前半数据做框内AG，后半数据做框间AG
    // AG Intra0: 框内 mesh AllGather (前半数据)
    //   OUTPUT -> OUTPUT, 在 OUTPUT 上聚合框内 M 个 rank 的 RS 结果
    //   repeatNum = 1
    void GenAGIntraParams0(const u64 dataOffset, const u64 dataCount,
                           const u64 scratchOffset,
                           TemplateDataParams &params) const;
    // AG Inter1: 框间 NHR AllGather (后半数据)
    //   OUTPUT -> OUTPUT, 通过 NHR 聚合框间 N 个 rank 的 RS 结果
    //   repeatNum = 1
    void GenAGInterParams1(const u64 dataOffset, const u64 dataCount,
                           const u64 scratchOffset,
                           TemplateDataParams &params) const;

    // Phase 4: 前半数据做框间AG(第二阶段)，后半数据做框内AG(第二阶段)
    // AG Inter0: 框间 NHR AllGather (前半数据，第二阶段)
    //   OUTPUT -> OUTPUT, 把 Phase3 Intra AG 结果通过 NHR 在框间分发
    //   repeatNum = M (框内rank数)
    void GenAGInterParams0(const u64 dataOffset, const u64 dataCount,
                           const u64 scratchOffset,
                           TemplateDataParams &params) const;
    // AG Intra1: 框内 mesh AllGather (后半数据，第二阶段)
    //   OUTPUT -> OUTPUT, 把 Phase3 Inter AG 结果在框内分发
    //   repeatNum = N (框间rank数)
    void GenAGIntraParams1(const u64 dataOffset, const u64 dataCount,
                           const u64 scratchOffset,
                           TemplateDataParams &params) const;

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
        tempAlgIntraAG.SetCollOp(op);

        tempAlgInterAG.SetDmaMode(dmaMode_);
        tempAlgInterAG.SetCollOp(op);
    }

    inline HcclResult CalcQue(AlgTempResReq &resReqIntraRS, AlgTempResReq &resReqInterRS,
                              AlgTempResReq &resReqIntraAG, AlgTempResReq &resReqInterAG)
    {
        // 申请算法模板所需资源
        if(!(resReqIntraRS.queNum > 0 && resReqInterRS.queNum > 0 && resReqIntraAG.queNum > 0 && resReqInterAG.queNum > 0)) {
            HCCL_ERROR("resReqIntra.queNum and resReqInter.queNum must larger than 0.");
            return HcclResult::HCCL_E_INTERNAL;
        }
        u32 intraQueNum = std::max(resReqIntraRS.queNum, resReqIntraAG.queNum);
        u32 interQueNum = std::max(resReqInterRS.queNum, resReqInterAG.queNum);
        u32 totalQueNum = intraQueNum + interQueNum;
        CHK_RET(InitQueue(totalQueNum, requiredQue_));
        for(u32 i = 0 ; i < requiredQue_.size(); i++) {
            if (i < intraQueNum) {
                intraQue_.push_back(requiredQue_[i]);
            } else {
                interQue_.push_back(requiredQue_[i]);
            }
        }
        syncQueues_.emplace_back(intraQue_[0]);
        syncQueues_.emplace_back(interQue_[0]);
        return HCCL_SUCCESS;
    }

    uint64_t rankSizeLevel0_{0};
    uint64_t rankSizeLevel1_{0};

    uint64_t rankIdxLevel0_{0};
    uint64_t rankIdxLevel1_{0};

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
};

} // namespace Hccl

#endif // HCCLV2_INS_ALL_REDUCE_PARALLEL_EXECUTOR_H

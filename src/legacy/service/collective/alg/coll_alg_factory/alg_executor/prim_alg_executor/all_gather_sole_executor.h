/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: 算法库AllGatherSoleExecutor类头文件
 * Author: shenyutian
 * Create: 2024-02-04
 */

#ifndef HCCLV2_ALL_GATHER_SOLE_EXECUTOR
#define HCCLV2_ALL_GATHER_SOLE_EXECUTOR

#include <string>
#include <vector>
#include <map>

#include <hccl/hccl_types.h>
#include "hccl/base.h"
#include "types/types.h"
#include "coll_alg_params.h"
#include "connected_link_mgr.h"
#include "template_utils.h"
#include "alg_template_base_v2.h"
#include "hccl_params_pub.h"
#include "coll_alg_base.h"
#include "rank_gph.h"
#include "coll_operator.h"

namespace Hccl {

template <typename AlgTopoMatch, typename AlgTemplate> class AllGatherSoleExecutor : public CollAlgBase {
public:
    explicit AllGatherSoleExecutor();
    ~AllGatherSoleExecutor() override;

    std::string Describe() const override
    {
        return "All Gather Sole Executor.";
    }

    HcclResult GenPrimQues(const RankGraph *rankGraph, const CollAlgOperator &op, const CollAlgParams &params,
                           PrimQuePtr primQue) override;
    HcclResult CalcResOffload(const RankGraph *rankGraph, const u64 &dataSize,
                              CollOffloadOpResReq &resReq) override;
    HcclResult CalcRes(const RankGraph *rankGraph, CollAlgResReq &algResReq) override;

    HcclResult GenPrimQuesAIC(const AlgTopoInfo &topoInfo, const CollAlgOperator &op, const CollAlgParams &params,
                              ConnectedLinkMgr *linkMgr, PrimQuePtr primQue) override;

private:
    HcclResult GenPrimQues4Offload(AlgTemplateBase &tempAlg);
    HcclResult GenPrimQues4Opbase(const u32 dataSizePerVolume, AlgTemplateBase &tempAlg);

    std::vector<RankId>              virtRanks_;
    std::map<RankId, u32>            virtRankMap_; // map<virtRank, virtRankOrder>
    std::vector<std::vector<RankId>> vTopo_;

    std::vector<PrimQuePtr> requiredQue_;
    ResLinks                tempResLinks_;
};

} // namespace Hccl

#endif // HCCLV2_ALL_GATHER_SOLE_EXECUTOR
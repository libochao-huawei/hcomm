/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "all_gather_v_auto_selector.h"
#include "selector_registry.h"
#include "coll_operator.h"
#include "coll_alg_params.h"

namespace Hccl {
SelectorStatus AllGatherVAutoSelector::SelectCcuMsAlgo(const TopoInfo &topoInfo, const CollAlgOperator &op,
    const std::map<OpType, std::vector<HcclAlgoType>> &configAlgMap, std::string &primQueueGenName) const
{
    if (topoInfo.levelNum > 1) {
        HCCL_WARNING("[Algo][AllGatherVAutoSelector] levelNum > 1 is not supported yet for ccu_ms mode.");
        return SelectorStatus::NOT_MATCH;
    }

    HcclAlgoType levle0Algo = HcclAlgoType::HCCL_ALGO_TYPE_DEFAULT;
    auto it = configAlgMap.find(op.opType);
    if ((it != configAlgMap.end()) && (it->second.size() > 0)) {
        levle0Algo = it->second[0];
    }
    if (IsDefaultAlg(levle0Algo) || levle0Algo == HcclAlgoType::HCCL_ALGO_TYPE_FULLMESH) {
        if (topoInfo.level0Shape == Level0Shape::MESH_1D) {
            primQueueGenName = "CcuAllGatherVMesh1D";
            return SelectorStatus::MATCH;
        } else if (topoInfo.level0Shape == Level0Shape::MESH_2D) {
            HCCL_WARNING("[Algo][AllGatherVAutoSelector] algo[%u] is not supported yet for ccu_ms 2D topo, not match.", levle0Algo); 
            return SelectorStatus::NOT_MATCH;
        }
    } else {
        HCCL_WARNING("[Algo][AllGatherVAutoSelector] algo[%u] is not supported yet for ccu_ms mode, not match.", levle0Algo);
        return SelectorStatus::NOT_MATCH;
    }
    return SelectorStatus::NOT_MATCH;
}

SelectorStatus AllGatherVAutoSelector::SelectCcuScheduleAlgo(const TopoInfo &topoInfo, const CollAlgOperator &op,
    const std::map<OpType, std::vector<HcclAlgoType>> &configAlgMap, std::string &primQueueGenName) const
{
    if (topoInfo.levelNum > 1) {
        HCCL_WARNING("[Algo][AllGatherVAutoSelector] levelNum > 1 is not supported yet for ccu_schedule mode.");
        return SelectorStatus::NOT_MATCH;
    }

    HcclAlgoType levle0Algo = HcclAlgoType::HCCL_ALGO_TYPE_DEFAULT;
    auto it = configAlgMap.find(op.opType);
    if ((it != configAlgMap.end()) && (it->second.size() > 0)) {
        levle0Algo = it->second[0];
    }
    if (IsDefaultAlg(levle0Algo) || levle0Algo == HcclAlgoType::HCCL_ALGO_TYPE_FULLMESH) {
        if (topoInfo.level0Shape == Level0Shape::MESH_1D) {
            primQueueGenName = "CcuAllGatherVMesh1D";
            return SelectorStatus::MATCH;
        } else if (topoInfo.level0Shape == Level0Shape::MESH_2D) {
            HCCL_WARNING(
                "[Algo][AllGatherVAutoSelector] algo[%u] is not supported yet for ccu_schedule 2D topo, not match.",
                levle0Algo);
            return SelectorStatus::NOT_MATCH;
        }
    } else {
        HCCL_WARNING(
            "[Algo][AllGatherVAutoSelector] algo[%u] is not supported yet for ccu_schedule mode, not match.", levle0Algo);
        return SelectorStatus::NOT_MATCH;
    }
    return SelectorStatus::NOT_MATCH;
}

SelectorStatus AllGatherVAutoSelector::SelectAicpuAlgo(const TopoInfo &topoInfo, const CollAlgOperator &op,
    const std::map<OpType, std::vector<HcclAlgoType>> &configAlgMap, std::string &primQueueGenName) const
{
    (void)topoInfo;
    (void)op;
    (void)configAlgMap;
    (void)primQueueGenName;

    // 暂时没有 aicpu 算法
    HCCL_WARNING("[Algo][AllGatherVAutoSelector] No aicpu algorithm for aicpu mode. Auto select failed.");
    return SelectorStatus::NOT_MATCH;
}

REGISTER_SELECTOR_BY_OPTYPE(OpType::ALLGATHERV, 18, AllGatherVAutoSelector);
}  // namespace Hccl

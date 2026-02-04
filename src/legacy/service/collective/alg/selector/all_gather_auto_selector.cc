/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "all_gather_auto_selector.h"
#include "selector_registry.h"
#include "coll_operator.h"
#include "coll_alg_params.h"

namespace Hccl {

constexpr u64 AG_2D_SMALL_DATA_SIZE = 1 * 1024 * 1024;

SelectorStatus AllGatherAutoSelector::SelectCcuMsAlgo(const TopoInfo &topoInfo, const CollAlgOperator &op,
    const std::map<OpType, std::vector<HcclAlgoType>> &configAlgMap, std::string &primQueueGenName) const
{
    HCCL_DEBUG("[AllGatherAutoSelector][%s] start", __func__);
    HCCL_DEBUG("[AllGatherAutoSelector][%s] topoInfo levelNum[%u]", __func__, topoInfo.levelNum);
    if (topoInfo.levelNum > 1) {
        HCCL_WARNING("[Algo][AllGatherAutoSelector] levelNum > 1 is not supported yet for ccu_ms mode.");
        return SelectorStatus::NOT_MATCH;
    }

    HcclAlgoType levle0Algo = HcclAlgoType::HCCL_ALGO_TYPE_DEFAULT;
    auto it = configAlgMap.find(op.opType);
    if ((it != configAlgMap.end()) && (it->second.size() > 0)) {
        levle0Algo = it->second[0];
    }
    if (IsDefaultAlg(levle0Algo) || levle0Algo == HcclAlgoType::HCCL_ALGO_TYPE_FULLMESH) {
        HCCL_DEBUG("[AllGatherAutoSelector][%s] SelectMeshAlgo", __func__);
        return SelectMeshAlgo(topoInfo, op, primQueueGenName);
    } else {
        HCCL_WARNING("[Algo][AllGatherAutoSelector] algo[%u] is not supported yet for ccu_ms mode, reset to default.",
            levle0Algo);
        return SelectorStatus::NOT_MATCH;
    }
    HCCL_DEBUG("[AllGatherAutoSelector][%s] end", __func__);
}

SelectorStatus AllGatherAutoSelector::SelectMeshAlgo(
    const TopoInfo &topoInfo, const CollAlgOperator &op, std::string &primQueueGenName) const
{
    (void)op;
    HCCL_DEBUG("[AllGatherAutoSelector][%s] start", __func__);
    if (topoInfo.level0Shape == Level0Shape::MESH_1D) {
        primQueueGenName = "CcuAllGatherMesh1D";
    } else if (topoInfo.level0Shape == Level0Shape::MESH_2D) {
        primQueueGenName = "CcuAllGatherMesh2D";
    }
    HCCL_DEBUG("[AllGatherAutoSelector][%s] end", __func__);
    return SelectorStatus::MATCH;
}

SelectorStatus AllGatherAutoSelector::SelectCcuScheduleAlgo(const TopoInfo &topoInfo, const CollAlgOperator &op,
    const std::map<OpType, std::vector<HcclAlgoType>> &configAlgMap, std::string &primQueueGenName) const
{
    if (topoInfo.levelNum > 1) {
        if (topoInfo.level0Shape == Level0Shape::MESH_1D) {
            if (GetNumRanksPerBoard() > 1) {
                primQueueGenName = "CcuAllGatherParallelMesh1DNHR";
                return SelectorStatus::MATCH;
            } else {
                primQueueGenName = "CcuAllGatherNHR1D";
                return SelectorStatus::MATCH;
            }
        } else {
            HCCL_WARNING("[Algo][AllGatherAutoSelector] level0Shape[%d] is not supported yet for ccu schedule mode.",
                topoInfo.level0Shape);
            return SelectorStatus::NOT_MATCH;
        }
    }

    HcclAlgoType levle0Algo = HcclAlgoType::HCCL_ALGO_TYPE_DEFAULT;
    auto it = configAlgMap.find(op.opType);
    if ((it != configAlgMap.end()) && (it->second.size() > 0)) {
        levle0Algo = it->second[0];
    }
    if ((IsDefaultAlg(levle0Algo) || levle0Algo == HcclAlgoType::HCCL_ALGO_TYPE_FULLMESH) &&
        (topoInfo.level0Shape == Level0Shape::MESH_1D)) {
        primQueueGenName = "CcuAllGatherMeshMem2Mem1D";
        return SelectorStatus::MATCH;
    } else if ((IsDefaultAlg(levle0Algo) || (levle0Algo == HcclAlgoType::HCCL_ALGO_TYPE_FULLMESH)) &&
               (topoInfo.level0Shape == Level0Shape::MESH_2D)) {
        primQueueGenName = "CcuAllGatherMeshMem2Mem2D";
        return SelectorStatus::MATCH;
    } else {
        HCCL_WARNING(
            "[Algo][AllGatherAutoSelector] algo[%u] is not supported yet for ccu_schedule mode, reset to default.",
            levle0Algo);
        return SelectorStatus::NOT_MATCH;
    }
}

SelectorStatus AllGatherAutoSelector::SelectAicpuAlgo(const TopoInfo &topoInfo, const CollAlgOperator &op,
    const std::map<OpType, std::vector<HcclAlgoType>> &configAlgMap, std::string &primQueueGenName) const
{
    std::vector<HcclAlgoType> algos =
        std::vector<HcclAlgoType>(HCCL_ALGO_LEVEL_NUM, HcclAlgoType::HCCL_ALGO_TYPE_DEFAULT);
    auto it = configAlgMap.find(op.opType);
    if (it != configAlgMap.end()) {
        algos = it->second;
    }

    HCCL_INFO("hccl algo op config: config opType:%s, level0:%u, level1:%u, level2:%u, level3:%u",
        op.opType.Describe().c_str(),
        algos[0],
        algos[1],
        algos[2],
        algos[3]);

    if (topoInfo.levelNum > 1) {
        if (topoInfo.Level1Nhr) {
            primQueueGenName = "InsAllGatherNHR";
        } else if (topoInfo.Level0Nhr) {
            primQueueGenName = "InsAllGatherParallelNHRNHR";
        } else if (topoInfo.level0Shape == Level0Shape::MESH_1D) {
            primQueueGenName = "InsAllGatherParallelMesh1DNHR";
        } else if (topoInfo.level0Shape == Level0Shape::MESH_2D) {
            primQueueGenName = "InsAllGatherParallelMesh2DNHR";
        }
    } else {
        if (topoInfo.level0Shape == Level0Shape::MESH_1D) {
            primQueueGenName = "InsAllGatherMesh";
        } else if (topoInfo.level0Shape == Level0Shape::MESH_2D) {
            primQueueGenName = "InsAllGatherMesh2D";
        } else {
            HCCL_WARNING("[AllGatherAutoSelector] topo not match");
            return SelectorStatus::NOT_MATCH;
        }
    }
    return SelectorStatus::MATCH;
}

SelectorStatus AllGatherAutoSelector::SelectAivAlgo(const TopoInfo &topoInfo, const CollAlgOperator &op,
    const std::map<OpType, std::vector<HcclAlgoType>> &configAlgMap, std::string &primQueueGenName) const
{
    std::vector<HcclAlgoType> algos =
        std::vector<HcclAlgoType>(HCCL_ALGO_LEVEL_NUM, HcclAlgoType::HCCL_ALGO_TYPE_DEFAULT);
    auto it = configAlgMap.find(op.opType);
    if (it != configAlgMap.end()) {
        algos = it->second;
    }

    HCCL_INFO("hccl algo op config: config opType:%s, level0:%u, level1:%u, level2:%u, level3:%u",
        op.opType.Describe().c_str(),
        algos[0],
        algos[1],
        algos[2],
        algos[3]);

    if (topoInfo.level0Shape == Level0Shape::MESH_1D && topoInfo.levelNum <= 1) {
        primQueueGenName = "AivAllGatherMesh1D";
    } else {
        HCCL_WARNING("[AllGatherAutoSelector] topo not match for aiv algo");
        return SelectorStatus::NOT_MATCH;
    }
    return SelectorStatus::MATCH;
}

REGISTER_SELECTOR_BY_OPTYPE(OpType::ALLGATHER, 18, AllGatherAutoSelector);
}  // namespace Hccl

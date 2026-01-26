/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: scatter 自适应算法选择实现
 * Author: libiaozhi
 * Create: 2025-03-22
 */
#include "scatter_auto_selector.h"
#include "selector_registry.h"
#include "coll_operator.h"
#include "coll_alg_params.h"

namespace Hccl {
SelectorStatus ScatterAutoSelector::SelectCcuMsAlgo(const TopoInfo &topoInfo, const CollAlgOperator &op,
    const std::map<OpType, std::vector<HcclAlgoType>> &configAlgMap, std::string &primQueueGenName) const
{
    (void)topoInfo;
    (void)op;
    (void)configAlgMap;
    (void)primQueueGenName;
    HCCL_WARNING("[Algo][ScatterAutoSelector] not supported yet for ccu_ms mode, reset to default.");
    return SelectorStatus::NOT_MATCH;
}

SelectorStatus ScatterAutoSelector::SelectCcuScheduleAlgo(const TopoInfo &topoInfo, const CollAlgOperator &op,
    const std::map<OpType, std::vector<HcclAlgoType>> &configAlgMap, std::string &primQueueGenName) const
{
    if (topoInfo.levelNum > 1) {
        if (topoInfo.level0Shape == Level0Shape::MESH_1D) {
            if (GetNumRanksPerBoard() > 1) {
                primQueueGenName = "CcuScatterParallelMesh1DNHR";
                return SelectorStatus::MATCH;
            } else {
                primQueueGenName = "CcuScatterNHRMem2Mem1D";
                return SelectorStatus::MATCH;
            }
        } else {
            HCCL_WARNING("[Algo][ScatterAutoSelector] level0Shape[%d] is not supported yet for ccu schedule mode.",
                topoInfo.level0Shape);
            return SelectorStatus::NOT_MATCH;
        }
    }

    HcclAlgoType levle0Algo = HcclAlgoType::HCCL_ALGO_TYPE_DEFAULT;
    auto it = configAlgMap.find(op.opType);
    if ((it != configAlgMap.end()) && (it->second.size() > 0)) {
        levle0Algo = it->second[0];
    }
    if (IsDefaultAlg(levle0Algo) || levle0Algo == HcclAlgoType::HCCL_ALGO_TYPE_FULLMESH) {
        if (topoInfo.level0Shape == Level0Shape::MESH_1D) {
            primQueueGenName = "CcuScatterMesh1D";
        } else if (topoInfo.level0Shape == Level0Shape::MESH_2D) {
            primQueueGenName = "CcuScatterMesh2D";
        }
        return SelectorStatus::MATCH;
    } else {
        HCCL_WARNING(
            "[Algo][ScatterAutoSelector] algo[%u] is not supported yet for ccu_schedule mode, reset to default.",
            levle0Algo);
        return SelectorStatus::NOT_MATCH;
    }
}

SelectorStatus ScatterAutoSelector::SelectAicpuAlgo(const TopoInfo &topoInfo, const CollAlgOperator &op,
    const std::map<OpType, std::vector<HcclAlgoType>> &configAlgMap, std::string &primQueueGenName) const
{
    (void)topoInfo;
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
        primQueueGenName = "InsScatterParallelMesh1DNHR";
    } else {
        if (topoInfo.level0Shape == Level0Shape::MESH_1D) {
            primQueueGenName = "InsScatterMesh1D";
        } else if (topoInfo.level0Shape == Level0Shape::MESH_2D) {
            primQueueGenName = "InsScatterMesh2D";
        } else {
            HCCL_WARNING("[ScatterAutoSelector] topo not match");
            return SelectorStatus::NOT_MATCH;
        }
    }
    return SelectorStatus::MATCH;
}

SelectorStatus ScatterAutoSelector::SelectAivAlgo(const TopoInfo &topoInfo, const CollAlgOperator &op,
    const std::map<OpType, std::vector<HcclAlgoType>> &configAlgMap, std::string &primQueueGenName) const
{
    std::vector<HcclAlgoType> algos = std::vector<HcclAlgoType>(HCCL_ALGO_LEVEL_NUM, HcclAlgoType::HCCL_ALGO_TYPE_DEFAULT);
    auto it = configAlgMap.find(op.opType);
    if (it != configAlgMap.end()) {
        algos = it->second;
    }

    HCCL_INFO("hccl algo op config: config opType:%s, level0:%u, level1:%u, level2:%u, level3:%u",
        op.opType.Describe().c_str(), algos[0], algos[1], algos[2], algos[3]);
 
    if (topoInfo.level0Shape == Level0Shape::MESH_1D && topoInfo.levelNum <= 1) {
        primQueueGenName = "AivScatterMesh1D";
    } else {
        HCCL_WARNING("[ScatterAutoSelector] topo not match for aiv algo");
        return SelectorStatus::NOT_MATCH;
    }
    return SelectorStatus::MATCH;
}

REGISTER_SELECTOR_BY_OPTYPE(OpType::SCATTER, 18, ScatterAutoSelector);
}  // namespace Hccl

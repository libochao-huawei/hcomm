/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "broadcast_auto_selector.h"
#include "selector_registry.h"
#include "coll_operator.h"
#include "coll_alg_params.h"

namespace Hccl {
SelectorStatus BroadcastAutoSelector::SelectCcuMsAlgo(const TopoInfo &topoInfo,
                                                    const CollAlgOperator &op,
                                                    const std::map<OpType, std::vector<HcclAlgoType>> &configAlgMap,
                                                    std::string &primQueueGenName) const
{
    if (topoInfo.levelNum > 1) {
        HCCL_WARNING("[Algo][BroadcastAutoSelector] levelNum > 1 is not supported yet for ccu_ms mode.");
        return SelectorStatus::NOT_MATCH;
    }

    HcclAlgoType levle0Algo = HcclAlgoType::HCCL_ALGO_TYPE_DEFAULT;
    auto it = configAlgMap.find(op.opType);
    if ((it != configAlgMap.end()) && (it->second.size() > 0)) {
        levle0Algo = it->second[0];
    }
    if (IsDefaultAlg(levle0Algo) || levle0Algo ==  HcclAlgoType::HCCL_ALGO_TYPE_FULLMESH) {
        return SelectMeshAlgoCcuMs(topoInfo, op, primQueueGenName);
    } else {
        HCCL_WARNING("[Algo][BroadcastAutoSelector] algo[%u] is not supported yet for ccu_ms mode, reset to default.", levle0Algo);
        return SelectorStatus::NOT_MATCH;
    }
}

SelectorStatus BroadcastAutoSelector::SelectMeshAlgoCcuMs(const TopoInfo &topoInfo,
                                                    const CollAlgOperator &op,
                                                    std::string &primQueueGenName) const
{
    (void)op;
    if (topoInfo.level0Shape == Level0Shape::MESH_1D) {
        primQueueGenName = "CcuBroadcastMesh1D";
    } else if (topoInfo.level0Shape == Level0Shape::MESH_2D) {
        primQueueGenName = "CcuBroadcastMesh2D";
    }
    return SelectorStatus::MATCH;
}

SelectorStatus BroadcastAutoSelector::SelectCcuScheduleAlgo(const TopoInfo &topoInfo,
                                                    const CollAlgOperator &op,
                                                    const std::map<OpType, std::vector<HcclAlgoType>> &configAlgMap,
                                                    std::string &primQueueGenName) const
{
    if (topoInfo.levelNum > 1) {
        if (topoInfo.level0Shape == Level0Shape::MESH_1D) {
            if(GetNumRanksPerBoard() > 1){
                primQueueGenName = "CcuBroadcastParallelMesh1DNHR";
                return SelectorStatus::MATCH;
            }else {
                primQueueGenName = "CcuBroadcastNHRMem2Mem1D";
                return SelectorStatus::MATCH;
            }
        } else {
             HCCL_WARNING("[Algo][SelectCcuScheduleAlgo] level0Shape[%d] is not supported yet for ccu schedule mode.",
                topoInfo.level0Shape);
            return  SelectorStatus::NOT_MATCH;
        }
    } else {
        HcclAlgoType levle0Algo = HcclAlgoType::HCCL_ALGO_TYPE_DEFAULT;
        auto it = configAlgMap.find(op.opType);
        if ((it != configAlgMap.end()) && (it->second.size() > 0)) {
            levle0Algo = it->second[0];
        }
        if ((IsDefaultAlg(levle0Algo) || levle0Algo == HcclAlgoType::HCCL_ALGO_TYPE_FULLMESH) && (topoInfo.level0Shape == Level0Shape::MESH_1D)) {
            primQueueGenName = "CcuBroadcastMeshMem2Mem1D";
            return SelectorStatus::MATCH;
        } else if ((IsDefaultAlg(levle0Algo) || levle0Algo == HcclAlgoType::HCCL_ALGO_TYPE_FULLMESH) && (topoInfo.level0Shape == Level0Shape::MESH_2D)) {
            primQueueGenName = "CcuBroadcastMeshMem2Mem2D";
            return SelectorStatus::MATCH;
        } else {
            HCCL_WARNING("[Algo][BroadcastAutoSelector] algo[%u] is not supported yet for ccu_schedule mode, reset to default.", levle0Algo);
            return SelectorStatus::NOT_MATCH;
        }
    }
}

SelectorStatus BroadcastAutoSelector::SelectAicpuAlgo(const TopoInfo &topoInfo,
                                                      const CollAlgOperator &op,
                                                      const std::map<OpType, std::vector<HcclAlgoType>> &configAlgMap,
                                                      std::string &primQueueGenName) const
{
    std::vector<HcclAlgoType> algos = std::vector<HcclAlgoType>(HCCL_ALGO_LEVEL_NUM, HcclAlgoType::HCCL_ALGO_TYPE_DEFAULT);
    auto it = configAlgMap.find(op.opType);
    if (it != configAlgMap.end()) {
        algos = it->second;
    }

    HCCL_INFO("hccl algo op config: config opType:%s, level0:%u, level1:%u, level2:%u, level3:%u",
        op.opType.Describe().c_str(), algos[0], algos[1], algos[2], algos[3]);

    if (topoInfo.levelNum > 1) {
        if (topoInfo.level0Shape == Level0Shape::MESH_1D) {
            primQueueGenName = "InsBroadcastParallelMesh1DNHR";
        } else {
            primQueueGenName = "InsBroadcastNHR";
        }
    } else {
        return SelectMeshAlgoAicpu(topoInfo, op, primQueueGenName);
    }
    return SelectorStatus::MATCH;
}

SelectorStatus BroadcastAutoSelector::SelectMeshAlgoAicpu(const TopoInfo &topoInfo,
                                                          const CollAlgOperator &op,
                                                          std::string &primQueueGenName) const
{
    (void)topoInfo;
    (void)op;
    if (topoInfo.level0Shape == Level0Shape::MESH_1D) {
        if(IsSmallData(dataSize_)){
            primQueueGenName = "InsBroadcastMesh1DOneShot";
        } else {
            primQueueGenName = "InsBroadcastMesh1DTwoShot";
        }
    } else if (topoInfo.level0Shape == Level0Shape::MESH_2D) {
        primQueueGenName = "InsBroadcastMesh2DTwoShot";
    } else {
        HCCL_WARNING("[BroadcastAutoSelector] topo not match");
        return SelectorStatus::NOT_MATCH;
    }

    return SelectorStatus::MATCH;
}


SelectorStatus BroadcastAutoSelector::SelectAivAlgo(const TopoInfo &topoInfo,
                                                      const CollAlgOperator &op,
                                                      const std::map<OpType, std::vector<HcclAlgoType>> &configAlgMap,
                                                      std::string &primQueueGenName) const
{
    std::vector<HcclAlgoType> algos = std::vector<HcclAlgoType>(HCCL_ALGO_LEVEL_NUM, HcclAlgoType::HCCL_ALGO_TYPE_DEFAULT);
    auto it = configAlgMap.find(op.opType);
    if (it != configAlgMap.end()) {
        algos = it->second;
    }

    HCCL_INFO("hccl algo op config: config opType:%s, level0:%u, level1:%u, level2:%u, level3:%u",
        op.opType.Describe().c_str(), algos[0], algos[1], algos[2], algos[3]);
 
    if (topoInfo.level0Shape == Level0Shape::MESH_1D && topoInfo.levelNum <= 1) {
        primQueueGenName = "AivBroadcastMesh1D";
    } else {
        HCCL_WARNING("[BroadcastAutoSelector] topo not match for aiv algo");
        return  SelectorStatus::NOT_MATCH;
    }
    return SelectorStatus::MATCH;
}

REGISTER_SELECTOR_BY_OPTYPE(OpType::BROADCAST, 18, BroadcastAutoSelector);
} // namespace Hccl

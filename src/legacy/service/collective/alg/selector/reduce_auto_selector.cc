/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "reduce_auto_selector.h"
#include "selector_registry.h"
#include "coll_operator.h"
#include "coll_alg_params.h"

namespace Hccl {

SelectorStatus ReduceAutoSelector::SelectCcuMsAlgo(const TopoInfo &topoInfo,
                                                    const CollAlgOperator &op,
                                                    const std::map<OpType, std::vector<HcclAlgoType>> &configAlgMap,
                                                    std::string &primQueueGenName) const
{
    if (topoInfo.levelNum > 1) {
        HCCL_WARNING("[Algo][ReduceAutoSelector] levelNum > 1 is not supported yet for ccu_ms mode.");
        return SelectorStatus::NOT_MATCH;
    }

    // MS 模式不支持 int8
    CHK_PRT_RET(op.dataType == DataType::INT8,
        HCCL_WARNING("[Algo][ReduceAutoSelector] dataType[%s] is not supported yet for ccu_ms mode.",
            op.dataType.Describe().c_str()),
        SelectorStatus::NOT_MATCH);

    // MS 模式不支持 PROD
    CHK_PRT_RET(op.reduceOp == ReduceOp::PROD,
        HCCL_WARNING("[Algo][ReduceAutoSelector] ReduceOp[%s] is not supported yet for ccu_ms mode.",
            op.reduceOp.Describe().c_str()),
        SelectorStatus::NOT_MATCH);
    
    if (op.dataType == DataType::INT64 || op.dataType == DataType::UINT64 || op.dataType == DataType::FP64) {
        HCCL_WARNING("[Algo][ReduceAutoSelector] ccu_ms mode not support INT64, UINT64, FP64.");
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
        HCCL_WARNING("[Algo][ReduceAutoSelector] algo[%u] is not supported yet for ccu_ms mode, reset to default.", levle0Algo);
         return SelectorStatus::NOT_MATCH;
    }
}

SelectorStatus ReduceAutoSelector::SelectMeshAlgoCcuMs(const TopoInfo &topoInfo,
                                                    const CollAlgOperator &op,
                                                    std::string &primQueueGenName) const
{
    (void)op;
    if (topoInfo.level0Shape == Level0Shape::MESH_1D) {
            primQueueGenName = "CcuReduceMesh1D";
    } else if (topoInfo.level0Shape == Level0Shape::MESH_2D) {
        primQueueGenName = "CcuReduceMesh2D";
    }
    return SelectorStatus::MATCH;
}

SelectorStatus ReduceAutoSelector::SelectCcuScheduleAlgo(const TopoInfo &topoInfo, const CollAlgOperator &op,
    const std::map<OpType, std::vector<HcclAlgoType>> &configAlgMap, std::string &primQueueGenName) const
{
    // ccu 模式不支持 inplace 场景
    CHK_PRT_RET(IsInputOutputOverlap(op.inputMem, op.outputMem) == true,
        HCCL_WARNING("[Algo][ReduceAutoSelector] ccu schedule does not support inplace allreduce."),
        SelectorStatus::NOT_MATCH);
    
    // ccu 模式不支持 PROD
    CHK_PRT_RET(op.reduceOp == ReduceOp::PROD,
        HCCL_WARNING("[Algo][ReduceAutoSelector] ReduceOp[%s] is not supported yet for ccu schedule mode.",
            op.reduceOp.Describe().c_str()),
        SelectorStatus::NOT_MATCH);

    if (op.dataType == DataType::INT64 || op.dataType == DataType::UINT64 || op.dataType == DataType::FP64) {
        HCCL_WARNING("[Algo][ReduceAutoSelector] ccu_schedule mode not support INT64, UINT64, FP64.");
        return SelectorStatus::NOT_MATCH;
    }

    if (topoInfo.levelNum > 1) {
        if (topoInfo.level0Shape == Level0Shape::MESH_1D) {
            if (GetNumRanksPerBoard() > 1) {
                primQueueGenName = "CcuReduceParallelMesh1DNHR";
                return SelectorStatus::MATCH;
            } else {
                primQueueGenName = "CcuReduceNHR1D";
                return SelectorStatus::MATCH;
            }
        } else {
            HCCL_WARNING("[Algo][ReduceAutoSelector] levelNum [%u] > 1 is not supported yet for ccu_schedule mode.",
                topoInfo.levelNum);
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
        primQueueGenName = "CcuReduceMeshMem2Mem1D";
        return SelectorStatus::MATCH;
    } else if ((IsDefaultAlg(levle0Algo) || levle0Algo == HcclAlgoType::HCCL_ALGO_TYPE_FULLMESH) &&
               (topoInfo.level0Shape == Level0Shape::MESH_2D)) {
        primQueueGenName = "CcuReduceMeshMem2Mem2D";
        return SelectorStatus::MATCH;
    } else {
        HCCL_WARNING(
            "[Algo][ReduceAutoSelector] algo[%u] is not supported yet for ccu_schedule mode, reset to default.",
            levle0Algo);
        return SelectorStatus::NOT_MATCH;
    }
}

SelectorStatus ReduceAutoSelector::SelectAicpuAlgo(const TopoInfo &topoInfo,
                                                      const CollAlgOperator &op,
                                                      const std::map<OpType, std::vector<HcclAlgoType>> &configAlgMap,
                                                      std::string &primQueueGenName) const
{
    (void) topoInfo;
    std::vector<HcclAlgoType> algos = std::vector<HcclAlgoType>(HCCL_ALGO_LEVEL_NUM, HcclAlgoType::HCCL_ALGO_TYPE_DEFAULT);
    auto it = configAlgMap.find(op.opType);
    if (it != configAlgMap.end()) {
        algos = it->second;
    }

    HCCL_INFO("hccl algo op config: config opType:%s, level0:%u, level1:%u, level2:%u, level3:%u",
        op.opType.Describe().c_str(), algos[0], algos[1], algos[2], algos[3]);

    if (topoInfo.levelNum > 1) {
        if (op.dataType == DataType::INT64 || op.dataType == DataType::UINT64 ||
                op.dataType == DataType::FP64) {
            HCCL_ERROR("[SelectAicpuAlgo] INT64, UINT64, FP64 only support in-box fullmesh algo type now.");
            return SelectorStatus::NOT_MATCH;
        }
        if (topoInfo.level0Shape == Level0Shape::MESH_1D) {
            primQueueGenName = "InsReduceParallelMesh1DNHR";
        } else {
            primQueueGenName = "InsReduceNHR";
        }
    } else {
        if (topoInfo.level0Shape == Level0Shape::MESH_1D) {
            if (op.dataType == DataType::INT64 || op.dataType == DataType::UINT64 || op.dataType == DataType::FP64) {
                primQueueGenName = "InsReduceAicpuReduce";
            } else {
                primQueueGenName = "InsReduceMesh1D";
            }
        } else if (topoInfo.level0Shape == Level0Shape::MESH_2D) {
            if (op.dataType == DataType::INT64 || op.dataType == DataType::UINT64 || op.dataType == DataType::FP64) {
                primQueueGenName = "InsReduceAicpuReduceMesh2D";
            } else {
                primQueueGenName = "InsReduceMesh2D";
            }
        } else {
            HCCL_WARNING("[ReduceAutoSelector] topo not match");
            return SelectorStatus::NOT_MATCH;
        }
    }

    return SelectorStatus::MATCH;
}

SelectorStatus ReduceAutoSelector::SelectAivAlgo(const TopoInfo &topoInfo,
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

    //aiv 模式不支持 PROD
    CHK_PRT_RET(op.reduceOp == ReduceOp::PROD,
        HCCL_WARNING("[Algo][ReduceAutoSelector] ReduceOp[%s] is not supported yet for aiv mode.",
            op.reduceOp.Describe().c_str()),
        SelectorStatus::NOT_MATCH);
    
    if (op.dataType == DataType::INT64 || op.dataType == DataType::UINT64 || op.dataType == DataType::FP64) {
        HCCL_WARNING("[Algo][ReduceAutoSelector] aiv mode not support INT64, UINT64, FP64.");
        return SelectorStatus::NOT_MATCH;
    }

    if (topoInfo.level0Shape == Level0Shape::MESH_1D && topoInfo.levelNum <= 1) {
        primQueueGenName = "AivReduceMesh1D";
    } else {
        HCCL_WARNING("[ReduceAutoSelector] topo not match for aiv algo");
        return  SelectorStatus::NOT_MATCH;
    }
    return SelectorStatus::MATCH;
}

REGISTER_SELECTOR_BY_OPTYPE(OpType::REDUCE, 18, ReduceAutoSelector);
} // namespace Hccl

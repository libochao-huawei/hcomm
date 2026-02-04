/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "all_reduce_auto_selector.h"
#include "selector_registry.h"
#include "coll_operator.h"
#include "coll_alg_params.h"

namespace Hccl {
constexpr u64 AR_M2M_1D_MAX_DATA_SIZE = 16 * 1024 * 1024;
constexpr u64 AR_AICPU_1D_SMALL_DATA_SIZE = 8 * 1024 * 1024;
constexpr u64 AR_AICPU_1D_MAX_DATA_SIZE = 32 * 1024 * 1024;
constexpr u64 AR_ONESHOT_1D_MAX_DATA_SIZE = 16 * 1024;

SelectorStatus AllReduceAutoSelector::SelectCcuMsAlgo(const TopoInfo &topoInfo,
                                                    const CollAlgOperator &op,
                                                    const std::map<OpType, std::vector<HcclAlgoType>> &configAlgMap,
                                                    std::string &primQueueGenName) const
{
    if (topoInfo.levelNum > 1) {
        HCCL_WARNING("[Algo][AllReduceAutoSelector] levelNum > 1 is not supported yet for ccu_ms mode.");
        return SelectorStatus::NOT_MATCH;
    }

    // MS 模式不支持 int8
    CHK_PRT_RET(op.dataType == DataType::INT8,
        HCCL_WARNING("[Algo][AllReduceAutoSelector] dataType[%s] is not supported yet for ccu_ms mode.",
            op.dataType.Describe().c_str()),
        SelectorStatus::NOT_MATCH);

    // MS 模式不支持 PROD
    CHK_PRT_RET(op.reduceOp == ReduceOp::PROD,
        HCCL_WARNING("[Algo][AllReduceAutoSelector] ReduceOp[%s] is not supported yet for ccu_ms mode.",
            op.reduceOp.Describe().c_str()),
        SelectorStatus::NOT_MATCH);

    if (op.dataType == DataType::INT64 || op.dataType == DataType::UINT64 || op.dataType == DataType::FP64) {
        HCCL_WARNING("[Algo][AllReduceAutoSelector] ccu_ms mode not support INT64, UINT64, FP64.");
        return SelectorStatus::NOT_MATCH;
    }

    HcclAlgoType levle0Algo = HcclAlgoType::HCCL_ALGO_TYPE_DEFAULT;
    auto it = configAlgMap.find(op.opType);
    if ((it != configAlgMap.end()) && (it->second.size() > 0)) {
        levle0Algo = it->second[0];
    }

    if (IsDefaultAlg(levle0Algo) || levle0Algo ==  HcclAlgoType::HCCL_ALGO_TYPE_FULLMESH) {
        return SelectMeshAlgo(topoInfo, op, primQueueGenName);
    } else {
        HCCL_WARNING("[Algo][AllReduceAutoSelector] algo[%u] is not supported yet for ccu_ms mode, reset to default.", levle0Algo);
        return SelectorStatus::NOT_MATCH;
    }
}

SelectorStatus AllReduceAutoSelector::SelectMeshAlgo(const TopoInfo &topoInfo,
                                                    const CollAlgOperator &op,
                                                    std::string &primQueueGenName) const
{
    (void)op;
    if (topoInfo.level0Shape == Level0Shape::MESH_1D) {
        if (IsInputOutputOverlap(op.inputMem, op.outputMem) == true) {
            return SelectorStatus::NOT_MATCH;
        }
        if (dataSize_ / rankSize_ > AR_ONESHOT_1D_MAX_DATA_SIZE) {
 	        primQueueGenName = "CcuAllReduceMesh1D";
 	    } else {
 	        primQueueGenName = "CcuAllReduceMesh1DOneShot";
 	    }
    } else if (topoInfo.level0Shape == Level0Shape::MESH_2D) {
        if (IsSmallData(dataSize_) && IsInputOutputOverlap(op.inputMem, op.outputMem) != true) {
            primQueueGenName = "CcuAllReduceMesh2DOneShot";
        } else {
            primQueueGenName = "CcuAllReduceMesh2DTwoShot";
        }
    }
    return SelectorStatus::MATCH;
}

SelectorStatus AllReduceAutoSelector::SelectCcuScheduleAlgo(const TopoInfo &topoInfo,
                                                    const CollAlgOperator &op,
                                                    const std::map<OpType, std::vector<HcclAlgoType>> &configAlgMap,
                                                    std::string &primQueueGenName) const
{
    // ccu 模式不支持 PROD
    CHK_PRT_RET(op.reduceOp == ReduceOp::PROD,
        HCCL_WARNING("[Algo][AllReduceAutoSelector] ReduceOp[%s] is not supported yet for ccu schedule mode.",
            op.reduceOp.Describe().c_str()),
        SelectorStatus::NOT_MATCH);

    if (op.dataType == DataType::INT64 || op.dataType == DataType::UINT64 || op.dataType == DataType::FP64) {
        HCCL_WARNING("[Algo][AllReduceAutoSelector] ccu_schedule mode not support INT64, UINT64, FP64.");
        return SelectorStatus::NOT_MATCH;
    }

    if (topoInfo.levelNum > 1) {
        if (topoInfo.level0Shape == Level0Shape::MESH_1D) {
            if (GetNumRanksPerBoard() > 1) {
                // 性能优化改用MS做reduce后不支持int8
                CHK_PRT_RET(op.dataType == DataType::INT8,
                    HCCL_WARNING("[Algo][AllReduceAutoSelector] dataType[%s] is not supported yet for ccu schedule "
                                 "mode with ms reduce. levelNum[%u]",
                        op.dataType.Describe().c_str(), topoInfo.levelNum),
                    SelectorStatus::NOT_MATCH);
                primQueueGenName = "CcuAllReduceParallelMesh1DNHR";
                return SelectorStatus::MATCH;
            } else {
                primQueueGenName = "CcuAllReduceNHR1D";
                return SelectorStatus::MATCH;
            }
        } else {
            HCCL_WARNING("[Algo][AllReduceAutoSelector] level0Shape[%d] is not supported yet for ccu schedule mode.",
                topoInfo.level0Shape);
            return SelectorStatus::NOT_MATCH;
        }
    } else {
        HcclAlgoType levle0Algo = HcclAlgoType::HCCL_ALGO_TYPE_DEFAULT;
        auto it = configAlgMap.find(op.opType);
        if ((it != configAlgMap.end()) && (it->second.size() > 0)) {
            levle0Algo = it->second[0];
        }
        if ((IsDefaultAlg(levle0Algo) || levle0Algo == HcclAlgoType::HCCL_ALGO_TYPE_FULLMESH) &&
            (topoInfo.level0Shape == Level0Shape::MESH_1D)) {
            // 性能优化改用MS做reduce后不支持int8
            CHK_PRT_RET(op.dataType == DataType::INT8,
                HCCL_WARNING("[Algo][AllReduceAutoSelector] dataType[%s] is not supported yet for ccu schedule mode "
                             "with ms reduce.",
                    op.dataType.Describe().c_str()),
                SelectorStatus::NOT_MATCH);
            if (dataSize_ > AR_M2M_1D_MAX_DATA_SIZE) {
                return SelectorStatus::NOT_MATCH;
            }
            primQueueGenName = "CcuAllReduceMeshMem2Mem1D";
            return SelectorStatus::MATCH;
        } else if ((IsDefaultAlg(levle0Algo) || levle0Algo == HcclAlgoType::HCCL_ALGO_TYPE_FULLMESH) &&
                   (topoInfo.level0Shape == Level0Shape::MESH_2D)) {
            primQueueGenName = "CcuAllReduceMeshTwoShotMem2Mem2D";
            return SelectorStatus::MATCH;
        } else {
            HCCL_WARNING("[Algo][AllReduceAutoSelector] algo[%u] is not supported yet for ccu_schedule mode, reset to default.", levle0Algo);
            return  SelectorStatus::NOT_MATCH;
        }
    }
}

SelectorStatus AllReduceAutoSelector::SelectAicpuAlgo(const TopoInfo &topoInfo,
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
        if (topoInfo.Level1Nhr) {
            primQueueGenName = "InsAllReduceNHR";
        } else if (topoInfo.Level0Nhr) {
            primQueueGenName = "InsAllReduceParallelNHRNHR";
        } else if (topoInfo.level0Shape == Level0Shape::MESH_1D) {
            primQueueGenName = "InsAllReduceParallelMesh1DNHR";
        } else if (topoInfo.level0Shape == Level0Shape::MESH_2D) {
            primQueueGenName = "InsAllReduceParallelMesh2DNHR";
        } else {
            return SelectorStatus::NOT_MATCH;
        }
    } else {
        return SelectMeshAlgoAicpu(topoInfo, op, primQueueGenName);
    }

    if (op.dataType == DataType::INT64 || op.dataType == DataType::UINT64 || op.dataType == DataType::FP64) {
        HCCL_ERROR("[SelectAicpuAlgo] INT64, UINT64, FP64 only support in-box fullmesh algo type now.");
        return SelectorStatus::NOT_MATCH;
    }
    return SelectorStatus::MATCH;
}

SelectorStatus AllReduceAutoSelector::SelectMeshAlgoAicpu(const TopoInfo &topoInfo,
                                                          const CollAlgOperator &op,
                                                          std::string &primQueueGenName) const
{
    if (topoInfo.level0Shape == Level0Shape::MESH_1D) {
        if (op.dataType == DataType::INT64 || op.dataType == DataType::UINT64 ||
            op.dataType == DataType::FP64) {
            primQueueGenName = "InsAllReduceAicpuReduce";
        } else if (dataSize_ <= AR_AICPU_1D_SMALL_DATA_SIZE) {
            primQueueGenName = "InsAllReduceMesh1DOneShot";
        } else if (dataSize_ > AR_AICPU_1D_MAX_DATA_SIZE) {
            primQueueGenName = "InsAllReduceMesh1DTwoShotMeshChunk";
        } else {
            primQueueGenName = "InsAllReduceMesh1DTwoShot";
        }
    } else if (topoInfo.level0Shape == Level0Shape::MESH_2D) {
        if (op.dataType == DataType::INT64 || op.dataType == DataType::UINT64 ||
            op.dataType == DataType::FP64) {
            primQueueGenName = "InsAllReduceAicpuReduceMesh2D";
        } else {
            primQueueGenName = "InsAllReduceMesh2DTwoShot";
        }
    } else {
        HCCL_WARNING("[AllReduceAutoSelector] topo not match");
        return SelectorStatus::NOT_MATCH;
    }

    return SelectorStatus::MATCH;
}

SelectorStatus AllReduceAutoSelector::SelectAivAlgo(const TopoInfo &topoInfo,
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
        HCCL_WARNING("[Algo][AllReduceAutoSelector] ReduceOp[%s] is not supported yet for aiv mode.",
            op.reduceOp.Describe().c_str()),
        SelectorStatus::NOT_MATCH);

    if (op.dataType == DataType::INT64 || op.dataType == DataType::UINT64 || op.dataType == DataType::FP64) {
        HCCL_WARNING("[Algo][AllReduceAutoSelector] aiv mode not support INT64, UINT64, FP64.");
        return SelectorStatus::NOT_MATCH;
    }

    if (topoInfo.level0Shape == Level0Shape::MESH_1D && topoInfo.levelNum <= 1) {
        if (IsSmallData(dataSize_)) {
            primQueueGenName = "AivAllReduceMesh1DOneShot";
        } else {
            primQueueGenName = "AivAllReduceMesh1DTwoShot";
        }
    } else {
        HCCL_WARNING("[AllReduceAutoSelector] topo not match for aiv algo");
        return  SelectorStatus::NOT_MATCH;
    }

    return SelectorStatus::MATCH;
}

REGISTER_SELECTOR_BY_OPTYPE(OpType::ALLREDUCE, 18, AllReduceAutoSelector);
} // namespace Hccl

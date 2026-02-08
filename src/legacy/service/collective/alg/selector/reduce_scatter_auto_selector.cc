/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "reduce_scatter_auto_selector.h"
#include "selector_registry.h"
#include "coll_operator.h"
#include "coll_alg_params.h"

namespace Hccl {
constexpr u64 RS_2D_SMALL_DATA_SIZE = 1024 * 1024;
constexpr u64 RS_M2M_1D_MAX_DATA_SIZE = 8 * 1024 * 1024;
constexpr u64 RS_AICPU_1D_MAX_DATA_SIZE = 16 * 1024 * 1024;

SelectorStatus ReduceScatterAutoSelector::SelectCcuMsAlgo(const TopoInfo &topoInfo,
                                                    const CollAlgOperator &op,
                                                    const std::map<OpType, std::vector<HcclAlgoType>> &configAlgMap,
                                                    std::string &primQueueGenName) const
{
    if (topoInfo.levelNum > 1) {
        HCCL_WARNING("[Algo][ReduceScatterAutoSelector] levelNum > 1 is not supported yet for ccu_ms mode.");
        return SelectorStatus::NOT_MATCH;
    }

    // MS 模式不支持 int8
    CHK_PRT_RET(op.dataType == DataType::INT8,
        HCCL_WARNING("[Algo][ReduceScatterAutoSelector] dataType[%s] is not supported yet for ccu_ms mode.",
            op.dataType.Describe().c_str()),
        SelectorStatus::NOT_MATCH);

    // MS 模式不支持 PROD
    CHK_PRT_RET(op.reduceOp == ReduceOp::PROD,
        HCCL_WARNING("[Algo][ReduceScatterAutoSelector] ReduceOp[%s] is not supported yet for ccu_ms mode.",
            op.reduceOp.Describe().c_str()),
        SelectorStatus::NOT_MATCH);

    if (op.dataType == DataType::INT64 || op.dataType == DataType::UINT64 || op.dataType == DataType::FP64) {
        HCCL_WARNING("[Algo][ReduceScatterAutoSelector] ccu_ms mode not support INT64, UINT64, FP64.");
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
        HCCL_WARNING("[Algo][ReduceScatterAutoSelector] algo[%u] is not supported yet for ccu_ms mode, reset to default.", levle0Algo);
        return SelectorStatus::NOT_MATCH;
    }
}

SelectorStatus ReduceScatterAutoSelector::SelectMeshAlgo(const TopoInfo &topoInfo,
                                                    const CollAlgOperator &op,
                                                    std::string &primQueueGenName) const
{
    (void)op;
    if (topoInfo.level0Shape == Level0Shape::MESH_1D) {
        HcclDetourType detourType = EnvConfig::GetInstance().GetDetourConfig().GetDetourType();
        if ((detourType == HcclDetourType::HCCL_DETOUR_ENABLE_2P && rankSize_ == 2)||
            (detourType == HcclDetourType::HCCL_DETOUR_ENABLE_4P && rankSize_ == 4)) {
            primQueueGenName = "CcuReduceScatterMeshDetour1D";
        } else if ((detourType == HcclDetourType::HCCL_DETOUR_ENABLE_2P && rankSize_ != 2)||
            (detourType == HcclDetourType::HCCL_DETOUR_ENABLE_4P && rankSize_ != 4)) {
            HCCL_WARNING("[Algo][ReduceScatterAutoSelector] detourType not match for rankSize.");
            return SelectorStatus::NOT_MATCH;
        } else if (detourType == HcclDetourType::HCCL_DETOUR_ENABLE_2P_AND_4P) {
            HCCL_WARNING("[Algo][ReduceScatterAutoSelector] HCCL_DETOUR_ENABLE_2P_AND_4P is not supported yet.");
            return SelectorStatus::NOT_MATCH;
        } else {
            primQueueGenName = "CcuReduceScatterMesh1D";
        }
    } else if (topoInfo.level0Shape == Level0Shape::MESH_2D) {
        primQueueGenName = "CcuReduceScatterMesh2D";
    }
    return SelectorStatus::MATCH;
}

SelectorStatus ReduceScatterAutoSelector::SelectCcuScheduleAlgo(const TopoInfo &topoInfo,
                                                    const CollAlgOperator &op,
                                                    const std::map<OpType, std::vector<HcclAlgoType>> &configAlgMap,
                                                    std::string &primQueueGenName) const
{
    // ccu 模式不支持 PROD
    CHK_PRT_RET(op.reduceOp == ReduceOp::PROD,
        HCCL_WARNING("[Algo][ReduceScatterAutoSelector] ReduceOp[%s] is not supported yet for ccu schedule mode.",
            op.reduceOp.Describe().c_str()),
        SelectorStatus::NOT_MATCH);

    if (op.dataType == DataType::INT64 || op.dataType == DataType::UINT64 || op.dataType == DataType::FP64) {
        HCCL_WARNING("[Algo][ReduceScatterAutoSelector] ccu_schedule mode not support INT64, UINT64, FP64.");
        return SelectorStatus::NOT_MATCH;
    }

    if (topoInfo.levelNum > 1) {
        if (topoInfo.level0Shape == Level0Shape::MESH_1D) {
            if (GetNumRanksPerBoard() > 1) {
                // 性能优化改用MS做reduce后不支持int8
                CHK_PRT_RET(op.dataType == DataType::INT8,
                    HCCL_WARNING("[Algo][ReduceScatterAutoSelector] dataType[%s] is not supported yet for "
                                 "ccu_schedule mode with ms reduce. levelNum[%u]",
                        op.dataType.Describe().c_str(), topoInfo.levelNum),
                    SelectorStatus::NOT_MATCH);
                primQueueGenName = "CcuReduceScatterParallelMesh1DNHR";
                return SelectorStatus::MATCH;
            } else {
                primQueueGenName = "CcuReduceScatterNHR1DMem2Mem";
                return SelectorStatus::MATCH;
            }
        } else {
            HCCL_WARNING("[Algo][SelectCcuScheduleAlgo] level0Shape[%d] is not supported yet for ccu schedule mode.",
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
                HCCL_WARNING("[Algo][ReduceScatterAutoSelector] dataType[%s] is not supported yet for "
                             "ccu_schedule mode with ms reduce.",
                    op.dataType.Describe().c_str()),
                SelectorStatus::NOT_MATCH);
            
            double ratio;   // 以8卡为基线确定ratio，用来表示不同卡数对下发的影响系数
            if (rankSize_ == 0) {
                HCCL_WARNING("[ReduceScatterAutoSelector]the selector is not set rankSize_");
                ratio = 1;
            } else {
                ratio = 8.0 / rankSize_;
            }
            if (dataSize_ * ratio >= RS_M2M_1D_MAX_DATA_SIZE) {
                return SelectorStatus::NOT_MATCH;
            }
            primQueueGenName = "CcuReduceScatterMeshMem2Mem1D";
            return SelectorStatus::MATCH;
        } else if ((IsDefaultAlg(levle0Algo) || (levle0Algo == HcclAlgoType::HCCL_ALGO_TYPE_FULLMESH)) &&
                   (topoInfo.level0Shape == Level0Shape::MESH_2D)) {
            primQueueGenName = "CcuReduceScatterMeshMem2Mem2D";
            return SelectorStatus::MATCH;
        } else {
            HCCL_WARNING("[Algo][ReduceScatterAutoSelector] algo[%u] is not supported yet for ccu_schedule mode, reset to default.", levle0Algo);
            return SelectorStatus::NOT_MATCH;
        }
    }
}

SelectorStatus ReduceScatterAutoSelector::SelectAicpuAlgo(const TopoInfo &topoInfo,
                                                      const CollAlgOperator &op,
                                                      const std::map<OpType, std::vector<HcclAlgoType>> &configAlgMap,
                                                      std::string &primQueueGenName) const
{
    (void) topoInfo;
    std::vector<HcclAlgoType> algos = std::vector<HcclAlgoType>(HCCL_ALGO_LEVEL_NUM, HcclAlgoType::HCCL_ALGO_TYPE_DEFAULT);
    auto it = configAlgMap.find(op.opType);
    if ((it != configAlgMap.end()) && (it->second.size() > 1)) {
        algos = it->second;
        if(algos[0] != HcclAlgoType::HCCL_ALGO_TYPE_NHR && algos[1] != HcclAlgoType::HCCL_ALGO_TYPE_NHR) {
            HCCL_WARNING("[Algo][ReduceScatterAutoSelector] algo[%u] is not supported yet, reset to default.", algos[0]);
        }
    }
    HCCL_INFO("hccl algo op config: config opType:%s, level0:%u, level1:%u, level2:%u, level3:%u",
        op.opType.Describe().c_str(), algos[0], algos[1], algos[2], algos[3]);
    if (topoInfo.levelNum > 1) {
        if (topoInfo.Level1Nhr) {
            primQueueGenName = "InsReduceScatterNHR";
        } else if (topoInfo.Level0Nhr) {
            primQueueGenName = "InsReduceScatterParallelNHRNHR";
        } else if (topoInfo.level0Shape == Level0Shape::MESH_1D) {
            primQueueGenName = "InsReduceScatterParallelMesh1DNHR";
        } else if (topoInfo.level0Shape == Level0Shape::MESH_2D) {
            primQueueGenName = "InsReduceScatterParallelMesh2DNHR";
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

SelectorStatus ReduceScatterAutoSelector::SelectMeshAlgoAicpu(const TopoInfo &topoInfo,
                                                          const CollAlgOperator &op,
                                                          std::string &primQueueGenName) const
{
    if (topoInfo.level0Shape == Level0Shape::MESH_1D){
        if (op.dataType == DataType::INT64 || op.dataType == DataType::UINT64 ||
            op.dataType == DataType::FP64) {
            primQueueGenName = "InsReduceScatterAicpuReduce";
        } else {
            double ratio;  // 以8卡为基线确定ratio，用来表示不同卡数对下发的影响系数
            if (rankSize_ == 0) {
                HCCL_WARNING("[ReduceScatterAutoSelector]the selector is not set rankSize_");
                ratio = 1;
            } else {
                ratio = (8.0 / rankSize_) * (8.0 / rankSize_);
            }

            if (dataSize_ * ratio > RS_AICPU_1D_MAX_DATA_SIZE) {
                primQueueGenName = "InsReduceScatterMesh1DMeshChunk";
            } else {
                primQueueGenName = "InsReduceScatterMesh1D";
            }
        }
    } else if (topoInfo.level0Shape == Level0Shape::MESH_2D) {
        if (op.dataType == DataType::INT64 || op.dataType == DataType::UINT64 ||
            op.dataType == DataType::FP64) {
            primQueueGenName = "InsReduceScatterAicpuReduceMesh2D";
        } else {
            primQueueGenName = "InsReduceScatterMesh2D";
        }
    } else {
        HCCL_WARNING("[ReduceScatterAutoSelector] topo not match");
        return SelectorStatus::NOT_MATCH;
    }
    return SelectorStatus::MATCH;
}

SelectorStatus ReduceScatterAutoSelector::SelectAivAlgo(const TopoInfo &topoInfo,
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
        HCCL_WARNING("[Algo][ReduceScatterAutoSelector] ReduceOp[%s] is not supported yet for aiv mode.",
            op.reduceOp.Describe().c_str()),
        SelectorStatus::NOT_MATCH);

    if (op.dataType == DataType::INT64 || op.dataType == DataType::UINT64 || op.dataType == DataType::FP64) {
        HCCL_WARNING("[Algo][ReduceScatterAutoSelector] aiv mode not support INT64, UINT64, FP64.");
        return SelectorStatus::NOT_MATCH;
    }

    if (topoInfo.level0Shape == Level0Shape::MESH_1D && topoInfo.levelNum <= 1) {
        primQueueGenName = "AivReduceScatterMesh1D";
    } else {
        HCCL_WARNING("[ReduceScatterAutoSelector] topo not match for aiv algo");
        return  SelectorStatus::NOT_MATCH;
    }
    return SelectorStatus::MATCH;
}

REGISTER_SELECTOR_BY_OPTYPE(OpType::REDUCESCATTER, 18, ReduceScatterAutoSelector);
} // namespace Hccl

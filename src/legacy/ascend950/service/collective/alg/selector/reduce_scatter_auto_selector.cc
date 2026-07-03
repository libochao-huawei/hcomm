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

namespace Hccl {
constexpr u64 RS_M2M_1D_MAX_DATA_SIZE = 8 * 1024 * 1024;
constexpr u64 RS_AICPU_1D_MAX_DATA_SIZE = 32 * 1024 * 1024;


SelectorStatus ReduceScatterAutoSelector::SelectCcuMsAlgo(const TopoInfo &topoInfo,
                                                    const CollAlgOperator &op,
                                                    const std::map<OpType, std::vector<HcclAlgoType>> &configAlgMap,
                                                    std::string &primQueueGenName) const
{
    (void)configAlgMap;
    if (topoInfo.levelNum > 1) {
        HCCL_WARNING("[Algo][ReduceScatterAutoSelector] levelNum > 1 is not supported yet for ccu_ms mode.");
        return SelectorStatus::NOT_MATCH;
    }
    u32 rankSize_2P = 2;
 	u32 rankSize_4P = 4; 
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

    CHK_PRT_RET(op.dataType == DataType::INT64 || op.dataType == DataType::UINT64 || op.dataType == DataType::FP64,
        HCCL_WARNING("[Algo][ReduceScatterAutoSelector] ccu_ms mode not support INT64, UINT64, FP64."),
        SelectorStatus::NOT_MATCH);

    HcclDetourType detourType = EnvConfig::GetInstance().GetDetourConfig().GetDetourType();
    CHK_PRT_RET((detourType == HcclDetourType::HCCL_DETOUR_ENABLE_2P && rankSize_ != rankSize_2P)||
        (detourType == HcclDetourType::HCCL_DETOUR_ENABLE_4P && rankSize_ != rankSize_4P),
        HCCL_WARNING("[Algo][ReduceScatterAutoSelector] detourType not match for rankSize."),
        SelectorStatus::NOT_MATCH);

    CHK_PRT_RET(detourType == HcclDetourType::HCCL_DETOUR_ENABLE_2P_AND_4P,
        HCCL_WARNING("[Algo][ReduceScatterAutoSelector] HCCL_DETOUR_ENABLE_2P_AND_4P is not supported yet."),
        SelectorStatus::NOT_MATCH);

    if (topoInfo.levelNum > 1) {
        HCCL_WARNING("[Algo][ReduceScatterAutoSelector] levelNum > 1 is not supported yet for ccu_ms mode.");
        return SelectorStatus::NOT_MATCH;
    } else {
        if (topoInfo.level0Shape == Level0Shape::MESH_1D) {
            if (IsInputOutputOverlap(op.inputMem, op.outputMem) == true) {
                // 不支持 inplace 场景
                return SelectorStatus::NOT_MATCH;
            }
            if (Is2DieFullMesh()) {
                primQueueGenName = "CcuReduceScatterMesh1D2Die";
            } else if ((detourType == HcclDetourType::HCCL_DETOUR_ENABLE_2P && rankSize_ == rankSize_2P)||
                (detourType == HcclDetourType::HCCL_DETOUR_ENABLE_4P && rankSize_ == rankSize_4P)) {
                primQueueGenName = "CcuReduceScatterMeshDetour1D";
            } else {
                primQueueGenName = "CcuReduceScatterMesh1D";
            }
        } else if (topoInfo.level0Shape == Level0Shape::MESH_2D) {
            primQueueGenName = "CcuReduceScatterMesh2D";
        } else if (topoInfo.level0Shape == Level0Shape::MESH_1D_CLOS) {
            if (IsLayerAllConnetedWithTopo(topoInfo, 0, TopoType::MESH_1D)) {
                // MESH_1D 即可链接所有卡， 使用 MESH_1D 算法
                if ((detourType == HcclDetourType::HCCL_DETOUR_ENABLE_2P && rankSize_ == rankSize_2P)||
                    (detourType == HcclDetourType::HCCL_DETOUR_ENABLE_4P && rankSize_ == rankSize_4P)) {
                    primQueueGenName = "CcuReduceScatterMeshDetour1D";
                } else {
                    primQueueGenName = "CcuReduceScatterMesh1D";
                }
            } else if (topoInfo.level0PcieMix) {
                HCCL_WARNING("[Algo][ReduceScatterAutoSelector] level0 PCIE mix is not supported yet for ccu_ms mode.");
                return SelectorStatus::NOT_MATCH;
            } else { // MS 不支持
                HCCL_WARNING("[Algo][ReduceScatterAutoSelector] level0Shape[%d] is not supported yet for ccu_ms mode.",
                    topoInfo.level0Shape);
                return SelectorStatus::NOT_MATCH;
            }
        } else if (topoInfo.level0Shape == Level0Shape::CLOS) {
            HCCL_WARNING("[Algo][ReduceScatterAutoSelector] level0Shape[%d] is not supported yet for ccu_ms mode.",
                    topoInfo.level0Shape);
            return SelectorStatus::NOT_MATCH;
        } else {
            HCCL_WARNING("[Algo][ReduceScatterAutoSelector] level0Shape[%d] is not supported yet for ccu_ms mode.",
                    topoInfo.level0Shape);
            return SelectorStatus::NOT_MATCH;
        }
    }
    HCCL_INFO("[Algo][ReduceScatterAutoSelector][%s] Algo match [%s]", __func__, primQueueGenName.c_str());
    return SelectorStatus::MATCH;
}

SelectorStatus ReduceScatterAutoSelector::SelectCcuScheduleAlgo(const TopoInfo &topoInfo,
                                                    const CollAlgOperator &op,
                                                    const std::map<OpType, std::vector<HcclAlgoType>> &configAlgMap,
                                                    std::string &primQueueGenName) const
{
    (void)configAlgMap;
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
            if (topoInfo.netLayerDetails.localNetInsSizeOfLayer[0] == 1) {
                // 每框出 1 卡
                primQueueGenName = "CcuReduceScatterNHR1DMem2Mem";
            } else {
                CHK_PRT_RET(op.dataType == DataType::INT8,
                    HCCL_WARNING("[Algo][ReduceScatterAutoSelector] dataType[%s] is not supported yet for "
                                 "ccu_schedule mode with ms reduce. levelNum[%u]",
                        op.dataType.Describe().c_str(), topoInfo.levelNum),
                    SelectorStatus::NOT_MATCH);
                if(IsSmallDataCCU((dataSize_ * rankSize_), rankSize_)){
                    primQueueGenName = "CcuReduceScatterParallelMesh1DNHR";//64M以下跑ccu
                } else {
                    return SelectorStatus::NOT_MATCH;//64M以上切为aicpu
                }
            }
        } else {
            HCCL_WARNING("[Algo][ReduceScatterAutoSelector] level0Shape[%d] is not supported yet for ccu schedule mode.",
                topoInfo.level0Shape);
            return SelectorStatus::NOT_MATCH;
        }
    } else {
        if (topoInfo.level0Shape == Level0Shape::MESH_1D) {
            if (IsInputOutputOverlap(op.inputMem, op.outputMem) == true) {
                // 不支持 inplace 场景
                return SelectorStatus::NOT_MATCH;
            }
            CHK_PRT_RET(op.dataType == DataType::INT8,
                HCCL_WARNING("[Algo][ReduceScatterAutoSelector] dataType[%s] is not supported yet for "
                             "ccu_schedule mode with ms reduce.",
                    op.dataType.Describe().c_str()),
                SelectorStatus::NOT_MATCH);
            double ratio; // 以8卡为基线确定ratio，用来表示不同卡数对下发的影响系数
            if (rankSize_ == 0) {
                HCCL_WARNING("[ReduceScatterAutoSelector]the selector is not set RankSize_]");
                ratio = 1;
            } else {
                ratio = DEFAULT_RANK_SIZE / rankSize_;
            }
            if (dataSize_ * ratio >= RS_M2M_1D_MAX_DATA_SIZE) {
                return SelectorStatus::NOT_MATCH;
            }
            primQueueGenName = "CcuReduceScatterMeshMem2Mem1D";
        } else if (topoInfo.level0Shape == Level0Shape::MESH_2D) {
            primQueueGenName = "CcuReduceScatterMeshMem2Mem2D";
        } else if (topoInfo.level0Shape == Level0Shape::MESH_1D_CLOS) {
            if (IsLayerAllConnetedWithTopo(topoInfo, 0, TopoType::MESH_1D)) {
                // MESH_1D 即可链接所有卡， 使用 MESH_1D 算法
                CHK_PRT_RET(op.dataType == DataType::INT8,
                HCCL_WARNING("[Algo][ReduceScatterAutoSelector] dataType[%s] is not supported yet for "
                             "ccu_schedule mode with ms reduce.",
                    op.dataType.Describe().c_str()),
                SelectorStatus::NOT_MATCH);
                double ratio; // 以8卡为基线确定ratio，用来表示不同卡数对下发的影响系数
                if (rankSize_ == 0) {
                    HCCL_WARNING("[ReduceScatterAutoSelector]the selector is not set RankSize_]");
                    ratio = 1;
                } else {
                    ratio = DEFAULT_RANK_SIZE / rankSize_;
                }
                if (dataSize_ * ratio >= RS_M2M_1D_MAX_DATA_SIZE) {
                    return SelectorStatus::NOT_MATCH;
                }
                primQueueGenName = "CcuReduceScatterMeshMem2Mem1D";
            } else if (topoInfo.level0PcieMix) {
                HCCL_WARNING("[Algo][ReduceScatterAutoSelector] level0 PCIE mix is not supported yet for ccu schedule mode.");
                return SelectorStatus::NOT_MATCH;
            } else {
                if(IsSmallDataCCU(dataSize_, rankSize_)){
                    primQueueGenName = "CcuReduceScatterParallelMesh1DNHR";//64M以下跑ccu
                } else {
                    return SelectorStatus::NOT_MATCH;//64M以上切为aicpu
                }
            }
        } else if (topoInfo.level0Shape == Level0Shape::CLOS) {
            HCCL_WARNING("[Algo][ReduceScatterAutoSelector] level0Shape[%d] is not supported yet for ccu schedule mode.",
                topoInfo.level0Shape);
            return SelectorStatus::NOT_MATCH;
        } else {
            HCCL_WARNING("[Algo][ReduceScatterAutoSelector] level0Shape[%d] is not supported yet for ccu schedule mode.",
                topoInfo.level0Shape);
            return SelectorStatus::NOT_MATCH;
        }
    }

    HCCL_INFO("[Algo][ReduceScatterAutoSelector][%s] Algo match [%s]", __func__, primQueueGenName.c_str());
    return SelectorStatus::MATCH;
}

SelectorStatus ReduceScatterAutoSelector::SelectAicpuAlgo(const TopoInfo &topoInfo,
                                                      const CollAlgOperator &op,
                                                      const std::map<OpType, std::vector<HcclAlgoType>> &configAlgMap,
                                                      std::string &primQueueGenName) const
{
    (void)configAlgMap;
    HCCL_DEBUG("[ReduceScatterAutoSelector][%s] start, topoInfo levelNum[%u]", __func__, topoInfo.levelNum);

    if (topoInfo.levelNum > 1) {
        if (op.dataType == DataType::INT64 || op.dataType == DataType::UINT64 || op.dataType == DataType::FP64 || op.reduceOp == ReduceOp::PROD) {
            HCCL_ERROR("[SelectAicpuAlgo] INT64, UINT64, FP64 and reduceop::prod only support in-box fullmesh algo type now.");
            return SelectorStatus::NOT_MATCH;
        }
        if (topoInfo.Level1Nhr) {
            primQueueGenName = "InsReduceScatterNHR";
        } else if (topoInfo.Level0Nhr) {
            primQueueGenName = "InsReduceScatterParallelNHRNHR";
        } else if (topoInfo.level0Shape == Level0Shape::MESH_1D) {
            if (topoInfo.netLayerDetails.localNetInsSizeOfLayer[0] == 1) {
                primQueueGenName = "InsReduceScatterNHR";
            } else {
                primQueueGenName = "InsReduceScatterParallelMesh1DNHR";
            }
        } else if (topoInfo.level0Shape == Level0Shape::MESH_2D) {
            primQueueGenName = "InsReduceScatterParallelMesh2DNHR";
        } else if (topoInfo.level0Shape == Level0Shape::CLOS) {
            primQueueGenName = "InsReduceScatterParallelNHRNHR";
        } else {
            return SelectorStatus::NOT_MATCH;
        }
    } else {
        if (topoInfo.level0Shape == Level0Shape::MESH_1D) {
            if (op.dataType == DataType::INT64 || op.dataType == DataType::UINT64 ||
                op.dataType == DataType::FP64 || op.reduceOp == ReduceOp::PROD) {
                primQueueGenName = "InsReduceScatterAicpuReduce";
            } else {
                double ratio; // 以8卡为基线确定ratio，用来表示不同卡数对下发的影响系数
                if (rankSize_ == 0) {
                    HCCL_WARNING("[ReduceScatterAutoSelector]the selector is not set RankSize_]");
                    ratio = 1;
                } else {
                    ratio = (DEFAULT_RANK_SIZE / rankSize_) * (DEFAULT_RANK_SIZE / rankSize_);
                }
                if (dataSize_ * ratio > RS_AICPU_1D_MAX_DATA_SIZE) {
                    primQueueGenName = "InsReduceScatterMesh1DMeshChunk";
                } else {
                    primQueueGenName = "InsReduceScatterMesh1D";
                }
            }
        } else if (topoInfo.level0Shape == Level0Shape::MESH_2D) {
            if (op.dataType == DataType::INT64 || op.dataType == DataType::UINT64 ||
                op.dataType == DataType::FP64 || op.reduceOp == ReduceOp::PROD) {
                primQueueGenName = "InsReduceScatterAicpuReduceMesh2D";
            } else {
                primQueueGenName = "InsReduceScatterMesh2D";
            }
        } else if (topoInfo.level0Shape == Level0Shape::MESH_1D_CLOS) {
            if (IsLayerAllConnetedWithTopo(topoInfo, 0, TopoType::MESH_1D)) {
                // MESH_1D 即可链接所有卡， 使用 MESH_1D 算法
                if (op.dataType == DataType::INT64 || op.dataType == DataType::UINT64 ||
                    op.dataType == DataType::FP64 || op.reduceOp == ReduceOp::PROD) {
                    primQueueGenName = "InsReduceScatterAicpuReduce";
                } else {
                    double ratio; // 以8卡为基线确定ratio，用来表示不同卡数对下发的影响系数
                    if (rankSize_ == 0) {
                        HCCL_WARNING("[ReduceScatterAutoSelector]the selector is not set RankSize_]");
                        ratio = 1;
                    } else {
                        ratio = (DEFAULT_RANK_SIZE / rankSize_) * (DEFAULT_RANK_SIZE / rankSize_);
                    }
                    if (dataSize_ * ratio > RS_AICPU_1D_MAX_DATA_SIZE) {
                        primQueueGenName = "InsReduceScatterMesh1DMeshChunk";
                    } else {
                        primQueueGenName = "InsReduceScatterMesh1D";
                    }
                }
            } else {
                if (op.dataType == DataType::INT64 || op.dataType == DataType::UINT64 ||
                    op.dataType == DataType::FP64 || op.reduceOp == ReduceOp::PROD) {
                    HCCL_ERROR("[SelectAicpuAlgo] level0Shape[%d], DataType[%s], reduceOp[%s] is not supported yet.",
                        topoInfo.level0Shape,
                        op.dataType.Describe().c_str(),
                        op.reduceOp.Describe().c_str());
                    return SelectorStatus::NOT_MATCH;
                } else if (topoInfo.level0PcieMix) {
                    // 预留PCIE mix入口，如果要更新算法可以直接改
                    primQueueGenName = "InsReduceScatterParallelMesh1DNHRPcie";
                } else {
                    primQueueGenName = "InsReduceScatterParallelMesh1DNHR";
                }
            }
        } else if (topoInfo.level0Shape == Level0Shape::CLOS) {
            if (op.dataType == DataType::INT64 || op.dataType == DataType::UINT64 ||
                op.dataType == DataType::FP64 || op.reduceOp == ReduceOp::PROD) {
                HCCL_ERROR("[SelectAicpuAlgo] level0Shape[%d], DataType[%s], reduceOp[%s] is not supported yet.",	 
                    topoInfo.level0Shape, 
                    op.dataType.Describe().c_str(), 
                    op.reduceOp.Describe().c_str()); 
                return SelectorStatus::NOT_MATCH;
            } else {
                primQueueGenName = "InsReduceScatterNHR";
            }
        } else {
            HCCL_WARNING("[ReduceScatterAutoSelector] topo not match");
            return SelectorStatus::NOT_MATCH;
        }
    }
    return SelectorStatus::MATCH;
}

SelectorStatus ReduceScatterAutoSelector::SelectAivAlgo(const TopoInfo &topoInfo,
                                                       const CollAlgOperator &op,
                                                       const std::map<OpType, std::vector<HcclAlgoType>> &configAlgMap,
                                                       std::string &primQueueGenName) const
{
    (void)topoInfo;
    (void)configAlgMap;
    //aiv 模式不支持 PROD
    CHK_PRT_RET(op.reduceOp == ReduceOp::PROD,
        HCCL_WARNING("[Algo][ReduceScatterAutoSelector] ReduceOp[%s] is not supported yet for aiv mode.",
            op.reduceOp.Describe().c_str()),
        SelectorStatus::NOT_MATCH);

    if (op.dataType == DataType::UINT64 || op.dataType == DataType::FP64) {
        HCCL_WARNING("[Algo][ReduceScatterAutoSelector] aiv mode not support INT64, UINT64, FP64.");
        return SelectorStatus::NOT_MATCH;
    }

    // aiv 直接走打平 mesh
    primQueueGenName = "AivReduceScatterMesh1D";

    HCCL_INFO("[Algo][ReduceScatterAutoSelector][%s] Algo match [%s]", __func__, primQueueGenName.c_str());
    return SelectorStatus::MATCH;
}

REGISTER_SELECTOR_BY_OPTYPE(OpType::REDUCESCATTER, 18, ReduceScatterAutoSelector);
} // namespace Hccl

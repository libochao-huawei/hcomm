/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: broadcast 自适应算法选择实现
 * Author: libiaozhi
 * Create: 2025-03-22
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
    HCCL_DEBUG("[BroadcastAutoSelector][%s] start, topoInfo levelNum[%u]", __func__, topoInfo.levelNum);

    if (topoInfo.levelNum > 1) {
        HCCL_WARNING("[Algo][BroadcastAutoSelector] levelNum > 1 is not supported yet for ccu_ms mode.");
        return SelectorStatus::NOT_MATCH;
    } else {
        if (topoInfo.level0Shape == Level0Shape::MESH_1D) {
            if (Is2DieFullMesh()) {
                HCCL_WARNING("[Algo][BroadcastAutoSelector] 2DieFullMesh is not supported yet for ccu_ms mode.");
                return SelectorStatus::NOT_MATCH;
            } else {
                primQueueGenName = "CcuBroadcastMesh1D";
            }
        } else if (topoInfo.level0Shape == Level0Shape::MESH_2D) {
            primQueueGenName = "CcuBroadcastMesh2D";
        } else if (topoInfo.level0Shape == Level0Shape::MESH_1D_CLOS) {
            if (IsLayerAllConnetedWithTopo(topoInfo, 0, TopoType::MESH_1D)) {
                // MESH_1D 即可链接所有卡， 使用 MESH_1D 算法
                primQueueGenName = "CcuBroadcastMesh1D";
            } else { // MS 不支持
                HCCL_WARNING("[Algo][BroadcastAutoSelector] level0Shape[%d] is not supported yet for ccu_ms mode.",
                    topoInfo.level0Shape);
                return SelectorStatus::NOT_MATCH;
            }
        } else if (topoInfo.level0Shape == Level0Shape::CLOS) {
            HCCL_WARNING("[Algo][BroadcastAutoSelector] level0Shape[%d] is not supported yet for ccu_ms mode.",
                    topoInfo.level0Shape);
            return SelectorStatus::NOT_MATCH;
        } else {
            HCCL_WARNING("[Algo][BroadcastAutoSelector] level0Shape[%d] is not supported yet for ccu_ms mode.",
                    topoInfo.level0Shape);
            return SelectorStatus::NOT_MATCH;
        }
    }
    HCCL_INFO("[Algo][BroadcastAutoSelector][%s] Algo match [%s]", __func__, primQueueGenName.c_str());
    return SelectorStatus::MATCH;
}

SelectorStatus BroadcastAutoSelector::SelectCcuScheduleAlgo(const TopoInfo &topoInfo,
                                                    const CollAlgOperator &op,
                                                    const std::map<OpType, std::vector<HcclAlgoType>> &configAlgMap,
                                                    std::string &primQueueGenName) const
{
    HCCL_DEBUG("[BroadcastAutoSelector][%s] start, topoInfo levelNum[%u]", __func__, topoInfo.levelNum);

    if (topoInfo.levelNum > 1) {
        if (topoInfo.level0Shape == Level0Shape::MESH_1D) {
            if (topoInfo.netLayerDetails.localNetInsSizeOfLayer[0] == 1) {
                // 每框出 1 卡
                primQueueGenName = "CcuBroadcastNHRMem2Mem1D";
            } else if (Is2DieFullMesh()) {
                HCCL_WARNING("[Algo][BroadcastAutoSelector] 2DieFullMesh is not supported yet for schedule mode.");
                return SelectorStatus::NOT_MATCH;
            } else {
                primQueueGenName = "CcuBroadcastParallelMesh1DNHR";
            }
        } else {
            HCCL_WARNING("[Algo][BroadcastAutoSelector] level0Shape[%d] is not supported yet for ccu schedule mode.",
                topoInfo.level0Shape);
            return SelectorStatus::NOT_MATCH;
        }
    } else {
        if (topoInfo.level0Shape == Level0Shape::MESH_1D) {
            if (Is2DieFullMesh()) {
                HCCL_WARNING("[Algo][BroadcastAutoSelector] 2DieFullMesh is not supported yet for ccu schedule mode.");
                return SelectorStatus::NOT_MATCH;
            } else {
                primQueueGenName = "CcuBroadcastMeshMem2Mem1D";
            }
        } else if (topoInfo.level0Shape == Level0Shape::MESH_2D) {
            primQueueGenName = "CcuBroadcastMeshMem2Mem2D";
        } else if (topoInfo.level0Shape == Level0Shape::MESH_1D_CLOS) {
            if (IsLayerAllConnetedWithTopo(topoInfo, 0, TopoType::MESH_1D)) {
                // MESH_1D 即可链接所有卡， 使用 MESH_1D 算法
                primQueueGenName = "CcuBroadcastMeshMem2Mem1D";
            } else {
                primQueueGenName = "CcuBroadcastParallelMesh1DNHR";
            }
        } else if (topoInfo.level0Shape == Level0Shape::CLOS) {
            HCCL_WARNING("[Algo][BroadcastAutoSelector] level0Shape[%d] is not supported yet for ccu schedule mode.");
                return SelectorStatus::NOT_MATCH;
        } else {
            HCCL_WARNING("[Algo][BroadcastAutoSelector] level0Shape[%d] is not supported yet for ccu schedule mode.",
                topoInfo.level0Shape);
            return SelectorStatus::NOT_MATCH;
        }
    }
    HCCL_INFO("[Algo][BroadcastAutoSelector][%s] Algo match [%s]", __func__, primQueueGenName.c_str());
    return SelectorStatus::MATCH;
}

SelectorStatus BroadcastAutoSelector::SelectAicpuAlgo(const TopoInfo &topoInfo,
                                                      const CollAlgOperator &op,
                                                      const std::map<OpType, std::vector<HcclAlgoType>> &configAlgMap,
                                                      std::string &primQueueGenName) const
{
    HCCL_DEBUG("[BroadcastAutoSelector][%s] start, topoInfo levelNum[%u]", __func__, topoInfo.levelNum);

    if (topoInfo.levelNum > 1) {
        if (topoInfo.level0Shape == Level0Shape::MESH_1D) {
            primQueueGenName = "InsBroadcastParallelMesh1DNHR";
        } else if (topoInfo.level0Shape == Level0Shape::MESH_2D) {
            primQueueGenName = "InsBroadcastNHR";
        } else if (topoInfo.level0Shape == Level0Shape::CLOS) {
            primQueueGenName = "InsBroadcastNHR";
        } else {
            return SelectorStatus::NOT_MATCH;
        }
    } else {
        if (topoInfo.level0Shape == Level0Shape::MESH_1D) {
            if(IsSmallData(dataSize_)){
                primQueueGenName = "InsBroadcastMesh1DOneShot";
            } else {
                primQueueGenName = "InsBroadcastMesh1DTwoShot";
            }
        } else if (topoInfo.level0Shape == Level0Shape::MESH_2D) {
            primQueueGenName = "InsBroadcastMesh2DTwoShot";
        } else if (topoInfo.level0Shape == Level0Shape::MESH_1D_CLOS) {
            if (IsLayerAllConnetedWithTopo(topoInfo, 0, TopoType::MESH_1D)) {
                // MESH_1D 即可链接所有卡， 使用 MESH_1D 算法
                if(IsSmallData(dataSize_)){
                    primQueueGenName = "InsBroadcastMesh1DOneShot";
                } else {
                    primQueueGenName = "InsBroadcastMesh1DTwoShot";
                }
            } else {
                primQueueGenName = "InsBroadcastParallelMesh1DNHR";
            }
        } else if (topoInfo.level0Shape == Level0Shape::CLOS) {
            primQueueGenName = "InsBroadcastNHR";
        } else {
            HCCL_WARNING("[BroadcastAutoSelector] topo not match");
            return SelectorStatus::NOT_MATCH;
        }
    }
    HCCL_INFO("[Algo][BroadcastAutoSelector][%s] Algo match [%s]", __func__, primQueueGenName.c_str());
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
    } else if (topoInfo.level0Shape == Level0Shape::CLOS) {
        primQueueGenName = "InsBroadcastNHR";
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
    HCCL_DEBUG("[BroadcastAutoSelector][%s] start, topoInfo levelNum[%u]", __func__, topoInfo.levelNum);

    // aicpu 直接走打平 mesh
    primQueueGenName = "AivBroadcastMesh1D";

    HCCL_INFO("[Algo][BroadcastAutoSelector][%s] Algo match [%s]", __func__, primQueueGenName.c_str());
    return SelectorStatus::MATCH;
}

REGISTER_SELECTOR_BY_OPTYPE(OpType::BROADCAST, 18, BroadcastAutoSelector);
} // namespace Hccl

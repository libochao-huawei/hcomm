/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "alltoall_auto_selector.h"
#include "selector_registry.h"
#include "coll_operator.h"
#include "coll_alg_params.h"

namespace Hccl {
SelectorStatus AlltoAllAutoSelector::SelectCcuMsAlgo(const TopoInfo &topoInfo,
                                                    const CollAlgOperator &op,
                                                    const std::map<OpType, std::vector<HcclAlgoType>> &configAlgMap,
                                                    std::string &primQueueGenName) const
{
    (void)topoInfo;
    (void)op;
    (void)configAlgMap;
    (void)primQueueGenName;
    HCCL_WARNING("[Algo][AlltoAllAutoSelector] is not supported yet for ccu_ms mode, reset to default.");
    return SelectorStatus::NOT_MATCH;
}

SelectorStatus AlltoAllAutoSelector::SelectCcuScheduleAlgo(const TopoInfo &topoInfo,
                                                    const CollAlgOperator &op,
                                                    const std::map<OpType, std::vector<HcclAlgoType>> &configAlgMap,
                                                    std::string &primQueueGenName) const
{
    if (topoInfo.levelNum > 1 || topoInfo.level0Shape != Level0Shape::MESH_1D) {
        HCCL_WARNING("[Algo][AlltoAllAutoSelector] levelNum > 1 is not supported yet for ccu_schedule mode.");
        return SelectorStatus::NOT_MATCH;
    }

    HcclAlgoType levle0Algo = HcclAlgoType::HCCL_ALGO_TYPE_DEFAULT; 
    auto it = configAlgMap.find(op.opType);
    if ((it != configAlgMap.end()) && (it->second.size() > 0)) {
        levle0Algo = it->second[0];
    }
    if (IsDefaultAlg(levle0Algo) || levle0Algo ==  HcclAlgoType::HCCL_ALGO_TYPE_FULLMESH) {
        primQueueGenName = "CcuAlltoAllMesh1D";
        return SelectorStatus::MATCH;
    } else {
        HCCL_WARNING("[Algo][AlltoAllAutoSelector] algo[%u] is not supported yet for ccu_schedule mode, reset to default.", levle0Algo);
        return SelectorStatus::NOT_MATCH;
    }
}

SelectorStatus AlltoAllAutoSelector::SelectAicpuAlgo(const TopoInfo &topoInfo,
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

    if(topoInfo.levelNum > 1 || topoInfo.level0Shape == Level0Shape::MESH_1D) {
        primQueueGenName = "InsAlltoAllMesh";
    } else if (topoInfo.level0Shape == Level0Shape::MESH_2D) {
       primQueueGenName = "InsAlltoAllMesh2D";
    } else {
	    HCCL_ERROR("hccl algo no match");
        return SelectorStatus::NOT_MATCH;
    }

    return SelectorStatus::MATCH;
}

SelectorStatus AlltoAllAutoSelector::SelectAivAlgo(const TopoInfo &topoInfo,
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
        primQueueGenName = "AivAlltoAllMesh1D";
    } else {
        HCCL_WARNING("[AlltoAllAutoSelector] topo not match for aiv algo");
        return  SelectorStatus::NOT_MATCH;
    }
    return SelectorStatus::MATCH;
}

REGISTER_SELECTOR_BY_OPTYPE(OpType::ALLTOALL, 18, AlltoAllAutoSelector);
} // namespace Hccl

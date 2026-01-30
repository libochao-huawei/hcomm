/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: alltoallvc 自适应算法选择实现
 * Author: chengyueming
 * Create: 2025-09-19
 */

#include "alltoallvc_auto_selector.h"
#include "selector_registry.h"
#include "coll_operator.h"
#include "coll_alg_params.h"

namespace Hccl {

SelectorStatus AlltoAllVCAutoSelector::SelectCcuScheduleAlgo(const TopoInfo &topoInfo,
                                                             const CollAlgOperator &op,
                                                             const std::map<OpType,
                                                             std::vector<HcclAlgoType>> &configAlgMap,
                                                             std::string &primQueueGenName) const
{
    (void)topoInfo;
    (void)op;
    (void)configAlgMap;
    (void)primQueueGenName;
    HCCL_WARNING("[Algo][AllToAllVCAutoSelector] algo is not supported yet for ccu_ms mode, reset to default.");
    return SelectorStatus::NOT_MATCH;
}

SelectorStatus AlltoAllVCAutoSelector::SelectAicpuAlgo(const TopoInfo &topoInfo,
                                                      const CollAlgOperator &op,
                                                      const std::map<OpType, std::vector<HcclAlgoType>> &configAlgMap,
                                                      std::string &primQueueGenName) const
{
    HCCL_DEBUG("[AlltoAllVCAutoSelector][%s] start, topoInfo levelNum[%u]", __func__, topoInfo.levelNum);

    // aicpu 直接走打平 mesh
    primQueueGenName = "InsAlltoAllvcMesh";

    HCCL_INFO("[Algo][AlltoAllVCAutoSelector][%s] Algo match [%s]", __func__, primQueueGenName.c_str());
    return SelectorStatus::MATCH;
}

REGISTER_SELECTOR_BY_OPTYPE(OpType::ALLTOALLVC, 18, AlltoAllVCAutoSelector);
} // namespace Hccl

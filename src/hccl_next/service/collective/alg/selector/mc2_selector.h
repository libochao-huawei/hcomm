/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: MC2 selector文件头
 * Author: libiaozhi
 * Create: 2025-03-22
 */
#ifndef HCCLV2_COLL_ALG_CCU_SELECTOR
#define HCCLV2_COLL_ALG_CCU_SELECTOR

#include "base_selector.h"
#include "coll_alg_params.h"
#include "coll_operator.h"
#include "virtual_topo.h"

namespace Hccl {
class Mc2Selector : public BaseSelector {
public:
    SelectorStatus SelectDefaultCcuMsAlgo(
        const CollAlgOperator &op,const CollAlgParams &params, std::string &primQueueGenName);

    SelectorStatus SelectDefaultAicpuAlgo(
        const CollAlgOperator &op,const CollAlgParams &params, std::string &primQueueGenName);

    SelectorStatus SelectCcuMsAlgo(const CollAlgOperator &op, CollAlgParams &params, std::string &primQueueGenName);

    SelectorStatus SelectAicpuAlgo(const CollAlgOperator &op, CollAlgParams &params, std::string &primQueueGenName);

    SelectorStatus Select(const CollAlgOperator &op, CollAlgParams &params, std::string &primQueueGenName) override;
};
}  // namespace Hccl
#endif
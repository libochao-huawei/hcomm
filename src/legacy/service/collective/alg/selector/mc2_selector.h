/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
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

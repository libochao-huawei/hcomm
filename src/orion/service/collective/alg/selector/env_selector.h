/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: 环境变量selector文件头
 * Author: yinding
 * Create: 2024-02-04
 */
#ifndef HCCLV2_COLL_ALG_ENV_SELECTOR
#define HCCLV2_COLL_ALG_ENV_SELECTOR

#include "base_selector.h"
#include "coll_alg_params.h"
#include "coll_operator.h"
#include "virtual_topo.h"

namespace Hccl {
class EnvSelector : public BaseSelector {
public:
    SelectorStatus Select(const CollAlgOperator &op, CollAlgParams &params,
                          std::string &primQueueGenName) override;
};

} // namespace Hccl
#endif
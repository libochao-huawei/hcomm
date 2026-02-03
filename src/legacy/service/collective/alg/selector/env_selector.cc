/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "env_selector.h"
#include "selector_registry.h"
#include "coll_operator.h"
#include "coll_alg_params.h"

namespace Hccl {
SelectorStatus EnvSelector::Select(const CollAlgOperator &op, CollAlgParams &params,
                                   std::string &primQueueGenName)
{
    (void)op;
    (void)params;
    const char *name = std::getenv("PRIM_QUEUE_GEN_NAME");
    HCCL_DEBUG("Extract PRIM_QUEUE_GEN_NAME from env.");
    if (name == nullptr || strcmp(name, "") == 0) {
        HCCL_WARNING("env PRIM_QUEUE_GEN_NAME is not set to choose alg.");
        return SelectorStatus::NOT_MATCH;
    }
    // example of HCCL_ALGO in 1.0: "level0:NA;level1:ring"
    std::string strName(name);
    primQueueGenName = strName;
    HCCL_DEBUG("Extract PRIM_QUEUE_GEN_NAME from env is %s", primQueueGenName.c_str());
    return SelectorStatus::MATCH;
}

REGISTER_SELECTOR(16, EnvSelector);

REGISTER_SELECTOR_BY_OPTYPE(OpType::ALLREDUCE, 16, EnvSelector);
REGISTER_SELECTOR_BY_OPTYPE(OpType::BROADCAST, 16, EnvSelector);
REGISTER_SELECTOR_BY_OPTYPE(OpType::ALLGATHER, 16, EnvSelector);
REGISTER_SELECTOR_BY_OPTYPE(OpType::REDUCESCATTER, 16, EnvSelector);
REGISTER_SELECTOR_BY_OPTYPE(OpType::SEND, 16, EnvSelector);
REGISTER_SELECTOR_BY_OPTYPE(OpType::RECV, 16, EnvSelector);
REGISTER_SELECTOR_BY_OPTYPE(OpType::BARRIER, 16, EnvSelector);
REGISTER_SELECTOR_BY_OPTYPE(OpType::ALLTOALL, 16, EnvSelector);
REGISTER_SELECTOR_BY_OPTYPE(OpType::GATHER, 16, EnvSelector);
REGISTER_SELECTOR_BY_OPTYPE(OpType::SCATTER, 16, EnvSelector);
REGISTER_SELECTOR_BY_OPTYPE(OpType::ALLTOALLV, 16, EnvSelector);
REGISTER_SELECTOR_BY_OPTYPE(OpType::ALLTOALLVC, 16, EnvSelector);
REGISTER_SELECTOR_BY_OPTYPE(OpType::REDUCE, 16, EnvSelector);
REGISTER_SELECTOR_BY_OPTYPE(OpType::BATCHSENDRECV, 16, EnvSelector);
} // namespace Hccl

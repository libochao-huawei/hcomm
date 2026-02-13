/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "coll_comm_aicpu_mgr.h"

HcclResult CollCommAicpuMgr::AcquireCollCommAicpu() {
    if (collCommAicpu_ != nullptr) {
        HCCL_ERROR("collCommAicpu_ is not nullptr, can not acquire");
        return HCCL_E_INTERNAL;
    }

    collCommAicpu_ = std::make_shared<CollCommAicpu>();
    return HCCL_SUCCESS;
}

HcclResult CollCommAicpuMgr::InitAicpuIndOp(CommAicpuParam *commAicpuParam) {
    return collCommAicpu_->InitAicpuIndOp(commAicpuParam);
}
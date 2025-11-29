/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "coll_comm_mgr.h"

namespace hccl {

CollCommMgr::~CollCommMgr() {
    std::lock_guard<std::mutex> lock(mutex_);
    collComms_.clear();
}

HcclResult CollCommMgr::CreateComm(const char* rankTable, uint32_t rank, const CommConfig& config, HcclComm* comm) {
    EXCEPTION_HANDLE_BEGIN
    auto collComm = std::make_shared<CollComm>(rank, rankTable, config);
    
    CHK_RET(collComm->Init());
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        collComms_.insert(collComm);
    }
    
    *comm = static_cast<HcclComm>(collComm.get());
    EXCEPTION_HANDLE_END
    return HCCL_SUCCESS;
}
}
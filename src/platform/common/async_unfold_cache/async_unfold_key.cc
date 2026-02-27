/*
 * This program is free software, you can redistribute it and/or modify it.
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <sstream>

#include "async_unfold_key.h"

#include "log.h"

namespace hccl {
    AsyncUnfoldKey::AsyncUnfoldKey() {}

    AsyncUnfoldKey::AsyncUnfoldKey(const AsyncUnfoldKey& other) {}

    HcclResult AsyncUnfoldKey::Init() {
        return HCCL_SUCCESS;
    }

    std::string AsyncUnfoldKey::GetKeyString() const {
        std::ostringstream oss;
        return oss.str();
    }

    bool AsyncUnfoldKey::operator==(const AsyncUnfoldKey& other) const {
        return true;
    }

    const AsyncUnfoldKey& AsyncUnfoldKey::operator=(const AsyncUnfoldKey& other) {
        if (this != &other) {
            // TODO: copy key info
        }
        return *this;
    }

}; // namespace hccl
/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef CCU_LOCAL_ADDR_HPP
#define CCU_LOCAL_ADDR_HPP

#include <type_traits>
#include "ccu_types.h"
#include "ccu_variable.hpp"
#include "ccu_address.hpp"

namespace ccu {

class LocalAddr final {
public:
    explicit LocalAddr() {}

    LocalAddr(const LocalAddr& other) {
        this->handle = other.handle;
        this->addr.handle = other.addr.handle;
        this->token.handle = other.token.handle;
    }

    void operator=(LocalAddr&& other) {
        this->handle = other.handle;
        this->addr.handle = other.addr.handle;
        this->token.handle = other.token.handle;
    }

    Address addr;
    Variable token;
    CcuLocalAddrHandle handle{0};
};

} // namespace ccu

#endif // CCU_LOCAL_ADDR_HPP

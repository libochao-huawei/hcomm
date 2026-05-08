/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef CCU_REMOTE_ADDR_HPP
#define CCU_REMOTE_ADDR_HPP

#include <type_traits>
#include "ccu_types.h"
#include "ccu_variable.hpp"
#include "ccu_address.hpp"

namespace ccu {

class RemoteAddr final {
public:
    explicit RemoteAddr() {}

    RemoteAddr(const RemoteAddr& other) {
        this->handle = other.handle;
        this->addr.handle = other.addr.handle;
        this->token.handle = other.token.handle;
    }
    void operator=(const RemoteAddr& other) {
        this->addr = other.addr;     
        this->token = other.token;   
    }
    void operator=(RemoteAddr&& other) {
        this->handle = other.handle;
        this->addr.handle = other.addr.handle;
        this->token.handle = other.token.handle;
    }

    Address addr;
    Variable token;
    CcuRemoteAddrHandle handle{0};
};

} // namespace ccu

#endif // CCU_REMOTE_ADDR_HPP

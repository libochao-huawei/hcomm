/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef CCU_EVENT_HPP
#define CCU_EVENT_HPP

#include <cstdint>
#include <type_traits>

#include "ccu_types.h"

class CcuEvent final {
public:
    explicit CcuEvent() {}

    CcuEvent(const CcuEvent& other) {
        this->handle = other.handle;
    }

    void operator=(CcuEvent&& other) {
        this->handle = other.handle;
    }

    void setMask(uint32_t mask) const;

    CcuEventHandle handle{0};
};

static_assert(std::is_standard_layout<CcuEvent>::value,
    "CcuEvent must be standard layout for .so ABI stability");
static_assert(sizeof(CcuEvent) == sizeof(CcuEventHandle),
    "CcuEvent layout changed - will break .so ABI!");

extern "C" CcuResult CcuSetEventMask(CcuEvent event, uint32_t mask);

inline void CcuEvent::setMask(uint32_t mask) const {
    auto ret = CcuSetEventMask(*this, mask);
    if (ret != CcuResult::CCU_SUCCESS) {
        throw "todo: failed";
    }
}

#endif // CCU_EVENT_HPP

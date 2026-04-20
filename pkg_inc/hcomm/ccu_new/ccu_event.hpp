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
#include "ccu_data_api_impl.h"

 class CcuEvent;
 
 class CcuEventMask {
 public:
     explicit CcuEventMask(CcuEventHandle* owner) : ownerHandle_(owner) {}
     void operator=(uint32_t newMask) const{
        auto ret = CcuSetMask(*ownerHandle_, newMask);
        if (ret != CcuResult::CCU_SUCCESS) {
            throw "todo: failed";
        }
    }
 private:
     CcuEventHandle* ownerHandle_;
 };
 
 class CcuEvent final {
 public:
     explicit CcuEvent() : mask(&handle) {}
 
     CcuEvent(const CcuEvent& other) : handle(other.handle), mask(&handle) {}
 
     void operator=(CcuEvent&& other) {
         this->handle = other.handle;
     }
 
     void setMask(uint32_t mask) const {
        auto ret = CcuSetMask(this->handle, mask);
        if (ret != CcuResult::CCU_SUCCESS) {
            throw "todo: failed";
        }
    }
 
     CcuEventHandle handle{0};
     CcuEventMask mask;
 };

 
 #endif // CCU_EVENT_HPP
 

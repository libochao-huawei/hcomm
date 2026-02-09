/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef HCCLV2_NOTIFY_FIXED_VALUE_H
#define HCCLV2_NOTIFY_FIXED_VALUE_H
#include "orion_adapter_hccp.h"
namespace Hccl {

class NotifyFixedValue {
public:
    explicit NotifyFixedValue();
    ~NotifyFixedValue();
    NotifyFixedValue(const NotifyFixedValue &that)            = delete;
    NotifyFixedValue &operator=(const NotifyFixedValue &that) = delete;
    u64               GetAddr() const;
    u32               GetSize() const;
    void              RegisterMem(RdmaHandle rdmaHandle);
    LocMemHandle      GetMemHandle(RdmaHandle rdmaHandle);

private:
    void Free() const;

    u64                  addr{0};
    u32                  size{0};
    static constexpr u64 LARGE_PAGE_MEMORY_MIN_SIZE
        = 2 * 1024 * 1024; // 申请内存用于MR注册时需要申请大页内存（最小2*1024*1024）规避DRV BUG
    std::map<RdmaHandle, LocMemHandle> memHandles;
};
} // namespace Hccl

#endif // HCCLV2_NOTIFY_FIXED_VALUE_H

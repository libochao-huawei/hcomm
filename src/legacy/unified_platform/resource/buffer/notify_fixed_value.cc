/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "notify_fixed_value.h"
#include "dev_capability.h"
#include "orion_adapter_rts.h"
#include "rdma_handle_manager.h"
#include "log.h"
#include "exception_util.h"
#include "resources_not_exist_exception.h"
#include "local_ub_rma_buffer.h"

namespace Hccl {
constexpr u32 V82_NOTIFY_SIZE = 8;
NotifyFixedValue::NotifyFixedValue() : size(DevCapability::GetInstance().GetNotifySize())
{
    size = HrtGetDeviceType() == DevType::DEV_TYPE_950 ? V82_NOTIFY_SIZE : DevCapability::GetInstance().GetNotifySize();
    u64   notifyValueSize = LARGE_PAGE_MEMORY_MIN_SIZE; // 避免申请小页内存。最小2*1024*1024
    void *ptr             = HrtMalloc(notifyValueSize, static_cast<int>(ACL_MEM_TYPE_HIGH_BAND_WIDTH));
    u32   notifyValue     = 1; // notify值写1表示record
    HrtMemcpy(ptr, notifyValueSize, &notifyValue, size, RT_MEMCPY_HOST_TO_DEVICE);
    addr = reinterpret_cast<uintptr_t>(ptr);
}

NotifyFixedValue::~NotifyFixedValue()
{
    memHandles.clear();
    DECTOR_TRY_CATCH("NotifyFixedValue", Free());
}

u64 NotifyFixedValue::GetAddr() const
{
    return addr;
}

u32 NotifyFixedValue::GetSize() const
{
    return size;
}

void NotifyFixedValue::Free() const
{
    HrtFree(reinterpret_cast<void *>(addr));
}

void NotifyFixedValue::RegisterMem(RdmaHandle rdmaHandle)
{
    if (memHandles.find(rdmaHandle) != memHandles.end()) {
        return;
    }
    TokenIdHandle tokenIdHandle = RdmaHandleManager::GetInstance().GetTokenIdInfo(rdmaHandle).first;
    std::pair<u64, u64> alignBuf = BufAlign(addr, size);
    HrtRaUbLocMemRegParam      memRegParam(alignBuf.first, alignBuf.second, GetUbToken(), tokenIdHandle, 0);
    HrtRaUbLocalMemRegOutParam memRegOutParam = HrtRaUbLocalMemReg(rdmaHandle, memRegParam);
    memHandles[rdmaHandle]                    = memRegOutParam.handle;
}
LocMemHandle NotifyFixedValue::GetMemHandle(RdmaHandle rdmaHandle)
{
    if (memHandles.find(rdmaHandle) == memHandles.end()) {
        std::string msg = "memHandle not found for input rdmaHandle";
        MACRO_THROW(ResourcesNotExistException, msg);
    }
    return memHandles[rdmaHandle];
}
} // namespace Hccl

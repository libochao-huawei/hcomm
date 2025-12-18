/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "mem_device.h"
#include "adapter_rts.h"
#include <execinfo.h>
#include <stdlib.h>
#include <unistd.h>

namespace hccl {
DeviceMem::DeviceMem(void *ptr, u64 size, bool owner) : ptr_(ptr), size_(size), owner_(owner)
{
}

DeviceMem::DeviceMem(const DeviceMem &that) : ptr_(that.ptr()), size_(that.size_), owner_(false)
{
}

DeviceMem::DeviceMem(DeviceMem &&that) noexcept : ptr_(that.ptr()), size_(that.size_), owner_(that.owner_)
{
    that.ptr_ = nullptr;
    that.size_ = 0;
    that.owner_ = false;
}

DeviceMem::~DeviceMem()
{
    if (owner_ && ptr_) {
        HCCL_DEBUG("ptr_[%p], size_[%llu]", ptr_, size_);

        HcclResult ret = hrtFree(ptr_);
        if (ret != HCCL_SUCCESS) {
            HCCL_WARNING("hrt_free error, ret[%d]", ret);
        }
    }
}

DeviceMem DeviceMem::alloc(u64 size, bool level2Address)
{
    HcclResult ret;
    void *ptr = nullptr;
    ret = hrtMalloc(&ptr, size, level2Address);
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("[DeviceMem][Alloc]rt_malloc error, ret[%d], size[%llu Byte]", ret, size);
    }

    if (ptr == nullptr) {
        HCCL_WARNING("DeviceMem alloc ptr null");
    }

    DeviceMem mem(ptr, size, true);
    return mem;
}

HcclResult DeviceMem::alloc(DeviceMem &mem, u64 size, bool level2Address)
{
    void *ptr = nullptr;
    HcclResult ret = hrtMalloc(&ptr, size, level2Address);
    if (ret != HCCL_SUCCESS || ptr == nullptr) {
        HCCL_ERROR("[DeviceMem][Alloc]rt_malloc error, ptr is nullptr, ret[%d], size[%llu Byte]", ret, size);
        return ret;
    }
    mem = DeviceMem(ptr, size, true);
    return ret;
}

void DeviceMem::free()
{
    if (ptr_) {
        HCCL_DEBUG("free ptr_[%p], size_[%llu Byte]", ptr_, size_);
        if (owner_) {
            HcclResult ret = hrtFree(ptr_);
            if (ret != HCCL_SUCCESS) {
                HCCL_WARNING("hrt_free error, ret[%d]", ret);
            }
        }
        ptr_ = nullptr;
    }
}

DeviceMem DeviceMem::create(void *ptr, u64 size)
{
    DeviceMem mem(ptr, size, false);
    return mem;
}

DeviceMem &DeviceMem::operator=(const DeviceMem &that)
{
    if (&that != this) {
        ptr_ = that.ptr();
        size_ = that.size_;
        owner_ = false;
    }

    return *this;
}

DeviceMem DeviceMem::operator=(DeviceMem &&that)
{
    if (&that != this) {
        ptr_ = that.ptr_;
        size_ = that.size_;
        owner_ = that.owner_;
    }

    that.ptr_ = nullptr;
    that.size_ = 0;
    that.owner_ = false;

    return *this;
}

DeviceMem DeviceMem::range(u64 offset, u64 size) const
{
    DeviceMem mem;
    if (ptr_ == nullptr){
        HCCL_ERROR("DeviceMem ptr is null");
        return mem;
    }
    if ((offset + size) > size_){
        // ================== 新增代码开始 ==================
        HCCL_ERROR("DeviceMem request range[%llu] is out of size_[%llu]", offset+size, size_);
        
        // 定义最大栈深度，例如 32 层
        const int MAX_STACK_SIZE = 32;
        void* buffer[MAX_STACK_SIZE];
        
        // 获取当前调用栈
        int stack_depth = backtrace(buffer, MAX_STACK_SIZE);
        
        // 将地址转换为符号字符串
        char** strings = backtrace_symbols(buffer, stack_depth);
        
        if (strings != NULL) {
            HCCL_ERROR("=== Call Stack Trace Start ===");
            for (int i = 0; i < stack_depth; i++) {
                // 打印每一层堆栈信息
                HCCL_ERROR("[%02d] %s", i, strings[i]);
            }
            HCCL_ERROR("=== Call Stack Trace End ===");
            ::free(strings); // 别忘了释放内存
        }
        // ================== 新增代码结束 ==================

        return mem;
    }
    mem = DeviceMem(static_cast<void *>(static_cast<s8 *>(ptr_) + offset), size, false);
    return mem;
}
}  // namespace hccl

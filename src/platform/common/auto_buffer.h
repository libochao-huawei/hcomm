/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef AUTO_BUFFER
#define AUTO_BUFFER

#include <memory>
#include "log.h"
#include "hccl_types.h"
#include "adapter_rts_common.h"

namespace hccl {

template <typename T>
class AutoBuffer {
public:

    AutoBuffer()
    {
    }

    ~AutoBuffer()
    {
        if (devMem_ != nullptr) {
            hrtFree(devMem_);
        }
    }

    T& operator[](size_t index)
    {
        return bufferAddr_[index];
    }

    HcclResult Resize(u32 size)
    {
        if (this->capacity_ >= size) {
            this->bufferAddr_ = static_cast<T *>(this->devMem_);
            this->count_ = size;
            return HCCL_SUCCESS;
        }

        if (devMem_ != nullptr) {
            CHK_RET(hrtFree(devMem_));
            devMem_ = nullptr;
        }

        CHK_RET(hrtMalloc(&devMem_, sizeof(T) * size));
        this->capacity_ = size;
        this->count_ = size;
        this->bufferAddr_ = static_cast<T *>(this->devMem_);

        return HCCL_SUCCESS;
    }

    void Set(T *addr)
    {
        bufferAddr_ = addr;
    }

    T *Get() const
    {
        return bufferAddr_;
    }

    u32 GetCount() const
    {
        return this->count_;
    }

    T *bufferAddr_{}; // 实际地址，是外部传入的内存地址，或者内存申请的内存bufferMem_地址

private:
    void *devMem_{};
    u32 count_{};
    u32 capacity_{};
};
}
#endif // AUTO_BUFFER

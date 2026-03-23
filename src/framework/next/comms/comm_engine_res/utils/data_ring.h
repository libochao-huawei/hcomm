/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef DATA_RING_H
#define DATA_RING_H

#include "hccl_res.h"
#include "resource_entities.h"
#include <cstdint>

namespace hccl {

constexpr uint32_t DATA_RING_CAPACITY = 32768;  // 32KB = 512 * 64B

class DataRing {
public:
    DataRing() = default;
    explicit DataRing(uint32_t capacity) : capacity_(capacity) {};
    ~DataRing() {
        if (!initialized_) {
            return;
        }
        if (addr_) {
            aclrtFree(reinterpret_cast<void*>(addr_));
        }
        if (head_) {
            aclrtFree(reinterpret_cast<void*>(head_));
        }
        if (tail_) {
            aclrtFree(reinterpret_cast<void*>(tail_));
        }
    }

    HcclResult Init() {
        void* mem = nullptr;
        int32_t ret = aclrtMalloc(&mem, capacity_, ACL_MEM_MALLOC_HUGE_ONLY);
        if (ret != 0) {
            HCCL_ERROR("[DataRing::%s] aclrtMalloc for data buffer failed", __func__);
            return HCCL_E_RUNTIME;
        }
        addr_ = reinterpret_cast<uint64_t>(mem);
        void* headPtr{};
        void* tailPtr{};
        ret = aclrtMalloc(&headPtr, sizeof(uint64_t), ACL_MEM_MALLOC_HUGE_ONLY);
        if (ret != 0) {
            HCCL_ERROR("[DataRing::%s] aclrtMalloc for head failed", __func__);
            aclrtFree(mem);
            return HCCL_E_RUNTIME;
        }
        ret = aclrtMalloc(&tailPtr, sizeof(uint64_t), ACL_MEM_MALLOC_HUGE_ONLY);
        if (ret != 0) {
            HCCL_ERROR("[DataRing::%s] aclrtMalloc for tail failed", __func__);
            aclrtFree(mem);
            aclrtFree(headPtr);
            return HCCL_E_RUNTIME;
        }

        uint64_t zero = 0;
        ret = aclrtMemcpy(headPtr, sizeof(uint64_t), &zero, sizeof(uint64_t), ACL_MEMCPY_HOST_TO_DEVICE);
        if (ret != 0) {
            HCCL_ERROR("[DataRing::%s] aclrtMemcpy for head init failed", __func__);
            aclrtFree(mem);
            aclrtFree(headPtr);
            aclrtFree(tailPtr);
            return HCCL_E_RUNTIME;
        }
        ret = aclrtMemcpy(tailPtr, sizeof(uint64_t), &zero, sizeof(uint64_t), ACL_MEMCPY_HOST_TO_DEVICE);
        if (ret != 0) {
            HCCL_ERROR("[DataRing::%s] aclrtMemcpy for tail init failed", __func__);
            aclrtFree(mem);
            aclrtFree(headPtr);
            aclrtFree(tailPtr);
            return HCCL_E_RUNTIME;
        }

        head_ = reinterpret_cast<uint64_t>(headPtr);
        tail_ = reinterpret_cast<uint64_t>(tailPtr);
        initialized_ = true;
        return HCCL_SUCCESS;
    }
    // Host 侧: 从 DataRing 读取 args 到本地缓冲区
    HcclResult ReadArgs(uint64_t argsOffset, uint64_t argsSizeByte, void* dstBuf, uint64_t dstBufSize) {
        if (!initialized_) {
            return HCCL_E_PARA;
        }
        if (argsSizeByte == 0 || argsSizeByte > dstBufSize) {
            HCCL_ERROR("[DataRing::%s] invalid argsSizeByte[%llu] or dstBufSize[%llu]",
                       __func__, argsSizeByte, dstBufSize);
            return HCCL_E_PARA;
        }
        uint64_t bufPos = argsOffset % capacity_;
        void* srcPtr = reinterpret_cast<void*>(addr_ + bufPos);
        aclError err = aclrtMemcpy(dstBuf, dstBufSize, srcPtr, argsSizeByte, ACL_MEMCPY_DEVICE_TO_HOST);
        if (err != ACL_ERROR_NONE) {
            HCCL_ERROR("[DataRing::%s] aclrtMemcpy read args failed, err[%d]", __func__, err);
            return HCCL_E_RUNTIME;
        }
        return HCCL_SUCCESS;
    }

    // Host 侧: 消费完一个 entry 后推进 head
    HcclResult AdvanceHead(uint64_t argsOffset, uint64_t argsSizeByte) {
        if (!initialized_) {
            return HCCL_E_PARA;
        }
        uint64_t alignedSize = DATA_RING_ALIGN_UP(argsSizeByte, DATA_RING_ALIGNMENT);
        uint64_t newHead = argsOffset + alignedSize;
        aclError err = aclrtMemcpy(reinterpret_cast<void*>(head_), sizeof(uint64_t),
                                   &newHead, sizeof(uint64_t), ACL_MEMCPY_HOST_TO_DEVICE);
        if (err != ACL_ERROR_NONE) {
            HCCL_ERROR("[DataRing::%s] aclrtMemcpy write head failed, err[%d]", __func__, err);
            return HCCL_E_RUNTIME;
        }
        return HCCL_SUCCESS;
    }

    DataRingInfo GetDataRingInfo() {
        DataRingInfo info;
        info.addr = addr_;
        info.headIdxAddr = head_;
        info.tailIdxAddr = tail_;
        info.capacity = capacity_;
        info.reserved = 0;
        return info;
    }

private:
    bool initialized_{false};
    uint64_t addr_{0};
    uint32_t capacity_{DATA_RING_CAPACITY};
    uint64_t head_{0};
    uint64_t tail_{0};
};

} // namespace hccl

#endif // DATA_RING_H

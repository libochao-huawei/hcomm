/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "endpoint_mgr.h"
#include "hccl_common.h"
#include "ub_mem.h"
#include <algorithm>
#include "log.h"
#include "hccl/hccl_res.h"
#include "hccl_mem_v2.h"
#include "local_ub_rma_buffer_manager.h"
#include "virtual_local_rma_buffer.h"

namespace hcomm {

UbMemRegedMemMgr::UbMemRegedMemMgr()
{
    localIpcRmaBufferMgr_ = std::make_unique<LocalIpcRmaBufferMgr>();
}

// ==================== 三种注册子函数 ====================

HcclResult UbMemRegedMemMgr::RegExactDup(HcommMem mem,
                                          std::shared_ptr<Hccl::LocalIpcRmaBuffer> &existingBuffer,
                                          void **memHandle)
{
    hccl::BufferKey<uintptr_t, u64> hwKey(existingBuffer->GetAlignedAddr(),
        existingBuffer->GetAlignedSize());
    localIpcRmaBufferMgr_->AddToTree(hwKey, existingBuffer);
    *memHandle = static_cast<void *>(existingBuffer.get());
    HCCL_INFO("[%s] exact duplicate, ref++, key {%p, %llu}", __func__, mem.addr, mem.size);
    return HCCL_SUCCESS;
}

HcclResult UbMemRegedMemMgr::RegSubset(HcommMem mem, const char *memTag,
                                        std::shared_ptr<Hccl::LocalIpcRmaBuffer> &existingBuffer,
                                        void **memHandle)
{
    uintptr_t reqAddr = reinterpret_cast<uintptr_t>(mem.addr);
    hccl::BufferKey<uintptr_t, u64> realHwKey(existingBuffer->GetAlignedAddr(),
        existingBuffer->GetAlignedSize());
    localIpcRmaBufferMgr_->AddToTree(realHwKey, existingBuffer);

    auto virtBuffer = std::make_shared<Hccl::VirtualLocalIpcRmaBuffer>(existingBuffer, reqAddr, mem.size, memTag);
    allRegisteredBuffers_.push_back(virtBuffer);
    *memHandle = static_cast<void *>(virtBuffer.get());
    HCCL_INFO("[%s] virtual IPC buffer for subset {%p, %llu} within HW range [%p, %llu]",
        __func__, mem.addr, mem.size,
        reinterpret_cast<void *>(existingBuffer->GetAlignedAddr()),
        static_cast<unsigned long long>(existingBuffer->GetAlignedSize()));
    return HCCL_SUCCESS;
}

HcclResult UbMemRegedMemMgr::RegNew(HcommMem mem, const char *memTag,
                                     hccl::BufferKey<uintptr_t, u64> &origKey,
                                     void **memHandle)
{
    uintptr_t reqAddr = reinterpret_cast<uintptr_t>(mem.addr);
    std::shared_ptr<Hccl::LocalIpcRmaBuffer> localIpcRmaBuffer = nullptr;
    std::shared_ptr<Hccl::Buffer> localBufferPtr = nullptr;
    EXECEPTION_CATCH((localBufferPtr = std::make_shared<Hccl::Buffer>(reqAddr, mem.size,
        static_cast<HcclMemType>(mem.type), memTag)), return HCCL_E_PTR);
    EXECEPTION_CATCH((localIpcRmaBuffer = std::make_shared<Hccl::LocalIpcRmaBuffer>(localBufferPtr)), return HCCL_E_PTR);

    hccl::BufferKey<uintptr_t, u64> hwKey(localIpcRmaBuffer->GetAlignedAddr(),
        localIpcRmaBuffer->GetAlignedSize());
    auto resultPair = localIpcRmaBufferMgr_->Add(hwKey, localIpcRmaBuffer);
    if (resultPair.first == localIpcRmaBufferMgr_->End()) {
        HCCL_ERROR("[%s] HW range overlaps with registered memory", __func__);
        return HCCL_E_INTERNAL;
    }

    std::shared_ptr<Hccl::LocalIpcRmaBuffer> &localBuffer = resultPair.first->second.buffer;
    CHK_SMART_PTR_NULL(localBuffer);
    *memHandle = static_cast<void *>(localBuffer.get());

    if (!resultPair.second) {
        HCCL_INFO("[%s] HW range already registered (unexpected), ref++.", __func__);
        return HCCL_SUCCESS;
    }

    allRegisteredBuffers_.push_back(localBuffer);
    HCCL_INFO("[%s] new registration! orig {%p, %llu} hw {%p, %llu}", __func__, mem.addr, mem.size,
        reinterpret_cast<void *>(localIpcRmaBuffer->GetAlignedAddr()),
        static_cast<unsigned long long>(localIpcRmaBuffer->GetAlignedSize()));
    return HCCL_SUCCESS;
}

// ==================== RegisterMemory 主入口 ====================

HcclResult UbMemRegedMemMgr::RegisterMemory(HcommMem mem, const char *memTag, void **memHandle)
{
    HCCL_INFO("[%s] Begin", __func__);
    CHK_PTR_NULL(localIpcRmaBufferMgr_);
    CHK_PTR_NULL(memHandle);
    CHK_PTR_NULL(mem.addr);
    CHK_PRT_RET(mem.size == 0, HCCL_ERROR("[%s] mem size is zero", __func__), HCCL_E_PARA);
    CHK_PRT_RET(mem.type == COMM_MEM_TYPE_INVALID,
        HCCL_ERROR("[%s] invalid mem type [%d]", __func__, mem.type), HCCL_E_PARA);

    uintptr_t reqAddr = reinterpret_cast<uintptr_t>(mem.addr);
    hccl::BufferKey<uintptr_t, u64> origKey(reqAddr, mem.size);
    auto findPair = localIpcRmaBufferMgr_->Find(origKey);

    if (findPair.first) {
        auto existingBuffer = findPair.second;
        if (existingBuffer->GetAddr() == reqAddr &&
            existingBuffer->GetSize() == static_cast<size_t>(mem.size)) {
            return RegExactDup(mem, existingBuffer, memHandle);
        }
        return RegSubset(mem, memTag, existingBuffer, memHandle);
    }

    return RegNew(mem, memTag, origKey, memHandle);
}

// ==================== UnregisterMemory ====================

HcclResult UbMemRegedMemMgr::UnregisterMemory(void* memHandle)
{
    HCCL_INFO("[%s] Begin", __func__);
    CHK_PTR_NULL(localIpcRmaBufferMgr_);

    Hccl::LocalIpcRmaBuffer* buffer = static_cast<Hccl::LocalIpcRmaBuffer*>(memHandle);
    CHK_PTR_NULL(buffer);

    if (buffer->IsVirtual()) {
        auto* realBuffer = static_cast<Hccl::LocalIpcRmaBuffer*>(buffer->GetRealBuffer());
        CHK_PTR_NULL(realBuffer);
        hccl::BufferKey<uintptr_t, u64> realHwKey(realBuffer->GetAlignedAddr(),
            realBuffer->GetAlignedSize());
        bool isDeleted = false;
        EXECEPTION_CATCH(isDeleted = localIpcRmaBufferMgr_->Del(realHwKey), isDeleted = false);
        if (!isDeleted) {
            HCCL_INFO("[%s] Virtual: HW ref > 0 or already removed.", __func__);
        }
        auto it = std::find_if(allRegisteredBuffers_.begin(), allRegisteredBuffers_.end(),
                [buffer](const std::shared_ptr<Hccl::LocalIpcRmaBuffer>& ptr) { return ptr.get() == buffer; });
        if (it != allRegisteredBuffers_.end()) {
            allRegisteredBuffers_.erase(it);
        }
        HCCL_INFO("[%s] Virtual buffer unregistered, memHandle[%p]", __func__, memHandle);
        return HCCL_SUCCESS;
    }

    hccl::BufferKey<uintptr_t, u64> hwKey(buffer->GetAlignedAddr(), buffer->GetAlignedSize());
    bool isDeleted = false;
    EXECEPTION_CATCH(isDeleted = localIpcRmaBufferMgr_->Del(hwKey), return HCCL_E_NOT_FOUND);
    if (!isDeleted) {
        HCCL_INFO("[%s] Memory ref count > 0, just decrease.", __func__);
        return HCCL_SUCCESS;
    }

    auto it = std::find_if(allRegisteredBuffers_.begin(), allRegisteredBuffers_.end(),
            [buffer](const std::shared_ptr<Hccl::LocalIpcRmaBuffer>& ptr) { return ptr.get() == buffer; });
    if (it == allRegisteredBuffers_.end()) {
        HCCL_ERROR("[%s] Memory not found in vector!", __func__);
        return HCCL_E_NOT_FOUND;
    }
    allRegisteredBuffers_.erase(it);
    return HCCL_SUCCESS;
}

HcclResult UbMemRegedMemMgr::MemoryExport(const EndpointDesc endpointDesc, void *memHandle, void **memDesc, uint32_t *memDescLen)
{
    HCCL_INFO("UbMemRegedMemMgr MemoryExport is not supported.");
    return HCCL_SUCCESS;
}

HcclResult UbMemRegedMemMgr::MemoryImport(const void *memDesc, uint32_t descLen, HcommMem *outMem)
{
    HCCL_INFO("UbMemRegedMemMgr MemoryImport is not supported.");
    return HCCL_SUCCESS;
}

HcclResult UbMemRegedMemMgr::MemoryUnimport(const void *memDesc, uint32_t descLen)
{
    HCCL_INFO("UbMemRegedMemMgr MemoryUnimport is not supported.");
    return HCCL_SUCCESS;
}

HcclResult UbMemRegedMemMgr::GetAllMemHandles(void **memHandles, uint32_t *memHandleNum)
{
    HCCL_INFO("UbMemRegedMemMgr GetAllMemHandles is not supported.");
    return HCCL_SUCCESS;
}
}

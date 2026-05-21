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

namespace hcomm {

UbMemRegedMemMgr::UbMemRegedMemMgr()
{
    localIpcRmaBufferMgr_ = std::make_unique<LocalIpcRmaBufferMgr>();
}
    
HcclResult UbMemRegedMemMgr::RegisterMemory(HcommMem mem, const char *memTag, void **memHandle)
{
    HCCL_INFO("[%s] Begin", __func__);
    CHK_PTR_NULL(localIpcRmaBufferMgr_);
    CHK_PTR_NULL(memHandle);
    CHK_PTR_NULL(mem.addr);
    CHK_PRT_RET(mem.size == 0, HCCL_ERROR("[%s] mem size is zero", __func__), HCCL_E_PARA);
    CHK_PRT_RET(mem.type == COMM_MEM_TYPE_INVALID,
        HCCL_ERROR("[%s] invalid mem type [%d]", __func__, mem.type), HCCL_E_PARA);

    std::shared_ptr<Hccl::LocalIpcRmaBuffer> localIpcRmaBuffer = nullptr;
    uintptr_t addr = reinterpret_cast<uintptr_t>(mem.addr);
    u64 size = mem.size;

    hccl::BufferKey<uintptr_t, u64> tempKey(addr, size);
    auto findPair = localIpcRmaBufferMgr_->Find(tempKey);
    if (findPair.first) {
        auto existingBuffer = findPair.second;
        // 创建带调用方tag的新Buffer — 每次注册都有独立的Buffer/RmaBuffer
        std::shared_ptr<Hccl::Buffer> localBufferPtr = nullptr;
        EXECEPTION_CATCH((localBufferPtr = std::make_shared<Hccl::Buffer>(addr, size,
            static_cast<HcclMemType>(mem.type), memTag)), return HCCL_E_PTR);

        EXECEPTION_CATCH((localIpcRmaBuffer = std::make_shared<Hccl::LocalIpcRmaBuffer>(
            localBufferPtr, existingBuffer)), return HCCL_E_PTR);

        // 别名不插入树中 — 仅递增父buffer的引用计数
        hccl::BufferKey<uintptr_t, u64> parentKey(existingBuffer->GetAddr(), existingBuffer->GetSize());
        localIpcRmaBufferMgr_->AddToTree(parentKey, existingBuffer);

        // 别名仅放入vector追踪（树中只包含实际HW注册的父buffer）
        *memHandle = static_cast<void *>(localIpcRmaBuffer.get());
        allRegisteredBuffers_.push_back(localIpcRmaBuffer);
        HCCL_INFO("[%s] alias created {addr[%p], size[%llu]} tag[%s] within parent {addr[%p], size[%llu]}",
            __func__, mem.addr, mem.size, memTag,
            reinterpret_cast<void *>(existingBuffer->GetAddr()),
            static_cast<unsigned long long>(existingBuffer->GetSize()));
        return HCCL_SUCCESS;
    } else {
        // 未注册：创建新buffer并执行HW注册
        std::shared_ptr<Hccl::Buffer> localBufferPtr = nullptr;
        EXECEPTION_CATCH((localBufferPtr = std::make_shared<Hccl::Buffer>(addr, size,
            static_cast<HcclMemType>(mem.type), memTag)), return HCCL_E_PTR);
        EXECEPTION_CATCH((localIpcRmaBuffer = std::make_shared<Hccl::LocalIpcRmaBuffer>(localBufferPtr)), return HCCL_E_PTR);
    }

    // 注册到LocalIpcRmaBuffer计数器
    auto resultPair = localIpcRmaBufferMgr_->Add(tempKey, localIpcRmaBuffer);
    if (resultPair.first == localIpcRmaBufferMgr_->End()) {
        HCCL_ERROR("[%s] The memory {addr[%p], size[%llu]} overlaps with the memory that has been registered.",
            __func__, mem.addr, mem.size);
        return HCCL_E_INTERNAL;
    }

    std::shared_ptr<Hccl::LocalIpcRmaBuffer> &localBuffer = resultPair.first->second.buffer;
    CHK_SMART_PTR_NULL(localBuffer);
    *memHandle = static_cast<void *>(localBuffer.get());

    if (!resultPair.second) {
        HCCL_INFO("[%s] Memory {addr[%p], size[%llu]} is already registered, just increase the reference count.",
            __func__, mem.addr, mem.size);
        return HCCL_SUCCESS;
    }

    allRegisteredBuffers_.push_back(localBuffer);
    HCCL_INFO("[%s] Register memory {addr[%p], size[%llu]} success!", __func__, mem.addr, mem.size);
    return HCCL_SUCCESS;
}

HcclResult UbMemRegedMemMgr::UnregisterMemory(void* memHandle)
{
    HCCL_INFO("[%s] Begin", __func__);
    CHK_PTR_NULL(localIpcRmaBufferMgr_);

    Hccl::LocalIpcRmaBuffer* buffer = static_cast<Hccl::LocalIpcRmaBuffer*>(memHandle);
    CHK_PTR_NULL(buffer);

    if (buffer->IsAlias()) {
        auto parent = buffer->GetParentBuffer();
        if (parent != nullptr) {
            hccl::BufferKey<uintptr_t, u64> parentKey(parent->GetAddr(), parent->GetSize());
            bool parentDeleted = false;
            EXECEPTION_CATCH(parentDeleted = localIpcRmaBufferMgr_->Del(parentKey), return HCCL_E_NOT_FOUND);
            if (parentDeleted) {
                auto parentRaw = parent.get();
                auto it = std::find_if(allRegisteredBuffers_.begin(), allRegisteredBuffers_.end(),
                    [parentRaw](const std::shared_ptr<Hccl::LocalIpcRmaBuffer>& ptr) {
                        return ptr.get() == parentRaw;
                    });
                if (it != allRegisteredBuffers_.end()) {
                    allRegisteredBuffers_.erase(it);
                }
            }
        }

        auto it = std::find_if(allRegisteredBuffers_.begin(), allRegisteredBuffers_.end(),
            [buffer](const std::shared_ptr<Hccl::LocalIpcRmaBuffer>& ptr) {
                return ptr.get() == buffer;
            });
        if (it != allRegisteredBuffers_.end()) {
            allRegisteredBuffers_.erase(it);
        }
        HCCL_INFO("[%s] alias unregistered, memHandle[%p]", __func__, memHandle);
        return HCCL_SUCCESS;
    }

    auto bufferInfo = buffer->GetBufferInfo();
    hccl::BufferKey<uintptr_t, u64> tempKey(bufferInfo.first, bufferInfo.second);
    bool isDeleted = false;
    EXECEPTION_CATCH(isDeleted = localIpcRmaBufferMgr_->Del(tempKey), return HCCL_E_NOT_FOUND);
    if (!isDeleted) {
        HCCL_INFO("[%s] Memory reference count is larger than 0, just decrease the reference count.", __func__);
        return HCCL_SUCCESS;
    }

    auto it = std::find_if(allRegisteredBuffers_.begin(), allRegisteredBuffers_.end(),
            [buffer](const std::shared_ptr<Hccl::LocalIpcRmaBuffer>& ptr) {
                return ptr.get() == buffer;
            });

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

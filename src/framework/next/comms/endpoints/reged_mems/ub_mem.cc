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

    hccl::BufferKey<uintptr_t, u64> tempKey(reinterpret_cast<uintptr_t>(mem.addr), mem.size);
    std::shared_ptr<Hccl::LocalIpcRmaBuffer> localIpcRmaBuffer = nullptr;

    // 检查是否已注册（精确匹配或子集）
    auto findPair = localIpcRmaBufferMgr_->Find(tempKey);
    if (findPair.first) {
        localIpcRmaBuffer = findPair.second;
        CHK_SMART_PTR_NULL(localIpcRmaBuffer);

        auto parentAddr = localIpcRmaBuffer->GetAddr();
        auto parentSize = localIpcRmaBuffer->GetSize();
        hccl::BufferKey<uintptr_t, u64> parentKey(parentAddr, parentSize);

        if (tempKey == parentKey) {
            // CASE 1: 精确匹配，增加引用计数
            HCCL_INFO("[%s] Memory {addr[%p], size[%llu]} is already registered, increase ref count.",
                __func__, mem.addr, mem.size);
            localIpcRmaBufferMgr_->Add(tempKey, localIpcRmaBuffer);
            *memHandle = static_cast<void *>(localIpcRmaBuffer.get());
            return HCCL_SUCCESS;
        }

        // CASE 2: 传入内存被包含于更大的已注册区域，构造虚拟注册
        HCCL_INFO("[%s] Memory {addr[%p], size[%llu]} is within registered region {addr[%p], size[%llu]}, "
            "create virtual entry.", __func__, mem.addr, mem.size,
            reinterpret_cast<void *>(parentAddr), parentSize);

        // 将子区间加入mgr做跟踪，同时增加父区间的引用计数
        localIpcRmaBufferMgr_->AddWithoutCheck(tempKey, localIpcRmaBuffer);
        localIpcRmaBufferMgr_->AddWithoutCheck(parentKey, localIpcRmaBuffer);

        virtualRegs_.push_back(
            {localIpcRmaBuffer, reinterpret_cast<uintptr_t>(mem.addr), mem.size});
        *memHandle = static_cast<void *>(&virtualRegs_.back());
        return HCCL_SUCCESS;
    }

    // CASE 3: 未注册，创建新的LocalIpcRmaBuffer
    std::shared_ptr<Hccl::Buffer> localBufferPtr = nullptr;
    EXECEPTION_CATCH((localBufferPtr = std::make_shared<Hccl::Buffer>(reinterpret_cast<uintptr_t>(mem.addr), mem.size,
        static_cast<HcclMemType>(mem.type), memTag)), return HCCL_E_PTR);
    EXECEPTION_CATCH((localIpcRmaBuffer = std::make_shared<Hccl::LocalIpcRmaBuffer>(localBufferPtr)), return HCCL_E_PTR);

    auto resultPair = localIpcRmaBufferMgr_->Add(tempKey, localIpcRmaBuffer);
    if (resultPair.first == localIpcRmaBufferMgr_->End()) {
        HCCL_ERROR("[%s] The memory {addr[%p], size[%llu]} overlaps with registered memory.",
            __func__, mem.addr, mem.size);
        return HCCL_E_INTERNAL;
    }

    std::shared_ptr<Hccl::LocalIpcRmaBuffer> &localBuffer = resultPair.first->second.buffer;
    CHK_SMART_PTR_NULL(localBuffer);
    *memHandle = static_cast<void *>(localBuffer.get());

    allRegisteredBuffers_.push_back(localBuffer);
    HCCL_INFO("[%s] Register memory {addr[%p], size[%llu]} success!", __func__, mem.addr, mem.size);
    return HCCL_SUCCESS;
}

HcclResult UbMemRegedMemMgr::UnregisterMemory(void* memHandle)
{
    HCCL_INFO("[%s] Begin", __func__);
    CHK_PTR_NULL(localIpcRmaBufferMgr_);

    // 检查是否为虚拟注册句柄
    for (auto it = virtualRegs_.begin(); it != virtualRegs_.end(); ++it) {
        if (static_cast<void *>(&(*it)) == memHandle) {
            HCCL_INFO("[%s] Unregister virtual memory {addr[%p], size[%llu]}.",
                __func__, reinterpret_cast<void *>(it->childAddr), it->childSize);

            hccl::BufferKey<uintptr_t, u64> childKey(it->childAddr, it->childSize);
            bool childDeleted = false;
            EXECEPTION_CATCH(childDeleted = localIpcRmaBufferMgr_->Del(childKey), return HCCL_E_NOT_FOUND);

            hccl::BufferKey<uintptr_t, u64> parentKey(
                reinterpret_cast<uintptr_t>(it->parentBuffer->GetAddr()),
                it->parentBuffer->GetSize());
            bool parentDeleted = false;
            EXECEPTION_CATCH(parentDeleted = localIpcRmaBufferMgr_->Del(parentKey), return HCCL_E_NOT_FOUND);

            if (parentDeleted) {
                auto vecIt = std::find_if(allRegisteredBuffers_.begin(), allRegisteredBuffers_.end(),
                    [parent = it->parentBuffer.get()](const std::shared_ptr<Hccl::LocalIpcRmaBuffer>& ptr) {
                        return ptr.get() == parent;
                    });
                if (vecIt != allRegisteredBuffers_.end()) {
                    allRegisteredBuffers_.erase(vecIt);
                }
            }

            virtualRegs_.erase(it);
            return HCCL_SUCCESS;
        }
    }

    Hccl::LocalIpcRmaBuffer* buffer = static_cast<Hccl::LocalIpcRmaBuffer*>(memHandle);
    CHK_PTR_NULL(buffer);
    auto bufferInfo = buffer->GetBufferInfo();

    // 从LocalIpcRmaBuffer计数器删除HcclBuf
    hccl::BufferKey<uintptr_t, u64> tempKey(bufferInfo.first, bufferInfo.second);
    bool isDeleted = false;
    EXECEPTION_CATCH(isDeleted = localIpcRmaBufferMgr_->Del(tempKey), return HCCL_E_NOT_FOUND);
    // 计数器大于1时，仅减少引用计数
    if (!isDeleted) {
        HCCL_INFO("[%s] Memory reference count is larger than 0, just decrease the reference count.", __func__);
        return HCCL_SUCCESS;
    }
    // 删除vector中的LocalIpcRmaBuffer
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

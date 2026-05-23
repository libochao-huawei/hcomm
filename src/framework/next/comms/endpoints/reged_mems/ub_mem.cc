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

    // LocalIpcRmaBuffer构造函数存在注册动作，在调用该构造函数前需检查是否注册过
    hccl::BufferKey<uintptr_t, u64> tempKey(reinterpret_cast<uintptr_t>(mem.addr), mem.size);
    auto findPair = localIpcRmaBufferMgr_->Find(tempKey);

    // 构造Buffer
    std::shared_ptr<Hccl::Buffer> localBufferPtr = nullptr;
    EXECEPTION_CATCH((localBufferPtr = std::make_shared<Hccl::Buffer>(reinterpret_cast<uintptr_t>(mem.addr), mem.size,
        static_cast<HcclMemType>(mem.type), memTag)), return HCCL_E_PTR);

    if (findPair.first) {
        localIpcRmaBuffer = findPair.second;
        hccl::BufferKey<uintptr_t, u64> actualRegKey(localIpcRmaBuffer->GetAddr(),
            static_cast<uint64_t>(localIpcRmaBuffer->GetSize()));
        auto resultPair = localIpcRmaBufferMgr_->AddWithoutCheck(actualRegKey, localIpcRmaBuffer);

        EXECEPTION_CATCH((localIpcRmaBuffer =
                            std::make_shared<Hccl::LocalIpcRmaBuffer>(localBufferPtr, *findPair.second)),
            return HCCL_E_PTR);
    } else {
        EXECEPTION_CATCH((localIpcRmaBuffer = std::make_shared<Hccl::LocalIpcRmaBuffer>(localBufferPtr)), return HCCL_E_PTR);

        hccl::BufferKey<uintptr_t, u64> actualRegKey(localIpcRmaBuffer->GetAddr(),
            static_cast<uint64_t>(localIpcRmaBuffer->GetSize()));

        auto resultPair = localIpcRmaBufferMgr_->AddWithoutCheck(actualRegKey, localIpcRmaBuffer);
    }

    HCCL_INFO("[%s] Register memory {addr[%p], size[%llu]} success!", __func__, mem.addr, mem.size);

    *memHandle = static_cast<void *>(localIpcRmaBuffer.get());
    allRegisteredBuffers_.push_back(localIpcRmaBuffer);
    return HCCL_SUCCESS;
}

HcclResult UbMemRegedMemMgr::UnregisterMemory(void* memHandle)
{
    HCCL_INFO("[%s] Begin", __func__);
    CHK_PTR_NULL(localIpcRmaBufferMgr_);

    Hccl::LocalIpcRmaBuffer* buffer = static_cast<Hccl::LocalIpcRmaBuffer*>(memHandle);
    CHK_PTR_NULL(buffer);
    auto bufferInfo = buffer->GetBufferInfo();

    // IsAlias() 直接区分父子buffer：
    //   - 父buffer (IsAlias()=false): 自己的key在tree中 → Del(ownKey)
    //   - 子buffer (IsAlias()=true):  自己的key不在tree中 → 通过Find找父key做Del
    hccl::BufferKey<uintptr_t, u64> ownKey(bufferInfo.first, bufferInfo.second);
    Hccl::LocalIpcRmaBuffer* refBuffer = buffer;

    if (buffer->IsAlias()) {
        auto hwHandle = buffer->GetIpcPtr();
        auto findResult = localIpcRmaBufferMgr_->Find(ownKey);
        if (findResult.first && findResult.second->GetIpcPtr() == hwHandle) {
            // Find模糊查找，结果匹配
            refBuffer = findResult.second.get();
        } else {
            // Find模糊查找不匹配，精确查找
            for (auto& ptr : allRegisteredBuffers_) {
                if (ptr->GetIpcPtr() == hwHandle) {
                    refBuffer = ptr.get();
                    break;
                }
            }
        }
    }

    auto refBufferInfo = refBuffer->GetBufferInfo();
    hccl::BufferKey<uintptr_t, u64> tempKey(refBufferInfo.first, refBufferInfo.second);
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

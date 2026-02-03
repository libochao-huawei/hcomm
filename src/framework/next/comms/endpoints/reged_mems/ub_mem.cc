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
#include "hccl_api.h"
#include "hccl_mem_v2.h"
#include "local_ub_rma_buffer_manager.h"

namespace hcomm {

UbMemRegedMemMgr::UbMemRegedMemMgr()
{
    localIpcRmaBufferMgr_ = std::make_unique<LocalIpcRmaBufferMgr>();
}
    
HcclResult UbRegedMemMgr::RegisterMemory(HcommMem mem, const char *memTag, void **memHandle)
{
    HCCL_INFO("[%s] Begin", __FUNCTION__);
    CHK_PTR_NULL(this->localUbRmaBufferMgr_);
    CHK_PTR_NULL(memHandle);

    std::shared_ptr<Hccl::LocalUbRmaBuffer> localUbRmaBuffer = nullptr;

    // LocalUbRmaBuffer构造函数存在注册动作，在调用该构造函数前需检查是否注册过
    hccl::BufferKey<uintptr_t, u64> tempKey(reinterpret_cast<uintptr_t>(mem.addr), mem.size);
    auto findPair = localUbRmaBufferMgr_->Find(tempKey);
    if(findPair.first) {
        localUbRmaBuffer = findPair.second;
    }
    else {
        // 构造LocalUbRmaBuffer
        std::shared_ptr<Hccl::Buffer> localBufferPtr = nullptr;
        EXECEPTION_CATCH((localBufferPtr = std::make_shared<Hccl::Buffer>(reinterpret_cast<uintptr_t>(mem.addr), mem.size, mem.type, memTag)),
            return HCCL_E_PTR);

        if(strcmp(memTag, "HcclBuffer") == 0) {
            EXECEPTION_CATCH((localUbRmaBuffer = std::make_shared<Hccl::LocalUbRmaBuffer>(localBufferPtr)),
                return HCCL_E_PTR);
        }
        else {
            EXECEPTION_CATCH((localUbRmaBuffer = std::make_shared<Hccl::LocalUbRmaBuffer>(localBufferPtr, this->rdmaHandle_)),
                return HCCL_E_PTR);
        }
    }
    
    // 注册到LocalUbRmaBuffer计数器
    auto resultPair = localUbRmaBufferMgr_->Add(tempKey, localUbRmaBuffer);
    if (resultPair.first == localUbRmaBufferMgr_->End()) {
        // 若已注册内存有交叉，返回HCCL_E_INTERNAL
        HCCL_ERROR("[UbRegedMemMgr][RegisterMemory] [%s]The memory overlaps with the memory that has been registered.", __FUNCTION__);
        return HCCL_E_INTERNAL;
    }

    std::shared_ptr<Hccl::LocalUbRmaBuffer> &localBuffer = resultPair.first->second.buffer;
    CHK_SMART_PTR_NULL(localBuffer);
    *memHandle = static_cast<void *>(localBuffer.get());

    // 已注册：输入key是表中某一最相近key的全集。 返回添加该key的迭代器，及false
    // 未注册：输入key是表中某一最相近key的空集。 返回添加成功的迭代器，及true
    if (resultPair.second) {
        HCCL_INFO("[UbRegedMemMgr][RegisterMemory]Register memory success! Add key {%p, %llu}", mem.addr, mem.size);
    } else {  
        HCCL_INFO("[UbRegedMemMgr][RegisterMemory]Memory is already registered, just increase the reference count. Add key "
                "{%p, %llu}", mem.addr, mem.size);;
        return HCCL_E_AGAIN;
    }

    return HCCL_SUCCESS;
}

HcclResult UbMemRegedMemMgr::UnregisterMemory(void* memHandle)
{
    HCCL_INFO("[%s] Begin", __FUNCTION__);
    CHK_PTR_NULL(localIpcRmaBufferMgr_);

    Hccl::LocalIpcRmaBuffer* buffer = static_cast<Hccl::LocalIpcRmaBuffer*>(memHandle);

    auto bufferInfo = buffer->GetBufferInfo();

    // 从LocalIpcRmaBuffer计数器删除HcclBuf
    hccl::BufferKey<uintptr_t, u64> tempKey(bufferInfo.first, bufferInfo.second);
    bool resultPair = false;
    EXECEPTION_CATCH(resultPair = localIpcRmaBufferMgr_->Del(tempKey), return HCCL_E_NOT_FOUND);
    // 计数器大于1时，返回false，说明框架层有其它设备在使用这段内存，返回HCCL_E_AGAIN
    if (!resultPair) {
        HCCL_INFO("[UbMemRegedMemMgr][[DeregMem] Memory reference count is larger than 0"
                  "(used by other RemoteRank), do not deregister memory.");
        return HCCL_E_AGAIN;
    }
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
}
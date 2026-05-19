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
#include "urma_mem.h"
#include <algorithm>
#include "log.h"
#include "hccl/hccl_res.h"
#include "hccl_mem_v2.h"
#include "exchange_ub_buffer_dto.h"
#include "local_ub_rma_buffer_manager.h"
#include "remote_rma_buffer.h"
#include "local_ub_rma_buffer.h"
#include "virtual_local_rma_buffer.h"

namespace hcomm {

namespace {
// 与 BufAlign 保持一致的 4K 对齐：算出传入 mem 对应的硬件实际覆盖范围
inline std::pair<uintptr_t, u64> UbHwRange(uintptr_t addr, u64 size)
{
    constexpr u64 kUbPageSize = 4096;
    uintptr_t alignedAddr = addr & ~(kUbPageSize - 1);
    u64 offset = addr - alignedAddr;
    return {alignedAddr, size + offset};
}
}  // namespace

UbRegedMemMgr::UbRegedMemMgr()
{
    localUbRmaBufferMgr_ = std::make_unique<LocalUbRmaBufferMgr>();
}

HcclResult UbRegedMemMgr::RegisterMemory(HcommMem mem, const char *memTag, void **memHandle)
{
    HCCL_INFO("[%s] Begin", __FUNCTION__);
    CHK_PTR_NULL(this->localUbRmaBufferMgr_);
    CHK_PTR_NULL(memHandle);
    CHK_PTR_NULL(mem.addr);
    CHK_PRT_RET(mem.size == 0, HCCL_ERROR("[%s] mem size is zero", __func__), HCCL_E_PARA);
    CHK_PRT_RET(mem.type == COMM_MEM_TYPE_INVALID,
        HCCL_ERROR("[%s] invalid mem type [%d]", __func__, mem.type), HCCL_E_PARA);

    uintptr_t reqAddr = reinterpret_cast<uintptr_t>(mem.addr);
    // 用传入 mem 算出硬件实际覆盖的地址范围作为树 key
    auto hwRange = UbHwRange(reqAddr, mem.size);
    hccl::BufferKey<uintptr_t, u64> hwKey(hwRange.first, hwRange.second);

    // 先用原始值查（精确重复场景），再用对齐后 HW 范围查（子集场景）
    hccl::BufferKey<uintptr_t, u64> origKey(reqAddr, mem.size);
    auto findPair = localUbRmaBufferMgr_->DirectFind(origKey);
    if (!findPair.first) {
        findPair = localUbRmaBufferMgr_->DirectFind(hwKey);
    }

    if (findPair.first) {
        auto existingBuffer = findPair.second;
        if (existingBuffer->GetAddr() == reqAddr &&
            existingBuffer->GetSize() == static_cast<size_t>(mem.size)) {
            // 精确重复，引用计数 +1
            localUbRmaBufferMgr_->AddWithoutCheck(
                hccl::BufferKey<uintptr_t, u64>(existingBuffer->GetAlignedAddr(),
                    existingBuffer->GetAlignedSize()), existingBuffer);
            *memHandle = static_cast<void *>(existingBuffer.get());
            HCCL_INFO("[UbRegedMemMgr][RegisterMemory] exact duplicate, ref++, key {%p, %llu}", mem.addr, mem.size);
            return HCCL_SUCCESS;
        }
        // 子集：传入区域落在已有 HW 注册范围内 → 创建虚拟 buffer，对真实 buffer 做引用计数
        hccl::BufferKey<uintptr_t, u64> realHwKey(existingBuffer->GetAlignedAddr(),
            existingBuffer->GetAlignedSize());
        localUbRmaBufferMgr_->AddWithoutCheck(realHwKey, existingBuffer);

        auto virtBuffer = std::make_shared<Hccl::VirtualLocalUbRmaBuffer>(existingBuffer, reqAddr, mem.size, memTag);
        allRegisteredBuffers_.push_back(virtBuffer);
        *memHandle = static_cast<void *>(virtBuffer.get());
        HCCL_INFO("[UbRegedMemMgr][RegisterMemory] Created virtual UB buffer for subset {%p, %llu} "
            "within HW range [%p, %llu]", mem.addr, mem.size,
            reinterpret_cast<void *>(existingBuffer->GetAlignedAddr()),
            static_cast<unsigned long long>(existingBuffer->GetAlignedSize()));
        return HCCL_SUCCESS;
    }

    // 全新注册：构造真实 buffer
    std::shared_ptr<Hccl::LocalUbRmaBuffer> localUbRmaBuffer = nullptr;
    std::shared_ptr<Hccl::Buffer> localBufferPtr = nullptr;
    EXECEPTION_CATCH((localBufferPtr = std::make_shared<Hccl::Buffer>(reqAddr,
        mem.size, static_cast<HcclMemType>(mem.type), memTag)), return HCCL_E_PTR);
    EXECEPTION_CATCH((localUbRmaBuffer = std::make_shared<Hccl::LocalUbRmaBuffer>(
        localBufferPtr, this->rdmaHandle_)), return HCCL_E_PTR);

    // 使用硬件实际注册范围（对齐后）作为树 key
    hccl::BufferKey<uintptr_t, u64> actualHwKey(localUbRmaBuffer->GetAlignedAddr(),
        localUbRmaBuffer->GetAlignedSize());
    auto resultPair = localUbRmaBufferMgr_->AddWithoutCheck(actualHwKey, localUbRmaBuffer);

    std::shared_ptr<Hccl::LocalUbRmaBuffer> &localBuffer = resultPair.first->second.buffer;
    CHK_SMART_PTR_NULL(localBuffer);
    *memHandle = static_cast<void *>(localBuffer.get());

    if (resultPair.second) {
        HCCL_INFO("[UbRegedMemMgr][RegisterMemory] new HW registration success! "
            "orig {%p, %llu} hwKey {%p, %llu}", mem.addr, mem.size,
            reinterpret_cast<void *>(localUbRmaBuffer->GetAlignedAddr()),
            static_cast<unsigned long long>(localUbRmaBuffer->GetAlignedSize()));
    } else {
        HCCL_INFO("[UbRegedMemMgr][RegisterMemory] HW range already registered (unexpected), ref++");
        return HCCL_SUCCESS;
    }

    this->allRegisteredBuffers_.push_back(localBuffer);
    return HCCL_SUCCESS;
}

HcclResult UbRegedMemMgr::UnregisterMemory(void* memHandle)
{
    HCCL_INFO("[%s] Begin", __FUNCTION__);
    CHK_PTR_NULL(this->localUbRmaBufferMgr_);

    Hccl::LocalUbRmaBuffer* buffer = static_cast<Hccl::LocalUbRmaBuffer*>(memHandle);
    CHK_PTR_NULL(buffer);

    if (buffer->IsVirtual()) {
        auto* realBuffer = static_cast<Hccl::LocalUbRmaBuffer*>(buffer->GetRealBuffer());
        CHK_PTR_NULL(realBuffer);
        // 用真实 buffer 的硬件覆盖范围做 Del
        hccl::BufferKey<uintptr_t, u64> realHwKey(realBuffer->GetAlignedAddr(),
            realBuffer->GetAlignedSize());
        bool resultPair = false;
        EXECEPTION_CATCH(resultPair = this->localUbRmaBufferMgr_->Del(realHwKey), resultPair = false);
        if (!resultPair) {
            HCCL_INFO("[UbRegedMemMgr][UnregisterMemory] Virtual: HW ref > 0 or already removed.");
        }
        auto it = std::find_if(allRegisteredBuffers_.begin(), allRegisteredBuffers_.end(),
                [buffer](const std::shared_ptr<Hccl::LocalUbRmaBuffer>& ptr) { return ptr.get() == buffer; });
        if (it != allRegisteredBuffers_.end()) {
            allRegisteredBuffers_.erase(it);
        }
        HCCL_INFO("[UbRegedMemMgr][UnregisterMemory] Virtual buffer unregistered, memHandle[%p]", memHandle);
        return HCCL_SUCCESS;
    }

    // 真实 buffer：用硬件覆盖范围做 Del
    hccl::BufferKey<uintptr_t, u64> hwKey(buffer->GetAlignedAddr(), buffer->GetAlignedSize());
    bool resultPair = false;
    EXECEPTION_CATCH(resultPair = this->localUbRmaBufferMgr_->Del(hwKey), return HCCL_E_NOT_FOUND);
    if (!resultPair) {
        HCCL_INFO("[UbRegedMemMgr][UnregisterMemory] Memory ref count > 0 (used by other), do not deregister.");
        return HCCL_SUCCESS;
    }

    auto it = std::find_if(allRegisteredBuffers_.begin(), allRegisteredBuffers_.end(),
            [buffer](const std::shared_ptr<Hccl::LocalUbRmaBuffer>& ptr) { return ptr.get() == buffer; });
    if (it == allRegisteredBuffers_.end()) {
        HCCL_ERROR("[UbRegedMemMgr][UnregisterMemory] Memory not found in vector!");
        return HCCL_E_NOT_FOUND;
    }
    allRegisteredBuffers_.erase(it);
    return HCCL_SUCCESS;
}

HcclResult UbRegedMemMgr::GetMemDesc(const EndpointDesc endpointDesc, Hccl::LocalUbRmaBuffer *localUbRmaBuffer)
{
    auto                      dto = localUbRmaBuffer->GetExchangeDto();
    Hccl::BinaryStream        localUbRmaBufferStream;
    dto->Serialize(localUbRmaBufferStream);
    std::vector<char> tempLocalMemDesc;
    localUbRmaBufferStream.Dump(tempLocalMemDesc);
    HCCL_DEBUG("[UbRegedMemMgr][GetMemDesc] [%s] dump data size [%u]", __func__, tempLocalMemDesc.size());
    if (tempLocalMemDesc.empty()) {
        HCCL_ERROR("[UbRegedMemMgr][GetMemDesc] [%s] tempLocalMemDesc export failed.", __func__);
        return HCCL_E_INTERNAL;
    }

    std::vector<char> tempLocalEndpointDesc;
    tempLocalEndpointDesc.resize(sizeof(EndpointDesc));
    if(memcpy_s(tempLocalEndpointDesc.data(), sizeof(EndpointDesc), &endpointDesc, sizeof(EndpointDesc)) != EOK) {
        HCCL_ERROR("[UbRegedMemMgr][GetMemDesc] [%s] endpointDesc memcpy_s failed.", __func__);
        return HCCL_E_INTERNAL;
    }

    tempLocalMemDesc.insert(tempLocalMemDesc.end(),
                       tempLocalEndpointDesc.begin(),
                       tempLocalEndpointDesc.end());

    localUbRmaBuffer->Desc = std::move(tempLocalMemDesc);
    return HCCL_SUCCESS;
}

HcclResult UbRegedMemMgr::MemoryExport(const EndpointDesc endpointDesc, void *memHandle, void **memDesc, uint32_t *memDescLen)
{
    HCCL_INFO("[%s] Begin", __FUNCTION__);

    Hccl::LocalUbRmaBuffer *localUbRmaBuffer = reinterpret_cast<Hccl::LocalUbRmaBuffer *>(memHandle);

    CHK_RET(GetMemDesc(endpointDesc, localUbRmaBuffer));

    *memDescLen = static_cast<uint32_t>(localUbRmaBuffer->Desc.size());
    *memDesc = static_cast<void *>(localUbRmaBuffer->Desc.data());

    return HCCL_SUCCESS;
}

HcclResult UbRegedMemMgr::GetParamsFromMemDesc(const void *memDesc, uint32_t descLen,
                                                EndpointDesc &endpointDesc, Hccl::ExchangeUbBufferDto &dto)
{
    const char *description = static_cast<const char *>(memDesc);

    if (memcpy_s(&endpointDesc, sizeof(EndpointDesc), description + descLen - sizeof(EndpointDesc), sizeof(EndpointDesc)) != EOK) {
        HCCL_ERROR("[UbRegedMemMgr][GetParamsFromMemDesc] [%s] endpointDesc copy error. aim size:[%llu]", __func__, sizeof(EndpointDesc));
        return HCCL_E_INTERNAL;
    }

    std::vector<char> tempDesc{};
    tempDesc.resize(TRANSPORT_EMD_ESC_SIZE);
    tempDesc.assign(description, description + descLen - sizeof(EndpointDesc));
    Hccl::BinaryStream        remoteUbRmaBufferStream(tempDesc);
    dto.Deserialize(remoteUbRmaBufferStream);
    return HCCL_SUCCESS;
}

HcclResult UbRegedMemMgr::MemoryImport(const void *memDesc, uint32_t descLen, HcommMem *outMem)
{
    HCCL_INFO("[%s] Begin", __FUNCTION__);

    EndpointDesc endpointDesc;
    Hccl::ExchangeUbBufferDto dto;
    CHK_RET(GetParamsFromMemDesc(memDesc, descLen, endpointDesc, dto));

    std::shared_ptr<Hccl::RemoteUbRmaBuffer> remoteUbRmaBuffer;
    EXECEPTION_CATCH(
        remoteUbRmaBuffer = std::make_shared<Hccl::RemoteUbRmaBuffer>(this->rdmaHandle_, dto),
        return HCCL_E_PTR;
    );
    CHK_SMART_PTR_NULL(remoteUbRmaBuffer);

    if(remoteUbRmaBufferMgrs_.find(endpointDesc) == remoteUbRmaBufferMgrs_.end()) {
        std::unique_ptr<RemoteUbRmaBufferMgr> remoteUbRmaBufferMgr;
        EXECEPTION_CATCH((remoteUbRmaBufferMgr = std::make_unique<RemoteUbRmaBufferMgr>()),
            return HCCL_E_PTR);
        CHK_SMART_PTR_NULL(remoteUbRmaBufferMgr);
        remoteUbRmaBufferMgrs_[endpointDesc] = std::move(remoteUbRmaBufferMgr);
        HCCL_INFO("remoteUbRmaBufferMgrs_ add remoteUbRmaBufferMgr successfully!");
    }

    hccl::BufferKey<uintptr_t, u64> tempKey(static_cast<uintptr_t>(dto.addr), dto.size);
    auto resultPair = remoteUbRmaBufferMgrs_[endpointDesc]->Add(tempKey, remoteUbRmaBuffer);
    if(!resultPair.second) {
        HCCL_ERROR("[UbRegedMemMgr][MemoryImport] This memDesc has already been imported!");
        return HCCL_E_AGAIN;
    }

    outMem->addr   = reinterpret_cast<void *>(remoteUbRmaBuffer->GetAddr());
    outMem->size    = remoteUbRmaBuffer->GetSize();

    return HCCL_SUCCESS;
}

HcclResult UbRegedMemMgr::MemoryUnimport(const void *memDesc, uint32_t descLen)
{
    HCCL_INFO("[%s] Begin", __FUNCTION__);

    EndpointDesc endpointDesc;
    Hccl::ExchangeUbBufferDto dto;
    CHK_RET(GetParamsFromMemDesc(memDesc, descLen, endpointDesc, dto));

    if(remoteUbRmaBufferMgrs_.find(endpointDesc) == remoteUbRmaBufferMgrs_.end()) {
        HCCL_ERROR("[UrmaRegedMemMgr][MemoryUnimport] Remote buffer manager Not Found.");
        return HCCL_E_NOT_FOUND;
    }

    HCCL_INFO("[MemoryUnimport][Ub] MemoryUnimport");
    hccl::BufferKey<uintptr_t, u64> tempKey(static_cast<uintptr_t>(dto.addr), dto.size);

    bool resultPair = false;
    EXECEPTION_CATCH(resultPair = remoteUbRmaBufferMgrs_[endpointDesc]->Del(tempKey), return HCCL_E_NOT_FOUND);
    if (!resultPair) {
        HCCL_INFO("[UrmaRegedMemMgr][[MemoryUnimport] Memory reference count is larger than 0"
                    "(used by other RemoteRank).");
        return HCCL_E_AGAIN;
    }
    if (!remoteUbRmaBufferMgrs_[endpointDesc]->size()) {
        remoteUbRmaBufferMgrs_.erase(endpointDesc);
    }
    return HCCL_SUCCESS;
}

HcclResult UbRegedMemMgr::GetAllMemHandles(void **memHandles, uint32_t *memHandleNum)
{
    HCCL_INFO("[%s] Begin", __FUNCTION__);
    CHK_PTR_NULL(memHandleNum);

    uint32_t bufferCount = static_cast<uint32_t>(allRegisteredBuffers_.size());
    *memHandleNum = bufferCount;

    HCCL_INFO("[UbRegedMemMgr][[GetAllMemHandles] memHandleNum is [%d]", bufferCount);

    *memHandles = (bufferCount == 0) ? nullptr : reinterpret_cast<void *>(allRegisteredBuffers_.data());

    return HCCL_SUCCESS;
}

}

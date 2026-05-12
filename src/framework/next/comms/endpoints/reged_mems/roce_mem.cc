/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "endpoint_pair.h"
#include "hccl_common.h"
#include "hccl/hccl_res.h"
#include "log.h"
#include "roce_mem.h"
#include "endpoint.h"
#include "orion_adapter_hccp.h"
#include "hccl_mem.h"
#include "exchange_rdma_buffer_dto.h"
#include "local_rdma_rma_buffer_manager.h"
#include "local_rdma_rma_buffer.h"
#include "hccl_one_sided_data.h"

namespace hcomm {

RoceRegedMemMgr::RoceRegedMemMgr()
{
    localRdmaRmaBufferMgr_ = std::make_unique<LocalRdmaRmaBufferMgr>();
}

HcclResult RoceRegedMemMgr::RegisterMemory(HcommMem mem, const char *memTag, void **memHandle)
{
    HCCL_INFO("[%s] Begin", __FUNCTION__);
    CHK_PTR_NULL(this->localRdmaRmaBufferMgr_);
    CHK_PTR_NULL(memHandle);

    std::shared_ptr<Hccl::LocalRdmaRmaBuffer> localRdmaRmaBuffer = nullptr;

    // LocalRdmaRmaBuffer构造函数存在注册动作，在调用该构造函数前需检查是否注册过
    hccl::BufferKey<uintptr_t, u64> tempKey(reinterpret_cast<uintptr_t>(mem.addr), mem.size);
    auto findPair = localRdmaRmaBufferMgr_->Find(tempKey);
    if (findPair.first) {
        localRdmaRmaBuffer = findPair.second;

        auto parentAddr = localRdmaRmaBuffer->GetAddr();
        auto parentSize = static_cast<uint64_t>(localRdmaRmaBuffer->GetSize());
        hccl::BufferKey<uintptr_t, u64> parentKey(parentAddr, parentSize);

        if (tempKey == parentKey) {
            // CASE 1: 精确匹配，增加引用计数
            HCCL_INFO("[RoceRegedMemMgr][RegisterMemory]Memory {addr[%p], size[%llu]} already registered, increase ref count.",
                mem.addr, mem.size);
            localRdmaRmaBufferMgr_->Add(tempKey, localRdmaRmaBuffer);
            *memHandle = static_cast<void *>(localRdmaRmaBuffer.get());
            return HCCL_SUCCESS;
        }

        // CASE 2: 传入内存被包含于更大的已注册区域，构造虚拟注册
        HCCL_INFO("[RoceRegedMemMgr][RegisterMemory]Memory {addr[%p], size[%llu]} is within registered region "
            "{addr[%p], size[%llu]}, create virtual entry.",
            mem.addr, mem.size, reinterpret_cast<void *>(parentAddr), parentSize);

        localRdmaRmaBufferMgr_->AddWithoutCheck(tempKey, localRdmaRmaBuffer);
        localRdmaRmaBufferMgr_->AddWithoutCheck(parentKey, localRdmaRmaBuffer);

        virtualRegs_.push_back(
            {localRdmaRmaBuffer, reinterpret_cast<uintptr_t>(mem.addr), mem.size, {}});
        *memHandle = static_cast<void *>(&virtualRegs_.back());
        return HCCL_SUCCESS;
    }

    // CASE 3: 未注册，构造LocalRdmaRmaBuffer
    std::shared_ptr<Hccl::Buffer> localBufferPtr = nullptr;
    EXECEPTION_CATCH((localBufferPtr = std::make_shared<Hccl::Buffer>(reinterpret_cast<uintptr_t>(mem.addr),
        mem.size, static_cast<HcclMemType>(mem.type), memTag)),
        return HCCL_E_PTR);

    EXECEPTION_CATCH((localRdmaRmaBuffer = std::make_shared<Hccl::LocalRdmaRmaBuffer>(localBufferPtr, this->rdmaHandle_)),
        return HCCL_E_PTR);

    // 注册到LocalRdmaRmaBuffer计数器
    auto resultPair = localRdmaRmaBufferMgr_->Add(tempKey, localRdmaRmaBuffer);
    if (resultPair.first == localRdmaRmaBufferMgr_->End()) {
        HCCL_ERROR("[RoceRegedMemMgr][RegisterMemory] [%s]The memory overlaps with the memory that has been registered.", __FUNCTION__);
        return HCCL_E_INTERNAL;
    }

    std::shared_ptr<Hccl::LocalRdmaRmaBuffer> &localBuffer = resultPair.first->second.buffer;
    CHK_SMART_PTR_NULL(localBuffer);
    *memHandle = static_cast<void *>(localBuffer.get());

    if (resultPair.second) {
        HCCL_INFO("[RoceRegedMemMgr][RegisterMemory]Register memory success! Add key {%p, %llu}", mem.addr, mem.size);
    } else {
        HCCL_INFO("[RoceRegedMemMgr][RegisterMemory]Memory is already registered, just increase the reference count. Add key "
                "{%p, %llu}", mem.addr, mem.size);
        return HCCL_SUCCESS;
    }

    this->allRegisteredBuffers_.push_back(localBuffer);
    return HCCL_SUCCESS;
}

HcclResult RoceRegedMemMgr::UnregisterMemory(void* memHandle)
{
    HCCL_INFO("[%s] Begin", __FUNCTION__);
    CHK_PTR_NULL(this->localRdmaRmaBufferMgr_);
    CHK_PTR_NULL(memHandle);

    // 检查是否为虚拟注册句柄
    for (auto it = virtualRegs_.begin(); it != virtualRegs_.end(); ++it) {
        if (static_cast<void *>(&(*it)) == memHandle) {
            HCCL_INFO("[RoceRegedMemMgr][UnregisterMemory] Unregister virtual memory {addr[%p], size[%llu]}.",
                reinterpret_cast<void *>(it->childAddr), it->childSize);

            hccl::BufferKey<uintptr_t, u64> childKey(it->childAddr, it->childSize);
            bool childDeleted = false;
            EXECEPTION_CATCH(childDeleted = localRdmaRmaBufferMgr_->Del(childKey), return HCCL_E_NOT_FOUND);

            hccl::BufferKey<uintptr_t, u64> parentKey(
                reinterpret_cast<uintptr_t>(it->parentBuffer->GetAddr()),
                static_cast<uint64_t>(it->parentBuffer->GetSize()));
            bool parentDeleted = false;
            EXECEPTION_CATCH(parentDeleted = localRdmaRmaBufferMgr_->Del(parentKey), return HCCL_E_NOT_FOUND);

            if (parentDeleted) {
                auto vecIt = std::find_if(allRegisteredBuffers_.begin(), allRegisteredBuffers_.end(),
                    [parent = it->parentBuffer.get()](const std::shared_ptr<Hccl::LocalRdmaRmaBuffer>& ptr) {
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

    Hccl::LocalRdmaRmaBuffer* buffer = static_cast<Hccl::LocalRdmaRmaBuffer*>(memHandle);
    CHK_PTR_NULL(buffer);
    auto bufferInfo = buffer->GetBufferInfo();

    // 从LocalRamBuffer计数器删除
    hccl::BufferKey<uintptr_t, u64> tempKey(bufferInfo.first, bufferInfo.second);
    bool resultPair = false;
    EXECEPTION_CATCH(resultPair = this->localRdmaRmaBufferMgr_->Del(tempKey), return HCCL_E_NOT_FOUND);
    // 计数器大于1时，返回false，说明框架层有其它设备在使用这段内存，返回HCCL_E_AGAIN
    if (!resultPair) {
        HCCL_INFO("[RoceRegedMemMgr][[UnregisterMemory] Memory reference count is larger than 0"
                  "(used by other RemoteRank), do not deregister memory.");
        return HCCL_SUCCESS;
    }

    // 删除vector中的LocalRdmaRmaBuffer
    auto it = std::find_if(allRegisteredBuffers_.begin(), allRegisteredBuffers_.end(),
            [buffer](const std::shared_ptr<Hccl::LocalRdmaRmaBuffer>& ptr) {
                return ptr.get() == buffer;
            });

    if (it == allRegisteredBuffers_.end()) {
        HCCL_ERROR("[RoceRegedMemMgr][UnregisterMemory] Memory not found in vector!");
        return HCCL_E_NOT_FOUND;
    }

    allRegisteredBuffers_.erase(it);
    return HCCL_SUCCESS;
}

HcclResult RoceRegedMemMgr::GetMemDesc(const EndpointDesc endpointDesc, Hccl::LocalRdmaRmaBuffer *localRdmaRmaBuffer) 
{
    auto                      dto = localRdmaRmaBuffer->GetExchangeDto();
    Hccl::BinaryStream        localRdmaRmaBufferStream;
    dto->Serialize(localRdmaRmaBufferStream);
    std::vector<char> tempLocalMemDesc;
    localRdmaRmaBufferStream.Dump(tempLocalMemDesc);
    HCCL_DEBUG("[RoceRegedMemMgr][GetMemDesc] [%s] dump data size [%u]", __func__, tempLocalMemDesc.size());
    // 判断内存描述符是否正确导出
    if (tempLocalMemDesc.empty()) {
        HCCL_ERROR("[RoceRegedMemMgr][GetMemDesc] [%s] tempLocalMemDesc export failed.", __func__);
        return HCCL_E_INTERNAL;
    }

    std::vector<char> tempLocalEndpointDesc;
    tempLocalEndpointDesc.resize(sizeof(EndpointDesc));
    if(memcpy_s(tempLocalEndpointDesc.data(), sizeof(EndpointDesc), &endpointDesc, sizeof(EndpointDesc)) != EOK) {
        HCCL_ERROR("[RoceRegedMemMgr][GetMemDesc] [%s] endpointDesc memcpy_s failed.", __func__);
        return HCCL_E_INTERNAL;
    }

    tempLocalMemDesc.insert(tempLocalMemDesc.end(), 
                       tempLocalEndpointDesc.begin(), 
                       tempLocalEndpointDesc.end());

    // 内存描述符拷贝
    localRdmaRmaBuffer->Desc = std::move(tempLocalMemDesc);
    return HCCL_SUCCESS;
}

HcclResult RoceRegedMemMgr::MemoryExport(const EndpointDesc endpointDesc, void *memHandle, void **memDesc, uint32_t *memDescLen)
{
    HCCL_INFO("[%s] Begin", __FUNCTION__);
    CHK_PTR_NULL(memHandle);

    // 检查是否为虚拟注册句柄
    for (auto &virt : virtualRegs_) {
        if (static_cast<void *>(&virt) == memHandle) {
            HCCL_INFO("[RoceRegedMemMgr][MemoryExport] Export virtual memory {addr[%p], size[%llu]}.",
                reinterpret_cast<void *>(virt.childAddr), virt.childSize);

            auto dto = virt.parentBuffer->GetExchangeDto();
            auto *rdmaDto = dynamic_cast<Hccl::ExchangeRdmaBufferDto *>(dto.get());
            CHK_PTR_NULL(rdmaDto);
            rdmaDto->addr = static_cast<u64>(virt.childAddr);
            rdmaDto->size = static_cast<u64>(virt.childSize);

            Hccl::BinaryStream stream;
            dto->Serialize(stream);
            virt.exportDesc.clear();
            stream.Dump(virt.exportDesc);

            std::vector<char> tempLocalEndpointDesc;
            tempLocalEndpointDesc.resize(sizeof(EndpointDesc));
            if (memcpy_s(tempLocalEndpointDesc.data(), sizeof(EndpointDesc), &endpointDesc, sizeof(EndpointDesc)) != EOK) {
                HCCL_ERROR("[RoceRegedMemMgr][MemoryExport] endpointDesc memcpy_s failed.");
                return HCCL_E_INTERNAL;
            }
            virt.exportDesc.insert(virt.exportDesc.end(),
                tempLocalEndpointDesc.begin(), tempLocalEndpointDesc.end());

            *memDescLen = static_cast<uint32_t>(virt.exportDesc.size());
            *memDesc = static_cast<void *>(virt.exportDesc.data());
            return HCCL_SUCCESS;
        }
    }

    // 非虚拟句柄，走原有逻辑
    Hccl::LocalRdmaRmaBuffer *localRdmaRmaBuffer = reinterpret_cast<Hccl::LocalRdmaRmaBuffer *>(memHandle);

    CHK_RET(GetMemDesc(endpointDesc, localRdmaRmaBuffer));

    *memDescLen = static_cast<uint32_t>(localRdmaRmaBuffer->Desc.size());
    *memDesc = static_cast<void *>(localRdmaRmaBuffer->Desc.data());
    return HCCL_SUCCESS;
}

HcclResult RoceRegedMemMgr::GetParamsFromMemDesc(const void *memDesc, uint32_t descLen, 
                                                EndpointDesc &endpointDesc, Hccl::ExchangeRdmaBufferDto &dto) 
{
    const char *description = static_cast<const char *>(memDesc);

     if (descLen < sizeof(EndpointDesc)) {
 	         HCCL_ERROR("[RoceRegedMemMgr][GetParamsFromMemDesc] [%s] descLen[%u] is too small. aim size:[%llu]", __func__, descLen, sizeof(EndpointDesc));
 	         return HCCL_E_INTERNAL;
 	}
    // 从memDesc末尾提取EndpointDesc
    if (memcpy_s(&endpointDesc, sizeof(EndpointDesc), description + descLen - sizeof(EndpointDesc), sizeof(EndpointDesc)) != EOK) {
        HCCL_ERROR("[RoceRegedMemMgr][GetParamsFromMemDesc] [%s] endpointDesc copy error. aim size:[%llu]", __func__, sizeof(EndpointDesc));
        return HCCL_E_INTERNAL;
    }

    // 反序列化
    std::vector<char> tempDesc{};
    tempDesc.resize(TRANSPORT_EMD_ESC_SIZE);
    tempDesc.assign(description, description + descLen - sizeof(EndpointDesc));
    Hccl::BinaryStream        remoteRdmaRmaBufferStream(tempDesc);
    dto.Deserialize(remoteRdmaRmaBufferStream);
    return HCCL_SUCCESS;
}

HcclResult RoceRegedMemMgr::MemoryImport(const void *memDesc, uint32_t descLen, HcommMem *outMem)
{
    HCCL_INFO("[%s] Begin", __FUNCTION__);

    EndpointDesc endpointDesc;
    Hccl::ExchangeRdmaBufferDto dto;
    CHK_RET(GetParamsFromMemDesc(memDesc, descLen, endpointDesc, dto));

    // 构造RemoteRdmaRmaBuffer
    std::shared_ptr<Hccl::RemoteRdmaRmaBuffer> remoteRdmaRmaBuffer;
    EXECEPTION_CATCH(
        remoteRdmaRmaBuffer = std::make_shared<Hccl::RemoteRdmaRmaBuffer>(this->rdmaHandle_, dto),
        return HCCL_E_PTR;
    );

    // 放到RemoteRdmaRmaBufferMgr_
    hccl::BufferKey<uintptr_t, u64> tempKey(static_cast<uintptr_t>(dto.addr), dto.size);
    if(remoteRdmaRmaBufferMgrs_.find(endpointDesc) == remoteRdmaRmaBufferMgrs_.end()) {
        std::unique_ptr<RemoteRdmaRmaBufferMgr> remoteRdmaRmaBufferMgr;
        EXECEPTION_CATCH((remoteRdmaRmaBufferMgr = std::make_unique<RemoteRdmaRmaBufferMgr>()),
            return HCCL_E_PTR);
        CHK_SMART_PTR_NULL(remoteRdmaRmaBufferMgr);
        remoteRdmaRmaBufferMgrs_[endpointDesc] = std::move(remoteRdmaRmaBufferMgr);
        HCCL_INFO("remoteRdmaRmaBufferMgrs_ add remoteRdmaRmaBufferMgr successfully!");
    }
    
    auto resultPair = remoteRdmaRmaBufferMgrs_[endpointDesc]->Add(tempKey, remoteRdmaRmaBuffer);
    if(!resultPair.second) {
        HCCL_ERROR("[RoceRegedMemMgr][MemoryImport] This memDesc has already been imported!");
        return HCCL_E_AGAIN;
    }

    outMem->addr   = reinterpret_cast<void *>(remoteRdmaRmaBuffer->GetAddr());
    outMem->size    = remoteRdmaRmaBuffer->GetSize();

    return HCCL_SUCCESS;
}

HcclResult RoceRegedMemMgr::MemoryUnimport(const void *memDesc, uint32_t descLen)
{
    HCCL_INFO("[%s] Begin", __FUNCTION__);

    EndpointDesc endpointDesc;
    Hccl::ExchangeRdmaBufferDto dto;
    CHK_RET(GetParamsFromMemDesc(memDesc, descLen, endpointDesc, dto));

    if(remoteRdmaRmaBufferMgrs_.find(endpointDesc) == remoteRdmaRmaBufferMgrs_.end()) {
        HCCL_ERROR("[RoceRegedMemMgr][MemoryUnimport] Remote buffer manager Not Found.");
        return HCCL_E_NOT_FOUND;
    }

    // 删除RemoteRdmaRmaBuffer
    HCCL_INFO("[MemoryUnimport][Rdma] MemoryUnimport");
    hccl::BufferKey<uintptr_t, u64> tempKey(static_cast<uintptr_t>(dto.addr), dto.size);

    bool resultPair = false;
    EXECEPTION_CATCH(resultPair = remoteRdmaRmaBufferMgrs_[endpointDesc]->Del(tempKey), return HCCL_E_NOT_FOUND);
    // 计数器大于1时，返回false，说明框架层有其它设备在使用这段内存，返回HCCL_E_AGAIN
    if (!resultPair) {
        HCCL_INFO("[RoceRegedMemMgr][[MemoryUnimport] Memory reference count is larger than 0"
                    "(used by other RemoteRank).");
        return HCCL_E_AGAIN;
    }
    if (!remoteRdmaRmaBufferMgrs_[endpointDesc]->size()) {
        remoteRdmaRmaBufferMgrs_.erase(endpointDesc);
    }
    return HCCL_SUCCESS;
}

HcclResult RoceRegedMemMgr::GetAllMemHandles(void **memHandles, uint32_t *memHandleNum)
{
    HCCL_INFO("[%s] Begin", __FUNCTION__);
    CHK_PTR_NULL(memHandleNum);

    uint32_t bufferCount = static_cast<uint32_t>(allRegisteredBuffers_.size());
    *memHandleNum = bufferCount;

    HCCL_INFO("[RoceRegedMemMgr][[GetAllMemHandles] memHandleNum is [%d]", bufferCount);

    *memHandles = (bufferCount == 0) ? nullptr : reinterpret_cast<void *>(allRegisteredBuffers_.data());

    return HCCL_SUCCESS;
}

}

/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "hccl_mem_v2.h"
#include "log.h"
#include "exchange_ub_buffer_dto.h"
#include "local_ub_rma_buffer_manager.h"
#include "remote_rma_buffer.h"
#include "local_ub_rma_buffer.h"

using namespace Hccl;

HcclResult HcclMemRegV2(HcclNetDev netDev, const HcclMem *mem, HcclBuf *buf)
{
    HCCL_INFO("[%s] Begin", __FUNCTION__);
    // 仅支持UB类型
    HcclNetDevice *hcclNetDevice = static_cast<HcclNetDevice *>(netDev);
    if (!hcclNetDevice->IsUB()) {
        HCCL_ERROR("[%s] only support UB", __FUNCTION__);
        return HCCL_E_NOT_SUPPORT;
    }

    // 构造LocalUbRmaBuffer
    std::shared_ptr<Buffer> localBufferPtr
        = make_shared<Buffer>(reinterpret_cast<uintptr_t>(mem->addr), mem->size, mem->type);
    std::shared_ptr<LocalUbRmaBuffer> localUbRmaBuffer
        = make_shared<LocalUbRmaBuffer>(localBufferPtr, hcclNetDevice, false);
    LocalUbRmaBufferMgr      *localRmaBufferMgr = LocalUbRmaBufferManager::GetInstance();

    // 注册到LocalUbRmaBuffer计数器
    BufferKey<uintptr_t, u64> tempKey(reinterpret_cast<uintptr_t>(mem->addr), mem->size);
    auto resultPair = localRmaBufferMgr->Add(tempKey, localUbRmaBuffer);
    if (resultPair.first == localRmaBufferMgr->End()) {
        // 若已注册内存有交叉，返回HCCL_E_INTERNAL
        HCCL_ERROR("[%s]The memory overlaps with the memory that has been registered.", __FUNCTION__);
        return HCCL_E_INTERNAL;
    }

    buf->addr   = mem->addr;
    buf->len    = mem->size;
    buf->handle = resultPair.first->second.buffer.get();
    return HCCL_SUCCESS;
}

HcclResult HcclMemDeregV2(const HcclBuf *buf)
{
    HCCL_INFO("[%s] Begin", __FUNCTION__);
    // 从LocalRamBuffer计数器删除HcclBuf
    LocalUbRmaBufferMgr      *localRmaBufferMgr = LocalUbRmaBufferManager::GetInstance();
    BufferKey<uintptr_t, u64> tempKey(reinterpret_cast<uintptr_t>(buf->addr), buf->len);
    try {
        auto resultPair = localRmaBufferMgr->Del(tempKey);
        // 计数器大于1时，返回false，说明框架层有其它设备在使用这段内存，返回HCCL_E_AGAIN
        if (!resultPair) {
            HCCL_INFO("[HcclOneSidedService][DeregMem]Memory reference count is larger than 0"
                      "(used by other RemoteRank), do not deregister memory.");
            return HCCL_E_AGAIN;
        }
        return HCCL_SUCCESS;
    } catch (const std::out_of_range &e) {
        // 若计数器内未找到buf，返回HCCL_E_NOT_FOUND
        HCCL_ERROR("[%s] %s", __FUNCTION__, e.what());
        return HCCL_E_NOT_FOUND;
    }
}

HcclResult HcclMemExportV2(HcclBuf *buf, char **outDesc, uint64_t *outDescLen)
{
    HCCL_INFO("[%s] Begin", __FUNCTION__);
    // 获取序列化信息
    LocalUbRmaBuffer             *localUbRmaBuffer = reinterpret_cast<LocalUbRmaBuffer *>(buf->handle);
    std::unique_ptr<Serializable> dto              = localUbRmaBuffer->GetExchangeDto();
    BinaryStream                  localRdmaRmaBufferStream;
    dto->Serialize(localRdmaRmaBufferStream);
    std::vector<char> tempLocalMemDesc;
    localRdmaRmaBufferStream.Dump(tempLocalMemDesc);
    HCCL_DEBUG("[%s] dump data size [%u]", __func__, tempLocalMemDesc.size());
    // 判断内存描述符是否正确导出
    if (tempLocalMemDesc.empty()) {
        HCCL_ERROR("[%s] tempLocalMemDesc export failed.", __func__);
        return HCCL_E_INTERNAL;
    }

    // 内存描述符拷贝
    *outDescLen = tempLocalMemDesc.size();
    if (memcpy_s(*outDesc, TRANSPORT_EMD_ESC_SIZE, tempLocalMemDesc.data(), tempLocalMemDesc.size()) != EOK) {
        HCCL_ERROR("[%s] tempLocalMemDesc copy error. aim size:[%llu]", __func__, tempLocalMemDesc.size());
        return HCCL_E_INTERNAL;
    }
    return HCCL_SUCCESS;
}

HcclResult HcclMemImportV2(const char *description, uint64_t descLen, bool isRemote, HcclBuf *outBuf, HcclNetDev netDev)
{
    (void)(isRemote);
    HCCL_INFO("[%s] Begin", __FUNCTION__);
    // 仅支持UB类型
    HcclNetDevice *hcclNetDevice = static_cast<HcclNetDevice *>(netDev);
    if (!hcclNetDevice->IsUB()) {
        HCCL_ERROR("[%s] only support UB", __FUNCTION__);
        return HCCL_E_NOT_SUPPORT;
    }

    // 反序列化
    std::vector<char> tempDesc{};
    tempDesc.resize(TRANSPORT_EMD_ESC_SIZE);
    tempDesc.assign(description, description + descLen);
    ExchangeUbBufferDto dto;
    BinaryStream        remoteRdmaRmaBufferStream(tempDesc);
    dto.Deserialize(remoteRdmaRmaBufferStream);

    // 构造RemoteUbRmaBuffer
    RemoteUbRmaBuffer *remoteUbRmaBuffer = new RemoteUbRmaBuffer(hcclNetDevice->GetRdmaHandle(), dto);

    // 填充HcclBuf
    outBuf->addr   = reinterpret_cast<void *>(remoteUbRmaBuffer->GetAddr());
    outBuf->len    = remoteUbRmaBuffer->GetSize();
    outBuf->handle = static_cast<void *>(remoteUbRmaBuffer);
    return HCCL_SUCCESS;
}

HcclResult HcclMemCloseV2(HcclBuf *buf)
{
    HCCL_INFO("[%s] Begin", __FUNCTION__);
    // 仅支持UB类型
    RemoteRmaBuffer *remoteRmaBuffer = static_cast<RemoteRmaBuffer *>(buf->handle);
    if (remoteRmaBuffer->GetRmaType() != RmaType::UB) {
        HCCL_ERROR("[%s] only support UB", __FUNCTION__);
        return HCCL_E_NOT_SUPPORT;
    }

    // 删除RemoteUbRmaBuffer
    HCCL_INFO("[HcclMemCloseV2][Ub] CloseMem");
    RemoteUbRmaBuffer *remoteUbRmaBuffer = static_cast<RemoteUbRmaBuffer *>(remoteRmaBuffer);
    delete remoteUbRmaBuffer;
    return HCCL_SUCCESS;
}
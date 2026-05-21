/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "local_ipc_rma_buffer.h"

#include "rma_buffer.h"
#include "exchange_ipc_buffer_dto.h"
#include "securec.h"

namespace Hccl {

LocalIpcRmaBuffer::LocalIpcRmaBuffer(std::shared_ptr<Buffer> buf) : LocalRmaBuffer(buf, RmaType::IPC)
{
    HrtDevMemAlignWithPage(reinterpret_cast<void *>(buf->GetAddr()), buf->GetSize(), ipcPtr, ipcSize, ipcOffset);
    HrtIpcSetMemoryName(ipcPtr, name, ipcSize, RTS_IPC_MEM_NAME_LEN);
}

LocalIpcRmaBuffer::LocalIpcRmaBuffer(std::shared_ptr<Buffer> buf, std::shared_ptr<LocalIpcRmaBuffer> parent)
    : LocalRmaBuffer(buf, RmaType::IPC)
{
    isAlias_ = true;
    parentBuffer_ = parent;
    // Copy parent's IPC info for the sub-region.
    // The sub-region's offset within the parent's page-aligned IPC memory is:
    //   alias_ipcOffset = parent->ipcOffset + (alias_addr - parent_buf_addr)
    uintptr_t parentBaseAddr = parent->buf->GetAddr();
    uintptr_t aliasAddr = buf->GetAddr();
    ipcOffset = parent->ipcOffset + (aliasAddr - parentBaseAddr);
    ipcSize = buf->GetSize();
    ipcPtr = parent->ipcPtr;
    if (memcpy_s(name, RTS_IPC_MEM_NAME_LEN, parent->name, RTS_IPC_MEM_NAME_LEN) != EOK) {
        HCCL_ERROR("[LocalIpcRmaBuffer] alias name copy failed");
        THROW<InternalException>("[%s] alias name copy failed.", __func__);
    }
    HCCL_INFO("[LocalIpcRmaBuffer] Alias created: buf=[%s] parent=[%s]",
        buf->Describe().c_str(), parent->Describe().c_str());
}

LocalIpcRmaBuffer::~LocalIpcRmaBuffer()
{
    if (isAlias_) {
        return;
    }
    DECTOR_TRY_CATCH("LocalIpcRmaBuffer", HrtIpcDestroyMemoryName(name));
}

string LocalIpcRmaBuffer::Describe() const
{
    return StringFormat("LocalIpcRmaBuffer[buf=%s, ipcPtr=0x%llx, ipcOffset=0x%llx, ipcSize=0x%llx, name=%s]",
                        buf->Describe().c_str(), reinterpret_cast<uintptr_t>(ipcPtr), ipcOffset, ipcSize, name);
}

std::unique_ptr<Serializable> LocalIpcRmaBuffer::GetExchangeDto()
{
    std::unique_ptr<ExchangeIpcBufferDto> dto
        = make_unique<ExchangeIpcBufferDto>(buf->GetAddr(), buf->GetSize(), ipcOffset, HrtDeviceGetBareTgid(), buf->GetMemTag().c_str());
    (void)memcpy_s(dto->name, RTS_IPC_MEM_NAME_LEN, name, RTS_IPC_MEM_NAME_LEN);
    return std::unique_ptr<Serializable>(dto.release());
}

void LocalIpcRmaBuffer::Grant(u32 pid)
{
    u32 myPid = HrtDeviceGetBareTgid();
    if (pid == myPid) {
        HCCL_INFO("pid is equal to myPid, do need to use HrtIpcSetMemoryPid grant, pid==myPid=%u", pid);
        return;
    }
    HrtIpcSetMemoryPid(name, static_cast<s32>(pid));
}

} // namespace Hccl
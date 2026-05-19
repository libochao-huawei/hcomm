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

namespace Hccl {

LocalIpcRmaBuffer::LocalIpcRmaBuffer(std::shared_ptr<Buffer> buf) : LocalRmaBuffer(buf, RmaType::IPC)
{
    HrtDevMemAlignWithPage(reinterpret_cast<void *>(buf->GetAddr()), buf->GetSize(), ipcPtr_, ipcSize_, ipcOffset_);
    HrtIpcSetMemoryName(ipcPtr_, name_, ipcSize_, RTS_IPC_MEM_NAME_LEN);
}

LocalIpcRmaBuffer::LocalIpcRmaBuffer(std::shared_ptr<Buffer> buf, bool skipReg)
    : LocalRmaBuffer(buf, RmaType::IPC)
{
    // Protected constructor for virtual subclass — skips HrtIpcSetMemoryName.
    // ipcPtr_/ipcSize_/ipcOffset_ stay zero; name_ stays empty.
    // Destructor must not call HrtIpcDestroyMemoryName for virtual buffers.
}

LocalIpcRmaBuffer::~LocalIpcRmaBuffer()
{
    if (name_[0] != '\0') {
        DECTOR_TRY_CATCH("LocalIpcRmaBuffer", HrtIpcDestroyMemoryName(name_));
    }
}

string LocalIpcRmaBuffer::Describe() const
{
    return StringFormat("LocalIpcRmaBuffer[buf=%s, ipcPtr=0x%llx, ipcOffset=0x%llx, ipcSize=0x%llx, name=%s]",
                        buf->Describe().c_str(), reinterpret_cast<uintptr_t>(ipcPtr_), ipcOffset_, ipcSize_, name_);
}

std::unique_ptr<Serializable> LocalIpcRmaBuffer::GetExchangeDto()
{
    std::unique_ptr<ExchangeIpcBufferDto> dto
        = make_unique<ExchangeIpcBufferDto>(GetAddr(), GetSize(), ipcOffset_, HrtDeviceGetBareTgid(), buf->GetMemTag().c_str());
    (void)memcpy_s(dto->name, RTS_IPC_MEM_NAME_LEN, name_, RTS_IPC_MEM_NAME_LEN);
    return std::unique_ptr<Serializable>(dto.release());
}

void LocalIpcRmaBuffer::Grant(u32 pid)
{
    u32 myPid = HrtDeviceGetBareTgid();
    if (pid == myPid) {
        HCCL_INFO("pid is equal to myPid, do need to use HrtIpcSetMemoryPid grant, pid==myPid=%u", pid);
        return;
    }
    HrtIpcSetMemoryPid(name_, static_cast<s32>(pid));
}

} // namespace Hccl

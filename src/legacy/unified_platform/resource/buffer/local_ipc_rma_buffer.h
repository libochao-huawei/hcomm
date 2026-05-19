/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef HCCLV2_LOCAL_IPC_RMA_BUFFER_H
#define HCCLV2_LOCAL_IPC_RMA_BUFFER_H

#include "local_rma_buffer.h"

#include "rma_buffer_lite.h"
#include "orion_adapter_rts.h"

namespace Hccl {

class LocalIpcRmaBuffer : public LocalRmaBuffer {
public:
    explicit LocalIpcRmaBuffer(std::shared_ptr<Buffer> buf);

    ~LocalIpcRmaBuffer() override;

    LocalIpcRmaBuffer(const LocalIpcRmaBuffer &that) = delete;

    LocalIpcRmaBuffer &operator=(const LocalIpcRmaBuffer &that) = delete;

    std::string Describe() const override;

    void Grant(u32 pid);

    virtual u64 GetIpcOffset() const { return ipcOffset_; }
    virtual const char* GetIpcName() const { return name_; }

    std::unique_ptr<Serializable> GetExchangeDto() override;
    std::pair<uintptr_t, u64> GetBufferInfo() override {return std::make_pair(GetAddr(), GetSize());}

protected:
    // Skip-registration constructor for virtual subclass — does not call HrtIpcSetMemoryName
    LocalIpcRmaBuffer(std::shared_ptr<Buffer> buf, bool skipReg);

    char  name_[RTS_IPC_MEM_NAME_LEN]{0};
    void *ipcPtr_{nullptr};
    u64   ipcOffset_{0};
    u64   ipcSize_{0};
};

} // namespace Hccl
#endif // HCCLV2_LOCAL_IPC_RMA_BUFFER_H

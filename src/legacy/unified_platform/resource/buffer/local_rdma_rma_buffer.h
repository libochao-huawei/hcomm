
/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef HCCLV2_LOCAL_RDMA_RMA_BUFFER_H
#define HCCLV2_LOCAL_RDMA_RMA_BUFFER_H

#include "local_rma_buffer.h"
#include "orion_adapter_hccp.h"

namespace Hccl {

class LocalRdmaRmaBuffer : public LocalRmaBuffer {
public:
    LocalRdmaRmaBuffer(std::shared_ptr<Buffer> buf, RdmaHandle rdmaHandle);

    ~LocalRdmaRmaBuffer() override;

    LocalRdmaRmaBuffer(const LocalRdmaRmaBuffer &that) = delete;

    LocalRdmaRmaBuffer &operator=(const LocalRdmaRmaBuffer &that) = delete;

    std::string Describe() const override;

    std::unique_ptr<Serializable> GetExchangeDto() override;

    virtual u32 GetLkey() const
    {
        return lkey_;
    }

    virtual u32 GetRkey() const
    {
        return rkey_;
    }

    virtual RdmaHandle GetRdmaHandle() const
    {
        return rdmaHandle_;
    }

    std::pair<uintptr_t, u64> GetBufferInfo() override {return make_pair(GetAddr(), GetSize());}

    std::vector<char> Desc;

protected:
    // Skip-registration constructor for virtual subclass — does not call RaRegisterMr
    LocalRdmaRmaBuffer(std::shared_ptr<Buffer> buf, RdmaHandle rdmaHandle, u32 lkey, u32 rkey);

    RdmaHandle rdmaHandle_;
    u8         key_[RDMA_MEM_KEY_MAX_LEN]{0};
    u32        lkey_{0};
    u32        rkey_{0};
    MrHandle   mrHandle_{nullptr};
};

} // namespace Hccl
#endif // HCCLV2_LOCAL_RDMA_RMA_BUFFER_H

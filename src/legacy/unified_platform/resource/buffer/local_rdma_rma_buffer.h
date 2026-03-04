
/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
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

    u32 GetLkey() const 
    {
        return lkey;
    }
    std::pair<uintptr_t, u64> GetBufferInfo() {return make_pair(buf->GetAddr(), buf->GetSize());}

    std::vector<char> Desc;
    
private:
    RdmaHandle rdmaHandle;
    u8         key[RDMA_MEM_KEY_MAX_LEN]{0};
    u32        lkey{0};
    u32        rkey{0};
    MrHandle   mrHandle{nullptr};
};

} // namespace Hccl
#endif // HCCLV2_LOCAL_RDMA_RMA_BUFFER_H
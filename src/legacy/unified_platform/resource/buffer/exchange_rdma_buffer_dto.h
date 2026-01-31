/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef HCCLV2_EXCHANGE_RDMA_BUFFER_DTO_H
#define HCCLV2_EXCHANGE_RDMA_BUFFER_DTO_H
#include <map>
#include <string>
#include "serializable.h"
#include "orion_adapter_hccp.h"
namespace Hccl {

class ExchangeRdmaBufferDto : public Serializable { // RDMA RMA Buffer / Notify  需要交换的DTO
public:
    ExchangeRdmaBufferDto()
    {
    }

    ExchangeRdmaBufferDto(u64 addr, u64 size, u32 rkey) : addr(addr), size(size), rkey(rkey)
    {
    }

    void Serialize(Hccl::BinaryStream &stream) override
    {
        stream << addr << size << rkey;
    }

    void Deserialize(Hccl::BinaryStream &stream) override
    {
        stream >> addr >> size >> rkey;
    }

    std::string Describe() const override
    {
        return StringFormat("ExchangeRdmaBufferDto[addr=0x%llx, size=0x%llx, rkey=%lu]", addr, size, rkey);
    }

    u64 addr{0};
    u32 size{0};
    u32 rkey{0};
    u8  key[RDMA_MEM_KEY_MAX_LEN]{0};
};

} // namespace Hccl

#endif
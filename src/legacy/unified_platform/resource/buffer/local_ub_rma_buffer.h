/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef HCCLV2_LOCAL_UB_RMA_BUFFER_H
#define HCCLV2_LOCAL_UB_RMA_BUFFER_H

#include "local_rma_buffer.h"

#include "enum_factory.h"
#include "orion_adapter_hccp.h"
#include "orion_adapter_rts.h"
#include "net_device.h"

namespace Hccl {

MAKE_ENUM(UbBufferStatus, INIT, READY, RELEASED);

class LocalUbRmaBuffer : public LocalRmaBuffer {
public:
    LocalUbRmaBuffer(std::shared_ptr<Buffer> buf, RdmaHandle rdmaHandle);

    LocalUbRmaBuffer(std::shared_ptr<Buffer> buf, void* netDevice, bool flag);

    LocalUbRmaBuffer(std::shared_ptr<Buffer> buf);

    ~LocalUbRmaBuffer() override;

    LocalUbRmaBuffer(const LocalUbRmaBuffer &that) = delete;

    LocalUbRmaBuffer &operator=(const LocalUbRmaBuffer &that) = delete;

    string Describe() const override;

    std::unique_ptr<Serializable> GetExchangeDto() override;

    virtual u32 GetTokenId() const;
    virtual u32 GetTokenValue() const;
    virtual TokenIdHandle GetTokenIdHandle() const;
    std::pair<uintptr_t, u64> GetBufferInfo() override {return make_pair(GetAddr(), GetSize());}
    u64 GetTargetSeg() const {return reqReg.targetSegVa;}
    virtual u32 GetKeySize() const { return keySize; }
    virtual const u8* GetKey() const { return key; }

    // 硬件实际注册范围：4K 对齐后可能大于传入的 addr/size
    uintptr_t GetAlignedAddr() const override { return alignedAddr_; }
    u64 GetAlignedSize() const override { return alignedSize_; }

    std::vector<char> Desc;

protected:
    // Skip-registration constructor for virtual subclass — does not call HrtRaUbLocalMemReg
    LocalUbRmaBuffer(std::shared_ptr<Buffer> buf, u32 tokenValue, u32 tokenId,
                     TokenIdHandle tokenIdHandle, u32 keySize, u64 segVa, const u8* keySrc);

    RdmaHandle           rdmaHandle{nullptr};
    HcclNetDevice        *netDev{nullptr};
    u8                   key[HRT_UB_MEM_KEY_MAX_LEN]{0};
    u32                  tokenValue{0};
    u32                  tokenId{0};
    TokenIdHandle        tokenIdHandle{0};
    u32                  keySize{0};

    HrtRaUbLocalMemRegOutParam    reqReg;
    void*                         lmemHandle{nullptr};
    u64                  segVa{0};

    uintptr_t            alignedAddr_{0};
    u64                  alignedSize_{0};
};
u32 GetUbToken(); // 生成伪随机数
} // namespace Hccl
#endif // HCCLV2_LOCAL_UB_RMA_BUFFER_H

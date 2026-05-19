/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef HCCLV2_VIRTUAL_LOCAL_RMA_BUFFER_H
#define HCCLV2_VIRTUAL_LOCAL_RMA_BUFFER_H

#include <memory>
#include "local_rdma_rma_buffer.h"
#include "local_ipc_rma_buffer.h"
#include "local_ub_rma_buffer.h"
#include "exchange_rdma_buffer_dto.h"
#include "exchange_ipc_buffer_dto.h"
#include "exchange_ub_buffer_dto.h"

namespace Hccl {

// Virtual wrapper for LocalRdmaRmaBuffer — represents a sub-region of an already-registered RDMA buffer.
class VirtualLocalRdmaRmaBuffer : public LocalRdmaRmaBuffer {
public:
    VirtualLocalRdmaRmaBuffer(std::shared_ptr<LocalRdmaRmaBuffer> real,
                              uintptr_t addr, u64 size,
                              const char *memTag = "")
        : LocalRdmaRmaBuffer(
              std::make_shared<Buffer>(addr, size, real->GetBuf()->GetMemType(),
                  memTag ? memTag : ""),
              real->GetRdmaHandle(),
              real->GetLkey(),
              real->GetRkey()),
          real_(std::move(real)),
          vAddr_(addr),
          vSize_(size),
          vMemTag_(memTag ? memTag : "")
    {
    }

    bool IsVirtual() const override { return true; }
    LocalRmaBuffer* GetRealBuffer() const override { return real_.get(); }

    uintptr_t GetAddr() const override { return vAddr_; }
    size_t GetSize() const override { return static_cast<size_t>(vSize_); }
    std::pair<uintptr_t, u64> GetBufferInfo() override { return {vAddr_, vSize_}; }

    u32 GetLkey() const override { return real_->GetLkey(); }
    u32 GetRkey() const override { return real_->GetRkey(); }
    RdmaHandle GetRdmaHandle() const override { return real_->GetRdmaHandle(); }

    std::unique_ptr<Serializable> GetExchangeDto() override
    {
        auto dto = std::make_unique<ExchangeRdmaBufferDto>(
            vAddr_, vSize_, real_->GetRkey(), vMemTag_.c_str());
        return std::unique_ptr<Serializable>(dto.release());
    }

    std::string Describe() const override
    {
        return StringFormat("VirtualLocalRdmaRmaBuffer[addr=0x%llx, size=%llu, memTag=%s, real=%s]",
                            static_cast<unsigned long long>(vAddr_),
                            static_cast<unsigned long long>(vSize_),
                            vMemTag_.c_str(),
                            real_->Describe().c_str());
    }

private:
    std::shared_ptr<LocalRdmaRmaBuffer> real_;
    uintptr_t vAddr_;
    u64 vSize_;
    std::string vMemTag_;
};

// Virtual wrapper for LocalIpcRmaBuffer — represents a sub-region of an already-registered IPC buffer.
class VirtualLocalIpcRmaBuffer : public LocalIpcRmaBuffer {
public:
    VirtualLocalIpcRmaBuffer(std::shared_ptr<LocalIpcRmaBuffer> real,
                             uintptr_t addr, u64 size,
                             const char *memTag = "")
        : LocalIpcRmaBuffer(
              std::make_shared<Buffer>(addr, size, real->GetBuf()->GetMemType(),
                  memTag ? memTag : ""),
              true),  // skipReg=true
          real_(std::move(real)),
          vAddr_(addr),
          vSize_(size),
          vMemTag_(memTag ? memTag : "")
    {
    }

    bool IsVirtual() const override { return true; }
    LocalRmaBuffer* GetRealBuffer() const override { return real_.get(); }

    uintptr_t GetAddr() const override { return vAddr_; }
    size_t GetSize() const override { return static_cast<size_t>(vSize_); }
    std::pair<uintptr_t, u64> GetBufferInfo() override { return {vAddr_, vSize_}; }

    u64 GetIpcOffset() const override { return real_->GetIpcOffset(); }
    const char* GetIpcName() const override { return real_->GetIpcName(); }

    std::unique_ptr<Serializable> GetExchangeDto() override
    {
        auto dto = std::make_unique<ExchangeIpcBufferDto>(
            vAddr_, vSize_, real_->GetIpcOffset(), HrtDeviceGetBareTgid(),
            vMemTag_.c_str());
        (void)memcpy_s(dto->name, RTS_IPC_MEM_NAME_LEN, real_->GetIpcName(), RTS_IPC_MEM_NAME_LEN);
        return std::unique_ptr<Serializable>(dto.release());
    }

    void Grant(u32 pid)
    {
        real_->Grant(pid);
    }

    std::string Describe() const override
    {
        return StringFormat("VirtualLocalIpcRmaBuffer[addr=0x%llx, size=%llu, memTag=%s, real=%s]",
                            static_cast<unsigned long long>(vAddr_),
                            static_cast<unsigned long long>(vSize_),
                            vMemTag_.c_str(),
                            real_->Describe().c_str());
    }

private:
    std::shared_ptr<LocalIpcRmaBuffer> real_;
    uintptr_t vAddr_;
    u64 vSize_;
    std::string vMemTag_;
};

// Virtual wrapper for LocalUbRmaBuffer — represents a sub-region of an already-registered UB buffer.
class VirtualLocalUbRmaBuffer : public LocalUbRmaBuffer {
public:
    VirtualLocalUbRmaBuffer(std::shared_ptr<LocalUbRmaBuffer> real,
                            uintptr_t addr, u64 size,
                            const char *memTag = "")
        : LocalUbRmaBuffer(
              std::make_shared<Buffer>(addr, size, real->GetBuf()->GetMemType(),
                  memTag ? memTag : ""),
              real->GetTokenValue(),
              real->GetTokenId(),
              real->GetTokenIdHandle(),
              real->GetKeySize(),
              real->GetTargetSeg(),
              real->GetKey()),
          real_(std::move(real)),
          vAddr_(addr),
          vSize_(size),
          vMemTag_(memTag ? memTag : "")
    {
    }

    bool IsVirtual() const override { return true; }
    LocalRmaBuffer* GetRealBuffer() const override { return real_.get(); }

    uintptr_t GetAddr() const override { return vAddr_; }
    size_t GetSize() const override { return static_cast<size_t>(vSize_); }
    std::pair<uintptr_t, u64> GetBufferInfo() override { return {vAddr_, vSize_}; }

    u32 GetTokenId() const override { return real_->GetTokenId(); }
    u32 GetTokenValue() const override { return real_->GetTokenValue(); }
    TokenIdHandle GetTokenIdHandle() const override { return real_->GetTokenIdHandle(); }
    u32 GetKeySize() const override { return real_->GetKeySize(); }
    const u8* GetKey() const override { return real_->GetKey(); }

    std::unique_ptr<Serializable> GetExchangeDto() override
    {
        auto dto = std::make_unique<ExchangeUbBufferDto>(
            vAddr_, vSize_,
            real_->GetBuf()->GetMemType(),
            vMemTag_.c_str(),
            real_->GetTokenValue(),
            real_->GetTokenId(),
            real_->GetKeySize());
        (void)memcpy_s(dto->key, HRT_UB_MEM_KEY_MAX_LEN, real_->GetKey(), HRT_UB_MEM_KEY_MAX_LEN);
        dto->segVa = real_->GetTargetSeg();
        return std::unique_ptr<Serializable>(dto.release());
    }

    std::string Describe() const override
    {
        return StringFormat("VirtualLocalUbRmaBuffer[addr=0x%llx, size=%llu, memTag=%s, real=%s]",
                            static_cast<unsigned long long>(vAddr_),
                            static_cast<unsigned long long>(vSize_),
                            vMemTag_.c_str(),
                            real_->Describe().c_str());
    }

private:
    std::shared_ptr<LocalUbRmaBuffer> real_;
    uintptr_t vAddr_;
    u64 vSize_;
    std::string vMemTag_;
};

} // namespace Hccl
#endif // HCCLV2_VIRTUAL_LOCAL_RMA_BUFFER_H

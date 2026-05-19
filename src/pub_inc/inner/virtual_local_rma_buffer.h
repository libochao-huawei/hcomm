/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef VIRTUAL_LOCAL_RMA_BUFFER_H
#define VIRTUAL_LOCAL_RMA_BUFFER_H

#include <memory>
#include <sstream>
#include <cstring>
#include "local_rdma_rma_buffer.h"
#include "local_ipc_rma_buffer.h"
#include "log.h"

namespace hccl {

// Virtual wrapper for LocalRdmaRmaBuffer — represents a sub-region of an already-registered RDMA buffer.
// Serialization format (from LocalRdmaRmaBufferImpl::Serialize):
//   type(u8,1) + addr(void*,8) + size(u64,8) + devAddr(void*,8) + memType(RmaMemType,4) + lkey(u32,4)
// Total: 33 bytes on 64-bit.
class VirtualLocalRdmaRmaBuffer : public LocalRdmaRmaBuffer {
    static constexpr size_t kRdmaSerAddrOff = sizeof(u8);                    // offset 1
    static constexpr size_t kRdmaSerSizeOff = kRdmaSerAddrOff + sizeof(void*); // offset 9
    static constexpr size_t kRdmaSerTailOff = kRdmaSerSizeOff + sizeof(u64);   // offset 17
    // Tail: devAddr(8) + memType(4) + lkey(4) = 16 bytes, copied verbatim from real buffer.

public:
    VirtualLocalRdmaRmaBuffer(std::shared_ptr<LocalRdmaRmaBuffer> real,
                              void* addr, u64 size)
        : LocalRdmaRmaBuffer(real->GetNetDevCtx(), addr, size, real->GetMemType(), true),
          real_(std::move(real)),
          vAddr_(addr),
          vSize_(size)
    {
    }

    bool IsVirtual() const override { return true; }
    RmaBuffer* GetRealBuffer() const override { return real_.get(); }

    void* GetAddr() const override { return vAddr_; }
    u64 GetSize() const override { return vSize_; }

    u32 GetKey() const override { return real_->GetKey(); }

    std::string &Serialize() override
    {
        if (!serializedCache_.empty()) {
            return serializedCache_;
        }
        std::string &realSer = real_->Serialize();
        if (realSer.size() < kRdmaSerTailOff) {
            HCCL_ERROR("[VirtualLocalRdmaRmaBuffer][Serialize] real serialized blob too small: %zu", realSer.size());
            serializedCache_ = realSer;
            return serializedCache_;
        }
        // Build modified blob: type(1) + vAddr(8) + vSize(8) + tail from real blob
        std::ostringstream oss;
        oss.write(realSer.data(), sizeof(u8));      // copy type
        oss.write(reinterpret_cast<const char*>(&vAddr_), sizeof(vAddr_));
        oss.write(reinterpret_cast<const char*>(&vSize_), sizeof(vSize_));
        oss.write(realSer.data() + kRdmaSerTailOff, realSer.size() - kRdmaSerTailOff); // copy tail
        serializedCache_ = oss.str();
        return serializedCache_;
    }

private:
    std::shared_ptr<LocalRdmaRmaBuffer> real_;
    void* vAddr_;
    u64 vSize_;
    std::string serializedCache_;
};

// Virtual wrapper for LocalIpcRmaBuffer — represents a sub-region of an already-registered IPC buffer.
// Serialization format (from LocalIpcRmaBufferImpl::Serialize):
//   type(u8,1) + addr(void*,8) + size(u64,8) + devAddr(void*,8) + memType(RmaMemType,4)
//   + ipcName(SecIpcName_t,65) + memOffset(u64,8)
// Total: 102 bytes on 64-bit.
class VirtualLocalIpcRmaBuffer : public LocalIpcRmaBuffer {
    static constexpr size_t kIpcSerAddrOff = sizeof(u8);                    // offset 1
    static constexpr size_t kIpcSerSizeOff = kIpcSerAddrOff + sizeof(void*); // offset 9
    static constexpr size_t kIpcSerTailOff = kIpcSerSizeOff + sizeof(u64);   // offset 17
    // Tail: devAddr(8) + memType(4) + ipcName(65) + memOffset(8) = 85 bytes

public:
    VirtualLocalIpcRmaBuffer(std::shared_ptr<LocalIpcRmaBuffer> real,
                             void* addr, u64 size)
        : LocalIpcRmaBuffer(real->GetNetDevCtx(), addr, size, real->GetMemType(), true),
          real_(std::move(real)),
          vAddr_(addr),
          vSize_(size)
    {
    }

    bool IsVirtual() const override { return true; }
    RmaBuffer* GetRealBuffer() const override { return real_.get(); }

    void* GetAddr() const override { return vAddr_; }
    u64 GetSize() const override { return vSize_; }

    std::string &Serialize() override
    {
        if (!serializedCache_.empty()) {
            return serializedCache_;
        }
        std::string &realSer = real_->Serialize();
        if (realSer.size() < kIpcSerTailOff) {
            HCCL_ERROR("[VirtualLocalIpcRmaBuffer][Serialize] real serialized blob too small: %zu", realSer.size());
            serializedCache_ = realSer;
            return serializedCache_;
        }
        std::ostringstream oss;
        oss.write(realSer.data(), sizeof(u8));      // copy type
        oss.write(reinterpret_cast<const char*>(&vAddr_), sizeof(vAddr_));
        oss.write(reinterpret_cast<const char*>(&vSize_), sizeof(vSize_));
        oss.write(realSer.data() + kIpcSerTailOff, realSer.size() - kIpcSerTailOff); // copy tail
        serializedCache_ = oss.str();
        return serializedCache_;
    }

    HcclResult Grant(u32 remotePid, u32 remoteSdid)
    {
        return real_->Grant(remotePid, remoteSdid);
    }

private:
    std::shared_ptr<LocalIpcRmaBuffer> real_;
    void* vAddr_;
    u64 vSize_;
    std::string serializedCache_;
};

}  // namespace hccl
#endif  // VIRTUAL_LOCAL_RMA_BUFFER_H

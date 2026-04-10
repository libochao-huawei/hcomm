/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef COMM_MEMS_H
#define COMM_MEMS_H

#include <memory>
#include <vector>
#include <unordered_map>
#include <mutex>
#include "hccl_types.h"
#include "log.h"
#include "hccl_mem_defs.h"
#include "rma_buffer_mgr.h"

namespace hccl { 
struct CommMemHandle {
    void* addr {nullptr};
    uint64_t size {0};
    CommMemType memType {COMM_MEM_TYPE_INVALID};
};
struct CommMemHandleEqual {
    bool operator()(const CommMemHandle& lhs, const CommMemHandle& rhs) const {
        return lhs.addr == rhs.addr;
    }
};

CommMemType ConvertHcclToCommMemType(HcclMemType hcclType);
HcclMemType ConvertCommToHcclMemType(CommMemType commType);

}  // namespace hccl
 
namespace std {
    template <>
    struct hash<hccl::CommMemHandle> {
        size_t operator()(const hccl::CommMemHandle& memHandle) const {
            return std::hash<void*>()(memHandle.addr);
        }
    };
}

namespace hccl {
/**
 * @note 职责：集合通信域内MyRank的通信内存管理，包括HCCL Buffer和其他待注册到EndPoint内存
 */
class CommMems {
public:
    using Handle = std::shared_ptr<CommMemHandle>;
    using MemKey = hccl::BufferKey<uintptr_t, uint64_t>;
    using Table  = hccl::RmaBufferMgr<MemKey, Handle>;
 
    explicit CommMems(uint64_t bufferSize);
    ~CommMems() = default;

    HcclResult Add(void *addr, uint64_t len);

    HcclResult GetHcclBuffer(void *&addr, uint64_t &len);

    HcclResult Init(HcclMem cclBuffer);

    HcclResult GetMemoryHandles(std::vector<HcclMem> &mem);

    // 用户注册/反注册内存
    HcclResult CommRegMem(const std::string& tag, const CommMem& mem, void** rawHandle);
    HcclResult CommUnregMem(const std::string& tag, const void* rawHandle);
    HcclResult GetTagMemoryHandles(void** memHandles, uint32_t memHandleNum, std::vector<HcclMem> &mem, 
        std::vector<std::string> &memTag);

private:
    uint64_t bufferSize_{};
    void*   addr_{nullptr};
    std::size_t size_{0};
    HcclMemType memType_{HcclMemType::HCCL_MEM_TYPE_DEVICE};
 
    static inline MemKey MakeKey(void* addr, uint64_t size) {
        return MemKey(reinterpret_cast<uintptr_t>(addr), static_cast<uint64_t>(size));
    }
    struct TagRegistry {
        Table table;                                        // 区间树 + ref 语义
    };
    // 用户绑定内存
    std::mutex memMutex_;
    // 每个 tag 一份 registry
    std::unordered_map<std::string, TagRegistry> tagRegs_;
    // 每个tag 1个 CommMemHandle
    std::unordered_map<std::string, std::shared_ptr<CommMemHandle>> opBindings_;
    std::unordered_map<void*, std::string> opReverseBindings_;
};
}

#endif // COMM_MEMS_H

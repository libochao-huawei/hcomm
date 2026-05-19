/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef REGED_MEM_COMMON_H
#define REGED_MEM_COMMON_H

#include <memory>
#include <vector>
#include "reged_mem_mgr.h"
#include "buffer_key.h"
#include "log.h"

namespace hcomm {

// ==================== RegExactDup: 精确重复 → ref++ ====================
// mgr:      RmaBufferMgr 实例
// hwKey:    已注册 buffer 的 HW 覆盖 key
// existing: 已注册的真实 buffer
// 行为：对树中已有 key 做 AddToTree，引用计数 +1，返回已有 buffer 指针
template <typename Mgr, typename Buf>
inline HcclResult RegExactDup(Mgr &mgr,
                               const hccl::BufferKey<uintptr_t, u64> &hwKey,
                               std::shared_ptr<Buf> &existing,
                               void **memHandle)
{
    mgr.AddToTree(hwKey, existing);
    *memHandle = static_cast<void *>(existing.get());
    return HCCL_SUCCESS;
}

// ==================== RegSubset: 子集 → 虚拟 buffer ====================
// mgr:      RmaBufferMgr 实例
// existing: 已注册的真实 buffer（大范围）
// reged:    allRegisteredBuffers_ 容器
// hwKey:    真实 buffer 的 HW 覆盖 key
// virtArgs: 传给虚拟 buffer 构造函数的参数（reqAddr, reqSize, memTag）
// 行为：对真实 buffer 的 HW key ref++，创建虚拟 buffer，加入容器，返回虚拟指针
template <typename Mgr, typename RealBuf, typename VirtBuf, typename... VirtArgs>
inline HcclResult RegSubset(Mgr &mgr,
                             std::shared_ptr<RealBuf> &existing,
                             std::vector<std::shared_ptr<RealBuf>> &reged,
                             const hccl::BufferKey<uintptr_t, u64> &realHwKey,
                             void **memHandle,
                             VirtArgs &&...virtArgs)
{
    mgr.AddToTree(realHwKey, existing);
    auto virt = std::make_shared<VirtBuf>(existing, std::forward<VirtArgs>(virtArgs)...);
    reged.push_back(virt);
    *memHandle = static_cast<void *>(virt.get());
    return HCCL_SUCCESS;
}

// ==================== RegSubset + Track: 子集（带额外追踪回调）====================
// 专用于 AicpuTsRoceRegedMemMgr 等需要在注册后调用 TrackRegisteredBuffer 的场景
template <typename Mgr, typename RealBuf, typename VirtBuf, typename TrackFn, typename... VirtArgs>
inline HcclResult RegSubsetTracked(Mgr &mgr,
                                    std::shared_ptr<RealBuf> &existing,
                                    std::vector<std::shared_ptr<RealBuf>> &reged,
                                    const hccl::BufferKey<uintptr_t, u64> &realHwKey,
                                    TrackFn &&track,
                                    void **memHandle,
                                    VirtArgs &&...virtArgs)
{
    mgr.AddToTree(realHwKey, existing);
    auto virt = std::make_shared<VirtBuf>(existing, std::forward<VirtArgs>(virtArgs)...);
    track(virt);
    reged.push_back(virt);
    *memHandle = static_cast<void *>(virt.get());
    return HCCL_SUCCESS;
}

}  // namespace hcomm
#endif  // REGED_MEM_COMMON_H

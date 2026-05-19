/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef REGED_MEM_MGR_H
#define REGED_MEM_MGR_H

#include <memory>
#include <vector>
#include "hcomm_c_adpt.h"
#include "log.h"
#include "buffer_key.h"

using RdmaHandle = void *;

namespace hcomm {
/**
 * @note 职责：用于通信设备EndPoint的注册内存信息管理，支持基于RmaBufferMgr类的重叠内存的检测报错等。
 */
class RegedMemMgr {
public:
    RegedMemMgr() = default;
    virtual ~RegedMemMgr() = default;

    // 注册内存
    virtual HcclResult RegisterMemory(HcommMem mem, const char *memTag, void **memHandle) = 0;

    // 注销内存
    virtual HcclResult UnregisterMemory(void* memHandle) = 0;

    // 导出指定内存描述，用于交换
    virtual HcclResult MemoryExport(const EndpointDesc endpointDesc, void *memHandle, void **memDesc, uint32_t *memDescLen) = 0;

    // 基于内存描述，导入获得内存
    virtual HcclResult MemoryImport(const void *memDesc, uint32_t descLen, HcommMem *outMem) = 0;

    // 关闭内存
    virtual HcclResult MemoryUnimport(const void *memDesc, uint32_t descLen) = 0;

    virtual HcclResult GetAllMemHandles(void **memHandles, uint32_t *memHandleNum) = 0;

    // 授权
    virtual HcclResult MemoryGrant(const HcommMemGrantInfo *remoteGrantInfo)
    {
        return HCCL_SUCCESS;
    }

    RdmaHandle rdmaHandle_{nullptr};
};

// ==================== RegisterMemory 公共子函数 ====================
// 各 Manager 的 RegisterMemory 入口共用，消除重复代码。

// RegExactDup: 精确重复 → ref++
// mgr:      RmaBufferMgr 实例
// hwKey:    已注册 buffer 的 HW 覆盖 key
// existing: 已注册的真实 buffer
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

// RegSubset: 子集 → 虚拟 buffer
// mgr:      RmaBufferMgr 实例
// existing: 已注册的真实 buffer（大范围）
// reged:    allRegisteredBuffers_ 容器
// realHwKey: 真实 buffer 的 HW 覆盖 key
// virtArgs: 转发给虚拟 buffer 构造函数的参数（reqAddr, reqSize, memTag）
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

// RegSubsetTracked: 子集（带额外追踪回调）
// 专用于需要在注册后调用额外追踪（如 TrackRegisteredBuffer）的场景
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

#endif // REGED_MEM_MGR_H

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

protected:
    // 参数校验：memHandle / addr / size / type
    static HcclResult ValidateMemParams(HcommMem mem, void **memHandle)
    {
        CHK_PTR_NULL(memHandle);
        CHK_PTR_NULL(mem.addr);
        CHK_PRT_RET(mem.size == 0, HCCL_ERROR("[%s] mem size is zero", __func__), HCCL_E_PARA);
        CHK_PRT_RET(mem.type == COMM_MEM_TYPE_INVALID,
            HCCL_ERROR("[%s] invalid mem type [%d]", __func__, mem.type), HCCL_E_PARA);
        return HCCL_SUCCESS;
    }

    // Find命中则基于父buffer构造别名，未命中则构造新buffer并注册
    // makeAlias(parentBuffer) — 基于父buffer构造别名
    // makeNew()              — 构造新buffer
    template <typename Mgr, typename FindResult, typename BufferPtr, typename MakeAlias, typename MakeNew>
    static void RegisterOrAlias(Mgr& mgr, const FindResult& findPair, BufferPtr& buffer,
        MakeAlias&& makeAlias, MakeNew&& makeNew)
    {
        if (findPair.first) {
            auto parentBuffer = findPair.second;
            AddRefToParent(mgr, parentBuffer);
            EXECEPTION_CATCH((buffer = makeAlias(parentBuffer)), return HCCL_E_PTR);
        } else {
            EXECEPTION_CATCH((buffer = makeNew()), return HCCL_E_PTR);
            AddRefToParent(mgr, buffer);
        }
    }

    // 用实际注册后的addr/size做key，对buffer增加引用计数
    template <typename Mgr, typename BufferPtr>
    static void AddRefToParent(Mgr& mgr, const BufferPtr& buffer)
    {
        hccl::BufferKey<uintptr_t, u64> actualRegKey(
            buffer->GetAddr(), static_cast<uint64_t>(buffer->GetSize()));
        (void)mgr->AddWithoutCheck(actualRegKey, buffer);
    }

    // UnregisterMemory: IsAlias为true时，通过硬件句柄定位父buffer
    // hwHandleGetter(buffer) — 获取硬件句柄用于匹配
    template <typename Mgr, typename BufferPtr, typename BufferVec, typename HwHandleFn>
    static BufferPtr ResolveAliasParent(Mgr& mgr, const hccl::BufferKey<uintptr_t, u64>& ownKey,
        BufferPtr buffer, BufferVec& allBuffers, HwHandleFn&& hwHandleGetter)
    {
        auto hw = hwHandleGetter(buffer);
        auto findResult = mgr->Find(ownKey);
        if (findResult.first && hwHandleGetter(findResult.second.get()) == hw) {
            return findResult.second.get();
        }
        for (auto& ptr : allBuffers) {
            if (hwHandleGetter(ptr.get()) == hw) {
                return ptr.get();
            }
        }
        return buffer;
    }
};
}
#endif // REGED_MEM_MGR_H

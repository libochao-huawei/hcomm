/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef REGED_MEMS_H
#define REGED_MEMS_H

#include <memory>
#include <vector>
#include <unordered_map>
#include <mutex>
#include "rma_buffer_mgr.h"

namespace hcomm {
/**
 * @note 职责：用于通信设备EndPoint的注册内存信息管理，支持基于RmaBufferMgr类的重叠内存的检测报错等。
 */
class RegedMemMgr {
public:
    RegedMemMgr();
    ~RegedMemMgr();
    using RemoteRmaBufferMgr = RmaBufferMgr<BufferKey<uintptr_t, u64>, void*>; // (addr, size) handle 

    // 添加注册内存
    HcclResult Add(MemHandle handle, const MemRegion& region);

    // 移除注册内存
    HcclResult Remove(MemHandle handle);

    // 查找内存区域
    HcclResult Find(MemHandle handle, MemRegion* region);

    // 获取所有注册内存
    HcclResult GetAllMemory(std::vector<MemRegion>& regions);

    // 检查内存重叠
    bool CheckOverlap(const MemRegion& newRegion) const;

private:
    std::mutex mutex_{};
};
}

#endif // REGED_MEMS_H

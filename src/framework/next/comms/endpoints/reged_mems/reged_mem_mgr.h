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
#include "hccl_types.h"
#include "hcomm_res_defs.h"

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
 
    RdmaHandle rdmaHandle_{nullptr};
};
}

#endif // REGED_MEM_MGR_H

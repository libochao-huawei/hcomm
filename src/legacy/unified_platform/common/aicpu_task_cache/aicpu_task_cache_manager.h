/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef HCCLV2_AICPU_TASK_CACHE_MANAGER_H
#define HCCLV2_AICPU_TASK_CACHE_MANAGER_H

#include "aicpu_task_cache.h"

namespace Hccl {

class AicpuTaskCacheManager
{
public:
    static AicpuTaskCache aicpuTaskCache; // 多个通信域共享的全局变量 (仅声明)

    // 单个通信域aicpu kfc线程独立使用的全局线程变量 (仅声明)
    static thread_local bool isCacheMiss_;
    static thread_local AicpuTaskCacheEntry* cacheEntryPtr_;
};

} // namespace Hccl

#endif
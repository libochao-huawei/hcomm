/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef HCOMM_AICPU_TASK_CACHE_MANAGER_H
#define HCOMM_AICPU_TASK_CACHE_MANAGER_H

#include <string>

#include "aicpu_task_cache.h"

namespace hcomm {

class AicpuTaskCacheManager
{
public:
    // 注意: 由于存在全局线程变量, 这里不适用单例模式

    // 多个通信域共享的全局变量 (仅声明)
    static AicpuTaskCache aicpuTaskCache;

    // 单个通信域aicpu kfc线程独立使用的全局线程变量 (仅声明)
    // Case 1: isCacheMiss == true && cacheEntryPtr == nullptr -> aicpu task cache未命中且容量已满, 不进行cache
    // Case 2: isCacheMiss == true && cacheEntryPtr != nullptr -> aicpu task cache未命中, 创建新的cache entry
    // Case 3: isCacheMiss == false && cacheEntryPtr != nullptr -> aicpu task cache命中, 使用已有cache entry刷新下发task
    static thread_local std::string cacheTag;
    static thread_local bool isCacheMiss;
    static thread_local AicpuTaskCacheEntry* cacheEntryPtr;
};

} // namespace hcomm

#endif
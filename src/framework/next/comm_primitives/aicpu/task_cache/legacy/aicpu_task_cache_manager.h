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

#include <shared_mutex>
#include <unordered_map>

#include "aicpu_task_cache.h"

namespace Hccl {

class AicpuTaskCacheManager
{
public:
    using CacheManagerHashMap = std::unordered_map<std::string, AicpuTaskCache*>;

    static CacheManagerHashMap cacheManagerHashMap; // 多个通信域共享的全局变量 (仅声明)

    // 通信域初始化时插入cache
    static HcclResult AddCache(const std::string& commTag);

    // 通信域运行过程中查找cache
    static HcclResult FindCache(const std::string& commTag, AicpuTaskCache*& cachePtr) const;

    // 通信域销毁时释放cache (注意: 释放cache的动作在cache manager外进行, 减少锁竞争)
    static HcclResult PopCache(const std::string& commTag, AicpuTaskCache*& cachePtr);
private:
    static mutable std::shared_time_mutex cacheMtx_; // 多个通信域共享的超时读写锁
};

} // namespace Hccl

#endif
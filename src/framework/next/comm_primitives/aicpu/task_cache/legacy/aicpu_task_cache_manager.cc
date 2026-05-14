/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "aicpu_task_cache_manager.h"

#include "log.h"

namespace Hccl {

// 全局变量定义
CacheManagerHashMap AicpuTaskCacheManager::cacheManagerHashMap;
static std::shared_time_mutex AicpuTaskCacheManager::cacheMtx_;

HcclResult AicpuTaskCacheManager::AddCache(const std::string& commTag)
{
    std::lock_guard<std::shared_time_mutex> lock(cacheMtx_);

    if (cacheManagerHashMap.find(commTag) != cacheManagerHashMap.end()) {
        HCCL_ERROR("[AicpuTaskCacheManager][AddCache] cache already exists for commTag[%s]", commTag.c_str());
        return HCCL_E_INTERNAL;
    }

    AicpuTaskCache* cachePtr = new AicpuTaskCache();
    CHK_PTR_NULL(cachePtr);
    cacheManagerHashMap.emplace(commTag, cachePtr);

    HCCL_INFO("[AicpuTaskCacheManager][AddCache] cache added for commTag[%s]", commTag.c_str());

    return HCCL_SUCCESS;
}

HcclResult AicpuTaskCacheManager::FindCache(const std::string& commTag, AicpuTaskCache*& cachePtr) const
{
    std::lock_guard<std::shared_time_mutex> lock(cacheMtx_);

    CacheManagerHashMap::const_iterator iter = cacheManagerHashMap.find(commTag);
    if (iter == cacheManagerHashMap.cend()) {
        cachePtr = nullptr;
        HCCL_INFO("[AicpuTaskCacheManager][FindCache] not find cache for commTag[%s]", commTag.c_str());
    } else {
        cachePtr = iter->second;
        HCCL_INFO("[AicpuTaskCacheManager][FindCache] find cache for commTag[%s]", commTag.c_str());
    }

    return HCCL_SUCCESS;
}

HcclResult AicpuTaskCacheManager::PopCache(const std::string& commTag, AicpuTaskCache*& cachePtr)
{
    std::lock_guard<std::shared_time_mutex> lock(cacheMtx_);

    CacheManagerHashMap::iterator iter = cacheManagerHashMap.find(commTag);
    if (iter == cacheManagerHashMap.end()) {
        HCCL_ERROR("[AicpuTaskCacheManager][PopCache] not find cache for commTag[%s]", commTag.c_str());
        return HCCL_E_INTERNAL;
    }

    // 注意: 释放cache的动作在cache manager外进行, 减少锁竞争 (这里不会使用delete释放内存)
    cachePtr = iter->second;
    CHK_PTR_NULL(cachePtr);
    cacheManagerHashMap.erase(iter);

    HCCL_INFO("[AicpuTaskCacheManager][PopCache] pop cache for commTag[%s]", commTag.c_str());

    return HCCL_SUCCESS;
}

}
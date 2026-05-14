/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "aicpu_task_cache.h"

#include "aicpu_task_cache_utils.h"
#include "log.h"

namespace Hccl {

constexpr size_t AICPU_TASK_CACHE_CAPACITY = 500;

AicpuTaskCache::AicpuTaskCache()
{
    HCCL_RUN_INFO("[AicpuTaskCache][AicpuTaskCache] create aicpu task cache at 0x%016llx", this);
}

AicpuTaskCache::~AicpuTaskCache()
{
    uint64_t totalBytes = 0;
    for (CacheHashMap::const_iterator constIter = cacheHashMap_.cbegin(); constIter != cacheHashMap_.cend();
         ++constIter) {
        AicpuTaskCacheEntry* entryPtr = constIter->second;
        if (UNLIKELY(entryPtr == nullptr)) {
            HCCL_ERROR("[AicpuTaskCache][~AicpuTaskCache] entryPtr is nullptr");
            continue;
        }

        totalBytes += entryPtr->GetBytes();

        delete entryPtr;
        entryPtr = nullptr;
    }

    HCCL_RUN_INFO("[AicpuTaskCache][~AicpuTaskCache] release aicpu task cache at 0x%016llx, totalBytes[%llu]",
        this, totalBytes);
}

bool AicpuTaskCache::IsCacheFull() const
{
    std::shared_lock<std::shared_time_mutex> lock(cacheMtx_); // 读锁

    const size_t curSize = cacheHashMap_.size();
    if (curSize >= AICPU_TASK_CACHE_CAPACITY) {
        HCCL_INFO("[AicpuTaskCache][IsCacheFull] cacheHashMap_.size[%u] >= capacity[%u]", cacheHashMap_.size(),
            AICPU_TASK_CACHE_CAPACITY);
        return true;
    }
    return false;
}

HcclResult AicpuTaskCache::FindEntry(const std::string& cacheTag, AicpuTaskCacheEntry** entryPtrPtr) const
{
    std::shared_lock<std::shared_time_mutex> lock(cacheMtx_); // 读锁

    CHK_PTRPTR_NULL(entryPtrPtr);

    CacheHashMap::const_iterator constIter = cacheHashMap_.find(cacheTag);
    if (constIter != cacheHashMap_.cend()) {
        *entryPtrPtr = constIter->second;
        CHK_PTR_NULL(*entryPtrPtr);
        HCCL_INFO("[AicpuTaskCache][FindEntry] find cache entry for cacheTag[%s]", cacheTag.c_str());
    } else {
        *entryPtrPtr = nullptr;
        HCCL_INFO("[AicpuTaskCache][FindEntry] not find cache entry for cacheTag[%s]", cacheTag.c_str());
    }

    return HCCL_SUCCESS;
}

HcclResult AicpuTaskCache::AddEntry(const std::string& cacheTag, AicpuTaskCacheEntry** entryPtrPtr)
{
    std::lock_guard<std::shared_time_mutex> lock(cacheMtx_); // 写锁

    CHK_PTRPTR_NULL(entryPtrPtr);

    CacheHashMap::iterator iter = cacheHashMap_.find(cacheTag);
    CHK_PRT_RET(iter != cacheHashMap_.end(),
        HCCL_ERROR("[AicpuTaskCache][AddEntry] cache entry for cacheTag[%s] exists", cacheTag.c_str()),
        HCCL_E_INTERNAL);

    // 初始化new cache entry
    AicpuTaskCacheEntry* newCacheEntryPtr = (new (std::nothrow) AicpuTaskCacheEntry());
    CHK_PTR_NULL(newCacheEntryPtr);

    // 添加到cache
    std::pair<CacheHashMap::iterator, bool> insertResult = cacheHashMap_.emplace(cacheTag, newCacheEntryPtr);
    if (UNLIKELY(!(insertResult.second))) {
        HCCL_ERROR("[AicpuTaskCache][AddEntry] fail to insert a new cache entry for cacheTag[%s]",
            cacheTag.c_str());

        delete newCacheEntryPtr;
        newCacheEntryPtr = nullptr;

        return HCCL_E_INTERNAL;
    } else {
        HCCL_INFO("[AicpuTaskCache][AddEntry] add a new cache entry for cacheTag[%s] cacheHashMap_.size[%u]",
            cacheTag.c_str(), cacheHashMap_.size());

        iter = insertResult.first;
        *entryPtrPtr = iter->second;
        CHK_PTR_NULL(*entryPtrPtr);
    }

    return HCCL_SUCCESS;
}

HcclResult AicpuTaskCache::ClearEntry(const std::string& cacheTag)
{
    std::lock_guard<std::shared_time_mutex> lock(cacheMtx_); // 写锁
    
    CacheHashMap::iterator iter = cacheHashMap_.find(cacheTag);
    if (iter != cacheHashMap_.end()) {
        HCCL_INFO("[AicpuTaskCache][ClearEntry] clear cache entry for cacheTag[%s]", cacheTag.c_str());

        AicpuTaskCacheEntry* entryPtr = iter->second;
        CHK_PTR_NULL(entryPtr);
        delete entryPtr;
        entryPtr = nullptr;
        iter->second = nullptr;

        cacheHashMap_.erase(iter);
    } else {
        HCCL_INFO("[AicpuTaskCache][ClearEntry] not find cache entry for cacheTag[%s]", cacheTag.c_str());
    }

    return HCCL_SUCCESS;
}

} // namespace Hccl

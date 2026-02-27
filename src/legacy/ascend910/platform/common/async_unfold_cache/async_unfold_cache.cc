/*
 * This program is free software, you can redistribute it and/or modify it.
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "async_unfold_cache.h"

#include "aicpu_cache_utils.h"

namespace hccl {

    // 注意: 当前异步展开只支持提前展开一个算子, 如果要展开多个算子 (即增加capacity), AicpuAsyncUnfolder需要维护vector<MainSlaveStreamInfo>
    constexpr size_t ASYNC_UNFOLD_CACHE_CAPACITY = 1; // Cache entry数目的上限

    AsyncUnfoldCache::AsyncUnfoldCache() {
        // TODOOPT: 初始化时提前分配AsyncUnfoldKey和AsyncUnfoldCacheEntry内存, 避免动态分配

        HCCL_INFO("[AsyncUnfoldCache][AsyncUnfoldCache] create async unfold cache at 0x%016llx", this);
    }

    AsyncUnfoldCache::~AsyncUnfoldCache() {
        HCCL_INFO("[AsyncUnfoldCache][~AsyncUnfoldCache] release async unfold cache at 0x%016llx", this);

        for (AsyncUnfoldCacheHashMap::const_iterator constIter = asyncUnfoldCacheMap_.cbegin();
            constIter != asyncUnfoldCacheMap_.cend(); ++constIter) {
            AsyncUnfoldCacheEntry *entryPtr = constIter->second;

            // 不能使用CHK_PTR_NULL，因为会return HcclResult
            if (UNLIKELY(entryPtr == nullptr)) {
                HCCL_ERROR("[AsyncUnfoldCache][~] entryPtr is nullptr");
                continue;
            }

            // 释放当前cache entry
            delete entryPtr;
            entryPtr = nullptr;
        }
    }

    bool AsyncUnfoldCache::IsAsyncUnfoldCacheFull() const {
        if (curCacheSize_ >= ASYNC_UNFOLD_CACHE_CAPACITY) {
            HCCL_INFO("[AsyncUnfoldCache][IsAsyncUnfoldCacheFull] curCacheSize_[%u] >= capacity[%u]",
                curCacheSize_, ASYNC_UNFOLD_CACHE_CAPACITY);
            return true;
        }
        return false;
    }

    HcclResult AsyncUnfoldCache::FindAsyncUnfoldEntry(const AsyncUnfoldKey& key, AsyncUnfoldCacheEntry **entryPtrPtr) const
    {
        // TODOSSY: 性能打点
        FUNCTION_TRACE;

        // 检查入参
        CHK_PTRPTR_NULL(entryPtrPtr); // 检查指针, entryPtrPtr不应该是null, 但*entryPtrPtr应该是null

        AsyncUnfoldCacheHashMap::const_iterator constIter = asyncUnfoldCacheMap_.find(key);
        if (constIter != asyncUnfoldCacheMap_.cend()) {
            // 检查cache entry pointer
            *entryPtrPtr = constIter->second;
            CHK_PTR_NULL(*entryPtrPtr);

            HCCL_INFO("[AsyncUnfoldCache][FindAsyncUnfoldEntry] find async unfold cache entry for key[%s]",
                key.GetKeyString().c_str());
            
            // 校验key是否一致
            CHK_RET(key.Verify(constIter->first));
        } else {
            *entryPtrPtr = nullptr;

            HCCL_INFO("[AsyncUnfoldCache][FindAsyncUnfoldEntry] not find async unfold cache entry for key[%s]",
                key.GetKeyString().c_str());
        }

        return HCCL_SUCCESS;
    }

    HcclResult AsyncUnfoldCache::AddAsyncUnfoldEntry(const AsyncUnfoldKey& key, AsyncUnfoldCacheEntry **entryPtrPtr)
    {
        // TODOSSY: 性能打点
        FUNCTION_TRACE;

        // 检查入参
        CHK_PTRPTR_NULL(entryPtrPtr); // 检查指针, entryPtrPtr不应该是null, 但*entryPtrPtr应该是null

        AsyncUnfoldCacheHashMap::iterator iter = asyncUnfoldCacheMap_.find(key);

        // key不应该存在
        CHK_PRT_RET(iter != asyncUnfoldCacheMap_.end(),
            HCCL_ERROR("[AsyncUnfoldCache][AddAsyncUnfoldEntry] async unfold cache entry for key[%s] exists",
                key.GetKeyString().c_str()),
            HCCL_E_INTERNAL);

        // TODOOPT: 校验curCacheSize_是否超过capacity
        // TODOOPT: 根据curCacheSize_, 设置对应的key和cacheEntry
        // TODOOPT: 设置后，更新curCacheSize_;

        // 创建一个新的cache entry
        AsyncUnfoldCacheEntry *newCacheEntryPtr = (new (std::nothrow) AsyncUnfoldCacheEntry());
        CHK_PTR_NULL(newCacheEntryPtr);

        // 插入新的cache entry
        std::pair<AsyncUnfoldCacheHashMap::iterator, bool> insertResult = asyncUnfoldCacheMap_.emplace(key, newCacheEntryPtr);
        if (UNLIKELY(!(insertResult.second))) {
            HCCL_ERROR("[AsyncUnfoldCache][AddAsyncUnfoldEntry] fail to insert a new async unfold cache entry for key[%s]",
                key.GetKeyString().c_str());

            delete newCacheEntryPtr;
            newCacheEntryPtr = nullptr;

            return HCCL_E_INTERNAL;
        } else {
            HCCL_INFO("[AsyncUnfoldCache][AddAsyncUnfoldEntry] add a new async unfold cache entry for key[%s] asyncUnfoldCacheMap_.size[%u]",
                key.GetKeyString().c_str(), asyncUnfoldCacheMap_.size());

            iter = insertResult.first;
            *entryPtrPtr = iter->second;
            CHK_PTR_NULL(*entryPtrPtr);
        }

        return HCCL_SUCCESS;
    }

    HcclResult AsyncUnfoldCache::ClearAsyncUnfoldEntry(const AsyncUnfoldKey& key)
    {
        // TODOSSY: 性能打点
        FUNCTION_TRACE;

        // TODOOPT: curCacheSize_ - 1, 重置尾部的key和cache entry
        
        AsyncUnfoldCacheHashMap::iterator iter = asyncUnfoldCacheMap_.find(key);
        if (iter != asyncUnfoldCacheMap_.end()) { // cache entry存在
            HCCL_INFO("[AsyncUnfoldCache][ClearAsyncUnfoldEntry] clear async unfold cache entry for key[%s]",
                key.GetKeyString().c_str());

            // 释放cache entry对应的内存
            AsyncUnfoldCacheEntry *entryPtr = iter->second;
            CHK_PTR_NULL(entryPtr);
            delete entryPtr;
            entryPtr = nullptr;
            iter->second = nullptr;

            // 清理cache entry
            asyncUnfoldCacheMap_.erase(iter);
        } else { // cache entry不存在
            HCCL_INFO("[AsyncUnfoldCache][ClearAsyncUnfoldEntry] not find async unfold cache entry for key [%s]",
                key.GetKeyString().c_str());
        }

        return HCCL_SUCCESS;
    }

    HcclResult AsyncUnfoldCache::ClearAll() {
        // TODOOPT: 重置curCacheSize_为0, 重置所有key和cache entry

        for (AsyncUnfoldCacheHashMap::iterator iter = asyncUnfoldCacheMap_.begin(); iter != asyncUnfoldCacheMap_.end();) {
            HCCL_INFO("[AsyncUnfoldCache][ClearAll] clear async unfold cache entry for key[%s]",
                iter->first.GetKeyString().c_str());

            // 备份iter的下一个位置 (注意: 不影响iter本身)
            AsyncUnfoldCacheHashMap::iterator nextIter = std::next(iter);

            // 释放cache entry对应的内存
            AsyncUnfoldCacheEntry *entryPtr = iter->second;
            CHK_PTR_NULL(entryPtr);
            delete entryPtr;
            entryPtr = nullptr;
            iter->second = nullptr;

            // 清理cache entry
            asyncUnfoldCacheMap_.erase(iter);

            // 更新iter为下一个位置
            iter = nextIter;
        }

        CHK_PRT_RET(asyncUnfoldCacheMap_.size() != 0,
            HCCL_ERROR("[AsyncUnfoldCache][ClearAll] asyncUnfoldCacheMap_.size[%u] != 0 after ClearAll",
                asyncUnfoldCacheMap_.size()),
            HCCL_E_INTERNAL);

        return HCCL_SUCCESS;
    }

} // namespace hccl
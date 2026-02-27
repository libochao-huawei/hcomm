/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "op_unfold_cache.h"

#include "aicpu_cache_utils.h"
#include "aicpu_hccl_sqcq.h"
#include "aicpu_hccl_sqcqv1.h"
#include "aicpu_hccl_sqcqv2.h"
#include "config_plf_log.h"
#include "log.h"

namespace hccl {

    constexpr size_t OP_UNFOLD_CACHE_CAPACITY = 500; // Cache entry数目的上限

    OpUnfoldCache::OpUnfoldCache() {
        HCCL_INFO("[OpUnfoldCache][OpUnfoldCache] create a cache at 0x%016llx for operator unfolding", this);
    }

    OpUnfoldCache::~OpUnfoldCache() {
        HCCL_INFO("[OpUnfoldCache][~OpUnfoldCache] release dynamically allocated entries from the cache at 0x%016llx", this);

        for (CacheHashMap::const_iterator constIter = cacheHashMap_.cbegin(); constIter != cacheHashMap_.cend(); ++constIter) {
            OpUnfoldCacheEntry *entryPtr = constIter->second;

            // 不能使用CHK_PTR_NULL，因为会return HcclResult
            if (UNLIKELY(entryPtr == nullptr)) {
                HCCL_ERROR("[OpUnfoldCache][~OpUnfoldCache] entryPtr is nullptr");
                continue;
            }

            // 释放当前cache entry
            delete entryPtr;
            entryPtr = nullptr;
        }
    }

    bool OpUnfoldCache::IsCacheFull() const {
        const size_t curSize = cacheHashMap_.size();
        if (curSize >= OP_UNFOLD_CACHE_CAPACITY) {
            HCCL_INFO("[OpUnfoldCache][IsCacheFull] cacheHashMap_.size[%u] >= capacity[%u]", cacheHashMap_.size(), OP_UNFOLD_CACHE_CAPACITY);
            return true;
        }
        return false;
    }

    HcclResult OpUnfoldCache::FindEntry(const OpUnfoldKey& key, OpUnfoldCacheEntry **entryPtrPtr) const {
        // 检查入参
        CHK_PTRPTR_NULL(entryPtrPtr); // 检查指针, entryPtrPtr不应该是null, 但*entryPtrPtr应该是null

        CacheHashMap::const_iterator constIter = cacheHashMap_.find(key);
        if (constIter != cacheHashMap_.cend()) {
            // 检查cache entry pointer
            *entryPtrPtr = constIter->second;
            CHK_PTR_NULL(*entryPtrPtr);

            HCCL_INFO("[OpUnfoldCache][FindEntry] find cache entry for key[%s]", key.GetKeyString().c_str());
        } else {
            *entryPtrPtr = nullptr;

            HCCL_INFO("[OpUnfoldCache][FindEntry] not find cache entry for key[%s]", key.GetKeyString().c_str());
        }

        return HCCL_SUCCESS;
    }

    HcclResult OpUnfoldCache::AddEntry(const OpUnfoldKey& key, const std::vector<OpUnfoldMemRange>& userInputMemRanges, const std::vector<OpUnfoldMemRange>& userOutputMemRanges, OpUnfoldCacheEntry **entryPtrPtr) {
        // 检查入参
        CHK_PTRPTR_NULL(entryPtrPtr); // 检查指针, entryPtrPtr不应该是null, 但*entryPtrPtr应该是null

        CacheHashMap::iterator iter = cacheHashMap_.find(key);

        // key不应该存在
        CHK_PRT_RET(iter != cacheHashMap_.end(), HCCL_ERROR("[OpUnfoldCache][AddEntry] cache entry for key[%s] exists at 0x%016llx", key.GetKeyString().c_str(), this), HCCL_E_INTERNAL);

        // 创建一个新的cache entry
        OpUnfoldCacheEntry *newCacheEntryPtr = (new (std::nothrow) OpUnfoldCacheEntry(userInputMemRanges, userOutputMemRanges));
        CHK_PTR_NULL(newCacheEntryPtr);

        // 插入新的cache entry
        std::pair<CacheHashMap::iterator, bool> insertResult = cacheHashMap_.emplace(key, newCacheEntryPtr);
        if (UNLIKELY(!(insertResult.second))) {
            HCCL_ERROR("[OpUnfoldCache][AddEntry] fail to insert a new cache entry for key[%s]", key.GetKeyString().c_str());

            delete newCacheEntryPtr;
            newCacheEntryPtr = nullptr;

            return HCCL_E_INTERNAL;
        } else {
            HCCL_INFO("[OpUnfoldCache][AddEntry] add a new cache entry for key[%s] cacheHashMap_.size[%u]", key.GetKeyString().c_str(), cacheHashMap_.size());

            iter = insertResult.first;
            *entryPtrPtr = iter->second;
            CHK_PTR_NULL(*entryPtrPtr);
        }

        return HCCL_SUCCESS;
    }

    HcclResult OpUnfoldCache::ClearEntry(const OpUnfoldKey& key) {
        CacheHashMap::iterator iter = cacheHashMap_.find(key);
        if (iter != cacheHashMap_.end()) { // cache entry存在
            HCCL_INFO("[OpUnfoldCache][ClearEntry] clear cache entry for key[%s]", key.GetKeyString().c_str());

            // 释放cache entry对应的内存
            OpUnfoldCacheEntry *entryPtr = iter->second;
            CHK_PTR_NULL(entryPtr);
            delete entryPtr;
            entryPtr = nullptr;
            iter->second = nullptr;

            // 清理cache entry
            cacheHashMap_.erase(iter);
        } else { // cache entry不存在
            HCCL_INFO("[OpUnfoldCache][ClearEntry] not find cache entry for key [%s]", key.GetKeyString().c_str());
        }

        return HCCL_SUCCESS;
    }

    HcclResult OpUnfoldCache::ClearEntryForAlltoallv() {
        // 清理所有与alltoallv类算子相关的cache entry
        for (CacheHashMap::iterator iter = cacheHashMap_.begin(); iter != cacheHashMap_.end();) {
            // 先备份iter的下一个位置 (使用std::next避免修改iter本身)
            CacheHashMap::iterator nextIter = std::next(iter);

            // 清理alltoallv类算子的cache entry
            const HcclCMDType opType = iter->first.opType;
            if (opType == HcclCMDType::HCCL_CMD_ALLTOALLV || opType == HcclCMDType::HCCL_CMD_ALLTOALLVC) {
                CHK_RET(ClearEntry(iter->first));
            }
            
            // 注意: ClearEntry后不能再访问iter (已经从cacheHashMap_中被erase了)

            // 推进iter到下一个位置
            iter = nextIter;
        }

        return HCCL_SUCCESS;
    }
} // namespace hccl
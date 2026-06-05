/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "aicpu_task_cache_c_adpt.h"

#include "aicpu_task_cache_manager.h"
#include "log.h"

using namespace hcomm;

HcommResult HcommAicpuTsCacheLookup(const char* tag, bool* isCacheMiss)
{
    CHK_PTR_NULL(tag);
    CHK_PTR_NULL(isCacheMiss);
    
    // 记录当前tag, 后续校验tag一致性
    AicpuTaskCacheManager::cacheTag = tag;
    
    // 注意: 相同tag的算子一定不会被多个aicpu kernel threads同时展开, 否则后续threads可能命中第一个thread插入的不完整的cache entry
    // 例如: tag中有commId时, 相同tag的算子一定属于同一个通信域, 必定按序展开
    // 因此, FindEntry和AddEntry不需要统一成单个接口, 即FindEntry后, 当前tag的缓存状态不会被其他threads改变, 可以直接AddEntry

    // 查询tag对应的cache entry
    AicpuTaskCacheManager::cacheEntryPtr = nullptr;
    CHK_RET(AicpuTaskCacheManager::aicpuTaskCache.FindEntry(tag, &AicpuTaskCacheManager::cacheEntryPtr));

    // 判断并记录是否为cache miss
    AicpuTaskCacheManager::isCacheMiss = (AicpuTaskCacheManager::cacheEntryPtr == nullptr);
    *isCacheMiss = AicpuTaskCacheManager::isCacheMiss;

    // 如果是cache miss, 尝试添加cache entry
    // 注意: 如果aicpu task cache容量已满, 不会添加新的cache entry, AicpuTaskCacheManager::cacheEntryPtr被设置为nullptr
    if (*isCacheMiss) {
        CHK_RET(AicpuTaskCacheManager::aicpuTaskCache.AddEntry(tag, &AicpuTaskCacheManager::cacheEntryPtr));
    }

    return HCCL_SUCCESS;
}

HcommResult HcommAicpuTsTaskCacheSubmit(const char* tag, void** addrs, uint64_t* sizes, uint32_t count)
{
    CHK_PTR_NULL(tag);
    CHK_PTR_NULL(addrs);
    CHK_PTR_NULL(sizes);

    // 校验tag一致性 (即Submit对应的tag一定与Lookup对应的tag一致)
    CHK_PRT_RET(AicpuTaskCacheManager::cacheTag != tag,
        HCCL_ERROR("[HcommAicpuTsTaskCacheSubmit] submit's tag[%s] != lookup's tag[%s]",
            tag, AicpuTaskCacheManager::cacheTag.c_str()),
        HCCL_E_PARA);

    // 注意: 如果cache miss且容量已满, do nothing
    if (AicpuTaskCacheManager::isCacheMiss && AicpuTaskCacheManager::cacheEntryPtr != nullptr) { // cache miss且容量未满
        // 保存地址信息到cache entry
        CHK_RET(AicpuTaskCacheManager::cacheEntryPtr->SubmitCacheEntry(addrs, sizes, count));

        // 更新cache空间消耗
        const uint64_t entryBytes = AicpuTaskCacheManager::cacheEntryPtr->GetEntryBytes();
        CHK_RET(AicpuTaskCacheManager::aicpuTaskCache.IncCacheBytes(entryBytes));
    } else if (!AicpuTaskCacheManager::isCacheMiss) { // cache hit
        CHK_PTR_NULL(AicpuTaskCacheManager::cacheEntryPtr);

        // 刷新并下发task
        CHK_RET(AicpuTaskCacheManager::cacheEntryPtr->RefreshAndLaunch(addrs, sizes, count));
    }

    // 重置cache上下文
    AicpuTaskCacheManager::cacheTag = "";
    AicpuTaskCacheManager::isCacheMiss = false;
    AicpuTaskCacheManager::cacheEntryPtr = nullptr;

    return HCCL_SUCCESS;
}

HcommResult HcommAicpuTsTaskCacheClear(const char* tag)
{
    CHK_PTR_NULL(tag);

    // 清除tag对应的cache entry (if any)
    CHK_RET(AicpuTaskCacheManager::aicpuTaskCache.ClearEntry(tag));

    return HCCL_SUCCESS;
}
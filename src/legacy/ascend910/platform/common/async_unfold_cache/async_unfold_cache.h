/*
 ,* This program is free software, you can redistribute it and/or modify it.
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef __ASYNC_UNFOLD_CACHE_H__
#define __ASYNC_UNFOLD_CACHE_H__

#include <unordered_map>

#include "async_unfold_key.h"
#include "async_unfold_cache_entry.h"

namespace hccl {

// 注意: 与OpUnfoldCache不同, AsyncUnfoldCache用于缓存AICPU异步展开后续算子的展开结果, 后续算子执行时直接下发
// 而OpUnfoldCache用于缓存AICPU同步展开当前算子的展开结果, 后续相同算子执行时需要先刷新缓存再下发
class AsyncUnfoldCache {
public:
    AsyncUnfoldCache();
    ~AsyncUnfoldCache();

    // 判断异步展开cache容量是否已满
    bool IsAsyncUnfoldCacheFull() const;

    // 查看是否存在key对应的cache entry (如果不存在, *entryPtrPtr会被置为空)
    HcclResult FindAsyncUnfoldEntry(const AsyncUnfoldKey& key, AsyncUnfoldCacheEntry **entryPtrPtr) const;

    // 插入新的cache entry
    HcclResult AddAsyncUnfoldEntry(const AsyncUnfoldKey& key, AsyncUnfoldCacheEntry **entryPtrPtr);

    // 如果key存在对应的cache entry, 清理entry
    HcclResult ClearAsyncUnfoldEntry(const AsyncUnfoldKey& key);

    // 清理所有entry
    HcclResult ClearAll();
private:
    using AsyncUnfoldCacheHashMap = std::unordered_map<AsyncUnfoldKey, AsyncUnfoldCacheEntry *>;

    AsyncUnfoldCacheHashMap asyncUnfoldCacheMap_;
    uint32_t curCacheSize_ = 0;
};

} // namespace hccl

#endif // __ASYNC_UNFOLD_CACHE_H__
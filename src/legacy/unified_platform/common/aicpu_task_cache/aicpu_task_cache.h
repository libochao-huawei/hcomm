/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef HCCLV2_AICPU_TASK_CACHE_H
#define HCCLV2_AICPU_TASK_CACHE_H

#include <unordered_map>

#include "aicpu_task_cache_entry.h"

namespace Hccl {

class AicpuTaskCache {
public:
    explicit AicpuTaskCache();
    ~AicpuTaskCache();

    bool IsCacheFull() const;

    HcclResult FindEntry(const std::string& opTag, AicpuTaskCacheEntry** entryPtrPtr) const;
    HcclResult AddEntry(const std::string& opTag, const AicpuTaskMemRange& localUserInputMemRange,
        const AicpuTaskMemRange& localUserOutputMemRange, AicpuTaskCacheEntry** entryPtrPtr);
    HcclResult ClearEntry(const std::string& opTag);

private:
    using CacheHashMap = std::unordered_map<std::string, AicpuTaskCacheEntry*>;

    CacheHashMap cacheHashMap_;
};

} // namespace Hccl

#endif // HCCLV2_AICPU_TASK_CACHE_H

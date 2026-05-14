/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "aicpu_task_cache_key.h"

#include <sstream>

namespace hcomm {

HcclResult AicpuTaskCacheKey::ParseAicpuTaskCacheTag(const std::string& cacheTag, uint64_t& inputSize, uint64_t& outputSize)
{
    std::istringstream iss(cacheTag);
    char delimiter = '-';
    iss >> inputSize >> delimiter >> outputSize;

    HCCL_INFO("[AicpuTaskCacheKey][ParseAicpuTaskCacheTag] cacheTag[%s] to inputSize[%llu] outputSize[%llu]",
        cacheTag.c_str(), inputSize, outputSize);
        
    return HCCL_SUCCESS;
}

} // namespace hcomm
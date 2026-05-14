/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef AICPU_TASK_CACHE_C_ADPT_H
#define AICPU_TASK_CACHE_C_ADPT_H

#include "hcomm_res_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

// 查询aicpu task cache: cache miss设置isCacheMiss为true, cache hit设置为false
HcommResult HcommAicpuTsCacheLookup(const char* tag, bool* isCacheMiss);

// 提交aicpu task cache: cache miss缓存地址信息, cache hit刷新task并下发
HcommResult HcommAicpuTsTaskCacheSubmit(const char* tag, void** addrs, uint32_t count);

// 清理aicpu task cache: 清除指定tag的cache entry
HcommResult HcommAicpuTsTaskCacheClear(const char* tag);

#ifdef __cplusplus
}
#endif

#endif

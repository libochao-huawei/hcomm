/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

// 为确保Scatter正常执行打的临时桩，需要适时移除

#include <cstdio>
#include <cstring>

#include "hccl/dtype_common.h"
#include "sim_log.h"

#define SCATTER_STUB_ERROR(format, ...) HCCL_VM_ERROR("[SCATTER_STUB]" format, ##_VA_ARGS__)
#define SCATTER_STUB_DEBUG(format, ...) HCCL_VM_DEBUG("[SCATTER_STUB]" format, ##_VA_ARGS__)
#define SCATTER_STUB_INFO(format, ...)  HCCL_VM_INFO("[SCATTER_STUB]" format, ##__VA_ARGS__)
#define SCATTER_STUB_WARN(format, ...)  HCCL_VM_WARN("[SCATTER_STUB]" format, ##__VA_ARGS__)
#define SCATTER_STUB_TRACE(format, ...) HCCL_VM_TRACE("[SCATTER_STUB]" format, ##__VA_ARGS__)

namespace ops_hccl {
bool RunIndependentOpExpansion(DevType deviceType)
{
    (void) deviceType;
    SCATTER_STUB_INFO("return true");
    return true;
}

bool IsAiCpuMode(DevType deviceType, u32 rankSize)
{
    (void) deviceType;
    (void) rankSize;
    const char* env = getenv("HCCL_OP_EXPANSION_MODE");
    SCATTER_STUB_INFO("env = {}", env);
    // 判断是否是AI_CPU
    if (env != nullptr && strcmp(env, "AI_CPU") == 0) {
        return true;
    }
    return false;
}
}

/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef HCCLINC_ADAPTER_ERROR_MANAGER_H
#define HCCLINC_ADAPTER_ERROR_MANAGER_H

#include <string>
#include "log.h"
#include "adapter_error_manager_pub.h"
#include "base/err_msg.h"
#include "base/err_mgr.h"

// 定义原始结构体（来自第二个文件）
struct Context {
    uint64_t work_stream_id;
    uint64_t reserved[7] = {0};
};

// ErrContext 类型：优先使用 error_message::ErrorManagerContext（如果存在）
// 否则使用 struct Context
#ifdef ERROR_MESSAGE_CONTEXT_DEFINED
using ErrContext = error_message::ErrorManagerContext;
#else
using ErrContext = struct Context;
#endif

#ifdef __cplusplus
extern "C" {
#endif

// 使用 weak 属性，允许外部覆盖实现
ErrContext __attribute__((weak)) hrtErrMGetErrorContext(void);
void __attribute__((weak)) hrtErrMSetErrorContext(ErrContext error_context);

#ifdef __cplusplus
}
#endif

#endif // HCCLINC_ADAPTER_ERROR_MANAGER_H
/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <string>
#include "adapter_error_manager_pub.h"
#include "adapter_error_manager.h" 
#include "base/err_msg.h"
#include "base/err_mgr.h"

ErrContext hrtErrMGetErrorContext(void)
{
    error_message::ErrorManagerContext sdk_ctx = error_message::GetErrMgrContext();
    
    ErrContext local_ctx;
    local_ctx.work_stream_id = sdk_ctx.work_stream_id;
    // 复制 reserved 数组
    memcpy_s(local_ctx.reserved, sizeof(local_ctx.reserved), 
        sdk_ctx.reserved, sizeof(sdk_ctx.reserved));
    
    return local_ctx;
}

void hrtErrMSetErrorContext(ErrContext error_context)
{
    error_message::ErrorManagerContext sdk_ctx;
    sdk_ctx.work_stream_id = error_context.work_stream_id;
    // 复制 reserved 数组
    memcpy_s(sdk_ctx.reserved, sizeof(sdk_ctx.reserved),
        error_context.reserved, 7 * sizeof(error_context.reserved[0]));
    
    error_message::SetErrMgrContext(sdk_ctx);
}
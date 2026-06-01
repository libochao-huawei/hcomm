/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * ... (License header) ...
 */

#include <string>
#include "log.h"
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
    for (int i = 0; i < 7; ++i) {
        local_ctx.reserved[i] = sdk_ctx.reserved[i];
    }
    
    return local_ctx;
}

void hrtErrMSetErrorContext(ErrContext error_context)
{
    error_message::ErrorManagerContext sdk_ctx;
    sdk_ctx.work_stream_id = error_context.work_stream_id;
    // 复制 reserved 数组
    for (int i = 0; i < 7; ++i) {
        sdk_ctx.reserved[i] = error_context.reserved[i];
    }
    
    error_message::SetErrMgrContext(sdk_ctx);
}
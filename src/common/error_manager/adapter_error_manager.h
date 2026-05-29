/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef HCCL_INC_ADAPTER_ERROR_MANAGER_H
#define HCCL_INC_ADAPTER_ERROR_MANAGER_H

#include <string>
#include "log.h"
#include "adapter_error_manager_pub.h"

// 根据编译目标平台或配置，决定 ErrContext 的具体类型
#ifdef USE_HOST_ERROR_MANAGER
    // Host 端：使用完整的错误管理上下文
    #include "base/err_msg.h"
    #include "base/err_mgr.h"
    using ErrContext = error_message::ErrorManagerContext;
    
    // Host 端通常为强符号，因为必须链接到具体的错误管理实现
    ErrContext hrtErrMGetErrorContext(void);
    void hrtErrMSetErrorContext(ErrContext error_context);
#else
    // Device/Legacy/HCCD 端：使用简化的上下文结构
    using ErrContext = struct Context {
        uint64_t work_stream_id;
        uint64_t reserved[7] = {0};
    };

    // 这些端可能允许弱符号，以便在没有具体实现时不报错，或允许用户自定义
    ErrContext __attribute__((weak)) hrtErrMGetErrorContext(void);
    void __attribute__((weak)) hrtErrMSetErrorContext(ErrContext error_context);
#endif

#endif  // HCCL_INC_ADAPTER_ERROR_MANAGER_H
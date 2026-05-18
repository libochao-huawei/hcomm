#ifndef HCCLINC_ADAPTER_ERROR_MANAGER_H
#define HCCLINC_ADAPTER_ERROR_MANAGER_H

#include <string>
#include "log.h"
#include "adapter_error_manager_pub.h"

// 优先包含新框架头文件
#if __has_include("base/err_msg.h")
    #include "base/err_msg.h"
    #include "base/err_mgr.h"
    using ErrContext = error_message::ErrorManagerContext;
#else
    // 降级到旧定义
    struct Context {
        uint64_t work_stream_id;
        uint64_t reserved[7] = {0};
    };
    using ErrContext = struct Context;
#endif

#ifdef __cplusplus
extern "C" {
#endif

ErrContext hrtErrMGetErrorContext(void);
void hrtErrMSetErrorContext(ErrContext error_context);

#ifdef __cplusplus
}
#endif

#endif
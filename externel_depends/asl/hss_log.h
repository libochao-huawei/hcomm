/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef HSS_LOG_H
#define HSS_LOG_H

#include <unistd.h>
#include <sys/syscall.h>
#include <dlog_pub.h>

namespace Hss {
#define LIKELY(x) (static_cast<bool>(__builtin_expect(static_cast<bool>(x), 1)))
#define UNLIKELY(x) (static_cast<bool>(__builtin_expect(static_cast<bool>(x), 0)))

#define HSS_ERR(fmt, args...)  dlog_error(static_cast<int>(HSS) | DEBUG_LOG_MASK, "tid:%d,%s(%d) : " fmt "\r\n", \
    syscall(__NR_gettid), __func__, __LINE__, ##args)
#define HSS_WARN(fmt, args...) dlog_warn(static_cast<int>(HSS) | DEBUG_LOG_MASK, "tid:%d,%s(%d) : " fmt "\r\n", \
    syscall(__NR_gettid), __func__, __LINE__, ##args)
#define HSS_INFO(fmt, args...) dlog_info(static_cast<int>(HSS) | DEBUG_LOG_MASK, "tid:%d,%s(%d) : " fmt "\r\n", \
    syscall(__NR_gettid), __func__, __LINE__, ##args)
#define HSS_DBG(fmt, args...)  dlog_debug(static_cast<int>(HSS) | DEBUG_LOG_MASK, "tid:%d,%s(%d) : " fmt "\r\n", \
    syscall(__NR_gettid), __func__, __LINE__, ##args)
#define HSS_EVENT(fmt, args...)  dlog_event(static_cast<int>(HSS) | RUN_LOG_MASK, "tid:%d,%s(%d) : " fmt "\r\n", \
    syscall(__NR_gettid), __func__, __LINE__, ##args)

#define TOOL_PRINT_INFO(fmt, args...)      \
    do {                                   \
        fprintf(stdout, fmt "\n", ##args); \
        HSS_INFO(fmt "\n", ##args);        \
    } while (0)

#define TOOL_PRINT_ERR(fmt, args...)       \
    do {                                   \
        fprintf(stderr, fmt "\n", ##args); \
        HSS_ERR(fmt "\n", ##args);         \
    } while (0)

#define MAKE_SHARED_EXECEPTION_CATCH(expression, retExp)                    \
    do {                                                                    \
        try {                                                               \
            expression;                                                     \
        } catch (std::exception& e) {                                       \
            HSS_ERR("make_shared failed, exception caught:%s", e.what()); \
            retExp;                                                         \
        }                                                                   \
    } while (0)

#define MAKE_UNIQUE_EXECEPTION_CATCH(expression, retExp)                    \
    do {                                                                    \
        try {                                                               \
            expression;                                                     \
        } catch (std::exception& e) {                                       \
            HSS_ERR("make_unique failed, exception caught:%s", e.what()); \
            retExp;                                                         \
        }                                                                   \
    } while (0)

#ifndef NEW_NOTHROW
#define NEW_NOTHROW(pointer, constructor, retExp)        \
    do {                                                 \
        pointer = new(std::nothrow) constructor;         \
        if (pointer == nullptr) {                        \
            HSS_ERR("Memory application failed.");     \
            retExp;                                      \
        }                                                \
    } while (0)
#endif

#define NEW_EXECEPTION_CATCH(expression, retExp)                            \
    do {                                                                    \
        try {                                                               \
            expression;                                                     \
        } catch (std::exception& e) {                                       \
            HSS_ERR("new failed, exception caught:%s", e.what()); \
            retExp;                                                         \
        }                                                                   \
    } while (0)
}

#endif

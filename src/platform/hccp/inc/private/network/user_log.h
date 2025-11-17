/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef USER_LOG_H
#define USER_LOG_H

#include <stdio.h>
#include <pwd.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <dlog_pub.h>
#define submodule_ccu "CCU"

#define CHK_PRT_RETURN(result, exeLog, ret) \
    do {                                      \
        if (result) {                         \
            exeLog;                           \
            return (ret);                       \
        }                                     \
    } while (0)

#define DEBUG_LEVEL 0
#define INFO_LEVEL 1
#define WARN_LEVEL 2
#define ERROR_LEVEL 3
#define EVENT_LEVEL 16

#ifdef DRV_HOST
#include "drv_log_user.h"
#define roce_err(fmt, ...)    DRV_ERR(HAL_MODULE_TYPE_NET, fmt, ##__VA_ARGS__)
#define roce_warn(fmt, ...)   DRV_WARN(HAL_MODULE_TYPE_NET, fmt, ##__VA_ARGS__)
#define roce_info(fmt, ...)   DRV_INFO(HAL_MODULE_TYPE_NET, fmt, ##__VA_ARGS__)
#define roce_dbg(fmt, ...)    DRV_DEBUG(HAL_MODULE_TYPE_NET, fmt, ##__VA_ARGS__)
#define roce_run_info(fmt, ...)  DRV_NOTICE(HAL_MODULE_TYPE_NET, fmt, ##__VA_ARGS__)
#else
#ifdef LOG_HOST
/* fix slog compatibility issue: rs will be packaged into runtime/opp and deployed on the device */
#ifdef OPEN_BUILD_PROJECT
int32_t CheckLogLevel(int32_t moduleId, int32_t logLevel) __attribute((weak));
int32_t CheckLogLevelForC(int32_t moduleId, int32_t logLevel) __attribute((weak));
void DlogRecordForC(int32_t moduleId, int32_t level, const char *fmt, ...) __attribute((weak));
void DlogRecord(int32_t moduleId, int32_t level, const char *fmt, ...) __attribute((weak));
#define HCCPDlogForC(moduleId, level, fmt, ...) do { \
    DlogRecord(moduleId, level, "[%s:%d]" fmt, __FILE__, __LINE__, ##__VA_ARGS__);  \
} while (0)
#else
#define HCCPDlogForC(moduleId, level, fmt, ...) do {                                            \
    if (CheckLogLevel(moduleId, level) == 1) {                                              \
        if (DlogRecord == NULL) {                                                           \
            DlogInner(moduleId, level, "[%s:%d]" fmt, __FILE__, __LINE__, ##__VA_ARGS__);   \
        } else {                                                                                \
            DlogRecord(moduleId, level, "[%s:%d]" fmt, __FILE__, __LINE__, ##__VA_ARGS__);  \
        }                                                                                       \
    }                                                                                           \
} while (0)
#endif

/* HCCP module */
#define hccp_err(fmt, args...)  HCCPDlogForC(HCCP, ERROR_LEVEL, "tid:%d,%s : " fmt, \
    syscall(__NR_gettid), __func__, ##args)
#define hccp_warn(fmt, args...) HCCPDlogForC(HCCP, WARN_LEVEL, "tid:%d,%s : " fmt, \
    syscall(__NR_gettid), __func__, ##args)
#define hccp_info(fmt, args...) HCCPDlogForC(HCCP, INFO_LEVEL, "tid:%d,%s : " fmt, \
    syscall(__NR_gettid), __func__, ##args)
#define hccp_dbg(fmt, args...)  HCCPDlogForC(HCCP, DEBUG_LEVEL, "tid:%d,%s : " fmt, \
    syscall(__NR_gettid), __func__, ##args)

#define hccp_run_err(fmt, args...) HCCPDlogForC(HCCP | RUN_LOG_MASK, ERROR_LEVEL, "tid:%d,%s : " fmt, \
    syscall(__NR_gettid), __func__, ##args)
#define hccp_run_warn(fmt, args...) HCCPDlogForC(HCCP | RUN_LOG_MASK, WARN_LEVEL, "tid:%d,%s : " fmt, \
    syscall(__NR_gettid), __func__, ##args)
#define hccp_run_info(fmt, args...) HCCPDlogForC(HCCP | RUN_LOG_MASK, INFO_LEVEL, "tid:%d,%s : " fmt, \
    syscall(__NR_gettid), __func__, ##args)
#define hccp_run_dbg(fmt, args...)  HCCPDlogForC(HCCP | RUN_LOG_MASK, DEBUG_LEVEL, "tid:%d,%s : " fmt, \
    syscall(__NR_gettid), __func__, ##args)

#define hccp_event(fmt, args...)  HCCPDlogForC(HCCP, EVENT_LEVEL, "tid:%d,%s : " fmt, \
    syscall(__NR_gettid), __func__, ##args)

/* ROCE module */
#define roce_err(fmt, args...)    HCCPDlogForC(ROCE, ERROR_LEVEL, "%s : " fmt, __func__, ##args)
#define roce_warn(fmt, args...)   HCCPDlogForC(ROCE, WARN_LEVEL, "%s : " fmt, __func__, ##args)
#define roce_info(fmt, args...)   HCCPDlogForC(ROCE, INFO_LEVEL, "%s : " fmt, __func__, ##args)
#define roce_dbg(fmt, args...)    HCCPDlogForC(ROCE, DEBUG_LEVEL, "%s : " fmt, __func__, ##args)

#define roce_run_err(fmt, args...)    HCCPDlogForC(ROCE | RUN_LOG_MASK, ERROR_LEVEL, "%s : " fmt, \
    __func__, ##args)
#define roce_run_warn(fmt, args...)   HCCPDlogForC(ROCE | RUN_LOG_MASK, WARN_LEVEL, "%s : " fmt, \
    __func__, ##args)
#define roce_run_info(fmt, args...)   HCCPDlogForC(ROCE | RUN_LOG_MASK, INFO_LEVEL, "%s : " fmt, \
    __func__, ##args)
#define roce_run_dbg(fmt, args...)    HCCPDlogForC(ROCE | RUN_LOG_MASK, DEBUG_LEVEL, "%s : " fmt, \
    __func__, ##args)

#define roce_event(fmt, args...)  HCCPDlogForC(ROCE, EVENT_LEVEL, "%s : " fmt, __func__, ##args)
#define roce_event_with_user(fmt, args...) \
    do { \
        struct passwd *pwd = getpwuid(getuid()); \
        if (pwd == NULL) { \
            roce_err("getpwuid failed, errno:%d", errno); \
            return (-1); \
        } \
        HCCPDlogForC(ROCE, EVENT_LEVEL, "%s user(%s): " fmt, __func__, pwd->pw_name, ##args); \
    } while (0);
#else
#define hccp_dlog(moduleId, level, fmt, ...)                                                   \
    do {                                                                                       \
        if (CheckLogLevel(moduleId, level) == 1) {                                             \
            DlogRecord(moduleId, level, "[%s:%d]" fmt, __FILE__, __LINE__, ##__VA_ARGS__);     \
        }                                                                                      \
    } while (0)
/* HCCP module */
#define hccp_err(fmt, args...)  hccp_dlog(HCCP, ERROR_LEVEL, "tid:%d,%s : " fmt, \
    syscall(__NR_gettid), __func__, ##args)
#define hccp_warn(fmt, args...) hccp_dlog(HCCP, WARN_LEVEL, "tid:%d,%s : " fmt, \
    syscall(__NR_gettid), __func__, ##args)
#define hccp_info(fmt, args...) hccp_dlog(HCCP, INFO_LEVEL, "tid:%d,%s : " fmt, \
    syscall(__NR_gettid), __func__, ##args)
#define hccp_dbg(fmt, args...)  hccp_dlog(HCCP, DEBUG_LEVEL, "tid:%d,%s : " fmt, \
    syscall(__NR_gettid), __func__, ##args)

#define hccp_run_err(fmt, args...)  hccp_dlog(HCCP | RUN_LOG_MASK, ERROR_LEVEL, "tid:%d,%s : " fmt, \
    syscall(__NR_gettid), __func__, ##args)
#define hccp_run_warn(fmt, args...) hccp_dlog(HCCP | RUN_LOG_MASK, WARN_LEVEL, "tid:%d,%s : " fmt, \
    syscall(__NR_gettid), __func__, ##args)
#define hccp_run_info(fmt, args...) hccp_dlog(HCCP | RUN_LOG_MASK, INFO_LEVEL, "tid:%d,%s : " fmt, \
    syscall(__NR_gettid), __func__, ##args)
#define hccp_run_dbg(fmt, args...)  hccp_dlog(HCCP | RUN_LOG_MASK, DEBUG_LEVEL, "tid:%d,%s : " fmt, \
    syscall(__NR_gettid), __func__, ##args)

#define hccp_event(fmt, args...)  hccp_dlog(HCCP, EVENT_LEVEL, "tid:%d,%s : " fmt, \
    syscall(__NR_gettid), __func__, ##args)
#define hccp_event_with_user(fmt, args...) \
    do { \
        if (getpwuid(getuid()) == NULL) { \
            hccp_err("getpwuid failed, errno:%d", errno); \
            return (-1); \
        } \
        hccp_dlog(HCCP, EVENT_LEVEL, "%s user(%s): " fmt, \
                  __func__, getpwuid(getuid())->pw_name, ##args); \
    } while (0);

/* ROCE module */
#define roce_err(fmt, args...)    hccp_dlog(ROCE, ERROR_LEVEL, "%s : " fmt, __func__, ##args)
#define roce_warn(fmt, args...)   hccp_dlog(ROCE, WARN_LEVEL, "%s : " fmt, __func__, ##args)
#define roce_info(fmt, args...)   hccp_dlog(ROCE, INFO_LEVEL, "%s : " fmt, __func__, ##args)
#define roce_dbg(fmt, args...)    hccp_dlog(ROCE, DEBUG_LEVEL, "%s : " fmt, __func__, ##args)

#define roce_run_err(fmt, args...)    hccp_dlog(ROCE | RUN_LOG_MASK, ERROR_LEVEL, "%s : " fmt, \
    __func__, ##args)
#define roce_run_warn(fmt, args...)   hccp_dlog(ROCE | RUN_LOG_MASK, WARN_LEVEL, "%s : " fmt, \
    __func__, ##args)
#define roce_run_info(fmt, args...)   hccp_dlog(ROCE | RUN_LOG_MASK, INFO_LEVEL, "%s : " fmt, \
    __func__, ##args)
#define roce_run_dbg(fmt, args...)    hccp_dlog(ROCE | RUN_LOG_MASK, DEBUG_LEVEL, "%s : " fmt, \
    __func__, ##args)

#define roce_event(fmt, args...)  hccp_dlog(ROCE, "%s : " fmt, __func__, ##args)
#define roce_event_with_user(fmt, args...) \
    do { \
        struct passwd *pwd = getpwuid(getuid()); \
        if (pwd == NULL) { \
            roce_err("getpwuid failed, errno:%d", errno); \
            return (-1); \
        } \
        hccp_dlog(ROCE, EVENT_LEVEL, "%s user(%s): " fmt, __func__, pwd->pw_name, ##args); \
    } while (0);
/* ccu dlog */
#define ccu_usr_dlog(moduleId, submodule, level, fmt, ...)                                                   \
    do {                                                                                       \
        if (CheckLogLevel(moduleId, level) == 1) {                                             \
            if (DlogRecord == NULL) {                                                          \
                DlogInner(moduleId, level, "[%s][%s]" fmt, submodule, __FILE__, ##__VA_ARGS__);  \
            } else {                                                                           \
                DlogSub(moduleId, submodule, level, "[%s]" fmt, __FILE__, ##__VA_ARGS__); \
            }                                                                                  \
        }                                                                                      \
    } while (0)

/* ccu module */
#define ccu_usr_err(fmt, args...)  ccu_usr_dlog(HCCP, submodule_ccu, ERROR_LEVEL, "(%d),%s,(tid:%d) : " fmt, \
    __LINE__, __func__, syscall(__NR_gettid), ##args)
#define ccu_usr_warn(fmt, args...) ccu_usr_dlog(HCCP, submodule_ccu, WARN_LEVEL, "(%d),%s,(tid:%d) : " fmt, \
    __LINE__, __func__, syscall(__NR_gettid), ##args)
#define ccu_usr_info(fmt, args...) ccu_usr_dlog(HCCP, submodule_ccu, INFO_LEVEL, "(%d),%s,(tid:%d) : " fmt, \
    __LINE__, __func__, syscall(__NR_gettid), ##args)
#define ccu_usr_dbg(fmt, args...)  ccu_usr_dlog(HCCP, submodule_ccu, DEBUG_LEVEL, "(%d),%s,(tid:%d) : " fmt, \
    __LINE__, __func__, syscall(__NR_gettid), ##args)

#define ccu_usr_run_err(fmt, args...)  ccu_usr_dlog(HCCP | RUN_LOG_MASK, submodule_ccu, ERROR_LEVEL, "(%d),%s,(tid:%d) : " fmt, \
    __LINE__, __func__, syscall(__NR_gettid), ##args)
#define ccu_usr_run_warn(fmt, args...) ccu_usr_dlog(HCCP | RUN_LOG_MASK, submodule_ccu, WARN_LEVEL, "(%d),%s,(tid:%d) : " fmt, \
    __LINE__, __func__, syscall(__NR_gettid), ##args)
#define ccu_usr_run_info(fmt, args...) ccu_usr_dlog(HCCP | RUN_LOG_MASK, submodule_ccu, INFO_LEVEL, "(%d),%s,(tid:%d) : " fmt, \
    __LINE__, __func__, syscall(__NR_gettid), ##args)
#define ccu_usr_run_dbg(fmt, args...)  ccu_usr_dlog(HCCP | RUN_LOG_MASK, submodule_ccu, DEBUG_LEVEL, "(%d),%s,(tid:%d) : " fmt, \
    __LINE__, __func__, syscall(__NR_gettid), ##args)

#define ccu_usr_event(fmt, args...)  ccu_usr_dlog(HCCP, submodule_ccu, EVENT_LEVEL, "(%d),%s,(tid:%d) : " fmt, \
    __LINE__, __func__, syscall(__NR_gettid), ##args)
#endif
#endif
#endif

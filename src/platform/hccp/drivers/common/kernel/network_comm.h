/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * Description: network kernel common function realization
 * Create: 2025-10-25
 */
#ifndef NETWORK_COMMON_H
#define NETWORK_COMMON_H

#ifndef CONFIG_LLT
#include "dmc_kernel_interface.h"
#else
#include "drv_log_llt.h"
#endif

#define PROCESS_EXCUTABLE_FLAG          0x75

#ifndef CONFIG_LLT
#define ccu_share_log_err(start, fmt, ...) share_log_err(start, fmt, ##__VA_ARGS__)
#define SMP_PROCESSOR_ID  smp_processor_id()
#define CURRENT_TGID  current->tgid
#define CURRENT_COMM current->comm
#define CURRENT_PID  current->pid
#else
#define ccu_share_log_err(start, fmt, ...) do { } while (0)
#define CURRENT_PID 0
#define SMP_PROCESSOR_ID  0
#define CURRENT_COMM  "LLT"
#define CURRENT_TGID  0
#endif

#ifndef CONFIG_LLT
#define STATIC static
#else
#define STATIC
#endif

#define DRV_CHK_PRT_RETURN(condition, exeLog, ret) \
    do {                                      \
        if (condition) {                         \
            exeLog;                           \
            return (ret);                       \
        }                                     \
    } while (0)

#define network_drv_err(module_name, fmt, ...) do { \
    drv_err(module_name, "<%s:%d:%d:%d> " fmt, \
        CURRENT_COMM, CURRENT_TGID, CURRENT_PID, SMP_PROCESSOR_ID, ##__VA_ARGS__); \
} while (0)
#define network_drv_info(module_name, fmt, ...) do { \
    drv_info(module_name, "<%s:%d:%d:%d> " fmt, \
        CURRENT_COMM, CURRENT_TGID, CURRENT_PID, SMP_PROCESSOR_ID, ##__VA_ARGS__); \
} while (0)
#define network_drv_warn(module_name, fmt, ...) do { \
    drv_warn(module_name, "<%s:%d:%d:%d> " fmt, \
        CURRENT_COMM, CURRENT_TGID, CURRENT_PID, SMP_PROCESSOR_ID, ##__VA_ARGS__); \
} while (0)
#define network_drv_debug(module_name, fmt, ...) do { \
    drv_pr_debug(module_name, "<%s:%d:%d:%d> " fmt, \
        CURRENT_COMM, CURRENT_TGID, CURRENT_PID, SMP_PROCESSOR_ID, ##__VA_ARGS__); \
} while (0)


typedef struct {
    const char *path;
    const char *user_name; /* minirc use only */
    uid_t uid; /* none minirc use, minirc uid could change */
} white_list_item_t;

int verify_process_whitelist(const char *module_name, const white_list_item_t *whitelist, size_t list_size);

#endif // NETWORK_COMMON_H
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * Create: 2025-10-22
 */

#ifndef HCCN_LOG_H
#define HCCN_LOG_H

#include "user_log.h"

#define HCCN_CHECK_USER_IS_ROOT "root"
#define HCCN_USER_NAME_LEN 32
#define HCCN_USER_IP_LEN 20

/* hccn module */
#define hccn_err(fmt, args...) \
    fprintf(stderr, "%s, pid(%d), %s(%d): " fmt "\n", __TIME__, getpid(), __func__, __LINE__, ##args)
#define hccn_warn(fmt, args...) \
    fprintf(stderr, "%s, pid(%d), %s(%d): " fmt "\n", __TIME__, getpid(), __func__, __LINE__, ##args)
#define hccn_info(fmt, args...) \
    fprintf(stderr, "%s, pid(%d), %s(%d): " fmt "\n", __TIME__, getpid(), __func__, __LINE__, ##args)
#define hccn_dbg(fmt, args...) \
    fprintf(stderr, "%s, pid(%d), %s(%d): " fmt "\n", __TIME__, getpid(), __func__, __LINE__, ##args)
#define hccn_run_warn hccn_warn

#endif // HCCN_LOG_H
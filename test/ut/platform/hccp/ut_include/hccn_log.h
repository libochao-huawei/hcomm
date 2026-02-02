/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
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
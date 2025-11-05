/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef RANK_TABLE_H
#define RANK_TABLE_H

#include <syslog.h>
#include <arpa/inet.h>
#include "rank_table_pub.h"

namespace cltm {
#define CLTM_CHECK(result, exeLog, retCode) do {    \
    if (result != CLTM_SUCCESS) {                   \
        exeLog;                                     \
        return retCode;                             \
    }                                           \
} while (0)

const char *PROP_STATUS = "status";
const char *PROP_VERSION = "version";
const char *PROP_SRV_NUM = "server_num";
const char *PROP_SRV_CNT = "server_count";
const char *PROP_RANK_ID = "rank_id";
const char *PROP_SERVER_ID = "server_id";
const char *PROP_SERVER_LIST = "server_list";
const char *PROP_DEVICE = "device";
const char *PROP_HOST_IP = "host_nic_ip";
const char *PROP_DEV_ID = "device_id";
const char *PROP_DEV_IP = "device_ip";
const char *PROP_GROUP_CNT = "group_count";
const char *PROP_ALLOC_RES = "allocated_group_resource";
const char *PROP_AVAIL_DEV_CNT = "avail_dev_count";
const char *PROP_AVAIL_DEV = "avail_dev";
const std::string CHIP_V80 = "910";
const u32 MAX_JSON_LEN = 4 * 1024 * 1024;  // 最大接受4 * 1024 * 1024长度的json输入
const u32 MAX_DEV_ID = 7;
const u32 MAX_GROUP_NAME_LEN = 127;
}  // namespace cltm
#endif

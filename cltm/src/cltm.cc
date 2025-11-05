/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <cltm.h>
#include <syslog.h>
#include "rank_table_pub.h"

using namespace std;
using namespace cltm;

const char* CLTM_VERSION = "cltm_version: 1.0 (2020-04-30)";

cltmResult_t cltm_generate_ranktable(
    const char* allocatedResource, char* rankTableBuf, unsigned int maxBufSize, unsigned int* usedBufSize)
{
    syslog(LOG_INFO, "[CLTM] generate rank table begin");

    /* 入参合法性检测 */
    if ((allocatedResource == nullptr) || (rankTableBuf == nullptr) || (usedBufSize == nullptr)) {
        syslog(LOG_ERR,
            "[CLTM] errNo[0x%016llx] input pointer para invalid!", CLTM_ERROR_CODE(CLTM_E_PARA));
        return CLTM_E_PARA;
    }

    /* 构造一个rank_table类对象 */
    RankTable rankTable(allocatedResource, maxBufSize);
    cltmResult_t ret = rankTable.init();
    if (ret != CLTM_SUCCESS) {
        syslog(LOG_ERR, "[CLTM] rank_table_init failed ret[%d]", ret);
        return ret;
    }

    syslog(LOG_INFO, "[CLTM] rank table init OK");

    ret = rankTable.GenerateRankTable(rankTableBuf, usedBufSize);
    if (ret != CLTM_SUCCESS) {
        syslog(LOG_ERR, "[CLTM] rank_table_init failed ret[%d]", ret);
        return ret;
    }
    syslog(LOG_INFO, "[CLTM] generate_rank_table OK! used_buff_size[%d]", *usedBufSize);

    return CLTM_SUCCESS;
}

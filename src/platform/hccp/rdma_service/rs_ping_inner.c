/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "user_log.h"
#include "rs_ping_inner.h"

uint32_t RsPingGetTripTime(struct rs_ping_timestamp *timestamp)
{
    uint64_t localInterval = ((timestamp->tv_sec4 - timestamp->tv_sec1) * RS_PING_SEC_TO_USEC + timestamp->tv_usec4) -
        timestamp->tv_usec1;
    uint64_t remoteInterval = ((timestamp->tv_sec3 - timestamp->tv_sec2) * RS_PING_SEC_TO_USEC + timestamp->tv_usec3) -
        timestamp->tv_usec2;

    hccp_dbg("t1:{%llu, %llu} t4:{%llu, %llu} t4-t1:%llu, t2:{%llu, %llu} t3:{%llu, %llu} t3-t2:%llu, rtt:%u",
        timestamp->tv_sec1, timestamp->tv_usec1,
        timestamp->tv_sec4, timestamp->tv_usec4, localInterval,
        timestamp->tv_sec2, timestamp->tv_usec2,
        timestamp->tv_sec3, timestamp->tv_usec3, remoteInterval, (uint32_t)(localInterval - remoteInterval));

    return (uint32_t)(localInterval - remoteInterval);
}

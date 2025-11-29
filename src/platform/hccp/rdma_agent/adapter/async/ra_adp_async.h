/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef RA_ADP_ASYNC_H
#define RA_ADP_ASYNC_H

#include <stdbool.h>
#include <sys/time.h>
#include <pthread.h>
#include <unistd.h>
#include "ascend_hal.h"

struct ra_hdc_op_sec {
    struct timeval t_last;
    unsigned long long token_num;
    unsigned int cfg_op_num;
    bool is_async_op;
};

struct ra_hdc_async_info {
    HDC_SESSION hdc_session;
    pthread_mutex_t send_mutex;
    struct ra_hdc_op_sec op_sec;
    struct ra_hdc_thread_pool *pool;
};

int RaHwAsyncInit(unsigned int chipId, pid_t pid);
void RaHwAsyncDeinit(void);
int RaRsAsyncHdcSessionConnect(char *inBuf, char *outBuf, int *outLen, int *opResult, int rcvBufLen);
int RaRsAsyncHdcSessionClose(char *inBuf, char *outBuf, int *outLen, int *opResult, int rcvBufLen);
#endif // RA_ADP_ASYNC_H

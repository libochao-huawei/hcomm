/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/wait.h>

typedef enum {
    TSD_HCCP = 0,    /**< HCCP*/
    TSD_COMPUTE, /**< Compute_process*/
    TSD_CUSTOM_COMPUTE, /**< Custom Compute_process*/
    TSD_QS,
    TSD_WAITTYPE_MAX /**< Max*/
} TsdWaitType;

int32_t SendStartUpFinishMsg(const uint32_t deviceId, const TsdWaitType waitType,
                             const uint32_t hostPid, const uint32_t vfId)
{
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    sigprocmask(SIG_BLOCK, &mask, NULL);
    int sig;
    sigwait(&mask, &sig);
    printf("recv father's signal, hccp_service.bin will exit\n");
    exit(0);
    return 0;
}

int32_t ReportProcessStartUpErrorCode(const uint32_t deviceId, const TsdWaitType waitType,
                                      const uint32_t hostPid, const uint32_t vfId,
                                      const char *errCode, const uint32_t errLen)
{
    return 0;
}
/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/uio.h>
#include <stdint.h>
#include <arpa/inet.h>
#include "ascend_hal.h"
#include "ascend_hal_stub.h"

static HdcSessionT *g_hdcMgr[MAX_DEV_ID] = {0};
static int g_sessionIdSeed = 1000;

// ==================== 工具函数：原子分配唯一会话ID ====================
int AllocSessionId(void)
{
    return __sync_fetch_and_add(&g_sessionIdSeed, 1);
}

HdcSessionT **GetSession()
{
    return g_hdcMgr;
}

//device call:host phy Id -> device phy Id yes
drvError_t drvGetLocalDevIDByHostDevID(unsigned int devId, unsigned int* chipId)
{
    for(int i = 0; i < MAX_DEV_ID; i++) {
        if (GetH2dInfo()[i].hostPhyId == devId) {
            *chipId = GetH2dInfo()[i].devPhyId;
            return DRV_ERROR_NONE;
        }
    }
    return 0;
}

//device call:device phy ID -> device logic ID yes
drvError_t drvDeviceGetIndexByPhyId(uint32_t phyId, uint32_t *devIndex)
{
    *devIndex = phyId;
    return DRV_ERROR_NONE;
}

//device call:device phy Id -> host phy ID yes
drvError_t drvGetDevIDByLocalDevID(unsigned int localDevId, unsigned int *devId)
{
    pid_t pid = getpid();
    for(int i = 0; i < MAX_DEV_ID; i++) {
        if (GetH2dInfo()[i].devPid == pid) {
            *devId = GetH2dInfo()[i].hostPhyId;
            return DRV_ERROR_NONE;
        }
    }
    return DRV_ERROR_NONE;
}

//device call:host logic ID -> device phy Id yes
drvError_t drvDeviceGetPhyIdByIndex(unsigned int devIndex, unsigned int *phyId)
{
    for(int i = 0; i < MAX_DEV_ID; i++) {
        if (GetH2dInfo()[i].hostLogicId == devIndex) {
            *phyId = GetH2dInfo()[i].devPhyId;
            return DRV_ERROR_NONE;
        }
    }
    return DRV_ERROR_NONE;
}

drvError_t halHdcGetSessionAttr(HDC_SESSION session, int attr, int *value)
{
    HdcSessionT *pSession = (HdcSessionT *)session;
    for(int i = 0; i < MAX_DEV_ID; i++) {
        if(g_hdcMgr[i] == NULL) {
            continue;
        }
        if (pSession->sessionId == g_hdcMgr[i]->sessionId) {
            switch(attr) {
                case HDC_SESSION_ATTR_PEER_CREATE_PID:
                    *value = pSession->hostPid;
                    break;
                default:
                    return DRV_ERROR_INVALID_VALUE;
            }
            return DRV_ERROR_NONE;
        }
    }
    return DRV_ERROR_INVALID_VALUE;
}
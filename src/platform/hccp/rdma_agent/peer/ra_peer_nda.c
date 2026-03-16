/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <stdlib.h>
#include <errno.h>
#include "securec.h"
#include "ra_comm.h"
#include "rs.h"
#include "ra_peer.h"
#include "ra_peer_nda.h"

int RaPeerNdaGetDirectFlag(struct RaRdmaHandle *rdmaHandle, int *directFlag)
{
    unsigned int phyId = rdmaHandle->rdevInfo.phyId;
    int ret = 0;

    RaPeerMutexLock(phyId);
    RsSetCtx(phyId);
    ret = RsNdaGetDirectFlag(phyId, rdmaHandle->rdevIndex, directFlag);
    RaPeerMutexUnlock(phyId);
    if (ret != 0) {
        hccp_err("[get][directFlag]RsNdaGetDirectFlag failed ret:%d", ret);
    }
    return ret;
}

int RaPeerNdaCqCreate(struct RaRdmaHandle *rdmaHandle, struct NdaCqInitAttr *attr, struct NdaCqInfo *info,
    void **cqHandle)
{
    struct RaCqHandleExt *cqPeer = NULL;
    unsigned int phyId = rdmaHandle->rdevInfo.phyId;
    int ret = 0;

    RaPeerMutexLock(phyId);
    RsSetCtx(phyId);

    ret = RsNdaCqCreate(phyId, rdmaHandle->rdevIndex, attr, info);
    RaPeerMutexUnlock(phyId);
    if (ret != 0) {
        hccp_err("[create][RaNdaCq]RsNdaCqCreate failed ret:%d", ret);
    }
    cqPeer->rdmaHandle = rdmaHandle;
    cqPeer->cqInfo = info;
    *cqHandle = cqPeer;
    return ret;
}

int RaPeerNdaCqDestroy(struct RaRdmaHandle *rdmaHandle, void *cqHandle)
{
    unsigned int phyId = rdmaHandle->rdevInfo.phyId;
    int ret = 0;

    RaPeerMutexLock(phyId);
    RsSetCtx(phyId);
    struct RaCqHandleExt *cqPeer = (struct RaCqHandleExt *)cqHandle;
    ret = RsNdaCqDestroy(phyId, rdmaHandle->rdevIndex, cqPeer->cqInfo);
    if (ret != 0) {
        hccp_err("[destroy][RaNdaCq]RsNdaCqDestroy failed ret:%d", ret);
    }
    RaPeerMutexUnlock(phyId);
    return ret;
}
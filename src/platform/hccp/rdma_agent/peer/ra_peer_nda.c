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

int RaPeerNdaQpCreate(struct RaRdmaHandle *rdmaHandle, struct NdaQpInitAttr *attr, struct NdaQpInfo *info,
    void **qpHandle)
{
    unsigned int phyId = rdmaHandle->rdevInfo.phyId;
    struct RaQpHandle *qpPeer = NULL;
    struct RsQpResp qpResp = {0};
    int ret = 0;

    qpPeer = (struct RaQpHandle *)calloc(1, sizeof(struct RaQpHandle));
    CHK_PRT_RETURN(qpPeer == NULL, hccp_err("[create][RaNdaQp]qpPeer calloc failed"), -ENOMEM);

    RaPeerMutexLock(phyId);
    RsSetCtx(phyId);
    ret = RsNdaQpCreate(phyId, rdmaHandle->rdevIndex, attr, info, &qpResp);
    RaPeerMutexUnlock(phyId);
    if (ret != 0) {
        hccp_err("[create][RaNdaQp]RsNdaQpCreate failed ret:%d", ret);
        goto nda_qp_create_err;
    }
    qpPeer->phyId = phyId;
    qpPeer->qpn = qpResp.qpn;
    qpPeer->psn = qpResp.psn;
    qpPeer->gidIdx = qpResp.gidIdx;
    qpPeer->rdevIndex = rdmaHandle->rdevIndex;
    qpPeer->rdmaHandle = rdmaHandle;
    qpPeer->rdmaOps = rdmaHandle->rdmaOps;

    *qpHandle = qpPeer;
    return ret;

nda_qp_create_err:
    free(qpPeer);
    qpPeer = NULL;
    return ret;
}
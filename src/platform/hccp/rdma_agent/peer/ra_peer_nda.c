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
#include "rs.h"
#include "ra_peer.h"
#include "ra_peer_nda.h"

int RaPeerNdaGetDirectFlag(struct RaRdmaHandle *rdmaHandle, int *directFlag)
{
    unsigned int phyId = rdmaHandle->rdevInfo.phyId;
    int ret = 0;

    PEER_PTHREAD_MUTEX_LOCK(&gRaPeerMutex[phyId]);
    RsSetCtx(phyId);
    ret = RsNdaGetDirectFlag(phyId, rdmaHandle->rdevIndex, directFlag);
    PEER_PTHREAD_MUTEX_UNLOCK(&gRaPeerMutex[phyId]);
    if (ret != 0) {
        hccp_err("[get][directFlag]RsNdaGetDirectFlag failed ret:%d", ret);
    }
    return ret;
}

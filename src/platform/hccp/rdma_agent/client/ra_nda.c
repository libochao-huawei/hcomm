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
#include "hccp_nda.h"
#include "ra.h"
#include "ra_peer_nda.h"
#include "ra_rs_comm.h"
#include "ra_client_host.h"

HCCP_ATTRI_VISI_DEF int RaNdaGetDirectFlag(void *rdmaHandle, int *directFlag)
{
    struct RaRdmaHandle *rdevHandleTmp = (struct RaRdmaHandle *)rdmaHandle;
    int ret = 0;

    CHK_PRT_RETURN(rdmaHandle == NULL || directFlag == NULL,
        hccp_err("[get][directFlag]rdmaHandle or directFlag is NULL, invalid"), ConverReturnCode(RDMA_OP, -EINVAL));

    ret = RaPeerNdaGetDirectFlag(rdevHandleTmp, directFlag);
    return ConverReturnCode(RDMA_OP, ret);
}
/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "ut_dispatch.h"
#include "rs_common_inner.h"
#include "rs_inner.h"
#include "rs_rdma.h"
#include "rs_nda.h"
#include "tc_ut_rs_nda.h"

extern int RsQueryRdevCb(unsigned int phyId, unsigned int rdevIndex, struct RsRdevCb **rdevCb);
extern __thread struct rs_cb *gRsCb;

int RsQueryRdevCbNdaStub(unsigned int phyId, unsigned int rdevIndex, struct RsRdevCb **rdevCb)
{
    static struct RsRdevCb ndaRdevCb = {0};

    *rdevCb = &ndaRdevCb;
    return 0;
}

void TcRsNdaGetDirectFlag()
{
    unsigned int rdevIndex = 0;
    unsigned int phyId = 0;
    int directFlag = 0;
    int ret = 0;

    ret = RsNdaGetDirectFlag(RS_MAX_DEV_NUM, rdevIndex, &directFlag);
    EXPECT_INT_EQ(ret, -EINVAL);

    mocker_clean();
    mocker_invoke(RsQueryRdevCb, RsQueryRdevCbNdaStub, 1);
    ret = RsNdaGetDirectFlag(phyId, rdevIndex, &directFlag);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

void TcRsNdaQpCreate()
{
    struct NdaQpInitAttr attr = {0};
    struct RsQpResp qpResp = {0};
    struct NdaQpInfo info = {0};
    unsigned int rdevIndex = 0;
    unsigned int phyId = 0;
    int ret = 0;

    ret = RsNdaQpCreate(phyId, rdevIndex, NULL, &info, &qpResp);
    EXPECT_INT_EQ(ret, -EINVAL);
}

// gRsCb = calloc(1, sizeof(struct rs_cb));

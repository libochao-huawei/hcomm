/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "ut_dispatch.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "ra_client_host.h"
#include "hccp.h"

typedef uint32_t u32;
typedef uint16_t u16;
typedef unsigned long long u64;
typedef signed int s32;

/* Stub functions for RaRdmaOps function pointers */
static int stub_raTypicalCqCreate(struct RaRdmaHandle *rdmaHandle, unsigned int cqDepth,
    unsigned int *cqn, void **cqHandle)
{
    static int dummyHandle;
    *cqn = 100;
    *cqHandle = &dummyHandle;
    return 0;
}

static int stub_raTypicalCqCreate_Fail(struct RaRdmaHandle *rdmaHandle, unsigned int cqDepth,
    unsigned int *cqn, void **cqHandle)
{
    return -EIO;
}

static int stub_raTypicalCqDestroy(struct RaRdmaHandle *rdmaHandle, unsigned int cqn, void *cqHandle)
{
    return 0;
}

static int stub_raQpCreateWithCQWithAttrs(struct RaRdmaHandle *rdmaHandle, struct QpExtAttrs *extAttrs,
    unsigned int sendCqn, unsigned int recvCqn, void **qpHandle)
{
    static int dummyHandle;
    *qpHandle = &dummyHandle;
    return 0;
}

static int stub_raQpDestroyWithoutCQ(struct RaQpHandle *qpHdc)
{
    return 0;
}

/* ===================================================================
 * TcHostTypicalCqCreateDestroy - Tests RaTypicalCqCreate/RaTypicalCqDestroy
 * =================================================================== */
void TcHostTypicalCqCreateDestroy(void)
{
    struct RaRdmaHandle rdmaHandle;
    struct RaRdmaOps rdmaOps;
    unsigned int cqn = 0;
    void *cqHandle = NULL;
    int ret;

    memset(&rdmaHandle, 0, sizeof(rdmaHandle));
    memset(&rdmaOps, 0, sizeof(rdmaOps));
    rdmaHandle.rdevInfo.phyId = 0;
    rdmaHandle.rdevIndex = 0;
    rdmaHandle.rdmaOps = &rdmaOps;

    /* Case 1: NULL rdevHandle */
    ret = RaTypicalCqCreate(NULL, 128, &cqn, &cqHandle);
    EXPECT_INT_NE(ret, 0);

    /* Case 2: NULL rdmaOps */
    rdmaHandle.rdmaOps = NULL;
    ret = RaTypicalCqCreate(&rdmaHandle, 128, &cqn, &cqHandle);
    EXPECT_INT_NE(ret, 0);

    /* Case 3: rdmaOps->raTypicalCqCreate is NULL */
    rdmaHandle.rdmaOps = &rdmaOps;
    /* rdmaOps.raTypicalCqCreate is already NULL from memset */
    ret = RaTypicalCqCreate(&rdmaHandle, 128, &cqn, &cqHandle);
    EXPECT_INT_NE(ret, 0);

    /* Case 4: NULL cqHandle */
    rdmaOps.raTypicalCqCreate = stub_raTypicalCqCreate;
    ret = RaTypicalCqCreate(&rdmaHandle, 128, &cqn, NULL);
    EXPECT_INT_NE(ret, 0);

    /* Case 5: NULL cqn */
    ret = RaTypicalCqCreate(&rdmaHandle, 128, NULL, &cqHandle);
    EXPECT_INT_NE(ret, 0);

    /* Case 6: phyId too large */
    rdmaHandle.rdevInfo.phyId = RA_MAX_PHY_ID_NUM;
    ret = RaTypicalCqCreate(&rdmaHandle, 128, &cqn, &cqHandle);
    EXPECT_INT_NE(ret, 0);
    rdmaHandle.rdevInfo.phyId = 0;

    /* Case 7: Success */
    rdmaOps.raTypicalCqCreate = stub_raTypicalCqCreate;
    cqn = 0;
    cqHandle = NULL;
    ret = RaTypicalCqCreate(&rdmaHandle, 128, &cqn, &cqHandle);
    EXPECT_INT_EQ(ret, 0);
    EXPECT_INT_EQ(cqn, 100);
    EXPECT_ADDR_NE(cqHandle, NULL);

    /* Case 8: raTypicalCqCreate returns failure */
    rdmaOps.raTypicalCqCreate = stub_raTypicalCqCreate_Fail;
    cqn = 0;
    cqHandle = NULL;
    ret = RaTypicalCqCreate(&rdmaHandle, 128, &cqn, &cqHandle);
    EXPECT_INT_NE(ret, 0);

    /* Case 9: RaTypicalCqDestroy - NULL rdevHandle */
    ret = RaTypicalCqDestroy(NULL, 100, cqHandle);
    EXPECT_INT_NE(ret, 0);

    /* Case 10: NULL cqHandle */
    rdmaOps.raTypicalCqDestroy = stub_raTypicalCqDestroy;
    ret = RaTypicalCqDestroy(&rdmaHandle, 100, NULL);
    EXPECT_INT_NE(ret, 0);

    /* Case 11: Success */
    rdmaOps.raTypicalCqDestroy = stub_raTypicalCqDestroy;
    /* Use a valid dummy pointer for cqHandle */
    static int dummyCq;
    ret = RaTypicalCqDestroy(&rdmaHandle, 100, &dummyCq);
    EXPECT_INT_EQ(ret, 0);
}

/* ===================================================================
 * TcHostQpCreateWithCQWithAttrs - Tests RaQpCreateWithCQWithAttrs
 * =================================================================== */
void TcHostQpCreateWithCQWithAttrs(void)
{
    struct RaRdmaHandle rdmaHandle;
    struct RaRdmaOps rdmaOps;
    struct QpExtAttrs extAttrs;
    void *qpHandle = NULL;
    int ret;

    memset(&rdmaHandle, 0, sizeof(rdmaHandle));
    memset(&rdmaOps, 0, sizeof(rdmaOps));
    memset(&extAttrs, 0, sizeof(extAttrs));
    rdmaHandle.rdevInfo.phyId = 0;
    rdmaHandle.rdevIndex = 0;
    rdmaHandle.rdmaOps = &rdmaOps;
    extAttrs.version = QP_CREATE_WITH_ATTR_VERSION;
    extAttrs.qpMode = 4;
    extAttrs.qpAttr.qp_type = IBV_QPT_RC;

    /* Case 1: NULL rdevHandle */
    ret = RaQpCreateWithCQWithAttrs(NULL, &extAttrs, 10, 20, &qpHandle);
    EXPECT_INT_NE(ret, 0);

    /* Case 2: NULL qpHandle */
    rdmaOps.raQpCreateWithCQWithAttrs = stub_raQpCreateWithCQWithAttrs;
    ret = RaQpCreateWithCQWithAttrs(&rdmaHandle, &extAttrs, 10, 20, NULL);
    EXPECT_INT_NE(ret, 0);

    /* Case 3: NULL extAttrs */
    ret = RaQpCreateWithCQWithAttrs(&rdmaHandle, NULL, 10, 20, &qpHandle);
    EXPECT_INT_NE(ret, 0);

    /* Case 4: Wrong version */
    extAttrs.version = 999;
    ret = RaQpCreateWithCQWithAttrs(&rdmaHandle, &extAttrs, 10, 20, &qpHandle);
    EXPECT_INT_NE(ret, 0);
    extAttrs.version = QP_CREATE_WITH_ATTR_VERSION;

    /* Case 5: Invalid qpMode */
    extAttrs.qpMode = -1;
    ret = RaQpCreateWithCQWithAttrs(&rdmaHandle, &extAttrs, 10, 20, &qpHandle);
    EXPECT_INT_NE(ret, 0);
    extAttrs.qpMode = 4;

    /* Case 6: phyId too large */
    rdmaHandle.rdevInfo.phyId = RA_MAX_PHY_ID_NUM;
    ret = RaQpCreateWithCQWithAttrs(&rdmaHandle, &extAttrs, 10, 20, &qpHandle);
    EXPECT_INT_NE(ret, 0);
    rdmaHandle.rdevInfo.phyId = 0;

    /* Case 7: Success */
    rdmaOps.raQpCreateWithCQWithAttrs = stub_raQpCreateWithCQWithAttrs;
    qpHandle = NULL;
    ret = RaQpCreateWithCQWithAttrs(&rdmaHandle, &extAttrs, 10, 20, &qpHandle);
    EXPECT_INT_EQ(ret, 0);
    EXPECT_ADDR_NE(qpHandle, NULL);
}

/* ===================================================================
 * TcHostQpDestroyWithoutCQ - Tests RaQpDestroyWithoutCQ
 * =================================================================== */
void TcHostQpDestroyWithoutCQ(void)
{
    struct RaQpHandle qpHdc;
    struct RaRdmaOps rdmaOps;
    int ret;

    memset(&qpHdc, 0, sizeof(qpHdc));
    memset(&rdmaOps, 0, sizeof(rdmaOps));
    qpHdc.rdmaOps = &rdmaOps;
    qpHdc.qpn = 100;
    qpHdc.phyId = 0;
    qpHdc.rdevIndex = 0;

    /* Case 1: NULL qpHandle */
    ret = RaQpDestroyWithoutCQ(NULL);
    EXPECT_INT_NE(ret, 0);

    /* Case 2: rdmaOps->raQpDestroyWithoutCQ is NULL */
    ret = RaQpDestroyWithoutCQ(&qpHdc);
    EXPECT_INT_NE(ret, 0);

    /* Case 3: Success */
    rdmaOps.raQpDestroyWithoutCQ = stub_raQpDestroyWithoutCQ;
    ret = RaQpDestroyWithoutCQ(&qpHdc);
    EXPECT_INT_EQ(ret, 0);
}

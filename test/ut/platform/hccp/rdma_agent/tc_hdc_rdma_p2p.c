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
#include "ra_hdc_rdma.h"
#include "ra_hdc_lite.h"
#include "ra_comm.h"
#include "ra_rs_err.h"
#include "hccp_common.h"
#include "tc_hdc.h"

typedef uint32_t u32;
typedef uint16_t u16;
typedef unsigned long long u64;
typedef signed int s32;

extern int RaHdcProcessMsg(unsigned int opcode, unsigned int deviceId, char *data, unsigned int dataSize);
extern int RaHdcLiteCqCreate(struct RaRdmaHandle *rdmaHandle, unsigned int cqDepth,
    union OpTypicalCqCreateData *cqData, struct rdma_lite_cq **liteCq);
extern void RaHdcLiteQpDestroyWithoutCQ(struct RaQpHandle *qpHdc);
extern int RaHdcLiteQpCreateWithCQ(struct RaRdmaHandle *rdmaHandle, struct RaQpHandle *qpHdc,
    struct rdma_lite_qp_cap *cap, struct rdma_lite_cq *sendLiteCq, struct rdma_lite_cq *recvLiteCq,
    unsigned int sendCqn, unsigned int recvCqn);

/* Stub for RaHdcProcessMsg that returns success and fills rxData */
int stub_RaHdcProcessMsg_Success(unsigned int opcode, unsigned int deviceId, char *data, unsigned int dataSize)
{
    return 0;
}

int stub_RaHdcProcessMsg_Fail(unsigned int opcode, unsigned int deviceId, char *data, unsigned int dataSize)
{
    return -EIO;
}

int stub_RaHdcLiteCqCreate_Success(struct RaRdmaHandle *rdmaHandle, unsigned int cqDepth,
    union OpTypicalCqCreateData *cqData, struct rdma_lite_cq **liteCq)
{
    static struct rdma_lite_cq fakeCq;
    *liteCq = &fakeCq;
    return 0;
}

int stub_RaHdcLiteCqCreate_Fail(struct RaRdmaHandle *rdmaHandle, unsigned int cqDepth,
    union OpTypicalCqCreateData *cqData, struct rdma_lite_cq **liteCq)
{
    return -EFAULT;
}

int stub_RaHdcLiteQpCreateWithCQ_Success(struct RaRdmaHandle *rdmaHandle, struct RaQpHandle *qpHdc,
    struct rdma_lite_qp_cap *cap, struct rdma_lite_cq *sendLiteCq, struct rdma_lite_cq *recvLiteCq,
    unsigned int sendCqn, unsigned int recvCqn)
{
    return 0;
}

int stub_RaHdcLiteQpCreateWithCQ_Fail(struct RaRdmaHandle *rdmaHandle, struct RaQpHandle *qpHdc,
    struct rdma_lite_qp_cap *cap, struct rdma_lite_cq *sendLiteCq, struct rdma_lite_cq *recvLiteCq,
    unsigned int sendCqn, unsigned int recvCqn)
{
    return -EFAULT;
}

/* ===================================================================
 * TcHdcTypicalCqCreateDestroy
 * =================================================================== */
void TcHdcTypicalCqCreateDestroy(void)
{
    struct RaRdmaHandle rdmaHandle;
    unsigned int cqn = 0;
    void *cqHandle = NULL;
    int ret;

    memset(&rdmaHandle, 0, sizeof(rdmaHandle));
    rdmaHandle.rdevInfo.phyId = 0;
    rdmaHandle.rdevIndex = 0;
    rdmaHandle.supportLite = 0;

    /* Case 1: Success without lite support */
    mocker_invoke((stub_fn_t)RaHdcProcessMsg, (stub_fn_t)stub_RaHdcProcessMsg_Success, 1);
    ret = RaHdcTypicalCqCreate(&rdmaHandle, 128, &cqn, &cqHandle);
    EXPECT_INT_EQ(ret, 0);
    EXPECT_ADDR_NE(cqHandle, NULL);

    /* Clean up the allocated cqHdc */
    if (cqHandle != NULL) {
        mocker_invoke((stub_fn_t)RaHdcProcessMsg, (stub_fn_t)stub_RaHdcProcessMsg_Success, 1);
        (void)RaHdcTypicalCqDestroy(&rdmaHandle, cqn, cqHandle);
    }
    mocker_clean();

    /* Case 2: RaHdcProcessMsg fails */
    mocker_invoke((stub_fn_t)RaHdcProcessMsg, (stub_fn_t)stub_RaHdcProcessMsg_Fail, 1);
    cqn = 0;
    cqHandle = NULL;
    ret = RaHdcTypicalCqCreate(&rdmaHandle, 256, &cqn, &cqHandle);
    EXPECT_INT_NE(ret, 0);
    EXPECT_ADDR_EQ(cqHandle, NULL);
    mocker_clean();

    /* Case 3: Destroy with NULL cqHandle */
    ret = RaHdcTypicalCqDestroy(&rdmaHandle, 42, NULL);
    EXPECT_INT_EQ(ret, -EINVAL);
}

/* ===================================================================
 * TcHdcQpCreateWithCQWithAttrs
 * =================================================================== */
void TcHdcQpCreateWithCQWithAttrs(void)
{
    struct RaRdmaHandle rdmaHandle;
    struct QpExtAttrs extAttrs;
    void *qpHandle = NULL;
    int ret;

    memset(&rdmaHandle, 0, sizeof(rdmaHandle));
    rdmaHandle.rdevInfo.phyId = 0;
    rdmaHandle.rdevIndex = 0;
    rdmaHandle.supportLite = 0;

    memset(&extAttrs, 0, sizeof(extAttrs));
    extAttrs.qpMode = 4; /* RA_RS_OP_QP_MODE */
    extAttrs.qpAttr.qp_type = IBV_QPT_RC;
    extAttrs.qpAttr.sq_sig_all = 0;
    extAttrs.qpAttr.cap.max_send_wr = 128;
    extAttrs.qpAttr.cap.max_recv_wr = 128;
    extAttrs.qpAttr.cap.max_send_sge = 1;
    extAttrs.qpAttr.cap.max_recv_sge = 1;

    /* Case 1: Success (without lite, no lite QP create needed) */
    mocker_invoke((stub_fn_t)RaHdcProcessMsg, (stub_fn_t)stub_RaHdcProcessMsg_Success, 1);
    ret = RaHdcQpCreateWithCQWithAttrs(&rdmaHandle, &extAttrs, 10, 20, &qpHandle);
    EXPECT_INT_EQ(ret, 0);
    EXPECT_ADDR_NE(qpHandle, NULL);

    /* Clean up */
    if (qpHandle != NULL) {
        mocker_invoke((stub_fn_t)RaHdcProcessMsg, (stub_fn_t)stub_RaHdcProcessMsg_Success, 1);
        (void)RaHdcQpDestroy(qpHandle);
    }
    mocker_clean();

    /* Case 2: RaHdcProcessMsg fails */
    qpHandle = NULL;
    mocker_invoke((stub_fn_t)RaHdcProcessMsg, (stub_fn_t)stub_RaHdcProcessMsg_Fail, 1);
    ret = RaHdcQpCreateWithCQWithAttrs(&rdmaHandle, &extAttrs, 10, 20, &qpHandle);
    EXPECT_INT_NE(ret, 0);
    EXPECT_ADDR_EQ(qpHandle, NULL);
    mocker_clean();
}

/* ===================================================================
 * TcHdcQpDestroyWithoutCQ
 * =================================================================== */
void TcHdcQpDestroyWithoutCQ(void)
{
    struct RaRdmaHandle rdmaHandle;
    int ret;

    memset(&rdmaHandle, 0, sizeof(rdmaHandle));
    rdmaHandle.rdevInfo.phyId = 0;
    rdmaHandle.rdevIndex = 0;
    rdmaHandle.supportLite = 0;

    struct RaQpHandle *qpHdc = calloc(1, sizeof(struct RaQpHandle));
    qpHdc->phyId = 0;
    qpHdc->rdevIndex = 0;
    qpHdc->qpn = 100;
    qpHdc->rdmaHandle = &rdmaHandle;
    qpHdc->supportLite = 0;

    /* Case 1: Success - destroy with RaHdcProcessMsg success */
    mocker_invoke((stub_fn_t)RaHdcProcessMsg, (stub_fn_t)stub_RaHdcProcessMsg_Success, 1);
    ret = RaHdcQpDestroyWithoutCQ(qpHdc);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

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
#include "ra_rs_err.h"
#include "rs_common_inner.h"
#include "rs_inner.h"
#include "dl_hal_function.h"
#include "dl_ibverbs_function.h"
#include "rs_rdma.h"
#include "rs_drv_rdma.h"
#include "rs.h"
#include "rs_nda.h"
#include "tc_ut_rs_nda.h"

#define rs_nda_ut_msg(fmt, args...)	fprintf(stderr, "\t>>>>> " fmt, ##args)

extern int RsQueryRdevCb(unsigned int phyId, unsigned int rdevIndex, struct RsRdevCb **rdevCb);
extern __thread struct rs_cb *gRsCb;
extern int RsNdaQpCreateEx(struct RsQpCb *qpCb, struct ibv_qp_init_attr_extend *qpInitAttrEx, struct NdaQpInfo *info);
extern int RsNdaQpCreateEx(struct RsQpCb *qpCb, struct ibv_qp_init_attr_extend *qpInitAttrEx, struct NdaQpInfo *info);

int RsQueryRdevCbNdaStub(unsigned int phyId, unsigned int rdevIndex, struct RsRdevCb **rdevCb)
{
    static struct RsRdevCb ndaRdevCb = {0};

    *rdevCb = &ndaRdevCb;
    return 0;
}

int DlDrvMemGetAttributeStub(DVdeviceptr vptr, struct DVattribute *attr)
{
    attr->memType = DV_MEM_SVM_DEVICE;
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

void TcRsNdaQpCreateFailed()
{
    struct NdaQpInitAttr attr = {0};
    struct RsQpResp qpResp = {0};
    struct NdaQpInfo info = {0};
    unsigned int rdevIndex = 0;
    unsigned int phyId = 0;
    int ret = 0;
    struct NdaOps ops = {0};

    gRsCb = malloc(sizeof(struct rs_cb));
    attr.ops = &ops;

    ret = RsNdaQpCreate(phyId, rdevIndex, NULL, &info, &qpResp);
    EXPECT_INT_EQ(ret, -EINVAL);

    attr.dmaMode = QBUF_DMA_MODE_MAX;
    ret = RsNdaQpCreate(phyId, rdevIndex, &attr, &info, &qpResp);
    EXPECT_INT_EQ(ret, -EINVAL);

    attr.dmaMode = QBUF_DMA_MODE_INDEP_UB;
    mocker_clean();
    mocker(RsQueryRdevCb, 1, -1);
    ret = RsNdaQpCreate(phyId, rdevIndex, &attr, &info, &qpResp);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

    mocker_invoke(RsQueryRdevCb, RsQueryRdevCbNdaStub, 1);
    mocker(RsNdaQpCreateEx, 1, -1);
    ret = RsNdaQpCreate(phyId, rdevIndex, &attr, &info, &qpResp);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();
    free(gRsCb);
    gRsCb = NULL;
}

void TcRsNdaQpCreate()
{
    struct RsInitConfig cfg = {0};
    struct NdaQpInitAttr attr = {0};
    struct NdaQpInfo info = {0};
    struct rdev rdevInfo = {0};
    uint32_t rdevIndex = 0;
    uint32_t phyId = 0;
    int ret = 0;
    struct RsQpResp resp = {0};
    uint32_t qpn = 0;
    struct NdaOps ops = {0};

    rdevInfo.phyId = 0;
    rdevInfo.family = AF_INET;
    rdevInfo.localIp.addr.s_addr = inet_addr("127.0.0.1");

    gRsCb = malloc(sizeof(struct rs_cb));

    rs_nda_ut_msg("resource prepare begin..................\n");
    /* +++++Resource Prepare+++++ */
    cfg.chipId = 0;
    cfg.hccpMode = NETWORK_OFFLINE;
    ret = RsInit(&cfg);
    EXPECT_INT_EQ(ret, 0);
    rs_nda_ut_msg("RS INIT, ret:%d !\n", ret);

    ret = RsRdevInit(rdevInfo, NO_USE, &rdevIndex);
    EXPECT_INT_EQ(ret, 0);

    attr.ops = &ops;
    ret = RsNdaQpCreate(phyId, rdevIndex, &attr, &info, &resp);
    EXPECT_INT_EQ(ret, 0);
    qpn = resp.qpn;
    rs_nda_ut_msg("RS CREATE NDA QP: QPN:%d, ret:%d\n", qpn, ret);

    ret = RsNdaQpDestroy(phyId, rdevIndex, qpn);
    EXPECT_INT_EQ(ret, 0);
    rs_nda_ut_msg("RS destroy NDA QP: ret:%d\n", ret);

    ret = RsRdevDeinit(phyId, NO_USE, rdevIndex);
    EXPECT_INT_EQ(ret, 0);

    ret = RsDeinit(&cfg);
    EXPECT_INT_EQ(ret, 0);
    rs_nda_ut_msg("resource free done..................\n");
    free(gRsCb);
    gRsCb = NULL;
}

void TcRsNdaQpCreateEx()
{
    struct ibv_qp_init_attr_extend qpInitAttrEx = {0};
    struct ibv_context_extend ibCtxEx = {0};
    struct RsRdevCb rdevCb = {0};
    struct NdaQpInfo info = {0};
    struct RsQpCb qpCb = {0};
    int ret = 0;

    rdevCb.ibCtxEx = &ibCtxEx;
    qpCb.rdevCb = &rdevCb;
    mocker_clean();
    mocker(RsIbvQueryQp, 1, -1);
    ret = RsNdaQpCreateEx(&qpCb, &qpInitAttrEx, &info);
    EXPECT_INT_EQ(ret, -EOPENSRC);
    mocker_clean();

    mocker(RsDrvQpInfoRelated, 1, -1);
    ret = RsNdaQpCreateEx(&qpCb, &qpInitAttrEx, &info);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean(); 
}

void TcRsNdaUbAlloc()
{
    struct NdaOps ops = {0};
    size_t size = 1024;
    void *ptr = NULL;

    gRsCb = malloc(sizeof(struct rs_cb));
    ptr = RsNdaUbAlloc(size);
    EXPECT_ADDR_EQ(ptr, NULL);
    gRsCb->ndaOps.alloc = malloc;
    ptr = RsNdaUbAlloc(size);
    EXPECT_ADDR_EQ(ptr, NULL);
    gRsCb->ndaOps.free = free;

    mocker_clean();
    mocker(malloc, 1, NULL);
    ptr = RsNdaUbAlloc(size);
    EXPECT_ADDR_EQ(ptr, NULL);
    mocker_clean();

    mocker(DlDrvMemGetAttribute, 1, -1);
    ptr = RsNdaUbAlloc(size);
    EXPECT_ADDR_EQ(ptr, NULL);
    mocker_clean();

    mocker_invoke(DlDrvMemGetAttribute, DlDrvMemGetAttributeStub, 10);
    mocker(DlHalMemRegUbSegment, 1, -1);
    ptr = RsNdaUbAlloc(size);
    EXPECT_ADDR_EQ(ptr, NULL);

    ptr = RsNdaUbAlloc(size);
    EXPECT_ADDR_NE(ptr, NULL);
    RsNdaUbFree(ptr);

    free(gRsCb);
    gRsCb = NULL;
}
/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

#include "rs.h"
#include "rs_inner.h"
#include "rs_drv_rdma.h"
#include "dl_ibverbs_function.h"
#include "ut_dispatch.h"
#include "ra_rs_err.h"
#include "tc_ut_rs.h"

/* =============== Stub functions =============== */

static int dummy_cq_storage;
static struct ibv_cq *g_fakeIbCq = (struct ibv_cq *)&dummy_cq_storage;

struct ibv_cq *stub_RsIbvExpCreateCq_Success(struct ibv_context *context,
    int cqe, void *cq_context, struct ibv_comp_channel *channel,
    int comp_vector, struct rdma_lite_device_cq_init_attr *attr,
    struct rdma_lite_device_cq_attr *cq_resp)
{
    cq_resp->cqn = 42;
    cq_resp->depth = (unsigned int)cqe;
    return g_fakeIbCq;
}

struct ibv_cq *stub_RsIbvExpCreateCq_Fail(struct ibv_context *context,
    int cqe, void *cq_context, struct ibv_comp_channel *channel,
    int comp_vector, struct rdma_lite_device_cq_init_attr *attr,
    struct rdma_lite_device_cq_attr *cq_resp)
{
    return NULL;
}

/* ===================================================================
 * TcRsDrvTypicalCqCreate
 * =================================================================== */
void TcRsDrvTypicalCqCreate(void)
{
    int ret;
    uint32_t phyId = 0;
    uint32_t rdevIndex = 0;
    unsigned int cqn = 0;
    struct ibv_cq *ibCq = NULL;
    struct rdma_lite_device_cq_attr deviceCqAttr;
    struct rdev rdevInfo = {0};
    struct RsInitConfig cfg = {0};

    rdevInfo.phyId = 0;
    rdevInfo.family = AF_INET;
    rdevInfo.localIp.addr.s_addr = inet_addr("127.0.0.1");

    ret = RsInit(&cfg);
    EXPECT_INT_EQ(ret, 0);
    ret = RsRdevInit(rdevInfo, NOTIFY, &rdevIndex);
    EXPECT_INT_EQ(ret, 0);

    /* Case 1: Success - RsIbvExpCreateCq returns valid CQ */
    {
        mocker_invoke(RsIbvExpCreateCq, stub_RsIbvExpCreateCq_Success, 1);

        struct RsRdevCb *rdevCb = NULL;
        ret = RsQueryRdevCb(phyId, rdevIndex, &rdevCb);
        EXPECT_INT_EQ(ret, 0);

        memset(&deviceCqAttr, 0, sizeof(deviceCqAttr));
        cqn = 0;
        ibCq = NULL;

        ret = RsDrvTypicalCqCreate(rdevCb, 128, &cqn, &ibCq, &deviceCqAttr);
        EXPECT_INT_EQ(ret, 0);
        EXPECT_INT_EQ(cqn, 42);
        EXPECT_ADDR_EQ(ibCq, g_fakeIbCq);
        mocker_clean();
    }

    /* Case 2: RsIbvExpCreateCq fails - returns NULL */
    {
        mocker_invoke(RsIbvExpCreateCq, stub_RsIbvExpCreateCq_Fail, 1);

        struct RsRdevCb *rdevCb = NULL;
        ret = RsQueryRdevCb(phyId, rdevIndex, &rdevCb);
        EXPECT_INT_EQ(ret, 0);

        cqn = 0;
        ibCq = NULL;
        memset(&deviceCqAttr, 0, sizeof(deviceCqAttr));

        ret = RsDrvTypicalCqCreate(rdevCb, 256, &cqn, &ibCq, &deviceCqAttr);
        EXPECT_INT_EQ(ret, -ENOMEM);
        EXPECT_INT_EQ(cqn, 0);
        mocker_clean();
    }

    ret = RsRdevDeinit(phyId, NOTIFY, rdevIndex);
    EXPECT_INT_EQ(ret, 0);
    cfg.chipId = 0;
    ret = RsDeinit(&cfg);
    EXPECT_INT_EQ(ret, 0);
}

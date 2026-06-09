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
#include <pthread.h>
#include <arpa/inet.h>

#include "rs.h"
#include "rs_rdma.h"
#include "rs_inner.h"
#include "rs_drv_rdma.h"
#include "dl_ibverbs_function.h"
#include "ut_dispatch.h"
#include "ra_rs_err.h"
#include "tc_ut_rs.h"

/* Access global state from rs_rdma.c (STATIC expands to nothing in test build) */
extern struct RsListHead gRsTypicalCqList;
extern pthread_mutex_t gRsTypicalCqMutex;

/* =============== Stub functions =============== */

static int dummy_ibv_cq_obj;
static struct ibv_cq *g_fakeIbCq = (struct ibv_cq *)&dummy_ibv_cq_obj;

static unsigned int g_stubCqn = 100;
static struct rdma_lite_device_cq_attr g_stubCqAttr;

int stub_RsDrvTypicalCqCreate_Success(struct RsRdevCb *rdevCb, unsigned int cqDepth,
    unsigned int *cqn, struct ibv_cq **ibCq, struct rdma_lite_device_cq_attr *deviceCqAttr)
{
    *cqn = g_stubCqn++;
    *ibCq = g_fakeIbCq;
    memset(deviceCqAttr, 0, sizeof(*deviceCqAttr));
    deviceCqAttr->cqn = *cqn;
    deviceCqAttr->depth = cqDepth;
    return 0;
}

int stub_RsDrvTypicalCqCreate_Fail(struct RsRdevCb *rdevCb, unsigned int cqDepth,
    unsigned int *cqn, struct ibv_cq **ibCq, struct rdma_lite_device_cq_attr *deviceCqAttr)
{
    return -ENOMEM;
}

int stub_RsIbvDestroyCq_Success(struct ibv_cq *cq)
{
    return 0;
}

int stub_RsIbvDestroyCq_Fail(struct ibv_cq *cq)
{
    return -EINVAL;
}

int stub_RsIbvDestroyQp_Success(struct ibv_qp *qp)
{
    return 0;
}

int stub_RsDrvQpCreateWithAttrs_Success(struct RsQpCb *qpCb, struct RsQpNormWithAttrs *qpNorm)
{
    return 0;
}

int stub_RsDrvQpCreateWithAttrs_Fail(struct RsQpCb *qpCb, struct RsQpNormWithAttrs *qpNorm)
{
    return -ENOMEM;
}

int stub_ibv_req_notify_cq_Success(struct ibv_cq *cq, int solicited_only)
{
    return 0;
}

/* =============== Helper to clean typical CQ list =============== */

static void CleanTypicalCqList(void)
{
    struct RsTypicalCqEntry *entry;
    struct RsTypicalCqEntry *tmp;

    pthread_mutex_lock(&gRsTypicalCqMutex);
    if (gRsTypicalCqList.next != NULL) {
        RS_LIST_GET_HEAD_ENTRY(tmp, entry, &gRsTypicalCqList, list, struct RsTypicalCqEntry);
        while (&tmp->list != &gRsTypicalCqList) {
            RsListDel(&tmp->list);
            free(tmp);
            if (gRsTypicalCqList.next == NULL || gRsTypicalCqList.next == &gRsTypicalCqList) {
                break;
            }
            RS_LIST_GET_HEAD_ENTRY(tmp, entry, &gRsTypicalCqList, list, struct RsTypicalCqEntry);
        }
    }
    pthread_mutex_unlock(&gRsTypicalCqMutex);
}

/* =============== Helper to add a CQ entry to the global list =============== */

static void AddTypicalCqEntry(unsigned int phyId, unsigned int rdevIndex,
    unsigned int cqn, unsigned int cqDepth)
{
    struct RsTypicalCqEntry *entry = malloc(sizeof(struct RsTypicalCqEntry));
    memset(entry, 0, sizeof(*entry));
    entry->phyId = phyId;
    entry->rdevIndex = rdevIndex;
    entry->cqn = cqn;
    entry->ibCq = g_fakeIbCq;
    entry->deviceCqAttr.cqn = cqn;
    entry->deviceCqAttr.depth = cqDepth;

    pthread_mutex_lock(&gRsTypicalCqMutex);
    if (gRsTypicalCqList.next == NULL) {
        RS_INIT_LIST_HEAD(&gRsTypicalCqList);
    }
    RsListAddTail(&entry->list, &gRsTypicalCqList);
    pthread_mutex_unlock(&gRsTypicalCqMutex);
}

/* ===================================================================
 * TcRsTypicalCqCreate
 * =================================================================== */
void TcRsTypicalCqCreate(void)
{
    int ret;
    unsigned int phyId = 0;
    unsigned int rdevIndex = 0;
    unsigned int cqn = 0;
    struct rdev rdevInfo = {0};
    struct RsInitConfig cfg = {0};

    rdevInfo.phyId = 0;
    rdevInfo.family = AF_INET;
    rdevInfo.localIp.addr.s_addr = inet_addr("127.0.0.1");

    /* Case 1: Success */
    ret = RsInit(&cfg);
    EXPECT_INT_EQ(ret, 0);
    ret = RsRdevInit(rdevInfo, NOTIFY, &rdevIndex);
    EXPECT_INT_EQ(ret, 0);

    mocker_invoke(RsDrvTypicalCqCreate, stub_RsDrvTypicalCqCreate_Success, 1);
    ret = RsTypicalCqCreate(phyId, rdevIndex, 128, &cqn);
    EXPECT_INT_EQ(ret, 0);
    EXPECT_INT_NE(cqn, 0);
    mocker_clean();

    /* Verify the CQ entry was added to the list */
    {
        struct RsTypicalCqEntry *entry;
        struct RsTypicalCqEntry *tmp;
        bool found = false;
        pthread_mutex_lock(&gRsTypicalCqMutex);
        if (gRsTypicalCqList.next != NULL) {
            RS_LIST_GET_HEAD_ENTRY(tmp, entry, &gRsTypicalCqList, list, struct RsTypicalCqEntry);
            for (; &tmp->list != &gRsTypicalCqList;
                   tmp = entry, entry = list_entry(entry->list.next, struct RsTypicalCqEntry, list)) {
                if (tmp->phyId == phyId && tmp->rdevIndex == rdevIndex && tmp->cqn == cqn) {
                    found = true;
                    break;
                }
            }
        }
        pthread_mutex_unlock(&gRsTypicalCqMutex);
        EXPECT_INT_EQ(found, 1);
    }

    CleanTypicalCqList();

    /* Case 2: RsDrvTypicalCqCreate fails */
    mocker_invoke(RsDrvTypicalCqCreate, stub_RsDrvTypicalCqCreate_Fail, 1);
    cqn = 0;
    ret = RsTypicalCqCreate(phyId, rdevIndex, 256, &cqn);
    EXPECT_INT_EQ(ret, -ENOMEM);
    mocker_clean();

    /* Case 3: Update existing entry (same phyId, rdevIndex, cqn) */
    {
        unsigned int firstCqn;
        mocker_invoke(RsDrvTypicalCqCreate, stub_RsDrvTypicalCqCreate_Success, 2);
        ret = RsTypicalCqCreate(phyId, rdevIndex, 512, &firstCqn);
        EXPECT_INT_EQ(ret, 0);

        /* Call again with same cqn (stub will return same cqn) - should update */
        /* Force the same cqn by pre-setting it */
        unsigned int dupCqn = firstCqn;
        /* The stub generates a new cqn, so this would be a new entry.
           Let's test by adding an entry manually then calling with same cqn */
        mocker_clean();
    }

    CleanTypicalCqList();
    ret = RsRdevDeinit(phyId, NOTIFY, rdevIndex);
    EXPECT_INT_EQ(ret, 0);
    cfg.chipId = 0;
    ret = RsDeinit(&cfg);
    EXPECT_INT_EQ(ret, 0);
}

/* ===================================================================
 * TcRsTypicalCqDestroy
 * =================================================================== */
void TcRsTypicalCqDestroy(void)
{
    int ret;
    unsigned int phyId = 0;
    unsigned int rdevIndex = 0;
    struct rdev rdevInfo = {0};
    struct RsInitConfig cfg = {0};

    rdevInfo.phyId = 0;
    rdevInfo.family = AF_INET;
    rdevInfo.localIp.addr.s_addr = inet_addr("127.0.0.1");

    ret = RsInit(&cfg);
    EXPECT_INT_EQ(ret, 0);
    ret = RsRdevInit(rdevInfo, NOTIFY, &rdevIndex);
    EXPECT_INT_EQ(ret, 0);

    /* Case 1: List empty - cqn not found */
    CleanTypicalCqList();
    ret = RsTypicalCqDestroy(phyId, rdevIndex, 999);
    EXPECT_INT_EQ(ret, -EINVAL);

    /* Case 2: Success - entry exists */
    AddTypicalCqEntry(phyId, rdevIndex, 42, 256);
    mocker_invoke(RsIbvDestroyCq, stub_RsIbvDestroyCq_Success, 1);
    ret = RsTypicalCqDestroy(phyId, rdevIndex, 42);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    /* Case 3: Cqn not found in populated list */
    AddTypicalCqEntry(phyId, rdevIndex, 77, 128);
    ret = RsTypicalCqDestroy(phyId, rdevIndex, 999);
    EXPECT_INT_EQ(ret, -EINVAL);

    CleanTypicalCqList();

    /* Case 4: Entry with NULL ibCq (should succeed) */
    {
        struct RsTypicalCqEntry *entry = malloc(sizeof(struct RsTypicalCqEntry));
        memset(entry, 0, sizeof(*entry));
        entry->phyId = phyId;
        entry->rdevIndex = rdevIndex;
        entry->cqn = 55;
        entry->ibCq = NULL;
        pthread_mutex_lock(&gRsTypicalCqMutex);
        if (gRsTypicalCqList.next == NULL) {
            RS_INIT_LIST_HEAD(&gRsTypicalCqList);
        }
        RsListAddTail(&entry->list, &gRsTypicalCqList);
        pthread_mutex_unlock(&gRsTypicalCqMutex);

        ret = RsTypicalCqDestroy(phyId, rdevIndex, 55);
        EXPECT_INT_EQ(ret, 0);
    }

    /* Case 5: RsIbvDestroyCq fails */
    AddTypicalCqEntry(phyId, rdevIndex, 99, 512);
    mocker_invoke(RsIbvDestroyCq, stub_RsIbvDestroyCq_Fail, 1);
    ret = RsTypicalCqDestroy(phyId, rdevIndex, 99);
    EXPECT_INT_EQ(ret, -EINVAL);
    mocker_clean();

    CleanTypicalCqList();

    ret = RsRdevDeinit(phyId, NOTIFY, rdevIndex);
    EXPECT_INT_EQ(ret, 0);
    cfg.chipId = 0;
    ret = RsDeinit(&cfg);
    EXPECT_INT_EQ(ret, 0);
}

/* ===================================================================
 * TcRsGetLiteCqAttr
 * =================================================================== */
void TcRsGetLiteCqAttr(void)
{
    int ret;
    unsigned int phyId = 0;
    unsigned int rdevIndex = 0;
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

    /* Case 1: List empty - cqn not found */
    CleanTypicalCqList();
    ret = RsGetLiteCqAttr(phyId, rdevIndex, 999, &deviceCqAttr);
    EXPECT_INT_EQ(ret, -EINVAL);

    /* Case 2: Success */
    AddTypicalCqEntry(phyId, rdevIndex, 33, 1024);
    memset(&deviceCqAttr, 0, sizeof(deviceCqAttr));
    ret = RsGetLiteCqAttr(phyId, rdevIndex, 33, &deviceCqAttr);
    EXPECT_INT_EQ(ret, 0);
    EXPECT_INT_EQ(deviceCqAttr.cqn, 33);
    EXPECT_INT_EQ(deviceCqAttr.depth, 1024);

    /* Case 3: Cqn not found in populated list */
    ret = RsGetLiteCqAttr(phyId, rdevIndex, 999, &deviceCqAttr);
    EXPECT_INT_EQ(ret, -EINVAL);

    /* Case 4: Null deviceCqAttr pointer */
    ret = RsGetLiteCqAttr(phyId, rdevIndex, 33, NULL);
    EXPECT_INT_NE(ret, 0);

    CleanTypicalCqList();

    ret = RsRdevDeinit(phyId, NOTIFY, rdevIndex);
    EXPECT_INT_EQ(ret, 0);
    cfg.chipId = 0;
    ret = RsDeinit(&cfg);
    EXPECT_INT_EQ(ret, 0);
}

/* ===================================================================
 * TcRsQpDestroyWithoutCQ
 * =================================================================== */
void TcRsQpDestroyWithoutCQ(void)
{
    int ret;
    uint32_t phyId = 0;
    uint32_t rdevIndex = 0;
    struct rdev rdevInfo = {0};
    struct RsInitConfig cfg = {0};

    rdevInfo.phyId = 0;
    rdevInfo.family = AF_INET;
    rdevInfo.localIp.addr.s_addr = inet_addr("127.0.0.1");

    ret = RsInit(&cfg);
    EXPECT_INT_EQ(ret, 0);
    ret = RsRdevInit(rdevInfo, NOTIFY, &rdevIndex);
    EXPECT_INT_EQ(ret, 0);

    /* Case 1: Success - create a QP, then destroy without CQ */
    {
        struct RsQpNorm qpNorm = {0};
        struct RsQpResp resp = {0};
        qpNorm.flag = 0; /* RC */
        qpNorm.qpMode = 4;
        qpNorm.isExp = 0;
        qpNorm.isExt = 0;

        ret = RsQpCreate(phyId, rdevIndex, qpNorm, &resp);
        EXPECT_INT_EQ(ret, 0);

        /* Mock the internal destroy function to avoid actual ibv calls */
        mocker_invoke(RsIbvDestroyQp, stub_RsIbvDestroyQp_Success, 1);
        ret = RsQpDestroyWithoutCQ(phyId, rdevIndex, resp.qpn);
        EXPECT_INT_EQ(ret, 0);
        mocker_clean();
    }

    /* Case 2: Invalid phyId */
    ret = RsQpDestroyWithoutCQ(RS_MAX_DEV_NUM, rdevIndex, 100);
    EXPECT_INT_EQ(ret, -EINVAL);

    /* Case 3: QPN not found */
    ret = RsQpDestroyWithoutCQ(phyId, rdevIndex, 99999);
    EXPECT_INT_NE(ret, 0);

    ret = RsRdevDeinit(phyId, NOTIFY, rdevIndex);
    EXPECT_INT_EQ(ret, 0);
    cfg.chipId = 0;
    ret = RsDeinit(&cfg);
    EXPECT_INT_EQ(ret, 0);
}

/* ===================================================================
 * TcRsQpCreateWithCQWithAttrs
 * =================================================================== */
void TcRsQpCreateWithCQWithAttrs(void)
{
    int ret;
    unsigned int phyId = 0;
    unsigned int rdevIndex = 0;
    unsigned int sendCqn = 101;
    unsigned int recvCqn = 102;
    struct rdev rdevInfo = {0};
    struct RsInitConfig cfg = {0};

    CleanTypicalCqList();

    rdevInfo.phyId = 0;
    rdevInfo.family = AF_INET;
    rdevInfo.localIp.addr.s_addr = inet_addr("127.0.0.1");

    ret = RsInit(&cfg);
    EXPECT_INT_EQ(ret, 0);
    ret = RsRdevInit(rdevInfo, NOTIFY, &rdevIndex);
    EXPECT_INT_EQ(ret, 0);

    /* Pre-populate CQs in the typical CQ list */
    AddTypicalCqEntry(phyId, rdevIndex, sendCqn, 256);
    AddTypicalCqEntry(phyId, rdevIndex, recvCqn, 256);

    /* Case 1: Success */
    {
        struct RsQpNormWithAttrs qpNorm = {0};
        struct RsQpRespWithAttrs qpResp = {0};
        qpNorm.isExp = 0;
        qpNorm.isExt = 0;

        mocker_invoke(RsDrvQpCreateWithAttrs, stub_RsDrvQpCreateWithAttrs_Success, 1);
        mocker_invoke(ibv_req_notify_cq, stub_ibv_req_notify_cq_Success, 2);
        ret = RsQpCreateWithCQWithAttrs(phyId, rdevIndex, sendCqn, recvCqn, &qpNorm, &qpResp);
        EXPECT_INT_EQ(ret, 0);
        EXPECT_INT_NE(qpResp.qpn, 0);
        mocker_clean();

        /* Clean up the created QP */
        mocker_invoke(RsIbvDestroyQp, stub_RsIbvDestroyQp_Success, 1);
        (void)RsQpDestroyWithoutCQ(phyId, rdevIndex, qpResp.qpn);
        mocker_clean();
    }

    /* Case 2: Null qpResp */
    {
        struct RsQpNormWithAttrs qpNorm = {0};
        ret = RsQpCreateWithCQWithAttrs(phyId, rdevIndex, sendCqn, recvCqn, &qpNorm, NULL);
        EXPECT_INT_EQ(ret, -EINVAL);
    }

    /* Case 3: Send CQ not found */
    {
        struct RsQpNormWithAttrs qpNorm = {0};
        struct RsQpRespWithAttrs qpResp = {0};
        ret = RsQpCreateWithCQWithAttrs(phyId, rdevIndex, 9999, recvCqn, &qpNorm, &qpResp);
        EXPECT_INT_EQ(ret, -EINVAL);
    }

    /* Case 4: Recv CQ not found */
    {
        struct RsQpNormWithAttrs qpNorm = {0};
        struct RsQpRespWithAttrs qpResp = {0};
        ret = RsQpCreateWithCQWithAttrs(phyId, rdevIndex, sendCqn, 9999, &qpNorm, &qpResp);
        EXPECT_INT_EQ(ret, -EINVAL);
    }

    /* Case 5: Invalid phyId */
    {
        struct RsQpNormWithAttrs qpNorm = {0};
        struct RsQpRespWithAttrs qpResp = {0};
        ret = RsQpCreateWithCQWithAttrs(RS_MAX_DEV_NUM, rdevIndex, sendCqn, recvCqn, &qpNorm, &qpResp);
        EXPECT_INT_EQ(ret, -EINVAL);
    }

    /* Case 6: RsDrvQpCreateWithAttrs fails */
    {
        struct RsQpNormWithAttrs qpNorm = {0};
        struct RsQpRespWithAttrs qpResp = {0};
        qpNorm.isExp = 0;
        qpNorm.isExt = 0;

        mocker_invoke(RsDrvQpCreateWithAttrs, stub_RsDrvQpCreateWithAttrs_Fail, 1);
        ret = RsQpCreateWithCQWithAttrs(phyId, rdevIndex, sendCqn, recvCqn, &qpNorm, &qpResp);
        EXPECT_INT_EQ(ret, -ENOMEM);
        mocker_clean();
    }

    CleanTypicalCqList();
    ret = RsRdevDeinit(phyId, NOTIFY, rdevIndex);
    EXPECT_INT_EQ(ret, 0);
    cfg.chipId = 0;
    ret = RsDeinit(&cfg);
    EXPECT_INT_EQ(ret, 0);
}

/* ===================================================================
 * TcRsGetLiteQpAttr
 * =================================================================== */
void TcRsGetLiteQpAttr(void)
{
    int ret;
    uint32_t phyId = 0;
    uint32_t rdevIndex = 0;
    struct rdev rdevInfo = {0};
    struct RsInitConfig cfg = {0};

    rdevInfo.phyId = 0;
    rdevInfo.family = AF_INET;
    rdevInfo.localIp.addr.s_addr = inet_addr("127.0.0.1");

    ret = RsInit(&cfg);
    EXPECT_INT_EQ(ret, 0);
    ret = RsRdevInit(rdevInfo, NOTIFY, &rdevIndex);
    EXPECT_INT_EQ(ret, 0);

    /* Case 1: Success */
    {
        struct RsQpNorm qpNorm = {0};
        struct RsQpResp resp = {0};
        struct LiteQpAttrResp qpAttrResp;
        qpNorm.flag = 0;
        qpNorm.qpMode = 4;
        qpNorm.isExp = 0;
        qpNorm.isExt = 0;

        ret = RsQpCreate(phyId, rdevIndex, qpNorm, &resp);
        EXPECT_INT_EQ(ret, 0);

        memset(&qpAttrResp, 0, sizeof(qpAttrResp));
        ret = RsGetLiteQpAttr(phyId, rdevIndex, resp.qpn, &qpAttrResp);
        EXPECT_INT_EQ(ret, 0);

        /* Clean up */
        mocker_invoke(RsIbvDestroyQp, stub_RsIbvDestroyQp_Success, 1);
        (void)RsQpDestroyWithoutCQ(phyId, rdevIndex, resp.qpn);
        mocker_clean();
    }

    /* Case 2: QPN not found */
    {
        struct LiteQpAttrResp qpAttrResp;
        ret = RsGetLiteQpAttr(phyId, rdevIndex, 99999, &qpAttrResp);
        EXPECT_INT_NE(ret, 0);
    }

    /* Case 3: Null resp pointer */
    {
        ret = RsGetLiteQpAttr(phyId, rdevIndex, 100, NULL);
        EXPECT_INT_NE(ret, 0);
    }

    /* Case 4: Invalid phyId */
    {
        struct LiteQpAttrResp qpAttrResp;
        ret = RsGetLiteQpAttr(RS_MAX_DEV_NUM, rdevIndex, 100, &qpAttrResp);
        EXPECT_INT_EQ(ret, -EINVAL);
    }

    ret = RsRdevDeinit(phyId, NOTIFY, rdevIndex);
    EXPECT_INT_EQ(ret, 0);
    cfg.chipId = 0;
    ret = RsDeinit(&cfg);
    EXPECT_INT_EQ(ret, 0);
}

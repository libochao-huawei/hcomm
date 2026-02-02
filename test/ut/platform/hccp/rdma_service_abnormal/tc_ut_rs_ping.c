/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include "ascend_hal.h"
#include "ut_dispatch.h"
#include "dl_ibverbs_function.h"
#include "ra_rs_comm.h"
#include "ra_rs_err.h"
#include "rs.h"
#include "hccp_common.h"
#include "rs_inner.h"
#include "rs_ping_inner.h"
#include "hccp_ping.h"
#include "rs_ping.h"
#include "rs_ping_roce.h"
#include "tc_ut_rs_ping.h"

extern int RsPingRocePingCbInit(unsigned int phyId, struct PingInitAttr *attr, struct PingInitInfo *info,
    struct RsPingCtxCb *pingCb);
extern int RsGetPingCb(struct RaRsDevInfo *rdev, struct RsPingCtxCb **pingCb);
extern int RsPingRoceFindTargetNode(struct RsPingCtxCb *pingCb, struct PingQpInfo *target,
    struct RsPingTargetInfo  **node);
extern int RsPingRoceAllocTargetNode(struct RsPingCtxCb *pingCb, struct PingTargetInfo  *target,
    struct RsPingTargetInfo  **node);
extern int RsPingRoceGetTargetResult(struct RsPingCtxCb *pingCb, struct PingTargetCommInfo *target,
    struct PingResultInfo *result);
extern void RsPingCommonDeinitLocalQp(struct rs_cb *rscb, struct RsPingCtxCb *pingCb,
    struct RsPingLocalQpCb *qp_cb);
extern int RsPingCbGetIbCtxAndIndex(struct rdev *rdev_info, struct RsPingCtxCb *pingCb);
extern int RsPingCbGetDevRdevIndex(struct RsPingCtxCb *pingCb, int index);

static struct rs_cb g_tmp_rs_cb0;
static struct rs_cb g_tmp_rs_cb1;
static struct rs_cb g_ping_rs_cb;
static struct RsPingCtxCb g_tmp_ping_cb;
static struct RsPingTargetInfo  g_ping_target_node;
static struct RsPingTargetInfo  g_ping_target_node1;

int RsDev2rscb_stub0(uint32_t dev_id, struct rs_cb **rs_cb, bool init_flag)
{
    *rs_cb = &g_tmp_rs_cb0;

    return 0;
}

int RsDev2rscb_stub1(uint32_t dev_id, struct rs_cb **rs_cb, bool init_flag)
{
    pthread_mutex_init(&g_tmp_rs_cb1.pingCb.pingMutex, NULL);
    g_tmp_rs_cb1.pingCb.threadStatus = RS_PING_THREAD_RUNNING;

    *rs_cb = &g_tmp_rs_cb1;

    return 0;
}

int rs_get_ping_cb_stub(struct RaRsDevInfo *rdev, struct RsPingCtxCb **pingCb)
{
    *pingCb = &g_tmp_ping_cb;
    (*pingCb)->pingPongOps = RsPingRoceGetOps();
    (*pingCb)->pingPongDfx = RsPingRoceGetDfx();

    return 0;
}

int rs_get_rs_cb_stub(unsigned int phyId, struct rs_cb **rs_cb)
{
    *rs_cb = &g_ping_rs_cb;
    (*rs_cb)->pingCb.pingPongOps = RsPingRoceGetOps();
    (*rs_cb)->pingCb.pingPongDfx = RsPingRoceGetDfx();

    return 0;
}

int rs_ping_roce_find_target_node_stub(struct RsPingCtxCb *pingCb, struct PingQpInfo *target,
    struct RsPingTargetInfo  **node)
{
    *node = &g_ping_target_node;

    return 0;
}

int rs_ping_roce_find_target_node_stub1(struct RsPingCtxCb *pingCb, struct PingQpInfo *target,
    struct RsPingTargetInfo  **node)
{
    *node = &g_ping_target_node1;

    return -ENODEV;
}

void tc_rs_ping_handle_init()
{
    unsigned int chipId;
    int hdc_type;
    int ret;

    chipId = 0;
    hdc_type = HDC_SERVICE_TYPE_RDMA;
    ret = RsPingHandleInit(chipId, hdc_type, WHITE_LIST_ENABLE);
    EXPECT_INT_EQ(ret, 0);

    hdc_type = HDC_SERVICE_TYPE_RDMA_V2;
    mocker(RsDev2rscb, 1, -1);
    ret = RsPingHandleInit(chipId, hdc_type, WHITE_LIST_ENABLE);
    EXPECT_INT_EQ(ret, -ENODEV);
    mocker_clean();

    mocker_invoke(RsDev2rscb, RsDev2rscb_stub0, 1);
    mocker((stub_fn_t)pthread_create, 1, -1);
    ret = RsPingHandleInit(chipId, hdc_type, WHITE_LIST_ENABLE);
    EXPECT_INT_EQ(ret, -ESYSFUNC);
    mocker_clean();
}

void tc_rs_ping_handle_deinit()
{
    unsigned int chipId;
    int ret;

    chipId = 0;
    mocker(RsDev2rscb, 1, -1);
    ret = RsPingHandleDeinit(chipId);
    EXPECT_INT_EQ(ret, -ENODEV);
    mocker_clean();

    mocker_invoke(RsDev2rscb, RsDev2rscb_stub0, 1);
    ret = RsPingHandleDeinit(chipId);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    mocker_invoke(RsDev2rscb, RsDev2rscb_stub1, 1);
    ret = RsPingHandleDeinit(chipId);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    pthread_mutex_destroy(&g_tmp_rs_cb1.pingCb.pingMutex);
}

void tc_rs_ping_init()
{
    struct PingInitAttr attr = { 0 };
    struct PingInitInfo info = { 0 };
    unsigned int rdevIndex = 0;
    int ret;

    ret = RsPingInit(&attr, &info, NULL);
    EXPECT_INT_EQ(ret, -EINVAL);

    mocker(RsGetRsCb, 1, -1);
    ret = RsPingInit(&attr, &info, &rdevIndex);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

    mocker(rsGetLocalDevIDByHostDevID, 1, 0);
    mocker_invoke(RsDev2rscb, RsDev2rscb_stub0, 1);
    g_tmp_rs_cb0.pingCb.initCnt = 1;
    ret = RsPingInit(&attr, &info, &rdevIndex);
    EXPECT_INT_NE(ret, 0);
    mocker_clean();

    mocker_invoke(RsGetRsCb, rs_get_rs_cb_stub, 1);
    mocker(rsGetLocalDevIDByHostDevID, 1, -1);
    ret = RsPingInit(&attr, &info, &rdevIndex);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

    mocker_invoke(RsGetRsCb, rs_get_rs_cb_stub, 1);
    mocker(rsGetLocalDevIDByHostDevID, 1, 0);
    mocker(RsSetupSharemem, 20, -1);
    ret = RsPingInit(&attr, &info, &rdevIndex);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

    mocker_invoke(RsGetRsCb, rs_get_rs_cb_stub, 1);
    mocker(rsGetLocalDevIDByHostDevID, 1, 0);
    mocker(RsSetupSharemem, 20, 0);
    mocker(RsPingRocePingCbInit, 1, -1);
    ret = RsPingInit(&attr, &info, &rdevIndex);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();
}

void tc_rs_ping_target_add()
{
    struct PingTargetInfo  target = { 0 };
    struct RaRsDevInfo rdev = { 0 };
    int ret;

    ret = RsPingTargetAdd(&rdev, NULL);
    EXPECT_INT_EQ(ret, -EINVAL);

    target.payload.size = PING_USER_PAYLOAD_MAX_SIZE + 1;
    ret = RsPingTargetAdd(&rdev, &target);
    EXPECT_INT_EQ(ret, -EINVAL);

    target.payload.size = PING_USER_PAYLOAD_MAX_SIZE;
    mocker(RsGetPingCb, 1, -1);
    ret = RsPingTargetAdd(&rdev, &target);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

    g_tmp_ping_cb.taskStatus = RS_PING_TASK_RUNNING;
    mocker_invoke(RsGetPingCb, rs_get_ping_cb_stub, 1);
    ret = RsPingTargetAdd(&rdev, &target);
    EXPECT_INT_NE(ret, 0);
    mocker_clean();

    g_tmp_ping_cb.taskStatus = RS_PING_TASK_RESET;
    mocker_invoke(RsGetPingCb, rs_get_ping_cb_stub, 1);
    mocker_invoke(RsPingRoceFindTargetNode, rs_ping_roce_find_target_node_stub, 1);
    ret = RsPingTargetAdd(&rdev, &target);
    EXPECT_INT_NE(ret, 0);
    mocker_clean();

    mocker_invoke(RsGetPingCb, rs_get_ping_cb_stub, 1);
    mocker_invoke(RsPingRoceFindTargetNode, rs_ping_roce_find_target_node_stub1, 1);
    mocker(RsPingRoceAllocTargetNode, 1, -1);
    ret = RsPingTargetAdd(&rdev, &target);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();
}

void tc_rs_ping_task_start()
{
    struct RsPingTargetInfo  tmp_target = {0};
    struct RaRsDevInfo rdev = { 0 };
    struct PingTaskAttr attr = { 0 };
    int ret;

    ret = RsPingTaskStart(&rdev, NULL);
    EXPECT_INT_EQ(ret, -EINVAL);

    mocker(RsGetPingCb, 1, -1);
    ret = RsPingTaskStart(&rdev, &attr);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

    g_tmp_ping_cb.taskStatus = RS_PING_TASK_RUNNING;
    mocker_invoke(RsGetPingCb, rs_get_ping_cb_stub, 1);
    ret = RsPingTaskStart(&rdev, &attr);
    EXPECT_INT_NE(ret, 0);
    mocker_clean();

    g_tmp_ping_cb.taskStatus = RS_PING_TASK_RESET;
    attr.packetCnt = 1;
    attr.packetInterval = 1;
    attr.timeoutInterval = 1;
    pthread_mutex_init(&g_tmp_ping_cb.pongQp.recvMrCb.mutex, NULL);
    pthread_mutex_init(&g_tmp_ping_cb.pingQp.recvMrCb.mutex, NULL);
    pthread_mutex_init(&g_tmp_ping_cb.pingMutex, NULL);
    RS_INIT_LIST_HEAD(&g_tmp_ping_cb.pingList);
    RsListAddTail(&tmp_target.list, &g_tmp_ping_cb.pingList);
    mocker_invoke(RsGetPingCb, rs_get_ping_cb_stub, 1);
    ret = RsPingTaskStart(&rdev, &attr);
    EXPECT_INT_EQ(ret, 0);
    RsListDel(&tmp_target.list);
    mocker_clean();
    pthread_mutex_destroy(&g_tmp_ping_cb.pingMutex);
    pthread_mutex_destroy(&g_tmp_ping_cb.pingQp.recvMrCb.mutex);
    pthread_mutex_destroy(&g_tmp_ping_cb.pongQp.recvMrCb.mutex);
}

void tc_rs_ping_get_results()
{
    struct PingTargetCommInfo target= { 0 };
    struct PingResultInfo result = { 0 };
    struct RaRsDevInfo rdev = { 0 };
    unsigned int num = 1;
    int ret;

    ret = RsPingGetResults(NULL, &target, &num, &result);
    EXPECT_INT_EQ(ret, -EINVAL);

    num = 1;
    mocker(RsGetPingCb, 1, -1);
    ret = RsPingGetResults(&rdev, &target, &num, &result);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

    num = 1;
    g_tmp_ping_cb.taskStatus = RS_PING_TASK_RUNNING;
    mocker_invoke(RsGetPingCb, rs_get_ping_cb_stub, 1);
    ret = RsPingGetResults(&rdev, &target, &num, &result);
    EXPECT_INT_NE(ret, 0);
    mocker_clean();

    num = 1;
    g_tmp_ping_cb.taskStatus = RS_PING_TASK_RESET;
    mocker_invoke(RsGetPingCb, rs_get_ping_cb_stub, 1);
    mocker(RsPingRoceGetTargetResult, 1, -1);
    ret = RsPingGetResults(&rdev, &target, &num, &result);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();
}

void tc_rs_ping_task_stop()
{
    struct RaRsDevInfo rdev = { 0 };
    int ret;

    ret = RsPingTaskStop(NULL);
    EXPECT_INT_EQ(ret, -EINVAL);

    mocker(RsGetPingCb, 1, -1);
    ret = RsPingTaskStop(&rdev);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();
}

void tc_rs_ping_target_del()
{
    struct PingTargetCommInfo target = { 0 };
    struct RaRsDevInfo rdev = { 0 };
    unsigned int num = 1;
    int ret;

    ret = RsPingTargetDel(&rdev, &target, NULL);
    EXPECT_INT_EQ(ret, -EINVAL);

    mocker(RsGetPingCb, 1, -1);
    ret = RsPingTargetDel(&rdev, &target, &num);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

    g_tmp_ping_cb.taskStatus = RS_PING_TASK_RUNNING;
    mocker_invoke(RsGetPingCb, rs_get_ping_cb_stub, 1);
    ret = RsPingTargetDel(&rdev, &target, &num);
    EXPECT_INT_NE(ret, 0);
    mocker_clean();

    g_tmp_ping_cb.taskStatus = RS_PING_TASK_RESET;
    mocker_invoke(RsGetPingCb, rs_get_ping_cb_stub, 1);
    mocker(RsPingRoceFindTargetNode, 1, -1);
    ret = RsPingTargetDel(&rdev, &target, &num);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();
}

void tc_rs_ping_deinit()
{
    struct RaRsDevInfo rdev = { 0 };
    int ret;

    ret = RsPingDeinit(NULL);
    EXPECT_INT_EQ(ret, -EINVAL);

    mocker(RsGetPingCb, 1, -1);
    ret = RsPingDeinit(&rdev);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

    g_tmp_ping_cb.initCnt = 0;
    mocker_invoke(RsGetPingCb, rs_get_ping_cb_stub, 1);
    ret = RsPingDeinit(&rdev);
    EXPECT_INT_EQ(ret, -ENODEV);
    mocker_clean();
}

void tc_rs_ping_urma_check_fd()
{
    urma_jfce_t jf = {0};
    struct RsPingCtxCb pingCb;
    pingCb.ping_jetty.jfce = &jf;
    pingCb.ping_jetty.jfce->fd = 1;
    pingCb.pong_jetty.jfce = &jf;
    pingCb.pong_jetty.jfce->fd = 1;
    int ret;

    ret = rs_ping_urma_check_fd(&pingCb, 1);
    EXPECT_INT_EQ(ret, 1);

    ret = rs_ping_urma_check_fd(&pingCb, 0);
    EXPECT_INT_EQ(ret, 0);

    ret = rs_pong_urma_check_fd(&pingCb, 1);
    EXPECT_INT_EQ(ret, 1);

    ret = rs_pong_urma_check_fd(&pingCb, 0);
    EXPECT_INT_EQ(ret, 0);
}

void tc_rs_ping_cb_get_ib_ctx_and_index()
{
    struct RsPingCtxCb pingCb = {0};
    struct ibv_device *devList[1] = {0};
    struct ibv_device dev_node = {0};
    struct rdev rdev_info = {0};
    int ret = 0;

    pingCb.rdevCb.devNum = 1;
    pingCb.rdevCb.devList = &devList;
    devList[0] = &dev_node;
    mocker(RsQueryGid, 1, 0);
    mocker(RsPingCbGetDevRdevIndex, 1, 0);
    mocker(RsIbvQueryGid, 1, -1);
    ret = RsPingCbGetIbCtxAndIndex(&rdev_info, &pingCb);
    EXPECT_INT_EQ(ret, -EOPENSRC);
    mocker_clean();
}

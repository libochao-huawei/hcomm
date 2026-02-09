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
#include "dl_hal_function.h"
#include "dl_ibverbs_function.h"
#include "rs_socket.h"
#include "ut_dispatch.h"
#include "ra_rs_comm.h"
#include "rs.h"
#include "hccp_common.h"
#include "rs_inner.h"
#include "rs_ping_inner.h"
#include "hccp_ping.h"
#include "rs_ping.h"
#include "rs_ping_roce.h"
#include "tc_ut_rs_ping.h"

extern int RsPingCbGetIbCtxAndIndex(struct rdev *rdev_info, struct RsPingCtxCb *pingCb);
extern void RsEpollCtl(int epollfd, int op, int fd, int state);
extern int RsGetPingCb(struct RaRsDevInfo *rdev, struct RsPingCtxCb **pingCb);
extern int RsPingCommonPostRecv(struct RsPingLocalQpCb *qp_cb);
extern int RsPingCommonInitPostRecvAll(struct RsPingLocalQpCb *qp_cb);
extern void RsPingCommonDeinitLocalBuffer(struct RsPingCtxCb *pingCb);
extern int RsPingPongInitLocalBuffer(struct rs_cb *rscb, struct PingInitAttr *attr, struct PingInitInfo *info,
    struct RsPingCtxCb *pingCb);
extern int RsPingCommonInitLocalQp(struct rs_cb *rscb, struct RsPingCtxCb *pingCb, union PingQpAttr*attr,
    struct RsPingLocalQpCb *qp_cb);
extern int RsPingRoceFindTargetNode(struct RsPingCtxCb *pingCb, struct PingQpInfo *target,
    struct RsPingTargetInfo  **node);
extern int RsPingRocePostSend(struct RsPingCtxCb *pingCb, struct RsPingTargetInfo  *target);
extern int RsPingRoceGetTargetResult(struct RsPingCtxCb *pingCb, struct PingTargetCommInfo *target,
    struct PingResultInfo *result);
extern int RsPingRoceAllocTargetNode(struct RsPingCtxCb *pingCb, struct PingTargetInfo  *target,
    struct RsPingTargetInfo  **node);
extern bool RsPingCommonCompareRdmaInfo(struct PingQpInfo *a, struct PingQpInfo *b);
extern int RsPongFindTargetNode(struct RsPingCtxCb *pingCb, struct PingQpInfo *target,
    struct RsPongTargetInfo **node);
extern int RsPongFindAllocTargetNode(struct RsPingCtxCb *pingCb, struct PingQpInfo *target,
    struct RsPongTargetInfo **node);
extern int RsPingCommonCreateAh(struct RsPingCtxCb *pingCb, struct PingLocalCommInfo *local_info,
    struct PingQpInfo *remote_info, struct ibv_ah **ah);
extern int RsPingCommonPollScq(struct RsPingLocalQpCb *qp_cb);
extern int RsPongPostSend(struct RsPingCtxCb *pingCb, struct ibv_wc *wc, struct timeval *timestamp2);
extern int RsPingRocePollRcq(struct RsPingCtxCb *pingCb, int *polled_cnt, struct timeval *timestamp2);
extern void RsPongRoceHandleSend(struct RsPingCtxCb *pingCb, int polled_cnt, struct timeval *timestamp2);
extern int RsPongResolveResponsePacket(struct RsPingCtxCb *pingCb, uint32_t sge_idx,
    struct timeval *timestamp4);
extern void RsPongRocePollRcq(struct RsPingCtxCb *pingCb);
extern int RsPingCbGetDevRdevIndex(struct RsPingCtxCb *pingCb, int index);
extern int RsPingCommonInitMrCb(struct rs_cb *rscb, struct RsPingCtxCb *pingCb, struct RsPingMrCb *mr_cb);
extern void RsPingCommonDeinitMrCb(struct RsPingMrCb *mr_cb);
extern struct ibv_mr* RsDrvMrReg(struct ibv_pd *pd, char *addr, size_t length, int access);
extern int RsDrvMrDereg(struct ibv_mr *ibMr);
extern int RsPingCommonModifyLocalQp(struct RsPingCtxCb *pingCb, struct RsPingLocalQpCb *qp_cb);
extern void RsPingCommonDeinitLocalQp(struct rs_cb *rscb, struct RsPingCtxCb *pingCb,
    struct RsPingLocalQpCb *qp_cb);
extern int RsPingPongInitLocalInfo(struct rs_cb *rscb, struct PingInitAttr *attr, struct PingInitInfo *info,
    struct RsPingCtxCb *pingCb);
extern int RsPingRocePollScq(struct RsPingCtxCb *pingCb, struct RsPingTargetInfo  *target);
extern void RsPingPongDelTargetList(struct RsPingCtxCb *pingCb);
extern void RsPingRocePingCbDeinit(unsigned int phyId, struct RsPingCtxCb *pingCb);
extern int RsPingRocePingCbInit(unsigned int phyId, struct PingInitAttr *attr, struct PingInitInfo *info,
    unsigned int *dev_index, struct RsPingCtxCb *pingCb);

static struct rs_cb g_tmp_rs_cb0;
static struct rs_cb g_tmp_rs_cb;
static struct rs_cb g_tmp_rs_cb1;
static struct rs_cb g_tmp_rs_cb_t;
static struct RsPingCtxCb g_tmp_ping_cb;
static struct ibv_cq g_tmp_cq;
static struct RsPingTargetInfo  g_tmp_target;
static struct RsPingTargetInfo  g_tmp_target1;
static struct ibv_mr g_ib_mr;

int RsDev2rscb_stub0(uint32_t dev_id, struct rs_cb **rs_cb, bool init_flag)
{
    *rs_cb = &g_tmp_rs_cb0;

    return 0;
}

int rs_get_rs_cb_stub(unsigned int phyId, struct rs_cb **rs_cb)
{
    *rs_cb = &g_tmp_rs_cb;
    (*rs_cb)->pingCb.pingPongOps = RsPingRoceGetOps();
    (*rs_cb)->pingCb.pingPongDfx = RsPingRoceGetDfx();
    return 0;
}

int rs_get_rs_cb_stub_true(unsigned int phyId, struct rs_cb **rs_cb)
{
    *rs_cb = &g_tmp_rs_cb_t;
    (*rs_cb)->pingCb.threadStatus = RS_PING_THREAD_RUNNING;
    return 0;
}

int rs_get_rs_cb_stub1(unsigned int phyId, struct rs_cb **rs_cb)
{
    g_tmp_rs_cb1.pingCb.threadStatus = RS_PING_THREAD_RUNNING;

    *rs_cb = &g_tmp_rs_cb1;
    (*rs_cb)->pingCb.pingPongOps = RsPingRoceGetOps();
    (*rs_cb)->pingCb.pingPongDfx = RsPingRoceGetDfx();
    return 0;
}

int rs_get_ping_cb_stub(struct RaRsDevInfo *rdev, struct RsPingCtxCb **pingCb)
{
    *pingCb = &g_tmp_ping_cb;
    (*pingCb)->pingPongOps = RsPingRoceGetOps();
    (*pingCb)->pingPongDfx = RsPingRoceGetDfx();
    return 0;
}

int rs_ping_roce_find_target_node_stub(struct RsPingCtxCb *pingCb, struct PingQpInfo *target,
    struct RsPingTargetInfo  **node)
{
    *node = &g_tmp_target;

    return -ENODEV;
}

int rs_ping_roce_find_target_node_stub1(struct RsPingCtxCb *pingCb, struct PingQpInfo *target,
    struct RsPingTargetInfo  **node)
{
    *node = &g_tmp_target1;

    return 0;
}

int rs_ping_roce_find_target_node_stub2(struct RsPingCtxCb *pingCb, struct PingQpInfo *target,
    struct RsPingTargetInfo  **node)
{
    *node = calloc(1, sizeof(struct RsPingTargetInfo ));
    RS_INIT_LIST_HEAD(&(*node)->list);
    (*node)->payloadSize = 1;
    (*node)->payloadBuffer = malloc(1);

    return 0;
}

int rs_pong_find_target_node_stub(struct RsPingCtxCb *pingCb, struct PingQpInfo *target,
    struct RsPongTargetInfo **node)
{
    *node = NULL;
    return -19;
}

int rs_pong_find_target_node_stub2(struct RsPingCtxCb *pingCb, struct PingQpInfo *target,
    struct RsPongTargetInfo **node)
{
    *node = calloc(1, sizeof(struct RsPongTargetInfo));
    RS_INIT_LIST_HEAD(&(*node)->list);
    return 0;
}

int rs_pong_find_alloc_target_node_stub(struct RsPingCtxCb *pingCb, struct PingQpInfo *target,
    struct RsPongTargetInfo **node)
{
    struct RsPongTargetInfo tmp_node = { 0 };

    *node = &tmp_node;

    return 0;
}

int RsIbvGetCqEvent_stub(struct ibv_comp_channel *channel, struct ibv_cq **cq, void **cq_context)
{
    *cq = &g_tmp_cq;

    return 0;
}

struct ibv_mr* rs_drv_mr_reg_stub(struct ibv_pd *pd, char *addr, size_t length, int access)
{
    return &g_ib_mr;
}

int rs_drv_mr_dereg_stub(struct ibv_mr *ibMr)
{
    return 0;
}

void tc_rs_payload_header_resv_custom_check()
{
    /* the llt verifies the size of the RS_PING_PAYLOAD_HEADER_RESV_CUSTOM field */
    EXPECT_INT_EQ(RS_PING_PAYLOAD_HEADER_RESV_CUSTOM, 216);
}

void tc_rs_ping_handle_init()
{
    unsigned int white_list_status = WHITE_LIST_DISABLE;
    int hdc_type = HDC_SERVICE_TYPE_RDMA_V2;
    unsigned int chipId = 0;
    int ret;

    mocker_invoke(RsDev2rscb, RsDev2rscb_stub0, 1);
    ret = RsPingHandleInit(hdc_type, chipId, white_list_status);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

void tc_rs_ping_handle_deinit()
{
    unsigned int chipId = 0;
    int ret;

    g_tmp_rs_cb.pingCb.threadStatus = RS_PING_THREAD_RUNNING;
    pthread_mutex_init(&g_tmp_rs_cb.pingCb.pingMutex, NULL);
    mocker_invoke(RsDev2rscb, RsDev2rscb_stub0, 1);
    ret = RsPingHandleDeinit(chipId);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
    pthread_mutex_destroy(&g_tmp_rs_cb.pingCb.pingMutex);
}

void tc_rs_ping_init()
{
    struct RsPingLocalQpCb qp_cb = { 0 };
    union PingQpAttr rdma_attr = { 0 };
    struct RsPingCtxCb pingCb = { 0 };
    struct ibv_context context = { 0 };
    struct PingInitAttr attr = { 0 };
    struct PingInitInfo info = { 0 };
    struct RaRsDevInfo rdev = { 0 };
    struct rs_cb tmp_rs_cb = { 0 };
    struct ibv_pd tmp_pd = { 0 };
    unsigned int rdevIndex = 0;
    struct ibv_qp ibQp = { 0 };
    struct ibv_pd pd = { 0 };
    int ret;

    mocker_invoke(RsGetRsCb, rs_get_rs_cb_stub, 20);
    mocker(rsGetLocalDevIDByHostDevID, 20, 0);
    mocker(RsInetNtop, 20, 0);
    mocker(RsSetupSharemem, 20, 0);
    ret = RsPingInit(&attr, &info, &rdevIndex);
    EXPECT_INT_EQ(ret, -ENODEV);
    mocker_clean();

    mocker_invoke(RsGetRsCb, rs_get_rs_cb_stub_true, 20);
    mocker(rsGetLocalDevIDByHostDevID, 20, 0);
    mocker(RsInetNtop, 20, 0);
    mocker(RsSetupSharemem, 20, 0);
    mocker(RsPingRocePingCbInit, 20, 0);
    ret = RsPingInit(&attr, &info, &rdevIndex);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    qp_cb.ibQp = &ibQp;
    mocker(RsIbvQueryQp, 20, -1);
    ret = RsPingCommonModifyLocalQp(&pingCb, &qp_cb);
    EXPECT_INT_NE(ret, 0);
    mocker_clean();

    mocker(RsIbvQueryQp, 20, 0);
    mocker(RsIbvModifyQp, 20, -1);
    ret = RsPingCommonModifyLocalQp(&pingCb, &qp_cb);
    EXPECT_INT_NE(ret, 0);
    mocker_clean();

    mocker(RsIbvQueryQp, 20, 0);
    mocker(RsIbvModifyQp, 20, 0);
    ret = RsPingCommonModifyLocalQp(&pingCb, &qp_cb);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    mocker((stub_fn_t)RsIbvCreateCq, 10, NULL);
    ret = RsPingCommonInitLocalQp(&tmp_rs_cb, &pingCb, &rdma_attr, &qp_cb);
    EXPECT_INT_NE(ret, 0);
    mocker_clean();

    pingCb.rdevCb.ibCtx = &context;
    pingCb.rdevCb.ibPd = &tmp_pd;
    mocker(RsEpollCtl, 20, 0);
    mocker(RsIbvExpCreateQp, 10, 0);
    ret = RsPingCommonInitLocalQp(&tmp_rs_cb, &pingCb, &rdma_attr, &qp_cb);
    EXPECT_INT_NE(ret, 0);
    mocker_clean();

    mocker(RsPingCommonInitMrCb, 20, -1);
    ret = RsPingPongInitLocalBuffer(&tmp_rs_cb, &attr, &info, &pingCb);
    EXPECT_INT_NE(ret, 0);
    mocker_clean();

    mocker(RsPingCommonInitMrCb, 20, 0);
    ret = RsPingPongInitLocalBuffer(&tmp_rs_cb, &attr, &info, &pingCb);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    qp_cb.recvMrCb.sgeNum = 1;
    qp_cb.qpCap.max_recv_wr = 1;
    mocker(RsPingCommonPostRecv, 20, 0);
    ret = RsPingCommonInitPostRecvAll(&qp_cb);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    qp_cb.recvMrCb.sgeNum = 1;
    qp_cb.qpCap.max_recv_wr = 1;
    mocker(RsPingCommonPostRecv, 20, -1);
    ret = RsPingCommonInitPostRecvAll(&qp_cb);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();
}

void tc_rs_ping_target_add()
{
    struct PingTargetInfo  target = { 0 };
    struct RaRsDevInfo rdev = { 0 };
    int ret;

    target.localInfo.rdma.hopLimit = 64;
    target.localInfo.rdma.qosAttr.tc = (33 & 0x3f) << 2;
    target.localInfo.rdma.qosAttr.sl = 4;
    g_tmp_ping_cb.taskStatus = RS_PING_TASK_RESET;
    RS_INIT_LIST_HEAD(&g_tmp_ping_cb.pingList);
    mocker_invoke(RsGetPingCb, rs_get_ping_cb_stub, 1);
    ret = RsPingTargetAdd(&rdev, &target);
    EXPECT_INT_EQ(ret, -9);
    mocker_clean();

    target.payload.size = 1;
    g_tmp_ping_cb.taskStatus = RS_PING_TASK_RESET;
    RS_INIT_LIST_HEAD(&g_tmp_ping_cb.pingList);
    mocker_invoke(RsGetPingCb, rs_get_ping_cb_stub, 1);
    mocker(RsPingCommonCreateAh, 1, -1);
    ret = RsPingTargetAdd(&rdev, &target);
    EXPECT_INT_NE(ret, 0);
    mocker_clean();

    mocker_invoke(RsGetPingCb, rs_get_ping_cb_stub, 1);
    mocker(calloc, 10, 0);
    ret = RsPingTargetAdd(&rdev, &target);
    EXPECT_INT_NE(ret, 0);
    mocker_clean();
}

void tc_rs_get_ping_cb()
{
    struct RaRsDevInfo rdev = { 0 };
    struct RsPingCtxCb *pingCb;
    int ret;

    mocker(RsGetRsCb, 1, -1);
    ret = RsGetPingCb(&rdev, &pingCb);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

    mocker_invoke(RsGetRsCb, rs_get_rs_cb_stub1, 1);
    ret = RsGetPingCb(&rdev, &pingCb);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

void tc_rs_ping_client_post_send()
{
    struct RsPingTargetInfo  target = { 0 };
    struct RsPingCtxCb pingCb = { 0 };
    struct ibv_qp server_ib_qp = { 0 };
    struct ibv_qp client_ib_qp = { 0 };
    struct ibv_sge sge = { 0 };
    void *addr = malloc(256);
    int ret;

    target.payloadBuffer = malloc(1);
    target.payloadSize = 1;
    sge.addr = (uintptr_t)addr;
    sge.length = 256;
    pingCb.pingQp.sendMrCb.sgeNum = 1;
    pingCb.pingQp.sendMrCb.sgeList = &sge;
    pingCb.pongQp.ibQp = &server_ib_qp;
    pingCb.pingQp.ibQp = &client_ib_qp;
    mocker(RsIbvPostSend, 1, -1);
    ret = RsPingRocePostSend(&pingCb, &target);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

    mocker(RsIbvPostSend, 1, 0);
    ret = RsPingRocePostSend(&pingCb, &target);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    free(addr);
    addr = NULL;
    free(target.payloadBuffer);
    target.payloadBuffer = NULL;
}

void tc_rs_ping_get_results()
{
    struct PingTargetCommInfo target = { 0 };
    struct PingResultInfo result = { 0 };
    struct RaRsDevInfo rdev = { 0 };
    unsigned int num = 1;
    int ret;

    g_tmp_ping_cb.taskStatus = RS_PING_TASK_RESET;
    mocker_invoke(RsGetPingCb, rs_get_ping_cb_stub, 1);
    mocker_invoke(RsPingRoceFindTargetNode, rs_ping_roce_find_target_node_stub, 1);
    ret = RsPingGetResults(&rdev, &target, &num, &result);
    EXPECT_INT_EQ(ret, -ENODEV);
    mocker_clean();

    num = 1;
    g_tmp_ping_cb.taskStatus = RS_PING_TASK_RESET;
    mocker_invoke(RsGetPingCb, rs_get_ping_cb_stub, 1);
    mocker_invoke(RsPingRoceFindTargetNode, rs_ping_roce_find_target_node_stub1, 1);
    ret = RsPingGetResults(&rdev, &target, &num, &result);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

void tc_rs_ping_task_stop()
{
    struct RaRsDevInfo rdev = { 0 };
    int ret;

    pthread_mutex_init(&g_tmp_ping_cb.pingMutex, NULL);
    mocker_invoke(RsGetPingCb, rs_get_ping_cb_stub, 1);
    ret = RsPingTaskStop(&rdev);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
    pthread_mutex_destroy(&g_tmp_ping_cb.pingMutex);
}

void tc_rs_ping_target_del()
{
    struct PingTargetCommInfo target = { 0 };
    struct RaRsDevInfo rdev = { 0 };
    unsigned int num = 1;
    int ret;

    g_tmp_ping_cb.taskStatus = RS_PING_TASK_RESET;
    mocker_invoke(RsGetPingCb, rs_get_ping_cb_stub, 1);
    mocker_invoke(RsPingRoceFindTargetNode, rs_ping_roce_find_target_node_stub2, 1);
    ret = RsPingTargetDel(&rdev, &target, &num);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

void tc_rs_ping_deinit()
{
    struct ibv_comp_channel client_channel = { 0 };
    struct ibv_comp_channel server_channel = { 0 };
    struct PingTargetInfo  target = { 0 };
    struct RaRsDevInfo rdev = { 0 };
    struct rs_cb rs_cb = { 0 };
    int ret;

    g_tmp_ping_cb.initCnt = 1;
    g_tmp_ping_cb.pingQp.channel = &client_channel;
    g_tmp_ping_cb.pongQp.channel = &server_channel;
    RS_INIT_LIST_HEAD(&g_tmp_ping_cb.pingList);
    RS_INIT_LIST_HEAD(&g_tmp_ping_cb.pingList);
    target.payload.size = 1;
    mocker_invoke(RsGetPingCb, rs_get_ping_cb_stub, 10);
    mocker(RsPingCommonCreateAh, 1, 0);
    ret = RsPingTargetAdd(&rdev, &target);
    EXPECT_INT_EQ(ret, 0);
    mocker(RsEpollCtl, 20, 0);
    mocker(RsIbvDestroyCompChannel, 20, 0);
    ret = RsPingDeinit(&rdev);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

void tc_rs_ping_compare_rdma_info()
{
    struct PingQpInfo a = { 0 };
    struct PingQpInfo b = { 0 };
    int ret;

    ret = RsPingCommonCompareRdmaInfo(&a, &b);
    EXPECT_INT_EQ(ret, true);

    a.rdma.qpn = 1;
    b.rdma.qpn = 2;
    ret = RsPingCommonCompareRdmaInfo(&a, &b);
    EXPECT_INT_EQ(ret, false);

    a.rdma.qpn = 1;
    b.rdma.qpn = 1;
    a.rdma.qkey = 1;
    b.rdma.qkey = 2;
    ret = RsPingCommonCompareRdmaInfo(&a, &b);
    EXPECT_INT_EQ(ret, false);

    a.rdma.qpn = 1;
    b.rdma.qpn = 1;
    a.rdma.qkey = 1;
    b.rdma.qkey = 1;
    a.rdma.gid.raw[0] = 1;
    b.rdma.gid.raw[0] = 2;
    ret = RsPingCommonCompareRdmaInfo(&a, &b);
    EXPECT_INT_EQ(ret, false);
}

void tc_rs_ping_roce_find_target_node()
{
    struct RsPingTargetInfo  stub_node = { 0 };
    struct RsPingTargetInfo  *node = NULL;
    struct RsPingCtxCb pingCb = { 0 };
    struct PingQpInfo target = { 0 };
    int ret;

    RS_INIT_LIST_HEAD(&pingCb.pingList);
    ret = RsPingRoceFindTargetNode(&pingCb, &target, &node);
    EXPECT_INT_EQ(ret, -ENODEV);

    RsListAddTail(&stub_node.list, &pingCb.pingList);
    ret = RsPingRoceFindTargetNode(&pingCb, &target, &node);
    EXPECT_INT_EQ(ret, 0);
}

void tc_rs_pong_find_target_node()
{
    struct RsPongTargetInfo stub_node = { 0 };
    struct RsPongTargetInfo *node = NULL;
    struct RsPingCtxCb pingCb = { 0 };
    struct PingQpInfo target = { 0 };
    int ret;

    RS_INIT_LIST_HEAD(&pingCb.pongList);
    ret = RsPongFindTargetNode(&pingCb, &target, &node);
    EXPECT_INT_EQ(ret, -ENODEV);

    RsListAddTail(&stub_node.list, &pingCb.pongList);
    ret = RsPongFindTargetNode(&pingCb, &target, &node);
    EXPECT_INT_EQ(ret, 0);
}

void tc_rs_pong_find_alloc_target_node()
{
    struct RsPongTargetInfo *node = NULL;
    struct RsPingCtxCb pingCb = { 0 };
    struct PingQpInfo target = { 0 };
    int ret;

    mocker_invoke(RsPongFindTargetNode, rs_pong_find_target_node_stub2, 1);
    mocker(RsPingCommonCreateAh, 1, -1);
    ret = RsPongFindAllocTargetNode(&pingCb, &target, &node);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

    pthread_mutex_init(&pingCb.pongMutex, NULL);
    RS_INIT_LIST_HEAD(&pingCb.pongList);
    mocker_invoke(RsPongFindTargetNode, rs_pong_find_target_node_stub, 1);
    mocker(RsPingCommonCreateAh, 1, 0);
    ret = RsPongFindAllocTargetNode(&pingCb, &target, &node);
    free(node);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

void tc_rs_ping_poll_send_cq()
{
    struct RsPingLocalQpCb qp_cb = { 0 };
    int ret;

    mocker(RsIbvPollCq, 1, -1);
    ret = RsPingCommonPollScq(&qp_cb);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    mocker(RsIbvPollCq, 1, 1);
    ret = RsPingCommonPollScq(&qp_cb);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

void tc_rs_ping_server_post_send()
{
    struct RsPingCtxCb pingCb = { 0 };
    struct timeval timestamp2;
    void *send_addr = malloc(1024);
    void *recv_addr = malloc(1024);
    struct ibv_wc wc = { 0};
    int ret;

    wc.wr_id = 1;
    mocker(RsPingCommonPollScq, 1, 0);
    ret = RsPongPostSend(&pingCb, &wc, &timestamp2);
    EXPECT_INT_EQ(ret, -EIO);
    mocker_clean();

    wc.wr_id = 0;
    wc.byte_len = 16 + RS_PING_PAYLOAD_HEADER_RESV_GRH;
    pingCb.pingQp.recvMrCb.sgeList = calloc(1, sizeof(struct ibv_sge));
    pingCb.pingQp.recvMrCb.sgeNum = 1;
    pingCb.pongQp.sendMrCb.sgeList = calloc(1, sizeof(struct ibv_sge));
    pingCb.pongQp.sendMrCb.sgeNum = 1;
    pingCb.pongQp.sendMrCb.sgeList->addr = (uintptr_t)send_addr;
    pingCb.pongQp.sendMrCb.sgeList->length = 1024;
    pingCb.pingQp.recvMrCb.sgeList->addr = (uintptr_t)recv_addr;
    pingCb.pingQp.recvMrCb.sgeList->length = 1024;
    mocker(RsPingCommonPollScq, 1, 0);
    mocker(RsPongFindAllocTargetNode, 1, -1);
    ret = RsPongPostSend(&pingCb, &wc, &timestamp2);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

    mocker(RsPingCommonPollScq, 1, 0);
    mocker_invoke(RsPongFindAllocTargetNode, rs_pong_find_alloc_target_node_stub, 1);
    mocker(gettimeofday, 20, 1);
    mocker(RsIbvPostSend, 1, -1);
    ret = RsPongPostSend(&pingCb, &wc, &timestamp2);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

    mocker(RsPingCommonPollScq, 1, 0);
    mocker_invoke(RsPongFindAllocTargetNode, rs_pong_find_alloc_target_node_stub, 1);
    mocker(RsIbvPostSend, 1, 0);
    ret = RsPongPostSend(&pingCb, &wc, &timestamp2);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    free(pingCb.pongQp.sendMrCb.sgeList);
    pingCb.pongQp.sendMrCb.sgeList = NULL;
    free(pingCb.pingQp.recvMrCb.sgeList);
    pingCb.pingQp.recvMrCb.sgeList = NULL;
    free(recv_addr);
    recv_addr = NULL;
    free(send_addr);
    send_addr = NULL;
}

void tc_rs_ping_post_recv()
{
    struct RsPingLocalQpCb qp_cb = { 0 };
    void *recv_addr = malloc(1024);
    int ret;

    qp_cb.recvMrCb.sgeList = calloc(1, sizeof(struct ibv_sge));
    qp_cb.recvMrCb.sgeList->addr = (uintptr_t)recv_addr;
    qp_cb.recvMrCb.sgeList->length = 1024;
    qp_cb.recvMrCb.sgeNum = 1;
    mocker(RsIbvPostRecv, 1, -1);
    ret = RsPingCommonPostRecv(&qp_cb);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

    mocker(RsIbvPostRecv, 1, 0);
    ret = RsPingCommonPostRecv(&qp_cb);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    free(qp_cb.recvMrCb.sgeList);
    qp_cb.recvMrCb.sgeList = NULL;
    free(recv_addr);
    recv_addr = NULL;
}

void tc_rs_ping_client_poll_cq()
{
    struct RsPingCtxCb pingCb = { 0 };
    struct timeval timestamp2;
    int polled_cnt;
    int ret;

    mocker_clean();
    mocker(RsIbvGetCqEvent, 1, -1);
    ret = RsPingRocePollRcq(&pingCb, &polled_cnt, &timestamp2);
    EXPECT_INT_EQ(ret, -259);
    mocker_clean();

    mocker(RsIbvGetCqEvent, 1, 0);
    mocker(RsIbvPollCq, 1, 0);
    ret = RsPingRocePollRcq(&pingCb, &polled_cnt, &timestamp2);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    pingCb.pingQp.recvCq.ibCq = &g_tmp_cq;
    mocker_invoke(RsIbvGetCqEvent, RsIbvGetCqEvent_stub, 1);
    mocker(RsIbvPollCq, 1, -1);
    ret = RsPingRocePollRcq(&pingCb, &polled_cnt, &timestamp2);
    EXPECT_INT_EQ(ret, -259);
    mocker_clean();

    pingCb.pingQp.recvCq.maxRecvWcNum = 16;
    mocker_invoke(RsIbvGetCqEvent, RsIbvGetCqEvent_stub, 1);
    mocker(RsIbvPollCq, 1, 1);
    mocker(RsPongPostSend, 1, -1);
    ret = RsPingRocePollRcq(&pingCb, &polled_cnt, &timestamp2);
    EXPECT_INT_EQ(ret, 0);
    RsPongRoceHandleSend(&pingCb, polled_cnt, &timestamp2);
    mocker_clean();

    mocker_invoke(RsIbvGetCqEvent, RsIbvGetCqEvent_stub, 1);
    mocker(RsIbvPollCq, 1, 1);
    mocker(RsPongPostSend, 1, 0);
    mocker(RsPingCommonPostRecv, 1, -1);
    ret = RsPingRocePollRcq(&pingCb, &polled_cnt, &timestamp2);
    EXPECT_INT_EQ(ret, 0);
    RsPongRoceHandleSend(&pingCb, polled_cnt, &timestamp2);
    mocker_clean();

    mocker_invoke(RsIbvGetCqEvent, RsIbvGetCqEvent_stub, 1);
    mocker(RsIbvPollCq, 1, 1);
    mocker(RsPongPostSend, 1, 0);
    mocker(RsPingCommonPostRecv, 1, 0);
    mocker(RsIbvReqNotifyCq, 1, -1);
    ret = RsPingRocePollRcq(&pingCb, &polled_cnt, &timestamp2);
    EXPECT_INT_EQ(ret, 0);
    RsPongRoceHandleSend(&pingCb, polled_cnt, &timestamp2);
    mocker_clean();
}

void tc_rs_epoll_event_ping_handle()
{
    struct ibv_comp_channel ping_channel = { 0 };
    struct ibv_comp_channel pong_channel = { 0 };
    struct rs_cb rscb = { 0 };
    int ret;

    pong_channel.fd = 1;
    pthread_mutex_init(&rscb.pingCb.devMutex, NULL);
    rscb.pingCb.initCnt = 1;
    rscb.pingCb.pingQp.channel = &ping_channel;
    rscb.pingCb.pongQp.channel = &pong_channel;
    rscb.pingCb.threadStatus = RS_PING_THREAD_RUNNING;
    rscb.pingCb.pingPongOps = RsPingRoceGetOps();
    rscb.pingCb.pingPongDfx = RsPingRoceGetDfx();

    mocker(RsPingRocePollRcq, 10, 0);
    mocker(RsPongRoceHandleSend, 10, 0);
    mocker(RsPongRocePollRcq, 10, 0);

    ret = RsEpollEventPingHandle(&rscb, 0);
    EXPECT_INT_EQ(ret, 0);
    ret = RsEpollEventPingHandle(&rscb, 1);
    EXPECT_INT_EQ(ret, 0);
    rscb.pingCb.initCnt = 0;
    ret = RsEpollEventPingHandle(&rscb, 1);
    EXPECT_INT_EQ(ret, -ENODEV);
    mocker_clean();
    return;
}

void tc_rs_ping_get_trip_time()
{
    struct RsPingTimestamp timestamp = { 0 };
    unsigned int ret;

    ret = RsPingGetTripTime(&timestamp);
    EXPECT_INT_EQ(ret, 0);
}

int pthread_mutex_init_stub_2(pthread_mutex_t lock,void * ptr)
{
    static int cnt = 0;
    cnt++;
    if (cnt == 2) {
        return -5;
    }
    return 0;
}

int pthread_mutex_init_stub_3(pthread_mutex_t lock,void * ptr)
{
    static int cnt = 0;
    cnt++;
    if (cnt == 3) {
        return -5;
    }
    return 0;
}

void tc_rs_ping_cb_init_mutex_ab1()
{
    struct RsPingCtxCb pingCb = { 0 };
    int ret;
    mocker(pthread_mutex_init, 10, -5);
    ret = RsPingCbInitMutex(&pingCb);
    EXPECT_INT_EQ(ret, -258);
    mocker_clean();
}

void tc_rs_ping_cb_init_mutex_ab2()
{
    struct RsPingCtxCb pingCb = { 0 };
    int ret;
    mocker_invoke(pthread_mutex_init, pthread_mutex_init_stub_2, 10);
    ret = RsPingCbInitMutex(&pingCb);
    EXPECT_INT_EQ(ret, -258);
    mocker_clean();
}

void tc_rs_ping_cb_init_mutex_ab3()
{
    struct RsPingCtxCb pingCb = { 0 };
    int ret;
    mocker_invoke(pthread_mutex_init, pthread_mutex_init_stub_3, 10);
    ret = RsPingCbInitMutex(&pingCb);
    EXPECT_INT_EQ(ret, -258);
    mocker_clean();
}

void tc_rs_ping_cb_init_mutex()
{
    tc_rs_ping_cb_init_mutex_ab1();
    tc_rs_ping_cb_init_mutex_ab2();
    tc_rs_ping_cb_init_mutex_ab3();
}

void tc_rs_ping_resolve_response_packet()
{
    struct RsPingCtxCb pingCb = { 0 };
    struct timeval timestamp4 = { 0 };
    void *recv_addr = calloc(1, 1024);
    uint32_t sge_idx = 0;
    int ret;

    pingCb.pongQp.recvMrCb.sgeList = calloc(1, sizeof(struct ibv_sge));
    pingCb.pongQp.recvMrCb.sgeList->addr = (uintptr_t)recv_addr;
    pingCb.pongQp.recvMrCb.sgeList->length = 1024;
    pingCb.taskId = 1;

    ret = RsPongResolveResponsePacket(&pingCb, sge_idx, &timestamp4);
    EXPECT_INT_EQ(ret, 0);

    pingCb.taskId = 0;
    mocker_invoke(RsPingRoceFindTargetNode, rs_ping_roce_find_target_node_stub, 1);
    ret = RsPongResolveResponsePacket(&pingCb, sge_idx, &timestamp4);
    EXPECT_INT_EQ(ret, -ENODEV);
    mocker_clean();

    g_tmp_target1.resultSummary.rttMax = 10;
    g_tmp_target1.resultSummary.rttMin = 4;
    pthread_mutex_init(&g_tmp_target1.tripMutex, NULL);
    mocker(RsPingGetTripTime, 1, 11);
    mocker_invoke(RsPingRoceFindTargetNode, rs_ping_roce_find_target_node_stub1, 1);
    ret = RsPongResolveResponsePacket(&pingCb, sge_idx, &timestamp4);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    g_tmp_target1.resultSummary.rttMax = 10;
    g_tmp_target1.resultSummary.rttMin = 4;
    g_tmp_target1.resultSummary.taskAttr.timeoutInterval = 12;
    pthread_mutex_init(&g_tmp_target1.tripMutex, NULL);
    mocker(RsPingGetTripTime, 1, 11);
    mocker_invoke(RsPingRoceFindTargetNode, rs_ping_roce_find_target_node_stub1, 1);
    ret = RsPongResolveResponsePacket(&pingCb, sge_idx, &timestamp4);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    pthread_mutex_destroy(&g_tmp_target1.tripMutex);
    free(pingCb.pongQp.recvMrCb.sgeList);
    pingCb.pongQp.recvMrCb.sgeList = NULL;
    free(recv_addr);
    recv_addr = NULL;
}

void tc_rs_ping_server_poll_cq()
{
    struct RsPingCtxCb pingCb = { 0 };

    mocker(RsIbvGetCqEvent, 1, -1);
    RsPongRocePollRcq(&pingCb);
    mocker_clean();

    mocker(RsIbvGetCqEvent, 1, 0);
    RsPongRocePollRcq(&pingCb);
    mocker_clean();

    pingCb.pongQp.recvCq.ibCq = &g_tmp_cq;
    pingCb.pongQp.recvCq.maxRecvWcNum = 16;
    mocker_invoke(RsIbvGetCqEvent, RsIbvGetCqEvent_stub, 1);
    mocker(RsIbvPollCq, 1, -1);
    RsPongRocePollRcq(&pingCb);
    mocker_clean();

    mocker_invoke(RsIbvGetCqEvent, RsIbvGetCqEvent_stub, 1);
    mocker(RsIbvPollCq, 1, 1);
    mocker(RsPongResolveResponsePacket, 1, -1);
    RsPongRocePollRcq(&pingCb);
    mocker_clean();

    mocker_invoke(RsIbvGetCqEvent, RsIbvGetCqEvent_stub, 1);
    mocker(RsIbvPollCq, 1, 1);
    mocker(RsPongResolveResponsePacket, 1, 0);
    mocker(RsPingCommonPostRecv, 1, -1);
    RsPongRocePollRcq(&pingCb);
    mocker_clean();

    mocker_invoke(RsIbvGetCqEvent, RsIbvGetCqEvent_stub, 1);
    mocker(RsIbvPollCq, 1, 1);
    mocker(RsPongResolveResponsePacket, 1, 0);
    mocker(RsPingCommonPostRecv, 1, 0);
    mocker(RsIbvReqNotifyCq, 1, -1);
    RsPongRocePollRcq(&pingCb);
    mocker_clean();
}

void tc_rs_ping_cb_get_dev_rdev_index()
{
#ifndef CUSTOM_INTERFACE
#define CUSTOM_INTERFACE
#endif

    struct ibv_device *devList = calloc(1, sizeof(struct ibv_device));
    struct RsPingCtxCb pingCb = { 0 };
    int index = 0;
    int ret;

    pthread_mutex_init(&pingCb.pingMutex, NULL);
    pingCb.rdevCb.devList = &devList;
    mocker(RsIbvGetDeviceName, 1, "dev");
    mocker(RsRoceGetRoceDevData, 1, -1);
    ret = RsPingCbGetDevRdevIndex(&pingCb, index);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

    mocker(RsIbvGetDeviceName, 1, "dev");
    mocker(RsRoceGetRoceDevData, 1, 0);
    ret = RsPingCbGetDevRdevIndex(&pingCb, index);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    free(devList);
    devList = NULL;
    pthread_mutex_destroy(&pingCb.pingMutex);
}

void tc_rs_ping_init_mr_cb()
{
    struct RsPingCtxCb pingCb = { 0 };
    struct RsPingMrCb mr_cb = { 0 };
    struct rs_cb rscb = { 0 };
    struct ibv_mr mr = { 0 };
    int ret;

    mocker((stub_fn_t)pthread_mutex_init, 1, -1);
    ret = RsPingCommonInitMrCb(&rscb, &pingCb, &mr_cb);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

    ret = RsPingCommonInitMrCb(&rscb, &pingCb, &mr_cb);
    EXPECT_INT_NE(ret, 0);

    mocker(DlHalBuffAllocAlignEx, 1, 0);
    mocker(RsDrvMrReg, 1, NULL);
    ret = RsPingCommonInitMrCb(&rscb, &pingCb, &mr_cb);
    EXPECT_INT_NE(ret, 0);
    mocker_clean();

    mocker(DlHalBuffAllocAlignEx, 1, 0);
    mocker_invoke(RsDrvMrReg, rs_drv_mr_reg_stub, 1);
    mocker_invoke(RsDrvMrDereg, rs_drv_mr_dereg_stub, 1);
    mocker(calloc, 10, NULL);
    ret = RsPingCommonInitMrCb(&rscb, &pingCb, &mr_cb);
    EXPECT_INT_NE(ret, 0);
    mocker_clean();

    mocker(DlHalBuffAllocAlignEx, 1, 0);
    mocker_invoke(RsDrvMrReg, rs_drv_mr_reg_stub, 1);
    mocker_invoke(RsDrvMrDereg, rs_drv_mr_dereg_stub, 1);
    mr_cb.sgeNum = 1;
    ret = RsPingCommonInitMrCb(&rscb, &pingCb, &mr_cb);
    EXPECT_INT_EQ(ret, 0);

    mocker(DlHalBuffFree, 1, 0);
    RsPingCommonDeinitMrCb(&mr_cb);
    mocker_clean();
}

void tc_rs_ping_common_deinit_local_buffer()
{
    struct RsPingCtxCb pingCb = { 0 };
    pingCb.pongQp.recvMrCb.sgeList = calloc(1, sizeof(struct ibv_sge));
    pingCb.pongQp.sendMrCb.sgeList = calloc(1, sizeof(struct ibv_sge));
    pingCb.pingQp.recvMrCb.sgeList = calloc(1, sizeof(struct ibv_sge));
    pingCb.pingQp.sendMrCb.sgeList = calloc(1, sizeof(struct ibv_sge));

    mocker(RsDrvMrDereg, 20, 0);
    mocker(DlHalBuffFree, 20, 0);
    mocker(pthread_mutex_destroy, 20, 0);

    RsPingCommonDeinitLocalBuffer(&pingCb);

    mocker_clean();
}

void tc_rs_ping_common_deinit_local_qp()
{
    struct RsPingLocalQpCb qp_cb = { 0 };
    struct ibv_comp_channel channel = { 0 };
    struct RsPingCtxCb pingCb = { 0 };
    struct rs_cb rscb = { 0 };

    qp_cb.channel = &channel;
    mocker(RsIbvDestroyQp, 20, 0);
    mocker(pthread_mutex_destroy, 20, 0);
    mocker(RsIbvAckCqEvents, 20, 0);
    mocker(RsIbvDestroyCq, 20, 0);
    mocker(RsEpollCtl, 20, 0);
    mocker(RsIbvDestroyCompChannel, 20, 0);

    RsPingCommonDeinitLocalQp(NULL, NULL, NULL);
    RsPingCommonDeinitLocalQp(&rscb, &pingCb, &qp_cb);
    mocker_clean();
}

void tc_rs_ping_pong_init_local_info()
{
    struct RsPingCtxCb pingCb = { 0 };
    struct PingInitAttr attr = { 0 };
    struct PingInitInfo info = { 0 };
    struct ibv_qp ibQp = { 0 };
    struct rs_cb rscb = { 0 };
    int ret;

    pingCb.pingQp.ibQp = &ibQp;
    pingCb.pongQp.ibQp = &ibQp;

    mocker(RsPingCommonInitLocalQp, 20, 0);
    mocker(RsPingPongInitLocalBuffer, 20, 0);
    mocker(RsPingCommonInitPostRecvAll, 20, 0);
    mocker(RsPingCommonDeinitLocalBuffer, 20, 0);
    mocker(RsPingCommonDeinitLocalQp, 20, 0);
    ret = RsPingPongInitLocalInfo(&rscb, &attr, &info, &pingCb);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    mocker(RsPingCommonInitLocalQp, 20, -1);
    mocker(RsPingPongInitLocalBuffer, 20, 0);
    mocker(RsPingCommonInitPostRecvAll, 20, 0);
    mocker(RsPingCommonDeinitLocalBuffer, 20, 0);
    mocker(RsPingCommonDeinitLocalQp, 20, 0);
    ret = RsPingPongInitLocalInfo(&rscb, &attr, &info, &pingCb);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

    mocker(RsPingCommonInitLocalQp, 20, 0);
    mocker(RsPingPongInitLocalBuffer, 20, -1);
    mocker(RsPingCommonInitPostRecvAll, 20, 0);
    mocker(RsPingCommonDeinitLocalBuffer, 20, 0);
    mocker(RsPingCommonDeinitLocalQp, 20, 0);
    ret = RsPingPongInitLocalInfo(&rscb, &attr, &info, &pingCb);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

    mocker(RsPingCommonInitLocalQp, 20, 0);
    mocker(RsPingPongInitLocalBuffer, 20, 0);
    mocker(RsPingCommonInitPostRecvAll, 20, -1);
    mocker(RsPingCommonDeinitLocalBuffer, 20, 0);
    mocker(RsPingCommonDeinitLocalQp, 20, 0);
    ret = RsPingPongInitLocalInfo(&rscb, &attr, &info, &pingCb);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();
}

int RsIbvPollCq_stub_ping0(struct ibv_cq *cq, int num_entries, struct ibv_wc *wc)
{
    return 0;
}

int RsIbvPollCq_stub_ping1(struct ibv_cq *cq, int num_entries, struct ibv_wc *wc)
{
    wc->status = IBV_WC_LOC_LEN_ERR;
    return 1;
}

int RsIbvPollCq_stub_ping2(struct ibv_cq *cq, int num_entries, struct ibv_wc *wc)
{
    wc->status = IBV_WC_SUCCESS;
    return 1;
}

void tc_rs_ping_roce_poll_scq()
{
    struct RsPingTargetInfo  target = { 0 };
    struct RsPingCtxCb pingCb = { 0 };
    int ret;

    mocker_invoke(RsIbvPollCq, RsIbvPollCq_stub_ping0, 10);
    ret = RsPingRocePollScq(&pingCb, &target);
    EXPECT_INT_EQ(ret, -61);
    mocker_clean();

    mocker_invoke(RsIbvPollCq, RsIbvPollCq_stub_ping1, 10);
    ret = RsPingRocePollScq(&pingCb, &target);
    EXPECT_INT_EQ(ret, -259);
    mocker_clean();

    mocker_invoke(RsIbvPollCq, RsIbvPollCq_stub_ping2, 10);
    ret = RsPingRocePollScq(&pingCb, &target);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

int stub_rs_ping_client_post_send(struct RsPingCtxCb *pingCb, struct RsPingTargetInfo  *target)
{
    pingCb->threadStatus = 0;
    return 1;
}

void tc_rs_ping_handle()
{
    struct rs_cb rs_cb = {0};
    struct RsPingTargetInfo  target_tmp = {0};

    mocker_clean();
    rs_cb.pingCb.threadStatus = 1;
    rs_cb.pingCb.taskStatus = 1;
    rs_cb.pingCb.taskAttr.packetCnt = 1;
    RS_INIT_LIST_HEAD(&rs_cb.pingCb.pingList);
    rs_cb.pingCb.pingList.next = &target_tmp.list;
    rs_cb.pingCb.pingList.prev = &target_tmp.list;
    rs_cb.pingCb.pingPongOps = RsPingRoceGetOps();
    rs_cb.pingCb.pingPongDfx = RsPingRoceGetDfx();
    target_tmp.list.next = &rs_cb.pingCb.pingList;
    target_tmp.list.prev = &rs_cb.pingCb.pingList;
    target_tmp.state = 1;
    mocker(RsGetCurTime, 1, 0);
    mocker(strncpy_s, 1, 0);
    mocker(RsHeartbeatAlivePrint, 1, 0);
    mocker(RsListEmpty, 1, 0);
    mocker_invoke(RsPingRocePostSend, stub_rs_ping_client_post_send, 1);
    mocker(pthread_mutex_lock, 1, 0);
    mocker(pthread_mutex_unlock, 1, 0);
    RsPingHandle((void *)&rs_cb);
    mocker_clean();
}

void tc_rs_ping_roce_ping_cb_deinit()
{
    struct RsPingTargetInfo  *stub_ping_node;
    struct RsPongTargetInfo *stub_pong_node;
    struct RsPingCtxCb pingCb = { 0 };

    mocker(RsGetRsCb, 20, 0);
    mocker(pthread_mutex_lock, 20, 0);
    mocker(pthread_mutex_unlock, 20, 0);

    stub_ping_node = calloc(1, sizeof(struct RsPingTargetInfo ));
    stub_pong_node = calloc(1, sizeof(struct RsPongTargetInfo));
    RS_INIT_LIST_HEAD(&pingCb.pingList);
    RsListAddTail(&stub_ping_node->list, &pingCb.pingList);
    RS_INIT_LIST_HEAD(&pingCb.pongList);
    RsListAddTail(&stub_pong_node->list, &pingCb.pongList);
    mocker(RsIbvDestroyAh, 20, 0);

    mocker(RsPingCommonDeinitLocalQp, 20, 0);
    mocker(RsPingCommonDeinitLocalBuffer, 20, 0);
    mocker(RsIbvDeallocPd, 20, 0);
    mocker(RsIbvCloseDevice, 20, 0);
    mocker(RsIbvFreeDeviceList, 20, 0);

    RsPingRocePingCbDeinit(0, &pingCb);
}

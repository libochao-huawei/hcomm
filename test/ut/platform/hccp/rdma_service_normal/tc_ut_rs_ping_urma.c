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
#include "dl_urma_function.h"
#include "rs_socket.h"
#include "ut_dispatch.h"
#include "ra_rs_comm.h"
#include "rs.h"
#include "hccp_common.h"
#include "rs_inner.h"
#include "rs_ping_inner.h"
#include "hccp_ping.h"
#include "rs_epoll.h"
#include "rs_ping.h"
#include "rs_ping_urma.h"
#include "tc_ut_rs_ping_urma.h"

extern int RsGetPingCb(struct RaRsDevInfo *rdev, struct RsPingCtxCb **pingCb);
extern int rs_ping_urma_poll_rcq(struct RsPingCtxCb *pingCb, int *polled_cnt, struct timeval *timestamp2);
extern void rs_pong_urma_handle_send(struct RsPingCtxCb *pingCb, int polled_cnt, struct timeval *timestamp2);
extern void rs_pong_urma_poll_rcq(struct RsPingCtxCb *pingCb);
extern int rs_ping_urma_poll_scq(struct RsPingCtxCb *pingCb, struct RsPingTargetInfo  *target);
extern struct RsPingPongOps *rs_ping_urma_get_ops(void);
extern struct RsPingPongDfx  *rs_ping_urma_get_dfx(void);
extern int rs_ping_common_import_jetty(urma_context_t *urma_ctx, struct PingQpInfo *target,
    urma_target_jetty_t **import_tjetty);
extern int rs_pong_jetty_post_send(struct RsPingCtxCb *pingCb, urma_cr_t *cr, struct timeval *timestamp2);
extern int rs_ping_common_jfr_post_recv(struct rs_ping_local_jetty_cb *jetty_cb);
extern int rs_pong_jetty_resolve_response_packet(struct RsPingCtxCb *pingCb, uint32_t sge_idx, struct timeval *timestamp4);
extern int rs_ping_urma_find_target_node(struct RsPingCtxCb *pingCb, struct PingQpInfo *target,
    struct RsPingTargetInfo  **node);
extern int rs_ping_common_poll_send_jfc(struct rs_ping_local_jetty_cb *jetty_cb);
extern int rs_pong_jetty_find_alloc_target_node(struct RsPingCtxCb *pingCb, struct PingQpInfo *target,
    struct RsPongTargetInfo **node);
extern int rs_pong_jetty_find_target_node(struct RsPingCtxCb *pingCb, struct PingQpInfo *target,
    struct RsPongTargetInfo **node);
extern int rs_get_jetty_info(struct PingQpInfo *qp_info, urma_jetty_id_t *jetty_id, urma_eid_t *eid);

urma_jfc_t g_tmp_jfc;
static struct rs_cb g_tmp_rs_cb;
static struct RsPingCtxCb g_tmp_ping_cb;
static struct RsPingTargetInfo  g_tmp_target;
static struct RsPingTargetInfo  g_tmp_target1;

#define TEST_SGE_LIST_LEN 1024

int rs_get_rs_cb_urma_stub(unsigned int phyId, struct rs_cb **rs_cb)
{
    *rs_cb = &g_tmp_rs_cb;
    (*rs_cb)->pingCb.pingPongOps = rs_ping_urma_get_ops();
    (*rs_cb)->pingCb.pingPongDfx = rs_ping_urma_get_dfx();
    return 0;
}

int rs_get_ping_cb_urma_stub(struct RaRsDevInfo *rdev, struct RsPingCtxCb **pingCb)
{
    *pingCb = &g_tmp_ping_cb;
    (*pingCb)->pingPongOps = rs_ping_urma_get_ops();
    (*pingCb)->pingPongDfx = rs_ping_urma_get_dfx();
    return 0;
}

int rs_get_ping_cb_urma_stub1(struct RaRsDevInfo *rdev, struct RsPingCtxCb **pingCb)
{
    *pingCb = &g_tmp_rs_cb.pingCb;
    (*pingCb)->pingPongOps = rs_ping_urma_get_ops();
    (*pingCb)->pingPongDfx = rs_ping_urma_get_dfx();
    return 0;
}

int rs_urma_poll_jfc_stub_ping0(urma_jfc_t *jfc, int cr_cnt, urma_cr_t *cr)
{
    cr->status = URMA_CR_LOC_LEN_ERR;
    return 1;
}

int rs_urma_poll_jfc_stub_ping1(urma_jfc_t *jfc, int cr_cnt, urma_cr_t *cr)
{
    cr->status = URMA_CR_SUCCESS;
    return 1;
}

int rs_urma_wait_jfc_stub(urma_jfce_t *jfce, uint32_t jfc_cnt, int time_out, urma_jfc_t *jfc[])
{
    *jfc = &g_tmp_jfc;
    return 1;
}

int rs_ping_urma_find_target_node_stub(struct RsPingCtxCb *pingCb, struct PingQpInfo *target,
    struct RsPingTargetInfo  **node)
{
    *node = &g_tmp_target;

    return -ENODEV;
}

int rs_ping_urma_find_target_node_stub1(struct RsPingCtxCb *pingCb, struct PingQpInfo *target,
    struct RsPingTargetInfo  **node)
{
    *node = &g_tmp_target1;

    return 0;
}

int rs_pong_jetty_find_alloc_target_node_stub(struct RsPingCtxCb *pingCb, struct PingQpInfo *target,
    struct RsPongTargetInfo **node)
{
    static struct RsPongTargetInfo tmp_node = {0};

    *node = &tmp_node;

    return 0;
}

int rs_pong_jetty_find_target_node_stub(struct RsPingCtxCb *pingCb, struct PingQpInfo *target,
    struct RsPongTargetInfo **node)
{
    *node = NULL;
    return -19;
}

int rs_pong_jetty_find_target_node_stub2(struct RsPingCtxCb *pingCb, struct PingQpInfo *target,
    struct RsPongTargetInfo **node)
{
    *node = calloc(1, sizeof(struct RsPongTargetInfo));
    RS_INIT_LIST_HEAD(&(*node)->list);
    return 0;
}

void tc_rs_ping_init_deinit_urma()
{
    struct ibv_comp_channel client_channel = {0};
    struct ibv_comp_channel server_channel = {0};
    struct PingTargetInfo  target = {0};
    struct PingInitAttr attr = {0};
    struct PingInitInfo info = {0};
    struct RaRsDevInfo rdev = {0};
    unsigned int rdevIndex = 0;
    int ret;

    mocker_invoke(RsGetRsCb, rs_get_rs_cb_urma_stub, 20);
    mocker(rsGetLocalDevIDByHostDevID, 20, 0);
    mocker(RsSetupSharemem, 20, 0);
    mocker(RsEpollCtl, 20, 0);
    mocker(DlHalBuffAllocAlignEx, 20, 0);
    attr.protocol = PROTOCOL_UDMA;
    ret = RsPingInit(&attr, &info, &rdevIndex);
    EXPECT_INT_EQ(ret, 0);
    EXPECT_INT_EQ(info.result.headerSize, RS_PING_PAYLOAD_HEADER_RESV_CUSTOM);
    mocker_clean();

    g_tmp_rs_cb.pingCb.initCnt = 1;
    RS_INIT_LIST_HEAD(&g_tmp_rs_cb.pingCb.pingList);
    RS_INIT_LIST_HEAD(&g_tmp_rs_cb.pingCb.pongList);
    target.payload.size = 1;
    mocker_invoke(RsGetPingCb, rs_get_ping_cb_urma_stub1, 10);
    mocker_invoke(RsGetRsCb, rs_get_rs_cb_urma_stub, 20);
    ret = RsPingTargetAdd(&rdev, &target);
    EXPECT_INT_EQ(ret, 0);
    mocker(RsEpollCtl, 20, 0);
    ret = RsPingDeinit(&rdev);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

void tc_rs_ping_target_add_del_urma()
{
    struct PingTargetInfo  target = {0};
    struct RaRsDevInfo rdev = {0};
    unsigned int num = 1;
    int ret;

    g_tmp_ping_cb.taskStatus = RS_PING_TASK_RESET;
    RS_INIT_LIST_HEAD(&g_tmp_ping_cb.pingList);
    mocker_invoke(RsGetPingCb, rs_get_ping_cb_urma_stub, 2);
    ret = RsPingTargetAdd(&rdev, &target);
    EXPECT_INT_EQ(ret, 0);
    ret = RsPingTargetDel(&rdev, &target, &num);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    target.payload.size = 1;
    g_tmp_ping_cb.taskStatus = RS_PING_TASK_RESET;
    mocker_invoke(RsGetPingCb, rs_get_ping_cb_urma_stub, 1);
    mocker(rs_ping_common_import_jetty, 1, -1);
    ret = RsPingTargetAdd(&rdev, &target);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

    mocker_invoke(RsGetPingCb, rs_get_ping_cb_urma_stub, 1);
    mocker(calloc, 10, 0);
    ret = RsPingTargetAdd(&rdev, &target);
    EXPECT_INT_EQ(ret, -12);
    mocker_clean();

    g_tmp_ping_cb.taskStatus = RS_PING_TASK_RESET;
    mocker_invoke(RsGetPingCb, rs_get_ping_cb_urma_stub, 1);
    mocker(rs_ping_urma_find_target_node, 1, -1);
    ret = RsPingTargetDel(&rdev, &target, &num);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();
}

void tc_rs_ping_urma_post_send()
{
    struct RsPingTargetInfo  target = {0};
    struct RsPingCtxCb pingCb = {0};
    urma_jetty_t server_jetty = {0};
    urma_jetty_t client_jetty = {0};
    void *addr = malloc(256);
    urma_sge_t sge = {0};
    int ret;

    target.payloadBuffer = malloc(1);
    target.payloadSize = 1;
    sge.addr = (uintptr_t)addr;
    sge.len = 256;
    pingCb.ping_jetty.send_seg_cb.sge_num = 1;
    pingCb.ping_jetty.send_seg_cb.sge_list = &sge;
    pingCb.pong_jetty.jetty = &server_jetty;
    pingCb.ping_jetty.jetty = &client_jetty;
    ret = rs_ping_urma_post_send(&pingCb, &target);
    EXPECT_INT_EQ(ret, 0);
    free(addr);
    addr = NULL;
    free(target.payloadBuffer);
    target.payloadBuffer = NULL;
}

void tc_rs_ping_client_poll_cq_urma()
{
    struct RsPingCtxCb pingCb = {0};
    struct timeval timestamp2;
    int polled_cnt;
    int ret;

    mocker_clean();
    ret = rs_ping_urma_poll_rcq(&pingCb, &polled_cnt, &timestamp2);
    EXPECT_INT_EQ(ret, -11);
    mocker(rs_urma_wait_jfc, 1, -1);
    ret = rs_ping_urma_poll_rcq(&pingCb, &polled_cnt, &timestamp2);
    EXPECT_INT_EQ(ret, -259);
    mocker_clean();

    mocker(rs_urma_wait_jfc, 1, 1);
    mocker(rs_urma_ack_jfc, 1, 0);
    ret = rs_ping_urma_poll_rcq(&pingCb, &polled_cnt, &timestamp2);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    pingCb.ping_jetty.recv_jfc.jfc = &g_tmp_jfc;
    mocker_invoke(rs_urma_wait_jfc, rs_urma_wait_jfc_stub, 1);
    mocker(rs_urma_poll_jfc, 1, -1);
    ret = rs_ping_urma_poll_rcq(&pingCb, &polled_cnt, &timestamp2);
    EXPECT_INT_EQ(ret, -259);
    mocker_clean();

    pingCb.ping_jetty.recv_jfc.max_recv_wc_num = 16;
    mocker_invoke(rs_urma_wait_jfc, rs_urma_wait_jfc_stub, 1);
    mocker(rs_urma_poll_jfc, 1, 1);
    mocker(rs_pong_jetty_post_send, 1, -1);
    ret = rs_ping_urma_poll_rcq(&pingCb, &polled_cnt, &timestamp2);
    EXPECT_INT_EQ(ret, 0);
    rs_pong_urma_handle_send(&pingCb, polled_cnt, &timestamp2);
    mocker_clean();

    mocker_invoke(rs_urma_wait_jfc, rs_urma_wait_jfc_stub, 1);
    mocker(rs_urma_poll_jfc, 1, 1);
    mocker(rs_pong_jetty_post_send, 1, 0);
    mocker(rs_ping_common_jfr_post_recv, 1, -1);
    ret = rs_ping_urma_poll_rcq(&pingCb, &polled_cnt, &timestamp2);
    EXPECT_INT_EQ(ret, 0);
    rs_pong_urma_handle_send(&pingCb, polled_cnt, &timestamp2);
    mocker_clean();

    mocker_invoke(rs_urma_wait_jfc, rs_urma_wait_jfc_stub, 1);
    mocker(rs_urma_poll_jfc, 1, 1);
    mocker(rs_pong_jetty_post_send, 1, 0);
    mocker(rs_ping_common_jfr_post_recv, 1, -1);
    mocker(rs_urma_rearm_jfc, 1, -1);
    ret = rs_ping_urma_poll_rcq(&pingCb, &polled_cnt, &timestamp2);
    EXPECT_INT_EQ(ret, 0);
    rs_pong_urma_handle_send(&pingCb, polled_cnt, &timestamp2);
    mocker_clean();
}

void tc_rs_ping_urma_poll_scq()
{
    struct RsPingTargetInfo  target = {0};
    struct RsPingCtxCb pingCb = {0};
    int ret;

    ret = rs_ping_urma_poll_scq(&pingCb, &target);
    EXPECT_INT_EQ(ret, -61);

    mocker_invoke(rs_urma_poll_jfc, rs_urma_poll_jfc_stub_ping0, 10);
    ret = rs_ping_urma_poll_scq(&pingCb, &target);
    EXPECT_INT_EQ(ret, -259);
    mocker_clean();

    mocker_invoke(rs_urma_poll_jfc, rs_urma_poll_jfc_stub_ping1, 10);
    ret = rs_ping_urma_poll_scq(&pingCb, &target);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

void tc_rs_ping_server_poll_cq_urma()
{
    struct RsPingCtxCb pingCb = {0};

    mocker(rs_urma_wait_jfc, 1, -1);
    rs_pong_urma_poll_rcq(&pingCb);
    mocker_clean();

    mocker(rs_urma_wait_jfc, 1, 0);
    rs_pong_urma_poll_rcq(&pingCb);
    mocker_clean();

    pingCb.pong_jetty.recv_jfc.jfc = &g_tmp_jfc;
    pingCb.pong_jetty.recv_jfc.max_recv_wc_num = 16;
    mocker_invoke(rs_urma_wait_jfc, rs_urma_wait_jfc_stub, 1);
    mocker(rs_urma_poll_jfc, 1, -1);
    rs_pong_urma_poll_rcq(&pingCb);
    mocker_clean();

    mocker_invoke(rs_urma_wait_jfc, rs_urma_wait_jfc_stub, 1);
    mocker(rs_urma_poll_jfc, 1, 1);
    mocker(rs_pong_jetty_resolve_response_packet, 1, -1);
    rs_pong_urma_poll_rcq(&pingCb);
    mocker_clean();

    mocker_invoke(rs_urma_wait_jfc, rs_urma_wait_jfc_stub, 1);
    mocker(rs_urma_poll_jfc, 1, 1);
    mocker(rs_pong_jetty_resolve_response_packet, 1, 0);
    mocker(rs_ping_common_jfr_post_recv, 1, -1);
    rs_pong_urma_poll_rcq(&pingCb);
    mocker_clean();

    mocker_invoke(rs_urma_wait_jfc, rs_urma_wait_jfc_stub, 1);
    mocker(rs_urma_poll_jfc, 1, 1);
    mocker(rs_pong_jetty_resolve_response_packet, 1, 0);
    mocker(rs_ping_common_jfr_post_recv, 1, 0);
    mocker(rs_urma_rearm_jfc, 1, -1);
    rs_pong_urma_poll_rcq(&pingCb);
    mocker_clean();
}

void tc_rs_epoll_event_ping_handle_urma()
{
    urma_jfce_t ping_jfce = {0};
    urma_jfce_t pong_jfce = {0};
    struct rs_cb rscb = {0};
    int ret;

    pong_jfce.fd = 1;
    rscb.pingCb.initCnt = 1;
    rscb.pingCb.ping_jetty.jfce = &ping_jfce;
    rscb.pingCb.pong_jetty.jfce = &pong_jfce;
    rscb.pingCb.threadStatus = RS_PING_THREAD_RUNNING;
    rscb.pingCb.pingPongOps = rs_ping_urma_get_ops();
    rscb.pingCb.pingPongDfx = rs_ping_urma_get_dfx();

    mocker(rs_ping_urma_poll_rcq, 10, 0);
    mocker(rs_pong_urma_handle_send, 10, 0);
    mocker(rs_pong_urma_poll_rcq, 10, 0);

    ret = RsEpollEventPingHandle(&rscb, 0);
    EXPECT_INT_EQ(ret, 0);
    ret = RsEpollEventPingHandle(&rscb, 1);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
    return;
}

void tc_rs_ping_get_results_urma()
{
    struct PingTargetCommInfo target = {0};
    struct PingResultInfo result = {0};
    struct RaRsDevInfo rdev = {0};
    unsigned int num = 1;
    int ret;

    g_tmp_ping_cb.taskStatus = RS_PING_TASK_RESET;
    mocker_invoke(RsGetPingCb, rs_get_ping_cb_urma_stub, 1);
    mocker_invoke(rs_ping_urma_find_target_node, rs_ping_urma_find_target_node_stub, 1);
    ret = RsPingGetResults(&rdev, &target, &num, &result);
    EXPECT_INT_EQ(ret, -ENODEV);
    mocker_clean();

    num = 1;
    g_tmp_ping_cb.taskStatus = RS_PING_TASK_RESET;
    mocker_invoke(RsGetPingCb, rs_get_ping_cb_urma_stub, 1);
    mocker_invoke(rs_ping_urma_find_target_node, rs_ping_urma_find_target_node_stub1, 1);
    ret = RsPingGetResults(&rdev, &target, &num, &result);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

void tc_rs_ping_server_post_send_urma()
{
    struct RsPingCtxCb pingCb = {0};
    struct timeval timestamp2;
    void *send_addr = malloc(TEST_SGE_LIST_LEN);
    void *recv_addr = malloc(TEST_SGE_LIST_LEN);
    urma_cr_t cr = {0};
    int ret;

    cr.user_ctx = 1;
    mocker(rs_ping_common_poll_send_jfc, 1, 0);
    ret = rs_pong_jetty_post_send(&pingCb, &cr, &timestamp2);
    EXPECT_INT_EQ(ret, -EIO);
    mocker_clean();

    cr.user_ctx = 0;
    cr.completion_len = 16;
    pingCb.ping_jetty.recv_seg_cb.sge_list  = calloc(1, sizeof(urma_sge_t));
    pingCb.ping_jetty.recv_seg_cb.sge_num = 1;
    pingCb.pong_jetty.send_seg_cb.sge_list  = calloc(1, sizeof(urma_sge_t));
    pingCb.pong_jetty.send_seg_cb.sge_num = 1;
    pingCb.pong_jetty.send_seg_cb.sge_list ->addr = (uintptr_t)send_addr;
    pingCb.pong_jetty.send_seg_cb.sge_list ->len = TEST_SGE_LIST_LEN;
    pingCb.ping_jetty.recv_seg_cb.sge_list->addr = (uintptr_t)recv_addr;
    pingCb.ping_jetty.recv_seg_cb.sge_list->len = TEST_SGE_LIST_LEN;
    mocker(rs_ping_common_poll_send_jfc, 1, 0);
    mocker(rs_pong_jetty_find_alloc_target_node, 1, -1);
    ret = rs_pong_jetty_post_send(&pingCb, &cr, &timestamp2);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

    mocker(rs_ping_common_poll_send_jfc, 1, 0);
    mocker_invoke(rs_pong_jetty_find_alloc_target_node, rs_pong_jetty_find_alloc_target_node_stub, 1);
    mocker(gettimeofday, 20, 1);
    mocker(rs_urma_post_jetty_send_wr, 1, -1);
    ret = rs_pong_jetty_post_send(&pingCb, &cr, &timestamp2);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

    mocker(rs_ping_common_poll_send_jfc, 1, 0);
    mocker_invoke(rs_pong_jetty_find_alloc_target_node, rs_pong_jetty_find_alloc_target_node_stub, 1);
    ret = rs_pong_jetty_post_send(&pingCb, &cr, &timestamp2);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    free(pingCb.pong_jetty.send_seg_cb.sge_list );
    pingCb.pong_jetty.send_seg_cb.sge_list  = NULL;
    free(pingCb.ping_jetty.recv_seg_cb.sge_list);
    pingCb.ping_jetty.recv_seg_cb.sge_list = NULL;
    free(recv_addr);
    recv_addr = NULL;
    free(send_addr);
    send_addr = NULL;
}

void tc_rs_pong_jetty_find_alloc_target_node()
{
    struct RsPongTargetInfo *node = NULL;
    struct RsPingCtxCb pingCb = {0};
    struct PingQpInfo target = {0};
    int ret;

    mocker_invoke(rs_pong_jetty_find_target_node, rs_pong_jetty_find_target_node_stub2, 1);
    mocker(rs_ping_common_import_jetty, 1, -1);
    ret = rs_pong_jetty_find_alloc_target_node(&pingCb, &target, &node);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

    pthread_mutex_init(&pingCb.pongMutex, NULL);
    RS_INIT_LIST_HEAD(&pingCb.pongList);
    mocker_invoke(rs_pong_jetty_find_target_node, rs_pong_jetty_find_target_node_stub, 1);
    mocker(rs_ping_common_import_jetty, 1, 0);
    ret = rs_pong_jetty_find_alloc_target_node(&pingCb, &target, &node);
    free(node);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

void tc_rs_ping_common_poll_send_jfc()
{
    struct RsPingLocalQpCb qp_cb = {0};
    int ret;

    mocker(rs_urma_poll_jfc, 1, -1);
    ret = rs_ping_common_poll_send_jfc(&qp_cb);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    mocker(rs_urma_poll_jfc, 1, 1);
    ret = rs_ping_common_poll_send_jfc(&qp_cb);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

void tc_rs_pong_jetty_find_target_node()
{
    struct RsPongTargetInfo stub_node = {0};
    struct RsPongTargetInfo *node = NULL;
    struct RsPingCtxCb pingCb = {0};
    struct PingQpInfo target = {0};
    int ret;

    RS_INIT_LIST_HEAD(&pingCb.pongList);
    ret = rs_pong_jetty_find_target_node(&pingCb, &target, &node);
    EXPECT_INT_EQ(ret, -ENODEV);

    RsListAddTail(&stub_node.list, &pingCb.pongList);
    ret = rs_pong_jetty_find_target_node(&pingCb, &target, &node);
    EXPECT_INT_EQ(ret, 0);
}

void tc_rs_pong_jetty_resolve_response_packet()
{
    struct RsPingCtxCb pingCb = {0};
    struct timeval timestamp4 = {0};
    void *recv_addr = calloc(1, TEST_SGE_LIST_LEN);
    uint32_t sge_idx = 0;
    int ret;

    pingCb.pong_jetty.recv_seg_cb.sge_list  = calloc(1, sizeof(urma_sge_t));
    pingCb.pong_jetty.recv_seg_cb.sge_list ->addr = (uintptr_t)recv_addr;
    pingCb.pong_jetty.recv_seg_cb.sge_list ->len = TEST_SGE_LIST_LEN;
    pingCb.taskId = 1;

    ret = rs_pong_jetty_resolve_response_packet(&pingCb, sge_idx, &timestamp4);
    EXPECT_INT_EQ(ret, 0);

    pingCb.taskId = 0;
    mocker_invoke(rs_ping_urma_find_target_node, rs_ping_urma_find_target_node_stub, 1);
    ret = rs_pong_jetty_resolve_response_packet(&pingCb, sge_idx, &timestamp4);
    EXPECT_INT_EQ(ret, -ENODEV);
    mocker_clean();

    g_tmp_target1.resultSummary.rttMax = 10;
    g_tmp_target1.resultSummary.rttMin = 4;
    pthread_mutex_init(&g_tmp_target1.tripMutex, NULL);
    mocker(RsPingGetTripTime, 1, 11);
    mocker_invoke(rs_ping_urma_find_target_node, rs_ping_urma_find_target_node_stub1, 1);
    ret = rs_pong_jetty_resolve_response_packet(&pingCb, sge_idx, &timestamp4);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    g_tmp_target1.resultSummary.rttMax = 10;
    g_tmp_target1.resultSummary.rttMin = 4;
    g_tmp_target1.resultSummary.taskAttr.timeoutInterval = 12;
    pthread_mutex_init(&g_tmp_target1.tripMutex, NULL);
    mocker(RsPingGetTripTime, 1, 11);
    mocker_invoke(rs_ping_urma_find_target_node, rs_ping_urma_find_target_node_stub1, 1);
    ret = rs_pong_jetty_resolve_response_packet(&pingCb, sge_idx, &timestamp4);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    pthread_mutex_destroy(&g_tmp_target1.tripMutex);
    free(pingCb.pong_jetty.recv_seg_cb.sge_list );
    pingCb.pong_jetty.recv_seg_cb.sge_list  = NULL;
    free(recv_addr);
    recv_addr = NULL;
}

void tc_rs_ping_common_import_jetty()
{
    urma_target_jetty_t *import_tjetty = NULL;
    struct PingQpInfo target = {0};
    urma_context_t urma_ctx = {0};
    int ret;

    mocker(rs_get_jetty_info, 1, 0);
    ret = rs_ping_common_import_jetty(&urma_ctx, &target, &import_tjetty);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
    free(import_tjetty);
    import_tjetty = NULL;
}

void tc_rs_ping_urma_reset_recv_buffer()
{
    struct RsPingCtxCb pingCb = {0};

    rs_ping_urma_reset_recv_buffer(&pingCb);
}

void tc_rs_ping_common_jfr_post_recv()
{
    struct rs_ping_local_jetty_cb jetty_cb = {0};
    urma_sge_t sge = {0};
    int ret;

    jetty_cb.recv_seg_cb.sge_num = 1;
    jetty_cb.recv_seg_cb.sge_list  = &sge;
    ret = rs_ping_common_jfr_post_recv(&jetty_cb);
    EXPECT_INT_EQ(ret, 0);
}

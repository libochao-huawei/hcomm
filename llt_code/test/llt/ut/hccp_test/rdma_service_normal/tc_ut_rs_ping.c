/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: rs ping abnormal unit test.
 * Author:
 * Create: 2024-09-23
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

extern int rs_ping_cb_get_ib_ctx_and_index(struct rdev *rdev_info, struct rs_ping_ctx_cb *ping_cb);
extern void rs_epoll_ctl(int epollfd, int op, int fd, int state);
extern int rs_get_ping_cb(struct ra_rs_dev_info *rdev, struct rs_ping_ctx_cb **ping_cb);
extern int rs_ping_common_post_recv(struct rs_ping_local_qp_cb *qp_cb);
extern int rs_ping_common_init_post_recv_all(struct rs_ping_local_qp_cb *qp_cb);
extern void rs_ping_common_deinit_local_buffer(struct rs_ping_ctx_cb *ping_cb);
extern int rs_ping_pong_init_local_buffer(struct rs_cb *rscb, struct ping_init_attr *attr, struct ping_init_info *info,
    struct rs_ping_ctx_cb *ping_cb);
extern int rs_ping_common_init_local_qp(struct rs_cb *rscb, struct rs_ping_ctx_cb *ping_cb, union ping_qp_attr *attr,
    struct rs_ping_local_qp_cb *qp_cb);
extern int rs_ping_roce_find_target_node(struct rs_ping_ctx_cb *ping_cb, struct ping_qp_info *target,
    struct rs_ping_target_info **node);
extern int rs_ping_roce_post_send(struct rs_ping_ctx_cb *ping_cb, struct rs_ping_target_info *target);
extern int rs_ping_roce_get_target_result(struct rs_ping_ctx_cb *ping_cb, struct ping_target_comm_info *target,
    struct ping_result_info *result);
extern int rs_ping_roce_alloc_target_node(struct rs_ping_ctx_cb *ping_cb, struct ping_target_info *target,
    struct rs_ping_target_info **node);
extern bool rs_ping_common_compare_rdma_info(struct ping_qp_info *a, struct ping_qp_info *b);
extern int rs_pong_find_target_node(struct rs_ping_ctx_cb *ping_cb, struct ping_qp_info *target,
    struct rs_pong_target_info **node);
extern int rs_pong_find_alloc_target_node(struct rs_ping_ctx_cb *ping_cb, struct ping_qp_info *target,
    struct rs_pong_target_info **node);
extern int rs_ping_common_create_ah(struct rs_ping_ctx_cb *ping_cb, struct ping_local_comm_info *local_info,
    struct ping_qp_info *remote_info, struct ibv_ah **ah);
extern int rs_ping_common_poll_scq(struct rs_ping_local_qp_cb *qp_cb);
extern int rs_pong_post_send(struct rs_ping_ctx_cb *ping_cb, struct ibv_wc *wc, struct timeval *timestamp2);
extern int rs_ping_roce_poll_rcq(struct rs_ping_ctx_cb *ping_cb, int *polled_cnt, struct timeval *timestamp2);
extern void rs_pong_roce_handle_send(struct rs_ping_ctx_cb *ping_cb, int polled_cnt, struct timeval *timestamp2);
extern int rs_pong_resolve_response_packet(struct rs_ping_ctx_cb *ping_cb, uint32_t sge_idx,
    struct timeval *timestamp4);
extern void rs_pong_roce_poll_rcq(struct rs_ping_ctx_cb *ping_cb);
extern int rs_ping_cb_get_dev_rdev_index(struct rs_ping_ctx_cb *ping_cb, int index);
extern int rs_ping_common_init_mr_cb(struct rs_cb *rscb, struct rs_ping_ctx_cb *ping_cb, struct rs_ping_mr_cb *mr_cb);
extern void rs_ping_common_deinit_mr_cb(struct rs_ping_mr_cb *mr_cb);
extern struct ibv_mr* rs_drv_mr_reg(struct ibv_pd *pd, char *addr, size_t length, int access);
extern int rs_drv_mr_dereg(struct ibv_mr *ib_mr);
extern int rs_ping_common_modify_local_qp(struct rs_ping_ctx_cb *ping_cb, struct rs_ping_local_qp_cb *qp_cb);
extern void rs_ping_common_deinit_local_qp(struct rs_cb *rscb, struct rs_ping_ctx_cb *ping_cb,
    struct rs_ping_local_qp_cb *qp_cb);
extern int rs_ping_pong_init_local_info(struct rs_cb *rscb, struct ping_init_attr *attr, struct ping_init_info *info,
    struct rs_ping_ctx_cb *ping_cb);
extern int rs_ping_roce_poll_scq(struct rs_ping_ctx_cb *ping_cb, struct rs_ping_target_info *target);
extern void rs_ping_pong_del_target_list(struct rs_ping_ctx_cb *ping_cb);
extern void rs_ping_roce_ping_cb_deinit(unsigned int phy_id, struct rs_ping_ctx_cb *ping_cb);
extern int rs_ping_roce_ping_cb_init(unsigned int phy_id, struct ping_init_attr *attr, struct ping_init_info *info,
    unsigned int *dev_index, struct rs_ping_ctx_cb *ping_cb);

static struct rs_cb g_tmp_rs_cb0;
static struct rs_cb g_tmp_rs_cb;
static struct rs_cb g_tmp_rs_cb1;
static struct rs_cb g_tmp_rs_cb_t;
static struct rs_ping_ctx_cb g_tmp_ping_cb;
static struct ibv_cq g_tmp_cq;
static struct rs_ping_target_info g_tmp_target;
static struct rs_ping_target_info g_tmp_target1;
static struct ibv_mr g_ib_mr;

int rs_dev2rscb_stub0(uint32_t dev_id, struct rs_cb **rs_cb, bool init_flag)
{
    *rs_cb = &g_tmp_rs_cb0;

    return 0;
}

int rs_get_rs_cb_stub(unsigned int phy_id, struct rs_cb **rs_cb)
{
    *rs_cb = &g_tmp_rs_cb;
    (*rs_cb)->ping_cb.ping_pong_ops = rs_ping_roce_get_ops();
    (*rs_cb)->ping_cb.ping_pong_dfx = rs_ping_roce_get_dfx();
    return 0;
}

int rs_get_rs_cb_stub_true(unsigned int phy_id, struct rs_cb **rs_cb)
{
    *rs_cb = &g_tmp_rs_cb_t;
    (*rs_cb)->ping_cb.thread_status = RS_PING_THREAD_RUNNING;
    return 0;
}

int rs_get_rs_cb_stub1(unsigned int phy_id, struct rs_cb **rs_cb)
{
    g_tmp_rs_cb1.ping_cb.thread_status = RS_PING_THREAD_RUNNING;

    *rs_cb = &g_tmp_rs_cb1;
    (*rs_cb)->ping_cb.ping_pong_ops = rs_ping_roce_get_ops();
    (*rs_cb)->ping_cb.ping_pong_dfx = rs_ping_roce_get_dfx();
    return 0;
}

int rs_get_ping_cb_stub(struct ra_rs_dev_info *rdev, struct rs_ping_ctx_cb **ping_cb)
{
    *ping_cb = &g_tmp_ping_cb;
    (*ping_cb)->ping_pong_ops = rs_ping_roce_get_ops();
    (*ping_cb)->ping_pong_dfx = rs_ping_roce_get_dfx();
    return 0;
}

int rs_ping_roce_find_target_node_stub(struct rs_ping_ctx_cb *ping_cb, struct ping_qp_info *target,
    struct rs_ping_target_info **node)
{
    *node = &g_tmp_target;

    return -ENODEV;
}

int rs_ping_roce_find_target_node_stub1(struct rs_ping_ctx_cb *ping_cb, struct ping_qp_info *target,
    struct rs_ping_target_info **node)
{
    *node = &g_tmp_target1;

    return 0;
}

int rs_ping_roce_find_target_node_stub2(struct rs_ping_ctx_cb *ping_cb, struct ping_qp_info *target,
    struct rs_ping_target_info **node)
{
    *node = calloc(1, sizeof(struct rs_ping_target_info));
    RS_INIT_LIST_HEAD(&(*node)->list);
    (*node)->payload_size = 1;
    (*node)->payload_buffer = malloc(1);

    return 0;
}

int rs_pong_find_target_node_stub(struct rs_ping_ctx_cb *ping_cb, struct ping_qp_info *target,
    struct rs_pong_target_info **node)
{
    *node = NULL;
    return -19;
}

int rs_pong_find_target_node_stub2(struct rs_ping_ctx_cb *ping_cb, struct ping_qp_info *target,
    struct rs_pong_target_info **node)
{
    *node = calloc(1, sizeof(struct rs_pong_target_info));
    RS_INIT_LIST_HEAD(&(*node)->list);
    return 0;
}

int rs_pong_find_alloc_target_node_stub(struct rs_ping_ctx_cb *ping_cb, struct ping_qp_info *target,
    struct rs_pong_target_info **node)
{
    struct rs_pong_target_info tmp_node = { 0 };

    *node = &tmp_node;

    return 0;
}

int rs_ibv_get_cq_event_stub(struct ibv_comp_channel *channel, struct ibv_cq **cq, void **cq_context)
{
    *cq = &g_tmp_cq;

    return 0;
}

struct ibv_mr* rs_drv_mr_reg_stub(struct ibv_pd *pd, char *addr, size_t length, int access)
{
    return &g_ib_mr;
}

int rs_drv_mr_dereg_stub(struct ibv_mr *ib_mr)
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
    unsigned int chip_id = 0;
    int ret;

    mocker_invoke(rs_dev2rscb, rs_dev2rscb_stub0, 1);
    ret = rs_ping_handle_init(hdc_type, chip_id, white_list_status);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

void tc_rs_ping_handle_deinit()
{
    unsigned int chip_id = 0;
    int ret;

    g_tmp_rs_cb.ping_cb.thread_status = RS_PING_THREAD_RUNNING;
    pthread_mutex_init(&g_tmp_rs_cb.ping_cb.ping_mutex, NULL);
    mocker_invoke(rs_dev2rscb, rs_dev2rscb_stub0, 1);
    ret = rs_ping_handle_deinit(chip_id);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
    pthread_mutex_destroy(&g_tmp_rs_cb.ping_cb.ping_mutex);
}

void tc_rs_ping_init()
{
    struct rs_ping_local_qp_cb qp_cb = { 0 };
    union ping_qp_attr rdma_attr = { 0 };
    struct rs_ping_ctx_cb ping_cb = { 0 };
    struct ibv_context context = { 0 };
    struct ping_init_attr attr = { 0 };
    struct ping_init_info info = { 0 };
    struct ra_rs_dev_info rdev = { 0 };
    struct rs_cb tmp_rs_cb = { 0 };
    struct ibv_pd tmp_pd = { 0 };
    unsigned int rdev_index = 0;
    struct ibv_qp ib_qp = { 0 };
    struct ibv_pd pd = { 0 };
    int ret;

    mocker_invoke(rs_get_rs_cb, rs_get_rs_cb_stub, 20);
    mocker(rsGetLocalDevIDByHostDevID, 20, 0);
    mocker(rs_inet_ntop, 20, 0);
    mocker(rs_setup_sharemem, 20, 0);
    ret = rs_ping_init(&attr, &info, &rdev_index);
    EXPECT_INT_EQ(ret, -ENODEV);
    mocker_clean();

    mocker_invoke(rs_get_rs_cb, rs_get_rs_cb_stub_true, 20);
    mocker(rsGetLocalDevIDByHostDevID, 20, 0);
    mocker(rs_inet_ntop, 20, 0);
    mocker(rs_setup_sharemem, 20, 0);
    mocker(rs_ping_roce_ping_cb_init, 20, 0);
    ret = rs_ping_init(&attr, &info, &rdev_index);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    qp_cb.ib_qp = &ib_qp;
    mocker(rs_ibv_query_qp, 20, -1);
    ret = rs_ping_common_modify_local_qp(&ping_cb, &qp_cb);
    EXPECT_INT_NE(ret, 0);
    mocker_clean();

    mocker(rs_ibv_query_qp, 20, 0);
    mocker(rs_ibv_modify_qp, 20, -1);
    ret = rs_ping_common_modify_local_qp(&ping_cb, &qp_cb);
    EXPECT_INT_NE(ret, 0);
    mocker_clean();

    mocker(rs_ibv_query_qp, 20, 0);
    mocker(rs_ibv_modify_qp, 20, 0);
    ret = rs_ping_common_modify_local_qp(&ping_cb, &qp_cb);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    mocker((stub_fn_t)rs_ibv_create_cq, 10, NULL);
    ret = rs_ping_common_init_local_qp(&tmp_rs_cb, &ping_cb, &rdma_attr, &qp_cb);
    EXPECT_INT_NE(ret, 0);
    mocker_clean();

    ping_cb.rdev_cb.ib_ctx = &context;
    ping_cb.rdev_cb.ib_pd = &tmp_pd;
    mocker(rs_epoll_ctl, 20, 0);
    mocker(rs_ibv_exp_create_qp, 10, 0);
    ret = rs_ping_common_init_local_qp(&tmp_rs_cb, &ping_cb, &rdma_attr, &qp_cb);
    EXPECT_INT_NE(ret, 0);
    mocker_clean();

    mocker(rs_ping_common_init_mr_cb, 20, -1);
    ret = rs_ping_pong_init_local_buffer(&tmp_rs_cb, &attr, &info, &ping_cb);
    EXPECT_INT_NE(ret, 0);
    mocker_clean();

    mocker(rs_ping_common_init_mr_cb, 20, 0);
    ret = rs_ping_pong_init_local_buffer(&tmp_rs_cb, &attr, &info, &ping_cb);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    qp_cb.recv_mr_cb.sge_num = 1;
    qp_cb.qp_cap.max_recv_wr = 1;
    mocker(rs_ping_common_post_recv, 20, 0);
    ret = rs_ping_common_init_post_recv_all(&qp_cb);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    qp_cb.recv_mr_cb.sge_num = 1;
    qp_cb.qp_cap.max_recv_wr = 1;
    mocker(rs_ping_common_post_recv, 20, -1);
    ret = rs_ping_common_init_post_recv_all(&qp_cb);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();
}

void tc_rs_ping_target_add()
{
    struct ping_target_info target = { 0 };
    struct ra_rs_dev_info rdev = { 0 };
    int ret;

    target.local_info.rdma.hop_limit = 64;
    target.local_info.rdma.qos_attr.tc = (33 & 0x3f) << 2;
    target.local_info.rdma.qos_attr.sl = 4;
    g_tmp_ping_cb.task_status = RS_PING_TASK_RESET;
    RS_INIT_LIST_HEAD(&g_tmp_ping_cb.ping_list);
    mocker_invoke(rs_get_ping_cb, rs_get_ping_cb_stub, 1);
    ret = rs_ping_target_add(&rdev, &target);
    EXPECT_INT_EQ(ret, -9);
    mocker_clean();

    target.payload.size = 1;
    g_tmp_ping_cb.task_status = RS_PING_TASK_RESET;
    RS_INIT_LIST_HEAD(&g_tmp_ping_cb.ping_list);
    mocker_invoke(rs_get_ping_cb, rs_get_ping_cb_stub, 1);
    mocker(rs_ping_common_create_ah, 1, -1);
    ret = rs_ping_target_add(&rdev, &target);
    EXPECT_INT_NE(ret, 0);
    mocker_clean();

    mocker_invoke(rs_get_ping_cb, rs_get_ping_cb_stub, 1);
    mocker(calloc, 10, 0);
    ret = rs_ping_target_add(&rdev, &target);
    EXPECT_INT_NE(ret, 0);
    mocker_clean();
}

void tc_rs_get_ping_cb()
{
    struct ra_rs_dev_info rdev = { 0 };
    struct rs_ping_ctx_cb *ping_cb;
    int ret;

    mocker(rs_get_rs_cb, 1, -1);
    ret = rs_get_ping_cb(&rdev, &ping_cb);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

    mocker_invoke(rs_get_rs_cb, rs_get_rs_cb_stub1, 1);
    ret = rs_get_ping_cb(&rdev, &ping_cb);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

void tc_rs_ping_client_post_send()
{
    struct rs_ping_target_info target = { 0 };
    struct rs_ping_ctx_cb ping_cb = { 0 };
    struct ibv_qp server_ib_qp = { 0 };
    struct ibv_qp client_ib_qp = { 0 };
    struct ibv_sge sge = { 0 };
    void *addr = malloc(256);
    int ret;

    target.payload_buffer = malloc(1);
    target.payload_size = 1;
    sge.addr = (uintptr_t)addr;
    sge.length = 256;
    ping_cb.ping_qp.send_mr_cb.sge_num = 1;
    ping_cb.ping_qp.send_mr_cb.sge_list = &sge;
    ping_cb.pong_qp.ib_qp = &server_ib_qp;
    ping_cb.ping_qp.ib_qp = &client_ib_qp;
    mocker(rs_ibv_post_send, 1, -1);
    ret = rs_ping_roce_post_send(&ping_cb, &target);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

    mocker(rs_ibv_post_send, 1, 0);
    ret = rs_ping_roce_post_send(&ping_cb, &target);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    free(addr);
    addr = NULL;
    free(target.payload_buffer);
    target.payload_buffer = NULL;
}

void tc_rs_ping_get_results()
{
    struct ping_target_comm_info target = { 0 };
    struct ping_result_info result = { 0 };
    struct ra_rs_dev_info rdev = { 0 };
    unsigned int num = 1;
    int ret;

    g_tmp_ping_cb.task_status = RS_PING_TASK_RESET;
    mocker_invoke(rs_get_ping_cb, rs_get_ping_cb_stub, 1);
    mocker_invoke(rs_ping_roce_find_target_node, rs_ping_roce_find_target_node_stub, 1);
    ret = rs_ping_get_results(&rdev, &target, &num, &result);
    EXPECT_INT_EQ(ret, -ENODEV);
    mocker_clean();

    num = 1;
    g_tmp_ping_cb.task_status = RS_PING_TASK_RESET;
    mocker_invoke(rs_get_ping_cb, rs_get_ping_cb_stub, 1);
    mocker_invoke(rs_ping_roce_find_target_node, rs_ping_roce_find_target_node_stub1, 1);
    ret = rs_ping_get_results(&rdev, &target, &num, &result);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

void tc_rs_ping_task_stop()
{
    struct ra_rs_dev_info rdev = { 0 };
    int ret;

    pthread_mutex_init(&g_tmp_ping_cb.ping_mutex, NULL);
    mocker_invoke(rs_get_ping_cb, rs_get_ping_cb_stub, 1);
    ret = rs_ping_task_stop(&rdev);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
    pthread_mutex_destroy(&g_tmp_ping_cb.ping_mutex);
}

void tc_rs_ping_target_del()
{
    struct ping_target_comm_info target = { 0 };
    struct ra_rs_dev_info rdev = { 0 };
    unsigned int num = 1;
    int ret;

    g_tmp_ping_cb.task_status = RS_PING_TASK_RESET;
    mocker_invoke(rs_get_ping_cb, rs_get_ping_cb_stub, 1);
    mocker_invoke(rs_ping_roce_find_target_node, rs_ping_roce_find_target_node_stub2, 1);
    ret = rs_ping_target_del(&rdev, &target, &num);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

void tc_rs_ping_deinit()
{
    struct ibv_comp_channel client_channel = { 0 };
    struct ibv_comp_channel server_channel = { 0 };
    struct ping_target_info target = { 0 };
    struct ra_rs_dev_info rdev = { 0 };
    struct rs_cb rs_cb = { 0 };
    int ret;

    g_tmp_ping_cb.init_cnt = 1;
    g_tmp_ping_cb.ping_qp.channel = &client_channel;
    g_tmp_ping_cb.pong_qp.channel = &server_channel;
    RS_INIT_LIST_HEAD(&g_tmp_ping_cb.ping_list);
    RS_INIT_LIST_HEAD(&g_tmp_ping_cb.pong_list);
    target.payload.size = 1;
    mocker_invoke(rs_get_ping_cb, rs_get_ping_cb_stub, 10);
    mocker(rs_ping_common_create_ah, 1, 0);
    ret = rs_ping_target_add(&rdev, &target);
    EXPECT_INT_EQ(ret, 0);
    mocker(rs_epoll_ctl, 20, 0);
    mocker(rs_ibv_destroy_comp_channel, 20, 0);
    ret = rs_ping_deinit(&rdev);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

void tc_rs_ping_compare_rdma_info()
{
    struct ping_qp_info a = { 0 };
    struct ping_qp_info b = { 0 };
    int ret;

    ret = rs_ping_common_compare_rdma_info(&a, &b);
    EXPECT_INT_EQ(ret, true);

    a.rdma.qpn = 1;
    b.rdma.qpn = 2;
    ret = rs_ping_common_compare_rdma_info(&a, &b);
    EXPECT_INT_EQ(ret, false);

    a.rdma.qpn = 1;
    b.rdma.qpn = 1;
    a.rdma.qkey = 1;
    b.rdma.qkey = 2;
    ret = rs_ping_common_compare_rdma_info(&a, &b);
    EXPECT_INT_EQ(ret, false);

    a.rdma.qpn = 1;
    b.rdma.qpn = 1;
    a.rdma.qkey = 1;
    b.rdma.qkey = 1;
    a.rdma.gid.raw[0] = 1;
    b.rdma.gid.raw[0] = 2;
    ret = rs_ping_common_compare_rdma_info(&a, &b);
    EXPECT_INT_EQ(ret, false);
}

void tc_rs_ping_roce_find_target_node()
{
    struct rs_ping_target_info stub_node = { 0 };
    struct rs_ping_target_info *node = NULL;
    struct rs_ping_ctx_cb ping_cb = { 0 };
    struct ping_qp_info target = { 0 };
    int ret;

    RS_INIT_LIST_HEAD(&ping_cb.ping_list);
    ret = rs_ping_roce_find_target_node(&ping_cb, &target, &node);
    EXPECT_INT_EQ(ret, -ENODEV);

    rs_list_add_tail(&stub_node.list, &ping_cb.ping_list);
    ret = rs_ping_roce_find_target_node(&ping_cb, &target, &node);
    EXPECT_INT_EQ(ret, 0);
}

void tc_rs_pong_find_target_node()
{
    struct rs_pong_target_info stub_node = { 0 };
    struct rs_pong_target_info *node = NULL;
    struct rs_ping_ctx_cb ping_cb = { 0 };
    struct ping_qp_info target = { 0 };
    int ret;

    RS_INIT_LIST_HEAD(&ping_cb.pong_list);
    ret = rs_pong_find_target_node(&ping_cb, &target, &node);
    EXPECT_INT_EQ(ret, -ENODEV);

    rs_list_add_tail(&stub_node.list, &ping_cb.pong_list);
    ret = rs_pong_find_target_node(&ping_cb, &target, &node);
    EXPECT_INT_EQ(ret, 0);
}

void tc_rs_pong_find_alloc_target_node()
{
    struct rs_pong_target_info *node = NULL;
    struct rs_ping_ctx_cb ping_cb = { 0 };
    struct ping_qp_info target = { 0 };
    int ret;

    mocker_invoke(rs_pong_find_target_node, rs_pong_find_target_node_stub2, 1);
    mocker(rs_ping_common_create_ah, 1, -1);
    ret = rs_pong_find_alloc_target_node(&ping_cb, &target, &node);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

    pthread_mutex_init(&ping_cb.pong_mutex, NULL);
    RS_INIT_LIST_HEAD(&ping_cb.pong_list);
    mocker_invoke(rs_pong_find_target_node, rs_pong_find_target_node_stub, 1);
    mocker(rs_ping_common_create_ah, 1, 0);
    ret = rs_pong_find_alloc_target_node(&ping_cb, &target, &node);
    free(node);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

void tc_rs_ping_poll_send_cq()
{
    struct rs_ping_local_qp_cb qp_cb = { 0 };
    int ret;

    mocker(rs_ibv_poll_cq, 1, -1);
    ret = rs_ping_common_poll_scq(&qp_cb);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    mocker(rs_ibv_poll_cq, 1, 1);
    ret = rs_ping_common_poll_scq(&qp_cb);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

void tc_rs_ping_server_post_send()
{
    struct rs_ping_ctx_cb ping_cb = { 0 };
    struct timeval timestamp2;
    void *send_addr = malloc(1024);
    void *recv_addr = malloc(1024);
    struct ibv_wc wc = { 0};
    int ret;

    wc.wr_id = 1;
    mocker(rs_ping_common_poll_scq, 1, 0);
    ret = rs_pong_post_send(&ping_cb, &wc, &timestamp2);
    EXPECT_INT_EQ(ret, -EIO);
    mocker_clean();

    wc.wr_id = 0;
    wc.byte_len = 16 + RS_PING_PAYLOAD_HEADER_RESV_GRH;
    ping_cb.ping_qp.recv_mr_cb.sge_list = calloc(1, sizeof(struct ibv_sge));
    ping_cb.pong_qp.send_mr_cb.sge_list = calloc(1, sizeof(struct ibv_sge));
    ping_cb.pong_qp.send_mr_cb.sge_num = 1;
    ping_cb.pong_qp.send_mr_cb.sge_list->addr = (uintptr_t)send_addr;
    ping_cb.pong_qp.send_mr_cb.sge_list->length = 1024;
    ping_cb.ping_qp.recv_mr_cb.sge_list->addr = (uintptr_t)recv_addr;
    ping_cb.ping_qp.recv_mr_cb.sge_list->length = 1024;
    mocker(rs_ping_common_poll_scq, 1, 0);
    mocker(rs_pong_find_alloc_target_node, 1, -1);
    ret = rs_pong_post_send(&ping_cb, &wc, &timestamp2);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

    mocker(rs_ping_common_poll_scq, 1, 0);
    mocker_invoke(rs_pong_find_alloc_target_node, rs_pong_find_alloc_target_node_stub, 1);
    mocker(gettimeofday, 20, 1);
    mocker(rs_ibv_post_send, 1, -1);
    ret = rs_pong_post_send(&ping_cb, &wc, &timestamp2);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

    mocker(rs_ping_common_poll_scq, 1, 0);
    mocker_invoke(rs_pong_find_alloc_target_node, rs_pong_find_alloc_target_node_stub, 1);
    mocker(rs_ibv_post_send, 1, 0);
    ret = rs_pong_post_send(&ping_cb, &wc, &timestamp2);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    free(ping_cb.pong_qp.send_mr_cb.sge_list);
    ping_cb.pong_qp.send_mr_cb.sge_list = NULL;
    free(ping_cb.ping_qp.recv_mr_cb.sge_list);
    ping_cb.ping_qp.recv_mr_cb.sge_list = NULL;
    free(recv_addr);
    recv_addr = NULL;
    free(send_addr);
    send_addr = NULL;
}

void tc_rs_ping_post_recv()
{
    struct rs_ping_local_qp_cb qp_cb = { 0 };
    void *recv_addr = malloc(1024);
    int ret;

    qp_cb.recv_mr_cb.sge_list = calloc(1, sizeof(struct ibv_sge));
    qp_cb.recv_mr_cb.sge_list->addr = (uintptr_t)recv_addr;
    qp_cb.recv_mr_cb.sge_list->length = 1024;
    qp_cb.recv_mr_cb.sge_num = 1;
    mocker(rs_ibv_post_recv, 1, -1);
    ret = rs_ping_common_post_recv(&qp_cb);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

    mocker(rs_ibv_post_recv, 1, 0);
    ret = rs_ping_common_post_recv(&qp_cb);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    free(qp_cb.recv_mr_cb.sge_list);
    qp_cb.recv_mr_cb.sge_list = NULL;
    free(recv_addr);
    recv_addr = NULL;
}

void tc_rs_ping_client_poll_cq()
{
    struct rs_ping_ctx_cb ping_cb = { 0 };
    struct timeval timestamp2;
    int polled_cnt;
    int ret;

    mocker_clean();
    mocker(rs_ibv_get_cq_event, 1, -1);
    ret = rs_ping_roce_poll_rcq(&ping_cb, &polled_cnt, &timestamp2);
    EXPECT_INT_EQ(ret, -259);
    mocker_clean();

    mocker(rs_ibv_get_cq_event, 1, 0);
    mocker(rs_ibv_poll_cq, 1, 0);
    ret = rs_ping_roce_poll_rcq(&ping_cb, &polled_cnt, &timestamp2);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    ping_cb.ping_qp.recv_cq.ib_cq = &g_tmp_cq;
    mocker_invoke(rs_ibv_get_cq_event, rs_ibv_get_cq_event_stub, 1);
    mocker(rs_ibv_poll_cq, 1, -1);
    ret = rs_ping_roce_poll_rcq(&ping_cb, &polled_cnt, &timestamp2);
    EXPECT_INT_EQ(ret, -259);
    mocker_clean();

    ping_cb.ping_qp.recv_cq.max_recv_wc_num = 16;
    mocker_invoke(rs_ibv_get_cq_event, rs_ibv_get_cq_event_stub, 1);
    mocker(rs_ibv_poll_cq, 1, 1);
    mocker(rs_pong_post_send, 1, -1);
    ret = rs_ping_roce_poll_rcq(&ping_cb, &polled_cnt, &timestamp2);
    EXPECT_INT_EQ(ret, 0);
    rs_pong_roce_handle_send(&ping_cb, polled_cnt, &timestamp2);
    mocker_clean();

    mocker_invoke(rs_ibv_get_cq_event, rs_ibv_get_cq_event_stub, 1);
    mocker(rs_ibv_poll_cq, 1, 1);
    mocker(rs_pong_post_send, 1, 0);
    mocker(rs_ping_common_post_recv, 1, -1);
    ret = rs_ping_roce_poll_rcq(&ping_cb, &polled_cnt, &timestamp2);
    EXPECT_INT_EQ(ret, 0);
    rs_pong_roce_handle_send(&ping_cb, polled_cnt, &timestamp2);
    mocker_clean();

    mocker_invoke(rs_ibv_get_cq_event, rs_ibv_get_cq_event_stub, 1);
    mocker(rs_ibv_poll_cq, 1, 1);
    mocker(rs_pong_post_send, 1, 0);
    mocker(rs_ping_common_post_recv, 1, 0);
    mocker(rs_ibv_req_notify_cq, 1, -1);
    ret = rs_ping_roce_poll_rcq(&ping_cb, &polled_cnt, &timestamp2);
    EXPECT_INT_EQ(ret, 0);
    rs_pong_roce_handle_send(&ping_cb, polled_cnt, &timestamp2);
    mocker_clean();
}

void tc_rs_epoll_event_ping_handle()
{
    struct ibv_comp_channel ping_channel = { 0 };
    struct ibv_comp_channel pong_channel = { 0 };
    struct rs_cb rscb = { 0 };
    int ret;

    pong_channel.fd = 1;
    pthread_mutex_init(&rscb.ping_cb.dev_mutex, NULL);
    rscb.ping_cb.init_cnt = 1;
    rscb.ping_cb.ping_qp.channel = &ping_channel;
    rscb.ping_cb.pong_qp.channel = &pong_channel;
    rscb.ping_cb.thread_status = RS_PING_THREAD_RUNNING;
    rscb.ping_cb.ping_pong_ops = rs_ping_roce_get_ops();
    rscb.ping_cb.ping_pong_dfx = rs_ping_roce_get_dfx();

    mocker(rs_ping_roce_poll_rcq, 10, 0);
    mocker(rs_pong_roce_handle_send, 10, 0);
    mocker(rs_pong_roce_poll_rcq, 10, 0);

    ret = rs_epoll_event_ping_handle(&rscb, 0);
    EXPECT_INT_EQ(ret, 0);
    ret = rs_epoll_event_ping_handle(&rscb, 1);
    EXPECT_INT_EQ(ret, 0);
    rscb.ping_cb.init_cnt = 0;
    ret = rs_epoll_event_ping_handle(&rscb, 1);
    EXPECT_INT_EQ(ret, -ENODEV);
    mocker_clean();
    return;
}

void tc_rs_ping_get_trip_time()
{
    struct rs_ping_timestamp timestamp = { 0 };
    unsigned int ret;

    ret = rs_ping_get_trip_time(&timestamp);
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
    struct rs_ping_ctx_cb ping_cb = { 0 };
    int ret;
    mocker(pthread_mutex_init, 10, -5);
    ret = rs_ping_cb_init_mutex(&ping_cb);
    EXPECT_INT_EQ(ret, -258);
    mocker_clean();
}

void tc_rs_ping_cb_init_mutex_ab2()
{
    struct rs_ping_ctx_cb ping_cb = { 0 };
    int ret;
    mocker_invoke(pthread_mutex_init, pthread_mutex_init_stub_2, 10);
    ret = rs_ping_cb_init_mutex(&ping_cb);
    EXPECT_INT_EQ(ret, -258);
    mocker_clean();
}

void tc_rs_ping_cb_init_mutex_ab3()
{
    struct rs_ping_ctx_cb ping_cb = { 0 };
    int ret;
    mocker_invoke(pthread_mutex_init, pthread_mutex_init_stub_3, 10);
    ret = rs_ping_cb_init_mutex(&ping_cb);
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
    struct rs_ping_ctx_cb ping_cb = { 0 };
    struct timeval timestamp4 = { 0 };
    void *recv_addr = calloc(1, 1024);
    uint32_t sge_idx = 0;
    int ret;

    ping_cb.pong_qp.recv_mr_cb.sge_list = calloc(1, sizeof(struct ibv_sge));
    ping_cb.pong_qp.recv_mr_cb.sge_list->addr = (uintptr_t)recv_addr;
    ping_cb.pong_qp.recv_mr_cb.sge_list->length = 1024;
    ping_cb.task_id = 1;

    ret = rs_pong_resolve_response_packet(&ping_cb, sge_idx, &timestamp4);
    EXPECT_INT_EQ(ret, 0);

    ping_cb.task_id = 0;
    mocker_invoke(rs_ping_roce_find_target_node, rs_ping_roce_find_target_node_stub, 1);
    ret = rs_pong_resolve_response_packet(&ping_cb, sge_idx, &timestamp4);
    EXPECT_INT_EQ(ret, -ENODEV);
    mocker_clean();

    g_tmp_target1.result_summary.rtt_max = 10;
    g_tmp_target1.result_summary.rtt_min = 4;
    pthread_mutex_init(&g_tmp_target1.trip_mutex, NULL);
    mocker(rs_ping_get_trip_time, 1, 11);
    mocker_invoke(rs_ping_roce_find_target_node, rs_ping_roce_find_target_node_stub1, 1);
    ret = rs_pong_resolve_response_packet(&ping_cb, sge_idx, &timestamp4);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    g_tmp_target1.result_summary.rtt_max = 10;
    g_tmp_target1.result_summary.rtt_min = 4;
    g_tmp_target1.result_summary.task_attr.timeout_interval = 12;
    pthread_mutex_init(&g_tmp_target1.trip_mutex, NULL);
    mocker(rs_ping_get_trip_time, 1, 11);
    mocker_invoke(rs_ping_roce_find_target_node, rs_ping_roce_find_target_node_stub1, 1);
    ret = rs_pong_resolve_response_packet(&ping_cb, sge_idx, &timestamp4);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    pthread_mutex_destroy(&g_tmp_target1.trip_mutex);
    free(ping_cb.pong_qp.recv_mr_cb.sge_list);
    ping_cb.pong_qp.recv_mr_cb.sge_list = NULL;
    free(recv_addr);
    recv_addr = NULL;
}

void tc_rs_ping_server_poll_cq()
{
    struct rs_ping_ctx_cb ping_cb = { 0 };

    mocker(rs_ibv_get_cq_event, 1, -1);
    rs_pong_roce_poll_rcq(&ping_cb);
    mocker_clean();

    mocker(rs_ibv_get_cq_event, 1, 0);
    rs_pong_roce_poll_rcq(&ping_cb);
    mocker_clean();

    ping_cb.pong_qp.recv_cq.ib_cq = &g_tmp_cq;
    ping_cb.pong_qp.recv_cq.max_recv_wc_num = 16;
    mocker_invoke(rs_ibv_get_cq_event, rs_ibv_get_cq_event_stub, 1);
    mocker(rs_ibv_poll_cq, 1, -1);
    rs_pong_roce_poll_rcq(&ping_cb);
    mocker_clean();

    mocker_invoke(rs_ibv_get_cq_event, rs_ibv_get_cq_event_stub, 1);
    mocker(rs_ibv_poll_cq, 1, 1);
    mocker(rs_pong_resolve_response_packet, 1, -1);
    rs_pong_roce_poll_rcq(&ping_cb);
    mocker_clean();

    mocker_invoke(rs_ibv_get_cq_event, rs_ibv_get_cq_event_stub, 1);
    mocker(rs_ibv_poll_cq, 1, 1);
    mocker(rs_pong_resolve_response_packet, 1, 0);
    mocker(rs_ping_common_post_recv, 1, -1);
    rs_pong_roce_poll_rcq(&ping_cb);
    mocker_clean();

    mocker_invoke(rs_ibv_get_cq_event, rs_ibv_get_cq_event_stub, 1);
    mocker(rs_ibv_poll_cq, 1, 1);
    mocker(rs_pong_resolve_response_packet, 1, 0);
    mocker(rs_ping_common_post_recv, 1, 0);
    mocker(rs_ibv_req_notify_cq, 1, -1);
    rs_pong_roce_poll_rcq(&ping_cb);
    mocker_clean();
}

void tc_rs_ping_cb_get_dev_rdev_index()
{
#ifndef CUSTOM_INTERFACE
#define CUSTOM_INTERFACE
#endif

    struct ibv_device *dev_list = calloc(1, sizeof(struct ibv_device));
    struct rs_ping_ctx_cb ping_cb = { 0 };
    int index = 0;
    int ret;

    pthread_mutex_init(&ping_cb.ping_mutex, NULL);
    ping_cb.rdev_cb.dev_list = &dev_list;
    mocker(rs_ibv_get_device_name, 1, "dev");
    mocker(rs_roce_get_roce_dev_data, 1, -1);
    ret = rs_ping_cb_get_dev_rdev_index(&ping_cb, index);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

    mocker(rs_ibv_get_device_name, 1, "dev");
    mocker(rs_roce_get_roce_dev_data, 1, 0);
    ret = rs_ping_cb_get_dev_rdev_index(&ping_cb, index);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    free(dev_list);
    dev_list = NULL;
    pthread_mutex_destroy(&ping_cb.ping_mutex);
}

void tc_rs_ping_init_mr_cb()
{
    struct rs_ping_ctx_cb ping_cb = { 0 };
    struct rs_ping_mr_cb mr_cb = { 0 };
    struct rs_cb rscb = { 0 };
    struct ibv_mr mr = { 0 };
    int ret;

    mocker((stub_fn_t)pthread_mutex_init, 1, -1);
    ret = rs_ping_common_init_mr_cb(&rscb, &ping_cb, &mr_cb);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

    ret = rs_ping_common_init_mr_cb(&rscb, &ping_cb, &mr_cb);
    EXPECT_INT_NE(ret, 0);

    mocker(dl_hal_buff_alloc_align_ex, 1, 0);
    mocker(rs_drv_mr_reg, 1, NULL);
    ret = rs_ping_common_init_mr_cb(&rscb, &ping_cb, &mr_cb);
    EXPECT_INT_NE(ret, 0);
    mocker_clean();

    mocker(dl_hal_buff_alloc_align_ex, 1, 0);
    mocker_invoke(rs_drv_mr_reg, rs_drv_mr_reg_stub, 1);
    mocker_invoke(rs_drv_mr_dereg, rs_drv_mr_dereg_stub, 1);
    mocker(calloc, 10, NULL);
    ret = rs_ping_common_init_mr_cb(&rscb, &ping_cb, &mr_cb);
    EXPECT_INT_NE(ret, 0);
    mocker_clean();

    mocker(dl_hal_buff_alloc_align_ex, 1, 0);
    mocker_invoke(rs_drv_mr_reg, rs_drv_mr_reg_stub, 1);
    mocker_invoke(rs_drv_mr_dereg, rs_drv_mr_dereg_stub, 1);
    mr_cb.sge_num = 1;
    ret = rs_ping_common_init_mr_cb(&rscb, &ping_cb, &mr_cb);
    EXPECT_INT_EQ(ret, 0);

    mocker(dl_hal_buff_free, 1, 0);
    rs_ping_common_deinit_mr_cb(&mr_cb);
    mocker_clean();
}

void tc_rs_ping_common_deinit_local_buffer()
{
    struct rs_ping_ctx_cb ping_cb = { 0 };
    ping_cb.pong_qp.recv_mr_cb.sge_list = calloc(1, sizeof(struct ibv_sge));
    ping_cb.pong_qp.send_mr_cb.sge_list = calloc(1, sizeof(struct ibv_sge));
    ping_cb.ping_qp.recv_mr_cb.sge_list = calloc(1, sizeof(struct ibv_sge));
    ping_cb.ping_qp.send_mr_cb.sge_list = calloc(1, sizeof(struct ibv_sge));

    mocker(rs_drv_mr_dereg, 20, 0);
    mocker(dl_hal_buff_free, 20, 0);
    mocker(pthread_mutex_destroy, 20, 0);

    rs_ping_common_deinit_local_buffer(&ping_cb);

    mocker_clean();
}

void tc_rs_ping_common_deinit_local_qp()
{
    struct rs_ping_local_qp_cb qp_cb = { 0 };
    struct ibv_comp_channel channel = { 0 };
    struct rs_ping_ctx_cb ping_cb = { 0 };
    struct rs_cb rscb = { 0 };

    qp_cb.channel = &channel;
    mocker(rs_ibv_destroy_qp, 20, 0);
    mocker(pthread_mutex_destroy, 20, 0);
    mocker(rs_ibv_ack_cq_events, 20, 0);
    mocker(rs_ibv_destroy_cq, 20, 0);
    mocker(rs_epoll_ctl, 20, 0);
    mocker(rs_ibv_destroy_comp_channel, 20, 0);

    rs_ping_common_deinit_local_qp(NULL, NULL, NULL);
    rs_ping_common_deinit_local_qp(&rscb, &ping_cb, &qp_cb);
    mocker_clean();
}

void tc_rs_ping_pong_init_local_info()
{
    struct rs_ping_ctx_cb ping_cb = { 0 };
    struct ping_init_attr attr = { 0 };
    struct ping_init_info info = { 0 };
    struct ibv_qp ib_qp = { 0 };
    struct rs_cb rscb = { 0 };
    int ret;

    ping_cb.ping_qp.ib_qp = &ib_qp;
    ping_cb.pong_qp.ib_qp = &ib_qp;

    mocker(rs_ping_common_init_local_qp, 20, 0);
    mocker(rs_ping_pong_init_local_buffer, 20, 0);
    mocker(rs_ping_common_init_post_recv_all, 20, 0);
    mocker(rs_ping_common_deinit_local_buffer, 20, 0);
    mocker(rs_ping_common_deinit_local_qp, 20, 0);
    ret = rs_ping_pong_init_local_info(&rscb, &attr, &info, &ping_cb);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    mocker(rs_ping_common_init_local_qp, 20, -1);
    mocker(rs_ping_pong_init_local_buffer, 20, 0);
    mocker(rs_ping_common_init_post_recv_all, 20, 0);
    mocker(rs_ping_common_deinit_local_buffer, 20, 0);
    mocker(rs_ping_common_deinit_local_qp, 20, 0);
    ret = rs_ping_pong_init_local_info(&rscb, &attr, &info, &ping_cb);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

    mocker(rs_ping_common_init_local_qp, 20, 0);
    mocker(rs_ping_pong_init_local_buffer, 20, -1);
    mocker(rs_ping_common_init_post_recv_all, 20, 0);
    mocker(rs_ping_common_deinit_local_buffer, 20, 0);
    mocker(rs_ping_common_deinit_local_qp, 20, 0);
    ret = rs_ping_pong_init_local_info(&rscb, &attr, &info, &ping_cb);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

    mocker(rs_ping_common_init_local_qp, 20, 0);
    mocker(rs_ping_pong_init_local_buffer, 20, 0);
    mocker(rs_ping_common_init_post_recv_all, 20, -1);
    mocker(rs_ping_common_deinit_local_buffer, 20, 0);
    mocker(rs_ping_common_deinit_local_qp, 20, 0);
    ret = rs_ping_pong_init_local_info(&rscb, &attr, &info, &ping_cb);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();
}

int rs_ibv_poll_cq_stub_ping0(struct ibv_cq *cq, int num_entries, struct ibv_wc *wc)
{
    return 0;
}

int rs_ibv_poll_cq_stub_ping1(struct ibv_cq *cq, int num_entries, struct ibv_wc *wc)
{
    wc->status = IBV_WC_LOC_LEN_ERR;
    return 1;
}

int rs_ibv_poll_cq_stub_ping2(struct ibv_cq *cq, int num_entries, struct ibv_wc *wc)
{
    wc->status = IBV_WC_SUCCESS;
    return 1;
}

void tc_rs_ping_roce_poll_scq()
{
    struct rs_ping_target_info target = { 0 };
    struct rs_ping_ctx_cb ping_cb = { 0 };
    int ret;

    mocker_invoke(rs_ibv_poll_cq, rs_ibv_poll_cq_stub_ping0, 10);
    ret = rs_ping_roce_poll_scq(&ping_cb, &target);
    EXPECT_INT_EQ(ret, -61);
    mocker_clean();

    mocker_invoke(rs_ibv_poll_cq, rs_ibv_poll_cq_stub_ping1, 10);
    ret = rs_ping_roce_poll_scq(&ping_cb, &target);
    EXPECT_INT_EQ(ret, -259);
    mocker_clean();

    mocker_invoke(rs_ibv_poll_cq, rs_ibv_poll_cq_stub_ping2, 10);
    ret = rs_ping_roce_poll_scq(&ping_cb, &target);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

int stub_rs_ping_client_post_send(struct rs_ping_ctx_cb *ping_cb, struct rs_ping_target_info *target)
{
    ping_cb->thread_status = 0;
    return 1;
}

void tc_rs_ping_handle()
{
    struct rs_cb rs_cb = {0};
    struct rs_ping_target_info target_tmp = {0};

    mocker_clean();
    rs_cb.ping_cb.thread_status = 1;
    rs_cb.ping_cb.task_status = 1;
    rs_cb.ping_cb.task_attr.packet_cnt = 1;
    RS_INIT_LIST_HEAD(&rs_cb.ping_cb.ping_list);
    rs_cb.ping_cb.ping_list.next = &target_tmp.list;
    rs_cb.ping_cb.ping_list.prev = &target_tmp.list;
    rs_cb.ping_cb.ping_pong_ops = rs_ping_roce_get_ops();
    rs_cb.ping_cb.ping_pong_dfx = rs_ping_roce_get_dfx();
    target_tmp.list.next = &rs_cb.ping_cb.ping_list;
    target_tmp.list.prev = &rs_cb.ping_cb.ping_list;
    target_tmp.state = 1;
    mocker(rs_get_cur_time, 1, 0);
    mocker(strncpy_s, 1, 0);
    mocker(rs_heartbeat_alive_print, 1, 0);
    mocker(rs_list_empty, 1, 0);
    mocker_invoke(rs_ping_roce_post_send, stub_rs_ping_client_post_send, 1);
    mocker(pthread_mutex_lock, 1, 0);
    mocker(pthread_mutex_unlock, 1, 0);
    rs_ping_handle((void *)&rs_cb);
    mocker_clean();
}

void tc_rs_ping_roce_ping_cb_deinit()
{
    struct rs_ping_target_info *stub_ping_node;
    struct rs_pong_target_info *stub_pong_node;
    struct rs_ping_ctx_cb ping_cb = { 0 };

    mocker(rs_get_rs_cb, 20, 0);
    mocker(pthread_mutex_lock, 20, 0);
    mocker(pthread_mutex_unlock, 20, 0);

    stub_ping_node = calloc(1, sizeof(struct rs_ping_target_info));
    stub_pong_node = calloc(1, sizeof(struct rs_pong_target_info));
    RS_INIT_LIST_HEAD(&ping_cb.ping_list);
    rs_list_add_tail(&stub_ping_node->list, &ping_cb.ping_list);
    RS_INIT_LIST_HEAD(&ping_cb.pong_list);
    rs_list_add_tail(&stub_pong_node->list, &ping_cb.pong_list);
    mocker(rs_ibv_destroy_ah, 20, 0);

    mocker(rs_ping_common_deinit_local_qp, 20, 0);
    mocker(rs_ping_common_deinit_local_buffer, 20, 0);
    mocker(rs_ibv_dealloc_pd, 20, 0);
    mocker(rs_ibv_close_device, 20, 0);
    mocker(rs_ibv_free_device_list, 20, 0);

    rs_ping_roce_ping_cb_deinit(0, &ping_cb);
}

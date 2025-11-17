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

extern int rs_get_ping_cb(struct ra_rs_dev_info *rdev, struct rs_ping_ctx_cb **ping_cb);
extern int rs_ping_urma_poll_rcq(struct rs_ping_ctx_cb *ping_cb, int *polled_cnt, struct timeval *timestamp2);
extern void rs_pong_urma_handle_send(struct rs_ping_ctx_cb *ping_cb, int polled_cnt, struct timeval *timestamp2);
extern void rs_pong_urma_poll_rcq(struct rs_ping_ctx_cb *ping_cb);
extern int rs_ping_urma_poll_scq(struct rs_ping_ctx_cb *ping_cb, struct rs_ping_target_info *target);
extern struct rs_ping_pong_ops *rs_ping_urma_get_ops(void);
extern struct rs_ping_pong_dfx *rs_ping_urma_get_dfx(void);
extern int rs_ping_common_import_jetty(urma_context_t *urma_ctx, struct ping_qp_info *target,
    urma_target_jetty_t **import_tjetty);
extern int rs_pong_jetty_post_send(struct rs_ping_ctx_cb *ping_cb, urma_cr_t *cr, struct timeval *timestamp2);
extern int rs_ping_common_jfr_post_recv(struct rs_ping_local_jetty_cb *jetty_cb);
extern int rs_pong_jetty_resolve_response_packet(struct rs_ping_ctx_cb *ping_cb, uint32_t sge_idx, struct timeval *timestamp4);
extern int rs_ping_urma_find_target_node(struct rs_ping_ctx_cb *ping_cb, struct ping_qp_info *target,
    struct rs_ping_target_info **node);
extern int rs_ping_common_poll_send_jfc(struct rs_ping_local_jetty_cb *jetty_cb);
extern int rs_pong_jetty_find_alloc_target_node(struct rs_ping_ctx_cb *ping_cb, struct ping_qp_info *target,
    struct rs_pong_target_info **node);
extern int rs_pong_jetty_find_target_node(struct rs_ping_ctx_cb *ping_cb, struct ping_qp_info *target,
    struct rs_pong_target_info **node);
extern int rs_get_jetty_info(struct ping_qp_info *qp_info, urma_jetty_id_t *jetty_id, urma_eid_t *eid);

urma_jfc_t g_tmp_jfc;
static struct rs_cb g_tmp_rs_cb;
static struct rs_ping_ctx_cb g_tmp_ping_cb;
static struct rs_ping_target_info g_tmp_target;
static struct rs_ping_target_info g_tmp_target1;

#define TEST_SGE_LIST_LEN 1024

int rs_get_rs_cb_urma_stub(unsigned int phy_id, struct rs_cb **rs_cb)
{
    *rs_cb = &g_tmp_rs_cb;
    (*rs_cb)->ping_cb.ping_pong_ops = rs_ping_urma_get_ops();
    (*rs_cb)->ping_cb.ping_pong_dfx = rs_ping_urma_get_dfx();
    return 0;
}

int rs_get_ping_cb_urma_stub(struct ra_rs_dev_info *rdev, struct rs_ping_ctx_cb **ping_cb)
{
    *ping_cb = &g_tmp_ping_cb;
    (*ping_cb)->ping_pong_ops = rs_ping_urma_get_ops();
    (*ping_cb)->ping_pong_dfx = rs_ping_urma_get_dfx();
    return 0;
}

int rs_get_ping_cb_urma_stub1(struct ra_rs_dev_info *rdev, struct rs_ping_ctx_cb **ping_cb)
{
    *ping_cb = &g_tmp_rs_cb.ping_cb;
    (*ping_cb)->ping_pong_ops = rs_ping_urma_get_ops();
    (*ping_cb)->ping_pong_dfx = rs_ping_urma_get_dfx();
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

int rs_ping_urma_find_target_node_stub(struct rs_ping_ctx_cb *ping_cb, struct ping_qp_info *target,
    struct rs_ping_target_info **node)
{
    *node = &g_tmp_target;

    return -ENODEV;
}

int rs_ping_urma_find_target_node_stub1(struct rs_ping_ctx_cb *ping_cb, struct ping_qp_info *target,
    struct rs_ping_target_info **node)
{
    *node = &g_tmp_target1;

    return 0;
}


int rs_pong_jetty_find_alloc_target_node_stub(struct rs_ping_ctx_cb *ping_cb, struct ping_qp_info *target,
    struct rs_pong_target_info **node)
{
    static struct rs_pong_target_info tmp_node = {0};

    *node = &tmp_node;

    return 0;
}

int rs_pong_jetty_find_target_node_stub(struct rs_ping_ctx_cb *ping_cb, struct ping_qp_info *target,
    struct rs_pong_target_info **node)
{
    *node = NULL;
    return -19;
}

int rs_pong_jetty_find_target_node_stub2(struct rs_ping_ctx_cb *ping_cb, struct ping_qp_info *target,
    struct rs_pong_target_info **node)
{
    *node = calloc(1, sizeof(struct rs_pong_target_info));
    RS_INIT_LIST_HEAD(&(*node)->list);
    return 0;
}

void tc_rs_ping_init_deinit_urma()
{
    struct ibv_comp_channel client_channel = {0};
    struct ibv_comp_channel server_channel = {0};
    struct ping_target_info target = {0};
    struct ping_init_attr attr = {0};
    struct ping_init_info info = {0};
    struct ra_rs_dev_info rdev = {0};
    unsigned int rdev_index = 0;
    int ret;

    mocker_invoke(rs_get_rs_cb, rs_get_rs_cb_urma_stub, 20);
    mocker(rsGetLocalDevIDByHostDevID, 20, 0);
    mocker(rs_setup_sharemem, 20, 0);
    mocker(rs_epoll_ctl, 20, 0);
    mocker(dl_hal_buff_alloc_align_ex, 20, 0);
    attr.protocol = PROTOCOL_UDMA;
    ret = rs_ping_init(&attr, &info, &rdev_index);
    EXPECT_INT_EQ(ret, 0);
    EXPECT_INT_EQ(info.result.header_size, RS_PING_PAYLOAD_HEADER_RESV_CUSTOM);
    mocker_clean();

    g_tmp_rs_cb.ping_cb.init_cnt = 1;
    RS_INIT_LIST_HEAD(&g_tmp_rs_cb.ping_cb.ping_list);
    RS_INIT_LIST_HEAD(&g_tmp_rs_cb.ping_cb.pong_list);
    target.payload.size = 1;
    mocker_invoke(rs_get_ping_cb, rs_get_ping_cb_urma_stub1, 10);
    mocker_invoke(rs_get_rs_cb, rs_get_rs_cb_urma_stub, 20);
    ret = rs_ping_target_add(&rdev, &target);
    EXPECT_INT_EQ(ret, 0);
    mocker(rs_epoll_ctl, 20, 0);
    ret = rs_ping_deinit(&rdev);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

void tc_rs_ping_target_add_del_urma()
{
    struct ping_target_info target = {0};
    struct ra_rs_dev_info rdev = {0};
    unsigned int num = 1;
    int ret;

    g_tmp_ping_cb.task_status = RS_PING_TASK_RESET;
    RS_INIT_LIST_HEAD(&g_tmp_ping_cb.ping_list);
    mocker_invoke(rs_get_ping_cb, rs_get_ping_cb_urma_stub, 2);
    ret = rs_ping_target_add(&rdev, &target);
    EXPECT_INT_EQ(ret, 0);
    ret = rs_ping_target_del(&rdev, &target, &num);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    target.payload.size = 1;
    g_tmp_ping_cb.task_status = RS_PING_TASK_RESET;
    mocker_invoke(rs_get_ping_cb, rs_get_ping_cb_urma_stub, 1);
    mocker(rs_ping_common_import_jetty, 1, -1);
    ret = rs_ping_target_add(&rdev, &target);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

    mocker_invoke(rs_get_ping_cb, rs_get_ping_cb_urma_stub, 1);
    mocker(calloc, 10, 0);
    ret = rs_ping_target_add(&rdev, &target);
    EXPECT_INT_EQ(ret, -12);
    mocker_clean();

    g_tmp_ping_cb.task_status = RS_PING_TASK_RESET;
    mocker_invoke(rs_get_ping_cb, rs_get_ping_cb_urma_stub, 1);
    mocker(rs_ping_urma_find_target_node, 1, -1);
    ret = rs_ping_target_del(&rdev, &target, &num);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();
}

void tc_rs_ping_urma_post_send()
{
    struct rs_ping_target_info target = {0};
    struct rs_ping_ctx_cb ping_cb = {0};
    urma_jetty_t server_jetty = {0};
    urma_jetty_t client_jetty = {0};
    void *addr = malloc(256);
    urma_sge_t sge = {0};
    int ret;

    target.payload_buffer = malloc(1);
    target.payload_size = 1;
    sge.addr = (uintptr_t)addr;
    sge.len = 256;
    ping_cb.ping_jetty.send_seg_cb.sge_num = 1;
    ping_cb.ping_jetty.send_seg_cb.sge_list = &sge;
    ping_cb.pong_jetty.jetty = &server_jetty;
    ping_cb.ping_jetty.jetty = &client_jetty;
    ret = rs_ping_urma_post_send(&ping_cb, &target);
    EXPECT_INT_EQ(ret, 0);
    free(addr);
    addr = NULL;
    free(target.payload_buffer);
    target.payload_buffer = NULL;
}

void tc_rs_ping_client_poll_cq_urma()
{
    struct rs_ping_ctx_cb ping_cb = {0};
    struct timeval timestamp2;
    int polled_cnt;
    int ret;

    mocker_clean();
    ret = rs_ping_urma_poll_rcq(&ping_cb, &polled_cnt, &timestamp2);
    EXPECT_INT_EQ(ret, -11);
    mocker(rs_urma_wait_jfc, 1, -1);
    ret = rs_ping_urma_poll_rcq(&ping_cb, &polled_cnt, &timestamp2);
    EXPECT_INT_EQ(ret, -259);
    mocker_clean();

    mocker(rs_urma_wait_jfc, 1, 1);
    mocker(rs_urma_ack_jfc, 1, 0);
    ret = rs_ping_urma_poll_rcq(&ping_cb, &polled_cnt, &timestamp2);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    ping_cb.ping_jetty.recv_jfc.jfc = &g_tmp_jfc;
    mocker_invoke(rs_urma_wait_jfc, rs_urma_wait_jfc_stub, 1);
    mocker(rs_urma_poll_jfc, 1, -1);
    ret = rs_ping_urma_poll_rcq(&ping_cb, &polled_cnt, &timestamp2);
    EXPECT_INT_EQ(ret, -259);
    mocker_clean();

    ping_cb.ping_jetty.recv_jfc.max_recv_wc_num = 16;
    mocker_invoke(rs_urma_wait_jfc, rs_urma_wait_jfc_stub, 1);
    mocker(rs_urma_poll_jfc, 1, 1);
    mocker(rs_pong_jetty_post_send, 1, -1);
    ret = rs_ping_urma_poll_rcq(&ping_cb, &polled_cnt, &timestamp2);
    EXPECT_INT_EQ(ret, 0);
    rs_pong_urma_handle_send(&ping_cb, polled_cnt, &timestamp2);
    mocker_clean();

    mocker_invoke(rs_urma_wait_jfc, rs_urma_wait_jfc_stub, 1);
    mocker(rs_urma_poll_jfc, 1, 1);
    mocker(rs_pong_jetty_post_send, 1, 0);
    mocker(rs_ping_common_jfr_post_recv, 1, -1);
    ret = rs_ping_urma_poll_rcq(&ping_cb, &polled_cnt, &timestamp2);
    EXPECT_INT_EQ(ret, 0);
    rs_pong_urma_handle_send(&ping_cb, polled_cnt, &timestamp2);
    mocker_clean();

    mocker_invoke(rs_urma_wait_jfc, rs_urma_wait_jfc_stub, 1);
    mocker(rs_urma_poll_jfc, 1, 1);
    mocker(rs_pong_jetty_post_send, 1, 0);
    mocker(rs_ping_common_jfr_post_recv, 1, -1);
    mocker(rs_urma_rearm_jfc, 1, -1);
    ret = rs_ping_urma_poll_rcq(&ping_cb, &polled_cnt, &timestamp2);
    EXPECT_INT_EQ(ret, 0);
    rs_pong_urma_handle_send(&ping_cb, polled_cnt, &timestamp2);
    mocker_clean();
}

void tc_rs_ping_urma_poll_scq()
{
    struct rs_ping_target_info target = {0};
    struct rs_ping_ctx_cb ping_cb = {0};
    int ret;

    ret = rs_ping_urma_poll_scq(&ping_cb, &target);
    EXPECT_INT_EQ(ret, -61);

    mocker_invoke(rs_urma_poll_jfc, rs_urma_poll_jfc_stub_ping0, 10);
    ret = rs_ping_urma_poll_scq(&ping_cb, &target);
    EXPECT_INT_EQ(ret, -259);
    mocker_clean();

    mocker_invoke(rs_urma_poll_jfc, rs_urma_poll_jfc_stub_ping1, 10);
    ret = rs_ping_urma_poll_scq(&ping_cb, &target);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

void tc_rs_ping_server_poll_cq_urma()
{
    struct rs_ping_ctx_cb ping_cb = {0};

    mocker(rs_urma_wait_jfc, 1, -1);
    rs_pong_urma_poll_rcq(&ping_cb);
    mocker_clean();

    mocker(rs_urma_wait_jfc, 1, 0);
    rs_pong_urma_poll_rcq(&ping_cb);
    mocker_clean();

    ping_cb.pong_jetty.recv_jfc.jfc = &g_tmp_jfc;
    ping_cb.pong_jetty.recv_jfc.max_recv_wc_num = 16;
    mocker_invoke(rs_urma_wait_jfc, rs_urma_wait_jfc_stub, 1);
    mocker(rs_urma_poll_jfc, 1, -1);
    rs_pong_urma_poll_rcq(&ping_cb);
    mocker_clean();

    mocker_invoke(rs_urma_wait_jfc, rs_urma_wait_jfc_stub, 1);
    mocker(rs_urma_poll_jfc, 1, 1);
    mocker(rs_pong_jetty_resolve_response_packet, 1, -1);
    rs_pong_urma_poll_rcq(&ping_cb);
    mocker_clean();

    mocker_invoke(rs_urma_wait_jfc, rs_urma_wait_jfc_stub, 1);
    mocker(rs_urma_poll_jfc, 1, 1);
    mocker(rs_pong_jetty_resolve_response_packet, 1, 0);
    mocker(rs_ping_common_jfr_post_recv, 1, -1);
    rs_pong_urma_poll_rcq(&ping_cb);
    mocker_clean();

    mocker_invoke(rs_urma_wait_jfc, rs_urma_wait_jfc_stub, 1);
    mocker(rs_urma_poll_jfc, 1, 1);
    mocker(rs_pong_jetty_resolve_response_packet, 1, 0);
    mocker(rs_ping_common_jfr_post_recv, 1, 0);
    mocker(rs_urma_rearm_jfc, 1, -1);
    rs_pong_urma_poll_rcq(&ping_cb);
    mocker_clean();
}

void tc_rs_epoll_event_ping_handle_urma()
{
    urma_jfce_t ping_jfce = {0};
    urma_jfce_t pong_jfce = {0};
    struct rs_cb rscb = {0};
    int ret;

    pong_jfce.fd = 1;
    rscb.ping_cb.init_cnt = 1;
    rscb.ping_cb.ping_jetty.jfce = &ping_jfce;
    rscb.ping_cb.pong_jetty.jfce = &pong_jfce;
    rscb.ping_cb.thread_status = RS_PING_THREAD_RUNNING;
    rscb.ping_cb.ping_pong_ops = rs_ping_urma_get_ops();
    rscb.ping_cb.ping_pong_dfx = rs_ping_urma_get_dfx();

    mocker(rs_ping_urma_poll_rcq, 10, 0);
    mocker(rs_pong_urma_handle_send, 10, 0);
    mocker(rs_pong_urma_poll_rcq, 10, 0);

    ret = rs_epoll_event_ping_handle(&rscb, 0);
    EXPECT_INT_EQ(ret, 0);
    ret = rs_epoll_event_ping_handle(&rscb, 1);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
    return;
}

void tc_rs_ping_get_results_urma()
{
    struct ping_target_comm_info target = {0};
    struct ping_result_info result = {0};
    struct ra_rs_dev_info rdev = {0};
    unsigned int num = 1;
    int ret;

    g_tmp_ping_cb.task_status = RS_PING_TASK_RESET;
    mocker_invoke(rs_get_ping_cb, rs_get_ping_cb_urma_stub, 1);
    mocker_invoke(rs_ping_urma_find_target_node, rs_ping_urma_find_target_node_stub, 1);
    ret = rs_ping_get_results(&rdev, &target, &num, &result);
    EXPECT_INT_EQ(ret, -ENODEV);
    mocker_clean();

    num = 1;
    g_tmp_ping_cb.task_status = RS_PING_TASK_RESET;
    mocker_invoke(rs_get_ping_cb, rs_get_ping_cb_urma_stub, 1);
    mocker_invoke(rs_ping_urma_find_target_node, rs_ping_urma_find_target_node_stub1, 1);
    ret = rs_ping_get_results(&rdev, &target, &num, &result);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

void tc_rs_ping_server_post_send_urma()
{
    struct rs_ping_ctx_cb ping_cb = {0};
    struct timeval timestamp2;
    void *send_addr = malloc(TEST_SGE_LIST_LEN);
    void *recv_addr = malloc(TEST_SGE_LIST_LEN);
    urma_cr_t cr = {0};
    int ret;

    cr.user_ctx = 1;
    mocker(rs_ping_common_poll_send_jfc, 1, 0);
    ret = rs_pong_jetty_post_send(&ping_cb, &cr, &timestamp2);
    EXPECT_INT_EQ(ret, -EIO);
    mocker_clean();

    cr.user_ctx = 0;
    cr.completion_len = 16;
    ping_cb.ping_jetty.recv_seg_cb.sge_list = calloc(1, sizeof(urma_sge_t));
    ping_cb.pong_jetty.send_seg_cb.sge_list = calloc(1, sizeof(urma_sge_t));
    ping_cb.pong_jetty.send_seg_cb.sge_num = 1;
    ping_cb.pong_jetty.send_seg_cb.sge_list->addr = (uintptr_t)send_addr;
    ping_cb.pong_jetty.send_seg_cb.sge_list->len = TEST_SGE_LIST_LEN;
    ping_cb.ping_jetty.recv_seg_cb.sge_list->addr = (uintptr_t)recv_addr;
    ping_cb.ping_jetty.recv_seg_cb.sge_list->len = TEST_SGE_LIST_LEN;
    mocker(rs_ping_common_poll_send_jfc, 1, 0);
    mocker(rs_pong_jetty_find_alloc_target_node, 1, -1);
    ret = rs_pong_jetty_post_send(&ping_cb, &cr, &timestamp2);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

    mocker(rs_ping_common_poll_send_jfc, 1, 0);
    mocker_invoke(rs_pong_jetty_find_alloc_target_node, rs_pong_jetty_find_alloc_target_node_stub, 1);
    mocker(gettimeofday, 20, 1);
    mocker(rs_urma_post_jetty_send_wr, 1, -1);
    ret = rs_pong_jetty_post_send(&ping_cb, &cr, &timestamp2);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

    mocker(rs_ping_common_poll_send_jfc, 1, 0);
    mocker_invoke(rs_pong_jetty_find_alloc_target_node, rs_pong_jetty_find_alloc_target_node_stub, 1);
    ret = rs_pong_jetty_post_send(&ping_cb, &cr, &timestamp2);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    free(ping_cb.pong_jetty.send_seg_cb.sge_list);
    ping_cb.pong_jetty.send_seg_cb.sge_list = NULL;
    free(ping_cb.ping_jetty.recv_seg_cb.sge_list);
    ping_cb.ping_jetty.recv_seg_cb.sge_list = NULL;
    free(recv_addr);
    recv_addr = NULL;
    free(send_addr);
    send_addr = NULL;
}

void tc_rs_pong_jetty_find_alloc_target_node()
{
    struct rs_pong_target_info *node = NULL;
    struct rs_ping_ctx_cb ping_cb = {0};
    struct ping_qp_info target = {0};
    int ret;

    mocker_invoke(rs_pong_jetty_find_target_node, rs_pong_jetty_find_target_node_stub2, 1);
    mocker(rs_ping_common_import_jetty, 1, -1);
    ret = rs_pong_jetty_find_alloc_target_node(&ping_cb, &target, &node);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

    pthread_mutex_init(&ping_cb.pong_mutex, NULL);
    RS_INIT_LIST_HEAD(&ping_cb.pong_list);
    mocker_invoke(rs_pong_jetty_find_target_node, rs_pong_jetty_find_target_node_stub, 1);
    mocker(rs_ping_common_import_jetty, 1, 0);
    ret = rs_pong_jetty_find_alloc_target_node(&ping_cb, &target, &node);
    free(node);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

void tc_rs_ping_common_poll_send_jfc()
{
    struct rs_ping_local_qp_cb qp_cb = {0};
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
    struct rs_pong_target_info stub_node = {0};
    struct rs_pong_target_info *node = NULL;
    struct rs_ping_ctx_cb ping_cb = {0};
    struct ping_qp_info target = {0};
    int ret;

    RS_INIT_LIST_HEAD(&ping_cb.pong_list);
    ret = rs_pong_jetty_find_target_node(&ping_cb, &target, &node);
    EXPECT_INT_EQ(ret, -ENODEV);

    rs_list_add_tail(&stub_node.list, &ping_cb.pong_list);
    ret = rs_pong_jetty_find_target_node(&ping_cb, &target, &node);
    EXPECT_INT_EQ(ret, 0);
}

void tc_rs_pong_jetty_resolve_response_packet()
{
    struct rs_ping_ctx_cb ping_cb = {0};
    struct timeval timestamp4 = {0};
    void *recv_addr = calloc(1, TEST_SGE_LIST_LEN);
    uint32_t sge_idx = 0;
    int ret;

    ping_cb.pong_jetty.recv_seg_cb.sge_list = calloc(1, sizeof(urma_sge_t));
    ping_cb.pong_jetty.recv_seg_cb.sge_list->addr = (uintptr_t)recv_addr;
    ping_cb.pong_jetty.recv_seg_cb.sge_list->len = TEST_SGE_LIST_LEN;
    ping_cb.task_id = 1;

    ret = rs_pong_jetty_resolve_response_packet(&ping_cb, sge_idx, &timestamp4);
    EXPECT_INT_EQ(ret, 0);

    ping_cb.task_id = 0;
    mocker_invoke(rs_ping_urma_find_target_node, rs_ping_urma_find_target_node_stub, 1);
    ret = rs_pong_jetty_resolve_response_packet(&ping_cb, sge_idx, &timestamp4);
    EXPECT_INT_EQ(ret, -ENODEV);
    mocker_clean();

    g_tmp_target1.result_summary.rtt_max = 10;
    g_tmp_target1.result_summary.rtt_min = 4;
    pthread_mutex_init(&g_tmp_target1.trip_mutex, NULL);
    mocker(rs_ping_get_trip_time, 1, 11);
    mocker_invoke(rs_ping_urma_find_target_node, rs_ping_urma_find_target_node_stub1, 1);
    ret = rs_pong_jetty_resolve_response_packet(&ping_cb, sge_idx, &timestamp4);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    g_tmp_target1.result_summary.rtt_max = 10;
    g_tmp_target1.result_summary.rtt_min = 4;
    g_tmp_target1.result_summary.task_attr.timeout_interval = 12;
    pthread_mutex_init(&g_tmp_target1.trip_mutex, NULL);
    mocker(rs_ping_get_trip_time, 1, 11);
    mocker_invoke(rs_ping_urma_find_target_node, rs_ping_urma_find_target_node_stub1, 1);
    ret = rs_pong_jetty_resolve_response_packet(&ping_cb, sge_idx, &timestamp4);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    pthread_mutex_destroy(&g_tmp_target1.trip_mutex);
    free(ping_cb.pong_jetty.recv_seg_cb.sge_list);
    ping_cb.pong_jetty.recv_seg_cb.sge_list = NULL;
    free(recv_addr);
    recv_addr = NULL;
}

void tc_rs_ping_common_import_jetty()
{
    urma_target_jetty_t *import_tjetty = NULL;
 
    struct ping_qp_info target = {0};
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
    struct rs_ping_ctx_cb ping_cb = {0};

    rs_ping_urma_reset_recv_buffer(&ping_cb);
}


void tc_rs_ping_common_jfr_post_recv()
{
    struct rs_ping_local_jetty_cb jetty_cb = {0};
    urma_sge_t sge = {0};
    int ret;

    jetty_cb.recv_seg_cb.sge_num = 1;
    jetty_cb.recv_seg_cb.sge_list = &sge;
    ret = rs_ping_common_jfr_post_recv(&jetty_cb);
    EXPECT_INT_EQ(ret, 0);
}

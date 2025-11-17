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

extern int rs_ping_roce_ping_cb_init(unsigned int phy_id, struct ping_init_attr *attr, struct ping_init_info *info,
    struct rs_ping_ctx_cb *ping_cb);
extern int rs_get_ping_cb(struct ra_rs_dev_info *rdev, struct rs_ping_ctx_cb **ping_cb);
extern int rs_ping_roce_find_target_node(struct rs_ping_ctx_cb *ping_cb, struct ping_qp_info *target,
    struct rs_ping_target_info **node);
extern int rs_ping_roce_alloc_target_node(struct rs_ping_ctx_cb *ping_cb, struct ping_target_info *target,
    struct rs_ping_target_info **node);
extern int rs_ping_roce_get_target_result(struct rs_ping_ctx_cb *ping_cb, struct ping_target_comm_info *target,
    struct ping_result_info *result);
extern void rs_ping_common_deinit_local_qp(struct rs_cb *rscb, struct rs_ping_ctx_cb *ping_cb,
    struct rs_ping_local_qp_cb *qp_cb);
extern int rs_ping_cb_get_ib_ctx_and_index(struct rdev *rdev_info, struct rs_ping_ctx_cb *ping_cb);
extern int rs_ping_cb_get_dev_rdev_index(struct rs_ping_ctx_cb *ping_cb, int index);

static struct rs_cb g_tmp_rs_cb0;
static struct rs_cb g_tmp_rs_cb1;
static struct rs_cb g_ping_rs_cb;
static struct rs_ping_ctx_cb g_tmp_ping_cb;
static struct rs_ping_target_info g_ping_target_node;
static struct rs_ping_target_info g_ping_target_node1;

int rs_dev2rscb_stub0(uint32_t dev_id, struct rs_cb **rs_cb, bool init_flag)
{
    *rs_cb = &g_tmp_rs_cb0;

    return 0;
}

int rs_dev2rscb_stub1(uint32_t dev_id, struct rs_cb **rs_cb, bool init_flag)
{
    pthread_mutex_init(&g_tmp_rs_cb1.ping_cb.ping_mutex, NULL);
    g_tmp_rs_cb1.ping_cb.thread_status = RS_PING_THREAD_RUNNING;

    *rs_cb = &g_tmp_rs_cb1;

    return 0;
}

int rs_get_ping_cb_stub(struct ra_rs_dev_info *rdev, struct rs_ping_ctx_cb **ping_cb)
{
    *ping_cb = &g_tmp_ping_cb;
    (*ping_cb)->ping_pong_ops = rs_ping_roce_get_ops();
    (*ping_cb)->ping_pong_dfx = rs_ping_roce_get_dfx();

    return 0;
}

int rs_get_rs_cb_stub(unsigned int phy_id, struct rs_cb **rs_cb)
{
    *rs_cb = &g_ping_rs_cb;
    (*rs_cb)->ping_cb.ping_pong_ops = rs_ping_roce_get_ops();
    (*rs_cb)->ping_cb.ping_pong_dfx = rs_ping_roce_get_dfx();

    return 0;
}

int rs_ping_roce_find_target_node_stub(struct rs_ping_ctx_cb *ping_cb, struct ping_qp_info *target,
    struct rs_ping_target_info **node)
{
    *node = &g_ping_target_node;

    return 0;
}

int rs_ping_roce_find_target_node_stub1(struct rs_ping_ctx_cb *ping_cb, struct ping_qp_info *target,
    struct rs_ping_target_info **node)
{
    *node = &g_ping_target_node1;

    return -ENODEV;
}

void tc_rs_ping_handle_init()
{
    unsigned int chip_id;
    int hdc_type;
    int ret;

    chip_id = 0;
    hdc_type = HDC_SERVICE_TYPE_RDMA;
    ret = rs_ping_handle_init(chip_id, hdc_type, WHITE_LIST_ENABLE);
    EXPECT_INT_EQ(ret, 0);

    hdc_type = HDC_SERVICE_TYPE_RDMA_V2;
    mocker(rs_dev2rscb, 1, -1);
    ret = rs_ping_handle_init(chip_id, hdc_type, WHITE_LIST_ENABLE);
    EXPECT_INT_EQ(ret, -ENODEV);
    mocker_clean();

    mocker_invoke(rs_dev2rscb, rs_dev2rscb_stub0, 1);
    mocker((stub_fn_t)pthread_create, 1, -1);
    ret = rs_ping_handle_init(chip_id, hdc_type, WHITE_LIST_ENABLE);
    EXPECT_INT_EQ(ret, -ESYSFUNC);
    mocker_clean();
}

void tc_rs_ping_handle_deinit()
{
    unsigned int chip_id;
    int ret;

    chip_id = 0;
    mocker(rs_dev2rscb, 1, -1);
    ret = rs_ping_handle_deinit(chip_id);
    EXPECT_INT_EQ(ret, -ENODEV);
    mocker_clean();

    mocker_invoke(rs_dev2rscb, rs_dev2rscb_stub0, 1);
    ret = rs_ping_handle_deinit(chip_id);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    mocker_invoke(rs_dev2rscb, rs_dev2rscb_stub1, 1);
    ret = rs_ping_handle_deinit(chip_id);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    pthread_mutex_destroy(&g_tmp_rs_cb1.ping_cb.ping_mutex);
}

void tc_rs_ping_init()
{
    struct ping_init_attr attr = { 0 };
    struct ping_init_info info = { 0 };
    unsigned int rdev_index = 0;
    int ret;

    ret = rs_ping_init(&attr, &info, NULL);
    EXPECT_INT_EQ(ret, -EINVAL);

    mocker(rs_get_rs_cb, 1, -1);
    ret = rs_ping_init(&attr, &info, &rdev_index);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

    mocker(rsGetLocalDevIDByHostDevID, 1, 0);
    mocker_invoke(rs_dev2rscb, rs_dev2rscb_stub0, 1);
    g_tmp_rs_cb0.ping_cb.init_cnt = 1;
    ret = rs_ping_init(&attr, &info, &rdev_index);
    EXPECT_INT_NE(ret, 0);
    mocker_clean();

    mocker_invoke(rs_get_rs_cb, rs_get_rs_cb_stub, 1);
    mocker(rsGetLocalDevIDByHostDevID, 1, -1);
    ret = rs_ping_init(&attr, &info, &rdev_index);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

    mocker_invoke(rs_get_rs_cb, rs_get_rs_cb_stub, 1);
    mocker(rsGetLocalDevIDByHostDevID, 1, 0);
    mocker(rs_setup_sharemem, 20, -1);
    ret = rs_ping_init(&attr, &info, &rdev_index);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

    mocker_invoke(rs_get_rs_cb, rs_get_rs_cb_stub, 1);
    mocker(rsGetLocalDevIDByHostDevID, 1, 0);
    mocker(rs_setup_sharemem, 20, 0);
    mocker(rs_ping_roce_ping_cb_init, 1, -1);
    ret = rs_ping_init(&attr, &info, &rdev_index);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();
}

void tc_rs_ping_target_add()
{
    struct ping_target_info target = { 0 };
    struct ra_rs_dev_info rdev = { 0 };
    int ret;

    ret = rs_ping_target_add(&rdev, NULL);
    EXPECT_INT_EQ(ret, -EINVAL);

    target.payload.size = PING_USER_PAYLOAD_MAX_SIZE + 1;
    ret = rs_ping_target_add(&rdev, &target);
    EXPECT_INT_EQ(ret, -EINVAL);

    target.payload.size = PING_USER_PAYLOAD_MAX_SIZE;
    mocker(rs_get_ping_cb, 1, -1);
    ret = rs_ping_target_add(&rdev, &target);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

    g_tmp_ping_cb.task_status = RS_PING_TASK_RUNNING;
    mocker_invoke(rs_get_ping_cb, rs_get_ping_cb_stub, 1);
    ret = rs_ping_target_add(&rdev, &target);
    EXPECT_INT_NE(ret, 0);
    mocker_clean();

    g_tmp_ping_cb.task_status = RS_PING_TASK_RESET;
    mocker_invoke(rs_get_ping_cb, rs_get_ping_cb_stub, 1);
    mocker_invoke(rs_ping_roce_find_target_node, rs_ping_roce_find_target_node_stub, 1);
    ret = rs_ping_target_add(&rdev, &target);
    EXPECT_INT_NE(ret, 0);
    mocker_clean();

    mocker_invoke(rs_get_ping_cb, rs_get_ping_cb_stub, 1);
    mocker_invoke(rs_ping_roce_find_target_node, rs_ping_roce_find_target_node_stub1, 1);
    mocker(rs_ping_roce_alloc_target_node, 1, -1);
    ret = rs_ping_target_add(&rdev, &target);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();
}

void tc_rs_ping_task_start()
{
    struct rs_ping_target_info tmp_target = {0};
    struct ra_rs_dev_info rdev = { 0 };
    struct ping_task_attr attr = { 0 };
    int ret;

    ret = rs_ping_task_start(&rdev, NULL);
    EXPECT_INT_EQ(ret, -EINVAL);

    mocker(rs_get_ping_cb, 1, -1);
    ret = rs_ping_task_start(&rdev, &attr);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

    g_tmp_ping_cb.task_status = RS_PING_TASK_RUNNING;
    mocker_invoke(rs_get_ping_cb, rs_get_ping_cb_stub, 1);
    ret = rs_ping_task_start(&rdev, &attr);
    EXPECT_INT_NE(ret, 0);
    mocker_clean();

    g_tmp_ping_cb.task_status = RS_PING_TASK_RESET;
    attr.packet_cnt = 1;
    attr.packet_interval = 1;
    attr.timeout_interval = 1;
    pthread_mutex_init(&g_tmp_ping_cb.pong_qp.recv_mr_cb.mutex, NULL);
    pthread_mutex_init(&g_tmp_ping_cb.ping_qp.recv_mr_cb.mutex, NULL);
    pthread_mutex_init(&g_tmp_ping_cb.ping_mutex, NULL);
    RS_INIT_LIST_HEAD(&g_tmp_ping_cb.ping_list);
    rs_list_add_tail(&tmp_target.list, &g_tmp_ping_cb.ping_list);
    mocker_invoke(rs_get_ping_cb, rs_get_ping_cb_stub, 1);
    ret = rs_ping_task_start(&rdev, &attr);
    EXPECT_INT_EQ(ret, 0);
    rs_list_del(&tmp_target.list);
    mocker_clean();
    pthread_mutex_destroy(&g_tmp_ping_cb.ping_mutex);
    pthread_mutex_destroy(&g_tmp_ping_cb.ping_qp.recv_mr_cb.mutex);
    pthread_mutex_destroy(&g_tmp_ping_cb.pong_qp.recv_mr_cb.mutex);
}

void tc_rs_ping_get_results()
{
    struct ping_target_comm_info target= { 0 };
    struct ping_result_info result = { 0 };
    struct ra_rs_dev_info rdev = { 0 };
    unsigned int num = 1;
    int ret;

    ret = rs_ping_get_results(NULL, &target, &num, &result);
    EXPECT_INT_EQ(ret, -EINVAL);

    num = 1;
    mocker(rs_get_ping_cb, 1, -1);
    ret = rs_ping_get_results(&rdev, &target, &num, &result);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

    num = 1;
    g_tmp_ping_cb.task_status = RS_PING_TASK_RUNNING;
    mocker_invoke(rs_get_ping_cb, rs_get_ping_cb_stub, 1);
    ret = rs_ping_get_results(&rdev, &target, &num, &result);
    EXPECT_INT_NE(ret, 0);
    mocker_clean();

    num = 1;
    g_tmp_ping_cb.task_status = RS_PING_TASK_RESET;
    mocker_invoke(rs_get_ping_cb, rs_get_ping_cb_stub, 1);
    mocker(rs_ping_roce_get_target_result, 1, -1);
    ret = rs_ping_get_results(&rdev, &target, &num, &result);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();
}

void tc_rs_ping_task_stop()
{
    struct ra_rs_dev_info rdev = { 0 };
    int ret;

    ret = rs_ping_task_stop(NULL);
    EXPECT_INT_EQ(ret, -EINVAL);

    mocker(rs_get_ping_cb, 1, -1);
    ret = rs_ping_task_stop(&rdev);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();
}

void tc_rs_ping_target_del()
{
    struct ping_target_comm_info target = { 0 };
    struct ra_rs_dev_info rdev = { 0 };
    unsigned int num = 1;
    int ret;

    ret = rs_ping_target_del(&rdev, &target, NULL);
    EXPECT_INT_EQ(ret, -EINVAL);

    mocker(rs_get_ping_cb, 1, -1);
    ret = rs_ping_target_del(&rdev, &target, &num);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

    g_tmp_ping_cb.task_status = RS_PING_TASK_RUNNING;
    mocker_invoke(rs_get_ping_cb, rs_get_ping_cb_stub, 1);
    ret = rs_ping_target_del(&rdev, &target, &num);
    EXPECT_INT_NE(ret, 0);
    mocker_clean();

    g_tmp_ping_cb.task_status = RS_PING_TASK_RESET;
    mocker_invoke(rs_get_ping_cb, rs_get_ping_cb_stub, 1);
    mocker(rs_ping_roce_find_target_node, 1, -1);
    ret = rs_ping_target_del(&rdev, &target, &num);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();
}

void tc_rs_ping_deinit()
{
    struct ra_rs_dev_info rdev = { 0 };
    int ret;

    ret = rs_ping_deinit(NULL);
    EXPECT_INT_EQ(ret, -EINVAL);

    mocker(rs_get_ping_cb, 1, -1);
    ret = rs_ping_deinit(&rdev);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

    g_tmp_ping_cb.init_cnt = 0;
    mocker_invoke(rs_get_ping_cb, rs_get_ping_cb_stub, 1);
    ret = rs_ping_deinit(&rdev);
    EXPECT_INT_EQ(ret, -ENODEV);
    mocker_clean();
}

void tc_rs_ping_urma_check_fd()
{
    urma_jfce_t jf = {0};
    struct rs_ping_ctx_cb ping_cb;
    ping_cb.ping_jetty.jfce = &jf;
    ping_cb.ping_jetty.jfce->fd = 1;
    ping_cb.pong_jetty.jfce = &jf;
    ping_cb.pong_jetty.jfce->fd = 1;
    int ret;

    ret = rs_ping_urma_check_fd(&ping_cb, 1);
    EXPECT_INT_EQ(ret, 1);

    ret = rs_ping_urma_check_fd(&ping_cb, 0);
    EXPECT_INT_EQ(ret, 0);

    ret = rs_pong_urma_check_fd(&ping_cb, 1);
    EXPECT_INT_EQ(ret, 1);

    ret = rs_pong_urma_check_fd(&ping_cb, 0);
    EXPECT_INT_EQ(ret, 0);
}

void tc_rs_ping_cb_get_ib_ctx_and_index()
{
    struct rs_ping_ctx_cb ping_cb = {0};
    struct ibv_device *dev_list[1] = {0};
    struct ibv_device dev_node = {0};
    struct rdev rdev_info = {0};
    int ret = 0;

    ping_cb.rdev_cb.dev_num = 1;
    ping_cb.rdev_cb.dev_list = &dev_list;
    dev_list[0] = &dev_node;
    mocker(rs_query_gid, 1, 0);
    mocker(rs_ping_cb_get_dev_rdev_index, 1, 0);
    mocker(rs_ibv_query_gid, 1, -1);
    ret = rs_ping_cb_get_ib_ctx_and_index(&rdev_info, &ping_cb);
    EXPECT_INT_EQ(ret, -EOPENSRC);
    mocker_clean();
}
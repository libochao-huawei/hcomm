/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: ra ping unit test.
 * Author:
 * Create: 2024-09-21
 */

#include <errno.h>
#include <stdlib.h>
#include <pthread.h>
#include "ut_dispatch.h"
#include "hccp_common.h"
#include "hccp_ping.h"
#include "ra.h"
#include "ra_ping.h"
#include "ra_hdc_ping.h"
#include "ra_hdc.h"
#include "ra_client_host.h"
#include "tc_ra_ping.h"

extern int ra_ping_init_get_handle(struct ping_init_attr *init_attr, struct ping_init_info *init_info,
    struct ra_ping_handle *ping_handle);
extern int ra_ping_deinit_para_check(struct ra_ping_handle *ping_handle);

void tc_ra_ping_init_get_handle_abnormal()
{
    struct ra_ping_handle ping_handle = { 0 };
    struct ping_init_attr init_attr = { 0 };
    struct ping_init_info init_info = { 0 };
    int ret;

    ret = ra_ping_init_get_handle(&init_attr, NULL, NULL);
    EXPECT_INT_EQ(ret, -EINVAL);

    init_attr.buffer_size = 1;
    ret = ra_ping_init_get_handle(&init_attr, &init_info, NULL);
    EXPECT_INT_EQ(ret, -EINVAL);

    init_attr.buffer_size = 0;
    init_attr.mode = NETWORK_PEER_ONLINE;
    ret = ra_ping_init_get_handle(&init_attr, &init_info, &ping_handle);
    EXPECT_INT_EQ(ret, -EINVAL);

    init_attr.mode = NETWORK_OFFLINE;
    mocker(ra_rdev_init_check, 1, -1);
    ret = ra_ping_init_get_handle(&init_attr, &init_info, &ping_handle);
    EXPECT_INT_EQ(ret, -22);
    mocker_clean();

    mocker(ra_rdev_init_check, 1, 0);
    mocker((stub_fn_t)pthread_mutex_init, 1, -1);
    ret = ra_ping_init_get_handle(&init_attr, &init_info, &ping_handle);
    EXPECT_INT_EQ(ret, -22);
    mocker_clean();

    init_attr.buffer_size = RA_RS_PING_BUFFER_ALIGN_4K_PAGE_SIZE;
    init_attr.comm_info.rdma.udp_sport = 65536;
    ret = ra_ping_init_get_handle(&init_attr, &init_info, &ping_handle);
    EXPECT_INT_EQ(ret, -22);
    mocker_clean();
}

void tc_ra_ping_init_abnormal()
{
    void *ping_handle = NULL;
    int ret;

    ret = ra_ping_init(NULL, NULL, NULL);
    EXPECT_INT_EQ(ret, -EINVAL);

    mocker((stub_fn_t)calloc, 1, NULL);
    ret = ra_ping_init(NULL, NULL, &ping_handle);
    EXPECT_INT_NE(ret, 0);
    mocker_clean();

    mocker(ra_ping_init_get_handle, 1, -1);
    ret = ra_ping_init(NULL, NULL, &ping_handle);
    EXPECT_INT_NE(ret, 0);
    mocker_clean();
}

void tc_ra_ping_target_add_abnormal()
{
    struct ping_target_info target[1] = { 0 };
    struct ra_ping_handle ping_handle = { 0 };
    struct ra_ping_ops ops = { 0 };
    int num;
    int ret;

    num = 0;
    ret = ra_ping_target_add((void *)(&ping_handle), target, num);
    EXPECT_INT_NE(ret, 0);

    num = 1;
    ping_handle.ping_ops = &ops;
    ret = ra_ping_target_add((void *)(&ping_handle), target, num);
    EXPECT_INT_NE(ret, 0);

    ops.ra_ping_target_add = ra_hdc_ping_target_add;
    ping_handle.phy_id = RA_MAX_PHY_ID_NUM;
    ret = ra_ping_target_add((void *)(&ping_handle), target, num);
    EXPECT_INT_NE(ret, 0);

    ping_handle.phy_id = 0;
    ping_handle.task_cnt = 1;
    ret = ra_ping_target_add((void *)(&ping_handle), target, num);
    EXPECT_INT_NE(ret, 0);

    ping_handle.task_cnt = 0;
    mocker(ra_hdc_ping_target_add, 1, -1);
    ret = ra_ping_target_add((void *)(&ping_handle), target, num);
    EXPECT_INT_NE(ret, 0);
    mocker_clean();
}

void tc_ra_ping_task_start_abnormal()
{
    struct ra_ping_handle ping_handle = { 0 };
    struct ping_task_attr attr = { 0 };
    struct ra_ping_ops ops = { 0 };
    int ret;

    attr.packet_cnt = 1;
    attr.packet_interval = 1;
    attr.timeout_interval = 1;

    ret = ra_ping_task_start((void *)(&ping_handle), NULL);
    EXPECT_INT_NE(ret, 0);

    ping_handle.ping_ops = &ops;
    ret = ra_ping_task_start((void *)(&ping_handle), &attr);
    EXPECT_INT_NE(ret, 0);

    ops.ra_ping_task_start = ra_hdc_ping_task_start;
    ping_handle.phy_id = RA_MAX_PHY_ID_NUM;
    ret = ra_ping_task_start((void *)(&ping_handle), &attr);
    EXPECT_INT_NE(ret, 0);

    ping_handle.phy_id = 0;
    ping_handle.task_cnt = 1;
    ret = ra_ping_task_start((void *)(&ping_handle), &attr);
    EXPECT_INT_NE(ret, 0);

    ping_handle.task_cnt = 0;
    ping_handle.target_cnt = 0;
    ret = ra_ping_task_start((void *)(&ping_handle), &attr);
    EXPECT_INT_NE(ret, 0);

    ping_handle.target_cnt = 1;
    ping_handle.buffer_size = 0;
    ret = ra_ping_task_start((void *)(&ping_handle), &attr);
    EXPECT_INT_NE(ret, 0);

    ping_handle.buffer_size = ping_handle.target_cnt * attr.packet_cnt * PING_TOTAL_PAYLOAD_MAX_SIZE;
    mocker(ra_hdc_ping_task_start, 1, -1);
    ret = ra_ping_task_start((void *)(&ping_handle), &attr);
    EXPECT_INT_NE(ret, 0);
    mocker_clean();
}

void tc_ra_ping_get_results_abnormal()
{
    struct ping_target_result target[1] = { 0 };
    struct ra_ping_handle ping_handle = { 0 };
    struct ra_ping_ops ops = { 0 };
    int num = 0;
    int ret;

    ret = ra_ping_get_results((void *)(&ping_handle), target, NULL);
    EXPECT_INT_NE(ret, 0);

    num = 1;
    ping_handle.ping_ops = &ops;
    ret = ra_ping_get_results((void *)(&ping_handle), target, &num);
    EXPECT_INT_NE(ret, 0);

    ops.ra_ping_get_results = ra_hdc_ping_get_results;
    ping_handle.phy_id = RA_MAX_PHY_ID_NUM;
    ret = ra_ping_get_results((void *)(&ping_handle), target, &num);
    EXPECT_INT_NE(ret, 0);

    ping_handle.phy_id = 0;
    ping_handle.target_cnt = 0;
    ret = ra_ping_get_results((void *)(&ping_handle), target, &num);
    EXPECT_INT_NE(ret, 0);

    ping_handle.target_cnt = 1;
    mocker(ra_hdc_ping_get_results, 1, -1);
    ret = ra_ping_get_results((void *)(&ping_handle), target, &num);
    EXPECT_INT_NE(ret, 0);
    mocker_clean();
}

void tc_ra_ping_target_del_abnoraml()
{
    struct ping_target_result target[1] = { 0 };
    struct ra_ping_handle ping_handle = { 0 };
    struct ra_ping_ops ops = { 0 };
    int num = 0;
    int ret;

    ret = ra_ping_target_del((void *)(&ping_handle), target, num);
    EXPECT_INT_NE(ret, 0);

    num = 1;
    ping_handle.ping_ops = &ops;
    ret = ra_ping_target_del((void *)(&ping_handle), target, num);
    EXPECT_INT_NE(ret, 0);

    ops.ra_ping_target_del = ra_hdc_ping_target_del;
    ping_handle.task_cnt = 1;
    ret = ra_ping_target_del((void *)(&ping_handle), target, num);
    EXPECT_INT_NE(ret, 0);

    ping_handle.task_cnt = 0;
    ping_handle.target_cnt = 0;
    ret = ra_ping_target_del((void *)(&ping_handle), target, num);
    EXPECT_INT_NE(ret, 0);

    ping_handle.target_cnt = 1;
    ping_handle.phy_id = RA_MAX_PHY_ID_NUM;
    ret = ra_ping_target_del((void *)(&ping_handle), target, num);
    EXPECT_INT_NE(ret, 0);

    ping_handle.phy_id = 0;
    mocker(ra_hdc_ping_target_del, 1, -1);
    ret = ra_ping_target_del((void *)(&ping_handle), target, num);
    EXPECT_INT_NE(ret, 0);
    mocker_clean();
}

void tc_ra_ping_task_stop_abnormal()
{
    struct ra_ping_handle ping_handle = { 0 };
    struct ra_ping_ops ops = { 0 };
    int ret;

    ret = ra_ping_task_stop(NULL);
    EXPECT_INT_NE(ret, 0);

    ping_handle.ping_ops = &ops;
    ret = ra_ping_task_stop((void *)(&ping_handle));
    EXPECT_INT_NE(ret, 0);

    ops.ra_ping_task_stop = ra_hdc_ping_task_stop;
    ping_handle.phy_id = RA_MAX_PHY_ID_NUM;
    ret = ra_ping_task_stop((void *)(&ping_handle));
    EXPECT_INT_NE(ret, 0);

    ping_handle.phy_id = 0;
    ping_handle.task_cnt = 0;
    ret = ra_ping_task_stop((void *)(&ping_handle));
    EXPECT_INT_NE(ret, 0);

    ping_handle.task_cnt = 1;
    mocker(ra_hdc_ping_task_stop, 1, -1);
    ret = ra_ping_task_stop((void *)(&ping_handle));
    EXPECT_INT_NE(ret, 0);
    mocker_clean();
}

int ra_hdc_ping_deinit_stub(struct ra_ping_handle *ping_handle)
{
    return 0;
}

void tc_ra_ping_deinit_para_check_abnormal()
{
    struct ra_ping_handle ping_handle = { 0 };
    struct ra_ping_ops ops = { 0 };
    int ret;

    ping_handle.phy_id = RA_MAX_PHY_ID_NUM;
    ret = ra_ping_deinit_para_check(&ping_handle);
    EXPECT_INT_EQ(ret, -EINVAL);

    ping_handle.phy_id = 0;
    ping_handle.ping_ops = &ops;
    ping_handle.ping_ops->ra_ping_deinit = ra_hdc_ping_deinit_stub;
    mocker(ra_inet_pton, 1, -1);
    ret = ra_ping_deinit_para_check(&ping_handle);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

    mocker(ra_inet_pton, 1, 0);
    ping_handle.ping_ops->ra_ping_deinit = NULL;
    ret = ra_ping_deinit_para_check(&ping_handle);
    EXPECT_INT_EQ(ret, -EINVAL);
    mocker_clean();
}

void tc_ra_ping_deinit_abnoaml()
{
    struct ra_ping_handle *ping_handle = calloc(1, sizeof(struct ra_ping_handle));
    struct ra_ping_ops ops = { 0 };
    int ret;

    ret = ra_ping_deinit(NULL);
    EXPECT_INT_NE(ret, 0);

    mocker(ra_ping_deinit_para_check, 1, -1);
    ret = ra_ping_deinit((void *)ping_handle);
    EXPECT_INT_NE(ret, 0);
    mocker_clean();

    ops.ra_ping_deinit = ra_hdc_ping_deinit;
    ping_handle->ping_ops = &ops;
    mocker(ra_ping_deinit_para_check, 1, 0);
    mocker(ra_hdc_ping_deinit, 1, -1);
    ret = ra_ping_deinit((void *)ping_handle);
    EXPECT_INT_NE(ret, 0);
    mocker_clean();
}

static void init_attr_fill(struct ping_init_attr *init_attr)
{
    struct rdev rdev_info = { 0 };
    rdev_info.phy_id = 0;
    rdev_info.family = AF_INET;

    init_attr->mode = NETWORK_OFFLINE;
    init_attr->dev.rdma = rdev_info;
    init_attr->client.rdma.cq_attr.send_cq_depth = 128;
    init_attr->client.rdma.cq_attr.recv_cq_depth = 128;
    init_attr->client.rdma.qp_attr.cap.max_inline_data = 32;
    init_attr->client.rdma.qp_attr.cap.max_send_sge = 1;
    init_attr->client.rdma.qp_attr.cap.max_send_wr = 128;
    init_attr->client.rdma.qp_attr.cap.max_recv_sge = 1;
    init_attr->client.rdma.qp_attr.cap.max_recv_wr = 128;

    init_attr->server.rdma.cq_attr.send_cq_depth = 128;
    init_attr->server.rdma.cq_attr.recv_cq_depth = 128;
    init_attr->server.rdma.qp_attr.cap.max_inline_data = 32;
    init_attr->server.rdma.qp_attr.cap.max_send_sge = 1;
    init_attr->server.rdma.qp_attr.cap.max_send_wr = 128;
    init_attr->server.rdma.qp_attr.cap.max_recv_sge = 1;
    init_attr->server.rdma.qp_attr.cap.max_recv_wr = 128;
    init_attr->buffer_size = 8192;
}

void tc_ra_ping()
{
    struct ping_target_comm_info target_comm_client = {0};
    struct ping_target_result target_result_client = {0};
    struct ping_target_info target_info_client = {0};
    char payload_client[20] = "hello, client";
    struct ping_init_info init_info = { 0 };
    struct ping_init_attr init_attr = { 0 };
    struct ping_task_attr task_attr = {0};
    unsigned int target_result_num = 1;
    struct ra_ping_ops ops = { 0 };
    void *ping_handle = NULL;
    int ret;

    init_attr_fill(&init_attr);
    init_attr.buffer_size = 4;
    ret = ra_ping_init(&init_attr, &init_info, &ping_handle);
    EXPECT_INT_NE(ret, 0);
    init_attr.buffer_size = 8192;
    mocker(ra_rdev_init_check, 20, 0);
    mocker((stub_fn_t)ra_hdc_process_msg, 20, 0);
    ret = ra_ping_init(&init_attr, &init_info, &ping_handle);
    EXPECT_INT_EQ(ret, 0);

    // add target
    target_info_client.local_info.rdma.hop_limit = 64;
    target_info_client.local_info.rdma.qos_attr.tc = (33 & 0x3f) << 2;
    target_info_client.local_info.rdma.qos_attr.sl = 4;
    target_info_client.remote_info.qp_info = init_info.client;
    ret = ra_ping_target_add(ping_handle, &target_info_client, 1);
    EXPECT_INT_EQ(ret, 0);

    // start
    task_attr.packet_cnt = 1;
    task_attr.packet_interval = 10;
    task_attr.timeout_interval = 10;
    ret = ra_ping_task_start(ping_handle, &task_attr);
    EXPECT_INT_EQ(ret, 0);

    // get result
    target_result_client.remote_info = target_info_client.remote_info;
    mocker(ra_hdc_ping_get_results, 20, 0);
    ret = ra_ping_get_results(ping_handle, &target_result_client, &target_result_num);
    EXPECT_INT_EQ(ret, 0);

    // stop
    ret = ra_ping_task_stop(ping_handle);
    EXPECT_INT_EQ(ret, 0);

    // del target
    target_comm_client.ip = target_info_client.remote_info.ip;
    target_comm_client.qp_info = target_info_client.remote_info.qp_info;
    ret = ra_ping_target_del(ping_handle, &target_comm_client, 1);
    EXPECT_INT_EQ(ret, 0);

    ret = ra_ping_deinit(ping_handle);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}
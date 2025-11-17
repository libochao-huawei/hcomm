/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2020. All rights reserved.
 * Description: ra adpter unit test.
 * Author:
 * Create: 2020-07-21
 */
#define _GNU_SOURCE
#include "tc_adp.h"
#include <stdlib.h>
#include <sched.h>
#include <stdint.h>
#include "ut_dispatch.h"
#include "rs.h"
#include "rs_ping.h"
#include "rs_tlv.h"
#include "ra_adp.h"
#include "ra_adp_tlv.h"
#include "ra_adp_ctx.h"
#include "ra_hdc.h"
#include "ra_hdc_lite.h"
#include "ra_hdc_rdma_notify.h"
#include "ra_hdc_rdma.h"
#include "ra_hdc_socket.h"
#include "ra_hdc_tlv.h"
#include "ra_hdc_ctx.h"
#include "ra_hdc_async_ctx.h"
#include "errno.h"
#undef TOKEN_RATE
#define TOKEN_RATE
extern struct rs_ctx_ops g_ra_rs_ctx_ops;
extern int recv_handle_send_pkt(HDC_SESSION session, unsigned int *close_session);
static int counter = 0;
int stub_recv_handle_send_pkt_0(HDC_SESSION session, unsigned int *close_session)
{
    counter++;
    if (counter <= 1) {
        *close_session = 0;
        return 0;
    } else {
        *close_session = 1;
        return 1;
    }
}

int stub_recv_handle_send_pkt(HDC_SESSION session, unsigned int *close_session)
{
    *close_session = 1;
    return 0;
}

static char* g_test_msg[MAX_TEST_MESSAGE] = {0};
static int g_msg_count = 0;
static int g_current_msg_index = 0;
static int g_accept_times = 0;
static HDC_SESSION g_test_session;
static pid_t g_host_tgid = 0;

DLLEXPORT drvError_t stub_get_msg_buffer(struct drvHdcMsg *msg, int index, char **pBuf, int *pLen)
{
    usleep(10*1000);
    *pBuf = g_test_msg[g_current_msg_index++];
    struct msg_head* tmsg = (struct msg_head*)*pBuf;
    *pLen = tmsg->msg_data_len + sizeof(struct msg_head);
    return 0;
}

char* add_test_msg(unsigned int opcode, unsigned int msg_len)
{
#define MAX_HDC_DATA_SIZE (4096 - 256 + 64)
    char* temp = (char*)calloc(sizeof(char), sizeof(struct msg_head) + msg_len);
    struct msg_head* tmsg = (struct msg_head*)temp;
    int ret;

    tmsg->opcode = opcode;
    tmsg->msg_data_len = msg_len;
    // check msg len without these have v2 version opcode
    if (opcode != RA_RS_SEND_WRLIST_EXT && opcode != RA_RS_SEND_WRLIST &&
        opcode != RA_RS_WLIST_DEL && opcode != RA_RS_WLIST_ADD &&
        opcode != RA_RS_GET_VNIC_IP_INFOS_V1) {
        ret = (msg_len > MAX_HDC_DATA_SIZE) ? 1 : 0;
        if (ret != 0) {
            printf("%s: opcode:%u, msg_len:%u exceeds %u\n", __func__, opcode, msg_len, MAX_HDC_DATA_SIZE);
        }
        EXPECT_INT_EQ(ret, 0);
    }

    tmsg->host_tgid = g_host_tgid;
    g_test_msg[g_msg_count++] = temp;
    return temp;
}


DLLEXPORT drvError_t stub_accept_session(HDC_SERVER server, HDC_SESSION *session)
{
    while(g_accept_times > 0) {
        *session = &g_test_session;
        -- g_accept_times;
        return 0;
    }
    return -1;
}

void msg_clear()
{
    
    int i;
    for (i = 0; i < g_msg_count; ++i) {
        free(g_test_msg[i]);
        g_test_msg[i] = NULL;
    }
    g_msg_count = 0;
    g_current_msg_index = 0;
}

void tc_adp_env_init()
{
    mocker_clean();
    msg_clear();
    mocker((stub_fn_t)halHdcRecv, 10, 0);
    mocker((stub_fn_t)halHdcSend, 10, 0);
    mocker_invoke((stub_fn_t)drvHdcGetMsgBuffer, (stub_fn_t)stub_get_msg_buffer, 10);
    mocker_invoke((stub_fn_t)drvHdcSessionAccept, (stub_fn_t)stub_accept_session, 10);
    g_accept_times = 1;
}
void tc_common_test()
{    
    unsigned int devid = 0;
    add_test_msg(RA_RS_HDC_SESSION_CLOSE, sizeof(union op_hdc_close_data));
    int ret = hccp_init(devid, g_host_tgid, HDC_SERVICE_TYPE_RDMA, WHITE_LIST_ENABLE);
    EXPECT_INT_EQ(ret , 0);
    sleep(1);
    ret = hccp_deinit(devid);
    EXPECT_INT_EQ(ret, 0);
    msg_clear();
    mocker_clean();
}

void tc_hccp_init_fail()
{
    unsigned int devid = 0;
    pid_t host_tgid = 0;
    mocker_clean();
    mocker((stub_fn_t)sched_setaffinity, 1, -1);
    int ret = hccp_init(devid, host_tgid, HDC_SERVICE_TYPE_RDMA, WHITE_LIST_ENABLE);
    EXPECT_INT_NE(ret, 0);

    mocker_clean();
    mocker((stub_fn_t)sched_setaffinity, 10, 0);
    mocker((stub_fn_t)pthread_create, 1, -1);
    ret = hccp_init(devid, host_tgid, HDC_SERVICE_TYPE_RDMA, WHITE_LIST_ENABLE);
    ret = hccp_deinit(devid);
    EXPECT_INT_EQ(ret, 0);

    mocker_clean();
    mocker((stub_fn_t)pthread_create, 10, 0);
    ret = hccp_init(devid, host_tgid, HDC_SERVICE_TYPE_RDMA, WHITE_LIST_ENABLE);
    EXPECT_INT_NE(ret, 0);
    ret = hccp_deinit(devid);
    EXPECT_INT_EQ(ret, 0);

    mocker_clean();
    mocker((stub_fn_t)pthread_detach, 1, -1);
    mocker((stub_fn_t)rs_init, 1, -1);
    add_test_msg(RA_RS_HDC_SESSION_CLOSE, sizeof(union op_socket_close_data));
    ret = hccp_init(devid, host_tgid, HDC_SERVICE_TYPE_RDMA, WHITE_LIST_ENABLE);
    EXPECT_INT_EQ(g_current_msg_index, 0);
    EXPECT_INT_NE(ret, 0);
    ret = hccp_deinit(devid);
    EXPECT_INT_EQ(ret, 0);

    mocker_clean();
    msg_clear();
    add_test_msg(RA_RS_HDC_SESSION_CLOSE, sizeof(union op_socket_close_data));
    ret = hccp_init(RA_MAX_PHY_ID_NUM , host_tgid, HDC_SERVICE_TYPE_RDMA, WHITE_LIST_ENABLE);
    EXPECT_INT_EQ(g_current_msg_index, 0);
    EXPECT_INT_NE(ret, 0);

    mocker_clean();
    msg_clear();
    mocker((stub_fn_t)drvHdcServerCreate, 1, -1);
    add_test_msg(RA_RS_HDC_SESSION_CLOSE, sizeof(union op_socket_close_data));
    ret = hccp_init(devid , host_tgid, HDC_SERVICE_TYPE_RDMA, WHITE_LIST_ENABLE);
    EXPECT_INT_EQ(g_current_msg_index, 0);
    EXPECT_INT_NE(ret, 0);

    mocker_clean();
    msg_clear();
}

void tc_hccp_deinit_fail()
{
    mocker_clean();
    unsigned int devid = 0;
    int ret = 0;
    mocker((stub_fn_t)rs_deinit, 1, -1);
    ret = hccp_deinit(devid);
    EXPECT_INT_NE(ret, 0);
    mocker_clean();

    
}

void tc_hccp_init()
{
    tc_adp_env_init();
    tc_common_test();

    msg_clear();
    mocker_clean();
}

void tc_socket_connect()
{
    tc_adp_env_init();
    mocker((stub_fn_t)rs_socket_batch_connect, 1, 0);
    add_test_msg(RA_RS_SOCKET_CONN, sizeof(union op_socket_connect_data));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)rs_socket_batch_connect, 1, -1);
    add_test_msg(RA_RS_SOCKET_CONN, sizeof(union op_socket_connect_data));
    tc_common_test();
    
}

void tc_socket_close()
{
    tc_adp_env_init();
    mocker((stub_fn_t)rs_socket_batch_close, 1, 0);
    add_test_msg(RA_RS_SOCKET_CLOSE, sizeof(union op_socket_close_data));
    tc_common_test();
    
}

void tc_socket_abort()
{
    tc_adp_env_init();
    mocker((stub_fn_t)rs_socket_batch_abort, 1, 0);
    add_test_msg(RA_RS_SOCKET_ABORT, sizeof(union op_socket_connect_data));
    tc_common_test(); 

    mocker_clean();
    tc_adp_env_init();
    mocker((stub_fn_t)rs_socket_batch_abort, 1, -1);
    add_test_msg(RA_RS_SOCKET_ABORT, sizeof(union op_socket_connect_data));
    tc_common_test(); 
}

void tc_socket_listen_start()
{
    tc_adp_env_init();
    mocker((stub_fn_t)rs_socket_listen_start, 1, 0);
    add_test_msg(RA_RS_SOCKET_LISTEN_START, sizeof(union op_socket_listen_data));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)rs_socket_listen_start, 1, -1);
    add_test_msg(RA_RS_SOCKET_LISTEN_START, sizeof(union op_socket_listen_data));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)rs_socket_listen_start, 1, -98);
    add_test_msg(RA_RS_SOCKET_LISTEN_START, sizeof(union op_socket_listen_data));
    tc_common_test();
}

void tc_socket_listen_stop()
{
    tc_adp_env_init();
    mocker((stub_fn_t)rs_socket_listen_stop, 1, 0);
    add_test_msg(RA_RS_SOCKET_LISTEN_STOP, sizeof(union op_socket_listen_data));
    tc_common_test();
}

void tc_socket_info()
{
    tc_adp_env_init();
    mocker((stub_fn_t)rs_get_sockets, 1, 0);
    add_test_msg(RA_RS_GET_SOCKET, sizeof(union op_socket_info_data));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)rs_get_sockets, 1, -1);
    add_test_msg(RA_RS_GET_SOCKET, sizeof(union op_socket_info_data));
    tc_common_test();
}

void tc_socket_send()
{
    tc_adp_env_init();
    mocker((stub_fn_t)rs_socket_send, 1, 0);
    add_test_msg(RA_RS_SOCKET_SEND, sizeof(union op_socket_send_data));
    tc_common_test();
}

void tc_socket_recv()
{
    tc_adp_env_init();
    mocker((stub_fn_t)rs_socket_recv, 1, 0);
    add_test_msg(RA_RS_SOCKET_RECV, sizeof(union op_socket_recv_data));
    tc_common_test();
}

void tc_socket_init()
{
    tc_adp_env_init();
    mocker((stub_fn_t)rs_socket_init, 1, 0);
    add_test_msg(RA_RS_SOCKET_INIT, sizeof(union op_socket_init_data));
    tc_common_test();
}

void tc_socket_deinit()
{
    tc_adp_env_init();
    mocker((stub_fn_t)rs_socket_deinit, 1, 0);
    add_test_msg(RA_RS_SOCKET_DEINIT, sizeof(union op_socket_deinit_data));
    tc_common_test();
}

void tc_set_tsqp_depth()
{
    tc_adp_env_init();
    mocker((stub_fn_t)rs_set_tsqp_depth, 1, 0);
    add_test_msg(RA_RS_SET_TSQP_DEPTH, sizeof(union op_set_tsqp_depth_data));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)rs_set_tsqp_depth, 1, -1);
    add_test_msg(RA_RS_SET_TSQP_DEPTH, sizeof(union op_set_tsqp_depth_data));
    tc_common_test();
}

void tc_get_tsqp_depth()
{
    tc_adp_env_init();
    mocker((stub_fn_t)rs_get_tsqp_depth, 1, 0);
    add_test_msg(RA_RS_GET_TSQP_DEPTH, sizeof(union op_get_tsqp_depth_data));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)rs_get_tsqp_depth, 1, -1);
    add_test_msg(RA_RS_GET_TSQP_DEPTH, sizeof(union op_get_tsqp_depth_data));
    tc_common_test();
}

void tc_qp_create()
{
    tc_adp_env_init();
    mocker((stub_fn_t)rs_qp_create, 1, 0);
    add_test_msg(RA_RS_QP_CREATE, sizeof(union op_qp_create_data));
    mocker((stub_fn_t)rs_qp_create_with_attrs, 10, 0);
    add_test_msg(RA_RS_QP_CREATE_WITH_ATTRS, sizeof(union op_qp_create_with_attrs_data));
    add_test_msg(RA_RS_AI_QP_CREATE, sizeof(union op_ai_qp_create_data));
    add_test_msg(RA_RS_AI_QP_CREATE_WITH_ATTRS, sizeof(union op_ai_qp_create_with_attrs_data));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)rs_qp_create, 1, -1);
    add_test_msg(RA_RS_QP_CREATE, sizeof(union op_qp_create_data));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)rs_qp_create_with_attrs, 10, -1);
    add_test_msg(RA_RS_QP_CREATE_WITH_ATTRS, sizeof(union op_qp_create_with_attrs_data));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)rs_qp_create, 10, -1);
    add_test_msg(RA_RS_TYPICAL_QP_CREATE, sizeof(union op_typical_qp_create_data));
    tc_common_test();
}

void tc_qp_destroy()
{
    tc_adp_env_init();
    mocker((stub_fn_t)rs_qp_destroy, 1, 0);
    add_test_msg(RA_RS_QP_DESTROY, sizeof(union op_qp_destroy_data));
    tc_common_test();
}

void tc_qp_status()
{
    tc_adp_env_init();
    mocker((stub_fn_t)rs_get_qp_status, 1, 0);
    add_test_msg(RA_RS_QP_STATUS, sizeof(union op_qp_status_data));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)rs_get_qp_status, 1, -1);
    add_test_msg(RA_RS_QP_STATUS, sizeof(union op_qp_status_data));
    tc_common_test();
}

void tc_qp_info()
{
    tc_adp_env_init();
    mocker((stub_fn_t)rs_get_qp_status, 1, 0);
    add_test_msg(RA_RS_QP_INFO, sizeof(union op_qp_info_data));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)rs_get_qp_status, 1, -1);
    add_test_msg(RA_RS_QP_INFO, sizeof(union op_qp_info_data));
    tc_common_test();
}

void tc_qp_connect()
{
    tc_adp_env_init();
    mocker((stub_fn_t)rs_qp_connect_async, 1, 0);
    add_test_msg(RA_RS_QP_CONNECT, sizeof(union op_qp_connect_data));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)rs_typical_qp_modify, 1, -1);
    add_test_msg(RA_RS_TYPICAL_QP_MODIFY, sizeof(union op_typical_qp_modify_data));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)rs_qp_batch_modify, 1, -1);
    add_test_msg(RA_RS_QP_BATCH_MODIFY, sizeof(union op_qp_batch_modify_data));
    tc_common_test();
}

void tc_mr_reg()
{
    tc_adp_env_init();
    mocker((stub_fn_t)rs_mr_reg, 1, 0);
    add_test_msg(RA_RS_MR_REG, sizeof(union op_mr_reg_data));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)rs_typical_register_mr, 1, 0);
    add_test_msg(RA_RS_TYPICAL_MR_REG, sizeof(union op_typical_mr_reg_data));
    tc_common_test();
}

void tc_mr_dreg()
{
    tc_adp_env_init();
    mocker((stub_fn_t)rs_mr_dereg, 1, 0);
    add_test_msg(RA_RS_MR_DEREG, sizeof(union op_mr_dereg_data));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)rs_typical_deregister_mr, 1, 0);
    add_test_msg(RA_RS_TYPICAL_MR_DEREG, sizeof(union op_typical_mr_dereg_data));
    tc_common_test();
}

void tc_send_wr()
{
    tc_adp_env_init();
    mocker((stub_fn_t)rs_send_wr, 1, 0);
    add_test_msg(RA_RS_SEND_WR, sizeof(union op_send_wr_data));
    tc_common_test();
}

extern memcpy_s(void *dest, size_t destMax, const void *src, size_t count);
void tc_send_wrlist()
{
    tc_adp_env_init();
    mocker((stub_fn_t)rs_send_wrlist, 1, 0);
    mocker((stub_fn_t)memcpy_s, 1, 0);
    add_test_msg(RA_RS_SEND_WRLIST, sizeof(union op_send_wrlist_data));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)rs_send_wrlist, 1, -1);
    mocker((stub_fn_t)memcpy_s, 1, 0);
    add_test_msg(RA_RS_SEND_WRLIST, sizeof(union op_send_wrlist_data));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)rs_send_wrlist, 1, -ENOENT);
    mocker((stub_fn_t)memcpy_s, 1, 0);
    add_test_msg(RA_RS_SEND_WRLIST, sizeof(union op_send_wrlist_data));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)rs_send_wrlist, 1, 0);
    mocker((stub_fn_t)memcpy_s, 1, -1);
    add_test_msg(RA_RS_SEND_WRLIST, sizeof(union op_send_wrlist_data));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)rs_send_wrlist, 1, 0);
    mocker((stub_fn_t)memcpy_s, 1, -1);
    add_test_msg(RA_RS_SEND_WRLIST_EXT, sizeof(union op_send_wrlist_data_ext));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)rs_send_wrlist, 1, 0);
    mocker((stub_fn_t)memcpy_s, 1, -1);
    add_test_msg(RA_RS_SEND_WRLIST_V2, sizeof(union op_send_wrlist_data_v2));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)rs_send_wrlist, 1, 0);
    mocker((stub_fn_t)memcpy_s, 1, -1);
    add_test_msg(RA_RS_SEND_WRLIST_EXT_V2, sizeof(union op_send_wrlist_data_ext_v2));
    tc_common_test();
}

void tc_rdev_init()
{
    tc_adp_env_init();
    mocker((stub_fn_t)rs_rdev_init, 1, 0);
    add_test_msg(RA_RS_RDEV_INIT, sizeof(union op_rdev_init_data));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)rs_rdev_init, 1, -1);
    add_test_msg(RA_RS_RDEV_INIT, sizeof(union op_rdev_init_data));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)rs_rdev_init_with_backup, 1, 0);
    add_test_msg(RA_RS_RDEV_INIT_WITH_BACKUP, sizeof(union op_rdev_init_with_backup_data));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)rs_rdev_init_with_backup, 1, -1);
    add_test_msg(RA_RS_RDEV_INIT_WITH_BACKUP, sizeof(union op_rdev_init_with_backup_data));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)rs_rdev_get_port_status, 1, -1);
    add_test_msg(RA_RS_RDEV_GET_PORT_STATUS, sizeof(union op_rdev_get_port_status_data));
    tc_common_test();
}

void tc_rdev_deinit()
{
    tc_adp_env_init();
    mocker((stub_fn_t)rs_rdev_deinit, 1, 0);
    add_test_msg(RA_RS_RDEV_DEINIT, sizeof(union op_rdev_deinit_data));
    tc_common_test();
}
void tc_get_notify_ba()
{
    tc_adp_env_init();
    mocker((stub_fn_t)rs_get_notify_mr_info, 1, 0);
    add_test_msg(RA_RS_GET_NOTIFY_BA, sizeof(union op_get_notify_ba_data));
    tc_common_test();
}
void tc_set_pid()
{
    tc_adp_env_init();
    mocker((stub_fn_t)rs_set_host_pid, 1, 0);
    add_test_msg(RA_RS_SET_PID, sizeof(union op_set_pid_data));
    tc_common_test();
}
void tc_get_vnic_ip()
{
    tc_adp_env_init();
    mocker((stub_fn_t)rs_get_vnic_ip, 1, 0);
    add_test_msg(RA_RS_GET_VNIC_IP, sizeof(union op_get_vnic_ip_data));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)rs_get_vnic_ip, 1, -1);
    add_test_msg(RA_RS_GET_VNIC_IP, sizeof(union op_get_vnic_ip_data));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)rs_get_vnic_ip_infos, 1, -1);
    add_test_msg(RA_RS_GET_VNIC_IP_INFOS_V1, sizeof(union op_get_vnic_ip_infos_data_v1));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)rs_get_vnic_ip_infos, 1, -1);
    add_test_msg(RA_RS_GET_VNIC_IP_INFOS, sizeof(union op_get_vnic_ip_infos_data));
    tc_common_test();
}
void tc_socket_white_list_add()
{
    tc_adp_env_init();
    mocker((stub_fn_t)rs_socket_white_list_add, 1, 0);
    add_test_msg(RA_RS_WLIST_ADD, sizeof(union op_wlist_data));
    tc_common_test();

    mocker_clean();
    tc_adp_env_init();
    mocker((stub_fn_t)rs_socket_white_list_add, 1, -1);
    add_test_msg(RA_RS_WLIST_ADD, sizeof(union op_wlist_data));
    tc_common_test();
}
void tc_socket_white_list_del()
{
    tc_adp_env_init();
    mocker((stub_fn_t)rs_socket_white_list_del, 1, 0);
    add_test_msg(RA_RS_WLIST_DEL, sizeof(union op_wlist_data));
    tc_common_test();

    mocker_clean();
    tc_adp_env_init();
    mocker((stub_fn_t)rs_socket_white_list_del, 1, -1);
    add_test_msg(RA_RS_WLIST_DEL, sizeof(union op_wlist_data));
    tc_common_test();
}
void tc_get_ifaddrs()
{
    tc_adp_env_init();
    mocker((stub_fn_t)rs_get_ifaddrs, 1, 0);
    char* databuf = add_test_msg(RA_RS_GET_IFADDRS, sizeof(union op_ifaddr_data));
    union op_ifaddr_data *ifaddr_data = (union op_ifaddr_data *)(databuf + sizeof(struct msg_head));

    databuf = add_test_msg(RA_RS_GET_IFADDRS, sizeof(union op_ifaddr_data));
    ifaddr_data = (union op_ifaddr_data *)(databuf + sizeof(struct msg_head));
    ifaddr_data->tx_data.num = 1;
    tc_common_test();
}

void tc_get_ifaddrs_v2()
{
    tc_adp_env_init();
    mocker((stub_fn_t)rs_get_ifaddrs_v2, 10, 0);
    char* databuf = add_test_msg(RA_RS_GET_IFADDRS_V2, sizeof(union op_ifaddr_data_v2));
    union op_ifaddr_data_v2 *ifaddr_data = (union op_ifaddr_data_v2 *)(databuf + sizeof(struct msg_head));
    databuf = add_test_msg(RA_RS_GET_IFADDRS_V2, sizeof(union op_ifaddr_data_v2));

    ifaddr_data->tx_data.num = 1;
    databuf = add_test_msg(RA_RS_GET_IFADDRS_V2, sizeof(union op_ifaddr_data_v2));
    tc_common_test();
}

void tc_get_ifnum()
{
    tc_adp_env_init();
    mocker((stub_fn_t)rs_get_ifnum, 2, 0);
    char* databuf = add_test_msg(RA_RS_GET_IFNUM, sizeof(union op_ifnum_data));
    union op_ifnum_data *ifnum_data = (union op_ifnum_data *)(databuf + sizeof(struct msg_head));

    databuf = add_test_msg(RA_RS_GET_IFNUM, sizeof(union op_ifnum_data));
    ifnum_data = (union op_ifnum_data *)(databuf + sizeof(struct msg_head));
    ifnum_data->tx_data.num = 1;
    tc_common_test();
}

void tc_get_interface_version()
{
    tc_adp_env_init();
    mocker((stub_fn_t)rs_get_interface_version, 1, 0);

    char* databuf = add_test_msg(RA_RS_GET_INTERFACE_VERSION, sizeof(union op_get_version_data));
    union op_get_version_data *version_info = (union op_get_version_data *)(databuf + sizeof(struct msg_head));
    #if 0
    databuf = add_test_msg(RA_RS_GET_INTERFACE_VERSION, sizeof(union op_get_version_data));
    version_info = (union op_get_version_data *)(databuf + sizeof(struct msg_head));
    #endif
    version_info->tx_data.opcode = 0;
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)rs_get_interface_version, 1, -1);
    databuf = add_test_msg(RA_RS_GET_INTERFACE_VERSION, sizeof(union op_get_version_data));
    version_info = (union op_get_version_data *)(databuf + sizeof(struct msg_head));
    version_info->tx_data.opcode = 0;
    tc_common_test();
}

void tc_message_process_fail()
{
    tc_adp_env_init();
    struct msg_head* head = (struct msg_head*)add_test_msg(RA_RS_GET_IFADDRS, sizeof(union op_ifaddr_data));
    head->msg_data_len = 0;
    tc_common_test();
    tc_adp_env_init();
    head = (struct msg_head*)add_test_msg(RA_RS_GET_IFADDRS, sizeof(union op_ifaddr_data));
    head->host_tgid = g_host_tgid + 1;
    tc_common_test();
    tc_adp_env_init();
    head = (struct msg_head*)add_test_msg(RA_RS_GET_IFADDRS, sizeof(union op_ifaddr_data));
    head->opcode = RA_RS_OP_MAX_NUM;
    tc_common_test();
    tc_adp_env_init();
    mocker((stub_fn_t)drvHdcAllocMsg, 1, -1);
    unsigned int devid = 0;
    int ret = hccp_init(devid, g_host_tgid, HDC_SERVICE_TYPE_RDMA, WHITE_LIST_ENABLE);
    ret = hccp_deinit(devid);
    EXPECT_INT_EQ(ret, 0);

    mocker_clean();
    msg_clear();
    mocker((stub_fn_t)halHdcRecv, 10, -1);
    mocker_invoke((stub_fn_t)drvHdcSessionAccept, (stub_fn_t)stub_accept_session, 10);
    ret = hccp_init(devid, g_host_tgid, HDC_SERVICE_TYPE_RDMA, WHITE_LIST_ENABLE);
    ret = hccp_deinit(devid);
    EXPECT_INT_EQ(ret, 0);

    mocker_clean();
    msg_clear();
    mocker((stub_fn_t)halHdcRecv, 10, 0);
    mocker((stub_fn_t)drvHdcReuseMsg, 1, -1);
    mocker_invoke((stub_fn_t)drvHdcSessionAccept, (stub_fn_t)stub_accept_session, 10);
    ret = hccp_init(devid, g_host_tgid, HDC_SERVICE_TYPE_RDMA, WHITE_LIST_ENABLE);
    ret = hccp_deinit(devid);
    EXPECT_INT_EQ(ret, 0);

    mocker_clean();
    msg_clear();
    mocker((stub_fn_t)halHdcRecv, 10, 0);
    mocker((stub_fn_t)drvHdcAddMsgBuffer, 1, -1);
    mocker_invoke((stub_fn_t)drvHdcSessionAccept, (stub_fn_t)stub_accept_session, 10);
    ret = hccp_init(devid, g_host_tgid, HDC_SERVICE_TYPE_RDMA, WHITE_LIST_ENABLE);
    ret = hccp_deinit(devid);
    EXPECT_INT_EQ(ret, 0);

    mocker_clean();
    msg_clear();
    mocker((stub_fn_t)halHdcRecv, 10, 0);
    mocker((stub_fn_t)halHdcSend, 1, -1);
    mocker_invoke((stub_fn_t)drvHdcSessionAccept, (stub_fn_t)stub_accept_session, 10);
    ret = hccp_init(devid, g_host_tgid, HDC_SERVICE_TYPE_RDMA, WHITE_LIST_ENABLE);
}

void tc_set_notify_cfg()
{
    int result;
    mocker_clean();
    unsigned int size = sizeof(union op_notify_cfg_set_data) + sizeof(struct msg_head);
    char *in_buf = calloc(1, size);
    union op_notify_cfg_set_data set_notify_ba_data = {0};
    memcpy(in_buf + sizeof(struct msg_head), &set_notify_ba_data, sizeof(union op_notify_cfg_set_data));
    ra_rs_notify_cfg_set(in_buf, NULL, NULL, &result, 1);

    free(in_buf);
    in_buf = NULL;

    tc_adp_env_init();
    mocker((stub_fn_t)rs_notify_cfg_set, 1, 0);
    add_test_msg(RA_RS_NOTIFY_CFG_SET, sizeof(union op_notify_cfg_set_data));
    tc_common_test();
}

void tc_get_notify_cfg()
{
    int result;
    mocker_clean();
    unsigned int size = sizeof(union op_notify_cfg_get_data) + sizeof(struct msg_head);
    char *in_buf = calloc(1, size);
    char *out_buf = calloc(1, size);
    union op_notify_cfg_get_data get_notify_ba_data = {0};
    memcpy(in_buf + sizeof(struct msg_head), &get_notify_ba_data, sizeof(union op_notify_cfg_get_data));
    memcpy(out_buf + sizeof(struct msg_head), &get_notify_ba_data, sizeof(union op_notify_cfg_get_data));
    ra_rs_notify_cfg_get(in_buf, out_buf, NULL, &result, 1);

    free(out_buf);
    out_buf = NULL;
    free(in_buf);
    in_buf = NULL;

    tc_adp_env_init();
    mocker((stub_fn_t)rs_notify_cfg_get, 1, 0);
    add_test_msg(RA_RS_NOTIFY_CFG_GET, sizeof(union op_notify_cfg_get_data));
    tc_common_test();
}

void tc_ra_rs_send_wr_list_v2()
{
    union op_send_wrlist_data_v2 send_wrlist;

    send_wrlist.tx_data.phy_id = 0;
    send_wrlist.tx_data.rdev_index = 0;
    send_wrlist.tx_data.qpn = 0;
    send_wrlist.tx_data.send_num = 1;
    send_wrlist.tx_data.wrlist[0].op = 0;
    send_wrlist.tx_data.wrlist[0].send_flags = 0;
    send_wrlist.tx_data.wrlist[0].dst_addr = 0;
    send_wrlist.tx_data.wrlist[0].mem_list.addr = 0;
    send_wrlist.tx_data.wrlist[0].mem_list.len = 0;
    send_wrlist.tx_data.wrlist[0].mem_list.lkey = 0;

    union op_send_wrlist_data_v2 send_wrlist_out;

    char* in_buf = (char*)(&send_wrlist);
    char* out_buf = (char*)(&send_wrlist_out);

    int out_len;
    int op_result;
    int rcv_buf_len = 0;

    ra_rs_send_wr_list_v2(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
}

void tc_ra_rs_send_wr_list()
{
    union op_send_wrlist_data send_wrlist;

    send_wrlist.tx_data.phy_id = 0;
    send_wrlist.tx_data.rdev_index = 0;
    send_wrlist.tx_data.qpn = 0;
    send_wrlist.tx_data.send_num = 1;
    send_wrlist.tx_data.wrlist[0].op = 0;
    send_wrlist.tx_data.wrlist[0].send_flags = 0;
    send_wrlist.tx_data.wrlist[0].dst_addr = 0;
    send_wrlist.tx_data.wrlist[0].mem_list.addr = 0;
    send_wrlist.tx_data.wrlist[0].mem_list.len = 0;
    send_wrlist.tx_data.wrlist[0].mem_list.lkey = 0;

    union op_send_wrlist_data send_wrlist_out;

    char* in_buf = (char*)(&send_wrlist);
    char* out_buf = (char*)(&send_wrlist_out);

    int out_len;
    int op_result;
    int rcv_buf_len = 0;
    
    ra_rs_send_wr_list(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);

    tc_ra_rs_send_wr_list_v2();
}

void tc_ra_rs_send_wr_list_ext_v2()
{
    union op_send_wrlist_data_ext_v2 send_wrlist;

    send_wrlist.tx_data.phy_id = 0;
    send_wrlist.tx_data.rdev_index = 0;
    send_wrlist.tx_data.qpn = 0;
    send_wrlist.tx_data.send_num = 1;
    send_wrlist.tx_data.wrlist[0].op = 0;
    send_wrlist.tx_data.wrlist[0].send_flags = 0;
    send_wrlist.tx_data.wrlist[0].dst_addr = 0;
    send_wrlist.tx_data.wrlist[0].mem_list.addr = 0;
    send_wrlist.tx_data.wrlist[0].mem_list.len = 0;
    send_wrlist.tx_data.wrlist[0].mem_list.lkey = 0;
    send_wrlist.tx_data.wrlist[0].aux.data_type = 0;
    send_wrlist.tx_data.wrlist[0].aux.reduce_type = 0;
    send_wrlist.tx_data.wrlist[0].aux.notify_offset = 0;

    union op_send_wrlist_data_ext_v2 send_wrlist_out;

    char* in_buf = (char*)(&send_wrlist);
    char* out_buf = (char*)(&send_wrlist_out);

    int out_len;
    int op_result;
    int rcv_buf_len = 0;

    ra_rs_send_wr_list_ext_v2(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
}

void tc_ra_rs_send_wr_list_ext()
{
    union op_send_wrlist_data_ext send_wrlist;

    send_wrlist.tx_data.phy_id = 0;
    send_wrlist.tx_data.rdev_index = 0;
    send_wrlist.tx_data.qpn = 0;
    send_wrlist.tx_data.send_num = 1;
    send_wrlist.tx_data.wrlist[0].op = 0;
    send_wrlist.tx_data.wrlist[0].send_flags = 0;
    send_wrlist.tx_data.wrlist[0].dst_addr = 0;
    send_wrlist.tx_data.wrlist[0].mem_list.addr = 0;
    send_wrlist.tx_data.wrlist[0].mem_list.len = 0;
    send_wrlist.tx_data.wrlist[0].mem_list.lkey = 0;
    send_wrlist.tx_data.wrlist[0].aux.data_type = 0;
    send_wrlist.tx_data.wrlist[0].aux.reduce_type = 0;
    send_wrlist.tx_data.wrlist[0].aux.notify_offset = 0;

    union op_send_wrlist_data_ext send_wrlist_out;
    
    char* in_buf = (char*)(&send_wrlist);
    char* out_buf = (char*)(&send_wrlist_out);

    int out_len;
    int op_result;
    int rcv_buf_len = 0;

    ra_rs_send_wr_list_ext(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);

    tc_ra_rs_send_wr_list_ext_v2();
}

void tc_ra_rs_send_normal_wrlist()
{
    tc_adp_env_init();
    mocker((stub_fn_t)rs_send_wrlist, 1, 0);
    add_test_msg(RA_RS_SEND_NORMAL_WRLIST, sizeof(union op_send_normal_wrlist_data));
    tc_common_test();
}

void tc_ra_rs_set_qp_attr_qos()
{
    tc_adp_env_init();
    mocker((stub_fn_t)rs_set_qp_attr_qos, 1, 0);
    add_test_msg(RA_RS_SET_QP_ATTR_QOS, sizeof(union op_set_qp_attr_qos_data));
    tc_common_test();
}

void tc_ra_rs_set_qp_attr_timeout()
{
    tc_adp_env_init();
    mocker((stub_fn_t)rs_set_qp_attr_timeout, 1, 0);
    add_test_msg(RA_RS_SET_QP_ATTR_TIMEOUT, sizeof(union op_set_qp_attr_timeout_data));
    tc_common_test();
}

void tc_ra_rs_set_qp_attr_retry_cnt()
{
    tc_adp_env_init();
    mocker((stub_fn_t)rs_set_qp_attr_retry_cnt, 1, 0);
    add_test_msg(RA_RS_SET_QP_ATTR_RETRY_CNT, sizeof(union op_set_qp_attr_retry_cnt_data));
    tc_common_test();
}

void tc_ra_rs_get_cqe_err_info()
{
    tc_adp_env_init();
    mocker((stub_fn_t)rs_get_cqe_err_info, 1, 0);
    add_test_msg(RA_RS_GET_CQE_ERR_INFO, sizeof(union op_get_cqe_err_info_data));
    tc_common_test();
}

void tc_ra_rs_get_cqe_err_info_num()
{
    tc_adp_env_init();
    mocker((stub_fn_t)rs_get_cqe_err_info_num, 1, 0);
    add_test_msg(RA_RS_GET_CQE_ERR_INFO_NUM, sizeof(union op_get_cqe_err_info_num_data));
    tc_common_test();
}

void tc_ra_rs_get_cqe_err_info_list()
{
    tc_adp_env_init();
    mocker((stub_fn_t)rs_get_cqe_err_info_list, 1, 0);
    add_test_msg(RA_RS_GET_CQE_ERR_INFO_LIST, sizeof(union op_get_cqe_err_info_list_data));
    tc_common_test();
}

void tc_ra_rs_get_lite_support()
{
    tc_adp_env_init();
    add_test_msg(RA_RS_GET_LITE_SUPPORT, sizeof(union op_lite_support_data));
    tc_common_test();
}

void tc_ra_rs_get_lite_rdev_cap()
{
    tc_adp_env_init();
    mocker((stub_fn_t)rs_get_lite_rdev_cap, 1, 0);
    add_test_msg(RA_RS_GET_LITE_RDEV_CAP, sizeof(union op_lite_rdev_cap_data));
    tc_common_test();
}

void tc_ra_rs_get_lite_qp_cq_attr()
{
    tc_adp_env_init();
    mocker((stub_fn_t)rs_get_lite_qp_cq_attr, 1, 0);
    add_test_msg(RA_RS_GET_LITE_QP_CQ_ATTR, sizeof(union op_lite_qp_cq_attr_data));
    tc_common_test();
}

void tc_ra_rs_get_lite_connected_info()
{
    tc_adp_env_init();
    mocker((stub_fn_t)rs_get_lite_connected_info, 1, 0);
    add_test_msg(RA_RS_GET_LITE_CONNECTED_INFO, sizeof(union op_lite_connected_info_data));
    tc_common_test();
}

void tc_ra_rs_socket_white_list_v2()
{
    tc_adp_env_init();
    mocker((stub_fn_t)rs_socket_white_list_add, 1, 0);
    mocker((stub_fn_t)rs_socket_white_list_del, 1, 0);
    add_test_msg(RA_RS_WLIST_ADD_V2, sizeof(union op_wlist_data_v2));
    add_test_msg(RA_RS_WLIST_DEL_V2, sizeof(union op_wlist_data_v2));
    tc_common_test();
}

void tc_ra_rs_socket_credit_add()
{
    tc_adp_env_init();
    mocker((stub_fn_t)rs_socket_accept_credit_add, 1, 0);
    add_test_msg(RA_RS_ACCEPT_CREDIT_ADD, sizeof(union op_accept_credit_data));
    tc_common_test();
}

void tc_ra_rs_get_lite_mem_attr()
{
    tc_adp_env_init();
    mocker((stub_fn_t)rs_get_lite_mem_attr, 1, 0);
    add_test_msg(RA_RS_GET_LITE_MEM_ATTR, sizeof(union op_lite_mem_attr_data));
    tc_common_test();
}

void tc_ra_rs_ping_init()
{
    tc_adp_env_init();
    mocker((stub_fn_t)rs_ping_init, 1, 0);
    add_test_msg(RA_RS_PING_INIT, sizeof(union op_ping_init_data));
    tc_common_test();
    tc_adp_env_init();
    mocker((stub_fn_t)rs_ping_init, 1, 12);
    add_test_msg(RA_RS_PING_INIT, sizeof(union op_ping_init_data));
    tc_common_test();
}

void tc_ra_rs_ping_target_add()
{
    tc_adp_env_init();
    mocker((stub_fn_t)rs_ping_target_add, 1, 0);
    add_test_msg(RA_RS_PING_ADD, sizeof(union op_ping_add_data));
    tc_common_test();
    tc_adp_env_init();
    mocker((stub_fn_t)rs_ping_target_add, 1, 12);
    add_test_msg(RA_RS_PING_ADD, sizeof(union op_ping_add_data));
    tc_common_test();
}

void tc_ra_rs_ping_task_start()
{
    tc_adp_env_init();
    mocker((stub_fn_t)rs_ping_task_start, 1, 0);
    add_test_msg(RA_RS_PING_START, sizeof(union op_ping_start_data));
    tc_common_test();
    tc_adp_env_init();
    mocker((stub_fn_t)rs_ping_task_start, 1, 12);
    add_test_msg(RA_RS_PING_START, sizeof(union op_ping_start_data));
    tc_common_test();
}

void tc_ra_rs_ping_get_results()
{
    tc_adp_env_init();
    mocker((stub_fn_t)rs_ping_get_results, 1, 0);
    add_test_msg(RA_RS_PING_GET_RESULTS, sizeof(union op_ping_results_data));
    tc_common_test();
    tc_adp_env_init();
    mocker((stub_fn_t)rs_ping_get_results, 1, 12);
    add_test_msg(RA_RS_PING_GET_RESULTS, sizeof(union op_ping_results_data));
    tc_common_test();
    tc_adp_env_init();
    mocker((stub_fn_t)rs_ping_get_results, 1, -11);
    add_test_msg(RA_RS_PING_GET_RESULTS, sizeof(union op_ping_results_data));
    tc_common_test();
}

void tc_ra_rs_ping_task_stop()
{
    tc_adp_env_init();
    mocker((stub_fn_t)rs_ping_task_stop, 1, 0);
    add_test_msg(RA_RS_PING_STOP, sizeof(union op_ping_stop_data));
    tc_common_test();
    tc_adp_env_init();
    mocker((stub_fn_t)rs_ping_task_stop, 1, 12);
    add_test_msg(RA_RS_PING_STOP, sizeof(union op_ping_stop_data));
    tc_common_test();
}

void tc_ra_rs_ping_target_del()
{
    tc_adp_env_init();
    mocker((stub_fn_t)rs_ping_target_del, 1, 0);
    add_test_msg(RA_RS_PING_DEL, sizeof(union op_ping_del_data));
    tc_common_test();
    tc_adp_env_init();
    mocker((stub_fn_t)rs_ping_target_del, 1, 12);
    add_test_msg(RA_RS_PING_DEL, sizeof(union op_ping_del_data));
    tc_common_test();
}

void tc_ra_rs_ping_deinit()
{
    tc_adp_env_init();
    mocker((stub_fn_t)rs_ping_deinit, 1, 0);
    add_test_msg(RA_RS_PING_DEINIT, sizeof(union op_ping_deinit_data));
    tc_common_test();
    tc_adp_env_init();
    mocker((stub_fn_t)rs_ping_deinit, 1, 12);
    add_test_msg(RA_RS_PING_DEINIT, sizeof(union op_ping_deinit_data));
    tc_common_test();
}

void tc_tlv_init()
{
    tc_adp_env_init();
    mocker((stub_fn_t)rs_tlv_init, 1, 0);
    add_test_msg(RA_RS_TLV_INIT, sizeof(union op_tlv_init_data));
    tc_common_test();
}

void tc_tlv_deinit()
{
    tc_adp_env_init();
    mocker((stub_fn_t)rs_tlv_deinit, 1, 0);
    add_test_msg(RA_RS_TLV_DEINIT, sizeof(union op_tlv_deinit_data));
    tc_common_test();
}

void tc_tlv_request()
{
    tc_adp_env_init();
    mocker((stub_fn_t)ra_rs_tlv_request, 1, 0);
    add_test_msg(RA_RS_TLV_REQUEST, sizeof(union op_tlv_request_data));
    tc_common_test();
}

void tc_ra_rs_remap_mr()
{
    mocker_clean();
    tc_adp_env_init();
    mocker((stub_fn_t)rs_remap_mr, 1, 0);
    add_test_msg(RA_RS_REMAP_MR, sizeof(union op_remap_mr_data));
    tc_common_test();
    mocker_clean();

    tc_adp_env_init();
    mocker((stub_fn_t)rs_remap_mr, 1, 1);
    add_test_msg(RA_RS_REMAP_MR, sizeof(union op_remap_mr_data));
    tc_common_test();
    mocker_clean();
}

void tc_ra_rs_test_ctx_ops()
{
    tc_adp_env_init();
    mocker((stub_fn_t)g_ra_rs_ctx_ops.get_dev_eid_info_num, 1, 0);
    add_test_msg(RA_RS_GET_DEV_EID_INFO_NUM, sizeof(union op_get_dev_eid_info_num_data));
    tc_common_test();
 
    tc_adp_env_init();
    mocker((stub_fn_t)g_ra_rs_ctx_ops.get_dev_eid_info_list, 1, 0);
    add_test_msg(RA_RS_GET_DEV_EID_INFO_LIST, sizeof(union op_get_dev_eid_info_list_data));
    tc_common_test();
 
    tc_adp_env_init();
    mocker((stub_fn_t)g_ra_rs_ctx_ops.ctx_init, 1, 0);
    add_test_msg(RA_RS_CTX_INIT, sizeof(union op_ctx_init_data));
    tc_common_test();
 
    tc_adp_env_init();
    mocker((stub_fn_t)g_ra_rs_ctx_ops.ctx_deinit, 1, 0);
    add_test_msg(RA_RS_CTX_DEINIT, sizeof(union op_ctx_deinit_data));
    tc_common_test();
 
    tc_adp_env_init();
    mocker((stub_fn_t)g_ra_rs_ctx_ops.ctx_lmem_reg, 1, 0);
    add_test_msg(RA_RS_LMEM_REG, sizeof(union op_lmem_reg_info_data));
    tc_common_test();
 
    tc_adp_env_init();
    mocker((stub_fn_t)g_ra_rs_ctx_ops.ctx_lmem_unreg, 1, 0);
    add_test_msg(RA_RS_LMEM_UNREG, sizeof(union op_lmem_unreg_info_data));
    tc_common_test();
 
    tc_adp_env_init();
    mocker((stub_fn_t)g_ra_rs_ctx_ops.ctx_rmem_import, 1, 0);
    add_test_msg(RA_RS_RMEM_IMPORT, sizeof(union op_rmem_import_info_data));
    tc_common_test();
 
    tc_adp_env_init();
    mocker((stub_fn_t)g_ra_rs_ctx_ops.ctx_rmem_unimport, 1, 0);
    add_test_msg(RA_RS_RMEM_UNIMPORT, sizeof(union op_rmem_unimport_info_data));
    tc_common_test();
 
    tc_adp_env_init();
    mocker((stub_fn_t)g_ra_rs_ctx_ops.ctx_chan_create, 1, 0);
    add_test_msg(RA_RS_CTX_CHAN_CREATE, sizeof(union op_ctx_chan_create_data));
    tc_common_test();
 
    tc_adp_env_init();
    mocker((stub_fn_t)g_ra_rs_ctx_ops.ctx_chan_destroy, 1, 0);
    add_test_msg(RA_RS_CTX_CHAN_DESTROY, sizeof(union op_ctx_chan_destroy_data));
    tc_common_test();
 
    tc_adp_env_init();
    mocker((stub_fn_t)g_ra_rs_ctx_ops.ctx_cq_create, 1, 0);
    add_test_msg(RA_RS_CTX_CQ_CREATE, sizeof(union op_ctx_cq_create_data));
    tc_common_test();
 
    tc_adp_env_init();
    mocker((stub_fn_t)g_ra_rs_ctx_ops.ctx_cq_destroy, 1, 0);
    add_test_msg(RA_RS_CTX_CQ_DESTROY, sizeof(union op_ctx_cq_destroy_data));
    tc_common_test();
 
    tc_adp_env_init();
    mocker((stub_fn_t)g_ra_rs_ctx_ops.ctx_qp_create, 1, 0);
    add_test_msg(RA_RS_CTX_QP_CREATE, sizeof(union op_ctx_qp_create_data));
    tc_common_test();
 
    tc_adp_env_init();
    mocker((stub_fn_t)g_ra_rs_ctx_ops.ctx_qp_destroy, 1, 0);
    add_test_msg(RA_RS_CTX_QP_DESTROY, sizeof(union op_ctx_qp_destroy_data));
    tc_common_test();
 
    tc_adp_env_init();
    mocker((stub_fn_t)g_ra_rs_ctx_ops.ctx_qp_import, 1, 0);
    add_test_msg(RA_RS_CTX_QP_IMPORT, sizeof(union op_ctx_qp_import_data));
    tc_common_test();
 
    tc_adp_env_init();
    mocker((stub_fn_t)g_ra_rs_ctx_ops.ctx_qp_unimport, 1, 0);
    add_test_msg(RA_RS_CTX_QP_UNIMPORT, sizeof(union op_ctx_qp_unimport_data));
    tc_common_test();
 
    tc_adp_env_init();
    mocker((stub_fn_t)g_ra_rs_ctx_ops.ctx_qp_bind, 1, 0);
    add_test_msg(RA_RS_CTX_QP_BIND, sizeof(union op_ctx_qp_bind_data));
    tc_common_test();
 
    tc_adp_env_init();
    mocker((stub_fn_t)g_ra_rs_ctx_ops.ctx_qp_unbind, 1, 0);
    add_test_msg(RA_RS_CTX_QP_UNBIND, sizeof(union op_ctx_qp_unbind_data));
    tc_common_test();
 
    tc_adp_env_init();
    mocker((stub_fn_t)g_ra_rs_ctx_ops.ctx_batch_send_wr, 1, 0);
    add_test_msg(RA_RS_CTX_BATCH_SEND_WR, sizeof(union op_ctx_batch_send_wr_data));
    tc_common_test();
 
    tc_adp_env_init();
    mocker((stub_fn_t)g_ra_rs_ctx_ops.ctx_update_ci, 1, 0);
    add_test_msg(RA_RS_CTX_UPDATE_CI, sizeof(union op_ctx_update_ci_data));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)g_ra_rs_ctx_ops.ctx_token_id_alloc, 1, 0);
    add_test_msg(RA_RS_CTX_TOKEN_ID_ALLOC, sizeof(union op_token_id_alloc_data));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)g_ra_rs_ctx_ops.ctx_token_id_free, 1, 0);
    add_test_msg(RA_RS_CTX_TOKEN_ID_FREE, sizeof(union op_token_id_free_data));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)g_ra_rs_ctx_ops.get_tp_info_list, 1, 0);
    add_test_msg(RA_RS_GET_TP_INFO_LIST, sizeof(union op_get_tp_info_list_data));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)g_ra_rs_ctx_ops.ccu_custom_channel, 1, 0);
    add_test_msg(RA_RS_CUSTOM_CHANNEL, sizeof(union op_custom_channel_data));
    tc_common_test();

    mocker_clean();
}

void tc_ra_rs_get_tls_enable0()
{
    mocker_clean();
    tc_adp_env_init();
    mocker((stub_fn_t)rs_get_tls_enable, 1, 0);
    add_test_msg(RA_RS_GET_TLS_ENABLE, sizeof(union op_get_tls_enable_data));
    tc_common_test();
    mocker_clean();
}

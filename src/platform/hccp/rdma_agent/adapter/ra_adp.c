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
#include <stdlib.h>
#include <errno.h>
#include <sys/prctl.h>
#include "securec.h"
#include "user_log.h"
#include "dl_hal_function.h"
#include "ra_comm.h"
#include "ra_hdc.h"
#include "ra_hdc_async.h"
#include "ra_hdc_lite.h"
#include "ra_hdc_rdma_notify.h"
#include "ra_hdc_rdma.h"
#include "ra_hdc_socket.h"
#include "ra_rs_comm.h"
#include "ra_hdc_tlv.h"
#include "ra_rs_err.h"
#include "rs.h"
#include "rs_ping.h"
#ifdef CONFIG_TLV
#include "ra_adp_tlv.h"
#endif
#include "ra_adp_async.h"
#include "ra_adp.h"

struct ra_hdc_server g_hdc_server[RA_MAX_PHY_ID_NUM] = {0};
struct ra_hdc_init_para g_hdc_init_para = {0};
struct rs_pthread_info g_ra_thread_info = {0};

struct rs_ops {
    int (*socket_batch_connect)(struct socket_connect_info conn[], unsigned int num);
    int (*socket_batch_close)(int disuse_linger, struct rs_socket_close_info_t conn[], unsigned int num);
    int (*socket_batch_abort)(struct socket_connect_info conn[], unsigned int num);
    int (*socket_listen_start)(struct socket_listen_info conn[], unsigned int num);
    int (*socket_listen_stop)(struct socket_listen_info conn[], unsigned int num);
    int (*get_sockets)(unsigned int role, struct socket_fd_data conn[], unsigned int num);
    int (*socket_send)(int fd, const void *data, uint64_t size);
    int (*socket_recv)(int fd, void *data, uint64_t size);
    int (*socket_init)(const unsigned int *vnic_ip, unsigned int num);
    int (*socket_deinit)(struct rdev rdev_info);
    int (*rdev_init)(struct rdev rdev_info, unsigned int notify_type, unsigned int *rdev_index);
    int (*rdev_init_with_backup)(struct rdev rdev_info, struct rdev backup_rdev_info,
        unsigned int notify_type, unsigned int *rdev_index);
    int (*rdev_get_port_status)(unsigned int phy_id, unsigned int rdev_index, enum port_status *status);
    int (*rdev_deinit)(unsigned int phy_id, unsigned int notify_type, unsigned int rdev_index);
    int (*qp_create)(unsigned int phy_id, unsigned int rdev_index, struct rs_qp_norm qp_norm,
        struct rs_qp_resp *qp_resp);
    int (*qp_create_with_attrs)(unsigned int phy_id, unsigned int rdev_index,
        struct rs_qp_norm_with_attrs *qp_norm, struct rs_qp_resp_with_attrs *qp_resp);
    int (*qp_destroy)(unsigned int phy_id, unsigned int rdev_index, unsigned int qpn);
    int (*typical_qp_modify)(unsigned int phy_id, unsigned int rdev_index, struct typical_qp local_qp_info,
        struct typical_qp remote_qp_info, unsigned int *udp_sport);
    int (*qp_batch_modify)(unsigned int phy_id, unsigned int rdev_index, int status, int qpn[], int qpn_num);
    int (*qp_connect_async)(unsigned int phy_id, unsigned int rdev_index, unsigned int qpn, int fd);
    int (*get_qp_status)(unsigned int phy_id, unsigned int rdev_index, unsigned int qpn,
        struct rs_qp_status_info *qp_info);
    int (*mr_reg)(unsigned int phy_id, unsigned int rdev_index, unsigned int qpn, struct rdma_mr_reg_info *mr_reg_info);
    int (*mr_dereg)(unsigned int phy_id, unsigned int rdev_index, unsigned int qpn, char *addr);
    int (*register_mr)(unsigned int phy_id, unsigned int rdev_index, struct rdma_mr_reg_info *mr_reg_info,
        void **mr_handle);
    int (*remap_mr)(unsigned int phy_id, unsigned int rdev_index, struct mem_remap_info mem_list[],
        unsigned int mem_num);
    int (*typical_deregister_mr)(unsigned int phy_id, unsigned int dev_index, unsigned long long addr);
    int (*send_wr)(unsigned int phy_id, unsigned int rdev_index, unsigned int qpn, struct send_wr *wr,
        struct send_wr_rsp *wr_rsp);
    int (*send_wr_list)(struct rs_wrlist_base_info base_info, struct wr_info *wr_list,
        unsigned int send_num, struct send_wr_rsp *wr_rsp, unsigned int *complete_num);
    int (*get_notify_mr_info)(unsigned int phy_id, unsigned int rdev_index, struct mr_info *info);
    int (*notify_cfg_set)(unsigned int phy_id, unsigned long long va, unsigned long long size);
    int (*notify_cfg_get)(unsigned int phy_id, unsigned long long *va, unsigned long long *size);
    int (*set_host_pid)(unsigned int phy_id, pid_t host_pid, const char *pid_sign);
    int (*white_list_add)(struct rdev rdev_info, struct socket_wlist_info_t white_list[], unsigned int num);
    int (*white_list_del)(struct rdev rdev_info, struct socket_wlist_info_t white_list[], unsigned int num);
    int (*accept_credit_add)(struct socket_listen_info conn[], uint32_t num, unsigned int credit_limit);
    int (*get_ifnum)(unsigned int phy_id, bool is_all, unsigned int *num);
    int (*get_ifaddrs)(struct ifaddr_info ifaddr_infos[], unsigned int *num, unsigned int phy_id);
    int (*get_ifaddrs_v2)(struct interface_info interface_infos[], unsigned int *num, unsigned int phy_id,
        bool is_all);
    int (*get_vnic_ip)(unsigned int phy_id, unsigned int *vnic_ip);
    int (*get_vnic_ip_infos)(unsigned int phy_id, enum id_type type, unsigned int ids[], unsigned int num,
        struct ip_info infos[]);
    int (*get_interface_version)(unsigned int opcode, unsigned int *version);
    int (*set_tsqp_depth)(unsigned int phy_id, unsigned int rdev_index, unsigned int temp_depth, unsigned int *qp_num);
    int (*get_tsqp_depth)(unsigned int phy_id, unsigned int rdev_index, unsigned int *temp_depth, unsigned int *qp_num);
    int (*set_qp_attr_qos)(unsigned int phy_id, unsigned int rdev_index, unsigned int qpn, struct qos_attr *attr);
    int (*set_qp_attr_timeout)(unsigned int phy_id, unsigned int rdev_index, unsigned int qpn, unsigned int *timeout);
    int (*set_qp_attr_retry_cnt)(unsigned int phy_id, unsigned int rdev_index, unsigned int qpn,
        unsigned int *retry_cnt);
    int (*get_cqe_err_info)(struct cqe_err_info *info);
    int (*get_lite_support)(unsigned int phy_id, unsigned int rdev_index, int *support_lite);
    int (*get_lite_rdev_cap)(unsigned int phy_id, unsigned int rdev_index, struct lite_rdev_cap_resp *resp);
    int (*get_lite_qp_cq_attr)(
        unsigned int phy_id, unsigned int rdev_index, unsigned int qpn, struct lite_qp_cq_attr_resp *resp);
    int (*get_lite_mem_attr)(
        unsigned int phy_id, unsigned int rdev_index, unsigned int qpn, struct lite_mem_attr_resp *resp);
    int (*get_lite_connected_info)(
        unsigned int phy_id, unsigned int rdev_index, unsigned int qpn, struct lite_connected_info_resp *resp);
    int (*ping_init)(struct ping_init_attr *attr, struct ping_init_info *info, unsigned int *dev_index);
    int (*ping_target_add)(struct ra_rs_dev_info *rdev, struct ping_target_info *target);
    int (*ping_task_start)(struct ra_rs_dev_info *rdev, struct ping_task_attr *attr);
    int (*ping_get_results)(struct ra_rs_dev_info *rdev, struct ping_target_comm_info target[],
        unsigned int *num, struct ping_result_info result[]);
    int (*ping_task_stop)(struct ra_rs_dev_info *rdev);
    int (*ping_target_del)(struct ra_rs_dev_info *rdev, struct ping_target_comm_info target[],
        unsigned int *num);
    int (*ping_deinit)(struct ra_rs_dev_info *rdev);
    int (*get_cqe_err_info_num)(unsigned int phy_id, unsigned int rdev_idx, unsigned int *num);
    int (*get_cqe_err_info_list)(unsigned int phy_id, unsigned int rdev_idx, struct cqe_err_info *info,
        unsigned int *num);
    int (*get_tls_enable)(unsigned int phy_id, bool *tls_enable);
    int (*get_sec_random)(int *value);
};

struct rs_ops g_ra_rs_ops = {
    .socket_batch_connect = rs_socket_batch_connect,
    .socket_batch_close = rs_socket_batch_close,
    .socket_batch_abort = rs_socket_batch_abort,
    .socket_listen_start = rs_socket_listen_start,
    .socket_listen_stop = rs_socket_listen_stop,
    .get_sockets = rs_get_sockets,
    .socket_send = rs_socket_send,
    .socket_recv = rs_socket_recv,
    .socket_init = rs_socket_init,
    .socket_deinit = rs_socket_deinit,
    .rdev_init = rs_rdev_init,
    .rdev_init_with_backup = rs_rdev_init_with_backup,
    .rdev_get_port_status = rs_rdev_get_port_status,
    .rdev_deinit = rs_rdev_deinit,
    .qp_create = rs_qp_create,
    .qp_create_with_attrs = rs_qp_create_with_attrs,
    .qp_destroy = rs_qp_destroy,
    .typical_qp_modify = rs_typical_qp_modify,
    .qp_batch_modify = rs_qp_batch_modify,
    .qp_connect_async = rs_qp_connect_async,
    .get_qp_status = rs_get_qp_status,
    .mr_reg = rs_mr_reg,
    .mr_dereg = rs_mr_dereg,
    .register_mr = rs_typical_register_mr,
    .remap_mr = rs_remap_mr,
    .typical_deregister_mr = rs_typical_deregister_mr,
    .send_wr = rs_send_wr,
    .send_wr_list = rs_send_wrlist,
    .get_notify_mr_info = rs_get_notify_mr_info,
    .notify_cfg_set = rs_notify_cfg_set,
    .notify_cfg_get = rs_notify_cfg_get,
    .set_host_pid = rs_set_host_pid,
    .white_list_add = rs_socket_white_list_add,
    .white_list_del = rs_socket_white_list_del,
    .accept_credit_add = rs_socket_accept_credit_add,
    .get_ifnum = rs_get_ifnum,
    .get_ifaddrs = rs_get_ifaddrs,
    .get_ifaddrs_v2 = rs_get_ifaddrs_v2,
    .get_vnic_ip = rs_get_vnic_ip,
    .get_vnic_ip_infos = rs_get_vnic_ip_infos,
    .get_interface_version = rs_get_interface_version,
    .set_tsqp_depth = rs_set_tsqp_depth,
    .get_tsqp_depth = rs_get_tsqp_depth,
    .set_qp_attr_qos = rs_set_qp_attr_qos,
    .set_qp_attr_timeout = rs_set_qp_attr_timeout,
    .set_qp_attr_retry_cnt = rs_set_qp_attr_retry_cnt,
    .get_cqe_err_info = rs_get_cqe_err_info,
    .get_lite_rdev_cap = rs_get_lite_rdev_cap,
    .get_lite_qp_cq_attr = rs_get_lite_qp_cq_attr,
    .get_lite_connected_info = rs_get_lite_connected_info,
    .get_lite_mem_attr = rs_get_lite_mem_attr,
    .get_lite_support = rs_get_lite_support,
    .ping_init = rs_ping_init,
    .ping_target_add = rs_ping_target_add,
    .ping_task_start = rs_ping_task_start,
    .ping_get_results = rs_ping_get_results,
    .ping_task_stop = rs_ping_task_stop,
    .ping_target_del = rs_ping_target_del,
    .ping_deinit = rs_ping_deinit,
    .get_cqe_err_info_num = rs_get_cqe_err_info_num,
    .get_cqe_err_info_list = rs_get_cqe_err_info_list,
    .get_tls_enable = rs_get_tls_enable,
    .get_sec_random = rs_drv_get_random_num,
};

struct hdc_ops g_ra_hdc_ops = {
    .get_capacity = dl_drv_hdc_get_capacity,
    .client_create = dl_drv_hdc_client_create,
    .client_destroy = dl_drv_hdc_client_destroy,
    .session_connect = dl_drv_hdc_session_connect,
    .session_connect_ex = dl_hal_hdc_session_connect_ex,
    .server_create = dl_drv_hdc_server_create,
    .server_destroy = dl_drv_hdc_server_destroy,
    .session_accept = dl_drv_hdc_session_accept,
    .session_close = dl_drv_hdc_session_close,
    .free_msg = dl_drv_hdc_free_msg,
    .reuse_msg = dl_drv_hdc_reuse_msg,
    .add_msg_buffer = dl_drv_hdc_add_msg_buffer,
    .get_msg_buffer = dl_drv_hdc_get_msg_buffer,
    .recv = dl_hal_hdc_recv,
    .send = dl_hal_hdc_send,
    .alloc_msg = dl_drv_hdc_alloc_msg,
    .set_session_reference = dl_drv_hdc_set_session_reference,
};

#define RA_HDC_OPS g_ra_hdc_ops

STATIC void msg_head_build_up_hw(char *p_send_rcv_buf, struct msg_head *recv_msg_head, int ret,
    unsigned int msg_data_len)
{
    struct msg_head *p_send_rcv_head = NULL;

    p_send_rcv_head = (struct msg_head *)p_send_rcv_buf;
    p_send_rcv_head->opcode = recv_msg_head->opcode;
    p_send_rcv_head->async_req_id = recv_msg_head->async_req_id;
    p_send_rcv_head->ret = ret;
    p_send_rcv_head->msg_data_len = msg_data_len;

    return;
}

STATIC int op_msg_err(char **out_buf, struct msg_head *recv_msg_head, int *out_buf_len, int op_right)
{
    unsigned int opcode = recv_msg_head->opcode;
    char *out_buf_tmp = NULL;
    int msg_ret = 0;

    out_buf_tmp = (char *)calloc(sizeof(struct msg_head), sizeof(char));
    CHK_PRT_RETURN(out_buf_tmp == NULL, hccp_err("send_buf calloc failed."), -ENOMEM);

    if (op_right == HAVE_OP_RIGHT) {
        if (opcode >= RA_RS_OP_MAX_NUM || ((opcode < RA_RS_HDC_SESSION_CLOSE) && (opcode >= RA_RS_EXTER_OP_MAX_NUM))) {
            msg_ret = -EPROTONOSUPPORT;
        } else {
            msg_ret = -EPIPE;
        }
    } else if (op_right == TGID_INVALID) {
        msg_ret = -EPERM;
    } else {
        msg_ret = -EACCES;
    }

    msg_head_build_up_hw(out_buf_tmp, recv_msg_head, msg_ret, 0);

    *out_buf = out_buf_tmp;
    *out_buf_len = sizeof(struct msg_head);

    return 0;
}

STATIC int ra_rs_socket_batch_connect(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_socket_connect_data *socket_connect_data =
        (union op_socket_connect_data *)(in_buf + sizeof(struct msg_head));
    unsigned int use_port = 0;
    unsigned int i;

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_socket_connect_data), sizeof(struct msg_head), rcv_buf_len,
        op_result);

    // clear resv bit 31 use_port, for compatibility issue
    use_port = socket_connect_data->tx_data.num >> SOCKET_USE_PORT_BIT;
    socket_connect_data->tx_data.num &= ~(1U << SOCKET_USE_PORT_BIT);
    HCCP_CHECK_PARAM_NUM(socket_connect_data->tx_data.num, MAX_SOCKET_NUM);

    for (i = 0; i < (socket_connect_data->tx_data).num; i++) {
        // use_port flag not specify, use default port for compatibility issue
        if (use_port == 0) {
            (socket_connect_data->tx_data).conn[i].port = RS_SOCK_PORT_DEF;
        } else if ((socket_connect_data->tx_data).conn[i].port > MAX_PORT_NUM) {
            hccp_err("[batch_connect]conn[%u].port=%u invalid", i, (socket_connect_data->tx_data).conn[i].port);
            return -EINVAL;
        }
    }

    *op_result = g_ra_rs_ops.socket_batch_connect((socket_connect_data->tx_data).conn,
        (socket_connect_data->tx_data).num);
    if (*op_result != 0) {
        hccp_err("socket batch connect failed ret[%d].", *op_result);
    }

    return 0;
}

STATIC int ra_rs_socket_batch_close(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_socket_close_data *socket_close_data = (union op_socket_close_data *)(in_buf + sizeof(struct msg_head));
    int disuse_linger = 0;
    unsigned int i;

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_socket_close_data), sizeof(struct msg_head), rcv_buf_len, op_result);

    // clear resv bit 31 disuse_linger, for compatibility issue(0 by default)
    disuse_linger = socket_close_data->tx_data.num >> SOCKET_DISUSE_LINGER_BIT;
    socket_close_data->tx_data.num &= ~(1U << SOCKET_DISUSE_LINGER_BIT);
    HCCP_CHECK_PARAM_NUM(socket_close_data->tx_data.num, MAX_SOCKET_NUM);

    struct rs_socket_close_info_t close_conn[MAX_SOCKET_NUM] = {0};
    for (i = 0; i < socket_close_data->tx_data.num; i++) {
        close_conn[i].fd = ((socket_close_data->tx_data).conn[i]).close_fd;
    }
    *op_result = g_ra_rs_ops.socket_batch_close(disuse_linger, close_conn, (socket_close_data->tx_data).num);
    if (*op_result != 0) {
        hccp_err("socket batch close failed ret[%d].", *op_result);
    }

    return 0;
}

STATIC int ra_rs_socket_batch_abort(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_socket_connect_data *socket_connect_data = (union op_socket_connect_data *)(in_buf +
        sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_socket_connect_data), sizeof(struct msg_head), rcv_buf_len,
        op_result);
    HCCP_CHECK_PARAM_NUM(socket_connect_data->tx_data.num, MAX_SOCKET_NUM);

    *op_result = g_ra_rs_ops.socket_batch_abort((socket_connect_data->tx_data).conn,
        (socket_connect_data->tx_data).num);
    if (*op_result != 0) {
        hccp_err("socket batch abort failed ret[%d]", *op_result);
    }

    return 0;
}

STATIC int ra_rs_socket_listen_start(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_socket_listen_data *socket_listen_data = (union op_socket_listen_data *)(in_buf + sizeof(struct msg_head));
    union op_socket_listen_data *socket_listen_data_return = NULL;
    unsigned int use_port = 0;
    unsigned int i;
    int ret;

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_socket_listen_data), sizeof(struct msg_head), rcv_buf_len, op_result);

    // clear resv bit 31 use_port, for compatibility issue
    use_port = socket_listen_data->tx_data.num >> SOCKET_USE_PORT_BIT;
    socket_listen_data->tx_data.num &= ~(1U << SOCKET_USE_PORT_BIT);
    HCCP_CHECK_PARAM_LEN_RET_HOST(socket_listen_data->tx_data.num, 0, MAX_SOCKET_NUM, op_result);

    for (i = 0; i < (socket_listen_data->tx_data).num; i++) {
        // use_port flag not specify, use default port for compatibility issue
        if (use_port == 0) {
            (socket_listen_data->tx_data).conn[i].port = RS_SOCK_PORT_DEF;
        } else if ((socket_listen_data->tx_data).conn[i].port > MAX_PORT_NUM) {
            hccp_err("[listen_start]conn[%u].port=%u invalid", i, (socket_listen_data->tx_data).conn[i].port);
            return -EINVAL;
        }
    }
    *op_result = g_ra_rs_ops.socket_listen_start((socket_listen_data->tx_data).conn, (socket_listen_data->tx_data).num);
    if (*op_result == -EADDRINUSE) {
        hccp_run_warn("socket listen start unsuccessful ret[%d]", *op_result);
        return 0;
    } else if (*op_result != 0) {
        hccp_err("socket listen start failed ret[%d]", *op_result);
        return 0;
    }

    socket_listen_data_return = (union op_socket_listen_data *)(out_buf + sizeof(struct msg_head));
    ret = memcpy_s((socket_listen_data_return->rx_data).conn, sizeof(struct socket_listen_info) * MAX_SOCKET_NUM,
        (socket_listen_data->tx_data).conn, sizeof(struct socket_listen_info) * (socket_listen_data->tx_data).num);
    CHK_PRT_RETURN(ret, hccp_err("memcpy_s socket_listen_info failed, ret[%d]", ret), -ESAFEFUNC);
    return 0;
}

STATIC int ra_rs_socket_listen_stop(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_socket_listen_data *socket_listen_data = (union op_socket_listen_data *)(in_buf + sizeof(struct msg_head));
    unsigned int use_port = 0;
    unsigned int i;

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_socket_listen_data), sizeof(struct msg_head), rcv_buf_len, op_result);

    // clear resv bit 31 use_port, for compatibility issue
    use_port = socket_listen_data->tx_data.num >> SOCKET_USE_PORT_BIT;
    socket_listen_data->tx_data.num &= ~(1U << SOCKET_USE_PORT_BIT);
    HCCP_CHECK_PARAM_LEN_RET_HOST(socket_listen_data->tx_data.num, 0, MAX_SOCKET_NUM, op_result);

    for (i = 0; i < (socket_listen_data->tx_data).num; i++) {
        // use_port flag not specify, use default port for compatibility issue
        if (use_port == 0) {
            (socket_listen_data->tx_data).conn[i].port = RS_SOCK_PORT_DEF;
        } else if ((socket_listen_data->tx_data).conn[i].port > MAX_PORT_NUM) {
            hccp_err("[listen_stop]conn[%u].port=%u invalid", i, (socket_listen_data->tx_data).conn[i].port);
            return -EINVAL;
        }
    }

    *op_result = g_ra_rs_ops.socket_listen_stop((socket_listen_data->tx_data).conn, (socket_listen_data->tx_data).num);
    if (*op_result != 0) {
        hccp_err("socket listen stop failed ret[%d].", *op_result);
    }

    return 0;
}

STATIC int ra_rs_get_sockets(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    int ret;
    union op_socket_info_data *socket_info_data_return = NULL;
    union op_socket_info_data *socket_info_data = (union op_socket_info_data *)(in_buf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_socket_info_data), sizeof(struct msg_head), rcv_buf_len, op_result);
    HCCP_CHECK_PARAM_LEN_RET_HOST(socket_info_data->tx_data.num, 0, MAX_SOCKET_NUM, op_result);

    *op_result = g_ra_rs_ops.get_sockets(socket_info_data->tx_data.role, socket_info_data->tx_data.conn,
        socket_info_data->tx_data.num);
    if (*op_result < 0) {
        hccp_err("socket info get failed ret[%d].", *op_result);
        return 0;
    }

    socket_info_data_return = (union op_socket_info_data *)(out_buf + sizeof(struct msg_head));

    (socket_info_data_return->rx_data).num = *op_result;
    ret = memcpy_s((socket_info_data_return->rx_data).conn, sizeof(struct socket_fd_data) * MAX_SOCKET_NUM,
        (socket_info_data->tx_data).conn, sizeof(struct socket_fd_data) * (socket_info_data->tx_data).num);
    CHK_PRT_RETURN(ret, hccp_err("ra_rs_get_sockets memcpy_s failed, ret[%d]. ", ret), -ESAFEFUNC);

    return 0;
}

STATIC int ra_rs_socket_recv(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    int recv_len;
    union op_socket_recv_data *recv_data = (union op_socket_recv_data *)(in_buf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_socket_recv_data), sizeof(struct msg_head), rcv_buf_len, op_result);
    HCCP_CHECK_PARAM_LEN(sizeof(union op_socket_recv_data) + recv_data->tx_data.recv_size, sizeof(struct msg_head),
        rcv_buf_len);

    recv_len = g_ra_rs_ops.socket_recv(recv_data->tx_data.fd,
        out_buf + sizeof(struct msg_head) + sizeof(union op_socket_recv_data), recv_data->tx_data.recv_size);
    *op_result = recv_len;

    recv_data = (union op_socket_recv_data *)(out_buf + sizeof(struct msg_head));
    recv_data->rx_data.real_recv_size = recv_len;

    return 0;
}

STATIC int ra_rs_socket_send(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    int send_len;
    union op_socket_send_data *send_data = (union op_socket_send_data *)(in_buf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_socket_send_data), sizeof(struct msg_head), rcv_buf_len, op_result);
    HCCP_CHECK_PARAM_LEN_RET_HOST(send_data->tx_data.send_size, 0, SOCKET_SEND_MAXLEN, op_result);

    send_len =
        g_ra_rs_ops.socket_send(send_data->tx_data.fd, send_data->tx_data.data_send, send_data->tx_data.send_size);
    if (send_len <= 0) {
        if (send_len == -EAGAIN) {
            hccp_dbg("socket send need retry, ret[%d]", send_len);
        }else {
            hccp_warn("send unsuccessful, send_len[%d] expect greater than 0.", send_len);
        }
    }

    *op_result = send_len;
    send_data = (union op_socket_send_data *)(out_buf + sizeof(struct msg_head));
    send_data->rx_data.real_send_size = send_len;

    return 0;
}

STATIC int ra_rs_rdev_init(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    unsigned int rdev_index = 0;
    union op_rdev_init_data *rdev_init_data = (union op_rdev_init_data *)(in_buf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_rdev_init_data), sizeof(struct msg_head), rcv_buf_len, op_result);

    *op_result = g_ra_rs_ops.rdev_init(rdev_init_data->tx_data.rdev_info, NOTIFY, &rdev_index);
    if (*op_result != 0) {
        hccp_err("rdev_init failed ret[%d].", *op_result);
        return 0;
    }

    rdev_init_data = (union op_rdev_init_data *)(out_buf + sizeof(struct msg_head));
    rdev_init_data->rx_data.rdev_index = rdev_index;

    return 0;
}

STATIC int ra_rs_rdev_init_with_backup(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_rdev_init_with_backup_data *rdev_init_data = (union op_rdev_init_with_backup_data *)(in_buf +
        sizeof(struct msg_head));
    unsigned int rdev_index = 0;

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_rdev_init_with_backup_data), sizeof(struct msg_head),
        rcv_buf_len, op_result);

    *op_result = g_ra_rs_ops.rdev_init_with_backup(rdev_init_data->tx_data.rdev_info,
        rdev_init_data->tx_data.backup_rdev_info, NOTIFY, &rdev_index);
    if (*op_result != 0) {
        hccp_err("rdev_init_with_backup failed ret[%d].", *op_result);
        return 0;
    }

    rdev_init_data = (union op_rdev_init_with_backup_data *)(out_buf + sizeof(struct msg_head));
    rdev_init_data->rx_data.rdev_index = rdev_index;

    return 0;
}

STATIC int ra_rs_rdev_get_port_status(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_rdev_get_port_status_data *status_data = NULL;
    enum port_status status = PORT_STATUS_DOWN;

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_rdev_get_port_status_data), sizeof(struct msg_head), rcv_buf_len,
        op_result);

    status_data = (union op_rdev_get_port_status_data *)(in_buf + sizeof(struct msg_head));
    *op_result = g_ra_rs_ops.rdev_get_port_status(status_data->tx_data.phy_id,
        status_data->tx_data.rdev_index, &status);
    if (*op_result != 0) {
        hccp_err("rdev_get_port_status failed ret[%d].", *op_result);
        return 0;
    }

    status_data = (union op_rdev_get_port_status_data *)(out_buf + sizeof(struct msg_head));
    status_data->rx_data.status = status;

    return 0;
}

STATIC int ra_rs_rdev_deinit(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_rdev_deinit_data *rdev_deinit_data = (union op_rdev_deinit_data *)(in_buf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_rdev_deinit_data), sizeof(struct msg_head), rcv_buf_len, op_result);

    *op_result = g_ra_rs_ops.rdev_deinit(rdev_deinit_data->tx_data.phy_id, NOTIFY,
        rdev_deinit_data->tx_data.rdev_index);
    if (*op_result != 0) {
        hccp_err("rdev_deinit failed ret[%d].", *op_result);
    }

    return 0;
}

STATIC int ra_rs_socket_init(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_socket_init_data *socket_init_data = (union op_socket_init_data *)(in_buf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_socket_init_data), sizeof(struct msg_head), rcv_buf_len, op_result);

    *op_result = g_ra_rs_ops.socket_init(socket_init_data->tx_data.vnic_ip, socket_init_data->tx_data.num);
    if (*op_result != 0) {
        hccp_err("socket init failed ret[%d].", *op_result);
    }

    return 0;
}

STATIC int ra_rs_socket_deinit(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_socket_deinit_data *socket_deinit_data = (union op_socket_deinit_data *)(in_buf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_socket_deinit_data), sizeof(struct msg_head), rcv_buf_len, op_result);

    *op_result = g_ra_rs_ops.socket_deinit(socket_deinit_data->tx_data.rdev_info);
    if (*op_result != 0) {
        hccp_err("socket deinit failed ret[%d].", *op_result);
    }

    return 0;
}

STATIC int ra_rs_get_tsqp_depth(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    unsigned int temp_depth = 0;
    unsigned int qp_num = 0;
    union op_get_tsqp_depth_data *get_tsqp_depth_data = (union op_get_tsqp_depth_data *)(in_buf +
        sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_get_tsqp_depth_data), sizeof(struct msg_head), rcv_buf_len,
        op_result);

    *op_result = g_ra_rs_ops.get_tsqp_depth(get_tsqp_depth_data->tx_data.phy_id,
        get_tsqp_depth_data->tx_data.rdev_index, &temp_depth, &qp_num);
    if (*op_result != 0) {
        hccp_err("set_tsqp_depth failed ret[%d].", *op_result);
        return 0;
    }

    get_tsqp_depth_data = (union op_get_tsqp_depth_data *)(out_buf + sizeof(struct msg_head));
    get_tsqp_depth_data->rx_data.temp_depth = temp_depth;
    get_tsqp_depth_data->rx_data.qp_num = qp_num;

    return 0;
}

STATIC int ra_rs_set_tsqp_depth(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    unsigned int qp_num = 0;
    union op_set_tsqp_depth_data *set_tsqp_depth_data = (union op_set_tsqp_depth_data *)(in_buf +
        sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_set_tsqp_depth_data), sizeof(struct msg_head), rcv_buf_len,
        op_result);

    *op_result = g_ra_rs_ops.set_tsqp_depth(set_tsqp_depth_data->tx_data.phy_id,
        set_tsqp_depth_data->tx_data.rdev_index, set_tsqp_depth_data->tx_data.temp_depth, &qp_num);
    if (*op_result != 0) {
        hccp_err("set_tsqp_depth failed ret[%d].", *op_result);
        return 0;
    }

    set_tsqp_depth_data = (union op_set_tsqp_depth_data *)(out_buf + sizeof(struct msg_head));
    set_tsqp_depth_data->rx_data.qp_num = qp_num;

    return 0;
}

STATIC int ra_rs_qp_create(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    struct rs_qp_norm qp_norm;
    struct rs_qp_resp qp_resp = { 0 };
    union op_qp_create_data *create_data = (union op_qp_create_data *)(in_buf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_qp_create_data), sizeof(struct msg_head), rcv_buf_len, op_result);

    int qp_mode = create_data->tx_data.qp_mode;
    qp_norm.flag = create_data->tx_data.flag;
    qp_norm.is_exp = 1;
    qp_norm.is_ext = 1;
    if (qp_mode == RA_RS_OP_QP_MODE_EXT) {
        qp_norm.qp_mode = RA_RS_OP_QP_MODE;
    } else {
        qp_norm.qp_mode = qp_mode;
    }
    qp_norm.mem_align = create_data->tx_data.mem_align;

    *op_result = g_ra_rs_ops.qp_create(create_data->tx_data.phy_id, create_data->tx_data.rdev_index, qp_norm, &qp_resp);
    if (*op_result != 0) {
        hccp_err("qp create failed ret[%d].", *op_result);
        return 0;
    }

    create_data = (union op_qp_create_data *)(out_buf + sizeof(struct msg_head));
    create_data->rx_data.qpn = qp_resp.qpn;
    create_data->rx_data.psn = qp_resp.psn;
    create_data->rx_data.gid_idx = qp_resp.gid_idx;

    return 0;
}

STATIC int ra_rs_qp_create_with_attrs(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_qp_create_with_attrs_data *create_data = NULL;
    struct rs_qp_norm_with_attrs qp_norm = { 0 };
    struct rs_qp_resp_with_attrs qp_resp = { 0 };

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_qp_create_with_attrs_data), sizeof(struct msg_head), rcv_buf_len,
        op_result);

    create_data = (union op_qp_create_with_attrs_data *)(in_buf + sizeof(struct msg_head));

    qp_norm.is_exp = 1;
    qp_norm.is_ext = 1;
    qp_norm.ext_attrs = create_data->tx_data.ext_attrs;

    *op_result = g_ra_rs_ops.qp_create_with_attrs(create_data->tx_data.phy_id, create_data->tx_data.rdev_index,
        &qp_norm, &qp_resp);
    if (*op_result != 0) {
        hccp_err("qp create failed ret[%d].", *op_result);
        return 0;
    }

    create_data = (union op_qp_create_with_attrs_data *)(out_buf + sizeof(struct msg_head));
    create_data->rx_data.qpn = qp_resp.qpn;
    create_data->rx_data.psn = qp_resp.psn;
    create_data->rx_data.gid_idx = qp_resp.gid_idx;

    return 0;
}

STATIC int ra_rs_ai_qp_create(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_ai_qp_create_data *create_data = NULL;
    struct rs_qp_norm_with_attrs qp_norm = { 0 };
    struct rs_qp_resp_with_attrs qp_resp = { 0 };

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_ai_qp_create_data), sizeof(struct msg_head), rcv_buf_len,
        op_result);

    create_data = (union op_ai_qp_create_data *)(in_buf + sizeof(struct msg_head));

    qp_norm.is_exp = 1;
    qp_norm.is_ext = 1;
    qp_norm.ext_attrs = create_data->tx_data.ext_attrs;
    qp_norm.ai_op_support = 1;

    *op_result = g_ra_rs_ops.qp_create_with_attrs(create_data->tx_data.phy_id, create_data->tx_data.rdev_index,
        &qp_norm, &qp_resp);
    if (*op_result != 0) {
        hccp_err("qp create failed ret[%d].", *op_result);
        return 0;
    }

    create_data = (union op_ai_qp_create_data *)(out_buf + sizeof(struct msg_head));
    create_data->rx_data.qpn = qp_resp.qpn;
    create_data->rx_data.ai_qp_addr = qp_resp.ai_qp_addr;
    create_data->rx_data.sq_index = qp_resp.sq_index;
    create_data->rx_data.db_index = qp_resp.db_index;
    create_data->rx_data.psn = qp_resp.psn;

    return 0;
}

STATIC int ra_rs_ai_qp_create_with_data(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_ai_qp_create_with_attrs_data *create_data = NULL;
    struct rs_qp_norm_with_attrs qp_norm = { 0 };
    struct rs_qp_resp_with_attrs qp_resp = { 0 };

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_ai_qp_create_with_attrs_data), sizeof(struct msg_head), rcv_buf_len,
        op_result);

    create_data = (union op_ai_qp_create_with_attrs_data *)(in_buf + sizeof(struct msg_head));

    qp_norm.is_exp = 1;
    qp_norm.is_ext = 1;
    qp_norm.ext_attrs = create_data->tx_data.ext_attrs;
    qp_norm.ai_op_support = 1;

    *op_result = g_ra_rs_ops.qp_create_with_attrs(create_data->tx_data.phy_id, create_data->tx_data.rdev_index,
        &qp_norm, &qp_resp);
    if (*op_result != 0) {
        hccp_err("qp create failed ret[%d].", *op_result);
        return 0;
    }

    create_data = (union op_ai_qp_create_with_attrs_data *)(out_buf + sizeof(struct msg_head));
    create_data->rx_data.qpn = qp_resp.qpn;
    create_data->rx_data.gid_idx = qp_resp.gid_idx;
    create_data->rx_data.psn = qp_resp.psn;
    create_data->rx_data.ai_qp_addr = qp_resp.ai_qp_addr;
    create_data->rx_data.sq_index = qp_resp.sq_index;
    create_data->rx_data.db_index = qp_resp.db_index;
    create_data->rx_data.ai_scq_addr = qp_resp.ai_scq_addr;
    create_data->rx_data.ai_rcq_addr = qp_resp.ai_rcq_addr;
    (void)memcpy_s(&create_data->rx_data.data_plane_info, sizeof(struct ai_data_plane_info), &qp_resp.data_plane_info,
        sizeof(struct ai_data_plane_info));

    return 0;
}

STATIC int ra_rs_typical_qp_create(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_typical_qp_create_data *create_data = (union op_typical_qp_create_data *)(in_buf +
        sizeof(struct msg_head));
    struct rs_qp_resp qp_resp = {0};
    struct rs_qp_norm qp_norm;
    int qp_mode;

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_typical_qp_create_data), sizeof(struct msg_head), rcv_buf_len,
        op_result);

    qp_mode = create_data->tx_data.qp_mode;
    qp_norm.flag = create_data->tx_data.flag;
    qp_norm.is_exp = 1;
    qp_norm.is_ext = 1;
    if (qp_mode == RA_RS_OP_QP_MODE_EXT) {
        qp_norm.qp_mode = RA_RS_OP_QP_MODE;
    } else {
        qp_norm.qp_mode = qp_mode;
    }
    qp_norm.mem_align = create_data->tx_data.mem_align;

    *op_result = g_ra_rs_ops.qp_create(create_data->tx_data.phy_id, create_data->tx_data.rdev_index, qp_norm, &qp_resp);
    if (*op_result != 0) {
        hccp_err("qp create failed ret[%d].", *op_result);
        return 0;
    }

    create_data = (union op_typical_qp_create_data *)(out_buf + sizeof(struct msg_head));
    create_data->rx_data.qpn = qp_resp.qpn;
    create_data->rx_data.gid_idx = qp_resp.gid_idx;
    create_data->rx_data.psn = qp_resp.psn;
    create_data->rx_data.gid = qp_resp.gid;

    return 0;
}

STATIC int ra_rs_qp_destroy(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_qp_destroy_data *qp_destroy_data = (union op_qp_destroy_data *)(in_buf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_qp_destroy_data), sizeof(struct msg_head), rcv_buf_len, op_result);

    *op_result = g_ra_rs_ops.qp_destroy(qp_destroy_data->tx_data.phy_id, qp_destroy_data->tx_data.rdev_index,
        qp_destroy_data->tx_data.qpn);
    if (*op_result != 0) {
        hccp_err("qp destroy failed ret[%d].", *op_result);
    }

    return 0;
}

STATIC int ra_rs_typical_qp_modify(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_typical_qp_modify_data *qp_modify_data = (union op_typical_qp_modify_data *)(in_buf +
        sizeof(struct msg_head));
    unsigned int udp_sport = 0;

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_typical_qp_modify_data), sizeof(struct msg_head), rcv_buf_len,
        op_result);

    *op_result = g_ra_rs_ops.typical_qp_modify(qp_modify_data->tx_data.phy_id, qp_modify_data->tx_data.rdev_index,
        qp_modify_data->tx_data.local_qp_info, qp_modify_data->tx_data.remote_qp_info,
        &udp_sport);
    if (*op_result != 0) {
        hccp_err("qp info modify failed ret[%d].", *op_result);
        return 0;
    }

    qp_modify_data = (union op_typical_qp_modify_data *)(out_buf + sizeof(struct msg_head));
    qp_modify_data->rx_data.udp_sport = udp_sport;

    return 0;
}

STATIC int ra_rs_qp_batch_modify(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_qp_batch_modify_data *qp_batch_modify_data = (union op_qp_batch_modify_data *)(in_buf +
        sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_qp_batch_modify_data), sizeof(struct msg_head), rcv_buf_len,
        op_result);
    HCCP_CHECK_PARAM_LEN_RET_HOST(qp_batch_modify_data->tx_data.qpn_num, 0, RA_MAX_BATCH_QP_MODIFY_NUM, op_result);

    *op_result = g_ra_rs_ops.qp_batch_modify(qp_batch_modify_data->tx_data.phy_id,
        qp_batch_modify_data->tx_data.rdev_index, qp_batch_modify_data->tx_data.status,
        qp_batch_modify_data->tx_data.qpn, qp_batch_modify_data->tx_data.qpn_num);
    if (*op_result != 0) {
        hccp_err("qp info modify failed ret[%d].", *op_result);
    }

    return 0;
}

STATIC int ra_rs_qp_connect_async(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_qp_connect_data *qp_connect_data = (union op_qp_connect_data *)(in_buf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_qp_connect_data), sizeof(struct msg_head), rcv_buf_len, op_result);

    *op_result = g_ra_rs_ops.qp_connect_async(qp_connect_data->tx_data.phy_id, qp_connect_data->tx_data.rdev_index,
        qp_connect_data->tx_data.qpn, qp_connect_data->tx_data.fd);
    if (*op_result != 0) {
        hccp_err("qp info async failed ret[%d].", *op_result);
    }

    return 0;
}

STATIC int ra_rs_get_qp_status(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_qp_status_data *qp_status_data = (union op_qp_status_data *)(in_buf + sizeof(struct msg_head));
    struct rs_qp_status_info qp_info = { 0 };

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_qp_status_data), sizeof(struct msg_head), rcv_buf_len, op_result);

    *op_result = g_ra_rs_ops.get_qp_status(qp_status_data->tx_data.phy_id, qp_status_data->tx_data.rdev_index,
        qp_status_data->tx_data.qpn, &qp_info);
    if (*op_result != 0) {
        hccp_err("query qp status async failed ret[%d].", *op_result);
        return 0;
    }

    qp_status_data = (union op_qp_status_data *)(out_buf + sizeof(struct msg_head));
    qp_status_data->rx_data.status = qp_info.status;

    return 0;
}

STATIC int ra_rs_get_qp_info(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_qp_info_data *qp_info_data = (union op_qp_info_data *)(in_buf + sizeof(struct msg_head));
    struct rs_qp_status_info qp_info = { 0 };

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_qp_info_data), sizeof(struct msg_head), rcv_buf_len, op_result);

    *op_result = g_ra_rs_ops.get_qp_status(qp_info_data->tx_data.phy_id, qp_info_data->tx_data.rdev_index,
        qp_info_data->tx_data.qpn, &qp_info);
    if (*op_result != 0) {
        hccp_err("query qp status async failed ret[%d].", *op_result);
        return 0;
    }

    qp_info_data = (union op_qp_info_data *)(out_buf + sizeof(struct msg_head));
    qp_info_data->rx_data.status = qp_info.status;
    qp_info_data->rx_data.udp_sport = qp_info.udp_sport;

    return 0;
}

STATIC int ra_rs_mr_reg(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_mr_reg_data *reg_mr_data = (union op_mr_reg_data *)(in_buf + sizeof(struct msg_head));
    struct rdma_mr_reg_info mr_reg_info = { 0 };

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_mr_reg_data), sizeof(struct msg_head), rcv_buf_len, op_result);

    mr_reg_info.addr = reg_mr_data->tx_data.mr_reg_attr.addr;
    mr_reg_info.len = reg_mr_data->tx_data.mr_reg_attr.len;
    mr_reg_info.access = reg_mr_data->tx_data.mr_reg_attr.access;
    *op_result = g_ra_rs_ops.mr_reg(reg_mr_data->tx_data.phy_id, reg_mr_data->tx_data.rdev_index,
        reg_mr_data->tx_data.qpn, &mr_reg_info);
    if (*op_result != 0) {
        hccp_err("reg_mr failed ret[%d].", *op_result);
        return 0;
    }

    reg_mr_data = (union op_mr_reg_data *)(out_buf + sizeof(struct msg_head));
    reg_mr_data->rx_data.lkey = mr_reg_info.lkey;
    reg_mr_data->rx_data.rkey = mr_reg_info.rkey;

    return 0;
}

STATIC int ra_rs_mr_dereg(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_mr_dereg_data *mr_dereg_data = (union op_mr_dereg_data *)(in_buf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_mr_dereg_data), sizeof(struct msg_head), rcv_buf_len, op_result);

    *op_result = g_ra_rs_ops.mr_dereg(mr_dereg_data->tx_data.phy_id, mr_dereg_data->tx_data.rdev_index,
        mr_dereg_data->tx_data.qpn, mr_dereg_data->tx_data.addr);
    if (*op_result != 0) {
        hccp_err("dereg_mr failed ret[%d].", *op_result);
    }

    return 0;
}

STATIC int ra_rs_typical_mr_reg(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_typical_mr_reg_data *reg_mr_data = (union op_typical_mr_reg_data *)(in_buf + sizeof(struct msg_head));
    struct rdma_mr_reg_info mr_reg_info = { 0 };
    struct ibv_mr *ra_rs_mr_handle = NULL;

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_typical_mr_reg_data), sizeof(struct msg_head),
        rcv_buf_len, op_result);

    mr_reg_info.addr = reg_mr_data->tx_data.mr_reg_attr.addr;
    mr_reg_info.len = reg_mr_data->tx_data.mr_reg_attr.len;
    mr_reg_info.access = reg_mr_data->tx_data.mr_reg_attr.access;
    *op_result = g_ra_rs_ops.register_mr(reg_mr_data->tx_data.phy_id, reg_mr_data->tx_data.rdev_index,
        &mr_reg_info, (void **)&ra_rs_mr_handle);
    if (*op_result != 0) {
        hccp_err("reg_mr failed ret[%d].", *op_result);
        return 0;
    }

    reg_mr_data = (union op_typical_mr_reg_data *)(out_buf + sizeof(struct msg_head));
    reg_mr_data->rx_data.lkey = mr_reg_info.lkey;
    reg_mr_data->rx_data.rkey = mr_reg_info.rkey;

    return 0;
}

STATIC int ra_rs_remap_mr(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_remap_mr_data *op_data = (union op_remap_mr_data *)(in_buf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_remap_mr_data), sizeof(struct msg_head), rcv_buf_len, op_result);
    HCCP_CHECK_PARAM_LEN_RET_HOST(op_data->tx_data.mem_num, 0, REMAP_MR_MAX_NUM, op_result);

    *op_result = g_ra_rs_ops.remap_mr(op_data->tx_data.phy_id, op_data->tx_data.rdev_index, op_data->tx_data.mem_list,
        op_data->tx_data.mem_num);
    if (*op_result) {
        hccp_err("remap_mr failed ret[%d]", *op_result);
    }
    return 0;
}

STATIC int ra_rs_typical_mr_dereg(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_typical_mr_dereg_data *mr_dereg_data =
        (union op_typical_mr_dereg_data *)(in_buf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_typical_mr_dereg_data), sizeof(struct msg_head),
        rcv_buf_len, op_result);

    *op_result = g_ra_rs_ops.typical_deregister_mr(mr_dereg_data->tx_data.phy_id, mr_dereg_data->tx_data.rdev_index,
        mr_dereg_data->tx_data.addr);
    if (*op_result != 0) {
        hccp_err("dereg_mr failed ret[%d].", *op_result);
    }

    return 0;
}

STATIC int ra_rs_send_wr(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    int ret;

    struct send_wr_rsp wr_rsp = { 0 };
    union op_send_wr_data *send_wr_data = (union op_send_wr_data *)(in_buf + sizeof(struct msg_head));
    struct send_wr s_wr = { 0 };

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_send_wr_data), sizeof(struct msg_head), rcv_buf_len, op_result);

    s_wr.buf_num = send_wr_data->tx_data.buf_num;
    s_wr.buf_list = (struct sg_list *)&send_wr_data->tx_data.mem_list[0];
    s_wr.dst_addr = send_wr_data->tx_data.dst_addr;
    s_wr.op = send_wr_data->tx_data.op;
    s_wr.send_flag = send_wr_data->tx_data.send_flags;

    ret = g_ra_rs_ops.send_wr(send_wr_data->tx_data.phy_id, send_wr_data->tx_data.rdev_index, send_wr_data->tx_data.qpn,
        &s_wr, &wr_rsp);
    *op_result = ret;
    if (ret) {
        if (ret == -ENOENT) {
            hccp_warn("not found remote mr_info, need try again");
        } else {
            hccp_err("send wr failed ret[%d].", ret);
        }
    }

    send_wr_data = (union op_send_wr_data *)(out_buf + sizeof(struct msg_head));
    send_wr_data->rx_data.wr_rsp = wr_rsp;

    return 0;
}

STATIC int ra_rs_send_wr_list(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    int ret;
    uint32_t i;
    unsigned int complete_num = 0;
    struct send_wr_rsp *wr_rsp = NULL;
    struct wr_info *wr_list = NULL;
    struct rs_wrlist_base_info base_info = {0};
    union op_send_wrlist_data *send_wrlist = (union op_send_wrlist_data *)(in_buf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_send_wrlist_data), sizeof(struct msg_head), rcv_buf_len, op_result);
    HCCP_CHECK_PARAM_LEN_RET_HOST(send_wrlist->tx_data.send_num, 0,  MAX_WR_NUM, op_result);

    wr_rsp = calloc(send_wrlist->tx_data.send_num, sizeof(struct send_wr_rsp));
    CHK_PRT_RETURN(wr_rsp == NULL, hccp_err("wr_rsp calloc failed."), -ENOMEM);

    wr_list = calloc(send_wrlist->tx_data.send_num, sizeof(struct wr_info));
    if (wr_list == NULL) {
        hccp_err("wr_list calloc failed.");
        ret = -ENOMEM;
        goto alloc_wr_list_fail;
    }

    base_info.phy_id = send_wrlist->tx_data.phy_id;
    base_info.rdev_index = send_wrlist->tx_data.rdev_index;
    base_info.qpn = send_wrlist->tx_data.qpn;
    base_info.key_flag = 0;

    for (i = 0; i < send_wrlist->tx_data.send_num; i++) {
        wr_list[i].op = send_wrlist->tx_data.wrlist[i].op;
        wr_list[i].send_flags = send_wrlist->tx_data.wrlist[i].send_flags;
        wr_list[i].dst_addr = send_wrlist->tx_data.wrlist[i].dst_addr;
        wr_list[i].mem_list.addr = send_wrlist->tx_data.wrlist[i].mem_list.addr;
        wr_list[i].mem_list.len = send_wrlist->tx_data.wrlist[i].mem_list.len;
        wr_list[i].mem_list.lkey = send_wrlist->tx_data.wrlist[i].mem_list.lkey;
    }
    ret = g_ra_rs_ops.send_wr_list(base_info, wr_list, send_wrlist->tx_data.send_num, wr_rsp, &complete_num);
    *op_result = ret;
    if (ret) {
        if (ret == -ENOENT) {
            hccp_warn("not found remote mr_info, need try again");
        } else {
            hccp_err("send wr failed ret[%d].", ret);
        }
    }
    send_wrlist = (union op_send_wrlist_data *)(out_buf + sizeof(struct msg_head));
    send_wrlist->rx_data.complete_num = complete_num;
    ret = memcpy_s(send_wrlist->rx_data.wr_rsp, sizeof(struct send_wr_rsp) * MAX_WR_NUM_V1, wr_rsp,
        complete_num * sizeof(struct send_wr_rsp));
    if (ret) {
        hccp_err("ra_rs_send_wr_list memcpy_s failed, ret[%d]. ", ret);
        ret = -ESAFEFUNC;
        goto copy_wr_rsp_fail;
    }

copy_wr_rsp_fail:
    free(wr_list);
    wr_list = NULL;

alloc_wr_list_fail:
    free(wr_rsp);
    wr_rsp = NULL;
    return 0;
}

STATIC void get_wr_list_v2(struct wr_info *wr_list, union op_send_wrlist_data_v2 *send_wrlist)
{
    uint32_t i;
    for (i = 0; i < send_wrlist->tx_data.send_num; i++) {
        wr_list[i].op = send_wrlist->tx_data.wrlist[i].op;
        wr_list[i].send_flags = send_wrlist->tx_data.wrlist[i].send_flags;
        wr_list[i].dst_addr = send_wrlist->tx_data.wrlist[i].dst_addr;
        wr_list[i].mem_list.addr = send_wrlist->tx_data.wrlist[i].mem_list.addr;
        wr_list[i].mem_list.len = send_wrlist->tx_data.wrlist[i].mem_list.len;
        wr_list[i].mem_list.lkey = send_wrlist->tx_data.wrlist[i].mem_list.lkey;
    }
    return;
}

STATIC int ra_rs_send_wr_list_v2(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    int ret;
    unsigned int complete_num = 0;
    struct wr_info *wr_list = NULL;
    struct send_wr_rsp *wr_rsp = NULL;
    struct rs_wrlist_base_info base_info = {0};
    union op_send_wrlist_data_v2 *send_wrlist = (union op_send_wrlist_data_v2 *)(in_buf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_send_wrlist_data_v2), sizeof(struct msg_head), rcv_buf_len,
        op_result);
    HCCP_CHECK_PARAM_LEN_RET_HOST(send_wrlist->tx_data.send_num, 0, MAX_WR_NUM, op_result);

    wr_rsp = calloc(send_wrlist->tx_data.send_num, sizeof(struct send_wr_rsp));
    CHK_PRT_RETURN(wr_rsp == NULL, hccp_err("wr_rsp calloc failed."), -ENOMEM);

    wr_list = calloc(send_wrlist->tx_data.send_num, sizeof(struct wr_info));
    if (wr_list == NULL) {
        hccp_err("wr_list calloc failed.");
        ret = -ENOMEM;
        goto alloc_wr_list_fail;
    }

    base_info.phy_id = send_wrlist->tx_data.phy_id;
    base_info.rdev_index = send_wrlist->tx_data.rdev_index;
    base_info.qpn = send_wrlist->tx_data.qpn;
    base_info.key_flag = 0;

    get_wr_list_v2(wr_list, send_wrlist);
    ret = g_ra_rs_ops.send_wr_list(base_info, wr_list, send_wrlist->tx_data.send_num, wr_rsp, &complete_num);
    *op_result = ret;
    if (ret) {
        if (ret == -ENOENT) {
            hccp_warn("not found remote mr_info, need try again");
        } else {
            hccp_err("send wr failed ret[%d].", ret);
        }
    }
    send_wrlist = (union op_send_wrlist_data_v2 *)(out_buf + sizeof(struct msg_head));
    send_wrlist->rx_data.complete_num = complete_num;
    ret = memcpy_s(send_wrlist->rx_data.wr_rsp, sizeof(struct send_wr_rsp) * MAX_WR_NUM, wr_rsp,
        complete_num * sizeof(struct send_wr_rsp));
    if (ret) {
        hccp_err("ra_rs_send_wr_list memcpy_s failed, ret[%d]. ", ret);
        ret = -ESAFEFUNC;
        goto copy_wr_rsp_fail;
    }

copy_wr_rsp_fail:
    free(wr_list);
    wr_list = NULL;

alloc_wr_list_fail:
    free(wr_rsp);
    wr_rsp = NULL;
    return 0;
}

STATIC void get_wr_list(struct wr_info *wr_list, union op_send_wrlist_data_ext *send_wrlist)
{
    uint32_t i;
    for (i = 0; i < send_wrlist->tx_data.send_num; i++) {
        wr_list[i].op = send_wrlist->tx_data.wrlist[i].op;
        wr_list[i].send_flags = send_wrlist->tx_data.wrlist[i].send_flags;
        wr_list[i].imm_data = send_wrlist->tx_data.wrlist[i].ext.imm_data;
        wr_list[i].dst_addr = send_wrlist->tx_data.wrlist[i].dst_addr;
        wr_list[i].mem_list.addr = send_wrlist->tx_data.wrlist[i].mem_list.addr;
        wr_list[i].mem_list.len = send_wrlist->tx_data.wrlist[i].mem_list.len;
        wr_list[i].mem_list.lkey = send_wrlist->tx_data.wrlist[i].mem_list.lkey;
        wr_list[i].aux.data_type = send_wrlist->tx_data.wrlist[i].aux.data_type;
        wr_list[i].aux.reduce_type = send_wrlist->tx_data.wrlist[i].aux.reduce_type;
        wr_list[i].aux.notify_offset = send_wrlist->tx_data.wrlist[i].aux.notify_offset;
    }
    return;
}

STATIC int ra_rs_send_wr_list_ext(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    int ret;
    unsigned int complete_num = 0;
    struct send_wr_rsp *wr_rsp = NULL;
    struct wr_info *wr_list = NULL;
    struct rs_wrlist_base_info base_info = {0};
    union op_send_wrlist_data_ext *send_wrlist = (union op_send_wrlist_data_ext *)(in_buf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_send_wrlist_data_ext), sizeof(struct msg_head), rcv_buf_len,
        op_result);
    HCCP_CHECK_PARAM_LEN_RET_HOST(send_wrlist->tx_data.send_num, 0, MAX_WR_NUM, op_result);

    wr_rsp = calloc(send_wrlist->tx_data.send_num, sizeof(struct send_wr_rsp));
    CHK_PRT_RETURN(wr_rsp == NULL, hccp_err("wr_rsp calloc failed."), -ENOMEM);

    wr_list = calloc(send_wrlist->tx_data.send_num, sizeof(struct wr_info));
    if (wr_list == NULL) {
        hccp_err("wr_list calloc failed.");
        ret = -ENOMEM;
        goto alloc_wr_list_fail;
    }

    base_info.phy_id = send_wrlist->tx_data.phy_id;
    base_info.rdev_index = send_wrlist->tx_data.rdev_index;
    base_info.qpn = send_wrlist->tx_data.qpn;
    base_info.key_flag = 0;
    get_wr_list(wr_list, send_wrlist);
    ret = g_ra_rs_ops.send_wr_list(base_info, wr_list, send_wrlist->tx_data.send_num, wr_rsp, &complete_num);
    *op_result = ret;
    if (ret) {
        if (ret == -ENOENT) {
            hccp_warn("not found remote mr_info, need try again");
        } else {
            hccp_err("send wr failed ret[%d].", ret);
        }
    }
    send_wrlist = (union op_send_wrlist_data_ext *)(out_buf + sizeof(struct msg_head));
    send_wrlist->rx_data.complete_num = complete_num;
    ret = memcpy_s(send_wrlist->rx_data.wr_rsp, sizeof(struct send_wr_rsp) * MAX_WR_NUM_V1, wr_rsp,
        complete_num * sizeof(struct send_wr_rsp));
    if (ret) {
        hccp_err("ra_rs_send_wr_list_ext memcpy_s failed, ret[%d]. ", ret);
        ret = -ESAFEFUNC;
        goto copy_wr_rsp_fail;
    }

copy_wr_rsp_fail:
    free(wr_list);
    wr_list = NULL;

alloc_wr_list_fail:
    free(wr_rsp);
    wr_rsp = NULL;
    return 0;
}

STATIC void get_wr_list_ext_v2(struct wr_info *wr_list, union op_send_wrlist_data_ext_v2 *send_wrlist)
{
    uint32_t i;
    for (i = 0; i < send_wrlist->tx_data.send_num; i++) {
        wr_list[i].op = send_wrlist->tx_data.wrlist[i].op;
        wr_list[i].send_flags = send_wrlist->tx_data.wrlist[i].send_flags;
        wr_list[i].imm_data = send_wrlist->tx_data.wrlist[i].ext.imm_data;
        wr_list[i].dst_addr = send_wrlist->tx_data.wrlist[i].dst_addr;
        wr_list[i].mem_list.addr = send_wrlist->tx_data.wrlist[i].mem_list.addr;
        wr_list[i].mem_list.len = send_wrlist->tx_data.wrlist[i].mem_list.len;
        wr_list[i].mem_list.lkey = send_wrlist->tx_data.wrlist[i].mem_list.lkey;
        wr_list[i].aux.data_type = send_wrlist->tx_data.wrlist[i].aux.data_type;
        wr_list[i].aux.reduce_type = send_wrlist->tx_data.wrlist[i].aux.reduce_type;
        wr_list[i].aux.notify_offset = send_wrlist->tx_data.wrlist[i].aux.notify_offset;
    }
    return;
}

STATIC int ra_rs_send_wr_list_ext_v2(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    int ret;
    unsigned int complete_num = 0;
    struct wr_info *wr_list = NULL;
    struct send_wr_rsp *wr_rsp = NULL;
    struct rs_wrlist_base_info base_info = {0};
    union op_send_wrlist_data_ext_v2 *send_wrlist = (union op_send_wrlist_data_ext_v2 *)(in_buf +
        sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_send_wrlist_data_ext_v2), sizeof(struct msg_head), rcv_buf_len,
        op_result);
    HCCP_CHECK_PARAM_LEN_RET_HOST(send_wrlist->tx_data.send_num, 0, MAX_WR_NUM, op_result);

    wr_rsp = calloc(send_wrlist->tx_data.send_num, sizeof(struct send_wr_rsp));
    CHK_PRT_RETURN(wr_rsp == NULL, hccp_err("wr_rsp calloc failed."), -ENOMEM);

    wr_list = calloc(send_wrlist->tx_data.send_num, sizeof(struct wr_info));
    if (wr_list == NULL) {
        hccp_err("wr_list calloc failed.");
        ret = -ENOMEM;
        goto alloc_wr_list_fail;
    }

    base_info.phy_id = send_wrlist->tx_data.phy_id;
    base_info.rdev_index = send_wrlist->tx_data.rdev_index;
    base_info.qpn = send_wrlist->tx_data.qpn;
    base_info.key_flag = 0;
    get_wr_list_ext_v2(wr_list, send_wrlist);
    ret = g_ra_rs_ops.send_wr_list(base_info, wr_list, send_wrlist->tx_data.send_num, wr_rsp, &complete_num);
    *op_result = ret;
    if (ret) {
        if (ret == -ENOENT) {
            hccp_warn("not found remote mr_info, need try again");
        } else {
            hccp_err("send wr failed ret[%d].", ret);
        }
    }
    send_wrlist = (union op_send_wrlist_data_ext_v2 *)(out_buf + sizeof(struct msg_head));
    send_wrlist->rx_data.complete_num = complete_num;
    ret = memcpy_s(send_wrlist->rx_data.wr_rsp, sizeof(struct send_wr_rsp) * MAX_WR_NUM, wr_rsp,
        complete_num * sizeof(struct send_wr_rsp));
    if (ret) {
        hccp_err("ra_rs_send_wr_list_ext_v2 memcpy_s failed, ret[%d]. ", ret);
        ret = -ESAFEFUNC;
        goto copy_wr_rsp_fail;
    }

copy_wr_rsp_fail:
    free(wr_list);
    wr_list = NULL;

alloc_wr_list_fail:
    free(wr_rsp);
    wr_rsp = NULL;
    return 0;
}

STATIC int ra_rs_send_normal_wrlist(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_send_normal_wrlist_data *send_wrlist_out = (union op_send_normal_wrlist_data *)(out_buf +
        sizeof(struct msg_head));
    union op_send_normal_wrlist_data *send_wrlist = (union op_send_normal_wrlist_data *)(in_buf +
        sizeof(struct msg_head));
    struct rs_wrlist_base_info base_info = {0};

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_send_normal_wrlist_data), sizeof(struct msg_head), rcv_buf_len,
        op_result);
    HCCP_CHECK_PARAM_LEN_RET_HOST(send_wrlist->tx_data.send_num, 0, MAX_WR_NUM, op_result);

    base_info.phy_id = send_wrlist->tx_data.phy_id;
    base_info.rdev_index = send_wrlist->tx_data.rdev_index;
    base_info.qpn = send_wrlist->tx_data.qpn;
    base_info.key_flag = 1;

    *op_result = g_ra_rs_ops.send_wr_list(base_info, send_wrlist->tx_data.wrlist, send_wrlist->tx_data.send_num,
        send_wrlist_out->rx_data.wr_rsp, &send_wrlist_out->rx_data.complete_num);
    if (*op_result != 0) {
        hccp_err("send_wr_list failed ret[%d].", *op_result);
    }
    return 0;
}

STATIC int ra_rs_get_notify_ba(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_get_notify_ba_data *get_notify_ba_data = (union op_get_notify_ba_data *)(in_buf + sizeof(struct msg_head));
    struct mr_info info = { 0 };

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_get_notify_ba_data), sizeof(struct msg_head), rcv_buf_len, op_result);

    *op_result = g_ra_rs_ops.get_notify_mr_info(get_notify_ba_data->tx_data.phy_id,
        get_notify_ba_data->tx_data.rdev_index, &info);
    if (*op_result != 0) {
        hccp_err("reg_notify_mr failed ret[%d].", *op_result);
        return 0;
    }

    get_notify_ba_data = (union op_get_notify_ba_data *)(out_buf + sizeof(struct msg_head));
    get_notify_ba_data->rx_data.va = (unsigned long long)info.addr;
    get_notify_ba_data->rx_data.size = info.size;
    get_notify_ba_data->rx_data.access = info.access;
    get_notify_ba_data->rx_data.lkey = info.lkey;

    return 0;
}

STATIC int ra_rs_notify_cfg_set(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_notify_cfg_set_data *set_notify_ba_data =
        (union op_notify_cfg_set_data *)(in_buf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_notify_cfg_set_data), sizeof(struct msg_head), rcv_buf_len,
        op_result);

    *op_result = g_ra_rs_ops.notify_cfg_set(set_notify_ba_data->tx_data.phy_id, set_notify_ba_data->tx_data.va,
        set_notify_ba_data->tx_data.size);
    if (*op_result != 0) {
        hccp_err("notify_cfg_set failed ret[%d].", *op_result);
    }

    return 0;
}

STATIC int ra_rs_notify_cfg_get(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    unsigned long long va = 0;
    unsigned long long size = 0;
    union op_notify_cfg_get_data *get_notify_ba_data =
        (union op_notify_cfg_get_data *)(in_buf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_notify_cfg_get_data), sizeof(struct msg_head), rcv_buf_len,
        op_result);

    *op_result = g_ra_rs_ops.notify_cfg_get(get_notify_ba_data->tx_data.phy_id, &va, &size);
    if (*op_result != 0) {
        hccp_err("notify_cfg_set failed ret[%d].", *op_result);
        return 0;
    }

    get_notify_ba_data = (union op_notify_cfg_get_data *)(out_buf + sizeof(struct msg_head));
    get_notify_ba_data->rx_data.va = va;
    get_notify_ba_data->rx_data.size = size;
    return 0;
}

STATIC int ra_set_pid(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_set_pid_data *set_pid_data = (union op_set_pid_data *)(in_buf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_set_pid_data), sizeof(struct msg_head), rcv_buf_len, op_result);

    hccp_info("ra get pid is [%d]", set_pid_data->tx_data.pid);

    *op_result = g_ra_rs_ops.set_host_pid(set_pid_data->tx_data.phy_id, set_pid_data->tx_data.pid,
        set_pid_data->tx_data.pid_sign);

    hccp_info("ra_set_pid finish");
    return 0;
}

STATIC int ra_rs_close_hdc_session(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    *op_result = 0;
    hccp_info("ra_rs_close_hdc_session finish");
    return 0;
}

STATIC int ra_rs_get_vnic_ip(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    unsigned int vnic_ip = 0;
    union op_get_vnic_ip_data *vnic_ip_data_ret = NULL;
    union op_get_vnic_ip_data *vnic_ip_data = (union op_get_vnic_ip_data *)(in_buf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_get_vnic_ip_data), sizeof(struct msg_head), rcv_buf_len, op_result);

    *op_result = g_ra_rs_ops.get_vnic_ip(vnic_ip_data->tx_data.phy_id, &vnic_ip);
    if (*op_result != 0) {
        hccp_err("rs get vnic ip failed, phy_id %d, ret %d", vnic_ip_data->tx_data.phy_id, *op_result);
        return 0;
    }

    vnic_ip_data_ret = (union op_get_vnic_ip_data *)(out_buf + sizeof(struct msg_head));
    hccp_info("rs get vnic_ip, phy_id %d, vnic_ip 0x%x", vnic_ip_data->tx_data.phy_id, vnic_ip);
    vnic_ip_data_ret->rx_data.vnic_ip = vnic_ip;
    return 0;
}

STATIC int ra_rs_get_vnic_ip_infos_v1(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_get_vnic_ip_infos_data_v1 *vnic_ip_data = (union op_get_vnic_ip_infos_data_v1 *)(in_buf +
        sizeof(struct msg_head));
    union op_get_vnic_ip_infos_data_v1 *vnic_ip_out = (union op_get_vnic_ip_infos_data_v1 *)(out_buf +
        sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_get_vnic_ip_infos_data_v1), sizeof(struct msg_head), rcv_buf_len,
        op_result);
    HCCP_CHECK_PARAM_LEN_RET_HOST(vnic_ip_data->tx_data.num, 0, MAX_IP_INFO_NUM_V1, op_result);

    *op_result = g_ra_rs_ops.get_vnic_ip_infos(vnic_ip_data->tx_data.phy_id, vnic_ip_data->tx_data.type,
        vnic_ip_data->tx_data.ids, vnic_ip_data->tx_data.num, vnic_ip_out->rx_data.infos);

    if (*op_result != 0) {
        hccp_err("rs get vnic ip infos failed, ret %d", *op_result);
    }

    return 0;
}

STATIC int ra_rs_get_vnic_ip_infos(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_get_vnic_ip_infos_data *vnic_ip_data = (union op_get_vnic_ip_infos_data *)(in_buf +
        sizeof(struct msg_head));
    union op_get_vnic_ip_infos_data *vnic_ip_out = (union op_get_vnic_ip_infos_data *)(out_buf +
        sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_get_vnic_ip_infos_data), sizeof(struct msg_head), rcv_buf_len,
        op_result);
    HCCP_CHECK_PARAM_LEN_RET_HOST(vnic_ip_data->tx_data.num, 0, MAX_IP_INFO_NUM, op_result);

    *op_result = g_ra_rs_ops.get_vnic_ip_infos(vnic_ip_data->tx_data.phy_id, vnic_ip_data->tx_data.type,
        vnic_ip_data->tx_data.ids, vnic_ip_data->tx_data.num, vnic_ip_out->rx_data.infos);

    if (*op_result != 0) {
        hccp_err("rs get vnic ip infos failed, ret %d", *op_result);
    }

    return 0;
}

STATIC int ra_rs_get_interface_version(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    unsigned int version = 0;
    union op_get_version_data *version_info_ret = NULL;
    union op_get_version_data *version_info = (union op_get_version_data *)(in_buf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_get_version_data), sizeof(struct msg_head), rcv_buf_len, op_result);

    *op_result = g_ra_rs_ops.get_interface_version(version_info->tx_data.opcode, &version);
    if (*op_result != 0) {
        hccp_err("get_interface_version failed, opcode %d, ret %d", version_info->tx_data.opcode, *op_result);
        return 0;
    }

    version_info_ret = (union op_get_version_data *)(out_buf + sizeof(struct msg_head));
    version_info_ret->rx_data.version = version;
    return 0;
}

STATIC int ra_rs_socket_white_list_add(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_wlist_data *wlist_data = (union op_wlist_data *)(in_buf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_wlist_data), sizeof(struct msg_head), rcv_buf_len, op_result);
    HCCP_CHECK_PARAM_NUM(wlist_data->tx_data.num, MAX_WLIST_NUM);

    *op_result = g_ra_rs_ops.white_list_add(wlist_data->tx_data.rdev_info, wlist_data->tx_data.wlist,
        wlist_data->tx_data.num);
    if (*op_result != 0) {
        hccp_err("white_list_add failed, ret[%d]", *op_result);
    }
    return 0;
}

STATIC int ra_rs_socket_white_list_add_v2(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_wlist_data_v2 *wlist_data = (union op_wlist_data_v2 *)(in_buf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_wlist_data_v2), sizeof(struct msg_head), rcv_buf_len, op_result);
    HCCP_CHECK_PARAM_NUM(wlist_data->tx_data.num, MAX_WLIST_NUM);

    *op_result = g_ra_rs_ops.white_list_add(wlist_data->tx_data.rdev_info, wlist_data->tx_data.wlist,
        wlist_data->tx_data.num);
    if (*op_result != 0) {
        hccp_err("white_list_add failed, ret[%d]", *op_result);
    }
    return 0;
}

STATIC int ra_rs_socket_white_list_del(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_wlist_data *wlist_data = (union op_wlist_data *)(in_buf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_wlist_data), sizeof(struct msg_head), rcv_buf_len, op_result);
    HCCP_CHECK_PARAM_NUM(wlist_data->tx_data.num, MAX_WLIST_NUM);

    *op_result = g_ra_rs_ops.white_list_del(wlist_data->tx_data.rdev_info, wlist_data->tx_data.wlist,
        wlist_data->tx_data.num);
    if (*op_result != 0) {
        hccp_err("white_list_del failed, ret[%d]", *op_result);
    }
    return 0;
}

STATIC int ra_rs_socket_white_list_del_v2(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_wlist_data_v2 *wlist_data = (union op_wlist_data_v2 *)(in_buf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_wlist_data_v2), sizeof(struct msg_head), rcv_buf_len, op_result);
    HCCP_CHECK_PARAM_NUM(wlist_data->tx_data.num, MAX_WLIST_NUM);

    *op_result = g_ra_rs_ops.white_list_del(wlist_data->tx_data.rdev_info, wlist_data->tx_data.wlist,
        wlist_data->tx_data.num);
    if (*op_result != 0) {
        hccp_err("white_list_del failed, ret[%d]", *op_result);
    }
    return 0;
}

STATIC int ra_rs_socket_credit_add(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_accept_credit_data *op_data = (union op_accept_credit_data *)(in_buf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_accept_credit_data), sizeof(struct msg_head), rcv_buf_len, op_result);
    HCCP_CHECK_PARAM_NUM(op_data->tx_data.num, MAX_SOCKET_NUM);

    *op_result = g_ra_rs_ops.accept_credit_add(op_data->tx_data.conn, op_data->tx_data.num,
        op_data->tx_data.credit_limit);
    if (*op_result != 0) {
        hccp_err("accept_credit_add failed, ret[%d]", *op_result);
    }
    return 0;
}

STATIC int ra_rs_get_ifnum(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_ifnum_data *ifnum_data = (union op_ifnum_data *)(in_buf + sizeof(struct msg_head));
    union op_ifnum_data *ifnum_data_return = NULL;
    unsigned int num = 0;
    bool is_all = false;

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_ifnum_data), sizeof(struct msg_head), rcv_buf_len, op_result);
    /* resv bit 31 for is_all for compatibility issue */
    if ((ifnum_data->tx_data.num & RA_RS_GET_ALL_IP_BIT_MASK) != 0) {
        is_all = true;
    }
    *op_result = g_ra_rs_ops.get_ifnum(ifnum_data->tx_data.phy_id, is_all, &num);
    if (*op_result != 0) {
        hccp_err("ra_rs_get_ifnum result ret[%d].", *op_result);
        return 0;
    }

    ifnum_data_return = (union op_ifnum_data *)(out_buf + sizeof(struct msg_head));
    (ifnum_data_return->rx_data).num = num;

    return 0;
}

STATIC int ra_rs_get_ifaddrs(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    int ret;
    union op_ifaddr_data *ifaddr_data_return = NULL;
    union op_ifaddr_data *ifaddr_data = (union op_ifaddr_data *)(in_buf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_ifaddr_data), sizeof(struct msg_head), rcv_buf_len, op_result);
    CHK_PRT_RETURN(ifaddr_data->tx_data.num > MAX_INTERFACE_NUM || ifaddr_data->tx_data.num == 0,
        hccp_err("interface number is invalid, num[%u]", ifaddr_data->tx_data.num), -EINVAL);

    *op_result = g_ra_rs_ops.get_ifaddrs(ifaddr_data->tx_data.ifaddr_infos, &(ifaddr_data->tx_data.num),
        ifaddr_data->tx_data.phy_id);
    if (*op_result != 0) {
        hccp_err("ra_rs_get_ifaddrs result ret[%d].", *op_result);
        return 0;
    }

    ifaddr_data_return = (union op_ifaddr_data *)(out_buf + sizeof(struct msg_head));

    (ifaddr_data_return->rx_data).num = ifaddr_data->tx_data.num;
    ret = memcpy_s((ifaddr_data_return->rx_data).ifaddr_infos, sizeof(struct ifaddr_info) * MAX_INTERFACE_NUM,
        (ifaddr_data->tx_data).ifaddr_infos, sizeof(struct ifaddr_info) * (ifaddr_data->tx_data).num);
    CHK_PRT_RETURN(ret, hccp_err("ra_rs_get_sockets memcpy_s failed, ret[%d]. ", ret), -ESAFEFUNC);

    return 0;
}

STATIC int ra_rs_get_ifaddrs_v2(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_ifaddr_data_v2 *ifaddr_data = (union op_ifaddr_data_v2 *)(in_buf + sizeof(struct msg_head));
    union op_ifaddr_data_v2 *ifaddr_data_return = NULL;
    bool is_all = false;
    int ret;

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_ifaddr_data_v2), sizeof(struct msg_head), rcv_buf_len, op_result);

    /* resv bit 31 for is_all for compatibility issue */
    if ((ifaddr_data->tx_data.num & RA_RS_GET_ALL_IP_BIT_MASK) != 0) {
        is_all = true;
    }
    ifaddr_data->tx_data.num = ifaddr_data->tx_data.num & (~RA_RS_GET_ALL_IP_BIT_MASK);
    CHK_PRT_RETURN(ifaddr_data->tx_data.num > MAX_INTERFACE_NUM || ifaddr_data->tx_data.num == 0,
        hccp_err("interface number of op_ifaddr_data_v2 is invalid, num[%u]", ifaddr_data->tx_data.num), -EINVAL);

    *op_result = g_ra_rs_ops.get_ifaddrs_v2(ifaddr_data->tx_data.interface_infos, &(ifaddr_data->tx_data.num),
        ifaddr_data->tx_data.phy_id, is_all);
    if (*op_result != 0) {
        hccp_err("ra_rs_get_ifaddrs_v2 result ret[%d].", *op_result);
        return 0;
    }

    ifaddr_data_return = (union op_ifaddr_data_v2 *)(out_buf + sizeof(struct msg_head));

    (ifaddr_data_return->rx_data).num = ifaddr_data->tx_data.num;
    ret = memcpy_s((ifaddr_data_return->rx_data).interface_infos, sizeof(struct interface_info) * MAX_INTERFACE_NUM,
        (ifaddr_data->tx_data).interface_infos, sizeof(struct interface_info) * (ifaddr_data->tx_data).num);
    CHK_PRT_RETURN(ret, hccp_err("ra_rs_get_ifaddrs_v2 memcpy_s failed, ret[%d].", ret), -ESAFEFUNC);

    return 0;
}

STATIC int ra_rs_set_qp_attr_qos(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_set_qp_attr_qos_data *attr_qos_data = (union op_set_qp_attr_qos_data *)(in_buf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_set_qp_attr_qos_data), sizeof(struct msg_head), rcv_buf_len,
        op_result);

    *op_result = g_ra_rs_ops.set_qp_attr_qos(attr_qos_data->tx_data.phy_id, attr_qos_data->tx_data.rdev_index,
        attr_qos_data->tx_data.qpn, &(attr_qos_data->tx_data.qos_attr));
    if (*op_result != 0) {
        hccp_err("set_qp_attr_qos failed ret[%d].", *op_result);
    }

    return 0;
}

STATIC int ra_rs_set_qp_attr_timeout(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_set_qp_attr_timeout_data *attr_time_data =
        (union op_set_qp_attr_timeout_data *)(in_buf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_set_qp_attr_timeout_data), sizeof(struct msg_head), rcv_buf_len,
        op_result);

    *op_result = g_ra_rs_ops.set_qp_attr_timeout(attr_time_data->tx_data.phy_id, attr_time_data->tx_data.rdev_index,
        attr_time_data->tx_data.qpn, &(attr_time_data->tx_data.timeout));
    if (*op_result != 0) {
        hccp_err("set_qp_attr_timeout failed ret[%d].", *op_result);
    }

    return 0;
}

STATIC int ra_rs_set_qp_attr_retry_cnt(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_set_qp_attr_retry_cnt_data *attr_retry_cnt_data =
        (union op_set_qp_attr_retry_cnt_data *)(in_buf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_set_qp_attr_retry_cnt_data), sizeof(struct msg_head), rcv_buf_len,
        op_result);

    *op_result =
        g_ra_rs_ops.set_qp_attr_retry_cnt(attr_retry_cnt_data->tx_data.phy_id, attr_retry_cnt_data->tx_data.rdev_index,
        attr_retry_cnt_data->tx_data.qpn, &(attr_retry_cnt_data->tx_data.retry_cnt));
    if (*op_result != 0) {
        hccp_err("set_qp_attr_retry_cnt failed ret[%d].", *op_result);
    }

    return 0;
}

STATIC int ra_rs_get_cqe_err_info(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    int ret;
    struct cqe_err_info info = { 0 };
    union op_get_cqe_err_info_data *cqe_err_info_ret = NULL;

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_get_cqe_err_info_data), sizeof(struct msg_head), rcv_buf_len,
        op_result);

    *op_result = g_ra_rs_ops.get_cqe_err_info(&info);
    if (*op_result != 0) {
        hccp_err("get_cqe_err_info failed, ret %d", *op_result);
        return 0;
    }

    cqe_err_info_ret = (union op_get_cqe_err_info_data *)(out_buf + sizeof(struct msg_head));
    ret = memcpy_s(&cqe_err_info_ret->rx_data.info, sizeof(struct cqe_err_info), &info, sizeof(struct cqe_err_info));
    CHK_PRT_RETURN(ret, hccp_err("ra_rs_get_cqe_err_info memcpy_s failed, ret[%d]. ", ret), -ESAFEFUNC);
    return 0;
}

STATIC int ra_rs_get_lite_support(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_lite_support_data *lite_support_data = (union op_lite_support_data *)(in_buf + sizeof(struct msg_head));
    union op_lite_support_data *lite_support_out = (union op_lite_support_data *)(out_buf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_lite_support_data), sizeof(struct msg_head), rcv_buf_len, op_result);

    *op_result = g_ra_rs_ops.get_lite_support(lite_support_data->tx_data.phy_id,
        lite_support_data->tx_data.rdev_index,
        &lite_support_out->rx_data.support_lite);
    if (*op_result != 0) {
        hccp_err("get_lite_support failed ret[%d].", *op_result);
    }

    return 0;
}

STATIC int ra_rs_get_lite_rdev_cap(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_lite_rdev_cap_data *lite_rdev_cap_data = (union op_lite_rdev_cap_data *)(in_buf + sizeof(struct msg_head));
    union op_lite_rdev_cap_data *lite_rdev_cap_out = (union op_lite_rdev_cap_data *)(out_buf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_lite_rdev_cap_data), sizeof(struct msg_head), rcv_buf_len, op_result);

    *op_result = g_ra_rs_ops.get_lite_rdev_cap(lite_rdev_cap_data->tx_data.phy_id,
        lite_rdev_cap_data->tx_data.rdev_index,
        (void *)&lite_rdev_cap_out->rx_data.resp);
    if (*op_result != 0) {
        hccp_err("get_lite_rdev_cap failed ret[%d].", *op_result);
    }

    return 0;
}

STATIC int ra_rs_get_lite_qp_cq_attr(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_lite_qp_cq_attr_data *lite_qp_cq_attr_data =
        (union op_lite_qp_cq_attr_data *)(in_buf + sizeof(struct msg_head));
    union op_lite_qp_cq_attr_data *lite_qp_cq_attr_out =
        (union op_lite_qp_cq_attr_data *)(out_buf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(
        sizeof(union op_lite_qp_cq_attr_data), sizeof(struct msg_head), rcv_buf_len, op_result);

    *op_result = g_ra_rs_ops.get_lite_qp_cq_attr(lite_qp_cq_attr_data->tx_data.phy_id,
        lite_qp_cq_attr_data->tx_data.rdev_index,
        lite_qp_cq_attr_data->tx_data.qpn,
        (void *)&lite_qp_cq_attr_out->rx_data.resp);
    if (*op_result != 0) {
        hccp_err("get_lite_qp_cq_attr failed ret[%d].", *op_result);
    }

    return 0;
}

STATIC int ra_rs_get_lite_connected_info(
    char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_lite_connected_info_data *lite_connected_info_data =
        (union op_lite_connected_info_data *)(in_buf + sizeof(struct msg_head));
    union op_lite_connected_info_data *lite_connected_info_out =
        (union op_lite_connected_info_data *)(out_buf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(
        sizeof(union op_lite_connected_info_data), sizeof(struct msg_head), rcv_buf_len, op_result);

    *op_result = g_ra_rs_ops.get_lite_connected_info(lite_connected_info_data->tx_data.phy_id,
        lite_connected_info_data->tx_data.rdev_index,
        lite_connected_info_data->tx_data.qpn,
        (void *)&lite_connected_info_out->rx_data.resp);
    if (*op_result != 0) {
        hccp_err("get_lite_connected_info failed ret[%d].", *op_result);
    }

    return 0;
}

STATIC int ra_rs_get_lite_mem_attr(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_lite_mem_attr_data *lite_mem_attr_data =
        (union op_lite_mem_attr_data *)(in_buf + sizeof(struct msg_head));
    union op_lite_mem_attr_data *lite_mem_attr_out =
        (union op_lite_mem_attr_data *)(out_buf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(
        sizeof(union op_lite_mem_attr_data), sizeof(struct msg_head), rcv_buf_len, op_result);

    *op_result = g_ra_rs_ops.get_lite_mem_attr(lite_mem_attr_data->tx_data.phy_id,
        lite_mem_attr_data->tx_data.rdev_index,
        lite_mem_attr_data->tx_data.qpn,
        (void *)&lite_mem_attr_out->rx_data.resp);
    if (*op_result != 0) {
        hccp_err("get_lite_mem_attr failed ret[%d].", *op_result);
    }

    return 0;
}

STATIC int ra_rs_ping_init(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_ping_init_data *ping_data_out = (union op_ping_init_data *)(out_buf + sizeof(struct msg_head));
    union op_ping_init_data *ping_data = (union op_ping_init_data *)(in_buf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_ping_init_data), sizeof(struct msg_head), rcv_buf_len, op_result);

    *op_result = g_ra_rs_ops.ping_init(&ping_data->tx_data.attr, &ping_data_out->rx_data.info,
        &ping_data_out->rx_data.dev_index);
    if (*op_result != 0) {
        hccp_err("ping_init failed ret[%d].", *op_result);
    }
    // only negative return value will be parsed
    if (*op_result > 0) {
        *op_result = -*op_result;
    }

    return 0;
}

STATIC int ra_rs_ping_target_add(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_ping_add_data *ping_data = (union op_ping_add_data *)(in_buf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_ping_add_data), sizeof(struct msg_head), rcv_buf_len, op_result);

    *op_result = g_ra_rs_ops.ping_target_add(&ping_data->tx_data.rdev, &ping_data->tx_data.target);
    if (*op_result != 0) {
        hccp_err("ping_target_add failed ret[%d].", *op_result);
    }
    // only negative return value will be parsed
    if (*op_result > 0) {
        *op_result = -*op_result;
    }

    return 0;
}

STATIC int ra_rs_ping_task_start(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_ping_start_data *ping_data = (union op_ping_start_data *)(in_buf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_ping_start_data), sizeof(struct msg_head), rcv_buf_len, op_result);

    *op_result = g_ra_rs_ops.ping_task_start(&ping_data->tx_data.rdev, &ping_data->tx_data.attr);
    if (*op_result != 0) {
        hccp_err("ping_task_start failed ret[%d].", *op_result);
    }
    // only negative return value will be parsed
    if (*op_result > 0) {
        *op_result = -*op_result;
    }

    return 0;
}

STATIC int ra_rs_ping_get_results(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_ping_results_data *ping_data_out = (union op_ping_results_data *)(out_buf + sizeof(struct msg_head));
    union op_ping_results_data *ping_data = (union op_ping_results_data *)(in_buf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_ping_results_data), sizeof(struct msg_head), rcv_buf_len, op_result);
    HCCP_CHECK_PARAM_LEN_RET_HOST(ping_data->tx_data.num, 0, RA_MAX_PING_TARGET_NUM, op_result);

    *op_result = g_ra_rs_ops.ping_get_results(&ping_data->tx_data.rdev, ping_data->tx_data.target,
        &ping_data->tx_data.num, ping_data_out->rx_data.target);
    // caller needs to retry, degrade log level
    if (*op_result == -EAGAIN) {
        hccp_warn("ping_get_results unsuccessful, ret[%d].", *op_result);
    } else if (*op_result != 0) {
        hccp_err("ping_get_results failed, ret[%d].", *op_result);
    }
    // only negative return value will be parsed
    if (*op_result > 0) {
        *op_result = -*op_result;
        return 0;
    }
    ping_data_out->rx_data.num = ping_data->tx_data.num;

    return 0;
}

STATIC int ra_rs_ping_task_stop(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_ping_stop_data *ping_data = (union op_ping_stop_data *)(in_buf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_ping_stop_data), sizeof(struct msg_head), rcv_buf_len, op_result);

    *op_result = g_ra_rs_ops.ping_task_stop(&ping_data->tx_data.rdev);
    if (*op_result != 0) {
        hccp_err("ping_task_stop failed ret[%d].", *op_result);
    }
    // only negative return value will be parsed
    if (*op_result > 0) {
        *op_result = -*op_result;
    }

    return 0;
}

STATIC int ra_rs_ping_target_del(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_ping_del_data *ping_data = (union op_ping_del_data *)(in_buf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_ping_del_data), sizeof(struct msg_head), rcv_buf_len, op_result);
    HCCP_CHECK_PARAM_LEN_RET_HOST(ping_data->tx_data.num, 0, RA_MAX_PING_TARGET_NUM, op_result);

    *op_result = g_ra_rs_ops.ping_target_del(&ping_data->tx_data.rdev, ping_data->tx_data.target,
        &ping_data->tx_data.num);
    if (*op_result != 0) {
        hccp_err("ping_target_del failed ret[%d].", *op_result);
    }
    // only negative return value will be parsed
    if (*op_result > 0) {
        *op_result = -*op_result;
    }

    return 0;
}

STATIC int ra_rs_ping_deinit(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_ping_deinit_data *ping_data = (union op_ping_deinit_data *)(in_buf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_ping_deinit_data), sizeof(struct msg_head), rcv_buf_len, op_result);

    *op_result = g_ra_rs_ops.ping_deinit(&ping_data->tx_data.rdev);
    if (*op_result != 0) {
        hccp_err("ping_deinit failed ret[%d].", *op_result);
    }
    // only negative return value will be parsed
    if (*op_result > 0) {
        *op_result = -*op_result;
    }

    return 0;
}

STATIC int ra_rs_get_cqe_err_info_num(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_get_cqe_err_info_num_data *cqe_err_info_num =
        (union op_get_cqe_err_info_num_data *)(in_buf + sizeof(struct msg_head));
    unsigned int num;

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_get_cqe_err_info_num_data), sizeof(struct msg_head), rcv_buf_len,
        op_result);

    *op_result = g_ra_rs_ops.get_cqe_err_info_num(cqe_err_info_num->tx_data.phy_id,
        cqe_err_info_num->tx_data.rdev_index, &num);
    if (*op_result != 0) {
        hccp_err("get_cqe_err_info_num failed, ret %d", *op_result);
        return 0;
    }

    cqe_err_info_num = (union op_get_cqe_err_info_num_data *)(out_buf + sizeof(struct msg_head));
    cqe_err_info_num->rx_data.num = num;

    return 0;
}

STATIC int ra_rs_get_cqe_err_info_list(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_get_cqe_err_info_list_data *cqe_err_info_list =
        (union op_get_cqe_err_info_list_data *)(in_buf + sizeof(struct msg_head));
    union op_get_cqe_err_info_list_data *cqe_err_info_list_ret =
        (union op_get_cqe_err_info_list_data *)(out_buf + sizeof(struct msg_head));
    unsigned int num;

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_get_cqe_err_info_list_data), sizeof(struct msg_head), rcv_buf_len,
        op_result);
    HCCP_CHECK_PARAM_LEN_RET_HOST(cqe_err_info_list->tx_data.num, 0, CQE_ERR_INFO_MAX_NUM, op_result);

    num = cqe_err_info_list->tx_data.num;
    *op_result = g_ra_rs_ops.get_cqe_err_info_list(cqe_err_info_list->tx_data.phy_id,
        cqe_err_info_list->tx_data.rdev_index, cqe_err_info_list_ret->rx_data.info_list, &num);
    if (*op_result != 0) {
        hccp_err("get_cqe_err_info_list failed, ret %d", *op_result);
        return 0;
    }

    cqe_err_info_list_ret->rx_data.num = num;

    return 0;
}

STATIC int ra_rs_get_tls_enable(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_get_tls_enable_data *op_data_ret = (union op_get_tls_enable_data *)(out_buf + sizeof(struct msg_head));
    union op_get_tls_enable_data *op_data = (union op_get_tls_enable_data *)(in_buf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_get_tls_enable_data), sizeof(struct msg_head), rcv_buf_len,
        op_result);

    *op_result = g_ra_rs_ops.get_tls_enable(op_data->tx_data.phy_id, &op_data_ret->rx_data.tls_enable);
    if (*op_result != 0) {
        hccp_err("get_tls_enable failed, ret %d", *op_result);
    }
    return 0;
}

STATIC int ra_rs_get_sec_random(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_get_sec_random_data *op_data_ret = (union op_get_sec_random_data *)(out_buf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_get_sec_random_data), sizeof(struct msg_head), rcv_buf_len,
        op_result);

    *op_result = g_ra_rs_ops.get_sec_random((int *)&op_data_ret->rx_data.value);
    if (*op_result != 0) {
        hccp_err("get sec random failed, ret %d", *op_result);
    }
    return 0;
}

#define US_PRE_SECOND 1000000
#define US_PRE_MSECOND 1000
#define MS_PRE_SECOND 1000

STATIC void ra_time_interval(struct timeval *end_time, struct timeval *start_time, long *msec)
{
    /* if low position is sufficient, then borrow one from the high position */
    if (end_time->tv_usec < start_time->tv_usec) {
        end_time->tv_sec -= 1;
        end_time->tv_usec += US_PRE_SECOND;
    }

    *msec = (end_time->tv_sec - start_time->tv_sec) * MS_PRE_SECOND +
        (end_time->tv_usec - start_time->tv_usec) / US_PRE_MSECOND;
}

#define OP_TYPE_CFG 0
#define OP_TYPE_QUERY 1

STATIC void ra_get_op_right(struct ra_hdc_op_sec *op_sec, unsigned int opcode, unsigned int async_req_id, int *right)
{
    long time_interval = 0;
    struct timeval t_cur;
    int exe_right;
    int ret;

    ret = gettimeofday(&t_cur, NULL);
    if (ret) {
        *right = OP_RIGHT_QUERY_ERR;
        hccp_err("ra gettimeofday failed ret[%d].", ret);
        return;
    }

    ra_time_interval(&t_cur, &op_sec->t_last, &time_interval);
    op_sec->t_last = (time_interval < 0) ? t_cur : op_sec->t_last;
    time_interval = (time_interval < 0) ? 0 : time_interval;
    op_sec->t_last = (time_interval == 0) ? op_sec->t_last : t_cur;
    op_sec->token_num += time_interval * TOKEN_RATE;

    if (op_sec->token_num == 0) {
        *right = HAVE_NOT_OP_RIGHT;
        hccp_err("ra handle have not op right. opcode[%u].", opcode);
        return;
    }

    exe_right = HAVE_OP_RIGHT;

    op_sec->token_num = op_sec->token_num - 1;
    op_sec->token_num = (op_sec->token_num > BUCKET_DEPTH) ? BUCKET_DEPTH : op_sec->token_num;
    if (op_sec->is_async_op) {
        hccp_dbg("opcode[%u], req_id[%u], exe_right[%d], token_num[%llu], cfg_op_num[%u]",
            opcode, async_req_id, exe_right, op_sec->token_num, op_sec->cfg_op_num);
    } else {
        hccp_dbg("opcode[%u], exe_right[%d], token_num[%llu], cfg_op_num[%u]",
            opcode, exe_right, op_sec->token_num, op_sec->cfg_op_num);
    }

    *right = exe_right;
    return;
}

struct ra_op_handle g_ra_op_handle[] = {
    {RA_RS_SOCKET_CONN, ra_rs_socket_batch_connect, sizeof(union op_socket_connect_data)},
    {RA_RS_SOCKET_CLOSE, ra_rs_socket_batch_close, sizeof(union op_socket_close_data)},
    {RA_RS_SOCKET_ABORT, ra_rs_socket_batch_abort, sizeof(union op_socket_connect_data)},
    {RA_RS_SOCKET_LISTEN_START, ra_rs_socket_listen_start, sizeof(union op_socket_listen_data)},
    {RA_RS_SOCKET_LISTEN_STOP, ra_rs_socket_listen_stop, sizeof(union op_socket_listen_data)},
    {RA_RS_GET_SOCKET, ra_rs_get_sockets, sizeof(union op_socket_info_data)},
    {RA_RS_SOCKET_SEND, ra_rs_socket_send, sizeof(union op_socket_send_data)},
    {RA_RS_SOCKET_RECV, ra_rs_socket_recv, sizeof(union op_socket_recv_data)},
    {RA_RS_QP_CREATE, ra_rs_qp_create, sizeof(union op_qp_create_data)},
    {RA_RS_QP_CREATE_WITH_ATTRS, ra_rs_qp_create_with_attrs, sizeof(union op_qp_create_with_attrs_data)},
    {RA_RS_AI_QP_CREATE, ra_rs_ai_qp_create, sizeof(union op_ai_qp_create_data)},
    {RA_RS_AI_QP_CREATE_WITH_ATTRS, ra_rs_ai_qp_create_with_data, sizeof(union op_ai_qp_create_with_attrs_data)},
    {RA_RS_TYPICAL_QP_CREATE, ra_rs_typical_qp_create, sizeof(union op_typical_qp_create_data)},
    {RA_RS_QP_DESTROY, ra_rs_qp_destroy, sizeof(union op_qp_destroy_data)},
    {RA_RS_TYPICAL_QP_MODIFY, ra_rs_typical_qp_modify, sizeof(union op_typical_qp_modify_data)},
    {RA_RS_QP_BATCH_MODIFY, ra_rs_qp_batch_modify, sizeof(union op_qp_batch_modify_data)},
    {RA_RS_QP_CONNECT, ra_rs_qp_connect_async, sizeof(union op_qp_connect_data)},
    {RA_RS_QP_STATUS, ra_rs_get_qp_status, sizeof(union op_qp_status_data)},
    {RA_RS_QP_INFO, ra_rs_get_qp_info, sizeof(union op_qp_info_data)},
    {RA_RS_MR_REG, ra_rs_mr_reg, sizeof(union op_mr_reg_data)},
    {RA_RS_MR_DEREG, ra_rs_mr_dereg, sizeof(union op_mr_dereg_data)},
    {RA_RS_TYPICAL_MR_REG, ra_rs_typical_mr_reg, sizeof(union op_typical_mr_reg_data)},
    {RA_RS_REMAP_MR, ra_rs_remap_mr, sizeof(union op_remap_mr_data)},
    {RA_RS_TYPICAL_MR_DEREG, ra_rs_typical_mr_dereg, sizeof(union op_typical_mr_dereg_data)},
    {RA_RS_SEND_WR, ra_rs_send_wr, sizeof(union op_send_wr_data)},
    {RA_RS_GET_NOTIFY_BA, ra_rs_get_notify_ba, sizeof(union op_get_notify_ba_data)},
    {RA_RS_SOCKET_INIT, ra_rs_socket_init, sizeof(union op_socket_init_data)},
    {RA_RS_SOCKET_DEINIT, ra_rs_socket_deinit, sizeof(union op_socket_deinit_data)},
    {RA_RS_RDEV_INIT, ra_rs_rdev_init, sizeof(union op_rdev_init_data)},
    {RA_RS_RDEV_INIT_WITH_BACKUP, ra_rs_rdev_init_with_backup, sizeof(union op_rdev_init_with_backup_data)},
    {RA_RS_RDEV_GET_PORT_STATUS, ra_rs_rdev_get_port_status, sizeof(union op_rdev_get_port_status_data)},
    {RA_RS_RDEV_DEINIT, ra_rs_rdev_deinit, sizeof(union op_rdev_deinit_data)},
    {RA_RS_WLIST_ADD, ra_rs_socket_white_list_add, sizeof(union op_wlist_data)},
    {RA_RS_WLIST_ADD_V2, ra_rs_socket_white_list_add_v2, sizeof(union op_wlist_data_v2)},
    {RA_RS_WLIST_DEL, ra_rs_socket_white_list_del, sizeof(union op_wlist_data)},
    {RA_RS_WLIST_DEL_V2, ra_rs_socket_white_list_del_v2, sizeof(union op_wlist_data_v2)},
    {RA_RS_ACCEPT_CREDIT_ADD, ra_rs_socket_credit_add, sizeof(union op_accept_credit_data)},
    {RA_RS_GET_IFNUM, ra_rs_get_ifnum, sizeof(union op_ifnum_data)},
    {RA_RS_GET_IFADDRS, ra_rs_get_ifaddrs, sizeof(union op_ifaddr_data)},
    {RA_RS_GET_IFADDRS_V2, ra_rs_get_ifaddrs_v2, sizeof(union op_ifaddr_data_v2)},
    {RA_RS_GET_INTERFACE_VERSION, ra_rs_get_interface_version, sizeof(union op_get_version_data)},
    {RA_RS_SEND_WRLIST, ra_rs_send_wr_list, sizeof(union op_send_wrlist_data)},
    {RA_RS_SEND_WRLIST_V2, ra_rs_send_wr_list_v2, sizeof(union op_send_wrlist_data_v2)},
    {RA_RS_SEND_WRLIST_EXT, ra_rs_send_wr_list_ext, sizeof(union op_send_wrlist_data_ext)},
    {RA_RS_SEND_WRLIST_EXT_V2, ra_rs_send_wr_list_ext_v2, sizeof(union op_send_wrlist_data_ext_v2)},
    {RA_RS_SEND_NORMAL_WRLIST, ra_rs_send_normal_wrlist, sizeof(union op_send_normal_wrlist_data)},
    {RA_RS_SET_TSQP_DEPTH, ra_rs_set_tsqp_depth, sizeof(union op_set_tsqp_depth_data)},
    {RA_RS_GET_TSQP_DEPTH, ra_rs_get_tsqp_depth, sizeof(union op_get_tsqp_depth_data)},
    {RA_RS_HDC_SESSION_CLOSE, ra_rs_close_hdc_session, sizeof(union op_socket_recv_data)},
    {RA_RS_GET_VNIC_IP, ra_rs_get_vnic_ip, sizeof(union op_get_vnic_ip_data)},
    {RA_RS_GET_VNIC_IP_INFOS_V1, ra_rs_get_vnic_ip_infos_v1, sizeof(union op_get_vnic_ip_infos_data_v1)},
    {RA_RS_GET_VNIC_IP_INFOS, ra_rs_get_vnic_ip_infos, sizeof(union op_get_vnic_ip_infos_data)},
    {RA_RS_NOTIFY_CFG_SET, ra_rs_notify_cfg_set, sizeof(union op_notify_cfg_set_data)},
    {RA_RS_NOTIFY_CFG_GET, ra_rs_notify_cfg_get, sizeof(union op_notify_cfg_get_data)},
    {RA_RS_SET_PID, ra_set_pid, sizeof(union op_set_pid_data)},
    {RA_RS_SET_QP_ATTR_QOS, ra_rs_set_qp_attr_qos, sizeof(union op_set_qp_attr_qos_data)},
    {RA_RS_SET_QP_ATTR_TIMEOUT, ra_rs_set_qp_attr_timeout, sizeof(union op_set_qp_attr_timeout_data)},
    {RA_RS_SET_QP_ATTR_RETRY_CNT, ra_rs_set_qp_attr_retry_cnt, sizeof(union op_set_qp_attr_retry_cnt_data)},
    {RA_RS_GET_CQE_ERR_INFO, ra_rs_get_cqe_err_info, sizeof(union op_get_cqe_err_info_data)},
    {RA_RS_GET_CQE_ERR_INFO_NUM, ra_rs_get_cqe_err_info_num, sizeof(union op_get_cqe_err_info_num_data)},
    {RA_RS_GET_CQE_ERR_INFO_LIST, ra_rs_get_cqe_err_info_list, sizeof(union op_get_cqe_err_info_list_data)},
    {RA_RS_GET_LITE_SUPPORT, ra_rs_get_lite_support, sizeof(union op_lite_support_data)},
    {RA_RS_GET_LITE_RDEV_CAP, ra_rs_get_lite_rdev_cap, sizeof(union op_lite_rdev_cap_data)},
    {RA_RS_GET_LITE_QP_CQ_ATTR, ra_rs_get_lite_qp_cq_attr, sizeof(union op_lite_qp_cq_attr_data)},
    {RA_RS_GET_LITE_CONNECTED_INFO, ra_rs_get_lite_connected_info, sizeof(union op_lite_connected_info_data)},
    {RA_RS_GET_LITE_MEM_ATTR, ra_rs_get_lite_mem_attr, sizeof(union op_lite_mem_attr_data)},
    {RA_RS_PING_INIT, ra_rs_ping_init, sizeof(union op_ping_init_data)},
    {RA_RS_PING_ADD, ra_rs_ping_target_add, sizeof(union op_ping_add_data)},
    {RA_RS_PING_START, ra_rs_ping_task_start, sizeof(union op_ping_start_data)},
    {RA_RS_PING_GET_RESULTS, ra_rs_ping_get_results, sizeof(union op_ping_results_data)},
    {RA_RS_PING_STOP, ra_rs_ping_task_stop, sizeof(union op_ping_stop_data)},
    {RA_RS_PING_DEL, ra_rs_ping_target_del, sizeof(union op_ping_del_data)},
    {RA_RS_PING_DEINIT, ra_rs_ping_deinit, sizeof(union op_ping_deinit_data)},
#ifdef CONFIG_TLV
    {RA_RS_TLV_INIT, ra_rs_tlv_init, sizeof(union op_tlv_init_data)},
    {RA_RS_TLV_DEINIT, ra_rs_tlv_deinit, sizeof(union op_tlv_deinit_data)},
    {RA_RS_TLV_REQUEST, ra_rs_tlv_request, sizeof(union op_tlv_request_data)},
#endif
    {RA_RS_GET_TLS_ENABLE, ra_rs_get_tls_enable, sizeof(union op_get_tls_enable_data)},
    {RA_RS_GET_SEC_RANDOM, ra_rs_get_sec_random, sizeof(union op_get_sec_random_data)},
    {RA_RS_ASYNC_HDC_SESSION_CONNECT, ra_rs_async_hdc_session_connect, sizeof(union op_async_hdc_connect_data)},
    {RA_RS_ASYNC_HDC_SESSION_CLOSE, ra_rs_async_hdc_session_close, sizeof(union op_async_hdc_close_data)},
};

STATIC int ra_check_param(char *recv_buf, int rcv_buf_len, char **send_buf, int *snd_buf_len, int *param_check_result)
{
    int i;
    int ret = 0;
    struct msg_head *recv_msg_head = (struct msg_head *)recv_buf;
    int num = sizeof(g_ra_op_handle) / sizeof(g_ra_op_handle[0]);
    unsigned int data_size = 0;

    *param_check_result = 1;
    if (rcv_buf_len < (int)sizeof(struct msg_head)) { // check rcv_buf_len
        hccp_err("rcv_buf_len[%d] form ra is invalid", rcv_buf_len);
        ret = op_msg_err(send_buf, recv_msg_head, snd_buf_len, RECV_BUF_LEN_INVALID);
        return ret;
    }

    if (((recv_msg_head->msg_data_len + sizeof(struct msg_head)) != (unsigned int)rcv_buf_len) ||
        (recv_msg_head->opcode >= RA_RS_OP_MAX_NUM ||
        ((recv_msg_head->opcode < RA_RS_HDC_SESSION_CLOSE) && (recv_msg_head->opcode >= RA_RS_EXTER_OP_MAX_NUM)))) {
        hccp_err("rcv data incomplete, because rcv_buf_len[%d] != msg_head_len[%u] + msg_data_len[%u] \
            or opcode[%u] is wrong, RA_RS_OP_MAX_NUM:[%d], RA_RS_EXTER_OP_MAX_NUM:[%d]",
            rcv_buf_len, sizeof(struct msg_head), recv_msg_head->msg_data_len, recv_msg_head->opcode,
            RA_RS_OP_MAX_NUM, RA_RS_EXTER_OP_MAX_NUM);
        ret = op_msg_err(send_buf, recv_msg_head, snd_buf_len, HAVE_OP_RIGHT);
        return ret;
    }
    for (i = 0; i < num; i++) {
        if (g_ra_op_handle[i].opcode == recv_msg_head->opcode) {
            data_size = g_ra_op_handle[i].data_size;
            break;
        }
    }
    if (recv_msg_head->opcode != RA_RS_SOCKET_RECV && recv_msg_head->msg_data_len != data_size) {
        hccp_err("rcv data incomplete. because msg_data_len[%d] != op_data_len[%u]",
            recv_msg_head->msg_data_len, data_size);
        ret = op_msg_err(send_buf, recv_msg_head, snd_buf_len, RECV_BUF_LEN_INVALID);
        return ret;
    }
    *param_check_result = 0;
    return ret;
}

int ra_handle(struct ra_hdc_op_sec *op_sec, char *recv_buf, int rcv_buf_len, char **send_buf, int *snd_buf_len,
    unsigned int *close_session)
{
    int i;
    int ret;
    int op_right = 0;
    int param_check_ret = 0;
    int op_ret = 0;
    struct msg_head *recv_msg_head = (struct msg_head *)recv_buf;
    int num = sizeof(g_ra_op_handle) / sizeof(g_ra_op_handle[0]);

    ret = ra_check_param(recv_buf, rcv_buf_len, send_buf, snd_buf_len, &param_check_ret);
    CHK_PRT_RETURN(param_check_ret != 0 || ret != 0, hccp_err("ra param check fail. param check ret:[%d]"
        "function call ret:[%d]", param_check_ret, ret), ret);

    ra_get_op_right(op_sec, recv_msg_head->opcode, recv_msg_head->async_req_id, &op_right);
    CHK_PRT_RETURN(op_right != HAVE_OP_RIGHT, ret = op_msg_err(send_buf, recv_msg_head, snd_buf_len, op_right), ret);

    *send_buf = (char *)calloc(sizeof(char), recv_msg_head->msg_data_len + sizeof(struct msg_head));
    CHK_PRT_RETURN(*send_buf == NULL, hccp_err("calloc fail."), -ENOMEM);

    for (i = 0; i < num; i++) {
        if (g_ra_op_handle[i].opcode == recv_msg_head->opcode) {
            ret = g_ra_op_handle[i].op_handle(recv_buf, *send_buf, snd_buf_len, &op_ret, rcv_buf_len);
            if (ret) {
                hccp_err("ra handle fail. ret:[%d]", ret);
                goto out;
            }
            msg_head_build_up_hw(*send_buf, recv_msg_head, op_ret, recv_msg_head->msg_data_len);
            *close_session = recv_msg_head->opcode == RA_RS_HDC_SESSION_CLOSE ? 1 : 0;
            *snd_buf_len = recv_msg_head->msg_data_len + sizeof(struct msg_head);
            return ret;
        }
    }

    hccp_warn("not support opcode:%d", recv_msg_head->opcode);
    ret = -EPROTONOSUPPORT;
out:
    free(*send_buf);
    *send_buf = NULL;
    return ret;
}

STATIC int ra_send_pkt(struct drvHdcMsg *msg_rcv, HDC_SESSION session, void *send_buf, int snd_buf_len)
{
    int ret;
    struct drvHdcMsg *msg_snd = NULL;

    ret = RA_HDC_OPS.reuse_msg(msg_rcv);
    CHK_PRT_RETURN(ret, hccp_err("reuse msg failed ret %d", ret), ret);

    msg_snd = msg_rcv;

    ret = RA_HDC_OPS.add_msg_buffer(msg_snd, send_buf, snd_buf_len);
    CHK_PRT_RETURN(ret, hccp_err("add msg buffer failed ret %d", ret), ret);

    ret = RA_HDC_OPS.send(session, msg_snd, 0, RA_HDC_RECV_SEND_TIMEOUT);
    CHK_PRT_RETURN(ret, hccp_err("send msg failed ret %d", ret), ret);

    return 0;
}

STATIC int recv_handle_send_pkt(HDC_SESSION session, unsigned int *close_session, unsigned int chip_id)
{
    int ret;
    void *recv_buf = NULL;
    void *send_buf = NULL;
    struct drvHdcMsg *msg_rcv = NULL;
    int recv_buf_cnt, snd_buf_len, rcv_buf_len;
    rs_set_ctx(chip_id);
    ret = RA_HDC_OPS.alloc_msg(session, &msg_rcv, 1);
    CHK_PRT_RETURN(ret, hccp_err("alloc hdc msg failed ret %d", ret), ret);

    ret = RA_HDC_OPS.recv(session, msg_rcv, MAX_HDC_DATA, 0, &recv_buf_cnt, RA_HDC_RECV_SEND_TIMEOUT);
    if (ret) {
        hccp_warn("recv hdc msg unsuccessful, ret %d", ret);
        goto out;
    }

    RA_HDC_OPS.get_msg_buffer(msg_rcv, 0, (char **)&recv_buf, &rcv_buf_len);
    if (recv_buf == NULL) {
        hccp_warn("rcv_buf_len is NULL, Session disconnect.");
        goto out;
    }

    if (!rcv_buf_len) {
        *close_session = 1;
        hccp_warn("rcv_buf_len is 0, Session disconnect.");
        RA_HDC_OPS.free_msg(msg_rcv);
        return 0;
    }

    ret = ra_handle(&g_hdc_server[chip_id].op_sec, recv_buf, rcv_buf_len, (char **)&send_buf, &snd_buf_len,
        close_session);
    if (ret) {
        hccp_err("ra_handle failed.");
        goto out;
    }

    ret = ra_send_pkt(msg_rcv, session, send_buf, snd_buf_len);
    if (ret) {
        hccp_err("ra send pkt failed ret %d", ret);
        goto err;
    }

err:
    free(send_buf);
    send_buf = NULL;
out:
    RA_HDC_OPS.free_msg(msg_rcv);
    return ret;
}

STATIC void ra_hdc_recv_handle_send_pkt(const unsigned int chip_id)
{
    unsigned int close_session = 0;
    int ret;

    ret = recv_handle_send_pkt(g_hdc_server[chip_id].hdc_session, &close_session, chip_id);
    if (close_session || ret) {
        hccp_warn("recv_handle_send_pkt close_session[%u] ret[%d]", close_session, ret);
        RA_PTHREAD_MUTEX_LOCK(&g_hdc_init_para.mutex);
        g_hdc_init_para.connect_status = HDC_UNCONNECTED;
        RA_PTHREAD_MUTEX_UNLOCK(&g_hdc_init_para.mutex);
    }

    return;
}

STATIC void ra_hw_hdc_close_session(HDC_SESSION *session)
{
    int ret;

    RA_PTHREAD_MUTEX_LOCK(&g_hdc_init_para.mutex);
    if (session == NULL || *session == NULL) {
        goto out;
    }

    ret = RA_HDC_OPS.session_close(*session);
    if (ret != 0) {
        hccp_warn("RA_HDC_OPS.session_close unsuccessful, ret:%d", ret);
    }
    *session = NULL;

out:
    RA_PTHREAD_MUTEX_UNLOCK(&g_hdc_init_para.mutex);
    return;
}

STATIC void *ra_pthread(void *arg)
{
    unsigned int chip_id = g_hdc_init_para.chip_id;
    int ret;

    ret = pthread_detach(pthread_self());
    CHK_PRT_RETURN(ret, hccp_err("pthread detach failed ret %d", ret), NULL);

    (void)prctl(PR_SET_NAME, (unsigned long)"hccp_ra");

    RA_PTHREAD_MUTEX_LOCK(&g_hdc_init_para.mutex);
    g_hdc_init_para.thread_status = THREAD_RUNNING;
    RA_PTHREAD_MUTEX_UNLOCK(&g_hdc_init_para.mutex);

    rs_get_cur_time(&g_ra_thread_info.last_check_time);
    ret = strncpy_s((char *)g_ra_thread_info.pthread_name, sizeof(g_ra_thread_info.pthread_name),
        "ra_thread", strlen("ra_thread"));
    CHK_PRT_RETURN(ret, hccp_err("strncpy_s pthread name failed, ret[%d]", ret), NULL);

    hccp_run_info("pthread[%s] is alive!", g_ra_thread_info.pthread_name);
    while (1) {
        if (g_hdc_init_para.thread_status == THREAD_DESTROYING) {
            break;
        }

        if (g_hdc_init_para.connect_status != HDC_CONNECTED) {
            usleep(THREAD_SLEEP_TIME);
            continue;
        }
        rs_heartbeat_alive_print(&g_ra_thread_info);
        ra_hdc_recv_handle_send_pkt(chip_id);
    }

    hccp_info("thread [%d] is out", getpid());
    ra_hw_hdc_close_session(&g_hdc_server[chip_id].hdc_session);
    RA_PTHREAD_MUTEX_LOCK(&g_hdc_init_para.mutex);
    g_hdc_init_para.thread_status = THREAD_HALT;
    RA_PTHREAD_MUTEX_UNLOCK(&g_hdc_init_para.mutex);
    return NULL;
}

STATIC int ra_hdc_server_init(unsigned int chip_id, int hdc_type)
{
    int ret;

    CHK_PRT_RETURN(chip_id > HCCP_MAX_CHIP_ID || g_hdc_server[chip_id].hdc_session != NULL, hccp_err("invalid "
        "chip id %u, or hdc_session is not NULL", chip_id), -EINVAL);
    CHK_PRT_RETURN(hdc_type != HDC_SERVICE_TYPE_RDMA && hdc_type != HDC_SERVICE_TYPE_RDMA_V2, hccp_err("invalid "
        "hdc_type %d", hdc_type), -EINVAL);

    ra_hdc_init_op_sec(&g_hdc_server[chip_id].op_sec, BUCKET_DEPTH, false);

    ret = RA_HDC_OPS.server_create(chip_id, hdc_type, &g_hdc_server[chip_id].hdc_server);
    CHK_PRT_RETURN(ret, hccp_err("Create Server failed, ret(%d) ", ret), -EINVAL);

    return 0;
}

void ra_hdc_init_op_sec(struct ra_hdc_op_sec *op_sec, unsigned long long token_num, bool is_async_op)
{
    op_sec->token_num = token_num;
    op_sec->cfg_op_num = 0;
    op_sec->t_last.tv_sec = 0;
    op_sec->t_last.tv_usec = 0;
    op_sec->is_async_op = is_async_op;
}

int ra_hdc_session_accept(unsigned int chip_id, HDC_SESSION *session, int init_host_tgid)
{
    int host_tgid;
    int ret = 0;

    ret = RA_HDC_OPS.session_accept(g_hdc_server[chip_id].hdc_server, session);
    if (ret != 0) {
        hccp_warn("Session accept failed, chip_id(%u), ret(%d) ", chip_id, ret);
        return ret;
    }

    RA_HDC_OPS.set_session_reference(*session);
    ret = dl_hal_hdc_get_session_attr(*session, HDC_SESSION_ATTR_PEER_CREATE_PID, &host_tgid);
    if (ret) {
        hccp_err("Session get host_pid failed, chip_id(%u), ret(%d)", chip_id, ret);
        goto out;
    }

    if (host_tgid != init_host_tgid) {
        hccp_warn("host_tgid[%d] from ra not equal to the tgid[%d] from hccp_init, invalid", host_tgid, init_host_tgid);
        goto out;
    }

    return 0;

out:
    ra_hdc_close_session(session);
    return ret;
}

int ra_hdc_async_recv_pkt(struct ra_hdc_async_info *async_info, unsigned int chip_id, void **recv_buf,
    unsigned int *recv_len)
{
    struct drvHdcMsg *msg_rcv = NULL;
    void *rcv_buf = NULL;
    int rcv_len = 0;
    int ret;

    ret = RA_HDC_OPS.alloc_msg(async_info->hdc_session, &msg_rcv, 1);
    CHK_PRT_RETURN(ret != 0, hccp_err("alloc hdc msg failed ret %d", ret), ret);

    ret = RA_HDC_OPS.recv(async_info->hdc_session, msg_rcv, MAX_HDC_DATA, 0, &rcv_len, RA_HDC_RECV_SEND_TIMEOUT);
    if (ret != 0) {
        hccp_warn("recv hdc msg unsuccessful ret %d", ret);
        goto out;
    }

    RA_HDC_OPS.get_msg_buffer(msg_rcv, 0, (char **)&rcv_buf, &rcv_len);
    if (rcv_buf == NULL || rcv_len == 0) {
        hccp_warn("get_msg_buffer unsuccessful, rcv_buf is NULL or rcv_len:%d is 0", rcv_len);
        goto out;
    }

    *recv_buf = (char *)calloc(rcv_len, sizeof(char));
    if (*recv_buf == NULL) {
        hccp_err("calloc recv_buf failed, errno:%d rcv_len:%d", errno, rcv_len);
        ret = -ENOMEM;
        goto out;
    }

    (void)memcpy_s(*recv_buf, rcv_len, rcv_buf, rcv_len);
    *recv_len = rcv_len;

out:
    RA_HDC_OPS.free_msg(msg_rcv);
    return ret;
}

int ra_hdc_async_send_pkt(struct ra_hdc_async_info *async_info, unsigned int chip_id, void *send_buf,
    unsigned int send_len)
{
    struct drvHdcMsg *msg_snd = NULL;
    int ret = -EINVAL;

    RA_PTHREAD_MUTEX_LOCK(&async_info->send_mutex);
    // degrade log level because session will be closed by recv thread and request will be abort
    if (async_info->hdc_session == NULL) {
        hccp_warn("[async][send_pkt]hdc_session is NULL, chip_id(%u)", chip_id);
        goto alloc_msg_err;
    }

    ret = RA_HDC_OPS.alloc_msg(async_info->hdc_session, &msg_snd, 1);
    if (ret != 0) {
        hccp_err("[async][send_pkt]HDC alloc msg err ret(%d) chip_id(%u)", ret, chip_id);
        goto alloc_msg_err;
    }

    ret = RA_HDC_OPS.add_msg_buffer(msg_snd, send_buf, send_len);
    if (ret != 0) {
        hccp_err("[async][send_pkt]HDC add msg buffer err ret(%d) chip_id(%u)", ret, chip_id);
        goto msg_err;
    }

    ret = RA_HDC_OPS.send(async_info->hdc_session, msg_snd, RA_HDC_WAIT_TIMEOUT, RA_HDC_RECV_SEND_TIMEOUT);
    if (ret != 0) {
        hccp_err("[async][send_pkt]HDC send err ret(%d) chip_id(%u)", ret, chip_id);
        goto msg_err;
    }

msg_err:
    RA_HDC_OPS.free_msg(msg_snd);
alloc_msg_err:
    RA_PTHREAD_MUTEX_UNLOCK(&async_info->send_mutex);
    return ret;
}

void ra_hdc_close_session(HDC_SESSION *session)
{
    RA_HDC_OPS.session_close(*session);
    *session = NULL;
    return;
}

STATIC void ra_hw_hdc_init(void *arg)
{
    unsigned int chip_id = g_hdc_init_para.chip_id;
    pthread_t tidp;
    int ret;

    ret = pthread_detach(pthread_self());
    if (ret) {
        hccp_err("pthread detach failed ret %d", ret);
        return;
    }

    (void)prctl(PR_SET_NAME, (unsigned long)"hccp_hw_hdc");

    hccp_info("chip_id(%u)", chip_id);
    g_hdc_init_para.hdc_flag = 1;

    ret = pthread_create(&tidp, NULL, (void *)ra_pthread, NULL);
    if (ret) {
        hccp_err("Create pthread failed, chip_id(%u), ret(%d) ", chip_id, ret);
        return;
    }

    while (1) {
        if (g_hdc_init_para.connect_status != HDC_UNCONNECTED) {
            usleep(HDC_ACCEPT_SLEEP_TIME);
            continue;
        }
        ra_hw_hdc_close_session(&g_hdc_server[chip_id].hdc_session);
        ret = ra_hdc_session_accept(chip_id, &g_hdc_server[chip_id].hdc_session, (int)g_hdc_init_para.host_tgid);
        if (ret != 0) {
            hccp_warn("Session Accept unsuccessful, chip_id(%u), ret(%d) ", chip_id, ret);
            g_hdc_init_para.hdc_flag = 0;
            return;
        }
        // original case, should continue to accept: host_tgid != g_hdc_init_para.host_tgid
        if (ret == 0 && g_hdc_server[chip_id].hdc_session == NULL) {
            continue;
        }
        RA_PTHREAD_MUTEX_LOCK(&g_hdc_init_para.mutex);
        g_hdc_init_para.connect_status = HDC_CONNECTED;
        RA_PTHREAD_MUTEX_UNLOCK(&g_hdc_init_para.mutex);
    }
}

STATIC void ra_hw_hdc_deinit(void)
{
    unsigned int chip_id = g_hdc_init_para.chip_id;
    int ret, try_again;

    RA_PTHREAD_MUTEX_LOCK(&g_hdc_init_para.mutex);
    g_hdc_init_para.thread_status = THREAD_DESTROYING;
    RA_PTHREAD_MUTEX_UNLOCK(&g_hdc_init_para.mutex);

    try_again = HDC_TRY_TIME;
    while ((g_hdc_init_para.thread_status != THREAD_HALT) && try_again != 0) {
        usleep(HDC_USLEEP_TIME);
        try_again--;
    }
    if (try_again == 0) {
        hccp_warn("hdc message thread quit timeout, chip_id:%u", chip_id);
    }

    if (g_hdc_server[chip_id].hdc_server != NULL) {
        ret = RA_HDC_OPS.server_destroy(g_hdc_server[chip_id].hdc_server);
        if (ret != 0) {
            hccp_warn("RA_HDC_OPS.server_destroy unsuccessful, ret:%d, chip_id:%u", ret, chip_id);
        }
        g_hdc_server[chip_id].hdc_server = NULL;
    } else {
        hccp_warn("hdc_server is NULL, chip_id:%u", chip_id);
    }
    pthread_mutex_destroy(&g_hdc_init_para.mutex);
}

STATIC int hccp_set_affinity(unsigned int chip_id)
{
    int ret;
    int64_t cpu_id;
    int64_t ccpu_num; /* ctrl cpu */
    int64_t dcpu_num; /* data cpu */
    int64_t acpu_num; /* ai cpu */
    int64_t cpu_core_num;
    cpu_set_t mask;

    ret = dl_hal_get_device_info(chip_id, MODULE_TYPE_CCPU, INFO_TYPE_CORE_NUM, &ccpu_num);
    CHK_PRT_RETURN(ret, hccp_err("get ccpu_num failed, ret(%d)", ret), ret);

    ret = dl_hal_get_device_info(chip_id, MODULE_TYPE_DCPU, INFO_TYPE_CORE_NUM, &dcpu_num);
    CHK_PRT_RETURN(ret, hccp_err("get dcpu_num failed, ret(%d)", ret), ret);

    ret = dl_hal_get_device_info(chip_id, MODULE_TYPE_AICPU, INFO_TYPE_CORE_NUM, &acpu_num);
    CHK_PRT_RETURN(ret, hccp_err("get acpu_num failed, ret(%d)", ret), ret);
    cpu_core_num = ccpu_num + dcpu_num + acpu_num;

    CPU_ZERO(&mask);
    cpu_id = cpu_core_num * chip_id + HCCP_RUN_CPU_CORE;
    /*lint -e574*/
    CPU_SET((size_t)cpu_id, &mask);  //lint !e573
    /*lint +e574*/
    hccp_run_info("chip_id:%u ccpu_num:%lld, dcpu_num:%lld, acpu_num:%lld, cpu_id:%lld",
        chip_id, ccpu_num, dcpu_num, acpu_num, cpu_id);
    ret = sched_setaffinity(getpid(), sizeof(mask), &mask); /* hccp use core0 of each chip to setaffinity */
    CHK_PRT_RETURN(ret == -1, hccp_err("sched_setaffinity failed: ret %d, errno %d ", ret, errno), -ESYSFUNC);

    return 0;
}

STATIC int ra_hw_init(unsigned int chip_id, pid_t pid)
{
    int ret;
    pthread_t tidp;
    int timeout = RA_THREAD_TRY_TIME;

    g_hdc_init_para.chip_id = chip_id;
    g_hdc_init_para.host_tgid = pid;

    ret = pthread_create(&tidp, NULL, (void *)ra_hw_hdc_init, NULL);
    CHK_PRT_RETURN(ret, hccp_err("Create pthread failed, ret(%d) ", ret), -ESYSFUNC);

    while (g_hdc_init_para.hdc_flag != 1 && timeout > 0) {
        usleep(RA_THREAD_SLEEP_TIME);
        timeout--;
    }

    CHK_PRT_RETURN(g_hdc_init_para.hdc_flag == 0 || timeout == 0, hccp_err("HDC server thread create timeout,"
        "flag %d, timeout %d", g_hdc_init_para.hdc_flag, timeout), -ESRCH);

    return 0;
}

RA_ADP_ATTRI_VISI_DEF int hccp_init(unsigned int chip_id, pid_t pid, int hdc_type, unsigned int white_list_status)
{
    struct timeval start, end;
    float time_cost = 0.0;
    int ret, ret_tmp;

    hccp_info("hccp[%u] hdc_type[%d] white_list_status[%u] init start", chip_id, hdc_type, white_list_status);

    ret = dl_hal_init();
    if (ret != 0) {
        hccp_err("dl_hal_init failed, ret = %d", ret);
        return ret;
    }

    ret = hccp_set_affinity(chip_id);
    if (ret != 0) {
        hccp_err("hccp_set_affinity failed, ret(%d) ", ret);
        goto out;
    }

    rs_get_cur_time(&start);
    ret = ra_hdc_server_init(chip_id, hdc_type);
    if (ret != 0) {
        hccp_err("chip_id[%u] hdc_type[%d] ra_hdc_server_init failed, ret[%d] ", chip_id, hdc_type, ret);
        goto out;
    }

    ret = pthread_mutex_init(&g_hdc_init_para.mutex, NULL);
    if (ret != 0) {
        hccp_err("g_hdc_init_para mutex_init failed ret %d!, normal ret 0", ret);
        ret = -ESYSFUNC;
        goto out;
    }

    ret = ra_hw_init(chip_id, pid);
    if (ret != 0) {
        hccp_err("ra_init failed, ret(%d) ", ret);
        goto hw_init_err;
    }

    ret = ra_hw_async_init(chip_id, pid);
    if (ret != 0) {
        hccp_err("ra_hw_async_init failed, ret(%d) ", ret);
        goto hw_init_err;
    }

    rs_get_cur_time(&end);
    hccp_time_interval(&end, &start, &time_cost);
    hccp_info("ra_hw_init ok cost [%f] ms", time_cost);

    struct rs_init_config offline_config = {
        .chip_id = chip_id,
        .hccp_mode = NETWORK_OFFLINE,
        .white_list_status = white_list_status,
    };

    rs_get_cur_time(&start);
    ret = rs_init(&offline_config);
    if (ret != 0) {
        hccp_err("rs_init failed (0x%x) ", ret);
        goto init_err;
    }
    rs_get_cur_time(&end);
    hccp_time_interval(&end, &start, &time_cost);
    hccp_info("rs_init ok cost [%f] ms", time_cost);

    rs_get_cur_time(&start);
    ret = rs_bind_hostpid(chip_id, pid);
    if (ret != 0) {
        hccp_err("rs_bind_hostpid failed, ret=%d ", ret);
        goto bind_hostpid_err;
    }
    rs_get_cur_time(&end);
    hccp_time_interval(&end, &start, &time_cost);
    hccp_info("rs_bind_hostpid ok cost [%f] ms", time_cost);

    rs_get_cur_time(&start);
    ret = rs_ping_handle_init(chip_id, hdc_type, white_list_status);
    if (ret != 0) {
        hccp_err("rs_ping_handle_init failed, ret=%d ", ret);
        goto bind_hostpid_err;
    }
    rs_get_cur_time(&end);
    hccp_time_interval(&end, &start, &time_cost);
    hccp_info("rs_ping_handle_init ok cost [%f] ms", time_cost);

    return 0;
bind_hostpid_err:
    ret_tmp = rs_deinit(&offline_config);
    if (ret_tmp) {
        hccp_err("rs_deinit failed %d ", ret_tmp);
    }
init_err:
    ra_hw_async_deinit();
hw_init_err:
    pthread_mutex_destroy(&g_hdc_init_para.mutex);
out:
    dl_hal_deinit();
    return ret;
}

RA_ADP_ATTRI_VISI_DEF int hccp_deinit(unsigned int chip_id)
{
    struct rs_init_config offline_config = {
        .chip_id = chip_id,
        .hccp_mode = NETWORK_OFFLINE,
        .white_list_status = WHITE_LIST_ENABLE,
    };
    int ret;

    hccp_info("hccp[%u] deinit start", chip_id);

    ret = rs_ping_handle_deinit(chip_id);
    CHK_PRT_RETURN(ret, hccp_err("rs_ping_handle_deinit failed %d ", ret), ret);

    ret = rs_deinit(&offline_config);
    CHK_PRT_RETURN(ret, hccp_err("rs_deinit failed %d ", ret), ret);

    ra_hw_hdc_deinit();

    ra_hw_async_deinit();
    dl_hal_deinit();
    hccp_info("hccp [%u] deinit success", chip_id);

    return ret;
}

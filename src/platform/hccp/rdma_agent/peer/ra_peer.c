/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include "securec.h"
#include "dl_hal_function.h"
#include "ra_comm.h"
#include "ra_rs_comm.h"
#include "ra_rs_err.h"
#include "rs.h"
#include "ra_peer.h"

#define PAGE_SHIFT              12
int g_notify_fd = -1;

static pthread_mutex_t g_ra_peer_mutex[RA_MAX_PHY_ID_NUM];
int g_ra_init_counter[RA_MAX_PHY_ID_NUM] = {0};

void ra_peer_mutex_lock(unsigned int phy_id)
{
    PEER_PTHREAD_MUTEX_LOCK(&g_ra_peer_mutex[phy_id]);
}

void ra_peer_mutex_unlock(unsigned int phy_id)
{
    PEER_PTHREAD_MUTEX_UNLOCK(&g_ra_peer_mutex[phy_id]);
}

int ra_peer_socket_batch_close(unsigned int dev_id, struct socket_close_info_t conn[], unsigned int num)
{
    int ret;
    unsigned int i;
    int disuse_linger;
    unsigned int index = 0;
    unsigned int close_num = 0;
    struct rs_socket_close_info_t close_info[MAX_SOCKET_NUM];

    ret = memset_s(close_info, sizeof(struct rs_socket_close_info_t) * MAX_SOCKET_NUM, 0,
                   sizeof(struct rs_socket_close_info_t) * MAX_SOCKET_NUM);
    CHK_PRT_RETURN(ret, hccp_err("[batch_close][ra_peer_socket]memset_s close_info failed, ret(%d)",
        ret), -ESAFEFUNC);

    for (i = 0; i < num; i++) {
        if (conn[i].fd_handle != NULL) {
            close_info[close_num].fd = ((struct socket_peer_info *)(conn[i].fd_handle))->fd;
            ++close_num;
        }
    }

    // use attr disuse_linger of the fist conn as the common attr for all(0 by default)
    disuse_linger = conn[0].disuse_linger;

    PEER_PTHREAD_MUTEX_LOCK(&g_ra_peer_mutex[dev_id]);
    rs_set_ctx(dev_id);
    PEER_PTHREAD_MUTEX_UNLOCK(&g_ra_peer_mutex[dev_id]);
    ret = rs_socket_batch_close(disuse_linger, &close_info[index], close_num);
    if (ret) {
        hccp_err("[batch_close][ra_peer_socket]ra close failed ret(%d).", ret);
        goto out;
    }

out:
    for (i = 0; i < num; i++) {
        if (conn[i].fd_handle != NULL) {
            free(conn[i].fd_handle);
            conn[i].fd_handle = NULL;
        }
    }
    return ret;
}

int ra_peer_socket_batch_abort(unsigned int dev_id, struct socket_connect_info_t conn[], unsigned int num)
{
    struct socket_connect_info conn_out[MAX_SOCKET_NUM];
    int ret = 0;

    ret = ra_get_socket_connect_info(conn, num, conn_out, MAX_SOCKET_NUM);
    CHK_PRT_RETURN(ret, hccp_err("[batch_abort][ra_peer_socket]ra_get_socket_connect_info failed, ret(%d)", ret), ret);

    PEER_PTHREAD_MUTEX_LOCK(&g_ra_peer_mutex[dev_id]);
    rs_set_ctx(dev_id);
    PEER_PTHREAD_MUTEX_UNLOCK(&g_ra_peer_mutex[dev_id]);
    ret = rs_socket_batch_abort(conn_out, num);
    CHK_PRT_RETURN(ret, hccp_err("[batch_abort][ra_peer_socket]abort failed ret(%d), phy_id(%u), num(%u)",
        ret, dev_id, num), ret);

    return ret;
}

int ra_peer_socket_batch_connect(unsigned int dev_id, struct socket_connect_info_t conn[], unsigned int num)
{
    int ret;
    struct socket_connect_info conn_out[MAX_SOCKET_NUM];

    ret = ra_get_socket_connect_info(conn, num, conn_out, MAX_SOCKET_NUM);
    CHK_PRT_RETURN(ret, hccp_err("[batch_connect][ra_peer_socket]ra_hdc_get_socket_connect_info failed,"
        "ret(%d). ", ret), ret);

    /* In peer online mode the server port number is user-defined */
    PEER_PTHREAD_MUTEX_LOCK(&g_ra_peer_mutex[dev_id]);
    rs_set_ctx(dev_id);
    ret = rs_socket_set_scope_id(dev_id, ((struct ra_socket_handle *)conn[0].socket_handle)->scope_id);
    if (ret != 0) {
        PEER_PTHREAD_MUTEX_UNLOCK(&g_ra_peer_mutex[dev_id]);
    }
    CHK_PRT_RETURN(ret, hccp_err("[set scope id][ra_peer_socket]ra_peer_socket_set_scope_id failed"
        "ret(%d).", ret), ret);

    ret = rs_socket_batch_connect(conn_out, num);
    if (ret) {
        hccp_err("[batch_connect][ra_peer_socket]ra client connect failed ret(%d).", ret);
    }
    PEER_PTHREAD_MUTEX_UNLOCK(&g_ra_peer_mutex[dev_id]);
    return ret;
}

int ra_peer_socket_listen_start(unsigned int dev_id, struct socket_listen_info_t conn[], unsigned int num)
{
    struct socket_listen_info rs_conn[MAX_SOCKET_NUM] = {0};
    unsigned int i;
    int ret;

    for (i = 0; i < num; i++) {
        CHK_PRT_RETURN(conn[i].port > MAX_PORT_NUM, hccp_err("[listen_start][ra_peer_socket]port(%u) of"
            "conn(%u) is invalid", conn[i].port, i), -EINVAL);
    }

    ret = ra_get_socket_listen_info(conn, num, rs_conn, MAX_SOCKET_NUM);
    CHK_PRT_RETURN(ret, hccp_err("[listen_start][ra_peer_socket]ra_get_socket_listen_info failed"
        "ret(%d)", ret), ret);

    PEER_PTHREAD_MUTEX_LOCK(&g_ra_peer_mutex[dev_id]);
    rs_set_ctx(dev_id);
    ret = rs_socket_set_scope_id(dev_id, ((struct ra_socket_handle *)conn[0].socket_handle)->scope_id);
    if (ret != 0) {
        PEER_PTHREAD_MUTEX_UNLOCK(&g_ra_peer_mutex[dev_id]);
    }
    CHK_PRT_RETURN(ret, hccp_err("[set scope id][ra_peer_socket]ra_peer_socket_set_scope_id failed"
        "ret(%d)", ret), ret);

    ret = rs_socket_listen_start(rs_conn, num);
    // listen node found, degrade log level make it consistent with inner call
    if (ret == -EEXIST) {
        hccp_info("[listen_start][ra_peer_socket]ra listen start unsuccessful ret(%d)", ret);
    } else if (ret == -EADDRINUSE) {
        hccp_run_warn("[listen_start][ra_peer_socket]ra listen start unsuccessful ret(%d)", ret);
    } else if (ret != 0) {
        hccp_err("[listen_start][ra_peer_socket]ra listen start failed ret(%d)", ret);
    }
    if (ret != 0) {
        PEER_PTHREAD_MUTEX_UNLOCK(&g_ra_peer_mutex[dev_id]);
        return ret;
    }
    PEER_PTHREAD_MUTEX_UNLOCK(&g_ra_peer_mutex[dev_id]);

    ret = ra_get_socket_listen_result(rs_conn, num, conn, MAX_SOCKET_NUM);
    CHK_PRT_RETURN(ret, hccp_err("[listen_start][ra_peer_socket]ra_get_socket_listen_result failed ret(%d)",
        ret), ret);

    return ret;
}

int ra_peer_socket_listen_stop(unsigned int dev_id, struct socket_listen_info_t conn[], unsigned int num)
{
    struct socket_listen_info rs_conn[MAX_SOCKET_NUM] = {0};
    unsigned int i;
    int ret;

    for (i = 0; i < num; i++) {
        CHK_PRT_RETURN(conn[i].port > MAX_PORT_NUM, hccp_err("[listen_stop][ra_peer_socket]port(%u) of"
            "conn(%u) is invalid", conn[i].port, i), -EINVAL);
    }

    ret = ra_get_socket_listen_info(conn, num, rs_conn, MAX_SOCKET_NUM);
    CHK_PRT_RETURN(ret, hccp_err("[listen_stop][ra_peer_socket]ra_peer_get_socket_listen_info failed ret(%d).",
        ret), ret);

    PEER_PTHREAD_MUTEX_LOCK(&g_ra_peer_mutex[dev_id]);
    rs_set_ctx(dev_id);
    ret = rs_socket_listen_stop(rs_conn, num);
    if (ret == -ENODEV) {
        hccp_warn("[listen_stop][ra_peer_socket]ra socket listen stop unsuccessful ret(%d).", ret);
    } else if (ret != 0) {
        hccp_err("[listen_stop][ra_peer_socket]ra socket listen stop failed ret(%d).", ret);
    }
    PEER_PTHREAD_MUTEX_UNLOCK(&g_ra_peer_mutex[dev_id]);
    return ret;
}

STATIC int ra_peer_set_rs_conn_param(struct socket_info_t conn[], unsigned int num,
    struct socket_fd_data rs_conn[], unsigned int rs_num)
{
    int ret;
    unsigned int i;
    struct ra_socket_handle *socket_handle = NULL;

    CHK_PRT_RETURN(num > rs_num, hccp_err("[set][ra_peer_rs_conn_param]num(%u) must smaller than rs_num(%u)",
        num, rs_num), -EINVAL);

    for (i = 0; i < num; i++) {
        socket_handle = (struct ra_socket_handle *)conn[i].socket_handle;
        rs_conn[i].phy_id = socket_handle->rdev_info.phy_id;
        rs_conn[i].family = socket_handle->rdev_info.family;
        rs_conn[i].status = conn[i].status;
        ret = memcpy_s(&(rs_conn[i].local_ip), sizeof(union hccp_ip_addr), &(socket_handle->rdev_info.local_ip),
            sizeof(union hccp_ip_addr));
        CHK_PRT_RETURN(ret, hccp_err("[set][ra_peer_rs_conn_param]memcpy_s local_ip failed, ret(%d)",
            ret), -ESAFEFUNC);
        ret = memcpy_s(&(rs_conn[i].remote_ip), sizeof(union hccp_ip_addr), &(conn[i].remote_ip),
            sizeof(union hccp_ip_addr));
        CHK_PRT_RETURN(ret, hccp_err("[set][ra_peer_rs_conn_param]memcpy_s remote_ip failed, ret(%d)", ret), ret);
        ret = memcpy_s(rs_conn[i].tag, sizeof(rs_conn[i].tag), conn[i].tag, sizeof(conn[i].tag));
        CHK_PRT_RETURN(ret, hccp_err("[set][ra_peer_rs_conn_param]memcpy_s tag failed, ret(%d)", ret), -ESAFEFUNC);
    }
    return 0;
}

STATIC int ra_peer_set_conn_param(struct socket_info_t conn[],
    struct socket_fd_data rs_conn[], unsigned int i, unsigned int ssl_enable)
{
    int ret;
    struct ra_socket_handle *socket_handle = NULL;
    
    socket_handle = (struct ra_socket_handle *)conn[i].socket_handle;
    socket_handle->rdev_info.phy_id = rs_conn[i].phy_id;

    ret = memcpy_s(&(socket_handle->rdev_info.local_ip), sizeof(union hccp_ip_addr),
        &(rs_conn[i].local_ip), sizeof(union hccp_ip_addr));
    CHK_PRT_RETURN(ret, hccp_err("[set][ra_peer_conn_param]memcpy_s local_ip failed, ret(%d)", ret), -ESAFEFUNC);
    ret = memcpy_s(&(conn[i].remote_ip), sizeof(union hccp_ip_addr),
        &(rs_conn[i].remote_ip), sizeof(union hccp_ip_addr));
    CHK_PRT_RETURN(ret, hccp_err("[set][ra_peer_conn_param]memcpy_s remote_ip failed, ret(%d)", ret), -ESAFEFUNC);

    if (conn[i].fd_handle != NULL) {
        ((struct socket_peer_info *)conn[i].fd_handle)->phy_id = (int)rs_conn[i].phy_id;
        ((struct socket_peer_info *)conn[i].fd_handle)->fd = rs_conn[i].fd;
        ((struct socket_peer_info *)conn[i].fd_handle)->socket_handle = socket_handle;
        ((struct socket_peer_info *)conn[i].fd_handle)->ssl_enable = ssl_enable;
    }
    conn[i].status = rs_conn[i].status;
    return 0;
}

int ra_peer_get_sockets(unsigned int phy_id, unsigned int role, struct socket_info_t conn[], unsigned int num)
{
    struct socket_fd_data rs_conn[MAX_SOCKET_NUM] = {0};
    unsigned int ssl_enable;
    int connected_num;
    unsigned int i;
    unsigned int j;
    int ret;

    ret = ra_peer_set_rs_conn_param(conn, num, rs_conn, MAX_SOCKET_NUM);
    CHK_PRT_RETURN(ret, hccp_err("[get][ra_peer_sockets]ra_peer_set_rs_conn_param failed, ret(%d).", ret), ret);

    PEER_PTHREAD_MUTEX_LOCK(&g_ra_peer_mutex[phy_id]);
    rs_set_ctx(phy_id);
    connected_num = rs_get_sockets(role, rs_conn, num);
    if (connected_num < 0) {
        hccp_err("[get][ra_peer_sockets]ra get socket failed ret(%d).", connected_num);
        PEER_PTHREAD_MUTEX_UNLOCK(&g_ra_peer_mutex[phy_id]);
        return connected_num;
    }
    ret = rs_get_ssl_enable(&ssl_enable);
    if (ret < 0) {
        hccp_err("[get][ra_peer_sockets]rs_get_ssl_enable failed ret(%d)", ret);
        PEER_PTHREAD_MUTEX_UNLOCK(&g_ra_peer_mutex[phy_id]);
        return ret;
    }
    PEER_PTHREAD_MUTEX_UNLOCK(&g_ra_peer_mutex[phy_id]);

    for (i = 0; i < num; i++) {
        if (rs_conn[i].status == RS_SOCK_STATUS_OK) {
            conn[i].fd_handle = (struct socket_peer_info *)calloc(1, sizeof(struct socket_peer_info));
            if (conn[i].fd_handle == NULL) {
                hccp_err("[get][ra_peer_sockets]socket handle calloc failed.");
                ret = -ENOMEM;
                goto err_out;
            }
        } else {
            conn[i].fd_handle = NULL;
        }

        ret = ra_peer_set_conn_param(conn, rs_conn, i, ssl_enable);
        if (ret) {
            hccp_err("[get][ra_peer_sockets]ra_peer_set_conn_param failed, ret(%d).", ret);
            goto err_out;
        }
        if (memcpy_s(conn[i].tag, sizeof(conn[i].tag), rs_conn[i].tag, sizeof(rs_conn[i].tag))) {
            hccp_err("[get][ra_peer_sockets]memcpy_s tag failed.");
            ret = -ESAFEFUNC;
            goto err_out;
        }
    }

    return connected_num;

err_out:
    for (j = 0; j <= i; j++) {
        if (conn[j].fd_handle != NULL) {
            free(conn[j].fd_handle);
            conn[j].fd_handle = NULL;
        }
    }

    return ret;
}

int ra_peer_socket_send(unsigned int dev_id, const void *handle, const void *data, unsigned long long size)
{
    int fd;
    int ret;
    unsigned int ssl_enable;

    fd = ((const struct socket_peer_info *)handle)->fd;
    ssl_enable = ((const struct socket_peer_info *)handle)->ssl_enable;
    if (ssl_enable != RA_SSL_DISABLE) {
        PEER_PTHREAD_MUTEX_LOCK(&g_ra_peer_mutex[dev_id]);
        rs_set_ctx(dev_id);
    }
    ret = rs_peer_socket_send(ssl_enable, fd, data, size);
    if (ssl_enable != RA_SSL_DISABLE) {
        PEER_PTHREAD_MUTEX_UNLOCK(&g_ra_peer_mutex[dev_id]);
    }
    return ret;
}

int ra_peer_socket_recv(unsigned int dev_id, const void *handle, void *data, unsigned long long size)
{
    int fd;
    int ret;
    unsigned int ssl_enable;

    fd = ((const struct socket_peer_info *)handle)->fd;
    ssl_enable = ((const struct socket_peer_info *)handle)->ssl_enable;
    if (ssl_enable != RA_SSL_DISABLE) {
        PEER_PTHREAD_MUTEX_LOCK(&g_ra_peer_mutex[dev_id]);
        rs_set_ctx(dev_id);
    }
    ret = rs_peer_socket_recv(ssl_enable, fd, data, size);
    if (ssl_enable != RA_SSL_DISABLE) {
        PEER_PTHREAD_MUTEX_UNLOCK(&g_ra_peer_mutex[dev_id]);
    }
    return ret;
}

int ra_peer_socket_white_list_add(struct rdev rdev_info,
    struct socket_wlist_info_t white_list[], unsigned int num)
{
    int ret;
    unsigned int i;
    char net_addr[MAX_IP_LEN] = {0};

    for (i = 0; i < num; i++) {
        CHK_PRT_RETURN(inet_ntop(rdev_info.family, &white_list[i].remote_ip, net_addr, sizeof(net_addr)) == NULL,
            hccp_err("[add][ra_peer_socket_white_list]remote ip is invalid! i(%u)", i), -EINVAL);
    }
    PEER_PTHREAD_MUTEX_LOCK(&g_ra_peer_mutex[rdev_info.phy_id]);
    rs_set_ctx(rdev_info.phy_id);
    ret = rs_socket_white_list_add(rdev_info, white_list, num);
    if (ret) {
        hccp_err("[add][ra_peer_socket_white_list]rs_socket_white_list_add failed ret(%d).", ret);
        PEER_PTHREAD_MUTEX_UNLOCK(&g_ra_peer_mutex[rdev_info.phy_id]);
        return ret;
    }
    PEER_PTHREAD_MUTEX_UNLOCK(&g_ra_peer_mutex[rdev_info.phy_id]);
    return ret;
}

int ra_peer_epoll_ctl_add(const void *fd_handle, enum RaEpollEvent event)
{
    int ret;

    ret = rs_epoll_ctl_add(fd_handle, event);
    if (ret) {
        hccp_err("[ra_peer_epoll_ctl_add]rs_epoll_ctl_add failed ret(%d).", ret);
    }
    return ret;
}

int ra_peer_epoll_ctl_mod(const void *fd_handle, enum RaEpollEvent event)
{
    int ret;

    ret = rs_epoll_ctl_mod(fd_handle, event);
    if (ret) {
        hccp_err("[ra_peer_epoll_ctl_mod]rs_epoll_ctl_mod failed ret(%d).", ret);
    }
    return ret;
}

int ra_peer_epoll_ctl_del(const void *fd_handle)
{
    int fd = -1;
    int ret;

    fd = ((const struct socket_peer_info *)fd_handle)->fd;
    ret = rs_epoll_ctl_del(fd);
    if (ret) {
        hccp_err("[ra_peer_epoll_ctl_del]rs_epoll_ctl_del failed ret(%d).", ret);
    }
    return ret;
}

void ra_peer_set_tcp_recv_callback(unsigned int phy_id, const void *callback)
{
    PEER_PTHREAD_MUTEX_LOCK(&g_ra_peer_mutex[phy_id]);
    rs_set_ctx(phy_id);
    rs_set_tcp_recv_callback(callback);
    PEER_PTHREAD_MUTEX_UNLOCK(&g_ra_peer_mutex[phy_id]);
}

int ra_peer_socket_white_list_del(struct rdev rdev_info,
    struct socket_wlist_info_t white_list[], unsigned int num)
{
    int ret;
    unsigned int i;
    char net_addr[MAX_IP_LEN] = {0};

    for (i = 0; i < num; i++) {
        CHK_PRT_RETURN(inet_ntop(rdev_info.family, &white_list[i].remote_ip, net_addr, sizeof(net_addr)) == NULL,
            hccp_err("[del][ra_peer_socket_white_list]remote ip is invalid! i(%u)", i), -EINVAL);
    }

    PEER_PTHREAD_MUTEX_LOCK(&g_ra_peer_mutex[rdev_info.phy_id]);
    rs_set_ctx(rdev_info.phy_id);
    ret = rs_socket_white_list_del(rdev_info, white_list, num);
    if (ret) {
        hccp_err("[del][ra_peer_socket_white_list]ra socket listen stop failed ret(%d).", ret);
    }
    PEER_PTHREAD_MUTEX_UNLOCK(&g_ra_peer_mutex[rdev_info.phy_id]);
    return ret;
}

int ra_peer_socket_deinit(struct rdev rdev_info)
{
    int ret;
    PEER_PTHREAD_MUTEX_LOCK(&g_ra_peer_mutex[rdev_info.phy_id]);
    rs_set_ctx(rdev_info.phy_id);
    ret = rs_socket_deinit(rdev_info);
    if (ret) {
        hccp_err("[deinit][ra_peer_socket]rs_socket_deinit failed, ret(%d)", ret);
        PEER_PTHREAD_MUTEX_UNLOCK(&g_ra_peer_mutex[rdev_info.phy_id]);
        return ret;
    }
    PEER_PTHREAD_MUTEX_UNLOCK(&g_ra_peer_mutex[rdev_info.phy_id]);
    return 0;
}

int ra_peer_qp_create(struct ra_rdma_handle *rdma_handle, int flag, int qp_mode, void **qp_handle)
{
    unsigned int phy_id = rdma_handle->rdev_info.phy_id;
    struct ra_qp_handle *qp_peer = NULL;
    struct rs_qp_resp qp_resp = { 0 };
    struct rs_qp_norm qp_norm = { 0 };
    int ret;

    qp_peer = (struct ra_qp_handle *)calloc(1, sizeof(struct ra_qp_handle));
    CHK_PRT_RETURN(qp_peer == NULL, hccp_err("[create][ra_peer_qp]qp_peer calloc failed."), -ENOMEM);

    qp_norm.flag = flag;
    qp_norm.is_exp = 1;
    qp_norm.is_ext = 0;
    qp_norm.qp_mode = qp_mode;

    PEER_PTHREAD_MUTEX_LOCK(&g_ra_peer_mutex[phy_id]);
    rs_set_ctx(phy_id);
    ret = rs_qp_create(phy_id, rdma_handle->rdev_index, qp_norm, &qp_resp);
    if (ret) {
        hccp_err("[create][ra_peer_qp]ra open failed ret[%d]", ret);
        PEER_PTHREAD_MUTEX_UNLOCK(&g_ra_peer_mutex[phy_id]);
        goto calloc_err;
    }
    PEER_PTHREAD_MUTEX_UNLOCK(&g_ra_peer_mutex[phy_id]);
    qp_peer->phy_id = phy_id;
    qp_peer->qpn = qp_resp.qpn;
    qp_peer->psn = qp_resp.psn;
    qp_peer->gid_idx = qp_resp.gid_idx;
    qp_peer->flag = flag;
    qp_peer->qp_mode = qp_mode;
    qp_peer->rdev_index = rdma_handle->rdev_index;
    qp_peer->rdma_handle = rdma_handle;
    qp_peer->rdma_ops = rdma_handle->rdma_ops;

    *qp_handle = qp_peer;
    return ret;

calloc_err:
    free(qp_peer);
    qp_peer = NULL;
    return ret;
}

int ra_peer_qp_create_with_attrs(struct ra_rdma_handle *rdma_handle, struct qp_ext_attrs *ext_attrs,
    void **qp_handle)
{
    unsigned int phy_id = rdma_handle->rdev_info.phy_id;
    struct rs_qp_norm_with_attrs qp_norm = { 0 };
    struct rs_qp_resp_with_attrs qp_resp = { 0 };
    struct ra_qp_handle *qp_peer = NULL;
    int ret;

    qp_peer = (struct ra_qp_handle *)calloc(1, sizeof(struct ra_qp_handle));
    CHK_PRT_RETURN(qp_peer == NULL, hccp_err("[create][ra_peer_qp_with_attrs]qp_peer calloc failed."), -ENOMEM);

    qp_norm.is_exp = 1;
    qp_norm.is_ext = 0;
    ret = memcpy_s(&qp_norm.ext_attrs, sizeof(struct qp_ext_attrs), ext_attrs, sizeof(struct qp_ext_attrs));
    if (ret) {
        hccp_err("[create][ra_peer_qp_with_attrs]memcpy_s for ext_attrs failed ret[%d]", ret);
        ret = -ESAFEFUNC;
        goto calloc_err;
    }

    PEER_PTHREAD_MUTEX_LOCK(&g_ra_peer_mutex[phy_id]);
    rs_set_ctx(phy_id);
    ret = rs_qp_create_with_attrs(phy_id, rdma_handle->rdev_index, &qp_norm, &qp_resp);
    if (ret) {
        hccp_err("[create][ra_peer_qp_with_attrs]ra open failed ret[%d]", ret);
        PEER_PTHREAD_MUTEX_UNLOCK(&g_ra_peer_mutex[phy_id]);
        goto calloc_err;
    }
    PEER_PTHREAD_MUTEX_UNLOCK(&g_ra_peer_mutex[phy_id]);
    qp_peer->phy_id = phy_id;
    qp_peer->qpn = qp_resp.qpn;
    qp_peer->psn = qp_resp.psn;
    qp_peer->gid_idx = qp_resp.gid_idx;
    qp_peer->flag = ext_attrs->qp_attr.qp_type == IBV_QPT_RC ? 0 : 1;
    qp_peer->qp_mode = ext_attrs->qp_mode;
    qp_peer->rdev_index = rdma_handle->rdev_index;
    qp_peer->rdma_handle = rdma_handle;
    qp_peer->rdma_ops = rdma_handle->rdma_ops;
    qp_peer->udp_sport = ext_attrs->udp_sport;

    *qp_handle = qp_peer;
    return ret;

calloc_err:
    free(qp_peer);
    qp_peer = NULL;
    return ret;
}

int ra_peer_mr_reg(struct ra_qp_handle *qp_peer, struct mr_info *info)
{
    int ret;
    struct rdma_mr_reg_info mr_reg_info = {0};

    mr_reg_info.addr = info->addr;
    mr_reg_info.len = info->size;
    mr_reg_info.access = info->access;

    PEER_PTHREAD_MUTEX_LOCK(&g_ra_peer_mutex[qp_peer->phy_id]);
    rs_set_ctx(qp_peer->phy_id);
    ret = rs_mr_reg(qp_peer->phy_id, qp_peer->rdev_index, qp_peer->qpn, &mr_reg_info);
    if (ret) {
        hccp_err("[reg][ra_peer_mr]ra_reg_mr failed ret(%d)", ret);
    }
    PEER_PTHREAD_MUTEX_UNLOCK(&g_ra_peer_mutex[qp_peer->phy_id]);
    info->lkey = mr_reg_info.lkey;
    info->rkey = mr_reg_info.rkey;
    return ret;
}

int ra_peer_mr_dereg(struct ra_qp_handle *qp_peer, struct mr_info *info)
{
    int ret;

    PEER_PTHREAD_MUTEX_LOCK(&g_ra_peer_mutex[qp_peer->phy_id]);
    rs_set_ctx(qp_peer->phy_id);
    ret = rs_mr_dereg(qp_peer->phy_id, qp_peer->rdev_index, qp_peer->qpn, (char *)info->addr);
    if (ret) {
        hccp_err("[dereg][ra_peer_mr]ra_de_reg_mr failed ret(%d)", ret);
    }
    PEER_PTHREAD_MUTEX_UNLOCK(&g_ra_peer_mutex[qp_peer->phy_id]);
    return ret;
}

int ra_peer_register_mr(struct ra_rdma_handle *rdma_peer, struct mr_info *info, void **mr_handle)
{
    int ret;
    struct rdma_mr_reg_info mr_reg_info = {0};

    mr_reg_info.addr = info->addr;
    mr_reg_info.len = info->size;
    mr_reg_info.access = info->access;

    rs_set_ctx(rdma_peer->rdev_info.phy_id);
    ret = rs_register_mr(rdma_peer->rdev_info.phy_id, rdma_peer->rdev_index, &mr_reg_info, mr_handle);
    if (ret) {
        hccp_err("[ra_peer_register_mr]rs_register_mr failed ret(%d)", ret);
    }
    info->lkey = mr_reg_info.lkey;
    info->rkey = mr_reg_info.rkey;
    return ret;
}

int ra_peer_deregister_mr(struct ra_rdma_handle *rdma_peer, void *mr_handle)
{
    int ret;

    rs_set_ctx(rdma_peer->rdev_info.phy_id);
    ret = rs_deregister_mr(mr_handle);
    if (ret) {
        hccp_err("[ra_peer_deregister_mr]rs_deregister_mr failed ret(%d)", ret);
    }
    return ret;
}

int ra_peer_typical_qp_modify(struct ra_qp_handle *qp_peer, struct typical_qp *local_qp_info,
    struct typical_qp *remote_qp_info)
{
    int ret;

    PEER_PTHREAD_MUTEX_LOCK(&g_ra_peer_mutex[qp_peer->phy_id]);
    rs_set_ctx(qp_peer->phy_id);
    ret = rs_typical_qp_modify(qp_peer->phy_id, qp_peer->rdev_index, *local_qp_info, *remote_qp_info,
        &(qp_peer->udp_sport));
    if (ret != 0) {
        hccp_err("[modify][ra_peer_qp]rs_typical_qp_modify failed ret(%d) phy_id(%u) qpn(%u)",
            ret, qp_peer->phy_id, qp_peer->qpn);
    }
    PEER_PTHREAD_MUTEX_UNLOCK(&g_ra_peer_mutex[qp_peer->phy_id]);
    return ret;
}

int ra_peer_qp_connect_async(struct ra_qp_handle *qp_peer, const void *sock_handle)
{
    int ret;
    int fd = ((const struct socket_peer_info *)sock_handle)->fd;

    PEER_PTHREAD_MUTEX_LOCK(&g_ra_peer_mutex[qp_peer->phy_id]);
    rs_set_ctx(qp_peer->phy_id);
    ret = rs_qp_connect_async(qp_peer->phy_id, qp_peer->rdev_index, qp_peer->qpn, fd);
    if (ret) {
        hccp_err("[connect_async][ra_peer_qp]ra qp info sync failed socket fd(%d) ret(%d).", fd, ret);
    }
    PEER_PTHREAD_MUTEX_UNLOCK(&g_ra_peer_mutex[qp_peer->phy_id]);
    return ret;
}

int ra_peer_get_qp_status(struct ra_qp_handle *qp_peer, int *status)
{
    struct rs_qp_status_info qp_info = { 0 };
    int ret;

    PEER_PTHREAD_MUTEX_LOCK(&g_ra_peer_mutex[qp_peer->phy_id]);
    rs_set_ctx(qp_peer->phy_id);
    ret = rs_get_qp_status(qp_peer->phy_id, qp_peer->rdev_index, qp_peer->qpn, &qp_info);
    if (ret) {
        hccp_err("[get][ra_peer_qp_status]ra get qp status failed ret(%d).", ret);
    }
    PEER_PTHREAD_MUTEX_UNLOCK(&g_ra_peer_mutex[qp_peer->phy_id]);
    *status = qp_info.status;
    return ret;
}

STATIC int ra_peer_loopback_qp_modify_prepare(struct ra_qp_handle *qp_handle, struct typical_qp *qp_info)
{
    int ret = 0;

    qp_info->qpn = qp_handle->qpn;
    qp_info->psn = qp_handle->psn;
    qp_info->gid_idx = qp_handle->gid_idx;
    qp_info->retry_cnt = QP_DEFAULT_MAX_ATTR_RETRY_CNT;
    qp_info->retry_time = QP_DEFAULT_MAX_ATTR_TIMEOUT;
    ret = memcpy_s(qp_info->gid, sizeof(qp_info->gid), qp_handle->rdma_handle->gid,
        sizeof(qp_handle->rdma_handle->gid));
    CHK_PRT_RETURN(ret != 0, hccp_err("memcpy_s gid failed, ret:%d, dst_len:%u, src_len:%d", ret,
        sizeof(qp_info->gid), qp_handle->rdma_handle->gid), -ESAFEFUNC);

    return ret;
}

STATIC int ra_peer_loopback_qp_modify(struct ra_qp_handle *qp_handle0, struct ra_qp_handle *qp_handle1)
{
    struct typical_qp qp0_info = {0};
    struct typical_qp qp1_info = {0};
    int ret = 0;

    ret = ra_peer_loopback_qp_modify_prepare(qp_handle0, &qp0_info);
    CHK_PRT_RETURN(ret != 0, hccp_err("ra_peer_loopback_qp_modify_prepare qp0 failed, ret:%d", ret), ret);
    ret = ra_peer_loopback_qp_modify_prepare(qp_handle1, &qp1_info);
    CHK_PRT_RETURN(ret != 0, hccp_err("ra_peer_loopback_qp_modify_prepare qp1 failed, ret:%d", ret), ret);

    ret = ra_peer_typical_qp_modify(qp_handle0, &qp0_info, &qp1_info);
    CHK_PRT_RETURN(ret, hccp_err("ra_peer_typical_qp_modify qp0 failed, ret:%d", ret), ret);
    ret = ra_peer_typical_qp_modify(qp_handle1, &qp1_info, &qp0_info);
    CHK_PRT_RETURN(ret, hccp_err("ra_peer_typical_qp_modify 1p1 failed, ret:%d", ret), ret);

    return ret;
}

STATIC void ra_peer_loopback_single_qp_destroy(struct ra_qp_handle *qp_handle)
{
    struct ra_loopback_info *loopback_info = qp_handle->loopback_info;
    struct ra_rdma_handle *rdma_handle = qp_handle->rdma_handle;
    struct cq_attr attr = {0};

    attr.qp_context = &(loopback_info->cq_context);
    attr.ib_send_cq = &(loopback_info->ib_send_cq);
    attr.ib_recv_cq = &(loopback_info->ib_recv_cq);

    (void)ra_peer_normal_qp_destroy(qp_handle);

    (void)ra_peer_cq_destroy(rdma_handle, &attr);

    free(loopback_info);
    loopback_info = NULL;
}

STATIC int ra_peer_loopback_single_qp_create(struct ra_rdma_handle *rdma_handle, struct ra_qp_handle **qp_handle,
    struct ibv_qp **qp)
{
    struct ra_loopback_info *loopback_info = NULL;
    struct ibv_qp_init_attr qp_init_attr = {0};
    struct ibv_qp_cap qp_cap = {0};
    struct cq_attr cq_attr = {0};
    int ret = 0;

    loopback_info = (struct ra_loopback_info *)calloc(1, sizeof(struct ra_loopback_info));
    CHK_PRT_RETURN(loopback_info == NULL, hccp_err("loopback_info calloc failed"),
        -ENOMEM);

    cq_attr.qp_context = &(loopback_info->cq_context);
    cq_attr.ib_send_cq = &(loopback_info->ib_send_cq);
    cq_attr.ib_recv_cq = &(loopback_info->ib_recv_cq);
    cq_attr.send_cq_depth = CQ_DEFAULT_MIN_SEND_DEPTH;
    cq_attr.recv_cq_depth = CQ_DEFAULT_MIN_RECV_DEPTH;
    ret = ra_peer_cq_create(rdma_handle, &cq_attr);
    if (ret != 0) {
        hccp_err("ra_peer_cq_create failed, ret:%d", ret);
        goto cq_create_err;
    }

    qp_cap.max_send_wr = QP_DEFAULT_MIN_CAP_SEND_WR;
    qp_cap.max_recv_wr = QP_DEFAULT_MIN_CAP_RECV_WR;
    qp_cap.max_send_sge = QP_DEFAULT_MIN_CAP_SEND_SGE;
    qp_cap.max_recv_sge = QP_DEFAULT_MIN_CAP_RECV_SGE;
    qp_cap.max_inline_data = QP_DEFAULT_MAX_CAP_INLINE_DATA;
    qp_init_attr.qp_context = *(cq_attr.qp_context);
    qp_init_attr.send_cq = *(cq_attr.ib_send_cq);
    qp_init_attr.recv_cq = *(cq_attr.ib_recv_cq);
    qp_init_attr.qp_type = IBV_QPT_RC;
    qp_init_attr.cap = qp_cap;
    ret = ra_peer_normal_qp_create(rdma_handle, &qp_init_attr, (void **)qp_handle, (void **)qp);
    if (ret != 0) {
        hccp_err("ra_peer_normal_qp_create failed, ret:%d", ret);
        goto qp_create_err;
    }
    (*qp_handle)->loopback_info = loopback_info;
    return ret;

qp_create_err:
    (void)ra_peer_cq_destroy(rdma_handle, &cq_attr);
cq_create_err:
    free(loopback_info);
    loopback_info = NULL;
    return ret;
}

int ra_peer_loopback_qp_create(struct ra_rdma_handle *rdma_handle, struct loopback_qp_pair *qp_pair, void **qp_handle)
{
    struct ra_qp_handle *qp_handle0 = NULL;
    struct ra_qp_handle *qp_handle1 = NULL;
    struct ibv_qp *qp0 = NULL;
    struct ibv_qp *qp1 = NULL;
    int ret;

    ret = ra_peer_loopback_single_qp_create(rdma_handle, &qp_handle0, &qp0);
    CHK_PRT_RETURN(ret != 0, hccp_err("ra_peer_loopback_single_qp_create qp0 failed, ret:%d", ret), ret);

    ret = ra_peer_loopback_single_qp_create(rdma_handle, &qp_handle1, &qp1);
    if (ret != 0) {
        hccp_err("ra_peer_loopback_single_qp_create qp1 failed, ret:%d", ret);
        goto qp1_create_err;
    }

    ret = ra_peer_loopback_qp_modify(qp_handle0, qp_handle1);
    if (ret != 0) {
        hccp_err("ra_peer_loopback_qp_modify failed, ret:%d", ret);
        goto qp_modify_err;
    }

    qp_pair->ibv_qp0 = qp0;
    qp_pair->ibv_qp1 = qp1;
    qp_handle0->loopback_qp_handle = qp_handle1;
    qp_handle1->loopback_qp_handle = qp_handle0;
    *qp_handle = qp_handle0;
    return ret;

qp_modify_err:
    ra_peer_loopback_single_qp_destroy(qp_handle1);
qp1_create_err:
    ra_peer_loopback_single_qp_destroy(qp_handle0);
    return ret;
}

STATIC void ra_peer_loopback_qp_destroy(struct ra_qp_handle *qp_handle0)
{
    struct ra_qp_handle *qp_handle1 = qp_handle0->loopback_qp_handle;

    ra_peer_loopback_single_qp_destroy(qp_handle1);
    ra_peer_loopback_single_qp_destroy(qp_handle0);
}

STATIC int ra_peer_single_qp_destroy(struct ra_qp_handle *qp_peer)
{
    int ret = 0;

    PEER_PTHREAD_MUTEX_LOCK(&g_ra_peer_mutex[qp_peer->phy_id]);
    rs_set_ctx(qp_peer->phy_id);
    ret = rs_qp_destroy(qp_peer->phy_id, qp_peer->rdev_index, qp_peer->qpn);
    if (ret != 0) {
        hccp_err("[destroy][ra_peer_qp]destroy failed ret(%d)", ret);
    }
    PEER_PTHREAD_MUTEX_UNLOCK(&g_ra_peer_mutex[qp_peer->phy_id]);
    free(qp_peer);
    qp_peer = NULL;
    return ret;
}

int ra_peer_qp_destroy(struct ra_qp_handle *qp_peer)
{
    if (qp_peer->loopback_qp_handle == NULL) {
        return ra_peer_single_qp_destroy(qp_peer);
    }

    ra_peer_loopback_qp_destroy(qp_peer);

    return 0;
}

int ra_peer_send_wr(struct ra_qp_handle *qp_peer, struct send_wr *wr, struct send_wr_rsp *wr_rsp)
{
    int ret;

    PEER_PTHREAD_MUTEX_LOCK(&g_ra_peer_mutex[qp_peer->phy_id]);
    rs_set_ctx(qp_peer->phy_id);
    ret = rs_send_wr(qp_peer->phy_id, qp_peer->rdev_index, qp_peer->qpn, wr, wr_rsp);
    if (ret) {
        hccp_err("[send][ra_peer_wr]ra_send_wr failed ret(%d)", ret);
    }
    PEER_PTHREAD_MUTEX_UNLOCK(&g_ra_peer_mutex[qp_peer->phy_id]);
    return ret;
}

int ra_peer_send_wrlist(struct ra_qp_handle *qp_handle, struct send_wrlist_data wr[], struct send_wr_rsp op_rsp[],
    struct wrlist_send_complete_num wrlist_num)
{
    int ret;
    unsigned int complete_cnt = 0;
    unsigned int send_cnt = 0;
    struct rs_wrlist_base_info base_info;
    struct wrlist_send_complete_num wrlist_once;
    unsigned int compelet_once_cnt, i;
    struct wr_info *wr_list = NULL;

    base_info.phy_id = qp_handle->phy_id;
    base_info.rdev_index = qp_handle->rdev_index;
    base_info.qpn = qp_handle->qpn;
    base_info.key_flag = 0;
    wr_list = calloc(wrlist_num.send_num, sizeof(struct wr_info));
    CHK_PRT_RETURN(wr_list == NULL, hccp_err("wr_list calloc failed."), -ENOMEM);

    for (i = 0; i < wrlist_num.send_num; i++) {
        wr_list[i].op = wr[i].op;
        wr_list[i].send_flags = wr[i].send_flags;
        wr_list[i].dst_addr = wr[i].dst_addr;
        wr_list[i].mem_list.addr = wr[i].mem_list.addr;
        wr_list[i].mem_list.len = wr[i].mem_list.len;
        wr_list[i].mem_list.lkey = wr[i].mem_list.lkey;
    }

    while (send_cnt < wrlist_num.send_num) {
        wrlist_once.send_num = (wrlist_num.send_num - send_cnt) > MAX_WR_NUM ? MAX_WR_NUM :
            (wrlist_num.send_num - send_cnt);

        PEER_PTHREAD_MUTEX_LOCK(&g_ra_peer_mutex[base_info.phy_id]);
        rs_set_ctx(base_info.phy_id);
        ret = rs_send_wrlist(base_info, &wr_list[send_cnt], wrlist_once.send_num, &op_rsp[send_cnt],
            &compelet_once_cnt);
        if (ret) {
            hccp_err("[send][ra_peer_wrlist]ra_peer_send_wrlist failed ret[%d], send_num[%u], send_cnt[%u]", ret,
                wrlist_num.send_num, send_cnt);
            PEER_PTHREAD_MUTEX_UNLOCK(&g_ra_peer_mutex[base_info.phy_id]);
            goto alloc_wr_list_fail;
        }
        PEER_PTHREAD_MUTEX_UNLOCK(&g_ra_peer_mutex[base_info.phy_id]);
        send_cnt += wrlist_once.send_num;
        complete_cnt += compelet_once_cnt;
    }

    if (send_cnt != complete_cnt) {
        hccp_err("[send][ra_peer_wrlist]complete_cnt[%u] != send_cnt[%u]", complete_cnt, send_cnt);
        ret = -EINVAL;
    } else {
        *(wrlist_num.complete_num) = complete_cnt;
    }

alloc_wr_list_fail:
    free(wr_list);
    wr_list = NULL;
    return ret;
}

int ra_peer_get_notify_base_addr(struct ra_rdma_handle *handle, unsigned long long *va, unsigned long long *size)
{
    struct mr_info info = { 0 };
    int ret;

    PEER_PTHREAD_MUTEX_LOCK(&g_ra_peer_mutex[handle->rdev_info.phy_id]);
    rs_set_ctx(handle->rdev_info.phy_id);
    ret = rs_get_notify_mr_info(handle->rdev_info.phy_id, handle->rdev_index, &info);
    if (ret) {
        hccp_err("[get][ra_peer_notify_base_addr]rs_get_notify_mr_info failed ret(%d)", ret);
    }
    *va = (unsigned long long)(uintptr_t)info.addr;
    *size = info.size;
    PEER_PTHREAD_MUTEX_UNLOCK(&g_ra_peer_mutex[handle->rdev_info.phy_id]);
    return ret;
}

int ra_peer_init(struct ra_init_config *cfg, unsigned int white_list_status)
{
    int ret;

    hccp_info("[init][ra_peer]ra_peer_init phy_id[%d] start", cfg->phy_id);

    /* In peer online mode chip id equals to phy id */
    struct rs_init_config rs_peer_online_cfg = {
        .chip_id = cfg->phy_id,
        .hccp_mode = cfg->nic_position,
        .white_list_status = white_list_status,
    };
    ret = dl_hal_init();
    if (ret) {
        hccp_err("[init][ra_peer]dl_hal_init failed, ret = %d", ret);
        return ret;
    }

    int counter = __sync_fetch_and_add(&(g_ra_init_counter[cfg->phy_id]), 1);
    if (counter > 0) {
        PEER_PTHREAD_MUTEX_LOCK(&g_ra_peer_mutex[cfg->phy_id]);
        rs_set_ctx(cfg->phy_id);
        PEER_PTHREAD_MUTEX_UNLOCK(&g_ra_peer_mutex[cfg->phy_id]);
        hccp_warn("ra peer has been init for device %u!", cfg->phy_id);
        return 0;
    }

    ret = pthread_mutex_init(&g_ra_peer_mutex[cfg->phy_id], NULL);
    CHK_PRT_RETURN(ret, hccp_err("[init][ra_peer]pthread_mutex_init failed, ret(%d) phy_id(%u)",
        ret, cfg->phy_id), -ESYSFUNC);

    PEER_PTHREAD_MUTEX_LOCK(&g_ra_peer_mutex[cfg->phy_id]);
    rs_set_ctx(cfg->phy_id);
    ret = rs_init(&rs_peer_online_cfg);
    if (ret) {
        hccp_err("[init][ra_peer]rs init failed(%d)", ret);
        PEER_PTHREAD_MUTEX_UNLOCK(&g_ra_peer_mutex[cfg->phy_id]);
        pthread_mutex_destroy(&g_ra_peer_mutex[cfg->phy_id]);
        return ret;
    }
    PEER_PTHREAD_MUTEX_UNLOCK(&g_ra_peer_mutex[cfg->phy_id]);
    hccp_info("[init][ra_peer]ra_peer_init phy_id[%d] succ", cfg->phy_id);
    return ret;
}

int ra_peer_get_tls_enable(unsigned int phy_id, bool *tls_enable)
{
    int ret;

    ra_peer_mutex_lock(phy_id);
    rs_set_ctx(phy_id);
    ret = rs_get_tls_enable(phy_id, tls_enable);
    if (ret != 0) {
        hccp_err("[get][tls_enable]rs_get_tls_enable failed, ret(%d) phy_id(%u)", ret, phy_id);
    }
    ra_peer_mutex_unlock(phy_id);
    return ret;
}

int ra_peer_get_sec_random(unsigned int *value)
{
    int ret;

    ret = rs_get_sec_random(value);
    if (ret != 0) {
        hccp_run_warn("[get_random] unsuccessful, ret(%d)", ret);
    }
    return ret;
}

int ra_peer_deinit(struct ra_init_config *cfg)
{
    int ret = 0;

    hccp_info("[deinit][ra_peer]ra_peer_deinit phy_id[%d] start", cfg->phy_id);

    /* In peer online mode chip id equals to phy id */
    struct rs_init_config rs_peer_online_cfg = {
        .chip_id = cfg->phy_id,
        .hccp_mode = cfg->nic_position,
        .white_list_status = WHITE_LIST_ENABLE,
    };

    if (__sync_fetch_and_sub(&(g_ra_init_counter[cfg->phy_id]), 1) > 1) {
        goto dl_deinit;
    }

    PEER_PTHREAD_MUTEX_LOCK(&g_ra_peer_mutex[cfg->phy_id]);
    rs_set_ctx(cfg->phy_id);
    ret = rs_deinit(&rs_peer_online_cfg);
    // no need to destroy lock & return immediately for retry
    if (ret == -EAGAIN) {
        hccp_warn("[deinit][ra_peer]rs deinit unsuccessful(%d)", ret);
        PEER_PTHREAD_MUTEX_UNLOCK(&g_ra_peer_mutex[cfg->phy_id]);
        return ret;
    }

    if (ret) {
        hccp_err("[deinit][ra_peer]rs deinit failed(%d)", ret);
    }
    PEER_PTHREAD_MUTEX_UNLOCK(&g_ra_peer_mutex[cfg->phy_id]);
    pthread_mutex_destroy(&g_ra_peer_mutex[cfg->phy_id]);

dl_deinit:
    dl_hal_deinit();
    hccp_info("[deinit][ra_peer]ra_peer_deinit phy_id[%d] succ", cfg->phy_id);
    return ret;
}

int ra_peer_get_ifnum(unsigned int phy_id, unsigned int *num)
{
    int ret;
    hccp_info("[get][ra_peer_ifnum]ra_peer_get_ifnum phy_id[%u] start", phy_id);
    PEER_PTHREAD_MUTEX_LOCK(&g_ra_peer_mutex[phy_id]);
    rs_set_ctx(phy_id);
    ret = rs_peer_get_ifnum(phy_id, num);
    if (ret) {
        hccp_err("[get][ra_peer_ifnum]rs_peer_get_ifnum failed(%d)", ret);
    }
    PEER_PTHREAD_MUTEX_UNLOCK(&g_ra_peer_mutex[phy_id]);
    hccp_info("[get][ra_peer_ifnum]ra_peer_get_ifnum phy_id[%u] succ", phy_id);
    return ret;
}

int ra_peer_get_ifaddrs(unsigned int phy_id, struct interface_info interface_infos[], unsigned int *num)
{
    int ret;
    hccp_info("[get][ra_peer_ifaddrs] ra_peer_get_ifaddrs phy_id[%u] start", phy_id);
    PEER_PTHREAD_MUTEX_LOCK(&g_ra_peer_mutex[phy_id]);
    rs_set_ctx(phy_id);
    ret = rs_peer_get_ifaddrs(interface_infos, num, phy_id);
    if (ret) {
        hccp_err("[get][ra_peer_ifaddrs]rs_peer_get_ifaddrs failed(%d)", ret);
    }
    PEER_PTHREAD_MUTEX_UNLOCK(&g_ra_peer_mutex[phy_id]);
    hccp_info("[get][ra_peer_ifaddrs] ra_peer_get_ifaddrs phy_id[%u] succ", phy_id);
    return ret;
}

#define PAGE_SHIFT              12

int host_notify_base_addr_init(unsigned int phy_id)
{
    int ret, ret_val;
    unsigned int notify_size = 0;
    unsigned long long *notify_va = NULL;
    unsigned int logic_id = 0;

    ret = dl_drv_device_get_index_by_phy_id(phy_id, &logic_id);
    CHK_PRT_RETURN(ret, hccp_err("[init][base_addr]drvDeviceGetIndexByPhyId failed, ret(%d), phy_id(%u)",
        ret, phy_id), ret);

    ret = dl_hal_notify_get_info(logic_id, 0, RA_NOTIFY_TYPE_TOTAL_SIZE, &notify_size);
    CHK_PRT_RETURN(ret, hccp_err("[init][base_addr]halNotifyGetInfo failed, ret(%d), logic_id(%u)",
        ret, logic_id), ret);

    g_notify_fd = open(HOST_DEVICE_NAME, O_RDWR);
    CHK_PRT_RETURN(g_notify_fd < 0, hccp_err("[init][base_addr]Failed to open file_path[%s], err_code[%d]",
        HOST_DEVICE_NAME, errno), -ENOENT);

    notify_va = mmap(NULL, notify_size, PROT_READ | PROT_WRITE, MAP_SHARED,
        g_notify_fd, (unsigned long long)logic_id << PAGE_SHIFT);
    if (notify_va == MAP_FAILED) {
        hccp_err("[init][base_addr]failed to mmap recv buf, fd[%d]", g_notify_fd);
        ret = -ENOMEM;
        goto close_fd;
    }

    ret = rs_notify_cfg_set(phy_id, (uintptr_t)notify_va, notify_size);
    if (ret) {
        hccp_err("[init][base_addr]ra_hdc_notify_cfg_set failed, ret(%d), phy_id(%u)", ret, phy_id);
        goto unmmap_mem;
    }
    return 0;

unmmap_mem:
    ret_val = munmap((void *)notify_va, notify_size);
    if (ret_val) {
        hccp_err("[init][base_addr]munmap buf munmap error, length:%lu, ret:%d", notify_size, ret_val);
    }
close_fd:
    HCCP_CLOSE_RETRY_FOR_EINTR(g_notify_fd);
    return ret;
}

int ra_peer_notify_base_addr_init(unsigned int notify_type, unsigned int phy_id)
{
    switch (notify_type) {
        case NOTIFY: return host_notify_base_addr_init(phy_id);
        case EVENTID: return 0;
        case NO_USE: return 0;
        default: {
            hccp_err("[init][base_addr]notify_type[%u] error", notify_type);
            return -EINVAL;
        }
    }
}

int host_notify_base_addr_uninit(unsigned int phy_id)
{
    int ret;
    unsigned long long va, size;
    unsigned int logic_id = 0;
    struct host_roce_notify_info notify_node = {0};

    ret = dl_drv_device_get_index_by_phy_id(phy_id, &logic_id);
    CHK_PRT_RETURN(ret, hccp_err("[uninit][base_addr]drvDeviceGetIndexByPhyId failed, ret(%d), phy_id(%u)",
        ret, phy_id), ret);

    ret = rs_notify_cfg_get(phy_id, &va, &size);
    CHK_PRT_RETURN(ret, hccp_err("[uninit][base_addr]rs_notify_cfg_get failed, ret(%d), phy_id(%u)",
        ret, phy_id), ret);
    notify_node.logic_id = logic_id;
    notify_node.va = va;
    notify_node.sz = size;

    CHK_PRT_RETURN(g_notify_fd < 0, hccp_err("[uninit][base_addr]file_path[%s] has closed",
        HOST_DEVICE_NAME), -ENOENT);

    ret = ioctl(g_notify_fd, HOST_CDEV_IOC_FREE_NOTIFY, &notify_node);
    if (ret < 0) {
        hccp_err("[uninit][base_addr]Failed to run ioctl, ret[%d], err_code[%d].", ret, errno);
        HCCP_CLOSE_RETRY_FOR_EINTR(g_notify_fd);
        return ret;
    }

    ret = munmap((void *)(uintptr_t)va, size);
    if (ret) {
        hccp_err("[uninit][base_addr]munmap buf munmap error, *size:%lu, ret:%d", size, ret);
        HCCP_CLOSE_RETRY_FOR_EINTR(g_notify_fd);
        return ret;
    }

    HCCP_CLOSE_RETRY_FOR_EINTR(g_notify_fd);
    return 0;
}

int notify_base_addr_uninit(unsigned int notify_type, unsigned int phy_id)
{
    switch (notify_type) {
        case NOTIFY: return host_notify_base_addr_uninit(phy_id);
        case EVENTID: return 0;
        case NO_USE: return 0;
        default: {
            hccp_err("[uninit][base_addr]notify_type[%u] error", notify_type);
            return -EINVAL;
        }
    }
}

int ra_peer_rdev_init(
    struct ra_rdma_handle *rdma_handle, unsigned int notify_type, struct rdev rdev_info, unsigned int *rdev_index)
{
    int ret, ret_val;

    hccp_run_info("[init][ra_peer_rdev]ra_peer_rdev_init phy_id[%d] notify_type[%u] physical device id[%u]",
        rdev_info.phy_id, notify_type, rdma_handle->rdev_info.phy_id);

    rs_set_ctx(rdev_info.phy_id);
    ret = ra_peer_notify_base_addr_init(notify_type, rdev_info.phy_id);
    CHK_PRT_RETURN(ret, hccp_err("[init][ra_peer_rdev] ra_peer_notify_base_addr_init failed[%d]", ret), ret);

    PEER_PTHREAD_MUTEX_LOCK(&g_ra_peer_mutex[rdev_info.phy_id]);
    ret = rs_rdev_init(rdev_info, notify_type, rdev_index);
    if (ret) {
        hccp_err("[init][ra_peer_rdev] rs_rdev_init failed[%d]", ret);
        PEER_PTHREAD_MUTEX_UNLOCK(&g_ra_peer_mutex[rdev_info.phy_id]);
        goto notify_base_addr_uninit;
    }
    PEER_PTHREAD_MUTEX_UNLOCK(&g_ra_peer_mutex[rdev_info.phy_id]);

    return 0;
notify_base_addr_uninit:
    ret_val = notify_base_addr_uninit(notify_type, rdev_info.phy_id);
    CHK_PRT_RETURN(ret_val, hccp_err("[init][ra_peer_rdev] notify_base_addr_uninit failed, ret(%d)",
        ret_val), ret_val);
    return ret;
}

int ra_peer_rdev_deinit(struct ra_rdma_handle *rdma_handle, unsigned int notify_type)
{
    int ret;

    hccp_info("[deinit][ra_peer_rdev]ra_peer_rdev_deinit phy_id[%d]", rdma_handle->rdev_info.phy_id);
    PEER_PTHREAD_MUTEX_LOCK(&g_ra_peer_mutex[rdma_handle->rdev_info.phy_id]);
    rs_set_ctx(rdma_handle->rdev_info.phy_id);
    ret = rs_rdev_deinit(rdma_handle->rdev_info.phy_id, notify_type, rdma_handle->rdev_index);
    if (ret) {
        hccp_err("[deinit][ra_peer_rdev] rs_rdev_deinit failed[%d]", ret);
        PEER_PTHREAD_MUTEX_UNLOCK(&g_ra_peer_mutex[rdma_handle->rdev_info.phy_id]);
        return ret;
    }
    PEER_PTHREAD_MUTEX_UNLOCK(&g_ra_peer_mutex[rdma_handle->rdev_info.phy_id]);

    ret = notify_base_addr_uninit(notify_type, rdma_handle->rdev_info.phy_id);
    CHK_PRT_RETURN(ret, hccp_err("[deinit][ra_peer_rdev] notify_base_addr_uninit failed, ret(%d)", ret), ret);

    return 0;
}

int ra_peer_set_tsqp_depth(struct ra_rdma_handle *rdma_handle, unsigned int temp_depth, unsigned int *qp_num)
{
    int ret;
    hccp_info("[set][peer_set_tsqp_depth]ra_peer_set_tsqp_depth phy_id[%d]", rdma_handle->rdev_info.phy_id);
    PEER_PTHREAD_MUTEX_LOCK(&g_ra_peer_mutex[rdma_handle->rdev_info.phy_id]);
    rs_set_ctx(rdma_handle->rdev_info.phy_id);
    ret = rs_set_tsqp_depth(rdma_handle->rdev_info.phy_id, rdma_handle->rdev_index, temp_depth, qp_num);
    if (ret) {
        hccp_err("[set][peer_set_tsqp_depth] rs_set_tsqp_depth failed[%d]", ret);
        PEER_PTHREAD_MUTEX_UNLOCK(&g_ra_peer_mutex[rdma_handle->rdev_info.phy_id]);
        return ret;
    }

    PEER_PTHREAD_MUTEX_UNLOCK(&g_ra_peer_mutex[rdma_handle->rdev_info.phy_id]);
    return 0;
}

int ra_peer_get_tsqp_depth(struct ra_rdma_handle *rdma_handle, unsigned int *temp_depth, unsigned int *qp_num)
{
    int ret;

    hccp_info("[get][peer_get_tsqp_depth]ra_peer_get_tsqp_depth phy_id[%d]", rdma_handle->rdev_info.phy_id);
    PEER_PTHREAD_MUTEX_LOCK(&g_ra_peer_mutex[rdma_handle->rdev_info.phy_id]);
    rs_set_ctx(rdma_handle->rdev_info.phy_id);
    ret = rs_get_tsqp_depth(rdma_handle->rdev_info.phy_id, rdma_handle->rdev_index, temp_depth, qp_num);
    if (ret) {
        hccp_err("[get][peer_set_tsqp_depth]rs_get_tsqp_depth failed[%d]", ret);
        PEER_PTHREAD_MUTEX_UNLOCK(&g_ra_peer_mutex[rdma_handle->rdev_info.phy_id]);
        return ret;
    }

    PEER_PTHREAD_MUTEX_UNLOCK(&g_ra_peer_mutex[rdma_handle->rdev_info.phy_id]);
    return ret;
}

int ra_peer_recv_wrlist(struct ra_qp_handle *qp_handle, struct recv_wrlist_data *wr, unsigned int recv_num,
    unsigned int *complete_num)
{
    int ret;
    struct rs_wrlist_base_info base_info = {0};
    unsigned int complete_cnt = 0;
    unsigned int recv_cnt = 0;
    unsigned int recv_num_per;
    unsigned int compelet_once_cnt;

    base_info.phy_id = qp_handle->phy_id;
    base_info.rdev_index = qp_handle->rdev_index;
    base_info.qpn = qp_handle->qpn;

    while (recv_cnt < recv_num) {
        recv_num_per = (recv_num - recv_cnt) > MAX_WR_NUM ? MAX_WR_NUM :
            (recv_num - recv_cnt);

        PEER_PTHREAD_MUTEX_LOCK(&g_ra_peer_mutex[base_info.phy_id]);
        rs_set_ctx(base_info.phy_id);
        ret = rs_recv_wrlist(base_info, &wr[recv_cnt], recv_num_per, &compelet_once_cnt);
        if (ret) {
            hccp_err("[recv][peer_recv_wrlist]ra_peer_recv_wrlist failed ret[%d], recv_cnt[%u], recv_num_per[%u]",
                ret, recv_cnt, recv_num_per);
            PEER_PTHREAD_MUTEX_UNLOCK(&g_ra_peer_mutex[base_info.phy_id]);
            return ret;
        }
        PEER_PTHREAD_MUTEX_UNLOCK(&g_ra_peer_mutex[base_info.phy_id]);
        recv_cnt += recv_num_per;
        complete_cnt += compelet_once_cnt;
    }

    CHK_PRT_RETURN(recv_cnt != complete_cnt, hccp_err("[recv][peer_recv_wrlist]complete_cnt[%u] != recv_cnt[%u]",
        complete_cnt, recv_cnt), -EINVAL);

    *complete_num = complete_cnt;
    return 0;
}

int ra_peer_get_qp_context(struct ra_qp_handle *qp_peer, void** qp, void** send_cq, void** recv_cq)
{
    int ret;
    rs_set_ctx(qp_peer->phy_id);
    ret = rs_get_qp_context(qp_peer->phy_id, qp_peer->rdev_index, qp_peer->qpn, qp, send_cq, recv_cq);
    if (ret) {
        hccp_err("[get][rs_get_qp_context]ra_peer_get_qp_context failed ret(%d)", ret);
    }
    return ret;
}

int ra_peer_cq_create(struct ra_rdma_handle *rdma_handle, struct cq_attr *attr)
{
    int ret;
    unsigned int phy_id = rdma_handle->rdev_info.phy_id;

    PEER_PTHREAD_MUTEX_LOCK(&g_ra_peer_mutex[phy_id]);
    rs_set_ctx(phy_id);
    ret = rs_cq_create(phy_id, rdma_handle->rdev_index, attr);
    if (ret) {
        hccp_err("[create][ra_peer_cq_create]rs_cq_create failed ret[%d]", ret);
    }
    PEER_PTHREAD_MUTEX_UNLOCK(&g_ra_peer_mutex[phy_id]);

    return ret;
}

int ra_peer_cq_destroy(struct ra_rdma_handle *rdma_handle, struct cq_attr *attr)
{
    int ret;
    unsigned int phy_id = rdma_handle->rdev_info.phy_id;

    PEER_PTHREAD_MUTEX_LOCK(&g_ra_peer_mutex[phy_id]);
    rs_set_ctx(phy_id);
    ret = rs_cq_destroy(phy_id, rdma_handle->rdev_index, attr);
    if (ret) {
        hccp_err("[destroy][ra_peer_cq_destroy]rs_cq_destroy failed ret[%d]", ret);
    }
    PEER_PTHREAD_MUTEX_UNLOCK(&g_ra_peer_mutex[phy_id]);

    return ret;
}

int ra_peer_normal_qp_create(struct ra_rdma_handle *rdma_handle, struct ibv_qp_init_attr *qp_init_attr,
    void **qp_handle, void **qp)
{
    unsigned int phy_id = rdma_handle->rdev_info.phy_id;
    struct ra_qp_handle *qp_peer = NULL;
    struct rs_qp_resp qp_resp = { 0 };
    int ret;

    qp_peer = (struct ra_qp_handle *)calloc(1, sizeof(struct ra_qp_handle));
    CHK_PRT_RETURN(qp_peer == NULL, hccp_err("[create][ra_normal_peer_qp]normal_qp_peer calloc failed."), -ENOMEM);

    PEER_PTHREAD_MUTEX_LOCK(&g_ra_peer_mutex[phy_id]);
    rs_set_ctx(phy_id);
    ret = rs_normal_qp_create(phy_id, rdma_handle->rdev_index, qp_init_attr, &qp_resp, qp);
    if (ret) {
        hccp_err("[create][ra_normal_peer_qp]rs_normal_qp_create failed ret[%d]", ret);
        PEER_PTHREAD_MUTEX_UNLOCK(&g_ra_peer_mutex[phy_id]);
        goto calloc_err;
    }
    PEER_PTHREAD_MUTEX_UNLOCK(&g_ra_peer_mutex[phy_id]);
    qp_peer->phy_id = phy_id;
    qp_peer->qpn = qp_resp.qpn;
    qp_peer->psn = qp_resp.psn;
    qp_peer->gid_idx = qp_resp.gid_idx;
    qp_peer->rdev_index = rdma_handle->rdev_index;
    qp_peer->rdma_handle = rdma_handle;
    qp_peer->rdma_ops = rdma_handle->rdma_ops;

    *qp_handle = qp_peer;
    return ret;

calloc_err:
    free(qp_peer);
    qp_peer = NULL;
    return ret;
}

int ra_peer_normal_qp_destroy(struct ra_qp_handle *qp_peer)
{
    int ret;

    PEER_PTHREAD_MUTEX_LOCK(&g_ra_peer_mutex[qp_peer->phy_id]);
    rs_set_ctx(qp_peer->phy_id);
    ret = rs_normal_qp_destroy(qp_peer->phy_id, qp_peer->rdev_index, qp_peer->qpn);
    if (ret) {
        hccp_err("[destroy][ra_peer_normal_qp]ra close failed ret(%d)", ret);
    }
    PEER_PTHREAD_MUTEX_UNLOCK(&g_ra_peer_mutex[qp_peer->phy_id]);
    free(qp_peer);
    qp_peer = NULL;
    return ret;
}

int ra_peer_set_qp_attr_qos(struct ra_qp_handle *qp_peer, struct qos_attr *attr)
{
    rs_set_ctx(qp_peer->phy_id);
    int ret = rs_set_qp_attr_qos(qp_peer->phy_id, qp_peer->rdev_index, qp_peer->qpn, attr);
    CHK_PRT_RETURN(ret, hccp_err("[ra_peer_set_qp_attr_qos]rs_set_qp_attr_qos failed ret(%d)", ret), ret);
    return ret;
}

int ra_peer_set_qp_attr_timeout(struct ra_qp_handle *qp_peer, unsigned int *timeout)
{
    rs_set_ctx(qp_peer->phy_id);
    int ret = rs_set_qp_attr_timeout(qp_peer->phy_id, qp_peer->rdev_index, qp_peer->qpn, timeout);
    CHK_PRT_RETURN(ret, hccp_err("[ra_peer_set_qp_attr_timeout]rs_set_qp_attr_timeout failed ret(%d)", ret), ret);
    return ret;
}

int ra_peer_set_qp_attr_retry_cnt(struct ra_qp_handle *qp_peer, unsigned int *retry_cnt)
{
    rs_set_ctx(qp_peer->phy_id);
    int ret = rs_set_qp_attr_retry_cnt(qp_peer->phy_id, qp_peer->rdev_index, qp_peer->qpn, retry_cnt);
    CHK_PRT_RETURN(ret, hccp_err("[ra_peer_set_qp_attr_retry_cnt]rs_set_qp_attr_retry_cnt failed ret(%d)", ret), ret);
    return ret;
}

int ra_peer_create_comp_channel(struct ra_rdma_handle *rdma_handle, void** comp_channel)
{
    int ret;
    rs_set_ctx(rdma_handle->rdev_info.phy_id);
    ret = rs_create_comp_channel(rdma_handle->rdev_info.phy_id, rdma_handle->rdev_index, comp_channel);
    CHK_PRT_RETURN(ret, hccp_err("[ra_peer_create_comp_channel]rs_create_comp_channel failed ret(%d)", ret), ret);

    return ret;
}

int ra_peer_destroy_comp_channel(void* comp_channel)
{
    int ret;

    ret = rs_destroy_comp_channel(comp_channel);
    CHK_PRT_RETURN(ret, hccp_err("[ra_peer_destroy_comp_channel]rs_create_comp_channel failed ret(%d)", ret), ret);

    return ret;
}

int ra_peer_create_srq(struct ra_rdma_handle *rdma_handle, struct srq_attr *attr)
{
    int ret;

    // 创建srq&srq cq
    rs_set_ctx(rdma_handle->rdev_info.phy_id);
    ret = rs_create_srq(rdma_handle->rdev_info.phy_id, rdma_handle->rdev_index, attr);
    CHK_PRT_RETURN(ret, hccp_err("[ra_peer_create_srq]rs_create_srq failed ret(%d)", ret), ret);

    return ret;
}

int ra_peer_destroy_srq(struct ra_rdma_handle *rdma_handle, struct srq_attr *attr)
{
    int ret;

    // 销毁srq&srq cq
    rs_set_ctx(rdma_handle->rdev_info.phy_id);
    ret = rs_destroy_srq(rdma_handle->rdev_info.phy_id, rdma_handle->rdev_index, attr);
    CHK_PRT_RETURN(ret, hccp_err("[ra_peer_destroy_srq]rs_destroy_srq failed ret(%d)", ret), ret);

    return ret;
}

int ra_peer_create_event_handle(int *event_handle)
{
    int ret;

    ret = rs_create_event_handle(event_handle);
    CHK_PRT_RETURN(ret, hccp_err("[ra_peer_create_event_handle]rs_create_event_handle failed ret(%d)", ret), ret);

    return ret;
}

int ra_peer_ctl_event_handle(int event_handle, const void *fd_handle, int opcode, enum RaEpollEvent event)
{
    int ret;

    ret = rs_ctl_event_handle(event_handle, fd_handle, opcode, event);
    CHK_PRT_RETURN(ret, hccp_err("[ra_peer_ctl_event_handle]rs_ctl_event_handle failed ret(%d)", ret), ret);

    return ret;
}

int ra_peer_wait_event_handle(int event_handle, struct socket_event_info *event_infos, int timeout,
    unsigned int maxevents, unsigned int *events_num)
{
    int ret;

    ret = rs_wait_event_handle(event_handle, event_infos, timeout, maxevents, events_num);
    CHK_PRT_RETURN(ret, hccp_err("[ra_peer_wait_event_handle]rs_wait_event_handle failed ret(%d)", ret), ret);

    return ret;
}

int ra_peer_destroy_event_handle(int *event_handle)
{
    int ret;

    ret = rs_destroy_event_handle(event_handle);
    CHK_PRT_RETURN(ret, hccp_err("[ra_peer_destroy_event_handle]rs_destroy_event_handle failed ret(%d)", ret), ret);

    return ret;
}

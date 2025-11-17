/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "securec.h"
#include "user_log.h"
#include "ra.h"
#include "ra_comm.h"
#include "ra_async.h"
#include "ra_rs_comm.h"
#include "ra_rs_err.h"
#include "ra_hdc.h"
#include "ra_hdc_socket.h"
#include "ra_hdc_async.h"
#include "ra_hdc_async_socket.h"

int ra_hdc_socket_send_async(const struct socket_hdc_info *fd_handle, const void *data, unsigned long long size,
    unsigned long long *sent_size, void **req_handle)
{
    unsigned long long send_size = (size > SOCKET_SEND_MAXLEN) ? SOCKET_SEND_MAXLEN : size;
    struct ra_request_handle *req_handle_tmp = NULL;
    union op_socket_send_data *async_data = NULL;
    unsigned int phy_id = fd_handle->phy_id;
    int ret = 0;

    async_data = (union op_socket_send_data *)calloc(sizeof(union op_socket_send_data), sizeof(char));
    CHK_PRT_RETURN(async_data == NULL, hccp_err("[send][ra_hdc_socket]calloc async_data failed, phy_id(%u)", phy_id),
        -ENOMEM);

    async_data->tx_data.fd = (unsigned int)fd_handle->fd;
    async_data->tx_data.send_size = send_size;
    ret = memcpy_s(async_data->tx_data.data_send, SOCKET_SEND_MAXLEN, data, send_size);
    if (ret != 0) {
        hccp_err("[send][ra_hdc_socket]memcpy_s data failed, ret(%d) send_size(%llu) phy_id(%u)",
            ret, send_size, phy_id);
        ret = -ESAFEFUNC;
        goto out;
    }

    req_handle_tmp = (struct ra_request_handle *)calloc(1, sizeof(struct ra_request_handle));
    if (req_handle_tmp == NULL) {
        hccp_err("[send][ra_hdc_socket]calloc req_handle_tmp failed, phy_id[%u]", phy_id);
        ret = -ENOMEM;
        goto out;
    }
    *sent_size = 0;
    req_handle_tmp->priv_data = (void *)sent_size;

    ret = ra_hdc_send_msg_async(RA_RS_SOCKET_SEND, phy_id, (char *)async_data, sizeof(union op_socket_send_data),
        req_handle_tmp);
    if (ret != 0) {
        hccp_err("[send][ra_hdc_socket]hdc async send message process failed ret(%d) phy_id(%u)", ret, phy_id);
        free(req_handle_tmp);
        req_handle_tmp = NULL;
        goto out;
    }

    *req_handle = (void *)req_handle_tmp;

out:
    free(async_data);
    async_data = NULL;
    return ret;
}

void ra_hdc_async_handle_socket_send(struct ra_request_handle *req_handle)
{
    union op_socket_send_data *async_data = NULL;

    if (req_handle->op_ret > 0) {
        async_data = (union op_socket_send_data *)req_handle->recv_buf;
        *(unsigned long long *)req_handle->priv_data = async_data->rx_data.real_send_size;
        req_handle->op_ret = 0;
    } else if (req_handle->op_ret == 0) {
        hccp_warn("[send][ra_hdc_socket]socket has been closed. sent_size is 0");
        *(unsigned long long *)req_handle->priv_data = 0;
        req_handle->op_ret = -ESOCKCLOSED;
    } else {
        if (req_handle->op_ret != -EAGAIN) {
            hccp_warn("[send][ra_hdc_socket]socket send unsuccessful ret(%d) phy_id(%u)", req_handle->op_ret,
                req_handle->phy_id);
        }
        *(unsigned long long *)req_handle->priv_data = 0;
    }

    return;
}

STATIC void ra_hdc_socket_prepare_recv_rsp(struct ra_response_socket_recv *recv_rsp, void *data,
    unsigned long long size, unsigned long long *received_size)
{
    recv_rsp->data = data;
    recv_rsp->size = size;
    *received_size = 0;
    recv_rsp->received_size = received_size;
}

int ra_hdc_socket_recv_async(const struct socket_hdc_info *fd_handle, void *data, unsigned long long size,
    unsigned long long *received_size, void **req_handle)
{
    unsigned long long recv_size = (size > SOCKET_SEND_MAXLEN) ? SOCKET_SEND_MAXLEN : size;
    struct ra_response_socket_recv *recv_rsp = NULL;
    struct ra_request_handle *req_handle_tmp = NULL;
    union op_socket_recv_data *async_data = NULL;
    unsigned int phy_id = fd_handle->phy_id;
    int ret = 0;

    recv_rsp = (struct ra_response_socket_recv *)calloc(1, sizeof(struct ra_response_socket_recv));
    CHK_PRT_RETURN(recv_rsp == NULL, hccp_err("[recv][ra_hdc_socket]calloc recv_rsp failed, phy_id(%u)", phy_id),
        -ENOMEM);
    ra_hdc_socket_prepare_recv_rsp(recv_rsp, data, recv_size, received_size);

    async_data = (union op_socket_recv_data *)calloc(sizeof(union op_socket_recv_data) + recv_size, sizeof(char));
    if (async_data == NULL) {
        hccp_err("[recv][ra_hdc_socket]calloc async_data failed, phy_id(%u)", phy_id);
        ret = -ENOMEM;
        goto free_recv_rsp;
    }

    async_data->tx_data.fd = (unsigned int)fd_handle->fd;
    async_data->tx_data.recv_size = recv_size;
    req_handle_tmp = (struct ra_request_handle *)calloc(1, sizeof(struct ra_request_handle));
    if (req_handle_tmp == NULL) {
        hccp_err("[recv][ra_hdc_socket]calloc req_handle_tmp failed, phy_id[%u]", phy_id);
        ret = -ENOMEM;
        goto out;
    }
    req_handle_tmp->priv_data = (void *)recv_rsp;
    ret = ra_hdc_send_msg_async(RA_RS_SOCKET_RECV, phy_id, (char *)async_data,
        (unsigned int)(sizeof(union op_socket_recv_data) + recv_size), req_handle_tmp);
    if (ret != 0) {
        hccp_err("[recv][ra_hdc_socket]hdc async send message process failed ret(%d) phy_id(%u)", ret, phy_id);
        free(req_handle_tmp);
        req_handle_tmp = NULL;
        goto out;
    }

    free(async_data);
    async_data = NULL;
    *req_handle = (void *)req_handle_tmp;
    return 0;

out:
    free(async_data);
    async_data = NULL;
free_recv_rsp:
    free(recv_rsp);
    recv_rsp = NULL;
    return ret;
}

void ra_hdc_async_handle_socket_recv(struct ra_request_handle *req_handle)
{
    struct ra_response_socket_recv *recv_rsp = NULL;
    union op_socket_recv_data *async_data = NULL;
    unsigned long long real_recv_size = 0;
    unsigned int phy_id = 0;
    int ret = 0;

    phy_id = req_handle->phy_id;
    if (req_handle->op_ret == 0) {
        hccp_warn("[recv][ra_hdc_socket]socket has been closed. received_size is 0");
        req_handle->op_ret = -ESOCKCLOSED;
        goto out;
    } else if (req_handle->op_ret < 0) {
        if (req_handle->op_ret != -EAGAIN) {
            hccp_warn("[recv][ra_hdc_socket]socket recv ret(%d) phy_id(%u)", req_handle->op_ret, phy_id);
        }
        goto out;
    }

    async_data = (union op_socket_recv_data *)req_handle->recv_buf;
    real_recv_size = async_data->rx_data.real_recv_size;
    if (real_recv_size > SOCKET_SEND_MAXLEN) {
        hccp_err("[recv][ra_hdc_socket]real_recv_size:%llu invalid, phy_id(%u)", real_recv_size, phy_id);
        req_handle->op_ret = -EINVAL;
        goto out;
    }

    recv_rsp = (struct ra_response_socket_recv *)req_handle->priv_data;
    ret = memcpy_s(recv_rsp->data, recv_rsp->size, (char *)async_data + sizeof(union op_socket_recv_data),
        real_recv_size);
    if (ret != 0) {
        hccp_err("[recv][ra_hdc_socket]memcpy_s failed, ret(%d) phy_id(%u) size(%llu) real_recv_size(%llu)",
            ret, phy_id, recv_rsp->size, real_recv_size);
        req_handle->op_ret = -ESAFEFUNC;
        goto out;
    }

    req_handle->op_ret = 0;
    *recv_rsp->received_size = real_recv_size;

out:
    free(req_handle->priv_data);
    req_handle->priv_data = NULL;
    return;
}

int ra_hdc_socket_listen_start_async(unsigned int phy_id, struct socket_listen_info_t conn[], unsigned int num,
    void **req_handle)
{
    struct ra_response_socket_listen *async_rsp = NULL;
    struct ra_request_handle *req_handle_tmp = NULL;
    union op_socket_listen_data async_data = {0};
    int ret = 0;

    ret = ra_get_socket_listen_info(conn, num, async_data.tx_data.conn, MAX_SOCKET_NUM);
    CHK_PRT_RETURN(ret != 0, hccp_err("[listen_start][ra_hdc_socket]get_socket_listen_info failed, ret(%d) phy_id(%u)",
        ret, phy_id), -EINVAL);
    async_data.tx_data.num = num | (1U << SOCKET_USE_PORT_BIT);

    async_rsp = (struct ra_response_socket_listen *)calloc(1, sizeof(struct ra_response_socket_listen));
    CHK_PRT_RETURN(async_rsp == NULL, hccp_err("[listen_start][ra_hdc_socket]calloc async_rsp failed, phy_id(%u)",
        phy_id), -ENOMEM);
    async_rsp->conn = conn;
    async_rsp->num = num;
    req_handle_tmp = (struct ra_request_handle *)calloc(1, sizeof(struct ra_request_handle));
    if (req_handle_tmp == NULL) {
        hccp_err("[listen_start][ra_hdc_socket]calloc ra_request_handle failed, phy_id[%u]", phy_id);
        ret = -ENOMEM;
        goto out;
    }
    req_handle_tmp->priv_data = (void *)async_rsp;

    ret = ra_hdc_send_msg_async(RA_RS_SOCKET_LISTEN_START, phy_id, (char *)&async_data,
        sizeof(union op_socket_listen_data), req_handle_tmp);
    if (ret != 0) {
        hccp_err("[listen_start][ra_hdc_socket]hdc async send message process failed ret(%d) phy_id(%u)", ret, phy_id);
        free(req_handle_tmp);
        req_handle_tmp = NULL;
        goto out;
    }
    *req_handle = (void *)req_handle_tmp;
    return 0;

out:
    free(async_rsp);
    async_rsp = NULL;
    return ret;
}

void ra_hdc_async_handle_socket_listen_start(struct ra_request_handle *req_handle)
{
    struct ra_response_socket_listen *async_rsp = NULL;
    union op_socket_listen_data *async_data = NULL;
    unsigned int phy_id = req_handle->phy_id;
    int ret = 0;

    async_data = (union op_socket_listen_data *)req_handle->recv_buf;
    async_rsp = (struct ra_response_socket_listen *)req_handle->priv_data;
    ret = ra_get_socket_listen_result(async_data->rx_data.conn, async_rsp->num, async_rsp->conn, MAX_SOCKET_NUM);
    if (ret != 0) {
        hccp_err("[listen_start][ra_hdc_socket]ra_get_socket_listen_result failed, ret(%d) phy_id(%u)", ret, phy_id);
        req_handle->op_ret = -EINVAL;
        goto out;
    }
    return;

out:
    free(req_handle->priv_data);
    req_handle->priv_data = NULL;
    return;
}

int ra_hdc_socket_listen_stop_async(unsigned int phy_id, struct socket_listen_info_t conn[], unsigned int num,
    void **req_handle)
{
    struct ra_request_handle *req_handle_tmp = NULL;
    union op_socket_listen_data async_data = {0};
    int ret = 0;

    ret = ra_get_socket_listen_info(conn, num, async_data.tx_data.conn, MAX_SOCKET_NUM);
    CHK_PRT_RETURN(ret != 0, hccp_err("[listen_stop][ra_hdc_socket]get_socket_listen_info failed, ret(%d) phy_id(%u)",
        ret, phy_id), -EINVAL);
    async_data.tx_data.num = num | (1U << SOCKET_USE_PORT_BIT);

    req_handle_tmp = (struct ra_request_handle *)calloc(1, sizeof(struct ra_request_handle));
    CHK_PRT_RETURN(req_handle_tmp == NULL,
        hccp_err("[listen_stop][ra_hdc_socket]calloc ra_request_handle failed, phy_id[%u]", phy_id), -ENOMEM);

    ret = ra_hdc_send_msg_async(RA_RS_SOCKET_LISTEN_STOP, phy_id, (char *)&async_data,
        sizeof(union op_socket_listen_data), req_handle_tmp);
    if (ret != 0) {
        hccp_err("[listen_stop][ra_hdc_socket]hdc async send message process failed ret(%d) phy_id(%u)", ret, phy_id);
        free(req_handle_tmp);
        req_handle_tmp = NULL;
        return ret;
    }

    *req_handle = (void *)req_handle_tmp;
    return 0;
}

int ra_hdc_socket_batch_connect_async(unsigned int phy_id, struct socket_connect_info_t conn[], unsigned int num,
    void **req_handle)
{
    struct ra_request_handle *req_handle_tmp = NULL;
    union op_socket_connect_data *async_data = NULL;
    int ret = 0;

    async_data = (union op_socket_connect_data *)calloc(sizeof(union op_socket_connect_data), sizeof(char));
    CHK_PRT_RETURN(async_data == NULL, hccp_err("[batch_connect][ra_hdc_socket]calloc async_data failed, phy_id(%u)",
        phy_id), -ENOMEM);

    async_data->tx_data.num = num | (1U << SOCKET_USE_PORT_BIT);
    ret = ra_get_socket_connect_info(conn, num, async_data->tx_data.conn, MAX_SOCKET_NUM);
    if (ret != 0) {
        hccp_err("[batch_connect][ra_hdc_socket]ra_get_socket_connect_info failed, ret(%d) phy_id(%u)", ret, phy_id);
        goto out;
    }

    req_handle_tmp = (struct ra_request_handle *)calloc(1, sizeof(struct ra_request_handle));
    if (req_handle_tmp == NULL) {
        hccp_err("[batch_connect][ra_hdc_socket]calloc ra_request_handle failed, phy_id[%u]", phy_id);
        ret = -ENOMEM;
        goto out;
    }

    ret = ra_hdc_send_msg_async(RA_RS_SOCKET_CONN, phy_id, (char *)async_data, sizeof(union op_socket_connect_data),
        req_handle_tmp);
    if (ret != 0) {
        hccp_err("[batch_connect][ra_hdc_socket]hdc async send message process failed ret(%d) phy_id(%u)", ret, phy_id);
        free(req_handle_tmp);
        req_handle_tmp = NULL;
        goto out;
    }

    *req_handle = (void *)req_handle_tmp;

out:
    free(async_data);
    async_data = NULL;
    return ret;
}

int ra_hdc_socket_batch_close_async(unsigned int phy_id, struct socket_close_info_t conn[], unsigned int num,
    void **req_handle)
{
    struct ra_response_socket_batch_close *async_rsp = NULL;
    struct ra_request_handle *req_handle_tmp = NULL;
    union op_socket_close_data async_data = {0};
    unsigned int i;
    int ret = 0;

    for (i = 0; i < num; i++) {
        if (conn[i].fd_handle == NULL) {
            hccp_err("[batch_close][ra_hdc_socket]i(%u), conn fd_handle is NULL", i);
            ret = -EINVAL;
            goto out;
        }
        async_data.tx_data.conn[i].phy_id = phy_id;
        async_data.tx_data.conn[i].close_fd = ((struct socket_hdc_info *)conn[i].fd_handle)->fd;
    }
    // use attr disuse_linger of the fist conn as the common attr for all(0 by default)
    async_data.tx_data.num = (conn[0].disuse_linger != 0) ? (num | (1U << SOCKET_DISUSE_LINGER_BIT)) : num;

    async_rsp = (struct ra_response_socket_batch_close *)calloc(1, sizeof(struct ra_response_socket_batch_close));
    CHK_PRT_RETURN(async_rsp == NULL, hccp_err("[batch_close][ra_hdc_socket]calloc async_rsp failed, phy_id(%u)",
        phy_id), -ENOMEM);
    async_rsp->conn = conn;
    async_rsp->num = num;

    req_handle_tmp = (struct ra_request_handle *)calloc(1, sizeof(struct ra_request_handle));
    if (req_handle_tmp == NULL) {
        hccp_err("[batch_close][ra_hdc_socket]calloc ra_request_handle failed, phy_id[%u]", phy_id);
        ret = -ENOMEM;
        goto out;
    }

    req_handle_tmp->priv_data = (void *)async_rsp;

    ret = ra_hdc_send_msg_async(RA_RS_SOCKET_CLOSE, phy_id, (char *)&async_data,
        sizeof(union op_socket_close_data), req_handle_tmp);
    if (ret != 0) {
        hccp_err("[batch_close][ra_hdc_socket]hdc async send message process failed, ret(%d) phy_id(%u)",
            ret, phy_id);
        free(req_handle_tmp);
        req_handle_tmp = NULL;
        goto out;
    }

    *req_handle = (void *)req_handle_tmp;
    return 0;

out:
    free(async_rsp);
    async_rsp = NULL;
    return ret;
}

void ra_hdc_async_handle_socket_batch_close(struct ra_request_handle *req_handle)
{
    struct ra_response_socket_batch_close *async_rsp = NULL;
    unsigned int i;

    // should free fd_handle when op_ret is not EAGAIN, otherwise caller will retry
    if (req_handle->op_ret == -EAGAIN) {
        return;
    }

    async_rsp = (struct ra_response_socket_batch_close *)req_handle->priv_data;
    for (i = 0; i < async_rsp->num; i++) {
        if (async_rsp->conn[i].fd_handle != NULL) {
            free(async_rsp->conn[i].fd_handle);
            async_rsp->conn[i].fd_handle = NULL;
        }
    }
    return;
}
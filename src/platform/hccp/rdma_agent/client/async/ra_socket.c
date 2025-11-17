/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "user_log.h"
#include "hccp_common.h"
#include "hccp_async.h"
#include "ra.h"
#include "ra_hdc.h"
#include "ra_hdc_async_socket.h"
#include "ra_client_host.h"

HCCP_ATTRI_VISI_DEF int ra_socket_batch_connect_async(struct socket_connect_info_t conn[], unsigned int num,
    void **req_handle)
{
    struct ra_socket_handle *socket_handle = NULL;
    char remote_ip[MAX_IP_LEN] = {0};
    char local_ip[MAX_IP_LEN] = {0};
    unsigned int phy_id;
    unsigned int i = 0;
    int ret = 0;

    CHK_PRT_RETURN(conn == NULL || req_handle == NULL, hccp_err("[batch_connect][ra_socket]conn or "
        "req_handle is NULL"), conver_return_code(SOCKET_OP, -EINVAL));

    CHK_PRT_RETURN(num == 0 || num > MAX_SOCKET_NUM, hccp_err("[batch_connect][ra_socket]num[%u] invalid, "
        "must in range of (0, %u]", num, MAX_SOCKET_NUM), conver_return_code(SOCKET_OP, -EINVAL));

    for (i = 0; i < num; i++) {
        socket_handle = (struct ra_socket_handle *)(conn[i].socket_handle);
        CHK_PRT_RETURN(socket_handle == NULL, hccp_err("[batch_connect][ra_socket]socket_handle is NULL"),
            conver_return_code(SOCKET_OP, -EINVAL));

        phy_id = socket_handle->rdev_info.phy_id;
        CHK_PRT_RETURN(phy_id >= RA_MAX_PHY_ID_NUM, hccp_err("[batch_connect][ra_socket]phy_id[%u]invalid, "
            "must in range of [0, %u)", phy_id, RA_MAX_PHY_ID_NUM), conver_return_code(SOCKET_OP, -EINVAL));

        CHK_PRT_RETURN(strlen(conn[i].tag) >= SOCK_CONN_TAG_SIZE,
            hccp_err("[batch_connect][ra_socket]conn tag len(%d) more than max len(%u)",
            strlen(conn[i].tag), SOCK_CONN_TAG_SIZE), conver_return_code(SOCKET_OP, -EINVAL));

        ret = ra_inet_pton(socket_handle->rdev_info.family, socket_handle->rdev_info.local_ip, local_ip, MAX_IP_LEN);
        CHK_PRT_RETURN(ret != 0, hccp_err("[batch_connect][ra_socket]ra_inet_pton for local_ip failed, ret(%d)", ret),
            conver_return_code(SOCKET_OP, ret));

        ret = ra_inet_pton(socket_handle->rdev_info.family, conn[i].remote_ip, remote_ip, MAX_IP_LEN);
        CHK_PRT_RETURN(ret != 0, hccp_err("[batch_connect][ra_socket]ra_inet_pton for remote_ip failed, ret(%d)", ret),
            conver_return_code(SOCKET_OP, ret));

        hccp_run_info("Input parameters: [%u]th, phy_id[%u], local_ip[%s], remote_ip[%s], port[%u], tag[%s], cnt[%u]",
            i, socket_handle->rdev_info.phy_id, local_ip, remote_ip, conn[i].port, conn[i].tag,
            socket_handle->connect_cnt);
    }

    socket_handle->connect_cnt++;
    ret = ra_hdc_socket_batch_connect_async(phy_id, conn, num, req_handle);
    return conver_return_code(SOCKET_OP, ret);
}

HCCP_ATTRI_VISI_DEF int ra_socket_listen_start_async(struct socket_listen_info_t conn[], unsigned int num,
    void **req_handle)
{
    struct ra_socket_handle *socket_handle = NULL;
    char local_ip[MAX_IP_LEN] = {0};
    unsigned int phy_id;
    unsigned int i = 0;
    int ret = 0;

    CHK_PRT_RETURN(conn == NULL || req_handle == NULL, hccp_err("[listen_start][ra_socket]conn or req_handle is NULL"),
        conver_return_code(SOCKET_OP, -EINVAL));

    CHK_PRT_RETURN(num == 0 || num > MAX_SOCKET_NUM, hccp_err("[listen_start][ra_socket]num[%u] invalid, "
        "must in range of (0, %u]", num, MAX_SOCKET_NUM),
        conver_return_code(SOCKET_OP, -EINVAL));

    for (i = 0; i < num; i++) {
        socket_handle = (struct ra_socket_handle *)(conn[i].socket_handle);
        CHK_PRT_RETURN(socket_handle == NULL, hccp_err("[listen_start][ra_socket]socket_handle is NULL"),
            conver_return_code(SOCKET_OP, -EINVAL));

        phy_id = socket_handle->rdev_info.phy_id;
        CHK_PRT_RETURN(phy_id >= RA_MAX_PHY_ID_NUM, hccp_err("[listen_start][ra_socket]phy_id[%u]invalid, "
            "must in range of [0, %u)", phy_id, RA_MAX_PHY_ID_NUM), conver_return_code(SOCKET_OP, -EINVAL));

        ret = ra_inet_pton(socket_handle->rdev_info.family, socket_handle->rdev_info.local_ip, local_ip, MAX_IP_LEN);
        CHK_PRT_RETURN(ret != 0, hccp_err("[listen_start][ra_socket]ra_inet_pton for server_ip failed, ret(%d)", ret),
            conver_return_code(SOCKET_OP, ret));

        hccp_run_info("Input parameters: [%u]th, phy_id[%u], local_ip[%s], port[%u]", i, phy_id, local_ip,
            conn[i].port);
    }

    ret = ra_hdc_socket_listen_start_async(phy_id, conn, num, req_handle);
    return conver_return_code(SOCKET_OP, ret);
}

HCCP_ATTRI_VISI_DEF int ra_socket_listen_stop_async(struct socket_listen_info_t conn[], unsigned int num,
    void **req_handle)
{
    struct ra_socket_handle *socket_handle = NULL;
    char local_ip[MAX_IP_LEN] = {0};
    unsigned int phy_id;
    unsigned int i = 0;
    int ret = 0;

    CHK_PRT_RETURN(conn == NULL || req_handle == NULL, hccp_err("[listen_stop][ra_socket]conn or req_handle is NULL"),
        conver_return_code(SOCKET_OP, -EINVAL));

    CHK_PRT_RETURN(num == 0 || num > MAX_SOCKET_NUM, hccp_err("[listen_stop][ra_socket]num[%u] invalid, "
        "must in range of (0, %u]", num, MAX_SOCKET_NUM), conver_return_code(SOCKET_OP, -EINVAL));

    for (i = 0; i < num; i++) {
        socket_handle = (struct ra_socket_handle *)(conn[i].socket_handle);
        CHK_PRT_RETURN(socket_handle == NULL, hccp_err("[listen_stop][ra_socket]socket_handle is NULL"),
            conver_return_code(SOCKET_OP, -EINVAL));

        phy_id = socket_handle->rdev_info.phy_id;
        CHK_PRT_RETURN(phy_id >= RA_MAX_PHY_ID_NUM, hccp_err("[listen_stop][ra_socket]phy_id[%u]invalid, "
            "must in range of [0, %u)", phy_id, RA_MAX_PHY_ID_NUM), conver_return_code(SOCKET_OP, -EINVAL));

        ret = ra_inet_pton(socket_handle->rdev_info.family, socket_handle->rdev_info.local_ip, local_ip, MAX_IP_LEN);
        CHK_PRT_RETURN(ret != 0, hccp_err("[listen_stop][ra_socket]ra_inet_pton for server_ip failed, ret(%d)", ret),
            conver_return_code(SOCKET_OP, ret));

        hccp_run_info("Input parameters: [%u]th, phy_id[%u], local_ip[%s]", i, phy_id, local_ip);
    }

    ret = ra_hdc_socket_listen_stop_async(phy_id, conn, num, req_handle);
    return conver_return_code(SOCKET_OP, ret);
}

HCCP_ATTRI_VISI_DEF int ra_socket_batch_close_async(struct socket_close_info_t conn[], unsigned int num,
    void **req_handle)
{
    struct ra_socket_handle *socket_handle = NULL;
    char local_ip[MAX_IP_LEN] = {0};
    unsigned int phy_id;
    unsigned int i = 0;
    int ret = 0;

    CHK_PRT_RETURN(conn == NULL || req_handle == NULL, hccp_err("[batch_close][ra_socket]conn or req_handle is NULL"),
        conver_return_code(SOCKET_OP, -EINVAL));

    CHK_PRT_RETURN(num == 0 || num > MAX_SOCKET_NUM, hccp_err("[batch_close][ra_socket]num[%u] invalid, "
        "must in range of (0, %u]", num, MAX_SOCKET_NUM), conver_return_code(SOCKET_OP, -EINVAL));

    for (i = 0; i < num; i++) {
        socket_handle = (struct ra_socket_handle *)(conn[i].socket_handle);
        CHK_PRT_RETURN(socket_handle == NULL, hccp_err("[batch_close][ra_socket]socket_handle is NULL"),
            conver_return_code(SOCKET_OP, -EINVAL));

        phy_id = socket_handle->rdev_info.phy_id;
        CHK_PRT_RETURN(phy_id >= RA_MAX_PHY_ID_NUM, hccp_err("[batch_close][ra_socket]phy_id[%u]invalid, "
            "must in range of [0, %u)", phy_id, RA_MAX_PHY_ID_NUM), conver_return_code(SOCKET_OP, -EINVAL));

        ret = ra_inet_pton(socket_handle->rdev_info.family, socket_handle->rdev_info.local_ip, local_ip, MAX_IP_LEN);
        CHK_PRT_RETURN(ret != 0, hccp_err("[batch_close][ra_socket]ra_inet_pton for local_ip failed, ret(%d)", ret),
            conver_return_code(SOCKET_OP, ret));

        hccp_run_info("Input parameters: [%u]th, phy_id[%u], local_ip[%s], cnt[%u]", i, phy_id, local_ip,
            socket_handle->close_cnt);
    }

    socket_handle->close_cnt++;
    ret = ra_hdc_socket_batch_close_async(phy_id, conn, num, req_handle);
    return conver_return_code(SOCKET_OP, ret);
}

HCCP_ATTRI_VISI_DEF int ra_socket_send_async(const void *fd_handle, const void *data, unsigned long long size,
    unsigned long long *sent_size, void **req_handle)
{
    int ret = 0;

    CHK_PRT_RETURN(fd_handle == NULL || data == NULL || sent_size == NULL || size == 0 || req_handle == NULL,
        hccp_err("[send][ra_socket]fd_handle or data or sent_size or req_handle is NULL or size[%llu] is 0", size),
        conver_return_code(SOCKET_OP, -EINVAL));

    ret = ra_hdc_socket_send_async((const struct socket_hdc_info *)fd_handle, data, size, sent_size, req_handle);
    return conver_return_code(SOCKET_OP, ret);
}

HCCP_ATTRI_VISI_DEF int ra_socket_recv_async(const void *fd_handle, void *data, unsigned long long size,
    unsigned long long *received_size, void **req_handle)
{
    int ret = 0;

    CHK_PRT_RETURN(fd_handle == NULL || data == NULL || received_size == NULL || size == 0 || req_handle == NULL,
        hccp_err("[recv][ra_socket]fd_handle or data or received_size or req_handle is NULL or size[%llu] is 0", size),
        conver_return_code(SOCKET_OP, -EINVAL));

    ret = ra_hdc_socket_recv_async((const struct socket_hdc_info *)fd_handle, data, size, received_size, req_handle);
    return conver_return_code(SOCKET_OP, ret);
}

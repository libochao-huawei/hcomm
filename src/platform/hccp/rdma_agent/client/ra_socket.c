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
#include "ra.h"
#include "ra_rs_comm.h"
#include "ra_client_host.h"
#include "hccp.h"

HCCP_ATTRI_VISI_DEF int ra_get_client_socket_err_info(struct socket_connect_info_t conn[],
    struct socket_err_info err[], unsigned int num)
{
    struct ra_socket_handle *socket_handle = NULL;
    char remote_ip[MAX_IP_LEN] = {0};
    char local_ip[MAX_IP_LEN] = {0};
    unsigned int phy_id;
    unsigned int i;
    int ret;

    CHK_PRT_RETURN(conn == NULL || err == NULL || num == 0 || num > MAX_SOCKET_NUM,
        hccp_err("[get][ra_socket]conn is NULL or err is NULL or num[%u] is zero or num is greater than %d",
        num, MAX_SOCKET_NUM), conver_return_code(SOCKET_OP, -EINVAL));

    for (i = 0; i < num; i++) {
        socket_handle = (struct ra_socket_handle *)conn[i].socket_handle;
        CHK_PRT_RETURN(socket_handle == NULL || socket_handle->socket_ops == NULL ||
            socket_handle->socket_ops->ra_get_client_socket_err_info == NULL,
            hccp_err("[get][ra_socket]socket_handle or func is NULL"), conver_return_code(SOCKET_OP, -EINVAL));

        phy_id = socket_handle->rdev_info.phy_id;

        ret = ra_inet_pton(socket_handle->rdev_info.family, socket_handle->rdev_info.local_ip, local_ip, MAX_IP_LEN);
        CHK_PRT_RETURN(ret != 0, hccp_err("[get][ra_socket]ra_inet_pton for local_ip failed, ret(%d)", ret),
            conver_return_code(SOCKET_OP, ret));

        ret = ra_inet_pton(socket_handle->rdev_info.family, conn[i].remote_ip, remote_ip, MAX_IP_LEN);
        CHK_PRT_RETURN(ret != 0, hccp_err("[get][ra_socket]ra_inet_pton for remote_ip failed, ret(%d)", ret),
            conver_return_code(SOCKET_OP, ret));

        hccp_info("Input parameters: [%u]th, phy_id[%u], local_ip[%s], remote_ip[%s], port[%u], tag[%s]",
            i, phy_id, local_ip, remote_ip, conn[i].port, conn[i].tag);
    }

    ret = socket_handle->socket_ops->ra_get_client_socket_err_info(phy_id, conn, err, num);
    return conver_return_code(SOCKET_OP, ret);
}

HCCP_ATTRI_VISI_DEF int ra_get_server_socket_err_info(struct socket_listen_info_t conn[],
    struct server_socket_err_info err[], unsigned int num)
{
    struct ra_socket_handle *socket_handle = NULL;
    char local_ip[MAX_IP_LEN] = {0};
    unsigned int phy_id;
    unsigned int i;
    int ret;

    CHK_PRT_RETURN(conn == NULL || err == NULL || num == 0 || num > MAX_SOCKET_NUM,
        hccp_err("[get][ra_socket]conn is NULL or err is NULL or num[%u] is zero or num is greater than %d",
        num, MAX_SOCKET_NUM), conver_return_code(SOCKET_OP, -EINVAL));

    for (i = 0; i < num; i++) {
        socket_handle = (struct ra_socket_handle *)conn[i].socket_handle;
        CHK_PRT_RETURN(socket_handle == NULL || socket_handle->socket_ops == NULL ||
            socket_handle->socket_ops->ra_get_server_socket_err_info == NULL,
            hccp_err("[get][ra_socket]socket_handle or func is NULL"), conver_return_code(SOCKET_OP, -EINVAL));

        phy_id = socket_handle->rdev_info.phy_id;

        ret = ra_inet_pton(socket_handle->rdev_info.family, socket_handle->rdev_info.local_ip, local_ip, MAX_IP_LEN);
        CHK_PRT_RETURN(ret, hccp_err("[get][ra_socket]ra_inet_pton for server_ip failed, ret(%d)", ret),
            conver_return_code(SOCKET_OP, ret));

        hccp_info("Input parameters: [%u]th, phy_id[%u], local_ip[%s], port[%u]",
            i, phy_id, local_ip, conn[i].port);
    }

    ret = socket_handle->socket_ops->ra_get_server_socket_err_info(phy_id, conn, err, num);
    return conver_return_code(SOCKET_OP, ret);
}

HCCP_ATTRI_VISI_DEF int ra_socket_accept_credit_add(struct socket_listen_info_t conn[], unsigned int num,
    unsigned int credit_limit)
{
    struct ra_socket_handle *socket_handle = NULL;
    char local_ip[MAX_IP_LEN] = {0};
    unsigned int phy_id;
    unsigned int i;
    int ret;

    CHK_PRT_RETURN(conn == NULL || num == 0 || num > MAX_SOCKET_NUM || credit_limit == 0,
        hccp_err("[set][ra_socket]conn is NULL or num[%u] is 0 or greater than %d, or credit_limit[%u] is 0", num,
        MAX_SOCKET_NUM, credit_limit), conver_return_code(SOCKET_OP, -EINVAL));

    for (i = 0; i < num; i++) {
        socket_handle = (struct ra_socket_handle *)conn[i].socket_handle;
        CHK_PRT_RETURN(socket_handle == NULL || socket_handle->socket_ops == NULL ||
            socket_handle->socket_ops->ra_socket_accept_credit_add == NULL,
            hccp_err("[set][ra_socket]socket_handle or func is NULL"), conver_return_code(SOCKET_OP, -EINVAL));

        phy_id = socket_handle->rdev_info.phy_id;
        CHK_PRT_RETURN(phy_id >= RA_MAX_PHY_ID_NUM,
            hccp_err("[set][ra_socket]phy_id(%u) must smaller than %u", phy_id, RA_MAX_PHY_ID_NUM),
            conver_return_code(SOCKET_OP, -EINVAL));

        ret = ra_inet_pton(socket_handle->rdev_info.family, socket_handle->rdev_info.local_ip, local_ip, MAX_IP_LEN);
        CHK_PRT_RETURN(ret, hccp_err("[set][ra_socket]ra_inet_pton for server_ip failed, ret(%d)", ret),
            conver_return_code(SOCKET_OP, ret));

        hccp_run_info("Input parameters: [%u]th, phy_id[%u], local_ip[%s] port[%u] credit_limit[%u]",
            i, phy_id, local_ip, conn[i].port, credit_limit);
    }

    ret = socket_handle->socket_ops->ra_socket_accept_credit_add(phy_id, conn, num, credit_limit);
    return conver_return_code(SOCKET_OP, ret);
}

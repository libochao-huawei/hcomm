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

HCCP_ATTRI_VISI_DEF int RaGetClientSocketErrInfo(struct socket_connect_info_t conn[],
    struct socket_err_info err[], unsigned int num)
{
    struct ra_socket_handle *socketHandle = NULL;
    char remoteIp[MAX_IP_LEN] = {0};
    char localIp[MAX_IP_LEN] = {0};
    unsigned int phyId;
    unsigned int i;
    int ret;

    CHK_PRT_RETURN(conn == NULL || err == NULL || num == 0 || num > MAX_SOCKET_NUM,
        hccp_err("[get][ra_socket]conn is NULL or err is NULL or num[%u] is zero or num is greater than %d",
        num, MAX_SOCKET_NUM), ConverReturnCode(SOCKET_OP, -EINVAL));

    for (i = 0; i < num; i++) {
        socketHandle = (struct ra_socket_handle *)conn[i].socket_handle;
        CHK_PRT_RETURN(socketHandle == NULL || socketHandle->socket_ops == NULL ||
            socketHandle->socket_ops->ra_get_client_socket_err_info == NULL,
            hccp_err("[get][ra_socket]socket_handle or func is NULL"), ConverReturnCode(SOCKET_OP, -EINVAL));

        phyId = socketHandle->rdev_info.phy_id;

        ret = RaInetPton(socketHandle->rdev_info.family, socketHandle->rdev_info.local_ip, localIp, MAX_IP_LEN);
        CHK_PRT_RETURN(ret != 0, hccp_err("[get][ra_socket]ra_inet_pton for local_ip failed, ret(%d)", ret),
            ConverReturnCode(SOCKET_OP, ret));

        ret = RaInetPton(socketHandle->rdev_info.family, conn[i].remote_ip, remoteIp, MAX_IP_LEN);
        CHK_PRT_RETURN(ret != 0, hccp_err("[get][ra_socket]ra_inet_pton for remote_ip failed, ret(%d)", ret),
            ConverReturnCode(SOCKET_OP, ret));

        hccp_info("Input parameters: [%u]th, phyId[%u], localIp[%s], remoteIp[%s], port[%u], tag[%s]",
            i, phyId, localIp, remoteIp, conn[i].port, conn[i].tag);
    }

    ret = socketHandle->socket_ops->ra_get_client_socket_err_info(phyId, conn, err, num);
    return ConverReturnCode(SOCKET_OP, ret);
}

HCCP_ATTRI_VISI_DEF int RaGetServerSocketErrInfo(struct socket_listen_info_t conn[],
    struct server_socket_err_info err[], unsigned int num)
{
    struct ra_socket_handle *socketHandle = NULL;
    char localIp[MAX_IP_LEN] = {0};
    unsigned int phyId;
    unsigned int i;
    int ret;

    CHK_PRT_RETURN(conn == NULL || err == NULL || num == 0 || num > MAX_SOCKET_NUM,
        hccp_err("[get][ra_socket]conn is NULL or err is NULL or num[%u] is zero or num is greater than %d",
        num, MAX_SOCKET_NUM), ConverReturnCode(SOCKET_OP, -EINVAL));

    for (i = 0; i < num; i++) {
        socketHandle = (struct ra_socket_handle *)conn[i].socket_handle;
        CHK_PRT_RETURN(socketHandle == NULL || socketHandle->socket_ops == NULL ||
            socketHandle->socket_ops->ra_get_server_socket_err_info == NULL,
            hccp_err("[get][ra_socket]socket_handle or func is NULL"), ConverReturnCode(SOCKET_OP, -EINVAL));

        phyId = socketHandle->rdev_info.phy_id;

        ret = RaInetPton(socketHandle->rdev_info.family, socketHandle->rdev_info.local_ip, localIp, MAX_IP_LEN);
        CHK_PRT_RETURN(ret, hccp_err("[get][ra_socket]ra_inet_pton for server_ip failed, ret(%d)", ret),
            ConverReturnCode(SOCKET_OP, ret));

        hccp_info("Input parameters: [%u]th, phyId[%u], localIp[%s], port[%u]",
            i, phyId, localIp, conn[i].port);
    }

    ret = socketHandle->socket_ops->ra_get_server_socket_err_info(phyId, conn, err, num);
    return ConverReturnCode(SOCKET_OP, ret);
}

HCCP_ATTRI_VISI_DEF int RaSocketAcceptCreditAdd(struct socket_listen_info_t conn[], unsigned int num,
    unsigned int creditLimit)
{
    struct ra_socket_handle *socketHandle = NULL;
    char localIp[MAX_IP_LEN] = {0};
    unsigned int phyId;
    unsigned int i;
    int ret;

    CHK_PRT_RETURN(conn == NULL || num == 0 || num > MAX_SOCKET_NUM || creditLimit == 0,
        hccp_err("[set][ra_socket]conn is NULL or num[%u] is 0 or greater than %d, or creditLimit[%u] is 0", num,
        MAX_SOCKET_NUM, creditLimit), ConverReturnCode(SOCKET_OP, -EINVAL));

    for (i = 0; i < num; i++) {
        socketHandle = (struct ra_socket_handle *)conn[i].socket_handle;
        CHK_PRT_RETURN(socketHandle == NULL || socketHandle->socket_ops == NULL ||
            socketHandle->socket_ops->ra_socket_accept_credit_add == NULL,
            hccp_err("[set][ra_socket]socket_handle or func is NULL"), ConverReturnCode(SOCKET_OP, -EINVAL));

        phyId = socketHandle->rdev_info.phy_id;
        CHK_PRT_RETURN(phyId >= RA_MAX_PHY_ID_NUM,
            hccp_err("[set][ra_socket]phy_id(%u) must smaller than %u", phyId, RA_MAX_PHY_ID_NUM),
            ConverReturnCode(SOCKET_OP, -EINVAL));

        ret = RaInetPton(socketHandle->rdev_info.family, socketHandle->rdev_info.local_ip, localIp, MAX_IP_LEN);
        CHK_PRT_RETURN(ret, hccp_err("[set][ra_socket]ra_inet_pton for server_ip failed, ret(%d)", ret),
            ConverReturnCode(SOCKET_OP, ret));

        hccp_run_info("Input parameters: [%u]th, phyId[%u], localIp[%s] port[%u] creditLimit[%u]",
            i, phyId, localIp, conn[i].port, creditLimit);
    }

    ret = socketHandle->socket_ops->ra_socket_accept_credit_add(phyId, conn, num, creditLimit);
    return ConverReturnCode(SOCKET_OP, ret);
}

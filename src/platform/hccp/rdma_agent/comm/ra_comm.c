/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "ra_comm.h"
#include <errno.h>
#include "securec.h"
#include "ra_rs_err.h"

int ra_get_socket_connect_info(const struct socket_connect_info_t conn[], unsigned int num,
    struct socket_connect_info rs_conn[], unsigned int rs_num)
{
    struct ra_socket_handle *socket_handle = NULL;
    unsigned int i;
    int ret;

    CHK_PRT_RETURN(num > rs_num || num > MAX_SOCKET_NUM || conn == NULL || rs_conn == NULL, hccp_err("[get]"
        "[ra_socket_connect_info]num(%u) > rs_num(%u) or conn or rs_conn is NULL, invalid", num, rs_num), -EINVAL);
    for (i = 0; i < num; i++) {
        socket_handle = (struct ra_socket_handle *)conn[i].socket_handle;
        rs_conn[i].phy_id = socket_handle->rdev_info.phy_id;
        rs_conn[i].family = socket_handle->rdev_info.family;
        rs_conn[i].port = conn[i].port;
        ret = memcpy_s(&(rs_conn[i].local_ip), sizeof(union hccp_ip_addr),
            &(socket_handle->rdev_info.local_ip), sizeof(union hccp_ip_addr));
        CHK_PRT_RETURN(ret, hccp_err("[get][ra_socket_connect_info]memcpy_s for local_ip failed, ret(%d)",
            ret), -ESAFEFUNC);
        ret = memcpy_s(&(rs_conn[i].remote_ip), sizeof(union hccp_ip_addr),
            &(conn[i].remote_ip), sizeof(union hccp_ip_addr));
        CHK_PRT_RETURN(ret, hccp_err("[get][ra_socket_connect_info]memcpy_s for remote_ip failed, ret(%d)",
            ret), -ESAFEFUNC);
        ret = memcpy_s(rs_conn[i].tag, SOCK_CONN_TAG_SIZE, conn[i].tag, SOCK_CONN_TAG_SIZE);
        CHK_PRT_RETURN(ret, hccp_err("[get][ra_socket_connect_info]memcpy_s for tag failed, ret(%d)",
            ret), -ESAFEFUNC);
    }
    return 0;
}

int ra_get_socket_listen_info(const struct socket_listen_info_t conn[], unsigned int num,
    struct socket_listen_info rs_conn[], unsigned int rs_num)
{
    unsigned int i;
    int ret;
    struct ra_socket_handle *socket_handle = NULL;

    CHK_PRT_RETURN(num > rs_num || num > MAX_SOCKET_NUM || conn == NULL || rs_conn == NULL, hccp_err("[get]"
        "[ra_socket_listen_info]num(%u) > rs_num(%u), or conn or rs_conn is NULL, invalid", num, rs_num), -EINVAL);

    for (i = 0; i < num; i++) {
        rs_conn[i].phase = conn[i].phase;
        rs_conn[i].err = conn[i].err;
        socket_handle = (struct ra_socket_handle *)conn[i].socket_handle;
        rs_conn[i].phy_id = socket_handle->rdev_info.phy_id;
        rs_conn[i].family = socket_handle->rdev_info.family;
        rs_conn[i].port = conn[i].port;
        ret = memcpy_s(&(rs_conn[i].local_ip), sizeof(union hccp_ip_addr),
            &(socket_handle->rdev_info.local_ip), sizeof(union hccp_ip_addr));
        CHK_PRT_RETURN(ret, hccp_err("[get][ra_socket_listen_info]memcpy_s for local_ip failed, ret(%d)",
            ret), -ESAFEFUNC);
    }
    return 0;
}

int ra_get_socket_listen_result(const struct socket_listen_info rs_conn[], unsigned int rs_num,
    struct socket_listen_info_t conn[], unsigned int num)
{
    struct ra_socket_handle *socket_handle = NULL;
    unsigned int i;
    int ret;

    CHK_PRT_RETURN(rs_num > num || rs_num > MAX_SOCKET_NUM || conn == NULL || rs_conn == NULL, hccp_err("[get]"
        "[ra_socket_listen_result]rs_num(%u) > num(%u) or conn or rs_conn is NULL, invalid", rs_num, num), -EINVAL);

    for (i = 0; i < rs_num; i++) {
        conn[i].phase = rs_conn[i].phase;
        conn[i].err = rs_conn[i].err;
        conn[i].port = rs_conn[i].port;
        socket_handle = (struct ra_socket_handle *)conn[i].socket_handle;
        socket_handle->rdev_info.phy_id = rs_conn[i].phy_id;
        socket_handle->rdev_info.family = rs_conn[i].family;
        ret = memcpy_s(&(socket_handle->rdev_info.local_ip), sizeof(union hccp_ip_addr),
            &(rs_conn[i].local_ip), sizeof(union hccp_ip_addr));
        CHK_PRT_RETURN(ret, hccp_err("[get][ra_socket_listen_result]memcpy_s for local_ip failed, ret(%d)",
            ret), -ESAFEFUNC);
    }
    return 0;
}

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
#include "ra_comm.h"
#include "rs.h"
#include "ra_peer.h"
#include "ra_peer_socket.h"

int ra_peer_get_client_socket_err_info(unsigned int phy_id, struct socket_connect_info_t conn[],
    struct socket_err_info err[], unsigned int num)
{
    struct socket_connect_info conn_out[MAX_SOCKET_NUM] = {0};
    int ret;

    ret = ra_get_socket_connect_info(conn, num, conn_out, MAX_SOCKET_NUM);
    CHK_PRT_RETURN(ret != 0, hccp_err("[get][ra_peer_socket]ra_get_socket_connect_info failed, "
        "ret(%d)", ret), ret);
    
    ra_peer_mutex_lock(phy_id);
    rs_set_ctx(phy_id);
    ret = rs_socket_get_client_socket_err_info(conn_out, err, num);
    if (ret != 0) {
        hccp_err("[get][ra_peer_socket]ra client get socket info failed, ret(%d)", ret);
    }
    ra_peer_mutex_unlock(phy_id);

    return ret;
}

int ra_peer_get_server_socket_err_info(unsigned int phy_id, struct socket_listen_info_t conn[],
        struct server_socket_err_info err[], unsigned int num)
{
    struct socket_listen_info conn_out[MAX_SOCKET_NUM] = {0};
    int ret;

    ret = ra_get_socket_listen_info(conn, num, conn_out, MAX_SOCKET_NUM);
    CHK_PRT_RETURN(ret, hccp_err("[get][ra_peer_socket]ra_get_socket_listen_info failed "
        "ret(%d)", ret), ret);

    ra_peer_mutex_lock(phy_id);
    rs_set_ctx(phy_id);
    ret = rs_socket_get_server_socket_err_info(conn_out, err, num);
    if (ret != 0) {
        hccp_err("[get][ra_peer_socket]ra server get socket info failed, ret(%d)", ret);
    }
    ra_peer_mutex_unlock(phy_id);

    return ret;
}

int ra_peer_socket_accept_credit_add(unsigned int phy_id, struct socket_listen_info_t conn[], unsigned int num,
    unsigned int credit_limit)
{
    struct socket_listen_info rs_conn[MAX_SOCKET_NUM] = {0};
    int ret;

    ret = ra_get_socket_listen_info(conn, num, rs_conn, MAX_SOCKET_NUM);
    CHK_PRT_RETURN(ret, hccp_err("[set][ra_peer_socket]ra_peer_get_socket_listen_info failed ret(%d) phy_id(%u)",
        ret, phy_id), ret);

    ra_peer_mutex_lock(phy_id);
    rs_set_ctx(phy_id);
    ret = rs_socket_accept_credit_add(rs_conn, num, credit_limit);
    if (ret == -ENODEV) {
        hccp_warn("[set][ra_peer_socket]rs_socket_accept_credit_add unsuccessful ret(%d) phy_id(%u)", ret, phy_id);
    } else if (ret != 0) {
        hccp_err("[set][ra_peer_socket]rs_socket_accept_credit_add failed ret(%d) phy_id(%u)", ret, phy_id);
    }
    ra_peer_mutex_unlock(phy_id);
    return ret;
}

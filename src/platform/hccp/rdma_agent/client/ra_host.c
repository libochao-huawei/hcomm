/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <sys/epoll.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include "securec.h"
#include "dl_hal_function.h"
#include "hccp.h"
#include "ra_client_host.h"
#include "ra.h"
#include "ra_init.h"
#include "ra_rdma.h"
#include "ra_hdc.h"
#include "ra_hdc_rdma_notify.h"
#include "ra_hdc_rdma.h"
#include "ra_hdc_socket.h"
#include "ra_peer.h"
#include "ra_peer_socket.h"
#include "ra_rs_comm.h"
#include "ra_rs_err.h"

static unsigned int g_white_list_switch = 0;

/* socket: nic on device, need use hdc channel, support: 910, 310 */
struct ra_socket_ops g_ra_hdc_socket_ops = {
    .ra_socket_init = ra_hdc_socket_init,
    .ra_socket_deinit = ra_hdc_socket_deinit,
    .ra_socket_batch_connect = ra_hdc_socket_batch_connect,
    .ra_socket_batch_close = ra_hdc_socket_batch_close,
    .ra_socket_batch_abort = ra_hdc_socket_batch_abort,
    .ra_socket_listen_start = ra_hdc_socket_listen_start,
    .ra_socket_listen_stop = ra_hdc_socket_listen_stop,
    .ra_get_sockets = ra_hdc_get_sockets,
    .ra_socket_send = ra_hdc_socket_send,
    .ra_socket_recv = ra_hdc_socket_recv,
    .ra_get_client_socket_err_info = NULL,
    .ra_get_server_socket_err_info = NULL,
    .ra_socket_set_white_list_status = NULL,
    .ra_socket_get_white_list_status = NULL,
    .ra_socket_white_list_add = ra_hdc_socket_white_list_add,
    .ra_socket_white_list_del = ra_hdc_socket_white_list_del,
    .ra_socket_accept_credit_add = ra_hdc_socket_accept_credit_add,
};

/* socket: nic on host/device, support: cx6, 1822 */
struct ra_socket_ops g_ra_peer_socket_ops = {
    .ra_socket_init = NULL,
    .ra_socket_deinit = ra_peer_socket_deinit,
    .ra_socket_batch_connect = ra_peer_socket_batch_connect,
    .ra_socket_batch_close = ra_peer_socket_batch_close,
    .ra_socket_batch_abort = ra_peer_socket_batch_abort,
    .ra_socket_listen_start = ra_peer_socket_listen_start,
    .ra_socket_listen_stop = ra_peer_socket_listen_stop,
    .ra_get_sockets = ra_peer_get_sockets,
    .ra_socket_send = ra_peer_socket_send,
    .ra_socket_recv = ra_peer_socket_recv,
    .ra_get_client_socket_err_info = ra_peer_get_client_socket_err_info,
    .ra_get_server_socket_err_info = ra_peer_get_server_socket_err_info,
    .ra_socket_set_white_list_status = NULL,
    .ra_socket_get_white_list_status = NULL,
    .ra_socket_white_list_add = ra_peer_socket_white_list_add,
    .ra_socket_white_list_del = ra_peer_socket_white_list_del,
    .ra_socket_accept_credit_add = ra_peer_socket_accept_credit_add,
};

/* rdma: nic on device, need use hdc channel, support:910 */
struct ra_rdma_ops g_ra_hdc_rdma_ops = {
    .ra_rdev_init = ra_hdc_rdev_init,
    .ra_rdev_get_port_status = ra_hdc_rdev_get_port_status,
    .ra_rdev_deinit = ra_hdc_rdev_deinit,
    .ra_set_tsqp_depth = ra_hdc_set_tsqp_depth,
    .ra_get_tsqp_depth = ra_hdc_get_tsqp_depth,
    .ra_qp_create = ra_hdc_qp_create,
    .ra_qp_create_with_attrs = ra_hdc_qp_create_with_attrs,
    .ra_ai_qp_create = ra_hdc_ai_qp_create,
    .ra_ai_qp_create_with_attrs = ra_hdc_ai_qp_create_with_attrs,
    .ra_typical_qp_create = ra_hdc_typical_qp_create,
    .ra_loopback_qp_create = NULL,
    .ra_qp_destroy = ra_hdc_qp_destroy,
    .ra_typical_qp_modify = ra_hdc_typical_qp_modify,
    .ra_qp_batch_modify = ra_hdc_qp_batch_modify,
    .ra_qp_connect_async = ra_hdc_qp_connect_async,
    .ra_get_qp_status = ra_hdc_get_qp_status,
    .ra_mr_reg = ra_hdc_mr_reg,
    .ra_mr_dereg = ra_hdc_mr_dereg,
    .ra_register_mr = ra_hdc_typical_mr_reg,
    .ra_remap_mr = ra_hdc_remap_mr,
    .ra_deregister_mr = ra_hdc_typical_mr_dereg,
    .ra_send_wr = ra_hdc_send_wr,
    .ra_send_wr_v2 = ra_hdc_send_wr_v2,
    .ra_typical_send_wr = ra_hdc_typical_send_wr,
    .ra_send_wrlist = ra_hdc_send_wrlist,
    .ra_send_wrlist_ext = ra_hdc_send_wrlist_ext,
    .ra_send_normal_wrlist = ra_hdc_send_normal_wrlist,
    .ra_get_notify_base_addr = ra_hdc_get_notify_base_addr,
    .ra_get_notify_mr_info = ra_hdc_get_notify_mr_info,
    .ra_recv_wrlist = ra_hdc_recv_wrlist,
    .ra_poll_cq = ra_hdc_poll_cq,
    .ra_get_qp_context = NULL,
    .ra_normal_qp_create = NULL,
    .ra_normal_qp_destroy = NULL,
    .ra_cq_create = NULL,
    .ra_cq_destroy = NULL,
    .ra_set_qp_attr_qos = ra_hdc_set_qp_attr_qos,
    .ra_set_qp_attr_timeout = ra_hdc_set_qp_attr_timeout,
    .ra_set_qp_attr_retry_cnt = ra_hdc_set_qp_attr_retry_cnt,
    .ra_create_comp_channel = NULL,
    .ra_destroy_comp_channel = NULL,
    .ra_create_srq = NULL,
    .ra_destroy_srq = NULL,
};

/* rdma: nic on host/device, support:cx6 1822 */
struct ra_rdma_ops g_ra_peer_rdma_ops = {
    .ra_rdev_init = ra_peer_rdev_init,
    .ra_rdev_get_port_status = NULL,
    .ra_rdev_deinit = ra_peer_rdev_deinit,
    .ra_set_tsqp_depth = ra_peer_set_tsqp_depth,
    .ra_get_tsqp_depth = ra_peer_get_tsqp_depth,
    .ra_qp_create = ra_peer_qp_create,
    .ra_qp_create_with_attrs = ra_peer_qp_create_with_attrs,
    .ra_ai_qp_create = NULL,
    .ra_ai_qp_create_with_attrs = NULL,
    .ra_typical_qp_create = NULL,
    .ra_loopback_qp_create = ra_peer_loopback_qp_create,
    .ra_qp_destroy = ra_peer_qp_destroy,
    .ra_typical_qp_modify = ra_peer_typical_qp_modify,
    .ra_qp_batch_modify = NULL,
    .ra_qp_connect_async = ra_peer_qp_connect_async,
    .ra_get_qp_status = ra_peer_get_qp_status,
    .ra_mr_reg = ra_peer_mr_reg,
    .ra_mr_dereg = ra_peer_mr_dereg,
    .ra_register_mr = ra_peer_register_mr,
    .ra_remap_mr = NULL,
    .ra_deregister_mr = ra_peer_deregister_mr,
    .ra_send_wr = ra_peer_send_wr,
    .ra_send_wr_v2 = NULL,
    .ra_typical_send_wr = NULL,
    .ra_send_wrlist = ra_peer_send_wrlist,
    .ra_send_wrlist_ext = NULL,
    .ra_send_normal_wrlist = NULL,
    .ra_get_notify_base_addr = ra_peer_get_notify_base_addr,
    .ra_get_notify_mr_info = NULL,
    .ra_recv_wrlist = ra_peer_recv_wrlist,
    .ra_poll_cq = NULL,
    .ra_get_qp_context = ra_peer_get_qp_context,
    .ra_normal_qp_create = ra_peer_normal_qp_create,
    .ra_normal_qp_destroy = ra_peer_normal_qp_destroy,
    .ra_cq_create = ra_peer_cq_create,
    .ra_cq_destroy = ra_peer_cq_destroy,
    .ra_set_qp_attr_qos = ra_peer_set_qp_attr_qos,
    .ra_set_qp_attr_timeout = ra_peer_set_qp_attr_timeout,
    .ra_set_qp_attr_retry_cnt = ra_peer_set_qp_attr_retry_cnt,
    .ra_create_comp_channel = ra_peer_create_comp_channel,
    .ra_destroy_comp_channel = ra_peer_destroy_comp_channel,
    .ra_create_srq = ra_peer_create_srq,
    .ra_destroy_srq = ra_peer_destroy_srq,
};

struct errcode_info g_errcode_info_list[] = {
    {-EPERM, 1, 0},
    {-EAGAIN, 1, 1},
    {-EACCES, 1, 2},
    {-EINVAL, 1, 3},
    {-ESYSFUNC, 1, 4},
    {-EADDRINUSE, 1, 5},
    {-EADDRNOTAVAIL, 1, 6},
    {-ESOCKCLOSED, 1, 7},
    {-ENOENT, 2, 0},
    {-ESRCH, 2, 1},
    {-ENODEV, 2, 2},
    {-ENOSPC, 2, 3},
    {-EPROTONOSUPPORT, 2, 4},
    {-EFILEOPER, 2, 5},
    {-ENOMEM, 3, 0},
    {-EFAULT, 3, 1},
    {-EEXIST, 3, 2},
    {-EPIPE, 3, 3},
    {-ENOLINK, 3, 4},
    {-ENETUNREACH, 3, 5},
    {-ESAFEFUNC, 3, 6},
    {-EDEFAULT, 3, 7},
    {-EINVALIDIP, 3, 8},
    {-EOPENSRC, 5, 1},
    {-ENOTSUPP, 5, 2},
};

int ra_inet_pton(int family, union hccp_ip_addr ip, char net_addr[], unsigned int len)
{
    const char *str = NULL;
    str = inet_ntop(family, &(ip.addr), net_addr, len);
    CHK_PRT_RETURN(str == NULL, hccp_err("[ntop_convert][ra_inet]the ip failed err(%d)", errno), -EINVAL);
    return 0;
}

HCCP_ATTRI_VISI_DEF int ra_socket_init(int mode, struct rdev rdev_info, void **socket_handle)
{
    struct ra_socket_handle *socket_handle_tmp = NULL;
    int ret;
    char local_ip[MAX_IP_LEN] = {0};

    CHK_PRT_RETURN(rdev_info.phy_id >= RA_MAX_PHY_ID_NUM || socket_handle == NULL,
        hccp_err("[init][ra_socket]phy_id(%u) is invalid! it must be [0,%d) or socket is null!",
                 rdev_info.phy_id, RA_MAX_PHY_ID_NUM), conver_return_code(HCCP_INIT, -EINVAL));

    ret = ra_inet_pton(rdev_info.family, rdev_info.local_ip, local_ip, MAX_IP_LEN);
    CHK_PRT_RETURN(ret, hccp_err("[init][ra_socket]ra_inet_pton for local_ip failed, ret(%d)", ret),
        conver_return_code(HCCP_INIT, ret));

    hccp_run_info("socket init:mode=%d phy_id=%u family=%d ip=%s", mode, rdev_info.phy_id, rdev_info.family, local_ip);

    socket_handle_tmp = calloc(1, sizeof(struct ra_socket_handle));
    CHK_PRT_RETURN(socket_handle_tmp == NULL,
        hccp_err("[init][ra_socket]ra_inet_pton for local_ip failed, ret(%d)", ret),
        conver_return_code(HCCP_INIT, -ENOMEM));

    if (mode == NETWORK_OFFLINE) {
        socket_handle_tmp->socket_ops = &g_ra_hdc_socket_ops;
    } else if (mode == NETWORK_PEER_ONLINE) {
        socket_handle_tmp->socket_ops = &g_ra_peer_socket_ops;
    } else {
        hccp_err("[init][ra_socket]Wrong mode(%d), do not support", mode);
        ret = -EINVAL;
        goto err;
    }

    ret = memcpy_s(&(socket_handle_tmp->rdev_info), sizeof(struct rdev), &rdev_info, sizeof(struct rdev));
    if (ret) {
        hccp_err("[init][ra_socket]memcpy_s for rdev_info failed, ret(%d)", ret);
        ret = -ESAFEFUNC;
        goto err;
    }

    if (rdev_info.family == AF_INET && rdev_info.local_ip.addr.s_addr < RA_VNIC_MAX &&
        socket_handle_tmp->socket_ops->ra_socket_init != NULL) {
        // HDC模式只支持IPv4
        if (rdev_info.local_ip.addr.s_addr < RA_VNIC_MAX) {
            ret = socket_handle_tmp->socket_ops->ra_socket_init(rdev_info);
            if (ret) {
                hccp_err("[init][ra_socket]ra socket init failed, ret(%d)", ret);
                goto err;
            }
        }
    }
    *socket_handle = (void*)socket_handle_tmp;
    return ret;
err:
    free(socket_handle_tmp);
    socket_handle_tmp = NULL;
    return conver_return_code(HCCP_INIT, ret);
}

HCCP_ATTRI_VISI_DEF int ra_socket_init_v1(int mode, struct socket_init_info_t socket_init, void **socket_handle)
{
    // 支持IPv4/IPv6 socket初始化
    // IPv6需要输入scope id
    struct ra_socket_handle *socket_handle_tmp = NULL;
    char local_ip[MAX_IP_LEN] = {0};
    int ret;

    CHK_PRT_RETURN(socket_init.rdev_info.phy_id >= RA_MAX_PHY_ID_NUM || socket_handle == NULL,
        hccp_err("[init][ra_socket]phy_id(%u) is invalid! it must be [0,%d) or socket is null!",
                 socket_init.rdev_info.phy_id, RA_MAX_PHY_ID_NUM), conver_return_code(HCCP_INIT, -EINVAL));

    ret = ra_inet_pton(socket_init.rdev_info.family, socket_init.rdev_info.local_ip, local_ip, MAX_IP_LEN);
    CHK_PRT_RETURN(ret, hccp_err("[init][ra_socket]ra_inet_pton for local_ip failed, ret(%d)", ret),
        conver_return_code(HCCP_INIT, ret));

    hccp_run_info("socket init:mode=%d phy_id=%u scope_id=%d family=%d ip=%s", mode, socket_init.rdev_info.phy_id,
        socket_init.scope_id, socket_init.rdev_info.family, local_ip);

    socket_handle_tmp = calloc(1, sizeof(struct ra_socket_handle));
    CHK_PRT_RETURN(socket_handle_tmp == NULL, hccp_err("[init][ra_socket]calloc for socket_handle failed"),
        conver_return_code(HCCP_INIT, -ENOMEM));

    if (mode == NETWORK_OFFLINE) {
        socket_handle_tmp->socket_ops = &g_ra_hdc_socket_ops;
        socket_handle_tmp->scope_id = socket_init.scope_id;
    } else if (mode == NETWORK_PEER_ONLINE) {
        socket_handle_tmp->socket_ops = &g_ra_peer_socket_ops;
        socket_handle_tmp->scope_id = socket_init.scope_id;
    } else {
        hccp_err("[init][ra_socket]Wrong mode(%d), do not support", mode);
        ret = -EINVAL;
        goto err;
    }

    ret = memcpy_s(&(socket_handle_tmp->rdev_info), sizeof(struct rdev), &socket_init.rdev_info, sizeof(struct rdev));
    if (ret) {
        hccp_err("[init][ra_socket]memcpy_s for rdev_info failed, ret(%d)", ret);
        ret = -ESAFEFUNC;
        goto err;
    }

    if (socket_init.rdev_info.family == AF_INET && socket_init.rdev_info.local_ip.addr.s_addr < RA_VNIC_MAX &&
        socket_handle_tmp->socket_ops->ra_socket_init != NULL) {
        // HDC模式只支持IPv4
        if (socket_init.rdev_info.local_ip.addr.s_addr < RA_VNIC_MAX) {
            ret = socket_handle_tmp->socket_ops->ra_socket_init(socket_init.rdev_info);
            if (ret) {
                hccp_err("[init][ra_socket]ra socket init v1 failed, ret(%d)", ret);
                goto err;
            }
        }
    }
    *socket_handle = (void*)socket_handle_tmp;
    return ret;
err:
    free(socket_handle_tmp);
    socket_handle_tmp = NULL;
    return conver_return_code(HCCP_INIT, ret);
}

HCCP_ATTRI_VISI_DEF int ra_socket_deinit(void *socket_handle)
{
    struct ra_socket_handle *socket_handle_tmp = NULL;
    struct rdev rdev_info;
    char local_ip[MAX_IP_LEN] = {0};
    int ret;

    CHK_PRT_RETURN(socket_handle == NULL, hccp_err("[deinit][ra_socket]socket_handle is NULL"),
        conver_return_code(HCCP_INIT, -EINVAL));

    socket_handle_tmp = (struct ra_socket_handle *)socket_handle;
    rdev_info = socket_handle_tmp->rdev_info;
    CHK_PRT_RETURN(rdev_info.phy_id >= RA_MAX_PHY_ID_NUM,
        hccp_err("[deinit][ra_socket]phy_id(%u) >= %u. invalid", rdev_info.phy_id, RA_MAX_PHY_ID_NUM),
        conver_return_code(HCCP_INIT, -EINVAL));

    ret = ra_inet_pton(rdev_info.family, rdev_info.local_ip, local_ip, MAX_IP_LEN);
    CHK_PRT_RETURN(ret, hccp_err("[deinit][ra_socket]ra_inet_pton for local_ip failed, ret(%d)", ret),
        conver_return_code(HCCP_INIT, ret));

    hccp_run_info("Input parameters: phy_id[%u] family[%d] local_ip[%s]", rdev_info.phy_id, rdev_info.family, local_ip);

    ret = socket_handle_tmp->socket_ops->ra_socket_deinit(rdev_info);
    if (ret) {
        hccp_err("[deinit][ra_socket]ra socket deinit failed, ret(%d)", ret);
    }

    socket_handle_tmp->socket_ops = NULL;
    free(socket_handle_tmp);
    socket_handle_tmp = NULL;
    return conver_return_code(HCCP_INIT, ret);
}

STATIC int ra_rdev_init_check_ip(int mode, struct rdev rdev_info, char local_ip[])
{
    struct interface_info *interface_infos = NULL;
    char interface_ip[MAX_IP_LEN] = { 0 };
    struct ra_get_ifattr config = { 0 };
    unsigned int i, interface_version;
    unsigned int num = 0;
    int ret;

    config.phy_id = rdev_info.phy_id;
    config.nic_position = (unsigned int)mode;
    if (config.nic_position == NETWORK_OFFLINE) {
        ret = ra_get_interface_version(config.phy_id, RA_RS_GET_IFNUM, &interface_version);
        if (ret != 0 || interface_version != RA_RS_GET_IFNUM_VERSION) {
            num = MAX_INTERFACE_NUM;
            goto get_addrs;
        }
    }
    /* get the number of interfaces */
    ret = ra_get_ifnum(&config, &num);
    CHK_PRT_RETURN(ret != 0 || num == 0, hccp_err("[check][ip]get_ifnum failed, ret(%d) or num is 0", ret), -EINVAL);

get_addrs:
    /* calloc for interface_infos according to the real num */
    interface_infos = calloc(num, sizeof(struct interface_info));
    CHK_PRT_RETURN(interface_infos == NULL, hccp_err("[check][ip]calloc for interface_infos failed"), -EINVAL);

    ret = ra_get_ifaddrs(&config, interface_infos, &num);
    if (ret || (num == 0)) {
        hccp_err("[check][ip]ra_get_ifaddrs for interface_infos failed, ret(%d), num(%u)", ret, num);
        free(interface_infos);
        return -EINVALIDIP;
    }

    for (i = 0; i < num; i++) {
        if (interface_infos[i].family != rdev_info.family) {
            continue;
        }

        ret = ra_inet_pton(interface_infos[i].family, interface_infos[i].ifaddr.ip, interface_ip, MAX_IP_LEN);
        if (ret != 0) {
            hccp_err("[check][ip]ra_inet_pton for interface_infos[%u] failed, ret(%d)", i, ret);
            free(interface_infos);
            return ret;
        }

        ret = strncmp(interface_ip, local_ip, MAX_IP_LEN - 1);
        if (!ret) {
            free(interface_infos);
            return 0;
        }
    }
    hccp_err("[check][ip]failed, ret(%d) the IP address(%s) in the ranktable is inconsistent with the IP(%s)"\
        "address of the network adapter, please make sure they're consistent. "\
        "num(%u)", ret, local_ip, interface_ip, num);

    free(interface_infos);
    return -EINVALIDIP;
}

int ra_rdev_init_check(int mode, struct rdev rdev_info, char local_ip[], unsigned int num, void *rdma_handle)
{
    int ret;

    CHK_PRT_RETURN(rdev_info.phy_id >= RA_MAX_PHY_ID_NUM, hccp_err("[check][ra_rdev_init]phy_id(%u) is invalid!"
        "it must greater or equal to 0 and less than %d!", rdev_info.phy_id, RA_MAX_PHY_ID_NUM), -EINVAL);

    CHK_PRT_RETURN(rdma_handle == NULL, hccp_err("[check][ra_rdev_init]phy_id(%u) rdma_handle is null!",
        rdev_info.phy_id), -EINVAL);

    ret = ra_inet_pton(rdev_info.family, rdev_info.local_ip, local_ip, num);
    CHK_PRT_RETURN(ret, hccp_err("[check][ra_rdev_init]ra_inet_pton for local_ip failed, ret(%d)", ret), -EINVAL);

    ret = ra_rdev_init_check_ip(mode, rdev_info, local_ip);
    CHK_PRT_RETURN(ret, hccp_err("[check][ra_rdev_init]ra_rdev_init_check_ip failed, ret(%d)", ret), ret);

    return 0;
}

STATIC int ra_get_init_rdma_handle(int mode, struct ra_rdma_handle *rdma_handle)
{
    if (mode == NETWORK_OFFLINE) {
        (void)ra_hdc_rdma_set_ops(rdma_handle, &g_ra_hdc_rdma_ops);
    } else if (mode == NETWORK_PEER_ONLINE) {
        (void)ra_hdc_rdma_set_ops(rdma_handle, &g_ra_peer_rdma_ops);
    } else {
        hccp_err("[init][ra_rdev]Wrong mode(%d), do not support", mode);
        return -EINVAL;
    }

    CHK_PRT_RETURN(rdma_handle->rdma_ops->ra_rdev_init == NULL, hccp_err("[init][ra_rdev] ra_rdev_init is NULL!"),
        -EINVAL);
    return 0;
}

STATIC void ra_generate_gid_by_rdev_info(struct ra_rdma_handle *rdma_handle)
{
#define RA_GID_SEQ_NUM   4
#define RA_GID_SEQ_ZERO  0
#define RA_GID_SEQ_ONE   1
#define RA_GID_SEQ_TWO   2
#define RA_GID_SEQ_THREE 3
    union hccp_ip_addr local_ip = rdma_handle->rdev_info.local_ip;
    int family = rdma_handle->rdev_info.family;
    unsigned int gid_v4[RA_GID_SEQ_NUM];

    if (family == AF_INET6) {
        (void)memcpy_s(rdma_handle->gid, HCCP_GID_RAW_LEN, &(local_ip.addr6), HCCP_GID_RAW_LEN);
    } else {
        gid_v4[RA_GID_SEQ_ZERO] = 0;
        gid_v4[RA_GID_SEQ_ONE] = 0;
        /* The gid format generated by ipv4 is filled with 0xFFFF in [33, 48] */
        gid_v4[RA_GID_SEQ_TWO]   = htonl(0x0000FFFF);
        gid_v4[RA_GID_SEQ_THREE] = local_ip.addr.s_addr;
        (void)memcpy_s(rdma_handle->gid, HCCP_GID_RAW_LEN, gid_v4, HCCP_GID_RAW_LEN);
    }

    return;
}

STATIC int ra_rdev_init_with_backup_info(struct rdev_init_info init_info, struct rdev rdev_info,
    struct ra_backup_info backup_info, void **rdma_handle)
{
    struct ra_rdma_handle *rdma_handle_tmp = NULL;
    char local_ip[MAX_IP_LEN] = {0};
    unsigned int rdev_index;
    int ret;

    ret = ra_rdev_init_check(init_info.mode, rdev_info, local_ip, MAX_IP_LEN, rdma_handle);
    CHK_PRT_RETURN(ret, hccp_err("[init][ra_rdev]ra_rdev_init_check failed ,ret(%d)", ret),
        conver_return_code(HCCP_INIT, ret));

    hccp_run_info("rdev_init:mode=%d phy_id=%u family=%d ip=%s notify_type=%u", init_info.mode, rdev_info.phy_id,
        rdev_info.family, local_ip, init_info.notify_type);

    rdma_handle_tmp = calloc(1, sizeof(struct ra_rdma_handle));
    CHK_PRT_RETURN(rdma_handle_tmp == NULL, hccp_err("[init][ra_rdev]calloc for rdma_handle failed"),
        conver_return_code(HCCP_INIT, -ENOMEM));

    // disabled_lite_thread will be invalid if enabled_910a_lite is false
    rdma_handle_tmp->disabled_lite_thread = init_info.disabled_lite_thread;
    rdma_handle_tmp->enabled_910a_lite = init_info.enabled_910a_lite;
    rdma_handle_tmp->enabled_2mb_lite = init_info.enabled_2mb_lite;
    (void)memcpy_s(&rdma_handle_tmp->backup_info, sizeof(struct ra_backup_info),
        &backup_info, sizeof(struct ra_backup_info));

    ret = ra_get_init_rdma_handle(init_info.mode, rdma_handle_tmp);
    if (ret) {
        hccp_err("[init][ra_rdev] get rdma handle failed, ret(%d)", ret);
        goto err;
    }

    ret = memcpy_s(&(rdma_handle_tmp->rdev_info), sizeof(struct rdev), &rdev_info, sizeof(struct rdev));
    if (ret) {
        hccp_err("[init][ra_rdev]memcpy_s for rdev_info failed, ret(%d)", ret);
        ret = -ESAFEFUNC;
        goto err;
    }

    ret = rdma_handle_tmp->rdma_ops->ra_rdev_init(rdma_handle_tmp, init_info.notify_type, rdev_info, &rdev_index);
    if (ret) {
        hccp_err("[init][ra_rdev]ra rdev init failed, ret(%d)", ret);
        goto err;
    }

    rdma_handle_tmp->rdev_index = rdev_index;
    ra_generate_gid_by_rdev_info(rdma_handle_tmp);
    *rdma_handle = (void *)rdma_handle_tmp;

    // save rdev handle for helper
    ra_rdev_set_handle(rdev_info.phy_id, *rdma_handle);

    return ret;
err:
    free(rdma_handle_tmp);
    rdma_handle_tmp = NULL;
    return conver_return_code(HCCP_INIT, ret);
}

HCCP_ATTRI_VISI_DEF int ra_rdev_init_with_backup(struct rdev_init_info *init_info, struct rdev *rdev_info,
    struct rdev *backup_rdev_info, void **rdma_handle)
{
    struct ra_backup_info backup_info = { 0 };

    if (init_info == NULL || rdev_info == NULL || backup_rdev_info == NULL || rdma_handle == NULL) {
        hccp_err("[init][ra_rdev]init_info or rdev_info or backup_rdev_info or rdma_handle is NULL");
        return -EINVAL;
    }

    backup_info.backup_flag = true;
    (void)memcpy_s(&backup_info.rdev_info, sizeof(struct rdev), backup_rdev_info, sizeof(struct rdev));

    return ra_rdev_init_with_backup_info(*init_info, *rdev_info, backup_info, rdma_handle);
}

HCCP_ATTRI_VISI_DEF int ra_rdev_init_v2(struct rdev_init_info init_info, struct rdev rdev_info, void **rdma_handle)
{
    struct ra_backup_info backup_info = { 0 };

    return ra_rdev_init_with_backup_info(init_info, rdev_info, backup_info, rdma_handle);
}

HCCP_ATTRI_VISI_DEF int ra_rdev_init(int mode, unsigned int notify_type, struct rdev rdev_info, void **rdma_handle)
{
    struct rdev_init_info init_info = { 0 };
    init_info.mode = mode;
    init_info.notify_type = notify_type;
    init_info.disabled_lite_thread = false; // will start lite thread by default
    init_info.enabled_910a_lite = false; // will disabled lite on 910A by default
    init_info.enabled_2mb_lite = false; // will disabled lite on 2MB page align scenario by default

    return ra_rdev_init_v2(init_info, rdev_info, rdma_handle);
}

STATIC int ra_rdev_deinit_para_check(void *rdma_handle, struct ra_rdma_handle **rdma_handle_tmp)
{
    int ret;
    char local_ip[MAX_IP_LEN] = {0};
    struct rdev rdev_info;

    *rdma_handle_tmp = (struct ra_rdma_handle *)rdma_handle;
    rdev_info = (*rdma_handle_tmp)->rdev_info;
    CHK_PRT_RETURN(rdev_info.phy_id >= RA_MAX_PHY_ID_NUM, hccp_err("[deinit][ra_rdev]phy_id(%u)"
        "must smaller than %u", rdev_info.phy_id, RA_MAX_PHY_ID_NUM), -EINVAL);

    ret = ra_inet_pton(rdev_info.family, rdev_info.local_ip, local_ip, MAX_IP_LEN);
    CHK_PRT_RETURN(ret, hccp_err("[deinit][ra_rdev]ra_inet_pton for local_ip failed, ret(%d)", ret), ret);

    CHK_PRT_RETURN((*rdma_handle_tmp)->rdma_ops == NULL || (*rdma_handle_tmp)->rdma_ops->ra_rdev_deinit == NULL,
        hccp_err("[deinit][ra_rdev]rdma_ops is NULL or ra_rdev_deinit is NULL"), -EINVAL);

    hccp_run_info("Input parameters: phy_id[%u] rdev_index[%u] family[%d] local_ip[%s]",
        rdev_info.phy_id, (*rdma_handle_tmp)->rdev_index, rdev_info.family, local_ip);

    return 0;
}

HCCP_ATTRI_VISI_DEF int ra_rdev_get_port_status(void *rdma_handle, enum port_status *status)
{
    struct ra_rdma_handle *rdma_handle_tmp = (struct ra_rdma_handle *)rdma_handle;
    unsigned int phy_id;
    int ret;

    CHK_PRT_RETURN(rdma_handle == NULL || status == NULL,
        hccp_err("[get][ra_port_status]rdma_handle or status is NULL, invalid"),
        conver_return_code(RDMA_OP, -EINVAL));

    phy_id = rdma_handle_tmp->rdev_info.phy_id;
    CHK_PRT_RETURN(phy_id >= RA_MAX_PHY_ID_NUM,
        hccp_err("[get][ra_port_status]phy_id(%u) must smaller than %u", phy_id, RA_MAX_PHY_ID_NUM),
        conver_return_code(RDMA_OP, -EINVAL));

    CHK_PRT_RETURN(rdma_handle_tmp->rdma_ops == NULL || rdma_handle_tmp->rdma_ops->ra_rdev_get_port_status == NULL,
        hccp_err("[get][ra_port_status]rdma_ops is NULL or ra_rdev_get_port_status is NULL, invalid"),
        conver_return_code(RDMA_OP, -EINVAL));

    ret = rdma_handle_tmp->rdma_ops->ra_rdev_get_port_status(rdma_handle_tmp, status);
    return conver_return_code(RDMA_OP, ret);
}

HCCP_ATTRI_VISI_DEF int ra_rdev_deinit(void *rdma_handle, unsigned int notify_type)
{
    struct ra_rdma_handle *rdma_handle_tmp = NULL;
    unsigned int phy_id;
    int ret;

    CHK_PRT_RETURN(rdma_handle == NULL, hccp_err("[deinit][ra_rdev] rdma_handle is NULL"),
        conver_return_code(HCCP_INIT, -EINVAL));

    ret = ra_rdev_deinit_para_check(rdma_handle, &rdma_handle_tmp);
    CHK_PRT_RETURN(ret, hccp_err("[deinit][ra_rdev] para check failed, ret(%d)", ret),
        conver_return_code(HCCP_INIT, ret));

    phy_id = rdma_handle_tmp->rdev_info.phy_id;

    ret = rdma_handle_tmp->rdma_ops->ra_rdev_deinit(rdma_handle_tmp, notify_type);
    if (ret) {
        hccp_err("[deinit][ra_rdev]ra rdv deinit failed, ret(%d)", ret);
        goto free_rdma_handle;
    }

free_rdma_handle:
    rdma_handle_tmp->rdma_ops = NULL;
    free(rdma_handle_tmp);
    rdma_handle_tmp = NULL;
    ra_rdev_set_handle(phy_id, NULL);
    return conver_return_code(HCCP_INIT, ret);
}

HCCP_ATTRI_VISI_DEF int ra_rdev_get_support_lite(void *rdma_handle, int *support_lite)
{
    struct ra_rdma_handle *rdma_handle_tmp = NULL;

    CHK_PRT_RETURN(rdma_handle == NULL || support_lite == NULL,
        hccp_err("[get][ra_rdev]rdma_handle is NULL or support_lite is NULL"), conver_return_code(RDMA_OP, -EINVAL));

    rdma_handle_tmp = (struct ra_rdma_handle *)rdma_handle;
    *support_lite = rdma_handle_tmp->support_lite;

    hccp_dbg("[get][ra_rdev]support_lite:%d", *support_lite);

    return 0;
}

HCCP_ATTRI_VISI_DEF int ra_socket_batch_connect(struct socket_connect_info_t conn[], unsigned int num)
{
    struct ra_socket_handle *socket_handle = NULL;
    char remote_ip[MAX_IP_LEN] = {0};
    char local_ip[MAX_IP_LEN] = {0};
    unsigned int i;
    int ret;

    CHK_PRT_RETURN(conn == NULL || num == 0 || num > MAX_SOCKET_NUM,
        hccp_err("[batch_connect][ra_socket]conn is NULL or num[%u] is zero or num is greater than %d", num,
        MAX_SOCKET_NUM), conver_return_code(SOCKET_OP, -EINVAL));

    for (i = 0; i < num; i++) {
        socket_handle = (struct ra_socket_handle *)conn[i].socket_handle;
        if (socket_handle == NULL || socket_handle->socket_ops == NULL ||
            socket_handle->socket_ops->ra_socket_batch_connect == NULL) {
            hccp_err("[batch_connect][ra_socket]socket_handle or func is NULL");
            return conver_return_code(SOCKET_OP, -EINVAL);
        }

        CHK_PRT_RETURN(socket_handle->rdev_info.phy_id >= RA_MAX_PHY_ID_NUM,
            hccp_err("[batch_connect][ra_socket]phy_id(%u) must smaller than %u", socket_handle->rdev_info.phy_id,
            RA_MAX_PHY_ID_NUM), conver_return_code(SOCKET_OP, -EINVAL));

        CHK_PRT_RETURN(strlen(conn[i].tag) >= SOCK_CONN_TAG_SIZE,
            hccp_err("[batch_connect][ra_socket]conn tag len(%d) more than max len(%d)", strlen(conn[i].tag),
            SOCK_CONN_TAG_SIZE), conver_return_code(SOCKET_OP, -EINVAL));

        ret = ra_inet_pton(socket_handle->rdev_info.family, socket_handle->rdev_info.local_ip, local_ip, MAX_IP_LEN);
        CHK_PRT_RETURN(ret, hccp_err("[batch_connect][ra_socket]ra_inet_pton for local_ip failed, ret(%d)", ret),
            conver_return_code(SOCKET_OP, ret));

        ret = ra_inet_pton(socket_handle->rdev_info.family, conn[i].remote_ip, remote_ip, MAX_IP_LEN);
        CHK_PRT_RETURN(ret, hccp_err("[batch_connect][ra_socket]ra_inet_pton for remote_ip failed, ret(%d)", ret),
            conver_return_code(SOCKET_OP, ret));

        hccp_run_info("Input parameters: [%u]th, phy_id[%u], local_ip[%s], remote_ip[%s], port[%u], tag[%s], cnt[%u]",
            i, socket_handle->rdev_info.phy_id, local_ip, remote_ip, conn[i].port, conn[i].tag,
            socket_handle->connect_cnt);
    }

    socket_handle->connect_cnt++;
    ret = socket_handle->socket_ops->ra_socket_batch_connect(socket_handle->rdev_info.phy_id, conn, num);
    return conver_return_code(SOCKET_OP, ret);
}

HCCP_ATTRI_VISI_DEF int ra_socket_batch_close(struct socket_close_info_t conn[], unsigned int num)
{
    struct ra_socket_handle *socket_handle = NULL;
    char local_ip[MAX_IP_LEN] = {0};
    unsigned int phy_id;
    unsigned int i;
    int ret;

    CHK_PRT_RETURN(conn == NULL || num == 0 || num > MAX_SOCKET_NUM,
        hccp_err("[batch_close][ra_socket]conn is NULL or num[%u] is zero or num is greater than %d", num,
        MAX_SOCKET_NUM), conver_return_code(SOCKET_OP, -EINVAL));

    for (i = 0; i < num; i++) {
        socket_handle = (struct ra_socket_handle *)conn[i].socket_handle;
        if (socket_handle == NULL || socket_handle->socket_ops == NULL ||
            socket_handle->socket_ops->ra_socket_batch_close == NULL) {
            hccp_err("[batch_close][ra_socket]socket_handle or func is NULL");
            return conver_return_code(SOCKET_OP, -EINVAL);
        }
        phy_id = socket_handle->rdev_info.phy_id;
        CHK_PRT_RETURN(phy_id >= RA_MAX_PHY_ID_NUM,
            hccp_err("[batch_close][ra_socket]phy_id(%u) must smaller than %u", phy_id, RA_MAX_PHY_ID_NUM),
            conver_return_code(SOCKET_OP, -EINVAL));

        ret = ra_inet_pton(socket_handle->rdev_info.family, socket_handle->rdev_info.local_ip, local_ip, MAX_IP_LEN);
        CHK_PRT_RETURN(ret, hccp_err("[batch_connect][ra_socket]ra_inet_pton for local_ip failed, ret(%d)", ret),
            conver_return_code(SOCKET_OP, ret));

        hccp_run_info("Input parameters: [%u]th, phy_id[%u], local_ip[%s], cnt[%u]", i, phy_id,
            local_ip, socket_handle->close_cnt);
    }

    socket_handle->close_cnt++;
    ret = socket_handle->socket_ops->ra_socket_batch_close(phy_id, conn, num);
    return conver_return_code(SOCKET_OP, ret);
}

HCCP_ATTRI_VISI_DEF int ra_socket_batch_abort(struct socket_connect_info_t conn[], unsigned int num)
{
    struct ra_socket_handle *socket_handle = NULL;
    char remote_ip[MAX_IP_LEN] = {0};
    char local_ip[MAX_IP_LEN] = {0};
    unsigned int phy_id;
    unsigned int i;
    int ret;

    CHK_PRT_RETURN(conn == NULL || num == 0 || num > MAX_SOCKET_NUM,
        hccp_err("[batch_abort][ra_socket]conn is NULL or num[%u] is zero or num is greater than %d",
        num, MAX_SOCKET_NUM), conver_return_code(SOCKET_OP, -EINVAL));

    for (i = 0; i < num; i++) {
        socket_handle = (struct ra_socket_handle *)conn[i].socket_handle;
        CHK_PRT_RETURN(socket_handle == NULL || socket_handle->socket_ops == NULL ||
            socket_handle->socket_ops->ra_socket_batch_abort == NULL,
            hccp_err("[batch_abort][ra_socket]socket_handle or func is NULL"),
            conver_return_code(SOCKET_OP, -EINVAL));

        phy_id = socket_handle->rdev_info.phy_id;
        CHK_PRT_RETURN(phy_id >= RA_MAX_PHY_ID_NUM,
            hccp_err("[batch_abort][ra_socket]phy_id(%u) must smaller than %u", phy_id, RA_MAX_PHY_ID_NUM),
            conver_return_code(SOCKET_OP, -EINVAL));

        ret = ra_inet_pton(socket_handle->rdev_info.family, socket_handle->rdev_info.local_ip, local_ip, MAX_IP_LEN);
        CHK_PRT_RETURN(ret, hccp_err("[batch_abort][ra_socket]ra_inet_pton for local_ip failed, ret(%d)", ret),
            conver_return_code(SOCKET_OP, ret));

        ret = ra_inet_pton(socket_handle->rdev_info.family, conn[i].remote_ip, remote_ip, MAX_IP_LEN);
        CHK_PRT_RETURN(ret, hccp_err("[batch_abort][ra_socket]ra_inet_pton for remote_ip failed, ret(%d)", ret),
            conver_return_code(SOCKET_OP, ret));

        hccp_run_info("Input parameters: [%u]th, phy_id[%u], local_ip[%s], remote_ip[%s], tag[%s], cnt[%u]",
            i, phy_id, local_ip, remote_ip, conn[i].tag, socket_handle->abort_cnt);
    }

    socket_handle->abort_cnt++;
    ret = socket_handle->socket_ops->ra_socket_batch_abort(phy_id, conn, num);
    return conver_return_code(SOCKET_OP, ret);
}

HCCP_ATTRI_VISI_DEF int ra_socket_listen_start(struct socket_listen_info_t conn[], unsigned int num)
{
    struct ra_socket_handle *socket_handle = NULL;
    char local_ip[MAX_IP_LEN] = {0};
    unsigned int phy_id;
    unsigned int i;
    int ret;

    CHK_PRT_RETURN(conn == NULL || num == 0 || num > MAX_SOCKET_NUM,
        hccp_err("[listen_start][ra_socket]conn is NULL or num[%u] is zero or num is greater than %d", num,
        MAX_SOCKET_NUM), conver_return_code(SOCKET_OP, -EINVAL));

    for (i = 0; i < num; i++) {
        socket_handle = (struct ra_socket_handle *)conn[i].socket_handle;
        if (socket_handle == NULL || socket_handle->socket_ops == NULL ||
            socket_handle->socket_ops->ra_socket_listen_start == NULL) {
            hccp_err("[listen_start][ra_socket]socket_handle or func is NULL");
            return conver_return_code(SOCKET_OP, -EINVAL);
        }
        phy_id = socket_handle->rdev_info.phy_id;
        CHK_PRT_RETURN(phy_id >= RA_MAX_PHY_ID_NUM,
            hccp_err("[listen_start][ra_socket]phy_id(%u) must smaller than %u", phy_id, RA_MAX_PHY_ID_NUM),
            conver_return_code(SOCKET_OP, -EINVAL));

        ret = ra_inet_pton(socket_handle->rdev_info.family, socket_handle->rdev_info.local_ip, local_ip, MAX_IP_LEN);
        CHK_PRT_RETURN(ret, hccp_err("[listen_start][ra_socket]ra_inet_pton for server_ip failed, ret(%d)", ret),
            conver_return_code(SOCKET_OP, ret));

        hccp_run_info("Input parameters: [%u]th, phy_id[%u], local_ip[%s], port[%u]",
            i, phy_id, local_ip, conn[i].port);
    }

    ret = socket_handle->socket_ops->ra_socket_listen_start(phy_id, conn, num);
    return conver_return_code(SOCKET_OP, ret);
}

HCCP_ATTRI_VISI_DEF int ra_socket_listen_stop(struct socket_listen_info_t conn[], unsigned int num)
{
    struct ra_socket_handle *socket_handle = NULL;
    char local_ip[MAX_IP_LEN] = {0};
    unsigned int phy_id;
    unsigned int i;
    int ret;

    CHK_PRT_RETURN(conn == NULL || num == 0 || num > MAX_SOCKET_NUM,
        hccp_err("[listen_stop][ra_socket]conn is NULL or num[%u] is zero or num is greater than %d", num,
        MAX_SOCKET_NUM), conver_return_code(SOCKET_OP, -EINVAL));

    for (i = 0; i < num; i++) {
        socket_handle = (struct ra_socket_handle *)conn[i].socket_handle;
        if (socket_handle == NULL || socket_handle->socket_ops == NULL ||
            socket_handle->socket_ops->ra_socket_listen_stop == NULL) {
            hccp_err("[listen_stop][ra_socket]socket_handle or func is NULL");
            return conver_return_code(SOCKET_OP, -EINVAL);
        }

        phy_id = socket_handle->rdev_info.phy_id;
        CHK_PRT_RETURN(phy_id >= RA_MAX_PHY_ID_NUM,
            hccp_err("[listen_stop][ra_socket]phy_id(%u) must smaller than %u", phy_id, RA_MAX_PHY_ID_NUM),
            conver_return_code(SOCKET_OP, -EINVAL));

        ret = ra_inet_pton(socket_handle->rdev_info.family, socket_handle->rdev_info.local_ip, local_ip, MAX_IP_LEN);
        CHK_PRT_RETURN(ret, hccp_err("[listen_stop][ra_socket]ra_inet_pton for server_ip failed, ret(%d)", ret),
            conver_return_code(SOCKET_OP, ret));

        hccp_run_info("Input parameters: [%u]th, phy_id[%u], local_ip[%s]", i, phy_id, local_ip);
    }

    ret = socket_handle->socket_ops->ra_socket_listen_stop(phy_id, conn, num);
    return conver_return_code(SOCKET_OP, ret);
}

HCCP_ATTRI_VISI_DEF int ra_get_sockets(unsigned int role, struct socket_info_t conn[], unsigned int num,
    unsigned int *connected_num)
{
    unsigned int i;
    struct ra_socket_handle *socket_handle = NULL;
    char local_ip[MAX_IP_LEN] = {0};
    char remote_ip[MAX_IP_LEN] = {0};
    int ret;
    unsigned int phy_id;

    CHK_PRT_RETURN(conn == NULL || connected_num == NULL || num == 0 || num > MAX_SOCKET_NUM,
        hccp_err("[get][ra_socket]conn or connected_num is NULL or num[%u] is zero or num greater than %d", num,
        MAX_SOCKET_NUM), conver_return_code(SOCKET_OP, -EINVAL));

    for (i = 0; i < num; i++) {
        socket_handle = (struct ra_socket_handle *)conn[i].socket_handle;
        if (socket_handle == NULL || socket_handle->socket_ops == NULL ||
            socket_handle->socket_ops->ra_get_sockets == NULL) {
            hccp_err("[get][ra_socket]socket_handle or func is NULL");
            return conver_return_code(SOCKET_OP, -EINVAL);
        }

        phy_id = socket_handle->rdev_info.phy_id;
        CHK_PRT_RETURN(phy_id >= RA_MAX_PHY_ID_NUM,
            hccp_err("[get][ra_socket]phy_id(%u) must smaller than %u", phy_id, RA_MAX_PHY_ID_NUM),
            conver_return_code(SOCKET_OP, -EINVAL));

        ret = ra_inet_pton(socket_handle->rdev_info.family, socket_handle->rdev_info.local_ip, local_ip, MAX_IP_LEN);
        CHK_PRT_RETURN(ret, hccp_err("[get][ra_socket]ra_inet_pton for local_ip failed, ret(%d)", ret),
            conver_return_code(SOCKET_OP, ret));

        ret = ra_inet_pton(socket_handle->rdev_info.family, conn[i].remote_ip, remote_ip, MAX_IP_LEN);
        CHK_PRT_RETURN(ret, hccp_err("[get][ra_socket]ra_inet_pton for remote_ip failed, ret(%d)", ret),
            conver_return_code(SOCKET_OP, ret));
    }

    ret = socket_handle->socket_ops->ra_get_sockets(phy_id, role, conn, num);
    if (ret >= 0) {
        *connected_num = (unsigned int)ret;
        return 0;
    }

    *connected_num = 0;
    return conver_return_code(SOCKET_OP, ret);
}

HCCP_ATTRI_VISI_DEF int ra_socket_recv(const void *fd_handle, void *data, unsigned long long size,
    unsigned long long *received_size)
{
    int ret;
    unsigned int phy_id;
    const struct ra_socket_handle *socket_handle_tmp = NULL;
    const struct socket_hdc_info *fd_handle_tmp = (const struct socket_hdc_info *)fd_handle;

    CHK_PRT_RETURN(fd_handle == NULL || data == NULL || size == 0 || received_size == NULL,
        hccp_err("[recv][ra_socket]fd_handle or data or received_size is NULL or size[%llu] is 0", size),
        conver_return_code(SOCKET_OP, -EINVAL));

    socket_handle_tmp = (const struct ra_socket_handle *)(fd_handle_tmp->socket_handle);
    if (socket_handle_tmp == NULL || socket_handle_tmp->socket_ops == NULL ||
        socket_handle_tmp->socket_ops->ra_socket_recv == NULL) {
        hccp_err("[recv][ra_socket]socket_handle_tmp or func is NULL");
        return conver_return_code(SOCKET_OP, -EINVAL);
    }
    phy_id = socket_handle_tmp->rdev_info.phy_id;
    CHK_PRT_RETURN(phy_id >= RA_MAX_PHY_ID_NUM,
        hccp_err("[recv][ra_socket]phy_id(%u) must smaller than %u", phy_id, RA_MAX_PHY_ID_NUM),
        conver_return_code(SOCKET_OP, -EINVAL));

    ret = socket_handle_tmp->socket_ops->ra_socket_recv(phy_id, fd_handle, data, size);
    if (ret > 0) {
        *received_size = (unsigned long long)(unsigned int)ret;
        return 0;
    } else if (ret == 0) {
        hccp_warn("[recv][ra_socket]socket has been closed. received_size is 0");
        ret = -ESOCKCLOSED;
    }

    *received_size = 0;
    return conver_return_code(SOCKET_OP, ret);
}

HCCP_ATTRI_VISI_DEF int ra_socket_send(const void *fd_handle, const void *data, unsigned long long size,
    unsigned long long *sent_size)
{
    int ret;
    unsigned int phy_id;
    const struct ra_socket_handle *socket_handle_tmp = NULL;
    const struct socket_hdc_info *fd_handle_tmp = (const struct socket_hdc_info *)fd_handle;

    CHK_PRT_RETURN(fd_handle == NULL || data == NULL || sent_size == NULL || size == 0,
        hccp_err("[send][ra_socket]fd_handle or data or sent_size is NULL or size[%llu] is 0", size),
        conver_return_code(SOCKET_OP, -EINVAL));

    socket_handle_tmp = (const struct ra_socket_handle *)(fd_handle_tmp->socket_handle);
    if (socket_handle_tmp == NULL || socket_handle_tmp->socket_ops == NULL ||
        socket_handle_tmp->socket_ops->ra_socket_send == NULL) {
        hccp_err("[send][ra_socket]socket_handle_tmp or func is NULL");
        return conver_return_code(SOCKET_OP, -EINVAL);
    }

    phy_id = socket_handle_tmp->rdev_info.phy_id;
    CHK_PRT_RETURN(phy_id >= RA_MAX_PHY_ID_NUM,
        hccp_err("[send][ra_socket]phy_id(%u) must smaller than %u", phy_id, RA_MAX_PHY_ID_NUM),
        conver_return_code(SOCKET_OP, -EINVAL));

    ret = socket_handle_tmp->socket_ops->ra_socket_send(phy_id, fd_handle, data, size);
    if (ret > 0) {
        *sent_size = (unsigned long long)(unsigned int)ret;
        return 0;
    } else if (ret == 0) {
        hccp_warn("[send][ra_socket]socket has been closed. sent_size is 0");
        ret = -ESOCKCLOSED;
    }

    *sent_size = 0;
    return conver_return_code(SOCKET_OP, ret);
}

HCCP_ATTRI_VISI_DEF int ra_epoll_ctl_add(const void *fd_handle, enum RaEpollEvent event)
{
    CHK_PRT_RETURN(fd_handle == NULL, hccp_err("[ra_epoll_ctl_add]fd_handle is NULL"),
        conver_return_code(SOCKET_OP, -EINVAL));

    CHK_PRT_RETURN(event != RA_EPOLLIN && event != RA_EPOLLONESHOT,
        hccp_err("[ra_epoll_ctl_add]wrong event, only RA_EPOLLIN and RA_EPOLLONESHOT are supported."),
        conver_return_code(SOCKET_OP, -EINVAL));

    int ret = ra_peer_epoll_ctl_add(fd_handle, event);
    CHK_PRT_RETURN(ret, hccp_err("[ra_epoll_ctl_add]ra_peer_epoll_ctl_add failed ret(%d)", ret),
        conver_return_code(SOCKET_OP, ret));

    return 0;
}

HCCP_ATTRI_VISI_DEF int ra_epoll_ctl_mod(const void *fd_handle, enum RaEpollEvent event)
{
    CHK_PRT_RETURN(fd_handle == NULL, hccp_err("[ra_epoll_ctl_mod]fd_handle is NULL"),
        conver_return_code(SOCKET_OP, -EINVAL));

    CHK_PRT_RETURN(event != RA_EPOLLIN && event != RA_EPOLLONESHOT,
        hccp_err("[ra_epoll_ctl_mod]wrong event, only RA_EPOLLIN and RA_EPOLLONESHOT are supported."),
        conver_return_code(SOCKET_OP, -EINVAL));

    int ret = ra_peer_epoll_ctl_mod(fd_handle, event);
    CHK_PRT_RETURN(ret, hccp_err("[ra_epoll_ctl_mod]ra_peer_epoll_ctl_mod failed ret(%d)", ret),
        conver_return_code(SOCKET_OP, ret));

    return 0;
}

HCCP_ATTRI_VISI_DEF int ra_epoll_ctl_del(const void *fd_handle)
{
    CHK_PRT_RETURN(fd_handle == NULL, hccp_err("[ra_epoll_ctl_del]fd_handle is NULL"),
        conver_return_code(SOCKET_OP, -EINVAL));

    int ret = ra_peer_epoll_ctl_del(fd_handle);
    CHK_PRT_RETURN(ret, hccp_err("[ra_epoll_ctl_del]ra_peer_epoll_ctl_del failed ret(%d)", ret),
        conver_return_code(SOCKET_OP, ret));

    return 0;
}

HCCP_ATTRI_VISI_DEF int ra_set_tcp_recv_callback(const void *socket_handle, const void *callback)
{
    const struct ra_socket_handle *socket_handle_tmp = (const struct ra_socket_handle *)socket_handle;
    unsigned int phy_id;

    CHK_PRT_RETURN(socket_handle_tmp == NULL || callback == NULL,
        hccp_err("[ra_socket]socket_handle is NULL or callback is NULL"),
        conver_return_code(SOCKET_OP, -EINVAL));

    phy_id = socket_handle_tmp->rdev_info.phy_id;
    CHK_PRT_RETURN(phy_id >= RA_MAX_PHY_ID_NUM,
        hccp_err("[ra_socket]phy_id(%u) must smaller than %u", phy_id, RA_MAX_PHY_ID_NUM),
        conver_return_code(SOCKET_OP, -EINVAL));

    ra_peer_set_tcp_recv_callback(phy_id, callback);

    return 0;
}

HCCP_ATTRI_VISI_DEF int ra_get_tsqp_depth(void *rdev_handle, unsigned int *temp_depth, unsigned int *qp_num)
{
    struct ra_rdma_handle *rdma_handle_tmp = (struct ra_rdma_handle *)rdev_handle;
    unsigned int phy_id;
    int ret;

    CHK_PRT_RETURN(rdev_handle == NULL || rdma_handle_tmp->rdma_ops == NULL ||
        rdma_handle_tmp->rdma_ops->ra_get_tsqp_depth == NULL,
        hccp_err("[get][ra_tsqp_depth]rdev_handle is NULL or func is NULL"), conver_return_code(RDMA_OP, -EINVAL));

    CHK_PRT_RETURN(temp_depth == NULL || qp_num == NULL, hccp_err("[get][ra_tsqp_depth]temp_depth or qp_num is NULL"),
        conver_return_code(RDMA_OP, -EINVAL));

    phy_id = rdma_handle_tmp->rdev_info.phy_id;
    CHK_PRT_RETURN(phy_id >= RA_MAX_PHY_ID_NUM,
        hccp_err("[get][ra_tsqp_depth]phy_id(%u) is invalid! it must greater or equal to 0 and less than %d!",
        phy_id, RA_MAX_PHY_ID_NUM), conver_return_code(RDMA_OP, -EINVAL));

    hccp_info("Input parameters: phy_id[%u], rdev_index[%u]", phy_id, rdma_handle_tmp->rdev_index);

    ret = rdma_handle_tmp->rdma_ops->ra_get_tsqp_depth(rdma_handle_tmp, temp_depth, qp_num);
    return conver_return_code(RDMA_OP, ret);
}

HCCP_ATTRI_VISI_DEF int ra_set_tsqp_depth(void *rdev_handle, unsigned int temp_depth, unsigned int *qp_num)
{
    struct ra_rdma_handle *rdma_handle_tmp = (struct ra_rdma_handle *)rdev_handle;
    unsigned int phy_id;
    int ret;

    CHK_PRT_RETURN(rdev_handle == NULL || rdma_handle_tmp->rdma_ops == NULL ||
        rdma_handle_tmp->rdma_ops->ra_set_tsqp_depth == NULL,
        hccp_err("[set][ra_tsqp_depth]rdev_handle is NULL or func is NULL"), conver_return_code(RDMA_OP, -EINVAL));

    CHK_PRT_RETURN(qp_num == NULL, hccp_err("[set][ra_tsqp_depth]qp_num is NULL"),
        conver_return_code(RDMA_OP, -EINVAL));

    CHK_PRT_RETURN(temp_depth < RA_MIN_TEMPTH_DEPTH || temp_depth > RA_MAX_TEMPTH_DEPTH,
        hccp_err("[set][ra_tsqp_depth]param error! temp_depth(%u) can not smaller than %d or bigger than %d",
        temp_depth, RA_MIN_TEMPTH_DEPTH, RA_MAX_TEMPTH_DEPTH), conver_return_code(RDMA_OP, -EINVAL));

    phy_id = rdma_handle_tmp->rdev_info.phy_id;
    CHK_PRT_RETURN(phy_id >= RA_MAX_PHY_ID_NUM,
        hccp_err("[set][ra_tsqp_depth]phy_id(%u) is invalid! it must greater or equal to 0 and less than %d!",
        phy_id, RA_MAX_PHY_ID_NUM), conver_return_code(RDMA_OP, -EINVAL));

    hccp_run_info("Input parameters: phy_id[%u], rdev_index[%u], temp_depth[%u]",
        phy_id, rdma_handle_tmp->rdev_index, temp_depth);

    ret = rdma_handle_tmp->rdma_ops->ra_set_tsqp_depth(rdma_handle_tmp, temp_depth, qp_num);
    return conver_return_code(RDMA_OP, ret);
}

HCCP_ATTRI_VISI_DEF int ra_qp_create(void *rdev_handle, int flag, int qp_mode, void **qp_handle)
{
    struct ra_rdma_handle *rdma_handle_tmp = (struct ra_rdma_handle *)rdev_handle;
    unsigned int phy_id;
    int ret;

    CHK_PRT_RETURN(rdev_handle == NULL || rdma_handle_tmp->rdma_ops == NULL ||
        rdma_handle_tmp->rdma_ops->ra_qp_create == NULL,
        hccp_err("[create][ra_qp]rdev_handle is NULL or func is NULL"), conver_return_code(RDMA_OP, -EINVAL));

    CHK_PRT_RETURN(qp_handle == NULL, hccp_err("[create][ra_qp]qp_handle is NULL"),
        conver_return_code(RDMA_OP, -EINVAL));

    /* 0 means RC mode */
    CHK_PRT_RETURN(flag != 0, hccp_err("[create][ra_qp]The flag(%d) is invalid, expect 0", flag),
        conver_return_code(RDMA_OP, -EINVAL));

    phy_id = rdma_handle_tmp->rdev_info.phy_id;
    CHK_PRT_RETURN(phy_id >= RA_MAX_PHY_ID_NUM,
        hccp_err("[create][ra_qp]phy_id(%u) must greater or equal to 0 and less than %d!", phy_id, RA_MAX_PHY_ID_NUM),
        conver_return_code(RDMA_OP, -EINVAL));

    CHK_PRT_RETURN(qp_mode < 0 || qp_mode >= RA_RS_ERR_QP_MODE,
        hccp_err("[create][ra_qp]QP mode(%d) must greater or equal to 0 and less than %d", qp_mode, RA_RS_ERR_QP_MODE),
        conver_return_code(RDMA_OP, -EINVAL));

    hccp_run_info("Input parameters: phy_id[%u], flag[%d] qp_mode [%d]", phy_id, flag, qp_mode);

    ret = rdma_handle_tmp->rdma_ops->ra_qp_create(rdma_handle_tmp, flag, qp_mode, qp_handle);
    CHK_PRT_RETURN(ret != 0 || *qp_handle == NULL,
        hccp_err("[create][ra_qp]create qp failed, ret(%d) phy_id(%u)", ret, phy_id),
        conver_return_code(RDMA_OP, ret));

    return 0;
}

HCCP_ATTRI_VISI_DEF int ra_qp_create_with_attrs(void *rdev_handle, struct qp_ext_attrs *ext_attrs, void **qp_handle)
{
    struct ra_rdma_handle *rdma_handle_tmp = (struct ra_rdma_handle *)rdev_handle;
    unsigned int phy_id;
    int ret;

    CHK_PRT_RETURN(rdev_handle == NULL || rdma_handle_tmp->rdma_ops == NULL ||
        rdma_handle_tmp->rdma_ops->ra_qp_create_with_attrs == NULL,
        hccp_err("[create][ra_qp_with_attrs]rdev_handle is NULL or func is NULL"),
        conver_return_code(RDMA_OP, -EINVAL));

    CHK_PRT_RETURN(qp_handle == NULL, hccp_err("[create][ra_qp_with_attrs]qp_handle is NULL"),
        conver_return_code(RDMA_OP, -EINVAL));

    CHK_PRT_RETURN(ext_attrs == NULL, hccp_err("[create][ra_qp_with_attrs]ext_attrs is NULL"),
        conver_return_code(RDMA_OP, -EINVAL));

    CHK_PRT_RETURN(ext_attrs->version != QP_CREATE_WITH_ATTR_VERSION,
        hccp_err("[create][ra_qp_with_attrs]attr version[%d] mismatch, expect [%d]", ext_attrs->version,
        QP_CREATE_WITH_ATTR_VERSION), conver_return_code(RDMA_OP, -EINVAL));

    CHK_PRT_RETURN(ext_attrs->qp_mode < 0 || ext_attrs->qp_mode >= RA_RS_ERR_QP_MODE,
        hccp_err("[create][ra_qp_with_attrs]QP mode[%d] must greater or equal to 0 and less than %d",
        ext_attrs->qp_mode, RA_RS_ERR_QP_MODE), conver_return_code(RDMA_OP, -EINVAL));
    // no need and disallow to set data_plane_flag, set it to default value 0
    ext_attrs->data_plane_flag.value = 0;

    phy_id = rdma_handle_tmp->rdev_info.phy_id;
    CHK_PRT_RETURN(phy_id >= RA_MAX_PHY_ID_NUM,
        hccp_err("[create][ra_qp_with_attrs]phy_id(%u) must greater or equal to 0 and less than %d!", phy_id,
        RA_MAX_PHY_ID_NUM), conver_return_code(RDMA_OP, -EINVAL));

    hccp_run_info("Input parameters: phy_id[%u] qp_mode[%d] cq_attr{%d,%d,%d,%d} qp_attr.cap{%u,%u,%u,%u,%u}"\
        " qp_type[%u] sq_sig_all[%d], cnt[%u]", phy_id, ext_attrs->qp_mode,
        ext_attrs->cq_attr.send_cq_depth, ext_attrs->cq_attr.send_cq_comp_vector,
        ext_attrs->cq_attr.recv_cq_depth, ext_attrs->cq_attr.recv_cq_comp_vector,
        ext_attrs->qp_attr.cap.max_send_wr, ext_attrs->qp_attr.cap.max_recv_wr,
        ext_attrs->qp_attr.cap.max_send_sge, ext_attrs->qp_attr.cap.max_recv_sge,
        ext_attrs->qp_attr.cap.max_inline_data, ext_attrs->qp_attr.qp_type, ext_attrs->qp_attr.sq_sig_all,
        rdma_handle_tmp->qp_cnt);

    rdma_handle_tmp->qp_cnt++;
    ret = rdma_handle_tmp->rdma_ops->ra_qp_create_with_attrs(rdma_handle_tmp, ext_attrs, qp_handle);
    CHK_PRT_RETURN(ret != 0 || *qp_handle == NULL,
        hccp_err("[create][ra_qp_with_attrs]create qp failed, ret(%d) phy_id(%u)", ret, phy_id),
        conver_return_code(RDMA_OP, ret));

    return 0;
}

HCCP_ATTRI_VISI_DEF int ra_ai_qp_create(void *rdev_handle, struct qp_ext_attrs *attrs, struct ai_qp_info *info,
    void **qp_handle)
{
    struct ra_rdma_handle *rdma_handle_tmp = (struct ra_rdma_handle *)rdev_handle;
    unsigned int interface_version = 0;
    unsigned int phy_id;
    int ret;

    CHK_PRT_RETURN(rdev_handle == NULL || rdma_handle_tmp->rdma_ops == NULL ||
        rdma_handle_tmp->rdma_ops->ra_ai_qp_create == NULL ||
        rdma_handle_tmp->rdma_ops->ra_ai_qp_create_with_attrs == NULL,
        hccp_err("[create][ra_ai_qp]rdev_handle is NULL or func is NULL"), conver_return_code(RDMA_OP, -EINVAL));

    CHK_PRT_RETURN(info == NULL || qp_handle == NULL, hccp_err("[create][ra_ai_qp]info is NULL or qp_handle is NULL"),
        conver_return_code(RDMA_OP, -EINVAL));

    CHK_PRT_RETURN(attrs == NULL, hccp_err("[create][ra_ai_qp]attrs is NULL"), conver_return_code(RDMA_OP, -EINVAL));

    CHK_PRT_RETURN(attrs->version != QP_CREATE_WITH_ATTR_VERSION,
        hccp_err("[create][ra_ai_qp]attr version[%d] mismatch, expect [%d]", attrs->version,
        QP_CREATE_WITH_ATTR_VERSION), conver_return_code(RDMA_OP, -EINVAL));

    CHK_PRT_RETURN(attrs->qp_mode < 0 || attrs->qp_mode >= RA_RS_ERR_QP_MODE,
        hccp_err("[create][ra_ai_qp]QP mode[%d] must greater or equal to 0 and less than %d", attrs->qp_mode,
        RA_RS_ERR_QP_MODE), conver_return_code(RDMA_OP, -EINVAL));

    phy_id = rdma_handle_tmp->rdev_info.phy_id;
    CHK_PRT_RETURN(phy_id >= RA_MAX_PHY_ID_NUM,
        hccp_err("[create][ra_ai_qp]phy_id(%u) must greater or equal to 0 and less than %d!", phy_id,
        RA_MAX_PHY_ID_NUM), conver_return_code(RDMA_OP, -EINVAL));

    hccp_run_info("Input parameters: phy_id[%u] qp_mode[%d] cq_attr{%d,%d,%d,%d} cq_cstm[%d] "
        "qp_attr.cap{%u,%u,%u,%u,%u} qp_type[%u] sq_sig_all[%d]", phy_id, attrs->qp_mode,
        attrs->cq_attr.send_cq_depth, attrs->cq_attr.send_cq_comp_vector,
        attrs->cq_attr.recv_cq_depth, attrs->cq_attr.recv_cq_comp_vector, attrs->data_plane_flag.bs.cq_cstm,
        attrs->qp_attr.cap.max_send_wr, attrs->qp_attr.cap.max_recv_wr,
        attrs->qp_attr.cap.max_send_sge, attrs->qp_attr.cap.max_recv_sge,
        attrs->qp_attr.cap.max_inline_data, attrs->qp_attr.qp_type, attrs->qp_attr.sq_sig_all);

    ret = ra_get_interface_version(phy_id, RA_RS_AI_QP_CREATE_WITH_ATTRS, &interface_version);
    if (ret == 0 && interface_version >= RA_RS_OPCODE_BASE_VERSION) {
        ret = rdma_handle_tmp->rdma_ops->ra_ai_qp_create_with_attrs(rdma_handle_tmp, attrs, info, qp_handle);
    } else {
        // origin procedure: not support to process data_plane_flag.bs.cq_cstm
        ret = rdma_handle_tmp->rdma_ops->ra_ai_qp_create(rdma_handle_tmp, attrs, info, qp_handle);
    }
    CHK_PRT_RETURN(ret != 0 || *qp_handle == NULL, hccp_err("[create][ra_ai_qp]create qp failed, ret(%d) phy_id(%u)",
        ret, phy_id), conver_return_code(RDMA_OP, ret));

    return 0;
}

HCCP_ATTRI_VISI_DEF int ra_typical_qp_create(void *rdev_handle, int flag, int qp_mode, struct typical_qp *qp_info,
    void **qp_handle)
{
    struct ra_rdma_handle *rdma_handle_tmp = (struct ra_rdma_handle *)rdev_handle;
    unsigned int phy_id;
    int ret;

    CHK_PRT_RETURN(rdev_handle == NULL || rdma_handle_tmp->rdma_ops == NULL ||
        rdma_handle_tmp->rdma_ops->ra_typical_qp_create == NULL,
        hccp_err("[create][ra_typical_qp]rdev_handle is NULL or func is NULL"), conver_return_code(RDMA_OP, -EINVAL));

    CHK_PRT_RETURN(qp_handle == NULL, hccp_err("[create][ra_typical_qp]qp_handle is NULL"),
        conver_return_code(RDMA_OP, -EINVAL));

    CHK_PRT_RETURN(qp_info == NULL, hccp_err("[create][ra_typical_qp]qp_info is NULL"),
        conver_return_code(RDMA_OP, -EINVAL));

    CHK_PRT_RETURN(flag != 0, hccp_err("[create][ra_typical_qp]The flag(%d) is invalid, expect 0", flag),
        conver_return_code(RDMA_OP, -EINVAL));

    phy_id = rdma_handle_tmp->rdev_info.phy_id;
    CHK_PRT_RETURN(phy_id >= RA_MAX_PHY_ID_NUM,
        hccp_err("[create][ra_typical_qp]phy_id(%u) must greater or equal to 0 and less than %d!", phy_id,
        RA_MAX_PHY_ID_NUM), conver_return_code(RDMA_OP, -EINVAL));

    CHK_PRT_RETURN(qp_mode < 0 || qp_mode >= RA_RS_ERR_QP_MODE,
        hccp_err("[create][ra_typical_qp]QP mode(%d) must greater or equal to 0 and less than %d", qp_mode,
        RA_RS_ERR_QP_MODE), conver_return_code(RDMA_OP, -EINVAL));

    hccp_run_info("Input parameters: phy_id[%u], flag[%d] qp_mode[%d]", phy_id, flag, qp_mode);

    ret = rdma_handle_tmp->rdma_ops->ra_typical_qp_create(rdma_handle_tmp, flag, qp_mode, qp_info, qp_handle);
    CHK_PRT_RETURN(ret != 0 || *qp_handle == NULL,
        hccp_err("[create][ra_typical_qp]create qp failed, ret(%d) phy_id(%u)", ret, phy_id),
        conver_return_code(RDMA_OP, ret));

    return 0;
}

HCCP_ATTRI_VISI_DEF int ra_loopback_qp_create(void *rdev_handle, struct loopback_qp_pair *qp_pair, void **qp_handle)
{
    struct ra_rdma_handle *rdma_handle_tmp = (struct ra_rdma_handle *)rdev_handle;
    unsigned int phy_id;
    int ret;

    CHK_PRT_RETURN(rdma_handle_tmp == NULL || rdma_handle_tmp->rdma_ops == NULL ||
        rdma_handle_tmp->rdma_ops->ra_loopback_qp_create == NULL,
        hccp_err("[create][ra_loopback_qp]rdev_handle is NULL or func is NULL"), conver_return_code(RDMA_OP, -EINVAL));

    CHK_PRT_RETURN(qp_pair == NULL, hccp_err("[create][ra_loopback_qp]qp_pair is NULL"),
        conver_return_code(RDMA_OP, -EINVAL));

    CHK_PRT_RETURN(qp_handle == NULL, hccp_err("[create][ra_loopback_qp]qp_handle is NULL"),
        conver_return_code(RDMA_OP, -EINVAL));

    phy_id = rdma_handle_tmp->rdev_info.phy_id;
    CHK_PRT_RETURN(phy_id >= RA_MAX_PHY_ID_NUM,
        hccp_err("[create][ra_loopback_qp]phy_id(%u) must greater or equal to 0 and less than %d!", phy_id, RA_MAX_PHY_ID_NUM),
        conver_return_code(RDMA_OP, -EINVAL));

    hccp_run_info("Input parameters: phy_id[%u]", phy_id);
    ret = rdma_handle_tmp->rdma_ops->ra_loopback_qp_create(rdma_handle_tmp, qp_pair, qp_handle);
    CHK_PRT_RETURN(ret != 0 || *qp_handle == NULL,
        hccp_err("[create][ra_loopback_qp]create qp failed, ret(%d) phy_id(%u)", ret, phy_id),
        conver_return_code(RDMA_OP, ret));

    return 0;
}

HCCP_ATTRI_VISI_DEF int ra_qp_destroy(void *qp_handle)
{
    struct ra_qp_handle *ra_qp_handle = (struct ra_qp_handle *)qp_handle;
    int ret;

    CHK_PRT_RETURN(qp_handle == NULL,
        hccp_err("[destroy][ra_qp]qp_handle is NULL"),
        conver_return_code(RDMA_OP, -EINVAL));

    CHK_PRT_RETURN(ra_qp_handle->rdma_ops == NULL || ra_qp_handle->rdma_ops->ra_qp_destroy == NULL,
        hccp_err("[destroy][ra_qp]rdma_ops is NULL or ra_qp_handle->rdma_ops->ra_qp_destroy is NULL, invalid"),
        conver_return_code(RDMA_OP, -EINVAL));

    hccp_run_info("Input parameters: qpn[%u], phy_id[%u], rdev_index[%u] qp_mode[%d] flag[%d]",
        ra_qp_handle->qpn, ra_qp_handle->phy_id, ra_qp_handle->rdev_index, ra_qp_handle->qp_mode, ra_qp_handle->flag);

    ret = ra_qp_handle->rdma_ops->ra_qp_destroy(ra_qp_handle);
    return conver_return_code(RDMA_OP, ret);
}

HCCP_ATTRI_VISI_DEF int ra_qp_connect_async(void *qp_handle, const void *fd_handle)
{
    struct ra_qp_handle *ra_qp_handle = (struct ra_qp_handle *)qp_handle;
    int ret;

    CHK_PRT_RETURN(qp_handle == NULL || fd_handle == NULL,
        hccp_err("[connect_async][ra_qp]ra_qp_handle or fd_handle is NULL, para error!"),
        conver_return_code(RDMA_OP, -EINVAL));

    CHK_PRT_RETURN(ra_qp_handle->rdma_ops == NULL || ra_qp_handle->rdma_ops->ra_qp_connect_async == NULL,
        hccp_err("[connect_async][ra_qp]rdma_ops or ra_qp_handle->rdma_ops->ra_qp_connect_async is NULL, invalid"),
        conver_return_code(RDMA_OP, -EINVAL));

    ret = ra_qp_handle->rdma_ops->ra_qp_connect_async(ra_qp_handle, fd_handle);
    return conver_return_code(RDMA_OP, ret);
}

HCCP_ATTRI_VISI_DEF int ra_get_qp_status(void *qp_handle, int *status)
{
    struct ra_qp_handle *ra_qp_handle = (struct ra_qp_handle *)qp_handle;
    int ret;

    CHK_PRT_RETURN(qp_handle == NULL || status == NULL,
        hccp_err("[get][ra_qp_status]ra_qp_handle or status is NULL, para error!"),
        conver_return_code(RDMA_OP, -EINVAL));

    CHK_PRT_RETURN(ra_qp_handle->rdma_ops == NULL || ra_qp_handle->rdma_ops->ra_get_qp_status == NULL,
        hccp_err("[get][ra_qp_status]rdma_ops is NULL or ra_qp_handle->rdma_ops->ra_get_qp_status is NULL, invalid"),
        conver_return_code(RDMA_OP, -EINVAL));

    ret = ra_qp_handle->rdma_ops->ra_get_qp_status(ra_qp_handle, status);
    return conver_return_code(RDMA_OP, ret);
}

HCCP_ATTRI_VISI_DEF int ra_mr_reg(void *qp_handle, struct mr_info *info)
{
    struct ra_qp_handle *ra_qp_handle = (struct ra_qp_handle *)qp_handle;
    int ret;

    CHK_PRT_RETURN(qp_handle == NULL || info == NULL,
        hccp_err("[reg][ra_mr]qp_handle is NULL or info is NULL, para error!"),
        conver_return_code(RDMA_OP, -EINVAL));

    CHK_PRT_RETURN(ra_qp_handle->rdma_ops == NULL || ra_qp_handle->rdma_ops->ra_mr_reg == NULL,
        hccp_err("[reg][ra_mr]rdma_ops is NULL or ra_qp_handle->rdma_ops->ra_mr_reg is NULL, invalid"),
        conver_return_code(RDMA_OP, -EINVAL));

    ret = ra_qp_handle->rdma_ops->ra_mr_reg(ra_qp_handle, info);
    return conver_return_code(RDMA_OP, ret);
}

HCCP_ATTRI_VISI_DEF int ra_mr_dereg(void *qp_handle, struct mr_info *info)
{
    struct ra_qp_handle *ra_qp_handle = (struct ra_qp_handle *)qp_handle;
    int ret;

    CHK_PRT_RETURN(qp_handle == NULL || info == NULL || info->addr == NULL,
        hccp_err("[dereg][ra_mr]qp_handle or info or addr is NULL, para error!"),
        conver_return_code(RDMA_OP, -EINVAL));

    CHK_PRT_RETURN(ra_qp_handle->rdma_ops == NULL || ra_qp_handle->rdma_ops->ra_mr_dereg == NULL,
        hccp_err("[dereg][ra_mr]rdma_ops is NULL or ra_qp_handle->rdma_ops->ra_mr_dereg is NULL, invalid"),
        conver_return_code(RDMA_OP, -EINVAL));

    ret = ra_qp_handle->rdma_ops->ra_mr_dereg(ra_qp_handle, info);
    return conver_return_code(RDMA_OP, ret);
}

HCCP_ATTRI_VISI_DEF int ra_send_wr(void *qp_handle, struct send_wr *wr, struct send_wr_rsp *op_rsp)
{
    struct ra_qp_handle *ra_qp_handle = (struct ra_qp_handle *)qp_handle;
    int ret;

    CHK_PRT_RETURN(qp_handle == NULL || wr == NULL || wr->buf_list == NULL || op_rsp == NULL,
        hccp_err("[send][ra_wr]qp_handle or wr or buf_list or op_rsp is NULL, para error!"),
        conver_return_code(RDMA_OP, -EINVAL));

    CHK_PRT_RETURN(wr->buf_list->len > MAX_SG_LIST_LEN_MAX,
        hccp_err("[send][ra_wr]sg list len is more than 2G, len(%u)", wr->buf_list->len),
        conver_return_code(RDMA_OP, -EINVAL));

    CHK_PRT_RETURN(ra_qp_handle->rdma_ops == NULL || ra_qp_handle->rdma_ops->ra_send_wr == NULL,
        hccp_err("[send][ra_wr]rdma_ops is NULL or ra_qp_handle->rdma_ops->ra_send_wr is NULL, invalid"),
        conver_return_code(RDMA_OP, -EINVAL));

    ret = ra_qp_handle->rdma_ops->ra_send_wr(ra_qp_handle, wr, op_rsp);
    ra_rdev_inc_send_wr_num();
    return conver_return_code(RDMA_OP, ret);
}

HCCP_ATTRI_VISI_DEF int ra_send_wr_v2(void *qp_handle, struct send_wr_v2 *wr, struct send_wr_rsp *op_rsp)
{
    struct ra_qp_handle *ra_qp_handle = (struct ra_qp_handle *)qp_handle;
    int ret;

    CHK_PRT_RETURN(qp_handle == NULL || wr == NULL || wr->buf_list == NULL || op_rsp == NULL,
        hccp_err("[send][ra_wr]qp_handle or wr or buf_list or op_rsp is NULL, para error!"),
        conver_return_code(RDMA_OP, -EINVAL));

    CHK_PRT_RETURN(wr->buf_list->len > MAX_SG_LIST_LEN_MAX,
        hccp_err("[send][ra_wr]sg list len is more than 2G, len(%u)", wr->buf_list->len),
        conver_return_code(RDMA_OP, -EINVAL));

    CHK_PRT_RETURN(ra_qp_handle->rdma_ops == NULL || ra_qp_handle->rdma_ops->ra_send_wr_v2 == NULL,
        hccp_err("[send][ra_wr]rdma_ops is NULL or ra_qp_handle->rdma_ops->ra_send_wr_v2 is NULL, invalid"),
        conver_return_code(RDMA_OP, -EINVAL));

    ret = ra_qp_handle->rdma_ops->ra_send_wr_v2(ra_qp_handle, wr, op_rsp);
    ra_rdev_inc_send_wr_num();
    return conver_return_code(RDMA_OP, ret);
}

HCCP_ATTRI_VISI_DEF int ra_send_wrlist(void *qp_handle, struct send_wrlist_data wr[], struct send_wr_rsp op_rsp[],
    unsigned int send_num, unsigned int *complete_num)
{
    int ret;
    unsigned int i;
    struct ra_qp_handle *ra_qp_handle = (struct ra_qp_handle *)qp_handle;
    struct wrlist_send_complete_num wrlist_num = {0};

    CHK_PRT_RETURN(qp_handle == NULL || wr == NULL || op_rsp == NULL || send_num == 0 || complete_num == NULL,
        hccp_err("[send][ra_wrlist]qp_handle or wr or op_rsp or complete_num is NULL or send_num is zero, para error!"),
        conver_return_code(RDMA_OP, -EINVAL));

    for (i = 0; i < send_num; i++) {
        if (wr[i].mem_list.len > MAX_SG_LIST_LEN_MAX) {
            hccp_err("[send][ra_wrlist]sg list len is more than 2G, len(%u)", wr[i].mem_list.len);
            return conver_return_code(RDMA_OP, -EINVAL);
        }
    }

    CHK_PRT_RETURN(ra_qp_handle->rdma_ops == NULL || ra_qp_handle->rdma_ops->ra_send_wrlist == NULL,
        hccp_err("[send][ra_wrlist]rdma_ops is NULL or ra_qp_handle->rdma_ops->ra_send_wr is NULL, invalid"),
        conver_return_code(RDMA_OP, -EINVAL));

    wrlist_num.send_num = send_num;
    wrlist_num.complete_num = complete_num;
    ret = ra_qp_handle->rdma_ops->ra_send_wrlist(ra_qp_handle, wr, op_rsp, wrlist_num);
    return conver_return_code(RDMA_OP, ret);
}

HCCP_ATTRI_VISI_DEF int ra_send_wrlist_ext(void *qp_handle, struct send_wrlist_data_ext wr[],
    struct send_wr_rsp op_rsp[], unsigned int send_num, unsigned int *complete_num)
{
    struct ra_qp_handle *ra_qp_handle = (struct ra_qp_handle *)qp_handle;
    struct wrlist_send_complete_num wrlist_num = {0};
    unsigned int i;
    int ret;

    CHK_PRT_RETURN(qp_handle == NULL || wr == NULL || op_rsp == NULL || send_num == 0 || complete_num == NULL,
        hccp_err("[send][ra_wrlist]qp_handle or wr or op_rsp or complete_num is NULL"\
            "or send_num is zero, para error!"),
        conver_return_code(RDMA_OP, -EINVAL));

    for (i = 0; i < send_num; i++) {
        if (wr[i].mem_list.len > MAX_SG_LIST_LEN_MAX) {
            hccp_err("[send][ra_wrlist]sg list len is more than 2G, len(%u)", wr[i].mem_list.len);
            return conver_return_code(RDMA_OP, -EINVAL);
        }
    }

    CHK_PRT_RETURN(ra_qp_handle->rdma_ops == NULL || ra_qp_handle->rdma_ops->ra_send_wrlist_ext == NULL,
        hccp_err("[send][ra_wrlist]rdma_ops is NULL or ra_qp_handle->rdma_ops->ra_send_wr is NULL, invalid"),
        conver_return_code(RDMA_OP, -EINVAL));

    wrlist_num.send_num = send_num;
    wrlist_num.complete_num = complete_num;
    ret = ra_qp_handle->rdma_ops->ra_send_wrlist_ext(ra_qp_handle, wr, op_rsp, wrlist_num);
    return conver_return_code(RDMA_OP, ret);
}

HCCP_ATTRI_VISI_DEF int ra_get_notify_base_addr(void *rdev_handle, unsigned long long *va, unsigned long long *size)
{
    struct ra_rdma_handle *rdev_handle_tmp = (struct ra_rdma_handle *)rdev_handle;
    int ret;

    CHK_PRT_RETURN(rdev_handle == NULL || va == NULL || size == NULL,
        hccp_err("[get][ra_notify_base_addr]rdev_handle or va or size is NULL, invalid"),
        conver_return_code(RDMA_OP, -EINVAL));

    CHK_PRT_RETURN(rdev_handle_tmp->rdma_ops == NULL || rdev_handle_tmp->rdma_ops->ra_get_notify_base_addr == NULL,
        hccp_err("[get][ra_notify_base_addr]rdma_ops is NULL or ra_get_notify_base_addr is NULL, invalid"),
        conver_return_code(RDMA_OP, -EINVAL));

    ret = rdev_handle_tmp->rdma_ops->ra_get_notify_base_addr(rdev_handle_tmp, va, size);
    ra_rdev_save_notify_mr(rdev_handle_tmp, ret, *va, *size);
    return conver_return_code(RDMA_OP, ret);
}

HCCP_ATTRI_VISI_DEF int ra_get_notify_mr_info(void *rdev_handle, struct mr_info *info)
{
    struct ra_rdma_handle *rdev_handle_tmp = (struct ra_rdma_handle *)rdev_handle;
    int ret;

    CHK_PRT_RETURN(rdev_handle == NULL || info == NULL,
        hccp_err("[get][ra_notify_mr_info]rdev_handle or info is NULL, invalid"),
        conver_return_code(RDMA_OP, -EINVAL));

    CHK_PRT_RETURN(rdev_handle_tmp->rdma_ops == NULL || rdev_handle_tmp->rdma_ops->ra_get_notify_mr_info == NULL,
        hccp_err("[get][ra_notify_mr_info]rdma_ops is NULL or ra_get_notify_mr_info is NULL, invalid"),
        conver_return_code(RDMA_OP, -EINVAL));

    ret = rdev_handle_tmp->rdma_ops->ra_get_notify_mr_info(rdev_handle_tmp, info);
    ra_rdev_save_notify_mr(rdev_handle_tmp, ret, (uint64_t)(uintptr_t)info->addr, info->size);
    return conver_return_code(RDMA_OP, ret);
}

HCCP_ATTRI_VISI_DEF int ra_socket_get_white_list_status(unsigned int *enable)
{
    CHK_PRT_RETURN(enable == NULL, hccp_err("[get][ra_socket_white_list_status]white list switch enable is NULL"),
        conver_return_code(SOCKET_OP, -EINVAL));

    *enable = g_white_list_switch;
    hccp_info("white list status: enable[%u]", *enable);
    return 0;
}

HCCP_ATTRI_VISI_DEF int ra_socket_set_white_list_status(unsigned int enable)
{
    hccp_run_info("Input parameters: enable[%u]", enable);

    CHK_PRT_RETURN(enable != WHITE_LIST_DISABLE && enable != WHITE_LIST_ENABLE,
        hccp_err("[set][ra_socket_white_list_status]white list switch is invalid, enable(%u)", enable),
        conver_return_code(SOCKET_OP, -EINVAL));

    g_white_list_switch = enable;
    return 0;
}

HCCP_ATTRI_VISI_DEF int ra_socket_white_list_add(void *socket_handle, struct socket_wlist_info_t white_list[],
    unsigned int num)
{
    struct ra_socket_handle *socket_handle_tmp = NULL;
    char local_ip[MAX_IP_LEN] = {0};
    unsigned int phy_id;
    int ret;

    CHK_PRT_RETURN(white_list == NULL || num > MAX_WLIST_NUM || num == 0,
        hccp_err("[add][ra_socket_white_list]white_list is NULL, or num(%u) > %u or = 0, invalid",
            num, MAX_WLIST_NUM),
        conver_return_code(SOCKET_OP, -EINVAL));

    socket_handle_tmp = (struct ra_socket_handle *)socket_handle;
    if (socket_handle_tmp == NULL || socket_handle_tmp->socket_ops == NULL ||
        socket_handle_tmp->socket_ops->ra_socket_white_list_add == NULL) {
        hccp_err("[add][ra_socket_white_list]socket_handle or func is NULL");
        return conver_return_code(SOCKET_OP, -EINVAL);
    }

    ret = ra_inet_pton(socket_handle_tmp->rdev_info.family, socket_handle_tmp->rdev_info.local_ip,
                       local_ip, MAX_IP_LEN);
    CHK_PRT_RETURN(ret, hccp_err("[add][ra_socket_white_list]ra_inet_pton for local_ip failed, ret(%d)", ret),
        conver_return_code(SOCKET_OP, ret));

    phy_id = socket_handle_tmp->rdev_info.phy_id;
    CHK_PRT_RETURN(phy_id >= RA_MAX_PHY_ID_NUM,
        hccp_err("[add][ra_socket_white_list]phy_id(%u) must smaller than %u", phy_id, RA_MAX_PHY_ID_NUM),
        conver_return_code(SOCKET_OP, -EINVAL));

    hccp_info("Input parameters: phy_id[%u], local_ip[%s], num[%u]", phy_id, local_ip, num);

    ret = socket_handle_tmp->socket_ops->ra_socket_white_list_add(socket_handle_tmp->rdev_info, white_list, num);
    return conver_return_code(SOCKET_OP, ret);
}

HCCP_ATTRI_VISI_DEF int ra_socket_white_list_del(void *socket_handle, struct socket_wlist_info_t white_list[],
    unsigned int num)
{
    struct ra_socket_handle *socket_handle_tmp = NULL;
    char local_ip[MAX_IP_LEN] = {0};
    unsigned int phy_id;
    int ret;

    if (white_list == NULL || num > MAX_WLIST_NUM || num == 0) {
        hccp_err("[del][ra_socket_white_list]white_list is NULL, or num (%u) > %u or = 0, invalid", num, MAX_WLIST_NUM);
        return conver_return_code(SOCKET_OP, -EINVAL);
    }
    socket_handle_tmp = (struct ra_socket_handle *)socket_handle;
    if (socket_handle_tmp == NULL || socket_handle_tmp->socket_ops == NULL ||
        socket_handle_tmp->socket_ops->ra_socket_white_list_del == NULL) {
        hccp_err("[del][ra_socket_white_list]socket_handle or func is NULL");
        return conver_return_code(SOCKET_OP, -EINVAL);
    }
    ret = ra_inet_pton(socket_handle_tmp->rdev_info.family, socket_handle_tmp->rdev_info.local_ip, local_ip,
        MAX_IP_LEN);
    if (ret) {
        hccp_err("[del][ra_socket_white_list]ra_inet_pton for local_ip failed, ret(%d)", ret);
        return conver_return_code(SOCKET_OP, ret);
    }

    phy_id = socket_handle_tmp->rdev_info.phy_id;
    if (phy_id >= RA_MAX_PHY_ID_NUM) {
        hccp_err("[del][ra_socket_white_list]phy_id(%u) must smaller than %u", phy_id, RA_MAX_PHY_ID_NUM);
        return conver_return_code(SOCKET_OP, -EINVAL);
    }

    hccp_info("Input parameters: phy_id[%u], local_ip[%s], num[%u]", phy_id, local_ip, num);

    ret = socket_handle_tmp->socket_ops->ra_socket_white_list_del(socket_handle_tmp->rdev_info, white_list, num);
    return conver_return_code(SOCKET_OP, ret);
}

STATIC int ra_ifaddr_info_converter(unsigned int phy_id, bool is_all, struct interface_info interface_infos[],
    unsigned int *num)
{
    struct ifaddr_info *ifaddr_infos = NULL;
    unsigned int interface_version_v2 = 0;
    unsigned int interface_version = 0;
    unsigned int i;
    int ret;

    ret = ra_get_interface_version(phy_id, RA_RS_GET_IFADDRS, &interface_version);
    CHK_PRT_RETURN(ret != 0 || interface_version == 0,
        hccp_err("[converter][ra_ifaddr]get interface version failed, ret(%d), phy_id(%u), interface_version(%u)",
            ret, phy_id, interface_version), -EINVAL);

    ret = ra_get_interface_version(phy_id, RA_RS_GET_IFADDRS_V2, &interface_version_v2);
    CHK_PRT_RETURN(ret != 0,
        hccp_err("[converter][ra_ifaddr]get interface version failed, ret(%d), phy_id(%u), interface_version(%u)",
            ret, phy_id, interface_version), -EINVAL);

    CHK_PRT_RETURN(interface_version_v2 < GET_IFADDRS_VERSION_3 && is_all,
        hccp_err("[converter][ra_ifaddr]current version do not support get all device ip addr, interface_version(%u), "
            "interface_version_v2(%u), is_all(%d)", interface_version, interface_version_v2, is_all), -EPROTONOSUPPORT);

    if (interface_version == GET_IFADDRS_VERSION_1) {
        ifaddr_infos = calloc(*num, sizeof(struct ifaddr_info));
        if (ifaddr_infos == NULL) {
            hccp_err("[converter][ra_ifaddr]calloc for ifaddr_infos failed");
            return -EINVAL;
        }

        ret = ra_hdc_get_ifaddrs(phy_id, ifaddr_infos, num);
        if (ret) {
            hccp_err("[converter][ra_ifaddr]ra_hdc_get_ifaddrs failed, ret(%d), phy_id(%u), num(%u)",
                ret, phy_id, *num);
            free(ifaddr_infos);
            return ret;
        }

        for (i = 0; i < *num; i++) {
            // device 网卡只支持IPv4
            interface_infos[i].family = AF_INET;
            interface_infos[i].scope_id = 0;
            ret = memcpy_s(&(interface_infos[i].ifaddr.ip), sizeof(union hccp_ip_addr), &(ifaddr_infos[i].ip),
                sizeof(union hccp_ip_addr));
            if (ret) {
                hccp_err("[converter][ra_ifaddr]memcpy_s interface[%u] ip failed, ret(%d)", i, ret);
                free(ifaddr_infos);
                return -ESAFEFUNC;
            }

            ret = memcpy_s(&(interface_infos[i].ifaddr.mask), sizeof(struct in_addr), &(ifaddr_infos[i].mask),
                sizeof(struct in_addr));
            if (ret) {
                hccp_err("[converter][ra_ifaddr]memcpy_s interface[%u] mask failed, ret(%d)", i, ret);
                free(ifaddr_infos);
                return -ESAFEFUNC;
            }
        }

        free(ifaddr_infos);
    } else if (interface_version == GET_IFADDRS_VERSION_2) { /* version 2 support IPV6 and IPV4 */
        ret = ra_hdc_get_ifaddrs_v2(phy_id, is_all, interface_infos, num);
        CHK_PRT_RETURN(ret,
            hccp_err("[converter][ra_ifaddr]ra_hdc_get_ifaddrs_v2 failed, ret(%d), phy_id(%u), is_all(%d), num(%u)",
                ret, phy_id, is_all, *num), ret);
    } else {
        hccp_err("[converter][ra_ifaddr]interface version not support, interface_version(%u)", interface_version);
        return -EINVAL;
    }

    return 0;
}

HCCP_ATTRI_VISI_DEF int ra_get_ifnum(struct ra_get_ifattr *config, unsigned int *num)
{
    unsigned int interface_version = 0;
    int ret;

    CHK_PRT_RETURN(config == NULL || num == NULL, hccp_err("[get][ra_ifnum]config or num is NULL"),
        conver_return_code(OTHERS, -EINVAL));

    CHK_PRT_RETURN(config->phy_id >= RA_MAX_PHY_ID_NUM,
        hccp_err("[get][ra_ifnum]phy_id(%u) is invalid! it must greater or equal to 0 and less than %d!",
        config->phy_id, RA_MAX_PHY_ID_NUM),
        conver_return_code(OTHERS, -EINVAL));

    hccp_run_info("Input parameters: phy_id[%u], nic_position:[%u], is_all:[%d]",
        config->phy_id, config->nic_position, config->is_all);

    ret = ra_get_interface_version(config->phy_id, RA_RS_GET_IFADDRS_V2, &interface_version);
    CHK_PRT_RETURN(ret != 0,
        hccp_err("[get][ra_ifnum]get interface version failed, ret(%d), phy_id(%u), interface_version(%u)", ret,
        config->phy_id, interface_version),
        conver_return_code(OTHERS, -EINVAL));

    CHK_PRT_RETURN(interface_version < GET_IFADDRS_VERSION_3 && config->nic_position == NETWORK_OFFLINE &&
        config->is_all,
        hccp_err("[get][ra_ifnum]current version do not support get all ip num, interface_version(%u), is_all(%d)",
        interface_version, config->is_all),
        conver_return_code(OTHERS, -ENOTSUPP));

    if (config->nic_position == NETWORK_OFFLINE) {
        ret = ra_hdc_get_ifnum(config->phy_id, config->is_all, num);
        CHK_PRT_RETURN(ret, hccp_err("[get][ra_ifnum]ra_hdc_get_ifnum failed, ret(%d)", ret),
            conver_return_code(OTHERS, ret));
    } else if (config->nic_position == NETWORK_PEER_ONLINE) {
        ret = ra_peer_get_ifnum(config->phy_id, num);
        CHK_PRT_RETURN(ret, hccp_err("[get][ra_ifnum]ra_peer_get_ifnum failed, ret(%d)", ret),
            conver_return_code(OTHERS, ret));
    } else {
        hccp_err("[get][ra_ifnum]Wrong mode, do not support online mode");
        return conver_return_code(OTHERS, -EPROTONOSUPPORT);
    }

    CHK_PRT_RETURN((*num) > MAX_SUPPORT_IFNUM,
        hccp_err("[get][ra_ifnum]get interface num(%d)! It must greater or equal to 0 and less or equal to %d", (*num),
        MAX_SUPPORT_IFNUM), conver_return_code(OTHERS, -EINVAL));

    return 0;
}

HCCP_ATTRI_VISI_DEF int ra_get_ifaddrs(struct ra_get_ifattr *config, struct interface_info interface_infos[],
    unsigned int *num)
{
    int ret;

    CHK_PRT_RETURN(config == NULL || interface_infos == NULL || num == NULL,
        hccp_err("[get][ra_ifaddrs]config or interface_infos or num is NULL"), conver_return_code(OTHERS, -EINVAL));

    CHK_PRT_RETURN(config->phy_id >= RA_MAX_PHY_ID_NUM,
        hccp_err("[get][ra_ifaddrs]phy_id(%u) is invalid! it must greater or equal to 0 and less than %d!",
        config->phy_id, RA_MAX_PHY_ID_NUM), conver_return_code(OTHERS, -EINVAL));

    CHK_PRT_RETURN(*num == 0, hccp_err("[get][ra_ifaddrs]interface num(%u) is invalid! it must greater than 0", *num),
        conver_return_code(OTHERS, -EINVAL));

    hccp_run_info("Input parameters: phy_id[%u], nic_position:[%u], is_all[%d], interface num[%u]",
        config->phy_id, config->nic_position, config->is_all, *num);

    if (config->nic_position == NETWORK_OFFLINE) {
        CHK_PRT_RETURN(*num > MAX_INTERFACE_NUM,
            hccp_err("[get][ra_ifaddrs]interface num(%u) is invalid! it must be less than %d!", *num, MAX_INTERFACE_NUM),
            conver_return_code(OTHERS, -EINVAL));

        ret = ra_ifaddr_info_converter(config->phy_id, config->is_all, interface_infos, num);
        CHK_PRT_RETURN(ret, hccp_err("[get][ra_ifaddrs]ra_hdc_get_ifaddrs failed, ret(%d)", ret),
            conver_return_code(OTHERS, ret));
    } else if (config->nic_position == NETWORK_PEER_ONLINE) {
        ret = ra_peer_get_ifaddrs(config->phy_id, interface_infos, num);
        CHK_PRT_RETURN(ret, hccp_err("[get][ra_ifaddrs]ra_peer_get_ifaddrs failed, ret(%d)", ret),
            conver_return_code(OTHERS, ret));
    } else {
        hccp_err("[get][ra_ifaddrs]Wrong mode, do not support online mode");
        return conver_return_code(OTHERS, -EPROTONOSUPPORT);
    }
    return 0;
}

HCCP_ATTRI_VISI_DEF int ra_get_interface_version(unsigned int phy_id, unsigned int interface_opcode,
    unsigned int* interface_version)
{
    int ret;

    if (interface_version == NULL || phy_id >= RA_MAX_PHY_ID_NUM || interface_opcode >= RA_RS_EXTER_OP_MAX_NUM) {
        hccp_err("[get][ra_interface_version]para is invalid! interface_version is NULL or phy_id(%u) is"\
            "greater than [%u] or interface_opcode(%u) more than [%u]", phy_id, RA_MAX_PHY_ID_NUM, interface_opcode,
            RA_RS_EXTER_OP_MAX_NUM);
        return conver_return_code(OTHERS, -EINVAL);
    }

    ret = ra_hdc_get_interface_version(phy_id, interface_opcode, interface_version);
    return conver_return_code(OTHERS, ret);
}

int conver_return_code(enum module_type module, int erro_code)
{
    unsigned int i;
    unsigned int num = sizeof(g_errcode_info_list) / sizeof(g_errcode_info_list[0]);
    int ret = CONVER_ERROR_CODE(module, DEFAULT_ERRCODE_TYPE, DEFAULT_MODULE_ERRCODE);

    if (erro_code == 0) {
        return 0;
    }

    if (erro_code / ACL_ERRCODE_DIGIT) {        /* ACL error codes are transparently transmitted */
        return erro_code;
    }

    for (i = 0; i < num; i++) {
        if (erro_code == g_errcode_info_list[i].orig_errcode) {
            ret = CONVER_ERROR_CODE(module, g_errcode_info_list[i].err_type, g_errcode_info_list[i].module_errcode);
            break;
        }
    }

    if (erro_code != -EAGAIN) {  // 防止刷屏
        hccp_info("conver_return_code: orig_errcode[%d] curr_errcode[%d]", erro_code, ret);
    }
    return ret;
}

HCCP_ATTRI_VISI_DEF int ra_recv_wrlist(void *qp_handle, struct recv_wrlist_data *wr, unsigned int recv_num,
    unsigned int *complete_num)
{
    int ret;
    unsigned int i;
    struct ra_qp_handle *ra_qp_handle = (struct ra_qp_handle *)qp_handle;

    if (qp_handle == NULL || wr == NULL || recv_num == 0 || complete_num == NULL) {
        hccp_err("[recv][ra_wrlist]qp_handle or wr or complete_num is NULL or recv_num[%u] is zero, para error!",
            recv_num);
        return conver_return_code(RDMA_OP, -EINVAL);
    }

    for (i = 0; i < recv_num; i++) {
        if (wr[i].mem_list.len > MAX_SG_LIST_LEN_MAX) {
            hccp_err("[recv][ra_wrlist]sg list len is more than 2G, len(%u)", wr[i].mem_list.len);
            return conver_return_code(RDMA_OP, -EINVAL);
        }
    }
    if (ra_qp_handle->rdma_ops == NULL || ra_qp_handle->rdma_ops->ra_recv_wrlist == NULL) {
        hccp_err("[recv][ra_wrlist]rdma_ops or ra_recv_wrlist is NULL, invalid");
        return conver_return_code(RDMA_OP, -EINVAL);
    }
    ret = ra_qp_handle->rdma_ops->ra_recv_wrlist(ra_qp_handle, wr, recv_num, complete_num);
    return conver_return_code(RDMA_OP, ret);
}

HCCP_ATTRI_VISI_DEF int ra_get_qp_context(void *qp_handle, void** qp, void** send_cq, void** recv_cq)
{
    int ret;
    struct ra_qp_handle *ra_qp_handle = (struct ra_qp_handle *)qp_handle;

    if (qp_handle == NULL) {
        hccp_err("[request][ra_get_qp_context]qp_handle is NULL, para error!");
        return conver_return_code(RDMA_OP, -EINVAL);
    }

    if (ra_qp_handle->rdma_ops == NULL || ra_qp_handle->rdma_ops->ra_get_qp_context == NULL) {
        hccp_err("[get][ra_get_qp_context]rdma_ops is NULL or ra_get_qp_context is NULL, invalid");
        return conver_return_code(RDMA_OP, -EINVAL);
    }

    ret = ra_qp_handle->rdma_ops->ra_get_qp_context(ra_qp_handle, qp, send_cq, recv_cq);
    return conver_return_code(RDMA_OP, ret);
}

HCCP_ATTRI_VISI_DEF int ra_cq_create(void *rdev_handle, struct cq_attr *attr)
{
    struct ra_rdma_handle *rdma_handle_tmp = (struct ra_rdma_handle *)rdev_handle;
    unsigned int phy_id;
    int ret;

    CHK_PRT_RETURN(rdev_handle == NULL || rdma_handle_tmp->rdma_ops == NULL ||
        rdma_handle_tmp->rdma_ops->ra_cq_create == NULL || attr == NULL || attr->ib_send_cq == NULL ||
        attr->ib_recv_cq == NULL || attr->qp_context == NULL, hccp_err("[create][ra_cq]para is NULL or func is NULL"),
        conver_return_code(RDMA_OP, -EINVAL));

    phy_id = rdma_handle_tmp->rdev_info.phy_id;
    CHK_PRT_RETURN(phy_id >= RA_MAX_PHY_ID_NUM,
        hccp_err("[create][ra_cq]phy_id(%u) must less than %d!", phy_id, RA_MAX_PHY_ID_NUM),
        conver_return_code(RDMA_OP, -EINVAL));

    hccp_run_info("Input parameters: phy_id[%u]", phy_id);

    ret = rdma_handle_tmp->rdma_ops->ra_cq_create(rdma_handle_tmp, attr);
    CHK_PRT_RETURN(ret != 0 || *attr->ib_send_cq == NULL || *attr->ib_recv_cq == NULL || *attr->qp_context == NULL,
        hccp_err("[create][ra_cq]create cp failed, ret(%d) phy_id(%u)", ret, phy_id),
        conver_return_code(RDMA_OP, -EINVAL));

    return 0;
}

HCCP_ATTRI_VISI_DEF int ra_cq_destroy(void *rdev_handle, struct cq_attr *attr)
{
    struct ra_rdma_handle *rdma_handle_tmp = (struct ra_rdma_handle *)rdev_handle;
    unsigned int phy_id;
    int ret;

    CHK_PRT_RETURN(rdev_handle == NULL || rdma_handle_tmp->rdma_ops == NULL || 
        rdma_handle_tmp->rdma_ops->ra_cq_destroy == NULL || attr == NULL || attr->ib_send_cq == NULL ||
        attr->ib_recv_cq == NULL || attr->qp_context == NULL, hccp_err("[destroy][ra_cq]para is NULL or func is NULL"),
        conver_return_code(RDMA_OP, -EINVAL));

    phy_id = rdma_handle_tmp->rdev_info.phy_id;
    CHK_PRT_RETURN(phy_id >= RA_MAX_PHY_ID_NUM,
        hccp_err("[destroy][ra_cq]phy_id(%u) must less than %d!", phy_id, RA_MAX_PHY_ID_NUM),
        conver_return_code(RDMA_OP, -EINVAL));

    hccp_run_info("Input parameters: phy_id[%u]", phy_id);

    ret = rdma_handle_tmp->rdma_ops->ra_cq_destroy(rdma_handle_tmp, attr);
    CHK_PRT_RETURN(ret != 0 || *attr->ib_send_cq == NULL || *attr->ib_recv_cq == NULL ||
                   *attr->qp_context == NULL,
        hccp_err("[destroy][ra_cq]destroy cp failed, ret(%d) phy_id(%u)", ret, phy_id),
        conver_return_code(RDMA_OP, -EINVAL));

    return 0;
}

HCCP_ATTRI_VISI_DEF int ra_normal_qp_create(void *rdev_handle, struct ibv_qp_init_attr *qp_init_attr, void **qp_handle,
    void **qp)
{
    struct ra_rdma_handle *rdma_handle_tmp = (struct ra_rdma_handle *)rdev_handle;
    unsigned int phy_id;
    int ret;

    if (rdev_handle == NULL || rdma_handle_tmp->rdma_ops == NULL ||
        rdma_handle_tmp->rdma_ops->ra_normal_qp_create == NULL || qp_init_attr == NULL || qp == NULL) {
        hccp_err("[create][ra_normal_qp]para is NULL or func is NULL");
        return conver_return_code(RDMA_OP, -EINVAL);
    }

    if (qp_handle == NULL) {
        hccp_err("[create][ra_normal_qp]qp_handle is NULL");
        return conver_return_code(RDMA_OP, -EINVAL);
    }

    phy_id = rdma_handle_tmp->rdev_info.phy_id;
    if (phy_id >= RA_MAX_PHY_ID_NUM) {
        hccp_err("[create][ra_normal_qp]phy_id(%u) must less than %d!", phy_id, RA_MAX_PHY_ID_NUM);
        return conver_return_code(RDMA_OP, -EINVAL);
    }

    hccp_run_info("Input parameters: phy_id[%u]", phy_id);

    ret = rdma_handle_tmp->rdma_ops->ra_normal_qp_create(rdma_handle_tmp, qp_init_attr, qp_handle, qp);
    if (ret != 0 || *qp_handle == NULL) {
        hccp_err("[create][ra_normal_qp]create qp failed, ret(%d) phy_id(%u)", ret, phy_id);
        return conver_return_code(RDMA_OP, ret);
    }

    return 0;
}

HCCP_ATTRI_VISI_DEF int ra_normal_qp_destroy(void *qp_handle)
{
    int ret;
    struct ra_qp_handle *ra_qp_handle = (struct ra_qp_handle *)qp_handle;

    if (qp_handle == NULL) {
        hccp_err("[destroy][ra_normal_qp]qp_handle is NULL");
        return conver_return_code(RDMA_OP, -EINVAL);
    }

    if (ra_qp_handle->rdma_ops == NULL || ra_qp_handle->rdma_ops->ra_normal_qp_destroy == NULL) {
        hccp_err("[destroy][ra_qp]rdma_ops is NULL or ra_qp_handle->rdma_ops->ra_normal_qp_destroy is NULL, invalid");
        return conver_return_code(RDMA_OP, -EINVAL);
    }

    hccp_run_info("Input parameters: qpn[%u], phy_id[%u], rdev_index[%u]",
        ra_qp_handle->qpn, ra_qp_handle->phy_id, ra_qp_handle->rdev_index);

    ret = ra_qp_handle->rdma_ops->ra_normal_qp_destroy(ra_qp_handle);
    return conver_return_code(RDMA_OP, ret);
}

HCCP_ATTRI_VISI_DEF int ra_set_qp_attr_qos(void *qp_handle, struct qos_attr *attr)
{
    int ret;
    struct ra_qp_handle *ra_qp_handle = (struct ra_qp_handle *)qp_handle;

    if (qp_handle == NULL || attr == NULL) {
        hccp_err("[set][ra_qp_attr_qos]qp_handle or attr is NULL, para error!");
        return conver_return_code(RDMA_OP, -EINVAL);
    }

    if (ra_qp_handle->rdma_ops == NULL || ra_qp_handle->rdma_ops->ra_set_qp_attr_qos == NULL) {
        hccp_err("[set][ra_qp_attr_qos]rdma_ops is NULL or rdma_ops->ra_set_qp_attr_qos is NULL, invalid");
        return conver_return_code(RDMA_OP, -EINVAL);
    }

    ret = ra_qp_handle->rdma_ops->ra_set_qp_attr_qos(ra_qp_handle, attr);
    return conver_return_code(RDMA_OP, ret);
}

HCCP_ATTRI_VISI_DEF int ra_set_qp_attr_timeout(void *qp_handle, unsigned int *timeout)
{
    int ret;
    struct ra_qp_handle *ra_qp_handle = (struct ra_qp_handle *)qp_handle;

    if (qp_handle == NULL || timeout == NULL) {
        hccp_err("[set][ra_qp_attr_timeout]qp_handle or timeout is NULL, para error!");
        return conver_return_code(RDMA_OP, -EINVAL);
    }

    if (ra_qp_handle->rdma_ops == NULL || ra_qp_handle->rdma_ops->ra_set_qp_attr_timeout == NULL) {
        hccp_err("[set][ra_qp_attr_timeout]rdma_ops is NULL or rdma_ops->ra_set_qp_attr_timeout is NULL, invalid");
        return conver_return_code(RDMA_OP, -EINVAL);
    }

    ret = ra_qp_handle->rdma_ops->ra_set_qp_attr_timeout(ra_qp_handle, timeout);
    return conver_return_code(RDMA_OP, ret);
}

HCCP_ATTRI_VISI_DEF int ra_set_qp_attr_retry_cnt(void *qp_handle, unsigned int *retry_cnt)
{
    int ret;
    struct ra_qp_handle *ra_qp_handle = (struct ra_qp_handle *)qp_handle;

    if (qp_handle == NULL || retry_cnt == NULL) {
        hccp_err("[set][ra_qp_attr_retry_cnt]qp_handle or retry_cnt is NULL, para error!");
        return conver_return_code(RDMA_OP, -EINVAL);
    }

    if (ra_qp_handle->rdma_ops == NULL || ra_qp_handle->rdma_ops->ra_set_qp_attr_retry_cnt == NULL) {
        hccp_err("[set][ra_qp_attr_retry_cnt]rdma_ops is NULL or rdma_ops->ra_set_qp_attr_retry_cnt is NULL, invalid");
        return conver_return_code(RDMA_OP, -EINVAL);
    }

    ret = ra_qp_handle->rdma_ops->ra_set_qp_attr_retry_cnt(ra_qp_handle, retry_cnt);
    return conver_return_code(RDMA_OP, ret);
}

HCCP_ATTRI_VISI_DEF int ra_create_comp_channel(const void *rdma_handle, void **comp_channel)
{
    int ret;
    struct ra_rdma_handle *ra_rdma_handle = (struct ra_rdma_handle *)rdma_handle;

    if (rdma_handle == NULL) {
        hccp_err("[ra_create_comp_channel]rdma_handle(%p) is NULL, para error!", rdma_handle);
        return conver_return_code(RDMA_OP, -EINVAL);
    }

    if (comp_channel == NULL) {
        hccp_err("[ra_create_comp_channel]comp_channel is NULL");
        return conver_return_code(RDMA_OP, -EINVAL);
    }

    if (ra_rdma_handle->rdma_ops == NULL || ra_rdma_handle->rdma_ops->ra_create_comp_channel == NULL) {
        hccp_err("[ra_create_comp_channel]rdma_ops is NULL or ra_rdma_handle->rdma_ops->ra_create_comp_channel "
            "is NULL, invalid");
        return conver_return_code(RDMA_OP, -EINVAL);
    }

    ret = ra_rdma_handle->rdma_ops->ra_create_comp_channel(ra_rdma_handle, comp_channel);
    return conver_return_code(RDMA_OP, ret);
}

HCCP_ATTRI_VISI_DEF int ra_destroy_comp_channel(const void *rdma_handle, void *comp_channel)
{
    int ret;
    struct ra_rdma_handle *ra_rdma_handle = (struct ra_rdma_handle *)rdma_handle;

    if (rdma_handle == NULL) {
        hccp_err("[ra_destroy_comp_channel]rdma_handle(%p) is NULL, para error!", rdma_handle);
        return conver_return_code(RDMA_OP, -EINVAL);
    }

    if (comp_channel == NULL) {
        hccp_err("[ra_destroy_comp_channel]comp_channel is NULL");
        return conver_return_code(RDMA_OP, -EINVAL);
    }

    if (ra_rdma_handle->rdma_ops == NULL || ra_rdma_handle->rdma_ops->ra_destroy_comp_channel == NULL) {
        hccp_err("[ra_destroy_comp_channel]rdma_ops is NULL or ra_rdma_handle->rdma_ops->ra_destroy_comp_channel "
            "is NULL, invalid");
        return conver_return_code(RDMA_OP, -EINVAL);
    }

    ret = ra_rdma_handle->rdma_ops->ra_destroy_comp_channel(comp_channel);
    return conver_return_code(RDMA_OP, ret);
}

HCCP_ATTRI_VISI_DEF int ra_get_cqe_err_info(unsigned int phy_id, struct cqe_err_info *info)
{
    int ret;

    if (info == NULL) {
        hccp_err("[ra_get_cqe_err_info]cqe_err_info is NULL, para error!");
        return conver_return_code(RDMA_OP, -EINVAL);
    }

    if (phy_id >= RA_MAX_PHY_ID_NUM) {
        hccp_err("[ra_get_cqe_err_info]phy_id(%u) must greater or equal to 0 and less than %d!",
            phy_id, RA_MAX_PHY_ID_NUM);
        return conver_return_code(RDMA_OP, -EINVAL);
    }

    ret = ra_hdc_get_cqe_err_info(phy_id, info);
    return conver_return_code(RDMA_OP, ret);
}

HCCP_ATTRI_VISI_DEF int ra_rdev_get_cqe_err_info_list(void *rdma_handle, struct cqe_err_info *info_list,
    unsigned int *num)
{
    struct ra_rdma_handle *ra_rdma_handle = (struct ra_rdma_handle *)rdma_handle;
    int ret;

    if (rdma_handle == NULL || info_list == NULL || num == NULL) {
        hccp_err("[get][cqe_err_info_list]rdma_handle or info_list or num is NULL, para error!");
        return conver_return_code(RDMA_OP, -EINVAL);
    }

    if (*num == 0 || *num > CQE_ERR_INFO_MAX_NUM) {
        hccp_err("[get][cqe_err_info_list]num(%u) is invalid!", *num);
        return conver_return_code(OTHERS, -EINVAL);
    }

    ret = ra_hdc_get_cqe_err_info_list(ra_rdma_handle, info_list, num);
    return conver_return_code(RDMA_OP, ret);
}

HCCP_ATTRI_VISI_DEF int ra_get_qp_attr(void *qp_handle, struct qp_attr *attr)
{
    struct ra_qp_handle *ra_qp_handle = (struct ra_qp_handle *)qp_handle;

    if (qp_handle == NULL || attr == NULL) {
        hccp_err("[get][get_qp_attr]qp_handle or attr is NULL, para error!");
        return conver_return_code(RDMA_OP, -EINVAL);
    }

    attr->qpn = ra_qp_handle->qpn;
    attr->udp_sport = ra_qp_handle->udp_sport;
    attr->psn = ra_qp_handle->psn;
    attr->gid_idx = ra_qp_handle->gid_idx;
    (void)memcpy_s(attr->gid, HCCP_GID_RAW_LEN, ra_qp_handle->rdma_handle->gid, HCCP_GID_RAW_LEN);
    return 0;
}

HCCP_ATTRI_VISI_DEF int ra_create_srq(const void *rdma_handle, struct srq_attr *attr)
{
    int ret;
    struct ra_rdma_handle *ra_rdma_handle = (struct ra_rdma_handle *)rdma_handle;

    if (rdma_handle == NULL) {
        hccp_err("[ra_create_srq]rdma_handle(%p) is NULL, para error!", rdma_handle);
        return conver_return_code(RDMA_OP, -EINVAL);
    }

    if (attr == NULL) {
        hccp_err("[ra_create_srq]srq_attr is NULL");
        return conver_return_code(RDMA_OP, -EINVAL);
    }

    // 创建srq&srq cq
    if (ra_rdma_handle->rdma_ops == NULL || ra_rdma_handle->rdma_ops->ra_create_srq == NULL) {
        hccp_err("[ra_create_srq]rdma_ops is NULL or ra_rdma_handle->rdma_ops->ra_create_srq "
            "is NULL, invalid");
        return conver_return_code(RDMA_OP, -EINVAL);
    }

    ret = ra_rdma_handle->rdma_ops->ra_create_srq(ra_rdma_handle, attr);
    return conver_return_code(RDMA_OP, ret);
}

HCCP_ATTRI_VISI_DEF int ra_destroy_srq(const void *rdma_handle, struct srq_attr *attr)
{
    int ret;
    struct ra_rdma_handle *ra_rdma_handle = (struct ra_rdma_handle *)rdma_handle;

    if (rdma_handle == NULL) {
        hccp_err("[ra_destroy_srq]rdma_handle(%p) is NULL, para error!", rdma_handle);
        return conver_return_code(RDMA_OP, -EINVAL);
    }

    if (attr == NULL) {
        hccp_err("[ra_destroy_srq]srq_handle is NULL");
        return conver_return_code(RDMA_OP, -EINVAL);
    }

    // 销毁srq&srq cq
    if (ra_rdma_handle->rdma_ops == NULL || ra_rdma_handle->rdma_ops->ra_destroy_srq == NULL) {
        hccp_err("[ra_destroy_srq]rdma_ops is NULL or ra_rdma_handle->rdma_ops->ra_destroy_srq "
            "is NULL, invalid");
        return conver_return_code(RDMA_OP, -EINVAL);
    }

    ret = ra_rdma_handle->rdma_ops->ra_destroy_srq(ra_rdma_handle, attr);
    return conver_return_code(RDMA_OP, ret);
}

HCCP_ATTRI_VISI_DEF int ra_create_event_handle(int *event_handle)
{
    int ret;

    if (event_handle == NULL) {
        hccp_err("[ra_create_event_handle]event_handle is NULL");
        return conver_return_code(SOCKET_OP, -EINVAL);
    }

    ret = ra_peer_create_event_handle(event_handle);
    if (ret) {
        hccp_err("[ra_create_event_handle]ra_peer_create_event_handle failed ret(%d)", ret);
        return conver_return_code(SOCKET_OP, ret);
    }
    return 0;
}

HCCP_ATTRI_VISI_DEF int ra_ctl_event_handle(int event_handle, const void *fd_handle, int opcode,
    enum RaEpollEvent event)
{
    int ret;

    if (event_handle < 0) {
        hccp_err("[ra_ctl_event_handle]event_handle[%d] is invalid", event_handle);
        return conver_return_code(SOCKET_OP, -EINVAL);
    }
    if (fd_handle == NULL) {
        hccp_err("[ra_ctl_event_handle]fd_handle is NULL");
        return conver_return_code(SOCKET_OP, -EINVAL);
    }
    if (opcode != EPOLL_CTL_ADD && opcode != EPOLL_CTL_DEL && opcode != EPOLL_CTL_MOD) {
        hccp_err("[ra_ctl_event_handle]opcode[%d] invalid, valid opcode includes {%d, %d, %d}",
            opcode, EPOLL_CTL_ADD, EPOLL_CTL_DEL, EPOLL_CTL_MOD);
        return conver_return_code(SOCKET_OP, -EINVAL);
    }
    if (event >= RA_EPOLLINVALD) {
        hccp_err("[ra_ctl_event_handle]event[%d] invalid, valid range [0, %d)", event, RA_EPOLLINVALD);
        return conver_return_code(SOCKET_OP, -EINVAL);
    }

    ret = ra_peer_ctl_event_handle(event_handle, fd_handle, opcode, event);
    if (ret) {
        hccp_err("[ra_ctl_event_handle]ra_peer_ctl_event_handle failed ret(%d)", ret);
        return conver_return_code(SOCKET_OP, ret);
    }
    return 0;
}

HCCP_ATTRI_VISI_DEF int ra_wait_event_handle(int event_handle, struct socket_event_info *event_infos, int timeout,
    unsigned int maxevents, unsigned int *events_num)
{
    int ret;

    if (event_handle < 0) {
        hccp_err("[ra_wait_event_handle]event_handle[%d] is invalid", event_handle);
        return conver_return_code(SOCKET_OP, -EINVAL);
    }

    if (event_infos == NULL) {
        hccp_err("[ra_wait_event_handle]event_infos is NULL");
        return conver_return_code(SOCKET_OP, -EINVAL);
    }

    if (timeout < -1) {
        hccp_err("[ra_wait_event_handle]timeout[%d] is invalid", timeout);
        return conver_return_code(SOCKET_OP, -EINVAL);
    }

    if (maxevents > MAX_SOCKET_EVENT_NUM) {
        hccp_err("[ra_wait_event_handle]maxevents[%u] exceeds %u", maxevents, MAX_SOCKET_EVENT_NUM);
        return conver_return_code(SOCKET_OP, -EINVAL);
    }

    if (events_num == NULL) {
        hccp_err("[ra_wait_event_handle]events_num is NULL");
        return conver_return_code(SOCKET_OP, -EINVAL);
    }

    ret = ra_peer_wait_event_handle(event_handle, event_infos, timeout, maxevents, events_num);
    if (ret) {
        hccp_err("[ra_wait_event_handle]ra_peer_wait_event_handle failed ret(%d)", ret);
        return conver_return_code(SOCKET_OP, ret);
    }
    return 0;
}

HCCP_ATTRI_VISI_DEF int ra_destroy_event_handle(int *event_handle)
{
    int ret;

    if (event_handle == NULL) {
        hccp_err("[ra_destroy_event_handle]event_handle is NULL");
        return conver_return_code(SOCKET_OP, -EINVAL);
    }

    ret = ra_peer_destroy_event_handle(event_handle);
    if (ret) {
        hccp_err("[ra_destroy_event_handle]ra_peer_destroy_event_handle failed ret(%d)", ret);
        return conver_return_code(SOCKET_OP, ret);
    }
    return 0;
}

HCCP_ATTRI_VISI_DEF int ra_poll_cq(void *qp_handle, bool is_send_cq, unsigned int num_entries, void *wc)
{
    int ret;
    struct ra_qp_handle *ra_qp_handle = (struct ra_qp_handle *)qp_handle;

    if (qp_handle == NULL || wc == NULL) {
        hccp_err("[ra_poll]qp_handle is NULL or wc is NULL, para error!");
        return conver_return_code(RDMA_OP, -EINVAL);
    }

    if (ra_qp_handle->rdma_ops == NULL || ra_qp_handle->rdma_ops->ra_poll_cq == NULL) {
        hccp_err("[ra_poll]rdma_ops is NULL or ra_qp_handle->rdma_ops->ra_poll_cq is NULL, invalid");
        return conver_return_code(RDMA_OP, -EINVAL);
    }

    ret = ra_qp_handle->rdma_ops->ra_poll_cq(ra_qp_handle, is_send_cq, num_entries, wc);
    if (ret < 0) {
        return conver_return_code(RDMA_OP, ret);
    }
    return ret;
}

HCCP_ATTRI_VISI_DEF int ra_typical_qp_modify(void *qp_handle, struct typical_qp *local_qp_info,
    struct typical_qp *remote_qp_info)
{
    struct ra_qp_handle *ra_qp_handle = (struct ra_qp_handle *)qp_handle;
    unsigned int phy_id;
    int ret;

    if (qp_handle == NULL || ra_qp_handle->rdma_ops == NULL ||
        ra_qp_handle->rdma_ops->ra_typical_qp_modify == NULL) {
        hccp_err("[modify][ra_qp]qp_handle is NULL or func is NULL");
        return conver_return_code(RDMA_OP, -EINVAL);
    }

    if (local_qp_info == NULL || remote_qp_info == NULL) {
        hccp_err("[modify][ra_qp]local_qp_info is NULL or remote_qp_info is NULL");
        return conver_return_code(RDMA_OP, -EINVAL);
    }

    phy_id = ra_qp_handle->phy_id;
    if (phy_id >= RA_MAX_PHY_ID_NUM) {
        hccp_err("[modify][ra_qp]phy_id(%u) must greater or equal to 0 and less than %d!", phy_id, RA_MAX_PHY_ID_NUM);
        return conver_return_code(RDMA_OP, -EINVAL);
    }

    hccp_run_info("Input parameters: phy_id[%u] local_qpn[%u] remote_qpn[%u]",
        phy_id, local_qp_info->qpn, remote_qp_info->qpn);

    ret = ra_qp_handle->rdma_ops->ra_typical_qp_modify(ra_qp_handle, local_qp_info, remote_qp_info);
    if (ret != 0) {
        hccp_err("[modify][ra_qp]modify qp failed, ret(%d) phy_id(%u)", ret, phy_id);
        return conver_return_code(RDMA_OP, ret);
    }

    return 0;
}

HCCP_ATTRI_VISI_DEF int ra_typical_send_wr(void *qp_handle, struct send_wr *wr, struct send_wr_rsp *op_rsp)
{
    struct ra_qp_handle *ra_qp_handle = (struct ra_qp_handle *)qp_handle;
    int ret;

    if (qp_handle == NULL || wr == NULL || wr->buf_list == NULL || op_rsp == NULL) {
        hccp_err("[send][ra_wr]qp_handle or wr or buf_list or op_rsp is NULL, para error!");
        return conver_return_code(RDMA_OP, -EINVAL);
    }

    if (wr->buf_list->len > MAX_SG_LIST_LEN_MAX) {
        hccp_err("[send][ra_wr]sg list len is more than 2G, len(%u)", wr->buf_list->len);
        return conver_return_code(RDMA_OP, -EINVAL);
    }

    if (ra_qp_handle->rdma_ops == NULL || ra_qp_handle->rdma_ops->ra_typical_send_wr == NULL) {
        hccp_err("[send][ra_wr]rdma_ops is NULL or ra_qp_handle->rdma_ops->ra_typical_send_wr is NULL, invalid");
        return conver_return_code(RDMA_OP, -EINVAL);
    }

    ret = ra_qp_handle->rdma_ops->ra_typical_send_wr(ra_qp_handle, wr, op_rsp);
    return conver_return_code(RDMA_OP, ret);
}

HCCP_ATTRI_VISI_DEF int ra_socket_get_vnic_ip_infos(unsigned int phy_id, enum id_type type, unsigned int ids[],
    unsigned int num, struct ip_info infos[])
{
    int ret;

    if (ids == NULL || num == 0 || infos == NULL) {
        hccp_err("[get][vnic_ip]ids is NULL or num:%u == 0 or infos is NULL", num);
        return conver_return_code(OTHERS, -EINVAL);
    }

    if (phy_id >= RA_MAX_PHY_ID_NUM) {
        hccp_err("[get][vnic_ip]phy_id(%u) is invalid! it must greater or equal to 0 and less than %d!",
            phy_id, RA_MAX_PHY_ID_NUM);
        return conver_return_code(OTHERS, -EINVAL);
    }

    if (type != PHY_ID_VNIC_IP && type != SDID_VNIC_IP) {
        hccp_err("[get][vnic_ip]type[%u] invalid", type);
        return conver_return_code(OTHERS, -EINVAL);
    }

    ret = ra_hdc_get_vnic_ip_infos(phy_id, type, ids, num, infos);
    if (ret) {
        hccp_err("[get][vnic_ip]ra_hdc_get_vnic_ip_infos failed, ret(%d)", ret);
        return conver_return_code(OTHERS, ret);
    }
    return 0;
}

int ra_qp_batch_modify_check_param(void *rdma_handle, void *qp_handle[], unsigned int num, int expect_status)
{
    unsigned int i;

    if (rdma_handle == NULL || num == 0 || qp_handle == NULL ||
        ((expect_status != RA_QP_STATUS_CONNECTED) && (expect_status != RA_QP_STATUS_PAUSE))) {
        hccp_err("[batch_modify][check param]expect_status is %d or rdma_handle is NULL or num[%u] is 0!",
            expect_status, num);
        return conver_return_code(RDMA_OP, -EINVAL);
    }

    for (i = 0; i < num; i++) {
        if (qp_handle[i] == NULL) {
            hccp_err("[modify][ra_qp]qp_handle[%u] is NULL", i);
            return conver_return_code(RDMA_OP, -EINVAL);
        }
    }

    return 0;
}

HCCP_ATTRI_VISI_DEF int ra_qp_batch_modify(void *rdma_handle, void *qp_handle[], unsigned int num, int expect_status)
{
    struct ra_rdma_handle *ra_rdma_handle = NULL;
    unsigned int phy_id;
    int ret;

    ret = ra_qp_batch_modify_check_param(rdma_handle, qp_handle, num, expect_status);
    CHK_PRT_RETURN(ret, hccp_err("ra_qp_batch_modify_check_param invalid[%d]", ret), ret);

    ra_rdma_handle = (struct ra_rdma_handle *)rdma_handle;
    phy_id = ra_rdma_handle->rdev_info.phy_id;
    if (phy_id >= RA_MAX_PHY_ID_NUM || ra_rdma_handle->rdma_ops == NULL ||
        ra_rdma_handle->rdma_ops->ra_qp_batch_modify == NULL) {
        hccp_err("[modify][ra_qp]phy_id(%u) must greater or equal to 0 and less than %d or ops is NULL or "
                 "ra_rdma_handle->rdma_ops->ra_qp_batch_modify is NULL!", phy_id, RA_MAX_PHY_ID_NUM);
        return conver_return_code(RDMA_OP, -EINVAL);
    }

    hccp_run_info("Input parameters: phy_id[%u] num[%u] expect_status[%d]", phy_id, num, expect_status);

    // avoid poll_cq thread to poll cq
    if ((ra_rdma_handle->support_lite != 0) && expect_status == RA_QP_STATUS_PAUSE) {
        RA_PTHREAD_MUTEX_LOCK(&ra_rdma_handle->rdev_mutex);
    }
    ret = ra_rdma_handle->rdma_ops->ra_qp_batch_modify(rdma_handle, qp_handle, num, expect_status);
    if (ret != 0) {
        hccp_err("[modify][ra_qp_batch_modify]modify qp to [%d] failed, ret[%d] phy_id[%u]",
            expect_status, ret, phy_id);
    }
    if ((ra_rdma_handle->support_lite != 0) && expect_status == RA_QP_STATUS_PAUSE) {
        RA_PTHREAD_MUTEX_UNLOCK(&ra_rdma_handle->rdev_mutex);
    }

    return ret;
}

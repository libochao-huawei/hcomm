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

static unsigned int gWhiteListSwitch = 0;

/* socket: nic on device, need use hdc channel, support: 910, 310 */
struct ra_socket_ops gRaHdcSocketOps = {
    .ra_socket_init = RaHdcSocketInit,
    .ra_socket_deinit = RaHdcSocketDeinit,
    .ra_socket_batch_connect = RaHdcSocketBatchConnect,
    .ra_socket_batch_close = RaHdcSocketBatchClose,
    .ra_socket_batch_abort = RaHdcSocketBatchAbort,
    .ra_socket_listen_start = RaHdcSocketListenStart,
    .ra_socket_listen_stop = RaHdcSocketListenStop,
    .ra_get_sockets = RaHdcGetSockets,
    .ra_socket_send = RaHdcSocketSend,
    .ra_socket_recv = RaHdcSocketRecv,
    .ra_get_client_socket_err_info = NULL,
    .ra_get_server_socket_err_info = NULL,
    .ra_socket_set_white_list_status = NULL,
    .ra_socket_get_white_list_status = NULL,
    .ra_socket_white_list_add = RaHdcSocketWhiteListAdd,
    .ra_socket_white_list_del = RaHdcSocketWhiteListDel,
    .ra_socket_accept_credit_add = RaHdcSocketAcceptCreditAdd,
};

/* socket: nic on host/device, support: cx6, 1822 */
struct ra_socket_ops gRaPeerSocketOps = {
    .ra_socket_init = NULL,
    .ra_socket_deinit = RaPeerSocketDeinit,
    .ra_socket_batch_connect = RaPeerSocketBatchConnect,
    .ra_socket_batch_close = RaPeerSocketBatchClose,
    .ra_socket_batch_abort = RaPeerSocketBatchAbort,
    .ra_socket_listen_start = RaPeerSocketListenStart,
    .ra_socket_listen_stop = RaPeerSocketListenStop,
    .ra_get_sockets = RaPeerGetSockets,
    .ra_socket_send = RaPeerSocketSend,
    .ra_socket_recv = RaPeerSocketRecv,
    .ra_get_client_socket_err_info = RaPeerGetClientSocketErrInfo,
    .ra_get_server_socket_err_info = RaPeerGetServerSocketErrInfo,
    .ra_socket_set_white_list_status = NULL,
    .ra_socket_get_white_list_status = NULL,
    .ra_socket_white_list_add = RaPeerSocketWhiteListAdd,
    .ra_socket_white_list_del = RaPeerSocketWhiteListDel,
    .ra_socket_accept_credit_add = RaPeerSocketAcceptCreditAdd,
};

/* rdma: nic on device, need use hdc channel, support:910 */
struct ra_rdma_ops gRaHdcRdmaOps = {
    .ra_rdev_init = RaHdcRdevInit,
    .ra_rdev_get_port_status = RaHdcRdevGetPortStatus,
    .ra_rdev_deinit = RaHdcRdevDeinit,
    .ra_set_tsqp_depth = RaHdcSetTsqpDepth,
    .ra_get_tsqp_depth = RaHdcGetTsqpDepth,
    .ra_qp_create = RaHdcQpCreate,
    .ra_qp_create_with_attrs = RaHdcQpCreateWithAttrs,
    .ra_ai_qp_create = RaHdcAiQpCreate,
    .ra_ai_qp_create_with_attrs = RaHdcAiQpCreateWithAttrs,
    .ra_typical_qp_create = RaHdcTypicalQpCreate,
    .ra_loopback_qp_create = NULL,
    .ra_qp_destroy = RaHdcQpDestroy,
    .ra_typical_qp_modify = RaHdcTypicalQpModify,
    .ra_qp_batch_modify = RaHdcQpBatchModify,
    .ra_qp_connect_async = RaHdcQpConnectAsync,
    .ra_get_qp_status = RaHdcGetQpStatus,
    .ra_mr_reg = RaHdcMrReg,
    .ra_mr_dereg = RaHdcMrDereg,
    .ra_register_mr = RaHdcTypicalMrReg,
    .ra_remap_mr = RaHdcRemapMr,
    .ra_deregister_mr = RaHdcTypicalMrDereg,
    .ra_send_wr = RaHdcSendWr,
    .ra_send_wr_v2 = RaHdcSendWrV2,
    .ra_typical_send_wr = RaHdcTypicalSendWr,
    .ra_send_wrlist = RaHdcSendWrlist,
    .ra_send_wrlist_ext = RaHdcSendWrlistExt,
    .ra_send_normal_wrlist = RaHdcSendNormalWrlist,
    .ra_get_notify_base_addr = RaHdcGetNotifyBaseAddr,
    .ra_get_notify_mr_info = RaHdcGetNotifyMrInfo,
    .ra_recv_wrlist = RaHdcRecvWrlist,
    .ra_poll_cq = RaHdcPollCq,
    .ra_get_qp_context = NULL,
    .ra_normal_qp_create = NULL,
    .ra_normal_qp_destroy = NULL,
    .ra_cq_create = NULL,
    .ra_cq_destroy = NULL,
    .ra_set_qp_attr_qos = RaHdcSetQpAttrQos,
    .ra_set_qp_attr_timeout = RaHdcSetQpAttrTimeout,
    .ra_set_qp_attr_retry_cnt = RaHdcSetQpAttrRetryCnt,
    .ra_create_comp_channel = NULL,
    .ra_destroy_comp_channel = NULL,
    .ra_create_srq = NULL,
    .ra_destroy_srq = NULL,
};

/* rdma: nic on host/device, support:cx6 1822 */
struct ra_rdma_ops gRaPeerRdmaOps = {
    .ra_rdev_init = RaPeerRdevInit,
    .ra_rdev_get_port_status = NULL,
    .ra_rdev_deinit = RaPeerRdevDeinit,
    .ra_set_tsqp_depth = RaPeerSetTsqpDepth,
    .ra_get_tsqp_depth = RaPeerGetTsqpDepth,
    .ra_qp_create = RaPeerQpCreate,
    .ra_qp_create_with_attrs = RaPeerQpCreateWithAttrs,
    .ra_ai_qp_create = NULL,
    .ra_ai_qp_create_with_attrs = NULL,
    .ra_typical_qp_create = NULL,
    .ra_loopback_qp_create = RaPeerLoopbackQpCreate,
    .ra_qp_destroy = RaPeerQpDestroy,
    .ra_typical_qp_modify = RaPeerTypicalQpModify,
    .ra_qp_batch_modify = NULL,
    .ra_qp_connect_async = RaPeerQpConnectAsync,
    .ra_get_qp_status = RaPeerGetQpStatus,
    .ra_mr_reg = RaPeerMrReg,
    .ra_mr_dereg = RaPeerMrDereg,
    .ra_register_mr = RaPeerRegisterMr,
    .ra_remap_mr = NULL,
    .ra_deregister_mr = RaPeerDeregisterMr,
    .ra_send_wr = RaPeerSendWr,
    .ra_send_wr_v2 = NULL,
    .ra_typical_send_wr = NULL,
    .ra_send_wrlist = RaPeerSendWrlist,
    .ra_send_wrlist_ext = NULL,
    .ra_send_normal_wrlist = NULL,
    .ra_get_notify_base_addr = RaPeerGetNotifyBaseAddr,
    .ra_get_notify_mr_info = NULL,
    .ra_recv_wrlist = RaPeerRecvWrlist,
    .ra_poll_cq = NULL,
    .ra_get_qp_context = RaPeerGetQpContext,
    .ra_normal_qp_create = RaPeerNormalQpCreate,
    .ra_normal_qp_destroy = RaPeerNormalQpDestroy,
    .ra_cq_create = RaPeerCqCreate,
    .ra_cq_destroy = RaPeerCqDestroy,
    .ra_set_qp_attr_qos = RaPeerSetQpAttrQos,
    .ra_set_qp_attr_timeout = RaPeerSetQpAttrTimeout,
    .ra_set_qp_attr_retry_cnt = RaPeerSetQpAttrRetryCnt,
    .ra_create_comp_channel = RaPeerCreateCompChannel,
    .ra_destroy_comp_channel = RaPeerDestroyCompChannel,
    .ra_create_srq = RaPeerCreateSrq,
    .ra_destroy_srq = RaPeerDestroySrq,
};

struct errcode_info gErrcodeInfoList[] = {
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

int RaInetPton(int family, union hccp_ip_addr ip, char netAddr[], unsigned int len)
{
    const char *str = NULL;
    str = inet_ntop(family, &(ip.addr), netAddr, len);
    CHK_PRT_RETURN(str == NULL, hccp_err("[ntop_convert][ra_inet]the ip failed err(%d)", errno), -EINVAL);
    return 0;
}

HCCP_ATTRI_VISI_DEF int RaSocketInit(int mode, struct rdev rdevInfo, void **socketHandle)
{
    struct ra_socket_handle *socketHandleTmp = NULL;
    int ret;
    char localIp[MAX_IP_LEN] = {0};

    CHK_PRT_RETURN(rdevInfo.phy_id >= RA_MAX_PHY_ID_NUM || socketHandle == NULL,
        hccp_err("[init][ra_socket]phy_id(%u) is invalid! it must be [0,%d) or socket is null!",
                 rdevInfo.phy_id, RA_MAX_PHY_ID_NUM), ConverReturnCode(HCCP_INIT, -EINVAL));

    ret = RaInetPton(rdevInfo.family, rdevInfo.local_ip, localIp, MAX_IP_LEN);
    CHK_PRT_RETURN(ret, hccp_err("[init][ra_socket]ra_inet_pton for local_ip failed, ret(%d)", ret),
        ConverReturnCode(HCCP_INIT, ret));

    hccp_run_info("socket init:mode=%d phy_id=%u family=%d ip=%s", mode, rdevInfo.phy_id, rdevInfo.family, localIp);

    socketHandleTmp = calloc(1, sizeof(struct ra_socket_handle));
    CHK_PRT_RETURN(socketHandleTmp == NULL,
        hccp_err("[init][ra_socket]ra_inet_pton for local_ip failed, ret(%d)", ret),
        ConverReturnCode(HCCP_INIT, -ENOMEM));

    if (mode == NETWORK_OFFLINE) {
        socketHandleTmp->socket_ops = &gRaHdcSocketOps;
    } else if (mode == NETWORK_PEER_ONLINE) {
        socketHandleTmp->socket_ops = &gRaPeerSocketOps;
    } else {
        hccp_err("[init][ra_socket]Wrong mode(%d), do not support", mode);
        ret = -EINVAL;
        goto err;
    }

    ret = memcpy_s(&(socketHandleTmp->rdev_info), sizeof(struct rdev), &rdevInfo, sizeof(struct rdev));
    if (ret) {
        hccp_err("[init][ra_socket]memcpy_s for rdev_info failed, ret(%d)", ret);
        ret = -ESAFEFUNC;
        goto err;
    }

    if (rdevInfo.family == AF_INET && rdevInfo.local_ip.addr.s_addr < RA_VNIC_MAX &&
        socketHandleTmp->socket_ops->ra_socket_init != NULL) {
        // HDC模式只支持IPv4
        if (rdevInfo.local_ip.addr.s_addr < RA_VNIC_MAX) {
            ret = socketHandleTmp->socket_ops->ra_socket_init(rdevInfo);
            if (ret) {
                hccp_err("[init][ra_socket]ra socket init failed, ret(%d)", ret);
                goto err;
            }
        }
    }
    *socketHandle = (void*)socketHandleTmp;
    return ret;
err:
    free(socketHandleTmp);
    socketHandleTmp = NULL;
    return ConverReturnCode(HCCP_INIT, ret);
}

HCCP_ATTRI_VISI_DEF int RaSocketInitV1(int mode, struct socket_init_info_t socketInit, void **socketHandle)
{
    // 支持IPv4/IPv6 socket初始化
    // IPv6需要输入scope id
    struct ra_socket_handle *socketHandleTmp = NULL;
    char localIp[MAX_IP_LEN] = {0};
    int ret;

    CHK_PRT_RETURN(socketInit.rdev_info.phy_id >= RA_MAX_PHY_ID_NUM || socketHandle == NULL,
        hccp_err("[init][ra_socket]phy_id(%u) is invalid! it must be [0,%d) or socket is null!",
                 socketInit.rdev_info.phy_id, RA_MAX_PHY_ID_NUM), ConverReturnCode(HCCP_INIT, -EINVAL));

    ret = RaInetPton(socketInit.rdev_info.family, socketInit.rdev_info.local_ip, localIp, MAX_IP_LEN);
    CHK_PRT_RETURN(ret, hccp_err("[init][ra_socket]ra_inet_pton for local_ip failed, ret(%d)", ret),
        ConverReturnCode(HCCP_INIT, ret));

    hccp_run_info("socket init:mode=%d phy_id=%u scope_id=%d family=%d ip=%s", mode, socketInit.rdev_info.phy_id,
        socketInit.scope_id, socketInit.rdev_info.family, localIp);

    socketHandleTmp = calloc(1, sizeof(struct ra_socket_handle));
    CHK_PRT_RETURN(socketHandleTmp == NULL, hccp_err("[init][ra_socket]calloc for socket_handle failed"),
        ConverReturnCode(HCCP_INIT, -ENOMEM));

    if (mode == NETWORK_OFFLINE) {
        socketHandleTmp->socket_ops = &gRaHdcSocketOps;
        socketHandleTmp->scope_id = socketInit.scope_id;
    } else if (mode == NETWORK_PEER_ONLINE) {
        socketHandleTmp->socket_ops = &gRaPeerSocketOps;
        socketHandleTmp->scope_id = socketInit.scope_id;
    } else {
        hccp_err("[init][ra_socket]Wrong mode(%d), do not support", mode);
        ret = -EINVAL;
        goto err;
    }

    ret = memcpy_s(&(socketHandleTmp->rdev_info), sizeof(struct rdev), &socketInit.rdev_info, sizeof(struct rdev));
    if (ret) {
        hccp_err("[init][ra_socket]memcpy_s for rdev_info failed, ret(%d)", ret);
        ret = -ESAFEFUNC;
        goto err;
    }

    if (socketInit.rdev_info.family == AF_INET && socketInit.rdev_info.local_ip.addr.s_addr < RA_VNIC_MAX &&
        socketHandleTmp->socket_ops->ra_socket_init != NULL) {
        // HDC模式只支持IPv4
        if (socketInit.rdev_info.local_ip.addr.s_addr < RA_VNIC_MAX) {
            ret = socketHandleTmp->socket_ops->ra_socket_init(socketInit.rdev_info);
            if (ret) {
                hccp_err("[init][ra_socket]ra socket init v1 failed, ret(%d)", ret);
                goto err;
            }
        }
    }
    *socketHandle = (void*)socketHandleTmp;
    return ret;
err:
    free(socketHandleTmp);
    socketHandleTmp = NULL;
    return ConverReturnCode(HCCP_INIT, ret);
}

HCCP_ATTRI_VISI_DEF int RaSocketDeinit(void *socketHandle)
{
    struct ra_socket_handle *socketHandleTmp = NULL;
    struct rdev rdevInfo;
    char localIp[MAX_IP_LEN] = {0};
    int ret;

    CHK_PRT_RETURN(socketHandle == NULL, hccp_err("[deinit][ra_socket]socket_handle is NULL"),
        ConverReturnCode(HCCP_INIT, -EINVAL));

    socketHandleTmp = (struct ra_socket_handle *)socketHandle;
    rdevInfo = socketHandleTmp->rdev_info;
    CHK_PRT_RETURN(rdevInfo.phy_id >= RA_MAX_PHY_ID_NUM,
        hccp_err("[deinit][ra_socket]phy_id(%u) >= %u. invalid", rdevInfo.phy_id, RA_MAX_PHY_ID_NUM),
        ConverReturnCode(HCCP_INIT, -EINVAL));

    ret = RaInetPton(rdevInfo.family, rdevInfo.local_ip, localIp, MAX_IP_LEN);
    CHK_PRT_RETURN(ret, hccp_err("[deinit][ra_socket]ra_inet_pton for local_ip failed, ret(%d)", ret),
        ConverReturnCode(HCCP_INIT, ret));

    hccp_run_info("Input parameters: phy_id[%u] family[%d] local_ip[%s]", rdevInfo.phy_id, rdevInfo.family, localIp);

    ret = socketHandleTmp->socket_ops->ra_socket_deinit(rdevInfo);
    if (ret) {
        hccp_err("[deinit][ra_socket]ra socket deinit failed, ret(%d)", ret);
    }

    socketHandleTmp->socket_ops = NULL;
    free(socketHandleTmp);
    socketHandleTmp = NULL;
    return ConverReturnCode(HCCP_INIT, ret);
}

STATIC int RaRdevInitCheckIp(int mode, struct rdev rdevInfo, char localIp[])
{
    struct interface_info *interfaceInfos = NULL;
    char interfaceIp[MAX_IP_LEN] = { 0 };
    struct ra_get_ifattr config = { 0 };
    unsigned int i, interfaceVersion;
    unsigned int num = 0;
    int ret;

    config.phy_id = rdevInfo.phy_id;
    config.nic_position = (unsigned int)mode;
    if (config.nic_position == NETWORK_OFFLINE) {
        ret = RaGetInterfaceVersion(config.phy_id, RA_RS_GET_IFNUM, &interfaceVersion);
        if (ret != 0 || interfaceVersion != RA_RS_GET_IFNUM_VERSION) {
            num = MAX_INTERFACE_NUM;
            goto get_addrs;
        }
    }
    /* get the number of interfaces */
    ret = RaGetIfnum(&config, &num);
    CHK_PRT_RETURN(ret != 0 || num == 0, hccp_err("[check][ip]get_ifnum failed, ret(%d) or num is 0", ret), -EINVAL);

get_addrs:
    /* calloc for interface_infos according to the real num */
    interfaceInfos = calloc(num, sizeof(struct interface_info));
    CHK_PRT_RETURN(interfaceInfos == NULL, hccp_err("[check][ip]calloc for interface_infos failed"), -EINVAL);

    ret = RaGetIfaddrs(&config, interfaceInfos, &num);
    if (ret || (num == 0)) {
        hccp_err("[check][ip]ra_get_ifaddrs for interface_infos failed, ret(%d), num(%u)", ret, num);
        free(interfaceInfos);
        return -EINVALIDIP;
    }

    for (i = 0; i < num; i++) {
        if (interfaceInfos[i].family != rdevInfo.family) {
            continue;
        }

        ret = RaInetPton(interfaceInfos[i].family, interfaceInfos[i].ifaddr.ip, interfaceIp, MAX_IP_LEN);
        if (ret != 0) {
            hccp_err("[check][ip]ra_inet_pton for interface_infos[%u] failed, ret(%d)", i, ret);
            free(interfaceInfos);
            return ret;
        }

        ret = strncmp(interfaceIp, localIp, MAX_IP_LEN - 1);
        if (!ret) {
            free(interfaceInfos);
            return 0;
        }
    }
    hccp_err("[check][ip]failed, ret(%d) the IP address(%s) in the ranktable is inconsistent with the IP(%s)"\
        "address of the network adapter, please make sure they're consistent. "\
        "num(%u)", ret, localIp, interfaceIp, num);

    free(interfaceInfos);
    return -EINVALIDIP;
}

int RaRdevInitCheck(int mode, struct rdev rdevInfo, char localIp[], unsigned int num, void *rdmaHandle)
{
    int ret;

    CHK_PRT_RETURN(rdevInfo.phy_id >= RA_MAX_PHY_ID_NUM, hccp_err("[check][ra_rdev_init]phy_id(%u) is invalid!"
        "it must greater or equal to 0 and less than %d!", rdevInfo.phy_id, RA_MAX_PHY_ID_NUM), -EINVAL);

    CHK_PRT_RETURN(rdmaHandle == NULL, hccp_err("[check][ra_rdev_init]phy_id(%u) rdma_handle is null!",
        rdevInfo.phy_id), -EINVAL);

    ret = RaInetPton(rdevInfo.family, rdevInfo.local_ip, localIp, num);
    CHK_PRT_RETURN(ret, hccp_err("[check][ra_rdev_init]ra_inet_pton for local_ip failed, ret(%d)", ret), -EINVAL);

    ret = RaRdevInitCheckIp(mode, rdevInfo, localIp);
    CHK_PRT_RETURN(ret, hccp_err("[check][ra_rdev_init]ra_rdev_init_check_ip failed, ret(%d)", ret), ret);

    return 0;
}

STATIC int RaGetInitRdmaHandle(int mode, struct ra_rdma_handle *rdmaHandle)
{
    if (mode == NETWORK_OFFLINE) {
        (void)RaHdcRdmaSetOps(rdmaHandle, &gRaHdcRdmaOps);
    } else if (mode == NETWORK_PEER_ONLINE) {
        (void)RaHdcRdmaSetOps(rdmaHandle, &gRaPeerRdmaOps);
    } else {
        hccp_err("[init][ra_rdev]Wrong mode(%d), do not support", mode);
        return -EINVAL;
    }

    CHK_PRT_RETURN(rdmaHandle->rdma_ops->ra_rdev_init == NULL, hccp_err("[init][ra_rdev] ra_rdev_init is NULL!"),
        -EINVAL);
    return 0;
}

STATIC void RaGenerateGidByRdevInfo(struct ra_rdma_handle *rdmaHandle)
{
#define RA_GID_SEQ_NUM   4
#define RA_GID_SEQ_ZERO  0
#define RA_GID_SEQ_ONE   1
#define RA_GID_SEQ_TWO   2
#define RA_GID_SEQ_THREE 3
    union hccp_ip_addr localIp = rdmaHandle->rdev_info.local_ip;
    int family = rdmaHandle->rdev_info.family;
    unsigned int gidV4[RA_GID_SEQ_NUM];

    if (family == AF_INET6) {
        (void)memcpy_s(rdmaHandle->gid, HCCP_GID_RAW_LEN, &(localIp.addr6), HCCP_GID_RAW_LEN);
    } else {
        gidV4[RA_GID_SEQ_ZERO] = 0;
        gidV4[RA_GID_SEQ_ONE] = 0;
        /* The gid format generated by ipv4 is filled with 0xFFFF in [33, 48] */
        gidV4[RA_GID_SEQ_TWO]   = htonl(0x0000FFFF);
        gidV4[RA_GID_SEQ_THREE] = localIp.addr.s_addr;
        (void)memcpy_s(rdmaHandle->gid, HCCP_GID_RAW_LEN, gidV4, HCCP_GID_RAW_LEN);
    }

    return;
}

STATIC int RaRdevInitWithBackupInfo(struct rdev_init_info initInfo, struct rdev rdevInfo,
    struct ra_backup_info backupInfo, void **rdmaHandle)
{
    struct ra_rdma_handle *rdmaHandleTmp = NULL;
    char localIp[MAX_IP_LEN] = {0};
    unsigned int rdevIndex;
    int ret;

    ret = RaRdevInitCheck(initInfo.mode, rdevInfo, localIp, MAX_IP_LEN, rdmaHandle);
    CHK_PRT_RETURN(ret, hccp_err("[init][ra_rdev]ra_rdev_init_check failed ,ret(%d)", ret),
        ConverReturnCode(HCCP_INIT, ret));

    hccp_run_info("rdev_init:mode=%d phy_id=%u family=%d ip=%s notify_type=%u", initInfo.mode, rdevInfo.phy_id,
        rdevInfo.family, localIp, initInfo.notify_type);

    rdmaHandleTmp = calloc(1, sizeof(struct ra_rdma_handle));
    CHK_PRT_RETURN(rdmaHandleTmp == NULL, hccp_err("[init][ra_rdev]calloc for rdma_handle failed"),
        ConverReturnCode(HCCP_INIT, -ENOMEM));

    // disabled_lite_thread will be invalid if enabled_910a_lite is false
    rdmaHandleTmp->disabled_lite_thread = initInfo.disabled_lite_thread;
    rdmaHandleTmp->enabled_910a_lite = initInfo.enabled_910a_lite;
    rdmaHandleTmp->enabled_2mb_lite = initInfo.enabled_2mb_lite;
    (void)memcpy_s(&rdmaHandleTmp->backup_info, sizeof(struct ra_backup_info),
        &backupInfo, sizeof(struct ra_backup_info));

    ret = RaGetInitRdmaHandle(initInfo.mode, rdmaHandleTmp);
    if (ret) {
        hccp_err("[init][ra_rdev] get rdma handle failed, ret(%d)", ret);
        goto err;
    }

    ret = memcpy_s(&(rdmaHandleTmp->rdev_info), sizeof(struct rdev), &rdevInfo, sizeof(struct rdev));
    if (ret) {
        hccp_err("[init][ra_rdev]memcpy_s for rdev_info failed, ret(%d)", ret);
        ret = -ESAFEFUNC;
        goto err;
    }

    ret = rdmaHandleTmp->rdma_ops->ra_rdev_init(rdmaHandleTmp, initInfo.notify_type, rdevInfo, &rdevIndex);
    if (ret) {
        hccp_err("[init][ra_rdev]ra rdev init failed, ret(%d)", ret);
        goto err;
    }

    rdmaHandleTmp->rdev_index = rdevIndex;
    RaGenerateGidByRdevInfo(rdmaHandleTmp);
    *rdmaHandle = (void *)rdmaHandleTmp;

    // save rdev handle for helper
    RaRdevSetHandle(rdevInfo.phy_id, *rdmaHandle);

    return ret;
err:
    free(rdmaHandleTmp);
    rdmaHandleTmp = NULL;
    return ConverReturnCode(HCCP_INIT, ret);
}

HCCP_ATTRI_VISI_DEF int RaRdevInitWithBackup(struct rdev_init_info *initInfo, struct rdev *rdevInfo,
    struct rdev *backupRdevInfo, void **rdmaHandle)
{
    struct ra_backup_info backupInfo = { 0 };

    if (initInfo == NULL || rdevInfo == NULL || backupRdevInfo == NULL || rdmaHandle == NULL) {
        hccp_err("[init][ra_rdev]init_info or rdev_info or backup_rdev_info or rdma_handle is NULL");
        return -EINVAL;
    }

    backupInfo.backup_flag = true;
    (void)memcpy_s(&backupInfo.rdev_info, sizeof(struct rdev), backupRdevInfo, sizeof(struct rdev));

    return RaRdevInitWithBackupInfo(*initInfo, *rdevInfo, backupInfo, rdmaHandle);
}

HCCP_ATTRI_VISI_DEF int RaRdevInitV2(struct rdev_init_info initInfo, struct rdev rdevInfo, void **rdmaHandle)
{
    struct ra_backup_info backupInfo = { 0 };

    return RaRdevInitWithBackupInfo(initInfo, rdevInfo, backupInfo, rdmaHandle);
}

HCCP_ATTRI_VISI_DEF int RaRdevInit(int mode, unsigned int notifyType, struct rdev rdevInfo, void **rdmaHandle)
{
    struct rdev_init_info initInfo = { 0 };
    initInfo.mode = mode;
    initInfo.notify_type = notifyType;
    initInfo.disabled_lite_thread = false; // will start lite thread by default
    initInfo.enabled_910a_lite = false; // will disabled lite on 910A by default
    initInfo.enabled_2mb_lite = false; // will disabled lite on 2MB page align scenario by default

    return RaRdevInitV2(initInfo, rdevInfo, rdmaHandle);
}

STATIC int RaRdevDeinitParaCheck(void *rdmaHandle, struct ra_rdma_handle **rdmaHandleTmp)
{
    int ret;
    char localIp[MAX_IP_LEN] = {0};
    struct rdev rdevInfo;

    *rdmaHandleTmp = (struct ra_rdma_handle *)rdmaHandle;
    rdevInfo = (*rdmaHandleTmp)->rdev_info;
    CHK_PRT_RETURN(rdevInfo.phy_id >= RA_MAX_PHY_ID_NUM, hccp_err("[deinit][ra_rdev]phy_id(%u)"
        "must smaller than %u", rdevInfo.phy_id, RA_MAX_PHY_ID_NUM), -EINVAL);

    ret = RaInetPton(rdevInfo.family, rdevInfo.local_ip, localIp, MAX_IP_LEN);
    CHK_PRT_RETURN(ret, hccp_err("[deinit][ra_rdev]ra_inet_pton for local_ip failed, ret(%d)", ret), ret);

    CHK_PRT_RETURN((*rdmaHandleTmp)->rdma_ops == NULL || (*rdmaHandleTmp)->rdma_ops->ra_rdev_deinit == NULL,
        hccp_err("[deinit][ra_rdev]rdma_ops is NULL or ra_rdev_deinit is NULL"), -EINVAL);

    hccp_run_info("Input parameters: phy_id[%u] rdev_index[%u] family[%d] local_ip[%s]",
        rdevInfo.phy_id, (*rdmaHandleTmp)->rdev_index, rdevInfo.family, localIp);

    return 0;
}

HCCP_ATTRI_VISI_DEF int RaRdevGetPortStatus(void *rdmaHandle, enum port_status *status)
{
    struct ra_rdma_handle *rdmaHandleTmp = (struct ra_rdma_handle *)rdmaHandle;
    unsigned int phyId;
    int ret;

    CHK_PRT_RETURN(rdmaHandle == NULL || status == NULL,
        hccp_err("[get][ra_port_status]rdma_handle or status is NULL, invalid"),
        ConverReturnCode(RDMA_OP, -EINVAL));

    phyId = rdmaHandleTmp->rdev_info.phy_id;
    CHK_PRT_RETURN(phyId >= RA_MAX_PHY_ID_NUM,
        hccp_err("[get][ra_port_status]phy_id(%u) must smaller than %u", phyId, RA_MAX_PHY_ID_NUM),
        ConverReturnCode(RDMA_OP, -EINVAL));

    CHK_PRT_RETURN(rdmaHandleTmp->rdma_ops == NULL || rdmaHandleTmp->rdma_ops->ra_rdev_get_port_status == NULL,
        hccp_err("[get][ra_port_status]rdma_ops is NULL or ra_rdev_get_port_status is NULL, invalid"),
        ConverReturnCode(RDMA_OP, -EINVAL));

    ret = rdmaHandleTmp->rdma_ops->ra_rdev_get_port_status(rdmaHandleTmp, status);
    return ConverReturnCode(RDMA_OP, ret);
}

HCCP_ATTRI_VISI_DEF int RaRdevDeinit(void *rdmaHandle, unsigned int notifyType)
{
    struct ra_rdma_handle *rdmaHandleTmp = NULL;
    unsigned int phyId;
    int ret;

    CHK_PRT_RETURN(rdmaHandle == NULL, hccp_err("[deinit][ra_rdev] rdma_handle is NULL"),
        ConverReturnCode(HCCP_INIT, -EINVAL));

    ret = RaRdevDeinitParaCheck(rdmaHandle, &rdmaHandleTmp);
    CHK_PRT_RETURN(ret, hccp_err("[deinit][ra_rdev] para check failed, ret(%d)", ret),
        ConverReturnCode(HCCP_INIT, ret));

    phyId = rdmaHandleTmp->rdev_info.phy_id;

    ret = rdmaHandleTmp->rdma_ops->ra_rdev_deinit(rdmaHandleTmp, notifyType);
    if (ret) {
        hccp_err("[deinit][ra_rdev]ra rdv deinit failed, ret(%d)", ret);
        goto free_rdma_handle;
    }

free_rdma_handle:
    rdmaHandleTmp->rdma_ops = NULL;
    free(rdmaHandleTmp);
    rdmaHandleTmp = NULL;
    RaRdevSetHandle(phyId, NULL);
    return ConverReturnCode(HCCP_INIT, ret);
}

HCCP_ATTRI_VISI_DEF int RaRdevGetSupportLite(void *rdmaHandle, int *supportLite)
{
    struct ra_rdma_handle *rdmaHandleTmp = NULL;

    CHK_PRT_RETURN(rdmaHandle == NULL || supportLite == NULL,
        hccp_err("[get][ra_rdev]rdma_handle is NULL or support_lite is NULL"), ConverReturnCode(RDMA_OP, -EINVAL));

    rdmaHandleTmp = (struct ra_rdma_handle *)rdmaHandle;
    *supportLite = rdmaHandleTmp->support_lite;

    hccp_dbg("[get][ra_rdev]support_lite:%d", *supportLite);

    return 0;
}

HCCP_ATTRI_VISI_DEF int RaSocketBatchConnect(struct socket_connect_info_t conn[], unsigned int num)
{
    struct ra_socket_handle *socketHandle = NULL;
    char remoteIp[MAX_IP_LEN] = {0};
    char localIp[MAX_IP_LEN] = {0};
    unsigned int i;
    int ret;

    CHK_PRT_RETURN(conn == NULL || num == 0 || num > MAX_SOCKET_NUM,
        hccp_err("[batch_connect][ra_socket]conn is NULL or num[%u] is zero or num is greater than %d", num,
        MAX_SOCKET_NUM), ConverReturnCode(SOCKET_OP, -EINVAL));

    for (i = 0; i < num; i++) {
        socketHandle = (struct ra_socket_handle *)conn[i].socket_handle;
        if (socketHandle == NULL || socketHandle->socket_ops == NULL ||
            socketHandle->socket_ops->ra_socket_batch_connect == NULL) {
            hccp_err("[batch_connect][ra_socket]socket_handle or func is NULL");
            return ConverReturnCode(SOCKET_OP, -EINVAL);
        }

        CHK_PRT_RETURN(socketHandle->rdev_info.phy_id >= RA_MAX_PHY_ID_NUM,
            hccp_err("[batch_connect][ra_socket]phy_id(%u) must smaller than %u", socketHandle->rdev_info.phy_id,
            RA_MAX_PHY_ID_NUM), ConverReturnCode(SOCKET_OP, -EINVAL));

        CHK_PRT_RETURN(strlen(conn[i].tag) >= SOCK_CONN_TAG_SIZE,
            hccp_err("[batch_connect][ra_socket]conn tag len(%d) more than max len(%d)", strlen(conn[i].tag),
            SOCK_CONN_TAG_SIZE), ConverReturnCode(SOCKET_OP, -EINVAL));

        ret = RaInetPton(socketHandle->rdev_info.family, socketHandle->rdev_info.local_ip, localIp, MAX_IP_LEN);
        CHK_PRT_RETURN(ret, hccp_err("[batch_connect][ra_socket]ra_inet_pton for local_ip failed, ret(%d)", ret),
            ConverReturnCode(SOCKET_OP, ret));

        ret = RaInetPton(socketHandle->rdev_info.family, conn[i].remote_ip, remoteIp, MAX_IP_LEN);
        CHK_PRT_RETURN(ret, hccp_err("[batch_connect][ra_socket]ra_inet_pton for remote_ip failed, ret(%d)", ret),
            ConverReturnCode(SOCKET_OP, ret));

        hccp_run_info("Input parameters: [%u]th, phyId[%u], localIp[%s], remoteIp[%s], port[%u], tag[%s], cnt[%u]",
            i, socketHandle->rdev_info.phy_id, localIp, remoteIp, conn[i].port, conn[i].tag,
            socketHandle->connect_cnt);
    }

    socketHandle->connect_cnt++;
    ret = socketHandle->socket_ops->ra_socket_batch_connect(socketHandle->rdev_info.phy_id, conn, num);
    return ConverReturnCode(SOCKET_OP, ret);
}

HCCP_ATTRI_VISI_DEF int RaSocketBatchClose(struct socket_close_info_t conn[], unsigned int num)
{
    struct ra_socket_handle *socketHandle = NULL;
    char localIp[MAX_IP_LEN] = {0};
    unsigned int phyId;
    unsigned int i;
    int ret;

    CHK_PRT_RETURN(conn == NULL || num == 0 || num > MAX_SOCKET_NUM,
        hccp_err("[batch_close][ra_socket]conn is NULL or num[%u] is zero or num is greater than %d", num,
        MAX_SOCKET_NUM), ConverReturnCode(SOCKET_OP, -EINVAL));

    for (i = 0; i < num; i++) {
        socketHandle = (struct ra_socket_handle *)conn[i].socket_handle;
        if (socketHandle == NULL || socketHandle->socket_ops == NULL ||
            socketHandle->socket_ops->ra_socket_batch_close == NULL) {
            hccp_err("[batch_close][ra_socket]socket_handle or func is NULL");
            return ConverReturnCode(SOCKET_OP, -EINVAL);
        }
        phyId = socketHandle->rdev_info.phy_id;
        CHK_PRT_RETURN(phyId >= RA_MAX_PHY_ID_NUM,
            hccp_err("[batch_close][ra_socket]phy_id(%u) must smaller than %u", phyId, RA_MAX_PHY_ID_NUM),
            ConverReturnCode(SOCKET_OP, -EINVAL));

        ret = RaInetPton(socketHandle->rdev_info.family, socketHandle->rdev_info.local_ip, localIp, MAX_IP_LEN);
        CHK_PRT_RETURN(ret, hccp_err("[batch_connect][ra_socket]ra_inet_pton for local_ip failed, ret(%d)", ret),
            ConverReturnCode(SOCKET_OP, ret));

        hccp_run_info("Input parameters: [%u]th, phyId[%u], localIp[%s], cnt[%u]", i, phyId,
            localIp, socketHandle->close_cnt);
    }

    socketHandle->close_cnt++;
    ret = socketHandle->socket_ops->ra_socket_batch_close(phyId, conn, num);
    return ConverReturnCode(SOCKET_OP, ret);
}

HCCP_ATTRI_VISI_DEF int RaSocketBatchAbort(struct socket_connect_info_t conn[], unsigned int num)
{
    struct ra_socket_handle *socketHandle = NULL;
    char remoteIp[MAX_IP_LEN] = {0};
    char localIp[MAX_IP_LEN] = {0};
    unsigned int phyId;
    unsigned int i;
    int ret;

    CHK_PRT_RETURN(conn == NULL || num == 0 || num > MAX_SOCKET_NUM,
        hccp_err("[batch_abort][ra_socket]conn is NULL or num[%u] is zero or num is greater than %d",
        num, MAX_SOCKET_NUM), ConverReturnCode(SOCKET_OP, -EINVAL));

    for (i = 0; i < num; i++) {
        socketHandle = (struct ra_socket_handle *)conn[i].socket_handle;
        CHK_PRT_RETURN(socketHandle == NULL || socketHandle->socket_ops == NULL ||
            socketHandle->socket_ops->ra_socket_batch_abort == NULL,
            hccp_err("[batch_abort][ra_socket]socket_handle or func is NULL"),
            ConverReturnCode(SOCKET_OP, -EINVAL));

        phyId = socketHandle->rdev_info.phy_id;
        CHK_PRT_RETURN(phyId >= RA_MAX_PHY_ID_NUM,
            hccp_err("[batch_abort][ra_socket]phy_id(%u) must smaller than %u", phyId, RA_MAX_PHY_ID_NUM),
            ConverReturnCode(SOCKET_OP, -EINVAL));

        ret = RaInetPton(socketHandle->rdev_info.family, socketHandle->rdev_info.local_ip, localIp, MAX_IP_LEN);
        CHK_PRT_RETURN(ret, hccp_err("[batch_abort][ra_socket]ra_inet_pton for local_ip failed, ret(%d)", ret),
            ConverReturnCode(SOCKET_OP, ret));

        ret = RaInetPton(socketHandle->rdev_info.family, conn[i].remote_ip, remoteIp, MAX_IP_LEN);
        CHK_PRT_RETURN(ret, hccp_err("[batch_abort][ra_socket]ra_inet_pton for remote_ip failed, ret(%d)", ret),
            ConverReturnCode(SOCKET_OP, ret));

        hccp_run_info("Input parameters: [%u]th, phyId[%u], localIp[%s], remoteIp[%s], tag[%s], cnt[%u]",
            i, phyId, localIp, remoteIp, conn[i].tag, socketHandle->abort_cnt);
    }

    socketHandle->abort_cnt++;
    ret = socketHandle->socket_ops->ra_socket_batch_abort(phyId, conn, num);
    return ConverReturnCode(SOCKET_OP, ret);
}

HCCP_ATTRI_VISI_DEF int RaSocketListenStart(struct socket_listen_info_t conn[], unsigned int num)
{
    struct ra_socket_handle *socketHandle = NULL;
    char localIp[MAX_IP_LEN] = {0};
    unsigned int phyId;
    unsigned int i;
    int ret;

    CHK_PRT_RETURN(conn == NULL || num == 0 || num > MAX_SOCKET_NUM,
        hccp_err("[listen_start][ra_socket]conn is NULL or num[%u] is zero or num is greater than %d", num,
        MAX_SOCKET_NUM), ConverReturnCode(SOCKET_OP, -EINVAL));

    for (i = 0; i < num; i++) {
        socketHandle = (struct ra_socket_handle *)conn[i].socket_handle;
        if (socketHandle == NULL || socketHandle->socket_ops == NULL ||
            socketHandle->socket_ops->ra_socket_listen_start == NULL) {
            hccp_err("[listen_start][ra_socket]socket_handle or func is NULL");
            return ConverReturnCode(SOCKET_OP, -EINVAL);
        }
        phyId = socketHandle->rdev_info.phy_id;
        CHK_PRT_RETURN(phyId >= RA_MAX_PHY_ID_NUM,
            hccp_err("[listen_start][ra_socket]phy_id(%u) must smaller than %u", phyId, RA_MAX_PHY_ID_NUM),
            ConverReturnCode(SOCKET_OP, -EINVAL));

        ret = RaInetPton(socketHandle->rdev_info.family, socketHandle->rdev_info.local_ip, localIp, MAX_IP_LEN);
        CHK_PRT_RETURN(ret, hccp_err("[listen_start][ra_socket]ra_inet_pton for server_ip failed, ret(%d)", ret),
            ConverReturnCode(SOCKET_OP, ret));

        hccp_run_info("Input parameters: [%u]th, phyId[%u], localIp[%s], port[%u]",
            i, phyId, localIp, conn[i].port);
    }

    ret = socketHandle->socket_ops->ra_socket_listen_start(phyId, conn, num);
    return ConverReturnCode(SOCKET_OP, ret);
}

HCCP_ATTRI_VISI_DEF int RaSocketListenStop(struct socket_listen_info_t conn[], unsigned int num)
{
    struct ra_socket_handle *socketHandle = NULL;
    char localIp[MAX_IP_LEN] = {0};
    unsigned int phyId;
    unsigned int i;
    int ret;

    CHK_PRT_RETURN(conn == NULL || num == 0 || num > MAX_SOCKET_NUM,
        hccp_err("[listen_stop][ra_socket]conn is NULL or num[%u] is zero or num is greater than %d", num,
        MAX_SOCKET_NUM), ConverReturnCode(SOCKET_OP, -EINVAL));

    for (i = 0; i < num; i++) {
        socketHandle = (struct ra_socket_handle *)conn[i].socket_handle;
        if (socketHandle == NULL || socketHandle->socket_ops == NULL ||
            socketHandle->socket_ops->ra_socket_listen_stop == NULL) {
            hccp_err("[listen_stop][ra_socket]socket_handle or func is NULL");
            return ConverReturnCode(SOCKET_OP, -EINVAL);
        }

        phyId = socketHandle->rdev_info.phy_id;
        CHK_PRT_RETURN(phyId >= RA_MAX_PHY_ID_NUM,
            hccp_err("[listen_stop][ra_socket]phy_id(%u) must smaller than %u", phyId, RA_MAX_PHY_ID_NUM),
            ConverReturnCode(SOCKET_OP, -EINVAL));

        ret = RaInetPton(socketHandle->rdev_info.family, socketHandle->rdev_info.local_ip, localIp, MAX_IP_LEN);
        CHK_PRT_RETURN(ret, hccp_err("[listen_stop][ra_socket]ra_inet_pton for server_ip failed, ret(%d)", ret),
            ConverReturnCode(SOCKET_OP, ret));

        hccp_run_info("Input parameters: [%u]th, phyId[%u], localIp[%s]", i, phyId, localIp);
    }

    ret = socketHandle->socket_ops->ra_socket_listen_stop(phyId, conn, num);
    return ConverReturnCode(SOCKET_OP, ret);
}

HCCP_ATTRI_VISI_DEF int RaGetSockets(unsigned int role, struct socket_info_t conn[], unsigned int num,
    unsigned int *connectedNum)
{
    unsigned int i;
    struct ra_socket_handle *socketHandle = NULL;
    char localIp[MAX_IP_LEN] = {0};
    char remoteIp[MAX_IP_LEN] = {0};
    int ret;
    unsigned int phyId;

    CHK_PRT_RETURN(conn == NULL || connectedNum == NULL || num == 0 || num > MAX_SOCKET_NUM,
        hccp_err("[get][ra_socket]conn or connected_num is NULL or num[%u] is zero or num greater than %d", num,
        MAX_SOCKET_NUM), ConverReturnCode(SOCKET_OP, -EINVAL));

    for (i = 0; i < num; i++) {
        socketHandle = (struct ra_socket_handle *)conn[i].socket_handle;
        if (socketHandle == NULL || socketHandle->socket_ops == NULL ||
            socketHandle->socket_ops->ra_get_sockets == NULL) {
            hccp_err("[get][ra_socket]socket_handle or func is NULL");
            return ConverReturnCode(SOCKET_OP, -EINVAL);
        }

        phyId = socketHandle->rdev_info.phy_id;
        CHK_PRT_RETURN(phyId >= RA_MAX_PHY_ID_NUM,
            hccp_err("[get][ra_socket]phy_id(%u) must smaller than %u", phyId, RA_MAX_PHY_ID_NUM),
            ConverReturnCode(SOCKET_OP, -EINVAL));

        ret = RaInetPton(socketHandle->rdev_info.family, socketHandle->rdev_info.local_ip, localIp, MAX_IP_LEN);
        CHK_PRT_RETURN(ret, hccp_err("[get][ra_socket]ra_inet_pton for local_ip failed, ret(%d)", ret),
            ConverReturnCode(SOCKET_OP, ret));

        ret = RaInetPton(socketHandle->rdev_info.family, conn[i].remote_ip, remoteIp, MAX_IP_LEN);
        CHK_PRT_RETURN(ret, hccp_err("[get][ra_socket]ra_inet_pton for remote_ip failed, ret(%d)", ret),
            ConverReturnCode(SOCKET_OP, ret));
    }

    ret = socketHandle->socket_ops->ra_get_sockets(phyId, role, conn, num);
    if (ret >= 0) {
        *connectedNum = (unsigned int)ret;
        return 0;
    }

    *connectedNum = 0;
    return ConverReturnCode(SOCKET_OP, ret);
}

HCCP_ATTRI_VISI_DEF int RaSocketRecv(const void *fdHandle, void *data, unsigned long long size,
    unsigned long long *receivedSize)
{
    int ret;
    unsigned int phyId;
    const struct ra_socket_handle *socketHandleTmp = NULL;
    const struct socket_hdc_info *fdHandleTmp = (const struct socket_hdc_info *)fdHandle;

    CHK_PRT_RETURN(fdHandle == NULL || data == NULL || size == 0 || receivedSize == NULL,
        hccp_err("[recv][ra_socket]fd_handle or data or received_size is NULL or size[%llu] is 0", size),
        ConverReturnCode(SOCKET_OP, -EINVAL));

    socketHandleTmp = (const struct ra_socket_handle *)(fdHandleTmp->socket_handle);
    if (socketHandleTmp == NULL || socketHandleTmp->socket_ops == NULL ||
        socketHandleTmp->socket_ops->ra_socket_recv == NULL) {
        hccp_err("[recv][ra_socket]socket_handle_tmp or func is NULL");
        return ConverReturnCode(SOCKET_OP, -EINVAL);
    }
    phyId = socketHandleTmp->rdev_info.phy_id;
    CHK_PRT_RETURN(phyId >= RA_MAX_PHY_ID_NUM,
        hccp_err("[recv][ra_socket]phy_id(%u) must smaller than %u", phyId, RA_MAX_PHY_ID_NUM),
        ConverReturnCode(SOCKET_OP, -EINVAL));

    ret = socketHandleTmp->socket_ops->ra_socket_recv(phyId, fdHandle, data, size);
    if (ret > 0) {
        *receivedSize = (unsigned long long)(unsigned int)ret;
        return 0;
    } else if (ret == 0) {
        hccp_warn("[recv][ra_socket]socket has been closed. received_size is 0");
        ret = -ESOCKCLOSED;
    }

    *receivedSize = 0;
    return ConverReturnCode(SOCKET_OP, ret);
}

HCCP_ATTRI_VISI_DEF int RaSocketSend(const void *fdHandle, const void *data, unsigned long long size,
    unsigned long long *sentSize)
{
    int ret;
    unsigned int phyId;
    const struct ra_socket_handle *socketHandleTmp = NULL;
    const struct socket_hdc_info *fdHandleTmp = (const struct socket_hdc_info *)fdHandle;

    CHK_PRT_RETURN(fdHandle == NULL || data == NULL || sentSize == NULL || size == 0,
        hccp_err("[send][ra_socket]fd_handle or data or sent_size is NULL or size[%llu] is 0", size),
        ConverReturnCode(SOCKET_OP, -EINVAL));

    socketHandleTmp = (const struct ra_socket_handle *)(fdHandleTmp->socket_handle);
    if (socketHandleTmp == NULL || socketHandleTmp->socket_ops == NULL ||
        socketHandleTmp->socket_ops->ra_socket_send == NULL) {
        hccp_err("[send][ra_socket]socket_handle_tmp or func is NULL");
        return ConverReturnCode(SOCKET_OP, -EINVAL);
    }

    phyId = socketHandleTmp->rdev_info.phy_id;
    CHK_PRT_RETURN(phyId >= RA_MAX_PHY_ID_NUM,
        hccp_err("[send][ra_socket]phy_id(%u) must smaller than %u", phyId, RA_MAX_PHY_ID_NUM),
        ConverReturnCode(SOCKET_OP, -EINVAL));

    ret = socketHandleTmp->socket_ops->ra_socket_send(phyId, fdHandle, data, size);
    if (ret > 0) {
        *sentSize = (unsigned long long)(unsigned int)ret;
        return 0;
    } else if (ret == 0) {
        hccp_warn("[send][ra_socket]socket has been closed. sent_size is 0");
        ret = -ESOCKCLOSED;
    }

    *sentSize = 0;
    return ConverReturnCode(SOCKET_OP, ret);
}

HCCP_ATTRI_VISI_DEF int RaEpollCtlAdd(const void *fdHandle, enum RaEpollEvent event)
{
    CHK_PRT_RETURN(fdHandle == NULL, hccp_err("[ra_epoll_ctl_add]fd_handle is NULL"),
        ConverReturnCode(SOCKET_OP, -EINVAL));

    CHK_PRT_RETURN(event != RA_EPOLLIN && event != RA_EPOLLONESHOT,
        hccp_err("[ra_epoll_ctl_add]wrong event, only RA_EPOLLIN and RA_EPOLLONESHOT are supported."),
        ConverReturnCode(SOCKET_OP, -EINVAL));

    int ret = RaPeerEpollCtlAdd(fdHandle, event);
    CHK_PRT_RETURN(ret, hccp_err("[ra_epoll_ctl_add]ra_peer_epoll_ctl_add failed ret(%d)", ret),
        ConverReturnCode(SOCKET_OP, ret));

    return 0;
}

HCCP_ATTRI_VISI_DEF int RaEpollCtlMod(const void *fdHandle, enum RaEpollEvent event)
{
    CHK_PRT_RETURN(fdHandle == NULL, hccp_err("[ra_epoll_ctl_mod]fd_handle is NULL"),
        ConverReturnCode(SOCKET_OP, -EINVAL));

    CHK_PRT_RETURN(event != RA_EPOLLIN && event != RA_EPOLLONESHOT,
        hccp_err("[ra_epoll_ctl_mod]wrong event, only RA_EPOLLIN and RA_EPOLLONESHOT are supported."),
        ConverReturnCode(SOCKET_OP, -EINVAL));

    int ret = RaPeerEpollCtlMod(fdHandle, event);
    CHK_PRT_RETURN(ret, hccp_err("[ra_epoll_ctl_mod]ra_peer_epoll_ctl_mod failed ret(%d)", ret),
        ConverReturnCode(SOCKET_OP, ret));

    return 0;
}

HCCP_ATTRI_VISI_DEF int RaEpollCtlDel(const void *fdHandle)
{
    CHK_PRT_RETURN(fdHandle == NULL, hccp_err("[ra_epoll_ctl_del]fd_handle is NULL"),
        ConverReturnCode(SOCKET_OP, -EINVAL));

    int ret = RaPeerEpollCtlDel(fdHandle);
    CHK_PRT_RETURN(ret, hccp_err("[ra_epoll_ctl_del]ra_peer_epoll_ctl_del failed ret(%d)", ret),
        ConverReturnCode(SOCKET_OP, ret));

    return 0;
}

HCCP_ATTRI_VISI_DEF int RaSetTcpRecvCallback(const void *socketHandle, const void *callback)
{
    const struct ra_socket_handle *socketHandleTmp = (const struct ra_socket_handle *)socketHandle;
    unsigned int phyId;

    CHK_PRT_RETURN(socketHandleTmp == NULL || callback == NULL,
        hccp_err("[ra_socket]socket_handle is NULL or callback is NULL"),
        ConverReturnCode(SOCKET_OP, -EINVAL));

    phyId = socketHandleTmp->rdev_info.phy_id;
    CHK_PRT_RETURN(phyId >= RA_MAX_PHY_ID_NUM,
        hccp_err("[ra_socket]phy_id(%u) must smaller than %u", phyId, RA_MAX_PHY_ID_NUM),
        ConverReturnCode(SOCKET_OP, -EINVAL));

    RaPeerSetTcpRecvCallback(phyId, callback);

    return 0;
}

HCCP_ATTRI_VISI_DEF int RaGetTsqpDepth(void *rdevHandle, unsigned int *tempDepth, unsigned int *qpNum)
{
    struct ra_rdma_handle *rdmaHandleTmp = (struct ra_rdma_handle *)rdevHandle;
    unsigned int phyId;
    int ret;

    CHK_PRT_RETURN(rdevHandle == NULL || rdmaHandleTmp->rdma_ops == NULL ||
        rdmaHandleTmp->rdma_ops->ra_get_tsqp_depth == NULL,
        hccp_err("[get][ra_tsqp_depth]rdev_handle is NULL or func is NULL"), ConverReturnCode(RDMA_OP, -EINVAL));

    CHK_PRT_RETURN(tempDepth == NULL || qpNum == NULL, hccp_err("[get][ra_tsqp_depth]temp_depth or qp_num is NULL"),
        ConverReturnCode(RDMA_OP, -EINVAL));

    phyId = rdmaHandleTmp->rdev_info.phy_id;
    CHK_PRT_RETURN(phyId >= RA_MAX_PHY_ID_NUM,
        hccp_err("[get][ra_tsqp_depth]phy_id(%u) is invalid! it must greater or equal to 0 and less than %d!",
        phyId, RA_MAX_PHY_ID_NUM), ConverReturnCode(RDMA_OP, -EINVAL));

    hccp_info("Input parameters: phy_id[%u], rdevIndex[%u]", phyId, rdmaHandleTmp->rdev_index);

    ret = rdmaHandleTmp->rdma_ops->ra_get_tsqp_depth(rdmaHandleTmp, tempDepth, qpNum);
    return ConverReturnCode(RDMA_OP, ret);
}

HCCP_ATTRI_VISI_DEF int RaSetTsqpDepth(void *rdevHandle, unsigned int tempDepth, unsigned int *qpNum)
{
    struct ra_rdma_handle *rdmaHandleTmp = (struct ra_rdma_handle *)rdevHandle;
    unsigned int phyId;
    int ret;

    CHK_PRT_RETURN(rdevHandle == NULL || rdmaHandleTmp->rdma_ops == NULL ||
        rdmaHandleTmp->rdma_ops->ra_set_tsqp_depth == NULL,
        hccp_err("[set][ra_tsqp_depth]rdev_handle is NULL or func is NULL"), ConverReturnCode(RDMA_OP, -EINVAL));

    CHK_PRT_RETURN(qpNum == NULL, hccp_err("[set][ra_tsqp_depth]qp_num is NULL"),
        ConverReturnCode(RDMA_OP, -EINVAL));

    CHK_PRT_RETURN(tempDepth < RA_MIN_TEMPTH_DEPTH || tempDepth > RA_MAX_TEMPTH_DEPTH,
        hccp_err("[set][ra_tsqp_depth]param error! temp_depth(%u) can not smaller than %d or bigger than %d",
        tempDepth, RA_MIN_TEMPTH_DEPTH, RA_MAX_TEMPTH_DEPTH), ConverReturnCode(RDMA_OP, -EINVAL));

    phyId = rdmaHandleTmp->rdev_info.phy_id;
    CHK_PRT_RETURN(phyId >= RA_MAX_PHY_ID_NUM,
        hccp_err("[set][ra_tsqp_depth]phy_id(%u) is invalid! it must greater or equal to 0 and less than %d!",
        phyId, RA_MAX_PHY_ID_NUM), ConverReturnCode(RDMA_OP, -EINVAL));

    hccp_run_info("Input parameters: phy_id[%u], rdevIndex[%u], tempDepth[%u]",
        phyId, rdmaHandleTmp->rdev_index, tempDepth);

    ret = rdmaHandleTmp->rdma_ops->ra_set_tsqp_depth(rdmaHandleTmp, tempDepth, qpNum);
    return ConverReturnCode(RDMA_OP, ret);
}

HCCP_ATTRI_VISI_DEF int RaQpCreate(void *rdevHandle, int flag, int qpMode, void **qpHandle)
{
    struct ra_rdma_handle *rdmaHandleTmp = (struct ra_rdma_handle *)rdevHandle;
    unsigned int phyId;
    int ret;

    CHK_PRT_RETURN(rdevHandle == NULL || rdmaHandleTmp->rdma_ops == NULL ||
        rdmaHandleTmp->rdma_ops->ra_qp_create == NULL,
        hccp_err("[create][ra_qp]rdev_handle is NULL or func is NULL"), ConverReturnCode(RDMA_OP, -EINVAL));

    CHK_PRT_RETURN(qpHandle == NULL, hccp_err("[create][ra_qp]qp_handle is NULL"),
        ConverReturnCode(RDMA_OP, -EINVAL));

    /* 0 means RC mode */
    CHK_PRT_RETURN(flag != 0, hccp_err("[create][ra_qp]The flag(%d) is invalid, expect 0", flag),
        ConverReturnCode(RDMA_OP, -EINVAL));

    phyId = rdmaHandleTmp->rdev_info.phy_id;
    CHK_PRT_RETURN(phyId >= RA_MAX_PHY_ID_NUM,
        hccp_err("[create][ra_qp]phy_id(%u) must greater or equal to 0 and less than %d!", phyId, RA_MAX_PHY_ID_NUM),
        ConverReturnCode(RDMA_OP, -EINVAL));

    CHK_PRT_RETURN(qpMode < 0 || qpMode >= RA_RS_ERR_QP_MODE,
        hccp_err("[create][ra_qp]QP mode(%d) must greater or equal to 0 and less than %d", qpMode, RA_RS_ERR_QP_MODE),
        ConverReturnCode(RDMA_OP, -EINVAL));

    hccp_run_info("Input parameters: phy_id[%u], flag[%d] qpMode [%d]", phyId, flag, qpMode);

    ret = rdmaHandleTmp->rdma_ops->ra_qp_create(rdmaHandleTmp, flag, qpMode, qpHandle);
    CHK_PRT_RETURN(ret != 0 || *qpHandle == NULL,
        hccp_err("[create][ra_qp]create qp failed, ret(%d) phyId(%u)", ret, phyId),
        ConverReturnCode(RDMA_OP, ret));

    return 0;
}

HCCP_ATTRI_VISI_DEF int RaQpCreateWithAttrs(void *rdevHandle, struct qp_ext_attrs *extAttrs, void **qpHandle)
{
    struct ra_rdma_handle *rdmaHandleTmp = (struct ra_rdma_handle *)rdevHandle;
    unsigned int phyId;
    int ret;

    CHK_PRT_RETURN(rdevHandle == NULL || rdmaHandleTmp->rdma_ops == NULL ||
        rdmaHandleTmp->rdma_ops->ra_qp_create_with_attrs == NULL,
        hccp_err("[create][ra_qp_with_attrs]rdev_handle is NULL or func is NULL"),
        ConverReturnCode(RDMA_OP, -EINVAL));

    CHK_PRT_RETURN(qpHandle == NULL, hccp_err("[create][ra_qp_with_attrs]qp_handle is NULL"),
        ConverReturnCode(RDMA_OP, -EINVAL));

    CHK_PRT_RETURN(extAttrs == NULL, hccp_err("[create][ra_qp_with_attrs]ext_attrs is NULL"),
        ConverReturnCode(RDMA_OP, -EINVAL));

    CHK_PRT_RETURN(extAttrs->version != QP_CREATE_WITH_ATTR_VERSION,
        hccp_err("[create][ra_qp_with_attrs]attr version[%d] mismatch, expect [%d]", extAttrs->version,
        QP_CREATE_WITH_ATTR_VERSION), ConverReturnCode(RDMA_OP, -EINVAL));

    CHK_PRT_RETURN(extAttrs->qp_mode < 0 || extAttrs->qp_mode >= RA_RS_ERR_QP_MODE,
        hccp_err("[create][ra_qp_with_attrs]QP mode[%d] must greater or equal to 0 and less than %d",
        extAttrs->qp_mode, RA_RS_ERR_QP_MODE), ConverReturnCode(RDMA_OP, -EINVAL));
    // no need and disallow to set data_plane_flag, set it to default value 0
    extAttrs->data_plane_flag.value = 0;

    phyId = rdmaHandleTmp->rdev_info.phy_id;
    CHK_PRT_RETURN(phyId >= RA_MAX_PHY_ID_NUM,
        hccp_err("[create][ra_qp_with_attrs]phy_id(%u) must greater or equal to 0 and less than %d!", phyId,
        RA_MAX_PHY_ID_NUM), ConverReturnCode(RDMA_OP, -EINVAL));

    hccp_run_info("Input parameters: phy_id[%u] qp_mode[%d] cq_attr{%d,%d,%d,%d} qp_attr.cap{%u,%u,%u,%u,%u}"\
        " qp_type[%u] sq_sig_all[%d], cnt[%u]", phyId, extAttrs->qp_mode,
        extAttrs->cq_attr.send_cq_depth, extAttrs->cq_attr.send_cq_comp_vector,
        extAttrs->cq_attr.recv_cq_depth, extAttrs->cq_attr.recv_cq_comp_vector,
        extAttrs->qp_attr.cap.max_send_wr, extAttrs->qp_attr.cap.max_recv_wr,
        extAttrs->qp_attr.cap.max_send_sge, extAttrs->qp_attr.cap.max_recv_sge,
        extAttrs->qp_attr.cap.max_inline_data, extAttrs->qp_attr.qp_type, extAttrs->qp_attr.sq_sig_all,
        rdmaHandleTmp->qp_cnt);

    rdmaHandleTmp->qp_cnt++;
    ret = rdmaHandleTmp->rdma_ops->ra_qp_create_with_attrs(rdmaHandleTmp, extAttrs, qpHandle);
    CHK_PRT_RETURN(ret != 0 || *qpHandle == NULL,
        hccp_err("[create][ra_qp_with_attrs]create qp failed, ret(%d) phyId(%u)", ret, phyId),
        ConverReturnCode(RDMA_OP, ret));

    return 0;
}

HCCP_ATTRI_VISI_DEF int RaAiQpCreate(void *rdevHandle, struct qp_ext_attrs *attrs, struct ai_qp_info *info,
    void **qpHandle)
{
    struct ra_rdma_handle *rdmaHandleTmp = (struct ra_rdma_handle *)rdevHandle;
    unsigned int interfaceVersion = 0;
    unsigned int phyId;
    int ret;

    CHK_PRT_RETURN(rdevHandle == NULL || rdmaHandleTmp->rdma_ops == NULL ||
        rdmaHandleTmp->rdma_ops->ra_ai_qp_create == NULL ||
        rdmaHandleTmp->rdma_ops->ra_ai_qp_create_with_attrs == NULL,
        hccp_err("[create][ra_ai_qp]rdev_handle is NULL or func is NULL"), ConverReturnCode(RDMA_OP, -EINVAL));

    CHK_PRT_RETURN(info == NULL || qpHandle == NULL, hccp_err("[create][ra_ai_qp]info is NULL or qp_handle is NULL"),
        ConverReturnCode(RDMA_OP, -EINVAL));

    CHK_PRT_RETURN(attrs == NULL, hccp_err("[create][ra_ai_qp]attrs is NULL"), ConverReturnCode(RDMA_OP, -EINVAL));

    CHK_PRT_RETURN(attrs->version != QP_CREATE_WITH_ATTR_VERSION,
        hccp_err("[create][ra_ai_qp]attr version[%d] mismatch, expect [%d]", attrs->version,
        QP_CREATE_WITH_ATTR_VERSION), ConverReturnCode(RDMA_OP, -EINVAL));

    CHK_PRT_RETURN(attrs->qp_mode < 0 || attrs->qp_mode >= RA_RS_ERR_QP_MODE,
        hccp_err("[create][ra_ai_qp]QP mode[%d] must greater or equal to 0 and less than %d", attrs->qp_mode,
        RA_RS_ERR_QP_MODE), ConverReturnCode(RDMA_OP, -EINVAL));

    phyId = rdmaHandleTmp->rdev_info.phy_id;
    CHK_PRT_RETURN(phyId >= RA_MAX_PHY_ID_NUM,
        hccp_err("[create][ra_ai_qp]phy_id(%u) must greater or equal to 0 and less than %d!", phyId,
        RA_MAX_PHY_ID_NUM), ConverReturnCode(RDMA_OP, -EINVAL));

    hccp_run_info("Input parameters: phy_id[%u] qp_mode[%d] cq_attr{%d,%d,%d,%d} cq_cstm[%d] "
        "qp_attr.cap{%u,%u,%u,%u,%u} qp_type[%u] sq_sig_all[%d]", phyId, attrs->qp_mode,
        attrs->cq_attr.send_cq_depth, attrs->cq_attr.send_cq_comp_vector,
        attrs->cq_attr.recv_cq_depth, attrs->cq_attr.recv_cq_comp_vector, attrs->data_plane_flag.bs.cq_cstm,
        attrs->qp_attr.cap.max_send_wr, attrs->qp_attr.cap.max_recv_wr,
        attrs->qp_attr.cap.max_send_sge, attrs->qp_attr.cap.max_recv_sge,
        attrs->qp_attr.cap.max_inline_data, attrs->qp_attr.qp_type, attrs->qp_attr.sq_sig_all);

    ret = RaGetInterfaceVersion(phyId, RA_RS_AI_QP_CREATE_WITH_ATTRS, &interfaceVersion);
    if (ret == 0 && interfaceVersion >= RA_RS_OPCODE_BASE_VERSION) {
        ret = rdmaHandleTmp->rdma_ops->ra_ai_qp_create_with_attrs(rdmaHandleTmp, attrs, info, qpHandle);
    } else {
        // origin procedure: not support to process data_plane_flag.bs.cq_cstm
        ret = rdmaHandleTmp->rdma_ops->ra_ai_qp_create(rdmaHandleTmp, attrs, info, qpHandle);
    }
    CHK_PRT_RETURN(ret != 0 || *qpHandle == NULL, hccp_err("[create][ra_ai_qp]create qp failed, ret(%d) phyId(%u)",
        ret, phyId), ConverReturnCode(RDMA_OP, ret));

    return 0;
}

HCCP_ATTRI_VISI_DEF int RaTypicalQpCreate(void *rdevHandle, int flag, int qpMode, struct typical_qp *qpInfo,
    void **qpHandle)
{
    struct ra_rdma_handle *rdmaHandleTmp = (struct ra_rdma_handle *)rdevHandle;
    unsigned int phyId;
    int ret;

    CHK_PRT_RETURN(rdevHandle == NULL || rdmaHandleTmp->rdma_ops == NULL ||
        rdmaHandleTmp->rdma_ops->ra_typical_qp_create == NULL,
        hccp_err("[create][ra_typical_qp]rdev_handle is NULL or func is NULL"), ConverReturnCode(RDMA_OP, -EINVAL));

    CHK_PRT_RETURN(qpHandle == NULL, hccp_err("[create][ra_typical_qp]qp_handle is NULL"),
        ConverReturnCode(RDMA_OP, -EINVAL));

    CHK_PRT_RETURN(qpInfo == NULL, hccp_err("[create][ra_typical_qp]qp_info is NULL"),
        ConverReturnCode(RDMA_OP, -EINVAL));

    CHK_PRT_RETURN(flag != 0, hccp_err("[create][ra_typical_qp]The flag(%d) is invalid, expect 0", flag),
        ConverReturnCode(RDMA_OP, -EINVAL));

    phyId = rdmaHandleTmp->rdev_info.phy_id;
    CHK_PRT_RETURN(phyId >= RA_MAX_PHY_ID_NUM,
        hccp_err("[create][ra_typical_qp]phy_id(%u) must greater or equal to 0 and less than %d!", phyId,
        RA_MAX_PHY_ID_NUM), ConverReturnCode(RDMA_OP, -EINVAL));

    CHK_PRT_RETURN(qpMode < 0 || qpMode >= RA_RS_ERR_QP_MODE,
        hccp_err("[create][ra_typical_qp]QP mode(%d) must greater or equal to 0 and less than %d", qpMode,
        RA_RS_ERR_QP_MODE), ConverReturnCode(RDMA_OP, -EINVAL));

    hccp_run_info("Input parameters: phy_id[%u], flag[%d] qpMode[%d]", phyId, flag, qpMode);

    ret = rdmaHandleTmp->rdma_ops->ra_typical_qp_create(rdmaHandleTmp, flag, qpMode, qpInfo, qpHandle);
    CHK_PRT_RETURN(ret != 0 || *qpHandle == NULL,
        hccp_err("[create][ra_typical_qp]create qp failed, ret(%d) phyId(%u)", ret, phyId),
        ConverReturnCode(RDMA_OP, ret));

    return 0;
}

HCCP_ATTRI_VISI_DEF int RaLoopbackQpCreate(void *rdevHandle, struct loopback_qp_pair *qpPair, void **qpHandle)
{
    struct ra_rdma_handle *rdmaHandleTmp = (struct ra_rdma_handle *)rdevHandle;
    unsigned int phyId;
    int ret;

    CHK_PRT_RETURN(rdmaHandleTmp == NULL || rdmaHandleTmp->rdma_ops == NULL ||
        rdmaHandleTmp->rdma_ops->ra_loopback_qp_create == NULL,
        hccp_err("[create][ra_loopback_qp]rdev_handle is NULL or func is NULL"), ConverReturnCode(RDMA_OP, -EINVAL));

    CHK_PRT_RETURN(qpPair == NULL, hccp_err("[create][ra_loopback_qp]qp_pair is NULL"),
        ConverReturnCode(RDMA_OP, -EINVAL));

    CHK_PRT_RETURN(qpHandle == NULL, hccp_err("[create][ra_loopback_qp]qp_handle is NULL"),
        ConverReturnCode(RDMA_OP, -EINVAL));

    phyId = rdmaHandleTmp->rdev_info.phy_id;
    CHK_PRT_RETURN(phyId >= RA_MAX_PHY_ID_NUM,
        hccp_err("[create][ra_loopback_qp]phy_id(%u) must greater or equal to 0 and less than %d!", phyId, RA_MAX_PHY_ID_NUM),
        ConverReturnCode(RDMA_OP, -EINVAL));

    hccp_run_info("Input parameters: phy_id[%u]", phyId);
    ret = rdmaHandleTmp->rdma_ops->ra_loopback_qp_create(rdmaHandleTmp, qpPair, qpHandle);
    CHK_PRT_RETURN(ret != 0 || *qpHandle == NULL,
        hccp_err("[create][ra_loopback_qp]create qp failed, ret(%d) phyId(%u)", ret, phyId),
        ConverReturnCode(RDMA_OP, ret));

    return 0;
}

HCCP_ATTRI_VISI_DEF int RaQpDestroy(void *qpHandle)
{
    struct ra_qp_handle *raQpHandle = (struct ra_qp_handle *)qpHandle;
    int ret;

    CHK_PRT_RETURN(qpHandle == NULL,
        hccp_err("[destroy][ra_qp]qp_handle is NULL"),
        ConverReturnCode(RDMA_OP, -EINVAL));

    CHK_PRT_RETURN(raQpHandle->rdma_ops == NULL || raQpHandle->rdma_ops->ra_qp_destroy == NULL,
        hccp_err("[destroy][ra_qp]rdma_ops is NULL or ra_qp_handle->rdma_ops->ra_qp_destroy is NULL, invalid"),
        ConverReturnCode(RDMA_OP, -EINVAL));

    hccp_run_info("Input parameters: qpn[%u], phyId[%u], rdevIndex[%u] qpMode[%d] flag[%d]",
        raQpHandle->qpn, raQpHandle->phy_id, raQpHandle->rdev_index, raQpHandle->qp_mode, raQpHandle->flag);

    ret = raQpHandle->rdma_ops->ra_qp_destroy(raQpHandle);
    return ConverReturnCode(RDMA_OP, ret);
}

HCCP_ATTRI_VISI_DEF int RaQpConnectAsync(void *qpHandle, const void *fdHandle)
{
    struct ra_qp_handle *raQpHandle = (struct ra_qp_handle *)qpHandle;
    int ret;

    CHK_PRT_RETURN(qpHandle == NULL || fdHandle == NULL,
        hccp_err("[connect_async][ra_qp]ra_qp_handle or fd_handle is NULL, para error!"),
        ConverReturnCode(RDMA_OP, -EINVAL));

    CHK_PRT_RETURN(raQpHandle->rdma_ops == NULL || raQpHandle->rdma_ops->ra_qp_connect_async == NULL,
        hccp_err("[connect_async][ra_qp]rdma_ops or ra_qp_handle->rdma_ops->ra_qp_connect_async is NULL, invalid"),
        ConverReturnCode(RDMA_OP, -EINVAL));

    ret = raQpHandle->rdma_ops->ra_qp_connect_async(raQpHandle, fdHandle);
    return ConverReturnCode(RDMA_OP, ret);
}

HCCP_ATTRI_VISI_DEF int RaGetQpStatus(void *qpHandle, int *status)
{
    struct ra_qp_handle *raQpHandle = (struct ra_qp_handle *)qpHandle;
    int ret;

    CHK_PRT_RETURN(qpHandle == NULL || status == NULL,
        hccp_err("[get][ra_qp_status]ra_qp_handle or status is NULL, para error!"),
        ConverReturnCode(RDMA_OP, -EINVAL));

    CHK_PRT_RETURN(raQpHandle->rdma_ops == NULL || raQpHandle->rdma_ops->ra_get_qp_status == NULL,
        hccp_err("[get][ra_qp_status]rdma_ops is NULL or ra_qp_handle->rdma_ops->ra_get_qp_status is NULL, invalid"),
        ConverReturnCode(RDMA_OP, -EINVAL));

    ret = raQpHandle->rdma_ops->ra_get_qp_status(raQpHandle, status);
    return ConverReturnCode(RDMA_OP, ret);
}

HCCP_ATTRI_VISI_DEF int RaMrReg(void *qpHandle, struct mr_info *info)
{
    struct ra_qp_handle *raQpHandle = (struct ra_qp_handle *)qpHandle;
    int ret;

    CHK_PRT_RETURN(qpHandle == NULL || info == NULL,
        hccp_err("[reg][ra_mr]qp_handle is NULL or info is NULL, para error!"),
        ConverReturnCode(RDMA_OP, -EINVAL));

    CHK_PRT_RETURN(raQpHandle->rdma_ops == NULL || raQpHandle->rdma_ops->ra_mr_reg == NULL,
        hccp_err("[reg][ra_mr]rdma_ops is NULL or ra_qp_handle->rdma_ops->ra_mr_reg is NULL, invalid"),
        ConverReturnCode(RDMA_OP, -EINVAL));

    ret = raQpHandle->rdma_ops->ra_mr_reg(raQpHandle, info);
    return ConverReturnCode(RDMA_OP, ret);
}

HCCP_ATTRI_VISI_DEF int RaMrDereg(void *qpHandle, struct mr_info *info)
{
    struct ra_qp_handle *raQpHandle = (struct ra_qp_handle *)qpHandle;
    int ret;

    CHK_PRT_RETURN(qpHandle == NULL || info == NULL || info->addr == NULL,
        hccp_err("[dereg][ra_mr]qp_handle or info or addr is NULL, para error!"),
        ConverReturnCode(RDMA_OP, -EINVAL));

    CHK_PRT_RETURN(raQpHandle->rdma_ops == NULL || raQpHandle->rdma_ops->ra_mr_dereg == NULL,
        hccp_err("[dereg][ra_mr]rdma_ops is NULL or ra_qp_handle->rdma_ops->ra_mr_dereg is NULL, invalid"),
        ConverReturnCode(RDMA_OP, -EINVAL));

    ret = raQpHandle->rdma_ops->ra_mr_dereg(raQpHandle, info);
    return ConverReturnCode(RDMA_OP, ret);
}

HCCP_ATTRI_VISI_DEF int RaSendWr(void *qpHandle, struct send_wr *wr, struct send_wr_rsp *opRsp)
{
    struct ra_qp_handle *raQpHandle = (struct ra_qp_handle *)qpHandle;
    int ret;

    CHK_PRT_RETURN(qpHandle == NULL || wr == NULL || wr->buf_list == NULL || opRsp == NULL,
        hccp_err("[send][ra_wr]qp_handle or wr or buf_list or op_rsp is NULL, para error!"),
        ConverReturnCode(RDMA_OP, -EINVAL));

    CHK_PRT_RETURN(wr->buf_list->len > MAX_SG_LIST_LEN_MAX,
        hccp_err("[send][ra_wr]sg list len is more than 2G, len(%u)", wr->buf_list->len),
        ConverReturnCode(RDMA_OP, -EINVAL));

    CHK_PRT_RETURN(raQpHandle->rdma_ops == NULL || raQpHandle->rdma_ops->ra_send_wr == NULL,
        hccp_err("[send][ra_wr]rdma_ops is NULL or ra_qp_handle->rdma_ops->ra_send_wr is NULL, invalid"),
        ConverReturnCode(RDMA_OP, -EINVAL));

    ret = raQpHandle->rdma_ops->ra_send_wr(raQpHandle, wr, opRsp);
    RaRdevIncSendWrNum();
    return ConverReturnCode(RDMA_OP, ret);
}

HCCP_ATTRI_VISI_DEF int RaSendWrV2(void *qpHandle, struct send_wr_v2 *wr, struct send_wr_rsp *opRsp)
{
    struct ra_qp_handle *raQpHandle = (struct ra_qp_handle *)qpHandle;
    int ret;

    CHK_PRT_RETURN(qpHandle == NULL || wr == NULL || wr->buf_list == NULL || opRsp == NULL,
        hccp_err("[send][ra_wr]qp_handle or wr or buf_list or op_rsp is NULL, para error!"),
        ConverReturnCode(RDMA_OP, -EINVAL));

    CHK_PRT_RETURN(wr->buf_list->len > MAX_SG_LIST_LEN_MAX,
        hccp_err("[send][ra_wr]sg list len is more than 2G, len(%u)", wr->buf_list->len),
        ConverReturnCode(RDMA_OP, -EINVAL));

    CHK_PRT_RETURN(raQpHandle->rdma_ops == NULL || raQpHandle->rdma_ops->ra_send_wr_v2 == NULL,
        hccp_err("[send][ra_wr]rdma_ops is NULL or ra_qp_handle->rdma_ops->ra_send_wr_v2 is NULL, invalid"),
        ConverReturnCode(RDMA_OP, -EINVAL));

    ret = raQpHandle->rdma_ops->ra_send_wr_v2(raQpHandle, wr, opRsp);
    RaRdevIncSendWrNum();
    return ConverReturnCode(RDMA_OP, ret);
}

HCCP_ATTRI_VISI_DEF int RaSendWrlist(void *qpHandle, struct send_wrlist_data wr[], struct send_wr_rsp opRsp[],
    unsigned int sendNum, unsigned int *completeNum)
{
    int ret;
    unsigned int i;
    struct ra_qp_handle *raQpHandle = (struct ra_qp_handle *)qpHandle;
    struct wrlist_send_complete_num wrlistNum = {0};

    CHK_PRT_RETURN(qpHandle == NULL || wr == NULL || opRsp == NULL || sendNum == 0 || completeNum == NULL,
        hccp_err("[send][ra_wrlist]qp_handle or wr or op_rsp or complete_num is NULL or send_num is zero, para error!"),
        ConverReturnCode(RDMA_OP, -EINVAL));

    for (i = 0; i < sendNum; i++) {
        if (wr[i].mem_list.len > MAX_SG_LIST_LEN_MAX) {
            hccp_err("[send][ra_wrlist]sg list len is more than 2G, len(%u)", wr[i].mem_list.len);
            return ConverReturnCode(RDMA_OP, -EINVAL);
        }
    }

    CHK_PRT_RETURN(raQpHandle->rdma_ops == NULL || raQpHandle->rdma_ops->ra_send_wrlist == NULL,
        hccp_err("[send][ra_wrlist]rdma_ops is NULL or ra_qp_handle->rdma_ops->ra_send_wr is NULL, invalid"),
        ConverReturnCode(RDMA_OP, -EINVAL));

    wrlistNum.send_num = sendNum;
    wrlistNum.complete_num = completeNum;
    ret = raQpHandle->rdma_ops->ra_send_wrlist(raQpHandle, wr, opRsp, wrlistNum);
    return ConverReturnCode(RDMA_OP, ret);
}

HCCP_ATTRI_VISI_DEF int RaSendWrlistExt(void *qpHandle, struct send_wrlist_data_ext wr[],
    struct send_wr_rsp opRsp[], unsigned int sendNum, unsigned int *completeNum)
{
    struct ra_qp_handle *raQpHandle = (struct ra_qp_handle *)qpHandle;
    struct wrlist_send_complete_num wrlistNum = {0};
    unsigned int i;
    int ret;

    CHK_PRT_RETURN(qpHandle == NULL || wr == NULL || opRsp == NULL || sendNum == 0 || completeNum == NULL,
        hccp_err("[send][ra_wrlist]qp_handle or wr or op_rsp or complete_num is NULL"\
            "or send_num is zero, para error!"),
        ConverReturnCode(RDMA_OP, -EINVAL));

    for (i = 0; i < sendNum; i++) {
        if (wr[i].mem_list.len > MAX_SG_LIST_LEN_MAX) {
            hccp_err("[send][ra_wrlist]sg list len is more than 2G, len(%u)", wr[i].mem_list.len);
            return ConverReturnCode(RDMA_OP, -EINVAL);
        }
    }

    CHK_PRT_RETURN(raQpHandle->rdma_ops == NULL || raQpHandle->rdma_ops->ra_send_wrlist_ext == NULL,
        hccp_err("[send][ra_wrlist]rdma_ops is NULL or ra_qp_handle->rdma_ops->ra_send_wr is NULL, invalid"),
        ConverReturnCode(RDMA_OP, -EINVAL));

    wrlistNum.send_num = sendNum;
    wrlistNum.complete_num = completeNum;
    ret = raQpHandle->rdma_ops->ra_send_wrlist_ext(raQpHandle, wr, opRsp, wrlistNum);
    return ConverReturnCode(RDMA_OP, ret);
}

HCCP_ATTRI_VISI_DEF int RaGetNotifyBaseAddr(void *rdevHandle, unsigned long long *va, unsigned long long *size)
{
    struct ra_rdma_handle *rdevHandleTmp = (struct ra_rdma_handle *)rdevHandle;
    int ret;

    CHK_PRT_RETURN(rdevHandle == NULL || va == NULL || size == NULL,
        hccp_err("[get][ra_notify_base_addr]rdev_handle or va or size is NULL, invalid"),
        ConverReturnCode(RDMA_OP, -EINVAL));

    CHK_PRT_RETURN(rdevHandleTmp->rdma_ops == NULL || rdevHandleTmp->rdma_ops->ra_get_notify_base_addr == NULL,
        hccp_err("[get][ra_notify_base_addr]rdma_ops is NULL or ra_get_notify_base_addr is NULL, invalid"),
        ConverReturnCode(RDMA_OP, -EINVAL));

    ret = rdevHandleTmp->rdma_ops->ra_get_notify_base_addr(rdevHandleTmp, va, size);
    RaRdevSaveNotifyMr(rdevHandleTmp, ret, *va, *size);
    return ConverReturnCode(RDMA_OP, ret);
}

HCCP_ATTRI_VISI_DEF int RaGetNotifyMrInfo(void *rdevHandle, struct mr_info *info)
{
    struct ra_rdma_handle *rdevHandleTmp = (struct ra_rdma_handle *)rdevHandle;
    int ret;

    CHK_PRT_RETURN(rdevHandle == NULL || info == NULL,
        hccp_err("[get][ra_notify_mr_info]rdev_handle or info is NULL, invalid"),
        ConverReturnCode(RDMA_OP, -EINVAL));

    CHK_PRT_RETURN(rdevHandleTmp->rdma_ops == NULL || rdevHandleTmp->rdma_ops->ra_get_notify_mr_info == NULL,
        hccp_err("[get][ra_notify_mr_info]rdma_ops is NULL or ra_get_notify_mr_info is NULL, invalid"),
        ConverReturnCode(RDMA_OP, -EINVAL));

    ret = rdevHandleTmp->rdma_ops->ra_get_notify_mr_info(rdevHandleTmp, info);
    RaRdevSaveNotifyMr(rdevHandleTmp, ret, (uint64_t)(uintptr_t)info->addr, info->size);
    return ConverReturnCode(RDMA_OP, ret);
}

HCCP_ATTRI_VISI_DEF int RaSocketGetWhiteListStatus(unsigned int *enable)
{
    CHK_PRT_RETURN(enable == NULL, hccp_err("[get][ra_socket_white_list_status]white list switch enable is NULL"),
        ConverReturnCode(SOCKET_OP, -EINVAL));

    *enable = gWhiteListSwitch;
    hccp_info("white list status: enable[%u]", *enable);
    return 0;
}

HCCP_ATTRI_VISI_DEF int RaSocketSetWhiteListStatus(unsigned int enable)
{
    hccp_run_info("Input parameters: enable[%u]", enable);

    CHK_PRT_RETURN(enable != WHITE_LIST_DISABLE && enable != WHITE_LIST_ENABLE,
        hccp_err("[set][ra_socket_white_list_status]white list switch is invalid, enable(%u)", enable),
        ConverReturnCode(SOCKET_OP, -EINVAL));

    gWhiteListSwitch = enable;
    return 0;
}

HCCP_ATTRI_VISI_DEF int RaSocketWhiteListAdd(void *socketHandle, struct socket_wlist_info_t whiteList[],
    unsigned int num)
{
    struct ra_socket_handle *socketHandleTmp = NULL;
    char localIp[MAX_IP_LEN] = {0};
    unsigned int phyId;
    int ret;

    CHK_PRT_RETURN(whiteList == NULL || num > MAX_WLIST_NUM || num == 0,
        hccp_err("[add][ra_socket_white_list]white_list is NULL, or num(%u) > %u or = 0, invalid",
            num, MAX_WLIST_NUM),
        ConverReturnCode(SOCKET_OP, -EINVAL));

    socketHandleTmp = (struct ra_socket_handle *)socketHandle;
    if (socketHandleTmp == NULL || socketHandleTmp->socket_ops == NULL ||
        socketHandleTmp->socket_ops->ra_socket_white_list_add == NULL) {
        hccp_err("[add][ra_socket_white_list]socket_handle or func is NULL");
        return ConverReturnCode(SOCKET_OP, -EINVAL);
    }

    ret = RaInetPton(socketHandleTmp->rdev_info.family, socketHandleTmp->rdev_info.local_ip,
                       localIp, MAX_IP_LEN);
    CHK_PRT_RETURN(ret, hccp_err("[add][ra_socket_white_list]ra_inet_pton for local_ip failed, ret(%d)", ret),
        ConverReturnCode(SOCKET_OP, ret));

    phyId = socketHandleTmp->rdev_info.phy_id;
    CHK_PRT_RETURN(phyId >= RA_MAX_PHY_ID_NUM,
        hccp_err("[add][ra_socket_white_list]phy_id(%u) must smaller than %u", phyId, RA_MAX_PHY_ID_NUM),
        ConverReturnCode(SOCKET_OP, -EINVAL));

    hccp_info("Input parameters: phy_id[%u], localIp[%s], num[%u]", phyId, localIp, num);

    ret = socketHandleTmp->socket_ops->ra_socket_white_list_add(socketHandleTmp->rdev_info, whiteList, num);
    return ConverReturnCode(SOCKET_OP, ret);
}

HCCP_ATTRI_VISI_DEF int RaSocketWhiteListDel(void *socketHandle, struct socket_wlist_info_t whiteList[],
    unsigned int num)
{
    struct ra_socket_handle *socketHandleTmp = NULL;
    char localIp[MAX_IP_LEN] = {0};
    unsigned int phyId;
    int ret;

    if (whiteList == NULL || num > MAX_WLIST_NUM || num == 0) {
        hccp_err("[del][ra_socket_white_list]white_list is NULL, or num (%u) > %u or = 0, invalid", num, MAX_WLIST_NUM);
        return ConverReturnCode(SOCKET_OP, -EINVAL);
    }
    socketHandleTmp = (struct ra_socket_handle *)socketHandle;
    if (socketHandleTmp == NULL || socketHandleTmp->socket_ops == NULL ||
        socketHandleTmp->socket_ops->ra_socket_white_list_del == NULL) {
        hccp_err("[del][ra_socket_white_list]socket_handle or func is NULL");
        return ConverReturnCode(SOCKET_OP, -EINVAL);
    }
    ret = RaInetPton(socketHandleTmp->rdev_info.family, socketHandleTmp->rdev_info.local_ip, localIp,
        MAX_IP_LEN);
    if (ret) {
        hccp_err("[del][ra_socket_white_list]ra_inet_pton for local_ip failed, ret(%d)", ret);
        return ConverReturnCode(SOCKET_OP, ret);
    }

    phyId = socketHandleTmp->rdev_info.phy_id;
    if (phyId >= RA_MAX_PHY_ID_NUM) {
        hccp_err("[del][ra_socket_white_list]phy_id(%u) must smaller than %u", phyId, RA_MAX_PHY_ID_NUM);
        return ConverReturnCode(SOCKET_OP, -EINVAL);
    }

    hccp_info("Input parameters: phy_id[%u], localIp[%s], num[%u]", phyId, localIp, num);

    ret = socketHandleTmp->socket_ops->ra_socket_white_list_del(socketHandleTmp->rdev_info, whiteList, num);
    return ConverReturnCode(SOCKET_OP, ret);
}

STATIC int RaIfaddrInfoConverter(unsigned int phyId, bool isAll, struct interface_info interfaceInfos[],
    unsigned int *num)
{
    struct ifaddr_info *ifaddrInfos = NULL;
    unsigned int interfaceVersionV2 = 0;
    unsigned int interfaceVersion = 0;
    unsigned int i;
    int ret;

    ret = RaGetInterfaceVersion(phyId, RA_RS_GET_IFADDRS, &interfaceVersion);
    CHK_PRT_RETURN(ret != 0 || interfaceVersion == 0,
        hccp_err("[converter][ra_ifaddr]get interface version failed, ret(%d), phyId(%u), interfaceVersion(%u)",
            ret, phyId, interfaceVersion), -EINVAL);

    ret = RaGetInterfaceVersion(phyId, RA_RS_GET_IFADDRS_V2, &interfaceVersionV2);
    CHK_PRT_RETURN(ret != 0,
        hccp_err("[converter][ra_ifaddr]get interface version failed, ret(%d), phyId(%u), interfaceVersion(%u)",
            ret, phyId, interfaceVersion), -EINVAL);

    CHK_PRT_RETURN(interfaceVersionV2 < GET_IFADDRS_VERSION_3 && isAll,
        hccp_err("[converter][ra_ifaddr]current version do not support get all device ip addr, interfaceVersion(%u), "
            "interfaceVersionV2(%u), isAll(%d)", interfaceVersion, interfaceVersionV2, isAll), -EPROTONOSUPPORT);

    if (interfaceVersion == GET_IFADDRS_VERSION_1) {
        ifaddrInfos = calloc(*num, sizeof(struct ifaddr_info));
        if (ifaddrInfos == NULL) {
            hccp_err("[converter][ra_ifaddr]calloc for ifaddr_infos failed");
            return -EINVAL;
        }

        ret = RaHdcGetIfaddrs(phyId, ifaddrInfos, num);
        if (ret) {
            hccp_err("[converter][ra_ifaddr]ra_hdc_get_ifaddrs failed, ret(%d), phyId(%u), num(%u)",
                ret, phyId, *num);
            free(ifaddrInfos);
            return ret;
        }

        for (i = 0; i < *num; i++) {
            // device 网卡只支持IPv4
            interfaceInfos[i].family = AF_INET;
            interfaceInfos[i].scope_id = 0;
            ret = memcpy_s(&(interfaceInfos[i].ifaddr.ip), sizeof(union hccp_ip_addr), &(ifaddrInfos[i].ip),
                sizeof(union hccp_ip_addr));
            if (ret) {
                hccp_err("[converter][ra_ifaddr]memcpy_s interface[%u] ip failed, ret(%d)", i, ret);
                free(ifaddrInfos);
                return -ESAFEFUNC;
            }

            ret = memcpy_s(&(interfaceInfos[i].ifaddr.mask), sizeof(struct in_addr), &(ifaddrInfos[i].mask),
                sizeof(struct in_addr));
            if (ret) {
                hccp_err("[converter][ra_ifaddr]memcpy_s interface[%u] mask failed, ret(%d)", i, ret);
                free(ifaddrInfos);
                return -ESAFEFUNC;
            }
        }

        free(ifaddrInfos);
    } else if (interfaceVersion == GET_IFADDRS_VERSION_2) { /* version 2 support IPV6 and IPV4 */
        ret = RaHdcGetIfaddrsV2(phyId, isAll, interfaceInfos, num);
        CHK_PRT_RETURN(ret,
            hccp_err("[converter][ra_ifaddr]ra_hdc_get_ifaddrs_v2 failed, ret(%d), phyId(%u), isAll(%d), num(%u)",
                ret, phyId, isAll, *num), ret);
    } else {
        hccp_err("[converter][ra_ifaddr]interface version not support, interfaceVersion(%u)", interfaceVersion);
        return -EINVAL;
    }

    return 0;
}

HCCP_ATTRI_VISI_DEF int RaGetIfnum(struct ra_get_ifattr *config, unsigned int *num)
{
    unsigned int interfaceVersion = 0;
    int ret;

    CHK_PRT_RETURN(config == NULL || num == NULL, hccp_err("[get][ra_ifnum]config or num is NULL"),
        ConverReturnCode(OTHERS, -EINVAL));

    CHK_PRT_RETURN(config->phy_id >= RA_MAX_PHY_ID_NUM,
        hccp_err("[get][ra_ifnum]phy_id(%u) is invalid! it must greater or equal to 0 and less than %d!",
        config->phy_id, RA_MAX_PHY_ID_NUM),
        ConverReturnCode(OTHERS, -EINVAL));

    hccp_run_info("Input parameters: phy_id[%u], nic_position:[%u], isAll:[%d]",
        config->phy_id, config->nic_position, config->is_all);

    ret = RaGetInterfaceVersion(config->phy_id, RA_RS_GET_IFADDRS_V2, &interfaceVersion);
    CHK_PRT_RETURN(ret != 0,
        hccp_err("[get][ra_ifnum]get interface version failed, ret(%d), phyId(%u), interfaceVersion(%u)", ret,
        config->phy_id, interfaceVersion),
        ConverReturnCode(OTHERS, -EINVAL));

    CHK_PRT_RETURN(interfaceVersion < GET_IFADDRS_VERSION_3 && config->nic_position == NETWORK_OFFLINE &&
        config->is_all,
        hccp_err("[get][ra_ifnum]current version do not support get all ip num, interfaceVersion(%u), isAll(%d)",
        interfaceVersion, config->is_all),
        ConverReturnCode(OTHERS, -ENOTSUPP));

    if (config->nic_position == NETWORK_OFFLINE) {
        ret = RaHdcGetIfnum(config->phy_id, config->is_all, num);
        CHK_PRT_RETURN(ret, hccp_err("[get][ra_ifnum]ra_hdc_get_ifnum failed, ret(%d)", ret),
            ConverReturnCode(OTHERS, ret));
    } else if (config->nic_position == NETWORK_PEER_ONLINE) {
        ret = RaPeerGetIfnum(config->phy_id, num);
        CHK_PRT_RETURN(ret, hccp_err("[get][ra_ifnum]ra_peer_get_ifnum failed, ret(%d)", ret),
            ConverReturnCode(OTHERS, ret));
    } else {
        hccp_err("[get][ra_ifnum]Wrong mode, do not support online mode");
        return ConverReturnCode(OTHERS, -EPROTONOSUPPORT);
    }

    CHK_PRT_RETURN((*num) > MAX_SUPPORT_IFNUM,
        hccp_err("[get][ra_ifnum]get interface num(%d)! It must greater or equal to 0 and less or equal to %d", (*num),
        MAX_SUPPORT_IFNUM), ConverReturnCode(OTHERS, -EINVAL));

    return 0;
}

HCCP_ATTRI_VISI_DEF int RaGetIfaddrs(struct ra_get_ifattr *config, struct interface_info interfaceInfos[],
    unsigned int *num)
{
    int ret;

    CHK_PRT_RETURN(config == NULL || interfaceInfos == NULL || num == NULL,
        hccp_err("[get][ra_ifaddrs]config or interface_infos or num is NULL"), ConverReturnCode(OTHERS, -EINVAL));

    CHK_PRT_RETURN(config->phy_id >= RA_MAX_PHY_ID_NUM,
        hccp_err("[get][ra_ifaddrs]phy_id(%u) is invalid! it must greater or equal to 0 and less than %d!",
        config->phy_id, RA_MAX_PHY_ID_NUM), ConverReturnCode(OTHERS, -EINVAL));

    CHK_PRT_RETURN(*num == 0, hccp_err("[get][ra_ifaddrs]interface num(%u) is invalid! it must greater than 0", *num),
        ConverReturnCode(OTHERS, -EINVAL));

    hccp_run_info("Input parameters: phy_id[%u], nic_position:[%u], isAll[%d], interface num[%u]",
        config->phy_id, config->nic_position, config->is_all, *num);

    if (config->nic_position == NETWORK_OFFLINE) {
        CHK_PRT_RETURN(*num > MAX_INTERFACE_NUM,
            hccp_err("[get][ra_ifaddrs]interface num(%u) is invalid! it must be less than %d!", *num, MAX_INTERFACE_NUM),
            ConverReturnCode(OTHERS, -EINVAL));

        ret = RaIfaddrInfoConverter(config->phy_id, config->is_all, interfaceInfos, num);
        CHK_PRT_RETURN(ret, hccp_err("[get][ra_ifaddrs]ra_hdc_get_ifaddrs failed, ret(%d)", ret),
            ConverReturnCode(OTHERS, ret));
    } else if (config->nic_position == NETWORK_PEER_ONLINE) {
        ret = RaPeerGetIfaddrs(config->phy_id, interfaceInfos, num);
        CHK_PRT_RETURN(ret, hccp_err("[get][ra_ifaddrs]ra_peer_get_ifaddrs failed, ret(%d)", ret),
            ConverReturnCode(OTHERS, ret));
    } else {
        hccp_err("[get][ra_ifaddrs]Wrong mode, do not support online mode");
        return ConverReturnCode(OTHERS, -EPROTONOSUPPORT);
    }
    return 0;
}

HCCP_ATTRI_VISI_DEF int RaGetInterfaceVersion(unsigned int phyId, unsigned int interfaceOpcode,
    unsigned int* interfaceVersion)
{
    int ret;

    if (interfaceVersion == NULL || phyId >= RA_MAX_PHY_ID_NUM || interfaceOpcode >= RA_RS_EXTER_OP_MAX_NUM) {
        hccp_err("[get][ra_interface_version]para is invalid! interface_version is NULL or phy_id(%u) is"\
            "greater than [%u] or interface_opcode(%u) more than [%u]", phyId, RA_MAX_PHY_ID_NUM, interfaceOpcode,
            RA_RS_EXTER_OP_MAX_NUM);
        return ConverReturnCode(OTHERS, -EINVAL);
    }

    ret = RaHdcGetInterfaceVersion(phyId, interfaceOpcode, interfaceVersion);
    return ConverReturnCode(OTHERS, ret);
}

int ConverReturnCode(enum module_type module, int erroCode)
{
    unsigned int i;
    unsigned int num = sizeof(gErrcodeInfoList) / sizeof(gErrcodeInfoList[0]);
    int ret = CONVER_ERROR_CODE(module, DEFAULT_ERRCODE_TYPE, DEFAULT_MODULE_ERRCODE);

    if (erroCode == 0) {
        return 0;
    }

    if (erroCode / ACL_ERRCODE_DIGIT) {        /* ACL error codes are transparently transmitted */
        return erroCode;
    }

    for (i = 0; i < num; i++) {
        if (erroCode == gErrcodeInfoList[i].orig_errcode) {
            ret = CONVER_ERROR_CODE(module, gErrcodeInfoList[i].err_type, gErrcodeInfoList[i].module_errcode);
            break;
        }
    }

    if (erroCode != -EAGAIN) {  // 防止刷屏
        hccp_info("conver_return_code: orig_errcode[%d] curr_errcode[%d]", erroCode, ret);
    }
    return ret;
}

HCCP_ATTRI_VISI_DEF int RaRecvWrlist(void *qpHandle, struct recv_wrlist_data *wr, unsigned int recvNum,
    unsigned int *completeNum)
{
    int ret;
    unsigned int i;
    struct ra_qp_handle *raQpHandle = (struct ra_qp_handle *)qpHandle;

    if (qpHandle == NULL || wr == NULL || recvNum == 0 || completeNum == NULL) {
        hccp_err("[recv][ra_wrlist]qp_handle or wr or complete_num is NULL or recv_num[%u] is zero, para error!",
            recvNum);
        return ConverReturnCode(RDMA_OP, -EINVAL);
    }

    for (i = 0; i < recvNum; i++) {
        if (wr[i].mem_list.len > MAX_SG_LIST_LEN_MAX) {
            hccp_err("[recv][ra_wrlist]sg list len is more than 2G, len(%u)", wr[i].mem_list.len);
            return ConverReturnCode(RDMA_OP, -EINVAL);
        }
    }
    if (raQpHandle->rdma_ops == NULL || raQpHandle->rdma_ops->ra_recv_wrlist == NULL) {
        hccp_err("[recv][ra_wrlist]rdma_ops or ra_recv_wrlist is NULL, invalid");
        return ConverReturnCode(RDMA_OP, -EINVAL);
    }
    ret = raQpHandle->rdma_ops->ra_recv_wrlist(raQpHandle, wr, recvNum, completeNum);
    return ConverReturnCode(RDMA_OP, ret);
}

HCCP_ATTRI_VISI_DEF int RaGetQpContext(void *qpHandle, void** qp, void** sendCq, void** recvCq)
{
    int ret;
    struct ra_qp_handle *raQpHandle = (struct ra_qp_handle *)qpHandle;

    if (qpHandle == NULL) {
        hccp_err("[request][ra_get_qp_context]qp_handle is NULL, para error!");
        return ConverReturnCode(RDMA_OP, -EINVAL);
    }

    if (raQpHandle->rdma_ops == NULL || raQpHandle->rdma_ops->ra_get_qp_context == NULL) {
        hccp_err("[get][ra_get_qp_context]rdma_ops is NULL or ra_get_qp_context is NULL, invalid");
        return ConverReturnCode(RDMA_OP, -EINVAL);
    }

    ret = raQpHandle->rdma_ops->ra_get_qp_context(raQpHandle, qp, sendCq, recvCq);
    return ConverReturnCode(RDMA_OP, ret);
}

HCCP_ATTRI_VISI_DEF int RaCqCreate(void *rdevHandle, struct cq_attr *attr)
{
    struct ra_rdma_handle *rdmaHandleTmp = (struct ra_rdma_handle *)rdevHandle;
    unsigned int phyId;
    int ret;

    CHK_PRT_RETURN(rdevHandle == NULL || rdmaHandleTmp->rdma_ops == NULL ||
        rdmaHandleTmp->rdma_ops->ra_cq_create == NULL || attr == NULL || attr->ib_send_cq == NULL ||
        attr->ib_recv_cq == NULL || attr->qp_context == NULL, hccp_err("[create][ra_cq]para is NULL or func is NULL"),
        ConverReturnCode(RDMA_OP, -EINVAL));

    phyId = rdmaHandleTmp->rdev_info.phy_id;
    CHK_PRT_RETURN(phyId >= RA_MAX_PHY_ID_NUM,
        hccp_err("[create][ra_cq]phy_id(%u) must less than %d!", phyId, RA_MAX_PHY_ID_NUM),
        ConverReturnCode(RDMA_OP, -EINVAL));

    hccp_run_info("Input parameters: phy_id[%u]", phyId);

    ret = rdmaHandleTmp->rdma_ops->ra_cq_create(rdmaHandleTmp, attr);
    CHK_PRT_RETURN(ret != 0 || *attr->ib_send_cq == NULL || *attr->ib_recv_cq == NULL || *attr->qp_context == NULL,
        hccp_err("[create][ra_cq]create cp failed, ret(%d) phyId(%u)", ret, phyId),
        ConverReturnCode(RDMA_OP, -EINVAL));

    return 0;
}

HCCP_ATTRI_VISI_DEF int RaCqDestroy(void *rdevHandle, struct cq_attr *attr)
{
    struct ra_rdma_handle *rdmaHandleTmp = (struct ra_rdma_handle *)rdevHandle;
    unsigned int phyId;
    int ret;

    CHK_PRT_RETURN(rdevHandle == NULL || rdmaHandleTmp->rdma_ops == NULL || 
        rdmaHandleTmp->rdma_ops->ra_cq_destroy == NULL || attr == NULL || attr->ib_send_cq == NULL ||
        attr->ib_recv_cq == NULL || attr->qp_context == NULL, hccp_err("[destroy][ra_cq]para is NULL or func is NULL"),
        ConverReturnCode(RDMA_OP, -EINVAL));

    phyId = rdmaHandleTmp->rdev_info.phy_id;
    CHK_PRT_RETURN(phyId >= RA_MAX_PHY_ID_NUM,
        hccp_err("[destroy][ra_cq]phy_id(%u) must less than %d!", phyId, RA_MAX_PHY_ID_NUM),
        ConverReturnCode(RDMA_OP, -EINVAL));

    hccp_run_info("Input parameters: phy_id[%u]", phyId);

    ret = rdmaHandleTmp->rdma_ops->ra_cq_destroy(rdmaHandleTmp, attr);
    CHK_PRT_RETURN(ret != 0 || *attr->ib_send_cq == NULL || *attr->ib_recv_cq == NULL ||
                   *attr->qp_context == NULL,
        hccp_err("[destroy][ra_cq]destroy cp failed, ret(%d) phyId(%u)", ret, phyId),
        ConverReturnCode(RDMA_OP, -EINVAL));

    return 0;
}

HCCP_ATTRI_VISI_DEF int RaNormalQpCreate(void *rdevHandle, struct ibv_qp_init_attr *qpInitAttr, void **qpHandle,
    void **qp)
{
    struct ra_rdma_handle *rdmaHandleTmp = (struct ra_rdma_handle *)rdevHandle;
    unsigned int phyId;
    int ret;

    if (rdevHandle == NULL || rdmaHandleTmp->rdma_ops == NULL ||
        rdmaHandleTmp->rdma_ops->ra_normal_qp_create == NULL || qpInitAttr == NULL || qp == NULL) {
        hccp_err("[create][ra_normal_qp]para is NULL or func is NULL");
        return ConverReturnCode(RDMA_OP, -EINVAL);
    }

    if (qpHandle == NULL) {
        hccp_err("[create][ra_normal_qp]qp_handle is NULL");
        return ConverReturnCode(RDMA_OP, -EINVAL);
    }

    phyId = rdmaHandleTmp->rdev_info.phy_id;
    if (phyId >= RA_MAX_PHY_ID_NUM) {
        hccp_err("[create][ra_normal_qp]phy_id(%u) must less than %d!", phyId, RA_MAX_PHY_ID_NUM);
        return ConverReturnCode(RDMA_OP, -EINVAL);
    }

    hccp_run_info("Input parameters: phy_id[%u]", phyId);

    ret = rdmaHandleTmp->rdma_ops->ra_normal_qp_create(rdmaHandleTmp, qpInitAttr, qpHandle, qp);
    if (ret != 0 || *qpHandle == NULL) {
        hccp_err("[create][ra_normal_qp]create qp failed, ret(%d) phyId(%u)", ret, phyId);
        return ConverReturnCode(RDMA_OP, ret);
    }

    return 0;
}

HCCP_ATTRI_VISI_DEF int RaNormalQpDestroy(void *qpHandle)
{
    int ret;
    struct ra_qp_handle *raQpHandle = (struct ra_qp_handle *)qpHandle;

    if (qpHandle == NULL) {
        hccp_err("[destroy][ra_normal_qp]qp_handle is NULL");
        return ConverReturnCode(RDMA_OP, -EINVAL);
    }

    if (raQpHandle->rdma_ops == NULL || raQpHandle->rdma_ops->ra_normal_qp_destroy == NULL) {
        hccp_err("[destroy][ra_qp]rdma_ops is NULL or ra_qp_handle->rdma_ops->ra_normal_qp_destroy is NULL, invalid");
        return ConverReturnCode(RDMA_OP, -EINVAL);
    }

    hccp_run_info("Input parameters: qpn[%u], phyId[%u], rdevIndex[%u]",
        raQpHandle->qpn, raQpHandle->phy_id, raQpHandle->rdev_index);

    ret = raQpHandle->rdma_ops->ra_normal_qp_destroy(raQpHandle);
    return ConverReturnCode(RDMA_OP, ret);
}

HCCP_ATTRI_VISI_DEF int RaSetQpAttrQos(void *qpHandle, struct qos_attr *attr)
{
    int ret;
    struct ra_qp_handle *raQpHandle = (struct ra_qp_handle *)qpHandle;

    if (qpHandle == NULL || attr == NULL) {
        hccp_err("[set][ra_qp_attr_qos]qp_handle or attr is NULL, para error!");
        return ConverReturnCode(RDMA_OP, -EINVAL);
    }

    if (raQpHandle->rdma_ops == NULL || raQpHandle->rdma_ops->ra_set_qp_attr_qos == NULL) {
        hccp_err("[set][ra_qp_attr_qos]rdma_ops is NULL or rdma_ops->ra_set_qp_attr_qos is NULL, invalid");
        return ConverReturnCode(RDMA_OP, -EINVAL);
    }

    ret = raQpHandle->rdma_ops->ra_set_qp_attr_qos(raQpHandle, attr);
    return ConverReturnCode(RDMA_OP, ret);
}

HCCP_ATTRI_VISI_DEF int RaSetQpAttrTimeout(void *qpHandle, unsigned int *timeout)
{
    int ret;
    struct ra_qp_handle *raQpHandle = (struct ra_qp_handle *)qpHandle;

    if (qpHandle == NULL || timeout == NULL) {
        hccp_err("[set][ra_qp_attr_timeout]qp_handle or timeout is NULL, para error!");
        return ConverReturnCode(RDMA_OP, -EINVAL);
    }

    if (raQpHandle->rdma_ops == NULL || raQpHandle->rdma_ops->ra_set_qp_attr_timeout == NULL) {
        hccp_err("[set][ra_qp_attr_timeout]rdma_ops is NULL or rdma_ops->ra_set_qp_attr_timeout is NULL, invalid");
        return ConverReturnCode(RDMA_OP, -EINVAL);
    }

    ret = raQpHandle->rdma_ops->ra_set_qp_attr_timeout(raQpHandle, timeout);
    return ConverReturnCode(RDMA_OP, ret);
}

HCCP_ATTRI_VISI_DEF int RaSetQpAttrRetryCnt(void *qpHandle, unsigned int *retryCnt)
{
    int ret;
    struct ra_qp_handle *raQpHandle = (struct ra_qp_handle *)qpHandle;

    if (qpHandle == NULL || retryCnt == NULL) {
        hccp_err("[set][ra_qp_attr_retry_cnt]qp_handle or retry_cnt is NULL, para error!");
        return ConverReturnCode(RDMA_OP, -EINVAL);
    }

    if (raQpHandle->rdma_ops == NULL || raQpHandle->rdma_ops->ra_set_qp_attr_retry_cnt == NULL) {
        hccp_err("[set][ra_qp_attr_retry_cnt]rdma_ops is NULL or rdma_ops->ra_set_qp_attr_retry_cnt is NULL, invalid");
        return ConverReturnCode(RDMA_OP, -EINVAL);
    }

    ret = raQpHandle->rdma_ops->ra_set_qp_attr_retry_cnt(raQpHandle, retryCnt);
    return ConverReturnCode(RDMA_OP, ret);
}

HCCP_ATTRI_VISI_DEF int RaCreateCompChannel(const void *rdmaHandle, void **compChannel)
{
    int ret;
    struct ra_rdma_handle *raRdmaHandle = (struct ra_rdma_handle *)rdmaHandle;

    if (rdmaHandle == NULL) {
        hccp_err("[ra_create_comp_channel]rdma_handle(%p) is NULL, para error!", rdmaHandle);
        return ConverReturnCode(RDMA_OP, -EINVAL);
    }

    if (compChannel == NULL) {
        hccp_err("[ra_create_comp_channel]comp_channel is NULL");
        return ConverReturnCode(RDMA_OP, -EINVAL);
    }

    if (raRdmaHandle->rdma_ops == NULL || raRdmaHandle->rdma_ops->ra_create_comp_channel == NULL) {
        hccp_err("[ra_create_comp_channel]rdma_ops is NULL or ra_rdma_handle->rdma_ops->ra_create_comp_channel "
            "is NULL, invalid");
        return ConverReturnCode(RDMA_OP, -EINVAL);
    }

    ret = raRdmaHandle->rdma_ops->ra_create_comp_channel(raRdmaHandle, compChannel);
    return ConverReturnCode(RDMA_OP, ret);
}

HCCP_ATTRI_VISI_DEF int RaDestroyCompChannel(const void *rdmaHandle, void *compChannel)
{
    int ret;
    struct ra_rdma_handle *raRdmaHandle = (struct ra_rdma_handle *)rdmaHandle;

    if (rdmaHandle == NULL) {
        hccp_err("[ra_destroy_comp_channel]rdma_handle(%p) is NULL, para error!", rdmaHandle);
        return ConverReturnCode(RDMA_OP, -EINVAL);
    }

    if (compChannel == NULL) {
        hccp_err("[ra_destroy_comp_channel]comp_channel is NULL");
        return ConverReturnCode(RDMA_OP, -EINVAL);
    }

    if (raRdmaHandle->rdma_ops == NULL || raRdmaHandle->rdma_ops->ra_destroy_comp_channel == NULL) {
        hccp_err("[ra_destroy_comp_channel]rdma_ops is NULL or ra_rdma_handle->rdma_ops->ra_destroy_comp_channel "
            "is NULL, invalid");
        return ConverReturnCode(RDMA_OP, -EINVAL);
    }

    ret = raRdmaHandle->rdma_ops->ra_destroy_comp_channel(compChannel);
    return ConverReturnCode(RDMA_OP, ret);
}

HCCP_ATTRI_VISI_DEF int RaGetCqeErrInfo(unsigned int phyId, struct cqe_err_info *info)
{
    int ret;

    if (info == NULL) {
        hccp_err("[ra_get_cqe_err_info]cqe_err_info is NULL, para error!");
        return ConverReturnCode(RDMA_OP, -EINVAL);
    }

    if (phyId >= RA_MAX_PHY_ID_NUM) {
        hccp_err("[ra_get_cqe_err_info]phy_id(%u) must greater or equal to 0 and less than %d!",
            phyId, RA_MAX_PHY_ID_NUM);
        return ConverReturnCode(RDMA_OP, -EINVAL);
    }

    ret = RaHdcGetCqeErrInfo(phyId, info);
    return ConverReturnCode(RDMA_OP, ret);
}

HCCP_ATTRI_VISI_DEF int RaRdevGetCqeErrInfoList(void *rdmaHandle, struct cqe_err_info *infoList,
    unsigned int *num)
{
    struct ra_rdma_handle *raRdmaHandle = (struct ra_rdma_handle *)rdmaHandle;
    int ret;

    if (rdmaHandle == NULL || infoList == NULL || num == NULL) {
        hccp_err("[get][cqe_err_info_list]rdma_handle or info_list or num is NULL, para error!");
        return ConverReturnCode(RDMA_OP, -EINVAL);
    }

    if (*num == 0 || *num > CQE_ERR_INFO_MAX_NUM) {
        hccp_err("[get][cqe_err_info_list]num(%u) is invalid!", *num);
        return ConverReturnCode(OTHERS, -EINVAL);
    }

    ret = RaHdcGetCqeErrInfoList(raRdmaHandle, infoList, num);
    return ConverReturnCode(RDMA_OP, ret);
}

HCCP_ATTRI_VISI_DEF int RaGetQpAttr(void *qpHandle, struct qp_attr *attr)
{
    struct ra_qp_handle *raQpHandle = (struct ra_qp_handle *)qpHandle;

    if (qpHandle == NULL || attr == NULL) {
        hccp_err("[get][get_qp_attr]qp_handle or attr is NULL, para error!");
        return ConverReturnCode(RDMA_OP, -EINVAL);
    }

    attr->qpn = raQpHandle->qpn;
    attr->udp_sport = raQpHandle->udp_sport;
    attr->psn = raQpHandle->psn;
    attr->gid_idx = raQpHandle->gid_idx;
    (void)memcpy_s(attr->gid, HCCP_GID_RAW_LEN, raQpHandle->rdma_handle->gid, HCCP_GID_RAW_LEN);
    return 0;
}

HCCP_ATTRI_VISI_DEF int RaCreateSrq(const void *rdmaHandle, struct srq_attr *attr)
{
    int ret;
    struct ra_rdma_handle *raRdmaHandle = (struct ra_rdma_handle *)rdmaHandle;

    if (rdmaHandle == NULL) {
        hccp_err("[ra_create_srq]rdma_handle(%p) is NULL, para error!", rdmaHandle);
        return ConverReturnCode(RDMA_OP, -EINVAL);
    }

    if (attr == NULL) {
        hccp_err("[ra_create_srq]srq_attr is NULL");
        return ConverReturnCode(RDMA_OP, -EINVAL);
    }

    // 创建srq&srq cq
    if (raRdmaHandle->rdma_ops == NULL || raRdmaHandle->rdma_ops->ra_create_srq == NULL) {
        hccp_err("[ra_create_srq]rdma_ops is NULL or ra_rdma_handle->rdma_ops->ra_create_srq "
            "is NULL, invalid");
        return ConverReturnCode(RDMA_OP, -EINVAL);
    }

    ret = raRdmaHandle->rdma_ops->ra_create_srq(raRdmaHandle, attr);
    return ConverReturnCode(RDMA_OP, ret);
}

HCCP_ATTRI_VISI_DEF int RaDestroySrq(const void *rdmaHandle, struct srq_attr *attr)
{
    int ret;
    struct ra_rdma_handle *raRdmaHandle = (struct ra_rdma_handle *)rdmaHandle;

    if (rdmaHandle == NULL) {
        hccp_err("[ra_destroy_srq]rdma_handle(%p) is NULL, para error!", rdmaHandle);
        return ConverReturnCode(RDMA_OP, -EINVAL);
    }

    if (attr == NULL) {
        hccp_err("[ra_destroy_srq]srq_handle is NULL");
        return ConverReturnCode(RDMA_OP, -EINVAL);
    }

    // 销毁srq&srq cq
    if (raRdmaHandle->rdma_ops == NULL || raRdmaHandle->rdma_ops->ra_destroy_srq == NULL) {
        hccp_err("[ra_destroy_srq]rdma_ops is NULL or ra_rdma_handle->rdma_ops->ra_destroy_srq "
            "is NULL, invalid");
        return ConverReturnCode(RDMA_OP, -EINVAL);
    }

    ret = raRdmaHandle->rdma_ops->ra_destroy_srq(raRdmaHandle, attr);
    return ConverReturnCode(RDMA_OP, ret);
}

HCCP_ATTRI_VISI_DEF int RaCreateEventHandle(int *eventHandle)
{
    int ret;

    if (eventHandle == NULL) {
        hccp_err("[ra_create_event_handle]event_handle is NULL");
        return ConverReturnCode(SOCKET_OP, -EINVAL);
    }

    ret = RaPeerCreateEventHandle(eventHandle);
    if (ret) {
        hccp_err("[ra_create_event_handle]ra_peer_create_event_handle failed ret(%d)", ret);
        return ConverReturnCode(SOCKET_OP, ret);
    }
    return 0;
}

HCCP_ATTRI_VISI_DEF int RaCtlEventHandle(int eventHandle, const void *fdHandle, int opcode,
    enum RaEpollEvent event)
{
    int ret;

    if (eventHandle < 0) {
        hccp_err("[ra_ctl_event_handle]event_handle[%d] is invalid", eventHandle);
        return ConverReturnCode(SOCKET_OP, -EINVAL);
    }
    if (fdHandle == NULL) {
        hccp_err("[ra_ctl_event_handle]fd_handle is NULL");
        return ConverReturnCode(SOCKET_OP, -EINVAL);
    }
    if (opcode != EPOLL_CTL_ADD && opcode != EPOLL_CTL_DEL && opcode != EPOLL_CTL_MOD) {
        hccp_err("[ra_ctl_event_handle]opcode[%d] invalid, valid opcode includes {%d, %d, %d}",
            opcode, EPOLL_CTL_ADD, EPOLL_CTL_DEL, EPOLL_CTL_MOD);
        return ConverReturnCode(SOCKET_OP, -EINVAL);
    }
    if (event >= RA_EPOLLINVALD) {
        hccp_err("[ra_ctl_event_handle]event[%d] invalid, valid range [0, %d)", event, RA_EPOLLINVALD);
        return ConverReturnCode(SOCKET_OP, -EINVAL);
    }

    ret = RaPeerCtlEventHandle(eventHandle, fdHandle, opcode, event);
    if (ret) {
        hccp_err("[ra_ctl_event_handle]ra_peer_ctl_event_handle failed ret(%d)", ret);
        return ConverReturnCode(SOCKET_OP, ret);
    }
    return 0;
}

HCCP_ATTRI_VISI_DEF int RaWaitEventHandle(int eventHandle, struct socket_event_info *eventInfos, int timeout,
    unsigned int maxevents, unsigned int *eventsNum)
{
    int ret;

    if (eventHandle < 0) {
        hccp_err("[ra_wait_event_handle]event_handle[%d] is invalid", eventHandle);
        return ConverReturnCode(SOCKET_OP, -EINVAL);
    }

    if (eventInfos == NULL) {
        hccp_err("[ra_wait_event_handle]event_infos is NULL");
        return ConverReturnCode(SOCKET_OP, -EINVAL);
    }

    if (timeout < -1) {
        hccp_err("[ra_wait_event_handle]timeout[%d] is invalid", timeout);
        return ConverReturnCode(SOCKET_OP, -EINVAL);
    }

    if (maxevents > MAX_SOCKET_EVENT_NUM) {
        hccp_err("[ra_wait_event_handle]maxevents[%u] exceeds %u", maxevents, MAX_SOCKET_EVENT_NUM);
        return ConverReturnCode(SOCKET_OP, -EINVAL);
    }

    if (eventsNum == NULL) {
        hccp_err("[ra_wait_event_handle]events_num is NULL");
        return ConverReturnCode(SOCKET_OP, -EINVAL);
    }

    ret = RaPeerWaitEventHandle(eventHandle, eventInfos, timeout, maxevents, eventsNum);
    if (ret) {
        hccp_err("[ra_wait_event_handle]ra_peer_wait_event_handle failed ret(%d)", ret);
        return ConverReturnCode(SOCKET_OP, ret);
    }
    return 0;
}

HCCP_ATTRI_VISI_DEF int RaDestroyEventHandle(int *eventHandle)
{
    int ret;

    if (eventHandle == NULL) {
        hccp_err("[ra_destroy_event_handle]event_handle is NULL");
        return ConverReturnCode(SOCKET_OP, -EINVAL);
    }

    ret = RaPeerDestroyEventHandle(eventHandle);
    if (ret) {
        hccp_err("[ra_destroy_event_handle]ra_peer_destroy_event_handle failed ret(%d)", ret);
        return ConverReturnCode(SOCKET_OP, ret);
    }
    return 0;
}

HCCP_ATTRI_VISI_DEF int RaPollCq(void *qpHandle, bool isSendCq, unsigned int numEntries, void *wc)
{
    int ret;
    struct ra_qp_handle *raQpHandle = (struct ra_qp_handle *)qpHandle;

    if (qpHandle == NULL || wc == NULL) {
        hccp_err("[ra_poll]qp_handle is NULL or wc is NULL, para error!");
        return ConverReturnCode(RDMA_OP, -EINVAL);
    }

    if (raQpHandle->rdma_ops == NULL || raQpHandle->rdma_ops->ra_poll_cq == NULL) {
        hccp_err("[ra_poll]rdma_ops is NULL or ra_qp_handle->rdma_ops->ra_poll_cq is NULL, invalid");
        return ConverReturnCode(RDMA_OP, -EINVAL);
    }

    ret = raQpHandle->rdma_ops->ra_poll_cq(raQpHandle, isSendCq, numEntries, wc);
    if (ret < 0) {
        return ConverReturnCode(RDMA_OP, ret);
    }
    return ret;
}

HCCP_ATTRI_VISI_DEF int RaTypicalQpModify(void *qpHandle, struct typical_qp *localQpInfo,
    struct typical_qp *remoteQpInfo)
{
    struct ra_qp_handle *raQpHandle = (struct ra_qp_handle *)qpHandle;
    unsigned int phyId;
    int ret;

    if (qpHandle == NULL || raQpHandle->rdma_ops == NULL ||
        raQpHandle->rdma_ops->ra_typical_qp_modify == NULL) {
        hccp_err("[modify][ra_qp]qp_handle is NULL or func is NULL");
        return ConverReturnCode(RDMA_OP, -EINVAL);
    }

    if (localQpInfo == NULL || remoteQpInfo == NULL) {
        hccp_err("[modify][ra_qp]local_qp_info is NULL or remote_qp_info is NULL");
        return ConverReturnCode(RDMA_OP, -EINVAL);
    }

    phyId = raQpHandle->phy_id;
    if (phyId >= RA_MAX_PHY_ID_NUM) {
        hccp_err("[modify][ra_qp]phy_id(%u) must greater or equal to 0 and less than %d!", phyId, RA_MAX_PHY_ID_NUM);
        return ConverReturnCode(RDMA_OP, -EINVAL);
    }

    hccp_run_info("Input parameters: phy_id[%u] local_qpn[%u] remote_qpn[%u]",
        phyId, localQpInfo->qpn, remoteQpInfo->qpn);

    ret = raQpHandle->rdma_ops->ra_typical_qp_modify(raQpHandle, localQpInfo, remoteQpInfo);
    if (ret != 0) {
        hccp_err("[modify][ra_qp]modify qp failed, ret(%d) phyId(%u)", ret, phyId);
        return ConverReturnCode(RDMA_OP, ret);
    }

    return 0;
}

HCCP_ATTRI_VISI_DEF int RaTypicalSendWr(void *qpHandle, struct send_wr *wr, struct send_wr_rsp *opRsp)
{
    struct ra_qp_handle *raQpHandle = (struct ra_qp_handle *)qpHandle;
    int ret;

    if (qpHandle == NULL || wr == NULL || wr->buf_list == NULL || opRsp == NULL) {
        hccp_err("[send][ra_wr]qp_handle or wr or buf_list or op_rsp is NULL, para error!");
        return ConverReturnCode(RDMA_OP, -EINVAL);
    }

    if (wr->buf_list->len > MAX_SG_LIST_LEN_MAX) {
        hccp_err("[send][ra_wr]sg list len is more than 2G, len(%u)", wr->buf_list->len);
        return ConverReturnCode(RDMA_OP, -EINVAL);
    }

    if (raQpHandle->rdma_ops == NULL || raQpHandle->rdma_ops->ra_typical_send_wr == NULL) {
        hccp_err("[send][ra_wr]rdma_ops is NULL or ra_qp_handle->rdma_ops->ra_typical_send_wr is NULL, invalid");
        return ConverReturnCode(RDMA_OP, -EINVAL);
    }

    ret = raQpHandle->rdma_ops->ra_typical_send_wr(raQpHandle, wr, opRsp);
    return ConverReturnCode(RDMA_OP, ret);
}

HCCP_ATTRI_VISI_DEF int RaSocketGetVnicIpInfos(unsigned int phyId, enum id_type type, unsigned int ids[],
    unsigned int num, struct ip_info infos[])
{
    int ret;

    if (ids == NULL || num == 0 || infos == NULL) {
        hccp_err("[get][vnic_ip]ids is NULL or num:%u == 0 or infos is NULL", num);
        return ConverReturnCode(OTHERS, -EINVAL);
    }

    if (phyId >= RA_MAX_PHY_ID_NUM) {
        hccp_err("[get][vnic_ip]phy_id(%u) is invalid! it must greater or equal to 0 and less than %d!",
            phyId, RA_MAX_PHY_ID_NUM);
        return ConverReturnCode(OTHERS, -EINVAL);
    }

    if (type != PHY_ID_VNIC_IP && type != SDID_VNIC_IP) {
        hccp_err("[get][vnic_ip]type[%u] invalid", type);
        return ConverReturnCode(OTHERS, -EINVAL);
    }

    ret = RaHdcGetVnicIpInfos(phyId, type, ids, num, infos);
    if (ret) {
        hccp_err("[get][vnic_ip]ra_hdc_get_vnic_ip_infos failed, ret(%d)", ret);
        return ConverReturnCode(OTHERS, ret);
    }
    return 0;
}

int RaQpBatchModifyCheckParam(void *rdmaHandle, void *qpHandle[], unsigned int num, int expectStatus)
{
    unsigned int i;

    if (rdmaHandle == NULL || num == 0 || qpHandle == NULL ||
        ((expectStatus != RA_QP_STATUS_CONNECTED) && (expectStatus != RA_QP_STATUS_PAUSE))) {
        hccp_err("[batch_modify][check param]expect_status is %d or rdma_handle is NULL or num[%u] is 0!",
            expectStatus, num);
        return ConverReturnCode(RDMA_OP, -EINVAL);
    }

    for (i = 0; i < num; i++) {
        if (qpHandle[i] == NULL) {
            hccp_err("[modify][ra_qp]qp_handle[%u] is NULL", i);
            return ConverReturnCode(RDMA_OP, -EINVAL);
        }
    }

    return 0;
}

HCCP_ATTRI_VISI_DEF int RaQpBatchModify(void *rdmaHandle, void *qpHandle[], unsigned int num, int expectStatus)
{
    struct ra_rdma_handle *raRdmaHandle = NULL;
    unsigned int phyId;
    int ret;

    ret = RaQpBatchModifyCheckParam(rdmaHandle, qpHandle, num, expectStatus);
    CHK_PRT_RETURN(ret, hccp_err("ra_qp_batch_modify_check_param invalid[%d]", ret), ret);

    raRdmaHandle = (struct ra_rdma_handle *)rdmaHandle;
    phyId = raRdmaHandle->rdev_info.phy_id;
    if (phyId >= RA_MAX_PHY_ID_NUM || raRdmaHandle->rdma_ops == NULL ||
        raRdmaHandle->rdma_ops->ra_qp_batch_modify == NULL) {
        hccp_err("[modify][ra_qp]phy_id(%u) must greater or equal to 0 and less than %d or ops is NULL or "
                 "ra_rdma_handle->rdma_ops->ra_qp_batch_modify is NULL!", phyId, RA_MAX_PHY_ID_NUM);
        return ConverReturnCode(RDMA_OP, -EINVAL);
    }

    hccp_run_info("Input parameters: phy_id[%u] num[%u] expect_status[%d]", phyId, num, expectStatus);

    // avoid poll_cq thread to poll cq
    if ((raRdmaHandle->support_lite != 0) && expectStatus == RA_QP_STATUS_PAUSE) {
        RA_PTHREAD_MUTEX_LOCK(&raRdmaHandle->rdev_mutex);
    }
    ret = raRdmaHandle->rdma_ops->ra_qp_batch_modify(rdmaHandle, qpHandle, num, expectStatus);
    if (ret != 0) {
        hccp_err("[modify][ra_qp_batch_modify]modify qp to [%d] failed, ret[%d] phyId[%u]",
            expectStatus, ret, phyId);
    }
    if ((raRdmaHandle->support_lite != 0) && expectStatus == RA_QP_STATUS_PAUSE) {
        RA_PTHREAD_MUTEX_UNLOCK(&raRdmaHandle->rdev_mutex);
    }

    return ret;
}

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

struct ra_hdc_server gHdcServer[RA_MAX_PHY_ID_NUM] = {0};
struct ra_hdc_init_para gHdcInitPara = {0};
struct rs_pthread_info gRaThreadInfo = {0};

struct rs_ops {
    int (*socket_batch_connect)(struct socket_connect_info conn[], unsigned int num);
    int (*socket_batch_close)(int disuseLinger, struct rs_socket_close_info_t conn[], unsigned int num);
    int (*socket_batch_abort)(struct socket_connect_info conn[], unsigned int num);
    int (*socket_listen_start)(struct socket_listen_info conn[], unsigned int num);
    int (*socket_listen_stop)(struct socket_listen_info conn[], unsigned int num);
    int (*get_sockets)(unsigned int role, struct socket_fd_data conn[], unsigned int num);
    int (*socket_send)(int fd, const void *data, uint64_t size);
    int (*socket_recv)(int fd, void *data, uint64_t size);
    int (*socket_init)(const unsigned int *vnicIp, unsigned int num);
    int (*socket_deinit)(struct rdev rdevInfo);
    int (*rdev_init)(struct rdev rdevInfo, unsigned int notifyType, unsigned int *rdevIndex);
    int (*rdev_init_with_backup)(struct rdev rdevInfo, struct rdev backupRdevInfo,
        unsigned int notifyType, unsigned int *rdevIndex);
    int (*rdev_get_port_status)(unsigned int phyId, unsigned int rdevIndex, enum port_status *status);
    int (*rdev_deinit)(unsigned int phyId, unsigned int notifyType, unsigned int rdevIndex);
    int (*qp_create)(unsigned int phyId, unsigned int rdevIndex, struct rs_qp_norm qpNorm,
        struct rs_qp_resp *qpResp);
    int (*qp_create_with_attrs)(unsigned int phyId, unsigned int rdevIndex,
        struct rs_qp_norm_with_attrs *qpNorm, struct rs_qp_resp_with_attrs *qpResp);
    int (*qp_destroy)(unsigned int phyId, unsigned int rdevIndex, unsigned int qpn);
    int (*typical_qp_modify)(unsigned int phyId, unsigned int rdevIndex, struct typical_qp localQpInfo,
        struct typical_qp remoteQpInfo, unsigned int *udpSport);
    int (*qp_batch_modify)(unsigned int phyId, unsigned int rdevIndex, int status, int qpn[], int qpnNum);
    int (*qp_connect_async)(unsigned int phyId, unsigned int rdevIndex, unsigned int qpn, int fd);
    int (*get_qp_status)(unsigned int phyId, unsigned int rdevIndex, unsigned int qpn,
        struct rs_qp_status_info *qpInfo);
    int (*mr_reg)(unsigned int phyId, unsigned int rdevIndex, unsigned int qpn, struct rdma_mr_reg_info *mrRegInfo);
    int (*mr_dereg)(unsigned int phyId, unsigned int rdevIndex, unsigned int qpn, char *addr);
    int (*register_mr)(unsigned int phyId, unsigned int rdevIndex, struct rdma_mr_reg_info *mrRegInfo,
        void **mrHandle);
    int (*typical_register_mr)(unsigned int phyId, unsigned int rdevIndex, struct rdma_mr_reg_info *mrRegInfo,
        void **mrHandle);
    int (*remap_mr)(unsigned int phyId, unsigned int rdevIndex, struct mem_remap_info memList[],
        unsigned int memNum);
    int (*typical_deregister_mr)(unsigned int phyId, unsigned int devIndex, unsigned long long addr);
    int (*send_wr)(unsigned int phyId, unsigned int rdevIndex, unsigned int qpn, struct send_wr *wr,
        struct send_wr_rsp *wrRsp);
    int (*send_wr_list)(struct rs_wrlist_base_info baseInfo, struct wr_info *wrList,
        unsigned int sendNum, struct send_wr_rsp *wrRsp, unsigned int *completeNum);
    int (*get_notify_mr_info)(unsigned int phyId, unsigned int rdevIndex, struct mr_info *info);
    int (*notify_cfg_set)(unsigned int phyId, unsigned long long va, unsigned long long size);
    int (*notify_cfg_get)(unsigned int phyId, unsigned long long *va, unsigned long long *size);
    int (*set_host_pid)(unsigned int phyId, pid_t hostPid, const char *pidSign);
    int (*white_list_add)(struct rdev rdevInfo, struct socket_wlist_info_t whiteList[], unsigned int num);
    int (*white_list_del)(struct rdev rdevInfo, struct socket_wlist_info_t whiteList[], unsigned int num);
    int (*accept_credit_add)(struct socket_listen_info conn[], uint32_t num, unsigned int creditLimit);
    int (*get_ifnum)(unsigned int phyId, bool isAll, unsigned int *num);
    int (*get_ifaddrs)(struct ifaddr_info ifaddrInfos[], unsigned int *num, unsigned int phyId);
    int (*get_ifaddrs_v2)(struct interface_info interfaceInfos[], unsigned int *num, unsigned int phyId,
        bool isAll);
    int (*get_vnic_ip)(unsigned int phyId, unsigned int *vnicIp);
    int (*get_vnic_ip_infos)(unsigned int phyId, enum id_type type, unsigned int ids[], unsigned int num,
        struct ip_info infos[]);
    int (*get_interface_version)(unsigned int opcode, unsigned int *version);
    int (*set_tsqp_depth)(unsigned int phyId, unsigned int rdevIndex, unsigned int tempDepth, unsigned int *qpNum);
    int (*get_tsqp_depth)(unsigned int phyId, unsigned int rdevIndex, unsigned int *tempDepth, unsigned int *qpNum);
    int (*set_qp_attr_qos)(unsigned int phyId, unsigned int rdevIndex, unsigned int qpn, struct qos_attr *attr);
    int (*set_qp_attr_timeout)(unsigned int phyId, unsigned int rdevIndex, unsigned int qpn, unsigned int *timeout);
    int (*set_qp_attr_retry_cnt)(unsigned int phyId, unsigned int rdevIndex, unsigned int qpn,
        unsigned int *retryCnt);
    int (*get_cqe_err_info)(struct cqe_err_info *info);
    int (*get_lite_support)(unsigned int phyId, unsigned int rdevIndex, int *supportLite);
    int (*get_lite_rdev_cap)(unsigned int phyId, unsigned int rdevIndex, struct lite_rdev_cap_resp *resp);
    int (*get_lite_qp_cq_attr)(
        unsigned int phyId, unsigned int rdevIndex, unsigned int qpn, struct lite_qp_cq_attr_resp *resp);
    int (*get_lite_mem_attr)(
        unsigned int phyId, unsigned int rdevIndex, unsigned int qpn, struct lite_mem_attr_resp *resp);
    int (*get_lite_connected_info)(
        unsigned int phyId, unsigned int rdevIndex, unsigned int qpn, struct lite_connected_info_resp *resp);
    int (*ping_init)(struct ping_init_attr *attr, struct ping_init_info *info, unsigned int *devIndex);
    int (*ping_target_add)(struct ra_rs_dev_info *rdev, struct ping_target_info *target);
    int (*ping_task_start)(struct ra_rs_dev_info *rdev, struct ping_task_attr *attr);
    int (*ping_get_results)(struct ra_rs_dev_info *rdev, struct ping_target_comm_info target[],
        unsigned int *num, struct ping_result_info result[]);
    int (*ping_task_stop)(struct ra_rs_dev_info *rdev);
    int (*ping_target_del)(struct ra_rs_dev_info *rdev, struct ping_target_comm_info target[],
        unsigned int *num);
    int (*ping_deinit)(struct ra_rs_dev_info *rdev);
    int (*get_cqe_err_info_num)(unsigned int phyId, unsigned int rdevIdx, unsigned int *num);
    int (*get_cqe_err_info_list)(unsigned int phyId, unsigned int rdevIdx, struct cqe_err_info *info,
        unsigned int *num);
    int (*get_tls_enable)(unsigned int phyId, bool *tlsEnable);
    int (*get_sec_random)(int *value);
    int (*get_hccn_cfg)(unsigned int phyId, enum hccn_cfg_key key, char *value, unsigned int *valueLen);
};

struct rs_ops gRaRsOps = {
    .socket_batch_connect = RsSocketBatchConnect,
    .socket_batch_close = RsSocketBatchClose,
    .socket_batch_abort = RsSocketBatchAbort,
    .socket_listen_start = RsSocketListenStart,
    .socket_listen_stop = RsSocketListenStop,
    .get_sockets = RsGetSockets,
    .socket_send = RsSocketSend,
    .socket_recv = RsSocketRecv,
    .socket_init = RsSocketInit,
    .socket_deinit = RsSocketDeinit,
    .rdev_init = RsRdevInit,
    .rdev_init_with_backup = RsRdevInitWithBackup,
    .rdev_get_port_status = RsRdevGetPortStatus,
    .rdev_deinit = RsRdevDeinit,
    .qp_create = RsQpCreate,
    .qp_create_with_attrs = RsQpCreateWithAttrs,
    .qp_destroy = RsQpDestroy,
    .typical_qp_modify = RsTypicalQpModify,
    .qp_batch_modify = RsQpBatchModify,
    .qp_connect_async = RsQpConnectAsync,
    .get_qp_status = RsGetQpStatus,
    .mr_reg = RsMrReg,
    .mr_dereg = RsMrDereg,
    .register_mr = RsTypicalRegisterMrV1,
    .typical_register_mr = RsTypicalRegisterMr,
    .remap_mr = RsRemapMr,
    .typical_deregister_mr = RsTypicalDeregisterMr,
    .send_wr = RsSendWr,
    .send_wr_list = RsSendWrlist,
    .get_notify_mr_info = RsGetNotifyMrInfo,
    .notify_cfg_set = RsNotifyCfgSet,
    .notify_cfg_get = RsNotifyCfgGet,
    .set_host_pid = RsSetHostPid,
    .white_list_add = RsSocketWhiteListAdd,
    .white_list_del = RsSocketWhiteListDel,
    .accept_credit_add = RsSocketAcceptCreditAdd,
    .get_ifnum = RsGetIfnum,
    .get_ifaddrs = RsGetIfaddrs,
    .get_ifaddrs_v2 = RsGetIfaddrsV2,
    .get_vnic_ip = RsGetVnicIp,
    .get_vnic_ip_infos = RsGetVnicIpInfos,
    .get_interface_version = RsGetInterfaceVersion,
    .set_tsqp_depth = RsSetTsqpDepth,
    .get_tsqp_depth = RsGetTsqpDepth,
    .set_qp_attr_qos = RsSetQpAttrQos,
    .set_qp_attr_timeout = RsSetQpAttrTimeout,
    .set_qp_attr_retry_cnt = RsSetQpAttrRetryCnt,
    .get_cqe_err_info = RsGetCqeErrInfo,
    .get_lite_rdev_cap = RsGetLiteRdevCap,
    .get_lite_qp_cq_attr = RsGetLiteQpCqAttr,
    .get_lite_connected_info = RsGetLiteConnectedInfo,
    .get_lite_mem_attr = RsGetLiteMemAttr,
    .get_lite_support = RsGetLiteSupport,
    .ping_init = RsPingInit,
    .ping_target_add = RsPingTargetAdd,
    .ping_task_start = RsPingTaskStart,
    .ping_get_results = RsPingGetResults,
    .ping_task_stop = RsPingTaskStop,
    .ping_target_del = RsPingTargetDel,
    .ping_deinit = RsPingDeinit,
    .get_cqe_err_info_num = RsGetCqeErrInfoNum,
    .get_cqe_err_info_list = RsGetCqeErrInfoList,
    .get_tls_enable = RsGetTlsEnable,
    .get_sec_random = RsDrvGetRandomNum,
    .get_hccn_cfg = RsGetHccnCfg,
};

struct hdc_ops gRaHdcOps = {
    .get_capacity = DlDrvHdcGetCapacity,
    .client_create = DlDrvHdcClientCreate,
    .client_destroy = DlDrvHdcClientDestroy,
    .session_connect = DlDrvHdcSessionConnect,
    .session_connect_ex = DlHalHdcSessionConnectEx,
    .server_create = DlDrvHdcServerCreate,
    .server_destroy = DlDrvHdcServerDestroy,
    .session_accept = DlDrvHdcSessionAccept,
    .session_close = DlDrvHdcSessionClose,
    .free_msg = DlDrvHdcFreeMsg,
    .reuse_msg = DlDrvHdcReuseMsg,
    .add_msg_buffer = DlDrvHdcAddMsgBuffer,
    .get_msg_buffer = DlDrvHdcGetMsgBuffer,
    .recv = DlHalHdcRecv,
    .send = DlHalHdcSend,
    .alloc_msg = DlDrvHdcAllocMsg,
    .set_session_reference = DlDrvHdcSetSessionReference,
};

#define RA_HDC_OPS gRaHdcOps

STATIC void MsgHeadBuildUpHw(char *pSendRcvBuf, struct msg_head *recvMsgHead, int ret,
    unsigned int msgDataLen)
{
    struct msg_head *pSendRcvHead = NULL;

    pSendRcvHead = (struct msg_head *)pSendRcvBuf;
    pSendRcvHead->opcode = recvMsgHead->opcode;
    pSendRcvHead->async_req_id = recvMsgHead->async_req_id;
    pSendRcvHead->ret = ret;
    pSendRcvHead->msg_data_len = msgDataLen;

    return;
}

STATIC int OpMsgErr(char **outBuf, struct msg_head *recvMsgHead, int *outBufLen, int opRight)
{
    unsigned int opcode = recvMsgHead->opcode;
    char *outBufTmp = NULL;
    int msgRet = 0;

    outBufTmp = (char *)calloc(sizeof(struct msg_head), sizeof(char));
    CHK_PRT_RETURN(outBufTmp == NULL, hccp_err("send_buf calloc failed."), -ENOMEM);

    if (opRight == HAVE_OP_RIGHT) {
        if (opcode >= RA_RS_OP_MAX_NUM || ((opcode < RA_RS_HDC_SESSION_CLOSE) && (opcode >= RA_RS_EXTER_OP_MAX_NUM))) {
            msgRet = -EPROTONOSUPPORT;
        } else {
            msgRet = -EPIPE;
        }
    } else if (opRight == TGID_INVALID) {
        msgRet = -EPERM;
    } else {
        msgRet = -EACCES;
    }

    MsgHeadBuildUpHw(outBufTmp, recvMsgHead, msgRet, 0);

    *outBuf = outBufTmp;
    *outBufLen = sizeof(struct msg_head);

    return 0;
}

STATIC int RaRsSocketBatchConnect(char *inBuf, char *outBuf, int *outLen, int *opResult, int rcvBufLen)
{
    union op_socket_connect_data *socketConnectData =
        (union op_socket_connect_data *)(inBuf + sizeof(struct msg_head));
    unsigned int usePort = 0;
    unsigned int i;

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_socket_connect_data), sizeof(struct msg_head), rcvBufLen,
        opResult);

    // clear resv bit 31 use_port, for compatibility issue
    usePort = socketConnectData->tx_data.num >> SOCKET_USE_PORT_BIT;
    socketConnectData->tx_data.num &= ~(1U << SOCKET_USE_PORT_BIT);
    HCCP_CHECK_PARAM_NUM(socketConnectData->tx_data.num, MAX_SOCKET_NUM);

    for (i = 0; i < (socketConnectData->tx_data).num; i++) {
        // use_port flag not specify, use default port for compatibility issue
        if (usePort == 0) {
            (socketConnectData->tx_data).conn[i].port = RS_SOCK_PORT_DEF;
        } else if ((socketConnectData->tx_data).conn[i].port > MAX_PORT_NUM) {
            hccp_err("[batch_connect]conn[%u].port=%u invalid", i, (socketConnectData->tx_data).conn[i].port);
            return -EINVAL;
        }
    }

    *opResult = gRaRsOps.socket_batch_connect((socketConnectData->tx_data).conn,
        (socketConnectData->tx_data).num);
    if (*opResult != 0) {
        hccp_err("socket batch connect failed ret[%d].", *opResult);
    }

    return 0;
}

STATIC int RaRsSocketBatchClose(char *inBuf, char *outBuf, int *outLen, int *opResult, int rcvBufLen)
{
    union op_socket_close_data *socketCloseData = (union op_socket_close_data *)(inBuf + sizeof(struct msg_head));
    int disuseLinger = 0;
    unsigned int i;

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_socket_close_data), sizeof(struct msg_head), rcvBufLen, opResult);

    // clear resv bit 31 disuse_linger, for compatibility issue(0 by default)
    disuseLinger = socketCloseData->tx_data.num >> SOCKET_DISUSE_LINGER_BIT;
    socketCloseData->tx_data.num &= ~(1U << SOCKET_DISUSE_LINGER_BIT);
    HCCP_CHECK_PARAM_NUM(socketCloseData->tx_data.num, MAX_SOCKET_NUM);

    struct rs_socket_close_info_t closeConn[MAX_SOCKET_NUM] = {0};
    for (i = 0; i < socketCloseData->tx_data.num; i++) {
        closeConn[i].fd = ((socketCloseData->tx_data).conn[i]).close_fd;
    }
    *opResult = gRaRsOps.socket_batch_close(disuseLinger, closeConn, (socketCloseData->tx_data).num);
    if (*opResult != 0) {
        hccp_err("socket batch close failed ret[%d].", *opResult);
    }

    return 0;
}

STATIC int RaRsSocketBatchAbort(char *inBuf, char *outBuf, int *outLen, int *opResult, int rcvBufLen)
{
    union op_socket_connect_data *socketConnectData = (union op_socket_connect_data *)(inBuf +
        sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_socket_connect_data), sizeof(struct msg_head), rcvBufLen,
        opResult);
    HCCP_CHECK_PARAM_NUM(socketConnectData->tx_data.num, MAX_SOCKET_NUM);

    *opResult = gRaRsOps.socket_batch_abort((socketConnectData->tx_data).conn,
        (socketConnectData->tx_data).num);
    if (*opResult != 0) {
        hccp_err("socket batch abort failed ret[%d]", *opResult);
    }

    return 0;
}

STATIC int RaRsSocketListenStart(char *inBuf, char *outBuf, int *outLen, int *opResult, int rcvBufLen)
{
    union op_socket_listen_data *socketListenData = (union op_socket_listen_data *)(inBuf + sizeof(struct msg_head));
    union op_socket_listen_data *socketListenDataReturn = NULL;
    unsigned int usePort = 0;
    unsigned int i;
    int ret;

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_socket_listen_data), sizeof(struct msg_head), rcvBufLen, opResult);

    // clear resv bit 31 use_port, for compatibility issue
    usePort = socketListenData->tx_data.num >> SOCKET_USE_PORT_BIT;
    socketListenData->tx_data.num &= ~(1U << SOCKET_USE_PORT_BIT);
    HCCP_CHECK_PARAM_LEN_RET_HOST(socketListenData->tx_data.num, 0, MAX_SOCKET_NUM, opResult);

    for (i = 0; i < (socketListenData->tx_data).num; i++) {
        // use_port flag not specify, use default port for compatibility issue
        if (usePort == 0) {
            (socketListenData->tx_data).conn[i].port = RS_SOCK_PORT_DEF;
        } else if ((socketListenData->tx_data).conn[i].port > MAX_PORT_NUM) {
            hccp_err("[listen_start]conn[%u].port=%u invalid", i, (socketListenData->tx_data).conn[i].port);
            return -EINVAL;
        }
    }
    *opResult = gRaRsOps.socket_listen_start((socketListenData->tx_data).conn, (socketListenData->tx_data).num);
    if (*opResult == -EADDRINUSE) {
        hccp_run_warn("socket listen start unsuccessful ret[%d]", *opResult);
        return 0;
    } else if (*opResult != 0) {
        hccp_err("socket listen start failed ret[%d]", *opResult);
        return 0;
    }

    socketListenDataReturn = (union op_socket_listen_data *)(outBuf + sizeof(struct msg_head));
    ret = memcpy_s((socketListenDataReturn->rx_data).conn, sizeof(struct socket_listen_info) * MAX_SOCKET_NUM,
        (socketListenData->tx_data).conn, sizeof(struct socket_listen_info) * (socketListenData->tx_data).num);
    CHK_PRT_RETURN(ret, hccp_err("memcpy_s socket_listen_info failed, ret[%d]", ret), -ESAFEFUNC);
    return 0;
}

STATIC int RaRsSocketListenStop(char *inBuf, char *outBuf, int *outLen, int *opResult, int rcvBufLen)
{
    union op_socket_listen_data *socketListenData = (union op_socket_listen_data *)(inBuf + sizeof(struct msg_head));
    unsigned int usePort = 0;
    unsigned int i;

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_socket_listen_data), sizeof(struct msg_head), rcvBufLen, opResult);

    // clear resv bit 31 use_port, for compatibility issue
    usePort = socketListenData->tx_data.num >> SOCKET_USE_PORT_BIT;
    socketListenData->tx_data.num &= ~(1U << SOCKET_USE_PORT_BIT);
    HCCP_CHECK_PARAM_LEN_RET_HOST(socketListenData->tx_data.num, 0, MAX_SOCKET_NUM, opResult);

    for (i = 0; i < (socketListenData->tx_data).num; i++) {
        // use_port flag not specify, use default port for compatibility issue
        if (usePort == 0) {
            (socketListenData->tx_data).conn[i].port = RS_SOCK_PORT_DEF;
        } else if ((socketListenData->tx_data).conn[i].port > MAX_PORT_NUM) {
            hccp_err("[listen_stop]conn[%u].port=%u invalid", i, (socketListenData->tx_data).conn[i].port);
            return -EINVAL;
        }
    }

    *opResult = gRaRsOps.socket_listen_stop((socketListenData->tx_data).conn, (socketListenData->tx_data).num);
    if (*opResult != 0) {
        hccp_err("socket listen stop failed ret[%d].", *opResult);
    }

    return 0;
}

STATIC int RaRsGetSockets(char *inBuf, char *outBuf, int *outLen, int *opResult, int rcvBufLen)
{
    int ret;
    union op_socket_info_data *socketInfoDataReturn = NULL;
    union op_socket_info_data *socketInfoData = (union op_socket_info_data *)(inBuf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_socket_info_data), sizeof(struct msg_head), rcvBufLen, opResult);
    HCCP_CHECK_PARAM_LEN_RET_HOST(socketInfoData->tx_data.num, 0, MAX_SOCKET_NUM, opResult);

    *opResult = gRaRsOps.get_sockets(socketInfoData->tx_data.role, socketInfoData->tx_data.conn,
        socketInfoData->tx_data.num);
    if (*opResult < 0) {
        hccp_err("socket info get failed ret[%d].", *opResult);
        return 0;
    }

    socketInfoDataReturn = (union op_socket_info_data *)(outBuf + sizeof(struct msg_head));

    (socketInfoDataReturn->rx_data).num = *opResult;
    ret = memcpy_s((socketInfoDataReturn->rx_data).conn, sizeof(struct socket_fd_data) * MAX_SOCKET_NUM,
        (socketInfoData->tx_data).conn, sizeof(struct socket_fd_data) * (socketInfoData->tx_data).num);
    CHK_PRT_RETURN(ret, hccp_err("ra_rs_get_sockets memcpy_s failed, ret[%d]. ", ret), -ESAFEFUNC);

    return 0;
}

STATIC int RaRsSocketRecv(char *inBuf, char *outBuf, int *outLen, int *opResult, int rcvBufLen)
{
    int recvLen;
    union op_socket_recv_data *recvData = (union op_socket_recv_data *)(inBuf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_socket_recv_data), sizeof(struct msg_head), rcvBufLen, opResult);
    HCCP_CHECK_PARAM_LEN(sizeof(union op_socket_recv_data) + recvData->tx_data.recv_size, sizeof(struct msg_head),
        rcvBufLen);

    recvLen = gRaRsOps.socket_recv(recvData->tx_data.fd,
        outBuf + sizeof(struct msg_head) + sizeof(union op_socket_recv_data), recvData->tx_data.recv_size);
    *opResult = recvLen;

    recvData = (union op_socket_recv_data *)(outBuf + sizeof(struct msg_head));
    recvData->rx_data.real_recv_size = recvLen;

    return 0;
}

STATIC int RaRsSocketSend(char *inBuf, char *outBuf, int *outLen, int *opResult, int rcvBufLen)
{
    int sendLen;
    union op_socket_send_data *sendData = (union op_socket_send_data *)(inBuf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_socket_send_data), sizeof(struct msg_head), rcvBufLen, opResult);
    HCCP_CHECK_PARAM_LEN_RET_HOST(sendData->tx_data.send_size, 0, SOCKET_SEND_MAXLEN, opResult);

    sendLen =
        gRaRsOps.socket_send(sendData->tx_data.fd, sendData->tx_data.data_send, sendData->tx_data.send_size);
    if (sendLen <= 0) {
        if (sendLen == -EAGAIN) {
            hccp_dbg("socket send need retry, ret[%d]", sendLen);
        }else {
            hccp_warn("send unsuccessful, sendLen[%d] expect greater than 0.", sendLen);
        }
    }

    *opResult = sendLen;
    sendData = (union op_socket_send_data *)(outBuf + sizeof(struct msg_head));
    sendData->rx_data.real_send_size = sendLen;

    return 0;
}

STATIC int RaRsRdevInit(char *inBuf, char *outBuf, int *outLen, int *opResult, int rcvBufLen)
{
    unsigned int rdevIndex = 0;
    union op_rdev_init_data *rdevInitData = (union op_rdev_init_data *)(inBuf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_rdev_init_data), sizeof(struct msg_head), rcvBufLen, opResult);

    *opResult = gRaRsOps.rdev_init(rdevInitData->tx_data.rdev_info, NOTIFY, &rdevIndex);
    if (*opResult != 0) {
        hccp_err("rdev_init failed ret[%d].", *opResult);
        return 0;
    }

    rdevInitData = (union op_rdev_init_data *)(outBuf + sizeof(struct msg_head));
    rdevInitData->rx_data.rdev_index = rdevIndex;

    return 0;
}

STATIC int RaRsRdevInitWithBackup(char *inBuf, char *outBuf, int *outLen, int *opResult, int rcvBufLen)
{
    union op_rdev_init_with_backup_data *rdevInitData = (union op_rdev_init_with_backup_data *)(inBuf +
        sizeof(struct msg_head));
    unsigned int rdevIndex = 0;

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_rdev_init_with_backup_data), sizeof(struct msg_head),
        rcvBufLen, opResult);

    *opResult = gRaRsOps.rdev_init_with_backup(rdevInitData->tx_data.rdev_info,
        rdevInitData->tx_data.backup_rdev_info, NOTIFY, &rdevIndex);
    if (*opResult != 0) {
        hccp_err("rdev_init_with_backup failed ret[%d].", *opResult);
        return 0;
    }

    rdevInitData = (union op_rdev_init_with_backup_data *)(outBuf + sizeof(struct msg_head));
    rdevInitData->rx_data.rdev_index = rdevIndex;

    return 0;
}

STATIC int RaRsRdevGetPortStatus(char *inBuf, char *outBuf, int *outLen, int *opResult, int rcvBufLen)
{
    union op_rdev_get_port_status_data *statusData = NULL;
    enum port_status status = PORT_STATUS_DOWN;

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_rdev_get_port_status_data), sizeof(struct msg_head), rcvBufLen,
        opResult);

    statusData = (union op_rdev_get_port_status_data *)(inBuf + sizeof(struct msg_head));
    *opResult = gRaRsOps.rdev_get_port_status(statusData->tx_data.phy_id,
        statusData->tx_data.rdev_index, &status);
    if (*opResult != 0) {
        hccp_err("rdev_get_port_status failed ret[%d].", *opResult);
        return 0;
    }

    statusData = (union op_rdev_get_port_status_data *)(outBuf + sizeof(struct msg_head));
    statusData->rx_data.status = status;

    return 0;
}

STATIC int RaRsRdevDeinit(char *inBuf, char *outBuf, int *outLen, int *opResult, int rcvBufLen)
{
    union op_rdev_deinit_data *rdevDeinitData = (union op_rdev_deinit_data *)(inBuf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_rdev_deinit_data), sizeof(struct msg_head), rcvBufLen, opResult);

    *opResult = gRaRsOps.rdev_deinit(rdevDeinitData->tx_data.phy_id, NOTIFY,
        rdevDeinitData->tx_data.rdev_index);
    if (*opResult != 0) {
        hccp_err("rdev_deinit failed ret[%d].", *opResult);
    }

    return 0;
}

STATIC int RaRsSocketInit(char *inBuf, char *outBuf, int *outLen, int *opResult, int rcvBufLen)
{
    union op_socket_init_data *socketInitData = (union op_socket_init_data *)(inBuf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_socket_init_data), sizeof(struct msg_head), rcvBufLen, opResult);

    *opResult = gRaRsOps.socket_init(socketInitData->tx_data.vnic_ip, socketInitData->tx_data.num);
    if (*opResult != 0) {
        hccp_err("socket init failed ret[%d].", *opResult);
    }

    return 0;
}

STATIC int RaRsSocketDeinit(char *inBuf, char *outBuf, int *outLen, int *opResult, int rcvBufLen)
{
    union op_socket_deinit_data *socketDeinitData = (union op_socket_deinit_data *)(inBuf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_socket_deinit_data), sizeof(struct msg_head), rcvBufLen, opResult);

    *opResult = gRaRsOps.socket_deinit(socketDeinitData->tx_data.rdev_info);
    if (*opResult != 0) {
        hccp_err("socket deinit failed ret[%d].", *opResult);
    }

    return 0;
}

STATIC int RaRsGetTsqpDepth(char *inBuf, char *outBuf, int *outLen, int *opResult, int rcvBufLen)
{
    unsigned int tempDepth = 0;
    unsigned int qpNum = 0;
    union op_get_tsqp_depth_data *getTsqpDepthData = (union op_get_tsqp_depth_data *)(inBuf +
        sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_get_tsqp_depth_data), sizeof(struct msg_head), rcvBufLen,
        opResult);

    *opResult = gRaRsOps.get_tsqp_depth(getTsqpDepthData->tx_data.phy_id,
        getTsqpDepthData->tx_data.rdev_index, &tempDepth, &qpNum);
    if (*opResult != 0) {
        hccp_err("set_tsqp_depth failed ret[%d].", *opResult);
        return 0;
    }

    getTsqpDepthData = (union op_get_tsqp_depth_data *)(outBuf + sizeof(struct msg_head));
    getTsqpDepthData->rx_data.temp_depth = tempDepth;
    getTsqpDepthData->rx_data.qp_num = qpNum;

    return 0;
}

STATIC int RaRsSetTsqpDepth(char *inBuf, char *outBuf, int *outLen, int *opResult, int rcvBufLen)
{
    unsigned int qpNum = 0;
    union op_set_tsqp_depth_data *setTsqpDepthData = (union op_set_tsqp_depth_data *)(inBuf +
        sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_set_tsqp_depth_data), sizeof(struct msg_head), rcvBufLen,
        opResult);

    *opResult = gRaRsOps.set_tsqp_depth(setTsqpDepthData->tx_data.phy_id,
        setTsqpDepthData->tx_data.rdev_index, setTsqpDepthData->tx_data.temp_depth, &qpNum);
    if (*opResult != 0) {
        hccp_err("set_tsqp_depth failed ret[%d].", *opResult);
        return 0;
    }

    setTsqpDepthData = (union op_set_tsqp_depth_data *)(outBuf + sizeof(struct msg_head));
    setTsqpDepthData->rx_data.qp_num = qpNum;

    return 0;
}

STATIC int RaRsQpCreate(char *inBuf, char *outBuf, int *outLen, int *opResult, int rcvBufLen)
{
    struct rs_qp_norm qpNorm;
    struct rs_qp_resp qpResp = { 0 };
    union op_qp_create_data *createData = (union op_qp_create_data *)(inBuf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_qp_create_data), sizeof(struct msg_head), rcvBufLen, opResult);

    int qpMode = createData->tx_data.qp_mode;
    qpNorm.flag = createData->tx_data.flag;
    qpNorm.is_exp = 1;
    qpNorm.is_ext = 1;
    if (qpMode == RA_RS_OP_QP_MODE_EXT) {
        qpNorm.qp_mode = RA_RS_OP_QP_MODE;
    } else {
        qpNorm.qp_mode = qpMode;
    }
    qpNorm.mem_align = createData->tx_data.mem_align;

    *opResult = gRaRsOps.qp_create(createData->tx_data.phy_id, createData->tx_data.rdev_index, qpNorm, &qpResp);
    if (*opResult != 0) {
        hccp_err("qp create failed ret[%d].", *opResult);
        return 0;
    }

    createData = (union op_qp_create_data *)(outBuf + sizeof(struct msg_head));
    createData->rx_data.qpn = qpResp.qpn;
    createData->rx_data.psn = qpResp.psn;
    createData->rx_data.gid_idx = qpResp.gid_idx;

    return 0;
}

STATIC int RaRsQpCreateWithAttrs(char *inBuf, char *outBuf, int *outLen, int *opResult, int rcvBufLen)
{
    union op_qp_create_with_attrs_data *createData = NULL;
    struct rs_qp_norm_with_attrs qpNorm = { 0 };
    struct rs_qp_resp_with_attrs qpResp = { 0 };

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_qp_create_with_attrs_data), sizeof(struct msg_head), rcvBufLen,
        opResult);

    createData = (union op_qp_create_with_attrs_data *)(inBuf + sizeof(struct msg_head));

    qpNorm.is_exp = 1;
    qpNorm.is_ext = 1;
    qpNorm.ext_attrs = createData->tx_data.ext_attrs;

    *opResult = gRaRsOps.qp_create_with_attrs(createData->tx_data.phy_id, createData->tx_data.rdev_index,
        &qpNorm, &qpResp);
    if (*opResult != 0) {
        hccp_err("qp create failed ret[%d].", *opResult);
        return 0;
    }

    createData = (union op_qp_create_with_attrs_data *)(outBuf + sizeof(struct msg_head));
    createData->rx_data.qpn = qpResp.qpn;
    createData->rx_data.psn = qpResp.psn;
    createData->rx_data.gid_idx = qpResp.gid_idx;

    return 0;
}

STATIC int RaRsAiQpCreate(char *inBuf, char *outBuf, int *outLen, int *opResult, int rcvBufLen)
{
    union op_ai_qp_create_data *createData = NULL;
    struct rs_qp_norm_with_attrs qpNorm = { 0 };
    struct rs_qp_resp_with_attrs qpResp = { 0 };

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_ai_qp_create_data), sizeof(struct msg_head), rcvBufLen,
        opResult);

    createData = (union op_ai_qp_create_data *)(inBuf + sizeof(struct msg_head));

    qpNorm.is_exp = 1;
    qpNorm.is_ext = 1;
    qpNorm.ext_attrs = createData->tx_data.ext_attrs;
    qpNorm.ai_op_support = 1;

    *opResult = gRaRsOps.qp_create_with_attrs(createData->tx_data.phy_id, createData->tx_data.rdev_index,
        &qpNorm, &qpResp);
    if (*opResult != 0) {
        hccp_err("qp create failed ret[%d].", *opResult);
        return 0;
    }

    createData = (union op_ai_qp_create_data *)(outBuf + sizeof(struct msg_head));
    createData->rx_data.qpn = qpResp.qpn;
    createData->rx_data.ai_qp_addr = qpResp.ai_qp_addr;
    createData->rx_data.sq_index = qpResp.sq_index;
    createData->rx_data.db_index = qpResp.db_index;
    createData->rx_data.psn = qpResp.psn;

    return 0;
}

STATIC int RaRsAiQpCreateWithData(char *inBuf, char *outBuf, int *outLen, int *opResult, int rcvBufLen)
{
    union op_ai_qp_create_with_attrs_data *createData = NULL;
    struct rs_qp_norm_with_attrs qpNorm = { 0 };
    struct rs_qp_resp_with_attrs qpResp = { 0 };

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_ai_qp_create_with_attrs_data), sizeof(struct msg_head), rcvBufLen,
        opResult);

    createData = (union op_ai_qp_create_with_attrs_data *)(inBuf + sizeof(struct msg_head));

    qpNorm.is_exp = 1;
    qpNorm.is_ext = 1;
    qpNorm.ext_attrs = createData->tx_data.ext_attrs;
    qpNorm.ai_op_support = 1;

    *opResult = gRaRsOps.qp_create_with_attrs(createData->tx_data.phy_id, createData->tx_data.rdev_index,
        &qpNorm, &qpResp);
    if (*opResult != 0) {
        hccp_err("qp create failed ret[%d].", *opResult);
        return 0;
    }

    createData = (union op_ai_qp_create_with_attrs_data *)(outBuf + sizeof(struct msg_head));
    createData->rx_data.qpn = qpResp.qpn;
    createData->rx_data.gid_idx = qpResp.gid_idx;
    createData->rx_data.psn = qpResp.psn;
    createData->rx_data.ai_qp_addr = qpResp.ai_qp_addr;
    createData->rx_data.sq_index = qpResp.sq_index;
    createData->rx_data.db_index = qpResp.db_index;
    createData->rx_data.ai_scq_addr = qpResp.ai_scq_addr;
    createData->rx_data.ai_rcq_addr = qpResp.ai_rcq_addr;
    (void)memcpy_s(&createData->rx_data.data_plane_info, sizeof(struct ai_data_plane_info), &qpResp.data_plane_info,
        sizeof(struct ai_data_plane_info));

    return 0;
}

STATIC int RaRsTypicalQpCreate(char *inBuf, char *outBuf, int *outLen, int *opResult, int rcvBufLen)
{
    union op_typical_qp_create_data *createData = (union op_typical_qp_create_data *)(inBuf +
        sizeof(struct msg_head));
    struct rs_qp_resp qpResp = {0};
    struct rs_qp_norm qpNorm;
    int qpMode;

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_typical_qp_create_data), sizeof(struct msg_head), rcvBufLen,
        opResult);

    qpMode = createData->tx_data.qp_mode;
    qpNorm.flag = createData->tx_data.flag;
    qpNorm.is_exp = 1;
    qpNorm.is_ext = 1;
    if (qpMode == RA_RS_OP_QP_MODE_EXT) {
        qpNorm.qp_mode = RA_RS_OP_QP_MODE;
    } else {
        qpNorm.qp_mode = qpMode;
    }
    qpNorm.mem_align = createData->tx_data.mem_align;

    *opResult = gRaRsOps.qp_create(createData->tx_data.phy_id, createData->tx_data.rdev_index, qpNorm, &qpResp);
    if (*opResult != 0) {
        hccp_err("qp create failed ret[%d].", *opResult);
        return 0;
    }

    createData = (union op_typical_qp_create_data *)(outBuf + sizeof(struct msg_head));
    createData->rx_data.qpn = qpResp.qpn;
    createData->rx_data.gid_idx = qpResp.gid_idx;
    createData->rx_data.psn = qpResp.psn;
    createData->rx_data.gid = qpResp.gid;

    return 0;
}

STATIC int RaRsQpDestroy(char *inBuf, char *outBuf, int *outLen, int *opResult, int rcvBufLen)
{
    union op_qp_destroy_data *qpDestroyData = (union op_qp_destroy_data *)(inBuf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_qp_destroy_data), sizeof(struct msg_head), rcvBufLen, opResult);

    *opResult = gRaRsOps.qp_destroy(qpDestroyData->tx_data.phy_id, qpDestroyData->tx_data.rdev_index,
        qpDestroyData->tx_data.qpn);
    if (*opResult != 0) {
        hccp_err("qp destroy failed ret[%d].", *opResult);
    }

    return 0;
}

STATIC int RaRsTypicalQpModify(char *inBuf, char *outBuf, int *outLen, int *opResult, int rcvBufLen)
{
    union op_typical_qp_modify_data *qpModifyData = (union op_typical_qp_modify_data *)(inBuf +
        sizeof(struct msg_head));
    unsigned int udpSport = 0;

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_typical_qp_modify_data), sizeof(struct msg_head), rcvBufLen,
        opResult);

    *opResult = gRaRsOps.typical_qp_modify(qpModifyData->tx_data.phy_id, qpModifyData->tx_data.rdev_index,
        qpModifyData->tx_data.local_qp_info, qpModifyData->tx_data.remote_qp_info,
        &udpSport);
    if (*opResult != 0) {
        hccp_err("qp info modify failed ret[%d].", *opResult);
        return 0;
    }

    qpModifyData = (union op_typical_qp_modify_data *)(outBuf + sizeof(struct msg_head));
    qpModifyData->rx_data.udp_sport = udpSport;

    return 0;
}

STATIC int RaRsQpBatchModify(char *inBuf, char *outBuf, int *outLen, int *opResult, int rcvBufLen)
{
    union op_qp_batch_modify_data *qpBatchModifyData = (union op_qp_batch_modify_data *)(inBuf +
        sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_qp_batch_modify_data), sizeof(struct msg_head), rcvBufLen,
        opResult);
    HCCP_CHECK_PARAM_LEN_RET_HOST(qpBatchModifyData->tx_data.qpn_num, 0, RA_MAX_BATCH_QP_MODIFY_NUM, opResult);

    *opResult = gRaRsOps.qp_batch_modify(qpBatchModifyData->tx_data.phy_id,
        qpBatchModifyData->tx_data.rdev_index, qpBatchModifyData->tx_data.status,
        qpBatchModifyData->tx_data.qpn, qpBatchModifyData->tx_data.qpn_num);
    if (*opResult != 0) {
        hccp_err("qp info modify failed ret[%d].", *opResult);
    }

    return 0;
}

STATIC int RaRsQpConnectAsync(char *inBuf, char *outBuf, int *outLen, int *opResult, int rcvBufLen)
{
    union op_qp_connect_data *qpConnectData = (union op_qp_connect_data *)(inBuf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_qp_connect_data), sizeof(struct msg_head), rcvBufLen, opResult);

    *opResult = gRaRsOps.qp_connect_async(qpConnectData->tx_data.phy_id, qpConnectData->tx_data.rdev_index,
        qpConnectData->tx_data.qpn, qpConnectData->tx_data.fd);
    if (*opResult != 0) {
        hccp_err("qp info async failed ret[%d].", *opResult);
    }

    return 0;
}

STATIC int RaRsGetQpStatus(char *inBuf, char *outBuf, int *outLen, int *opResult, int rcvBufLen)
{
    union op_qp_status_data *qpStatusData = (union op_qp_status_data *)(inBuf + sizeof(struct msg_head));
    struct rs_qp_status_info qpInfo = { 0 };

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_qp_status_data), sizeof(struct msg_head), rcvBufLen, opResult);

    *opResult = gRaRsOps.get_qp_status(qpStatusData->tx_data.phy_id, qpStatusData->tx_data.rdev_index,
        qpStatusData->tx_data.qpn, &qpInfo);
    if (*opResult != 0) {
        hccp_err("query qp status async failed ret[%d].", *opResult);
        return 0;
    }

    qpStatusData = (union op_qp_status_data *)(outBuf + sizeof(struct msg_head));
    qpStatusData->rx_data.status = qpInfo.status;

    return 0;
}

STATIC int RaRsGetQpInfo(char *inBuf, char *outBuf, int *outLen, int *opResult, int rcvBufLen)
{
    union op_qp_info_data *qpInfoData = (union op_qp_info_data *)(inBuf + sizeof(struct msg_head));
    struct rs_qp_status_info qpInfo = { 0 };

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_qp_info_data), sizeof(struct msg_head), rcvBufLen, opResult);

    *opResult = gRaRsOps.get_qp_status(qpInfoData->tx_data.phy_id, qpInfoData->tx_data.rdev_index,
        qpInfoData->tx_data.qpn, &qpInfo);
    if (*opResult != 0) {
        hccp_err("query qp status async failed ret[%d].", *opResult);
        return 0;
    }

    qpInfoData = (union op_qp_info_data *)(outBuf + sizeof(struct msg_head));
    qpInfoData->rx_data.status = qpInfo.status;
    qpInfoData->rx_data.udp_sport = qpInfo.udp_sport;

    return 0;
}

STATIC int RaRsMrReg(char *inBuf, char *outBuf, int *outLen, int *opResult, int rcvBufLen)
{
    union op_mr_reg_data *regMrData = (union op_mr_reg_data *)(inBuf + sizeof(struct msg_head));
    struct rdma_mr_reg_info mrRegInfo = { 0 };

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_mr_reg_data), sizeof(struct msg_head), rcvBufLen, opResult);

    mrRegInfo.addr = regMrData->tx_data.mr_reg_attr.addr;
    mrRegInfo.len = regMrData->tx_data.mr_reg_attr.len;
    mrRegInfo.access = regMrData->tx_data.mr_reg_attr.access;
    *opResult = gRaRsOps.mr_reg(regMrData->tx_data.phy_id, regMrData->tx_data.rdev_index,
        regMrData->tx_data.qpn, &mrRegInfo);
    if (*opResult != 0) {
        hccp_err("reg_mr failed ret[%d].", *opResult);
        return 0;
    }

    regMrData = (union op_mr_reg_data *)(outBuf + sizeof(struct msg_head));
    regMrData->rx_data.lkey = mrRegInfo.lkey;
    regMrData->rx_data.rkey = mrRegInfo.rkey;

    return 0;
}

STATIC int RaRsMrDereg(char *inBuf, char *outBuf, int *outLen, int *opResult, int rcvBufLen)
{
    union op_mr_dereg_data *mrDeregData = (union op_mr_dereg_data *)(inBuf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_mr_dereg_data), sizeof(struct msg_head), rcvBufLen, opResult);

    *opResult = gRaRsOps.mr_dereg(mrDeregData->tx_data.phy_id, mrDeregData->tx_data.rdev_index,
        mrDeregData->tx_data.qpn, mrDeregData->tx_data.addr);
    if (*opResult != 0) {
        hccp_err("dereg_mr failed ret[%d].", *opResult);
    }

    return 0;
}

STATIC int RaRsTypicalMrRegV1(char *inBuf, char *outBuf, int *outLen, int *opResult, int rcvBufLen)
{
    union op_typical_mr_reg_data *regMrData = (union op_typical_mr_reg_data *)(inBuf + sizeof(struct msg_head));
    struct rdma_mr_reg_info mrRegInfo = { 0 };
    struct ibv_mr *raRsMrHandle = NULL;

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_typical_mr_reg_data), sizeof(struct msg_head),
        rcvBufLen, opResult);

    mrRegInfo.addr = regMrData->tx_data.mr_reg_attr.addr;
    mrRegInfo.len = regMrData->tx_data.mr_reg_attr.len;
    mrRegInfo.access = regMrData->tx_data.mr_reg_attr.access;
    *opResult = gRaRsOps.register_mr(regMrData->tx_data.phy_id, regMrData->tx_data.rdev_index,
        &mrRegInfo, (void **)&raRsMrHandle);
    if (*opResult != 0) {
        hccp_err("reg_mr failed ret[%d].", *opResult);
        return 0;
    }

    regMrData = (union op_typical_mr_reg_data *)(outBuf + sizeof(struct msg_head));
    regMrData->rx_data.lkey = mrRegInfo.lkey;
    regMrData->rx_data.rkey = mrRegInfo.rkey;

    return 0;
}

STATIC int RaRsTypicalMrReg(char *inBuf, char *outBuf, int *outLen, int *opResult, int rcvBufLen)
{
    union op_typical_mr_reg_data *regMrData = (union op_typical_mr_reg_data *)(inBuf + sizeof(struct msg_head));
    struct rdma_mr_reg_info mrRegInfo = { 0 };
    struct ibv_mr *raRsMrHandle = NULL;

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_typical_mr_reg_data), sizeof(struct msg_head),
        rcvBufLen, opResult);

    mrRegInfo.addr = regMrData->tx_data.mr_reg_attr.addr;
    mrRegInfo.len = regMrData->tx_data.mr_reg_attr.len;
    mrRegInfo.access = regMrData->tx_data.mr_reg_attr.access;
    *opResult = gRaRsOps.typical_register_mr(regMrData->tx_data.phy_id, regMrData->tx_data.rdev_index,
        &mrRegInfo, (void **)&raRsMrHandle);
    if (*opResult != 0) {
        hccp_err("reg_mr failed ret[%d].", *opResult);
        return 0;
    }

    regMrData = (union op_typical_mr_reg_data *)(outBuf + sizeof(struct msg_head));
    regMrData->rx_data.lkey = mrRegInfo.lkey;
    regMrData->rx_data.rkey = mrRegInfo.rkey;
    regMrData->rx_data.addr = (uint64_t)(uintptr_t)raRsMrHandle;

    return 0;
}

STATIC int RaRsRemapMr(char *inBuf, char *outBuf, int *outLen, int *opResult, int rcvBufLen)
{
    union op_remap_mr_data *opData = (union op_remap_mr_data *)(inBuf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_remap_mr_data), sizeof(struct msg_head), rcvBufLen, opResult);
    HCCP_CHECK_PARAM_LEN_RET_HOST(opData->tx_data.mem_num, 0, REMAP_MR_MAX_NUM, opResult);

    *opResult = gRaRsOps.remap_mr(opData->tx_data.phy_id, opData->tx_data.rdev_index, opData->tx_data.mem_list,
        opData->tx_data.mem_num);
    if (*opResult) {
        hccp_err("remap_mr failed ret[%d]", *opResult);
    }
    return 0;
}

STATIC int RaRsTypicalMrDereg(char *inBuf, char *outBuf, int *outLen, int *opResult, int rcvBufLen)
{
    union op_typical_mr_dereg_data *mrDeregData =
        (union op_typical_mr_dereg_data *)(inBuf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_typical_mr_dereg_data), sizeof(struct msg_head),
        rcvBufLen, opResult);

    *opResult = gRaRsOps.typical_deregister_mr(mrDeregData->tx_data.phy_id, mrDeregData->tx_data.rdev_index,
        mrDeregData->tx_data.addr);
    if (*opResult != 0) {
        hccp_err("dereg_mr failed ret[%d].", *opResult);
    }

    return 0;
}

STATIC int RaRsSendWr(char *inBuf, char *outBuf, int *outLen, int *opResult, int rcvBufLen)
{
    int ret;

    struct send_wr_rsp wrRsp = { 0 };
    union op_send_wr_data *sendWrData = (union op_send_wr_data *)(inBuf + sizeof(struct msg_head));
    struct send_wr sWr = { 0 };

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_send_wr_data), sizeof(struct msg_head), rcvBufLen, opResult);

    sWr.buf_num = sendWrData->tx_data.buf_num;
    sWr.buf_list = (struct sg_list *)&sendWrData->tx_data.mem_list[0];
    sWr.dst_addr = sendWrData->tx_data.dst_addr;
    sWr.op = sendWrData->tx_data.op;
    sWr.send_flag = sendWrData->tx_data.send_flags;

    ret = gRaRsOps.send_wr(sendWrData->tx_data.phy_id, sendWrData->tx_data.rdev_index, sendWrData->tx_data.qpn,
        &sWr, &wrRsp);
    *opResult = ret;
    if (ret) {
        if (ret == -ENOENT) {
            hccp_warn("not found remote mr_info, need try again");
        } else {
            hccp_err("send wr failed ret[%d].", ret);
        }
    }

    sendWrData = (union op_send_wr_data *)(outBuf + sizeof(struct msg_head));
    sendWrData->rx_data.wr_rsp = wrRsp;

    return 0;
}

STATIC int RaRsSendWrList(char *inBuf, char *outBuf, int *outLen, int *opResult, int rcvBufLen)
{
    int ret;
    uint32_t i;
    unsigned int completeNum = 0;
    struct send_wr_rsp *wrRsp = NULL;
    struct wr_info *wrList = NULL;
    struct rs_wrlist_base_info baseInfo = {0};
    union op_send_wrlist_data *sendWrlist = (union op_send_wrlist_data *)(inBuf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_send_wrlist_data), sizeof(struct msg_head), rcvBufLen, opResult);
    HCCP_CHECK_PARAM_LEN_RET_HOST(sendWrlist->tx_data.send_num, 0,  MAX_WR_NUM, opResult);

    wrRsp = calloc(sendWrlist->tx_data.send_num, sizeof(struct send_wr_rsp));
    CHK_PRT_RETURN(wrRsp == NULL, hccp_err("wr_rsp calloc failed."), -ENOMEM);

    wrList = calloc(sendWrlist->tx_data.send_num, sizeof(struct wr_info));
    if (wrList == NULL) {
        hccp_err("wr_list calloc failed.");
        ret = -ENOMEM;
        goto alloc_wr_list_fail;
    }

    baseInfo.phy_id = sendWrlist->tx_data.phy_id;
    baseInfo.rdev_index = sendWrlist->tx_data.rdev_index;
    baseInfo.qpn = sendWrlist->tx_data.qpn;
    baseInfo.key_flag = 0;

    for (i = 0; i < sendWrlist->tx_data.send_num; i++) {
        wrList[i].op = sendWrlist->tx_data.wrlist[i].op;
        wrList[i].send_flags = sendWrlist->tx_data.wrlist[i].send_flags;
        wrList[i].dst_addr = sendWrlist->tx_data.wrlist[i].dst_addr;
        wrList[i].mem_list.addr = sendWrlist->tx_data.wrlist[i].mem_list.addr;
        wrList[i].mem_list.len = sendWrlist->tx_data.wrlist[i].mem_list.len;
        wrList[i].mem_list.lkey = sendWrlist->tx_data.wrlist[i].mem_list.lkey;
    }
    ret = gRaRsOps.send_wr_list(baseInfo, wrList, sendWrlist->tx_data.send_num, wrRsp, &completeNum);
    *opResult = ret;
    if (ret) {
        if (ret == -ENOENT) {
            hccp_warn("not found remote mr_info, need try again");
        } else {
            hccp_err("send wr failed ret[%d].", ret);
        }
    }
    sendWrlist = (union op_send_wrlist_data *)(outBuf + sizeof(struct msg_head));
    sendWrlist->rx_data.complete_num = completeNum;
    ret = memcpy_s(sendWrlist->rx_data.wr_rsp, sizeof(struct send_wr_rsp) * MAX_WR_NUM_V1, wrRsp,
        completeNum * sizeof(struct send_wr_rsp));
    if (ret) {
        hccp_err("ra_rs_send_wr_list memcpy_s failed, ret[%d]. ", ret);
        ret = -ESAFEFUNC;
        goto copy_wr_rsp_fail;
    }

copy_wr_rsp_fail:
    free(wrList);
    wrList = NULL;

alloc_wr_list_fail:
    free(wrRsp);
    wrRsp = NULL;
    return 0;
}

STATIC void GetWrListV2(struct wr_info *wrList, union op_send_wrlist_data_v2 *sendWrlist)
{
    uint32_t i;
    for (i = 0; i < sendWrlist->tx_data.send_num; i++) {
        wrList[i].op = sendWrlist->tx_data.wrlist[i].op;
        wrList[i].send_flags = sendWrlist->tx_data.wrlist[i].send_flags;
        wrList[i].dst_addr = sendWrlist->tx_data.wrlist[i].dst_addr;
        wrList[i].mem_list.addr = sendWrlist->tx_data.wrlist[i].mem_list.addr;
        wrList[i].mem_list.len = sendWrlist->tx_data.wrlist[i].mem_list.len;
        wrList[i].mem_list.lkey = sendWrlist->tx_data.wrlist[i].mem_list.lkey;
    }
    return;
}

STATIC int RaRsSendWrListV2(char *inBuf, char *outBuf, int *outLen, int *opResult, int rcvBufLen)
{
    int ret;
    unsigned int completeNum = 0;
    struct wr_info *wrList = NULL;
    struct send_wr_rsp *wrRsp = NULL;
    struct rs_wrlist_base_info baseInfo = {0};
    union op_send_wrlist_data_v2 *sendWrlist = (union op_send_wrlist_data_v2 *)(inBuf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_send_wrlist_data_v2), sizeof(struct msg_head), rcvBufLen,
        opResult);
    HCCP_CHECK_PARAM_LEN_RET_HOST(sendWrlist->tx_data.send_num, 0, MAX_WR_NUM, opResult);

    wrRsp = calloc(sendWrlist->tx_data.send_num, sizeof(struct send_wr_rsp));
    CHK_PRT_RETURN(wrRsp == NULL, hccp_err("wr_rsp calloc failed."), -ENOMEM);

    wrList = calloc(sendWrlist->tx_data.send_num, sizeof(struct wr_info));
    if (wrList == NULL) {
        hccp_err("wr_list calloc failed.");
        ret = -ENOMEM;
        goto alloc_wr_list_fail;
    }

    baseInfo.phy_id = sendWrlist->tx_data.phy_id;
    baseInfo.rdev_index = sendWrlist->tx_data.rdev_index;
    baseInfo.qpn = sendWrlist->tx_data.qpn;
    baseInfo.key_flag = 0;

    GetWrListV2(wrList, sendWrlist);
    ret = gRaRsOps.send_wr_list(baseInfo, wrList, sendWrlist->tx_data.send_num, wrRsp, &completeNum);
    *opResult = ret;
    if (ret) {
        if (ret == -ENOENT) {
            hccp_warn("not found remote mr_info, need try again");
        } else {
            hccp_err("send wr failed ret[%d].", ret);
        }
    }
    sendWrlist = (union op_send_wrlist_data_v2 *)(outBuf + sizeof(struct msg_head));
    sendWrlist->rx_data.complete_num = completeNum;
    ret = memcpy_s(sendWrlist->rx_data.wr_rsp, sizeof(struct send_wr_rsp) * MAX_WR_NUM, wrRsp,
        completeNum * sizeof(struct send_wr_rsp));
    if (ret) {
        hccp_err("ra_rs_send_wr_list memcpy_s failed, ret[%d]. ", ret);
        ret = -ESAFEFUNC;
        goto copy_wr_rsp_fail;
    }

copy_wr_rsp_fail:
    free(wrList);
    wrList = NULL;

alloc_wr_list_fail:
    free(wrRsp);
    wrRsp = NULL;
    return 0;
}

STATIC void GetWrList(struct wr_info *wrList, union op_send_wrlist_data_ext *sendWrlist)
{
    uint32_t i;
    for (i = 0; i < sendWrlist->tx_data.send_num; i++) {
        wrList[i].op = sendWrlist->tx_data.wrlist[i].op;
        wrList[i].send_flags = sendWrlist->tx_data.wrlist[i].send_flags;
        wrList[i].imm_data = sendWrlist->tx_data.wrlist[i].ext.imm_data;
        wrList[i].dst_addr = sendWrlist->tx_data.wrlist[i].dst_addr;
        wrList[i].mem_list.addr = sendWrlist->tx_data.wrlist[i].mem_list.addr;
        wrList[i].mem_list.len = sendWrlist->tx_data.wrlist[i].mem_list.len;
        wrList[i].mem_list.lkey = sendWrlist->tx_data.wrlist[i].mem_list.lkey;
        wrList[i].aux.data_type = sendWrlist->tx_data.wrlist[i].aux.data_type;
        wrList[i].aux.reduce_type = sendWrlist->tx_data.wrlist[i].aux.reduce_type;
        wrList[i].aux.notify_offset = sendWrlist->tx_data.wrlist[i].aux.notify_offset;
    }
    return;
}

STATIC int RaRsSendWrListExt(char *inBuf, char *outBuf, int *outLen, int *opResult, int rcvBufLen)
{
    int ret;
    unsigned int completeNum = 0;
    struct send_wr_rsp *wrRsp = NULL;
    struct wr_info *wrList = NULL;
    struct rs_wrlist_base_info baseInfo = {0};
    union op_send_wrlist_data_ext *sendWrlist = (union op_send_wrlist_data_ext *)(inBuf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_send_wrlist_data_ext), sizeof(struct msg_head), rcvBufLen,
        opResult);
    HCCP_CHECK_PARAM_LEN_RET_HOST(sendWrlist->tx_data.send_num, 0, MAX_WR_NUM, opResult);

    wrRsp = calloc(sendWrlist->tx_data.send_num, sizeof(struct send_wr_rsp));
    CHK_PRT_RETURN(wrRsp == NULL, hccp_err("wr_rsp calloc failed."), -ENOMEM);

    wrList = calloc(sendWrlist->tx_data.send_num, sizeof(struct wr_info));
    if (wrList == NULL) {
        hccp_err("wr_list calloc failed.");
        ret = -ENOMEM;
        goto alloc_wr_list_fail;
    }

    baseInfo.phy_id = sendWrlist->tx_data.phy_id;
    baseInfo.rdev_index = sendWrlist->tx_data.rdev_index;
    baseInfo.qpn = sendWrlist->tx_data.qpn;
    baseInfo.key_flag = 0;
    GetWrList(wrList, sendWrlist);
    ret = gRaRsOps.send_wr_list(baseInfo, wrList, sendWrlist->tx_data.send_num, wrRsp, &completeNum);
    *opResult = ret;
    if (ret) {
        if (ret == -ENOENT) {
            hccp_warn("not found remote mr_info, need try again");
        } else {
            hccp_err("send wr failed ret[%d].", ret);
        }
    }
    sendWrlist = (union op_send_wrlist_data_ext *)(outBuf + sizeof(struct msg_head));
    sendWrlist->rx_data.complete_num = completeNum;
    ret = memcpy_s(sendWrlist->rx_data.wr_rsp, sizeof(struct send_wr_rsp) * MAX_WR_NUM_V1, wrRsp,
        completeNum * sizeof(struct send_wr_rsp));
    if (ret) {
        hccp_err("ra_rs_send_wr_list_ext memcpy_s failed, ret[%d]. ", ret);
        ret = -ESAFEFUNC;
        goto copy_wr_rsp_fail;
    }

copy_wr_rsp_fail:
    free(wrList);
    wrList = NULL;

alloc_wr_list_fail:
    free(wrRsp);
    wrRsp = NULL;
    return 0;
}

STATIC void GetWrListExtV2(struct wr_info *wrList, union op_send_wrlist_data_ext_v2 *sendWrlist)
{
    uint32_t i;
    for (i = 0; i < sendWrlist->tx_data.send_num; i++) {
        wrList[i].op = sendWrlist->tx_data.wrlist[i].op;
        wrList[i].send_flags = sendWrlist->tx_data.wrlist[i].send_flags;
        wrList[i].imm_data = sendWrlist->tx_data.wrlist[i].ext.imm_data;
        wrList[i].dst_addr = sendWrlist->tx_data.wrlist[i].dst_addr;
        wrList[i].mem_list.addr = sendWrlist->tx_data.wrlist[i].mem_list.addr;
        wrList[i].mem_list.len = sendWrlist->tx_data.wrlist[i].mem_list.len;
        wrList[i].mem_list.lkey = sendWrlist->tx_data.wrlist[i].mem_list.lkey;
        wrList[i].aux.data_type = sendWrlist->tx_data.wrlist[i].aux.data_type;
        wrList[i].aux.reduce_type = sendWrlist->tx_data.wrlist[i].aux.reduce_type;
        wrList[i].aux.notify_offset = sendWrlist->tx_data.wrlist[i].aux.notify_offset;
    }
    return;
}

STATIC int RaRsSendWrListExtV2(char *inBuf, char *outBuf, int *outLen, int *opResult, int rcvBufLen)
{
    int ret;
    unsigned int completeNum = 0;
    struct wr_info *wrList = NULL;
    struct send_wr_rsp *wrRsp = NULL;
    struct rs_wrlist_base_info baseInfo = {0};
    union op_send_wrlist_data_ext_v2 *sendWrlist = (union op_send_wrlist_data_ext_v2 *)(inBuf +
        sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_send_wrlist_data_ext_v2), sizeof(struct msg_head), rcvBufLen,
        opResult);
    HCCP_CHECK_PARAM_LEN_RET_HOST(sendWrlist->tx_data.send_num, 0, MAX_WR_NUM, opResult);

    wrRsp = calloc(sendWrlist->tx_data.send_num, sizeof(struct send_wr_rsp));
    CHK_PRT_RETURN(wrRsp == NULL, hccp_err("wr_rsp calloc failed."), -ENOMEM);

    wrList = calloc(sendWrlist->tx_data.send_num, sizeof(struct wr_info));
    if (wrList == NULL) {
        hccp_err("wr_list calloc failed.");
        ret = -ENOMEM;
        goto alloc_wr_list_fail;
    }

    baseInfo.phy_id = sendWrlist->tx_data.phy_id;
    baseInfo.rdev_index = sendWrlist->tx_data.rdev_index;
    baseInfo.qpn = sendWrlist->tx_data.qpn;
    baseInfo.key_flag = 0;
    GetWrListExtV2(wrList, sendWrlist);
    ret = gRaRsOps.send_wr_list(baseInfo, wrList, sendWrlist->tx_data.send_num, wrRsp, &completeNum);
    *opResult = ret;
    if (ret) {
        if (ret == -ENOENT) {
            hccp_warn("not found remote mr_info, need try again");
        } else {
            hccp_err("send wr failed ret[%d].", ret);
        }
    }
    sendWrlist = (union op_send_wrlist_data_ext_v2 *)(outBuf + sizeof(struct msg_head));
    sendWrlist->rx_data.complete_num = completeNum;
    ret = memcpy_s(sendWrlist->rx_data.wr_rsp, sizeof(struct send_wr_rsp) * MAX_WR_NUM, wrRsp,
        completeNum * sizeof(struct send_wr_rsp));
    if (ret) {
        hccp_err("ra_rs_send_wr_list_ext_v2 memcpy_s failed, ret[%d]. ", ret);
        ret = -ESAFEFUNC;
        goto copy_wr_rsp_fail;
    }

copy_wr_rsp_fail:
    free(wrList);
    wrList = NULL;

alloc_wr_list_fail:
    free(wrRsp);
    wrRsp = NULL;
    return 0;
}

STATIC int RaRsSendNormalWrlist(char *inBuf, char *outBuf, int *outLen, int *opResult, int rcvBufLen)
{
    union op_send_normal_wrlist_data *sendWrlistOut = (union op_send_normal_wrlist_data *)(outBuf +
        sizeof(struct msg_head));
    union op_send_normal_wrlist_data *sendWrlist = (union op_send_normal_wrlist_data *)(inBuf +
        sizeof(struct msg_head));
    struct rs_wrlist_base_info baseInfo = {0};

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_send_normal_wrlist_data), sizeof(struct msg_head), rcvBufLen,
        opResult);
    HCCP_CHECK_PARAM_LEN_RET_HOST(sendWrlist->tx_data.send_num, 0, MAX_WR_NUM, opResult);

    baseInfo.phy_id = sendWrlist->tx_data.phy_id;
    baseInfo.rdev_index = sendWrlist->tx_data.rdev_index;
    baseInfo.qpn = sendWrlist->tx_data.qpn;
    baseInfo.key_flag = 1;

    *opResult = gRaRsOps.send_wr_list(baseInfo, sendWrlist->tx_data.wrlist, sendWrlist->tx_data.send_num,
        sendWrlistOut->rx_data.wr_rsp, &sendWrlistOut->rx_data.complete_num);
    if (*opResult != 0) {
        hccp_err("send_wr_list failed ret[%d].", *opResult);
    }
    return 0;
}

STATIC int RaRsGetNotifyBa(char *inBuf, char *outBuf, int *outLen, int *opResult, int rcvBufLen)
{
    union op_get_notify_ba_data *getNotifyBaData = (union op_get_notify_ba_data *)(inBuf + sizeof(struct msg_head));
    struct mr_info info = { 0 };

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_get_notify_ba_data), sizeof(struct msg_head), rcvBufLen, opResult);

    *opResult = gRaRsOps.get_notify_mr_info(getNotifyBaData->tx_data.phy_id,
        getNotifyBaData->tx_data.rdev_index, &info);
    if (*opResult != 0) {
        hccp_err("reg_notify_mr failed ret[%d].", *opResult);
        return 0;
    }

    getNotifyBaData = (union op_get_notify_ba_data *)(outBuf + sizeof(struct msg_head));
    getNotifyBaData->rx_data.va = (unsigned long long)info.addr;
    getNotifyBaData->rx_data.size = info.size;
    getNotifyBaData->rx_data.access = info.access;
    getNotifyBaData->rx_data.lkey = info.lkey;

    return 0;
}

STATIC int RaRsNotifyCfgSet(char *inBuf, char *outBuf, int *outLen, int *opResult, int rcvBufLen)
{
    union op_notify_cfg_set_data *setNotifyBaData =
        (union op_notify_cfg_set_data *)(inBuf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_notify_cfg_set_data), sizeof(struct msg_head), rcvBufLen,
        opResult);

    *opResult = gRaRsOps.notify_cfg_set(setNotifyBaData->tx_data.phy_id, setNotifyBaData->tx_data.va,
        setNotifyBaData->tx_data.size);
    if (*opResult != 0) {
        hccp_err("notify_cfg_set failed ret[%d].", *opResult);
    }

    return 0;
}

STATIC int RaRsNotifyCfgGet(char *inBuf, char *outBuf, int *outLen, int *opResult, int rcvBufLen)
{
    unsigned long long va = 0;
    unsigned long long size = 0;
    union op_notify_cfg_get_data *getNotifyBaData =
        (union op_notify_cfg_get_data *)(inBuf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_notify_cfg_get_data), sizeof(struct msg_head), rcvBufLen,
        opResult);

    *opResult = gRaRsOps.notify_cfg_get(getNotifyBaData->tx_data.phy_id, &va, &size);
    if (*opResult != 0) {
        hccp_err("notify_cfg_set failed ret[%d].", *opResult);
        return 0;
    }

    getNotifyBaData = (union op_notify_cfg_get_data *)(outBuf + sizeof(struct msg_head));
    getNotifyBaData->rx_data.va = va;
    getNotifyBaData->rx_data.size = size;
    return 0;
}

STATIC int RaSetPid(char *inBuf, char *outBuf, int *outLen, int *opResult, int rcvBufLen)
{
    union op_set_pid_data *setPidData = (union op_set_pid_data *)(inBuf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_set_pid_data), sizeof(struct msg_head), rcvBufLen, opResult);

    hccp_info("ra get pid is [%d]", setPidData->tx_data.pid);

    *opResult = gRaRsOps.set_host_pid(setPidData->tx_data.phy_id, setPidData->tx_data.pid,
        setPidData->tx_data.pid_sign);

    hccp_info("ra_set_pid finish");
    return 0;
}

STATIC int RaRsCloseHdcSession(char *inBuf, char *outBuf, int *outLen, int *opResult, int rcvBufLen)
{
    *opResult = 0;
    hccp_info("ra_rs_close_hdc_session finish");
    return 0;
}

STATIC int RaRsGetVnicIp(char *inBuf, char *outBuf, int *outLen, int *opResult, int rcvBufLen)
{
    unsigned int vnicIp = 0;
    union op_get_vnic_ip_data *vnicIpDataRet = NULL;
    union op_get_vnic_ip_data *vnicIpData = (union op_get_vnic_ip_data *)(inBuf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_get_vnic_ip_data), sizeof(struct msg_head), rcvBufLen, opResult);

    *opResult = gRaRsOps.get_vnic_ip(vnicIpData->tx_data.phy_id, &vnicIp);
    if (*opResult != 0) {
        hccp_err("rs get vnic ip failed, phyId %d, ret %d", vnicIpData->tx_data.phy_id, *opResult);
        return 0;
    }

    vnicIpDataRet = (union op_get_vnic_ip_data *)(outBuf + sizeof(struct msg_head));
    hccp_info("rs get vnic_ip, phyId %d, vnicIp 0x%x", vnicIpData->tx_data.phy_id, vnicIp);
    vnicIpDataRet->rx_data.vnic_ip = vnicIp;
    return 0;
}

STATIC int RaRsGetVnicIpInfosV1(char *inBuf, char *outBuf, int *outLen, int *opResult, int rcvBufLen)
{
    union op_get_vnic_ip_infos_data_v1 *vnicIpData = (union op_get_vnic_ip_infos_data_v1 *)(inBuf +
        sizeof(struct msg_head));
    union op_get_vnic_ip_infos_data_v1 *vnicIpOut = (union op_get_vnic_ip_infos_data_v1 *)(outBuf +
        sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_get_vnic_ip_infos_data_v1), sizeof(struct msg_head), rcvBufLen,
        opResult);
    HCCP_CHECK_PARAM_LEN_RET_HOST(vnicIpData->tx_data.num, 0, MAX_IP_INFO_NUM_V1, opResult);

    *opResult = gRaRsOps.get_vnic_ip_infos(vnicIpData->tx_data.phy_id, vnicIpData->tx_data.type,
        vnicIpData->tx_data.ids, vnicIpData->tx_data.num, vnicIpOut->rx_data.infos);

    if (*opResult != 0) {
        hccp_err("rs get vnic ip infos failed, ret %d", *opResult);
    }

    return 0;
}

STATIC int RaRsGetVnicIpInfos(char *inBuf, char *outBuf, int *outLen, int *opResult, int rcvBufLen)
{
    union op_get_vnic_ip_infos_data *vnicIpData = (union op_get_vnic_ip_infos_data *)(inBuf +
        sizeof(struct msg_head));
    union op_get_vnic_ip_infos_data *vnicIpOut = (union op_get_vnic_ip_infos_data *)(outBuf +
        sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_get_vnic_ip_infos_data), sizeof(struct msg_head), rcvBufLen,
        opResult);
    HCCP_CHECK_PARAM_LEN_RET_HOST(vnicIpData->tx_data.num, 0, MAX_IP_INFO_NUM, opResult);

    *opResult = gRaRsOps.get_vnic_ip_infos(vnicIpData->tx_data.phy_id, vnicIpData->tx_data.type,
        vnicIpData->tx_data.ids, vnicIpData->tx_data.num, vnicIpOut->rx_data.infos);

    if (*opResult != 0) {
        hccp_err("rs get vnic ip infos failed, ret %d", *opResult);
    }

    return 0;
}

STATIC int RaRsGetInterfaceVersion(char *inBuf, char *outBuf, int *outLen, int *opResult, int rcvBufLen)
{
    unsigned int version = 0;
    union op_get_version_data *versionInfoRet = NULL;
    union op_get_version_data *versionInfo = (union op_get_version_data *)(inBuf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_get_version_data), sizeof(struct msg_head), rcvBufLen, opResult);

    *opResult = gRaRsOps.get_interface_version(versionInfo->tx_data.opcode, &version);
    if (*opResult != 0) {
        hccp_err("get_interface_version failed, opcode %d, ret %d", versionInfo->tx_data.opcode, *opResult);
        return 0;
    }

    versionInfoRet = (union op_get_version_data *)(outBuf + sizeof(struct msg_head));
    versionInfoRet->rx_data.version = version;
    return 0;
}

STATIC int RaRsSocketWhiteListAdd(char *inBuf, char *outBuf, int *outLen, int *opResult, int rcvBufLen)
{
    union op_wlist_data *wlistData = (union op_wlist_data *)(inBuf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_wlist_data), sizeof(struct msg_head), rcvBufLen, opResult);
    HCCP_CHECK_PARAM_NUM(wlistData->tx_data.num, MAX_WLIST_NUM);

    *opResult = gRaRsOps.white_list_add(wlistData->tx_data.rdev_info, wlistData->tx_data.wlist,
        wlistData->tx_data.num);
    if (*opResult != 0) {
        hccp_err("white_list_add failed, ret[%d]", *opResult);
    }
    return 0;
}

STATIC int RaRsSocketWhiteListAddV2(char *inBuf, char *outBuf, int *outLen, int *opResult, int rcvBufLen)
{
    union op_wlist_data_v2 *wlistData = (union op_wlist_data_v2 *)(inBuf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_wlist_data_v2), sizeof(struct msg_head), rcvBufLen, opResult);
    HCCP_CHECK_PARAM_NUM(wlistData->tx_data.num, MAX_WLIST_NUM);

    *opResult = gRaRsOps.white_list_add(wlistData->tx_data.rdev_info, wlistData->tx_data.wlist,
        wlistData->tx_data.num);
    if (*opResult != 0) {
        hccp_err("white_list_add failed, ret[%d]", *opResult);
    }
    return 0;
}

STATIC int RaRsSocketWhiteListDel(char *inBuf, char *outBuf, int *outLen, int *opResult, int rcvBufLen)
{
    union op_wlist_data *wlistData = (union op_wlist_data *)(inBuf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_wlist_data), sizeof(struct msg_head), rcvBufLen, opResult);
    HCCP_CHECK_PARAM_NUM(wlistData->tx_data.num, MAX_WLIST_NUM);

    *opResult = gRaRsOps.white_list_del(wlistData->tx_data.rdev_info, wlistData->tx_data.wlist,
        wlistData->tx_data.num);
    if (*opResult != 0) {
        hccp_err("white_list_del failed, ret[%d]", *opResult);
    }
    return 0;
}

STATIC int RaRsSocketWhiteListDelV2(char *inBuf, char *outBuf, int *outLen, int *opResult, int rcvBufLen)
{
    union op_wlist_data_v2 *wlistData = (union op_wlist_data_v2 *)(inBuf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_wlist_data_v2), sizeof(struct msg_head), rcvBufLen, opResult);
    HCCP_CHECK_PARAM_NUM(wlistData->tx_data.num, MAX_WLIST_NUM);

    *opResult = gRaRsOps.white_list_del(wlistData->tx_data.rdev_info, wlistData->tx_data.wlist,
        wlistData->tx_data.num);
    if (*opResult != 0) {
        hccp_err("white_list_del failed, ret[%d]", *opResult);
    }
    return 0;
}

STATIC int RaRsSocketCreditAdd(char *inBuf, char *outBuf, int *outLen, int *opResult, int rcvBufLen)
{
    union op_accept_credit_data *opData = (union op_accept_credit_data *)(inBuf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_accept_credit_data), sizeof(struct msg_head), rcvBufLen, opResult);
    HCCP_CHECK_PARAM_NUM(opData->tx_data.num, MAX_SOCKET_NUM);

    *opResult = gRaRsOps.accept_credit_add(opData->tx_data.conn, opData->tx_data.num,
        opData->tx_data.credit_limit);
    if (*opResult != 0) {
        hccp_err("accept_credit_add failed, ret[%d]", *opResult);
    }
    return 0;
}

STATIC int RaRsGetIfnum(char *inBuf, char *outBuf, int *outLen, int *opResult, int rcvBufLen)
{
    union op_ifnum_data *ifnumData = (union op_ifnum_data *)(inBuf + sizeof(struct msg_head));
    union op_ifnum_data *ifnumDataReturn = NULL;
    unsigned int num = 0;
    bool isAll = false;

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_ifnum_data), sizeof(struct msg_head), rcvBufLen, opResult);
    /* resv bit 31 for is_all for compatibility issue */
    if ((ifnumData->tx_data.num & RA_RS_GET_ALL_IP_BIT_MASK) != 0) {
        isAll = true;
    }
    *opResult = gRaRsOps.get_ifnum(ifnumData->tx_data.phy_id, isAll, &num);
    if (*opResult != 0) {
        hccp_err("ra_rs_get_ifnum result ret[%d].", *opResult);
        return 0;
    }

    ifnumDataReturn = (union op_ifnum_data *)(outBuf + sizeof(struct msg_head));
    (ifnumDataReturn->rx_data).num = num;

    return 0;
}

STATIC int RaRsGetIfaddrs(char *inBuf, char *outBuf, int *outLen, int *opResult, int rcvBufLen)
{
    int ret;
    union op_ifaddr_data *ifaddrDataReturn = NULL;
    union op_ifaddr_data *ifaddrData = (union op_ifaddr_data *)(inBuf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_ifaddr_data), sizeof(struct msg_head), rcvBufLen, opResult);
    CHK_PRT_RETURN(ifaddrData->tx_data.num > MAX_INTERFACE_NUM || ifaddrData->tx_data.num == 0,
        hccp_err("interface number is invalid, num[%u]", ifaddrData->tx_data.num), -EINVAL);

    *opResult = gRaRsOps.get_ifaddrs(ifaddrData->tx_data.ifaddr_infos, &(ifaddrData->tx_data.num),
        ifaddrData->tx_data.phy_id);
    if (*opResult != 0) {
        hccp_err("ra_rs_get_ifaddrs result ret[%d].", *opResult);
        return 0;
    }

    ifaddrDataReturn = (union op_ifaddr_data *)(outBuf + sizeof(struct msg_head));

    (ifaddrDataReturn->rx_data).num = ifaddrData->tx_data.num;
    ret = memcpy_s((ifaddrDataReturn->rx_data).ifaddr_infos, sizeof(struct ifaddr_info) * MAX_INTERFACE_NUM,
        (ifaddrData->tx_data).ifaddr_infos, sizeof(struct ifaddr_info) * (ifaddrData->tx_data).num);
    CHK_PRT_RETURN(ret, hccp_err("ra_rs_get_sockets memcpy_s failed, ret[%d]. ", ret), -ESAFEFUNC);

    return 0;
}

STATIC int RaRsGetIfaddrsV2(char *inBuf, char *outBuf, int *outLen, int *opResult, int rcvBufLen)
{
    union op_ifaddr_data_v2 *ifaddrData = (union op_ifaddr_data_v2 *)(inBuf + sizeof(struct msg_head));
    union op_ifaddr_data_v2 *ifaddrDataReturn = NULL;
    bool isAll = false;
    int ret;

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_ifaddr_data_v2), sizeof(struct msg_head), rcvBufLen, opResult);

    /* resv bit 31 for is_all for compatibility issue */
    if ((ifaddrData->tx_data.num & RA_RS_GET_ALL_IP_BIT_MASK) != 0) {
        isAll = true;
    }
    ifaddrData->tx_data.num = ifaddrData->tx_data.num & (~RA_RS_GET_ALL_IP_BIT_MASK);
    CHK_PRT_RETURN(ifaddrData->tx_data.num > MAX_INTERFACE_NUM || ifaddrData->tx_data.num == 0,
        hccp_err("interface number of op_ifaddr_data_v2 is invalid, num[%u]", ifaddrData->tx_data.num), -EINVAL);

    *opResult = gRaRsOps.get_ifaddrs_v2(ifaddrData->tx_data.interface_infos, &(ifaddrData->tx_data.num),
        ifaddrData->tx_data.phy_id, isAll);
    if (*opResult != 0) {
        hccp_err("ra_rs_get_ifaddrs_v2 result ret[%d].", *opResult);
        return 0;
    }

    ifaddrDataReturn = (union op_ifaddr_data_v2 *)(outBuf + sizeof(struct msg_head));

    (ifaddrDataReturn->rx_data).num = ifaddrData->tx_data.num;
    ret = memcpy_s((ifaddrDataReturn->rx_data).interface_infos, sizeof(struct interface_info) * MAX_INTERFACE_NUM,
        (ifaddrData->tx_data).interface_infos, sizeof(struct interface_info) * (ifaddrData->tx_data).num);
    CHK_PRT_RETURN(ret, hccp_err("ra_rs_get_ifaddrs_v2 memcpy_s failed, ret[%d].", ret), -ESAFEFUNC);

    return 0;
}

STATIC int RaRsSetQpAttrQos(char *inBuf, char *outBuf, int *outLen, int *opResult, int rcvBufLen)
{
    union op_set_qp_attr_qos_data *attrQosData = (union op_set_qp_attr_qos_data *)(inBuf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_set_qp_attr_qos_data), sizeof(struct msg_head), rcvBufLen,
        opResult);

    *opResult = gRaRsOps.set_qp_attr_qos(attrQosData->tx_data.phy_id, attrQosData->tx_data.rdev_index,
        attrQosData->tx_data.qpn, &(attrQosData->tx_data.qos_attr));
    if (*opResult != 0) {
        hccp_err("set_qp_attr_qos failed ret[%d].", *opResult);
    }

    return 0;
}

STATIC int RaRsSetQpAttrTimeout(char *inBuf, char *outBuf, int *outLen, int *opResult, int rcvBufLen)
{
    union op_set_qp_attr_timeout_data *attrTimeData =
        (union op_set_qp_attr_timeout_data *)(inBuf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_set_qp_attr_timeout_data), sizeof(struct msg_head), rcvBufLen,
        opResult);

    *opResult = gRaRsOps.set_qp_attr_timeout(attrTimeData->tx_data.phy_id, attrTimeData->tx_data.rdev_index,
        attrTimeData->tx_data.qpn, &(attrTimeData->tx_data.timeout));
    if (*opResult != 0) {
        hccp_err("set_qp_attr_timeout failed ret[%d].", *opResult);
    }

    return 0;
}

STATIC int RaRsSetQpAttrRetryCnt(char *inBuf, char *outBuf, int *outLen, int *opResult, int rcvBufLen)
{
    union op_set_qp_attr_retry_cnt_data *attrRetryCntData =
        (union op_set_qp_attr_retry_cnt_data *)(inBuf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_set_qp_attr_retry_cnt_data), sizeof(struct msg_head), rcvBufLen,
        opResult);

    *opResult =
        gRaRsOps.set_qp_attr_retry_cnt(attrRetryCntData->tx_data.phy_id, attrRetryCntData->tx_data.rdev_index,
        attrRetryCntData->tx_data.qpn, &(attrRetryCntData->tx_data.retry_cnt));
    if (*opResult != 0) {
        hccp_err("set_qp_attr_retry_cnt failed ret[%d].", *opResult);
    }

    return 0;
}

STATIC int RaRsGetCqeErrInfo(char *inBuf, char *outBuf, int *outLen, int *opResult, int rcvBufLen)
{
    int ret;
    struct cqe_err_info info = { 0 };
    union op_get_cqe_err_info_data *cqeErrInfoRet = NULL;

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_get_cqe_err_info_data), sizeof(struct msg_head), rcvBufLen,
        opResult);

    *opResult = gRaRsOps.get_cqe_err_info(&info);
    if (*opResult != 0) {
        hccp_err("get_cqe_err_info failed, ret %d", *opResult);
        return 0;
    }

    cqeErrInfoRet = (union op_get_cqe_err_info_data *)(outBuf + sizeof(struct msg_head));
    ret = memcpy_s(&cqeErrInfoRet->rx_data.info, sizeof(struct cqe_err_info), &info, sizeof(struct cqe_err_info));
    CHK_PRT_RETURN(ret, hccp_err("ra_rs_get_cqe_err_info memcpy_s failed, ret[%d]. ", ret), -ESAFEFUNC);
    return 0;
}

STATIC int RaRsGetLiteSupport(char *inBuf, char *outBuf, int *outLen, int *opResult, int rcvBufLen)
{
    union op_lite_support_data *liteSupportData = (union op_lite_support_data *)(inBuf + sizeof(struct msg_head));
    union op_lite_support_data *liteSupportOut = (union op_lite_support_data *)(outBuf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_lite_support_data), sizeof(struct msg_head), rcvBufLen, opResult);

    *opResult = gRaRsOps.get_lite_support(liteSupportData->tx_data.phy_id,
        liteSupportData->tx_data.rdev_index,
        &liteSupportOut->rx_data.support_lite);
    if (*opResult != 0) {
        hccp_err("get_lite_support failed ret[%d].", *opResult);
    }

    return 0;
}

STATIC int RaRsGetLiteRdevCap(char *inBuf, char *outBuf, int *outLen, int *opResult, int rcvBufLen)
{
    union op_lite_rdev_cap_data *liteRdevCapData = (union op_lite_rdev_cap_data *)(inBuf + sizeof(struct msg_head));
    union op_lite_rdev_cap_data *liteRdevCapOut = (union op_lite_rdev_cap_data *)(outBuf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_lite_rdev_cap_data), sizeof(struct msg_head), rcvBufLen, opResult);

    *opResult = gRaRsOps.get_lite_rdev_cap(liteRdevCapData->tx_data.phy_id,
        liteRdevCapData->tx_data.rdev_index,
        (void *)&liteRdevCapOut->rx_data.resp);
    if (*opResult != 0) {
        hccp_err("get_lite_rdev_cap failed ret[%d].", *opResult);
    }

    return 0;
}

STATIC int RaRsGetLiteQpCqAttr(char *inBuf, char *outBuf, int *outLen, int *opResult, int rcvBufLen)
{
    union op_lite_qp_cq_attr_data *liteQpCqAttrData =
        (union op_lite_qp_cq_attr_data *)(inBuf + sizeof(struct msg_head));
    union op_lite_qp_cq_attr_data *liteQpCqAttrOut =
        (union op_lite_qp_cq_attr_data *)(outBuf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(
        sizeof(union op_lite_qp_cq_attr_data), sizeof(struct msg_head), rcvBufLen, opResult);

    *opResult = gRaRsOps.get_lite_qp_cq_attr(liteQpCqAttrData->tx_data.phy_id,
        liteQpCqAttrData->tx_data.rdev_index,
        liteQpCqAttrData->tx_data.qpn,
        (void *)&liteQpCqAttrOut->rx_data.resp);
    if (*opResult != 0) {
        hccp_err("get_lite_qp_cq_attr failed ret[%d].", *opResult);
    }

    return 0;
}

STATIC int RaRsGetLiteConnectedInfo(
    char *inBuf, char *outBuf, int *outLen, int *opResult, int rcvBufLen)
{
    union op_lite_connected_info_data *liteConnectedInfoData =
        (union op_lite_connected_info_data *)(inBuf + sizeof(struct msg_head));
    union op_lite_connected_info_data *liteConnectedInfoOut =
        (union op_lite_connected_info_data *)(outBuf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(
        sizeof(union op_lite_connected_info_data), sizeof(struct msg_head), rcvBufLen, opResult);

    *opResult = gRaRsOps.get_lite_connected_info(liteConnectedInfoData->tx_data.phy_id,
        liteConnectedInfoData->tx_data.rdev_index,
        liteConnectedInfoData->tx_data.qpn,
        (void *)&liteConnectedInfoOut->rx_data.resp);
    if (*opResult != 0) {
        hccp_err("get_lite_connected_info failed ret[%d].", *opResult);
    }

    return 0;
}

STATIC int RaRsGetLiteMemAttr(char *inBuf, char *outBuf, int *outLen, int *opResult, int rcvBufLen)
{
    union op_lite_mem_attr_data *liteMemAttrData =
        (union op_lite_mem_attr_data *)(inBuf + sizeof(struct msg_head));
    union op_lite_mem_attr_data *liteMemAttrOut =
        (union op_lite_mem_attr_data *)(outBuf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(
        sizeof(union op_lite_mem_attr_data), sizeof(struct msg_head), rcvBufLen, opResult);

    *opResult = gRaRsOps.get_lite_mem_attr(liteMemAttrData->tx_data.phy_id,
        liteMemAttrData->tx_data.rdev_index,
        liteMemAttrData->tx_data.qpn,
        (void *)&liteMemAttrOut->rx_data.resp);
    if (*opResult != 0) {
        hccp_err("get_lite_mem_attr failed ret[%d].", *opResult);
    }

    return 0;
}

STATIC int RaRsPingInit(char *inBuf, char *outBuf, int *outLen, int *opResult, int rcvBufLen)
{
    union op_ping_init_data *pingDataOut = (union op_ping_init_data *)(outBuf + sizeof(struct msg_head));
    union op_ping_init_data *pingData = (union op_ping_init_data *)(inBuf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_ping_init_data), sizeof(struct msg_head), rcvBufLen, opResult);

    *opResult = gRaRsOps.ping_init(&pingData->tx_data.attr, &pingDataOut->rx_data.info,
        &pingDataOut->rx_data.dev_index);
    if (*opResult != 0) {
        hccp_err("ping_init failed ret[%d].", *opResult);
    }
    // only negative return value will be parsed
    if (*opResult > 0) {
        *opResult = -*opResult;
    }

    return 0;
}

STATIC int RaRsPingTargetAdd(char *inBuf, char *outBuf, int *outLen, int *opResult, int rcvBufLen)
{
    union op_ping_add_data *pingData = (union op_ping_add_data *)(inBuf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_ping_add_data), sizeof(struct msg_head), rcvBufLen, opResult);

    *opResult = gRaRsOps.ping_target_add(&pingData->tx_data.rdev, &pingData->tx_data.target);
    if (*opResult != 0) {
        hccp_err("ping_target_add failed ret[%d].", *opResult);
    }
    // only negative return value will be parsed
    if (*opResult > 0) {
        *opResult = -*opResult;
    }

    return 0;
}

STATIC int RaRsPingTaskStart(char *inBuf, char *outBuf, int *outLen, int *opResult, int rcvBufLen)
{
    union op_ping_start_data *pingData = (union op_ping_start_data *)(inBuf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_ping_start_data), sizeof(struct msg_head), rcvBufLen, opResult);

    *opResult = gRaRsOps.ping_task_start(&pingData->tx_data.rdev, &pingData->tx_data.attr);
    if (*opResult != 0) {
        hccp_err("ping_task_start failed ret[%d].", *opResult);
    }
    // only negative return value will be parsed
    if (*opResult > 0) {
        *opResult = -*opResult;
    }

    return 0;
}

STATIC int RaRsPingGetResults(char *inBuf, char *outBuf, int *outLen, int *opResult, int rcvBufLen)
{
    union op_ping_results_data *pingDataOut = (union op_ping_results_data *)(outBuf + sizeof(struct msg_head));
    union op_ping_results_data *pingData = (union op_ping_results_data *)(inBuf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_ping_results_data), sizeof(struct msg_head), rcvBufLen, opResult);
    HCCP_CHECK_PARAM_LEN_RET_HOST(pingData->tx_data.num, 0, RA_MAX_PING_TARGET_NUM, opResult);

    *opResult = gRaRsOps.ping_get_results(&pingData->tx_data.rdev, pingData->tx_data.target,
        &pingData->tx_data.num, pingDataOut->rx_data.target);
    // caller needs to retry, degrade log level
    if (*opResult == -EAGAIN) {
        hccp_warn("ping_get_results unsuccessful, ret[%d].", *opResult);
    } else if (*opResult != 0) {
        hccp_err("ping_get_results failed, ret[%d].", *opResult);
    }
    // only negative return value will be parsed
    if (*opResult > 0) {
        *opResult = -*opResult;
        return 0;
    }
    pingDataOut->rx_data.num = pingData->tx_data.num;

    return 0;
}

STATIC int RaRsPingTaskStop(char *inBuf, char *outBuf, int *outLen, int *opResult, int rcvBufLen)
{
    union op_ping_stop_data *pingData = (union op_ping_stop_data *)(inBuf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_ping_stop_data), sizeof(struct msg_head), rcvBufLen, opResult);

    *opResult = gRaRsOps.ping_task_stop(&pingData->tx_data.rdev);
    if (*opResult != 0) {
        hccp_err("ping_task_stop failed ret[%d].", *opResult);
    }
    // only negative return value will be parsed
    if (*opResult > 0) {
        *opResult = -*opResult;
    }

    return 0;
}

STATIC int RaRsPingTargetDel(char *inBuf, char *outBuf, int *outLen, int *opResult, int rcvBufLen)
{
    union op_ping_del_data *pingData = (union op_ping_del_data *)(inBuf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_ping_del_data), sizeof(struct msg_head), rcvBufLen, opResult);
    HCCP_CHECK_PARAM_LEN_RET_HOST(pingData->tx_data.num, 0, RA_MAX_PING_TARGET_NUM, opResult);

    *opResult = gRaRsOps.ping_target_del(&pingData->tx_data.rdev, pingData->tx_data.target,
        &pingData->tx_data.num);
    if (*opResult != 0) {
        hccp_err("ping_target_del failed ret[%d].", *opResult);
    }
    // only negative return value will be parsed
    if (*opResult > 0) {
        *opResult = -*opResult;
    }

    return 0;
}

STATIC int RaRsPingDeinit(char *inBuf, char *outBuf, int *outLen, int *opResult, int rcvBufLen)
{
    union op_ping_deinit_data *pingData = (union op_ping_deinit_data *)(inBuf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_ping_deinit_data), sizeof(struct msg_head), rcvBufLen, opResult);

    *opResult = gRaRsOps.ping_deinit(&pingData->tx_data.rdev);
    if (*opResult != 0) {
        hccp_err("ping_deinit failed ret[%d].", *opResult);
    }
    // only negative return value will be parsed
    if (*opResult > 0) {
        *opResult = -*opResult;
    }

    return 0;
}

STATIC int RaRsGetCqeErrInfoNum(char *inBuf, char *outBuf, int *outLen, int *opResult, int rcvBufLen)
{
    union op_get_cqe_err_info_num_data *cqeErrInfoNum =
        (union op_get_cqe_err_info_num_data *)(inBuf + sizeof(struct msg_head));
    unsigned int num;

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_get_cqe_err_info_num_data), sizeof(struct msg_head), rcvBufLen,
        opResult);

    *opResult = gRaRsOps.get_cqe_err_info_num(cqeErrInfoNum->tx_data.phy_id,
        cqeErrInfoNum->tx_data.rdev_index, &num);
    if (*opResult != 0) {
        hccp_err("get_cqe_err_info_num failed, ret %d", *opResult);
        return 0;
    }

    cqeErrInfoNum = (union op_get_cqe_err_info_num_data *)(outBuf + sizeof(struct msg_head));
    cqeErrInfoNum->rx_data.num = num;

    return 0;
}

STATIC int RaRsGetCqeErrInfoList(char *inBuf, char *outBuf, int *outLen, int *opResult, int rcvBufLen)
{
    union op_get_cqe_err_info_list_data *cqeErrInfoList =
        (union op_get_cqe_err_info_list_data *)(inBuf + sizeof(struct msg_head));
    union op_get_cqe_err_info_list_data *cqeErrInfoListRet =
        (union op_get_cqe_err_info_list_data *)(outBuf + sizeof(struct msg_head));
    unsigned int num;

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_get_cqe_err_info_list_data), sizeof(struct msg_head), rcvBufLen,
        opResult);
    HCCP_CHECK_PARAM_LEN_RET_HOST(cqeErrInfoList->tx_data.num, 0, CQE_ERR_INFO_MAX_NUM, opResult);

    num = cqeErrInfoList->tx_data.num;
    *opResult = gRaRsOps.get_cqe_err_info_list(cqeErrInfoList->tx_data.phy_id,
        cqeErrInfoList->tx_data.rdev_index, cqeErrInfoListRet->rx_data.info_list, &num);
    if (*opResult != 0) {
        hccp_err("get_cqe_err_info_list failed, ret %d", *opResult);
        return 0;
    }

    cqeErrInfoListRet->rx_data.num = num;

    return 0;
}

STATIC int RaRsGetTlsEnable(char *inBuf, char *outBuf, int *outLen, int *opResult, int rcvBufLen)
{
    union op_get_tls_enable_data *opDataRet = (union op_get_tls_enable_data *)(outBuf + sizeof(struct msg_head));
    union op_get_tls_enable_data *opData = (union op_get_tls_enable_data *)(inBuf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_get_tls_enable_data), sizeof(struct msg_head), rcvBufLen,
        opResult);

    *opResult = gRaRsOps.get_tls_enable(opData->tx_data.phy_id, &opDataRet->rx_data.tls_enable);
    if (*opResult != 0) {
        hccp_err("get_tls_enable failed, ret %d", *opResult);
    }
    return 0;
}

STATIC int RaRsGetSecRandom(char *inBuf, char *outBuf, int *outLen, int *opResult, int rcvBufLen)
{
    union op_get_sec_random_data *opDataRet = (union op_get_sec_random_data *)(outBuf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_get_sec_random_data), sizeof(struct msg_head), rcvBufLen,
        opResult);

    *opResult = gRaRsOps.get_sec_random((int *)&opDataRet->rx_data.value);
    if (*opResult != 0) {
        hccp_err("get sec random failed, ret %d", *opResult);
    }
    return 0;
}

STATIC int RaRsGetHccnCfg(char *inBuf, char *outBuf, int *outLen, int *opResult, int rcvBufLen)
{
    union op_get_hccn_cfg_data *opDataRet = (union op_get_hccn_cfg_data *)(outBuf + sizeof(struct msg_head));
    union op_get_hccn_cfg_data *opData = (union op_get_hccn_cfg_data *)(inBuf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_get_hccn_cfg_data), sizeof(struct msg_head), rcvBufLen,
        opResult);

    opDataRet->rx_data.value_len = HCCN_CFG_MSG_DATA_LEN;
    *opResult = gRaRsOps.get_hccn_cfg(opData->tx_data.phy_id, opData->tx_data.key, opDataRet->rx_data.value,
        &opDataRet->rx_data.value_len);
    if (*opResult != 0) {
        hccp_err("get hccn cfg failed, ret %d", *opResult);
    }
    return 0;
}

#define US_PRE_SECOND 1000000
#define US_PRE_MSECOND 1000
#define MS_PRE_SECOND 1000

STATIC void RaTimeInterval(struct timeval *endTime, struct timeval *startTime, long *msec)
{
    /* if low position is sufficient, then borrow one from the high position */
    if (endTime->tv_usec < startTime->tv_usec) {
        endTime->tv_sec -= 1;
        endTime->tv_usec += US_PRE_SECOND;
    }

    *msec = (endTime->tv_sec - startTime->tv_sec) * MS_PRE_SECOND +
        (endTime->tv_usec - startTime->tv_usec) / US_PRE_MSECOND;
}

#define OP_TYPE_CFG 0
#define OP_TYPE_QUERY 1

STATIC void RaGetOpRight(struct ra_hdc_op_sec *opSec, unsigned int opcode, unsigned int asyncReqId, int *right)
{
    long timeInterval = 0;
    struct timeval tCur;
    int exeRight;
    int ret;

    ret = gettimeofday(&tCur, NULL);
    if (ret) {
        *right = OP_RIGHT_QUERY_ERR;
        hccp_err("ra gettimeofday failed ret[%d].", ret);
        return;
    }

    RaTimeInterval(&tCur, &opSec->t_last, &timeInterval);
    opSec->t_last = (timeInterval < 0) ? tCur : opSec->t_last;
    timeInterval = (timeInterval < 0) ? 0 : timeInterval;
    opSec->t_last = (timeInterval == 0) ? opSec->t_last : tCur;
    opSec->token_num += timeInterval * TOKEN_RATE;

    if (opSec->token_num == 0) {
        *right = HAVE_NOT_OP_RIGHT;
        hccp_err("ra handle have not op right. opcode[%u].", opcode);
        return;
    }

    exeRight = HAVE_OP_RIGHT;

    opSec->token_num = opSec->token_num - 1;
    opSec->token_num = (opSec->token_num > BUCKET_DEPTH) ? BUCKET_DEPTH : opSec->token_num;
    if (opSec->is_async_op) {
        hccp_dbg("opcode[%u], req_id[%u], exeRight[%d], tokenNum[%llu], cfg_op_num[%u]",
            opcode, asyncReqId, exeRight, opSec->token_num, opSec->cfg_op_num);
    } else {
        hccp_dbg("opcode[%u], exeRight[%d], tokenNum[%llu], cfg_op_num[%u]",
            opcode, exeRight, opSec->token_num, opSec->cfg_op_num);
    }

    *right = exeRight;
    return;
}

struct ra_op_handle gRaOpHandle[] = {
    {RA_RS_SOCKET_CONN, RaRsSocketBatchConnect, sizeof(union op_socket_connect_data)},
    {RA_RS_SOCKET_CLOSE, RaRsSocketBatchClose, sizeof(union op_socket_close_data)},
    {RA_RS_SOCKET_ABORT, RaRsSocketBatchAbort, sizeof(union op_socket_connect_data)},
    {RA_RS_SOCKET_LISTEN_START, RaRsSocketListenStart, sizeof(union op_socket_listen_data)},
    {RA_RS_SOCKET_LISTEN_STOP, RaRsSocketListenStop, sizeof(union op_socket_listen_data)},
    {RA_RS_GET_SOCKET, RaRsGetSockets, sizeof(union op_socket_info_data)},
    {RA_RS_SOCKET_SEND, RaRsSocketSend, sizeof(union op_socket_send_data)},
    {RA_RS_SOCKET_RECV, RaRsSocketRecv, sizeof(union op_socket_recv_data)},
    {RA_RS_QP_CREATE, RaRsQpCreate, sizeof(union op_qp_create_data)},
    {RA_RS_QP_CREATE_WITH_ATTRS, RaRsQpCreateWithAttrs, sizeof(union op_qp_create_with_attrs_data)},
    {RA_RS_AI_QP_CREATE, RaRsAiQpCreate, sizeof(union op_ai_qp_create_data)},
    {RA_RS_AI_QP_CREATE_WITH_ATTRS, RaRsAiQpCreateWithData, sizeof(union op_ai_qp_create_with_attrs_data)},
    {RA_RS_TYPICAL_QP_CREATE, RaRsTypicalQpCreate, sizeof(union op_typical_qp_create_data)},
    {RA_RS_QP_DESTROY, RaRsQpDestroy, sizeof(union op_qp_destroy_data)},
    {RA_RS_TYPICAL_QP_MODIFY, RaRsTypicalQpModify, sizeof(union op_typical_qp_modify_data)},
    {RA_RS_QP_BATCH_MODIFY, RaRsQpBatchModify, sizeof(union op_qp_batch_modify_data)},
    {RA_RS_QP_CONNECT, RaRsQpConnectAsync, sizeof(union op_qp_connect_data)},
    {RA_RS_QP_STATUS, RaRsGetQpStatus, sizeof(union op_qp_status_data)},
    {RA_RS_QP_INFO, RaRsGetQpInfo, sizeof(union op_qp_info_data)},
    {RA_RS_MR_REG, RaRsMrReg, sizeof(union op_mr_reg_data)},
    {RA_RS_MR_DEREG, RaRsMrDereg, sizeof(union op_mr_dereg_data)},
    {RA_RS_TYPICAL_MR_REG_V1, RaRsTypicalMrRegV1, sizeof(union op_typical_mr_reg_data)},
    {RA_RS_TYPICAL_MR_REG, RaRsTypicalMrReg, sizeof(union op_typical_mr_reg_data)},
    {RA_RS_REMAP_MR, RaRsRemapMr, sizeof(union op_remap_mr_data)},
    {RA_RS_TYPICAL_MR_DEREG, RaRsTypicalMrDereg, sizeof(union op_typical_mr_dereg_data)},
    {RA_RS_SEND_WR, RaRsSendWr, sizeof(union op_send_wr_data)},
    {RA_RS_GET_NOTIFY_BA, RaRsGetNotifyBa, sizeof(union op_get_notify_ba_data)},
    {RA_RS_SOCKET_INIT, RaRsSocketInit, sizeof(union op_socket_init_data)},
    {RA_RS_SOCKET_DEINIT, RaRsSocketDeinit, sizeof(union op_socket_deinit_data)},
    {RA_RS_RDEV_INIT, RaRsRdevInit, sizeof(union op_rdev_init_data)},
    {RA_RS_RDEV_INIT_WITH_BACKUP, RaRsRdevInitWithBackup, sizeof(union op_rdev_init_with_backup_data)},
    {RA_RS_RDEV_GET_PORT_STATUS, RaRsRdevGetPortStatus, sizeof(union op_rdev_get_port_status_data)},
    {RA_RS_RDEV_DEINIT, RaRsRdevDeinit, sizeof(union op_rdev_deinit_data)},
    {RA_RS_WLIST_ADD, RaRsSocketWhiteListAdd, sizeof(union op_wlist_data)},
    {RA_RS_WLIST_ADD_V2, RaRsSocketWhiteListAddV2, sizeof(union op_wlist_data_v2)},
    {RA_RS_WLIST_DEL, RaRsSocketWhiteListDel, sizeof(union op_wlist_data)},
    {RA_RS_WLIST_DEL_V2, RaRsSocketWhiteListDelV2, sizeof(union op_wlist_data_v2)},
    {RA_RS_ACCEPT_CREDIT_ADD, RaRsSocketCreditAdd, sizeof(union op_accept_credit_data)},
    {RA_RS_GET_IFNUM, RaRsGetIfnum, sizeof(union op_ifnum_data)},
    {RA_RS_GET_IFADDRS, RaRsGetIfaddrs, sizeof(union op_ifaddr_data)},
    {RA_RS_GET_IFADDRS_V2, RaRsGetIfaddrsV2, sizeof(union op_ifaddr_data_v2)},
    {RA_RS_GET_INTERFACE_VERSION, RaRsGetInterfaceVersion, sizeof(union op_get_version_data)},
    {RA_RS_SEND_WRLIST, RaRsSendWrList, sizeof(union op_send_wrlist_data)},
    {RA_RS_SEND_WRLIST_V2, RaRsSendWrListV2, sizeof(union op_send_wrlist_data_v2)},
    {RA_RS_SEND_WRLIST_EXT, RaRsSendWrListExt, sizeof(union op_send_wrlist_data_ext)},
    {RA_RS_SEND_WRLIST_EXT_V2, RaRsSendWrListExtV2, sizeof(union op_send_wrlist_data_ext_v2)},
    {RA_RS_SEND_NORMAL_WRLIST, RaRsSendNormalWrlist, sizeof(union op_send_normal_wrlist_data)},
    {RA_RS_SET_TSQP_DEPTH, RaRsSetTsqpDepth, sizeof(union op_set_tsqp_depth_data)},
    {RA_RS_GET_TSQP_DEPTH, RaRsGetTsqpDepth, sizeof(union op_get_tsqp_depth_data)},
    {RA_RS_HDC_SESSION_CLOSE, RaRsCloseHdcSession, sizeof(union op_socket_recv_data)},
    {RA_RS_GET_VNIC_IP, RaRsGetVnicIp, sizeof(union op_get_vnic_ip_data)},
    {RA_RS_GET_VNIC_IP_INFOS_V1, RaRsGetVnicIpInfosV1, sizeof(union op_get_vnic_ip_infos_data_v1)},
    {RA_RS_GET_VNIC_IP_INFOS, RaRsGetVnicIpInfos, sizeof(union op_get_vnic_ip_infos_data)},
    {RA_RS_NOTIFY_CFG_SET, RaRsNotifyCfgSet, sizeof(union op_notify_cfg_set_data)},
    {RA_RS_NOTIFY_CFG_GET, RaRsNotifyCfgGet, sizeof(union op_notify_cfg_get_data)},
    {RA_RS_SET_PID, RaSetPid, sizeof(union op_set_pid_data)},
    {RA_RS_SET_QP_ATTR_QOS, RaRsSetQpAttrQos, sizeof(union op_set_qp_attr_qos_data)},
    {RA_RS_SET_QP_ATTR_TIMEOUT, RaRsSetQpAttrTimeout, sizeof(union op_set_qp_attr_timeout_data)},
    {RA_RS_SET_QP_ATTR_RETRY_CNT, RaRsSetQpAttrRetryCnt, sizeof(union op_set_qp_attr_retry_cnt_data)},
    {RA_RS_GET_CQE_ERR_INFO, RaRsGetCqeErrInfo, sizeof(union op_get_cqe_err_info_data)},
    {RA_RS_GET_CQE_ERR_INFO_NUM, RaRsGetCqeErrInfoNum, sizeof(union op_get_cqe_err_info_num_data)},
    {RA_RS_GET_CQE_ERR_INFO_LIST, RaRsGetCqeErrInfoList, sizeof(union op_get_cqe_err_info_list_data)},
    {RA_RS_GET_LITE_SUPPORT, RaRsGetLiteSupport, sizeof(union op_lite_support_data)},
    {RA_RS_GET_LITE_RDEV_CAP, RaRsGetLiteRdevCap, sizeof(union op_lite_rdev_cap_data)},
    {RA_RS_GET_LITE_QP_CQ_ATTR, RaRsGetLiteQpCqAttr, sizeof(union op_lite_qp_cq_attr_data)},
    {RA_RS_GET_LITE_CONNECTED_INFO, RaRsGetLiteConnectedInfo, sizeof(union op_lite_connected_info_data)},
    {RA_RS_GET_LITE_MEM_ATTR, RaRsGetLiteMemAttr, sizeof(union op_lite_mem_attr_data)},
    {RA_RS_PING_INIT, RaRsPingInit, sizeof(union op_ping_init_data)},
    {RA_RS_PING_ADD, RaRsPingTargetAdd, sizeof(union op_ping_add_data)},
    {RA_RS_PING_START, RaRsPingTaskStart, sizeof(union op_ping_start_data)},
    {RA_RS_PING_GET_RESULTS, RaRsPingGetResults, sizeof(union op_ping_results_data)},
    {RA_RS_PING_STOP, RaRsPingTaskStop, sizeof(union op_ping_stop_data)},
    {RA_RS_PING_DEL, RaRsPingTargetDel, sizeof(union op_ping_del_data)},
    {RA_RS_PING_DEINIT, RaRsPingDeinit, sizeof(union op_ping_deinit_data)},
#ifdef CONFIG_TLV
    {RA_RS_TLV_INIT, RaRsTlvInit, sizeof(union op_tlv_init_data)},
    {RA_RS_TLV_DEINIT, RaRsTlvDeinit, sizeof(union op_tlv_deinit_data)},
    {RA_RS_TLV_REQUEST, RaRsTlvRequest, sizeof(union op_tlv_request_data)},
#endif
    {RA_RS_GET_TLS_ENABLE, RaRsGetTlsEnable, sizeof(union op_get_tls_enable_data)},
    {RA_RS_GET_SEC_RANDOM, RaRsGetSecRandom, sizeof(union op_get_sec_random_data)},
    {RA_RS_GET_HCCN_CFG, RaRsGetHccnCfg, sizeof(union op_get_hccn_cfg_data)},
    {RA_RS_ASYNC_HDC_SESSION_CONNECT, RaRsAsyncHdcSessionConnect, sizeof(union op_async_hdc_connect_data)},
    {RA_RS_ASYNC_HDC_SESSION_CLOSE, RaRsAsyncHdcSessionClose, sizeof(union op_async_hdc_close_data)},
};

STATIC int RaCheckParam(char *recvBuf, int rcvBufLen, char **sendBuf, int *sndBufLen, int *paramCheckResult)
{
    int i;
    int ret = 0;
    struct msg_head *recvMsgHead = (struct msg_head *)recvBuf;
    int num = sizeof(gRaOpHandle) / sizeof(gRaOpHandle[0]);
    unsigned int dataSize = 0;

    *paramCheckResult = 1;
    if (rcvBufLen < (int)sizeof(struct msg_head)) { // check rcv_buf_len
        hccp_err("rcv_buf_len[%d] form ra is invalid", rcvBufLen);
        ret = OpMsgErr(sendBuf, recvMsgHead, sndBufLen, RECV_BUF_LEN_INVALID);
        return ret;
    }

    if (((recvMsgHead->msg_data_len + sizeof(struct msg_head)) != (unsigned int)rcvBufLen) ||
        (recvMsgHead->opcode >= RA_RS_OP_MAX_NUM ||
        ((recvMsgHead->opcode < RA_RS_HDC_SESSION_CLOSE) && (recvMsgHead->opcode >= RA_RS_EXTER_OP_MAX_NUM)))) {
        hccp_err("rcv data incomplete, because rcvBufLen[%d] != msg_head_len[%u] + msgDataLen[%u] \
            or opcode[%u] is wrong, RA_RS_OP_MAX_NUM:[%d], RA_RS_EXTER_OP_MAX_NUM:[%d]",
            rcvBufLen, sizeof(struct msg_head), recvMsgHead->msg_data_len, recvMsgHead->opcode,
            RA_RS_OP_MAX_NUM, RA_RS_EXTER_OP_MAX_NUM);
        ret = OpMsgErr(sendBuf, recvMsgHead, sndBufLen, HAVE_OP_RIGHT);
        return ret;
    }
    for (i = 0; i < num; i++) {
        if (gRaOpHandle[i].opcode == recvMsgHead->opcode) {
            dataSize = gRaOpHandle[i].data_size;
            break;
        }
    }
    if (recvMsgHead->opcode != RA_RS_SOCKET_RECV && recvMsgHead->msg_data_len != dataSize) {
        hccp_err("rcv data incomplete. because msg_data_len[%d] != op_data_len[%u]",
            recvMsgHead->msg_data_len, dataSize);
        ret = OpMsgErr(sendBuf, recvMsgHead, sndBufLen, RECV_BUF_LEN_INVALID);
        return ret;
    }
    *paramCheckResult = 0;
    return ret;
}

int RaHandle(struct ra_hdc_op_sec *opSec, char *recvBuf, int rcvBufLen, char **sendBuf, int *sndBufLen,
    unsigned int *closeSession)
{
    int i;
    int ret;
    int opRight = 0;
    int paramCheckRet = 0;
    int opRet = 0;
    struct msg_head *recvMsgHead = (struct msg_head *)recvBuf;
    int num = sizeof(gRaOpHandle) / sizeof(gRaOpHandle[0]);

    ret = RaCheckParam(recvBuf, rcvBufLen, sendBuf, sndBufLen, &paramCheckRet);
    CHK_PRT_RETURN(paramCheckRet != 0 || ret != 0, hccp_err("ra param check failed. param check ret:[%d]"
        "function call ret:[%d]", paramCheckRet, ret), ret);

    RaGetOpRight(opSec, recvMsgHead->opcode, recvMsgHead->async_req_id, &opRight);
    CHK_PRT_RETURN(opRight != HAVE_OP_RIGHT, ret = OpMsgErr(sendBuf, recvMsgHead, sndBufLen, opRight), ret);

    *sendBuf = (char *)calloc(sizeof(char), recvMsgHead->msg_data_len + sizeof(struct msg_head));
    CHK_PRT_RETURN(*sendBuf == NULL, hccp_err("calloc failed."), -ENOMEM);

    for (i = 0; i < num; i++) {
        if (gRaOpHandle[i].opcode == recvMsgHead->opcode) {
            ret = gRaOpHandle[i].op_handle(recvBuf, *sendBuf, sndBufLen, &opRet, rcvBufLen);
            if (ret) {
                hccp_err("ra handle failed. ret:[%d]", ret);
                goto out;
            }
            MsgHeadBuildUpHw(*sendBuf, recvMsgHead, opRet, recvMsgHead->msg_data_len);
            *closeSession = recvMsgHead->opcode == RA_RS_HDC_SESSION_CLOSE ? 1 : 0;
            *sndBufLen = recvMsgHead->msg_data_len + sizeof(struct msg_head);
            return ret;
        }
    }

    hccp_warn("not support opcode:%d", recvMsgHead->opcode);
    ret = -EPROTONOSUPPORT;
out:
    free(*sendBuf);
    *sendBuf = NULL;
    return ret;
}

STATIC int RaSendPkt(struct drvHdcMsg *msgRcv, HDC_SESSION session, void *sendBuf, int sndBufLen)
{
    int ret;
    struct drvHdcMsg *msgSnd = NULL;

    ret = RA_HDC_OPS.reuse_msg(msgRcv);
    CHK_PRT_RETURN(ret, hccp_err("reuse msg failed ret %d", ret), ret);

    msgSnd = msgRcv;

    ret = RA_HDC_OPS.add_msg_buffer(msgSnd, sendBuf, sndBufLen);
    CHK_PRT_RETURN(ret, hccp_err("add msg buffer failed ret %d", ret), ret);

    ret = RA_HDC_OPS.send(session, msgSnd, 0, RA_HDC_RECV_SEND_TIMEOUT);
    CHK_PRT_RETURN(ret, hccp_err("send msg failed ret %d", ret), ret);

    return 0;
}

STATIC int RecvHandleSendPkt(HDC_SESSION session, unsigned int *closeSession, unsigned int chipId)
{
    int ret;
    void *recvBuf = NULL;
    void *sendBuf = NULL;
    struct drvHdcMsg *msgRcv = NULL;
    int recvBufCnt, sndBufLen, rcvBufLen;
    RsSetCtx(chipId);
    ret = RA_HDC_OPS.alloc_msg(session, &msgRcv, 1);
    CHK_PRT_RETURN(ret, hccp_err("alloc hdc msg failed ret %d", ret), ret);

    ret = RA_HDC_OPS.recv(session, msgRcv, MAX_HDC_DATA, 0, &recvBufCnt, RA_HDC_RECV_SEND_TIMEOUT);
    if (ret) {
        hccp_warn("recv hdc msg unsuccessful, ret %d", ret);
        goto out;
    }

    RA_HDC_OPS.get_msg_buffer(msgRcv, 0, (char **)&recvBuf, &rcvBufLen);
    if (recvBuf == NULL) {
        hccp_warn("rcv_buf_len is NULL, Session disconnect.");
        goto out;
    }

    if (!rcvBufLen) {
        *closeSession = 1;
        hccp_warn("rcv_buf_len is 0, Session disconnect.");
        RA_HDC_OPS.free_msg(msgRcv);
        return 0;
    }

    ret = RaHandle(&gHdcServer[chipId].op_sec, recvBuf, rcvBufLen, (char **)&sendBuf, &sndBufLen,
        closeSession);
    if (ret) {
        hccp_err("ra_handle failed.");
        goto out;
    }

    ret = RaSendPkt(msgRcv, session, sendBuf, sndBufLen);
    if (ret) {
        hccp_err("ra send pkt failed ret %d", ret);
        goto err;
    }

err:
    free(sendBuf);
    sendBuf = NULL;
out:
    RA_HDC_OPS.free_msg(msgRcv);
    return ret;
}

STATIC void RaHdcRecvHandleSendPkt(const unsigned int chipId)
{
    unsigned int closeSession = 0;
    int ret;

    ret = RecvHandleSendPkt(gHdcServer[chipId].hdc_session, &closeSession, chipId);
    if (closeSession || ret) {
        hccp_warn("recv_handle_send_pkt close_session[%u] ret[%d]", closeSession, ret);
        RA_PTHREAD_MUTEX_LOCK(&gHdcInitPara.mutex);
        gHdcInitPara.connect_status = HDC_UNCONNECTED;
        RA_PTHREAD_MUTEX_UNLOCK(&gHdcInitPara.mutex);
    }

    return;
}

STATIC void RaHwHdcCloseSession(HDC_SESSION *session)
{
    int ret;

    RA_PTHREAD_MUTEX_LOCK(&gHdcInitPara.mutex);
    if (session == NULL || *session == NULL) {
        goto out;
    }

    ret = RA_HDC_OPS.session_close(*session);
    if (ret != 0) {
        hccp_warn("RA_HDC_OPS.session_close unsuccessful, ret:%d", ret);
    }
    *session = NULL;

out:
    RA_PTHREAD_MUTEX_UNLOCK(&gHdcInitPara.mutex);
    return;
}

STATIC void *RaPthread(void *arg)
{
    unsigned int chipId = gHdcInitPara.chip_id;
    int ret;

    ret = pthread_detach(pthread_self());
    CHK_PRT_RETURN(ret, hccp_err("pthread detach failed ret %d", ret), NULL);

    (void)prctl(PR_SET_NAME, (unsigned long)"hccp_ra");

    RA_PTHREAD_MUTEX_LOCK(&gHdcInitPara.mutex);
    gHdcInitPara.thread_status = THREAD_RUNNING;
    RA_PTHREAD_MUTEX_UNLOCK(&gHdcInitPara.mutex);

    RsGetCurTime(&gRaThreadInfo.last_check_time);
    ret = strncpy_s((char *)gRaThreadInfo.pthread_name, sizeof(gRaThreadInfo.pthread_name),
        "ra_thread", strlen("ra_thread"));
    CHK_PRT_RETURN(ret, hccp_err("strncpy_s pthread name failed, ret[%d]", ret), NULL);

    hccp_run_info("pthread[%s] is alive!", gRaThreadInfo.pthread_name);
    while (1) {
        if (gHdcInitPara.thread_status == THREAD_DESTROYING) {
            break;
        }

        if (gHdcInitPara.connect_status != HDC_CONNECTED) {
            usleep(THREAD_SLEEP_TIME);
            continue;
        }
        RsHeartbeatAlivePrint(&gRaThreadInfo);
        RaHdcRecvHandleSendPkt(chipId);
    }

    hccp_info("thread [%d] is out", getpid());
    RaHwHdcCloseSession(&gHdcServer[chipId].hdc_session);
    RA_PTHREAD_MUTEX_LOCK(&gHdcInitPara.mutex);
    gHdcInitPara.thread_status = THREAD_HALT;
    RA_PTHREAD_MUTEX_UNLOCK(&gHdcInitPara.mutex);
    return NULL;
}

STATIC int RaHdcServerInit(unsigned int chipId, int hdcType)
{
    int ret;

    CHK_PRT_RETURN(chipId > HCCP_MAX_CHIP_ID || gHdcServer[chipId].hdc_session != NULL, hccp_err("invalid "
        "chip id %u, or hdc_session is not NULL", chipId), -EINVAL);
    CHK_PRT_RETURN(hdcType != HDC_SERVICE_TYPE_RDMA && hdcType != HDC_SERVICE_TYPE_RDMA_V2, hccp_err("invalid "
        "hdc_type %d", hdcType), -EINVAL);

    RaHdcInitOpSec(&gHdcServer[chipId].op_sec, BUCKET_DEPTH, false);

    ret = RA_HDC_OPS.server_create(chipId, hdcType, &gHdcServer[chipId].hdc_server);
    CHK_PRT_RETURN(ret, hccp_err("Create Server failed, ret(%d) ", ret), -EINVAL);

    return 0;
}

void RaHdcInitOpSec(struct ra_hdc_op_sec *opSec, unsigned long long tokenNum, bool isAsyncOp)
{
    opSec->token_num = tokenNum;
    opSec->cfg_op_num = 0;
    opSec->t_last.tv_sec = 0;
    opSec->t_last.tv_usec = 0;
    opSec->is_async_op = isAsyncOp;
}

int RaHdcSessionAccept(unsigned int chipId, HDC_SESSION *session, int initHostTgid)
{
    int hostTgid;
    int ret = 0;

    ret = RA_HDC_OPS.session_accept(gHdcServer[chipId].hdc_server, session);
    if (ret != 0) {
        hccp_warn("Session accept failed, chipId(%u), ret(%d) ", chipId, ret);
        return ret;
    }

    RA_HDC_OPS.set_session_reference(*session);
    ret = DlHalHdcGetSessionAttr(*session, HDC_SESSION_ATTR_PEER_CREATE_PID, &hostTgid);
    if (ret) {
        hccp_err("Session get host_pid failed, chipId(%u), ret(%d)", chipId, ret);
        goto out;
    }

    if (hostTgid != initHostTgid) {
        hccp_warn("host_tgid[%d] from ra not equal to the tgid[%d] from hccp_init, invalid", hostTgid, initHostTgid);
        goto out;
    }

    return 0;

out:
    RaHdcCloseSession(session);
    return ret;
}

int RaHdcAsyncRecvPkt(struct ra_hdc_async_info *asyncInfo, unsigned int chipId, void **recvBuf,
    unsigned int *recvLen)
{
    struct drvHdcMsg *msgRcv = NULL;
    void *rcvBuf = NULL;
    int rcvLen = 0;
    int ret;

    ret = RA_HDC_OPS.alloc_msg(asyncInfo->hdc_session, &msgRcv, 1);
    CHK_PRT_RETURN(ret != 0, hccp_err("alloc hdc msg failed ret %d", ret), ret);

    ret = RA_HDC_OPS.recv(asyncInfo->hdc_session, msgRcv, MAX_HDC_DATA, 0, &rcvLen, RA_HDC_RECV_SEND_TIMEOUT);
    if (ret != 0) {
        hccp_warn("recv hdc msg unsuccessful ret %d", ret);
        goto out;
    }

    RA_HDC_OPS.get_msg_buffer(msgRcv, 0, (char **)&rcvBuf, &rcvLen);
    if (rcvBuf == NULL || rcvLen == 0) {
        hccp_warn("get_msg_buffer unsuccessful, rcvBuf is NULL or rcvLen:%d is 0", rcvLen);
        goto out;
    }

    *recvBuf = (char *)calloc(rcvLen, sizeof(char));
    if (*recvBuf == NULL) {
        hccp_err("calloc recv_buf failed, errno:%d rcvLen:%d", errno, rcvLen);
        ret = -ENOMEM;
        goto out;
    }

    (void)memcpy_s(*recvBuf, rcvLen, rcvBuf, rcvLen);
    *recvLen = rcvLen;

out:
    RA_HDC_OPS.free_msg(msgRcv);
    return ret;
}

int RaHdcAsyncSendPkt(struct ra_hdc_async_info *asyncInfo, unsigned int chipId, void *sendBuf,
    unsigned int sendLen)
{
    struct drvHdcMsg *msgSnd = NULL;
    int ret = -EINVAL;

    RA_PTHREAD_MUTEX_LOCK(&asyncInfo->send_mutex);
    // degrade log level because session will be closed by recv thread and request will be abort
    if (asyncInfo->hdc_session == NULL) {
        hccp_warn("[async][send_pkt]hdc_session is NULL, chipId(%u)", chipId);
        goto alloc_msg_err;
    }

    ret = RA_HDC_OPS.alloc_msg(asyncInfo->hdc_session, &msgSnd, 1);
    if (ret != 0) {
        hccp_err("[async][send_pkt]HDC alloc msg err ret(%d) chip_id(%u)", ret, chipId);
        goto alloc_msg_err;
    }

    ret = RA_HDC_OPS.add_msg_buffer(msgSnd, sendBuf, sendLen);
    if (ret != 0) {
        hccp_err("[async][send_pkt]HDC add msg buffer err ret(%d) chip_id(%u)", ret, chipId);
        goto msg_err;
    }

    ret = RA_HDC_OPS.send(asyncInfo->hdc_session, msgSnd, RA_HDC_WAIT_TIMEOUT, RA_HDC_RECV_SEND_TIMEOUT);
    if (ret != 0) {
        hccp_err("[async][send_pkt]HDC send err ret(%d) chip_id(%u)", ret, chipId);
        goto msg_err;
    }

msg_err:
    RA_HDC_OPS.free_msg(msgSnd);
alloc_msg_err:
    RA_PTHREAD_MUTEX_UNLOCK(&asyncInfo->send_mutex);
    return ret;
}

void RaHdcCloseSession(HDC_SESSION *session)
{
    RA_HDC_OPS.session_close(*session);
    *session = NULL;
    return;
}

STATIC void RaHwHdcInit(void *arg)
{
    unsigned int chipId = gHdcInitPara.chip_id;
    pthread_t tidp;
    int ret;

    ret = pthread_detach(pthread_self());
    if (ret) {
        hccp_err("pthread detach failed ret %d", ret);
        return;
    }

    (void)prctl(PR_SET_NAME, (unsigned long)"hccp_hw_hdc");

    hccp_info("chip_id(%u)", chipId);
    gHdcInitPara.hdc_flag = 1;

    ret = pthread_create(&tidp, NULL, (void *)RaPthread, NULL);
    if (ret) {
        hccp_err("Create pthread failed, chipId(%u), ret(%d) ", chipId, ret);
        return;
    }

    while (1) {
        if (gHdcInitPara.connect_status != HDC_UNCONNECTED) {
            usleep(HDC_ACCEPT_SLEEP_TIME);
            continue;
        }
        RaHwHdcCloseSession(&gHdcServer[chipId].hdc_session);
        ret = RaHdcSessionAccept(chipId, &gHdcServer[chipId].hdc_session, (int)gHdcInitPara.host_tgid);
        if (ret != 0) {
            hccp_warn("Session Accept unsuccessful, chipId(%u), ret(%d) ", chipId, ret);
            gHdcInitPara.hdc_flag = 0;
            return;
        }
        // original case, should continue to accept: host_tgid != g_hdc_init_para.host_tgid
        if (ret == 0 && gHdcServer[chipId].hdc_session == NULL) {
            continue;
        }
        RA_PTHREAD_MUTEX_LOCK(&gHdcInitPara.mutex);
        gHdcInitPara.connect_status = HDC_CONNECTED;
        RA_PTHREAD_MUTEX_UNLOCK(&gHdcInitPara.mutex);
    }
}

STATIC void RaHwHdcDeinit(void)
{
    unsigned int chipId = gHdcInitPara.chip_id;
    int ret, tryAgain;

    RA_PTHREAD_MUTEX_LOCK(&gHdcInitPara.mutex);
    gHdcInitPara.thread_status = THREAD_DESTROYING;
    RA_PTHREAD_MUTEX_UNLOCK(&gHdcInitPara.mutex);

    tryAgain = HDC_TRY_TIME;
    while ((gHdcInitPara.thread_status != THREAD_HALT) && tryAgain != 0) {
        usleep(HDC_USLEEP_TIME);
        tryAgain--;
    }
    if (tryAgain == 0) {
        hccp_warn("hdc message thread quit timeout, chipId:%u", chipId);
    }

    if (gHdcServer[chipId].hdc_server != NULL) {
        ret = RA_HDC_OPS.server_destroy(gHdcServer[chipId].hdc_server);
        if (ret != 0) {
            hccp_warn("RA_HDC_OPS.server_destroy unsuccessful, ret:%d, chipId:%u", ret, chipId);
        }
        gHdcServer[chipId].hdc_server = NULL;
    } else {
        hccp_warn("hdc_server is NULL, chipId:%u", chipId);
    }
    pthread_mutex_destroy(&gHdcInitPara.mutex);
}

STATIC int HccpSetAffinity(unsigned int chipId)
{
    int ret;
    int64_t cpuId;
    int64_t ccpuNum; /* ctrl cpu */
    int64_t dcpuNum; /* data cpu */
    int64_t acpuNum; /* ai cpu */
    int64_t cpuCoreNum;
    cpu_set_t mask;

    ret = DlHalGetDeviceInfo(chipId, MODULE_TYPE_CCPU, INFO_TYPE_CORE_NUM, &ccpuNum);
    CHK_PRT_RETURN(ret, hccp_err("get ccpu_num failed, ret(%d)", ret), ret);

    ret = DlHalGetDeviceInfo(chipId, MODULE_TYPE_DCPU, INFO_TYPE_CORE_NUM, &dcpuNum);
    CHK_PRT_RETURN(ret, hccp_err("get dcpu_num failed, ret(%d)", ret), ret);

    ret = DlHalGetDeviceInfo(chipId, MODULE_TYPE_AICPU, INFO_TYPE_CORE_NUM, &acpuNum);
    CHK_PRT_RETURN(ret, hccp_err("get acpu_num failed, ret(%d)", ret), ret);
    cpuCoreNum = ccpuNum + dcpuNum + acpuNum;

    CPU_ZERO(&mask);
    cpuId = cpuCoreNum * chipId + HCCP_RUN_CPU_CORE;
    /*lint -e574*/
    CPU_SET((size_t)cpuId, &mask);  //lint !e573
    /*lint +e574*/
    hccp_run_info("chip_id:%u ccpu_num:%lld, dcpuNum:%lld, acpuNum:%lld, cpuId:%lld",
        chipId, ccpuNum, dcpuNum, acpuNum, cpuId);
    ret = sched_setaffinity(getpid(), sizeof(mask), &mask); /* hccp use core0 of each chip to setaffinity */
    CHK_PRT_RETURN(ret == -1, hccp_err("sched_setaffinity failed: ret %d, errno %d ", ret, errno), -ESYSFUNC);

    return 0;
}

STATIC int RaHwInit(unsigned int chipId, pid_t pid)
{
    int ret;
    pthread_t tidp;
    int timeout = RA_THREAD_TRY_TIME;

    gHdcInitPara.chip_id = chipId;
    gHdcInitPara.host_tgid = pid;

    ret = pthread_create(&tidp, NULL, (void *)RaHwHdcInit, NULL);
    CHK_PRT_RETURN(ret, hccp_err("Create pthread failed, ret(%d) ", ret), -ESYSFUNC);

    while (gHdcInitPara.hdc_flag != 1 && timeout > 0) {
        usleep(RA_THREAD_SLEEP_TIME);
        timeout--;
    }

    CHK_PRT_RETURN(gHdcInitPara.hdc_flag == 0 || timeout == 0, hccp_err("HDC server thread create timeout,"
        "flag %d, timeout %d", gHdcInitPara.hdc_flag, timeout), -ESRCH);

    return 0;
}

RA_ADP_ATTRI_VISI_DEF int HccpInit(unsigned int chipId, pid_t pid, int hdcType, unsigned int whiteListStatus)
{
    struct timeval start, end;
    float timeCost = 0.0;
    int ret, retTmp;

    hccp_info("hccp[%u] hdc_type[%d] white_list_status[%u] init start", chipId, hdcType, whiteListStatus);

    ret = DlHalInit();
    if (ret != 0) {
        hccp_err("dl_hal_init failed, ret = %d", ret);
        return ret;
    }

    ret = HccpSetAffinity(chipId);
    if (ret != 0) {
        hccp_err("hccp_set_affinity failed, ret(%d) ", ret);
        goto out;
    }

    RsGetCurTime(&start);
    ret = RaHdcServerInit(chipId, hdcType);
    if (ret != 0) {
        hccp_err("chip_id[%u] hdc_type[%d] ra_hdc_server_init failed, ret[%d] ", chipId, hdcType, ret);
        goto out;
    }

    ret = pthread_mutex_init(&gHdcInitPara.mutex, NULL);
    if (ret != 0) {
        hccp_err("g_hdc_init_para mutex_init failed ret %d!, normal ret 0", ret);
        ret = -ESYSFUNC;
        goto out;
    }

    ret = RaHwInit(chipId, pid);
    if (ret != 0) {
        hccp_err("ra_init failed, ret(%d) ", ret);
        goto hw_init_err;
    }

    ret = RaHwAsyncInit(chipId, pid);
    if (ret != 0) {
        hccp_err("ra_hw_async_init failed, ret(%d) ", ret);
        goto hw_init_err;
    }

    RsGetCurTime(&end);
    HccpTimeInterval(&end, &start, &timeCost);
    hccp_info("ra_hw_init ok cost [%f] ms", timeCost);

    struct rs_init_config offlineConfig = {
        .chip_id = chipId,
        .hccp_mode = NETWORK_OFFLINE,
        .white_list_status = whiteListStatus,
    };

    RsGetCurTime(&start);
    ret = RsInit(&offlineConfig);
    if (ret != 0) {
        hccp_err("rs_init failed (0x%x) ", ret);
        goto init_err;
    }
    RsGetCurTime(&end);
    HccpTimeInterval(&end, &start, &timeCost);
    hccp_info("rs_init ok cost [%f] ms", timeCost);

    RsGetCurTime(&start);
    ret = RsBindHostpid(chipId, pid);
    if (ret != 0) {
        hccp_err("rs_bind_hostpid failed, ret=%d ", ret);
        goto bind_hostpid_err;
    }
    RsGetCurTime(&end);
    HccpTimeInterval(&end, &start, &timeCost);
    hccp_info("rs_bind_hostpid ok cost [%f] ms", timeCost);

    RsGetCurTime(&start);
    ret = RsPingHandleInit(chipId, hdcType, whiteListStatus);
    if (ret != 0) {
        hccp_err("rs_ping_handle_init failed, ret=%d ", ret);
        goto bind_hostpid_err;
    }
    RsGetCurTime(&end);
    HccpTimeInterval(&end, &start, &timeCost);
    hccp_info("rs_ping_handle_init ok cost [%f] ms", timeCost);

    return 0;
bind_hostpid_err:
    retTmp = RsDeinit(&offlineConfig);
    if (retTmp) {
        hccp_err("rs_deinit failed %d ", retTmp);
    }
init_err:
    RaHwAsyncDeinit();
hw_init_err:
    pthread_mutex_destroy(&gHdcInitPara.mutex);
out:
    DlHalDeinit();
    return ret;
}

RA_ADP_ATTRI_VISI_DEF int HccpDeinit(unsigned int chipId)
{
    struct rs_init_config offlineConfig = {
        .chip_id = chipId,
        .hccp_mode = NETWORK_OFFLINE,
        .white_list_status = WHITE_LIST_ENABLE,
    };
    int ret;

    hccp_info("hccp[%u] deinit start", chipId);

    ret = RsPingHandleDeinit(chipId);
    CHK_PRT_RETURN(ret, hccp_err("rs_ping_handle_deinit failed %d ", ret), ret);

    ret = RsDeinit(&offlineConfig);
    CHK_PRT_RETURN(ret, hccp_err("rs_deinit failed %d ", ret), ret);

    RaHwHdcDeinit();

    RaHwAsyncDeinit();
    DlHalDeinit();
    hccp_info("hccp [%u] deinit success", chipId);

    return ret;
}

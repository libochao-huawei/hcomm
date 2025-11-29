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
int gNotifyFd = -1;

static pthread_mutex_t gRaPeerMutex[RA_MAX_PHY_ID_NUM];
int gRaInitCounter[RA_MAX_PHY_ID_NUM] = {0};

void RaPeerMutexLock(unsigned int phyId)
{
    PEER_PTHREAD_MUTEX_LOCK(&gRaPeerMutex[phyId]);
}

void RaPeerMutexUnlock(unsigned int phyId)
{
    PEER_PTHREAD_MUTEX_UNLOCK(&gRaPeerMutex[phyId]);
}

int RaPeerSocketBatchClose(unsigned int devId, struct socket_close_info_t conn[], unsigned int num)
{
    int ret;
    unsigned int i;
    int disuseLinger;
    unsigned int index = 0;
    unsigned int closeNum = 0;
    struct rs_socket_close_info_t closeInfo[MAX_SOCKET_NUM];

    ret = memset_s(closeInfo, sizeof(struct rs_socket_close_info_t) * MAX_SOCKET_NUM, 0,
                   sizeof(struct rs_socket_close_info_t) * MAX_SOCKET_NUM);
    CHK_PRT_RETURN(ret, hccp_err("[batch_close][ra_peer_socket]memset_s close_info failed, ret(%d)",
        ret), -ESAFEFUNC);

    for (i = 0; i < num; i++) {
        if (conn[i].fd_handle != NULL) {
            closeInfo[closeNum].fd = ((struct socket_peer_info *)(conn[i].fd_handle))->fd;
            ++closeNum;
        }
    }

    // use attr disuse_linger of the fist conn as the common attr for all(0 by default)
    disuseLinger = conn[0].disuse_linger;

    PEER_PTHREAD_MUTEX_LOCK(&gRaPeerMutex[devId]);
    RsSetCtx(devId);
    PEER_PTHREAD_MUTEX_UNLOCK(&gRaPeerMutex[devId]);
    ret = RsSocketBatchClose(disuseLinger, &closeInfo[index], closeNum);
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

int RaPeerSocketBatchAbort(unsigned int devId, struct socket_connect_info_t conn[], unsigned int num)
{
    struct socket_connect_info connOut[MAX_SOCKET_NUM];
    int ret = 0;

    ret = RaGetSocketConnectInfo(conn, num, connOut, MAX_SOCKET_NUM);
    CHK_PRT_RETURN(ret, hccp_err("[batch_abort][ra_peer_socket]ra_get_socket_connect_info failed, ret(%d)", ret), ret);

    PEER_PTHREAD_MUTEX_LOCK(&gRaPeerMutex[devId]);
    RsSetCtx(devId);
    PEER_PTHREAD_MUTEX_UNLOCK(&gRaPeerMutex[devId]);
    ret = RsSocketBatchAbort(connOut, num);
    CHK_PRT_RETURN(ret, hccp_err("[batch_abort][ra_peer_socket]abort failed ret(%d), phyId(%u), num(%u)",
        ret, devId, num), ret);

    return ret;
}

int RaPeerSocketBatchConnect(unsigned int devId, struct socket_connect_info_t conn[], unsigned int num)
{
    int ret;
    struct socket_connect_info connOut[MAX_SOCKET_NUM];

    ret = RaGetSocketConnectInfo(conn, num, connOut, MAX_SOCKET_NUM);
    CHK_PRT_RETURN(ret, hccp_err("[batch_connect][ra_peer_socket]ra_hdc_get_socket_connect_info failed,"
        "ret(%d). ", ret), ret);

    /* In peer online mode the server port number is user-defined */
    PEER_PTHREAD_MUTEX_LOCK(&gRaPeerMutex[devId]);
    RsSetCtx(devId);
    ret = RsSocketSetScopeId(devId, ((struct ra_socket_handle *)conn[0].socket_handle)->scope_id);
    if (ret != 0) {
        PEER_PTHREAD_MUTEX_UNLOCK(&gRaPeerMutex[devId]);
    }
    CHK_PRT_RETURN(ret, hccp_err("[set scope id][ra_peer_socket]ra_peer_socket_set_scope_id failed"
        "ret(%d).", ret), ret);

    ret = RsSocketBatchConnect(connOut, num);
    if (ret) {
        hccp_err("[batch_connect][ra_peer_socket]ra client connect failed ret(%d).", ret);
    }
    PEER_PTHREAD_MUTEX_UNLOCK(&gRaPeerMutex[devId]);
    return ret;
}

int RaPeerSocketListenStart(unsigned int devId, struct socket_listen_info_t conn[], unsigned int num)
{
    struct socket_listen_info rsConn[MAX_SOCKET_NUM] = {0};
    unsigned int i;
    int ret;

    for (i = 0; i < num; i++) {
        CHK_PRT_RETURN(conn[i].port > MAX_PORT_NUM, hccp_err("[listen_start][ra_peer_socket]port(%u) of"
            "conn(%u) is invalid", conn[i].port, i), -EINVAL);
    }

    ret = RaGetSocketListenInfo(conn, num, rsConn, MAX_SOCKET_NUM);
    CHK_PRT_RETURN(ret, hccp_err("[listen_start][ra_peer_socket]ra_get_socket_listen_info failed"
        "ret(%d)", ret), ret);

    PEER_PTHREAD_MUTEX_LOCK(&gRaPeerMutex[devId]);
    RsSetCtx(devId);
    ret = RsSocketSetScopeId(devId, ((struct ra_socket_handle *)conn[0].socket_handle)->scope_id);
    if (ret != 0) {
        PEER_PTHREAD_MUTEX_UNLOCK(&gRaPeerMutex[devId]);
    }
    CHK_PRT_RETURN(ret, hccp_err("[set scope id][ra_peer_socket]ra_peer_socket_set_scope_id failed"
        "ret(%d)", ret), ret);

    ret = RsSocketListenStart(rsConn, num);
    // listen node found, degrade log level make it consistent with inner call
    if (ret == -EEXIST) {
        hccp_info("[listen_start][ra_peer_socket]ra listen start unsuccessful ret(%d)", ret);
    } else if (ret == -EADDRINUSE) {
        hccp_run_warn("[listen_start][ra_peer_socket]ra listen start unsuccessful ret(%d)", ret);
    } else if (ret != 0) {
        hccp_err("[listen_start][ra_peer_socket]ra listen start failed ret(%d)", ret);
    }
    if (ret != 0) {
        PEER_PTHREAD_MUTEX_UNLOCK(&gRaPeerMutex[devId]);
        return ret;
    }
    PEER_PTHREAD_MUTEX_UNLOCK(&gRaPeerMutex[devId]);

    ret = RaGetSocketListenResult(rsConn, num, conn, MAX_SOCKET_NUM);
    CHK_PRT_RETURN(ret, hccp_err("[listen_start][ra_peer_socket]ra_get_socket_listen_result failed ret(%d)",
        ret), ret);

    return ret;
}

int RaPeerSocketListenStop(unsigned int devId, struct socket_listen_info_t conn[], unsigned int num)
{
    struct socket_listen_info rsConn[MAX_SOCKET_NUM] = {0};
    unsigned int i;
    int ret;

    for (i = 0; i < num; i++) {
        CHK_PRT_RETURN(conn[i].port > MAX_PORT_NUM, hccp_err("[listen_stop][ra_peer_socket]port(%u) of"
            "conn(%u) is invalid", conn[i].port, i), -EINVAL);
    }

    ret = RaGetSocketListenInfo(conn, num, rsConn, MAX_SOCKET_NUM);
    CHK_PRT_RETURN(ret, hccp_err("[listen_stop][ra_peer_socket]ra_peer_get_socket_listen_info failed ret(%d).",
        ret), ret);

    PEER_PTHREAD_MUTEX_LOCK(&gRaPeerMutex[devId]);
    RsSetCtx(devId);
    ret = RsSocketListenStop(rsConn, num);
    if (ret == -ENODEV) {
        hccp_warn("[listen_stop][ra_peer_socket]ra socket listen stop unsuccessful ret(%d).", ret);
    } else if (ret != 0) {
        hccp_err("[listen_stop][ra_peer_socket]ra socket listen stop failed ret(%d).", ret);
    }
    PEER_PTHREAD_MUTEX_UNLOCK(&gRaPeerMutex[devId]);
    return ret;
}

STATIC int RaPeerSetRsConnParam(struct socket_info_t conn[], unsigned int num,
    struct socket_fd_data rsConn[], unsigned int rsNum)
{
    int ret;
    unsigned int i;
    struct ra_socket_handle *socketHandle = NULL;

    CHK_PRT_RETURN(num > rsNum, hccp_err("[set][ra_peer_rs_conn_param]num(%u) must smaller than rs_num(%u)",
        num, rsNum), -EINVAL);

    for (i = 0; i < num; i++) {
        socketHandle = (struct ra_socket_handle *)conn[i].socket_handle;
        rsConn[i].phy_id = socketHandle->rdev_info.phy_id;
        rsConn[i].family = socketHandle->rdev_info.family;
        rsConn[i].status = conn[i].status;
        ret = memcpy_s(&(rsConn[i].local_ip), sizeof(union hccp_ip_addr), &(socketHandle->rdev_info.local_ip),
            sizeof(union hccp_ip_addr));
        CHK_PRT_RETURN(ret, hccp_err("[set][ra_peer_rs_conn_param]memcpy_s local_ip failed, ret(%d)",
            ret), -ESAFEFUNC);
        ret = memcpy_s(&(rsConn[i].remote_ip), sizeof(union hccp_ip_addr), &(conn[i].remote_ip),
            sizeof(union hccp_ip_addr));
        CHK_PRT_RETURN(ret, hccp_err("[set][ra_peer_rs_conn_param]memcpy_s remote_ip failed, ret(%d)", ret), ret);
        ret = memcpy_s(rsConn[i].tag, sizeof(rsConn[i].tag), conn[i].tag, sizeof(conn[i].tag));
        CHK_PRT_RETURN(ret, hccp_err("[set][ra_peer_rs_conn_param]memcpy_s tag failed, ret(%d)", ret), -ESAFEFUNC);
    }
    return 0;
}

STATIC int RaPeerSetConnParam(struct socket_info_t conn[],
    struct socket_fd_data rsConn[], unsigned int i, unsigned int sslEnable)
{
    int ret;
    struct ra_socket_handle *socketHandle = NULL;
    
    socketHandle = (struct ra_socket_handle *)conn[i].socket_handle;
    socketHandle->rdev_info.phy_id = rsConn[i].phy_id;

    ret = memcpy_s(&(socketHandle->rdev_info.local_ip), sizeof(union hccp_ip_addr),
        &(rsConn[i].local_ip), sizeof(union hccp_ip_addr));
    CHK_PRT_RETURN(ret, hccp_err("[set][ra_peer_conn_param]memcpy_s local_ip failed, ret(%d)", ret), -ESAFEFUNC);
    ret = memcpy_s(&(conn[i].remote_ip), sizeof(union hccp_ip_addr),
        &(rsConn[i].remote_ip), sizeof(union hccp_ip_addr));
    CHK_PRT_RETURN(ret, hccp_err("[set][ra_peer_conn_param]memcpy_s remote_ip failed, ret(%d)", ret), -ESAFEFUNC);

    if (conn[i].fd_handle != NULL) {
        ((struct socket_peer_info *)conn[i].fd_handle)->phy_id = (int)rsConn[i].phy_id;
        ((struct socket_peer_info *)conn[i].fd_handle)->fd = rsConn[i].fd;
        ((struct socket_peer_info *)conn[i].fd_handle)->socket_handle = socketHandle;
        ((struct socket_peer_info *)conn[i].fd_handle)->ssl_enable = sslEnable;
    }
    conn[i].status = rsConn[i].status;
    return 0;
}

int RaPeerGetSockets(unsigned int phyId, unsigned int role, struct socket_info_t conn[], unsigned int num)
{
    struct socket_fd_data rsConn[MAX_SOCKET_NUM] = {0};
    unsigned int sslEnable;
    int connectedNum;
    unsigned int i;
    unsigned int j;
    int ret;

    ret = RaPeerSetRsConnParam(conn, num, rsConn, MAX_SOCKET_NUM);
    CHK_PRT_RETURN(ret, hccp_err("[get][ra_peer_sockets]ra_peer_set_rs_conn_param failed, ret(%d).", ret), ret);

    PEER_PTHREAD_MUTEX_LOCK(&gRaPeerMutex[phyId]);
    RsSetCtx(phyId);
    connectedNum = RsGetSockets(role, rsConn, num);
    if (connectedNum < 0) {
        hccp_err("[get][ra_peer_sockets]ra get socket failed ret(%d).", connectedNum);
        PEER_PTHREAD_MUTEX_UNLOCK(&gRaPeerMutex[phyId]);
        return connectedNum;
    }
    ret = RsGetSslEnable(&sslEnable);
    if (ret < 0) {
        hccp_err("[get][ra_peer_sockets]rs_get_ssl_enable failed ret(%d)", ret);
        PEER_PTHREAD_MUTEX_UNLOCK(&gRaPeerMutex[phyId]);
        return ret;
    }
    PEER_PTHREAD_MUTEX_UNLOCK(&gRaPeerMutex[phyId]);

    for (i = 0; i < num; i++) {
        if (rsConn[i].status == RS_SOCK_STATUS_OK) {
            conn[i].fd_handle = (struct socket_peer_info *)calloc(1, sizeof(struct socket_peer_info));
            if (conn[i].fd_handle == NULL) {
                hccp_err("[get][ra_peer_sockets]socket handle calloc failed.");
                ret = -ENOMEM;
                goto err_out;
            }
        } else {
            conn[i].fd_handle = NULL;
        }

        ret = RaPeerSetConnParam(conn, rsConn, i, sslEnable);
        if (ret) {
            hccp_err("[get][ra_peer_sockets]ra_peer_set_conn_param failed, ret(%d).", ret);
            goto err_out;
        }
        if (memcpy_s(conn[i].tag, sizeof(conn[i].tag), rsConn[i].tag, sizeof(rsConn[i].tag))) {
            hccp_err("[get][ra_peer_sockets]memcpy_s tag failed.");
            ret = -ESAFEFUNC;
            goto err_out;
        }
    }

    return connectedNum;

err_out:
    for (j = 0; j <= i; j++) {
        if (conn[j].fd_handle != NULL) {
            free(conn[j].fd_handle);
            conn[j].fd_handle = NULL;
        }
    }

    return ret;
}

int RaPeerSocketSend(unsigned int devId, const void *handle, const void *data, unsigned long long size)
{
    int fd;
    int ret;
    unsigned int sslEnable;

    fd = ((const struct socket_peer_info *)handle)->fd;
    sslEnable = ((const struct socket_peer_info *)handle)->ssl_enable;
    if (sslEnable != RA_SSL_DISABLE) {
        PEER_PTHREAD_MUTEX_LOCK(&gRaPeerMutex[devId]);
        RsSetCtx(devId);
    }
    ret = RsPeerSocketSend(sslEnable, fd, data, size);
    if (sslEnable != RA_SSL_DISABLE) {
        PEER_PTHREAD_MUTEX_UNLOCK(&gRaPeerMutex[devId]);
    }
    return ret;
}

int RaPeerSocketRecv(unsigned int devId, const void *handle, void *data, unsigned long long size)
{
    int fd;
    int ret;
    unsigned int sslEnable;

    fd = ((const struct socket_peer_info *)handle)->fd;
    sslEnable = ((const struct socket_peer_info *)handle)->ssl_enable;
    if (sslEnable != RA_SSL_DISABLE) {
        PEER_PTHREAD_MUTEX_LOCK(&gRaPeerMutex[devId]);
        RsSetCtx(devId);
    }
    ret = RsPeerSocketRecv(sslEnable, fd, data, size);
    if (sslEnable != RA_SSL_DISABLE) {
        PEER_PTHREAD_MUTEX_UNLOCK(&gRaPeerMutex[devId]);
    }
    return ret;
}

int RaPeerSocketWhiteListAdd(struct rdev rdevInfo,
    struct socket_wlist_info_t whiteList[], unsigned int num)
{
    int ret;
    unsigned int i;
    char netAddr[MAX_IP_LEN] = {0};

    for (i = 0; i < num; i++) {
        CHK_PRT_RETURN(inet_ntop(rdevInfo.family, &whiteList[i].remote_ip, netAddr, sizeof(netAddr)) == NULL,
            hccp_err("[add][ra_peer_socket_white_list]remote ip is invalid! i(%u)", i), -EINVAL);
    }
    PEER_PTHREAD_MUTEX_LOCK(&gRaPeerMutex[rdevInfo.phy_id]);
    RsSetCtx(rdevInfo.phy_id);
    ret = RsSocketWhiteListAdd(rdevInfo, whiteList, num);
    if (ret) {
        hccp_err("[add][ra_peer_socket_white_list]rs_socket_white_list_add failed ret(%d).", ret);
        PEER_PTHREAD_MUTEX_UNLOCK(&gRaPeerMutex[rdevInfo.phy_id]);
        return ret;
    }
    PEER_PTHREAD_MUTEX_UNLOCK(&gRaPeerMutex[rdevInfo.phy_id]);
    return ret;
}

int RaPeerEpollCtlAdd(const void *fdHandle, enum RaEpollEvent event)
{
    int ret;

    ret = RsEpollCtlAdd(fdHandle, event);
    if (ret) {
        hccp_err("[ra_peer_epoll_ctl_add]rs_epoll_ctl_add failed ret(%d).", ret);
    }
    return ret;
}

int RaPeerEpollCtlMod(const void *fdHandle, enum RaEpollEvent event)
{
    int ret;

    ret = RsEpollCtlMod(fdHandle, event);
    if (ret) {
        hccp_err("[ra_peer_epoll_ctl_mod]rs_epoll_ctl_mod failed ret(%d).", ret);
    }
    return ret;
}

int RaPeerEpollCtlDel(const void *fdHandle)
{
    int fd = -1;
    int ret;

    fd = ((const struct socket_peer_info *)fdHandle)->fd;
    ret = RsEpollCtlDel(fd);
    if (ret) {
        hccp_err("[ra_peer_epoll_ctl_del]rs_epoll_ctl_del failed ret(%d).", ret);
    }
    return ret;
}

void RaPeerSetTcpRecvCallback(unsigned int phyId, const void *callback)
{
    PEER_PTHREAD_MUTEX_LOCK(&gRaPeerMutex[phyId]);
    RsSetCtx(phyId);
    RsSetTcpRecvCallback(callback);
    PEER_PTHREAD_MUTEX_UNLOCK(&gRaPeerMutex[phyId]);
}

int RaPeerSocketWhiteListDel(struct rdev rdevInfo,
    struct socket_wlist_info_t whiteList[], unsigned int num)
{
    int ret;
    unsigned int i;
    char netAddr[MAX_IP_LEN] = {0};

    for (i = 0; i < num; i++) {
        CHK_PRT_RETURN(inet_ntop(rdevInfo.family, &whiteList[i].remote_ip, netAddr, sizeof(netAddr)) == NULL,
            hccp_err("[del][ra_peer_socket_white_list]remote ip is invalid! i(%u)", i), -EINVAL);
    }

    PEER_PTHREAD_MUTEX_LOCK(&gRaPeerMutex[rdevInfo.phy_id]);
    RsSetCtx(rdevInfo.phy_id);
    ret = RsSocketWhiteListDel(rdevInfo, whiteList, num);
    if (ret) {
        hccp_err("[del][ra_peer_socket_white_list]ra socket listen stop failed ret(%d).", ret);
    }
    PEER_PTHREAD_MUTEX_UNLOCK(&gRaPeerMutex[rdevInfo.phy_id]);
    return ret;
}

int RaPeerSocketDeinit(struct rdev rdevInfo)
{
    int ret;
    PEER_PTHREAD_MUTEX_LOCK(&gRaPeerMutex[rdevInfo.phy_id]);
    RsSetCtx(rdevInfo.phy_id);
    ret = RsSocketDeinit(rdevInfo);
    if (ret) {
        hccp_err("[deinit][ra_peer_socket]rs_socket_deinit failed, ret(%d)", ret);
        PEER_PTHREAD_MUTEX_UNLOCK(&gRaPeerMutex[rdevInfo.phy_id]);
        return ret;
    }
    PEER_PTHREAD_MUTEX_UNLOCK(&gRaPeerMutex[rdevInfo.phy_id]);
    return 0;
}

int RaPeerQpCreate(struct ra_rdma_handle *rdmaHandle, int flag, int qpMode, void **qpHandle)
{
    unsigned int phyId = rdmaHandle->rdev_info.phy_id;
    struct ra_qp_handle *qpPeer = NULL;
    struct rs_qp_resp qpResp = { 0 };
    struct rs_qp_norm qpNorm = { 0 };
    int ret;

    qpPeer = (struct ra_qp_handle *)calloc(1, sizeof(struct ra_qp_handle));
    CHK_PRT_RETURN(qpPeer == NULL, hccp_err("[create][ra_peer_qp]qp_peer calloc failed."), -ENOMEM);

    qpNorm.flag = flag;
    qpNorm.is_exp = 1;
    qpNorm.is_ext = 0;
    qpNorm.qp_mode = qpMode;

    PEER_PTHREAD_MUTEX_LOCK(&gRaPeerMutex[phyId]);
    RsSetCtx(phyId);
    ret = RsQpCreate(phyId, rdmaHandle->rdev_index, qpNorm, &qpResp);
    if (ret) {
        hccp_err("[create][ra_peer_qp]ra open failed ret[%d]", ret);
        PEER_PTHREAD_MUTEX_UNLOCK(&gRaPeerMutex[phyId]);
        goto calloc_err;
    }
    PEER_PTHREAD_MUTEX_UNLOCK(&gRaPeerMutex[phyId]);
    qpPeer->phy_id = phyId;
    qpPeer->qpn = qpResp.qpn;
    qpPeer->psn = qpResp.psn;
    qpPeer->gid_idx = qpResp.gid_idx;
    qpPeer->flag = flag;
    qpPeer->qp_mode = qpMode;
    qpPeer->rdev_index = rdmaHandle->rdev_index;
    qpPeer->rdma_handle = rdmaHandle;
    qpPeer->rdma_ops = rdmaHandle->rdma_ops;

    *qpHandle = qpPeer;
    return ret;

calloc_err:
    free(qpPeer);
    qpPeer = NULL;
    return ret;
}

int RaPeerQpCreateWithAttrs(struct ra_rdma_handle *rdmaHandle, struct qp_ext_attrs *extAttrs,
    void **qpHandle)
{
    unsigned int phyId = rdmaHandle->rdev_info.phy_id;
    struct rs_qp_norm_with_attrs qpNorm = { 0 };
    struct rs_qp_resp_with_attrs qpResp = { 0 };
    struct ra_qp_handle *qpPeer = NULL;
    int ret;

    qpPeer = (struct ra_qp_handle *)calloc(1, sizeof(struct ra_qp_handle));
    CHK_PRT_RETURN(qpPeer == NULL, hccp_err("[create][ra_peer_qp_with_attrs]qp_peer calloc failed."), -ENOMEM);

    qpNorm.is_exp = 1;
    qpNorm.is_ext = 0;
    ret = memcpy_s(&qpNorm.ext_attrs, sizeof(struct qp_ext_attrs), extAttrs, sizeof(struct qp_ext_attrs));
    if (ret) {
        hccp_err("[create][ra_peer_qp_with_attrs]memcpy_s for ext_attrs failed ret[%d]", ret);
        ret = -ESAFEFUNC;
        goto calloc_err;
    }

    PEER_PTHREAD_MUTEX_LOCK(&gRaPeerMutex[phyId]);
    RsSetCtx(phyId);
    ret = RsQpCreateWithAttrs(phyId, rdmaHandle->rdev_index, &qpNorm, &qpResp);
    if (ret) {
        hccp_err("[create][ra_peer_qp_with_attrs]ra open failed ret[%d]", ret);
        PEER_PTHREAD_MUTEX_UNLOCK(&gRaPeerMutex[phyId]);
        goto calloc_err;
    }
    PEER_PTHREAD_MUTEX_UNLOCK(&gRaPeerMutex[phyId]);
    qpPeer->phy_id = phyId;
    qpPeer->qpn = qpResp.qpn;
    qpPeer->psn = qpResp.psn;
    qpPeer->gid_idx = qpResp.gid_idx;
    qpPeer->flag = extAttrs->qp_attr.qp_type == IBV_QPT_RC ? 0 : 1;
    qpPeer->qp_mode = extAttrs->qp_mode;
    qpPeer->rdev_index = rdmaHandle->rdev_index;
    qpPeer->rdma_handle = rdmaHandle;
    qpPeer->rdma_ops = rdmaHandle->rdma_ops;
    qpPeer->udp_sport = extAttrs->udp_sport;

    *qpHandle = qpPeer;
    return ret;

calloc_err:
    free(qpPeer);
    qpPeer = NULL;
    return ret;
}

int RaPeerMrReg(struct ra_qp_handle *qpPeer, struct mr_info *info)
{
    int ret;
    struct rdma_mr_reg_info mrRegInfo = {0};

    mrRegInfo.addr = info->addr;
    mrRegInfo.len = info->size;
    mrRegInfo.access = info->access;

    PEER_PTHREAD_MUTEX_LOCK(&gRaPeerMutex[qpPeer->phy_id]);
    RsSetCtx(qpPeer->phy_id);
    ret = RsMrReg(qpPeer->phy_id, qpPeer->rdev_index, qpPeer->qpn, &mrRegInfo);
    if (ret) {
        hccp_err("[reg][ra_peer_mr]ra_reg_mr failed ret(%d)", ret);
    }
    PEER_PTHREAD_MUTEX_UNLOCK(&gRaPeerMutex[qpPeer->phy_id]);
    info->lkey = mrRegInfo.lkey;
    info->rkey = mrRegInfo.rkey;
    return ret;
}

int RaPeerMrDereg(struct ra_qp_handle *qpPeer, struct mr_info *info)
{
    int ret;

    PEER_PTHREAD_MUTEX_LOCK(&gRaPeerMutex[qpPeer->phy_id]);
    RsSetCtx(qpPeer->phy_id);
    ret = RsMrDereg(qpPeer->phy_id, qpPeer->rdev_index, qpPeer->qpn, (char *)info->addr);
    if (ret) {
        hccp_err("[dereg][ra_peer_mr]ra_de_reg_mr failed ret(%d)", ret);
    }
    PEER_PTHREAD_MUTEX_UNLOCK(&gRaPeerMutex[qpPeer->phy_id]);
    return ret;
}

int RaPeerRegisterMr(struct ra_rdma_handle *rdmaPeer, struct mr_info *info, void **mrHandle)
{
    int ret;
    struct rdma_mr_reg_info mrRegInfo = {0};

    mrRegInfo.addr = info->addr;
    mrRegInfo.len = info->size;
    mrRegInfo.access = info->access;

    RsSetCtx(rdmaPeer->rdev_info.phy_id);
    ret = RsRegisterMr(rdmaPeer->rdev_info.phy_id, rdmaPeer->rdev_index, &mrRegInfo, mrHandle);
    if (ret) {
        hccp_err("[ra_peer_register_mr]rs_register_mr failed ret(%d)", ret);
    }
    info->lkey = mrRegInfo.lkey;
    info->rkey = mrRegInfo.rkey;
    return ret;
}

int RaPeerDeregisterMr(struct ra_rdma_handle *rdmaPeer, void *mrHandle)
{
    int ret;

    RsSetCtx(rdmaPeer->rdev_info.phy_id);
    ret = RsDeregisterMr(mrHandle);
    if (ret) {
        hccp_err("[ra_peer_deregister_mr]rs_deregister_mr failed ret(%d)", ret);
    }
    return ret;
}

int RaPeerTypicalQpModify(struct ra_qp_handle *qpPeer, struct typical_qp *localQpInfo,
    struct typical_qp *remoteQpInfo)
{
    int ret;

    PEER_PTHREAD_MUTEX_LOCK(&gRaPeerMutex[qpPeer->phy_id]);
    RsSetCtx(qpPeer->phy_id);
    ret = RsTypicalQpModify(qpPeer->phy_id, qpPeer->rdev_index, *localQpInfo, *remoteQpInfo,
        &(qpPeer->udp_sport));
    if (ret != 0) {
        hccp_err("[modify][ra_peer_qp]rs_typical_qp_modify failed ret(%d) phy_id(%u) qpn(%u)",
            ret, qpPeer->phy_id, qpPeer->qpn);
    }
    PEER_PTHREAD_MUTEX_UNLOCK(&gRaPeerMutex[qpPeer->phy_id]);
    return ret;
}

int RaPeerQpConnectAsync(struct ra_qp_handle *qpPeer, const void *sockHandle)
{
    int ret;
    int fd = ((const struct socket_peer_info *)sockHandle)->fd;

    PEER_PTHREAD_MUTEX_LOCK(&gRaPeerMutex[qpPeer->phy_id]);
    RsSetCtx(qpPeer->phy_id);
    ret = RsQpConnectAsync(qpPeer->phy_id, qpPeer->rdev_index, qpPeer->qpn, fd);
    if (ret) {
        hccp_err("[connect_async][ra_peer_qp]ra qp info sync failed socket fd(%d) ret(%d).", fd, ret);
    }
    PEER_PTHREAD_MUTEX_UNLOCK(&gRaPeerMutex[qpPeer->phy_id]);
    return ret;
}

int RaPeerGetQpStatus(struct ra_qp_handle *qpPeer, int *status)
{
    struct rs_qp_status_info qpInfo = { 0 };
    int ret;

    PEER_PTHREAD_MUTEX_LOCK(&gRaPeerMutex[qpPeer->phy_id]);
    RsSetCtx(qpPeer->phy_id);
    ret = RsGetQpStatus(qpPeer->phy_id, qpPeer->rdev_index, qpPeer->qpn, &qpInfo);
    if (ret) {
        hccp_err("[get][ra_peer_qp_status]ra get qp status failed ret(%d).", ret);
    }
    PEER_PTHREAD_MUTEX_UNLOCK(&gRaPeerMutex[qpPeer->phy_id]);
    *status = qpInfo.status;
    return ret;
}

STATIC int RaPeerLoopbackQpModifyPrepare(struct ra_qp_handle *qpHandle, struct typical_qp *qpInfo)
{
    int ret = 0;

    qpInfo->qpn = qpHandle->qpn;
    qpInfo->psn = qpHandle->psn;
    qpInfo->gid_idx = qpHandle->gid_idx;
    qpInfo->retry_cnt = QP_DEFAULT_MAX_ATTR_RETRY_CNT;
    qpInfo->retry_time = QP_DEFAULT_MAX_ATTR_TIMEOUT;
    ret = memcpy_s(qpInfo->gid, sizeof(qpInfo->gid), qpHandle->rdma_handle->gid,
        sizeof(qpHandle->rdma_handle->gid));
    CHK_PRT_RETURN(ret != 0, hccp_err("memcpy_s gid failed, ret:%d, dst_len:%u, src_len:%d", ret,
        sizeof(qpInfo->gid), qpHandle->rdma_handle->gid), -ESAFEFUNC);

    return ret;
}

STATIC int RaPeerLoopbackQpModify(struct ra_qp_handle *qpHandle0, struct ra_qp_handle *qpHandle1)
{
    struct typical_qp qp0Info = {0};
    struct typical_qp qp1Info = {0};
    int ret = 0;

    ret = RaPeerLoopbackQpModifyPrepare(qpHandle0, &qp0Info);
    CHK_PRT_RETURN(ret != 0, hccp_err("ra_peer_loopback_qp_modify_prepare qp0 failed, ret:%d", ret), ret);
    ret = RaPeerLoopbackQpModifyPrepare(qpHandle1, &qp1Info);
    CHK_PRT_RETURN(ret != 0, hccp_err("ra_peer_loopback_qp_modify_prepare qp1 failed, ret:%d", ret), ret);

    ret = RaPeerTypicalQpModify(qpHandle0, &qp0Info, &qp1Info);
    CHK_PRT_RETURN(ret, hccp_err("ra_peer_typical_qp_modify qp0 failed, ret:%d", ret), ret);
    ret = RaPeerTypicalQpModify(qpHandle1, &qp1Info, &qp0Info);
    CHK_PRT_RETURN(ret, hccp_err("ra_peer_typical_qp_modify 1p1 failed, ret:%d", ret), ret);

    return ret;
}

STATIC void RaPeerLoopbackSingleQpDestroy(struct ra_qp_handle *qpHandle)
{
    struct ra_loopback_info *loopbackInfo = qpHandle->loopback_info;
    struct ra_rdma_handle *rdmaHandle = qpHandle->rdma_handle;
    struct cq_attr attr = {0};

    attr.qp_context = &(loopbackInfo->cq_context);
    attr.ib_send_cq = &(loopbackInfo->ib_send_cq);
    attr.ib_recv_cq = &(loopbackInfo->ib_recv_cq);

    (void)RaPeerNormalQpDestroy(qpHandle);

    (void)RaPeerCqDestroy(rdmaHandle, &attr);

    free(loopbackInfo);
    loopbackInfo = NULL;
}

STATIC int RaPeerLoopbackSingleQpCreate(struct ra_rdma_handle *rdmaHandle, struct ra_qp_handle **qpHandle,
    struct ibv_qp **qp)
{
    struct ra_loopback_info *loopbackInfo = NULL;
    struct ibv_qp_init_attr qpInitAttr = {0};
    struct ibv_qp_cap qpCap = {0};
    struct cq_attr cqAttr = {0};
    int ret = 0;

    loopbackInfo = (struct ra_loopback_info *)calloc(1, sizeof(struct ra_loopback_info));
    CHK_PRT_RETURN(loopbackInfo == NULL, hccp_err("loopback_info calloc failed"),
        -ENOMEM);

    cqAttr.qp_context = &(loopbackInfo->cq_context);
    cqAttr.ib_send_cq = &(loopbackInfo->ib_send_cq);
    cqAttr.ib_recv_cq = &(loopbackInfo->ib_recv_cq);
    cqAttr.send_cq_depth = CQ_DEFAULT_MIN_SEND_DEPTH;
    cqAttr.recv_cq_depth = CQ_DEFAULT_MIN_RECV_DEPTH;
    ret = RaPeerCqCreate(rdmaHandle, &cqAttr);
    if (ret != 0) {
        hccp_err("ra_peer_cq_create failed, ret:%d", ret);
        goto cq_create_err;
    }

    qpCap.max_send_wr = QP_DEFAULT_MIN_CAP_SEND_WR;
    qpCap.max_recv_wr = QP_DEFAULT_MIN_CAP_RECV_WR;
    qpCap.max_send_sge = QP_DEFAULT_MIN_CAP_SEND_SGE;
    qpCap.max_recv_sge = QP_DEFAULT_MIN_CAP_RECV_SGE;
    qpCap.max_inline_data = QP_DEFAULT_MAX_CAP_INLINE_DATA;
    qpInitAttr.qp_context = *(cqAttr.qp_context);
    qpInitAttr.send_cq = *(cqAttr.ib_send_cq);
    qpInitAttr.recv_cq = *(cqAttr.ib_recv_cq);
    qpInitAttr.qp_type = IBV_QPT_RC;
    qpInitAttr.cap = qpCap;
    ret = RaPeerNormalQpCreate(rdmaHandle, &qpInitAttr, (void **)qpHandle, (void **)qp);
    if (ret != 0) {
        hccp_err("ra_peer_normal_qp_create failed, ret:%d", ret);
        goto qp_create_err;
    }
    (*qpHandle)->loopback_info = loopbackInfo;
    return ret;

qp_create_err:
    (void)RaPeerCqDestroy(rdmaHandle, &cqAttr);
cq_create_err:
    free(loopbackInfo);
    loopbackInfo = NULL;
    return ret;
}

int RaPeerLoopbackQpCreate(struct ra_rdma_handle *rdmaHandle, struct loopback_qp_pair *qpPair, void **qpHandle)
{
    struct ra_qp_handle *qpHandle0 = NULL;
    struct ra_qp_handle *qpHandle1 = NULL;
    struct ibv_qp *qp0 = NULL;
    struct ibv_qp *qp1 = NULL;
    int ret;

    ret = RaPeerLoopbackSingleQpCreate(rdmaHandle, &qpHandle0, &qp0);
    CHK_PRT_RETURN(ret != 0, hccp_err("ra_peer_loopback_single_qp_create qp0 failed, ret:%d", ret), ret);

    ret = RaPeerLoopbackSingleQpCreate(rdmaHandle, &qpHandle1, &qp1);
    if (ret != 0) {
        hccp_err("ra_peer_loopback_single_qp_create qp1 failed, ret:%d", ret);
        goto qp1_create_err;
    }

    ret = RaPeerLoopbackQpModify(qpHandle0, qpHandle1);
    if (ret != 0) {
        hccp_err("ra_peer_loopback_qp_modify failed, ret:%d", ret);
        goto qp_modify_err;
    }

    qpPair->ibv_qp0 = qp0;
    qpPair->ibv_qp1 = qp1;
    qpHandle0->loopback_qp_handle = qpHandle1;
    qpHandle1->loopback_qp_handle = qpHandle0;
    *qpHandle = qpHandle0;
    return ret;

qp_modify_err:
    RaPeerLoopbackSingleQpDestroy(qpHandle1);
qp1_create_err:
    RaPeerLoopbackSingleQpDestroy(qpHandle0);
    return ret;
}

STATIC void RaPeerLoopbackQpDestroy(struct ra_qp_handle *qpHandle0)
{
    struct ra_qp_handle *qpHandle1 = qpHandle0->loopback_qp_handle;

    RaPeerLoopbackSingleQpDestroy(qpHandle1);
    RaPeerLoopbackSingleQpDestroy(qpHandle0);
}

STATIC int RaPeerSingleQpDestroy(struct ra_qp_handle *qpPeer)
{
    int ret = 0;

    PEER_PTHREAD_MUTEX_LOCK(&gRaPeerMutex[qpPeer->phy_id]);
    RsSetCtx(qpPeer->phy_id);
    ret = RsQpDestroy(qpPeer->phy_id, qpPeer->rdev_index, qpPeer->qpn);
    if (ret != 0) {
        hccp_err("[destroy][ra_peer_qp]destroy failed ret(%d)", ret);
    }
    PEER_PTHREAD_MUTEX_UNLOCK(&gRaPeerMutex[qpPeer->phy_id]);
    free(qpPeer);
    qpPeer = NULL;
    return ret;
}

int RaPeerQpDestroy(struct ra_qp_handle *qpPeer)
{
    if (qpPeer->loopback_qp_handle == NULL) {
        return RaPeerSingleQpDestroy(qpPeer);
    }

    RaPeerLoopbackQpDestroy(qpPeer);

    return 0;
}

int RaPeerSendWr(struct ra_qp_handle *qpPeer, struct send_wr *wr, struct send_wr_rsp *wrRsp)
{
    int ret;

    PEER_PTHREAD_MUTEX_LOCK(&gRaPeerMutex[qpPeer->phy_id]);
    RsSetCtx(qpPeer->phy_id);
    ret = RsSendWr(qpPeer->phy_id, qpPeer->rdev_index, qpPeer->qpn, wr, wrRsp);
    if (ret) {
        hccp_err("[send][ra_peer_wr]ra_send_wr failed ret(%d)", ret);
    }
    PEER_PTHREAD_MUTEX_UNLOCK(&gRaPeerMutex[qpPeer->phy_id]);
    return ret;
}

int RaPeerSendWrlist(struct ra_qp_handle *qpHandle, struct send_wrlist_data wr[], struct send_wr_rsp opRsp[],
    struct wrlist_send_complete_num wrlistNum)
{
    int ret;
    unsigned int completeCnt = 0;
    unsigned int sendCnt = 0;
    struct rs_wrlist_base_info baseInfo;
    struct wrlist_send_complete_num wrlistOnce;
    unsigned int compeletOnceCnt, i;
    struct wr_info *wrList = NULL;

    baseInfo.phy_id = qpHandle->phy_id;
    baseInfo.rdev_index = qpHandle->rdev_index;
    baseInfo.qpn = qpHandle->qpn;
    baseInfo.key_flag = 0;
    wrList = calloc(wrlistNum.send_num, sizeof(struct wr_info));
    CHK_PRT_RETURN(wrList == NULL, hccp_err("wr_list calloc failed."), -ENOMEM);

    for (i = 0; i < wrlistNum.send_num; i++) {
        wrList[i].op = wr[i].op;
        wrList[i].send_flags = wr[i].send_flags;
        wrList[i].dst_addr = wr[i].dst_addr;
        wrList[i].mem_list.addr = wr[i].mem_list.addr;
        wrList[i].mem_list.len = wr[i].mem_list.len;
        wrList[i].mem_list.lkey = wr[i].mem_list.lkey;
    }

    while (sendCnt < wrlistNum.send_num) {
        wrlistOnce.send_num = (wrlistNum.send_num - sendCnt) > MAX_WR_NUM ? MAX_WR_NUM :
            (wrlistNum.send_num - sendCnt);

        PEER_PTHREAD_MUTEX_LOCK(&gRaPeerMutex[baseInfo.phy_id]);
        RsSetCtx(baseInfo.phy_id);
        ret = RsSendWrlist(baseInfo, &wrList[sendCnt], wrlistOnce.send_num, &opRsp[sendCnt],
            &compeletOnceCnt);
        if (ret) {
            hccp_err("[send][ra_peer_wrlist]ra_peer_send_wrlist failed ret[%d], send_num[%u], sendCnt[%u]", ret,
                wrlistNum.send_num, sendCnt);
            PEER_PTHREAD_MUTEX_UNLOCK(&gRaPeerMutex[baseInfo.phy_id]);
            goto alloc_wr_list_fail;
        }
        PEER_PTHREAD_MUTEX_UNLOCK(&gRaPeerMutex[baseInfo.phy_id]);
        sendCnt += wrlistOnce.send_num;
        completeCnt += compeletOnceCnt;
    }

    if (sendCnt != completeCnt) {
        hccp_err("[send][ra_peer_wrlist]complete_cnt[%u] != send_cnt[%u]", completeCnt, sendCnt);
        ret = -EINVAL;
    } else {
        *(wrlistNum.complete_num) = completeCnt;
    }

alloc_wr_list_fail:
    free(wrList);
    wrList = NULL;
    return ret;
}

int RaPeerGetNotifyBaseAddr(struct ra_rdma_handle *handle, unsigned long long *va, unsigned long long *size)
{
    struct mr_info info = { 0 };
    int ret;

    PEER_PTHREAD_MUTEX_LOCK(&gRaPeerMutex[handle->rdev_info.phy_id]);
    RsSetCtx(handle->rdev_info.phy_id);
    ret = RsGetNotifyMrInfo(handle->rdev_info.phy_id, handle->rdev_index, &info);
    if (ret) {
        hccp_err("[get][ra_peer_notify_base_addr]rs_get_notify_mr_info failed ret(%d)", ret);
    }
    *va = (unsigned long long)(uintptr_t)info.addr;
    *size = info.size;
    PEER_PTHREAD_MUTEX_UNLOCK(&gRaPeerMutex[handle->rdev_info.phy_id]);
    return ret;
}

int RaPeerInit(struct ra_init_config *cfg, unsigned int whiteListStatus)
{
    int ret;

    hccp_info("[init][ra_peer]ra_peer_init phy_id[%d] start", cfg->phy_id);

    /* In peer online mode chip id equals to phy id */
    struct rs_init_config rsPeerOnlineCfg = {
        .chip_id = cfg->phy_id,
        .hccp_mode = cfg->nic_position,
        .white_list_status = whiteListStatus,
    };
    ret = DlHalInit();
    if (ret) {
        hccp_err("[init][ra_peer]dl_hal_init failed, ret = %d", ret);
        return ret;
    }

    int counter = __sync_fetch_and_add(&(gRaInitCounter[cfg->phy_id]), 1);
    if (counter > 0) {
        PEER_PTHREAD_MUTEX_LOCK(&gRaPeerMutex[cfg->phy_id]);
        RsSetCtx(cfg->phy_id);
        PEER_PTHREAD_MUTEX_UNLOCK(&gRaPeerMutex[cfg->phy_id]);
        hccp_warn("ra peer has been init for device %u!", cfg->phy_id);
        return 0;
    }

    ret = pthread_mutex_init(&gRaPeerMutex[cfg->phy_id], NULL);
    CHK_PRT_RETURN(ret, hccp_err("[init][ra_peer]pthread_mutex_init failed, ret(%d) phyId(%u)",
        ret, cfg->phy_id), -ESYSFUNC);

    PEER_PTHREAD_MUTEX_LOCK(&gRaPeerMutex[cfg->phy_id]);
    RsSetCtx(cfg->phy_id);
    ret = RsInit(&rsPeerOnlineCfg);
    if (ret) {
        hccp_err("[init][ra_peer]rs init failed(%d)", ret);
        PEER_PTHREAD_MUTEX_UNLOCK(&gRaPeerMutex[cfg->phy_id]);
        pthread_mutex_destroy(&gRaPeerMutex[cfg->phy_id]);
        return ret;
    }
    PEER_PTHREAD_MUTEX_UNLOCK(&gRaPeerMutex[cfg->phy_id]);
    hccp_info("[init][ra_peer]ra_peer_init phy_id[%d] succ", cfg->phy_id);
    return ret;
}

int RaPeerGetTlsEnable(unsigned int phyId, bool *tlsEnable)
{
    int ret;

    RaPeerMutexLock(phyId);
    RsSetCtx(phyId);
    ret = RsGetTlsEnable(phyId, tlsEnable);
    if (ret != 0) {
        hccp_err("[get][tls_enable]rs_get_tls_enable failed, ret(%d) phyId(%u)", ret, phyId);
    }
    RaPeerMutexUnlock(phyId);
    return ret;
}

int RaPeerGetSecRandom(unsigned int *value)
{
    int ret;

    ret = RsGetSecRandom(value);
    if (ret != 0) {
        hccp_run_warn("[get_random] unsuccessful, ret(%d)", ret);
    }
    return ret;
}

int RaPeerDeinit(struct ra_init_config *cfg)
{
    int ret = 0;

    hccp_info("[deinit][ra_peer]ra_peer_deinit phy_id[%d] start", cfg->phy_id);

    /* In peer online mode chip id equals to phy id */
    struct rs_init_config rsPeerOnlineCfg = {
        .chip_id = cfg->phy_id,
        .hccp_mode = cfg->nic_position,
        .white_list_status = WHITE_LIST_ENABLE,
    };

    if (__sync_fetch_and_sub(&(gRaInitCounter[cfg->phy_id]), 1) > 1) {
        goto dl_deinit;
    }

    PEER_PTHREAD_MUTEX_LOCK(&gRaPeerMutex[cfg->phy_id]);
    RsSetCtx(cfg->phy_id);
    ret = RsDeinit(&rsPeerOnlineCfg);
    // no need to destroy lock & return immediately for retry
    if (ret == -EAGAIN) {
        hccp_warn("[deinit][ra_peer]rs deinit unsuccessful(%d)", ret);
        PEER_PTHREAD_MUTEX_UNLOCK(&gRaPeerMutex[cfg->phy_id]);
        return ret;
    }

    if (ret) {
        hccp_err("[deinit][ra_peer]rs deinit failed(%d)", ret);
    }
    PEER_PTHREAD_MUTEX_UNLOCK(&gRaPeerMutex[cfg->phy_id]);
    pthread_mutex_destroy(&gRaPeerMutex[cfg->phy_id]);

dl_deinit:
    DlHalDeinit();
    hccp_info("[deinit][ra_peer]ra_peer_deinit phy_id[%d] succ", cfg->phy_id);
    return ret;
}

int RaPeerGetIfnum(unsigned int phyId, unsigned int *num)
{
    int ret;
    hccp_info("[get][ra_peer_ifnum]ra_peer_get_ifnum phy_id[%u] start", phyId);
    PEER_PTHREAD_MUTEX_LOCK(&gRaPeerMutex[phyId]);
    RsSetCtx(phyId);
    ret = RsPeerGetIfnum(phyId, num);
    if (ret) {
        hccp_err("[get][ra_peer_ifnum]rs_peer_get_ifnum failed(%d)", ret);
    }
    PEER_PTHREAD_MUTEX_UNLOCK(&gRaPeerMutex[phyId]);
    hccp_info("[get][ra_peer_ifnum]ra_peer_get_ifnum phy_id[%u] succ", phyId);
    return ret;
}

int RaPeerGetIfaddrs(unsigned int phyId, struct interface_info interfaceInfos[], unsigned int *num)
{
    int ret;
    hccp_info("[get][ra_peer_ifaddrs] ra_peer_get_ifaddrs phy_id[%u] start", phyId);
    PEER_PTHREAD_MUTEX_LOCK(&gRaPeerMutex[phyId]);
    RsSetCtx(phyId);
    ret = RsPeerGetIfaddrs(interfaceInfos, num, phyId);
    if (ret) {
        hccp_err("[get][ra_peer_ifaddrs]rs_peer_get_ifaddrs failed(%d)", ret);
    }
    PEER_PTHREAD_MUTEX_UNLOCK(&gRaPeerMutex[phyId]);
    hccp_info("[get][ra_peer_ifaddrs] ra_peer_get_ifaddrs phy_id[%u] succ", phyId);
    return ret;
}

#define PAGE_SHIFT              12

int HostNotifyBaseAddrInit(unsigned int phyId)
{
    int ret, retVal;
    unsigned int notifySize = 0;
    unsigned long long *notifyVa = NULL;
    unsigned int logicId = 0;

    ret = DlDrvDeviceGetIndexByPhyId(phyId, &logicId);
    CHK_PRT_RETURN(ret, hccp_err("[init][base_addr]drvDeviceGetIndexByPhyId failed, ret(%d), phyId(%u)",
        ret, phyId), ret);

    ret = DlHalNotifyGetInfo(logicId, 0, RA_NOTIFY_TYPE_TOTAL_SIZE, &notifySize);
    CHK_PRT_RETURN(ret, hccp_err("[init][base_addr]halNotifyGetInfo failed, ret(%d), logicId(%u)",
        ret, logicId), ret);

    gNotifyFd = open(HOST_DEVICE_NAME, O_RDWR);
    CHK_PRT_RETURN(gNotifyFd < 0, hccp_err("[init][base_addr]Failed to open file_path[%s], err_code[%d]",
        HOST_DEVICE_NAME, errno), -ENOENT);

    notifyVa = mmap(NULL, notifySize, PROT_READ | PROT_WRITE, MAP_SHARED,
        gNotifyFd, (unsigned long long)logicId << PAGE_SHIFT);
    if (notifyVa == MAP_FAILED) {
        hccp_err("[init][base_addr]failed to mmap recv buf, fd[%d], err_code[%d]", gNotifyFd, errno);
        ret = -ENOMEM;
        goto close_fd;
    }

    ret = RsNotifyCfgSet(phyId, (uintptr_t)notifyVa, notifySize);
    if (ret) {
        hccp_err("[init][base_addr]ra_hdc_notify_cfg_set failed, ret(%d), phyId(%u)", ret, phyId);
        goto unmmap_mem;
    }
    return 0;

unmmap_mem:
    retVal = munmap((void *)notifyVa, notifySize);
    if (retVal) {
        hccp_err("[init][base_addr]munmap buf munmap error, length:%lu, ret:%d", notifySize, retVal);
    }
close_fd:
    HCCP_CLOSE_RETRY_FOR_EINTR(gNotifyFd);
    return ret;
}

int RaPeerNotifyBaseAddrInit(unsigned int notifyType, unsigned int phyId)
{
    switch (notifyType) {
        case NOTIFY: return HostNotifyBaseAddrInit(phyId);
        case EVENTID: return 0;
        case NO_USE: return 0;
        default: {
            hccp_err("[init][base_addr]notify_type[%u] error", notifyType);
            return -EINVAL;
        }
    }
}

int HostNotifyBaseAddrUninit(unsigned int phyId)
{
    int ret;
    unsigned long long va, size;
    unsigned int logicId = 0;
    struct host_roce_notify_info notifyNode = {0};

    ret = DlDrvDeviceGetIndexByPhyId(phyId, &logicId);
    CHK_PRT_RETURN(ret, hccp_err("[uninit][base_addr]drvDeviceGetIndexByPhyId failed, ret(%d), phyId(%u)",
        ret, phyId), ret);

    ret = RsNotifyCfgGet(phyId, &va, &size);
    CHK_PRT_RETURN(ret, hccp_err("[uninit][base_addr]rs_notify_cfg_get failed, ret(%d), phyId(%u)",
        ret, phyId), ret);
    notifyNode.logic_id = logicId;
    notifyNode.va = va;
    notifyNode.sz = size;

    CHK_PRT_RETURN(gNotifyFd < 0, hccp_err("[uninit][base_addr]file_path[%s] has closed",
        HOST_DEVICE_NAME), -ENOENT);

    ret = ioctl(gNotifyFd, HOST_CDEV_IOC_FREE_NOTIFY, &notifyNode);
    if (ret < 0) {
        hccp_err("[uninit][base_addr]Failed to run ioctl, ret[%d], err_code[%d].", ret, errno);
        HCCP_CLOSE_RETRY_FOR_EINTR(gNotifyFd);
        return ret;
    }

    ret = munmap((void *)(uintptr_t)va, size);
    if (ret) {
        hccp_err("[uninit][base_addr]munmap buf munmap error, *size:%lu, ret:%d", size, ret);
        HCCP_CLOSE_RETRY_FOR_EINTR(gNotifyFd);
        return ret;
    }

    HCCP_CLOSE_RETRY_FOR_EINTR(gNotifyFd);
    return 0;
}

int NotifyBaseAddrUninit(unsigned int notifyType, unsigned int phyId)
{
    switch (notifyType) {
        case NOTIFY: return HostNotifyBaseAddrUninit(phyId);
        case EVENTID: return 0;
        case NO_USE: return 0;
        default: {
            hccp_err("[uninit][base_addr]notify_type[%u] error", notifyType);
            return -EINVAL;
        }
    }
}

int RaPeerRdevInit(
    struct ra_rdma_handle *rdmaHandle, unsigned int notifyType, struct rdev rdevInfo, unsigned int *rdevIndex)
{
    int ret, retVal;

    hccp_run_info("[init][ra_peer_rdev]ra_peer_rdev_init phy_id[%d] notify_type[%u] physical device id[%u]",
        rdevInfo.phy_id, notifyType, rdmaHandle->rdev_info.phy_id);

    RsSetCtx(rdevInfo.phy_id);
    ret = RaPeerNotifyBaseAddrInit(notifyType, rdevInfo.phy_id);
    CHK_PRT_RETURN(ret, hccp_err("[init][ra_peer_rdev] ra_peer_notify_base_addr_init failed[%d]", ret), ret);

    PEER_PTHREAD_MUTEX_LOCK(&gRaPeerMutex[rdevInfo.phy_id]);
    ret = RsRdevInit(rdevInfo, notifyType, rdevIndex);
    if (ret) {
        hccp_err("[init][ra_peer_rdev] rs_rdev_init failed[%d]", ret);
        PEER_PTHREAD_MUTEX_UNLOCK(&gRaPeerMutex[rdevInfo.phy_id]);
        goto notify_base_addr_uninit;
    }
    PEER_PTHREAD_MUTEX_UNLOCK(&gRaPeerMutex[rdevInfo.phy_id]);

    return 0;
notify_base_addr_uninit:
    retVal = NotifyBaseAddrUninit(notifyType, rdevInfo.phy_id);
    CHK_PRT_RETURN(retVal, hccp_err("[init][ra_peer_rdev] notify_base_addr_uninit failed, ret(%d)",
        retVal), retVal);
    return ret;
}

int RaPeerRdevDeinit(struct ra_rdma_handle *rdmaHandle, unsigned int notifyType)
{
    int ret;

    hccp_info("[deinit][ra_peer_rdev]ra_peer_rdev_deinit phy_id[%d]", rdmaHandle->rdev_info.phy_id);
    PEER_PTHREAD_MUTEX_LOCK(&gRaPeerMutex[rdmaHandle->rdev_info.phy_id]);
    RsSetCtx(rdmaHandle->rdev_info.phy_id);
    ret = RsRdevDeinit(rdmaHandle->rdev_info.phy_id, notifyType, rdmaHandle->rdev_index);
    if (ret) {
        hccp_err("[deinit][ra_peer_rdev] rs_rdev_deinit failed[%d]", ret);
        PEER_PTHREAD_MUTEX_UNLOCK(&gRaPeerMutex[rdmaHandle->rdev_info.phy_id]);
        return ret;
    }
    PEER_PTHREAD_MUTEX_UNLOCK(&gRaPeerMutex[rdmaHandle->rdev_info.phy_id]);

    ret = NotifyBaseAddrUninit(notifyType, rdmaHandle->rdev_info.phy_id);
    CHK_PRT_RETURN(ret, hccp_err("[deinit][ra_peer_rdev] notify_base_addr_uninit failed, ret(%d)", ret), ret);

    return 0;
}

int RaPeerSetTsqpDepth(struct ra_rdma_handle *rdmaHandle, unsigned int tempDepth, unsigned int *qpNum)
{
    int ret;
    hccp_info("[set][peer_set_tsqp_depth]ra_peer_set_tsqp_depth phy_id[%d]", rdmaHandle->rdev_info.phy_id);
    PEER_PTHREAD_MUTEX_LOCK(&gRaPeerMutex[rdmaHandle->rdev_info.phy_id]);
    RsSetCtx(rdmaHandle->rdev_info.phy_id);
    ret = RsSetTsqpDepth(rdmaHandle->rdev_info.phy_id, rdmaHandle->rdev_index, tempDepth, qpNum);
    if (ret) {
        hccp_err("[set][peer_set_tsqp_depth] rs_set_tsqp_depth failed[%d]", ret);
        PEER_PTHREAD_MUTEX_UNLOCK(&gRaPeerMutex[rdmaHandle->rdev_info.phy_id]);
        return ret;
    }

    PEER_PTHREAD_MUTEX_UNLOCK(&gRaPeerMutex[rdmaHandle->rdev_info.phy_id]);
    return 0;
}

int RaPeerGetTsqpDepth(struct ra_rdma_handle *rdmaHandle, unsigned int *tempDepth, unsigned int *qpNum)
{
    int ret;

    hccp_info("[get][peer_get_tsqp_depth]ra_peer_get_tsqp_depth phy_id[%d]", rdmaHandle->rdev_info.phy_id);
    PEER_PTHREAD_MUTEX_LOCK(&gRaPeerMutex[rdmaHandle->rdev_info.phy_id]);
    RsSetCtx(rdmaHandle->rdev_info.phy_id);
    ret = RsGetTsqpDepth(rdmaHandle->rdev_info.phy_id, rdmaHandle->rdev_index, tempDepth, qpNum);
    if (ret) {
        hccp_err("[get][peer_set_tsqp_depth]rs_get_tsqp_depth failed[%d]", ret);
        PEER_PTHREAD_MUTEX_UNLOCK(&gRaPeerMutex[rdmaHandle->rdev_info.phy_id]);
        return ret;
    }

    PEER_PTHREAD_MUTEX_UNLOCK(&gRaPeerMutex[rdmaHandle->rdev_info.phy_id]);
    return ret;
}

int RaPeerRecvWrlist(struct ra_qp_handle *qpHandle, struct recv_wrlist_data *wr, unsigned int recvNum,
    unsigned int *completeNum)
{
    int ret;
    struct rs_wrlist_base_info baseInfo = {0};
    unsigned int completeCnt = 0;
    unsigned int recvCnt = 0;
    unsigned int recvNumPer;
    unsigned int compeletOnceCnt;

    baseInfo.phy_id = qpHandle->phy_id;
    baseInfo.rdev_index = qpHandle->rdev_index;
    baseInfo.qpn = qpHandle->qpn;

    while (recvCnt < recvNum) {
        recvNumPer = (recvNum - recvCnt) > MAX_WR_NUM ? MAX_WR_NUM :
            (recvNum - recvCnt);

        PEER_PTHREAD_MUTEX_LOCK(&gRaPeerMutex[baseInfo.phy_id]);
        RsSetCtx(baseInfo.phy_id);
        ret = RsRecvWrlist(baseInfo, &wr[recvCnt], recvNumPer, &compeletOnceCnt);
        if (ret) {
            hccp_err("[recv][peer_recv_wrlist]ra_peer_recv_wrlist failed ret[%d], recvCnt[%u], recvNumPer[%u]",
                ret, recvCnt, recvNumPer);
            PEER_PTHREAD_MUTEX_UNLOCK(&gRaPeerMutex[baseInfo.phy_id]);
            return ret;
        }
        PEER_PTHREAD_MUTEX_UNLOCK(&gRaPeerMutex[baseInfo.phy_id]);
        recvCnt += recvNumPer;
        completeCnt += compeletOnceCnt;
    }

    CHK_PRT_RETURN(recvCnt != completeCnt, hccp_err("[recv][peer_recv_wrlist]complete_cnt[%u] != recv_cnt[%u]",
        completeCnt, recvCnt), -EINVAL);

    *completeNum = completeCnt;
    return 0;
}

int RaPeerGetQpContext(struct ra_qp_handle *qpPeer, void** qp, void** sendCq, void** recvCq)
{
    int ret;
    RsSetCtx(qpPeer->phy_id);
    ret = RsGetQpContext(qpPeer->phy_id, qpPeer->rdev_index, qpPeer->qpn, qp, sendCq, recvCq);
    if (ret) {
        hccp_err("[get][rs_get_qp_context]ra_peer_get_qp_context failed ret(%d)", ret);
    }
    return ret;
}

int RaPeerCqCreate(struct ra_rdma_handle *rdmaHandle, struct cq_attr *attr)
{
    int ret;
    unsigned int phyId = rdmaHandle->rdev_info.phy_id;

    PEER_PTHREAD_MUTEX_LOCK(&gRaPeerMutex[phyId]);
    RsSetCtx(phyId);
    ret = RsCqCreate(phyId, rdmaHandle->rdev_index, attr);
    if (ret) {
        hccp_err("[create][ra_peer_cq_create]rs_cq_create failed ret[%d]", ret);
    }
    PEER_PTHREAD_MUTEX_UNLOCK(&gRaPeerMutex[phyId]);

    return ret;
}

int RaPeerCqDestroy(struct ra_rdma_handle *rdmaHandle, struct cq_attr *attr)
{
    int ret;
    unsigned int phyId = rdmaHandle->rdev_info.phy_id;

    PEER_PTHREAD_MUTEX_LOCK(&gRaPeerMutex[phyId]);
    RsSetCtx(phyId);
    ret = RsCqDestroy(phyId, rdmaHandle->rdev_index, attr);
    if (ret) {
        hccp_err("[destroy][ra_peer_cq_destroy]rs_cq_destroy failed ret[%d]", ret);
    }
    PEER_PTHREAD_MUTEX_UNLOCK(&gRaPeerMutex[phyId]);

    return ret;
}

int RaPeerNormalQpCreate(struct ra_rdma_handle *rdmaHandle, struct ibv_qp_init_attr *qpInitAttr,
    void **qpHandle, void **qp)
{
    unsigned int phyId = rdmaHandle->rdev_info.phy_id;
    struct ra_qp_handle *qpPeer = NULL;
    struct rs_qp_resp qpResp = { 0 };
    int ret;

    qpPeer = (struct ra_qp_handle *)calloc(1, sizeof(struct ra_qp_handle));
    CHK_PRT_RETURN(qpPeer == NULL, hccp_err("[create][ra_normal_peer_qp]normal_qp_peer calloc failed."), -ENOMEM);

    PEER_PTHREAD_MUTEX_LOCK(&gRaPeerMutex[phyId]);
    RsSetCtx(phyId);
    ret = RsNormalQpCreate(phyId, rdmaHandle->rdev_index, qpInitAttr, &qpResp, qp);
    if (ret) {
        hccp_err("[create][ra_normal_peer_qp]rs_normal_qp_create failed ret[%d]", ret);
        PEER_PTHREAD_MUTEX_UNLOCK(&gRaPeerMutex[phyId]);
        goto calloc_err;
    }
    PEER_PTHREAD_MUTEX_UNLOCK(&gRaPeerMutex[phyId]);
    qpPeer->phy_id = phyId;
    qpPeer->qpn = qpResp.qpn;
    qpPeer->psn = qpResp.psn;
    qpPeer->gid_idx = qpResp.gid_idx;
    qpPeer->rdev_index = rdmaHandle->rdev_index;
    qpPeer->rdma_handle = rdmaHandle;
    qpPeer->rdma_ops = rdmaHandle->rdma_ops;

    *qpHandle = qpPeer;
    return ret;

calloc_err:
    free(qpPeer);
    qpPeer = NULL;
    return ret;
}

int RaPeerNormalQpDestroy(struct ra_qp_handle *qpPeer)
{
    int ret;

    PEER_PTHREAD_MUTEX_LOCK(&gRaPeerMutex[qpPeer->phy_id]);
    RsSetCtx(qpPeer->phy_id);
    ret = RsNormalQpDestroy(qpPeer->phy_id, qpPeer->rdev_index, qpPeer->qpn);
    if (ret) {
        hccp_err("[destroy][ra_peer_normal_qp]ra close failed ret(%d)", ret);
    }
    PEER_PTHREAD_MUTEX_UNLOCK(&gRaPeerMutex[qpPeer->phy_id]);
    free(qpPeer);
    qpPeer = NULL;
    return ret;
}

int RaPeerSetQpAttrQos(struct ra_qp_handle *qpPeer, struct qos_attr *attr)
{
    RsSetCtx(qpPeer->phy_id);
    int ret = RsSetQpAttrQos(qpPeer->phy_id, qpPeer->rdev_index, qpPeer->qpn, attr);
    CHK_PRT_RETURN(ret, hccp_err("[ra_peer_set_qp_attr_qos]rs_set_qp_attr_qos failed ret(%d)", ret), ret);
    return ret;
}

int RaPeerSetQpAttrTimeout(struct ra_qp_handle *qpPeer, unsigned int *timeout)
{
    RsSetCtx(qpPeer->phy_id);
    int ret = RsSetQpAttrTimeout(qpPeer->phy_id, qpPeer->rdev_index, qpPeer->qpn, timeout);
    CHK_PRT_RETURN(ret, hccp_err("[ra_peer_set_qp_attr_timeout]rs_set_qp_attr_timeout failed ret(%d)", ret), ret);
    return ret;
}

int RaPeerSetQpAttrRetryCnt(struct ra_qp_handle *qpPeer, unsigned int *retryCnt)
{
    RsSetCtx(qpPeer->phy_id);
    int ret = RsSetQpAttrRetryCnt(qpPeer->phy_id, qpPeer->rdev_index, qpPeer->qpn, retryCnt);
    CHK_PRT_RETURN(ret, hccp_err("[ra_peer_set_qp_attr_retry_cnt]rs_set_qp_attr_retry_cnt failed ret(%d)", ret), ret);
    return ret;
}

int RaPeerCreateCompChannel(struct ra_rdma_handle *rdmaHandle, void** compChannel)
{
    int ret;
    RsSetCtx(rdmaHandle->rdev_info.phy_id);
    ret = RsCreateCompChannel(rdmaHandle->rdev_info.phy_id, rdmaHandle->rdev_index, compChannel);
    CHK_PRT_RETURN(ret, hccp_err("[ra_peer_create_comp_channel]rs_create_comp_channel failed ret(%d)", ret), ret);

    return ret;
}

int RaPeerDestroyCompChannel(void* compChannel)
{
    int ret;

    ret = RsDestroyCompChannel(compChannel);
    CHK_PRT_RETURN(ret, hccp_err("[ra_peer_destroy_comp_channel]rs_create_comp_channel failed ret(%d)", ret), ret);

    return ret;
}

int RaPeerCreateSrq(struct ra_rdma_handle *rdmaHandle, struct srq_attr *attr)
{
    int ret;

    // 创建srq&srq cq
    RsSetCtx(rdmaHandle->rdev_info.phy_id);
    ret = RsCreateSrq(rdmaHandle->rdev_info.phy_id, rdmaHandle->rdev_index, attr);
    CHK_PRT_RETURN(ret, hccp_err("[ra_peer_create_srq]rs_create_srq failed ret(%d)", ret), ret);

    return ret;
}

int RaPeerDestroySrq(struct ra_rdma_handle *rdmaHandle, struct srq_attr *attr)
{
    int ret;

    // 销毁srq&srq cq
    RsSetCtx(rdmaHandle->rdev_info.phy_id);
    ret = RsDestroySrq(rdmaHandle->rdev_info.phy_id, rdmaHandle->rdev_index, attr);
    CHK_PRT_RETURN(ret, hccp_err("[ra_peer_destroy_srq]rs_destroy_srq failed ret(%d)", ret), ret);

    return ret;
}

int RaPeerCreateEventHandle(int *eventHandle)
{
    int ret;

    ret = RsCreateEventHandle(eventHandle);
    CHK_PRT_RETURN(ret, hccp_err("[ra_peer_create_event_handle]rs_create_event_handle failed ret(%d)", ret), ret);

    return ret;
}

int RaPeerCtlEventHandle(int eventHandle, const void *fdHandle, int opcode, enum RaEpollEvent event)
{
    int ret;

    ret = RsCtlEventHandle(eventHandle, fdHandle, opcode, event);
    CHK_PRT_RETURN(ret, hccp_err("[ra_peer_ctl_event_handle]rs_ctl_event_handle failed ret(%d)", ret), ret);

    return ret;
}

int RaPeerWaitEventHandle(int eventHandle, struct socket_event_info *eventInfos, int timeout,
    unsigned int maxevents, unsigned int *eventsNum)
{
    int ret;

    ret = RsWaitEventHandle(eventHandle, eventInfos, timeout, maxevents, eventsNum);
    CHK_PRT_RETURN(ret, hccp_err("[ra_peer_wait_event_handle]rs_wait_event_handle failed ret(%d)", ret), ret);

    return ret;
}

int RaPeerDestroyEventHandle(int *eventHandle)
{
    int ret;

    ret = RsDestroyEventHandle(eventHandle);
    CHK_PRT_RETURN(ret, hccp_err("[ra_peer_destroy_event_handle]rs_destroy_event_handle failed ret(%d)", ret), ret);

    return ret;
}

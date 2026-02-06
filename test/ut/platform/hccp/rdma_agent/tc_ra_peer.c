/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "ra_peer.h"
#include "ra_rs_err.h"
#include <errno.h>
#include "securec.h"
#include "ut_dispatch.h"
#include "rs.h"
extern int RaPeerSetConnParam(struct SocketInfoT conn[],
    struct SocketFdData rs_conn[], unsigned int i, int buf_size);
extern int RaRdevInitCheckIp(int mode, struct rdev rdevInfo, char localIp[]);
extern int RaPeerLoopbackQpCreate(struct RaRdmaHandle *rdma_handle, struct LoopbackQpPair *qp_pair,
    void **qp_handle);
extern int RaPeerLoopbackSingleQpCreate(struct RaRdmaHandle *rdma_handle, struct RaQpHandle **qp_handle,
    struct ibv_qp **qp);
extern int RaPeerLoopbackQpModify(struct RaQpHandle *qp_handle0, struct RaQpHandle *qp_handle1);

int ra_peer_loopback_single_qp_create_stub(struct RaRdmaHandle *rdma_handle, struct RaQpHandle **qp_handle,
    struct ibv_qp **qp)
{
    static int call_num = 0;
    int ret = 0;
    call_num++;

    if (call_num == 1) {
        return RaPeerLoopbackSingleQpCreate(rdma_handle, qp_handle, qp);
    } else {
        return -1;
    }
    return ret;
}

void tc_peer()
{
    int ret;
    int dev_id = 0;
    int flag = 0;
    int port = 0;
    int timeout = 100;
    void *addr = NULL;
    void *data = NULL;
    int size = 0;
    int max_size = 2050;
    int access = 0;
    struct SendWr *wr = NULL;
    int wqe_index = 0;
    int index = 0;
    unsigned long pa = 0;
    unsigned long va = 0;
    struct qp_peer_info *qp_info = NULL;
    struct SocketConnectInfoT conn[1];
    struct SocketListenInfoT listen[1];
    struct SocketInfoT info[1];
    struct SocketCloseInfoT close[1] = {0};
    int sock_fd = 1;
    void *qp_handle;
    void *qp_handle_with_attr;
    int status = 0;
    struct RaInitConfig config = {
        .phyId = dev_id,
        .nicPosition = 1,
        .hdcType = 0,
    };
    config.phyId = 0;
    int ip_addr;
    unsigned int host_tgid = 0;
    int qpMode = 0;
    struct rdev rdev_info = {0};
    rdev_info.phyId = 0;
    rdev_info.family = AF_INET;
    rdev_info.localIp.addr.s_addr = 0;
    struct RaRdmaHandle rdma_handle_tmp = {
        .rdevInfo = rdev_info,
        .rdevIndex = 0,
    };
    struct RaSocketHandle socket_handle_tmp ={
        .rdevInfo = rdev_info,
    };
    struct RaRdmaHandle *rdma_handle = &rdma_handle_tmp;
    struct RaSocketHandle *socket_handle = &socket_handle_tmp;
    struct SocketFdData rs_conn[] = {0};

    listen[0].socketHandle = socket_handle;
    conn[0].socketHandle = socket_handle;
    close[0].socketHandle = socket_handle;
    info[0].socketHandle = socket_handle;
    struct QpExtAttrs extAttrs;
    extAttrs.version = QP_CREATE_WITH_ATTR_VERSION;
    extAttrs.qpMode = RA_RS_NOR_QP_MODE;

    ret = RaPeerInit(&config, 1);
    EXPECT_INT_EQ(0, ret);

    ret = RaPeerInit(&config, 1);
    EXPECT_INT_EQ(0, ret);

    ret = RaPeerSocketBatchConnect(0, conn, 1);
    EXPECT_INT_EQ(0, ret);

    ret = RaPeerSocketListenStart(0, listen, 1);

    ret = RaPeerSocketListenStop(0, listen, 1);

    ret = RaPeerGetSockets(0, 0, info, 1);
    EXPECT_INT_EQ(1, ret);

    info[0].socketHandle = socket_handle;
    info[0].status = 1;
    mocker((stub_fn_t)calloc, 10, NULL);
    ret = RaPeerGetSockets(0, 0, info, 1);
    EXPECT_INT_EQ(-12, ret);
    mocker_clean();

    info[0].fdHandle = calloc(1, sizeof(struct SocketPeerInfo));

    ret = RaPeerSocketSend(0, info[0].fdHandle, data, size);
    EXPECT_INT_EQ(0, size);

    ret = RaPeerSocketSend(0, info[0].fdHandle, data, max_size);
    EXPECT_INT_EQ(max_size, ret);

    ret = RaPeerSocketRecv(0, info[0].fdHandle, data, size);
    EXPECT_INT_EQ(0, size);

    ret = RaPeerSocketRecv(0, info[0].fdHandle, data, max_size);
    EXPECT_INT_EQ(max_size, ret);

    rs_conn[0].phyId = 0;
    rs_conn[0].fd = 0;
    ret = RaPeerSetConnParam(info, rs_conn, 0, 0);
    EXPECT_INT_EQ(0, ret);

    unsigned int temp_depth = 128;
    unsigned int qp_num = 0;
    ret = RaPeerSetTsqpDepth(rdma_handle, temp_depth, &qp_num);
    EXPECT_INT_EQ(0, ret);

    ret = RaPeerGetTsqpDepth(rdma_handle, &temp_depth, &qp_num);
    EXPECT_INT_EQ(0, ret);

    ret = RaPeerQpCreate(rdma_handle, flag, qpMode, &qp_handle);
    EXPECT_INT_EQ(0, ret);
    EXPECT_ADDR_NE(NULL, qp_handle);
    ret = RaPeerQpCreateWithAttrs(rdma_handle, &extAttrs, &qp_handle_with_attr);
    EXPECT_INT_EQ(0, ret);
    EXPECT_ADDR_NE(NULL, qp_handle_with_attr);

    struct QosAttr QosAttr= {0};
    QosAttr.tc = 110;
    QosAttr.sl = 3;
    ret = RaPeerSetQpAttrQos(qp_handle, &QosAttr);
    EXPECT_INT_EQ(0, ret);

    unsigned int rdma_timeout = 6;
    ret = RaPeerSetQpAttrTimeout(qp_handle, &rdma_timeout);
    EXPECT_INT_EQ(0, ret);

    unsigned int retry_cnt = 5;
    ret = RaPeerSetQpAttrRetryCnt(qp_handle, &retry_cnt);
    EXPECT_INT_EQ(0, ret);

    ret = RaPeerNotifyBaseAddrInit(EVENTID, 0);
    EXPECT_INT_EQ(0, ret);

    ret = RaPeerNotifyBaseAddrInit(NO_USE, 0);
    EXPECT_INT_EQ(0, ret);

    ret = NotifyBaseAddrUninit(EVENTID, 0);
    EXPECT_INT_EQ(0, ret);

    ret = NotifyBaseAddrUninit(NO_USE, 0);
    EXPECT_INT_EQ(0, ret);

    ret = RaPeerQpConnectAsync(qp_handle, info[0].fdHandle);
    EXPECT_INT_EQ(0, size);

    close[0].fdHandle = info[0].fdHandle;
    ret = RaPeerSocketBatchClose(0, close, 1);
    EXPECT_INT_EQ(0, size);

    mocker(memset_s, 20, 1);
    RaPeerSocketBatchClose(0, close, 1);
    mocker_clean();

    ret = RaPeerGetQpStatus(qp_handle, &status);
    EXPECT_INT_EQ(0, ret);

    struct MrInfoT mr_info;
    mr_info.addr = addr;
    mr_info.size = size;
    mr_info.access = access;

    void *mr_handle = NULL;

    ret = RaPeerMrReg(qp_handle, &mr_info);
    EXPECT_INT_EQ(0, ret);

    ret = RaPeerMrDereg(qp_handle, &mr_info);
    EXPECT_INT_EQ(0, ret);

    ret = RaPeerRegisterMr(rdma_handle, &mr_info, &mr_handle);
    EXPECT_INT_EQ(0, ret);

    ret = RaPeerDeregisterMr(rdma_handle, mr_handle);
    EXPECT_INT_EQ(0, ret);

    void *comp_channel = NULL;
    ret = RaPeerCreateCompChannel(rdma_handle, &comp_channel);
    EXPECT_INT_EQ(0, ret);

    ret = RaPeerDestroyCompChannel(comp_channel);
    EXPECT_INT_EQ(0, ret);

    ret = RaPeerSendWr(qp_handle, wr, &wqe_index);
    EXPECT_INT_EQ(0, ret);

    struct SrqAttr attr = {0};
    ret = RaPeerCreateSrq(rdma_handle, &attr);
    EXPECT_INT_EQ(0, ret);

    ret = RaPeerDestroySrq(rdma_handle, &attr);
    EXPECT_INT_EQ(0, ret);

    struct RecvWrlistData rev_wr = {0};
    rev_wr.wrId = 100;
    rev_wr.memList.lkey = 0xff;
    rev_wr.memList.addr = addr;
    rev_wr.memList.len = size;
    unsigned int recv_num = 1;
    unsigned int rev_complete_num = 0;
    ret = RaPeerRecvWrlist(qp_handle, &rev_wr, recv_num, &rev_complete_num);
    EXPECT_INT_EQ(0, ret);

    unsigned long long notify_size;
    ret = RaPeerGetNotifyBaseAddr(qp_handle, &va, &notify_size);
    EXPECT_INT_EQ(0, ret);

    ret = RaPeerQpDestroy(qp_handle);
    EXPECT_INT_EQ(0, ret);
    ret = RaPeerQpDestroy(qp_handle_with_attr);
    EXPECT_INT_EQ(0, ret);

    ret = RaPeerDeinit(&config);
    EXPECT_INT_EQ(0, ret);

    ret = RaPeerDeinit(&config);
    EXPECT_INT_EQ(0, ret);

    struct SocketConnectInfoT connect_err_rs[1] = { 0 };
    connect_err_rs[0].socketHandle = socket_handle;
    mocker((stub_fn_t)RsSocketBatchConnect, 10, -1);
    ret = RaPeerSocketBatchConnect(0, connect_err_rs, 1);
    EXPECT_INT_EQ(-1, ret);
    mocker((stub_fn_t)RsSocketSetScopeId, 10, -2);
    ret = RaPeerSocketBatchConnect(0, connect_err_rs, 1);
    EXPECT_INT_EQ(-2, ret);
    mocker_clean();

    struct SocketListenInfoT listen_err_rs[1] = {0};
    listen_err_rs[0].socketHandle = socket_handle;
    mocker((stub_fn_t)RsSocketListenStart, 10, -1);
    ret = RaPeerSocketListenStart(0, listen_err_rs, 1);
    EXPECT_INT_NE(0, ret);
    mocker((stub_fn_t)RsSocketSetScopeId, 10, -2);
    ret = RaPeerSocketListenStart(0, listen_err_rs, 1);
    EXPECT_INT_EQ(-2, ret);
    mocker_clean();

    struct SocketListenInfoT listen_err_rs2[1];
    listen_err_rs2[0].socketHandle = socket_handle;
    listen_err_rs2[0].port = 0;
    mocker((stub_fn_t)RsSocketListenStop, 10, -1);
    ret = RaPeerSocketListenStop(0, listen_err_rs2, 1);
    EXPECT_INT_EQ(-1, ret);
    mocker_clean();

    struct SocketInfoT info_err_rs[1];
    info_err_rs[0].socketHandle = socket_handle;
    info_err_rs[0].fdHandle = NULL;
    mocker((stub_fn_t)calloc, 10, NULL);
    ret = RaPeerGetSockets(0, 0, info_err_rs, 1);
    EXPECT_INT_EQ(1, ret);
    mocker_clean();

    mocker(RaPeerSetConnParam, 1, 1);
    ret = RaPeerGetSockets(0, 0, info_err_rs, 1);
    EXPECT_INT_EQ(1, ret);
    mocker_clean();

    struct SocketInfoT info_err_rs2[1];
    info_err_rs2[0].socketHandle = socket_handle;
    info_err_rs2[0].fdHandle = NULL;
    mocker((stub_fn_t)memcpy_s, 10, -1);
    ret = RaPeerGetSockets(0, 0, info_err_rs2, 1);
    EXPECT_INT_EQ(-ESAFEFUNC, ret);
    mocker_clean();

    struct SocketInfoT info_err_rs3[1];
    info_err_rs3[0].socketHandle = socket_handle;
    info_err_rs3[0].fdHandle = NULL;
    mocker_ret((stub_fn_t)memcpy_s, 0, 1, 1);
    ret = RaPeerGetSockets(0, 0, info_err_rs3, 1);
    EXPECT_INT_EQ(1, ret);
    mocker_clean();

    struct SocketInfoT  info_err_rs4[1];
    info_err_rs4[0].socketHandle = socket_handle;
    info_err_rs4[0].fdHandle = NULL;
    mocker((stub_fn_t)RsGetSockets, 10, 0);
    mocker((stub_fn_t)RsGetSslEnable, 10, -1);
    ret = RaPeerGetSockets(0, 0, info_err_rs4, 1);
    EXPECT_INT_EQ(-1, ret);
    mocker_clean();

    mocker((stub_fn_t)RsSetTsqpDepth, 10, -1);
    ret = RaPeerSetTsqpDepth(rdma_handle, temp_depth, &qp_num);
    EXPECT_INT_EQ(-1, ret);

    mocker((stub_fn_t)RsGetTsqpDepth, 10, -1);
    ret = RaPeerGetTsqpDepth(rdma_handle, &temp_depth, &qp_num);
    EXPECT_INT_EQ(-1, ret);

	qp_handle = NULL;
    qp_handle_with_attr = NULL;
    mocker((stub_fn_t)calloc, 10, NULL);
    ret  = RaPeerQpCreate(rdma_handle, flag, qpMode, &qp_handle);
    EXPECT_INT_EQ(-ENOMEM, ret);
    EXPECT_ADDR_EQ(NULL, qp_handle);
    ret  = RaPeerQpCreateWithAttrs(rdma_handle, &extAttrs, &qp_handle_with_attr);
    EXPECT_INT_EQ(-ENOMEM, ret);
    EXPECT_ADDR_EQ(NULL, qp_handle_with_attr);
    mocker_clean();

    mocker((stub_fn_t)RsQpCreate, 10, 1);
    mocker((stub_fn_t)RsQpCreateWithAttrs, 10, 1);
    ret = RaPeerQpCreate(rdma_handle, flag, qpMode, &qp_handle);
    EXPECT_INT_EQ(1, ret);
    EXPECT_ADDR_EQ(NULL, qp_handle);
    ret  = RaPeerQpCreateWithAttrs(rdma_handle, &extAttrs, &qp_handle_with_attr);
    EXPECT_INT_EQ(1, ret);
    EXPECT_ADDR_EQ(NULL, qp_handle_with_attr);
    mocker_clean();

    ret = RaPeerQpCreate(rdma_handle, flag, qpMode, &qp_handle);
    EXPECT_INT_EQ(0, ret);
    mocker((stub_fn_t)RsQpDestroy, 10, -1);
    ret = RaPeerQpDestroy(qp_handle);
    EXPECT_INT_EQ(-1, ret);
    mocker_clean();

    qpMode = 2;
    ret = RaPeerQpCreate(rdma_handle, flag, qpMode, &qp_handle);
    EXPECT_INT_EQ(0, ret);

    ret = RaPeerSetQpAttrQos(qp_handle, &QosAttr);
    EXPECT_INT_EQ(0, ret);

    ret = RaPeerSetQpAttrTimeout(qp_handle, &timeout);
    EXPECT_INT_EQ(0, ret);

    ret = RaPeerSetQpAttrRetryCnt(qp_handle, &retry_cnt);
    EXPECT_INT_EQ(0, ret);

    ret = RaPeerNotifyBaseAddrInit(1000, 0);
    EXPECT_INT_EQ(-EINVAL, ret);

    ret = NotifyBaseAddrUninit(1000, 0);
    EXPECT_INT_EQ(-EINVAL, ret);

    mocker((stub_fn_t)RsGetQpStatus, 10, -1);
    ret = RaPeerGetQpStatus(qp_handle, &status);
    EXPECT_INT_EQ(-1, ret);

    mocker((stub_fn_t)RsMrReg, 10, -1);
    ret = RaPeerMrReg(qp_handle, &mr_info);
    EXPECT_INT_EQ(-1, ret);

    mocker((stub_fn_t)RsMrDereg, 10, -1);
    ret = RaPeerMrDereg(qp_handle, &mr_info);
    EXPECT_INT_EQ(-1, ret);

    mocker((stub_fn_t)RsRegisterMr, 10, -1);
    ret = RaPeerRegisterMr(rdma_handle, &mr_info, &mr_handle);
    EXPECT_INT_EQ(-1, ret);

    mocker((stub_fn_t)RsDeregisterMr, 10, -1);
    ret = RaPeerDeregisterMr(rdma_handle, mr_handle);
    EXPECT_INT_EQ(-1, ret);

    mocker((stub_fn_t)RsCreateCompChannel, 10, -1);
    ret = RaPeerCreateCompChannel(rdma_handle, &comp_channel);
    EXPECT_INT_EQ(-1, ret);

    mocker((stub_fn_t)RsDestroyCompChannel, 10, -1);
    ret = RaPeerDestroyCompChannel(comp_channel);
    EXPECT_INT_EQ(-1, ret);

    struct SrqAttr attr_srq = {0};
    mocker((stub_fn_t)RsCreateSrq, 10, -1);
    ret = RaPeerCreateSrq(rdma_handle, &attr_srq);
    EXPECT_INT_EQ(-1, ret);

    mocker((stub_fn_t)RsDestroySrq, 10, -1);
    ret = RaPeerDestroySrq(rdma_handle, &attr_srq);
    EXPECT_INT_EQ(-1, ret);

    mocker((stub_fn_t)RsRecvWrlist, 10, -1);
    ret = RaPeerRecvWrlist(qp_handle, &rev_wr, recv_num, &rev_complete_num);
    EXPECT_INT_EQ(-1, ret);
    mocker_clean();

    struct SocketInfoT info_rs[1];
    info_rs[0].socketHandle = socket_handle;

    ret = RaPeerGetSockets(0, 0, info_rs, 1);
    EXPECT_INT_EQ(1, ret);

    info_rs[0].fdHandle = calloc(1, sizeof(struct SocketPeerInfo));

    mocker((stub_fn_t)RsQpConnectAsync, 10, -1);
    ret = RaPeerQpConnectAsync(qp_handle, info_rs[0].fdHandle);
    EXPECT_INT_EQ(-1, ret);

    mocker((stub_fn_t)RsGetNotifyMrInfo, 10, -1);
    ret = RaPeerGetNotifyBaseAddr(qp_handle, &va, &notify_size);
    EXPECT_INT_EQ(-1, ret);

    mocker((stub_fn_t)RsPeerSocketSend, 10, -1);
    ret = RaPeerSocketSend(dev_id, info_rs[0].fdHandle, data, size);
    EXPECT_INT_EQ(-1, ret);

    ret = RaPeerQpDestroy(qp_handle);
    EXPECT_INT_EQ(0, ret);

    struct SocketCloseInfoT close_rs[1] = {0};
    close_rs[0].fdHandle = info_rs[0].fdHandle;
    close_rs[0].socketHandle = socket_handle;
    mocker((stub_fn_t)RsSocketBatchClose, 10, -1);
    ret = RaPeerSocketBatchClose(0, close_rs, 1);
    EXPECT_INT_EQ(-1, ret);
    mocker_clean();

    mocker((stub_fn_t)RsInit, 10, -1);
    ret = RaPeerInit(&config, 1);
    EXPECT_INT_EQ(-1, ret);
    mocker_clean();

    mocker((stub_fn_t)RsDeinit, 10, -11);
    ret = RaPeerDeinit(&config);
    EXPECT_INT_EQ(-11, ret);
    mocker_clean();

    mocker((stub_fn_t)RsDeinit, 10, -1);
    ret = RaPeerDeinit(&config);
    EXPECT_INT_EQ(-1, ret);

    mocker_clean();

    return;
}

void tc_peer_fail()
{
    struct RaSocketHandle socket_handle;
    socket_handle.rdevInfo.phyId = 0;
    socket_handle.rdevInfo.family = 0;
    struct SocketConnectInfoT conn[1];
    conn[0].socketHandle = &socket_handle;
    conn[0].port = 0;
    struct SocketConnectInfo rs_conn[1] = {0};
    mocker((stub_fn_t)memcpy_s, 10, -1);
    RaGetSocketConnectInfo(conn, 1, rs_conn, 2);
    mocker_clean();

    struct SocketListenInfoT conn_listen[1];
    struct SocketListenInfo rs_conn_listen[1];
    conn_listen[0].phase = 0;
    conn_listen[0].err = 0;
    conn_listen[0].socketHandle = &socket_handle;
    conn_listen[0].port = 0;
    mocker((stub_fn_t)memcpy_s, 10, -1);
    RaGetSocketListenInfo(conn_listen, 1, rs_conn_listen, 2);
    mocker_clean();

    rs_conn_listen[0].phase = 0;
    rs_conn_listen[0].err = 0;
    rs_conn_listen[0].phyId = 0;
    rs_conn_listen[0].family = 0;
    conn_listen[0].socketHandle = &socket_handle;
    rs_conn_listen[0].port = 0;
    mocker((stub_fn_t)memcpy_s, 10, -1);
    RaGetSocketListenResult(rs_conn_listen, 1, conn_listen, 2);
    mocker_clean();

    struct SocketListenInfoT conn_listen_info[1] = {0};
    conn_listen_info[0].port  = 0;
    conn_listen_info[0].socketHandle = &socket_handle;
    mocker((stub_fn_t)RaGetSocketListenInfo, 10, 0);
    mocker((stub_fn_t)RsSocketListenStart, 10, -1);
    RaPeerSocketListenStart(0, conn_listen_info, 1);
    mocker((stub_fn_t)RsSocketListenStop, 10, -1);
    RaPeerSocketListenStop(0, conn_listen_info, 1);
    mocker_clean();

    struct SocketPeerInfo peer_socket_handle = {0};
    int ret;

    ret = RaPeerSocketSend(0, &peer_socket_handle, NULL, 0);
    EXPECT_INT_EQ(0, ret);

    peer_socket_handle.sslEnable = 1;
    ret = RaPeerSocketSend(0, &peer_socket_handle, NULL, 0);
    EXPECT_INT_EQ(0, ret);

    peer_socket_handle.sslEnable = 0;
    ret = RaPeerSocketRecv(0, &peer_socket_handle, NULL, 0);
    EXPECT_INT_EQ(0, ret);

    peer_socket_handle.sslEnable = 1;
    ret = RaPeerSocketRecv(0, &peer_socket_handle, NULL, 0);
    EXPECT_INT_EQ(0, ret);

    struct rdev rdev_info;
	rdev_info.phyId = 0;
    struct SocketWlistInfoT white_list[1];
    mocker((stub_fn_t)inet_ntoa, 10, NULL);
    RaPeerSocketWhiteListAdd(rdev_info, white_list, 1);
    RaPeerSocketWhiteListDel(rdev_info, white_list, 1);
    mocker_clean();

    mocker((stub_fn_t)RsSocketDeinit, 10, -1);
    RaPeerSocketDeinit(rdev_info);
    mocker_clean();

    mocker((stub_fn_t)RsPeerGetIfnum, 10, -1);
    unsigned int num = 0;
    RaPeerGetIfnum(0, &num);
    mocker_clean();

    struct InterfaceInfo interface_infos[1];
    mocker((stub_fn_t)RsPeerGetIfaddrs, 10, -1);
    RaPeerGetIfaddrs(0, interface_infos, &num);
    mocker_clean();

    return;
}

void tc_ra_peer_epoll_ctl_add()
{
    int ret;

    ret = RaPeerEpollCtlAdd(NULL, RA_EPOLLIN);
    EXPECT_INT_EQ(0, ret);

    mocker((stub_fn_t)RsEpollCtlAdd, 3, -1);
    ret = RaPeerEpollCtlAdd(NULL, RA_EPOLLIN);
    EXPECT_INT_EQ(-1, ret);
    mocker_clean();

    return;
}

void tc_ra_peer_set_tcp_recv_callback()
{
    RaSetTcpRecvCallback(NULL, NULL);
    (void)RaPeerSetTcpRecvCallback(0, NULL);

    struct RaSocketHandle abc = {0};
    int cb = 0;
    RaSetTcpRecvCallback(&abc, &cb);

    abc.rdevInfo.phyId = 10000;
    RaSetTcpRecvCallback(&abc, &cb);
    return;
}

void tc_ra_peer_epoll_ctl_mod()
{
    int ret;

    ret = RaPeerEpollCtlMod(NULL, RA_EPOLLIN);
    EXPECT_INT_EQ(0, ret);

    mocker((stub_fn_t)RsEpollCtlMod, 3, -1);
    ret = RaPeerEpollCtlMod(NULL, RA_EPOLLIN);
    EXPECT_INT_EQ(-1, ret);
    mocker_clean();

    return;
}

void tc_ra_peer_epoll_ctl_del()
{
    int ret;
    struct SocketPeerInfo fd_handle = {0};

    ret = RaPeerEpollCtlDel((const void *)&fd_handle);
    EXPECT_INT_EQ(0, ret);

    mocker((stub_fn_t)RsEpollCtlDel, 3, -1);
    ret = RaPeerEpollCtlDel((const void *)&fd_handle);
    EXPECT_INT_EQ(-1, ret);
    mocker_clean();

    return;
}

void tc_ra_peer_cq_create()
{
    int ret;
    struct RaRdmaHandle rdma_handle;
    rdma_handle.rdevInfo.phyId = 0;
    rdma_handle.rdevIndex = 0;

    struct ibv_cq *ibSendCq;
    struct ibv_cq *ibRecvCq;
    struct CqAttr attr;
    attr.ibSendCq = &ibSendCq;
    attr.ibRecvCq = &ibRecvCq;
    attr.sendCqDepth = 16384;
    attr.recvCqDepth = 16384;
    attr.sendCqEventId = 1;
    attr.recvCqEventId = 2;

    ret = RaPeerCqCreate(&rdma_handle, &attr);
    EXPECT_INT_EQ(0, ret);

    ret = RaPeerCqDestroy(&rdma_handle, &attr);
    EXPECT_INT_EQ(0, ret);

    mocker((stub_fn_t)RsCqCreate, 3, -1);
    ret = RaPeerCqCreate(&rdma_handle, &attr);
    EXPECT_INT_EQ(-1, ret);
    mocker_clean();

    ret = RaPeerCqCreate(&rdma_handle, &attr);
    EXPECT_INT_EQ(0, ret);
    mocker((stub_fn_t)RsCqDestroy, 3, -1);
    ret = RaPeerCqDestroy(&rdma_handle, &attr);
    EXPECT_INT_EQ(-1, ret);
    mocker_clean();
    return;
}

void tc_ra_peer_normal_qp_create()
{
    int ret;
    struct RaQpHandle *qp_handle;
    struct ibv_qp_init_attr qp_init_attr;
    struct RaRdmaHandle rdma_handle;
    rdma_handle.rdevInfo.phyId = 0;
    rdma_handle.rdevIndex = 0;
    void** qp = NULL;
    ret = RaPeerNormalQpCreate(&rdma_handle, &qp_init_attr, &qp_handle, qp);
    EXPECT_INT_EQ(0, ret);

    ret = RaPeerNormalQpDestroy(qp_handle);
    EXPECT_INT_EQ(0, ret);

    mocker((stub_fn_t)calloc, 3, NULL);
    ret = RaPeerNormalQpCreate(&rdma_handle, &qp_init_attr, &qp_handle, qp);
    EXPECT_INT_EQ(-ENOMEM, ret);
    mocker_clean();

    mocker((stub_fn_t)RsNormalQpCreate, 3, -1);
    ret = RaPeerNormalQpCreate(&rdma_handle, &qp_init_attr, &qp_handle, qp);
    EXPECT_INT_EQ(-1, ret);
    mocker_clean();

    ret = RaPeerNormalQpCreate(&rdma_handle, &qp_init_attr, &qp_handle, qp);
    EXPECT_INT_EQ(0, ret);
    mocker((stub_fn_t)RsNormalQpDestroy, 3, -1);
    ret = RaPeerNormalQpDestroy(qp_handle);
    EXPECT_INT_EQ(-1, ret);
    mocker_clean();
    return;
}

void tc_ra_peer_create_event_handle()
{
    int ret;
    int fd;

    ret = RaPeerCreateEventHandle(&fd);
    EXPECT_INT_EQ(0, ret);
}

void tc_ra_peer_ctl_event_handle()
{
    int ret;
    int fd_handle;

    ret = RaPeerCtlEventHandle(0, NULL, 0, RA_EPOLLONESHOT);
    EXPECT_INT_EQ(-EINVAL, ret);

    ret = RaPeerCtlEventHandle(0, &fd_handle, 1, RA_EPOLLONESHOT);
    EXPECT_INT_EQ(0, ret);
}

void tc_ra_peer_wait_event_handle()
{
    int ret;
    int fd;

    ret = RaPeerWaitEventHandle(0, NULL, 0, -1, 0);
    EXPECT_INT_EQ(0, ret);
}

void tc_ra_peer_destroy_event_handle()
{
    int ret;
    int fd;

    ret = RaPeerDestroyEventHandle(&fd);
    EXPECT_INT_EQ(0, ret);
}

void tc_ra_loopback_qp_create()
{
    struct RaRdmaHandle *rdma_handle2;
    struct RaRdmaHandle *rdma_handle;
    struct rdev rdev_info = {0};
    rdev_info.phyId = 0;
    rdev_info.family = AF_INET;
    rdev_info.localIp.addr.s_addr = 0;
    int ret = 0;

    mocker(RaRdevInitCheckIp, 10, 0);
    ret = RaRdevInit(NETWORK_PEER_ONLINE, NO_USE, rdev_info, &rdma_handle);
    EXPECT_INT_EQ(0, ret);
    ret = RaRdevGetHandle(rdev_info.phyId, &rdma_handle2);
    EXPECT_INT_EQ(0, ret);
    EXPECT_INT_EQ(rdma_handle, rdma_handle2);
    mocker_clean();

    struct LoopbackQpPair qp_pair;
    void *qp_handle = NULL;

    ret = RaLoopbackQpCreate(NULL, NULL, NULL);
    EXPECT_INT_EQ(128103, ret);

    ret = RaLoopbackQpCreate(rdma_handle, NULL, NULL);
    EXPECT_INT_EQ(128103, ret);

    ret = RaLoopbackQpCreate(rdma_handle, &qp_pair, NULL);
    EXPECT_INT_EQ(128103, ret);

    rdma_handle->rdevInfo.phyId = 128;
    ret = RaLoopbackQpCreate(rdma_handle, &qp_pair, &qp_handle);
    EXPECT_INT_EQ(128103, ret);

    rdma_handle->rdevInfo.phyId = 0;
    mocker(RaPeerLoopbackQpCreate, 10, -1);
    ret = RaLoopbackQpCreate(rdma_handle, &qp_pair, &qp_handle);
    EXPECT_INT_EQ(128100, ret);

    mocker_clean();
    ret = RaLoopbackQpCreate(rdma_handle, &qp_pair, &qp_handle);
    EXPECT_INT_EQ(0, ret);
    ret = RaQpDestroy(qp_handle);
    EXPECT_INT_EQ(0, ret);

    ret = RaRdevDeinit(rdma_handle, NO_USE);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();
}

void tc_ra_peer_loopback_qp_create()
{
    struct rdev rdev_info = {0};
    rdev_info.phyId = 0;
    rdev_info.family = AF_INET;
    rdev_info.localIp.addr.s_addr = 0;
    struct RaRdmaHandle rdma_handle_tmp = {
        .rdevInfo = rdev_info,
        .rdevIndex = 0,
    };
    struct LoopbackQpPair qp_pair;
    void *qp_handle = NULL;
    int ret = 0;

    mocker(RaPeerLoopbackSingleQpCreate, 10, -1);
    ret = RaPeerLoopbackQpCreate(&rdma_handle_tmp, &qp_pair, &qp_handle);
    EXPECT_INT_EQ(-1, ret);
    mocker_clean();

    mocker_invoke(RaPeerLoopbackSingleQpCreate, ra_peer_loopback_single_qp_create_stub, 10);
    ret = RaPeerLoopbackQpCreate(&rdma_handle_tmp, &qp_pair, &qp_handle);
    EXPECT_INT_EQ(-1, ret);
    mocker_clean();

    mocker(RaPeerLoopbackQpModify, 10, -1);
    ret = RaPeerLoopbackQpCreate(&rdma_handle_tmp, &qp_pair, &qp_handle);
    EXPECT_INT_EQ(-1, ret);
    mocker_clean();
}

void tc_ra_peer_loopback_single_qp_create()
{
    struct rdev rdev_info = {0};
    rdev_info.phyId = 0;
    rdev_info.family = AF_INET;
    rdev_info.localIp.addr.s_addr = 0;
    struct RaRdmaHandle rdma_handle_tmp = {
        .rdevInfo = rdev_info,
        .rdevIndex = 0,
    };
    struct RaQpHandle *qp_handle = NULL;
    struct ibv_qp *qp = NULL;
    int ret = 0;

    mocker(RaPeerCqCreate, 10, -1);
    ret = RaPeerLoopbackSingleQpCreate(&rdma_handle_tmp, &qp_handle, &qp);
    EXPECT_INT_EQ(-1, ret);
    mocker_clean();

    mocker(RaPeerNormalQpCreate, 10, -1);
    ret = RaPeerLoopbackSingleQpCreate(&rdma_handle_tmp, &qp_handle, &qp);
    EXPECT_INT_EQ(-1, ret);
    mocker_clean();
}

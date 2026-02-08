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
#include <sched.h>
#include <sys/mman.h>
#include "ra.h"
#include "ra_rs_err.h"
#include "ra_client_host.h"
#include "hccp.h"
#include "ut_dispatch.h"
#include "stdlib.h"
#include "securec.h"
#include <pthread.h>
#include "dlfcn.h"
#include "rs.h"
#include "dl.h"
#include "ra_hdc.h"
#include "ra_hdc_lite.h"
#include "ra_hdc_rdma_notify.h"
#include "ra_hdc_rdma.h"
#include "ra_async.h"
#include "ra_hdc_socket.h"
#include "dl_hal_function.h"
#include "ra_peer.h"
#include "ra_adp.h"
#include "ascend_hal.h"
#include <errno.h>
#include "ra_comm.h"

extern int HdcSendRecvPkt(void *session, void *p_send_rcv_buf, unsigned int in_buf_len, unsigned int out_data_len);

extern int RaPeerRdevInit(struct RaRdmaHandle *rdma_handle, unsigned int notify_type, struct rdev rdev_info, unsigned int *rdevIndex);
extern int RsRdevInit(struct rdev rdev_info, unsigned int notify_type, unsigned int *rdevIndex);
extern int ra_peer_get_server_devid(int logic_devid, int *server_devid);
extern int RsRdevDeinit(unsigned int dev_id, unsigned int notify_type, unsigned int rdevIndex);
extern int RaPeerSocketWhiteListAdd(struct rdev rdev_info, struct SocketWlistInfoT white_list[], unsigned int num);
extern int RsSocketWhiteListAdd(struct rdev rdev_info, struct SocketWlistInfoT white_list[], unsigned int num);
extern int RsSocketWhiteListDel(struct rdev rdev_info, struct SocketWlistInfoT white_list[], unsigned int num);
extern int RaGetSocketConnectInfo(const struct SocketConnectInfoT conn[], unsigned int num,
    struct SocketConnectInfo rsConn[], unsigned int rsNum);
extern int RaGetSocketListenResult(const struct SocketListenInfo rsConn[], unsigned int rsNum,
    struct SocketListenInfoT conn[], unsigned int num);
extern int RsSocketListenStart(struct SocketListenInfo conn[], uint32_t num);
extern int RaPeerSetRsConnParam(struct SocketInfoT conn[], unsigned int num,
    struct SocketFdData rsConn[], unsigned int rsNum);
extern int RaInetPton(int family, union HccpIpAddr ip, char net_addr[], unsigned int len);
extern int RaHdcRdevDeinit(struct RaRdmaHandle *rdmaHandle, unsigned int notifyType);
extern int RaHdcRdevInit(struct RaRdmaHandle *rdmaHandle, unsigned int notifyType, struct rdev rdevInfo,
    unsigned int *rdevIndex);
extern int RaHdcInitApart(int dev_id, unsigned int *phyId);
extern int MsgHeadCheck(struct MsgHead *sendRcvHead, unsigned int opcode, int rsRet, unsigned int msgDataLen);
extern int RaRdevInitCheckIp(int mode, struct rdev rdevInfo, char localIp[]);
extern int RaHdcGetLiteSupport(struct RaRdmaHandle *rdmaHandle, unsigned int phyId);
extern int RaHdcNotifyBaseAddrInit(unsigned int notifyType, unsigned int phyId, unsigned long long **notifyVa);
extern int RaHdcAsyncSessionClose(unsigned int phyId);
extern void RaHwAsyncHdcClientDeinit(unsigned int phyId);
extern void RaHdcAsyncMutexDeinit(unsigned int phyId);
extern int RaRdevGetHandle(unsigned int phyId, void **rdmaHandle);
extern int RaSaveSnapshot(struct RaInfo *info, enum SaveSnapshotAction action);
extern int RaRestoreSnapshot(struct RaInfo *info);
extern int RaHdcAsyncSessionConnect(struct RaInitConfig *cfg);
extern int RaHdcInitSession(int peerNode, int peerDevid, unsigned int phyId, int hdcType, HDC_SESSION *session);

extern void *mmap(void *start, size_t length, int prot, int flags, int fd, off_t offsize);
extern int munmap(void *start, size_t length);
extern int open(const char *pathname, int flags);
extern int ioctl(int fd, unsigned long cmd, struct HostRoceNotifyInfo* info);
extern hdcError_t DlHalHdcRecv(HDC_SESSION session, struct drvHdcMsg *pMsg, int bufLen, UINT64 flag,
    int *recvBufCount, UINT32 timeout);
extern hdcError_t DlDrvHdcAllocMsg(HDC_SESSION session, struct drvHdcMsg **ppMsg, int count);
extern hdcError_t DlDrvHdcFreeMsg(struct drvHdcMsg *msg);
extern hdcError_t DlDrvHdcSessionClose(HDC_SESSION session);
extern int DlDrvDeviceGetIndexByPhyId(uint32_t phyId, uint32_t *devIndex);
extern int dlHalNotifyGetInfo(uint32_t devId, uint32_t tsId, uint32_t type, uint32_t *val);
extern int dlHalMemAlloc(void **pp, unsigned long long size, unsigned long long flag);
extern int gNotifyFd;

int sec_cpy_ret = 0;

#define MAX_DEV_NUM 8

void *stub_calloc(size_t nmemb, size_t size)
{
    static int i = 0;
    void *p = NULL;
    if (i == 0) {
        i++;
        p = (void *)malloc(nmemb * size);
        return;
    } else {
        return NULL;
    }
}

static unsigned int g_interface_version;

static int ra_get_interface_version_stub(unsigned int phyId, unsigned int interface_opcode, unsigned int* interface_version)
{
    *interface_version = g_interface_version;
    return 0;
}

DLLEXPORT drvError_t stub_session_connect_hdc(int peer_node, int peer_devid, HDC_CLIENT client, HDC_SESSION *session)
{
    static HDC_SESSION g_hdc_session = 1;
    *session = g_hdc_session;
    return 0;
}

void tc_hdc_env_init()
{
    struct RaInitConfig offline_hdc_config = {
        .phyId = 0,
        .nicPosition = NETWORK_OFFLINE,
        .hdcType = HDC_SERVICE_TYPE_RDMA,
        .enableHdcAsync = false,
    };
    struct ProcessRaSign p_ra_sign;
    p_ra_sign.tgid = 0;

    mocker_clean();
    mocker((stub_fn_t)drvHdcClientCreate, 1, 0);
    mocker_invoke((stub_fn_t)drvHdcSessionConnect, (stub_fn_t)stub_session_connect_hdc, 1);
    mocker((stub_fn_t)drvHdcSetSessionReference, 1, 0);
    mocker((stub_fn_t)halHdcRecv, 10, 0);
    int ret = RaHdcInit(&offline_hdc_config, p_ra_sign);
    EXPECT_INT_EQ(ret, 0);
}

void tc_hdc_env_deinit()
{
    struct RaInitConfig offline_hdc_config = {
        .phyId = 0,
        .nicPosition = NETWORK_OFFLINE,
        .hdcType = HDC_SERVICE_TYPE_RDMA,
        .enableHdcAsync = false,
    };

    mocker((stub_fn_t)halHdcRecv, 10, 0);
    mocker((stub_fn_t)drvHdcSessionClose, 1, 0);
    mocker((stub_fn_t)drvHdcClientDestroy, 1, 0);
    int ret = RaHdcDeinit(&offline_hdc_config);
    EXPECT_INT_EQ(ret, 0);
	mocker_clean();
}

int ra_hdc_get_lite_support_stub(struct RaRdmaHandle *rdma_handle, unsigned int phyId)
{
    rdma_handle->supportLite = 1;
    return 0;
}

void tc_host_abnormal_qp_mode_test()
{
    int ret;
    struct rdev rdev_info = {0};
    rdev_info.family = AF_INET;
    struct RaRdmaHandle *rdma_handle = NULL;
    void* qp_handle;
    RaRdevInit(NETWORK_OFFLINE, NOTIFY, rdev_info, &rdma_handle);

    ret = RaQpCreate(rdma_handle, 0, 3, &qp_handle);
    EXPECT_INT_NE(0, ret);
}

extern void RaHwInit(void *arg);

extern int HdcSendRecvPktRecvCheck(int rcvBufLen, unsigned int outDataLen, struct MsgHead *recvMsgHead,
    struct drvHdcMsg *pMsgRcv);
void tc_hdc_send_recv_pkt_recv_check()
{

}

void tc_ra_peer_socket_white_list_add_01()
{
    struct rdev rdev_info = {0};
    struct SocketWlistInfoT white_list[4] = {0};
    RaPeerSocketWhiteListAdd(rdev_info, white_list, 1);
}

void tc_ra_peer_socket_white_list_add_02()
{
    struct rdev rdev_info = {0};
    mocker(RsSocketWhiteListAdd, 20,1);
    struct SocketWlistInfoT white_list[4] = {0};
    RaPeerSocketWhiteListAdd(rdev_info, white_list, 1);
    mocker_clean();
}

void tc_ra_peer_socket_white_list_del()
{
    struct rdev rdev_info = {0};
    struct SocketWlistInfoT white_list[5] = {0};
    mocker(RsSocketWhiteListDel, 20, 1);
    RaPeerSocketWhiteListDel(rdev_info, white_list, 5);
    mocker_clean();
}

void tc_ra_peer_rdev_init_01()
{
	int ret;
    struct rdev rdev_info = {0};
    struct RaRdmaHandle rdma_handle;
    unsigned int *rdevIndex = (unsigned int *)malloc(sizeof(unsigned int));
	mocker(HostNotifyBaseAddrInit, 1, 0);
	mocker(RsRdevInit, 1, 0);
    ret = RaPeerRdevInit(&rdma_handle, NOTIFY, rdev_info, rdevIndex);
	mocker_clean();
    free(rdevIndex);
	EXPECT_INT_EQ(0, ret);
}

void tc_ra_peer_rdev_init_02()
{
	int ret;
    struct rdev rdev_info = {0};
    struct RaRdmaHandle rdma_handle;
    unsigned int *rdevIndex = (unsigned int *)malloc(sizeof(unsigned int));
	mocker(HostNotifyBaseAddrInit, 1, 1);
    ret = RaPeerRdevInit(&rdma_handle, NOTIFY, rdev_info, rdevIndex);
	mocker_clean();
    free(rdevIndex);
	EXPECT_INT_EQ(1, ret);
}

void tc_ra_peer_rdev_init_03()
{
	int ret;
    struct rdev rdev_info = {0};
    struct RaRdmaHandle rdma_handle;
    unsigned int *rdevIndex = (unsigned int *)malloc(sizeof(unsigned int));
	mocker(HostNotifyBaseAddrInit, 1, 0);
	mocker(RsRdevInit, 1, 1);
	mocker(HostNotifyBaseAddrUninit, 1, 0);
    ret = RaPeerRdevInit(&rdma_handle, NOTIFY, rdev_info, rdevIndex);
	mocker_clean();
    free(rdevIndex);
	EXPECT_INT_EQ(1, ret);
}

void tc_ra_peer_rdev_init_04()
{
	int ret;
    struct rdev rdev_info = {0};
    struct RaRdmaHandle rdma_handle;
    unsigned int *rdevIndex = (unsigned int *)malloc(sizeof(unsigned int));
	mocker(HostNotifyBaseAddrInit, 1, 0);
	mocker(RsRdevInit, 1, 1);
	mocker(HostNotifyBaseAddrUninit, 1, 2);
    ret = RaPeerRdevInit(&rdma_handle, NOTIFY, rdev_info, rdevIndex);
	mocker_clean();
    free(rdevIndex);
	EXPECT_INT_EQ(2, ret);
}

void tc_ra_peer_rdev_deinit_01()
{
	int ret;
    struct RaRdmaHandle *rdma_handle = (struct RaRdmaHandle *)malloc(sizeof(struct RaRdmaHandle));
	rdma_handle->rdevInfo.phyId = 0;
	mocker(RsRdevDeinit, 1, 0);
	mocker(HostNotifyBaseAddrUninit, 1, 0);
    ret = RaPeerRdevDeinit(rdma_handle, NOTIFY);
	mocker_clean();
    free(rdma_handle);
    rdma_handle = NULL;
	EXPECT_INT_EQ(0, ret);
}

void tc_ra_peer_rdev_deinit_02()
{
	int ret;
    struct RaRdmaHandle *rdma_handle = (struct RaRdmaHandle *)malloc(sizeof(struct RaRdmaHandle));
	rdma_handle->rdevInfo.phyId = 0;
	mocker(RsRdevDeinit, 1, 1);
    ret = RaPeerRdevDeinit(rdma_handle, NOTIFY);
	mocker_clean();
    free(rdma_handle);
    rdma_handle = NULL;
	EXPECT_INT_EQ(1, ret);
}

void tc_ra_peer_rdev_deinit_03()
{
	int ret;
    struct RaRdmaHandle *rdma_handle = (struct RaRdmaHandle *)malloc(sizeof(struct RaRdmaHandle));
	rdma_handle->rdevInfo.phyId = 0;
	mocker(RsRdevDeinit, 1, 0);
	mocker(HostNotifyBaseAddrUninit, 1, 2);
    ret = RaPeerRdevDeinit(rdma_handle, NOTIFY);
	mocker_clean();
    free(rdma_handle);
    rdma_handle = NULL;
	EXPECT_INT_EQ(2, ret);
}

void tc_ra_peer_socket_batch_connect()
{
    unsigned int dev_id;
    struct SocketConnectInfoT conn[4] = {0};
    mocker(RaGetSocketConnectInfo, 20, 1);
    RaPeerSocketBatchConnect(dev_id, conn, 5);
    mocker_clean();
}

void tc_ra_peer_socket_batch_abort()
{
    unsigned int dev_id;
    struct SocketConnectInfoT conn[4] = {0};
    int ret = 0;

    mocker(RaGetSocketConnectInfo, 20, 1);
    ret = RaPeerSocketBatchAbort(dev_id, conn, 5);
    EXPECT_INT_NE(ret, 0);
    mocker_clean();

    mocker(RaGetSocketConnectInfo, 20, 0);
    mocker(pthread_mutex_lock, 10, 0);
    mocker(pthread_mutex_unlock, 10, 0);
    mocker(RsSocketBatchAbort, 10, 1);
    ret = RaPeerSocketBatchAbort(dev_id, conn, 5);
    EXPECT_INT_EQ(ret, 1);
    mocker_clean();

    mocker(RaGetSocketConnectInfo, 20, 0);
    mocker(pthread_mutex_lock, 10, 0);
    mocker(pthread_mutex_unlock, 10, 0);
    mocker(RsSocketBatchAbort, 10, 0);
    ret = RaPeerSocketBatchAbort(dev_id, conn, 5);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

void tc_ra_peer_socket_listen_start_01()
{
    unsigned int dev_id;
    struct SocketListenInfoT conn[5] = {0};
    mocker(RaGetSocketListenInfo, 10, 1);
    RaPeerSocketListenStart(dev_id, conn, 5);
    mocker_clean();
}

void tc_ra_peer_socket_listen_start_02()
{
    unsigned int dev_id;
    struct SocketListenInfoT conn[5] = {0};
    mocker(RaGetSocketListenInfo, 10, 0);
    mocker(RsSocketListenStart, 10, 0);
    mocker(RaGetSocketListenResult, 10, 1);
    mocker_clean();
}

void tc_ra_peer_socket_listen_stop()
{
    unsigned int dev_id;
    struct SocketListenInfoT conn[5] = {0};
    mocker(RaGetSocketListenInfo, 10, 1);
    RaPeerSocketListenStop(dev_id, conn, 5);
    mocker_clean();
}

void tc_ra_peer_set_rs_conn_param()
{
    struct SocketInfoT  conn[6] = {0};
    struct SocketFdData  rs_conn[5] = {0};
    RaPeerSetRsConnParam(conn, 6, rs_conn, 5);
}

void tc_ra_inet_pton_01()
{
    char net_addr[5] = {0};
    union HccpIpAddr ip;
    RaInetPton(0, ip, net_addr, 32);
}

void tc_ra_inet_pton_02()
{
    char net_addr[5] = {0};
    union HccpIpAddr ip;
    RaInetPton(2, ip, net_addr, 0);
}

void tc_ra_socket_init()
{
    struct rdev rdev_info = {0};
    void* socket_handle = NULL;

    rdev_info.phyId = 0;
    rdev_info.family = AF_INET;
    rdev_info.localIp.addr.s_addr = 0;
    RaSocketInit(NETWORK_OFFLINE, rdev_info, &socket_handle);
}

void tc_ra_socket_init_v1()
{
    struct SocketInitInfoT socket_init = {0};
    void* socket_handle = NULL;

    socket_init.scopeId = 0;
    socket_init.rdevInfo.phyId = 0;
    socket_init.rdevInfo.family = AF_INET;
    socket_init.rdevInfo.localIp.addr.s_addr = 0;
    RaSocketInitV1(NETWORK_OFFLINE, socket_init, &socket_handle);

    socket_init.scopeId = 0;
    socket_init.rdevInfo.phyId = 0;
    socket_init.rdevInfo.family = AF_INET6;
    socket_init.rdevInfo.localIp.addr.s_addr = 0;
    RaSocketInitV1(NETWORK_PEER_ONLINE, socket_init, &socket_handle);
    RaSocketDeinit(socket_handle);

    RaSocketInitV1(NETWORK_ONLINE, socket_init, &socket_handle);

    mocker(calloc, 1, NULL);
    RaSocketInitV1(NETWORK_PEER_ONLINE, socket_init, &socket_handle);
    mocker_clean();

    mocker(RaInetPton, 1, 99);
    RaSocketInitV1(NETWORK_PEER_ONLINE, socket_init, &socket_handle);
    mocker_clean();

    mocker(memcpy_s, 1, 1);
    RaSocketInitV1(NETWORK_PEER_ONLINE, socket_init, &socket_handle);
    mocker_clean();

    RaSocketInitV1(NETWORK_PEER_ONLINE, socket_init, NULL);
}

void tc_ra_send_wrlist()
{
    struct RaQpHandle qp_handle;
    struct RaRdmaOps rdma_ops;
    qp_handle.rdmaOps = &rdma_ops;
    unsigned int send_num = 1;
    unsigned int complete_num = 0;
    struct SendWrlistData wrlist[1];
    struct SendWrRsp op_rsp[1];
    RaSendWrlist(NULL, NULL, NULL, send_num, &complete_num);
    qp_handle.rdmaOps = NULL;
    RaSendWrlist(&qp_handle, wrlist, op_rsp, send_num, &complete_num);
    wrlist[0].memList.len = 2147483649;
    RaSendWrlist(&qp_handle, wrlist, op_rsp, send_num, &complete_num);
}

void tc_ra_rdev_init()
{
    struct rdev rdev_info;
    void* rdma_handle = NULL;
    rdev_info.phyId = 0;
    RaRdevInit(2, NOTIFY, rdev_info, &rdma_handle);
}

void tc_ra_rdev_get_port_status()
{
    enum PortStatus status = PORT_STATUS_DOWN;
    struct RaRdmaHandle rdma_handle = { 0 };
    struct RaRdmaOps ops = {0};
    int ret;

    ret = RaRdevGetPortStatus(NULL, NULL);
    EXPECT_INT_NE(0, ret);

    rdma_handle.rdevInfo.phyId = 100000;
    ret = RaRdevGetPortStatus(&rdma_handle, &status);
    EXPECT_INT_NE(0, ret);

    rdma_handle.rdevInfo.phyId = 0;
    ret = RaRdevGetPortStatus(&rdma_handle, &status);
    EXPECT_INT_NE(0, ret);

    ops.raRdevGetPortStatus = RaHdcRdevGetPortStatus;
    rdma_handle.rdmaOps = &ops;
    mocker(RaHdcProcessMsg, 5, -1);
    ret = RaRdevGetPortStatus(&rdma_handle, &status);
    EXPECT_INT_NE(0, ret);
    mocker_clean();

    mocker(RaHdcProcessMsg, 5, 0);
    ret = RaRdevGetPortStatus(&rdma_handle, &status);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();

    int out_len;
    int op_result;
    int rcv_buf_len = 300;

    char in_buf[512];
    char out_buf[512];

    ret = RaRsRdevGetPortStatus(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(ret, 0);
}

void tc_ra_hdc_rdev_deinit()
{
    struct RaRdmaHandle rdma_handle = { 0 };
    mocker(calloc, 10 , NULL);
    mocker(rdma_lite_free_context, 10 , 0);
    int ret = RaHdcRdevDeinit(&rdma_handle, NOTIFY);
    EXPECT_INT_EQ(-ENOMEM, ret);
    mocker_clean();

    mocker(HdcSendRecvPkt, 20, 0);
    mocker(MsgHeadCheck, 20, 1);
    mocker(rdma_lite_free_context, 10 , 0);
    ret = RaHdcRdevDeinit(&rdma_handle, NOTIFY);
    mocker_clean();
}

void tc_ra_hdc_socket_white_list_add()
{
    struct rdev rdev_info = {};
    struct SocketWlistInfoT white_list[1];
    mocker(HdcSendRecvPkt, 20, 1);
    RaHdcSocketWhiteListAdd(rdev_info, white_list, 1);
    mocker_clean();

    mocker(HdcSendRecvPkt, 20, 0);
    mocker(MsgHeadCheck, 20, 1);
    RaHdcSocketWhiteListAdd(rdev_info, white_list, 1);
    mocker_clean();
}

void tc_ra_hdc_socket_white_list_del()
{
    struct rdev rdev_info;
    struct SocketWlistInfoT white_list[1];
    int ret;
    mocker(HdcSendRecvPkt, 20, 1);
    ret = RaHdcSocketWhiteListDel(rdev_info, white_list, 1);
    EXPECT_INT_EQ(1, ret);
    mocker_clean();

    mocker(HdcSendRecvPkt, 20, 0);
    mocker(MsgHeadCheck, 20, 1);
    ret = RaHdcSocketWhiteListDel(rdev_info, white_list, 1);
    EXPECT_INT_EQ(1, ret);
    mocker_clean();
}

void tc_ra_hdc_socket_accept_credit_add()
{
    struct SocketListenInfoT conn[1];
    int ret;
    mocker(RaGetSocketListenInfo, 1, -1);
    ret = RaHdcSocketAcceptCreditAdd(1, conn, 1, 1);
    EXPECT_INT_EQ(1, 1);
    mocker_clean();

    mocker(RaHdcProcessMsg, 1, -1);
    ret = RaHdcSocketAcceptCreditAdd(1, conn, 1, 1);
    EXPECT_INT_EQ(1, 1);
    mocker_clean();
}

void tc_ra_hdc_rdev_init()
{
    struct rdev rdev_info = {0};
    unsigned int rdevIndex;
    int ret;
    struct RaRdmaHandle rdma_handle = { 0 };
    mocker(DlDrvDeviceGetIndexByPhyId, 20, 0);
    mocker(DlHalNotifyGetInfo, 20, 0);
    mocker(DlHalMemAlloc, 20, 0);
    mocker(calloc, 20, NULL);
    ret = RaHdcRdevInit(&rdma_handle, NOTIFY, rdev_info, &rdevIndex);
    EXPECT_INT_EQ(-ENOMEM, ret);
    mocker_clean();

    mocker(memcpy_s, 20, 1);
    mocker(DlDrvDeviceGetIndexByPhyId, 20, 0);
    mocker(DlHalNotifyGetInfo, 20, 0);
    mocker(DlHalMemAlloc, 20, 0);
    ret = RaHdcRdevInit(&rdma_handle, NOTIFY, rdev_info, &rdevIndex);
    EXPECT_INT_EQ(-ESAFEFUNC, ret);
    mocker_clean();

    mocker(HdcSendRecvPkt, 20, 0);
    mocker(MsgHeadCheck, 20, 1);
    mocker(DlDrvDeviceGetIndexByPhyId, 20, 0);
    mocker(DlHalNotifyGetInfo, 20, 0);
    mocker(DlHalMemAlloc, 20, 0);
    ret = RaHdcRdevInit(&rdma_handle, NOTIFY, rdev_info, &rdevIndex);
    EXPECT_INT_EQ(1, ret);
    mocker_clean();
}

void tc_ra_hdc_init_apart()
{
    unsigned int phyId;
    mocker(DlDrvDeviceGetIndexByPhyId, 20, 0);
    mocker(pthread_mutex_init, 20, 1);
    int ret = RaHdcInitApart(1, &phyId);
    EXPECT_INT_EQ(-ESYSFUNC, ret);
    mocker_clean();
}

void tc_ra_hdc_qp_destroy()
{
    struct RaQpHandle *qp_hdc = (struct RaQpHandle *)malloc(sizeof(struct RaQpHandle));
    *qp_hdc = (struct RaQpHandle){0};
    mocker(HdcSendRecvPkt, 20, 1);
    mocker(rdma_lite_destroy_qp, 20, 0);
    mocker(rdma_lite_destroy_cq, 20, 0);
    int ret = RaHdcQpDestroy(qp_hdc);
    EXPECT_INT_EQ(1, ret);
    mocker_clean();
}
void tc_ra_hdc_qp_destroy_01()
{
    struct RaQpHandle *qp_hdc = (struct RaQpHandle *)malloc(sizeof(struct RaQpHandle));
    *qp_hdc = (struct RaQpHandle){0};
    mocker(HdcSendRecvPkt, 20, 0);
    mocker(MsgHeadCheck, 20, 1);
    mocker(rdma_lite_destroy_qp, 20, 0);
    mocker(rdma_lite_destroy_cq, 20, 0);
    int ret = RaHdcQpDestroy(qp_hdc);
    EXPECT_INT_EQ(1, ret);
    mocker_clean();
}

void tc_ra_get_socket_connect_info()
{
    int ret = RaGetSocketConnectInfo(NULL, 1, NULL, 1);
    EXPECT_INT_EQ(-EINVAL, ret);
}

void tc_ra_get_socket_listen_info()
{
    int ret = RaGetSocketListenInfo(NULL, 1, NULL, 1);
    EXPECT_INT_EQ(-EINVAL, ret);
}

void tc_ra_get_socket_listen_result()
{
    int ret = RaGetSocketListenResult(NULL, 1, NULL, 1);
    EXPECT_INT_EQ(-EINVAL, ret);
}

void tc_ra_hw_hdc_init() {
    mocker((stub_fn_t)pthread_detach, 1, 0);
    mocker((stub_fn_t)pthread_create, 1, -1);
    RaHwHdcInit(NULL);
    mocker_clean();
}

void tc_ra_peer_init_fail_001()
{
	struct RaInitConfig cfg = {0};
	unsigned int white_list_status = 0;

	mocker(pthread_mutex_init, 1, 1);
    int ret = RaPeerInit(&cfg, white_list_status);
	mocker_clean();
    EXPECT_INT_EQ(-ESYSFUNC, ret);
}

void tc_ra_peer_socket_deinit_001()
{
	struct rdev rdev_info = {0};

	mocker(RsSocketDeinit, 1, 0);
    int ret = RaPeerSocketDeinit(rdev_info);
	mocker_clean();
    EXPECT_INT_EQ(0, ret);
}

void tc_host_notify_base_addr_init()
{
	int ret;

	mocker(drvDeviceGetIndexByPhyId, 1, 0);
	mocker(halNotifyGetInfo, 1, 0);
	mocker(open, 1, 1);
	mocker(mmap, 1, 1);
	mocker(RsNotifyCfgSet, 1, 0);
    ret = HostNotifyBaseAddrInit(0);
	mocker_clean();
    EXPECT_INT_EQ(0, ret);
}

void tc_host_notify_base_addr_init_001()
{
	int ret;

	mocker(drvDeviceGetIndexByPhyId, 1, 1);
    ret = HostNotifyBaseAddrInit(0);
	mocker_clean();
    EXPECT_INT_EQ(1, ret);
}

void tc_host_notify_base_addr_init_002()
{
	int ret;

	mocker(drvDeviceGetIndexByPhyId, 1, 0);
	mocker(halNotifyGetInfo, 1, 2);
	mocker(RsNotifyCfgSet, 1, 0);
    ret = HostNotifyBaseAddrInit(0);
	mocker_clean();
    EXPECT_INT_EQ(2, ret);
}

void tc_host_notify_base_addr_init_003()
{
	int ret;

	mocker(drvDeviceGetIndexByPhyId, 1, 0);
	mocker(halNotifyGetInfo, 1, 0);
	mocker(open, 1, -1);
	mocker(mmap, 1, MAP_FAILED);
	ret = HostNotifyBaseAddrInit(0);
	EXPECT_INT_EQ(-ENOENT, ret);
	mocker_clean();
}

void tc_host_notify_base_addr_init_005()
{
	int ret;

	mocker(drvDeviceGetIndexByPhyId, 1, 0);
	mocker(halNotifyGetInfo, 1, 0);
	mocker(open, 1, 1);
	mocker(mmap, 1, 1);
	mocker(RsNotifyCfgSet, 1, 4);
	mocker(munmap, 1, 1);
	mocker(close, 1, 0);
	ret = HostNotifyBaseAddrInit(0);
	mocker_clean();
}

void tc_host_notify_base_addr_init_006()
{
	int ret;

	mocker(drvDeviceGetIndexByPhyId, 1, 0);
	mocker(halNotifyGetInfo, 1, 0);
	mocker(open, 1, 1);
	mocker(mmap, 1, 1);
	mocker(RsNotifyCfgSet, 1, 4);
	mocker(munmap, 1, 0);
    mocker(close, 1, 0);
	ret = HostNotifyBaseAddrInit(0);
	mocker_clean();
	EXPECT_INT_EQ(4, ret);
}

void *stub_mmap(void *addr, size_t length, int prot, int flags,
                  int fd, off_t offset)
{
	errno = 1;
	return (void*)-1;
};

void tc_host_notify_base_addr_init_007()
{
	int ret;

    mocker_clean();
	mocker(drvDeviceGetIndexByPhyId, 1, 0);
	mocker(halNotifyGetInfo, 1, 0);
	mocker(open, 1, 1);
	mocker_u64_invoke(mmap, stub_mmap, 20);
	ret = HostNotifyBaseAddrInit(0);
	EXPECT_INT_EQ(-ENOMEM, ret);
	mocker_clean();
}

void tc_host_notify_base_addr_uninit()
{
	int ret;

	mocker(RsNotifyCfgGet, 1, 0);
	mocker(open, 1, 0);
	mocker(ioctl, 1, 0);
	mocker(munmap, 1, 0);
	ret = HostNotifyBaseAddrUninit(0);
	mocker_clean();
	EXPECT_INT_NE(0, ret);
}

void tc_host_notify_base_addr_uninit_001()
{
	int ret;

	mocker(RsNotifyCfgGet, 1, 1);
    ret = HostNotifyBaseAddrUninit(0);
	mocker_clean();
    EXPECT_INT_EQ(1, ret);
}

void tc_host_notify_base_addr_uninit_002()
{
	int ret;

	mocker(RsNotifyCfgGet, 1, 0);
	mocker(open, 1, -1);
mocker(drvDeviceGetIndexByPhyId, 1, 1);
    ret = HostNotifyBaseAddrUninit(0);
	mocker_clean();
    EXPECT_INT_EQ(1, ret);
}

void tc_host_notify_base_addr_uninit_003()
{
	int ret;

	mocker(RsNotifyCfgGet, 1, 0);
	mocker(open, 1, 0);
	mocker(ioctl, 1, -1);
    ret = HostNotifyBaseAddrUninit(0);
	mocker_clean();
    EXPECT_INT_EQ(-ENOENT, ret);
}

void tc_host_notify_base_addr_uninit_004()
{
	int ret;

	mocker(RsNotifyCfgGet, 1, 0);
	mocker(open, 1, 0);
	mocker(ioctl, 1, 0);
	mocker(munmap, 1, 3);
    ret = HostNotifyBaseAddrUninit(0);
	mocker_clean();
    EXPECT_INT_EQ(-ENOENT, ret);
}

void tc_host_notify_base_addr_uninit_005()
{
	int ret;
	gNotifyFd = 1;
	mocker(RsNotifyCfgGet, 10, 0);
	mocker(open, 10, 1);
	mocker(ioctl, 10, 0);
    mocker(munmap, 1, 1);
    mocker(close, 1, 0);
    ret = HostNotifyBaseAddrUninit(0);
    EXPECT_INT_NE(0, ret);
	mocker_clean();
}

void tc_ra_peer_send_wrlist()
{
	int ret;
	struct RaQpHandle qp_handle = {0};
	struct SendWrlistData wr = {0};
	struct SendWrRsp op_rsp = {0};
	struct WrlistSendCompleteNum wrlist_num = {0};

	wrlist_num.sendNum = 1;
	mocker(RsSendWrlist, 1, 0);
	ret = RaPeerSendWrlist(&qp_handle, &wr, &op_rsp, wrlist_num);
	mocker_clean();
}

void tc_ra_peer_send_wrlist_001()
{
	int ret;
	struct RaQpHandle qp_handle = {0};
	struct SendWrlistData wr = {0};
	struct SendWrRsp op_rsp = {0};
	struct WrlistSendCompleteNum wrlist_num = {0};

	wrlist_num.sendNum = 1;
	mocker(RsSendWrlist, 1, -1);
	ret = RaPeerSendWrlist(&qp_handle, &wr, &op_rsp, wrlist_num);
	mocker_clean();
	EXPECT_INT_EQ(-1, ret);
}

void tc_ra_get_qp_context()
{
    struct RaQpHandle RaQpHandle;
    void *qp_handle = (void *)&RaQpHandle;
    void *qp = NULL;
    void *send_cq= NULL;
    void *recv_cq = NULL;
    struct RaRdmaOps ops;
    RaQpHandle.rdmaOps = NULL;
    RaGetQpContext(qp_handle, &qp, &send_cq, &recv_cq);
    RaGetQpContext(NULL, &qp, &send_cq, &recv_cq);
    ops.raGetQpContext = RaPeerGetQpContext;
    RaQpHandle.rdmaOps = &ops;
    RaGetQpContext(qp_handle, &qp, &send_cq, &recv_cq);
}

void tc_ra_create_cq()
{
    struct ibv_cq *ibSendCq;
    struct ibv_cq *ibRecvCq;
    void* context;
    struct CqAttr attr;
    attr.qpContext = &context;
    attr.ibSendCq = &ibSendCq;
    attr.ibRecvCq = &ibRecvCq;
    attr.sendCqDepth = 16384;
    attr.recvCqDepth = 16384;
    attr.sendCqEventId = 1;
    attr.recvCqEventId = 2;

    struct RaRdmaHandle RaRdmaHandle;
    void *rdma_handle = (void *)&RaRdmaHandle;
    RaRdmaHandle.rdevIndex = 0;
    RaRdmaHandle.rdevInfo.phyId = 32767;
    RaRdmaHandle.rdmaOps = NULL;
    RaCqCreate(rdma_handle, &attr);
    RaCqDestroy(rdma_handle, &attr);

    struct RaRdmaOps ops;
    ops.raCqCreate = RaPeerCqCreate;
    ops.raCqDestroy = RaPeerCqDestroy;
    RaRdmaHandle.rdmaOps = &ops;
    RaCqCreate(rdma_handle, &attr);
    RaCqDestroy(rdma_handle, &attr);

    RaRdmaHandle.rdevInfo.phyId = 0;
    RaCqCreate(rdma_handle, &attr);
    RaCqDestroy(rdma_handle, &attr);

    mocker((stub_fn_t)RaPeerCqCreate, 1, 0);
    mocker((stub_fn_t)RaPeerCqDestroy, 1, 0);
    RaCqCreate(rdma_handle, &attr);
    RaCqDestroy(rdma_handle, &attr);
    mocker_clean();
}

void tc_ra_create_notmal_qp()
{
    struct ibv_cq *ib_send_cq;
    struct ibv_cq *ib_recv_cq;
    void* context;
    struct ibv_qp_init_attr qp_init_attr;
    qp_init_attr.qp_context = context;
    qp_init_attr.send_cq = ib_send_cq;
    qp_init_attr.recv_cq = ib_recv_cq;
    qp_init_attr.qp_type = 2;
    qp_init_attr.cap.max_inline_data = 32;
    qp_init_attr.cap.max_send_wr = 4096;
    qp_init_attr.cap.max_send_sge = 4096;
    qp_init_attr.cap.max_recv_wr = 4096;
    qp_init_attr.cap.max_recv_sge = 1;
	struct ibv_qp* qp;
    struct RaQpHandle RaQpHandle;
    void *qp_handle = &RaQpHandle;

    struct RaRdmaHandle RaRdmaHandle;
    void *rdma_handle = (void *)&RaRdmaHandle;
    RaRdmaHandle.rdevIndex = 0;
    RaRdmaHandle.rdevInfo.phyId = 32767;
    RaRdmaHandle.rdmaOps = NULL;
    struct RaRdmaOps ops;
    RaRdmaHandle.rdmaOps = &ops;
    ops.raNormalQpCreate = NULL;
    ops.raNormalQpDestroy = NULL;
    RaNormalQpCreate(rdma_handle, &qp_init_attr, &qp_handle, &qp);
    RaQpHandle.rdmaOps = NULL;
    RaNormalQpDestroy(qp_handle);

    ops.raNormalQpCreate = RaPeerNormalQpCreate;
    ops.raNormalQpDestroy = RaPeerNormalQpDestroy;

    mocker((stub_fn_t)RaPeerNormalQpCreate, 10, 0);
    mocker((stub_fn_t)RaPeerNormalQpDestroy, 10, 0);
    RaNormalQpCreate(rdma_handle, &qp_init_attr, &qp_handle, &qp);
    RaNormalQpDestroy(qp_handle);

    RaNormalQpCreate(rdma_handle, &qp_init_attr, NULL, &qp);
    RaNormalQpDestroy(NULL);

    RaRdmaHandle.rdevInfo.phyId = 0;
    RaNormalQpCreate(rdma_handle, &qp_init_attr, &qp_handle, &qp);
    RaNormalQpDestroy(qp_handle);
    mocker_clean();

    mocker((stub_fn_t)RaPeerNormalQpCreate, 10, -1);
    mocker((stub_fn_t)RaPeerNormalQpDestroy, 10, -1);
    RaNormalQpCreate(rdma_handle, &qp_init_attr, &qp_handle, &qp);
    RaQpHandle.rdmaOps = &ops;
    RaNormalQpDestroy(qp_handle);
    mocker_clean();
}

void tc_ra_create_comp_channel()
{
    struct RaRdmaHandle RaRdmaHandle;
    void *rdma_handle = (void *)&RaRdmaHandle;
    RaRdmaHandle.rdevIndex = 0;
    RaRdmaHandle.rdevInfo.phyId = 32767;
    RaRdmaHandle.rdmaOps = NULL;

    void *comp_channel = NULL;
    RaCreateCompChannel(rdma_handle, &comp_channel);
    RaDestroyCompChannel(rdma_handle, comp_channel);

    comp_channel = (void *)0xabcd;
    RaCreateCompChannel(rdma_handle, &comp_channel);
    RaDestroyCompChannel(rdma_handle, comp_channel);

    struct RaRdmaOps ops;
    ops.raCreateCompChannel = RaPeerCreateCompChannel;
    ops.raDestroyCompChannel = RaPeerDestroyCompChannel;
    RaRdmaHandle.rdmaOps = &ops;
    RaCreateCompChannel(rdma_handle, &comp_channel);
    RaDestroyCompChannel(rdma_handle, comp_channel);

    RaCreateCompChannel(rdma_handle, NULL);
    RaDestroyCompChannel(rdma_handle, NULL);
    RaCreateCompChannel(NULL, NULL);
    RaDestroyCompChannel(NULL, NULL);

    RaRdmaHandle.rdevInfo.phyId = 0;
    RaCreateCompChannel(rdma_handle, &comp_channel);
    RaDestroyCompChannel(rdma_handle, comp_channel);
}

void tc_ra_get_cqe_err_info()
{
    int ret;
    struct CqeErrInfo info = {0};

    ret = RaGetCqeErrInfo(0, NULL);
    EXPECT_INT_EQ(128103, ret);

    mocker(RaHdcGetCqeErrInfo, 1, 0);
    ret = RaGetCqeErrInfo(0, &info);
    EXPECT_INT_EQ(0, ret);

    ret = RaGetCqeErrInfo(128, &info);
    EXPECT_INT_NE(0, ret);
    return;
}

void tc_ra_rdev_get_cqe_err_info_list()
{
    struct RaRdmaHandle ra_rdma_handle;
    struct CqeErrInfo info[128] = {0};
    unsigned int num = 128;
    int ret;

    ra_rdma_handle.rdevIndex = 0;
    ra_rdma_handle.rdevInfo.phyId = 32767;
    ra_rdma_handle.rdmaOps = NULL;

    mocker(RaHdcGetCqeErrInfoList, 10, 0);
    ret = RaRdevGetCqeErrInfoList((void *)&ra_rdma_handle, info, &num);
    EXPECT_INT_EQ(0, ret);

    ret = RaRdevGetCqeErrInfoList((void *)&ra_rdma_handle, info, NULL);
    EXPECT_INT_EQ(128103, ret);

    num = 129;
    ret = RaRdevGetCqeErrInfoList((void *)&ra_rdma_handle, info, &num);
    EXPECT_INT_EQ(128303, ret);
    mocker_clean();

    return;
}

void tc_ra_rs_get_ifnum()
{
    int ret;
    int out_len;
    int op_result;
    int rcv_buf_len = 300;

    char in_buf[512];
    char out_buf[512];

    ret = RaRsGetIfnum(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(0, ret);

    return;
}

void tc_ra_create_srq()
{
    struct RaRdmaHandle RaRdmaHandle;
    void *rdma_handle = (void *)&RaRdmaHandle;
    RaRdmaHandle.rdevIndex = 0;
    RaRdmaHandle.rdevInfo.phyId = 32767;
    RaRdmaHandle.rdmaOps = NULL;
    struct SrqAttr attr = {0};

    RaCreateSrq(rdma_handle, NULL);
    RaDestroySrq(rdma_handle, NULL);

    RaCreateSrq(rdma_handle, &attr);
    RaDestroySrq(rdma_handle, &attr);

    struct RaRdmaOps ops;
    ops.raCreateSrq = RaPeerCreateSrq;
    ops.raDestroySrq = RaPeerDestroySrq;
    RaRdmaHandle.rdmaOps = &ops;
    RaCreateSrq(rdma_handle, &attr);
    RaDestroySrq(rdma_handle, &attr);

    RaCreateSrq(NULL, NULL);
    RaDestroySrq(NULL, NULL);

    RaRdmaHandle.rdevInfo.phyId = 0;
    RaCreateSrq(rdma_handle, &attr);
    RaDestroySrq(rdma_handle, &attr);
}

void tc_ra_rs_socket_port_is_use()
{
    unsigned int size = sizeof(union OpSocketConnectData) + sizeof(struct MsgHead);
    union OpSocketConnectData socket_connect_data = {{0}};
    unsigned int port = 0x16;

    socket_connect_data.txData.conn[0].port = port;
    socket_connect_data.txData.num = 1;

    char* in_buf = calloc(1, size);
    char* out_buf = calloc(1, size);
    int out_len;
    int op_result;

    memcpy(in_buf + sizeof(struct MsgHead), &socket_connect_data, sizeof(union OpSocketConnectData));
    memcpy(out_buf + sizeof(struct MsgHead), &socket_connect_data, sizeof(union OpSocketConnectData));
    RaRsSocketBatchConnect(in_buf, out_buf, &out_len, &op_result, size);

    socket_connect_data.txData.num = 1U | (1U << 31U);
    socket_connect_data.txData.conn[0].port = 0xFFFFFFFF;
    memcpy(in_buf + sizeof(struct MsgHead), &socket_connect_data, sizeof(union OpSocketConnectData));
    memcpy(out_buf + sizeof(struct MsgHead), &socket_connect_data, sizeof(union OpSocketConnectData));
    RaRsSocketBatchConnect(in_buf, out_buf, &out_len, &op_result, size);

    free(in_buf);
    free(out_buf);
    in_buf = NULL;
    out_buf = NULL;

    size = sizeof(union OpSocketListenData) + sizeof(struct MsgHead);
    union OpSocketListenData socket_listen_data = {{0}};
    socket_listen_data.txData.conn[0].port = port;
    socket_listen_data.txData.num = 1;

    in_buf = calloc(1, size);
    out_buf = calloc(1, size);
    memcpy(in_buf + sizeof(struct MsgHead), &socket_listen_data, sizeof(union OpSocketListenData));
    memcpy(out_buf + sizeof(struct MsgHead), &socket_listen_data, sizeof(union OpSocketListenData));
    RaRsSocketListenStart(in_buf, out_buf, &out_len, &op_result, size);
    RaRsSocketListenStop(in_buf, out_buf, &out_len, &op_result, size);

    socket_listen_data.txData.num = 1U | (1U << 31U);
    socket_listen_data.txData.conn[0].port = 0xFFFFFFFF;
    memcpy(in_buf + sizeof(struct MsgHead), &socket_listen_data, sizeof(union OpSocketListenData));
    memcpy(out_buf + sizeof(struct MsgHead), &socket_listen_data, sizeof(union OpSocketListenData));
    RaRsSocketListenStart(in_buf, out_buf, &out_len, &op_result, size);
    RaRsSocketListenStop(in_buf, out_buf, &out_len, &op_result, size);

    free(in_buf);
    free(out_buf);
    in_buf = NULL;
    out_buf = NULL;
}

void tc_ra_rs_get_vnic_ip_infos_v1()
{
    unsigned int size = sizeof(union OpGetVnicIpInfosDataV1) + sizeof(struct MsgHead);
    union OpGetVnicIpInfosDataV1 vnic_infos = {{0}};

    vnic_infos.txData.phyId = 0;
    vnic_infos.txData.type = 0;
    vnic_infos.txData.ids[0] = 3232235521;
    vnic_infos.txData.num = 1;

    char* in_buf = calloc(1, size);
    char* out_buf = calloc(1, size);
    int out_len;
    int op_result;

    memcpy(in_buf + sizeof(struct MsgHead), &vnic_infos, sizeof(union OpGetVnicIpInfosDataV1));
    memcpy(out_buf + sizeof(struct MsgHead), &vnic_infos, sizeof(union OpGetVnicIpInfosDataV1));
    RaRsGetVnicIpInfosV1(in_buf, out_buf, &out_len, &op_result, size);

    free(in_buf);
    free(out_buf);
    in_buf = NULL;
    out_buf = NULL;
}

void tc_ra_rs_get_vnic_ip_infos()
{
    unsigned int size = sizeof(union OpGetVnicIpInfosData) + sizeof(struct MsgHead);
    union OpGetVnicIpInfosData vnic_infos = {{0}};

    vnic_infos.txData.phyId = 0;
    vnic_infos.txData.type = 0;
    vnic_infos.txData.ids[0] = 3232235521;
    vnic_infos.txData.num = 1;

    char* in_buf = calloc(1, size);
    char* out_buf = calloc(1, size);
    int out_len;
    int op_result;

    memcpy(in_buf + sizeof(struct MsgHead), &vnic_infos, sizeof(union OpGetVnicIpInfosData));
    memcpy(out_buf + sizeof(struct MsgHead), &vnic_infos, sizeof(union OpGetVnicIpInfosData));
    RaRsGetVnicIpInfos(in_buf, out_buf, &out_len, &op_result, size);

    free(in_buf);
    free(out_buf);
    in_buf = NULL;
    out_buf = NULL;
}

void tc_ra_rs_typical_mr_reg()
{
    int ret;
    int out_len;
    int op_result;
    int rcv_buf_len = 300;

    char in_buf[512];
    char out_buf[512];

    ret = RaRsTypicalMrRegV1(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(ret, 0);
    ret = RaRsTypicalMrDereg(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(ret, 0);

    ret = RaRsTypicalMrReg(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(ret, 0);
    ret = RaRsTypicalMrDereg(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(ret, 0);
}

void tc_ra_rs_typical_qp_create()
{
    int ret;
    int out_len;
    int op_result;
    int rcv_buf_len = 300;

    char in_buf[512];
    char out_buf[512];

    ret = RaRsTypicalQpCreate(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(ret, 0);
    ret = RaRsTypicalQpModify(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(0, ret);
}

void tc_ra_hdc_recv_handle_send_pkt_unsuccess()
{
    mocker_clean();
    mocker(DlHalHdcRecv, 1, 1);
    mocker(DlDrvHdcAllocMsg, 1, 0);
    mocker(DlDrvHdcFreeMsg, 1, 1);
    mocker(DlDrvHdcSessionClose, 1, 1);
    mocker(RsSetCtx, 1, 0);
    mocker(pthread_mutex_lock, 1, 0);
    mocker(pthread_mutex_unlock, 1, 0);
    RaHdcRecvHandleSendPkt(0);
    mocker_clean();
}

void tc_ra_get_tls_enable()
{
    struct RaInfo info = {0};
    bool tls_enable;
    int ret;

    info.mode = NETWORK_PEER_ONLINE;
    ret = RaGetTlsEnable(&info, &tls_enable);
    EXPECT_INT_EQ(0, ret);

    info.mode = NETWORK_OFFLINE;
    mocker(RaHdcProcessMsg, 1, 0);
    ret = RaGetTlsEnable(&info, &tls_enable);
    EXPECT_INT_EQ(0, ret);

    info.phyId = RA_MAX_PHY_ID_NUM;
    ret = RaGetTlsEnable(&info, &tls_enable);
    EXPECT_INT_EQ(128303, ret);

}

void tc_ra_get_sec_random()
{
    struct RaInfo info = {0};
    unsigned int value = 0;
    int ret;

    mocker_clean();
    info.mode = NETWORK_PEER_ONLINE;
    ret = ra_get_sec_random(&info, NULL);
    EXPECT_INT_EQ(128303, ret);

    info.mode = NETWORK_OFFLINE;
    ret = ra_get_sec_random(&info, &value);
    EXPECT_INT_EQ(0, ret);

    info.mode = NETWORK_OFFLINE;
    mocker(RaHdcProcessMsg, 10, 0);
    mocker(RaPeerGetSecRandom, 10, -1);
    ret = ra_get_sec_random(&info, &value);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();
}

void tc_ra_rs_get_sec_random()
{
    unsigned int size = sizeof(union OpGetSecRandomData) + sizeof(struct MsgHead);
    union OpGetSecRandomData op_data  = {{0}};

    char* in_buf = calloc(1, size);
    char* out_buf = calloc(1, size);
    int out_len;
    int op_result;
    int ret;

    memcpy(in_buf + sizeof(struct MsgHead), &op_data , sizeof(union OpGetSecRandomData));
    memcpy(out_buf + sizeof(struct MsgHead), &op_data, sizeof(union OpGetSecRandomData));
    ret = RaRsGetSecRandom(in_buf, out_buf, &out_len, &op_result, size);
    EXPECT_INT_EQ(0, ret);

    free(in_buf);
    free(out_buf);
    in_buf = NULL;
    out_buf = NULL;
}

void tc_ra_rs_get_tls_enable()
{
    unsigned int size = sizeof(union OpGetTlsEnableData) + sizeof(struct MsgHead);
    union OpGetTlsEnableData op_data  = {{0}};

    char* in_buf = calloc(1, size);
    char* out_buf = calloc(1, size);
    int out_len;
    int op_result;
    int ret;

    memcpy(in_buf + sizeof(struct MsgHead), &op_data , sizeof(union OpGetTlsEnableData));
    memcpy(out_buf + sizeof(struct MsgHead), &op_data, sizeof(union OpGetTlsEnableData));
    ret = RaRsGetTlsEnable(in_buf, out_buf, &out_len, &op_result, size);
    EXPECT_INT_EQ(0, ret);

    free(in_buf);
    free(out_buf);
    in_buf = NULL;
    out_buf = NULL;
}

void tc_ra_get_hccn_cfg()
{
    struct RaInfo info = {0};
    char *value = calloc(1, 2048);
    unsigned int val_len = 2048;
    int ret;

    mocker_clean();
    info.mode = NETWORK_OFFLINE;
    ret = RaGetHccnCfg(NULL, HCCN_CFG_UDP_PORT_MODE, value, &val_len);
    EXPECT_INT_EQ(128303, ret);

    val_len = 1024;
    ret = RaGetHccnCfg(&info, HCCN_CFG_UDP_PORT_MODE, value, &val_len);
    EXPECT_INT_EQ(128303, ret);

    info.phyId = 64;
    info.mode = NETWORK_OFFLINE;
    val_len = 2048;
    ret = RaGetHccnCfg(&info, HCCN_CFG_UDP_PORT_MODE, value, &val_len);
    EXPECT_INT_EQ(128303, ret);

    info.phyId = 0;
    mocker(RaHdcProcessMsg, 10, 0);
    ret = RaGetHccnCfg(&info, HCCN_CFG_UDP_PORT_MODE, value, &val_len);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();
    free(value);
}

void tc_ra_rs_get_hccn_cfg()
{
    unsigned int size = sizeof(union OpGetHccnCfgData) + sizeof(struct MsgHead);
    union OpGetHccnCfgData op_data  = {{0}};

    char* in_buf = calloc(1, size);
    char* out_buf = calloc(1, size);
    int out_len;
    int op_result;
    int ret;

    memcpy(in_buf + sizeof(struct MsgHead), &op_data , sizeof(union OpGetHccnCfgData));
    memcpy(out_buf + sizeof(struct MsgHead), &op_data, sizeof(union OpGetHccnCfgData));
    ret = RaRsGetHccnCfg(in_buf, out_buf, &out_len, &op_result, size);
    EXPECT_INT_EQ(0, ret);

    free(in_buf);
    free(out_buf);
    in_buf = NULL;
    out_buf = NULL;
}

void tc_ra_save_snapshot_input()
{
    enum SaveSnapshotAction action;
    struct RaInfo *info = NULL;
    int ret;

    ret = RaSaveSnapshot(info, action);
    EXPECT_INT_NE(0, ret);

    ret = RaRestoreSnapshot(info);
    EXPECT_INT_NE(0, ret);

    info = calloc(1,sizeof(struct RaInfo));
    info->phyId = RA_MAX_PHY_ID_NUM;
    info->mode = NETWORK_PEER_ONLINE;
    ret = RaSaveSnapshot(info, action);
    EXPECT_INT_EQ(0, ret);

    ret = RaRestoreSnapshot(info);
    EXPECT_INT_EQ(0, ret);

    info->phyId = 0;
    action = SAVE_SNAPSHOT_ACTION_POST_PROCESSING + 1;
    ret = RaSaveSnapshot(info, action);
    EXPECT_INT_NE(0, ret);

    info->mode = NETWORK_PEER_ONLINE;
    info->phyId = 0;
    ret = RaSaveSnapshot(info, SAVE_SNAPSHOT_ACTION_PRE_PROCESSING);
    EXPECT_INT_EQ(0, ret);

    ret = RaRestoreSnapshot(info);
    EXPECT_INT_EQ(0, ret);

    info->mode = NETWORK_OFFLINE + 1;
    ret = RaSaveSnapshot(info, SAVE_SNAPSHOT_ACTION_PRE_PROCESSING);
    EXPECT_INT_NE(0, ret);

    ret = RaRestoreSnapshot(info);
    EXPECT_INT_NE(0, ret);

    free(info);
    info = NULL;
}

void tc_ra_save_snapshot_pre()
{
    struct RaInfo info = {0};
    struct RaRdmaHandle *rdma_handle = NULL;
    struct rdev rdev_info = {0};
    rdev_info.phyId = 0;
    rdev_info.family = AF_INET;
    rdev_info.localIp.addr.s_addr = 0;
    struct RdevInitInfo init_info = {0};
    init_info.disabledLiteThread = false;
    init_info.mode = NETWORK_OFFLINE;
    init_info.notifyType = NOTIFY;
    int ret;

    tc_hdc_env_init();

    mocker(RaRdevInitCheckIp, 10, 0);
    mocker((stub_fn_t)HdcSendRecvPkt, 10, 0);
    mocker_invoke(RaHdcGetInterfaceVersion, ra_get_interface_version_stub, 10);
    mocker_invoke(RaHdcGetLiteSupport, ra_hdc_get_lite_support_stub, 10);
    mocker(RaHdcNotifyBaseAddrInit, 10, 0);
    g_interface_version = 1;
    ret = RaRdevInitV2(init_info, rdev_info, &rdma_handle);
    EXPECT_INT_EQ(ret, 0);

    info.mode = NETWORK_OFFLINE;
    rdma_handle->supportLite = LITE_NOT_SUPPORT;
    ret = RaSaveSnapshot(&info, SAVE_SNAPSHOT_ACTION_PRE_PROCESSING);
    EXPECT_INT_EQ(0, ret);

    rdma_handle->supportLite = LITE_ALIGN_4KB;
    rdma_handle->threadStatus = LITE_THREAD_STATUS_RUNNING;
    ret = RaSaveSnapshot(&info, SAVE_SNAPSHOT_ACTION_PRE_PROCESSING);
    EXPECT_INT_EQ(128300, ret);

    ret = RaSaveSnapshot(&info, SAVE_SNAPSHOT_ACTION_PRE_PROCESSING);
    EXPECT_INT_EQ(128300, ret);

    ret = RaSaveSnapshot(&info, SAVE_SNAPSHOT_ACTION_POST_PROCESSING);
    EXPECT_INT_EQ(0, ret);

    ret = RaRdevDeinit(rdma_handle, NOTIFY);
    tc_hdc_env_deinit();
}

void tc_ra_save_snapshot_post()
{
    struct RaInfo info = {0};
    struct RaRdmaHandle *rdma_handle = NULL;
    struct rdev rdev_info = {0};
    rdev_info.phyId = 0;
    rdev_info.family = AF_INET;
    rdev_info.localIp.addr.s_addr = 0;
    struct RdevInitInfo init_info = {0};
    init_info.disabledLiteThread = false;
    init_info.mode = NETWORK_OFFLINE;
    init_info.notifyType = NOTIFY;
    int ret;

    tc_hdc_env_init();

    mocker(RaRdevInitCheckIp, 10, 0);
    mocker((stub_fn_t)HdcSendRecvPkt, 10, 0);
    mocker_invoke(RaHdcGetInterfaceVersion, ra_get_interface_version_stub, 10);
    mocker_invoke(RaHdcGetLiteSupport, ra_hdc_get_lite_support_stub, 10);
    mocker(RaHdcNotifyBaseAddrInit, 10, 0);
    g_interface_version = 1;
    ret = RaRdevInitV2(init_info, rdev_info, &rdma_handle);
    EXPECT_INT_EQ(0, ret);

    info.mode = NETWORK_OFFLINE;
    ret = RaRestoreSnapshot(&info);
    EXPECT_INT_EQ(128300, ret);

    rdma_handle->supportLite = LITE_NOT_SUPPORT;
    ret = RaRestoreSnapshot(&info);
    EXPECT_INT_EQ(0, ret);

    rdma_handle->threadStatus = LITE_THREAD_STATUS_RUNNING;
    rdma_handle->supportLite = LITE_ALIGN_4KB;
    ret = RaSaveSnapshot(&info, SAVE_SNAPSHOT_ACTION_PRE_PROCESSING);
    EXPECT_INT_EQ(0, ret);

    ret = RaSaveSnapshot(&info, SAVE_SNAPSHOT_ACTION_PRE_PROCESSING);
    EXPECT_INT_NE(0, ret);

    ret = RaSaveSnapshot(&info, SAVE_SNAPSHOT_ACTION_PRE_PROCESSING);
    EXPECT_INT_NE(0 ,ret);

    ret = RaRestoreSnapshot(&info);
    EXPECT_INT_EQ(0, ret);

    ret = RaRestoreSnapshot(&info);
    EXPECT_INT_EQ(0, ret);

    ret = RaRdevDeinit (rdma_handle, NOTIFY);
    tc_hdc_env_deinit();
}

void tc_hdc_async_del_req_handle()
{
    pthread_mutex_t req_mutex;
    pthread_mutex_init(&req_mutex, NULL);

    struct RaListHead list1 = {0};

    RA_INIT_LIST_HEAD(&list1);
    RaHwAsyncDelList(&list1, &req_mutex);
}

void tc_ra_hdc_uninit_async()
{
    RaHdcUninitAsync();
}

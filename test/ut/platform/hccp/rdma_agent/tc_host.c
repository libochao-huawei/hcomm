/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "hccp.h"
#include "securec.h"
#include "ut_dispatch.h"
#include "ra_client_host.h"
#include "ra_hdc.h"
#include "ra_hdc_rdma_notify.h"
#include "ra_hdc_rdma.h"
#include "ra_hdc_socket.h"
#include "ra_peer.h"

extern struct RaSocketOps gRaPeerSocketOps;
extern int HdcSendRecvPkt(void *session, void *p_send_rcv_buf, unsigned int in_buf_len, unsigned int out_data_len);
extern int RaInetPton(int family, union HccpIpAddr ip, char net_addr[], unsigned int len);
extern int RaHdcNotifyBaseAddrInit(unsigned int notifyType, unsigned int phyId, unsigned long long **notifyVa);
extern int RaRdevInitCheck(int mode, struct rdev rdev_info, char localIp[], unsigned int num, void *rdma_handle);
extern int RaRdevInitCheckIp(int mode, struct rdev rdevInfo, char localIp[]);
extern void RsSetCtx(unsigned int phyId);
extern int RsSocketGetClientSocketErrInfo(struct SocketConnectInfo conn[], struct SocketErrInfo  err[],
    unsigned int num);
extern int RsSocketGetServerSocketErrInfo(struct SocketListenInfo conn[], struct ServerSocketErrInfo err[],
    unsigned int num);
extern int RsSocketAcceptCreditAdd(struct SocketListenInfo conn[], uint32_t num, unsigned int credit_limit);
extern int RaPeerSocketAcceptCreditAdd(unsigned int phyId, struct SocketListenInfoT conn[], unsigned int num,
    unsigned int credit_limit);

extern unsigned int g_interface_version;

int ra_hdc_get_ifaddrs_stub(unsigned int phyId, struct IfaddrInfo ifaddr_infos[], unsigned int *num)
{
    *num = 4;
    return 0;
}

int ra_get_interface_version_stub(unsigned int phyId, unsigned int interface_opcode, unsigned int* interface_version)
{
    *interface_version = g_interface_version;
    return 0;
}

void tc_ifaddr()
{
    int ret;

    mocker_invoke(RaHdcGetInterfaceVersion, ra_get_interface_version_stub, 100);
    g_interface_version = 0;
    unsigned int ifaddr_num = 4;
    struct InterfaceInfo interface_infos[4] = {0};
    bool is_all = false;

    ret = RaIfaddrInfoConverter(0, is_all, interface_infos, &ifaddr_num);
    EXPECT_INT_EQ(-22, ret);

    g_interface_version = 1;
    ifaddr_num = 4;
    ret = RaIfaddrInfoConverter(0, is_all, interface_infos, &ifaddr_num);
    EXPECT_INT_EQ(-22, ret);

    mocker(RaHdcGetIfaddrs, 100 , 0);
    g_interface_version = 1;
    ifaddr_num = 4;
    ret = RaIfaddrInfoConverter(0, is_all, interface_infos, &ifaddr_num);
    EXPECT_INT_EQ(0, ret);

    g_interface_version = 2;
    ifaddr_num = 4;
    ret = RaIfaddrInfoConverter(0, is_all, interface_infos, &ifaddr_num);
    EXPECT_INT_EQ(-22, ret);

    g_interface_version = 3;
    ifaddr_num = 4;
    ret = RaIfaddrInfoConverter(0, is_all, interface_infos, &ifaddr_num);
    EXPECT_INT_EQ(-22, ret);

    g_interface_version = 1;
    ifaddr_num = 4;
    mocker(calloc, 10 , NULL);
    ret = RaIfaddrInfoConverter(0, is_all, interface_infos, &ifaddr_num);
    EXPECT_INT_EQ(-22, ret);
    mocker_clean();

    return;
}

int stub_ra_hdc_send_wr_v2(struct RaQpHandle *qp_hdc, struct SendWrV2 *wr, struct SendWrRsp *op_rsp)
{
    return 0;
}

void tc_host()
{
    DlHalInit();
    int ret = 0;
    struct RdevInitInfo init_info = {0};
    struct rdev rdev_info = {0};
    rdev_info.phyId = 0;
    rdev_info.family = AF_INET;
    rdev_info.localIp.addr.s_addr = 0;
    struct MrInfoT mr_info;
    mocker((stub_fn_t)HdcSendRecvPkt, 200, 0);
    mocker_invoke(RaHdcGetInterfaceVersion, ra_get_interface_version_stub, 200);
    g_interface_version = 2;

    ret = RaRdevInitWithBackup(NULL, NULL, NULL, NULL);
    EXPECT_INT_NE(0, ret);

    struct RaRdmaHandle *rdma_handle_bakcup = NULL;
    ret = RaRdevInitWithBackup(&init_info, &rdev_info, &rdev_info, &rdma_handle_bakcup);
    EXPECT_INT_NE(0, ret);

    RaRdevDeinit(NULL, NOTIFY);
    struct RaRdmaHandle *rdma_handle = NULL;
    struct RaRdmaHandle *rdma_handle2 = NULL;
    ret = RaRdevInit(NETWORK_OFFLINE, NOTIFY, rdev_info, NULL);
    EXPECT_INT_NE(0, ret);

    ret = RaRdevInit(5, NOTIFY, rdev_info, &rdma_handle);
    EXPECT_INT_NE(0, ret);

    ret = RaRdevInit(NETWORK_OFFLINE, NOTIFY, rdev_info, &rdma_handle);
    EXPECT_INT_EQ(328008, ret);

    mocker(RaRdevInitCheckIp, 10, 0);
    ret = RaRdevInit(NETWORK_OFFLINE, NOTIFY, rdev_info, &rdma_handle);
    EXPECT_INT_EQ(0, ret);
    ret = RaRdevGetHandle(rdev_info.phyId, &rdma_handle2);
    EXPECT_INT_EQ(0, ret);
    EXPECT_INT_EQ(rdma_handle, rdma_handle2);
    mocker_clean();

    mocker((stub_fn_t)HdcSendRecvPkt, 200, 0);
    mocker_invoke(RaHdcGetInterfaceVersion, ra_get_interface_version_stub, 200);
    struct RaSocketHandle *socket_handle = NULL;
    ret = RaSocketInit(NETWORK_OFFLINE, rdev_info, &socket_handle);
    EXPECT_INT_EQ(0, ret);
    ret = RaQpCreate(NULL, 0, 1, NULL);
    EXPECT_INT_NE(0, ret);
    ret = RaQpCreateWithAttrs(NULL, NULL, NULL);
    EXPECT_INT_NE(0, ret);
    ret = RaAiQpCreate(NULL, NULL, NULL, NULL);
    EXPECT_INT_NE(0, ret);

    RaGetIfnum(NULL, NULL);

    int ifnum = 0;
    struct RaInitConfig config = { 0 };
    struct RaGetIfattr ifattr_config = { 0 };
	ifattr_config.nicPosition = NETWORK_PEER_ONLINE;
    ret = RaGetIfnum(&ifattr_config, &ifnum);
    EXPECT_INT_EQ(0, ret);

    ifattr_config.phyId = 0;
    ifattr_config.nicPosition = NETWORK_OFFLINE;
    ret = RaGetIfnum(&ifattr_config, &ifnum);
    EXPECT_INT_EQ(0, ret);

    ifattr_config.isAll = true;
    ret = RaGetIfnum(&ifattr_config, &ifnum);
    EXPECT_INT_NE(0, ret);
    ifattr_config.isAll = false;

    ifattr_config.nicPosition = 5;
    ret = RaGetIfnum(&ifattr_config, &ifnum);
    EXPECT_INT_EQ(228304, ret);

    ifattr_config.phyId = 129;
    ifattr_config.nicPosition = NETWORK_OFFLINE;
    ret = RaGetIfnum(&ifattr_config, &ifnum);
    EXPECT_INT_EQ(128303, ret);

    ifattr_config.nicPosition = 5;
    ret = RaGetIfnum(&ifattr_config, &ifnum);
    EXPECT_INT_EQ(128303, ret);

    RaGetIfaddrs(NULL, NULL, NULL);
    RaSocketWhiteListAdd(socket_handle, NULL, 0);
    RaSocketWhiteListDel(socket_handle, NULL, 0);

    ret = RaSocketSetWhiteListStatus(0);
    EXPECT_INT_EQ(0, ret);
    ret = RaSocketSetWhiteListStatus(3);
    EXPECT_INT_EQ(128203, ret);
    ret = RaSocketGetWhiteListStatus(NULL);
    EXPECT_INT_EQ(128203, ret);
    unsigned int enable;
    ret = RaSocketGetWhiteListStatus(&enable);
    EXPECT_INT_EQ(0, ret);

    RaSocketListenStart(NULL, 0);
    RaSocketListenStop(NULL, 0);
    RaGetSockets(0, NULL, 0, NULL);
    RaSocketSend(NULL, NULL, 0, NULL);
    RaSocketRecv(NULL, NULL, 0, NULL);
    RaGetQpStatus(NULL, NULL);
    RaMrReg(NULL, NULL);
    RaMrDereg(NULL, NULL);
    RaSendWr(NULL, NULL, NULL);
    RaSendWrlist(NULL, NULL, NULL, 0, NULL);
    RaGetNotifyBaseAddr(NULL, NULL, NULL);
    RaGetNotifyMrInfo(NULL, NULL);
    RaQpDestroy(NULL);
    RaQpConnectAsync(NULL, NULL);
    RaRegisterMr(NULL, NULL, NULL);
    RaDeregisterMr(NULL, NULL);
    RaGetCqeErrInfo(0, NULL);
    RaGetQpAttr(NULL, NULL);

    ret = RaSendWrV2(NULL, NULL, NULL);
    EXPECT_INT_NE(0, ret);
    struct RaQpHandle ra_qp_handle_v2 = {0};
    struct SendWrV2 wr_v2 = {0};
    struct SendWrRsp op_rsp_v2 = {0};
    struct SgList list_v2 = {0};
    list_v2.len = 0xFFFFFFFF;
    wr_v2.bufList = &list_v2;
    ret = RaSendWrV2(&ra_qp_handle_v2, &wr_v2, &op_rsp_v2);
    EXPECT_INT_NE(0, ret);

    list_v2.len = 0x1;
    ret = RaSendWrV2(&ra_qp_handle_v2, &wr_v2, &op_rsp_v2);
    EXPECT_INT_NE(0, ret);

    struct RaRdmaOps rdma_ops_v2 = {0};
    rdma_ops_v2.raSendWrV2 = stub_ra_hdc_send_wr_v2;
    ra_qp_handle_v2.rdmaOps = &rdma_ops_v2;
    ret = RaSendWrV2(&ra_qp_handle_v2, &wr_v2, &op_rsp_v2);
    EXPECT_INT_EQ(0, ret);

    int devid = 0;
    int ra_case_other = 2;
    unsigned int remote_ip[1] = {0};
    struct SocketWlistInfoT white_list[1];
    int flag = 0;
    int port = 0;
    int timeout = 0;
    void *addr = malloc(1);
    void *data = malloc(1);
    unsigned long long size = 1;
    int access = 1;

    struct SendWr *wr = calloc(1, sizeof(struct SendWr));
    struct SgList list[2];
    list[0].len = 1;
    list[1].len = 2147483649;
    int wqe_index = 0;
    wr->bufList = list;
    int index = 0;
    unsigned long pa = 1;
    unsigned long va = 1;
    int sock_fd = 1;
    struct RaRdmaOps rdma_ops;

    struct SocketHdcInfo *hdc_socket_handle = calloc(1, sizeof(struct SocketHdcInfo));
    struct SocketConnectInfoT conn[1];
    conn[0].socketHandle = NULL;
    RaSocketBatchConnect(&conn, 1);
    RaSocketBatchClose(&conn, 1);

    conn[0].socketHandle = socket_handle;
    struct SocketInfoT conn_tmp = {0};
    struct SocketCloseInfoT close[1] = {0};
    struct SocketListenInfoT listen[1];

    listen[0].socketHandle = NULL;
    RaSocketListenStart(&listen, 1);
    RaSocketListenStop(&listen, 1);

    listen[0].socketHandle = socket_handle;
    close[0].socketHandle = socket_handle;
    close[0].fdHandle = hdc_socket_handle;

    struct SocketInfoT socket_info[1] = {0};

	struct RaInitConfig offline_config = {
		.phyId = 0,
		.nicPosition = 1,
        .hdcType = HDC_SERVICE_TYPE_RDMA,
	};
    struct RaGetIfattr offline_ifattr_config = {
		.phyId = 0,
		.nicPosition = 1,
        .isAll = 0,
	};
    int qp_status = 0;
    int server = 0;
    int client = 1;
    struct SendWrRsp op_rsp = {0};

    struct SendWrlistData wrlist_send[1];
    struct SendWrRsp wrlist_rsp[1];
	unsigned int ifaddr_num = 4;
	struct InterfaceInfo interface_infos[4] = {0};
	ret = RaGetIfaddrs(&offline_ifattr_config, interface_infos, &ifaddr_num);
	EXPECT_INT_EQ(0, ret);

    ifaddr_num = 0;
    ret = RaGetIfaddrs(&offline_ifattr_config, interface_infos, &ifaddr_num);
    EXPECT_INT_EQ(128303, ret);

	ifaddr_num = 9;
	ret = RaGetIfaddrs(&offline_ifattr_config, interface_infos, &ifaddr_num);
	EXPECT_INT_EQ(128303, ret);

    ifaddr_num = 4;
    offline_ifattr_config.phyId = 128;
    ret = RaGetIfaddrs(&offline_ifattr_config, interface_infos, &ifaddr_num);
    EXPECT_INT_EQ(128303, ret);

    offline_ifattr_config.phyId = 0;
    offline_ifattr_config.nicPosition = NETWORK_PEER_ONLINE;
    ret = RaGetIfaddrs(&offline_ifattr_config, interface_infos, &ifaddr_num);
    EXPECT_INT_EQ(0, ret);

    offline_ifattr_config.nicPosition = 5;
    ret = RaGetIfaddrs(&offline_ifattr_config, interface_infos, &ifaddr_num);
    EXPECT_INT_EQ(228304, ret);

    offline_ifattr_config.isAll = true;
    ret = RaGetIfaddrs(&offline_ifattr_config, interface_infos, &ifaddr_num);
    EXPECT_INT_NE(0, ret);
    offline_ifattr_config.isAll = false;

    mocker_invoke(RaHdcGetIfaddrs, ra_hdc_get_ifaddrs_stub, 10);
    ifaddr_num = 4;
    ret = RaIfaddrInfoConverter(0, 0, interface_infos, &ifaddr_num);
    EXPECT_INT_EQ(0, ret);

    mocker_clean();
    mocker((stub_fn_t)HdcSendRecvPkt, 200, 0);
    offline_config.nicPosition = 1;
    unsigned int interface_version = 0;
    ret  = RaGetInterfaceVersion (0, 0, &interface_version);
    EXPECT_INT_EQ(0, ret);

    interface_version = 0;
    ret  = RaGetInterfaceVersion (0, 12, &interface_version);
    EXPECT_INT_EQ(0, ret);

    interface_version = 0;
    ret  = RaGetInterfaceVersion (0, 24, &interface_version);
    EXPECT_INT_EQ(0, ret);

    interface_version = 0;
    ret  = RaGetInterfaceVersion (0, 25, NULL);
    EXPECT_INT_EQ(128303, ret);

    ret  = ConverReturnCode (0, 100001);
    EXPECT_INT_EQ(100001, ret);

    mocker_invoke(RaHdcGetInterfaceVersion, ra_get_interface_version_stub, 200);
    g_interface_version = 2;
    ret = RaSocketWhiteListAdd(socket_handle, white_list, 1);
    EXPECT_INT_EQ(0, ret);
    ret = RaSocketWhiteListDel(socket_handle, white_list, 1);
    EXPECT_INT_EQ(0, ret);

    ret = RaSocketBatchConnect(NULL, 1);
    EXPECT_INT_NE(0, ret);
    ret = RaSocketBatchConnect(&conn, 1);
    EXPECT_INT_EQ(0, ret);

    unsigned long long sent_size = 0;
    ret = RaSocketSend(hdc_socket_handle, data, 1, &sent_size);
    EXPECT_INT_EQ(128203, ret);

    unsigned long long received_size = 0;
    ret = RaSocketRecv(hdc_socket_handle, data, 1, &received_size);
    EXPECT_INT_EQ(128203, ret);

    ret = RaSocketBatchClose(NULL, 1);
    EXPECT_INT_NE(0, ret);
    ret = RaSocketBatchClose(&close, 1);
    EXPECT_INT_EQ(0, ret);

    hdc_socket_handle = calloc(1, sizeof(struct SocketHdcInfo));

    socket_info[0].socketHandle = NULL;
    unsigned int connected_num = 0;
    RaGetSockets(0, &socket_info, 1, &connected_num);

    socket_info[0].fdHandle = hdc_socket_handle;
    socket_info[0].socketHandle = socket_handle;
    ret = RaGetSockets(0, &socket_info, 1, &connected_num);
    EXPECT_INT_EQ(0, ret);

    ret = RaSocketListenStart(&listen, 1);
    EXPECT_INT_EQ(0, ret);

    socket_handle->rdevInfo.family = AF_INET;
    socket_handle->rdevInfo.phyId = 0;
    ret = RaSocketListenStop(&listen, 1);
    EXPECT_INT_EQ(0, ret);

    socket_handle->rdevInfo.phyId = 8;
    ret = RaSocketSend(hdc_socket_handle, data, 1, &sent_size);

    ret = RaSocketRecv(hdc_socket_handle, data, 1, &received_size);

    mocker(RaHdcSocketSend, 10 , 1);
    ret = RaSocketSend(hdc_socket_handle, data, 1, &sent_size);

    mocker(RaHdcSocketRecv, 10 , 1);
    ret = RaSocketRecv(hdc_socket_handle, data, 1, &received_size);

    socket_handle->rdevInfo.phyId = 129;
    ret = RaSocketWhiteListAdd(socket_handle, white_list, 1);
    EXPECT_INT_NE(0, ret);
    ret = RaSocketWhiteListDel(socket_handle, white_list, 1);
    EXPECT_INT_NE(0, ret);

    ret = RaSocketBatchConnect(&conn, 1);
    EXPECT_INT_NE(0, ret);

    ret = RaSocketSend(hdc_socket_handle, data, 1, &sent_size);
    EXPECT_INT_NE(0, ret);

    ret = RaSocketRecv(hdc_socket_handle, data, 1, &received_size);
    EXPECT_INT_NE(0, ret);

    ret = RaSocketBatchClose(&close, 1);
    EXPECT_INT_NE(0, ret);

    RaGetSockets(0, &socket_info, 1, &connected_num);
    EXPECT_INT_NE(0, ret);

    ret = RaSocketListenStart(&listen, 1);
    EXPECT_INT_NE(0, ret);
    socket_handle->rdevInfo.family = AF_INET;
    ret = RaSocketListenStop(&listen, 1);
    EXPECT_INT_NE(0, ret);

    ret = RaQpCreate(rdma_handle, 3, 0, NULL);
    EXPECT_INT_NE(0, ret);
    ret = RaQpCreateWithAttrs(rdma_handle, NULL, NULL);
    EXPECT_INT_NE(0, ret);
    ret = RaAiQpCreate(rdma_handle, NULL, NULL, NULL);
    EXPECT_INT_NE(0, ret);
    struct RaQpHandle *qp_handle = NULL;
    struct RaQpHandle *qp_handle_with_attr = NULL;
    struct RaQpHandle *typical_qp_handle = NULL;
    struct RaQpHandle *ai_qp_handle = NULL;
    struct AiQpInfo info;
    ret = RaQpCreateWithAttrs(rdma_handle, NULL, &qp_handle_with_attr);
    EXPECT_INT_NE(0, ret);
    ret = RaAiQpCreate(rdma_handle, NULL, &info, &ai_qp_handle);
    EXPECT_INT_NE(0, ret);
    struct QpExtAttrs extAttrs;
    extAttrs.version = 0;
    ret = RaQpCreateWithAttrs(rdma_handle, &extAttrs, &qp_handle_with_attr);
    EXPECT_INT_NE(0, ret);
    ret = RaAiQpCreate(rdma_handle, &extAttrs, &info, &ai_qp_handle);
    EXPECT_INT_NE(0, ret);
    extAttrs.version = QP_CREATE_WITH_ATTR_VERSION;
    extAttrs.qpMode = -1;
    ret = RaQpCreateWithAttrs(rdma_handle, &extAttrs, &qp_handle_with_attr);
    EXPECT_INT_NE(0, ret);
    ret = RaAiQpCreate(rdma_handle, &extAttrs, &info, &qp_handle_with_attr);
    EXPECT_INT_NE(0, ret);
    extAttrs.qpMode = RA_RS_GDR_TMPL_QP_MODE;
    ret = RaQpCreate(rdma_handle, 1, 0, &qp_handle);
    EXPECT_INT_NE(0, ret);
    ret = RaQpCreate(rdma_handle, 0, 0, &qp_handle);
    EXPECT_INT_EQ(0, ret);
    ret = RaQpCreateWithAttrs(rdma_handle, &extAttrs, &qp_handle_with_attr);
    EXPECT_INT_EQ(0, ret);
    ret = RaAiQpCreate(rdma_handle, &extAttrs, &info, &ai_qp_handle);
    EXPECT_INT_EQ(0, ret);
    ret = RaQpDestroy(qp_handle_with_attr);
    EXPECT_INT_EQ(0, ret);
    ret = RaQpDestroy(ai_qp_handle);
    EXPECT_INT_EQ(0, ret);

    ret = RaQpBatchModify(NULL, NULL, 0, 0);
    EXPECT_INT_NE(0, ret);

    struct RaQpHandle *batch_modify_qp_hdc[1];
    batch_modify_qp_hdc[0] = qp_handle;

    ret = RaQpBatchModify(rdma_handle, batch_modify_qp_hdc, 1, 5);
    EXPECT_INT_EQ(0, ret);

    ret = RaQpBatchModify(rdma_handle, batch_modify_qp_hdc, 1, 1);
    EXPECT_INT_EQ(0, ret);

    struct RaRdmaHandle rdma_handle_err = {0};
    rdma_handle_err = *rdma_handle;
    (&rdma_handle_err)->rdevInfo.phyId = 128;

    ret = RaQpBatchModify(&rdma_handle_err, batch_modify_qp_hdc, 1, 1);
    EXPECT_INT_NE(0, ret);

    batch_modify_qp_hdc[0] = NULL;
    ret = RaQpBatchModify(rdma_handle, batch_modify_qp_hdc, 1, 5);
    EXPECT_INT_NE(0, ret);

	unsigned int temp_depth, qp_num;
	ret = RaGetTsqpDepth(NULL, &temp_depth, &qp_num);
	EXPECT_INT_EQ(128103, ret);

	ret = RaGetTsqpDepth(rdma_handle, NULL, &qp_num);
	EXPECT_INT_EQ(128103, ret);

	ret = RaGetTsqpDepth(rdma_handle, &temp_depth, &qp_num);
	EXPECT_INT_EQ(0, ret);

	ret = RaSetTsqpDepth(NULL, temp_depth, &qp_num);
	EXPECT_INT_EQ(128103, ret);

	ret = RaSetTsqpDepth(rdma_handle, temp_depth, NULL);
	EXPECT_INT_EQ(128103, ret);

	temp_depth = 1;
	ret = RaSetTsqpDepth(rdma_handle, temp_depth, &qp_num);
	EXPECT_INT_EQ(128103, ret);

	temp_depth = 8;
	ret = RaSetTsqpDepth(rdma_handle, temp_depth, &qp_num);
	EXPECT_INT_EQ(0, ret);

    struct RaQpHandle *qp_handle_wrlist = NULL;
    ret = RaQpCreate(rdma_handle, 0, 0, &qp_handle_wrlist);
    EXPECT_INT_EQ(0, ret);
    qp_handle_wrlist->rdmaOps = NULL;
    unsigned int complete_num = 1;
    wrlist_send[0].memList.len = 1;
    ret = RaSendWrlist(qp_handle_wrlist, wrlist_send, wrlist_rsp, 1, &complete_num);
    qp_handle_wrlist->rdmaOps = rdma_handle->rdmaOps;
    ret = RaQpDestroy(qp_handle_wrlist);
    EXPECT_INT_EQ(0, ret);

    mr_info.addr = addr;
    mr_info.access = access;
    mr_info.lkey = 0;
    mr_info.size = size;
    ret = RaMrReg(qp_handle, &mr_info);
    EXPECT_INT_EQ(0, ret);

    ret = RaGetNotifyBaseAddr(rdma_handle, &va, &size);
    EXPECT_INT_EQ(0, ret);
    struct MrInfoT mr_info2;
    ret = RaGetNotifyMrInfo(rdma_handle, &mr_info2);
    EXPECT_INT_EQ(0, ret);

    ret = RaGetQpStatus(qp_handle, &qp_status);
    EXPECT_INT_EQ(0, ret);

    ret = RaSendWr(qp_handle, wr, &op_rsp);
    EXPECT_INT_EQ(0, ret);

    ret = RaQpConnectAsync(qp_handle, hdc_socket_handle);
    EXPECT_INT_EQ(0, ret);

    ret = RaMrDereg(qp_handle, &mr_info);
    EXPECT_INT_EQ(0, ret);

    ret = RaQpDestroy(qp_handle);
    EXPECT_INT_EQ(0, ret);

    struct TypicalQp qp_info = {0};
    ret = RaTypicalQpCreate(NULL, 0, RA_RS_OP_QP_MODE_EXT, &qp_info, &typical_qp_handle);
    EXPECT_INT_NE(0, ret);
    ret = RaTypicalQpCreate(rdma_handle, 0, RA_RS_OP_QP_MODE_EXT, &qp_info, NULL);
    EXPECT_INT_NE(0, ret);
    ret = RaTypicalQpCreate(rdma_handle, 0, RA_RS_OP_QP_MODE_EXT, NULL, &typical_qp_handle);
    EXPECT_INT_NE(0, ret);
    ret = RaTypicalQpCreate(rdma_handle, 3, RA_RS_OP_QP_MODE_EXT, &qp_info, &typical_qp_handle);
    EXPECT_INT_NE(0, ret);
    ret = RaTypicalQpCreate(rdma_handle, 0, -1, &qp_info, &typical_qp_handle);
    EXPECT_INT_NE(0, ret);
    ret = RaTypicalQpCreate(rdma_handle, 0, RA_RS_OP_QP_MODE_EXT, &qp_info, &typical_qp_handle);
    EXPECT_INT_EQ(0, ret);

    struct TypicalQp remote_qp_info = {0};
    ret = RaTypicalQpModify(typical_qp_handle, NULL, &remote_qp_info);
    EXPECT_INT_NE(0, ret);
    ret = RaTypicalQpModify(NULL, &qp_info, &remote_qp_info);
    EXPECT_INT_NE(0, ret);
    ret = RaTypicalQpModify(typical_qp_handle, &qp_info, &remote_qp_info);
    EXPECT_INT_EQ(0, ret);

    ret = RaTypicalSendWr(NULL, wr, &op_rsp);
    EXPECT_INT_NE(0, ret);
    ret = RaTypicalSendWr(typical_qp_handle, wr, &op_rsp);
    EXPECT_INT_EQ(0, ret);

    ret = RaQpDestroy(typical_qp_handle);
    EXPECT_INT_EQ(0, ret);

    struct RaQpHandle *qp_handle1 = NULL;
    struct RaQpHandle *qp_handle1_with_attr = NULL;
    struct RaQpHandle *typical_qp_handle1 = NULL;
    ret = RaQpCreate(rdma_handle, 0, 5, &qp_handle1);
    EXPECT_INT_NE(0, ret);

    ret = RaGetQpAttr(qp_handle1, &qp_num);
    EXPECT_INT_NE(0, ret);

    struct QpAttr attr;
    struct RaQpHandle qp_handle_tmp;
    ret = RaGetQpAttr(&qp_handle_tmp, &attr);
    EXPECT_INT_EQ(0, ret);

    rdma_handle->rdevInfo.phyId =129;
    ret = RaQpCreate(rdma_handle, 0, 0, &qp_handle1);
    EXPECT_INT_NE(0, ret);
    ret = RaQpCreateWithAttrs(rdma_handle, &extAttrs, &qp_handle1_with_attr);
    EXPECT_INT_NE(0, ret);
    ret = RaTypicalQpCreate(rdma_handle, 0, RA_RS_OP_QP_MODE_EXT, &qp_info, &typical_qp_handle1);
    EXPECT_INT_NE(0, ret);

    ret = RaGetTsqpDepth(rdma_handle, &temp_depth, &qp_num);
    EXPECT_INT_EQ(128103, ret);
    ret = RaSetTsqpDepth(rdma_handle, temp_depth, &qp_num);
    EXPECT_INT_EQ(128103, ret);

    rdma_handle->rdmaOps->raQpCreate = NULL;
    rdma_handle->rdmaOps->raQpCreateWithAttrs = NULL;
    rdma_handle->rdmaOps->raTypicalQpCreate = NULL;
    ret = RaQpCreate(rdma_handle, 0, 0, &qp_handle1);
    EXPECT_INT_NE(0, ret);
    ret = RaQpCreateWithAttrs(rdma_handle, &extAttrs, &qp_handle1_with_attr);
    EXPECT_INT_NE(0, ret);
    ret = RaTypicalQpCreate(rdma_handle, 0, RA_RS_OP_QP_MODE_EXT, &qp_info, &typical_qp_handle1);
    EXPECT_INT_NE(0, ret);

    struct RaQpHandle qp_handle2 = {0};
    qp_handle = &qp_handle2;
    qp_handle->rdmaOps = NULL;
    mr_info.addr = addr;
    mr_info.access = access;
    mr_info.lkey = 0;
    mr_info.size = size;
    ret = RaMrReg(qp_handle, &mr_info);
    EXPECT_INT_NE(0, ret);

    rdma_handle->rdevInfo.phyId = 0;
    ret = RaGetNotifyBaseAddr(rdma_handle, &va, &size);
    EXPECT_INT_EQ(0, ret);
	ret = RaGetNotifyMrInfo(rdma_handle, &mr_info2);
	EXPECT_INT_EQ(0, ret);

    ret = RaGetQpStatus(qp_handle, &qp_status);
    EXPECT_INT_NE(0, ret);

    ret = RaSendWr(qp_handle, wr, &op_rsp);
    EXPECT_INT_NE(0, ret);

    ret = RaQpConnectAsync(qp_handle, hdc_socket_handle);
    EXPECT_INT_NE(0, ret);

    ret = RaMrDereg(qp_handle, &mr_info);
    EXPECT_INT_NE(0, ret);

    ret = RaQpDestroy(qp_handle);
    EXPECT_INT_NE(0, ret);

    socket_handle->rdevInfo.phyId = 128;
    ret = RaSocketDeinit(socket_handle);
    EXPECT_INT_EQ(128003, ret);

    mocker(RaHdcSocketDeinit, 1, -1);
    socket_handle->rdevInfo.phyId = 0;
    ret = RaSocketDeinit(socket_handle);
    EXPECT_INT_EQ(128000, ret);

    rdma_handle->rdevInfo.phyId = 128;
    ret = RaRdevDeinit(rdma_handle, NOTIFY);
    EXPECT_INT_EQ(128003, ret);

    rdma_handle->rdevInfo.phyId = 0;
    ret = RaRdevDeinit(rdma_handle, NOTIFY);
    EXPECT_INT_EQ(0, ret);

    mocker_clean();

    ret = RaInit(NULL);
    EXPECT_INT_EQ(128003, ret);
    ret = RaDeinit(NULL);
    EXPECT_INT_EQ(128003, ret);
    struct RaInitConfig erro_device_config = {
        .phyId = 129,
        .nicPosition = 0,
        .hdcType = HDC_SERVICE_TYPE_RDMA,
    };
    ret = RaInit(&erro_device_config);
    EXPECT_INT_EQ(128003, ret);

    ret = RaDeinit(&erro_device_config);
    EXPECT_INT_EQ(128003, ret);

    struct RaInitConfig device_config = {
        .phyId = 0,
        .nicPosition = 0,
        .hdcType = 0,
    };
    ret = RaInit(&device_config);
    EXPECT_INT_EQ(0, ret);

    mocker(RaHdcInit, 10 ,0);
    ret = RaInit(&offline_config);
    mocker_clean();

    mocker(RaHdcInit, 10 ,-1);
    ret = RaInit(&offline_config);
    EXPECT_INT_NE(0, ret);
    mocker_clean();

    mocker_invoke(RaHdcGetInterfaceVersion, ra_get_interface_version_stub, 10);
    ret  = RaGetInterfaceVersion (0, 24, &interface_version);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();

    struct RaInitConfig online_config = {
        .phyId = 0,
        .nicPosition = 2,
        .hdcType = HDC_SERVICE_TYPE_RDMA,
    };
    ret = RaInit(&online_config);
    EXPECT_INT_EQ(228004, ret);

    mocker(drvGetProcessSign, 10 ,-1);
    ret = RaInit(&offline_config);
    EXPECT_INT_NE(0, ret);
    mocker_clean();

    mocker(strcpy_s, 10 ,-1);
    ret = RaInit(&offline_config);
    EXPECT_INT_NE(0, ret);
    mocker_clean();

    ret = RaDeinit(&device_config);
    EXPECT_INT_EQ(0, ret);

    device_config.nicPosition = 5;
    ret = RaDeinit(&device_config);
    EXPECT_INT_NE(0, ret);

    mocker(RaHdcDeinit, 10 ,0);
    ret = RaDeinit(&offline_config);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();

    mocker(RaHdcDeinit, 10 ,-1);
    ret = RaDeinit(&offline_config);
    EXPECT_INT_NE(0, ret);
    mocker_clean();

    mocker(RaInetPton, 10, 0);
    ret =  RaSocketInit(NETWORK_OFFLINE, rdev_info, &socket_handle);
    mocker_clean();

    mocker(RaInetPton, 10 , -22);
    ret = RaSocketInit(NETWORK_OFFLINE, rdev_info, &socket_handle);
    mocker_clean();

    mocker(memcpy_s, 10 ,-1);
    mocker_invoke(RaHdcGetInterfaceVersion, ra_get_interface_version_stub, 10);
    ret  = RaSocketInit(NETWORK_OFFLINE, rdev_info, &socket_handle);
    EXPECT_INT_NE(0, ret);

    ret = RaRdevInit(NETWORK_OFFLINE, NOTIFY, rdev_info, &rdma_handle);
    EXPECT_INT_NE(0, ret);
    mocker_clean();

    mocker(calloc, 10 , 0);
    mocker_invoke(RaHdcGetInterfaceVersion, ra_get_interface_version_stub, 10);
    ret = RaSocketInit(NETWORK_OFFLINE, rdev_info, &socket_handle);
    EXPECT_INT_NE(0, ret);

    ret = RaRdevInit(NETWORK_OFFLINE, NOTIFY, rdev_info, &rdma_handle);
    EXPECT_INT_NE(0, ret);
    mocker_clean();

    mocker_invoke(RaHdcGetInterfaceVersion, ra_get_interface_version_stub, 100);
    rdev_info.phyId = 129;
    ret = RaSocketInit(NETWORK_OFFLINE, rdev_info, &socket_handle);
    EXPECT_INT_NE(0, ret);

    ret = RaRdevInit(NETWORK_OFFLINE, NOTIFY, rdev_info, &rdma_handle);
    EXPECT_INT_NE(0, ret);

    ret = RaSocketDeinit(NULL);
    EXPECT_INT_NE(0, ret);

    rdev_info.phyId = 0;
    ret = RaSocketInit(5, rdev_info, &socket_handle);
    EXPECT_INT_NE(0, ret);

    ret = RaRdevInit(5, NOTIFY, rdev_info, &rdma_handle);
    EXPECT_INT_NE(0, ret);

    mocker(RaRdevInitCheck, 2 , 0);
    mocker(RaHdcNotifyBaseAddrInit, 5 , 0);
    mocker(calloc, 10 , NULL);
    ret = RaRdevInit(5, NOTIFY, rdev_info, &rdma_handle);
    EXPECT_INT_EQ(328000, ret);
    mocker_clean();

    rdev_info.phyId = 0;
    rdev_info.family = AF_INET;
    rdev_info.localIp.addr.s_addr = 0;
    char localIp[1];
    mocker(RaGetIfaddrs, 10, 0);
    mocker(RaInetPton, 10, -1);
    RaRdevInitCheckIp(NETWORK_OFFLINE, rdev_info, localIp);
    mocker_clean();

    mocker(RaRdevInitCheck, 2 , 0);
    mocker(RaHdcNotifyBaseAddrInit, 5 , 0);
    ret = RaRdevInit(10, NOTIFY, rdev_info, &rdma_handle);
    EXPECT_INT_EQ(128003, ret);
    mocker_clean();

    mocker(RaRdevInitCheck, 2 , 0);
    mocker(RaPeerNotifyBaseAddrInit, 5 , 0);
    mocker(memcpy_s, 10 , -1);
    ret = RaRdevInit(NETWORK_PEER_ONLINE, NOTIFY, rdev_info, &rdma_handle);
    EXPECT_INT_EQ(328006, ret);
    mocker_clean();

    mocker(RaRdevInitCheck, 2 , 0);
    mocker(RaHdcNotifyBaseAddrInit, 5 , 0);
    ret = RaRdevInit(NETWORK_OFFLINE, NOTIFY, rdev_info, &rdma_handle);
    EXPECT_INT_EQ(128003, ret);
    mocker_clean();

    rdev_info.phyId = 0;
    rdev_info.family = AF_INET;
    rdev_info.localIp.addr.s_addr = 7;
    mocker(RaGetIfaddrs, 10 , 0);
    ret = RaRdevInit(NETWORK_OFFLINE, NOTIFY, rdev_info, &rdma_handle);
    EXPECT_INT_EQ(328008, ret);
    mocker_clean();

	unsigned long long *notify_va = NULL;
    mocker(drvDeviceGetIndexByPhyId, 10 , -1);
    ret = RaHdcNotifyBaseAddrInit(NOTIFY, 0, &notify_va);
    EXPECT_INT_EQ(-1, ret);
    mocker_clean();

    mocker(halNotifyGetInfo, 10 , -1);
    ret = RaHdcNotifyBaseAddrInit(NOTIFY, 0, &notify_va);
    EXPECT_INT_EQ(-1, ret);
    mocker_clean();

    mocker(halMemAlloc, 10 , -1);
    ret = RaHdcNotifyBaseAddrInit(NOTIFY, 0, &notify_va);
    EXPECT_INT_EQ(-1, ret);
    mocker_clean();

    mocker(RaHdcNotifyCfgSet, 5 , -1);
    mocker(halMemFree, 10 , -1);
    ret = RaHdcNotifyBaseAddrInit(NOTIFY, 0, &notify_va);
    EXPECT_INT_EQ(-1, ret);
    mocker_clean();

	struct RaRdmaHandle rdma_handle_tmp = {0};
	struct RaRdmaOps ops = {0};
	rdma_handle_tmp.rdevInfo.phyId = 0;
	rdma_handle_tmp.rdevInfo.family = AF_INET;
	rdma_handle_tmp.rdevInfo.family = 0;
	rdma_handle_tmp.rdmaOps = &ops;
	mocker(RaInetPton, 1, -1);
	ret = RaRdevDeinit(&rdma_handle_tmp, NOTIFY);
	EXPECT_INT_EQ(128000, ret);
	mocker_clean();

	mocker(RaInetPton, 1, 0);
	ret = RaRdevDeinit(&rdma_handle_tmp, NOTIFY);
	EXPECT_INT_EQ(128003, ret);
	mocker_clean();

    free(addr);
    free(data);
    free(wr);
    free(hdc_socket_handle);
    DlHalDeinit();
    return;
}

int ra_recv_wrlist_stub(struct RaQpHandle *handle, struct RecvWrlistData *wr, unsigned int recv_num,
        unsigned int *complete_num)
{
    return 0;
}
void tc_ra_recv_wrlist(void)
{
    int ret;
    struct RaQpHandle qp_handle = {0};
    struct RecvWrlistData wr = {0};
    unsigned int recv_num = 1;
    unsigned int complete_num = 0;

    ret = RaRecvWrlist(NULL, NULL, recv_num, &complete_num);
    EXPECT_INT_EQ(128103, ret);

    wr.memList.len = 0xffffffff;
    ret = RaRecvWrlist(&qp_handle, &wr, recv_num, &complete_num);
    EXPECT_INT_EQ(128103, ret);

    wr.memList.len = 100;
    qp_handle.rdmaOps = NULL;
    ret = RaRecvWrlist(&qp_handle, &wr, recv_num, &complete_num);
    EXPECT_INT_EQ(128103, ret);

    struct RaRdmaOps rdma_ops = {0};
    rdma_ops.raRecvWrlist = ra_recv_wrlist_stub;
    qp_handle.rdmaOps = &rdma_ops;
    ret = RaRecvWrlist(&qp_handle, &wr, recv_num, &complete_num);
    EXPECT_INT_EQ(0, ret);
    return;
}

void tc_host_ra_send_wrlist_ext()
{
    struct RaQpHandle qp_handle;
    qp_handle.rdmaOps = NULL;

    struct SendWrlistDataExt wr[1];
    struct SendWrRsp op_rsp[1];

    unsigned int complete_num;

    RaSendWrlistExt(&qp_handle, wr, op_rsp, 1, &complete_num);
}

int ra_send_normal_wrlist_stub(void *qp_handle, struct WrInfo wr[], struct SendWrRsp op_rsp[], unsigned int send_num,
    unsigned int *complete_num)
{
    return 0;
}

void tc_host_ra_send_normal_wrlist()
{
    struct RaRdmaOps rdma_ops = {0};
    struct RaQpHandle qp_handle;
    struct SendWrRsp op_rsp[1];
    qp_handle.rdmaOps = NULL;
    unsigned int complete_num;
    struct WrInfo wr[1];
    int ret = 0;

    ret = RaSendNormalWrlist(&qp_handle, wr, op_rsp, 1, &complete_num);
    EXPECT_INT_EQ(128103, ret);

    rdma_ops.raSendNormalWrlist = ra_send_normal_wrlist_stub;
    qp_handle.rdmaOps = &rdma_ops;
    wr[0].memList.len = MAX_SG_LIST_LEN_MAX + 1;
    ret = RaSendNormalWrlist(&qp_handle, wr, op_rsp, 1, &complete_num);
    EXPECT_INT_EQ(128103, ret);

    wr[0].memList.len = 1;
    ret = RaSendNormalWrlist(&qp_handle, wr, op_rsp, 1, &complete_num);
    EXPECT_INT_EQ(0, ret);

    return;
}

int ra_set_qp_attr_qos_stub(struct RaQpHandle *qp_stub, struct QosAttr *attr)
{
    return 0;
}

void tc_ra_set_qp_attr_qos()
{
    int ret;
    struct QosAttr attr = {0};
    struct RaQpHandle qp_handle;
    qp_handle.rdmaOps = NULL;

    ret = RaSetQpAttrQos(NULL, &attr);
    EXPECT_INT_EQ(128103, ret);

    ret = RaSetQpAttrQos(&qp_handle, NULL);
    EXPECT_INT_EQ(128103, ret);

    attr.tc = 256;
    attr.sl = 8;
    ret = RaSetQpAttrQos(&qp_handle, &attr);
    EXPECT_INT_EQ(128103, ret);

    attr.sl = 4;
    ret = RaSetQpAttrQos(&qp_handle, &attr);
    EXPECT_INT_EQ(128103, ret);

    attr.tc = 33 * 4;
    ret = RaSetQpAttrQos(&qp_handle, &attr);
    EXPECT_INT_EQ(128103, ret);

    struct RaRdmaOps rdma_ops = {0};
    rdma_ops.raSetQpAttrQos = ra_set_qp_attr_qos_stub;
    qp_handle.rdmaOps = &rdma_ops;
    ret = RaSetQpAttrQos(&qp_handle, &attr);
    EXPECT_INT_EQ(0, ret);

    return;
}

int ra_set_qp_attr_timeout_stub(struct RaQpHandle *qp_stub, unsigned int *attr)
{
    return 0;
}

void tc_ra_set_qp_attr_timeout()
{
   int ret;
    unsigned int timeout = 0;
    struct RaQpHandle qp_handle;
    qp_handle.rdmaOps = NULL;

    ret = RaSetQpAttrTimeout(NULL, &timeout);
    EXPECT_INT_EQ(128103, ret);

    ret = RaSetQpAttrTimeout(&qp_handle, NULL);
    EXPECT_INT_EQ(128103, ret);

    timeout = 4;
    ret = RaSetQpAttrTimeout(&qp_handle, &timeout);
    EXPECT_INT_EQ(128103, ret);

    timeout = 4;
    ret = RaSetQpAttrTimeout(&qp_handle, &timeout);
    EXPECT_INT_EQ(128103, ret);

    timeout = 23;
    ret = RaSetQpAttrTimeout(&qp_handle, &timeout);
    EXPECT_INT_EQ(128103, ret);

    struct RaRdmaOps rdma_ops = {0};
    rdma_ops.raSetQpAttrTimeout = ra_set_qp_attr_timeout_stub;
    qp_handle.rdmaOps = &rdma_ops;
    ret = RaSetQpAttrTimeout(&qp_handle, &timeout);
    EXPECT_INT_EQ(0, ret);

    return;
}

int ra_set_qp_attr_retry_cnt_stub(struct RaQpHandle *qp_stub, unsigned int *retry_cnt)
{
    return 0;
}

void tc_ra_set_qp_attr_retry_cnt()
{
    int ret;
    unsigned int retry_cnt = 0;
    struct RaQpHandle qp_handle;
    qp_handle.rdmaOps = NULL;

    ret = RaSetQpAttrRetryCnt(NULL, &retry_cnt);
    EXPECT_INT_EQ(128103, ret);

    ret = RaSetQpAttrRetryCnt(&qp_handle, NULL);
    EXPECT_INT_EQ(128103, ret);

    retry_cnt = 8;
    ret = RaSetQpAttrRetryCnt(&qp_handle, &retry_cnt);
    EXPECT_INT_EQ(128103, ret);

    retry_cnt = 4;
    ret = RaSetQpAttrRetryCnt(&qp_handle, &retry_cnt);
    EXPECT_INT_EQ(128103, ret);

    retry_cnt = 7;
    ret = RaSetQpAttrRetryCnt(&qp_handle, &retry_cnt);
    EXPECT_INT_EQ(128103, ret);

    struct RaRdmaOps rdma_ops = {0};
    rdma_ops.raSetQpAttrRetryCnt = ra_set_qp_attr_retry_cnt_stub;
    qp_handle.rdmaOps = &rdma_ops;
    ret = RaSetQpAttrRetryCnt(&qp_handle, &retry_cnt);
    EXPECT_INT_EQ(0, ret);

    return;
}

void tc_ra_create_event_handle(void)
{
    int ret;
    int fd;

    ret = RaCreateEventHandle(NULL);
    EXPECT_INT_NE(0, ret);

    ret = RaCreateEventHandle(&fd);
    EXPECT_INT_EQ(0, ret);

    mocker(RaPeerCreateEventHandle, 1024, -EINVAL);
    ret = RaCreateEventHandle(&fd);
    EXPECT_INT_NE(0, ret);
    mocker_clean();
}

void tc_ra_ctl_event_handle(void)
{
    int ret;
    int fd = 0;
    int fd_handle;

    ret = RaCtlEventHandle(-1, NULL, 4, 100);
    EXPECT_INT_NE(0, ret);

    ret = RaCtlEventHandle(fd, NULL, 4, 100);
    EXPECT_INT_NE(0, ret);

    ret = RaCtlEventHandle(fd, &fd_handle, 4, 100);
    EXPECT_INT_NE(0, ret);

    ret = RaCtlEventHandle(fd, &fd_handle, 1, 100);
    EXPECT_INT_NE(0, ret);

    ret = RaCtlEventHandle(fd, &fd_handle, 1, 0);
    EXPECT_INT_EQ(0, ret);

    mocker(RaPeerCtlEventHandle, 1024, -EINVAL);
    ret = RaCtlEventHandle(fd, &fd_handle, 1, 0);
    EXPECT_INT_NE(0, ret);
    mocker_clean();
}

void tc_ra_wait_event_handle(void)
{
    int ret;
    int fd = 0;
    unsigned int events_num = 0;
    struct SocketEventInfoT event_info = {};

    ret = RaWaitEventHandle(-1, NULL, -2, -1, NULL);
    EXPECT_INT_NE(0, ret);

    ret = RaWaitEventHandle(fd, NULL, -2, -1, NULL);
    EXPECT_INT_NE(0, ret);

    ret = RaWaitEventHandle(fd, &event_info, -2, -1, NULL);
    EXPECT_INT_NE(0, ret);

    ret = RaWaitEventHandle(fd, &event_info, 0, -1, NULL);
    EXPECT_INT_NE(0, ret);

    ret = RaWaitEventHandle(fd, &event_info, 0, 1, NULL);
    EXPECT_INT_NE(0, ret);

    ret = RaWaitEventHandle(fd, &event_info, 0, 1, &events_num);
    EXPECT_INT_EQ(0, ret);

    mocker(RaPeerWaitEventHandle, 1024, -EINVAL);
    ret = RaWaitEventHandle(fd, &event_info, 0, 1, &events_num);
    EXPECT_INT_NE(0, ret);
    mocker_clean();
}

void tc_ra_destroy_event_handle(void)
{
    int ret;
    int fd;

    ret = RaDestroyEventHandle(NULL);
    EXPECT_INT_NE(0, ret);

    ret = RaDestroyEventHandle(&fd);
    EXPECT_INT_EQ(0, ret);

    mocker(RaPeerDestroyEventHandle, 1024, -EINVAL);
    ret = RaDestroyEventHandle(&fd);
    EXPECT_INT_NE(0, ret);
    mocker_clean();
}

int ra_hdc_poll_cq_stub(struct RaQpHandle *qp_hdc, bool is_send_cq, unsigned int num_entries, void *wc)
{
    if (is_send_cq) {
        return -1;
    }
    return 0;
}

void tc_ra_poll_cq(void)
{
    int ret;
    struct RaQpHandle qp_handle = {0};
    struct RaRdmaOps rdma_ops = {0};
    struct rdma_lite_wc_v2 lite_wc = {0};

    ret = RaPollCq(NULL, true, 0, NULL);
    EXPECT_INT_NE(0, ret);

    qp_handle.rdmaOps = &rdma_ops;
    rdma_ops.raPollCq = NULL;
    ret = RaPollCq(&qp_handle, true, 1, &lite_wc);
    EXPECT_INT_NE(0, ret);

    rdma_ops.raPollCq = ra_hdc_poll_cq_stub;
    ret = RaPollCq(&qp_handle, true, 1, &lite_wc);
    EXPECT_INT_NE(0, ret);

    ret = RaPollCq(&qp_handle, false, 1, &lite_wc);
    EXPECT_INT_EQ(0, ret);
}

void tc_get_vnic_ip_infos(void)
{
    int ret;
    unsigned int ids[1] = {0};
    unsigned int infos[1] = {0};

    ret = RaSocketGetVnicIpInfos(0, PHY_ID_VNIC_IP, NULL, 0, NULL);
    EXPECT_INT_NE(0, ret);

    ret = RaSocketGetVnicIpInfos(0xFFFF, PHY_ID_VNIC_IP, ids, 1, infos);
    EXPECT_INT_NE(0, ret);

    ret = RaSocketGetVnicIpInfos(0, PHY_ID_VNIC_IP, ids, 1, infos);
    EXPECT_INT_NE(0, ret);

    ret = RaSocketGetVnicIpInfos(0, 0xFFFF, ids, 1, infos);
    EXPECT_INT_NE(0, ret);

    g_interface_version = 0;
    mocker_invoke(RaHdcGetInterfaceVersion, ra_get_interface_version_stub, 100);
    ret = RaSocketGetVnicIpInfos(0, PHY_ID_VNIC_IP, ids, 1, infos);
    EXPECT_INT_NE(0, ret);
    mocker_clean();
}

int ra_socket_batch_abort_stub(unsigned int phyId, struct SocketConnectInfoT conn[], unsigned int num)
{
    return 0;
}

void tc_ra_socket_batch_abort(void)
{
    int ret;
    unsigned int phyId;
    struct SocketConnectInfoT conn = {0};
    struct RaSocketHandle socket_handle = {0};
    struct RaSocketOps socket_ops = {0};

    ret = RaSocketBatchAbort(NULL, 0);
    EXPECT_INT_EQ(128203, ret);

    conn.socketHandle = NULL;
    ret = RaSocketBatchAbort(&conn, 1);
    EXPECT_INT_EQ(128203, ret);

    socket_ops.raSocketBatchAbort = ra_socket_batch_abort_stub;
    socket_handle.socketOps = &socket_ops;
    socket_handle.rdevInfo.phyId = 16;
    conn.socketHandle = &socket_handle;
    ret = RaSocketBatchAbort(&conn, 1);
    EXPECT_INT_EQ(128203, ret);

    socket_handle.rdevInfo.phyId = 0;
    mocker(RaInetPton, 5, -1);
    ret = RaSocketBatchAbort(&conn, 1);
    EXPECT_INT_NE(0, ret);
    mocker_clean();

    mocker(RaInetPton, 5, 0);
    ret = RaSocketBatchAbort(&conn, 1);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();
}

void tc_ra_get_client_socket_err_info(void)
{
    int ret = 0;
    struct SocketConnectInfoT conn[10] = {0};
    struct SocketErrInfo err[10] = {0};
    unsigned int num = 1;
    struct RaSocketHandle *socket_handle = NULL;
    struct rdev rdev_info = {0};

    socket_handle = malloc(sizeof(struct RaSocketHandle));
    socket_handle->rdevInfo = rdev_info;
    mocker_clean();

    conn[0].socketHandle = NULL;
    ret = RaGetClientSocketErrInfo(conn, err, num);
    EXPECT_INT_EQ(128203, ret);

    conn[0].socketHandle = socket_handle;
    socket_handle->socketOps = &gRaPeerSocketOps;
    rdev_info.phyId = 0;
    mocker(RaInetPton, 5, 0);
    mocker(RaGetSocketConnectInfo, 1, 0);
    mocker(RsSetCtx, 1, 0);
    mocker(RsSocketGetClientSocketErrInfo, 1, 1);
    ret = RaGetClientSocketErrInfo(conn, err, num);
    EXPECT_INT_EQ(328207, ret);
    mocker_clean();

    mocker(RaInetPton, 5, 0);
    mocker(RaGetSocketConnectInfo, 1, 0);
    mocker(RsSetCtx, 1, 0);
    mocker(RsSocketGetClientSocketErrInfo, 1, 0);
    ret = RaGetClientSocketErrInfo(conn, err, num);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();

    free(socket_handle);
    socket_handle = NULL;
}

void tc_ra_get_server_socket_err_info(void)
{
    int ret = 0;
    struct SocketListenInfoT conn[10] = {0};
    struct ServerSocketErrInfo err[10] = {0};
    unsigned int num = 1;
    struct RaSocketHandle *socket_handle = NULL;
    struct rdev rdev_info = {0};

    socket_handle = malloc(sizeof(struct RaSocketHandle));
    socket_handle->rdevInfo = rdev_info;
    mocker_clean();

    conn[0].socketHandle = NULL;
    ret = RaGetServerSocketErrInfo(conn, err, num);
    EXPECT_INT_EQ(128203, ret);

    conn[0].socketHandle = socket_handle;
    socket_handle->socketOps = &gRaPeerSocketOps;
    rdev_info.phyId = 0;
    mocker(RaInetPton, 5, 0);
    mocker(RaGetSocketListenInfo, 1, 0);
    mocker(RsSetCtx, 1, 0);
    mocker(RsSocketGetServerSocketErrInfo, 1, 1);
    ret = RaGetServerSocketErrInfo(conn, err, num);
    EXPECT_INT_EQ(328207, ret);
    mocker_clean();

    mocker(RaInetPton, 5, 0);
    mocker(RaGetSocketListenInfo, 1, 0);
    mocker(RsSetCtx, 1, 0);
    mocker(RsSocketGetServerSocketErrInfo, 1, 0);
    ret = RaGetServerSocketErrInfo(conn, err, num);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();

    free(socket_handle);
    socket_handle = NULL;
}

void tc_ra_socket_accept_credit_add(void)
{
    struct SocketListenInfoT conn[10] = {0};
    struct RaSocketHandle socket_handle = {0};
    struct RaSocketOps socket_ops = {0};
    int ret = 0;

    conn[0].socketHandle = &socket_handle;
    socket_handle.socketOps = &socket_ops;
    socket_handle.socketOps->raSocketAcceptCreditAdd = RaPeerSocketAcceptCreditAdd;
    mocker(RaInetPton, 1, 0);
    mocker(RaGetSocketListenInfo, 1, 0);
    mocker(RsSetCtx, 1, 0);
    mocker(RaPeerMutexLock, 1, 0);
    mocker(RaPeerMutexUnlock, 1, 0);
    mocker(RsSocketAcceptCreditAdd, 1, 0);
    ret = RaSocketAcceptCreditAdd(conn, 1, 1);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();
}

void tc_ra_remap_mr(void)
{
    struct RaRdmaHandle rdma_handle = {0};
    struct MemRemapInfo info[1] = {0};
    struct RaRdmaOps rdma_ops = {0};
    int ret = 0;

    mocker_clean();
    rdma_handle.rdmaOps = &rdma_ops;
    rdma_handle.rdmaOps->raRemapMr = RaHdcRemapMr;
    mocker(RaHdcProcessMsg, 1, 0);
    ret = RaRemapMr((void *)&rdma_handle, info, 1);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();
}

void tc_ra_register_mr(void)
{
    struct RaRdmaHandle rdma_handle = {0};
    struct RaMrHandle *mr_handle = NULL;
    struct RaRdmaOps rdma_ops = {0};
    struct MrInfoT mr_info = {0};
    int ret = 0;

    mocker_clean();
    mocker(RaHdcProcessMsg, 2, 0);
    rdma_handle.rdmaOps = &rdma_ops;
    rdma_handle.rdmaOps->raRegisterMr = RaHdcTypicalMrReg;
    rdma_handle.rdmaOps->raDeregisterMr = RaHdcTypicalMrDereg;
    ret = RaRegisterMr((void *)&rdma_handle, &mr_info, (void **)&mr_handle);
    EXPECT_INT_EQ(0, ret);
    ret = RaDeregisterMr((void *)&rdma_handle, (void *)mr_handle);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();
}

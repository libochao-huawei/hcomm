/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "ut_dispatch.h"
#include <stdlib.h>
#include <errno.h>
#include "securec.h"
#include "ra_hdc.h"
#include "ra_hdc_rdma_notify.h"
#include "ra_hdc_rdma.h"
#include "ra_hdc_socket.h"
#include "ra_hdc_lite.h"
#include "ra_hdc_tlv.h"
#include "hccp.h"
#include "ra_comm.h"
#include "ra_rs_err.h"
#include "dsmi_common_interface.h"

#define RA_QP_TX_DEPTH         32767
#define TC_TLV_HDC_MSG_SIZE    (32 * 1024)
typedef uint32_t u32;
typedef uint16_t u16;
typedef unsigned long long u64;
typedef signed int s32;

extern int RaHdcInitApart(unsigned int phyId, unsigned int *logicId);
extern int RaHdcSessionClose(unsigned int phyId);
extern RaHdcProcessMsg(unsigned int opcode, unsigned int device_id, char *data, unsigned int data_size);
extern int RaHdcNotifyCfgSet(unsigned int phyId, unsigned long long va, unsigned long long size);
extern int RaHdcNotifyCfgGet(unsigned int phyId, unsigned long long *va, unsigned long long *size);
extern STATIC int RaHdcNotifyBaseAddrInit(unsigned int notifyType, unsigned int phyId, unsigned long long **notifyVa);
extern void RaHdcLiteSaveCqeErrInfo(struct RaQpHandle *qpHdc, unsigned int status);
extern struct rdma_lite_context *RaRdmaLiteAllocCtx(u8 phyId, struct dev_cap_info *cap);
extern struct rdma_lite_cq *RaRdmaLiteCreateCq(struct rdma_lite_context *liteCtx, struct rdma_lite_cq_attr *liteCqAttr);
extern struct rdma_lite_qp *RaRdmaLiteCreateQp(struct rdma_lite_context *liteCtx, struct rdma_lite_qp_attr *liteQpAttr);
extern int RaHdcGetLiteSupport(struct RaRdmaHandle *rdmaHandle, unsigned int phyId);
extern int RaHdcGetDrvLiteSupport(unsigned int phyId, bool enabled910aLite, unsigned int *support);
extern int DlDrvDeviceGetIndexByPhyId(uint32_t phyId, uint32_t *devIndex);
extern int DlHalMemCtl(int type, void *paramValue, size_t paramValueSize, void *outValue, size_t *outSizeRet);
extern int RaRdmaLiteSetQpSl(struct rdma_lite_qp *lite_qp, unsigned char sl);
extern int RaHdcLitePostSend(struct RaQpHandle *qp_hdc, struct LiteMrInfo *local_mr,
    struct LiteMrInfo *rem_mr, struct LiteSendWr *wr, struct SendWrRsp *wr_rsp);
extern void RaHdcGetOpcodeLiteSupport(unsigned int phyId, unsigned int supportFeature, int *support);
extern int RaHdcGetOpcodeVersion(unsigned int phyId, unsigned int interfaceOpcode,
    unsigned int *interfaceVersion);
extern void RaHdcLiteMutexDeinit(struct RaRdmaHandle *rdmaHandle);
extern void RaRdmaLiteFreeCtx(struct rdma_lite_context *liteCtx);
extern int DlHalSensorNodeUnregister(uint32_t devid, uint64_t handle);
extern int RaSensorNodeRegister(unsigned int phyId, struct RaRdmaHandle *rdmaHandle);
extern int RaHdcLiteMutexInit(struct RaRdmaHandle *rdmaHandle, unsigned int phyId);
extern int RaHdcLiteGetCqQpAttr(struct RaQpHandle *qpHdc, struct rdma_lite_cq_attr *liteSendCqAttr,
    struct rdma_lite_cq_attr *liteRecvCqAttr, struct rdma_lite_qp_attr *liteQpAttr);
extern int RaHdcLiteInitMemPool(struct RaRdmaHandle *rdmaHandle, struct RaQpHandle *qpHdc,
    struct rdma_lite_cq_attr *liteSendCqAttr, struct rdma_lite_cq_attr *liteRecvCqAttr,
    struct rdma_lite_qp_attr *liteQpAttr);
extern int RaRdmaLiteDestroyCq(struct rdma_lite_cq *liteCq);
extern void RaHdcLiteDeinitMemPool(struct RaRdmaHandle *rdmaHandle, struct RaQpHandle *qpHdc);
extern void RaHdcLiteQpAttrInit(struct RaQpHandle *qpHdc, struct rdma_lite_qp_attr *liteQpAttr,
    struct rdma_lite_qp_cap *cap);
extern int RaRdmaLiteInitMemPool(struct rdma_lite_context *lite_ctx, struct rdma_lite_mem_attr *lite_mem_attr);
extern int RaRdmaLiteDeinitMemPool(struct rdma_lite_context *lite_ctx, u32 mem_idx);
extern int RaHdcGetCqeErrInfoNum(struct RaRdmaHandle *rdmaHandle, unsigned int *num);
extern int RaRdmaLiteDestroyQp(struct rdma_lite_qp *liteQp);
extern int ra_hdc_get_tlv_recv_msg(struct TlvMsg *recv_msg, char *recv_data);
extern int RaRdmaLiteRestoreSnapshot(struct rdma_lite_context *liteCtx);
extern unsigned int RaRdmaLiteGetApiVersion(void);

struct MsgHead g_msg_tmp;
DLLEXPORT hdcError_t stub_drvHdcGetMsgBuffer_pid_error(struct drvHdcMsg *msg, int index,
                                             char **pBuf, int *pLen)
{
    g_msg_tmp.ret = -EPERM;
    *pBuf = &g_msg_tmp;
    *pLen = ((struct drvHdcMsgBuf *)(msg + 1))->len;
    return DRV_ERROR_NONE;
}

static unsigned int g_devid = 0;
static struct RaInitConfig g_config = {
        .phyId = 0,
        .nicPosition = NETWORK_OFFLINE,
        .hdcType = HDC_SERVICE_TYPE_RDMA,
};
DLLEXPORT drvError_t stub_session_connect(int peer_node, int peer_devid, HDC_CLIENT client, HDC_SESSION *session)
{
    static HDC_SESSION g_hdc_session = 1;
    *session = g_hdc_session;
    return 0;
}
static char g_drv_recv_msg[4096];
static int g_drv_recv_msg_len = 0;
static DLLEXPORT drvError_t stub_drvHdcGetMsgBuffer(struct drvHdcMsg *msg, int index, char **pBuf, int *pLen)
{
    *pBuf = g_drv_recv_msg;
    *pLen = g_drv_recv_msg_len;
    return 0;
}

extern int dsmi_get_device_ip_address(int device_id, int port_type, int port_id, ip_addr_t *ip_address,
                               ip_addr_t *mask_address);

DLLEXPORT drvError_t drvGetDevNum(unsigned int *num_dev);

int RaHdcGetAllVnic(unsigned int *vnic_ip, unsigned int num);

unsigned int g_interface_version = 0;

int stub_ra_get_interface_version(unsigned int phyId, unsigned int interface_opcode, unsigned int* interface_version)
{
    *interface_version = g_interface_version;
    return 0;
}

void tc_hdc_init()
{
    struct ProcessRaSign p_ra_sign;
    p_ra_sign.tgid = 0;
    struct RaInitConfig config = {
        .phyId = g_devid,
        .nicPosition = NETWORK_OFFLINE,
        .hdcType = HDC_SERVICE_TYPE_RDMA,
    };
    mocker((stub_fn_t)drvHdcClientCreate, 1, 0);
    mocker_invoke((stub_fn_t)drvHdcSessionConnect, (stub_fn_t)stub_session_connect, 1);
    mocker((stub_fn_t)drvHdcSetSessionReference, 1, 0);
    mocker((stub_fn_t)halHdcRecv, 2, 0);
    int ret = RaHdcInit(&config, p_ra_sign);
    EXPECT_INT_EQ(ret, 0);

    mocker((stub_fn_t)memset_s, 1, 0);
    ret = RaHdcInit(&config, p_ra_sign);
    EXPECT_INT_EQ(ret, -EEXIST);

    mocker((stub_fn_t)drvHdcSessionClose, 1, 0);
    mocker((stub_fn_t)drvHdcClientDestroy, 1, 0);
    ret = RaHdcDeinit(&config);
    EXPECT_INT_EQ(ret, 0);
}

void tc_hdc_test_env_init()
{
    struct ProcessRaSign p_ra_sign;
    p_ra_sign.tgid = 0;

    mocker((stub_fn_t)drvHdcClientCreate, 1, 0);
    mocker_invoke((stub_fn_t)drvHdcSessionConnect, (stub_fn_t)stub_session_connect, 1);
    mocker((stub_fn_t)drvHdcSetSessionReference, 1, 0);
    mocker((stub_fn_t)halHdcRecv, 10, 0);
    int ret = RaHdcInit(&g_config, p_ra_sign);
    EXPECT_INT_EQ(ret, 0);
}

void tc_hdc_test_env_deinit()
{
    mocker((stub_fn_t)drvHdcSessionClose, 1, 0);
    mocker((stub_fn_t)drvHdcClientDestroy, 1, 0);
    int ret = RaHdcDeinit(&g_config);
    EXPECT_INT_EQ(ret, 0);
	mocker_clean();
}

void tc_hdc_init_fail()
{
    mocker_clean();
    struct ProcessRaSign p_ra_sign;
    p_ra_sign.tgid = 0;
    struct RaInitConfig config = {
        .phyId = RA_MAX_PHY_ID_NUM,
        .nicPosition = NETWORK_OFFLINE,
        .hdcType = HDC_SERVICE_TYPE_RDMA,
    };
    mocker(RaHdcInitApart, 1, -1);
    int ret = RaHdcInit(&config, p_ra_sign);
    EXPECT_INT_NE(ret, 0);

    config.phyId = g_devid;

    mocker_clean();
    mocker((stub_fn_t)RaHdcInitApart, 1, -EINVAL);
    ret = RaHdcInit(&config, p_ra_sign);
    EXPECT_INT_EQ(ret, -EINVAL);

    mocker_clean();
    mocker((stub_fn_t)pthread_mutex_init, 1, -1);
    ret = RaHdcInit(&config, p_ra_sign);
    EXPECT_INT_EQ(ret, -ESYSFUNC);

    mocker_clean();
    mocker((stub_fn_t)drvHdcClientCreate, 1, 0);
    mocker((stub_fn_t)drvHdcSessionConnect, 1, -1);
    ret = RaHdcInit(&config, p_ra_sign);
    EXPECT_INT_EQ(ret, -EPERM);

    mocker_clean();
    mocker((stub_fn_t)drvHdcClientCreate, 1, 0);
    mocker_invoke((stub_fn_t)drvHdcSessionConnect, (stub_fn_t)stub_session_connect, 1);
    mocker((stub_fn_t)drvHdcSetSessionReference, 1, -1);
    ret = RaHdcInit(&config, p_ra_sign);
    EXPECT_INT_EQ(ret, -EPERM);
}

void tc_hdc_deinit_fail()
{
    struct RaInitConfig config = {
        .phyId = RA_MAX_PHY_ID_NUM,
        .nicPosition = NETWORK_OFFLINE,
        .hdcType = HDC_SERVICE_TYPE_RDMA,
    };
    int ret;

    mocker_clean();
    mocker((stub_fn_t)drvHdcSessionClose, 10, -10);
    mocker((stub_fn_t)drvHdcClientDestroy, 10, -10);
    mocker((stub_fn_t)pthread_mutex_destroy, 10, -10);
    mocker((stub_fn_t)calloc, 1, NULL);
    config.phyId = g_devid;
    ret = RaHdcDeinit(&config);
    EXPECT_INT_EQ(ret, -ESYSFUNC);
    mocker_clean();
}

void tc_hdc_socket_batch_connect()
{
    struct SocketConnectInfoT conn[1];
    mocker_clean();
    tc_hdc_test_env_init();

    mocker((stub_fn_t)RaGetSocketConnectInfo, 10, 0);
    int ret = RaHdcSocketBatchConnect(g_devid, conn, 1);
    EXPECT_INT_EQ(ret, 0);

    tc_hdc_test_env_deinit();

    mocker_clean();
    mocker((stub_fn_t)calloc, 1, NULL);
    ret = RaHdcSocketBatchConnect(g_devid, conn, 1);
    EXPECT_INT_EQ(ret, -ENOMEM);

    mocker_clean();
    mocker((stub_fn_t)RaGetSocketConnectInfo, 1, -1);
    ret = RaHdcSocketBatchConnect(g_devid, conn, 1);
    EXPECT_INT_EQ(ret, -1);

    mocker_clean();
    mocker((stub_fn_t)RaGetSocketConnectInfo, 1, 0);
    ret = RaHdcSocketBatchConnect(0, conn, 1);
    EXPECT_INT_EQ(ret, -22);

    mocker_clean();
}

void tc_hdc_socket_batch_close()
{
    struct SocketCloseInfoT conn[1] = {0};
    conn[0].fdHandle = calloc(sizeof(struct SocketHdcInfo), 1);
    ((struct SocketHdcInfo *)conn[0].fdHandle)->fd = 0;
    ((struct SocketHdcInfo *)conn[0].fdHandle)->phyId = 0;
    mocker_clean();
    tc_hdc_test_env_init();
    int ret = RaHdcSocketBatchClose(g_devid, conn, 1);
    EXPECT_INT_EQ(ret, 0);

    tc_hdc_test_env_deinit();

    conn[0].fdHandle = calloc(sizeof(struct SocketHdcInfo), 1);
    ((struct SocketHdcInfo *)conn[0].fdHandle)->fd = 0;
    ((struct SocketHdcInfo *)conn[0].fdHandle)->phyId = 0;
    mocker_clean();
    mocker((stub_fn_t)calloc, 10, NULL);
    ret = RaHdcSocketBatchClose(g_devid, conn, 1);
    EXPECT_INT_EQ(ret, -ENOMEM);
    mocker_clean();

    conn[0].fdHandle = calloc(sizeof(struct SocketHdcInfo), 1);
    ((struct SocketHdcInfo *)conn[0].fdHandle)->fd = 0;
    ((struct SocketHdcInfo *)conn[0].fdHandle)->phyId = 0;
    mocker((stub_fn_t)RaHdcProcessMsg, 10, -1);
    ret = RaHdcSocketBatchClose(g_devid, conn, 1);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();
}

void tc_hdc_socket_listen_start()
{
    int ret;
    struct SocketListenInfoT conn[1];
    mocker_clean();
    tc_hdc_test_env_init();
    mocker((stub_fn_t)RaHdcProcessMsg, 5, 0);
    mocker((stub_fn_t)RaGetSocketListenResult, 10, 0);
    ret = RaHdcSocketListenStart(g_devid, conn, 1);
    EXPECT_INT_EQ(ret, 0);

    tc_hdc_test_env_deinit();

    mocker_clean();
    mocker((stub_fn_t)RaGetSocketListenInfo, 1, -1);
    ret = RaHdcSocketListenStart(g_devid, conn, 1);
    EXPECT_INT_EQ(ret, -EINVAL);

    mocker_clean();
    mocker((stub_fn_t)RaGetSocketListenInfo, 1, 0);
    ret = RaHdcSocketListenStart(0, conn, 1);
    EXPECT_INT_EQ(ret, -EINVAL);

    mocker_clean();
    mocker((stub_fn_t)RaHdcProcessMsg, 1, 0);
    mocker((stub_fn_t)RaGetSocketListenResult, 10, -1);
    ret = RaHdcSocketListenStart(g_devid, conn, 1);
    EXPECT_INT_EQ(ret, -EINVAL);

    mocker_clean();
    mocker((stub_fn_t)RaGetSocketConnectInfo, 1, -1);
    ret = RaHdcSocketListenStart(g_devid, conn, 1);
    EXPECT_INT_EQ(ret, -EINVAL);
    mocker_clean();
}

void tc_hdc_socket_batch_abort()
{
    struct SocketListenInfoT conn[1];

    mocker_clean();
    tc_hdc_test_env_init();

    mocker((stub_fn_t)RaGetSocketConnectInfo, 10, 0);
    int ret = RaHdcSocketBatchAbort(g_devid, conn, 1);
    EXPECT_INT_EQ(ret, 0);

    tc_hdc_test_env_deinit();

    mocker_clean();
    mocker((stub_fn_t)calloc, 1, NULL);
    ret = RaHdcSocketBatchAbort(g_devid, conn, 1);
    EXPECT_INT_EQ(ret, -ENOMEM);

    mocker_clean();
    mocker((stub_fn_t)RaGetSocketConnectInfo, 1, -1);
    ret = RaHdcSocketBatchAbort(g_devid, conn, 1);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();
}

void tc_hdc_socket_listen_stop()
{
    struct SocketListenInfoT conn[1];
    mocker_clean();
    tc_hdc_test_env_init();
    int ret = RaHdcSocketListenStop(g_devid, conn, 1);
    EXPECT_INT_EQ(ret, 0);

    tc_hdc_test_env_deinit();

    mocker_clean();
    mocker((stub_fn_t)RaGetSocketConnectInfo, 1, -1);
    ret = RaHdcSocketListenStart(g_devid, conn, 1);
    EXPECT_INT_EQ(ret, -EINVAL);
    mocker_clean();

    mocker((stub_fn_t)RaGetSocketListenInfo, 1, -1);
    ret = RaHdcSocketListenStop(g_devid, conn, 1);
    EXPECT_INT_EQ(ret, -EINVAL);
    mocker_clean();

    mocker((stub_fn_t)RaHdcProcessMsg, 1, -1);
    ret = RaHdcSocketListenStop(g_devid, conn, 1);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();
}

void tc_hdc_get_sockets()
{
    struct SocketInfoT conn[1];
    conn[0].fdHandle = NULL;
    conn[0].socketHandle = calloc(sizeof(struct RaSocketHandle), 1);
    mocker_clean();
    tc_hdc_test_env_init();
    int ret = RaHdcGetSockets(g_devid, 0, conn, 1);
    EXPECT_INT_NE(ret, 0);
    tc_hdc_test_env_deinit();

    mocker_clean();
    mocker((stub_fn_t)calloc, 1, NULL);
    ret = RaHdcGetSockets(g_devid, 0, conn, 1);
    EXPECT_INT_EQ(ret, -ENOMEM);

    mocker_clean();
    mocker((stub_fn_t)RaHdcProcessMsg, 1, -1);
    ret = RaHdcGetSockets(g_devid, 0, conn, 1);
    EXPECT_INT_EQ(ret, -EINVAL);

    mocker_clean();
    mocker((stub_fn_t)memcpy_s, 1, -1);
    ret = RaHdcGetSockets(g_devid, 0, conn, 1);
    EXPECT_INT_EQ(ret, -EINVAL);
    mocker_clean();

    free(conn[0].socketHandle);
    if(conn[0].fdHandle != NULL) {
        free(conn[0].fdHandle);
        conn[0].fdHandle = NULL;
    }

}

void tc_hdc_socket_send()
{
    struct SocketHdcInfo socket_info;
    char send_buf[16] = {0};

    mocker_clean();
    tc_hdc_test_env_init();
    mocker_invoke((stub_fn_t)drvHdcGetMsgBuffer, (stub_fn_t)stub_drvHdcGetMsgBuffer, 10);
    struct MsgHead* test_head = (struct MsgHead*)g_drv_recv_msg;
    g_drv_recv_msg_len = sizeof(union OpSocketSendData) + sizeof(struct MsgHead);
    test_head->opcode = RA_RS_SOCKET_SEND;
    test_head->msgDataLen = sizeof(union OpSocketSendData);
    test_head->ret = 0;

    union OpSocketSendData* data = (union OpSocketSendData*)(g_drv_recv_msg + sizeof(struct MsgHead));
    data->rxData.realSendSize = 100;

    int ret = RaHdcSocketSend(g_devid, &socket_info, send_buf, sizeof(send_buf));
    EXPECT_INT_EQ(ret, 100);

    test_head->ret = -EAGAIN;
    ret = RaHdcSocketSend(g_devid, &socket_info, send_buf, sizeof(send_buf));
    EXPECT_INT_EQ(ret, -EAGAIN);

    mocker((stub_fn_t)drvHdcAddMsgBuffer, 10, 10);
    ret = RaHdcSocketSend(g_devid, &socket_info, send_buf, sizeof(send_buf));
    EXPECT_INT_EQ(ret, -EINVAL);
	mocker((stub_fn_t)RaHdcSessionClose, 10, 0);
    tc_hdc_test_env_deinit();

    char max_send_buf[SOCKET_SEND_MAXLEN + 1] = {0};
    mocker_clean();
    mocker((stub_fn_t)calloc, 1, NULL);
    ret = RaHdcSocketSend(g_devid, &socket_info, max_send_buf, sizeof(max_send_buf));
    EXPECT_INT_EQ(ret, -ENOMEM);

    mocker_clean();
    mocker((stub_fn_t)memcpy_s, 1, -1);
    ret = RaHdcSocketSend(g_devid, &socket_info, max_send_buf, sizeof(max_send_buf));
    EXPECT_INT_EQ(ret, -ESAFEFUNC);

}

void tc_hdc_socket_recv()
{
    struct SocketHdcInfo socket_info;
    char send_buf[16] = {0};

    mocker_clean();
    tc_hdc_test_env_init();
    mocker((stub_fn_t)RaHdcProcessMsg, 5, 0);
    mocker((stub_fn_t)memcpy_s, 10, 0);
    int ret = RaHdcSocketRecv(g_devid, &socket_info, send_buf, sizeof(send_buf));
    tc_hdc_test_env_deinit();

    mocker_clean();
    mocker((stub_fn_t)calloc, 1, NULL);
    ret = RaHdcSocketRecv(g_devid, &socket_info, send_buf, sizeof(send_buf));
    EXPECT_INT_EQ(ret, -ENOMEM);

    mocker_clean();
    mocker((stub_fn_t)RaHdcProcessMsg, 1, -1);
    ret = RaHdcSocketRecv(g_devid, &socket_info, send_buf, sizeof(send_buf));
    EXPECT_INT_EQ(ret, -1);

    mocker_clean();
    mocker((stub_fn_t)RaHdcProcessMsg, 5, 0);
    mocker((stub_fn_t)memcpy_s, 10, -1);
    ret = RaHdcSocketRecv(g_devid, &socket_info, send_buf, sizeof(send_buf));
}

void tc_hdc_qp_create_destroy()
{
    mocker_clean();
    tc_hdc_test_env_init();
    struct RaRdmaHandle rdma_handle = {0};
    void* qp_handle = NULL;
    rdma_handle.supportLite = 1;
	int ret = RaHdcQpCreate(&rdma_handle, 0, 0, &qp_handle);
    ASSERT_ADDR_NE(qp_handle, NULL);
    ret = RaHdcQpDestroy(qp_handle);
    EXPECT_INT_EQ(ret, 0);
    tc_hdc_test_env_deinit();

    mocker_clean();
    mocker((stub_fn_t)calloc, 1, NULL);
	ret = RaHdcQpCreate(&rdma_handle, 0, 0, &qp_handle);
    EXPECT_INT_EQ(ret, -ENOMEM);
    mocker_clean();

    mocker((stub_fn_t)RaHdcProcessMsg, 1, -1);
    ret = RaHdcQpCreate(&rdma_handle, 0, 0, &qp_handle);
    EXPECT_INT_EQ(ret, -1);
}

void tc_hdc_get_qp_status()
{
    mocker_clean();
    tc_hdc_test_env_init();
    struct RaRdmaHandle rdma_handle = {0};
    void* qp_handle = NULL;
    rdma_handle.supportLite = 1;
	int ret = RaHdcQpCreate(&rdma_handle, 0, 0, &qp_handle);
    ASSERT_ADDR_NE(qp_handle, NULL);
    int status = 0;
    ret = RaHdcGetQpStatus(qp_handle, &status);
    EXPECT_INT_EQ(ret, 0);
    ret = RaHdcQpDestroy(qp_handle);
    EXPECT_INT_EQ(ret, 0);
    tc_hdc_test_env_deinit();

    mocker_clean();
    struct RaQpHandle test_qp_handle;
    test_qp_handle.phyId = g_devid;
    mocker((stub_fn_t)calloc, 1, NULL);
    ret = RaHdcGetQpStatus(&test_qp_handle, &status);
    EXPECT_INT_NE(ret, 0);
}

void tc_hdc_qp_connect_async()
{
    mocker_clean();
    tc_hdc_test_env_init();
    struct RaRdmaHandle rdma_handle = {0};
    void* qp_handle = NULL;
	int ret = RaHdcQpCreate(&rdma_handle, 0, 0, &qp_handle);
    struct SocketHdcInfo socket_handle = {0};
    ASSERT_ADDR_NE(qp_handle, NULL);

    ret = RaHdcQpConnectAsync(qp_handle, &socket_handle);
    EXPECT_INT_EQ(ret, 0);
    ret = RaHdcQpDestroy(qp_handle);
    EXPECT_INT_EQ(ret, 0);
    tc_hdc_test_env_deinit();

    mocker_clean();
    mocker((stub_fn_t)calloc, 1, NULL);
    struct RaQpHandle test_qp_handle;
    ret = RaHdcQpConnectAsync(&test_qp_handle, &socket_handle);
    EXPECT_INT_NE(ret, 0);
}

void tc_hdc_mr_dereg()
{
    mocker_clean();
    tc_hdc_test_env_init();
    struct RaRdmaHandle rdma_handle = {0};
    void* qp_handle = NULL;
	int ret = RaHdcQpCreate(&rdma_handle, 0, 0, &qp_handle);
    char qp_reg[16] = {0};
    ASSERT_ADDR_NE(qp_handle, NULL);

    ret = RaHdcMrDereg(qp_handle, qp_reg);
    EXPECT_INT_EQ(ret, 0);
    ret = RaHdcQpDestroy(qp_handle);
    EXPECT_INT_EQ(ret, 0);
    tc_hdc_test_env_deinit();

    mocker_clean();
    mocker((stub_fn_t)calloc, 1, NULL);
    struct RaQpHandle test_qp_handle;
    ret = RaHdcMrDereg(&test_qp_handle, qp_reg);
    EXPECT_INT_NE(ret, 0);
}

int stub_ra_hdc_process_msg_wrlist(unsigned int opcode, int device_id, char *data, unsigned int data_size)
{
    union OpSendWrlistData *send_wrlist = (union OpSendWrlistData *)data;
    send_wrlist->rxData.completeNum = 1;
    return 0;
}

int stub_ra_hdc_process_msg_wrlist_1(unsigned int opcode, int device_id, char *data, unsigned int data_size)
{
    union OpSendWrlistData *send_wrlist = (union OpSendWrlistData *)data;
    send_wrlist->rxData.completeNum = 1;
    return -ENOENT;
}

int stub_ra_hdc_process_msg_wrlist_2(unsigned int opcode, int device_id, char *data, unsigned int data_size)
{
    union OpSendWrlistData *send_wrlist = (union OpSendWrlistData *)data;
    send_wrlist->rxData.completeNum = 1;
    return -3;
}

int stub_ra_hdc_process_msg_wrlist_3(unsigned int opcode, int device_id, char *data, unsigned int data_size)
{
    union OpSendWrlistData *send_wrlist = (union OpSendWrlistData *)data;
    send_wrlist->rxData.completeNum = 5;
    return 0;
}

void tc_hdc_send_wrlist_v2()
{
    mocker_clean();
    struct SendWrlistData wrlist[1];
    struct SendWrRsp op_rsp[1];
    struct WrlistSendCompleteNum wrlist_num;
    unsigned int complete_num = 0;
    wrlist_num.sendNum = 1;
    wrlist_num.completeNum = &complete_num;

    struct RaQpHandle handle;
    handle.qpMode = 1;

    mocker_invoke((stub_fn_t)RaHdcProcessMsg, (stub_fn_t)stub_ra_hdc_process_msg_wrlist, 1);
    g_interface_version = RA_RS_SEND_WRLIST_V2_VERSION;
    mocker_invoke((stub_fn_t)RaGetInterfaceVersion, (stub_fn_t)stub_ra_get_interface_version, 1);
    int ret = RaHdcSendWrlist(&handle, wrlist, op_rsp, wrlist_num);
    g_interface_version = 0;
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    mocker_invoke((stub_fn_t)RaHdcProcessMsg, (stub_fn_t)stub_ra_hdc_process_msg_wrlist_1, 1);
    g_interface_version = RA_RS_SEND_WRLIST_V2_VERSION;
    mocker_invoke((stub_fn_t)RaGetInterfaceVersion, (stub_fn_t)stub_ra_get_interface_version, 1);
    ret = RaHdcSendWrlist(&handle, wrlist, op_rsp, wrlist_num);
    g_interface_version = 0;
    EXPECT_INT_EQ(ret, -ENOENT);
    mocker_clean();

    mocker_invoke((stub_fn_t)RaHdcProcessMsg, (stub_fn_t)stub_ra_hdc_process_msg_wrlist_2, 1);
    g_interface_version = RA_RS_SEND_WRLIST_V2_VERSION;
    mocker_invoke((stub_fn_t)RaGetInterfaceVersion, (stub_fn_t)stub_ra_get_interface_version, 1);
    ret = RaHdcSendWrlist(&handle, wrlist, op_rsp, wrlist_num);
    g_interface_version = 0;
    EXPECT_INT_EQ(ret, -3);
    mocker_clean();

    mocker_invoke((stub_fn_t)RaHdcProcessMsg, (stub_fn_t)stub_ra_hdc_process_msg_wrlist_3, 1);
    g_interface_version = RA_RS_SEND_WRLIST_V2_VERSION;
    mocker_invoke((stub_fn_t)RaGetInterfaceVersion, (stub_fn_t)stub_ra_get_interface_version, 1);
    ret = RaHdcSendWrlist(&handle, wrlist, op_rsp, wrlist_num);
    g_interface_version = 0;
    EXPECT_INT_EQ(ret, -EINVAL);
    mocker_clean();

    mocker((stub_fn_t)memcpy_s, 1, -1);
    g_interface_version = RA_RS_SEND_WRLIST_V2_VERSION;
    mocker_invoke((stub_fn_t)RaGetInterfaceVersion, (stub_fn_t)stub_ra_get_interface_version, 1);
    ret = RaHdcSendWrlist(&handle, wrlist, op_rsp, wrlist_num);
    g_interface_version = 0;
    EXPECT_INT_EQ(ret, -ESAFEFUNC);
    mocker_clean();

    mocker((stub_fn_t)calloc, 1, NULL);
    g_interface_version = RA_RS_SEND_WRLIST_V2_VERSION;
    mocker_invoke((stub_fn_t)RaGetInterfaceVersion, (stub_fn_t)stub_ra_get_interface_version, 1);
    ret = RaHdcSendWrlist(&handle, wrlist, op_rsp, wrlist_num);
    g_interface_version = 0;
    EXPECT_INT_EQ(ret, -ENOMEM);
    mocker_clean();
}

void tc_hdc_send_wrlist()
{
    mocker_clean();
    struct SendWrlistData wrlist[1];
    struct SendWrRsp op_rsp[1];
    struct WrlistSendCompleteNum wrlist_num;
    unsigned int complete_num = 0;
    wrlist_num.sendNum = 1;
    wrlist_num.completeNum = &complete_num;

    struct RaQpHandle handle;
    handle.qpMode = 1;

    mocker_invoke((stub_fn_t)RaHdcProcessMsg, (stub_fn_t)stub_ra_hdc_process_msg_wrlist, 1);
    int ret = RaHdcSendWrlist(&handle, wrlist, op_rsp, wrlist_num);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    mocker_invoke((stub_fn_t)RaHdcProcessMsg, (stub_fn_t)stub_ra_hdc_process_msg_wrlist_1, 1);
    ret = RaHdcSendWrlist(&handle, wrlist, op_rsp, wrlist_num);
    EXPECT_INT_EQ(ret, -ENOENT);
    mocker_clean();

    mocker_invoke((stub_fn_t)RaHdcProcessMsg, (stub_fn_t)stub_ra_hdc_process_msg_wrlist_2, 1);
    ret = RaHdcSendWrlist(&handle, wrlist, op_rsp, wrlist_num);
    EXPECT_INT_EQ(ret, -3);
    mocker_clean();

    mocker_invoke((stub_fn_t)RaHdcProcessMsg, (stub_fn_t)stub_ra_hdc_process_msg_wrlist_3, 1);
    ret = RaHdcSendWrlist(&handle, wrlist, op_rsp, wrlist_num);
    EXPECT_INT_EQ(ret, -EINVAL);
    mocker_clean();

    mocker((stub_fn_t)memcpy_s, 1, -1);
    ret = RaHdcSendWrlist(&handle, wrlist, op_rsp, wrlist_num);
    EXPECT_INT_EQ(ret, -ESAFEFUNC);
    mocker_clean();

    mocker((stub_fn_t)calloc, 1, NULL);
    ret = RaHdcSendWrlist(&handle, wrlist, op_rsp, wrlist_num);
    EXPECT_INT_EQ(ret, -ENOMEM);
    mocker_clean();

    mocker((stub_fn_t)strcpy_s, 1, -1);
    ret = RaHdcSendPid(0, 0);
    EXPECT_INT_EQ(ret, -ESAFEFUNC);
    mocker_clean();

    mocker((stub_fn_t)RaHdcProcessMsg, 1, -1);
    ret = RaHdcSendPid(0, 0);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

    mocker((stub_fn_t)RaHdcProcessMsg, 1, -1);
    ret = RaHdcNotifyCfgSet(0, 0, 0);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

    mocker((stub_fn_t)RaHdcProcessMsg, 1, -1);
    unsigned long long va = 0;
    unsigned long long size = 0;
    ret = RaHdcNotifyCfgGet(0, &va, &size);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

    mocker((stub_fn_t)drvDeviceGetIndexByPhyId, 1, -1);
    unsigned long long logic_id = 0;
    ret = RaHdcInitApart(0, &logic_id);
    EXPECT_INT_EQ(ret, -ENODEV);
    mocker_clean();

    mocker((stub_fn_t)pthread_mutex_init, 1, -1);
    ret = RaHdcInitApart(0, &logic_id);
    EXPECT_INT_EQ(ret, -ESYSFUNC);
    mocker_clean();

    mocker((stub_fn_t)memcpy_s, 1, -1);
    char data = 0;
    ret = RaHdcProcessMsg(0, 0, &data, 0);
    EXPECT_INT_EQ(ret, -ESAFEFUNC);
    mocker_clean();

    tc_hdc_send_wrlist_v2();
}

void tc_hdc_send_wr()
{
    mocker_clean();
    tc_hdc_test_env_init();
    struct RaRdmaHandle rdma_handle = {0};
    void* qp_handle = NULL;
	int ret = RaHdcQpCreate(&rdma_handle, 0, 0, &qp_handle);
    struct SendWr wr = {0};
    struct SendWrRsp rsp = {0};

    ASSERT_ADDR_NE(qp_handle, NULL);

    ret = RaHdcSendWr(qp_handle, &wr, &rsp);
    EXPECT_INT_EQ(ret, 0);

    struct MsgHead* test_head = (struct MsgHead*)g_drv_recv_msg;
    test_head->ret = -ENOENT;
    test_head->opcode = RA_RS_SEND_WR;
    g_drv_recv_msg_len = sizeof(union OpSendWrData) + sizeof(struct MsgHead);
    mocker_invoke((stub_fn_t)drvHdcGetMsgBuffer, (stub_fn_t)stub_drvHdcGetMsgBuffer, 3);

    ret = RaHdcSendWr(qp_handle, &wr, &rsp);
    EXPECT_INT_EQ(ret, -ENOENT);

    test_head->ret = 0;
    test_head->opcode = RA_RS_QP_DESTROY;
    test_head->msgDataLen = sizeof(union OpQpDestroyData);
    g_drv_recv_msg_len = sizeof(union OpQpDestroyData) + sizeof(struct MsgHead);
    ret = RaHdcQpDestroy(qp_handle);
    EXPECT_INT_EQ(ret, 0);

    mocker_clean();
    mocker((stub_fn_t)drvHdcClientCreate, 1, 0);
    mocker_invoke((stub_fn_t)drvHdcSessionConnect, (stub_fn_t)stub_session_connect, 1);
    mocker((stub_fn_t)drvHdcSetSessionReference, 1, 0);
    mocker((stub_fn_t)halHdcRecv, 10, 0);
    tc_hdc_test_env_deinit();

    struct RaQpHandle test_qp_handle;
    mocker_clean();
    mocker((stub_fn_t)memcpy_s, 1, -1);
    ret = RaHdcSendWr(&test_qp_handle, &wr, &rsp);
    EXPECT_INT_EQ(ret, -ESAFEFUNC);

    mocker_clean();
    mocker((stub_fn_t)calloc, 1, NULL);
    ret = RaHdcSendWr(&test_qp_handle, &wr, &rsp);
    EXPECT_INT_NE(ret, 0);
}

void tc_hdc_get_notify_base_addr()
{
    mocker_clean();
    tc_hdc_test_env_init();
    struct RaRdmaHandle rdma_handle = {0};
    void* qp_handle = NULL;
	int ret = RaHdcQpCreate(&rdma_handle, 0, 0, &qp_handle);
    unsigned long long va;
    unsigned long long size;
    ASSERT_ADDR_NE(qp_handle, NULL);

    ret = RaHdcGetNotifyBaseAddr(qp_handle, &va, &size);
    EXPECT_INT_EQ(ret, 0);

    ret = RaHdcQpDestroy(qp_handle);
    EXPECT_INT_EQ(ret, 0);
    tc_hdc_test_env_deinit();

    struct RaQpHandle test_qp_handle;
    mocker_clean();
    mocker((stub_fn_t)calloc, 1, NULL);
    ret = RaHdcGetNotifyBaseAddr(&test_qp_handle, &va, &size);
    EXPECT_INT_NE(ret, 0);
}

void tc_hdc_socket_init()
{
    mocker_clean();
    tc_hdc_test_env_init();
    struct rdev rdev_info = {0};

    int ret = RaHdcSocketInit(rdev_info);
    EXPECT_INT_EQ(ret, 0);
    tc_hdc_test_env_deinit();

    mocker_clean();
    mocker((stub_fn_t)RaHdcGetAllVnic, 1, 0);
    mocker((stub_fn_t)memcpy_s, 1, -10);
    ret = RaHdcSocketInit(rdev_info);
    EXPECT_INT_EQ(ret, -ESAFEFUNC);

    mocker_clean();
    mocker((stub_fn_t)drvHdcAllocMsg, 1, -10);
    ret = RaHdcSocketInit(rdev_info);
    EXPECT_INT_EQ(ret, -EINVAL);
    mocker_clean();

    mocker((stub_fn_t)memset_s, 1, -1);
    ret = RaHdcSocketInit(rdev_info);
    EXPECT_INT_EQ(ret, -ESAFEFUNC);
    mocker_clean();

    mocker((stub_fn_t)drvGetDevNum, 1, -1);
    ret = RaHdcSocketInit(rdev_info);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

    mocker((stub_fn_t)RaHdcGetAllVnic, 1, -1);
    ret = RaHdcSocketInit(rdev_info);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();
}

void tc_hdc_socket_deinit()
{
    mocker_clean();
    tc_hdc_test_env_init();
    struct rdev rdev_info = {0};
    int ret = RaHdcSocketDeinit(rdev_info);
    EXPECT_INT_EQ(ret, 0);
    tc_hdc_test_env_deinit();

    mocker_clean();
    mocker((stub_fn_t)memset_s, 1, -10);
    ret = RaHdcSocketDeinit(rdev_info);
    EXPECT_INT_EQ(ret, -ESAFEFUNC);

    mocker_clean();
    mocker((stub_fn_t)memcpy_s, 1, -10);
    ret = RaHdcSocketDeinit(rdev_info);
    EXPECT_INT_EQ(ret, -ESAFEFUNC);

    mocker_clean();
    mocker((stub_fn_t)drvHdcAllocMsg, 1, -10);
    ret = RaHdcSocketDeinit(rdev_info);
    EXPECT_INT_EQ(ret, -EINVAL);
}

int stub_ra_hdc_process_rdev_init(unsigned int opcode, int device_id, char *data, unsigned int data_size)
{
    union OpRdevInitData *rdev_init_data;
    union OpLiteSupportData *lite_support_data;

    if (opcode == RA_RS_RDEV_INIT) {
        rdev_init_data = (union rdev_init_data *)data;
        rdev_init_data->rxData.rdevIndex = 0;
    } else if (opcode == RA_RS_GET_LITE_SUPPORT) {
        lite_support_data = (union OpLiteSupportData *)data;
        lite_support_data->rxData.supportLite = 1;
    }
    return 0;
}

void stub_ra_hdc_get_opcode_lite_support(unsigned int phyId, unsigned int support_feature, int *support)
{
    if (support_feature == 1) {
        *support = 1;
        return;
    }
}

int stub_ra_hdc_process_rdev_init_error(unsigned int opcode, int device_id, char *data, unsigned int data_size)
{
    union OpRdevInitData *rdev_init_data;

    if (opcode == RA_RS_RDEV_INIT) {
        rdev_init_data = (union rdev_init_data *)data;
        rdev_init_data->rxData.rdevIndex = 0;
    } else if (opcode == RA_RS_GET_LITE_SUPPORT) {
        return -EPROTONOSUPPORT;
    }

    return 0;
}

extern int DlHalGetDeviceInfo(uint32_t devId, int32_t moduleType, int32_t infoType, int64_t *value);
int stub_dl_hal_get_device_info(uint32_t devId, int32_t moduleType, int32_t infoType, int64_t *value)
{
    *value = (1 << 8);
    return 0;
}

void tc_hdc_rdev_init()
{
    mocker_clean();
    tc_hdc_test_env_init();
    struct rdev rdev_info = {0};
    unsigned int rdevIndex = 0;
    struct RaRdmaHandle rdma_handle = { 0 };
    mocker_invoke((stub_fn_t)RaHdcGetOpcodeLiteSupport, (stub_fn_t)stub_ra_hdc_get_opcode_lite_support, 100);
    mocker_invoke((stub_fn_t)RaHdcProcessMsg, (stub_fn_t)stub_ra_hdc_process_rdev_init, 100);
    int ret = RaHdcRdevInit(&rdma_handle, NOTIFY, rdev_info, &rdevIndex);
    EXPECT_INT_EQ(ret, 0);
    RaHdcRdevDeinit(&rdma_handle, NOTIFY);
    tc_hdc_test_env_deinit();

    mocker_clean();
	mocker((stub_fn_t)RaHdcNotifyBaseAddrInit, 1, 0);
    mocker((stub_fn_t)memcpy_s, 10, -10);
    ret = RaHdcRdevInit(&rdma_handle, NOTIFY, rdev_info, &rdevIndex);
    EXPECT_INT_EQ(ret, -ESAFEFUNC);

    mocker_clean();
	mocker((stub_fn_t)RaHdcNotifyBaseAddrInit, 1, 0);
    ret = RaHdcRdevInit(&rdma_handle, NOTIFY, rdev_info, &rdevIndex);
    EXPECT_INT_NE(ret, 0);

    mocker_clean();
	mocker((stub_fn_t)RaHdcNotifyBaseAddrInit, 1, 0);
    mocker((stub_fn_t)drvHdcAllocMsg, 1, -10);
    ret = RaHdcRdevInit(&rdma_handle, NOTIFY, rdev_info, &rdevIndex);
    EXPECT_INT_EQ(ret, -EINVAL);

	mocker_clean();
	mocker((stub_fn_t)RaHdcNotifyBaseAddrInit, 1, 0);
    mocker((stub_fn_t)RaHdcProcessMsg, 1, -1);
	mocker((stub_fn_t)halMemFree, 1, -1);
    ret = RaHdcRdevInit(&rdma_handle, NOTIFY, rdev_info, &rdevIndex);
    EXPECT_INT_EQ(ret, -1);
	mocker_clean();

    mocker_invoke((stub_fn_t)RaHdcGetOpcodeLiteSupport, (stub_fn_t)stub_ra_hdc_get_opcode_lite_support, 100);
    mocker_invoke((stub_fn_t)RaHdcProcessMsg, (stub_fn_t)stub_ra_hdc_process_rdev_init_error, 10);
    ret = RaHdcRdevInit(&rdma_handle, NOTIFY, rdev_info, &rdevIndex);
    EXPECT_INT_EQ(ret, 0);
    RaHdcRdevDeinit(&rdma_handle, NOTIFY);
    mocker_clean();

    mocker_invoke((stub_fn_t)RaHdcGetOpcodeLiteSupport, (stub_fn_t)stub_ra_hdc_get_opcode_lite_support, 100);
    mocker_invoke((stub_fn_t)RaHdcProcessMsg, (stub_fn_t)stub_ra_hdc_process_rdev_init, 100);
    mocker((stub_fn_t)RaRdmaLiteAllocCtx, 1, 0);
    ret = RaHdcRdevInit(&rdma_handle, NOTIFY, rdev_info, &rdevIndex);
    EXPECT_INT_EQ(ret, -EFAULT);
    mocker_clean();

    mocker_invoke((stub_fn_t)RaHdcGetOpcodeLiteSupport, (stub_fn_t)stub_ra_hdc_get_opcode_lite_support, 100);
    mocker_invoke((stub_fn_t)RaHdcProcessMsg, (stub_fn_t)stub_ra_hdc_process_rdev_init, 100);
    mocker((stub_fn_t)pthread_mutex_init, 1, -1);
    ret = RaHdcRdevInit(&rdma_handle, NOTIFY, rdev_info, &rdevIndex);
    EXPECT_INT_EQ(ret, -ESYSFUNC);
    mocker_clean();

    mocker_invoke((stub_fn_t)RaHdcGetOpcodeLiteSupport, (stub_fn_t)stub_ra_hdc_get_opcode_lite_support, 100);
    mocker_invoke((stub_fn_t)RaHdcProcessMsg, (stub_fn_t)stub_ra_hdc_process_rdev_init, 100);
    mocker((stub_fn_t)pthread_create, 1, -1);
    ret = RaHdcRdevInit(&rdma_handle, NOTIFY, rdev_info, &rdevIndex);
    EXPECT_INT_EQ(ret, -ESYSFUNC);
    mocker_clean();

    mocker_invoke((stub_fn_t)RaHdcGetOpcodeLiteSupport, (stub_fn_t)stub_ra_hdc_get_opcode_lite_support, 100);
    mocker_invoke((stub_fn_t)RaHdcProcessMsg, (stub_fn_t)stub_ra_hdc_process_rdev_init, 100);
    mocker((stub_fn_t)RaHdcGetLiteSupport, 1, -1);
    ret = RaHdcRdevInit(&rdma_handle, NOTIFY, rdev_info, &rdevIndex);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

    unsigned int support = 0;
    mocker((stub_fn_t)DlDrvDeviceGetIndexByPhyId, 1, 0);
    mocker((stub_fn_t)DlHalMemCtl, 1, 0);
    RaHdcGetDrvLiteSupport(0, true, &support);
    EXPECT_INT_EQ(support, 0);
    mocker_clean();

    mocker((stub_fn_t)DlDrvDeviceGetIndexByPhyId, 1, 0);
    mocker_invoke((stub_fn_t)DlHalGetDeviceInfo, (stub_fn_t)stub_dl_hal_get_device_info, 10);
    mocker((stub_fn_t)DlHalMemCtl, 1, 0);
    RaHdcGetDrvLiteSupport(0, false, &support);
    EXPECT_INT_EQ(support, 0);
    mocker_clean();
}

void tc_hdc_rdev_deinit()
{
    mocker_clean();
    tc_hdc_test_env_init();
    struct RaRdmaHandle rdma_handle = {0};
    int ret = RaHdcRdevDeinit(&rdma_handle, NOTIFY);
    EXPECT_INT_EQ(ret, 0);
    tc_hdc_test_env_deinit();

	mocker((stub_fn_t)RaHdcProcessMsg, 1, 0);
	mocker((stub_fn_t)RaHdcNotifyCfgGet, 1, -1);
	ret = RaHdcRdevDeinit(&rdma_handle, NOTIFY);
	EXPECT_INT_EQ(ret, -1);
	mocker_clean();

	mocker((stub_fn_t)RaHdcProcessMsg, 1, 0);
	mocker((stub_fn_t)RaHdcNotifyCfgGet, 1, 0);
	mocker((stub_fn_t)halMemFree, 1, -2);
	ret = RaHdcRdevDeinit(&rdma_handle, NOTIFY);
	EXPECT_INT_EQ(ret, -2);
	mocker_clean();
}

void tc_hdc_socket_white_list_add_v2()
{
    mocker_clean();
    tc_hdc_test_env_init();
    struct rdev rdev_info = {0};
    struct SocketWlistInfoT white_list[1] = {{0}};
    g_interface_version = RA_RS_WLIST_ADD_V2_VERSION;
    mocker_invoke((stub_fn_t)RaGetInterfaceVersion, (stub_fn_t)stub_ra_get_interface_version, 1);
    int ret = RaHdcSocketWhiteListAdd(rdev_info, white_list, 1);
    g_interface_version = 0;
    EXPECT_INT_EQ(ret, 0);
    tc_hdc_test_env_deinit();
}

void tc_hdc_socket_white_list_add()
{
    mocker_clean();
    tc_hdc_test_env_init();
    struct rdev rdev_info = {0};
    struct SocketWlistInfoT white_list[1] = {{0}};
    int ret = RaHdcSocketWhiteListAdd(rdev_info, white_list, 1);
    EXPECT_INT_EQ(ret, 0);
    tc_hdc_test_env_deinit();

    tc_hdc_socket_white_list_add_v2();
}

void tc_hdc_socket_white_list_del_v2()
{
    mocker_clean();
    tc_hdc_test_env_init();
    struct rdev rdev_info = {0};
    struct SocketWlistInfoT white_list[1] = {{0}};
    int ret = RaHdcSocketWhiteListDel(rdev_info, white_list, 1);
    EXPECT_INT_EQ(ret, 0);
    tc_hdc_test_env_deinit();

    mocker_clean();
    mocker((stub_fn_t)calloc, 1, NULL);
    g_interface_version = RA_RS_WLIST_DEL_V2_VERSION;
    mocker_invoke((stub_fn_t)RaGetInterfaceVersion, (stub_fn_t)stub_ra_get_interface_version, 1);
    ret = RaHdcSocketWhiteListDel(rdev_info, white_list, 1);
    g_interface_version = 0;
    EXPECT_INT_EQ(ret, -ENOMEM);

    mocker_clean();
    mocker((stub_fn_t)memcpy_s, 1, -1);
    g_interface_version = RA_RS_WLIST_DEL_V2_VERSION;
    mocker_invoke((stub_fn_t)RaGetInterfaceVersion, (stub_fn_t)stub_ra_get_interface_version, 1);
    ret = RaHdcSocketWhiteListDel(rdev_info, white_list, 1);
    g_interface_version = 0;
    EXPECT_INT_EQ(ret, -ESAFEFUNC);

    mocker_clean();
    mocker((stub_fn_t)drvHdcAllocMsg, 1, -10);
    g_interface_version = RA_RS_WLIST_DEL_V2_VERSION;
    mocker_invoke((stub_fn_t)RaGetInterfaceVersion, (stub_fn_t)stub_ra_get_interface_version, 1);
    ret = RaHdcSocketWhiteListDel(rdev_info, white_list, 1);
    g_interface_version = 0;
    EXPECT_INT_EQ(ret, -EINVAL);
    mocker_clean();
}

void tc_hdc_socket_white_list_del()
{
    mocker_clean();
    tc_hdc_test_env_init();
    struct rdev rdev_info = {0};
    struct SocketWlistInfoT white_list[1] = {{0}};
    int ret = RaHdcSocketWhiteListDel(rdev_info, white_list, 1);
    EXPECT_INT_EQ(ret, 0);
    tc_hdc_test_env_deinit();

    mocker_clean();
    mocker((stub_fn_t)calloc, 1, NULL);
    ret = RaHdcSocketWhiteListDel(rdev_info, white_list, 1);
    EXPECT_INT_EQ(ret, -ENOMEM);

    mocker_clean();
    mocker((stub_fn_t)memcpy_s, 1, -1);
    ret = RaHdcSocketWhiteListDel(rdev_info, white_list, 1);
    EXPECT_INT_EQ(ret, -ESAFEFUNC);

    mocker_clean();
    mocker((stub_fn_t)drvHdcAllocMsg, 1, -10);
    ret = RaHdcSocketWhiteListDel(rdev_info, white_list, 1);
    EXPECT_INT_EQ(ret, -EINVAL);
    mocker_clean();

    tc_hdc_socket_white_list_del_v2();
}

void tc_hdc_get_ifaddrs()
{
    mocker_clean();
    tc_hdc_test_env_init();
    struct IfaddrInfo infos[1] = {{0}};
    unsigned int num = 1;
    int ret = RaHdcGetIfaddrs(0, infos, &num);
    EXPECT_INT_EQ(ret, 0);
    tc_hdc_test_env_deinit();
    mocker(drvDeviceGetPhyIdByIndex, 1, -1);
    ret = RaHdcGetIfaddrs(0, infos, &num);
    EXPECT_INT_EQ(ret, -EINVAL);
}

void tc_hdc_get_ifaddrs_v2()
{
    mocker_clean();
    tc_hdc_test_env_init();
    struct InterfaceInfo infos[1] = {{0}};
    unsigned int num = 1;
    int ret = RaHdcGetIfaddrsV2(0, 0, infos, &num);
    EXPECT_INT_EQ(ret, 0);
    tc_hdc_test_env_deinit();
    mocker(drvDeviceGetPhyIdByIndex, 1, -1);
    ret = RaHdcGetIfaddrsV2(0, 0, infos, &num);
    EXPECT_INT_EQ(ret, -EINVAL);
}

void tc_hdc_get_ifnum()
{
    mocker_clean();
    tc_hdc_test_env_init();

    unsigned int num = 1;
    int ret = RaHdcGetIfnum(0, 0, &num);
    EXPECT_INT_EQ(ret, 0);
    tc_hdc_test_env_deinit();

    mocker(drvDeviceGetPhyIdByIndex, 1, -1);
    ret = RaHdcGetIfnum(0, 0, &num);
    EXPECT_INT_EQ(ret, -EINVAL);
}

void tc_hdc_message_process_fail()
{
    mocker_clean();
    struct ProcessRaSign p_ra_sign;
    p_ra_sign.tgid = 0;
    struct RaInitConfig config = {
        .phyId = g_devid,
        .nicPosition = NETWORK_OFFLINE,
        .hdcType = HDC_SERVICE_TYPE_RDMA,
    };
    mocker((stub_fn_t)drvHdcClientCreate, 1, 0);
    mocker_invoke((stub_fn_t)drvHdcSessionConnect, (stub_fn_t)stub_session_connect, 1);
    mocker((stub_fn_t)drvHdcSetSessionReference, 1, 0);
    mocker((stub_fn_t)halHdcRecv, 2, 0);
    int ret = RaHdcInit(&config, p_ra_sign);
    EXPECT_INT_EQ(ret, 0);

    struct IfaddrInfo infos[1] = {{0}};
    unsigned int num = 1;
    mocker_clean();
    mocker((stub_fn_t)calloc, 1, NULL);
    ret = RaHdcGetIfaddrs(0, infos, &num);
    EXPECT_INT_EQ(ret, -ENOMEM);

    mocker_clean();
    mocker((stub_fn_t)memcpy_s, 1, -1);
    ret = RaHdcGetIfaddrs(0, infos, &num);
    EXPECT_INT_EQ(ret, -ESAFEFUNC);

    mocker_clean();
    mocker((stub_fn_t)drvHdcAllocMsg, 1, -10);
    ret = RaHdcGetIfaddrs(0, infos, &num);
    EXPECT_INT_EQ(ret, -10);

    mocker_clean();
    mocker((stub_fn_t)drvHdcAddMsgBuffer, 1, -10);
    ret = RaHdcGetIfaddrs(0, infos, &num);
    EXPECT_INT_EQ(ret, -10);

    mocker_clean();
    mocker((stub_fn_t)halHdcSend, 1, -10);
    ret = RaHdcGetIfaddrs(0, infos, &num);
    EXPECT_INT_EQ(ret, -10);

    mocker_clean();
    mocker((stub_fn_t)drvHdcReuseMsg, 1, -10);
    ret = RaHdcGetIfaddrs(0, infos, &num);
    EXPECT_INT_EQ(ret, -10);

    mocker_clean();
    mocker((stub_fn_t)halHdcRecv, 1, -10);
    ret = RaHdcGetIfaddrs(0, infos, &num);
    EXPECT_INT_EQ(ret, -10);

    mocker_clean();
    mocker((stub_fn_t)halHdcRecv, 1, 0);
    mocker((stub_fn_t)drvHdcGetMsgBuffer, 1, -10);
    ret = RaHdcGetIfaddrs(0, infos, &num);
    EXPECT_INT_EQ(ret, -10);

    mocker_clean();
    mocker((stub_fn_t)halHdcRecv, 1, 0);
	tc_hdc_test_env_deinit();
}

void tc_hdc_socket_recv_fail()
{
    struct SocketHdcInfo socket_info;
    char send_buf[16] = {0};

    mocker_clean();
    tc_hdc_test_env_init();

    mocker_invoke((stub_fn_t)drvHdcGetMsgBuffer, (stub_fn_t)stub_drvHdcGetMsgBuffer, 2);
    struct MsgHead* test_head = (struct MsgHead*)g_drv_recv_msg;
    test_head->ret = -EAGAIN;
    test_head->opcode = RA_RS_SOCKET_RECV;
    g_drv_recv_msg_len = sizeof(union OpSocketRecvData) + sizeof(send_buf) + sizeof(struct MsgHead);
    int ret = RaHdcSocketRecv(g_devid, &socket_info, send_buf, sizeof(send_buf));
    EXPECT_INT_EQ(ret, -EAGAIN);

    test_head->ret = -100;
    ret = RaHdcSocketRecv(g_devid, &socket_info, send_buf, sizeof(send_buf));
    EXPECT_INT_EQ(ret, -100);

    mocker((stub_fn_t)drvHdcAddMsgBuffer, 10, 10);
    ret = RaHdcSocketRecv(g_devid, &socket_info, send_buf, sizeof(send_buf));
    EXPECT_INT_EQ(ret, -EINVAL);
	mocker((stub_fn_t)RaHdcSessionClose, 10, 0);
    tc_hdc_test_env_deinit();
}

void tc_ra_hdc_send_wrlist_ext_init_v2()
{
    union OpSendWrlistDataExtV2 send_wrlist;
    struct RaQpHandle qp_hdc;
    unsigned int complete_cnt;
    struct WrlistSendCompleteNum wrlist_num;
    RaHdcSendWrlistExtInitV2(&send_wrlist, &qp_hdc, complete_cnt, wrlist_num);
}

void tc_ra_hdc_send_wrlist_ext_init()
{
    union OpSendWrlistDataExt send_wrlist;
    struct RaQpHandle qp_hdc;
    unsigned int complete_cnt;
    struct WrlistSendCompleteNum wrlist_num;
    RaHdcSendWrlistExtInit(&send_wrlist, &qp_hdc, complete_cnt, wrlist_num);

    tc_ra_hdc_send_wrlist_ext_init_v2();
}

void tc_ra_hdc_send_wrlist_ext_v2()
{
    mocker_clean();

    struct RaQpHandle qp_handle;
    int ret;
    struct SendWrlistDataExt wr[1];
    struct SendWrRsp op_rsp[1];
    struct WrlistSendCompleteNum wrlist_num;
    unsigned int complete_num = 0;
    wrlist_num.sendNum = 1;
    wrlist_num.completeNum = &complete_num;

    struct RaQpHandle handle;
    handle.qpMode = 1;

    mocker_invoke((stub_fn_t)RaHdcProcessMsg, (stub_fn_t)stub_ra_hdc_process_msg_wrlist, 1);
    g_interface_version = RA_RS_SEND_WRLIST_EXT_V2_VERSION;
    mocker_invoke((stub_fn_t)RaGetInterfaceVersion, (stub_fn_t)stub_ra_get_interface_version, 1);
    RaHdcSendWrlistExt(&qp_handle, wr, op_rsp, wrlist_num);
    g_interface_version = 0;
    mocker_clean();

    mocker_invoke((stub_fn_t)RaHdcProcessMsg, (stub_fn_t)stub_ra_hdc_process_msg_wrlist_3, 1);
    g_interface_version = RA_RS_SEND_WRLIST_EXT_V2_VERSION;
    mocker_invoke((stub_fn_t)RaGetInterfaceVersion, (stub_fn_t)stub_ra_get_interface_version, 1);
    ret = RaHdcSendWrlistExt(&qp_handle, wr, op_rsp, wrlist_num);
    g_interface_version = 0;
    EXPECT_INT_EQ(ret, -EINVAL);
    mocker_clean();

    mocker((stub_fn_t)memcpy_s, 1, -1);
    g_interface_version = RA_RS_SEND_WRLIST_EXT_V2_VERSION;
    mocker_invoke((stub_fn_t)RaGetInterfaceVersion, (stub_fn_t)stub_ra_get_interface_version, 1);
    ret = RaHdcSendWrlistExt(&qp_handle, wr, op_rsp, wrlist_num);
    g_interface_version = 0;
    EXPECT_INT_EQ(ret, -ESAFEFUNC);
    mocker_clean();
}

void tc_ra_hdc_send_wrlist_ext()
{
    mocker_clean();

    struct RaQpHandle qp_handle;
    int ret;
    struct SendWrlistDataExt wr[1];
    struct SendWrRsp op_rsp[1];
    struct WrlistSendCompleteNum wrlist_num;
    unsigned int complete_num = 0;
    wrlist_num.sendNum = 1;
    wrlist_num.completeNum = &complete_num;

    struct RaQpHandle handle;
    handle.qpMode = 1;

    mocker_invoke((stub_fn_t)RaHdcProcessMsg, (stub_fn_t)stub_ra_hdc_process_msg_wrlist, 1);
    RaHdcSendWrlistExt(&qp_handle, wr, op_rsp, wrlist_num);
    mocker_clean();

    mocker_invoke((stub_fn_t)RaHdcProcessMsg, (stub_fn_t)stub_ra_hdc_process_msg_wrlist_3, 1);
    ret = RaHdcSendWrlistExt(&qp_handle, wr, op_rsp, wrlist_num);
    EXPECT_INT_EQ(ret, -EINVAL);
    mocker_clean();

    tc_ra_hdc_send_wrlist_ext_v2();
}

void tc_ra_hdc_send_normal_wrlist()
{
    struct RaQpHandle qp_handle;
    struct WrInfo wr[1];
    struct SendWrRsp op_rsp[1];
    struct WrlistSendCompleteNum wrlist_num = { 0 };
    unsigned int complete_num = 0;
    wrlist_num.sendNum = 1;
    wrlist_num.completeNum = &complete_num;
    int ret = 0;

    mocker_clean();

    qp_handle.qpMode = RA_RS_OP_QP_MODE;
    qp_handle.supportLite = 1;
    RaHdcSendNormalWrlist(&qp_handle, wr, op_rsp, wrlist_num);

    qp_handle.qpMode = RA_RS_NOR_QP_MODE;
    wrlist_num.completeNum = &complete_num;
    mocker_invoke((stub_fn_t)RaHdcProcessMsg, (stub_fn_t)stub_ra_hdc_process_msg_wrlist, 1);
    ret = RaHdcSendNormalWrlist(&qp_handle, wr, op_rsp, wrlist_num);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

void tc_ra_hdc_set_qp_attr_qos()
{
    struct QosAttr attr = {0};
    attr.tc = 33 * 4;
    attr.sl = 4;

    mocker_clean();
    tc_hdc_test_env_init();
    struct RaRdmaHandle rdma_handle = {0};
    void* qp_handle = NULL;
	int ret = RaHdcQpCreate(&rdma_handle, 0, 0, &qp_handle);
    char qp_reg[16] = {0};
    ASSERT_ADDR_NE(qp_handle, NULL);

    ret = RaHdcSetQpAttrQos(qp_handle, &attr);
    EXPECT_INT_EQ(ret, 0);
    ret = RaHdcQpDestroy(qp_handle);
    EXPECT_INT_EQ(ret, 0);
    tc_hdc_test_env_deinit();

    mocker_clean();
    mocker((stub_fn_t)calloc, 1, NULL);
    struct RaQpHandle test_qp_handle;
    ret = RaHdcSetQpAttrQos(&test_qp_handle, &attr);
    EXPECT_INT_NE(ret, 0);
    mocker_clean();
}

void tc_ra_hdc_set_qp_attr_timeout()
{
    unsigned int timeout = 15;

    mocker_clean();
    tc_hdc_test_env_init();
    struct RaRdmaHandle rdma_handle = {0};
    void* qp_handle = NULL;
	int ret = RaHdcQpCreate(&rdma_handle, 0, 0, &qp_handle);
    char qp_reg[16] = {0};
    ASSERT_ADDR_NE(qp_handle, NULL);

    ret = RaHdcSetQpAttrTimeout(qp_handle, &timeout);
    EXPECT_INT_EQ(ret, 0);
    ret = RaHdcQpDestroy(qp_handle);
    EXPECT_INT_EQ(ret, 0);
    tc_hdc_test_env_deinit();

    mocker_clean();
    mocker((stub_fn_t)calloc, 1, NULL);
    struct RaQpHandle test_qp_handle;
    ret = RaHdcSetQpAttrTimeout(&test_qp_handle, &timeout);
    EXPECT_INT_NE(ret, 0);
    mocker_clean();
}

void tc_ra_hdc_set_qp_attr_retry_cnt()
{
    unsigned int retry_cnt = 6;

    mocker_clean();
    tc_hdc_test_env_init();
    struct RaRdmaHandle rdma_handle = {0};
    void* qp_handle = NULL;
	int ret = RaHdcQpCreate(&rdma_handle, 0, 0, &qp_handle);
    char qp_reg[16] = {0};
    ASSERT_ADDR_NE(qp_handle, NULL);

    ret = RaHdcSetQpAttrRetryCnt(qp_handle, &retry_cnt);
    EXPECT_INT_EQ(ret, 0);
    ret = RaHdcQpDestroy(qp_handle);
    EXPECT_INT_EQ(ret, 0);
    tc_hdc_test_env_deinit();

    mocker_clean();
    mocker((stub_fn_t)calloc, 1, NULL);
    struct RaQpHandle test_qp_handle;
    ret = RaHdcSetQpAttrRetryCnt(&test_qp_handle, &retry_cnt);
    EXPECT_INT_NE(ret, 0);
    mocker_clean();
}

void tc_ra_hdc_get_cqe_err_info()
{
    mocker_clean();
    tc_hdc_test_env_init();
    int ret;
    struct CqeErrInfo info = {0};
    ret = RaHdcGetCqeErrInfo(0, &info);
    EXPECT_INT_EQ(ret, 0);

    struct RaQpHandle qp_hdc;
    qp_hdc.phyId = 0;
    RaHdcLiteSaveCqeErrInfo(&qp_hdc, 0x12);
    ret = RaHdcGetCqeErrInfo(0, &info);
    EXPECT_INT_EQ(ret, 0);
    EXPECT_INT_EQ(info.status, 0x12);
    RaHdcLiteSaveCqeErrInfo(&qp_hdc, 0x15);
    ret = RaHdcGetCqeErrInfo(0, &info);
    EXPECT_INT_EQ(ret, 0);
    EXPECT_INT_EQ(info.status, 0x15);
    tc_hdc_test_env_deinit();

    ret = RaHdcGetCqeErrInfo(0, &info);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

int stub_ra_hdc_get_cqe_err_num(unsigned int opcode, int device_id, char *data, unsigned int data_size)
{
    if (data_size == sizeof(union OpGetCqeErrInfoNumData)) {
        union OpGetCqeErrInfoNumData *cqe_err_info_num_data = (union OpGetCqeErrInfoNumData *)data;
        cqe_err_info_num_data->rxData.num = 10;
    } else if (data_size == sizeof(union OpGetCqeErrInfoListData)) {
        union OpGetCqeErrInfoListData *cqe_err_info_list =
            (union op_get_cqe_eop_get_cqe_err_info_list_datarr_info_num_data *)data;
        cqe_err_info_list->rxData.num = 1;
    }
    return 0;
}

int stub_ra_hdc_get_cqe_err_num_v2(unsigned int opcode, int device_id, char *data, unsigned int data_size)
{
    stub_ra_hdc_get_cqe_err_num(opcode, device_id, data, data_size);
    ((union OpGetCqeErrInfoListData *)data)->rxData.infoList[0].qpn = 12345;
    return 0;
}

int stub_ra_hdc_get_cqe_err_info_num(struct RaRdmaHandle *rdma_handle, unsigned int *num)
{
    *num = 10;
    return 0;
}

int stub_ra_hdc_get_interface_version(unsigned int phyId, unsigned int interface_opcode, unsigned int *interface_version)
{
    *interface_version = 1;
    return 0;
}

int stub_ra_hdc_lite_get_cqe_err_info_list(struct RaRdmaHandle *rdma_handle, struct CqeErrInfo *info_list,
    unsigned int *num)
{
    *num = 10;
    return 0;
}

void tc_ra_hdc_get_cqe_err_info_list()
{
    struct RaRdmaHandle rdma_handle = {0};
    struct RaQpHandle qp_hdc = {0};
    struct CqeErrInfo info[130] = {0};
    int num = 11;
    int ret = 0;

    mocker_clean();
    mocker_invoke(RaHdcLiteGetCqeErrInfoList, stub_ra_hdc_lite_get_cqe_err_info_list, 1);
    mocker_invoke(RaHdcGetCqeErrInfoNum, stub_ra_hdc_get_cqe_err_info_num, 1);
    mocker((stub_fn_t)RaHdcProcessMsg, 10, 0);
    ret = RaHdcGetCqeErrInfoList(&rdma_handle, info, &num);
    EXPECT_INT_EQ(ret, 0);
    EXPECT_INT_EQ(num, 10);

    mocker_clean();
    mocker_invoke(RaHdcLiteGetCqeErrInfoList, stub_ra_hdc_lite_get_cqe_err_info_list, 1);
    mocker_invoke(RaHdcGetCqeErrInfoNum, stub_ra_hdc_get_cqe_err_info_num, 1);
    mocker_invoke((stub_fn_t)RaHdcProcessMsg, (stub_fn_t)stub_ra_hdc_get_cqe_err_num, 10);
    num = 11;
    ret = RaHdcGetCqeErrInfoList(&rdma_handle, info, &num);
    EXPECT_INT_EQ(ret, 0);
    EXPECT_INT_EQ(num, 11);

    mocker_clean();
    mocker_invoke(RaHdcLiteGetCqeErrInfoList, stub_ra_hdc_lite_get_cqe_err_info_list, 1);
    mocker_invoke(RaHdcGetCqeErrInfoNum, stub_ra_hdc_get_cqe_err_info_num, 1);
    mocker_invoke((stub_fn_t)RaHdcProcessMsg, (stub_fn_t)stub_ra_hdc_get_cqe_err_num_v2, 10);
    num = 129;
    ret = RaHdcGetCqeErrInfoList(&rdma_handle, info, &num);
    EXPECT_INT_EQ(info[10].qpn, 12345);
    EXPECT_INT_EQ(ret, 0);
    EXPECT_INT_EQ(num, 11);

    mocker_clean();
    mocker_invoke(RaHdcLiteGetCqeErrInfoList, stub_ra_hdc_lite_get_cqe_err_info_list, 1);
    mocker_invoke(RaHdcGetCqeErrInfoNum, stub_ra_hdc_get_cqe_err_info_num, 1);
    mocker_invoke((stub_fn_t)RaHdcProcessMsg, (stub_fn_t)stub_ra_hdc_get_cqe_err_num, 10);
    mocker((stub_fn_t)memcpy_s, 1, -1);
    num = 11;
    ret = RaHdcGetCqeErrInfoList(&rdma_handle, info, &num);
    EXPECT_INT_EQ(ret, -1);

    mocker_clean();
    rdma_handle.supportLite = 0;
    ret = RaHdcLiteGetCqeErrInfoList(&rdma_handle, info, &num);
    EXPECT_INT_EQ(ret, 0);
    EXPECT_INT_EQ(num, 0);

    mocker_clean();
    rdma_handle.supportLite = 1;
    rdma_handle.cqeErrCnt = 0;
    mocker(pthread_mutex_lock, 10, 0);
    mocker(pthread_mutex_unlock, 10, 0);
    ret = RaHdcLiteGetCqeErrInfoList(&rdma_handle, info, &num);
    EXPECT_INT_EQ(ret, 0);
    EXPECT_INT_EQ(num, 0);

    mocker_clean();
    mocker(RaHdcGetInterfaceVersion, 10, -1);
    ret = RaHdcGetCqeErrInfoNum(&rdma_handle, &num);
    EXPECT_INT_EQ(ret, 0);
    EXPECT_INT_EQ(num, 0);

    mocker_clean();
    mocker(RaHdcProcessMsg, 10, 0);
    mocker_invoke(RaHdcGetInterfaceVersion, stub_ra_hdc_get_interface_version, 1);
    ret = RaHdcGetCqeErrInfoNum(&rdma_handle, &num);
    EXPECT_INT_EQ(ret, 0);
    return;
}

void tc_ra_hdc_qp_create_op()
{
    mocker_clean();
    tc_hdc_test_env_init();
    struct RaRdmaHandle rdma_handle = {0};
    void* qp_handle = NULL;
    rdma_handle.supportLite = 1;
    RA_INIT_LIST_HEAD(&rdma_handle.qpList);
	int ret = RaHdcQpCreate(&rdma_handle, 0, 2, &qp_handle);
    EXPECT_INT_EQ(ret, 0);
    ASSERT_ADDR_NE(qp_handle, NULL);
    struct rdma_lite_qp_cap cap;

    ret = RaHdcQpDestroy(qp_handle);
    EXPECT_INT_EQ(ret, 0);
    tc_hdc_test_env_deinit();

    mocker_clean();

    mocker((stub_fn_t)RaHdcProcessMsg, 10, 0);
    mocker((stub_fn_t)RaRdmaLiteCreateCq, 1, 0);
    ret = RaHdcQpCreate(&rdma_handle, 0, 2, &qp_handle);
    EXPECT_INT_EQ(ret, -EFAULT);
    mocker_clean();

    mocker((stub_fn_t)RaHdcProcessMsg, 10, 0);
    mocker((stub_fn_t)RaRdmaLiteCreateQp, 1, 0);
    ret = RaHdcQpCreate(&rdma_handle, 0, 2, &qp_handle);
    EXPECT_INT_EQ(ret, -EFAULT);
    mocker_clean();

    mocker((stub_fn_t)RaHdcProcessMsg, 10, 0);
    mocker((stub_fn_t)pthread_mutex_init, 1, -1);
    ret = RaHdcQpCreate(&rdma_handle, 0, 2, &qp_handle);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

    struct RaQpHandle qp_hdc;
    qp_hdc.supportLite = 1;
    qp_hdc.qpMode = 2;
    cap.max_inline_data = QP_DEFAULT_MAX_CAP_INLINE_DATA;
    cap.max_send_sge = QP_DEFAULT_MIN_CAP_SEND_SGE;
    cap.max_recv_sge = 1;
    cap.max_send_wr = RA_QP_TX_DEPTH;
    cap.max_recv_wr = RA_QP_TX_DEPTH;
    mocker((stub_fn_t)RaHdcProcessMsg, 10, -1);
    ret = RaHdcLiteQpCreate(&rdma_handle, &qp_hdc, &cap);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();
}

void tc_ra_hdc_get_qp_status_op()
{
    int status;

    mocker_clean();
    tc_hdc_test_env_init();
    struct RaRdmaHandle rdma_handle = {0};
    void* qp_handle = NULL;
    rdma_handle.supportLite = 1;
    RA_INIT_LIST_HEAD(&rdma_handle.qpList);
	int ret = RaHdcQpCreate(&rdma_handle, 0, 2, &qp_handle);
    EXPECT_INT_EQ(ret, 0);
    ASSERT_ADDR_NE(qp_handle, NULL);

    status = 0;
    mocker((stub_fn_t)RaHdcGetInterfaceVersion, 10, -22);
    ret = RaHdcGetQpStatus(qp_handle, &status);
    EXPECT_INT_EQ(ret, 0);

    g_interface_version = RA_RS_OPCODE_BASE_VERSION;
    status = 0;
    mocker_invoke(RaHdcGetInterfaceVersion, stub_ra_get_interface_version, 10);
    ret = RaHdcGetQpStatus(qp_handle, &status);
    EXPECT_INT_EQ(ret, 0);

    ret = RaHdcQpDestroy(qp_handle);
    EXPECT_INT_EQ(ret, 0);
    tc_hdc_test_env_deinit();

    mocker_clean();

    struct RaQpHandle qp_hdc;
    qp_hdc.supportLite = 1;
    qp_hdc.qpMode = 2;
    mocker((stub_fn_t)RaHdcProcessMsg, 10, -1);
    ret = RaHdcLiteGetConnectedInfo(&qp_hdc);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

    mocker((stub_fn_t)RaHdcProcessMsg, 10, 0);
    mocker((stub_fn_t)RaRdmaLiteSetQpSl, 1, -1);
    ret = RaHdcLiteGetConnectedInfo(&qp_hdc);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();
}

extern int RaRdmaLitePollCq(struct rdma_lite_cq *liteCq, int numEntries, struct rdma_lite_wc *liteWc);
int stub_RaRdmaLitePollCq(struct rdma_lite_cq *lite_cq, int num_entries, struct rdma_lite_wc *lite_wc)
{
    int i = 0;
    for (i = 0; i < num_entries; i++) {
        lite_wc[i].status = 0x12;
    }

    return 2;
}

void tc_hdc_send_wr_op()
{
    mocker_clean();
    tc_hdc_test_env_init();
    struct rdev rdev_info = {0};
    unsigned int rdevIndex = 0;
    struct RaRdmaHandle rdma_handle = {0};
    struct RaQpHandle* qp_handle = NULL;

    mocker_invoke((stub_fn_t)RaHdcGetOpcodeLiteSupport, (stub_fn_t)stub_ra_hdc_get_opcode_lite_support, 100);
    mocker_invoke((stub_fn_t)RaHdcProcessMsg, (stub_fn_t)stub_ra_hdc_process_rdev_init, 100);
    int ret = RaHdcRdevInit(&rdma_handle, NOTIFY, rdev_info, &rdevIndex);
    EXPECT_INT_EQ(ret, 0);

	ret = RaHdcQpCreate(&rdma_handle, 0, 2, &qp_handle);
    EXPECT_INT_EQ(ret, 0);

    struct SendWr wr = {0};
    struct SendWrRsp rsp = {0};
    int i = 0;

    ASSERT_ADDR_NE(qp_handle, NULL);

    void *addr = malloc(10);
    struct SgList mem;
    mem.addr = addr;
	mem.len = 10;
	wr.bufList = &mem;
	wr.dstAddr = 0x111;
	wr.bufNum = 1;
	wr.op = 0;
	wr.sendFlag = 0;
    qp_handle->localMr[0].addr = wr.bufList[0].addr;
    qp_handle->localMr[0].len = 0x10000;
    qp_handle->remMr[0].addr = wr.dstAddr;
    qp_handle->remMr[0].len = 0x10000;
    qp_handle->sendWrNum = 999;
    mocker_invoke((stub_fn_t)RaRdmaLitePollCq, stub_RaRdmaLitePollCq, 10);
    ret = RaHdcSendWr(qp_handle, &wr, &rsp);
    EXPECT_INT_EQ(ret, 0);

    unsigned int complete_num = 0;
    struct WrlistSendCompleteNum wrlist_num;
    wrlist_num.sendNum = 2;
    wrlist_num.completeNum = &complete_num;
    struct SendWrlistData wrlist[wrlist_num.sendNum];
	struct SendWrRsp op_rsp_list[wrlist_num.sendNum];
	wrlist[0].memList = mem;
	wrlist[0].dstAddr = 0x111;
	wrlist[0].op = 0;
	wrlist[0].sendFlags = 0;
	wrlist[1].memList = mem;
	wrlist[1].dstAddr = 0x111;
	wrlist[1].op = 0;
	wrlist[1].sendFlags = 0;
    qp_handle->rdmaOps = rdma_handle.rdmaOps;
    g_interface_version = RA_RS_SEND_WRLIST_V2_VERSION;
    mocker_invoke((stub_fn_t)RaGetInterfaceVersion, (stub_fn_t)stub_ra_get_interface_version, 1);
    ret = RaHdcSendWrlist(qp_handle, wrlist, op_rsp_list, wrlist_num);
    g_interface_version = 0;
    EXPECT_INT_EQ(0, ret);

    struct SendWrlistDataExt data_ext[wrlist_num.sendNum];
    data_ext[0].memList = mem;
	data_ext[0].dstAddr = 0x111;
	data_ext[0].op = 0;
	data_ext[0].sendFlags = 0;
	data_ext[1].memList = mem;
	data_ext[1].dstAddr = 0x111;
	data_ext[1].op = 0;
	data_ext[1].sendFlags = 0;
    g_interface_version = RA_RS_SEND_WRLIST_EXT_V2_VERSION;
    mocker_invoke((stub_fn_t)RaGetInterfaceVersion, (stub_fn_t)stub_ra_get_interface_version, 1);
    ret = RaHdcSendWrlistExt(qp_handle, data_ext, op_rsp_list, wrlist_num);
    g_interface_version = 0;
    EXPECT_INT_EQ(0, ret);

    mocker((stub_fn_t)RaHdcLitePostSend, 10, -1);
    g_interface_version = RA_RS_SEND_WRLIST_V2_VERSION;
    mocker_invoke((stub_fn_t)RaGetInterfaceVersion, (stub_fn_t)stub_ra_get_interface_version, 1);
    ret = RaHdcSendWrlist(qp_handle, wrlist, op_rsp_list, wrlist_num);
    g_interface_version = 0;
    EXPECT_INT_EQ(ret, -1);

    mocker((stub_fn_t)RaHdcLitePostSend, 10, -1);
    g_interface_version = RA_RS_SEND_WRLIST_EXT_V2_VERSION;
    mocker_invoke((stub_fn_t)RaGetInterfaceVersion, (stub_fn_t)stub_ra_get_interface_version, 1);
    ret = RaHdcSendWrlistExt(qp_handle, data_ext, op_rsp_list, wrlist_num);
    g_interface_version = 0;
    EXPECT_INT_EQ(ret, -1);

    RaHdcLitePeriodPollCqe(&rdma_handle);

    ret = RaHdcQpDestroy(qp_handle);
    EXPECT_INT_EQ(ret, 0);

    RaHdcRdevDeinit(&rdma_handle, NOTIFY);

    tc_hdc_test_env_deinit();

    free(addr);
    mocker_clean();
}

void tc_hdc_lite_send_wr_op()
{
    mocker_clean();
    tc_hdc_test_env_init();
    struct rdev rdev_info = {0};
    unsigned int rdevIndex = 0;
    struct RaRdmaHandle rdma_handle = {0};
    struct RaQpHandle* qp_handle = NULL;

    mocker_invoke((stub_fn_t)RaHdcGetOpcodeLiteSupport, (stub_fn_t)stub_ra_hdc_get_opcode_lite_support, 100);
    mocker_invoke((stub_fn_t)RaHdcProcessMsg, (stub_fn_t)stub_ra_hdc_process_rdev_init, 100);
    int ret = RaHdcRdevInit(&rdma_handle, NOTIFY, rdev_info, &rdevIndex);
    EXPECT_INT_EQ(ret, 0);

    ret = RaHdcQpCreate(&rdma_handle, 0, 2, &qp_handle);
    EXPECT_INT_EQ(ret, 0);

    struct SendWr wr = {0};
    struct SendWrV2 wr_v2 = {0};
    struct SendWrRsp rsp = {0};
    int i = 0;

    ASSERT_ADDR_NE(qp_handle, NULL);

    void *addr = malloc(10);
    struct SgList mem;
    mem.addr = addr;
    mem.len = 10;
    wr.bufList = &mem;
    wr.dstAddr = 0x111;
    wr.bufNum = 1;
    wr.op = 0;
    wr.sendFlag = 0;
    qp_handle->localMr[0].addr = wr.bufList[0].addr;
    qp_handle->localMr[0].len = 0x10000;
    qp_handle->remMr[0].addr = wr.dstAddr;
    qp_handle->remMr[0].len = 0x10000;
    qp_handle->sendWrNum = 999;
    qp_handle->supportLite = 1;
    mocker_invoke((stub_fn_t)RaRdmaLitePollCq, stub_RaRdmaLitePollCq, 10);
    ret = RaHdcSendWr(qp_handle, &wr, &rsp);
    EXPECT_INT_EQ(ret, 0);

    unsigned int complete_num = 0;
    struct WrlistSendCompleteNum wrlist_num;
    wrlist_num.sendNum = 2;
    wrlist_num.completeNum = &complete_num;
    struct SendWrlistData wrlist[wrlist_num.sendNum];
    struct SendWrRsp op_rsp_list[wrlist_num.sendNum];
    wrlist[0].memList = mem;
    wrlist[0].dstAddr = 0x111;
    wrlist[0].op = 0;
    wrlist[0].sendFlags = 0;
    wrlist[1].memList = mem;
    wrlist[1].dstAddr = 0x111;
    wrlist[1].op = 0;
    wrlist[1].sendFlags = 0;
    qp_handle->rdmaOps = rdma_handle.rdmaOps;
    ret = RaHdcSendWrlist(qp_handle, wrlist, op_rsp_list, wrlist_num);
    EXPECT_INT_EQ(0, ret);

    struct SendWrlistDataExt data_ext[wrlist_num.sendNum];
    data_ext[0].memList = mem;
    data_ext[0].dstAddr = 0x111;
    data_ext[0].op = 0;
    data_ext[0].sendFlags = 0;
    data_ext[1].memList = mem;
    data_ext[1].dstAddr = 0x111;
    data_ext[1].op = 0;
    data_ext[1].sendFlags = 0;
    ret = RaHdcSendWrlistExt(qp_handle, data_ext, op_rsp_list, wrlist_num);
    EXPECT_INT_EQ(0, ret);

    mocker((stub_fn_t)RaHdcLitePostSend, 10, -12);
    ret = RaHdcSendWrlist(qp_handle, wrlist, op_rsp_list, wrlist_num);
    EXPECT_INT_EQ(ret, -12);

    mocker((stub_fn_t)RaHdcLitePostSend, 10, -12);
    ret = RaHdcSendWrlistExt(qp_handle, data_ext, op_rsp_list, wrlist_num);
    EXPECT_INT_EQ(ret, -12);

    RaHdcLitePeriodPollCqe(&rdma_handle);

    qp_handle->qpMode = RA_RS_OP_QP_MODE;
    qp_handle->sqDepth = 256;
    qp_handle->sendWrNum = 256;
    qp_handle->pollCqeNum = 0;
    ret = RaHdcSendWrV2(qp_handle, &wr_v2, &rsp);
    EXPECT_INT_EQ(ret, -12);
    ret = RaHdcSendWrlistExt(qp_handle, data_ext, op_rsp_list, wrlist_num);
    EXPECT_INT_EQ(ret, -12);
    qp_handle->pollCqeNum = 2;
    ret = RaHdcSendWrV2(qp_handle, &wr_v2, &rsp);
    EXPECT_INT_EQ(ret, -12);
    ret = RaHdcSendWrlistExt(qp_handle, data_ext, op_rsp_list, wrlist_num);
    EXPECT_INT_EQ(ret, -12);
    qp_handle->sendWrNum = 0;
    qp_handle->pollCqeNum = -256;
    ret = RaHdcSendWrV2(qp_handle, &wr_v2, &rsp);
    EXPECT_INT_EQ(ret, -12);
    ret = RaHdcSendWrlistExt(qp_handle, data_ext, op_rsp_list, wrlist_num);
    EXPECT_INT_EQ(ret, -12);

    ret = RaHdcQpDestroy(qp_handle);
    EXPECT_INT_EQ(ret, 0);

    RaHdcRdevDeinit(&rdma_handle, NOTIFY);

    tc_hdc_test_env_deinit();

    free(addr);
    mocker_clean();
}

void tc_hdc_recv_wrlist()
{
    mocker_clean();
    void *addr = NULL;
    int size = 0;
    int ret = 0;
    struct RecvWrlistData rev_wr = {0};
    unsigned int recv_num = 1;
    unsigned int rev_complete_num = 0;
    tc_hdc_test_env_init();
    struct rdev rdev_info = {0};
    unsigned int rdevIndex = 0;
    struct RaRdmaHandle rdma_handle = {0};
    struct RaQpHandle* qp_handle = NULL;
    struct RaQpHandle qp_handle_tmp = { 0 };

    rev_wr.wrId = 100;
    rev_wr.memList.lkey = 0xff;
    rev_wr.memList.addr = addr;
    rev_wr.memList.len = size;

    qp_handle_tmp.qpMode = 0;
    ret = RaHdcRecvWrlist(&qp_handle_tmp, &rev_wr, recv_num, &rev_complete_num);
    EXPECT_INT_NE(ret, 0);

    mocker_invoke((stub_fn_t)RaHdcGetOpcodeLiteSupport, (stub_fn_t)stub_ra_hdc_get_opcode_lite_support, 100);
    mocker_invoke((stub_fn_t)RaHdcProcessMsg, (stub_fn_t)stub_ra_hdc_process_rdev_init, 100);
    ret = RaHdcRdevInit(&rdma_handle, NOTIFY, rdev_info, &rdevIndex);
    EXPECT_INT_EQ(ret, 0);
    ret = RaHdcQpCreate(&rdma_handle, 0, 2, &qp_handle);
    EXPECT_INT_EQ(ret, 0);
    ASSERT_ADDR_NE(qp_handle, NULL);
    qp_handle->supportLite = 1;

    ret = RaHdcRecvWrlist(qp_handle, &rev_wr, recv_num, &rev_complete_num);
    EXPECT_INT_EQ(ret, 0);

    ret = RaHdcQpDestroy(qp_handle);
    EXPECT_INT_EQ(ret, 0);
    RaHdcRdevDeinit(&rdma_handle, NOTIFY);
    tc_hdc_test_env_deinit();
    mocker_clean();
}

void tc_hdc_poll_cq()
{
    mocker_clean();
    int ret = 0;
    unsigned int num_entries = 1;
    struct rdma_lite_wc_v2 lite_wc = {0};
    tc_hdc_test_env_init();
    struct rdev rdev_info = {0};
    unsigned int rdevIndex = 0;
    struct RaRdmaHandle rdma_handle = {0};
    struct RaQpHandle* qp_handle = NULL;
    struct RaQpHandle qp_handle_tmp = { 0 };

    qp_handle_tmp.qpMode = 0;
    ret = RaHdcPollCq(&qp_handle_tmp, true, num_entries, &lite_wc);
    EXPECT_INT_NE(ret, 0);

    mocker_invoke((stub_fn_t)RaHdcGetOpcodeLiteSupport, (stub_fn_t)stub_ra_hdc_get_opcode_lite_support, 100);
    mocker_invoke((stub_fn_t)RaHdcProcessMsg, (stub_fn_t)stub_ra_hdc_process_rdev_init, 100);
    rdma_handle.disabledLiteThread = true;
    ret = RaHdcRdevInit(&rdma_handle, NOTIFY, rdev_info, &rdevIndex);
    EXPECT_INT_EQ(ret, 0);
    ret = RaHdcQpCreate(&rdma_handle, 0, 2, &qp_handle);
    EXPECT_INT_EQ(ret, 0);
    ASSERT_ADDR_NE(qp_handle, NULL);
    qp_handle->supportLite = 1;
    qp_handle->recvWrNum = 1;

    ret = RaHdcPollCq(qp_handle, false, num_entries, &lite_wc);
    EXPECT_INT_EQ(ret, 0);

    ret = RaHdcQpDestroy(qp_handle);
    EXPECT_INT_EQ(ret, 0);
    RaHdcRdevDeinit(&rdma_handle, NOTIFY);
    tc_hdc_test_env_deinit();
    mocker_clean();
}

void tc_hdc_get_lite_support()
{
    int support;

    g_interface_version = 0;
    mocker_invoke(RaHdcGetInterfaceVersion, stub_ra_get_interface_version, 100);
    RaHdcGetOpcodeLiteSupport(0, 0x3, &support);
    EXPECT_INT_EQ(support, 0);
    mocker_clean();

    g_interface_version = 2;
    mocker_invoke(RaHdcGetInterfaceVersion, stub_ra_get_interface_version, 100);
    RaHdcGetOpcodeLiteSupport(0, 0x3, &support);
    EXPECT_INT_EQ(support, 1);
    mocker_clean();

    g_interface_version = 2;
    mocker_invoke(RaHdcGetInterfaceVersion, stub_ra_get_interface_version, 100);
    RaHdcGetOpcodeLiteSupport(0, 0x2, &support);
    EXPECT_INT_EQ(support, 2);
    mocker_clean();

    g_interface_version = 1;
    mocker_invoke(RaHdcGetInterfaceVersion, stub_ra_get_interface_version, 100);
    RaHdcGetOpcodeLiteSupport(0, 0x2, &support);
    EXPECT_INT_EQ(support, 0);
    mocker_clean();
}

void tc_ra_rdev_get_support_lite()
{
    struct RaRdmaHandle rdma_handle = {0};
    int support_lite = 1;
    int ret;

    ret = RaRdevGetSupportLite(NULL, NULL);
    EXPECT_INT_NE(ret, 0);

    ret = RaRdevGetSupportLite(&rdma_handle, &support_lite);
    EXPECT_INT_EQ(ret, 0);
    EXPECT_INT_EQ(support_lite, rdma_handle.supportLite);
}

void tc_ra_rdev_get_handle()
{
    void *rdma_handle = NULL;
    int ret;

    ret = RaRdevGetHandle(1024, NULL);
    EXPECT_INT_EQ(ret, -EINVAL);

    ret = RaRdevGetHandle(0, NULL);
    EXPECT_INT_EQ(ret, -EINVAL);

    ret = RaRdevGetHandle(0, &rdma_handle);
    EXPECT_INT_EQ(ret, -ENODEV);
}

void tc_ra_is_first_or_last_used()
{
    s32 ret = 0;

    ret = RaIsFirstUsed(-1);
    EXPECT_INT_EQ(ret, -EINVAL);

    ret = RaIsFirstUsed(0);
    EXPECT_INT_EQ(ret, 1);

    ret = RaIsFirstUsed(0);
    EXPECT_INT_EQ(ret, 0);

    ret = RaIsFirstUsed(0);
    EXPECT_INT_EQ(ret, 0);

    ret = RaIsLastUsed(-1);
    EXPECT_INT_EQ(ret, -EINVAL);

    ret = RaIsLastUsed(0);
    EXPECT_INT_EQ(ret, 0);

    ret = RaIsLastUsed(0);
    EXPECT_INT_EQ(ret, 0);

    ret = RaIsLastUsed(0);
    EXPECT_INT_EQ(ret, 1);

    ret = RaIsLastUsed(0);
    EXPECT_INT_EQ(ret, -EINVAL);

    ret = RaIsLastUsed(128);
    EXPECT_INT_EQ(ret, -EINVAL);

    ret = RaIsFirstUsed(128);
    EXPECT_INT_EQ(ret, -EINVAL);

    ret = RaIsFirstUsed(15);
    EXPECT_INT_EQ(ret, 1);

    ret = RaIsLastUsed(15);
    EXPECT_INT_EQ(ret, 1);
}

void tc_ra_hdc_lite_ctx_init()
{
    struct RaRdmaHandle rdma_handle = {0};
    unsigned int phyId = 0;
    unsigned int rdevIndex = 0;
    struct rdma_lite_context rdma_lite_context = {0};
    int ret = 0;

    rdma_handle.supportLite = 2 * 1024 * 1024;
    mocker_clean();
    mocker(RaHdcLiteMutexDeinit, 10, 0);
    mocker(RaRdmaLiteFreeCtx, 10, 0);
    mocker(DlHalSensorNodeUnregister, 10, 0);
    mocker(DlDrvDeviceGetIndexByPhyId, 10, 0);
    mocker(RaSensorNodeRegister, 10, 0);
    mocker(RaHdcProcessMsg, 10, 0);
    mocker(RaRdmaLiteAllocCtx, 1, NULL);
    ret = RaHdcLiteCtxInit(&rdma_handle, phyId, rdevIndex);
    EXPECT_INT_EQ(ret, -14);

    mocker_clean();
    mocker(RaHdcLiteMutexDeinit, 10, 0);
    mocker(RaRdmaLiteFreeCtx, 10, 0);
    mocker(DlHalSensorNodeUnregister, 10, 0);
    mocker(DlDrvDeviceGetIndexByPhyId, 10, 0);
    mocker(RaSensorNodeRegister, 10, 0);
    mocker(RaHdcProcessMsg, 10, 0);
    mocker(RaRdmaLiteAllocCtx, 10, &rdma_lite_context);
    mocker(RaHdcLiteMutexInit, 10, 0);
    rdma_handle.disabledLiteThread = true;
    ret = RaHdcLiteCtxInit(&rdma_handle, phyId, rdevIndex);
    EXPECT_INT_EQ(ret, 0);

    rdma_handle.disabledLiteThread = false;
    mocker(pthread_create, 10, -1);
    ret = RaHdcLiteCtxInit(&rdma_handle, phyId, rdevIndex);
    EXPECT_INT_EQ(ret, -258);

    mocker_clean();
    mocker(RaHdcLiteMutexDeinit, 10, 0);
    mocker(RaRdmaLiteFreeCtx, 10, 0);
    mocker(DlHalSensorNodeUnregister, 10, 0);
    mocker(DlDrvDeviceGetIndexByPhyId, 10, 0);
    mocker(RaSensorNodeRegister, 10, 0);
    mocker(RaHdcProcessMsg, 10, 0);
    mocker(RaRdmaLiteAllocCtx, 10, &rdma_lite_context);
    mocker(RaHdcLiteMutexInit, 10, 0);
    mocker(pthread_create, 10, 0);
    ret = RaHdcLiteCtxInit(&rdma_handle, phyId, rdevIndex);
    EXPECT_INT_EQ(ret, 0);

    mocker_clean();
    mocker(pthread_mutex_init, 10, 0);
    ret = RaHdcLiteMutexInit(&rdma_handle, phyId);
    EXPECT_INT_EQ(ret, 0);

    mocker_clean();
    mocker(pthread_mutex_init, 1, -1);
    ret = RaHdcLiteMutexInit(&rdma_handle, phyId);
    EXPECT_INT_EQ(ret, -258);
}

struct rdma_lite_cq *stub_RaRdmaLiteCreateCq(struct rdma_lite_context *lite_ctx,
    struct rdma_lite_cq_attr *lite_cq_attr)
{
    static cnt = 0;
    static struct rdma_lite_cq lite_cq = {0};

    cnt++;
    if (cnt == 1) {
        return NULL;
    }
    else {
        return &lite_cq;
    }
}

void rc_ra_hdc_lite_qp_create()
{
    struct RaQpHandle qp_hdc = {0};
    struct rdma_lite_qp_cap cap = {0};
    struct rdma_lite_cq lite_cq = {0};
    struct rdma_lite_qp lite_qp = {0};
    struct RaRdmaHandle rdma_handle = {0};
    struct rdma_lite_cq_attr lite_send_cq_attr = {0};
    struct rdma_lite_cq_attr lite_recv_cq_attr = {0};
    struct rdma_lite_qp_attr lite_qp_attr = {0};
    struct rdma_lite_wc lite_wc = {0};
    unsigned int api_version = 0;
    int ret = 0;

    qp_hdc.list.next = &(qp_hdc.list);
    qp_hdc.list.prev = &(qp_hdc.list);
    rdma_handle.qpList.next = &(rdma_handle.qpList);
    rdma_handle.qpList.prev = &(rdma_handle.qpList);
    qp_hdc.supportLite = 1;
    qp_hdc.qpMode = 2;
    rdma_handle.supportLite = 1;
    cap.max_inline_data = QP_DEFAULT_MAX_CAP_INLINE_DATA;
    cap.max_send_sge = QP_DEFAULT_MIN_CAP_SEND_SGE;
    cap.max_recv_sge = 1;
    cap.max_send_wr = RA_QP_TX_DEPTH;
    cap.max_recv_wr = RA_QP_TX_DEPTH;

    mocker_clean();
    mocker(RaHdcLiteGetCqQpAttr, 10, 0);
    mocker(RaHdcLiteInitMemPool, 10, 0);
    mocker(RaRdmaLiteDestroyCq, 10, 0);
    mocker(RaHdcLiteDeinitMemPool, 10, 0);
    mocker(RaRdmaLiteCreateCq, 1, NULL);
    ret = RaHdcLiteQpCreate(&rdma_handle, &qp_hdc, &cap);
    EXPECT_INT_EQ(ret, -14);

    mocker_clean();
    mocker(RaHdcLiteGetCqQpAttr, 10, 0);
    mocker(RaHdcLiteInitMemPool, 10, 0);
    mocker(RaRdmaLiteDestroyCq, 10, 0);
    mocker(RaHdcLiteDeinitMemPool, 10, 0);
    mocker_invoke(RaRdmaLiteCreateCq, stub_RaRdmaLiteCreateCq, 2);
    ret = RaHdcLiteQpCreate(&rdma_handle, &qp_hdc, &cap);
    EXPECT_INT_EQ(ret, -14);

    mocker_clean();
    mocker(RaHdcLiteGetCqQpAttr, 10, 0);
    mocker(RaHdcLiteInitMemPool, 10, 0);
    mocker(RaRdmaLiteDestroyCq, 10, 0);
    mocker(RaHdcLiteDeinitMemPool, 10, 0);
    mocker(RaRdmaLiteCreateCq, 2, &lite_cq);
    mocker(RaRdmaLiteCreateQp, 1, NULL);
    ret = RaHdcLiteQpCreate(&rdma_handle, &qp_hdc, &cap);
    EXPECT_INT_EQ(ret, -14);

    mocker_clean();
    mocker(RaHdcLiteGetCqQpAttr, 10, 0);
    mocker(RaHdcLiteInitMemPool, 10, 0);
    mocker(RaRdmaLiteDestroyQp, 10, 0);
    mocker(RaRdmaLiteDestroyCq, 10, 0);
    mocker(RaHdcLiteDeinitMemPool, 10, 0);
    mocker(RaRdmaLiteCreateCq, 10, &lite_cq);
    mocker(RaRdmaLiteCreateQp, 10, &lite_qp);
    mocker(pthread_mutex_init, 10, 0);
    mocker(pthread_mutex_lock, 10, 0);
    mocker(pthread_mutex_unlock, 10, 0);
    mocker(calloc, 10, NULL);
    ret = RaHdcLiteQpCreate(&rdma_handle, &qp_hdc, &cap);
    EXPECT_INT_EQ(ret, -12);

    mocker_clean();
    mocker(RaHdcLiteGetCqQpAttr, 10, 0);
    mocker(RaHdcLiteInitMemPool, 10, 0);
    mocker(RaRdmaLiteDestroyCq, 10, 0);
    mocker(RaHdcLiteDeinitMemPool, 10, 0);
    mocker(RaRdmaLiteCreateCq, 10, &lite_cq);
    mocker(RaRdmaLiteCreateQp, 10, &lite_qp);
    mocker(pthread_mutex_init, 10, 0);
    mocker(pthread_mutex_lock, 10, 0);
    mocker(pthread_mutex_unlock, 10, 0);
    mocker(calloc, 10, &lite_wc);
    ret = RaHdcLiteQpCreate(&rdma_handle, &qp_hdc, &cap);
    EXPECT_INT_EQ(ret, 0);

    mocker_clean();
    mocker(RaHdcProcessMsg, 10, 0);
    mocker(RaHdcLiteQpAttrInit, 10, 0);
    mocker(memcpy_s, 10, 0);
    ret = RaHdcLiteGetCqQpAttr(&qp_hdc, &lite_send_cq_attr, &lite_recv_cq_attr, &lite_qp_attr);
    EXPECT_INT_EQ(ret, 0);

    mocker_clean();
    mocker(RaHdcProcessMsg, 10, 0);
    mocker(RaRdmaLiteInitMemPool, 10, 0);
    ret = RaHdcLiteInitMemPool(&qp_hdc, &cap, &lite_send_cq_attr, &lite_recv_cq_attr, &lite_qp_attr);
    EXPECT_INT_EQ(ret, 0);

    mocker_clean();
    mocker(RaRdmaLiteDeinitMemPool, 10, 0);
    RaHdcLiteDeinitMemPool(&rdma_handle, &qp_hdc);

    mocker_clean();
    ret = RaRdmaLiteGetApiVersion();
    EXPECT_INT_EQ(ret, 0);
}

void tc_ra_hdc_tlv_request()
{
    struct RaTlvHandle tlv_handle_tmp = {0};
    struct TlvMsg send_msg = {0};
    struct TlvMsg recv_msg = {0};
    unsigned int moduleType;
    int ret = 0;

    moduleType = TLV_MODULE_TYPE_CCU;
    send_msg.length = 0;
    send_msg.type = 0;
    mocker(RaHdcProcessMsg, 100, 0);
    ret = RaHdcTlvRequest(&tlv_handle_tmp, moduleType, &send_msg, &recv_msg);
    EXPECT_INT_EQ(ret, 0);

    tlv_handle_tmp.initInfo.phyId = 0;
    moduleType = TLV_MODULE_TYPE_NSLB;
    send_msg.length = TC_TLV_HDC_MSG_SIZE;
    send_msg.type = 0;
    send_msg.data = (char *)malloc(TC_TLV_HDC_MSG_SIZE);
    int i = 0;
    for (i = 0; i < TC_TLV_HDC_MSG_SIZE; i++) {
        send_msg.data[i] = (char)(i % 256);
    }

    mocker(RaHdcProcessMsg, 100, 0);
    ret = RaHdcTlvRequest(&tlv_handle_tmp, moduleType, &send_msg, &recv_msg);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    free(send_msg.data);
    send_msg.data = NULL;
}

void tc_ra_hdc_qp_create_with_attrs()
{
    struct RaRdmaHandle rdma_handle = {0};
    struct QpExtAttrs extAttrs = {0};
    struct AiQpInfo info = {0};
    void *qp_handle = NULL;
    int ret = 0;

    mocker(memcpy_s, 1, -1);
    ret = RaHdcQpCreateWithAttrs(&rdma_handle, &extAttrs, &qp_handle);
    EXPECT_INT_EQ(ret, -256);
    mocker_clean();

    mocker(memcpy_s, 1, -1);
    ret = RaHdcAiQpCreate(&rdma_handle, &extAttrs, &info, &qp_handle);
    EXPECT_INT_EQ(ret, -256);
    mocker_clean();

    mocker(memcpy_s, 1, -1);
    ret = RaHdcAiQpCreateWithAttrs(&rdma_handle, &extAttrs, &info, &qp_handle);
    EXPECT_INT_EQ(ret, -256);
    mocker_clean();
}

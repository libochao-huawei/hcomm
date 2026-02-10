/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <errno.h>
#include <stdlib.h>
#include <pthread.h>
#include "securec.h"
#include "ut_dispatch.h"
#include "dl_hal_function.h"
#include "ra_rs_comm.h"
#include "hccp_common.h"
#include "ra_async.h"
#include "ra.h"
#include "ra_hdc.h"
#include "ra_hdc_ctx.h"
#include "ra_hdc_async.h"
#include "ra_hdc_async_ctx.h"
#include "ra_hdc_async_socket.h"
#include "ra_client_host.h"
#include "ra_comm.h"
#include "tc_ra_async.h"

extern int RaHdcAsyncInitSession(struct RaInitConfig *cfg);
extern int RaHdcAsyncMutexInit(unsigned int phyId);
extern void RaHwAsyncSetConnectStatus(unsigned int phyId, unsigned int connectStatus);
extern int RaHdcAsyncSessionClose(unsigned int phyId);
extern struct HdcAsyncInfo gRaHdcAsync[RA_MAX_PHY_ID_NUM];

int ra_hdc_send_msg_async_stub(unsigned int opcode, unsigned int phyId, char *data, unsigned int dataSize,
    struct RaRequestHandle *reqHandle)
{
    reqHandle->isDone = true;
    return 0;
}

void hdc_async_del_response_stub(struct RaRequestHandle *reqHandle)
{
    free(reqHandle);
    reqHandle = NULL;
    return;
}

void *calloc_first_stub(unsigned long num, unsigned long size)
{
    static int calloc_first_stub_call_num1 = 0;
    calloc_first_stub_call_num1++;

    if (calloc_first_stub_call_num1 == 1) {
        return calloc(num, size);
    }

    return NULL;
}

void tc_ra_ctx_lmem_register_async()
{
    struct ra_ctx_handle ctx_handle = {0};
    struct mr_reg_info_t lmem_info = {0};
    struct ra_lmem_handle *lmem_handle = NULL;
    struct RaRequestHandle *req_handle = NULL;
    union op_lmem_reg_info_data async_data = {0};
    struct mem_reg_info info = {0};
    int ret;

    mocker_clean();
    ctx_handle.protocol = 1;

    mocker(ra_hdc_ctx_lmem_register_async, 10, -1);
    ret = ra_ctx_lmem_register_async(&ctx_handle, &lmem_info, &lmem_handle, &req_handle);
    EXPECT_INT_NE(ret, 0);
    mocker_clean();

    mocker(RaHdcSendMsgAsync, 10, 0);
    ret = ra_ctx_lmem_register_async(&ctx_handle, &lmem_info, &lmem_handle, &req_handle);
    EXPECT_INT_EQ(ret, 0);

    req_handle->recvBuf = &async_data;
    req_handle->privData = &info;
    mocker(memcpy_s, 10, -1);
    ra_hdc_async_handle_lmem_register(req_handle);
    free(lmem_handle);
    mocker_clean();
    free(req_handle);
}

void tc_ra_ctx_lmem_unregister_async()
{
    struct ra_ctx_handle ctx_handle = {0};
    struct ra_lmem_handle *lmem_handle = malloc(sizeof(struct ra_lmem_handle));
    void *req_handle = NULL;

    mocker_clean();

    mocker(RaHdcSendMsgAsync, 1, 0);
    ra_ctx_lmem_unregister_async(&ctx_handle, lmem_handle, &req_handle);
    mocker_clean();
    free(req_handle);

    mocker(RaHdcSendMsgAsync, 1, 0);
    lmem_handle = malloc(sizeof(struct ra_lmem_handle));
    ra_ctx_lmem_unregister_async(&ctx_handle, lmem_handle, &req_handle);
    mocker_clean();
    free(req_handle);

    mocker(ra_hdc_ctx_lmem_unregister_async, 1, -1);
    lmem_handle = malloc(sizeof(struct ra_lmem_handle));
    ra_ctx_lmem_unregister_async(&ctx_handle, lmem_handle, &req_handle);
    mocker_clean();
}

void tc_ra_ctx_qp_create_async()
{
    struct ra_ctx_handle ctx_handle = {0};
    struct qp_create_attr qpAttr = {0};
    struct qp_info_t *qp_info = NULL;
    struct ra_ctx_qp_handle *qp_handle = NULL;
    struct RaRequestHandle *req_handle = NULL;
    struct ra_cq_handle scq_handle = {0};
    struct ra_cq_handle rcq_handle = {0};

    qpAttr.scq_handle = (void*)&scq_handle;
    qpAttr.rcq_handle = (void*)&rcq_handle;
    ctx_handle.protocol = 1;
    mocker_clean();

    mocker(RaHdcSendMsgAsync, 1, 0);
    ra_ctx_qp_create_async(&ctx_handle, &qpAttr, &qp_info, &qp_handle, &req_handle);
    free(qp_handle);
    free(req_handle);
    mocker_clean();

    mocker(RaHdcSendMsgAsync, 1, 0);
    ra_ctx_qp_create_async(&ctx_handle, &qpAttr, &qp_info, &qp_handle, &req_handle);
    mocker_clean();
    free(qp_handle);
    free(req_handle);

    mocker(ra_hdc_ctx_qp_create_async, 1, -1);
    ra_ctx_qp_create_async(&ctx_handle, &qpAttr, &qp_info, &qp_handle, &req_handle);
    mocker_clean();
}

void tc_ra_ctx_qp_destroy_async()
{
    struct ra_ctx_qp_handle *qp_handle = malloc(sizeof(struct ra_ctx_qp_handle));
    void *req_handle = NULL;

    mocker_clean();

    mocker(RaHdcSendMsgAsync, 1, 0);
    ra_ctx_qp_destroy_async(qp_handle, &req_handle);
    mocker_clean();

    free(req_handle);

    qp_handle = malloc(sizeof(struct ra_ctx_qp_handle));
    mocker(ra_hdc_ctx_qp_destroy_async, 1, -1);
    ra_ctx_qp_destroy_async(qp_handle, &req_handle);
    mocker_clean();
}

void tc_ra_ctx_qp_import_async()
{
    struct ra_ctx_handle ctx_handle = {0};
    struct qp_import_info_t info = {0};
    struct ra_ctx_rem_qp_handle *rem_qp_handle = NULL;
    struct RaRequestHandle *req_handle = NULL;

    ctx_handle.protocol = 1;
    mocker_clean();

    mocker(RaHdcSendMsgAsync, 1, 0);
    ra_ctx_qp_import_async(&ctx_handle, &info, &rem_qp_handle, &req_handle);
    free(rem_qp_handle);
    mocker_clean();

    mocker(ra_hdc_ctx_qp_import_async, 1, -1);
    ra_ctx_qp_import_async(&ctx_handle, &info, &rem_qp_handle, &req_handle);
    mocker_clean();
    free(req_handle);
}

void tc_ra_ctx_qp_unimport_async()
{
    struct ra_ctx_rem_qp_handle *rem_qp_handle = malloc(sizeof(struct ra_ctx_rem_qp_handle));
    void *req_handle = NULL;
    mocker_clean();

    mocker(RaHdcSendMsgAsync, 1, 0);
    ra_ctx_qp_unimport_async(rem_qp_handle, &req_handle);
    mocker_clean();

    free(req_handle);

    rem_qp_handle = malloc(sizeof(struct ra_ctx_rem_qp_handle));
    mocker(ra_hdc_ctx_qp_unimport_async, 1, -1);
    ra_ctx_qp_unimport_async(rem_qp_handle, &req_handle);
    mocker_clean();
}

void tc_ra_socket_send_async()
{
    struct SocketHdcInfo fd_handle = {0};
    struct RaRequestHandle *req_handle = NULL;
    unsigned long long sent_size = 0;

    mocker_clean();
    mocker(RaHdcSendMsgAsync, 1, 0);
    RaSocketSendAsync(&fd_handle,"a", 1, &sent_size, &req_handle);
    mocker_clean();

    free(req_handle);
}

void tc_ra_socket_recv_async()
{
    struct SocketHdcInfo fd_handle = {0};
    struct RaRequestHandle *req_handle = NULL;
    unsigned long long received_size = 0;
    char data = 0;

    mocker_clean();
    mocker(RaHdcSendMsgAsync, 1, 0);
    RaSocketRecvAsync(&fd_handle, &data, 1, &received_size, &req_handle);
    free(req_handle->privData);
    mocker_clean();

    free(req_handle);
}

void tc_ra_get_async_req_result()
{
    struct RaRequestHandle *req_handle = malloc(sizeof(struct RaRequestHandle));
    struct RaAsyncOpHandle op_handle = {0};
    int req_result = 0;

    req_handle->isDone = 1;
    req_handle->phyId = 0;
    req_handle->recvLen = 1;
    req_handle->opHandle = &op_handle;
    RA_INIT_LIST_HEAD(&req_handle->list);
    req_handle->recvBuf = malloc(1);
    req_handle->privHandle = NULL;
    mocker_clean();

    mocker(pthread_mutex_lock, 1, 0);
    mocker(pthread_mutex_unlock, 1, 0);
    RaGetAsyncReqResult(req_handle, &req_result);
    mocker_clean();
}

void tc_ra_socket_batch_connect_async_normal()
{
    struct RaRequestHandle *req_handle = NULL;
    struct RaSocketHandle socket_handle = {0};
    struct SocketConnectInfoT conn[1] = {0};
    int ret = 0;

    conn[0].socketHandle = (void *)&socket_handle;

    mocker_clean();
    mocker(RaInetPton, 10, 0);

    mocker(RaHdcSendMsgAsync, 10, 0);
    mocker(RaGetSocketConnectInfo, 10, 0);

    ret = RaSocketBatchConnectAsync(conn, 1, &req_handle);
    free(req_handle);
    mocker_clean();
    return;
}

void tc_ra_socket_batch_connect_async_fail()
{
    struct RaRequestHandle *req_handle = NULL;
    struct RaSocketHandle socket_handle = {0};
    struct SocketConnectInfoT conn[1] = {0};
    int ret = 0;

    conn[0].socketHandle = (void *)&socket_handle;

    mocker_clean();
    mocker(RaInetPton, 10, 0);

    mocker(RaHdcSendMsgAsync, 10, 0);
    mocker(RaGetSocketConnectInfo, 10, 0);

    ret = RaSocketBatchConnectAsync(NULL, 1, &req_handle);
    EXPECT_INT_NE(ret, 0);

    ret = RaSocketBatchConnectAsync(conn, 0, &req_handle);
    EXPECT_INT_NE(ret, 0);

    ret = RaSocketBatchConnectAsync(conn, 1, NULL);
    EXPECT_INT_NE(ret, 0);

    conn[0].socketHandle = NULL;
    ret = RaSocketBatchConnectAsync(conn, 1, &req_handle);
    EXPECT_INT_NE(ret, 0);

    mocker_clean();
    return;
}

void tc_ra_socket_batch_connect_async()
{
    tc_ra_socket_batch_connect_async_normal();
    tc_ra_socket_batch_connect_async_fail();
}

void tc_ra_socket_listen_start_async_normal()
{
    struct RaRequestHandle *req_handle = NULL;
    struct RaSocketHandle socket_handle = {0};
    struct SocketListenInfoT conn[1] = {0};
    int ret = 0;

    conn[0].socketHandle = (void *)&socket_handle;

    mocker_clean();
    mocker(RaInetPton, 10, 0);
    mocker(RaHdcSendMsgAsync, 10, 0);
    mocker(RaGetSocketListenInfo, 10, 0);

    ret = RaSocketListenStartAsync(conn, 1, &req_handle);
    free(req_handle->privData);
    free(req_handle);
    mocker_clean();
    return;
}

void tc_ra_socket_listen_start_async_fail()
{
    struct RaRequestHandle *req_handle = NULL;
    struct RaSocketHandle socket_handle = {0};
    struct SocketListenInfoT conn[1] = {0};
    int ret = 0;

    conn[0].socketHandle = (void *)&socket_handle;

    mocker_clean();
    mocker(RaInetPton, 10, 0);
    mocker(RaHdcSendMsgAsync, 10, 0);
    mocker(RaGetSocketListenInfo, 10, 0);

    ret = RaSocketListenStartAsync(NULL, 1, &req_handle);
    EXPECT_INT_NE(ret, 0);

    ret = RaSocketListenStartAsync(conn, 0, &req_handle);
    EXPECT_INT_NE(ret, 0);

    ret = RaSocketListenStartAsync(conn, 1, NULL);
    EXPECT_INT_NE(ret, 0);

    conn[0].socketHandle = NULL;
    ret = RaSocketListenStartAsync(conn, 1, &req_handle);
    EXPECT_INT_NE(ret, 0);

    mocker_clean();
    return;
}

void tc_ra_socket_listen_start_async()
{
    tc_ra_socket_listen_start_async_normal();
    tc_ra_socket_listen_start_async_fail();
}

void tc_ra_socket_listen_stop_async_normal()
{
    struct RaRequestHandle *req_handle = NULL;
    struct RaSocketHandle socket_handle = {0};
    struct SocketListenInfoT conn[1] = {0};

    conn[0].socketHandle = (void *)&socket_handle;

    mocker_clean();
    mocker(RaInetPton, 10, 0);
    mocker(RaHdcSendMsgAsync, 10, 0);
    mocker(RaGetSocketListenInfo, 10, 0);

    (void)RaSocketListenStopAsync(conn, 1, &req_handle);
    free(req_handle);
    mocker_clean();
    return;
}

void tc_ra_socket_listen_stop_async_fail()
{
    struct RaRequestHandle *req_handle = NULL;
    struct RaSocketHandle socket_handle = {0};
    struct SocketListenInfoT conn[1] = {0};
    int ret = 0;

    conn[0].socketHandle = (void *)&socket_handle;

    mocker_clean();
    mocker(RaInetPton, 10, 0);
    mocker(RaHdcSendMsgAsync, 10, 0);
    mocker(RaGetSocketListenInfo, 10, 0);

    ret = RaSocketListenStopAsync(NULL, 1, &req_handle);
    EXPECT_INT_NE(ret, 0);

    ret = RaSocketListenStopAsync(conn, 0, &req_handle);
    EXPECT_INT_NE(ret, 0);

    ret = RaSocketListenStopAsync(conn, 1, NULL);
    EXPECT_INT_NE(ret, 0);

    conn[0].socketHandle = NULL;
    ret = RaSocketListenStopAsync(conn, 1, &req_handle);
    EXPECT_INT_NE(ret, 0);

    mocker_clean();
    return;
}

void tc_ra_socket_listen_stop_async()
{
    tc_ra_socket_listen_stop_async_normal();
    tc_ra_socket_listen_stop_async_fail();
}

void tc_ra_socket_batch_close_async_normal()
{
    struct RaRequestHandle *req_handle = NULL;
    struct RaSocketHandle socket_handle = {0};
    struct SocketHdcInfo fd_handle = {0};
    struct SocketCloseInfoT conn[1] = {0};
    int ret = 0;

    conn[0].socketHandle = (void *)&socket_handle;
    conn[0].fdHandle = (void *)&fd_handle;

    mocker_clean();
    mocker(RaInetPton, 10, 0);
    mocker(RaHdcSendMsgAsync, 10, 0);
    mocker(RaGetSocketConnectInfo, 10, 0);

    ret = RaSocketBatchCloseAsync(conn, 1, &req_handle);
    free(req_handle->privData);
    free(req_handle);
    mocker_clean();
    return;
}

void tc_ra_socket_batch_close_async_fail()
{
    struct RaRequestHandle *req_handle = NULL;
    struct RaSocketHandle socket_handle = {0};
    struct SocketHdcInfo fd_handle = {0};
    struct SocketCloseInfoT conn[1] = {0};
    int ret = 0;

    conn[0].socketHandle = (void *)&socket_handle;
    conn[0].fdHandle = (void *)&fd_handle;

    mocker_clean();
    mocker(RaInetPton, 10, 0);
    mocker(RaHdcSendMsgAsync, 10, 0);
    mocker(RaGetSocketConnectInfo, 10, 0);

    ret = RaSocketBatchCloseAsync(NULL, 1, &req_handle);
    EXPECT_INT_NE(ret, 0);

    ret = RaSocketBatchCloseAsync(conn, 0, &req_handle);
    EXPECT_INT_NE(ret, 0);

    ret = RaSocketBatchCloseAsync(conn, 1, NULL);
    EXPECT_INT_NE(ret, 0);

    conn[0].socketHandle = NULL;
    ret = RaSocketBatchCloseAsync(conn, 1, &req_handle);
    EXPECT_INT_NE(ret, 0);

    mocker_clean();
    return;
}

void tc_ra_socket_batch_close_async()
{
    tc_ra_socket_batch_close_async_normal();
    tc_ra_socket_batch_close_async_fail();
}

void tc_ra_hdc_async_init_session()
{
    unsigned int connect_status = HDC_CONNECTED;
    struct RaInitConfig cfg = {0};
    unsigned int phyId = 0;
    int ret = 0;

    mocker(pthread_create, 2, 0);
    mocker(DlDrvDeviceGetBareTgid, 1, 0);
    mocker(RaHdcAsyncMutexInit, 1, -1);
    RaHwAsyncSetConnectStatus(phyId, connect_status);
    ret = RaHdcAsyncInitSession(&cfg);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();
}

void tc_ra_get_eid_by_ip_async()
{
    struct RaRequestHandle *req_handle = NULL;
    struct ra_ctx_handle ctx_handle = {0};
    union hccp_eid eid[32] = {0};
    struct IpInfo ip[32] = {0};
    unsigned int num = 32;
    int ret = 0;

    mocker_clean();
    ret = ra_get_eid_by_ip_async(NULL, ip, eid, &num, &req_handle);
    EXPECT_INT_EQ(ret, 128103);

    num = 33;
    ret = ra_get_eid_by_ip_async(&ctx_handle, ip, eid, &num, &req_handle);
    EXPECT_INT_EQ(ret, 128103);

    num = 32;
    mocker(ra_hdc_get_eid_by_ip_async, 10, -1);
    ret = ra_get_eid_by_ip_async(&ctx_handle, ip, eid, &num, &req_handle);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

    mocker(ra_hdc_get_eid_by_ip_async, 10, 0);
    ret = ra_get_eid_by_ip_async(&ctx_handle, ip, eid, &num, &req_handle);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

void tc_ra_hdc_get_eid_by_ip_async()
{
    union op_get_eid_by_ip_data *async_data = NULL;
    struct ra_response_eid_list *priv_data = NULL;
    struct RaRequestHandle *req_handle = NULL;
    struct ra_ctx_handle ctx_handle = {0};
    unsigned int priv_data_num = 1;
    union hccp_eid eid[32] = {0};
    struct IpInfo ip[32] = {0};
    unsigned int num = 32;
    int ret = 0;

    mocker_clean();
    mocker(calloc, 1, NULL);
    ret = ra_hdc_get_eid_by_ip_async(&ctx_handle, ip, eid, &num, &req_handle);
    EXPECT_INT_EQ(ret, -ENOMEM);
    mocker_clean();

    mocker_invoke(calloc, calloc_first_stub, 2);
    ret = ra_hdc_get_eid_by_ip_async(&ctx_handle, ip, eid, &num, &req_handle);
    EXPECT_INT_EQ(ret, -ENOMEM);
    mocker_clean();

    mocker(RaHdcSendMsgAsync, 1, -1);
    ret = ra_hdc_get_eid_by_ip_async(&ctx_handle, ip, eid, &num, &req_handle);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

    mocker(RaHdcSendMsgAsync, 1, 0);
    ret = ra_hdc_get_eid_by_ip_async(&ctx_handle, ip, eid, &num, &req_handle);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    mocker(memcpy_s, 1, 0);
    async_data = calloc(1, sizeof(union op_get_eid_by_ip_data));
    req_handle->opRet = 0;
    async_data->rx_data.num = 1;
    priv_data = (struct ra_response_eid_list *)(req_handle->privData);
    priv_data->num = &priv_data_num;
    req_handle->recvBuf = (void *)async_data;
    ra_hdc_async_handle_get_eid_by_ip(req_handle);
    mocker_clean();

    free(async_data);
    async_data = NULL;
    free(req_handle);
    req_handle = NULL;
}

void tc_ra_hdc_async_session_close()
{
    int ret = 0;

    mocker_clean();
    pthread_mutex_init(&gRaHdcAsync[0].reqMutex, NULL);
    mocker_invoke(RaHdcSendMsgAsync, ra_hdc_send_msg_async_stub, 1);
    mocker(RaHdcProcessMsg, 1, 0);
    mocker_invoke(HdcAsyncDelResponse, hdc_async_del_response_stub, 1);
    ret = RaHdcAsyncSessionClose(0);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    pthread_mutex_destroy(&gRaHdcAsync[0].reqMutex);
}

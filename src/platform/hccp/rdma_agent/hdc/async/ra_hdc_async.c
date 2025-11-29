/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <stdlib.h>
#include <sys/prctl.h>
#include "securec.h"
#include "user_log.h"
#include "dl_hal_function.h"
#include "ra.h"
#include "ra_async.h"
#include "ra_rs_comm.h"
#include "ra_rs_err.h"
#include "ra_hdc.h"
#include "ra_hdc_socket.h"
#include "ra_hdc_async_socket.h"
#include "ra_hdc_async.h"

struct hdc_async_info gRaHdcAsync[RA_MAX_PHY_ID_NUM] = { 0 };

struct ra_async_op_handle gRaAsyncOpHandle[] = {
    {RA_RS_SOCKET_SEND, SOCKET_OP, RaHdcAsyncHandleSocketSend, sizeof(union op_socket_send_data)},
    {RA_RS_SOCKET_RECV, SOCKET_OP, RaHdcAsyncHandleSocketRecv, sizeof(union op_socket_recv_data)},
    {RA_RS_SOCKET_LISTEN_START, SOCKET_OP, RaHdcAsyncHandleSocketListenStart,
        sizeof(union op_socket_listen_data)},
    {RA_RS_SOCKET_LISTEN_STOP, SOCKET_OP, NULL, sizeof(union op_socket_listen_data)},
    {RA_RS_SOCKET_CONN, SOCKET_OP, NULL, sizeof(union op_socket_connect_data)},
    {RA_RS_SOCKET_CLOSE, SOCKET_OP, RaHdcAsyncHandleSocketBatchClose, sizeof(union op_socket_close_data)},
    {RA_RS_HDC_SESSION_CLOSE, OTHERS, NULL, sizeof(union op_hdc_close_data)},
};

STATIC struct ra_async_op_handle *RaHdcIsAsyncOp(unsigned int opcode)
{
    int num = sizeof(gRaAsyncOpHandle) / sizeof(gRaAsyncOpHandle[0]);
    int i;

    for (i = 0; i < num; i++) {
        if (gRaAsyncOpHandle[i].opcode == (enum op_type)opcode) {
            return &gRaAsyncOpHandle[i];
        }
    }
    return NULL;
}

STATIC void HdcAsyncHandlePrivData(struct ra_request_handle *reqHandle)
{
    if (reqHandle->op_handle->priv_data_handle == NULL) {
        return;
    }

    reqHandle->op_handle->priv_data_handle(reqHandle);
}

STATIC void HdcAsyncSetRequest(struct ra_request_handle *reqHandle, unsigned int reqId,
    struct ra_async_op_handle *opHandle, unsigned int phyId, unsigned int dataSize)
{
    reqHandle->req_id = reqId;
    reqHandle->op_handle = opHandle;
    reqHandle->phy_id = phyId;
    reqHandle->data_size = dataSize;
}

STATIC int HdcAsyncGetRequest(struct hdc_async_info *asyncInfo, unsigned int reqId,
    struct ra_request_handle **reqHandle)
{
    struct ra_request_handle *reqTmp2 = NULL;
    struct ra_request_handle *reqTmp = NULL;

    // no need to use lock: req_id always exist in current req_list(the data is always sent before it is received)
    RA_LIST_GET_HEAD_ENTRY(reqTmp, reqTmp2, &asyncInfo->req_list, list, struct ra_request_handle);
    for (; (&reqTmp->list) != &asyncInfo->req_list;
        reqTmp = reqTmp2, reqTmp2 = list_entry(reqTmp2->list.next, struct ra_request_handle, list)) {
        if (reqTmp->req_id == reqId) {
            *reqHandle = reqTmp;
            return 0;
        }
    }
    *reqHandle = NULL;
    return -ENODEV;
}

STATIC void HdcAsyncSetReqDone(struct ra_request_handle *reqHandle, unsigned int phyId, int ret)
{
    RA_PTHREAD_MUTEX_LOCK(&gRaHdcAsync[phyId].rsp_mutex);
    RaListAddTail(&reqHandle->list, &gRaHdcAsync[phyId].rsp_list);
    RA_PTHREAD_MUTEX_UNLOCK(&gRaHdcAsync[phyId].rsp_mutex);
    reqHandle->op_ret = (ret != 0) ? ret : reqHandle->op_ret;
    hccp_dbg("opcode[%u] req_id[%u] phy_id[%u]", reqHandle->op_handle->opcode, reqHandle->req_id, phyId);
    reqHandle->is_done = true;
}

STATIC void RaHwAsyncSetConnectStatus(unsigned int phyId, unsigned int connectStatus)
{
    gRaHdcAsync[phyId].connect_status = connectStatus;
}

STATIC bool HdcAsyncIsMsgValid(unsigned int phyId, struct msg_head *recvMsgHead, unsigned int recvLen,
    struct ra_request_handle **reqHandle)
{
    struct ra_request_handle *reqHandleTmp = NULL;
    int ret;

    // check recv_len and get req_handle
    CHK_PRT_RETURN(recvLen < sizeof(struct msg_head),
        hccp_run_warn("[async][ra_hdc_recv]recv_len[%u] < [%lu] is invalid", recvLen, sizeof(struct msg_head)), false);
    ret = HdcAsyncGetRequest(&gRaHdcAsync[phyId], recvMsgHead->async_req_id, &reqHandleTmp);
    CHK_PRT_RETURN(reqHandleTmp == NULL, hccp_run_warn("[async][ra_hdc_recv]req_id[%u] invalid, ret[%d], opcode[%u]",
        recvMsgHead->async_req_id, ret, recvMsgHead->opcode), false);

    // del req_handle from req_list
    RaListDel(&reqHandleTmp->list);

    // opcode RA_RS_HDC_SESSION_CLOSE
    if (recvMsgHead->opcode == RA_RS_HDC_SESSION_CLOSE) {
        RaHwAsyncSetConnectStatus(phyId, HDC_UNCONNECTED);
        hccp_dbg("opcode[%u] req_id[%u] phy_id[%u]", recvMsgHead->opcode, reqHandleTmp->req_id, phyId);
        reqHandleTmp->is_done = true;
        return false;
    }

    // need to check op_data size and recv data size
    if ((reqHandleTmp->data_size != recvMsgHead->msg_data_len) ||
        (recvMsgHead->msg_data_len + (unsigned int)sizeof(struct msg_head)) != recvLen) {
        hccp_run_warn("[async][ra_hdc_recv]opcode[%u] data_size[%u] msg_data_len[%u] mismatch or recv_len[%u] mismatch",
            recvMsgHead->opcode, reqHandleTmp->data_size, recvMsgHead->msg_data_len, recvLen);
        HdcAsyncSetReqDone(reqHandleTmp, phyId, -EINVAL);
        return false;
    }

    *reqHandle = reqHandleTmp;
    return true;
}

STATIC int HdcAsyncAddResponse(unsigned int phyId, void *recvBuf, unsigned int recvLen)
{
    struct ra_request_handle *reqHandleTmp = NULL;
    struct msg_head *recvMsgHead = NULL;
    int ret = 0;

    recvMsgHead = (struct msg_head *)recvBuf;
    // check recv msg: req_id, opcode, msg_data_len and get req_handle
    if (!HdcAsyncIsMsgValid(phyId, recvMsgHead, recvLen, &reqHandleTmp)) {
        return -EINVAL;
    }

    //  handle recv msg
    reqHandleTmp->recv_buf = (void *)calloc(recvMsgHead->msg_data_len, sizeof(char));
    if (reqHandleTmp->recv_buf == NULL) {
        hccp_err("[async][ra_hdc_recv]calloc recv_buf failed, msg_data_len[%u] reqId[%u] opcode[%u]",
            recvMsgHead->msg_data_len, recvMsgHead->async_req_id, recvMsgHead->opcode);
        ret = -ENOMEM;
        goto out;
    }
    (void)memcpy_s(reqHandleTmp->recv_buf, recvMsgHead->msg_data_len, recvBuf + sizeof(struct msg_head),
        recvMsgHead->msg_data_len);
    reqHandleTmp->recv_len = recvMsgHead->msg_data_len;
    reqHandleTmp->op_ret = recvMsgHead->ret;
    HdcAsyncHandlePrivData(reqHandleTmp);

out:
    HdcAsyncSetReqDone(reqHandleTmp, phyId, ret);
    return ret;
}

static void HdcAsyncDelReqHandle(struct ra_request_handle *reqHandle, pthread_mutex_t *mutex)
{
    RA_PTHREAD_MUTEX_LOCK(mutex);
    RaListDel(&reqHandle->list);
    RA_PTHREAD_MUTEX_UNLOCK(mutex);
    if (reqHandle->recv_buf != NULL && reqHandle->recv_len != 0) {
        free(reqHandle->recv_buf);
        reqHandle->recv_buf = NULL;
        reqHandle->recv_len = 0;
    }
    // async api return failed, free corresponding handle
    if (reqHandle->op_ret != 0 && reqHandle->priv_handle != NULL) {
        free(reqHandle->priv_handle);
        reqHandle->priv_handle = NULL;
    }
    free(reqHandle);
    reqHandle = NULL;
    return;
}

void HdcAsyncDelResponse(struct ra_request_handle *reqHandle)
{
    HdcAsyncDelReqHandle(reqHandle, &gRaHdcAsync[reqHandle->phy_id].rsp_mutex);
}

int RaHdcSendMsgAsync(unsigned int opcode, unsigned int phyId, char *data, unsigned int dataSize,
    struct ra_request_handle *reqHandle)
{
    struct ra_async_op_handle *opHandleTmp = NULL;
    unsigned int asyncReqId = 0;
    void *sendBuf = NULL;
    unsigned int sendLen;
    pid_t hostTgid;
    int ret;

    if (gRaHdcAsync[phyId].restore_flag != 0) {
        return 0;
    }

    CHK_PRT_RETURN(RaHdcIsBroken(gRaHdcAsync[phyId].last_recv_status),
        hccp_err("[async][ra_hdc_send]HDC broken, phyId(%u)", phyId), -gRaHdcAsync[phyId].last_recv_status);
    opHandleTmp = RaHdcIsAsyncOp(opcode);
    CHK_PRT_RETURN(opHandleTmp == NULL, hccp_err("[async][ra_hdc_send]opcode[%u] invalid", opcode), -EINVAL);

    hostTgid = gRaHdcAsync[phyId].host_tgid;
    sendLen = (unsigned int)sizeof(struct msg_head) + dataSize;
    sendBuf = (void *)calloc(sendLen, sizeof(char));
    CHK_PRT_RETURN(sendBuf == NULL, hccp_err("[async][ra_hdc_send]calloc send_buf failed. phy_id(%u) opcode(%u)",
        phyId, opcode), -ENOMEM);

    RA_PTHREAD_MUTEX_LOCK(&gRaHdcAsync[phyId].req_mutex);
    asyncReqId = gRaHdcAsync[phyId].req_id;
    gRaHdcAsync[phyId].req_id++;

    MsgHeadBuildUp(sendBuf, opcode, asyncReqId, dataSize, hostTgid);
    ret = memcpy_s(sendBuf + sizeof(struct msg_head), sendLen - sizeof(struct msg_head), data, dataSize);
    if (ret != 0) {
        hccp_err("[async][ra_hdc_send]memcpy_s failed, ret(%d) phyId(%u) opcode(%u)", ret, phyId, opcode);
        ret = -ESAFEFUNC;
        goto out;
    }

    HdcAsyncSetRequest(reqHandle, asyncReqId, opHandleTmp, phyId, dataSize);
    ret = HdcAsyncSendPkt(&gRaHdcAsync[phyId], phyId, sendBuf, sendLen, reqHandle);
    if (ret != 0) {
        hccp_err("[async][ra_hdc_send]hdc_async_send_pkt opcode(%u) failed ret(%d) phy_id(%u)", opcode, ret, phyId);
        goto out;
    }

    hccp_dbg("opcode[%u] req_id[%u] phy_id[%u]", opHandleTmp->opcode, asyncReqId, phyId);

out:
    RA_PTHREAD_MUTEX_UNLOCK(&gRaHdcAsync[phyId].req_mutex);
    free(sendBuf);
    sendBuf = NULL;
    return ret;
}

STATIC int RaHdcAsyncSessionConnect(struct ra_init_config *cfg)
{
    union op_async_hdc_connect_data asyncData = {0};
    int ret;

    asyncData.tx_data.phy_id = cfg->phy_id;
    asyncData.tx_data.queue_size = MAX_POOL_QUEUE_SIZE;
    asyncData.tx_data.thread_num = RA_POOL_THREAD_NUM;
    ret = RaHdcProcessMsg(RA_RS_ASYNC_HDC_SESSION_CONNECT, cfg->phy_id, (char *)&asyncData,
        sizeof(union op_async_hdc_connect_data));
    CHK_PRT_RETURN(ret != 0, hccp_err("[init][ra_hdc_async]ra hdc message process failed ret[%d] phy_id[%u]",
        ret, cfg->phy_id), ret);
    return ret;
}

STATIC int RaHdcAsyncSessionClose(unsigned int phyId)
{
    union op_async_hdc_close_data asyncData = {0};
    struct ra_request_handle *reqHandle = NULL;
    union op_hdc_close_data opData = {0};
    int timeout = RA_THREAD_TRY_TIME;
    int ret;

    if (gRaHdcAsync[phyId].restore_flag != 0) {
        return 0;
    }

    // close async session
    opData.tx_data.phy_id = phyId;
    reqHandle = (struct ra_request_handle *)calloc(1, sizeof(struct ra_request_handle));
    CHK_PRT_RETURN(reqHandle == NULL,
        hccp_err("[deinit][ra_hdc_async]calloc req_handle failed, phyId[%u]", phyId), -ENOMEM);
    ret = RaHdcSendMsgAsync(RA_RS_HDC_SESSION_CLOSE, phyId, (char *)&opData, sizeof(union op_hdc_close_data),
        reqHandle);
    if (ret != 0) {
        hccp_err("[deinit][ra_hdc_async]hdc async send message failed ret[%d] phy_id[%u]", ret, phyId);
        free(reqHandle);
        reqHandle = NULL;
        return ret;
    }

    // wait request done until time out: RA_THREAD_TRY_TIME * RA_THREAD_SLEEP_TIME us
    while (!reqHandle->is_done && timeout > 0) {
        usleep(RA_THREAD_SLEEP_TIME);
        timeout--;
    }
    if (timeout <= 0) {
        hccp_warn("[deinit][ra_hdc_async]hdc async session close timeout:%d phy_id[%u]", timeout, phyId);
    }
    free(reqHandle);
    reqHandle = NULL;

    // destroy async recv thread and work thread pool
    ret = RaHdcProcessMsg(RA_RS_ASYNC_HDC_SESSION_CLOSE, phyId, (char *)&asyncData,
        sizeof(union op_async_hdc_close_data));
    CHK_PRT_RETURN(ret != 0, hccp_err("[deinit][ra_hdc_async]ra hdc message process failed ret[%d] phy_id[%u]",
        ret, phyId), ret);
    return ret;
}

STATIC void RaHwAsyncHdcServerInit(void *arg)
{
    struct ra_init_config cfg = {0};
    int ret;

    if (arg == NULL) {
        hccp_err("[init][ra_hdc_async]arg is NULL");
        return;
    }

    cfg = *(struct ra_init_config *)arg;
    ret = pthread_detach(pthread_self());
    if (ret != 0) {
        hccp_err("[init][ra_hdc_async]pthread detach failed ret %d", ret);
        return;
    }

    (void)prctl(PR_SET_NAME, (unsigned long)"hccp_async_server");

    // trigger server to connect session
    ret = RaHdcAsyncSessionConnect(&cfg);
    if (ret != 0) {
        hccp_err("[init][ra_hdc_async]ra_hdc_async_session_connect failed ret[%d] phy_id[%u]", ret, cfg.phy_id);
        return;
    }
    return;
}

STATIC void RaHwAsyncHdcClientInit(void *arg)
{
    struct ra_init_config cfg = {0};
    unsigned int logicId = 0;
    unsigned int phyId = 0;
    int ret;

    if (arg == NULL) {
        hccp_err("[init][ra_hdc_async]arg is NULL");
        return;
    }

    cfg = *(struct ra_init_config *)arg;
    ret = pthread_detach(pthread_self());
    if (ret != 0) {
        hccp_err("[init][ra_hdc_async]pthread detach failed ret %d", ret);
        return;
    }

    (void)prctl(PR_SET_NAME, (unsigned long)"hccp_async_client");

    phyId = cfg.phy_id;
    ret = DlDrvDeviceGetIndexByPhyId(phyId, &logicId);
    if (ret != 0) {
        hccp_err("get logic id failed(%d), phyId(%u)", ret, phyId);
        return;
    }

    ret = RaHdcInitSession(0, (int)logicId, phyId, cfg.hdc_type, &gRaHdcAsync[phyId].session);
    if (ret != 0) {
        hccp_err("hdc session_connect failed ret(%d) phy_id(%u)", ret, phyId);
        return;
    }

    ret = RaHdcSetSessionReference(&gRaHdcAsync[phyId].session);
    if (ret != 0) {
        goto set_ref_err;
    }

    RaHwAsyncSetConnectStatus(phyId, HDC_CONNECTED);
    return;

set_ref_err:
    RaHdcDeinitSession(&gRaHdcAsync[phyId].session);
    return;
}

STATIC void RaHwAsyncSetThreadStatus(unsigned int phyId, unsigned int threadStatus)
{
    gRaHdcAsync[phyId].thread_status = threadStatus;
}

STATIC void RaHwAsyncDelList(struct ra_list_head *head, pthread_mutex_t *mutex)
{
    struct ra_request_handle *reqNext = NULL;
    struct ra_request_handle *reqCur = NULL;

    RA_LIST_GET_HEAD_ENTRY(reqCur, reqNext, head, list, struct ra_request_handle);
    for (; (&reqCur->list) != head;
        reqCur = reqNext, reqNext = list_entry(reqNext->list.next, struct ra_request_handle, list)) {
        HdcAsyncDelReqHandle(reqCur, mutex);
    }
}

STATIC void RaHwAsyncHdcClientDeinit(unsigned int phyId)
{
    int tryAgain = HDC_TRY_TIME;

    // destroy thread
    RaHwAsyncSetThreadStatus(phyId, THREAD_DESTROYING);
    while ((gRaHdcAsync[phyId].thread_status != THREAD_HALT) && (tryAgain != 0)) {
        usleep(HDC_USLEEP_TIME);
        tryAgain--;
    }
    if (tryAgain <= 0) {
        hccp_warn("hdc async message thread quit timeout");
    }

    // close session
    RaHwAsyncSetConnectStatus(phyId, HDC_UNCONNECTED);
    RA_PTHREAD_MUTEX_LOCK(&gRaHdcAsync[phyId].send_mutex);
    RA_PTHREAD_MUTEX_LOCK(&gRaHdcAsync[phyId].recv_mutex);
    RaHdcDeinitSession(&gRaHdcAsync[phyId].snapshot_session);
    RaHdcDeinitSession(&gRaHdcAsync[phyId].session);
    RA_PTHREAD_MUTEX_UNLOCK(&gRaHdcAsync[phyId].recv_mutex);
    RA_PTHREAD_MUTEX_UNLOCK(&gRaHdcAsync[phyId].send_mutex);

    RaHwAsyncDelList(&gRaHdcAsync[phyId].req_list, &gRaHdcAsync[phyId].req_mutex);
    RaHwAsyncDelList(&gRaHdcAsync[phyId].rsp_list, &gRaHdcAsync[phyId].rsp_mutex);
}

STATIC int RaHdcAsyncMutexInit(unsigned int phyId)
{
    int ret = 0;

    ret = pthread_mutex_init(&gRaHdcAsync[phyId].send_mutex, NULL);
    if (ret != 0) {
        hccp_err("[init][ra_hdc_async]pthread_mutex_init send_mutex failed ret(%d) phy_id(%u)", ret, phyId);
        return -ESYSFUNC;
    }
    ret = pthread_mutex_init(&gRaHdcAsync[phyId].recv_mutex, NULL);
    if (ret != 0) {
        hccp_err("[init][ra_hdc_async]pthread_mutex_init recv_mutex failed ret(%d) phy_id(%u)", ret, phyId);
        ret = -ESYSFUNC;
        goto recv_mutex_fail;
    }
    ret = pthread_mutex_init(&gRaHdcAsync[phyId].req_mutex, NULL);
    if (ret != 0) {
        hccp_err("[init][ra_hdc_async]pthread_mutex_init req_mutex failed ret(%d) phy_id(%u)", ret, phyId);
        ret = -ESYSFUNC;
        goto req_mutex_fail;
    }
    ret = pthread_mutex_init(&gRaHdcAsync[phyId].rsp_mutex, NULL);
    if (ret != 0) {
        hccp_err("[init][ra_hdc_async]pthread_mutex_init rsp_mutex failed ret(%d) phy_id(%u)", ret, phyId);
        ret = -ESYSFUNC;
        goto rsp_mutex_fail;
    }

    return 0;

rsp_mutex_fail:
    (void)pthread_mutex_destroy(&gRaHdcAsync[phyId].req_mutex);
req_mutex_fail:
    (void)pthread_mutex_destroy(&gRaHdcAsync[phyId].recv_mutex);
recv_mutex_fail:
    (void)pthread_mutex_destroy(&gRaHdcAsync[phyId].send_mutex);
    return ret;
}

STATIC void RaHdcAsyncMutexDeinit(unsigned int phyId)
{
    (void)pthread_mutex_destroy(&gRaHdcAsync[phyId].rsp_mutex);
    (void)pthread_mutex_destroy(&gRaHdcAsync[phyId].req_mutex);
    (void)pthread_mutex_destroy(&gRaHdcAsync[phyId].recv_mutex);
    (void)pthread_mutex_destroy(&gRaHdcAsync[phyId].send_mutex);
}

STATIC int RaHdcAsyncInitSession(struct ra_init_config *cfg)
{
    unsigned int phyId = cfg->phy_id;
    int timeout = RA_THREAD_TRY_TIME;
    pthread_t serverTidp;
    pthread_t clientTidp;
    int ret = 0;

    CHK_PRT_RETURN(gRaHdcAsync[phyId].session != NULL, hccp_warn("hdc async session for phy_id[%u] already existed",
        phyId), -EEXIST);

    // server will be blocked, use a thread to trigger server to accept
    ret = pthread_create(&serverTidp, NULL, (void *)RaHwAsyncHdcServerInit, cfg);
    CHK_PRT_RETURN(ret != 0, hccp_err("Create async_hdc_server_init pthread failed, ret(%d)", ret), -ESYSFUNC);

    // client will be blocked, use a thread to trigger client to connect
    ret = pthread_create(&clientTidp, NULL, (void *)RaHwAsyncHdcClientInit, cfg);
    CHK_PRT_RETURN(ret != 0, hccp_err("Create async_hdc_client_init pthread failed, ret(%d)", ret), -ESYSFUNC);

    // will block until time out: RA_CONNECT_TRY_TIME * RA_THREAD_SLEEP_TIME us
    timeout = RA_CONNECT_TRY_TIME;
    while (gRaHdcAsync[phyId].connect_status != HDC_CONNECTED && timeout > 0) {
        usleep(RA_THREAD_SLEEP_TIME);
        timeout--;
    }
    if (gRaHdcAsync[phyId].connect_status == HDC_UNCONNECTED || timeout <= 0) {
        hccp_err("HDC async connect timeout, connectStatus %d, timeout %d, total_timeout %d(us)",
            gRaHdcAsync[phyId].connect_status, timeout, RA_CONNECT_TRY_TIME * RA_THREAD_SLEEP_TIME);
        return -ETIMEDOUT;
    }

    gRaHdcAsync[phyId].host_tgid = DlDrvDeviceGetBareTgid();
    ret = RaHdcAsyncMutexInit(phyId);
    CHK_PRT_RETURN(ret != 0, hccp_err("ra_hdc_async_mutex_init failed, ret(%d), phyId(%u)", ret, phyId), ret);

    RA_INIT_LIST_HEAD(&gRaHdcAsync[phyId].req_list);
    RA_INIT_LIST_HEAD(&gRaHdcAsync[phyId].rsp_list);
    return 0;
}

STATIC void HdcAsyncHandleRecvBroken(struct hdc_async_info *asyncInfo)
{
    struct ra_request_handle *reqNext = NULL;
    struct ra_request_handle *reqCurr = NULL;

    if (!RaHdcIsBroken(asyncInfo->last_recv_status)) {
        return;
    }

    RA_PTHREAD_MUTEX_LOCK(&asyncInfo->req_mutex);
    RA_LIST_GET_HEAD_ENTRY(reqCurr, reqNext, &asyncInfo->req_list, list, struct ra_request_handle);
    for (; (&reqCurr->list) != &asyncInfo->req_list;
        reqCurr = reqNext, reqNext = list_entry(reqNext->list.next, struct ra_request_handle, list)) {
        RaListDel(&reqCurr->list);
        HdcAsyncSetReqDone(reqCurr, reqCurr->phy_id, -asyncInfo->last_recv_status);
    }
    RA_PTHREAD_MUTEX_UNLOCK(&asyncInfo->req_mutex);
}

STATIC void *RaHdcRecvMsgAsync(void *arg)
{
    unsigned int phyId = *(unsigned int *)arg;
    unsigned int recvLen = MAX_HDC_MSG_DATA;
    void *recvBuf = NULL;
    int ret;

    // free memory after using arg
    free(arg);
    arg = NULL;

    ret = pthread_detach(pthread_self());
    CHK_PRT_RETURN(ret, hccp_err("pthread detach failed ret %d, phyId %u", ret, phyId), NULL);

    (void)prctl(PR_SET_NAME, (unsigned long)"hccp_ra_async");

    hccp_info("[async][ra_hdc_recv]thread[%d] phy_id[%u] enter", getpid(), phyId);
    RaHwAsyncSetThreadStatus(phyId, THREAD_RUNNING);
    recvBuf = (void *)calloc(recvLen, sizeof(char));
    CHK_PRT_RETURN(recvBuf == NULL, hccp_err("[async][ra_hdc_recv]calloc recv_buf failed. phy_id(%u)", phyId), NULL);

    while (1) {
        if (gRaHdcAsync[phyId].thread_status == THREAD_DESTROYING) {
            break;
        }
        RA_PTHREAD_MUTEX_LOCK(&gRaHdcAsync[phyId].recv_mutex);
        if (gRaHdcAsync[phyId].connect_status != HDC_CONNECTED) {
            RA_PTHREAD_MUTEX_UNLOCK(&gRaHdcAsync[phyId].recv_mutex);
            usleep(THREAD_SLEEP_TIME);
            continue;
        }

        if (RaListEmpty(&gRaHdcAsync[phyId].req_list)) {
            RA_PTHREAD_MUTEX_UNLOCK(&gRaHdcAsync[phyId].recv_mutex);
            usleep(THREAD_SLEEP_TIME);
            continue;
        }

        recvLen = MAX_HDC_MSG_DATA;
        ret = HdcAsyncRecvPkt(&gRaHdcAsync[phyId], phyId, recvBuf, &recvLen);
        if (ret != 0) {
            RA_PTHREAD_MUTEX_UNLOCK(&gRaHdcAsync[phyId].recv_mutex);
            HdcAsyncHandleRecvBroken(&gRaHdcAsync[phyId]);
            continue;
        }
        RA_PTHREAD_MUTEX_UNLOCK(&gRaHdcAsync[phyId].recv_mutex);

        RA_PTHREAD_MUTEX_LOCK(&gRaHdcAsync[phyId].req_mutex);
        (void)HdcAsyncAddResponse(phyId, recvBuf, recvLen);
        RA_PTHREAD_MUTEX_UNLOCK(&gRaHdcAsync[phyId].req_mutex);
    }

    hccp_info("[async][ra_hdc_recv]thread[%d] phy_id[%u] is out", getpid(), phyId);
    RaHwAsyncSetThreadStatus(phyId, THREAD_HALT);
    free(recvBuf);
    recvBuf = NULL;
    return NULL;
}

STATIC int RaHdcAsyncInitRecvThread(unsigned int phyId)
{
    unsigned int *phyIdTmp = NULL;
    int ret = 0;

    phyIdTmp = (unsigned int *)calloc(1, sizeof(unsigned int));
    CHK_PRT_RETURN(phyIdTmp == NULL, hccp_err("calloc phy_id_tmp failed, errno(%d)", errno), -ENOMEM);
    *phyIdTmp = phyId;

    // create a thread to recv msg from server
    ret = pthread_create(&gRaHdcAsync[phyId].tid, NULL, RaHdcRecvMsgAsync, (void *)phyIdTmp);
    if (ret != 0) {
        hccp_err("Create ra_hdc_recv_msg_async pthread failed, ret(%d)", ret);
        goto err;
    }

    return 0;

err:
    free(phyIdTmp);
    phyIdTmp = NULL;
    return ret;
}

int RaHdcInitAsync(struct ra_init_config *cfg)
{
    unsigned int interfaceVersion = 0;
    int ret = 0;

    CHK_PRT_RETURN(!cfg->enable_hdc_async, hccp_info("[init][ra_hdc_async]no need to init async hdc session"), 0);

    ret = RaHdcGetInterfaceVersion(cfg->phy_id, RA_RS_ASYNC_HDC_SESSION_CONNECT, &interfaceVersion);
    // normal case: driver not support to or no need to init async hdc session
    CHK_PRT_RETURN(ret != 0 || interfaceVersion < RA_RS_OPCODE_BASE_VERSION,
        hccp_run_warn("[init][ra_hdc_async]not support to init async hdc session, ret(%d), interfaceVersion(%u)",
        ret, interfaceVersion), 0);

    ret = RaHdcAsyncInitSession(cfg);
    CHK_PRT_RETURN(ret != 0, hccp_err("[init][ra_hdc_async]ra_hdc_async_init_session failed, ret(%d) phyId(%u)",
        ret, cfg->phy_id), ret);

    ret = RaHdcAsyncInitRecvThread(cfg->phy_id);
    if (ret != 0) {
        hccp_err("[init][ra_hdc_async]ra_hdc_async_init_recv_thread failed, ret(%d) phyId(%u)", ret, cfg->phy_id);
        goto err;
    }

    return 0;

err:
    RaHdcAsyncMutexDeinit(cfg->phy_id);
    return -ESRCH;
}

int RaHdcDeinitAsync(unsigned int phyId)
{
    int ret;

    hccp_run_info("hdc deinit async start! phy_id[%u] restore_flag[%u]", phyId, gRaHdcAsync[phyId].restore_flag);

    CHK_PRT_RETURN(gRaHdcAsync[phyId].session == NULL && gRaHdcAsync[phyId].restore_flag == 0,
        hccp_warn("hdc async session for phy_id[%u] is NULL", phyId), -ENODEV);

    // close server session
    ret = RaHdcAsyncSessionClose(phyId);
    CHK_PRT_RETURN(ret != 0, hccp_err("[deinit][ra_hdc_async]ra_hdc_async_session_close failed ret[%d] phy_id[%u]",
        ret, phyId), ret);

    // close client session & deinit client resources
    RaHwAsyncHdcClientDeinit(phyId);

    RaHdcAsyncMutexDeinit(phyId);

    (void)memset_s(&gRaHdcAsync[phyId], sizeof(gRaHdcAsync[phyId]), 0, sizeof(gRaHdcAsync[phyId]));

    return 0;
}

int RaHdcAsyncSaveSnapshot(unsigned int phyId, enum save_snapshot_action action)
{
    int ret = 0;

    if (gRaHdcAsync[phyId].thread_status == THREAD_HALT) {
        return 0;
    }

#ifndef HNS_ROCE_LLT
    RA_PTHREAD_MUTEX_LOCK(&gRaHdcAsync[phyId].send_mutex);
    RA_PTHREAD_MUTEX_LOCK(&gRaHdcAsync[phyId].recv_mutex);
    if (action == SAVE_SNAPSHOT_ACTION_PRE_PROCESSING && gRaHdcAsync[phyId].session != NULL) {
        RaHwAsyncSetConnectStatus(phyId, HDC_UNCONNECTED);
        gRaHdcAsync[phyId].snapshot_session = gRaHdcAsync[phyId].session;
        gRaHdcAsync[phyId].session = NULL;
    } else if (action == SAVE_SNAPSHOT_ACTION_POST_PROCESSING && gRaHdcAsync[phyId].session == NULL) {
        RaHwAsyncSetConnectStatus(phyId, HDC_CONNECTED);
        gRaHdcAsync[phyId].session = gRaHdcAsync[phyId].snapshot_session;
        gRaHdcAsync[phyId].snapshot_session = NULL;
    } else {
        hccp_err("duplicate or incorrect order calls are not allowed, phyId[%u] action[%d]", phyId, action);
        ret = -EPERM;
    }
    RA_PTHREAD_MUTEX_UNLOCK(&gRaHdcAsync[phyId].recv_mutex);
    RA_PTHREAD_MUTEX_UNLOCK(&gRaHdcAsync[phyId].send_mutex);
#endif
    return ret;
}

int RaHdcAsyncRestoreSnapshot(unsigned int phyId)
{
    int ret = 0;

    if (gRaHdcAsync[phyId].thread_status == THREAD_HALT) {
        return 0;
    }

#ifndef HNS_ROCE_LLT
    RA_PTHREAD_MUTEX_LOCK(&gRaHdcAsync[phyId].send_mutex);
    RA_PTHREAD_MUTEX_LOCK(&gRaHdcAsync[phyId].recv_mutex);
    if (gRaHdcAsync[phyId].connect_status != HDC_UNCONNECTED) {
        hccp_err("incorrect order calls are not allowed, phyId[%u] connectStatus[%u]", phyId,
            gRaHdcAsync[phyId].connect_status);
        ret = -EPERM;
    } else {
        gRaHdcAsync[phyId].restore_flag = 1;
    }
    RA_PTHREAD_MUTEX_UNLOCK(&gRaHdcAsync[phyId].recv_mutex);
    RA_PTHREAD_MUTEX_UNLOCK(&gRaHdcAsync[phyId].send_mutex);
#endif
    return ret;
}

STATIC void __attribute__ ((destructor)) RaHdcUninitAsync(void)
{
    unsigned int phyId = 0;

    for (phyId = 0; phyId < RA_MAX_PHY_ID_NUM; phyId++) {
        if (gRaHdcAsync[phyId].session == NULL || gRaHdcAsync[phyId].thread_status != THREAD_RUNNING) {
            continue;
        }

        (void)RaHdcDeinitAsync(phyId);
    }
}

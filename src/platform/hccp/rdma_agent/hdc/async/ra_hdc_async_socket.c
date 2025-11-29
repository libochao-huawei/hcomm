/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "securec.h"
#include "user_log.h"
#include "ra.h"
#include "ra_comm.h"
#include "ra_async.h"
#include "ra_rs_comm.h"
#include "ra_rs_err.h"
#include "ra_hdc.h"
#include "ra_hdc_socket.h"
#include "ra_hdc_async.h"
#include "ra_hdc_async_socket.h"

int RaHdcSocketSendAsync(const struct socket_hdc_info *fdHandle, const void *data, unsigned long long size,
    unsigned long long *sentSize, void **reqHandle)
{
    unsigned long long sendSize = (size > SOCKET_SEND_MAXLEN) ? SOCKET_SEND_MAXLEN : size;
    struct ra_request_handle *reqHandleTmp = NULL;
    union op_socket_send_data *asyncData = NULL;
    unsigned int phyId = fdHandle->phy_id;
    int ret = 0;

    asyncData = (union op_socket_send_data *)calloc(sizeof(union op_socket_send_data), sizeof(char));
    CHK_PRT_RETURN(asyncData == NULL, hccp_err("[send][ra_hdc_socket]calloc async_data failed, phyId(%u)", phyId),
        -ENOMEM);

    asyncData->tx_data.fd = (unsigned int)fdHandle->fd;
    asyncData->tx_data.send_size = sendSize;
    ret = memcpy_s(asyncData->tx_data.data_send, SOCKET_SEND_MAXLEN, data, sendSize);
    if (ret != 0) {
        hccp_err("[send][ra_hdc_socket]memcpy_s data failed, ret(%d) sendSize(%llu) phyId(%u)",
            ret, sendSize, phyId);
        ret = -ESAFEFUNC;
        goto out;
    }

    reqHandleTmp = (struct ra_request_handle *)calloc(1, sizeof(struct ra_request_handle));
    if (reqHandleTmp == NULL) {
        hccp_err("[send][ra_hdc_socket]calloc req_handle_tmp failed, phyId[%u]", phyId);
        ret = -ENOMEM;
        goto out;
    }
    *sentSize = 0;
    reqHandleTmp->priv_data = (void *)sentSize;

    ret = RaHdcSendMsgAsync(RA_RS_SOCKET_SEND, phyId, (char *)asyncData, sizeof(union op_socket_send_data),
        reqHandleTmp);
    if (ret != 0) {
        hccp_err("[send][ra_hdc_socket]hdc async send message process failed ret(%d) phy_id(%u)", ret, phyId);
        free(reqHandleTmp);
        reqHandleTmp = NULL;
        goto out;
    }

    *reqHandle = (void *)reqHandleTmp;

out:
    free(asyncData);
    asyncData = NULL;
    return ret;
}

void RaHdcAsyncHandleSocketSend(struct ra_request_handle *reqHandle)
{
    union op_socket_send_data *asyncData = NULL;

    if (reqHandle->op_ret > 0) {
        asyncData = (union op_socket_send_data *)reqHandle->recv_buf;
        *(unsigned long long *)reqHandle->priv_data = asyncData->rx_data.real_send_size;
        reqHandle->op_ret = 0;
    } else if (reqHandle->op_ret == 0) {
        hccp_warn("[send][ra_hdc_socket]socket has been closed. sent_size is 0");
        *(unsigned long long *)reqHandle->priv_data = 0;
        reqHandle->op_ret = -ESOCKCLOSED;
    } else {
        if (reqHandle->op_ret != -EAGAIN) {
            hccp_warn("[send][ra_hdc_socket]socket send unsuccessful ret(%d) phy_id(%u)", reqHandle->op_ret,
                reqHandle->phy_id);
        }
        *(unsigned long long *)reqHandle->priv_data = 0;
    }

    return;
}

STATIC void RaHdcSocketPrepareRecvRsp(struct ra_response_socket_recv *recvRsp, void *data,
    unsigned long long size, unsigned long long *receivedSize)
{
    recvRsp->data = data;
    recvRsp->size = size;
    *receivedSize = 0;
    recvRsp->received_size = receivedSize;
}

int RaHdcSocketRecvAsync(const struct socket_hdc_info *fdHandle, void *data, unsigned long long size,
    unsigned long long *receivedSize, void **reqHandle)
{
    unsigned long long recvSize = (size > SOCKET_SEND_MAXLEN) ? SOCKET_SEND_MAXLEN : size;
    struct ra_response_socket_recv *recvRsp = NULL;
    struct ra_request_handle *reqHandleTmp = NULL;
    union op_socket_recv_data *asyncData = NULL;
    unsigned int phyId = fdHandle->phy_id;
    int ret = 0;

    recvRsp = (struct ra_response_socket_recv *)calloc(1, sizeof(struct ra_response_socket_recv));
    CHK_PRT_RETURN(recvRsp == NULL, hccp_err("[recv][ra_hdc_socket]calloc recv_rsp failed, phyId(%u)", phyId),
        -ENOMEM);
    RaHdcSocketPrepareRecvRsp(recvRsp, data, recvSize, receivedSize);

    asyncData = (union op_socket_recv_data *)calloc(sizeof(union op_socket_recv_data) + recvSize, sizeof(char));
    if (asyncData == NULL) {
        hccp_err("[recv][ra_hdc_socket]calloc async_data failed, phyId(%u)", phyId);
        ret = -ENOMEM;
        goto free_recv_rsp;
    }

    asyncData->tx_data.fd = (unsigned int)fdHandle->fd;
    asyncData->tx_data.recv_size = recvSize;
    reqHandleTmp = (struct ra_request_handle *)calloc(1, sizeof(struct ra_request_handle));
    if (reqHandleTmp == NULL) {
        hccp_err("[recv][ra_hdc_socket]calloc req_handle_tmp failed, phyId[%u]", phyId);
        ret = -ENOMEM;
        goto out;
    }
    reqHandleTmp->priv_data = (void *)recvRsp;
    ret = RaHdcSendMsgAsync(RA_RS_SOCKET_RECV, phyId, (char *)asyncData,
        (unsigned int)(sizeof(union op_socket_recv_data) + recvSize), reqHandleTmp);
    if (ret != 0) {
        hccp_err("[recv][ra_hdc_socket]hdc async send message process failed ret(%d) phy_id(%u)", ret, phyId);
        free(reqHandleTmp);
        reqHandleTmp = NULL;
        goto out;
    }

    free(asyncData);
    asyncData = NULL;
    *reqHandle = (void *)reqHandleTmp;
    return 0;

out:
    free(asyncData);
    asyncData = NULL;
free_recv_rsp:
    free(recvRsp);
    recvRsp = NULL;
    return ret;
}

void RaHdcAsyncHandleSocketRecv(struct ra_request_handle *reqHandle)
{
    struct ra_response_socket_recv *recvRsp = NULL;
    union op_socket_recv_data *asyncData = NULL;
    unsigned long long realRecvSize = 0;
    unsigned int phyId = 0;
    int ret = 0;

    phyId = reqHandle->phy_id;
    if (reqHandle->op_ret == 0) {
        hccp_warn("[recv][ra_hdc_socket]socket has been closed. received_size is 0");
        reqHandle->op_ret = -ESOCKCLOSED;
        goto out;
    } else if (reqHandle->op_ret < 0) {
        if (reqHandle->op_ret != -EAGAIN) {
            hccp_warn("[recv][ra_hdc_socket]socket recv ret(%d) phy_id(%u)", reqHandle->op_ret, phyId);
        }
        goto out;
    }

    asyncData = (union op_socket_recv_data *)reqHandle->recv_buf;
    realRecvSize = asyncData->rx_data.real_recv_size;
    if (realRecvSize > SOCKET_SEND_MAXLEN) {
        hccp_err("[recv][ra_hdc_socket]real_recv_size:%llu invalid, phyId(%u)", realRecvSize, phyId);
        reqHandle->op_ret = -EINVAL;
        goto out;
    }

    recvRsp = (struct ra_response_socket_recv *)reqHandle->priv_data;
    ret = memcpy_s(recvRsp->data, recvRsp->size, (char *)asyncData + sizeof(union op_socket_recv_data),
        realRecvSize);
    if (ret != 0) {
        hccp_err("[recv][ra_hdc_socket]memcpy_s failed, ret(%d) phyId(%u) size(%llu) realRecvSize(%llu)",
            ret, phyId, recvRsp->size, realRecvSize);
        reqHandle->op_ret = -ESAFEFUNC;
        goto out;
    }

    reqHandle->op_ret = 0;
    *recvRsp->received_size = realRecvSize;

out:
    free(reqHandle->priv_data);
    reqHandle->priv_data = NULL;
    return;
}

int RaHdcSocketListenStartAsync(unsigned int phyId, struct socket_listen_info_t conn[], unsigned int num,
    void **reqHandle)
{
    struct ra_response_socket_listen *asyncRsp = NULL;
    struct ra_request_handle *reqHandleTmp = NULL;
    union op_socket_listen_data asyncData = {0};
    int ret = 0;

    ret = RaGetSocketListenInfo(conn, num, asyncData.tx_data.conn, MAX_SOCKET_NUM);
    CHK_PRT_RETURN(ret != 0, hccp_err("[listen_start][ra_hdc_socket]get_socket_listen_info failed, ret(%d) phyId(%u)",
        ret, phyId), -EINVAL);
    asyncData.tx_data.num = num | (1U << SOCKET_USE_PORT_BIT);

    asyncRsp = (struct ra_response_socket_listen *)calloc(1, sizeof(struct ra_response_socket_listen));
    CHK_PRT_RETURN(asyncRsp == NULL, hccp_err("[listen_start][ra_hdc_socket]calloc async_rsp failed, phyId(%u)",
        phyId), -ENOMEM);
    asyncRsp->conn = conn;
    asyncRsp->num = num;
    reqHandleTmp = (struct ra_request_handle *)calloc(1, sizeof(struct ra_request_handle));
    if (reqHandleTmp == NULL) {
        hccp_err("[listen_start][ra_hdc_socket]calloc ra_request_handle failed, phyId[%u]", phyId);
        ret = -ENOMEM;
        goto out;
    }
    reqHandleTmp->priv_data = (void *)asyncRsp;

    ret = RaHdcSendMsgAsync(RA_RS_SOCKET_LISTEN_START, phyId, (char *)&asyncData,
        sizeof(union op_socket_listen_data), reqHandleTmp);
    if (ret != 0) {
        hccp_err("[listen_start][ra_hdc_socket]hdc async send message process failed ret(%d) phy_id(%u)", ret, phyId);
        free(reqHandleTmp);
        reqHandleTmp = NULL;
        goto out;
    }
    *reqHandle = (void *)reqHandleTmp;
    return 0;

out:
    free(asyncRsp);
    asyncRsp = NULL;
    return ret;
}

void RaHdcAsyncHandleSocketListenStart(struct ra_request_handle *reqHandle)
{
    struct ra_response_socket_listen *asyncRsp = NULL;
    union op_socket_listen_data *asyncData = NULL;
    unsigned int phyId = reqHandle->phy_id;
    int ret = 0;

    asyncData = (union op_socket_listen_data *)reqHandle->recv_buf;
    asyncRsp = (struct ra_response_socket_listen *)reqHandle->priv_data;
    ret = RaGetSocketListenResult(asyncData->rx_data.conn, asyncRsp->num, asyncRsp->conn, MAX_SOCKET_NUM);
    if (ret != 0) {
        hccp_err("[listen_start][ra_hdc_socket]ra_get_socket_listen_result failed, ret(%d) phyId(%u)", ret, phyId);
        reqHandle->op_ret = -EINVAL;
        goto out;
    }
    return;

out:
    free(reqHandle->priv_data);
    reqHandle->priv_data = NULL;
    return;
}

int RaHdcSocketListenStopAsync(unsigned int phyId, struct socket_listen_info_t conn[], unsigned int num,
    void **reqHandle)
{
    struct ra_request_handle *reqHandleTmp = NULL;
    union op_socket_listen_data asyncData = {0};
    int ret = 0;

    ret = RaGetSocketListenInfo(conn, num, asyncData.tx_data.conn, MAX_SOCKET_NUM);
    CHK_PRT_RETURN(ret != 0, hccp_err("[listen_stop][ra_hdc_socket]get_socket_listen_info failed, ret(%d) phyId(%u)",
        ret, phyId), -EINVAL);
    asyncData.tx_data.num = num | (1U << SOCKET_USE_PORT_BIT);

    reqHandleTmp = (struct ra_request_handle *)calloc(1, sizeof(struct ra_request_handle));
    CHK_PRT_RETURN(reqHandleTmp == NULL,
        hccp_err("[listen_stop][ra_hdc_socket]calloc ra_request_handle failed, phyId[%u]", phyId), -ENOMEM);

    ret = RaHdcSendMsgAsync(RA_RS_SOCKET_LISTEN_STOP, phyId, (char *)&asyncData,
        sizeof(union op_socket_listen_data), reqHandleTmp);
    if (ret != 0) {
        hccp_err("[listen_stop][ra_hdc_socket]hdc async send message process failed ret(%d) phy_id(%u)", ret, phyId);
        free(reqHandleTmp);
        reqHandleTmp = NULL;
        return ret;
    }

    *reqHandle = (void *)reqHandleTmp;
    return 0;
}

int RaHdcSocketBatchConnectAsync(unsigned int phyId, struct socket_connect_info_t conn[], unsigned int num,
    void **reqHandle)
{
    struct ra_request_handle *reqHandleTmp = NULL;
    union op_socket_connect_data *asyncData = NULL;
    int ret = 0;

    asyncData = (union op_socket_connect_data *)calloc(sizeof(union op_socket_connect_data), sizeof(char));
    CHK_PRT_RETURN(asyncData == NULL, hccp_err("[batch_connect][ra_hdc_socket]calloc async_data failed, phyId(%u)",
        phyId), -ENOMEM);

    asyncData->tx_data.num = num | (1U << SOCKET_USE_PORT_BIT);
    ret = RaGetSocketConnectInfo(conn, num, asyncData->tx_data.conn, MAX_SOCKET_NUM);
    if (ret != 0) {
        hccp_err("[batch_connect][ra_hdc_socket]ra_get_socket_connect_info failed, ret(%d) phyId(%u)", ret, phyId);
        goto out;
    }

    reqHandleTmp = (struct ra_request_handle *)calloc(1, sizeof(struct ra_request_handle));
    if (reqHandleTmp == NULL) {
        hccp_err("[batch_connect][ra_hdc_socket]calloc ra_request_handle failed, phyId[%u]", phyId);
        ret = -ENOMEM;
        goto out;
    }

    ret = RaHdcSendMsgAsync(RA_RS_SOCKET_CONN, phyId, (char *)asyncData, sizeof(union op_socket_connect_data),
        reqHandleTmp);
    if (ret != 0) {
        hccp_err("[batch_connect][ra_hdc_socket]hdc async send message process failed ret(%d) phy_id(%u)", ret, phyId);
        free(reqHandleTmp);
        reqHandleTmp = NULL;
        goto out;
    }

    *reqHandle = (void *)reqHandleTmp;

out:
    free(asyncData);
    asyncData = NULL;
    return ret;
}

int RaHdcSocketBatchCloseAsync(unsigned int phyId, struct socket_close_info_t conn[], unsigned int num,
    void **reqHandle)
{
    struct ra_response_socket_batch_close *asyncRsp = NULL;
    struct ra_request_handle *reqHandleTmp = NULL;
    union op_socket_close_data asyncData = {0};
    unsigned int i;
    int ret = 0;

    for (i = 0; i < num; i++) {
        if (conn[i].fd_handle == NULL) {
            hccp_err("[batch_close][ra_hdc_socket]i(%u), conn fdHandle is NULL", i);
            ret = -EINVAL;
            goto out;
        }
        asyncData.tx_data.conn[i].phy_id = phyId;
        asyncData.tx_data.conn[i].close_fd = ((struct socket_hdc_info *)conn[i].fd_handle)->fd;
    }
    // use attr disuse_linger of the fist conn as the common attr for all(0 by default)
    asyncData.tx_data.num = (conn[0].disuse_linger != 0) ? (num | (1U << SOCKET_DISUSE_LINGER_BIT)) : num;

    asyncRsp = (struct ra_response_socket_batch_close *)calloc(1, sizeof(struct ra_response_socket_batch_close));
    CHK_PRT_RETURN(asyncRsp == NULL, hccp_err("[batch_close][ra_hdc_socket]calloc async_rsp failed, phyId(%u)",
        phyId), -ENOMEM);
    asyncRsp->conn = conn;
    asyncRsp->num = num;

    reqHandleTmp = (struct ra_request_handle *)calloc(1, sizeof(struct ra_request_handle));
    if (reqHandleTmp == NULL) {
        hccp_err("[batch_close][ra_hdc_socket]calloc ra_request_handle failed, phyId[%u]", phyId);
        ret = -ENOMEM;
        goto out;
    }

    reqHandleTmp->priv_data = (void *)asyncRsp;

    ret = RaHdcSendMsgAsync(RA_RS_SOCKET_CLOSE, phyId, (char *)&asyncData,
        sizeof(union op_socket_close_data), reqHandleTmp);
    if (ret != 0) {
        hccp_err("[batch_close][ra_hdc_socket]hdc async send message process failed, ret(%d) phyId(%u)",
            ret, phyId);
        free(reqHandleTmp);
        reqHandleTmp = NULL;
        goto out;
    }

    *reqHandle = (void *)reqHandleTmp;
    return 0;

out:
    free(asyncRsp);
    asyncRsp = NULL;
    return ret;
}

void RaHdcAsyncHandleSocketBatchClose(struct ra_request_handle *reqHandle)
{
    struct ra_response_socket_batch_close *asyncRsp = NULL;
    unsigned int i;

    // should free fd_handle when op_ret is not EAGAIN, otherwise caller will retry
    if (reqHandle->op_ret == -EAGAIN) {
        return;
    }

    asyncRsp = (struct ra_response_socket_batch_close *)reqHandle->priv_data;
    for (i = 0; i < asyncRsp->num; i++) {
        if (asyncRsp->conn[i].fd_handle != NULL) {
            free(asyncRsp->conn[i].fd_handle);
            asyncRsp->conn[i].fd_handle = NULL;
        }
    }
    return;
}
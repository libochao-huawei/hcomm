/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "user_log.h"
#include "ra_hdc.h"
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include "securec.h"
#include "dl_hal_function.h"
#include "ra.h"
#include "ra_async.h"
#include "ra_comm.h"
#include "ra_rdma_lite.h"
#include "ra_hdc_lite.h"
#include "ra_rs_err.h"
#include "ra_hdc_async.h"

struct hdc_info g_ra_hdc[RA_MAX_PHY_ID_NUM] = {0};

struct hdc_ops g_ra_hdc_ops_host = {
    .get_capacity = dl_drv_hdc_get_capacity,
    .client_create = dl_drv_hdc_client_create,
    .client_destroy = dl_drv_hdc_client_destroy,
    .session_connect = dl_drv_hdc_session_connect,
    .session_connect_ex = dl_hal_hdc_session_connect_ex,
    .server_create = dl_drv_hdc_server_create,
    .server_destroy = dl_drv_hdc_server_destroy,
    .session_accept = dl_drv_hdc_session_accept,
    .session_close = dl_drv_hdc_session_close,
    .free_msg = dl_drv_hdc_free_msg,
    .reuse_msg = dl_drv_hdc_reuse_msg,
    .add_msg_buffer = dl_drv_hdc_add_msg_buffer,
    .get_msg_buffer = dl_drv_hdc_get_msg_buffer,
    .recv = dl_hal_hdc_recv,
    .send = dl_hal_hdc_send,
    .alloc_msg = dl_drv_hdc_alloc_msg,
    .set_session_reference = dl_drv_hdc_set_session_reference,
};

#define RA_HDC_OPS g_ra_hdc_ops_host

struct opcode_interface_info g_ra_interface_info_list[] = {
    // outer opcode version: 1.0
    {RA_RS_SOCKET_CONN, 0},
    {RA_RS_SOCKET_CLOSE, 0},
    {RA_RS_SOCKET_ABORT, 0},
    {RA_RS_SOCKET_LISTEN_START, 0},
    {RA_RS_SOCKET_LISTEN_STOP, 0},
    {RA_RS_GET_SOCKET, 0},
    {RA_RS_SOCKET_SEND, 0},
    {RA_RS_SOCKET_RECV, 0},
    {RA_RS_QP_CREATE, 0},
    {RA_RS_QP_CREATE_WITH_ATTRS, 0},
    {RA_RS_AI_QP_CREATE, 0},
    {RA_RS_AI_QP_CREATE_WITH_ATTRS, 0},
    {RA_RS_TYPICAL_QP_CREATE, 0},
    {RA_RS_QP_DESTROY, 0},
    {RA_RS_QP_CONNECT, 0},
    {RA_RS_TYPICAL_QP_MODIFY, 0},
    {RA_RS_QP_STATUS, 0},
    {RA_RS_QP_INFO, 0},
    {RA_RS_QP_BATCH_MODIFY, 0},
    {RA_RS_MR_REG, 0},
    {RA_RS_MR_DEREG, 0},
    {RA_RS_TYPICAL_MR_REG, 0},
    {RA_RS_REMAP_MR, 0},
    {RA_RS_TYPICAL_MR_DEREG, 0},
    {RA_RS_SEND_WR, 0},
    {RA_RS_GET_NOTIFY_BA, 0},
    {RA_RS_INIT, 0},
    {RA_RS_DEINIT, 0},
    {RA_RS_SOCKET_INIT, 0},
    {RA_RS_SOCKET_DEINIT, 0},
    {RA_RS_RDEV_INIT, 0},
    {RA_RS_RDEV_INIT_WITH_BACKUP, 0},
    {RA_RS_RDEV_GET_PORT_STATUS, 0},
    {RA_RS_RDEV_DEINIT, 0},
    {RA_RS_WLIST_ADD, 0},
    {RA_RS_WLIST_ADD_V2, 0},
    {RA_RS_WLIST_DEL, 0},
    {RA_RS_WLIST_DEL_V2, 0},
    {RA_RS_ACCEPT_CREDIT_ADD, 0},
    {RA_RS_GET_IFADDRS, 0},
    {RA_RS_GET_IFADDRS_V2, 0},
    {RA_RS_GET_INTERFACE_VERSION, 0},
    {RA_RS_SEND_WRLIST, 0},
    {RA_RS_SEND_WRLIST_V2, 0},
    {RA_RS_SEND_WRLIST_EXT, 0},
    {RA_RS_SEND_WRLIST_EXT_V2, 0},
    {RA_RS_SEND_NORMAL_WRLIST, 0},
    {RA_RS_SET_TSQP_DEPTH, 0},
    {RA_RS_GET_TSQP_DEPTH, 0},
    {RA_RS_SET_QP_ATTR_QOS, 0},
    {RA_RS_SET_QP_ATTR_TIMEOUT, 0},
    {RA_RS_SET_QP_ATTR_RETRY_CNT, 0},
    {RA_RS_GET_CQE_ERR_INFO, 0},
    {RA_RS_GET_CQE_ERR_INFO_NUM, 0},
    {RA_RS_GET_CQE_ERR_INFO_LIST, 0},
    {RA_RS_GET_LITE_SUPPORT, 0},
    {RA_RS_GET_LITE_RDEV_CAP, 0},
    {RA_RS_GET_LITE_QP_CQ_ATTR, 0},
    {RA_RS_GET_LITE_CONNECTED_INFO, 0},
    {RA_RS_GET_LITE_MEM_ATTR, 0},
    {RA_RS_PING_INIT, 0},
    {RA_RS_PING_ADD, 0},
    {RA_RS_PING_START, 0},
    {RA_RS_PING_GET_RESULTS, 0},
    {RA_RS_PING_STOP, 0},
    {RA_RS_PING_DEL, 0},
    {RA_RS_PING_DEINIT, 0},
    {RA_RS_GET_VNIC_IP_INFOS_V1, 0},
    {RA_RS_GET_VNIC_IP_INFOS, 0},
    {RA_RS_TLV_INIT, 0},
    {RA_RS_TLV_DEINIT, 0},
    {RA_RS_TLV_REQUEST, 0},
    {RA_RS_GET_TLS_ENABLE, 0},
    {RA_RS_GET_SEC_RANDOM, 0},
    {RA_RS_GET_ROCE_API_VERSION, 0},

    // outer opcode version: 2.0
    {RA_RS_CTX_INIT, 0},
    {RA_RS_CTX_DEINIT, 0},
    {RA_RS_GET_TP_INFO_LIST, 0},
    {RA_RS_CTX_TOKEN_ID_ALLOC, 0},
    {RA_RS_CTX_TOKEN_ID_FREE, 0},
    {RA_RS_LMEM_REG, 0},
    {RA_RS_LMEM_UNREG, 0},
    {RA_RS_RMEM_IMPORT, 0},
    {RA_RS_RMEM_UNIMPORT, 0},
    {RA_RS_GET_DEV_EID_INFO_NUM, 0},
    {RA_RS_GET_DEV_EID_INFO_LIST, 0},
    {RA_RS_CTX_CHAN_CREATE, 0},
    {RA_RS_CTX_CHAN_DESTROY, 0},
    {RA_RS_CTX_CQ_CREATE, 0},
    {RA_RS_CTX_CQ_DESTROY, 0},
    {RA_RS_CTX_QP_CREATE, 0},
    {RA_RS_CTX_QP_DESTROY, 0},
    {RA_RS_CTX_QP_IMPORT, 0},
    {RA_RS_CTX_QP_UNIMPORT, 0},
    {RA_RS_CTX_QP_BIND, 0},
    {RA_RS_CTX_QP_UNBIND, 0},
    {RA_RS_CTX_BATCH_SEND_WR, 0},
    {RA_RS_CUSTOM_CHANNEL, 0},
    {RA_RS_CTX_UPDATE_CI, 0},

    // inner opcode version
    {RA_RS_HDC_SESSION_CLOSE, 0},
    {RA_RS_GET_VNIC_IP, 0},
    {RA_RS_NOTIFY_CFG_SET, 0},
    {RA_RS_NOTIFY_CFG_GET, 0},
    {RA_RS_SET_PID, 0},
    {RA_RS_ASYNC_HDC_SESSION_CONNECT, 0},
    {RA_RS_ASYNC_HDC_SESSION_CLOSE, 0},
};

STATIC int msg_head_check(struct msg_head *send_rcv_head, unsigned int opcode, int rs_ret, unsigned int msg_data_len);

static int hdc_send_recv_pkt_send(struct drvHdcMsg *p_msg_snd, char *send_rcv_buf, unsigned int in_buf_len,
    HDC_SESSION session, struct drvHdcMsg **p_msg_rcv)
{
    int ret;
    ret = RA_HDC_OPS.add_msg_buffer(p_msg_snd, send_rcv_buf, in_buf_len);
    CHK_PRT_RETURN(ret != 0, hccp_err("[send][hdc_send_recv_pkt]HDC add msg buffer err ret(%d)", ret), ret);

    ret = RA_HDC_OPS.send(session, p_msg_snd, RA_HDC_WAIT_TIMEOUT, RA_HDC_RECV_SEND_TIMEOUT);
    CHK_PRT_RETURN(ret != 0, hccp_err("[send][hdc_send_recv_pkt]HDC send err ret(%d)", ret), ret);

    ret = RA_HDC_OPS.reuse_msg(p_msg_snd);
    CHK_PRT_RETURN(ret != 0, hccp_err("[send][hdc_send_recv_pkt]HDC reuser msg err ret(%d)", ret), ret);

    *p_msg_rcv = p_msg_snd;
    return 0;
}

#ifndef HNS_ROCE_LLT
STATIC int hdc_send_retry_pkt(
    unsigned int phy_id, void *send_rcv_buf, unsigned int in_buf_len, struct drvHdcMsg **p_msg_rcv)
{
    int ret;
    struct drvHdcMsg *p_msg_snd = NULL;

    HDC_SESSION session = g_ra_hdc[phy_id].session;
    if (session == NULL) {
        hccp_err("[send_recv][pkt]session is NULL!, phy_id(%u)", phy_id);
        return -EINVAL;
    }

    ret = RA_HDC_OPS.alloc_msg(session, &p_msg_snd, 1);
    if (ret != 0) {
        hccp_err("[send_recv][pkt]HDC alloc msg err ret(%d) phy_id(%u)", ret, phy_id);
        return ret;
    }
    ret = hdc_send_recv_pkt_send(p_msg_snd, (char *)send_rcv_buf, in_buf_len, session, p_msg_rcv);
    if (ret) {
        hccp_err("[send_recv][pkt]HDC pkt send err ret(%d) phy_id(%u)", ret, phy_id);
        goto msg_err;
    }

    return 0;

msg_err:
    RA_HDC_OPS.free_msg(p_msg_snd);
    return ret;
}

STATIC int ra_hdc_send_retry_msg(unsigned int phy_id, struct drvHdcMsg **p_msg_rcv)
{
    int ret;
    void *send_rcv_buf = NULL;
    unsigned int send_rcv_len;
    union op_ifnum_data ifnum_data;
    ifnum_data.tx_data.phy_id = phy_id;

    pid_t host_tgid = dl_drv_device_get_bare_tgid();
    unsigned int data_size = sizeof(union op_ifnum_data);
    send_rcv_len = sizeof(struct msg_head) + data_size;
    send_rcv_buf = (void *)calloc(send_rcv_len, sizeof(char));
    CHK_PRT_RETURN(send_rcv_buf == NULL, hccp_err("[process][ra_hdc_msg]send_rcv_buf calloc failed. phy_id(%u)",
        phy_id), -ENOMEM);
    msg_head_build_up(send_rcv_buf, RA_RS_GET_IFNUM, 0, data_size, host_tgid);

    ret = memcpy_s(
        send_rcv_buf + sizeof(struct msg_head), send_rcv_len - sizeof(struct msg_head), &ifnum_data, data_size);
    if (ret) {
        hccp_err("[process][ra_hdc_msg]memcpy_s failed, ret(%d) phy_id(%u)", ret, phy_id);
        ret = -ESAFEFUNC;
        goto out;
    }

    ret = hdc_send_retry_pkt(phy_id, send_rcv_buf, send_rcv_len, p_msg_rcv);
    if (ret) {
        hccp_err("[process][ra_hdc_msg]hdc_send_recv_pkt failed ret(%d) phy_id(%u)", ret, phy_id);
        goto out;
    }

out:
    free(send_rcv_buf);
    send_rcv_buf = NULL;
    return ret;
}

static int ra_hdc_recv_retry_msg(HDC_SESSION session, struct drvHdcMsg *p_msg_rcv)
{
    int out_buf_len = sizeof(struct msg_head) + sizeof(union op_ifnum_data);
    char *recv_buf = NULL;
    int recv_buf_cnt = 0;
    int rcv_buf_len = 0;
    int ret;

    ret = RA_HDC_OPS.recv(
        session, p_msg_rcv, MAX_HDC_DATA, RA_HDC_WAIT_TIMEOUT, &recv_buf_cnt, RA_HDC_RETRY_SEND_TIMEOUT);
    if (ret) {
        hccp_err("[recv][ra_hdc_recv_retry_msg]HDC get retry recv msg fail(%d)", ret);
        return ret;
    }

    ret = RA_HDC_OPS.get_msg_buffer(p_msg_rcv, 0, &recv_buf, &rcv_buf_len);
    if (ret) {
        hccp_err("[recv][ra_hdc_recv_retry_msg]HDC get retry msg buffer fail(%d)", ret);
        return ret;
    }

    ret = msg_head_check((struct msg_head *)recv_buf, RA_RS_GET_IFNUM, 0, sizeof(union op_ifnum_data));
    if (rcv_buf_len != out_buf_len || ret != 0) {
        hccp_err("[recv][ra_hdc_recv_retry_msg]HDC get retry recv msg fail, ret(%d), rcv_buf_len:%d, out_buf_len:%d",
            ret, rcv_buf_len, out_buf_len);
        return ret;
    }

    return 0;
}
#endif

static int hdc_send_recv_pkt_recv(HDC_SESSION session, unsigned int phy_id, struct drvHdcMsg *p_msg_rcv,
    char **recv_buf, int *rcv_buf_len)
{
    struct drvHdcMsg *p_retry_rcv = NULL;
    int recv_buf_cnt = 0;
    int ret;

    ret =
        RA_HDC_OPS.recv(session, p_msg_rcv, MAX_HDC_DATA, RA_HDC_WAIT_TIMEOUT, &recv_buf_cnt, RA_HDC_RECV_SEND_TIMEOUT);
#ifndef HNS_ROCE_LLT
    /* if timeout, start retry */
    if (g_ra_hdc[phy_id].start_deinit == 0 && ret == DRV_ERROR_WAIT_TIMEOUT) {
        hccp_run_info("[recv][hdc_send_recv_pkt_recv]HDC recv timeout, start retry");
        ret = ra_hdc_send_retry_msg(phy_id, &p_retry_rcv);
        CHK_PRT_RETURN(
            ret != 0, hccp_err("[recv][hdc_send_recv_pkt_recv]HDC get msg by first retry fail(%d)", ret), ret);

        ret = RA_HDC_OPS.recv(
            session, p_msg_rcv, MAX_HDC_DATA, RA_HDC_WAIT_TIMEOUT, &recv_buf_cnt, RA_HDC_RECV_SEND_TIMEOUT);
        if (ret) {
            hccp_err("[recv][hdc_send_recv_pkt_recv]HDC get msg by first retry fail(%d)", ret);
            RA_HDC_OPS.free_msg(p_retry_rcv);
            return ret;
        }

        ret = RA_HDC_OPS.get_msg_buffer(p_msg_rcv, 0, (char **)recv_buf, rcv_buf_len);
        if (ret) {
            hccp_err("[recv][hdc_send_recv_pkt]HDC get_msg_buffer msg err ret(%d), rcv_buf_len(%d)", ret, *rcv_buf_len);
            RA_HDC_OPS.free_msg(p_retry_rcv);
            return ret;
        }

        ret = ra_hdc_recv_retry_msg(session, p_retry_rcv);
        if (ret) {
            hccp_err("[recv][hdc_send_recv_pkt_recv]HDC recv first retry msg fail(%d)", ret);
        }

        RA_HDC_OPS.free_msg(p_retry_rcv);
        return ret;
    }
#endif
    CHK_PRT_RETURN(ret != 0, hccp_err("[recv][hdc_send_recv_pkt]HDC recv msg err ret(%d)", ret), ret);

    ret = RA_HDC_OPS.get_msg_buffer(p_msg_rcv, 0, (char **)recv_buf, rcv_buf_len);
    CHK_PRT_RETURN(ret != 0, hccp_err("[recv][hdc_send_recv_pkt]HDC get_msg_buffer msg err ret(%d), rcv_buf_len(%d)",
        ret, *rcv_buf_len), ret);

    return 0;
}

STATIC int hdc_send_recv_pkt_recv_check(int rcv_buf_len, unsigned int out_data_len, struct msg_head *recv_msg_head,
    struct drvHdcMsg *p_msg_rcv)
{
    unsigned int rcv_buf_len_tmp;

    rcv_buf_len_tmp = (unsigned int)rcv_buf_len;
    if (out_data_len != rcv_buf_len_tmp) {
        if (recv_msg_head->ret == -EACCES) {
            hccp_warn("exceed the speed limit, need try again, ret:%d", recv_msg_head->ret);
            RA_HDC_OPS.free_msg(p_msg_rcv);
            return -EAGAIN;
        } else if (recv_msg_head->ret == -EPROTONOSUPPORT) {
            hccp_err("[check][hdc_send_recv_pkt_recv]unsupported opcode, ret(%d)", recv_msg_head->ret);
            RA_HDC_OPS.free_msg(p_msg_rcv);
            return -EPROTONOSUPPORT;
        } else if (recv_msg_head->ret == -EPERM) {
            hccp_err("[check][hdc_send_recv_pkt_recv]host pid is invalid, ret(%d)", recv_msg_head->ret);
            RA_HDC_OPS.free_msg(p_msg_rcv);
            return -EPERM;
        }
        hccp_err("[check][hdc_send_recv_pkt_recv]date len err out_data_len(%d) != rcv_buf_len(%d) ",
                 out_data_len, rcv_buf_len);
        RA_HDC_OPS.free_msg(p_msg_rcv);
        return -EPIPE;
    }
    return 0;
}

STATIC int hdc_send_recv_pkt(unsigned int phy_id, void *send_rcv_buf, unsigned int in_buf_len, unsigned int out_buf_len)
{
    int ret;
    char *recv_buf = NULL;
    struct drvHdcMsg *p_msg_rcv = NULL, *p_msg_snd = NULL;
    int rcv_buf_len = 0;

    struct msg_head *recv_msg_head = NULL;

    RA_PTHREAD_MUTEX_LOCK(&g_ra_hdc[phy_id].lock);
    HDC_SESSION session = g_ra_hdc[phy_id].session;
    if (session == NULL) {
        hccp_err("[send_recv][pkt]session is NULL!, phy_id(%u)", phy_id);
        RA_PTHREAD_MUTEX_UNLOCK(&g_ra_hdc[phy_id].lock);
        return -EINVAL;
    }

    // check last recv status
    if (g_ra_hdc[phy_id].last_recv_status == DRV_ERROR_WAIT_TIMEOUT) {
        ret = DRV_ERROR_WAIT_TIMEOUT;
        goto alloc_msg_err;
    }

    ret = RA_HDC_OPS.alloc_msg(session, &p_msg_snd, 1);
    if (ret != 0) {
        hccp_err("[send_recv][pkt]HDC alloc msg err ret(%d) phy_id(%u)", ret, phy_id);
        goto alloc_msg_err;
    }
    ret = hdc_send_recv_pkt_send(p_msg_snd, (char *)send_rcv_buf, in_buf_len, session, &p_msg_rcv);
    if (ret) {
        hccp_err("[send_recv][pkt]HDC pkt send err ret(%d) phy_id(%u)", ret, phy_id);
        goto msg_err;
    }
    ret = hdc_send_recv_pkt_recv(session, phy_id, p_msg_rcv, &recv_buf, &rcv_buf_len);
    if (ret == DRV_ERROR_WAIT_TIMEOUT) {
        hccp_err("[send_recv][pkt]HDC broken, pkt recv err ret(%d) phy_id(%u)", ret, phy_id);
        g_ra_hdc[phy_id].last_recv_status = DRV_ERROR_WAIT_TIMEOUT;
        goto msg_err;
    }
    if (ret) {
        hccp_err("[send_recv][pkt]HDC pkt recv err ret(%d) phy_id(%u)", ret, phy_id);
        goto msg_err;
    }
    RA_PTHREAD_MUTEX_UNLOCK(&g_ra_hdc[phy_id].lock);
    recv_msg_head = (struct msg_head *)recv_buf;
    ret = hdc_send_recv_pkt_recv_check(rcv_buf_len, out_buf_len, recv_msg_head, p_msg_rcv);
    CHK_PRT_RETURN(ret, hccp_err("[send_recv][pkt]HDC pkt recv check ret(%d) phy_id(%u)", ret, phy_id), ret);

    ret = memcpy_s(send_rcv_buf, out_buf_len, recv_buf, rcv_buf_len);
    if (ret) {
        hccp_err("[send_recv][pkt]memcpy_s failed, ret(%d) phy_id(%u)", ret, phy_id);
        RA_HDC_OPS.free_msg(p_msg_rcv);
        return -ESAFEFUNC;
    }

    RA_HDC_OPS.free_msg(p_msg_rcv);
    return 0;
msg_err:
    RA_HDC_OPS.free_msg(p_msg_snd);
alloc_msg_err:
    RA_PTHREAD_MUTEX_UNLOCK(&g_ra_hdc[phy_id].lock);
    return ret;
}

void msg_head_build_up(struct msg_head *p_send_rcv_head, unsigned int opcode, unsigned int req_id,
    unsigned int msg_data_len, pid_t host_tgid)
{
    p_send_rcv_head->opcode = opcode;
    p_send_rcv_head->ret = 0;
    p_send_rcv_head->async_req_id = req_id;
    p_send_rcv_head->msg_data_len = msg_data_len;
    p_send_rcv_head->host_tgid = host_tgid;

    return;
}

STATIC int msg_head_check(struct msg_head *send_rcv_head, unsigned int opcode, int rs_ret, unsigned int msg_data_len)
{
    unsigned int ret;

    /* return rs real return value */
    if (send_rcv_head->ret < rs_ret) {
        return send_rcv_head->ret;
    }

    ret = (send_rcv_head->opcode != opcode) || (send_rcv_head->msg_data_len != msg_data_len);

    return (ret ? -EPERM : 0);
}

int ra_hdc_process_msg(unsigned int opcode, unsigned int phy_id, char *data, unsigned int data_size)
{
    pid_t host_tgid = dl_drv_device_get_bare_tgid();
    void *send_rcv_buf = NULL;
    unsigned int send_rcv_len;
    int ret;

    if (g_ra_hdc[phy_id].restore_flag != 0) {
        return 0;
    }

    send_rcv_len = sizeof(struct msg_head) + data_size;
    CHK_PRT_RETURN(data == NULL, hccp_err("[process][ra_hdc_msg]data is NULL. phy_id(%u)", phy_id), -EINVAL);
    send_rcv_buf = (void *)calloc(send_rcv_len, sizeof(char));
    CHK_PRT_RETURN(send_rcv_buf == NULL, hccp_err("[process][ra_hdc_msg]send_rcv_buf calloc failed. phy_id(%u)",
        phy_id), -ENOMEM);
    msg_head_build_up(send_rcv_buf, opcode, 0, data_size, host_tgid);
    ret = memcpy_s(send_rcv_buf + sizeof(struct msg_head), send_rcv_len - sizeof(struct msg_head), data, data_size);
    if (ret) {
        hccp_err("[process][ra_hdc_msg]memcpy_s failed, ret(%d) phy_id(%u)", ret, phy_id);
        ret = -ESAFEFUNC;
        goto out;
    }

    ret = hdc_send_recv_pkt(phy_id, send_rcv_buf, send_rcv_len, send_rcv_len);
    if (ret) {
        hccp_err("[process][ra_hdc_msg]hdc_send_recv_pkt opcode(%u) failed ret(%d) phy_id(%u)", opcode, ret, phy_id);
        goto out;
    }

    ret = msg_head_check(send_rcv_buf, opcode, 0, data_size);
    // opcode RA_RS_SOCKET_RECV not to print EAGAIN to avoid log flush
    if ((ret != 0) && (ret != -EAGAIN || opcode != RA_RS_SOCKET_RECV)) {
        /* maybe has retry return value, record warning log */
        hccp_warn("message head check unsuccessful, ret[%d] phy_id[%u]", ret, phy_id);
    }

    if (memcpy_s(data, data_size, send_rcv_buf + sizeof(struct msg_head), data_size)) {
        hccp_err("[process][ra_hdc_msg]memcpy_s fail. dest size(%d) ret(%d)  phy_id(%u)", data_size, ret, phy_id);
        ret = -ESAFEFUNC;
    }
out:
    free(send_rcv_buf);
    send_rcv_buf = NULL;
    return ret;
}

int hdc_async_send_pkt(struct hdc_async_info *async_info, unsigned int phy_id, void *send_buf, unsigned int send_len,
    struct ra_request_handle *req_handle)
{
    struct drvHdcMsg *p_msg_snd = NULL;
    HDC_SESSION session = NULL;
    int ret = 0;

    RA_PTHREAD_MUTEX_LOCK(&async_info->send_mutex);
    session = async_info->session;
    if (session == NULL) {
        RA_PTHREAD_MUTEX_UNLOCK(&async_info->send_mutex);
        hccp_err("[async][send_pkt]session is NULL!, phy_id(%u)", phy_id);
        return -EINVAL;
    }

    ret = RA_HDC_OPS.alloc_msg(session, &p_msg_snd, 1);
    if (ret != 0) {
        hccp_err("[async][send_pkt]HDC alloc msg err ret(%d) phy_id(%u)", ret, phy_id);
        goto alloc_msg_err;
    }

    ret = RA_HDC_OPS.add_msg_buffer(p_msg_snd, send_buf, (int)send_len);
    if (ret != 0) {
        hccp_err("[async][send_pkt]HDC add msg buffer err ret(%d) phy_id(%u)", ret, phy_id);
        goto msg_err;
    }

    ret = RA_HDC_OPS.send(session, p_msg_snd, RA_HDC_WAIT_TIMEOUT, RA_HDC_RECV_SEND_TIMEOUT);
    if (ret != 0) {
        hccp_err("[async][send_pkt]HDC send err ret(%d) phy_id(%u)", ret, phy_id);
        goto msg_err;
    }

    // make sure request has been added to req_list
    ra_list_add_tail(&req_handle->list, &async_info->req_list);
msg_err:
    RA_HDC_OPS.free_msg(p_msg_snd);
alloc_msg_err:
    RA_PTHREAD_MUTEX_UNLOCK(&async_info->send_mutex);
    return ret;
}

STATIC int hdc_async_prepare_recv_pkt(struct hdc_async_info *async_info, unsigned int phy_id, HDC_SESSION *session,
    struct drvHdcMsg **p_msg_rcv)
{
    int ret = 0;

    *session = async_info->session;
    if (*session == NULL) {
        hccp_err("[async][recv_pkt]session is NULL!, phy_id(%u)", phy_id);
        return -EINVAL;
    }

    // check last recv status
    if (ra_hdc_is_broken(async_info->last_recv_status)) {
        return async_info->last_recv_status;
    }

    ret = RA_HDC_OPS.alloc_msg(*session, p_msg_rcv, 1);
    if (ret != 0) {
        hccp_err("[async][recv_pkt]HDC alloc msg err ret(%d) phy_id(%u)", ret, phy_id);
        return ret;
    }

    return 0;
}

int hdc_async_recv_pkt(struct hdc_async_info *async_info, unsigned int phy_id, void *recv_buf, unsigned int *recv_len)
{
    struct drvHdcMsg *p_msg_rcv = NULL;
    HDC_SESSION session = NULL;
    int recv_buf_cnt = 0;
    char *rcv_buf = NULL;
    int rcv_len = 0;
    int ret = 0;

    ret = hdc_async_prepare_recv_pkt(async_info, phy_id, &session, &p_msg_rcv);
    if (ret != 0) {
        goto session_err;
    }

    ret = RA_HDC_OPS.recv(session, p_msg_rcv, MAX_HDC_DATA, RA_HDC_WAIT_TIMEOUT, &recv_buf_cnt,
        RA_HDC_RECV_SEND_TIMEOUT);
    // occur hdc time out when async session was closed before async request done
    if (ret == DRV_ERROR_WAIT_TIMEOUT) {
        hccp_run_warn("[async][recv_pkt]HDC recv timeout, phy_id(%u)", phy_id);
        goto msg_err;
    }

    if (ret != 0) {
        hccp_err("[async][recv_pkt]HDC recv err ret(%d) phy_id(%u)", ret, phy_id);
        async_info->last_recv_status = ret;
        goto msg_err;
    }

    ret = RA_HDC_OPS.get_msg_buffer(p_msg_rcv, 0, (char **)&rcv_buf, &rcv_len);
    if (ret != 0 || rcv_len <= 0) {
        hccp_err("[async][recv_pkt]HDC get_msg_buffer err ret(%d) phy_id(%u) rcv_len(%d)", ret, phy_id, rcv_len);
        goto msg_err;
    }

    ret = memcpy_s(recv_buf, *recv_len, rcv_buf, (unsigned int)rcv_len);
    if (ret != 0) {
        hccp_err("[async][recv_pkt]memcpy_s failed, ret(%d) phy_id(%u)", ret, phy_id);
        RA_HDC_OPS.free_msg(p_msg_rcv);
        return -ESAFEFUNC;
    }

    *recv_len = (unsigned int)rcv_len;
    RA_HDC_OPS.free_msg(p_msg_rcv);
    return 0;

msg_err:
    RA_HDC_OPS.free_msg(p_msg_rcv);
session_err:
    return ret;
}

int ra_hdc_get_interface_version(unsigned int phy_id, unsigned int interface_opcode, unsigned int *interface_version)
{
    int num = sizeof(g_ra_interface_info_list) / sizeof(g_ra_interface_info_list[0]);
    int i;

    CHK_PRT_RETURN(interface_version == NULL || phy_id >= RA_MAX_PHY_ID_NUM,
        hccp_err("[get][ra_interface_version]para invalid! interface_version is NULL or phy_id(%u) >= [%u]",
        phy_id, RA_MAX_PHY_ID_NUM), -EINVAL);

    *interface_version = 0;
    for (i = 0; i < num; i++) {
        if (g_ra_interface_info_list[i].opcode == interface_opcode) {
            *interface_version = g_ra_interface_info_list[i].version;
            break;
        }
    }
    return 0;
}

STATIC int ra_hdc_get_opcode_version(unsigned int phy_id, unsigned int interface_opcode,
    unsigned int *interface_version)
{
    union op_get_version_data version_info = {0};
    int ret;

    version_info.tx_data.opcode = interface_opcode;

    ret = ra_hdc_process_msg(RA_RS_GET_INTERFACE_VERSION, phy_id, (char *)&version_info,
        sizeof(union op_get_version_data));
    CHK_PRT_RETURN(ret, hccp_err("[get][ra_hdc_interface_version]ra hdc message process failed ret(%d) phy_id(%u)",
        ret, phy_id), ret);

    *interface_version = version_info.rx_data.version;
    return 0;
}

void ra_hdc_get_all_opcode_version(unsigned int phy_id)
{
    int num = sizeof(g_ra_interface_info_list) / sizeof(g_ra_interface_info_list[0]);
    int ret;
    int i;

    for (i = 0; i < num; i++) {
        ret = ra_hdc_get_opcode_version(phy_id, g_ra_interface_info_list[i].opcode,
            &g_ra_interface_info_list[i].version);
        if (ret != 0) {
            hccp_warn("ra_hdc_get_opcode_version unsuccessful, ret[%d], opcode[%d]",
                ret, g_ra_interface_info_list[i].opcode);
            continue;
        }
    }

    return;
}

STATIC int ra_hdc_send_pid(unsigned int phy_id, struct process_ra_sign p_ra_sign)
{
    union op_set_pid_data set_pid_data = {0};
    int ret;

    set_pid_data.tx_data.pid = p_ra_sign.tgid;
    set_pid_data.tx_data.phy_id = phy_id;

    ret = strcpy_s(set_pid_data.tx_data.pid_sign, PROCESS_RA_SIGN_LENGTH, p_ra_sign.sign);
    CHK_PRT_RETURN(ret, hccp_err("[send][ra_hdc_pid]Invalid pid sign, ret(%d)", ret), -ESAFEFUNC);

    ret = ra_hdc_process_msg(RA_RS_SET_PID, phy_id,
        (char *)&set_pid_data, sizeof(union op_set_pid_data));
    CHK_PRT_RETURN(ret, hccp_err("[send][ra_hdc_pid]ra hdc message process failed ret(%d) phy_id(%u)",
        ret, phy_id), ret);

    return 0;
}

STATIC int ra_hdc_init_apart(unsigned int phy_id, unsigned int *logic_id)
{
    int ret;
    ret = dl_drv_device_get_index_by_phy_id(phy_id, logic_id);
    CHK_PRT_RETURN(ret, hccp_err("[init][ra_hdc_apart]get logic id fail(%d), phy_id(%u)", ret, phy_id), -ENODEV);

    ret = pthread_mutex_init(&g_ra_hdc[phy_id].lock, NULL);
    CHK_PRT_RETURN(ret, hccp_err("[init][ra_hdc_apart]pthread_mutex_init failed, ret(%d) phy_id(%u)",
        ret, phy_id), -ESYSFUNC);
    return 0;
}

STATIC int ra_hdc_init_session_connect_ex(int peer_node, int peer_devid, unsigned int phy_id, HDC_SESSION *session)
{
    struct halQueryDevpidInfo info = {0};
    pid_t dev_pid;
    int ret;

    info.hostpid = getpid();
    info.devid = (unsigned int)peer_devid;
    info.proc_type = DEVDRV_PROCESS_HCCP;
    ret = dl_hal_query_dev_pid(info, &dev_pid);
    if (ret != 0) {
        hccp_err("[init][ra_hdc]hdc dl_hal_query_dev_pid failed ret(%d) peer_devid(%d) phy_id(%u)",
            ret, peer_devid, phy_id);
        return ret;
    }

    return RA_HDC_OPS.session_connect_ex(peer_node, peer_devid, dev_pid, g_ra_hdc[phy_id].client, session);
}

int ra_hdc_init_session(int peer_node, int peer_devid, unsigned int phy_id, int hdc_type, HDC_SESSION *session)
{
    if (hdc_type == HDC_SERVICE_TYPE_RDMA_V2) {
        return ra_hdc_init_session_connect_ex(peer_node, peer_devid, phy_id, session);
    }

    // default hdc type: HDC_SERVICE_TYPE_RDMA
    return RA_HDC_OPS.session_connect(peer_node, peer_devid, g_ra_hdc[phy_id].client, session);
}

void ra_hdc_deinit_session(HDC_SESSION *session)
{
    if (*session == NULL) {
        return;
    }

    (void)RA_HDC_OPS.session_close(*session);
    *session = NULL;
    return;
}

int ra_hdc_set_session_reference(HDC_SESSION *session)
{
    return RA_HDC_OPS.set_session_reference(*session);
}

int ra_hdc_init(struct ra_init_config *cfg, struct process_ra_sign p_ra_sign)
{
    unsigned int logic_id = RA_MAX_PHY_ID_NUM;
    unsigned int phy_id = cfg->phy_id;
    int hdc_type = cfg->hdc_type;
    int ret;

    ret = dl_hal_init();
    CHK_PRT_RETURN(ret, hccp_err("[init][ra_hdc]dl_hal_init failed, ret = %d", ret), ret);

    ret = ra_hdc_init_apart(phy_id, &logic_id);
    CHK_PRT_RETURN(ret, hccp_err("[init][ra_hdc]ra_hdc_init_apart failed, ret(%d)", ret), ret);

    hccp_run_info("hdc init start! logic id is %u, phy id is %u, hdc_type is %d", logic_id, phy_id, hdc_type);

    if (g_ra_hdc[phy_id].session == NULL) {
        // maxSessionNum 2U include: sync & async session
        ret = RA_HDC_OPS.client_create(&g_ra_hdc[phy_id].client, 2U, hdc_type, 0);
        if (ret != 0) {
            hccp_err("[init][ra_hdc]hdc client create failed, hccp not up ret(%d) phy_id(%u)", ret, phy_id);
            goto HDC_ERR;
        }
        ret = ra_hdc_init_session(0, (int)logic_id, phy_id, hdc_type, &g_ra_hdc[phy_id].session);
        if (ret != 0) {
            hccp_err("[init][ra_hdc]hdc session_connect failed ret(%d) logic_id(%u) phy_id(%u)",
                ret, logic_id, phy_id);
            goto CONN_ERR;
        }
        ret = RA_HDC_OPS.set_session_reference(g_ra_hdc[phy_id].session);
        if (ret != 0) {
            hccp_err("[init][ra_hdc]hdc set_session_reference failed, ret(%d) phy_id(%u)", ret, phy_id);
            goto SESS_ERR;
        }
    } else {
        hccp_warn("hdc session for phy_id[%u] already existed", phy_id);
        return -EEXIST;
    }

    ret = ra_hdc_send_pid(phy_id, p_ra_sign);
    if (ret) {
        hccp_err("[init][ra_hdc]set pid for phy_id(%u) failed, ret(%d), host_tgid(%d)", phy_id, ret, p_ra_sign.tgid);
        goto SESS_ERR;
    }

    ret = ra_hdc_lite_init_cqe_err_info(phy_id);
    if (ret) {
        hccp_err("[init][ra_hdc]ra_hdc_lite_init_cqe_err_info failed, ret(%d)", ret);
        goto SESS_ERR;
    }

    hccp_run_info("hdc init OK! phy_id[%u]", phy_id);

    return 0;
SESS_ERR:
    RA_HDC_OPS.session_close(g_ra_hdc[phy_id].session);
    g_ra_hdc[phy_id].session = NULL;
CONN_ERR:
    RA_HDC_OPS.client_destroy(g_ra_hdc[phy_id].client);
    g_ra_hdc[phy_id].client = NULL;
HDC_ERR:
    pthread_mutex_destroy(&g_ra_hdc[phy_id].lock);
    return -EPERM;
}

int ra_hdc_get_tls_enable(unsigned int phy_id, bool *tls_enable)
{
    union op_get_tls_enable_data op_data = {0};
    int ret;

    op_data.tx_data.phy_id = phy_id;
    ret = ra_hdc_process_msg(RA_RS_GET_TLS_ENABLE, phy_id, (char *)&op_data, sizeof(union op_get_tls_enable_data));
    CHK_PRT_RETURN(ret != 0, hccp_err("[get][tls_enable]ra hdc message process failed ret(%d) phy_id(%u)",
        ret, phy_id), ret);

    *tls_enable = op_data.rx_data.tls_enable;
    return ret;
}

STATIC int ra_hdc_session_close(unsigned int phy_id)
{
    union op_hdc_close_data hdc_close_data = {0};
    int ret;

    hdc_close_data.tx_data.phy_id = phy_id;

    ret = ra_hdc_process_msg(RA_RS_HDC_SESSION_CLOSE, phy_id, (char *)&hdc_close_data,
        sizeof(union op_hdc_close_data));
    CHK_PRT_RETURN(ret, hccp_err("[close][ra_hdc_session]ra hdc message process failed ret(%d) phy_id(%u)",
        ret, phy_id), ret);

    return 0;
}

STATIC int ra_hdc_client_deinit(unsigned int phy_id)
{
    int ret;

    g_ra_hdc[phy_id].start_deinit = 1;
    ret = ra_hdc_session_close(phy_id);
    if (ret) {
        hccp_err("[deinit][ra_hdc]close hdc session failed, ret(%d) phy_id(%u)", ret, phy_id);
    }
    ra_hdc_deinit_session(&g_ra_hdc[phy_id].snapshot_session);
    ra_hdc_deinit_session(&g_ra_hdc[phy_id].session);

    ret = RA_HDC_OPS.client_destroy(g_ra_hdc[phy_id].client);
    if (ret) {
        hccp_err("[deinit][ra_hdc]hdc client_destroy failed, ret(%d) phy_id(%u)", ret, phy_id);
    }
    g_ra_hdc[phy_id].client = NULL;
    g_ra_hdc[phy_id].start_deinit = 0;

    return ret;
}

int ra_hdc_deinit(struct ra_init_config *cfg)
{
    unsigned int phy_id = cfg->phy_id;
    int ret;

    hccp_run_info("hdc deinit start! phy_id[%u] restore_flag[%u]", phy_id, g_ra_hdc[phy_id].restore_flag);

    ra_hdc_lite_deinit_cqe_err_info(phy_id);

    ret = ra_hdc_client_deinit(phy_id);
    if (ret != 0) {
        hccp_err("[deinit][ra_hdc]client deinit failed! ret(%d) phy_id(%u)", ret, phy_id);
    }

    ret = pthread_mutex_destroy(&g_ra_hdc[phy_id].lock);
    if (ret != 0) {
        hccp_err("[deinit][ra_hdc]pthread_mutex_destroy failed! ret(%d) phy_id(%u)", ret, phy_id);
        ret = -ESYSFUNC;
    }

    CHK_PRT_RETURN(ret != 0, hccp_err("[deinit][ra_hdc]hdc deinit failed! phy_id(%u)", phy_id), ret);
    dl_hal_deinit();

    (void)memset_s(&g_ra_hdc[phy_id], sizeof(g_ra_hdc[phy_id]), 0, sizeof(g_ra_hdc[phy_id]));

    hccp_run_info("hdc deinit OK! phy_id[%u]", phy_id);
    return 0;
}

STATIC int ra_hdc_get_valid_cqe_err_info(
    struct cqe_err_info *out_info, struct cqe_err_info info0, struct cqe_err_info info1)
{
    int ret;

    if (info0.status != 0 && info1.status != 0) {
        if (timercmp((&info0.time), (&info1.time), <)) {
            ret = memcpy_s(out_info, sizeof(struct cqe_err_info), &info0, sizeof(struct cqe_err_info));
        } else {
            ret = memcpy_s(out_info, sizeof(struct cqe_err_info), &info1, sizeof(struct cqe_err_info));
        }
    } else {
        if (info0.status == 0) {
            ret = memcpy_s(out_info, sizeof(struct cqe_err_info), &info1, sizeof(struct cqe_err_info));
        } else {
            ret = memcpy_s(out_info, sizeof(struct cqe_err_info), &info0, sizeof(struct cqe_err_info));
        }
    }

    if (ret) {
        hccp_err("memcpy_s failed, ret(%d)", ret);
        return -ESAFEFUNC;
    }

    return 0;
}

int ra_hdc_get_cqe_err_info(unsigned int phy_id, struct cqe_err_info *info)
{
    int ret;
    struct cqe_err_info op_cqe_info = { 0 };
    union op_get_cqe_err_info_data cqe_err_info_data;

    ra_hdc_lite_get_cqe_err_info(phy_id, &op_cqe_info);

    RA_PTHREAD_MUTEX_LOCK(&g_ra_hdc[phy_id].lock);
    HDC_SESSION session = g_ra_hdc[phy_id].session;
    if (session == NULL) {
        RA_PTHREAD_MUTEX_UNLOCK(&g_ra_hdc[phy_id].lock);
        return 0;
    }
    RA_PTHREAD_MUTEX_UNLOCK(&g_ra_hdc[phy_id].lock);

    ret = memset_s(&cqe_err_info_data, sizeof(cqe_err_info_data), 0, sizeof(cqe_err_info_data));
    CHK_PRT_RETURN(ret, hccp_err("[init]memset_s failed ret(%d)", ret), -ESAFEFUNC);
    ret = ra_hdc_process_msg(RA_RS_GET_CQE_ERR_INFO, phy_id,
        (char *)&cqe_err_info_data, sizeof(union op_get_cqe_err_info_data));
    CHK_PRT_RETURN(ret, hccp_err("ra hdc message process failed ret(%d)", ret), ret);

    return ra_hdc_get_valid_cqe_err_info(info, op_cqe_info, cqe_err_info_data.rx_data.info);
}

STATIC int ra_hdc_session_save_snapshot(unsigned int phy_id, enum save_snapshot_action action)
{
    int ret = 0;

    RA_PTHREAD_MUTEX_LOCK(&g_ra_hdc[phy_id].lock);
    if (action == SAVE_SNAPSHOT_ACTION_PRE_PROCESSING && g_ra_hdc[phy_id].session != NULL) {
        g_ra_hdc[phy_id].snapshot_session = g_ra_hdc[phy_id].session;
        g_ra_hdc[phy_id].session = NULL;
    } else if (action == SAVE_SNAPSHOT_ACTION_POST_PROCESSING && g_ra_hdc[phy_id].session == NULL) {
        g_ra_hdc[phy_id].session = g_ra_hdc[phy_id].snapshot_session;
        g_ra_hdc[phy_id].snapshot_session = NULL;
    } else {
        hccp_err("duplicate or incorrect order calls are not allowed, phy_id[%u] action[%d]", phy_id, action);
        ret = -EPERM;
    }
    RA_PTHREAD_MUTEX_UNLOCK(&g_ra_hdc[phy_id].lock);

    return ret;
}

int ra_hdc_save_snapshot(unsigned int phy_id, enum save_snapshot_action action)
{
    int ret;

    ret = ra_hdc_async_save_snapshot(phy_id, action);
    CHK_PRT_RETURN(ret != 0, hccp_err("ra_hdc_async_save_snapshot failed ret[%d]", ret), ret);
    ret = ra_hdc_session_save_snapshot(phy_id, action);
    CHK_PRT_RETURN(ret != 0, hccp_err("ra_hdc_session_save_snapshot failed ret[%d]", ret), ret);

    return ret;
}

STATIC void ra_hdc_session_restore_snapshot(unsigned int phy_id)
{
    RA_PTHREAD_MUTEX_LOCK(&g_ra_hdc[phy_id].lock);
    g_ra_hdc[phy_id].restore_flag = 1;
    RA_PTHREAD_MUTEX_UNLOCK(&g_ra_hdc[phy_id].lock);
}

int ra_hdc_restore_snapshot(unsigned int phy_id)
{
    int ret;

    ret = ra_hdc_async_restore_snapshot(phy_id);
    CHK_PRT_RETURN(ret != 0, hccp_err("ra_hdc_async_restore_snapshot failed ret[%d]", ret), ret);
    ra_hdc_session_restore_snapshot(phy_id);

    return ret;
}

int ra_hdc_get_sec_random(unsigned int phy_id, unsigned int *value)
{
    union op_get_sec_random_data op_data = {0};
    int ret;

    ret = ra_hdc_process_msg(RA_RS_GET_SEC_RANDOM, phy_id, (char *)&op_data, sizeof(union op_get_sec_random_data));
    CHK_PRT_RETURN(ret != 0, hccp_err("[get][sec_random]ra hdc message process failed ret(%d) phy_id(%u)",
        ret, phy_id), ret);

    *value = op_data.rx_data.value;
    return ret;
}

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

struct hdc_async_info g_ra_hdc_async[RA_MAX_PHY_ID_NUM] = { 0 };

struct ra_async_op_handle g_ra_async_op_handle[] = {
    {RA_RS_SOCKET_SEND, SOCKET_OP, ra_hdc_async_handle_socket_send, sizeof(union op_socket_send_data)},
    {RA_RS_SOCKET_RECV, SOCKET_OP, ra_hdc_async_handle_socket_recv, sizeof(union op_socket_recv_data)},
    {RA_RS_SOCKET_LISTEN_START, SOCKET_OP, ra_hdc_async_handle_socket_listen_start,
        sizeof(union op_socket_listen_data)},
    {RA_RS_SOCKET_LISTEN_STOP, SOCKET_OP, NULL, sizeof(union op_socket_listen_data)},
    {RA_RS_SOCKET_CONN, SOCKET_OP, NULL, sizeof(union op_socket_connect_data)},
    {RA_RS_SOCKET_CLOSE, SOCKET_OP, ra_hdc_async_handle_socket_batch_close, sizeof(union op_socket_close_data)},
    {RA_RS_HDC_SESSION_CLOSE, OTHERS, NULL, sizeof(union op_hdc_close_data)},
};

STATIC struct ra_async_op_handle *ra_hdc_is_async_op(unsigned int opcode)
{
    int num = sizeof(g_ra_async_op_handle) / sizeof(g_ra_async_op_handle[0]);
    int i;

    for (i = 0; i < num; i++) {
        if (g_ra_async_op_handle[i].opcode == (enum op_type)opcode) {
            return &g_ra_async_op_handle[i];
        }
    }
    return NULL;
}

STATIC void hdc_async_handle_priv_data(struct ra_request_handle *req_handle)
{
    if (req_handle->op_handle->priv_data_handle == NULL) {
        return;
    }

    req_handle->op_handle->priv_data_handle(req_handle);
}

STATIC void hdc_async_set_request(struct ra_request_handle *req_handle, unsigned int req_id,
    struct ra_async_op_handle *op_handle, unsigned int phy_id, unsigned int data_size)
{
    req_handle->req_id = req_id;
    req_handle->op_handle = op_handle;
    req_handle->phy_id = phy_id;
    req_handle->data_size = data_size;
}

STATIC int hdc_async_get_request(struct hdc_async_info *async_info, unsigned int req_id,
    struct ra_request_handle **req_handle)
{
    struct ra_request_handle *req_tmp2 = NULL;
    struct ra_request_handle *req_tmp = NULL;

    // no need to use lock: req_id always exist in current req_list(the data is always sent before it is received)
    RA_LIST_GET_HEAD_ENTRY(req_tmp, req_tmp2, &async_info->req_list, list, struct ra_request_handle);
    for (; (&req_tmp->list) != &async_info->req_list;
        req_tmp = req_tmp2, req_tmp2 = list_entry(req_tmp2->list.next, struct ra_request_handle, list)) {
        if (req_tmp->req_id == req_id) {
            *req_handle = req_tmp;
            return 0;
        }
    }
    *req_handle = NULL;
    return -ENODEV;
}

STATIC void hdc_async_set_req_done(struct ra_request_handle *req_handle, unsigned int phy_id, int ret)
{
    RA_PTHREAD_MUTEX_LOCK(&g_ra_hdc_async[phy_id].rsp_mutex);
    ra_list_add_tail(&req_handle->list, &g_ra_hdc_async[phy_id].rsp_list);
    RA_PTHREAD_MUTEX_UNLOCK(&g_ra_hdc_async[phy_id].rsp_mutex);
    req_handle->op_ret = (ret != 0) ? ret : req_handle->op_ret;
    hccp_dbg("opcode[%u] req_id[%u] phy_id[%u]", req_handle->op_handle->opcode, req_handle->req_id, phy_id);
    req_handle->is_done = true;
}

STATIC void ra_hw_async_set_connect_status(unsigned int phy_id, unsigned int connect_status)
{
    g_ra_hdc_async[phy_id].connect_status = connect_status;
}

STATIC bool hdc_async_is_msg_valid(unsigned int phy_id, struct msg_head *recv_msg_head, unsigned int recv_len,
    struct ra_request_handle **req_handle)
{
    struct ra_request_handle *req_handle_tmp = NULL;
    int ret;

    // check recv_len and get req_handle
    CHK_PRT_RETURN(recv_len < sizeof(struct msg_head),
        hccp_run_warn("[async][ra_hdc_recv]recv_len[%u] < [%lu] is invalid", recv_len, sizeof(struct msg_head)), false);
    ret = hdc_async_get_request(&g_ra_hdc_async[phy_id], recv_msg_head->async_req_id, &req_handle_tmp);
    CHK_PRT_RETURN(req_handle_tmp == NULL, hccp_run_warn("[async][ra_hdc_recv]req_id[%u] invalid, ret[%d], opcode[%u]",
        recv_msg_head->async_req_id, ret, recv_msg_head->opcode), false);

    // del req_handle from req_list
    ra_list_del(&req_handle_tmp->list);

    // opcode RA_RS_HDC_SESSION_CLOSE
    if (recv_msg_head->opcode == RA_RS_HDC_SESSION_CLOSE) {
        ra_hw_async_set_connect_status(phy_id, HDC_UNCONNECTED);
        hccp_dbg("opcode[%u] req_id[%u] phy_id[%u]", recv_msg_head->opcode, req_handle_tmp->req_id, phy_id);
        req_handle_tmp->is_done = true;
        return false;
    }

    // need to check op_data size and recv data size
    if ((req_handle_tmp->data_size != recv_msg_head->msg_data_len) ||
        (recv_msg_head->msg_data_len + (unsigned int)sizeof(struct msg_head)) != recv_len) {
        hccp_run_warn("[async][ra_hdc_recv]opcode[%u] data_size[%u] msg_data_len[%u] mismatch or recv_len[%u] mismatch",
            recv_msg_head->opcode, req_handle_tmp->data_size, recv_msg_head->msg_data_len, recv_len);
        hdc_async_set_req_done(req_handle_tmp, phy_id, -EINVAL);
        return false;
    }

    *req_handle = req_handle_tmp;
    return true;
}

STATIC int hdc_async_add_response(unsigned int phy_id, void *recv_buf, unsigned int recv_len)
{
    struct ra_request_handle *req_handle_tmp = NULL;
    struct msg_head *recv_msg_head = NULL;
    int ret = 0;

    recv_msg_head = (struct msg_head *)recv_buf;
    // check recv msg: req_id, opcode, msg_data_len and get req_handle
    if (!hdc_async_is_msg_valid(phy_id, recv_msg_head, recv_len, &req_handle_tmp)) {
        return -EINVAL;
    }

    //  handle recv msg
    req_handle_tmp->recv_buf = (void *)calloc(recv_msg_head->msg_data_len, sizeof(char));
    if (req_handle_tmp->recv_buf == NULL) {
        hccp_err("[async][ra_hdc_recv]calloc recv_buf failed, msg_data_len[%u] req_id[%u] opcode[%u]",
            recv_msg_head->msg_data_len, recv_msg_head->async_req_id, recv_msg_head->opcode);
        ret = -ENOMEM;
        goto out;
    }
    (void)memcpy_s(req_handle_tmp->recv_buf, recv_msg_head->msg_data_len, recv_buf + sizeof(struct msg_head),
        recv_msg_head->msg_data_len);
    req_handle_tmp->recv_len = recv_msg_head->msg_data_len;
    req_handle_tmp->op_ret = recv_msg_head->ret;
    hdc_async_handle_priv_data(req_handle_tmp);

out:
    hdc_async_set_req_done(req_handle_tmp, phy_id, ret);
    return ret;
}

static void hdc_async_del_req_handle(struct ra_request_handle *req_handle, pthread_mutex_t *mutex)
{
    RA_PTHREAD_MUTEX_LOCK(mutex);
    ra_list_del(&req_handle->list);
    RA_PTHREAD_MUTEX_UNLOCK(mutex);
    if (req_handle->recv_buf != NULL && req_handle->recv_len != 0) {
        free(req_handle->recv_buf);
        req_handle->recv_buf = NULL;
        req_handle->recv_len = 0;
    }
    // async api return failed, free corresponding handle
    if (req_handle->op_ret != 0 && req_handle->priv_handle != NULL) {
        free(req_handle->priv_handle);
        req_handle->priv_handle = NULL;
    }
    free(req_handle);
    req_handle = NULL;
    return;
}

void hdc_async_del_response(struct ra_request_handle *req_handle)
{
    hdc_async_del_req_handle(req_handle, &g_ra_hdc_async[req_handle->phy_id].rsp_mutex);
}

int ra_hdc_send_msg_async(unsigned int opcode, unsigned int phy_id, char *data, unsigned int data_size,
    struct ra_request_handle *req_handle)
{
    struct ra_async_op_handle *op_handle_tmp = NULL;
    unsigned int async_req_id = 0;
    void *send_buf = NULL;
    unsigned int send_len;
    pid_t host_tgid;
    int ret;

    if (g_ra_hdc_async[phy_id].restore_flag != 0) {
        return 0;
    }

    CHK_PRT_RETURN(ra_hdc_is_broken(g_ra_hdc_async[phy_id].last_recv_status),
        hccp_err("[async][ra_hdc_send]HDC broken, phy_id(%u)", phy_id), -g_ra_hdc_async[phy_id].last_recv_status);
    op_handle_tmp = ra_hdc_is_async_op(opcode);
    CHK_PRT_RETURN(op_handle_tmp == NULL, hccp_err("[async][ra_hdc_send]opcode[%u] invalid", opcode), -EINVAL);

    host_tgid = g_ra_hdc_async[phy_id].host_tgid;
    send_len = (unsigned int)sizeof(struct msg_head) + data_size;
    send_buf = (void *)calloc(send_len, sizeof(char));
    CHK_PRT_RETURN(send_buf == NULL, hccp_err("[async][ra_hdc_send]calloc send_buf failed. phy_id(%u) opcode(%u)",
        phy_id, opcode), -ENOMEM);

    RA_PTHREAD_MUTEX_LOCK(&g_ra_hdc_async[phy_id].req_mutex);
    async_req_id = g_ra_hdc_async[phy_id].req_id;
    g_ra_hdc_async[phy_id].req_id++;

    msg_head_build_up(send_buf, opcode, async_req_id, data_size, host_tgid);
    ret = memcpy_s(send_buf + sizeof(struct msg_head), send_len - sizeof(struct msg_head), data, data_size);
    if (ret != 0) {
        hccp_err("[async][ra_hdc_send]memcpy_s failed, ret(%d) phy_id(%u) opcode(%u)", ret, phy_id, opcode);
        ret = -ESAFEFUNC;
        goto out;
    }

    hdc_async_set_request(req_handle, async_req_id, op_handle_tmp, phy_id, data_size);
    ret = hdc_async_send_pkt(&g_ra_hdc_async[phy_id], phy_id, send_buf, send_len, req_handle);
    if (ret != 0) {
        hccp_err("[async][ra_hdc_send]hdc_async_send_pkt opcode(%u) failed ret(%d) phy_id(%u)", opcode, ret, phy_id);
        goto out;
    }

    hccp_dbg("opcode[%u] req_id[%u] phy_id[%u]", op_handle_tmp->opcode, async_req_id, phy_id);

out:
    RA_PTHREAD_MUTEX_UNLOCK(&g_ra_hdc_async[phy_id].req_mutex);
    free(send_buf);
    send_buf = NULL;
    return ret;
}

STATIC int ra_hdc_async_session_connect(struct ra_init_config *cfg)
{
    union op_async_hdc_connect_data async_data = {0};
    int ret;

    async_data.tx_data.phy_id = cfg->phy_id;
    async_data.tx_data.queue_size = MAX_POOL_QUEUE_SIZE;
    async_data.tx_data.thread_num = RA_POOL_THREAD_NUM;
    ret = ra_hdc_process_msg(RA_RS_ASYNC_HDC_SESSION_CONNECT, cfg->phy_id, (char *)&async_data,
        sizeof(union op_async_hdc_connect_data));
    CHK_PRT_RETURN(ret != 0, hccp_err("[init][ra_hdc_async]ra hdc message process failed ret[%d] phy_id[%u]",
        ret, cfg->phy_id), ret);
    return ret;
}

STATIC int ra_hdc_async_session_close(unsigned int phy_id)
{
    union op_async_hdc_close_data async_data = {0};
    struct ra_request_handle *req_handle = NULL;
    union op_hdc_close_data op_data = {0};
    int timeout = RA_THREAD_TRY_TIME;
    int ret;

    if (g_ra_hdc_async[phy_id].restore_flag != 0) {
        return 0;
    }

    // close async session
    op_data.tx_data.phy_id = phy_id;
    req_handle = (struct ra_request_handle *)calloc(1, sizeof(struct ra_request_handle));
    CHK_PRT_RETURN(req_handle == NULL,
        hccp_err("[deinit][ra_hdc_async]calloc req_handle failed, phy_id[%u]", phy_id), -ENOMEM);
    ret = ra_hdc_send_msg_async(RA_RS_HDC_SESSION_CLOSE, phy_id, (char *)&op_data, sizeof(union op_hdc_close_data),
        req_handle);
    if (ret != 0) {
        hccp_err("[deinit][ra_hdc_async]hdc async send message failed ret[%d] phy_id[%u]", ret, phy_id);
        free(req_handle);
        req_handle = NULL;
        return ret;
    }

    // wait request done until time out: RA_THREAD_TRY_TIME * RA_THREAD_SLEEP_TIME us
    while (!req_handle->is_done && timeout > 0) {
        usleep(RA_THREAD_SLEEP_TIME);
        timeout--;
    }
    if (timeout <= 0) {
        hccp_warn("[deinit][ra_hdc_async]hdc async session close timeout:%d phy_id[%u]", timeout, phy_id);
    }
    free(req_handle);
    req_handle = NULL;

    // destroy async recv thread and work thread pool
    ret = ra_hdc_process_msg(RA_RS_ASYNC_HDC_SESSION_CLOSE, phy_id, (char *)&async_data,
        sizeof(union op_async_hdc_close_data));
    CHK_PRT_RETURN(ret != 0, hccp_err("[deinit][ra_hdc_async]ra hdc message process failed ret[%d] phy_id[%u]",
        ret, phy_id), ret);
    return ret;
}

STATIC void ra_hw_async_hdc_server_init(void *arg)
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
    ret = ra_hdc_async_session_connect(&cfg);
    if (ret != 0) {
        hccp_err("[init][ra_hdc_async]ra_hdc_async_session_connect failed ret[%d] phy_id[%u]", ret, cfg.phy_id);
        return;
    }
    return;
}

STATIC void ra_hw_async_hdc_client_init(void *arg)
{
    struct ra_init_config cfg = {0};
    unsigned int logic_id = 0;
    unsigned int phy_id = 0;
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

    phy_id = cfg.phy_id;
    ret = dl_drv_device_get_index_by_phy_id(phy_id, &logic_id);
    if (ret != 0) {
        hccp_err("get logic id failed(%d), phy_id(%u)", ret, phy_id);
        return;
    }

    ret = ra_hdc_init_session(0, (int)logic_id, phy_id, cfg.hdc_type, &g_ra_hdc_async[phy_id].session);
    if (ret != 0) {
        hccp_err("hdc session_connect failed ret(%d) phy_id(%u)", ret, phy_id);
        return;
    }

    ret = ra_hdc_set_session_reference(&g_ra_hdc_async[phy_id].session);
    if (ret != 0) {
        goto set_ref_err;
    }

    ra_hw_async_set_connect_status(phy_id, HDC_CONNECTED);
    return;

set_ref_err:
    ra_hdc_deinit_session(&g_ra_hdc_async[phy_id].session);
    return;
}

STATIC void ra_hw_async_set_thread_status(unsigned int phy_id, unsigned int thread_status)
{
    g_ra_hdc_async[phy_id].thread_status = thread_status;
}

STATIC void ra_hw_async_del_list(struct ra_list_head *head, pthread_mutex_t *mutex)
{
    struct ra_request_handle *req_next = NULL;
    struct ra_request_handle *req_cur = NULL;

    RA_LIST_GET_HEAD_ENTRY(req_cur, req_next, head, list, struct ra_request_handle);
    for (; (&req_cur->list) != head;
        req_cur = req_next, req_next = list_entry(req_next->list.next, struct ra_request_handle, list)) {
        hdc_async_del_req_handle(req_cur, mutex);
    }
}

STATIC void ra_hw_async_hdc_client_deinit(unsigned int phy_id)
{
    int try_again = HDC_TRY_TIME;

    // destroy thread
    ra_hw_async_set_thread_status(phy_id, THREAD_DESTROYING);
    while ((g_ra_hdc_async[phy_id].thread_status != THREAD_HALT) && (try_again != 0)) {
        usleep(HDC_USLEEP_TIME);
        try_again--;
    }
    if (try_again <= 0) {
        hccp_warn("hdc async message thread quit timeout");
    }

    // close session
    ra_hw_async_set_connect_status(phy_id, HDC_UNCONNECTED);
    RA_PTHREAD_MUTEX_LOCK(&g_ra_hdc_async[phy_id].send_mutex);
    RA_PTHREAD_MUTEX_LOCK(&g_ra_hdc_async[phy_id].recv_mutex);
    ra_hdc_deinit_session(&g_ra_hdc_async[phy_id].snapshot_session);
    ra_hdc_deinit_session(&g_ra_hdc_async[phy_id].session);
    RA_PTHREAD_MUTEX_UNLOCK(&g_ra_hdc_async[phy_id].recv_mutex);
    RA_PTHREAD_MUTEX_UNLOCK(&g_ra_hdc_async[phy_id].send_mutex);

    ra_hw_async_del_list(&g_ra_hdc_async[phy_id].req_list, &g_ra_hdc_async[phy_id].req_mutex);
    ra_hw_async_del_list(&g_ra_hdc_async[phy_id].rsp_list, &g_ra_hdc_async[phy_id].rsp_mutex);
}

STATIC int ra_hdc_async_mutex_init(unsigned int phy_id)
{
    int ret = 0;

    ret = pthread_mutex_init(&g_ra_hdc_async[phy_id].send_mutex, NULL);
    if (ret != 0) {
        hccp_err("[init][ra_hdc_async]pthread_mutex_init send_mutex failed ret(%d) phy_id(%u)", ret, phy_id);
        return -ESYSFUNC;
    }
    ret = pthread_mutex_init(&g_ra_hdc_async[phy_id].recv_mutex, NULL);
    if (ret != 0) {
        hccp_err("[init][ra_hdc_async]pthread_mutex_init recv_mutex failed ret(%d) phy_id(%u)", ret, phy_id);
        ret = -ESYSFUNC;
        goto recv_mutex_fail;
    }
    ret = pthread_mutex_init(&g_ra_hdc_async[phy_id].req_mutex, NULL);
    if (ret != 0) {
        hccp_err("[init][ra_hdc_async]pthread_mutex_init req_mutex failed ret(%d) phy_id(%u)", ret, phy_id);
        ret = -ESYSFUNC;
        goto req_mutex_fail;
    }
    ret = pthread_mutex_init(&g_ra_hdc_async[phy_id].rsp_mutex, NULL);
    if (ret != 0) {
        hccp_err("[init][ra_hdc_async]pthread_mutex_init rsp_mutex failed ret(%d) phy_id(%u)", ret, phy_id);
        ret = -ESYSFUNC;
        goto rsp_mutex_fail;
    }

    return 0;

rsp_mutex_fail:
    (void)pthread_mutex_destroy(&g_ra_hdc_async[phy_id].req_mutex);
req_mutex_fail:
    (void)pthread_mutex_destroy(&g_ra_hdc_async[phy_id].recv_mutex);
recv_mutex_fail:
    (void)pthread_mutex_destroy(&g_ra_hdc_async[phy_id].send_mutex);
    return ret;
}

STATIC void ra_hdc_async_mutex_deinit(unsigned int phy_id)
{
    (void)pthread_mutex_destroy(&g_ra_hdc_async[phy_id].rsp_mutex);
    (void)pthread_mutex_destroy(&g_ra_hdc_async[phy_id].req_mutex);
    (void)pthread_mutex_destroy(&g_ra_hdc_async[phy_id].recv_mutex);
    (void)pthread_mutex_destroy(&g_ra_hdc_async[phy_id].send_mutex);
}

STATIC int ra_hdc_async_init_session(struct ra_init_config *cfg)
{
    unsigned int phy_id = cfg->phy_id;
    int timeout = RA_THREAD_TRY_TIME;
    pthread_t server_tidp;
    pthread_t client_tidp;
    int ret = 0;

    CHK_PRT_RETURN(g_ra_hdc_async[phy_id].session != NULL, hccp_warn("hdc async session for phy_id[%u] already existed",
        phy_id), -EEXIST);

    // server will be blocked, use a thread to trigger server to accept
    ret = pthread_create(&server_tidp, NULL, (void *)ra_hw_async_hdc_server_init, cfg);
    CHK_PRT_RETURN(ret != 0, hccp_err("Create async_hdc_server_init pthread failed, ret(%d)", ret), -ESYSFUNC);

    // client will be blocked, use a thread to trigger client to connect
    ret = pthread_create(&client_tidp, NULL, (void *)ra_hw_async_hdc_client_init, cfg);
    CHK_PRT_RETURN(ret != 0, hccp_err("Create async_hdc_client_init pthread failed, ret(%d)", ret), -ESYSFUNC);

    // will block until time out: RA_CONNECT_TRY_TIME * RA_THREAD_SLEEP_TIME us
    timeout = RA_CONNECT_TRY_TIME;
    while (g_ra_hdc_async[phy_id].connect_status != HDC_CONNECTED && timeout > 0) {
        usleep(RA_THREAD_SLEEP_TIME);
        timeout--;
    }
    if (g_ra_hdc_async[phy_id].connect_status == HDC_UNCONNECTED || timeout <= 0) {
        hccp_err("HDC async connect timeout, connect_status %d, timeout %d, total_timeout %d(us)",
            g_ra_hdc_async[phy_id].connect_status, timeout, RA_CONNECT_TRY_TIME * RA_THREAD_SLEEP_TIME);
        return -ETIMEDOUT;
    }

    g_ra_hdc_async[phy_id].host_tgid = dl_drv_device_get_bare_tgid();
    ret = ra_hdc_async_mutex_init(phy_id);
    CHK_PRT_RETURN(ret != 0, hccp_err("ra_hdc_async_mutex_init failed, ret(%d), phy_id(%u)", ret, phy_id), ret);

    RA_INIT_LIST_HEAD(&g_ra_hdc_async[phy_id].req_list);
    RA_INIT_LIST_HEAD(&g_ra_hdc_async[phy_id].rsp_list);
    return 0;
}

STATIC void hdc_async_handle_recv_broken(struct hdc_async_info *async_info)
{
    struct ra_request_handle *req_next = NULL;
    struct ra_request_handle *req_curr = NULL;

    if (!ra_hdc_is_broken(async_info->last_recv_status)) {
        return;
    }

    RA_PTHREAD_MUTEX_LOCK(&async_info->req_mutex);
    RA_LIST_GET_HEAD_ENTRY(req_curr, req_next, &async_info->req_list, list, struct ra_request_handle);
    for (; (&req_curr->list) != &async_info->req_list;
        req_curr = req_next, req_next = list_entry(req_next->list.next, struct ra_request_handle, list)) {
        ra_list_del(&req_curr->list);
        hdc_async_set_req_done(req_curr, req_curr->phy_id, -async_info->last_recv_status);
    }
    RA_PTHREAD_MUTEX_UNLOCK(&async_info->req_mutex);
}

STATIC void *ra_hdc_recv_msg_async(void *arg)
{
    unsigned int phy_id = *(unsigned int *)arg;
    unsigned int recv_len = MAX_HDC_MSG_DATA;
    void *recv_buf = NULL;
    int ret;

    // free memory after using arg
    free(arg);
    arg = NULL;

    ret = pthread_detach(pthread_self());
    CHK_PRT_RETURN(ret, hccp_err("pthread detach failed ret %d, phy_id %u", ret, phy_id), NULL);

    (void)prctl(PR_SET_NAME, (unsigned long)"hccp_ra_async");

    hccp_info("[async][ra_hdc_recv]thread[%d] phy_id[%u] enter", getpid(), phy_id);
    ra_hw_async_set_thread_status(phy_id, THREAD_RUNNING);
    recv_buf = (void *)calloc(recv_len, sizeof(char));
    CHK_PRT_RETURN(recv_buf == NULL, hccp_err("[async][ra_hdc_recv]calloc recv_buf failed. phy_id(%u)", phy_id), NULL);

    while (1) {
        if (g_ra_hdc_async[phy_id].thread_status == THREAD_DESTROYING) {
            break;
        }
        RA_PTHREAD_MUTEX_LOCK(&g_ra_hdc_async[phy_id].recv_mutex);
        if (g_ra_hdc_async[phy_id].connect_status != HDC_CONNECTED) {
            RA_PTHREAD_MUTEX_UNLOCK(&g_ra_hdc_async[phy_id].recv_mutex);
            usleep(THREAD_SLEEP_TIME);
            continue;
        }

        if (ra_list_empty(&g_ra_hdc_async[phy_id].req_list)) {
            RA_PTHREAD_MUTEX_UNLOCK(&g_ra_hdc_async[phy_id].recv_mutex);
            usleep(THREAD_SLEEP_TIME);
            continue;
        }

        recv_len = MAX_HDC_MSG_DATA;
        ret = hdc_async_recv_pkt(&g_ra_hdc_async[phy_id], phy_id, recv_buf, &recv_len);
        if (ret != 0) {
            RA_PTHREAD_MUTEX_UNLOCK(&g_ra_hdc_async[phy_id].recv_mutex);
            hdc_async_handle_recv_broken(&g_ra_hdc_async[phy_id]);
            continue;
        }
        RA_PTHREAD_MUTEX_UNLOCK(&g_ra_hdc_async[phy_id].recv_mutex);

        RA_PTHREAD_MUTEX_LOCK(&g_ra_hdc_async[phy_id].req_mutex);
        (void)hdc_async_add_response(phy_id, recv_buf, recv_len);
        RA_PTHREAD_MUTEX_UNLOCK(&g_ra_hdc_async[phy_id].req_mutex);
    }

    hccp_info("[async][ra_hdc_recv]thread[%d] phy_id[%u] is out", getpid(), phy_id);
    ra_hw_async_set_thread_status(phy_id, THREAD_HALT);
    free(recv_buf);
    recv_buf = NULL;
    return NULL;
}

STATIC int ra_hdc_async_init_recv_thread(unsigned int phy_id)
{
    unsigned int *phy_id_tmp = NULL;
    int ret = 0;

    phy_id_tmp = (unsigned int *)calloc(1, sizeof(unsigned int));
    CHK_PRT_RETURN(phy_id_tmp == NULL, hccp_err("calloc phy_id_tmp failed, errno(%d)", errno), -ENOMEM);
    *phy_id_tmp = phy_id;

    // create a thread to recv msg from server
    ret = pthread_create(&g_ra_hdc_async[phy_id].tid, NULL, ra_hdc_recv_msg_async, (void *)phy_id_tmp);
    if (ret != 0) {
        hccp_err("Create ra_hdc_recv_msg_async pthread failed, ret(%d)", ret);
        goto err;
    }

    return 0;

err:
    free(phy_id_tmp);
    phy_id_tmp = NULL;
    return ret;
}

int ra_hdc_init_async(struct ra_init_config *cfg)
{
    unsigned int interface_version = 0;
    int ret = 0;

    CHK_PRT_RETURN(!cfg->enable_hdc_async, hccp_info("[init][ra_hdc_async]no need to init async hdc session"), 0);

    ret = ra_hdc_get_interface_version(cfg->phy_id, RA_RS_ASYNC_HDC_SESSION_CONNECT, &interface_version);
    // normal case: driver not support to or no need to init async hdc session
    CHK_PRT_RETURN(ret != 0 || interface_version < RA_RS_OPCODE_BASE_VERSION,
        hccp_run_warn("[init][ra_hdc_async]not support to init async hdc session, ret(%d), interface_version(%u)",
        ret, interface_version), 0);

    ret = ra_hdc_async_init_session(cfg);
    CHK_PRT_RETURN(ret != 0, hccp_err("[init][ra_hdc_async]ra_hdc_async_init_session failed, ret(%d) phy_id(%u)",
        ret, cfg->phy_id), ret);

    ret = ra_hdc_async_init_recv_thread(cfg->phy_id);
    if (ret != 0) {
        hccp_err("[init][ra_hdc_async]ra_hdc_async_init_recv_thread failed, ret(%d) phy_id(%u)", ret, cfg->phy_id);
        goto err;
    }

    return 0;

err:
    ra_hdc_async_mutex_deinit(cfg->phy_id);
    return -ESRCH;
}

int ra_hdc_deinit_async(unsigned int phy_id)
{
    int ret;

    hccp_run_info("hdc deinit async start! phy_id[%u] restore_flag[%u]", phy_id, g_ra_hdc_async[phy_id].restore_flag);

    CHK_PRT_RETURN(g_ra_hdc_async[phy_id].session == NULL && g_ra_hdc_async[phy_id].restore_flag == 0,
        hccp_warn("hdc async session for phy_id[%u] is NULL", phy_id), -ENODEV);

    // close server session
    ret = ra_hdc_async_session_close(phy_id);
    CHK_PRT_RETURN(ret != 0, hccp_err("[deinit][ra_hdc_async]ra_hdc_async_session_close failed ret[%d] phy_id[%u]",
        ret, phy_id), ret);

    // close client session & deinit client resources
    ra_hw_async_hdc_client_deinit(phy_id);

    ra_hdc_async_mutex_deinit(phy_id);

    (void)memset_s(&g_ra_hdc_async[phy_id], sizeof(g_ra_hdc_async[phy_id]), 0, sizeof(g_ra_hdc_async[phy_id]));

    return 0;
}

int ra_hdc_async_save_snapshot(unsigned int phy_id, enum save_snapshot_action action)
{
    int ret = 0;

    if (g_ra_hdc_async[phy_id].thread_status == THREAD_HALT) {
        return 0;
    }

#ifndef HNS_ROCE_LLT
    RA_PTHREAD_MUTEX_LOCK(&g_ra_hdc_async[phy_id].send_mutex);
    RA_PTHREAD_MUTEX_LOCK(&g_ra_hdc_async[phy_id].recv_mutex);
    if (action == SAVE_SNAPSHOT_ACTION_PRE_PROCESSING && g_ra_hdc_async[phy_id].session != NULL) {
        ra_hw_async_set_connect_status(phy_id, HDC_UNCONNECTED);
        g_ra_hdc_async[phy_id].snapshot_session = g_ra_hdc_async[phy_id].session;
        g_ra_hdc_async[phy_id].session = NULL;
    } else if (action == SAVE_SNAPSHOT_ACTION_POST_PROCESSING && g_ra_hdc_async[phy_id].session == NULL) {
        ra_hw_async_set_connect_status(phy_id, HDC_CONNECTED);
        g_ra_hdc_async[phy_id].session = g_ra_hdc_async[phy_id].snapshot_session;
        g_ra_hdc_async[phy_id].snapshot_session = NULL;
    } else {
        hccp_err("duplicate or incorrect order calls are not allowed, phy_id[%u] action[%d]", phy_id, action);
        ret = -EPERM;
    }
    RA_PTHREAD_MUTEX_UNLOCK(&g_ra_hdc_async[phy_id].recv_mutex);
    RA_PTHREAD_MUTEX_UNLOCK(&g_ra_hdc_async[phy_id].send_mutex);
#endif
    return ret;
}

int ra_hdc_async_restore_snapshot(unsigned int phy_id)
{
    int ret = 0;

    if (g_ra_hdc_async[phy_id].thread_status == THREAD_HALT) {
        return 0;
    }

#ifndef HNS_ROCE_LLT
    RA_PTHREAD_MUTEX_LOCK(&g_ra_hdc_async[phy_id].send_mutex);
    RA_PTHREAD_MUTEX_LOCK(&g_ra_hdc_async[phy_id].recv_mutex);
    if (g_ra_hdc_async[phy_id].connect_status != HDC_UNCONNECTED) {
        hccp_err("incorrect order calls are not allowed, phy_id[%u] connect_status[%u]", phy_id,
            g_ra_hdc_async[phy_id].connect_status);
        ret = -EPERM;
    } else {
        g_ra_hdc_async[phy_id].restore_flag = 1;
    }
    RA_PTHREAD_MUTEX_UNLOCK(&g_ra_hdc_async[phy_id].recv_mutex);
    RA_PTHREAD_MUTEX_UNLOCK(&g_ra_hdc_async[phy_id].send_mutex);
#endif
    return ret;
}

STATIC void __attribute__ ((destructor)) ra_hdc_uninit_async(void)
{
    unsigned int phy_id = 0;

    for (phy_id = 0; phy_id < RA_MAX_PHY_ID_NUM; phy_id++) {
        if (g_ra_hdc_async[phy_id].session == NULL || g_ra_hdc_async[phy_id].thread_status != THREAD_RUNNING) {
            continue;
        }

        (void)ra_hdc_deinit_async(phy_id);
    }
}

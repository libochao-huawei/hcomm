/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <sys/prctl.h>
#include "securec.h"
#include "user_log.h"
#include "dl_hal_function.h"
#include "ra_hdc_async.h"
#include "ra_rs_comm.h"
#include "ra_rs_err.h"
#include "ra_adp.h"
#include "ra_adp_pool.h"
#include "ra_adp_async.h"

struct ra_hdc_async_info g_hdc_async[RA_MAX_PHY_ID_NUM] = { 0 };
struct ra_hdc_init_para g_hdc_async_init_para = { 0 };
struct rs_pthread_info g_ra_async_thread_info = { 0 };

int ra_hw_async_init(unsigned int chip_id, pid_t pid)
{
    int ret;

    ret = pthread_mutex_init(&g_hdc_async_init_para.mutex, NULL);
    CHK_PRT_RETURN(ret != 0, hccp_err("g_hdc_async_init_para mutex_init failed ret %d", ret), -ESYSFUNC);

    g_hdc_async_init_para.chip_id = chip_id;
    g_hdc_async_init_para.host_tgid = pid;

    ret = pthread_mutex_init(&g_hdc_async[chip_id].send_mutex, NULL);
    if (ret != 0) {
        hccp_err("send_mutex mutex_init failed ret %d", ret);
        pthread_mutex_destroy(&g_hdc_async_init_para.mutex);
        return -ESYSFUNC;
    }
    ra_hdc_init_op_sec(&g_hdc_async[chip_id].op_sec, BUCKET_DEPTH, true);
    return 0;
}

STATIC int ra_hdc_handle_send_pkt(unsigned int chip_id, void *recv_buf, unsigned int recv_len)
{
    unsigned int close_session = 0;
    void *send_buf = NULL;
    int send_len = 0;
    int ret;

    rs_set_ctx(chip_id);

    ret = ra_handle(&g_hdc_async[chip_id].op_sec, recv_buf, recv_len, (char **)&send_buf, &send_len, &close_session);
    if (ret != 0) {
        hccp_err("ra_handle failed, ret:%d", ret);
        goto out;
    }

    ret = ra_hdc_async_send_pkt(&g_hdc_async[chip_id], chip_id, send_buf, send_len);
    if (ret != 0) {
        hccp_err("send_pkt failed, ret:%d", ret);
        goto err;
    }

err:
    free(send_buf);
    send_buf = NULL;
out:
    return ret;
}

STATIC void ra_async_handle_pkt(unsigned int chip_id, void *recv_buf, unsigned int recv_len)
{
    struct msg_head *recv_msg_head = (struct msg_head *)recv_buf;
    bool close_session = false;

    // should handle RA_RS_HDC_SESSION_CLOSE on recv thread
    if (recv_len < sizeof(struct msg_head) || recv_msg_head->opcode == RA_RS_HDC_SESSION_CLOSE) {
        close_session = true;
    }
    if (close_session) {
        (void)ra_hdc_handle_send_pkt(chip_id, recv_buf, recv_len);
        RA_PTHREAD_MUTEX_LOCK(&g_hdc_async_init_para.mutex);
        g_hdc_async_init_para.connect_status = HDC_UNCONNECTED;
        RA_PTHREAD_MUTEX_UNLOCK(&g_hdc_async_init_para.mutex);
        return;
    }

    // handle other opcode: generate task and process the msg with work thread
    ra_hdc_pool_add_task(g_hdc_async[chip_id].pool, ra_hdc_handle_send_pkt, chip_id, recv_buf, recv_len);
}

STATIC void *ra_async_pthread(void *arg)
{
    unsigned int chip_id = g_hdc_async_init_para.chip_id;
    unsigned int recv_len = 0;
    void *recv_buf = NULL;
    int ret;

    ret = pthread_detach(pthread_self());
    CHK_PRT_RETURN(ret != 0, hccp_err("pthread detach failed ret %d", ret), NULL);

    (void)prctl(PR_SET_NAME, (unsigned long)"hccp_ra_async");

    RA_PTHREAD_MUTEX_LOCK(&g_hdc_async_init_para.mutex);
    g_hdc_async_init_para.thread_status = THREAD_RUNNING;
    RA_PTHREAD_MUTEX_UNLOCK(&g_hdc_async_init_para.mutex);

    rs_get_cur_time(&g_ra_async_thread_info.last_check_time);
    ret = strncpy_s((char *)g_ra_async_thread_info.pthread_name, sizeof(g_ra_async_thread_info.pthread_name),
        "ra_async_thread", strlen("ra_async_thread"));
    CHK_PRT_RETURN(ret != 0, hccp_err("strncpy_s pthread name failed, ret[%d]", ret), NULL);

    hccp_run_info("pthread[%s] is alive!", g_ra_async_thread_info.pthread_name);
    while (1) {
        if (g_hdc_async_init_para.thread_status == THREAD_DESTROYING) {
            break;
        }

        if (g_hdc_async_init_para.connect_status != HDC_CONNECTED) {
            usleep(THREAD_SLEEP_TIME);
            continue;
        }
        rs_heartbeat_alive_print(&g_ra_async_thread_info);
        // recv msg from hdc, alloc recv_buf in ra_async_pthread, free in work_pthread
        ret = ra_hdc_async_recv_pkt(&g_hdc_async[chip_id], chip_id, &recv_buf, &recv_len);
        if (ret != 0) {
            hccp_err("ra_hdc_async_recv_pkt failed, ret:%d chip_id:%u", ret, chip_id);
            break;
        }

        ra_async_handle_pkt(chip_id, recv_buf, recv_len);
    }

    hccp_info("thread [%d] is out, cleaning resources", getpid());
    RA_PTHREAD_MUTEX_LOCK(&g_hdc_async_init_para.mutex);
    g_hdc_async_init_para.thread_status = THREAD_HALT;
    RA_PTHREAD_MUTEX_UNLOCK(&g_hdc_async_init_para.mutex);
    RA_PTHREAD_MUTEX_LOCK(&g_hdc_async[chip_id].send_mutex);
    ra_hdc_close_session(&g_hdc_async[chip_id].hdc_session);
    RA_PTHREAD_MUTEX_UNLOCK(&g_hdc_async[chip_id].send_mutex);
    return NULL;
}

STATIC void ra_hw_async_hdc_init(void *arg)
{
    unsigned int chip_id = g_hdc_async_init_para.chip_id;
    pthread_t tidp;
    int ret;

    ret = pthread_detach(pthread_self());
    if (ret != 0) {
        hccp_err("pthread detach failed chip_id(%u), ret %d", chip_id, ret);
        return;
    }

    (void)prctl(PR_SET_NAME, (unsigned long)"hccp_hw_async");

    hccp_info("chip_id(%u)", chip_id);
    g_hdc_async_init_para.hdc_flag = 1;

    ret = pthread_create(&tidp, NULL, (void *)ra_async_pthread, NULL);
    if (ret != 0) {
        hccp_err("Create pthread failed, chip_id(%u), ret(%d) ", chip_id, ret);
        return;
    }

    while (1) {
        if (g_hdc_async_init_para.connect_status != HDC_UNCONNECTED) {
            usleep(HDC_ACCEPT_SLEEP_TIME);
            continue;
        }
        ret = ra_hdc_session_accept(chip_id, &g_hdc_async[chip_id].hdc_session, (int)g_hdc_async_init_para.host_tgid);
        if (ret != 0) {
            g_hdc_async_init_para.hdc_flag = 0;
            return;
        }
        // should continue to accept: host_tgid != g_hdc_async_init_para.host_tgid
        if (ret == 0 && g_hdc_async[chip_id].hdc_session == NULL) {
            continue;
        }

        RA_PTHREAD_MUTEX_LOCK(&g_hdc_async_init_para.mutex);
        g_hdc_async_init_para.connect_status = HDC_CONNECTED;
        RA_PTHREAD_MUTEX_UNLOCK(&g_hdc_async_init_para.mutex);
        return;
    }
}

void ra_hw_async_deinit(void)
{
   pthread_mutex_destroy(&g_hdc_async[g_hdc_async_init_para.chip_id].send_mutex);
   pthread_mutex_destroy(&g_hdc_async_init_para.mutex);
}

int ra_rs_async_hdc_session_connect(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_async_hdc_connect_data *async_data = NULL;
    int timeout = RA_THREAD_TRY_TIME;
    unsigned int phy_id = 0;
    pthread_t tidp;
    int ret;

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_async_hdc_connect_data), sizeof(struct msg_head), rcv_buf_len,
        op_result);
    async_data = (union op_async_hdc_connect_data *)(in_buf + sizeof(struct msg_head));
    HCCP_CHECK_PARAM_LEN_RET_HOST(async_data->tx_data.queue_size, 0, MAX_POOL_QUEUE_SIZE, op_result);
    HCCP_CHECK_PARAM_LEN_RET_HOST(async_data->tx_data.thread_num, 0, MAX_POOL_THREAD_NUM, op_result);

    phy_id = g_hdc_async_init_para.chip_id;
    g_hdc_async[phy_id].pool = ra_hdc_pool_create(async_data->tx_data.queue_size, async_data->tx_data.thread_num);
    if (g_hdc_async[phy_id].pool == NULL) {
        hccp_err("ra_hdc_pool_create failed, queue_size:%u thread_num:%u phy_id:%u",
            async_data->tx_data.queue_size, async_data->tx_data.thread_num, async_data->tx_data.phy_id);
        *op_result = -ESYSFUNC;
        return 0;
    }

    ret = pthread_create(&tidp, NULL, (void *)ra_hw_async_hdc_init, NULL);
    if (ret != 0) {
        hccp_err("Create pthread failed, ret(%d)", ret);
        *op_result = -ESYSFUNC;
        ra_hdc_pool_destroy(g_hdc_async[phy_id].pool);
        g_hdc_async[phy_id].pool = NULL;
        return 0;
    }

    // will block until time out: RA_THREAD_TRY_TIME * RA_THREAD_SLEEP_TIME us
    while (g_hdc_async_init_para.hdc_flag != 1 && timeout > 0) {
        usleep(RA_THREAD_SLEEP_TIME);
        timeout--;
    }

    if (g_hdc_async_init_para.hdc_flag == 0 || timeout <= 0) {
        hccp_err("HDC server thread create timeout, flag %d, timeout %d", g_hdc_async_init_para.hdc_flag, timeout);
        *op_result = -ESRCH;
        ra_hdc_pool_destroy(g_hdc_async[phy_id].pool);
        g_hdc_async[phy_id].pool = NULL;
        return 0;
    }

    *op_result = 0;
    return 0;
}

int ra_rs_async_hdc_session_close(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    int try_again = HDC_TRY_TIME;
    unsigned int phy_id = 0;

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_async_hdc_close_data), sizeof(struct msg_head), rcv_buf_len,
        op_result);

    RA_PTHREAD_MUTEX_LOCK(&g_hdc_async_init_para.mutex);
    g_hdc_async_init_para.thread_status = THREAD_DESTROYING;
    RA_PTHREAD_MUTEX_UNLOCK(&g_hdc_async_init_para.mutex);

    while ((g_hdc_async_init_para.thread_status != THREAD_HALT) && try_again != 0) {
        usleep(HDC_USLEEP_TIME);
        try_again--;
    }

    if (try_again <= 0) {
        hccp_warn("hdc async message thread quit timeout");
    }

    phy_id = g_hdc_async_init_para.chip_id;
    ra_hdc_pool_destroy(g_hdc_async[phy_id].pool);
    g_hdc_async[phy_id].pool = NULL;
    *op_result = 0;
    return 0;
}

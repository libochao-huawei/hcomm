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

struct ra_hdc_async_info gHdcAsync[RA_MAX_PHY_ID_NUM] = { 0 };
struct ra_hdc_init_para gHdcAsyncInitPara = { 0 };
struct rs_pthread_info gRaAsyncThreadInfo = { 0 };

int RaHwAsyncInit(unsigned int chipId, pid_t pid)
{
    int ret;

    ret = pthread_mutex_init(&gHdcAsyncInitPara.mutex, NULL);
    CHK_PRT_RETURN(ret != 0, hccp_err("g_hdc_async_init_para mutex_init failed ret %d", ret), -ESYSFUNC);

    gHdcAsyncInitPara.chip_id = chipId;
    gHdcAsyncInitPara.host_tgid = pid;

    ret = pthread_mutex_init(&gHdcAsync[chipId].send_mutex, NULL);
    if (ret != 0) {
        hccp_err("send_mutex mutex_init failed ret %d", ret);
        pthread_mutex_destroy(&gHdcAsyncInitPara.mutex);
        return -ESYSFUNC;
    }
    RaHdcInitOpSec(&gHdcAsync[chipId].op_sec, BUCKET_DEPTH, true);
    return 0;
}

STATIC int RaHdcHandleSendPkt(unsigned int chipId, void *recvBuf, unsigned int recvLen)
{
    unsigned int closeSession = 0;
    void *sendBuf = NULL;
    int sendLen = 0;
    int ret;

    RsSetCtx(chipId);

    ret = RaHandle(&gHdcAsync[chipId].op_sec, recvBuf, recvLen, (char **)&sendBuf, &sendLen, &closeSession);
    if (ret != 0) {
        hccp_err("ra_handle failed, ret:%d", ret);
        goto out;
    }

    ret = RaHdcAsyncSendPkt(&gHdcAsync[chipId], chipId, sendBuf, sendLen);
    if (ret != 0) {
        hccp_err("send_pkt failed, ret:%d", ret);
        goto err;
    }

err:
    free(sendBuf);
    sendBuf = NULL;
out:
    return ret;
}

STATIC void RaAsyncHandlePkt(unsigned int chipId, void *recvBuf, unsigned int recvLen)
{
    struct msg_head *recvMsgHead = (struct msg_head *)recvBuf;
    bool closeSession = false;

    // should handle RA_RS_HDC_SESSION_CLOSE on recv thread
    if (recvLen < sizeof(struct msg_head) || recvMsgHead->opcode == RA_RS_HDC_SESSION_CLOSE) {
        closeSession = true;
    }
    if (closeSession) {
        (void)RaHdcHandleSendPkt(chipId, recvBuf, recvLen);
        RA_PTHREAD_MUTEX_LOCK(&gHdcAsyncInitPara.mutex);
        gHdcAsyncInitPara.connect_status = HDC_UNCONNECTED;
        RA_PTHREAD_MUTEX_UNLOCK(&gHdcAsyncInitPara.mutex);
        return;
    }

    // handle other opcode: generate task and process the msg with work thread
    RaHdcPoolAddTask(gHdcAsync[chipId].pool, RaHdcHandleSendPkt, chipId, recvBuf, recvLen);
}

STATIC void *RaAsyncPthread(void *arg)
{
    unsigned int chipId = gHdcAsyncInitPara.chip_id;
    unsigned int recvLen = 0;
    void *recvBuf = NULL;
    int ret;

    ret = pthread_detach(pthread_self());
    CHK_PRT_RETURN(ret != 0, hccp_err("pthread detach failed ret %d", ret), NULL);

    (void)prctl(PR_SET_NAME, (unsigned long)"hccp_ra_async");

    RA_PTHREAD_MUTEX_LOCK(&gHdcAsyncInitPara.mutex);
    gHdcAsyncInitPara.thread_status = THREAD_RUNNING;
    RA_PTHREAD_MUTEX_UNLOCK(&gHdcAsyncInitPara.mutex);

    RsGetCurTime(&gRaAsyncThreadInfo.last_check_time);
    ret = strncpy_s((char *)gRaAsyncThreadInfo.pthread_name, sizeof(gRaAsyncThreadInfo.pthread_name),
        "ra_async_thread", strlen("ra_async_thread"));
    CHK_PRT_RETURN(ret != 0, hccp_err("strncpy_s pthread name failed, ret[%d]", ret), NULL);

    hccp_run_info("pthread[%s] is alive!", gRaAsyncThreadInfo.pthread_name);
    while (1) {
        if (gHdcAsyncInitPara.thread_status == THREAD_DESTROYING) {
            break;
        }

        if (gHdcAsyncInitPara.connect_status != HDC_CONNECTED) {
            usleep(THREAD_SLEEP_TIME);
            continue;
        }
        RsHeartbeatAlivePrint(&gRaAsyncThreadInfo);
        // recv msg from hdc, alloc recv_buf in ra_async_pthread, free in work_pthread
        ret = RaHdcAsyncRecvPkt(&gHdcAsync[chipId], chipId, &recvBuf, &recvLen);
        if (ret != 0) {
            hccp_err("ra_hdc_async_recv_pkt failed, ret:%d chipId:%u", ret, chipId);
            break;
        }

        RaAsyncHandlePkt(chipId, recvBuf, recvLen);
    }

    hccp_info("thread [%d] is out, cleaning resources", getpid());
    RA_PTHREAD_MUTEX_LOCK(&gHdcAsyncInitPara.mutex);
    gHdcAsyncInitPara.thread_status = THREAD_HALT;
    RA_PTHREAD_MUTEX_UNLOCK(&gHdcAsyncInitPara.mutex);
    RA_PTHREAD_MUTEX_LOCK(&gHdcAsync[chipId].send_mutex);
    RaHdcCloseSession(&gHdcAsync[chipId].hdc_session);
    RA_PTHREAD_MUTEX_UNLOCK(&gHdcAsync[chipId].send_mutex);
    return NULL;
}

STATIC void RaHwAsyncHdcInit(void *arg)
{
    unsigned int chipId = gHdcAsyncInitPara.chip_id;
    pthread_t tidp;
    int ret;

    ret = pthread_detach(pthread_self());
    if (ret != 0) {
        hccp_err("pthread detach failed chip_id(%u), ret %d", chipId, ret);
        return;
    }

    (void)prctl(PR_SET_NAME, (unsigned long)"hccp_hw_async");

    hccp_info("chip_id(%u)", chipId);
    gHdcAsyncInitPara.hdc_flag = 1;

    ret = pthread_create(&tidp, NULL, (void *)RaAsyncPthread, NULL);
    if (ret != 0) {
        hccp_err("Create pthread failed, chipId(%u), ret(%d) ", chipId, ret);
        return;
    }

    while (1) {
        if (gHdcAsyncInitPara.connect_status != HDC_UNCONNECTED) {
            usleep(HDC_ACCEPT_SLEEP_TIME);
            continue;
        }
        ret = RaHdcSessionAccept(chipId, &gHdcAsync[chipId].hdc_session, (int)gHdcAsyncInitPara.host_tgid);
        if (ret != 0) {
            gHdcAsyncInitPara.hdc_flag = 0;
            return;
        }
        // should continue to accept: host_tgid != g_hdc_async_init_para.host_tgid
        if (ret == 0 && gHdcAsync[chipId].hdc_session == NULL) {
            continue;
        }

        RA_PTHREAD_MUTEX_LOCK(&gHdcAsyncInitPara.mutex);
        gHdcAsyncInitPara.connect_status = HDC_CONNECTED;
        RA_PTHREAD_MUTEX_UNLOCK(&gHdcAsyncInitPara.mutex);
        return;
    }
}

void RaHwAsyncDeinit(void)
{
   pthread_mutex_destroy(&gHdcAsync[gHdcAsyncInitPara.chip_id].send_mutex);
   pthread_mutex_destroy(&gHdcAsyncInitPara.mutex);
}

int RaRsAsyncHdcSessionConnect(char *inBuf, char *outBuf, int *outLen, int *opResult, int rcvBufLen)
{
    union op_async_hdc_connect_data *asyncData = NULL;
    int timeout = RA_THREAD_TRY_TIME;
    unsigned int phyId = 0;
    pthread_t tidp;
    int ret;

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_async_hdc_connect_data), sizeof(struct msg_head), rcvBufLen,
        opResult);
    asyncData = (union op_async_hdc_connect_data *)(inBuf + sizeof(struct msg_head));
    HCCP_CHECK_PARAM_LEN_RET_HOST(asyncData->tx_data.queue_size, 0, MAX_POOL_QUEUE_SIZE, opResult);
    HCCP_CHECK_PARAM_LEN_RET_HOST(asyncData->tx_data.thread_num, 0, MAX_POOL_THREAD_NUM, opResult);

    phyId = gHdcAsyncInitPara.chip_id;
    gHdcAsync[phyId].pool = RaHdcPoolCreate(asyncData->tx_data.queue_size, asyncData->tx_data.thread_num);
    if (gHdcAsync[phyId].pool == NULL) {
        hccp_err("ra_hdc_pool_create failed, queue_size:%u thread_num:%u phyId:%u",
            asyncData->tx_data.queue_size, asyncData->tx_data.thread_num, asyncData->tx_data.phy_id);
        *opResult = -ESYSFUNC;
        return 0;
    }

    ret = pthread_create(&tidp, NULL, (void *)RaHwAsyncHdcInit, NULL);
    if (ret != 0) {
        hccp_err("Create pthread failed, ret(%d)", ret);
        *opResult = -ESYSFUNC;
        RaHdcPoolDestroy(gHdcAsync[phyId].pool);
        gHdcAsync[phyId].pool = NULL;
        return 0;
    }

    // will block until time out: RA_THREAD_TRY_TIME * RA_THREAD_SLEEP_TIME us
    while (gHdcAsyncInitPara.hdc_flag != 1 && timeout > 0) {
        usleep(RA_THREAD_SLEEP_TIME);
        timeout--;
    }

    if (gHdcAsyncInitPara.hdc_flag == 0 || timeout <= 0) {
        hccp_err("HDC server thread create timeout, flag %d, timeout %d", gHdcAsyncInitPara.hdc_flag, timeout);
        *opResult = -ESRCH;
        RaHdcPoolDestroy(gHdcAsync[phyId].pool);
        gHdcAsync[phyId].pool = NULL;
        return 0;
    }

    *opResult = 0;
    return 0;
}

int RaRsAsyncHdcSessionClose(char *inBuf, char *outBuf, int *outLen, int *opResult, int rcvBufLen)
{
    int tryAgain = HDC_TRY_TIME;
    unsigned int phyId = 0;

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_async_hdc_close_data), sizeof(struct msg_head), rcvBufLen,
        opResult);

    RA_PTHREAD_MUTEX_LOCK(&gHdcAsyncInitPara.mutex);
    gHdcAsyncInitPara.thread_status = THREAD_DESTROYING;
    RA_PTHREAD_MUTEX_UNLOCK(&gHdcAsyncInitPara.mutex);

    while ((gHdcAsyncInitPara.thread_status != THREAD_HALT) && tryAgain != 0) {
        usleep(HDC_USLEEP_TIME);
        tryAgain--;
    }

    if (tryAgain <= 0) {
        hccp_warn("hdc async message thread quit timeout");
    }

    phyId = gHdcAsyncInitPara.chip_id;
    RaHdcPoolDestroy(gHdcAsync[phyId].pool);
    gHdcAsync[phyId].pool = NULL;
    *opResult = 0;
    return 0;
}

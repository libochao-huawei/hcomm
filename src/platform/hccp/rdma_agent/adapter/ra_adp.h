/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef RA_ADP_HW_H
#define RA_ADP_HW_H
#include "ra.h"
#include "ra_hdc.h"
#include "ra_hdc_ping.h"
#include "ra_adp_async.h"
#include "rs.h"

#define BUCKET_DEPTH    (1024 * 2)
#define MAX_CFG_OP_NUM  (1024 * 64)
#define TOKEN_RATE      400 /* 400 tokens per 1ms */
#define HDC_ACCEPT_SLEEP_TIME 100 /* 100us */
#define HCCP_MAX_CHIP_ID 3U
#define ERR_EJ1 "EJ0001"
#define ERR_LEN 6
#define HAVE_OP_RIGHT 1
#define HAVE_NOT_OP_RIGHT 0
#define OP_RIGHT_QUERY_ERR (-1)
#define TGID_INVALID 2
#define RECV_BUF_LEN_INVALID 3
#define HCCP_RUN_CPU_CORE 0

struct ra_hdc_server {
    HDC_SERVER hdc_server;
    HDC_SESSION hdc_session;
    struct ra_hdc_op_sec op_sec;
};

struct ra_hdc_init_para {
    unsigned int chip_id;
    pid_t host_tgid;
    pthread_mutex_t mutex;
    unsigned int hdc_flag;
    unsigned int connect_status;
    unsigned int thread_status;  /* 0: sleep or init; 1: running; 2: destroying */
    char pid_sign[PROCESS_RA_SIGN_LENGTH];
};

struct ra_op_handle {
    unsigned int opcode;
    int (*op_handle)(char *, char *, int *, int *, int);
    unsigned int data_size;
};

#define RA_ADP_ATTRI_VISI_DEF __attribute__ ((visibility ("default")))
RA_ADP_ATTRI_VISI_DEF int HccpInit(unsigned int chipId, pid_t pid, int hdcType, unsigned int whiteListStatus);
RA_ADP_ATTRI_VISI_DEF int HccpDeinit(unsigned int chipId);

#define HCCP_CHECK_PARAM_LEN(data_size, head_size, rcv_buf_len) do { \
    if ((data_size) + (head_size) != (rcv_buf_len)) {         \
        hccp_err("op data size is invalid. data size:[%d] head size:[%d] recv buff len:[%d]",     \
                 data_size, head_size, rcv_buf_len);            \
        return (-EINVAL);         \
    }           \
} while (0)

#if defined(HNS_ROCE_LLT) || defined(DEFINE_HNS_LLT)
#define HCCP_CHECK_PARAM_LEN_RET_HOST(data_size, head_size, rcv_buf_len, pResult) do { \
       \
} while (0)
#else
#define HCCP_CHECK_PARAM_LEN_RET_HOST(data_size, head_size, rcv_buf_len, pResult) do { \
    if ((data_size) + (head_size) > (rcv_buf_len)) {         \
        hccp_err("op data size is invalid. data size:[%d] head size:[%d] recv buff len:[%d]",     \
                 data_size, head_size, rcv_buf_len);            \
        *pResult = -EINVAL;                                       \
        return 0;         \
    }           \
} while (0)
#endif

#define HCCP_CHECK_PARAM_NUM(num, max_num) do { \
    if ((num) > (max_num)) {            \
        hccp_err("conn number is invalid");            \
        return (-EINVAL);         \
    }           \
} while (0)

void RaHdcInitOpSec(struct ra_hdc_op_sec *opSec, unsigned long long tokenNum, bool isAsyncOp);
int RaHdcSessionAccept(unsigned int chipId, HDC_SESSION *session, int initHostTgid);
int RaHdcAsyncRecvPkt(struct ra_hdc_async_info *asyncInfo, unsigned int chipId, void **recvBuf,
    unsigned int *recvLen);
int RaHandle(struct ra_hdc_op_sec *opSec, char *recvBuf, int rcvBufLen, char **sendBuf, int *sndBufLen,
    unsigned int *closeSession);
int RaHdcAsyncSendPkt(struct ra_hdc_async_info *asyncInfo, unsigned int chipId, void *sendBuf,
    unsigned int sendLen);
void RaHdcCloseSession(HDC_SESSION *session);
#endif // RA_ADP_HW_H

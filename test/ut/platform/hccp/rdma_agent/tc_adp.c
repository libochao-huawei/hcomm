/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#define _GNU_SOURCE
#include "tc_adp.h"
#include <stdlib.h>
#include <sched.h>
#include <stdint.h>
#include "ut_dispatch.h"
#include "rs.h"
#include "rs_ping.h"
#include "rs_tlv.h"
#include "ra_adp.h"
#include "ra_adp_tlv.h"
#include "ra_adp_ctx.h"
#include "ra_hdc.h"
#include "ra_hdc_lite.h"
#include "ra_hdc_rdma_notify.h"
#include "ra_hdc_rdma.h"
#include "ra_hdc_socket.h"
#include "ra_hdc_tlv.h"
#include "ra_hdc_ctx.h"
#include "ra_hdc_async_ctx.h"
#include "errno.h"
#undef TOKEN_RATE
#define TOKEN_RATE
extern struct rs_ctx_ops g_ra_rs_ctx_ops;
extern int RecvHandleSendPkt(HDC_SESSION session, unsigned int *chipId);
static int counter = 0;
int stub_recv_handle_send_pkt_0(HDC_SESSION session, unsigned int *close_session)
{
    counter++;
    if (counter <= 1) {
        *close_session = 0;
        return 0;
    } else {
        *close_session = 1;
        return 1;
    }
}

int stub_recv_handle_send_pkt(HDC_SESSION session, unsigned int *close_session)
{
    *close_session = 1;
    return 0;
}

static char* g_test_msg[MAX_TEST_MESSAGE] = {0};
static int g_msg_count = 0;
static int g_current_msg_index = 0;
static int g_accept_times = 0;
static HDC_SESSION g_test_session;
static pid_t g_host_tgid = 0;

DLLEXPORT drvError_t stub_get_msg_buffer(struct drvHdcMsg *msg, int index, char **pBuf, int *pLen)
{
    usleep(10*1000);
    *pBuf = g_test_msg[g_current_msg_index++];
    struct MsgHead* tmsg = (struct MsgHead*)*pBuf;
    *pLen = tmsg->msgDataLen + sizeof(struct MsgHead);
    return 0;
}

char* add_test_msg(unsigned int opcode, unsigned int msg_len)
{
#define MAX_HDC_DATA_SIZE (4096 - 256 + 64)
    char* temp = (char*)calloc(sizeof(char), sizeof(struct MsgHead) + msg_len);
    struct MsgHead* tmsg = (struct MsgHead*)temp;
    int ret;

    tmsg->opcode = opcode;
    tmsg->msgDataLen = msg_len;
    if (opcode != RA_RS_SEND_WRLIST_EXT && opcode != RA_RS_SEND_WRLIST &&
        opcode != RA_RS_WLIST_DEL && opcode != RA_RS_WLIST_ADD &&
        opcode != RA_RS_GET_VNIC_IP_INFOS_V1) {
        ret = (msg_len > MAX_HDC_DATA_SIZE) ? 1 : 0;
        if (ret != 0) {
            printf("%s: opcode:%u, msg_len:%u exceeds %u\n", __func__, opcode, msg_len, MAX_HDC_DATA_SIZE);
        }
        EXPECT_INT_EQ(ret, 0);
    }

    tmsg->hostTgid = g_host_tgid;
    g_test_msg[g_msg_count++] = temp;
    return temp;
}

DLLEXPORT drvError_t stub_accept_session(HDC_SERVER server, HDC_SESSION *session)
{
    while(g_accept_times > 0) {
        *session = &g_test_session;
        -- g_accept_times;
        return 0;
    }
    return -1;
}

void msg_clear()
{

    int i;
    for (i = 0; i < g_msg_count; ++i) {
        free(g_test_msg[i]);
        g_test_msg[i] = NULL;
    }
    g_msg_count = 0;
    g_current_msg_index = 0;
}

void tc_adp_env_init()
{
    mocker_clean();
    msg_clear();
    mocker((stub_fn_t)halHdcRecv, 10, 0);
    mocker((stub_fn_t)halHdcSend, 10, 0);
    mocker_invoke((stub_fn_t)drvHdcGetMsgBuffer, (stub_fn_t)stub_get_msg_buffer, 10);
    mocker_invoke((stub_fn_t)drvHdcSessionAccept, (stub_fn_t)stub_accept_session, 10);
    g_accept_times = 1;
}
void tc_common_test()
{
    unsigned int devid = 0;
    add_test_msg(RA_RS_HDC_SESSION_CLOSE, sizeof(union OpHdcCloseData));
    int ret = HccpInit(devid, g_host_tgid, HDC_SERVICE_TYPE_RDMA, WHITE_LIST_ENABLE);
    EXPECT_INT_EQ(ret , 0);
    sleep(1);
    ret = HccpDeinit(devid);
    EXPECT_INT_EQ(ret, 0);
    msg_clear();
    mocker_clean();
}

void tc_hccp_init_fail()
{
    unsigned int devid = 0;
    pid_t host_tgid = 0;
    mocker_clean();
    mocker((stub_fn_t)sched_setaffinity, 1, -1);
    int ret = HccpInit(devid, host_tgid, HDC_SERVICE_TYPE_RDMA, WHITE_LIST_ENABLE);
    EXPECT_INT_NE(ret, 0);

    mocker_clean();
    mocker((stub_fn_t)sched_setaffinity, 10, 0);
    mocker((stub_fn_t)pthread_create, 1, -1);
    ret = HccpInit(devid, host_tgid, HDC_SERVICE_TYPE_RDMA, WHITE_LIST_ENABLE);
    ret = HccpDeinit(devid);
    EXPECT_INT_EQ(ret, 0);

    mocker_clean();
    mocker((stub_fn_t)pthread_create, 10, 0);
    ret = HccpInit(devid, host_tgid, HDC_SERVICE_TYPE_RDMA, WHITE_LIST_ENABLE);
    EXPECT_INT_NE(ret, 0);
    ret = HccpDeinit(devid);
    EXPECT_INT_EQ(ret, 0);

    mocker_clean();
    mocker((stub_fn_t)pthread_detach, 1, -1);
    mocker((stub_fn_t)RsInit, 1, -1);
    add_test_msg(RA_RS_HDC_SESSION_CLOSE, sizeof(union OpSocketCloseData));
    ret = HccpInit(devid, host_tgid, HDC_SERVICE_TYPE_RDMA, WHITE_LIST_ENABLE);
    EXPECT_INT_EQ(g_current_msg_index, 0);
    EXPECT_INT_NE(ret, 0);
    ret = HccpDeinit(devid);
    EXPECT_INT_EQ(ret, 0);

    mocker_clean();
    msg_clear();
    add_test_msg(RA_RS_HDC_SESSION_CLOSE, sizeof(union OpSocketCloseData));
    ret = HccpInit(RA_MAX_PHY_ID_NUM , host_tgid, HDC_SERVICE_TYPE_RDMA, WHITE_LIST_ENABLE);
    EXPECT_INT_EQ(g_current_msg_index, 0);
    EXPECT_INT_NE(ret, 0);

    mocker_clean();
    msg_clear();
    mocker((stub_fn_t)drvHdcServerCreate, 1, -1);
    add_test_msg(RA_RS_HDC_SESSION_CLOSE, sizeof(union OpSocketCloseData));
    ret = HccpInit(devid , host_tgid, HDC_SERVICE_TYPE_RDMA, WHITE_LIST_ENABLE);
    EXPECT_INT_EQ(g_current_msg_index, 0);
    EXPECT_INT_NE(ret, 0);

    mocker_clean();
    msg_clear();
}

void tc_hccp_deinit_fail()
{
    mocker_clean();
    unsigned int devid = 0;
    int ret = 0;
    mocker((stub_fn_t)RsDeinit, 1, -1);
    ret = HccpDeinit(devid);
    EXPECT_INT_NE(ret, 0);
    mocker_clean();
}

void tc_hccp_init()
{
    tc_adp_env_init();
    tc_common_test();

    msg_clear();
    mocker_clean();
}

void tc_socket_connect()
{
    tc_adp_env_init();
    mocker((stub_fn_t)RsSocketBatchConnect, 1, 0);
    add_test_msg(RA_RS_SOCKET_CONN, sizeof(union OpSocketConnectData));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)RsSocketBatchConnect, 1, -1);
    add_test_msg(RA_RS_SOCKET_CONN, sizeof(union OpSocketConnectData));
    tc_common_test();

}

void tc_socket_close()
{
    tc_adp_env_init();
    mocker((stub_fn_t)RsSocketBatchClose, 1, 0);
    add_test_msg(RA_RS_SOCKET_CLOSE, sizeof(union OpSocketCloseData));
    tc_common_test();

}

void tc_socket_abort()
{
    tc_adp_env_init();
    mocker((stub_fn_t)RsSocketBatchAbort, 1, 0);
    add_test_msg(RA_RS_SOCKET_ABORT, sizeof(union OpSocketConnectData));
    tc_common_test();

    mocker_clean();
    tc_adp_env_init();
    mocker((stub_fn_t)RsSocketBatchAbort, 1, -1);
    add_test_msg(RA_RS_SOCKET_ABORT, sizeof(union OpSocketConnectData));
    tc_common_test();
}

void tc_socket_listen_start()
{
    tc_adp_env_init();
    mocker((stub_fn_t)RsSocketListenStart, 1, 0);
    add_test_msg(RA_RS_SOCKET_LISTEN_START, sizeof(union OpSocketListenData));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)RsSocketListenStart, 1, -1);
    add_test_msg(RA_RS_SOCKET_LISTEN_START, sizeof(union OpSocketListenData));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)RsSocketListenStart, 1, -98);
    add_test_msg(RA_RS_SOCKET_LISTEN_START, sizeof(union OpSocketListenData));
    tc_common_test();
}

void tc_socket_listen_stop()
{
    tc_adp_env_init();
    mocker((stub_fn_t)RsSocketListenStop, 1, 0);
    add_test_msg(RA_RS_SOCKET_LISTEN_STOP, sizeof(union OpSocketListenData));
    tc_common_test();
}

void tc_socket_info()
{
    tc_adp_env_init();
    mocker((stub_fn_t)RsGetSockets, 1, 0);
    add_test_msg(RA_RS_GET_SOCKET, sizeof(union OpSocketInfoData));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)RsGetSockets, 1, -1);
    add_test_msg(RA_RS_GET_SOCKET, sizeof(union OpSocketInfoData));
    tc_common_test();
}

void tc_socket_send()
{
    tc_adp_env_init();
    mocker((stub_fn_t)RsSocketSend, 1, 0);
    add_test_msg(RA_RS_SOCKET_SEND, sizeof(union OpSocketSendData));
    tc_common_test();
}

void tc_socket_recv()
{
    tc_adp_env_init();
    mocker((stub_fn_t)RsSocketRecv, 1, 0);
    add_test_msg(RA_RS_SOCKET_RECV, sizeof(union OpSocketRecvData));
    tc_common_test();
}

void tc_socket_init()
{
    tc_adp_env_init();
    mocker((stub_fn_t)RsSocketInit, 1, 0);
    add_test_msg(RA_RS_SOCKET_INIT, sizeof(union OpSocketInitData));
    tc_common_test();
}

void tc_socket_deinit()
{
    tc_adp_env_init();
    mocker((stub_fn_t)RsSocketDeinit, 1, 0);
    add_test_msg(RA_RS_SOCKET_DEINIT, sizeof(union OpSocketDeinitData));
    tc_common_test();
}

void tc_set_tsqp_depth()
{
    tc_adp_env_init();
    mocker((stub_fn_t)RsSetTsqpDepth, 1, 0);
    add_test_msg(RA_RS_SET_TSQP_DEPTH, sizeof(union OpSetTsqpDepthData));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)RsSetTsqpDepth, 1, -1);
    add_test_msg(RA_RS_SET_TSQP_DEPTH, sizeof(union OpSetTsqpDepthData));
    tc_common_test();
}

void tc_get_tsqp_depth()
{
    tc_adp_env_init();
    mocker((stub_fn_t)RsGetTsqpDepth, 1, 0);
    add_test_msg(RA_RS_GET_TSQP_DEPTH, sizeof(union OpGetTsqpDepthData));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)RsGetTsqpDepth, 1, -1);
    add_test_msg(RA_RS_GET_TSQP_DEPTH, sizeof(union OpGetTsqpDepthData));
    tc_common_test();
}

void tc_qp_create()
{
    tc_adp_env_init();
    mocker((stub_fn_t)RsQpCreate, 1, 0);
    add_test_msg(RA_RS_QP_CREATE, sizeof(union OpQpCreateData));
    mocker((stub_fn_t)RsQpCreateWithAttrs, 10, 0);
    add_test_msg(RA_RS_QP_CREATE_WITH_ATTRS, sizeof(union OpQpCreateWithAttrsData));
    add_test_msg(RA_RS_AI_QP_CREATE, sizeof(union OpAiQpCreateData));
    add_test_msg(RA_RS_AI_QP_CREATE_WITH_ATTRS, sizeof(union OpAiQpCreateWithAttrsData));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)RsQpCreate, 1, -1);
    add_test_msg(RA_RS_QP_CREATE, sizeof(union OpQpCreateData));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)RsQpCreateWithAttrs, 10, -1);
    add_test_msg(RA_RS_QP_CREATE_WITH_ATTRS, sizeof(union OpQpCreateWithAttrsData));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)RsQpCreate, 10, -1);
    add_test_msg(RA_RS_TYPICAL_QP_CREATE, sizeof(union OpTypicalQpCreateData));
    tc_common_test();
}

void tc_qp_destroy()
{
    tc_adp_env_init();
    mocker((stub_fn_t)RsQpDestroy, 1, 0);
    add_test_msg(RA_RS_QP_DESTROY, sizeof(union OpQpDestroyData));
    tc_common_test();
}

void tc_qp_status()
{
    tc_adp_env_init();
    mocker((stub_fn_t)RsGetQpStatus, 1, 0);
    add_test_msg(RA_RS_QP_STATUS, sizeof(union OpQpStatusData));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)RsGetQpStatus, 1, -1);
    add_test_msg(RA_RS_QP_STATUS, sizeof(union OpQpStatusData));
    tc_common_test();
}

void tc_qp_info()
{
    tc_adp_env_init();
    mocker((stub_fn_t)RsGetQpStatus, 1, 0);
    add_test_msg(RA_RS_QP_INFO, sizeof(union OpQpInfoData));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)RsGetQpStatus, 1, -1);
    add_test_msg(RA_RS_QP_INFO, sizeof(union OpQpInfoData));
    tc_common_test();
}

void tc_qp_connect()
{
    tc_adp_env_init();
    mocker((stub_fn_t)RsQpConnectAsync, 1, 0);
    add_test_msg(RA_RS_QP_CONNECT, sizeof(union OpQpConnectData));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)RsTypicalQpModify, 1, -1);
    add_test_msg(RA_RS_TYPICAL_QP_MODIFY, sizeof(union OpTypicalQpModifyData));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)RsQpBatchModify, 1, -1);
    add_test_msg(RA_RS_QP_BATCH_MODIFY, sizeof(union OpQpBatchModifyData));
    tc_common_test();
}

void tc_mr_reg()
{
    tc_adp_env_init();
    mocker((stub_fn_t)RsMrReg, 1, 0);
    add_test_msg(RA_RS_MR_REG, sizeof(union OpMrRegData));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)RsTypicalRegisterMrV1, 1, 0);
    add_test_msg(RA_RS_TYPICAL_MR_REG_V1, sizeof(union OpTypicalMrRegData));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)RsTypicalRegisterMr, 1, 0);
    add_test_msg(RA_RS_TYPICAL_MR_REG, sizeof(union OpTypicalMrRegData));
    tc_common_test();
}

void tc_mr_dreg()
{
    tc_adp_env_init();
    mocker((stub_fn_t)RsMrDereg, 1, 0);
    add_test_msg(RA_RS_MR_DEREG, sizeof(union OpMrDeregData));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)RsTypicalDeregisterMr, 1, 0);
    add_test_msg(RA_RS_TYPICAL_MR_DEREG, sizeof(union OpTypicalMrDeregData));
    tc_common_test();
}

void tc_send_wr()
{
    tc_adp_env_init();
    mocker((stub_fn_t)RsSendWr, 1, 0);
    add_test_msg(RA_RS_SEND_WR, sizeof(union OpSendWrData));
    tc_common_test();
}

extern memcpy_s(void *dest, size_t destMax, const void *src, size_t count);
void tc_send_wrlist()
{
    tc_adp_env_init();
    mocker((stub_fn_t)RsSendWrlist, 1, 0);
    mocker((stub_fn_t)memcpy_s, 1, 0);
    add_test_msg(RA_RS_SEND_WRLIST, sizeof(union OpSendWrlistData));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)RsSendWrlist, 1, -1);
    mocker((stub_fn_t)memcpy_s, 1, 0);
    add_test_msg(RA_RS_SEND_WRLIST, sizeof(union OpSendWrlistData));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)RsSendWrlist, 1, -ENOENT);
    mocker((stub_fn_t)memcpy_s, 1, 0);
    add_test_msg(RA_RS_SEND_WRLIST, sizeof(union OpSendWrlistData));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)RsSendWrlist, 1, 0);
    mocker((stub_fn_t)memcpy_s, 1, -1);
    add_test_msg(RA_RS_SEND_WRLIST, sizeof(union OpSendWrlistData));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)RsSendWrlist, 1, 0);
    mocker((stub_fn_t)memcpy_s, 1, -1);
    add_test_msg(RA_RS_SEND_WRLIST_EXT, sizeof(union OpSendWrlistDataExt));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)RsSendWrlist, 1, 0);
    mocker((stub_fn_t)memcpy_s, 1, -1);
    add_test_msg(RA_RS_SEND_WRLIST_V2, sizeof(union OpSendWrlistDataV2));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)RsSendWrlist, 1, 0);
    mocker((stub_fn_t)memcpy_s, 1, -1);
    add_test_msg(RA_RS_SEND_WRLIST_EXT_V2, sizeof(union OpSendWrlistDataExtV2));
    tc_common_test();
}

void tc_rdev_init()
{
    tc_adp_env_init();
    mocker((stub_fn_t)RsRdevInit, 1, 0);
    add_test_msg(RA_RS_RDEV_INIT, sizeof(union OpRdevInitData));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)RsRdevInit, 1, -1);
    add_test_msg(RA_RS_RDEV_INIT, sizeof(union OpRdevInitData));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)RsRdevInitWithBackup, 1, 0);
    add_test_msg(RA_RS_RDEV_INIT_WITH_BACKUP, sizeof(union OpRdevInitWithBackupData));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)RsRdevInitWithBackup, 1, -1);
    add_test_msg(RA_RS_RDEV_INIT_WITH_BACKUP, sizeof(union OpRdevInitWithBackupData));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)RsRdevGetPortStatus, 1, -1);
    add_test_msg(RA_RS_RDEV_GET_PORT_STATUS, sizeof(union OpRdevGetPortStatusData));
    tc_common_test();
}

void tc_rdev_deinit()
{
    tc_adp_env_init();
    mocker((stub_fn_t)RsRdevDeinit, 1, 0);
    add_test_msg(RA_RS_RDEV_DEINIT, sizeof(union OpRdevDeinitData));
    tc_common_test();
}
void tc_get_notify_ba()
{
    tc_adp_env_init();
    mocker((stub_fn_t)RsGetNotifyMrInfo, 1, 0);
    add_test_msg(RA_RS_GET_NOTIFY_BA, sizeof(union OpGetNotifyBaData));
    tc_common_test();
}
void tc_set_pid()
{
    tc_adp_env_init();
    mocker((stub_fn_t)RsSetHostPid, 1, 0);
    add_test_msg(RA_RS_SET_PID, sizeof(union OpSetPidData));
    tc_common_test();
}
void tc_get_vnic_ip()
{
    tc_adp_env_init();
    mocker((stub_fn_t)RsGetVnicIp, 1, 0);
    add_test_msg(RA_RS_GET_VNIC_IP, sizeof(union OpGetVnicIpData));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)RsGetVnicIp, 1, -1);
    add_test_msg(RA_RS_GET_VNIC_IP, sizeof(union OpGetVnicIpData));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)RsGetVnicIpInfos, 1, -1);
    add_test_msg(RA_RS_GET_VNIC_IP_INFOS_V1, sizeof(union OpGetVnicIpInfosDataV1));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)RsGetVnicIpInfos, 1, -1);
    add_test_msg(RA_RS_GET_VNIC_IP_INFOS, sizeof(union OpGetVnicIpInfosData));
    tc_common_test();
}
void tc_socket_white_list_add()
{
    tc_adp_env_init();
    mocker((stub_fn_t)RsSocketWhiteListAdd, 1, 0);
    add_test_msg(RA_RS_WLIST_ADD, sizeof(union OpWlistData));
    tc_common_test();

    mocker_clean();
    tc_adp_env_init();
    mocker((stub_fn_t)RsSocketWhiteListAdd, 1, -1);
    add_test_msg(RA_RS_WLIST_ADD, sizeof(union OpWlistData));
    tc_common_test();
}
void tc_socket_white_list_del()
{
    tc_adp_env_init();
    mocker((stub_fn_t)RsSocketWhiteListDel, 1, 0);
    add_test_msg(RA_RS_WLIST_DEL, sizeof(union OpWlistData));
    tc_common_test();

    mocker_clean();
    tc_adp_env_init();
    mocker((stub_fn_t)RsSocketWhiteListDel, 1, -1);
    add_test_msg(RA_RS_WLIST_DEL, sizeof(union OpWlistData));
    tc_common_test();
}
void tc_get_ifaddrs()
{
    tc_adp_env_init();
    mocker((stub_fn_t)RsGetIfaddrs, 1, 0);
    char* databuf = add_test_msg(RA_RS_GET_IFADDRS, sizeof(union OpIfaddrData));
    union OpIfaddrData *ifaddr_data = (union OpIfaddrData *)(databuf + sizeof(struct MsgHead));

    databuf = add_test_msg(RA_RS_GET_IFADDRS, sizeof(union OpIfaddrData));
    ifaddr_data = (union OpIfaddrData *)(databuf + sizeof(struct MsgHead));
    ifaddr_data->txData.num = 1;
    tc_common_test();
}

void tc_get_ifaddrs_v2()
{
    tc_adp_env_init();
    mocker((stub_fn_t)RsGetIfaddrsV2, 10, 0);
    char* databuf = add_test_msg(RA_RS_GET_IFADDRS_V2, sizeof(union OpIfaddrDataV2));
    union OpIfaddrDataV2 *ifaddr_data = (union OpIfaddrDataV2 *)(databuf + sizeof(struct MsgHead));
    databuf = add_test_msg(RA_RS_GET_IFADDRS_V2, sizeof(union OpIfaddrDataV2));

    ifaddr_data->txData.num = 1;
    databuf = add_test_msg(RA_RS_GET_IFADDRS_V2, sizeof(union OpIfaddrDataV2));
    tc_common_test();
}

void tc_get_ifnum()
{
    tc_adp_env_init();
    mocker((stub_fn_t)RsGetIfnum, 2, 0);
    char* databuf = add_test_msg(RA_RS_GET_IFNUM, sizeof(union OpIfnumData));
    union OpIfnumData *ifnum_data = (union OpIfnumData *)(databuf + sizeof(struct MsgHead));

    databuf = add_test_msg(RA_RS_GET_IFNUM, sizeof(union OpIfnumData));
    ifnum_data = (union OpIfnumData *)(databuf + sizeof(struct MsgHead));
    ifnum_data->txData.num = 1;
    tc_common_test();
}

void tc_get_interface_version()
{
    tc_adp_env_init();
    mocker((stub_fn_t)RsGetInterfaceVersion, 1, 0);

    char* databuf = add_test_msg(RA_RS_GET_INTERFACE_VERSION, sizeof(union OpGetVersionData));
    union OpGetVersionData *version_info = (union OpGetVersionData *)(databuf + sizeof(struct MsgHead));

    version_info->txData.opcode = 0;
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)RsGetInterfaceVersion, 1, -1);
    databuf = add_test_msg(RA_RS_GET_INTERFACE_VERSION, sizeof(union OpGetVersionData));
    version_info = (union OpGetVersionData *)(databuf + sizeof(struct MsgHead));
    version_info->txData.opcode = 0;
    tc_common_test();
}

void tc_set_notify_cfg()
{
    int result;
    mocker_clean();
    unsigned int size = sizeof(union OpNotifyCfgSetData) + sizeof(struct MsgHead);
    char *in_buf = calloc(1, size);
    union OpNotifyCfgSetData set_notify_ba_data = {0};
    memcpy(in_buf + sizeof(struct MsgHead), &set_notify_ba_data, sizeof(union OpNotifyCfgSetData));
    RaRsNotifyCfgSet(in_buf, NULL, NULL, &result, 1);

    free(in_buf);
    in_buf = NULL;

    tc_adp_env_init();
    mocker((stub_fn_t)RsNotifyCfgSet, 1, 0);
    add_test_msg(RA_RS_NOTIFY_CFG_SET, sizeof(union OpNotifyCfgSetData));
    tc_common_test();
}

void tc_get_notify_cfg()
{
    int result;
    mocker_clean();
    unsigned int size = sizeof(union OpNotifyCfgGetData) + sizeof(struct MsgHead);
    char *in_buf = calloc(1, size);
    char *out_buf = calloc(1, size);
    union OpNotifyCfgGetData get_notify_ba_data = {0};
    memcpy(in_buf + sizeof(struct MsgHead), &get_notify_ba_data, sizeof(union OpNotifyCfgGetData));
    memcpy(out_buf + sizeof(struct MsgHead), &get_notify_ba_data, sizeof(union OpNotifyCfgGetData));
    RaRsNotifyCfgGet(in_buf, out_buf, NULL, &result, 1);

    free(out_buf);
    out_buf = NULL;
    free(in_buf);
    in_buf = NULL;

    tc_adp_env_init();
    mocker((stub_fn_t)RsNotifyCfgGet, 1, 0);
    add_test_msg(RA_RS_NOTIFY_CFG_GET, sizeof(union OpNotifyCfgGetData));
    tc_common_test();
}

void tc_ra_rs_send_wr_list_v2()
{
    union OpSendWrlistDataV2 send_wrlist;

    send_wrlist.txData.phyId = 0;
    send_wrlist.txData.rdevIndex = 0;
    send_wrlist.txData.qpn = 0;
    send_wrlist.txData.sendNum = 1;
    send_wrlist.txData.wrlist[0].op = 0;
    send_wrlist.txData.wrlist[0].sendFlags = 0;
    send_wrlist.txData.wrlist[0].dstAddr = 0;
    send_wrlist.txData.wrlist[0].memList.addr = 0;
    send_wrlist.txData.wrlist[0].memList.len = 0;
    send_wrlist.txData.wrlist[0].memList.lkey = 0;

    union OpSendWrlistDataV2 send_wrlist_out;

    char* in_buf = (char*)(&send_wrlist);
    char* out_buf = (char*)(&send_wrlist_out);

    int out_len;
    int op_result;
    int rcv_buf_len = 0;

    RaRsSendWrListV2(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
}

void tc_ra_rs_send_wr_list()
{
    union OpSendWrlistData send_wrlist;

    send_wrlist.txData.phyId = 0;
    send_wrlist.txData.rdevIndex = 0;
    send_wrlist.txData.qpn = 0;
    send_wrlist.txData.sendNum = 1;
    send_wrlist.txData.wrlist[0].op = 0;
    send_wrlist.txData.wrlist[0].sendFlags = 0;
    send_wrlist.txData.wrlist[0].dstAddr = 0;
    send_wrlist.txData.wrlist[0].memList.addr = 0;
    send_wrlist.txData.wrlist[0].memList.len = 0;
    send_wrlist.txData.wrlist[0].memList.lkey = 0;

    union OpSendWrlistData send_wrlist_out;

    char* in_buf = (char*)(&send_wrlist);
    char* out_buf = (char*)(&send_wrlist_out);

    int out_len;
    int op_result;
    int rcv_buf_len = 0;

    RaRsSendWrList(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);

    tc_ra_rs_send_wr_list_v2();
}

void tc_ra_rs_send_wr_list_ext_v2()
{
    union OpSendWrlistDataExtV2 send_wrlist;

    send_wrlist.txData.phyId = 0;
    send_wrlist.txData.rdevIndex = 0;
    send_wrlist.txData.qpn = 0;
    send_wrlist.txData.sendNum = 1;
    send_wrlist.txData.wrlist[0].op = 0;
    send_wrlist.txData.wrlist[0].sendFlags = 0;
    send_wrlist.txData.wrlist[0].dstAddr = 0;
    send_wrlist.txData.wrlist[0].memList.addr = 0;
    send_wrlist.txData.wrlist[0].memList.len = 0;
    send_wrlist.txData.wrlist[0].memList.lkey = 0;
    send_wrlist.txData.wrlist[0].aux.dataType = 0;
    send_wrlist.txData.wrlist[0].aux.reduceType = 0;
    send_wrlist.txData.wrlist[0].aux.notifyOffset = 0;

    union OpSendWrlistDataExtV2 send_wrlist_out;

    char* in_buf = (char*)(&send_wrlist);
    char* out_buf = (char*)(&send_wrlist_out);

    int out_len;
    int op_result;
    int rcv_buf_len = 0;

    RaRsSendWrListExtV2(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
}

void tc_ra_rs_send_wr_list_ext()
{
    union OpSendWrlistDataExt send_wrlist;

    send_wrlist.txData.phyId = 0;
    send_wrlist.txData.rdevIndex = 0;
    send_wrlist.txData.qpn = 0;
    send_wrlist.txData.sendNum = 1;
    send_wrlist.txData.wrlist[0].op = 0;
    send_wrlist.txData.wrlist[0].sendFlags = 0;
    send_wrlist.txData.wrlist[0].dstAddr = 0;
    send_wrlist.txData.wrlist[0].memList.addr = 0;
    send_wrlist.txData.wrlist[0].memList.len = 0;
    send_wrlist.txData.wrlist[0].memList.lkey = 0;
    send_wrlist.txData.wrlist[0].aux.dataType = 0;
    send_wrlist.txData.wrlist[0].aux.reduceType = 0;
    send_wrlist.txData.wrlist[0].aux.notifyOffset = 0;

    union OpSendWrlistDataExt send_wrlist_out;

    char* in_buf = (char*)(&send_wrlist);
    char* out_buf = (char*)(&send_wrlist_out);

    int out_len;
    int op_result;
    int rcv_buf_len = 0;

    RaRsSendWrListExt(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);

    tc_ra_rs_send_wr_list_ext_v2();
}

void tc_ra_rs_send_normal_wrlist()
{
    tc_adp_env_init();
    mocker((stub_fn_t)RsSendWrlist, 1, 0);
    add_test_msg(RA_RS_SEND_NORMAL_WRLIST, sizeof(union OpSendNormalWrlistData));
    tc_common_test();
}

void tc_ra_rs_set_qp_attr_qos()
{
    tc_adp_env_init();
    mocker((stub_fn_t)RsSetQpAttrQos, 1, 0);
    add_test_msg(RA_RS_SET_QP_ATTR_QOS, sizeof(union OpSetQpAttrQosData));
    tc_common_test();
}

void tc_ra_rs_set_qp_attr_timeout()
{
    tc_adp_env_init();
    mocker((stub_fn_t)RsSetQpAttrTimeout, 1, 0);
    add_test_msg(RA_RS_SET_QP_ATTR_TIMEOUT, sizeof(union OpSetQpAttrTimeoutData));
    tc_common_test();
}

void tc_ra_rs_set_qp_attr_retry_cnt()
{
    tc_adp_env_init();
    mocker((stub_fn_t)RsSetQpAttrRetryCnt, 1, 0);
    add_test_msg(RA_RS_SET_QP_ATTR_RETRY_CNT, sizeof(union OpSetQpAttrRetryCntData));
    tc_common_test();
}

void tc_ra_rs_get_cqe_err_info()
{
    tc_adp_env_init();
    mocker((stub_fn_t)RsGetCqeErrInfo, 1, 0);
    add_test_msg(RA_RS_GET_CQE_ERR_INFO, sizeof(union OpGetCqeErrInfoData));
    tc_common_test();
}

void tc_ra_rs_get_cqe_err_info_num()
{
    tc_adp_env_init();
    mocker((stub_fn_t)RsGetCqeErrInfoNum, 1, 0);
    add_test_msg(RA_RS_GET_CQE_ERR_INFO_NUM, sizeof(union OpGetCqeErrInfoListData));
    tc_common_test();
}

void tc_ra_rs_get_cqe_err_info_list()
{
    tc_adp_env_init();
    mocker((stub_fn_t)RsGetCqeErrInfoList, 1, 0);
    add_test_msg(RA_RS_GET_CQE_ERR_INFO_LIST, sizeof(union OpGetCqeErrInfoListData));
    tc_common_test();
}

void tc_ra_rs_get_lite_support()
{
    tc_adp_env_init();
    add_test_msg(RA_RS_GET_LITE_SUPPORT, sizeof(union OpLiteSupportData));
    tc_common_test();
}

void tc_ra_RsGetLiteRdevCap()
{
    tc_adp_env_init();
    mocker((stub_fn_t)RsGetLiteRdevCap, 1, 0);
    add_test_msg(RA_RS_GET_LITE_RDEV_CAP, sizeof(union OpLiteRdevCapData));
    tc_common_test();
}

void tc_ra_rs_get_lite_qp_cq_attr()
{
    tc_adp_env_init();
    mocker((stub_fn_t)RsGetLiteQpCqAttr, 1, 0);
    add_test_msg(RA_RS_GET_LITE_QP_CQ_ATTR, sizeof(union OpLiteQpCqAttrData));
    tc_common_test();
}

void tc_ra_rs_get_lite_connected_info()
{
    tc_adp_env_init();
    mocker((stub_fn_t)RsGetLiteConnectedInfo, 1, 0);
    add_test_msg(RA_RS_GET_LITE_CONNECTED_INFO, sizeof(union OpLiteConnectedInfoData));
    tc_common_test();
}

void tc_ra_rs_socket_white_list_v2()
{
    tc_adp_env_init();
    mocker((stub_fn_t)RsSocketWhiteListAdd, 1, 0);
    mocker((stub_fn_t)RsSocketWhiteListDel, 1, 0);
    add_test_msg(RA_RS_WLIST_ADD_V2, sizeof(union OpWlistDataV2));
    add_test_msg(RA_RS_WLIST_DEL_V2, sizeof(union OpWlistDataV2));
    tc_common_test();
}

void tc_ra_rs_socket_credit_add()
{
    tc_adp_env_init();
    mocker((stub_fn_t)RsSocketAcceptCreditAdd, 1, 0);
    add_test_msg(RA_RS_ACCEPT_CREDIT_ADD, sizeof(union OpAcceptCreditData));
    tc_common_test();
}

void tc_ra_rs_get_lite_mem_attr()
{
    tc_adp_env_init();
    mocker((stub_fn_t)RsGetLiteMemAttr, 1, 0);
    add_test_msg(RA_RS_GET_LITE_MEM_ATTR, sizeof(union OpLiteMemAttrData));
    tc_common_test();
}

void tc_ra_rs_ping_init()
{
    tc_adp_env_init();
    mocker((stub_fn_t)RsPingInit, 1, 0);
    add_test_msg(RA_RS_PING_INIT, sizeof(union OpPingInitData));
    tc_common_test();
    tc_adp_env_init();
    mocker((stub_fn_t)RsPingInit, 1, 12);
    add_test_msg(RA_RS_PING_INIT, sizeof(union OpPingInitData));
    tc_common_test();
}

void tc_ra_rs_ping_target_add()
{
    tc_adp_env_init();
    mocker((stub_fn_t)RsPingTargetAdd, 1, 0);
    add_test_msg(RA_RS_PING_ADD, sizeof(union OpPingAddData));
    tc_common_test();
    tc_adp_env_init();
    mocker((stub_fn_t)RsPingTargetAdd, 1, 12);
    add_test_msg(RA_RS_PING_ADD, sizeof(union OpPingAddData));
    tc_common_test();
}

void tc_ra_rs_ping_task_start()
{
    tc_adp_env_init();
    mocker((stub_fn_t)RsPingTaskStart, 1, 0);
    add_test_msg(RA_RS_PING_START, sizeof(union OpPingStartData));
    tc_common_test();
    tc_adp_env_init();
    mocker((stub_fn_t)RsPingTaskStart, 1, 12);
    add_test_msg(RA_RS_PING_START, sizeof(union OpPingStartData));
    tc_common_test();
}

void tc_ra_rs_ping_get_results()
{
    tc_adp_env_init();
    mocker((stub_fn_t)RsPingGetResults, 1, 0);
    add_test_msg(RA_RS_PING_GET_RESULTS, sizeof(union OpPingResultsData));
    tc_common_test();
    tc_adp_env_init();
    mocker((stub_fn_t)RsPingGetResults, 1, 12);
    add_test_msg(RA_RS_PING_GET_RESULTS, sizeof(union OpPingResultsData));
    tc_common_test();
    tc_adp_env_init();
    mocker((stub_fn_t)RsPingGetResults, 1, -11);
    add_test_msg(RA_RS_PING_GET_RESULTS, sizeof(union OpPingResultsData));
    tc_common_test();
}

void tc_ra_rs_ping_task_stop()
{
    tc_adp_env_init();
    mocker((stub_fn_t)RsPingTaskStop, 1, 0);
    add_test_msg(RA_RS_PING_STOP, sizeof(union OpPingStopData));
    tc_common_test();
    tc_adp_env_init();
    mocker((stub_fn_t)RsPingTaskStop, 1, 12);
    add_test_msg(RA_RS_PING_STOP, sizeof(union OpPingStopData));
    tc_common_test();
}

void tc_ra_rs_ping_target_del()
{
    tc_adp_env_init();
    mocker((stub_fn_t)RsPingTargetDel, 1, 0);
    add_test_msg(RA_RS_PING_DEL, sizeof(union OpPingDelData));
    tc_common_test();
    tc_adp_env_init();
    mocker((stub_fn_t)RsPingTargetDel, 1, 12);
    add_test_msg(RA_RS_PING_DEL, sizeof(union OpPingDelData));
    tc_common_test();
}

void tc_ra_rs_ping_deinit()
{
    tc_adp_env_init();
    mocker((stub_fn_t)RsPingDeinit, 1, 0);
    add_test_msg(RA_RS_PING_DEINIT, sizeof(union OpPingDeinitData));
    tc_common_test();
    tc_adp_env_init();
    mocker((stub_fn_t)RsPingDeinit, 1, 12);
    add_test_msg(RA_RS_PING_DEINIT, sizeof(union OpPingDeinitData));
    tc_common_test();
}

void tc_tlv_init()
{
    tc_adp_env_init();
    mocker((stub_fn_t)RsTlvInit, 1, 0);
    add_test_msg(RA_RS_TLV_INIT, sizeof(union OpTlvInitData));
    tc_common_test();
}

void tc_tlv_deinit()
{
    tc_adp_env_init();
    mocker((stub_fn_t)RsTlvDeinit, 1, 0);
    add_test_msg(RA_RS_TLV_DEINIT, sizeof(union OpTlvDeinitData));
    tc_common_test();
}

void tc_tlv_request()
{
    tc_adp_env_init();
    mocker((stub_fn_t)RaRsTlvRequest, 1, 0);
    add_test_msg(RA_RS_TLV_REQUEST, sizeof(union OpTlvRequestData));
    tc_common_test();
}

void tc_ra_rs_remap_mr()
{
    mocker_clean();
    tc_adp_env_init();
    mocker((stub_fn_t)RsRemapMr, 1, 0);
    add_test_msg(RA_RS_REMAP_MR, sizeof(union OpRemapMrData));
    tc_common_test();
    mocker_clean();

    tc_adp_env_init();
    mocker((stub_fn_t)RsRemapMr, 1, 1);
    add_test_msg(RA_RS_REMAP_MR, sizeof(union OpRemapMrData));
    tc_common_test();
    mocker_clean();
}

void tc_ra_rs_test_ctx_ops()
{
    tc_adp_env_init();
    mocker((stub_fn_t)g_ra_rs_ctx_ops.get_dev_eid_info_num, 1, 0);
    add_test_msg(RA_RS_GET_DEV_EID_INFO_NUM, sizeof(union op_get_dev_eid_info_num_data));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)g_ra_rs_ctx_ops.get_dev_eid_info_list, 1, 0);
    add_test_msg(RA_RS_GET_DEV_EID_INFO_LIST, sizeof(union op_get_dev_eid_info_list_data));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)g_ra_rs_ctx_ops.ctx_init, 1, 0);
    add_test_msg(RA_RS_CTX_INIT, sizeof(union op_ctx_init_data));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)g_ra_rs_ctx_ops.ctx_deinit, 1, 0);
    add_test_msg(RA_RS_CTX_DEINIT, sizeof(union op_ctx_deinit_data));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)g_ra_rs_ctx_ops.ctx_lmem_reg, 1, 0);
    add_test_msg(RA_RS_LMEM_REG, sizeof(union op_lmem_reg_info_data));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)g_ra_rs_ctx_ops.ctx_lmem_unreg, 1, 0);
    add_test_msg(RA_RS_LMEM_UNREG, sizeof(union op_lmem_unreg_info_data));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)g_ra_rs_ctx_ops.ctx_rmem_import, 1, 0);
    add_test_msg(RA_RS_RMEM_IMPORT, sizeof(union op_rmem_import_info_data));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)g_ra_rs_ctx_ops.ctx_rmem_unimport, 1, 0);
    add_test_msg(RA_RS_RMEM_UNIMPORT, sizeof(union op_rmem_unimport_info_data));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)g_ra_rs_ctx_ops.ctx_chan_create, 1, 0);
    add_test_msg(RA_RS_CTX_CHAN_CREATE, sizeof(union op_ctx_chan_create_data));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)g_ra_rs_ctx_ops.ctx_chan_destroy, 1, 0);
    add_test_msg(RA_RS_CTX_CHAN_DESTROY, sizeof(union op_ctx_chan_destroy_data));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)g_ra_rs_ctx_ops.ctx_cq_create, 1, 0);
    add_test_msg(RA_RS_CTX_CQ_CREATE, sizeof(union op_ctx_cq_create_data));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)g_ra_rs_ctx_ops.ctx_cq_destroy, 1, 0);
    add_test_msg(RA_RS_CTX_CQ_DESTROY, sizeof(union op_ctx_cq_destroy_data));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)g_ra_rs_ctx_ops.ctx_qp_create, 1, 0);
    add_test_msg(RA_RS_CTX_QP_CREATE, sizeof(union op_ctx_qp_create_data));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)g_ra_rs_ctx_ops.ctx_qp_destroy, 1, 0);
    add_test_msg(RA_RS_CTX_QP_DESTROY, sizeof(union op_ctx_qp_destroy_data));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)g_ra_rs_ctx_ops.ctx_qp_import, 1, 0);
    add_test_msg(RA_RS_CTX_QP_IMPORT, sizeof(union op_ctx_qp_import_data));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)g_ra_rs_ctx_ops.ctx_qp_unimport, 1, 0);
    add_test_msg(RA_RS_CTX_QP_UNIMPORT, sizeof(union op_ctx_qp_unimport_data));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)g_ra_rs_ctx_ops.ctx_qp_bind, 1, 0);
    add_test_msg(RA_RS_CTX_QP_BIND, sizeof(union op_ctx_qp_bind_data));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)g_ra_rs_ctx_ops.ctx_qp_unbind, 1, 0);
    add_test_msg(RA_RS_CTX_QP_UNBIND, sizeof(union op_ctx_qp_unbind_data));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)g_ra_rs_ctx_ops.ctx_batch_send_wr, 1, 0);
    add_test_msg(RA_RS_CTX_BATCH_SEND_WR, sizeof(union op_ctx_batch_send_wr_data));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)g_ra_rs_ctx_ops.ctx_update_ci, 1, 0);
    add_test_msg(RA_RS_CTX_UPDATE_CI, sizeof(union op_ctx_update_ci_data));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)g_ra_rs_ctx_ops.ctx_token_id_alloc, 1, 0);
    add_test_msg(RA_RS_CTX_TOKEN_ID_ALLOC, sizeof(union op_token_id_alloc_data));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)g_ra_rs_ctx_ops.ctx_token_id_free, 1, 0);
    add_test_msg(RA_RS_CTX_TOKEN_ID_FREE, sizeof(union op_token_id_free_data));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)g_ra_rs_ctx_ops.get_tp_info_list, 1, 0);
    add_test_msg(RA_RS_GET_TP_INFO_LIST, sizeof(union op_get_tp_info_list_data));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)g_ra_rs_ctx_ops.ccu_custom_channel, 1, 0);
    add_test_msg(RA_RS_CUSTOM_CHANNEL, sizeof(union op_custom_channel_data));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)g_ra_rs_ctx_ops.ctx_qp_destroy_batch, 1, 0);
    add_test_msg(RA_RS_CTX_QP_DESTROY_BATCH, sizeof(union op_ctx_qp_destroy_batch_data));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)g_ra_rs_ctx_ops.ctx_qp_query_batch, 1, 0);
    add_test_msg(RA_RS_CTX_QUERY_QP_BATCH, sizeof(union op_ctx_qp_query_batch_data));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)g_ra_rs_ctx_ops.get_eid_by_ip, 1, 0);
    add_test_msg(RA_RS_GET_EID_BY_IP, sizeof(union op_get_eid_by_ip_data));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)g_ra_rs_ctx_ops.ctx_get_aux_info, 1, 0);
    add_test_msg(RA_RS_CTX_GET_AUX_INFO, sizeof(union op_ctx_get_aux_info_data));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)g_ra_rs_ctx_ops.get_tp_attr, 1, 0);
    add_test_msg(RA_RS_GET_TP_ATTR, sizeof(union op_get_tp_attr_data));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)g_ra_rs_ctx_ops.set_tp_attr, 1, 0);
    add_test_msg(RA_RS_SET_TP_ATTR, sizeof(union op_set_tp_attr_data));
    tc_common_test();

    tc_adp_env_init();
    mocker((stub_fn_t)g_ra_rs_ctx_ops.ctx_get_cr_err_info_list, 1, 0);
    add_test_msg(RA_RS_CTX_GET_CR_ERR_INFO_LIST, sizeof(union op_ctx_get_cr_err_info_list_data));
    tc_common_test();

    mocker_clean();
}

void tc_ra_rs_get_tls_enable0()
{
    mocker_clean();
    tc_adp_env_init();
    mocker((stub_fn_t)RsGetTlsEnable, 1, 0);
    add_test_msg(RA_RS_GET_TLS_ENABLE, sizeof(union OpGetTlsEnableData));
    tc_common_test();
    mocker_clean();
}

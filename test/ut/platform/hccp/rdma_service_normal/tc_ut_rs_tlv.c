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
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <pthread.h>
#include "securec.h"
#include "dl_hal_function.h"
#include "dl_netco_function.h"
#include "dl_ccu_function.h"
#include "ut_dispatch.h"
#include "hccp_tlv.h"
#include "rs_inner.h"
#include "rs_tlv.h"
#include "rs_epoll.h"
#include "ra_rs_comm.h"
#include "ra_rs_err.h"
#include "file_opt.h"
#include "rs_adp_nslb.h"
#include "network_comm.h"

static struct rs_cb stub_rs_cb;
extern int RsTlvAssembleSendData(struct TlvBufInfo *buf_info, struct TlvRequestMsgHead *head, char *data,
    unsigned int *send_finish);
extern void RsEpollEventHandleOne(struct rs_cb *rs_cb, struct epoll_event *events);
extern int RsNslbRequest(struct TlvRequestMsgHead *head, char *data);
extern int RsNetcoTblApiInit(void);
extern int rs_ccu_request(struct TlvRequestMsgHead *head, char *data);
extern int RsGetTlvCb(uint32_t phyId, struct RsTlvCb **tlvCb);
extern int RsNetcoInitArg(unsigned int phyId, NetCoIpPortArg *netcoArg);

int stub_rs_get_nslb_cb(uint32_t phyId, struct RsTlvCb **tlvCb)
{
    stub_rs_cb.connCb.epollfd = 0;
    stub_rs_cb.tlvCb.bufInfo.bufferSize = RS_TLV_BUFFER_SIZE;
    stub_rs_cb.tlvCb.bufInfo.buf = (char *)calloc(stub_rs_cb.tlvCb.bufInfo.bufferSize, sizeof(char));
    stub_rs_cb.tlvCb.initFlag = false;
    pthread_mutex_init(&stub_rs_cb.tlvCb.mutex, NULL);
    *tlvCb = &stub_rs_cb.tlvCb;
    return 0;
}

int stub_rs_get_nslb_cb_deinit(uint32_t phyId, struct RsTlvCb **tlvCb)
{
    stub_rs_cb.connCb.epollfd = 0;
    stub_rs_cb.tlvCb.bufInfo.bufferSize = RS_TLV_BUFFER_SIZE;
    stub_rs_cb.tlvCb.bufInfo.buf = (char *)calloc(stub_rs_cb.tlvCb.bufInfo.bufferSize, sizeof(char));
    stub_rs_cb.tlvCb.initFlag = true;
    pthread_mutex_init(&stub_rs_cb.tlvCb.mutex, NULL);
    *tlvCb = &stub_rs_cb.tlvCb;
    return 0;
}

int stub_rs_get_nslb_cb_after_deinit(uint32_t phyId, struct RsTlvCb **tlvCb)
{
    *tlvCb = &stub_rs_cb.tlvCb;
    return 0;
}

int stub_rs_get_nslb_cb_init(uint32_t phyId, struct RsTlvCb **tlvCb)
{
    stub_rs_cb.tlvCb.initFlag = false;
    *tlvCb = &stub_rs_cb.tlvCb;
    return 0;
}

int stub_rs_tlv_assemble_send_data(struct TlvBufInfo *buf_info, struct TlvRequestMsgHead *head, char *data,
    bool *send_finish)
{
    if (head->offset == 0) {
        *send_finish = false;
    } else {
        *send_finish = true;
    }
    return 0;
}

int stub_rs_get_rs_cb_v2(unsigned int phyId, struct rs_cb **rs_cb)
{
    stub_rs_cb.tlvCb.initFlag = false;
    stub_rs_cb.connCb.epollfd = 0;
    *rs_cb = &stub_rs_cb;
    return 0;
}

int stub_file_read_cfg(const char *file_path, int dev_id, const char *conf_name, char *conf_value, unsigned int len)
{
    if (strncmp(conf_name, "udp_port_mode", strlen("udp_port_mode") + 1) == 0){
        memcpy_s(conf_value, len, "nslb_dp", strlen("nslb_dp"));
    } else {
        memcpy_s(conf_value, len, "16666", strlen("16666"));
    }
    return 0;
}

void free_rs_cb() {
    pthread_mutex_destroy(&stub_rs_cb.tlvCb.mutex);
    free(stub_rs_cb.tlvCb.bufInfo.buf);
    stub_rs_cb.tlvCb.bufInfo.buf = NULL;
}

void tc_rs_nslb_init()
{
    unsigned int buffer_size = 0;
    unsigned int phyId = 0;
    int ret;

    mocker_invoke(RsGetRsCb, stub_rs_get_rs_cb_v2, 10);
    mocker(calloc, 10, NULL);
    ret = RsTlvInit(phyId, &buffer_size);
    EXPECT_INT_EQ(-ENOMEM, ret);
    mocker_clean();

    mocker_invoke(RsGetRsCb, stub_rs_get_rs_cb_v2, 10);
    ret = RsTlvInit(phyId, &buffer_size);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();
    free_rs_cb();
}

void tc_rs_nslb_deinit()
{
    unsigned int phyId = 0;
    int ret;

    mocker_invoke(RsGetTlvCb, stub_rs_get_nslb_cb_deinit, 10);
    ret = RsTlvDeinit(phyId);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();

    mocker_invoke(RsGetTlvCb, stub_rs_get_nslb_cb_after_deinit, 10);
    ret = RsTlvDeinit(phyId);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();
}

void tc_rs_nslb_request()
{
    struct TlvRequestMsgHead head = {0};
    char *data;
    int ret;

    head.phyId = 0;
    head.type = 0;
    data = (char *)calloc(16, sizeof(char));

    mocker_invoke(RsGetTlvCb, stub_rs_get_nslb_cb, 10);
    mocker(RsTlvAssembleSendData, 10, -EINVAL);
    ret = RsTlvRequest(&head, data);
    EXPECT_INT_EQ(-EINVAL, ret);
    mocker_clean();
    free_rs_cb();

    head.offset = 0;
    mocker_invoke(RsGetTlvCb, stub_rs_get_nslb_cb, 10);
    mocker_invoke(RsTlvAssembleSendData, stub_rs_tlv_assemble_send_data, 10);
    ret = RsTlvRequest(&head, data);
    EXPECT_INT_EQ(0, ret);
    free_rs_cb();

    head.offset = 1U;
    head.totalBytes = 16U;
    ret = RsTlvRequest(&head, data);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();
    free_rs_cb();

    free(data);
    data = NULL;
}

void tc_rs_tlv_assemble_send_data()
{
    struct TlvRequestMsgHead head;
    struct TlvBufInfo buf_info;
    bool send_finish;
    char data[16] = {0};
    int ret;

    buf_info.bufferSize = RS_TLV_BUFFER_SIZE;
    buf_info.buf = (char *)calloc(RS_TLV_BUFFER_SIZE, sizeof(char));
    memset_s(buf_info.buf, buf_info.bufferSize, 0, buf_info.bufferSize);
    head.sendBytes = 16U;
    head.totalBytes = 16U;
    head.offset = 0;
    head.phyId = 0;
    head.type = 0;

    mocker(memset_s, 10 , 0);
    mocker(memcpy_s, 10 , 0);
    ret = RsTlvAssembleSendData(&buf_info, &head, data, &send_finish);
    EXPECT_INT_EQ(0, ret);

    head.offset = 0;
    head.sendBytes = 8U;
    head.totalBytes = 16U;
    ret = RsTlvAssembleSendData(&buf_info, &head, data, &send_finish);
    EXPECT_INT_EQ(0, ret);

    head.offset = 16U;
    head.sendBytes = 16U;
    head.totalBytes = 16U;
    ret = RsTlvAssembleSendData(&buf_info, &head, data, &send_finish);
    EXPECT_INT_NE(0, ret);

    head.offset = RS_TLV_BUFFER_SIZE;
    ret = RsTlvAssembleSendData(&buf_info, &head, data, &send_finish);
    EXPECT_INT_EQ(-EINVAL, ret);

    head.offset = 0;
    head.sendBytes = 2049U;
    ret = RsTlvAssembleSendData(&buf_info, &head, data, &send_finish);
    EXPECT_INT_EQ(-EINVAL, ret);
    mocker_clean();

    free(buf_info.buf);
    buf_info.buf = NULL;
}

void tc_rs_epoll_nslb_event_handle()
{
    struct epoll_event test_events;
    struct rs_cb test_rs_cb = {0};
    int ret;

    test_rs_cb.tlvCb.nslbCb.initFlag = true;
    test_events.events = 0;

    mocker(RsEpollNslbEventHandle, 10, NET_CO_PROCED);
    RsEpollEventHandleOne(&test_rs_cb, &test_events);
    mocker_clean();

    pthread_mutex_init(&test_rs_cb.tlvCb.nslbCb.mutex, NULL);
    ret = RsEpollNslbEventHandle(&test_rs_cb.tlvCb.nslbCb, 0, 0);
    EXPECT_INT_EQ(NET_CO_PROCED, ret);
    mocker_clean();

    mocker(RsNetcoEventDispatch, 10, -1);
    ret = RsEpollNslbEventHandle(&test_rs_cb.tlvCb.nslbCb, 0, 0);
    EXPECT_INT_NE(NET_CO_PROCED, ret);
    mocker_clean();
    pthread_mutex_destroy(&test_rs_cb.tlvCb.nslbCb.mutex);
}

void tc_rs_get_tlv_cb()
{
    struct RsTlvCb *tlvCb = NULL;
    uint32_t phyId;
    int ret;

    mocker_invoke(RsGetRsCb, stub_rs_get_rs_cb_v2, 10);
    ret = RsGetTlvCb(phyId, &tlvCb);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();
}

void tc_rs_nslb_api_init()
{
    NetCoIpPortArg arg = {0};
    unsigned int data_len = 0;
    unsigned int type = 0;
    void *stub_co;
    char *data;
    int ret;

    RsNetcoInit(0, arg);

    mocker(RsNetcoTblApiInit, 10, -1);
    ret = RsNetcoApiInit();
    EXPECT_INT_NE(0, ret);
    mocker_clean();
}

void tc_rs_ccu_request()
{
    struct TlvRequestMsgHead head = {0};
    char data[10];
    int ret;

    head.type = MSG_TYPE_CCU_INIT;
    mocker(rs_ccu_init, 10, -1);
    ret = rs_ccu_request(&head, data);
    EXPECT_INT_NE(0, ret);
    mocker_clean();

    mocker(rs_ccu_init, 10, 0);
    ret = rs_ccu_request(&head, data);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();

    head.type = MSG_TYPE_CCU_UNINIT;
    mocker(rs_ccu_uninit, 10, -1);
    ret = rs_ccu_request(&head, data);
    EXPECT_INT_NE(0, ret);
    mocker_clean();

    mocker(rs_ccu_uninit, 10, 0);
    ret = rs_ccu_request(&head, data);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();

    head.type = MSG_TYPE_CCU_MAX;
    ret = rs_ccu_request(&head, data);
    EXPECT_INT_NE(0, ret);
}

void tc_RsNslbNetcoInitDeinit()
{
    struct RsNslbCb nslbCb = {0};
    char netcoCb = 0;
    int ret = 0;

    mocker(RsNetcoInitArg, 10, 0);
    mocker(RsNslbApiInit, 10, 0);
    mocker_invoke(RsGetRsCb, stub_rs_get_rs_cb_v2, 10);
    mocker(RsNetcoInit, 10, &netcoCb);
    ret = RsNslbNetcoInit(0, &nslbCb);
    EXPECT_INT_EQ(0, ret);

    mocker(RsNetcoDeinit, 10, 0);
    mocker(RsNslbApiDeinit, 10, 0);
    RsNslbNetcoDeinit(&nslbCb);
    mocker_clean();
}

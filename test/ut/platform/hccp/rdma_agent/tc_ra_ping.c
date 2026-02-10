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
#include "ut_dispatch.h"
#include "hccp_common.h"
#include "hccp_ping.h"
#include "ra.h"
#include "ra_ping.h"
#include "ra_hdc_ping.h"
#include "ra_hdc.h"
#include "ra_client_host.h"
#include "tc_ra_ping.h"

extern int RaPingInitGetHandle(struct PingInitAttr *initAttr, struct PingInitInfo *initInfo,
    struct RaPingHandle *pingHandle);
extern int RaPingDeinitParaCheck(struct RaPingHandle *pingHandle);

void tc_ra_ping_init_get_handle_abnormal()
{
    struct RaPingHandle ping_handle = { 0 };
    struct PingInitAttr init_attr = { 0 };
    struct PingInitInfo init_info = { 0 };
    int ret;

    ret = RaPingInitGetHandle(&init_attr, NULL, NULL);
    EXPECT_INT_EQ(ret, -EINVAL);

    init_attr.bufferSize = 1;
    ret = RaPingInitGetHandle(&init_attr, &init_info, NULL);
    EXPECT_INT_EQ(ret, -EINVAL);

    init_attr.bufferSize = 0;
    init_attr.mode = NETWORK_PEER_ONLINE;
    ret = RaPingInitGetHandle(&init_attr, &init_info, &ping_handle);
    EXPECT_INT_EQ(ret, -EINVAL);

    init_attr.mode = NETWORK_OFFLINE;
    mocker(RaRdevInitCheck, 1, -1);
    ret = RaPingInitGetHandle(&init_attr, &init_info, &ping_handle);
    EXPECT_INT_EQ(ret, -22);
    mocker_clean();

    mocker(RaRdevInitCheck, 1, 0);
    mocker((stub_fn_t)pthread_mutex_init, 1, -1);
    ret = RaPingInitGetHandle(&init_attr, &init_info, &ping_handle);
    EXPECT_INT_EQ(ret, -22);
    mocker_clean();

    init_attr.bufferSize = RA_RS_PING_BUFFER_ALIGN_4K_PAGE_SIZE;
    init_attr.commInfo.rdma.udpSport = 65536;
    ret = RaPingInitGetHandle(&init_attr, &init_info, &ping_handle);
    EXPECT_INT_EQ(ret, -22);
    mocker_clean();
}

void tc_ra_ping_init_abnormal()
{
    void *ping_handle = NULL;
    int ret;

    ret = RaPingInit(NULL, NULL, NULL);
    EXPECT_INT_EQ(ret, -EINVAL);

    mocker((stub_fn_t)calloc, 1, NULL);
    ret = RaPingInit(NULL, NULL, &ping_handle);
    EXPECT_INT_NE(ret, 0);
    mocker_clean();

    mocker(RaPingInitGetHandle, 1, -1);
    ret = RaPingInit(NULL, NULL, &ping_handle);
    EXPECT_INT_NE(ret, 0);
    mocker_clean();
}

void tc_ra_ping_target_add_abnormal()
{
    struct PingTargetInfo  target[1] = { 0 };
    struct RaPingHandle ping_handle = { 0 };
    struct RaPingOps ops = { 0 };
    int num;
    int ret;

    num = 0;
    ret = RaPingTargetAdd((void *)(&ping_handle), target, num);
    EXPECT_INT_NE(ret, 0);

    num = 1;
    ping_handle.pingOps = &ops;
    ret = RaPingTargetAdd((void *)(&ping_handle), target, num);
    EXPECT_INT_NE(ret, 0);

    ops.raPingTargetAdd = RaHdcPingTargetAdd;
    ping_handle.phyId = RA_MAX_PHY_ID_NUM;
    ret = RaPingTargetAdd((void *)(&ping_handle), target, num);
    EXPECT_INT_NE(ret, 0);

    ping_handle.phyId = 0;
    ping_handle.taskCnt = 1;
    ret = RaPingTargetAdd((void *)(&ping_handle), target, num);
    EXPECT_INT_NE(ret, 0);

    ping_handle.taskCnt = 0;
    mocker(RaHdcPingTargetAdd, 1, -1);
    ret = RaPingTargetAdd((void *)(&ping_handle), target, num);
    EXPECT_INT_NE(ret, 0);
    mocker_clean();
}

void tc_ra_ping_task_start_abnormal()
{
    struct RaPingHandle ping_handle = { 0 };
    struct PingTaskAttr attr = { 0 };
    struct RaPingOps ops = { 0 };
    int ret;

    attr.packetCnt = 1;
    attr.packetInterval = 1;
    attr.timeoutInterval = 1;

    ret = RaPingTaskStart((void *)(&ping_handle), NULL);
    EXPECT_INT_NE(ret, 0);

    ping_handle.pingOps = &ops;
    ret = RaPingTaskStart((void *)(&ping_handle), &attr);
    EXPECT_INT_NE(ret, 0);

    ops.raPingTaskStart = RaHdcPingTaskStart;
    ping_handle.phyId = RA_MAX_PHY_ID_NUM;
    ret = RaPingTaskStart((void *)(&ping_handle), &attr);
    EXPECT_INT_NE(ret, 0);

    ping_handle.phyId = 0;
    ping_handle.taskCnt = 1;
    ret = RaPingTaskStart((void *)(&ping_handle), &attr);
    EXPECT_INT_NE(ret, 0);

    ping_handle.taskCnt = 0;
    ping_handle.targetCnt = 0;
    ret = RaPingTaskStart((void *)(&ping_handle), &attr);
    EXPECT_INT_NE(ret, 0);

    ping_handle.targetCnt = 1;
    ping_handle.bufferSize = 0;
    ret = RaPingTaskStart((void *)(&ping_handle), &attr);
    EXPECT_INT_NE(ret, 0);

    ping_handle.bufferSize = ping_handle.targetCnt * attr.packetCnt * PING_TOTAL_PAYLOAD_MAX_SIZE;
    mocker(RaHdcPingTaskStart, 1, -1);
    ret = RaPingTaskStart((void *)(&ping_handle), &attr);
    EXPECT_INT_NE(ret, 0);
    mocker_clean();
}

void tc_ra_ping_get_results_abnormal()
{
    struct PingTargetResult target[1] = { 0 };
    struct RaPingHandle ping_handle = { 0 };
    struct RaPingOps ops = { 0 };
    int num = 0;
    int ret;

    ret = RaPingGetResults((void *)(&ping_handle), target, NULL);
    EXPECT_INT_NE(ret, 0);

    num = 1;
    ping_handle.pingOps = &ops;
    ret = RaPingGetResults((void *)(&ping_handle), target, &num);
    EXPECT_INT_NE(ret, 0);

    ops.raPingGetResults = RaHdcPingGetResults;
    ping_handle.phyId = RA_MAX_PHY_ID_NUM;
    ret = RaPingGetResults((void *)(&ping_handle), target, &num);
    EXPECT_INT_NE(ret, 0);

    ping_handle.phyId = 0;
    ping_handle.targetCnt = 0;
    ret = RaPingGetResults((void *)(&ping_handle), target, &num);
    EXPECT_INT_NE(ret, 0);

    ping_handle.targetCnt = 1;
    mocker(RaHdcPingGetResults, 1, -1);
    ret = RaPingGetResults((void *)(&ping_handle), target, &num);
    EXPECT_INT_NE(ret, 0);
    mocker_clean();
}

void tc_ra_ping_target_del_abnoraml()
{
    struct PingTargetResult target[1] = { 0 };
    struct RaPingHandle ping_handle = { 0 };
    struct RaPingOps ops = { 0 };
    int num = 0;
    int ret;

    ret = RaPingTargetDel((void *)(&ping_handle), target, num);
    EXPECT_INT_NE(ret, 0);

    num = 1;
    ping_handle.pingOps = &ops;
    ret = RaPingTargetDel((void *)(&ping_handle), target, num);
    EXPECT_INT_NE(ret, 0);

    ops.raPingTargetDel = RaHdcPingTargetDel;
    ping_handle.taskCnt = 1;
    ret = RaPingTargetDel((void *)(&ping_handle), target, num);
    EXPECT_INT_NE(ret, 0);

    ping_handle.taskCnt = 0;
    ping_handle.targetCnt = 0;
    ret = RaPingTargetDel((void *)(&ping_handle), target, num);
    EXPECT_INT_NE(ret, 0);

    ping_handle.targetCnt = 1;
    ping_handle.phyId = RA_MAX_PHY_ID_NUM;
    ret = RaPingTargetDel((void *)(&ping_handle), target, num);
    EXPECT_INT_NE(ret, 0);

    ping_handle.phyId = 0;
    mocker(RaHdcPingTargetDel, 1, -1);
    ret = RaPingTargetDel((void *)(&ping_handle), target, num);
    EXPECT_INT_NE(ret, 0);
    mocker_clean();
}

void tc_ra_ping_task_stop_abnormal()
{
    struct RaPingHandle ping_handle = { 0 };
    struct RaPingOps ops = { 0 };
    int ret;

    ret = RaPingTaskStop(NULL);
    EXPECT_INT_NE(ret, 0);

    ping_handle.pingOps = &ops;
    ret = RaPingTaskStop((void *)(&ping_handle));
    EXPECT_INT_NE(ret, 0);

    ops.raPingTaskStop = RaHdcPingTaskStop;
    ping_handle.phyId = RA_MAX_PHY_ID_NUM;
    ret = RaPingTaskStop((void *)(&ping_handle));
    EXPECT_INT_NE(ret, 0);

    ping_handle.phyId = 0;
    ping_handle.taskCnt = 0;
    ret = RaPingTaskStop((void *)(&ping_handle));
    EXPECT_INT_NE(ret, 0);

    ping_handle.taskCnt = 1;
    mocker(RaHdcPingTaskStop, 1, -1);
    ret = RaPingTaskStop((void *)(&ping_handle));
    EXPECT_INT_NE(ret, 0);
    mocker_clean();
}

int ra_hdc_ping_deinit_stub(struct RaPingHandle *ping_handle)
{
    return 0;
}

void tc_ra_ping_deinit_para_check_abnormal()
{
    struct RaPingHandle ping_handle = { 0 };
    struct RaPingOps ops = { 0 };
    int ret;

    ping_handle.phyId = RA_MAX_PHY_ID_NUM;
    ret = RaPingDeinitParaCheck(&ping_handle);
    EXPECT_INT_EQ(ret, -EINVAL);

    ping_handle.phyId = 0;
    ping_handle.pingOps = &ops;
    ping_handle.pingOps->raPingDeinit = ra_hdc_ping_deinit_stub;
    mocker(RaInetPton, 1, -1);
    ret = RaPingDeinitParaCheck(&ping_handle);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

    mocker(RaInetPton, 1, 0);
    ping_handle.pingOps->raPingDeinit = NULL;
    ret = RaPingDeinitParaCheck(&ping_handle);
    EXPECT_INT_EQ(ret, -EINVAL);
    mocker_clean();
}

void tc_ra_ping_deinit_abnoaml()
{
    struct RaPingHandle *ping_handle = calloc(1, sizeof(struct RaPingHandle));
    struct RaPingOps ops = { 0 };
    int ret;

    ret = RaPingDeinit(NULL);
    EXPECT_INT_NE(ret, 0);

    mocker(RaPingDeinitParaCheck, 1, -1);
    ret = RaPingDeinit((void *)ping_handle);
    EXPECT_INT_NE(ret, 0);
    mocker_clean();

    ops.raPingDeinit = RaHdcPingDeinit;
    ping_handle->pingOps = &ops;
    mocker(RaPingDeinitParaCheck, 1, 0);
    mocker(RaHdcPingDeinit, 1, -1);
    ret = RaPingDeinit((void *)ping_handle);
    EXPECT_INT_NE(ret, 0);
    mocker_clean();
}

static void init_attr_fill(struct PingInitAttr *init_attr)
{
    struct rdev rdev_info = { 0 };
    rdev_info.phyId = 0;
    rdev_info.family = AF_INET;

    init_attr->mode = NETWORK_OFFLINE;
    init_attr->dev.rdma = rdev_info;
    init_attr->client.rdma.cqAttr.sendCqDepth = 128;
    init_attr->client.rdma.cqAttr.recvCqDepth = 128;
    init_attr->client.rdma.qpAttr.cap.maxInlineData = 32;
    init_attr->client.rdma.qpAttr.cap.maxSendSge = 1;
    init_attr->client.rdma.qpAttr.cap.maxSendWr = 128;
    init_attr->client.rdma.qpAttr.cap.maxRecvSge = 1;
    init_attr->client.rdma.qpAttr.cap.maxRecvWr = 128;

    init_attr->server.rdma.cqAttr.sendCqDepth = 128;
    init_attr->server.rdma.cqAttr.recvCqDepth = 128;
    init_attr->server.rdma.qpAttr.cap.maxInlineData = 32;
    init_attr->server.rdma.qpAttr.cap.maxSendSge = 1;
    init_attr->server.rdma.qpAttr.cap.maxSendWr = 128;
    init_attr->server.rdma.qpAttr.cap.maxRecvSge = 1;
    init_attr->server.rdma.qpAttr.cap.maxRecvWr = 128;
    init_attr->bufferSize = 8192;
}

void tc_ra_ping()
{
    struct PingTargetCommInfo target_comm_client = {0};
    struct PingTargetResult target_result_client = {0};
    struct PingTargetInfo  target_info_client = {0};
    char payload_client[20] = "hello, client";
    struct PingInitInfo init_info = { 0 };
    struct PingInitAttr init_attr = { 0 };
    struct PingTaskAttr taskAttr = {0};
    unsigned int target_result_num = 1;
    struct RaPingOps ops = { 0 };
    void *ping_handle = NULL;
    int ret;

    init_attr_fill(&init_attr);
    init_attr.bufferSize = 4;
    ret = RaPingInit(&init_attr, &init_info, &ping_handle);
    EXPECT_INT_NE(ret, 0);
    init_attr.bufferSize = 8192;
    mocker(RaRdevInitCheck, 20, 0);
    mocker((stub_fn_t)RaHdcProcessMsg, 20, 0);
    ret = RaPingInit(&init_attr, &init_info, &ping_handle);
    EXPECT_INT_EQ(ret, 0);

    target_info_client.localInfo.rdma.hopLimit = 64;
    target_info_client.localInfo.rdma.qosAttr.tc = (33 & 0x3f) << 2;
    target_info_client.localInfo.rdma.qosAttr.sl = 4;
    target_info_client.remoteInfo.qpInfo = init_info.client;
    ret = RaPingTargetAdd(ping_handle, &target_info_client, 1);
    EXPECT_INT_EQ(ret, 0);

    taskAttr.packetCnt = 1;
    taskAttr.packetInterval = 10;
    taskAttr.timeoutInterval = 10;
    ret = RaPingTaskStart(ping_handle, &taskAttr);
    EXPECT_INT_EQ(ret, 0);

    target_result_client.remoteInfo = target_info_client.remoteInfo;
    mocker(RaHdcPingGetResults, 20, 0);
    ret = RaPingGetResults(ping_handle, &target_result_client, &target_result_num);
    EXPECT_INT_EQ(ret, 0);

    ret = RaPingTaskStop(ping_handle);
    EXPECT_INT_EQ(ret, 0);

    target_comm_client.ip = target_info_client.remoteInfo.ip;
    target_comm_client.qpInfo = target_info_client.remoteInfo.qpInfo;
    ret = RaPingTargetDel(ping_handle, &target_comm_client, 1);
    EXPECT_INT_EQ(ret, 0);

    ret = RaPingDeinit(ping_handle);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

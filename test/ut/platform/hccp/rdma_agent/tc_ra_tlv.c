/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "hccp_tlv.h"
#include "securec.h"
#include "ut_dispatch.h"
#include "dl_hal_function.h"
#include "ra_tlv.h"
#include "ra_hdc_tlv.h"

#define TC_TLV_MSG_SIZE    (64 * 1024)
#define TC_TLV_MSG_SIZE_INVALID    (64 * 1024 + 1U)

void tc_ra_tlv_init() {
    struct TlvInitInfo init_info = {0};
    struct RaTlvHandle *tlv_handle_tmp = NULL;
    unsigned int buffer_size = 0;
    int ret = 0;

    init_info.nicPosition = NETWORK_OFFLINE;
    init_info.phyId = 0;

    mocker(memcpy_s, 10 , 0);
    mocker(RaHdcTlvInit, 100 , -1);
    ret = RaTlvInit(&init_info, &buffer_size, &tlv_handle_tmp);
    EXPECT_INT_NE(0, ret);
    mocker_clean();

    mocker(RaHdcTlvInit, 100 , 0);
    mocker(memcpy_s, 10 , 0);
    mocker(pthread_mutex_init, 100 , -1);
    ret = RaTlvInit(&init_info, &buffer_size, &tlv_handle_tmp);
    EXPECT_INT_NE(0, ret);
    mocker_clean();

    mocker(RaHdcTlvInit, 100 , 0);
    mocker(memcpy_s, 10 , 0);
    mocker(pthread_mutex_init, 100 , 0);
    ret = RaTlvInit(&init_info, &buffer_size, &tlv_handle_tmp);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();
    free(tlv_handle_tmp);
    tlv_handle_tmp = NULL;
}

void tc_ra_tlv_deinit() {
    struct RaTlvHandle *tlv_handle_tmp = calloc(1, sizeof(struct RaTlvHandle));
    struct RaTlvOps tlv_ops  = {0};
    int ret = 0;

    tlv_handle_tmp->tlvOps = &tlv_ops;
    tlv_ops.raTlvDeinit = NULL;
    tlv_handle_tmp->initInfo.phyId = 0;
    ret = RaTlvDeinit(tlv_handle_tmp);
    EXPECT_INT_NE(0, ret);

    tlv_handle_tmp = calloc(1, sizeof(struct RaTlvHandle));
    tlv_handle_tmp->tlvOps = &tlv_ops;
    tlv_handle_tmp->initInfo.phyId = 0;
    tlv_ops.raTlvDeinit = RaHdcTlvDeinit;
    mocker(RaHdcTlvDeinit, 100 , -1);
    ret = RaTlvDeinit(tlv_handle_tmp);
    EXPECT_INT_NE(0, ret);
    mocker_clean();

    tlv_handle_tmp = calloc(1, sizeof(struct RaTlvHandle));
    tlv_handle_tmp->tlvOps = &tlv_ops;
    tlv_handle_tmp->initInfo.phyId = 0;
    tlv_ops.raTlvDeinit = RaHdcTlvDeinit;
    mocker(RaHdcTlvDeinit, 100 , 0);
    ret = RaTlvDeinit(tlv_handle_tmp);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();
}

void tc_ra_tlv_request() {
    unsigned int moduleType = TLV_MODULE_TYPE_CCU;
    struct RaTlvHandle tlv_handle_tmp = {0};
    struct RaTlvOps tlv_ops  = {0};
    struct TlvMsg send_msg = {0};
    struct TlvMsg recv_msg = {0};
    int ret = 0;

    tlv_handle_tmp.bufferSize = TC_TLV_MSG_SIZE;
    send_msg.length = TC_TLV_MSG_SIZE_INVALID;
    ret = RaTlvRequest(&tlv_handle_tmp, moduleType, &send_msg, &recv_msg);
    EXPECT_INT_NE(0, ret);

    tlv_handle_tmp.tlvOps = &tlv_ops;
    tlv_ops.raTlvRequest = NULL;
    send_msg.length = TC_TLV_MSG_SIZE;
    ret = RaTlvRequest(&tlv_handle_tmp, moduleType, &send_msg, &recv_msg);
    EXPECT_INT_NE(0, ret);

    tlv_ops.raTlvRequest = RaHdcTlvRequest;
    mocker(RaHdcTlvRequest, 100 , -1);
    ret = RaTlvRequest(&tlv_handle_tmp, moduleType, &send_msg, &recv_msg);
    EXPECT_INT_NE(0, ret);
    mocker_clean();

    mocker(RaHdcTlvRequest, 100 , 0);
    ret = RaTlvRequest(&tlv_handle_tmp, moduleType, &send_msg, &recv_msg);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();
}

/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * Description: ra tlv unit test.
 * Author:
 * Create: 2025-03-21
 */
#include "hccp_tlv.h"
#include "securec.h"
#include "ut_dispatch.h"
#include "dl_hal_function.h"
#include "ra_tlv.h"
#include "ra_hdc_tlv.h"

#define TC_TLV_MSG_SIZE    (64 * 1024) // 64KB
#define TC_TLV_MSG_SIZE_INVALID    (64 * 1024 + 1U) // 64KB + 1

void tc_ra_tlv_init() {
    struct tlv_init_info init_info = {0};
    struct ra_tlv_handle *tlv_handle_tmp = NULL;
    unsigned int module_type = TLV_MODULE_TYPE_NSLB;
    unsigned int buffer_size = 0;
    int ret = 0;

    init_info.nic_position = NETWORK_OFFLINE;
    init_info.phy_id = 0;

    mocker(memcpy_s, 10 , 0);
    mocker(ra_hdc_tlv_init, 100 , -1);
    ret = ra_tlv_init(&init_info, module_type, &buffer_size, &tlv_handle_tmp);
    EXPECT_INT_NE(0, ret);
    mocker_clean();

    mocker(ra_hdc_tlv_init, 100 , 0);
    mocker(memcpy_s, 10 , 0);
    ret = ra_tlv_init(&init_info, module_type, &buffer_size, &tlv_handle_tmp);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();
    free(tlv_handle_tmp);
    tlv_handle_tmp = NULL;
}

void tc_ra_tlv_deinit() {
    struct ra_tlv_handle *tlv_handle_tmp = calloc(1, sizeof(struct ra_tlv_handle));
    struct ra_tlv_ops tlv_ops  = {0};
    int ret = 0;
 
    tlv_handle_tmp->tlv_ops = &tlv_ops;
    tlv_ops.ra_tlv_deinit = NULL;
    tlv_handle_tmp->init_info.phy_id = 0;
    ret = ra_tlv_deinit(tlv_handle_tmp);
    EXPECT_INT_NE(0, ret);
 
    tlv_handle_tmp = calloc(1, sizeof(struct ra_tlv_handle));
    tlv_handle_tmp->tlv_ops = &tlv_ops;
    tlv_handle_tmp->init_info.phy_id = 0;
    tlv_ops.ra_tlv_deinit = ra_hdc_tlv_deinit;
    mocker(ra_hdc_tlv_deinit, 100 , -1);
    ret = ra_tlv_deinit(tlv_handle_tmp);
    EXPECT_INT_NE(0, ret);
    mocker_clean();
 
    tlv_handle_tmp = calloc(1, sizeof(struct ra_tlv_handle));
    tlv_handle_tmp->tlv_ops = &tlv_ops;
    tlv_handle_tmp->init_info.phy_id = 0;
    tlv_ops.ra_tlv_deinit = ra_hdc_tlv_deinit;
    mocker(ra_hdc_tlv_deinit, 100 , 0);
    ret = ra_tlv_deinit(tlv_handle_tmp);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();
}

void tc_ra_tlv_request() {
    struct ra_tlv_handle tlv_handle_tmp = {0};
    struct ra_tlv_ops tlv_ops  = {0};
    struct tlv_msg send_msg = {0};
    struct tlv_msg recv_msg = {0};
    int ret = 0;

    tlv_handle_tmp.buffer_size = TC_TLV_MSG_SIZE;
    send_msg.length = TC_TLV_MSG_SIZE_INVALID;
    ret = ra_tlv_request(&tlv_handle_tmp, &send_msg, &recv_msg);
    EXPECT_INT_NE(0, ret);

    send_msg.length = 0;
    ret = ra_tlv_request(&tlv_handle_tmp, &send_msg, &recv_msg);
    EXPECT_INT_NE(0, ret);

    tlv_handle_tmp.tlv_ops = &tlv_ops;
    tlv_ops.ra_tlv_request = NULL;
    send_msg.length = TC_TLV_MSG_SIZE;
    ret = ra_tlv_request(&tlv_handle_tmp, &send_msg, &recv_msg);
    EXPECT_INT_NE(0, ret);

    tlv_ops.ra_tlv_request = ra_hdc_tlv_request;
    mocker(ra_hdc_tlv_request, 100 , -1);
    ret = ra_tlv_request(&tlv_handle_tmp, &send_msg, &recv_msg);
    EXPECT_INT_NE(0, ret);
    mocker_clean();

    mocker(ra_hdc_tlv_request, 100 , 0);
    ret = ra_tlv_request(&tlv_handle_tmp, &send_msg, &recv_msg);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();
}

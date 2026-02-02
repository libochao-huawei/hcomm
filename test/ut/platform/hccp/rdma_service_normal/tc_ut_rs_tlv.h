/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * Description: rs tlv unit test.
 * Create: 2025-03-21
 */

#ifndef TC_UT_RS_TLV_H
#define TC_UT_RS_TLV_H

void tc_rs_nslb_init();
void tc_rs_nslb_deinit();
void tc_rs_nslb_request();
void tc_rs_epoll_nslb_event_handle();
void tc_rs_tlv_assemble_send_data();
void tc_rs_get_tlv_cb();
void tc_rs_nslb_api_init();
void tc_rs_ccu_request();
void tc_RsNslbNetcoInitDeinit();

#endif // TC_UT_RS_TLV_H

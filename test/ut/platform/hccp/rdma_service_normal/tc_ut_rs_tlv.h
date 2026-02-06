/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
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

#endif

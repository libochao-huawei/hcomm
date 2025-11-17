/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef RA_HDC_ASYNC_H
#define RA_HDC_ASYNC_H

#include "hccp_common.h"
#include "hccp_ctx.h"
#include "ra.h"
#include "ra_ctx.h"
#include "ra_async.h"
#include "ra_rs_comm.h"
#include "ra_hdc.h"

union op_async_hdc_connect_data {
    struct {
        unsigned int phy_id;
        unsigned int queue_size;
        unsigned int thread_num;
        unsigned int resv[RA_RSVD_NUM_4];
    } tx_data;

    struct {
        unsigned int resv[RA_RSVD_NUM_4];
    } rx_data;
};

union op_async_hdc_close_data {
    struct {
        unsigned int phy_id;
        unsigned int resv[RA_RSVD_NUM_4];
    } tx_data;

    struct {
        unsigned int resv[RA_RSVD_NUM_4];
    } rx_data;
};

int ra_hdc_init_async(struct ra_init_config *cfg);
int ra_hdc_deinit_async(unsigned int phy_id);
int ra_hdc_send_msg_async(unsigned int opcode, unsigned int phy_id, char *data, unsigned int data_size,
    struct ra_request_handle *req_handle);
void hdc_async_del_response(struct ra_request_handle *req_handle);
int ra_hdc_async_save_snapshot(unsigned int phy_id, enum save_snapshot_action action);
int ra_hdc_async_restore_snapshot(unsigned int phy_id);
#endif // RA_HDC_ASYNC_H

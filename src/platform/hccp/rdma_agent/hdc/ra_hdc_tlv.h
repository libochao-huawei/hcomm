/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef RA_HDC_TLV_H
#define RA_HDC_TLV_H

#include "ra_tlv.h"
#include "ra_rs_comm.h"
#include "ra_hdc.h"

#define MAX_TLV_MSG_DATA_LEN 2048U

union op_tlv_init_data {
    struct {
        unsigned int phy_id;
        unsigned int module_type;
        uint32_t reserved[RA_RSVD_NUM_61];
    } tx_data;

    struct {
        unsigned int buffer_size;
        uint32_t reserved[RA_RSVD_NUM_63];
    } rx_data;
};

union op_tlv_deinit_data {
    struct {
        unsigned int phy_id;
        unsigned int module_type;
        uint32_t reserved[RA_RSVD_NUM_61];
    } tx_data;

    struct {
        uint32_t reserved[RA_RSVD_NUM_64];
    } rx_data;
};

union op_tlv_request_data {
    struct {
        struct tlv_request_msg_head head;
        char data[MAX_TLV_MSG_DATA_LEN];
    } tx_data;

    struct {
        unsigned int recv_bytes;
        char recv_data[MAX_TLV_MSG_DATA_LEN];
    } rx_data;
};

int RaHdcTlvInit(struct ra_tlv_handle *tlvHandle);
int RaHdcTlvDeinit(struct ra_tlv_handle *tlvHandle);
int RaHdcTlvRequest(struct ra_tlv_handle *tlvHandle, struct tlv_msg *sendMsg, struct tlv_msg *recvMsg);
#endif // RA_HDC_TLV_H

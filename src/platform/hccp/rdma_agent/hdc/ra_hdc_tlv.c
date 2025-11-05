/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "securec.h"
#include "user_log.h"
#include "ra_rs_comm.h"
#include "ra_rs_err.h"
#include "hccp_tlv.h"
#include "ra_hdc_tlv.h"

int ra_hdc_tlv_init(struct ra_tlv_handle *tlv_handle)
{
    unsigned int phy_id = tlv_handle->init_info.phy_id;
    union op_tlv_init_data tlv_data = { 0 };
    int ret = 0;

    tlv_data.tx_data.module_type = tlv_handle->module_type;
    tlv_data.tx_data.phy_id = phy_id;

    ret = ra_hdc_process_msg(RA_RS_TLV_INIT, phy_id, (char *)&tlv_data, sizeof(union op_tlv_init_data));
    CHK_PRT_RETURN(ret == -ENOTSUPP, hccp_warn("[init][ra_hdc_tlv]ra hdc message process unsuccessful ret(%d) phy_id(%u)",
        ret, phy_id), ret);
    CHK_PRT_RETURN(ret != 0, hccp_err("[init][ra_hdc_tlv]ra hdc message process failed ret(%d) phy_id(%u)",
        ret, phy_id), ret);

    tlv_handle->buffer_size = tlv_data.rx_data.buffer_size;
    return ret;
}

int ra_hdc_tlv_deinit(struct ra_tlv_handle *tlv_handle)
{
    unsigned int phy_id = tlv_handle->init_info.phy_id;
    union op_tlv_deinit_data tlv_data = { 0 };
    int ret;

    tlv_data.tx_data.module_type = tlv_handle->module_type;
    tlv_data.tx_data.phy_id = phy_id;

    ret = ra_hdc_process_msg(RA_RS_TLV_DEINIT, phy_id, (char *)&tlv_data, sizeof(union op_tlv_deinit_data));
    CHK_PRT_RETURN(ret != 0, hccp_err("[deinit][ra_hdc_tlv]ra hdc message process failed ret(%d) phy_id(%u)",
        ret, phy_id), ret);

    return 0;
}

STATIC void ra_hdc_tlv_request_head_init(struct ra_tlv_handle *tlv_handle, struct tlv_msg *send_msg,
    struct tlv_request_msg_head *head)
{
    head->module_type = tlv_handle->module_type;
    head->phy_id = tlv_handle->init_info.phy_id;
    head->total_bytes = send_msg->length;
    head->type = send_msg->type;
    head->offset = 0;
}

int ra_hdc_tlv_request(struct ra_tlv_handle *tlv_handle, struct tlv_msg *send_msg, struct tlv_msg *recv_msg)
{
    unsigned int phy_id = tlv_handle->init_info.phy_id;
    union op_tlv_request_data tlv_data = { 0 };
    struct tlv_request_msg_head head = { 0 };
    int ret = 0;

    ra_hdc_tlv_request_head_init(tlv_handle, send_msg, &head);
    while (head.offset < send_msg->length) {
        head.send_bytes = (head.total_bytes - head.offset) >= MAX_TLV_MSG_DATA_LEN ?
            MAX_TLV_MSG_DATA_LEN : (head.total_bytes - head.offset);
        ret = memcpy_s(&(tlv_data.tx_data.head), sizeof(struct tlv_request_msg_head),
            &head, sizeof(struct tlv_request_msg_head));

        (void)memset_s(tlv_data.tx_data.data, MAX_TLV_MSG_DATA_LEN, 0, MAX_TLV_MSG_DATA_LEN);
        ret = memcpy_s(&(tlv_data.tx_data.data), MAX_TLV_MSG_DATA_LEN, (send_msg->data + head.offset), head.send_bytes);
        CHK_PRT_RETURN(ret != 0, hccp_err("[request][ra_hdc_tlv]memcpy_s data failed ret(%d) phy_id(%u) send_bytes(%u)",
            ret, phy_id, head.send_bytes), -ESAFEFUNC);

        ret = ra_hdc_process_msg(RA_RS_TLV_REQUEST, phy_id, (char *)&tlv_data, sizeof(union op_tlv_request_data));
        CHK_PRT_RETURN(ret != 0, hccp_err("[request][ra_hdc_tlv]hdc message process failed ret(%d) phy_id(%u)",
            ret, phy_id), ret);
        head.offset += head.send_bytes;
    }

    recv_msg->length = tlv_data.rx_data.recv_bytes;
    return ret;
}

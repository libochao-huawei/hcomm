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

int RaHdcTlvInit(struct ra_tlv_handle *tlvHandle)
{
    unsigned int phyId = tlvHandle->init_info.phy_id;
    union op_tlv_init_data tlvData = { 0 };
    int ret = 0;

    tlvData.tx_data.module_type = tlvHandle->module_type;
    tlvData.tx_data.phy_id = phyId;

    ret = RaHdcProcessMsg(RA_RS_TLV_INIT, phyId, (char *)&tlvData, sizeof(union op_tlv_init_data));
    CHK_PRT_RETURN(ret == -ENOTSUPP, hccp_warn("[init][ra_hdc_tlv]ra hdc message process unsuccessful ret(%d) phy_id(%u)",
        ret, phyId), ret);
    CHK_PRT_RETURN(ret != 0, hccp_err("[init][ra_hdc_tlv]ra hdc message process failed ret(%d) phy_id(%u)",
        ret, phyId), ret);

    tlvHandle->buffer_size = tlvData.rx_data.buffer_size;
    return ret;
}

int RaHdcTlvDeinit(struct ra_tlv_handle *tlvHandle)
{
    unsigned int phyId = tlvHandle->init_info.phy_id;
    union op_tlv_deinit_data tlvData = { 0 };
    int ret;

    tlvData.tx_data.module_type = tlvHandle->module_type;
    tlvData.tx_data.phy_id = phyId;

    ret = RaHdcProcessMsg(RA_RS_TLV_DEINIT, phyId, (char *)&tlvData, sizeof(union op_tlv_deinit_data));
    CHK_PRT_RETURN(ret != 0, hccp_err("[deinit][ra_hdc_tlv]ra hdc message process failed ret(%d) phy_id(%u)",
        ret, phyId), ret);

    return 0;
}

STATIC void RaHdcTlvRequestHeadInit(struct ra_tlv_handle *tlvHandle, struct tlv_msg *sendMsg,
    struct tlv_request_msg_head *head)
{
    head->module_type = tlvHandle->module_type;
    head->phy_id = tlvHandle->init_info.phy_id;
    head->total_bytes = sendMsg->length;
    head->type = sendMsg->type;
    head->offset = 0;
}

int RaHdcTlvRequest(struct ra_tlv_handle *tlvHandle, struct tlv_msg *sendMsg, struct tlv_msg *recvMsg)
{
    unsigned int phyId = tlvHandle->init_info.phy_id;
    union op_tlv_request_data tlvData = { 0 };
    struct tlv_request_msg_head head = { 0 };
    int ret = 0;

    RaHdcTlvRequestHeadInit(tlvHandle, sendMsg, &head);
    while (head.offset < sendMsg->length) {
        head.send_bytes = (head.total_bytes - head.offset) >= MAX_TLV_MSG_DATA_LEN ?
            MAX_TLV_MSG_DATA_LEN : (head.total_bytes - head.offset);
        ret = memcpy_s(&(tlvData.tx_data.head), sizeof(struct tlv_request_msg_head),
            &head, sizeof(struct tlv_request_msg_head));

        (void)memset_s(tlvData.tx_data.data, MAX_TLV_MSG_DATA_LEN, 0, MAX_TLV_MSG_DATA_LEN);
        ret = memcpy_s(&(tlvData.tx_data.data), MAX_TLV_MSG_DATA_LEN, (sendMsg->data + head.offset), head.send_bytes);
        CHK_PRT_RETURN(ret != 0, hccp_err("[request][ra_hdc_tlv]memcpy_s data failed ret(%d) phy_id(%u) send_bytes(%u)",
            ret, phyId, head.send_bytes), -ESAFEFUNC);

        ret = RaHdcProcessMsg(RA_RS_TLV_REQUEST, phyId, (char *)&tlvData, sizeof(union op_tlv_request_data));
        CHK_PRT_RETURN(ret != 0, hccp_err("[request][ra_hdc_tlv]hdc message process failed ret(%d) phy_id(%u)",
            ret, phyId), ret);
        head.offset += head.send_bytes;
    }

    recvMsg->length = tlvData.rx_data.recv_bytes;
    return ret;
}

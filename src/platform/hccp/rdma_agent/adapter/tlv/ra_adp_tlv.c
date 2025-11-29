/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <stdlib.h>
#include <errno.h>
#include "securec.h"
#include "ra_hdc_tlv.h"
#include "ra_rs_err.h"
#include "rs_tlv.h"
#include "ra_adp.h"
#include "ra_adp_tlv.h"

struct rs_tlv_ops {
    int (*tlv_init)(unsigned int moduleType, unsigned int phyId, unsigned int *bufferSize);
    int (*tlv_deinit)(unsigned int moduleType, unsigned int phyId);
    int (*tlv_request)(struct tlv_request_msg_head *head, char *data);
};

struct rs_tlv_ops gRaRsTlvOps = {
    .tlv_init = RsTlvInit,
    .tlv_deinit = RsTlvDeinit,
    .tlv_request = RsTlvRequest,
};

int RaRsTlvInit(char *inBuf, char *outBuf, int *outLen, int *opResult, int rcvBufLen)
{
    union op_tlv_init_data *dataOut = (union op_tlv_init_data *)(outBuf + sizeof(struct msg_head));
    union op_tlv_init_data *dataIn = (union op_tlv_init_data *)(inBuf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_tlv_init_data), sizeof(struct msg_head), rcvBufLen, opResult);

    *opResult = gRaRsTlvOps.tlv_init(dataIn->tx_data.module_type,
        dataIn->tx_data.phy_id, &dataOut->rx_data.buffer_size);
    CHK_PRT_RETURN(*opResult == -ENOTSUPP, hccp_warn("tlv_init unsuccessful ret[%d]", *opResult), 0);
    if (*opResult != 0) {
        hccp_err("tlv_init failed ret[%d]", *opResult);
    }

    return 0;
}

int RaRsTlvDeinit(char *inBuf, char *outBuf, int *outLen, int *opResult, int rcvBufLen)
{
    union op_tlv_deinit_data *dataIn = (union op_tlv_deinit_data *)(inBuf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_tlv_deinit_data), sizeof(struct msg_head), rcvBufLen, opResult);

    *opResult = gRaRsTlvOps.tlv_deinit(dataIn->tx_data.module_type, dataIn->tx_data.phy_id);
    if (*opResult != 0) {
        hccp_err("tlv_deinit failed ret[%d]", *opResult);
    }

    return 0;
}

int RaRsTlvRequest(char *inBuf, char *outBuf, int *outLen, int *opResult, int rcvBufLen)
{
    union op_tlv_request_data *dataIn = (union op_tlv_request_data *)(inBuf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_tlv_request_data), sizeof(struct msg_head), rcvBufLen, opResult);

    *opResult = gRaRsTlvOps.tlv_request(&dataIn->tx_data.head, dataIn->tx_data.data);
    if (*opResult != 0) {
        hccp_err("tlv_request failed ret[%d]", *opResult);
    }

    return 0;
}

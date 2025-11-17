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
    int (*tlv_init)(unsigned int module_type, unsigned int phy_id, unsigned int *buffer_size);
    int (*tlv_deinit)(unsigned int module_type, unsigned int phy_id);
    int (*tlv_request)(struct tlv_request_msg_head *head, char *data);
};

struct rs_tlv_ops g_ra_rs_tlv_ops = {
    .tlv_init = rs_tlv_init,
    .tlv_deinit = rs_tlv_deinit,
    .tlv_request = rs_tlv_request,
};

int ra_rs_tlv_init(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_tlv_init_data *data_out = (union op_tlv_init_data *)(out_buf + sizeof(struct msg_head));
    union op_tlv_init_data *data_in = (union op_tlv_init_data *)(in_buf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_tlv_init_data), sizeof(struct msg_head), rcv_buf_len, op_result);

    *op_result = g_ra_rs_tlv_ops.tlv_init(data_in->tx_data.module_type,
        data_in->tx_data.phy_id, &data_out->rx_data.buffer_size);
    CHK_PRT_RETURN(*op_result == -ENOTSUPP, hccp_warn("tlv_init unsuccessful ret[%d]", *op_result), 0);
    if (*op_result != 0) {
        hccp_err("tlv_init failed ret[%d]", *op_result);
    }

    return 0;
}

int ra_rs_tlv_deinit(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_tlv_deinit_data *data_in = (union op_tlv_deinit_data *)(in_buf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_tlv_deinit_data), sizeof(struct msg_head), rcv_buf_len, op_result);

    *op_result = g_ra_rs_tlv_ops.tlv_deinit(data_in->tx_data.module_type, data_in->tx_data.phy_id);
    if (*op_result != 0) {
        hccp_err("tlv_deinit failed ret[%d]", *op_result);
    }

    return 0;
}

int ra_rs_tlv_request(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_tlv_request_data *data_in = (union op_tlv_request_data *)(in_buf + sizeof(struct msg_head));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_tlv_request_data), sizeof(struct msg_head), rcv_buf_len, op_result);

    *op_result = g_ra_rs_tlv_ops.tlv_request(&data_in->tx_data.head, data_in->tx_data.data);
    if (*op_result != 0) {
        hccp_err("tlv_request failed ret[%d]", *op_result);
    }

    return 0;
}

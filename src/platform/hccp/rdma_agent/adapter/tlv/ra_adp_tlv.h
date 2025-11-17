/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef RA_ADP_TLV_H
#define RA_ADP_TLV_H

int ra_rs_tlv_init(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len);
int ra_rs_tlv_deinit(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len);
int ra_rs_tlv_request(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len);
#endif // RA_ADP_TLV_H

/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "ut_dispatch.h"
#include "ra_hdc_tlv.h"
#include "rs_tlv.h"
#include "ra_adp_tlv.h"
#include "ra_rs_err.h"

#define TC_TLV_HDC_MSG_SIZE    (32 * 1024)

void tc_ra_rs_tlv_init()
{
    union OpTlvInitData data_in;
    union OpTlvInitData data_out;
    int rcv_buf_len = 0;
    int op_result;
    int out_len;
    int ret;

    char* in_buf = calloc(1, sizeof(struct MsgHead) + sizeof(union OpTlvInitData));
    char* out_buf = calloc(1, sizeof(struct MsgHead) + sizeof(union OpTlvInitData));

    data_in.txData.phyId = 0;
    mocker((stub_fn_t)RsTlvInit, 1, 0);
    memcpy_s(in_buf + sizeof(struct MsgHead), sizeof(union OpTlvInitData),
        &data_in, sizeof(union OpTlvInitData));
    ret = RaRsTlvInit(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    mocker((stub_fn_t)RsTlvInit, 1, -1);
    ret = RaRsTlvInit(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    mocker((stub_fn_t)RsTlvInit, 1, -ENOTSUPP);
    ret = RaRsTlvInit(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    free(in_buf);
    in_buf = NULL;
    free(out_buf);
    out_buf = NULL;
}

void tc_ra_rs_tlv_deinit()
{
    union OpTlvDeinitData data_in;
    union OpTlvDeinitData data_out;
    int rcv_buf_len = 0;
    int op_result;
    int out_len;
    int ret;

    char* in_buf = calloc(1, sizeof(struct MsgHead) + sizeof(union OpTlvDeinitData));
    char* out_buf = calloc(1, sizeof(struct MsgHead) + sizeof(union OpTlvDeinitData));

    data_in.txData.phyId = 0;
    mocker((stub_fn_t)RsTlvDeinit, 1, 0);
    memcpy_s(in_buf + sizeof(struct MsgHead), sizeof(union OpTlvDeinitData),
        &data_in, sizeof(union OpTlvDeinitData));
    ret = RaRsTlvDeinit(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    mocker((stub_fn_t)RsTlvDeinit, 1, -1);
    ret = RaRsTlvDeinit(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    free(in_buf);
    in_buf = NULL;
    free(out_buf);
    out_buf = NULL;
}

void tc_ra_rs_tlv_request()
{
    union OpTlvRequestData data_in;
    union OpTlvRequestData data_out;
    int rcv_buf_len = 0;
    int op_result;
    int out_len;
    int ret;

    char* in_buf = calloc(1, sizeof(struct MsgHead) + sizeof(union OpTlvRequestData));
    char* out_buf = calloc(1, sizeof(struct MsgHead) + sizeof(union OpTlvRequestData));

    data_in.txData.head.moduleType = TLV_MODULE_TYPE_NSLB;
    data_in.txData.head.phyId = 0;
    mocker((stub_fn_t)RsTlvRequest, 1, 0);
    memcpy_s(in_buf + sizeof(struct MsgHead), sizeof(union OpTlvRequestData),
        &data_in, sizeof(union OpTlvRequestData));
    ret = RaRsTlvRequest(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    mocker((stub_fn_t)RsTlvRequest, 1, -1);
    ret = RaRsTlvRequest(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    free(in_buf);
    in_buf = NULL;
    free(out_buf);
    out_buf = NULL;
}

#include "ut_dispatch.h"
#include "ra_hdc_tlv.h"
#include "rs_tlv.h"
#include "ra_adp_tlv.h"
#include "ra_rs_err.h"

#define TC_TLV_HDC_MSG_SIZE    (32 * 1024) // 32KB

void tc_ra_rs_tlv_init()
{
    union op_tlv_init_data data_in;
    union op_tlv_init_data data_out;
    int rcv_buf_len = 0;
    int op_result;
    int out_len;
    int ret;

    char* in_buf = calloc(1, sizeof(struct msg_head) + sizeof(union op_tlv_init_data));
    char* out_buf = calloc(1, sizeof(struct msg_head) + sizeof(union op_tlv_init_data));

    data_in.tx_data.module_type = TLV_MODULE_TYPE_NSLB;
    data_in.tx_data.phy_id = 0;
    mocker((stub_fn_t)rs_tlv_init, 1, 0);
    memcpy_s(in_buf + sizeof(struct msg_head), sizeof(union op_tlv_init_data),
        &data_in, sizeof(union op_tlv_init_data));
    ret = ra_rs_tlv_init(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    mocker((stub_fn_t)rs_tlv_init, 1, -1);
    ret = ra_rs_tlv_init(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    mocker((stub_fn_t)rs_tlv_init, 1, -ENOTSUPP);
    ret = ra_rs_tlv_init(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    free(in_buf);
    in_buf = NULL;
    free(out_buf);
    out_buf = NULL;
}

void tc_ra_rs_tlv_deinit()
{
    union op_tlv_deinit_data data_in;
    union op_tlv_deinit_data data_out;
    int rcv_buf_len = 0;
    int op_result;
    int out_len;
    int ret;

    char* in_buf = calloc(1, sizeof(struct msg_head) + sizeof(union op_tlv_deinit_data));
    char* out_buf = calloc(1, sizeof(struct msg_head) + sizeof(union op_tlv_deinit_data));

    data_in.tx_data.module_type = TLV_MODULE_TYPE_NSLB;
    data_in.tx_data.phy_id = 0;
    mocker((stub_fn_t)rs_tlv_deinit, 1, 0);
    memcpy_s(in_buf + sizeof(struct msg_head), sizeof(union op_tlv_deinit_data),
        &data_in, sizeof(union op_tlv_deinit_data));
    ret = ra_rs_tlv_deinit(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    mocker((stub_fn_t)rs_tlv_deinit, 1, -1);
    ret = ra_rs_tlv_deinit(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    free(in_buf);
    in_buf = NULL;
    free(out_buf);
    out_buf = NULL;
}

void tc_ra_rs_tlv_request()
{
    union op_tlv_request_data data_in;
    union op_tlv_request_data data_out;
    int rcv_buf_len = 0;
    int op_result;
    int out_len;
    int ret;

    char* in_buf = calloc(1, sizeof(struct msg_head) + sizeof(union op_tlv_request_data));
    char* out_buf = calloc(1, sizeof(struct msg_head) + sizeof(union op_tlv_request_data));

    data_in.tx_data.head.module_type = TLV_MODULE_TYPE_NSLB;
    data_in.tx_data.head.phy_id = 0;
    mocker((stub_fn_t)rs_tlv_request, 1, 0);
    memcpy_s(in_buf + sizeof(struct msg_head), sizeof(union op_tlv_request_data),
        &data_in, sizeof(union op_tlv_request_data));
    ret = ra_rs_tlv_request(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    mocker((stub_fn_t)rs_tlv_request, 1, -1);
    ret = ra_rs_tlv_request(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    free(in_buf);
    in_buf = NULL;
    free(out_buf);
    out_buf = NULL;
}
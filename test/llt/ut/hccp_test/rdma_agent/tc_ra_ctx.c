/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: ra ctx unit test.
 * Author:
 * Create: 2025-03-31
 */

#include <errno.h>
#include <stdlib.h>
#include <pthread.h>
#include "securec.h"
#include "ut_dispatch.h"
#include "hccp_common.h"
#include "hccp_ctx.h"
#include "ra.h"
#include "ra_ctx.h"
#include "ra_adp.h"
#include "ra_adp_async.h"
#include "ra_adp_pool.h"
#include "ra_hdc.h"
#include "ra_hdc_ctx.h"
#include "ra_hdc_async.h"
#include "ra_hdc_async_ctx.h"
#include "ra_hdc_async_socket.h"
#include "ra_hdc_socket.h"
#include "rs_ctx.h"
#include "tc_ra_ctx.h"

extern bool ra_is_protocol_udma(unsigned int phy_id);
extern int ra_get_chip_protocol(unsigned int phy_id, enum protocol_type *protocol);
extern void hdc_async_set_req_done(struct ra_request_handle *req_handle, unsigned int phy_id, int ret);
extern int dl_drv_device_get_index_by_phy_id(uint32_t phyId, uint32_t *devIndex);
extern int dl_hal_get_chip_info(unsigned int dev_id, halChipInfo *chip_info);
extern hdcError_t dl_drv_hdc_alloc_msg(HDC_SESSION session, struct drvHdcMsg **ppMsg, int count);
extern hdcError_t dl_hal_hdc_recv(HDC_SESSION session, struct drvHdcMsg *pMsg, int bufLen, UINT64 flag,
    int *recvBufCount, UINT32 timeout);
extern hdcError_t dl_drv_hdc_get_msg_buffer(struct drvHdcMsg *msg, int index, char **pBuf, int *pLen);
extern hdcError_t dl_drv_hdc_free_msg(struct drvHdcMsg *msg);
extern int ra_hdc_handle_send_pkt(unsigned int chip_id, void *recv_buf, unsigned int recv_len);
extern int dl_drv_device_get_index_by_phy_id(uint32_t phyId, uint32_t *devIndex);
extern int dl_hal_get_chip_info(unsigned int dev_id, halChipInfo *chip_info);

extern struct ra_ctx_ops g_ra_hdc_ctx_ops;

void tc_ra_get_dev_eid_info_num()
{
    struct ra_info info = {0};
    unsigned int num = 0;
    int ret = 0;

    mocker_clean();
    info.mode = NETWORK_OFFLINE;
    mocker(ra_is_protocol_udma, 1, 1);
    mocker(ra_hdc_process_msg, 1, 0);
    ret = ra_get_dev_eid_info_num(info, &num);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();
    mocker(dl_drv_device_get_index_by_phy_id, 1, 0);
    mocker(dl_hal_get_chip_info, 1, 0);
    ret = ra_is_protocol_udma(0);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();
}

void tc_ra_get_dev_eid_info_list()
{
    struct dev_eid_info info_list[35] = {0};
    struct ra_info info = {0};
    unsigned int num = 35;
    int ret = 0;

    mocker_clean();
    info.mode = NETWORK_OFFLINE;
    mocker(ra_is_protocol_udma, 1, 1);
    mocker(ra_hdc_process_msg, 2, 0);
    ret = ra_get_dev_eid_info_list(info, info_list, &num);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();
}

int stub_ra_get_chip_protocol(unsigned int phy_id, enum protocol_type *protocol)
{
    *protocol = PROTOCOL_UDMA;
    return 0;
}

int stub_dl_hal_get_chip_info(unsigned int dev_id, halChipInfo *chip_info)
{
    strncpy_s(chip_info->name, 32,"910_96", 7);
    return 0;
}

void tc_ra_ctx_init()
{
    struct ctx_init_attr attr = {0};
    struct ctx_init_cfg cfg = {0};
    void *ctx_handle = NULL;
    int ret = 0;

    mocker_clean();
    mocker_invoke(ra_get_chip_protocol, stub_ra_get_chip_protocol, 1);
    mocker(dl_drv_device_get_index_by_phy_id, 1, 0);
    mocker_invoke(dl_hal_get_chip_info, stub_dl_hal_get_chip_info, 1);
    mocker(ra_hdc_process_msg, 1, 0);
    cfg.mode = NETWORK_OFFLINE;
    ret = ra_ctx_init(&cfg, &attr, &ctx_handle);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();
    free(ctx_handle);
}

void tc_ra_get_dev_base_attr()
{
    struct ra_ctx_handle ctx_handle = {0};
    struct dev_base_attr attr = {0};
    int ret = 0;

    ret = ra_get_dev_base_attr(&ctx_handle, &attr);
    EXPECT_INT_EQ(0, ret);
}

void tc_ra_ctx_deinit()
{
    struct ra_ctx_handle *ctx_handle = malloc(sizeof(struct ra_ctx_handle));
    int ret = 0;

    mocker_clean();
    ctx_handle->ctx_ops = &g_ra_hdc_ctx_ops;
    mocker(ra_hdc_process_msg, 1, 0);
    ret = ra_ctx_deinit(ctx_handle);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();
}

void tc_ra_ctx_lmem_register()
{
    struct ra_ctx_handle ctx_handle = {0};
    struct mr_reg_info_t lmem_info = {0};
    void *lmem_handle = NULL;
    int ret = 0;

    mocker_clean();
    ctx_handle.ctx_ops = &g_ra_hdc_ctx_ops;
    mocker(ra_hdc_process_msg, 3, 0);
    ctx_handle.protocol = PROTOCOL_RDMA;
    ret = ra_ctx_lmem_register(&ctx_handle, &lmem_info, &lmem_handle);
    EXPECT_INT_EQ(0, ret);
    free(lmem_handle);
    ctx_handle.protocol = PROTOCOL_UDMA;
    ret = ra_ctx_lmem_register(&ctx_handle, &lmem_info, &lmem_handle);
    EXPECT_INT_EQ(0, ret);
    ret = ra_ctx_lmem_unregister(&ctx_handle, lmem_handle);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();
}

void tc_ra_ctx_rmem_import()
{
    struct mr_import_info_t rmem_info = {0};
    struct ra_ctx_handle ctx_handle = {0};
    void *rmem_handle = NULL;
    int ret = 0;

    mocker_clean();
    ctx_handle.ctx_ops = &g_ra_hdc_ctx_ops;
    rmem_info.in.key.size = 4;
    ctx_handle.protocol = PROTOCOL_UDMA;
    mocker(ra_hdc_process_msg, 2, 0);
    ret = ra_ctx_rmem_import(&ctx_handle, &rmem_info, &rmem_handle);
    EXPECT_INT_EQ(0, ret);
    ret = ra_ctx_rmem_unimport(&ctx_handle, rmem_handle);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();
}

void tc_ra_ctx_chan_create()
{
    struct ra_ctx_handle ctx_handle = {0};
    struct chan_info_t chan_info = {0};
    void *chan_handle = NULL;
    int ret = 0;

    mocker_clean();
    ctx_handle.ctx_ops = &g_ra_hdc_ctx_ops;
    mocker(ra_hdc_process_msg, 2, 0);
    ret = ra_ctx_chan_create(&ctx_handle, &chan_info, &chan_handle);
    EXPECT_INT_EQ(0, ret);
    ret = ra_ctx_chan_destroy(&ctx_handle, chan_handle);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();
}

void tc_ra_ctx_cq_create()
{
    struct ra_ctx_handle ctx_handle = {0};
    struct cq_info_t cq_info_t = {0};
    void *cq_handle = NULL;
    int ret = 0;

    mocker_clean();
    ctx_handle.ctx_ops = &g_ra_hdc_ctx_ops;
    mocker(ra_hdc_process_msg, 5, 0);

    ctx_handle.protocol = PROTOCOL_UDMA;
    ret = ra_ctx_cq_create(&ctx_handle, &cq_info_t, &cq_handle);
    EXPECT_INT_EQ(0, ret);
    ret = ra_ctx_cq_destroy(&ctx_handle, cq_handle);
    EXPECT_INT_EQ(0, ret);

    // new case
    ctx_handle.protocol = PROTOCOL_UDMA;
    cq_info_t.in.ub.ccu_ex_cfg.valid = 1;
    cq_info_t.in.ub.mode = JFC_MODE_CCU_POLL;
    ret = ra_ctx_cq_create(&ctx_handle, &cq_info_t, &cq_handle);
    EXPECT_INT_EQ(0, ret);
    ret = ra_ctx_cq_destroy(&ctx_handle, cq_handle);
    EXPECT_INT_EQ(0, ret);

    ctx_handle.protocol = PROTOCOL_RDMA;
    ret = ra_ctx_cq_create(&ctx_handle, &cq_info_t, &cq_handle);
    EXPECT_INT_EQ(0, ret);

    ret = ra_ctx_cq_create(&ctx_handle, &cq_info_t, NULL);
    EXPECT_INT_EQ(conver_return_code(RDMA_OP, -EINVAL), ret);

    free(cq_handle);
    mocker_clean();
}

void tc_ra_ctx_token_id_alloc()
{
    tc_ra_ctx_token_id_alloc1();
    tc_ra_ctx_token_id_alloc2();
    tc_ra_ctx_token_id_alloc3();
}

void tc_ra_ctx_token_id_alloc1()
{
    struct ra_ctx_handle ctx_handle = {0};
    struct hccp_token_id info = {0};
    void *token_id_handle = NULL;
    int ret;

    mocker_clean();
    ctx_handle.ctx_ops = &g_ra_hdc_ctx_ops;
    mocker(ra_hdc_process_msg, 3, 0);
    ctx_handle.protocol = PROTOCOL_UDMA;
    ret = ra_ctx_token_id_alloc(&ctx_handle, &info, NULL);
    EXPECT_INT_NE(0, ret);
    ret = ra_ctx_token_id_free(NULL, NULL);
    EXPECT_INT_NE(0, ret);

    ret = ra_ctx_token_id_alloc(&ctx_handle, &info, &token_id_handle);
    EXPECT_INT_EQ(0, ret);
    ret = ra_ctx_token_id_free(&ctx_handle, token_id_handle);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();
}

void tc_ra_ctx_token_id_alloc2()
{
    struct ra_ctx_handle ctx_handle = {0};
    struct hccp_token_id info = {0};
    void *token_id_handle = NULL;
    int ret;

    mocker_clean();
    ctx_handle.ctx_ops = &g_ra_hdc_ctx_ops;
    mocker(ra_hdc_process_msg, 3, 0);
    ctx_handle.protocol = PROTOCOL_UDMA;

    mocker(ra_hdc_ctx_token_id_alloc, 10, -EPERM);
    ret = ra_ctx_token_id_alloc(&ctx_handle, &info, &token_id_handle);
    EXPECT_INT_NE(0, ret);
    mocker_clean();
}

void tc_ra_ctx_token_id_alloc3()
{
    struct ra_ctx_handle ctx_handle = {0};
    struct hccp_token_id info = {0};
    void *token_id_handle = NULL;
    int ret;

    mocker_clean();
    ctx_handle.ctx_ops = &g_ra_hdc_ctx_ops;
    mocker(ra_hdc_process_msg, 3, 0);
    ctx_handle.protocol = PROTOCOL_UDMA;

    ret = ra_ctx_token_id_alloc(&ctx_handle, &info, &token_id_handle);
    EXPECT_INT_EQ(0, ret);

    mocker(ra_hdc_ctx_token_id_free, 10, -EPERM);
    ret = ra_ctx_token_id_free(&ctx_handle, token_id_handle);
    EXPECT_INT_NE(0, ret);
    mocker_clean();
}

void tc_ra_ctx_qp_create()
{
    struct ra_ctx_qp_handle *qp_handle = NULL;
    struct ra_ctx_handle ctx_handle = {0};
    struct ra_cq_handle scq_handle = {0};
    struct ra_cq_handle rcq_handle = {0};
    struct qp_create_attr attr = {0};
    struct qp_create_info info = {0};
    void *cq_handle = NULL;
    int ret = 0;

    mocker_clean();
    attr.scq_handle = &scq_handle;
    attr.rcq_handle = &rcq_handle;
    ctx_handle.ctx_ops = &g_ra_hdc_ctx_ops;
    mocker(ra_hdc_process_msg, 5, 0);

    ctx_handle.protocol = PROTOCOL_UDMA;
    ret = ra_ctx_qp_create(&ctx_handle, &attr, &info, &qp_handle);
    EXPECT_INT_EQ(0, ret);
    ret = ra_ctx_qp_destroy(qp_handle);
    EXPECT_INT_EQ(0, ret);

    // new case
    attr.ub.mode = JETTY_MODE_CCU_TA_CACHE;
    attr.ub.ta_cache_mode.lock_flag = 1;
    ret = ra_ctx_qp_create(&ctx_handle, &attr, &info, &qp_handle);
    EXPECT_INT_EQ(0, ret);
    ret = ra_ctx_qp_destroy(qp_handle);
    EXPECT_INT_EQ(0, ret);  

    ctx_handle.protocol = PROTOCOL_RDMA;
    ret = ra_ctx_qp_create(&ctx_handle, &attr, &info, &qp_handle);
    EXPECT_INT_EQ(0, ret);

    ret = ra_ctx_qp_create(&ctx_handle, &attr, &info, NULL);
    EXPECT_INT_EQ(conver_return_code(RDMA_OP, -EINVAL), ret);

    free(qp_handle);
    mocker_clean();
}

void tc_ra_ctx_qp_import()
{
    struct ra_ctx_handle ctx_handle = {0};
    struct qp_import_info_t qp_info = {0};
    struct ra_ctx_rem_qp_handle *rem_qp_handle = NULL;
    int ret = 0;

    mocker_clean();
    ctx_handle.ctx_ops = &g_ra_hdc_ctx_ops;
    mocker(ra_hdc_process_msg, 2, 0);
    ctx_handle.protocol = PROTOCOL_UDMA;
    ret = ra_ctx_qp_import(&ctx_handle, &qp_info, &rem_qp_handle);
    EXPECT_INT_EQ(0, ret);
    ret = ra_ctx_qp_unimport(&ctx_handle, rem_qp_handle);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();
}

void tc_ra_ctx_qp_bind()
{
    struct ra_ctx_rem_qp_handle rem_qp_handle = {0};
    struct ra_ctx_qp_handle qp_handle = {0};
    struct ra_ctx_handle ctx_handle = {0};
    int ret = 0;

    mocker_clean();
    rem_qp_handle.protocol = PROTOCOL_UDMA;
    qp_handle.ctx_handle = &ctx_handle;
    ctx_handle.ctx_ops = &g_ra_hdc_ctx_ops;
    mocker(ra_hdc_process_msg, 2, 0);
    ret = ra_ctx_qp_bind(&qp_handle, &rem_qp_handle);
    EXPECT_INT_EQ(0, ret);
    ret = ra_ctx_qp_unbind(&qp_handle);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();
    mocker(ra_hdc_process_msg, 2, -ENODEV);
    ret = ra_ctx_qp_unbind(&qp_handle);
    EXPECT_INT_EQ(conver_return_code(RDMA_OP, -ENODEV), ret);
    mocker_clean();
}

void tc_ra_batch_send_wr()
{
    struct ra_ctx_rem_qp_handle rem_qp_handle = {0};
    struct ra_lmem_handle rmem_handle = {0};
    struct ra_ctx_qp_handle qp_handle = {0};
    struct ra_ctx_handle ctx_handle = {0};
    struct send_wr_data wr_list[1] = {0};
    struct send_wr_resp op_resp[1] = {0};

    unsigned int complete_num = 0;
    int inline_data = 0;
    int ret = 0;

    mocker_clean();
    qp_handle.ctx_handle = &ctx_handle;
    ctx_handle.ctx_ops = &g_ra_hdc_ctx_ops;
    wr_list[0].rmem_handle = &rmem_handle;
    qp_handle.protocol = PROTOCOL_RDMA;
    wr_list[0].rdma.flags = RA_SEND_INLINE;
    wr_list[0].inline_data = &inline_data;
    mocker(ra_hdc_process_msg, 2, 0);
    ret = ra_batch_send_wr(&qp_handle, wr_list, op_resp, 1, &complete_num);
    EXPECT_INT_EQ(0, ret);
    qp_handle.protocol = PROTOCOL_UDMA;
    wr_list[0].ub.flags.bs.inline_flag = 1;
    wr_list[0].ub.rem_qp_handle = &rem_qp_handle;
    wr_list[0].ub.opcode = RA_UB_OPC_WRITE;
    ret = ra_batch_send_wr(&qp_handle, wr_list, op_resp, 1, &complete_num);
    EXPECT_INT_EQ(0, ret);
}

void tc_ra_ctx_update_ci()
{
    struct ra_ctx_qp_handle qp_handle = {0};
    struct ra_ctx_handle ctx_handle = {0};
    int ret = 0;

    mocker_clean();
    mocker(ra_hdc_process_msg, 1, 0);
    qp_handle.ctx_handle = &ctx_handle;
    ctx_handle.ctx_ops = &g_ra_hdc_ctx_ops;
    ret = ra_ctx_update_ci(&qp_handle, 1);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();
}

void tc_ra_custom_channel()
{
    struct custom_chan_info_out out = {0};
    struct custom_chan_info_in in = {0};
    struct ra_info info = {0};
    int ret = 0;

    mocker_clean();
    mocker(ra_hdc_process_msg, 1, 0);
    info.mode = NETWORK_OFFLINE;
    ret = ra_custom_channel(info, &in, &out);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();
}

void tc_ra_get_tp_info_list_async()
{
    struct tp_info info_list[HCCP_MAX_TPID_INFO_NUM] = {0};
    struct ra_request_handle *req_handle = NULL;
    struct ra_ctx_handle ctx_handle = {0};
    struct get_tp_cfg cfg = {0};
    unsigned int num = 0;
    int ret = 0;

    ret = ra_get_tp_info_list_async(NULL, &cfg, &info_list, &num, &req_handle);
    EXPECT_INT_NE(0, ret);

    ret = ra_get_tp_info_list_async(&ctx_handle, NULL, &info_list, &num, &req_handle);
    EXPECT_INT_NE(0, ret);

    ret = ra_get_tp_info_list_async(&ctx_handle, &cfg, NULL, &num, &req_handle);
    EXPECT_INT_NE(0, ret);

    ret = ra_get_tp_info_list_async(&ctx_handle, &cfg, &info_list, NULL, &req_handle);
    EXPECT_INT_NE(0, ret);

    ret = ra_get_tp_info_list_async(&ctx_handle, &cfg, &info_list, &num, NULL);
    EXPECT_INT_NE(0, ret);

    ret = ra_get_tp_info_list_async(&ctx_handle, &cfg, &info_list, &num, &req_handle);
    EXPECT_INT_NE(0, ret);

    num = HCCP_MAX_TPID_INFO_NUM + 1;
    ret = ra_get_tp_info_list_async(&ctx_handle, &cfg, &info_list, &num, &req_handle);
    EXPECT_INT_NE(0, ret);

    num = 1;
    mocker(ra_hdc_get_tp_info_list_async, 1, 0);
    ret = ra_get_tp_info_list_async(&ctx_handle, &cfg, &info_list, &num, &req_handle);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();
}

void tc_ra_hdc_get_tp_info_list_async()
{
    struct tp_info info_list[HCCP_MAX_TPID_INFO_NUM] = {0};
    struct ra_response_tp_info_list *async_rsp = NULL;
    union op_get_tp_info_list_data recv_buf = {0};
    struct ra_request_handle *req_handle = NULL;
    struct ra_ctx_handle ctx_handle = {0};
    struct get_tp_cfg cfg = {0};
    unsigned int num = 1;
    int ret = 0;

    mocker(ra_hdc_send_msg_async, 1, -1);
    ret =  ra_hdc_get_tp_info_list_async(&ctx_handle, &cfg, &info_list, &num, &req_handle);
    EXPECT_INT_NE(0, ret);
    mocker_clean();

    mocker(ra_hdc_send_msg_async, 1, 0);
    ret =  ra_hdc_get_tp_info_list_async(&ctx_handle, &cfg, &info_list, &num, &req_handle);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();

    req_handle->op_ret = -1;
    ra_hdc_async_handle_tp_info_list(req_handle);

    // tp_num = 0
    req_handle->op_ret = 0;
    req_handle->recv_buf = &recv_buf;
    async_rsp = (struct ra_response_tp_info_list *)calloc(1, sizeof(struct ra_response_tp_info_list));
    async_rsp->num = &num;
    async_rsp->info_list = info_list;
    req_handle->priv_data = (void *)async_rsp;
    ra_hdc_async_handle_tp_info_list(req_handle);

    req_handle->op_ret = 0;
    recv_buf.rx_data.num = 1;
    req_handle->recv_buf = &recv_buf;
    async_rsp = (struct ra_response_tp_info_list *)calloc(1, sizeof(struct ra_response_tp_info_list));
    async_rsp->num = &num;
    async_rsp->info_list = info_list;
    req_handle->priv_data = (void *)async_rsp;
    mocker(memcpy_s, 1, -1);
    ra_hdc_async_handle_tp_info_list(req_handle);
    mocker_clean();

    req_handle->op_ret = 0;
    recv_buf.rx_data.num = 1;
    req_handle->recv_buf = &recv_buf;
    async_rsp = (struct ra_response_tp_info_list *)calloc(1, sizeof(struct ra_response_tp_info_list));
    async_rsp->num = &num;
    async_rsp->info_list = info_list;
    req_handle->priv_data = (void *)async_rsp;
    mocker(memcpy_s, 1, 0);
    ra_hdc_async_handle_tp_info_list(req_handle);
    mocker_clean();

    free(req_handle);
    req_handle = NULL;
}

void tc_ra_rs_get_tp_info_list()
{
    union op_get_tp_info_list_data data_in;
    union op_get_tp_info_list_data data_out;
    int rcv_buf_len = 0;
    int op_result;
    int out_len;
    int ret;

    char* in_buf = calloc(1, sizeof(struct msg_head) + sizeof(union op_get_tp_info_list_data));
    char* out_buf = calloc(1, sizeof(struct msg_head) + sizeof(union op_get_tp_info_list_data));
    
    data_in.tx_data.phy_id = 0;
    data_in.tx_data.dev_index = 0;
    memcpy_s(in_buf + sizeof(struct msg_head), sizeof(union op_get_tp_info_list_data),
        &data_in, sizeof(union op_get_tp_info_list_data));
    ret = ra_rs_get_tp_info_list(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    mocker(rs_get_tp_info_list, 1, -1);
    ret = ra_rs_get_tp_info_list(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    free(in_buf);
    in_buf = NULL;
    free(out_buf);
    out_buf = NULL;
}

void tc_ra_rs_async_hdc_session_connect()
{
    ra_hw_async_init(0, 0);
    union op_async_hdc_connect_data connect_data = {0};
    connect_data.tx_data.phy_id = 0;
    connect_data.tx_data.queue_size = MAX_POOL_QUEUE_SIZE;
    connect_data.tx_data.thread_num = MAX_POOL_THREAD_NUM;
    unsigned int connect_data_size = sizeof(union op_async_hdc_connect_data);

    void *send_rcv_buf = NULL;
    unsigned int send_rcv_len;
    int ret;
    pid_t host_tgid = 0;
    unsigned int opcode = RA_RS_ASYNC_HDC_SESSION_CONNECT;
    send_rcv_len = sizeof(struct msg_head) + connect_data_size;
    send_rcv_buf = (void *)calloc(send_rcv_len, sizeof(char));
    msg_head_build_up(send_rcv_buf, opcode, 0, connect_data_size, host_tgid);

    ret = memcpy_s(send_rcv_buf + sizeof(struct msg_head), send_rcv_len - sizeof(struct msg_head), &connect_data, connect_data_size);
    if (ret) {
        hccp_err("[process][ra_hdc_msg]memcpy_s failed, ret(%d) phy_id(%u)", ret, connect_data.tx_data.phy_id);
        return;
    }
    int op_ret = 0;
    void *send_buf = NULL;
    int snd_buf_len = 0;

    struct msg_head *recv_msg_head = (struct msg_head *)send_rcv_buf;
    send_buf = (char *)calloc(sizeof(char), recv_msg_head->msg_data_len + sizeof(struct msg_head));
    CHK_PRT_RETURN(send_buf == NULL, hccp_err("calloc fail."), -ENOMEM);

    mocker(ra_hdc_async_recv_pkt, 1, -1);
    ra_rs_async_hdc_session_connect(send_rcv_buf, send_buf, snd_buf_len, &op_ret, send_rcv_len);

    union op_async_hdc_close_data close_data = {0};
    unsigned int close_data_size = sizeof(union op_async_hdc_close_data);
    void *send_rcv_buf2 = NULL;
    unsigned int send_rcv_len2;
    unsigned int opcode2 = RA_RS_ASYNC_HDC_SESSION_CLOSE;
    send_rcv_len2 = sizeof(struct msg_head) + close_data_size;
    send_rcv_buf2 = (void *)calloc(send_rcv_len, sizeof(char));
    msg_head_build_up(send_rcv_buf2, opcode2, 0, close_data_size, host_tgid);
    ret = memcpy_s(send_rcv_buf2 + sizeof(struct msg_head), send_rcv_len2 - sizeof(struct msg_head), &close_data, close_data_size);

    int op_ret2 = 0;
    void *send_buf2 = NULL;
    int snd_buf_len2 = 0;
 
    ra_rs_async_hdc_session_close(send_rcv_buf2, send_buf2, snd_buf_len2, &op_ret2, send_rcv_len2);
    ra_hw_async_deinit();

    free(send_rcv_buf);
    free(send_buf);
    free(send_rcv_buf2);

    mocker_clean();
}

void tc_ra_hdc_async_send_pkt()
{
    char *data;
    data = (char *)calloc(100, sizeof(char));
    unsigned long long size = 100;
    union op_socket_send_data *async_data = NULL;
    async_data = (union op_socket_send_data *)calloc(sizeof(union op_socket_send_data), sizeof(char));
    async_data->tx_data.fd = 0;
    async_data->tx_data.send_size = size;
    memcpy_s(async_data->tx_data.data_send, SOCKET_SEND_MAXLEN, data, size);

    void *send_buf = NULL;
    unsigned int send_len;
    int ret;
    pid_t host_tgid = 0;
    unsigned int opcode = RA_RS_SOCKET_SEND;
    send_len = sizeof(struct msg_head) + sizeof(union op_socket_send_data);
    send_buf = (void *)calloc(send_len, sizeof(char));
    msg_head_build_up(send_buf, opcode, 0, sizeof(union op_socket_send_data), host_tgid);

    memcpy_s(send_buf + sizeof(struct msg_head), send_len - sizeof(struct msg_head), async_data, sizeof(union op_socket_send_data));
    msg_head_build_up(send_buf, opcode, 0, sizeof(union op_socket_send_data), host_tgid);

    ra_hdc_handle_send_pkt(0, send_buf, send_len);

    free(data);
    free(async_data);
    free(send_buf);
}
 
void tc_ra_hdc_async_recv_pkt()
{
    struct ra_hdc_async_info async_info = {0};
    void *recv_buf = NULL;

    mocker_clean();
    mocker(dl_drv_hdc_alloc_msg, 1, 0);
    mocker(dl_hal_hdc_recv, 1, -1);
    mocker(dl_drv_hdc_free_msg, 1, 0);
    ra_hdc_async_recv_pkt(&async_info, 0, NULL, NULL);
    mocker_clean();

    mocker(dl_drv_hdc_alloc_msg, 1, 0);
    mocker(dl_hal_hdc_recv, 1, 0);
    mocker(dl_drv_hdc_get_msg_buffer, 1, -1);
    mocker(dl_drv_hdc_free_msg, 1, 0);
    ra_hdc_async_recv_pkt(&async_info, 0, NULL, NULL);
    mocker_clean();

    mocker(dl_drv_hdc_alloc_msg, 1, 0);
    mocker(dl_hal_hdc_recv, 1, 0);
    mocker(dl_drv_hdc_get_msg_buffer, 1, 0);
    mocker(dl_drv_hdc_free_msg, 1, 0);
    ra_hdc_async_recv_pkt(&async_info, 0, &recv_buf, NULL);
    free(recv_buf);
    mocker_clean();
}

hdcError_t dl_drv_hdc_get_msg_buffer_stub(struct drvHdcMsg *msg, int index, char **pBuf, int *pLen)
{
    *pLen = 1;
    return 0;
}

void tc_hdc_async_recv_pkt()
{
    struct ra_request_handle stub_req_handle = { 0 };
    struct hdc_async_info async_info = {0};
    HDC_SESSION stub_session = { 0 };
    void *recv_buf = NULL;
    unsigned int recv_len;
    int ret;

    async_info.session = &stub_session;
    RA_INIT_LIST_HEAD(&async_info.req_list);
    ra_list_add_tail(&stub_req_handle.list, &async_info.req_list);

    mocker_clean();
    mocker(pthread_mutex_lock, 10, 0);
    mocker(dl_drv_hdc_alloc_msg, 10, 0);
    mocker(dl_hal_hdc_recv, 10, 25);
    mocker(dl_drv_hdc_get_msg_buffer, 10, 0);
    mocker(pthread_mutex_unlock, 10, 0);
    mocker(memcpy_s, 10, 0);
    mocker(dl_drv_hdc_free_msg, 10, 0);
    mocker(hdc_async_set_req_done, 10, (void*)0);
    ret = hdc_async_recv_pkt(&async_info, 0, recv_buf, &recv_len);
    EXPECT_INT_EQ(ret, 25);
    hdc_async_handle_recv_broken(&async_info);

    mocker_clean();
    mocker(pthread_mutex_lock, 10, 0);
    mocker(dl_drv_hdc_alloc_msg, 10, 0);
    mocker(dl_hal_hdc_recv, 10, 0);
    mocker_invoke(dl_drv_hdc_get_msg_buffer, dl_drv_hdc_get_msg_buffer_stub, 10);
    mocker(pthread_mutex_unlock, 10, 0);
    mocker(memcpy_s, 10, 0);
    mocker(dl_drv_hdc_free_msg, 10, 0);
    ret = hdc_async_recv_pkt(&async_info, 0, recv_buf, &recv_len);
    EXPECT_INT_EQ(ret, 25);
    hdc_async_handle_recv_broken(&async_info);

    EXPECT_INT_EQ(async_info.last_recv_status, 25);
    ret = ra_hdc_send_msg_async(4, 0, NULL, 0, NULL);
    EXPECT_INT_EQ(ret, -EINVAL);
    mocker_clean();
}

void tc_ra_hdc_pool_add_task()
{
    struct ra_hdc_thread_pool pool = {0};
    struct ra_hdc_task task_queue[5] = {0};
    int (*ra_hdc_handle_send_pkt)(unsigned int chip_id, void *recv_buf, unsigned int recv_len);

    mocker_clean();
    pool.task_queue = &task_queue;
    pool.queue_pi = 0;
    pool.task_num = 2;
    pool.queue_size = 5;
    mocker(pthread_mutex_lock, 1, 0);
    mocker(pthread_mutex_unlock, 1, 0);
    ra_hdc_pool_add_task(&pool, ra_hdc_handle_send_pkt, 0, NULL, 2);
    mocker_clean();
}

int stub_ra_hdc_pool_create_pthread_create(pthread_t *thread, const pthread_attr_t *attr,
    void *(*start_routine) (void *), void *arg)
{
    return -1;
}

void tc_ra_hdc_pool_create()
{
    mocker_clean();
    mocker_invoke(pthread_create, stub_ra_hdc_pool_create_pthread_create, 1);
    ra_hdc_pool_create(1, 1);
    mocker_clean();
}

void tc_ra_async_handle_pkt()
{
    struct msg_head recv_buf = {0};

    mocker_clean();
    mocker(ra_hdc_handle_send_pkt, 1, 0);
    mocker(ra_hdc_close_session, 1, 0);
    mocker(ra_hdc_pool_add_task, 1, 0);
    mocker(pthread_mutex_lock, 10, 0);
    mocker(pthread_mutex_unlock, 10, 0);
    ra_async_handle_pkt(1, &recv_buf, 0);
    mocker_clean();
}

void tc_ra_hdc_async_handle_socket_listen_start()
{
    struct ra_request_handle req_handle = {0};

    mocker_clean();
    req_handle.priv_data = malloc(sizeof(struct ra_response_socket_listen));
    mocker(ra_get_socket_listen_result, 1, -1);
    ra_hdc_async_handle_socket_listen_start(&req_handle);
    mocker_clean();
}

void tc_ra_hdc_async_handle_qp_import()
{
    struct ra_request_handle req_handle = {0};
    union op_ctx_qp_import_data recv_buf = {0};
    struct qp_import_info priv_data = {0};
    struct ra_ctx_rem_qp_handle priv_handle = {0};

    req_handle.recv_buf = &recv_buf;
    req_handle.priv_data = &priv_data;
    req_handle.priv_handle = &priv_handle;
    priv_handle.protocol = PROTOCOL_UDMA;
    ra_hdc_async_handle_qp_import(&req_handle);
}

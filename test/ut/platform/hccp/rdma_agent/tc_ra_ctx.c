/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <errno.h>
#include <stdlib.h>
#include <pthread.h>
#include "securec.h"
#include "ut_dispatch.h"
#include "hccp_common.h"
#include "ra_rs_err.h"
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
#include "ra_peer_ctx.h"
#include "rs_ctx.h"
#include "tc_ra_ctx.h"

extern void HdcAsyncSetReqDone(struct RaRequestHandle *reqHandle, unsigned int phyId, int ret);
extern int DlDrvDeviceGetIndexByPhyId(uint32_t phyId, uint32_t *devIndex);
extern int DlHalGetChipInfo(unsigned int devId, halChipInfo *chipInfo);
extern hdcError_t DlDrvHdcAllocMsg(HDC_SESSION session, struct drvHdcMsg **ppMsg, int count);
extern hdcError_t DlHalHdcRecv(HDC_SESSION session, struct drvHdcMsg *pMsg, int bufLen, UINT64 flag,
    int *recvBufCount, UINT32 timeout);
extern hdcError_t DlDrvHdcGetMsgBuffer(struct drvHdcMsg *msg, int index, char **pBuf, int *pLen);
extern hdcError_t DlDrvHdcFreeMsg(struct drvHdcMsg *msg);
extern int RaHdcHandleSendPkt(unsigned int chipId, void *recvBuf, unsigned int recvLen);
extern int DlDrvGetLocalDevIdByHostDevId(uint32_t phyId, uint32_t *devIndex);
extern int DlHalGetChipInfo(unsigned int devId, halChipInfo *chipInfo);
extern int ra_ctx_prepare_qp_create(struct qp_create_attr *qpAttr, struct ctx_qp_attr *ctx_qp_attr);
extern int qp_query_batch_param_check(void *qp_handle[], unsigned int *num, unsigned int phyId, unsigned int ids[]);
extern int qp_destroy_batch_param_check(struct ra_ctx_handle *ctx_handle, void *qp_handle[],
    unsigned int ids[], unsigned int *num);

extern struct ra_ctx_ops g_ra_hdc_ctx_ops;

int ra_hdc_process_msg_stub(unsigned int opcode, unsigned int phyId, char *data, unsigned int data_size)
{
    union op_ctx_qp_query_batch_data *op_data = (union op_ctx_qp_query_batch_data *)data;
    op_data->rx_data.num = 10;
    return 0;
}

void tc_ra_get_dev_eid_info_num()
{
    struct RaInfo info = {0};
    unsigned int num = 0;
    int ret = 0;

    mocker_clean();
    info.mode = NETWORK_OFFLINE;
    mocker(RaHdcProcessMsg, 1, 0);
    ret = ra_get_dev_eid_info_num(info, &num);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();

    info.mode = NETWORK_PEER_ONLINE;
    mocker(ra_peer_get_dev_eid_info_num, 1, 0);
    ret = ra_get_dev_eid_info_num(info, &num);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();

    mocker(ra_peer_get_dev_eid_info_num, 1, -1);
    ret = ra_get_dev_eid_info_num(info, &num);
    EXPECT_INT_EQ(128100, ret);
    mocker_clean();
}

void tc_ra_get_dev_eid_info_list()
{
    struct dev_eid_info info_list[35] = {0};
    struct RaInfo info = {0};
    unsigned int num = 35;
    int ret = 0;

    mocker_clean();
    info.mode = NETWORK_OFFLINE;
    mocker(RaHdcProcessMsg, 2, 0);
    ret = ra_get_dev_eid_info_list(info, info_list, &num);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();

    info.mode = NETWORK_PEER_ONLINE;
    mocker(ra_peer_get_dev_eid_info_list, 1, 0);
    ret = ra_get_dev_eid_info_list(info, info_list, &num);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();

    mocker(ra_peer_get_dev_eid_info_list, 1, -1);
    ret = ra_get_dev_eid_info_list(info, info_list, &num);
    EXPECT_INT_EQ(128100, ret);
    mocker_clean();
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
    mocker(DlDrvGetLocalDevIdByHostDevId, 1, 0);
    mocker_invoke(DlHalGetChipInfo, stub_dl_hal_get_chip_info, 1);
    mocker(RaHdcProcessMsg, 1, 0);
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
    mocker(RaHdcProcessMsg, 1, 0);
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
    mocker(RaHdcProcessMsg, 3, 0);
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
    mocker(RaHdcProcessMsg, 2, 0);
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
    mocker(RaHdcProcessMsg, 2, 0);
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
    mocker(RaHdcProcessMsg, 5, 0);

    ctx_handle.protocol = PROTOCOL_UDMA;
    ret = ra_ctx_cq_create(&ctx_handle, &cq_info_t, &cq_handle);
    EXPECT_INT_EQ(0, ret);
    ret = ra_ctx_cq_destroy(&ctx_handle, cq_handle);
    EXPECT_INT_EQ(0, ret);

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
    EXPECT_INT_EQ(ConverReturnCode(RDMA_OP, -EINVAL), ret);

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
    mocker(RaHdcProcessMsg, 3, 0);
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
    mocker(RaHdcProcessMsg, 3, 0);
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
    mocker(RaHdcProcessMsg, 3, 0);
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
    mocker(RaHdcProcessMsg, 5, 0);

    ctx_handle.protocol = PROTOCOL_UDMA;
    ret = ra_ctx_qp_create(&ctx_handle, &attr, &info, &qp_handle);
    EXPECT_INT_EQ(0, ret);
    ret = ra_ctx_qp_destroy(qp_handle);
    EXPECT_INT_EQ(0, ret);

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
    EXPECT_INT_EQ(ConverReturnCode(RDMA_OP, -EINVAL), ret);

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
    mocker(RaHdcProcessMsg, 2, 0);
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
    mocker(RaHdcProcessMsg, 2, 0);
    ret = ra_ctx_qp_bind(&qp_handle, &rem_qp_handle);
    EXPECT_INT_EQ(0, ret);
    ret = ra_ctx_qp_unbind(&qp_handle);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();
    mocker(RaHdcProcessMsg, 2, -ENODEV);
    ret = ra_ctx_qp_unbind(&qp_handle);
    EXPECT_INT_EQ(ConverReturnCode(RDMA_OP, -ENODEV), ret);
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
    mocker(RaHdcProcessMsg, 2, 0);
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
    mocker(RaHdcProcessMsg, 1, 0);
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
    struct RaInfo info = {0};
    int ret = 0;

    mocker_clean();
    mocker(RaHdcProcessMsg, 1, 0);
    info.mode = NETWORK_OFFLINE;
    ret = ra_custom_channel(info, &in, &out);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();
}

void tc_ra_get_tp_info_list_async()
{
    struct tp_info info_list[HCCP_MAX_TPID_INFO_NUM] = {0};
    struct RaRequestHandle *req_handle = NULL;
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
    struct RaRequestHandle *req_handle = NULL;
    struct ra_ctx_handle ctx_handle = {0};
    struct get_tp_cfg cfg = {0};
    unsigned int num = 1;
    int ret = 0;

    mocker(RaHdcSendMsgAsync, 1, -1);
    ret =  ra_hdc_get_tp_info_list_async(&ctx_handle, &cfg, &info_list, &num, &req_handle);
    EXPECT_INT_NE(0, ret);
    mocker_clean();

    mocker(RaHdcSendMsgAsync, 1, 0);
    ret =  ra_hdc_get_tp_info_list_async(&ctx_handle, &cfg, &info_list, &num, &req_handle);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();

    req_handle->opRet = -1;
    ra_hdc_async_handle_tp_info_list(req_handle);

    req_handle->opRet = 0;
    req_handle->recvBuf = &recv_buf;
    async_rsp = (struct ra_response_tp_info_list *)calloc(1, sizeof(struct ra_response_tp_info_list));
    async_rsp->num = &num;
    async_rsp->info_list = info_list;
    req_handle->privData = (void *)async_rsp;
    ra_hdc_async_handle_tp_info_list(req_handle);

    req_handle->opRet = 0;
    recv_buf.rx_data.num = 1;
    req_handle->recvBuf = &recv_buf;
    async_rsp = (struct ra_response_tp_info_list *)calloc(1, sizeof(struct ra_response_tp_info_list));
    async_rsp->num = &num;
    async_rsp->info_list = info_list;
    req_handle->privData = (void *)async_rsp;
    mocker(memcpy_s, 1, -1);
    ra_hdc_async_handle_tp_info_list(req_handle);
    mocker_clean();

    req_handle->opRet = 0;
    recv_buf.rx_data.num = 1;
    req_handle->recvBuf = &recv_buf;
    async_rsp = (struct ra_response_tp_info_list *)calloc(1, sizeof(struct ra_response_tp_info_list));
    async_rsp->num = &num;
    async_rsp->info_list = info_list;
    req_handle->privData = (void *)async_rsp;
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

    char* in_buf = calloc(1, sizeof(struct MsgHead) + sizeof(union op_get_tp_info_list_data));
    char* out_buf = calloc(1, sizeof(struct MsgHead) + sizeof(union op_get_tp_info_list_data));

    data_in.tx_data.phy_id = 0;
    data_in.tx_data.dev_index = 0;
    memcpy_s(in_buf + sizeof(struct MsgHead), sizeof(union op_get_tp_info_list_data),
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
    RaHwAsyncInit(0, 0);
    union OpAsyncHdcConnectData connect_data = {0};
    connect_data.txData.phyId = 0;
    connect_data.txData.queueSize = MAX_POOL_QUEUE_SIZE;
    connect_data.txData.threadNum = MAX_POOL_THREAD_NUM;
    unsigned int connect_data_size = sizeof(union OpAsyncHdcConnectData);

    void *send_rcv_buf = NULL;
    unsigned int send_rcv_len;
    int ret;
    pid_t host_tgid = 0;
    unsigned int opcode = RA_RS_ASYNC_HDC_SESSION_CONNECT;
    send_rcv_len = sizeof(struct MsgHead) + connect_data_size;
    send_rcv_buf = (void *)calloc(send_rcv_len, sizeof(char));
    MsgHeadBuildUp(send_rcv_buf, opcode, 0, connect_data_size, host_tgid);

    ret = memcpy_s(send_rcv_buf + sizeof(struct MsgHead), send_rcv_len - sizeof(struct MsgHead), &connect_data, connect_data_size);
    if (ret) {
        hccp_err("[process][ra_hdc_msg]memcpy_s failed, ret(%d) phyId(%u)", ret, connect_data.txData.phyId);
        return;
    }
    int op_ret = 0;
    void *send_buf = NULL;
    int snd_buf_len = 0;

    struct MsgHead *recv_msg_head = (struct MsgHead *)send_rcv_buf;
    send_buf = (char *)calloc(sizeof(char), recv_msg_head->msgDataLen + sizeof(struct MsgHead));
    CHK_PRT_RETURN(send_buf == NULL, hccp_err("calloc fail."), -ENOMEM);

    mocker(RaHdcAsyncRecvPkt, 1, -1);
    RaRsAsyncHdcSessionConnect(send_rcv_buf, send_buf, snd_buf_len, &op_ret, send_rcv_len);

    union OpAsyncHdcCloseData close_data = {0};
    unsigned int close_data_size = sizeof(union OpAsyncHdcCloseData);
    void *send_rcv_buf2 = NULL;
    unsigned int send_rcv_len2;
    unsigned int opcode2 = RA_RS_ASYNC_HDC_SESSION_CLOSE;
    send_rcv_len2 = sizeof(struct MsgHead) + close_data_size;
    send_rcv_buf2 = (void *)calloc(send_rcv_len, sizeof(char));
    MsgHeadBuildUp(send_rcv_buf2, opcode2, 0, close_data_size, host_tgid);
    ret = memcpy_s(send_rcv_buf2 + sizeof(struct MsgHead), send_rcv_len2 - sizeof(struct MsgHead), &close_data, close_data_size);

    int op_ret2 = 0;
    void *send_buf2 = NULL;
    int snd_buf_len2 = 0;

    RaRsAsyncHdcSessionClose(send_rcv_buf2, send_buf2, snd_buf_len2, &op_ret2, send_rcv_len2);
    RaHwAsyncDeinit();

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
    union OpSocketSendData *async_data = NULL;
    async_data = (union OpSocketSendData *)calloc(sizeof(union OpSocketSendData), sizeof(char));
    async_data->txData.fd = 0;
    async_data->txData.sendSize = size;
    memcpy_s(async_data->txData.dataSend, SOCKET_SEND_MAXLEN, data, size);

    void *send_buf = NULL;
    unsigned int send_len;
    int ret;
    pid_t host_tgid = 0;
    unsigned int opcode = RA_RS_SOCKET_SEND;
    send_len = sizeof(struct MsgHead) + sizeof(union OpSocketSendData);
    send_buf = (void *)calloc(send_len, sizeof(char));
    MsgHeadBuildUp(send_buf, opcode, 0, sizeof(union OpSocketSendData), host_tgid);

    memcpy_s(send_buf + sizeof(struct MsgHead), send_len - sizeof(struct MsgHead), async_data, sizeof(union OpSocketSendData));
    MsgHeadBuildUp(send_buf, opcode, 0, sizeof(union OpSocketSendData), host_tgid);

    RaHdcHandleSendPkt(0, send_buf, send_len);

    free(data);
    free(async_data);
    free(send_buf);
}

void tc_ra_hdc_async_recv_pkt()
{
    struct RaHdcAsyncInfo async_info = {0};
    void *recv_buf = NULL;

    mocker_clean();
    mocker(DlDrvHdcAllocMsg, 1, 0);
    mocker(DlHalHdcRecv, 1, -1);
    mocker(DlDrvHdcFreeMsg, 1, 0);
    RaHdcAsyncRecvPkt(&async_info, 0, NULL, NULL);
    mocker_clean();

    mocker(DlDrvHdcAllocMsg, 1, 0);
    mocker(DlHalHdcRecv, 1, 0);
    mocker(DlDrvHdcGetMsgBuffer, 1, -1);
    mocker(DlDrvHdcFreeMsg, 1, 0);
    RaHdcAsyncRecvPkt(&async_info, 0, NULL, NULL);
    mocker_clean();

    mocker(DlDrvHdcAllocMsg, 1, 0);
    mocker(DlHalHdcRecv, 1, 0);
    mocker(DlDrvHdcGetMsgBuffer, 1, 0);
    mocker(DlDrvHdcFreeMsg, 1, 0);
    RaHdcAsyncRecvPkt(&async_info, 0, &recv_buf, NULL);
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
    struct RaRequestHandle stub_req_handle = { 0 };
    struct HdcAsyncInfo async_info = {0};
    HDC_SESSION stub_session = { 0 };
    void *recv_buf = NULL;
    unsigned int recv_len;
    int ret;

    async_info.session = &stub_session;
    RA_INIT_LIST_HEAD(&async_info.reqList);
    RaListAddTail(&stub_req_handle.list, &async_info.reqList);

    mocker_clean();
    mocker(pthread_mutex_lock, 10, 0);
    mocker(DlDrvHdcAllocMsg, 10, 0);
    mocker(DlHalHdcRecv, 10, 25);
    mocker(DlDrvHdcGetMsgBuffer, 10, 0);
    mocker(pthread_mutex_unlock, 10, 0);
    mocker(memcpy_s, 10, 0);
    mocker(DlDrvHdcFreeMsg, 10, 0);
    mocker(HdcAsyncSetReqDone, 10, (void*)0);
    ret = HdcAsyncRecvPkt(&async_info, 0, recv_buf, &recv_len);
    EXPECT_INT_EQ(ret, 25);
    HdcAsyncHandleRecvBroken(&async_info);

    mocker_clean();
    mocker(pthread_mutex_lock, 10, 0);
    mocker(DlDrvHdcAllocMsg, 10, 0);
    mocker(DlHalHdcRecv, 10, 0);
    mocker_invoke(DlDrvHdcGetMsgBuffer, dl_drv_hdc_get_msg_buffer_stub, 10);
    mocker(pthread_mutex_unlock, 10, 0);
    mocker(memcpy_s, 10, 0);
    mocker(DlDrvHdcFreeMsg, 10, 0);
    ret = HdcAsyncRecvPkt(&async_info, 0, recv_buf, &recv_len);
    EXPECT_INT_EQ(ret, 25);
    HdcAsyncHandleRecvBroken(&async_info);

    EXPECT_INT_EQ(async_info.lastRecvStatus, 25);
    ret = RaHdcSendMsgAsync(4, 0, NULL, 0, NULL);
    EXPECT_INT_EQ(ret, -EINVAL);
    mocker_clean();
}

void tc_ra_hdc_pool_add_task()
{
    struct RaHdcThreadPool pool = {0};
    struct RaHdcTask task_queue[5] = {0};
    int (*RaHdcHandleSendPkt)(unsigned int chipId, void *recvBuf, unsigned int recvLen);

    mocker_clean();
    pool.taskQueue = &task_queue;
    pool.queuePi = 0;
    pool.taskNum = 2;
    pool.queueSize = 5;
    mocker(pthread_mutex_lock, 1, 0);
    mocker(pthread_mutex_unlock, 1, 0);
    RaHdcPoolAddTask(&pool, RaHdcHandleSendPkt, 0, NULL, 2);
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
    RaHdcPoolCreate(1, 1);
    mocker_clean();
}

void tc_ra_async_handle_pkt()
{
    struct MsgHead recv_buf = {0};

    mocker_clean();
    mocker(RaHdcHandleSendPkt, 1, 0);
    mocker(RaHdcCloseSession, 1, 0);
    mocker(RaHdcPoolAddTask, 1, 0);
    mocker(pthread_mutex_lock, 10, 0);
    mocker(pthread_mutex_unlock, 10, 0);
    RaAsyncHandlePkt(1, &recv_buf, 0);
    mocker_clean();
}

void tc_ra_hdc_async_handle_socket_listen_start()
{
    struct RaRequestHandle req_handle = {0};

    mocker_clean();
    req_handle.privData = malloc(sizeof(struct RaResponseSocketListen));
    mocker(RaGetSocketListenResult, 1, -1);
    RaHdcAsyncHandleSocketListenStart(&req_handle);
    mocker_clean();
}

void tc_ra_hdc_async_handle_qp_import()
{
    struct RaRequestHandle req_handle = {0};
    union op_ctx_qp_import_data recv_buf = {0};
    struct qp_import_info priv_data = {0};
    struct ra_ctx_rem_qp_handle privHandle = {0};

    req_handle.recvBuf = &recv_buf;
    req_handle.privData = &priv_data;
    req_handle.privHandle = &privHandle;
    privHandle.protocol = PROTOCOL_UDMA;
    ra_hdc_async_handle_qp_import(&req_handle);
}

void tc_ra_peer_ctx_init()
{
    struct dev_base_attr dev_base_attr = {0};
    struct ra_ctx_handle ctx_handle = {0};
    struct ctx_init_attr info = {0};
    unsigned int dev_index = 0;
    int ret = 0;

    ret = ra_peer_ctx_init(&ctx_handle, &info, &dev_index, &dev_base_attr);
    EXPECT_INT_EQ(ret, 0);

    mocker(rs_ctx_init, 1, -1);
    ret = ra_peer_ctx_init(&ctx_handle, &info, &dev_index, &dev_base_attr);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();
}

void tc_ra_peer_ctx_deinit()
{
    struct ra_ctx_handle ctx_handle = {0};
    int ret = 0;

    ret = ra_peer_ctx_deinit(&ctx_handle);
    EXPECT_INT_EQ(ret, 0);

    mocker(rs_ctx_deinit, 1, -1);
    ret = ra_peer_ctx_deinit(&ctx_handle);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();
}

void tc_ra_peer_get_dev_eid_info_num()
{
    struct RaInfo info = {0};
    unsigned int num = 0;
    int ret = 0;

    ret = ra_peer_get_dev_eid_info_num(info, &num);
    EXPECT_INT_EQ(ret, 0);

    mocker(rs_get_dev_eid_info_num, 1, -1);
    ret = ra_peer_get_dev_eid_info_num(info, &num);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();
}

void tc_ra_peer_get_dev_eid_info_list()
{
    struct dev_eid_info info_list[35] = {0};
    unsigned int phyId = 0;
    unsigned int num = 0;
    int ret = 0;

    ret = ra_peer_get_dev_eid_info_list(phyId, info_list, &num);
    EXPECT_INT_EQ(ret, 0);

    mocker(rs_get_dev_eid_info_list, 1, -1);
    ret = ra_peer_get_dev_eid_info_list(phyId, info_list, &num);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();
}

void tc_ra_peer_ctx_token_id_alloc()
{
    struct ra_token_id_handle token_id_handle = {0};
    struct ra_ctx_handle ctx_handle = {0};
    struct hccp_token_id info = {0};
    int ret = 0;

    ret = ra_peer_ctx_token_id_alloc(&ctx_handle, &info, &token_id_handle);
    EXPECT_INT_EQ(ret, 0);

    mocker(rs_ctx_token_id_alloc, 1, -1);
    ret = ra_peer_ctx_token_id_alloc(&ctx_handle, &info, &token_id_handle);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();
}

void tc_ra_peer_ctx_token_id_free()
{
    struct ra_token_id_handle token_id_handle = {0};
    struct ra_ctx_handle ctx_handle = {0};
    int ret = 0;

    ret = ra_peer_ctx_token_id_free(&ctx_handle, &token_id_handle);
    EXPECT_INT_EQ(ret, 0);

    mocker(rs_ctx_token_id_free, 1, -1);
    ret = ra_peer_ctx_token_id_free(&ctx_handle, &token_id_handle);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();
}

void tc_ra_peer_ctx_lmem_register()
{
    struct ra_lmem_handle lmem_handle = {0};
    struct ra_ctx_handle ctx_handle = {0};
    struct mr_reg_info_t lmem_info = {0};
    int ret = 0;

    lmem_info.in.ub.flags.bs.token_id_valid = 1;
    ret = ra_peer_ctx_lmem_register(&ctx_handle, &lmem_info, &lmem_handle);
    EXPECT_INT_EQ(ret, -22);

    lmem_info.in.ub.flags.bs.token_id_valid = 0;
    ret = ra_peer_ctx_lmem_register(&ctx_handle, &lmem_info, &lmem_handle);
    EXPECT_INT_EQ(ret, 0);

    mocker(rs_ctx_lmem_reg, 1, -1);
    ret = ra_peer_ctx_lmem_register(&ctx_handle, &lmem_info, &lmem_handle);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();
}

void tc_ra_peer_ctx_lmem_unregister()
{
    struct ra_lmem_handle lmem_handle = {0};
    struct ra_ctx_handle ctx_handle = {0};
    int ret = 0;

    ret = ra_peer_ctx_lmem_unregister(&ctx_handle, &lmem_handle);
    EXPECT_INT_EQ(ret, 0);

    mocker(rs_ctx_lmem_unreg, 1, -1);
    ret = ra_peer_ctx_lmem_unregister(&ctx_handle, &lmem_handle);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();
}

void tc_ra_peer_ctx_rmem_import()
{
    struct mr_import_info_t rmem_info = {0};
    struct ra_ctx_handle ctx_handle = {0};
    int ret = 0;

    ret = ra_peer_ctx_rmem_import(&ctx_handle, &rmem_info);
    EXPECT_INT_EQ(ret, 0);

    mocker(rs_ctx_rmem_import, 1, -1);
    ret = ra_peer_ctx_rmem_import(&ctx_handle, &rmem_info);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();
}

void tc_ra_peer_ctx_rmem_unimport()
{
    struct ra_rmem_handle rmem_handle = {0};
    struct ra_ctx_handle ctx_handle = {0};
    int ret = 0;

    ret = ra_peer_ctx_rmem_unimport(&ctx_handle, &rmem_handle);
    EXPECT_INT_EQ(ret, 0);

    mocker(rs_ctx_rmem_unimport, 1, -1);
    ret = ra_peer_ctx_rmem_unimport(&ctx_handle, &rmem_handle);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();
}

void tc_ra_peer_ctx_chan_create()
{
    struct ra_chan_handle chan_handle = {0};
    struct ra_ctx_handle ctx_handle = {0};
    struct chan_info_t chan_info = {0};
    int ret = 0;

    ret = ra_peer_ctx_chan_create(&ctx_handle, &chan_info, &chan_handle);
    EXPECT_INT_EQ(ret, 0);

    mocker(rs_ctx_chan_create, 1, -1);
    ret = ra_peer_ctx_chan_create(&ctx_handle, &chan_info, &chan_handle);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();
}

void tc_ra_peer_ctx_chan_destroy()
{
    struct ra_chan_handle chan_handle = {0};
    struct ra_ctx_handle ctx_handle = {0};
    int ret = 0;

    ret = ra_peer_ctx_chan_destroy(&ctx_handle, &chan_handle);
    EXPECT_INT_EQ(ret, 0);

    mocker(rs_ctx_chan_destroy, 1, -1);
    ret = ra_peer_ctx_chan_destroy(&ctx_handle, &chan_handle);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();
}

void tc_ra_peer_ctx_cq_create()
{
    struct ra_chan_handle chan_handle = {0};
    struct ra_ctx_handle ctx_handle = {0};
    struct ra_cq_handle cq_handle = {0};
    struct cq_info_t info = {0};
    int ret = 0;

    info.in.ub.mode = JFC_MODE_CCU_POLL;
    info.in.ub.ccu_ex_cfg.valid = true;
    ret = ra_peer_ctx_cq_create(&ctx_handle, &info, &cq_handle);
    EXPECT_INT_EQ(ret, -22);

    mocker(rs_ctx_cq_create, 1, -1);
    info.in.ub.mode = JFC_MODE_NORMAL;
    info.in.chan_handle = (void *)&chan_handle;
    ret = ra_peer_ctx_cq_create(&ctx_handle, &info, &cq_handle);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();
}

void tc_ra_peer_ctx_cq_destroy()
{
    struct ra_ctx_handle ctx_handle = {0};
    struct ra_cq_handle cq_handle = {0};
    int ret = 0;

    ret = ra_peer_ctx_cq_destroy(&ctx_handle, &cq_handle);
    EXPECT_INT_EQ(ret, 0);

    mocker(rs_ctx_cq_destroy, 1, -1);
    ret = ra_peer_ctx_cq_destroy(&ctx_handle, &cq_handle);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();
}

void tc_ra_peer_ctx_qp_create()
{
    struct ra_ctx_qp_handle qp_handle = {0};
    struct ra_ctx_handle ctx_handle = {0};
    struct qp_create_attr qpAttr = {0};
    struct qp_create_info qp_info = {0};
    int ret = 0;

    qpAttr.ub.mode = JETTY_MODE_CCU;
    ret = ra_peer_ctx_qp_create(&ctx_handle, &qpAttr, &qp_info, &qp_handle);
    EXPECT_INT_EQ(ret, -22);

    qpAttr.ub.mode = JETTY_MODE_URMA_NORMAL;
    mocker(ra_ctx_prepare_qp_create, 1, 0);
    ret = ra_peer_ctx_qp_create(&ctx_handle, &qpAttr, &qp_info, &qp_handle);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    mocker(ra_ctx_prepare_qp_create, 1, 0);
    mocker(rs_ctx_qp_create, 1, -1);
    ret = ra_peer_ctx_qp_create(&ctx_handle, &qpAttr, &qp_info, &qp_handle);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();
}

void tc_ra_ctx_prepare_qp_create()
{
    struct ra_token_id_handle token_id_handle = {0};
    struct ctx_qp_attr ctx_qp_attr = {0};
    struct qp_create_attr qpAttr = {0};
    struct ra_cq_handle cq_handle = {0};
    int ret = 0;

    ret = ra_ctx_prepare_qp_create(&qpAttr, &ctx_qp_attr);
    EXPECT_INT_EQ(ret, -22);

    qpAttr.scq_handle = (void *)&cq_handle;
    ret = ra_ctx_prepare_qp_create(&qpAttr, &ctx_qp_attr);
    EXPECT_INT_EQ(ret, -22);

    qpAttr.rcq_handle = (void *)&cq_handle;

    qpAttr.ub.mode = JETTY_MODE_URMA_NORMAL;
    qpAttr.ub.token_id_handle = (void *)&token_id_handle;
    ret = ra_ctx_prepare_qp_create(&qpAttr, &ctx_qp_attr);
    EXPECT_INT_EQ(ret, 0);
}

void tc_ra_peer_ctx_qp_destroy()
{
    struct ra_ctx_qp_handle qp_handle = {0};
    int ret = 0;

    ret = ra_peer_ctx_qp_destroy(&qp_handle);
    EXPECT_INT_EQ(ret, 0);

    mocker(rs_ctx_qp_destroy, 1, -1);
    ret = ra_peer_ctx_qp_destroy(&qp_handle);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();
}

void tc_ra_peer_ctx_qp_import()
{
    struct ra_ctx_rem_qp_handle rem_qp_handle = {0};
    struct ra_ctx_handle ctx_handle = {0};
    struct qp_import_info_t qp_info = {0};
    int ret = 0;

    ret = ra_peer_ctx_qp_import(&ctx_handle, &qp_info, &rem_qp_handle);
    EXPECT_INT_EQ(ret, 0);

    mocker(rs_ctx_qp_import, 1, -1);
    ret = ra_peer_ctx_qp_import(&ctx_handle, &qp_info, &rem_qp_handle);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();
}

void tc_ra_peer_ctx_qp_unimport()
{
    struct ra_ctx_rem_qp_handle rem_qp_handle = {0};
    int ret = 0;

    ret = ra_peer_ctx_qp_unimport(&rem_qp_handle);
    EXPECT_INT_EQ(ret, 0);

    mocker(rs_ctx_qp_unimport, 1, -1);
    ret = ra_peer_ctx_qp_unimport(&rem_qp_handle);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();
}

void tc_ra_peer_ctx_qp_bind()
{
    struct ra_ctx_rem_qp_handle rem_qp_handle = {0};
    struct ra_ctx_qp_handle qp_handle = {0};
    int ret = 0;

    ret = ra_peer_ctx_qp_bind(&qp_handle, &rem_qp_handle);
    EXPECT_INT_EQ(ret, 0);

    mocker(rs_ctx_qp_bind, 1, -1);
    ret = ra_peer_ctx_qp_bind(&qp_handle, &rem_qp_handle);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();
}

void tc_ra_peer_ctx_qp_unbind()
{
    struct ra_ctx_qp_handle qp_handle = {0};
    int ret = 0;

    ret = ra_peer_ctx_qp_unbind(&qp_handle);
    EXPECT_INT_EQ(ret, 0);

    mocker(rs_ctx_qp_unbind, 1, -1);
    ret = ra_peer_ctx_qp_unbind(&qp_handle);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();
}

void tc_ra_ctx_qp_destroy_batch_async()
{
    struct ra_ctx_qp_handle *qp_handle[1] = {0};
    struct RaRequestHandle *req_handle = NULL;
    struct ra_ctx_handle ctx_handle = {0};
    unsigned int num = 0;
    int ret;

    mocker_clean();
    ret = ra_ctx_qp_destroy_batch_async(NULL, qp_handle, &num, &req_handle);
    EXPECT_INT_NE(0, ret);

    ret = ra_ctx_qp_destroy_batch_async(&ctx_handle, NULL, &num, &req_handle);
    EXPECT_INT_NE(0, ret);

    ret = ra_ctx_qp_destroy_batch_async(&ctx_handle, qp_handle, NULL, &req_handle);
    EXPECT_INT_NE(0, ret);

    ret = ra_ctx_qp_destroy_batch_async(&ctx_handle, qp_handle, &num, NULL);
    EXPECT_INT_NE(0, ret);

    ret = ra_ctx_qp_destroy_batch_async(&ctx_handle, qp_handle, &num, &req_handle);
    EXPECT_INT_NE(0, ret);

    num = HCCP_MAX_QP_DESTROY_BATCH_NUM + 1;
    ret = ra_ctx_qp_destroy_batch_async(&ctx_handle, qp_handle, &num, &req_handle);
    EXPECT_INT_NE(0, ret);

    num = 1;
    qp_handle[0] = (struct ra_ctx_qp_handle *)calloc(1, sizeof(struct ra_ctx_qp_handle));
    mocker(ra_hdc_ctx_qp_destroy_batch_async, 1, -1);
    ret = ra_ctx_qp_destroy_batch_async(&ctx_handle, qp_handle, &num, &req_handle);
    EXPECT_INT_NE(0, ret);
    mocker_clean();

    qp_handle[0] = (struct ra_ctx_qp_handle *)calloc(1, sizeof(struct ra_ctx_qp_handle));
    mocker(ra_hdc_ctx_qp_destroy_batch_async, 1, 0);
    ret = ra_ctx_qp_destroy_batch_async(&ctx_handle, qp_handle, &num, &req_handle);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();
}

void tc_qp_destroy_batch_param_check()
{
    struct ra_ctx_qp_handle qp_handle_tmp;
    struct ra_ctx_handle ctx_handle = {0};
    unsigned int num = 1;
    unsigned int ids[1];
    void *qp_handle[1];
    int ret;

    mocker_clean();
    qp_handle_tmp.ctx_handle = &ctx_handle;
    qp_handle_tmp.id = 123;
    qp_handle[0] = &qp_handle_tmp;
    ret = qp_destroy_batch_param_check(&ctx_handle, qp_handle, ids, &num);
    EXPECT_INT_EQ(0, ret);

    qp_handle[0] = NULL;
    ret = qp_destroy_batch_param_check(&ctx_handle, qp_handle, ids, &num);
    EXPECT_INT_EQ(-EINVAL, ret);

    qp_handle_tmp.ctx_handle = NULL;
    qp_handle[0] = &qp_handle_tmp;
    ret = qp_destroy_batch_param_check(&ctx_handle, qp_handle, ids, &num);
    EXPECT_INT_EQ(-EINVAL, ret);

    qp_handle_tmp.ctx_handle = (struct ra_ctx_handle *)0x1234;
    ret = qp_destroy_batch_param_check(&ctx_handle, qp_handle, ids, &num);
    EXPECT_INT_EQ(-EINVAL, ret);
}

void tc_ra_hdc_ctx_qp_destroy_batch_async()
{
    union op_ctx_qp_destroy_batch_data recv_buf = {0};
    struct RaRequestHandle *req_handle = NULL;
    struct ra_ctx_qp_handle qp_handle_tmp;
    struct ra_ctx_handle ctx_handle = {0};
    unsigned int num = 1;
    void *qp_handle[1];
    int ret;

    mocker_clean();
    qp_handle_tmp.ctx_handle = &ctx_handle;
    qp_handle[0] = &qp_handle_tmp;

    mocker(qp_destroy_batch_param_check, 1, -EINVAL);
    ret = ra_hdc_ctx_qp_destroy_batch_async(&ctx_handle, qp_handle, &num, &req_handle);
    EXPECT_INT_EQ(-EINVAL, ret);
    mocker_clean();

    mocker(calloc, 1, NULL);
    ret = ra_hdc_ctx_qp_destroy_batch_async(&ctx_handle, qp_handle, &num, &req_handle);
    EXPECT_INT_EQ(-ENOMEM, ret);
    mocker_clean();

    mocker(qp_destroy_batch_param_check, 1, 0);
    mocker(RaHdcSendMsgAsync, 1, -EINVAL);
    ret = ra_hdc_ctx_qp_destroy_batch_async(&ctx_handle, qp_handle, &num, &req_handle);
    EXPECT_INT_EQ(-EINVAL, ret);
    mocker_clean();

    mocker(RaHdcSendMsgAsync, 1, 0);
    ret = ra_hdc_ctx_qp_destroy_batch_async(&ctx_handle, qp_handle, &num, &req_handle);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();

    req_handle->recvBuf = &recv_buf;
    ra_hdc_async_handle_qp_destroy_batch(req_handle);
    free(req_handle);
    req_handle = NULL;
}

void tc_ra_rs_ctx_qp_destroy_batch()
{
    union op_ctx_qp_destroy_batch_data data_in;
    union op_ctx_qp_destroy_batch_data data_out;
    int rcv_buf_len = 0;
    int op_result;
    int out_len;
    int ret;

    char* in_buf = calloc(1, sizeof(struct MsgHead) + sizeof(union op_ctx_qp_destroy_batch_data));
    char* out_buf = calloc(1, sizeof(struct MsgHead) + sizeof(union op_ctx_qp_destroy_batch_data));

    mocker_clean();
    data_in.tx_data.phy_id = 0;
    data_in.tx_data.dev_index = 0;
    memcpy_s(in_buf + sizeof(struct MsgHead), sizeof(union op_ctx_qp_destroy_batch_data),
        &data_in, sizeof(union op_ctx_qp_destroy_batch_data));
    ret = ra_rs_ctx_qp_destroy_batch(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    mocker(rs_ctx_qp_destroy_batch, 1, -1);
    ret = ra_rs_ctx_qp_destroy_batch(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    free(in_buf);
    in_buf = NULL;
    free(out_buf);
    out_buf = NULL;
}

void tc_ra_ctx_qp_query_batch()
{
    struct ra_ctx_qp_handle qp_handle_tmp = {0};
    struct ra_ctx_qp_handle *qp_handle[2];
    struct ra_ctx_handle ctx_handle = {0};
    struct jetty_attr attr[2];
    unsigned int num;
    int ret;

    mocker_clean();
    ret = ra_ctx_qp_query_batch(NULL, attr, &num);
    EXPECT_INT_NE(0, ret);

    ret = ra_ctx_qp_query_batch(qp_handle, NULL, &num);
    EXPECT_INT_NE(0, ret);

    ret = ra_ctx_qp_query_batch(qp_handle, attr, NULL);
    EXPECT_INT_NE(0, ret);

    mocker(qp_query_batch_param_check, 1, -1);
    ret = ra_ctx_qp_query_batch(qp_handle, attr, &num);
    EXPECT_INT_NE(0, ret);
    mocker_clean();

    ctx_handle.ctx_ops = &g_ra_hdc_ctx_ops;
    qp_handle_tmp.ctx_handle = &ctx_handle;
    qp_handle[0] = &qp_handle_tmp;
    qp_handle[1] = &qp_handle_tmp;
    num = 2;
    mocker(qp_query_batch_param_check, 1, 0);
    mocker(ra_hdc_ctx_qp_query_batch, 1, -1);
    ret = ra_ctx_qp_query_batch(qp_handle, attr, &num);
    EXPECT_INT_NE(0, ret);
    mocker_clean();

    mocker(qp_query_batch_param_check, 1, 0);
    mocker(ra_hdc_ctx_qp_query_batch, 1, 0);
    ret = ra_ctx_qp_query_batch(qp_handle, attr, &num);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();
}

void tc_qp_query_batch_param_check()
{
    struct ra_ctx_qp_handle qp_handle1 = {0};
    struct ra_ctx_qp_handle qp_handle2 = {0};
    struct ra_ctx_handle ctx_handle = {0};
    struct ra_ctx_ops ctx_ops_tmp = {0};
    unsigned int phyId = 1;
    unsigned int num = 2;
    void *qp_handles[2];
    unsigned int ids[2];
    int ret;

    qp_handle1.id = 1;
    qp_handle2.id = 2;
    qp_handle1.phy_id = 1;
    qp_handle2.phy_id = 1;
    qp_handles[0] = &qp_handle1;
    qp_handles[1] = &qp_handle2;
    ctx_handle.ctx_ops = &g_ra_hdc_ctx_ops;
    qp_handle1.ctx_handle = &ctx_handle;
    qp_handle2.ctx_handle = &ctx_handle;

    ret = qp_query_batch_param_check(qp_handles, &num, phyId, ids);
    EXPECT_INT_EQ(0, ret);

    qp_handles[0] = NULL;
    ret = qp_query_batch_param_check(qp_handles, &num, phyId, ids);
    EXPECT_INT_NE(0, ret);

    qp_handles[0] = &qp_handle1;
    qp_handle1.phy_id = 2;
    ret = qp_query_batch_param_check(qp_handles, &num, phyId, ids);
    EXPECT_INT_NE(0, ret);

    qp_handle1.phy_id = 1;
    qp_handle1.ctx_handle = NULL;
    ret = qp_query_batch_param_check(qp_handles, &num, phyId, ids);
    EXPECT_INT_NE(0, ret);

    ctx_handle.ctx_ops = NULL;
    qp_handle1.ctx_handle = &ctx_handle;
    ret = qp_query_batch_param_check(qp_handles, &num, phyId, ids);
    EXPECT_INT_NE(0, ret);

    ctx_handle.ctx_ops = &ctx_ops_tmp;
    ret = qp_query_batch_param_check(qp_handles, &num, phyId, ids);
    EXPECT_INT_NE(0, ret);
}

void tc_ra_hdc_ctx_qp_query_batch()
{
    unsigned int ids[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    struct jetty_attr attr[10] = {0};
    unsigned int dev_index = 2;
    unsigned int phyId = 1;
    unsigned int num = 10;
    int ret;

    mocker_clean();
    mocker(RaHdcProcessMsg, 1, 0);
    mocker(memcpy_s, 2, 0);
    ret = ra_hdc_ctx_qp_query_batch(phyId, dev_index, ids, attr, &num);
    EXPECT_INT_EQ(-EOPENSRC, ret);
    mocker_clean();

    mocker(RaHdcProcessMsg, 1, -1);
    ret = ra_hdc_ctx_qp_query_batch(phyId, dev_index, ids, attr, &num);
    EXPECT_INT_EQ(-EOPENSRC, ret);
    mocker_clean();

    mocker_invoke(RaHdcProcessMsg, ra_hdc_process_msg_stub, 1);
    ret = ra_hdc_ctx_qp_query_batch(phyId, dev_index, ids, attr, &num);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();
}

void tc_ra_rs_ctx_qp_query_batch()
{
    union op_ctx_qp_query_batch_data data_in;
    union op_ctx_qp_query_batch_data data_out;
    int rcv_buf_len = 0;
    int op_result;
    int out_len;
    int ret;

    char* in_buf = calloc(1, sizeof(struct MsgHead) + sizeof(union op_ctx_qp_query_batch_data));
    char* out_buf = calloc(1, sizeof(struct MsgHead) + sizeof(union op_ctx_qp_query_batch_data));

    mocker_clean();
    data_in.tx_data.phy_id = 0;
    data_in.tx_data.dev_index = 0;
    memcpy_s(in_buf + sizeof(struct MsgHead), sizeof(union op_ctx_qp_query_batch_data),
        &data_in, sizeof(union op_ctx_qp_query_batch_data));
    ret = ra_rs_ctx_qp_query_batch(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(ret, 0);

    mocker(rs_ctx_qp_query_batch, 1, -1);
    ret = ra_rs_ctx_qp_query_batch(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    free(in_buf);
    in_buf = NULL;
    free(out_buf);
    out_buf = NULL;
}

void tc_ra_get_eid_by_ip()
{
    struct ra_ctx_handle ctx_handle = {0};
    union hccp_eid eid[32] = {0};
    struct IpInfo ip[32] = {0};
    unsigned int num = 32;
    int ret = 0;

    ret = ra_get_eid_by_ip(NULL, eid, ip, &num);
    EXPECT_INT_EQ(ret, 128103);

    num = 33;
    ret = ra_get_eid_by_ip(&ctx_handle, eid, ip, &num);
    EXPECT_INT_EQ(ret, 128103);

    num = 32;
    ret = ra_get_eid_by_ip(&ctx_handle, eid, ip, &num);
    EXPECT_INT_EQ(ret, 128103);

    ctx_handle.ctx_ops = &g_ra_hdc_ctx_ops;
    mocker(ra_hdc_get_eid_by_ip, 1, 0);
    ret = ra_get_eid_by_ip(&ctx_handle, eid, ip, &num);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    mocker(ra_hdc_get_eid_by_ip, 1, -1);
    ret = ra_get_eid_by_ip(&ctx_handle, eid, ip, &num);
    EXPECT_INT_EQ(ret, 128100);
    mocker_clean();
}

void tc_ra_hdc_get_eid_by_ip()
{
    struct ra_ctx_handle ctx_handle = {0};
    union hccp_eid eid[32] = {0};
    struct IpInfo ip[32] = {0};
    unsigned int num = 32;
    int ret = 0;

    mocker(RaHdcProcessMsg, 1, -1);
    mocker(ra_hdc_get_eid_results, 1, 0);
    ret = ra_hdc_get_eid_by_ip(&ctx_handle, ip, eid, &num);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

    mocker(RaHdcProcessMsg, 1, 0);
    mocker(ra_hdc_get_eid_results, 1, 0);
    ret = ra_hdc_get_eid_by_ip(&ctx_handle, ip, eid, &num);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

void tc_ra_rs_get_eid_by_ip()
{
    union op_get_eid_by_ip_data data_out = {0};
    union op_get_eid_by_ip_data data_in = {0};

    int rcv_buf_len = 0;
    int op_result;
    int out_len;
    int ret;

    char* in_buf = calloc(1, sizeof(struct MsgHead) + sizeof(union op_get_eid_by_ip_data));
    char* out_buf = calloc(1, sizeof(struct MsgHead) + sizeof(union op_get_eid_by_ip_data));

    data_in.tx_data.phy_id = 0;
    data_in.tx_data.dev_index = 0;
    memcpy_s(in_buf + sizeof(struct MsgHead), sizeof(union op_get_eid_by_ip_data),
        &data_in, sizeof(union op_get_eid_by_ip_data));
    data_in.tx_data.num = 32;
    mocker(rs_get_eid_by_ip, 1, 0);
    ret = ra_rs_get_eid_by_ip(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(op_result, 0);
    mocker_clean();

    mocker(rs_get_eid_by_ip, 1, -1);
    ret = ra_rs_get_eid_by_ip(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(op_result, -1);
    mocker_clean();

    free(in_buf);
    in_buf = NULL;
    free(out_buf);
    out_buf = NULL;
}

void tc_ra_peer_get_eid_by_ip()
{
    struct ra_ctx_handle ctx_handle = {0};
    union hccp_eid eid[32] = {0};
    struct IpInfo ip[32] = {0};
    unsigned int num = 32;
    int ret = 0;

    ret = ra_peer_get_eid_by_ip(&ctx_handle, ip, eid, &num);
    EXPECT_INT_EQ(ret, 0);

    mocker(rs_get_eid_by_ip, 1, -1);
    ret = ra_peer_get_eid_by_ip(&ctx_handle, ip, eid, &num);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();
}

void tc_ra_ctx_get_aux_info()
{
    struct ra_ctx_handle ctx_handle = {0};
    struct aux_info_out out;
    struct aux_info_in in;
    int ret = 0;

    ret = ra_ctx_get_aux_info(NULL, &in, &out);
    EXPECT_INT_EQ(ret, 128103);

    in.type = AUX_INFO_IN_TYPE_MAX;
    ret = ra_ctx_get_aux_info(&ctx_handle, &in, &out);
    EXPECT_INT_EQ(ret, 128103);

    in.type = AUX_INFO_IN_TYPE_MAX - 1;
    ret = ra_ctx_get_aux_info(&ctx_handle, &in, &out);
    EXPECT_INT_EQ(ret, 128103);

    mocker_clean();
    mocker(ra_hdc_ctx_get_aux_info, 1, -1);
    ctx_handle.ctx_ops = &g_ra_hdc_ctx_ops;
    ret = ra_ctx_get_aux_info(&ctx_handle, &in, &out);
    EXPECT_INT_EQ(ret, 128100);
    mocker_clean();

    mocker(ra_hdc_ctx_get_aux_info, 1, 0);
    ctx_handle.ctx_ops = &g_ra_hdc_ctx_ops;
    ret = ra_ctx_get_aux_info(&ctx_handle, &in, &out);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

void tc_ra_hdc_ctx_get_aux_info()
{
    struct ra_ctx_handle ctx_handle = {0};
    struct aux_info_out out;
    struct aux_info_in in;
    int ret = 0;

    mocker_clean();
    mocker(RaHdcProcessMsg, 1, -1);
    ret = ra_hdc_ctx_get_aux_info(&ctx_handle, &in, &out);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

    mocker(RaHdcProcessMsg, 1, 0);
    ret = ra_hdc_ctx_get_aux_info(&ctx_handle, &in, &out);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

void tc_ra_rs_ctx_get_aux_info()
{
    union op_ctx_get_aux_info_data data_out = {0};
    union op_ctx_get_aux_info_data data_in = {0};

    int rcv_buf_len = 0;
    int op_result;
    int out_len;
    int ret;

    char* in_buf = calloc(1, sizeof(struct MsgHead) + sizeof(union op_ctx_get_aux_info_data));
    char* out_buf = calloc(1, sizeof(struct MsgHead) + sizeof(union op_ctx_get_aux_info_data));

    data_in.tx_data.phy_id = 0;
    data_in.tx_data.dev_index = 0;
    memcpy_s(in_buf + sizeof(struct MsgHead), sizeof(union op_ctx_get_aux_info_data),
        &data_in, sizeof(union op_ctx_get_aux_info_data));
    mocker(rs_ctx_get_aux_info, 1, 0);
    ret = ra_rs_ctx_get_aux_info(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(op_result, 0);
    mocker_clean();

    mocker(rs_ctx_get_aux_info, 1, -1);
    ret = ra_rs_ctx_get_aux_info(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(op_result, -1);
    mocker_clean();

    free(in_buf);
    in_buf = NULL;
    free(out_buf);
    out_buf = NULL;
}

void tc_ra_get_tp_attr_async()
{
    struct RaRequestHandle *req_handle = NULL;
    struct ra_ctx_handle ctx_handle = {0};
    struct tp_attr attr = {0};
    uint32_t attr_bitmap = 1;
    uint64_t tp_handle = 0;
    int ret;

    mocker_clean();
    ret = ra_get_tp_attr_async(NULL, tp_handle, &attr_bitmap, &attr, &req_handle);
    EXPECT_INT_NE(0, ret);

    ret = ra_get_tp_attr_async(&ctx_handle, tp_handle, NULL, &attr, &req_handle);
    EXPECT_INT_NE(0, ret);

    ret = ra_get_tp_attr_async(&ctx_handle, tp_handle, &attr_bitmap, NULL, &req_handle);
    EXPECT_INT_NE(0, ret);

    ret = ra_get_tp_attr_async(&ctx_handle, tp_handle, &attr_bitmap, &attr, NULL);
    EXPECT_INT_NE(0, ret);

    mocker(ra_hdc_get_tp_attr_async, 1, -1);
    ret = ra_get_tp_attr_async(&ctx_handle, tp_handle, &attr_bitmap, &attr, &req_handle);
    EXPECT_INT_NE(0, ret);
    mocker_clean();

    mocker(ra_hdc_get_tp_attr_async, 1, 0);
    ret = ra_get_tp_attr_async(&ctx_handle, tp_handle, &attr_bitmap, &attr, &req_handle);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();
}

void tc_ra_hdc_get_tp_attr_async()
{
    struct ra_response_get_tp_attr *async_rsp = NULL;
    union op_get_tp_attr_data *async_data = NULL;
    struct RaRequestHandle  *req_handle = NULL;
    union op_get_tp_attr_data recv_buf = {0};
    struct ra_ctx_handle ctx_handle = {0};
    uint64_t tp_handle = 1234;
    uint32_t attr_bitmap = 0;
    struct tp_attr attr = {0};
    int ret;

    mocker_clean();
    mocker(calloc, 2, NULL);
    ret = ra_hdc_get_tp_attr_async(&ctx_handle, tp_handle, &attr_bitmap, &attr, &req_handle);
    EXPECT_INT_EQ(-ENOMEM, ret);
    mocker_clean();

    mocker(RaHdcSendMsgAsync, 1, -1);
    ret = ra_hdc_get_tp_attr_async(&ctx_handle, tp_handle, &attr_bitmap, &attr, &req_handle);
    EXPECT_INT_EQ(-1, ret);
    mocker_clean();

    mocker(RaHdcSendMsgAsync, 1, 0);
    ret = ra_hdc_get_tp_attr_async(&ctx_handle, tp_handle, &attr_bitmap, &attr, &req_handle);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();

    req_handle->opRet = 0;
    req_handle->recvBuf = &recv_buf;
    mocker(memcpy_s, 1, 0);
    ra_hdc_async_handle_get_tp_attr(req_handle);
    mocker_clean();

    req_handle->opRet = -1;
    async_rsp = (struct ra_response_get_tp_attr *)calloc(1, sizeof(struct ra_response_get_tp_attr));
    async_rsp->attr = &attr;
    async_rsp->attr_bitmap = &attr_bitmap;
    req_handle->privData = (void *)async_rsp;
    ra_hdc_async_handle_get_tp_attr((struct RaRequestHandle  *)req_handle);
    free(req_handle);
    req_handle = NULL;
}

void tc_ra_rs_get_tp_attr()
{
    union op_get_tp_attr_data data_in;
    union op_get_tp_attr_data data_out;
    int rcv_buf_len = 0;
    int op_result;
    int out_len;
    int ret;

    char* in_buf = calloc(1, sizeof(struct MsgHead) + sizeof(union op_get_tp_attr_data));
    char* out_buf = calloc(1, sizeof(struct MsgHead) + sizeof(union op_get_tp_attr_data));

    mocker_clean();
    data_in.tx_data.phy_id = 0;
    data_in.tx_data.dev_index = 0;
    memcpy_s(in_buf + sizeof(struct MsgHead), sizeof(union op_get_tp_attr_data),
        &data_in, sizeof(union op_get_tp_attr_data));
    ret = ra_rs_get_tp_attr(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(ret, 0);

    mocker(rs_get_tp_attr, 1, -1);
    ret = ra_rs_get_tp_attr(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    free(in_buf);
    in_buf = NULL;
    free(out_buf);
    out_buf = NULL;
}

void tc_ra_set_tp_attr_async()
{
    struct RaRequestHandle  *req_handle = NULL;
    struct ra_ctx_handle ctx_handle = {0};
    struct tp_attr attr = {0};
    uint32_t attr_bitmap = 1;
    uint64_t tp_handle = 0;
    int ret;

    mocker_clean();
    ret = ra_set_tp_attr_async(NULL, tp_handle, attr_bitmap, &attr, &req_handle);
    EXPECT_INT_NE(0, ret);

    ret = ra_set_tp_attr_async(&ctx_handle, tp_handle, attr_bitmap, NULL, &req_handle);
    EXPECT_INT_NE(0, ret);

    ret = ra_set_tp_attr_async(&ctx_handle, tp_handle, attr_bitmap, &attr, NULL);
    EXPECT_INT_NE(0, ret);

    mocker(ra_hdc_set_tp_attr_async, 1, -1);
    ret = ra_set_tp_attr_async(&ctx_handle, tp_handle, attr_bitmap, &attr, &req_handle);
    EXPECT_INT_NE(0, ret);
    mocker_clean();

    mocker(ra_hdc_set_tp_attr_async, 1, 0);
    ret = ra_set_tp_attr_async(&ctx_handle, tp_handle, attr_bitmap, &attr, &req_handle);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();
}

void tc_ra_hdc_set_tp_attr_async()
{
    struct RaRequestHandle  *req_handle = NULL;
    struct ra_ctx_handle ctx = {0};
    uint64_t tp_handle = 1234;
    uint32_t attr_bitmap = 0;
    struct tp_attr attr = {0};

    mocker_clean();
    mocker(calloc, 2, NULL);
    int ret = ra_hdc_set_tp_attr_async(&ctx, tp_handle, attr_bitmap, &attr, &req_handle);
    EXPECT_INT_EQ(-ENOMEM, ret);
    mocker_clean();

    mocker(RaHdcSendMsgAsync, 1, -1);
    ret = ra_hdc_set_tp_attr_async(&ctx, tp_handle, attr_bitmap, &attr, &req_handle);
    EXPECT_INT_EQ(-1, ret);
    mocker_clean();

    mocker(RaHdcSendMsgAsync, 1, 0);
    ret = ra_hdc_set_tp_attr_async(&ctx, tp_handle, attr_bitmap, &attr, &req_handle);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();

    free(req_handle);
    req_handle = NULL;
}

void tc_ra_rs_set_tp_attr()
{
    union op_set_tp_attr_data data_in;
    union op_set_tp_attr_data data_out;
    int rcv_buf_len = 0;
    int op_result;
    int out_len;
    int ret;

    char* in_buf = calloc(1, sizeof(struct MsgHead) + sizeof(union op_set_tp_attr_data));
    char* out_buf = calloc(1, sizeof(struct MsgHead) + sizeof(union op_set_tp_attr_data));

    mocker_clean();
    data_in.tx_data.phy_id = 0;
    data_in.tx_data.dev_index = 0;
    memcpy_s(in_buf + sizeof(struct MsgHead), sizeof(union op_set_tp_attr_data),
        &data_in, sizeof(union op_set_tp_attr_data));
    ret = ra_rs_set_tp_attr(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    mocker(rs_set_tp_attr, 1, -1);
    ret = ra_rs_set_tp_attr(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    free(in_buf);
    in_buf = NULL;
    free(out_buf);
    out_buf = NULL;
}

void tc_ra_ctx_get_cr_err_info_list()
{
    struct ra_ctx_handle ctx_handle = {0};
    struct CrErrInfo info_list[1] = {0};
    unsigned int num = 0;
    int ret = 0;

    mocker_clean();
    ret = ra_ctx_get_cr_err_info_list(NULL, NULL, NULL);
    EXPECT_INT_EQ(ret, 128103);

    ret = ra_ctx_get_cr_err_info_list(&ctx_handle, NULL, NULL);
    EXPECT_INT_EQ(ret, 128103);

    ret = ra_ctx_get_cr_err_info_list(&ctx_handle, info_list, NULL);
    EXPECT_INT_EQ(ret, 128103);

    ret = ra_ctx_get_cr_err_info_list(&ctx_handle, info_list, &num);
    EXPECT_INT_EQ(ret, 128103);

    num = CR_ERR_INFO_MAX_NUM + 1;
    ret = ra_ctx_get_cr_err_info_list(&ctx_handle, info_list, &num);
    EXPECT_INT_EQ(ret, 128103);

    num = 1;
    mocker(ra_hdc_ctx_get_cr_err_info_list, 1, -1);
    ret = ra_ctx_get_cr_err_info_list(&ctx_handle, info_list, &num);
    EXPECT_INT_EQ(ret, 128100);
    mocker_clean();

    mocker(ra_hdc_ctx_get_cr_err_info_list, 1, 0);
    ret = ra_ctx_get_cr_err_info_list(&ctx_handle, info_list, &num);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

void tc_ra_hdc_ctx_get_cr_err_info_list()
{
    struct ra_ctx_handle ctx_handle = {0};
    struct CrErrInfo info_list[1] = {0};
    unsigned int num = 1;
    int ret = 0;

    mocker_clean();
    mocker(RaHdcProcessMsg, 1, -1);
    ret = ra_hdc_ctx_get_cr_err_info_list(&ctx_handle, info_list, &num);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

    mocker(RaHdcProcessMsg, 1, 0);
    ret = ra_hdc_ctx_get_cr_err_info_list(&ctx_handle, info_list, &num);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

void tc_ra_rs_ctx_get_cr_err_info_list()
{
    union op_ctx_get_cr_err_info_list_data data_out = {0};
    union op_ctx_get_cr_err_info_list_data data_in = {0};

    int rcv_buf_len = 0;
    int op_result;
    int out_len;
    int ret;

    char* in_buf = calloc(1, sizeof(struct MsgHead) + sizeof(union op_ctx_get_cr_err_info_list_data));
    char* out_buf = calloc(1, sizeof(struct MsgHead) + sizeof(union op_ctx_get_cr_err_info_list_data));

    data_in.tx_data.phy_id = 0;
    data_in.tx_data.dev_index = 0;
    memcpy_s(in_buf + sizeof(struct MsgHead), sizeof(union op_ctx_get_cr_err_info_list_data),
        &data_in, sizeof(union op_ctx_get_cr_err_info_list_data));
    data_in.tx_data.num = CQE_ERR_INFO_MAX_NUM;
    mocker(rs_ctx_get_cr_err_info_list, 1, 0);
    ret = ra_rs_ctx_get_cr_err_info_list(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(op_result, 0);
    mocker_clean();

    mocker(rs_ctx_get_cr_err_info_list, 1, -1);
    ret = ra_rs_ctx_get_cr_err_info_list(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(op_result, -1);
    mocker_clean();

    free(in_buf);
    in_buf = NULL;
    free(out_buf);
    out_buf = NULL;
}

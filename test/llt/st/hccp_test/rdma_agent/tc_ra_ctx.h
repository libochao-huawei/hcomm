/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: ra ctx unit test.
 * Author:
 * Create: 2025-03-31
 */

#ifndef TC_RA_CTX_H
#define TC_RA_CTX_H
#ifdef __cplusplus
extern "C" {
#endif
void tc_ra_get_dev_eid_info_num();
void tc_ra_get_dev_eid_info_list();
void tc_ra_ctx_init();
void tc_ra_get_dev_base_attr();
void tc_ra_ctx_deinit();
void tc_ra_ctx_token_id_alloc();
void tc_ra_rs_ctx_token_id_alloc();
void tc_ra_ctx_lmem_register();
void tc_ra_ctx_rmem_import();
void tc_ra_ctx_chan_create();
void tc_ra_ctx_cq_create();
void tc_ra_ctx_qp_create();
void tc_ra_ctx_qp_import();
void tc_ra_ctx_qp_bind();
void tc_ra_rs_ctx_ops();
void tc_ra_batch_send_wr();
void tc_ra_ctx_update_ci();
void tc_ra_custom_channel();
void tc_ra_get_tp_info_list_async();
void tc_ra_hdc_get_tp_info_list_async();
void tc_ra_rs_get_tp_info_list();
void tc_ra_hdc_pool_create();
void tc_ra_rs_async_hdc_session_connect();
void tc_hdc_async_recv_pkt();
#ifdef __cplusplus
}
#endif
#endif // TC_RA_CTX_H
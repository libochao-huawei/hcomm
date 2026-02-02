/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
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
void tc_ra_ctx_lmem_register();
void tc_ra_ctx_rmem_import();
void tc_ra_ctx_chan_create();
void tc_ra_ctx_token_id_alloc();
void tc_ra_ctx_token_id_alloc1();
void tc_ra_ctx_token_id_alloc2();
void tc_ra_ctx_token_id_alloc3();
void tc_ra_ctx_cq_create();
void tc_ra_ctx_qp_create();
void tc_ra_ctx_qp_import();
void tc_ra_ctx_qp_bind();
void tc_ra_batch_send_wr();
void tc_ra_ctx_update_ci();
void tc_ra_custom_channel();
void tc_ra_get_eid_by_ip();
void tc_ra_hdc_get_eid_by_ip();
void tc_ra_rs_get_eid_by_ip();
void tc_ra_peer_get_eid_by_ip();
void tc_ra_ctx_get_aux_info();
void tc_ra_hdc_ctx_get_aux_info();
void tc_ra_rs_ctx_get_aux_info();
void tc_ra_ctx_get_cr_err_info_list();
void tc_ra_hdc_ctx_get_cr_err_info_list();
void tc_ra_rs_ctx_get_cr_err_info_list();

void tc_ra_get_tp_info_list_async();
void tc_ra_hdc_get_tp_info_list_async();
void tc_ra_rs_get_tp_info_list();
void tc_ra_rs_async_hdc_session_connect();
void tc_ra_hdc_async_send_pkt();
void tc_ra_hdc_pool_add_task();
void tc_ra_hdc_async_recv_pkt();
void tc_hdc_async_recv_pkt();
void tc_ra_hdc_pool_create();
void tc_ra_async_handle_pkt();
void tc_ra_hdc_async_handle_socket_listen_start();
void tc_ra_hdc_async_handle_qp_import();
void tc_ra_peer_ctx_init();
void tc_ra_peer_ctx_deinit();
void tc_ra_peer_get_dev_eid_info_num();
void tc_ra_peer_get_dev_eid_info_list();
void tc_ra_peer_ctx_token_id_alloc();
void tc_ra_peer_ctx_token_id_free();
void tc_ra_peer_ctx_lmem_register();
void tc_ra_peer_ctx_lmem_unregister();
void tc_ra_peer_ctx_rmem_import();
void tc_ra_peer_ctx_rmem_unimport();
void tc_ra_peer_ctx_chan_create();
void tc_ra_peer_ctx_chan_destroy();
void tc_ra_peer_ctx_cq_create();
void tc_ra_peer_ctx_cq_destroy();
void tc_ra_peer_ctx_qp_create();
void tc_ra_ctx_prepare_qp_create();
void tc_ra_peer_ctx_qp_destroy();
void tc_ra_peer_ctx_qp_import();
void tc_ra_peer_ctx_qp_unimport();
void tc_ra_peer_ctx_qp_bind();
void tc_ra_peer_ctx_qp_unbind();
void tc_ra_ctx_qp_destroy_batch_async();
void tc_qp_destroy_batch_param_check();
void tc_ra_hdc_ctx_qp_destroy_batch_async();
void tc_ra_rs_ctx_qp_destroy_batch();
void tc_ra_ctx_qp_query_batch();
void tc_qp_query_batch_param_check();
void tc_ra_hdc_ctx_qp_query_batch();
void tc_ra_rs_ctx_qp_query_batch();
void tc_ra_get_tp_attr_async();
void tc_ra_hdc_get_tp_attr_async();
void tc_ra_rs_get_tp_attr();
void tc_ra_set_tp_attr_async();
void tc_ra_hdc_set_tp_attr_async();
void tc_ra_rs_set_tp_attr();
#ifdef __cplusplus
}
#endif
#endif

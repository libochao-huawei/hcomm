/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: ra async unit test.
 * Create: 2025-04-12
 */

#ifndef TC_RA_ASYNC_H
#define TC_RA_ASYNC_H
#ifdef __cplusplus
extern "C" {
#endif
void tc_ra_ctx_lmem_register_async();
void tc_ra_ctx_lmem_unregister_async();
void tc_ra_ctx_qp_create_async();
void tc_ra_ctx_qp_destroy_async();
void tc_ra_ctx_qp_import_async();
void tc_ra_ctx_qp_unimport_async();
void tc_ra_socket_send_async();
void tc_ra_socket_recv_async();
void tc_ra_get_async_req_result();
void tc_ra_socket_batch_connect_async();
void tc_ra_socket_listen_start_async();
void tc_ra_socket_listen_stop_async();
void tc_ra_socket_batch_close_async();
void tc_ra_hdc_async_init_session();
void tc_ra_get_eid_by_ip_async();
void tc_ra_hdc_get_eid_by_ip_async();
void tc_ra_hdc_async_session_close();
#ifdef __cplusplus
}
#endif
#endif // TC_RA_ASYNC_H

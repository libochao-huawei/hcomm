/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
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
#endif

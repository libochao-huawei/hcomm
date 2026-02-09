/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef _TC_UT_RS_CTX_H
#define _TC_UT_RS_CTX_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif
void tc_rs_get_dev_eid_info_num();
void tc_rs_get_dev_eid_info_list();
void tc_rs_ctx_init();
void tc_rs_ctx_deinit();
void tc_rs_ctx_token_id_alloc();
void tc_rs_ctx_token_id_free();
void tc_rs_ctx_lmem_reg();
void tc_rs_ctx_lmem_unreg();
void tc_rs_ctx_rmem_import();
void tc_rs_ctx_rmem_unimport();
void tc_rs_ctx_chan_create();
void tc_rs_ctx_chan_destroy();
void tc_rs_ctx_cq_create();
void tc_rs_ctx_cq_destroy();
void tc_rs_ctx_qp_create();
void tc_rs_ctx_qp_destroy();
void tc_rs_ctx_qp_import();
void tc_rs_ctx_qp_unimport();
void tc_rs_ctx_qp_bind();
void tc_rs_ctx_qp_unbind();
void tc_rs_ctx_batch_send_wr();
void tc_rs_ctx_update_ci();
void tc_rs_ctx_custom_channel();
void tc_rs_ctx_esched();
void tc_dl_ccu_api_init();
void tc_rs_get_tp_info_list();
void tc_rs_ctx_qp_destroy_batch();
void tc_rs_ctx_qp_query_batch();
void tc_rs_net_api_init_deinit();
void tc_rs_net_alloc_jfc_id();
void tc_rs_net_free_jfc_id();
void tc_rs_net_alloc_jetty_id();
void tc_rs_net_free_jetty_id();
void tc_rs_net_get_cqe_base_addr();
void tc_rs_ccu_get_cqe_base_addr();
void tc_rs_ctx_get_aux_info();
void tc_rs_get_tp_attr();
void tc_rs_set_tp_attr();
void tc_rs_ctx_get_cr_err_info_list();
#ifdef __cplusplus
}
#endif
#endif

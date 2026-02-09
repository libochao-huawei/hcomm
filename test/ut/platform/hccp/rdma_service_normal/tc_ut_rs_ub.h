/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef _TC_UT_RS_UB_H
#define _TC_UT_RS_UB_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif
void tc_rs_ub_get_rdev_cb();
void tc_rs_urma_api_init_abnormal();
void tc_rs_ub_v2();
void tc_rs_ub_get_dev_eid_info_num();
void tc_rs_ub_get_dev_eid_info_list();
struct rs_cb *tc_rs_ub_v2_init(int mode, unsigned int *dev_index);
void tc_rs_ub_v2_deinit(struct rs_cb *rs_cb, int mode, unsigned int dev_index);
void tc_rs_ub_ctx_token_id_alloc();
void tc_rs_ub_ctx_token_id_alloc1();
void tc_rs_ub_ctx_token_id_alloc2();
void tc_rs_ub_ctx_token_id_alloc3();
void tc_rs_ub_ctx_jfce_create();
void tc_rs_ub_ctx_jfc_create();
void tc_rs_ub_ctx_jfc_create_normal();
void tc_rs_ub_ctx_jetty_create();
void tc_rs_ub_ctx_jetty_import();
void tc_rs_ub_ctx_jetty_bind();
void tc_rs_ub_ctx_batch_send_wr();
void tc_rs_ub_free_cb_list();
void tc_rs_ub_ctx_ext_jetty_create();
void tc_rs_ub_ctx_rmem_import();
void tc_rs_get_tp_info_list();
void tc_rs_ub_ctx_drv_jetty_import();
void tc_rs_ub_dev_cb_init();
void tc_rs_ub_ctx_init();
void tc_rs_ub_ctx_jfc_destroy();
void tc_rs_ub_ctx_ext_jetty_delete();
void tc_rs_ub_ctx_chan_create();
void tc_rs_ub_ctx_deinit();
void tc_rs_ub_init_seg_cb();
void tc_rs_ub_ctx_lmem_reg();
void tc_rs_ub_ctx_jfc_create_fail();
void tc_rs_ub_ctx_init_jetty_cb();
void tc_rs_ub_ctx_jetty_create_fail();
void tc_rs_ub_ctx_jetty_import_fail();
void tc_rs_ub_ctx_batch_send_wr_fail();
void tc_rs_ub_ctx_jetty_destroy_batch();
void tc_rs_ub_ctx_query_jetty_batch();
void tc_rs_get_eid_by_ip();
void tc_rs_ub_get_eid_by_ip();
void tc_rs_ub_ctx_get_aux_info();
void tc_rs_ub_get_tp_attr();
void tc_rs_ub_set_tp_attr();
void tc_rs_epoll_event_jfc_in_handle();
void tc_rs_handle_epoll_poll_jfc();
void tc_rs_jfc_callback_process();
#ifdef __cplusplus
}
#endif
#endif

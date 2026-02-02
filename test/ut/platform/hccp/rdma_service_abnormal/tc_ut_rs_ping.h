/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef TC_UT_RS_PING_H
#define TC_UT_RS_PING_H

void tc_rs_ping_handle_init();
void tc_rs_ping_handle_deinit();
void tc_rs_ping_init();
void tc_rs_ping_target_add();
void tc_rs_ping_task_start();
void tc_rs_ping_get_results();
void tc_rs_ping_task_stop();
void tc_rs_ping_target_del();
void tc_rs_ping_deinit();
void tc_rs_ping_urma_check_fd();
void tc_rs_ping_cb_get_ib_ctx_and_index();

#endif

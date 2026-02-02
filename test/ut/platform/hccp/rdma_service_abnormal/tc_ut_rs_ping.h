/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: rs ping abnormal unit test.
 * Author:
 * Create: 2024-09-23
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

#endif  // TC_UT_RS_PING_H
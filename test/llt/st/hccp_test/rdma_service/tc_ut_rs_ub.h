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
void tc_rs_ub_ctx_jfce_create();
void tc_rs_ub_ctx_jfc_create();
void tc_rs_ub_ctx_jetty_create();
void tc_rs_ub_ctx_jetty_import();
void tc_rs_ub_ctx_jetty_bind();
void tc_rs_ub_ctx_batch_send_wr();
void tc_rs_ub_free_cb_list();
void tc_rs_ub_ctx_ext_jetty_create();
void tc_rs_ub_ctx_rmem_import();
void tc_rs_get_tp_info_list();
void tc_rs_ub_ctx_drv_jetty_import();
#ifdef __cplusplus
}
#endif
#endif

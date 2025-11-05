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
#ifdef __cplusplus
}
#endif
#endif
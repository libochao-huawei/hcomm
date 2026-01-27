/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2025. All rights reserved.
 * Description: rdma_server ctx ub JFC internal interface declaration
 * Create: 2025-12-17
 */

#ifndef RS_UB_JFC_H
#define RS_UB_JFC_H

#include "urma_types.h"
#include "udma_u_ctl.h"
#include "rs_ctx_inner.h"

union create_jfc_cfg {
    struct udma_u_lock_jfc_cfg lock_jfc_cfg;
    struct udma_u_jfc_cfg_ex jfc_cfg_ex;
};

int rs_ub_delete_jfc_ext(struct rs_ub_dev_cb *dev_cb, struct rs_ctx_jfc_cb *jfc_cb);
int rs_ub_ctx_jfc_create_ext(struct rs_ctx_jfc_cb *ctx_jfc_cb, urma_jfc_cfg_t jfc_cfg, urma_jfc_t **jfc);

#endif // RS_UB_JFC_H

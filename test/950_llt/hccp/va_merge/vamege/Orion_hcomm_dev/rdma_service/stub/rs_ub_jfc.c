/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * Description: stub rdma service ub extern jfc interface
 * Create: 2025-12-17
 */

#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include "user_log.h"
#include "ra_rs_err.h"
#include "rs_ctx_inner.h"
#include "rs_ub_jfc.h"

int rs_ub_delete_jfc_ext(struct rs_ub_dev_cb *dev_cb, struct rs_ctx_jfc_cb *jfc_cb)
{
    hccp_err("product type do not support");
    return -EOPENSRC;
}

int rs_ub_ctx_jfc_create_ext(struct rs_ctx_jfc_cb *ctx_jfc_cb, urma_jfc_cfg_t jfc_cfg, urma_jfc_t **jfc)
{
    hccp_err("product type do not support");
    return -EOPENSRC;
}

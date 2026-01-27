/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * Description: stub rdma service ub extern jetty interface
 * Create: 2025-12-20
 */

#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include "user_log.h"
#include "ra_rs_err.h"
#include "rs_ctx_inner.h"
#include "rs_ub_jetty.h"

void rs_ub_ctx_ext_jetty_create(struct rs_ctx_jetty_cb *jettyCb, urma_jetty_cfg_t *jettyCfg)
{
    hccp_err("product type do not support");
    jettyCb->jetty = NULL;
    return;
}

void rs_ub_ctx_ext_jetty_delete(struct rs_ctx_jetty_cb *jettyCb)
{
    hccp_err("product type do not support");
    return;
}

void rs_ub_va_munmap_batch(struct rs_ctx_jetty_cb **jettyCbArr, unsigned int num)
{
    hccp_err("product type do not support");
    return;
}

void rs_ub_free_jetty_id_batch(struct rs_ctx_jetty_cb **jettyCbArr, unsigned int num)
{
    hccp_err("product type do not support");
    return;
}

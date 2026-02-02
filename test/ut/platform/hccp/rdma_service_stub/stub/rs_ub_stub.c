/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "udma_u_ctl.h"
#include "rs_ctx_inner.h"
#include "rs_ub.h"

int rs_ub_ctx_reg_jetty_db(struct rs_ctx_jetty_cb *jetty_cb, struct udma_u_jetty_info *jetty_info)
{
    return 0;
}

void rs_ub_ctx_ext_jetty_create(struct rs_ctx_jetty_cb *jetty_cb, urma_jetty_cfg_t *jetty_cfg)
{
    return;
}

void rs_ub_ctx_ext_jetty_delete(struct rs_ctx_jetty_cb *jetty_cb)
{
    return;
}

int rs_ub_ctx_jfc_create_ext(struct rs_ctx_jfc_cb *ctx_jfc_cb, urma_jfc_cfg_t jfc_cfg, urma_jfc_t **jfc)
{
    return 0;
}
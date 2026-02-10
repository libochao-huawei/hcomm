/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
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

int rs_ub_ctx_jfc_create_ext(struct rs_ctx_jfc_cb *ctx_jfc_cb, urma_jfc_cfg_t *jfc_cfg, urma_jfc_t **jfc)
{
    hccp_err("product type do not support");
    return -EOPENSRC;
}

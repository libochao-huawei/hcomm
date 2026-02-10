/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
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
int rs_ub_ctx_jfc_create_ext(struct rs_ctx_jfc_cb *ctx_jfc_cb, urma_jfc_cfg_t *jfc_cfg, urma_jfc_t **jfc);

#endif // RS_UB_JFC_H

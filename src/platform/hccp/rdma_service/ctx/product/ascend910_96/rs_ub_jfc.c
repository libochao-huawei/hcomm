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
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dlfcn.h>
#include <urma_opcode.h>
#include <udma_u_ctl.h>
#include "securec.h"
#include "user_log.h"
#include "dl_hal_function.h"
#include "dl_urma_function.h"
#include "ra_rs_err.h"
#include "ra_rs_ctx.h"
#include "rs_inner.h"
#include "rs_ctx_inner.h"
#include "rs_ctx.h"
#include "rs_ub_jfc.h"

int rs_ub_delete_jfc_ext(struct rs_ub_dev_cb *dev_cb, struct rs_ctx_jfc_cb *jfc_cb)
{
    urma_user_ctl_out_t out = {0};
    urma_user_ctl_in_t in = {0};
    int out_buff = 0;
    int ret;

    in.addr = (uint64_t)(uintptr_t)&(jfc_cb->jfc_addr);
    in.len = sizeof(urma_jfc_t *);
    in.opcode = (jfc_cb->jfc_type == JFC_MODE_CCU_POLL && jfc_cb->ccu_ex_cfg.valid) ?
        UDMA_U_USER_CTL_DELETE_CCU_JFC_EX : UDMA_U_USER_CTL_DELETE_JFC_EX;
    out.addr = (uint64_t)(uintptr_t)&out_buff;
    out.len = sizeof(int);

    ret = rs_urma_user_ctl(dev_cb->urma_ctx, &in, &out);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_urma_user_ctl delete jfc failed, ret:%d errno:%d", ret, errno), -EOPENSRC);

    return 0;
}

int rs_ub_ctx_jfc_create_ext(struct rs_ctx_jfc_cb *ctx_jfc_cb, urma_jfc_cfg_t *jfc_cfg, urma_jfc_t **jfc)
{
    union create_jfc_cfg create_jfc_in = {0};
    urma_user_ctl_out_t out = {0};
    urma_user_ctl_in_t in = {0};
    urma_jfc_t *jfc_out = NULL;
    int ret;

    if (ctx_jfc_cb->jfc_type == JFC_MODE_CCU_POLL && ctx_jfc_cb->ccu_ex_cfg.valid) {
        create_jfc_in.lock_jfc_cfg.base_cfg = *jfc_cfg;
        create_jfc_in.lock_jfc_cfg.ccu_cfg.ccu_cqe_flag = ctx_jfc_cb->ccu_ex_cfg.cqe_flag;
        in.len = (uint32_t)sizeof(struct udma_u_lock_jfc_cfg);
        in.opcode = UDMA_U_USER_CTL_CREATE_CCU_JFC_EX;
        in.addr = (uint64_t)(uintptr_t)&create_jfc_in.lock_jfc_cfg;
    } else {
        create_jfc_in.jfc_cfg_ex.base_cfg = *jfc_cfg;
        create_jfc_in.jfc_cfg_ex.jfc_mode = (enum udma_u_jfc_type)ctx_jfc_cb->jfc_type;
        in.len = (uint32_t)sizeof(struct udma_u_jfc_cfg_ex);
        in.opcode = UDMA_U_USER_CTL_CREATE_JFC_EX;
        in.addr = (uint64_t)(uintptr_t)&create_jfc_in.jfc_cfg_ex;
    }

    out.len = sizeof(urma_jfc_t *);
    out.addr = (uint64_t)(uintptr_t)&jfc_out;

    ret = rs_urma_user_ctl(ctx_jfc_cb->dev_cb->urma_ctx, &in, &out);
    if (ret != 0) {
        hccp_err("rs_urma_user_ctl create jfc failed, ret:%d errno:%d", ret, errno);
        return -EOPENSRC;
    }

    *jfc = jfc_out;

    return ret;
}

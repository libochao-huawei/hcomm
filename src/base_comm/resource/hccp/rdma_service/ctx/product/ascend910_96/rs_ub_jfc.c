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

int RsUbDeleteJfcExt(struct RsUbDevCb *devCb, struct RsCtxJfcCb *jfcCb)
{
    urma_user_ctl_out_t out = {0};
    urma_user_ctl_in_t in = {0};
    int outBuff = 0;
    int ret;

    in.addr = (uint64_t)(uintptr_t)&(jfcCb->jfcAddr);
    in.len = sizeof(urma_jfc_t *);
    in.opcode = (jfcCb->jfcType == JFC_MODE_CCU_POLL && jfcCb->ccuExCfg.valid) ?
        UDMA_U_USER_CTL_DELETE_CCU_JFC_EX : UDMA_U_USER_CTL_DELETE_JFC_EX;
    out.addr = (uint64_t)(uintptr_t)&outBuff;
    out.len = sizeof(int);

    ret = RsUrmaUserCtl(devCb->urmaCtx, &in, &out);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_urma_user_ctl delete jfc failed, ret:%d errno:%d", ret, errno), -EOPENSRC);

    return 0;
}

int RsUbCtxJfcCreateExt(struct RsCtxJfcCb *ctxJfcCb, urma_jfc_cfg_t *jfcCfg, urma_jfc_t **jfc)
{
    union CreateJfcCfg createJfcIn = {0};
    urma_user_ctl_out_t out = {0};
    urma_user_ctl_in_t in = {0};
    urma_jfc_t *jfcOut = NULL;
    int ret;

    if (ctxJfcCb->jfcType == JFC_MODE_CCU_POLL && ctxJfcCb->ccuExCfg.valid) {
        createJfcIn.lockJfcCfg.base_cfg = *jfcCfg;
        createJfcIn.lockJfcCfg.ccu_cfg.ccu_cqe_flag = ctxJfcCb->ccuExCfg.cqeFlag;
        in.len = (uint32_t)sizeof(struct udma_u_lock_jfc_cfg);
        in.opcode = UDMA_U_USER_CTL_CREATE_CCU_JFC_EX;
        in.addr = (uint64_t)(uintptr_t)&createJfcIn.lockJfcCfg;
    } else {
        createJfcIn.jfcCfgEx.base_cfg = *jfcCfg;
        createJfcIn.jfcCfgEx.jfc_mode = (enum udma_u_jfc_type)ctxJfcCb->jfcType;
        in.len = (uint32_t)sizeof(struct udma_u_jfc_cfg_ex);
        in.opcode = UDMA_U_USER_CTL_CREATE_JFC_EX;
        in.addr = (uint64_t)(uintptr_t)&createJfcIn.jfcCfgEx;
    }

    out.len = sizeof(urma_jfc_t *);
    out.addr = (uint64_t)(uintptr_t)&jfcOut;

    ret = RsUrmaUserCtl(ctxJfcCb->devCb->urmaCtx, &in, &out);
    if (ret != 0) {
        hccp_err("rs_urma_user_ctl create jfc failed, ret:%d errno:%d", ret, errno);
        return -EOPENSRC;
    }

    *jfc = jfcOut;

    return ret;
}

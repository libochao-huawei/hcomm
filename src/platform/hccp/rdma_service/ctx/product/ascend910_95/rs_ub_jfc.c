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
#include "securec.h"
#include "urma_opcode.h"
#include "user_log.h"
#include "dl_hal_function.h"
#include "dl_urma_function.h"
#include "dl_net_function.h"
#include "dl_ccu_function.h"
#include "ra_rs_err.h"
#include "ra_rs_ctx.h"
#include "rs_inner.h"
#include "rs_ctx_inner.h"
#include "rs_ctx.h"
#include "rs_ub_jfc.h"

struct ExtJfcAttr {
    urma_jfc_t *jfc;
    unsigned int jfcId;
    unsigned long long cqeBaseAddrVa;
};

STATIC int RsInitJfcAttr(struct RsCtxJfcCb *ctxJfcCb, urma_jfc_cfg_t *jfcCfg, struct ExtJfcAttr *jfcAttr)
{
    int ret = 0;

    ret = RsUrmaAllocJfc(ctxJfcCb->devCb->urmaCtx, jfcCfg, &jfcAttr->jfc);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_urma_alloc_jfc failed, ret:%d errno:%d", ret, errno), -EOPENSRC);

    if (ctxJfcCb->jfcType == JFC_MODE_CCU_POLL) {
        ret = RsCcuGetCqeBaseAddr(ctxJfcCb->devCb->devAttr.ub.dieId, &jfcAttr->cqeBaseAddrVa);
        if (ret != 0 || jfcAttr->cqeBaseAddrVa == 0) {
            hccp_err("rs_ccu_get_cqe_base_addr failed, ret:%d, dieId:%u", ret, ctxJfcCb->devCb->devAttr.ub.dieId);
            ret = -EOPENSRC;
            goto free_jfc;
        }

    } else {
        ret = RsNetGetCqeBaseAddr(ctxJfcCb->devCb->devAttr.ub.dieId, &jfcAttr->cqeBaseAddrVa);
        if (ret != 0 || jfcAttr->cqeBaseAddrVa == 0) {
            hccp_err("rs_net_get_cqe_base_addr failed, ret:%d, dieId:%u", ret, ctxJfcCb->devCb->devAttr.ub.dieId);
            ret = -EOPENSRC;
            goto free_jfc;
        }
    }

    ret = RsNetAllocJfcId(ctxJfcCb->devCb->urmaDev->name, ctxJfcCb->jfcType, &jfcAttr->jfcId);
    if (ret != 0) {
        hccp_err("rs_net_alloc_jfc_id failed, ret:%d", ret);
        goto free_jfc;
    }

    return 0;

free_jfc:
    (void)RsUrmaFreeJfc(jfcAttr->jfc);
    return ret;
}

STATIC void RsDeinitJfcAttr(struct RsCtxJfcCb *ctxJfcCb, urma_jfc_cfg_t *jfcCfg, struct ExtJfcAttr *jfcAttr)
{
    (void)RsNetFreeJfcId(ctxJfcCb->devCb->urmaDev->name, ctxJfcCb->jfcType, jfcAttr->jfcId);
    (void)RsUrmaFreeJfc(jfcAttr->jfc);
}

STATIC int RsSetJfcOpt(struct ExtJfcAttr *jfcAttr)
{
    int ret = 0;

    ret = RsUrmaSetJfcOpt(jfcAttr->jfc, URMA_JFC_ID, (void *)&jfcAttr->jfcId, sizeof(uint32_t));
    CHK_PRT_RETURN(ret != 0,
        hccp_err("rs_urma_set_jfc_opt URMA_JFC_ID failed, ret:%d, errno:%d", ret, errno), ret);

    ret = RsUrmaSetJfcOpt(jfcAttr->jfc, URMA_JFC_CQE_BASE_ADDR,
        (void *)&jfcAttr->cqeBaseAddrVa, sizeof(uint64_t));
    CHK_PRT_RETURN(ret != 0,
        hccp_err("rs_urma_set_jfc_opt URMA_JFC_CQE_BASE_ADDR failed, ret:%d, errno:%d", ret, errno), ret);

    return 0;
}

int RsUbCtxJfcCreateExt(struct RsCtxJfcCb *ctxJfcCb, urma_jfc_cfg_t jfcCfg, urma_jfc_t **jfc)
{
    struct ExtJfcAttr jfcAttr = {0};
    int ret = 0;

    ret = RsInitJfcAttr(ctxJfcCb, &jfcCfg, &jfcAttr);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_init_jfc_attr failed, ret:%d", ret), ret);

    ret = RsSetJfcOpt(&jfcAttr);
    if (ret != 0) {
        hccp_err("rs_set_jfc_attr failed, ret:%d", ret);
        goto deinit_attr;
    }

    ret = RsUrmaActiveJfc(jfcAttr.jfc);
    if (ret != 0) {
        hccp_err("rs_urma_active_jfc failed, jfcId:%u, ret:%d, errno:%d", jfcAttr.jfcId, ret, errno);
        goto deinit_attr;
    }

    *jfc = jfcAttr.jfc;
    return 0;

deinit_attr:
    (void)RsDeinitJfcAttr(ctxJfcCb, &jfcCfg, &jfcAttr);
    *jfc = NULL;
    return ret;
}

int RsUbDeleteJfcExt(struct RsUbDevCb *devCb, struct RsCtxJfcCb *jfcCb)
{
    urma_jfc_t *jfc = (urma_jfc_t *)(uintptr_t)(jfcCb->jfcAddr);
    unsigned int jfcId = jfc->jfc_id.id;
    int ret = 0;

    ret = RsUrmaDeactiveJfc(jfc);
    if (ret != 0) {
        hccp_err("rs_urma_deactive_jfc failed, jfcId:%u, ret:%d, errno:%d", jfcId, ret, errno);
    }

    ret = RsUrmaFreeJfc(jfc);
    if (ret != 0) {
        hccp_err("rs_urma_free_jfc failed, jfcId:%u, ret:%d, errno:%d", jfcId, ret, errno);
    }

    ret = RsNetFreeJfcId(devCb->urmaDev->name, jfcCb->jfcType, jfcId);
    if (ret != 0) {
        hccp_err("rs_net_free_jfc_id failed, jfcId:%u, ret:%d", jfcId, ret);
    }

    return ret;
}

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

struct ext_jfc_attr {
    urma_jfc_t *jfc;
    unsigned int jfc_id;
    unsigned long long cqe_base_addr_va;
};

STATIC int rs_init_jfc_attr(struct rs_ctx_jfc_cb *ctx_jfc_cb, urma_jfc_cfg_t *jfc_cfg, struct ext_jfc_attr *jfc_attr)
{
    int ret = 0;

    ret = rs_urma_alloc_jfc(ctx_jfc_cb->dev_cb->urma_ctx, jfc_cfg, &jfc_attr->jfc);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_urma_alloc_jfc failed, ret:%d errno:%d", ret, errno), -EOPENSRC);

    if (ctx_jfc_cb->jfc_type == JFC_MODE_CCU_POLL) {
        ret = rs_ccu_get_cqe_base_addr(ctx_jfc_cb->dev_cb->dev_attr.ub.die_id, &jfc_attr->cqe_base_addr_va);
        if (ret != 0 || jfc_attr->cqe_base_addr_va == 0) {
            hccp_err("rs_ccu_get_cqe_base_addr failed, ret:%d, die_id:%u", ret, ctx_jfc_cb->dev_cb->dev_attr.ub.die_id);
            ret = -EOPENSRC;
            goto free_jfc;
        }

    } else {
        ret = rs_net_get_cqe_base_addr(ctx_jfc_cb->dev_cb->dev_attr.ub.die_id, &jfc_attr->cqe_base_addr_va);
        if (ret != 0 || jfc_attr->cqe_base_addr_va == 0) {
            hccp_err("rs_net_get_cqe_base_addr failed, ret:%d, die_id:%u", ret, ctx_jfc_cb->dev_cb->dev_attr.ub.die_id);
            ret = -EOPENSRC;
            goto free_jfc;
        }
    }

    ret = rs_net_alloc_jfc_id(ctx_jfc_cb->dev_cb->urma_dev->name, ctx_jfc_cb->jfc_type, &jfc_attr->jfc_id);
    if (ret != 0) {
        hccp_err("rs_net_alloc_jfc_id failed, ret:%d", ret);
        goto free_jfc;
    }

    return 0;

free_jfc:
    (void)rs_urma_free_jfc(jfc_attr->jfc);
    return ret;
}

STATIC void rs_deinit_jfc_attr(struct rs_ctx_jfc_cb *ctx_jfc_cb, urma_jfc_cfg_t *jfc_cfg, struct ext_jfc_attr *jfc_attr)
{
    (void)rs_net_free_jfc_id(ctx_jfc_cb->dev_cb->urma_dev->name, ctx_jfc_cb->jfc_type, jfc_attr->jfc_id);
    (void)rs_urma_free_jfc(jfc_attr->jfc);
}

STATIC int rs_set_jfc_opt(struct ext_jfc_attr *jfc_attr)
{
    int ret = 0;

    ret = rs_urma_set_jfc_opt(jfc_attr->jfc, URMA_JFC_ID, (void *)&jfc_attr->jfc_id, sizeof(uint32_t));
    CHK_PRT_RETURN(ret != 0,
        hccp_err("rs_urma_set_jfc_opt URMA_JFC_ID failed, ret:%d, errno:%d", ret, errno), ret);

    ret = rs_urma_set_jfc_opt(jfc_attr->jfc, URMA_JFC_CQE_BASE_ADDR,
        (void *)&jfc_attr->cqe_base_addr_va, sizeof(uint64_t));
    CHK_PRT_RETURN(ret != 0,
        hccp_err("rs_urma_set_jfc_opt URMA_JFC_CQE_BASE_ADDR failed, ret:%d, errno:%d", ret, errno), ret);

    return 0;
}

int rs_ub_ctx_jfc_create_ext(struct rs_ctx_jfc_cb *ctx_jfc_cb, urma_jfc_cfg_t jfc_cfg, urma_jfc_t **jfc)
{
    struct ext_jfc_attr jfc_attr = {0};
    int ret = 0;

    ret = rs_init_jfc_attr(ctx_jfc_cb, &jfc_cfg, &jfc_attr);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_init_jfc_attr failed, ret:%d", ret), ret);

    ret = rs_set_jfc_opt(&jfc_attr);
    if (ret != 0) {
        hccp_err("rs_set_jfc_attr failed, ret:%d", ret);
        goto deinit_attr;
    }

    ret = rs_urma_active_jfc(jfc_attr.jfc);
    if (ret != 0) {
        hccp_err("rs_urma_active_jfc failed, jfc_id:%u, ret:%d, errno:%d", jfc_attr.jfc_id, ret, errno);
        goto deinit_attr;
    }

    *jfc = jfc_attr.jfc;
    return 0;

deinit_attr:
    (void)rs_deinit_jfc_attr(ctx_jfc_cb, &jfc_cfg, &jfc_attr);
    *jfc = NULL;
    return ret;
}

int rs_ub_delete_jfc_ext(struct rs_ub_dev_cb *dev_cb, struct rs_ctx_jfc_cb *jfc_cb)
{
    urma_jfc_t *jfc = (urma_jfc_t *)(uintptr_t)(jfc_cb->jfc_addr);
    unsigned int jfc_id = jfc->jfc_id.id;
    int ret = 0;

    ret = rs_urma_deactive_jfc(jfc);
    if (ret != 0) {
        hccp_err("rs_urma_deactive_jfc failed, jfc_id:%u, ret:%d, errno:%d", jfc_id, ret, errno);
    }

    ret = rs_urma_free_jfc(jfc);
    if (ret != 0) {
        hccp_err("rs_urma_free_jfc failed, jfc_id:%u, ret:%d, errno:%d", jfc_id, ret, errno);
    }

    ret = rs_net_free_jfc_id(dev_cb->urma_dev->name, jfc_cb->jfc_type, jfc_id);
    if (ret != 0) {
        hccp_err("rs_net_free_jfc_id failed, jfc_id:%u, ret:%d", jfc_id, ret);
    }

    return ret;
}

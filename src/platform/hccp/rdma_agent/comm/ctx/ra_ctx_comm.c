/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <errno.h>
#include "securec.h"
#include "ra_rs_err.h"
#include "ra_ctx_comm.h"

int ra_ctx_prepare_lmem_register(struct mr_reg_info_t *lmem_info, struct mem_reg_attr_t *mem_attr)
{
    struct ra_token_id_handle *token_id_handle = NULL;
    bool is_token_id_valid = false;

    mem_attr->mem = lmem_info->in.mem;
    is_token_id_valid = lmem_info->in.ub.flags.bs.token_id_valid == 1;
    CHK_PRT_RETURN(is_token_id_valid && lmem_info->in.ub.token_id_handle == NULL,
        hccp_err("[init][ra_ctx_lmem]lmem_info specify token id, but token_id_handle is NULL"), -EINVAL);
    mem_attr->ub.flags = lmem_info->in.ub.flags;
    mem_attr->ub.token_value = lmem_info->in.ub.token_value;
    token_id_handle = (struct ra_token_id_handle *)(lmem_info->in.ub.token_id_handle);
    mem_attr->ub.token_id_addr = is_token_id_valid ? token_id_handle->addr : 0;

    return 0;
}

void ra_ctx_get_lmem_info(struct mem_reg_info_t *mem_info, struct mr_reg_info_t *lmem_info,
    struct ra_lmem_handle *lmem_handle)
{
    lmem_info->out.key = mem_info->key;
    lmem_info->out.ub.token_id = mem_info->ub.token_id;
    lmem_info->out.ub.target_seg_handle = mem_info->ub.target_seg_handle;
    lmem_handle->addr = lmem_info->out.ub.target_seg_handle;
}

void ra_ctx_prepare_rmem_import(struct mr_import_info_t *rmem_info, struct mem_import_attr_t *mem_attr)
{
    mem_attr->key = rmem_info->in.key;
    mem_attr->ub.flags = rmem_info->in.ub.flags;
    mem_attr->ub.mapping_addr = rmem_info->in.ub.mapping_addr;
    mem_attr->ub.token_value = rmem_info->in.ub.token_value;
}

void ra_ctx_prepare_cq_create(struct cq_info_t *info, struct ctx_cq_attr *cq_attr)
{
    struct ra_chan_handle *chan_handle = NULL;

    cq_attr->depth = info->in.depth;
    cq_attr->ub.user_ctx = info->in.ub.user_ctx;
    cq_attr->ub.mode = info->in.ub.mode;
    cq_attr->ub.ceqn = info->in.ub.ceqn;
    cq_attr->ub.flag = info->in.ub.flag;
    if (info->in.chan_handle != NULL) {
        chan_handle = (struct ra_chan_handle *)info->in.chan_handle;
        cq_attr->chan_addr = chan_handle->addr;
    }
}

void ra_ctx_get_cq_create_info(struct ctx_cq_info *cq_info, struct cq_info_t *info)
{
    info->out.va = cq_info->addr;
    info->out.id = cq_info->ub.id;
    info->out.cqe_size = cq_info->ub.cqe_size;
    info->out.buf_addr = cq_info->ub.buf_addr;
    info->out.swdb_addr = cq_info->ub.swdb_addr;
}

int ra_ctx_prepare_qp_create(struct qp_create_attr *qp_attr, struct ctx_qp_attr *ctx_qp_attr)
{
    struct ra_token_id_handle *token_id_handle = NULL;

    CHK_PRT_RETURN(qp_attr->scq_handle == NULL, hccp_err("[init][ra_ctx_qp]scq_handle is NULL"), -EINVAL);
    CHK_PRT_RETURN(qp_attr->rcq_handle == NULL, hccp_err("[init][ra_ctx_qp]rcq_handle is NULL"), -EINVAL);

    ctx_qp_attr->scq_index = ((struct ra_cq_handle *)qp_attr->scq_handle)->addr;
    ctx_qp_attr->rcq_index = ((struct ra_cq_handle *)qp_attr->rcq_handle)->addr;
    ctx_qp_attr->srq_index = 0;
    ctx_qp_attr->sq_depth = qp_attr->sq_depth;
    ctx_qp_attr->rq_depth = qp_attr->rq_depth;
    ctx_qp_attr->transport_mode = qp_attr->transport_mode;

    ctx_qp_attr->ub.mode = qp_attr->ub.mode;
    ctx_qp_attr->ub.jetty_id = qp_attr->ub.jetty_id;
    ctx_qp_attr->ub.flag = qp_attr->ub.flag;
    ctx_qp_attr->ub.jfs_flag = qp_attr->ub.jfs_flag;
    ctx_qp_attr->ub.token_value = qp_attr->ub.token_value;
    ctx_qp_attr->ub.priority = qp_attr->ub.priority;
    ctx_qp_attr->ub.rnr_retry = qp_attr->ub.rnr_retry;
    ctx_qp_attr->ub.err_timeout = qp_attr->ub.err_timeout;

    if (qp_attr->ub.token_id_handle != NULL) {
        token_id_handle = (struct ra_token_id_handle *)(qp_attr->ub.token_id_handle);
        ctx_qp_attr->ub.token_id_addr = token_id_handle->addr;
    }

    return 0;
}

void ra_ctx_get_qp_create_info(struct ra_ctx_handle *ctx_handle, struct qp_create_attr *qp_attr,
    struct qp_create_info *qp_info, struct ra_ctx_qp_handle *qp_handle)
{
    qp_handle->dev_index = ctx_handle->dev_index;
    qp_handle->phy_id = ctx_handle->attr.phy_id;
    qp_handle->ctx_handle = ctx_handle;
    qp_handle->protocol = ctx_handle->protocol;
    qp_handle->id = qp_info->ub.id;
    (void)memcpy_s(&qp_handle->qp_attr, sizeof(struct qp_create_attr), qp_attr, sizeof(struct qp_create_attr));
    (void)memcpy_s(&qp_handle->qp_info, sizeof(struct qp_create_info), qp_info,
        sizeof(struct qp_create_info));
}

void ra_ctx_prepare_qp_import(struct qp_import_info_t *qp_info, struct ra_rs_jetty_import_attr *import_attr)
{
    import_attr->mode = qp_info->in.ub.mode;
    import_attr->token_value = qp_info->in.ub.token_value;
    import_attr->policy = qp_info->in.ub.policy;
    import_attr->type = qp_info->in.ub.type;
    import_attr->flag = qp_info->in.ub.flag;
    import_attr->exp_import_cfg = qp_info->in.ub.exp_import_cfg;
    import_attr->tp_type = qp_info->in.ub.tp_type;
}

void ra_ctx_get_qp_import_info(struct ra_ctx_handle *ctx_handle, struct qp_import_info_t *qp_info,
    struct ra_rs_jetty_import_info *import_info, struct ra_ctx_rem_qp_handle *qp_handle)
{
    qp_info->out.ub.tjetty_handle = import_info->tjetty_handle;
    qp_info->out.ub.tpn = import_info->tpn;
    qp_handle->dev_index = ctx_handle->dev_index;
    qp_handle->phy_id = ctx_handle->attr.phy_id;
    qp_handle->protocol = ctx_handle->protocol;
    qp_handle->qp_key = qp_info->in.key;
}

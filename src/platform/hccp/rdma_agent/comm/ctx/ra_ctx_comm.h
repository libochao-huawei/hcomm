/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef RA_CTX_COMM_H
#define RA_CTX_COMM_H

#include "ra.h"
#include "hccp_common.h"
#include "hccp_ctx.h"
#include "ra_ctx.h"
#include "ra_rs_comm.h"
#include "ra_rs_ctx.h"

int ra_ctx_prepare_lmem_register(struct mr_reg_info_t *lmem_info, struct mem_reg_attr_t *mem_attr);

void ra_ctx_get_lmem_info(struct mem_reg_info_t *mem_info, struct mr_reg_info_t *lmem_info,
    struct ra_lmem_handle *lmem_handle);

void ra_ctx_prepare_rmem_import(struct mr_import_info_t *rmem_info, struct mem_import_attr_t *mem_attr);

void ra_ctx_prepare_cq_create(struct cq_info_t *info, struct ctx_cq_attr *cq_attr);

void ra_ctx_get_cq_create_info(struct ctx_cq_info *cq_info, struct cq_info_t *info);

int ra_ctx_prepare_qp_create(struct qp_create_attr *qp_attr, struct ctx_qp_attr *ctx_qp_attr);

void ra_ctx_get_qp_create_info(struct ra_ctx_handle *ctx_handle, struct qp_create_attr *qp_attr,
    struct qp_create_info *qp_info, struct ra_ctx_qp_handle *qp_handle);

void ra_ctx_prepare_qp_import(struct qp_import_info_t *qp_info, struct ra_rs_jetty_import_attr *import_attr);

void ra_ctx_get_qp_import_info(struct ra_ctx_handle *ctx_handle, struct qp_import_info_t *qp_info,
    struct ra_rs_jetty_import_info *import_info, struct ra_ctx_rem_qp_handle *qp_handle);

#endif // RA_CTX_COMM_H

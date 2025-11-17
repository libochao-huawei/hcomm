/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef RA_ASYNC_CTX_H
#define RA_ASYNC_CTX_H

#include "hccp_ctx.h"
#include "ra_ctx.h"
#include "ra_async.h"

union op_get_tp_info_list_data {
    struct {
        unsigned int phy_id;
        unsigned int dev_index;
        struct get_tp_cfg cfg;
        unsigned int num;
        uint32_t resv[4U];
    } tx_data;

    struct {
        struct tp_info info_list[HCCP_MAX_TPID_INFO_NUM];
        unsigned int num;
        uint32_t resv[4U];
    } rx_data;
};

struct ra_response_tp_info_list {
    struct tp_info *info_list;
    unsigned int *num;
};

int ra_hdc_ctx_lmem_register_async(struct ra_ctx_handle *ctx_handle, struct mr_reg_info_t *lmem_info,
    struct ra_lmem_handle *lmem_handle, void **req_handle);
void ra_hdc_async_handle_lmem_register(struct ra_request_handle *req_handle);
int ra_hdc_ctx_lmem_unregister_async(struct ra_ctx_handle *ctx_handle, struct ra_lmem_handle *lmem_handle,
    void **req_handle);
int ra_hdc_ctx_qp_create_async(struct ra_ctx_handle *ctx_handle, struct qp_create_attr *attr,
	struct qp_create_info *info, struct ra_ctx_qp_handle *qp_handle, void **req_handle);
void ra_hdc_async_handle_qp_create(struct ra_request_handle *req_handle);
int ra_hdc_ctx_qp_destroy_async(struct ra_ctx_qp_handle *qp_handle, void **req_handle);
int ra_hdc_ctx_qp_import_async(struct ra_ctx_handle *ctx_handle, struct qp_import_info_t *info,
    struct ra_ctx_rem_qp_handle *rem_qp_handle, void **req_handle);
void ra_hdc_async_handle_qp_import(struct ra_request_handle *req_handle);
int ra_hdc_ctx_qp_unimport_async(struct ra_ctx_rem_qp_handle *rem_qp_handle, void **req_handle);
int ra_hdc_get_tp_info_list_async(struct ra_ctx_handle *ctx_handle, struct get_tp_cfg *cfg, struct tp_info info_list[],
    unsigned int *num, void **req_handle);
void ra_hdc_async_handle_tp_info_list(struct ra_request_handle *req_handle);
#endif // RA_ASYNC_CTX_H

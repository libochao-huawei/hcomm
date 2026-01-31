/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "hccp_ctx.h"
#include "hccp_async.h"
#include "hccp_async_ctx.h"

int ra_ctx_qp_create(void *ctx_handle, struct qp_create_attr *attr, struct qp_create_info *info,
    void **qp_handle)
{
    return 0;
}

int ra_ctx_qp_destroy(void *qp_handle)
{
    return 0;
}

int ra_ctx_qp_import(void *ctx_handle, struct qp_import_info_t *qp_info, void **rem_qp_handle)
{
    return 0;
}

int ra_ctx_qp_unimport(void *ctx_handle, void *rem_qp_handle)
{
    return 0;
}

int RaGetAsyncReqResult(void *reqHandle, int *reqResult)
{
    *reqResult = 0;
    return 0;
}

int ra_ctx_qp_create_async(void *ctx_handle, struct qp_create_attr *attr,
    struct qp_create_info *info, void **qp_handle, void **req_handle)
{
    int a = 12378;
    *req_handle = &a;
    return 0;
}

int ra_ctx_qp_import_async(void *ctx_handle, struct qp_import_info_t *info, void **rem_qp_handle,
    void **req_handle)
{
    int a = 12378;
    *req_handle = &a;
    return 0;
}

int ra_get_tp_info_list_async(void *ctx_handle, struct get_tp_cfg *cfg, struct tp_info info_list[],
    unsigned int *num, void **req_handle)
{
    int a = 12378;
    *req_handle = &a;
    return 0;
}

int ra_custom_channel(struct RaInfo info, struct custom_chan_info_in *in,
    struct custom_chan_info_out *out)
{
    return 0;
}

int ra_get_dev_eid_info_num(struct RaInfo info, unsigned int *num)
{
    *num = 2;
    return 0;
}

int ra_get_dev_eid_info_list(struct RaInfo info, struct dev_eid_info info_list[],
    unsigned int *num)
{
    if (info.phyId == 0) {
        info_list[0].eid.in4.addr = 167772383;
    } else {
        info_list[0].eid.in4.addr = 469762271;
    }
    
    info_list[0].die_id = 0;
    info_list[0].chip_id = 0;
    info_list[0].func_id = 2;

    info_list[1].eid.in4.addr = 12346;
    info_list[1].die_id = 1;
    info_list[1].chip_id = 0;
    info_list[1].func_id = 3;

    return 0;
}
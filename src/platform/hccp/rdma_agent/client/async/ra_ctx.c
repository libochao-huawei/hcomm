/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <stdlib.h>
#include "securec.h"
#include "user_log.h"
#include "hccp_ctx.h"
#include "hccp_async.h"
#include "ra_async.h"
#include "ra_hdc_async_ctx.h"
#include "ra_hdc_async.h"
#include "ra_ctx.h"

HCCP_ATTRI_VISI_DEF int ra_get_eid_by_ip_async(void *ctx_handle, struct IpInfo ip[], union hccp_eid eid[],
    unsigned int *num, void **req_handle)
{
    struct ra_ctx_handle *ctx_handle_tmp = NULL;
    int ret = 0;

    CHK_PRT_RETURN(ctx_handle == NULL || ip == NULL || eid == NULL || num == NULL || req_handle == NULL,
        hccp_err("[get][eid_by_ip]ctx_handle or ip or eid or num or req_handle is NULL"),
        ConverReturnCode(RDMA_OP, -EINVAL));

    CHK_PRT_RETURN(*num == 0 || *num > GET_EID_BY_IP_MAX_NUM, hccp_err("[get][eid_by_ip]num(%u) must greater than 0"
        " and less or equal to %d", *num, GET_EID_BY_IP_MAX_NUM), ConverReturnCode(RDMA_OP, -EINVAL));

    ctx_handle_tmp = (struct ra_ctx_handle *)ctx_handle;

    hccp_run_info("Input parameters: phy_id(%u), dev_index(0x%x)",
        ctx_handle_tmp->attr.phy_id, ctx_handle_tmp->dev_index);
    ret = ra_hdc_get_eid_by_ip_async(ctx_handle_tmp, ip, eid, num, req_handle);
    if (ret != 0) {
        hccp_err("[get][eid_by_ip]ra_hdc_get_eid_by_ip_async failed, ret(%d) phy_id(%u), dev_index(0x%x)", ret,
            ctx_handle_tmp->attr.phy_id, ctx_handle_tmp->dev_index);
    }

    return ret;
}

HCCP_ATTRI_VISI_DEF int ra_ctx_lmem_register_async(void *ctx_handle, struct mr_reg_info_t *lmem_info,
    void **lmem_handle, void **req_handle)
{
    struct ra_lmem_handle *lmem_handle_tmp = NULL;
    struct ra_ctx_handle *ctx_handle_tmp = NULL;
    int ret = 0;

    CHK_PRT_RETURN(ctx_handle == NULL || lmem_info == NULL || lmem_handle == NULL || req_handle == NULL,
        hccp_err("[init][ra_lmem]ctx_handle or lmem_info or lmem_handle or req_handle is NULL"),
        ConverReturnCode(RDMA_OP, -EINVAL));

    ctx_handle_tmp = (struct ra_ctx_handle *)ctx_handle;
    lmem_handle_tmp = calloc(1, sizeof(struct ra_lmem_handle));
    CHK_PRT_RETURN(lmem_handle_tmp == NULL,
        hccp_err("[init][ra_lmem]calloc lmem_handle_tmp failed, errno(%d) phy_id(%u) dev_index(%u)",
        errno, ctx_handle_tmp->attr.phy_id, ctx_handle_tmp->dev_index), ConverReturnCode(RDMA_OP, -ENOMEM));

    ret = ra_hdc_ctx_lmem_register_async(ctx_handle_tmp, lmem_info, lmem_handle_tmp, req_handle);
    if (ret != 0) {
        hccp_err("[init][ra_lmem]register_async failed, ret(%d) phy_id(%u), dev_index(%u)", ret,
            ctx_handle_tmp->attr.phy_id, ctx_handle_tmp->dev_index);
        goto err;
    }

    *lmem_handle = (void *)lmem_handle_tmp;
    return 0;

err:
    free(lmem_handle_tmp);
    lmem_handle_tmp = NULL;
    return ConverReturnCode(RDMA_OP, ret);
}

HCCP_ATTRI_VISI_DEF int ra_ctx_lmem_unregister_async(void *ctx_handle, void *lmem_handle, void **req_handle)
{
    struct ra_lmem_handle *lmem_handle_tmp = NULL;
    struct ra_ctx_handle *ctx_handle_tmp = NULL;
    int ret;

    CHK_PRT_RETURN(ctx_handle == NULL || lmem_handle == NULL || req_handle == NULL,
        hccp_err("[deinit][ra_lmem]ctx_handle or lmem_handle or req_handle is NULL"),
        ConverReturnCode(RDMA_OP, -EINVAL));

    ctx_handle_tmp = (struct ra_ctx_handle *)ctx_handle;
    lmem_handle_tmp = (struct ra_lmem_handle *)lmem_handle;
    ret = ra_hdc_ctx_lmem_unregister_async(ctx_handle_tmp, lmem_handle_tmp, req_handle);
    if (ret != 0) {
        hccp_err("[deinit][ra_lmem]unregister_async failed, ret(%d) phy_id(%u), dev_index(%u)", ret,
            ctx_handle_tmp->attr.phy_id, ctx_handle_tmp->dev_index);
    }

    free(lmem_handle_tmp);
    lmem_handle_tmp = NULL;
    return ConverReturnCode(RDMA_OP, ret);
}

HCCP_ATTRI_VISI_DEF int ra_ctx_qp_create_async(void *ctx_handle, struct qp_create_attr *attr,
    struct qp_create_info *info, void **qp_handle, void **req_handle)
{
    struct ra_ctx_qp_handle *qp_handle_tmp = NULL;
    struct ra_ctx_handle *ctx_handle_tmp = NULL;
    int ret;

    CHK_PRT_RETURN(ctx_handle == NULL || attr == NULL || info == NULL || qp_handle == NULL || req_handle == NULL,
        hccp_err("[init][ra_qp]ctx_handle or attr or info or qp_handle or req_handle is NULL"),
        ConverReturnCode(RDMA_OP, -EINVAL));

    ctx_handle_tmp = (struct ra_ctx_handle *)ctx_handle;
    qp_handle_tmp = calloc(1, sizeof(struct ra_ctx_qp_handle));
    CHK_PRT_RETURN(qp_handle_tmp == NULL,
        hccp_err("[init][ra_qp]calloc qp_handle_tmp failed, errno(%d) phy_id(%u) dev_index(%u)",
        errno, ctx_handle_tmp->attr.phy_id, ctx_handle_tmp->dev_index), ConverReturnCode(RDMA_OP, -ENOMEM));

    ret = ra_hdc_ctx_qp_create_async(ctx_handle_tmp, attr, info, qp_handle_tmp, req_handle);
    if (ret != 0) {
        hccp_err("[init][ra_qp]create_async failed, ret(%d) phy_id(%u), dev_index(%u)", ret,
            ctx_handle_tmp->attr.phy_id, ctx_handle_tmp->dev_index);
        goto err;
    }

    *qp_handle = (void *)qp_handle_tmp;
    return 0;

err:
    free(qp_handle_tmp);
    qp_handle_tmp = NULL;
    return ConverReturnCode(RDMA_OP, ret);
}

HCCP_ATTRI_VISI_DEF int ra_ctx_qp_destroy_async(void *qp_handle, void **req_handle)
{
    struct ra_ctx_qp_handle *qp_handle_tmp = NULL;
    int ret;

    CHK_PRT_RETURN(qp_handle == NULL || req_handle == NULL, hccp_err("[deinit][ra_qp]qp_handle or req_handle is NULL"),
        ConverReturnCode(RDMA_OP, -EINVAL));

    qp_handle_tmp = (struct ra_ctx_qp_handle *)qp_handle;
    ret = ra_hdc_ctx_qp_destroy_async(qp_handle_tmp, req_handle);
    if (ret != 0) {
        hccp_err("[deinit][ra_qp]destroy_async failed, ret(%d) phy_id(%u), dev_index(%u) qp_id(%u)",
            ret, qp_handle_tmp->phy_id, qp_handle_tmp->dev_index, qp_handle_tmp->id);
    }

    free(qp_handle_tmp);
    qp_handle_tmp = NULL;
    return ConverReturnCode(RDMA_OP, ret);
}

STATIC void ra_free_qp_handle_batch(void *qp_handle[], unsigned int num)
{
    unsigned int i;

    for (i = 0; i < num; ++i) {
        if (qp_handle[i] != NULL) {
            free(qp_handle[i]);
            qp_handle[i] = NULL;
        }
    }
}

HCCP_ATTRI_VISI_DEF int ra_ctx_qp_destroy_batch_async(void *ctx_handle, void *qp_handle[],
    unsigned int *num, void **req_handle)
{
    struct ra_ctx_handle *ctx_handle_tmp = NULL;
    int ret;

    CHK_PRT_RETURN(ctx_handle == NULL || qp_handle == NULL || req_handle == NULL || num == NULL,
        hccp_err("[destroy_batch][ra_qp]ctx_handle or qp_handle or req_handle or num is NULL"),
        ConverReturnCode(RDMA_OP, -EINVAL));
    CHK_PRT_RETURN(*num == 0 || *num > HCCP_MAX_QP_DESTROY_BATCH_NUM,
        hccp_err("[destroy_batch][ra_qp]num(%u) is out of range(0, %u]", *num, HCCP_MAX_QP_DESTROY_BATCH_NUM),
        ConverReturnCode(RDMA_OP, -EINVAL));

    ctx_handle_tmp = (struct ra_ctx_handle *)ctx_handle;
    ret = ra_hdc_ctx_qp_destroy_batch_async(ctx_handle_tmp, qp_handle, num, req_handle);
    if (ret != 0) {
        hccp_err("[destroy_batch][ra_qp]qp_destroy_batch_async failed, ret[%d] phy_id[%u] num[%u] dev_index[%u]",
            ret, ctx_handle_tmp->attr.phy_id, *num, ctx_handle_tmp->dev_index);
    }

    ra_free_qp_handle_batch(qp_handle, *num);
    return ConverReturnCode(RDMA_OP, ret);
}

HCCP_ATTRI_VISI_DEF int ra_ctx_qp_import_async(void *ctx_handle, struct qp_import_info_t *info, void **rem_qp_handle,
    void **req_handle)
{
    struct ra_ctx_rem_qp_handle *rem_qp_handle_tmp = NULL;
    struct ra_ctx_handle *ctx_handle_tmp = NULL;
    int ret;

    CHK_PRT_RETURN(ctx_handle == NULL || info == NULL || rem_qp_handle == NULL || req_handle == NULL,
        hccp_err("[init][ra_qp]ctx_handle or info or rem_qp_handle or req_handle is NULL"),
        ConverReturnCode(RDMA_OP, -EINVAL));

    ctx_handle_tmp = (struct ra_ctx_handle *)ctx_handle;
    rem_qp_handle_tmp = calloc(1, sizeof(struct ra_ctx_rem_qp_handle));
    CHK_PRT_RETURN(rem_qp_handle_tmp == NULL,
        hccp_err("[init][ra_qp]calloc rem_qp_handle_tmp failed, errno(%d) phy_id(%u) dev_index(%u)",
        errno, ctx_handle_tmp->attr.phy_id, ctx_handle_tmp->dev_index), ConverReturnCode(RDMA_OP, -ENOMEM));

    ret = ra_hdc_ctx_qp_import_async(ctx_handle_tmp, info, rem_qp_handle_tmp, req_handle);
    if (ret != 0) {
        hccp_err("[init][ra_qp]import_async failed, ret(%d) phy_id(%u), dev_index(%u)",
            ret, ctx_handle_tmp->attr.phy_id, ctx_handle_tmp->dev_index);
        goto err;
    }

    *rem_qp_handle = (void *)rem_qp_handle_tmp;
    return 0;

err:
    free(rem_qp_handle_tmp);
    rem_qp_handle_tmp = NULL;
    return ConverReturnCode(RDMA_OP, ret);
}

HCCP_ATTRI_VISI_DEF int ra_ctx_qp_unimport_async(void *rem_qp_handle, void **req_handle)
{
    struct ra_ctx_rem_qp_handle *rem_qp_handle_tmp = NULL;
    int ret;

    CHK_PRT_RETURN(rem_qp_handle == NULL || req_handle == NULL,
        hccp_err("[deinit][ra_qp]rem_qp_handle or req_handle is NULL"), ConverReturnCode(RDMA_OP, -EINVAL));

    rem_qp_handle_tmp = (struct ra_ctx_rem_qp_handle *)rem_qp_handle;
    ret = ra_hdc_ctx_qp_unimport_async(rem_qp_handle_tmp, req_handle);
    if (ret != 0) {
        hccp_err("[deinit][ra_qp]unimport_async failed, ret(%d) phy_id(%u) dev_index(%u) qp_id(%u)",
            ret, rem_qp_handle_tmp->phy_id, rem_qp_handle_tmp->dev_index, rem_qp_handle_tmp->id);
    }

    free(rem_qp_handle_tmp);
    rem_qp_handle_tmp = NULL;
    return ConverReturnCode(RDMA_OP, ret);
}

HCCP_ATTRI_VISI_DEF int ra_get_tp_info_list_async(void *ctx_handle, struct get_tp_cfg *cfg, struct tp_info info_list[],
    unsigned int *num, void **req_handle)
{
    struct ra_ctx_handle *ctx_handle_tmp = NULL;
    int ret;

    CHK_PRT_RETURN(ctx_handle == NULL || cfg == NULL || req_handle == NULL,
        hccp_err("[get][ra_tp_info]ctx_handle or cfg or req_handle is NULL"), ConverReturnCode(RDMA_OP, -EINVAL));
    CHK_PRT_RETURN(info_list == NULL || num == NULL,
        hccp_err("[get][ra_tp_info]info_list or num is NULL"), ConverReturnCode(RDMA_OP, -EINVAL));
    CHK_PRT_RETURN(*num == 0 || *num > HCCP_MAX_TPID_INFO_NUM,
        hccp_err("[get][ra_tp_info]*num(%u) is out of range[0, %d]", *num, HCCP_MAX_TPID_INFO_NUM),
        ConverReturnCode(RDMA_OP, -EINVAL));

    ctx_handle_tmp = (struct ra_ctx_handle *)ctx_handle;
    ret = ra_hdc_get_tp_info_list_async(ctx_handle_tmp, cfg, info_list, num, req_handle);
    CHK_PRT_RETURN(ret != 0, hccp_err("[get][ra_tp_info]get_tp_info_list_async failed, ret[%d] phy_id[%u], dev_index"
        "[%u]", ret, ctx_handle_tmp->attr.phy_id, ctx_handle_tmp->dev_index), ConverReturnCode(RDMA_OP, ret));

    return ConverReturnCode(RDMA_OP, ret);
}

HCCP_ATTRI_VISI_DEF int ra_get_tp_attr_async(void *ctx_handle, uint64_t tp_handle, uint32_t *attr_bitmap,
    struct tp_attr *attr, void **req_handle)
{
    struct ra_ctx_handle *ctx_handle_tmp = NULL;
    int ret;

    CHK_PRT_RETURN(ctx_handle == NULL || attr_bitmap == NULL || req_handle == NULL || attr == NULL,
        hccp_err("[get][ra_tp_attr]ctx_handle or attr_bitmap or req_handle or attr is NULL"),
        ConverReturnCode(RDMA_OP, -EINVAL));

    ctx_handle_tmp = (struct ra_ctx_handle *)ctx_handle;
    ret = ra_hdc_get_tp_attr_async(ctx_handle_tmp, tp_handle, attr_bitmap, attr, req_handle);
    CHK_PRT_RETURN(ret != 0, hccp_err("[get][ra_tp_attr]get_tp_attr_async failed, ret[%d] phy_id[%u] dev_index[0x%x]",
        ret, ctx_handle_tmp->attr.phy_id, ctx_handle_tmp->dev_index), ConverReturnCode(RDMA_OP, ret));

    return ConverReturnCode(RDMA_OP, ret);
}

HCCP_ATTRI_VISI_DEF int ra_set_tp_attr_async(void *ctx_handle, uint64_t tp_handle, uint32_t attr_bitmap,
    struct tp_attr *attr, void **req_handle)
{
    struct ra_ctx_handle *ctx_handle_tmp = NULL;
    int ret;

    CHK_PRT_RETURN(ctx_handle == NULL || attr == NULL || req_handle == NULL,
        hccp_err("[get][ra_tp_attr]ctx_handle or attr or req_handle or attr is NULL"),
        ConverReturnCode(RDMA_OP, -EINVAL));

    ctx_handle_tmp = (struct ra_ctx_handle *)ctx_handle;
    ret = ra_hdc_set_tp_attr_async(ctx_handle_tmp, tp_handle, attr_bitmap, attr, req_handle);
    CHK_PRT_RETURN(ret != 0, hccp_err("[set][ra_tp_attr]set_tp_attr_async failed, ret[%d] phy_id[%u] dev_index[0x%x]",
        ret, ctx_handle_tmp->attr.phy_id, ctx_handle_tmp->dev_index), ConverReturnCode(RDMA_OP, ret));

    return ConverReturnCode(RDMA_OP, ret);
}

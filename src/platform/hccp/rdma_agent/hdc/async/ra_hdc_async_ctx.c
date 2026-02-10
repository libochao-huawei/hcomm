/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "securec.h"
#include "user_log.h"
#include "ra_async.h"
#include "ra_rs_comm.h"
#include "ra_rs_err.h"
#include "ra_rs_ctx.h"
#include "ra_hdc_ctx.h"
#include "ra_hdc_async.h"
#include "ra_hdc_async_ctx.h"

int ra_hdc_get_eid_by_ip_async(struct ra_ctx_handle *ctx_handle, struct IpInfo ip[], union hccp_eid eid[],
    unsigned int *num, void **req_handle)
{
    struct RaRequestHandle *req_handle_tmp = NULL;
    struct ra_response_eid_list *async_rsp = NULL;
    unsigned int phy_id = ctx_handle->attr.phy_id;
    union op_get_eid_by_ip_data async_data = {0};
    int ret = 0;

    async_rsp = (struct ra_response_eid_list *)calloc(1, sizeof(struct ra_response_eid_list));
    CHK_PRT_RETURN(async_rsp == NULL,
        hccp_err("[get][eid_by_ip]calloc async_rsp failed, phy_id[%u] dev_index[0x%x]",
        phy_id, ctx_handle->dev_index), -ENOMEM);
    async_rsp->eid_list = eid;
    async_rsp->num = num;

    req_handle_tmp = (struct RaRequestHandle *)calloc(1, sizeof(struct RaRequestHandle));
    if (req_handle_tmp == NULL) {
        hccp_err("[get][eid_by_ip]calloc req_handle_tmp failed, phy_id[%u], dev_index[0x%x]",
            phy_id, ctx_handle->dev_index);
        ret = -ENOMEM;
        goto out;
    }

    ra_hdc_prepare_get_eid_by_ip(ctx_handle, ip, *num, &async_data);
    req_handle_tmp->devIndex = ctx_handle->dev_index;
    req_handle_tmp->privData = (void *)async_rsp;
    ret = RaHdcSendMsgAsync(RA_RS_GET_EID_BY_IP, phy_id, (char *)&async_data, sizeof(union op_get_eid_by_ip_data),
        req_handle_tmp);
    if (ret != 0) {
        hccp_err("[get][eid_by_ip]hdc async send message failed ret[%d], phy_id[%u], dev_index[0x%x]",
            ret, phy_id, ctx_handle->dev_index);
        free(req_handle_tmp);
        req_handle_tmp = NULL;
        goto out;
    }

    *req_handle = (void *)req_handle_tmp;
    return ret;

out:
    free(async_rsp);
    async_rsp = NULL;
    return ret;
}

void ra_hdc_async_handle_get_eid_by_ip(struct RaRequestHandle *req_handle)
{
    union op_get_eid_by_ip_data *async_data = NULL;
    struct ra_response_eid_list *async_rsp = NULL;
    unsigned int ip_num = 0;
    int ret = 0;

    async_data = (union op_get_eid_by_ip_data *)req_handle->recvBuf;
    async_rsp = (struct ra_response_eid_list *)req_handle->privData;
    ip_num = *async_rsp->num;

    ret = ra_hdc_get_eid_results(async_data, ip_num, async_rsp->eid_list, async_rsp->num);
    if (ret != 0) {
        hccp_err("[get][eid_by_ip]ra_hdc_get_eid_results failed ret[%d], phy_id[%u] dev_index[0x%x]", ret,
            req_handle->phyId, req_handle->devIndex);
        req_handle->opRet = ret;
    }

    free(req_handle->privData);
    req_handle->privData = NULL;
    return;
}

int ra_hdc_ctx_lmem_register_async(struct ra_ctx_handle *ctx_handle, struct mr_reg_info_t *lmem_info, 
    struct ra_lmem_handle *lmem_handle, void **req_handle)
{
    struct RaRequestHandle *req_handle_tmp = NULL;
    unsigned int phy_id = ctx_handle->attr.phy_id;
    union op_lmem_reg_info_data async_data = {0};
    int ret;

    ret = ra_hdc_ctx_prepare_lmem_register(ctx_handle, lmem_info, &async_data);
    CHK_PRT_RETURN(ret != 0, hccp_err("[init][ra_hdc_lmem]prepare register failed ret[%d], phy_id[%u] dev_index[%u]",
        ret, phy_id, ctx_handle->dev_index), ret);

    req_handle_tmp = (struct RaRequestHandle *)calloc(1, sizeof(struct RaRequestHandle));
    CHK_PRT_RETURN(req_handle_tmp == NULL,
        hccp_err("[init][ra_hdc_lmem]calloc req_handle_tmp failed, phy_id[%u], dev_index[0x%x]",
        phy_id, ctx_handle->dev_index), -ENOMEM);

    req_handle_tmp->devIndex = ctx_handle->dev_index;
    req_handle_tmp->privData = (void *)&lmem_info->out;
    req_handle_tmp->privHandle = (void *)lmem_handle;
    ret = RaHdcSendMsgAsync(RA_RS_LMEM_REG, phy_id, (char *)&async_data, sizeof(union op_lmem_reg_info_data),
        req_handle_tmp);
    if (ret != 0) {
        hccp_err("[init][ra_hdc_lmem]hdc async send message failed ret[%d], phy_id[%u], dev_index[0x%x]",
            ret, phy_id, ctx_handle->dev_index);
        free(req_handle_tmp);
        req_handle_tmp = NULL;
        return ret;
    }

    *req_handle = (void *)req_handle_tmp;
    return 0;
}

void ra_hdc_async_handle_lmem_register(struct RaRequestHandle *req_handle)
{
    union op_lmem_reg_info_data *async_data = NULL;
    struct ra_lmem_handle *lmem_handle = NULL;
    struct mem_reg_info *info = NULL;
    int ret;

    async_data = (union op_lmem_reg_info_data *)req_handle->recvBuf;
    lmem_handle = (struct ra_lmem_handle *)req_handle->privHandle;
    info = (struct mem_reg_info *)req_handle->privData;
    ret = memcpy_s(info, sizeof(struct mem_reg_info), &async_data->rx_data.mem_info, sizeof(struct mem_reg_info_t));
    if (ret != 0) {
        hccp_err("[init][ra_hdc_lmem]memcpy_s mem_info failed ret[%d], phy_id[%u] dev_index[0x%x]",
            ret, req_handle->phyId, req_handle->devIndex);
        req_handle->opRet = -ESAFEFUNC;
        return;
    }

    lmem_handle->addr = info->ub.target_seg_handle;
    return;
}

int ra_hdc_ctx_lmem_unregister_async(struct ra_ctx_handle *ctx_handle, struct ra_lmem_handle *lmem_handle,
    void **req_handle)
{
    struct RaRequestHandle *req_handle_tmp = NULL;
    union op_lmem_unreg_info_data async_data = {0};
    unsigned int phy_id = ctx_handle->attr.phy_id;
    int ret = 0;

    async_data.tx_data.phy_id = phy_id;
    async_data.tx_data.dev_index = ctx_handle->dev_index;
    async_data.tx_data.addr = lmem_handle->addr;
    req_handle_tmp = (struct RaRequestHandle *)calloc(1, sizeof(struct RaRequestHandle));
    CHK_PRT_RETURN(req_handle_tmp == NULL,
        hccp_err("[deinit][ra_hdc_lmem]calloc req_handle_tmp failed, phy_id[%u], dev_index[0x%x]",
        phy_id, ctx_handle->dev_index), -ENOMEM);

    ret = RaHdcSendMsgAsync(RA_RS_LMEM_UNREG, phy_id, (char *)&async_data, sizeof(union op_lmem_unreg_info_data),
        req_handle_tmp);
    if (ret != 0) {
        hccp_err("[deinit][ra_hdc_lmem]hdc async send message failed ret[%d], phy_id[%u] dev_index[0x%x]",
            ret, phy_id, ctx_handle->dev_index);
        free(req_handle_tmp);
        req_handle_tmp = NULL;
        return ret;
    }

    *req_handle = (void *)req_handle_tmp;
    return 0;
}

int ra_hdc_ctx_qp_create_async(struct ra_ctx_handle *ctx_handle, struct qp_create_attr *attr,
	struct qp_create_info *info, struct ra_ctx_qp_handle *qp_handle, void **req_handle)
{
    struct RaRequestHandle *req_handle_tmp = NULL;
    unsigned int phy_id = ctx_handle->attr.phy_id;
    union op_ctx_qp_create_data async_data = {0};
    int ret;

    ret = ra_hdc_ctx_prepare_qp_create(ctx_handle, attr, &async_data);
    CHK_PRT_RETURN(ret != 0, hccp_err("[init][ra_hdc_qp]prepare qp_create failed ret[%d], phy_id[%u] dev_index[0x%x]",
        ret, phy_id, ctx_handle->dev_index), ret);

    req_handle_tmp = (struct RaRequestHandle *)calloc(1, sizeof(struct RaRequestHandle));
    CHK_PRT_RETURN(req_handle_tmp == NULL,
        hccp_err("[init][ra_hdc_qp]calloc req_handle_tmp failed, phy_id[%u], dev_index[0x%x]",
        phy_id, ctx_handle->dev_index), -ENOMEM);
    req_handle_tmp->devIndex = ctx_handle->dev_index;
    req_handle_tmp->privData = (void *)info;
    qp_handle->ctx_handle = ctx_handle;
    qp_handle->dev_index = ctx_handle->dev_index;
    qp_handle->phy_id = ctx_handle->attr.phy_id;
    qp_handle->protocol = ctx_handle->protocol;
    (void)memcpy_s(&qp_handle->qp_attr, sizeof(struct qp_create_attr), attr, sizeof(struct qp_create_attr));
    req_handle_tmp->privHandle = (void *)qp_handle;

    ret = RaHdcSendMsgAsync(RA_RS_CTX_QP_CREATE, phy_id, (char *)&async_data,
        sizeof(union op_ctx_qp_create_data), req_handle_tmp);
    if (ret != 0) {
        hccp_err("[init][ra_hdc_qp]hdc async send message failed ret[%d], phy_id[%u] dev_index[0x%x]",
            ret, phy_id, ctx_handle->dev_index);
        free(req_handle_tmp);
        req_handle_tmp = NULL;
        return ret;
    }

    *req_handle = (void *)req_handle_tmp;
    return 0;
}

void ra_hdc_async_handle_qp_create(struct RaRequestHandle *req_handle)
{
    union op_ctx_qp_create_data *async_data = NULL;
    struct ra_ctx_qp_handle *qp_handle = NULL;
    struct qp_create_info *info = NULL;

    async_data = (union op_ctx_qp_create_data *)req_handle->recvBuf;
    info = (struct qp_create_info *)req_handle->privData;
    qp_handle = (struct ra_ctx_qp_handle *)req_handle->privHandle;
    (void)memcpy_s(info, sizeof(struct qp_create_info), &async_data->rx_data.qp_info, sizeof(struct qp_create_info));
    qp_handle->id = info->ub.id;
    (void)memcpy_s(&qp_handle->qp_info, sizeof(struct qp_create_info), info, sizeof(struct qp_create_info));

    return;
}

int ra_hdc_ctx_qp_destroy_async(struct ra_ctx_qp_handle *qp_handle, void **req_handle)
{
    struct RaRequestHandle *req_handle_tmp = NULL;
    union op_ctx_qp_destroy_data async_data = {0};
    unsigned int phy_id = qp_handle->phy_id;
    int ret;

    async_data.tx_data.phy_id = phy_id;
    async_data.tx_data.dev_index = qp_handle->dev_index;
    async_data.tx_data.id = qp_handle->id;

    req_handle_tmp = (struct RaRequestHandle *)calloc(1, sizeof(struct RaRequestHandle));
    CHK_PRT_RETURN(req_handle_tmp == NULL,
        hccp_err("[deinit][ra_hdc_qp]calloc req_handle_tmp failed, phy_id[%u], dev_index[0x%x]",
        phy_id, qp_handle->dev_index), -ENOMEM);

    ret = RaHdcSendMsgAsync(RA_RS_CTX_QP_DESTROY, phy_id, (char *)&async_data,
        sizeof(union op_ctx_qp_destroy_data), req_handle_tmp);
    if (ret != 0) {
        hccp_err("[deinit][ra_hdc_qp]hdc async send message failed ret[%d], phy_id[%u] dev_index[0x%x]",
            ret, phy_id, qp_handle->dev_index);
        free(req_handle_tmp);
        req_handle_tmp = NULL;
        return ret;
    }

    *req_handle = (void *)req_handle_tmp;
    return 0;
}

int ra_hdc_ctx_qp_import_async(struct ra_ctx_handle *ctx_handle, struct qp_import_info_t *info,
    struct ra_ctx_rem_qp_handle *rem_qp_handle, void **req_handle)
{
    struct RaRequestHandle *req_handle_tmp = NULL;
    unsigned int phy_id = ctx_handle->attr.phy_id;
    union op_ctx_qp_import_data async_data = {0};
    int ret = 0;

    ret = ra_hdc_ctx_prepare_qp_import(ctx_handle, info, &async_data);
    CHK_PRT_RETURN(ret != 0, hccp_err("[init][ra_hdc_qp]prepare qp_import failed ret[%d], phy_id[%u] dev_index[0x%x]",
        ret, phy_id, ctx_handle->dev_index), ret);

    req_handle_tmp = (struct RaRequestHandle *)calloc(1, sizeof(struct RaRequestHandle));
    CHK_PRT_RETURN(req_handle_tmp == NULL,
        hccp_err("[init][ra_hdc_qp]calloc req_handle_tmp failed, phy_id[%u], dev_index[0x%x]",
        phy_id, ctx_handle->dev_index), -ENOMEM);

    req_handle_tmp->devIndex = ctx_handle->dev_index;
    req_handle_tmp->privData = (void *)&info->out;
    rem_qp_handle->dev_index = ctx_handle->dev_index;
    rem_qp_handle->phy_id = ctx_handle->attr.phy_id;
    rem_qp_handle->protocol = ctx_handle->protocol;
    req_handle_tmp->privHandle = (void *)rem_qp_handle;

    ret = RaHdcSendMsgAsync(RA_RS_CTX_QP_IMPORT, phy_id, (char *)&async_data,
        sizeof(union op_ctx_qp_import_data), req_handle_tmp);
    if (ret != 0) {
        hccp_err("[init][ra_hdc_qp]hdc async send message failed ret[%d], phy_id[%u] dev_index[0x%x]",
            ret, phy_id, ctx_handle->dev_index);
        free(req_handle_tmp);
        req_handle_tmp = NULL;
        return ret;
    }

    *req_handle = (void *)req_handle_tmp;
    return 0;
}

void ra_hdc_async_handle_qp_import(struct RaRequestHandle *req_handle)
{
    struct ra_ctx_rem_qp_handle *rem_qp_handle = NULL;
    union op_ctx_qp_import_data *async_data = NULL;
    struct qp_import_info *info = NULL;

    async_data = (union op_ctx_qp_import_data *)req_handle->recvBuf;
    info = (struct qp_import_info *)req_handle->privData;
    rem_qp_handle = (struct ra_ctx_rem_qp_handle *)req_handle->privHandle;

    info->ub.tjetty_handle = async_data->rx_data.info.tjetty_handle;
    info->ub.tpn = async_data->rx_data.info.tpn;
    rem_qp_handle->id = async_data->rx_data.rem_jetty_id;

    return;
}

int ra_hdc_ctx_qp_unimport_async(struct ra_ctx_rem_qp_handle *rem_qp_handle, void **req_handle)
{
    struct RaRequestHandle *req_handle_tmp = NULL;
    union op_ctx_qp_unimport_data async_data = {0};
    unsigned int phy_id = rem_qp_handle->phy_id;
    int ret;

    async_data.tx_data.phy_id = phy_id;
    async_data.tx_data.dev_index = rem_qp_handle->dev_index;
    async_data.tx_data.rem_jetty_id = rem_qp_handle->id;

    req_handle_tmp = (struct RaRequestHandle *)calloc(1, sizeof(struct RaRequestHandle));
    CHK_PRT_RETURN(req_handle_tmp == NULL,
        hccp_err("[deinit][ra_hdc_qp]calloc req_handle_tmp failed, phy_id[%u], dev_index[0x%x]",
        phy_id, rem_qp_handle->dev_index), -ENOMEM);

    ret = RaHdcSendMsgAsync(RA_RS_CTX_QP_UNIMPORT, phy_id, (char *)&async_data,
        sizeof(union op_ctx_qp_unimport_data), req_handle_tmp);
    if (ret != 0) {
        hccp_err("[deinit][ra_hdc_qp]hdc async send message failed ret[%d], phy_id[%u] dev_index[0x%x]",
            ret, phy_id, rem_qp_handle->dev_index);
        free(req_handle_tmp);
        req_handle_tmp = NULL;
        return ret;
    }

    *req_handle = (void *)req_handle_tmp;
    return 0;
}

int ra_hdc_get_tp_info_list_async(struct ra_ctx_handle *ctx_handle, struct get_tp_cfg *cfg, struct tp_info info_list[],
    unsigned int *num, void **req_handle)
{
    struct ra_response_tp_info_list *async_rsp = NULL;
    struct RaRequestHandle *req_handle_tmp = NULL;
    union op_get_tp_info_list_data async_data = {0};
    unsigned int phy_id = ctx_handle->attr.phy_id;
    int ret = 0;

    async_rsp = (struct ra_response_tp_info_list *)calloc(1, sizeof(struct ra_response_tp_info_list));
    CHK_PRT_RETURN(async_rsp == NULL,
        hccp_err("[get][ra_hdc_tp_info]calloc async_rsp failed, phy_id[%u] dev_index[0x%x]",
        phy_id, ctx_handle->dev_index), -ENOMEM);
    async_rsp->info_list = info_list;
    async_rsp->num = num;

    async_data.tx_data.phy_id = phy_id;
    async_data.tx_data.dev_index = ctx_handle->dev_index;
    async_data.tx_data.num = *num;
    (void)memcpy_s(&async_data.tx_data.cfg, sizeof(struct get_tp_cfg), cfg, sizeof(struct get_tp_cfg));

    req_handle_tmp = (struct RaRequestHandle *)calloc(1, sizeof(struct RaRequestHandle));
    if (req_handle_tmp == NULL) {
        hccp_err("[get][ra_hdc_tp_info]calloc RaRequestHandle failed, phy_id[%u], dev_index[0x%x]",
            phy_id, ctx_handle->dev_index);
        ret = -ENOMEM;
        goto out;
    }

    req_handle_tmp->devIndex = ctx_handle->dev_index;
    req_handle_tmp->privData = (void *)async_rsp;
    ret = RaHdcSendMsgAsync(RA_RS_GET_TP_INFO_LIST, phy_id, (char *)&async_data,
        sizeof(union op_get_tp_info_list_data), req_handle_tmp);
    if (ret != 0) {
        hccp_err("[get][ra_hdc_tp_info]hdc async send message failed ret[%d], phy_id[%u] dev_index[0x%x]",
            ret, phy_id, ctx_handle->dev_index);
        free(req_handle_tmp);
        req_handle_tmp = NULL;
        goto out;
    }

    *req_handle = (void *)req_handle_tmp;
    return 0;
out:
    free(async_rsp);
    async_rsp = NULL;
    return ret;
}

void ra_hdc_async_handle_tp_info_list(struct RaRequestHandle *req_handle)
{
    struct ra_response_tp_info_list *async_rsp = NULL;
    union op_get_tp_info_list_data *async_data = NULL;
    int ret;

    if (req_handle->opRet != 0) {
        goto out;
    }
    async_data = (union op_get_tp_info_list_data *)req_handle->recvBuf;
    async_rsp = (struct ra_response_tp_info_list *)req_handle->privData;
    if (async_data->rx_data.num == 0) {
        *async_rsp->num = async_data->rx_data.num;
        goto out;
    }

    ret = memcpy_s(async_rsp->info_list, (*async_rsp->num) * sizeof(struct tp_info),
        async_data->rx_data.info_list, async_data->rx_data.num * sizeof(struct tp_info));
    if (ret != 0) {
        hccp_err("[get][ra_hdc_tp_info]memcpy_s tp_info failed ret[%d] *async_rsp->num[%u] rx_data.num[%u], "
            "phy_id[%u] dev_index[0x%x]", ret, *async_rsp->num, async_data->rx_data.num,
            req_handle->phyId, req_handle->devIndex);
        req_handle->opRet = -ESAFEFUNC;
        goto out;
    }
    *async_rsp->num = async_data->rx_data.num;
out:
    free(req_handle->privData);
    req_handle->privData = NULL;
    return;
}

int ra_hdc_get_tp_attr_async(struct ra_ctx_handle *ctx_handle, uint64_t tp_handle, uint32_t *attr_bitmap,
    struct tp_attr *attr, void **req_handle)
{
    struct ra_response_get_tp_attr *async_rsp = NULL;
    struct RaRequestHandle *req_handle_tmp = NULL;
    unsigned int phy_id = ctx_handle->attr.phy_id;
    union op_get_tp_attr_data async_data = {0};
    int ret = 0;

    async_rsp = (struct ra_response_get_tp_attr *)calloc(1, sizeof(struct ra_response_get_tp_attr));
    CHK_PRT_RETURN(async_rsp == NULL,
        hccp_err("[get][ra_hdc_tp_attr]calloc ra_response_get_tp_attr failed, phy_id[%u] dev_index[0x%x]",
        phy_id, ctx_handle->dev_index), -ENOMEM);
    async_rsp->attr = attr;
    async_rsp->attr_bitmap = attr_bitmap;

    async_data.tx_data.dev_index = ctx_handle->dev_index;
    async_data.tx_data.phy_id = phy_id;
    async_data.tx_data.tp_handle = tp_handle;
    async_data.tx_data.attr_bitmap = *attr_bitmap;
    req_handle_tmp = (struct RaRequestHandle *)calloc(1, sizeof(struct RaRequestHandle));
    if (req_handle_tmp == NULL) {
        hccp_err("[get][ra_hdc_tp_attr]calloc RaRequestHandle failed, phy_id[%u] dev_index[0x%x]",
            phy_id, ctx_handle->dev_index);
        ret = -ENOMEM;
        goto out;
    }

    req_handle_tmp->devIndex = ctx_handle->dev_index;
    req_handle_tmp->privData = (void *)async_rsp;
    ret = RaHdcSendMsgAsync(RA_RS_GET_TP_ATTR, phy_id, (char *)&async_data,
        sizeof(union op_get_tp_attr_data), req_handle_tmp);
    if (ret != 0) {
        hccp_err("[get][ra_hdc_tp_attr]hdc async send message failed ret[%d], phy_id[%u] dev_index[0x%x]",
            ret, phy_id, ctx_handle->dev_index);
        free(req_handle_tmp);
        req_handle_tmp = NULL;
        goto out;
    }

    *req_handle = (void *)req_handle_tmp;
    return 0;
out:
    free(async_rsp);
    async_rsp = NULL;
    return ret;
}

void ra_hdc_async_handle_get_tp_attr(struct RaRequestHandle *req_handle)
{
    struct ra_response_get_tp_attr *async_rsp = NULL;
    union op_get_tp_attr_data *async_data = NULL;

    if (req_handle->opRet != 0) {
        goto out;
    }
    async_data = (union op_get_tp_attr_data *)req_handle->recvBuf;
    async_rsp = (struct ra_response_get_tp_attr *)req_handle->privData;
    *async_rsp->attr_bitmap = async_data->rx_data.attr_bitmap;
    (void)memcpy_s(async_rsp->attr, sizeof(struct tp_attr), &async_data->rx_data.attr, sizeof(struct tp_attr));

out:
    free(req_handle->privData);
    req_handle->privData = NULL;
    return;
}

int ra_hdc_set_tp_attr_async(struct ra_ctx_handle *ctx_handle, uint64_t tp_handle, uint32_t attr_bitmap,
    struct tp_attr *attr, void **req_handle)
{
    struct RaRequestHandle *req_handle_tmp = NULL;
    unsigned int phy_id = ctx_handle->attr.phy_id;
    union op_set_tp_attr_data async_data = {0};
    int ret = 0;

    async_data.tx_data.dev_index = ctx_handle->dev_index;
    async_data.tx_data.phy_id = phy_id;
    async_data.tx_data.tp_handle = tp_handle;
    async_data.tx_data.attr_bitmap = attr_bitmap;
    (void)memcpy_s(&async_data.tx_data.attr, sizeof(struct tp_attr), attr, sizeof(struct tp_attr));
    req_handle_tmp = (struct RaRequestHandle *)calloc(1, sizeof(struct RaRequestHandle));
    CHK_PRT_RETURN(req_handle_tmp == NULL,
        hccp_err("[set][ra_hdc_tp_attr]calloc RaRequestHandle failed, phy_id[%u] dev_index[0x%x]",
        phy_id, ctx_handle->dev_index), -ENOMEM);

    req_handle_tmp->devIndex = ctx_handle->dev_index;
    ret = RaHdcSendMsgAsync(RA_RS_SET_TP_ATTR, phy_id, (char *)&async_data,
        sizeof(union op_set_tp_attr_data), req_handle_tmp);
    if (ret != 0) {
        hccp_err("[set][ra_hdc_tp_attr]hdc async send message failed ret[%d], phy_id[%u] dev_index[0x%x]",
            ret, phy_id, ctx_handle->dev_index);
        free(req_handle_tmp);
        req_handle_tmp = NULL;
        return ret;
    }

    *req_handle = (void *)req_handle_tmp;
    return 0;
}

STATIC int qp_destroy_batch_param_check(struct ra_ctx_handle *ctx_handle, void *qp_handle[],
    unsigned int ids[], unsigned int *num)
{
    struct ra_ctx_qp_handle *qp_handle_tmp = NULL;
    unsigned int i;

    for (i = 0; i < *num; ++i) {
        qp_handle_tmp = (struct ra_ctx_qp_handle *)qp_handle[i];
        CHK_PRT_RETURN(qp_handle_tmp == NULL,
            hccp_err("[destroy_batch][ra_hdc_ctx_qp]qp_handle[%u] is NULL", i), -EINVAL);
        CHK_PRT_RETURN(qp_handle_tmp->ctx_handle == NULL,
            hccp_err("[destroy_batch][ra_hdc_ctx_qp]ctx_handle[%u] is NULL", i), -EINVAL);
        CHK_PRT_RETURN(qp_handle_tmp->ctx_handle != ctx_handle,
            hccp_err("[destroy_batch][ra_hdc_ctx_qp]qp_handle[%u]->ctx_handle is different from others", i), -EINVAL);

        ids[i] = qp_handle_tmp->id;
    }

    return 0;
}

int ra_hdc_ctx_qp_destroy_batch_async(struct ra_ctx_handle *ctx_handle, void *qp_handle[],
    unsigned int *num, void **req_handle)
{
    union op_ctx_qp_destroy_batch_data async_data = {0};
    struct RaRequestHandle *req_handle_tmp = NULL;
    unsigned int phy_id = ctx_handle->attr.phy_id;
    int ret;

    ret = qp_destroy_batch_param_check(ctx_handle, qp_handle, async_data.tx_data.ids, num);
    CHK_PRT_RETURN(ret != 0, hccp_err("[destroy_batch][ra_hdc_ctx_qp]param check failed, phy_id[%u] dev_index[0x%x]",
        phy_id, ctx_handle->dev_index), ret);
    
    async_data.tx_data.phy_id = phy_id;
    async_data.tx_data.dev_index = ctx_handle->dev_index;
    async_data.tx_data.num = *num;

    req_handle_tmp = (struct RaRequestHandle *)calloc(1, sizeof(struct RaRequestHandle));
    CHK_PRT_RETURN(req_handle_tmp == NULL, hccp_err("[destroy_batch][ra_hdc_ctx_qp]calloc RaRequestHandle failed, "
        "phy_id[%u] dev_index[0x%x]", phy_id, ctx_handle->dev_index), -ENOMEM);

    req_handle_tmp->devIndex = ctx_handle->dev_index;
    req_handle_tmp->privData = (void *)num;
    ret = RaHdcSendMsgAsync(RA_RS_CTX_QP_DESTROY_BATCH, phy_id, (char *)&async_data,
        sizeof(union op_ctx_qp_destroy_batch_data), req_handle_tmp);
    if (ret != 0) {
        hccp_err("[destroy_batch][ra_hdc_ctx_qp]hdc async send message failed ret[%d], phy_id[%u] dev_index[0x%x]",
            ret, phy_id, ctx_handle->dev_index);
        free(req_handle_tmp);
        req_handle_tmp = NULL;
        return ret;
    }

    *req_handle = (void *)req_handle_tmp;
    return 0;
}

void ra_hdc_async_handle_qp_destroy_batch(struct RaRequestHandle *req_handle)
{
    union op_ctx_qp_destroy_batch_data *async_data = NULL;
    unsigned int *num = NULL;

    async_data = (union op_ctx_qp_destroy_batch_data *)req_handle->recvBuf;
    num = (unsigned int *)req_handle->privData;
    *num = async_data->rx_data.num;

    return;
}

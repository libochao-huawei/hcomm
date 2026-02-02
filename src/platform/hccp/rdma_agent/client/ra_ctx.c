/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024. All rights reserved.
 * Description: rdma_agent ctx function realization
 * Create: 2024-09-16
 */
#include "securec.h"
#include "user_log.h"
#include "dl_hal_function.h"
#include "hccp_common.h"
#include "hccp_ctx.h"
#include "ra_client_host.h"
#include "ra.h"
#include "ra_hdc.h"
#include "ra_hdc_ctx.h"
#include "ra_peer_ctx.h"
#include "ra_rs_ctx.h"
#include "ra_ctx.h"

struct ra_ctx_ops g_ra_hdc_ctx_ops = {
    .ra_ctx_init = ra_hdc_ctx_init,
    .ra_ctx_get_async_events = ra_hdc_ctx_get_async_events,
    .ra_ctx_deinit = ra_hdc_ctx_deinit,
    .ra_ctx_get_eid_by_ip = ra_hdc_get_eid_by_ip,
    .ra_ctx_token_id_alloc = ra_hdc_ctx_token_id_alloc,
    .ra_ctx_token_id_free = ra_hdc_ctx_token_id_free,
    .ra_ctx_lmem_register = ra_hdc_ctx_lmem_register,
    .ra_ctx_lmem_unregister = ra_hdc_ctx_lmem_unregister,
    .ra_ctx_rmem_import = ra_hdc_ctx_rmem_import,
    .ra_ctx_rmem_unimport = ra_hdc_ctx_rmem_unimport,
    .ra_ctx_chan_create = ra_hdc_ctx_chan_create,
    .ra_ctx_chan_destroy = ra_hdc_ctx_chan_destroy,
    .ra_ctx_cq_create = ra_hdc_ctx_cq_create,
    .ra_ctx_cq_destroy = ra_hdc_ctx_cq_destroy,
    .ra_ctx_qp_create = ra_hdc_ctx_qp_create,
    .ra_ctx_qp_destroy = ra_hdc_ctx_qp_destroy,
    .ra_ctx_qp_import = ra_hdc_ctx_qp_import,
    .ra_ctx_qp_unimport = ra_hdc_ctx_qp_unimport,
    .ra_ctx_qp_bind = ra_hdc_ctx_qp_bind,
    .ra_ctx_qp_unbind = ra_hdc_ctx_qp_unbind,
    .ra_ctx_batch_send_wr = ra_hdc_ctx_batch_send_wr,
    .ra_ctx_update_ci = ra_hdc_ctx_update_ci,
    .ra_ctx_query_qp_batch = ra_hdc_ctx_qp_query_batch,
    .ra_ctx_get_aux_info = ra_hdc_ctx_get_aux_info,
};

struct ra_ctx_ops g_ra_peer_ctx_ops = {
    .ra_ctx_init = ra_peer_ctx_init,
    .ra_ctx_get_async_events = ra_peer_ctx_get_async_events,
    .ra_ctx_deinit = ra_peer_ctx_deinit,
    .ra_ctx_get_eid_by_ip = ra_peer_get_eid_by_ip,
    .ra_ctx_token_id_alloc = ra_peer_ctx_token_id_alloc,
    .ra_ctx_token_id_free = ra_peer_ctx_token_id_free,
    .ra_ctx_lmem_register = ra_peer_ctx_lmem_register,
    .ra_ctx_lmem_unregister = ra_peer_ctx_lmem_unregister,
    .ra_ctx_rmem_import = ra_peer_ctx_rmem_import,
    .ra_ctx_rmem_unimport = ra_peer_ctx_rmem_unimport,
    .ra_ctx_chan_create = ra_peer_ctx_chan_create,
    .ra_ctx_chan_destroy = ra_peer_ctx_chan_destroy,
    .ra_ctx_cq_create = ra_peer_ctx_cq_create,
    .ra_ctx_cq_destroy = ra_peer_ctx_cq_destroy,
    .ra_ctx_qp_create = ra_peer_ctx_qp_create,
    .ra_ctx_qp_destroy = ra_peer_ctx_qp_destroy,
    .ra_ctx_qp_import = ra_peer_ctx_qp_import,
    .ra_ctx_qp_unimport = ra_peer_ctx_qp_unimport,
    .ra_ctx_qp_bind = ra_peer_ctx_qp_bind,
    .ra_ctx_qp_unbind = ra_peer_ctx_qp_unbind,
    .ra_ctx_batch_send_wr = NULL,
    .ra_ctx_update_ci = NULL,
    .ra_ctx_query_qp_batch = NULL,
    .ra_ctx_get_aux_info = NULL,
};

HCCP_ATTRI_VISI_DEF int ra_get_dev_eid_info_num(struct RaInfo info, unsigned int *num)
{
    int ret;

    CHK_PRT_RETURN(num == NULL, hccp_err("[get][eid]num is NULL"), ConverReturnCode(RDMA_OP, -EINVAL));
    CHK_PRT_RETURN(info.phyId >= RA_MAX_PHY_ID_NUM, hccp_err("[get][eid]phy_id(%u) must smaller than %u",
        info.phyId, RA_MAX_PHY_ID_NUM), ConverReturnCode(RDMA_OP, -EINVAL));

    hccp_run_info("Input parameters: phy_id[%u], nic_position:[%d]", info.phyId, info.mode);
    if (info.mode == NETWORK_OFFLINE) {
        ret = ra_hdc_get_dev_eid_info_num(info, num);
        CHK_PRT_RETURN(ret != 0, hccp_err("[get][eid]ra_hdc_get_dev_eid_info_num failed, ret(%d) phy_id(%u)",
            ret, info.phyId), ConverReturnCode(RDMA_OP, ret));
    } else if (info.mode == NETWORK_PEER_ONLINE) {
        ret = ra_peer_get_dev_eid_info_num(info, num);
        CHK_PRT_RETURN(ret != 0, hccp_err("[get][eid]ra_peer_get_dev_eid_info_num failed, ret(%d) phy_id(%u)",
            ret, info.phyId), ConverReturnCode(RDMA_OP, ret));
    } else {
        hccp_err("[get][eid]mode(%d) do not support, phy_id(%u)", info.mode, info.phyId);
        return ConverReturnCode(RDMA_OP, -EINVAL);
    }

    return 0;
}

HCCP_ATTRI_VISI_DEF int ra_get_dev_eid_info_list(struct RaInfo info, struct dev_eid_info info_list[],
    unsigned int *num)
{
    int ret;

    CHK_PRT_RETURN(info_list == NULL || num == NULL, hccp_err("[get][eid]info_list or num is NULL"),
        ConverReturnCode(RDMA_OP, -EINVAL));
    CHK_PRT_RETURN(info.phyId >= RA_MAX_PHY_ID_NUM, hccp_err("[get][eid]phy_id(%u) must smaller than %u",
        info.phyId, RA_MAX_PHY_ID_NUM), ConverReturnCode(RDMA_OP, -EINVAL));

    hccp_run_info("Input parameters: phy_id[%u], nic_position:[%d]", info.phyId, info.mode);
    if (info.mode == NETWORK_OFFLINE) {
        ret = ra_hdc_get_dev_eid_info_list(info.phyId, info_list, num);
        CHK_PRT_RETURN(ret != 0, hccp_err("[get][eid]ra_hdc_get_dev_eid_info_list failed, ret(%d) phy_id(%u)",
            ret, info.phyId), ConverReturnCode(RDMA_OP, ret));
    } else if (info.mode == NETWORK_PEER_ONLINE) {
        ret = ra_peer_get_dev_eid_info_list(info.phyId, info_list, num);
        CHK_PRT_RETURN(ret != 0, hccp_err("[get][eid]ra_peer_get_dev_eid_info_list failed, ret(%d) phy_id(%u)",
            ret, info.phyId), ConverReturnCode(RDMA_OP, ret));
    } else {
        hccp_err("[get][eid]mode(%d) do not support, phy_id(%u)", info.mode, info.phyId);
        return ConverReturnCode(RDMA_OP, -EINVAL);
    }

    return 0;
}

STATIC int ra_get_init_ctx_handle(struct ctx_init_cfg *cfg, struct ctx_init_attr *attr,
    struct ra_ctx_handle *ctx_handle)
{
    ctx_handle->protocol = PROTOCOL_UDMA;

    if (cfg->mode == NETWORK_OFFLINE) {
        ctx_handle->ctx_ops = &g_ra_hdc_ctx_ops;
    } else if (cfg->mode == NETWORK_PEER_ONLINE) {
        ctx_handle->ctx_ops = &g_ra_peer_ctx_ops;
    } else {
        hccp_err("[init][ra_ctx]mode(%d) do not support, phy_id(%u)", cfg->mode, attr->phy_id);
        return -EINVAL;
    }

    CHK_PRT_RETURN(ctx_handle->ctx_ops->ra_ctx_init == NULL, hccp_err("[init][ra_ctx]ra_ctx_init is NULL, phy_id(%u)",
        attr->phy_id), -EINVAL);

    (void)memcpy_s(&(ctx_handle->attr), sizeof(struct ctx_init_attr), attr, sizeof(struct ctx_init_attr));
    return 0;
}

HCCP_ATTRI_VISI_DEF int ra_ctx_init(struct ctx_init_cfg *cfg, struct ctx_init_attr *attr, void **ctx_handle)
{
    struct ra_ctx_handle *ctx_handle_tmp = NULL;
    int ret;

    CHK_PRT_RETURN(cfg == NULL || attr == NULL || ctx_handle == NULL,
        hccp_err("[init][ra_ctx]cfg or attr or ctx_handle is NULL"), ConverReturnCode(HCCP_INIT, -EINVAL));
    CHK_PRT_RETURN(attr->phy_id >= RA_MAX_PHY_ID_NUM, hccp_err("[init][ra_ctx]phy_id(%u) must smaller than %u",
        attr->phy_id, RA_MAX_PHY_ID_NUM), ConverReturnCode(HCCP_INIT, -EINVAL));

    ctx_handle_tmp = calloc(1, sizeof(struct ra_ctx_handle));
    CHK_PRT_RETURN(ctx_handle_tmp == NULL, hccp_err("[init][ra_ctx]calloc ctx_handle failed, errno(%d) phy_id(%u)",
        errno, attr->phy_id), ConverReturnCode(HCCP_INIT, -ENOMEM));

    ret = ra_get_init_ctx_handle(cfg, attr, ctx_handle_tmp);
    if (ret != 0) {
        hccp_err("[init][ra_ctx]ra_get_init_ctx_handle failed, ret(%d) phy_id(%u)", ret, attr->phy_id);
        goto err;
    }

    hccp_run_info("Input parameters: phy_id[%u], nic_position:[%d]", attr->phy_id, cfg->mode);
    ret = ctx_handle_tmp->ctx_ops->ra_ctx_init(ctx_handle_tmp, attr, &(ctx_handle_tmp->dev_index),
        &(ctx_handle_tmp->dev_attr));
    if (ret != 0) {
        hccp_err("[init][ra_ctx]ctx init failed, ret(%d) phy_id(%u)", ret, attr->phy_id);
        goto err;
    }

    *ctx_handle = (void *)ctx_handle_tmp;
    return 0;

err:
    free(ctx_handle_tmp);
    ctx_handle_tmp = NULL;
    return ConverReturnCode(HCCP_INIT, ret);
}

HCCP_ATTRI_VISI_DEF int ra_get_dev_base_attr(void *ctx_handle, struct dev_base_attr *attr)
{
    struct ra_ctx_handle *ctx_handle_tmp = NULL;

    CHK_PRT_RETURN(ctx_handle == NULL || attr == NULL, hccp_err("[get][dev_attr]ctx_handle or attr is NULL"),
        ConverReturnCode(RDMA_OP, -EINVAL));

    ctx_handle_tmp = (struct ra_ctx_handle *)ctx_handle;
    (void)memcpy_s(attr, sizeof(struct dev_base_attr), &(ctx_handle_tmp->dev_attr), sizeof(struct dev_base_attr));

    hccp_info("[get][dev_attr]phy_id(%u), dev_index(%u)", ctx_handle_tmp->attr.phy_id, ctx_handle_tmp->dev_index);
    return 0;
}

HCCP_ATTRI_VISI_DEF int ra_ctx_get_async_events(void *ctx_handle, struct async_event events[], unsigned int *num)
{
    struct ra_ctx_handle *ctx_handle_tmp = NULL;
    int ret = 0;

    CHK_PRT_RETURN(ctx_handle == NULL || events == NULL || num == NULL,
        hccp_err("[get][async_events]ctx_handle or events or num is NULL"), ConverReturnCode(RDMA_OP, -EINVAL));

    CHK_PRT_RETURN(*num == 0 || *num > ASYNC_EVENT_MAX_NUM, hccp_err("[get][async_events]num:%u must greater than 0"
        " and less or equal to %d", *num, ASYNC_EVENT_MAX_NUM), ConverReturnCode(RDMA_OP, -EINVAL));

    ctx_handle_tmp = (struct ra_ctx_handle *)ctx_handle;
    CHK_PRT_RETURN(ctx_handle_tmp->ctx_ops == NULL || ctx_handle_tmp->ctx_ops->ra_ctx_get_async_events == NULL,
        hccp_err("[get][async_events]ctx_ops or ra_ctx_get_async_events is NULL"), ConverReturnCode(RDMA_OP, -EINVAL));

    ret = ctx_handle_tmp->ctx_ops->ra_ctx_get_async_events(ctx_handle_tmp, events, num);
    CHK_PRT_RETURN(ret != 0, hccp_err("[get][async_events]get async events failed, ret:%d phy_id:%u dev_index:0x%x",
        ret, ctx_handle_tmp->attr.phy_id, ctx_handle_tmp->dev_index), ConverReturnCode(RDMA_OP, ret));

    return ConverReturnCode(RDMA_OP, ret);
}

HCCP_ATTRI_VISI_DEF int ra_ctx_deinit(void *ctx_handle)
{
    struct ra_ctx_handle *ctx_handle_tmp = NULL;
    int ret;

    CHK_PRT_RETURN(ctx_handle == NULL, hccp_err("[deinit][ra_ctx]ctx_handle is NULL"),
        ConverReturnCode(RDMA_OP, -EINVAL));

    ctx_handle_tmp = (struct ra_ctx_handle *)ctx_handle;
    CHK_PRT_RETURN(ctx_handle_tmp->ctx_ops == NULL || ctx_handle_tmp->ctx_ops->ra_ctx_deinit == NULL,
        hccp_err("[deinit][ra_ctx]ctx_ops or ra_ctx_deinit is NULL"), ConverReturnCode(RDMA_OP, -EINVAL));

    hccp_run_info("Input parameters: phy_id[%u], dev_index[%u], protocol[%d]",
        ctx_handle_tmp->attr.phy_id, ctx_handle_tmp->dev_index, ctx_handle_tmp->protocol);
    ret = ctx_handle_tmp->ctx_ops->ra_ctx_deinit(ctx_handle_tmp);
    if (ret != 0) {
        hccp_err("[deinit][ra_ctx]ctx deinit failed, ret(%d) phy_id(%u) dev_index(%u)",
            ret, ctx_handle_tmp->attr.phy_id, ctx_handle_tmp->dev_index);
    }

    ctx_handle_tmp->ctx_ops = NULL;
    free(ctx_handle_tmp);
    ctx_handle_tmp = NULL;
    return ConverReturnCode(HCCP_INIT, ret);
}

HCCP_ATTRI_VISI_DEF int ra_get_eid_by_ip(void *ctx_handle, struct IpInfo ip[], union hccp_eid eid[], unsigned int *num)
{
    struct ra_ctx_handle *ctx_handle_tmp = NULL;
    int ret = 0;

    CHK_PRT_RETURN(ctx_handle == NULL || ip == NULL || eid == NULL || num == NULL,
        hccp_err("[get][eid_by_ip]ctx_handle or ip or eid or num is NULL"), ConverReturnCode(RDMA_OP, -EINVAL));

    CHK_PRT_RETURN(*num == 0 || *num > GET_EID_BY_IP_MAX_NUM, hccp_err("[get][eid_by_ip]num(%u) must greater than 0"
        " and less or equal to %d", *num, GET_EID_BY_IP_MAX_NUM), ConverReturnCode(RDMA_OP, -EINVAL));

    ctx_handle_tmp = (struct ra_ctx_handle *)ctx_handle;

    CHK_PRT_RETURN(ctx_handle_tmp->ctx_ops == NULL || ctx_handle_tmp->ctx_ops->ra_ctx_get_eid_by_ip == NULL,
        hccp_err("[get][eid_by_ip]ctx_ops or ra_ctx_get_eid_by_ip is NULL"), ConverReturnCode(RDMA_OP, -EINVAL));

    hccp_run_info("Input parameters: phy_id(%u), dev_index(0x%x)",
        ctx_handle_tmp->attr.phy_id, ctx_handle_tmp->dev_index);

    ret = ctx_handle_tmp->ctx_ops->ra_ctx_get_eid_by_ip(ctx_handle, ip, eid, num);
    CHK_PRT_RETURN(ret != 0, hccp_err("[get][eid_by_ip]ra_ctx_get_eid_by_ip failed, ret(%d) phy_id(%u) dev_index(0x%x)",
        ret, ctx_handle_tmp->attr.phy_id, ctx_handle_tmp->dev_index), ConverReturnCode(RDMA_OP, ret));

    return ret;
}

HCCP_ATTRI_VISI_DEF int ra_ctx_token_id_alloc(void *ctx_handle, struct hccp_token_id *info, void **token_id_handle)
{
    struct ra_token_id_handle *token_id_handle_tmp = NULL;
    struct ra_ctx_handle *ctx_handle_tmp = NULL;
    int ret;

    CHK_PRT_RETURN(ctx_handle == NULL || info == NULL || token_id_handle == NULL,
        hccp_err("[init][ra_token_id]ctx_handle or info or token_id_handle is NULL"),
        ConverReturnCode(RDMA_OP, -EINVAL));

    ctx_handle_tmp = (struct ra_ctx_handle *)ctx_handle;
    CHK_PRT_RETURN(ctx_handle_tmp->ctx_ops == NULL || ctx_handle_tmp->ctx_ops->ra_ctx_token_id_alloc == NULL,
        hccp_err("[init][ra_token_id]ctx_ops or ra_ctx_token_id_alloc is NULL"), ConverReturnCode(RDMA_OP, -EINVAL));

    token_id_handle_tmp = (struct ra_token_id_handle *)calloc(1, sizeof(struct ra_token_id_handle));
    CHK_PRT_RETURN(token_id_handle_tmp == NULL,
        hccp_err("[init][ra_token_id]calloc token_id_handle_tmp failed, errno(%d) phy_id(%u) dev_index(%u)",
        errno, ctx_handle_tmp->attr.phy_id, ctx_handle_tmp->dev_index), ConverReturnCode(RDMA_OP, -ENOMEM));

    ret = ctx_handle_tmp->ctx_ops->ra_ctx_token_id_alloc(ctx_handle_tmp, info, token_id_handle_tmp);
    if (ret != 0) {
        hccp_err("[init][ra_token_id]alloc failed, ret(%d) phy_id(%u) dev_index(%u)",
            ret, ctx_handle_tmp->attr.phy_id, ctx_handle_tmp->dev_index);
        goto err;
    }

    *token_id_handle = (void *)token_id_handle_tmp;
    return 0;

err:
    free(token_id_handle_tmp);
    token_id_handle_tmp = NULL;
    return ConverReturnCode(RDMA_OP, ret);
}

HCCP_ATTRI_VISI_DEF int ra_ctx_token_id_free(void *ctx_handle, void *token_id_handle)
{
    struct ra_token_id_handle *token_id_handle_tmp = NULL;
    struct ra_ctx_handle *ctx_handle_tmp = NULL;
    int ret;

    CHK_PRT_RETURN(ctx_handle == NULL || token_id_handle == NULL,
        hccp_err("[deinit][ra_token_id]ctx_handle or token_id_handle is NULL"), ConverReturnCode(RDMA_OP, -EINVAL));

    ctx_handle_tmp = (struct ra_ctx_handle *)ctx_handle;
    CHK_PRT_RETURN(ctx_handle_tmp->ctx_ops == NULL || ctx_handle_tmp->ctx_ops->ra_ctx_token_id_free == NULL,
        hccp_err("[deinit][ra_token_id]ctx_ops or ra_ctx_token_id_free is NULL"), ConverReturnCode(RDMA_OP, -EINVAL));

    token_id_handle_tmp = (struct ra_token_id_handle *)token_id_handle;
    ret = ctx_handle_tmp->ctx_ops->ra_ctx_token_id_free(ctx_handle_tmp, token_id_handle_tmp);
    if (ret != 0) {
        hccp_err("[deinit][ra_token_id]free failed, ret(%d) phy_id(%u) dev_index(%u)",
            ret, ctx_handle_tmp->attr.phy_id, ctx_handle_tmp->dev_index);
    }

    free(token_id_handle_tmp);
    token_id_handle_tmp = NULL;
    return ConverReturnCode(RDMA_OP, ret);
}

HCCP_ATTRI_VISI_DEF int ra_ctx_lmem_register(void *ctx_handle, struct mr_reg_info_t *lmem_info, void **lmem_handle)
{
    struct ra_lmem_handle *lmem_handle_tmp = NULL;
    struct ra_ctx_handle *ctx_handle_tmp = NULL;
    int ret;

    CHK_PRT_RETURN(ctx_handle == NULL || lmem_info == NULL || lmem_handle == NULL,
        hccp_err("[init][ra_lmem]ctx_handle or lmem_info or lmem_handle is NULL"),
        ConverReturnCode(RDMA_OP, -EINVAL));

    ctx_handle_tmp = (struct ra_ctx_handle *)ctx_handle;
    CHK_PRT_RETURN(ctx_handle_tmp->ctx_ops == NULL || ctx_handle_tmp->ctx_ops->ra_ctx_lmem_register == NULL,
        hccp_err("[init][ra_lmem]ctx_ops or ra_ctx_lmem_register is NULL"), ConverReturnCode(RDMA_OP, -EINVAL));

    lmem_handle_tmp = calloc(1, sizeof(struct ra_lmem_handle));
    CHK_PRT_RETURN(lmem_handle_tmp == NULL,
        hccp_err("[init][ra_lmem]calloc lmem_handle_tmp failed, errno(%d) phy_id(%u) dev_index(%u)",
        errno, ctx_handle_tmp->attr.phy_id, ctx_handle_tmp->dev_index), ConverReturnCode(RDMA_OP, -ENOMEM));

    ret = ctx_handle_tmp->ctx_ops->ra_ctx_lmem_register(ctx_handle_tmp, lmem_info, lmem_handle_tmp);
    if (ret != 0) {
        hccp_err("[init][ra_lmem]register failed, ret(%d) phy_id(%u) dev_index(%u)",
            ret, ctx_handle_tmp->attr.phy_id, ctx_handle_tmp->dev_index);
        goto err;
    }

    *lmem_handle = (void *)lmem_handle_tmp;
    return 0;

err:
    free(lmem_handle_tmp);
    lmem_handle_tmp = NULL;
    return ConverReturnCode(RDMA_OP, ret);
}

HCCP_ATTRI_VISI_DEF int ra_ctx_lmem_unregister(void *ctx_handle, void *lmem_handle)
{
    struct ra_lmem_handle *lmem_handle_tmp = NULL;
    struct ra_ctx_handle *ctx_handle_tmp = NULL;
    int ret;

    CHK_PRT_RETURN(ctx_handle == NULL || lmem_handle == NULL,
        hccp_err("[deinit][ra_lmem]ctx_handle or lmem_handle is NULL"), ConverReturnCode(RDMA_OP, -EINVAL));

    ctx_handle_tmp = (struct ra_ctx_handle *)ctx_handle;
    CHK_PRT_RETURN(ctx_handle_tmp->ctx_ops == NULL || ctx_handle_tmp->ctx_ops->ra_ctx_lmem_unregister == NULL,
        hccp_err("[deinit][ra_lmem]ctx_ops or ra_ctx_lmem_unregister is NULL"),
        ConverReturnCode(RDMA_OP, -EINVAL));

    lmem_handle_tmp = (struct ra_lmem_handle *)lmem_handle;
    ret = ctx_handle_tmp->ctx_ops->ra_ctx_lmem_unregister(ctx_handle_tmp, lmem_handle_tmp);
    if (ret != 0) {
        hccp_err("[deinit][ra_lmem]unregister failed, ret(%d) phy_id(%u) dev_index(%u)",
            ret, ctx_handle_tmp->attr.phy_id, ctx_handle_tmp->dev_index);
    }

    free(lmem_handle_tmp);
    lmem_handle_tmp = NULL;
    return ConverReturnCode(RDMA_OP, ret);
}

HCCP_ATTRI_VISI_DEF int ra_ctx_rmem_import(void *ctx_handle, struct mr_import_info_t *rmem_info, void **rmem_handle)
{
    struct ra_rmem_handle *rmem_handle_tmp = NULL;
    struct ra_ctx_handle *ctx_handle_tmp = NULL;
    int ret;

    CHK_PRT_RETURN(ctx_handle == NULL || rmem_info == NULL || rmem_handle == NULL,
        hccp_err("[init][ra_rmem]ctx_handle or rmem_info or rmem_handle is NULL"),
        ConverReturnCode(RDMA_OP, -EINVAL));

    ctx_handle_tmp = (struct ra_ctx_handle *)ctx_handle;
    CHK_PRT_RETURN(ctx_handle_tmp->ctx_ops == NULL || ctx_handle_tmp->ctx_ops->ra_ctx_rmem_import == NULL,
        hccp_err("[init][ra_rmem]ctx_ops is NULL or ra_ctx_rmem_import ops is NULL"),
        ConverReturnCode(RDMA_OP, -EINVAL));

    rmem_handle_tmp = calloc(1, sizeof(struct ra_rmem_handle));
    CHK_PRT_RETURN(rmem_handle_tmp == NULL,
        hccp_err("[init][ra_rmem]calloc rmem_handle_tmp failed, errno(%d) phy_id(%u) dev_index(%u)",
        errno, ctx_handle_tmp->attr.phy_id, ctx_handle_tmp->dev_index), ConverReturnCode(RDMA_OP, -ENOMEM));

    ret = ctx_handle_tmp->ctx_ops->ra_ctx_rmem_import(ctx_handle_tmp, rmem_info);
    if (ret != 0) {
        hccp_err("[init][ra_lmem]import failed, ret(%d) phy_id(%u) dev_index(%u)",
            ret, ctx_handle_tmp->attr.phy_id, ctx_handle_tmp->dev_index);
        goto err;
    }

    rmem_handle_tmp->key = rmem_info->in.key;
    rmem_handle_tmp->addr = rmem_info->out.ub.target_seg_handle;
    *rmem_handle = (void *)rmem_handle_tmp;
    return 0;

err:
    free(rmem_handle_tmp);
    rmem_handle_tmp = NULL;
    return ConverReturnCode(RDMA_OP, ret);
}

HCCP_ATTRI_VISI_DEF int ra_ctx_rmem_unimport(void *ctx_handle, void *rmem_handle)
{
    struct ra_rmem_handle *rmem_handle_tmp = NULL;
    struct ra_ctx_handle *ctx_handle_tmp = NULL;
    int ret;

    CHK_PRT_RETURN(ctx_handle == NULL || rmem_handle == NULL,
        hccp_err("[deinit][ra_rmem]ctx_handle or rmem_handle is NULL"), ConverReturnCode(RDMA_OP, -EINVAL));

    ctx_handle_tmp = (struct ra_ctx_handle *)ctx_handle;
    CHK_PRT_RETURN(ctx_handle_tmp->ctx_ops == NULL || ctx_handle_tmp->ctx_ops->ra_ctx_rmem_unimport == NULL,
        hccp_err("[deinit][ra_rmem]ctx_ops or ra_ctx_rmem_unimport is NULL"), ConverReturnCode(RDMA_OP, -EINVAL));

    rmem_handle_tmp = (struct ra_rmem_handle *)rmem_handle;
    ret = ctx_handle_tmp->ctx_ops->ra_ctx_rmem_unimport(ctx_handle_tmp, rmem_handle_tmp);
    if (ret != 0) {
        hccp_err("[deinit][ra_rmem]unimport failed, ret(%d) phy_id(%u) dev_index(%u)",
            ret, ctx_handle_tmp->attr.phy_id, ctx_handle_tmp->dev_index);
    }

    free(rmem_handle_tmp);
    rmem_handle_tmp = NULL;
    return ConverReturnCode(RDMA_OP, ret);
}

HCCP_ATTRI_VISI_DEF int ra_ctx_chan_create(void *ctx_handle, struct chan_info_t *chan_info, void **chan_handle)
{
    struct ra_chan_handle *chan_handle_tmp = NULL;
    struct ra_ctx_handle *ctx_handle_tmp = NULL;
    int ret;

    CHK_PRT_RETURN(ctx_handle == NULL || chan_info == NULL || chan_handle == NULL,
        hccp_err("[init][ra_chan]ctx_handle or chan_info or chan_handle is NULL"),
        ConverReturnCode(RDMA_OP, -EINVAL));

    ctx_handle_tmp = (struct ra_ctx_handle *)ctx_handle;
    CHK_PRT_RETURN(ctx_handle_tmp->ctx_ops == NULL || ctx_handle_tmp->ctx_ops->ra_ctx_chan_create == NULL,
        hccp_err("[init][ra_chan]ctx_ops or ra_ctx_chan_create ops is NULL"),
        ConverReturnCode(RDMA_OP, -EINVAL));

    chan_handle_tmp = (struct ra_chan_handle *)calloc(1, sizeof(struct ra_chan_handle));
    CHK_PRT_RETURN(chan_handle_tmp == NULL,
        hccp_err("[init][ra_chan]calloc chan_handle_tmp failed, errno(%d) phy_id(%u) dev_index(%u)",
        errno, ctx_handle_tmp->attr.phy_id, ctx_handle_tmp->dev_index), ConverReturnCode(RDMA_OP, -ENOMEM));

    ret = ctx_handle_tmp->ctx_ops->ra_ctx_chan_create(ctx_handle_tmp, chan_info, chan_handle_tmp);
    if (ret != 0) {
        hccp_err("[init][ra_chan]create failed, ret(%d)", ret);
        goto err;
    }

    *chan_handle = (void *)chan_handle_tmp;
    return 0;

err:
    free(chan_handle_tmp);
    chan_handle_tmp = NULL;
    return ConverReturnCode(RDMA_OP, ret);
}

HCCP_ATTRI_VISI_DEF int ra_ctx_chan_destroy(void *ctx_handle, void *chan_handle)
{
    struct ra_chan_handle *chan_handle_tmp = NULL;
    struct ra_ctx_handle *ctx_handle_tmp = NULL;
    int ret;

    CHK_PRT_RETURN(ctx_handle == NULL || chan_handle == NULL,
        hccp_err("[deinit][ra_chan]ctx_handle or chan_handle is NULL"), ConverReturnCode(RDMA_OP, -EINVAL));

    ctx_handle_tmp = (struct ra_ctx_handle *)ctx_handle;
    CHK_PRT_RETURN(ctx_handle_tmp->ctx_ops == NULL || ctx_handle_tmp->ctx_ops->ra_ctx_chan_destroy == NULL,
        hccp_err("[deinit][ra_chan]ctx_ops or ra_ctx_chan_destroy is NULL"), ConverReturnCode(RDMA_OP, -EINVAL));

    chan_handle_tmp = (struct ra_chan_handle *)chan_handle;
    ret = ctx_handle_tmp->ctx_ops->ra_ctx_chan_destroy(ctx_handle_tmp, chan_handle_tmp);
    if (ret != 0) {
        hccp_err("[deinit][ra_chan]destroy failed, ret(%d) phy_id(%u) dev_index(%u)",
            ret, ctx_handle_tmp->attr.phy_id, ctx_handle_tmp->dev_index);
    }

    free(chan_handle_tmp);
    chan_handle_tmp = NULL;
    return ConverReturnCode(RDMA_OP, ret);
}

HCCP_ATTRI_VISI_DEF int ra_ctx_cq_create(void *ctx_handle, struct cq_info_t *info, void **cq_handle)
{
    struct ra_ctx_handle *ctx_handle_tmp = NULL;
    struct ra_cq_handle *cq_handle_tmp = NULL;
    int ret;

    CHK_PRT_RETURN(ctx_handle == NULL || info == NULL || cq_handle == NULL,
        hccp_err("[init][ra_cq]ctx_handle or info or cq_handle is NULL"), ConverReturnCode(RDMA_OP, -EINVAL));

    ctx_handle_tmp = (struct ra_ctx_handle *)ctx_handle;
    CHK_PRT_RETURN(ctx_handle_tmp->ctx_ops == NULL || ctx_handle_tmp->ctx_ops->ra_ctx_cq_create == NULL,
        hccp_err("[init][ra_cq]ctx_ops or ra_ctx_cq_create ops is NULL"), ConverReturnCode(RDMA_OP, -EINVAL));

    cq_handle_tmp = (struct ra_cq_handle *)calloc(1, sizeof(struct ra_cq_handle));
    CHK_PRT_RETURN(cq_handle_tmp == NULL,
        hccp_err("[init][ra_cq]calloc cq_handle_tmp failed, errno(%d) phy_id(%u) dev_index(%u)",
        errno, ctx_handle_tmp->attr.phy_id, ctx_handle_tmp->dev_index), ConverReturnCode(RDMA_OP, -ENOMEM));

    ret = ctx_handle_tmp->ctx_ops->ra_ctx_cq_create(ctx_handle_tmp, info, cq_handle_tmp);
    if (ret != 0) {
        hccp_err("[init][ra_cq]create failed, ret(%d) phy_id(%u) dev_index(%u)",
            ret, ctx_handle_tmp->attr.phy_id, ctx_handle_tmp->dev_index);
        goto err;
    }

    *cq_handle = (void *)cq_handle_tmp;
    return 0;

err:
    free(cq_handle_tmp);
    cq_handle_tmp = NULL;
    return ConverReturnCode(RDMA_OP, ret);
}

HCCP_ATTRI_VISI_DEF int ra_ctx_cq_destroy(void *ctx_handle, void *cq_handle)
{
    struct ra_ctx_handle *ctx_handle_tmp = NULL;
    struct ra_cq_handle *cq_handle_tmp = NULL;
    int ret;

    CHK_PRT_RETURN(ctx_handle == NULL || cq_handle == NULL,
        hccp_err("[deinit][ra_cq]ctx_handle or cq_handle is NULL"), ConverReturnCode(RDMA_OP, -EINVAL));

    ctx_handle_tmp = (struct ra_ctx_handle *)ctx_handle;
    CHK_PRT_RETURN(ctx_handle_tmp->ctx_ops == NULL || ctx_handle_tmp->ctx_ops->ra_ctx_cq_destroy == NULL,
        hccp_err("[deinit][ra_cq]ctx_ops or ra_ctx_cq_destroy is NULL"), ConverReturnCode(RDMA_OP, -EINVAL));

    cq_handle_tmp = (struct ra_cq_handle *)cq_handle;
    ret = ctx_handle_tmp->ctx_ops->ra_ctx_cq_destroy(ctx_handle_tmp, cq_handle_tmp);
    if (ret != 0) {
        hccp_err("[deinit][ra_cq]destroy failed, ret(%d) phy_id(%u) dev_index(%u)",
            ret, ctx_handle_tmp->attr.phy_id, ctx_handle_tmp->dev_index);
    }

    free(cq_handle_tmp);
    cq_handle_tmp = NULL;
    return ConverReturnCode(RDMA_OP, ret);
}

HCCP_ATTRI_VISI_DEF int ra_ctx_qp_create(void *ctx_handle, struct qp_create_attr *attr, struct qp_create_info *info,
    void **qp_handle)
{
    struct ra_ctx_qp_handle *qp_handle_tmp = NULL;
    struct ra_ctx_handle *ctx_handle_tmp = NULL;
    int ret;

    CHK_PRT_RETURN(ctx_handle == NULL || attr == NULL || info == NULL || qp_handle == NULL,
        hccp_err("[init][ra_qp]ctx_handle or attr or info or qp_handle is NULL"),
        ConverReturnCode(RDMA_OP, -EINVAL));

    ctx_handle_tmp = (struct ra_ctx_handle *)ctx_handle;
    CHK_PRT_RETURN(ctx_handle_tmp->ctx_ops == NULL || ctx_handle_tmp->ctx_ops->ra_ctx_qp_create == NULL,
        hccp_err("[init][ra_qp]ctx_ops or ra_ctx_qp_create is NULL"), ConverReturnCode(RDMA_OP, -EINVAL));

    qp_handle_tmp = calloc(1, sizeof(struct ra_ctx_qp_handle));
    CHK_PRT_RETURN(qp_handle_tmp == NULL,
        hccp_err("[init][ra_qp]calloc qp_handle_tmp failed, errno(%d) phy_id(%u) dev_index(%u)",
        errno, ctx_handle_tmp->attr.phy_id, ctx_handle_tmp->dev_index), ConverReturnCode(RDMA_OP, -ENOMEM));

    ret = ctx_handle_tmp->ctx_ops->ra_ctx_qp_create(ctx_handle_tmp, attr, info, qp_handle_tmp);
    if (ret != 0) {
        hccp_err("[init][ra_qp]create failed, ret(%d) phy_id(%u) dev_index(%u)",
            ret, ctx_handle_tmp->attr.phy_id, ctx_handle_tmp->dev_index);
        goto err;
    }

    *qp_handle = (void *)qp_handle_tmp;
    return 0;

err:
    free(qp_handle_tmp);
    qp_handle_tmp = NULL;
    return ConverReturnCode(RDMA_OP, ret);
}

STATIC int qp_query_batch_param_check(void *qp_handle[], unsigned int *num, unsigned int phy_id, unsigned int ids[])
{
    struct ra_ctx_qp_handle *qp_handle_tmp = NULL;
    unsigned int i;

    for (i = 0; i < *num; i++) {
        qp_handle_tmp = (struct ra_ctx_qp_handle *)qp_handle[i];
        CHK_PRT_RETURN(qp_handle_tmp == NULL, hccp_err("[query][ra_qp]qp_handle[%u] is NULL", i), -EINVAL);
        CHK_PRT_RETURN(qp_handle_tmp->phy_id != phy_id,
            hccp_err("[query][ra_qp]qp_handle[%u] comes from different devices, phy_id[%u] != qp_handle[0]->phy_id(%u)",
            i, qp_handle_tmp->phy_id, phy_id), -EINVAL);
        CHK_PRT_RETURN(qp_handle_tmp->ctx_handle == NULL || qp_handle_tmp->ctx_handle->ctx_ops == NULL ||
            qp_handle_tmp->ctx_handle->ctx_ops->ra_ctx_query_qp_batch == NULL,
            hccp_err("[send][ra_qp]ctx_handle or ctx_ops or ra_ctx_query_qp_batch is NULL"), -EINVAL);
        
        ids[i] = qp_handle_tmp->id;
    }

    return 0;
}

HCCP_ATTRI_VISI_DEF int ra_ctx_qp_query_batch(void *qp_handle[], struct jetty_attr attr[], unsigned int *num)
{
    struct ra_ctx_qp_handle *qp_handle_tmp = NULL;
    unsigned int ids[HCCP_MAX_QP_QUERY_NUM] = {0};
    unsigned int phy_id, dev_index;
    int ret;

    CHK_PRT_RETURN(qp_handle == NULL || attr == NULL, hccp_err("[query][ra_qp]qp_handle or attr is NULL"),
        ConverReturnCode(RDMA_OP, -EINVAL));
    CHK_PRT_RETURN(num == NULL, hccp_err("num is NULL"), ConverReturnCode(RDMA_OP, -EINVAL));
    CHK_PRT_RETURN(*num == 0 || *num > HCCP_MAX_QP_QUERY_NUM, hccp_err("[query][ra_qp]num(%u) is out of range(0, %u]",
        *num, HCCP_MAX_QP_QUERY_NUM), ConverReturnCode(RDMA_OP, -EINVAL));

    qp_handle_tmp = (struct ra_ctx_qp_handle *)qp_handle[0];
    CHK_PRT_RETURN(qp_handle_tmp == NULL, hccp_err("[query][ra_qp]qp_handle[0] is NULL"), -EINVAL);
    phy_id = qp_handle_tmp->phy_id;
    dev_index = qp_handle_tmp->dev_index;

    ret = qp_query_batch_param_check(qp_handle, num, phy_id, ids);
    CHK_PRT_RETURN(ret != 0, hccp_err("[query][ra_qp]qp_query_batch_param_check failed, ret(%d)", ret),
        ConverReturnCode(RDMA_OP, ret));

    ret =  qp_handle_tmp->ctx_handle->ctx_ops->ra_ctx_query_qp_batch(phy_id, dev_index, ids, attr, num);
    CHK_PRT_RETURN(ret != 0, hccp_err("[query][ra_qp]query_qp_batch failed, ret(%d)", ret),
        ConverReturnCode(RDMA_OP, ret));

    return ConverReturnCode(RDMA_OP, ret);
}

HCCP_ATTRI_VISI_DEF int ra_ctx_qp_destroy(void *qp_handle)
{
    struct ra_ctx_qp_handle *qp_handle_tmp = NULL;
    int ret;

    CHK_PRT_RETURN(qp_handle == NULL, hccp_err("[deinit][ra_qp]qp_handle is NULL"),
        ConverReturnCode(RDMA_OP, -EINVAL));

    qp_handle_tmp = (struct ra_ctx_qp_handle *)qp_handle;
    CHK_PRT_RETURN(qp_handle_tmp->ctx_handle == NULL || qp_handle_tmp->ctx_handle->ctx_ops == NULL ||
        qp_handle_tmp->ctx_handle->ctx_ops->ra_ctx_qp_destroy == NULL, hccp_err("[deinit][ra_qp]ctx_handle or ctx_ops "
        "or ra_ctx_qp_destroy is NULL"), ConverReturnCode(RDMA_OP, -EINVAL));

    ret = qp_handle_tmp->ctx_handle->ctx_ops->ra_ctx_qp_destroy(qp_handle_tmp);
    if (ret == -ENODEV) {
        hccp_warn("[deinit][ra_qp]destroy unsuccessful, ret(%d) phy_id(%u) dev_index(%u) qp_id(%u)",
            ret, qp_handle_tmp->phy_id, qp_handle_tmp->dev_index, qp_handle_tmp->id);
        goto out;
    }
    if (ret != 0) {
        hccp_err("[deinit][ra_qp]destroy failed, ret(%d) phy_id(%u) dev_index(%u) qp_id(%u)",
            ret, qp_handle_tmp->phy_id, qp_handle_tmp->dev_index, qp_handle_tmp->id);
    }

out:
    free(qp_handle_tmp);
    qp_handle_tmp = NULL;
    return ConverReturnCode(RDMA_OP, ret);
}

HCCP_ATTRI_VISI_DEF int ra_ctx_qp_import(void *ctx_handle, struct qp_import_info_t *qp_info, void **rem_qp_handle)
{
    struct ra_ctx_rem_qp_handle *rem_qp_handle_tmp = NULL;
    struct ra_ctx_handle *ctx_handle_tmp = NULL;
    int ret;

    CHK_PRT_RETURN(ctx_handle == NULL || qp_info == NULL || rem_qp_handle == NULL,
        hccp_err("[init][ra_qp]ctx_handle or qp_info or rem_qp_handle is NULL"), ConverReturnCode(RDMA_OP, -EINVAL));

    ctx_handle_tmp = (struct ra_ctx_handle *)ctx_handle;
    CHK_PRT_RETURN(ctx_handle_tmp->ctx_ops == NULL || ctx_handle_tmp->ctx_ops->ra_ctx_qp_import == NULL,
        hccp_err("[init][ra_qp]ctx_ops or ra_ctx_qp_import is NULL"), ConverReturnCode(RDMA_OP, -EINVAL));

    rem_qp_handle_tmp = calloc(1, sizeof(struct ra_ctx_rem_qp_handle));
    CHK_PRT_RETURN(rem_qp_handle_tmp == NULL,
        hccp_err("[init][ra_qp]calloc rem_qp_handle_tmp failed, errno(%d) phy_id(%u) dev_index(%u)",
        errno, ctx_handle_tmp->attr.phy_id, ctx_handle_tmp->dev_index), ConverReturnCode(RDMA_OP, -ENOMEM));

    ret = ctx_handle_tmp->ctx_ops->ra_ctx_qp_import(ctx_handle_tmp, qp_info, rem_qp_handle_tmp);
    if (ret != 0) {
        hccp_err("[init][ra_qp]import failed, ret(%d) phy_id(%u) dev_index(%u)",
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

HCCP_ATTRI_VISI_DEF int ra_ctx_qp_unimport(void *ctx_handle, void *rem_qp_handle)
{
    struct ra_ctx_rem_qp_handle *rem_qp_handle_tmp = NULL;
    struct ra_ctx_handle *ctx_handle_tmp = NULL;
    int ret;

    CHK_PRT_RETURN(ctx_handle == NULL || rem_qp_handle == NULL,
        hccp_err("[deinit][ra_qp]ctx_handle or rem_qp_handle is NULL"), ConverReturnCode(RDMA_OP, -EINVAL));

    ctx_handle_tmp = (struct ra_ctx_handle *)ctx_handle;
    CHK_PRT_RETURN(ctx_handle_tmp->ctx_ops == NULL || ctx_handle_tmp->ctx_ops->ra_ctx_qp_unimport == NULL,
        hccp_err("[deinit][ra_qp]ctx_ops or ra_ctx_qp_unimport is NULL"),
        ConverReturnCode(RDMA_OP, -EINVAL));

    rem_qp_handle_tmp = (struct ra_ctx_rem_qp_handle *)rem_qp_handle;
    ret = ctx_handle_tmp->ctx_ops->ra_ctx_qp_unimport(rem_qp_handle_tmp);
    if (ret != 0) {
        hccp_err("[deinit][ra_qp]unimport failed, ret(%d) phy_id(%u) dev_index(%u) qp_id(%u)",
            ret, rem_qp_handle_tmp->phy_id, rem_qp_handle_tmp->dev_index, rem_qp_handle_tmp->id);
    }

    free(rem_qp_handle_tmp);
    rem_qp_handle_tmp = NULL;
    return ConverReturnCode(RDMA_OP, ret);
}

HCCP_ATTRI_VISI_DEF int ra_ctx_qp_bind(void *qp_handle, void *rem_qp_handle)
{
    struct ra_ctx_rem_qp_handle *rem_qp_handle_tmp = NULL;
    struct ra_ctx_qp_handle *qp_handle_tmp = NULL;
    int ret;

    CHK_PRT_RETURN(qp_handle == NULL || rem_qp_handle == NULL,
        hccp_err("[init][ra_qp]qp_handle or rem_qp_handle is NULL"), ConverReturnCode(RDMA_OP, -EINVAL));

    qp_handle_tmp = (struct ra_ctx_qp_handle *)qp_handle;
    CHK_PRT_RETURN(qp_handle_tmp->ctx_handle == NULL || qp_handle_tmp->ctx_handle->ctx_ops == NULL ||
        qp_handle_tmp->ctx_handle->ctx_ops->ra_ctx_qp_bind == NULL, hccp_err("[init][ra_qp]ctx_handle or ctx_ops "
        "or ra_ctx_qp_bind is NULL"), ConverReturnCode(RDMA_OP, -EINVAL));

    rem_qp_handle_tmp = (struct ra_ctx_rem_qp_handle *)rem_qp_handle;
    ret = qp_handle_tmp->ctx_handle->ctx_ops->ra_ctx_qp_bind(qp_handle_tmp, rem_qp_handle_tmp);
    CHK_PRT_RETURN(ret != 0, hccp_err("[init][ra_qp]bind failed, ret(%d) phy_id(%u) dev_index(%u) "
        "local_id(%u) remote_id(%u)", ret, qp_handle_tmp->ctx_handle->attr.phy_id, qp_handle_tmp->ctx_handle->dev_index,
        qp_handle_tmp->id, rem_qp_handle_tmp->id), ConverReturnCode(RDMA_OP, ret));

    return 0;
}

HCCP_ATTRI_VISI_DEF int ra_ctx_qp_unbind(void *qp_handle)
{
    struct ra_ctx_qp_handle *qp_handle_tmp = NULL;
    int ret;

    CHK_PRT_RETURN(qp_handle == NULL, hccp_err("[deinit][ra_qp]qp_handle is NULL"),
        ConverReturnCode(RDMA_OP, -EINVAL));

    qp_handle_tmp = (struct ra_ctx_qp_handle *)qp_handle;
    CHK_PRT_RETURN(qp_handle_tmp->ctx_handle == NULL || qp_handle_tmp->ctx_handle->ctx_ops == NULL ||
        qp_handle_tmp->ctx_handle->ctx_ops->ra_ctx_qp_unbind == NULL, hccp_err("[deinit][ra_qp]ctx_handle or ctx_ops "
        "or ra_ctx_qp_unbind is NULL"), ConverReturnCode(RDMA_OP, -EINVAL));

    ret = qp_handle_tmp->ctx_handle->ctx_ops->ra_ctx_qp_unbind(qp_handle_tmp);
    CHK_PRT_RETURN(ret == -ENODEV, hccp_warn("[deinit][ra_qp]unbind unsuccessful, ret(%d) phy_id(%u) dev_index(%u)",
        ret, qp_handle_tmp->ctx_handle->attr.phy_id, qp_handle_tmp->ctx_handle->dev_index),
        ConverReturnCode(RDMA_OP, ret));

    CHK_PRT_RETURN(ret != 0, hccp_err("[deinit][ra_qp]unbind failed, ret(%d) phy_id(%u) dev_index(%u)",
        ret, qp_handle_tmp->ctx_handle->attr.phy_id, qp_handle_tmp->ctx_handle->dev_index),
        ConverReturnCode(RDMA_OP, ret));
    return ConverReturnCode(RDMA_OP, ret);
}

STATIC int ra_ctx_batch_send_wr_check(struct ra_ctx_qp_handle *qp_handle, struct send_wr_data wr_list[],
    unsigned int num)
{
    enum ProtocolTypeT protocol = qp_handle->protocol;
    bool is_inline = false;
    unsigned int i, j;

    for (i = 0; i < num; i++) {
        // NOP opcode no need to check
        if (protocol == PROTOCOL_UDMA && wr_list[i].ub.opcode == RA_UB_OPC_NOP) {
            continue;
        }

        CHK_PRT_RETURN(wr_list[i].rmem_handle == NULL, hccp_err("[send][ra_qp]wr[%u] rmem_handle is NULL", i), -EINVAL);

        is_inline = ((protocol == PROTOCOL_RDMA && (wr_list[i].rdma.flags & RA_SEND_INLINE) != 0) ||
            (protocol == PROTOCOL_UDMA && wr_list[i].ub.flags.bs.inline_flag != 0));
        if (!is_inline) {
            CHK_PRT_RETURN((wr_list[i].sges == NULL), hccp_err("[send][ra_qp]wr[%u] sges is NULL", i), -EINVAL);

            for (j = 0; j < wr_list[i].num_sge; j++) {
                CHK_PRT_RETURN((wr_list[i].sges[j].lmem_handle == NULL),
                    hccp_err("[send][ra_qp]wr[%u] sges[%u] lmem_handle is NULL", i, j), -EINVAL);
            }
        } else if (wr_list[i].inline_data == NULL) {
            hccp_err("[send][ra_qp]wr[%u] inline_data is NULL", i);
            return -EINVAL;
        }

        if (protocol == PROTOCOL_UDMA) {
            if (wr_list[i].ub.rem_qp_handle == NULL ||
                (wr_list[i].ub.opcode == RA_UB_OPC_WRITE_NOTIFY && wr_list[i].ub.notify_info.notify_handle == NULL)) {
                hccp_err("[send][ra_qp]wr[%u] opcode[%d] rem_qp_handle or notify_handle is NULL",
                    i, wr_list[i].ub.opcode);
                return -EINVAL;
            }

            // checkreduce, only write & write with notify & read op supportreduce
            if ((wr_list[i].ub.opcode != RA_UB_OPC_WRITE && wr_list[i].ub.opcode != RA_UB_OPC_WRITE_NOTIFY
                && wr_list[i].ub.opcode != RA_UB_OPC_READ) && wr_list[i].ub.reduce_info.reduce_en) {
                hccp_err("[send][ra_qp]wr[%u] opcode[%d] not supportreduce", i, wr_list[i].ub.opcode);
                return -EINVAL;
            }
        }
    }

    return 0;
}

HCCP_ATTRI_VISI_DEF int ra_batch_send_wr(void *qp_handle, struct send_wr_data wr_list[], struct send_wr_resp op_resp[],
    unsigned int num, unsigned int *complete_num)
{
    struct ra_ctx_qp_handle *qp_handle_tmp = NULL;
    int ret;

    CHK_PRT_RETURN(qp_handle == NULL || wr_list == NULL || op_resp == NULL || num == 0 || complete_num == NULL,
        hccp_err("[send][ra_qp]qp_handle or wr_list or op_resp or complete_num is NULL, or num[%u] is 0", num),
        ConverReturnCode(RDMA_OP, -EINVAL));

    qp_handle_tmp = (struct ra_ctx_qp_handle *)qp_handle;
    CHK_PRT_RETURN(qp_handle_tmp->ctx_handle == NULL || qp_handle_tmp->ctx_handle->ctx_ops == NULL ||
        qp_handle_tmp->ctx_handle->ctx_ops->ra_ctx_batch_send_wr == NULL,
        hccp_err("[send][ra_qp]ctx_handle or ctx_ops or ra_ctx_batch_send_wr is NULL"),
        ConverReturnCode(RDMA_OP, -EINVAL));

    ret = ra_ctx_batch_send_wr_check(qp_handle_tmp, wr_list, num);
    CHK_PRT_RETURN(ret != 0, hccp_err("[send][ra_qp]check failed, ret(%d), protocol(%d) phy_id(%u), dev_index(%u)",
        ret, qp_handle_tmp->protocol, qp_handle_tmp->ctx_handle->attr.phy_id, qp_handle_tmp->ctx_handle->dev_index),
        ConverReturnCode(RDMA_OP, ret));

    ret = qp_handle_tmp->ctx_handle->ctx_ops->ra_ctx_batch_send_wr(qp_handle_tmp, wr_list, op_resp, num, complete_num);
    return ConverReturnCode(RDMA_OP, ret);
}

HCCP_ATTRI_VISI_DEF int ra_ctx_update_ci(void *qp_handle, uint16_t ci)
{
    struct ra_ctx_qp_handle *qp_handle_tmp = NULL;
    int ret;

    CHK_PRT_RETURN(qp_handle == NULL, hccp_err("[update][ra_qp]qp_handle is NULL"),
        ConverReturnCode(RDMA_OP, -EINVAL));

    qp_handle_tmp = (struct ra_ctx_qp_handle *)qp_handle;
    CHK_PRT_RETURN(qp_handle_tmp->ctx_handle == NULL || qp_handle_tmp->ctx_handle->ctx_ops == NULL ||
        qp_handle_tmp->ctx_handle->ctx_ops->ra_ctx_update_ci == NULL, hccp_err("[update][ra_qp]ctx_handle or ctx_ops "
        "or ra_ctx_update_ci is NULL"), ConverReturnCode(RDMA_OP, -EINVAL));

    ret = qp_handle_tmp->ctx_handle->ctx_ops->ra_ctx_update_ci(qp_handle_tmp, ci);
    return ConverReturnCode(RDMA_OP, ret);
}

HCCP_ATTRI_VISI_DEF int ra_custom_channel(struct RaInfo info, struct custom_chan_info_in *in,
    struct custom_chan_info_out *out)
{
    int ret = 0;

    CHK_PRT_RETURN(info.phyId >= RA_MAX_PHY_ID_NUM, hccp_err("[custom]phy_id(%u) must smaller than %u",
        info.phyId, RA_MAX_PHY_ID_NUM), ConverReturnCode(RDMA_OP, -EINVAL));
    CHK_PRT_RETURN(in == NULL || out == NULL, hccp_err("[custom]in or out is NULL"),
        ConverReturnCode(RDMA_OP, -EINVAL));

    if (info.mode == NETWORK_OFFLINE) {
        ret = ra_hdc_custom_channel(info.phyId, in, out);
        CHK_PRT_RETURN(ret != 0, hccp_err("[custom]ra_hdc_custom_channel failed, ret(%d) phy_id(%u)",
            ret, info.phyId), ConverReturnCode(RDMA_OP, ret));
    } else {
        hccp_err("[custom]mode(%d) do not support, phy_id(%u)", info.mode, info.phyId);
        return ConverReturnCode(RDMA_OP, -EINVAL);
    }

    return ret;
}

HCCP_ATTRI_VISI_DEF int ra_ctx_get_aux_info(void *ctx_handle, struct aux_info_in *in, struct aux_info_out *out)
{
    struct ra_ctx_handle *ctx_handle_tmp = NULL;
    int ret = 0;

    CHK_PRT_RETURN(ctx_handle == NULL || in == NULL || out == NULL,
        hccp_err("[get][aux_info]ctx_handle or in or out is NULL"), ConverReturnCode(RDMA_OP, -EINVAL));

    CHK_PRT_RETURN(in->type < AUX_INFO_IN_TYPE_CQE  || in->type >= AUX_INFO_IN_TYPE_MAX,
        hccp_err("[get][aux_info]in->type(%d) must greater or equal to %d and less than %d", in->type,
        AUX_INFO_IN_TYPE_CQE, AUX_INFO_IN_TYPE_MAX), ConverReturnCode(RDMA_OP, -EINVAL));

    ctx_handle_tmp = (struct ra_ctx_handle *)ctx_handle;
    CHK_PRT_RETURN(ctx_handle_tmp->ctx_ops == NULL || ctx_handle_tmp->ctx_ops->ra_ctx_get_aux_info == NULL,
        hccp_err("[get][aux_info]ctx_ops or ra_ctx_get_aux_info is NULL"), ConverReturnCode(RDMA_OP, -EINVAL));

    ret = ctx_handle_tmp->ctx_ops->ra_ctx_get_aux_info(ctx_handle, in, out);
    CHK_PRT_RETURN(ret != 0, hccp_err("[get][aux_info]ra_ctx_get_aux_info failed, ret(%d) phy_id(%u) dev_index(0x%x)",
        ret, ctx_handle_tmp->attr.phy_id, ctx_handle_tmp->dev_index), ConverReturnCode(RDMA_OP, ret));

    return ret;
}

HCCP_ATTRI_VISI_DEF int ra_ctx_get_cr_err_info_list(void *ctx_handle, struct CrErrInfo *info_list,
    unsigned int *num)
{
    struct ra_ctx_handle *ctx_handle_tmp = NULL;
    int ret;
 
    CHK_PRT_RETURN(ctx_handle == NULL || info_list == NULL || num == NULL,
        hccp_err("[get][cr_err_info_list]ctx_handle or info_list or num is NULL"), ConverReturnCode(RDMA_OP, -EINVAL));
 
    CHK_PRT_RETURN(*num == 0 || *num > CR_ERR_INFO_MAX_NUM, hccp_err("[get][cr_err_info_list]num:%u must greater "
        "than 0 and less or equal to %d", *num, CR_ERR_INFO_MAX_NUM), ConverReturnCode(RDMA_OP, -EINVAL));
 
    ctx_handle_tmp = (struct ra_ctx_handle *)ctx_handle;
 
    hccp_run_info("Input parameters: phy_id(%u), dev_index(0x%x) num(%u)",
        ctx_handle_tmp->attr.phy_id, ctx_handle_tmp->dev_index, *num);
 
    ret = ra_hdc_ctx_get_cr_err_info_list(ctx_handle_tmp, info_list, num);
    CHK_PRT_RETURN(ret != 0, hccp_err("[get][cr_err_info_list]ra_hdc_ctx_get_cr_err_info_list failed, ret:%d "
        "phy_id:%u dev_index:0x%x", ret, ctx_handle_tmp->attr.phy_id, ctx_handle_tmp->dev_index),
        ConverReturnCode(RDMA_OP, ret));
 
    return ConverReturnCode(RDMA_OP, ret);
}
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
#include "ra_rs_ctx.h"
#include "ra_peer.h"
#include "rs_ctx.h"
#include "ra_comm.h"
#include "ra_ctx_comm.h"
#include "ra_peer_ctx.h"

int ra_peer_get_dev_eid_info_num(struct RaInfo info, unsigned int *num)
{
    unsigned int phy_id = info.phyId;
    int ret = 0;

    RaPeerMutexLock(phy_id);
    RsSetCtx(phy_id);
    ret = rs_get_dev_eid_info_num(phy_id, num);
    if (ret != 0) {
        hccp_err("[get][eid]rs_get_dev_eid_info_num failed ret[%d], phy_id[%u]", ret, phy_id);
    }
    RaPeerMutexUnlock(phy_id);
    return ret;
}

int ra_peer_get_dev_eid_info_list(unsigned int phy_id, struct dev_eid_info info_list[], unsigned int *num)
{
    unsigned int start_index = 0;
    unsigned int count = *num;
    int ret = 0;

    RaPeerMutexLock(phy_id);
    RsSetCtx(phy_id);
    ret = rs_get_dev_eid_info_list(phy_id, info_list, start_index, count);
    if (ret != 0) {
        *num = 0;
        hccp_err("[get][eid]rs_get_dev_eid_info_list failed ret[%d], phy_id[%u]", ret, phy_id);
    } else {
        *num = count;
    }
    RaPeerMutexUnlock(phy_id);
    return ret;
}

int ra_peer_ctx_init(struct ra_ctx_handle *ctx_handle, struct ctx_init_attr *attr, unsigned int *dev_index,
    struct dev_base_attr *dev_base_attr)
{
    unsigned int phy_id = ctx_handle->attr.phy_id;
    int ret = 0;

    RaPeerMutexLock(phy_id);
    RsSetCtx(phy_id);
    ret = rs_ctx_init(attr, dev_index, dev_base_attr);
    if (ret != 0) {
        hccp_err("[init][ra_peer_ctx]ctx init failed[%d] phy_id[%u]", ret, phy_id);
    }

    RaPeerMutexUnlock(phy_id);
    return ret;
}

int ra_peer_ctx_get_async_events(struct ra_ctx_handle *ctx_handle, struct async_event events[], unsigned int *num)
{
    unsigned int phy_id = ctx_handle->attr.phy_id;
    struct RaRsDevInfo dev_info = {0};
    int ret = 0;

    ra_rs_set_dev_info(&dev_info, phy_id, ctx_handle->dev_index);

    RaPeerMutexLock(phy_id);
    RsSetCtx(phy_id);
    ret = rs_ctx_get_async_events(&dev_info, events, num);
    RaPeerMutexUnlock(phy_id);
    if (ret != 0) {
        hccp_err("[get][async_events]rs_ctx_get_async_events failed ret:%d phy_id:%u dev_index:0x%x", ret, phy_id,
            ctx_handle->dev_index);
    }

    return ret;
}

int ra_peer_ctx_deinit(struct ra_ctx_handle *ctx_handle)
{
    unsigned int phy_id = ctx_handle->attr.phy_id;
    struct RaRsDevInfo dev_info = {0};
    int ret = 0;

    ra_rs_set_dev_info(&dev_info, phy_id, ctx_handle->dev_index);

    RaPeerMutexLock(phy_id);
    RsSetCtx(phy_id);
    ret = rs_ctx_deinit(&dev_info);
    if (ret != 0) {
        hccp_err("[deinit][ra_peer_ctx]ctx deinit failed[%d] phy_id[%u]", ret, phy_id);
    }

    RaPeerMutexUnlock(phy_id);
    return ret;
}

int ra_peer_get_eid_by_ip(struct ra_ctx_handle *ctx_handle, struct IpInfo ip[], union hccp_eid eid[],
    unsigned int *num)
{
    unsigned int phy_id = ctx_handle->attr.phy_id;
    struct RaRsDevInfo dev_info = {0};
    int ret = 0;

    ra_rs_set_dev_info(&dev_info, phy_id, ctx_handle->dev_index);

    RaPeerMutexLock(phy_id);
    RsSetCtx(phy_id);
    ret = rs_get_eid_by_ip(&dev_info, ip, eid, num);
    RaPeerMutexUnlock(phy_id);
    if (ret != 0) {
        hccp_err("[get][eid_by_ip]rs_get_eid_by_ip failed ret[%d] phy_id[%u]", ret, phy_id);
    }

    return ret;
}

int ra_peer_ctx_token_id_alloc(struct ra_ctx_handle *ctx_handle, struct hccp_token_id *info,
    struct ra_token_id_handle *token_id_handle)
{
    unsigned int phy_id = ctx_handle->attr.phy_id;
    struct RaRsDevInfo dev_info = {0};
    int ret = 0;

    ra_rs_set_dev_info(&dev_info, phy_id, ctx_handle->dev_index);

    RaPeerMutexLock(phy_id);
    RsSetCtx(phy_id);
    ret = rs_ctx_token_id_alloc(&dev_info, &token_id_handle->addr, &info->token_id);
    RaPeerMutexUnlock(phy_id);
    if (ret != 0) {
        hccp_err("[init][ra_token_id]rs_ctx_token_id_alloc failed, ret[%d] phy_id[%u]", ret, phy_id);
    }

    return ret;
}

int ra_peer_ctx_token_id_free(struct ra_ctx_handle *ctx_handle, struct ra_token_id_handle *token_id_handle)
{
    unsigned int phy_id = ctx_handle->attr.phy_id;
    struct RaRsDevInfo dev_info = {0};
    int ret = 0;

    ra_rs_set_dev_info(&dev_info, phy_id, ctx_handle->dev_index);

    RaPeerMutexLock(phy_id);
    RsSetCtx(phy_id);
    ret = rs_ctx_token_id_free(&dev_info, token_id_handle->addr);
    RaPeerMutexUnlock(phy_id);
    if (ret != 0) {
        hccp_err("[deinit][ra_token_id]rs_ctx_token_id_free failed, ret[%d] phy_id[%u]", ret, phy_id);
    }

    return ret;
}

int ra_peer_ctx_lmem_register(struct ra_ctx_handle *ctx_handle, struct mr_reg_info_t *lmem_info,
    struct ra_lmem_handle *lmem_handle)
{
    unsigned int phy_id = ctx_handle->attr.phy_id;
    struct RaRsDevInfo dev_info = {0};
    struct mem_reg_attr_t mem_attr = {0};
    struct mem_reg_info_t mem_info = {0};
    int ret = 0;

    ra_rs_set_dev_info(&dev_info, phy_id, ctx_handle->dev_index);
    ret = ra_ctx_prepare_lmem_register(lmem_info, &mem_attr);
    CHK_PRT_RETURN(ret != 0, hccp_err("[init][ra_peer_lmem]ra_ctx_prepare_lmem_register failed, ret[%d]",
        ret), ret);

    RaPeerMutexLock(phy_id);
    RsSetCtx(phy_id);
    ret = rs_ctx_lmem_reg(&dev_info, &mem_attr, &mem_info);
    RaPeerMutexUnlock(phy_id);
    CHK_PRT_RETURN(ret != 0, hccp_err("[init][ra_peer_lmem]rs_ctx_lmem_reg failed, ret[%d] phy_id[%u]", ret, phy_id),
        ret);

    ra_ctx_get_lmem_info(&mem_info, lmem_info, lmem_handle);

    return ret;
}

int ra_peer_ctx_lmem_unregister(struct ra_ctx_handle *ctx_handle, struct ra_lmem_handle *lmem_handle)
{
    unsigned int phy_id = ctx_handle->attr.phy_id;
    struct RaRsDevInfo dev_info = {0};
    int ret = 0;

    ra_rs_set_dev_info(&dev_info, phy_id, ctx_handle->dev_index);

    RaPeerMutexLock(phy_id);
    RsSetCtx(phy_id);
    ret = rs_ctx_lmem_unreg(&dev_info, lmem_handle->addr);
    RaPeerMutexUnlock(phy_id);
    if (ret != 0) {
        hccp_err("[init][ra_peer_lmem]rs_ctx_lmem_unreg failed, ret[%d] phy_id[%u]", ret, phy_id);
    }

    return ret;
}

int ra_peer_ctx_rmem_import(struct ra_ctx_handle *ctx_handle, struct mr_import_info_t *rmem_info)
{
    unsigned int phy_id = ctx_handle->attr.phy_id;
    struct mem_import_attr_t mem_attr = {0};
    struct mem_import_info_t mem_info = {0};
    struct RaRsDevInfo dev_info = {0};
    int ret = 0;

    ra_rs_set_dev_info(&dev_info, phy_id, ctx_handle->dev_index);
    ra_ctx_prepare_rmem_import(rmem_info, &mem_attr);

    RaPeerMutexLock(phy_id);
    RsSetCtx(phy_id);
    ret = rs_ctx_rmem_import(&dev_info, &mem_attr, &mem_info);
    RaPeerMutexUnlock(phy_id);
    CHK_PRT_RETURN(ret != 0, hccp_err("[init][ra_peer_rmem]rs_ctx_rmem_import failed, ret[%d] phy_id[%u]", ret, phy_id),
        ret);

    rmem_info->out.ub.target_seg_handle = mem_info.ub.target_seg_handle;

    return ret;
}

int ra_peer_ctx_rmem_unimport(struct ra_ctx_handle *ctx_handle, struct ra_rmem_handle *rmem_handle)
{
    unsigned int phy_id = ctx_handle->attr.phy_id;
    struct RaRsDevInfo dev_info = {0};
    int ret = 0;

    ra_rs_set_dev_info(&dev_info, phy_id, ctx_handle->dev_index);

    RaPeerMutexLock(phy_id);
    RsSetCtx(phy_id);
    ret = rs_ctx_rmem_unimport(&dev_info, rmem_handle->addr);
    RaPeerMutexUnlock(phy_id);
    if (ret != 0) {
        hccp_err("[deinit][ra_peer_rmem]rs_ctx_rmem_unimport failed, ret[%d] phy_id[%u]", ret, phy_id);
    }

    return ret;
}

int ra_peer_ctx_chan_create(struct ra_ctx_handle *ctx_handle, struct chan_info_t *chan_info,
    struct ra_chan_handle *chan_handle)
{
    unsigned int phy_id = ctx_handle->attr.phy_id;
    struct RaRsDevInfo dev_info = {0};
    int ret = 0;

    ra_rs_set_dev_info(&dev_info, phy_id, ctx_handle->dev_index);

    RaPeerMutexLock(phy_id);
    RsSetCtx(phy_id);
    ret = rs_ctx_chan_create(&dev_info, chan_info->in.data_plane_flag, &chan_handle->addr, &chan_info->out.fd);
    RaPeerMutexUnlock(phy_id);
    if (ret != 0) {
        hccp_err("[init][ctx_chan]rs_ctx_chan_create failed, ret[%d] phy_id[%u]", ret, phy_id);
    }

    return ret;
}

int ra_peer_ctx_chan_destroy(struct ra_ctx_handle *ctx_handle, struct ra_chan_handle *chan_handle)
{
    unsigned int phy_id = ctx_handle->attr.phy_id;
    struct RaRsDevInfo dev_info = {0};
    int ret = 0;

    ra_rs_set_dev_info(&dev_info, phy_id, ctx_handle->dev_index);

    RaPeerMutexLock(phy_id);
    RsSetCtx(phy_id);
    ret = rs_ctx_chan_destroy(&dev_info, chan_handle->addr);
    RaPeerMutexUnlock(phy_id);
    if (ret != 0) {
        hccp_err("[deinit][ctx_chan]rs_ctx_chan_destroy failed, ret[%d] phy_id[%u]", ret, phy_id);
    }

    return ret;
}

int ra_peer_ctx_cq_create(struct ra_ctx_handle *ctx_handle, struct cq_info_t *info, struct ra_cq_handle *cq_handle)
{
    unsigned int phy_id = ctx_handle->attr.phy_id;
    struct RaRsDevInfo dev_info = {0};
    struct ctx_cq_attr cq_attr = {0};
    struct ctx_cq_info cq_info = {0};
    int ret = 0;

    CHK_PRT_RETURN(info->in.ub.mode != JFC_MODE_NORMAL, hccp_err("[init][ctx_cq]jfc_mode[%d] not support, phy_id[%u]",
        info->in.ub.mode, phy_id), -EINVAL);

    ra_rs_set_dev_info(&dev_info, phy_id, ctx_handle->dev_index);
    ra_ctx_prepare_cq_create(info, &cq_attr);

    RaPeerMutexLock(phy_id);
    RsSetCtx(phy_id);
    ret = rs_ctx_cq_create(&dev_info, &cq_attr, &cq_info);
    RaPeerMutexUnlock(phy_id);
    CHK_PRT_RETURN(ret != 0, hccp_err("[init][ctx_cq]rs_ctx_cq_create failed, ret[%d] phy_id[%u]", ret, phy_id), ret);

    cq_handle->addr = cq_info.addr;
    ra_ctx_get_cq_create_info(&cq_info, info);
    return ret;
}

int ra_peer_ctx_cq_destroy(struct ra_ctx_handle *ctx_handle, struct ra_cq_handle *cq_handle)
{
    unsigned int phy_id = ctx_handle->attr.phy_id;
    struct RaRsDevInfo dev_info = {0};
    int ret = 0;

    ra_rs_set_dev_info(&dev_info, phy_id, ctx_handle->dev_index);

    RaPeerMutexLock(phy_id);
    RsSetCtx(phy_id);
    ret = rs_ctx_cq_destroy(&dev_info, cq_handle->addr);
    RaPeerMutexUnlock(phy_id);
    if (ret != 0) {
        hccp_err("[deinit][ctx_cq]rs_ctx_cq_destroy failed, ret[%d] phy_id[%u]", ret, phy_id);
    }

    return ret;
}

int ra_peer_ctx_qp_create(struct ra_ctx_handle *ctx_handle, struct qp_create_attr *qp_attr,
    struct qp_create_info *qp_info, struct ra_ctx_qp_handle *qp_handle)
{
    unsigned int dev_index = ctx_handle->dev_index;
    unsigned int phy_id = ctx_handle->attr.phy_id;
    struct RaRsDevInfo dev_info = {0};
    struct ctx_qp_attr ctx_qp_attr = {0};
    int ret = 0;

    CHK_PRT_RETURN(qp_attr->ub.mode != JETTY_MODE_URMA_NORMAL, hccp_err("[init][ctx_cq]jetty_mode[%d] not support,"
        " phy_id[%u]", qp_attr->ub.mode, phy_id), -EINVAL);

    ra_rs_set_dev_info(&dev_info, phy_id, ctx_handle->dev_index);
    ret = ra_ctx_prepare_qp_create(qp_attr, &ctx_qp_attr);
    CHK_PRT_RETURN(ret, hccp_err("[init][ra_peer_qp]ra_ctx_prepare_qp_create failed ret[%d], phy_id[%u] dev_index[%u]",
        ret, phy_id, dev_index), ret);

    RaPeerMutexLock(phy_id);
    RsSetCtx(phy_id);
    ret = rs_ctx_qp_create(&dev_info, &ctx_qp_attr, qp_info);
    RaPeerMutexUnlock(phy_id);
    CHK_PRT_RETURN(ret != 0, hccp_err("[init][ra_peer_qp]rs_ctx_qp_create failed, ret[%d] phy_id[%u]",
        ret, phy_id), ret);

    ra_ctx_get_qp_create_info(ctx_handle, qp_attr, qp_info, qp_handle);

    return ret;
}

int ra_peer_ctx_qp_destroy(struct ra_ctx_qp_handle *qp_handle)
{
    unsigned int phy_id = qp_handle->phy_id;
    struct RaRsDevInfo dev_info = {0};
    int ret = 0;

    ra_rs_set_dev_info(&dev_info, phy_id, qp_handle->dev_index);

    RaPeerMutexLock(phy_id);
    RsSetCtx(phy_id);
    ret = rs_ctx_qp_destroy(&dev_info, qp_handle->id);
    RaPeerMutexUnlock(phy_id);
    if (ret != 0) {
        hccp_err("[deinit][ra_peer_qp]rs_ctx_qp_destroy failed, ret[%d] phy_id[%u]", ret, phy_id);
    }

    return ret;
}

STATIC void ra_peer_prepare_qp_import(struct qp_import_info_t *qp_info, struct rs_jetty_import_attr *import_attr)
{
    struct ra_rs_jetty_import_attr *ra_rs_import_attr = NULL;

    ra_rs_import_attr = &(import_attr->attr);
    import_attr->key = qp_info->in.key;
    ra_ctx_prepare_qp_import(qp_info, ra_rs_import_attr);
}

STATIC void ra_peer_get_qp_import_info(struct ra_ctx_handle *ctx_handle, struct qp_import_info_t *qp_info,
    struct rs_jetty_import_info *import_info, struct ra_ctx_rem_qp_handle *qp_handle)
{
    struct ra_rs_jetty_import_info *ra_rs_import_info = NULL;

    ra_rs_import_info = &(import_info->info);
    ra_ctx_get_qp_import_info(ctx_handle, qp_info, ra_rs_import_info, qp_handle);
    qp_handle->id = import_info->rem_jetty_id;
}

int ra_peer_ctx_qp_import(struct ra_ctx_handle *ctx_handle, struct qp_import_info_t *qp_info,
    struct ra_ctx_rem_qp_handle *rem_qp_handle)
{
    unsigned int phy_id = ctx_handle->attr.phy_id;
    struct rs_jetty_import_attr import_attr = {0};
    struct rs_jetty_import_info import_info = {0};
    struct RaRsDevInfo dev_info = {0};
    int ret = 0;

    ra_rs_set_dev_info(&dev_info, phy_id, ctx_handle->dev_index);
    ra_peer_prepare_qp_import(qp_info, &import_attr);

    RaPeerMutexLock(phy_id);
    RsSetCtx(phy_id);
    ret = rs_ctx_qp_import(&dev_info, &import_attr, &import_info);
    RaPeerMutexUnlock(phy_id);
    CHK_PRT_RETURN(ret != 0, hccp_err("[init][ra_peer_qp]rs_ctx_qp_import failed, ret[%d] phy_id[%u]", ret, phy_id),
        ret);

    ra_peer_get_qp_import_info(ctx_handle, qp_info, &import_info, rem_qp_handle);
    return ret;
}

int ra_peer_ctx_qp_unimport(struct ra_ctx_rem_qp_handle *rem_qp_handle)
{
    unsigned int phy_id = rem_qp_handle->phy_id;
    struct RaRsDevInfo dev_info = {0};
    int ret = 0;

    ra_rs_set_dev_info(&dev_info, phy_id, rem_qp_handle->dev_index);

    RaPeerMutexLock(phy_id);
    RsSetCtx(phy_id);
    ret = rs_ctx_qp_unimport(&dev_info, rem_qp_handle->id);
    RaPeerMutexUnlock(phy_id);
    CHK_PRT_RETURN(ret != 0, hccp_err("[deinit][ra_peer_qp]rs_ctx_qp_unimport failed, ret[%d] phy_id[%u]", ret, phy_id),
        ret);

    return ret;
}

int ra_peer_ctx_qp_bind(struct ra_ctx_qp_handle *qp_handle, struct ra_ctx_rem_qp_handle *rem_qp_handle)
{
    struct rs_ctx_qp_info remote_qp_info = {0};
    struct rs_ctx_qp_info local_qp_info = {0};
    unsigned int phy_id = qp_handle->phy_id;
    struct RaRsDevInfo dev_info = {0};
    int ret = 0;

    ra_rs_set_dev_info(&dev_info, phy_id, rem_qp_handle->dev_index);
    local_qp_info.id = qp_handle->id;
    remote_qp_info.id = rem_qp_handle->id;

    RaPeerMutexLock(phy_id);
    RsSetCtx(phy_id);
    ret = rs_ctx_qp_bind(&dev_info, &local_qp_info, &remote_qp_info);
    RaPeerMutexUnlock(phy_id);
    CHK_PRT_RETURN(ret != 0, hccp_err("[init][ra_peer_qp]rs_ctx_qp_bind failed, ret[%d] phy_id[%u]", ret, phy_id), ret);

    return ret;
}

int ra_peer_ctx_qp_unbind(struct ra_ctx_qp_handle *qp_handle)
{
    unsigned int phy_id = qp_handle->phy_id;
    struct RaRsDevInfo dev_info = {0};
    int ret = 0;

    ra_rs_set_dev_info(&dev_info, phy_id, qp_handle->dev_index);

    RaPeerMutexLock(phy_id);
    RsSetCtx(phy_id);
    ret = rs_ctx_qp_unbind(&dev_info, qp_handle->id);
    RaPeerMutexUnlock(phy_id);
    CHK_PRT_RETURN(ret != 0, hccp_err("[deinit][ra_peer_qp]rs_ctx_qp_unbind failed, ret[%d] phy_id[%u]", ret, phy_id),
        ret);

    return ret;
}

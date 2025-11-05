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
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include "securec.h"
#include "user_log.h"
#include "dl_hal_function.h"
#include "hccp.h"
#include "ra.h"
#include "ra_comm.h"
#include "ra_ctx.h"
#include "ra_rs_err.h"
#include "ra_hdc.h"
#include "ra_hdc_ctx.h"

int ra_hdc_get_dev_eid_info_num(struct ra_info info, unsigned int *num)
{
    union op_get_dev_eid_info_num_data op_data = {0};
    int ret;

    *num = 0;
    op_data.tx_data.phy_id = info.phy_id;
    ret = ra_hdc_process_msg(RA_RS_GET_DEV_EID_INFO_NUM, info.phy_id, (char *)&op_data,
        sizeof(union op_get_dev_eid_info_num_data));
    CHK_PRT_RETURN(ret != 0, hccp_err("[get][eid]hdc message process failed ret[%d], phy_id[%u]",
        ret, info.phy_id), ret);

    *num = op_data.rx_data.num;
    return 0;
}

STATIC int ra_hdc_get_dev_eid_sub_info_list(unsigned int phy_id, struct dev_eid_info info_list[],
    unsigned int start_index, unsigned int count)
{
    union op_get_dev_eid_info_list_data op_data = {0};
    int ret;

    op_data.tx_data.phy_id = phy_id;
    op_data.tx_data.start_index = start_index;
    op_data.tx_data.count = count;
    ret = ra_hdc_process_msg(RA_RS_GET_DEV_EID_INFO_LIST, phy_id, (char *)&op_data,
        sizeof(union op_get_dev_eid_info_list_data));
    CHK_PRT_RETURN(ret, hccp_err("[get][eid]hdc message process failed ret[%d], phy_id[%u]", ret, phy_id), ret);

    (void)memcpy_s(info_list, sizeof(struct dev_eid_info) * MAX_DEV_INFO_TRANS_NUM,
        op_data.rx_data.info_list, sizeof(struct dev_eid_info) * MAX_DEV_INFO_TRANS_NUM);
    return 0;
}

int ra_hdc_get_dev_eid_info_list(unsigned int phy_id, struct dev_eid_info info_list[], unsigned int *num)
{
    struct dev_eid_info sub_info_list[MAX_DEV_INFO_TRANS_NUM] = {0};
    unsigned int info_size = *num;
    unsigned int remain_count = 0;
    unsigned int start_index = 0;
    int ret = 0;

    *num = 0;
    // get MAX_DEV_INFO_TRANS_NUM num of eid info every time: will fallthrough here
    for (start_index = 0; start_index + MAX_DEV_INFO_TRANS_NUM <= info_size; start_index += MAX_DEV_INFO_TRANS_NUM) {
        ret = ra_hdc_get_dev_eid_sub_info_list(phy_id, sub_info_list, start_index, MAX_DEV_INFO_TRANS_NUM);
        CHK_PRT_RETURN(ret != 0, hccp_err("[get][eid]get sub_info_list failed, ret(%d) phy_id(%u) "
            "start_index(%u) count(%d)", ret, phy_id, start_index, MAX_DEV_INFO_TRANS_NUM), ret);

        *num += MAX_DEV_INFO_TRANS_NUM;
        (void)memcpy_s(info_list + start_index, sizeof(struct dev_eid_info) * MAX_DEV_INFO_TRANS_NUM,
            sub_info_list, sizeof(struct dev_eid_info) * MAX_DEV_INFO_TRANS_NUM);
    }

    remain_count = info_size % MAX_DEV_INFO_TRANS_NUM;
    if (remain_count == 0) {
        return ret;
    }

    // get remain count of eid info
    ret = ra_hdc_get_dev_eid_sub_info_list(phy_id, sub_info_list, start_index, remain_count);
    CHK_PRT_RETURN(ret != 0, hccp_err("[get][eid]get sub_info_list failed, ret(%d) phy_id(%u) "
        "start_index(%u) remain_count(%u)", ret, phy_id, start_index, remain_count), ret);

    *num += remain_count;
    (void)memcpy_s(info_list + start_index, sizeof(struct dev_eid_info) * remain_count,
        sub_info_list, sizeof(struct dev_eid_info) * remain_count);
    return ret;
}

int ra_hdc_ctx_init(struct ra_ctx_handle *ctx_handle, struct ctx_init_attr *attr, unsigned int *dev_index,
    struct dev_base_attr *dev_attr)
{
    union op_ctx_init_data op_data = {0};
    int ret;

    CHK_PRT_RETURN(ctx_handle->protocol != PROTOCOL_UDMA, hccp_err("[init][ra_hdc_ctx]protocol:%d != PROTOCOL_UDMA "
        "not support, phy_id[%u]", ctx_handle->protocol, attr->phy_id), -EINVAL);

    (void)memcpy_s(&(op_data.tx_data.attr), sizeof(struct ctx_init_attr), attr, sizeof(struct ctx_init_attr));
    ret = ra_hdc_process_msg(RA_RS_CTX_INIT, attr->phy_id, (char *)&op_data, sizeof(union op_ctx_init_data));
    CHK_PRT_RETURN(ret != 0, hccp_err("[init][ra_hdc_ctx]hdc message process failed ret[%d], phy_id[%u]",
        ret, attr->phy_id), ret);

    *dev_index = op_data.rx_data.dev_index;
    (void)memcpy_s(dev_attr, sizeof(struct dev_base_attr), &op_data.rx_data.dev_attr, sizeof(struct dev_base_attr));

    return 0;
}

int ra_hdc_ctx_deinit(struct ra_ctx_handle *ctx_handle)
{
    unsigned int phy_id = ctx_handle->attr.phy_id;
    union op_ctx_deinit_data op_data = {0};
    int ret;

    op_data.tx_data.phy_id = phy_id;
    op_data.tx_data.dev_index = ctx_handle->dev_index;
    ret = ra_hdc_process_msg(RA_RS_CTX_DEINIT, phy_id, (char *)&op_data, sizeof(union op_ctx_deinit_data));
    CHK_PRT_RETURN(ret, hccp_err("[deinit][ra_hdc_ctx]hdc message process failed ret[%d], phy_id[%u] dev_index[%u]",
        ret, phy_id, ctx_handle->dev_index), ret);

    return 0;
}

int ra_hdc_ctx_token_id_alloc(struct ra_ctx_handle *ctx_handle, struct hccp_token_id *info,
    struct ra_token_id_handle *token_id_handle)
{
    unsigned int phy_id = ctx_handle->attr.phy_id;
    union op_token_id_alloc_data op_data = {0};
    int ret;

    CHK_PRT_RETURN(ctx_handle->protocol != PROTOCOL_UDMA,
        hccp_err("[init][ra_token_id]unsupported protocol[%d], phy_id[%u], dev_index[%u]",
        ctx_handle->protocol, phy_id, ctx_handle->dev_index), -EINVAL);

    op_data.tx_data.phy_id = phy_id;
    op_data.tx_data.dev_index = ctx_handle->dev_index;

    ret = ra_hdc_process_msg(RA_RS_CTX_TOKEN_ID_ALLOC, phy_id, (char *)&op_data,
        sizeof(union op_token_id_alloc_data));
    CHK_PRT_RETURN(ret != 0, hccp_err("[init][ra_token_id]hdc message process failed ret[%d], phy_id[%u] "
        "dev_index[%u]", ret, phy_id, ctx_handle->dev_index), ret);

    info->token_id = op_data.rx_data.token_id;
    token_id_handle->addr = op_data.rx_data.addr;
    return 0;
}

int ra_hdc_ctx_token_id_free(struct ra_ctx_handle *ctx_handle, struct ra_token_id_handle *token_id_handle)
{
    unsigned int phy_id = ctx_handle->attr.phy_id;
    union op_token_id_free_data op_data = {0};
    int ret;

    op_data.tx_data.phy_id = phy_id;
    op_data.tx_data.dev_index = ctx_handle->dev_index;
    op_data.tx_data.addr = token_id_handle->addr;
    ret = ra_hdc_process_msg(RA_RS_CTX_TOKEN_ID_FREE, phy_id, (char *)&op_data,
        sizeof(union op_token_id_free_data));
    CHK_PRT_RETURN(ret != 0, hccp_err("[deinit][ra_token_id]hdc message process failed ret[%d], phy_id[%u] "
        "dev_index[%u]", ret, phy_id, ctx_handle->dev_index), ret);

    return 0;
}

int ra_hdc_ctx_prepare_lmem_register(struct ra_ctx_handle *ctx_handle, struct mr_reg_info_t *lmem_info,
    union op_lmem_reg_info_data *op_data)
{
    struct ra_token_id_handle *token_id_handle = NULL;
    bool is_token_id_valid = false;

    op_data->tx_data.phy_id = ctx_handle->attr.phy_id;
    op_data->tx_data.dev_index = ctx_handle->dev_index;
    op_data->tx_data.mem_attr.mem = lmem_info->in.mem;
    if (ctx_handle->protocol == PROTOCOL_RDMA) {
        op_data->tx_data.mem_attr.rdma.access = lmem_info->in.rdma.access;
    } else { // protocol == PROTOCOL_UDMA
        is_token_id_valid = lmem_info->in.ub.flags.bs.token_id_valid == 1;
        CHK_PRT_RETURN(is_token_id_valid && lmem_info->in.ub.token_id_handle == NULL,
            hccp_err("[init][ra_hdc_lmem]lmem_info specify token id, but token_id_handle is NULL"), -EINVAL);

        op_data->tx_data.mem_attr.ub.flags = lmem_info->in.ub.flags;
        op_data->tx_data.mem_attr.ub.token_value = lmem_info->in.ub.token_value;
        token_id_handle = (struct ra_token_id_handle *)(lmem_info->in.ub.token_id_handle);
        op_data->tx_data.mem_attr.ub.token_id_addr = is_token_id_valid ? token_id_handle->addr : 0;
    }

    return 0;
}

int ra_hdc_ctx_lmem_register(struct ra_ctx_handle *ctx_handle, struct mr_reg_info_t *lmem_info,
    struct ra_lmem_handle *lmem_handle)
{
    unsigned int phy_id = ctx_handle->attr.phy_id;
    union op_lmem_reg_info_data op_data = {0};
    int ret;

    ret = ra_hdc_ctx_prepare_lmem_register(ctx_handle, lmem_info, &op_data);
    CHK_PRT_RETURN(ret != 0, hccp_err("[init][ra_hdc_lmem]prepare register failed ret[%d], phy_id[%u] dev_index[%u]",
        ret, phy_id, ctx_handle->dev_index), ret);

    ret = ra_hdc_process_msg(RA_RS_LMEM_REG, phy_id, (char *)&op_data, sizeof(union op_lmem_reg_info_data));
    CHK_PRT_RETURN(ret, hccp_err("[init][ra_hdc_lmem]hdc message process failed ret[%d], phy_id[%u] dev_index[%u]",
        ret, phy_id, ctx_handle->dev_index), ret);

    lmem_info->out.key = op_data.rx_data.mem_info.key;
    if (ctx_handle->protocol == PROTOCOL_RDMA) {
        lmem_info->out.rdma.lkey = op_data.rx_data.mem_info.rdma.lkey;
    } else { // protocol == PROTOCOL_UDMA
        lmem_info->out.ub.token_id = op_data.rx_data.mem_info.ub.token_id;
        lmem_info->out.ub.target_seg_handle = op_data.rx_data.mem_info.ub.target_seg_handle;
        lmem_handle->addr = lmem_info->out.ub.target_seg_handle;
    }

    return 0;
}

int ra_hdc_ctx_lmem_unregister(struct ra_ctx_handle *ctx_handle, struct ra_lmem_handle *lmem_handle)
{
    unsigned int phy_id = ctx_handle->attr.phy_id;
    union op_lmem_unreg_info_data op_data = {0};
    int ret;

    op_data.tx_data.phy_id = phy_id;
    op_data.tx_data.dev_index = ctx_handle->dev_index;
    op_data.tx_data.addr = lmem_handle->addr;
    ret = ra_hdc_process_msg(RA_RS_LMEM_UNREG, phy_id, (char *)&op_data, sizeof(union op_lmem_unreg_info_data));
    CHK_PRT_RETURN(ret, hccp_err("[deinit][ra_hdc_lmem]hdc message process failed ret[%d], phy_id[%u] dev_index[%u]",
        ret, phy_id, ctx_handle->dev_index), ret);

    return 0;
}

int ra_hdc_ctx_rmem_import(struct ra_ctx_handle *ctx_handle, struct mr_import_info_t *rmem_info)
{
    unsigned int phy_id = ctx_handle->attr.phy_id;
    union op_rmem_import_info_data op_data = {0};
    int ret;

    // protocol RDMA only need to save rkey
    if (ctx_handle->protocol == PROTOCOL_RDMA) {
        ret = memcpy_s(&rmem_info->out.rdma.rkey, sizeof(unsigned int),
            &rmem_info->in.key.value, rmem_info->in.key.size);
        CHK_PRT_RETURN(ret != 0, hccp_err("[init][ra_hdc_rmem]memcpy_s rkey failed ret[%d], phy_id[%u] dev_index[%u]",
            ret, phy_id, ctx_handle->dev_index), -ESAFEFUNC);
        return 0;
    }

    // protocol UDMA need to import rmem
    op_data.tx_data.phy_id = phy_id;
    op_data.tx_data.dev_index = ctx_handle->dev_index;
    op_data.tx_data.mem_attr.key = rmem_info->in.key;
    op_data.tx_data.mem_attr.ub.flags = rmem_info->in.ub.flags;
    op_data.tx_data.mem_attr.ub.mapping_addr = rmem_info->in.ub.mapping_addr;
    op_data.tx_data.mem_attr.ub.token_value = rmem_info->in.ub.token_value;
    ret = ra_hdc_process_msg(RA_RS_RMEM_IMPORT, phy_id, (char *)&op_data, sizeof(union op_rmem_import_info_data));
    CHK_PRT_RETURN(ret, hccp_err("[init][ra_hdc_rmem]hdc message process failed ret[%d], phy_id[%u] dev_index[%u]",
        ret, phy_id, ctx_handle->dev_index), ret);

    rmem_info->out.ub.target_seg_handle = op_data.rx_data.mem_info.ub.target_seg_handle;
    return 0;
}

int ra_hdc_ctx_rmem_unimport(struct ra_ctx_handle *ctx_handle, struct ra_rmem_handle *rmem_handle)
{
    union op_rmem_unimport_info_data op_data = {0};
    unsigned int phy_id = ctx_handle->attr.phy_id;
    int ret;

    op_data.tx_data.phy_id = phy_id;
    op_data.tx_data.dev_index = ctx_handle->dev_index;
    op_data.tx_data.addr = rmem_handle->addr;
    ret = ra_hdc_process_msg(RA_RS_RMEM_UNIMPORT, phy_id, (char *)&op_data, sizeof(union op_rmem_unimport_info_data));
    CHK_PRT_RETURN(ret, hccp_err("[deinit][ra_hdc_rmem]hdc message process failed ret[%d], phy_id[%u], dev_index[%u]",
        ret, phy_id, ctx_handle->dev_index), ret);

    return 0;
}

int ra_hdc_ctx_chan_create(struct ra_ctx_handle *ctx_handle, struct ra_chan_handle *chan_handle)
{
    unsigned int phy_id = ctx_handle->attr.phy_id;
    union op_ctx_chan_create_data op_data = {0};
    int ret;

    op_data.tx_data.phy_id = phy_id;
    op_data.tx_data.dev_index = ctx_handle->dev_index;
    ret = ra_hdc_process_msg(RA_RS_CTX_CHAN_CREATE, phy_id, (char *)&op_data, sizeof(union op_ctx_chan_create_data));
    CHK_PRT_RETURN(ret, hccp_err("[init][ctx_chan]hdc message process failed ret[%d], phy_id[%u], dev_index[%u]",
        ret, phy_id, ctx_handle->dev_index), ret);

    chan_handle->addr = op_data.rx_data.addr;
    return 0;
}

int ra_hdc_ctx_chan_destroy(struct ra_ctx_handle *ctx_handle, struct ra_chan_handle *chan_handle)
{
    unsigned int phy_id = ctx_handle->attr.phy_id;
    union op_ctx_chan_destroy_data op_data = {0};
    int ret;

    op_data.tx_data.phy_id = phy_id;
    op_data.tx_data.dev_index = ctx_handle->dev_index;
    op_data.tx_data.addr = chan_handle->addr;

    ret = ra_hdc_process_msg(RA_RS_CTX_CHAN_DESTROY, phy_id, (char *)&op_data, sizeof(union op_ctx_chan_destroy_data));
    CHK_PRT_RETURN(ret, hccp_err("[deinit][ctx_chan]hdc message process failed ret[%d], phy_id[%u], dev_index[%u]",
        ret, phy_id, ctx_handle->dev_index), ret);

    return 0;
}

int ra_hdc_ctx_cq_create(struct ra_ctx_handle *ctx_handle, struct cq_info_t *info, struct ra_cq_handle *cq_handle)
{
    unsigned int phy_id = ctx_handle->attr.phy_id;
    union op_ctx_cq_create_data op_data = {0};
    int ret;

    op_data.tx_data.phy_id = phy_id;
    op_data.tx_data.dev_index = ctx_handle->dev_index;
    op_data.tx_data.attr.depth = info->in.depth;
    if (ctx_handle->protocol == PROTOCOL_RDMA) {
        op_data.tx_data.attr.rdma.cq_context = info->in.rdma.cq_context;
        op_data.tx_data.attr.rdma.mode = info->in.rdma.mode;
        op_data.tx_data.attr.rdma.comp_vector = info->in.rdma.comp_vector;
    } else { // protocol == PROTOCOL_UDMA
        op_data.tx_data.attr.ub.user_ctx = info->in.ub.user_ctx;
        op_data.tx_data.attr.ub.mode = info->in.ub.mode;
        op_data.tx_data.attr.ub.ceqn = info->in.ub.ceqn;
        op_data.tx_data.attr.ub.flag = info->in.ub.flag;
        if (op_data.tx_data.attr.ub.mode == JFC_MODE_CCU_POLL && info->in.ub.ccu_ex_cfg.valid) {
            op_data.tx_data.attr.ub.ccu_ex_cfg.valid = info->in.ub.ccu_ex_cfg.valid;
            op_data.tx_data.attr.ub.ccu_ex_cfg.cqe_flag = info->in.ub.ccu_ex_cfg.cqe_flag;
        }
    }

    ret = ra_hdc_process_msg(RA_RS_CTX_CQ_CREATE, phy_id, (char *)&op_data, sizeof(union op_ctx_cq_create_data));
    CHK_PRT_RETURN(ret, hccp_err("[init][ra_cq]hdc message process failed ret[%d], phy_id[%u], dev_index[%u]",
        ret, phy_id, ctx_handle->dev_index), ret);

    cq_handle->addr = op_data.rx_data.info.addr;
    return 0;
}

int ra_hdc_ctx_cq_destroy(struct ra_ctx_handle *ctx_handle, struct ra_cq_handle *cq_handle)
{
    unsigned int phy_id = ctx_handle->attr.phy_id;
    union op_ctx_cq_destroy_data op_data = {0};
    int ret;

    op_data.tx_data.phy_id = phy_id;
    op_data.tx_data.dev_index = ctx_handle->dev_index;
    op_data.tx_data.addr = cq_handle->addr;
    ret = ra_hdc_process_msg(RA_RS_CTX_CQ_DESTROY, phy_id, (char *)&op_data, sizeof(union op_ctx_cq_destroy_data));
    CHK_PRT_RETURN(ret, hccp_err("[deinit][ra_cq]hdc message process failed ret[%d], phy_id[%u], dev_index[%u]",
        ret, phy_id, ctx_handle->dev_index), ret);

    return 0;
}

STATIC int ra_hdc_get_qp_create_info(struct ra_ctx_handle *ctx_handle, struct qp_create_attr *qp_attr,
    union op_ctx_qp_create_data *op_data, struct qp_create_info *qp_info, struct ra_ctx_qp_handle *qp_handle)
{
    qp_handle->dev_index = ctx_handle->dev_index;
    qp_handle->phy_id = ctx_handle->attr.phy_id;
    qp_handle->protocol = ctx_handle->protocol;
    qp_handle->ctx_handle = ctx_handle;
    if (qp_handle->protocol == PROTOCOL_RDMA) {
        qp_handle->id = op_data->rx_data.qp_info.rdma.qpn;
    } else {
        // qp_handle->protocol == PROTOCOL_UDMA
        qp_handle->id = op_data->rx_data.qp_info.ub.id;
    }
    (void)memcpy_s(&qp_handle->qp_attr, sizeof(struct qp_create_attr), qp_attr, sizeof(struct qp_create_attr));
    (void)memcpy_s(&qp_handle->qp_info, sizeof(struct qp_create_info), &op_data->rx_data.qp_info,
        sizeof(struct qp_create_info));
    (void)memcpy_s(qp_info, sizeof(struct qp_create_info), &op_data->rx_data.qp_info, sizeof(struct qp_create_info));
    return 0;
}

int ra_hdc_ctx_prepare_qp_create(struct ra_ctx_handle *ctx_handle, struct qp_create_attr *qp_attr,
    union op_ctx_qp_create_data *op_data)
{
    struct ra_token_id_handle *token_id_handle = NULL;
    enum protocol_type protocol = ctx_handle->protocol;
    struct ctx_qp_attr *ctx_qp_attr = NULL;

    CHK_PRT_RETURN(qp_attr->scq_handle == NULL, hccp_err("[init][ra_hdc_qp]scq_handle is NULL"), -EINVAL);
    CHK_PRT_RETURN(qp_attr->rcq_handle == NULL, hccp_err("[init][ra_hdc_qp]rcq_handle is NULL"), -EINVAL);

    op_data->tx_data.phy_id = ctx_handle->attr.phy_id;
    op_data->tx_data.dev_index = ctx_handle->dev_index;
    ctx_qp_attr = &op_data->tx_data.qp_attr;
    ctx_qp_attr->scq_index = ((struct ra_cq_handle *)qp_attr->scq_handle)->addr;
    ctx_qp_attr->rcq_index = ((struct ra_cq_handle *)qp_attr->rcq_handle)->addr;
    ctx_qp_attr->srq_index = 0;
    ctx_qp_attr->sq_depth = qp_attr->sq_depth;
    ctx_qp_attr->rq_depth = qp_attr->rq_depth;
    ctx_qp_attr->transport_mode = qp_attr->transport_mode;
    if (protocol == PROTOCOL_RDMA) {
        ctx_qp_attr->rdma.mode = qp_attr->rdma.mode;
        ctx_qp_attr->rdma.udp_sport = qp_attr->rdma.udp_sport;
        ctx_qp_attr->rdma.traffic_class = qp_attr->rdma.traffic_class;
        ctx_qp_attr->rdma.sl = qp_attr->rdma.sl;
        ctx_qp_attr->rdma.timeout = qp_attr->rdma.timeout;
        ctx_qp_attr->rdma.rnr_retry = qp_attr->rdma.rnr_retry;
        ctx_qp_attr->rdma.retry_cnt = qp_attr->rdma.retry_cnt;
    } else { // protocol == PROTOCOL_UDMA
        ctx_qp_attr->ub.mode = qp_attr->ub.mode;
        ctx_qp_attr->ub.jetty_id = qp_attr->ub.jetty_id;
        ctx_qp_attr->ub.flag = qp_attr->ub.flag;
        ctx_qp_attr->ub.jfs_flag = qp_attr->ub.jfs_flag;
        ctx_qp_attr->ub.token_value = qp_attr->ub.token_value;
        ctx_qp_attr->ub.priority = qp_attr->ub.priority;
        ctx_qp_attr->ub.rnr_retry = qp_attr->ub.rnr_retry;
        ctx_qp_attr->ub.err_timeout = qp_attr->ub.err_timeout;
        if (ctx_qp_attr->ub.mode == JETTY_MODE_CCU_TA_CACHE) {
            ctx_qp_attr->ub.ta_cache_mode.lock_flag = qp_attr->ub.ta_cache_mode.lock_flag;
            ctx_qp_attr->ub.ta_cache_mode.sqe_buf_idx = qp_attr->ub.ta_cache_mode.sqe_buf_idx;
        } else {
            ctx_qp_attr->ub.ext_mode.sq = qp_attr->ub.ext_mode.sq;
            ctx_qp_attr->ub.ext_mode.pi_type = qp_attr->ub.ext_mode.pi_type;
            ctx_qp_attr->ub.ext_mode.cstm_flag = qp_attr->ub.ext_mode.cstm_flag;
            ctx_qp_attr->ub.ext_mode.sqebb_num = qp_attr->ub.ext_mode.sqebb_num;
        }
        if (qp_attr->ub.token_id_handle != NULL) {
            token_id_handle = (struct ra_token_id_handle *)(qp_attr->ub.token_id_handle);
            ctx_qp_attr->ub.token_id_addr = token_id_handle->addr;
        }
    }
    return 0;
}

int ra_hdc_ctx_qp_create(struct ra_ctx_handle *ctx_handle, struct qp_create_attr *qp_attr,
    struct qp_create_info *qp_info, struct ra_ctx_qp_handle *qp_handle)
{
    unsigned int dev_index = ctx_handle->dev_index;
    unsigned int phy_id = ctx_handle->attr.phy_id;
    union op_ctx_qp_create_data op_data = {0};
    int ret;

    ret = ra_hdc_ctx_prepare_qp_create(ctx_handle, qp_attr, &op_data);
    CHK_PRT_RETURN(ret, hccp_err("[init][ra_hdc_qp]prepare qp_create failed ret[%d], phy_id[%u] dev_index[%u]",
        ret, phy_id, dev_index), ret);

    ret = ra_hdc_process_msg(RA_RS_CTX_QP_CREATE, phy_id, (char *)&op_data, sizeof(union op_ctx_qp_create_data));
    CHK_PRT_RETURN(ret, hccp_err("[init][ra_hdc_qp]hdc message process failed ret[%d], phy_id[%u] dev_index[%u]",
        ret, phy_id, dev_index), ret);

    ret = ra_hdc_get_qp_create_info(ctx_handle, qp_attr, &op_data, qp_info, qp_handle);
    CHK_PRT_RETURN(ret, hccp_err("[init][ra_hdc_qp]ra_hdc_get_qp_create_info failed ret[%d], phy_id[%u] dev_index[%u]",
        ret, phy_id, dev_index), ret);

    return 0;
}

int ra_hdc_ctx_qp_destroy(struct ra_ctx_qp_handle *qp_handle)
{
    unsigned int phy_id = qp_handle->phy_id;
    union op_ctx_qp_destroy_data op_data = {0};
    int ret;

    op_data.tx_data.phy_id = phy_id;
    op_data.tx_data.dev_index = qp_handle->dev_index;
    op_data.tx_data.id = qp_handle->id;

    ret = ra_hdc_process_msg(RA_RS_CTX_QP_DESTROY, phy_id, (char *)&op_data, sizeof(union op_ctx_qp_destroy_data));
    CHK_PRT_RETURN(ret == -ENODEV, hccp_warn("[deinit][ra_hdc_qp]hdc message process ret[%d], phy_id[%u] dev_index[%u]",
        ret, phy_id, qp_handle->dev_index), ret);
    CHK_PRT_RETURN(ret != 0, hccp_err("[deinit][ra_hdc_qp]hdc message process failed ret[%d], phy_id[%u] dev_index[%u]",
        ret, phy_id, qp_handle->dev_index), ret);
    return 0;
}

int ra_hdc_ctx_prepare_qp_import(struct ra_ctx_handle *ctx_handle, struct qp_import_info_t *qp_info,
    union op_ctx_qp_import_data *op_data)
{
    CHK_PRT_RETURN(ctx_handle->protocol != PROTOCOL_UDMA, hccp_err("[init][ra_hdc_qp]protocol:%d not support",
        ctx_handle->protocol), -EINVAL);

    op_data->tx_data.dev_index = ctx_handle->dev_index;
    op_data->tx_data.phy_id = ctx_handle->attr.phy_id;
    op_data->tx_data.key = qp_info->in.key;
    op_data->tx_data.attr.mode = qp_info->in.ub.mode;
    op_data->tx_data.attr.token_value = qp_info->in.ub.token_value;
    op_data->tx_data.attr.policy = qp_info->in.ub.policy;
    op_data->tx_data.attr.type = qp_info->in.ub.type;
    op_data->tx_data.attr.flag = qp_info->in.ub.flag;
    op_data->tx_data.attr.exp_import_cfg = qp_info->in.ub.exp_import_cfg;
    return 0;
}

STATIC void ra_hdc_get_qp_import_info(struct ra_ctx_handle *ctx_handle, struct qp_import_info_t *qp_info,
    union op_ctx_qp_import_data *op_data, struct ra_ctx_rem_qp_handle *qp_handle)
{
    qp_info->out.ub.tjetty_handle = op_data->rx_data.info.tjetty_handle;
    qp_info->out.ub.tpn = op_data->rx_data.info.tpn;
    qp_handle->id = op_data->rx_data.rem_jetty_id;
    qp_handle->dev_index = ctx_handle->dev_index;
    qp_handle->phy_id = ctx_handle->attr.phy_id;
    qp_handle->protocol = ctx_handle->protocol;
    qp_handle->qp_key = qp_info->in.key;
}

int ra_hdc_ctx_qp_import(struct ra_ctx_handle *ctx_handle, struct qp_import_info_t *qp_info,
    struct ra_ctx_rem_qp_handle *rem_qp_handle)
{
    unsigned int dev_index = ctx_handle->dev_index;
    unsigned int phy_id = ctx_handle->attr.phy_id;
    union op_ctx_qp_import_data op_data = {0};
    int ret;

    ret = ra_hdc_ctx_prepare_qp_import(ctx_handle, qp_info, &op_data);
    CHK_PRT_RETURN(ret, hccp_err("[init][ra_hdc_qp]prepare qp_import failed ret[%d], phy_id[%u] dev_index[%u]",
        ret, phy_id, dev_index), ret);

    ret = ra_hdc_process_msg(RA_RS_CTX_QP_IMPORT, phy_id, (char *)&op_data, sizeof(union op_ctx_qp_import_data));
    CHK_PRT_RETURN(ret, hccp_err("[init][ra_hdc_qp]hdc message process failed ret[%d], phy_id[%u] dev_index[%u]",
        ret, phy_id, dev_index), ret);

    ra_hdc_get_qp_import_info(ctx_handle, qp_info, &op_data, rem_qp_handle);
    return 0;
}

int ra_hdc_ctx_qp_unimport(struct ra_ctx_rem_qp_handle *rem_qp_handle)
{
    unsigned int phy_id = rem_qp_handle->phy_id;
    union op_ctx_qp_unimport_data op_data = {0};
    int ret;

    op_data.tx_data.phy_id = phy_id;
    op_data.tx_data.dev_index = rem_qp_handle->dev_index;
    op_data.tx_data.rem_jetty_id = rem_qp_handle->id;
    ret = ra_hdc_process_msg(RA_RS_CTX_QP_UNIMPORT, phy_id, (char *)&op_data, sizeof(union op_ctx_qp_unimport_data));
    CHK_PRT_RETURN(ret, hccp_err("[deinit][ra_qp]hdc message process failed ret[%d], phy_id[%u] dev_index[%u]",
        ret, phy_id, rem_qp_handle->dev_index), ret);

    return 0;
}

static int ra_hdc_ctx_prepare_qp_bind(struct ra_ctx_qp_handle *qp_handle, struct ra_ctx_rem_qp_handle *rem_qp_handle,
    union op_ctx_qp_bind_data *op_data)
{
    unsigned int protocol = qp_handle->protocol;

    op_data->tx_data.phy_id = qp_handle->phy_id;
    op_data->tx_data.dev_index = qp_handle->dev_index;
    op_data->tx_data.id = qp_handle->id;

    if (protocol == PROTOCOL_RDMA) {
        op_data->tx_data.local_qp_key = qp_handle->qp_info.key;
        op_data->tx_data.remote_qp_key = rem_qp_handle->qp_key;
    } else { // protocol == PROTOCOL_UDMA
        op_data->tx_data.rem_id = rem_qp_handle->id;
    }

    return 0;
}

int ra_hdc_ctx_qp_bind(struct ra_ctx_qp_handle *qp_handle, struct ra_ctx_rem_qp_handle *rem_qp_handle)
{
    unsigned int dev_index = qp_handle->dev_index;
    unsigned int phy_id = qp_handle->phy_id;
    union op_ctx_qp_bind_data op_data = {0};
    int ret;

    ret = ra_hdc_ctx_prepare_qp_bind(qp_handle, rem_qp_handle, &op_data);
    CHK_PRT_RETURN(ret, hccp_err("[init][ra_hdc_qp]prepare qp_bind failed ret[%d], phy_id[%u] dev_index[%u]",
        ret, phy_id, dev_index), ret);

    ret = ra_hdc_process_msg(RA_RS_CTX_QP_BIND, phy_id, (char *)&op_data, sizeof(union op_ctx_qp_bind_data));
    CHK_PRT_RETURN(ret, hccp_err("[init][ra_hdc_qp]hdc message process failed ret[%d], phy_id[%u] dev_index[%u]",
        ret, phy_id, dev_index), ret);

    return 0;
}

int ra_hdc_ctx_qp_unbind(struct ra_ctx_qp_handle *qp_handle)
{
    union op_ctx_qp_unbind_data op_data = {0};
    unsigned int phy_id = qp_handle->phy_id;
    int ret;

    op_data.tx_data.phy_id = phy_id;
    op_data.tx_data.dev_index = qp_handle->dev_index;
    op_data.tx_data.id = qp_handle->id;

    ret = ra_hdc_process_msg(RA_RS_CTX_QP_UNBIND, phy_id, (char *)&op_data, sizeof(union op_ctx_qp_unbind_data));
    CHK_PRT_RETURN(ret == -ENODEV, hccp_warn("[deinit][ra_qp]hdc message process ret[%d], phy_id[%u] dev_index[%u]",
        ret, phy_id, qp_handle->dev_index), ret);
    CHK_PRT_RETURN(ret != 0, hccp_err("[deinit][ra_qp]hdc message process failed ret[%d], phy_id[%u] dev_index[%u]",
        ret, phy_id, qp_handle->dev_index), ret);
    return 0;
}

STATIC int ra_hdc_send_wr_data_protocol_init(struct send_wr_data *wr, struct batch_send_wr_data *wr_data,
    enum protocol_type protocol, bool *is_inline)
{
    struct ra_ctx_rem_qp_handle *rem_qp_handle = NULL;
    struct ra_rmem_handle *rmem_handle = NULL;
    int ret;

    if (protocol == PROTOCOL_UDMA) {
        *is_inline = (wr->ub.flags.bs.inline_flag != 0);
        wr_data->ub.user_ctx = wr->ub.user_ctx;
        wr_data->ub.opcode = wr->ub.opcode;
        wr_data->ub.flags = wr->ub.flags;
        /* nop no need to init other infos */
        if (wr->ub.opcode == RA_UB_OPC_NOP) {
            return 0;
        }
        rem_qp_handle = (struct ra_ctx_rem_qp_handle *)wr->ub.rem_qp_handle;
        wr_data->ub.rem_jetty = rem_qp_handle->id;
        /* notify */
        if (wr->ub.opcode == RA_UB_OPC_WRITE_NOTIFY) {
            wr_data->ub.notify_info.notify_addr = wr->ub.notify_info.notify_addr;
            wr_data->ub.notify_info.notify_data = wr->ub.notify_info.notify_data;
            rmem_handle = (struct ra_rmem_handle *)(wr->ub.notify_info.notify_handle);
            wr_data->ub.notify_info.notify_handle = rmem_handle->addr;
        }
    } else {
        *is_inline = ((wr->rdma.flags & RA_SEND_INLINE) != 0);
        ret = memcpy_s(&wr_data->rdma, sizeof(wr_data->rdma),
            &(wr->rdma), sizeof(wr->rdma));
        CHK_PRT_RETURN(ret, hccp_err("[send][ra_hdc_ctx]memcpy_s protocol failed, ret[%d]", ret),
            -ESAFEFUNC);
    }

    return 0;
}

STATIC int ra_hdc_send_wr_data_init(struct ra_ctx_qp_handle *qp_handle, struct send_wr_data wr_list[],
    union op_ctx_batch_send_wr_data *op_data, unsigned int complete_cnt, unsigned int send_num)
{
    unsigned int curr_batch_num = (send_num - complete_cnt) >= MAX_CTX_WR_NUM ?
        MAX_CTX_WR_NUM : (send_num - complete_cnt);
    struct ra_lmem_handle *lmem_handle = NULL;
    struct ra_rmem_handle *rmem_handle = NULL;
    struct batch_send_wr_data *hdc_wr = NULL;
    struct send_wr_data *wr = NULL;
    unsigned int i, j;
    bool is_inline;
    int ret;

    (void)memset_s(op_data, sizeof(union op_ctx_batch_send_wr_data), 0, sizeof(union op_ctx_batch_send_wr_data));
    op_data->tx_data.base_info.phy_id = qp_handle->phy_id;
    op_data->tx_data.base_info.dev_index = qp_handle->dev_index;
    op_data->tx_data.base_info.qpn = qp_handle->id;
    op_data->tx_data.send_num = curr_batch_num;

    for (i = 0; i < curr_batch_num; i++) {
        wr = &wr_list[complete_cnt + i];
        hdc_wr = &op_data->tx_data.wr_data[i];
        /* protocol cfg */
        ret = ra_hdc_send_wr_data_protocol_init(wr, hdc_wr, qp_handle->protocol, &is_inline);
        if (ret != 0) {
            hccp_err("[send][ra_hdc_ctx]init protocol cfg failed, ret[%d], wr[%u]",
                ret, (complete_cnt + i));
            return ret;
        }

        /* nop no need init other infos */
        if (qp_handle->protocol == PROTOCOL_UDMA && wr->ub.opcode == RA_UB_OPC_NOP) {
            continue;
        }

        /* lmem */
        if (is_inline) {
            ret = memcpy_s(hdc_wr->inline_data, MAX_INLINE_SIZE, wr->inline_data, wr->inline_size);
            CHK_PRT_RETURN(ret, hccp_err("[send][ra_hdc_ctx]memcpy_s inline data failed, ret[%d]",
                ret), -ESAFEFUNC);
            hdc_wr->inline_size = wr->inline_size;
        } else {
            for (j = 0; j < wr->num_sge; j++) {
                hdc_wr->sges[j].addr = wr->sges[j].addr;
                hdc_wr->sges[j].len = wr->sges[j].len;
                lmem_handle = (struct ra_lmem_handle *)wr->sges[j].lmem_handle;
                hdc_wr->sges[j].dev_lmem_handle = lmem_handle->addr;
            }
            hdc_wr->num_sge = wr->num_sge;
        }

        /* rmem */
        hdc_wr->remote_addr = wr->remote_addr;
        rmem_handle = (struct ra_rmem_handle *)(wr->rmem_handle);
        hdc_wr->dev_rmem_handle = rmem_handle->addr;

        /* inline reduce */
        hdc_wr->ub.reduce_info = wr->ub.reduce_info;

        /* imm data */
        hdc_wr->imm_data = wr->imm_data;
    }

    return 0;
}

int ra_hdc_ctx_batch_send_wr(struct ra_ctx_qp_handle *qp_handle, struct send_wr_data wr_list[],
    struct send_wr_resp op_resp[], unsigned int send_num, unsigned int *complete_num)
{
    union op_ctx_batch_send_wr_data op_data = {0};
    unsigned int complete_cnt = 0;
    unsigned int curr_send_num;
    bool is_finished = false;
    int ret = 0;

    while (complete_cnt < send_num) {
        ret = ra_hdc_send_wr_data_init(qp_handle, wr_list, &op_data, complete_cnt, send_num);
        if (ret != 0) {
            hccp_err("[send][ra_hdc_ctx]ra_hdc_send_wr_data_init failed, ret[%d] phy_id[%u] dev_index[%u] qp_id[%u]",
                ret, qp_handle->phy_id, qp_handle->dev_index, qp_handle->id);
            break;
        }

        curr_send_num = op_data.tx_data.send_num;
        ret = ra_hdc_process_msg(RA_RS_CTX_BATCH_SEND_WR, qp_handle->phy_id, (char *)&op_data,
            sizeof(union op_ctx_batch_send_wr_data));

        if (op_data.rx_data.complete_num > curr_send_num) {
            hccp_err("[send][ra_hdc_ctx]complete_num[%u] is larger than send_num[%u], ret[%d]",
                op_data.rx_data.complete_num, curr_send_num, ret);
            ret = -EINVAL;
            break;
        }
        if (ret != 0 || op_data.rx_data.complete_num < curr_send_num) {
            hccp_err("[send][ra_hdc_ctx]batch send wr failed, ret[%d], send_num[%u], complete_num[%u]",
                ret, curr_send_num, op_data.rx_data.complete_num);
            ret = -EOPENSRC;
            is_finished = true;
        }

        ret = memcpy_s(&op_resp[complete_cnt], (sizeof(struct send_wr_resp) * (send_num - complete_cnt)),
            op_data.rx_data.wr_resp, (sizeof(struct send_wr_resp) * op_data.rx_data.complete_num));
        if (ret != 0) {
            hccp_err("[send][ra_hdc_ctx]memcpy_s wr_resp failed, ret[%d]", ret);
            break;
        }

        complete_cnt = complete_cnt + op_data.rx_data.complete_num;
        if (is_finished) {
            break;
        }
    }

    *complete_num = complete_cnt;
    return ret;
}

int ra_hdc_ctx_update_ci(struct ra_ctx_qp_handle *qp_handle, uint16_t ci)
{
    unsigned int phy_id = qp_handle->phy_id;
    union op_ctx_update_ci_data op_data = {0};
    int ret;

    op_data.tx_data.phy_id = phy_id;
    op_data.tx_data.dev_index = qp_handle->dev_index;
    op_data.tx_data.jetty_id = qp_handle->id;
    op_data.tx_data.ci = ci;

    ret = ra_hdc_process_msg(RA_RS_CTX_UPDATE_CI, phy_id, (char *)&op_data, sizeof(union op_ctx_update_ci_data));
    CHK_PRT_RETURN(ret, hccp_err("[update][ra_qp]hdc message process failed ret[%d], phy_id[%u] dev_index[%u]",
        ret, phy_id, qp_handle->dev_index), ret);

    return 0;
}

int ra_hdc_custom_channel(unsigned int phy_id, struct custom_chan_info_in *in, struct custom_chan_info_out *out)
{
    union op_custom_channel_data op_data = {0};
    int ret;

    op_data.tx_data.phy_id = phy_id;
    (void)memcpy_s(&op_data.tx_data.info, sizeof(struct custom_chan_info_in), in, sizeof(struct custom_chan_info_in));

    ret = ra_hdc_process_msg(RA_RS_CUSTOM_CHANNEL, phy_id, (char *)&op_data, sizeof(union op_custom_channel_data));
    CHK_PRT_RETURN(ret != 0, hccp_err("[custom]hdc message process failed ret[%d], phy_id[%u]", ret, phy_id), ret);

    (void)memcpy_s(out, sizeof(struct custom_chan_info_out), &op_data.rx_data.info,
        sizeof(struct custom_chan_info_out));
    return 0;
}

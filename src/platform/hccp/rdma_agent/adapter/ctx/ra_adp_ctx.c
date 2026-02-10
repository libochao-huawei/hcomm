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
#include "hccp_ctx.h"
#include "ra_hdc_ctx.h"
#include "ra_hdc_async_ctx.h"
#include "ra_rs_ctx.h"
#include "ra_rs_comm.h"
#include "rs_ctx.h"
#include "ra_adp.h"
#include "ra_adp_ctx.h"

struct rs_ctx_ops g_ra_rs_ctx_ops = {
    .get_dev_eid_info_num = rs_get_dev_eid_info_num,
    .get_dev_eid_info_list = rs_get_dev_eid_info_list,
    .ctx_init = rs_ctx_init,
    .ctx_get_async_events = rs_ctx_get_async_events,
    .ctx_deinit = rs_ctx_deinit,
    .get_eid_by_ip = rs_get_eid_by_ip,
    .get_tp_info_list = rs_get_tp_info_list,
    .get_tp_attr = rs_get_tp_attr,
    .set_tp_attr = rs_set_tp_attr,
    .ctx_token_id_alloc = rs_ctx_token_id_alloc,
    .ctx_token_id_free = rs_ctx_token_id_free,
    .ctx_lmem_reg = rs_ctx_lmem_reg,
    .ctx_lmem_unreg = rs_ctx_lmem_unreg,
    .ctx_rmem_import = rs_ctx_rmem_import,
    .ctx_rmem_unimport = rs_ctx_rmem_unimport,
    .ctx_chan_create = rs_ctx_chan_create,
    .ctx_chan_destroy = rs_ctx_chan_destroy,
    .ctx_cq_create = rs_ctx_cq_create,
    .ctx_cq_destroy = rs_ctx_cq_destroy,
    .ctx_qp_create = rs_ctx_qp_create,
    .ctx_qp_query_batch = rs_ctx_qp_query_batch,
    .ctx_qp_destroy = rs_ctx_qp_destroy,
    .ctx_qp_destroy_batch = rs_ctx_qp_destroy_batch,
    .ctx_qp_import = rs_ctx_qp_import,
    .ctx_qp_unimport = rs_ctx_qp_unimport,
    .ctx_qp_bind = rs_ctx_qp_bind,
    .ctx_qp_unbind = rs_ctx_qp_unbind,
    .ctx_batch_send_wr = rs_ctx_batch_send_wr,
    .ccu_custom_channel = rs_ctx_custom_channel,
    .ctx_update_ci = rs_ctx_update_ci,
    .ctx_get_aux_info = rs_ctx_get_aux_info,
    .ctx_get_cr_err_info_list = rs_ctx_get_cr_err_info_list,
};

int ra_rs_get_dev_eid_info_num(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_get_dev_eid_info_num_data *op_data = (union op_get_dev_eid_info_num_data *)(in_buf +
        sizeof(struct MsgHead));
    unsigned int num = 0;

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_get_dev_eid_info_num_data), sizeof(struct MsgHead), rcv_buf_len,
        op_result);

    *op_result = g_ra_rs_ctx_ops.get_dev_eid_info_num(op_data->tx_data.phy_id, &num);
    CHK_PRT_RETURN(*op_result != 0, hccp_err("[get][eid]get_dev_eid_info_num failed, ret[%d].", *op_result), 0);

    op_data = (union op_get_dev_eid_info_num_data *)(out_buf + sizeof(struct MsgHead));
    op_data->rx_data.num = num;
    return 0;
}

int ra_rs_get_dev_eid_info_list(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_get_dev_eid_info_list_data *op_data = (union op_get_dev_eid_info_list_data *)(in_buf +
        sizeof(struct MsgHead));
    struct dev_eid_info info_list[MAX_DEV_INFO_TRANS_NUM] = {0};

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_get_dev_eid_info_list_data), sizeof(struct MsgHead), rcv_buf_len,
        op_result);
    HCCP_CHECK_PARAM_LEN_RET_HOST(op_data->tx_data.count, 0, MAX_DEV_INFO_TRANS_NUM, op_result);

    *op_result = g_ra_rs_ctx_ops.get_dev_eid_info_list(op_data->tx_data.phy_id, info_list,
        op_data->tx_data.start_index, op_data->tx_data.count);
    CHK_PRT_RETURN(*op_result != 0, hccp_err("[get][eid]get_dev_eid_info_list failed, ret[%d].", *op_result), 0);

    op_data = (union op_get_dev_eid_info_list_data *)(out_buf + sizeof(struct MsgHead));
    (void)memcpy_s(op_data->rx_data.info_list, sizeof(struct dev_eid_info) * MAX_DEV_INFO_TRANS_NUM,
        info_list, sizeof(struct dev_eid_info) * MAX_DEV_INFO_TRANS_NUM);
    return 0;
}

int ra_rs_ctx_init(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_ctx_init_data *op_data_out = (union op_ctx_init_data *)(out_buf + sizeof(struct MsgHead));
    union op_ctx_init_data *op_data = (union op_ctx_init_data *)(in_buf + sizeof(struct MsgHead));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_ctx_init_data), sizeof(struct MsgHead), rcv_buf_len, op_result);

    *op_result = g_ra_rs_ctx_ops.ctx_init(&op_data->tx_data.attr, &op_data_out->rx_data.dev_index,
        &op_data_out->rx_data.dev_attr);
    if (*op_result != 0) {
        hccp_err("[init][ra_rs_ctx]init failed, ret[%d]", *op_result);
    }

    return 0;
}

int ra_rs_ctx_get_async_events(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_ctx_get_async_events_data *op_data_out =
        (union op_ctx_get_async_events_data *)(out_buf + sizeof(struct MsgHead));
    union op_ctx_get_async_events_data *op_data =
        (union op_ctx_get_async_events_data *)(in_buf + sizeof(struct MsgHead));
    struct RaRsDevInfo dev_info = {0};

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_ctx_get_async_events_data), sizeof(struct MsgHead), rcv_buf_len,
        op_result);
    HCCP_CHECK_PARAM_LEN_RET_HOST(op_data->tx_data.num, 0, ASYNC_EVENT_MAX_NUM, op_result);

    op_data_out->rx_data.num = op_data->tx_data.num;
    ra_rs_set_dev_info(&dev_info, op_data->tx_data.phy_id, op_data->tx_data.dev_index);
    *op_result = g_ra_rs_ctx_ops.ctx_get_async_events(&dev_info, op_data_out->rx_data.events,
        &op_data_out->rx_data.num);
    if (*op_result != 0) {
        hccp_err("[get][async_events]ctx_get_async_events failed, ret[%d] phy_id[%u] dev_index[0x%x]",
            *op_result, dev_info.phyId, dev_info.devIndex);
    }

    return 0;
}

int ra_rs_ctx_deinit(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_ctx_deinit_data *op_data = (union op_ctx_deinit_data *)(in_buf + sizeof(struct MsgHead));
    struct RaRsDevInfo dev_info = {0};

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_ctx_deinit_data), sizeof(struct MsgHead), rcv_buf_len, op_result);

    ra_rs_set_dev_info(&dev_info, op_data->tx_data.phy_id, op_data->tx_data.dev_index);
    *op_result = g_ra_rs_ctx_ops.ctx_deinit(&dev_info);
    if (*op_result != 0) {
        hccp_err("[deinit][ra_rs_ctx]deinit failed, ret[%d] phy_id[%u] dev_index[0x%x]",
            *op_result, dev_info.phyId, dev_info.devIndex);
    }

    return 0;
}

int ra_rs_get_eid_by_ip(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_get_eid_by_ip_data *op_data_out = (union op_get_eid_by_ip_data *)(out_buf + sizeof(struct MsgHead));
    union op_get_eid_by_ip_data *op_data = (union op_get_eid_by_ip_data *)(in_buf + sizeof(struct MsgHead));
    struct RaRsDevInfo dev_info = {0};

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_get_eid_by_ip_data), sizeof(struct MsgHead), rcv_buf_len, op_result);
    HCCP_CHECK_PARAM_LEN_RET_HOST(op_data->tx_data.num, 0, GET_EID_BY_IP_MAX_NUM, op_result);

    op_data_out->rx_data.num = op_data->tx_data.num;
    ra_rs_set_dev_info(&dev_info, op_data->tx_data.phy_id, op_data->tx_data.dev_index);
    *op_result = g_ra_rs_ctx_ops.get_eid_by_ip(&dev_info, op_data->tx_data.ip, op_data_out->rx_data.eid,
        &op_data_out->rx_data.num);
    if (*op_result != 0) {
        hccp_err("[get][eid_by_ip]get_eid_by_ip failed, ret[%d], phy_id[%u]", *op_result,
            op_data->tx_data.phy_id);
    }

    return 0;
}

int ra_rs_get_tp_info_list(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_get_tp_info_list_data *op_data_out = (union op_get_tp_info_list_data *)(out_buf + sizeof(struct MsgHead));
    union op_get_tp_info_list_data *op_data = (union op_get_tp_info_list_data *)(in_buf + sizeof(struct MsgHead));
    struct RaRsDevInfo dev_info = {0};

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_get_tp_info_list_data), sizeof(struct MsgHead), rcv_buf_len,
        op_result);

    HCCP_CHECK_PARAM_LEN_RET_HOST(op_data->tx_data.num, 0, HCCP_MAX_TPID_INFO_NUM, op_result);
    op_data_out->rx_data.num = op_data->tx_data.num;
    ra_rs_set_dev_info(&dev_info, op_data->tx_data.phy_id, op_data->tx_data.dev_index);
    *op_result = g_ra_rs_ctx_ops.get_tp_info_list(&dev_info, &op_data->tx_data.cfg, op_data_out->rx_data.info_list,
        &op_data_out->rx_data.num);
    if (*op_result != 0) {
        hccp_err("[get][ra_rs_ctx]get_tp_info_list failed, ret[%d] phy_id[%u] dev_index[0x%x]",
            *op_result, dev_info.phyId, dev_info.devIndex);
    }

    return 0;
}

int ra_rs_get_tp_attr(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_get_tp_attr_data *op_data_out = (union op_get_tp_attr_data *)(out_buf + sizeof(struct MsgHead));
    union op_get_tp_attr_data *op_data = (union op_get_tp_attr_data *)(in_buf + sizeof(struct MsgHead));
    struct RaRsDevInfo dev_info = {0};

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_get_tp_attr_data), sizeof(struct MsgHead), rcv_buf_len,
        op_result);

    ra_rs_set_dev_info(&dev_info, op_data->tx_data.phy_id, op_data->tx_data.dev_index);
    op_data_out->rx_data.attr_bitmap = op_data->tx_data.attr_bitmap;
    *op_result = g_ra_rs_ctx_ops.get_tp_attr(&dev_info, &op_data_out->rx_data.attr_bitmap,
        op_data->tx_data.tp_handle, &op_data_out->rx_data.attr);
    if (*op_result != 0) {
        hccp_err("[get_tp_attr][ra_rs_ctx]get attr failed, ret[%d], phy_id[%u]", *op_result, op_data->tx_data.phy_id);
    }

    return 0;
}

int ra_rs_set_tp_attr(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_set_tp_attr_data *op_data = (union op_set_tp_attr_data *)(in_buf + sizeof(struct MsgHead));
    struct RaRsDevInfo dev_info = {0};

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_set_tp_attr_data), sizeof(struct MsgHead), rcv_buf_len,
        op_result);

    ra_rs_set_dev_info(&dev_info, op_data->tx_data.phy_id, op_data->tx_data.dev_index);
    *op_result = g_ra_rs_ctx_ops.set_tp_attr(&dev_info, op_data->tx_data.attr_bitmap,
        op_data->tx_data.tp_handle, &op_data->tx_data.attr);
    if (*op_result != 0) {
        hccp_err("[set_tp_attr][ra_rs_ctx]set attr failed, ret[%d], phy_id[%u]", *op_result, op_data->tx_data.phy_id);
    }

    return 0;
}

int ra_rs_ctx_token_id_alloc(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_token_id_alloc_data *op_data_out = (union op_token_id_alloc_data *)(out_buf + sizeof(struct MsgHead));
    union op_token_id_alloc_data *op_data = (union op_token_id_alloc_data *)(in_buf + sizeof(struct MsgHead));
    struct RaRsDevInfo dev_info = {0};

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_token_id_alloc_data), sizeof(struct MsgHead), rcv_buf_len,
        op_result);

    ra_rs_set_dev_info(&dev_info, op_data->tx_data.phy_id, op_data->tx_data.dev_index);
    *op_result = g_ra_rs_ctx_ops.ctx_token_id_alloc(&dev_info, &op_data_out->rx_data.addr,
        &op_data_out->rx_data.token_id);
    if (*op_result != 0) {
        hccp_err("[init][ra_rs_token_id]alloc failed, ret[%d] phy_id[%u] dev_index[0x%x]",
            *op_result, dev_info.phyId, dev_info.devIndex);
    }
    return 0;
}

int ra_rs_ctx_token_id_free(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_token_id_free_data *op_data = (union op_token_id_free_data *)(in_buf + sizeof(struct MsgHead));
    struct RaRsDevInfo dev_info = {0};

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_token_id_free_data), sizeof(struct MsgHead), rcv_buf_len,
        op_result);

    ra_rs_set_dev_info(&dev_info, op_data->tx_data.phy_id, op_data->tx_data.dev_index);
    *op_result = g_ra_rs_ctx_ops.ctx_token_id_free(&dev_info, op_data->tx_data.addr);
    if (*op_result != 0) {
        hccp_err("[deinit][ra_rs_token_id]free failed, ret[%d] phy_id[%u] dev_index[0x%x]",
            *op_result, dev_info.phyId, dev_info.devIndex);
    }
    return 0;
}

int ra_rs_lmem_reg(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_lmem_reg_info_data *op_data_out = (union op_lmem_reg_info_data *)(out_buf + sizeof(struct MsgHead));
    union op_lmem_reg_info_data *op_data = (union op_lmem_reg_info_data *)(in_buf + sizeof(struct MsgHead));
    struct RaRsDevInfo dev_info = {0};

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_lmem_reg_info_data), sizeof(struct MsgHead), rcv_buf_len, op_result);

    ra_rs_set_dev_info(&dev_info, op_data->tx_data.phy_id, op_data->tx_data.dev_index);
    *op_result = g_ra_rs_ctx_ops.ctx_lmem_reg(&dev_info, &op_data->tx_data.mem_attr, &op_data_out->rx_data.mem_info);
    if (*op_result != 0) {
        hccp_err("[init][ra_rs_lmem]reg failed, ret[%d] phy_id[%u] dev_index[0x%x]",
            *op_result, dev_info.phyId, dev_info.devIndex);
    }

    return 0;
}

int ra_rs_lmem_unreg(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_lmem_unreg_info_data *op_data = (union op_lmem_unreg_info_data *)(in_buf + sizeof(struct MsgHead));
    struct RaRsDevInfo dev_info = {0};

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_lmem_unreg_info_data), sizeof(struct MsgHead), rcv_buf_len,
        op_result);

    ra_rs_set_dev_info(&dev_info, op_data->tx_data.phy_id, op_data->tx_data.dev_index);
    *op_result = g_ra_rs_ctx_ops.ctx_lmem_unreg(&dev_info, op_data->tx_data.addr);
    if (*op_result != 0) {
        hccp_err("[deinit][ra_rs_lmem]unreg failed, ret[%d] phy_id[%u] dev_index[0x%x]",
            *op_result, dev_info.phyId, dev_info.devIndex);
    }

    return 0;
}

int ra_rs_rmem_import(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_rmem_import_info_data *op_data_out = (union op_rmem_import_info_data *)(out_buf + sizeof(struct MsgHead));
    union op_rmem_import_info_data *op_data = (union op_rmem_import_info_data *)(in_buf + sizeof(struct MsgHead));
    struct RaRsDevInfo dev_info = {0};

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_rmem_import_info_data), sizeof(struct MsgHead), rcv_buf_len,
        op_result);

    ra_rs_set_dev_info(&dev_info, op_data->tx_data.phy_id, op_data->tx_data.dev_index);
    *op_result = g_ra_rs_ctx_ops.ctx_rmem_import(&dev_info, &op_data->tx_data.mem_attr, &op_data_out->rx_data.mem_info);
    if (*op_result != 0) {
        hccp_err("[init][ra_rs_rmem]import failed, ret[%d] phy_id[%u] dev_index[0x%x]",
            *op_result, dev_info.phyId, dev_info.devIndex);
    }

    return 0;
}

int ra_rs_rmem_unimport(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_rmem_unimport_info_data *op_data = (union op_rmem_unimport_info_data *)(in_buf + sizeof(struct MsgHead));
    struct RaRsDevInfo dev_info = {0};

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_rmem_unimport_info_data), sizeof(struct MsgHead), rcv_buf_len,
        op_result);

    ra_rs_set_dev_info(&dev_info, op_data->tx_data.phy_id, op_data->tx_data.dev_index);
    *op_result = g_ra_rs_ctx_ops.ctx_rmem_unimport(&dev_info, op_data->tx_data.addr);
    if (*op_result != 0) {
        hccp_err("[deinit][ra_rs_rmem]unimport failed, ret[%d] phy_id[%u] dev_index[0x%x]",
            *op_result, dev_info.phyId, dev_info.devIndex);
    }

    return 0;
}

int ra_rs_ctx_chan_create(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_ctx_chan_create_data *op_data_out = (union op_ctx_chan_create_data *)(out_buf + sizeof(struct MsgHead));
    union op_ctx_chan_create_data *op_data = (union op_ctx_chan_create_data *)(in_buf + sizeof(struct MsgHead));
    struct RaRsDevInfo dev_info = {0};

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_ctx_chan_create_data), sizeof(struct MsgHead), rcv_buf_len,
        op_result);

    ra_rs_set_dev_info(&dev_info, op_data->tx_data.phy_id, op_data->tx_data.dev_index);
    *op_result = g_ra_rs_ctx_ops.ctx_chan_create(&dev_info, op_data->tx_data.data_plane_flag,
        &op_data_out->rx_data.addr, &op_data_out->rx_data.fd);
    if (*op_result != 0) {
        hccp_err("[init][ra_rs_chan]create failed, ret[%d] phy_id[%u] dev_index[0x%x]",
            *op_result, dev_info.phyId, dev_info.devIndex);
    }

    return 0;
}

int ra_rs_ctx_chan_destroy(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_ctx_chan_destroy_data *op_data = (union op_ctx_chan_destroy_data *)(in_buf + sizeof(struct MsgHead));
    struct RaRsDevInfo dev_info = {0};

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_ctx_chan_destroy_data), sizeof(struct MsgHead), rcv_buf_len, op_result);

    ra_rs_set_dev_info(&dev_info, op_data->tx_data.phy_id, op_data->tx_data.dev_index);
    *op_result = g_ra_rs_ctx_ops.ctx_chan_destroy(&dev_info, op_data->tx_data.addr);
    if (*op_result != 0) {
        hccp_err("[deinit][ra_rs_chan]destroy failed, ret[%d] phy_id[%u] dev_index[0x%x]",
            *op_result, dev_info.phyId, dev_info.devIndex);
    }

    return 0;
}

int ra_rs_ctx_cq_create(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_ctx_cq_create_data *op_data_out = (union op_ctx_cq_create_data *)(out_buf + sizeof(struct MsgHead));
    union op_ctx_cq_create_data *op_data = (union op_ctx_cq_create_data *)(in_buf + sizeof(struct MsgHead));
    struct RaRsDevInfo dev_info = {0};

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_ctx_cq_create_data), sizeof(struct MsgHead), rcv_buf_len, op_result);

    ra_rs_set_dev_info(&dev_info, op_data->tx_data.phy_id, op_data->tx_data.dev_index);
    *op_result = g_ra_rs_ctx_ops.ctx_cq_create(&dev_info, &op_data->tx_data.attr, &op_data_out->rx_data.info);
    if (*op_result != 0) {
        hccp_err("[init][ra_rs_cq]create failed, ret[%d] phy_id[%u] dev_index[0x%x]",
            *op_result, dev_info.phyId, dev_info.devIndex);
    }

    return 0;
}

int ra_rs_ctx_cq_destroy(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_ctx_cq_destroy_data *op_data = (union op_ctx_cq_destroy_data *)(in_buf + sizeof(struct MsgHead));
    struct RaRsDevInfo dev_info = {0};

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_ctx_cq_destroy_data), sizeof(struct MsgHead), rcv_buf_len,
        op_result);

    ra_rs_set_dev_info(&dev_info, op_data->tx_data.phy_id, op_data->tx_data.dev_index);
    *op_result = g_ra_rs_ctx_ops.ctx_cq_destroy(&dev_info, op_data->tx_data.addr);
    if (*op_result != 0) {
        hccp_err("[deinit][ra_rs_cq]destroy failed, ret[%d] phy_id[%u] dev_index[0x%x]",
            *op_result, dev_info.phyId, dev_info.devIndex);
    }

    return 0;
}

int ra_rs_ctx_qp_create(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_ctx_qp_create_data *op_data_out = (union op_ctx_qp_create_data *)(out_buf + sizeof(struct MsgHead));
    union op_ctx_qp_create_data *op_data = (union op_ctx_qp_create_data *)(in_buf + sizeof(struct MsgHead));
    struct RaRsDevInfo dev_info = {0};

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_ctx_qp_create_data), sizeof(struct MsgHead), rcv_buf_len, op_result);

    ra_rs_set_dev_info(&dev_info, op_data->tx_data.phy_id, op_data->tx_data.dev_index);
    *op_result = g_ra_rs_ctx_ops.ctx_qp_create(&dev_info, &op_data->tx_data.qp_attr, &op_data_out->rx_data.qp_info);
    if (*op_result != 0) {
        hccp_err("[init][ra_rs_qp]create failed, ret[%d] phy_id[%u] dev_index[0x%x]",
            *op_result, dev_info.phyId, dev_info.devIndex);
    }

    return 0;
}

int ra_rs_ctx_qp_query_batch(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_ctx_qp_query_batch_data *op_data = (union op_ctx_qp_query_batch_data *)(in_buf + sizeof(struct MsgHead));
    union op_ctx_qp_query_batch_data *op_data_out = (union op_ctx_qp_query_batch_data *)(out_buf +
        sizeof(struct MsgHead));
    struct RaRsDevInfo dev_info = {0};

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_ctx_qp_query_batch_data), sizeof(struct MsgHead), rcv_buf_len,
        op_result);
    HCCP_CHECK_PARAM_LEN_RET_HOST(op_data->tx_data.num, 0, HCCP_MAX_QP_QUERY_NUM, op_result);

    ra_rs_set_dev_info(&dev_info, op_data->tx_data.phy_id, op_data->tx_data.dev_index);
    op_data_out->rx_data.num = op_data->tx_data.num;
    *op_result = g_ra_rs_ctx_ops.ctx_qp_query_batch(&dev_info, op_data->tx_data.ids, op_data_out->rx_data.attr,
        &op_data_out->rx_data.num);
    if (*op_result != 0) {
        hccp_err("[qp_batch][ra_rs_ctx]query failed, ret[%d], phy_id[%u]", *op_result, op_data->tx_data.phy_id);
    }

    return 0;
}

int ra_rs_ctx_qp_destroy(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_ctx_qp_destroy_data *op_data = (union op_ctx_qp_destroy_data *)(in_buf + sizeof(struct MsgHead));
    struct RaRsDevInfo dev_info = {0};

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_ctx_qp_destroy_data), sizeof(struct MsgHead), rcv_buf_len,
        op_result);

    ra_rs_set_dev_info(&dev_info, op_data->tx_data.phy_id, op_data->tx_data.dev_index);

    *op_result = g_ra_rs_ctx_ops.ctx_qp_destroy(&dev_info, op_data->tx_data.id);
    CHK_PRT_RETURN(*op_result == -ENODEV, hccp_warn("[deinit][ra_rs_qp]jetty not found, ret[%d] phy_id[%u] "
        "dev_index[0x%x] qp_id[%u]", *op_result, dev_info.phyId, dev_info.devIndex, op_data->tx_data.id), 0);
    CHK_PRT_RETURN(*op_result != 0, hccp_err("[deinit][ra_rs_qp]destroy failed, ret[%d] phy_id[%u] dev_index[0x%x] "
        "qp_id[%u]", *op_result, dev_info.phyId, dev_info.devIndex, op_data->tx_data.id), 0);
    return 0;
}

int ra_rs_ctx_qp_destroy_batch(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_ctx_qp_destroy_batch_data *op_data_out = (union op_ctx_qp_destroy_batch_data *)(out_buf +
        sizeof(struct MsgHead));
    union op_ctx_qp_destroy_batch_data *op_data = (union op_ctx_qp_destroy_batch_data *)(in_buf +
        sizeof(struct MsgHead));
    struct RaRsDevInfo dev_info = {0};

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_ctx_qp_destroy_batch_data), sizeof(struct MsgHead), rcv_buf_len,
        op_result);
    HCCP_CHECK_PARAM_LEN_RET_HOST(op_data->tx_data.num, 0, HCCP_MAX_QP_DESTROY_BATCH_NUM, op_result);

    ra_rs_set_dev_info(&dev_info, op_data->tx_data.phy_id, op_data->tx_data.dev_index);
    op_data_out->rx_data.num = op_data->tx_data.num;
    *op_result = g_ra_rs_ctx_ops.ctx_qp_destroy_batch(&dev_info, op_data->tx_data.ids, &op_data_out->rx_data.num);
    if (*op_result != 0) {
        hccp_err("[qp_batch][ra_rs_ctx]destroy failed, ret[%d], phy_id[%u]", *op_result, op_data->tx_data.phy_id);
    }

    return 0;
}

int ra_rs_ctx_qp_import(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_ctx_qp_import_data *op_data_out = (union op_ctx_qp_import_data *)(out_buf + sizeof(struct MsgHead));
    union op_ctx_qp_import_data *op_data = (union op_ctx_qp_import_data *)(in_buf + sizeof(struct MsgHead));
    struct rs_jetty_import_attr import_attr = {0};
    struct rs_jetty_import_info import_info = {0};
    struct RaRsDevInfo dev_info = {0};

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_ctx_qp_import_data), sizeof(struct MsgHead), rcv_buf_len, op_result);

    ra_rs_set_dev_info(&dev_info, op_data->tx_data.phy_id, op_data->tx_data.dev_index);
    import_attr.key = op_data->tx_data.key;
    import_attr.attr = op_data->tx_data.attr;
    *op_result = g_ra_rs_ctx_ops.ctx_qp_import(&dev_info, &import_attr, &import_info);
    if (*op_result != 0) {
        hccp_err("[init][ra_rs_qp]import failed, ret[%d] phy_id[%u] dev_index[0x%x]",
            *op_result, dev_info.phyId, dev_info.devIndex);
    }

    op_data_out->rx_data.rem_jetty_id = import_info.rem_jetty_id;
    op_data_out->rx_data.info = import_info.info;

    return 0;
}

int ra_rs_ctx_qp_unimport(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_ctx_qp_unimport_data *op_data = (union op_ctx_qp_unimport_data *)(in_buf + sizeof(struct MsgHead));
    struct RaRsDevInfo dev_info = {0};

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_ctx_qp_unimport_data), sizeof(struct MsgHead), rcv_buf_len,
        op_result);

    ra_rs_set_dev_info(&dev_info, op_data->tx_data.phy_id, op_data->tx_data.dev_index);
    *op_result = g_ra_rs_ctx_ops.ctx_qp_unimport(&dev_info, op_data->tx_data.rem_jetty_id);
    if (*op_result != 0) {
        hccp_err("[deinit][ra_rs_qp]unimport failed, ret[%d] phy_id[%u] dev_index[0x%x] rem_qp_id[%u]",
            *op_result, dev_info.phyId, dev_info.devIndex, op_data->tx_data.rem_jetty_id);
    }

    return 0;
}

int ra_rs_ctx_qp_bind(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_ctx_qp_bind_data *op_data = (union op_ctx_qp_bind_data *)(in_buf + sizeof(struct MsgHead));
    struct rs_ctx_qp_info remote_qp_info = {0};
    struct rs_ctx_qp_info local_qp_info = {0};
    struct RaRsDevInfo dev_info = {0};

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_ctx_qp_bind_data), sizeof(struct MsgHead), rcv_buf_len, op_result);

    ra_rs_set_dev_info(&dev_info, op_data->tx_data.phy_id, op_data->tx_data.dev_index);
    local_qp_info.id = op_data->tx_data.id;
    local_qp_info.key = op_data->tx_data.local_qp_key;
    remote_qp_info.id = op_data->tx_data.rem_id;
    remote_qp_info.key = op_data->tx_data.remote_qp_key;
    *op_result = g_ra_rs_ctx_ops.ctx_qp_bind(&dev_info, &local_qp_info, &remote_qp_info);
    if (*op_result != 0) {
        hccp_err("[init][ra_rs_qp]bind failed, ret[%d] phy_id[%u] dev_index[0x%x] qp_id[%u] rem_qp_id[%u]",
            *op_result, dev_info.phyId, dev_info.devIndex, op_data->tx_data.id, op_data->tx_data.rem_id);
    }

    return 0;
}

int ra_rs_ctx_qp_unbind(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_ctx_qp_unbind_data *op_data = (union op_ctx_qp_unbind_data *)(in_buf + sizeof(struct MsgHead));
    struct RaRsDevInfo dev_info = {0};

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_ctx_qp_unbind_data), sizeof(struct MsgHead), rcv_buf_len, op_result);

    ra_rs_set_dev_info(&dev_info, op_data->tx_data.phy_id, op_data->tx_data.dev_index);

    *op_result = g_ra_rs_ctx_ops.ctx_qp_unbind(&dev_info, op_data->tx_data.id);
    CHK_PRT_RETURN(*op_result == -ENODEV, hccp_warn("[deinit][ra_rs_qp]jetty not found, ret[%d] phy_id[%u] "
        "dev_index[0x%x] qp_id[%u]", *op_result, dev_info.phyId, dev_info.devIndex, op_data->tx_data.id), 0);
    CHK_PRT_RETURN(*op_result != 0, hccp_err("[deinit][ra_rs_qp]unbind failed, ret[%d] phy_id[%u] dev_index[0x%x] "
        "qp_id[%u]", *op_result, dev_info.phyId, dev_info.devIndex, op_data->tx_data.id), 0);
    return 0;
}

int ra_rs_ctx_batch_send_wr(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_ctx_batch_send_wr_data *op_data = (union op_ctx_batch_send_wr_data *)(in_buf + sizeof(struct MsgHead));
    union op_ctx_batch_send_wr_data *op_data_out = (union op_ctx_batch_send_wr_data *)(out_buf +
        sizeof(struct MsgHead));
    struct WrlistSendCompleteNum wrlist_num = {0};

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_ctx_batch_send_wr_data), sizeof(struct MsgHead), rcv_buf_len,
        op_result);

    wrlist_num.sendNum = op_data->tx_data.send_num;
    wrlist_num.completeNum = &op_data_out->rx_data.complete_num;
    *op_result = g_ra_rs_ctx_ops.ctx_batch_send_wr(&op_data->tx_data.base_info, op_data->tx_data.wr_data,
        op_data_out->rx_data.wr_resp, &wrlist_num);
    if (*op_result != 0) {
        hccp_err("[send][ra_rs_ctx]batch send wr failed, ret[%d] qp_id[%u] send_num[%d] complete_num[%d]",
            *op_result, op_data->tx_data.base_info.qpn, wrlist_num.sendNum, *wrlist_num.completeNum);
    }

    return 0;
}

int ra_rs_ctx_update_ci(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_ctx_update_ci_data *op_data = (union op_ctx_update_ci_data *)(in_buf + sizeof(struct MsgHead));
    struct RaRsDevInfo dev_info = {0};

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_ctx_update_ci_data), sizeof(struct MsgHead), rcv_buf_len, op_result);

    ra_rs_set_dev_info(&dev_info, op_data->tx_data.phy_id, op_data->tx_data.dev_index);
    *op_result = g_ra_rs_ctx_ops.ctx_update_ci(&dev_info, op_data->tx_data.jetty_id, op_data->tx_data.ci);
    if (*op_result != 0) {
        hccp_err("[update_ci][ra_rs_ctx]update ci failed, ret[%d] phy_id[%u] dev_index[0x%x] qp_id[%u]",
            *op_result, dev_info.phyId, dev_info.devIndex, op_data->tx_data.jetty_id);
    }

    return 0;
}

int ra_rs_custom_channel(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_custom_channel_data *op_data_out = (union op_custom_channel_data *)(out_buf + sizeof(struct MsgHead));
    union op_custom_channel_data *op_data = (union op_custom_channel_data *)(in_buf + sizeof(struct MsgHead));

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_custom_channel_data), sizeof(struct MsgHead), rcv_buf_len,
        op_result);

    *op_result = g_ra_rs_ctx_ops.ccu_custom_channel(&op_data->tx_data.info, &op_data_out->rx_data.info);
    if (*op_result != 0) {
        hccp_err("[ccu]custom channel failed, ret[%d], phy_id[%u]", *op_result, op_data->tx_data.phy_id);
    }

    return 0;
}

int ra_rs_ctx_get_aux_info(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_ctx_get_aux_info_data *op_data_out = (union op_ctx_get_aux_info_data *)(out_buf + sizeof(struct MsgHead));
    union op_ctx_get_aux_info_data *op_data = (union op_ctx_get_aux_info_data *)(in_buf + sizeof(struct MsgHead));
    struct RaRsDevInfo dev_info = {0};

    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_ctx_get_aux_info_data), sizeof(struct MsgHead), rcv_buf_len,
        op_result);

    ra_rs_set_dev_info(&dev_info, op_data->tx_data.phy_id, op_data->tx_data.dev_index);
    *op_result = g_ra_rs_ctx_ops.ctx_get_aux_info(&dev_info, &op_data->tx_data.info, &op_data_out->rx_data.info);
    if (*op_result != 0) {
        hccp_err("[get_aux_info][ra_rs_ctx]get aux info failed, ret[%d] phy_id[%u] dev_index[0x%x]", *op_result,
            dev_info.phyId, dev_info.devIndex);
    }

    return 0;
}

int ra_rs_ctx_get_cr_err_info_list(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len)
{
    union op_ctx_get_cr_err_info_list_data *op_data_out =
        (union op_ctx_get_cr_err_info_list_data *)(out_buf + sizeof(struct MsgHead));
    union op_ctx_get_cr_err_info_list_data *op_data =
        (union op_ctx_get_cr_err_info_list_data *)(in_buf + sizeof(struct MsgHead));
    struct RaRsDevInfo dev_info = {0};
 
    HCCP_CHECK_PARAM_LEN_RET_HOST(sizeof(union op_ctx_get_cr_err_info_list_data), sizeof(struct MsgHead), rcv_buf_len,
        op_result);
    HCCP_CHECK_PARAM_LEN_RET_HOST(op_data->tx_data.num, 0, CR_ERR_INFO_MAX_NUM, op_result);
 
    ra_rs_set_dev_info(&dev_info, op_data->tx_data.phy_id, op_data->tx_data.dev_index);
    op_data_out->rx_data.num = op_data->tx_data.num;
    *op_result = g_ra_rs_ctx_ops.ctx_get_cr_err_info_list(&dev_info, op_data_out->rx_data.info_list,
        &op_data_out->rx_data.num);
    if (*op_result != 0) {
        hccp_err("[get][cr_err]ctx_get_cr_err_info_list failed, ret[%d], phy_id[%u]", *op_result,
            op_data->tx_data.phy_id);
    }
 
    return 0;
}

/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024. All rights reserved.
 * Description: ra hdc ctx
 * Create: 2024-09-13
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
#include "hccp_ctx.h"
#include "ra.h"
#include "ra_comm.h"
#include "ra_ctx.h"
#include "ra_ctx_comm.h"
#include "ra_rs_err.h"
#include "ra_hdc.h"
#include "ra_hdc_ctx.h"

int ra_hdc_get_dev_eid_info_num(struct RaInfo info, unsigned int *num)
{
    union op_get_dev_eid_info_num_data op_data = {0};
    int ret;

    *num = 0;
    op_data.tx_data.phy_id = info.phyId;
    ret = RaHdcProcessMsg(RA_RS_GET_DEV_EID_INFO_NUM, info.phyId, (char *)&op_data,
        sizeof(union op_get_dev_eid_info_num_data));
    CHK_PRT_RETURN(ret != 0, hccp_err("[get][eid]hdc message process failed ret[%d], phy_id[%u]",
        ret, info.phyId), ret);

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
    ret = RaHdcProcessMsg(RA_RS_GET_DEV_EID_INFO_LIST, phy_id, (char *)&op_data,
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
    unsigned int phy_id = ctx_handle->attr.phy_id;
    union op_ctx_init_data op_data = {0};
    int ret;

    (void)memcpy_s(&(op_data.tx_data.attr), sizeof(struct ctx_init_attr), attr, sizeof(struct ctx_init_attr));
    ret = RaHdcProcessMsg(RA_RS_CTX_INIT, attr->phy_id, (char *)&op_data, sizeof(union op_ctx_init_data));
    CHK_PRT_RETURN(ret != 0, hccp_err("[init][ra_hdc_ctx]hdc message process failed ret[%d], phy_id[%u]",
        ret, phy_id), ret);

    *dev_index = op_data.rx_data.dev_index;
    (void)memcpy_s(dev_attr, sizeof(struct dev_base_attr), &op_data.rx_data.dev_attr, sizeof(struct dev_base_attr));

    return 0;
}

int ra_hdc_ctx_get_async_events(struct ra_ctx_handle *ctx_handle, struct async_event events[], unsigned int *num)
{
    union op_ctx_get_async_events_data op_data = {0};
    unsigned int phy_id = ctx_handle->attr.phy_id;
    unsigned int expected_num = *num;
    unsigned int i;
    int ret = 0;

    op_data.tx_data.phy_id = phy_id;
    op_data.tx_data.dev_index = ctx_handle->dev_index;
    op_data.tx_data.num = *num;
    ret = RaHdcProcessMsg(RA_RS_CTX_GET_ASYNC_EVENTS, phy_id, (char *)&op_data,
        sizeof(union op_ctx_get_async_events_data));

    CHK_PRT_RETURN(op_data.rx_data.num > expected_num, hccp_err("[get][async_events]rx_data.num:%u > expected_num:%u,"
        " phy_id:%u dev_index:0x%x", op_data.rx_data.num, expected_num, phy_id, ctx_handle->dev_index), -EINVAL);

    for (i = 0; i < op_data.rx_data.num; i++) {
        (void)memcpy_s(&events[i], sizeof(struct async_event), &op_data.rx_data.events[i], sizeof(struct async_event));
    }
    *num = op_data.rx_data.num;

    CHK_PRT_RETURN(ret != 0, hccp_err("[get][async_events]hdc message process failed ret:%d, phy_id:%u"
        " dev_index:0x%x", ret, phy_id, ctx_handle->dev_index), ret);

    return ret;
}

int ra_hdc_ctx_deinit(struct ra_ctx_handle *ctx_handle)
{
    unsigned int phy_id = ctx_handle->attr.phy_id;
    union op_ctx_deinit_data op_data = {0};
    int ret;

    op_data.tx_data.phy_id = phy_id;
    op_data.tx_data.dev_index = ctx_handle->dev_index;
    ret = RaHdcProcessMsg(RA_RS_CTX_DEINIT, phy_id, (char *)&op_data, sizeof(union op_ctx_deinit_data));
    CHK_PRT_RETURN(ret, hccp_err("[deinit][ra_hdc_ctx]hdc message process failed ret[%d], phy_id[%u] dev_index[%u]",
        ret, phy_id, ctx_handle->dev_index), ret);

    return 0;
}

void ra_hdc_prepare_get_eid_by_ip(struct ra_ctx_handle *ctx_handle, struct IpInfo ip[], unsigned int ip_num,
    union op_get_eid_by_ip_data *op_data)
{
    unsigned int i = 0;

    op_data->tx_data.phy_id = ctx_handle->attr.phy_id;
    op_data->tx_data.dev_index = ctx_handle->dev_index;
    op_data->tx_data.num = ip_num;
    for (i = 0; i < ip_num; i++) {
        (void)memcpy_s(&op_data->tx_data.ip[i], sizeof(struct IpInfo), &ip[i], sizeof(struct IpInfo));
    }
}

int ra_hdc_get_eid_results(union op_get_eid_by_ip_data *op_data, unsigned int ip_num, union hccp_eid eid[],
    unsigned int *num)
{
    unsigned int i = 0;

    *num = 0;
    CHK_PRT_RETURN(op_data->rx_data.num > ip_num, hccp_err("[get][eid_by_ip]rx_data.num:%u > ip_num:%u",
        op_data->rx_data.num, ip_num), -EINVAL);

    *num = op_data->rx_data.num;
    for (i = 0; i < op_data->rx_data.num; i++) {
        (void)memcpy_s(&eid[i], sizeof(union hccp_eid), &op_data->rx_data.eid[i], sizeof(union hccp_eid));
    }

    return 0;
}

int ra_hdc_get_eid_by_ip(struct ra_ctx_handle *ctx_handle, struct IpInfo ip[], union hccp_eid eid[], unsigned int *num)
{
    unsigned int phy_id = ctx_handle->attr.phy_id;
    union op_get_eid_by_ip_data op_data = {0};
    unsigned int ip_num = *num;
    int ret_tmp = 0;
    int ret = 0;

    ra_hdc_prepare_get_eid_by_ip(ctx_handle, ip, ip_num, &op_data);
    ret = RaHdcProcessMsg(RA_RS_GET_EID_BY_IP, phy_id, (char *)&op_data, sizeof(union op_get_eid_by_ip_data));

    ret_tmp = ra_hdc_get_eid_results(&op_data, ip_num, eid, num);
    CHK_PRT_RETURN(ret_tmp != 0, hccp_err("[get][eid_by_ip]ra_hdc_get_eid_results failed ret[%d], phy_id[%u]"
        " dev_index[0x%x]", ret_tmp, phy_id, ctx_handle->dev_index), ret_tmp);

    CHK_PRT_RETURN(ret != 0, hccp_err("[get][eid_by_ip]hdc message process failed ret[%d], phy_id[%u]"
        " dev_index[0x%x]", ret, phy_id, ctx_handle->dev_index), ret);

    return ret;
}

int ra_hdc_ctx_token_id_alloc(struct ra_ctx_handle *ctx_handle, struct hccp_token_id *info,
    struct ra_token_id_handle *token_id_handle)
{
    unsigned int phy_id = ctx_handle->attr.phy_id;
    union op_token_id_alloc_data op_data = {0};
    int ret;

    op_data.tx_data.phy_id = phy_id;
    op_data.tx_data.dev_index = ctx_handle->dev_index;

    ret = RaHdcProcessMsg(RA_RS_CTX_TOKEN_ID_ALLOC, phy_id, (char *)&op_data,
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
    ret = RaHdcProcessMsg(RA_RS_CTX_TOKEN_ID_FREE, phy_id, (char *)&op_data,
        sizeof(union op_token_id_free_data));
    CHK_PRT_RETURN(ret != 0, hccp_err("[deinit][ra_token_id]hdc message process failed ret[%d], phy_id[%u] "
        "dev_index[%u]", ret, phy_id, ctx_handle->dev_index), ret);

    return 0;
}

int ra_hdc_ctx_prepare_lmem_register(struct ra_ctx_handle *ctx_handle, struct mr_reg_info_t *lmem_info,
    union op_lmem_reg_info_data *op_data)
{
    struct mem_reg_attr_t *mem_attr = NULL;
    int ret = 0;

    mem_attr = &(op_data->tx_data.mem_attr);
    op_data->tx_data.phy_id = ctx_handle->attr.phy_id;
    op_data->tx_data.dev_index = ctx_handle->dev_index;
    ret = ra_ctx_prepare_lmem_register(lmem_info, mem_attr);
    CHK_PRT_RETURN(ret != 0, hccp_err("[init][ra_hdc_lmem]ra_ctx_prepare_lmem_register failed, ret[%d]", ret), ret);

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

    ret = RaHdcProcessMsg(RA_RS_LMEM_REG, phy_id, (char *)&op_data, sizeof(union op_lmem_reg_info_data));
    CHK_PRT_RETURN(ret, hccp_err("[init][ra_hdc_lmem]hdc message process failed ret[%d], phy_id[%u] dev_index[%u]",
        ret, phy_id, ctx_handle->dev_index), ret);

    ra_ctx_get_lmem_info(&(op_data.rx_data.mem_info), lmem_info, lmem_handle);

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
    ret = RaHdcProcessMsg(RA_RS_LMEM_UNREG, phy_id, (char *)&op_data, sizeof(union op_lmem_unreg_info_data));
    CHK_PRT_RETURN(ret, hccp_err("[deinit][ra_hdc_lmem]hdc message process failed ret[%d], phy_id[%u] dev_index[%u]",
        ret, phy_id, ctx_handle->dev_index), ret);

    return 0;
}

STATIC void ra_hdc_prepare_rmem_import(struct ra_ctx_handle *ctx_handle, union op_rmem_import_info_data *op_data,
    struct mr_import_info_t *rmem_info)
{
    struct mem_import_attr_t *mem_attr = NULL;

    mem_attr = &(op_data->tx_data.mem_attr);
    op_data->tx_data.phy_id = ctx_handle->attr.phy_id;
    op_data->tx_data.dev_index = ctx_handle->dev_index;
    ra_ctx_prepare_rmem_import(rmem_info, mem_attr);
}

int ra_hdc_ctx_rmem_import(struct ra_ctx_handle *ctx_handle, struct mr_import_info_t *rmem_info)
{
    unsigned int phy_id = ctx_handle->attr.phy_id;
    union op_rmem_import_info_data op_data = {0};
    int ret;

    ra_hdc_prepare_rmem_import(ctx_handle, &op_data, rmem_info);

    ret = RaHdcProcessMsg(RA_RS_RMEM_IMPORT, phy_id, (char *)&op_data, sizeof(union op_rmem_import_info_data));
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
    ret = RaHdcProcessMsg(RA_RS_RMEM_UNIMPORT, phy_id, (char *)&op_data, sizeof(union op_rmem_unimport_info_data));
    CHK_PRT_RETURN(ret, hccp_err("[deinit][ra_hdc_rmem]hdc message process failed ret[%d], phy_id[%u], dev_index[%u]",
        ret, phy_id, ctx_handle->dev_index), ret);

    return 0;
}

int ra_hdc_ctx_chan_create(struct ra_ctx_handle *ctx_handle, struct chan_info_t *chan_info,
    struct ra_chan_handle *chan_handle)
{
    unsigned int phy_id = ctx_handle->attr.phy_id;
    union op_ctx_chan_create_data op_data = {0};
    int ret;

    op_data.tx_data.phy_id = phy_id;
    op_data.tx_data.dev_index = ctx_handle->dev_index;
    op_data.tx_data.data_plane_flag = chan_info->in.data_plane_flag;
    ret = RaHdcProcessMsg(RA_RS_CTX_CHAN_CREATE, phy_id, (char *)&op_data, sizeof(union op_ctx_chan_create_data));
    CHK_PRT_RETURN(ret, hccp_err("[init][ctx_chan]hdc message process failed ret[%d], phy_id[%u], dev_index[%u]",
        ret, phy_id, ctx_handle->dev_index), ret);

    chan_handle->addr = op_data.rx_data.addr;
    chan_info->out.fd = op_data.rx_data.fd;
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

    ret = RaHdcProcessMsg(RA_RS_CTX_CHAN_DESTROY, phy_id, (char *)&op_data, sizeof(union op_ctx_chan_destroy_data));
    CHK_PRT_RETURN(ret, hccp_err("[deinit][ctx_chan]hdc message process failed ret[%d], phy_id[%u], dev_index[%u]",
        ret, phy_id, ctx_handle->dev_index), ret);

    return 0;
}

STATIC void ra_hdc_prepare_cq_create(struct ra_ctx_handle *ctx_handle, union op_ctx_cq_create_data *op_data,
    struct cq_info_t *info)
{
    struct ctx_cq_attr *cq_attr = NULL;

    cq_attr = &(op_data->tx_data.attr);
    op_data->tx_data.phy_id = ctx_handle->attr.phy_id;
    op_data->tx_data.dev_index = ctx_handle->dev_index;
    ra_ctx_prepare_cq_create(info, cq_attr);
    if (op_data->tx_data.attr.ub.mode == JFC_MODE_CCU_POLL && info->in.ub.ccu_ex_cfg.valid) {
        op_data->tx_data.attr.ub.ccu_ex_cfg.valid = info->in.ub.ccu_ex_cfg.valid;
        op_data->tx_data.attr.ub.ccu_ex_cfg.cqe_flag = info->in.ub.ccu_ex_cfg.cqe_flag;
    }
}

int ra_hdc_ctx_cq_create(struct ra_ctx_handle *ctx_handle, struct cq_info_t *info, struct ra_cq_handle *cq_handle)
{
    unsigned int phy_id = ctx_handle->attr.phy_id;
    union op_ctx_cq_create_data op_data = {0};
    int ret;

    ra_hdc_prepare_cq_create(ctx_handle, &op_data, info);

    ret = RaHdcProcessMsg(RA_RS_CTX_CQ_CREATE, phy_id, (char *)&op_data, sizeof(union op_ctx_cq_create_data));
    CHK_PRT_RETURN(ret, hccp_err("[init][ra_cq]hdc message process failed ret[%d], phy_id[%u], dev_index[%u]",
        ret, phy_id, ctx_handle->dev_index), ret);

    cq_handle->addr = op_data.rx_data.info.addr;
    ra_ctx_get_cq_create_info(&op_data.rx_data.info, info);
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
    ret = RaHdcProcessMsg(RA_RS_CTX_CQ_DESTROY, phy_id, (char *)&op_data, sizeof(union op_ctx_cq_destroy_data));
    CHK_PRT_RETURN(ret, hccp_err("[deinit][ra_cq]hdc message process failed ret[%d], phy_id[%u], dev_index[%u]",
        ret, phy_id, ctx_handle->dev_index), ret);

    return 0;
}

int ra_hdc_ctx_prepare_qp_create(struct ra_ctx_handle *ctx_handle, struct qp_create_attr *qp_attr,
    union op_ctx_qp_create_data *op_data)
{
    struct ctx_qp_attr *ctx_qp_attr = NULL;
    int ret = 0;

    op_data->tx_data.phy_id = ctx_handle->attr.phy_id;
    op_data->tx_data.dev_index = ctx_handle->dev_index;
    ctx_qp_attr = &op_data->tx_data.qp_attr;
    ret = ra_ctx_prepare_qp_create(qp_attr, ctx_qp_attr);
    CHK_PRT_RETURN(ret != 0, hccp_err("[init][ra_hdc_qp]ra_ctx_prepare_qp_create failed, ret[%d]", ret), ret);

    if (ctx_qp_attr->ub.mode == JETTY_MODE_CCU_TA_CACHE) {
        ctx_qp_attr->ub.ta_cache_mode.lock_flag = qp_attr->ub.ta_cache_mode.lock_flag;
        ctx_qp_attr->ub.ta_cache_mode.sqe_buf_idx = qp_attr->ub.ta_cache_mode.sqe_buf_idx;
    } else {
        ctx_qp_attr->ub.ext_mode.sq = qp_attr->ub.ext_mode.sq;
        ctx_qp_attr->ub.ext_mode.pi_type = qp_attr->ub.ext_mode.pi_type;
        ctx_qp_attr->ub.ext_mode.cstm_flag = qp_attr->ub.ext_mode.cstm_flag;
        ctx_qp_attr->ub.ext_mode.sqebb_num = qp_attr->ub.ext_mode.sqebb_num;
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

    ret = RaHdcProcessMsg(RA_RS_CTX_QP_CREATE, phy_id, (char *)&op_data, sizeof(union op_ctx_qp_create_data));
    CHK_PRT_RETURN(ret, hccp_err("[init][ra_hdc_qp]hdc message process failed ret[%d], phy_id[%u] dev_index[%u]",
        ret, phy_id, dev_index), ret);

    ra_ctx_get_qp_create_info(ctx_handle, qp_attr, &(op_data.rx_data.qp_info), qp_handle);
    (void)memcpy_s(qp_info, sizeof(struct qp_create_info), &(op_data.rx_data.qp_info), sizeof(struct qp_create_info));

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

    ret = RaHdcProcessMsg(RA_RS_CTX_QP_DESTROY, phy_id, (char *)&op_data, sizeof(union op_ctx_qp_destroy_data));
    CHK_PRT_RETURN(ret == -ENODEV, hccp_warn("[deinit][ra_hdc_qp]hdc message process ret[%d], phy_id[%u] dev_index[%u]",
        ret, phy_id, qp_handle->dev_index), ret);
    CHK_PRT_RETURN(ret != 0, hccp_err("[deinit][ra_hdc_qp]hdc message process failed ret[%d], phy_id[%u] dev_index[%u]",
        ret, phy_id, qp_handle->dev_index), ret);
    return 0;
}

int ra_hdc_ctx_prepare_qp_import(struct ra_ctx_handle *ctx_handle, struct qp_import_info_t *qp_info,
    union op_ctx_qp_import_data *op_data)
{
    struct ra_rs_jetty_import_attr *import_attr = NULL;

    import_attr = &(op_data->tx_data.attr);
    op_data->tx_data.dev_index = ctx_handle->dev_index;
    op_data->tx_data.phy_id = ctx_handle->attr.phy_id;
    op_data->tx_data.key = qp_info->in.key;
    ra_ctx_prepare_qp_import(qp_info, import_attr);

    return 0;
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

    ret = RaHdcProcessMsg(RA_RS_CTX_QP_IMPORT, phy_id, (char *)&op_data, sizeof(union op_ctx_qp_import_data));
    CHK_PRT_RETURN(ret, hccp_err("[init][ra_hdc_qp]hdc message process failed ret[%d], phy_id[%u] dev_index[%u]",
        ret, phy_id, dev_index), ret);

    ra_ctx_get_qp_import_info(ctx_handle, qp_info, &(op_data.rx_data.info), rem_qp_handle);
    rem_qp_handle->id = op_data.rx_data.rem_jetty_id;

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
    ret = RaHdcProcessMsg(RA_RS_CTX_QP_UNIMPORT, phy_id, (char *)&op_data, sizeof(union op_ctx_qp_unimport_data));
    CHK_PRT_RETURN(ret, hccp_err("[deinit][ra_qp]hdc message process failed ret[%d], phy_id[%u] dev_index[%u]",
        ret, phy_id, rem_qp_handle->dev_index), ret);

    return 0;
}

int ra_hdc_ctx_qp_bind(struct ra_ctx_qp_handle *qp_handle, struct ra_ctx_rem_qp_handle *rem_qp_handle)
{
    unsigned int dev_index = qp_handle->dev_index;
    unsigned int phy_id = qp_handle->phy_id;
    union op_ctx_qp_bind_data op_data = {0};
    int ret;

    op_data.tx_data.phy_id = qp_handle->phy_id;
    op_data.tx_data.dev_index = qp_handle->dev_index;
    op_data.tx_data.id = qp_handle->id;
    op_data.tx_data.rem_id = rem_qp_handle->id;

    ret = RaHdcProcessMsg(RA_RS_CTX_QP_BIND, phy_id, (char *)&op_data, sizeof(union op_ctx_qp_bind_data));
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

    ret = RaHdcProcessMsg(RA_RS_CTX_QP_UNBIND, phy_id, (char *)&op_data, sizeof(union op_ctx_qp_unbind_data));
    CHK_PRT_RETURN(ret == -ENODEV, hccp_warn("[deinit][ra_qp]hdc message process ret[%d], phy_id[%u] dev_index[%u]",
        ret, phy_id, qp_handle->dev_index), ret);
    CHK_PRT_RETURN(ret != 0, hccp_err("[deinit][ra_qp]hdc message process failed ret[%d], phy_id[%u] dev_index[%u]",
        ret, phy_id, qp_handle->dev_index), ret);
    return 0;
}

STATIC int ra_hdc_send_wr_data_protocol_init(struct send_wr_data *wr, struct batch_send_wr_data *wr_data,
    enum ProtocolTypeT protocol, bool *is_inline)
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
        ret = RaHdcProcessMsg(RA_RS_CTX_BATCH_SEND_WR, qp_handle->phy_id, (char *)&op_data,
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

    ret = RaHdcProcessMsg(RA_RS_CTX_UPDATE_CI, phy_id, (char *)&op_data, sizeof(union op_ctx_update_ci_data));
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

    ret = RaHdcProcessMsg(RA_RS_CUSTOM_CHANNEL, phy_id, (char *)&op_data, sizeof(union op_custom_channel_data));
    CHK_PRT_RETURN(ret != 0, hccp_err("[custom]hdc message process failed ret[%d], phy_id[%u]", ret, phy_id), ret);

    (void)memcpy_s(out, sizeof(struct custom_chan_info_out), &op_data.rx_data.info,
        sizeof(struct custom_chan_info_out));
    return 0;
}

int ra_hdc_ctx_qp_query_batch(unsigned int phy_id, unsigned int dev_index, unsigned int ids[],
    struct jetty_attr attr[], unsigned int *num)
{
    union op_ctx_qp_query_batch_data op_data = {0};
    int ret, ret_tmp;

    op_data.tx_data.phy_id = phy_id;
    op_data.tx_data.dev_index = dev_index;
    op_data.tx_data.num = *num;
    (void)memcpy_s(op_data.tx_data.ids, sizeof(unsigned int) * (*num), ids, sizeof(unsigned int) * (*num));
    ret = RaHdcProcessMsg(RA_RS_CTX_QUERY_QP_BATCH, phy_id, (char *)&op_data,
        sizeof(union op_ctx_qp_query_batch_data));
    CHK_PRT_RETURN(op_data.rx_data.num > *num, hccp_err("[query][ra_qp]op_data.rx_data.num[%u] is larger than num[%u], "
        "ret[%d]", op_data.rx_data.num, *num, ret), ret);

    if(ret != 0 || op_data.rx_data.num < *num) {
        hccp_err("[query][ra_qp]hdc message process failed ret[%d], phy_id[%u], num[%u], op_data.rx_data.num[%u]",
            ret, phy_id, *num, op_data.rx_data.num);
        ret = -EOPENSRC;
    }

    ret_tmp = memcpy_s(attr, sizeof(struct jetty_attr) * (*num), op_data.rx_data.attr,
        sizeof(struct jetty_attr) * op_data.rx_data.num);
    CHK_PRT_RETURN(ret_tmp != 0, hccp_err("[query][ra_qp]memcpy_s failed, ret[%d], phy_id[%u], num[%u],"
        "op_data.rx_data.num[%u]", ret, phy_id, *num, op_data.rx_data.num), ret);

    *num = op_data.rx_data.num;
    return ret;
}

int ra_hdc_ctx_get_aux_info(struct ra_ctx_handle *ctx_handle, struct aux_info_in *in, struct aux_info_out *out)
{
    unsigned int phy_id = ctx_handle->attr.phy_id;
    union op_ctx_get_aux_info_data op_data = {0};
    int ret = 0;

    op_data.tx_data.phy_id = phy_id;
    op_data.tx_data.dev_index = ctx_handle->dev_index;
    (void)memcpy_s(&(op_data.tx_data.info), sizeof(struct aux_info_in), in, sizeof(struct aux_info_in));
    ret = RaHdcProcessMsg(RA_RS_CTX_GET_AUX_INFO, phy_id, (char *)&op_data,
        sizeof(union op_ctx_get_aux_info_data));
    CHK_PRT_RETURN(ret != 0, hccp_err("[get][aux_info]hdc message process failed ret[%d], phy_id[%u] "
        "dev_index[0x%x]", ret, phy_id, ctx_handle->dev_index), ret);

    (void)memcpy_s(&(out->aux_info_type), sizeof(unsigned int) * AUX_INFO_NUM_MAX,
        &(op_data.rx_data.info.aux_info_type), sizeof(unsigned int) * AUX_INFO_NUM_MAX);
    (void)memcpy_s(&(out->aux_info_value), sizeof(unsigned int) * AUX_INFO_NUM_MAX,
        &(op_data.rx_data.info.aux_info_value), sizeof(unsigned int) * AUX_INFO_NUM_MAX);
    out->aux_info_num = op_data.rx_data.info.aux_info_num;
    return ret;
}

int ra_hdc_ctx_get_cr_err_info_list(struct ra_ctx_handle *ctx_handle, struct CrErrInfo *info_list, unsigned int *num)
{
    union op_ctx_get_cr_err_info_list_data op_data = {0};
    unsigned int phy_id = ctx_handle->attr.phy_id;
    unsigned int expected_num = *num;
    unsigned int i;
    int ret = 0;

    op_data.tx_data.phy_id = phy_id;
    op_data.tx_data.dev_index = ctx_handle->dev_index;
    op_data.tx_data.num = *num;
    ret = RaHdcProcessMsg(RA_RS_CTX_GET_CR_ERR_INFO_LIST, phy_id, (char *)&op_data,
        sizeof(union op_ctx_get_cr_err_info_list_data));

    if (op_data.rx_data.num > expected_num) {
        hccp_err("[get][cr_err_info_list]rx_data.num(%u) > expected_num(%u), phy_id(%u) dev_index(0x%x)",
            op_data.rx_data.num, expected_num, phy_id, ctx_handle->dev_index);
        return -EINVAL;
    }

    CHK_PRT_RETURN(ret != 0, hccp_err("[get][cr_err_info_list]hdc message process failed ret[%d], phy_id[%u]"
        " dev_index[0x%x]", ret, phy_id, ctx_handle->dev_index), ret);

    for (i = 0; i < op_data.rx_data.num; i++) {
        (void)memcpy_s(&info_list[i], sizeof(struct CrErrInfo),
            &op_data.rx_data.info_list[i], sizeof(struct CrErrInfo));
    }
    *num = op_data.rx_data.num;

    return ret;
}

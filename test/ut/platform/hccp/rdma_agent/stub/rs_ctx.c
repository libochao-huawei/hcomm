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
#include "ra_rs_comm.h"
#include "ra_rs_ctx.h"
#include "ra_hdc_ctx.h"

int rs_get_dev_eid_info_num(unsigned int phyId, unsigned int *num)
{
    return 0;
}

int rs_get_dev_eid_info_list(unsigned int phyId, struct dev_eid_info info_list[], unsigned int start_index,
    unsigned int count)
{
    return 0;
}

int rs_get_eid_by_ip(struct RaRsDevInfo *dev_info, struct IpInfo ip[], unsigned int num, union hccp_eid eid[])
{
    return 0;
}

int rs_ctx_init(struct ctx_init_attr *attr, unsigned int *dev_index, struct dev_base_attr *dev_attr)
{
    return 0;
}

int rs_ctx_deinit(struct RaRsDevInfo *dev_info)
{
    return 0;
}

int rs_ctx_token_id_alloc(struct RaRsDevInfo *dev_info, unsigned long long *addr, unsigned int *token_id)
{
    return 0;
}

int rs_ctx_token_id_free(struct RaRsDevInfo *dev_info, unsigned long long addr)
{
    return 0;
}

int rs_ctx_lmem_reg(struct RaRsDevInfo *dev_info, struct mem_reg_attr_t *mem_attr, struct mem_reg_info_t *mem_info)
{
    return 0;
}

int rs_ctx_lmem_unreg(struct RaRsDevInfo *dev_info, unsigned long long addr)
{
    return 0;
}

int rs_ctx_rmem_import(struct RaRsDevInfo *dev_info, struct mem_import_attr_t *mem_attr,
    struct mem_import_info_t *mem_info)
{
    return 0;
}

int rs_ctx_rmem_unimport(struct RaRsDevInfo *dev_info, unsigned long long addr)
{
    return 0;
}

int rs_ctx_chan_create(struct RaRsDevInfo *dev_info, unsigned long long *addr)
{
    return 0;
}

int rs_ctx_chan_destroy(struct RaRsDevInfo *dev_info, unsigned long long addr)
{
    return 0;
}

int rs_ctx_cq_create(struct RaRsDevInfo *dev_info, struct ctx_cq_attr *attr,
    struct ctx_cq_info *info)
{
    return 0;
}

int rs_ctx_cq_destroy(struct RaRsDevInfo *dev_info, unsigned long long addr)
{
    return 0;
}

int rs_ctx_qp_create(struct RaRsDevInfo *dev_info, struct ctx_qp_attr *qpAttr,
    struct qp_create_info *qp_info)
{
    return 0;
}

int rs_ctx_qp_destroy(struct RaRsDevInfo *dev_info, unsigned int id)
{
    return 0;
}

int rs_ctx_qp_import(struct RaRsDevInfo *dev_info, struct rs_jetty_import_attr *import_attr,
    struct rs_jetty_import_info *import_info)
{
    return 0;
}

int rs_ctx_qp_unimport(struct RaRsDevInfo *dev_info, unsigned int rem_jetty_id)
{
    return 0;
}

int rs_ctx_qp_bind(struct RaRsDevInfo *dev_info, struct rs_ctx_qp_info *local_qp_info,
    struct rs_ctx_qp_info *remote_qp_info)
{
    return 0;
}

int rs_ctx_qp_unbind(struct RaRsDevInfo *dev_info, unsigned int qp_id)
{
    return 0;
}

int rs_ctx_batch_send_wr(struct wrlist_base_info *base_info, struct batch_send_wr_data *wr_data,
    struct send_wr_resp *wr_resp, struct WrlistSendCompleteNum *wrlist_num)
{
    return 0;
}

int rs_ctx_custom_channel(const struct channel_info_in *in, struct channel_info_out *out)
{
    return 0;
}

int rs_ctx_update_ci(struct RaRsDevInfo *dev_info, unsigned int qp_id, uint16_t ci)
{
    return 0;
}

int rs_get_tp_info_list(struct RaRsDevInfo *dev_info, struct get_tp_cfg *cfg,
    struct tp_info info_list[], unsigned int *num)
{
    return 0;
}

int rs_ctx_qp_destroy_batch(struct RaRsDevInfo *dev_info, unsigned int ids[], unsigned int *num)
{
    return 0;
}

int rs_ctx_qp_query_batch(struct RaRsDevInfo *dev_info, unsigned int ids[],
    struct jetty_attr attr[], unsigned int *num)
{
    return 0;
}

int rs_ctx_get_aux_info(struct ra_rs_dev_info *dev_info, struct aux_info_in *info_in,
    struct aux_info_out *info_out)
{
    return 0;
}

int rs_get_tp_attr(struct RaRsDevInfo *dev_info, unsigned int *attr_bitmap,
    const uint64_t tp_handle, struct tp_attr *attr)
{
    return 0;
}

int rs_set_tp_attr(struct RaRsDevInfo *dev_info, const unsigned int attr_bitmap,
    const uint64_t tp_handle, struct tp_attr *attr)
{
    return 0;
}

int rs_ctx_get_cr_err_info_list(struct RaRsDevInfo *dev_info, struct CqeErrInfo info_list[], unsigned int *num)
{
    return 0;
}

int rs_ctx_get_async_events(struct RaRsDevInfo *dev_info, struct async_event async_events[],
    unsigned int *num)
{
    return 0;
}

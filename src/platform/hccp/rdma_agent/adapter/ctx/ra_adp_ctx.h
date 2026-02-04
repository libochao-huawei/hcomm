/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef RA_ADP_CTX_H
#define RA_ADP_CTX_H

#include "hccp_async_ctx.h"
#include "ra_rs_comm.h"
#include "ra_rs_ctx.h"
#include "rs_ctx.h"

struct rs_ctx_ops {
    int (*get_dev_eid_info_num)(unsigned int phyId, unsigned int *num);
    int (*get_dev_eid_info_list)(unsigned int phyId, struct dev_eid_info info_list[], unsigned int start_index,
        unsigned int count);
    int (*ctx_init)(struct ctx_init_attr *attr, unsigned int *dev_index, struct dev_base_attr *dev_attr);
    int (*ctx_get_async_events)(struct RaRsDevInfo *dev_info, struct async_event async_events[], unsigned int *num);
    int (*ctx_deinit)(struct RaRsDevInfo *dev_info);
    int (*get_eid_by_ip)(struct RaRsDevInfo *dev_info, struct IpInfo ip[], union hccp_eid eid[], unsigned int *num);
    int (*get_tp_info_list)(struct RaRsDevInfo *dev_info, struct get_tp_cfg *cfg, struct tp_info info_list[],
        unsigned int *num);
    int (*get_tp_attr)(struct RaRsDevInfo *dev_info, unsigned int *attr_bitmap, const uint64_t tp_handle,
        struct tp_attr *attr);
    int (*set_tp_attr)(struct RaRsDevInfo *dev_info, const unsigned int attr_bitmap, const uint64_t tp_handle,
        struct tp_attr *attr);
    int (*ctx_token_id_alloc)(struct RaRsDevInfo *dev_info, unsigned long long *addr, unsigned int *token_id);
    int (*ctx_token_id_free)(struct RaRsDevInfo *dev_info, unsigned long long addr);
    int (*ctx_lmem_reg)(struct RaRsDevInfo *dev_info, struct mem_reg_attr_t *mem_attr,
        struct mem_reg_info_t *mem_info);
    int (*ctx_lmem_unreg)(struct RaRsDevInfo *dev_info, unsigned long long addr);
    int (*ctx_rmem_import)(struct RaRsDevInfo *dev_info, struct mem_import_attr_t *mem_attr,
        struct mem_import_info_t *mem_info);
    int (*ctx_rmem_unimport)(struct RaRsDevInfo *dev_info, unsigned long long addr);
    int (*ctx_chan_create)(struct RaRsDevInfo *dev_info, union data_plane_cstm_flag data_plane_flag,
        unsigned long long *addr, int *fd);
    int (*ctx_chan_destroy)(struct RaRsDevInfo *dev_info, unsigned long long addr);
    int (*ctx_cq_create)(struct RaRsDevInfo *dev_info, struct ctx_cq_attr *attr, struct ctx_cq_info *info);
    int (*ctx_cq_destroy)(struct RaRsDevInfo *dev_info, unsigned long long addr);
    int (*ctx_qp_create)(struct RaRsDevInfo *dev_info, struct ctx_qp_attr *qp_attr, struct qp_create_info *qp_info);
    int (*ctx_qp_destroy)(struct RaRsDevInfo *dev_info, unsigned int id);
    int (*ctx_qp_destroy_batch)(struct RaRsDevInfo *dev_info, unsigned int ids[], unsigned int *num);
    int (*ctx_qp_import)(struct RaRsDevInfo *dev_info, struct rs_jetty_import_attr *import_attr,
        struct rs_jetty_import_info *import_info);
    int (*ctx_qp_unimport)(struct RaRsDevInfo *dev_info, unsigned int rem_jetty_id);
    int (*ctx_qp_bind)(struct RaRsDevInfo *dev_info, struct rs_ctx_qp_info *local_qp_info,
        struct rs_ctx_qp_info *remote_qp_info);
    int (*ctx_qp_unbind)(struct RaRsDevInfo *dev_info, unsigned int qp_id);
    int (*ctx_batch_send_wr)(struct wrlist_base_info *base_info, struct batch_send_wr_data *wr_data,
        struct send_wr_resp *wr_resp, struct WrlistSendCompleteNum *wrlist_num);
    int (*ctx_update_ci)(struct RaRsDevInfo *dev_info, unsigned int qp_id, uint16_t ci);
    int (*ccu_custom_channel)(const struct custom_chan_info_in *in, struct custom_chan_info_out *out);
    int (*ctx_qp_query_batch)(struct RaRsDevInfo *dev_info, unsigned int ids[], struct jetty_attr attr[],
        unsigned int *num);
    int (*ctx_get_aux_info)(struct RaRsDevInfo *dev_info, struct aux_info_in *info_in,
        struct aux_info_out *info_out);
    int (*ctx_get_cr_err_info_list)(struct RaRsDevInfo *dev_info, struct CrErrInfo info_list[], unsigned int *num);
};

int ra_rs_get_dev_eid_info_num(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len);
int ra_rs_get_dev_eid_info_list(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len);
int ra_rs_ctx_init(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len);
int ra_rs_ctx_get_async_events(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len);
int ra_rs_ctx_deinit(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len);
int ra_rs_get_eid_by_ip(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len);
int ra_rs_get_tp_info_list(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len);
int ra_rs_get_tp_attr(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len);
int ra_rs_set_tp_attr(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len);
int ra_rs_ctx_token_id_alloc(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len);
int ra_rs_ctx_token_id_free(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len);
int ra_rs_lmem_reg(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len);
int ra_rs_lmem_unreg(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len);
int ra_rs_rmem_import(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len);
int ra_rs_rmem_unimport(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len);
int ra_rs_ctx_chan_create(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len);
int ra_rs_ctx_chan_destroy(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len);
int ra_rs_ctx_cq_create(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len);
int ra_rs_ctx_cq_destroy(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len);
int ra_rs_ctx_qp_create(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len);
int ra_rs_ctx_qp_query_batch(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len);
int ra_rs_ctx_qp_destroy(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len);
int ra_rs_ctx_qp_destroy_batch(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len);
int ra_rs_ctx_qp_import(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len);
int ra_rs_ctx_qp_unimport(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len);
int ra_rs_ctx_qp_bind(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len);
int ra_rs_ctx_qp_unbind(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len);
int ra_rs_ctx_update_ci(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len);
int ra_rs_ctx_batch_send_wr(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len);
int ra_rs_custom_channel(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len);
int ra_rs_ctx_get_aux_info(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len);
int ra_rs_ctx_get_cr_err_info_list(char *in_buf, char *out_buf, int *out_len, int *op_result, int rcv_buf_len);
#endif // RA_ADP_CTX_H

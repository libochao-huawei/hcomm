/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef RA_HDC_CTX_H
#define RA_HDC_CTX_H

#include "hccp_common.h"
#include "hccp_ctx.h"
#include "ra_rs_comm.h"
#include "ra_rs_ctx.h"
#include "ra_comm.h"
#include "ra_ctx.h"
#include "ra_hdc.h"

#define MAX_DEV_INFO_TRANS_NUM 30

union op_get_dev_eid_info_num_data {
    struct {
        unsigned int phy_id;
        unsigned int rsvd;
    } tx_data;

    struct {
        unsigned int num;
        unsigned int rsvd;
    } rx_data;
};

union op_get_dev_eid_info_list_data {
    struct {
        unsigned int phy_id;
        unsigned int start_index;
        unsigned int count;
        unsigned int rsvd;
    } tx_data;

    struct {
        struct dev_eid_info info_list[MAX_DEV_INFO_TRANS_NUM];
        unsigned int rsvd;
    } rx_data;
};

union op_ctx_init_data {
    struct {
        struct ctx_init_attr attr;
        unsigned int rsvd;
    } tx_data;

    struct {
        struct dev_base_attr dev_attr;
        unsigned int dev_index;
        unsigned int rsvd;
    } rx_data;
};

union op_ctx_get_async_events_data {
    struct {
        unsigned int phy_id;
        unsigned int dev_index;
        unsigned int num;
        unsigned int rsvd;
    } tx_data;

    struct {
        struct async_event events[ASYNC_EVENT_MAX_NUM];
        unsigned int num;
        unsigned int rsvd;
    } rx_data;
};

union op_ctx_deinit_data {
    struct {
        unsigned int phy_id;
        unsigned int dev_index;
        unsigned int rsvd;
    } tx_data;

    struct {
        unsigned int rsvd;
    } rx_data;
};

union op_get_eid_by_ip_data {
    struct {
        unsigned int phy_id;
        unsigned int dev_index;
        struct IpInfo ip[GET_EID_BY_IP_MAX_NUM];
        unsigned int num;
        unsigned int rsvd[RA_RSVD_NUM_4];
    } tx_data;

    struct {
        union hccp_eid eid[GET_EID_BY_IP_MAX_NUM];
        unsigned int num;
        unsigned int rsvd[RA_RSVD_NUM_4];
    } rx_data;
};

union op_token_id_alloc_data {
    struct {
        unsigned int phy_id;
        unsigned int dev_index;
        unsigned int rsvd[RA_RSVD_NUM_4];
    } tx_data;
    struct {
        unsigned long long addr;
        unsigned int token_id;
        unsigned int rsvd[RA_RSVD_NUM_4];
    } rx_data;
};

union op_token_id_free_data {
    struct {
        unsigned int phy_id;
        unsigned int dev_index;
        unsigned long long addr;
        unsigned int rsvd[RA_RSVD_NUM_4];
    } tx_data;
    struct {
        unsigned int rsvd[RA_RSVD_NUM_4];
    } rx_data;
};

union op_lmem_reg_info_data {
    struct {
        unsigned int phy_id;
        unsigned int dev_index;
        struct mem_reg_attr_t mem_attr;
        unsigned int rsvd;
    } tx_data;

    struct {
        struct mem_reg_info_t mem_info;
        unsigned int rsvd;
    } rx_data;
};

union op_lmem_unreg_info_data {
    struct {
        unsigned int phy_id;
        unsigned int dev_index;
        unsigned long long addr;
        unsigned int rsvd;
    } tx_data;

    struct {
        unsigned int rsvd;
    } rx_data;
};

union op_rmem_import_info_data {
    struct {
        unsigned int phy_id;
        unsigned int dev_index;
        struct mem_import_attr_t mem_attr;
        unsigned int rsvd;
    } tx_data;

    struct {
        struct mem_import_info_t mem_info;
        unsigned int rsvd;
    } rx_data;
};

union op_rmem_unimport_info_data {
    struct {
        unsigned int phy_id;
        unsigned int dev_index;
        unsigned long long addr;
        unsigned int rsvd;
    } tx_data;

    struct {
        unsigned int rsvd;
    } rx_data;
};

union op_ctx_chan_create_data {
    struct {
        unsigned int phy_id;
        unsigned int dev_index;
        union data_plane_cstm_flag data_plane_flag;
        uint32_t resv[RA_RSVD_NUM_4];
    } tx_data;
    struct {
        unsigned long long addr; /**< refer to ibv_comp_channel*, urma_jfce_t* for chan_cb index */
        int fd;
        uint32_t resv[RA_RSVD_NUM_8];
    } rx_data;
};

union op_ctx_chan_destroy_data {
    struct {
        unsigned int phy_id;
        unsigned int dev_index;
        unsigned long long addr; /**< refer to ibv_comp_channel*, urma_jfce_t* for chan_cb index */
        uint32_t resv[RA_RSVD_NUM_4];
    } tx_data;
    struct {
        uint32_t resv[RA_RSVD_NUM_4];
    } rx_data;
};

union op_ctx_cq_create_data {
    struct {
        unsigned int phy_id;
        unsigned int dev_index;
        struct ctx_cq_attr attr;
        uint32_t resv[4U];
    } tx_data;

    struct {
        struct ctx_cq_info info;
        uint32_t resv[4U];
    } rx_data;
};

union op_ctx_cq_destroy_data {
    struct {
        unsigned int phy_id;
        unsigned int dev_index;
        uint64_t addr; /**< refer to ibv_cq*, urma_jfc_t* for cq_cb index */
        uint32_t resv[4U];
    } tx_data;

    struct {
        uint32_t resv[4U];
    } rx_data;
};

union op_ctx_qp_create_data {
    struct {
        unsigned int phy_id;
        unsigned int dev_index;
        struct ctx_qp_attr qp_attr;
        unsigned int rsvd[RA_RSVD_NUM_3];
    } tx_data;

    struct {
        struct qp_create_info qp_info;
        unsigned int rsvd[RA_RSVD_NUM_3];
    } rx_data;
};

union op_ctx_qp_destroy_data {
    struct {
        unsigned int phy_id;
        unsigned int dev_index;
        unsigned int id; // qpn(rdma) or jetty_id(udma)
        unsigned int rsvd[RA_RSVD_NUM_3];
    } tx_data;

    struct {
        unsigned int rsvd;
    } rx_data;
};

union op_ctx_qp_import_data {
    struct {
        unsigned int phy_id;
        unsigned int dev_index;
        struct qp_key key;
        struct ra_rs_jetty_import_attr attr;
        unsigned int rsvd[RA_RSVD_NUM_3];
    } tx_data;

    struct {
        unsigned int rem_jetty_id; // only for ub
        struct ra_rs_jetty_import_info info;
        unsigned int rsvd[RA_RSVD_NUM_3];
    } rx_data;
};

union op_ctx_qp_unimport_data {
    struct {
        unsigned int phy_id;
        unsigned int dev_index;
        unsigned int rem_jetty_id;
        unsigned int rsvd[RA_RSVD_NUM_6];
    } tx_data;

    struct {
        unsigned int rsvd[RA_RSVD_NUM_6];
    } rx_data;
};

union op_ctx_qp_bind_data {
    struct {
        unsigned int phy_id;
        unsigned int dev_index;
        unsigned int id; // local qpn(rdma) or local jetty_id(udma)
        unsigned int rem_id; // only for UB, equivalent to rem_jetty_id
        struct qp_key local_qp_key;
        struct qp_key remote_qp_key;
        unsigned int rsvd[RA_RSVD_NUM_6];
    } tx_data;

    struct {
        unsigned int rsvd[RA_RSVD_NUM_6];
    } rx_data;
};

union op_ctx_qp_unbind_data {
    struct {
        unsigned int phy_id;
        unsigned int dev_index;
        unsigned int id; // local qpn(rdma) or local jetty_id(udma)
        unsigned int rsvd[RA_RSVD_NUM_3];
    } tx_data;

    struct {
        unsigned int rsvd;
    } rx_data;
};

union op_ctx_batch_send_wr_data {
    struct {
        struct wrlist_base_info base_info;
        unsigned int send_num;
        struct batch_send_wr_data wr_data[MAX_CTX_WR_NUM];
    } tx_data;

    struct {
        unsigned int complete_num;
        struct send_wr_resp wr_resp[MAX_CTX_WR_NUM];
    } rx_data;
};

union op_ctx_update_ci_data {
    struct {
        unsigned int phy_id;
        unsigned int dev_index;
        unsigned int jetty_id;
        uint16_t ci;
        unsigned int rsvd[RA_RSVD_NUM_4];
    } tx_data;
    struct {
        unsigned int rsvd[RA_RSVD_NUM_4];
    } rx_data;
};

union op_custom_channel_data {
    struct {
        unsigned int phy_id;
        struct custom_chan_info_in info;
        unsigned int rsvd[64U];
    } tx_data;

    struct {
        struct custom_chan_info_out info;
        unsigned int rsvd[64U];
    } rx_data;
};

union op_ctx_qp_query_batch_data {
    struct {
        unsigned int phy_id;
        unsigned int dev_index;
        unsigned int num;
        unsigned int ids[HCCP_MAX_QP_QUERY_NUM];
    } tx_data;

    struct {
        unsigned int num;
        struct jetty_attr attr[HCCP_MAX_QP_QUERY_NUM];
    } rx_data;
};

union op_ctx_get_aux_info_data {
    struct {
        unsigned int phy_id;
        unsigned int dev_index;
        struct aux_info_in info;
    } tx_data;

    struct {
        struct aux_info_out info;
    } rx_data;
};

union op_ctx_get_cr_err_info_list_data {
    struct {
        unsigned int phy_id;
        unsigned int dev_index;
        unsigned int num;
        unsigned int rsvd[RA_RSVD_NUM_4];
    } tx_data;

    struct {
        struct CrErrInfo info_list[CR_ERR_INFO_MAX_NUM];
        unsigned int num;
        unsigned int rsvd[RA_RSVD_NUM_4];
    } rx_data;
};

int ra_hdc_get_dev_eid_info_num(struct RaInfo info, unsigned int *num);
int ra_hdc_get_dev_eid_info_list(unsigned int phy_id, struct dev_eid_info info_list[], unsigned int *num);
int ra_hdc_ctx_init(struct ra_ctx_handle *ctx_handle, struct ctx_init_attr *attr, unsigned int *dev_index,
    struct dev_base_attr *dev_attr);
int ra_hdc_ctx_get_async_events(struct ra_ctx_handle *ctx_handle, struct async_event events[], unsigned int *num);
int ra_hdc_ctx_deinit(struct ra_ctx_handle *ctx_handle);
void ra_hdc_prepare_get_eid_by_ip(struct ra_ctx_handle *ctx_handle, struct IpInfo ip[], unsigned int ip_num,
    union op_get_eid_by_ip_data *op_data);
int ra_hdc_get_eid_results(union op_get_eid_by_ip_data *op_data, unsigned int ip_num, union hccp_eid eid[],
    unsigned int *num);
int ra_hdc_get_eid_by_ip(struct ra_ctx_handle *ctx_handle, struct IpInfo ip[], union hccp_eid eid[],
    unsigned int *num);
int ra_hdc_ctx_token_id_alloc(struct ra_ctx_handle *ctx_handle, struct hccp_token_id *info,
    struct ra_token_id_handle *token_id_handle);
int ra_hdc_ctx_token_id_free(struct ra_ctx_handle *ctx_handle, struct ra_token_id_handle *token_id_handle);
int ra_hdc_ctx_prepare_lmem_register(struct ra_ctx_handle *ctx_handle, struct mr_reg_info_t *lmem_info,
    union op_lmem_reg_info_data *op_data);
int ra_hdc_ctx_lmem_register(struct ra_ctx_handle *ctx_handle, struct mr_reg_info_t *lmem_info,
    struct ra_lmem_handle *lmem_handle);
int ra_hdc_ctx_lmem_unregister(struct ra_ctx_handle *ctx_handle, struct ra_lmem_handle *lmem_handle);
int ra_hdc_ctx_rmem_import(struct ra_ctx_handle *ctx_handle, struct mr_import_info_t *rmem_info);
int ra_hdc_ctx_rmem_unimport(struct ra_ctx_handle *ctx_handle, struct ra_rmem_handle *rmem_handle);
int ra_hdc_ctx_chan_create(struct ra_ctx_handle *ctx_handle, struct chan_info_t *chan_info,
    struct ra_chan_handle *chan_handle);
int ra_hdc_ctx_chan_destroy(struct ra_ctx_handle *ctx_handle, struct ra_chan_handle *chan_handle);
int ra_hdc_ctx_cq_create(struct ra_ctx_handle *ctx_handle, struct cq_info_t *info, struct ra_cq_handle *cq_handle);
int ra_hdc_ctx_cq_destroy(struct ra_ctx_handle *ctx_handle, struct ra_cq_handle *cq_handle);
int ra_hdc_ctx_prepare_qp_create(struct ra_ctx_handle *ctx_handle, struct qp_create_attr *qp_attr,
    union op_ctx_qp_create_data *op_data);
int ra_hdc_ctx_qp_create(struct ra_ctx_handle *ctx_handle, struct qp_create_attr *qp_attr,
    struct qp_create_info *qp_info, struct ra_ctx_qp_handle *qp_handle);
int ra_hdc_ctx_qp_query_batch(unsigned int phy_id, unsigned int dev_index, unsigned int ids[],
    struct jetty_attr attr[], unsigned int *num);
int ra_hdc_ctx_qp_destroy(struct ra_ctx_qp_handle *qp_handle);
int ra_hdc_ctx_prepare_qp_import(struct ra_ctx_handle *ctx_handle, struct qp_import_info_t *qp_info,
    union op_ctx_qp_import_data *op_data);
int ra_hdc_ctx_qp_import(struct ra_ctx_handle *ctx_handle, struct qp_import_info_t *qp_info,
    struct ra_ctx_rem_qp_handle *rem_qp_handle);
int ra_hdc_ctx_qp_unimport(struct ra_ctx_rem_qp_handle *rem_qp_handle);
int ra_hdc_ctx_qp_bind(struct ra_ctx_qp_handle *qp_handle, struct ra_ctx_rem_qp_handle *rem_qp_handle);
int ra_hdc_ctx_qp_unbind(struct ra_ctx_qp_handle *qp_handle);
int ra_hdc_ctx_batch_send_wr(struct ra_ctx_qp_handle *qp_handle, struct send_wr_data wr_list[],
    struct send_wr_resp op_resp[], unsigned int send_num, unsigned int *complete_num);
int ra_hdc_ctx_update_ci(struct ra_ctx_qp_handle *qp_handle, uint16_t ci);
int ra_hdc_custom_channel(unsigned int phy_id, struct custom_chan_info_in *in, struct custom_chan_info_out *out);
int ra_hdc_ctx_get_aux_info(struct ra_ctx_handle *ctx_handle, struct aux_info_in *in, struct aux_info_out *out);
int ra_hdc_ctx_get_cr_err_info_list(struct ra_ctx_handle *ctx_handle, struct CrErrInfo *info_list, unsigned int *num);
#endif // RA_HDC_CTX_H

/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef RA_CTX_H
#define RA_CTX_H

#include <pthread.h>
#include "hccp_ctx.h"
#include "ra_rs_ctx.h"

struct ra_ctx_handle {
    enum ProtocolTypeT protocol;
    struct ra_ctx_ops *ctx_ops;
    struct ctx_init_attr attr;
    unsigned int dev_index;
    struct dev_base_attr dev_attr;
    union {
        struct {
            pthread_mutex_t dev_mutex;
            bool disabled_lite_thread;
        } rdma;
    };
};

struct ra_token_id_handle {
    unsigned long long addr;
};

struct ra_lmem_handle {
    unsigned long long addr;
};

struct ra_rmem_handle {
    struct mem_key key;
    unsigned long long addr;
};

struct ra_ctx_qp_handle {
    unsigned int id; // qpn(rdma) or jetty_id(udma)
    unsigned int phy_id;
    unsigned int dev_index;
    enum ProtocolTypeT protocol;
    struct qp_create_attr qp_attr;
    struct qp_create_info qp_info;
    struct ra_ctx_handle *ctx_handle;
};

struct ra_ctx_rem_qp_handle {
    unsigned int id; // qpn(rdma) or jetty_id(udma)
    unsigned int phy_id;
    unsigned int dev_index;
    enum ProtocolTypeT protocol;
    struct qp_key qp_key; // only for rdma
};

struct ra_chan_handle {
    unsigned long long addr; /**< refer to ibv_comp_channel*, urma_jfce_t* for chan_cb index */
};

struct ra_cq_handle {
    unsigned long long addr; /**< refer to ibv_cq*, urma_jfc_t* for cq_cb index */
};

struct ra_ctx_ops {
    int (*ra_ctx_init)(struct ra_ctx_handle *ctx_handle, struct ctx_init_attr *attr, unsigned int *devIndex,
        struct dev_base_attr *dev_attr);
    int (*ra_ctx_get_async_events)(struct ra_ctx_handle *ctx_handle, struct async_event events[], unsigned int *num);
    int (*ra_ctx_deinit)(struct ra_ctx_handle *ctx_handle);
    int (*ra_ctx_get_eid_by_ip)(struct ra_ctx_handle *ctx_handle, struct IpInfo ip[], union hccp_eid eid[],
        unsigned int *num);
    int (*ra_ctx_token_id_alloc)(struct ra_ctx_handle *ctx_handle, struct hccp_token_id *info,
        struct ra_token_id_handle *token_id_handle);
    int (*ra_ctx_token_id_free)(struct ra_ctx_handle *ctx_handle, struct ra_token_id_handle *token_id_handle);
    int (*ra_ctx_lmem_register)(struct ra_ctx_handle *ctx_handle, struct mr_reg_info_t *lmem_info,
        struct ra_lmem_handle *lmem_handle);
    int (*ra_ctx_lmem_unregister)(struct ra_ctx_handle *ctx_handle, struct ra_lmem_handle *lmem_handle);
    int (*ra_ctx_rmem_import)(struct ra_ctx_handle *ctx_handle, struct mr_import_info_t *rmem_info);
    int (*ra_ctx_rmem_unimport)(struct ra_ctx_handle *ctx_handle, struct ra_rmem_handle *rmem_handle);
    int (*ra_ctx_chan_create)(struct ra_ctx_handle *ctx_handle, struct chan_info_t *chan_info,
        struct ra_chan_handle *chan_handle);
    int (*ra_ctx_chan_destroy)(struct ra_ctx_handle *ctx_handle, struct ra_chan_handle *chan_handle);
    int (*ra_ctx_cq_create)(struct ra_ctx_handle *ctx_handle, struct cq_info_t *info,
        struct ra_cq_handle *cq_handle);
    int (*ra_ctx_cq_destroy)(struct ra_ctx_handle *ctx_handle, struct ra_cq_handle *cq_handle);
    int (*ra_ctx_qp_create)(struct ra_ctx_handle *ctx_handle, struct qp_create_attr *qp_attr, struct qp_create_info *qp_info,
        struct ra_ctx_qp_handle *qp_handle);
    int (*ra_ctx_query_qp_batch)(unsigned int phyId, unsigned int devIndex, unsigned int ids[],
        struct jetty_attr attr[], unsigned int *num);
    int (*ra_ctx_qp_destroy)(struct ra_ctx_qp_handle *qp_handle);
    int (*ra_ctx_qp_import)(struct ra_ctx_handle *ctx_handle, struct qp_import_info_t *qp_import_info,
        struct ra_ctx_rem_qp_handle *rem_qp_handle);
    int (*ra_ctx_qp_unimport)(struct ra_ctx_rem_qp_handle *rem_qp_handle);
    int (*ra_ctx_qp_bind)(struct ra_ctx_qp_handle *qp_handle, struct ra_ctx_rem_qp_handle *rem_qp_handle);
    int (*ra_ctx_qp_unbind)(struct ra_ctx_qp_handle *qp_handle);
    int (*ra_ctx_batch_send_wr)(struct ra_ctx_qp_handle *qp_handle, struct send_wr_data wr_list[],
        struct send_wr_resp op_resp[], unsigned int send_num, unsigned int *complete_num);
    int (*ra_ctx_update_ci)(struct ra_ctx_qp_handle *qp_handle, uint16_t ci);
    int (*ra_ctx_get_aux_info)(struct ra_ctx_handle *ctx_handle, struct aux_info_in *in, struct aux_info_out *out);
};

#endif // RA_CTX_H

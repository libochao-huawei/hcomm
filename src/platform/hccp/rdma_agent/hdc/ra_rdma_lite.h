/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef RA_RDMA_LITE_H
#define RA_RDMA_LITE_H

#include "user_log.h"
#include "rdma_lite.h"
#include "hccp_common.h"

#define DL_ATTRI_VISI_DEF __attribute__ ((visibility ("default")))

#define DL_API_RET_IS_NULL_CHECK(p, str, release_lock)           \
    do {                                           \
        if ((p) == NULL) {                         \
            pthread_mutex_unlock(&release_lock);   \
            hccp_err("ptr is NULL!, [%s]", (str)); \
            return (-EINVAL);                      \
        }                                          \
    } while (0)

#define DL_API_PTR_IS_NULL_CHECK(p, str)           \
    do {                                           \
        if ((p) == NULL) {                         \
            hccp_warn("%s is NULL!", (str));       \
        }                                          \
    } while (0)

struct ra_rdma_lite_ops {
    struct rdma_lite_context *(*ra_rdma_lite_alloc_ctx)(u8 phy_id, struct dev_cap_info *cap);
    void (*ra_rdma_lite_free_ctx)(struct rdma_lite_context *lite_ctx);
    int (*ra_rdma_lite_init_mem_pool)(struct rdma_lite_context *lite_ctx, struct rdma_lite_mem_attr *lite_mem_attr);
    int (*ra_rdma_lite_deinit_mem_pool)(struct rdma_lite_context *lite_ctx, u32 mem_idx);
    struct rdma_lite_cq *(*ra_rdma_lite_create_cq)(
        struct rdma_lite_context *lite_ctx, struct rdma_lite_cq_attr *lite_cq_attr);
    int (*ra_rdma_lite_destroy_cq)(struct rdma_lite_cq *lite_cq);
    int (*ra_rdma_lite_poll_cq)(struct rdma_lite_cq *lite_cq, int num_entries, struct rdma_lite_wc *lite_wc);
    int (*ra_rdma_lite_poll_cq_v2)(struct rdma_lite_cq *lite_cq, int num_entries, struct rdma_lite_wc_v2 *lite_wc);
    struct rdma_lite_qp *(*ra_rdma_lite_create_qp)(
        struct rdma_lite_context *lite_ctx, struct rdma_lite_qp_attr *lite_qp_attr);
    int (*ra_rdma_lite_destroy_qp)(struct rdma_lite_qp *lite_qp);
    int (*ra_rdma_lite_post_send)(struct rdma_lite_qp *lite_qp, struct rdma_lite_send_wr *wr,
        struct rdma_lite_send_wr **bad_wr, struct rdma_lite_post_send_attr *attr,
        struct rdma_lite_post_send_resp *resp);
    int (*ra_rdma_lite_post_recv)(struct rdma_lite_qp *lite_qp, struct rdma_lite_recv_wr *wr,
        struct rdma_lite_recv_wr **bad_wr);
    int (*ra_rdma_lite_set_qp_sl)(struct rdma_lite_qp *lite_qp, int sl);
    int (*ra_rdma_lite_clean_qp)(struct rdma_lite_qp *lite_qp);
    int (*ra_rdma_lite_restore_snapshot)(struct rdma_lite_context *lite_ctx);
    unsigned int (*ra_rdma_lite_get_api_version)(void);
};

DL_ATTRI_VISI_DEF void ra_hdc_rdma_lite_api_deinit(void);
DL_ATTRI_VISI_DEF int ra_hdc_rdma_lite_api_init(void);
struct rdma_lite_context *ra_rdma_lite_alloc_ctx(u8 phy_id, struct dev_cap_info *cap);

void ra_rdma_lite_free_ctx(struct rdma_lite_context *lite_ctx);

struct rdma_lite_cq *ra_rdma_lite_create_cq(struct rdma_lite_context *lite_ctx, struct rdma_lite_cq_attr *lite_cq_attr);

int ra_rdma_lite_destroy_cq(struct rdma_lite_cq *lite_cq);

int ra_rdma_lite_poll_cq(struct rdma_lite_cq *lite_cq, int num_entries, struct rdma_lite_wc *lite_wc);
int ra_rdma_lite_poll_cq_v2(struct rdma_lite_cq *lite_cq, int num_entries, struct rdma_lite_wc_v2 *lite_wc);

struct rdma_lite_qp *ra_rdma_lite_create_qp(struct rdma_lite_context *lite_ctx, struct rdma_lite_qp_attr *lite_qp_attr);

int ra_rdma_lite_destroy_qp(struct rdma_lite_qp *lite_qp);

int ra_rdma_lite_post_send(struct rdma_lite_qp *lite_qp, struct rdma_lite_send_wr *wr,
    struct rdma_lite_send_wr **bad_wr, struct rdma_lite_post_send_attr *attr, struct rdma_lite_post_send_resp *resp);

int ra_rdma_lite_post_recv(struct rdma_lite_qp *lite_qp, struct rdma_lite_recv_wr *wr,
    struct rdma_lite_recv_wr **bad_wr);

int ra_rdma_lite_set_qp_sl(struct rdma_lite_qp *lite_qp, unsigned char sl);
int ra_rdma_lite_clean_qp(struct rdma_lite_qp *lite_qp);
int ra_rdma_lite_init_mem_pool(struct rdma_lite_context *lite_ctx, struct rdma_lite_mem_attr *lite_mem_attr);
int ra_rdma_lite_deinit_mem_pool(struct rdma_lite_context *lite_ctx, u32 mem_idx);
int ra_rdma_lite_restore_snapshot(struct rdma_lite_context *lite_ctx);
unsigned int ra_rdma_lite_get_api_version(void);
#endif // RA_RDMA_LITE_H

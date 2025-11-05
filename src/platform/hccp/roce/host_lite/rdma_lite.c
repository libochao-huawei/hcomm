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
#include "user_log.h"
#include "rdma_lite.h"

struct rdma_lite_ops *get_hns_roce_lite_ops(void)
{
    return &g_hns_roce_lite_ops;
}

struct rdma_lite_context *rdma_lite_alloc_context(u8 phy_id, struct dev_cap_info *cap)
{
    struct rdma_lite_ops *hns_roce_lite_ops = get_hns_roce_lite_ops();
    struct rdma_lite_context *lite_ctx;

    lite_ctx = hns_roce_lite_ops->rdma_lite_alloc_context(phy_id, cap);
    if (lite_ctx == NULL) {
        return NULL;
    }

    lite_ctx->cap = *cap;

    return lite_ctx;
}
void rdma_lite_free_context(struct rdma_lite_context *lite_ctx)
{
    struct rdma_lite_ops *hns_roce_lite_ops = get_hns_roce_lite_ops();

    hns_roce_lite_ops->rdma_lite_free_context(lite_ctx);
}

struct rdma_lite_cq *rdma_lite_create_cq(struct rdma_lite_context *lite_ctx,
    struct rdma_lite_cq_attr *lite_cq_attr)
{
    struct rdma_lite_ops *hns_roce_lite_ops = get_hns_roce_lite_ops();
    struct rdma_lite_cq *lite_cq;

    lite_cq = hns_roce_lite_ops->rdma_lite_create_cq(lite_ctx, lite_cq_attr);
    if (lite_cq == NULL) {
        return NULL;
    }

    lite_cq->ctx = lite_ctx;

    return lite_cq;
}

int rdma_lite_destroy_cq(struct rdma_lite_cq *lite_cq)
{
    struct rdma_lite_ops *hns_roce_lite_ops = get_hns_roce_lite_ops();

    return hns_roce_lite_ops->rdma_lite_destroy_cq(lite_cq);
}

struct rdma_lite_qp *rdma_lite_create_qp(struct rdma_lite_context *lite_ctx,
    struct rdma_lite_qp_attr *lite_qp_attr)
{
    struct rdma_lite_ops *hns_roce_lite_ops = get_hns_roce_lite_ops();
    struct rdma_lite_qp *lite_qp;

    lite_qp = hns_roce_lite_ops->rdma_lite_create_qp(lite_ctx, lite_qp_attr);
    if (lite_qp == NULL) {
        return NULL;
    }

    lite_qp->ctx = lite_ctx;
    lite_qp->send_cq = lite_qp_attr->send_cq;
    lite_qp->recv_cq = lite_qp_attr->recv_cq;
    lite_qp->qp_type = lite_qp_attr->qp_type;
    lite_qp->qp_state = lite_qp_attr->qp_state;

    return lite_qp;
}

int rdma_lite_destroy_qp(struct rdma_lite_qp *lite_qp)
{
    struct rdma_lite_ops *hns_roce_lite_ops = get_hns_roce_lite_ops();

    return hns_roce_lite_ops->rdma_lite_destroy_qp(lite_qp);
}

int rdma_lite_poll_cq(struct rdma_lite_cq *lite_cq, int num_entries, struct rdma_lite_wc *lite_wc)
{
    struct rdma_lite_ops *hns_roce_lite_ops = get_hns_roce_lite_ops();

    return hns_roce_lite_ops->rdma_lite_poll_cq(lite_cq, num_entries, lite_wc);
}

int rdma_lite_poll_cq_v2(struct rdma_lite_cq *lite_cq, int num_entries, struct rdma_lite_wc_v2 *lite_wc)
{
    struct rdma_lite_ops *hns_roce_lite_ops = get_hns_roce_lite_ops();

    return hns_roce_lite_ops->rdma_lite_poll_cq_v2(lite_cq, num_entries, lite_wc);
}

int rdma_lite_post_send(struct rdma_lite_qp *lite_qp, struct rdma_lite_send_wr *wr,
    struct rdma_lite_send_wr **bad_wr, struct rdma_lite_post_send_attr *attr, struct rdma_lite_post_send_resp *resp)
{
    struct rdma_lite_ops *hns_roce_lite_ops = get_hns_roce_lite_ops();

    return hns_roce_lite_ops->rdma_lite_post_send(lite_qp, wr, bad_wr, attr, resp);
}

int rdma_lite_post_recv(struct rdma_lite_qp *lite_qp, struct rdma_lite_recv_wr *wr, struct rdma_lite_recv_wr **bad_wr)
{
    struct rdma_lite_ops *hns_roce_lite_ops = get_hns_roce_lite_ops();

    return hns_roce_lite_ops->rdma_lite_post_recv(lite_qp, wr, bad_wr);
}

int rdma_lite_set_qp_sl(struct rdma_lite_qp *lite_qp, int sl)
{
    struct rdma_lite_ops *hns_roce_lite_ops = get_hns_roce_lite_ops();

    return hns_roce_lite_ops->rdma_lite_set_qp_sl(lite_qp, sl);
}

int rdma_lite_init_mem_pool(struct rdma_lite_context *lite_ctx, struct rdma_lite_mem_attr *lite_mem_attr)
{
    struct rdma_lite_ops *hns_roce_lite_ops = get_hns_roce_lite_ops();

    return hns_roce_lite_ops->rdma_lite_init_mem_pool(lite_ctx, lite_mem_attr);
}

int rdma_lite_deinit_mem_pool(struct rdma_lite_context *lite_ctx, u32 mem_idx)
{
    struct rdma_lite_ops *hns_roce_lite_ops = get_hns_roce_lite_ops();

    return hns_roce_lite_ops->rdma_lite_deinit_mem_pool(lite_ctx, mem_idx);
}

int rdma_lite_clean_qp(struct rdma_lite_qp *lite_qp)
{
    struct rdma_lite_ops *hns_roce_lite_ops = get_hns_roce_lite_ops();

    return hns_roce_lite_ops->rdma_lite_clean_qp(lite_qp);
}

unsigned int rdma_lite_get_api_version(void)
{
    return LITE_API_VERSION;
}

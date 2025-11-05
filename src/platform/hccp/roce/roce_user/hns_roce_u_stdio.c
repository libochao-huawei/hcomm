/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#define _GNU_SOURCE
#include <stdlib.h>
#include "hns_roce_u.h"
#include "hns_roce_u_sec.h"
#include "hns_roce_u_hw_v2.h"
#include "hns_roce_u_stdio.h"

#ifdef DEFINE_HNS_LLT
#undef static
#define static
#endif

void hns_roce_init_qp_indices(struct hns_roce_qp *qp)
{
    qp->sq.head = 0;
    qp->sq.tail = 0;
    qp->rq.head = 0;
    qp->rq.tail = 0;
    qp->next_sge = 0;
}

int align_queue_size(int req)
{
    unsigned int nent;

    for (nent = 1; nent < (unsigned int)req; nent <<= 1) {
        ;
    }
    return nent;
}

int align_qp_size(int req)
{
    unsigned int nent;

    for (nent = HNS_ROCE_MIN_WQE_NUM; nent < (unsigned int)req; nent <<= 1) {
        ;
    }
    return nent;
}

void hns_roce_calc_sq_wqe_size(struct ibv_qp_cap *cap, struct hns_roce_qp *qp)
{
    unsigned int size = sizeof(struct hns_roce_rc_send_wqe);

    for (qp->sq.wqe_shift = WQE_SHIFT_START; (1U << (unsigned int)(qp->sq.wqe_shift)) < size;) {
        qp->sq.wqe_shift++;
    }
}

void hns_roce_set_qp_params(struct ibv_pd *pd, struct ibv_qp_init_attr *attr, struct hns_roce_qp *qp)
{
    unsigned int sge_ex_count;
    unsigned int sq_shift;

    if (to_hr_dev(pd->context->device)->hw_version == HNS_ROCE_HW_VER1) {
        qp->sq.wqe_cnt = align_qp_size(attr->cap.max_send_wr);
        qp->rq.wqe_cnt = align_qp_size(attr->cap.max_recv_wr);
    } else {
        qp->sq.wqe_cnt = align_queue_size(attr->cap.max_send_wr);
        qp->rq.wqe_cnt = align_queue_size(attr->cap.max_recv_wr);
    }

    for (sq_shift = 0; (1U << sq_shift) < (qp->sq.wqe_cnt); ++sq_shift) {
        ;
    }
    qp->sq.shift = sq_shift;

    if (to_hr_dev(pd->context->device)->hw_version == HNS_ROCE_HW_VER1) {
        qp->sq.max_gs = HNS_ROCE_SGE_IN_WQE;
    } else {
        qp->sq.max_gs = attr->cap.max_send_sge;
        if (qp->sq.max_gs > HNS_ROCE_SGE_IN_WQE) {
            sge_ex_count = qp->sq.wqe_cnt * (qp->sq.max_gs - HNS_ROCE_SGE_IN_WQE);
            for (qp->sge.sge_cnt = 1; qp->sge.sge_cnt < sge_ex_count;) {
                qp->sge.sge_cnt <<= 1;
            }
        } else {
            qp->sge.sge_cnt = 0;
        }
    }

    if (attr->qp_type == IBV_QPT_UD) {
        sge_ex_count = qp->sq.wqe_cnt * (qp->sq.max_gs);
        for (qp->sge.sge_cnt = 1; qp->sge.sge_cnt < sge_ex_count;) {
            qp->sge.sge_cnt <<= 1;
        }
    }
}

int hns_roce_verify_qp(struct ibv_qp_init_attr *attr, struct hns_roce_context *context)
{
    struct hns_roce_device *hr_dev = to_hr_dev(context->ibv_ctx.context.device);

    if (hr_dev->hw_version == HNS_ROCE_HW_VER1) {
        if (attr->cap.max_send_wr < HNS_ROCE_MIN_WQE_NUM) {
            roce_warn("max_send_wr = %u, less than minimum WQE number.", attr->cap.max_send_wr);
            attr->cap.max_send_wr = HNS_ROCE_MIN_WQE_NUM;
        }

        if (attr->cap.max_recv_wr < HNS_ROCE_MIN_WQE_NUM) {
            roce_warn("max_recv_wr = %u, less than minimum WQE number.", attr->cap.max_recv_wr);
            attr->cap.max_recv_wr = HNS_ROCE_MIN_WQE_NUM;
        }
    }

    if (attr->cap.max_recv_sge < 1) {
        attr->cap.max_recv_sge = 1;
    }

    if ((unsigned int)attr->cap.max_send_wr > context->max_qp_wr ||
        (unsigned int)attr->cap.max_recv_wr > context->max_qp_wr ||
        (unsigned int)attr->cap.max_send_sge > context->max_sge  ||
        (unsigned int)attr->cap.max_recv_sge > context->max_sge) {
        roce_err("verify failed, attr max_send_wr is %u, max_recv_wr is %u, \
                 max_send_sge is %u, max_recv_sge is %u, context max_qp_wr is %u, max_sge is %u",
                 attr->cap.max_send_wr, attr->cap.max_recv_wr,
                 attr->cap.max_send_sge, attr->cap.max_recv_sge,
                 context->max_qp_wr, context->max_sge);
        return -1;
    }

    if ((attr->qp_type != IBV_QPT_RC) && (attr->qp_type != IBV_QPT_UD)) {
        roce_err("verify failed, qp_type is %d", attr->qp_type);
        return -EINVAL;
    }

    if ((attr->qp_type == IBV_QPT_RC) && (attr->cap.max_inline_data > HNS_ROCE_MAX_INLINE_DATA_LEN)) {
        roce_err("verify failed, attr qp_type is %d, max_inline_data is %u, define max_inline is %d",
                 attr->qp_type, attr->cap.max_inline_data, HNS_ROCE_MAX_INLINE_DATA_LEN);
        return -1;
    }

#ifndef DEFINE_HNS_LLT
    if (attr->qp_type == IBV_QPT_UC) {
        roce_err("verify failed, attr qp_type is %d", attr->qp_type);
        return -1;
    }
#endif
    return 0;
}

#ifndef DEFINE_HNS_LLT
static void hns_roce_free_recv_inl_buf(struct hns_roce_qp *qp)
{
    if (qp->rq_rinl_buf.wqe_list != NULL && qp->rq_rinl_buf.wqe_list[0].sg_list != NULL) {
        free(qp->rq_rinl_buf.wqe_list[0].sg_list);
        qp->rq_rinl_buf.wqe_list[0].sg_list = NULL;
    }

    if (qp->rq_rinl_buf.wqe_list != NULL) {
        free(qp->rq_rinl_buf.wqe_list);
        qp->rq_rinl_buf.wqe_list = NULL;
    }
}
#endif

STATIC int hns_roce_alloc_recv_inl_buf(const struct ibv_qp_cap *cap, struct hns_roce_qp *qp)
{
    uint32_t i;
    uint32_t sge_len;

    sge_len = qp->rq.wqe_cnt;
    HNS_ROCE_U_PARA_CHECK_RETURN_INT(sge_len);
    sge_len *= sizeof(struct hns_roce_rinl_wqe);
    qp->rq_rinl_buf.wqe_list = (struct hns_roce_rinl_wqe *)calloc(1, sge_len);
    if (qp->rq_rinl_buf.wqe_list == NULL) {
        roce_err("calloc wqe_list failed, sge_len is %u", sge_len);
        return -1;
    }

    qp->rq_rinl_buf.wqe_cnt = qp->rq.wqe_cnt;
    sge_len = qp->rq.wqe_cnt * cap->max_recv_sge;
    if (sge_len <= 0) {
        roce_err("sge_len is %u", sge_len);
        goto out;
    }

    sge_len *= sizeof(struct hns_roce_rinl_sge);
    qp->rq_rinl_buf.wqe_list[0].sg_list = (struct hns_roce_rinl_sge *)calloc(1, sge_len);
    if (qp->rq_rinl_buf.wqe_list[0].sg_list == NULL) {
#ifndef DEFINE_HNS_LLT
        roce_err("calloc sg_list failed, sge_len is %u", sge_len);
        goto out;
#endif
    }

    for (i = 0; i < qp->rq_rinl_buf.wqe_cnt; i++) {
        int wqe_size = i * cap->max_recv_sge;
        qp->rq_rinl_buf.wqe_list[i].sg_list = &(qp->rq_rinl_buf.wqe_list[0].sg_list[wqe_size]);
    }

    return 0;
out:
    free(qp->rq_rinl_buf.wqe_list);
    qp->rq_rinl_buf.wqe_list = NULL;
    return -ENOMEM;
}

static int hns_roce_calc_qp_buff_size(struct ibv_pd *pd, struct ibv_qp_cap *cap,
                                      enum ibv_qp_type type,
                                      struct hns_roce_qp *qp)
{
    int ret;
    unsigned int page_size = (unsigned int)to_hr_dev(pd->context->device)->page_size;

    for (qp->rq.wqe_shift = HNS_ROCE_SGE_SHIFT;
            (1U << (unsigned int)qp->rq.wqe_shift) < (HNS_ROCE_SGE_SIZE * cap->max_recv_sge);) {
        qp->rq.wqe_shift++;
    }

    if (qp->sq.max_gs > HNS_ROCE_SGE_IN_WQE || type == IBV_QPT_UD) {
        qp->sge.sge_shift = HNS_ROCE_SGE_SHIFT;
    } else {
        qp->sge.sge_shift = 0;
    }

    /* alloc recv inline buf */
    ret = hns_roce_alloc_recv_inl_buf(cap, qp);
    if (ret) {
        roce_err("hns_roce_alloc_recv_inl_buf failed");
        return ret;
    }

    if (qp->gdr_enabled != HNS_ROCE_CREATE_EXP) {
        qp->buf_size = hns_roce_align((unsigned int)(qp->sq.wqe_cnt << (unsigned int)(qp->sq.wqe_shift)), page_size) +
                       hns_roce_align((unsigned int)(qp->sge.sge_cnt << (unsigned int)(qp->sge.sge_shift)), page_size) +
                       (unsigned int)(qp->rq.wqe_cnt << (unsigned int)(qp->rq.wqe_shift));
    } else {
        qp->buf_size = hns_roce_align((unsigned int)(qp->sge.sge_cnt << (unsigned int)(qp->sge.sge_shift)), page_size) +
                       (unsigned int)(qp->rq.wqe_cnt << (unsigned int)(qp->rq.wqe_shift));
    }

    if (qp->sge.sge_cnt) {
        qp->sq.offset = 0;
        qp->sge.offset = hns_roce_align((unsigned int)(qp->sq.wqe_cnt << (unsigned int)qp->sq.wqe_shift), page_size);
        qp->rq.offset = qp->sge.offset +
                        hns_roce_align((unsigned int)(qp->sge.sge_cnt << (unsigned int)(qp->sge.sge_shift)), page_size);
    } else {
        qp->sq.offset = 0;
        qp->sge.offset = 0;
        qp->rq.offset = hns_roce_align((unsigned int)(qp->sq.wqe_cnt << (unsigned int)(qp->sq.wqe_shift)), page_size);
    }

    if (qp->gdr_enabled == HNS_ROCE_QP_MODE_GDR) {
        qp->sq.offset = 0;
        qp->sge.offset = 0;
        qp->rq.offset = qp->sge.sge_cnt ?
            hns_roce_align((unsigned int)(qp->sge.sge_cnt << (unsigned int)(qp->sge.sge_shift)), page_size) : 0;
    }

    return 0;
}

static int hns_roce_alloc_qp_wrid_init(struct hns_roce_qp *qp)
{
    size_t rq_wrid_len;
    if (qp->rq.wqe_cnt > 0) {
        rq_wrid_len = qp->rq.wqe_cnt * sizeof(uint64_t);
        if (rq_wrid_len < sizeof(uint64_t)) {  //lint !e574
            roce_err("invalid malloc size:%lu", rq_wrid_len);
            goto out;
        }
        qp->rq.wrid = (unsigned long *)calloc(1, rq_wrid_len);
        if (qp->rq.wrid == NULL) {
            roce_err("rq wrid malloc failed, wqe_cnt is %d", qp->rq.wqe_cnt);
            goto out;
        }
    }

    return 0;
out:
    return -ENOMEM;
}

int hns_roce_alloc_qp_buf(struct ibv_pd *pd, struct ibv_qp_cap *cap,
    enum ibv_qp_type type, struct hns_roce_qp *qp)
{
    struct hns_roce_context *ctx = to_hr_ctx(pd->context);
    unsigned int page_size;
    size_t sq_wrid_len;
    int ret;

    page_size = (unsigned int)to_hr_dev(pd->context->device)->page_size;
    HNS_ROCE_U_PARA_CHECK_RETURN_INT(qp->sq.wqe_cnt);
    sq_wrid_len = qp->sq.wqe_cnt * sizeof(uint64_t);

    qp->sq.wrid = (unsigned long *)calloc(1, sq_wrid_len);
    if (qp->sq.wrid == NULL) {
        roce_err("sq wrid malloc failed, wqe_cnt is %d", qp->sq.wqe_cnt);
        ret = -ENOMEM;
        goto out;
    }

    ret = hns_roce_alloc_qp_wrid_init(qp);
    if (ret) {
        roce_err("hns_roce_alloc qp buf failed");
        goto malloc_rq_wrid_err;
    }

    ret = hns_roce_calc_qp_buff_size(pd, cap, type, qp);
    if (ret) {
        roce_err("hns_roce_calc_qp_buff_size failed");
        goto calc_qp_buff_err;
    }

    if (qp->buf_type == HNS_ROCE_BUF_TYPE_LITE_ALIGN_4KB) {
        ret = hns_roce_hal_alloc_buf(&qp->buf, (unsigned int)hns_roce_align((unsigned long)qp->buf_size, page_size),
                                     page_size, ctx->dev_id);
    } else if (qp->buf_type == HNS_ROCE_BUF_TYPE_LITE_ALIGN_2MB) {
        ret = hns_roce_hal_alloc_mem(&qp->buf, (unsigned int)hns_roce_align((unsigned long)qp->buf_size, page_size), page_size);
    } else {
        ret = hns_roce_alloc_buf(&qp->buf, (unsigned int)hns_roce_align((unsigned long)qp->buf_size, page_size),
                                 (int)page_size);
    }

    if (ret) {
#ifndef DEFINE_HNS_LLT
        roce_err("hns_roce_alloc_buf failed, qp->buf_size is %d", qp->buf_size);
        goto alloc_buf_err;
#endif
    }

    return 0;

alloc_buf_err:
#ifndef DEFINE_HNS_LLT
    hns_roce_free_recv_inl_buf(qp);
#endif
calc_qp_buff_err:
    if (qp->rq.wqe_cnt > 0) {
        free(qp->rq.wrid);
        qp->rq.wrid = NULL;
    }
malloc_rq_wrid_err:
    free(qp->sq.wrid);
    qp->sq.wrid = NULL;
out:
    return ret;
}

void hns_roce_free_qp_buf(struct hns_roce_qp *qp)
{
    hns_roce_free_buf(&qp->buf);
#ifndef DEFINE_HNS_LLT
    hns_roce_free_recv_inl_buf(qp);
#endif
    if (qp->rq.wrid != NULL) {
        free(qp->rq.wrid);
        qp->rq.wrid = NULL;
    }

    if (qp->sq.wrid != NULL) {
        free(qp->sq.wrid);
        qp->sq.wrid = NULL;
    }
}

int hns_roce_store_qp(struct hns_roce_context *ctx, uint32_t qpn, struct hns_roce_qp *qp)
{
    uint32_t tind = (qpn & ((uint32_t)ctx->num_qps - 1)) >> (uint32_t)ctx->qp_table_shift;

    if (ctx->qp_table[tind].refcnt == 0) {
        HNS_ROCE_U_PARA_CHECK_RETURN_INT((ctx->qp_table_mask + 1));
        ctx->qp_table[tind].table = calloc(ctx->qp_table_mask + 1, sizeof(struct hns_roce_qp *));
        if (ctx->qp_table[tind].table == NULL) {
            roce_err("calloc qp table failed, qp_table_mask is %d", ctx->qp_table_mask);
            return -1;
        }
    }

    ++ctx->qp_table[tind].refcnt;
    ctx->qp_table[tind].table[qpn & (unsigned int)(ctx->qp_table_mask)] = qp;

    return 0;
}


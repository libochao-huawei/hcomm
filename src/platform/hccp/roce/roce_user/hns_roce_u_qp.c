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
#include "hns_roce_u_db.h"
#include "hns_roce_u_abi.h"
#include "hns_roce_u_sec.h"
#include "hns_roce_u_stdio.h"
#include "hns_roce_u_qp.h"

#ifdef DEFINE_HNS_LLT
#undef static
#define static
#endif

STATIC int hns_roce_qp_gdr_init(struct hns_roce_qp *qp, struct hns_roce_create_qp_resp resp, int qp_type)
{
    uint32_t i, bitmap_num;
    int mod, div;

    if (qp_type != HNS_ROCE_CREATE_EXP) {
        return 0;
    }

    qp->gdr_enabled = HNS_ROCE_QP_MODE_GDR;
    qp->create_flags |= CREATE_FLAG_NO_DOORBELL;

    pthread_mutex_init(&qp->gdr_temp_wqe.wqe_mutex, NULL);

    qp->gdr_share_sq.index = resp.share_sq_index;
    qp->gdr_share_sq.sq_depth = resp.share_sq_depth;
    qp->gdr_share_sq.temp_depth = resp.share_temp_depth;
    qp->gdr_share_sq.db_depth = resp.share_db_depth;
    qp->gdr_share_sq.dfx_depth = resp.share_dfx_depth;
    qp->gdr_share_sq.sq_offset = resp.share_sq_offset;
    qp->gdr_share_sq.temp_offset = resp.share_temp_offset;
    qp->gdr_share_sq.db_offset = resp.share_db_offset;
    qp->gdr_share_sq.dfx_offset = resp.share_dfx_offset;
    qp->gdr_share_sq.max_sq_num = resp.share_max_sq_num;
    qp->gdr_temp_wqe.temp_offset = resp.share_temp_offset;
    qp->gdr_temp_wqe.wqe_num = resp.share_temp_depth;
    qp->gdr_temp_wqe.use_cnt = 0;

    mod = qp->gdr_temp_wqe.wqe_num % ((int)sizeof(unsigned long) * BIT_CNT_PER_BYTE);
    div = qp->gdr_temp_wqe.wqe_num / ((int)sizeof(unsigned long) * BIT_CNT_PER_BYTE);
    bitmap_num = mod ? (div  + 1) : div;

    if (qp->gdr_temp_wqe.wqe_num == 0) {
        bitmap_num = 1;
    }

    if (bitmap_num == 0) {
        roce_err("bitmap_num is %u", bitmap_num);
        pthread_mutex_destroy(&qp->gdr_temp_wqe.wqe_mutex);
        return -EINVAL;
    }

    qp->gdr_temp_wqe.bitmap = calloc(bitmap_num, sizeof(unsigned long));
    if (qp->gdr_temp_wqe.bitmap == NULL) {
        roce_err("temp wqe bitmap calloc failed, bitmap_num is %u", bitmap_num);
        pthread_mutex_destroy(&qp->gdr_temp_wqe.wqe_mutex);
        return -EINVAL;
    }

    qp->gdr_temp_wqe.bitmap_len = bitmap_num;
    /* init the bitmap */
    for (i = 0; i < bitmap_num; ++i) {
        qp->gdr_temp_wqe.bitmap[i] = ~(0UL);
    }

    roce_info("hns_roce_qp_gdr_init bitmap_num:%u wqe_num:%u!", bitmap_num, qp->gdr_temp_wqe.wqe_num);

    return 0;
}

STATIC int get_db_addr_sender(struct ibv_pd *pd, const struct ibv_qp_init_attr *attr,
                              struct hns_roce_qp *qp,
                              struct hns_roce_context *context,
                              struct hns_roce_create_qp *cmd)
{
    if ((to_hr_dev(pd->context->device)->hw_version != HNS_ROCE_HW_VER1) &&
            attr->cap.max_send_wr) {
        qp->sdb = hns_roce_alloc_db(context, HNS_ROCE_QP_TYPE_DB, qp->buf_type, qp->buf.mem_idx);
        if (qp->sdb == NULL) {
            roce_err("alloc sdb buffer failed!");
            return -ENOMEM;
        }

        *(qp->sdb) = 0;
        cmd->sdb_addr = (uintptr_t)qp->sdb;
    } else {
        cmd->sdb_addr = 0;
    }

    return 0;
}

static void free_db_addr_sender(struct ibv_pd *pd,
    const struct ibv_qp_init_attr *attr, struct hns_roce_qp *qp)
{
#ifndef DEFINE_HNS_LLT
    struct hns_roce_context *context = to_hr_ctx(pd->context);
    if ((to_hr_dev(pd->context->device)->hw_version != HNS_ROCE_HW_VER1) &&
            attr->cap.max_send_sge) {
        hns_roce_free_db(context, qp->sdb, HNS_ROCE_QP_TYPE_DB);
    }
#endif
}

STATIC int get_db_addr_responder(struct ibv_pd *pd,
    const struct ibv_qp_init_attr *attr, struct hns_roce_qp *qp,
    struct hns_roce_context *context, struct hns_roce_create_qp *cmd)
{
    if ((to_hr_dev(pd->context->device)->hw_version != HNS_ROCE_HW_VER1) &&
            attr->cap.max_recv_sge) {
        qp->rdb = hns_roce_alloc_db(context, HNS_ROCE_QP_TYPE_DB, qp->buf_type, qp->buf.mem_idx);
        if (qp->rdb == NULL) {
            roce_err("alloc rdb buffer failed!");
            return -ENOMEM;
        }

        *(qp->rdb) = 0;
        cmd->db_addr = (uintptr_t) qp->rdb;
    } else {
        cmd->db_addr = 0;
    }

    return 0;
}

static void free_db_addr_responder(struct ibv_pd *pd,
    const struct ibv_qp_init_attr *attr, struct hns_roce_qp *qp)
{
#ifndef DEFINE_HNS_LLT
    struct hns_roce_context *context = to_hr_ctx(pd->context);
    if ((to_hr_dev(pd->context->device)->hw_version != HNS_ROCE_HW_VER1) &&
            attr->cap.max_recv_sge) {
        hns_roce_free_db(context, qp->rdb, HNS_ROCE_QP_TYPE_DB);
    }
#endif
}

STATIC void hns_roce_u_add_qp_list(struct ibv_pd *pd, struct ibv_qp_init_attr *attr,
    struct hns_roce_qp *qp)
{
    struct hns_roce_cq *send_cq = NULL;
    struct hns_roce_cq *recv_cq = NULL;
    struct hns_roce_device *hr_dev = to_hr_dev(pd->context->device);

    if (hr_dev->hw_version != HNS_ROCE_HW_VER1) {
        send_cq = attr->send_cq ? to_hr_cq(attr->send_cq) : NULL;
        recv_cq = attr->recv_cq ? to_hr_cq(attr->recv_cq) : NULL;

        if (send_cq != NULL) {
            list_add_tail(&send_cq->list_sq, &qp->scq_list);
        }
        if (recv_cq != NULL) {
            list_add_tail(&recv_cq->list_rq, &qp->rcq_list);
        }
    }
}

STATIC void get_log_sq_bb_count(const struct hns_roce_qp *qp, struct hns_roce_create_qp *cmd)
{
    for (cmd->log_sq_bb_count = 0; (int)qp->sq.wqe_cnt > (1 << cmd->log_sq_bb_count);) {
        ++cmd->log_sq_bb_count;
    }
}

STATIC void hns_roce_qp_attr(struct ibv_pd *pd, struct hns_roce_qp *qp, struct ibv_qp_init_attr *attr)
{
    struct hns_roce_context *ctx = to_hr_ctx(qp->ibv_qp.context);
    struct hns_roce_context *context = to_hr_ctx(pd->context);
    /* adjust rq maxima to not exceed reported device maxima */
    attr->cap.max_recv_wr = ROCE_MIN(context->max_qp_wr, (unsigned int)attr->cap.max_recv_wr);
    attr->cap.max_recv_sge = ROCE_MIN(context->max_sge, (unsigned int)attr->cap.max_recv_sge);

    qp->rq.max_post = attr->cap.max_recv_wr;

    attr->cap.max_send_sge = ROCE_MIN(ctx->max_sge, qp->sq.max_gs);
    qp->sq.max_post = ROCE_MIN(ctx->max_qp_wr, qp->sq.wqe_cnt);
    attr->cap.max_send_wr = qp->sq.max_post;

    if (to_hr_dev(qp->ibv_qp.context->device)->hw_version == HNS_ROCE_HW_VER3) {
        qp->max_inline_data  = HNS_ROCE_V3_MAX_INLINE_DATA_LEN;
    } else {
        qp->max_inline_data  = HNS_ROCE_MAX_INLINE_DATA_LEN;
    }

    attr->cap.max_inline_data = qp->max_inline_data;
    qp->sq_signal_bits = attr->sq_sig_all ? 0 : 1;
}

STATIC void hns_roce_free_qp_table(struct ibv_pd *pd, const struct hns_roce_qp *qp)
{
    int tind;
    struct hns_roce_context *ctx = to_hr_ctx(pd->context);

#ifndef DEFINE_HNS_LLT
    tind = (qp->ibv_qp.qp_num & ((unsigned int)ctx->num_qps - 1)) >> (unsigned int)ctx->qp_table_shift;
#else
    tind = 0;
#endif
    if (ctx->qp_table[tind].table) {
        free(ctx->qp_table[tind].table);
        ctx->qp_table[tind].table = NULL;
    }
}

STATIC int _hns_roce_u_qp_lock_init(struct hns_roce_qp *qp)
{
    int ret;

    ret = pthread_spin_init(&qp->sq.lock, PTHREAD_PROCESS_PRIVATE);
    if (ret) {
        roce_err("sq lock init failed!,ret:%d", ret);
        return ret;
    }

    ret = pthread_spin_init(&qp->rq.lock, PTHREAD_PROCESS_PRIVATE);
    if (ret) {
        roce_err("rq lock init failed!,ret:%d", ret);
        pthread_spin_destroy(&qp->sq.lock);
        return ret;
    }

    return ret;
}

STATIC void _hns_roce_u_qp_lock_uninit(struct hns_roce_qp *qp)
{
    pthread_spin_destroy(&qp->rq.lock);
    pthread_spin_destroy(&qp->sq.lock);
}

STATIC int _hns_roce_u_create_qp_init(struct ibv_pd *pd, struct hns_roce_qp *qp,
    struct ibv_qp_init_attr *attr, struct hns_roce_create_qp *cmd, struct hns_roce_create_type hr_qp_type)
{
    int ret;

    struct hns_roce_context *context = to_hr_ctx(pd->context);
    cmd->share_addr_base = (uintptr_t)(context->share_buffer_base);
    cmd->gdr_enable = hr_qp_type.type;
    roce_dbg("qp type is [%d]", hr_qp_type.type);
    qp->gdr_enabled = hr_qp_type.type;
    qp->create_flags = 0;
    qp->buf_type = HNS_ROCE_BUF_TYPE_NORMAL;

    if (hr_qp_type.type == HNS_ROCE_QP_MODE_OP && hr_qp_type.lite_op_supported != 0) {
        qp->buf_type = HNS_ROCE_BUF_TYPE_LITE_ALIGN_4KB;
        if (hr_qp_type.mem_align == LITE_ALIGN_2MB) {
            qp->buf_type = HNS_ROCE_BUF_TYPE_LITE_ALIGN_2MB;
            qp->buf.mem_idx = hr_qp_type.mem_idx;
            qp->buf.mem_align = hr_qp_type.mem_align;
        }
    }

    hns_roce_calc_sq_wqe_size(&attr->cap, qp);
    hns_roce_set_qp_params(pd, attr, qp);
    ret = hns_roce_alloc_qp_buf(pd, &attr->cap, attr->qp_type, qp);
    if (ret) {
        return ret;
    }

    hns_roce_init_qp_indices(qp);
    ret = _hns_roce_u_qp_lock_init(qp);
    if (ret) {
        roce_err("qp lock init failed!, ret:%d", ret);
        goto err_free;
    }

    ret = get_db_addr_sender(pd, attr, qp, context, cmd);
    if (ret) {
        roce_err("get_db_addr_sender failed, ret [%d], expect 0", ret);
        goto err_addr_sender;
    }

    ret = get_db_addr_responder(pd, attr, qp, context, cmd);
    if (ret) {
        roce_err("get_db_addr_responder failed, ret [%d], expect 0", ret);
        goto err_sq_db;
    }

    cmd->buf_addr = (uintptr_t)((char *)qp->buf.buf + qp->buf.offset);
    cmd->log_sq_stride = qp->sq.wqe_shift;
    get_log_sq_bb_count(qp, cmd);
    return 0;

err_sq_db:
    free_db_addr_sender(pd, attr, qp);
err_addr_sender:
    _hns_roce_u_qp_lock_uninit(qp);
err_free:
    hns_roce_free_qp_buf(qp);
    return ret;
}

STATIC int _hns_roce_u_create_qp_gdr(struct ibv_pd *pd, struct hns_roce_qp *qp,
    struct ibv_qp_init_attr *attr, struct hns_roce_create_qp *cmd, struct hns_roce_create_type hr_qp_type)
{
    int ret;
    struct hns_roce_create_qp_resp resp = {};

    (void)pthread_mutex_lock(&to_hr_ctx(pd->context)->qp_table_mutex);
    ret = ibv_cmd_create_qp(pd, &qp->ibv_qp, attr, (struct ibv_create_qp *)cmd, sizeof(struct hns_roce_create_qp),
        (struct ib_uverbs_create_qp_resp *)&resp, sizeof(resp));
    if (ret) {
        roce_err("ibv_cmd_create_qp failed!, ret [%d], expect 0", ret);
        goto err_rq_db;
    }

    ret = hns_roce_store_qp(to_hr_ctx(pd->context), qp->ibv_qp.qp_num, qp);
    if (ret) {
        roce_err("hns_roce_store_qp failed!, ret [%d], expect 0", ret);
        goto err_destroy;
    }

    qp->rq.wqe_cnt = attr->cap.max_recv_wr;
    qp->rq.max_gs = attr->cap.max_recv_sge;
    qp->flags = resp.cap_flags;

    ret = hns_roce_qp_gdr_init(qp, resp, hr_qp_type.type);
    if (ret) {
        roce_err("hns_roce_qp_gdr_init failed!, ret [%d], expect 0", ret);
        goto err_qp_table;
    }

    (void)pthread_mutex_unlock(&to_hr_ctx(pd->context)->qp_table_mutex);

    return 0;
err_qp_table:
    hns_roce_free_qp_table(pd, qp);

err_destroy:
    (void)ibv_cmd_destroy_qp(&qp->ibv_qp);

err_rq_db:
    (void)pthread_mutex_unlock(&to_hr_ctx(pd->context)->qp_table_mutex);
    return ret;
}

STATIC void hns_roce_u_destory_qp_gdr(struct ibv_pd *pd,
    struct ibv_qp_init_attr *attr, struct hns_roce_qp *qp)
{
    free(qp->gdr_temp_wqe.bitmap);
    qp->gdr_temp_wqe.bitmap = NULL;
    pthread_mutex_destroy(&qp->gdr_temp_wqe.wqe_mutex);
    free_db_addr_responder(pd, attr, qp);
    free_db_addr_sender(pd, attr, qp);
    pthread_spin_destroy(&qp->rq.lock);
    pthread_spin_destroy(&qp->sq.lock);
    hns_roce_free_qp_buf(qp);
}

STATIC void hns_roce_set_op_qp_resp(struct rdma_lite_device_qp_attr *qp_resp,
                                    struct hns_roce_qp *qp, struct hns_roce_context *ctx)
{
    unsigned long page_size = (unsigned long)to_hr_dev(ctx->ibv_ctx.context.device)->page_size;

    qp_resp->max_inline_data = qp->max_inline_data;
    qp_resp->sge.sge_cnt = qp->sge.sge_cnt;
    qp_resp->next_sge = qp->next_sge;
    qp_resp->sge.offset = qp->sge.offset;
    qp_resp->sge.sge_shift = qp->sge.sge_shift;
    qp_resp->qp_info = 0;
    qp_resp->qpn = qp->ibv_qp.qp_num;
    qp_resp->qp_buf.va = (unsigned long long)qp->buf.buf;
    qp_resp->qp_buf.len = qp->buf.length;
    qp_resp->sq.offset = qp->sq.offset;
    qp_resp->rq.offset = qp->rq.offset;
    qp_resp->sq.wrid_len = qp->sq.wqe_cnt * sizeof(uint64_t);
    qp_resp->rq.wrid_len = qp->rq.wqe_cnt * sizeof(uint64_t);
    qp_resp->sq.max_post = qp->sq.max_post;
    qp_resp->rq.max_post = qp->rq.max_post;
    qp_resp->sq.max_gs = qp->sq.max_gs;
    qp_resp->rq.max_gs = qp->rq.max_gs;
    qp_resp->sq.wqe_cnt = qp->sq.wqe_cnt;
    qp_resp->rq.wqe_cnt = qp->rq.wqe_cnt;
    qp_resp->sq.shift = qp->sq.shift;
    qp_resp->sq.wqe_shift = (unsigned int)qp->sq.wqe_shift;
    qp_resp->rq.wqe_shift = (unsigned int)qp->rq.wqe_shift;
    qp_resp->sq.db_buf.va = (unsigned long long)qp->sdb;
    if (qp->buf.mem_align == LITE_ALIGN_2MB) {
        qp_resp->sq.db_buf.va = (unsigned long long)qp->buf.buf;
    }
    qp_resp->sq.db_buf.len = (unsigned int)page_size;
    qp_resp->rq.db_buf.va = (unsigned long long)qp->rdb;
    if (qp->buf.mem_align == LITE_ALIGN_2MB) {
        qp_resp->rq.db_buf.va = (unsigned long long)qp->buf.buf;
    }
    qp_resp->rq.db_buf.len = (unsigned int)page_size;
}

STATIC struct ibv_qp *_hns_roce_u_create_qp(struct ibv_pd *pd,
    struct ibv_qp_init_attr *attr, struct hns_roce_create_type hr_qp_type)
{
    int ret;
    struct hns_roce_qp *qp = NULL;
    struct hns_roce_create_qp cmd = {
        .reserved = {0}
    };
    struct hns_roce_context *context = to_hr_ctx(pd->context);

    if (hns_roce_verify_qp(attr, context)) {
        roce_err("hns_roce_verify_sizes failed!");
        return NULL;
    }

    qp = calloc(1, sizeof(struct hns_roce_qp));
    if (qp == NULL) {
        roce_err("calloc qp failed!");
        return NULL;
    }

    ret = _hns_roce_u_create_qp_init(pd, qp, attr, &cmd, hr_qp_type);
    if (ret) {
        roce_err("u qp init failed, ret [%d], expect 0", ret);
        goto err_buf;
    }

    ret = _hns_roce_u_create_qp_gdr(pd, qp, attr, &cmd, hr_qp_type);
    if (ret) {
        roce_err("u qp init failed, ret [%d], expect 0", ret);
        goto err_qp_gdr;
    }

    hns_roce_qp_attr(pd, qp, attr);
    hns_roce_u_add_qp_list(pd, attr, qp);

    return &qp->ibv_qp;

err_qp_gdr:
    hns_roce_u_destory_qp_gdr(pd, attr, qp);

err_buf:
    free(qp);
    qp = NULL;
    return NULL;
}

struct ibv_qp *hns_roce_u_create_qp(struct ibv_pd *pd,
                                    struct ibv_qp_init_attr *attr)
{
    struct hns_roce_create_type hr_qp_type = { 0 };
    HNS_ROCE_U_NULL_POINT_RETURN_NULL(pd);
    HNS_ROCE_U_NULL_POINT_RETURN_NULL(attr);

    hr_qp_type.type = HNS_ROCE_CREATE_NOR;
    hr_qp_type.lite_op_supported = HNS_ROCE_NOT_SUPPORT_LITE_OP;

    return _hns_roce_u_create_qp(pd, attr, hr_qp_type);
}

int hns_roce_u_query_qp(struct ibv_qp *ibqp, struct ibv_qp_attr *attr,
                        int attr_mask, struct ibv_qp_init_attr *init_attr)
{
    int ret;
    struct ibv_query_qp cmd;
    struct hns_roce_qp *qp = NULL;
    HNS_ROCE_U_NULL_POINT_RETURN_ERR(ibqp);
    HNS_ROCE_U_NULL_POINT_RETURN_ERR(attr);
    HNS_ROCE_U_NULL_POINT_RETURN_ERR(init_attr);

    qp = to_hr_qp(ibqp);
    ret = ibv_cmd_query_qp(ibqp, attr, attr_mask, init_attr, &cmd,
                           sizeof(cmd));
    if (ret) {
        roce_err("ibv_cmd_query_qp failed!, ret [%d], expect 0", ret);
        return ret;
    }

    init_attr->cap.max_send_wr = qp->sq.max_post;
    init_attr->cap.max_send_sge = qp->sq.max_gs;
    init_attr->cap.max_inline_data = qp->max_inline_data;

    attr->cap = init_attr->cap;

    return ret;
}

struct ibv_qp *hns_roce_u_exp_create_qp(struct ibv_pd *pd, struct ibv_exp_qp_init_attr *attrx,
                                        struct rdma_lite_device_qp_attr *qp_resp)
{
    struct hns_roce_create_type hr_qp_type = {0};
    struct hns_roce_context *context = NULL;
    struct hns_roce_qp *qp = NULL;
    struct ibv_qp *ib_qp =  NULL;

    HNS_ROCE_U_NULL_POINT_RETURN_NULL(pd);
    HNS_ROCE_U_NULL_POINT_RETURN_NULL(attrx);
    HNS_ROCE_U_NULL_POINT_RETURN_NULL(qp_resp);

    if (attrx->gdr_enable >= HNS_ROCE_QP_MODE_ERR) {
        roce_err("Invalid param, [%d]", attrx->gdr_enable);
        return NULL;
    }

    context = to_hr_ctx(pd->context);
    hr_qp_type.type = attrx->gdr_enable;
    hr_qp_type.lite_op_supported = attrx->lite_op_support;
    hr_qp_type.mem_align = attrx->mem_align;
    hr_qp_type.mem_idx = attrx->mem_idx;
    ib_qp = _hns_roce_u_create_qp(pd, &(attrx->attr), hr_qp_type);
    if (ib_qp != NULL) {
        qp = to_hr_qp(ib_qp);
        if (attrx->gdr_enable == HNS_ROCE_QP_MODE_GDR && context->port_num != 0) {
            qp_resp->qp_info = (int)(qp->gdr_share_sq.index + qp->gdr_share_sq.max_sq_num * context->port_id);
        } else if (attrx->gdr_enable == HNS_ROCE_QP_MODE_OP &&
                   attrx->lite_op_support == HNS_ROCE_SUPPORT_LITE_OP) {
            hns_roce_set_op_qp_resp(qp_resp, qp, context);
        } else if (attrx->gdr_enable == HNS_ROCE_QP_MODE_OP) {
            qp_resp->qp_info = 0;
        }
    }

    return ib_qp;
}

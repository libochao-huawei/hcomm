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
#include <errno.h>
#include <unistd.h>
#include <securec.h>
#include <sys/mman.h>
#include <ccan/minmax.h>
#include "hns_roce_u.h"
#include "hns_roce_u_abi.h"
#include "hns_roce_u_db.h"
#include "hns_roce_u_hw_v2.h"
#include "hns_roce_u_sec.h"
#include "hns_roce_u_stdio.h"
#include "hns_roce_u_buf.h"

#define CQN_CNT            2
#define HNS_ROCE_CQN_SHIFT_TWO      2
#define HNS_ROCE_CREATE_SRQ_OUT     1
#define HNS_ROCE_CREATE_SRQ_ERR_IDX_QUE     2
#define HNS_ROCE_CREATE_SRQ_ERR_SRQ_BUF     3

static int align_cq_size(int req)
{
    unsigned int nent;

    for (nent = HNS_ROCE_MIN_CQE_NUM; nent < (unsigned int)req; nent <<= 1) {
        ;
    }
    return nent;
}

static int align_srq_size(int req)
{
    unsigned int nent;

    for (nent = 1; nent < (unsigned int)req; nent <<= 1) {
        ;
    }
    return nent;
}

STATIC int hns_roce_verify_cq(int *cqe, const struct hns_roce_context *context)
{
    if (*cqe < 1 || *cqe > context->max_cqe) {
        roce_err("para check failed *cqe is [%d], max_cqe is [%d]", *cqe, context->max_cqe);
        return -1;
    }

    if (*cqe < HNS_ROCE_MIN_CQE_NUM) {
        roce_warn("cqe = %d, less than minimum CQE number.", *cqe);
        *cqe = HNS_ROCE_MIN_CQE_NUM;
    }

    return 0;
}

static int hns_roce_alloc_cq_buf(struct hns_roce_device *dev, struct hns_roce_buf *buf,
                                 int nent, enum hns_roce_buf_type buf_type, unsigned int dev_id)
{
    if (buf_type == HNS_ROCE_BUF_TYPE_LITE_ALIGN_4KB) {
        if (hns_roce_hal_alloc_buf(buf,
            (unsigned int)hns_roce_align((unsigned long)(nent * HNS_ROCE_CQE_ENTRY_SIZE), (unsigned long)dev->page_size),
            (unsigned int)dev->page_size, dev_id)) {
            roce_err("hns_roce_hal_alloc_buf failed, nent is [%d], dev_id[%u]", nent, dev_id);
            return -1;
        }
    } else if (buf_type == HNS_ROCE_BUF_TYPE_LITE_ALIGN_2MB) {
        if (hns_roce_hal_alloc_mem(buf,
            (unsigned int)hns_roce_align((unsigned long)(nent * HNS_ROCE_CQE_ENTRY_SIZE), (unsigned long)dev->page_size),
            (unsigned int)dev->page_size)) {
            roce_err("hns_roce_hal_alloc_mem failed, nent is [%d]", nent);
            return -1;
        }
    } else {
        if (hns_roce_alloc_buf(buf, hns_roce_align((unsigned int)(nent * HNS_ROCE_CQE_ENTRY_SIZE), dev->page_size),
            dev->page_size)) {
            roce_err("hns_roce_alloc_buf failed, nent is [%d]", nent);
            return -1;
        }
    }

    return 0;
}

STATIC int hns_roce_calloc_cq(struct hns_roce_cq **cq)
{
    *cq = calloc(1, sizeof(struct hns_roce_cq));
    if (*cq == NULL) {
        roce_err("malloc cq failed");
        return -ENOMEM;
    }

    return 0;
}

static void hns_roce_u_create_cq_add(struct hns_roce_cq *cq, struct hns_roce_create_cq_resp resp, int cqe,
    struct ibv_context *context)
{
    cq->cqn = resp.cqn;
    cq->cq_depth = cqe;
    cq->flags = resp.cap_flags;

    if (to_hr_dev(context->device)->hw_version == HNS_ROCE_HW_VER1) {
        cq->set_ci_db = (unsigned int *)((char *)(to_hr_ctx(context)->cq_tptr_base) + (unsigned int)(cq->cqn * HNS_ROCE_CQN_SHIFT_TWO));
    }

    cq->arm_db    = cq->set_ci_db;
    cq->arm_sn    = 1;
    *(cq->set_ci_db) = 0;
    *(cq->arm_db) = 0;

    list_head_init(&cq->list_sq);
    list_head_init(&cq->list_rq);
}

static int hns_roce_u_create_cq_alloc(struct hns_roce_cq *cq, struct ibv_context *context, int *cqe,
    struct hns_roce_create_cq *cmd, struct hns_roce_create_type hr_cq_type)
{
    struct hns_roce_context *ctx = NULL;

    cq->buf_type = HNS_ROCE_BUF_TYPE_NORMAL;
    cq->cons_index = 0;
    ctx = to_hr_ctx(context);

    if (pthread_spin_init(&cq->lock, PTHREAD_PROCESS_PRIVATE)) {
        roce_err("spin lock init fail");
        return -1;
    }

    if (to_hr_dev(context->device)->hw_version == HNS_ROCE_HW_VER1) {
        *cqe = align_cq_size(*cqe);
    } else {
        *cqe = align_queue_size(*cqe);
    }

    if ((hr_cq_type.type == HNS_ROCE_CREATE_OP) && hr_cq_type.lite_op_supported != 0) {
        cq->buf_type = HNS_ROCE_BUF_TYPE_LITE_ALIGN_4KB;
        if (hr_cq_type.mem_align == LITE_ALIGN_2MB) {
            cq->buf_type = HNS_ROCE_BUF_TYPE_LITE_ALIGN_2MB;
            cq->buf.mem_idx = hr_cq_type.mem_idx;
            cq->buf.mem_align = hr_cq_type.mem_align;
        }
    }

    if (hns_roce_alloc_cq_buf(to_hr_dev(context->device), &cq->buf, *cqe, cq->buf_type, ctx->dev_id)) {
        roce_err("alloc_cq_buf fail, cqe is %d", *cqe);
        pthread_spin_destroy(&cq->lock);
        return -1;
    }

    cmd->buf_addr = (uintptr_t)((char *)cq->buf.buf + cq->buf.offset);
    return 0;
}

STATIC void hns_roce_set_op_cq_resp(struct rdma_lite_device_cq_attr *cq_resp, struct hns_roce_cq *cq,
                                    struct ibv_context *context)
{
    unsigned long page_size = (unsigned long)to_hr_dev(context->device)->page_size;

    cq_resp->depth = cq->cq_depth;
    cq_resp->flags = cq->flags;
    cq_resp->cqe_size = HNS_ROCE_CQE_ENTRY_SIZE;
    cq_resp->cqn = cq->cqn;
    cq_resp->cq_buf.va = (unsigned long long)cq->buf.buf;
    cq_resp->cq_buf.len = (unsigned int)cq->buf.length;
    cq_resp->swdb_buf.va = (unsigned long long)cq->set_ci_db;
    if (cq->buf.mem_align == LITE_ALIGN_2MB) {
        cq_resp->swdb_buf.va = (unsigned long long)cq->buf.buf;
    }
    cq_resp->swdb_buf.len = (unsigned int)page_size;
}

STATIC struct ibv_cq *_hns_roce_u_create_cq(struct ibv_context *context, int cqe, struct ibv_comp_channel *channel,
    int vector, struct rdma_lite_device_cq_init_attr *attr, struct rdma_lite_device_cq_attr *cq_resp)
{
    struct hns_roce_create_type hr_cq_type = { 0 };
    struct hns_roce_create_cq_resp resp = {};
    struct hns_roce_create_cq cmd = {};
    struct hns_roce_cq *cq = NULL;
    int ret;

    hr_cq_type.type = attr->cq_type;
    hr_cq_type.lite_op_supported = attr->lite_op_supported;
    hr_cq_type.mem_align = attr->mem_align;
    hr_cq_type.mem_idx = attr->mem_idx;

    if (context == NULL) {
        roce_err("invalid param, context is NULL\n");
        return NULL;
    }

    if (hns_roce_verify_cq(&cqe, to_hr_ctx(context))) {
        roce_err("hns_roce_verify_cq failed, cqe is %d", cqe);
        return NULL;
    }

    ret = hns_roce_calloc_cq(&cq);
    if (ret) {
        roce_err("alloc cq err, ret [%d], expect 0", ret);
        return NULL;
    }

    if (hns_roce_u_create_cq_alloc(cq, context, &cqe, &cmd, hr_cq_type)) {
        roce_err("hns_roce_u_create_cq_alloc failed");
        goto err;
    }

    if (to_hr_dev(context->device)->hw_version != HNS_ROCE_HW_VER1) {
        cq->set_ci_db = hns_roce_alloc_db(to_hr_ctx(context), HNS_ROCE_CQ_TYPE_DB, cq->buf_type, cq->buf.mem_idx);
        if (cq->set_ci_db == NULL) {
            roce_err("alloc cq db buffer failed!");
            goto err_buf;
        }
        cmd.db_addr  = (uintptr_t) cq->set_ci_db;
    }

    ret = ibv_cmd_create_cq(context, cqe, channel, vector, &cq->verbs_cq.cq, &cmd.ibv_cmd, sizeof(cmd),
        &resp.ibv_resp, sizeof(resp));
    if (ret) {
        roce_err("ibv_cmd_create_cq failed, ret [%d], expect 0", ret);
        goto err_db;
    }

    hns_roce_u_create_cq_add(cq, resp, cqe, context);

    if (hr_cq_type.type == HNS_ROCE_CREATE_OP && hr_cq_type.lite_op_supported == HNS_ROCE_SUPPORT_LITE_OP) {
        hns_roce_set_op_cq_resp(cq_resp, cq, context);
    }

    return &cq->verbs_cq.cq;

err_db:
    if (to_hr_dev(context->device)->hw_version != HNS_ROCE_HW_VER1) {
        hns_roce_free_db(to_hr_ctx(context), cq->set_ci_db, HNS_ROCE_CQ_TYPE_DB);
    }

err_buf:
    hns_roce_free_buf(&cq->buf);
    pthread_spin_destroy(&cq->lock);
err:
    free(cq);
    cq = NULL;
    return NULL;
}

struct ibv_cq *hns_roce_u_create_cq(struct ibv_context *context, int cqe, struct ibv_comp_channel *channel, int vector)
{
    struct rdma_lite_device_cq_attr resp = {};
    struct rdma_lite_device_cq_init_attr attr = {};

    attr.cq_type = HNS_ROCE_CREATE_NOR;
    attr.lite_op_supported = HNS_ROCE_NOT_SUPPORT_LITE_OP;
    return _hns_roce_u_create_cq(context, cqe, channel, vector, &attr, &resp);
}

struct ibv_cq *hns_roce_u_exp_create_cq(struct ibv_context *context, int cqe, struct ibv_comp_channel *channel,
    int vector, struct rdma_lite_device_cq_init_attr *attr, struct rdma_lite_device_cq_attr *cq_resp)
{
    HNS_ROCE_U_NULL_POINT_RETURN_NULL(context);
    HNS_ROCE_U_NULL_POINT_RETURN_NULL(attr);
    HNS_ROCE_U_NULL_POINT_RETURN_NULL(cq_resp);

    return _hns_roce_u_create_cq(context, cqe, channel, vector, attr, cq_resp);
}

void hns_roce_u_cq_event(struct ibv_cq *cq)
{
    HNS_ROCE_U_NULL_POINT_RETURN_VOID(cq);
    to_hr_cq(cq)->arm_sn++;
}

int hns_roce_u_modify_cq(struct ibv_cq *cq, struct ibv_modify_cq_attr *attr)
{
    struct ibv_modify_cq cmd = {};
    HNS_ROCE_U_NULL_POINT_RETURN_ERR(cq);
    HNS_ROCE_U_NULL_POINT_RETURN_ERR(attr);

    return ibv_cmd_modify_cq(cq, attr, &cmd, sizeof(cmd));
}

int hns_roce_u_destroy_cq(struct ibv_cq *cq)
{
    int ret;
    HNS_ROCE_U_NULL_POINT_RETURN_ERR(cq);

    ret = ibv_cmd_destroy_cq(cq);
    if (ret) {
        roce_err("ibv_cmd_destroy_cq failed, ret [%d], expect 0", ret);
        return ret;
    }

    if (to_hr_dev(cq->context->device)->hw_version != HNS_ROCE_HW_VER1) {
        hns_roce_free_db(to_hr_ctx(cq->context),
                         to_hr_cq(cq)->set_ci_db, HNS_ROCE_CQ_TYPE_DB);
    }

    hns_roce_free_buf(&to_hr_cq(cq)->buf);
    free(to_hr_cq(cq));
    cq = NULL;
    return ret;
}

static void hns_roce_free_idx_que(struct hns_roce_srq *srq)
{
    struct hns_roce_idx_que *idx_que = &srq->idx_que;
    free(idx_que->bitmap);
    idx_que->bitmap = NULL;
    hns_roce_free_buf(&idx_que->buf);
}

static int hns_roce_create_idx_que(struct ibv_pd *pd, struct hns_roce_srq *srq)
{
    struct hns_roce_idx_que *idx_que = &srq->idx_que;
    uint32_t bitmap_num, i;

    idx_que->entry_sz = HNS_ROCE_IDX_QUE_ENTRY_SZ;

    /* bits needed in bitmap */
    bitmap_num = hns_roce_align(srq->max, BIT_CNT_PER_BYTE * sizeof(uint64_t));

    HNS_ROCE_U_PARA_CHECK_RETURN_INT(bitmap_num);

    idx_que->bitmap = calloc(1, bitmap_num / BIT_CNT_PER_BYTE);
    if (idx_que->bitmap == NULL) {
        roce_err("calloc bitmap failed");
        return -1;
    }

    /* bitmap_num indicates amount of u64 */
    bitmap_num = bitmap_num / (BIT_CNT_PER_BYTE * (unsigned int)sizeof(uint64_t));

    idx_que->buf_size = srq->max * idx_que->entry_sz;
    if (hns_roce_alloc_buf(&idx_que->buf, hns_roce_align(idx_que->buf_size, 0x1000),
                           to_hr_dev(pd->context->device)->page_size)) {
        roce_err("alloc idx que buf failed, buf size is %d", idx_que->buf_size);
        free(idx_que->bitmap);
        idx_que->bitmap = NULL;
        return -1;
    }

    /* init the idx_que bitmap */
    for (i = 0; i < bitmap_num; ++i) {
        idx_que->bitmap[i] = ~(0UL);
    }
    idx_que->bitmap_len = bitmap_num;

    return 0;
}

static int hns_roce_alloc_srq_buf(struct ibv_pd *pd, struct hns_roce_srq *srq)
{
    unsigned int srq_buf_size, srq_size;
    HNS_ROCE_U_PARA_CHECK_RETURN_INT(srq->max);

    srq->wrid = calloc(1, srq->max * sizeof(unsigned long));
    if (srq->wrid == NULL) {
        roce_err("calloc srq wrid failed");
        return -1;
    }

    /* srq size */
    srq_size = srq->max_gs * (unsigned int)sizeof(struct hns_roce_v2_wqe_data_seg);

    for (srq->wqe_shift = HNS_ROCE_SGE_SHIFT;
            (1U << (uint32_t)(srq->wqe_shift)) < srq_size;) {
        ++srq->wqe_shift; /* nothing */
    }

    srq_buf_size = (uint32_t)(srq->max) << (uint32_t)(srq->wqe_shift);
    /* allocate srq wqe buf */
    if (hns_roce_alloc_buf(&srq->buf, srq_buf_size,
                           to_hr_dev(pd->context->device)->page_size)) {
        roce_err("alloc buf failed, srq_buf_size is %u", srq_buf_size);
        free(srq->wrid);
        srq->wrid = NULL;
        return -1;
    }

    srq->head = 0;
    srq->tail = srq->max - 1;

    return 0;
}

static void hns_roce_free_srq_buf(struct hns_roce_srq *srq)
{
    free(srq->wrid);
    srq->wrid = NULL;
    hns_roce_free_buf(&srq->buf);
}

STATIC int hns_roce_calloc_srq(int num, struct hns_roce_srq **srq)
{
    if (num <= 0) {
        roce_err("invalid para num is %d", num);
        return -EINVAL;
    }

    *srq = calloc(num, sizeof(struct hns_roce_srq));
    if (*srq == NULL) {
        roce_err("calloc srq failed");
        return -ENOMEM;
    }

    return 0;
}

static int hns_roce_u_create_srq_check(const struct ibv_srq_init_attr *srq_init_attr, struct hns_roce_srq **srq)
{
    int ret;
    if (srq_init_attr->attr.max_wr > HNS_ROCE_MAX_SRQWQE_NUM || srq_init_attr->attr.max_sge > HNS_ROCE_MAX_SRQSGE_NUM) {
        roce_err("para check failed max_wr is %u, max_sge is %u",
                 srq_init_attr->attr.max_wr, srq_init_attr->attr.max_sge);
        return -1;
    }

    ret = hns_roce_calloc_srq(1, srq);
    if (ret) {
        roce_err("calloc srq failed");
        return -1;
    }
    return 0;
}

static int hns_roce_u_create_srq_alloc(struct hns_roce_srq *srq, struct ibv_srq_init_attr *srq_init_attr,
    struct ibv_pd *pd, struct hns_roce_create_srq *cmd)
{
    int ret;
    if (pthread_spin_init(&srq->lock, PTHREAD_PROCESS_PRIVATE)) {
        roce_err("init spin lock failed");
        return HNS_ROCE_CREATE_SRQ_OUT;
    }

    srq->max = align_srq_size(srq_init_attr->attr.max_wr + 1);
    srq->max_gs = srq_init_attr->attr.max_sge;
    srq->counter = 0;

    ret = hns_roce_create_idx_que(pd, srq);
    if (ret) {
        roce_err("hns_roce_create_idx_que failed, ret [%d], expect 0", ret);
        pthread_spin_destroy(&srq->lock);
        return HNS_ROCE_CREATE_SRQ_OUT;
    }

    if (hns_roce_alloc_srq_buf(pd, srq)) {
        roce_err("hns_roce_alloc_srq_buf failed!");
        hns_roce_free_idx_que(srq);
        pthread_spin_destroy(&srq->lock);
        return HNS_ROCE_CREATE_SRQ_ERR_IDX_QUE;
    }

    srq->db = hns_roce_alloc_db(to_hr_ctx(pd->context), HNS_ROCE_QP_TYPE_DB, HNS_ROCE_BUF_TYPE_NORMAL, -1);
    if (srq->db == NULL) {
        roce_err("hns_roce_alloc_db failed!");
        hns_roce_free_srq_buf(srq);
        hns_roce_free_idx_que(srq);
        pthread_spin_destroy(&srq->lock);
        return HNS_ROCE_CREATE_SRQ_ERR_SRQ_BUF;
    }

    *(srq->db) = 0;
    cmd->buf_addr = (uintptr_t)srq->buf.buf;
    cmd->que_addr = (uintptr_t)srq->idx_que.buf.buf;
    cmd->db_addr = (uintptr_t)srq->db;
    return 0;
}

struct ibv_srq *hns_roce_u_create_srq(struct ibv_pd *pd, struct ibv_srq_init_attr *srq_init_attr)
{
    int ret;
    struct hns_roce_create_srq cmd;
    struct hns_roce_create_srq_resp resp;
    struct hns_roce_srq *srq = NULL;

    HNS_ROCE_U_NULL_POINT_RETURN_NULL(pd);
    HNS_ROCE_U_NULL_POINT_RETURN_NULL(srq_init_attr);
    resp.srqn = 0;

    if (hns_roce_u_create_srq_check(srq_init_attr, &srq)) {
        roce_err("hns_roce_u_create_srq_check failed");
        return NULL;
    }

    ret = hns_roce_u_create_srq_alloc(srq, srq_init_attr, pd, &cmd);
    if (ret != 0) {
        roce_err("hns_roce_u_create_srq_alloc failed, ret [%d], expect 0", ret);
        goto out;
    }

    ret = ibv_cmd_create_srq(pd, &srq->verbs_srq.srq, srq_init_attr, (struct ibv_create_srq *)&cmd, sizeof(cmd),
        (struct ib_uverbs_create_srq_resp *)&resp, sizeof(resp));
    if (ret) {
        roce_err("ibv_cmd_create_srq failed!, ret [%d], expect 0", ret);
        goto err_srq_db;
    }

    srq->srqn = resp.srqn;
    return &srq->verbs_srq.srq;

err_srq_db:
    hns_roce_free_db(to_hr_ctx(pd->context), srq->db, HNS_ROCE_QP_TYPE_DB);
    hns_roce_free_srq_buf(srq);
    hns_roce_free_idx_que(srq);
    pthread_spin_destroy(&srq->lock);
out:
    free(srq);
    srq = NULL;
    return NULL;
}

int hns_roce_u_modify_srq(struct ibv_srq *srq, struct ibv_srq_attr *srq_attr,
                          int srq_attr_mask)
{
    struct ibv_modify_srq cmd;

    HNS_ROCE_U_NULL_POINT_RETURN_ERR(srq);
    HNS_ROCE_U_NULL_POINT_RETURN_ERR(srq_attr);

    return ibv_cmd_modify_srq(srq, srq_attr, srq_attr_mask, &cmd, sizeof(cmd));
}

int hns_roce_u_query_srq(struct ibv_srq *srq, struct ibv_srq_attr *srq_attr)
{
    struct ibv_query_srq cmd;

    HNS_ROCE_U_NULL_POINT_RETURN_ERR(srq);
    HNS_ROCE_U_NULL_POINT_RETURN_ERR(srq_attr);

    return ibv_cmd_query_srq(srq, srq_attr, &cmd, sizeof(cmd));
}

int hns_roce_u_destroy_srq(struct ibv_srq *srq)
{
    int ret;

    HNS_ROCE_U_NULL_POINT_RETURN_ERR(srq);
    ret = ibv_cmd_destroy_srq(srq);
    if (ret) {
        roce_err("ibv_cmd_destroy_srq failed!, ret [%d], expect 0", ret);
        return ret;
    }

    hns_roce_free_db(to_hr_ctx(srq->context), to_hr_srq(srq)->db,
                     HNS_ROCE_QP_TYPE_DB);
    hns_roce_free_buf(&to_hr_srq(srq)->buf);
    free(to_hr_srq(srq)->wrid);
    to_hr_srq(srq)->wrid = NULL;
    hns_roce_free_buf(&to_hr_srq(srq)->idx_que.buf);
    free(to_hr_srq(srq)->idx_que.bitmap);
    to_hr_srq(srq)->idx_que.bitmap = NULL;
    pthread_spin_destroy(&to_hr_srq(srq)->lock);
    free(to_hr_srq(srq));
    srq = NULL;

    return 0;
}

static int hns_roce_u_map_notify(unsigned long long notify_va, unsigned long long size, int cmd_fd)
{
    if (mmap((void *)(uintptr_t)notify_va, size, PROT_READ | PROT_WRITE, MAP_SHARED,
        cmd_fd, HNS_ROCE_NOTIFY_OFFSET) == MAP_FAILED) {
        roce_err("Warning: Failed to mmap notify memory page, cmd_fd[%d], errno[%d]", cmd_fd, errno);
        return -ENOMEM;
    }
    return 0;
}

int hns_roce_u_exp_query_notify(struct ibv_context *ibv_context,
                                unsigned long long *notify_va, unsigned long long *size)
{
    int ret;
    struct hns_roce_context *context = NULL;
    HNS_ROCE_U_NULL_POINT_RETURN_ERR(ibv_context);
    HNS_ROCE_U_NULL_POINT_RETURN_ERR(notify_va);
    HNS_ROCE_U_NULL_POINT_RETURN_ERR(size);
    context = to_hr_ctx(ibv_context);
    context->notify_va_base = (void *)(uintptr_t)*notify_va;
    context->notify_size = *size;

    ret = hns_roce_u_map_notify(*notify_va, *size, ibv_context->cmd_fd);
    if (ret) {
        roce_err("hns_roce_u_map_notify failed, ret[%d]", ret);
        return ret;
    }

    return 0;
}

int hns_roce_u_exp_query_device(struct ibv_context *ibv_context, struct dev_cap_info *cap)
{
    struct hns_roce_context *context = NULL;
    struct hns_roce_device *hr_dev = NULL;

    HNS_ROCE_U_NULL_POINT_RETURN_ERR(ibv_context);
    HNS_ROCE_U_NULL_POINT_RETURN_ERR(cap);

    context = to_hr_ctx(ibv_context);
    hr_dev = to_hr_dev(ibv_context->device);

    cap->num_qps = (unsigned int)context->num_qps;
    cap->port_num = context->port_num;
    cap->qp_table_mask = context->qp_table_mask;
    cap->qp_table_shift = context->qp_table_shift;
    cap->max_qp_wr = context->max_qp_wr;
    cap->max_sge = context->max_sge;
    cap->page_size = (unsigned int)hr_dev->page_size;

    return 0;
}

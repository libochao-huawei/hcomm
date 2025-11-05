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
#include "hns_roce_lite.h"
#include "rdma_lite.h"
#include "user_log.h"
#include "hns_roce_u.h"
#include "hns_roce_u_sec.h"
#include "hns_roce_lite_stdio.h"

#ifndef DEFINE_HNS_LLT
#define STATIC static
#else
#define STATIC
#endif

int hns_roce_verify_lite_qp(struct rdma_lite_qp_attr *attr)
{
    if (attr->cap.max_recv_sge < 1) {
        attr->cap.max_recv_sge = 1;
    }

    if (attr->qp_type != RDMA_LITE_QPT_RC) {
        roce_err("verify failed, qp_type is %d", attr->qp_type);
        return -EINVAL;
    }

    return 0;
}

void hns_roce_set_lite_qp_attr(struct hns_roce_lite_qp *qp, struct rdma_lite_qp_attr *attr)
{
    struct hns_roce_lite_cq *send_cq = NULL;
    struct hns_roce_lite_cq *recv_cq = NULL;

    qp->sq.wqe_shift = attr->device_qp_attr.sq.wqe_shift;
    qp->rq.wqe_shift = attr->device_qp_attr.rq.wqe_shift;
    qp->sq.wqe_cnt = attr->device_qp_attr.sq.wqe_cnt;
    qp->rq.wqe_cnt = attr->device_qp_attr.rq.wqe_cnt;
    qp->sq.shift = attr->device_qp_attr.sq.shift;
    qp->rq.shift = attr->device_qp_attr.rq.shift;
    qp->sq.max_gs = attr->device_qp_attr.sq.max_gs;
    qp->rq.max_gs = attr->device_qp_attr.rq.max_gs;
    qp->sq.offset = attr->device_qp_attr.sq.offset;
    qp->rq.offset = attr->device_qp_attr.rq.offset;
    qp->sge.sge_cnt = attr->device_qp_attr.sge.sge_cnt;
    qp->sge.sge_shift = attr->device_qp_attr.sge.sge_shift;
    qp->sge.offset = attr->device_qp_attr.sge.offset;
    qp->sq.wrid_len = attr->device_qp_attr.sq.wrid_len;
    qp->rq.wrid_len = attr->device_qp_attr.rq.wrid_len;

    qp->rq.max_post = (unsigned int)attr->device_qp_attr.rq.max_post;
    qp->sq.max_post = (unsigned int)attr->device_qp_attr.sq.max_post;
    qp->max_inline_data =  attr->device_qp_attr.max_inline_data;
    qp->sq_signal_bits = (attr->sq_sig_all != 0 ? 0 : 1);
    qp->gdr_enabled = attr->qp_mode;

    send_cq = attr->send_cq ? to_hr_lite_cq(attr->send_cq) : NULL;
    recv_cq = attr->recv_cq ? to_hr_lite_cq(attr->recv_cq) : NULL;

    if (send_cq != NULL) {
        list_add_tail(&send_cq->list_sq, &qp->scq_list);
    }

    if (recv_cq != NULL) {
        list_add_tail(&recv_cq->list_rq, &qp->rcq_list);
    }

    return;
}

int hns_roce_lite_qp_lock_init(struct hns_roce_lite_qp *qp)
{
    int ret;

    ret = pthread_spin_init(&qp->sq.lock, PTHREAD_PROCESS_PRIVATE);
    if (ret) {
        roce_err("lite_sq lock init failed!,ret:%d", ret);
        return ret;
    }

    ret = pthread_spin_init(&qp->rq.lock, PTHREAD_PROCESS_PRIVATE);
    if (ret) {
        roce_err("lite_rq lock init failed!,ret:%d", ret);
        pthread_spin_destroy(&qp->sq.lock);
    }

    return ret;
}

void hns_roce_lite_qp_lock_uninit(struct hns_roce_lite_qp *qp)
{
    pthread_spin_destroy(&qp->rq.lock);
    pthread_spin_destroy(&qp->sq.lock);
}

void hns_roce_init_lite_qp_indices(struct hns_roce_lite_qp *qp)
{
    qp->sq.head = 0;
    qp->sq.tail = 0;
    qp->rq.head = 0;
    qp->rq.tail = 0;
    qp->next_sge = 0;
}

int hns_roce_store_lite_qp(struct hns_roce_lite_context *ctx, uint32_t qpn, struct hns_roce_lite_qp *qp)
{
    uint32_t tind = (qpn & ((uint32_t)ctx->num_qps - 1)) >> (uint32_t)ctx->qp_table_shift;

    if (ctx->qp_table[tind].refcnt == 0) {
        HNS_ROCE_U_PARA_CHECK_RETURN_INT((ctx->qp_table_mask + 1));
        ctx->qp_table[tind].table = calloc((unsigned long)(ctx->qp_table_mask + 1), sizeof(struct hns_roce_lite_qp *));
        if (ctx->qp_table[tind].table == NULL) {
            roce_err("calloc qp table failed, qp_table_mask is 0x%x", ctx->qp_table_mask);
            return -1;
        }
    }

    ctx->qp_table[tind].refcnt++;
    ctx->qp_table[tind].table[qpn & (u32)(ctx->qp_table_mask)] = qp;

    return 0;
}

void hns_roce_clear_lite_qp(struct hns_roce_lite_context *ctx, uint32_t qpn)
{
    u32 tind = (qpn & ((u32)(ctx->num_qps) - 1)) >> (u32)ctx->qp_table_shift;

    if (--ctx->qp_table[tind].refcnt == 0) {
        free(ctx->qp_table[tind].table);
        ctx->qp_table[tind].table = NULL;
    } else {
        ctx->qp_table[tind].table[qpn & (uint32_t)(ctx->qp_table_mask)] = NULL;
    }
}

void *hns_roce_lite_mmap_host_va(u64 device_va, u32 device_va_len, struct hns_roce_lite_context *ctx)
{
    u64 align_va = align_down(device_va, ctx->page_size);
    u64 align_len = align_up(device_va_len, ctx->page_size);
    void *dst_addr;
    int ret;

    ret = halHostRegister((void *)align_va, align_len, DEV_MEM_MAP_HOST | MEM_REGISTER_HCCP_PROC_TYPE, ctx->dev_id,
        &dst_addr);
    if (ret) {
        roce_err("hns_roce_lite_mmap_host_va halHostRegister fail, "
            "device va: 0x%llx, len: 0x%x, align device len: 0x%llx", device_va, device_va_len, align_len);
        return NULL;
    }

    dst_addr = (align_va ^ device_va) + (char *)dst_addr;

    return dst_addr;
}

int hns_roce_lite_unmmap_host_va(u64 device_va, struct hns_roce_lite_context *ctx)
{
    u64 align_va = align_down(device_va, ctx->page_size);
    int ret;

    ret = halHostUnregisterEx((void *)align_va, ctx->dev_id, DEV_MEM_MAP_HOST | MEM_REGISTER_HCCP_PROC_TYPE);
    if (ret) {
        roce_err("hns_roce_lite_unmmap_host_va halHostUnregisterEx fail, device va: 0x%llx, ret = %d", align_va, ret);
        return ret;
    }

    return 0;
}

STATIC void *hns_roce_lite_mmap_db(u64 device_va, u32 dva_len, struct hns_roce_lite_context *ctx)
{
    u64 align_va = align_down(device_va, ctx->page_size);
    struct db_dva_node *db_node = NULL;
    struct db_dva_node *db_next = NULL;
    void *host_va = NULL;

    (void)pthread_mutex_lock(&ctx->mutex);
    list_for_each(&ctx->db_list, db_next, entry) {
        if (db_next->db_align_dva == align_va) {
            host_va = (void *)align_down((u64)db_next->hva, ctx->page_size);
            host_va = (align_va ^ device_va) + (char *)host_va;
            db_next->ref_cnt++;
            goto out;
        }
    }

    db_node = (struct db_dva_node *)calloc(1, sizeof(struct db_dva_node));
    if (db_node == NULL) {
        roce_err("%s calloc db_node failed", __func__);
        (void)pthread_mutex_unlock(&ctx->mutex);
        return NULL;
    }

    host_va = hns_roce_lite_mmap_host_va(device_va, dva_len, ctx);
    if (host_va == NULL) {
        free(db_node);
        roce_err("%s hns_roce_lite_mmap_host_va host_va fail", __func__);
        goto out;
    }

    db_node->db_align_dva = align_va;
    db_node->hva = host_va;
    db_node->ref_cnt++;
    list_add_tail(&ctx->db_list, &db_node->entry);

out:
    (void)pthread_mutex_unlock(&ctx->mutex);

    return host_va;
}

STATIC int hns_roce_lite_unmmap_db(u64 device_va, struct hns_roce_lite_context *ctx)
{
    u64 align_va = align_down(device_va, ctx->page_size);
    struct db_dva_node *db_next = NULL;
    int ret;

    (void)pthread_mutex_lock(&ctx->mutex);
    list_for_each(&ctx->db_list, db_next, entry) {
        if (db_next->db_align_dva == align_va) {
            db_next->ref_cnt--;
            if (db_next->ref_cnt == 0) {
                list_del(&db_next->entry);
                free(db_next);
                db_next = NULL;
                ret = hns_roce_lite_unmmap_host_va(device_va, ctx);
                if (ret) {
                    roce_err("hns_roce_lite_unmmap_host_va failed, ret = %d", ret);
                    (void)pthread_mutex_unlock(&ctx->mutex);
                    return ret;
                }
            }
            goto out;
        }
    }

out:
    (void)pthread_mutex_unlock(&ctx->mutex);
    return 0;
}

int hns_roce_lite_mmap_hva(struct rdma_lite_device_buf *dev_buf, struct rdma_lite_host_buf *host_buf,
    struct hns_roce_lite_context *ctx)
{
    struct rdma_lite_mem_pool *mem_pool = NULL;
    unsigned int alloc_size = 0;

    host_buf->dva = dev_buf->va;
    host_buf->length = dev_buf->len;
    if (ctx->page_size != PAGE_ALIGN_2MB) {
        host_buf->hva = hns_roce_lite_mmap_host_va(dev_buf->va, host_buf->length, ctx);
        goto page_align_4k_out;
    }

    if (host_buf->mem_idx >= BITMAP_WORD_SIZE) {
        roce_err("param error, mem_idx[%u] >= %u", host_buf->mem_idx, BITMAP_WORD_SIZE);
        return -EINVAL;
    }

    mem_pool = ctx->mem_pool_list[host_buf->mem_idx];
    if (mem_pool == NULL) {
        roce_err("param error, mem_idx[%u] not found in mem_pool_list", host_buf->mem_idx);
        return -EIO;
    }

    alloc_size = (unsigned int)align_up(host_buf->length, PAGE_ALIGN_4KB);
    if (mem_pool->offset + alloc_size > mem_pool->host_buf.length) {
        roce_err("mem pool mem exhaust, mem_idx:%u, alloc_size:0x%x", host_buf->mem_idx, alloc_size);
        return -ENOMEM;
    }
    host_buf->hva = (char *)(mem_pool->host_buf.hva) + mem_pool->offset;
    mem_pool->offset += alloc_size;
    mem_pool->used_size += alloc_size;

page_align_4k_out:
    if (host_buf->hva == NULL) {
        roce_err("hns_roce_lite_mmap_host_va failed, host_buf->hva is NULL");
        return -ENOMEM;
    }
    return 0;
}

int hns_roce_lite_unmmap_hva(struct rdma_lite_host_buf *host_buf, struct hns_roce_lite_context *ctx)
{
    struct rdma_lite_mem_pool *mem_pool = NULL;
    unsigned int free_size = 0;

    if (ctx->page_size != PAGE_ALIGN_2MB) {
        return hns_roce_lite_unmmap_host_va(host_buf->dva, ctx);
    }

    if (host_buf->mem_idx >= BITMAP_WORD_SIZE) {
        roce_err("param error, mem_idx[%u] >= %u", host_buf->mem_idx, BITMAP_WORD_SIZE);
        return -EINVAL;
    }

    mem_pool = ctx->mem_pool_list[host_buf->mem_idx];
    if (mem_pool == NULL || mem_pool->host_buf.dva != host_buf->dva) {
        roce_err("param error, mem_idx[%u] mem_pool[%pK] is NULL or dva not match", host_buf->mem_idx, mem_pool);
        return -EINVAL;
    }

    free_size = (unsigned int)align_up(host_buf->length, PAGE_ALIGN_4KB);
    if (mem_pool->used_size < free_size) {
        roce_err("param error, mem_pool->used_size[%u] > free_size[%u]", mem_pool->used_size, free_size);
        return -EINVAL;
    }

    mem_pool->used_size -= free_size;
    return 0;
}

int hns_roce_lite_mmap_hdb(struct rdma_lite_device_buf *dev_buf, struct rdma_lite_host_buf *host_buf,
    struct hns_roce_lite_context *ctx)
{
    int ret;

    host_buf->dva = dev_buf->va;
    host_buf->length = dev_buf->len;
    if (ctx->page_size != PAGE_ALIGN_2MB) {
        host_buf->hva = hns_roce_lite_mmap_db(host_buf->dva, host_buf->length, ctx);
        goto page_align_4k_out;
    }

    ret = hns_roce_lite_mmap_hva(dev_buf, host_buf, ctx);

    return ret;

page_align_4k_out:
    if (host_buf->hva == NULL) {
        roce_err("hns_roce_lite_mmap_db failed, host_buf->hva is NULL");
        return -ENOMEM;
    }
    return 0;
}

int hns_roce_lite_unmmap_hdb(struct rdma_lite_host_buf *host_buf, struct hns_roce_lite_context *ctx)
{
    if (ctx->page_size != PAGE_ALIGN_2MB) {
        return hns_roce_lite_unmmap_db(host_buf->dva, ctx);
    }

    return hns_roce_lite_unmmap_hva(host_buf, ctx);
}

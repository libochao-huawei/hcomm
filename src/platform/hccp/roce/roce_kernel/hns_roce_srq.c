/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <rdma/ib_umem.h>
#include "hns-abi.h"
#include "roce_k_compat.h"
#include "hns_roce_device.h"
#include "hns_roce_cmd.h"
#include "hns_roce_hem.h"
#include "hns_roce_common.h"
#include "hns_roce_sec.h"

#if defined(HNS_ROCE_LLT) || defined(DEFINE_HNS_LLT) || defined(CONFIG_LLT)
#define STATIC
#else
#define STATIC static
#endif

#define PAGE_CNT        2

void hns_roce_srq_event(struct hns_roce_dev *hr_dev, u32 srqn, int event_type)
{
    struct hns_roce_srq_table *srq_table = &hr_dev->srq_table;
    struct hns_roce_srq *srq = NULL;

    rcu_read_lock();
    srq = radix_tree_lookup(&srq_table->tree, srqn & (unsigned int)(hr_dev->caps.num_srqs - 1));
    rcu_read_unlock();
    if (srq != NULL) {
        atomic_inc(&srq->refcount.refs);
    } else {
        dev_warn(hr_dev->dev, "Async event for bogus SRQ %08x\n", srqn);
        return;
    }

    srq->event(srq, event_type);

    if (atomic_dec_and_test(&srq->refcount.refs)) {
        complete(&srq->free);
    }
}

STATIC void hns_roce_ib_srq_event(struct hns_roce_srq *srq, enum hns_roce_event event_type)
{
    struct hns_roce_dev *hr_dev = to_hr_dev(srq->ibsrq.device);
    struct ib_srq *ibsrq = &srq->ibsrq;
    struct ib_event event;

    if (ibsrq->event_handler) {
        event.device      = ibsrq->device;
        event.element.srq = ibsrq;
        switch (event_type) {
            case HNS_ROCE_EVENT_TYPE_SRQ_LIMIT_REACH:
                event.event = IB_EVENT_SRQ_LIMIT_REACHED;
                break;
            case HNS_ROCE_EVENT_TYPE_SRQ_CATAS_ERROR:
                event.event = IB_EVENT_SRQ_ERR;
                break;
            default:
                dev_err(hr_dev->dev, "hns_roce:Unexpected event type 0x%x on SRQ %06lx\n", event_type, srq->srqn);
                return;
        }

        ibsrq->event_handler(&event, ibsrq->srq_context);
    }
}

static int hns_roce_sw2hw_srq(struct hns_roce_dev *dev, struct hns_roce_cmd_mailbox *mailbox,
    unsigned long srq_num)
{
    struct hns_roce_mbox_post_params mbox_pst_params = {0};

    mbox_pst_params.in_param = mailbox->dma;
    mbox_pst_params.out_param = 0;
    mbox_pst_params.in_modifier = srq_num;
    mbox_pst_params.op_modifier = 0;
    mbox_pst_params.op = HNS_ROCE_CMD_SW2HW_SRQ;
    return hns_roce_cmd_mbox(dev, mbox_pst_params, HNS_ROCE_CMD_TIMEOUT_MSECS);
}

static int hns_roce_hw2sw_srq(struct hns_roce_dev *dev, struct hns_roce_cmd_mailbox *mailbox,
    unsigned long srq_num)
{
    struct hns_roce_mbox_post_params mbox_pst_params = {0};

    mbox_pst_params.in_param = 0;
    mbox_pst_params.out_param = mailbox ? mailbox->dma : 0;
    mbox_pst_params.in_modifier = srq_num;
    mbox_pst_params.op_modifier = mailbox ? 0 : 1;
    mbox_pst_params.op = HNS_ROCE_CMD_HW2SW_SRQ;
    return hns_roce_cmd_mbox(dev, mbox_pst_params, HNS_ROCE_CMD_TIMEOUT_MSECS);
}

STATIC int hns_roce_srq_alloc_find(struct hns_roce_dev *hr_dev, struct hns_roce_srq *srq,
    dma_addr_t *dma_handle_wqe, u64 **mtts_idx, dma_addr_t *dma_handle_idx)
{
    u64 *mtts_wqe = NULL;

    /* Get the physical address of srq buf */
    mtts_wqe = hns_roce_table_find(hr_dev, &hr_dev->mr_table.mtt_srqwqe_table, srq->mtt.first_seg, dma_handle_wqe);
    if (mtts_wqe == NULL) {
        dev_err(hr_dev->dev, "SRQ alloc.Failed to find srq buf addr.\n");
        return -EINVAL;
    }

    /* Get physical address of idx que buf */
    *mtts_idx = hns_roce_table_find(hr_dev, &hr_dev->mr_table.mtt_idx_table, srq->idx_que.mtt.first_seg,
        dma_handle_idx);
    if (*mtts_idx == NULL) {
        dev_err(hr_dev->dev, "SRQ alloc.Failed to find idx que buf addr.\n");
        return -EINVAL;
    }

    return 0;
}

STATIC void hns_roce_fill_srqc_info(struct hns_roce_wrt_srqc_info *srqc_info,
    struct hns_roce_srq_info srq_info, dma_addr_t dma_handle_wqe, dma_addr_t dma_handle_idx)
{
    srqc_info->pdn = srq_info.pdn;
    srqc_info->cqn = srq_info.cqn;
    srqc_info->xrcd = srq_info.xrcd;
    srqc_info->dma_handle_idx = dma_handle_idx;
    srqc_info->dma_handle_wqe = dma_handle_wqe;
}

STATIC int hns_roce_srq_fill_hr_dev(struct hns_roce_dev *hr_dev, struct hns_roce_srq *srq,
    struct hns_roce_wrt_srqc_info srqc_info, u64 *mtts_idx, struct hns_roce_cmd_mailbox *mailbox)
{
    int ret;
    hr_dev->hw->write_srqc(hr_dev, srq, srqc_info, mtts_idx, mailbox->buf);

    ret = hns_roce_sw2hw_srq(hr_dev, mailbox, srq->srqn);
    if (ret) {
        dev_err(hr_dev->dev, "sw 2 hw srq err, ret:%d\n", ret);
        return ret;
    }

    refcount_set(&srq->refcount, 1);
    init_completion(&srq->free);
    return 0;
}

static int hns_roce_srq_tree_insert(struct hns_roce_dev *hr_dev, struct hns_roce_srq *srq)
{
    int ret;

    spin_lock_irq(&hr_dev->srq_table.lock);
    ret = radix_tree_insert(&hr_dev->srq_table.tree, srq->srqn, srq);
    spin_unlock_irq(&hr_dev->srq_table.lock);
    if (ret) {
        dev_err(hr_dev->dev, "radix_tree_insert err, srq - 0x%lx.\n", srq->srqn);
    }

    return ret;
}

static void hns_roce_srq_tree_delete(struct hns_roce_dev *hr_dev, struct hns_roce_srq *srq)
{
    spin_lock_irq(&hr_dev->srq_table.lock);
    radix_tree_delete(&hr_dev->srq_table.tree, srq->srqn);
    spin_unlock_irq(&hr_dev->srq_table.lock);
}

STATIC int hns_roce_srq_alloc(struct hns_roce_dev *hr_dev, struct hns_roce_srq_info srq_info,
                              struct hns_roce_mtt *hr_mtt, u64 db_rec_addr, struct hns_roce_srq *srq)
{
    struct hns_roce_cmd_mailbox *mailbox = NULL;
    struct hns_roce_wrt_srqc_info srqc_info = {0};
    dma_addr_t dma_handle_wqe, dma_handle_idx;
    u64 *mtts_idx = NULL;
    int ret;

    ret = hns_roce_srq_alloc_find(hr_dev, srq, &dma_handle_wqe, &mtts_idx, &dma_handle_idx);
    if (ret) {
        dev_err(hr_dev->dev, "SRQ alloc.Failed to find index.\n");
        return ret;
    }
    ret = hns_roce_bitmap_alloc(&hr_dev->srq_table.bitmap, &srq->srqn);
    if (ret == -1) {
        dev_err(hr_dev->dev, "SRQ alloc.Failed to alloc index.\n");
        return -ENOMEM;
    }

    ret = hns_roce_table_get(hr_dev, &hr_dev->srq_table.table, srq->srqn);
    if (ret) {
        dev_err(hr_dev->dev, "SRQ alloc.Failed to get table, srq - 0x%lx.\n", srq->srqn);
        goto err_out;
    }

    ret = hns_roce_srq_tree_insert(hr_dev, srq);
    if (ret) {
        dev_err(hr_dev->dev, "hns_roce_srq_tree_insert err, srq - 0x%lx.\n", srq->srqn);
        goto err_put;
    }

    mailbox = hns_roce_alloc_cmd_mailbox(hr_dev);
    if (IS_ERR(mailbox)) {
        dev_err(hr_dev->dev, "hns_roce_alloc_cmd_mailbox err, srq - 0x%lx.\n", srq->srqn);
        ret = PTR_ERR(mailbox);
        goto err_radix;
    }

    hns_roce_fill_srqc_info(&srqc_info, srq_info, dma_handle_wqe, dma_handle_idx);

    ret = hns_roce_srq_fill_hr_dev(hr_dev, srq, srqc_info, mtts_idx, mailbox);
    hns_roce_free_cmd_mailbox(hr_dev, mailbox);
    if (ret) {
        dev_err(hr_dev->dev, "hns_roce_srq_fill_hr_dev err, srq - 0x%lx.\n", srq->srqn);
        goto err_radix;
    }
    return 0;

err_radix:
    hns_roce_srq_tree_delete(hr_dev, srq);

err_put:
    hns_roce_table_put(hr_dev, &hr_dev->srq_table.table, srq->srqn);

err_out:
    hns_roce_bitmap_free(&hr_dev->srq_table.bitmap, srq->srqn, BITMAP_NO_RR);
    return ret;
}

STATIC void hns_roce_srq_free(struct hns_roce_dev *hr_dev, struct hns_roce_srq *srq)
{
    struct hns_roce_srq_table *srq_table = &hr_dev->srq_table;
    int ret;

    ret = hns_roce_hw2sw_srq(hr_dev, NULL, srq->srqn);
    if (ret) {
        dev_err(hr_dev->dev, "HW2SW_SRQ failed (%d) for CQN %06lx\n", ret, srq->srqn);
    }

    spin_lock_irq(&srq_table->lock);
    radix_tree_delete(&srq_table->tree, srq->srqn);
    spin_unlock_irq(&srq_table->lock);

    if (atomic_dec_and_test(&srq->refcount.refs)) {
        complete(&srq->free);
    }
    wait_for_completion(&srq->free);

    hns_roce_table_put(hr_dev, &srq_table->table, srq->srqn);
    hns_roce_bitmap_free(&srq_table->bitmap, srq->srqn, BITMAP_NO_RR);
}

STATIC int create_srq_umem(struct hns_roce_dev *hr_dev, struct hns_roce_srq *srq,
    struct hns_roce_ib_create_srq ucmd, int srq_buf_size)
{
    u32 page_shift, npages;
    int ret;

    srq->umem = ib_umem_get(&hr_dev->ib_dev, ucmd.buf_addr, srq_buf_size, 0);
    if (IS_ERR(srq->umem)) {
        dev_err(hr_dev->dev, "ib_umem got err\n");
        return PTR_ERR(srq->umem);
    }

    if (hr_dev->caps.srqwqe_buf_pg_sz) {
        npages = (ib_umem_num_pages(srq->umem) +
                  (1 << hr_dev->caps.srqwqe_buf_pg_sz) - 1) / (1 << hr_dev->caps.srqwqe_buf_pg_sz);
        page_shift = PAGE_SHIFT + hr_dev->caps.srqwqe_buf_pg_sz;
        ret = hns_roce_mtt_init(hr_dev, npages, page_shift, &srq->mtt);
    } else {
        ret = hns_roce_mtt_init(hr_dev, ib_umem_num_pages(srq->umem), PAGE_SHIFT, &srq->mtt);
    }

    if (ret) {
        dev_err(hr_dev->dev, "mtt init error when create srq\n");
        ib_umem_release(srq->umem);
        return ret;
    }

    ret = hns_roce_ib_umem_write_mtt(hr_dev, &srq->mtt, srq->umem);
    if (ret) {
        dev_err(hr_dev->dev, "hns_roce_ib_umem write mtt failed\n");
        hns_roce_mtt_cleanup(hr_dev, &srq->mtt);
        ib_umem_release(srq->umem);
        return ret;
    }

    return 0;
}

static void free_srq_umem(struct hns_roce_dev *hr_dev, struct hns_roce_srq *srq)
{
    hns_roce_mtt_cleanup(hr_dev, &srq->mtt);
    ib_umem_release(srq->umem);
}

STATIC int create_srq_idx_que_umem(struct hns_roce_dev *hr_dev, struct hns_roce_srq *srq,
    struct hns_roce_ib_create_srq ucmd)
{
    u32 page_shift, npages;
    int ret;

    /* config index queue BA */
    srq->idx_que.umem = ib_umem_get(&hr_dev->ib_dev, ucmd.que_addr, srq->idx_que.buf_size, 0);
    if (IS_ERR(srq->idx_que.umem)) {
        dev_err(hr_dev->dev, "umem get error for idx que\n");
        return -1;
    }

    if (hr_dev->caps.idx_buf_pg_sz) {
        npages = (ib_umem_num_pages(srq->idx_que.umem) + (1 << hr_dev->caps.idx_buf_pg_sz) - 1) /
                 (1 << hr_dev->caps.idx_buf_pg_sz);
        page_shift = PAGE_SHIFT + hr_dev->caps.idx_buf_pg_sz;
        ret = hns_roce_mtt_init(hr_dev, npages, page_shift,
                                &srq->idx_que.mtt);
    } else {
        ret = hns_roce_mtt_init(hr_dev, ib_umem_num_pages(srq->idx_que.umem), PAGE_SHIFT,
                                &srq->idx_que.mtt);
    }

    if (ret) {
        ib_umem_release(srq->idx_que.umem);
        dev_err(hr_dev->dev, "mtt init error when create srq\n");
        return ret;
    }

    ret = hns_roce_ib_umem_write_mtt(hr_dev, &srq->idx_que.mtt, srq->idx_que.umem);
    if (ret) {
        dev_err(hr_dev->dev, "write mtt error for idx que, ret[%d]\n", ret);
        hns_roce_mtt_cleanup(hr_dev, &srq->idx_que.mtt);
        ib_umem_release(srq->idx_que.umem);
        return ret;
    }

    return 0;
}

STATIC int create_user_srq(struct hns_roce_srq *srq, struct ib_udata *udata, int srq_buf_size)
{
    struct hns_roce_dev *hr_dev = to_hr_dev(srq->ibsrq.device);
    struct hns_roce_ib_create_srq ucmd;
    int ret;

    if (ib_copy_from_udata(&ucmd, udata, sizeof(ucmd))) {
        dev_err(hr_dev->dev, "ib copy from udata failed\n");
        return -EFAULT;
    }

    ret = create_srq_umem(hr_dev, srq, ucmd, srq_buf_size);
    if (ret) {
        dev_err(hr_dev->dev, "create srq umem failed\n");
        return ret;
    }

    ret = create_srq_idx_que_umem(hr_dev, srq, ucmd);
    if (ret) {
        dev_err(hr_dev->dev, "create srq idx que umem failed\n");
        free_srq_umem(hr_dev, srq);
        return ret;
    }

    return 0;
}

STATIC int hns_roce_create_idx_que(struct hns_roce_srq *srq, u32 page_shift)
{
    struct hns_roce_dev *hr_dev = to_hr_dev(srq->ibsrq.device);
    struct hns_roce_idx_que *idx_que = &srq->idx_que;
    u32 bitmap_num;
    u32 i;

    idx_que->entry_sz = HNS_ROCE_IDX_QUE_ENTRY_SZ;
    bitmap_num = HNS_ROCE_ALOGN_UP(srq->max, BITS_PER_LONG_LONG);

    idx_que->bitmap = kcalloc(1, bitmap_num / BITS_PER_BYTE, GFP_KERNEL);
    if (idx_que->bitmap == NULL) {
        dev_err(hr_dev->dev, "alloc hns_roce_idx_que bit failed\n");
        return -ENOMEM;
    }

    bitmap_num = bitmap_num / BITS_PER_LONG_LONG;

    idx_que->buf_size = srq->max * idx_que->entry_sz;

    if (hns_roce_buf_alloc(hr_dev, idx_que->buf_size, (1 << page_shift) * PAGE_CNT,
                           &idx_que->idx_buf, page_shift)) {
        dev_err(hr_dev->dev, "alloc idx_que idx_buf failed\n");
        kfree(idx_que->bitmap);
        idx_que->bitmap = NULL;
        return -ENOMEM;
    }

    for (i = 0; i < bitmap_num; i++) {
        idx_que->bitmap[i] = ~(0UL);
    }
    idx_que->bitmap_len = bitmap_num;

    return 0;
}

STATIC int hns_roce_srq_init(struct hns_roce_srq *srq, int srq_buf_size)
{
    struct hns_roce_dev *hr_dev = to_hr_dev(srq->ibsrq.device);
    u32 page_shift = PAGE_SHIFT + hr_dev->caps.srqwqe_buf_pg_sz;
    int ret;

    if (hns_roce_buf_alloc(hr_dev, srq_buf_size, (1 << page_shift) * PAGE_CNT, &srq->buf, page_shift)) {
        dev_err(hr_dev->dev, "alloc idx_que idx_buf failed\n");
        return -ENOMEM;
    }

    srq->head = 0;
    srq->tail = srq->max - 1;
    srq->wqe_ctr = 0;

    ret = hns_roce_mtt_init(hr_dev, srq->buf.npages, srq->buf.page_shift, &srq->mtt);
    if (ret) {
        dev_err(hr_dev->dev, "mtt init error when create srq, ret[%d]\n", ret);
        goto err_kernel_buf;
    }

    ret = hns_roce_buf_write_mtt(hr_dev, &srq->mtt, &srq->buf);
    if (ret) {
        dev_err(hr_dev->dev, "hns_roce_buf wrote mtt failed, ret[%d]\n", ret);
        goto err_kernel_srq_mtt;
    }

    page_shift = PAGE_SHIFT + hr_dev->caps.idx_buf_pg_sz;
    ret = hns_roce_create_idx_que(srq, page_shift);
    if (ret) {
        dev_err(hr_dev->dev, "create idx queue fail, ret[%d]\n", ret);
        goto err_kernel_srq_mtt;
    }

    /* Init mtt table for idx_que */
    ret = hns_roce_mtt_init(hr_dev, srq->idx_que.idx_buf.npages, srq->idx_que.idx_buf.page_shift, &srq->idx_que.mtt);
    if (ret) {
        dev_err(hr_dev->dev, "mtt init error for idx que\n");
        goto err_kernel_create_idx;
    }

    return 0;

err_kernel_create_idx:
    hns_roce_buf_free(hr_dev, srq->idx_que.buf_size, &srq->idx_que.idx_buf);
    kfree(srq->idx_que.bitmap);
    srq->idx_que.bitmap = NULL;

err_kernel_srq_mtt:
    hns_roce_mtt_cleanup(hr_dev, &srq->mtt);

err_kernel_buf:
    hns_roce_buf_free(hr_dev, srq_buf_size, &srq->buf);

    return ret;
}

static void hns_roce_srq_deinit(struct hns_roce_dev *hr_dev, struct hns_roce_srq *srq, int srq_buf_size)
{
    hns_roce_mtt_cleanup(hr_dev, &srq->idx_que.mtt);
    hns_roce_buf_free(hr_dev, srq->idx_que.buf_size, &srq->idx_que.idx_buf);
    kfree(srq->idx_que.bitmap);
    srq->idx_que.bitmap = NULL;
    hns_roce_mtt_cleanup(hr_dev, &srq->mtt);
    hns_roce_buf_free(hr_dev, srq_buf_size, &srq->buf);
}

STATIC int create_kernel_srq(struct hns_roce_srq *srq, int srq_buf_size)
{
    struct hns_roce_dev *hr_dev = to_hr_dev(srq->ibsrq.device);
    int ret;

    ret = hns_roce_srq_init(srq, srq_buf_size);
    if (ret != 0) {
        dev_err(hr_dev->dev, "hns_roce_srq fail, ret[%d]\n", ret);
        return ret;
    }

    /* Write buffer address into the mtt table */
    ret = hns_roce_buf_write_mtt(hr_dev, &srq->idx_que.mtt, &srq->idx_que.idx_buf);
    if (ret) {
        dev_err(hr_dev->dev, "write mtt error for idx que\n");
        goto err_kernel_idx_buf;
    }
    srq->wrid = kvmalloc_array(srq->max, sizeof(u64), GFP_KERNEL);
    if (srq->wrid == NULL) {
        dev_err(hr_dev->dev, "kvmalloc_array operation failed\n");
        ret = -ENOMEM;
        goto err_kernel_idx_buf;
    }
    ret = memset_s(srq->wrid, srq->max * sizeof(u64), 0, srq->max * sizeof(u64));
    if (ret) {
        pr_err("hns3: [create_kernel_srq]memset_s failed, err = %d\n", ret);
        goto err_kernel_free_wrid;
    }

    return 0;

err_kernel_free_wrid:
    kvfree(srq->wrid);
    srq->wrid = NULL;

err_kernel_idx_buf:
    hns_roce_srq_deinit(hr_dev, srq, srq_buf_size);

    return ret;
}

static void destroy_user_srq(struct hns_roce_dev *hr_dev, struct hns_roce_srq *srq)
{
    hns_roce_mtt_cleanup(hr_dev, &srq->idx_que.mtt);
    ib_umem_release(srq->idx_que.umem);
    hns_roce_mtt_cleanup(hr_dev, &srq->mtt);
    ib_umem_release(srq->umem);
}

static void destroy_kernel_srq(struct hns_roce_dev *hr_dev, struct hns_roce_srq *srq, int srq_buf_size)
{
    kvfree(srq->wrid);
    srq->wrid = NULL;
    hns_roce_srq_deinit(hr_dev, srq, srq_buf_size);
}

STATIC bool hns_roce_create_srq_avail_para(struct ib_srq *ib_srq,
    struct ib_srq_init_attr *srq_init_attr, struct ib_udata *udata)
{
    struct hns_roce_dev *hr_dev = NULL;

    if (ib_srq == NULL || srq_init_attr == NULL || udata == NULL) {
        pr_err("hns3: null point:ib_srq %pK, srq_init_attr %pK, udata %pK\n", ib_srq, srq_init_attr, udata);
        return false;
    }
    hr_dev = to_hr_dev(ib_srq->device);
    /* Check the actual SRQ wqe and SRQ sge num */
    if (srq_init_attr->attr.max_wr >= hr_dev->caps.max_srq_wrs ||
            srq_init_attr->attr.max_sge > hr_dev->caps.max_srq_sges) {
        dev_err(hr_dev->dev, "SRQ wqe num [%u, max: %u] or SRQ sge num[%u, max: %u] is invalid\n",
                srq_init_attr->attr.max_wr, hr_dev->caps.max_srq_wrs,
                srq_init_attr->attr.max_sge, hr_dev->caps.max_srq_sges);
        return false;
    }

    return true;
}

STATIC void hns_roce_set_srq(struct hns_roce_srq *srq, struct ib_srq_init_attr *srq_init_attr, int *srq_buf_size)
{
    int srq_desc_size;
    spin_lock_init(&srq->lock);

    srq->max = (unsigned int)roundup_pow_of_two(srq_init_attr->attr.max_wr + 1);
    srq->max_gs = srq_init_attr->attr.max_sge;

    srq_desc_size = max(HNS_ROCE_SGE_SIZE, HNS_ROCE_SGE_SIZE * srq->max_gs);

    srq->wqe_shift = ilog2(srq_desc_size);

    *srq_buf_size = srq->max * srq_desc_size;

    srq->idx_que.entry_sz = HNS_ROCE_IDX_QUE_ENTRY_SZ;
    srq->idx_que.buf_size = srq->max * srq->idx_que.entry_sz;
    srq->mtt.mtt_type = MTT_TYPE_SRQWQE;
    srq->idx_que.mtt.mtt_type = MTT_TYPE_IDX;
}

STATIC int hns_roce_create_srq_queue(struct hns_roce_srq *srq, int srq_buf_size,
    struct ib_udata *udata)
{
    int ret;
    struct hns_roce_dev *hr_dev = to_hr_dev(srq->ibsrq.device);

    if (udata) {
        ret = create_user_srq(srq, udata, srq_buf_size);
        if (ret) {
            dev_err(hr_dev->dev, "Create user srq fail\n");
            return ret;
        }
    } else {
        ret = create_kernel_srq(srq, srq_buf_size);
        if (ret) {
            dev_err(hr_dev->dev, "Create kernel srq fail\n");
            return ret;
        }
    }

    return 0;
}

STATIC int hns_roce_create_srq_ctx(struct ib_pd *pd, struct ib_srq_init_attr *srq_init_attr, struct hns_roce_srq *srq)
{
    u32 cqn;
    int ret;
    struct hns_roce_dev *hr_dev = to_hr_dev(pd->device);
    struct hns_roce_srq_info srq_info = {0};

    cqn = ib_srq_has_cq(srq_init_attr->srq_type) ? to_hr_cq(srq_init_attr->ext.cq)->cqn : 0;

    srq->db_reg_l = hr_dev->reg_base + SRQ_DB_REG;

    srq_info.pdn = to_hr_pd(pd)->pdn;
    srq_info.cqn = cqn;
    srq_info.xrcd = 0;
    ret = hns_roce_srq_alloc(hr_dev, srq_info, &srq->mtt, 0, srq);
    if (ret) {
        dev_err(hr_dev->dev, "failed to alloc srq, cqn - 0x%x, pdn - 0x%lx\n", cqn, to_hr_pd(pd)->pdn);
        return ret;
    }

    srq->event = hns_roce_ib_srq_event;
    srq->ibsrq.ext.xrc.srq_num = srq->srqn;

    return 0;
}

int hns_roce_create_srq(struct ib_srq *ib_srq, struct ib_srq_init_attr *srq_init_attr,
                        struct ib_udata *udata)
{
    struct hns_roce_dev *hr_dev = NULL;
    struct hns_roce_srq *srq = NULL;
    int srq_buf_size, ret;

    if (hns_roce_create_srq_avail_para(ib_srq, srq_init_attr, udata) == false) {
        pr_err("hns3: create srq fail, invalid para\n");
        return -EINVAL;
    }

    hr_dev = to_hr_dev(ib_srq->device);
    srq = to_hr_srq(ib_srq);

    hns_roce_set_srq(srq, srq_init_attr, &srq_buf_size);

    ret = hns_roce_create_srq_queue(srq, srq_buf_size, udata);
    if (ret) {
        dev_err(hr_dev->dev, "hns_roce_create_srq_queue failed, ret:%d\n", ret);
        return ret;
    }

    ret = hns_roce_create_srq_ctx(ib_srq->pd, srq_init_attr, srq);
    if (ret) {
        dev_err(hr_dev->dev, "hns_roce_create_srq_ctx failed, ret:%d\n", ret);
        goto err_srq_ctx;
    }

    if (udata) {
        if (ib_copy_to_udata(udata, &srq->srqn, sizeof(__u32))) {
            dev_err(hr_dev->dev, "ib copy to udata failed\n");
            ret = -EFAULT;
            goto err_srqc_alloc;
        }
    }

    return 0;

err_srqc_alloc:
    hns_roce_srq_free(hr_dev, srq);

err_srq_ctx:
    if (udata) {
        destroy_user_srq(hr_dev, srq);
    } else {
        destroy_kernel_srq(hr_dev, srq, srq_buf_size);
    }

    return ret;
}

int hns_roce_destroy_srq(struct ib_srq *ibsrq, struct ib_udata *udata)
{
    struct hns_roce_dev *hr_dev = NULL;
    struct hns_roce_srq *srq = NULL;

    if (ibsrq == NULL) {
        pr_err("hns3: destroy srq param err, ibsrq is NULL\n");
        return -EINVAL;
    }

    hr_dev = to_hr_dev(ibsrq->device);
    srq = to_hr_srq(ibsrq);

    hns_roce_srq_free(hr_dev, srq);
    hns_roce_mtt_cleanup(hr_dev, &srq->mtt);

    if (ibsrq->uobject) {
        hns_roce_mtt_cleanup(hr_dev, &srq->idx_que.mtt);
        ib_umem_release(srq->idx_que.umem);
        ib_umem_release(srq->umem);
    } else {
        kvfree(srq->wrid);
        srq->wrid = NULL;
        hns_roce_buf_free(hr_dev, srq->max << (unsigned int)srq->wqe_shift, &srq->buf);
    }

    return 0;
}

int hns_roce_init_srq_table(struct hns_roce_dev *hr_dev)
{
    struct hns_roce_srq_table *srq_table = &hr_dev->srq_table;

    spin_lock_init(&srq_table->lock);
    INIT_RADIX_TREE(&srq_table->tree, GFP_ATOMIC);

    return hns_roce_bitmap_init(&srq_table->bitmap, hr_dev->caps.num_srqs, hr_dev->caps.num_srqs - 1,
                                hr_dev->caps.reserved_srqs, 0);
}

void hns_roce_cleanup_srq_table(struct hns_roce_dev *hr_dev)
{
    hns_roce_bitmap_cleanup(&hr_dev->srq_table.bitmap);
}

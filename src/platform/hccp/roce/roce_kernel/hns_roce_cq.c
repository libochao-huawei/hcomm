/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <linux/platform_device.h>
#include <rdma/ib_umem.h>
#include <rdma/uverbs_ioctl.h>
#include "hns_roce_device.h"
#include "hns_roce_cmd.h"
#include "hns_roce_hem.h"
#include "hns_roce_common.h"
#include "roce_k_compat.h"
#include "hns_roce_cq.h"

static void hns_roce_ib_cq_comp(struct hns_roce_cq *hr_cq)
{
    struct ib_cq *ibcq = &hr_cq->ib_cq;

    ibcq->comp_handler(ibcq, ibcq->cq_context);
}

static void hns_roce_ib_cq_event(struct hns_roce_cq *hr_cq,
                                 enum hns_roce_event event_type)
{
    struct hns_roce_dev *hr_dev = NULL;
    struct ib_event event;
    struct ib_cq *ibcq = NULL;

    ibcq = &hr_cq->ib_cq;
    hr_dev = to_hr_dev(ibcq->device);

    if (event_type != HNS_ROCE_EVENT_TYPE_CQ_ID_INVALID &&
            event_type != HNS_ROCE_EVENT_TYPE_CQ_ACCESS_ERROR &&
            event_type != HNS_ROCE_EVENT_TYPE_CQ_OVERFLOW) {
        dev_err(hr_dev->dev,
                "hns_roce_ib: Unexpected event type 0x%x on CQ %06lx\n",
                event_type, hr_cq->cqn);
        return;
    }

    if (ibcq->event_handler) {
        event.device = ibcq->device;
        event.event = IB_EVENT_CQ_ERR;
        event.element.cq = ibcq;
        ibcq->event_handler(&event, ibcq->cq_context);
    }
}

STATIC int hns_roce_sw2hw_cq(struct hns_roce_dev *dev,
                             struct hns_roce_cmd_mailbox *mailbox,
                             unsigned long cq_num)
{
    struct hns_roce_mbox_post_params mbox_pst_params = {0};

    mbox_pst_params.in_param = mailbox->dma;
    mbox_pst_params.out_param = 0;
    mbox_pst_params.in_modifier = cq_num;
    mbox_pst_params.op_modifier = 0;
    mbox_pst_params.op = HNS_ROCE_CMD_SW2HW_CQ;
    return hns_roce_cmd_mbox(dev, mbox_pst_params, HNS_ROCE_CMD_TIMEOUT_MSECS);
}

int hns_roce_alloc_cq(struct hns_roce_dev *hr_dev, struct hns_roce_cq *hr_cq, struct hns_roce_cq_table *cq_table,
    int vector)
{
    int ret;
    struct device *dev = hr_dev->dev;

    if (vector >= hr_dev->caps.num_comp_vectors) {
        dev_err(dev, "CQ alloc.Invalid vector[%d] >= num_comp_vectors [%d].\n", vector, hr_dev->caps.num_comp_vectors);
        return -EINVAL;
    }
    hr_cq->vector = vector;

    ret = hns_roce_bitmap_alloc(&cq_table->bitmap, &hr_cq->cqn);
    if (ret == -1) {
        dev_err(dev, "CQ alloc.Failed to alloc index.\n");
        return -ENOMEM;
    }

    /* Get CQC memory HEM(Hardware Entry Memory) table */
    ret = hns_roce_table_get(hr_dev, &cq_table->table, hr_cq->cqn);
    if (ret) {
        dev_err(dev, "CQ(0x%lx) alloc.Failed to get context mem(%d).\n", hr_cq->cqn, ret);
        hns_roce_bitmap_free(&cq_table->bitmap, hr_cq->cqn, BITMAP_NO_RR);
        return ret;
    }

    /* The cq insert radix tree */
    spin_lock_irq(&cq_table->lock);
    /* Radix_tree: The associated pointer and long integer key value like */
    ret = radix_tree_insert(&cq_table->tree, hr_cq->cqn, hr_cq);
    spin_unlock_irq(&cq_table->lock);
    if (ret) {
        dev_err(dev, "CQ(0x%lx) alloc.Failed to radix_tree_insert.\n", hr_cq->cqn);
        hns_roce_table_put(hr_dev, &cq_table->table, hr_cq->cqn);
        hns_roce_bitmap_free(&cq_table->bitmap, hr_cq->cqn, BITMAP_NO_RR);
        return ret;
    }

    return ret;
}

void hns_roce_set_alloc_init_flag(struct hns_roce_cq *hr_cq, struct hns_roce_uar *hr_uar)
{
    hr_cq->cons_index = 0;
    hr_cq->arm_sn = 1;
    hr_cq->uar = hr_uar;

    atomic_set(&hr_cq->refcount, 1);
    init_completion(&hr_cq->free);
}

void hns_roce_get_cqbuf_physic_addr(struct hns_roce_dev *hr_dev, struct hns_roce_hem_table **mtt_table)
{
    if (hns_roce_check_whether_mhop(hr_dev, HEM_TYPE_CQE)) {
        *mtt_table = &hr_dev->mr_table.mtt_cqe_table;
    } else {
        *mtt_table = &hr_dev->mr_table.mtt_table;
    }
}

void hns_roce_free_radix_tree(struct hns_roce_cq *hr_cq, struct hns_roce_cq_table *cq_table)
{
    spin_lock_irq(&cq_table->lock);
    radix_tree_delete(&cq_table->tree, hr_cq->cqn);
    spin_unlock_irq(&cq_table->lock);
}

static int hns_roce_cq_alloc(struct hns_roce_dev *hr_dev, int nent, struct hns_roce_mtt *hr_mtt,
    struct hns_roce_uar *hr_uar, struct hns_roce_cq *hr_cq, int vector)
{
    struct hns_roce_cmd_mailbox *mailbox = NULL;
    struct hns_roce_hem_table *mtt_table = NULL;
    struct hns_roce_cq_table *cq_table = NULL;
    struct hns_roce_wrtcqc_info wrt_cqc_info = {0};
    struct device *dev = hr_dev->dev;
    dma_addr_t dma_handle;
    u64 *mtts = NULL;
    int ret;

    cq_table = &hr_dev->cq_table;

    /* Get the physical address of cq buf */
    hns_roce_get_cqbuf_physic_addr(hr_dev, &mtt_table);

    mtts = hns_roce_table_find(hr_dev, mtt_table, hr_mtt->first_seg, &dma_handle);
    if (mtts == NULL) {
        dev_err(dev, "CQ alloc.Failed to find cq buf addr.\n");
        return -EINVAL;
    }

    ret = hns_roce_alloc_cq(hr_dev, hr_cq, cq_table, vector);
    if (ret) {
        dev_err(dev, "CQ alloc failed, ret %d\n", ret);
        return ret;
    }

    /* Allocate mailbox memory */
    mailbox = hns_roce_alloc_cmd_mailbox(hr_dev);
    if (IS_ERR(mailbox)) {
        dev_err(hr_dev->dev, "%s(%d) hns_roce malloc mailbox failed\n", __func__, __LINE__);
        ret = PTR_ERR(mailbox);
        goto err_radix;
    }

    HNS_ROCE_WRITE_CQC(hr_dev);

    /* Send mailbox to hw */
    ret = hns_roce_sw2hw_cq(hr_dev, mailbox, hr_cq->cqn);
    hns_roce_free_cmd_mailbox(hr_dev, mailbox);
    if (ret) {
        dev_err(dev, "CQ(0x%lx) alloc.Failed to cmd mailbox(%d).\n", hr_cq->cqn, ret);
        goto err_radix;
    }

    hns_roce_set_alloc_init_flag(hr_cq, hr_uar);

    return 0;

err_radix:
    hns_roce_free_radix_tree(hr_cq, cq_table);
    hns_roce_table_put(hr_dev, &cq_table->table, hr_cq->cqn);
    hns_roce_bitmap_free(&cq_table->bitmap, hr_cq->cqn, BITMAP_NO_RR);
    return ret;
}

STATIC int hns_roce_hw2sw_cq(struct hns_roce_dev *dev,
                             struct hns_roce_cmd_mailbox *mailbox,
                             unsigned long cq_num)
{
    struct hns_roce_mbox_post_params mbox_pst_params = {0};

    mbox_pst_params.in_param = 0;
    mbox_pst_params.out_param = mailbox ? mailbox->dma : 0;
    mbox_pst_params.in_modifier = cq_num;
    mbox_pst_params.op_modifier = mailbox ? 0 : 1;
    mbox_pst_params.op = HNS_ROCE_CMD_HW2SW_CQ;
    return hns_roce_cmd_mbox(dev, mbox_pst_params, HNS_ROCE_CMD_TIMEOUT_MSECS);
}

STATIC void hns_roce_free_cq(struct hns_roce_dev *hr_dev, struct hns_roce_cq *hr_cq)
{
    struct hns_roce_cq_table *cq_table = &hr_dev->cq_table;
    struct device *dev = hr_dev->dev;
    int ret;

    ret = hns_roce_hw2sw_cq(hr_dev, NULL, hr_cq->cqn);
    if (ret) {
        dev_err(dev, "HW2SW_CQ failed (%d) for CQN %06lx\n", ret,
                hr_cq->cqn);
    }

    /* Waiting interrupt process procedure carried out */
    synchronize_irq(hr_dev->eq_table.eq[hr_cq->vector].irq);

    /* wait for all interrupt processed */
    if (atomic_dec_and_test(&hr_cq->refcount)) {
        complete(&hr_cq->free);
    }
    wait_for_completion(&hr_cq->free);

    spin_lock_irq(&cq_table->lock);
    radix_tree_delete(&cq_table->tree, hr_cq->cqn);
    spin_unlock_irq(&cq_table->lock);

    hns_roce_table_put(hr_dev, &cq_table->table, hr_cq->cqn);
    hns_roce_bitmap_free(&cq_table->bitmap, hr_cq->cqn, BITMAP_NO_RR);
}

static int hns_roce_ib_get_cq_umem(struct hns_roce_dev *hr_dev,
                                   struct hns_roce_cq_buf *buf,
                                   struct ib_umem **umem, u64 buf_addr, int cqe)
{
    int ret;
    u32 page_shift;
    u32 npages;

    *umem = ib_umem_get(&hr_dev->ib_dev, buf_addr, cqe * hr_dev->caps.cq_entry_sz,
                        IB_ACCESS_LOCAL_WRITE);
    if (IS_ERR(*umem)) {
        dev_err(hr_dev->dev, "ib_umem_get operation failed\n");
        return PTR_ERR(*umem);
    }

    if (hns_roce_check_whether_mhop(hr_dev, HEM_TYPE_CQE)) {
        buf->hr_mtt.mtt_type = MTT_TYPE_CQE;
    } else {
        buf->hr_mtt.mtt_type = MTT_TYPE_WQE;
    }

    if (hr_dev->caps.cqe_buf_pg_sz) {
        npages = (ib_umem_num_pages(*umem) +
                  (1U << hr_dev->caps.cqe_buf_pg_sz) - 1) /
                 (1U << hr_dev->caps.cqe_buf_pg_sz);
        page_shift = PAGE_SHIFT + hr_dev->caps.cqe_buf_pg_sz;
        ret = hns_roce_mtt_init(hr_dev, npages, page_shift,
                                &buf->hr_mtt);
    } else {
        ret = hns_roce_mtt_init(hr_dev, ib_umem_num_pages(*umem),
                                PAGE_SHIFT,
                                &buf->hr_mtt);
    }
    if (ret) {
        dev_err(hr_dev->dev, "hns_roce_mtt_init error for create cq\n");
        goto err_buf;
    }

    ret = hns_roce_ib_umem_write_mtt(hr_dev, &buf->hr_mtt, *umem);
    if (ret) {
        dev_err(hr_dev->dev, "hns_roce_ib_umem_write_mtt error for create cq\n");
        goto err_mtt;
    }

    return 0;

err_mtt:
    hns_roce_mtt_cleanup(hr_dev, &buf->hr_mtt);

err_buf:
    ib_umem_release(*umem);
    return ret;
}

static int hns_roce_ib_alloc_cq_buf(struct hns_roce_dev *hr_dev,
                                    struct hns_roce_cq_buf *buf, u32 nent)
{
    int ret;
    u32 page_shift = PAGE_SHIFT + hr_dev->caps.cqe_buf_pg_sz;

    ret = hns_roce_buf_alloc(hr_dev, nent * hr_dev->caps.cq_entry_sz,
                             (1U << page_shift) * HNS_ROCE_IB_NUM_TWO, &buf->hr_buf,
                             page_shift);
    if (ret) {
        dev_err(hr_dev->dev, "hns_roce alloc buff failed, ret[%d]\n", ret);
        return ret;
    }

    if (hns_roce_check_whether_mhop(hr_dev, HEM_TYPE_CQE)) {
        buf->hr_mtt.mtt_type = MTT_TYPE_CQE;
    } else {
        buf->hr_mtt.mtt_type = MTT_TYPE_WQE;
    }

    ret = hns_roce_mtt_init(hr_dev, buf->hr_buf.npages,
                            buf->hr_buf.page_shift, &buf->hr_mtt);
    if (ret) {
        dev_err(hr_dev->dev, "hns_roce_mtt_init error for kernel create cq\n");
        goto err_buf;
    }

    ret = hns_roce_buf_write_mtt(hr_dev, &buf->hr_mtt, &buf->hr_buf);
    if (ret) {
        dev_err(hr_dev->dev, "hns_roce alloc buff write mtt failrd, ret[%d]\n", ret);
        goto err_mtt;
    }

    return 0;

err_mtt:
    hns_roce_mtt_cleanup(hr_dev, &buf->hr_mtt);

err_buf:
    hns_roce_buf_free(hr_dev, nent * hr_dev->caps.cq_entry_sz,
                      &buf->hr_buf);
    return ret;
}

STATIC void hns_roce_ib_free_cq_buf(struct hns_roce_dev *hr_dev,
                                    struct hns_roce_cq_buf *buf, int cqe)
{
    hns_roce_buf_free(hr_dev, (cqe + 1) * hr_dev->caps.cq_entry_sz,
                      &buf->hr_buf);
}

static int create_user_cq(struct hns_roce_dev *hr_dev,
                          struct hns_roce_cq *hr_cq,
                          struct hns_roce_uar *uar,
                          int cq_entries, struct hns_roce_user_cq *ucq)
{
    struct hns_roce_ib_create_cq ucmd = {0};
    struct device *dev = hr_dev->dev;
    struct hns_roce_ucontext *uctx;
    int ret;

    if (ucq->udata == NULL) {
        pr_err("hns3: create user cq fail, null pointer\n");
        return -EINVAL;
    }

    if (ucq->udata->inlen >= sizeof(ucmd)) {
        if (ib_copy_from_udata(&ucmd, ucq->udata, sizeof(ucmd))) {
            dev_err(dev, "Failed to copy_from_udata.\n");
            return -EFAULT;
        }
    }

    /* Get user space address, write it into mtt table */
    ret = hns_roce_ib_get_cq_umem(hr_dev, &hr_cq->hr_buf,
                                  &hr_cq->umem, ucmd.buf_addr,
                                  cq_entries);
    if (ret) {
        dev_err(dev, "Failed to get_cq_umem.\n");
        return ret;
    }

    if ((hr_dev->caps.flags & HNS_ROCE_CAP_FLAG_RECORD_DB) &&
            (ucq->udata->outlen >= sizeof(ucq->resp))) {
        uctx = rdma_udata_to_drv_context(ucq->udata, struct hns_roce_ucontext, ibucontext);
        ret = hns_roce_db_map_user(uctx, ucmd.db_addr, &hr_cq->db);
        if (ret) {
            dev_err(dev, "cq record doorbell map failed!\n");
            goto err_mtt;
        }
        hr_cq->db_en = 1;
        ucq->resp.cap_flags |= HNS_ROCE_SUPPORT_CQ_RECORD_DB;
    }

    /* Get user space parameters */
    uar = &to_hr_ucontext(ucq->context)->uar;

    return 0;

err_mtt:
    hns_roce_mtt_cleanup(hr_dev, &hr_cq->hr_buf.hr_mtt);
    ib_umem_release(hr_cq->umem);

    return ret;
}

static int create_kernel_cq(struct hns_roce_dev *hr_dev, struct hns_roce_cq *hr_cq, struct hns_roce_uar *uar,
    int cq_entries)
{
    struct device *dev = hr_dev->dev;
    int ret;

    hr_cq->workq = create_singlethread_workqueue("hns_roce_cq_workqueue");
    if (hr_cq->workq == NULL) {
        dev_err(dev, "Failed to create cq workqueue!\n");
        return -ENOMEM;
    }

    if (hr_dev->caps.flags & HNS_ROCE_CAP_FLAG_RECORD_DB) {
#ifndef DEFINE_HNS_LLT
        ret = hns_roce_alloc_db(hr_dev, &hr_cq->db, 1);
        if (ret) {
            dev_err(dev, "Failed to alloc db for cq.\n");
            goto err_workq;
        }

        hr_cq->set_ci_db = hr_cq->db.db_record;
        *hr_cq->set_ci_db = 0;
        hr_cq->db_en = 1;
#endif
    }

    /* Init mmt table and write buff address to mtt table */
    ret = hns_roce_ib_alloc_cq_buf(hr_dev, &hr_cq->hr_buf, cq_entries);
    if (ret) {
        dev_err(dev, "Failed to alloc cq buf.\n");
        goto err_db;
    }

    uar = &hr_dev->priv_uar;
    hr_cq->cq_db_l = hr_dev->reg_base + hr_dev->odb_offset + DB_REG_OFFSET * uar->index;
    return 0;

err_db:
    if (hr_dev->caps.flags & HNS_ROCE_CAP_FLAG_RECORD_DB) {
#ifndef DEFINE_HNS_LLT
        hns_roce_free_db(hr_dev, &hr_cq->db);
#endif
    }

err_workq:
    destroy_workqueue(hr_cq->workq);
    return ret;
}

static void destroy_user_cq(struct hns_roce_dev *hr_dev,
                            struct hns_roce_cq *hr_cq,
                            struct ib_ucontext *context,
                            struct ib_udata *udata,
                            struct hns_roce_ib_create_cq_resp *resp)
{
    if ((hr_dev->caps.flags & HNS_ROCE_CAP_FLAG_RECORD_DB) &&
        (udata != NULL) && (udata->outlen >= sizeof(*resp)))
        hns_roce_db_unmap_user(to_hr_ucontext(context), &hr_cq->db);

    hns_roce_mtt_cleanup(hr_dev, &hr_cq->hr_buf.hr_mtt);
    ib_umem_release(hr_cq->umem);
}

STATIC void destroy_kernel_cq(struct hns_roce_dev *hr_dev,
                              struct hns_roce_cq *hr_cq)
{
    hns_roce_mtt_cleanup(hr_dev, &hr_cq->hr_buf.hr_mtt);
    hns_roce_ib_free_cq_buf(hr_dev, &hr_cq->hr_buf, hr_cq->ib_cq.cqe);

    if (hr_dev->caps.flags & HNS_ROCE_CAP_FLAG_RECORD_DB) {
        hns_roce_free_db(hr_dev, &hr_cq->db);
    }
    destroy_workqueue(hr_cq->workq);
}

static bool hns_roce_ib_avail_para(struct ib_cq *ib_cq,
    const struct ib_cq_init_attr *attr)
{
    struct hns_roce_dev *hr_dev = NULL;
    struct device *dev = NULL;
    int cq_entries;

    if (ib_cq == NULL || ib_cq->device == NULL || attr == NULL) {
        pr_err("hns3: null point, ib_cq %pK, attr %pK\n", ib_cq, attr);
        return false;
    }

    hr_dev = to_hr_dev(ib_cq->device);
    dev = hr_dev->dev;
    cq_entries = attr->cqe;

    if (cq_entries < 1 || cq_entries > hr_dev->caps.max_cqes) {
        dev_err(dev, "Create CQ failed. entries=%d, max=%d\n",
                cq_entries, hr_dev->caps.max_cqes);
        return false;
    }
    return true;
}

int hns_roce_alloc_hr_cq(struct hns_roce_dev *hr_dev, struct hns_roce_cq **hr_cq, int *cq_entries)
{
    *hr_cq = kzalloc(sizeof(**hr_cq), GFP_KERNEL);
    if ((*hr_cq) == NULL) {
        dev_err(hr_dev->dev, "alloc hns_roce cq failed\n");
        return -ENOMEM;
    }

    if (hr_dev->caps.min_cqes) {
        *cq_entries = max(*cq_entries, hr_dev->caps.min_cqes);
    }

    *cq_entries = (int)roundup_pow_of_two((unsigned int)(*cq_entries));
    (*hr_cq)->ib_cq.cqe = *cq_entries - 1;
    spin_lock_init(&(*hr_cq)->lock);
    INIT_LIST_HEAD(&(*hr_cq)->list_sq);
    INIT_LIST_HEAD(&(*hr_cq)->list_rq);

    return 0;
}

void hns_roce_destory_created_cq(struct hns_roce_dev *hr_dev, struct hns_roce_cq *hr_cq,
    struct hns_roce_ib_create_cq_resp *resp, struct ib_ucontext *context, struct ib_udata *udata)
{
    if (context != NULL) {
        destroy_user_cq(hr_dev, hr_cq, context, udata, resp);
    } else {
        destroy_kernel_cq(hr_dev, hr_cq);
    }
}

int hns_roce_get_cq_handler_and_event(struct hns_roce_dev *hr_dev, struct hns_roce_cq **hr_cq,
    struct hns_roce_ib_create_cq_resp *resp, struct ib_ucontext *context, struct ib_udata *udata)
{
    int ret = 0;

    (*hr_cq)->comp = hns_roce_ib_cq_comp;
    (*hr_cq)->event = hns_roce_ib_cq_event;

    if (context != NULL && udata != NULL) {
        resp->cqn = (*hr_cq)->cqn;
        ret = ib_copy_to_udata(udata, resp, sizeof(*resp));
        if (ret) {
            dev_err(hr_dev->dev, "ib_copy_to_udata failed, ret[%d]\n", ret);
            return ret;
        }
    }

    return ret;
}

void hns_roce_free_hr_cq(struct hns_roce_cq **hr_cq)
{
    kfree(*hr_cq);
    *hr_cq = NULL;
}

int hns_roce_ib_create_cq(struct ib_cq *ib_cq, const struct ib_cq_init_attr *attr,
    struct ib_udata *udata)
{
    struct hns_roce_ucontext *uctx = NULL;
    struct ib_ucontext *context = NULL;
    struct hns_roce_dev *hr_dev = NULL;
    struct hns_roce_user_cq ucq = {0};
    struct hns_roce_cq *hr_cq = NULL;
    struct hns_roce_uar *uar = NULL;
    int vector, cq_entries, ret;
    struct device *dev = NULL;

    if (hns_roce_ib_avail_para(ib_cq, attr) == false) {
        pr_err("hns3: ib get dev fail, invalid parameter\n");
        return -EINVAL;
    }
    HNS_ROCE_LOC_VAR_INIT(hr_dev, (ib_cq->device), attr);

    hr_cq = (struct hns_roce_cq *)ib_cq;
    if (hr_dev->caps.min_cqes) {
        cq_entries = max(cq_entries, hr_dev->caps.min_cqes);
    }
    cq_entries = (int)roundup_pow_of_two((unsigned int)(cq_entries));
    ib_cq->cqe = cq_entries - 1;
    spin_lock_init(&hr_cq->lock);
    INIT_LIST_HEAD(&hr_cq->list_sq);
    INIT_LIST_HEAD(&hr_cq->list_rq);

    if (udata != NULL) {
        uctx = rdma_udata_to_drv_context(udata, struct hns_roce_ucontext, ibucontext);
        context = &(uctx->ibucontext);
        ucq.context = context;
        ucq.udata = udata;
        ret = create_user_cq(hr_dev, hr_cq, uar, cq_entries, &ucq);
    } else {
        ret = create_kernel_cq(hr_dev, hr_cq, uar, cq_entries);
    }
    if (ret) {
        dev_err(dev, "Failed to create cq, (udata != NULL) is %u, ret : %d\n", (udata != NULL), ret);
        return ret;
    }

    /* Allocate cq index, fill cq_context */
    ret = hns_roce_cq_alloc(hr_dev, cq_entries, &hr_cq->hr_buf.hr_mtt, uar, hr_cq, vector);
    if (ret) {
        dev_err(dev, "Create CQ .Failed to cq_alloc.\n");
        goto err_dbmap;
    }

    /*
     * For the QP created by kernel space, tptr value should be initialized
     * to zero; For the QP created by user space, it will cause synchronous
     * problems if tptr is set to zero here, so we initialze it in user
     * space.
     */
    HNS_ROCE_INIT_HR_CQ_TPTR();

    /* Get created cq handler and carry out event */
    ret = hns_roce_get_cq_handler_and_event(hr_dev, &hr_cq, &ucq.resp, context, udata);
    if (ret) {
        dev_err(dev, "Create CQ handler and event failed.\n");
        goto err_handler_and_event;
    }

    hns_roce_inc_rdma_hw_stats(&hr_dev->ib_dev, HW_STATS_CQ_ALLOC);

    return 0;

err_handler_and_event:
    hns_roce_free_cq(hr_dev, hr_cq);

err_dbmap:
    hns_roce_destory_created_cq(hr_dev, hr_cq, &ucq.resp, context, udata);

    return ret;
}

int hns_roce_ib_destroy_cq(struct ib_cq *ib_cq, struct ib_udata *udata)
{
    struct hns_roce_ucontext *uctx = NULL;
    struct hns_roce_dev *hr_dev = NULL;
    struct hns_roce_cq *hr_cq = NULL;
    int ret = 0;

    if (ib_cq == NULL) {
        pr_err("hns3: ib destroy cq param err, ib_cq is NULL\n");
        return -EINVAL;
    }
    hr_dev = to_hr_dev(ib_cq->device);
    hr_cq = to_hr_cq(ib_cq);

    hns_roce_inc_rdma_hw_stats(ib_cq->device, HW_STATS_CQ_DEALLOC);

    if (hr_dev->hw->destroy_cq) {
        ret = hr_dev->hw->destroy_cq(ib_cq);
    } else {
        hns_roce_free_cq(hr_dev, hr_cq);
        hns_roce_mtt_cleanup(hr_dev, &hr_cq->hr_buf.hr_mtt);

        if (udata) {
            uctx = rdma_udata_to_drv_context(udata, struct hns_roce_ucontext, ibucontext);
            ib_umem_release(hr_cq->umem);

            if (hr_cq->db_en == 1)
                hns_roce_db_unmap_user(uctx, &hr_cq->db);
        } else {
            /* Free the buff of stored cq */
            hns_roce_ib_free_cq_buf(hr_dev, &hr_cq->hr_buf,
                                    ib_cq->cqe);
            if (hr_dev->caps.flags & HNS_ROCE_CAP_FLAG_RECORD_DB) {
                hns_roce_free_db(hr_dev, &hr_cq->db);
            }
            flush_workqueue(hr_cq->workq);
            destroy_workqueue(hr_cq->workq);
        }
    }

    return ret;
}

void hns_roce_cq_completion(struct hns_roce_dev *hr_dev, u32 cqn)
{
    struct device *dev = hr_dev->dev;
    struct hns_roce_cq *cq = NULL;

    rcu_read_lock();
    cq = radix_tree_lookup(&hr_dev->cq_table.tree,
                           cqn & ((unsigned int)hr_dev->caps.num_cqs - 1));
    rcu_read_unlock();
    if (cq == NULL) {
        dev_warn(dev, "Completion event for bogus CQ 0x%08x\n", cqn);
        return;
    }

    ++cq->arm_sn;
    cq->comp(cq);
}

void hns_roce_cq_event(struct hns_roce_dev *hr_dev, u32 cqn, int event_type)
{
    struct hns_roce_cq_table *cq_table = &hr_dev->cq_table;
    struct device *dev = hr_dev->dev;
    struct hns_roce_cq *cq = NULL;

    rcu_read_lock();
    cq = radix_tree_lookup(&cq_table->tree,
                           cqn & (unsigned int)(hr_dev->caps.num_cqs - 1));
    rcu_read_unlock();
    if (cq != NULL) {
        atomic_inc(&cq->refcount);
    }

    if (cq == NULL) {
        dev_warn(dev, "Async event for bogus CQ %08x\n", cqn);
        return;
    }

    cq->event(cq, (enum hns_roce_event)event_type);

    if (atomic_dec_and_test(&cq->refcount)) {
        complete(&cq->free);
    }
}

int hns_roce_init_cq_table(struct hns_roce_dev *hr_dev)
{
    struct hns_roce_cq_table *cq_table = &hr_dev->cq_table;

    spin_lock_init(&cq_table->lock);
    INIT_RADIX_TREE(&cq_table->tree, GFP_ATOMIC);

    return hns_roce_bitmap_init(&cq_table->bitmap, hr_dev->caps.num_cqs,
                                hr_dev->caps.num_cqs - 1,
                                hr_dev->caps.reserved_cqs, 0);
}

void hns_roce_cleanup_cq_table(struct hns_roce_dev *hr_dev)
{
    hns_roce_bitmap_cleanup(&hr_dev->cq_table.bitmap);
}

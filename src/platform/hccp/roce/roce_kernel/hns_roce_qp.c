/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <linux/pci.h>
#include <linux/platform_device.h>
#include <linux/version.h>
#include <rdma/ib_addr.h>
#include <rdma/ib_umem.h>
#include <rdma/uverbs_ioctl.h>
#include "hclge_main.h"
#include "hns-abi.h"
#include "hns_roce_common.h"
#include "hns_roce_device.h"
#include "hns_roce_hem.h"
#include "hns_roce_sm.h"
#include "hns_roce_qp.h"
#include "hns_roce_ah.h"
#ifdef DEFINE_HNS_LLT
#undef static
#define static
#endif

#define SQP_NUM             (2 * HNS_ROCE_MAX_PORTS)
#define PAGE_CNT            2

static void flush_work_handle(struct work_struct *work)
{
    struct hns_roce_flush_work *flush_work = container_of(work, struct hns_roce_flush_work, work);
    struct hns_roce_qp *hr_qp = flush_work->hr_qp;
    struct device *dev = flush_work->hr_dev->dev;
    struct ib_qp_attr attr = {0};
    int attr_mask;
    int ret;

    attr_mask = IB_QP_STATE;
    attr.qp_state = IB_QPS_ERR;

    ret = hns_roce_modify_qp(&hr_qp->ibqp, &attr, attr_mask, NULL);
    if (ret) {
        dev_err(dev, "Modify qp to err for flush cqe fail(%d)\n", ret);
    }

    kfree(flush_work);
    flush_work = NULL;
#ifndef DEFINE_HNS_LLT
    if (atomic_dec_and_test(&hr_qp->refcount)) {
        complete(&hr_qp->free);
    }
#endif
}

void init_flush_work(struct hns_roce_dev *hr_dev, struct hns_roce_qp *hr_qp)
{
    struct hns_roce_flush_work *flush_work = NULL;
    flush_work = kzalloc(sizeof(struct hns_roce_flush_work), GFP_ATOMIC);
    if (flush_work == NULL) {
        dev_err(hr_dev->dev, "init flush work queue fail!\n");
        return;
    }
    flush_work->hr_dev = hr_dev;
    flush_work->hr_qp = hr_qp;
    INIT_WORK(&flush_work->work, flush_work_handle);
    atomic_inc(&hr_qp->refcount);
    queue_work(hr_dev->flush_workq, &flush_work->work);
}

void hns_roce_qp_event(struct hns_roce_dev *hr_dev, u32 qpn, int event_type)
{
    struct hns_roce_qp_table *qp_table = &hr_dev->qp_table;
    struct device *dev = hr_dev->dev;
    struct hns_roce_qp *qp = NULL;

    spin_lock(&qp_table->lock);

    qp = __hns_roce_qp_lookup(hr_dev, qpn);
    if (qp != NULL) {
        atomic_inc(&qp->refcount);
    }

    spin_unlock(&qp_table->lock);

    if (qp == NULL) {
        dev_warn(dev, "Async event for bogus QP %08x\n", qpn);
        return;
    }

#ifndef DEFINE_HNS_LLT
    if (event_type == HNS_ROCE_EVENT_TYPE_WQ_CATAS_ERROR ||
            event_type == HNS_ROCE_EVENT_TYPE_INV_REQ_LOCAL_WQ_ERROR ||
            event_type == HNS_ROCE_EVENT_TYPE_LOCAL_WQ_ACCESS_ERROR) {
        qp->state = IB_QPS_ERR;
        init_flush_work(hr_dev, qp);
    }
#endif

    qp->event(qp, (enum hns_roce_event)event_type);

    if (atomic_dec_and_test(&qp->refcount)) {
        complete(&qp->free);
    }
}

void hns_roce_qp_remove(struct hns_roce_dev *hr_dev, struct hns_roce_qp *hr_qp)
{
    struct hns_roce_qp_table *qp_table = &hr_dev->qp_table;
    unsigned long flags;

    spin_lock_irqsave(&qp_table->lock, flags);
    radix_tree_delete(&hr_dev->qp_table_tree,
                      hr_qp->qpn & ((unsigned int)hr_dev->caps.num_qps - 1));
    spin_unlock_irqrestore(&qp_table->lock, flags);
}

void hns_roce_qp_free(struct hns_roce_dev *hr_dev, struct hns_roce_qp *hr_qp)
{
    struct hns_roce_qp_table *qp_table = &hr_dev->qp_table;

    if (atomic_dec_and_test(&hr_qp->refcount)) {
        complete(&hr_qp->free);
    }
    wait_for_completion(&hr_qp->free);

    if ((hr_qp->ibqp.qp_type) != IB_QPT_GSI) {
        if (hr_dev->caps.trrl_entry_sz)
            hns_roce_table_put(hr_dev, &qp_table->trrl_table,
                               hr_qp->qpn);
        hns_roce_table_put(hr_dev, &qp_table->irrl_table, hr_qp->qpn);
        hns_roce_table_put(hr_dev, &qp_table->qp_table, hr_qp->qpn);
    }
}

void hns_roce_release_range_qp(struct hns_roce_dev *hr_dev, int base_qpn,
    int cnt)
{
    struct hns_roce_qp_table *qp_table = &hr_dev->qp_table;

    if (base_qpn < hr_dev->caps.reserved_qps) {
        return;
    }

    hns_roce_bitmap_free_range(&qp_table->bitmap, base_qpn, cnt, BITMAP_RR);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
int hns_roce_create_qp(struct ib_qp *qp, struct ib_qp_init_attr *init_attr,
		       struct ib_udata *udata)
{
    pr_err("hns3: hns_roce_create_qp is not supported\n");
    return -1;
}
#else
STATIC void hns_roce_qp_free_scc_ctx_trrl_irrl_qp_table(struct hns_roce_dev *hr_dev,
    struct hns_roce_qp_table *qp_table, struct hns_roce_qp *hr_qp)
{
    if (hr_dev->caps.scc_ctx_entry_sz) {
        hns_roce_table_put(hr_dev, &qp_table->scc_ctx_table, hr_qp->qpn);
    }

    if (hr_dev->caps.trrl_entry_sz) {
        hns_roce_table_put(hr_dev, &qp_table->trrl_table, hr_qp->qpn);
    }

    hns_roce_table_put(hr_dev, &qp_table->irrl_table, hr_qp->qpn);
    hns_roce_table_put(hr_dev, &qp_table->qp_table, hr_qp->qpn);
}

STATIC int hns_roce_qp_alloc_qpc_irrl_trrl_scc_ctx(struct hns_roce_dev *hr_dev, struct hns_roce_qp *hr_qp,
    struct hns_roce_qp_table *qp_table)
{
    int ret;
    struct device *dev = hr_dev->dev;
    /* Alloc memory for QPC */
    ret = hns_roce_table_get(hr_dev, &qp_table->qp_table, hr_qp->qpn);
    if (ret) {
        dev_err(dev, "qpn[%lu] QPC table get failed, retL%d\n", hr_qp->qpn, ret);
        goto err_out;
    }

    /* Alloc memory for IRRL */
    ret = hns_roce_table_get(hr_dev, &qp_table->irrl_table, hr_qp->qpn);
    if (ret) {
        dev_err(dev, "qpn(%lu) IRRL table get failed, ret:%d\n", hr_qp->qpn, ret);
        goto err_put_qp;
    }

    if (hr_dev->caps.trrl_entry_sz) {
        /* Alloc memory for TRRL */
        ret = hns_roce_table_get(hr_dev, &qp_table->trrl_table, hr_qp->qpn);
        if (ret) {
            dev_err(dev, "qp(%lu) TRRL table get failed, ret:%d\n", hr_qp->qpn, ret);
            goto err_put_irrl;
        }
    }

    if (hr_dev->caps.scc_ctx_entry_sz) {
        /* Alloc memory for SCC CTX */
        ret = hns_roce_table_get(hr_dev, &qp_table->scc_ctx_table, hr_qp->qpn);
        if (ret) {
            dev_err(dev, "qpn(%lu) SCC CTX table get failed, ret:%d\n", hr_qp->qpn, ret);
            goto err_put_trrl;
        }
    }

    return 0;

err_put_trrl:
    if (hr_dev->caps.trrl_entry_sz) {
        hns_roce_table_put(hr_dev, &qp_table->trrl_table, hr_qp->qpn);
    }

err_put_irrl:
    hns_roce_table_put(hr_dev, &qp_table->irrl_table, hr_qp->qpn);
err_put_qp:
    hns_roce_table_put(hr_dev, &qp_table->qp_table, hr_qp->qpn);
err_out:
    return ret;
}

STATIC void hns_roce_free_qp_gdr(struct hns_roce_dev *hr_dev, u32 gdr_enable, u32 index)
{
    struct hns_roce_sm_priv *priv = NULL;
    priv = (struct hns_roce_sm_priv *)hr_dev->share_mem_priv;

    if (priv == NULL) {
        dev_err(hr_dev->dev, "current hns_roce dev get share_mem_priv failed\n");
        return;
    }

    if (gdr_enable == HNS_ROCE_GDR_QP_MODE) {
        hns_roce_bitmap_free(&priv->sq_bitmap, index, BITMAP_NO_RR);
    }
    return;
}

STATIC int hns_roce_qp_alloc(struct hns_roce_dev *hr_dev, unsigned long qpn,
    struct hns_roce_qp *hr_qp)
{
    struct hns_roce_qp_table *qp_table = &hr_dev->qp_table;
    struct device *dev = hr_dev->dev;
    int ret;

    if (!qpn) {
        dev_err(hr_dev->dev, "invalid parameter qpn[%lu]\n", qpn);
        return -EINVAL;
    }

    hr_qp->qpn = qpn;

    ret = hns_roce_qp_alloc_qpc_irrl_trrl_scc_ctx(hr_dev, hr_qp, qp_table);
    if (ret) {
        dev_err(hr_dev->dev, "alloc for qpn %lu qpc irrl failed\n", qpn);
        return ret;
    }

    spin_lock_irq(&qp_table->lock);
    ret = radix_tree_insert(&hr_dev->qp_table_tree, hr_qp->qpn & ((unsigned int)hr_dev->caps.num_qps - 1), hr_qp);
    spin_unlock_irq(&qp_table->lock);
    if (ret) {
        dev_err(dev, "QPC radix_tree_insert failed\n");
        goto out;
    }

    atomic_set(&hr_qp->refcount, 1);
    init_completion(&hr_qp->free);

    return 0;

out:
    hns_roce_qp_free_scc_ctx_trrl_irrl_qp_table(hr_dev, qp_table, hr_qp);
    return ret;
}

static int hns_roce_gsi_qp_alloc(struct hns_roce_dev *hr_dev, unsigned long qpn, struct hns_roce_qp *hr_qp)
{
    struct hns_roce_qp_table *qp_table = &hr_dev->qp_table;
    int ret;

    if (!qpn) {
        dev_err(hr_dev->dev, "invalid parameter qpn[%lu]\n", qpn);
        return -EINVAL;
    }

    hr_qp->qpn = qpn;

    spin_lock_irq(&qp_table->lock);
    ret = radix_tree_insert(&hr_dev->qp_table_tree,
                            hr_qp->qpn & ((unsigned int)hr_dev->caps.num_qps - 1), hr_qp);
    spin_unlock_irq(&qp_table->lock);
    if (ret) {
        dev_err(hr_dev->dev, "QPC radix_tree_insert failed\n");
        return ret;
    }

    atomic_set(&hr_qp->refcount, 1);
    init_completion(&hr_qp->free);

    return 0;
}

STATIC int hns_roce_reserve_range_qp(struct hns_roce_dev *hr_dev, int cnt,
    int align, unsigned long *base)
{
    struct hns_roce_qp_table *qp_table = &hr_dev->qp_table;

    return hns_roce_bitmap_alloc_range(&qp_table->bitmap, cnt, align, base);
}

STATIC void hns_roce_ib_qp_event(struct hns_roce_qp *hr_qp,
    enum hns_roce_event type)
{
    struct ib_event event;
    struct ib_qp *ibqp = &hr_qp->ibqp;

    if (ibqp->event_handler) {
        event.device = ibqp->device;
        event.element.qp = ibqp;
        switch (type) {
            case HNS_ROCE_EVENT_TYPE_PATH_MIG:
                event.event = IB_EVENT_PATH_MIG;
                break;
            case HNS_ROCE_EVENT_TYPE_COMM_EST:
                event.event = IB_EVENT_COMM_EST;
                break;
            case HNS_ROCE_EVENT_TYPE_SQ_DRAINED:
                event.event = IB_EVENT_SQ_DRAINED;
                break;
            case HNS_ROCE_EVENT_TYPE_SRQ_LAST_WQE_REACH:
                event.event = IB_EVENT_QP_LAST_WQE_REACHED;
                break;
            case HNS_ROCE_EVENT_TYPE_WQ_CATAS_ERROR:
                event.event = IB_EVENT_QP_FATAL;
                break;
            case HNS_ROCE_EVENT_TYPE_PATH_MIG_FAILED:
                event.event = IB_EVENT_PATH_MIG_ERR;
                break;
            case HNS_ROCE_EVENT_TYPE_INV_REQ_LOCAL_WQ_ERROR:
                event.event = IB_EVENT_QP_REQ_ERR;
                break;
            case HNS_ROCE_EVENT_TYPE_LOCAL_WQ_ACCESS_ERROR:
                event.event = IB_EVENT_QP_ACCESS_ERR;
                break;
            default:
                dev_dbg(ibqp->device->dev.parent, "roce_ib: Unexpected event type %d on QP %06lx\n",
                        type, hr_qp->qpn);
                return;
        }
        ibqp->event_handler(&event, ibqp->qp_context);
    }
}

static int check_sq_size_with_integrity(struct hns_roce_dev *hr_dev,
                                        struct ib_qp_cap *cap,
                                        struct hns_roce_ib_create_qp *ucmd)
{
    unsigned long roundup_sq_stride = roundup_pow_of_two(hr_dev->caps.max_sq_desc_sz);
    u8 max_sq_stride = ilog2(roundup_sq_stride);
    /* Sanity check SQ size before proceeding */
    if ((u32)(1 << ucmd->log_sq_bb_count) > hr_dev->caps.max_wqes ||
            ucmd->log_sq_stride > max_sq_stride ||
            ucmd->log_sq_stride < HNS_ROCE_IB_MIN_SQ_STRIDE) {
        dev_err(hr_dev->dev,
                "check SQ size error!Log sq stride 0x%x\n",
                ucmd->log_sq_stride);
        return -EINVAL;
    }

    if ((ucmd->gdr_enable == HNS_ROCE_GDR_QP_MODE) &&
            (u32)(1 << ucmd->log_sq_bb_count) > hr_dev->sq_depth) {
        dev_err(hr_dev->dev, "check gdr SQ size[%u] error! log_sq_bb_count[%u]\n",
            hr_dev->sq_depth, ucmd->log_sq_bb_count);
        return -EINVAL;
    }

    if (ucmd->gdr_enable == HNS_ROCE_GDR_QP_MODE) {
        mutex_lock(&hr_dev->sm_mutex);
        if (hr_dev->qp_cnt >= hr_dev->max_sq_num) {
            dev_err(hr_dev->dev, "hr_dev->qp_cnt:%u exceeded the maximum QP limit(%u)",
                hr_dev->qp_cnt, hr_dev->max_sq_num);
            mutex_unlock(&hr_dev->sm_mutex);
            return -EINVAL;
        }
        mutex_unlock(&hr_dev->sm_mutex);
    }

    if (cap->max_send_sge > hr_dev->caps.max_sq_sg) {
        dev_err(hr_dev->dev, "SQ sge error!Max send sge %d\n",
                cap->max_send_sge);
        return -EINVAL;
    }

    return 0;
}

static int hns_roce_sge_cnt_get(struct hns_roce_dev *hr_dev, struct hns_roce_qp *hr_qp)
{
    if (hr_qp->sq.max_gs > HNS_ROCE_SGE_IN_WQE) {
        hr_qp->sge.sge_cnt = (int)roundup_pow_of_two(hr_qp->sq.wqe_cnt * (hr_qp->sq.max_gs - HNS_ROCE_SGE_IN_WQE));
    }

    if (hr_qp->ibqp.qp_type == IB_QPT_UD) {
        hr_qp->sge.sge_cnt = (int)roundup_pow_of_two(hr_qp->sq.wqe_cnt * hr_qp->sq.max_gs);
    }

    if ((hr_qp->sq.max_gs > HNS_ROCE_SGE_IN_WQE) && (hr_dev->pci_dev->revision == PCI_REVISION_ID_HIP08_A)) {
        if ((u32)hr_qp->sge.sge_cnt > hr_dev->caps.max_extend_sg) {
            dev_err(hr_dev->dev, "SQ(0x%lx) extended sge cnt error! sge_cnt=%d\n", hr_qp->qpn, hr_qp->sge.sge_cnt);
            return -EINVAL;
        }
    }

    return 0;
}

static void hns_roce_set_user_sq_size_para_init(struct hns_roce_dev *hr_dev, struct ib_qp_cap *cap,
    struct hns_roce_qp *hr_qp, struct hns_roce_ib_create_qp *ucmd)
{
    u32 max_cnt;
    hr_qp->sq.wqe_cnt = 1 << ucmd->log_sq_bb_count;
    hr_qp->sq.wqe_shift = ucmd->log_sq_stride;

    max_cnt = max(1U, cap->max_send_sge);
    hr_qp->sq.max_gs = (int)(hr_dev->caps.max_sq_sg <= HNS_ROCE_MAX_SGE_NUM ? roundup_pow_of_two(max_cnt) : max_cnt);
    return;
}

static int hns_roce_set_user_sq_size(struct hns_roce_dev *hr_dev, struct ib_qp_cap *cap,
    struct hns_roce_qp *hr_qp, struct hns_roce_ib_create_qp *ucmd)
{
    u32 ex_sge_num, page_size;
    int ret;

    ret = check_sq_size_with_integrity(hr_dev, cap, ucmd);
    if (ret) {
        dev_err(hr_dev->dev, "Sanity check sq(0x%lx) size fail\n", hr_qp->qpn);
        return ret;
    }

    hns_roce_set_user_sq_size_para_init(hr_dev, cap, hr_qp, ucmd);

    ret = hns_roce_sge_cnt_get(hr_dev, hr_qp);
    if (ret) {
        dev_err(hr_dev->dev, "SQ(0x%lx) get sge cnt fail\n", hr_qp->qpn);
        return ret;
    }

    hr_qp->sge.sge_shift = HNS_ROCE_SGE_SHIFT;
    ex_sge_num = hr_qp->sge.sge_cnt;

    /* Get buf size, SQ and RQ  are aligned to page_szie */
    if (hr_dev->caps.max_sq_sg <= HNS_ROCE_MAX_SGE_NUM) {
        hr_qp->buff_size = HNS_ROCE_ALOGN_UP(((unsigned int)hr_qp->rq.wqe_cnt <<
            (unsigned int)hr_qp->rq.wqe_shift), PAGE_SIZE) +
            HNS_ROCE_ALOGN_UP(((unsigned int)hr_qp->sq.wqe_cnt << (unsigned int)hr_qp->sq.wqe_shift), PAGE_SIZE);
        hr_qp->sq.offset = 0;
        hr_qp->rq.offset = HNS_ROCE_ALOGN_UP(((unsigned int)hr_qp->sq.wqe_cnt <<
            (unsigned int)hr_qp->sq.wqe_shift), PAGE_SIZE);
    } else {
        page_size = 1 << (hr_dev->caps.mtt_buf_pg_sz + PAGE_SHIFT);
        hr_qp->sge.sge_cnt = ex_sge_num ? max(page_size / (1 << (unsigned int)hr_qp->sge.sge_shift), ex_sge_num) : 0;
        hr_qp->buff_size = HNS_ROCE_ALOGN_UP(((unsigned int)hr_qp->rq.wqe_cnt <<
            (unsigned int)hr_qp->rq.wqe_shift), page_size) +
            HNS_ROCE_ALOGN_UP(((unsigned int)hr_qp->sge.sge_cnt << (unsigned int)hr_qp->sge.sge_shift), page_size) +
            HNS_ROCE_ALOGN_UP(((unsigned int)hr_qp->sq.wqe_cnt << (unsigned int)hr_qp->sq.wqe_shift), page_size);
        hr_qp->sq.offset = 0;

        if (ex_sge_num) {
            hr_qp->sge.offset = HNS_ROCE_ALOGN_UP(((unsigned int)hr_qp->sq.wqe_cnt <<
                (unsigned int)hr_qp->sq.wqe_shift), page_size);
            hr_qp->rq.offset = hr_qp->sge.offset +
                HNS_ROCE_ALOGN_UP(((unsigned int)hr_qp->sge.sge_cnt << (unsigned int)hr_qp->sge.sge_shift), page_size);
        } else {
            hr_qp->rq.offset = HNS_ROCE_ALOGN_UP(((unsigned int)hr_qp->sq.wqe_cnt <<
                (unsigned int)hr_qp->sq.wqe_shift), page_size);
        }
    }

    if (ucmd->gdr_enable == HNS_ROCE_GDR_QP_MODE) {
        hr_qp->buff_size -= HNS_ROCE_ALOGN_UP(((unsigned int)hr_qp->sq.wqe_cnt << (unsigned int)hr_qp->sq.wqe_shift),
            PAGE_SIZE);
    }

    return 0;
}

static int set_extend_sge_param(struct hns_roce_dev *hr_dev,
    struct hns_roce_qp *hr_qp)
{
    struct device *dev = hr_dev->dev;

    if (hr_qp->sq.max_gs > HNS_ROCE_SGE_IN_WQE) {
        hr_qp->sge.sge_cnt = (int)roundup_pow_of_two(hr_qp->sq.wqe_cnt *
                                                (hr_qp->sq.max_gs - HNS_ROCE_SGE_IN_WQE));
        hr_qp->sge.sge_shift = HNS_ROCE_SGE_SHIFT;
    }

    /* ud sqwqe's sge use extend sge */
    if (hr_dev->caps.max_sq_sg > HNS_ROCE_SGE_IN_WQE &&
            hr_qp->ibqp.qp_type == IB_QPT_GSI) {
        hr_qp->sge.sge_cnt = (int)roundup_pow_of_two(hr_qp->sq.wqe_cnt *
                                                hr_qp->sq.max_gs);
        hr_qp->sge.sge_shift = HNS_ROCE_SGE_SHIFT;
    }

    if ((hr_qp->sq.max_gs > HNS_ROCE_SGE_IN_WQE) &&
            hr_dev->pci_dev->revision == PCI_REVISION_ID_HIP08_A) {
        if ((u32)hr_qp->sge.sge_cnt > hr_dev->caps.max_extend_sg) {
            dev_err(dev, "The extended sge cnt error! sge_cnt=%d\n",
                    hr_qp->sge.sge_cnt);
            return -EINVAL;
        }
    }

    return 0;
}

static int hns_roce_set_kernel_sq_size(struct hns_roce_dev *hr_dev,
                                       struct ib_qp_cap *cap, struct hns_roce_qp *hr_qp)
{
    struct device *dev = hr_dev->dev;
    u32 page_size;
    u32 max_cnt;
    int size;
    int ret;

    if (cap->max_send_wr  > hr_dev->caps.max_wqes  || cap->max_send_sge > hr_dev->caps.max_sq_sg ||
            cap->max_inline_data > hr_dev->caps.max_sq_inline) {
        dev_err(dev, "SQ WR or sge or inline data error!\n");
        return -EINVAL;
    }

    hr_qp->sq.wqe_shift = ilog2(hr_dev->caps.max_sq_desc_sz);
    max_cnt = hr_dev->caps.min_wqes ? max(cap->max_send_wr, hr_dev->caps.min_wqes) : cap->max_send_wr;
    hr_qp->sq.wqe_cnt = (int)roundup_pow_of_two(max_cnt);
    if ((u32)hr_qp->sq.wqe_cnt > hr_dev->caps.max_wqes) {
        dev_err(dev, "while setting kernel sq size, sq.wqe_cnt too large\n");
        return -EINVAL;
    }

    /* Get data_seg numbers */
    max_cnt = max(1U, cap->max_send_sge);
    if (hr_dev->caps.max_sq_sg <= HNS_ROCE_MAX_SGE_NUM) {
        hr_qp->sq.max_gs = (int)roundup_pow_of_two(max_cnt);
    } else {
        hr_qp->sq.max_gs = max_cnt;
    }

    ret = set_extend_sge_param(hr_dev, hr_qp);
    if (ret) {
        dev_err(dev, "set extend sge parameters fail\n");
        return ret;
    }

    /* Get buf size, SQ and RQ are aligned to PAGE_SIZE */
    page_size = 1 << (hr_dev->caps.mtt_buf_pg_sz + PAGE_SHIFT);
    hr_qp->sq.offset = 0;
    size = HNS_ROCE_ALOGN_UP((unsigned int)hr_qp->sq.wqe_cnt << (unsigned int)hr_qp->sq.wqe_shift, page_size);
    if (hr_dev->caps.max_sq_sg > HNS_ROCE_MAX_SGE_NUM && hr_qp->sge.sge_cnt) {
        hr_qp->sge.sge_cnt = max(page_size / (1 << (unsigned int)hr_qp->sge.sge_shift), (u32)hr_qp->sge.sge_cnt);
        hr_qp->sge.offset = size;
        size += HNS_ROCE_ALOGN_UP((unsigned int)hr_qp->sge.sge_cnt << (unsigned int)hr_qp->sge.sge_shift, page_size);
    }

    hr_qp->rq.offset = size;
    size += HNS_ROCE_ALOGN_UP(((unsigned int)hr_qp->rq.wqe_cnt << (unsigned int)hr_qp->rq.wqe_shift), page_size);
    hr_qp->buff_size = size;

    /* Get wr and sge number which send */
    cap->max_send_wr = hr_qp->sq.max_post = hr_qp->sq.wqe_cnt;
    cap->max_send_sge = hr_qp->sq.max_gs;

    /* We don't support inline sends for kernel QPs (yet) */
    cap->max_inline_data = 0;

    return 0;
}

static int hns_roce_qp_has_sq(struct ib_qp_init_attr *attr)
{
    if (attr->qp_type == IB_QPT_XRC_TGT || !attr->cap.max_send_wr) {
        return 0;
    }

    return 1;
}

static int hns_roce_qp_has_rq(struct ib_qp_init_attr *attr)
{
    if (attr->qp_type == IB_QPT_XRC_INI ||
            attr->qp_type == IB_QPT_XRC_TGT || attr->srq ||
            !attr->cap.max_recv_wr) {
        return 0;
    }

    return 1;
}

STATIC int hns_roce_create_qp_gdr(struct hns_roce_dev *hr_dev, struct ib_pd *ib_pd,
                                  struct hns_roce_qp *hr_qp, struct hns_roce_ib_create_qp *ucmd)
{
    int ret;
    struct device *dev = hr_dev->dev;
    if (ucmd->gdr_enable != HNS_ROCE_GDR_QP_MODE) {
        return 0;
    }

    hr_qp->gdr_enable = HNS_ROCE_GDR_QP_MODE;

    // alloc share sq buffer
    ret = hns_roce_alloc_shared_sq(hr_dev, &hr_qp->share_sq);
    if (ret) {
        dev_err(dev, "hns_roce_alloc_shared_sq fail! ret:%u\n", ret);
        return ret;
    }

    return 0;
}

STATIC void hns_roce_init_gdr_resp(struct hns_roce_dev *hr_dev,
    struct hns_roce_qp *hr_qp, struct hns_roce_ib_create_qp *ucmd,
    struct hns_roce_ib_create_qp_resp *resp)
{
    if (ucmd->gdr_enable != HNS_ROCE_GDR_QP_MODE) {
        return;
    }

    resp->share_sq_index = hr_qp->share_sq.index;
    resp->share_sq_offset = hr_qp->share_sq.sq_addr - hns_roce_get_shared_mem_base(hr_dev);
    resp->share_temp_offset = hr_qp->share_sq.temp_addr - hns_roce_get_shared_mem_base(hr_dev);
    resp->share_db_offset = hr_qp->share_sq.db_addr - hns_roce_get_shared_mem_base(hr_dev);
    resp->share_dfx_offset = hr_qp->share_sq.dfx_addr - hns_roce_get_shared_mem_base(hr_dev);
    resp->share_sq_depth = hr_dev->sq_depth;
    resp->share_temp_depth = hr_dev->temp_depth;
    resp->share_db_depth = HNS_ROCE_SHARED_DB_DEPTH;
    resp->share_dfx_depth = HNS_ROCE_SHARED_DFX_DEPTH;
    resp->share_max_sq_num = hr_qp->share_sq.max_sq_num;
    /*
    * show buffer info
    */
    dev_notice(hr_dev->dev, "resp index:%u sq_offset:0x%x temp_offset:0x%x db_offset:0x%x \
share_gdrdfx_offset:0x%x sq_depth:%u temp_depth:%u db_depth:%u share_gdrdfx_depth:%u, share_max_sq_num:%u\n", \
               resp->share_sq_index, resp->share_sq_offset, resp->share_temp_offset, \
               resp->share_db_offset, resp->share_dfx_offset, resp->share_sq_depth, \
               resp->share_temp_depth, resp->share_db_depth, resp->share_dfx_depth, resp->share_max_sq_num);
}

static void hns_roce_add_qp_list(struct hns_roce_dev *hr_dev,
                                 struct ib_qp_init_attr *init_attr,
                                 struct hns_roce_qp *hr_qp)
{
    struct hns_roce_cq *send_cq = NULL;
    struct hns_roce_cq *recv_cq = NULL;
    unsigned long flags;

    if (hr_dev->hw_rev != HNS_ROCE_HW_VER1) {
        send_cq = init_attr->send_cq ?
                  to_hr_cq(init_attr->send_cq) : NULL;
        recv_cq = init_attr->recv_cq ?
                  to_hr_cq(init_attr->recv_cq) : NULL;
        spin_lock_irqsave(&hr_dev->reset_lock, flags);
        if ((recv_cq == NULL) || (send_cq == NULL)) {
            spin_unlock_irqrestore(&hr_dev->reset_lock, flags);
            return;
        }
        hns_roce_lock_cqs(send_cq, recv_cq);
        list_add_tail(&hr_qp->qps_list, &hr_dev->qp_list);
        if (send_cq != NULL) {
            list_add_tail(&hr_qp->scq_list, &send_cq->list_sq);
        }
        if (recv_cq != NULL) {
            list_add_tail(&hr_qp->rcq_list, &recv_cq->list_rq);
        }
        hns_roce_unlock_cqs(send_cq, recv_cq);
        spin_unlock_irqrestore(&hr_dev->reset_lock, flags);
    }
}

static int hns_roce_init_mtt_table(struct hns_roce_dev *hr_dev,
    struct ib_pd *ib_pd, struct hns_roce_qp *hr_qp, struct hns_roce_ib_create_qp ucmd)
{
    int ret;
    u32 page_shift;
    u32 page_count = 0;
    u32 npages;
    struct device *dev = hr_dev->dev;

    hr_qp->umem = ib_umem_get(&hr_dev->ib_dev,
                              ucmd.buf_addr, hr_qp->buff_size, 0);
    if (IS_ERR(hr_qp->umem)) {
        ret = PTR_ERR(hr_qp->umem);
        dev_err(dev, "ib_umem_get error for create qp ret[%d]\n", ret);
        return -EINVAL;
    }

    hr_qp->mtt.mtt_type = MTT_TYPE_WQE;

    if (ucmd.gdr_enable == HNS_ROCE_GDR_QP_MODE) {
        page_count = (unsigned int)HNS_ROCE_ALOGN_UP(((unsigned int)hr_qp->sq.wqe_cnt <<
            (unsigned int)hr_qp->sq.wqe_shift), PAGE_SIZE) >> (unsigned int)PAGE_SHIFT;
    }

    if (hr_dev->caps.mtt_buf_pg_sz) {
        page_count += ib_umem_num_pages(hr_qp->umem);
        npages = (page_count +
                  (1 << hr_dev->caps.mtt_buf_pg_sz) - 1) /
                 (1 << hr_dev->caps.mtt_buf_pg_sz);
        page_shift = PAGE_SHIFT + hr_dev->caps.mtt_buf_pg_sz;
        ret = hns_roce_mtt_init(hr_dev, npages,
                                page_shift,
                                &hr_qp->mtt);
    } else {
        page_count += ib_umem_num_pages(hr_qp->umem);
        ret = hns_roce_mtt_init(hr_dev,
                                page_count,
                                PAGE_SHIFT,
                                &hr_qp->mtt);
    }
    if (ret) {
        ib_umem_release(hr_qp->umem);
        dev_err(dev, "hns_roce_mtt_init error for create qp\n");
        return ret;
    }
    return 0;
}

static int hns_roce_init_write_mtt(struct hns_roce_dev *hr_dev,
    struct hns_roce_qp *hr_qp, struct hns_roce_ib_create_qp ucmd)
{
    int ret;
    struct device *dev = hr_dev->dev;
    if (ucmd.gdr_enable == HNS_ROCE_GDR_QP_MODE) {
        hr_qp->share_sq.sq_buf_len = HNS_ROCE_ALOGN_UP(((unsigned int)hr_qp->sq.wqe_cnt <<
                                                        (unsigned int)hr_qp->sq.wqe_shift), PAGE_SIZE);

        ret = hns_roce_gdr_write_mtt(hr_dev, &hr_qp->mtt,
                                     hr_qp->umem, &hr_qp->share_sq);
        if (ret) {
            dev_err(dev, "hns_roce_gdr_write_mtt error for create qp\n");
            return ret;
        }
    } else {
        ret = hns_roce_ib_umem_write_mtt(hr_dev, &hr_qp->mtt,
                                         hr_qp->umem);
        if (ret) {
            dev_err(dev, "hns_roce_ib_umem_write_mtt error for create qp\n");
            return ret;
        }
    }
    return 0;
}

static void hns_roce_init_qp_free(struct hns_roce_dev *hr_dev,
    struct ib_qp_init_attr *init_attr, struct hns_roce_qp *hr_qp)
{
    if (init_attr->qp_type == IB_QPT_GSI &&
            hr_dev->hw_rev == HNS_ROCE_HW_VER1) {
        hns_roce_qp_remove(hr_dev, hr_qp);
    } else {
        hns_roce_qp_free(hr_dev, hr_qp);
    }
}

STATIC int hns_roce_init_get_qpn(struct hns_roce_dev *hr_dev,
    unsigned long sqpn, unsigned long *qpn)
{
    int ret;
    struct device *dev = hr_dev->dev;

    if (sqpn) {
        *qpn = sqpn;
    } else {
        /* Get QPN */
        ret = hns_roce_reserve_range_qp(hr_dev, 1, 1, qpn);
        if (ret) {
            dev_err(dev, "hns_roce_reserve_range_qp alloc qpn error\n");
            return ret;
        }
    }
    return 0;
}

STATIC int hns_roce_init_qp_alloc(struct hns_roce_dev *hr_dev,
    struct ib_qp_init_attr *init_attr, unsigned long qpn,
    unsigned long sqpn, struct hns_roce_qp *hr_qp)
{
    int ret;
    struct device *dev = hr_dev->dev;

    if (init_attr->qp_type == IB_QPT_GSI &&
            hr_dev->hw_rev == HNS_ROCE_HW_VER1) {
#ifndef DEFINE_HNS_LLT
        /* In v1 engine, GSI QP context in RoCE engine's register */
        ret = hns_roce_gsi_qp_alloc(hr_dev, qpn, hr_qp);
        if (ret) {
            dev_err(dev, "hns_roce_qp_alloc failed!\n");
            return ret;
        }
#endif
    } else {
        ret = hns_roce_qp_alloc(hr_dev, qpn, hr_qp);
        if (ret) {
            dev_err(dev, "hns_roce_qp_alloc failed!\n");
            return ret;
        }
    }

    if (sqpn) {
        hr_qp->doorbell_qpn = 1;
    } else {
        hr_qp->doorbell_qpn = cpu_to_le64(hr_qp->qpn);
    }
    return 0;
}

STATIC int hns_roce_init_db_map_user(struct hns_roce_dev *hr_dev,
    struct ib_pd *ib_pd, struct ib_qp_init_attr *init_attr,
    struct hns_roce_ib_info *hns_ib_ptr, struct hns_roce_qp *hr_qp)
{
    int ret = 0;
#ifndef DEFINE_HNS_LLT
    bool sq_record_db;
    bool rq_record_db = false;
    struct device *dev = hr_dev->dev;
    struct hns_roce_ucontext *uctx = rdma_udata_to_drv_context(hns_ib_ptr->udata,
        struct hns_roce_ucontext, ibucontext);

    sq_record_db = (hr_dev->caps.flags & HNS_ROCE_CAP_FLAG_SQ_RECORD_DB) &&
                (hns_ib_ptr->udata->inlen >= sizeof(hns_ib_ptr->ucmd)) &&
                (hns_ib_ptr->udata->outlen >= sizeof(*(hns_ib_ptr->resp))) && hns_roce_qp_has_sq(init_attr);
    if (sq_record_db) {
        ret = hns_roce_db_map_user(uctx, (hns_ib_ptr->ucmd).sdb_addr, &hr_qp->sdb);
        if (ret) {
            dev_err(dev, "sq record doorbell map failed!\n");
            return ret;
        }

        /* indicate kernel supports sq record db */
        hns_ib_ptr->resp->cap_flags |= HNS_ROCE_SUPPORT_SQ_RECORD_DB;
        hr_qp->sdb_en = 1;
    }

    rq_record_db = (hr_dev->caps.flags & HNS_ROCE_CAP_FLAG_RECORD_DB) &&
            (hns_ib_ptr->udata->outlen >= sizeof(*(hns_ib_ptr->resp))) &&
            hns_roce_qp_has_rq(init_attr);
    if (rq_record_db) {
        ret = hns_roce_db_map_user(uctx, (hns_ib_ptr->ucmd).db_addr, &hr_qp->rdb);
        if (ret) {
            dev_err(dev, "rq record doorbell map failed!\n");
            goto err_sq_dbmap;
        }
        /* indicate kernel supports rq record db */
        hns_ib_ptr->resp->cap_flags |= HNS_ROCE_SUPPORT_RQ_RECORD_DB;
        hr_qp->rdb_en = 1;
    }

    return 0;
err_sq_dbmap:
    if (sq_record_db)
        hns_roce_db_unmap_user(
            to_hr_ucontext(ib_pd->uobject->context), &hr_qp->sdb);
#endif
    return ret;
}

static void hns_roce_init_db_unmap_user(struct hns_roce_dev *hr_dev,
    struct ib_pd *ib_pd, struct ib_qp_init_attr *init_attr,
    struct ib_udata *udata, struct hns_roce_qp *hr_qp)
{
#ifndef DEFINE_HNS_LLT
    bool sq_record_db;
    bool rq_record_db;
    struct hns_roce_ib_create_qp ucmd;
    struct hns_roce_ib_create_qp_resp resp = {};
    sq_record_db = (hr_dev->caps.flags & HNS_ROCE_CAP_FLAG_SQ_RECORD_DB) &&
                (udata->inlen >= sizeof(ucmd)) &&
                (udata->outlen >= sizeof(resp)) && hns_roce_qp_has_sq(init_attr);

    rq_record_db = (hr_dev->caps.flags & HNS_ROCE_CAP_FLAG_RECORD_DB) &&
            (udata->outlen >= sizeof(resp)) &&
            hns_roce_qp_has_rq(init_attr);
    if (rq_record_db)
        hns_roce_db_unmap_user(
            to_hr_ucontext(ib_pd->uobject->context),
            &hr_qp->rdb);

    if (sq_record_db)
        hns_roce_db_unmap_user(
            to_hr_ucontext(ib_pd->uobject->context), &hr_qp->sdb);
#endif
}

static void hns_roce_init_free_db(struct hns_roce_dev *hr_dev,
    struct ib_qp_init_attr *init_attr, struct hns_roce_qp *hr_qp)
{
    bool record_db;
    record_db = hns_roce_qp_has_rq(init_attr) &&
            (hr_dev->caps.flags & HNS_ROCE_CAP_FLAG_RECORD_DB);
    if (record_db) {
        hns_roce_free_db(hr_dev, &hr_qp->rdb);
    }
}

STATIC int hns_roce_init_qp_with_uobj(struct hns_roce_dev *hr_dev,
    struct ib_pd *ib_pd, struct ib_qp_init_attr *init_attr,
    struct hns_roce_qp *hr_qp, struct hns_roce_ib_create_qp *ucmd)
{
    int ret;
    struct device *dev = hr_dev->dev;

    ret = hns_roce_set_user_sq_size(hr_dev, &init_attr->cap, hr_qp,
                                    ucmd);
    if (ret) {
        dev_err(dev, "hns_roce_set_user_sq_size error for create qp\n");
        return ret;
    }

    ret = hns_roce_init_mtt_table(hr_dev, ib_pd, hr_qp, *ucmd);
    if (ret) {
        dev_err(dev, "hns_roce_init_mtt_table error for create qp [%d]\n", ret);
        return ret;
    }

    ret = hns_roce_create_qp_gdr(hr_dev, ib_pd, hr_qp, ucmd);
    if (ret) {
        dev_err(dev, "hns_roce_create_qp_gdr error for create qp\n");
        goto qp_gdr_err;
    }

    ret = hns_roce_init_write_mtt(hr_dev, hr_qp, *ucmd);
    if (ret) {
        dev_err(dev, "hns_roce_init_write_mtt error for create qp [%d]\n", ret);
        goto err_mtt;
    }

    return 0;

err_mtt:
    hns_roce_free_qp_gdr(hr_dev, ucmd->gdr_enable, hr_qp->share_sq.index);
qp_gdr_err:
    hns_roce_mtt_cleanup(hr_dev, &hr_qp->mtt);
    return ret;
}

STATIC int hns_roce_init_qp_check_flags(struct hns_roce_dev *hr_dev,
    struct ib_qp_init_attr *init_attr)
{
    struct device *dev = hr_dev->dev;
    if ((unsigned int)init_attr->create_flags &
            IB_QP_CREATE_BLOCK_MULTICAST_LOOPBACK) {
        dev_err(dev, "init_attr->create_flags error(%d)!\n",
                init_attr->create_flags);
        return -EINVAL;
    }

    if ((unsigned int)init_attr->create_flags & IB_QP_CREATE_IPOIB_UD_LSO) {
        dev_err(dev, "init_attr->create_flags error(%d)!\n",
                init_attr->create_flags);
        return -EINVAL;
    }
    return 0;
}

static void hns_roce_init_qp_db_reg(struct hns_roce_dev *hr_dev,
    struct hns_roce_qp *hr_qp)
{
    hr_qp->sq.db_reg_l = hr_dev->reg_base + hr_dev->sdb_offset +
                         DB_REG_OFFSET * hr_dev->priv_uar.index;
    hr_qp->rq.db_reg_l = hr_dev->reg_base + hr_dev->odb_offset +
                         DB_REG_OFFSET * hr_dev->priv_uar.index;
}

STATIC int hns_roce_init_alloc_db(struct hns_roce_dev *hr_dev,
    struct ib_qp_init_attr *init_attr, struct hns_roce_qp *hr_qp)
{
    bool record_db;
    int ret;
    struct device *dev = hr_dev->dev;

    record_db = (hr_dev->caps.flags & HNS_ROCE_CAP_FLAG_RECORD_DB) &&
            hns_roce_qp_has_rq(init_attr);
    if (record_db) {
#ifndef DEFINE_HNS_LLT
        ret = hns_roce_alloc_db(hr_dev, &hr_qp->rdb, 0);
        if (ret) {
            dev_err(dev, "rq record doorbell alloc failed!\n");
            return ret;
        }
        *hr_qp->rdb.db_record = 0;
        hr_qp->rdb_en = 1;
#endif
    }
    return 0;
}

STATIC int hns_roce_init_qp_kernel(struct hns_roce_dev *hr_dev,
    struct ib_qp_init_attr *init_attr, struct hns_roce_qp *hr_qp)
{
    int ret;
    u32 page_shift;
    struct device *dev = hr_dev->dev;
    if (hns_roce_init_qp_check_flags(hr_dev, init_attr)) {
        dev_err(dev, "hns_roce_init_qp_check_flags check fail !\n");
        return -EINVAL;
    }

    /* Set SQ size */
    ret = hns_roce_set_kernel_sq_size(hr_dev, &init_attr->cap, hr_qp);
    if (ret) {
        dev_err(dev, "hns_roce_set_kernel_sq_size error!\n");
        return ret;
    }

    /* QP doorbell register address */
    hns_roce_init_qp_db_reg(hr_dev, hr_qp);

    ret = hns_roce_init_alloc_db(hr_dev, init_attr, hr_qp);
    if (ret) {
        dev_err(dev, "hns_roce_init_alloc_db alloc failed!\n");
        return ret;
    }

    /* Allocate QP buf */
    page_shift = PAGE_SHIFT + hr_dev->caps.mtt_buf_pg_sz;
    if (hns_roce_buf_alloc(hr_dev, hr_qp->buff_size, (1 << page_shift) * PAGE_CNT,
                           &hr_qp->hr_buf, page_shift)) {
        dev_err(dev, "hns_roce_buf_alloc error!\n");
        ret = -ENOMEM;
        goto err_db;
    }

    hr_qp->mtt.mtt_type = MTT_TYPE_WQE;
    /* Write MTT */
    ret = hns_roce_mtt_init(hr_dev, hr_qp->hr_buf.npages, hr_qp->hr_buf.page_shift, &hr_qp->mtt);
    if (ret) {
        dev_err(dev, "hns_roce_mtt_init error for kernel create qp\n");
        goto err_buf;
    }

    ret = hns_roce_buf_write_mtt(hr_dev, &hr_qp->mtt, &hr_qp->hr_buf);
    if (ret) {
        dev_err(dev, "hns_roce_buf_write_mtt error for kernel create qp\n");
        goto err_mtt;
    }
    return 0;
err_mtt:
    hns_roce_mtt_cleanup(hr_dev, &hr_qp->mtt);

err_buf:
    hns_roce_buf_free(hr_dev, hr_qp->buff_size, &hr_qp->hr_buf);

err_db:
    hns_roce_init_free_db(hr_dev, init_attr, hr_qp);
    return ret;
}

static void hns_roce_init_inl_sg_list(struct ib_qp_init_attr *init_attr,
    struct hns_roce_qp *hr_qp)
{
    u32 i;
    /* Secondly, reallocate the buffer */
    for (i = 1; i < hr_qp->rq_inl_buf.wqe_cnt; i++) {
        hr_qp->rq_inl_buf.wqe_list[i].sg_list =
            &hr_qp->rq_inl_buf.wqe_list[0].sg_list[i *
                                                   init_attr->cap.max_recv_sge];
    }
}

STATIC int hns_roce_init_qp_create(struct hns_roce_dev *hr_dev,
                                   unsigned long sqpn, struct ib_qp_init_attr *init_attr, struct hns_roce_qp *hr_qp)
{
    int ret;
    struct device *dev = hr_dev->dev;
    unsigned long qpn = 0;

    /* Get QPN */
    ret = hns_roce_init_get_qpn(hr_dev, sqpn, &qpn);
    if (ret) {
        dev_err(dev, "hns_roce_init_get_qpn alloc qpn error\n");
        return ret;
    }

    if (hr_qp->gdr_enable == HNS_ROCE_GDR_QP_MODE) {
        mutex_lock(&hr_dev->sm_mutex);
        hr_dev->qp_cnt++;
        mutex_unlock(&hr_dev->sm_mutex);
    }

    ret = hns_roce_init_qp_alloc(hr_dev, init_attr, qpn, sqpn, hr_qp);
    if (ret) {
        dev_err(dev, "hns_roce_qp_alloc failed!\n");
        goto err_qpn;
    }

    if (hr_dev->caps.flags & HNS_ROCE_CAP_FLAG_QP_FLOW_CTRL) {
        ret = hr_dev->hw->qp_flow_control_init(hr_dev, hr_qp);
        if (ret) {
            dev_err(hr_dev->dev, "qp flow control init failure!");
            goto err_qp;
        }
    }

    hr_qp->event = hns_roce_ib_qp_event;

    ret = hns_roce_mailbox_shared_sq(hr_dev, hr_qp->share_sq, hr_qp->gdr_enable);
    if (ret) {
        dev_err(hr_dev->dev, "hns_roce_mailbox_shared_sq fail! ret:%u\n", ret);
        goto err_qp;
    }

    hns_roce_add_qp_list(hr_dev, init_attr, hr_qp);
    return 0;
err_qp:
    hns_roce_init_qp_free(hr_dev, init_attr, hr_qp);

err_qpn:
    if (!sqpn) {
        hns_roce_release_range_qp(hr_dev, qpn, 1);
    }
    return ret;
}

STATIC void hns_roce_init_qp_release(struct hns_roce_dev *hr_dev,
    struct ib_qp_init_attr *init_attr, unsigned long sqpn,
    unsigned long qpn, struct hns_roce_qp *hr_qp)
{
    hns_roce_init_qp_free(hr_dev, init_attr, hr_qp);
    if (!sqpn) {
        hns_roce_release_range_qp(hr_dev, qpn, 1);
    }
}

static void hns_roce_init_free_rq_inl_wqe_list(struct hns_roce_dev *hr_dev,
    struct ib_qp_init_attr *init_attr, struct hns_roce_qp *hr_qp)
{
    if ((hr_dev->caps.flags & HNS_ROCE_CAP_FLAG_RQ_INLINE) &&
            hns_roce_qp_has_rq(init_attr)) {
        kfree(hr_qp->rq_inl_buf.wqe_list);
        hr_qp->rq_inl_buf.wqe_list = NULL;
    }
}

static void hns_roce_init_free_rq_inl_sg_list(struct hns_roce_dev *hr_dev,
    struct ib_qp_init_attr *init_attr, struct hns_roce_qp *hr_qp)
{
    if ((hr_dev->caps.flags & HNS_ROCE_CAP_FLAG_RQ_INLINE) &&
            hns_roce_qp_has_rq(init_attr)) {
        kfree(hr_qp->rq_inl_buf.wqe_list[0].sg_list);
        hr_qp->rq_inl_buf.wqe_list[0].sg_list = NULL;
    }
}

static void hns_roce_free_qp_wqe_sge_list(struct hns_roce_dev *hr_dev,
    struct ib_qp_init_attr *init_attr, struct hns_roce_qp *hr_qp)
{
    hns_roce_init_free_rq_inl_sg_list(hr_dev, init_attr, hr_qp);
    hns_roce_init_free_rq_inl_wqe_list(hr_dev, init_attr, hr_qp);
}

STATIC void hns_roce_free_qp_with_uobj(struct hns_roce_dev *hr_dev, struct hns_roce_qp *hr_qp,
    struct hns_roce_ib_create_qp ucmd)
{
    hns_roce_mtt_cleanup(hr_dev, &hr_qp->mtt);
    hns_roce_free_qp_gdr(hr_dev, ucmd.gdr_enable, hr_qp->share_sq.index);
    hns_roce_buf_free(hr_dev, hr_qp->buff_size, &hr_qp->hr_buf);
}

STATIC int hns_roce_create_user_qp(struct hns_roce_dev *hr_dev, struct ib_udata *udata,
    struct ib_pd *ib_pd, struct ib_qp_init_attr *init_attr, struct hns_roce_qp *hr_qp)
{
    int ret;
    int qpn = 0;
    struct device *dev = hr_dev->dev;
    struct hns_roce_ib_create_qp ucmd;
    struct hns_roce_ib_info hns_ib_info;
    struct hns_roce_ib_create_qp_resp resp = {0};
    unsigned long sqpn = hr_qp->ibqp.qp_num;

    if (udata == NULL) {
        pr_err("hns3: udata is NULL\n");
        return -EINVAL;
    }

    if (ib_copy_from_udata(&ucmd, udata, sizeof(ucmd))) {
        dev_err(dev, "ib_copy_from_udata error for create qp\n");
        return -EFAULT;
    }

    ret = hns_roce_init_qp_with_uobj(hr_dev, ib_pd, init_attr, hr_qp, &ucmd);
    if (ret) {
        dev_err(dev, "hns_roce_init_qp_with_uobj error for create qp\n");
        return ret;
    }

    hns_roce_init_gdr_resp(hr_dev, hr_qp, &ucmd, &resp);

    hns_ib_info.udata = udata;
    hns_ib_info.resp = &resp;
    hns_ib_info.ucmd = ucmd;
    ret = hns_roce_init_db_map_user(hr_dev, ib_pd, init_attr, &hns_ib_info, hr_qp);
    if (ret) {
        dev_err(dev, "hns_roce_init_db_map_user failed for create qp [%d]\n", ret);
        goto err_db;
    }

    ret = hns_roce_init_qp_create(hr_dev, sqpn, init_attr, hr_qp);
    if (ret) {
        dev_err(dev, "hns_roce_init_get_qpn alloc qpn error %d\n", ret);
        goto err_qp;
    }

    if (udata->outlen >= sizeof(resp)) {
        ret = ib_copy_to_udata(udata, &resp, sizeof(resp));
        if (ret) {
            dev_err(dev, "ib copy [%lu] bytes to udata failed\n", sizeof(resp));
            goto err_copy;
        }
    }

    return 0;

err_copy:
    hns_roce_init_qp_release(hr_dev, init_attr, sqpn, qpn, hr_qp);
err_qp:
    hns_roce_init_db_unmap_user(hr_dev, ib_pd, init_attr, udata, hr_qp);
err_db:
    hns_roce_free_qp_with_uobj(hr_dev, hr_qp, ucmd);
    return ret;
}

STATIC void hns_roce_uninit_qp_kernel(struct hns_roce_dev *hr_dev, struct hns_roce_qp *hr_qp,
    struct ib_qp_init_attr *init_attr)
{
    hns_roce_mtt_cleanup(hr_dev, &hr_qp->mtt);
    hns_roce_buf_free(hr_dev, hr_qp->buff_size, &hr_qp->hr_buf);
    hns_roce_init_free_db(hr_dev, init_attr, hr_qp);
}

STATIC int hns_roce_alloc_wrid(struct hns_roce_dev *hr_dev, struct hns_roce_qp *hr_qp)
{
    hr_qp->sq.wrid = kcalloc(hr_qp->sq.wqe_cnt, sizeof(u64), GFP_KERNEL);
    if (hr_qp->sq.wrid == NULL) {
        dev_err(hr_dev->dev, "kcalloc sq wrid failed\n");
        return -ENOMEM;
    }

    hr_qp->rq.wrid = kcalloc(hr_qp->rq.wqe_cnt, sizeof(u64), GFP_KERNEL);
    if (hr_qp->rq.wrid == NULL) {
        dev_err(hr_dev->dev, "kcalloc rq srid failed\n");
        kfree(hr_qp->sq.wrid);
        hr_qp->sq.wrid = NULL;
        return -ENOMEM;
    }

    return 0;
}

STATIC void hns_roce_free_wrid(struct hns_roce_qp *hr_qp)
{
    if (hr_qp->sq.wrid) {
        kfree(hr_qp->sq.wrid);
        hr_qp->sq.wrid = NULL;
    }
    
    if (hr_qp->rq.wrid) {
        kfree(hr_qp->rq.wrid);
        hr_qp->rq.wrid = NULL;
    }
}

static int hns_roce_create_kernel_qp(struct hns_roce_dev *hr_dev,
    struct ib_qp_init_attr *init_attr, struct hns_roce_qp *hr_qp)
{
    int ret;
    struct device *dev = hr_dev->dev;
    unsigned long sqpn = hr_qp->ibqp.qp_num;

    ret = hns_roce_init_qp_kernel(hr_dev, init_attr, hr_qp);
    if (ret) {
        dev_err(dev, "hns_roce_buf_write_mtt error for kernel create qp\n");
        return ret;
    }

    ret = hns_roce_alloc_wrid(hr_dev, hr_qp);
    if (ret) {
        dev_err(dev, "hns_roce_buf_write_mtt error for kernel create qp\n");
        hns_roce_uninit_qp_kernel(hr_dev, hr_qp, init_attr);
        return ret;
    }

    /* Get QPN */
    ret = hns_roce_init_qp_create(hr_dev, sqpn, init_attr, hr_qp);
    if (ret) {
        dev_err(dev, "hns_roce_init_get_qpn alloc qpn error %d\n", ret);
        hns_roce_free_wrid(hr_qp);
        hns_roce_uninit_qp_kernel(hr_dev, hr_qp, init_attr);
        return ret;
    }

    return 0;
}

static int hns_roce_set_rq_size(struct hns_roce_dev *hr_dev,
                                struct ib_qp_cap *cap, int is_user, int has_rq,
                                struct hns_roce_qp *hr_qp)
{
    struct device *dev = hr_dev->dev;
    u32 max_cnt;

    /* Check the validity of QP support capacity */
    if (cap->max_recv_wr > hr_dev->caps.max_wqes ||
            cap->max_recv_sge > hr_dev->caps.max_rq_sg) {
        dev_err(dev, "RQ(0x%lx) WR or sge error!max_recv_wr=%d max_recv_sge=%d\n",
                hr_qp->qpn, cap->max_recv_wr, cap->max_recv_sge);
        return -EINVAL;
    }

    /* If srq exist, set zero for relative number of rq */
    if (!has_rq) {
        hr_qp->rq.wqe_cnt = 0;
        hr_qp->rq.max_gs = 0;
        cap->max_recv_wr = 0;
        cap->max_recv_sge = 0;
    } else {
        if (is_user && (!cap->max_recv_wr || !cap->max_recv_sge)) {
            dev_err(dev, "user space max_recv_wr=%d or max_recv_sge=%d is 0\n",
                cap->max_recv_wr, cap->max_recv_sge);
            return -EINVAL;
        }

        max_cnt = max(cap->max_recv_wr, hr_dev->caps.min_wqes);

        hr_qp->rq.wqe_cnt = (int)roundup_pow_of_two(max_cnt);
        if ((u32)hr_qp->rq.wqe_cnt > hr_dev->caps.max_wqes) {
            dev_err(dev, "while setting rq(0x%lx) size, rq.wqe_cnt too large\n",
                    hr_qp->qpn);
            return -EINVAL;
        }

        max_cnt = max(1U, cap->max_recv_sge);
        hr_qp->rq.max_gs = (int)roundup_pow_of_two(max_cnt);
        if (hr_dev->caps.max_rq_sg <= HNS_ROCE_MAX_SGE_NUM) {
            hr_qp->rq.wqe_shift =
                ilog2(hr_dev->caps.max_rq_desc_sz);
        } else {
            hr_qp->rq.wqe_shift =
                ilog2(hr_dev->caps.max_rq_desc_sz
                      * hr_qp->rq.max_gs);
        }
    }

    hr_qp->rq.max_post = hr_qp->rq.wqe_cnt;
    cap->max_recv_wr = hr_qp->rq.wqe_cnt;
    cap->max_recv_sge = hr_qp->rq.max_gs;

    return 0;
}

static void hns_roce_init_signal_bits(struct hns_roce_qp *hr_qp,
    struct ib_qp_init_attr *init_attr)
{
    if (init_attr->sq_sig_type == IB_SIGNAL_ALL_WR) {
        hr_qp->sq_signal_bits = cpu_to_le32(IB_SIGNAL_ALL_WR);
    } else {
        hr_qp->sq_signal_bits = cpu_to_le32(IB_SIGNAL_REQ_WR);
    }
}

static int hns_roce_create_qp_wqe_sge_list(struct hns_roce_dev *hr_dev,
    struct ib_qp_init_attr *init_attr, struct hns_roce_qp *hr_qp)
{
    struct device *dev = hr_dev->dev;

    if ((hr_dev->caps.flags & HNS_ROCE_CAP_FLAG_RQ_INLINE) &&
        hns_roce_qp_has_rq(init_attr)) {
        /* allocate recv inline buf */
        hr_qp->rq_inl_buf.wqe_list = kcalloc(hr_qp->rq.wqe_cnt,
                                             sizeof(struct hns_roce_rinl_wqe), GFP_KERNEL);
        if (!hr_qp->rq_inl_buf.wqe_list) {
            dev_err(dev, "kcalloc wqe list for qp rq_inl_buf failed\n");
            return -ENOMEM;
        }

        hr_qp->rq_inl_buf.wqe_cnt = (u32)hr_qp->rq.wqe_cnt;

        /* Firstly, allocate a list of sge space buffer */
        hr_qp->rq_inl_buf.wqe_list[0].sg_list = kcalloc(hr_qp->rq_inl_buf.wqe_cnt,
            init_attr->cap.max_recv_sge * sizeof(struct hns_roce_rinl_sge), GFP_KERNEL);
        if (hr_qp->rq_inl_buf.wqe_list[0].sg_list == NULL) {
            dev_err(dev, "kcalloc sg list for qp rq_inl_buf wqe_list[0] failed\n");
            goto err_wqe_list;
        }

        hns_roce_init_inl_sg_list(init_attr, hr_qp);
    }

    return 0;

err_wqe_list:
    if ((hr_dev->caps.flags & HNS_ROCE_CAP_FLAG_RQ_INLINE) &&
            hns_roce_qp_has_rq(init_attr)) {
        kfree(hr_qp->rq_inl_buf.wqe_list);
        hr_qp->rq_inl_buf.wqe_list = NULL;
    }

    return -ENOMEM;
}

static int hns_roce_create_qp_common(struct hns_roce_dev *hr_dev,
                                     struct ib_pd *ib_pd,
                                     struct ib_qp_init_attr *init_attr,
                                     struct ib_udata *udata,
                                     struct hns_roce_qp *hr_qp)
{
    struct device *dev = hr_dev->dev;
    int ret;

    mutex_init(&hr_qp->mutex);
    spin_lock_init(&hr_qp->sq.lock);
    spin_lock_init(&hr_qp->rq.lock);

    hr_qp->state = IB_QPS_RESET;
    hr_qp->next_state = IB_QPS_RESET;
    hr_qp->ibqp.qp_type = init_attr->qp_type;

    hns_roce_init_signal_bits(hr_qp, init_attr);
    ret = hns_roce_set_rq_size(hr_dev, &init_attr->cap, !!ib_pd->uobject, hns_roce_qp_has_rq(init_attr), hr_qp);
    if (ret) {
        dev_err(dev, "hns_roce_set_rq_size failed\n");
        return ret;
    }

    ret = hns_roce_create_qp_wqe_sge_list(hr_dev, init_attr, hr_qp);
    if (ret) {
        dev_err(dev, "hns_roce_create wqe sge list failed\n");
        return ret;
    }

    if (ib_pd->uobject) {
        ret = hns_roce_create_user_qp(hr_dev, udata, ib_pd, init_attr, hr_qp);
    } else {
        ret = hns_roce_create_kernel_qp(hr_dev, init_attr, hr_qp);
    }

    if (ret) {
        dev_err(dev, "hns_roce_qp_create error %d\n", ret);
        goto out;
    }

    return 0;

out:
    hns_roce_free_qp_wqe_sge_list(hr_dev, init_attr, hr_qp);
    return ret;
}

static struct hns_roce_dev *hns_roce_get_roce_dev(struct ib_qp_init_attr *init_attr,
    struct ib_pd *pd)
{
    struct hns_roce_dev *hr_dev = NULL;

    if (init_attr == NULL) {
        pr_err("hns3: null point:init_attr %pK\n", init_attr);
        return NULL;
    }
    hr_dev = pd ? to_hr_dev(pd->device) : to_hr_dev(init_attr->xrcd->device);
    return hr_dev;
}

static int hns_roce_set_xrc_tgt_qp_attr(struct hns_roce_dev *hr_dev, struct ib_pd *pd,
    struct ib_qp_init_attr *init_attr, u16 *xrcdn)
{
    if (!(hr_dev->caps.flags & HNS_ROCE_CAP_FLAG_XRC)) {
        dev_err(hr_dev->dev, "qp_type[%d] get wrong hr_dev caps flags[%llu]\n", IB_QPT_XRC_TGT, hr_dev->caps.flags);
        return -EINVAL;
    }
    pd = to_hr_xrcd(init_attr->xrcd)->pd;
    *xrcdn = to_hr_xrcd(init_attr->xrcd)->xrcdn;
    init_attr->send_cq = to_hr_xrcd(init_attr->xrcd)->cq;
    return 0;
}

static int hns_roce_create_ud_uc_rc_qp(struct hns_roce_dev *hr_dev, struct ib_pd *pd,
    struct ib_qp_init_attr *init_attr, struct ib_udata *udata, struct hns_roce_qp **hr_qp)
{
    int ret;
    struct device *dev = hr_dev->dev;
    *hr_qp = kzalloc(sizeof(struct hns_roce_qp), GFP_KERNEL);
    if (*hr_qp == NULL) {
        dev_err(dev, "kzalloc hns_roce_qp failed\n");
        return -ENOMEM;
    }

    ret = hns_roce_create_qp_common(hr_dev, pd, init_attr, udata, *hr_qp);
    if (ret) {
        dev_err(dev, "Create RC QP 0x%06lx failed(%d)\n", (*hr_qp)->qpn, ret);
        kfree(*hr_qp);
        *hr_qp = NULL;
        return ret;
    }

    (*hr_qp)->ibqp.qp_num = (*hr_qp)->qpn;
    return 0;
}

static int hns_roce_create_gsi_qp(struct hns_roce_dev *hr_dev, struct ib_pd *pd,
    struct ib_qp_init_attr *init_attr, struct ib_udata *udata, struct hns_roce_qp **hr_qp)
{
    int ret;
    struct hns_roce_sqp *hr_sqp = NULL;
    struct device *dev = hr_dev->dev;
    /* Userspace is not allowed to create special QPs: */
    if (pd->uobject) {
        dev_err(dev, "not support usr space GSI\n");
        return -EINVAL;
    }

    hr_sqp = kzalloc(sizeof(*hr_sqp), GFP_KERNEL);
    if (hr_sqp == NULL) {
        dev_err(dev, "kzalloc hns_roce_sqp failed\n");
        return -ENOMEM;
    }

    *hr_qp = &hr_sqp->hr_qp;
    (*hr_qp)->port = init_attr->port_num - 1;
    (*hr_qp)->phy_port = hr_dev->iboe.phy_port[(*hr_qp)->port];

    /* when hw version is v1, the sqpn is allocated */
    if (hr_dev->caps.max_sq_sg <= HNS_ROCE_MAX_SGE_NUM) {
        (*hr_qp)->ibqp.qp_num = HNS_ROCE_MAX_PORTS + hr_dev->iboe.phy_port[(*hr_qp)->port];
    } else {
        (*hr_qp)->ibqp.qp_num = 1;
    }

    ret = hns_roce_create_qp_common(hr_dev, pd, init_attr, udata, *hr_qp);
    if (ret) {
        dev_err(dev, "Create GSI QP failed(%d)!\n", ret);
        kfree(hr_sqp);
        hr_sqp = NULL;
        return ret;
    }

    return 0;
}

struct ib_qp *hns_roce_create_qp(struct ib_pd *pd, struct ib_qp_init_attr *init_attr, struct ib_udata *udata)
{
    struct hns_roce_dev *hr_dev = NULL;
    struct hns_roce_qp *hr_qp = NULL;
    u16 xrcdn = 0;
    int ret;

    hr_dev = hns_roce_get_roce_dev(init_attr, pd);
    if (hr_dev == NULL) {
        pr_err("hns3: Create QP failed, null point\n");
        return ERR_PTR(-EINVAL);
    }

    switch (init_attr->qp_type) {
        case IB_QPT_XRC_TGT:
            ret = hns_roce_set_xrc_tgt_qp_attr(hr_dev, pd, init_attr, &xrcdn);
            if (ret) {
                dev_err(hr_dev->dev, "create xrc tgt qp err ret %d", ret);
                return ERR_PTR(ret);
            }
            /* fall-through */
        case IB_QPT_XRC_INI:
            if (!(hr_dev->caps.flags & HNS_ROCE_CAP_FLAG_XRC)) {
                dev_err(hr_dev->dev, "qp_type[%d] get wrong hr_dev caps flags[%llu]\n", IB_QPT_XRC_INI,
                    hr_dev->caps.flags);
                return ERR_PTR(-EINVAL);
            }
            init_attr->recv_cq = init_attr->send_cq;
            /* fall-through */
        case IB_QPT_UD:
            /* fall-through */
        case IB_QPT_UC:
            /* fall-through */
        case IB_QPT_RC:
            ret = hns_roce_create_ud_uc_rc_qp(hr_dev, pd, init_attr, udata, &hr_qp);
            if (ret) {
                dev_err(hr_dev->dev, "create rc qp failed, ret:%d\n", ret);
                return ERR_PTR(ret);
            }
            hr_qp->xrcdn = xrcdn;
            break;
        case IB_QPT_GSI:
            ret = hns_roce_create_gsi_qp(hr_dev, pd, init_attr, udata, &hr_qp);
            if (ret) {
                dev_err(hr_dev->dev, "create gsi qp failed, ret:%d\n", ret);
                return ERR_PTR(ret);
            }
            break;
        default:
            dev_err(hr_dev->dev, "not support QP type %d\n", init_attr->qp_type);
            return ERR_PTR(-EINVAL);
    }

    hns_roce_inc_rdma_hw_stats(&hr_dev->ib_dev, HW_STATS_QP_ALLOC);

    return &hr_qp->ibqp;
}
#endif

int to_hr_qp_type(int qp_type)
{
    int transport_type;

    if (qp_type == IB_QPT_RC) {
        transport_type = SERV_TYPE_RC;
    } else if (qp_type == IB_QPT_UC) {
        transport_type = SERV_TYPE_UC;
    } else if (qp_type == IB_QPT_UD) {
        transport_type = SERV_TYPE_UD;
    } else if (qp_type == IB_QPT_GSI) {
        transport_type = SERV_TYPE_UD;
    } else if (qp_type == IB_QPT_XRC_INI || qp_type == IB_QPT_XRC_TGT) {
        transport_type = SERV_TYPE_XRC;
    } else {
        transport_type = -1;
    }

    return transport_type;
}

static int check_mtu_validate(struct hns_roce_dev *hr_dev,
                              struct hns_roce_qp *hr_qp,
                              struct ib_qp_attr *attr, int attr_mask)
{
    struct device *dev = hr_dev->dev;
    enum ib_mtu active_mtu;
    int p;

    p = (unsigned int)attr_mask & IB_QP_PORT ? (attr->port_num - 1) : hr_qp->port;
    active_mtu = iboe_get_mtu(hr_dev->iboe.netdevs[p]->mtu);
    if ((hr_dev->caps.max_mtu >= IB_MTU_2048 &&
            attr->path_mtu > hr_dev->caps.max_mtu) ||
            attr->path_mtu < IB_MTU_256 || attr->path_mtu > active_mtu) {
        dev_err(dev, "attr path_mtu(%d)invalid while modify qp(0x%lx)",
                attr->path_mtu, hr_qp->qpn);
#ifndef DEFINE_HNS_LLT
        return -EINVAL;
#endif
    }

    return 0;
}

static int hns_roce_check_qp_rd_attr(struct ib_qp *ibqp, struct ib_qp_attr *attr, int attr_mask)
{
    struct hns_roce_dev *hr_dev = to_hr_dev(ibqp->device);
    struct hns_roce_qp *hr_qp = to_hr_qp(ibqp);
    struct device *dev = hr_dev->dev;
    int access_flags;

    if (((unsigned int)attr_mask & IB_QP_MAX_QP_RD_ATOMIC) && attr->max_rd_atomic > hr_dev->caps.max_qp_init_rdma) {
#ifndef DEFINE_HNS_LLT
        dev_err(dev, "attr max_rd_atomic(%d) invalid.\n", attr->max_rd_atomic);
        return -EINVAL;
#endif
    }

    if (((unsigned int)attr_mask & IB_QP_MAX_DEST_RD_ATOMIC) &&
        attr->max_dest_rd_atomic > hr_dev->caps.max_qp_dest_rdma) {
        dev_err(dev, "attr max_dest_rd_atomic(%d) invalid.\n", attr->max_dest_rd_atomic);
        return -EINVAL;
    }

    access_flags = ((unsigned int)attr_mask & IB_QP_ACCESS_FLAGS) ? attr->qp_access_flags : hr_qp->atomic_rd_en;
    if (((unsigned int)attr_mask & IB_QP_MAX_DEST_RD_ATOMIC) &&
        (((unsigned int)access_flags & IB_ACCESS_REMOTE_READ) &&
        ((unsigned int)access_flags & IB_ACCESS_REMOTE_ATOMIC)) && !attr->max_dest_rd_atomic) {
        dev_warn(dev, "attr max_dest_rd_atomic(%d) will only support write, unless cm.\n", attr->max_dest_rd_atomic);
    }

    return 0;
}

static int hns_roce_check_qp_sl_attr(struct ib_qp *ibqp, struct ib_qp_attr *attr, int attr_mask)
{
    struct hns_roce_dev *hr_dev = to_hr_dev(ibqp->device);
    struct hns_roce_qp *hr_qp = to_hr_qp(ibqp);
    struct device *dev = hr_dev->dev;
    struct pci_dev *pdev = hr_dev->pci_dev;
    struct hnae3_ae_dev *ae_dev = (struct hnae3_ae_dev *)pdev->dev.driver_data;
    struct hclge_dev *hdev = ae_dev->priv;
    u8 dscp = (attr->ah_attr.grh.traffic_class) >> DSCP_SHIFT;
    u8 pfc_en = hdev->tm_info.pfc_en;
    enum ib_qp_state new_state;
    u8 sl = attr->ah_attr.sl;
    u8 prio_to_tc;
    u8 dscp_to_tc;

    new_state = attr_mask & IB_QP_STATE ? attr->qp_state : hr_qp->state;
    if (new_state != IB_QPS_RTR) {
        return 0;
    }

    if (sl >= HNAE3_MAX_USER_PRIO || dscp >= HNAE3_MAX_USER_TOS) {
        dev_warn(dev, "attr sl or dscp exceed limited.sl=%u, dscp=%u\n", sl, dscp);
        return 0;
    }

    if (!((pfc_en >> sl) & 0x1)) {
        dev_warn(dev, "attr sl respond pfc not enable.sl=%u, pfc_en=%u\n", sl, pfc_en);
    }

    prio_to_tc = hdev->tm_info.prio_tc[sl];
    dscp_to_tc = hdev->tm_info.tos_tc[dscp];
    if (prio_to_tc != dscp_to_tc) {
        dev_warn(dev, "sl to tc not match dscp to tc.sl=%u, prio_to_tc=%u, dscp_to_tc:%u\n",
            sl, prio_to_tc, dscp_to_tc);
    }

    return 0;
}

static int hns_roce_check_qp_attr(struct ib_qp *ibqp, struct ib_qp_attr *attr, int attr_mask)
{
    struct hns_roce_dev *hr_dev = to_hr_dev(ibqp->device);
    struct hns_roce_qp *hr_qp = to_hr_qp(ibqp);
    struct device *dev = hr_dev->dev;
    int ret = 0;
    int p;

    if (((unsigned int)attr_mask & IB_QP_PORT) && (attr->port_num == 0 || attr->port_num > hr_dev->caps.num_ports)) {
        dev_err(dev, "attr port_num invalid.attr->port_num=%d\n",
                attr->port_num);
        return -EINVAL;
    }

    if ((unsigned int)attr_mask & IB_QP_PKEY_INDEX) {
        p = (unsigned int)attr_mask & IB_QP_PORT ? (attr->port_num - 1) : hr_qp->port;
        if (attr->pkey_index >= hr_dev->caps.pkey_table_len[p]) {
#ifndef DEFINE_HNS_LLT
            dev_err(dev, "attr pkey_index invalid.attr->pkey_index=%d\n", attr->pkey_index);
            return -EINVAL;
#endif
        }
    }

    if ((unsigned int)attr_mask & IB_QP_PATH_MTU) {
        ret = check_mtu_validate(hr_dev, hr_qp, attr, attr_mask);
#ifndef DEFINE_HNS_LLT
        if (ret) {
            dev_err(dev, "MTU legality check failed\n");
            return ret;
        }
#endif
    }

    ret = hns_roce_check_qp_rd_attr(ibqp, attr, attr_mask);
    if (ret) {
        dev_err(dev, "hns_roce_check_qp_rd_attr check failed\n");
        return ret;
    }

    ret = hns_roce_check_qp_sl_attr(ibqp, attr, attr_mask);
    if (ret) {
        dev_err(dev, "hns_roce_check_qp_sl_attr check failed\n");
        return ret;
    }

    return ret;
}

static void hns_roce_get_cur_new_state(int attr_mask, struct hns_roce_qp *hr_qp,
    struct ib_qp_attr *attr, enum ib_qp_state* cur_state, enum ib_qp_state* new_state)
{
    *cur_state = (unsigned int)attr_mask & IB_QP_CUR_STATE ?
                attr->cur_qp_state : (enum ib_qp_state)hr_qp->state;
    *new_state = (unsigned int)attr_mask & IB_QP_STATE ?
                attr->qp_state : *cur_state;
}

int hns_roce_modify_qp(struct ib_qp *ibqp, struct ib_qp_attr *attr, int attr_mask, struct ib_udata *udata)
{
    struct hns_roce_dev *hr_dev = NULL;
    struct hns_roce_qp *hr_qp = NULL;
    enum ib_qp_state cur_state, new_state;
    int ret = -EINVAL;

    if (ibqp == NULL || attr == NULL) {
        pr_err("hns3: null point:ibqp %pK, attr %pK\n", ibqp, attr);
        return -EINVAL;
    }

    hr_dev = to_hr_dev(ibqp->device);
    hr_qp = to_hr_qp(ibqp);

    mutex_lock(&hr_qp->mutex);
    hns_roce_get_cur_new_state(attr_mask, hr_qp, attr, &cur_state, &new_state);
    hr_qp->next_state = new_state;
    hr_qp->attr_mask = attr_mask;

    /* modify qp to err, need to clean cqe */
    if (ibqp->pd->uobject && ((unsigned int)attr_mask & IB_QP_STATE) && new_state == IB_QPS_ERR) {
        if (hr_qp->sdb_en == 1) {
            hr_qp->sq.head = *(int *)(hr_qp->sdb.virt_addr);
            if (hr_qp->rdb_en == 1) {
                hr_qp->rq.head = *(int *)(hr_qp->rdb.virt_addr);
            }
        } else {
            dev_warn(hr_dev->dev, "flush cqe is not supported in userspace!\n");
            goto out;
        }
    }

    if (!ib_modify_qp_is_ok(cur_state, new_state, ibqp->qp_type, attr_mask)) {
        dev_err(hr_dev->dev, "ib_modify_qp_is_ok failed\n");
        goto out;
    }

    ret = hns_roce_check_qp_attr(ibqp, attr, attr_mask);
    if (ret) {
        dev_err(hr_dev->dev, "hns_roce_check_qp_attr failed, attr_mask[%d], ret[%d]\n", attr_mask, ret);
        goto out;
    }

    if (cur_state == new_state && cur_state == IB_QPS_RESET) {
        ret = hr_dev->caps.min_wqes ? (-EPERM) : 0;
        goto out;
    }

    ret = hr_dev->hw->modify_qp(ibqp, attr, attr_mask, cur_state, new_state);
    if (ret) {
        dev_err(hr_dev->dev, "hw modify qp err, cur_state=%d new_state=%d\n", cur_state, new_state);
    }

out:
    mutex_unlock(&hr_qp->mutex);

    return ret;
}

void hns_roce_lock_cqs(struct hns_roce_cq *send_cq, struct hns_roce_cq *recv_cq)
__acquires(&send_cq->lock) __acquires(&recv_cq->lock)
{
    if (send_cq == recv_cq) {
        spin_lock_irq(&send_cq->lock);
        __acquire(&recv_cq->lock);
    } else if (send_cq->cqn < recv_cq->cqn) {
        spin_lock_irq(&send_cq->lock);
        spin_lock_nested(&recv_cq->lock, SINGLE_DEPTH_NESTING);
    } else {
        spin_lock_irq(&recv_cq->lock);
        spin_lock_nested(&send_cq->lock, SINGLE_DEPTH_NESTING);
    }
}

void hns_roce_unlock_cqs(struct hns_roce_cq *send_cq,
                         struct hns_roce_cq *recv_cq) __releases(&send_cq->lock)
__releases(&recv_cq->lock)
{
    if (send_cq == recv_cq) {
        __release(&recv_cq->lock);
        spin_unlock_irq(&send_cq->lock);
    } else if (send_cq->cqn < recv_cq->cqn) {
        spin_unlock(&recv_cq->lock);
        spin_unlock_irq(&send_cq->lock);
    } else {
        spin_unlock(&send_cq->lock);
        spin_unlock_irq(&recv_cq->lock);
    }
}

static void *get_wqe(struct hns_roce_qp *hr_qp, int offset)
{
    return hns_roce_buf_offset(&hr_qp->hr_buf, offset);
}

void *get_recv_wqe(struct hns_roce_qp *hr_qp, int n)
{
    return get_wqe(hr_qp, hr_qp->rq.offset + ((unsigned int)n << (unsigned int)(hr_qp->rq.wqe_shift)));
}

void *get_send_wqe(struct hns_roce_qp *hr_qp, unsigned int n)
{
    return get_wqe(hr_qp, hr_qp->sq.offset + (n << (unsigned int)(hr_qp->sq.wqe_shift)));
}

void *get_send_extend_sge(struct hns_roce_qp *hr_qp, int n)
{
    return hns_roce_buf_offset(&hr_qp->hr_buf, hr_qp->sge.offset +
                               ((unsigned int)n << (unsigned int)hr_qp->sge.sge_shift));
}

bool hns_roce_wq_overflow(struct hns_roce_wq *hr_wq, int nreq,
    struct ib_cq *ib_cq)
{
    struct hns_roce_cq *hr_cq = NULL;
    u32 cur;

    cur = hr_wq->head - hr_wq->tail;
    if (likely(cur + nreq < hr_wq->max_post)) {
        return false;
    }

    hr_cq = to_hr_cq(ib_cq);
    spin_lock(&hr_cq->lock);
    cur = hr_wq->head - hr_wq->tail;
    spin_unlock(&hr_cq->lock);

    return cur + nreq >= hr_wq->max_post;
}

int hns_roce_init_qp_table(struct hns_roce_dev *hr_dev)
{
    struct hns_roce_qp_table *qp_table = &hr_dev->qp_table;
    int reserved_from_top = 0;
    int reserved_from_bot;
    int ret;

    spin_lock_init(&qp_table->lock);
    INIT_RADIX_TREE(&hr_dev->qp_table_tree, GFP_ATOMIC);

    reserved_from_bot = hr_dev->caps.reserved_qps;

    ret = hns_roce_bitmap_init(&qp_table->bitmap, hr_dev->caps.num_qps,
                               hr_dev->caps.num_qps - 1, reserved_from_bot,
                               reserved_from_top);
    if (ret) {
        dev_err(hr_dev->dev, "qp bitmap init failed!error=%d\n", ret);
        return ret;
    }

    return 0;
}

void hns_roce_cleanup_qp_table(struct hns_roce_dev *hr_dev)
{
    hns_roce_bitmap_cleanup(&hr_dev->qp_table.bitmap);
}

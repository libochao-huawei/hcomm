/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <linux/acpi.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/etherdevice.h>
#include <rdma/ib_cache.h>
#include <rdma/ib_addr.h>
#include <rdma/ib_umem.h>
#include "hnae3.h"
#include "hns_roce_device.h"
#include "hns_roce_common.h"
#include "hns_roce_peer.h"
#include "hns_roce_hem.h"
#include "hns_roce_sec.h"
#include "hns_roce_cmd.h"
#include "hns_roce_sm.h"
#include "hns3_bbox.h"
#include "hns_roce_hw_v2.h"

#ifdef DEFINE_HNS_LLT
#undef static
#define static

#undef inline
#define inline

#endif

static void hns_roce_irq_work_handle_err_event(const struct hns_roce_work *irq_work)
{
    u32 qpn = irq_work->qpn;
    struct device *dev = irq_work->hr_dev->dev;
    int devid = hns_roce_get_devid(irq_work->hr_dev);
    if (devid < 0) {
        dev_err(dev, "get err devid[%d] \n", devid);
        return;
    }
    switch (irq_work->event_type) {
        case HNS_ROCE_EVENT_TYPE_WQ_CATAS_ERROR:
            dev_err(dev, "Local work queue 0x%x catast error, sub_event type is: %d, excep_id 0x%x\n",
                    qpn, irq_work->sub_type, HNS_EXCEPID_ABNM_CQ_CAUSE_CQE_ERR);
            hns_bbox_excep_report((u32)devid, HNS_EXCEPID_ABNM_CQ_CAUSE_CQE_ERR, NULL);
            break;
        case HNS_ROCE_EVENT_TYPE_INV_REQ_LOCAL_WQ_ERROR:
            dev_err(dev, "Invalid request local work queue 0x%x error, excep_id 0x%x\n",
                    qpn, HNS_EXCEPID_INV_REQ_LOCAL_WQ_ERR);
            hns_bbox_excep_report((u32)devid, HNS_EXCEPID_INV_REQ_LOCAL_WQ_ERR, NULL);
            break;
        case HNS_ROCE_EVENT_TYPE_LOCAL_WQ_ACCESS_ERROR:
            dev_err(dev, "Local access violation work queue 0x%x error, sub_event type is: %d, excep_id 0x%x\n",
                    qpn, irq_work->sub_type, HNS_EXCEPID_LOC_WQ_ACCESS_ERR);
            hns_bbox_excep_report((u32)devid, HNS_EXCEPID_LOC_WQ_ACCESS_ERR, NULL);
            break;
        case HNS_ROCE_EVENT_TYPE_SRQ_CATAS_ERROR:
            dev_err(dev, "SRQ catas error.\n");
            break;
        default:
            break;
    }
    return;
}

static void hns_roce_irq_work_handle(struct work_struct *work)
{
    struct hns_roce_work *irq_work = container_of(work, struct hns_roce_work, work);
    struct device *dev = irq_work->hr_dev->dev;
    u32 cqn = irq_work->cqn;
    int devid = hns_roce_get_devid(irq_work->hr_dev);
    if (devid < 0) {
        dev_err(dev, "get err devid[%d] \n", devid);
        return;
    }
    switch (irq_work->event_type) {
        case HNS_ROCE_EVENT_TYPE_PATH_MIG:
            dev_info(dev, "Path migrated succeeded.\n");
            break;
        case HNS_ROCE_EVENT_TYPE_PATH_MIG_FAILED:
            dev_err(dev, "Path migration failed.\n");
            break;
        case HNS_ROCE_EVENT_TYPE_COMM_EST:
            dev_info(dev, "Communication established.\n");
            break;
        case HNS_ROCE_EVENT_TYPE_SQ_DRAINED:
            dev_warn(dev, "Send queue drained, excep_id 0x%x\n", HNS_EXCEPID_SQ_DRAINED_ERR);
            hns_bbox_excep_report((u32)devid, HNS_EXCEPID_SQ_DRAINED_ERR, NULL);
            break;
        case HNS_ROCE_EVENT_TYPE_SRQ_LIMIT_REACH:
            dev_warn(dev, "SRQ limit reach.\n");
            break;
        case HNS_ROCE_EVENT_TYPE_SRQ_LAST_WQE_REACH:
            dev_warn(dev, "SRQ last wqe reach.\n");
            break;
        case HNS_ROCE_EVENT_TYPE_CQ_OVERFLOW:
            dev_warn(dev, "CQ 0x%x overflow, excep_id 0x%x\n", cqn, HNS_EXCEPID_CQ_OVERFLOW_ERR);
            hns_bbox_excep_report((u32)devid, HNS_EXCEPID_CQ_OVERFLOW_ERR, NULL);
            break;
        case HNS_ROCE_EVENT_TYPE_DB_OVERFLOW:
            dev_warn(dev, "DB overflow, excep_id 0x%x\n", HNS_EXCEPID_DB_OVERFLOW_ERR);
            hns_bbox_excep_report((u32)devid, HNS_EXCEPID_DB_OVERFLOW_ERR, NULL);
            break;
        case HNS_ROCE_EVENT_TYPE_FLR:
            dev_warn(dev, "Function level reset.\n");
            break;
        default:
            break;
    }

    hns_roce_irq_work_handle_err_event(irq_work);

    kfree(irq_work);
    irq_work = NULL;
}

static void hns_roce_v2_init_irq_work(struct hns_roce_dev *hr_dev,
    struct hns_roce_eq *eq,
    u32 qpn, u32 cqn)
{
    struct hns_roce_work *irq_work;

    irq_work = kzalloc(sizeof(struct hns_roce_work), GFP_ATOMIC);
    if (irq_work == NULL) {
        dev_err(hr_dev->dev, "alloc hns_roce_work failed\n");
        return;
    }

    INIT_WORK(&(irq_work->work), hns_roce_irq_work_handle);

    irq_work->hr_dev = hr_dev;
    irq_work->qpn = qpn;
    irq_work->cqn = cqn;
    irq_work->event_type = eq->event_type;
    irq_work->sub_type = eq->sub_type;
    queue_work(hr_dev->irq_workq, &(irq_work->work));
}

static void set_eq_cons_index_v2(struct hns_roce_eq *eq)
{
    struct hns_roce_dev *hr_dev = eq->hr_dev;
    u32 doorbell[DOORBELL_NUM];

    doorbell[0] = 0;
    doorbell[1] = 0;

    if (eq->type_flag == HNS_ROCE_AEQ) {
        roce_set_field(doorbell[0], HNS_ROCE_V2_EQ_DB_CMD_M,
                       HNS_ROCE_V2_EQ_DB_CMD_S,
                       eq->arm_st == HNS_ROCE_V2_EQ_ALWAYS_ARMED ?
                       HNS_ROCE_EQ_DB_CMD_AEQ :
                       HNS_ROCE_EQ_DB_CMD_AEQ_ARMED);
    } else {
        roce_set_field(doorbell[0], HNS_ROCE_V2_EQ_DB_TAG_M,
                       HNS_ROCE_V2_EQ_DB_TAG_S, eq->eqn);

        roce_set_field(doorbell[0], HNS_ROCE_V2_EQ_DB_CMD_M,
                       HNS_ROCE_V2_EQ_DB_CMD_S,
                       eq->arm_st == HNS_ROCE_V2_EQ_ALWAYS_ARMED ?
                       HNS_ROCE_EQ_DB_CMD_CEQ :
                       HNS_ROCE_EQ_DB_CMD_CEQ_ARMED);
    }

    roce_set_field(doorbell[1], HNS_ROCE_V2_EQ_DB_PARA_M,
                   HNS_ROCE_V2_EQ_DB_PARA_S,
                   ((unsigned int)eq->cons_index & HNS_ROCE_V2_CONS_IDX_M));

    hns_roce_write64(hr_dev, doorbell, sizeof(doorbell) / sizeof(u32), eq->doorbell);
}

static struct hns_roce_aeqe *get_aeqe_v2(struct hns_roce_eq *eq, u32 entry)
{
    u32 buf_chk_sz;
    unsigned long off;

    buf_chk_sz = 1U << (unsigned int)(eq->eqe_buf_pg_sz + PAGE_SHIFT);
    off = (entry & (eq->entries - 1)) * HNS_ROCE_AEQ_ENTRY_SIZE;

    return (struct hns_roce_aeqe *)((char *)(eq->buf_list->buf) +
                                    off % buf_chk_sz);
}

static struct hns_roce_aeqe *mhop_get_aeqe(struct hns_roce_eq *eq, u32 entry)
{
    u32 chk_sz;
    unsigned long off;

    chk_sz = 1U << (unsigned int)(eq->eqe_buf_pg_sz + PAGE_SHIFT);

    off = (entry & (eq->entries - 1)) * HNS_ROCE_AEQ_ENTRY_SIZE;

    if (eq->hop_num == HNS_ROCE_HOP_NUM_0)
        return (struct hns_roce_aeqe *)((u8 *)(eq->bt_l0) +
                                        off % chk_sz);
    else
        return (struct hns_roce_aeqe *)((u8 *)(eq->buf[off / chk_sz]) +
                                        off % chk_sz);
}

static struct hns_roce_aeqe *next_aeqe_sw_v2(struct hns_roce_eq *eq)
{
    struct hns_roce_aeqe *aeqe = NULL;

    if (!eq->hop_num) {
        aeqe = get_aeqe_v2(eq, eq->cons_index);
    } else {
        aeqe = mhop_get_aeqe(eq, eq->cons_index);
    }

    return (roce_get_bit(aeqe->asyn, HNS_ROCE_V2_AEQ_AEQE_OWNER_S) ^
            (unsigned int)!!(unsigned int)((unsigned int)eq->cons_index & eq->entries)) ? aeqe : NULL;
}

static void hns_roce_v2_aeq_event_handle(struct hns_roce_dev *hr_dev,
    struct hns_roce_eq *eq, struct hns_roce_aeqe *aeqe, u32 qpn, u32 cqn)
{
    struct device *dev = hr_dev->dev;
    int event_type;
    int sub_type;
    u32 srqn;

    event_type = roce_get_field(aeqe->asyn, HNS_ROCE_V2_AEQE_EVENT_TYPE_M,
                                HNS_ROCE_V2_AEQE_EVENT_TYPE_S);
    sub_type = roce_get_field(aeqe->asyn, HNS_ROCE_V2_AEQE_SUB_TYPE_M,
                              HNS_ROCE_V2_AEQE_SUB_TYPE_S);
    srqn = roce_get_field(aeqe->event.srq_event.srq, HNS_ROCE_V2_AEQE_EVENT_QUEUE_NUM_M,
                          HNS_ROCE_V2_AEQE_EVENT_QUEUE_NUM_S);

    switch (event_type) {
        case HNS_ROCE_EVENT_TYPE_PATH_MIG:
        case HNS_ROCE_EVENT_TYPE_PATH_MIG_FAILED:
        case HNS_ROCE_EVENT_TYPE_COMM_EST:
        case HNS_ROCE_EVENT_TYPE_SQ_DRAINED:
        case HNS_ROCE_EVENT_TYPE_WQ_CATAS_ERROR:
        case HNS_ROCE_EVENT_TYPE_SRQ_LAST_WQE_REACH:
        case HNS_ROCE_EVENT_TYPE_INV_REQ_LOCAL_WQ_ERROR:
        case HNS_ROCE_EVENT_TYPE_LOCAL_WQ_ACCESS_ERROR:
            hns_roce_qp_event(hr_dev, qpn, event_type);
            break;
        case HNS_ROCE_EVENT_TYPE_SRQ_LIMIT_REACH:
        case HNS_ROCE_EVENT_TYPE_SRQ_CATAS_ERROR:
            hns_roce_srq_event(hr_dev, srqn, event_type);
            break;
        case HNS_ROCE_EVENT_TYPE_CQ_ACCESS_ERROR:
        case HNS_ROCE_EVENT_TYPE_CQ_OVERFLOW:
            hns_roce_cq_event(hr_dev, cqn, event_type);
            break;
        case HNS_ROCE_EVENT_TYPE_DB_OVERFLOW:
            break;
        case HNS_ROCE_EVENT_TYPE_MB:
            hns_roce_cmd_event(hr_dev, le16_to_cpu(aeqe->event.cmd.token),
                               aeqe->event.cmd.status, le64_to_cpu(aeqe->event.cmd.out_param));
            break;
        case HNS_ROCE_EVENT_TYPE_CEQ_OVERFLOW:
            break;
        case HNS_ROCE_EVENT_TYPE_FLR:
            break;
        default:
            dev_err(dev, "Unhandled event %d on EQ %d at idx %u.\n",
                    event_type, eq->eqn, eq->cons_index);
            break;
    };

    eq->event_type = event_type;
    eq->sub_type = sub_type;
}

static int hns_roce_v2_aeq_int(struct hns_roce_dev *hr_dev,
    struct hns_roce_eq *eq)
{
    struct device *dev = hr_dev->dev;
    struct hns_roce_aeqe *aeqe;
    int aeqe_found = 0;
    u32 ci_max;
    u32 qpn;
    u32 cqn;

    aeqe = next_aeqe_sw_v2(eq);
    while (aeqe != NULL) {
        /* Make sure we read AEQ entry after we have checked the
         * ownership bit
         */
        dma_rmb();
        qpn = roce_get_field(aeqe->event.qp_event.qp, HNS_ROCE_V2_AEQE_EVENT_QUEUE_NUM_M,
                             HNS_ROCE_V2_AEQE_EVENT_QUEUE_NUM_S);
        cqn = roce_get_field(aeqe->event.cq_event.cq, HNS_ROCE_V2_AEQE_EVENT_QUEUE_NUM_M,
                             HNS_ROCE_V2_AEQE_EVENT_QUEUE_NUM_S);

        hns_roce_v2_aeq_event_handle(hr_dev, eq, aeqe, qpn, cqn);
        ++eq->cons_index;
        aeqe_found = 1;

        ci_max = CIMAX(eq->entries);
        if ((u32)eq->cons_index > ci_max) {
            dev_warn(dev, "aeq cons_index overflow, set back to 0.\n");
            eq->cons_index = 0;
        }

        hr_dev->dfx_cnt[HNS_ROCE_DFX_AEQE]++;
        hns_roce_v2_init_irq_work(hr_dev, eq, qpn, cqn);
        aeqe = next_aeqe_sw_v2(eq);
    }

    set_eq_cons_index_v2(eq);
    return aeqe_found;
}

static struct hns_roce_ceqe *get_ceqe_v2(struct hns_roce_eq *eq, u32 entry)
{
    u32 buf_chk_sz;
    unsigned long off;

    buf_chk_sz = 1U << (unsigned int)(eq->eqe_buf_pg_sz + PAGE_SHIFT);
    off = (entry & (eq->entries - 1)) * HNS_ROCE_CEQ_ENTRY_SIZE;

    return (struct hns_roce_ceqe *)((char *)(eq->buf_list->buf) +
                                    off % buf_chk_sz);
}

static struct hns_roce_ceqe *mhop_get_ceqe(struct hns_roce_eq *eq, u32 entry)
{
    u32 buf_chk_sz;
    unsigned long off;

    buf_chk_sz = 1U << (unsigned int)(eq->eqe_buf_pg_sz + PAGE_SHIFT);

    off = (entry & (eq->entries - 1)) * HNS_ROCE_CEQ_ENTRY_SIZE;

    if (eq->hop_num == HNS_ROCE_HOP_NUM_0)
        return (struct hns_roce_ceqe *)((u8 *)(eq->bt_l0) +
                                        off % buf_chk_sz);
    else
        return (struct hns_roce_ceqe *)((u8 *)(eq->buf[off /
                                                       buf_chk_sz]) + off % buf_chk_sz);
}

static struct hns_roce_ceqe *next_ceqe_sw_v2(struct hns_roce_eq *eq)
{
    struct hns_roce_ceqe *ceqe = NULL;

    if (!eq->hop_num) {
        ceqe = get_ceqe_v2(eq, eq->cons_index);
    } else {
        ceqe = mhop_get_ceqe(eq, eq->cons_index);
    }

    return ((unsigned int)!!((unsigned int)roce_get_bit(ceqe->comp, HNS_ROCE_V2_CEQ_CEQE_OWNER_S))) ^
        (unsigned int)(!!((unsigned int)eq->cons_index & eq->entries)) ? ceqe : NULL;
}

static int hns_roce_v2_ceq_int(struct hns_roce_dev *hr_dev,
    struct hns_roce_eq *eq)
{
    struct device *dev = hr_dev->dev;
    struct hns_roce_ceqe *ceqe;
    int ceqe_found = 0;
    u32 ci_max;
    u32 cqn;

    ceqe = next_ceqe_sw_v2(eq);
    while (ceqe != NULL) {
        /* Make sure we read CEQ entry after we have checked the
         * ownership bit
         */
        dma_rmb();

        cqn = roce_get_field(ceqe->comp,
                             HNS_ROCE_V2_CEQE_COMP_CQN_M,
                             HNS_ROCE_V2_CEQE_COMP_CQN_S);

        hns_roce_cq_completion(hr_dev, cqn);

        ++eq->cons_index;
        ceqe_found = 1;

        ci_max = CIMAX(eq->entries);
        if ((u32)eq->cons_index > ci_max) {
            dev_warn(dev, "ceq cons_index overflow, set back to 0.\n");
            eq->cons_index = 0;
        }
        hr_dev->dfx_cnt[HNS_ROCE_DFX_CEQE]++;
        ceqe = next_ceqe_sw_v2(eq);
    }

    set_eq_cons_index_v2(eq);

    return ceqe_found;
}

static irqreturn_t hns_roce_v2_msix_interrupt_eq(int irq, void *eq_ptr)
{
    struct hns_roce_eq *eq = eq_ptr;
    struct hns_roce_dev *hr_dev = eq->hr_dev;
    struct device *dev = hr_dev->dev;
    int int_work = 0;

    if (eq->type_flag == HNS_ROCE_CEQ) {
        /* Completion event interrupt */
        int_work = hns_roce_v2_ceq_int(hr_dev, eq);
    } else if (eq->type_flag == HNS_ROCE_AEQ) {
        /* Asychronous event interrupt */
        int_work = hns_roce_v2_aeq_int(hr_dev, eq);
    } else {
        dev_err(dev, "Unsupport type of event interrrupt!\n");
    }

    return IRQ_RETVAL(int_work);
}

static irqreturn_t hns_roce_v2_msix_interrupt_abn(int irq, void *dev_id)
{
    struct hns_roce_dev *hr_dev = dev_id;
    struct device *dev = hr_dev->dev;
    int int_work = 0;
    u32 int_st;
    u32 int_en;

    /* Abnormal interrupt */
    int_st = roce_read(hr_dev, ROCEE_VF_ABN_INT_ST_REG);
    int_en = roce_read(hr_dev, ROCEE_VF_ABN_INT_EN_REG);

    if (roce_get_bit(int_st, HNS_ROCE_V2_VF_INT_ST_AEQ_OVERFLOW_S)) {
        struct pci_dev *pdev = hr_dev->pci_dev;
        struct hnae3_ae_dev *ae_dev = pci_get_drvdata(pdev);
        const struct hnae3_ae_ops *ops = ae_dev->ops;

        dev_err(dev, "AEQ overflow!\n");

        roce_set_bit(int_st, HNS_ROCE_V2_VF_INT_ST_AEQ_OVERFLOW_S, 1);
        roce_write(hr_dev, ROCEE_VF_ABN_INT_ST_REG, int_st);

        /* Set reset level for the following reset_event() call */
        if (ops->set_default_reset_request)
            ops->set_default_reset_request(ae_dev,
                                           HNAE3_FUNC_RESET);

        if (ops->reset_event) {
            ops->reset_event(pdev, NULL);
        }

        roce_set_bit(int_en, HNS_ROCE_V2_VF_ABN_INT_EN_S, 1);
        roce_write(hr_dev, ROCEE_VF_ABN_INT_EN_REG, int_en);

        int_work = 1;
    } else if (roce_get_bit(int_st, HNS_ROCE_V2_VF_INT_ST_BUS_ERR_S)) {
        dev_err(dev, "BUS ERR!\n");

        roce_set_bit(int_st, HNS_ROCE_V2_VF_INT_ST_BUS_ERR_S, 1);
        roce_write(hr_dev, ROCEE_VF_ABN_INT_ST_REG, int_st);

        roce_set_bit(int_en, HNS_ROCE_V2_VF_ABN_INT_EN_S, 1);
        roce_write(hr_dev, ROCEE_VF_ABN_INT_EN_REG, int_en);

        int_work = 1;
    } else if (roce_get_bit(int_st, HNS_ROCE_V2_VF_INT_ST_OTHER_ERR_S)) {
        dev_err(dev, "OTHER ERR!\n");

        roce_set_bit(int_st, HNS_ROCE_V2_VF_INT_ST_OTHER_ERR_S, 1);
        roce_write(hr_dev, ROCEE_VF_ABN_INT_ST_REG, int_st);

        roce_set_bit(int_en, HNS_ROCE_V2_VF_ABN_INT_EN_S, 1);
        roce_write(hr_dev, ROCEE_VF_ABN_INT_EN_REG, int_en);

        int_work = 1;
    } else {
        dev_err(dev, "There is no abnormal irq found!\n");
    }

    return IRQ_RETVAL(int_work);
}

static void hns_roce_v2_int_mask_enable(struct hns_roce_dev *hr_dev,
    int eq_num, int enable_flag)
{
    int i;

    if (enable_flag == EQ_ENABLE) {
        for (i = 0; i < eq_num; i++)
            roce_write(hr_dev, ROCEE_VF_EVENT_INT_EN_REG +
                       i * EQ_REG_OFFSET,
                       HNS_ROCE_V2_VF_EVENT_INT_EN_M);

        roce_write(hr_dev, ROCEE_VF_ABN_INT_EN_REG,
                   HNS_ROCE_V2_VF_ABN_INT_EN_M);
        roce_write(hr_dev, ROCEE_VF_ABN_INT_CFG_REG,
                   HNS_ROCE_V2_VF_ABN_INT_CFG_M);
    } else {
        for (i = 0; i < eq_num; i++)
            roce_write(hr_dev, ROCEE_VF_EVENT_INT_EN_REG +
                       i * EQ_REG_OFFSET,
                       HNS_ROCE_V2_VF_EVENT_INT_EN_M & 0x0);

        roce_write(hr_dev, ROCEE_VF_ABN_INT_EN_REG,
                   HNS_ROCE_V2_VF_ABN_INT_EN_M & 0x0);
        roce_write(hr_dev, ROCEE_VF_ABN_INT_CFG_REG,
                   HNS_ROCE_V2_VF_ABN_INT_CFG_M & 0x0);
    }
}

static void hns_roce_v2_destroy_eqc(struct hns_roce_dev *hr_dev, int eqn)
{
    struct device *dev = hr_dev->dev;
    struct hns_roce_mbox_post_params mbox_pst_params  = {0};
    int ret;

    if (eqn < hr_dev->caps.num_comp_vectors) {
        mbox_pst_params.in_param = 0;
        mbox_pst_params.out_param = 0;
        mbox_pst_params.in_modifier = (unsigned int)eqn & HNS_ROCE_V2_EQN_M;
        mbox_pst_params.op_modifier = 0;
        mbox_pst_params.op = HNS_ROCE_CMD_DESTROY_CEQC;
        ret = hns_roce_cmd_mbox(hr_dev, mbox_pst_params, HNS_ROCE_CMD_TIMEOUT_MSECS);
    } else {
        mbox_pst_params.in_param = 0;
        mbox_pst_params.out_param = 0;
        mbox_pst_params.in_modifier = (unsigned int)eqn & HNS_ROCE_V2_EQN_M;
        mbox_pst_params.op_modifier = 0;
        mbox_pst_params.op = HNS_ROCE_CMD_DESTROY_AEQC;
        ret = hns_roce_cmd_mbox(hr_dev, mbox_pst_params, HNS_ROCE_CMD_TIMEOUT_MSECS);
    }
    if (ret) {
        dev_err(dev, "[mailbox cmd] destroy eqc(0x%x) failed(%d).\n", eqn, ret);
    }
}

static void hns_roce_mhop_level_two_free_dma(struct device *dev, struct hns_roce_eq *eq, u32 bt_chk_sz,
    u32 buf_chk_sz)
{
    u32 i, j;
    u64 idx;
    int eqe_alloc;
    u64 size;

    for (i = 0; i < eq->l0_last_num; i++) {
        dma_free_coherent(dev, bt_chk_sz, eq->bt_l1[i], eq->l1_dma[i]);
        eq->bt_l1[i] = NULL;
        for (j = 0; j < bt_chk_sz / BA_BYTE_LEN; j++) {
            idx = i * (bt_chk_sz / BA_BYTE_LEN) + j;
            if ((i == eq->l0_last_num - 1)
                    && j == eq->l1_last_num - 1) {
                eqe_alloc = (buf_chk_sz / eq->eqe_size) * idx;
                size = (eq->entries - eqe_alloc) * eq->eqe_size;
                dma_free_coherent(dev, size, eq->buf[idx], eq->buf_dma[idx]);
                eq->buf[idx] = NULL;
                break;
            }
            dma_free_coherent(dev, buf_chk_sz, eq->buf[idx], eq->buf_dma[idx]);
            eq->buf[idx] = NULL;
        }
    }
}

static void hns_roce_mhop_free_dma(struct hns_roce_dev *hr_dev,
    struct hns_roce_eq *eq, u32 buf_chk_sz, u32 bt_chk_sz, u32 mhop_num)
{
    struct device *dev = hr_dev->dev;
    u32 eqe_alloc;
    u64 size;
    u32 i = 0;

    dma_free_coherent(dev, bt_chk_sz, eq->bt_l0, eq->l0_dma);
    eq->bt_l0 = NULL;
    if (mhop_num == 1) {
        for (i = 0; i < eq->l0_last_num; i++) {
            if (i == eq->l0_last_num - 1) {
                eqe_alloc = i * (buf_chk_sz / eq->eqe_size);
                size = (eq->entries - eqe_alloc) * eq->eqe_size;
                dma_free_coherent(dev, size, eq->buf[i], eq->buf_dma[i]);
                eq->buf[i] = NULL;
                break;
            }
            dma_free_coherent(dev, buf_chk_sz, eq->buf[i], eq->buf_dma[i]);
            eq->buf[i] = NULL;
        }
    } else if (mhop_num == PBL_2LEVELS) {
        hns_roce_mhop_level_two_free_dma(dev, eq, bt_chk_sz, buf_chk_sz);
    }
}

static void hns_roce_mhop_free_eq(struct hns_roce_dev *hr_dev,
    struct hns_roce_eq *eq)
{
    struct device *dev = hr_dev->dev;
    u32 buf_chk_sz;
    u32 bt_chk_sz;
    u32 mhop_num;

    mhop_num = hr_dev->caps.eqe_hop_num;
    buf_chk_sz = 1U << (unsigned int)(hr_dev->caps.eqe_buf_pg_sz + PAGE_SHIFT);
    bt_chk_sz = 1U << (unsigned int)(hr_dev->caps.eqe_ba_pg_sz + PAGE_SHIFT);

    if (mhop_num == HNS_ROCE_HOP_NUM_0) {
#ifndef DEFINE_HNS_LLT
        dma_free_coherent(dev, (unsigned int)(eq->entries * eq->eqe_size), eq->bt_l0, eq->l0_dma);
        eq->bt_l0 = NULL;
        return;
#endif
    }

    hns_roce_mhop_free_dma(hr_dev, eq, buf_chk_sz, bt_chk_sz, mhop_num);

    kfree(eq->buf_dma);
    eq->buf_dma = NULL;
    kfree(eq->buf);
    eq->buf = NULL;
    kfree(eq->l1_dma);
    eq->l1_dma = NULL;
    kfree(eq->bt_l1);
    eq->bt_l1 = NULL;
}

static void hns_roce_v2_free_eq(struct hns_roce_dev *hr_dev,
    struct hns_roce_eq *eq)
{
    u32 buf_chk_sz;

    buf_chk_sz = 1U << (unsigned int)(eq->eqe_buf_pg_sz + PAGE_SHIFT);

    if (hr_dev->caps.eqe_hop_num) {
        hns_roce_mhop_free_eq(hr_dev, eq);
        return;
    }

    if (eq->buf_list != NULL) {
        dma_free_coherent(hr_dev->dev, buf_chk_sz, eq->buf_list->buf, eq->buf_list->map);
        eq->buf_list->buf = NULL;
    }
}

static void hns_roce_config_eqc_field(struct hns_roce_eq *eq, struct hns_roce_eq_context *eqc)
{
    /* set eqc state */
    roce_set_field(eqc->byte_4, HNS_ROCE_EQC_EQ_ST_M, HNS_ROCE_EQC_EQ_ST_S, HNS_ROCE_V2_EQ_STATE_VALID);

    /* set eqe hop num */
    roce_set_field(eqc->byte_4, HNS_ROCE_EQC_HOP_NUM_M, HNS_ROCE_EQC_HOP_NUM_S, eq->hop_num);

    /* set eqc over_ignore */
    roce_set_field(eqc->byte_4, HNS_ROCE_EQC_OVER_IGNORE_M, HNS_ROCE_EQC_OVER_IGNORE_S, eq->over_ignore);

    /* set eqc coalesce */
    roce_set_field(eqc->byte_4, HNS_ROCE_EQC_COALESCE_M, HNS_ROCE_EQC_COALESCE_S, eq->coalesce);

    /* set eqc arm_state */
    roce_set_field(eqc->byte_4, HNS_ROCE_EQC_ARM_ST_M, HNS_ROCE_EQC_ARM_ST_S, eq->arm_st);

    /* set eqn */
    roce_set_field(eqc->byte_4, HNS_ROCE_EQC_EQN_M, HNS_ROCE_EQC_EQN_S, eq->eqn);

    /* set eqe_cnt */
    roce_set_field(eqc->byte_4, HNS_ROCE_EQC_EQE_CNT_M, HNS_ROCE_EQC_EQE_CNT_S, HNS_ROCE_EQ_INIT_EQE_CNT);

    /* set eqe_ba_pg_sz */
    roce_set_field(eqc->byte_8, HNS_ROCE_EQC_BA_PG_SZ_M,
                   HNS_ROCE_EQC_BA_PG_SZ_S, eq->eqe_ba_pg_sz + PG_SHIFT_OFFSET);

    /* set eqe_buf_pg_sz */
    roce_set_field(eqc->byte_8, HNS_ROCE_EQC_BUF_PG_SZ_M,
                   HNS_ROCE_EQC_BUF_PG_SZ_S, eq->eqe_buf_pg_sz + PG_SHIFT_OFFSET);

    /* set eq_producer_idx */
    roce_set_field(eqc->byte_8, HNS_ROCE_EQC_PROD_INDX_M, HNS_ROCE_EQC_PROD_INDX_S, HNS_ROCE_EQ_INIT_PROD_IDX);

    /* set eq_max_cnt */
    roce_set_field(eqc->byte_12, HNS_ROCE_EQC_MAX_CNT_M, HNS_ROCE_EQC_MAX_CNT_S, eq->eq_max_cnt);

    /* set eq_period */
    roce_set_field(eqc->byte_12, HNS_ROCE_EQC_PERIOD_M, HNS_ROCE_EQC_PERIOD_S, eq->eq_period);

    /* set eqe_report_timer */
    roce_set_field(eqc->eqe_report_timer, HNS_ROCE_EQC_REPORT_TIMER_M,
                   HNS_ROCE_EQC_REPORT_TIMER_S, HNS_ROCE_EQ_INIT_REPORT_TIMER);

    /* set eqe_ba [34:3] */
    roce_set_field(eqc->eqe_ba0, HNS_ROCE_EQC_EQE_BA_L_M,
                   HNS_ROCE_EQC_EQE_BA_L_S, eq->eqe_ba >> EQBA_LOWBITS_OFFSET);

    /* set eqe_ba [64:35] */
    roce_set_field(eqc->eqe_ba1, HNS_ROCE_EQC_EQE_BA_H_M,
                   HNS_ROCE_EQC_EQE_BA_H_S, eq->eqe_ba >> EQBA_HIGBITS_OFFSET);

    /* set eq shift */
    roce_set_field(eqc->byte_28, HNS_ROCE_EQC_SHIFT_M, HNS_ROCE_EQC_SHIFT_S, eq->shift);

    /* set eq MSI_IDX */
    roce_set_field(eqc->byte_28, HNS_ROCE_EQC_MSI_INDX_M, HNS_ROCE_EQC_MSI_INDX_S, HNS_ROCE_EQ_INIT_MSI_IDX);

    /* set cur_eqe_ba [27:12] */
    roce_set_field(eqc->byte_28, HNS_ROCE_EQC_CUR_EQE_BA_L_M,
                   HNS_ROCE_EQC_CUR_EQE_BA_L_S, eq->cur_eqe_ba >> CUREQBA_OFFSET);

    /* set cur_eqe_ba [59:28] */
    roce_set_field(eqc->byte_32, HNS_ROCE_EQC_CUR_EQE_BA_M_M,
                   HNS_ROCE_EQC_CUR_EQE_BA_M_S, eq->cur_eqe_ba >> CUREQBA_HIGH_OFFSET);

    /* set cur_eqe_ba [63:60] */
    roce_set_field(eqc->byte_36, HNS_ROCE_EQC_CUR_EQE_BA_H_M,
                   HNS_ROCE_EQC_CUR_EQE_BA_H_S, eq->cur_eqe_ba >> CUREQBA_HUGE_OFFSET);

    /* set eq consumer idx */
    roce_set_field(eqc->byte_36, HNS_ROCE_EQC_CONS_INDX_M, HNS_ROCE_EQC_CONS_INDX_S, HNS_ROCE_EQ_INIT_CONS_IDX);

    /* set nex_eqe_ba[43:12] */
    roce_set_field(eqc->nxt_eqe_ba0, HNS_ROCE_EQC_NXT_EQE_BA_L_M,
                   HNS_ROCE_EQC_NXT_EQE_BA_L_S, eq->nxt_eqe_ba >> NEXTEQE_BA_OFFSET);

    /* set nex_eqe_ba[63:44] */
    roce_set_field(eqc->nxt_eqe_ba1, HNS_ROCE_EQC_NXT_EQE_BA_H_M,
                   HNS_ROCE_EQC_NXT_EQE_BA_H_S, eq->nxt_eqe_ba >> NEXTEQE_HIGH_OFFSET);
}

static void hns_roce_config_eqc(struct hns_roce_dev *hr_dev,
                                struct hns_roce_eq *eq,
                                struct hns_roce_eq_context *mb_buf)
{
    struct hns_roce_eq_context *eqc;
    unsigned int eq_period = HNS_ROCE_V2_EQ_DEFAULT_INTERVAL;
    unsigned int eq_max_cnt = HNS_ROCE_V2_EQ_DEFAULT_BURST_NUM;
    unsigned int eq_arm_st = HNS_ROCE_V2_EQ_ALWAYS_ARMED;
    int ret;

    eqc = mb_buf;
    ret = memset_s(eqc, sizeof(struct hns_roce_eq_context), 0, sizeof(struct hns_roce_eq_context));
    HNS_ROCE_SEC_CHECK_RET_VOID(hr_dev->dev, ret);

#ifdef CONFIG_INFINIBAND_HNS_TEST
    test_set_eq_param(eq->type_flag, &eq_period, &eq_max_cnt, &eq_arm_st);
#endif

    /* init eqc */
    eq->doorbell = hr_dev->reg_base + ROCEE_VF_EQ_DB_CFG0_REG;
    eq->hop_num = hr_dev->caps.eqe_hop_num;
    eq->cons_index = 0;
    eq->over_ignore = HNS_ROCE_V2_EQ_OVER_IGNORE_0;
    eq->coalesce = HNS_ROCE_V2_EQ_COALESCE_0;
    eq->arm_st = HNS_ROCE_V2_EQ_ALWAYS_ARMED;
    eq->eqe_ba_pg_sz = hr_dev->caps.eqe_ba_pg_sz;
    eq->eqe_buf_pg_sz = hr_dev->caps.eqe_buf_pg_sz;
    eq->shift = ilog2((unsigned int)eq->entries);
    eq->eq_max_cnt = eq_max_cnt;
    eq->eq_period = eq_period;
    eq->arm_st = eq_arm_st;

    if (!eq->hop_num) {
        eq->eqe_ba = eq->buf_list->map;
    } else {
        eq->eqe_ba = eq->l0_dma;
    }

    hns_roce_config_eqc_field(eq, eqc);
}

STATIC int hns_roce_mhop_alloc_mhop0_eq(struct hns_roce_dev *hr_dev,
    struct hns_roce_eq *eq, u32 buf_chk_sz)
{
    int ret;

    if (eq->entries > buf_chk_sz / eq->eqe_size) {
        dev_err(hr_dev->dev, "eq entries %u is larger than buf_pg_sz!", eq->entries);
        return -EINVAL;
    }
    eq->bt_l0 = dma_alloc_coherent(hr_dev->dev, eq->entries * eq->eqe_size,
                                   &(eq->l0_dma), GFP_KERNEL | __GFP_NOWARN);
    if (eq->bt_l0 == NULL) {
        dev_err(hr_dev->dev, "dma_alloc_coherent hns_roce_eq bt_10 failed\n");
        return -ENOMEM;
    }

    eq->cur_eqe_ba = eq->l0_dma;
    eq->nxt_eqe_ba = 0;

    ret = memset_s(eq->bt_l0, eq->entries * eq->eqe_size, 0, eq->entries * eq->eqe_size);
    if (ret) {
        dev_err(hr_dev->dev, "%s(%d) memset_s failed, ret[%d]\n", __func__, __LINE__, ret);
        dma_free_coherent(hr_dev->dev, eq->entries * eq->eqe_size, eq->bt_l0, eq->l0_dma);
        eq->bt_l0 = NULL;
        return ret;
    }

    return 0;
}

STATIC int hns_roce_mhop_alloc_common_eq_buf_dma(struct hns_roce_dev *hr_dev,
    struct hns_roce_eq *eq, int ba_num, u32 bt_chk_sz)
{
    int ret;
    struct device *dev = hr_dev->dev;

    eq->buf_dma = kcalloc(ba_num, sizeof(*eq->buf_dma), GFP_KERNEL);
    if (eq->buf_dma == NULL) {
        dev_err(dev, "alloc hns_roce_eq buf_dma failed\n");
        return -ENOMEM;
    }
    eq->buf = kcalloc(ba_num, sizeof(*eq->buf), GFP_KERNEL);
    if (eq->buf == NULL) {
        dev_err(dev, "alloc hns_roce_eq buf failed\n");
        goto err_kcalloc_buf;
    }

    /* alloc L0 BT */
    eq->bt_l0 = dma_alloc_coherent(hr_dev->dev, bt_chk_sz, &eq->l0_dma, GFP_KERNEL | __GFP_NOWARN);
    if (eq->bt_l0 == NULL) {
        dev_err(dev, "dma_alloc_coherent hns_roce_eq L0 BT failed\n");
        goto err_dma_alloc_l0;
    }

    ret = memset_s(eq->bt_l0, bt_chk_sz, 0, bt_chk_sz);
    if (ret) {
        dev_err(dev, "%s(%d) memset_s failed, ret[%d]\n", __func__, __LINE__, ret);
        goto mem_err;
    }

    return ret;
mem_err:
    dma_free_coherent(hr_dev->dev, bt_chk_sz, eq->bt_l0, eq->l0_dma);
    eq->bt_l0 = NULL;
err_dma_alloc_l0:
    kfree(eq->buf);
    eq->buf = NULL;
err_kcalloc_buf:
    kfree(eq->buf_dma);
    eq->buf_dma = NULL;

    return -ENOMEM;
}

static void hns_roce_mhop_free_common_eq_buf_dma(struct hns_roce_dev *hr_dev,
    struct hns_roce_eq *eq, u32 bt_chk_sz)
{
    dma_free_coherent(hr_dev->dev, bt_chk_sz, eq->bt_l0, eq->l0_dma);
    eq->bt_l0 = NULL;
    kfree(eq->buf);
    eq->buf = NULL;
    kfree(eq->buf_dma);
    eq->buf_dma = NULL;
}

STATIC int hns_roce_mhop_alloc_every_eq_buf(struct hns_roce_dev *hr_dev,
    struct hns_roce_eq *eq, int ba_num, u32 bt_chk_sz)
{
    int eq_buf_cnt = 0;
    u64 size;
    int i;
    int ret;
    u32 buf_chk_sz;
    u32 eqe_alloc;

    buf_chk_sz = 1U << (unsigned int)(hr_dev->caps.eqe_buf_pg_sz + PAGE_SHIFT);
    /* alloc buf */
    for (i = 0; i < (int)(bt_chk_sz / BA_BYTE_LEN); i++) {
        if (eq_buf_cnt + 1 < ba_num) {
            size = buf_chk_sz;
        } else {
            eqe_alloc = (u32)i * (buf_chk_sz / (u32)eq->eqe_size);
            size = (eq->entries - eqe_alloc) * (u32)eq->eqe_size;
        }

        eq->buf[i] = dma_alloc_coherent(hr_dev->dev, size, &(eq->buf_dma[i]), GFP_KERNEL | __GFP_NOWARN);
        if (eq->buf[i] == NULL) {
            dev_err(hr_dev->dev, "dma_alloc_coherent hns_roce_eq buf[%d] failed\n", i);
            goto err_dma_alloc_buf;
        }

        ret = memset_s(eq->buf[i], size, 0, size);
        if (ret) {
            dev_err(hr_dev->dev, "memset hns_roce_eq buf[%d] err %d, normal ret 0\n", i, ret);
            goto mem_err;
        }

        *(eq->bt_l0 + i) = eq->buf_dma[i];

        eq_buf_cnt++;
        if (eq_buf_cnt >= ba_num) {
            break;
        }
    }

    return i;
mem_err:
    i += 1;  // free cur
err_dma_alloc_buf:            // free before
    for (i -= 1; i >= 0; i--) {
        dma_free_coherent(hr_dev->dev, buf_chk_sz, eq->buf[i], eq->buf_dma[i]);
        eq->buf[i] = NULL;
    }

    return -ENOMEM;
}

STATIC int hns_roce_mhop_alloc_mhop1_eq(struct hns_roce_dev *hr_dev,
    struct hns_roce_eq *eq, int ba_num, u32 bt_chk_sz)
{
    int ret;

    ret =  hns_roce_mhop_alloc_common_eq_buf_dma(hr_dev, eq, ba_num, bt_chk_sz);
    if (ret) {
        dev_err(hr_dev->dev, "alloc buf dma err ret %d, normal ret 0\n", ret);
        return ret;
    }

    if (ba_num > (int)(bt_chk_sz / BA_BYTE_LEN)) {
        dev_err(hr_dev->dev, "ba_num %d is too large for 1 hop\n", ba_num);
        goto out;
    }

    ret = hns_roce_mhop_alloc_every_eq_buf(hr_dev, eq, ba_num, bt_chk_sz);
    if (ret < 0) {
        dev_err(hr_dev->dev, "alloc evert eq buf err ret %d, normal ret >= 0\n", ret);
        goto out;
    }

    eq->l0_last_num = ret + 1;
    eq->cur_eqe_ba = eq->buf_dma[0];
    eq->nxt_eqe_ba = eq->buf_dma[1];

    return 0;

out:
    dma_free_coherent(hr_dev->dev, bt_chk_sz, eq->bt_l0, eq->l0_dma);
    eq->bt_l0 = NULL;
    kfree(eq->buf);
    eq->buf = NULL;
    kfree(eq->buf_dma);
    eq->buf_dma = NULL;
    return ret;
}

STATIC void hns_roce_mhop_free_every_eq_buf_idx(struct hns_roce_dev *hr_dev, struct hns_roce_eq *eq,
    u32 i, u32 j)
{
    u32 buf_chk_sz, bt_chk_sz;
    u32 record_i, record_j;
    u64 idx;

    buf_chk_sz = 1U << (unsigned int)(hr_dev->caps.eqe_buf_pg_sz + PAGE_SHIFT);
    bt_chk_sz = 1U << (hr_dev->caps.eqe_ba_pg_sz + PAGE_SHIFT);

    record_i = i;
    record_j = j;

    for (j = 0; j < bt_chk_sz / BA_BYTE_LEN; j++) {
        if (i == record_i && j >= record_j) {
            break;
        }
        idx = i * bt_chk_sz / BA_BYTE_LEN + j;
        dma_free_coherent(hr_dev->dev, buf_chk_sz, eq->buf[idx], eq->buf_dma[idx]);
        eq->buf[idx] = NULL;
    }
}

STATIC int hns_roce_mhop_alloc_every_eq_buf_idx(struct hns_roce_dev *hr_dev,
    struct hns_roce_eq *eq, int eq_buf_cnt, int *eq_alloc_done, u32 i)
{
    int ba_num, eqe_alloc, ret;
    u64 size, idx;
    u32 buf_chk_sz, bt_chk_sz;
    u32 j;

    buf_chk_sz = 1U << (unsigned int)(hr_dev->caps.eqe_buf_pg_sz + PAGE_SHIFT);
    bt_chk_sz = 1U << (hr_dev->caps.eqe_ba_pg_sz + PAGE_SHIFT);
    ba_num = DIV_ROUND_UP(PAGE_ALIGN(eq->entries * (unsigned int)eq->eqe_size), buf_chk_sz);

    for (j = 0; j < bt_chk_sz / BA_BYTE_LEN; j++) {
        idx = i * bt_chk_sz / BA_BYTE_LEN + j;
        if (eq_buf_cnt + 1 < ba_num) {
            size = buf_chk_sz;
        } else {
            eqe_alloc = (buf_chk_sz / eq->eqe_size) * idx;
            size = (eq->entries - eqe_alloc) * eq->eqe_size;
        }
        eq->buf[idx] = dma_alloc_coherent(hr_dev->dev, size, &(eq->buf_dma[idx]), GFP_KERNEL | __GFP_NOWARN);
        if (eq->buf[idx] == NULL) {
            goto err_dma_alloc_buf;
        }

        ret = memset_s(eq->buf[idx], size, 0, size);
        if (ret) {
            dev_err(hr_dev->dev, "memset hns_roce_eq buf[%llu] failed, ret[%d]\n", idx, ret);
            goto mem_buf_err;
        }
        *(eq->bt_l1[i] + j) = eq->buf_dma[idx];

        eq_buf_cnt++;
        if (eq_buf_cnt >= ba_num) {
            *eq_alloc_done = 1;
            break;
        }
    }
    return (int)j;

mem_buf_err: // free cur
    dma_free_coherent(hr_dev->dev, buf_chk_sz, eq->buf[idx], eq->buf_dma[idx]);
    eq->buf[idx] = NULL;
err_dma_alloc_buf:    // free before

    hns_roce_mhop_free_every_eq_buf_idx(hr_dev, eq, i, j);
    return -ENOMEM;
}

STATIC int hns_roce_mhop_alloc_l1_bt_buf(struct hns_roce_dev *hr_dev,
    struct hns_roce_eq *eq, int ba_num, u32 bt_chk_sz)
{
    int ret = 0;
    int i;
    int eq_alloc_done = 0;
    int eq_buf_cnt = 0;

    for (i = 0; i < (int)(bt_chk_sz / BA_BYTE_LEN); i++) {
        eq->bt_l1[i] = dma_alloc_coherent(hr_dev->dev, bt_chk_sz, &(eq->l1_dma[i]), GFP_KERNEL | __GFP_NOWARN);
        if (eq->bt_l1[i] == NULL) {
            dev_err(hr_dev->dev, "dma_alloc_coherent hns_roce_eq L1 BT failed\n");
            goto err_dma_alloc_l1;
        }

        ret = memset_s(eq->bt_l1[i], bt_chk_sz, 0, bt_chk_sz);
        if (ret) {
            dev_err(hr_dev->dev, "memset hns_roce_eq L1 BT[%d] failed, ret[%d]\n", i, ret);
            goto mem_err;
        }

        *(eq->bt_l0 + i) = eq->l1_dma[i];

        ret = hns_roce_mhop_alloc_every_eq_buf_idx(hr_dev, eq, eq_buf_cnt, &eq_alloc_done, i);
        if (eq_alloc_done > 0) {
            break;
        }

        if (ret < 0) {
            dev_err(hr_dev->dev, "alloc_every_eq_buf_idx err %d, normal ret > 0\n", ret);
            goto mem_err;
        }
        eq_buf_cnt += ret;
    }

    eq->cur_eqe_ba = eq->buf_dma[0];
    eq->nxt_eqe_ba = eq->buf_dma[1];
    eq->l0_last_num = i + 1;
    eq->l1_last_num = ret + 1;

    return 0;
mem_err:            // free cur
    i += 1;
err_dma_alloc_l1:   // free_before
    for (i -= 1; i >= 0; i--) {
        dma_free_coherent(hr_dev->dev, bt_chk_sz, eq->bt_l1[i], eq->l1_dma[i]);
        eq->bt_l1[i] = NULL;
    }

    return -ENOMEM;
}

STATIC int hns_roce_mhop_alloc_mhop2_eq(struct hns_roce_dev *hr_dev,
    struct hns_roce_eq *eq, int ba_num, u32 bt_chk_sz)
{
    int ret;
    u32 buf_chk_sz;
    int bt_num;
    struct device *dev = hr_dev->dev;

    buf_chk_sz = 1U << (unsigned int)(hr_dev->caps.eqe_buf_pg_sz + PAGE_SHIFT);
    bt_num = DIV_ROUND_UP(ba_num, bt_chk_sz / BA_BYTE_LEN);

    ret = hns_roce_mhop_alloc_common_eq_buf_dma(hr_dev, eq, ba_num, bt_chk_sz);
    if (ret) {
        dev_err(dev, "alloc buf dma err ret %d, normal ret 0\n", ret);
        return ret;
    }

    eq->l1_dma = kcalloc(bt_num, sizeof(*eq->l1_dma), GFP_KERNEL);
    if (eq->l1_dma == NULL) {
        dev_err(dev, "alloc hns_roce_eq L1 DMA failed\n");
        goto err_kcalloc_l1_dma;
    }

    eq->bt_l1 = kcalloc(bt_num, sizeof(*eq->bt_l1), GFP_KERNEL);
    if (eq->bt_l1 == NULL) {
        dev_err(dev, "alloc hns_roce_eq L1 BT failed\n");
        goto err_kcalloc_bt_l1;
    }

    ret = hns_roce_mhop_alloc_l1_bt_buf(hr_dev, eq, ba_num, bt_chk_sz);
    if (ret) {
        dev_err(dev, "alloc hns_roce_eq L1 BT buff failed\n");
        goto err_alloc_l1_bt_buf;
    }

    return 0;
err_alloc_l1_bt_buf:
    kfree(eq->bt_l1);
    eq->bt_l1 = NULL;

err_kcalloc_bt_l1:
    kfree(eq->l1_dma);
    eq->l1_dma = NULL;

err_kcalloc_l1_dma:
    hns_roce_mhop_free_common_eq_buf_dma(hr_dev, eq, bt_chk_sz);

    return -ENOMEM;
}

static int hns_roce_mhop_alloc_eq(struct hns_roce_dev *hr_dev,
    struct hns_roce_eq *eq)
{
    u32 buf_chk_sz;
    u32 bt_chk_sz;
    u32 mhop_num;
    int ba_num;
    int ret = 0;

    mhop_num = hr_dev->caps.eqe_hop_num;
    buf_chk_sz = 1U << (unsigned int)(hr_dev->caps.eqe_buf_pg_sz + PAGE_SHIFT);
    bt_chk_sz = 1U << (hr_dev->caps.eqe_ba_pg_sz + PAGE_SHIFT);

    ba_num = DIV_ROUND_UP(PAGE_ALIGN(eq->entries * (unsigned int)eq->eqe_size),
                          buf_chk_sz);

    if (mhop_num == HNS_ROCE_HOP_NUM_0) {
        ret =  hns_roce_mhop_alloc_mhop0_eq(hr_dev, eq, buf_chk_sz);
    } else if (mhop_num == PBL_LEVEL) {
        ret = hns_roce_mhop_alloc_mhop1_eq(hr_dev, eq, ba_num, bt_chk_sz);
    } else if (mhop_num == PBL_2LEVELS) {
        ret = hns_roce_mhop_alloc_mhop2_eq(hr_dev, eq, ba_num, bt_chk_sz);
    }

    return ret;
}

static int hns_roce_cmd_mbox_for_create_eq(struct hns_roce_dev *hr_dev,
    struct hns_roce_eq *eq, unsigned int eq_cmd)
{
    struct hns_roce_mbox_post_params mbox_pst_params = {0};
    struct device *dev = hr_dev->dev;
    int ret;
    struct hns_roce_cmd_mailbox *mailbox = NULL;

    /* Allocate mailbox memory */
    mailbox = hns_roce_alloc_cmd_mailbox(hr_dev);
    HNS_ROCE_ALLOC_CMD_MAILBOX_RET_CHECK(mailbox);

    hns_roce_config_eqc(hr_dev, eq, (struct hns_roce_eq_context *)mailbox->buf);

    mbox_pst_params.in_param = mailbox->dma;
    mbox_pst_params.out_param = 0;
    mbox_pst_params.in_modifier = eq->eqn;
    mbox_pst_params.op_modifier = 0;
    mbox_pst_params.op = eq_cmd;
    ret = hns_roce_cmd_mbox(hr_dev, mbox_pst_params, HNS_ROCE_CMD_TIMEOUT_MSECS);
    if (ret) {
        dev_err(dev, "[mailbox cmd] create eqc(0x%x) failed(%d).\n", eq->eqn, ret);
    }

    hns_roce_free_cmd_mailbox(hr_dev, mailbox);
    return ret;
}

static int hns_roce_v2_create_eq(struct hns_roce_dev *hr_dev, struct hns_roce_eq *eq, unsigned int eq_cmd)
{
    struct device *dev = hr_dev->dev;
    u32 buf_chk_sz = 0;
    int ret = -ENOMEM;

    if (!hr_dev->caps.eqe_hop_num) {
        buf_chk_sz = 1U << (unsigned int)(hr_dev->caps.eqe_buf_pg_sz + PAGE_SHIFT);
        eq->buf_list = kzalloc(sizeof(struct hns_roce_buf_list), GFP_KERNEL);
        if (eq->buf_list == NULL) {
            dev_err(dev, "alloc hns_roce_eq buf list failed\n");
            return ret;
        }

        eq->buf_list->buf = dma_alloc_coherent(dev, buf_chk_sz, &(eq->buf_list->map), GFP_KERNEL | __GFP_NOWARN);
        if (eq->buf_list->buf == NULL) {
            dev_err(dev, "dma_alloc_coherent hns_roce_eq buf_list buff failed\n");
            goto err_alloc_buf;
        }

        ret = memset_s(eq->buf_list->buf, buf_chk_sz, 0, buf_chk_sz);
        HNS_ROCE_SEC_CHECK_GOTO_MEM_ERR(dev, ret);
    } else {
        ret = hns_roce_mhop_alloc_eq(hr_dev, eq);
        if (ret) {
            dev_err(dev, "hns_roce_mhop alloc hns_roce_eq failed\n");
            return ret;
        }
    }

    ret = hns_roce_cmd_mbox_for_create_eq(hr_dev, eq, eq_cmd);
    if (ret) {
        dev_err(dev, "cmd mbox for create eqc(0x%x) failed(%d).\n", eq->eqn, ret);
        goto err_cmd_mbox;
    }

    return 0;

err_cmd_mbox:
    if (!hr_dev->caps.eqe_hop_num) {
        goto mem_err;
    } else {
        hns_roce_mhop_free_eq(hr_dev, eq);
        return ret;
    }

mem_err:
    dma_free_coherent(dev, buf_chk_sz, eq->buf_list->buf, eq->buf_list->map);
    eq->buf_list->buf = NULL;

err_alloc_buf:
    kfree(eq->buf_list);
    eq->buf_list = NULL;

    return ret;
}

static int __hns_roce_set_irq_name(struct hns_roce_dev *hr_dev,
    int irq_num, int aeq_num, int other_num)
{
    int j = 0;
    int ret = 0;
    /* irq contains: abnormal + AEQ + CEQ */
    for (j = 0; j < irq_num; j++) {
        if (j < other_num) {
            ret = snprintf_s((char *)hr_dev->irq_names[j],
                             HNS_ROCE_INT_NAME_LEN, HNS_ROCE_INT_NAME_LEN - 1, "hns-abn-%d", j);
        } else if (j < (other_num + aeq_num)) {
            ret = snprintf_s((char *)hr_dev->irq_names[j],
                             HNS_ROCE_INT_NAME_LEN, HNS_ROCE_INT_NAME_LEN - 1, "hns-aeq-%d",
                             j - other_num);
        } else {
            ret = snprintf_s((char *)hr_dev->irq_names[j],
                             HNS_ROCE_INT_NAME_LEN, HNS_ROCE_INT_NAME_LEN - 1, "hns-ceq-%d",
                             j - other_num - aeq_num);
        }

        if (ret == -1) {
            dev_err(hr_dev->dev, "snprintf_s failed, other_num:[%d], aeq_num[%d] \n", other_num, aeq_num);
            return ret;
        }
    }
    return ret;
}

#ifdef HNS_ROCE_DEVICE
STATIC void __hns_roce_set_irq_affinity(struct hns_roce_dev *hr_dev,
    int irq_num, int other_num)
{
    int j = 0;
    struct hns_roce_eq_table *eq_table = &hr_dev->eq_table;
    int devid = hns_roce_get_devid(hr_dev);
    if (devid < 0) {
        dev_err(hr_dev->dev, "get err devid[%d] \n", devid);
        return;
    }
    for (j = 0; j < irq_num; j++) {
        if (j < other_num) {
            irq_set_affinity_hint(hr_dev->irq[j],
                cpumask_of((j & 0x1) + devid * HNS_ROCE_DEVICE_CPUS));
        } else {
            irq_set_affinity_hint(eq_table->eq[j - other_num].irq,
                cpumask_of((j & 0x1) + devid * HNS_ROCE_DEVICE_CPUS));
        }
    }
}
#endif

static int __hns_roce_request_irq(struct hns_roce_dev *hr_dev, int irq_num, int comp_num, int aeq_num, int other_num)
{
    struct hns_roce_eq_table *eq_table = &hr_dev->eq_table;
    int i, j, ret;

    for (i = 0; i < irq_num; i++) {
        hr_dev->irq_names[i] = kzalloc(HNS_ROCE_INT_NAME_LEN, GFP_KERNEL);
        if (hr_dev->irq_names[i] == NULL) {
            ret = -ENOMEM;
            dev_err(hr_dev->dev, "alloc hr_dev irq_names[%d] failed\n", i);
            goto err_kzalloc_failed;
        }
    }

    ret = __hns_roce_set_irq_name(hr_dev, irq_num, aeq_num, other_num);
    HNS_ROCE_SPRINTF_CHECK_GOTO_KZ_FAILED(hr_dev->dev, ret);

    for (j = 0; j < irq_num; j++) {
#ifndef DEFINE_HNS_LLT
        if (j < other_num) {
            ret = request_irq(hr_dev->irq[j], hns_roce_v2_msix_interrupt_abn, 0, hr_dev->irq_names[j], hr_dev);
        } else if (j < (other_num + comp_num)) {
            ret = request_irq(eq_table->eq[j - other_num].irq, hns_roce_v2_msix_interrupt_eq,
                              0, hr_dev->irq_names[j + aeq_num], &eq_table->eq[j - other_num]);
        } else {
            ret = request_irq(eq_table->eq[j - other_num].irq, hns_roce_v2_msix_interrupt_eq,
                              0, hr_dev->irq_names[j - comp_num], &eq_table->eq[j - other_num]);
        }
        if (ret) {
            dev_err(hr_dev->dev, "Request irq error, ret = %d\n", ret);
            goto err_request_failed;
        }
#endif
    }

#ifdef HNS_ROCE_DEVICE
    __hns_roce_set_irq_affinity(hr_dev, irq_num, other_num);
#endif
    return 0;

err_request_failed:
    for (j -= 1; j >= 0; j--) {
        if (j < other_num) {
            free_irq(hr_dev->irq[j], hr_dev);
        } else {
            free_irq(eq_table->eq[j - other_num].irq, &eq_table->eq[j - other_num]);
        }
    }
err_kzalloc_failed:
    for (i -= 1; i >= 0; i--) {
        kfree(hr_dev->irq_names[i]);
        hr_dev->irq_names[i] = NULL;
    }

    return ret;
}

static void __hns_roce_free_irq(struct hns_roce_dev *hr_dev)
{
    int irq_num;
    int eq_num;
    int i;

    eq_num = hr_dev->caps.num_comp_vectors + hr_dev->caps.num_aeq_vectors;
    irq_num = eq_num + hr_dev->caps.num_other_vectors;

    for (i = 0; i < hr_dev->caps.num_other_vectors; i++) {
#ifdef HNS_ROCE_DEVICE
        irq_set_affinity_hint(hr_dev->irq[i], NULL);
#endif
        free_irq(hr_dev->irq[i], hr_dev);
    }

    for (i = 0; i < eq_num; i++) {
#ifdef HNS_ROCE_DEVICE
        irq_set_affinity_hint(hr_dev->eq_table.eq[i].irq, NULL);
#endif
        free_irq(hr_dev->eq_table.eq[i].irq, &hr_dev->eq_table.eq[i]);
    }

    for (i = 0; i < irq_num; i++) {
        kfree(hr_dev->irq_names[i]);
        hr_dev->irq_names[i] = NULL;
    }
}

static void hns_roce_v2_clean_eq_table(struct hns_roce_dev *hr_dev, int i, struct hns_roce_eq_table *eq_table)
{
    for (i -= 1; i >= 0; i--) {
        hns_roce_v2_destroy_eqc(hr_dev, i);
        hns_roce_v2_free_eq(hr_dev, &eq_table->eq[i]);
    }

    kfree(eq_table->eq);
    eq_table->eq = NULL;
}

static int hns_roce_v2_set_eq_table(struct hns_roce_dev *hr_dev,
    int eq_num, int comp_num, int aeq_num, int other_num)
{
    struct hns_roce_eq_table *eq_table = &hr_dev->eq_table;
    struct device *dev = hr_dev->dev;
    struct hns_roce_eq *eq = NULL;
    unsigned int eq_cmd;
    int ret;
    int i;

    eq_table->eq = kcalloc(eq_num, sizeof(*eq_table->eq), GFP_KERNEL);
    if (eq_table->eq == NULL) {
        dev_err(dev, "alloc hns_roce_eq_table hns_roce_eq failed\n");
        return -ENOMEM;
    }

    /* create eq */
    for (i = 0; i < eq_num; i++) {
        eq = &eq_table->eq[i];
        eq->hr_dev = hr_dev;
        eq->eqn = i;
        if (i < comp_num) {
            /* CEQ */
            eq_cmd = HNS_ROCE_CMD_CREATE_CEQC;
            eq->type_flag = HNS_ROCE_CEQ;
            eq->entries = hr_dev->caps.ceqe_depth;
            eq->eqe_size = HNS_ROCE_CEQ_ENTRY_SIZE;
            eq->irq = hr_dev->irq[i + other_num + aeq_num];
            eq->eq_max_cnt = HNS_ROCE_CEQ_DEFAULT_BURST_NUM;
            eq->eq_period = HNS_ROCE_CEQ_DEFAULT_INTERVAL;
        } else {
            /* AEQ */
            eq_cmd = HNS_ROCE_CMD_CREATE_AEQC;
            eq->type_flag = HNS_ROCE_AEQ;
            eq->entries = hr_dev->caps.aeqe_depth;
            eq->eqe_size = HNS_ROCE_AEQ_ENTRY_SIZE;
            eq->irq = hr_dev->irq[i - comp_num + other_num];
            eq->eq_max_cnt = HNS_ROCE_AEQ_DEFAULT_BURST_NUM;
            eq->eq_period = HNS_ROCE_AEQ_DEFAULT_INTERVAL;
        }

        ret = hns_roce_v2_create_eq(hr_dev, eq, eq_cmd);
        if (ret) {
            dev_err(dev, "eq(0x%x) create failed(%d).\n", eq->eqn, ret);
            goto err_create_eq_fail;
        }
    }

    return 0;

err_create_eq_fail:
    hns_roce_v2_clean_eq_table(hr_dev, i, eq_table);
    return ret;
}

int hns_roce_v2_init_eq_table(struct hns_roce_dev *hr_dev)
{
    struct device *dev = hr_dev->dev;
    int irq_num;
    int eq_num;
    int other_num;
    int comp_num;
    int aeq_num;
    int ret;

    other_num = hr_dev->caps.num_other_vectors;
    comp_num = hr_dev->caps.num_comp_vectors;
    aeq_num = hr_dev->caps.num_aeq_vectors;

    eq_num = comp_num + aeq_num;
    irq_num = eq_num + other_num;

    ret = hns_roce_v2_set_eq_table(hr_dev, eq_num, comp_num, aeq_num, other_num);
    if (ret) {
        dev_err(dev, "set eq table failed.\n");
        return ret;
    }

    /* enable irq */
    hns_roce_v2_int_mask_enable(hr_dev, eq_num, EQ_ENABLE);

    ret = __hns_roce_request_irq(hr_dev, irq_num, comp_num, aeq_num, other_num);
    if (ret) {
        dev_err(dev, "Request irq failed.\n");
        goto err_request_irq_fail;
    }

    hr_dev->irq_workq = create_singlethread_workqueue("hns_roce_irq_workqueue");
    if (hr_dev->irq_workq == NULL) {
        dev_err(dev, "Create irq workqueue failed!\n");
        ret = -ENOMEM;
        goto err_create_wq_fail;
    }

    return 0;

err_create_wq_fail:
    __hns_roce_free_irq(hr_dev);

err_request_irq_fail:
    hns_roce_v2_int_mask_enable(hr_dev, eq_num, EQ_DISABLE);
    hns_roce_v2_clean_eq_table(hr_dev, eq_num, &hr_dev->eq_table);
    return ret;
}

void hns_roce_v2_cleanup_eq_table(struct hns_roce_dev *hr_dev)
{
    struct hns_roce_eq_table *eq_table = &hr_dev->eq_table;
    int irq_num;
    int eq_num;
    int i;

    eq_num = hr_dev->caps.num_comp_vectors + hr_dev->caps.num_aeq_vectors;
    irq_num = eq_num + hr_dev->caps.num_other_vectors;

    /* Disable irq */
    hns_roce_v2_int_mask_enable(hr_dev, eq_num, EQ_DISABLE);

    __hns_roce_free_irq(hr_dev);

    for (i = 0; i < eq_num; i++) {
        hns_roce_v2_destroy_eqc(hr_dev, i);

        hns_roce_v2_free_eq(hr_dev, &eq_table->eq[i]);
    }

    kfree(eq_table->eq);
    eq_table->eq = NULL;

    flush_workqueue(hr_dev->irq_workq);
    destroy_workqueue(hr_dev->irq_workq);
}

static void hns_roce_config_llm_field(struct hns_roce_cfg_llm_a *req_a,
    struct hns_roce_cfg_llm_b *req_b, struct hns_roce_cmq_desc *desc,
    struct hns_roce_link_table *link_tbl, enum hns_roce_opcode_type opcode)
{
    struct hns_roce_link_table_entry *entry = NULL;
    u32 page_num;
    int i;

    page_num = link_tbl->npages;
    entry = link_tbl->table.buf;

    for (i = 0; i < CONFIG_LLM_CMDQ_DESC_NUM; i++) {
        hns_roce_cmq_setup_basic_desc(&desc[i], opcode, false);

        if (i == 0) {
            desc[i].flag |= cpu_to_le16(HNS_ROCE_CMD_FLAG_NEXT);
        } else {
            desc[i].flag &= ~cpu_to_le16(HNS_ROCE_CMD_FLAG_NEXT);
        }

        if (i == 0) {
            req_a->base_addr_l = link_tbl->table.map & LOWBITS_MASK;
            req_a->base_addr_h = (link_tbl->table.map >> LOWBITS_WIDTH) &
                                 LOWBITS_MASK;
            roce_set_field(req_a->depth_pgsz_init_en,
                           CFG_LLM_QUE_DEPTH_M,
                           CFG_LLM_QUE_DEPTH_S,
                           link_tbl->npages);
            roce_set_field(req_a->depth_pgsz_init_en,
                           CFG_LLM_QUE_PGSZ_M,
                           CFG_LLM_QUE_PGSZ_S,
                           link_tbl->pg_sz);
            req_a->head_ba_l = entry[0].blk_ba0;
            req_a->head_ba_h_nxtptr = entry[0].blk_ba1_nxt_ptr;
            roce_set_field(req_a->head_ptr, CFG_LLM_HEAD_PTR_M, CFG_LLM_HEAD_PTR_S, 0);
        } else {
            req_b->tail_ba_l = entry[page_num - FSTLAST_PG].blk_ba0;
            roce_set_field(req_b->tail_ba_h,
                           CFG_LLM_TAIL_BA_H_M,
                           CFG_LLM_TAIL_BA_H_S,
                           entry[page_num - 1].blk_ba1_nxt_ptr &
                           HNS_ROCE_LINK_TABLE_BA1_M);
            /* (page_num - 2) indicates to the second last page */
            roce_set_field(req_b->tail_ptr,
                           CFG_LLM_TAIL_PTR_M,
                           CFG_LLM_TAIL_PTR_S,
                           (entry[page_num - SNDLAST_PG].blk_ba1_nxt_ptr &
                            HNS_ROCE_LINK_TABLE_NXT_PTR_M) >>
                           HNS_ROCE_LINK_TABLE_NXT_PTR_S);
        }
    }

    roce_set_field(req_a->depth_pgsz_init_en, CFG_LLM_INIT_EN_M, CFG_LLM_INIT_EN_S, 1);
}

static int hns_roce_config_link_table(struct hns_roce_dev *hr_dev,
    enum hns_roce_link_table_type type)
{
    struct hns_roce_cmq_desc desc[CONFIG_LLM_CMDQ_DESC_NUM] = { {0} };
    struct hns_roce_cfg_llm_a *req_a =
        (struct hns_roce_cfg_llm_a *)desc[0].data;
    struct hns_roce_cfg_llm_b *req_b =
        (struct hns_roce_cfg_llm_b *)desc[1].data;
    struct hns_roce_v2_priv *priv = hr_dev->priv;
    struct hns_roce_link_table *link_tbl = NULL;
    enum hns_roce_opcode_type opcode;
    int ret;

    switch (type) {
        case TSQ_LINK_TABLE:
            link_tbl = &priv->tsq;
            opcode = HNS_ROCE_OPC_CFG_EXT_LLM;
            break;
        case TPQ_LINK_TABLE:
            link_tbl = &priv->tpq;
            opcode = HNS_ROCE_OPC_CFG_TMOUT_LLM;
            break;
        default:
            dev_err(hr_dev->dev, "not supported link table type: 0x%x!\n", type);
            return -EINVAL;
    }

    ret = memset_s(req_a, sizeof(*req_a), 0, sizeof(*req_a));
    HNS_ROCE_SEC_CHECK_RET_INT(hr_dev->dev, ret);
    ret = memset_s(req_b, sizeof(*req_b), 0, sizeof(*req_b));
    HNS_ROCE_SEC_CHECK_RET_INT(hr_dev->dev, ret);

    hns_roce_config_llm_field(req_a, req_b, desc, link_tbl, opcode);

    return hns_roce_cmq_send(hr_dev, desc, CONFIG_LLM_CMDQ_DESC_NUM);
}

static int hns_roce_init_link_tbl_pg_list(struct hns_roce_dev *hr_dev,
    struct hns_roce_link_table *link_tbl, u32 buf_chk_sz, u32 pg_num)
{
    struct hns_roce_link_table_entry *entry = NULL;
    struct device *dev = hr_dev->dev;
    dma_addr_t t;
    int ret, i;

    link_tbl->pg_list = kcalloc(pg_num, sizeof(*link_tbl->pg_list), GFP_KERNEL);
    if (link_tbl->pg_list == NULL) {
        dev_err(hr_dev->dev, "alloc link table page list failed\n");
        return -ENOMEM;
    }

    entry = link_tbl->table.buf;
    for (i = 0; i < (int)pg_num; ++i) {
        link_tbl->pg_list[i].buf = dma_alloc_coherent(dev, buf_chk_sz,
                                                      &t, GFP_KERNEL | __GFP_NOWARN);
        if (link_tbl->pg_list[i].buf == NULL) {
            dev_err(hr_dev->dev, "dma_alloc_coherent link table buff failed\n");
            goto err_alloc_buf_failed;
        }

        link_tbl->pg_list[i].map = t;

        ret = memset_s(link_tbl->pg_list[i].buf, buf_chk_sz, 0, buf_chk_sz);
        if (ret) {
            /* free from current pg_list node instead of previous one */
            i++;
            dev_err(dev, "%s(%d) memset_s failed, ret[%d]\n", __func__, __LINE__, ret);
            goto err_alloc_buf_failed;
        }

        entry[i].blk_ba0 = (t >> PAGE_4K) & LOWBITS_MASK;
        roce_set_field(entry[i].blk_ba1_nxt_ptr,
                       HNS_ROCE_LINK_TABLE_BA1_M,
                       HNS_ROCE_LINK_TABLE_BA1_S,
                       t >> PA_BITS_NUM);

        if (i < (int)(pg_num - 1))
            roce_set_field(entry[i].blk_ba1_nxt_ptr,
                           HNS_ROCE_LINK_TABLE_NXT_PTR_M,
                           HNS_ROCE_LINK_TABLE_NXT_PTR_S,
                           i + 1);
    }
    link_tbl->npages = pg_num;
    link_tbl->pg_sz = buf_chk_sz;

    return 0;

err_alloc_buf_failed:
    for (i -= 1; i >= 0; i--) {
        dma_free_coherent(dev, buf_chk_sz, link_tbl->pg_list[i].buf, link_tbl->pg_list[i].map);
        link_tbl->pg_list[i].buf = NULL;
    }
    kfree(link_tbl->pg_list);
    link_tbl->pg_list = NULL;

    return -ENOMEM;
}

int hns_roce_init_link_table(struct hns_roce_dev *hr_dev, enum hns_roce_link_table_type type)
{
    struct hns_roce_v2_priv *priv = hr_dev->priv;
    struct hns_roce_link_table *link_tbl = NULL;
    struct device *dev = hr_dev->dev;
    u32 pg_num_a, pg_num_b;
    size_t size;
    u32 func_num = 1;
    u32 buf_chk_sz;
    u32 pg_num;
    int ret;

    switch (type) {
        case TSQ_LINK_TABLE:
            link_tbl = &priv->tsq;
            buf_chk_sz = 1U << (hr_dev->caps.tsq_buf_pg_sz + PAGE_SHIFT);
            pg_num_a = (u32)hr_dev->caps.num_qps * QP_EX_DB_SIZE / buf_chk_sz;
            /*
             * every transport service queue(tsq) need 2 page and reserved 1
             * page, it includes tx queue and rx queue.
             */
            pg_num_b = hr_dev->caps.sl_num * HNS_ROCE_HW_NUM_FOUR + HNS_ROCE_HW_NUM_TWO;
            break;
        case TPQ_LINK_TABLE:
            link_tbl = &priv->tpq;
            buf_chk_sz = 1U << (hr_dev->caps.tpq_buf_pg_sz + PAGE_SHIFT);
            pg_num_a = (u32)hr_dev->caps.num_cqs * CQ_EX_DB_SIZE / buf_chk_sz;
            /* every function need 2 page and reserved 2 page */
            pg_num_b = 2 * TIMEOUT_POLL_QUEUE_NUM * func_num + 2;
            break;
        default:
            dev_err(hr_dev->dev, "not support type [%d]\n", type);
            return -EINVAL;
    }

    pg_num = max(pg_num_a, pg_num_b);
    size = pg_num * sizeof(struct hns_roce_link_table_entry);

    link_tbl->table.buf = dma_alloc_coherent(dev, size, &link_tbl->table.map, GFP_KERNEL | __GFP_NOWARN);
    if (link_tbl->table.buf == NULL) {
        dev_err(hr_dev->dev, "dma_alloc_coherent link table buff failed\n");
        goto out;
    }

    ret = memset_s(link_tbl->table.buf, size, 0, size);
    if (ret) {
        dev_err(dev, "%s(%d) memset_s failed, ret[%d]\n", __func__, __LINE__, ret);
        goto err_kcalloc_failed;
    }

    ret = hns_roce_init_link_tbl_pg_list(hr_dev, link_tbl, buf_chk_sz, pg_num);
    if (ret) {
        dev_err(dev, "hns_roce_init_link_tbl_pg_list failed, ret[%d]\n", ret);
        goto err_kcalloc_failed;
    }

    return hns_roce_config_link_table(hr_dev, type);

err_kcalloc_failed:
    dma_free_coherent(dev, size, link_tbl->table.buf, link_tbl->table.map);
    link_tbl->table.buf = NULL;

out:
    return -ENOMEM;
}

void hns_roce_free_link_table(struct hns_roce_dev *hr_dev,
    struct hns_roce_link_table *link_tbl)
{
    struct device *dev = hr_dev->dev;
    size_t size;
    u32 i;

    size = link_tbl->npages * sizeof(struct hns_roce_link_table_entry);

    for (i = 0; i < link_tbl->npages; ++i)
        if (link_tbl->pg_list[i].buf) {
            dma_free_coherent(dev, link_tbl->pg_sz, link_tbl->pg_list[i].buf, link_tbl->pg_list[i].map);
            link_tbl->pg_list[i].buf = NULL;
    }
    kfree(link_tbl->pg_list);
    link_tbl->pg_list = NULL;

    dma_free_coherent(dev, size, link_tbl->table.buf, link_tbl->table.map);
    link_tbl->table.buf = NULL;
}

static int hns_roce_mem_size_to_win_size(struct hns_roce_dev *hr_dev, u64 mem_size, int *w_size)
{
    /* window size should be 16M aligned */
    if ((mem_size & MASK_24BITS) || (mem_size <= MASK_24BITS)) {
        dev_warn(hr_dev->dev, "mem size(0x%llx) should be 16M aligned.\n", mem_size);
        return -EFAULT;
    }

    *w_size = ilog2(mem_size >> ALIGNED_16M_OFFSET);
#ifndef DEFINE_HNS_LLT
    if (*w_size > WSIZE_HIGHBITS_NUM) {
        dev_warn(hr_dev->dev, "mem size(0x%llx) should less than 128T.\n", mem_size);
        return -EFAULT;
    }
#endif

    return 0;
}

static void hns_roce_config_atu_field(struct hns_roce_cfg_atu_tb *atu_tb, u64 hpa, u64 dpa,
    int win_size, unsigned long win_idx)
{
    u32 win_start_addr_l = (hpa >> ADDR_LOWBITS_OFFSET) & LOWBITS_MASK;
    u32 win_start_addr_h = (hpa >> (ADDR_LOWBITS_OFFSET + ADDR_HIGHBITS_OFFSET)) & MASK_8BITS;
    u32 win_trans_addr = (dpa >> ADDR_LOWBITS_OFFSET) & MASK_24BITS;
    int cmd_type = 0x0; /* SMMU bypass */

    roce_set_bit(atu_tb->win_cfg,
                 CFG_ATU_TB_WIN_EN_S,
                 1);
    roce_set_field(atu_tb->win_cfg,
                   CFG_ATU_TB_WIN_SIZE_M,
                   CFG_ATU_TB_WIN_SIZE_S, win_size);
    roce_set_field(atu_tb->win_cfg,
                   CFG_ATU_TB_CMD_TYPE_M,
                   CFG_ATU_TB_CMD_TYPE_S, cmd_type);
    roce_set_field(atu_tb->win_cfg,
                   CFG_ATU_TB_WIN_IDX_M,
                   CFG_ATU_TB_WIN_IDX_S, win_idx);

    atu_tb->win_start_addr_l = cpu_to_le32(win_start_addr_l);

    roce_set_field(atu_tb->win_trans_addr,
                   CFG_ATU_TB_WIN_TRANS_ADDR_M,
                   CFG_ATU_TB_WIN_TRANS_ADDR_S, win_trans_addr);
    roce_set_field(atu_tb->win_trans_addr,
                   CFG_ATU_TB_WIN_START_ADDR_H_M,
                   CFG_ATU_TB_WIN_START_ADDR_H_S, win_start_addr_h);
}

static int hns_roce_config_atu_table(struct hns_roce_dev *hr_dev, u64 hpa, u64 size, u64 dpa)
{
    int ret;
    struct hns_roce_cmq_desc desc;
    struct hns_roce_cfg_atu_tb *atu_tb = (struct hns_roce_cfg_atu_tb *)desc.data;
    int win_size;
    unsigned long win_idx;

    ret = hns_roce_atu_alloc(hr_dev, &win_idx);
    if (ret) {
        dev_err(hr_dev->dev, "hns_roce_alloc atu failed\n");
        goto err_atu_alloc;
    }

    ret = hns_roce_mem_size_to_win_size(hr_dev, size, &win_size);
    if (ret) {
        dev_err(hr_dev->dev, "hns_roce_mem size [%llu] to win_size failed\n", size);
        goto err_mem_h2d;
    }

    dev_info(hr_dev->dev, "win_idx:%lu, size:0x%llx, win_size:0x%x.\n", win_idx, size, win_size);
    hns_roce_cmq_setup_basic_desc(&desc, HNS_ROCE_OPC_CFG_ATU_TB, false);

    hns_roce_config_atu_field(atu_tb, hpa, dpa, win_size, win_idx);

    dev_info(hr_dev->dev, "CMDQ:0x%08x 0x%08x 0x%08x\n", desc.data[0], desc.data[1], desc.data[HNS_ROCE_HW_NUM_TWO]);
    ret = hns_roce_cmq_send(hr_dev, &desc, 1);
    if (ret) {
        dev_err(hr_dev->dev, "%s(%d) hns_roce_cmq send failed, ret[%d]\n", __func__, __LINE__, ret);
        goto err_cmq_send;
    }

    return 0;

err_cmq_send:
err_mem_h2d:
    hns_roce_atu_free(hr_dev, win_idx);

err_atu_alloc:
    return ret;
}

int hns_roce_v2_reset_atu_table(struct hns_roce_dev *hr_dev)
{
    struct hns_roce_cmq_desc desc;
    struct hns_roce_cfg_atu_tb *atu_tb =
        (struct hns_roce_cfg_atu_tb *)desc.data;
    if (hr_dev == NULL) {
        pr_err("hns3: param is NULL\n");
        return -EINVAL;
    }
    if (hr_dev->pci_dev->device != HNS_SDI_MODE_DEV_ID &&
            hr_dev->pci_dev->device != HNS_SDI_100G_DEV_ID) {
        dev_notice(hr_dev->dev, "ATU Reset: Non-SDI Mode.\n");
        return 0;
    }

    hns_roce_cmq_setup_basic_desc(&desc, HNS_ROCE_OPC_CFG_ATU_TB, false);

    roce_set_bit(atu_tb->win_cfg, CFG_ATU_TB_RST_EN_S, 1);

    return hns_roce_cmq_send(hr_dev, &desc, 1);
}

STATIC struct pci_dev *hns_roce_get_peer_mem_dev(struct hns_roce_dev *hr_dev)
{
    int bus_num = hr_dev->pci_dev->bus->number;
    u32 dev_fn = PCI_DEVFN(PCI_SLOT(hr_dev->pci_dev->devfn), 0);
    struct pci_dev *pdev = NULL;

    pdev = pci_get_domain_bus_and_slot(0, bus_num, dev_fn);
    if (pdev == NULL) {
        dev_err(hr_dev->dev, "get pci device bus and slot failed\n");
        return NULL;
    }

    return pdev;
}

static int hns_roce_get_peer_mem_info(struct hns_roce_dev *hr_dev, u64 *hpa,
    u64 *size, u64 *dpa)
{
    struct pci_dev *pdev = NULL;

    pdev = hns_roce_get_peer_mem_dev(hr_dev);
    if (pdev == NULL) {
        dev_err(hr_dev->dev, "get peer mem device failed\n");
        return -1;
    }

    *size = pci_resource_len(pdev, PCI_BAR_NUMBER);
    dev_notice(hr_dev->dev, "BAR 4/5 size:0x%llx.\n", *size);

    *hpa = pci_resource_start(pdev, PCI_BAR_NUMBER);
    pci_dev_put(pdev);
    *dpa = 0;

    return 0;
}

int hns_roce_init_gdr(struct hns_roce_dev *hr_dev)
{
    int ret;
    u64 hpa, dpa, size;

    ret = hns_roce_get_peer_mem_info(hr_dev, &hpa, &size, &dpa);
    if (ret) {
        dev_warn(hr_dev->dev, "get peer mem info failed, ret:0x%x.\n", ret);
        return ret;
    }

    ret = hns_roce_config_atu_table(hr_dev, hpa, size, dpa);
    if (ret) {
        dev_warn(hr_dev->dev, "ATU is not available(%d).\n", ret);
    }
    return ret;
}

static int hns_roce_config_so_reg(struct hns_roce_dev *hr_dev)
{
    struct hns_roce_cmq_desc desc;
    hns_roce_cmq_setup_basic_desc(&desc, HNS_ROCE_OPC_STRONG_ORDER_RES, false);
    /** Configuring the RoCE Order Preserving Register */
    desc.data[0] = cpu_to_le32(0x1);
    return hns_roce_cmq_send(hr_dev, &desc, 1);
}

int hns_roce_init_so_reg(struct hns_roce_dev *hr_dev)
{
    return hns_roce_config_so_reg(hr_dev);
}

int hns_roce_v2_uar_init(struct hns_roce_dev *hr_dev)
{
    struct hns_roce_v2_priv *priv = NULL;
    struct hns_roce_buf_list *uar = NULL;
    struct device *dev = NULL;
    int ret;
    if (hr_dev == NULL) {
        pr_err("hns3: param is NULL\n");
        return -EINVAL;
    }
    priv = hr_dev->priv;
    uar = &priv->uar;
    dev = &hr_dev->pci_dev->dev;
    uar->buf = dma_alloc_coherent(dev, HNS_ROCE_V2_UAR_BUF_SIZE, &uar->map,
                                  GFP_KERNEL | __GFP_NOWARN);
    if (uar->buf == NULL) {
        dev_err(hr_dev->dev, "dma_alloc_coherent hns_roce buf_list buff failed\n");
        return -ENOMEM;
    }

    ret = memset_s(uar->buf, HNS_ROCE_V2_UAR_BUF_SIZE, 0, HNS_ROCE_V2_UAR_BUF_SIZE);
    if (ret) {
        dev_err(dev, "%s(%d) memset_s failed, ret[%d]\n", __func__, __LINE__, ret);
        dma_free_coherent(dev, HNS_ROCE_V2_UAR_BUF_SIZE, uar->buf, uar->map);
        uar->buf = NULL;
        return ret;
    }

    hr_dev->uar2_dma_addr = uar->map;
    hr_dev->uar2_size = HNS_ROCE_V2_UAR_BUF_SIZE;

    return 0;
}

void hns_roce_v2_uar_free(struct hns_roce_dev *hr_dev)
{
    struct hns_roce_v2_priv *priv = hr_dev->priv;
    struct hns_roce_buf_list *uar = &priv->uar;
    struct device *dev = &hr_dev->pci_dev->dev;

    dma_free_coherent(dev, HNS_ROCE_V2_UAR_BUF_SIZE, uar->buf, uar->map);
    uar->buf = NULL;
    hr_dev->uar2_dma_addr = 0;
    hr_dev->uar2_size = 0;
}

int create_flush_workqueue(struct hns_roce_dev *hr_dev)
{
    int ret;
    struct device *dev = hr_dev->dev;
    char queue_name[QUEUE_NUM];
    static int device_id = 0;
    u8 index = 0;
    ret = snprintf_s(queue_name, sizeof(queue_name), QUEUE_NAME_CNT, "hns_roce_%d_flush_wq", device_id);
    HNS_ROCE_SEC_CHECK_NEG_RET_INT(dev, ret);

    device_id++;
    hr_dev->flush_workq =
        create_singlethread_workqueue(&queue_name[index]);
    if (hr_dev->flush_workq == NULL) {
        dev_err(dev, "failed to create flush workqueue!\n");
        return -ENOMEM;
    }

    return 0;
}

int hns_roce_v2_init_timer_table(struct hns_roce_dev *hr_dev)
{
    u32 qpc_count;
    u32 cqc_count;
    int ret;
    u32 i;

    /* Alloc memory for QPC Timer buffer space chunk */
    for (qpc_count = 0; qpc_count < hr_dev->caps.qpc_timer_bt_num;
            qpc_count++) {
        ret = hns_roce_table_get(hr_dev, &hr_dev->qpc_timer_table.table,
                                 qpc_count);
        if (ret) {
            dev_err(hr_dev->dev, "QPC Timer get failed\n");
            goto err_qpc_timer_failed;
        }
    }

    /* Alloc memory for CQC Timer buffer space chunk */
    for (cqc_count = 0; cqc_count < hr_dev->caps.cqc_timer_bt_num;
            cqc_count++) {
        ret = hns_roce_table_get(hr_dev, &hr_dev->cqc_timer_table.table,
                                 cqc_count);
        if (ret) {
            dev_err(hr_dev->dev, "CQC Timer get failed\n");
            goto err_cqc_timer_failed;
        }
    }

    return 0;

err_cqc_timer_failed:
    for (i = 0; i < cqc_count; i++) {
        hns_roce_table_put(hr_dev, &hr_dev->cqc_timer_table.table, i);
    }

err_qpc_timer_failed:
    for (i = 0; i < qpc_count; i++) {
        hns_roce_table_put(hr_dev, &hr_dev->qpc_timer_table.table, i);
    }

    return ret;
}


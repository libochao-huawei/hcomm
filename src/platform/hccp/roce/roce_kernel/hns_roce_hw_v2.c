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
#include <linux/etherdevice.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <net/addrconf.h>
#include <rdma/ib_addr.h>
#include <rdma/ib_cache.h>
#include <rdma/ib_umem.h>

#include "hnae3.h"
#include "hns_roce_common.h"
#include "hns_roce_device.h"
#include "hns_roce_cmd.h"
#include "hns_roce_hem.h"
#include "hns_roce_hw_v2_qp.h"
#include "hns_roce_hw_v2_eq.h"
#include "hns_roce_hw_v2_ops.h"
#include "hns_roce_peer.h"
#include "hns_roce_sm.h"
#include "hns_roce_notify.h"
#include "hns3_bbox.h"
#include "hns_roce_sec.h"
#include "hns_roce_hw_v2.h"

#ifdef CONFIG_INFINIBAND_HNS_TEST
#include "hns_hw_v2_test.h"
#endif

#ifdef DEFINE_HNS_LLT
#undef static
#define static

#undef inline
#define inline

#endif

static int g_loopback;
static int g_is_d;

static void hns_roce_handle_device_err(struct hns_roce_dev *hr_dev);

int hns_roce_get_g_is_d()
{
    return g_is_d;
}

STATIC int hns_roce_v2_init_tsq_tpq_link_table(struct hns_roce_dev *hr_dev, struct hns_roce_v2_priv *priv)
{
    int ret;
    /* TSQ includes SQ doorbell and ack doorbell */
    ret = hns_roce_init_link_table(hr_dev, TSQ_LINK_TABLE);
    if (ret) {
        dev_err(hr_dev->dev, "TSQ init failed, ret = %d.\n", ret);
        return ret;
    }

    ret = hns_roce_init_link_table(hr_dev, TPQ_LINK_TABLE);
    if (ret) {
        dev_err(hr_dev->dev, "TPQ init failed, ret = %d.\n", ret);
        goto err_tpq_init_failed;
    }
    return 0;

err_tpq_init_failed:
    hns_roce_free_link_table(hr_dev, &priv->tsq);
    return ret;
}

STATIC int hns_roce_v2_init(struct hns_roce_dev *hr_dev)
{
    struct hns_roce_v2_priv *priv = NULL;
    int ret;
    if (hr_dev == NULL) {
        pr_err("hns3: param is NULL\n");
        return -EINVAL;
    }
    priv = hr_dev->priv;
    ret = create_flush_workqueue(hr_dev);
    if (ret) {
        dev_err(hr_dev->dev, "create flush workqueue failed, ret = %d.\n", ret);
        return ret;
    }

    ret = hns_roce_v2_uar_init(hr_dev);
    if (ret) {
        dev_err(hr_dev->dev, "uar init failed, ret = %d.\n", ret);
        goto err_creat_flushwq_failed;
    }

    ret = hns_roce_init_so_reg(hr_dev);
    if (ret) {
        dev_err(hr_dev->dev, "strong order reg init failed, ret = %d.\n", ret);
        goto err_tsq_init_failed;
    }

    ret = hns_roce_v2_init_tsq_tpq_link_table(hr_dev, priv);
    if (ret) {
        dev_err(hr_dev->dev, "tsq_tpq_link_table init failed, ret = %d.\n", ret);
        goto err_tsq_init_failed;
    }

    ret = hns_roce_v2_init_timer_table(hr_dev);
    if (ret) {
        dev_err(hr_dev->dev, "TPQ init failed, ret = %d.\n", ret);
        goto err_timer_init_failed;
    }

    if (hr_dev->pci_dev->device == HNS_SDI_MODE_DEV_ID || hr_dev->pci_dev->device == HNS_SDI_100G_DEV_ID) {
        /* gdr is enhanced feature, do not affect normal function while failed  */
        (void)hns_roce_init_gdr(hr_dev);
    }

    return 0;

err_timer_init_failed:
    hns_roce_free_link_table(hr_dev, &priv->tpq);
    hns_roce_free_link_table(hr_dev, &priv->tsq);

err_tsq_init_failed:
    hns_roce_v2_uar_free(hr_dev);

err_creat_flushwq_failed:
    destroy_workqueue(hr_dev->flush_workq);
    return ret;
}

static void hns_roce_v2_exit(struct hns_roce_dev *hr_dev)
{
    struct hns_roce_v2_priv *priv = NULL;
    if (hr_dev == NULL) {
        pr_err("hns3: param is NULL\n");
        return;
    }
    priv = hr_dev->priv;
    if (hr_dev->pci_dev->revision == PCI_REVISION_ID_HIP08_B) {
        hns_roce_function_clear(hr_dev);
    }

    flush_workqueue(hr_dev->flush_workq);
    destroy_workqueue(hr_dev->flush_workq);
    hns_roce_free_link_table(hr_dev, &priv->tpq);
    hns_roce_free_link_table(hr_dev, &priv->tsq);
    hns_roce_v2_uar_free(hr_dev);
}

static u32 hns_roce_query_mbox_status(struct hns_roce_dev *hr_dev)
{
    struct hns_roce_cmq_desc desc;
    struct hns_roce_mbox_status *mb_st = (struct hns_roce_mbox_status *)desc.data;
    enum hns_roce_cmd_return_status status;

    hns_roce_cmq_setup_basic_desc(&desc, HNS_ROCE_OPC_QUERY_MB_ST, true);

    status = hns_roce_cmq_send(hr_dev, &desc, 1);
    HNS_ROCE_CMQ_SEND_RET_CHECK(status);

    return mb_st->mb_status_hw_run;
}

static int hns_roce_v2_cmd_pending(struct hns_roce_dev *hr_dev)
{
    u32 status = hns_roce_query_mbox_status(hr_dev);

    return status >> HNS_ROCE_HW_RUN_BIT_SHIFT;
}

static int hns_roce_v2_cmd_complete(struct hns_roce_dev *hr_dev)
{
    u32 status = hns_roce_query_mbox_status(hr_dev);

    return status & HNS_ROCE_HW_MB_STATUS_MASK;
}

static int hns_roce_mbox_post(struct hns_roce_dev *hr_dev, struct hns_roce_mbox_post_params mbx_pst_params)
{
    struct hns_roce_cmq_desc desc;
    struct hns_roce_post_mbox *mb = (struct hns_roce_post_mbox *)desc.data;
    struct hns_roce_qp *qp = NULL;
    unsigned long sq_flags;
    unsigned long rq_flags;
    int to_be_err_state = false;
    int ret;

    hns_roce_cmq_setup_basic_desc(&desc, HNS_ROCE_OPC_POST_MB, false);

    mb->in_param_l = cpu_to_le64(mbx_pst_params.in_param);
    mb->in_param_h = cpu_to_le64(mbx_pst_params.in_param) >> LOWBITS_WIDTH;
    mb->out_param_l = cpu_to_le64(mbx_pst_params.out_param);
    mb->out_param_h = cpu_to_le64(mbx_pst_params.out_param) >> LOWBITS_WIDTH;
    mb->cmd_tag = ((unsigned int)mbx_pst_params.in_modifier << HNS_ROCE_MB_TAG_S) | mbx_pst_params.op;
    mb->token_event_en = ((unsigned int)mbx_pst_params.event << HNS_ROCE_MB_EVENT_EN_S) | mbx_pst_params.token;

    spin_lock(&hr_dev->qp_table.lock);
    qp = __hns_roce_qp_lookup(hr_dev, mbx_pst_params.in_modifier);
    spin_unlock(&hr_dev->qp_table.lock);

    if (qp && !qp->ibqp.uobject &&
            ((unsigned int)qp->attr_mask & IB_QP_STATE) && qp->next_state == IB_QPS_ERR) {
#ifndef DEFINE_HNS_LLT
        spin_lock_irqsave(&qp->sq.lock, sq_flags);
        spin_lock_irqsave(&qp->rq.lock, rq_flags);
        to_be_err_state = true;
#endif
    }

    ret = hns_roce_cmq_send(hr_dev, &desc, 1);

    if (to_be_err_state) {
#ifndef DEFINE_HNS_LLT
        spin_unlock_irqrestore(&qp->rq.lock, rq_flags);
        spin_unlock_irqrestore(&qp->sq.lock, sq_flags);
#endif
    }

    return ret;
}

STATIC int hns_roce_v2_post_mbox(struct hns_roce_dev *hr_dev,
    struct hns_roce_mbox_post_params mbx_pst_params)
{
    struct device *dev = NULL;
    unsigned long end;
    int ret;
    if (hr_dev == NULL) {
        pr_err("hns3: param is NULL\n");
        return -EINVAL;
    }
    dev = hr_dev->dev;
    end = msecs_to_jiffies(HNS_ROCE_V2_GO_BIT_TIMEOUT_MSECS) + jiffies;
    while (hns_roce_v2_cmd_pending(hr_dev)) {
        if (time_after(jiffies, end)) {
#ifndef DEFINE_HNS_LLT
            dev_dbg(dev, "jiffies=%d end=%d\n", (int)jiffies, (int)end);
            return -EAGAIN;
#endif
        }
        cond_resched();
    }

    ret = hns_roce_mbox_post(hr_dev, mbx_pst_params);
    if (ret) {
        dev_err(dev, "Post mailbox fail(%d)\n", ret);
    }

    return ret;
}

static int hns_roce_v2_chk_mbox(struct hns_roce_dev *hr_dev,
    unsigned long timeout)
{
    struct device *dev = NULL;
    unsigned long end;
    u32 status;
    if (hr_dev == NULL) {
        pr_err("hns3: param is NULL\n");
        return -EINVAL;
    }
    dev = hr_dev->dev;
    end = msecs_to_jiffies(timeout) + jiffies;
    while (hns_roce_v2_cmd_pending(hr_dev) && time_before(jiffies, end)) {
        cond_resched();
    }

    if (hns_roce_v2_cmd_pending(hr_dev)) {
#ifndef DEFINE_HNS_LLT
        dev_err(dev, "[cmd_poll]hw run cmd TIMEDOUT!\n");
        return -ETIMEDOUT;
#endif
    }

    status = hns_roce_v2_cmd_complete(hr_dev);
    if (unlikely(status != HNS_ROCE_CMD_SUCCESS)) {
        if (status == CMD_RST_PRC_EBUSY) {
            return status;
        }

        dev_err(dev, "mailbox status 0x%x!\n", status);
        return -EBUSY;
    }

    return 0;
}

static int hns_roce_config_sgid_table(struct hns_roce_dev *hr_dev,
    int gid_index, const union ib_gid *gid,
    enum hns_roce_sgid_type sgid_type)
{
    struct hns_roce_cmq_desc desc;
    struct hns_roce_cfg_sgid_tb *sgid_tb =
        (struct hns_roce_cfg_sgid_tb *)desc.data;
    u32 *p = NULL;

    hns_roce_cmq_setup_basic_desc(&desc, HNS_ROCE_OPC_CFG_SGID_TB, false);

    roce_set_field(sgid_tb->table_idx_rsv,
                   CFG_SGID_TB_TABLE_IDX_M,
                   CFG_SGID_TB_TABLE_IDX_S, gid_index);
    roce_set_field(sgid_tb->vf_sgid_type_rsv,
                   CFG_SGID_TB_VF_SGID_TYPE_M,
                   CFG_SGID_TB_VF_SGID_TYPE_S, sgid_type);

    p = (u32 *)&gid->raw[VF_SGIDL];
    sgid_tb->vf_sgid_l = cpu_to_le32(*p);

    p = (u32 *)&gid->raw[VF_SGIDML];
    sgid_tb->vf_sgid_ml = cpu_to_le32(*p);

    p = (u32 *)&gid->raw[VF_SGIDMH];
    sgid_tb->vf_sgid_mh = cpu_to_le32(*p);

    p = (u32 *)&gid->raw[VF_SGIDH];
    sgid_tb->vf_sgid_h = cpu_to_le32(*p);

    return hns_roce_cmq_send(hr_dev, &desc, 1);
}

static int hns_roce_v2_set_gid(struct hns_roce_dev *hr_dev, u8 port,
    int gid_index, const union ib_gid *gid,
    const struct ib_gid_attr *attr)
{
    enum hns_roce_sgid_type sgid_type = GID_TYPE_FLAG_ROCE_V1;
    int ret;

    if ((hr_dev == NULL) || (gid == NULL) || (attr == NULL)) {
        pr_err("hns3: invalid param, hr_dev[%pK], gid[%pK], attr[%pK] \n", hr_dev, gid, attr);
        return -EINVAL;
    }

    if (attr->gid_type == IB_GID_TYPE_ROCE) {
        sgid_type = GID_TYPE_FLAG_ROCE_V1;
    }

    if (attr->gid_type == IB_GID_TYPE_ROCE_UDP_ENCAP) {
        if (ipv6_addr_v4mapped((void *)gid)) {
#ifndef DEFINE_HNS_LLT
            sgid_type = GID_TYPE_FLAG_ROCE_V2_IPV4;
#endif
        } else {
            sgid_type = GID_TYPE_FLAG_ROCE_V2_IPV6;
        }
    }

    ret = hns_roce_config_sgid_table(hr_dev, gid_index, gid, sgid_type);
    if (ret) {
        dev_err(hr_dev->dev, "Configure sgid table failed(%d)!\n", ret);
    }

    return ret;
}

static int hns_roce_v2_set_mac(struct hns_roce_dev *hr_dev, u8 phy_port,
                               u8 *addr)
{
    struct hns_roce_cmq_desc desc;
    struct hns_roce_cfg_smac_tb *smac_tb =
        (struct hns_roce_cfg_smac_tb *)desc.data;
    u16 reg_smac_h;
    u32 reg_smac_l;
    int ret;
    if (hr_dev == NULL || addr == NULL) {
        pr_err("hns3: invalid param, hr_dev[%pK], addr[%pK] \n", hr_dev, addr);
        return -EINVAL;
    }
    hns_roce_cmq_setup_basic_desc(&desc, HNS_ROCE_OPC_CFG_SMAC_TB, false);

    reg_smac_l = *(u32 *)(&addr[0]);
    reg_smac_h = *(u16 *)(&addr[REG_SMACH]);

    ret = memset_s(smac_tb, sizeof(*smac_tb), 0, sizeof(*smac_tb));
    HNS_ROCE_SEC_CHECK_RET_INT(hr_dev->dev, ret);

    roce_set_field(smac_tb->tb_idx_rsv,
                   CFG_SMAC_TB_IDX_M,
                   CFG_SMAC_TB_IDX_S, phy_port);
    roce_set_field(smac_tb->vf_smac_h_rsv,
                   CFG_SMAC_TB_VF_SMAC_H_M,
                   CFG_SMAC_TB_VF_SMAC_H_S, reg_smac_h);
    smac_tb->vf_smac_l = reg_smac_l;

    return hns_roce_cmq_send(hr_dev, &desc, 1);
}

static int set_mtpt_pbl(struct hns_roce_v2_mpt_entry *mpt_entry,
                        struct hns_roce_mr *mr)
{
    struct scatterlist *sg = NULL;
    u64 page_addr;
    u64 *pages = NULL;
    int entry;
    int len;
    int i;
    int j;

    mpt_entry->pbl_size = cpu_to_le32(mr->pbl_size);
    mpt_entry->pbl_ba_l = cpu_to_le32(lower_32_bits(mr->pbl_ba >> PBLBA_ALIGN_OFFSET));
    roce_set_field(mpt_entry->byte_48_mode_ba, V2_MPT_BYTE_48_PBL_BA_H_M, V2_MPT_BYTE_48_PBL_BA_H_S,
                   upper_32_bits(mr->pbl_ba >> PBLBA_ALIGN_OFFSET));

    pages = (u64 *)(uintptr_t)__get_free_page(GFP_KERNEL);
    if (pages == NULL) {
        pr_err("hns3: get free page failed\n");
        return -ENOMEM;
    }

    i = 0;
    for_each_sg(mr->umem->sg_head.sgl, sg, mr->umem->nmap, entry) {
        len = sg_dma_len(sg) >> (mr->hr_umem.page_shift);
        for (j = 0; j < len; ++j) {
            page_addr = sg_dma_address(sg) + ((unsigned int)j << mr->hr_umem.page_shift);
            pages[i] = page_addr >> PAGESZ_OFFSET;
            /* Record the first 2 entry directly to MTPT table */
            if (i >= HNS_ROCE_V2_MAX_INNER_MTPT_NUM - 1) {
                goto found;
            }
            i++;
        }
    }
found:
    mpt_entry->pa0_l = cpu_to_le32(lower_32_bits(pages[0]));
    roce_set_field(mpt_entry->byte_56_pa0_h, V2_MPT_BYTE_56_PA0_H_M,
                   V2_MPT_BYTE_56_PA0_H_S, upper_32_bits(pages[0]));

    mpt_entry->pa1_l = cpu_to_le32(lower_32_bits(pages[1]));
    roce_set_field(mpt_entry->byte_64_buf_pa1, V2_MPT_BYTE_64_PA1_H_M,
                   V2_MPT_BYTE_64_PA1_H_S, upper_32_bits(pages[1]));
    roce_set_field(mpt_entry->byte_64_buf_pa1,
                   V2_MPT_BYTE_64_PBL_BUF_PG_SZ_M,
                   V2_MPT_BYTE_64_PBL_BUF_PG_SZ_S,
                   mr->pbl_buf_pg_sz + mr->hr_umem.page_shift - PAGE_SHIFT);

    free_page((uintptr_t)pages);

    return 0;
}

static void hns_roce_v2_set_mpt_entry(struct hns_roce_v2_mpt_entry *mpt_entry,
    struct hns_roce_mr *mr)
{
    roce_set_field(mpt_entry->byte_4_pd_hop_st,
                   V2_MPT_BYTE_4_MPT_ST_M,
                   V2_MPT_BYTE_4_MPT_ST_S,
                   V2_MPT_ST_VALID);
    roce_set_field(mpt_entry->byte_4_pd_hop_st,
                   V2_MPT_BYTE_4_PBL_HOP_NUM_M,
                   V2_MPT_BYTE_4_PBL_HOP_NUM_S,
                   mr->pbl_hop_num == HNS_ROCE_HOP_NUM_0 ? 0 : mr->pbl_hop_num);
    roce_set_field(mpt_entry->byte_4_pd_hop_st,
                   V2_MPT_BYTE_4_PBL_BA_PG_SZ_M,
                   V2_MPT_BYTE_4_PBL_BA_PG_SZ_S,
                   mr->pbl_ba_pg_sz + PG_SHIFT_OFFSET);
    roce_set_field(mpt_entry->byte_4_pd_hop_st,
                   V2_MPT_BYTE_4_PD_M,
                   V2_MPT_BYTE_4_PD_S,
                   mr->pd);

    roce_set_bit(mpt_entry->byte_8_mw_cnt_en, V2_MPT_BYTE_8_RA_EN_S, 0);
    roce_set_bit(mpt_entry->byte_8_mw_cnt_en, V2_MPT_BYTE_8_R_INV_EN_S, 0);
    roce_set_bit(mpt_entry->byte_8_mw_cnt_en, V2_MPT_BYTE_8_L_INV_EN_S, 1);
    roce_set_bit(mpt_entry->byte_8_mw_cnt_en, V2_MPT_BYTE_8_BIND_EN_S,
                 (mr->access & IB_ACCESS_MW_BIND ? 1 : 0));
    roce_set_bit(mpt_entry->byte_8_mw_cnt_en, V2_MPT_BYTE_8_ATOMIC_EN_S,
                 mr->access & IB_ACCESS_REMOTE_ATOMIC ? 1 : 0);
    roce_set_bit(mpt_entry->byte_8_mw_cnt_en, V2_MPT_BYTE_8_RR_EN_S,
                 (mr->access & IB_ACCESS_REMOTE_READ ? 1 : 0));
    roce_set_bit(mpt_entry->byte_8_mw_cnt_en, V2_MPT_BYTE_8_RW_EN_S,
                 (mr->access & IB_ACCESS_REMOTE_WRITE ? 1 : 0));
    roce_set_bit(mpt_entry->byte_8_mw_cnt_en, V2_MPT_BYTE_8_LW_EN_S,
                 (mr->access & IB_ACCESS_LOCAL_WRITE ? 1 : 0));

    roce_set_bit(mpt_entry->byte_12_mw_pa, V2_MPT_BYTE_12_PA_S,
                 mr->type == MR_TYPE_MR ? 0 : 1);
    roce_set_bit(mpt_entry->byte_12_mw_pa, V2_MPT_BYTE_12_INNER_PA_VLD_S,
                 1);

    mpt_entry->len_l = cpu_to_le32(lower_32_bits(mr->size));
    mpt_entry->len_h = cpu_to_le32(upper_32_bits(mr->size));
    mpt_entry->lkey = cpu_to_le32(mr->key);
    mpt_entry->va_l = cpu_to_le32(lower_32_bits(mr->iova));
    mpt_entry->va_h = cpu_to_le32(upper_32_bits(mr->iova));
}


static int hns_roce_v2_write_mtpt(void *mb_buf, struct hns_roce_mr *mr,
    unsigned long mtpt_idx)
{
    struct hns_roce_v2_mpt_entry *mpt_entry;
    int ret;

    mpt_entry = mb_buf;
    ret = memset_s(mpt_entry, sizeof(*mpt_entry), 0, sizeof(*mpt_entry));
    HNS_ROCE_SEC_CHECK_RET_INT_WITHOUT_DEV(ret);

    hns_roce_v2_set_mpt_entry(mpt_entry, mr);

    if (mr->type == MR_TYPE_DMA) {
        return 0;
    }

    ret = set_mtpt_pbl(mpt_entry, mr);
    if (ret) {
        pr_err("set_mtpt_pbl err, ret:%d\n", ret);
    }

    return ret;
}

static int hns_roce_v2_rereg_write_mtpt(struct hns_roce_dev *hr_dev,
                                        struct hns_roce_mr *mr, struct rreg_wrt_mtpt_info rreg_info, void *mb_buf)
{
    struct hns_roce_v2_mpt_entry *mpt_entry = mb_buf;
    int ret = 0;

    roce_set_field(mpt_entry->byte_4_pd_hop_st, V2_MPT_BYTE_4_MPT_ST_M,
                   V2_MPT_BYTE_4_MPT_ST_S, V2_MPT_ST_VALID);

    if ((unsigned int)rreg_info.flags & IB_MR_REREG_PD) {
        roce_set_field(mpt_entry->byte_4_pd_hop_st, V2_MPT_BYTE_4_PD_M,
                       V2_MPT_BYTE_4_PD_S, rreg_info.pdn);
        mr->pd = rreg_info.pdn;
    }

    if ((unsigned int)rreg_info.flags & IB_MR_REREG_ACCESS) {
        roce_set_bit(mpt_entry->byte_8_mw_cnt_en,
                     V2_MPT_BYTE_8_BIND_EN_S,
                     ((unsigned int)rreg_info.mr_access_flags & IB_ACCESS_MW_BIND ? 1 : 0));
        roce_set_bit(mpt_entry->byte_8_mw_cnt_en,
                     V2_MPT_BYTE_8_ATOMIC_EN_S,
                     (unsigned int)rreg_info.mr_access_flags & IB_ACCESS_REMOTE_ATOMIC ? 1 : 0);
        roce_set_bit(mpt_entry->byte_8_mw_cnt_en, V2_MPT_BYTE_8_RR_EN_S,
                     (unsigned int)rreg_info.mr_access_flags & IB_ACCESS_REMOTE_READ ? 1 : 0);
        roce_set_bit(mpt_entry->byte_8_mw_cnt_en, V2_MPT_BYTE_8_RW_EN_S,
                     (unsigned int)rreg_info.mr_access_flags & IB_ACCESS_REMOTE_WRITE ? 1 : 0);
        roce_set_bit(mpt_entry->byte_8_mw_cnt_en, V2_MPT_BYTE_8_LW_EN_S,
                     (unsigned int)rreg_info.mr_access_flags & IB_ACCESS_LOCAL_WRITE ? 1 : 0);
    }

    if ((unsigned int)rreg_info.flags & IB_MR_REREG_TRANS) {
        mpt_entry->va_l = cpu_to_le32(lower_32_bits(rreg_info.iova));
        mpt_entry->va_h = cpu_to_le32(upper_32_bits(rreg_info.iova));
        mpt_entry->len_l = cpu_to_le32(lower_32_bits(rreg_info.size));
        mpt_entry->len_h = cpu_to_le32(upper_32_bits(rreg_info.size));

        mr->iova = rreg_info.iova;
        mr->size = rreg_info.size;

        ret = set_mtpt_pbl(mpt_entry, mr);
        if (ret) {
            pr_err("set_mtpt_pbl err, ret:%d\n", ret);
        }
    }

    return ret;
}

static int hns_roce_v2_frmr_write_mtpt(void *mb_buf, struct hns_roce_mr *mr)
{
    struct hns_roce_v2_mpt_entry *mpt_entry;
    int ret;

    mpt_entry = mb_buf;
    ret = memset_s(mpt_entry, sizeof(*mpt_entry), 0, sizeof(*mpt_entry));
    HNS_ROCE_SEC_CHECK_RET_INT_WITHOUT_DEV(ret);

    roce_set_field(mpt_entry->byte_4_pd_hop_st, V2_MPT_BYTE_4_MPT_ST_M,
                   V2_MPT_BYTE_4_MPT_ST_S, V2_MPT_ST_FREE);
    roce_set_field(mpt_entry->byte_4_pd_hop_st, V2_MPT_BYTE_4_PBL_HOP_NUM_M,
                   V2_MPT_BYTE_4_PBL_HOP_NUM_S, 1);
    roce_set_field(mpt_entry->byte_4_pd_hop_st,
                   V2_MPT_BYTE_4_PBL_BA_PG_SZ_M,
                   V2_MPT_BYTE_4_PBL_BA_PG_SZ_S,
                   mr->pbl_ba_pg_sz + PG_SHIFT_OFFSET);
    roce_set_field(mpt_entry->byte_4_pd_hop_st, V2_MPT_BYTE_4_PD_M,
                   V2_MPT_BYTE_4_PD_S, mr->pd);
    mpt_entry->byte_4_pd_hop_st = cpu_to_le32(mpt_entry->byte_4_pd_hop_st);

    roce_set_bit(mpt_entry->byte_8_mw_cnt_en, V2_MPT_BYTE_8_RA_EN_S, 1);
    roce_set_bit(mpt_entry->byte_8_mw_cnt_en, V2_MPT_BYTE_8_R_INV_EN_S, 1);
    roce_set_bit(mpt_entry->byte_8_mw_cnt_en, V2_MPT_BYTE_8_L_INV_EN_S, 1);

    roce_set_bit(mpt_entry->byte_12_mw_pa, V2_MPT_BYTE_12_FRE_S, 1);
    roce_set_bit(mpt_entry->byte_12_mw_pa, V2_MPT_BYTE_12_PA_S, 0);
    roce_set_bit(mpt_entry->byte_12_mw_pa, V2_MPT_BYTE_12_MR_MW_S, 0);
    roce_set_bit(mpt_entry->byte_12_mw_pa, V2_MPT_BYTE_12_BPD_S, 1);

    mpt_entry->pbl_size = cpu_to_le32(mr->pbl_size);

    mpt_entry->pbl_ba_l = cpu_to_le32(lower_32_bits(mr->pbl_ba >> PBLBA_ALIGN_OFFSET));
    roce_set_field(mpt_entry->byte_48_mode_ba,
                   V2_MPT_BYTE_48_PBL_BA_H_M,
                   V2_MPT_BYTE_48_PBL_BA_H_S,
                   upper_32_bits(mr->pbl_ba >> PBLBA_ALIGN_OFFSET));

    roce_set_field(mpt_entry->byte_64_buf_pa1,
                   V2_MPT_BYTE_64_PBL_BUF_PG_SZ_M,
                   V2_MPT_BYTE_64_PBL_BUF_PG_SZ_S,
                   mr->pbl_buf_pg_sz + PG_SHIFT_OFFSET);

    return 0;
}

static int hns_roce_v2_mw_write_mtpt(void *mb_buf, struct hns_roce_mw *mw)
{
    struct hns_roce_v2_mpt_entry *mpt_entry;
    int ret;

    mpt_entry = mb_buf;
    ret = memset_s(mpt_entry, sizeof(*mpt_entry), 0, sizeof(*mpt_entry));
    HNS_ROCE_SEC_CHECK_RET_INT_WITHOUT_DEV(ret);

    roce_set_field(mpt_entry->byte_4_pd_hop_st, V2_MPT_BYTE_4_MPT_ST_M,
                   V2_MPT_BYTE_4_MPT_ST_S, V2_MPT_ST_FREE);
    roce_set_field(mpt_entry->byte_4_pd_hop_st, V2_MPT_BYTE_4_PD_M,
                   V2_MPT_BYTE_4_PD_S, mw->pdn);
    roce_set_field(mpt_entry->byte_4_pd_hop_st, V2_MPT_BYTE_4_PBL_HOP_NUM_M,
                   V2_MPT_BYTE_4_PBL_HOP_NUM_S, mw->pbl_hop_num ==
                   HNS_ROCE_HOP_NUM_0 ? 0 : mw->pbl_hop_num);
    roce_set_field(mpt_entry->byte_4_pd_hop_st,
                   V2_MPT_BYTE_4_PBL_BA_PG_SZ_M,
                   V2_MPT_BYTE_4_PBL_BA_PG_SZ_S,
                   mw->pbl_ba_pg_sz + PG_SHIFT_OFFSET);

    roce_set_bit(mpt_entry->byte_8_mw_cnt_en, V2_MPT_BYTE_8_R_INV_EN_S, 1);
    roce_set_bit(mpt_entry->byte_8_mw_cnt_en, V2_MPT_BYTE_8_L_INV_EN_S, 1);

    roce_set_bit(mpt_entry->byte_12_mw_pa, V2_MPT_BYTE_12_PA_S, 0);
    roce_set_bit(mpt_entry->byte_12_mw_pa, V2_MPT_BYTE_12_MR_MW_S, 1);
    roce_set_bit(mpt_entry->byte_12_mw_pa, V2_MPT_BYTE_12_BPD_S, 1);
    roce_set_bit(mpt_entry->byte_12_mw_pa, V2_MPT_BYTE_12_BQP_S,
                 mw->ibmw.type == IB_MW_TYPE_1 ? 0 : 1);

    roce_set_field(mpt_entry->byte_64_buf_pa1,
                   V2_MPT_BYTE_64_PBL_BUF_PG_SZ_M,
                   V2_MPT_BYTE_64_PBL_BUF_PG_SZ_S,
                   mw->pbl_buf_pg_sz + PG_SHIFT_OFFSET);

    mpt_entry->lkey = cpu_to_le32(mw->rkey);

    return 0;
}

void *get_cqe_v2(struct hns_roce_cq *hr_cq, int n)
{
    return hns_roce_buf_offset(&hr_cq->hr_buf.hr_buf, n * HNS_ROCE_V2_CQE_ENTRY_SIZE);
}

void *get_sw_cqe_v2(struct hns_roce_cq *hr_cq, int n)
{
    struct hns_roce_v2_cqe *cqe = get_cqe_v2(hr_cq, (unsigned int)n & (unsigned int)hr_cq->ib_cq.cqe);

    /* Get cqe when Owner bit is Conversely with the MSB of cons_idx */
    return (roce_get_bit(cqe->byte_4, V2_CQE_BYTE_4_OWNER_S) ^
            (unsigned int)!!((unsigned int)n & (unsigned int)(hr_cq->ib_cq.cqe + 1))) ? cqe : NULL;
}

static struct hns_roce_v2_cqe *next_cqe_sw_v2(struct hns_roce_cq *hr_cq)
{
    return get_sw_cqe_v2(hr_cq, hr_cq->cons_index);
}

static void *get_srq_wqe(struct hns_roce_srq *srq, int n)
{
    return hns_roce_buf_offset(&srq->buf, (unsigned int)n << (unsigned int)srq->wqe_shift);
}

void hns_roce_free_srq_wqe(struct hns_roce_srq *srq, int wqe_index)
{
    u32 bitmap_num;
    int bit_num;

    /* always called with interrupts disabled. */
    spin_lock(&srq->lock);

    bitmap_num = wqe_index / BITS_PER_LONG_LONG;
    bit_num = wqe_index % BITS_PER_LONG_LONG;

    if (bitmap_num < srq->idx_que.bitmap_len) {
        srq->idx_que.bitmap[bitmap_num] |= (unsigned int)(1ULL << (unsigned int)bit_num);
        srq->tail++;
    } else {
        return;
    }

    spin_unlock(&srq->lock);
}

static void hns_roce_v2_set_cq_context(struct hns_roce_dev *hr_dev,
    struct hns_roce_v2_cq_context *cq_context, struct hns_roce_cq *hr_cq,
    u64 *mtts, struct hns_roce_wrtcqc_info wrt_cqc_info)
{
    roce_set_field(cq_context->byte_4_pg_ceqn, V2_CQC_BYTE_4_CQ_ST_M,
                   V2_CQC_BYTE_4_CQ_ST_S, V2_CQ_STATE_VALID);
    roce_set_field(cq_context->byte_4_pg_ceqn, V2_CQC_BYTE_4_ARM_ST_M,
                   V2_CQC_BYTE_4_ARM_ST_S, REG_NXT_CEQE);
    roce_set_field(cq_context->byte_4_pg_ceqn, V2_CQC_BYTE_4_SHIFT_M,
                   V2_CQC_BYTE_4_SHIFT_S, ilog2((unsigned int)wrt_cqc_info.nent));
    roce_set_field(cq_context->byte_4_pg_ceqn, V2_CQC_BYTE_4_CEQN_M,
                   V2_CQC_BYTE_4_CEQN_S, wrt_cqc_info.vector);
    cq_context->byte_4_pg_ceqn = cpu_to_le32(cq_context->byte_4_pg_ceqn);

    roce_set_field(cq_context->byte_8_cqn, V2_CQC_BYTE_8_CQN_M,
                   V2_CQC_BYTE_8_CQN_S, hr_cq->cqn);

    cq_context->cqe_cur_blk_addr = (u32)(mtts[0] >> PAGE_ADDR_SHIFT);
    cq_context->cqe_cur_blk_addr =
        cpu_to_le32(cq_context->cqe_cur_blk_addr);

    roce_set_field(cq_context->byte_16_hop_addr,
                   V2_CQC_BYTE_16_CQE_CUR_BLK_ADDR_M,
                   V2_CQC_BYTE_16_CQE_CUR_BLK_ADDR_S,
                   cpu_to_le32((mtts[0]) >> (LOWBITS_WIDTH + PAGE_ADDR_SHIFT)));
    roce_set_field(cq_context->byte_16_hop_addr,
                   V2_CQC_BYTE_16_CQE_HOP_NUM_M,
                   V2_CQC_BYTE_16_CQE_HOP_NUM_S, hr_dev->caps.cqe_hop_num ==
                   HNS_ROCE_HOP_NUM_0 ? 0 : hr_dev->caps.cqe_hop_num);

    cq_context->cqe_nxt_blk_addr = (u32)(mtts[1] >> PAGE_ADDR_SHIFT);
    roce_set_field(cq_context->byte_24_pgsz_addr,
                   V2_CQC_BYTE_24_CQE_NXT_BLK_ADDR_M,
                   V2_CQC_BYTE_24_CQE_NXT_BLK_ADDR_S,
                   cpu_to_le32((mtts[1]) >> (LOWBITS_WIDTH + PAGE_ADDR_SHIFT)));
    roce_set_field(cq_context->byte_24_pgsz_addr,
                   V2_CQC_BYTE_24_CQE_BA_PG_SZ_M,
                   V2_CQC_BYTE_24_CQE_BA_PG_SZ_S,
                   hr_dev->caps.cqe_ba_pg_sz + PG_SHIFT_OFFSET);
    roce_set_field(cq_context->byte_24_pgsz_addr,
                   V2_CQC_BYTE_24_CQE_BUF_PG_SZ_M,
                   V2_CQC_BYTE_24_CQE_BUF_PG_SZ_S,
                   hr_dev->caps.cqe_buf_pg_sz + PG_SHIFT_OFFSET);

    cq_context->cqe_ba = (u32)(wrt_cqc_info.dma_handle >> PBLBA_ALIGN_OFFSET);

    roce_set_field(cq_context->byte_40_cqe_ba, V2_CQC_BYTE_40_CQE_BA_M,
                   V2_CQC_BYTE_40_CQE_BA_S, (wrt_cqc_info.dma_handle >> (LOWBITS_WIDTH + PBLBA_ALIGN_OFFSET)));
}

static int hns_roce_v2_set_cqc_notify_ba(struct hns_roce_dev *hr_dev, struct hns_roce_v2_cq_context *cq_context)
{
    u64 notify_phy_addr, size;
    u32 notify_phy_addr_h, notify_phy_addr_l;
    int ret;

    ret = hns_roce_get_notify_base_addr(hr_dev, &notify_phy_addr, &size);
    if (ret) {
        dev_err(hr_dev->dev, "Get notify base addr failed when setting cqc, %d\n", ret);
        return ret;
    }

    notify_phy_addr_l = (u32)(notify_phy_addr);
    notify_phy_addr_h = (u32)(notify_phy_addr >> NOTIFY_BASE_ADDR_SHIFT_32);

    roce_set_field(cq_context->byte_4_pg_ceqn, V2_CQC_BYTE_4_NOTIFY_BASE_ADDR_0_7_M,
                   V2_CQC_BYTE_4_NOTIFY_BASE_ADDR_0_7_S, notify_phy_addr_l & 0xFF);
    roce_set_field(cq_context->byte_16_hop_addr, V2_CQC_BYTE_16_NOTIFY_BASE_ADDR_8_17_M,
                   V2_CQC_BYTE_16_NOTIFY_BASE_ADDR_8_17_S, (notify_phy_addr_l >> NOTIFY_BASE_ADDR_SHIFT_8) & 0x3FF);
    roce_set_field(cq_context->byte_24_pgsz_addr, V2_CQC_BYTE_24_NOTIFY_BASE_ADDR_18_21_M,
                   V2_CQC_BYTE_24_NOTIFY_BASE_ADDR_18_21_S, (notify_phy_addr_l >> NOTIFY_BASE_ADDR_SHIFT_18) & 0xF);
    roce_set_field(cq_context->byte_28_cq_pi, V2_CQC_BYTE_28_NOTIFY_BASE_ADDR_22_29_M,
                   V2_CQC_BYTE_28_NOTIFY_BASE_ADDR_22_29_S, ((notify_phy_addr_l >> NOTIFY_BASE_ADDR_SHIFT_22) & 0xFF));
    roce_set_field(cq_context->byte_32_cq_ci, V2_CQC_BYTE_32_NOTIFY_BASE_ADDR_30_37_M,
                   V2_CQC_BYTE_32_NOTIFY_BASE_ADDR_30_37_S,
                   ((notify_phy_addr_l >> NOTIFY_BASE_ADDR_SHIFT_30) & 0x3) | (notify_phy_addr_h & 0x3F));
    roce_set_field(cq_context->byte_52_cqe_cnt, V2_CQC_BYTE_52_NOTIFY_BASE_ADDR_38_45_M,
                   V2_CQC_BYTE_52_NOTIFY_BASE_ADDR_38_45_S,
                   (notify_phy_addr_h >> NOTIFY_BASE_ADDR_SHIFT_7) & 0xFF);
    roce_set_field(cq_context->byte_64_se_cqe_idx, V2_CQC_BYTE_64_NOTIFY_BASE_ADDR_46_51_M,
                   V2_CQC_BYTE_64_NOTIFY_BASE_ADDR_46_51_S,
                   (notify_phy_addr_h >> NOTIFY_BASE_ADDR_SHIFT_15) & 0x3F);
    roce_set_bit(cq_context->byte_64_se_cqe_idx, V2_CQC_BYTE_64_NOTIFY_EN_S, 1);

    return 0;
}

static void hns_roce_v2_write_cqc(struct hns_roce_dev *hr_dev,
                                  struct hns_roce_cq *hr_cq, void *mb_buf,
                                  u64 *mtts, struct hns_roce_wrtcqc_info wrt_cqc_info)
{
    struct hns_roce_v2_cq_context *cq_context = NULL;
    unsigned int cq_period = HNS_ROCE_V2_CQ_DEFAULT_INTERVAL;
    unsigned int cq_max_cnt = HNS_ROCE_V2_CQ_DEFAULT_BURST_NUM;
    int ret;

#ifdef CONFIG_INFINIBAND_HNS_TEST
    test_set_cqc_param(&cq_period, &cq_max_cnt);
#endif
    cq_context = mb_buf;
    ret = memset_s(cq_context, sizeof(*cq_context), 0, sizeof(*cq_context));
    HNS_ROCE_SEC_CHECK_RET_VOID(hr_dev->dev, ret);

    hns_roce_v2_set_cq_context(hr_dev, cq_context, hr_cq, mtts, wrt_cqc_info);

    if (hr_cq->db_en)
        roce_set_bit(cq_context->byte_44_db_record, V2_CQC_BYTE_44_DB_RECORD_EN_S, 1);

    roce_set_field(cq_context->byte_44_db_record,
                   V2_CQC_BYTE_44_DB_RECORD_ADDR_M,
                   V2_CQC_BYTE_44_DB_RECORD_ADDR_S,
                   ((u32)hr_cq->db.dma) >> 1);
    cq_context->db_record_addr = hr_cq->db.dma >> LOWBITS_WIDTH;

    roce_set_field(cq_context->byte_56_cqe_period_maxcnt,
                   V2_CQC_BYTE_56_CQ_MAX_CNT_M,
                   V2_CQC_BYTE_56_CQ_MAX_CNT_S, cq_max_cnt);
    roce_set_field(cq_context->byte_56_cqe_period_maxcnt,
                   V2_CQC_BYTE_56_CQ_PERIOD_M,
                   V2_CQC_BYTE_56_CQ_PERIOD_S, cq_period);

    /* Set CQC field of Write with Notify Base Address */
    ret = hns_roce_v2_set_cqc_notify_ba(hr_dev, cq_context);
    if (ret) {
        dev_err(hr_dev->dev, "Set cqc notify ba failed, %d\n", ret);
    }
}

static int hns_roce_v2_req_notify_cq(struct ib_cq *ibcq, enum ib_cq_notify_flags flags)
{
    struct hns_roce_dev *hr_dev = NULL;
    struct hns_roce_cq *hr_cq = NULL;
    u32 notification_flag;
    u32 doorbell[DOORBELL_NUM];

    if (ibcq == NULL) {
        pr_err("hns3: req notify cq, null pointer\n");
        return -EINVAL;
    }

    hr_dev = to_hr_dev(ibcq->device);
    hr_cq = to_hr_cq(ibcq);

    doorbell[0] = 0;
    doorbell[1] = 0;

    notification_flag = ((unsigned int)flags & IB_CQ_SOLICITED_MASK) == IB_CQ_SOLICITED ?
                        V2_CQ_DB_REQ_NOT : V2_CQ_DB_REQ_NOT_SOL;
    /*
     * flags is 0; Notification Flag is 1, next
     * flags is 1; Notification Flag is 0, solocited
     */
    roce_set_field(doorbell[0], V2_CQ_DB_BYTE_4_TAG_M, V2_DB_BYTE_4_TAG_S, hr_cq->cqn);
    roce_set_field(doorbell[0], V2_CQ_DB_BYTE_4_CMD_M, V2_DB_BYTE_4_CMD_S, HNS_ROCE_V2_CQ_DB_NTR);
    roce_set_field(doorbell[1], V2_CQ_DB_PARAMETER_CONS_IDX_M, V2_CQ_DB_PARAMETER_CONS_IDX_S,
                   hr_cq->cons_index & ((hr_cq->cq_depth << 1) - 1));
    roce_set_field(doorbell[1], V2_CQ_DB_PARAMETER_CMD_SN_M,
                   V2_CQ_DB_PARAMETER_CMD_SN_S, (unsigned int)hr_cq->arm_sn & 0x3);
    roce_set_bit(doorbell[1], V2_CQ_DB_PARAMETER_NOTIFY_S, notification_flag);

    hns_roce_write64(hr_dev, doorbell, sizeof(doorbell) / sizeof(u32), hr_cq->cq_db_l);

    return 0;
}

static int hns_roce_handle_recv_inl_wqe(struct hns_roce_v2_cqe *cqe,
    struct hns_roce_qp **cur_qp,
    struct ib_wc *wc)
{
    struct hns_roce_rinl_sge *sge_list;
    void *wqe_buf;
    u32 data_len;
    u32 sge_num;
    u32 sge_cnt;
    u32 wr_num;
    u32 wr_cnt;
    u32 size;
    int ret;

    wr_num = roce_get_field(cqe->byte_4, V2_CQE_BYTE_4_WQE_INDX_M,
                            V2_CQE_BYTE_4_WQE_INDX_S) & SHORT_MASK;
    wr_cnt = wr_num & (unsigned int)((*cur_qp)->rq.wqe_cnt - 1);

    sge_list = (*cur_qp)->rq_inl_buf.wqe_list[wr_cnt].sg_list;
    sge_num = (*cur_qp)->rq_inl_buf.wqe_list[wr_cnt].sge_cnt;
    wqe_buf = get_recv_wqe(*cur_qp, wr_cnt);
    data_len = wc->byte_len;

    for (sge_cnt = 0; (sge_cnt < sge_num) && (data_len); sge_cnt++) {
        size = min(sge_list[sge_cnt].len, data_len);
        ret = memcpy_s((void *)sge_list[sge_cnt].addr, size, wqe_buf, size);
        HNS_ROCE_SEC_CHECK_RET_INT_WITHOUT_DEV(ret);

        data_len -= size;
        wqe_buf += size;
    }

    if (data_len) {
        wc->status = IB_WC_LOC_LEN_ERR;
        return -EAGAIN;
    }

    return 0;
}

static void hns_roce_v2_get_wc_wr_id(int is_send, struct hns_roce_v2_cqe *cqe, struct ib_wc *wc,
    struct hns_roce_qp **cur_qp)
{
    u16 wqe_ctr;
    struct hns_roce_wq *wq = NULL;
    struct hns_roce_srq *srq = NULL;

    if (is_send) {
        wq = &(*cur_qp)->sq;
        if ((*cur_qp)->sq_signal_bits) {
            /*
             * If sg_signal_bit is 1,
             * firstly tail pointer updated to wqe
             * which current cqe correspond to
             */
            wqe_ctr = (u16)roce_get_field(cqe->byte_4, V2_CQE_BYTE_4_WQE_INDX_M, V2_CQE_BYTE_4_WQE_INDX_S);
            wq->tail += (wqe_ctr - (u16)wq->tail) & (unsigned int)(wq->wqe_cnt - 1);
        }

        wc->wr_id = wq->wrid[wq->tail & (unsigned int)(wq->wqe_cnt - 1)];
        wq->wrid[wq->tail & (unsigned int)(wq->wqe_cnt - 1)] = 0;
        ++wq->tail;
    } else if ((*cur_qp)->ibqp.srq) {
        srq = to_hr_srq((*cur_qp)->ibqp.srq);
        wqe_ctr = le16_to_cpu(roce_get_field(cqe->byte_4, V2_CQE_BYTE_4_WQE_INDX_M, V2_CQE_BYTE_4_WQE_INDX_S));
        if (unlikely(wqe_ctr >= srq->max)) {
            pr_err("hns3: wqe_ctr:%u is more than max:%u\n", wqe_ctr, srq->max);
            return;
        }
        wc->wr_id = srq->wrid[wqe_ctr];
        srq->wrid[wqe_ctr] = 0;
        hns_roce_free_srq_wqe(srq, wqe_ctr);
    } else {
        /* Update tail pointer, record wr_id */
        wq = &(*cur_qp)->rq;
        wc->wr_id = wq->wrid[wq->tail & (unsigned int)(wq->wqe_cnt - 1)];
        wq->wrid[wq->tail & (unsigned int)(wq->wqe_cnt - 1)] = 0;
        ++wq->tail;
    }
}

static void hns_roce_v2_get_wc_remote_err(u32 status, struct ib_wc *wc)
{
    switch (status & HNS_ROCE_V2_CQE_STATUS_MASK) {
        case HNS_ROCE_CQE_V2_REMOTE_INVAL_REQ_ERR:
            wc->status = IB_WC_REM_INV_REQ_ERR;
            break;
        case HNS_ROCE_CQE_V2_REMOTE_ACCESS_ERR:
            wc->status = IB_WC_REM_ACCESS_ERR;
            break;
        case HNS_ROCE_CQE_V2_REMOTE_OP_ERR:
            wc->status = IB_WC_REM_OP_ERR;
            break;
        default:
            wc->status = IB_WC_GENERAL_ERR;
    }
    return;
}

static void hns_roce_v2_get_wc_status(u32 status, struct ib_wc *wc)
{
    switch (status & HNS_ROCE_V2_CQE_STATUS_MASK) {
        case HNS_ROCE_CQE_V2_SUCCESS:
            wc->status = IB_WC_SUCCESS;
            break;
        case HNS_ROCE_CQE_V2_LOCAL_LENGTH_ERR:
            wc->status = IB_WC_LOC_LEN_ERR;
            break;
        case HNS_ROCE_CQE_V2_LOCAL_QP_OP_ERR:
            wc->status = IB_WC_LOC_QP_OP_ERR;
            break;
        case HNS_ROCE_CQE_V2_LOCAL_PROT_ERR:
            wc->status = IB_WC_LOC_PROT_ERR;
            break;
        case HNS_ROCE_CQE_V2_WR_FLUSH_ERR:
            wc->status = IB_WC_WR_FLUSH_ERR;
            break;
        case HNS_ROCE_CQE_V2_MW_BIND_ERR:
            wc->status = IB_WC_MW_BIND_ERR;
            break;
        case HNS_ROCE_CQE_V2_BAD_RESP_ERR:
            wc->status = IB_WC_BAD_RESP_ERR;
            break;
        case HNS_ROCE_CQE_V2_LOCAL_ACCESS_ERR:
            wc->status = IB_WC_LOC_ACCESS_ERR;
            break;
        case HNS_ROCE_CQE_V2_TRANSPORT_RETRY_EXC_ERR:
            wc->status = IB_WC_RETRY_EXC_ERR;
            break;
        case HNS_ROCE_CQE_V2_RNR_RETRY_EXC_ERR:
            wc->status = IB_WC_RNR_RETRY_EXC_ERR;
            break;
        case HNS_ROCE_CQE_V2_REMOTE_ABORT_ERR:
            wc->status = IB_WC_REM_ABORT_ERR;
            break;
        default:
            hns_roce_v2_get_wc_remote_err(status, wc);
    }
    return;
}

static void hns_roce_v2_get_send_wc_opcode(struct hns_roce_v2_cqe *cqe, struct ib_wc *wc)
{
    switch (roce_get_field(cqe->byte_4, V2_CQE_BYTE_4_OPCODE_M, V2_CQE_BYTE_4_OPCODE_S) & 0x1f) {
        case HNS_ROCE_SQ_OPCODE_SEND:
            wc->opcode = IB_WC_SEND;
            break;
        case HNS_ROCE_SQ_OPCODE_SEND_WITH_INV:
            wc->opcode = IB_WC_SEND;
            break;
        case HNS_ROCE_SQ_OPCODE_SEND_WITH_IMM:
            wc->opcode = IB_WC_SEND;
            wc->wc_flags = (unsigned int)wc->wc_flags | IB_WC_WITH_IMM;
            break;
        case HNS_ROCE_SQ_OPCODE_RDMA_READ:
            HNS_ROCE_V2_SET_OPCODE_BYTE_LEN(IB_WC_RDMA_READ, le32_to_cpu(cqe->byte_cnt));
            break;
        case HNS_ROCE_SQ_OPCODE_RDMA_WRITE:
            wc->opcode = IB_WC_RDMA_WRITE;
            break;
        case HNS_ROCE_SQ_OPCODE_RDMA_WRITE_WITH_IMM:
            wc->opcode = IB_WC_RDMA_WRITE;
            wc->wc_flags = (unsigned int)wc->wc_flags | IB_WC_WITH_IMM;
            break;
        case HNS_ROCE_SQ_OPCODE_LOCAL_INV:
            wc->opcode = IB_WC_LOCAL_INV;
            wc->wc_flags = (unsigned int)wc->wc_flags | IB_WC_WITH_INVALIDATE;
            break;
        case HNS_ROCE_SQ_OPCODE_ATOMIC_COMP_AND_SWAP:
            HNS_ROCE_V2_SET_OPCODE_BYTE_LEN(IB_WC_COMP_SWAP, WC_BYTE_LEN);
            break;
        case HNS_ROCE_SQ_OPCODE_ATOMIC_FETCH_AND_ADD:
            HNS_ROCE_V2_SET_OPCODE_BYTE_LEN(IB_WC_FETCH_ADD, WC_BYTE_LEN);
            break;
        case HNS_ROCE_SQ_OPCODE_ATOMIC_MASK_COMP_AND_SWAP:
            HNS_ROCE_V2_SET_OPCODE_BYTE_LEN(IB_WC_MASKED_COMP_SWAP, WC_BYTE_LEN);
            break;
        case HNS_ROCE_SQ_OPCODE_ATOMIC_MASK_FETCH_AND_ADD:
            HNS_ROCE_V2_SET_OPCODE_BYTE_LEN(IB_WC_MASKED_FETCH_ADD, WC_BYTE_LEN);
            break;
        case HNS_ROCE_SQ_OPCODE_FAST_REG_WR:
            wc->opcode = IB_WC_REG_MR;
            break;
        case HNS_ROCE_SQ_OPCODE_BIND_MW:
            wc->opcode = IB_WC_REG_MR;
            break;
        default:
            wc->status = IB_WC_GENERAL_ERR;
    }
    return;
}

static int hns_roce_v2_get_recv_wc_opcode(struct hns_roce_v2_cqe *cqe, struct ib_wc *wc,
    struct hns_roce_qp **cur_qp, struct hns_roce_dev *hr_dev)
{
    u32 opcode;
    int ret;

    wc->byte_len = le32_to_cpu(cqe->byte_cnt);

    opcode = roce_get_field(cqe->byte_4, V2_CQE_BYTE_4_OPCODE_M, V2_CQE_BYTE_4_OPCODE_S);
    switch (opcode & 0x1f) {
        case HNS_ROCE_V2_OPCODE_RDMA_WRITE_IMM:
            wc->opcode = IB_WC_RECV_RDMA_WITH_IMM;
            wc->wc_flags = IB_WC_WITH_IMM;
            wc->ex.imm_data = cpu_to_be32(le32_to_cpu(cqe->immtdata));
            break;
        case HNS_ROCE_V2_OPCODE_SEND:
            wc->opcode = IB_WC_RECV;
            wc->wc_flags = 0;
            break;
        case HNS_ROCE_V2_OPCODE_SEND_WITH_IMM:
            wc->opcode = IB_WC_RECV;
            wc->wc_flags = IB_WC_WITH_IMM;
            wc->ex.imm_data = cpu_to_be32(le32_to_cpu(cqe->immtdata));
            break;
        case HNS_ROCE_V2_OPCODE_SEND_WITH_INV:
            wc->opcode = IB_WC_RECV;
            wc->wc_flags = IB_WC_WITH_INVALIDATE;
            wc->ex.invalidate_rkey = le32_to_cpu(cqe->rkey);
            break;
        default:
            wc->status = IB_WC_GENERAL_ERR;
    }
#ifndef DEFINE_HNS_LLT
    if ((wc->qp->qp_type == IB_QPT_RC || wc->qp->qp_type == IB_QPT_UC) &&
        (opcode == HNS_ROCE_V2_OPCODE_SEND || opcode == HNS_ROCE_V2_OPCODE_SEND_WITH_IMM ||
        opcode == HNS_ROCE_V2_OPCODE_SEND_WITH_INV) && (roce_get_bit(cqe->byte_4, V2_CQE_BYTE_4_RQ_INLINE_S))) {
        ret = hns_roce_handle_recv_inl_wqe(cqe, cur_qp, wc);
        if (ret) {
            return -EAGAIN;
        }
    }
#endif
    return 0;
}

static int hns_roce_v2_fill_recv_wc(struct hns_roce_v2_cqe *cqe, struct ib_wc *wc,
    struct hns_roce_dev *hr_dev)
{
    int ret;

    wc->sl = (u8)roce_get_field(cqe->byte_32, V2_CQE_BYTE_32_SL_M, V2_CQE_BYTE_32_SL_S);
    wc->src_qp = (u8)roce_get_field(cqe->byte_32, V2_CQE_BYTE_32_RMT_QPN_M, V2_CQE_BYTE_32_RMT_QPN_S);
    wc->slid = 0;
    wc->wc_flags = (unsigned int)wc->wc_flags | (roce_get_bit(cqe->byte_32, V2_CQE_BYTE_32_GRH_S) ? IB_WC_GRH : 0);
    wc->port_num = roce_get_field(cqe->byte_32, V2_CQE_BYTE_32_PORTN_M, V2_CQE_BYTE_32_PORTN_S);
    wc->pkey_index = 0;
    ret = memcpy_s(wc->smac, sizeof(u32), cqe->smac, sizeof(u32));
    HNS_ROCE_SEC_CHECK_RET_INT(hr_dev->dev, ret);
    wc->smac[SMAC_INDEX4] = roce_get_field(cqe->byte_28, V2_CQE_BYTE_28_SMAC_4_M, V2_CQE_BYTE_28_SMAC_4_S);
    wc->smac[SMAC_INDEX5] = roce_get_field(cqe->byte_28, V2_CQE_BYTE_28_SMAC_5_M, V2_CQE_BYTE_28_SMAC_5_S);
    if (roce_get_bit(cqe->byte_28, V2_CQE_BYTE_28_VID_VLD_S)) {
        wc->vlan_id = (u16)roce_get_field(cqe->byte_28, V2_CQE_BYTE_28_VID_M, V2_CQE_BYTE_28_VID_S);
    } else {
        wc->vlan_id = SHORT_MASK;
    }

    wc->wc_flags = (unsigned int)wc->wc_flags | (IB_WC_WITH_VLAN | IB_WC_WITH_SMAC);
    wc->network_hdr_type = roce_get_field(cqe->byte_28, V2_CQE_BYTE_28_PORT_TYPE_M, V2_CQE_BYTE_28_PORT_TYPE_S);
    return 0;
}

static int hns_roce_v2_fill_qp_wc(int is_send, struct hns_roce_v2_cqe *cqe, struct ib_wc *wc,
    struct hns_roce_dev *hr_dev, struct hns_roce_qp **cur_qp)
{
    int ret;

    if (is_send) {
        wc->wc_flags = 0;
        /* SQ corresponding to CQE */
        hns_roce_v2_get_send_wc_opcode(cqe, wc);
    } else {
        /* RQ correspond to CQE */
        ret = hns_roce_v2_get_recv_wc_opcode(cqe, wc, cur_qp, hr_dev);
        if (ret) {
            return ret;
        }
        ret = hns_roce_v2_fill_recv_wc(cqe, wc, hr_dev);
        if (ret) {
            return ret;
        }
    }
    return 0;
}

static int hns_roce_v2_poll_one(struct hns_roce_cq *hr_cq, struct hns_roce_qp **cur_qp, struct ib_wc *wc)
{
    struct hns_roce_dev *hr_dev = to_hr_dev(hr_cq->ib_cq.device);
    struct hns_roce_v2_cqe *cqe = next_cqe_sw_v2(hr_cq);
    struct hns_roce_qp *hr_qp = NULL;
    int is_send;
    u32 status;
    int qpn;
    int ret;

    /* Find cqe according to consumer index */
    if (cqe == NULL) {
        return -EAGAIN;
    }

    ++hr_cq->cons_index;
    /* Memory barrier */
    rmb();

    /* 0->SQ, 1->RQ */
    is_send = !roce_get_bit(cqe->byte_4, V2_CQE_BYTE_4_S_R_S);

    qpn = roce_get_field(cqe->byte_16, V2_CQE_BYTE_16_LCL_QPN_M, V2_CQE_BYTE_16_LCL_QPN_S);
    if (!*cur_qp || ((unsigned int)qpn & HNS_ROCE_V2_CQE_QPN_MASK) != (*cur_qp)->qpn) {
        spin_lock(&hr_dev->qp_table.lock);
        hr_qp = __hns_roce_qp_lookup(hr_dev, qpn);
        spin_unlock(&hr_dev->qp_table.lock);
        if (unlikely(hr_qp == NULL)) {
            dev_err(hr_dev->dev, "CQ %06lx with entry for unknown QPN %06x\n",
                    hr_cq->cqn, ((unsigned int)qpn & HNS_ROCE_V2_CQE_QPN_MASK));
            return -EINVAL;
        }
        *cur_qp = hr_qp;
    }

    wc->qp = &(*cur_qp)->ibqp;
    wc->vendor_err = 0;

    hns_roce_v2_get_wc_wr_id(is_send, cqe, wc, cur_qp);

    status = roce_get_field(cqe->byte_4, V2_CQE_BYTE_4_STATUS_M, V2_CQE_BYTE_4_STATUS_S);
    hns_roce_v2_get_wc_status(status, wc);

    /* flush cqe if wc status is error, excluding flush error */
    if (wc->status != IB_WC_SUCCESS && wc->status != IB_WC_WR_FLUSH_ERR) {
        init_flush_work(hr_dev, *cur_qp);
        return 0;
    }

    if (wc->status == IB_WC_WR_FLUSH_ERR) {
        return 0;
    }

    ret = hns_roce_v2_fill_qp_wc(is_send, cqe, wc, hr_dev, cur_qp);
    if (ret) {
        return ret;
    }

    return 0;
}

static void sw_comp(struct hns_roce_qp *hr_qp, int num_entries,
    struct ib_wc *wc, int *npolled, struct hns_roce_wq *wq)
{
    unsigned int cur, i;
    int np;

    cur = wq->head - wq->tail;
    np = *npolled;

    if (cur == 0) {
        return;
    }

    for (i = 0;  i < cur && np < num_entries; i++) {
        wc->wr_id = wq->wrid[wq->tail & (unsigned int)(wq->wqe_cnt - 1)];
        wc->status = IB_WC_WR_FLUSH_ERR;
        wc->vendor_err = 0;
        wc->qp = &hr_qp->ibqp;
        wq->tail++;
        np++;
        wc++;
    }
    *npolled = np;
}

static void hns_roce_v2_poll_sw_cq(struct hns_roce_cq *hr_cq, int num_entries,
    struct ib_wc *wc, int *npolled)
{
    struct hns_roce_qp *hr_qp = NULL;

    *npolled = 0;

    list_for_each_entry(hr_qp, &hr_cq->list_sq, scq_list) {
        sw_comp(hr_qp, num_entries, wc + *npolled, npolled, &hr_qp->sq);
        if (*npolled >= num_entries) {
            return;
        }
    }

    list_for_each_entry(hr_qp, &hr_cq->list_rq, rcq_list) {
        sw_comp(hr_qp, num_entries, wc + *npolled, npolled, &hr_qp->rq);
        if (*npolled >= num_entries) {
            return;
        }
    }
}

static int hns_roce_v2_poll_cq(struct ib_cq *ibcq, int num_entries, struct ib_wc *wc)
{
    struct hns_roce_cq *hr_cq = NULL;
    struct hns_roce_dev *hr_dev = NULL;
    struct hns_roce_qp *cur_qp = NULL;
    unsigned long flags;
    int npolled;
    int ret = 0;

    if (ibcq == NULL || wc == NULL) {
        pr_err("hns3: null pointer:ibcq %pK, wc %pK\n", ibcq, wc);
        return -EINVAL;
    }
    hr_cq = to_hr_cq(ibcq);
    hr_dev = to_hr_dev(hr_cq->ib_cq.device);

    spin_lock_irqsave(&hr_cq->lock, flags);

    if (hr_dev->state == HNS_ROCE_DEVICE_STATE_UNINIT) {
        hns_roce_v2_poll_sw_cq(hr_cq, num_entries, wc, &npolled);
        goto out;
    }

    for (npolled = 0; npolled < num_entries; ++npolled) {
        ret = hns_roce_v2_poll_one(hr_cq, &cur_qp, wc + npolled);
        if (ret) {
            break;
        }
    }

    if (npolled) {
        /* Memory barrier */
        wmb();
        hns_roce_v2_cq_set_ci(hr_cq, hr_cq->cons_index);
    }

out:
    spin_unlock_irqrestore(&hr_cq->lock, flags);

    if (ret) {
        dev_err(hr_dev->dev, "hns_roce_v2_poll_one failed, npolled[%d], ret[%d]\n", npolled, ret);
    }

    return npolled;
}

static int get_op_for_set_hem(struct hns_roce_dev *hr_dev, u32 type, int step_idx)
{
    int op;

    switch (type) {
        case HEM_TYPE_QPC:
            op = HNS_ROCE_CMD_WRITE_QPC_BT0;
            break;
        case HEM_TYPE_MTPT:
            op = HNS_ROCE_CMD_WRITE_MPT_BT0;
            break;
        case HEM_TYPE_CQC:
            op = HNS_ROCE_CMD_WRITE_CQC_BT0;
            break;
        case HEM_TYPE_SRQC:
            op = HNS_ROCE_CMD_WRITE_SRQC_BT0;
            break;
        case HEM_TYPE_SCC_CTX:
            if (step_idx) {
                /* No need to notify Hardware when step_idx is 1 or 2 */
                dev_warn(hr_dev->dev, "step_idx is [%d], no need to notify hardware\n", step_idx);
                return -EINVAL;
            }
            op = HNS_ROCE_CMD_WRITE_SCC_CTX_BT0;
            break;
        case HEM_TYPE_QPC_TIMER:
            op = HNS_ROCE_CMD_WRITE_QPC_TIMER_BT0;
            break;
        case HEM_TYPE_CQC_TIMER:
            op = HNS_ROCE_CMD_WRITE_CQC_TIMER_BT0;
            break;
        default:
            dev_warn(hr_dev->dev, "Table %d not to be written by mailbox!\n", type);
            return -EINVAL;
    }

    return op + step_idx;
}

static int hns_roce_v2_set_hem_cmd_mailbox(struct hns_roce_dev *hr_dev,
    struct hns_roce_hem_table *table, struct hns_roce_set_hem_para *para)
{
    struct hns_roce_cmd_mailbox *mailbox = NULL;
    struct hns_roce_mbox_post_params mbox_pst_params = {0};
    struct hns_roce_hem_iter iter;
    struct hns_roce_hem *hem = NULL;
    u64 bt_ba = 0;
    int ret = 0;

    mailbox = hns_roce_alloc_cmd_mailbox(hr_dev);
    HNS_ROCE_ALLOC_CMD_MAILBOX_RET_CHECK(mailbox);

    if (table->type == HEM_TYPE_SCC_CTX) {
        para->obj = para->l0_idx;
    }

    if (check_whether_last_step(para->hop_num, para->step_idx)) {
        hem = table->hem[para->hem_idx];
        for (hns_roce_hem_first(hem, &iter); !hns_roce_hem_last(&iter); hns_roce_hem_next(&iter)) {
            /* configure the ba, tag, and op */
            mbox_pst_params.in_param = hns_roce_hem_addr(&iter);
            mbox_pst_params.out_param = mailbox->dma;
            mbox_pst_params.in_modifier = para->obj;
            mbox_pst_params.op = para->op;
            ret = hns_roce_cmd_mbox(hr_dev, mbox_pst_params, HNS_ROCE_CMD_TIMEOUT_MSECS);
            if (ret) {
                dev_err(hr_dev->dev, "hns_roce_cmd_mbox last_step failed, ret[%d]\n", ret);
                goto out;
            }
        }
    } else {
        if (para->step_idx == 0) {
            bt_ba = table->bt_l0_dma_addr[para->l0_idx];
        } else if (para->step_idx == STEP_IDXONE && para->hop_num == PBL_2LEVELS) {
            bt_ba = table->bt_l1_dma_addr[para->l1_idx];
        }

        /* configure the ba, tag, and op */
        mbox_pst_params.in_param = bt_ba;
        mbox_pst_params.out_param = mailbox->dma;
        mbox_pst_params.in_modifier = para->obj;
        mbox_pst_params.op = para->op;
        ret = hns_roce_cmd_mbox(hr_dev, mbox_pst_params, HNS_ROCE_CMD_TIMEOUT_MSECS);
        if (ret) {
            dev_err(hr_dev->dev, "hns_roce_cmd_mbox failed, ret[%d]\n", ret);
            goto out;
        }
    }
out:
    hns_roce_free_cmd_mailbox(hr_dev, mailbox);
    mailbox = NULL;
    return ret;
}

static int hns_roce_v2_set_hem(struct hns_roce_dev *hr_dev,
    struct hns_roce_hem_table *table, int obj, int step_idx)
{
    struct hns_roce_hem_mhop mhop;
    unsigned long mhop_obj = obj;
    int ret;
    u64 hem_idx = 0;
    u64 l1_idx = 0;
    u32 chunk_ba_num;
    u32 hop_num;
    int i, j, k;
    struct hns_roce_set_hem_para para = {0};

    if (!hns_roce_check_whether_mhop(hr_dev, table->type)) {
        return 0;
    }

    hns_roce_calc_hem_mhop(hr_dev, table, &mhop_obj, &mhop);
    i = mhop.l0_idx;
    j = mhop.l1_idx;
    k = mhop.l2_idx;
    hop_num = mhop.hop_num;
    chunk_ba_num = mhop.bt_chunk_size / BA_BYTE_LEN;

    if (hop_num == PBL_2LEVELS) {
        hem_idx = i * chunk_ba_num * chunk_ba_num + j * chunk_ba_num +
                  k;
        l1_idx = i * chunk_ba_num + j;
    } else if (hop_num == 1) {
        hem_idx = i * chunk_ba_num + j;
    } else if (hop_num == HNS_ROCE_HOP_NUM_0) {
        hem_idx = i;
    }

    para.op = get_op_for_set_hem(hr_dev, table->type, step_idx);
    if (para.op == -EINVAL) {
        dev_warn(hr_dev->dev, "get op [%d], no need to set hem\n", para.op);
        return 0;
    }
    para.obj = obj;
    para.step_idx = step_idx;
    para.hem_idx = hem_idx;
    para.l0_idx = mhop.l0_idx;
    para.l1_idx = l1_idx;
    para.hop_num = hop_num;

    ret = hns_roce_v2_set_hem_cmd_mailbox(hr_dev, table, &para);
    if (ret) {
        dev_err(hr_dev->dev, "hns_roce_v2_set_hem_cmd_mailbox failed, ret[%d]\n", ret);
        return ret;
    }
    return ret;
}

static void hns_roce_v2_clear_hem(struct hns_roce_dev *hr_dev,
    struct hns_roce_hem_table *table, int obj, int step_idx)
{
    struct device *dev = hr_dev->dev;
    struct hns_roce_cmd_mailbox *mailbox = NULL;
    struct hns_roce_mbox_post_params mbox_pst_params = {0};
    u16 op = 0xff;

    if (!hns_roce_check_whether_mhop(hr_dev, table->type)) {
        dev_warn(hr_dev->dev, "table type[%u] isn't an invalid mhop\n", table->type);
        return;
    }

    switch (table->type) {
        case HEM_TYPE_QPC:
            op = HNS_ROCE_CMD_DESTROY_QPC_BT0;
            break;
        case HEM_TYPE_MTPT:
            op = HNS_ROCE_CMD_DESTROY_MPT_BT0;
            break;
        case HEM_TYPE_CQC:
            op = HNS_ROCE_CMD_DESTROY_CQC_BT0;
            break;
        case HEM_TYPE_SCC_CTX:
        case HEM_TYPE_QPC_TIMER:
        case HEM_TYPE_CQC_TIMER:
            /* there is no need to destroy these ctx */
            return;
        case HEM_TYPE_SRQC:
            op = HNS_ROCE_CMD_DESTROY_SRQC_BT0;
            break;
        default:
            dev_warn(dev, "Table %d not to be destroyed by mailbox!\n", table->type);
            return;
    }
    op += step_idx;

    mailbox = hns_roce_alloc_cmd_mailbox(hr_dev);
    if (IS_ERR(mailbox)) {
        dev_err(dev, "Failed to alloc cmd mailbox.\n");
        return;
    }

    /* configure the tag and op */
    mbox_pst_params.in_param = 0;
    mbox_pst_params.out_param = mailbox->dma;
    mbox_pst_params.in_modifier = obj;
    mbox_pst_params.op_modifier = 0;
    mbox_pst_params.op = op;
    if (hns_roce_cmd_mbox(hr_dev, mbox_pst_params, HNS_ROCE_CMD_TIMEOUT_MSECS)) {
        dev_warn(dev, "Failed to clear HEM.\n");
    }

    hns_roce_free_cmd_mailbox(hr_dev, mailbox);
    mailbox = NULL;
    return;
}

static int hns_roce_v2_modify_cq(struct ib_cq *cq, u16 cq_count, u16 cq_period)
{
    struct hns_roce_dev *hr_dev = NULL;
    struct hns_roce_v2_cq_context *cq_context = NULL;
    struct hns_roce_cq *hr_cq = NULL;
    struct hns_roce_v2_cq_context *cqc_mask = NULL;
    struct hns_roce_cmd_mailbox *mailbox = NULL;
    struct hns_roce_mbox_post_params mbox_pst_params = {0};
    int ret;

    if (cq == NULL) {
        pr_err("hns3: modify_cq param err, cq is NULL\n");
        return -EINVAL;
    }

    hr_dev = to_hr_dev(cq->device);
    hr_cq = to_hr_cq(cq);

    mailbox = hns_roce_alloc_cmd_mailbox(hr_dev);
    HNS_ROCE_ALLOC_CMD_MAILBOX_RET_CHECK(mailbox);

    cq_context = mailbox->buf;
    cqc_mask = (struct hns_roce_v2_cq_context *)mailbox->buf + 1;

    ret = memset_s(cqc_mask, sizeof(*cqc_mask), 0xff, sizeof(*cqc_mask));
    HNS_ROCE_SEC_CHECK_GOTO_OUT(hr_dev->dev, ret);

    roce_set_field(cq_context->byte_56_cqe_period_maxcnt,
                   V2_CQC_BYTE_56_CQ_MAX_CNT_M, V2_CQC_BYTE_56_CQ_MAX_CNT_S,
                   cq_count);
    roce_set_field(cqc_mask->byte_56_cqe_period_maxcnt,
                   V2_CQC_BYTE_56_CQ_MAX_CNT_M, V2_CQC_BYTE_56_CQ_MAX_CNT_S,
                   0);
    roce_set_field(cq_context->byte_56_cqe_period_maxcnt,
                   V2_CQC_BYTE_56_CQ_PERIOD_M, V2_CQC_BYTE_56_CQ_PERIOD_S,
                   cq_period);
    roce_set_field(cqc_mask->byte_56_cqe_period_maxcnt,
                   V2_CQC_BYTE_56_CQ_PERIOD_M, V2_CQC_BYTE_56_CQ_PERIOD_S,
                   0);

    mbox_pst_params.in_param = mailbox->dma;
    mbox_pst_params.out_param = 0;
    mbox_pst_params.in_modifier = hr_cq->cqn;
    mbox_pst_params.op_modifier = 1;
    mbox_pst_params.op = HNS_ROCE_CMD_MODIFY_CQC;
    ret = hns_roce_cmd_mbox(hr_dev, mbox_pst_params, HNS_ROCE_CMD_TIMEOUT_MSECS);
    if (ret) {
        dev_err(hr_dev->dev, "MODIFY CQ(0x%lx) cmd process error.\n", hr_cq->cqn);
    }

out:
    hns_roce_free_cmd_mailbox(hr_dev, mailbox);
    mailbox = NULL;

    return ret;
}

static void hns_roce_v2_set_srqc_other_field(struct hns_roce_srq_context *srq_context,
    struct hns_roce_dev *hr_dev, u32 cqn, u64 *mtts_idx, dma_addr_t dma_handle_idx)
{
    srq_context->idx_bt_ba = (u32)(dma_handle_idx >> DMA_HANDLE_OFFSET);
    srq_context->idx_bt_ba = cpu_to_le32(srq_context->idx_bt_ba);
    roce_set_field(srq_context->rsv_idx_bt_ba,
                   SRQC_BYTE_36_SRQ_IDX_BT_BA_M, SRQC_BYTE_36_SRQ_IDX_BT_BA_S,
                   cpu_to_le32(dma_handle_idx >> DMA_HANDLE_HIGHOFF));

    srq_context->idx_cur_blk_addr = (u32)(mtts_idx[0] >> PAGE_ADDR_SHIFT);
    srq_context->idx_cur_blk_addr = cpu_to_le32(srq_context->idx_cur_blk_addr);
    roce_set_field(srq_context->byte_44_idxbufpgsz_addr,
                   SRQC_BYTE_44_SRQ_IDX_CUR_BLK_ADDR_M, SRQC_BYTE_44_SRQ_IDX_CUR_BLK_ADDR_S,
                   cpu_to_le32((mtts_idx[0]) >> (LOWBITS_WIDTH + PAGE_ADDR_SHIFT)));
    roce_set_field(srq_context->byte_44_idxbufpgsz_addr,
                   SRQC_BYTE_44_SRQ_IDX_HOP_NUM_M, SRQC_BYTE_44_SRQ_IDX_HOP_NUM_S,
                   hr_dev->caps.idx_hop_num == HNS_ROCE_HOP_NUM_0 ? 0 : hr_dev->caps.idx_hop_num);

    roce_set_field(srq_context->byte_44_idxbufpgsz_addr, SRQC_BYTE_44_SRQ_IDX_BA_PG_SZ_M,
                   SRQC_BYTE_44_SRQ_IDX_BA_PG_SZ_S, hr_dev->caps.idx_ba_pg_sz);
    roce_set_field(srq_context->byte_44_idxbufpgsz_addr, SRQC_BYTE_44_SRQ_IDX_BUF_PG_SZ_M,
                   SRQC_BYTE_44_SRQ_IDX_BUF_PG_SZ_S, hr_dev->caps.idx_buf_pg_sz);

    srq_context->idx_nxt_blk_addr = (u32)(mtts_idx[1] >> PAGE_ADDR_SHIFT);
    srq_context->idx_nxt_blk_addr = cpu_to_le32(srq_context->idx_nxt_blk_addr);
    roce_set_field(srq_context->rsv_idxnxtblkaddr, SRQC_BYTE_52_SRQ_IDX_NXT_BLK_ADDR_M,
                   SRQC_BYTE_52_SRQ_IDX_NXT_BLK_ADDR_S,
                   cpu_to_le32((mtts_idx[1]) >> (LOWBITS_WIDTH + PAGE_ADDR_SHIFT)));
    roce_set_field(srq_context->byte_56_xrc_cqn, SRQC_BYTE_56_SRQ_XRC_CQN_M,
                   SRQC_BYTE_56_SRQ_XRC_CQN_S, cqn);
    roce_set_field(srq_context->byte_56_xrc_cqn, SRQC_BYTE_56_SRQ_WQE_BA_PG_SZ_M,
                   SRQC_BYTE_56_SRQ_WQE_BA_PG_SZ_S, hr_dev->caps.srqwqe_ba_pg_sz);
    roce_set_field(srq_context->byte_56_xrc_cqn, SRQC_BYTE_56_SRQ_WQE_BUF_PG_SZ_M,
                   SRQC_BYTE_56_SRQ_WQE_BUF_PG_SZ_S, hr_dev->caps.srqwqe_buf_pg_sz);

    roce_set_bit(srq_context->db_record_addr_record_en, SRQC_BYTE_60_SRQ_RECORD_EN_S, 0);
}

static void hns_roce_v2_write_srqc(struct hns_roce_dev *hr_dev,
    struct hns_roce_srq *srq, struct hns_roce_wrt_srqc_info srqc_info, u64 *mtts_idx, void *mb_buf)
{
    struct hns_roce_srq_context *srq_context;
    int ret;

    srq_context = mb_buf;
    ret = memset_s(srq_context, sizeof(*srq_context), 0, sizeof(*srq_context));
    HNS_ROCE_SEC_CHECK_RET_VOID(hr_dev->dev, ret);

    roce_set_field(srq_context->byte_4_srqn_srqst, SRQC_BYTE_4_SRQ_ST_M,
                   SRQC_BYTE_4_SRQ_ST_S, 1);

    roce_set_field(srq_context->byte_4_srqn_srqst,
                   SRQC_BYTE_4_SRQ_WQE_HOP_NUM_M, SRQC_BYTE_4_SRQ_WQE_HOP_NUM_S,
                   (hr_dev->caps.srqwqe_hop_num == HNS_ROCE_HOP_NUM_0 ? 0 : hr_dev->caps.srqwqe_hop_num));
    roce_set_field(srq_context->byte_4_srqn_srqst,
                   SRQC_BYTE_4_SRQ_SHIFT_M, SRQC_BYTE_4_SRQ_SHIFT_S, ilog2(srq->max));

    roce_set_field(srq_context->byte_4_srqn_srqst, SRQC_BYTE_4_SRQN_M,
                   SRQC_BYTE_4_SRQN_S, srq->srqn);

    roce_set_field(srq_context->byte_8_limit_wl, SRQC_BYTE_8_SRQ_LIMIT_WL_M,
                   SRQC_BYTE_8_SRQ_LIMIT_WL_S, 0);

    roce_set_field(srq_context->byte_12_xrcd, SRQC_BYTE_12_SRQ_XRCD_M,
                   SRQC_BYTE_12_SRQ_XRCD_S, srqc_info.xrcd);

    srq_context->wqe_bt_ba = cpu_to_le32((u32)(srqc_info.dma_handle_wqe >> DMA_HANDLE_OFFSET));

    roce_set_field(srq_context->byte_24_wqe_bt_ba,
                   SRQC_BYTE_24_SRQ_WQE_BT_BA_M, SRQC_BYTE_24_SRQ_WQE_BT_BA_S,
                   cpu_to_le32(srqc_info.dma_handle_wqe >> DMA_HANDLE_HIGHOFF));

    roce_set_field(srq_context->byte_28_rqws_pd, SRQC_BYTE_28_PD_M,
                   SRQC_BYTE_28_PD_S, srqc_info.pdn);
    roce_set_field(srq_context->byte_28_rqws_pd, SRQC_BYTE_28_RQWS_M,
                   SRQC_BYTE_28_RQWS_S, srq->max_gs <= 0 ? 0 : fls(srq->max_gs - 1));

    hns_roce_v2_set_srqc_other_field(srq_context, hr_dev, srqc_info.cqn, mtts_idx, srqc_info.dma_handle_idx);
}

static int hns_roce_v2_modify_srq(struct ib_srq *ibsrq,
    struct ib_srq_attr *srq_attr, enum ib_srq_attr_mask srq_attr_mask, struct ib_udata *udata)
{
    struct hns_roce_dev *hr_dev = NULL;
    struct hns_roce_srq *srq = NULL;
    struct hns_roce_srq_context *srq_context = NULL;
    struct hns_roce_srq_context *srqc_mask = NULL;
    struct hns_roce_cmd_mailbox *mailbox = NULL;
    struct hns_roce_mbox_post_params mbox_pst_params = {0};
    struct device *dev = NULL;
    int ret;

    if (ibsrq == NULL || srq_attr == NULL) {
        pr_err("hns3: modify srq param err, ibsrq %pK, ib_srq_attr %pK\n", ibsrq, srq_attr);
        return -EINVAL;
    }

    hr_dev = to_hr_dev(ibsrq->device);
    srq = to_hr_srq(ibsrq);
    dev = hr_dev->dev;

    if ((unsigned int)srq_attr_mask & IB_SRQ_LIMIT) {
        if (srq_attr->srq_limit >= srq->max) {
            dev_err(dev, "invalid srq_limit[%d], the maximum[%d]\n", srq_attr->srq_limit, srq->max);
            return -EINVAL;
        }

        mailbox = hns_roce_alloc_cmd_mailbox(hr_dev);
        HNS_ROCE_ALLOC_CMD_MAILBOX_RET_CHECK(mailbox);

        srq_context = mailbox->buf;
        srqc_mask = (struct hns_roce_srq_context *)mailbox->buf + 1;

        ret = memset_s(srqc_mask, sizeof(*srqc_mask), 0xff, sizeof(*srqc_mask));
        if (ret) {
            hns_roce_free_cmd_mailbox(hr_dev, mailbox);
            dev_err(hr_dev->dev, "memset srqc_mask failed, ret[%d]\n", ret);
            return ret;
        }

        roce_set_field(srq_context->byte_8_limit_wl, SRQC_BYTE_8_SRQ_LIMIT_WL_M,
                       SRQC_BYTE_8_SRQ_LIMIT_WL_S, srq_attr->srq_limit);
        roce_set_field(srqc_mask->byte_8_limit_wl, SRQC_BYTE_8_SRQ_LIMIT_WL_M, SRQC_BYTE_8_SRQ_LIMIT_WL_S, 0);

        mbox_pst_params.in_param = mailbox->dma;
        mbox_pst_params.out_param = 0;
        mbox_pst_params.in_modifier = srq->srqn;
        mbox_pst_params.op_modifier = 0;
        mbox_pst_params.op = HNS_ROCE_CMD_MODIFY_SRQC;
        ret = hns_roce_cmd_mbox(hr_dev, mbox_pst_params, HNS_ROCE_CMD_TIMEOUT_MSECS);
        hns_roce_free_cmd_mailbox(hr_dev, mailbox);
        if (ret) {
            dev_err(hr_dev->dev, "MODIFY SRQ(0x%lx) cmd process error(%d).\n", srq->srqn, ret);
            return ret;
        }
    }

    return 0;
}

STATIC int hns_roce_v2_query_srq(struct ib_srq *ibsrq, struct ib_srq_attr *attr)
{
    struct hns_roce_dev *hr_dev = NULL;
    struct hns_roce_srq *srq = NULL;
    struct hns_roce_srq_context *srq_context = NULL;
    struct hns_roce_cmd_mailbox *mailbox = NULL;
    struct hns_roce_mbox_post_params mbox_pst_params = {0};
    int limit_wl;
    int ret;

    if (ibsrq == NULL || attr == NULL) {
        pr_err("hns3: query srq param err, ibsrq %pK, ib_srq_attr %pK\n", ibsrq, attr);
        return -EINVAL;
    }
    hr_dev = to_hr_dev(ibsrq->device);
    srq = to_hr_srq(ibsrq);

    mailbox = hns_roce_alloc_cmd_mailbox(hr_dev);
    HNS_ROCE_ALLOC_CMD_MAILBOX_RET_CHECK(mailbox);

    srq_context = mailbox->buf;
    mbox_pst_params.in_param = 0;
    mbox_pst_params.out_param = mailbox->dma;
    mbox_pst_params.in_modifier = srq->srqn;
    mbox_pst_params.op_modifier = 0;
    mbox_pst_params.op = HNS_ROCE_CMD_QUERY_SRQC;
    ret = hns_roce_cmd_mbox(hr_dev, mbox_pst_params, HNS_ROCE_CMD_TIMEOUT_MSECS);
    if (ret) {
        dev_err(hr_dev->dev, "QUERY SRQ(0x%lx) cmd process error(%d).\n", srq->srqn, ret);
        goto out;
    }

    limit_wl = roce_get_field(srq_context->byte_8_limit_wl, SRQC_BYTE_8_SRQ_LIMIT_WL_M, SRQC_BYTE_8_SRQ_LIMIT_WL_S);

    attr->srq_limit = limit_wl;
    attr->max_wr    = srq->max - 1;
    attr->max_sge   = srq->max_gs;

out:
    hns_roce_free_cmd_mailbox(hr_dev, mailbox);
    return ret;
}

static int find_empty_entry(struct hns_roce_idx_que *idx_que, unsigned int *wqe_idx)
{
    unsigned int i;
    int bit_num;

    /* bitmap[i] is set zero if all bits are allocated */
    for (i = 0; (i < idx_que->bitmap_len) && (idx_que->bitmap[i] == 0); ++i) {
        ;
    }

    if (i >= idx_que->bitmap_len) {
        pr_err("hns3: the idx que bitmap space is used up!\n");
        return -ENOMEM;
    }

    bit_num = __ffs64(idx_que->bitmap[i]) + 1;
    idx_que->bitmap[i] &= ~(1ULL << (unsigned int)(bit_num - 1));

    *wqe_idx =  i * BITS_PER_LONG_LONG + ((unsigned int)bit_num - 1);
    return 0;
}

static void fill_idx_queue(struct hns_roce_idx_que *idx_que,
    unsigned int cur_idx, unsigned int wqe_idx)
{
    unsigned int *addr;

    addr = (unsigned int *)hns_roce_buf_offset(&idx_que->idx_buf,
                                               cur_idx * idx_que->entry_sz);
    *addr = wqe_idx;
}

static int hns_roce_v2_post_srq_recv_one(struct hns_roce_dev *hr_dev,
    const struct ib_recv_wr *wr, const struct ib_recv_wr **bad_wr, struct hns_roce_srq *srq, unsigned int ind)
{
    struct hns_roce_v2_wqe_data_seg *dseg = NULL;
    unsigned int wqe_idx;
    int ret = 0;
    void *wqe = NULL;
    int i;

    if (unlikely(wr->num_sge > srq->max_gs)) {
        ret = -EINVAL;
        *bad_wr = wr;
        return ret;
    }

    if (unlikely(srq->head == srq->tail)) {
        ret = -ENOSPC;
        *bad_wr = wr;
        return ret;
    }

    ret = find_empty_entry(&srq->idx_que, &wqe_idx);
    if (ret) {
        return -ENOMEM;
    }

    fill_idx_queue(&srq->idx_que, ind, wqe_idx);
    wqe = get_srq_wqe(srq, wqe_idx);
    dseg = (struct hns_roce_v2_wqe_data_seg *)wqe;

    for (i = 0; i < wr->num_sge; ++i) {
        dseg[i].len = cpu_to_le32(wr->sg_list[i].length);
        dseg[i].lkey = cpu_to_le32(wr->sg_list[i].lkey);
        dseg[i].addr = cpu_to_le64(wr->sg_list[i].addr);
    }

    if (i < srq->max_gs) {
        dseg[i].len = 0;
        dseg[i].lkey = cpu_to_le32(0x100);
        dseg[i].addr = 0;
    }

    srq->wrid[wqe_idx] = wr->wr_id;

    return ret;
}

static int hns_roce_v2_post_srq_recv(struct ib_srq *ibsrq,
                                     const struct ib_recv_wr *wr,
                                     const struct ib_recv_wr **bad_wr)
{
    struct hns_roce_dev *hr_dev = NULL;
    struct hns_roce_srq *srq = NULL;
    struct hns_roce_v2_db srq_db;
    unsigned long flags;
    unsigned int ind;
    int ret = 0;
    int nreq;

    if (ibsrq == NULL || wr == NULL || bad_wr == NULL) {
        pr_err("hns3: post_srq_recv param err, ibsrq %pK, wr %pK, bad_wr %pK\n", ibsrq, wr, bad_wr);
        return -EINVAL;
    }

    hr_dev = to_hr_dev(ibsrq->device);
    srq = to_hr_srq(ibsrq);

    spin_lock_irqsave(&srq->lock, flags);

    ind = (unsigned int)(srq->head & (srq->max - 1));
    for (nreq = 0; wr; ++nreq, wr = wr->next) {
        ret = hns_roce_v2_post_srq_recv_one(hr_dev, wr, bad_wr, srq, ind);
        if (ret) {
            break;
        }
        ind = (ind + 1) & (srq->max - 1);
    }

    if (likely(nreq)) {
        srq->head += nreq;

        /*
         * Make sure that descriptors are written before
         * doorbell record.
         */
        wmb();

        srq_db.byte_4 = HNS_ROCE_V2_SRQ_DB << V2_DB_BYTE_4_CMD_S |
                        (srq->srqn & V2_DB_BYTE_4_TAG_M);
        srq_db.parameter = srq->head;
        hns_roce_write64(hr_dev, (__le32 *)&srq_db, 1, srq->db_reg_l);
    }

    spin_unlock_irqrestore(&srq->lock, flags);

    if (ret) {
        dev_err(hr_dev->dev, "srq[%ld] post_srq_recv_one failed, ret %d\n", srq->srqn, ret);
    }

    return ret;
}

static const struct hns_roce_dfx_hw g_hns_roce_dfx_hw_v2 = {
    .query_cqc_info = hns_roce_v2_query_cqc_info,
    .query_qpc_info = hns_roce_v2_query_qpc_info,
    .query_mpt_info = hns_roce_v2_query_mpt_info,
    .query_cqc_stat = hns_roce_v2_query_cqc_stat,
    .query_cmd_stat = hns_roce_v2_query_cmd_stat,
    .query_ceqc_stat = hns_roce_v2_query_ceqc_stat,
    .query_aeqc_stat = hns_roce_v2_query_aeqc_stat,
    .query_qpc_stat = hns_roce_v2_query_qpc_stat,
    .query_srqc_stat = hns_roce_v2_query_srqc_stat,
    .query_mpt_stat = hns_roce_v2_query_mpt_stat,
    .query_pkt_stat = hns_roce_v2_query_pkt_stat,
    .modify_eq = hns_roce_v2_modify_eq,

};

static const struct ib_device_ops hns_roce_v2_dev_ops = {
    .destroy_qp = hns_roce_v2_destroy_qp,
    .modify_cq = hns_roce_v2_modify_cq,
    .poll_cq = hns_roce_v2_poll_cq,
    .post_recv = hns_roce_v2_post_recv,
    .post_send = hns_roce_v2_post_send,
    .query_qp = hns_roce_v2_query_qp,
    .req_notify_cq = hns_roce_v2_req_notify_cq,
};

static const struct ib_device_ops hns_roce_v2_dev_srq_ops = {
    .modify_srq = hns_roce_v2_modify_srq,
    .post_srq_recv = hns_roce_v2_post_srq_recv,
    .query_srq = hns_roce_v2_query_srq,
};

STATIC const struct hns_roce_hw g_hns_roce_hw_v2 = {
    .cmq_init = hns_roce_v2_cmq_init,
    .cmq_exit = hns_roce_v2_cmq_exit,
    .hw_profile = hns_roce_v2_profile,
    .hw_rst_atu = hns_roce_v2_reset_atu_table,
    .hw_init = hns_roce_v2_init,
    .hw_exit = hns_roce_v2_exit,
    .post_mbox = hns_roce_v2_post_mbox,
    .chk_mbox = hns_roce_v2_chk_mbox,
    .rst_prc_mbox = hns_roce_v2_rst_process_cmd,
    .set_gid = hns_roce_v2_set_gid,
    .set_mac = hns_roce_v2_set_mac,
    .write_mtpt = hns_roce_v2_write_mtpt,
    .rereg_write_mtpt = hns_roce_v2_rereg_write_mtpt,
    .frmr_write_mtpt = hns_roce_v2_frmr_write_mtpt,
    .mw_write_mtpt = hns_roce_v2_mw_write_mtpt,
    .write_cqc = hns_roce_v2_write_cqc,
    .set_hem = hns_roce_v2_set_hem,
    .clear_hem = hns_roce_v2_clear_hem,
    .modify_qp = hns_roce_v2_modify_qp,
    .qp_flow_control_init = hns_roce_v2_qp_flow_control_init,
    .init_eq = hns_roce_v2_init_eq_table,
    .cleanup_eq = hns_roce_v2_cleanup_eq_table,
    .write_srqc = hns_roce_v2_write_srqc,
    .hns_roce_dev_ops = &hns_roce_v2_dev_ops,
    .hns_roce_dev_srq_ops = &hns_roce_v2_dev_srq_ops,
};

static const struct pci_device_id g_hns_roce_hw_v2_pci_tbl[] = {
    {PCI_VDEVICE(HUAWEI, HNAE3_DEV_ID_25GE_RDMA), 0},
    {PCI_VDEVICE(HUAWEI, HNAE3_DEV_ID_25GE_RDMA_SDI), 0},
    {PCI_VDEVICE(HUAWEI, HNAE3_DEV_ID_25GE_RDMA_MACSEC), 0},
    {PCI_VDEVICE(HUAWEI, HNAE3_DEV_ID_50GE_RDMA), 0},
    {PCI_VDEVICE(HUAWEI, HNAE3_DEV_ID_50GE_RDMA_MACSEC), 0},
    {PCI_VDEVICE(HUAWEI, HNAE3_DEV_ID_100G_RDMA_MACSEC), 0},
    {PCI_VDEVICE(HUAWEI, HNAE3_DEV_ID_100G_RDMA_SDI), 0},
    {PCI_VDEVICE(HUAWEI, HNAE3_DEV_ID_200GE_RDMA), 0},
    {PCI_VDEVICE(HUAWEI, HNAE3_DEV_ID_400GE_RDMA), 0},
    /* required last entry */
    {0, }
};

MODULE_DEVICE_TABLE(pci, g_hns_roce_hw_v2_pci_tbl);

static int hns_roce_hw_v2_get_cfg(struct hns_roce_dev *hr_dev,
    struct hnae3_handle *handle)
{
    struct hns_roce_v2_priv *priv = hr_dev->priv;
    const struct pci_device_id *id;
    int d;
    int i;

    id = pci_match_id(g_hns_roce_hw_v2_pci_tbl, hr_dev->pci_dev);
    if (id == NULL) {
        dev_err(hr_dev->dev, "device is not compatible!\n");
        return -ENXIO;
    }

    hr_dev->hw = &g_hns_roce_hw_v2;
    hr_dev->dfx = &g_hns_roce_dfx_hw_v2;
    hr_dev->sdb_offset = ROCEE_DB_SQ_L_0_REG;
    hr_dev->odb_offset = hr_dev->sdb_offset;

    /* Get info from NIC driver. */
    hr_dev->reg_base = handle->rinfo.roce_io_base;
    hr_dev->caps.num_ports = 1;
    hr_dev->iboe.netdevs[0] = handle->rinfo.netdev;
    hr_dev->iboe.phy_port[0] = 0;
    d = g_is_d;

    addrconf_addr_eui48((u8 *)&hr_dev->ib_dev.node_guid,
                        hr_dev->iboe.netdevs[0]->dev_addr);

    for (i = 0; i < HNS_ROCE_V2_MAX_IRQ_NUM(d); i++)
        hr_dev->irq[i] = pci_irq_vector(handle->pdev,
                                        i + handle->rinfo.base_vector);

    /* cmd issue mode: 0 is poll, 1 is event */
    hr_dev->cmd_mod = 1;
    hr_dev->loop_idc = g_loopback;

    hr_dev->reset_cnt = handle->ae_algo->ops->ae_dev_reset_cnt(handle);
    priv->handle = handle;

    mutex_init(&hr_dev->hr_stat_mutex);

    return 0;
}

static struct hns_roce_dev *hns_roce_hw_v2_alloc_dev(struct hnae3_handle *handle)
{
    struct hns_roce_dev *hr_dev;

    hr_dev = (struct hns_roce_dev *)ib_alloc_device(hns_roce_dev, ib_dev);
    if (hr_dev == NULL) {
        pr_err("hns3:ib alloc device failed\n");
        return NULL;
    }

    hr_dev->priv = kzalloc(sizeof(struct hns_roce_v2_priv), GFP_KERNEL);
    if (hr_dev->priv == NULL) {
        dev_err(hr_dev->dev, "alloc device priv failed\n");
        ib_dealloc_device(&hr_dev->ib_dev);
        return NULL;
    }

    hr_dev->pci_dev = handle->pdev;
    hr_dev->dev = &handle->pdev->dev;

    return hr_dev;
}

static int __hns_roce_hw_v2_init_instance(struct hnae3_handle *handle)
{
    struct hns_roce_dev *hr_dev;
    int ret;

    hr_dev = hns_roce_hw_v2_alloc_dev(handle);
    if (hr_dev == NULL) {
        pr_err("hns3: alloc hr dev failed\n");
        return -ENOMEM;
    }

    ret = hns_roce_hw_v2_get_cfg(hr_dev, handle);
    if (ret) {
#ifndef DEFINE_HNS_LLT
        dev_err(hr_dev->dev, "Get Configuration failed(%d)!\n", ret);
        goto error_failed_get_cfg;
#endif
    }

    hr_dev->notify_priv = hns_roce_notify_client_init();
    if (hr_dev->notify_priv == NULL) {
        ret = -ENOMEM;
        dev_err(hr_dev->dev, "RoCE Notify init failed!\n");
        goto error_notify_client_init;
    }

    ret = hns_roce_init(hr_dev);
    if (ret) {
#ifndef DEFINE_HNS_LLT
        dev_err(hr_dev->dev, "RoCE Engine init failed(%d)!\n", ret);
        goto error_roce_init;
#endif
    }

    handle->priv = hr_dev;
    hns_roce_query_func_info(hr_dev);

    ret = hns_roce_init_shared_buffer(hr_dev);
    if (ret) {
        dev_err(hr_dev->dev, "hns_roce_init_shared_buffer failed! ret:%d\n", ret);
        goto error_init_shared_buffer;
    }
    return 0;

error_init_shared_buffer:
    hns_roce_uninit(hr_dev);
error_roce_init:
    hns_roce_notify_client_cleanup(hr_dev->notify_priv);
error_notify_client_init:
error_failed_get_cfg:
    kfree(hr_dev->priv);
    hr_dev->priv = NULL;
    ib_dealloc_device(&hr_dev->ib_dev);

    return ret;
}

static void __hns_roce_hw_v2_uninit_instance(struct hnae3_handle *handle,
    bool reset)
{
    struct hns_roce_dev *hr_dev = (struct hns_roce_dev *)handle->priv;
    int devid;

    if (hr_dev == NULL) {
        pr_warn("hns roce dev is null, has uninited.\n");
        return;
    }
    devid = hns_roce_get_devid(hr_dev);
    if (devid < 0) {
        dev_err(hr_dev->dev, "get err devid[%d] \n", devid);
        return;
    }
    hns_bbox_excep_report((u32)devid, HNS_EXCEPID_RoCE_UNINIT, NULL);

    handle->priv = NULL;
    hr_dev->state = HNS_ROCE_DEVICE_STATE_UNINIT;
    rmb();
    hns_roce_handle_device_err(hr_dev);
    hns_roce_exit(hr_dev);
    // notify
    hns_roce_notify_client_cleanup(hr_dev->notify_priv);
    // shared buffer
    hns_roce_free_shared_buffer(hr_dev);
    kfree(hr_dev->priv);
    hr_dev->priv = NULL;
    ib_dealloc_device(&hr_dev->ib_dev);

    mutex_destroy(&hr_dev->hr_stat_mutex);
}

static void hns_wait_reset(struct hnae3_handle *handle, unsigned long end, const struct hnae3_ae_ops *ops)
{
#define SLEEP_TIME 20

#ifndef DEFINE_HNS_LLT
        end = msecs_to_jiffies(HNS_ROCE_V2_RST_PRC_MAX_TIME) + jiffies;
        while (ops->ae_dev_resetting(handle) && time_before(jiffies, end)) {
            msleep(SLEEP_TIME);
        }

        if (!ops->ae_dev_resetting(handle)) {
            dev_info(&handle->pdev->dev, "Device completed reset.\n");
        } else {
            dev_warn(&handle->pdev->dev,
                     "Device is still resetting! timeout!\n");
            WARN_ON(1);
        }

        __hns_roce_hw_v2_uninit_instance(handle, false);
        handle->rinfo.instance_state = HNS_ROCE_STATE_NON_INIT;
#endif
}

static int hns_roce_hw_v2_init_instance(struct hnae3_handle *handle)
{
    const struct hnae3_ae_ops *ops = handle->ae_algo->ops;
    const struct pci_device_id *id = NULL;
    struct hns_roce_dev *hr_dev = NULL;
    unsigned long end = 0;
    int ret;

    handle->rinfo.instance_state = HNS_ROCE_STATE_INIT;

    if (ops->ae_dev_resetting(handle) || ops->get_hw_reset_stat(handle)) {
        handle->rinfo.instance_state = HNS_ROCE_STATE_NON_INIT;
        goto head_chk_rst;
    }

    id = pci_match_id(g_hns_roce_hw_v2_pci_tbl, handle->pdev);
    if (id == NULL) {
        return 0;
    }

    ret = __hns_roce_hw_v2_init_instance(handle);
    if (ret) {
        handle->rinfo.instance_state = HNS_ROCE_STATE_NON_INIT;
        dev_err(&handle->pdev->dev, "RoCE instance init failed! ret = %d\n", ret);
        if (ops->ae_dev_resetting(handle) ||
                ops->get_hw_reset_stat(handle)) {
            goto head_chk_rst;
        } else {
            return ret;
        }
    }

    handle->rinfo.instance_state = HNS_ROCE_STATE_INITED;

    hr_dev = (struct hns_roce_dev *)handle->priv;
    if (ops->ae_dev_resetting(handle) || ops->get_hw_reset_stat(handle) ||
            hr_dev->reset_cnt != ops->ae_dev_reset_cnt(handle)) {
#ifndef DEFINE_HNS_LLT
        handle->rinfo.instance_state = HNS_ROCE_STATE_INIT;
        goto tail_chk_rst;
#endif
    }

    return 0;

tail_chk_rst:
    /* Wait until software reset process finished, in order to ensure that
     * reset process and this function will not call
     * __hns_roce_hw_v2_uninit_instance at the same time.
     * If a timeout occurs, it indicates that the network subsystem has
     * encountered a serious error and cannot be recovered from the reset
     * processing.
     */
    hns_wait_reset(handle, end, ops);

head_chk_rst:
    dev_err(&handle->pdev->dev, "Device is busy in resetting state.\n"
            "please retry later.\n");

    return -EBUSY;
}

static void hns_roce_hw_v2_uninit_instance(struct hnae3_handle *handle,
    bool reset)
{
    const struct hnae3_ae_ops *ops = handle->ae_algo->ops;
    unsigned long end;

    if (handle->rinfo.instance_state != HNS_ROCE_STATE_INITED) {
        return;
    }

    handle->rinfo.instance_state = HNS_ROCE_STATE_UNINIT;

    /* Check the status of the current software reset process, if in
     * software reset process, wait until software reset process finished,
     * in order to ensure that reset process and this function will not call
     * __hns_roce_hw_v2_uninit_instance at the same time.
     * If a timeout occurs, it indicates that the network subsystem has
     * encountered a serious error and cannot be recovered from the reset
     * processing.
     */
    if (ops->ae_dev_resetting(handle)) {
        dev_warn(&handle->pdev->dev, "Device is busy in resetting state. waiting.\n");
        end = msecs_to_jiffies(HNS_ROCE_V2_RST_PRC_MAX_TIME) + jiffies;
        while (ops->ae_dev_resetting(handle) &&
                time_before(jiffies, end)) {
            msleep(SLEEP_TIME);
        }

        if (!ops->ae_dev_resetting(handle))
            dev_info(&handle->pdev->dev, "Device completed reset.\n");
        else {
            dev_warn(&handle->pdev->dev, "Device is still resetting! timeout!\n");
            WARN_ON(1);
        }
    }

    __hns_roce_hw_v2_uninit_instance(handle, reset);

    handle->rinfo.instance_state = HNS_ROCE_STATE_NON_INIT;
}

static void hns_roce_hw_v2_reset_notify_usr(struct hns_roce_dev *hr_dev)
{
    struct hns_roce_v2_priv *priv = hr_dev->priv;
    struct hns_roce_v2_uar *uar = (struct hns_roce_v2_uar *)priv->uar.buf;

    uar->dis_db = HNS_ROCE_DISABLE_DB;
}

/*
 * We need set device state before handle device err. So, sq/rq lock will be
 * effect to return error or involve cq.
 */
static void hns_roce_handle_device_err(struct hns_roce_dev *hr_dev)
{
    struct hns_roce_qp *hr_qp = NULL;
    struct hns_roce_cq *hr_scq = NULL;
    struct hns_roce_cq *hr_rcq = NULL;
    struct hns_roce_cq *hr_cq = NULL;
    struct list_head cq_armed_list;
    unsigned long flags_qp;
    unsigned long flags_cq;
    unsigned long flags;

    INIT_LIST_HEAD(&cq_armed_list);

    spin_lock_irqsave(&hr_dev->reset_lock, flags);
    list_for_each_entry(hr_qp, &hr_dev->qp_list, qps_list) {
        spin_lock_irqsave(&hr_qp->sq.lock, flags_qp);
        if (hr_qp->sq.tail != hr_qp->sq.head) {
            hr_scq = to_hr_cq(hr_qp->ibqp.send_cq);
            spin_lock_irqsave(&hr_scq->lock, flags_cq);
            if (hr_scq->comp && hr_qp->ibqp.send_cq->comp_handler) {
                if (!hr_scq->reset_has_notifyed) {
                    hr_scq->reset_has_notifyed = 1;
                    list_add_tail(&hr_scq->reset_notify, &cq_armed_list);
                }
            }
            spin_unlock_irqrestore(&hr_scq->lock, flags_cq);
        }
        spin_unlock_irqrestore(&hr_qp->sq.lock, flags_qp);

        spin_lock_irqsave(&hr_qp->rq.lock, flags_qp);
        if (hr_qp->ibqp.srq) {
            spin_unlock_irqrestore(&hr_qp->rq.lock, flags_qp);
            continue;
        }

        if (hr_qp->rq.tail != hr_qp->rq.head) {
            hr_rcq = to_hr_cq(hr_qp->ibqp.recv_cq);
            spin_lock_irqsave(&hr_rcq->lock, flags_cq);
            if (hr_rcq->comp && hr_qp->ibqp.recv_cq->comp_handler) {
                if (!hr_rcq->reset_has_notifyed) {
                    hr_rcq->reset_has_notifyed = 1;
                    list_add_tail(&hr_rcq->reset_notify, &cq_armed_list);
                }
            }
            spin_unlock_irqrestore(&hr_rcq->lock, flags_cq);
        }

        spin_unlock_irqrestore(&hr_qp->rq.lock, flags_qp);
    }

    list_for_each_entry(hr_cq, &cq_armed_list, reset_notify) {
        hr_cq->comp(hr_cq);
    }
    spin_unlock_irqrestore(&hr_dev->reset_lock, flags);
}

static int hns_roce_hw_v2_reset_notify_down(struct hnae3_handle *handle)
{
    struct hns_roce_dev *hr_dev = (struct hns_roce_dev *)handle->priv;
    struct ib_event event;

    if (handle->rinfo.instance_state != HNS_ROCE_STATE_INITED) {
        set_bit(HNS_ROCE_RST_DIRECT_RETURN, &handle->rinfo.state);
        return 0;
    }

    handle->rinfo.reset_state = HNS_ROCE_STATE_RST_DOWN;
    clear_bit(HNS_ROCE_RST_DIRECT_RETURN, &handle->rinfo.state);

    if (hr_dev == NULL) {
        return 0;
    }

    hr_dev->active = false;
    hr_dev->dis_db = true;
    hr_dev->is_reset = true;
    hns_roce_hw_v2_reset_notify_usr(hr_dev);
    hr_dev->state = HNS_ROCE_DEVICE_STATE_RST_DOWN;

    event.event = IB_EVENT_DEVICE_FATAL;
    event.device = &hr_dev->ib_dev;
    event.element.port_num = 1;
    ib_dispatch_event(&event);

    return 0;
}

static int hns_roce_hw_v2_reset_notify_init(struct hnae3_handle *handle)
{
    int ret;

    if (test_bit(HNS_ROCE_RST_DIRECT_RETURN, &handle->rinfo.state)) {
        clear_bit(HNS_ROCE_RST_DIRECT_RETURN, &handle->rinfo.state);
        handle->rinfo.reset_state = HNS_ROCE_STATE_RST_INITED;
        return 0;
    }

    handle->rinfo.reset_state = HNS_ROCE_STATE_RST_INIT;

    dev_info(&handle->pdev->dev, "In reset process RoCE client reinit.\n");
    ret = __hns_roce_hw_v2_init_instance(handle);
    if (ret) {
        /* when reset notify type is HNAE3_INIT_CLIENT In reset notify
         * callback function, RoCE Engine reinitialize. If RoCE reinit
         * failed, we should inform NIC driver.
         */
        handle->priv = NULL;
        dev_err(&handle->pdev->dev, "In reset process RoCE reinit failed %d.\n", ret);
    } else {
        handle->rinfo.reset_state = HNS_ROCE_STATE_RST_INITED;
        dev_info(&handle->pdev->dev, "Reset done, RoCE client reinit finished.\n");
    }

    return ret;
}

static int hns_roce_hw_v2_reset_notify_uninit(struct hnae3_handle *handle)
{
    if (test_bit(HNS_ROCE_RST_DIRECT_RETURN, &handle->rinfo.state)) {
        return 0;
    }
    handle->rinfo.reset_state = HNS_ROCE_STATE_RST_UNINIT;

    msleep(SLEEP_LONGTIME);
    __hns_roce_hw_v2_uninit_instance(handle, false);

    return 0;
}

static int hns_roce_hw_v2_reset_notify(struct hnae3_handle *handle,
    enum hnae3_reset_notify_type type)
{
    int ret = 0;

    switch (type) {
        case HNAE3_DOWN_CLIENT:
            ret = hns_roce_hw_v2_reset_notify_down(handle);
            break;
        case HNAE3_INIT_CLIENT:
            ret = hns_roce_hw_v2_reset_notify_init(handle);
            break;
        case HNAE3_UNINIT_CLIENT:
            ret = hns_roce_hw_v2_reset_notify_uninit(handle);
            break;
        default:
            return 0;
    }

    return ret;
}

static const struct hnae3_client_ops g_hns_roce_hw_v2_ops = {
    .init_instance = hns_roce_hw_v2_init_instance,
    .uninit_instance = hns_roce_hw_v2_uninit_instance,
    .reset_notify = hns_roce_hw_v2_reset_notify,
};

static struct hnae3_client g_hns_roce_hw_v2_client = {
    .name = "hns_roce_hw_v2",
    .type = HNAE3_CLIENT_ROCE,
    .ops = &g_hns_roce_hw_v2_ops,
};

static int __init hns_roce_hw_v2_init(void)
{
    return hnae3_register_client(&g_hns_roce_hw_v2_client);
}

static void __exit hns_roce_hw_v2_exit(void)
{
    hnae3_unregister_client(&g_hns_roce_hw_v2_client);
}

module_init(hns_roce_hw_v2_init);
module_exit(hns_roce_hw_v2_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Huawei Tech. Co., Ltd.");
MODULE_DESCRIPTION("Hisilicon Hip08 Family RoCE Driver");
#ifndef DEFINE_HNS_LLT
module_param(g_loopback, int, 0444);
MODULE_PARM_DESC(g_loopback, "default: 0");
module_param(g_is_d, int, 0444);
MODULE_PARM_DESC(g_is_d, "default: 0");
#endif

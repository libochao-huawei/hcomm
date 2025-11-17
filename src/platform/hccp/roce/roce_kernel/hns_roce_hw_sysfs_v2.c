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
#include <linux/version.h>
#include <net/addrconf.h>
#include <rdma/ib_addr.h>
#include <rdma/ib_cache.h>
#include <rdma/ib_umem.h>
#include "hnae3.h"
#include "hns_roce_common.h"
#include "hns_roce_device.h"
#include "hns_roce_cmd.h"
#include "hns_roce_hem.h"
#include "hns_roce_hw_v2.h"
#include "hns_roce_sec.h"
#include "hns_roce_hw_sysfs_v2.h"

#ifdef DEFINE_HNS_LLT
#undef static
#define static
#endif

struct reg_info g_reg_info[] = {
    /* ERR regs */
    {ROCEE_RAS_INT_CFG2_LABEL, 0x2004},
    {ROCEE_RAS_INT_SRC1_LABEL, 0x2008},
    {ROCEE_RAS_INT_SRC2_LABEL, 0x200c},
    {SCC_INT_SRC_LABEL, 0x4058},
    {ROCEE_TDP_STA_LABEL, 0x1010},
    {ROCEE_TDP_ALM_LABEL, 0x1014},
    {ROCEE_TWP_STA_LABEL, 0x1018},
    {ROCEE_TWP_ALM_LABEL, 0x101C},
    {ROCEE_TGP_STA_LABEL, 0x1020},
    {ROCEE_TGP_ALM_LABEL, 0x1024},
    {ROCEE_TMP_STA_LABEL, 0x1028},
    {ROCEE_TMP_ALM_LABEL, 0x102C},
    {ROCEE_TPP_STA_LABEL, 0x1030},
    {ROCEE_TPP_ALM_LABEL, 0x1034},
    {ROCEE_SSU_TC_XOFF_LABEL, 0x1038},
    {ROCEE_TPP_TC_XOFF_LABEL, 0x103C},

    /* TRP regs */
    {ROCEE_TRP_EMPTY_0_LABEL, 0x10ac},
    {ROCEE_TRP_EMPTY_1_LABEL, 0x10b0},
    {ROCEE_TRP_EMPTY_2_LABEL, 0x10b4},
    {ROCEE_TRP_EMPTY_3_LABEL, 0x10b8},
    {ROCEE_TRP_EMPTY_4_LABEL, 0x10bc},
    {ROCEE_TRP_FULL_0_LABEL, 0x10c0},
    {ROCEE_TRP_FULL_1_LABEL, 0x10c4},
    {ROCEE_TRP_FULL_2_LABEL, 0x10c8},
    {ROCEE_TRP_FULL_3_LABEL, 0x10cc},
    {ROCEE_TRP_FSM_LABEL, 0x10f0},
    {TRP_RX_CNP_CNT_LABEL, 0x163c},
    {TRP_SCC_CNP_CNT_LABEL, 0x1640},
    {TRP_INNER_STA_0_LABEL, 0x1654},
    {TRP_INNER_STA_1_LABEL, 0x1658},
    {TRP_INNER_STA_2_LABEL, 0x165c},
    {TRP_INNER_STA_3_LABEL, 0x1660},
    {TRP_INNER_STA_4_LABEL, 0x1664},
    {TRP_INNER_STA_5_LABEL, 0x1668},
    {TRP_INNER_STA_6_LABEL, 0x166c},
    {TRP_INNER_STA_7_LABEL, 0x1670},
    {TRP_INNER_STA_8_LABEL, 0x1674},
    {TRP_RX_CQE_CNT_0_LABEL, 0x1680},
    {TRP_RX_CQE_CNT_1_LABEL, 0x1684},
    {TRP_RX_CQE_CNT_2_LABEL, 0x1688},
    {TRP_RX_CQE_CNT_3_LABEL, 0x168c},

    /* TSP regs */
    {ROCEE_TPP_STA1_LABEL, 0x1300},
    {ROCEE_TPP_STA2_LABEL, 0x1304},
    {ROCEE_TPP_STA3_LABEL, 0x1308},
    {ROCEE_TPP_STA4_LABEL, 0x130c},
    {ROCEE_TPP_STA5_LABEL, 0x1310},
    {ROCEE_TPP_STA6_LABEL, 0x1314},
    {ROCEE_TPP_STA7_LABEL, 0x1318},
    {ROCEE_TWP_STA1_LABEL, 0x141c},
    {ROCEE_TDP_STA1_LABEL, 0x1420},
    {ROCEE_TPP_STA_RSV0_LABEL, 0x14b8},
    {ROCEE_TPP_STA_RSV1_LABEL, 0x14bc},
    {ROCEE_TPP_STA_RSV2_LABEL, 0x14c0},
    {ROCEE_TPP_STA_RSV3_LABEL, 0x14c4},
    {TWP_TC_HDR_XOFF_LABEL, 0x153c},
    {TWP_TC_ATM_XOFF_LABEL, 0x1540},
    {TSP_SGE_ERR_DROP_LEN_LABEL, 0x16a8},
    {TSP_SGE_AXI_CNT_LABEL, 0x16ac},
    {ROCEE_TDP_TRP_CNT_LABEL, 0x1464},
    {ROCEE_TDP_MDB_CNT_LABEL, 0x1468},
    {ROCEE_TDP_EXT_DEQ_CNT_LABEL, 0x146c},
    {ROCEE_TDP_LP_CNT_LABEL, 0x1470},
    {ROCEE_TDP_QMM_CNT_LABEL, 0x1474},
    {ROCEE_TDP_EXT_ENQ_CNT_LABEL, 0x1478},
    {ROCEE_TDP_TWP_CNT0_LABEL, 0x147c},
    {ROCEE_TDP_TWP_CNT1_LABEL, 0x1480},
    {ROCEE_TDP_TWP_CNT2_LABEL, 0x1484},
    {ROCEE_TDP_SCC_CNT_LABEL, 0x1488},
    {ROCEE_SCC_TDP_CNT_LABEL, 0x148c},
    {ROCEE_TDP_TM_CNT_LABEL, 0x1490},
    {ROCEE_TM_TDP_CNT_LABEL, 0x1494},

    /* CNT regs */
    {CAEP_TRP_AE_CNT_I_LABEL, 0x151c},
    {CAEP_MDB_AE_CNT_I_LABEL, 0x1520},
    {CAEP_QMM_CE_CNT_I_LABEL, 0x1524},
    {CAEP_QMM_CE_VLD_CNT_I_LABEL, 0x1528},
    {CAEP_VFT_ERR_CNT_O_LABEL, 0x152c},
    {CAEP_ACE_DISCARD_CNT_O_LABEL, 0x1530},
    {CAEP_ACE_VLD_CNT_O_LABEL, 0x1534},
    {ROCEE_MBX_ISSUE_CNT_LABEL, 0x02dc},
    {ROCEE_MBX_EXEC_CNT_LABEL, 0x02e0},
    {ROCEE_DB_ISSUE_CNT_LABEL, 0x02e4},
    {ROCEE_DB_EXEC_CNT_LABEL, 0x02e8},
    {ROCEE_EQDB_ISSUE_CNT_LABEL, 0x02ec},
    {ROCEE_EQDB_EXEC_CNT_LABEL, 0x02f0},
};

static int hns_roce_v2_mbox_query_cmd(struct hns_roce_dev *hr_dev, struct hns_roce_cmd_mailbox *mailbox, 
                                      int index, u16 op)
{
    struct hns_roce_mbox_post_params mbox_pst_params = {0};

    mbox_pst_params.in_param = 0;
    mbox_pst_params.out_param = mailbox->dma;
    mbox_pst_params.in_modifier = index;
    mbox_pst_params.op_modifier = 0;
    mbox_pst_params.op = op;
    return hns_roce_cmd_mbox(hr_dev, mbox_pst_params, HNS_ROCE_CMD_TIMEOUT_MSECS);
}

static int hns_roce_v2_query_mpt_bt(struct hns_roce_dev *hr_dev, struct hns_roce_cmd_mailbox *mailbox, int key,
                                    u64 *bt0_ba, u64 *bt1_ba)
{
    int ret;

    ret = hns_roce_v2_mbox_query_cmd(hr_dev, mailbox, key, HNS_ROCE_CMD_READ_MPT_BT0);
    if (ret) {
        dev_err(hr_dev->dev, "hns_roce read mpt bt op[%d] failed\n", HNS_ROCE_CMD_READ_MPT_BT0);
        return ret;
    }

    ret = memcpy_s(bt0_ba, sizeof(u64), mailbox->buf, sizeof(u64));
    if (ret) {
        dev_err(hr_dev->dev, "%s, %d: memcpy_s failed, ret[%d]\n", __func__, __LINE__, ret);
        return ret;
    }

    ret = hns_roce_v2_mbox_query_cmd(hr_dev, mailbox, key, HNS_ROCE_CMD_READ_MPT_BT1);
    if (ret) {
        dev_err(hr_dev->dev, "hns_roce read mpt bt op[%d] failed\n", HNS_ROCE_CMD_READ_MPT_BT1);
        return ret;
    }

    ret = memcpy_s(bt1_ba, sizeof(u64), mailbox->buf, sizeof(u64));
    if (ret) {
        dev_err(hr_dev->dev, "%s, %d: memcpy_s failed, ret[%d]\n", __func__, __LINE__, ret);
        return ret;
    }

    return 0;
}

static int hns_roce_v2_query_mpt_ctx(struct hns_roce_dev *hr_dev, struct hns_roce_cmd_mailbox *mailbox, int key,
                                     struct hns_roce_v2_mpt_entry *mpt_ctx)
{
    int ret;

    ret = hns_roce_v2_mbox_query_cmd(hr_dev, mailbox, key, HNS_ROCE_CMD_QUERY_MPT);
    if (ret) {
        dev_err(hr_dev->dev, "hns_roce cmd mbox op[%d] failed, key[0x%x]\n", HNS_ROCE_CMD_QUERY_MPT, key);
        return ret;
    }

    ret = memcpy_s(mpt_ctx, sizeof(*mpt_ctx), mailbox->buf, sizeof(*mpt_ctx));
    if (ret) {
        dev_err(hr_dev->dev, "%s, %d: memcpy_s failed, ret[%d]\n", __func__, __LINE__, ret);
        return ret;
    }

    return 0;
}

static void hns_roce_v2_mpt_info_print(struct hns_roce_dev *hr_dev,
    struct hns_roce_v2_mpt_entry *mpt_ctx)
{
    dev_notice(hr_dev->dev, "hns roce v2 mpt info print!!!");
    dev_notice(hr_dev->dev, "mpt_st:0x%x\t", mpt_ctx->byte_4_pd_hop_st & 0x3);
    dev_notice(hr_dev->dev, "pbl_hop_num:0x%x\t", (mpt_ctx->byte_4_pd_hop_st >> 0x2) & 0x3);
    dev_notice(hr_dev->dev, "pbl_ba_pg_sz:0x%x\t", (mpt_ctx->byte_4_pd_hop_st >> 0x4) & 0xF);
    dev_notice(hr_dev->dev, "pd:0x%x\t", (mpt_ctx->byte_4_pd_hop_st >> 0x8) & 0xFFFFFF);
    dev_notice(hr_dev->dev, "ra_en:0x%x\t", mpt_ctx->byte_8_mw_cnt_en & 0x1);
    dev_notice(hr_dev->dev, "r_inv_en:0x%x\t", (mpt_ctx->byte_8_mw_cnt_en >> 0x1) & 0x1);
    dev_notice(hr_dev->dev, "l_inv_en:0x%x\t", (mpt_ctx->byte_8_mw_cnt_en >> 0x2) & 0x1);
    dev_notice(hr_dev->dev, "bind_en:0x%x\t", (mpt_ctx->byte_8_mw_cnt_en >> 0x3) & 0x1);
    dev_notice(hr_dev->dev, "atomic_en:0x%x\t", (mpt_ctx->byte_8_mw_cnt_en >> 0x4) & 0x1);
    dev_notice(hr_dev->dev, "rr_en:0x%x\t", (mpt_ctx->byte_8_mw_cnt_en >> 0x5) & 0x1);
    dev_notice(hr_dev->dev, "rw_en:0x%x\t", (mpt_ctx->byte_8_mw_cnt_en >> 0x6) & 0x1);
    dev_notice(hr_dev->dev, "lw_en:0x%x\t", (mpt_ctx->byte_8_mw_cnt_en >> 0x7) & 0x1);
    dev_notice(hr_dev->dev, "mw_cnt:0x%x\t", (mpt_ctx->byte_8_mw_cnt_en >> 0x8) & 0xFFFFFF);
    dev_notice(hr_dev->dev, "fre:0x%x\t", mpt_ctx->byte_12_mw_pa & 0x1);
    dev_notice(hr_dev->dev, "pa:0x%x\t", (mpt_ctx->byte_12_mw_pa >> 0x1) & 0x1);
    dev_notice(hr_dev->dev, "zbva:0x%x\t", (mpt_ctx->byte_12_mw_pa >> 0x2) & 0x1);
    dev_notice(hr_dev->dev, "share:0x%x\t", (mpt_ctx->byte_12_mw_pa >> 0x3) & 0x1);
    dev_notice(hr_dev->dev, "mr_mw:0x%x\t", (mpt_ctx->byte_12_mw_pa >> 0x4) & 0x1);
    dev_notice(hr_dev->dev, "bpd:0x%x\t", (mpt_ctx->byte_12_mw_pa >> 0x5) & 0x1);
    dev_notice(hr_dev->dev, "bqp:0x%x\t", (mpt_ctx->byte_12_mw_pa >> 0x6) & 0x1);
    dev_notice(hr_dev->dev, "inner_pa_vld:0x%x\t", (mpt_ctx->byte_12_mw_pa >> 0x7) & 0x1);
    dev_notice(hr_dev->dev, "mw_bind_qpn:0x%x\t", (mpt_ctx->byte_12_mw_pa >> 0x8) & 0xFFFFFF);
    dev_notice(hr_dev->dev, "bound_lkey:0x%x\t", mpt_ctx->bound_lkey);
    dev_notice(hr_dev->dev, "len_l:0x%x\t", mpt_ctx->len_l);
    dev_notice(hr_dev->dev, "len_h:0x%x\t", mpt_ctx->len_h);
    dev_notice(hr_dev->dev, "lkey:0x%x\t", mpt_ctx->lkey);
    dev_notice(hr_dev->dev, "va_l:0x%x\t", mpt_ctx->va_l);
    dev_notice(hr_dev->dev, "va_h:0x%x\t", mpt_ctx->va_h);
    dev_notice(hr_dev->dev, "pbl_size:0x%x\t", mpt_ctx->pbl_size);
    dev_notice(hr_dev->dev, "pbl_ba_l:0x%x\t", mpt_ctx->pbl_ba_l);
    dev_notice(hr_dev->dev, "pbl_ba_h:0x%x\t", mpt_ctx->byte_48_mode_ba & 0x1FFFFFFF);
    dev_notice(hr_dev->dev, "blk_mode:0x%x\t", (mpt_ctx->byte_48_mode_ba >> 0x1D) & 0x1);
    dev_notice(hr_dev->dev, "rsv0:0x%x\t", (mpt_ctx->byte_48_mode_ba >> 0x1E) & 0x3);
    dev_notice(hr_dev->dev, "pa0_l:0x%x\t", mpt_ctx->pa0_l);
    dev_notice(hr_dev->dev, "pa0_h:0x%x\t", mpt_ctx->byte_56_pa0_h & 0x3FFFFFF);
    dev_notice(hr_dev->dev, "bound_va:0x%x\t", (mpt_ctx->byte_56_pa0_h >> 0x1A) & 0x3F);
    dev_notice(hr_dev->dev, "pa1_l:0x%x\t", mpt_ctx->pa1_l);
    dev_notice(hr_dev->dev, "pa1_h:0x%x\t", mpt_ctx->byte_64_buf_pa1 & 0x3FFFFFF);
    dev_notice(hr_dev->dev, "rsv2:0x%x\t", (mpt_ctx->byte_64_buf_pa1 >> 0x1A) & 0x3);
    dev_notice(hr_dev->dev, "pbl_buf_pg_sz:0x%x\t", (mpt_ctx->byte_64_buf_pa1 >> 0x1C) & 0xF);
}

int hns_roce_v2_query_mpt_stat(struct hns_roce_dev *hr_dev, char *buf, int *desc)
{
    struct hns_roce_v2_mpt_entry *mpt_ctx = NULL;
    struct hns_roce_cmd_mailbox *mailbox;
    int key = hr_dev->hr_stat.key;
    int cur_len = 0;
    char *out = buf;
    u64 bt0_ba = 0;
    u64 bt1_ba = 0;
    int *mpt = NULL;
    int ret, i;

    mailbox = hns_roce_alloc_cmd_mailbox(hr_dev);
    HNS_ROCE_ALLOC_CMD_MAILBOX_RET_CHECK(mailbox);

    ret = hns_roce_v2_query_mpt_bt(hr_dev, mailbox, key, &bt0_ba, &bt1_ba);
    if (ret) {
        dev_err(hr_dev->dev, "query mpt bt failed, ret[%d], key[0x%x]\n", ret, key);
        goto free_mailbox;
    }

    mpt_ctx = kzalloc(sizeof(*mpt_ctx), GFP_KERNEL);
    if (mpt_ctx == NULL) {
        ret = -ENOMEM;
        dev_err(hr_dev->dev, "kzalloc hns_roce_v2_mpt_entry failed, size[%lu]\n", sizeof(*mpt_ctx));
        goto free_mailbox;
    }

    ret = hns_roce_v2_query_mpt_ctx(hr_dev, mailbox, key, mpt_ctx);
    if (ret) {
        dev_err(hr_dev->dev, "query mpt ctx failed, ret[%d], key[0x%x]\n", ret, key);
        goto free_ctx;
    }

    hns_roce_v2_mpt_info_print(hr_dev, mpt_ctx);
    mpt = (int *)mpt_ctx;
    HNS_ROCE_SYSFS_MPT_PRINT(mpt);
    *desc += cur_len;

free_ctx:
    kfree(mpt_ctx);
    mpt_ctx = NULL;

free_mailbox:
    hns_roce_free_cmd_mailbox(hr_dev, mailbox);

    return ret;
}

static int hns_roce_v2_read_srqc_bt(struct hns_roce_dev *hr_dev, struct hns_roce_cmd_mailbox *mailbox, int srqn,
                                    u64 *bt0_ba)
{
    int ret;

    ret = hns_roce_v2_mbox_query_cmd(hr_dev, mailbox, srqn, HNS_ROCE_CMD_READ_SRQC_BT0);
    if (ret) {
        dev_err(hr_dev->dev, "hns_roce read srq bt op[%d] failed\n", HNS_ROCE_CMD_READ_MPT_BT0);
        return ret;
    }

    ret = memcpy_s(bt0_ba, sizeof(u64), mailbox->buf, sizeof(u64));
    if (ret) {
        dev_err(hr_dev->dev, "%s, %d: memcpy_s failed, ret[%d]\n", __func__, __LINE__, ret);
        return ret;
    }

    return 0;
}

static int hns_roce_v2_query_srqc_ctx(struct hns_roce_dev *hr_dev, struct hns_roce_cmd_mailbox *mailbox, int srqn,
                                      struct hns_roce_srq_context *srq_ctx)
{
    int ret;

    ret = hns_roce_v2_mbox_query_cmd(hr_dev, mailbox, srqn, HNS_ROCE_CMD_QUERY_SRQC);
    if (ret) {
        dev_err(hr_dev->dev, "hns_roce cmd mbox op[%d] failed, srqn[0x%x]\n", HNS_ROCE_CMD_QUERY_SRQC, srqn);
        return ret;
    }

    ret = memcpy_s(srq_ctx, sizeof(*srq_ctx), mailbox->buf, sizeof(*srq_ctx));
    if (ret) {
        dev_err(hr_dev->dev, "%s, %d: memcpy_s failed, ret[%d]\n", __func__, __LINE__, ret);
        return ret;
    }

    return 0;
}

int hns_roce_v2_query_srqc_stat(struct hns_roce_dev *hr_dev, char *buf, int *desc)
{
    struct hns_roce_srq_context *srq_context = NULL;
    struct hns_roce_cmd_mailbox *mailbox = NULL;
    int srqn = hr_dev->hr_stat.srqn;
    int cur_len = 0;
    char *out = buf;
    u64 bt0_ba = 0;
    u64 bt1_ba = 0;
    int *srqc = NULL;
    int i = 0;
    int ret;

    mailbox = hns_roce_alloc_cmd_mailbox(hr_dev);
    HNS_ROCE_ALLOC_CMD_MAILBOX_RET_CHECK(mailbox);

    ret = hns_roce_v2_read_srqc_bt(hr_dev, mailbox, srqn, &bt0_ba);
    if (ret) {
        dev_err(hr_dev->dev, "query srq bt failed, ret[%d], srqn[0x%x]\n", ret, srqn);
        goto free_mailbox;
    }

    srq_context = kzalloc(sizeof(*srq_context), GFP_KERNEL);
    if (srq_context == NULL) {
        ret = -ENOMEM;
        dev_err(hr_dev->dev, "kzalloc hns_roce_srq_context failed, size[%lu]\n", sizeof(*srq_context));
        goto free_mailbox;
    }

    ret = hns_roce_v2_query_srqc_ctx(hr_dev, mailbox, srqn, srq_context);
    if (ret) {
        dev_err(hr_dev->dev, "query srq ctx failed, ret[%d], srqn[0x%x]\n", ret, srqn);
        goto free_ctx;
    }

    srqc = (int *)srq_context;
    HNS_ROCE_SYSFS_SRQC_PRINT(srqc);
    *desc += cur_len;

free_ctx:
    kfree(srq_context);
    srq_context = NULL;

free_mailbox:
    hns_roce_free_cmd_mailbox(hr_dev, mailbox);

    return ret;
}

static void hns_roce_v2_qpc_dgid_print(struct hns_roce_dev *hr_dev,
    struct hns_roce_v2_qp_context *qp_context)
{
    u32 *tmp = (u32*)qp_context->dgid;
    dev_notice(hr_dev->dev, "dgid_l:0x%x\t", *tmp++);
    dev_notice(hr_dev->dev, "dgid_ml:0x%x\t", *tmp++);
    dev_notice(hr_dev->dev, "dgid_mh:0x%x\t", *tmp++);
    dev_notice(hr_dev->dev, "dgid_h:0x%x\t", *tmp);
}
static void hns_roce_v2_qpc_info4_print(struct hns_roce_dev *hr_dev,
    struct hns_roce_v2_qp_context *qp_context)
{
    dev_notice(hr_dev->dev, "irrl_ba_h:0x%x\t", qp_context->byte_208_irrl & 0x3FFFFFF);
    dev_notice(hr_dev->dev, "pkt_rnr_flg:0x%x\t", (qp_context->byte_208_irrl >> 0x1A) & 0x1);
    dev_notice(hr_dev->dev, "pkt_rty_flg:0x%x\t", (qp_context->byte_208_irrl >> 0x1B) & 0x1);
    dev_notice(hr_dev->dev, "rmt_e2e:0x%x\t", (qp_context->byte_208_irrl >> 0x1C) & 0x1);
    dev_notice(hr_dev->dev, "sr_max:0x%x\t", (qp_context->byte_208_irrl >> 0x1D) & 0x7);
    dev_notice(hr_dev->dev, "lsn:0x%x\t", qp_context->byte_212_lsn & 0xFFFFFF);
    dev_notice(hr_dev->dev, "retry_num_init:0x%x\t", (qp_context->byte_212_lsn >> 0x18) & 0x7);
    dev_notice(hr_dev->dev, "check_flg:0x%x\t", (qp_context->byte_212_lsn >> 0x1B) & 0x3);
    dev_notice(hr_dev->dev, "retry_cnt:0x%x\t", (qp_context->byte_212_lsn >> 0x1D) & 0x7);
    dev_notice(hr_dev->dev, "sq_timer:0x%x\t", qp_context->sq_timer);
    dev_notice(hr_dev->dev, "retry_msg_msn:0x%x\t", qp_context->byte_220_retry_psn_msn & 0xFFFF);
    dev_notice(hr_dev->dev, "retry_msg_psn_l:0x%x\t", (qp_context->byte_220_retry_psn_msn >> 0x10) & 0xFFFF);
    dev_notice(hr_dev->dev, "retry_msg_psn_h:0x%x\t", qp_context->byte_224_retry_msg & 0xFF);
    dev_notice(hr_dev->dev, "retry_msg_fpkt_psn:0x%x\t", (qp_context->byte_224_retry_msg >> 0x8) & 0xFFFFFF);
    dev_notice(hr_dev->dev, "rx_sq_cur_blk_addr_l:0x%x\t", qp_context->rx_sq_cur_blk_addr);
    dev_notice(hr_dev->dev, "rx_sq_cur_blk_addr_h:0x%x\t", qp_context->byte_232_irrl_sge & 0xFFFFF);
    dev_notice(hr_dev->dev, "irrl_sge_idx:0x%x\t", (qp_context->byte_232_irrl_sge >> 0x14) & 0x1FF);
    dev_notice(hr_dev->dev, "so_lp_vld:0x%x\t", (qp_context->byte_232_irrl_sge >> 0x1D) & 0x1);
    dev_notice(hr_dev->dev, "fence_lp_vld:0x%x\t", (qp_context->byte_232_irrl_sge >> 0x1E) & 0x1);
    dev_notice(hr_dev->dev, "irrl_lp_vld:0x%x\t", (qp_context->byte_232_irrl_sge >> 0x1F) & 0x1);
    dev_notice(hr_dev->dev, "irrl_cur_sge_offset:0x%x\t", qp_context->irrl_cur_sge_offset);
    dev_notice(hr_dev->dev, "irrl_tail_real:0x%x\t", qp_context->byte_240_irrl_tail & 0xFF);
    dev_notice(hr_dev->dev, "irrl_tail_rd:0x%x\t", (qp_context->byte_240_irrl_tail >> 0x8) & 0xFF);
    dev_notice(hr_dev->dev, "rx_ack_msn:0x%x\t", (qp_context->byte_240_irrl_tail >> 0x10) & 0xFFFF);
    dev_notice(hr_dev->dev, "rx_ack_epsn:0x%x\t", qp_context->byte_244_rnr_rxack & 0xFFFFFF);
    dev_notice(hr_dev->dev, "rnr_num_init:0x%x\t", (qp_context->byte_244_rnr_rxack >> 0x18) & 0x7);
    dev_notice(hr_dev->dev, "rnr_cnt:0x%x\t", (qp_context->byte_244_rnr_rxack >> 0x1B) & 0x7);
    dev_notice(hr_dev->dev, "lcl_op_flg:0x%x\t", (qp_context->byte_244_rnr_rxack >> 0x1E) & 0x1);
    dev_notice(hr_dev->dev, "irrl_rd_flg:0x%x\t", (qp_context->byte_244_rnr_rxack >> 0x1F) & 0x1);
    dev_notice(hr_dev->dev, "irrl_psn:0x%x\t", qp_context->byte_248_ack_psn & 0xFFFFFF);
    dev_notice(hr_dev->dev, "ack_psn_err:0x%x\t", (qp_context->byte_248_ack_psn >> 0x18) & 0x1);
    dev_notice(hr_dev->dev, "ack_last_optype:0x%x\t", (qp_context->byte_248_ack_psn >> 0x19) & 0x3);
    dev_notice(hr_dev->dev, "irrl_psn_vld:0x%x\t", (qp_context->byte_248_ack_psn >> 0x1B) & 0x1);
    dev_notice(hr_dev->dev, "rnr_retry_flag:0x%x\t", (qp_context->byte_248_ack_psn >> 0x1C) & 0x1);
    dev_notice(hr_dev->dev, "sq_rty_tx_err:0x%x\t", (qp_context->byte_248_ack_psn >> 0x1D) & 0x1);
    dev_notice(hr_dev->dev, "last_ind:0x%x\t", (qp_context->byte_248_ack_psn >> 0x1E) & 0x1);
    dev_notice(hr_dev->dev, "cq_err_ind:0x%x\t", (qp_context->byte_248_ack_psn >> 0x1F) & 0x1);
    dev_notice(hr_dev->dev, "tx_cqn:0x%x\t", qp_context->byte_252_err_txcqn & 0xFFFFFF);
    dev_notice(hr_dev->dev, "sig_type:0x%x\t", (qp_context->byte_252_err_txcqn >> 0x18) & 0x1);
    dev_notice(hr_dev->dev, "err_type:0x%x\t", (qp_context->byte_252_err_txcqn >> 0x19) & 0x7F);
    dev_notice(hr_dev->dev, "rq_cqe_idx:0x%x\t", qp_context->byte_256_sqflush_rqcqe & 0xFFFF);
    dev_notice(hr_dev->dev, "sq_flush_idx:0x%x\t", (qp_context->byte_256_sqflush_rqcqe >> 0x10) & 0xFFFF);
}
static void hns_roce_v2_qpc_info3_print(struct hns_roce_dev *hr_dev,
    struct hns_roce_v2_qp_context *qp_context)
{
    dev_notice(hr_dev->dev, "rq_rty_wait_do:0x%x\t", (qp_context->byte_140_raq >> 0xF) & 0x1);
    dev_notice(hr_dev->dev, "raq_trrl_head:0x%x\t", (qp_context->byte_140_raq >> 0x10) & 0xFF);
    dev_notice(hr_dev->dev, "raq_trrl_tail:0x%x\t", (qp_context->byte_140_raq >> 0x18) & 0xFF);
    dev_notice(hr_dev->dev, "raq_rty_ini_psn:0x%x\t", qp_context->byte_144_raq & 0xFFFFFF);
    dev_notice(hr_dev->dev, "rvd8:0x%x\t", (qp_context->byte_144_raq >> 0x18) & 0x1);
    dev_notice(hr_dev->dev, "raq_credit:0x%x\t", (qp_context->byte_144_raq >> 0x19) & 0x1F);
    dev_notice(hr_dev->dev, "rvd9:0x%x\t", (qp_context->byte_144_raq >> 0x1E) & 0x1);
    dev_notice(hr_dev->dev, "resp_rty_flg:0x%x\t", (qp_context->byte_144_raq >> 0x1F) & 0x1);
    dev_notice(hr_dev->dev, "raq_msn:0x%x\t", qp_context->byte_148_raq & 0xFFFFFF);
    dev_notice(hr_dev->dev, "raq_syndrome:0x%x\t", (qp_context->byte_148_raq >> 0x18) & 0xFF);
    dev_notice(hr_dev->dev, "raq_psn:0x%x\t", qp_context->byte_152_raq & 0xFFFFFF);
    dev_notice(hr_dev->dev, "raq_trrl_rty_head:0x%x\t", (qp_context->byte_152_raq >> 0x18) & 0xFF);
    dev_notice(hr_dev->dev, "raq_use_pktn:0x%x\t", qp_context->byte_156_raq & 0xFFFFFF);
    dev_notice(hr_dev->dev, "rq_scc_token:0x%x\t", (qp_context->byte_156_raq >> 0x18) & 0x3F);
    dev_notice(hr_dev->dev, "rvd10:0x%x\t", (qp_context->byte_156_raq >> 0x1E) & 0x3);
    dev_notice(hr_dev->dev, "sq_producer_idx:0x%x\t", qp_context->byte_160_sq_ci_pi & 0xFFFF);
    dev_notice(hr_dev->dev, "sq_consumer_idx:0x%x\t", (qp_context->byte_160_sq_ci_pi >> 0x10) & 0xFFFF);
    dev_notice(hr_dev->dev, "sq_cur_blk_addr_l:0x%x\t", qp_context->sq_cur_blk_addr);
    dev_notice(hr_dev->dev, "sq_cur_blk_addr_h:0x%x\t", qp_context->byte_168_irrl_idx & 0xFFFFF);
    dev_notice(hr_dev->dev, "msg_rty_lp_flg:0x%x\t", (qp_context->byte_168_irrl_idx >> 0x14) & 0x1);
    dev_notice(hr_dev->dev, "sq_invld_flg:0x%x\t", (qp_context->byte_168_irrl_idx >> 0x15) & 0x1);
    dev_notice(hr_dev->dev, "lp_sgen_ini:0x%x\t", (qp_context->byte_168_irrl_idx >> 0x16) & 0x3);
    dev_notice(hr_dev->dev, "sq_vlan_en:0x%x\t", (qp_context->byte_168_irrl_idx >> 0x18) & 0x1);
    dev_notice(hr_dev->dev, "poll_db_wait_do:0x%x\t", (qp_context->byte_168_irrl_idx >> 0x19) & 0x1);
    dev_notice(hr_dev->dev, "scc_token_forbid_sq_deq:0x%x\t", (qp_context->byte_168_irrl_idx >> 0x1A) & 0x1);
    dev_notice(hr_dev->dev, "wait_ack_timeout:0x%x\t", (qp_context->byte_168_irrl_idx >> 0x1B) & 0x1);
    dev_notice(hr_dev->dev, "irrl_idx_lsb:0x%x\t", (qp_context->byte_168_irrl_idx >> 0x1C) & 0xF);
    dev_notice(hr_dev->dev, "ack_req_freq:0x%x\t", qp_context->byte_172_sq_psn & 0x3F);
    dev_notice(hr_dev->dev, "msg_rnr_flg:0x%x\t", (qp_context->byte_172_sq_psn >> 0x6) & 0x1);
    dev_notice(hr_dev->dev, "fre:0x%x\t", (qp_context->byte_172_sq_psn >> 0x7) & 0x1);
    dev_notice(hr_dev->dev, "sq_cur_psn:0x%x\t", (qp_context->byte_172_sq_psn >> 0x8) & 0xFFFFFF);
    dev_notice(hr_dev->dev, "msg_use_pktn:0x%x\t", qp_context->byte_176_msg_pktn & 0xFFFFFF);
    dev_notice(hr_dev->dev, "irrl_head_pre:0x%x\t", (qp_context->byte_176_msg_pktn >> 0x18) & 0xFF);
    dev_notice(hr_dev->dev, "sq_cur_sge_blk_addr_l:0x%x\t", qp_context->sq_cur_sge_blk_addr);
    dev_notice(hr_dev->dev, "sq_cur_sge_blk_addr_h:0x%x\t", qp_context->byte_184_irrl_idx & 0xFFFFF);
    dev_notice(hr_dev->dev, "irrl_idx_msb:0x%x\t", (qp_context->byte_184_irrl_idx >> 0x14) & 0xFFF);
    dev_notice(hr_dev->dev, "cur_sge_offset:0x%x\t", qp_context->cur_sge_offset);
    dev_notice(hr_dev->dev, "cur_sge_idx:0x%x\t", qp_context->byte_192_ext_sge & 0xFFFFFF);
    dev_notice(hr_dev->dev, "ext_sge_num_left:0x%x\t", (qp_context->byte_192_ext_sge >> 0x18) & 0xFF);
    dev_notice(hr_dev->dev, "irrl_head:0x%x\t", qp_context->byte_196_sq_psn & 0xFF);
    dev_notice(hr_dev->dev, "sq_max_psn:0x%x\t", (qp_context->byte_196_sq_psn >> 0x8) & 0xFFFFFF);
    dev_notice(hr_dev->dev, "sq_max_idx:0x%x\t", qp_context->byte_200_sq_max & 0xFFFF);
    dev_notice(hr_dev->dev, "lcl_operated_cnt:0x%x\t", (qp_context->byte_200_sq_max >> 0x10) & 0xFFFF);
    dev_notice(hr_dev->dev, "irrl_ba_l:0x%x\t", qp_context->irrl_ba);
}
static void hns_roce_v2_qpc_info2_print(struct hns_roce_dev *hr_dev,
    struct hns_roce_v2_qp_context *qp_context)
{
    dev_notice(hr_dev->dev, "qkey_xrcd:0x%x\t", qp_context->qkey_xrcd);
    dev_notice(hr_dev->dev, "rq_record_en:0x%x\t", qp_context->byte_68_rq_db & 0x1);
    dev_notice(hr_dev->dev, "rq_db_record_addr_l:0x%x\t", (qp_context->byte_68_rq_db >> 1) & 0x7FFFFFFF);
    dev_notice(hr_dev->dev, "rq_db_record_addr_h:0x%x\t", qp_context->rq_db_record_addr);
    dev_notice(hr_dev->dev, "srqn:0x%x\t", qp_context->byte_76_srqn_op_en & 0xFFFFFF);
    dev_notice(hr_dev->dev, "srq_en:0x%x\t", (qp_context->byte_76_srqn_op_en >> 0x18) & 0x1);
    dev_notice(hr_dev->dev, "rre:0x%x\t", (qp_context->byte_76_srqn_op_en >> 0x19) & 0x1);
    dev_notice(hr_dev->dev, "rwe:0x%x\t", (qp_context->byte_76_srqn_op_en >> 0x1A) & 0x1);
    dev_notice(hr_dev->dev, "ate:0x%x\t", (qp_context->byte_76_srqn_op_en >> 0x1B) & 0x1);
    dev_notice(hr_dev->dev, "rqie:0x%x\t", (qp_context->byte_76_srqn_op_en >> 0x1C) & 0x1);
    dev_notice(hr_dev->dev, "ext_ate:0x%x\t", (qp_context->byte_76_srqn_op_en >> 0x1D) & 0x1);
    dev_notice(hr_dev->dev, "rq_vlan_en:0x%x\t", (qp_context->byte_76_srqn_op_en >> 0x1E) & 0x1);
    dev_notice(hr_dev->dev, "rq_rty_tx_err:0x%x\t", (qp_context->byte_76_srqn_op_en >> 0x1F) & 0x1);
    dev_notice(hr_dev->dev, "rx_cqn:0x%x\t", qp_context->byte_80_rnr_rx_cqn & 0xFFFFFF);
    dev_notice(hr_dev->dev, "rsv3:0x%x\t", (qp_context->byte_80_rnr_rx_cqn >> 0x18) & 0x7);
    dev_notice(hr_dev->dev, "min_rnr_time:0x%x\t", (qp_context->byte_80_rnr_rx_cqn >> 0x1B) & 0x1F);
    dev_notice(hr_dev->dev, "rq_producer_idx:0x%x\t", qp_context->byte_84_rq_ci_pi & 0xFFFF);
    dev_notice(hr_dev->dev, "rq_consumer_idx:0x%x\t", (qp_context->byte_84_rq_ci_pi >> 0x10) & 0xFFFF);
    dev_notice(hr_dev->dev, "rq_cur_blk_addr_l:0x%x\t", qp_context->rq_cur_blk_addr);
    dev_notice(hr_dev->dev, "rq_cur_blk_addr_h:0x%x\t", qp_context->byte_92_srq_info & 0xFFFFF);
    dev_notice(hr_dev->dev, "srq_info:0x%x\t", (qp_context->byte_92_srq_info >> 0x14) & 0xFFF);
    dev_notice(hr_dev->dev, "rx_req_msn:0x%x\t", qp_context->byte_96_rx_reqmsn & 0xFFFFFF);
    dev_notice(hr_dev->dev, "rsv5:0x%x\t", (qp_context->byte_96_rx_reqmsn >> 0x18) & 0xFF);
    dev_notice(hr_dev->dev, "rq_nxt_blk_addr_l:0x%x\t", qp_context->rq_nxt_blk_addr);
    dev_notice(hr_dev->dev, "rq_nxt_blk_addr_h:0x%x\t", qp_context->byte_104_rq_sge & 0xFFFFF);
    dev_notice(hr_dev->dev, "rsv6:0x%x\t", (qp_context->byte_104_rq_sge >> 0x14) & 0xF);
    dev_notice(hr_dev->dev, "rq_cur_wqe_sge_num:0x%x\t", (qp_context->byte_104_rq_sge >> 0x18) & 0xFF);
    dev_notice(hr_dev->dev, "inv_credit:0x%x\t", qp_context->byte_108_rx_reqepsn & 0x1);
    dev_notice(hr_dev->dev, "rsv7:0x%x\t", (qp_context->byte_108_rx_reqepsn >> 0x1) & 0x3);
    dev_notice(hr_dev->dev, "rx_req_psn_err:0x%x\t", (qp_context->byte_108_rx_reqepsn >> 0x3) & 0x1);
    dev_notice(hr_dev->dev, "rx_req_last_optype:0x%x\t", (qp_context->byte_108_rx_reqepsn >> 0x4) & 0x7);
    dev_notice(hr_dev->dev, "rx_req_rnr:0x%x\t", (qp_context->byte_108_rx_reqepsn >> 0x7) & 0x1);
    dev_notice(hr_dev->dev, "rx_req_epsn:0x%x\t", (qp_context->byte_108_rx_reqepsn >> 0x8) & 0xFFFFFF);
    dev_notice(hr_dev->dev, "rq_rnr_timer:0x%x\t", qp_context->rq_rnr_timer);
    dev_notice(hr_dev->dev, "rx_msg_len:0x%x\t", qp_context->rx_msg_len);
    dev_notice(hr_dev->dev, "rx_rkey_pkt_info:0x%x\t", qp_context->rx_rkey_pkt_info);
    dev_notice(hr_dev->dev, "rx_va:0x%llx\t", qp_context->rx_va);
    dev_notice(hr_dev->dev, "trrl_head_max:0x%x\t", qp_context->byte_132_trrl & 0xFF);
    dev_notice(hr_dev->dev, "trrl_tail_max:0x%x\t", (qp_context->byte_132_trrl >> 0x8) & 0xFF);
    dev_notice(hr_dev->dev, "trrl_ba_l:0x%x\t", (qp_context->byte_132_trrl >> 0x10) & 0xFFFF);
    dev_notice(hr_dev->dev, "trrl_ba_m:0x%x\t", qp_context->trrl_ba);
    dev_notice(hr_dev->dev, "trrl_ba_h:0x%x\t", qp_context->byte_140_raq & 0xFFF);
    dev_notice(hr_dev->dev, "rr_max:0x%x\t", (qp_context->byte_140_raq >> 0xC) & 0x7);
}

static void hns_roce_v2_qpc_info_print(struct hns_roce_dev *hr_dev,
    struct hns_roce_v2_qp_context *qp_context)
{
    dev_notice(hr_dev->dev, "hns roce v2 qpc info print!!!");
    dev_notice(hr_dev->dev, "tst:0x%x\t", qp_context->byte_4_sqpn_tst & 0x7);
    dev_notice(hr_dev->dev, "sge_shift:0x%x\t", (qp_context->byte_4_sqpn_tst >> 0x3) & 0x1F);
    dev_notice(hr_dev->dev, "cnp_timer:0x%x\t", (qp_context->byte_4_sqpn_tst >> 0x8) & 0xFFFFFF);
    dev_notice(hr_dev->dev, "wqe_sge_ba_l:0x%x\t", qp_context->wqe_sge_ba);
    dev_notice(hr_dev->dev, "wqe_sge_ba_h:0x%x\t", qp_context->byte_12_sq_hop & 0x1FFFFFFF);
    dev_notice(hr_dev->dev, "sq_hop_num:0x%x\t", (qp_context->byte_12_sq_hop >> 0x1D) & 0x3);
    dev_notice(hr_dev->dev, "rsvd_lkey_en:0x%x\t", (qp_context->byte_12_sq_hop >> 0x1F) & 0x1);
    dev_notice(hr_dev->dev, "wqe_sge_ba_pg_sz:0x%x\t", qp_context->byte_16_buf_ba_pg_sz & 0xF);
    dev_notice(hr_dev->dev, "wqe_sge_buf_pg_sz:0x%x\t", (qp_context->byte_16_buf_ba_pg_sz >> 0x4) & 0xF);
    dev_notice(hr_dev->dev, "pd:0x%x\t", (qp_context->byte_16_buf_ba_pg_sz >> 0x8) & 0xFFFFFF);
    dev_notice(hr_dev->dev, "rq_hop_num:0x%x\t", qp_context->byte_20_smac_sgid_idx & 0x3);
    dev_notice(hr_dev->dev, "sge_hop_num:0x%x\t", (qp_context->byte_20_smac_sgid_idx >> 0x2) & 0x3);
    dev_notice(hr_dev->dev, "rqws:0x%x\t", (qp_context->byte_20_smac_sgid_idx >> 0x4) & 0xF);
    dev_notice(hr_dev->dev, "sq_shift:0x%x\t", (qp_context->byte_20_smac_sgid_idx >> 0x8) & 0xF);
    dev_notice(hr_dev->dev, "rq_shift:0x%x\t", (qp_context->byte_20_smac_sgid_idx >> 0xC) & 0xF);
    dev_notice(hr_dev->dev, "sgid_idx:0x%x\t", (qp_context->byte_20_smac_sgid_idx >> 0x10) & 0xFF);
    dev_notice(hr_dev->dev, "smac_idx:0x%x\t", (qp_context->byte_20_smac_sgid_idx >> 0x18) & 0xFF);
    dev_notice(hr_dev->dev, "hoplimit:0x%x\t", qp_context->byte_24_mtu_tc & 0xFF);
    dev_notice(hr_dev->dev, "tc:0x%x\t", (qp_context->byte_24_mtu_tc >> 0x8) & 0xFF);
    dev_notice(hr_dev->dev, "vlan_id:0x%x\t", (qp_context->byte_24_mtu_tc >> 0x10) & 0xFFF);
    dev_notice(hr_dev->dev, "mtu:0x%x\t", (qp_context->byte_24_mtu_tc >> 0x1C) & 0xF);
    dev_notice(hr_dev->dev, "fl:0x%x\t", qp_context->byte_28_at_fl & 0xFFFFF);
    dev_notice(hr_dev->dev, "sl:0x%x\t", (qp_context->byte_28_at_fl >> 0x14) & 0xF);
    dev_notice(hr_dev->dev, "cnp_tx_flag:0x%x\t", (qp_context->byte_28_at_fl >> 0x18) & 0x1);
    dev_notice(hr_dev->dev, "ce_flag:0x%x\t", (qp_context->byte_28_at_fl >> 0x19) & 0x1);
    dev_notice(hr_dev->dev, "lbi:0x%x\t", (qp_context->byte_28_at_fl >> 0x1A) & 0x1);
    dev_notice(hr_dev->dev, "at:0x%x\t", (qp_context->byte_28_at_fl >> 0x1B) & 0x1F);
    hns_roce_v2_qpc_dgid_print(hr_dev, qp_context);
    dev_notice(hr_dev->dev, "dmac_l:0x%x\t", qp_context->dmac);
    dev_notice(hr_dev->dev, "dmac_h:0x%x\t", qp_context->byte_52_udpspn_dmac & 0xFFFF);
    dev_notice(hr_dev->dev, "udpspn:0x%x\t", (qp_context->byte_52_udpspn_dmac >> 0x10) & 0xFFFF);
    dev_notice(hr_dev->dev, "dqpn:0x%x\t", qp_context->byte_56_dqpn_err & 0xFFFFFF);
    dev_notice(hr_dev->dev, "sq_tx_err:0x%x\t", (qp_context->byte_56_dqpn_err >> 0x18) & 0x1);
    dev_notice(hr_dev->dev, "sq_rx_err:0x%x\t", (qp_context->byte_56_dqpn_err >> 0x19) & 0x1);
    dev_notice(hr_dev->dev, "rq_tx_err:0x%x\t", (qp_context->byte_56_dqpn_err >> 0x1A) & 0x1);
    dev_notice(hr_dev->dev, "rq_rx_err:0x%x\t", (qp_context->byte_56_dqpn_err >> 0x1B) & 0x1);
    dev_notice(hr_dev->dev, "lp_pktn_ini:0x%x\t", (qp_context->byte_56_dqpn_err >> 0x1C) & 0xF);
    dev_notice(hr_dev->dev, "tempid:0x%x\t", qp_context->byte_60_qpst_tempid & 0xFF);
    dev_notice(hr_dev->dev, "scc_token:0x%x\t", (qp_context->byte_60_qpst_tempid >> 0x8) & 0x7FFFF);
    dev_notice(hr_dev->dev, "sq_db_doing:0x%x\t", (qp_context->byte_60_qpst_tempid >> 0x1B) & 0x1);
    dev_notice(hr_dev->dev, "rq_db_doing:0x%x\t", (qp_context->byte_60_qpst_tempid >> 0x1C) & 0x1);
    dev_notice(hr_dev->dev, "qp_st:0x%x\t", (qp_context->byte_60_qpst_tempid >> 0x1D) & 0x7);
}

static void hns_roce_v2_qpc_ctx_info_print(struct hns_roce_dev *hr_dev,
    struct hns_roce_v2_qp_context *qp_context)
{
    hns_roce_v2_qpc_info_print(hr_dev, qp_context);
    hns_roce_v2_qpc_info2_print(hr_dev, qp_context);
    hns_roce_v2_qpc_info3_print(hr_dev, qp_context);
    hns_roce_v2_qpc_info4_print(hr_dev, qp_context);
}

static int hns_roce_v2_query_qpc_bt(struct hns_roce_dev *hr_dev, struct hns_roce_cmd_mailbox *mailbox, int qpn,
                                    u64 *bt0_ba, u64 *bt1_ba)
{
    int ret;

    ret = hns_roce_v2_mbox_query_cmd(hr_dev, mailbox, qpn, HNS_ROCE_CMD_READ_QPC_BT0);
    if (ret) {
        dev_err(hr_dev->dev, "hns_roce cmd mbox op[%d] failed\n", HNS_ROCE_CMD_READ_QPC_BT0);
        return ret;
    }

    ret = memcpy_s(bt0_ba, sizeof(u64), mailbox->buf, sizeof(u64));
    if (ret) {
        dev_err(hr_dev->dev, "%s, %d: memcpy_s failed, ret[%d]\n", __func__, __LINE__, ret);
        return ret;
    }

    ret = hns_roce_v2_mbox_query_cmd(hr_dev, mailbox, qpn, HNS_ROCE_CMD_READ_QPC_BT1);
    if (ret) {
        dev_err(hr_dev->dev, "hns_roce cmd mbox op[%d] failed\n", HNS_ROCE_CMD_READ_QPC_BT1);
        return ret;
    }

    ret = memcpy_s(bt1_ba, sizeof(u64), mailbox->buf, sizeof(u64));
    if (ret) {
        dev_err(hr_dev->dev, "%s, %d: memcpy_s failed, ret[%d]\n", __func__, __LINE__, ret);
        return ret;
    }

    return 0;
}

static int hns_roce_v2_query_qpc_ctx(struct hns_roce_dev *hr_dev, struct hns_roce_cmd_mailbox *mailbox, int qpn,
                                     struct hns_roce_v2_qp_context *qp_ctx)
{
    int ret;

    ret = hns_roce_v2_mbox_query_cmd(hr_dev, mailbox, qpn, HNS_ROCE_CMD_QUERY_QPC);
    if (ret) {
        dev_err(hr_dev->dev, "hns_roce cmd mbox op[%d] failed, qpn[0x%x]\n", HNS_ROCE_CMD_QUERY_QPC, qpn);
        return ret;
    }

    ret = memcpy_s(qp_ctx, sizeof(*qp_ctx), mailbox->buf, sizeof(*qp_ctx));
    if (ret) {
        dev_err(hr_dev->dev, "memcpy_s failed\n");
        return ret;
    }

    return 0;
}

int hns_roce_v2_query_qpc_stat(struct hns_roce_dev *hr_dev, char *buf, int *desc)
{
    struct hns_roce_cmd_mailbox *mailbox = NULL;
    struct hns_roce_v2_qp_context *qp_context = NULL;
    int qpn = hr_dev->hr_stat.qpn;
    int cur_len = 0;
    char *out = buf;
    u64 bt0_ba = 0;
    u64 bt1_ba = 0;
    int *qpc = NULL;
    int i, ret;

    mailbox = hns_roce_alloc_cmd_mailbox(hr_dev);
    HNS_ROCE_ALLOC_CMD_MAILBOX_RET_CHECK(mailbox);

    ret = hns_roce_v2_query_qpc_bt(hr_dev, mailbox, qpn, &bt0_ba, &bt1_ba);
    if (ret) {
        dev_err(hr_dev->dev, "query qpc bt failed, ret[%d], qpn[0x%x]\n", ret, qpn);
        goto free_mailbox;
    }

    qp_context = kzalloc(sizeof(*qp_context), GFP_KERNEL);
    if (qp_context == NULL) {
        ret = -ENOMEM;
        dev_err(hr_dev->dev, "kzalloc hns_roce_v2_qp_context failed, size[%lu]\n", sizeof(*qp_context));
        goto free_mailbox;
    }

    ret = hns_roce_v2_query_qpc_ctx(hr_dev, mailbox, qpn, qp_context);
    if (ret) {
        dev_err(hr_dev->dev, "query qp ctx failed, ret[%d], qpn[0x%x]\n", ret, qpn);
        goto free_ctx;
    }

    hns_roce_v2_qpc_ctx_info_print(hr_dev, qp_context);
    qpc = (int *)qp_context;
    HNS_ROCE_SYSFS_QPC_PRINT(qpc);
    *desc += cur_len;

free_ctx:
    kfree(qp_context);
    qp_context = NULL;

free_mailbox:
    hns_roce_free_cmd_mailbox(hr_dev, mailbox);

    return ret;
}

static void hns_roce_v2_eqc_info_print(struct hns_roce_dev *hr_dev,
    struct hns_roce_eq_context *eq_context, bool flag)
{
    if (flag == 0) {
        dev_notice(hr_dev->dev, "hns roce v2 aeqc info print!!!");
    } else {
        dev_notice(hr_dev->dev, "hns roce v2 ceqc info print!!!");
    }
    dev_notice(hr_dev->dev, "eq_st:0x%x\t", eq_context->byte_4 & 0x3);
    dev_notice(hr_dev->dev, "eqe_hop_num:0x%x\t", (eq_context->byte_4 >> 0x2) & 0x3);
    dev_notice(hr_dev->dev, "over_ignore:0x%x\t", (eq_context->byte_4 >> 0x4) & 0x1);
    dev_notice(hr_dev->dev, "coalesce:0x%x\t", (eq_context->byte_4 >> 0x5) & 0x1);
    dev_notice(hr_dev->dev, "arm_st:0x%x\t", (eq_context->byte_4 >> 0x6) & 0x3);
    dev_notice(hr_dev->dev, "eqn:0x%x\t", (eq_context->byte_4 >> 0x8) & 0xFF);
    dev_notice(hr_dev->dev, "eqe_cnt:0x%x\t", (eq_context->byte_4 >> 0x10) & 0xFFFF);
    dev_notice(hr_dev->dev, "eqe_ba_pg_sz:0x%x\t", eq_context->byte_8 & 0xF);
    dev_notice(hr_dev->dev, "eqe_buf_pg_sz:0x%x\t", (eq_context->byte_8 >> 0x4) & 0xF);
    dev_notice(hr_dev->dev, "eq_producer_idx:0x%x\t", (eq_context->byte_8 >> 0x8) & 0xFFFFFF);
    dev_notice(hr_dev->dev, "eq_max_cnt:0x%x\t", eq_context->byte_12 & 0xFFFF);
    dev_notice(hr_dev->dev, "eq_max_cnt:0x%x\t", (eq_context->byte_12 >> 0x10) & 0xFFFF);
    dev_notice(hr_dev->dev, "eqe_report_timer:0x%x\t", eq_context->eqe_report_timer);
    dev_notice(hr_dev->dev, "eqe_ba_l:0x%x\t", eq_context->eqe_ba0);
    dev_notice(hr_dev->dev, "eqe_ba_h:0x%x\t", eq_context->eqe_ba1 & 0x1FFFFFFF);
    dev_notice(hr_dev->dev, "rsv0:0x%x\t", (eq_context->eqe_ba1 >> 0x1D) & 0x7);
    dev_notice(hr_dev->dev, "shift:0x%x\t", eq_context->byte_28 & 0xFF);
    dev_notice(hr_dev->dev, "msi_idx:0x%x\t", (eq_context->byte_28 >> 0x8) & 0xFF);
    dev_notice(hr_dev->dev, "cur_eqe_ba_l:0x%x\t", (eq_context->byte_28 >> 0x10) & 0xFFFF);
    dev_notice(hr_dev->dev, "cur_eqe_ba_m:0x%x\t", eq_context->byte_32);
    dev_notice(hr_dev->dev, "cur_eqe_ba_h:0x%x\t", eq_context->byte_36 & 0xF);
    dev_notice(hr_dev->dev, "rsv1:0x%x\t", (eq_context->byte_36 >> 0x4) & 0xF);
    dev_notice(hr_dev->dev, "eq_consumer_idx:0x%x\t", (eq_context->byte_36 >> 0x8) & 0xFFFFFF);
    dev_notice(hr_dev->dev, "nxt_eqe_ba_l:0x%x\t", eq_context->nxt_eqe_ba0);
    dev_notice(hr_dev->dev, "nxt_eqe_ba_h:0x%x\t", eq_context->nxt_eqe_ba1 & 0xFFFFF);
}

static int hns_roce_v2_query_aeqc_ctx(struct hns_roce_dev *hr_dev, struct hns_roce_cmd_mailbox *mailbox, int aeqn,
                                      struct hns_roce_eq_context *eq_ctx)
{
    int ret;

    ret = hns_roce_v2_mbox_query_cmd(hr_dev, mailbox, aeqn, HNS_ROCE_CMD_QUERY_AEQC);
    if (ret) {
        dev_err(hr_dev->dev, "hns_roce cmd mbox op[%d] failed, aeqn[0x%x]\n", HNS_ROCE_CMD_QUERY_AEQC, aeqn);
        return ret;
    }

    ret = memcpy_s(eq_ctx, sizeof(*eq_ctx), mailbox->buf, sizeof(*eq_ctx));
    if (ret) {
        dev_err(hr_dev->dev, "%s, %d: memcpy_s failed, ret[%d]\n", __func__, __LINE__, ret);
        return ret;
    }

    return 0;
}

int hns_roce_v2_query_aeqc_stat(struct hns_roce_dev *hr_dev, char *buf, int *desc)
{
    struct hns_roce_eq_context *eq_context = NULL;
    struct hns_roce_cmd_mailbox *mailbox = NULL;
    int aeqn = hr_dev->hr_stat.aeqn;
    unsigned int *aeqc = NULL;
    int cur_len = 0;
    char *out = buf;
    int ret;
    u32 i;

    mailbox = hns_roce_alloc_cmd_mailbox(hr_dev);
    HNS_ROCE_ALLOC_CMD_MAILBOX_RET_CHECK(mailbox);

    eq_context = kzalloc(sizeof(*eq_context), GFP_KERNEL);
    if (eq_context == NULL) {
        ret = -ENOMEM;
        dev_err(hr_dev->dev, "kzalloc hns_roce_v2_query_aeqc_stat failed, size[%lu]\n", sizeof(*eq_context));
        goto free_mailbox;
    }

    ret = hns_roce_v2_query_aeqc_ctx(hr_dev, mailbox, aeqn, eq_context);
    if (ret) {
        dev_err(hr_dev->dev, "query aeq ctx failed, ret[%d], aeqn[0x%x]\n", ret, aeqn);
        goto free_ctx;
    }

    hns_roce_v2_eqc_info_print(hr_dev, eq_context, 0);
    aeqc = (unsigned int *)eq_context;
    for (i = 0; i < (u32)(sizeof(*eq_context) >> GET_LEN_BY_OFF); i += BYTE_UNIT_LEN) {
        hns_roce_v2_sysfs_print(out, cur_len, "AEQC(0x%x): %08x %08x %08x %08x %08x %08x %08x %08x\n",
                                (u32)aeqn, *aeqc, *(aeqc + INT_PTR_OFF_1), *(aeqc + INT_PTR_OFF_2),
                                *(aeqc + INT_PTR_OFF_3), *(aeqc + INT_PTR_OFF_4), *(aeqc + INT_PTR_OFF_5),
                                *(aeqc + INT_PTR_OFF_6), *(aeqc + INT_PTR_OFF_7));
        aeqc += BYTE_UNIT_LEN;
    }
    *desc += cur_len;

free_ctx:
    kfree(eq_context);
    eq_context = NULL;

free_mailbox:
    hns_roce_free_cmd_mailbox(hr_dev, mailbox);

    return ret;
}

STATIC int hns_roce_v2_get_cnp_stat_cnt(struct hns_roce_dev *hr_dev,
                                        struct hns_roce_cmq_desc *desc_cnp_rx,
                                        struct hns_roce_cmq_desc *desc_cnp_tx)
{
    int status;

    hns_roce_cmq_setup_basic_desc(desc_cnp_rx,
                                  HNS_ROCE_OPC_QUEYR_CNP_RX_CNT, true);
    status = hns_roce_cmq_send(hr_dev, desc_cnp_rx, 1);
    if (status) {
        dev_err(hr_dev->dev, "hns_roce cmq send failed, opcode[%d], status[%d]\n",
                HNS_ROCE_OPC_QUEYR_CNP_RX_CNT, status);
        return status;
    }

    hns_roce_cmq_setup_basic_desc(desc_cnp_tx,
                                  HNS_ROCE_OPC_QUEYR_CNP_TX_CNT, true);
    status = hns_roce_cmq_send(hr_dev, desc_cnp_tx, 1);
    if (status) {
        dev_err(hr_dev->dev, "hns_roce cmq send failed, opcode[%d], status[%d]\n",
                HNS_ROCE_OPC_QUEYR_CNP_TX_CNT, status);
        return status;
    }

    return 0;
}

STATIC int hns_roce_v2_get_pkt_cnt(struct hns_roce_dev *hr_dev, struct rdfx_query_pkt_cnt *resp_query[],
                                   struct hns_roce_cmq_desc desc[], int resp_desc_len)
{
    int status, i;

    for (i = 0; i < resp_desc_len; i++) {
        hns_roce_cmq_setup_basic_desc(&desc[i], HNS_ROCE_OPC_QUEYR_PKT_CNT, true);

        if (i < (resp_desc_len - 1)) {
            desc[i].flag |= cpu_to_le16(HNS_ROCE_CMD_FLAG_NEXT);
        } else {
            desc[i].flag &= ~cpu_to_le16(HNS_ROCE_CMD_FLAG_NEXT);
        }
        resp_query[i] = (struct rdfx_query_pkt_cnt *)desc[i].data;
    }
    status = hns_roce_cmq_send(hr_dev, desc, resp_desc_len);
    if (status) {
        dev_err(hr_dev->dev, "hns_roce cmq send fail, opcode[%d], status[%d]\n", HNS_ROCE_OPC_QUEYR_PKT_CNT, status);
        return status;
    }

    return 0;
}

#define CMD_NUM_QUERY_PKT_CNT   8
int hns_roce_v2_query_pkt_stat(struct hns_roce_dev *hr_dev, char *buf, int *buff_size)
{
    struct hns_roce_cmq_desc desc[CMD_NUM_QUERY_PKT_CNT] = { {0} };
    struct rdfx_query_pkt_cnt *resp_query[CMD_NUM_QUERY_PKT_CNT];
    struct hns_roce_cmq_desc desc_cqe = {0};
    struct rdfx_query_cqe_cnt *resp_cqe = (struct rdfx_query_cqe_cnt *)desc_cqe.data;
    struct hns_roce_cmq_desc desc_cnp_rx = {0};
    struct rdfx_query_cnp_rx_cnt *resp_cnp_rx = (struct rdfx_query_cnp_rx_cnt *)desc_cnp_rx.data;
    struct hns_roce_cmq_desc desc_cnp_tx = {0};
    struct rdfx_query_cnp_tx_cnt *resp_cnp_tx = (struct rdfx_query_cnp_tx_cnt *)desc_cnp_tx.data;
    unsigned int cur_len = 0;
    char *out = buf;
    int status;

    status = hns_roce_v2_get_pkt_cnt(hr_dev, resp_query, desc, CMD_NUM_QUERY_PKT_CNT);
    if (status) {
        dev_err(hr_dev->dev, "hns_roce_v2_get_pkt_cnt fail, status[%d]\n", status);
        return status;
    }

    hns_roce_cmq_setup_basic_desc(&desc_cqe, HNS_ROCE_OPC_QUEYR_CQE_CNT, true);
    status = hns_roce_cmq_send(hr_dev, &desc_cqe, 1);
    if (status) {
        dev_err(hr_dev->dev, "hns_roce cmq send fail, opcode[%d], status[%d]\n", HNS_ROCE_OPC_QUEYR_CQE_CNT, status);
        return status;
    }

    if (hr_dev->pci_dev->revision == PCI_REVISION_ID_HIP08_B) {
        status = hns_roce_v2_get_cnp_stat_cnt(hr_dev, &desc_cnp_rx, &desc_cnp_tx);
        if (status) {
            dev_err(hr_dev->dev, "hns_roce query_pkt_stat get cnp fail, status[%d]\n", status);
            return status;
        }
    }

    HNS_ROCE_PRINT_RX_RC_UC_STAT();
    HNS_ROCE_PRINT_RX_UD_XRC_STAT();
    HNS_ROCE_PRINT_RX_ERR_ALL_STAT();
    HNS_ROCE_PRINT_TX_RC_UC_STAT();
    HNS_ROCE_PRINT_TX_UD_XRC_STAT();
    HNS_ROCE_PRINT_TX_ERR_ALL_STAT();
    HNS_ROCE_PRINT_CX_STAT();
    *buff_size += (int)cur_len;
    return status;
}

STATIC int hns_roce_v2_query_ceqc_ctx(struct hns_roce_dev *hr_dev, struct hns_roce_cmd_mailbox *mailbox, int ceqn,
                                      struct hns_roce_eq_context *eq_ctx)
{
    int ret;

    ret = hns_roce_v2_mbox_query_cmd(hr_dev, mailbox, ceqn, HNS_ROCE_CMD_QUERY_CEQC);
    if (ret) {
        dev_err(hr_dev->dev, "hns_roce cmd mbox op[%d] failed, ceqn[0x%x]\n", HNS_ROCE_CMD_QUERY_CEQC, ceqn);
        return ret;
    }

    ret = memcpy_s(eq_ctx, sizeof(*eq_ctx), mailbox->buf, sizeof(*eq_ctx));
    if (ret) {
        dev_err(hr_dev->dev, "%s, %d: memcpy_s failed, ret[%d]\n", __func__, __LINE__, ret);
        return ret;
    }

    return 0;
}

int hns_roce_v2_query_ceqc_stat(struct hns_roce_dev *hr_dev, char *buf, int *desc)
{
    struct hns_roce_cmd_mailbox *mailbox = NULL;
    struct hns_roce_eq_context *eq_context = NULL;
    int ceqn = hr_dev->hr_stat.ceqn;
    int cur_len = 0;
    char *out = buf;
    int *ceqc = NULL;
    int ret;
    u32 i;

    mailbox = hns_roce_alloc_cmd_mailbox(hr_dev);
    HNS_ROCE_ALLOC_CMD_MAILBOX_RET_CHECK(mailbox);

    eq_context = kzalloc(sizeof(*eq_context), GFP_KERNEL);
    if (eq_context == NULL) {
        ret = -ENOMEM;
        dev_err(hr_dev->dev, "kzalloc hns_roce_v2_query_ceqc_stat failed, size[%lu]\n", sizeof(*eq_context));
        goto free_mailbox;
    }

    ret = hns_roce_v2_query_ceqc_ctx(hr_dev, mailbox, ceqn, eq_context);
    if (ret) {
        dev_err(hr_dev->dev, "query ceq ctx failed, ret[%d], cqen[0x%x]\n", ret, ceqn);
        goto free_ctx;
    }
    hns_roce_v2_eqc_info_print(hr_dev, eq_context, 1);
    ceqc = (int *)eq_context;
    for (i = 0; i < (u32)(sizeof(*eq_context) >> GET_LEN_BY_OFF); i += BYTE_UNIT_LEN) {
        hns_roce_v2_sysfs_print(out, cur_len, "CEQC(0x%x): %08x %08x %08x %08x %08x %08x %08x %08x\n",
                                ceqn, *ceqc, *(ceqc + INT_PTR_OFF_1), *(ceqc + INT_PTR_OFF_2),
                                *(ceqc + INT_PTR_OFF_3), *(ceqc + INT_PTR_OFF_4), *(ceqc + INT_PTR_OFF_5),
                                *(ceqc + INT_PTR_OFF_6), *(ceqc + INT_PTR_OFF_7));
        ceqc += BYTE_UNIT_LEN;
    }
    *desc += cur_len;

free_ctx:
    kfree(eq_context);
    eq_context = NULL;

free_mailbox:
    hns_roce_free_cmd_mailbox(hr_dev, mailbox);

    return ret;
}

int hns_roce_v2_query_cmd_stat(struct hns_roce_dev *hr_dev,
    char *buf, int *desc)
{
    struct hns_roce_cmq_desc desc_cnt;
    struct hns_roce_query_mbdb_cnt *resp_cnt =
        (struct hns_roce_query_mbdb_cnt *)desc_cnt.data;
    struct hns_roce_cmq_desc desc_dfx;
    unsigned int cur_len = 0;
    char *out = buf;
    int status;

    hns_roce_cmq_setup_basic_desc(&desc_cnt,
                                  HNS_ROCE_OPC_QUEYR_MBDB_CNT, true);
    status = hns_roce_cmq_send(hr_dev, &desc_cnt, 1);
    if (status) {
        dev_err(hr_dev->dev, "hns_roce cmq send failed, opcode[%d], status[%d]\n", HNS_ROCE_OPC_QUEYR_MBDB_CNT, status);
        return status;
    }

    hns_roce_cmq_setup_basic_desc(&desc_dfx,
                                  HNS_ROCE_OPC_QUEYR_MDB_DFX, true);
    status = hns_roce_cmq_send(hr_dev, &desc_dfx, 1);
    if (status) {
        dev_err(hr_dev->dev, "hns_roce cmq send failed, opcode[%d], status[%d]\n", HNS_ROCE_OPC_QUEYR_MDB_DFX, status);
        return status;
    }

    hns_roce_v2_sysfs_print(out, cur_len, "MB ISSUE CNT   : 0x%08x\n",
                            resp_cnt->mailbox_issue_cnt);
    hns_roce_v2_sysfs_print(out, cur_len, "MB EXEC CNT    : 0x%08x\n",
                            resp_cnt->mailbox_exe_cnt);
    hns_roce_v2_sysfs_print(out, cur_len, "DB ISSUE CNT   : 0x%08x\n",
                            resp_cnt->doorbell_issue_cnt);
    hns_roce_v2_sysfs_print(out, cur_len, "DB EXEC CNT    : 0x%08x\n",
                            resp_cnt->doorbell_exe_cnt);
    hns_roce_v2_sysfs_print(out, cur_len, "EQDB ISSUE CNT : 0x%08x\n",
                            resp_cnt->eq_doorbell_issue_cnt);
    hns_roce_v2_sysfs_print(out, cur_len, "EQDB EXEC CNT  : 0x%08x\n",
                            resp_cnt->eq_doorbell_exe_cnt);
    *desc += (int)cur_len;
    return status;
}

STATIC int hns_roce_v2_query_cqc(struct hns_roce_dev *hr_dev, u64 *bt0_ba, u64 *bt1_ba, int cqn,
                                 struct hns_roce_v2_cq_context *cq_context)
{
    struct hns_roce_cmd_mailbox *mailbox = NULL;
    int ret;

    mailbox = hns_roce_alloc_cmd_mailbox(hr_dev);
    HNS_ROCE_ALLOC_CMD_MAILBOX_RET_CHECK(mailbox);

    ret = hns_roce_v2_mbox_query_cmd(hr_dev, mailbox, cqn, HNS_ROCE_CMD_READ_CQC_BT0);
    if (ret) {
        dev_err(hr_dev->dev, "hns_roce read cqc btx op[%d] failed\n", HNS_ROCE_CMD_READ_CQC_BT0);
        goto free_mailbox;
    }

    ret = memcpy_s(bt0_ba, sizeof(u64), mailbox->buf, sizeof(u64));
    if (ret) {
        dev_err(hr_dev->dev, "%s, %d: memcpy_s failed, ret[%d]\n", __func__, __LINE__, ret);
        goto free_mailbox;
    }

    ret = hns_roce_v2_mbox_query_cmd(hr_dev, mailbox, cqn, HNS_ROCE_CMD_READ_CQC_BT1);
    if (ret) {
        dev_err(hr_dev->dev, "hns_roce read cqc btx op[%d] failed\n", HNS_ROCE_CMD_READ_CQC_BT1);
        goto free_mailbox;
    }

    ret = memcpy_s(bt1_ba, sizeof(u64), mailbox->buf, sizeof(u64));
    if (ret) {
        dev_err(hr_dev->dev, "%s, %d: memcpy_s failed, ret[%d]\n", __func__, __LINE__, ret);
        goto free_mailbox;
    }

    ret = hns_roce_v2_mbox_query_cmd(hr_dev, mailbox, cqn, HNS_ROCE_CMD_QUERY_CQC);
    if (ret) {
        dev_err(hr_dev->dev, "hns_roce cmd mbox op[%d] failed, cqn[0x%x]\n", HNS_ROCE_CMD_QUERY_CQC, cqn);
        goto free_mailbox;
    }

    ret = memcpy_s(cq_context, sizeof(*cq_context), mailbox->buf, sizeof(*cq_context));
    if (ret) {
        dev_err(hr_dev->dev, "%s, %d: memcpy_s failed, ret[%d]\n", __func__, __LINE__, ret);
    }

free_mailbox:
    hns_roce_free_cmd_mailbox(hr_dev, mailbox);
    return ret;
}

static void hns_roce_v2_cqc_info_print(struct hns_roce_dev *hr_dev,
    struct hns_roce_v2_cq_context *cq_context)
{
    dev_notice(hr_dev->dev, "hns roce v2 cqc info print!!!");
    dev_notice(hr_dev->dev, "cq_st:0x%x\t", cq_context->byte_4_pg_ceqn & 0x3);
    dev_notice(hr_dev->dev, "poll:0x%x\t", (cq_context->byte_4_pg_ceqn >> 0x2) & 0x1);
    dev_notice(hr_dev->dev, "se:0x%x\t", (cq_context->byte_4_pg_ceqn >> 0x3) & 0x1);
    dev_notice(hr_dev->dev, "over_ignore:0x%x\t", (cq_context->byte_4_pg_ceqn >> 0x4) & 0x1);
    dev_notice(hr_dev->dev, "coalesce:0x%x\t", (cq_context->byte_4_pg_ceqn >> 0x5) & 0x1);
    dev_notice(hr_dev->dev, "arm_st:0x%x\t", (cq_context->byte_4_pg_ceqn >> 0x6) & 0x3);
    dev_notice(hr_dev->dev, "shift:0x%x\t", (cq_context->byte_4_pg_ceqn >> 0x8) & 0x1F);
    dev_notice(hr_dev->dev, "cmd_sn:0x%x\t", (cq_context->byte_4_pg_ceqn >> 0xD) & 0x3);
    dev_notice(hr_dev->dev, "ceqn:0x%x\t", (cq_context->byte_4_pg_ceqn >> 0xF) & 0x1FF);
    dev_notice(hr_dev->dev, "page_offset:0x%x\t", (cq_context->byte_4_pg_ceqn >> 0x18) & 0xFF);
    dev_notice(hr_dev->dev, "cqn:0x%x\t", cq_context->byte_8_cqn & 0xFFFFFF);
    dev_notice(hr_dev->dev, "poe_en:0x%x\t", (cq_context->byte_8_cqn >> 0x18) & 0x1);
    dev_notice(hr_dev->dev, "poe_num:0x%x\t", (cq_context->byte_8_cqn >> 0x19) & 0x3);
    dev_notice(hr_dev->dev, "rsv1:0x%x\t", (cq_context->byte_8_cqn >> 0x1B) & 0x1F);
    dev_notice(hr_dev->dev, "cqe_cur_blk_addr_l:0x%x\t", cq_context->cqe_cur_blk_addr);
    dev_notice(hr_dev->dev, "cqe_cur_blk_addr_h:0x%x\t", cq_context->byte_16_hop_addr & 0xFFFFF);
    dev_notice(hr_dev->dev, "poe_qid:0x%x\t", (cq_context->byte_16_hop_addr >> 0x14) & 0x3FF);
    dev_notice(hr_dev->dev, "cqe_hop_num:0x%x\t", (cq_context->byte_16_hop_addr >> 0x1E) & 0x3);
    dev_notice(hr_dev->dev, "cqe_nxt_blk_addr_l:0x%x\t", cq_context->cqe_nxt_blk_addr);
    dev_notice(hr_dev->dev, "cqe_nxt_blk_addr_h:0x%x\t", cq_context->byte_24_pgsz_addr & 0xFFFFF);
    dev_notice(hr_dev->dev, "rsv3:0x%x\t", (cq_context->byte_24_pgsz_addr >> 0x14) & 0xF);
    dev_notice(hr_dev->dev, "cqe_ba_pg_sz:0x%x\t", (cq_context->byte_24_pgsz_addr >> 0x18) & 0xF);
    dev_notice(hr_dev->dev, "cqe_buf_pg_sz:0x%x\t", (cq_context->byte_24_pgsz_addr >> 0x1C) & 0xF);
    dev_notice(hr_dev->dev, "cq_producer_idx:0x%x\t", cq_context->byte_28_cq_pi & 0xFFFFFF);
    dev_notice(hr_dev->dev, "rsv4:0x%x\t", (cq_context->byte_28_cq_pi >> 0x18) & 0xFF);
    dev_notice(hr_dev->dev, "cq_consumer_idx:0x%x\t", cq_context->byte_32_cq_ci & 0xFFFFFF);
    dev_notice(hr_dev->dev, "rsv5:0x%x\t", (cq_context->byte_32_cq_ci >> 0x18) & 0xFF);
    dev_notice(hr_dev->dev, "cqe_ba_l:0x%x\t", cq_context->cqe_ba);
    dev_notice(hr_dev->dev, "cqe_ba_h:0x%x\t", cq_context->byte_40_cqe_ba & 0x1FFFFFFF);
    dev_notice(hr_dev->dev, "rsv6:0x%x\t", (cq_context->byte_40_cqe_ba >> 0x1D) & 0x7);
    dev_notice(hr_dev->dev, "db_record_en:0x%x\t", cq_context->byte_44_db_record & 0x1);
    dev_notice(hr_dev->dev, "db_record_addr_l:0x%x\t", (cq_context->byte_44_db_record >> 0x1) & 0x7FFFFFFF);
    dev_notice(hr_dev->dev, "db_record_addr_h:0x%x\t", cq_context->db_record_addr);
    dev_notice(hr_dev->dev, "cqe_cnt:0x%x\t", cq_context->byte_52_cqe_cnt & 0xFFFFFF);
    dev_notice(hr_dev->dev, "rsv7:0x%x\t", (cq_context->byte_52_cqe_cnt >> 0x18) & 0xFF);
    dev_notice(hr_dev->dev, "cq_max_cnt:0x%x\t", cq_context->byte_56_cqe_period_maxcnt & 0xFFFF);
    dev_notice(hr_dev->dev, "cq_period:0x%x\t", (cq_context->byte_56_cqe_period_maxcnt >> 0x10) & 0xFFFF);
    dev_notice(hr_dev->dev, "cqe_report_timer:0x%x\t", cq_context->cqe_report_timer);
    dev_notice(hr_dev->dev, "se_cqe_idx:0x%x\t", cq_context->byte_64_se_cqe_idx & 0xFFFFFF);
    dev_notice(hr_dev->dev, "rsv8:0x%x\t", (cq_context->byte_64_se_cqe_idx >> 0x18) & 0xFF);
}
int hns_roce_v2_query_cqc_stat(struct hns_roce_dev *hr_dev,
    char *buf, int *desc)
{
    struct hns_roce_v2_cq_context *cq_context = NULL;
    int cqn = hr_dev->hr_stat.cqn;
    unsigned int cur_len = 0;
    char *out = buf;
    u64 bt0_ba = 0;
    u64 bt1_ba = 0;
    int *cqc = NULL;
    int ret;
    u32 i;

    cq_context = kzalloc(sizeof(*cq_context), GFP_KERNEL);
#ifndef DEFINE_HNS_LLT
    if (cq_context == NULL) {
        dev_err(hr_dev->dev, "kzalloc hns_roce_v2_cq_context failed\n");
        return -ENOMEM;
    }
#endif

    ret = hns_roce_v2_query_cqc(hr_dev, &bt0_ba, &bt1_ba, cqn, cq_context);
    if (ret) {
        dev_err(hr_dev->dev, "hns_roce_v2 query cqc failed, cqn[%d]\n", cqn);
        goto out;
    }

    hns_roce_v2_cqc_info_print(hr_dev, cq_context);
    hns_roce_v2_sysfs_print(out, cur_len,
                            "CQC(0x%x) BT0: 0x%llx\n", cqn, bt0_ba);
    hns_roce_v2_sysfs_print(out, cur_len,
                            "CQC(0x%x) BT1: 0x%llx\n", cqn, bt1_ba);

    cqc = (int *)cq_context;
    for (i = 0; i < (sizeof(*cq_context) >> GET_LEN_BY_OFF); i += BYTE_UNIT_LEN) {
        hns_roce_v2_sysfs_print(out, cur_len,
                                "CQC(0x%x): %08x %08x %08x %08x %08x %08x %08x %08x\n",
                                cqn, *cqc, *(cqc + INT_PTR_OFF_1), *(cqc + INT_PTR_OFF_2),
                                *(cqc + INT_PTR_OFF_3), *(cqc + INT_PTR_OFF_4), *(cqc + INT_PTR_OFF_5),
                                *(cqc + INT_PTR_OFF_6), *(cqc + INT_PTR_OFF_7));
        cqc += BYTE_UNIT_LEN;
    }
    *desc += (int)cur_len;
out:
    kfree(cq_context);
    cq_context = NULL;
    return ret;
}

int hns_roce_v2_modify_eq(struct hns_roce_dev *hr_dev,
    u16 eq_count, u16 eq_period, u16 type)
{
    struct hns_roce_eq *eq = hr_dev->eq_table.eq;
    struct hns_roce_eq_context *eqc = NULL;
    struct hns_roce_eq_context *eqc_mask = NULL;
    struct hns_roce_cmd_mailbox *mailbox = NULL;
    struct hns_roce_mbox_post_params mbox_pst_params = {0};
    unsigned int eq_cmd;
    int ret;

    mailbox = hns_roce_alloc_cmd_mailbox(hr_dev);
    HNS_ROCE_ALLOC_CMD_MAILBOX_RET_CHECK(mailbox);

    eqc = mailbox->buf;
    eqc_mask = (struct hns_roce_eq_context *)mailbox->buf + 1;

    ret = memset_s(eqc_mask, sizeof(*eqc_mask), 0xff, sizeof(*eqc_mask));
    HNS_ROCE_SEC_CHECK_GOTO_OUT(hr_dev->dev, ret);

    if (type == HNS_ROCE_EQ_MAXCNT_MASK) {
        roce_set_field(eqc->byte_12,
                       HNS_ROCE_EQC_MAX_CNT_M,
                       HNS_ROCE_EQC_MAX_CNT_S, eq_count);
        roce_set_field(eqc_mask->byte_12,
                       HNS_ROCE_EQC_MAX_CNT_M,
                       HNS_ROCE_EQC_MAX_CNT_S, 0);
    } else if (type == HNS_ROCE_EQ_PERIOD_MASK) {
        roce_set_field(eqc->byte_12,
                       HNS_ROCE_EQC_PERIOD_M,
                       HNS_ROCE_EQC_PERIOD_S, eq_period);
        roce_set_field(eqc_mask->byte_12,
                       HNS_ROCE_EQC_PERIOD_M,
                       HNS_ROCE_EQC_PERIOD_S, 0);
    }
    eq_cmd = HNS_ROCE_CMD_MODIFY_CEQC;
    mbox_pst_params.in_param = mailbox->dma;
    mbox_pst_params.out_param = 0;
    mbox_pst_params.in_modifier = eq->eqn;
    mbox_pst_params.op_modifier = 1;
    mbox_pst_params.op = eq_cmd;
    ret = hns_roce_cmd_mbox(hr_dev, mbox_pst_params, HNS_ROCE_CMD_TIMEOUT_MSECS);
    if (ret) {
        dev_err(hr_dev->dev, "MODIFY EQ Failed to cmd mailbox.\n");
    }

out:
    hns_roce_free_cmd_mailbox(hr_dev, mailbox);
    return ret;
}

STATIC struct hns_roce_dev *hns_roce_dev_from_dev_id(int dev_id)
{
    struct hnae3_handle *handle = NULL;
    struct hns_roce_dev *hr_dev = NULL;

    handle = hclge_get_hnae3_handle(dev_id);
    if (handle == NULL) {
        pr_err("hns3: hclge get hnae3 handle fail.\n");
        return NULL;
    }

    hr_dev = (struct hns_roce_dev *)handle->priv;
    return hr_dev;
}

STATIC int hns_roce_v2_get_roce_stat_cnt(struct hns_roce_dev *hr_dev, struct roce_stat_info *roce_stat)
{
    int status;
    struct hns_roce_cmq_desc desc_cqe = {0};
    struct rdfx_query_cqe_cnt *resp_cqe =
        (struct rdfx_query_cqe_cnt *)desc_cqe.data;
    struct hns_roce_cmq_desc desc_cnp_rx = {0};
    struct rdfx_query_cnp_rx_cnt *resp_cnp_rx =
        (struct rdfx_query_cnp_rx_cnt *)desc_cnp_rx.data;
    struct hns_roce_cmq_desc desc_cnp_tx = {0};
    struct rdfx_query_cnp_tx_cnt *resp_cnp_tx =
        (struct rdfx_query_cnp_tx_cnt *)desc_cnp_tx.data;

    hns_roce_cmq_setup_basic_desc(&desc_cqe,
                                  HNS_ROCE_OPC_QUEYR_CQE_CNT, true);
    status = hns_roce_cmq_send(hr_dev, &desc_cqe, 1);
    if (status) {
        dev_err(hr_dev->dev, "hns_roce cmq send failed, opcode[%d], status[%d]\n",
                HNS_ROCE_OPC_QUEYR_CQE_CNT, status);
        return status;
    }

    if (hr_dev->pci_dev->revision == PCI_REVISION_ID_HIP08_B) {
        status = hns_roce_v2_get_cnp_stat_cnt(hr_dev, &desc_cnp_rx, &desc_cnp_tx);
        if (status) {
            dev_err(hr_dev->dev, "hns_roce roce stat get cnp fail, status[%d]\n", status);
            return status;
        }
    }

    roce_stat->roce_cqe_num = resp_cqe->port0_cqe + resp_cqe->port2_cqe;
    roce_stat->roce_rx_cnp_pkt_num = resp_cnp_rx->port0_cnp_rx + resp_cnp_rx->port2_cnp_rx;
    roce_stat->roce_tx_cnp_pkt_num = resp_cnp_tx->port0_cnp_tx + resp_cnp_tx->port2_cnp_tx;
    return 0;
}

int hns_roce_v2_get_roce_packet_stat(int dev_id, char *buf, int len)
{
    int status;
    struct roce_stat_info *roce_stat = (struct roce_stat_info *)buf;
    struct hns_roce_cmq_desc desc[CMD_NUM_QUERY_PKT_CNT] = { {0} };
    struct rdfx_query_pkt_cnt *resp_query[CMD_NUM_QUERY_PKT_CNT] = {0};
    struct hns_roce_dev *hr_dev = NULL;

    if (buf == NULL || len != sizeof(struct roce_stat_info)) {
        pr_err("hns_roce_v2_get_roce_packet_stat input para error.\n");
        return -EINVAL;
    }
    hr_dev = hns_roce_dev_from_dev_id(dev_id);
    if (hr_dev == NULL) {
        pr_err("hns3: hns roce dev from dev_id fail.\n");
        return -EINVAL;
    }

    status = hns_roce_v2_get_pkt_cnt(hr_dev, resp_query, desc, CMD_NUM_QUERY_PKT_CNT);
    if (status) {
        dev_err(hr_dev->dev, "hns_roce_v2_get_pkt_cnt fail, status[%d]\n", status);
        return status;
    }

    roce_stat->roce_rx_rc_pkt_num = resp_query[0]->rc_pkt_num +
        resp_query[RESP_QUERY_ARRAY_IDX_2]->rc_pkt_num;
    roce_stat->roce_rx_all_pkt_num = resp_query[0]->total_pkt_num +
        resp_query[RESP_QUERY_ARRAY_IDX_2]->total_pkt_num;
    roce_stat->roce_rx_err_pkt_num = resp_query[0]->error_pkt_num +
        resp_query[RESP_QUERY_ARRAY_IDX_2]->error_pkt_num;
    roce_stat->roce_tx_rc_pkt_num = resp_query[RESP_QUERY_ARRAY_IDX_4]->rc_pkt_num +
        resp_query[RESP_QUERY_ARRAY_IDX_6]->rc_pkt_num;
    roce_stat->roce_tx_all_pkt_num = resp_query[RESP_QUERY_ARRAY_IDX_4]->total_pkt_num +
        resp_query[RESP_QUERY_ARRAY_IDX_6]->total_pkt_num;
    roce_stat->roce_tx_err_pkt_num = resp_query[RESP_QUERY_ARRAY_IDX_4]->error_pkt_num +
        resp_query[RESP_QUERY_ARRAY_IDX_6]->error_pkt_num;

    status = hns_roce_v2_get_roce_stat_cnt(hr_dev, roce_stat);
    if (status) {
        dev_err(hr_dev->dev, "hns_roce_v2_get_roce_stat_cnt fail, status[%d]\n", status);
        return status;
    }
    return 0;
}
EXPORT_SYMBOL(hns_roce_v2_get_roce_packet_stat);

int hns_roce_v2_get_roce_qpc_stat(struct hns_roce_dev *hr_dev,
    const char *index, char *buf, int len)
{
    int ret;
    int qpn;

    ret = kstrtou32(index, DECIMAL, &qpn);
    if (ret) {
        dev_err(hr_dev->dev, "Input params format unmatch, ret[%d]\n", ret);
        return -EINVAL;
    }

    if ((qpn < 0) || (qpn >= hr_dev->caps.num_qps)) {
        dev_err(hr_dev->dev, "get roce qpc stat, qpn[%d], hr_dev->caps.num_qps[%d]\n",
            qpn, hr_dev->caps.num_qps);
        return -EINVAL;
    }

    mutex_lock(&hr_dev->hr_stat_mutex);
    hr_dev->hr_stat.qpn = qpn;

    ret = hns_roce_v2_query_qpc_stat(hr_dev, buf, &len);
    if (ret) {
        dev_err(hr_dev->dev, "hns roce v2 query qpc stat, ret[%d]\n", ret);
        mutex_unlock(&hr_dev->hr_stat_mutex);
        return -EINVAL;
    }

    mutex_unlock(&hr_dev->hr_stat_mutex);
    return 0;
}

STATIC int hns_roce_v2_get_roce_aeqc_stat(struct hns_roce_dev *hr_dev,
    const char *index, char *buf, int len)
{
    int ret;
    int aeqn;

    ret = kstrtou32(index, DECIMAL, &aeqn);
    if (ret) {
        dev_err(hr_dev->dev, "Input params format unmatch\n");
        return -EINVAL;
    }

    /* hardware ignore aeqn, eqn = 0 */
    if ((aeqn < 0) || (aeqn >= hr_dev->caps.num_aeq_vectors))  {
        dev_err(hr_dev->dev, "get roce aeqc stat, aeqn[%d], num_aeq_vectors[%d]\n",
            aeqn, hr_dev->caps.num_aeq_vectors);
        return -EINVAL;
    }

    hr_dev->hr_stat.aeqn = aeqn;
    ret = hns_roce_v2_query_aeqc_stat(hr_dev, buf, &len);
    if (ret) {
        dev_err(hr_dev->dev, "hns roce v2 query aeqc stat, ret[%d]\n", ret);
        return -EINVAL;
    }
    return 0;
}

STATIC int hns_roce_v2_get_roce_ceqc_stat(struct hns_roce_dev *hr_dev,
    const char *index, char *buf, int len)
{
    int ret;
    int ceqn;

    ret = kstrtou32(index, DECIMAL, &ceqn);
    if (ret) {
        dev_err(hr_dev->dev, "Input params format unmatch\n");
        return -EINVAL;
    }

    /* hardware handle eqn =  ceqn + 1 */
    if ((ceqn < 0) || (ceqn >= hr_dev->caps.num_comp_vectors)) {
        dev_err(hr_dev->dev, "get roce ceqc stat, ceqn[%d], hr_dev->caps.num_comp_vectors[%d]\n",
            ceqn, hr_dev->caps.num_comp_vectors);
        return -EINVAL;
    }

    hr_dev->hr_stat.ceqn = ceqn;
    ret = hns_roce_v2_query_ceqc_stat(hr_dev, buf, &len);
    if (ret) {
        dev_err(hr_dev->dev, "hns roce v2 query ceqc stat, ret[%d]\n", ret);
        return -EINVAL;
    }
    return 0;
}

STATIC int hns_roce_v2_get_roce_cqc_stat(struct hns_roce_dev *hr_dev,
    const char *index, char *buf, int len)
{
    int ret;
    int cqn;

    ret = kstrtou32(index, DECIMAL, &cqn);
    if (ret) {
        dev_err(hr_dev->dev, "Input params format unmatch\n");
        return -EINVAL;
    }

    if ((cqn < 0) || (cqn >= hr_dev->caps.num_cqs)) {
        dev_err(hr_dev->dev, "get roce cqc stat, cqn[%d], hr_dev->caps.num_cqs[%d]\n",
            cqn, hr_dev->caps.num_cqs);
        return -EINVAL;
    }

    hr_dev->hr_stat.cqn = cqn;
    ret = hns_roce_v2_query_cqc_stat(hr_dev, buf, &len);
    if (ret) {
        dev_err(hr_dev->dev, "hns roce v2 query cqc stat, ret[%d]\n", ret);
        return -EINVAL;
    }
    return 0;
}

STATIC int hns_roce_v2_get_roce_mtp_stat(struct hns_roce_dev *hr_dev,
    const char *index, char *buf, int len)
{
    int ret;
    int key;

    ret = kstrtou32(index, DECIMAL, &key);
    if (ret) {
        dev_err(hr_dev->dev, "Input params format unmatch, ret[%d]\n", ret);
        return -EINVAL;
    }

    if ((key < 0) || (key >= hr_dev->caps.num_mtpts)) {
        dev_err(hr_dev->dev, "get roce mpt stat, key[%d], hr_dev->caps.num_mtpts[%d]\n",
            key, hr_dev->caps.num_mtpts);
        return -EINVAL;
    }

    hr_dev->hr_stat.key = key;
    ret = hns_roce_v2_query_mpt_stat(hr_dev, buf, &len);
    if (ret) {
        dev_err(hr_dev->dev, "hns roce v2 query mtp stat, ret[%d]\n", ret);
        return -EINVAL;
    }
    return 0;
}

int hns_roce_v2_get_roce_context(int dev_id, unsigned int type,
    char* index, char* buf, int buf_len)
{
    int ret = 0;
    struct hns_roce_dev *hr_dev = NULL;
    if (buf == NULL || index == NULL) {
        pr_err("hns3: invalid param, buf[%pK], index[%pK] \n", buf, index);
        return -EINVAL;
    }
    hr_dev = hns_roce_dev_from_dev_id(dev_id);
    if (hr_dev == NULL) {
        pr_err("hns3: hns roce dev from dev_id fail.\n");
        return -EINVAL;
    }

    switch (type) {
        case DS_TYPE_QPC:
            ret = hns_roce_v2_get_roce_qpc_stat(hr_dev, index, buf, buf_len);
            break;
        case DS_TYPE_AEQC:
            ret = hns_roce_v2_get_roce_aeqc_stat(hr_dev, index, buf, buf_len);
            break;
        case DS_TYPE_CEQC:
            ret = hns_roce_v2_get_roce_ceqc_stat(hr_dev, index, buf, buf_len);
            break;
        case DS_TYPE_CQC:
            ret = hns_roce_v2_get_roce_cqc_stat(hr_dev, index, buf, buf_len);
            break;
        case DS_TYPE_MPT:
            ret = hns_roce_v2_get_roce_mtp_stat(hr_dev, index, buf, buf_len);
            break;
        default:
            ret = -EINVAL;
            break;
    }
    if (ret) {
        dev_err(hr_dev->dev, "roce_net_context_type fail!type[%u] ret[%d] fail.", type, ret);
        return ret;
    }
    return 0;
}
EXPORT_SYMBOL(hns_roce_v2_get_roce_context);

STATIC int hns_roce_v2_get_rocee_tsp_info(struct hns_roce_dev *hr_dev,
    struct hns_rocee_reg_info *info)
{
    int status;
    int i;
    struct hns_roce_cmq_desc desc[HNS_ROCEE_CMD_DESC_NUM_TWO] = {0};

    for (i = 0; i < HNS_ROCEE_CMD_DESC_NUM_TWO; i++) {
        hns_roce_cmq_setup_basic_desc(&desc[i], HNS_ROCE_OPC_QUEYR_TSP_DFX, true);

        if (i == 0) {
            desc[i].flag |= cpu_to_le16(HNS_ROCEE_REG_FLAG_NEXT);
        }
    }

    status = hns_roce_cmq_send(hr_dev, desc, HNS_ROCEE_CMD_SEND_NUM_TWO);
    if (status) {
        dev_err(hr_dev->dev, "hns_roce cmq send failed, opcode[%d], status[%d]\n",
                HNS_ROCE_OPC_QUEYR_TSP_DFX, status);
        return status;
    }

    info->rocee_twp_alm = le32_to_cpu(desc[0].data[HNS_ROCEE_REG_INDEX_THREE]);
    info->rocee_tpp_alm = le32_to_cpu(desc[1].data[HNS_ROCEE_REG_INDEX_THREE]);

    return 0;
}

STATIC int hns_roce_v2_get_rocee_trp_err_info(struct hns_roce_dev *hr_dev,
    struct hns_rocee_reg_info *info)
{
    int status;
    int i;
    struct hns_roce_cmq_desc desc[HNS_ROCEE_CMD_DESC_NUM_TWO] = {0};

    for (i = 0; i < HNS_ROCEE_CMD_DESC_NUM_TWO; i++) {
        hns_roce_cmq_setup_basic_desc(&desc[i], HNS_ROCE_OPC_QUEYR_TRP_ERR_DFX, true);

        if (i == 0) {
            desc[i].flag |= cpu_to_le16(HNS_ROCEE_REG_FLAG_NEXT);
        }
    }

    status = hns_roce_cmq_send(hr_dev, desc, HNS_ROCEE_CMD_SEND_NUM_TWO);
    if (status) {
        dev_err(hr_dev->dev, "hns_roce cmq send failed, opcode[%d], status[%d]\n",
                HNS_ROCE_OPC_QUEYR_TRP_ERR_DFX, status);
        return status;
    }

    info->rocee_trp_err_flg_0 = le32_to_cpu(desc[0].data[0]);
    info->rocee_trp_err_flg_1 = le32_to_cpu(desc[1].data[0]);

    return 0;
}

int hns_roce_v2_get_rocee_reg_info(int dev_id, char *buf, int buf_len)
{
    int ret;
    struct hns_roce_dev *hr_dev = NULL;
    struct hns_rocee_reg_info *reg_info = NULL;

    if (buf == NULL || buf_len != sizeof(struct hns_rocee_reg_info)) {
        pr_err("hns_roce_v2_get_rocee_reg_info input para error.\n");
        return -EINVAL;
    }

    hr_dev = hns_roce_dev_from_dev_id(dev_id);
    if (hr_dev == NULL) {
        pr_err("hns3: hns roce dev from dev_id fail.\n");
        return -EINVAL;
    }

    reg_info = (struct hns_rocee_reg_info*)buf;
    ret = hns_roce_v2_get_rocee_tsp_info(hr_dev, reg_info);
    if (ret) {
        dev_err(hr_dev->dev, "hns_roce_v2_get_rocee_tsp_info fail ret[%d]", ret);
        return ret;
    }

    ret = hns_roce_v2_get_rocee_trp_err_info(hr_dev, reg_info);
    if (ret) {
        dev_err(hr_dev->dev, "hns_roce_v2_get_rocee_trp_err_info fail ret[%d]", ret);
        return ret;
    }

    return 0;
}
EXPORT_SYMBOL(hns_roce_v2_get_rocee_reg_info);

int hns_roce_v2_get_register_from_imp(struct hns_roce_dev *hr_dev, u64 addr, unsigned int *value)
{
    int status;
    struct hns_roce_cmq_desc desc;

    hns_roce_cmq_setup_basic_desc(&desc, HNS_GET_NETWORK_REG_DATA_CMD, true);
    desc.data[0] = cpu_to_le32(addr & 0xffffffff);
    desc.data[1] = cpu_to_le32(addr >> OPERATE_NUMBER_32);
    desc.data[DESC_DATA_INDEX_FOUR] = OPERATE_NUMBER_32;

    status = hns_roce_cmq_send(hr_dev, &desc, 1);
    if (status) {
        dev_err(hr_dev->dev, "hns_roce_cmq_send, opcode[%d], status[%d]\n",
                HNS_GET_NETWORK_REG_DATA_CMD, status);
        return status;
    }

    *value = le32_to_cpu(desc.data[DESC_DATA_INDEX_TWO]);

    return 0;
}

int hns_roce_v2_get_network_register(int dev_id, u64 addr, char *buf, int buf_len)
{
    unsigned int *value = (unsigned int *)buf;
    struct hns_roce_dev *hr_dev = NULL;
    int ret;

    if (buf == NULL || buf_len != sizeof(unsigned int)) {
        pr_err("hns3: hns_roce_v2_get_network_register input para error. buf_len[%d]\n", buf_len);
        return -EINVAL;
    }

    hr_dev = hns_roce_dev_from_dev_id(dev_id);
    if (hr_dev == NULL) {
        pr_err("hns3: hns roce dev from dev_id fail.\n");
        return -EINVAL;
    }

    ret = hns_roce_v2_get_register_from_imp(hr_dev, addr, value);
    if (ret) {
        dev_err(hr_dev->dev, "hns_roce_v2_get_register_from_imp fail ret[%d]", ret);
        return ret;
    }

    return 0;
}
EXPORT_SYMBOL(hns_roce_v2_get_network_register);

static int hns_roce_v2_network_register_write(struct hns_roce_dev *hr_dev, u64 addr, unsigned int value)
{
    struct hns_roce_cmq_desc desc;
    int status;

    hns_roce_cmq_setup_basic_desc(&desc, HNS_GET_NETWORK_REG_DATA_CMD, false);
    desc.data[0] = cpu_to_le32(addr & 0xffffffff);
    desc.data[1] = cpu_to_le32(addr >> OPERATE_NUMBER_32);
    desc.data[DESC_DATA_INDEX_TWO] = cpu_to_le32(value);
    desc.data[DESC_DATA_INDEX_FOUR] = OPERATE_NUMBER_32;

    status = hns_roce_cmq_send(hr_dev, &desc, 1);
    if (status != 0) {
        dev_err(hr_dev->dev, "hns_roce_cmq_send, opcode[%d], status[%d]\n",
                HNS_GET_NETWORK_REG_DATA_CMD, status);
        return status;
    }

    return 0;
}

int hns_roce_v2_set_network_register(int dev_id, u64 addr, unsigned int value)
{
    struct hns_roce_dev *hr_dev = NULL;
    int ret;

    hr_dev = hns_roce_dev_from_dev_id(dev_id);
    if (hr_dev == NULL) {
        pr_err("hns3: hns roce dev from dev_id[%d] fail\n", dev_id);
        return -EINVAL;
    }

    ret = hns_roce_v2_network_register_write(hr_dev, addr, value);
    if (ret != 0) {
        dev_err(hr_dev->dev, "hns_roce_v2_network_register_write fail dev_id[%d] ret[%d]\n", dev_id, ret);
        return ret;
    }

    return 0;
}
EXPORT_SYMBOL(hns_roce_v2_set_network_register);

static int hns_roce_v2_get_dfx_regs(struct hns_roce_dev *hr_dev, u32 bd_num, u32 regs_num, void *data)
{
#define HNS_ROCE_32_BIT_REG_RTN_DATANUM 6

    struct hns_roce_cmq_desc *desc;
    u32 regs_num_tmp = regs_num;
    u32 *reg_val = data;
    __le32 *desc_data;
    int cmd_num;
    int i, j;
    int ret;

    if (regs_num_tmp == 0) {
        dev_err(hr_dev->dev, "get roce dfx failed, regs_num = 0.\n");
        return -EINVAL;
    }

    cmd_num = DIV_ROUND_UP(regs_num_tmp, HNS_ROCE_32_BIT_REG_RTN_DATANUM);
    if (bd_num != cmd_num) {
        dev_err(hr_dev->dev, "get roce dfx failed, bd_num(%u) != cmd_num(%u).\n", bd_num, cmd_num);
        return -EINVAL;
    }
    desc = kcalloc(cmd_num, sizeof(struct hns_roce_cmq_desc), GFP_KERNEL);
    if (!desc) {
        dev_err(hr_dev->dev, "get roce dfx kcalloc failed, cmd_num(%u).\n", cmd_num);
        return -ENOMEM;
    }

    for (i = 0; i < cmd_num; i++) {
        hns_roce_cmq_setup_basic_desc(&desc[i], NETWORK_QUERY_ROCE_DFX_REG_INFO_CMD, true);
        if (i < (cmd_num - 1)) {
            desc[i].flag |= cpu_to_le16(HNS_ROCE_CMD_FLAG_NEXT);
        } else {
            desc[i].flag &= ~cpu_to_le16(HNS_ROCE_CMD_FLAG_NEXT);
        }
    }
    ret = hns_roce_cmq_send(hr_dev, desc, cmd_num);
    if (ret) {
        dev_err(hr_dev->dev, "get roce dfx cmd failed, ret = %d.\n", ret);
        kfree(desc);
        desc = NULL;
        return ret;
    }

    for (i = 0; i < cmd_num; i++) {
        desc_data = (__le32 *)(&desc[i].data[0]);
        for (j = 0; j < HNS_ROCE_32_BIT_REG_RTN_DATANUM; j++) {
            *reg_val++ = le32_to_cpu(*desc_data++);

            regs_num_tmp--;
            if (!regs_num_tmp)
                break;
        }
    }

    kfree(desc);
    desc = NULL;
    return 0;
}

STATIC int hns_roce_get_regs_num(struct hns_roce_dev *hr_dev, u32 *bd_num, u32 *regs_num)
{
    struct hns_roce_cmq_desc desc;
    int ret;

    hns_roce_cmq_setup_basic_desc(&desc, NETWORK_QUERY_ROCE_DFX_REG_NUM_CMD, true);
    ret = hns_roce_cmq_send(hr_dev, &desc, 1);
    if (ret) {
        dev_err(hr_dev->dev, "Query roce register number cmd failed, ret = %d.\n", ret);
        return ret;
    }

    *bd_num = le32_to_cpu(desc.data[0]);
    *regs_num = le32_to_cpu(desc.data[1]);

    return 0;
}

int hns_roce_v2_get_roce_dfx_len(int dev_id, char *buf, int buf_len)
{
    unsigned int *value = (unsigned int *)buf;
    struct hns_roce_dev *hr_dev = NULL;
    u32 bd_num, regs_num;
    int ret;

    if (buf == NULL || buf_len != sizeof(u32)) {
        pr_err("hns_roce_v2_get_roce_dfx_len input para error. buf_len[%d].\n", buf_len);
        return -EINVAL;
    }

    hr_dev = hns_roce_dev_from_dev_id(dev_id);
    if (hr_dev == NULL) {
        pr_err("hns3: hns roce dev from dev_id fail.\n");
        return -EINVAL;
    }

    ret = hns_roce_get_regs_num(hr_dev, &bd_num, &regs_num);
    if (ret) {
        dev_err(hr_dev->dev, "Get register number failed, ret = %d.\n", ret);
        return ret;
    }

    *value = regs_num * sizeof(u32);

    return 0;
}
EXPORT_SYMBOL(hns_roce_v2_get_roce_dfx_len);

int hns_roce_v2_get_roce_dfx(int dev_id, char *buf, int buf_len)
{
    struct hns_roce_dev *hr_dev = NULL;
    u32 bd_num, regs_num;
    int ret;

    hr_dev = hns_roce_dev_from_dev_id(dev_id);
    if (hr_dev == NULL) {
        pr_err("hns3: hns roce dev from dev_id fail.\n");
        return -EINVAL;
    }

    ret = hns_roce_get_regs_num(hr_dev, &bd_num, &regs_num);
    if (ret) {
        dev_err(hr_dev->dev, "Get register number failed, ret = %d.\n", ret);
        return ret;
    }

    if (buf == NULL || buf_len < (regs_num * sizeof(u32))) {
        dev_err(hr_dev->dev, "hns_roce_v2_get_roce_dfx input para error. buf_len[%d].\n", buf_len);
        return -EINVAL;
    }

    ret = hns_roce_v2_get_dfx_regs(hr_dev, bd_num, regs_num, (void *)buf);
    if (ret) {
        dev_err(hr_dev->dev, "Get roce dfx failed, ret = %d.\n", ret);
        return ret;
    }

    return 0;
}
EXPORT_SYMBOL(hns_roce_v2_get_roce_dfx);

int hns_roce_v2_get_hw_stats_count(int dev_id, char *buf, int buf_len)
{
    struct hns_roce_dev *hr_dev = NULL;
    struct ib_device *ib_dev = NULL;
    int *value = (int *)buf;

    if (buf == NULL || buf_len != sizeof(int)) {
        pr_err("hns_roce_v2_get_hw_stats_count input para error. buf_len[%d].\n", buf_len);
        return -EINVAL;
    }

    hr_dev = hns_roce_dev_from_dev_id(dev_id);
    if (hr_dev == NULL) {
        pr_err("hns3: hns roce dev from dev_id fail.\n");
        return -EINVAL;
    }

    ib_dev = &hr_dev->ib_dev;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
    *value = hns_roce_get_hw_stats_num_counters(ib_dev);
#else
    if (ib_dev->hw_stats == NULL) {
        pr_err("hns3: hw_stats from hns roce dev fail.\n");
        return -EINVAL;
    }

    *value = ib_dev->hw_stats->num_counters;
#endif

    return 0;
}
EXPORT_SYMBOL(hns_roce_v2_get_hw_stats_count);

static void hns_roce_v2_update_hw_stats(struct rdma_hw_stats *stats)
{
    unsigned long long *value = stats->value;

    value[HW_STATS_PD_ACTIVE] = value[HW_STATS_PD_ALLOC] - value[HW_STATS_PD_DEALLOC];
    value[HW_STATS_MR_ACTIVE] = value[HW_STATS_MR_ALLOC] - value[HW_STATS_MR_DEALLOC];
    value[HW_STATS_CQ_ACTIVE] = value[HW_STATS_CQ_ALLOC] - value[HW_STATS_CQ_DEALLOC];
    value[HW_STATS_QP_ACTIVE] = value[HW_STATS_QP_ALLOC] - value[HW_STATS_QP_DEALLOC];
}

int hns_roce_v2_get_hw_stats(int dev_id, char *buf, int buf_len)
{
    struct hns_roce_dev *hr_dev = NULL;
    struct ib_device *ib_dev = NULL;
    struct rdma_hw_stats *stats;
    int ret = 0;

    hr_dev = hns_roce_dev_from_dev_id(dev_id);
    if (hr_dev == NULL) {
        pr_err("hns3: hns roce dev from dev_id fail.\n");
        return -EINVAL;
    }

    ib_dev = &hr_dev->ib_dev;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
    stats = hns_roce_get_net_hw_stats(ib_dev);
#else
    if (ib_dev->hw_stats == NULL) {
        pr_err("hns3: hw_stats from hns roce dev fail.\n");
        return -EINVAL;
    }

    stats = ib_dev->hw_stats;
#endif
    if (stats == NULL || buf == NULL || buf_len != stats->num_counters * sizeof(unsigned long long)) {
        pr_err("hns_roce_v2_get_hw_stats_count input para error. buf_len[%d].\n", buf_len);
        return -EINVAL;
    }

    hns_roce_v2_update_hw_stats(stats);
    ret = memcpy_s(buf, buf_len, (char *)stats->value, stats->num_counters * sizeof(unsigned long long));
    if (ret) {
        pr_err("hns3: copy hw_stats value to buf fail.\n");
        return -ENOMEM;
    }

    return 0;
}
EXPORT_SYMBOL(hns_roce_v2_get_hw_stats);

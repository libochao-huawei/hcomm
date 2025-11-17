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
#include <rdma/ib_addr.h>
#include <rdma/ib_cache.h>
#include <rdma/ib_umem.h>
#include "hnae3.h"
#include "hns_roce_sm.h"
#include "hns_roce_common.h"
#include "hns_roce_device.h"
#include "hns_roce_cmd.h"
#include "hns_roce_hem.h"
#include "hns_roce_sec.h"
#include "hns_roce_hw_v2.h"

#ifdef DEFINE_HNS_LLT
#undef static
#define static

#undef inline
#define inline
#endif

#ifndef CONFIG_DISABLE_DCQCN
static int g_dcqcn = 1;
#else
static int g_dcqcn;
#endif

static void set_data_seg_v2(struct hns_roce_v2_wqe_data_seg *dseg,
    struct ib_sge *sg)
{
    dseg->lkey = cpu_to_le32(sg->lkey);
    dseg->addr = cpu_to_le64(sg->addr);
    dseg->len  = cpu_to_le32(sg->length);
}

static void set_frmr_seg(struct hns_roce_v2_rc_send_wqe *rc_sq_wqe,
    struct hns_roce_wqe_frmr_seg *fseg,
    const struct ib_reg_wr *wr)
{
    struct hns_roce_mr *mr = to_hr_mr(wr->mr);

    /* use ib_access_flags */
    roce_set_bit(rc_sq_wqe->byte_4, V2_RC_FRMR_WQE_BYTE_4_BIND_EN_S,
                 (unsigned int)wr->access & IB_ACCESS_MW_BIND ? 1 : 0);
    roce_set_bit(rc_sq_wqe->byte_4, V2_RC_FRMR_WQE_BYTE_4_ATOMIC_S,
                 (unsigned int)wr->access & IB_ACCESS_REMOTE_ATOMIC ? 1 : 0);
    roce_set_bit(rc_sq_wqe->byte_4, V2_RC_FRMR_WQE_BYTE_4_RR_S,
                 (unsigned int)wr->access & IB_ACCESS_REMOTE_READ ? 1 : 0);
    roce_set_bit(rc_sq_wqe->byte_4, V2_RC_FRMR_WQE_BYTE_4_RW_S,
                 (unsigned int)wr->access & IB_ACCESS_REMOTE_WRITE ? 1 : 0);
    roce_set_bit(rc_sq_wqe->byte_4, V2_RC_FRMR_WQE_BYTE_4_LW_S,
                 (unsigned int)wr->access & IB_ACCESS_LOCAL_WRITE ? 1 : 0);

    /* Data structure reuse may lead to confusion */
    rc_sq_wqe->msg_len = cpu_to_le32(mr->pbl_ba & LOWBITS_MASK);
    rc_sq_wqe->inv_key = cpu_to_le32(mr->pbl_ba >> LOWBITS_WIDTH);

    rc_sq_wqe->byte_16 = cpu_to_le32(wr->mr->length & LOWBITS_MASK);
    rc_sq_wqe->byte_20 = cpu_to_le32(wr->mr->length >> LOWBITS_WIDTH);
    rc_sq_wqe->rkey = cpu_to_le32(wr->key);
    rc_sq_wqe->va = cpu_to_le64(wr->mr->iova);

    fseg->pbl_size = cpu_to_le32(mr->pbl_size);
    roce_set_field(fseg->mode_buf_pg_sz,
                   V2_RC_FRMR_WQE_BYTE_40_PBL_BUF_PG_SZ_M,
                   V2_RC_FRMR_WQE_BYTE_40_PBL_BUF_PG_SZ_S,
                   mr->pbl_buf_pg_sz + PG_SHIFT_OFFSET);
    roce_set_bit(fseg->mode_buf_pg_sz, V2_RC_FRMR_WQE_BYTE_40_BLK_MODE_S,
                 0);
}

static void set_atomic_seg(struct hns_roce_wqe_atomic_seg *aseg,
    const struct ib_atomic_wr *wr)
{
    if (wr->wr.opcode == IB_WR_ATOMIC_CMP_AND_SWP) {
        aseg->fetchadd_swap_data = cpu_to_le64(wr->swap);
        aseg->cmp_data  = cpu_to_le64(wr->compare_add);
    } else {
        aseg->fetchadd_swap_data = cpu_to_le64(wr->compare_add);
        aseg->cmp_data  = 0;
    }
}

static void set_extend_sge(struct hns_roce_qp *qp, const struct ib_send_wr *wr,
    unsigned int *sge_ind)
{
    struct hns_roce_v2_wqe_data_seg *dseg = NULL;
    struct ib_sge *sg = NULL;
    int num_in_wqe = 0;
    int extend_sge_num;
    int fi_sge_num;
    int se_sge_num;
    int shift;
    int i;

    if (qp->ibqp.qp_type == IB_QPT_RC || qp->ibqp.qp_type == IB_QPT_UC) {
        num_in_wqe = HNS_ROCE_V2_UC_RC_SGE_NUM_IN_WQE;
    }
    extend_sge_num = wr->num_sge - num_in_wqe;
    sg = wr->sg_list + num_in_wqe;
    shift = qp->hr_buf.page_shift;

    /*
     * Check whether wr->num_sge sges are in the same page. If not, we
     * should calculate how many sges in the first page and the second
     * page.
     */
    dseg = get_send_extend_sge(qp, (*sge_ind) & (unsigned int)(qp->sge.sge_cnt - 1));
    fi_sge_num = (int)(round_up((uintptr_t)dseg, 1 << (unsigned int)shift) -
                 (uintptr_t)dseg) / sizeof(struct hns_roce_v2_wqe_data_seg);
    if (extend_sge_num > fi_sge_num) {
        se_sge_num = extend_sge_num - fi_sge_num;
        for (i = 0; i < fi_sge_num; i++) {
            set_data_seg_v2(dseg++, sg + i);
            (*sge_ind)++;
        }
        dseg = get_send_extend_sge(qp,
                                   (*sge_ind) & (unsigned int)(qp->sge.sge_cnt - 1));
        for (i = 0; i < se_sge_num; i++) {
            set_data_seg_v2(dseg++, sg + fi_sge_num + i);
            (*sge_ind)++;
        }
    } else {
        for (i = 0; i < extend_sge_num; i++) {
            set_data_seg_v2(dseg++, sg + i);
            (*sge_ind)++;
        }
    }
}

static int set_rwqe_inline_data_seg(struct ib_qp *ibqp, const struct ib_send_wr *wr,
    struct hns_roce_v2_rc_send_wqe *rc_sq_wqe,
    char *wqe, const struct ib_send_wr **bad_wr)
{
    struct hns_roce_dev *hr_dev = to_hr_dev(ibqp->device);
    int ret;
    int i;

    if (le32_to_cpu(rc_sq_wqe->msg_len) >
            hr_dev->caps.max_sq_inline) {
        *bad_wr = wr;
        dev_err(hr_dev->dev, "inline len(1-%d)=%d, illegal",
                rc_sq_wqe->msg_len, hr_dev->caps.max_sq_inline);
        return -EINVAL;
    }

    if (wr->opcode == IB_WR_RDMA_READ) {
        *bad_wr =  wr;
        dev_err(hr_dev->dev, "not support inline data in rdma read!\n");
        return -EINVAL;
    }

    for (i = 0; i < wr->num_sge; i++) {
        ret = memcpy_s(wqe, wr->sg_list[i].length, ((char *)(uintptr_t)wr->sg_list[i].addr),
                       wr->sg_list[i].length);
        HNS_ROCE_SEC_CHECK_RET_INT(hr_dev->dev, ret);
        wqe += wr->sg_list[i].length;
    }

    roce_set_bit(rc_sq_wqe->byte_4, V2_RC_SEND_WQE_BYTE_4_INLINE_S,
                 1);
    return 0;
}

static int set_rwqe_data_seg(struct ib_qp *ibqp, const struct ib_send_wr *wr,
    struct hns_roce_v2_rc_send_wqe *rc_sq_wqe,
    struct hns_roce_wqe_sge *wqe_sge,
    const struct ib_send_wr **bad_wr)
{
    struct hns_roce_dev *hr_dev = to_hr_dev(ibqp->device);
    struct hns_roce_v2_wqe_data_seg *dseg = (struct hns_roce_v2_wqe_data_seg *)wqe_sge->wqe;
    struct hns_roce_qp *qp = to_hr_qp(ibqp);
    int i;
    int ret;

    if (((unsigned int)wr->send_flags & IB_SEND_INLINE) && wr->num_sge) {
        ret = set_rwqe_inline_data_seg(ibqp, wr, rc_sq_wqe, wqe_sge->wqe, bad_wr);
        if (ret) {
            dev_err(hr_dev->dev, "set_rwqe_inline_data_seg err!\n");
            return ret;
        }

        return 0;
    }

    if (wr->num_sge <= HNS_ROCE_V2_UC_RC_SGE_NUM_IN_WQE) {
        for (i = 0; i < wr->num_sge; i++) {
            if (likely(wr->sg_list[i].length)) {
                set_data_seg_v2(dseg, wr->sg_list + i);
                dseg++;
            }
        }
    } else {
        roce_set_field(rc_sq_wqe->byte_20,
                       V2_RC_SEND_WQE_BYTE_20_MSG_START_SGE_IDX_M,
                       V2_RC_SEND_WQE_BYTE_20_MSG_START_SGE_IDX_S,
                       (wqe_sge->sge_ind) & ((unsigned int)qp->sge.sge_cnt - 1));

        for (i = 0; i < HNS_ROCE_V2_UC_RC_SGE_NUM_IN_WQE; i++) {
            if (likely(wr->sg_list[i].length)) {
                set_data_seg_v2(dseg, wr->sg_list + i);
                dseg++;
            }
        }

        set_extend_sge(qp, wr, &wqe_sge->sge_ind);
    }

    roce_set_field(rc_sq_wqe->byte_16,
                   V2_RC_SEND_WQE_BYTE_16_SGE_NUM_M,
                   V2_RC_SEND_WQE_BYTE_16_SGE_NUM_S, wr->num_sge);

    return 0;
}

static int hns_roce_v2_supp_type(struct device *dev, struct ib_qp *ibqp, struct hns_roce_qp *qp)
{
    if (unlikely(ibqp->qp_type != IB_QPT_RC &&
             ibqp->qp_type != IB_QPT_UC &&
             ibqp->qp_type != IB_QPT_GSI &&
             ibqp->qp_type != IB_QPT_UD) &&
            (ibqp->qp_type != IB_QPT_XRC_INI) &&
            (ibqp->qp_type != IB_QPT_XRC_TGT)) {
        dev_err(dev, "Not supported QP type, type-0x%x, qpn-0x%x!\n",
                ibqp->qp_type, ibqp->qp_num);
        return -EOPNOTSUPP;
    }

    if (unlikely(qp->state == IB_QPS_RESET || qp->state == IB_QPS_INIT ||
                 qp->state == IB_QPS_RTR)) {
        dev_err(dev, "Post WQE fail, QP(0x%x) state %d err!\n",
                ibqp->qp_num, qp->state);

        return -EINVAL;
    }

    return 0;
}

static void hns_roce_v2_set_ud_wqe_dmac(struct hns_roce_v2_ud_send_wqe *ud_sq_wqe,
    struct hns_roce_ah *ah)
{
    roce_set_field(ud_sq_wqe->dmac, V2_UD_SEND_WQE_DMAC_0_M,
                   V2_UD_SEND_WQE_DMAC_0_S, ah->av.mac[MAC_INDEX_ZERO]);
    roce_set_field(ud_sq_wqe->dmac, V2_UD_SEND_WQE_DMAC_1_M,
                   V2_UD_SEND_WQE_DMAC_1_S, ah->av.mac[MAC_INDEX_FST]);
    roce_set_field(ud_sq_wqe->dmac, V2_UD_SEND_WQE_DMAC_2_M,
                   V2_UD_SEND_WQE_DMAC_2_S, ah->av.mac[MAC_INDEX_SND]);
    roce_set_field(ud_sq_wqe->dmac, V2_UD_SEND_WQE_DMAC_3_M,
                   V2_UD_SEND_WQE_DMAC_3_S, ah->av.mac[MAC_INDEX_TRD]);
    roce_set_field(ud_sq_wqe->byte_48,
                   V2_UD_SEND_WQE_BYTE_48_DMAC_4_M,
                   V2_UD_SEND_WQE_BYTE_48_DMAC_4_S,
                   ah->av.mac[MAC_INDEX_FORTH]);
    roce_set_field(ud_sq_wqe->byte_48,
                   V2_UD_SEND_WQE_BYTE_48_DMAC_5_M,
                   V2_UD_SEND_WQE_BYTE_48_DMAC_5_S,
                   ah->av.mac[MAC_INDEX_FORTH]);
}

static void hns_roce_v2_set_ud_wqe_msg_len(struct hns_roce_v2_ud_send_wqe *ud_sq_wqe,
    const struct ib_send_wr *wr)
{
    int i;
    u32 tmp_len = 0;
    for (i = 0; i < wr->num_sge; i++) {
        tmp_len += wr->sg_list[i].length;
    }

    ud_sq_wqe->msg_len =
        cpu_to_le32(le32_to_cpu(ud_sq_wqe->msg_len) + tmp_len);
}

static void hns_roce_v2_set_ud_wqe_immtdata(
    struct hns_roce_v2_ud_send_wqe *ud_sq_wqe,
    const struct ib_send_wr *wr)
{
    switch (wr->opcode) {
        case IB_WR_SEND_WITH_IMM:
        case IB_WR_RDMA_WRITE_WITH_IMM:
            ud_sq_wqe->immtdata =
                cpu_to_le32(be32_to_cpu(wr->ex.imm_data));
            break;
        default:
            ud_sq_wqe->immtdata = 0;
            break;
    }
}

static void hns_roce_v2_set_ud_wqe_attr(
    struct ib_qp *ibqp, const struct ib_send_wr *wr,
    struct hns_roce_v2_ud_send_wqe *ud_sq_wqe,
    unsigned int owner_bit, unsigned int *sge_ind)
{
    struct hns_roce_qp *qp = NULL;
    struct hns_roce_ah *ah = NULL;
    qp = to_hr_qp(ibqp);
    ah = to_hr_ah(ud_wr(wr)->ah);
    /* Set se attr */
    roce_set_bit(ud_sq_wqe->byte_4, V2_UD_SEND_WQE_BYTE_4_SE_S,
                 ((unsigned int)wr->send_flags & IB_SEND_SOLICITED) ? 1 : 0);

    roce_set_bit(ud_sq_wqe->byte_4,
                 V2_UD_SEND_WQE_BYTE_4_OWNER_S, owner_bit);

    roce_set_field(ud_sq_wqe->byte_16, V2_UD_SEND_WQE_BYTE_16_PD_M,
                   V2_UD_SEND_WQE_BYTE_16_PD_S, to_hr_pd(ibqp->pd)->pdn);

    roce_set_field(ud_sq_wqe->byte_16, V2_UD_SEND_WQE_BYTE_16_SGE_NUM_M,
                   V2_UD_SEND_WQE_BYTE_16_SGE_NUM_S, wr->num_sge);

    roce_set_field(ud_sq_wqe->byte_20, V2_UD_SEND_WQE_BYTE_20_MSG_START_SGE_IDX_M,
                   V2_UD_SEND_WQE_BYTE_20_MSG_START_SGE_IDX_S,
                   (*sge_ind) & ((unsigned int)qp->sge.sge_cnt - 1));

    roce_set_field(ud_sq_wqe->byte_24, V2_UD_SEND_WQE_BYTE_24_UDPSPN_M,
                   V2_UD_SEND_WQE_BYTE_24_UDPSPN_S, 0);
    ud_sq_wqe->qkey =
        cpu_to_le32(ud_wr(wr)->remote_qkey & 0x80000000 ?
                    qp->qkey : ud_wr(wr)->remote_qkey);
    roce_set_field(ud_sq_wqe->byte_32, V2_UD_SEND_WQE_BYTE_32_DQPN_M,
                   V2_UD_SEND_WQE_BYTE_32_DQPN_S, ud_wr(wr)->remote_qpn);

    roce_set_field(ud_sq_wqe->byte_36, V2_UD_SEND_WQE_BYTE_36_VLAN_M,
                   V2_UD_SEND_WQE_BYTE_36_VLAN_S, le16_to_cpu(ah->av.vlan));
    roce_set_field(ud_sq_wqe->byte_36, V2_UD_SEND_WQE_BYTE_36_HOPLIMIT_M,
                   V2_UD_SEND_WQE_BYTE_36_HOPLIMIT_S, ah->av.hop_limit);
    roce_set_field(ud_sq_wqe->byte_36, V2_UD_SEND_WQE_BYTE_36_TCLASS_M,
                   V2_UD_SEND_WQE_BYTE_36_TCLASS_S, ah->av.tclass);
    roce_set_field(ud_sq_wqe->byte_40, V2_UD_SEND_WQE_BYTE_40_FLOW_LABEL_M,
                   V2_UD_SEND_WQE_BYTE_40_FLOW_LABEL_S, ah->av.flowlabel);
    roce_set_field(ud_sq_wqe->byte_40, V2_UD_SEND_WQE_BYTE_40_SL_M,
                   V2_UD_SEND_WQE_BYTE_40_SL_S, ah->av.sl & 0x7);
    roce_set_field(ud_sq_wqe->byte_40, V2_UD_SEND_WQE_BYTE_40_PORTN_M,
                   V2_UD_SEND_WQE_BYTE_40_PORTN_S, qp->port);

    roce_set_bit(ud_sq_wqe->byte_40, V2_UD_SEND_WQE_BYTE_40_UD_VLAN_EN_S,
                 ah->av.vlan_en ? 1 : 0);
    roce_set_field(ud_sq_wqe->byte_48, V2_UD_SEND_WQE_BYTE_48_SGID_INDX_M,
                   V2_UD_SEND_WQE_BYTE_48_SGID_INDX_S, ah->av.gid_index);
}

static int hns_roce_v2_set_ud_wqe(struct ib_qp *ibqp, const struct ib_send_wr *wr,
    char *wqe, unsigned int owner_bit, unsigned int *sge_ind)
{
    struct hns_roce_v2_ud_send_wqe *ud_sq_wqe = NULL;
    struct hns_roce_ah *ah = NULL;
    struct hns_roce_qp *qp = NULL;
    int ret;
    struct hns_roce_dev *hr_dev = NULL;

    hr_dev = to_hr_dev(ibqp->device);
    ah = to_hr_ah(ud_wr(wr)->ah);
    qp = to_hr_qp(ibqp);

    ud_sq_wqe = (struct hns_roce_v2_ud_send_wqe *)wqe;
    ret = memset_s(ud_sq_wqe, sizeof(*ud_sq_wqe), 0, sizeof(*ud_sq_wqe));
    if (ret) {
        dev_err(hr_dev->dev, "memset sq_wqe fail ret[%d]\n", ret);
        return ret;
    }

    hns_roce_v2_set_ud_wqe_dmac(ud_sq_wqe, ah);

    /* When lbi is set, the roce port support loopback */
    roce_set_bit(ud_sq_wqe->byte_40,
                 V2_UD_SEND_WQE_BYTE_40_LBI_S, hr_dev->loop_idc);

    roce_set_field(ud_sq_wqe->byte_4, V2_UD_SEND_WQE_BYTE_4_OPCODE_M,
                   V2_UD_SEND_WQE_BYTE_4_OPCODE_S, HNS_ROCE_V2_WQE_OP_SEND);

    hns_roce_v2_set_ud_wqe_msg_len(ud_sq_wqe, wr);
    hns_roce_v2_set_ud_wqe_immtdata(ud_sq_wqe, wr);

    /* Set sig attr */
    roce_set_bit(ud_sq_wqe->byte_4,
                 V2_UD_SEND_WQE_BYTE_4_CQE_S,
                 ((unsigned int)wr->send_flags & IB_SEND_SIGNALED) ? 1 : 0);

    hns_roce_v2_set_ud_wqe_attr(ibqp, wr, ud_sq_wqe, owner_bit, sge_ind);

    ret = memcpy_s(&ud_sq_wqe->dgid[0], GID_LEN_V2, &ah->av.dgid[0], GID_LEN_V2);
    if (ret) {
        dev_err(hr_dev->dev, "memset sq_wqe dgid fail ret[%d]\n", ret);
        return ret;
    }

    set_extend_sge(qp, wr, sge_ind);
    return 0;
}

static void hns_roce_v2_set_rc_wqe_msg_len(
    struct hns_roce_v2_rc_send_wqe *rc_sq_wqe, const struct ib_send_wr *wr)
{
    int i;
    u32 tmp_len = 0;
    for (i = 0; i < wr->num_sge; i++) {
        tmp_len += wr->sg_list[i].length;
    }

    rc_sq_wqe->msg_len =
        cpu_to_le32(le32_to_cpu(rc_sq_wqe->msg_len) + tmp_len);
}

static void hns_roce_v2_set_rc_wqe_immtdata(
    struct hns_roce_v2_rc_send_wqe *rc_sq_wqe,
    const struct ib_send_wr *wr)
{
    switch (wr->opcode) {
        case IB_WR_SEND_WITH_IMM:
        case IB_WR_RDMA_WRITE_WITH_IMM:
            rc_sq_wqe->immtdata =
                cpu_to_le32(be32_to_cpu(wr->ex.imm_data));
            break;
        case IB_WR_SEND_WITH_INV:
            rc_sq_wqe->inv_key =
                cpu_to_le32(wr->ex.invalidate_rkey);
            break;
        default:
            rc_sq_wqe->immtdata = 0;
            break;
    }
}

static u32 hns_roce_v2_set_rc_wqe_with_atomic(
    struct hns_roce_v2_rc_send_wqe *rc_sq_wqe, const struct ib_send_wr *wr)
{
    u32 hr_op = 0;
    switch (wr->opcode) {
        case IB_WR_ATOMIC_CMP_AND_SWP:
            hr_op = HNS_ROCE_V2_WQE_OP_ATOM_CMP_AND_SWAP;
            rc_sq_wqe->rkey = cpu_to_le32(atomic_wr(wr)->rkey);
            rc_sq_wqe->va = cpu_to_le64(atomic_wr(wr)->remote_addr);
            break;
        case IB_WR_ATOMIC_FETCH_AND_ADD:
            hr_op = HNS_ROCE_V2_WQE_OP_ATOM_FETCH_AND_ADD;
            rc_sq_wqe->rkey = cpu_to_le32(atomic_wr(wr)->rkey);
            rc_sq_wqe->va = cpu_to_le64(atomic_wr(wr)->remote_addr);
            break;
        case IB_WR_MASKED_ATOMIC_CMP_AND_SWP:
            hr_op = HNS_ROCE_V2_WQE_OP_ATOM_MSK_CMP_AND_SWAP;
            break;
        case IB_WR_MASKED_ATOMIC_FETCH_AND_ADD:
            hr_op = HNS_ROCE_V2_WQE_OP_ATOM_MSK_FETCH_AND_ADD;
            break;
        default:
            break;
    }
    return hr_op;
}

static u32 hns_roce_v2_set_rc_wqe_with_write(
    struct hns_roce_v2_rc_send_wqe *rc_sq_wqe, const struct ib_send_wr *wr)
{
    u32 hr_op = 0;
    switch (wr->opcode) {
        case IB_WR_RDMA_WRITE:
            hr_op = HNS_ROCE_V2_WQE_OP_RDMA_WRITE;
            rc_sq_wqe->rkey = cpu_to_le32(rdma_wr(wr)->rkey);
            rc_sq_wqe->va = cpu_to_le64(rdma_wr(wr)->remote_addr);
            break;
        case IB_WR_RDMA_WRITE_WITH_IMM:
            hr_op = HNS_ROCE_V2_WQE_OP_RDMA_WRITE_WITH_IMM;
            rc_sq_wqe->rkey = cpu_to_le32(rdma_wr(wr)->rkey);
            rc_sq_wqe->va = cpu_to_le64(rdma_wr(wr)->remote_addr);
            break;
        default:
            break;
    }
    return hr_op;
}

static u32 hns_roce_v2_set_rc_wqe_with_read(
    struct hns_roce_v2_rc_send_wqe *rc_sq_wqe, const struct ib_send_wr *wr)
{
    u32 hr_op = 0;
    if (wr->opcode == IB_WR_RDMA_READ) {
        hr_op = HNS_ROCE_V2_WQE_OP_RDMA_READ;
        rc_sq_wqe->rkey = cpu_to_le32(rdma_wr(wr)->rkey);
        rc_sq_wqe->va = cpu_to_le64(rdma_wr(wr)->remote_addr);
    }
    return hr_op;
}

static u32 hns_roce_v2_set_rc_wqe_with_local_inv(
    struct hns_roce_v2_rc_send_wqe *rc_sq_wqe, const struct ib_send_wr *wr)
{
    u32 hr_op = 0;
    if (wr->opcode == IB_WR_LOCAL_INV) {
        hr_op = HNS_ROCE_V2_WQE_OP_LOCAL_INV;
        roce_set_bit(rc_sq_wqe->byte_4, V2_RC_SEND_WQE_BYTE_4_SO_S, 1);
        rc_sq_wqe->inv_key = cpu_to_le32(wr->ex.invalidate_rkey);
    }
    return hr_op;
}

static void hns_roce_v2_set_rc_wqe_opcode(
    struct hns_roce_v2_rc_send_wqe *rc_sq_wqe, const struct ib_send_wr *wr, char *wqe)
{
    u32 hr_op = 0;
    struct hns_roce_wqe_frmr_seg *fseg = (struct hns_roce_wqe_frmr_seg *)wqe;
    switch (wr->opcode) {
        case IB_WR_RDMA_READ:
            hr_op = hns_roce_v2_set_rc_wqe_with_read(rc_sq_wqe, wr);
            break;
        case IB_WR_RDMA_WRITE:
            hr_op = hns_roce_v2_set_rc_wqe_with_write(rc_sq_wqe, wr);
            break;
        case IB_WR_RDMA_WRITE_WITH_IMM:
            hr_op = hns_roce_v2_set_rc_wqe_with_write(rc_sq_wqe, wr);
            break;
        case IB_WR_SEND:
            hr_op = HNS_ROCE_V2_WQE_OP_SEND;
            break;
        case IB_WR_SEND_WITH_INV:
            hr_op = HNS_ROCE_V2_WQE_OP_SEND_WITH_INV;
            break;
        case IB_WR_SEND_WITH_IMM:
            hr_op = HNS_ROCE_V2_WQE_OP_SEND_WITH_IMM;
            break;
        case IB_WR_LOCAL_INV:
            hr_op = HNS_ROCE_V2_WQE_OP_LOCAL_INV;
            hns_roce_v2_set_rc_wqe_with_local_inv(rc_sq_wqe, wr);
            break;
        case IB_WR_REG_MR:
            hr_op = HNS_ROCE_V2_WQE_OP_FAST_REG_PMR;
            set_frmr_seg(rc_sq_wqe, fseg, reg_wr(wr));
            break;
        case IB_WR_ATOMIC_CMP_AND_SWP:
            /* fall-through */
        case IB_WR_ATOMIC_FETCH_AND_ADD:
            /* fall-through */
        case IB_WR_MASKED_ATOMIC_CMP_AND_SWP:
            /* fall-through */
        case IB_WR_MASKED_ATOMIC_FETCH_AND_ADD:
            hr_op = hns_roce_v2_set_rc_wqe_with_atomic(rc_sq_wqe, wr);
            break;
        default:
            hr_op = HNS_ROCE_V2_WQE_OP_MASK;
            break;
    }

    roce_set_field(rc_sq_wqe->byte_4, V2_RC_SEND_WQE_BYTE_4_OPCODE_M,
                   V2_RC_SEND_WQE_BYTE_4_OPCODE_S, hr_op);
}

static int hns_roce_v2_set_rc_wqe(struct ib_qp *ibqp,
    const struct ib_send_wr *wr, const struct ib_send_wr **bad_wr,
    struct hns_roce_wqe_sge *wqe_sge, unsigned int owner_bit)
{
    int ret;
    struct hns_roce_dev *hr_dev = NULL;
    struct hns_roce_v2_wqe_data_seg *dseg = NULL;
    struct hns_roce_v2_rc_send_wqe *rc_sq_wqe = NULL;
    hr_dev = to_hr_dev(ibqp->device);

    rc_sq_wqe = (struct hns_roce_v2_rc_send_wqe *)wqe_sge->wqe;
    ret = memset_s(rc_sq_wqe, sizeof(*rc_sq_wqe), 0, sizeof(*rc_sq_wqe));
    if (ret) {
        dev_err(hr_dev->dev, "memset rc_sq_wqe fail ret[%d]\n", ret);
        return ret;
    }

    hns_roce_v2_set_rc_wqe_msg_len(rc_sq_wqe, wr);
    hns_roce_v2_set_rc_wqe_immtdata(rc_sq_wqe, wr);

    roce_set_bit(rc_sq_wqe->byte_4, V2_RC_SEND_WQE_BYTE_4_FENCE_S,
                 ((unsigned int)wr->send_flags & IB_SEND_FENCE) ? 1 : 0);

    roce_set_bit(rc_sq_wqe->byte_4, V2_RC_SEND_WQE_BYTE_4_SE_S,
                 ((unsigned int)wr->send_flags & IB_SEND_SOLICITED) ? 1 : 0);

    roce_set_bit(rc_sq_wqe->byte_4, V2_RC_SEND_WQE_BYTE_4_CQE_S,
                 ((unsigned int)wr->send_flags & IB_SEND_SIGNALED) ? 1 : 0);

    roce_set_bit(rc_sq_wqe->byte_4,
                 V2_RC_SEND_WQE_BYTE_4_OWNER_S, owner_bit);

    wqe_sge->wqe += sizeof(struct hns_roce_v2_rc_send_wqe);
    hns_roce_v2_set_rc_wqe_opcode(rc_sq_wqe, wr, wqe_sge->wqe);

    if (wr->opcode == IB_WR_ATOMIC_CMP_AND_SWP ||
            wr->opcode == IB_WR_ATOMIC_FETCH_AND_ADD) {
        dseg = (struct hns_roce_v2_wqe_data_seg *)wqe_sge->wqe;
        set_data_seg_v2(dseg, wr->sg_list);
        wqe_sge->wqe += sizeof(struct hns_roce_v2_wqe_data_seg);
        set_atomic_seg((struct hns_roce_wqe_atomic_seg *)wqe_sge->wqe, atomic_wr(wr));
        roce_set_field(rc_sq_wqe->byte_16, V2_RC_SEND_WQE_BYTE_16_SGE_NUM_M,
                       V2_RC_SEND_WQE_BYTE_16_SGE_NUM_S, wr->num_sge);
    } else if (wr->opcode != IB_WR_REG_MR) {
        ret = set_rwqe_data_seg(ibqp, wr, rc_sq_wqe, wqe_sge, bad_wr);
        if (ret) {
            dev_err(hr_dev->dev, "set rwqe data seg failed, ret[%d]\n", ret);
            return ret;
        }
    }
    return 0;
}

static int hns_roce_v2_set_wqe(struct ib_qp *ibqp,
    const struct ib_send_wr *wr, const struct ib_send_wr **bad_wr,
    struct hns_roce_wqe_sge *wqe_sge, unsigned int owner_bit)
{
    int ret;
    struct hns_roce_dev *hr_dev;
    hr_dev = to_hr_dev(ibqp->device);

    if (ibqp->qp_type == IB_QPT_GSI || ibqp->qp_type == IB_QPT_UD) {
        ret = hns_roce_v2_set_ud_wqe(ibqp, wr, wqe_sge->wqe, owner_bit, &wqe_sge->sge_ind);
        if (ret) {
            dev_err(hr_dev->dev, "set ud wqe failed, ret:[%d]", ret);
            *bad_wr = wr;
            return -EINVAL;
        }
    } else if (ibqp->qp_type == IB_QPT_RC ||
               ibqp->qp_type == IB_QPT_UC) {
        ret = hns_roce_v2_set_rc_wqe(ibqp, wr, bad_wr, wqe_sge, owner_bit);
        if (ret) {
            dev_err(hr_dev->dev, "set rc uc wqe failed, ret:[%d]", ret);
            *bad_wr = wr;
            return -EINVAL;
        }
    } else {
        dev_err(hr_dev->dev, "Illegal qp(0x%x) type:0x%x\n", ibqp->qp_num, ibqp->qp_type);
        return -EOPNOTSUPP;
    }
    return 0;
}

static void hns_roce_v2_post_send_handle_out(struct hns_roce_dev *hr_dev, struct hns_roce_qp *qp,
    struct hns_roce_post_send_para *post_send_para)
{
    struct hns_roce_v2_db sq_db;

    if (likely(post_send_para->nreq)) {
        qp->sq.head += post_send_para->nreq;
        /* Memory barrier */
        wmb();

        sq_db.byte_4 = 0;
        sq_db.parameter = 0;

        roce_set_field(sq_db.byte_4, V2_DB_BYTE_4_TAG_M, V2_DB_BYTE_4_TAG_S, qp->doorbell_qpn);
        roce_set_field(sq_db.byte_4, V2_DB_BYTE_4_CMD_M, V2_DB_BYTE_4_CMD_S, HNS_ROCE_V2_SQ_DB);
        roce_set_field(sq_db.parameter, V2_DB_PARAMETER_IDX_M, V2_DB_PARAMETER_IDX_S,
                       qp->sq.head & (((unsigned int)qp->sq.wqe_cnt << 1) - 1));
        roce_set_field(sq_db.parameter, V2_DB_PARAMETER_SL_M, V2_DB_PARAMETER_SL_S, qp->sl);
        /* when qp is err, stop to write db */
        if (qp->state == IB_QPS_ERR) {
            init_flush_work(hr_dev, qp);
            qp->sq_next_wqe = post_send_para->ind;
            qp->next_sge = post_send_para->wqe_sge.sge_ind;
            return;
        }

        hns_roce_write64(hr_dev, (__le32 *)&sq_db, 1, qp->sq.db_reg_l);
        qp->sq_next_wqe = post_send_para->ind;
        qp->next_sge = post_send_para->wqe_sge.sge_ind;
    }

    return;
}

static int hns_roce_v2_get_send_wqe(struct ib_qp *ibqp, const struct ib_send_wr *wr, struct hns_roce_qp *qp,
    struct hns_roce_post_send_para *post_send_para, const struct ib_send_wr **bad_wr)
{
    int ret;
    unsigned int owner_bit;
    struct hns_roce_dev *hr_dev = to_hr_dev(ibqp->device);
    struct device *dev = hr_dev->dev;

    if (hns_roce_wq_overflow(&qp->sq, post_send_para->nreq, qp->ibqp.send_cq)) {
        *bad_wr = wr;
        dev_err(dev, "qp(0x%x): wq overflow, nreq=0x%x\n", ibqp->qp_num, post_send_para->nreq);
        return -ENOMEM;
    }

    if (unlikely(wr->num_sge > qp->sq.max_gs)) {
        dev_err(dev, "num_sge=%d > qp->sq.max_gs=%d\n", wr->num_sge, qp->sq.max_gs);
        *bad_wr = wr;
        return -EINVAL;
    }

    post_send_para->wqe_sge.wqe = get_send_wqe(qp, (qp->sq_next_wqe) & (unsigned int)(qp->sq.wqe_cnt - 1));
    qp->sq.wrid[(unsigned int)(qp->sq.head + post_send_para->nreq) & (unsigned int)(qp->sq.wqe_cnt - 1)] = wr->wr_id;
    owner_bit = ~(((unsigned int)(qp->sq.head + (unsigned int)post_send_para->nreq) >>
        (unsigned int)ilog2((unsigned int)qp->sq.wqe_cnt)) & 0x1);

    /* Corresponding to the QP type, wqe process separately */
    ret = hns_roce_v2_set_wqe(ibqp, wr, bad_wr, &(post_send_para->wqe_sge), owner_bit);
    if (ret) {
        dev_err(dev, "hns_roce_v2_set_wqe err\n");
        *bad_wr = wr;
        return ret;
    }
    post_send_para->ind++;

    return 0;
}

int hns_roce_v2_post_send(struct ib_qp *ibqp, const struct ib_send_wr *wr,
    const struct ib_send_wr **bad_wr)
{
    struct hns_roce_dev *hr_dev = NULL;
    struct hns_roce_ah *ah = NULL;
    struct hns_roce_qp *qp = NULL;
    struct device *dev = NULL;
    int ret;
    struct hns_roce_post_send_para post_send_para = {0};

    if (ibqp == NULL || wr == NULL || bad_wr == NULL) {
        pr_err("hns3: null point:ibqp %pK, wr %pK, bad_wr %pK\n", ibqp, wr, bad_wr);
        return -EINVAL;
    }
    hr_dev = to_hr_dev(ibqp->device);
    ah = to_hr_ah(ud_wr(wr)->ah);
    qp = to_hr_qp(ibqp);
    dev = hr_dev->dev;

    ret = hns_roce_v2_supp_type(dev, ibqp, qp);
    if (ret) {
        *bad_wr = wr;
        dev_err(dev, "unsupport type\n");
        return ret;
    }

    spin_lock_irqsave(&qp->sq.lock, post_send_para.flags);
    ROCE_DEV_STATE_CHECK_GOTO_OUT(hr_dev->state, bad_wr, wr, ret, post_send_para.nreq);

    post_send_para.ind = qp->sq_next_wqe;
    post_send_para.wqe_sge.sge_ind = qp->next_sge;

    for (post_send_para.nreq = 0; wr != NULL; ++post_send_para.nreq, wr = wr->next) {
        ret = hns_roce_v2_get_send_wqe(ibqp, wr, qp, &post_send_para, bad_wr);
        if (ret) {
            if (ret == -EOPNOTSUPP) {
                spin_unlock_irqrestore(&qp->sq.lock, post_send_para.flags);
                dev_err(dev, "get_send_wqe fail, Illegal qp(0x%x) type:0x%x\n", ibqp->qp_num, ibqp->qp_type);
                return ret;
            }
            dev_err(dev, "get_send_wqe fail, ret:%d, Illegal qp(0x%x) type:0x%x\n", ret, ibqp->qp_num, ibqp->qp_type);
            goto out;
        }
    }

out:
    hns_roce_v2_post_send_handle_out(hr_dev, qp, &post_send_para);
    spin_unlock_irqrestore(&qp->sq.lock, post_send_para.flags);
    return ret;
}

static int hns_roce_v2_post_recv_one(struct ib_qp *ibqp, const struct ib_recv_wr *wr,
                                     const struct ib_recv_wr **bad_wr, int nreq, int *ind)
{
    struct hns_roce_dev *hr_dev = to_hr_dev(ibqp->device);
    struct hns_roce_qp *hr_qp = to_hr_qp(ibqp);
    struct hns_roce_v2_wqe_data_seg *dseg = NULL;
    struct hns_roce_rinl_sge *sge_list = NULL;
    int i;

    if (hns_roce_wq_overflow(&hr_qp->rq, nreq, hr_qp->ibqp.recv_cq)) {
        *bad_wr = wr;
        return -ENOMEM;
    }

    if (unlikely(wr->num_sge > hr_qp->rq.max_gs)) {
        *bad_wr = wr;
        return -EINVAL;
    }

    dseg = (struct hns_roce_v2_wqe_data_seg *)get_recv_wqe(hr_qp, *ind);
    for (i = 0; i < wr->num_sge; i++) {
        if (!wr->sg_list[i].length) {
            continue;
        }
        set_data_seg_v2(dseg, wr->sg_list + i);
        dseg++;
    }

    if (i < hr_qp->rq.max_gs) {
        dseg->lkey = cpu_to_le32(HNS_ROCE_INVALID_LKEY);
        dseg->addr = 0;
    }

    /* rq support inline data */
    if (hr_dev->caps.flags & HNS_ROCE_CAP_FLAG_RQ_INLINE) {
        sge_list = hr_qp->rq_inl_buf.wqe_list[*ind].sg_list;
        hr_qp->rq_inl_buf.wqe_list[*ind].sge_cnt = (u32)wr->num_sge;
        for (i = 0; i < wr->num_sge; i++) {
            sge_list[i].addr = (void *)(uintptr_t)wr->sg_list[i].addr;
            sge_list[i].len = wr->sg_list[i].length;
        }
    }

    hr_qp->rq.wrid[*ind] = wr->wr_id;
    *ind = ((unsigned int)*ind + 1) & ((unsigned int)hr_qp->rq.wqe_cnt - 1);

    return 0;
}

int hns_roce_v2_post_recv(struct ib_qp *ibqp,
    const struct ib_recv_wr *wr,
    const struct ib_recv_wr **bad_wr)
{
    struct hns_roce_dev *hr_dev = NULL;
    struct hns_roce_qp *hr_qp = NULL;
    struct device *dev = NULL;
    unsigned long flags;
    int ret = 0;
    int nreq;
    int ind;

    if (ibqp == NULL || bad_wr == NULL) {
        pr_err("hns3: null point: ibqp %pK, bad_wr %pK\n", ibqp, bad_wr);
        return -EINVAL;
    }
    hr_dev = to_hr_dev(ibqp->device);
    hr_qp = to_hr_qp(ibqp);
    dev = hr_dev->dev;

    spin_lock_irqsave(&hr_qp->rq.lock, flags);
    ind = hr_qp->rq.head & ((unsigned int)hr_qp->rq.wqe_cnt - 1);

    if (hr_qp->state == IB_QPS_RESET) {
        spin_unlock_irqrestore(&hr_qp->rq.lock, flags);
        dev_err(hr_dev->dev, "hr_qp status == IB_QPS_RESET[%u]\n", IB_QPS_RESET);
        *bad_wr = wr;
        return -EINVAL;
    }

    for (nreq = 0; wr; ++nreq, wr = wr->next) {
        ret = hns_roce_v2_post_recv_one(ibqp, wr, bad_wr, nreq, &ind);
        if (ret) {
            goto out;
        }
    }

out:
    if (likely(nreq)) {
        hr_qp->rq.head += (u32)nreq;
        /* Memory barrier */
        wmb();

        /* when qp is err, stop to write record db */
        if (hr_qp->state == IB_QPS_ERR) {
                init_flush_work(hr_dev, hr_qp);
        } else {
            *hr_qp->rdb.db_record = hr_qp->rq.head & SHORT_MASK;
        }
    }

    spin_unlock_irqrestore(&hr_qp->rq.lock, flags);

    if (ret) {
        dev_err(hr_dev->dev, "post recv err, ret:%d\n", ret);
    }

    return ret;
}

static int hns_roce_v2_cmd_hw_reseted(struct hns_roce_dev *hr_dev,
                                      unsigned long instance_stage,
                                      unsigned long reset_stage)
{
    /* When hardware reset has been completed once or more, we should stop
     * sending mailbox&cmq&doorbell to hardware. If now in .init_instance()
     * function, we should exit with error. If now at HNAE3_INIT_CLIENT
     * stage of soft reset process, we should exit with error, and then
     * HNAE3_INIT_CLIENT related process can rollback the operation like
     * notifing hardware to free resources, HNAE3_INIT_CLIENT related
     * process will exit with error to notify NIC driver to reschedule soft
     * reset process once again.
     */
    hr_dev->is_reset = true;
    hr_dev->dis_db = true;
    if (reset_stage == HNS_ROCE_STATE_RST_INIT ||
            instance_stage == HNS_ROCE_STATE_INIT) {
        return CMD_RST_PRC_EBUSY;
    }

    return CMD_RST_PRC_SUCCESS;
}

static int hns_roce_v2_cmd_hw_resetting(struct hns_roce_dev *hr_dev,
                                        unsigned long instance_stage,
                                        unsigned long reset_stage)
{
#define HNS_ROCE_V2_TIME_US 1000
    struct hns_roce_v2_priv *priv = (struct hns_roce_v2_priv *)hr_dev->priv;
    struct hnae3_handle *handle = priv->handle;
    const struct hnae3_ae_ops *ops = handle->ae_algo->ops;
    unsigned long end;

    /* When hardware reset is detected, we should stop sending mailbox&cmq&
     * doorbell to hardware, and wait until hardware reset finished. If now
     * in .init_instance() function, we should exit with error. If now at
     * HNAE3_INIT_CLIENT stage of soft reset process, we should exit with
     * error, and then HNAE3_INIT_CLIENT related process can rollback the
     * operation like notifing hardware to free resources, HNAE3_INIT_CLIENT
     * related process will exit with error to notify NIC driver to
     * reschedule soft reset process once again.
     */
    hr_dev->dis_db = true;
    end = HNS_ROCE_V2_HW_RST_TIMEOUT * HNS_ROCE_V2_TIME_US;
    while (ops->get_hw_reset_stat(handle) && (end > 0)) {
        udelay(1);
        end--;
    }

    if (!ops->get_hw_reset_stat(handle)) {
        hr_dev->is_reset = true;
    } else {
        dev_warn(hr_dev->dev, "hw_resetting!\n");
    }

    if (!hr_dev->is_reset || reset_stage == HNS_ROCE_STATE_RST_INIT ||
            instance_stage == HNS_ROCE_STATE_INIT) {
        return CMD_RST_PRC_EBUSY;
    }

    return CMD_RST_PRC_SUCCESS;
}

static int hns_roce_v2_cmd_sw_resetting(struct hns_roce_dev *hr_dev)
{
#define HNS_ROCE_V2_TIME_US 1000
    struct hns_roce_v2_priv *priv = (struct hns_roce_v2_priv *)hr_dev->priv;
    struct hnae3_handle *handle = priv->handle;
    const struct hnae3_ae_ops *ops = handle->ae_algo->ops;
    unsigned long end;

    /* When software reset is detected at .init_instance() function, we
     * should stop sending mailbox&cmq&doorbell to hardware, and
     * wait until hardware reset finished, we should exit with error.
     */
    hr_dev->dis_db = true;
    end = HNS_ROCE_V2_HW_RST_TIMEOUT * HNS_ROCE_V2_TIME_US;
    while (ops->ae_dev_reset_cnt(handle) == hr_dev->reset_cnt &&
           (end > 0)) {
        udelay(1);
        end--;
    }

    if (ops->ae_dev_reset_cnt(handle) != hr_dev->reset_cnt) {
        hr_dev->is_reset = true;
    } else {
        dev_warn(hr_dev->dev, "reset_cnt no change!\n");
    }

    return CMD_RST_PRC_EBUSY;
}

int hns_roce_v2_rst_process_cmd(struct hns_roce_dev *hr_dev)
{
    struct hns_roce_v2_priv *priv = (struct hns_roce_v2_priv *)hr_dev->priv;
    struct hnae3_handle *handle = priv->handle;
    const struct hnae3_ae_ops *ops = handle->ae_algo->ops;
    unsigned long instance_stage;   /* the current instance stage */
    unsigned long reset_stage;  /* the current reset stage */
    unsigned long reset_cnt;
    bool sw_resetting = true;
    bool hw_resetting = true;

    if (hr_dev->is_reset) {
        return CMD_RST_PRC_SUCCESS;
    }

    /* Get information about reset from NIC driver or RoCE driver itself,
     * the meaning of the following variables from NIC driver are described
     * as below:
     * reset_cnt -- The count value of completed hardware reset.
     * hw_resetting -- Whether hardware device is resetting now.
     * sw_resetting -- Whether NIC's software reset process is running now.
     */
    instance_stage = handle->rinfo.instance_state;
    reset_stage = handle->rinfo.reset_state;
    reset_cnt = ops->ae_dev_reset_cnt(handle);
    hw_resetting = ops->get_hw_reset_stat(handle);
    sw_resetting = ops->ae_dev_resetting(handle);

    if (reset_cnt != hr_dev->reset_cnt) {
        return hns_roce_v2_cmd_hw_reseted(hr_dev, instance_stage,
                                          reset_stage);
    } else if (hw_resetting) {
        return hns_roce_v2_cmd_hw_resetting(hr_dev, instance_stage,
                                            reset_stage);
    } else if (sw_resetting && instance_stage == HNS_ROCE_STATE_INIT) {
        return hns_roce_v2_cmd_sw_resetting(hr_dev);
    }

    return 0;
}

static int hns_roce_cmq_space(struct hns_roce_v2_cmq_ring *ring)
{
    int ntu = ring->next_to_use;
    int ntc = ring->next_to_clean;
    int used = (ntu - ntc + ring->desc_num) % ring->desc_num;

    return ring->desc_num - used - 1;
}

static int hns_roce_alloc_cmq_desc(struct hns_roce_dev *hr_dev,
    struct hns_roce_v2_cmq_ring *ring)
{
    size_t size = ring->desc_num * sizeof(struct hns_roce_cmq_desc);

    ring->desc = kzalloc(size, GFP_KERNEL);
    if (ring->desc == NULL) {
        dev_err(hr_dev->dev, "alloc hns_roce_v2_cmq_ring desc failed\n");
        return -ENOMEM;
    }

    ring->desc_dma_addr = dma_map_single(hr_dev->dev, ring->desc, size,
                                         DMA_BIDIRECTIONAL);
    if (dma_mapping_error(hr_dev->dev, ring->desc_dma_addr)) {
        ring->desc_dma_addr = 0;
        kfree(ring->desc);
        ring->desc = NULL;
        dev_err(hr_dev->dev, "dma mapping error\n");
        return -ENOMEM;
    }

    return 0;
}

static void hns_roce_free_cmq_desc(struct hns_roce_dev *hr_dev,
    struct hns_roce_v2_cmq_ring *ring)
{
    dma_unmap_single(hr_dev->dev, ring->desc_dma_addr,
                     ring->desc_num * sizeof(struct hns_roce_cmq_desc),
                     DMA_BIDIRECTIONAL);

    ring->desc_dma_addr = 0;
    kfree(ring->desc);
    ring->desc = NULL;
}

STATIC int hns_roce_init_cmq_ring(struct hns_roce_dev *hr_dev, bool ring_type)
{
    struct hns_roce_v2_priv *priv = (struct hns_roce_v2_priv *)hr_dev->priv;
    struct hns_roce_v2_cmq_ring *ring = (ring_type == TYPE_CSQ) ?
                                        &priv->cmq.csq : &priv->cmq.crq;

    ring->flag = ring_type;
    ring->next_to_clean = 0;
    ring->next_to_use = 0;

    return hns_roce_alloc_cmq_desc(hr_dev, ring);
}

static void hns_roce_cmq_init_regs(struct hns_roce_dev *hr_dev, bool ring_type)
{
    struct hns_roce_v2_priv *priv = (struct hns_roce_v2_priv *)hr_dev->priv;
    struct hns_roce_v2_cmq_ring *ring = (ring_type == TYPE_CSQ) ?
                                        &priv->cmq.csq : &priv->cmq.crq;
    dma_addr_t dma = ring->desc_dma_addr;

    if (ring_type == TYPE_CSQ) {
        roce_write(hr_dev, ROCEE_TX_CMQ_BASEADDR_L_REG, (u32)dma);
        roce_write(hr_dev, ROCEE_TX_CMQ_BASEADDR_H_REG,
                   upper_32_bits(dma));
        roce_write(hr_dev, ROCEE_TX_CMQ_DEPTH_REG,
                   (ring->desc_num >> HNS_ROCE_CMQ_DESC_NUM_S) |
                   HNS_ROCE_CMQ_ENABLE);
        roce_write(hr_dev, ROCEE_TX_CMQ_HEAD_REG, 0);
        roce_write(hr_dev, ROCEE_TX_CMQ_TAIL_REG, 0);
    } else {
        roce_write(hr_dev, ROCEE_RX_CMQ_BASEADDR_L_REG, (u32)dma);
        roce_write(hr_dev, ROCEE_RX_CMQ_BASEADDR_H_REG,
                   upper_32_bits(dma));
        roce_write(hr_dev, ROCEE_RX_CMQ_DEPTH_REG,
                   (ring->desc_num >> HNS_ROCE_CMQ_DESC_NUM_S) |
                   HNS_ROCE_CMQ_ENABLE);
        roce_write(hr_dev, ROCEE_RX_CMQ_HEAD_REG, 0);
        roce_write(hr_dev, ROCEE_RX_CMQ_TAIL_REG, 0);
    }
}

int hns_roce_v2_cmq_init(struct hns_roce_dev *hr_dev)
{
    struct hns_roce_v2_priv *priv = (struct hns_roce_v2_priv *)hr_dev->priv;
    int ret;

    /* Setup the queue entries for command queue */
    priv->cmq.csq.desc_num = CMD_CSQ_DESC_NUM;
    priv->cmq.crq.desc_num = CMD_CRQ_DESC_NUM;

    /* Setup the lock for command queue */
    spin_lock_init(&priv->cmq.csq.lock);
    spin_lock_init(&priv->cmq.crq.lock);

    /* Setup Tx write back timeout */
    priv->cmq.tx_timeout = HNS_ROCE_CMQ_TX_TIMEOUT;

    /* Init CSQ */
    ret = hns_roce_init_cmq_ring(hr_dev, TYPE_CSQ);
    if (ret) {
        dev_err(hr_dev->dev, "Init CSQ error, ret = %d.\n", ret);
        return ret;
    }

    /* Init CRQ */
    ret = hns_roce_init_cmq_ring(hr_dev, TYPE_CRQ);
    if (ret) {
#ifndef DEFINE_HNS_LLT
        dev_err(hr_dev->dev, "Init CRQ error, ret = %d.\n", ret);
        goto err_crq;
#endif
    }

    /* Init CSQ REG */
    hns_roce_cmq_init_regs(hr_dev, TYPE_CSQ);

    /* Init CRQ REG */
    hns_roce_cmq_init_regs(hr_dev, TYPE_CRQ);

    return 0;
#ifndef DEFINE_HNS_LLT
err_crq:
    hns_roce_free_cmq_desc(hr_dev, &priv->cmq.csq);

    return ret;
#endif
}

void hns_roce_v2_cmq_exit(struct hns_roce_dev *hr_dev)
{
    struct hns_roce_v2_priv *priv = (struct hns_roce_v2_priv *)hr_dev->priv;

    hns_roce_free_cmq_desc(hr_dev, &priv->cmq.csq);
    hns_roce_free_cmq_desc(hr_dev, &priv->cmq.crq);
}

void hns_roce_cmq_setup_basic_desc(struct hns_roce_cmq_desc *desc,
    enum hns_roce_opcode_type opcode,
    bool is_read)
{
    int ret;

    ret = memset_s((void *)desc, sizeof(struct hns_roce_cmq_desc),
                   0, sizeof(struct hns_roce_cmq_desc));
    HNS_ROCE_SEC_CHECK_RET_VOID_WITHOUT_DEV(ret);

    desc->opcode = cpu_to_le16(opcode);
    desc->flag = cpu_to_le16(HNS_ROCE_CMD_FLAG_NO_INTR | HNS_ROCE_CMD_FLAG_IN);
    if (is_read) {
        desc->flag |= cpu_to_le16(HNS_ROCE_CMD_FLAG_WR);
    } else {
        desc->flag &= cpu_to_le16(~HNS_ROCE_CMD_FLAG_WR);
    }
}

static int hns_roce_cmq_csq_done(struct hns_roce_dev *hr_dev)
{
    struct hns_roce_v2_priv *priv = (struct hns_roce_v2_priv *)hr_dev->priv;
    u32 head = roce_read(hr_dev, ROCEE_TX_CMQ_HEAD_REG);

    return head == priv->cmq.csq.next_to_use;
}

static int hns_roce_cmq_csq_clean(struct hns_roce_dev *hr_dev)
{
    struct hns_roce_v2_priv *priv = (struct hns_roce_v2_priv *)hr_dev->priv;
    struct hns_roce_v2_cmq_ring *csq = &priv->cmq.csq;
    struct hns_roce_cmq_desc *desc = NULL;
    u16 ntc = csq->next_to_clean;
    u32 head;
    int clean = 0;
    int ret;

    desc = &csq->desc[ntc];
    head = roce_read(hr_dev, ROCEE_TX_CMQ_HEAD_REG);
    if (head >= csq->desc_num) {
        dev_err(hr_dev->dev, "Invalid head number\n");
        return -EAGAIN;
    }

    while (head != ntc) {
        ret = memset_s(desc, sizeof(*desc), 0, sizeof(*desc));
        HNS_ROCE_SEC_CHECK_RET_INT(hr_dev->dev, ret);

        ntc++;
        if (ntc == csq->desc_num) {
            ntc = 0;
        }
        desc = &csq->desc[ntc];
        clean++;
    }
    csq->next_to_clean = ntc;

    return clean;
}

static int hns_roce_wait_cmq_csq_done(struct hns_roce_dev *hr_dev,
    struct hns_roce_cmq_desc *desc, int num, int *ntc)
{
    struct hns_roce_v2_priv *priv = (struct hns_roce_v2_priv *)hr_dev->priv;
    struct hns_roce_v2_cmq_ring *csq = &priv->cmq.csq;
    struct hns_roce_cmq_desc *desc_to_use = NULL;
    bool complete = false;
    u32 timeout = 0;
    int handle = 0;
    u16 desc_ret;
    int ret = 0;

    /*
     * If the command is sync, wait for the firmware to write back,
     * if multi descriptors to be sent, use the first one to check
     */
    if ((desc->flag) & HNS_ROCE_CMD_FLAG_NO_INTR) {
        do {
            if (hns_roce_cmq_csq_done(hr_dev)) {
                break;
            }
            udelay(1);
            timeout++;
        } while (timeout < priv->cmq.tx_timeout);
    }

    if (hns_roce_cmq_csq_done(hr_dev)) {
        complete = true;
        handle = 0;
        while (handle < num) {
            /* get the result of hardware write back */
            desc_to_use = &csq->desc[*ntc];
            desc[handle] = *desc_to_use;
            desc_ret = desc[handle].retval;
            if (desc_ret == CMD_EXEC_SUCCESS) {
                ret = 0;
            } else {
                ret = -EIO;
            }
            priv->cmq.last_status = desc_ret;
            (*ntc)++;
            handle++;
            if (*ntc == csq->desc_num) {
                *ntc = 0;
            }
        }
    }

    if (!complete) {
        dev_warn(hr_dev->dev, "hns_roce_cmq_csq not completed\n");
        ret = -EAGAIN;
    }

    return ret;
}

static int __hns_roce_cmq_send(struct hns_roce_dev *hr_dev,
    struct hns_roce_cmq_desc *desc, int num)
{
    struct hns_roce_v2_priv *priv = (struct hns_roce_v2_priv *)hr_dev->priv;
    struct hns_roce_v2_cmq_ring *csq = &priv->cmq.csq;
    struct hns_roce_cmq_desc *desc_to_use = NULL;
    int handle = 0;
    int ret;
    int ntc;

    spin_lock_bh(&csq->lock);

    if (num > hns_roce_cmq_space(csq)) {
        dev_err(hr_dev->dev, "cmq num(%d) is out of space %pK\n", num, csq);
        spin_unlock_bh(&csq->lock);
        return -EBUSY;
    }

    /*
     * Record the location of desc in the cmq for this time
     * which will be use for hardware to write back
     */
    ntc = csq->next_to_use;

    while (handle < num) {
        desc_to_use = &csq->desc[csq->next_to_use];
        *desc_to_use = desc[handle];
        csq->next_to_use++;
        if (csq->next_to_use == csq->desc_num) {
            csq->next_to_use = 0;
        }
        handle++;
    }

    /* Write to hardware */
    roce_write(hr_dev, ROCEE_TX_CMQ_TAIL_REG, csq->next_to_use);
    ret = hns_roce_wait_cmq_csq_done(hr_dev, desc, num, &ntc);
    if (ret) {
        dev_warn(hr_dev->dev, "hns_roce_wait_cmq_csq_done err\n");
    }

    /* clean the command send queue */
    handle = hns_roce_cmq_csq_clean(hr_dev);
    if (handle != num) {
        dev_warn(hr_dev->dev, "cleaned %d, need to clean %d\n", handle, num);
    }

    spin_unlock_bh(&csq->lock);

    return ret;
}

int hns_roce_cmq_send(struct hns_roce_dev *hr_dev,
    struct hns_roce_cmq_desc *desc, int num)
{
    int retval;
    int ret;

    ret = hns_roce_v2_rst_process_cmd(hr_dev);
    if (ret == CMD_RST_PRC_SUCCESS) {
        return 0;
    }
    if (ret == CMD_RST_PRC_EBUSY) {
        return ret;
    }

    ret = __hns_roce_cmq_send(hr_dev, desc, num);
    if (ret) {
        retval = hns_roce_v2_rst_process_cmd(hr_dev);
        if (retval == CMD_RST_PRC_SUCCESS) {
            return 0;
        } else if (retval == CMD_RST_PRC_EBUSY) {
            return retval;
        }
    }

    return ret;
}

static int hns_roce_cmq_query_hw_info(struct hns_roce_dev *hr_dev)
{
    struct hns_roce_query_version *resp = NULL;
    struct hns_roce_cmq_desc desc;
    int ret;

    hns_roce_cmq_setup_basic_desc(&desc, HNS_ROCE_OPC_QUERY_HW_VER, true);
    ret = hns_roce_cmq_send(hr_dev, &desc, 1);
    HNS_ROCE_CMQ_SEND_RET_CHECK(ret);

    resp = (struct hns_roce_query_version *)desc.data;
    hr_dev->hw_rev = le32_to_cpu(resp->rocee_hw_version);
    hr_dev->vendor_id = hr_dev->pci_dev->vendor;

    return 0;
}

static bool hns_roce_func_clr_chk_rst(struct hns_roce_dev *hr_dev)
{
    struct hns_roce_v2_priv *priv = (struct hns_roce_v2_priv *)hr_dev->priv;
    struct hnae3_handle *handle = priv->handle;
    const struct hnae3_ae_ops *ops = handle->ae_algo->ops;
    unsigned long reset_cnt;
    bool sw_resetting;
    bool hw_resetting;

    reset_cnt = ops->ae_dev_reset_cnt(handle);
    hw_resetting = ops->get_hw_reset_stat(handle);
    sw_resetting = ops->ae_dev_resetting(handle);
    if (reset_cnt != hr_dev->reset_cnt || hw_resetting || sw_resetting) {
        return true;
    }

    return false;
}

static bool hns_roce_func_clr_hw_rst(struct hns_roce_dev *hr_dev)
{
    struct hns_roce_v2_priv *priv = (struct hns_roce_v2_priv *)hr_dev->priv;
    struct hnae3_handle *handle = priv->handle;
    const struct hnae3_ae_ops *ops = handle->ae_algo->ops;
    unsigned long end;

    hr_dev->dis_db = true;
    dev_warn(hr_dev->dev,
             "Func clear is pending, device in resetting state.\n");
    end = msecs_to_jiffies(HNS_ROCE_V2_HW_RST_TIMEOUT) + jiffies;
    while (time_before(jiffies, end)) {
        if (!ops->get_hw_reset_stat(handle)) {
            hr_dev->is_reset = true;
            dev_info(hr_dev->dev,
                     "Func clear success after reset.\n");
            return true;
        }
        msleep(HNS_ROCE_V2_HW_RST_COMPLETION_WAIT);
    }

    dev_err(hr_dev->dev, "Func clear failed.\n");
    return false;
}

static bool hns_roce_func_clr_sw_rst(struct hns_roce_dev *hr_dev)
{
    struct hns_roce_v2_priv *priv = (struct hns_roce_v2_priv *)hr_dev->priv;
    struct hnae3_handle *handle = priv->handle;
    const struct hnae3_ae_ops *ops = handle->ae_algo->ops;
    unsigned long end;

    hr_dev->dis_db = true;
    dev_warn(hr_dev->dev, "Func clear is pending, device in resetting state.\n");
    end = msecs_to_jiffies(HNS_ROCE_V2_HW_RST_TIMEOUT) + jiffies;
    while (time_before(jiffies, end)) {
        if (ops->ae_dev_reset_cnt(handle) !=
                hr_dev->reset_cnt) {
            hr_dev->is_reset = true;
            dev_info(hr_dev->dev,
                     "Func clear success after sw reset\n");
            return true;
        }
        msleep(HNS_ROCE_V2_HW_RST_COMPLETION_WAIT);
    }
    dev_warn(hr_dev->dev, "Func clear failed because of unfinished sw reset\n");

    return false;
}

static bool hns_roce_func_clr_rst_prc(struct hns_roce_dev *hr_dev, int retval,
    int flag)
{
    struct hns_roce_v2_priv *priv = (struct hns_roce_v2_priv *)hr_dev->priv;
    struct hnae3_handle *handle = priv->handle;
    const struct hnae3_ae_ops *ops = handle->ae_algo->ops;
    unsigned long instance_stage;
    unsigned long reset_cnt;
    bool sw_resetting;
    bool hw_resetting;
    bool fclr_read_after_write_flag = false;

    instance_stage = handle->rinfo.instance_state;
    reset_cnt = ops->ae_dev_reset_cnt(handle);
    hw_resetting = ops->get_hw_reset_stat(handle);
    sw_resetting = ops->ae_dev_resetting(handle);

    if (reset_cnt != hr_dev->reset_cnt) {
        hr_dev->dis_db = true;
        hr_dev->is_reset = true;
        dev_info(hr_dev->dev, "Func clear success after reset.\n");
    } else if (hw_resetting) {
        if (hns_roce_func_clr_hw_rst(hr_dev)) {
            goto out;
        }

        dev_warn(hr_dev->dev, "hns_roce_func_clr_hw_rst failed.\n");
    } else if (sw_resetting && instance_stage == HNS_ROCE_STATE_INIT) {
        if (hns_roce_func_clr_sw_rst(hr_dev)) {
            goto out;
        }

        dev_warn(hr_dev->dev, "hns_roce_func_clr_sw_rst failed.\n");
    } else {
        if (!flag) {
            fclr_read_after_write_flag = true;
        }
        if (retval && !flag) {
            dev_warn(hr_dev->dev, "Func clear read failed, ret = %d.\n", retval);
        }
    }

out:
    return fclr_read_after_write_flag;
}

void hns_roce_query_func_info(struct hns_roce_dev *hr_dev)
{
    struct hns_roce_cmq_desc desc;
    struct hns_roce_pf_func_info *resp = NULL;
    int ret;

    hns_roce_cmq_setup_basic_desc(&desc, HNS_ROCE_OPC_QUERY_FUNC_INFO, true);
    ret = hns_roce_cmq_send(hr_dev, &desc, 1);
    if (ret) {
        dev_err(hr_dev->dev, "Query vf count fail, ret = %d.\n", ret);
        return;
    }

    resp = (struct hns_roce_pf_func_info *)desc.data;
    hr_dev->func_num = resp->pf_own_func_num;
    hr_dev->mac_id = resp->pf_own_mac_id;
    hr_dev->port_num = resp->pf_own_port_num;
    hr_dev->port_id = resp->pf_own_port_id;
#ifdef DEFINE_HNS_LLT
    hr_dev->port_num = 1;
#endif
}

static void hns_roce_clear_func(struct hns_roce_dev *hr_dev, int vf_id)
{
    struct hns_roce_func_clear *resp = NULL;
    struct hns_roce_cmq_desc desc;
    unsigned long end;
    bool fclr_write_fail_flag = false;
    int ret = 0;

    if (hns_roce_func_clr_chk_rst(hr_dev)) {
        goto out;
    }

    resp = (struct hns_roce_func_clear *)desc.data;

    hns_roce_cmq_setup_basic_desc(&desc, HNS_ROCE_OPC_FUNC_CLEAR, false);
    resp->rst_funcid_en = vf_id;

    ret = hns_roce_cmq_send(hr_dev, &desc, 1);
    if (ret) {
        fclr_write_fail_flag = true;
        dev_err(hr_dev->dev, "Func clear write failed, ret = %d.\n", ret);
        goto out;
    }

    end = msecs_to_jiffies(HNS_ROCE_V2_FUNC_CLEAR_TIMEOUT_MSECS) + jiffies;

    msleep(HNS_ROCE_V2_READ_FUNC_CLEAR_FLAG_INTERVAL);
    while (time_before(jiffies, end)) {
        if (hns_roce_func_clr_chk_rst(hr_dev)) {
            ret = hns_roce_func_clr_rst_prc(hr_dev, ret, fclr_write_fail_flag);
            if (!ret) {
                return;
            }
        }

        hns_roce_cmq_setup_basic_desc(&desc, HNS_ROCE_OPC_FUNC_CLEAR,
                                      true);
        resp->rst_funcid_en = vf_id;

        ret = hns_roce_cmq_send(hr_dev, &desc, 1);
        if (ret) {
            msleep(HNS_ROCE_V2_READ_FUNC_CLEAR_FLAG_FAIL_WAIT);
            continue;
        }

        if (roce_get_bit(resp->func_done, FUNC_CLEAR_RST_FUN_DONE_S)) {
            if (vf_id == 0) {
                hr_dev->is_reset = true;
            }
            return;
        }
    }

out:
    (void)hns_roce_func_clr_rst_prc(hr_dev, ret, fclr_write_fail_flag);
}

void hns_roce_function_clear(struct hns_roce_dev *hr_dev)
{
    int i;
    int vf_num = 0; /* should be (hr_dev->func_num-1) when enable ROCE VF */

    /* Clear vf first, then clear pf */
    for (i = vf_num; i >= 0; i--) {
        hns_roce_clear_func(hr_dev, i);
    }
}

static int hns_roce_query_fw_ver(struct hns_roce_dev *hr_dev)
{
    struct hns_roce_query_fw_info *resp = NULL;
    struct hns_roce_cmq_desc desc;
    int ret;

    hns_roce_cmq_setup_basic_desc(&desc, HNS_QUERY_FW_VER, true);
    ret = hns_roce_cmq_send(hr_dev, &desc, 1);
    HNS_ROCE_CMQ_SEND_RET_CHECK(ret);

    resp = (struct hns_roce_query_fw_info *)desc.data;
    hr_dev->caps.fw_ver = (u64)(le32_to_cpu(resp->fw_ver));

    return 0;
}

static int hns_roce_config_global_param(struct hns_roce_dev *hr_dev)
{
    struct hns_roce_cfg_global_param *req = NULL;
    struct hns_roce_cmq_desc desc;
    int ret;

    hns_roce_cmq_setup_basic_desc(&desc, HNS_ROCE_OPC_CFG_GLOBAL_PARAM,
                                  false);

    req = (struct hns_roce_cfg_global_param *)desc.data;
    ret = memset_s(req, sizeof(*req), 0, sizeof(*req));
    HNS_ROCE_SEC_CHECK_RET_INT(hr_dev->dev, ret);
    roce_set_field(req->time_cfg_udp_port,
                   CFG_GLOBAL_PARAM_DATA_0_ROCEE_TIME_1US_CFG_M,
                   CFG_GLOBAL_PARAM_DATA_0_ROCEE_TIME_1US_CFG_S, 0x3e8);
    roce_set_field(req->time_cfg_udp_port,
                   CFG_GLOBAL_PARAM_DATA_0_ROCEE_UDP_PORT_M,
                   CFG_GLOBAL_PARAM_DATA_0_ROCEE_UDP_PORT_S, 0x12b7);

    return hns_roce_cmq_send(hr_dev, &desc, 1);
}

static int hns_roce_query_pf_resource(struct hns_roce_dev *hr_dev)
{
    struct hns_roce_cmq_desc desc[QUERY_PF_RES_CMDQ_DESC_NUM];
    struct hns_roce_pf_res_a *req_a = NULL;
    struct hns_roce_pf_res_b *req_b = NULL;
    int ret;
    int i;

    for (i = 0; i < QUERY_PF_RES_CMDQ_DESC_NUM; i++) {
        hns_roce_cmq_setup_basic_desc(&desc[i],
                                      HNS_ROCE_OPC_QUERY_PF_RES, true);

        if (i == 0) {
            desc[i].flag |= cpu_to_le16(HNS_ROCE_CMD_FLAG_NEXT);
        } else {
            desc[i].flag &= ~cpu_to_le16(HNS_ROCE_CMD_FLAG_NEXT);
        }
    }

    ret = hns_roce_cmq_send(hr_dev, desc, QUERY_PF_RES_CMDQ_DESC_NUM);
    HNS_ROCE_CMQ_SEND_RET_CHECK(ret);

    req_a = (struct hns_roce_pf_res_a *)desc[0].data;
    req_b = (struct hns_roce_pf_res_b *)desc[1].data;

    hr_dev->caps.qpc_bt_num = roce_get_field(req_a->qpc_bt_idx_num,
                                             PF_RES_DATA_1_PF_QPC_BT_NUM_M,
                                             PF_RES_DATA_1_PF_QPC_BT_NUM_S);
    hr_dev->caps.srqc_bt_num = roce_get_field(req_a->srqc_bt_idx_num,
                                              PF_RES_DATA_2_PF_SRQC_BT_NUM_M,
                                              PF_RES_DATA_2_PF_SRQC_BT_NUM_S);
    hr_dev->caps.cqc_bt_num = roce_get_field(req_a->cqc_bt_idx_num,
                                             PF_RES_DATA_3_PF_CQC_BT_NUM_M,
                                             PF_RES_DATA_3_PF_CQC_BT_NUM_S);
    hr_dev->caps.mpt_bt_num = roce_get_field(req_a->mpt_bt_idx_num,
                                             PF_RES_DATA_4_PF_MPT_BT_NUM_M,
                                             PF_RES_DATA_4_PF_MPT_BT_NUM_S);

    hr_dev->caps.sl_num = roce_get_field(req_b->qid_idx_sl_num,
                                         PF_RES_DATA_3_PF_SL_NUM_M,
                                         PF_RES_DATA_3_PF_SL_NUM_S);
    hr_dev->caps.scc_ctx_bt_num = roce_get_field(req_b->scc_ctx_bt_idx_num,
                                                 PF_RES_DATA_4_PF_SCC_CTX_BT_NUM_M,
                                                 PF_RES_DATA_4_PF_SCC_CTX_BT_NUM_S);

    return 0;
}

static int hns_roce_query_pf_timer_resource(struct hns_roce_dev *hr_dev)
{
    struct hns_roce_cmq_desc desc[QUERY_PF_TIMER_RES_CMDQ_DESC_NUM];
    struct hns_roce_pf_timer_res_a *req_a = NULL;
    int ret;
    int i;

    for (i = 0; i < QUERY_PF_TIMER_RES_CMDQ_DESC_NUM; i++) {
        hns_roce_cmq_setup_basic_desc(&desc[i],
                                      HNS_ROCE_OPC_QUERY_PF_TIMER_RES,
                                      true);

        if (i == 0) {
            desc[i].flag |= cpu_to_le16(HNS_ROCE_CMD_FLAG_NEXT);
        } else {
            desc[i].flag &= ~cpu_to_le16(HNS_ROCE_CMD_FLAG_NEXT);
        }
    }

    ret = hns_roce_cmq_send(hr_dev, desc, QUERY_PF_TIMER_RES_CMDQ_DESC_NUM);
    HNS_ROCE_CMQ_SEND_RET_CHECK(ret);

    req_a = (struct hns_roce_pf_timer_res_a *)desc[0].data;

    hr_dev->caps.qpc_timer_bt_num =
        roce_get_field(req_a->qpc_timer_bt_idx_num,
                       PF_RES_DATA_1_PF_QPC_TIMER_BT_NUM_M,
                       PF_RES_DATA_1_PF_QPC_TIMER_BT_NUM_S);
    hr_dev->caps.cqc_timer_bt_num =
        roce_get_field(req_a->cqc_timer_bt_idx_num,
                       PF_RES_DATA_2_PF_CQC_TIMER_BT_NUM_M,
                       PF_RES_DATA_2_PF_CQC_TIMER_BT_NUM_S);

    return 0;
}

static int hns_roce_set_vf_switch_param(struct hns_roce_dev *hr_dev,
    int vf_id)
{
    struct hns_roce_cmq_desc desc;
    struct hns_roce_vf_switch *swt = NULL;
    int ret;

    swt = (struct hns_roce_vf_switch *)desc.data;
    hns_roce_cmq_setup_basic_desc(&desc, HNS_SWITCH_PARAMETER_CFG, true);
    swt->rocee_sel |= cpu_to_le16(HNS_ICL_SWITCH_CMD_ROCEE_SEL);
    roce_set_field(swt->fun_id,
                   VF_SWITCH_DATA_FUN_ID_VF_ID_M,
                   VF_SWITCH_DATA_FUN_ID_VF_ID_S,
                   vf_id);
    ret = hns_roce_cmq_send(hr_dev, &desc, 1);
    HNS_ROCE_CMQ_SEND_RET_CHECK(ret);

    desc.flag =
        cpu_to_le16(HNS_ROCE_CMD_FLAG_NO_INTR | HNS_ROCE_CMD_FLAG_IN);
    desc.flag &= cpu_to_le16(~HNS_ROCE_CMD_FLAG_WR);
    roce_set_bit(swt->cfg, VF_SWITCH_DATA_CFG_ALW_LPBK_S, 1);
    roce_set_bit(swt->cfg, VF_SWITCH_DATA_CFG_ALW_LCL_LPBK_S, 1);
    roce_set_bit(swt->cfg, VF_SWITCH_DATA_CFG_ALW_DST_OVRD_S, 1);

    return hns_roce_cmq_send(hr_dev, &desc, 1);
}

static void hns_roce_set_vf_req_a_data(struct hns_roce_vf_res_a *req_a, int d)
{
    roce_set_field(req_a->vf_qpc_bt_idx_num,
                   VF_RES_A_DATA_1_VF_QPC_BT_IDX_M,
                   VF_RES_A_DATA_1_VF_QPC_BT_IDX_S, 0);
    roce_set_field(req_a->vf_qpc_bt_idx_num,
                   VF_RES_A_DATA_1_VF_QPC_BT_NUM_M,
                   VF_RES_A_DATA_1_VF_QPC_BT_NUM_S,
                   HNS_ROCE_VF_QPC_BT_NUM(d));

    roce_set_field(req_a->vf_srqc_bt_idx_num,
                   VF_RES_A_DATA_2_VF_SRQC_BT_IDX_M,
                   VF_RES_A_DATA_2_VF_SRQC_BT_IDX_S, 0);
    roce_set_field(req_a->vf_srqc_bt_idx_num,
                   VF_RES_A_DATA_2_VF_SRQC_BT_NUM_M,
                   VF_RES_A_DATA_2_VF_SRQC_BT_NUM_S,
                   HNS_ROCE_VF_SRQC_BT_NUM(d));

    roce_set_field(req_a->vf_cqc_bt_idx_num,
                   VF_RES_A_DATA_3_VF_CQC_BT_IDX_M,
                   VF_RES_A_DATA_3_VF_CQC_BT_IDX_S, 0);
    roce_set_field(req_a->vf_cqc_bt_idx_num,
                   VF_RES_A_DATA_3_VF_CQC_BT_NUM_M,
                   VF_RES_A_DATA_3_VF_CQC_BT_NUM_S,
                   HNS_ROCE_VF_CQC_BT_NUM(d));

    roce_set_field(req_a->vf_mpt_bt_idx_num,
                   VF_RES_A_DATA_4_VF_MPT_BT_IDX_M,
                   VF_RES_A_DATA_4_VF_MPT_BT_IDX_S, 0);
    roce_set_field(req_a->vf_mpt_bt_idx_num,
                   VF_RES_A_DATA_4_VF_MPT_BT_NUM_M,
                   VF_RES_A_DATA_4_VF_MPT_BT_NUM_S,
                   HNS_ROCE_VF_MPT_BT_NUM(d));

    roce_set_field(req_a->vf_eqc_bt_idx_num,
                   VF_RES_A_DATA_5_VF_EQC_IDX_M,
                   VF_RES_A_DATA_5_VF_EQC_IDX_S, 0);
    roce_set_field(req_a->vf_eqc_bt_idx_num,
                   VF_RES_A_DATA_5_VF_EQC_NUM_M,
                   VF_RES_A_DATA_5_VF_EQC_NUM_S,
                   HNS_ROCE_VF_EQC_NUM(d));
}

static void hns_roce_set_vf_req_b_data(struct hns_roce_vf_res_b *req_b, int d)
{
    roce_set_field(req_b->vf_smac_idx_num,
                   VF_RES_B_DATA_1_VF_SMAC_IDX_M,
                   VF_RES_B_DATA_1_VF_SMAC_IDX_S, 0);
    roce_set_field(req_b->vf_smac_idx_num,
                   VF_RES_B_DATA_1_VF_SMAC_NUM_M,
                   VF_RES_B_DATA_1_VF_SMAC_NUM_S,
                   HNS_ROCE_VF_SMAC_NUM(d));

    roce_set_field(req_b->vf_sgid_idx_num,
                   VF_RES_B_DATA_2_VF_SGID_IDX_M,
                   VF_RES_B_DATA_2_VF_SGID_IDX_S, 0);
    roce_set_field(req_b->vf_sgid_idx_num,
                   VF_RES_B_DATA_2_VF_SGID_NUM_M,
                   VF_RES_B_DATA_2_VF_SGID_NUM_S,
                   HNS_ROCE_V2_GID_INDEX_NUM);

    roce_set_field(req_b->vf_qid_idx_sl_num,
                   VF_RES_B_DATA_3_VF_QID_IDX_M,
                   VF_RES_B_DATA_3_VF_QID_IDX_S, 0);
    roce_set_field(req_b->vf_qid_idx_sl_num,
                   VF_RES_B_DATA_3_VF_SL_NUM_M,
                   VF_RES_B_DATA_3_VF_SL_NUM_S,
                   HNS_ROCE_VF_SL_NUM);

    roce_set_field(req_b->vf_sccc_idx_num,
                   VF_RES_B_DATA_4_VF_SCCC_BT_IDX_M,
                   VF_RES_B_DATA_4_VF_SCCC_BT_IDX_S, 0);
    roce_set_field(req_b->vf_sccc_idx_num,
                   VF_RES_B_DATA_4_VF_SCCC_BT_NUM_M,
                   VF_RES_B_DATA_4_VF_SCCC_BT_NUM_S,
                   HNS_ROCE_VF_SCCC_BT_NUM(d));
}

static int hns_roce_alloc_vf_resource(struct hns_roce_dev *hr_dev)
{
    struct hns_roce_cmq_desc desc[ALLOC_VF_RES_CMDQ_DESC_NUM];
    struct hns_roce_vf_res_a *req_a = NULL;
    struct hns_roce_vf_res_b *req_b = NULL;
    int d;
    int i;
    int ret;

    d = hns_roce_get_g_is_d();
    req_a = (struct hns_roce_vf_res_a *)desc[0].data;
    req_b = (struct hns_roce_vf_res_b *)desc[1].data;
    ret = memset_s(req_a, sizeof(*req_a), 0, sizeof(*req_a));
    HNS_ROCE_SEC_CHECK_RET_INT(hr_dev->dev, ret);
    ret = memset_s(req_b, sizeof(*req_b), 0, sizeof(*req_b));
    HNS_ROCE_SEC_CHECK_RET_INT(hr_dev->dev, ret);

    for (i = 0; i < ALLOC_VF_RES_CMDQ_DESC_NUM; i++) {
        hns_roce_cmq_setup_basic_desc(&desc[i],
                                      HNS_ROCE_OPC_ALLOC_VF_RES, false);

        if (i == 0) {
            desc[i].flag |= cpu_to_le16(HNS_ROCE_CMD_FLAG_NEXT);
            hns_roce_set_vf_req_a_data(req_a, d);
        } else {
            desc[i].flag &= ~cpu_to_le16(HNS_ROCE_CMD_FLAG_NEXT);
            hns_roce_set_vf_req_b_data(req_b, d);
        }
    }

    return hns_roce_cmq_send(hr_dev, desc, ALLOC_VF_RES_CMDQ_DESC_NUM);
}

static void hns_roce_v2_set_bt_attr(struct hns_roce_dev *hr_dev,
    struct hns_roce_cfg_bt_attr *req)
{
    u8 srqc_hop_num = hr_dev->caps.srqc_hop_num;
    u8 qpc_hop_num = hr_dev->caps.qpc_hop_num;
    u8 cqc_hop_num = hr_dev->caps.cqc_hop_num;
    u8 mpt_hop_num = hr_dev->caps.mpt_hop_num;
    u8 scc_ctx_hop_num = hr_dev->caps.scc_ctx_hop_num;

    roce_set_field(req->vf_qpc_cfg, CFG_BT_ATTR_DATA_0_VF_QPC_BA_PGSZ_M,
                   CFG_BT_ATTR_DATA_0_VF_QPC_BA_PGSZ_S, hr_dev->caps.qpc_ba_pg_sz + PG_SHIFT_OFFSET);
    roce_set_field(req->vf_qpc_cfg, CFG_BT_ATTR_DATA_0_VF_QPC_BUF_PGSZ_M,
                   CFG_BT_ATTR_DATA_0_VF_QPC_BUF_PGSZ_S, hr_dev->caps.qpc_buf_pg_sz + PG_SHIFT_OFFSET);
    roce_set_field(req->vf_qpc_cfg, CFG_BT_ATTR_DATA_0_VF_QPC_HOPNUM_M,
                   CFG_BT_ATTR_DATA_0_VF_QPC_HOPNUM_S, qpc_hop_num == HNS_ROCE_HOP_NUM_0 ? 0 : qpc_hop_num);

    roce_set_field(req->vf_srqc_cfg, CFG_BT_ATTR_DATA_1_VF_SRQC_BA_PGSZ_M,
                   CFG_BT_ATTR_DATA_1_VF_SRQC_BA_PGSZ_S, hr_dev->caps.srqc_ba_pg_sz + PG_SHIFT_OFFSET);
    roce_set_field(req->vf_srqc_cfg, CFG_BT_ATTR_DATA_1_VF_SRQC_BUF_PGSZ_M,
                   CFG_BT_ATTR_DATA_1_VF_SRQC_BUF_PGSZ_S, hr_dev->caps.srqc_buf_pg_sz + PG_SHIFT_OFFSET);
    roce_set_field(req->vf_srqc_cfg, CFG_BT_ATTR_DATA_1_VF_SRQC_HOPNUM_M,
                   CFG_BT_ATTR_DATA_1_VF_SRQC_HOPNUM_S, srqc_hop_num == HNS_ROCE_HOP_NUM_0 ? 0 : srqc_hop_num);

    roce_set_field(req->vf_cqc_cfg, CFG_BT_ATTR_DATA_2_VF_CQC_BA_PGSZ_M,
                   CFG_BT_ATTR_DATA_2_VF_CQC_BA_PGSZ_S, hr_dev->caps.cqc_ba_pg_sz + PG_SHIFT_OFFSET);
    roce_set_field(req->vf_cqc_cfg, CFG_BT_ATTR_DATA_2_VF_CQC_BUF_PGSZ_M,
                   CFG_BT_ATTR_DATA_2_VF_CQC_BUF_PGSZ_S, hr_dev->caps.cqc_buf_pg_sz + PG_SHIFT_OFFSET);
    roce_set_field(req->vf_cqc_cfg, CFG_BT_ATTR_DATA_2_VF_CQC_HOPNUM_M,
                   CFG_BT_ATTR_DATA_2_VF_CQC_HOPNUM_S, cqc_hop_num == HNS_ROCE_HOP_NUM_0 ? 0 : cqc_hop_num);

    roce_set_field(req->vf_mpt_cfg, CFG_BT_ATTR_DATA_3_VF_MPT_BA_PGSZ_M,
                   CFG_BT_ATTR_DATA_3_VF_MPT_BA_PGSZ_S, hr_dev->caps.mpt_ba_pg_sz + PG_SHIFT_OFFSET);
    roce_set_field(req->vf_mpt_cfg, CFG_BT_ATTR_DATA_3_VF_MPT_BUF_PGSZ_M,
                   CFG_BT_ATTR_DATA_3_VF_MPT_BUF_PGSZ_S, hr_dev->caps.mpt_buf_pg_sz + PG_SHIFT_OFFSET);
    roce_set_field(req->vf_mpt_cfg, CFG_BT_ATTR_DATA_3_VF_MPT_HOPNUM_M,
                   CFG_BT_ATTR_DATA_3_VF_MPT_HOPNUM_S, mpt_hop_num == HNS_ROCE_HOP_NUM_0 ? 0 : mpt_hop_num);

    roce_set_field(req->vf_scc_ctx_cfg, CFG_BT_ATTR_DATA_4_VF_SCC_CTX_BA_PGSZ_M,
                   CFG_BT_ATTR_DATA_4_VF_SCC_CTX_BA_PGSZ_S, hr_dev->caps.scc_ctx_ba_pg_sz + PG_SHIFT_OFFSET);
    roce_set_field(req->vf_scc_ctx_cfg, CFG_BT_ATTR_DATA_4_VF_SCC_CTX_BUF_PGSZ_M,
                   CFG_BT_ATTR_DATA_4_VF_SCC_CTX_BUF_PGSZ_S, hr_dev->caps.scc_ctx_buf_pg_sz + PG_SHIFT_OFFSET);
    roce_set_field(req->vf_scc_ctx_cfg, CFG_BT_ATTR_DATA_4_VF_SCC_CTX_HOPNUM_M,
                   CFG_BT_ATTR_DATA_4_VF_SCC_CTX_HOPNUM_S,
                   scc_ctx_hop_num == HNS_ROCE_HOP_NUM_0 ? 0 : scc_ctx_hop_num);
}

static int hns_roce_v2_set_bt(struct hns_roce_dev *hr_dev)
{
    struct hns_roce_cfg_bt_attr *req = NULL;
    struct hns_roce_cmq_desc desc;
    int ret;

    hns_roce_cmq_setup_basic_desc(&desc, HNS_ROCE_OPC_CFG_BT_ATTR, false);
    req = (struct hns_roce_cfg_bt_attr *)desc.data;
    ret = memset_s(req, sizeof(*req), 0, sizeof(*req));
    HNS_ROCE_SEC_CHECK_RET_INT(hr_dev->dev, ret);

    hns_roce_v2_set_bt_attr(hr_dev, req);

    return hns_roce_cmq_send(hr_dev, &desc, 1);
}

static void hns_roce_v2_set_caps_attr1(struct hns_roce_caps *caps)
{
    caps->num_qps       = HNS_ROCE_V2_MAX_QP_NUM;
    caps->max_wqes      = HNS_ROCE_V2_MAX_WQE_NUM;
    caps->num_cqs       = HNS_ROCE_V2_MAX_CQ_NUM;
    caps->num_srqs      = HNS_ROCE_V2_MAX_SRQ_NUM;
    caps->min_cqes      = HNS_ROCE_MIN_CQE_NUM;
    caps->max_cqes      = HNS_ROCE_V2_MAX_CQE_NUM;
    caps->max_srqwqes   = HNS_ROCE_V2_MAX_SRQWQE_NUM;
    caps->max_sq_sg     = HNS_ROCE_V2_MAX_SQ_SGE_NUM;
    caps->max_extend_sg = HNS_ROCE_V2_MAX_EXTEND_SGE_NUM;
    caps->max_rq_sg     = HNS_ROCE_V2_MAX_RQ_SGE_NUM;
    caps->max_sq_inline = HNS_ROCE_V2_MAX_SQ_INLINE;
    caps->max_srq_sg    = HNS_ROCE_V2_MAX_SRQ_SGE_NUM;
    caps->num_uars      = HNS_ROCE_V2_UAR_NUM;
    caps->phy_num_uars  = HNS_ROCE_V2_PHY_UAR_NUM;
    caps->num_aeq_vectors   = HNS_ROCE_V2_AEQE_VEC_NUM;
    caps->num_comp_vectors  = HNS_ROCE_V2_COMP_VEC_NUM(hns_roce_get_g_is_d());
    caps->num_other_vectors = HNS_ROCE_V2_ABNORMAL_VEC_NUM;
    caps->num_mtpts     = HNS_ROCE_V2_MAX_MTPT_NUM;
    caps->num_mtt_segs  = HNS_ROCE_V2_MAX_MTT_SEGS;
    caps->num_cqe_segs  = HNS_ROCE_V2_MAX_CQE_SEGS;
    caps->num_srqwqe_segs   = HNS_ROCE_V2_MAX_SRQWQE_SEGS;
    caps->num_idx_segs  = HNS_ROCE_V2_MAX_IDX_SEGS;
    caps->num_pds       = HNS_ROCE_V2_MAX_PD_NUM;
    caps->num_xrcds     = HNS_ROCE_V2_MAX_XRCD_NUM;
    caps->max_qp_init_rdma  = HNS_ROCE_V2_MAX_QP_INIT_RDMA;
    caps->max_qp_dest_rdma  = HNS_ROCE_V2_MAX_QP_DEST_RDMA;
    caps->max_sq_desc_sz    = HNS_ROCE_V2_MAX_SQ_DESC_SZ;
    caps->max_rq_desc_sz    = HNS_ROCE_V2_MAX_RQ_DESC_SZ;
    caps->max_srq_desc_sz   = HNS_ROCE_V2_MAX_SRQ_DESC_SZ;
    caps->qpc_entry_sz  = HNS_ROCE_V2_QPC_ENTRY_SZ;
    caps->irrl_entry_sz = HNS_ROCE_V2_IRRL_ENTRY_SZ;
    caps->trrl_entry_sz = HNS_ROCE_V2_TRRL_ENTRY_SZ;
    caps->cqc_entry_sz  = HNS_ROCE_V2_CQC_ENTRY_SZ;
    caps->srqc_entry_sz = HNS_ROCE_V2_SRQC_ENTRY_SZ;
    caps->mtpt_entry_sz = HNS_ROCE_V2_MTPT_ENTRY_SZ;
    caps->mtt_entry_sz  = HNS_ROCE_V2_MTT_ENTRY_SZ;
    caps->idx_entry_sz  = HNS_ROCE_V2_IDX_ENTRY_SZ;
    caps->cq_entry_sz   = HNS_ROCE_V2_CQE_ENTRY_SIZE;
    caps->page_size_cap = HNS_ROCE_V2_PAGE_SIZE_SUPPORTED;
    caps->reserved_lkey = 0;
    caps->reserved_pds  = 0;
    caps->reserved_xrcds = 0;
    caps->reserved_mrws = 0;
    caps->reserved_uars = 0;
    caps->reserved_cqs  = 0;
    caps->reserved_srqs = 0;
    caps->reserved_qps  = HNS_ROCE_V2_RSV_QPS;
}

static void hns_roce_v2_set_caps_attr2(struct hns_roce_caps *caps)
{
    caps->qpc_ba_pg_sz  = QPC_BA_PGSZ;
    caps->qpc_buf_pg_sz = QPC_BUF_PGSZ;
    caps->qpc_hop_num   = HNS_ROCE_CONTEXT_HOP_NUM;
    caps->srqc_ba_pg_sz = SRQC_BA_PGSZ;
    caps->srqc_buf_pg_sz    = SRQC_BUF_PGSZ;
    caps->srqc_hop_num  = HNS_ROCE_CONTEXT_HOP_NUM;
    caps->cqc_ba_pg_sz  = CQC_BA_PGSZ;
    caps->cqc_buf_pg_sz = CQC_BUF_PGSZ;
    caps->cqc_hop_num   = HNS_ROCE_CONTEXT_HOP_NUM;
    caps->mpt_ba_pg_sz  = MPT_BA_PGSIZE;
    caps->mpt_buf_pg_sz = MPT_BUF_PAGESZ;
    caps->mpt_hop_num   = HNS_ROCE_CONTEXT_HOP_NUM;
    caps->pbl_ba_pg_sz  = HNS_ROCE_MEM_PAGE_SUPPORT_64G;
    caps->pbl_buf_pg_sz = 0;
    caps->pbl_hop_num   = HNS_ROCE_PBL_HOP_NUM;
    caps->mtt_ba_pg_sz  = MTT_BA_PAGESZ;
    caps->mtt_buf_pg_sz = 0;
    caps->mtt_hop_num   = HNS_ROCE_MTT_HOP_NUM;
    caps->cqe_ba_pg_sz  = CQE_BA_PAGESZ;
    caps->cqe_buf_pg_sz = 0;
    caps->cqe_hop_num   = HNS_ROCE_CQE_HOP_NUM;
    caps->srqwqe_ba_pg_sz   = 0;
    caps->srqwqe_buf_pg_sz  = 0;
    caps->srqwqe_hop_num    = HNS_ROCE_SRQWQE_HOP_NUM;
    caps->idx_ba_pg_sz  = 0;
    caps->idx_buf_pg_sz = 0;
    caps->idx_hop_num   = HNS_ROCE_IDX_HOP_NUM;
    caps->eqe_ba_pg_sz  = EQE_BA_PAGESZ;
    caps->eqe_buf_pg_sz = EQE_BUF_PGSZ;
    caps->eqe_hop_num   = HNS_ROCE_EQE_HOP_NUM;
    caps->tsq_buf_pg_sz = 0;
    caps->chunk_sz      = HNS_ROCE_V2_TABLE_CHUNK_SIZE;

    caps->flags     = HNS_ROCE_CAP_FLAG_REREG_MR |
                      HNS_ROCE_CAP_FLAG_ROCE_V1_V2 |
                      HNS_ROCE_CAP_FLAG_RECORD_DB |
                      HNS_ROCE_CAP_FLAG_SQ_RECORD_DB;
    caps->pkey_table_len[0] = 1;
    caps->gid_table_len[0] = HNS_ROCE_V2_GID_INDEX_NUM;
    caps->ceqe_depth    = HNS_ROCE_V2_COMP_EQE_NUM;
    caps->aeqe_depth    = HNS_ROCE_V2_ASYNC_EQE_NUM;
    caps->local_ca_ack_delay = 0;
    caps->max_mtu = IB_MTU_4096;

    caps->max_srqs      = HNS_ROCE_V2_MAX_SRQ;
    caps->max_srq_wrs   = HNS_ROCE_V2_MAX_SRQ_WR;
    caps->max_srq_sges  = HNS_ROCE_V2_MAX_SRQ_SGE;
}

static void hns_roce_v2_set_caps_attr3(struct hns_roce_dev *hr_dev)
{
    struct hns_roce_caps *caps = &hr_dev->caps;

    if (hr_dev->pci_dev->revision == PCI_REVISION_ID_HIP08_B) {
        caps->flags |= (HNS_ROCE_CAP_FLAG_XRC | HNS_ROCE_CAP_FLAG_SRQ |
                        HNS_ROCE_CAP_FLAG_MW |
                        HNS_ROCE_CAP_FLAG_FRMR |
                        HNS_ROCE_CAP_FLAG_ATOMIC);
        caps->num_qpc_timer   = HNS_ROCE_V2_MAX_QPC_TIMER_NUM;
        caps->num_cqc_timer   = HNS_ROCE_V2_MAX_CQC_TIMER_NUM;
        caps->qpc_timer_entry_sz  = HNS_ROCE_V2_QPC_TIMER_ENTRY_SZ;
        caps->cqc_timer_entry_sz  = HNS_ROCE_V2_CQC_TIMER_ENTRY_SZ;
        caps->qpc_timer_ba_pg_sz  = 0;
        caps->qpc_timer_buf_pg_sz = 0;
        caps->qpc_timer_hop_num   = HNS_ROCE_HOP_NUM_0;
        caps->cqc_timer_ba_pg_sz  = 0;
        caps->cqc_timer_buf_pg_sz = 0;
        caps->cqc_timer_hop_num   = HNS_ROCE_HOP_NUM_0;

        if (g_dcqcn == 1) {
            caps->flags |= HNS_ROCE_CAP_FLAG_QP_FLOW_CTRL;
            caps->scc_ctx_entry_sz  = HNS_ROCE_V2_SCC_CTX_ENTRY_SZ;
            caps->scc_ctx_ba_pg_sz  = 0;
            caps->scc_ctx_buf_pg_sz = 0;
            caps->scc_ctx_hop_num   = HNS_ROCE_SCC_CTX_HOP_NUM;
        }
    }
}

static int hns_roce_v2_query_profile_info(struct hns_roce_dev *hr_dev)
{
    int ret;

    ret = hns_roce_cmq_query_hw_info(hr_dev);
    if (ret) {
        dev_err(hr_dev->dev, "Query hardware version fail, ret = %d.\n", ret);
        return ret;
    }

    ret = hns_roce_query_fw_ver(hr_dev);
    if (ret) {
        dev_err(hr_dev->dev, "Query firmware version fail, ret = %d.\n", ret);
        return ret;
    }

    ret = hns_roce_config_global_param(hr_dev);
    if (ret) {
        dev_err(hr_dev->dev, "Configure global param fail, ret = %d.\n", ret);
        return ret;
    }

    /* Get pf resource owned by every pf */
    ret = hns_roce_query_pf_resource(hr_dev);
    if (ret) {
        dev_err(hr_dev->dev, "Query pf resource fail, ret = %d.\n", ret);
        return ret;
    }

    hns_roce_query_func_info(hr_dev);

    if (hr_dev->pci_dev->revision == PCI_REVISION_ID_HIP08_B) {
        ret = hns_roce_query_pf_timer_resource(hr_dev);
        if (ret) {
            dev_err(hr_dev->dev, "query pf timer resource fail, ret = %d.\n", ret);
            return ret;
        }
    }
    return 0;
}

int hns_roce_v2_profile(struct hns_roce_dev *hr_dev)
{
    struct hns_roce_caps *caps = &hr_dev->caps;
    int ret;

    ret = hns_roce_v2_query_profile_info(hr_dev);
    if (ret) {
        dev_err(hr_dev->dev, "hns_roce_v2_query_profile_info fail, ret = %d.\n", ret);
        return ret;
    }

    ret = hns_roce_alloc_vf_resource(hr_dev);
    if (ret) {
        dev_err(hr_dev->dev, "Allocate vf resource fail, ret = %d.\n", ret);
        return ret;
    }

    if (hr_dev->pci_dev->revision == PCI_REVISION_ID_HIP08_B) {
        ret = hns_roce_set_vf_switch_param(hr_dev, 0);
        if (ret) {
            dev_err(hr_dev->dev, "set function switch param fail, ret = %d.\n", ret);
            return ret;
        }
    }

    hr_dev->vendor_part_id = hr_dev->pci_dev->device;
    hr_dev->sys_image_guid = be64_to_cpu(hr_dev->ib_dev.node_guid);

    hns_roce_v2_set_caps_attr1(caps);
    hns_roce_v2_set_caps_attr2(caps);
    hns_roce_v2_set_caps_attr3(hr_dev);

    ret = hns_roce_v2_set_bt(hr_dev);
    if (ret) {
        dev_err(hr_dev->dev, "Configure bt attribute fail, ret = %d.\n", ret);
    }

    return ret;
}

module_param(g_dcqcn, int, 0444);
MODULE_PARM_DESC(g_dcqcn, "default: 1");


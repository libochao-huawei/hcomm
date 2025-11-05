/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <rdma/ib_addr.h>
#include <rdma/ib_umem.h>
#include <rdma/ib_cache.h>
#include <rdma/uverbs_ioctl.h>
#include "hnae3.h"
#include "hns_roce_common.h"
#include "hns_roce_device.h"
#include "hns_roce_hem.h"
#include "hns_roce_sec.h"
#include "hns_roce_cmd.h"
#include "hns_roce_hw_v2.h"
#include "hns_roce_sm.h"

#ifdef DEFINE_HNS_LLT
#undef static
#define static

#undef inline
#define inline
#endif

static int g_pktn_ini = 0;

static int hns_roce_v2_qp_modify(struct hns_roce_dev *hr_dev,
                                 struct hns_roce_mtt *mtt,
                                 struct hns_roce_v2_qp_context *context,
                                 struct hns_roce_qp *hr_qp)
{
    int ret;
    struct hns_roce_cmd_mailbox *mailbox;
    struct hns_roce_mbox_post_params mbox_pst_params = {0};

    mailbox = hns_roce_alloc_cmd_mailbox(hr_dev);
    HNS_ROCE_ALLOC_CMD_MAILBOX_RET_CHECK(mailbox);

    /*
     * The context include qp context and qp mask context,
     * it needs to be guaranteed to be continuous
     */
    ret = memcpy_s(mailbox->buf, sizeof(*context) * CONTEXT_NUM, context, sizeof(*context) * CONTEXT_NUM);
    HNS_ROCE_SEC_CHECK_GOTO_OUT(hr_dev->dev, ret);

    mbox_pst_params.in_param = mailbox->dma;
    mbox_pst_params.out_param = 0;
    mbox_pst_params.in_modifier = hr_qp->qpn;
    mbox_pst_params.op_modifier = 0;
    mbox_pst_params.op = HNS_ROCE_CMD_MODIFY_QPC;
    ret = hns_roce_cmd_mbox(hr_dev, mbox_pst_params, HNS_ROCE_CMD_TIMEOUT_MSECS);

out:
    hns_roce_free_cmd_mailbox(hr_dev, mailbox);

    return ret;
}

static void set_access_flags(struct hns_roce_qp *hr_qp,
                             struct hns_roce_v2_qp_context *context,
                             struct hns_roce_v2_qp_context *qpc_mask,
                             const struct ib_qp_attr *attr, int attr_mask)
{
    u8 dest_rd_atomic;
    u32 access_flags;
    struct device *dev = hr_qp->ibqp.device->dma_device;

    dest_rd_atomic = ((unsigned int)attr_mask & IB_QP_MAX_DEST_RD_ATOMIC) ?
                     attr->max_dest_rd_atomic : hr_qp->resp_depth;

    access_flags = ((unsigned int)attr_mask & IB_QP_ACCESS_FLAGS) ?
                   attr->qp_access_flags : hr_qp->atomic_rd_en;

    if (!dest_rd_atomic) {
        dev_warn(dev, "dest_rd_atomic is 0, access_flag will only support IB_ACCESS_REMOTE_WRITE.\n");
        access_flags &= IB_ACCESS_REMOTE_WRITE;
    }

    roce_set_bit(context->byte_76_srqn_op_en, V2_QPC_BYTE_76_RRE_S,
                 !!(access_flags & IB_ACCESS_REMOTE_READ));
    roce_set_bit(qpc_mask->byte_76_srqn_op_en, V2_QPC_BYTE_76_RRE_S, 0);

    roce_set_bit(context->byte_76_srqn_op_en, V2_QPC_BYTE_76_RWE_S,
                 !!(access_flags & IB_ACCESS_REMOTE_WRITE));
    roce_set_bit(qpc_mask->byte_76_srqn_op_en, V2_QPC_BYTE_76_RWE_S, 0);

    roce_set_bit(context->byte_76_srqn_op_en, V2_QPC_BYTE_76_ATE_S,
                 !!(access_flags & IB_ACCESS_REMOTE_ATOMIC));
    roce_set_bit(qpc_mask->byte_76_srqn_op_en, V2_QPC_BYTE_76_ATE_S, 0);
}

static inline enum ib_qp_state to_ib_qp_st(enum hns_roce_v2_qp_state state)
{
    switch (state) {
        case HNS_ROCE_QP_ST_RST:
            return IB_QPS_RESET;
        case HNS_ROCE_QP_ST_INIT:
            return IB_QPS_INIT;
        case HNS_ROCE_QP_ST_RTR:
            return IB_QPS_RTR;
        case HNS_ROCE_QP_ST_RTS:
            return IB_QPS_RTS;
        case HNS_ROCE_QP_ST_SQ_DRAINING:
        case HNS_ROCE_QP_ST_SQD:
            return IB_QPS_SQD;
        case HNS_ROCE_QP_ST_SQER:
            return IB_QPS_SQE;
        case HNS_ROCE_QP_ST_ERR:
            return IB_QPS_ERR;
        default:
            return -1;
    }
}

static inline enum hns_roce_v2_qp_state to_hns_roce_qp_st(
        enum ib_qp_state state)
{
    switch (state) {
        case IB_QPS_RESET:
            return HNS_ROCE_QP_ST_RST;
        case IB_QPS_INIT:
            return HNS_ROCE_QP_ST_INIT;
        case IB_QPS_RTR:
            return HNS_ROCE_QP_ST_RTR;
        case IB_QPS_RTS:
            return HNS_ROCE_QP_ST_RTS;
        case IB_QPS_SQD:
            return HNS_ROCE_QP_ST_SQD;
        case IB_QPS_SQE:
            return HNS_ROCE_QP_ST_SQER;
        case IB_QPS_ERR:
            return HNS_ROCE_QP_ST_ERR;
        default:
            return HNS_ROCE_QP_NUM_ST;
        }
    }

static void hns_roce_get_cqs(
    struct ib_qp *ibqp,
    struct hns_roce_cq **send_cq,
    struct hns_roce_cq **recv_cq)
{
    switch (ibqp->qp_type) {
        case IB_QPT_XRC_TGT:
            *send_cq = to_hr_cq(to_hr_xrcd(ibqp->xrcd)->cq);
            *recv_cq = *send_cq;
            break;
        case IB_QPT_XRC_INI:
            *send_cq = to_hr_cq(ibqp->send_cq);
            *recv_cq = *send_cq;
            break;
        default:
            *send_cq = to_hr_cq(ibqp->send_cq);
            *recv_cq = to_hr_cq(ibqp->recv_cq);
            break;
    }
}

static void modify_qp_reset_to_init_set_filed_1(struct ib_qp *ibqp,
                                                struct hns_roce_v2_qp_context *context,
                                                struct hns_roce_v2_qp_context *qpc_mask)
{
    struct hns_roce_qp *hr_qp = to_hr_qp(ibqp);

    roce_set_field(context->byte_4_sqpn_tst, V2_QPC_BYTE_4_TST_M,
                   V2_QPC_BYTE_4_TST_S, to_hr_qp_type(hr_qp->ibqp.qp_type));
    roce_set_field(qpc_mask->byte_4_sqpn_tst, V2_QPC_BYTE_4_TST_M,
                   V2_QPC_BYTE_4_TST_S, 0);

    if (ibqp->qp_type == IB_QPT_GSI || ibqp->qp_type == IB_QPT_UD) {
        roce_set_field(context->byte_4_sqpn_tst,
                       V2_QPC_BYTE_4_SGE_SHIFT_M,
                       V2_QPC_BYTE_4_SGE_SHIFT_S,
                       ilog2((unsigned int)hr_qp->sge.sge_cnt));
    } else {
        roce_set_field(context->byte_4_sqpn_tst,
                       V2_QPC_BYTE_4_SGE_SHIFT_M,
                       V2_QPC_BYTE_4_SGE_SHIFT_S,
                       hr_qp->sq.max_gs > HNS_ROCE_V2_UC_RC_SGE_NUM_IN_WQE ?
                       ilog2((unsigned int)hr_qp->sge.sge_cnt) : 0);
    }

    roce_set_field(qpc_mask->byte_4_sqpn_tst, V2_QPC_BYTE_4_SGE_SHIFT_M, V2_QPC_BYTE_4_SGE_SHIFT_S, 0);

    roce_set_field(context->byte_4_sqpn_tst, V2_QPC_BYTE_4_SQPN_M, V2_QPC_BYTE_4_SQPN_S, hr_qp->qpn);
    roce_set_field(qpc_mask->byte_4_sqpn_tst, V2_QPC_BYTE_4_SQPN_M, V2_QPC_BYTE_4_SQPN_S, 0);

    roce_set_field(context->byte_16_buf_ba_pg_sz, V2_QPC_BYTE_16_PD_M,
                   V2_QPC_BYTE_16_PD_S,
                   (hr_qp->ibqp.qp_type == IB_QPT_XRC_TGT) ?
                   to_hr_pd(to_hr_xrcd(ibqp->xrcd)->pd)->pdn :
                   to_hr_pd(ibqp->pd)->pdn);
    roce_set_field(qpc_mask->byte_16_buf_ba_pg_sz, V2_QPC_BYTE_16_PD_M, V2_QPC_BYTE_16_PD_S, 0);

    roce_set_field(context->byte_20_smac_sgid_idx, V2_QPC_BYTE_20_RQWS_M,
                   V2_QPC_BYTE_20_RQWS_S,
                   (hr_qp->ibqp.qp_type == IB_QPT_XRC_INI ||
                    hr_qp->ibqp.qp_type == IB_QPT_XRC_TGT || ibqp->srq) ? 0 : ilog2(hr_qp->rq.max_gs));
    roce_set_field(qpc_mask->byte_20_smac_sgid_idx, V2_QPC_BYTE_20_RQWS_M,
                   V2_QPC_BYTE_20_RQWS_S, 0);

    roce_set_field(context->byte_20_smac_sgid_idx,
                   V2_QPC_BYTE_20_SQ_SHIFT_M, V2_QPC_BYTE_20_SQ_SHIFT_S,
                   ilog2((unsigned int)hr_qp->sq.wqe_cnt));
    roce_set_field(qpc_mask->byte_20_smac_sgid_idx,
                   V2_QPC_BYTE_20_SQ_SHIFT_M, V2_QPC_BYTE_20_SQ_SHIFT_S, 0);

    roce_set_field(context->byte_20_smac_sgid_idx,
                   V2_QPC_BYTE_20_RQ_SHIFT_M, V2_QPC_BYTE_20_RQ_SHIFT_S,
                   (hr_qp->ibqp.qp_type == IB_QPT_XRC_INI ||
                    hr_qp->ibqp.qp_type == IB_QPT_XRC_TGT || ibqp->srq) ? 0 : ilog2((unsigned int)hr_qp->rq.wqe_cnt));
    roce_set_field(qpc_mask->byte_20_smac_sgid_idx, V2_QPC_BYTE_20_RQ_SHIFT_M, V2_QPC_BYTE_20_RQ_SHIFT_S, 0);
}

static void modify_qp_reset_to_init_set_filed_2(struct ib_qp *ibqp, struct hns_roce_v2_qp_context *context,
                                                struct hns_roce_v2_qp_context *qpc_mask)
{
    struct hns_roce_qp *hr_qp = to_hr_qp(ibqp);
    struct hns_roce_dev *hr_dev = to_hr_dev(ibqp->device);

    /* No VLAN need to set 0xFFF */
    roce_set_field(context->byte_24_mtu_tc, V2_QPC_BYTE_24_VLAN_ID_M,
                   V2_QPC_BYTE_24_VLAN_ID_S, 0xfff);
    roce_set_field(qpc_mask->byte_24_mtu_tc, V2_QPC_BYTE_24_VLAN_ID_M,
                   V2_QPC_BYTE_24_VLAN_ID_S, 0);
    roce_set_bit(context->byte_76_srqn_op_en, V2_QPC_BYTE_76_RQ_VLAN_EN_S, 0);
    roce_set_bit(qpc_mask->byte_76_srqn_op_en, V2_QPC_BYTE_76_RQ_VLAN_EN_S, 0);
    roce_set_bit(context->byte_168_irrl_idx, V2_QPC_BYTE_168_SQ_VLAN_EN_S, 0);
    roce_set_bit(qpc_mask->byte_168_irrl_idx, V2_QPC_BYTE_168_SQ_VLAN_EN_S, 0);

    /*
     * Set some fields in context to zero, Because the default values
     * of all fields in context are zero, we need not set them to 0 again.
     * but we should set the relevant fields of context mask to 0.
     */
    roce_set_bit(qpc_mask->byte_56_dqpn_err, V2_QPC_BYTE_56_SQ_TX_ERR_S, 0);
    roce_set_bit(qpc_mask->byte_56_dqpn_err, V2_QPC_BYTE_56_SQ_RX_ERR_S, 0);
    roce_set_bit(qpc_mask->byte_56_dqpn_err, V2_QPC_BYTE_56_RQ_TX_ERR_S, 0);
    roce_set_bit(qpc_mask->byte_56_dqpn_err, V2_QPC_BYTE_56_RQ_RX_ERR_S, 0);

    roce_set_field(context->byte_60_qpst_tempid, V2_QPC_BYTE_60_TEMPID_M,
                   V2_QPC_BYTE_60_TEMPID_S, hr_dev->mac_id);
    roce_set_field(qpc_mask->byte_60_qpst_tempid, V2_QPC_BYTE_60_TEMPID_M,
                   V2_QPC_BYTE_60_TEMPID_S, 0);

    roce_set_field(qpc_mask->byte_60_qpst_tempid,
                   V2_QPC_BYTE_60_SCC_TOKEN_M, V2_QPC_BYTE_60_SCC_TOKEN_S,
                   0);
    roce_set_bit(qpc_mask->byte_60_qpst_tempid,
                 V2_QPC_BYTE_60_SQ_DB_DOING_S, 0);
    roce_set_bit(qpc_mask->byte_60_qpst_tempid,
                 V2_QPC_BYTE_60_RQ_DB_DOING_S, 0);
    roce_set_bit(qpc_mask->byte_28_at_fl, V2_QPC_BYTE_28_CNP_TX_FLAG_S, 0);
    roce_set_bit(qpc_mask->byte_28_at_fl, V2_QPC_BYTE_28_CE_FLAG_S, 0);

    if (to_hr_qp_type(hr_qp->ibqp.qp_type) == SERV_TYPE_XRC) {
        context->qkey_xrcd = hr_qp->xrcdn;
        qpc_mask->qkey_xrcd = 0;
    }

    if (hr_qp->rdb_en) {
        roce_set_bit(context->byte_68_rq_db,
                     V2_QPC_BYTE_68_RQ_RECORD_EN_S, 1);
        roce_set_bit(qpc_mask->byte_68_rq_db,
                     V2_QPC_BYTE_68_RQ_RECORD_EN_S, 0);
    }
}

static void modify_qp_reset_to_init_set_filed_3(struct ib_qp *ibqp,
                                                struct hns_roce_v2_qp_context *context,
                                                struct hns_roce_v2_qp_context *qpc_mask,
                                                struct hns_roce_cq *recv_cq)
{
    struct hns_roce_qp *hr_qp = to_hr_qp(ibqp);
    struct hns_roce_dev *hr_dev = to_hr_dev(ibqp->device);

    /*
     * 63 bits of the record db address are needed for hardware:
     * first, right shift 1 bit and fill the low 31 bits into the low
     * positions of rq_db_record_address field; then right shift 32 bits
     * and fill the high 32 bits into the high positions of
     * rq_db_record_address
     */
    roce_set_field(context->byte_68_rq_db,
                   V2_QPC_BYTE_68_RQ_DB_RECORD_ADDR_M,
                   V2_QPC_BYTE_68_RQ_DB_RECORD_ADDR_S,
                   ((u32)hr_qp->rdb.dma) >> 1);
    roce_set_field(qpc_mask->byte_68_rq_db,
                   V2_QPC_BYTE_68_RQ_DB_RECORD_ADDR_M,
                   V2_QPC_BYTE_68_RQ_DB_RECORD_ADDR_S, 0);
    context->rq_db_record_addr = hr_qp->rdb.dma >> LOWBITS_WIDTH;
    qpc_mask->rq_db_record_addr = 0;

    roce_set_bit(context->byte_76_srqn_op_en, V2_QPC_BYTE_76_RQIE_S,
                 (hr_dev->caps.flags & HNS_ROCE_CAP_FLAG_RQ_INLINE) ? 1 : 0);
    roce_set_bit(qpc_mask->byte_76_srqn_op_en, V2_QPC_BYTE_76_RQIE_S, 0);

    roce_set_field(context->byte_80_rnr_rx_cqn, V2_QPC_BYTE_80_RX_CQN_M,
                   V2_QPC_BYTE_80_RX_CQN_S, recv_cq->cqn);
    roce_set_field(qpc_mask->byte_80_rnr_rx_cqn, V2_QPC_BYTE_80_RX_CQN_M,
                   V2_QPC_BYTE_80_RX_CQN_S, 0);
    if (ibqp->srq) {
        roce_set_field(context->byte_76_srqn_op_en,
                       V2_QPC_BYTE_76_SRQN_M, V2_QPC_BYTE_76_SRQN_S,
                       to_hr_srq(ibqp->srq)->srqn);
        roce_set_field(qpc_mask->byte_76_srqn_op_en,
                       V2_QPC_BYTE_76_SRQN_M, V2_QPC_BYTE_76_SRQN_S, 0);
        roce_set_bit(context->byte_76_srqn_op_en,
                     V2_QPC_BYTE_76_SRQ_EN_S, 1);
        roce_set_bit(qpc_mask->byte_76_srqn_op_en,
                     V2_QPC_BYTE_76_SRQ_EN_S, 0);
    }

    roce_set_field(qpc_mask->byte_84_rq_ci_pi,
                   V2_QPC_BYTE_84_RQ_PRODUCER_IDX_M,
                   V2_QPC_BYTE_84_RQ_PRODUCER_IDX_S, 0);
    roce_set_field(qpc_mask->byte_84_rq_ci_pi,
                   V2_QPC_BYTE_84_RQ_CONSUMER_IDX_M,
                   V2_QPC_BYTE_84_RQ_CONSUMER_IDX_S, 0);
}

static void modify_qp_reset_to_init_set_filed_4(struct ib_qp *ibqp,
                                                struct hns_roce_v2_qp_context *context,
                                                struct hns_roce_v2_qp_context *qpc_mask)
{
    roce_set_field(qpc_mask->byte_92_srq_info, V2_QPC_BYTE_92_SRQ_INFO_M,
                   V2_QPC_BYTE_92_SRQ_INFO_S, 0);

    roce_set_field(qpc_mask->byte_96_rx_reqmsn, V2_QPC_BYTE_96_RX_REQ_MSN_M,
                   V2_QPC_BYTE_96_RX_REQ_MSN_S, 0);

    roce_set_field(qpc_mask->byte_104_rq_sge,
                   V2_QPC_BYTE_104_RQ_CUR_WQE_SGE_NUM_M,
                   V2_QPC_BYTE_104_RQ_CUR_WQE_SGE_NUM_S, 0);

    roce_set_bit(qpc_mask->byte_108_rx_reqepsn,
                 V2_QPC_BYTE_108_RX_REQ_PSN_ERR_S, 0);
    roce_set_field(qpc_mask->byte_108_rx_reqepsn,
                   V2_QPC_BYTE_108_RX_REQ_LAST_OPTYPE_M,
                   V2_QPC_BYTE_108_RX_REQ_LAST_OPTYPE_S, 0);
    roce_set_bit(qpc_mask->byte_108_rx_reqepsn,
                 V2_QPC_BYTE_108_RX_REQ_RNR_S, 0);

    qpc_mask->rq_rnr_timer = 0;
    qpc_mask->rx_msg_len = 0;
    qpc_mask->rx_rkey_pkt_info = 0;
    qpc_mask->rx_va = 0;

    roce_set_field(qpc_mask->byte_132_trrl, V2_QPC_BYTE_132_TRRL_HEAD_MAX_M,
                   V2_QPC_BYTE_132_TRRL_HEAD_MAX_S, 0);
    roce_set_field(qpc_mask->byte_132_trrl, V2_QPC_BYTE_132_TRRL_TAIL_MAX_M,
                   V2_QPC_BYTE_132_TRRL_TAIL_MAX_S, 0);

    roce_set_bit(qpc_mask->byte_140_raq, V2_QPC_BYTE_140_RQ_RTY_WAIT_DO_S, 0);
    roce_set_field(qpc_mask->byte_140_raq, V2_QPC_BYTE_140_RAQ_TRRL_HEAD_M,
                   V2_QPC_BYTE_140_RAQ_TRRL_HEAD_S, 0);
    roce_set_field(qpc_mask->byte_140_raq, V2_QPC_BYTE_140_RAQ_TRRL_TAIL_M,
                   V2_QPC_BYTE_140_RAQ_TRRL_TAIL_S, 0);

    roce_set_field(qpc_mask->byte_144_raq,
                   V2_QPC_BYTE_144_RAQ_RTY_INI_PSN_M,
                   V2_QPC_BYTE_144_RAQ_RTY_INI_PSN_S, 0);
    roce_set_field(qpc_mask->byte_144_raq, V2_QPC_BYTE_144_RAQ_CREDIT_M,
                   V2_QPC_BYTE_144_RAQ_CREDIT_S, 0);
    roce_set_bit(qpc_mask->byte_144_raq, V2_QPC_BYTE_144_RESP_RTY_FLG_S, 0);

    roce_set_field(qpc_mask->byte_148_raq, V2_QPC_BYTE_148_RQ_MSN_M,
                   V2_QPC_BYTE_148_RQ_MSN_S, 0);
    roce_set_field(qpc_mask->byte_148_raq, V2_QPC_BYTE_148_RAQ_SYNDROME_M,
                   V2_QPC_BYTE_148_RAQ_SYNDROME_S, 0);
}

static void modify_qp_reset_to_init_set_filed_5(struct ib_qp *ibqp,
                                                struct hns_roce_v2_qp_context *context,
                                                struct hns_roce_v2_qp_context *qpc_mask)
{
    roce_set_field(qpc_mask->byte_152_raq, V2_QPC_BYTE_152_RAQ_PSN_M,
                   V2_QPC_BYTE_152_RAQ_PSN_S, 0);
    roce_set_field(qpc_mask->byte_152_raq,
                   V2_QPC_BYTE_152_RAQ_TRRL_RTY_HEAD_M,
                   V2_QPC_BYTE_152_RAQ_TRRL_RTY_HEAD_S, 0);

    roce_set_field(qpc_mask->byte_156_raq, V2_QPC_BYTE_156_RAQ_USE_PKTN_M,
                   V2_QPC_BYTE_156_RAQ_USE_PKTN_S, 0);

    roce_set_field(qpc_mask->byte_160_sq_ci_pi,
                   V2_QPC_BYTE_160_SQ_PRODUCER_IDX_M,
                   V2_QPC_BYTE_160_SQ_PRODUCER_IDX_S, 0);
    roce_set_field(qpc_mask->byte_160_sq_ci_pi,
                   V2_QPC_BYTE_160_SQ_CONSUMER_IDX_M,
                   V2_QPC_BYTE_160_SQ_CONSUMER_IDX_S, 0);

    roce_set_bit(qpc_mask->byte_168_irrl_idx,
                 V2_QPC_BYTE_168_POLL_DB_WAIT_DO_S, 0);
    roce_set_bit(qpc_mask->byte_168_irrl_idx,
                 V2_QPC_BYTE_168_SCC_TOKEN_FORBID_SQ_DEQ_S, 0);
    roce_set_bit(qpc_mask->byte_168_irrl_idx,
                 V2_QPC_BYTE_168_WAIT_ACK_TIMEOUT_S, 0);
    roce_set_bit(qpc_mask->byte_168_irrl_idx,
                 V2_QPC_BYTE_168_MSG_RTY_LP_FLG_S, 0);
    roce_set_bit(qpc_mask->byte_168_irrl_idx,
                 V2_QPC_BYTE_168_SQ_INVLD_FLG_S, 0);
    roce_set_field(qpc_mask->byte_168_irrl_idx,
                   V2_QPC_BYTE_168_IRRL_IDX_LSB_M,
                   V2_QPC_BYTE_168_IRRL_IDX_LSB_S, 0);

    roce_set_field(context->byte_172_sq_psn, V2_QPC_BYTE_172_ACK_REQ_FREQ_M,
                   V2_QPC_BYTE_172_ACK_REQ_FREQ_S, SQ_PSN_VAL);
    roce_set_field(qpc_mask->byte_172_sq_psn,
                   V2_QPC_BYTE_172_ACK_REQ_FREQ_M,
                   V2_QPC_BYTE_172_ACK_REQ_FREQ_S, 0);

    roce_set_bit(qpc_mask->byte_172_sq_psn, V2_QPC_BYTE_172_MSG_RNR_FLG_S, 0);

    roce_set_bit(context->byte_172_sq_psn, V2_QPC_BYTE_172_FRE_S, 1);
    roce_set_bit(qpc_mask->byte_172_sq_psn, V2_QPC_BYTE_172_FRE_S, 0);
}

static void modify_qp_reset_to_init_set_filed_6(struct ib_qp *ibqp,
                                                struct hns_roce_v2_qp_context *context,
                                                struct hns_roce_v2_qp_context *qpc_mask)
{
    roce_set_field(qpc_mask->byte_176_msg_pktn,
                   V2_QPC_BYTE_176_MSG_USE_PKTN_M,
                   V2_QPC_BYTE_176_MSG_USE_PKTN_S, 0);
    roce_set_field(qpc_mask->byte_176_msg_pktn,
                   V2_QPC_BYTE_176_IRRL_HEAD_PRE_M,
                   V2_QPC_BYTE_176_IRRL_HEAD_PRE_S, 0);

    roce_set_field(qpc_mask->byte_184_irrl_idx,
                   V2_QPC_BYTE_184_IRRL_IDX_MSB_M,
                   V2_QPC_BYTE_184_IRRL_IDX_MSB_S, 0);

    qpc_mask->cur_sge_offset = 0;

    roce_set_field(qpc_mask->byte_192_ext_sge,
                   V2_QPC_BYTE_192_CUR_SGE_IDX_M,
                   V2_QPC_BYTE_192_CUR_SGE_IDX_S, 0);
    roce_set_field(qpc_mask->byte_192_ext_sge,
                   V2_QPC_BYTE_192_EXT_SGE_NUM_LEFT_M,
                   V2_QPC_BYTE_192_EXT_SGE_NUM_LEFT_S, 0);

    roce_set_field(qpc_mask->byte_196_sq_psn, V2_QPC_BYTE_196_IRRL_HEAD_M,
                   V2_QPC_BYTE_196_IRRL_HEAD_S, 0);

    roce_set_field(qpc_mask->byte_200_sq_max, V2_QPC_BYTE_200_SQ_MAX_IDX_M,
                   V2_QPC_BYTE_200_SQ_MAX_IDX_S, 0);
    roce_set_field(qpc_mask->byte_200_sq_max,
                   V2_QPC_BYTE_200_LCL_OPERATED_CNT_M,
                   V2_QPC_BYTE_200_LCL_OPERATED_CNT_S, 0);

    roce_set_bit(qpc_mask->byte_208_irrl, V2_QPC_BYTE_208_PKT_RNR_FLG_S, 0);
    roce_set_bit(qpc_mask->byte_208_irrl, V2_QPC_BYTE_208_PKT_RTY_FLG_S, 0);

    roce_set_field(qpc_mask->byte_212_lsn, V2_QPC_BYTE_212_CHECK_FLG_M,
                   V2_QPC_BYTE_212_CHECK_FLG_S, 0);

    qpc_mask->sq_timer = 0;

    roce_set_field(qpc_mask->byte_220_retry_psn_msn,
                   V2_QPC_BYTE_220_RETRY_MSG_MSN_M,
                   V2_QPC_BYTE_220_RETRY_MSG_MSN_S, 0);
    roce_set_field(qpc_mask->byte_232_irrl_sge,
                   V2_QPC_BYTE_232_IRRL_SGE_IDX_M,
                   V2_QPC_BYTE_232_IRRL_SGE_IDX_S, 0);

    roce_set_bit(qpc_mask->byte_232_irrl_sge, V2_QPC_BYTE_232_SO_LP_VLD_S, 0);
    roce_set_bit(qpc_mask->byte_232_irrl_sge, V2_QPC_BYTE_232_FENCE_LP_VLD_S, 0);
    roce_set_bit(qpc_mask->byte_232_irrl_sge, V2_QPC_BYTE_232_IRRL_LP_VLD_S, 0);
}

static void modify_qp_reset_to_init_set_filed_7(struct ib_qp *ibqp,
                                                struct hns_roce_v2_qp_context *context,
                                                struct hns_roce_v2_qp_context *qpc_mask)
{
    qpc_mask->irrl_cur_sge_offset = 0;

    roce_set_field(qpc_mask->byte_240_irrl_tail,
                   V2_QPC_BYTE_240_IRRL_TAIL_REAL_M,
                   V2_QPC_BYTE_240_IRRL_TAIL_REAL_S, 0);
    roce_set_field(qpc_mask->byte_240_irrl_tail,
                   V2_QPC_BYTE_240_IRRL_TAIL_RD_M,
                   V2_QPC_BYTE_240_IRRL_TAIL_RD_S, 0);
    roce_set_field(qpc_mask->byte_240_irrl_tail,
                   V2_QPC_BYTE_240_RX_ACK_MSN_M,
                   V2_QPC_BYTE_240_RX_ACK_MSN_S, 0);

    roce_set_bit(qpc_mask->byte_244_rnr_rxack, V2_QPC_BYTE_244_LCL_OP_FLG_S, 0);
    roce_set_bit(qpc_mask->byte_244_rnr_rxack,
                 V2_QPC_BYTE_244_IRRL_RD_FLG_S, 0);
    roce_set_field(qpc_mask->byte_248_ack_psn, V2_QPC_BYTE_248_IRRL_PSN_M,
                   V2_QPC_BYTE_248_IRRL_PSN_S, 0);
    roce_set_bit(qpc_mask->byte_248_ack_psn, V2_QPC_BYTE_248_ACK_PSN_ERR_S, 0);
    roce_set_field(qpc_mask->byte_248_ack_psn,
                   V2_QPC_BYTE_248_ACK_LAST_OPTYPE_M,
                   V2_QPC_BYTE_248_ACK_LAST_OPTYPE_S, 0);
    roce_set_bit(qpc_mask->byte_248_ack_psn, V2_QPC_BYTE_248_IRRL_PSN_VLD_S, 0);
    roce_set_bit(qpc_mask->byte_248_ack_psn,
                 V2_QPC_BYTE_248_RNR_RETRY_FLAG_S, 0);
    roce_set_bit(qpc_mask->byte_248_ack_psn, V2_QPC_BYTE_248_CQ_ERR_IND_S, 0);
}

static void modify_qp_reset_to_init(struct ib_qp *ibqp,
                                    const struct ib_qp_attr *attr,
                                    int attr_mask,
                                    struct hns_roce_v2_qp_context *context,
                                    struct hns_roce_v2_qp_context *qpc_mask)
{
    struct hns_roce_qp *hr_qp = to_hr_qp(ibqp);
    struct hns_roce_cq *send_cq = NULL;
    struct hns_roce_cq *recv_cq = NULL;

    hns_roce_get_cqs(ibqp, &send_cq, &recv_cq);
    /*
     * In v2 engine, software pass context and context mask to hardware
     * when modifying qp. If software need modify some fields in context,
     * we should set all bits of the relevant fields in context mask to
     * 0 at the same time, else set them to 0x1.
     */
    modify_qp_reset_to_init_set_filed_1(ibqp, context, qpc_mask);

    modify_qp_reset_to_init_set_filed_2(ibqp, context, qpc_mask);

    modify_qp_reset_to_init_set_filed_3(ibqp, context, qpc_mask, recv_cq);

    modify_qp_reset_to_init_set_filed_4(ibqp, context, qpc_mask);

    modify_qp_reset_to_init_set_filed_5(ibqp, context, qpc_mask);

    modify_qp_reset_to_init_set_filed_6(ibqp, context, qpc_mask);

    modify_qp_reset_to_init_set_filed_7(ibqp, context, qpc_mask);

    if (attr != NULL) {
        hr_qp->access_flags = attr->qp_access_flags;
    }

    roce_set_field(context->byte_252_err_txcqn, V2_QPC_BYTE_252_TX_CQN_M,
                   V2_QPC_BYTE_252_TX_CQN_S, send_cq->cqn);
    roce_set_field(qpc_mask->byte_252_err_txcqn, V2_QPC_BYTE_252_TX_CQN_M,
                   V2_QPC_BYTE_252_TX_CQN_S, 0);

    roce_set_field(qpc_mask->byte_252_err_txcqn, V2_QPC_BYTE_252_ERR_TYPE_M,
                   V2_QPC_BYTE_252_ERR_TYPE_S, 0);

    roce_set_field(qpc_mask->byte_256_sqflush_rqcqe, V2_QPC_BYTE_256_RQ_CQE_IDX_M,
                   V2_QPC_BYTE_256_RQ_CQE_IDX_S, 0);
    roce_set_field(qpc_mask->byte_256_sqflush_rqcqe, V2_QPC_BYTE_256_SQ_FLUSH_IDX_M,
                   V2_QPC_BYTE_256_SQ_FLUSH_IDX_S, 0);
}

static void modify_qp_init_to_init_set_srq(struct ib_qp *ibqp,
    const struct ib_qp_attr *attr, int attr_mask,
    struct hns_roce_v2_qp_context *context, struct hns_roce_v2_qp_context *qpc_mask)
{
    struct hns_roce_qp *hr_qp = to_hr_qp(ibqp);
    struct hns_roce_cq *send_cq = NULL;
    struct hns_roce_cq *recv_cq = NULL;

    hns_roce_get_cqs(ibqp, &send_cq, &recv_cq);

    if ((unsigned int)attr_mask & IB_QP_ACCESS_FLAGS) {
        if (attr == NULL) {
            return;
        }
        roce_set_bit(context->byte_76_srqn_op_en, V2_QPC_BYTE_76_RRE_S,
                     !!((unsigned int)attr->qp_access_flags & IB_ACCESS_REMOTE_READ));
        roce_set_bit(qpc_mask->byte_76_srqn_op_en, V2_QPC_BYTE_76_RRE_S, 0);

        roce_set_bit(context->byte_76_srqn_op_en, V2_QPC_BYTE_76_RWE_S,
                     !!((unsigned int)attr->qp_access_flags & IB_ACCESS_REMOTE_WRITE));
        roce_set_bit(qpc_mask->byte_76_srqn_op_en, V2_QPC_BYTE_76_RWE_S, 0);

        roce_set_bit(context->byte_76_srqn_op_en, V2_QPC_BYTE_76_ATE_S,
                     !!((unsigned int)attr->qp_access_flags & IB_ACCESS_REMOTE_ATOMIC));
        roce_set_bit(qpc_mask->byte_76_srqn_op_en, V2_QPC_BYTE_76_ATE_S, 0);
    } else {
        roce_set_bit(context->byte_76_srqn_op_en, V2_QPC_BYTE_76_RRE_S,
                     !!(hr_qp->access_flags & IB_ACCESS_REMOTE_READ));
        roce_set_bit(qpc_mask->byte_76_srqn_op_en, V2_QPC_BYTE_76_RRE_S, 0);

        roce_set_bit(context->byte_76_srqn_op_en, V2_QPC_BYTE_76_RWE_S,
                     !!(hr_qp->access_flags & IB_ACCESS_REMOTE_WRITE));
        roce_set_bit(qpc_mask->byte_76_srqn_op_en, V2_QPC_BYTE_76_RWE_S, 0);

        roce_set_bit(context->byte_76_srqn_op_en, V2_QPC_BYTE_76_ATE_S,
                     !!(hr_qp->access_flags & IB_ACCESS_REMOTE_ATOMIC));
        roce_set_bit(qpc_mask->byte_76_srqn_op_en, V2_QPC_BYTE_76_ATE_S, 0);
    }

    roce_set_field(context->byte_16_buf_ba_pg_sz, V2_QPC_BYTE_16_PD_M,
                   V2_QPC_BYTE_16_PD_S, (hr_qp->ibqp.qp_type == IB_QPT_XRC_TGT) ?
                   to_hr_pd(to_hr_xrcd(ibqp->xrcd)->pd)->pdn : to_hr_pd(ibqp->pd)->pdn);
    roce_set_field(qpc_mask->byte_16_buf_ba_pg_sz, V2_QPC_BYTE_16_PD_M, V2_QPC_BYTE_16_PD_S, 0);

    roce_set_field(context->byte_80_rnr_rx_cqn, V2_QPC_BYTE_80_RX_CQN_M,
                   V2_QPC_BYTE_80_RX_CQN_S, recv_cq->cqn);
    roce_set_field(qpc_mask->byte_80_rnr_rx_cqn, V2_QPC_BYTE_80_RX_CQN_M, V2_QPC_BYTE_80_RX_CQN_S, 0);

    roce_set_field(context->byte_252_err_txcqn, V2_QPC_BYTE_252_TX_CQN_M, V2_QPC_BYTE_252_TX_CQN_S, send_cq->cqn);
    roce_set_field(qpc_mask->byte_252_err_txcqn, V2_QPC_BYTE_252_TX_CQN_M,
                   V2_QPC_BYTE_252_TX_CQN_S, 0);

    if (ibqp->srq) {
        roce_set_bit(context->byte_76_srqn_op_en, V2_QPC_BYTE_76_SRQ_EN_S, 1);
        roce_set_bit(qpc_mask->byte_76_srqn_op_en, V2_QPC_BYTE_76_SRQ_EN_S, 0);
        roce_set_field(context->byte_76_srqn_op_en, V2_QPC_BYTE_76_SRQN_M,
            V2_QPC_BYTE_76_SRQN_S, to_hr_srq(ibqp->srq)->srqn);
        roce_set_field(qpc_mask->byte_76_srqn_op_en, V2_QPC_BYTE_76_SRQN_M, V2_QPC_BYTE_76_SRQN_S, 0);
    }
}

static void modify_qp_init_to_init(struct ib_qp *ibqp,
                                   const struct ib_qp_attr *attr, int attr_mask,
                                   struct hns_roce_v2_qp_context *context,
                                   struct hns_roce_v2_qp_context *qpc_mask)
{
    struct hns_roce_qp *hr_qp = to_hr_qp(ibqp);

    /*
     * In v2 engine, software pass context and context mask to hardware
     * when modifying qp. If software need modify some fields in context,
     * we should set all bits of the relevant fields in context mask to
     * 0 at the same time, else set them to 0x1.
     */
    roce_set_field(context->byte_4_sqpn_tst, V2_QPC_BYTE_4_TST_M,
                   V2_QPC_BYTE_4_TST_S, to_hr_qp_type(hr_qp->ibqp.qp_type));
    roce_set_field(qpc_mask->byte_4_sqpn_tst, V2_QPC_BYTE_4_TST_M,
                   V2_QPC_BYTE_4_TST_S, 0);

    if (ibqp->qp_type == IB_QPT_GSI)
        roce_set_field(context->byte_4_sqpn_tst,
                       V2_QPC_BYTE_4_SGE_SHIFT_M,
                       V2_QPC_BYTE_4_SGE_SHIFT_S,
                       ilog2((unsigned int)hr_qp->sge.sge_cnt));
    else
        roce_set_field(context->byte_4_sqpn_tst,
                       V2_QPC_BYTE_4_SGE_SHIFT_M,
                       V2_QPC_BYTE_4_SGE_SHIFT_S,
                       hr_qp->sq.max_gs >
                       HNS_ROCE_V2_UC_RC_SGE_NUM_IN_WQE ?
                       ilog2((unsigned int)hr_qp->sge.sge_cnt) : 0);

    roce_set_field(qpc_mask->byte_4_sqpn_tst, V2_QPC_BYTE_4_SGE_SHIFT_M,
                   V2_QPC_BYTE_4_SGE_SHIFT_S, 0);

    modify_qp_init_to_init_set_srq(ibqp, attr, attr_mask, context, qpc_mask);

    roce_set_field(context->byte_4_sqpn_tst, V2_QPC_BYTE_4_SQPN_M,
                   V2_QPC_BYTE_4_SQPN_S, hr_qp->qpn);
    roce_set_field(qpc_mask->byte_4_sqpn_tst, V2_QPC_BYTE_4_SQPN_M,
                   V2_QPC_BYTE_4_SQPN_S, 0);

    if ((unsigned int)attr_mask & IB_QP_DEST_QPN) {
        roce_set_field(context->byte_56_dqpn_err, V2_QPC_BYTE_56_DQPN_M,
                       V2_QPC_BYTE_56_DQPN_S, hr_qp->qpn);
        roce_set_field(qpc_mask->byte_56_dqpn_err,
                       V2_QPC_BYTE_56_DQPN_M, V2_QPC_BYTE_56_DQPN_S, 0);
    }
}

static void modify_qp_init_to_rtr_set_feild(struct ib_qp *ibqp,
                                            struct hns_roce_v2_qp_context *context,
                                            struct hns_roce_v2_qp_context *qpc_mask,
                                            dma_addr_t dma_handle)
{
    struct hns_roce_dev *hr_dev = to_hr_dev(ibqp->device);
    struct hns_roce_qp *hr_qp = to_hr_qp(ibqp);

    roce_set_field(context->byte_12_sq_hop, V2_QPC_BYTE_12_WQE_SGE_BA_M,
                   V2_QPC_BYTE_12_WQE_SGE_BA_S, dma_handle >> (LOWBITS_WIDTH + WQE_SGE_BA_OFFSET));
    roce_set_field(qpc_mask->byte_12_sq_hop, V2_QPC_BYTE_12_WQE_SGE_BA_M,
                   V2_QPC_BYTE_12_WQE_SGE_BA_S, 0);

    roce_set_field(context->byte_12_sq_hop, V2_QPC_BYTE_12_SQ_HOP_NUM_M,
                   V2_QPC_BYTE_12_SQ_HOP_NUM_S, hr_dev->caps.mtt_hop_num == HNS_ROCE_HOP_NUM_0 ?
                   0 : hr_dev->caps.mtt_hop_num);
    roce_set_field(qpc_mask->byte_12_sq_hop, V2_QPC_BYTE_12_SQ_HOP_NUM_M,
                   V2_QPC_BYTE_12_SQ_HOP_NUM_S, 0);

    roce_set_field(context->byte_20_smac_sgid_idx, V2_QPC_BYTE_20_SGE_HOP_NUM_M,
                   V2_QPC_BYTE_20_SGE_HOP_NUM_S,
                   (((ibqp->qp_type == IB_QPT_GSI) || ibqp->qp_type == IB_QPT_UD) ||
                    hr_qp->sq.max_gs > HNS_ROCE_V2_UC_RC_SGE_NUM_IN_WQE) ?
                   hr_dev->caps.mtt_hop_num : 0);
    roce_set_field(qpc_mask->byte_20_smac_sgid_idx, V2_QPC_BYTE_20_SGE_HOP_NUM_M,
                   V2_QPC_BYTE_20_SGE_HOP_NUM_S, 0);

    roce_set_field(context->byte_20_smac_sgid_idx, V2_QPC_BYTE_20_RQ_HOP_NUM_M,
                   V2_QPC_BYTE_20_RQ_HOP_NUM_S, hr_dev->caps.mtt_hop_num == HNS_ROCE_HOP_NUM_0 ?
                   0 : hr_dev->caps.mtt_hop_num);
    roce_set_field(qpc_mask->byte_20_smac_sgid_idx, V2_QPC_BYTE_20_RQ_HOP_NUM_M,
                   V2_QPC_BYTE_20_RQ_HOP_NUM_S, 0);

    roce_set_field(context->byte_16_buf_ba_pg_sz, V2_QPC_BYTE_16_WQE_SGE_BA_PG_SZ_M,
                   V2_QPC_BYTE_16_WQE_SGE_BA_PG_SZ_S, hr_dev->caps.mtt_ba_pg_sz + PG_SHIFT_OFFSET);
    roce_set_field(qpc_mask->byte_16_buf_ba_pg_sz, V2_QPC_BYTE_16_WQE_SGE_BA_PG_SZ_M,
                   V2_QPC_BYTE_16_WQE_SGE_BA_PG_SZ_S, 0);

    roce_set_field(context->byte_16_buf_ba_pg_sz, V2_QPC_BYTE_16_WQE_SGE_BUF_PG_SZ_M,
                   V2_QPC_BYTE_16_WQE_SGE_BUF_PG_SZ_S, hr_dev->caps.mtt_buf_pg_sz + PG_SHIFT_OFFSET);
    roce_set_field(qpc_mask->byte_16_buf_ba_pg_sz, V2_QPC_BYTE_16_WQE_SGE_BUF_PG_SZ_M,
                   V2_QPC_BYTE_16_WQE_SGE_BUF_PG_SZ_S, 0);
}

static int modify_qp_init_search_mtts(struct hns_roce_dev *hr_dev, struct hns_roce_qp *hr_qp,
    dma_addr_t *dma_handle_2, dma_addr_t *dma_handle_3)
{
    u64 *mtts_3 = NULL;
    u64 *mtts_2 = NULL;
    struct device *dev = hr_dev->dev;

    /* Search IRRL's mtts */
    mtts_2 = hns_roce_table_find(hr_dev, &hr_dev->qp_table.irrl_table, hr_qp->qpn, dma_handle_2);
    if (mtts_2 == NULL) {
        dev_err(dev, "qp(0x%lx) irrl_table find failed\n", hr_qp->qpn);
        return -EINVAL;
    }

    /* Search TRRL's mtts */
    mtts_3 = hns_roce_table_find(hr_dev, &hr_dev->qp_table.trrl_table, hr_qp->qpn, dma_handle_3);
    if (mtts_3 == NULL) {
        dev_err(dev, "qp(0x%lx) trrl_table find failed\n", hr_qp->qpn);
        return -EINVAL;
    }
    return 0;
}

static int modify_qp_init_to_rtr_set_page(struct ib_qp *ibqp, struct hns_roce_v2_qp_context *context,
                                          struct hns_roce_v2_qp_context *qpc_mask, u64 *mtts)
{
    struct hns_roce_dev *hr_dev = to_hr_dev(ibqp->device);
    struct hns_roce_qp *hr_qp = to_hr_qp(ibqp);
    struct device *dev = hr_dev->dev;
    dma_addr_t dma_handle_2, dma_handle_3;
    u32 page_size;
    int ret;

    ret = modify_qp_init_search_mtts(hr_dev, hr_qp, &dma_handle_2, &dma_handle_3);
    if (ret) {
        dev_err(dev, "modify_qp_init_search_mtts for qp(0x%lx) failed\n", hr_qp->qpn);
        return ret;
    }

    page_size = 1 << (hr_dev->caps.mtt_buf_pg_sz + PAGE_SHIFT);
    context->rq_cur_blk_addr = (u32)(mtts[hr_qp->rq.offset / page_size] >> PAGE_ADDR_SHIFT);
    qpc_mask->rq_cur_blk_addr = 0;

    roce_set_field(context->byte_92_srq_info, V2_QPC_BYTE_92_RQ_CUR_BLK_ADDR_M, V2_QPC_BYTE_92_RQ_CUR_BLK_ADDR_S,
        mtts[hr_qp->rq.offset / page_size] >> (LOWBITS_WIDTH + PAGE_ADDR_SHIFT));
    roce_set_field(qpc_mask->byte_92_srq_info, V2_QPC_BYTE_92_RQ_CUR_BLK_ADDR_M, V2_QPC_BYTE_92_RQ_CUR_BLK_ADDR_S, 0);

    context->rq_nxt_blk_addr = (u32)(mtts[hr_qp->rq.offset / page_size + 1] >> PAGE_ADDR_SHIFT);
    qpc_mask->rq_nxt_blk_addr = 0;

    roce_set_field(context->byte_104_rq_sge, V2_QPC_BYTE_104_RQ_NXT_BLK_ADDR_M, V2_QPC_BYTE_104_RQ_NXT_BLK_ADDR_S,
                   mtts[hr_qp->rq.offset / page_size + 1] >> (LOWBITS_WIDTH + PAGE_ADDR_SHIFT));
    roce_set_field(qpc_mask->byte_104_rq_sge, V2_QPC_BYTE_104_RQ_NXT_BLK_ADDR_M, V2_QPC_BYTE_104_RQ_NXT_BLK_ADDR_S, 0);

    roce_set_field(context->byte_132_trrl, V2_QPC_BYTE_132_TRRL_BA_M, V2_QPC_BYTE_132_TRRL_BA_S,
        dma_handle_3 >> TRRL_OFFSET);
    roce_set_field(qpc_mask->byte_132_trrl, V2_QPC_BYTE_132_TRRL_BA_M, V2_QPC_BYTE_132_TRRL_BA_S, 0);
    context->trrl_ba = (u32)(dma_handle_3 >> (TRRL_BA_OFFSET + TRRL_OFFSET));
    qpc_mask->trrl_ba = 0;
    roce_set_field(context->byte_140_raq, V2_QPC_BYTE_140_TRRL_BA_M, V2_QPC_BYTE_140_TRRL_BA_S,
                   (u32)(dma_handle_3 >> (RAQ_OFFSET + TRRL_BA_OFFSET + TRRL_OFFSET)));
    roce_set_field(qpc_mask->byte_140_raq, V2_QPC_BYTE_140_TRRL_BA_M, V2_QPC_BYTE_140_TRRL_BA_S, 0);

    context->irrl_ba = (u32)(dma_handle_2 >> IRRLBA_OFFSET);
    qpc_mask->irrl_ba = 0;
    roce_set_field(context->byte_208_irrl, V2_QPC_BYTE_208_IRRL_BA_M, V2_QPC_BYTE_208_IRRL_BA_S,
        dma_handle_2 >> (LOWBITS_WIDTH + IRRLBA_OFFSET));
    roce_set_field(qpc_mask->byte_208_irrl, V2_QPC_BYTE_208_IRRL_BA_M, V2_QPC_BYTE_208_IRRL_BA_S, 0);

    roce_set_bit(context->byte_208_irrl, V2_QPC_BYTE_208_RMT_E2E_S, 1);
    roce_set_bit(qpc_mask->byte_208_irrl, V2_QPC_BYTE_208_RMT_E2E_S, 0);

    roce_set_bit(context->byte_252_err_txcqn, V2_QPC_BYTE_252_SIG_TYPE_S, hr_qp->sq_signal_bits);
    roce_set_bit(qpc_mask->byte_252_err_txcqn, V2_QPC_BYTE_252_SIG_TYPE_S, 0);

    return 0;
}

STATIC int modify_qp_init_to_rtr_set_port(struct ib_qp *ibqp, const struct ib_qp_attr *attr, int attr_mask,
    struct hns_roce_v2_qp_context *context, struct hns_roce_v2_qp_context *qpc_mask)
{
    const struct ib_global_route *grh = rdma_ah_read_grh(&attr->ah_attr);
    struct hns_roce_dev *hr_dev = to_hr_dev(ibqp->device);
    struct hns_roce_qp *hr_qp = to_hr_qp(ibqp);
    struct device *dev = hr_dev->dev;
    u8 *dmac = (u8 *)attr->ah_attr.roce.dmac;
    u8 port_num;
    int ret;

    /* when loop_idc is 1, it should loopback */
    if (ibqp->qp_type == IB_QPT_RC) {
        roce_set_bit(context->byte_28_at_fl, V2_QPC_BYTE_28_LBI_S, hr_dev->loop_idc);
        roce_set_bit(qpc_mask->byte_28_at_fl, V2_QPC_BYTE_28_LBI_S, 0);
    }

    if ((unsigned int)attr_mask & IB_QP_DEST_QPN) {
        roce_set_field(context->byte_56_dqpn_err, V2_QPC_BYTE_56_DQPN_M, V2_QPC_BYTE_56_DQPN_S, attr->dest_qp_num);
        roce_set_field(qpc_mask->byte_56_dqpn_err, V2_QPC_BYTE_56_DQPN_M, V2_QPC_BYTE_56_DQPN_S, 0);
    }

    /* Configure GID index */
    port_num = rdma_ah_get_port_num(&attr->ah_attr);
    roce_set_field(context->byte_20_smac_sgid_idx, V2_QPC_BYTE_20_SGID_IDX_M, V2_QPC_BYTE_20_SGID_IDX_S,
                   hns_get_gid_index(hr_dev, port_num - 1, grh->sgid_index));
    roce_set_field(qpc_mask->byte_20_smac_sgid_idx, V2_QPC_BYTE_20_SGID_IDX_M, V2_QPC_BYTE_20_SGID_IDX_S, 0);
    ret = memcpy_s(&(context->dmac), sizeof(u32), dmac, sizeof(u32));
    HNS_ROCE_SEC_CHECK_RET_INT(dev, ret);

    roce_set_field(context->byte_52_udpspn_dmac, V2_QPC_BYTE_52_DMAC_M,
                   V2_QPC_BYTE_52_DMAC_S, *((u16 *)(&dmac[DMAC_INDEX_FORTH])));
    qpc_mask->dmac = 0;
    roce_set_field(qpc_mask->byte_52_udpspn_dmac, V2_QPC_BYTE_52_DMAC_M, V2_QPC_BYTE_52_DMAC_S, 0);

    /* mtu*(2^LP_PKTN_INI) should not bigger then 1 message length 64kb */
    roce_set_field(context->byte_56_dqpn_err, V2_QPC_BYTE_56_LP_PKTN_INI_M, V2_QPC_BYTE_56_LP_PKTN_INI_S, g_pktn_ini);
    roce_set_field(qpc_mask->byte_56_dqpn_err, V2_QPC_BYTE_56_LP_PKTN_INI_M, V2_QPC_BYTE_56_LP_PKTN_INI_S, 0);

    if (ibqp->qp_type == IB_QPT_GSI || ibqp->qp_type == IB_QPT_UD) {
        roce_set_field(context->byte_24_mtu_tc, V2_QPC_BYTE_24_MTU_M, V2_QPC_BYTE_24_MTU_S, IB_MTU_4096);
    } else if ((unsigned int)attr_mask & IB_QP_PATH_MTU) {
        roce_set_field(context->byte_24_mtu_tc, V2_QPC_BYTE_24_MTU_M, V2_QPC_BYTE_24_MTU_S, attr->path_mtu);
    }

    roce_set_field(qpc_mask->byte_24_mtu_tc, V2_QPC_BYTE_24_MTU_M, V2_QPC_BYTE_24_MTU_S, 0);

    roce_set_field(context->byte_84_rq_ci_pi, V2_QPC_BYTE_84_RQ_PRODUCER_IDX_M,
        V2_QPC_BYTE_84_RQ_PRODUCER_IDX_S, hr_qp->rq.head);
    roce_set_field(qpc_mask->byte_84_rq_ci_pi, V2_QPC_BYTE_84_RQ_PRODUCER_IDX_M, V2_QPC_BYTE_84_RQ_PRODUCER_IDX_S, 0);

    return 0;
}

static int modify_qp_init_to_rtr(struct ib_qp *ibqp, const struct ib_qp_attr *attr, int attr_mask,
    struct hns_roce_v2_qp_context *context, struct hns_roce_v2_qp_context *qpc_mask)
{
    struct hns_roce_dev *hr_dev = to_hr_dev(ibqp->device);
    struct hns_roce_qp *hr_qp = to_hr_qp(ibqp);
    struct device *dev = hr_dev->dev;
    dma_addr_t dma_handle;
    u64 *mtts = NULL;
    int ret;

    /* Search qp buf's mtts */
    mtts = hns_roce_table_find(hr_dev, &hr_dev->mr_table.mtt_table, hr_qp->mtt.first_seg, &dma_handle);
    if (mtts == NULL) {
        dev_err(dev, "qp(0x%lx) buf pa find failed\n", hr_qp->qpn);
        return -EINVAL;
    }

    if ((unsigned int)attr_mask & IB_QP_ALT_PATH) {
        dev_err(dev, "INIT2RTR attr_mask (0x%x) error\n", attr_mask);
        return -EINVAL;
    }

    context->wqe_sge_ba = (u32)(dma_handle >> HNS_ROCE_HW_THIRTY_THREE);
    qpc_mask->wqe_sge_ba = 0;

    /*
     * In v2 engine, software pass context and context mask to hardware
     * when modifying qp. If software need modify some fields in context,
     * we should set all bits of the relevant fields in context mask to
     * 0 at the same time, else set them to 0x1.
     */
    modify_qp_init_to_rtr_set_feild(ibqp, context, qpc_mask, dma_handle);

    ret = modify_qp_init_to_rtr_set_page(ibqp, context, qpc_mask, mtts);
    if (ret) {
        dev_err(dev, "modify_qp_init_to_rtr_set_page error[%d]\n", ret);
        return ret;
    }

    if (attr != NULL) {
        ret = modify_qp_init_to_rtr_set_port(ibqp, attr, attr_mask, context, qpc_mask);
        if (ret) {
            dev_err(dev, "modify_qp_init_to_rtr_set_port error[%d]\n", ret);
            return ret;
        }
    }

    roce_set_field(qpc_mask->byte_84_rq_ci_pi, V2_QPC_BYTE_84_RQ_CONSUMER_IDX_M,
                   V2_QPC_BYTE_84_RQ_CONSUMER_IDX_S, 0);
    roce_set_bit(qpc_mask->byte_108_rx_reqepsn, V2_QPC_BYTE_108_RX_REQ_PSN_ERR_S, 0);
    roce_set_field(qpc_mask->byte_96_rx_reqmsn, V2_QPC_BYTE_96_RX_REQ_MSN_M,
                   V2_QPC_BYTE_96_RX_REQ_MSN_S, 0);
    roce_set_field(qpc_mask->byte_108_rx_reqepsn, V2_QPC_BYTE_108_RX_REQ_LAST_OPTYPE_M,
                   V2_QPC_BYTE_108_RX_REQ_LAST_OPTYPE_S, 0);

    context->rq_rnr_timer = 0;
    qpc_mask->rq_rnr_timer = 0;

    roce_set_field(qpc_mask->byte_132_trrl, V2_QPC_BYTE_132_TRRL_HEAD_MAX_M, V2_QPC_BYTE_132_TRRL_HEAD_MAX_S, 0);
    roce_set_field(qpc_mask->byte_132_trrl, V2_QPC_BYTE_132_TRRL_TAIL_MAX_M, V2_QPC_BYTE_132_TRRL_TAIL_MAX_S, 0);

    /* rocee send 2^lp_sgen_ini segs every time */
    roce_set_field(context->byte_168_irrl_idx,
                   V2_QPC_BYTE_168_LP_SGEN_INI_M, V2_QPC_BYTE_168_LP_SGEN_INI_S, IRRL_IDX_VAL);
    roce_set_field(qpc_mask->byte_168_irrl_idx,
                   V2_QPC_BYTE_168_LP_SGEN_INI_M, V2_QPC_BYTE_168_LP_SGEN_INI_S, 0);

    return 0;
}

static void modify_qp_rtr_to_rts_set_field(struct ib_qp *ibqp,
                                           int attr_mask, struct hns_roce_v2_qp_context *context,
                                           struct hns_roce_v2_qp_context *qpc_mask, u64 *mtts)
{
    struct hns_roce_dev *hr_dev = to_hr_dev(ibqp->device);
    struct hns_roce_qp *hr_qp = to_hr_qp(ibqp);
    u32 page_size;

    context->sq_cur_blk_addr = (u32)(mtts[0] >> PAGE_ADDR_SHIFT);
    roce_set_field(context->byte_168_irrl_idx,
                   V2_QPC_BYTE_168_SQ_CUR_BLK_ADDR_M,
                   V2_QPC_BYTE_168_SQ_CUR_BLK_ADDR_S,
                   mtts[0] >> (LOWBITS_WIDTH + PAGE_ADDR_SHIFT));
    qpc_mask->sq_cur_blk_addr = 0;
    roce_set_field(qpc_mask->byte_168_irrl_idx,
                   V2_QPC_BYTE_168_SQ_CUR_BLK_ADDR_M,
                   V2_QPC_BYTE_168_SQ_CUR_BLK_ADDR_S, 0);

    page_size = 1 << (hr_dev->caps.mtt_buf_pg_sz + PAGE_SHIFT);
    context->sq_cur_sge_blk_addr = ((ibqp->qp_type == IB_QPT_GSI ||
                                     ibqp->qp_type == IB_QPT_UD) ||
                                    hr_qp->sq.max_gs >
                                    HNS_ROCE_V2_UC_RC_SGE_NUM_IN_WQE) ?
                                   ((u32)(mtts[hr_qp->sge.offset / page_size]
                                          >> PAGE_ADDR_SHIFT)) : 0;
    roce_set_field(context->byte_184_irrl_idx,
                   V2_QPC_BYTE_184_SQ_CUR_SGE_BLK_ADDR_M,
                   V2_QPC_BYTE_184_SQ_CUR_SGE_BLK_ADDR_S,
                   ((ibqp->qp_type == IB_QPT_GSI ||
                     ibqp->qp_type == IB_QPT_UD) ||
                    hr_qp->sq.max_gs > HNS_ROCE_V2_UC_RC_SGE_NUM_IN_WQE) ?
                   (mtts[hr_qp->sge.offset / page_size] >>
                    (LOWBITS_WIDTH + PAGE_ADDR_SHIFT)) : 0);
    qpc_mask->sq_cur_sge_blk_addr = 0;
    roce_set_field(qpc_mask->byte_184_irrl_idx,
                   V2_QPC_BYTE_184_SQ_CUR_SGE_BLK_ADDR_M,
                   V2_QPC_BYTE_184_SQ_CUR_SGE_BLK_ADDR_S, 0);

    context->rx_sq_cur_blk_addr = (u32)(mtts[0] >> PAGE_ADDR_SHIFT);
    roce_set_field(context->byte_232_irrl_sge,
                   V2_QPC_BYTE_232_RX_SQ_CUR_BLK_ADDR_M,
                   V2_QPC_BYTE_232_RX_SQ_CUR_BLK_ADDR_S,
                   mtts[0] >> (LOWBITS_WIDTH + PAGE_ADDR_SHIFT));
    qpc_mask->rx_sq_cur_blk_addr = 0;
    roce_set_field(qpc_mask->byte_232_irrl_sge,
                   V2_QPC_BYTE_232_RX_SQ_CUR_BLK_ADDR_M,
                   V2_QPC_BYTE_232_RX_SQ_CUR_BLK_ADDR_S, 0);
}

static void modify_qp_rtr_to_rts_set_def_value(struct hns_roce_v2_qp_context *context,
                                               struct hns_roce_v2_qp_context *qpc_mask)
{
    roce_set_field(qpc_mask->byte_232_irrl_sge,
                   V2_QPC_BYTE_232_IRRL_SGE_IDX_M,
                   V2_QPC_BYTE_232_IRRL_SGE_IDX_S, 0);

    roce_set_field(qpc_mask->byte_240_irrl_tail,
                   V2_QPC_BYTE_240_RX_ACK_MSN_M,
                   V2_QPC_BYTE_240_RX_ACK_MSN_S, 0);

    roce_set_field(qpc_mask->byte_248_ack_psn,
                   V2_QPC_BYTE_248_ACK_LAST_OPTYPE_M,
                   V2_QPC_BYTE_248_ACK_LAST_OPTYPE_S, 0);
    roce_set_bit(qpc_mask->byte_248_ack_psn,
                 V2_QPC_BYTE_248_IRRL_PSN_VLD_S, 0);
    roce_set_field(qpc_mask->byte_248_ack_psn,
                   V2_QPC_BYTE_248_IRRL_PSN_M,
                   V2_QPC_BYTE_248_IRRL_PSN_S, 0);

    roce_set_field(qpc_mask->byte_240_irrl_tail,
                   V2_QPC_BYTE_240_IRRL_TAIL_REAL_M,
                   V2_QPC_BYTE_240_IRRL_TAIL_REAL_S, 0);

    roce_set_field(qpc_mask->byte_220_retry_psn_msn,
                   V2_QPC_BYTE_220_RETRY_MSG_MSN_M,
                   V2_QPC_BYTE_220_RETRY_MSG_MSN_S, 0);

    roce_set_bit(qpc_mask->byte_248_ack_psn,
                 V2_QPC_BYTE_248_RNR_RETRY_FLAG_S, 0);

    roce_set_field(qpc_mask->byte_212_lsn, V2_QPC_BYTE_212_CHECK_FLG_M,
                   V2_QPC_BYTE_212_CHECK_FLG_S, 0);

    roce_set_field(context->byte_212_lsn, V2_QPC_BYTE_212_LSN_M,
                   V2_QPC_BYTE_212_LSN_S, 0x100);
    roce_set_field(qpc_mask->byte_212_lsn, V2_QPC_BYTE_212_LSN_M,
                   V2_QPC_BYTE_212_LSN_S, 0);

    roce_set_field(qpc_mask->byte_196_sq_psn, V2_QPC_BYTE_196_IRRL_HEAD_M,
                   V2_QPC_BYTE_196_IRRL_HEAD_S, 0);
}

static int modify_qp_rtr_to_rts(struct ib_qp *ibqp,
                                const struct ib_qp_attr *attr, int attr_mask,
                                struct hns_roce_v2_qp_context *context,
                                struct hns_roce_v2_qp_context *qpc_mask)
{
    struct hns_roce_dev *hr_dev = to_hr_dev(ibqp->device);
    struct hns_roce_qp *hr_qp = to_hr_qp(ibqp);
    struct device *dev = hr_dev->dev;
    dma_addr_t dma_handle;
    u64 *mtts;

    /* Search qp buf's mtts */
    mtts = hns_roce_table_find(hr_dev, &hr_dev->mr_table.mtt_table,
                               hr_qp->mtt.first_seg, &dma_handle);
    if (mtts == NULL) {
        dev_err(dev, "qp(0x%lx) buf pa find failed\n", hr_qp->qpn);
        return -EINVAL;
    }

    /* Not support alternate path and path migration */
    if (((unsigned int)attr_mask & IB_QP_ALT_PATH) ||
            ((unsigned int)attr_mask & IB_QP_PATH_MIG_STATE)) {
        dev_err(dev, "RTR2RTS attr_mask (0x%x)error\n", attr_mask);
        return -EINVAL;
    }

    /*
     * In v2 engine, software pass context and context mask to hardware
     * when modifying qp. If software need modify some fields in context,
     * we should set all bits of the relevant fields in context mask to
     * 0 at the same time, else set them to 0x1.
     */
    modify_qp_rtr_to_rts_set_field(ibqp, attr_mask, context, qpc_mask, mtts);

    /*
     * Set some fields in context to zero, Because the default values
     * of all fields in context are zero, we need not set them to 0 again.
     * but we should set the relevant fields of context mask to 0.
     */
    modify_qp_rtr_to_rts_set_def_value(context, qpc_mask);

    return 0;
}

static int hns_handle_roce_prtcl(struct ib_qp *ibqp, const struct ib_qp_attr *attr,
    struct hns_roce_v2_qp_context *context, struct hns_roce_v2_qp_context *qpc_mask, struct roce_prtcl_data *data)
{
    struct hns_roce_dev *hr_dev = to_hr_dev(ibqp->device);
    u16 vlan = SHORT_MASK;
    int is_roce_protocol;

    is_roce_protocol = (unsigned int)rdma_cap_eth_ah(&hr_dev->ib_dev, data->ib_port) &&
        ((unsigned int)(rdma_ah_get_ah_flags(&attr->ah_attr)) & IB_AH_GRH);
    if (is_roce_protocol) {
        vlan = rdma_vlan_dev_vlan_id(data->gid_attr->ndev);
        if (is_vlan_dev(data->gid_attr->ndev)) {
            roce_set_bit(context->byte_76_srqn_op_en, V2_QPC_BYTE_76_RQ_VLAN_EN_S, 1);
            roce_set_bit(qpc_mask->byte_76_srqn_op_en, V2_QPC_BYTE_76_RQ_VLAN_EN_S, 0);
            roce_set_bit(context->byte_168_irrl_idx, V2_QPC_BYTE_168_SQ_VLAN_EN_S, 1);
            roce_set_bit(qpc_mask->byte_168_irrl_idx, V2_QPC_BYTE_168_SQ_VLAN_EN_S, 0);
        }
    }

    roce_set_field(context->byte_24_mtu_tc, V2_QPC_BYTE_24_VLAN_ID_M, V2_QPC_BYTE_24_VLAN_ID_S, vlan);
    roce_set_field(qpc_mask->byte_24_mtu_tc, V2_QPC_BYTE_24_VLAN_ID_M, V2_QPC_BYTE_24_VLAN_ID_S, 0);
    return 0;
}

static int hns_roce_v2_set_qp_context(struct ib_qp *ibqp, const struct ib_global_route *grh,
    struct hns_roce_v2_qp_context *context, struct hns_roce_v2_qp_context *qpc_mask, struct hns_roce_attr *roce_attr)
{
    int ret;
    struct hns_roce_dev *hr_dev = to_hr_dev(ibqp->device);
    struct hns_roce_qp *hr_qp = to_hr_qp(ibqp);

    roce_set_field(context->byte_52_udpspn_dmac, V2_QPC_BYTE_52_UDPSPN_M, V2_QPC_BYTE_52_UDPSPN_S,
                   (roce_attr->gid_attr->gid_type != IB_GID_TYPE_ROCE_UDP_ENCAP) ? 0 : 0x12b7);
    roce_set_field(qpc_mask->byte_52_udpspn_dmac, V2_QPC_BYTE_52_UDPSPN_M, V2_QPC_BYTE_52_UDPSPN_S, 0);
    roce_set_field(context->byte_20_smac_sgid_idx,
                   V2_QPC_BYTE_20_SGID_IDX_M, V2_QPC_BYTE_20_SGID_IDX_S, grh->sgid_index);
    roce_set_field(qpc_mask->byte_20_smac_sgid_idx, V2_QPC_BYTE_20_SGID_IDX_M, V2_QPC_BYTE_20_SGID_IDX_S, 0);
    roce_set_field(context->byte_24_mtu_tc, V2_QPC_BYTE_24_HOP_LIMIT_M, V2_QPC_BYTE_24_HOP_LIMIT_S, grh->hop_limit);
    roce_set_field(qpc_mask->byte_24_mtu_tc, V2_QPC_BYTE_24_HOP_LIMIT_M, V2_QPC_BYTE_24_HOP_LIMIT_S, 0);

    if (hr_dev->pci_dev->revision == PCI_REVISION_ID_HIP08_B &&
        roce_attr->gid_attr->gid_type == IB_GID_TYPE_ROCE_UDP_ENCAP) {
        roce_set_field(context->byte_24_mtu_tc, V2_QPC_BYTE_24_TC_M, V2_QPC_BYTE_24_TC_S,
            grh->traffic_class >> TRAFFIC_CLASS_OFFSET);
    } else {
        roce_set_field(context->byte_24_mtu_tc, V2_QPC_BYTE_24_TC_M, V2_QPC_BYTE_24_TC_S, grh->traffic_class);
    }

    roce_set_field(qpc_mask->byte_24_mtu_tc, V2_QPC_BYTE_24_TC_M, V2_QPC_BYTE_24_TC_S, 0);
    roce_set_field(context->byte_28_at_fl, V2_QPC_BYTE_28_FL_M, V2_QPC_BYTE_28_FL_S, grh->flow_label);
    roce_set_field(qpc_mask->byte_28_at_fl, V2_QPC_BYTE_28_FL_M, V2_QPC_BYTE_28_FL_S, 0);
    ret = memcpy_s(context->dgid, sizeof(context->dgid), grh->dgid.raw, sizeof(grh->dgid.raw));
    HNS_ROCE_SEC_CHECK_RET_INT(hr_dev->dev, ret);

    ret = memset_s(qpc_mask->dgid, sizeof(qpc_mask->dgid), 0, sizeof(grh->dgid.raw));
    HNS_ROCE_SEC_CHECK_RET_INT(hr_dev->dev, ret);

    roce_set_field(context->byte_28_at_fl, V2_QPC_BYTE_28_SL_M, V2_QPC_BYTE_28_SL_S,
        rdma_ah_get_sl((&roce_attr->attr->ah_attr)));
    roce_set_field(qpc_mask->byte_28_at_fl, V2_QPC_BYTE_28_SL_M, V2_QPC_BYTE_28_SL_S, 0);
    hr_qp->sl = rdma_ah_get_sl(&(roce_attr->attr->ah_attr));

    return 0;
}

static int hns_roce_v2_set_path(struct ib_qp *ibqp, const struct ib_qp_attr *attr, int attr_mask,
    struct hns_roce_v2_qp_context *context, struct hns_roce_v2_qp_context *qpc_mask)
{
    const struct ib_global_route *grh = rdma_ah_read_grh(&attr->ah_attr);
    struct hns_roce_dev *hr_dev = to_hr_dev(ibqp->device);
    struct hns_roce_qp *hr_qp = to_hr_qp(ibqp);
    struct roce_prtcl_data data = {0};
    struct hns_roce_attr roce_attr = {0};
    u8 ib_port;
    u8 hr_port;
    int ret;

    const struct ib_gid_attr *gid_attr = attr->ah_attr.grh.sgid_attr;
    data.gid_attr = gid_attr;

    ib_port = ((unsigned int)attr_mask & IB_QP_PORT) ? attr->port_num : hr_qp->port + 1;
    hr_port = ib_port - 1;
    data.index = grh->sgid_index;
    data.ib_port = ib_port;
    ret = hns_handle_roce_prtcl(ibqp, attr, context, qpc_mask, &data);
    if (ret) {
        dev_err(hr_dev->dev, "hns_handle_roce_prtcl fail, ret:%d\n", ret);
        return ret;
    }

    if (grh->sgid_index >= hr_dev->caps.gid_table_len[hr_port]) {
        dev_err(hr_dev->dev, "sgid_index(%u) too large. max is %d\n",
                grh->sgid_index, hr_dev->caps.gid_table_len[hr_port]);
        return -EINVAL;
    }

    if (attr->ah_attr.type != RDMA_AH_ATTR_TYPE_ROCE) {
        dev_err(hr_dev->dev, "ah attr is not RDMA roce type\n");
        return -EINVAL;
    }

    roce_attr.attr = attr;
    roce_attr.gid_attr = data.gid_attr;
    ret = hns_roce_v2_set_qp_context(ibqp, grh, context, qpc_mask, &roce_attr);
    if (ret) {
        dev_err(hr_dev->dev, "hns_roce_v2_set_qp_context fail, ret:%d\n", ret);
        return ret;
    }

    return 0;
}

static int hns_roce_v2_set_abs_fields(struct ib_qp *ibqp,
                                      const struct ib_qp_attr *attr,
                                      struct hns_roce_abs_filed abs_field,
                                      struct hns_roce_v2_qp_context *context,
                                      struct hns_roce_v2_qp_context *qpc_mask)
{
    struct hns_roce_dev *hr_dev = to_hr_dev(ibqp->device);
    int ret = 0;

    if (hr_dev == NULL) {
        pr_err("hns3: ibqp device to hr_dev failed\n");
        return -EINVAL;
    }

    if (abs_field.cur_state == IB_QPS_RESET && abs_field.new_state == IB_QPS_INIT) {
        ret = memset_s(qpc_mask, sizeof(*qpc_mask), 0, sizeof(*qpc_mask));
        HNS_ROCE_SEC_CHECK_GOTO_OUT(hr_dev->dev, ret);

        modify_qp_reset_to_init(ibqp, attr, abs_field.attr_mask, context, qpc_mask);
    } else if (abs_field.cur_state == IB_QPS_INIT && abs_field.new_state == IB_QPS_INIT) {
        modify_qp_init_to_init(ibqp, attr, abs_field.attr_mask, context, qpc_mask);
    } else if (abs_field.cur_state == IB_QPS_INIT && abs_field.new_state == IB_QPS_RTR) {
        ret = modify_qp_init_to_rtr(ibqp, attr, abs_field.attr_mask, context, qpc_mask);
        if (ret) {
            dev_err(hr_dev->dev, "modify qp_init to rtr failed, ret[%d]\n", ret);
            goto out;
        }
    } else if (abs_field.cur_state == IB_QPS_RTR && abs_field.new_state == IB_QPS_RTS) {
        ret = modify_qp_rtr_to_rts(ibqp, attr, abs_field.attr_mask, context, qpc_mask);
        if (ret) {
            dev_err(hr_dev->dev, "modify qp_rtr to rts failed, ret[%d]\n", ret);
            goto out;
        }
    } else if (V2_QP_SUPPORT_STATE(abs_field.cur_state, abs_field.new_state)) {
        /* do nothing */
    } else {
        dev_err(hr_dev->dev, "Illegal state for QP(0x%x),cur state-%d, new_state-%d!\n",
                ibqp->qp_num, abs_field.cur_state, abs_field.new_state);
        ret = -EAGAIN;
        goto out;
    }

out:
    return ret;
}

static void hns_roce_v2_set_qp_retry_fields(const struct ib_qp_attr *attr, int attr_mask,
    struct hns_roce_v2_qp_context *context, struct hns_roce_v2_qp_context *qpc_mask)
{
    if ((unsigned int)attr_mask & IB_QP_RETRY_CNT) {
        roce_set_field(context->byte_212_lsn, V2_QPC_BYTE_212_RETRY_NUM_INIT_M,
                       V2_QPC_BYTE_212_RETRY_NUM_INIT_S, attr->retry_cnt);
        roce_set_field(qpc_mask->byte_212_lsn, V2_QPC_BYTE_212_RETRY_NUM_INIT_M,
                       V2_QPC_BYTE_212_RETRY_NUM_INIT_S, 0);

        roce_set_field(context->byte_212_lsn, V2_QPC_BYTE_212_RETRY_CNT_M,
                       V2_QPC_BYTE_212_RETRY_CNT_S, attr->retry_cnt);
        roce_set_field(qpc_mask->byte_212_lsn, V2_QPC_BYTE_212_RETRY_CNT_M, V2_QPC_BYTE_212_RETRY_CNT_S, 0);
    }

    if ((unsigned int)attr_mask & IB_QP_RNR_RETRY) {
        roce_set_field(context->byte_244_rnr_rxack, V2_QPC_BYTE_244_RNR_NUM_INIT_M,
                       V2_QPC_BYTE_244_RNR_NUM_INIT_S, attr->rnr_retry);
        roce_set_field(qpc_mask->byte_244_rnr_rxack, V2_QPC_BYTE_244_RNR_NUM_INIT_M,
                       V2_QPC_BYTE_244_RNR_NUM_INIT_S, 0);

        roce_set_field(context->byte_244_rnr_rxack, V2_QPC_BYTE_244_RNR_CNT_M,
                       V2_QPC_BYTE_244_RNR_CNT_S, attr->rnr_retry);
        roce_set_field(qpc_mask->byte_244_rnr_rxack, V2_QPC_BYTE_244_RNR_CNT_M,
                       V2_QPC_BYTE_244_RNR_CNT_S, 0);
    }

    return;
}

static void hns_roce_v2_set_sq_psn_fields(const struct ib_qp_attr *attr, int attr_mask,
    struct hns_roce_v2_qp_context *context, struct hns_roce_v2_qp_context *qpc_mask)
{
    /* RC&UC&UD required attr */
    if ((unsigned int)attr_mask & IB_QP_SQ_PSN) {
        roce_set_field(context->byte_172_sq_psn, V2_QPC_BYTE_172_SQ_CUR_PSN_M,
                       V2_QPC_BYTE_172_SQ_CUR_PSN_S, attr->sq_psn);
        roce_set_field(qpc_mask->byte_172_sq_psn, V2_QPC_BYTE_172_SQ_CUR_PSN_M, V2_QPC_BYTE_172_SQ_CUR_PSN_S, 0);

        roce_set_field(context->byte_196_sq_psn, V2_QPC_BYTE_196_SQ_MAX_PSN_M,
                       V2_QPC_BYTE_196_SQ_MAX_PSN_S, attr->sq_psn);
        roce_set_field(qpc_mask->byte_196_sq_psn, V2_QPC_BYTE_196_SQ_MAX_PSN_M, V2_QPC_BYTE_196_SQ_MAX_PSN_S, 0);

        roce_set_field(context->byte_220_retry_psn_msn, V2_QPC_BYTE_220_RETRY_MSG_PSN_M,
                       V2_QPC_BYTE_220_RETRY_MSG_PSN_S, attr->sq_psn);
        roce_set_field(qpc_mask->byte_220_retry_psn_msn, V2_QPC_BYTE_220_RETRY_MSG_PSN_M,
                       V2_QPC_BYTE_220_RETRY_MSG_PSN_S, 0);

        roce_set_field(context->byte_224_retry_msg, V2_QPC_BYTE_224_RETRY_MSG_PSN_M,
                       V2_QPC_BYTE_224_RETRY_MSG_PSN_S, attr->sq_psn >> V2_QPC_BYTE_220_RETRY_MSG_PSN_S);
        roce_set_field(qpc_mask->byte_224_retry_msg, V2_QPC_BYTE_224_RETRY_MSG_PSN_M,
                       V2_QPC_BYTE_224_RETRY_MSG_PSN_S, 0);

        roce_set_field(context->byte_224_retry_msg, V2_QPC_BYTE_224_RETRY_MSG_FPKT_PSN_M,
                       V2_QPC_BYTE_224_RETRY_MSG_FPKT_PSN_S, attr->sq_psn);
        roce_set_field(qpc_mask->byte_224_retry_msg, V2_QPC_BYTE_224_RETRY_MSG_FPKT_PSN_M,
                       V2_QPC_BYTE_224_RETRY_MSG_FPKT_PSN_S, 0);

        roce_set_field(context->byte_244_rnr_rxack, V2_QPC_BYTE_244_RX_ACK_EPSN_M,
                       V2_QPC_BYTE_244_RX_ACK_EPSN_S, attr->sq_psn);
        roce_set_field(qpc_mask->byte_244_rnr_rxack, V2_QPC_BYTE_244_RX_ACK_EPSN_M,
                       V2_QPC_BYTE_244_RX_ACK_EPSN_S, 0);
    }

    return;
}

static void hns_roce_v2_set_qp_limit_fields(const struct ib_qp_attr *attr, int attr_mask,
    struct hns_roce_v2_qp_context *context, struct hns_roce_v2_qp_context *qpc_mask, struct hns_roce_qp *hr_qp)
{
    if (((unsigned int)attr_mask & IB_QP_MAX_DEST_RD_ATOMIC) && attr->max_dest_rd_atomic) {
        roce_set_field(context->byte_140_raq, V2_QPC_BYTE_140_RR_MAX_M,
                       V2_QPC_BYTE_140_RR_MAX_S, fls(attr->max_dest_rd_atomic - 1));
        roce_set_field(qpc_mask->byte_140_raq, V2_QPC_BYTE_140_RR_MAX_M, V2_QPC_BYTE_140_RR_MAX_S, 0);
    }

    if (((unsigned int)attr_mask & IB_QP_MAX_QP_RD_ATOMIC) && attr->max_rd_atomic) {
        roce_set_field(context->byte_208_irrl, V2_QPC_BYTE_208_SR_MAX_M,
                       V2_QPC_BYTE_208_SR_MAX_S, fls(attr->max_rd_atomic - 1));
        roce_set_field(qpc_mask->byte_208_irrl, V2_QPC_BYTE_208_SR_MAX_M, V2_QPC_BYTE_208_SR_MAX_S, 0);
    }

    if ((unsigned int)attr_mask & (IB_QP_ACCESS_FLAGS | IB_QP_MAX_DEST_RD_ATOMIC)) {
        set_access_flags(hr_qp, context, qpc_mask, attr, attr_mask);
    }

    if ((unsigned int)attr_mask & IB_QP_MIN_RNR_TIMER) {
        roce_set_field(context->byte_80_rnr_rx_cqn, V2_QPC_BYTE_80_MIN_RNR_TIME_M,
                       V2_QPC_BYTE_80_MIN_RNR_TIME_S, attr->min_rnr_timer);
        roce_set_field(qpc_mask->byte_80_rnr_rx_cqn, V2_QPC_BYTE_80_MIN_RNR_TIME_M,
                       V2_QPC_BYTE_80_MIN_RNR_TIME_S, 0);
    }

    return;
}

STATIC int hns_roce_v2_set_opt_fields(struct ib_qp *ibqp, const struct ib_qp_attr *attr, int attr_mask,
    struct hns_roce_v2_qp_context *context, struct hns_roce_v2_qp_context *qpc_mask)
{
    struct hns_roce_dev *hr_dev = to_hr_dev(ibqp->device);
    struct hns_roce_qp *hr_qp = to_hr_qp(ibqp);
    struct device *dev = hr_dev->dev;
    int ret = 0;

    if (attr != NULL) {
        /* The AV component shall be modified for RoCEv2 */
        if (((unsigned int)attr_mask) & IB_QP_AV) {
#ifndef DEFINE_HNS_LLT
            ret = hns_roce_v2_set_path(ibqp, attr, attr_mask, context, qpc_mask);
            if (ret) {
                dev_err(hr_dev->dev, "hns_roce_v2 set path failed, ret[%d]\n", ret);
                return ret;
            }
#endif
        }

        if (((unsigned int)attr_mask) & IB_QP_TIMEOUT) {
            if (attr->timeout < TIMEOUT_MAX) {
                roce_set_field(context->byte_28_at_fl, V2_QPC_BYTE_28_AT_M, V2_QPC_BYTE_28_AT_S, attr->timeout);
                roce_set_field(qpc_mask->byte_28_at_fl, V2_QPC_BYTE_28_AT_M, V2_QPC_BYTE_28_AT_S, 0);
            } else {
                dev_warn(dev, "Local ACK timeout shall be 0 to 30.\n");
            }
        }

        hns_roce_v2_set_qp_retry_fields(attr, attr_mask, context, qpc_mask);
        hns_roce_v2_set_sq_psn_fields(attr, attr_mask, context, qpc_mask);
        hns_roce_v2_set_qp_limit_fields(attr, attr_mask, context, qpc_mask, hr_qp);
        /* RC&UC required attr */
        if (((unsigned int)attr_mask) & IB_QP_RQ_PSN) {
            roce_set_field(context->byte_108_rx_reqepsn, V2_QPC_BYTE_108_RX_REQ_EPSN_M,
                V2_QPC_BYTE_108_RX_REQ_EPSN_S, attr->rq_psn);
            roce_set_field(qpc_mask->byte_108_rx_reqepsn, V2_QPC_BYTE_108_RX_REQ_EPSN_M,
                V2_QPC_BYTE_108_RX_REQ_EPSN_S, 0);

            roce_set_field(context->byte_152_raq, V2_QPC_BYTE_152_RAQ_PSN_M,
                V2_QPC_BYTE_152_RAQ_PSN_S, attr->rq_psn - 1);
            roce_set_field(qpc_mask->byte_152_raq, V2_QPC_BYTE_152_RAQ_PSN_M, V2_QPC_BYTE_152_RAQ_PSN_S, 0);
        }
    }

    roce_set_bit(context->byte_104_rq_sge, V2_QPC_BYTE_104_WN_EN_S, 1);
    roce_set_bit(qpc_mask->byte_104_rq_sge, V2_QPC_BYTE_104_WN_EN_S, 1);

    if ((((unsigned int)attr_mask) & IB_QP_QKEY) && (attr != NULL)) {
        context->qkey_xrcd = attr->qkey;
        qpc_mask->qkey_xrcd = 0;
        hr_qp->qkey = attr->qkey;
    }

    return ret;
}

void hns_roce_v2_cq_set_ci(struct hns_roce_cq *hr_cq, u32 cons_index)
{
    *hr_cq->set_ci_db = cons_index & MASK_24BITS;
}

static int hns_roce_v2_free_cqe(struct hns_roce_cq *hr_cq, u32 qpn,
    struct hns_roce_srq *srq, int *nfreed, u32 *prod_index)
{
    struct hns_roce_v2_cqe *cqe = NULL;
    struct hns_roce_v2_cqe *dest = NULL;
    int wqe_index = 0;
    u8 owner_bit;
    int ret;

    cqe = get_cqe_v2(hr_cq, *prod_index & (u32)hr_cq->ib_cq.cqe);
    if ((roce_get_field(cqe->byte_16, V2_CQE_BYTE_16_LCL_QPN_M,
                        V2_CQE_BYTE_16_LCL_QPN_S) &
            HNS_ROCE_V2_CQE_QPN_MASK) == qpn) {
        if (srq &&
                roce_get_bit(cqe->byte_4, V2_CQE_BYTE_4_S_R_S)) {
            wqe_index = roce_get_field(cqe->byte_4,
                                       V2_CQE_BYTE_4_WQE_INDX_M,
                                       V2_CQE_BYTE_4_WQE_INDX_S);
            hns_roce_free_srq_wqe(srq, wqe_index);
        }
        ++(*nfreed);
    } else if (*nfreed) {
        dest = get_cqe_v2(hr_cq, (*prod_index + (u32)*nfreed) &
                          (u32)(hr_cq->ib_cq.cqe));
        owner_bit = roce_get_bit(dest->byte_4,
                                 V2_CQE_BYTE_4_OWNER_S);
        ret = memcpy_s(dest, sizeof(*cqe), cqe, sizeof(*cqe));
        if (ret) {
            hns_roce_free_srq_wqe(srq, wqe_index);
            return ret;
        }

        roce_set_bit(dest->byte_4, V2_CQE_BYTE_4_OWNER_S,
                     owner_bit);
    }

    return 0;
}

static void __hns_roce_v2_cq_clean(struct hns_roce_cq *hr_cq, u32 qpn,
    struct hns_roce_srq *srq)
{
    u32 prod_index;
    int nfreed = 0;
    int ret;

    for (prod_index = hr_cq->cons_index; get_sw_cqe_v2(hr_cq, prod_index); ++prod_index) {
        if (prod_index == hr_cq->cons_index + hr_cq->ib_cq.cqe) {
            break;
        }
    }

    /*
     * Now backwards through the CQ, removing CQ entries
     * that match our QP by overwriting them with next entries.
     */
    while ((int) --prod_index - (int) hr_cq->cons_index >= 0) {
        ret = hns_roce_v2_free_cqe(hr_cq, qpn, srq, &nfreed, &prod_index);
        if (ret) {
            pr_err("hns_roce_v2_free_cqe err, ret:%d\n", ret);
            return;
        }
    }

    if (nfreed) {
        hr_cq->cons_index += nfreed;
        /*
         * Make sure update of buffer contents is done before
         * updating consumer index.
         */
        wmb();
        hns_roce_v2_cq_set_ci(hr_cq, hr_cq->cons_index);
    }
}

static void hns_roce_v2_cq_clean(struct hns_roce_cq *hr_cq, u32 qpn,
    struct hns_roce_srq *srq)
{
    spin_lock_irq(&hr_cq->lock);
    __hns_roce_v2_cq_clean(hr_cq, qpn, srq);
    spin_unlock_irq(&hr_cq->lock);
}

static void hns_roce_v2_record_opt_fields(struct ib_qp *ibqp,
                                          const struct ib_qp_attr *attr,
                                          int attr_mask, enum ib_qp_state new_state)
{
    struct hns_roce_dev *hr_dev = to_hr_dev(ibqp->device);
    struct hns_roce_qp *hr_qp = to_hr_qp(ibqp);

    if (attr != NULL) {
        if (((unsigned int)attr_mask) & IB_QP_ACCESS_FLAGS) {
            hr_qp->atomic_rd_en = attr->qp_access_flags;
        }

        if (((unsigned int)attr_mask) & IB_QP_MAX_DEST_RD_ATOMIC) {
            hr_qp->resp_depth = attr->max_dest_rd_atomic;
        }
        if (((unsigned int)attr_mask) & IB_QP_PORT) {
            hr_qp->port = attr->port_num - 1;
            hr_qp->phy_port = hr_dev->iboe.phy_port[hr_qp->port];
        }
    }

    if (new_state == IB_QPS_RESET && !ibqp->uobject) {
        struct hns_roce_cq *send_cq = NULL;
        struct hns_roce_cq *recv_cq = NULL;

        hns_roce_get_cqs(ibqp, &send_cq, &recv_cq);
        hns_roce_v2_cq_clean(recv_cq, hr_qp->qpn,
                             ibqp->srq ? to_hr_srq(ibqp->srq) : NULL);
        if (send_cq != recv_cq) {
            hns_roce_v2_cq_clean(send_cq, hr_qp->qpn, NULL);
        }

        hr_qp->rq.head = 0;
        hr_qp->rq.tail = 0;
        hr_qp->sq.head = 0;
        hr_qp->sq.tail = 0;
        hr_qp->sq_next_wqe = 0;
        hr_qp->next_sge = 0;
        if (hr_qp->rq.wqe_cnt) {
            *hr_qp->rdb.db_record = 0;
        }
    }
}

static void hns_roce_v2_change_migrate_field(struct device *dev, struct ib_qp *ibqp,
                                             struct hns_roce_v2_qp_context *qpc_mask,
                                             struct hns_roce_v2_qp_context *context,
                                             enum ib_qp_state new_state)
{
    struct hns_roce_qp *hr_qp = to_hr_qp(ibqp);
    enum hns_roce_v2_qp_state qp_st;
    unsigned long sq_flags;
    unsigned long rq_flags;

    /* When QP state is err, SQ and RQ WQE should be flushed */
    if (new_state == IB_QPS_ERR) {
        spin_lock_irqsave(&hr_qp->sq.lock, sq_flags);
        roce_set_field(context->byte_160_sq_ci_pi,
                       V2_QPC_BYTE_160_SQ_PRODUCER_IDX_M,
                       V2_QPC_BYTE_160_SQ_PRODUCER_IDX_S,
                       hr_qp->sq.head);

        roce_set_field(qpc_mask->byte_160_sq_ci_pi,
                       V2_QPC_BYTE_160_SQ_PRODUCER_IDX_M,
                       V2_QPC_BYTE_160_SQ_PRODUCER_IDX_S, 0);
        hr_qp->state = IB_QPS_ERR;
        spin_unlock_irqrestore(&hr_qp->sq.lock, sq_flags);
        if (!ibqp->srq) {
            spin_lock_irqsave(&hr_qp->rq.lock, rq_flags);
            roce_set_field(context->byte_84_rq_ci_pi,
                           V2_QPC_BYTE_84_RQ_PRODUCER_IDX_M,
                           V2_QPC_BYTE_84_RQ_PRODUCER_IDX_S,
                           hr_qp->rq.head);
            roce_set_field(qpc_mask->byte_84_rq_ci_pi,
                           V2_QPC_BYTE_84_RQ_PRODUCER_IDX_M,
                           V2_QPC_BYTE_84_RQ_PRODUCER_IDX_S, 0);
            spin_unlock_irqrestore(&hr_qp->rq.lock, rq_flags);
        }
    }

    roce_set_bit(context->byte_108_rx_reqepsn, V2_QPC_BYTE_108_INV_CREDIT_S,
                 ((ibqp->srq || (to_hr_qp_type(hr_qp->ibqp.qp_type) == SERV_TYPE_XRC)) ? 1 : 0));
    roce_set_bit(qpc_mask->byte_108_rx_reqepsn, V2_QPC_BYTE_108_INV_CREDIT_S, 0);

    /* Every status migrate must change state */
    qp_st = to_hns_roce_qp_st(new_state);
    roce_set_field(context->byte_60_qpst_tempid, V2_QPC_BYTE_60_QP_ST_M,
                   V2_QPC_BYTE_60_QP_ST_S, qp_st);
    if (qp_st == HNS_ROCE_QP_NUM_ST) {
        dev_warn(dev, "Illegal hns_roce_qp_st[%u]\n", qp_st);
    }
    roce_set_field(qpc_mask->byte_60_qpst_tempid, V2_QPC_BYTE_60_QP_ST_M,
                   V2_QPC_BYTE_60_QP_ST_S, 0);
}

int hns_roce_v2_modify_qp(struct ib_qp *ibqp,
    const struct ib_qp_attr *attr,
    int attr_mask, enum ib_qp_state cur_state,
    enum ib_qp_state new_state)
{
    struct hns_roce_dev *hr_dev = NULL;
    struct hns_roce_qp *hr_qp = NULL;
    struct hns_roce_abs_filed abs_field = {0};
    struct hns_roce_v2_qp_context cmd_qpc[CMD_QPC_NUM];
    struct hns_roce_v2_qp_context *context = &cmd_qpc[0];
    struct hns_roce_v2_qp_context *qpc_mask = &cmd_qpc[1];
    struct device *dev = NULL;
    int ret;
    if (ibqp == NULL) {
        pr_err("hns3: invalid param, ibqp[%pK] \n", ibqp);
        return -EINVAL;
    }
    hr_dev = to_hr_dev(ibqp->device);
    hr_qp = to_hr_qp(ibqp);
    dev = hr_dev->dev;
    /*
     * In v2 engine, software pass context and context mask to hardware
     * when modifying qp. If software need modify some fields in context,
     * we should set all bits of the relevant fields in context mask to
     * 0 at the same time, else set them to 0x1.
     */
    ret = memset_s(context, sizeof(*context), 0, sizeof(*context));
    HNS_ROCE_SEC_CHECK_GOTO_OUT(dev, ret);
    ret = memset_s(qpc_mask, sizeof(*qpc_mask), 0xff, sizeof(*qpc_mask));
    HNS_ROCE_SEC_CHECK_GOTO_OUT(dev, ret);

    /* Configure the mandatory fields */
    abs_field.attr_mask = attr_mask;
    abs_field.cur_state = cur_state;
    abs_field.new_state = new_state;
    ret = hns_roce_v2_set_abs_fields(ibqp, attr, abs_field, context, qpc_mask);
    if (ret) {
        dev_err(dev, "set fields for modify qp(0x%x) from state %d to state %d failed, ret = %d\n",
                ibqp->qp_num, to_hns_roce_qp_st(cur_state), to_hns_roce_qp_st(new_state), ret);
        goto out;
    }

    /* Configure the optional fields */
    ret = hns_roce_v2_set_opt_fields(ibqp, attr, attr_mask, context, qpc_mask);
    if (ret) {
        dev_err(hr_dev->dev, "hns_roce_v2 set opt fields failed, ret[%d]\n", ret);
        goto out;
    }

    hns_roce_v2_change_migrate_field(dev, ibqp, qpc_mask, context, new_state);

    /* SW pass context to HW */
    ret = hns_roce_v2_qp_modify(hr_dev, &hr_qp->mtt, context, hr_qp);
    if (ret) {
        dev_err(dev, "modify qp(0x%x) from state %d to state %d failed, ret = %d\n",
                ibqp->qp_num, to_hns_roce_qp_st(cur_state), to_hns_roce_qp_st(new_state), ret);
        goto out;
    }

    hr_qp->state = new_state;

    hns_roce_v2_record_opt_fields(ibqp, attr, attr_mask, new_state);

out:
    return ret;
}

static int hns_roce_v2_query_qpc(struct hns_roce_dev *hr_dev,
                                 struct hns_roce_qp *hr_qp,
                                 struct hns_roce_v2_qp_context *hr_context)
{
    struct hns_roce_cmd_mailbox *mailbox;
    struct hns_roce_mbox_post_params mbox_pst_params = {0};
    int ret;

    mailbox = hns_roce_alloc_cmd_mailbox(hr_dev);
    HNS_ROCE_ALLOC_CMD_MAILBOX_RET_CHECK(mailbox);

    mbox_pst_params.in_param = 0;
    mbox_pst_params.out_param = mailbox->dma;
    mbox_pst_params.in_modifier = hr_qp->qpn;
    mbox_pst_params.op_modifier = 0;
    mbox_pst_params.op = HNS_ROCE_CMD_QUERY_QPC;
    ret = hns_roce_cmd_mbox(hr_dev, mbox_pst_params, HNS_ROCE_CMD_TIMEOUT_MSECS);
    if (ret) {
#ifndef DEFINE_HNS_LLT
        dev_err(hr_dev->dev, "QUERY QP cmd process error, ret[%d]\n", ret);
        goto out;
#endif
    }

    ret = memcpy_s(hr_context, sizeof(*hr_context), mailbox->buf, sizeof(*hr_context));
    HNS_ROCE_SEC_CHECK_GOTO_OUT(hr_dev->dev, ret);

out:
    hns_roce_free_cmd_mailbox(hr_dev, mailbox);
    return ret;
}

static bool hns_roce_v2_query_qp_avail_para(struct ib_qp *ibqp, struct ib_qp_attr *qp_attr,
    struct ib_qp_init_attr *qp_init_attr)
{
    int ret;

    if (ibqp == NULL || qp_attr == NULL || qp_init_attr == NULL)  {
        pr_err("hns3: null point:ibqp %pK, qp_attr %pK, qp_init_attr %pK\n",
            ibqp, qp_attr, qp_init_attr);
        return false;
    }
    ret = memset_s(qp_attr, sizeof(*qp_attr), 0, sizeof(*qp_attr));
    if (ret != 0) {
        pr_err("hns3: memset qp_attr fail, ret[%d]\n", ret);
        return false;
    }
    ret = memset_s(qp_init_attr, sizeof(*qp_init_attr), 0, sizeof(*qp_init_attr));
    if (ret != 0) {
        pr_err("hns3: memset qp_init_attr fail, ret[%d]\n", ret);
        return false;
    }

    return true;
}

static int hns_roce_v2_set_qp_state(struct hns_roce_qp *hr_qp,
    struct hns_roce_v2_qp_context *context, struct ib_qp_attr *qp_attr,
    struct device *dev, int state)
{
    int tmp_qp_state;
    int ret = 0;

    tmp_qp_state = to_ib_qp_st((enum hns_roce_v2_qp_state)state);
    if (tmp_qp_state == -1) {
        dev_err(dev, "Illegal ib_qp_state[-1]\n");
        ret = -EINVAL;
    }

    hr_qp->state = (u8)tmp_qp_state;
    qp_attr->qp_state = (enum ib_qp_state)hr_qp->state;
    qp_attr->path_mtu = (enum ib_mtu)roce_get_field(context->byte_24_mtu_tc,
                                                    V2_QPC_BYTE_24_MTU_M, V2_QPC_BYTE_24_MTU_S);
    qp_attr->path_mig_state = IB_MIG_ARMED;
    qp_attr->ah_attr.type   = RDMA_AH_ATTR_TYPE_ROCE;
    if (hr_qp->ibqp.qp_type == IB_QPT_UD) {
        qp_attr->qkey = V2_QKEY_VAL;
    }

    qp_attr->rq_psn = roce_get_field(context->byte_108_rx_reqepsn,
                                     V2_QPC_BYTE_108_RX_REQ_EPSN_M, V2_QPC_BYTE_108_RX_REQ_EPSN_S);
    qp_attr->sq_psn = (u32)roce_get_field(context->byte_172_sq_psn,
                                          V2_QPC_BYTE_172_SQ_CUR_PSN_M, V2_QPC_BYTE_172_SQ_CUR_PSN_S);
    qp_attr->dest_qp_num = (u8)roce_get_field(context->byte_56_dqpn_err,
                                              V2_QPC_BYTE_56_DQPN_M, V2_QPC_BYTE_56_DQPN_S);
    qp_attr->qp_access_flags = ((roce_get_bit(context->byte_76_srqn_op_en,
                                              V2_QPC_BYTE_76_RRE_S)) << V2_QP_RWE_S) |
                               ((roce_get_bit(context->byte_76_srqn_op_en,
                                              V2_QPC_BYTE_76_RWE_S)) << V2_QP_RRE_S) |
                               ((roce_get_bit(context->byte_76_srqn_op_en,
                                              V2_QPC_BYTE_76_ATE_S)) << V2_QP_ATE_S);

    return ret;
}

static int hns_roce_v2_set_grh(struct hns_roce_qp *hr_qp,
    struct hns_roce_v2_qp_context *context, struct ib_qp_attr *qp_attr,
    struct device *dev, int state)
{
    int ret = 0;

    if (hr_qp->ibqp.qp_type == IB_QPT_RC || hr_qp->ibqp.qp_type == IB_QPT_UC) {
        struct ib_global_route *grh = rdma_ah_retrieve_grh(&qp_attr->ah_attr);

        rdma_ah_set_sl(&qp_attr->ah_attr, roce_get_field(context->byte_28_at_fl,
                                                         V2_QPC_BYTE_28_SL_M, V2_QPC_BYTE_28_SL_S));
        grh->flow_label = roce_get_field(context->byte_28_at_fl,
                                         V2_QPC_BYTE_28_FL_M, V2_QPC_BYTE_28_FL_S);
        grh->sgid_index = roce_get_field(context->byte_20_smac_sgid_idx,
                                         V2_QPC_BYTE_20_SGID_IDX_M, V2_QPC_BYTE_20_SGID_IDX_S);
        grh->hop_limit = roce_get_field(context->byte_24_mtu_tc,
                                        V2_QPC_BYTE_24_HOP_LIMIT_M, V2_QPC_BYTE_24_HOP_LIMIT_S);
        grh->traffic_class = roce_get_field(context->byte_24_mtu_tc,
                                            V2_QPC_BYTE_24_TC_M, V2_QPC_BYTE_24_TC_S);

        ret = memcpy_s(grh->dgid.raw, sizeof(grh->dgid.raw), context->dgid, sizeof(grh->dgid.raw));
        HNS_ROCE_SEC_CHECK_GOTO_OUT(dev, ret);
    }

    qp_attr->port_num = hr_qp->port + 1;
    qp_attr->sq_draining = (state == HNS_ROCE_QP_ST_SQ_DRAINING);

    qp_attr->max_rd_atomic = 1 << roce_get_field(context->byte_208_irrl,
                                                 V2_QPC_BYTE_208_SR_MAX_M,
                                                 V2_QPC_BYTE_208_SR_MAX_S);
    if (roce_get_bit(context->byte_76_srqn_op_en, V2_QPC_BYTE_76_RRE_S) ||
            roce_get_bit(context->byte_76_srqn_op_en, V2_QPC_BYTE_76_ATE_S)) {
        qp_attr->max_dest_rd_atomic = 1 << roce_get_field(context->byte_140_raq,
                                                          V2_QPC_BYTE_140_RR_MAX_M,
                                                          V2_QPC_BYTE_140_RR_MAX_S);
    } else {
        qp_attr->max_dest_rd_atomic = 0;
    }

    qp_attr->min_rnr_timer = (u8)roce_get_field(context->byte_80_rnr_rx_cqn,
                                                V2_QPC_BYTE_80_MIN_RNR_TIME_M,
                                                V2_QPC_BYTE_80_MIN_RNR_TIME_S);
    qp_attr->timeout = (u8)roce_get_field(context->byte_28_at_fl,
                                          V2_QPC_BYTE_28_AT_M, V2_QPC_BYTE_28_AT_S);
    qp_attr->retry_cnt = roce_get_field(context->byte_212_lsn,
                                        V2_QPC_BYTE_212_RETRY_CNT_M, V2_QPC_BYTE_212_RETRY_CNT_S);
    qp_attr->rnr_retry = context->rq_rnr_timer;

out:
    return ret;
}

static void hns_roce_set_qp_attr_done(struct ib_qp *ibqp, struct ib_qp_attr *qp_attr,
    struct ib_qp_init_attr *qp_init_attr, struct hns_roce_qp *hr_qp)
{
    qp_attr->cur_qp_state = qp_attr->qp_state;
    qp_attr->cap.max_recv_wr = hr_qp->rq.wqe_cnt;
    qp_attr->cap.max_recv_sge = hr_qp->rq.max_gs;

    if (ibqp->uobject == NULL) {
        qp_attr->cap.max_send_wr = hr_qp->sq.wqe_cnt;
        qp_attr->cap.max_send_sge = hr_qp->sq.max_gs;
    } else {
        qp_attr->cap.max_send_wr = 0;
        qp_attr->cap.max_send_sge = 0;
    }

    qp_init_attr->cap = qp_attr->cap;
}

static int hns_roce_v2_set_qp_grh(struct hns_roce_qp *hr_qp, struct ib_qp_attr *qp_attr,
    struct device *dev, struct hns_roce_v2_qp_context *context, struct ib_qp *ibqp)
{
    int ret, state;

    state = roce_get_field(context->byte_60_qpst_tempid, V2_QPC_BYTE_60_QP_ST_M, V2_QPC_BYTE_60_QP_ST_S);

    ret = hns_roce_v2_set_qp_state(hr_qp, context, qp_attr, dev, state);
    if (ret) {
        dev_err(dev, "set qpc(0x%x) state error, ret = %d\n", ibqp->qp_num, ret);
        ret = -EINVAL;
        return ret;
    }

    ret = hns_roce_v2_set_grh(hr_qp, context, qp_attr, dev, state);
    if (ret) {
        dev_err(dev, "set grh error, ret = %d\n", ret);
        ret = -EINVAL;
        return ret;
    }

    return 0;
}

int hns_roce_v2_query_qp(struct ib_qp *ibqp, struct ib_qp_attr *qp_attr,
    int qp_attr_mask, struct ib_qp_init_attr *qp_init_attr)
{
    struct hns_roce_dev *hr_dev = NULL;
    struct hns_roce_qp *hr_qp = NULL;
    struct hns_roce_v2_qp_context *context = NULL;
    struct device *dev = NULL;
    int ret;

    if (hns_roce_v2_query_qp_avail_para(ibqp, qp_attr, qp_init_attr) == false) {
        pr_err("hns3: query qp fail, invalid para\n");
        return -EINVAL;
    }
    hr_dev = to_hr_dev(ibqp->device);
    hr_qp = to_hr_qp(ibqp);
    dev = hr_dev->dev;

    context = kzalloc(sizeof(*context), GFP_KERNEL);
    if (context == NULL) {
        dev_err(dev, "alloc hns_roce_v2_qp context failed\n");
        return -ENOMEM;
    }
    mutex_lock(&hr_qp->mutex);

    if (hr_qp->state == IB_QPS_RESET) {
        qp_attr->qp_state = IB_QPS_RESET;
        ret = 0;
        goto done;
    }

    ret = hns_roce_v2_query_qpc(hr_dev, hr_qp, context);
    if (ret) {
        dev_err(dev, "query qpc(0x%x) error, ret = %d\n", ibqp->qp_num, ret);
        ret = -EINVAL;
        goto out;
    }

    ret = hns_roce_v2_set_qp_grh(hr_qp, qp_attr, dev, context, ibqp);
    if (ret) {
        dev_err(dev, "set qp state and grh error, ret = %d\n", ret);
        goto out;
    }

done:
    hns_roce_set_qp_attr_done(ibqp, qp_attr, qp_init_attr, hr_qp);

out:
    mutex_unlock(&hr_qp->mutex);

    kfree(context);
    context = NULL;
    return ret;
}

static void hns_roce_v2_del_qp_list(struct hns_roce_qp *hr_qp)
{
    struct hns_roce_cq *send_cq = NULL;
    struct hns_roce_cq *recv_cq = NULL;

    hns_roce_get_cqs(&hr_qp->ibqp, &send_cq, &recv_cq);
    list_del(&hr_qp->qps_list);
    if (send_cq != NULL) {
        list_del(&hr_qp->scq_list);
    }

    if (recv_cq != NULL) {
        list_del(&hr_qp->rcq_list);
    }
}

static void hns_roce_v2_free_qp(struct hns_roce_dev *hr_dev,
    struct hns_roce_qp *hr_qp, struct ib_udata *udata)
{
    struct hns_roce_ucontext *uctx;
    // gdr
    if (hr_qp->gdr_enable == HNS_ROCE_GDR_QP_MODE) {
        hns_roce_free_shared_sq(hr_dev, hr_qp->share_sq.index);
    }

    if (udata) {
        uctx = rdma_udata_to_drv_context(udata, struct hns_roce_ucontext, ibucontext);

        if (hr_qp->sq.wqe_cnt && (hr_qp->sdb_en == 1))
            hns_roce_db_unmap_user(uctx, &hr_qp->sdb);

        if (hr_qp->rq.wqe_cnt && (hr_qp->rdb_en == 1))
            hns_roce_db_unmap_user(uctx, &hr_qp->rdb);
        ib_umem_release(hr_qp->umem);
    } else {
        kfree(hr_qp->sq.wrid);
        hr_qp->sq.wrid = NULL;
        kfree(hr_qp->rq.wrid);
        hr_qp->rq.wrid = NULL;
        hns_roce_buf_free(hr_dev, hr_qp->buff_size, &hr_qp->hr_buf);
        if (hr_qp->rq.wqe_cnt) {
            hns_roce_free_db(hr_dev, &hr_qp->rdb);
        }
    }

    if ((hr_dev->caps.flags & HNS_ROCE_CAP_FLAG_RQ_INLINE) && hr_qp->rq.wqe_cnt) {
        kfree(hr_qp->rq_inl_buf.wqe_list[0].sg_list);
        hr_qp->rq_inl_buf.wqe_list[0].sg_list = NULL;
        kfree(hr_qp->rq_inl_buf.wqe_list);
        hr_qp->rq_inl_buf.wqe_list = NULL;
    }
    return;
}

static int hns_roce_v2_destroy_qp_common(struct hns_roce_dev *hr_dev,
    struct hns_roce_qp *hr_qp, struct ib_udata *udata)
{
    struct hns_roce_cq *send_cq = NULL;
    struct hns_roce_cq *recv_cq = NULL;
    struct device *dev = hr_dev->dev;
    unsigned long flags;
    int ret = 0;

    if (hr_qp->ibqp.qp_type == IB_QPT_RC && hr_qp->state != IB_QPS_RESET) {
        /* Modify qp to reset before destroying qp */
        ret = hns_roce_v2_modify_qp(&hr_qp->ibqp, NULL, 0, hr_qp->state, IB_QPS_RESET);
        if (ret) {
            dev_err(dev, "modify QP %06lx to Reset failed, ret = %d.\n", hr_qp->qpn, ret);
        }
    }

    hns_roce_get_cqs(&hr_qp->ibqp, &send_cq, &recv_cq);

    spin_lock_irqsave(&hr_dev->reset_lock, flags);
    hns_roce_lock_cqs(send_cq, recv_cq);
    hns_roce_v2_del_qp_list(hr_qp);

    if (udata == NULL) {
        __hns_roce_v2_cq_clean(recv_cq, hr_qp->qpn, hr_qp->ibqp.srq ? to_hr_srq(hr_qp->ibqp.srq) : NULL);
        if (send_cq != recv_cq) {
            __hns_roce_v2_cq_clean(send_cq, hr_qp->qpn, NULL);
        }
    }

    hns_roce_qp_remove(hr_dev, hr_qp);

    hns_roce_unlock_cqs(send_cq, recv_cq);
    spin_unlock_irqrestore(&hr_dev->reset_lock, flags);

    hns_roce_qp_free(hr_dev, hr_qp);

    /* Not special_QP, free their QPN */
    if ((hr_qp->ibqp.qp_type == IB_QPT_RC) || (hr_qp->ibqp.qp_type == IB_QPT_UC) ||
        (hr_qp->ibqp.qp_type == IB_QPT_UD)) {
        hns_roce_release_range_qp(hr_dev, hr_qp->qpn, 1);
    }

    if (hr_qp->gdr_enable == HNS_ROCE_GDR_QP_MODE) {
        mutex_lock(&hr_dev->sm_mutex);
        hr_dev->qp_cnt--;
        mutex_unlock(&hr_dev->sm_mutex);
    }

    hns_roce_mtt_cleanup(hr_dev, &hr_qp->mtt);
    hns_roce_v2_free_qp(hr_dev, hr_qp, udata);
    mutex_destroy(&hr_qp->mutex);
    return ret;
}

int hns_roce_v2_destroy_qp(struct ib_qp *ibqp, struct ib_udata *udata)
{
    struct hns_roce_dev *hr_dev = NULL;
    struct hns_roce_qp *hr_qp = NULL;
    struct hns_roce_sqp *hr_sqp = NULL;
    int ret;

    if (ibqp == NULL) {
        pr_err("hns3: destroy_qp param err, ibqp is NULL\n");
        return -EINVAL;
    }
    hr_dev = to_hr_dev(ibqp->device);
    hr_qp = to_hr_qp(ibqp);

    hns_roce_inc_rdma_hw_stats(ibqp->device, HW_STATS_QP_DEALLOC);

    ret = hns_roce_v2_destroy_qp_common(hr_dev, hr_qp, udata);
    if (ret) {
        dev_err(hr_dev->dev, "Destroy qp 0x%06lx failed(%d)\n", hr_qp->qpn, ret);
    }

    if (hr_qp->ibqp.qp_type == IB_QPT_GSI) {
        hr_sqp = hr_to_hr_sqp(hr_qp);
        kfree(hr_sqp);
        hr_sqp = NULL;
    } else {
        kfree(hr_qp);
        hr_qp = NULL;
    }

    return 0;
}

int hns_roce_v2_qp_flow_control_init(struct hns_roce_dev *hr_dev,
    struct hns_roce_qp *hr_qp)
{
    struct hns_roce_scc_ctx_clr *scc_cxt_clr = NULL;
    struct hns_roce_scc_ctx_clr_done *resp = NULL;
    struct hns_roce_scc_ctx_clr_done *rst = NULL;
    struct hns_roce_cmq_desc desc;
    int ret;
    int i;
    if (hr_dev == NULL || hr_qp == NULL) {
        pr_err("hns3: invalid param, hr_dev[%pK], hr_qp[%pK] \n", hr_dev, hr_qp);
        return -EINVAL;
    }
    /* set scc ctx clear done flag */
    hns_roce_cmq_setup_basic_desc(&desc, HNS_ROCE_OPC_RESET_SCC_CTX, false);

    rst = (struct hns_roce_scc_ctx_clr_done *)desc.data;
    ret = memset_s(rst, sizeof(*rst), 0, sizeof(*rst));
    HNS_ROCE_SEC_CHECK_RET_INT(hr_dev->dev, ret);
    roce_set_bit(rst->rocee_scc_ctx_clr_done,
                 HNS_ROCE_V2_SCC_CTX_DONE_S,
                 0);

    ret =  hns_roce_cmq_send(hr_dev, &desc, 1);
    HNS_ROCE_CMQ_SEND_RET_CHECK(ret);

    /* clear scc context */
    hns_roce_cmq_setup_basic_desc(&desc, HNS_ROCE_OPC_SCC_CTX_CLR, false);

    scc_cxt_clr = (struct hns_roce_scc_ctx_clr *)desc.data;
    ret = memset_s(scc_cxt_clr, sizeof(*scc_cxt_clr), 0, sizeof(*scc_cxt_clr));
    HNS_ROCE_SEC_CHECK_RET_INT(hr_dev->dev, ret);
    scc_cxt_clr->rocee_scc_ctx_clr_qpn = hr_qp->qpn;

    ret =  hns_roce_cmq_send(hr_dev, &desc, 1);
    HNS_ROCE_CMQ_SEND_RET_CHECK(ret);

    /* query scc context clear is done or not */
    for (i = 0; i <= HNS_ROCE_CMQ_SCC_CLR_DONE_CNT; i++) {
        hns_roce_cmq_setup_basic_desc(&desc,
                                      HNS_ROCE_OPC_QUERY_SCC_CTX, true);
        resp = (struct hns_roce_scc_ctx_clr_done *)desc.data;
        ret = memset_s(resp, sizeof(*resp), 0, sizeof(*resp));
        HNS_ROCE_SEC_CHECK_RET_INT(hr_dev->dev, ret);

        ret = hns_roce_cmq_send(hr_dev, &desc, 1);
        HNS_ROCE_CMQ_SEND_RET_CHECK(ret);

        if (resp->rocee_scc_ctx_clr_done == 1) {
            return 0;
        }
    }

    dev_err(hr_dev->dev, "clear scc ctx failure!");
    return -EINVAL;
}

module_param(g_pktn_ini, int, 0644);
MODULE_PARM_DESC(g_pktn_ini, "default: 4");

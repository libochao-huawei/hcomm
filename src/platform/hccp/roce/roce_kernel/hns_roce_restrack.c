/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "roce_k_compat.h"

#ifdef HAVE_FILL_RES_ENTRY

#include <rdma/rdma_cm.h>
#include <rdma/restrack.h>
#include <uapi/rdma/rdma_netlink.h>
#include "hnae3.h"
#include "hns_roce_common.h"
#include "hns_roce_device.h"

#define VAL_OFFSET        32

static int hns_roce_fill_cq_apart(struct sk_buff *msg, struct hns_roce_v2_cq_context *context)
{
    if (rdma_nl_put_driver_u32(msg, "pi",
        roce_get_field(context->byte_28_cq_pi, V2_CQC_BYTE_28_CQ_PRODUCER_IDX_M, V2_CQC_BYTE_28_CQ_PRODUCER_IDX_S))) {
        PTR_RDMA_NL_PUT_DRIVER_U32_ERR("pi");
        return -EMSGSIZE;
    }

    if (rdma_nl_put_driver_u32(msg, "ci",
        roce_get_field(context->byte_32_cq_ci, V2_CQC_BYTE_32_CQ_CONSUMER_IDX_M, V2_CQC_BYTE_32_CQ_CONSUMER_IDX_S))) {
        PTR_RDMA_NL_PUT_DRIVER_U32_ERR("ci");
        return -EMSGSIZE;
    }

    if (rdma_nl_put_driver_u32(msg, "rdb_en",
        roce_get_field(context->byte_44_db_record, V2_CQC_BYTE_44_DB_RECORD_ADDR_M,
            V2_CQC_BYTE_44_DB_RECORD_ADDR_S))) {
        PTR_RDMA_NL_PUT_DRIVER_U32_ERR("rdb_en");
        return -EMSGSIZE;
    }

    if (rdma_nl_put_driver_u32(msg, "coalesce",
        roce_get_field(context->byte_56_cqe_period_maxcnt, V2_CQC_BYTE_56_CQ_MAX_CNT_M,
            V2_CQC_BYTE_56_CQ_MAX_CNT_S))) {
        PTR_RDMA_NL_PUT_DRIVER_U32_ERR("coalesce");
        return -EMSGSIZE;
    }

    if (rdma_nl_put_driver_u32(msg, "period",
        roce_get_field(context->byte_56_cqe_period_maxcnt, V2_CQC_BYTE_56_CQ_PERIOD_M, V2_CQC_BYTE_56_CQ_PERIOD_S))) {
        PTR_RDMA_NL_PUT_DRIVER_U32_ERR("period");
        return -EMSGSIZE;
    }

    if (rdma_nl_put_driver_u32(msg, "cnt", roce_get_field(context->byte_52_cqe_cnt, V2_CQC_BYTE_52_CQE_CNT_M,
        V2_CQC_BYTE_52_CQE_CNT_S))) {
        PTR_RDMA_NL_PUT_DRIVER_U32_ERR("cnt");
        return -EMSGSIZE;
    }

    if (rdma_nl_put_driver_u32(msg, "se_idx", roce_get_field(context->byte_64_se_cqe_idx,
        V2_CQC_BYTE_64_SE_CQE_IDX_M, V2_CQC_BYTE_64_SE_CQE_IDX_S))) {
        PTR_RDMA_NL_PUT_DRIVER_U32_ERR("se_idx");
        return -EMSGSIZE;
    }

    return 0;
}

static int hns_roce_fill_cq(struct sk_buff *msg, struct hns_roce_v2_cq_context *context)
{
    if (rdma_nl_put_driver_u32(msg, "state", roce_get_field(context->byte_4_pg_ceqn, V2_CQC_BYTE_4_ARM_ST_M,
        V2_CQC_BYTE_4_ARM_ST_S))) {
        PTR_RDMA_NL_PUT_DRIVER_U32_ERR("state");
        return -EMSGSIZE;
    }

    if (rdma_nl_put_driver_u32(msg, "ceqn",
        roce_get_field(context->byte_4_pg_ceqn, V2_CQC_BYTE_4_CEQN_M, V2_CQC_BYTE_4_CEQN_S))) {
        PTR_RDMA_NL_PUT_DRIVER_U32_ERR("ceqn");
        return -EMSGSIZE;
    }

    if (rdma_nl_put_driver_u32(msg, "poll", roce_get_bit(context->byte_4_pg_ceqn, V2_CQC_BYTE_4_POLL_S))) {
        PTR_RDMA_NL_PUT_DRIVER_U32_ERR("poll");
        return -EMSGSIZE;
    }

    if (rdma_nl_put_driver_u32(msg, "shift",
        roce_get_field(context->byte_4_pg_ceqn, V2_CQC_BYTE_4_SHIFT_M, V2_CQC_BYTE_4_SHIFT_S))) {
        PTR_RDMA_NL_PUT_DRIVER_U32_ERR("shift");
        return -EMSGSIZE;
    }

    if (rdma_nl_put_driver_u32(msg, "cmd_sn",
        roce_get_field(context->byte_4_pg_ceqn, V2_CQC_BYTE_4_CMD_SN_M, V2_CQC_BYTE_4_CMD_SN_S))) {
        PTR_RDMA_NL_PUT_DRIVER_U32_ERR("cmd_sn");
        return -EMSGSIZE;
    }

    if (rdma_nl_put_driver_u32(msg, "cqn",
        roce_get_field(context->byte_8_cqn, V2_CQC_BYTE_8_CQN_M, V2_CQC_BYTE_8_CQN_S))) {
        PTR_RDMA_NL_PUT_DRIVER_U32_ERR("cqn");
        return -EMSGSIZE;
    }

    if (rdma_nl_put_driver_u32(msg, "hopnum",
        roce_get_field(context->byte_16_hop_addr, V2_CQC_BYTE_16_CQE_HOP_NUM_M, V2_CQC_BYTE_16_CQE_HOP_NUM_S))) {
        PTR_RDMA_NL_PUT_DRIVER_U32_ERR("hopnum");
        return -EMSGSIZE;
    }

    return hns_roce_fill_cq_apart(msg, context);
}

static int hns_roce_fill_res_cq_entry(struct sk_buff *msg,
                                      struct rdma_restrack_entry *res)
{
    struct ib_cq  *ib_cq = container_of(res, struct ib_cq, res);
    struct hns_roce_dev *hr_dev = to_hr_dev(ib_cq->device);
    struct hns_roce_cq *hr_cq = to_hr_cq(ib_cq);
    struct hns_roce_v2_cq_context context;
    struct nlattr *table_attr = NULL;
    int ret;

    if (hr_dev->dfx->query_cqc_info == NULL) {
        dev_err(hr_dev->dev, "current hns_roce device dfx query cqc info function is abnormal\n");
        return -EINVAL;
    }

    ret = hr_dev->dfx->query_cqc_info(hr_dev, hr_cq->cqn, (int *)&context);
    if (ret) {
        dev_err(hr_dev->dev, "dfx query cqc info failed\n");
        goto err;
    }

    table_attr = nla_nest_start(msg, RDMA_NLDEV_ATTR_DRIVER);
    if (table_attr == NULL) {
        dev_err(hr_dev->dev, "start a new level of nested attribute RDMA_NLDEV_ATTR_DRIVER[%d] failed\n",
            RDMA_NLDEV_ATTR_DRIVER);
        goto err;
    }

    if (hns_roce_fill_cq(msg, &context)) {
        dev_err(hr_dev->dev, "fill cq failed\n");
        goto del_err_table;
    }

    nla_nest_end(msg, table_attr);

    return 0;

del_err_table:
    nla_nest_cancel(msg, table_attr);
err:
    return -EMSGSIZE;
}

static int hns_roce_qp_fill_rp(struct sk_buff *msg, struct hns_roce_v2_qp_context *context)
{
    if (rdma_nl_put_driver_u32(msg, "rq_pi", roce_get_field(context->byte_84_rq_ci_pi,
        V2_QPC_BYTE_84_RQ_PRODUCER_IDX_M, V2_QPC_BYTE_84_RQ_PRODUCER_IDX_S))) {
        PTR_RDMA_NL_PUT_DRIVER_U32_ERR("rq_pi");
        return -EMSGSIZE;
    }

    if (rdma_nl_put_driver_u32(msg, "rq_ci", roce_get_field(context->byte_84_rq_ci_pi,
        V2_QPC_BYTE_84_RQ_CONSUMER_IDX_M, V2_QPC_BYTE_84_RQ_CONSUMER_IDX_S))) {
        PTR_RDMA_NL_PUT_DRIVER_U32_ERR("rq_ci");
        return -EMSGSIZE;
    }

    if (rdma_nl_put_driver_u32(msg, "rq_shift", roce_get_field(context->byte_20_smac_sgid_idx,
        V2_QPC_BYTE_20_RQ_SHIFT_M, V2_QPC_BYTE_20_RQ_SHIFT_S))) {
        PTR_RDMA_NL_PUT_DRIVER_U32_ERR("rq_shift");
        return -EMSGSIZE;
    }

    if (rdma_nl_put_driver_u32(msg, "rq_cqeidx", roce_get_field(context->byte_256_sqflush_rqcqe,
        V2_QPC_BYTE_256_RQ_CQE_IDX_M, V2_QPC_BYTE_256_RQ_CQE_IDX_S))) {
        PTR_RDMA_NL_PUT_DRIVER_U32_ERR("rq_cqeidx");
        return -EMSGSIZE;
    }

    if (rdma_nl_put_driver_u32(msg, "rq_rx_err", roce_get_bit(context->byte_56_dqpn_err,
        V2_QPC_BYTE_56_RQ_RX_ERR_S))) {
        PTR_RDMA_NL_PUT_DRIVER_U32_ERR("rq_rx_err");
        return -EMSGSIZE;
    }

    if (rdma_nl_put_driver_u32(msg, "rq_tx_err", roce_get_bit(context->byte_56_dqpn_err,
        V2_QPC_BYTE_56_RQ_TX_ERR_S))) {
        PTR_RDMA_NL_PUT_DRIVER_U32_ERR("rq_tx_err");
        return -EMSGSIZE;
    }

    if (rdma_nl_put_driver_u32(msg, "rq_rty_tx_err", roce_get_bit(context->byte_76_srqn_op_en,
        V2_QPC_BYTE_76_RQ_RTY_TX_ERR_S))) {
        PTR_RDMA_NL_PUT_DRIVER_U32_ERR("rq_rty_tx_err");
        return -EMSGSIZE;
    }

    if (rdma_nl_put_driver_u32(msg, "rq_db_doing", roce_get_bit(context->byte_60_qpst_tempid,
        V2_QPC_BYTE_60_RQ_DB_DOING_S))) {
        PTR_RDMA_NL_PUT_DRIVER_U32_ERR("rq_db_doing");
        return -EMSGSIZE;
    }

    if (rdma_nl_put_driver_u32(msg, "rx_cqn", roce_get_field(context->byte_80_rnr_rx_cqn,
        V2_QPC_BYTE_80_RX_CQN_M, V2_QPC_BYTE_80_RX_CQN_S))) {
        PTR_RDMA_NL_PUT_DRIVER_U32_ERR("rx_cqn");
        return -EMSGSIZE;
    }

    return 0;
}

static int hns_roce_qp_fill_sp(struct sk_buff *msg, struct hns_roce_v2_qp_context *context)
{
    if (rdma_nl_put_driver_u32(msg, "sq_pi", roce_get_field(context->byte_160_sq_ci_pi,
        V2_QPC_BYTE_160_SQ_PRODUCER_IDX_M, V2_QPC_BYTE_160_SQ_PRODUCER_IDX_S))) {
        PTR_RDMA_NL_PUT_DRIVER_U32_ERR("sq_pi");
        return -EMSGSIZE;
    }

    if (rdma_nl_put_driver_u32(msg, "sq_ci", roce_get_field(context->byte_160_sq_ci_pi,
        V2_QPC_BYTE_160_SQ_CONSUMER_IDX_M, V2_QPC_BYTE_160_SQ_CONSUMER_IDX_S))) {
        PTR_RDMA_NL_PUT_DRIVER_U32_ERR("sq_ci");
        return -EMSGSIZE;
    }

    if (rdma_nl_put_driver_u32(msg, "sq_shift", roce_get_field(
                                   context->byte_20_smac_sgid_idx, V2_QPC_BYTE_20_SQ_SHIFT_M,
                                   V2_QPC_BYTE_20_SQ_SHIFT_S))) {
        PTR_RDMA_NL_PUT_DRIVER_U32_ERR("sq_shift");
        return -EMSGSIZE;
    }

    if (rdma_nl_put_driver_u32(msg, "sq_maxidx", roce_get_field(context->byte_200_sq_max,
        V2_QPC_BYTE_200_SQ_MAX_IDX_M, V2_QPC_BYTE_200_SQ_MAX_IDX_S))) {
        PTR_RDMA_NL_PUT_DRIVER_U32_ERR("sq_maxidx");
        return -EMSGSIZE;
    }

    if (rdma_nl_put_driver_u32(msg, "sq_rx_err", roce_get_bit(context->byte_56_dqpn_err,
        V2_QPC_BYTE_56_SQ_RX_ERR_S))) {
        PTR_RDMA_NL_PUT_DRIVER_U32_ERR("sq_rx_err");
        return -EMSGSIZE;
    }

    if (rdma_nl_put_driver_u32(msg, "sq_tx_err", roce_get_bit(context->byte_56_dqpn_err,
        V2_QPC_BYTE_56_SQ_TX_ERR_S))) {
        PTR_RDMA_NL_PUT_DRIVER_U32_ERR("sq_tx_err");
        return -EMSGSIZE;
    }

    if (rdma_nl_put_driver_u32(msg, "sq_db_doing", roce_get_bit(context->byte_60_qpst_tempid,
        V2_QPC_BYTE_60_SQ_DB_DOING_S))) {
        PTR_RDMA_NL_PUT_DRIVER_U32_ERR("sq_db_doing");
        return -EMSGSIZE;
    }

    if (rdma_nl_put_driver_u32(msg, "sq_tx_err", roce_get_bit(context->byte_56_dqpn_err,
        V2_QPC_BYTE_56_SQ_TX_ERR_S))) {
        PTR_RDMA_NL_PUT_DRIVER_U32_ERR("sq_tx_err");
        return -EMSGSIZE;
    }

    if (rdma_nl_put_driver_u32(msg, "tx_cqn", roce_get_field(context->byte_252_err_txcqn,
        V2_QPC_BYTE_252_TX_CQN_M, V2_QPC_BYTE_252_TX_CQN_S))) {
        PTR_RDMA_NL_PUT_DRIVER_U32_ERR("tx_cqn");
        return -EMSGSIZE;
    }

    return 0;
}

static int hns_roce_fill_qp_apart(struct sk_buff *msg, struct hns_roce_v2_qp_context *context)
{
    if (rdma_nl_put_driver_u32(msg, "srqn", roce_get_field(context->byte_76_srqn_op_en, V2_QPC_BYTE_76_SRQN_M,
        V2_QPC_BYTE_76_SRQN_S))) {
        PTR_RDMA_NL_PUT_DRIVER_U32_ERR("srqn");
        return -EMSGSIZE;
    }

    if (rdma_nl_put_driver_u32(msg, "srq_en", roce_get_bit(context->byte_76_srqn_op_en, V2_QPC_BYTE_76_SRQ_EN_S))) {
        PTR_RDMA_NL_PUT_DRIVER_U32_ERR("srq_en");
        return -EMSGSIZE;
    }

    if (rdma_nl_put_driver_u32(msg, "chk_flg", roce_get_field(context->byte_212_lsn, V2_QPC_BYTE_212_CHECK_FLG_M,
        V2_QPC_BYTE_212_CHECK_FLG_S))) {
        PTR_RDMA_NL_PUT_DRIVER_U32_ERR("chk_flg");
        return -EMSGSIZE;
    }

    if (rdma_nl_put_driver_u32(msg, "retry_cnt", roce_get_field(context->byte_212_lsn, V2_QPC_BYTE_212_RETRY_CNT_M,
        V2_QPC_BYTE_212_RETRY_CNT_S))) {
        PTR_RDMA_NL_PUT_DRIVER_U32_ERR("retry_cnt");
        return -EMSGSIZE;
    }

    if (rdma_nl_put_driver_u32(msg, "err_type", roce_get_field(context->byte_252_err_txcqn, V2_QPC_BYTE_252_ERR_TYPE_M,
        V2_QPC_BYTE_252_ERR_TYPE_S))) {
        PTR_RDMA_NL_PUT_DRIVER_U32_ERR("err_type");
        return -EMSGSIZE;
    }

    if (rdma_nl_put_driver_u32(msg, "flush_idx",
                               roce_get_field(context->byte_256_sqflush_rqcqe, V2_QPC_BYTE_256_SQ_FLUSH_IDX_M,
                                   V2_QPC_BYTE_256_SQ_FLUSH_IDX_S))) {
        PTR_RDMA_NL_PUT_DRIVER_U32_ERR("flush_idx");
        return -EMSGSIZE;
    }

    if (hns_roce_qp_fill_rp(msg, context)) {
        pr_err("hns3: qp fill rp failed\n");
        return -EMSGSIZE;
    }

    if (hns_roce_qp_fill_sp(msg, context)) {
        pr_err("hns3: qp fill sp failed\n");
        return -EMSGSIZE;
    }

    return 0;
}

static int hns_roce_fill_qp(struct sk_buff *msg, struct hns_roce_v2_qp_context *context)
{
    if (rdma_nl_put_driver_u32(msg, "smac_idx",
                               roce_get_field(context->byte_20_smac_sgid_idx, V2_QPC_BYTE_20_SMAC_IDX_M,
                                   V2_QPC_BYTE_20_SMAC_IDX_S))) {
        PTR_RDMA_NL_PUT_DRIVER_U32_ERR("smac_idx");
        return -EMSGSIZE;
    }

    if (rdma_nl_put_driver_u32(msg, "vid", roce_get_field(context->byte_24_mtu_tc, V2_QPC_BYTE_24_VLAN_ID_M,
        V2_QPC_BYTE_24_VLAN_ID_S))) {
        PTR_RDMA_NL_PUT_DRIVER_U32_ERR("vid");
        return -EMSGSIZE;
    }

    if (rdma_nl_put_driver_u32(msg, "mtu", roce_get_field(context->byte_24_mtu_tc, V2_QPC_BYTE_24_MTU_M,
        V2_QPC_BYTE_24_MTU_S))) {
        PTR_RDMA_NL_PUT_DRIVER_U32_ERR("mtu");
        return -EMSGSIZE;
    }

    if (rdma_nl_put_driver_u32(msg, "sgid_idx",
                               roce_get_field(context->byte_20_smac_sgid_idx, V2_QPC_BYTE_20_SGID_IDX_M,
                                   V2_QPC_BYTE_20_SGID_IDX_S))) {
        PTR_RDMA_NL_PUT_DRIVER_U32_ERR("sgid_idx");
        return -EMSGSIZE;
    }
    return hns_roce_fill_qp_apart(msg, context);
}

static int hns_roce_fill_res_qp_entry(struct sk_buff *msg,
                                      struct rdma_restrack_entry *res)
{
    struct ib_qp  *ib_qp = container_of(res, struct ib_qp, res);
    struct hns_roce_dev *hr_dev = to_hr_dev(ib_qp->device);
    struct hns_roce_qp *hr_qp = to_hr_qp(ib_qp);
    struct hns_roce_v2_qp_context context;
    struct nlattr *table_attr = NULL;
    int ret;

    if (hr_dev->dfx->query_qpc_info == NULL) {
        dev_err(hr_dev->dev, "current hns_roce device dfx query qpc info function is abnormal\n");
        return -EINVAL;
    }

    ret = hr_dev->dfx->query_qpc_info(hr_dev, hr_qp->qpn, (int *)&context);
    if (ret) {
        dev_err(hr_dev->dev, "dfx query qpc info failed, ret[%d]\n", ret);
        goto err;
    }

    table_attr = nla_nest_start(msg, RDMA_NLDEV_ATTR_DRIVER);
    if (table_attr == NULL) {
        dev_err(hr_dev->dev, "start a new level of nested attribute RDMA_NLDEV_ATTR_DRIVER[%d] failed\n",
            RDMA_NLDEV_ATTR_DRIVER);
        goto err;
    }

    if (hns_roce_fill_qp(msg, &context)) {
        dev_err(hr_dev->dev, "fill qp failed\n");
        goto del_err_table;
    }

    nla_nest_end(msg, table_attr);

    return 0;

del_err_table:
    nla_nest_cancel(msg, table_attr);
err:
    return -EMSGSIZE;
}

static int hns_roce_fill_mr_apart(struct sk_buff *msg, struct hns_roce_v2_mpt_entry *context)
{
    if (rdma_nl_put_driver_u32(msg, "atomic_en", roce_get_bit(context->byte_8_mw_cnt_en, V2_MPT_BYTE_8_ATOMIC_EN_S))) {
        PTR_RDMA_NL_PUT_DRIVER_U32_ERR("atomic_en");
        return -EMSGSIZE;
    }

    if (rdma_nl_put_driver_u32(msg, "rr_en", roce_get_bit(context->byte_8_mw_cnt_en, V2_MPT_BYTE_8_RR_EN_S))) {
        PTR_RDMA_NL_PUT_DRIVER_U32_ERR("rr_en");
        return -EMSGSIZE;
    }

    if (rdma_nl_put_driver_u32(msg, "rw_en", roce_get_bit(context->byte_8_mw_cnt_en, V2_MPT_BYTE_8_RW_EN_S))) {
        PTR_RDMA_NL_PUT_DRIVER_U32_ERR("rw_en");
        return -EMSGSIZE;
    }

    if (rdma_nl_put_driver_u32(msg, "lw_en", roce_get_bit(context->byte_8_mw_cnt_en, V2_MPT_BYTE_8_LW_EN_S))) {
        PTR_RDMA_NL_PUT_DRIVER_U32_ERR("lw_en");
        return -EMSGSIZE;
    }

    if (rdma_nl_put_driver_u32(msg, "pbl_buf_pgsz", roce_get_field(context->byte_64_buf_pa1,
        V2_MPT_BYTE_64_PBL_BUF_PG_SZ_M, V2_MPT_BYTE_64_PBL_BUF_PG_SZ_S))) {
        PTR_RDMA_NL_PUT_DRIVER_U32_ERR("pbl_buf_pasz");
        return -EMSGSIZE;
    }

    val_h32 = context->len_h;
    if (rdma_nl_put_driver_u64(msg, "len", (val_h32 << VAL_OFFSET) | context->len_l)) {
        PTR_RDMA_NL_PUT_DRIVER_U32_ERR("context len");
        return -EMSGSIZE;
    }

    return 0;
}

static int hns_roce_fill_mr(struct sk_buff *msg, struct hns_roce_v2_mpt_entry *context)
{
    u64 val_h32;

    if (rdma_nl_put_driver_u32(msg, "status", roce_get_field(context->byte_4_pd_hop_st, V2_MPT_BYTE_4_MPT_ST_M,
        V2_MPT_BYTE_4_MPT_ST_S))) {
        PTR_RDMA_NL_PUT_DRIVER_U32_ERR("byte_4_pd_hop status");
        return -EMSGSIZE;
    }

    if (rdma_nl_put_driver_u32(msg, "lkey", context->lkey)) {
        PTR_RDMA_NL_PUT_DRIVER_U32_ERR("lkey");
        return -EMSGSIZE;
    }

    if (rdma_nl_put_driver_u32(msg, "size", context->pbl_size)) {
        PTR_RDMA_NL_PUT_DRIVER_U32_ERR("PBL size");
        return -EMSGSIZE;
    }

    if (rdma_nl_put_driver_u32(msg, "ra", roce_get_bit(context->byte_8_mw_cnt_en, V2_MPT_BYTE_8_RA_EN_S))) {
        PTR_RDMA_NL_PUT_DRIVER_U32_ERR("ra");
        return -EMSGSIZE;
    }

    if (rdma_nl_put_driver_u32(msg, "ri", roce_get_bit(context->byte_8_mw_cnt_en, V2_MPT_BYTE_8_R_INV_EN_S))) {
        PTR_RDMA_NL_PUT_DRIVER_U32_ERR("ri");
        return -EMSGSIZE;
    }

    if (rdma_nl_put_driver_u32(msg, "li", roce_get_bit(context->byte_8_mw_cnt_en, V2_MPT_BYTE_8_L_INV_EN_S))) {
        PTR_RDMA_NL_PUT_DRIVER_U32_ERR("li");
        return -EMSGSIZE;
    }
    return hns_roce_fill_mr_apart(msg, context);
}

static int hns_roce_fill_res_mr_entry(struct sk_buff *msg,
                                      struct rdma_restrack_entry *res)
{
    struct ib_mr  *ib_mr = container_of(res, struct ib_mr, res);
    struct hns_roce_dev *hr_dev = to_hr_dev(ib_mr->device);
    struct hns_roce_mr *hr_mr = to_hr_mr(ib_mr);
    struct hns_roce_v2_mpt_entry context;
    int key = hr_mr->key;
    struct nlattr *table_attr = NULL;
    int ret;

    if (!hr_dev->dfx->query_mpt_info) {
        dev_err(hr_dev->dev, "current hns_roce device dfx query mpt info function is abnormal\n");
        return -EINVAL;
    }

    ret = hr_dev->dfx->query_mpt_info(hr_dev, key, (int *)&context);
    if (ret) {
        dev_err(hr_dev->dev, "dfx query mpt info failed\n");
        goto err;
    }

    table_attr = nla_nest_start(msg, RDMA_NLDEV_ATTR_DRIVER);
    if (table_attr == NULL) {
        dev_err(hr_dev->dev, "start a new level of nested attribute RDMA_NLDEV_ATTR_DRIVER[%d] failed\n",
            RDMA_NLDEV_ATTR_DRIVER);
        goto err;
    }

    if (hns_roce_fill_mr(msg, &context)) {
        dev_err(hr_dev->dev, "fill mr failed\n");
        goto del_err_table;
    }

    nla_nest_end(msg, table_attr);

    return 0;

del_err_table:
    nla_nest_cancel(msg, table_attr);
err:
    return -EMSGSIZE;
}

static int hns_roce_fill_pd(struct sk_buff *msg,
                            struct hns_roce_pd *hr_pd)
{
    if (rdma_nl_put_driver_u32(msg, "pdn", hr_pd->pdn)) {
        PTR_RDMA_NL_PUT_DRIVER_U32_ERR("hr_pd pdn");
        goto err;
    }

    return 0;

err:
    return -EMSGSIZE;
}

static int hns_roce_fill_res_pd_entry(struct sk_buff *msg,
                                      struct rdma_restrack_entry *res)
{
    struct ib_pd  *ib_pd = container_of(res, struct ib_pd, res);
    struct hns_roce_pd *hr_pd = to_hr_pd(ib_pd);
    struct nlattr *table_attr;

    table_attr = nla_nest_start(msg, RDMA_NLDEV_ATTR_DRIVER);
    if (table_attr == NULL) {
        dev_err(hr_dev->dev, "start a new level of nested attribute RDMA_NLDEV_ATTR_DRIVER[%d] failed\n",
            RDMA_NLDEV_ATTR_DRIVER);
        goto err;
    }

    if (hns_roce_fill_pd(msg, hr_pd)) {
        dev_err(hr_dev->dev, "fill hr_pd failed\n");
        goto del_err_table;
    }

    nla_nest_end(msg, table_attr);

    return 0;

del_err_table:
    nla_nest_cancel(msg, table_attr);
err:
    return -EMSGSIZE;
}


int hns_roce_fill_res_entry(struct sk_buff *msg,
                            struct rdma_restrack_entry *res)
{
    if (msg == NULL || res == NULL) {
        pr_err("hns3: fill_res_entry param err, msg %pK, res %pK\n", msg, res);
        return -EINVAL;
    }

    if (res->type == RDMA_RESTRACK_PD) {
        return hns_roce_fill_res_pd_entry(msg, res);
    }

    if (res->type == RDMA_RESTRACK_CQ) {
        return hns_roce_fill_res_cq_entry(msg, res);
    }

    if (res->type == RDMA_RESTRACK_QP) {
        return hns_roce_fill_res_qp_entry(msg, res);
    }

    if (res->type == RDMA_RESTRACK_MR) {
        return hns_roce_fill_res_mr_entry(msg, res);
    }

    return 0;
}
#endif


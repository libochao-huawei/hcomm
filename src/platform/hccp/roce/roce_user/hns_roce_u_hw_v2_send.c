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
#include <util/compiler.h>
#include <malloc.h>
#include "hns_roce_u.h"
#include "hns_roce_u_abi.h"
#include "hns_roce_u_hw_v2.h"
#include "securec.h"
#include "hns_roce_u_hw_v2_qp.h"
#include "hns_roce_u_hw_v2_opreation.h"
#include "hns_roce_u_hw_v2_send.h"

#ifdef DEFINE_HNS_LLT
#undef static
#define static
#endif

STATIC void hns_roce_u_v2_set_ud_wqe_dmac(const struct hns_roce_ah *ah,
                                          struct hns_roce_ud_sq_wqe *ud_sq_wqe)
{
    roce_set_field(ud_sq_wqe->dmac, UD_SQ_WQE_DMAC_0_M,
                   UD_SQ_WQE_DMAC_0_S, ah->av.mac[MAC_IDENX_0]);

    roce_set_field(ud_sq_wqe->dmac, UD_SQ_WQE_DMAC_1_M,
                   UD_SQ_WQE_DMAC_1_S, ah->av.mac[MAC_IDENX_1]);

    roce_set_field(ud_sq_wqe->dmac, UD_SQ_WQE_DMAC_2_M,
                   UD_SQ_WQE_DMAC_2_S, ah->av.mac[MAC_IDENX_2]);

    roce_set_field(ud_sq_wqe->dmac, UD_SQ_WQE_DMAC_3_M,
                   UD_SQ_WQE_DMAC_3_S, ah->av.mac[MAC_IDENX_3]);

    roce_set_field(ud_sq_wqe->smac_index_dmac, UD_SQ_WQE_DMAC_4_M,
                   UD_SQ_WQE_DMAC_4_S, ah->av.mac[MAC_IDENX_4]);

    roce_set_field(ud_sq_wqe->smac_index_dmac, UD_SQ_WQE_DMAC_5_M,
                   UD_SQ_WQE_DMAC_5_S, ah->av.mac[MAC_IDENX_5]);

    roce_set_field(ud_sq_wqe->smac_index_dmac, UD_SQ_WQE_SGID_IDX_M,
                   UD_SQ_WQE_SGID_IDX_S, ah->av.gid_index);
}

STATIC void hns_roce_u_v2_set_ud_wqe_common(struct ibv_send_wr *wr,
                                            struct hns_roce_ud_sq_wqe *ud_sq_wqe,
                                            struct hns_roce_qp *qp, int nreq,
                                            struct hns_roce_sge_info *sge_info)
{
    /* Set sig attr */
    roce_set_bit(ud_sq_wqe->rsv_opcode, UD_SQ_WQE_CQE_S,
                 (wr->send_flags & IBV_SEND_SIGNALED) ? 1 : 0);

    /* Set se attr */
    roce_set_bit(ud_sq_wqe->rsv_opcode, UD_SQ_WQE_SE_S,
                 (wr->send_flags & IBV_SEND_SOLICITED) ? 1 : 0);

    roce_set_bit(ud_sq_wqe->rsv_opcode, UD_SQ_WQE_OWNER_S,
                 ~(((qp->sq.head + (unsigned int)nreq) >> qp->sq.shift) & 0x1));

    roce_set_field(ud_sq_wqe->sge_num_pd, UD_SQ_WQE_PD_M,
                   UD_SQ_WQE_PD_S, to_hr_pd(qp->ibv_qp.pd)->pdn);

    roce_set_field(ud_sq_wqe->rsv_msg_start_sge_idx,
                   UD_SQ_WQE_MSG_START_SGE_IDX_M,
                   UD_SQ_WQE_MSG_START_SGE_IDX_S,
                   (sge_info->start_idx) & (qp->sge.sge_cnt - 1));

    roce_set_field(ud_sq_wqe->udpspn_rsv, UD_SQ_WQE_UDP_SPN_M,
                   UD_SQ_WQE_UDP_SPN_S, 0);
}

STATIC void hns_roce_u_v2_set_ud_wqe_dest(struct ibv_send_wr *wr,
                                          struct hns_roce_ud_sq_wqe *ud_sq_wqe,
                                          struct hns_roce_ah *ah, const struct hns_roce_qp *qp)
{
    roce_set_field(ud_sq_wqe->rsv_dqpn, UD_SQ_WQE_DQPN_M,
                   UD_SQ_WQE_DQPN_S, htole32(wr->wr.ud.remote_qpn));

    roce_set_field(ud_sq_wqe->tclass_vlan, UD_SQ_WQE_VLAN_M,
                   UD_SQ_WQE_VLAN_S, ah->av.vlan);

    roce_set_field(ud_sq_wqe->tclass_vlan, UD_SQ_WQE_TCLASS_M,
                   UD_SQ_WQE_TCLASS_S, htole16(ah->av.tclass));

    roce_set_field(ud_sq_wqe->tclass_vlan, UD_SQ_WQE_HOPLIMIT_M,
                   UD_SQ_WQE_HOPLIMIT_S, ah->av.hop_limit);

    roce_set_field(ud_sq_wqe->lbi_flow_label, UD_SQ_WQE_FLOW_LABEL_M,
                   UD_SQ_WQE_FLOW_LABEL_S, htole32(ah->av.flowlabel));

    roce_set_bit(ud_sq_wqe->lbi_flow_label, UD_SQ_WQE_LBI_S, 0);

    roce_set_field(ud_sq_wqe->lbi_flow_label, UD_SQ_WQE_SL_M,
                   UD_SQ_WQE_SL_S, htole16(ah->av.sl) & 0x7);

    roce_set_field(ud_sq_wqe->lbi_flow_label, UD_SQ_WQE_PORTN_M,
                   UD_SQ_WQE_PORTN_S, qp->ibv_qp.qp_num);
}
/*lint -e409*/
STATIC void hns_roce_u_v2_get_sge_info(struct ibv_send_wr *wr,
                                       struct hns_roce_sge_info *sge_info)
{
    int i;

    sge_info->total_len = 0;
    sge_info->valid_num = 0;

    for (i = 0; i < wr->num_sge; i++) {
        if (likely(wr->sg_list[i].length)) {
            sge_info->total_len += wr->sg_list[i].length;
            sge_info->valid_num++;
        }
    }
}

STATIC void hns_roce_u_v2_set_sge(struct hns_roce_v2_wqe_data_seg *dseg,
                                  struct hns_roce_qp *qp, struct ibv_send_wr *wr,
                                  struct hns_roce_sge_info *sge_info)
{
    int i;
    int valid_num = 0;

    for (i = 0; i < wr->num_sge; i++) {
        if (likely(wr->sg_list[i].length)) {
            valid_num++;
            /* No inner sge in UD wqe */
            if (valid_num <= HNS_ROCE_SGE_IN_WQE && qp->ibv_qp.qp_type != IBV_QPT_UD) {
                set_data_seg_v2(dseg, wr->sg_list + i);
                dseg++;
            } else {
                dseg = get_send_sge_ex(qp, (sge_info->start_idx) & (qp->sge.sge_cnt - 1));
                set_data_seg_v2(dseg, wr->sg_list + i);
                sge_info->start_idx++;
            }
        }
    }
}

STATIC int hns_roce_u_v2_set_ud_wqe_opcode(struct ibv_send_wr *wr,
                                           struct hns_roce_ud_sq_wqe *ud_sq_wqe)
{
    unsigned int hr_op;

    switch (wr->opcode) {
        case IBV_WR_SEND:
            hr_op = HNS_ROCE_WQE_OP_SEND;
            ud_sq_wqe->immtdata = 0;
            break;
        case IBV_WR_SEND_WITH_IMM:
            hr_op = HNS_ROCE_WQE_OP_SEND_WITH_IMM;
            ud_sq_wqe->immtdata = htole32(be32toh(wr->imm_data));
            break;
        default:
            roce_err("Not supported wr opcode %d", wr->opcode);
            return -EINVAL;
    }

    roce_set_field(ud_sq_wqe->rsv_opcode, UD_SQ_WQE_OPCODE_M,
                   UD_SQ_WQE_OPCODE_S, hr_op);

    return 0;
}

STATIC int hns_roce_u_v2_fill_ext_sge_inl_data(struct hns_roce_qp *qp,
                                               const struct ibv_send_wr *wr,
                                               struct hns_roce_sge_info *sge_info)
{
    int i;
    int ret = 0;
    unsigned int sge_sz = sizeof(struct hns_roce_v2_wqe_data_seg);
    void *dseg = NULL;

    if (sge_info->total_len > qp->sq.max_gs * sge_sz) {
        roce_err("no enough extended sge space for inline data.");
        return -EINVAL;
    }

    dseg = get_send_sge_ex(qp, sge_info->start_idx & (qp->sge.sge_cnt - 1));

    for (i = 0; i < wr->num_sge; i++) {
        ret = memcpy_s(dseg, wr->sg_list[i].length,
                       (void *)(uintptr_t)wr->sg_list[i].addr,
                       wr->sg_list[i].length);
        if (ret) {
            roce_err("memcpy_s inline data extend sge failed! ret %d", ret);
            return ret;
        }

        dseg = wr->sg_list[i].length + (char *)dseg;
    }

    sge_info->start_idx += sge_info->total_len / sge_sz;

    return 0;
}

STATIC int hns_roce_u_v2_fill_ud_inn_inl_data(const struct ibv_send_wr *wr,
                                              struct hns_roce_ud_sq_wqe *ud_sq_wqe)
{
    uint8_t data[HNS_ROCE_MAX_UD_INL_INN_SZ] = {0};
    uint32_t *loc = (uint32_t *)data;
    uint32_t tmp_data;
    void *tmp = data;
    int i;
    int ret = 0;

    for (i = 0; i < wr->num_sge; i++) {
        ret = memcpy_s(tmp, wr->sg_list[i].length,
                       (void *)(uintptr_t)wr->sg_list[i].addr,
                       wr->sg_list[i].length);
        if (ret) {
            roce_err("memcpy_s ud inline data failed! ret %d", ret);
            return ret;
        }

        tmp = wr->sg_list[i].length + (char *)tmp;
    }

    roce_set_field(ud_sq_wqe->msg_len,
                   UD_SQ_WQE_BYTE_8_INL_DATE_15_0_M,
                   UD_SQ_WQE_BYTE_8_INL_DATE_15_0_S,
                   *loc & 0xffff);

    roce_set_field(ud_sq_wqe->sge_num_pd,
                   UD_SQ_WQE_BYTE_16_INL_DATA_23_16_M,
                   UD_SQ_WQE_BYTE_16_INL_DATA_23_16_S,
                   (*loc >> 0x10) & 0xff);

    tmp_data = *loc >> 0x18;
    loc++;
    tmp_data |= ((*loc & 0xffff) << 0x8);

    roce_set_field(ud_sq_wqe->rsv_msg_start_sge_idx,
                   UD_SQ_WQE_BYTE_20_INL_DATA_47_24_M,
                   UD_SQ_WQE_BYTE_20_INL_DATA_47_24_S,
                   tmp_data);

    roce_set_field(ud_sq_wqe->udpspn_rsv,
                   UD_SQ_WQE_BYTE_24_INL_DATA_63_48_M,
                   UD_SQ_WQE_BYTE_24_INL_DATA_63_48_S,
                   *loc >> 0x10);

    return ret;
}

STATIC int hns_roce_u_v2_set_ud_inl(struct hns_roce_qp *qp, const struct ibv_send_wr *wr,
                                    struct hns_roce_ud_sq_wqe *ud_sq_wqe,
                                    struct hns_roce_sge_info *sge_info)
{
    int ret;
    unsigned int sge_idx = sge_info->start_idx;

    if (wr->opcode == IBV_WR_RDMA_READ) {
        roce_err("Not support rdma read inline data!");
        return -EINVAL;
    }

    if (sge_info->total_len > qp->max_inline_data) {
        roce_err("failed to inline, data len %u, max inline len %u!",
                 sge_info->total_len, qp->max_inline_data);
        return -EINVAL;
    }

    roce_set_bit(ud_sq_wqe->rsv_opcode, UD_SQ_WQE_BYTE_4_INL_S, 1);

    if (sge_info->total_len <= HNS_ROCE_MAX_UD_INL_INN_SZ) {
        roce_set_bit(ud_sq_wqe->rsv_msg_start_sge_idx,
                     UD_SQ_WQE_BYTE_20_INL_TYPE_S, 0);

        ret = hns_roce_u_v2_fill_ud_inn_inl_data(wr, ud_sq_wqe);
        if (ret) {
            roce_err("fill ud inline data failed! ret %d", ret);
            return ret;
        }
    } else {
        roce_set_bit(ud_sq_wqe->rsv_msg_start_sge_idx,
                     UD_SQ_WQE_BYTE_20_INL_TYPE_S, 1U);

        ret = hns_roce_u_v2_fill_ext_sge_inl_data(qp, wr, sge_info);
        if (ret) {
            roce_err("fill extend sge inline data failed! ret %d", ret);
            return ret;
        }

        sge_info->valid_num = sge_info->start_idx - sge_idx;

        roce_set_field(ud_sq_wqe->sge_num_pd, UD_SQ_WQE_SGE_NUM_M,
                       UD_SQ_WQE_SGE_NUM_S, sge_info->valid_num);
    }

    return 0;
}

STATIC int hns_roce_u_v2_set_ud_wqe(void *wqe, struct hns_roce_qp *qp, struct ibv_send_wr *wr,
                                    int nreq, struct hns_roce_sge_info *sge_info)
{
    int ret;
    struct hns_roce_ah *ah = NULL;
    struct hns_roce_ud_sq_wqe *ud_sq_wqe;
    struct hns_roce_v2_wqe_data_seg *dseg;

    ah = to_hr_ah(wr->wr.ud.ah);
    ud_sq_wqe = wqe;
    dseg = wqe;

    ret = memset_s(ud_sq_wqe, sizeof(*ud_sq_wqe), 0, sizeof(*ud_sq_wqe));
    if (ret) {
        roce_err("memset_s ud_sq_wqe failed, ret %d", ret);
        return ret;
    }

    hns_roce_u_v2_set_ud_wqe_dmac(ah, ud_sq_wqe);

    ret = hns_roce_u_v2_set_ud_wqe_opcode(wr, ud_sq_wqe);
    if (ret) {
        roce_err("The opcode %d is not supported in UD mode", wr->opcode);
        return ret;
    }

    hns_roce_u_v2_set_ud_wqe_common(wr, ud_sq_wqe, qp, nreq, sge_info);

    ud_sq_wqe->qkey = htole32(wr->wr.ud.remote_qkey);

    hns_roce_u_v2_set_ud_wqe_dest(wr, ud_sq_wqe, ah, qp);

    ret = memcpy_s(ud_sq_wqe->dgid, HNS_ROCE_GID_SIZE, ah->av.dgid, HNS_ROCE_GID_SIZE);
    if (ret) {
        roce_err("memcpy_s ud_sq_wqe dgid failed, ret %d", ret);
        return ret;
    }

    hns_roce_u_v2_get_sge_info(wr, sge_info);

    ud_sq_wqe->msg_len = htole32(sge_info->total_len);
    roce_set_field(ud_sq_wqe->sge_num_pd, UD_SQ_WQE_SGE_NUM_M, UD_SQ_WQE_SGE_NUM_S, sge_info->valid_num);

    if (wr->send_flags & IBV_SEND_INLINE) { /* set inline wqe */
        if (wr->num_sge &&
            to_hr_dev(qp->ibv_qp.context->device)->hw_version == HNS_ROCE_HW_VER3) {
            ret = hns_roce_u_v2_set_ud_inl(qp, wr, ud_sq_wqe, sge_info);
            if (ret) {
                roce_err("roce user set ud inline data wqe failed, ret %d!", ret);
                return ret;
            }
        }
    } else {
        /* set ud extend sge */
        hns_roce_u_v2_set_sge(dseg, qp, wr, sge_info);
    }

    return 0;
}

STATIC void hns_roce_u_v2_set_rc_wqe_common(const struct ibv_send_wr *wr,
                                            struct hns_roce_rc_sq_wqe *rc_sq_wqe,
                                            const struct hns_roce_qp *qp, int nreq,
                                            const struct hns_roce_sge_info *sge_info)
{
    roce_set_field(rc_sq_wqe->byte_20, RC_SQ_WQE_BYTE_20_MSG_START_SGE_IDX_S,
                   RC_SQ_WQE_BYTE_20_MSG_START_SGE_IDX_S, 0);

    roce_set_bit(rc_sq_wqe->byte_4, RC_SQ_WQE_BYTE_4_CQE_S,
                 (wr->send_flags & IBV_SEND_SIGNALED) ? 1 : 0);

    /* Set fence attr */
    roce_set_bit(rc_sq_wqe->byte_4, RC_SQ_WQE_BYTE_4_FENCE_S,
                 (wr->send_flags & IBV_SEND_FENCE) ? 1 : 0);

    /* Set solicited attr */
    roce_set_bit(rc_sq_wqe->byte_4, RC_SQ_WQE_BYTE_4_SE_S,
                 (wr->send_flags & IBV_SEND_SOLICITED) ? 1 : 0);

    roce_set_bit(rc_sq_wqe->byte_4, RC_SQ_WQE_BYTE_4_OWNER_S,
                 ~(((qp->sq.head + (unsigned int)nreq) >> qp->sq.shift) & 0x1));

    roce_set_field(rc_sq_wqe->byte_20,
                   RC_SQ_WQE_BYTE_20_MSG_START_SGE_IDX_M,
                   RC_SQ_WQE_BYTE_20_MSG_START_SGE_IDX_S,
                   (sge_info->start_idx) & (qp->sge.sge_cnt - 1));
}

STATIC int hns_roce_u_v3_fill_rc_inl_wqe(struct ibv_send_wr *wr,
                                         struct hns_roce_qp *qp,
                                         struct hns_roce_rc_sq_wqe *rc_sq_wqe,
                                         struct hns_roce_sge_info *sge_info,
                                         void *dseg)
{
    int i;
    int ret = 0;
    unsigned int sge_idx = sge_info->start_idx;

    if (sge_info->total_len > qp->max_inline_data) {
        roce_err("failed to inline, data len %u, max inline len %u!",
            sge_info->total_len, qp->max_inline_data);
        return -EINVAL;
    }

    roce_set_bit(rc_sq_wqe->byte_4, RC_SQ_WQE_BYTE_4_INLINE_S, 1);

    if (sge_info->total_len <= HNS_ROCE_MAX_RC_INL_INN_SZ) {
        roce_set_bit(rc_sq_wqe->byte_20, RC_SQ_WQE_BYTE_20_INLINE_TYPE_S, 0);

        for (i = 0; i < wr->num_sge; i++) {
            ret = memcpy_s(dseg, wr->sg_list[i].length,
                (void *)(uintptr_t)(wr->sg_list[i].addr),
                wr->sg_list[i].length);
            if (ret) {
                roce_err("memcpy_s rc inline wqe inner dseg failed! ret %d", ret);
                return ret;
            }

            dseg = wr->sg_list[i].length + (char *)dseg;
        }
    } else {
        roce_set_bit(rc_sq_wqe->byte_20, RC_SQ_WQE_BYTE_20_INLINE_TYPE_S, 1u);

        ret = hns_roce_u_v2_fill_ext_sge_inl_data(qp, wr, sge_info);
        if (ret) {
            roce_err("fill extend sge inline data failed, ret %d.", ret);
            return ret;
        }

        sge_info->valid_num = sge_info->start_idx - sge_idx;

        roce_set_field(rc_sq_wqe->byte_16, RC_SQ_WQE_BYTE_16_SGE_NUM_M,
                       RC_SQ_WQE_BYTE_16_SGE_NUM_S,
                       sge_info->valid_num);
    }

    return 0;
}

STATIC int hns_roce_u_v2_fill_rc_inl_wqe(struct ibv_send_wr *wr,
                                         const struct hns_roce_qp *qp,
                                         struct hns_roce_rc_sq_wqe *rc_sq_wqe,
                                         const struct hns_roce_sge_info *sge_info,
                                         void *dseg)
{
    int i;
    int ret = 0;

    if (sge_info->total_len > qp->max_inline_data) {
        roce_err("failed to inline, data len %u, max inline len %u!",
            sge_info->total_len, qp->max_inline_data);
        return -EINVAL;
    }

    for (i = 0; i < wr->num_sge; i++) {
        ret = memcpy_s(dseg, wr->sg_list[i].length,
            (void *)(uintptr_t)(wr->sg_list[i].addr),
            wr->sg_list[i].length);
        if (ret) {
            roce_err("memcpy_s rc inline wqe inner dseg failed! ret %d", ret);
            return ret;
        }

        dseg = wr->sg_list[i].length + (char *)dseg;
    }

    roce_set_bit(rc_sq_wqe->byte_4, RC_SQ_WQE_BYTE_4_INLINE_S, 1);

    return 0;
}
/*lint +e409*/
STATIC int hns_roce_u_v2_set_rc_inl_wqe(struct ibv_send_wr *wr,
                                        struct hns_roce_qp *qp,
                                        struct hns_roce_rc_sq_wqe *rc_sq_wqe,
                                        struct hns_roce_sge_info *sge_info)
{
    int ret = 0;
    void *dseg = rc_sq_wqe;

    dseg = (char *)dseg + sizeof(struct hns_roce_rc_sq_wqe);

    if (wr->opcode == IBV_WR_RDMA_READ) {
        roce_err("Not support rdma read inline data!");
        return -EINVAL;
    }

    if (to_hr_dev(qp->ibv_qp.context->device)->hw_version == HNS_ROCE_HW_VER3) {
        ret = hns_roce_u_v3_fill_rc_inl_wqe(wr, qp, rc_sq_wqe, sge_info, dseg);
        if (ret) {
            roce_err("hns_roce_u_v3_fill_rc_inl_wqe, ret %d", ret);
            return ret;
        }
    } else {
        ret = hns_roce_u_v2_fill_rc_inl_wqe(wr, qp, rc_sq_wqe, sge_info, dseg);
        if (ret) {
            roce_err("hns_roce_u_v2_fill_rc_inl_wqe, ret %d", ret);
            return ret;
        }
    }

    return 0;
}

STATIC int hns_roce_u_v2_set_rc_wqe(void *wqe, struct hns_roce_qp *qp,
                                    struct ibv_send_wr *wr, int nreq,
                                    struct hns_roce_sge_info *sge_info)
{
    int ret;
    struct hns_roce_rc_sq_wqe *rc_sq_wqe = NULL;
    struct hns_roce_v2_wqe_data_seg *dseg = NULL;

    rc_sq_wqe = wqe;
    ret = memset_s(rc_sq_wqe, sizeof(struct hns_roce_rc_sq_wqe),
                   0, sizeof(struct hns_roce_rc_sq_wqe));
    if (ret) {
        roce_err("memset_s rc_sq_wqe failed, ret %d", ret);
        return ret;
    }

    hns_roce_u_v2_set_rc_wqe_common(wr, rc_sq_wqe, qp, nreq, sge_info);

    /* set remote addr segment */
    if (hns_roce_u_v2_operation(wr, rc_sq_wqe)) {
        roce_err("hns_roce_u_v2_operation fail!");
        return -EINVAL;
    }

    wqe = (char *)wqe + sizeof(struct hns_roce_rc_sq_wqe);
    dseg = wqe;

    hns_roce_u_v2_get_sge_info(wr, sge_info);

    rc_sq_wqe->msg_len = htole32(sge_info->total_len);
    roce_set_field(rc_sq_wqe->byte_16, RC_SQ_WQE_BYTE_16_SGE_NUM_M,
                   RC_SQ_WQE_BYTE_16_SGE_NUM_S, sge_info->valid_num);

    if (wr->send_flags & IBV_SEND_INLINE) { /* set inline data */
        if (wr->num_sge) {
            ret = hns_roce_u_v2_set_rc_inl_wqe(wr, qp, rc_sq_wqe, sge_info);
            if (ret) {
                roce_err("roce user set rc inline data wqe failed, ret %d!", ret);
                return ret;
            }
        }
    } else {
        /* set rc inner & extend sge */
        hns_roce_u_v2_set_sge(dseg, qp, wr, sge_info);
    }

    return 0;
}

STATIC int hns_roce_u_v2_check_wr(const struct ibv_send_wr *wr, int nreq, struct hns_roce_qp *qp)
{
    if (hns_roce_v2_wq_overflow(&qp->sq, nreq, to_hr_cq(qp->ibv_qp.send_cq))) {
        roce_warn("wq overflow!");
        return -ENOMEM;
    }

    if ((unsigned int)wr->num_sge > qp->sq.max_gs) {
        roce_err("Num of sge(%d) in wr larger than qp's capability(%d)!",
                 wr->num_sge, qp->sq.max_gs);
        return -EINVAL;
    }

    return 0;
}

STATIC int hns_roce_u_v2_gdr_check_wr(struct ibv_send_wr *wr, struct hns_roce_qp *qp, int nreq)
{
    int ret;

    if (wr->send_flags & IBV_SEND_INLINE) {
        roce_err("gdr not support inline data!");
        return -EINVAL;
    }

    if (wr->next != NULL) {
        roce_err("gdr not support wr list mode!");
        return -EINVAL;
    }

    ret = hns_roce_u_v2_check_wr(wr, nreq, qp);
    if (ret) {
        roce_warn("hns_roce_u_v2_check_wr failed, ret %d", ret);
        return ret;
    }

    return 0;
}

/* set owner bit in tmpelate wqe */
STATIC int hns_roce_u_v2_gdr_fill_share_wqe(void *share_wqe, struct hns_roce_qp *qp,
                                            struct hns_roce_context *ctx)
{
    int ret;
    unsigned int rc_len, seg_len, wqe_len;
    unsigned int wqe_byte_4_val;

    rc_len = sizeof(struct hns_roce_rc_sq_wqe);
    seg_len = sizeof(struct hns_roce_v2_wqe_data_seg);
    wqe_len = rc_len + MAX_INNER_SEG_CNT * seg_len;

    ret = memcpy_s((void *)((char *)share_wqe + wqe_len), wqe_len, share_wqe, wqe_len);
    if (ret) {
        roce_err("memcpy_s share_wqe failed! ret %d", ret);
        return ret;
    }

    wqe_byte_4_val = ((struct hns_roce_rc_sq_wqe *)share_wqe)->byte_4;

    roce_set_bit(((struct hns_roce_rc_sq_wqe *)((char *)share_wqe + wqe_len))->byte_4,
                 RC_SQ_WQE_BYTE_4_OWNER_S,
                 ~((wqe_byte_4_val >> RC_SQ_WQE_BYTE_4_OWNER_S) & 0x1));

    /* fill share mem doorbell info */
    qp->sq.head++;
    hns_roce_get_sq_db(qp->ibv_qp.qp_num,
                       qp->sl, qp->sq.head & ((qp->sq.wqe_cnt << 1) - 1),
                       &qp->peer_ctrl_db);
    roce_dbg("peer_ctrl_db= %ld", qp->peer_ctrl_db);

    ret = memcpy_s((void *)((char *)ctx->share_buffer_base + qp->gdr_share_sq.db_offset),
                   sizeof(qp->peer_ctrl_db), &qp->peer_ctrl_db,
                   sizeof(qp->peer_ctrl_db));
    if (ret) {
        roce_err("memcpy_s gdr share sq db failed! ret = %d", ret);
        return ret;
    }

    return 0;
}

STATIC int hns_roce_u_v2_gdr_post_send(struct hns_roce_qp *qp, unsigned int *wqe_index,
                                       struct ibv_send_wr *wr,
                                       struct hns_roce_context *ctx,
                                       struct hns_roce_sge_info *sge_info)
{
    int ret;
    int nreq = 0; /* gdr only support post send one wr each time */
    void *share_wqe = NULL;

    if (qp->ibv_qp.qp_type != IBV_QPT_RC) {
        roce_err("gdr only support RC mode!");
        return -EINVAL;
    }

    ret = hns_roce_u_v2_gdr_check_wr(wr, qp, nreq);
    if (ret) {
        roce_err("hns_roce_u_v2_gdr_check_wr fail, ret %d", ret);
        return ret;
    }

    ret = hns_roce_alloc_gdr_template_wqe(qp, wqe_index);
    if (ret) {
        roce_err("hns_roce_alloc_gdr_template_wqe fail, ret %d", ret);
        return ret;
    }

    share_wqe = ((char *)ctx->share_buffer_base +
                qp->gdr_temp_wqe.temp_offset +
                (unsigned int)(*wqe_index * IBV_EXP_TEMP_WQE_ENTRY_LENGTH));

    ret = hns_roce_u_v2_set_rc_wqe(share_wqe, qp, wr, nreq, sge_info);
    if (ret) {
        roce_err("gdr set rc wqe fail, ret %d", ret);
        goto out;
    }

    ret = hns_roce_u_v2_gdr_fill_share_wqe(share_wqe, qp, ctx);
    if (ret) {
        roce_err("hns roce user gdr fill share wqe fail, ret %d", ret);
        goto out;
    }

    return 0;

out:
    /* set bitmap[i] to one so it can be used next time */
    hns_roce_dealloc_gdr_template_wqe(qp, *wqe_index);

    return ret;
}

STATIC int hns_roce_u_v2_check_qp_state(const struct ibv_qp *ibvqp)
{
    if (ibvqp->state == IBV_QPS_RESET || ibvqp->state == IBV_QPS_INIT ||
        ibvqp->state == IBV_QPS_RTR) {
        roce_err("check qp state fail! qp state %d", ibvqp->state);
        return -EINVAL;
    }

    return 0;
}

STATIC void hns_roce_u_v2_notify_send_qp(struct hns_roce_qp *qp, int nreq,
                                         struct hns_roce_sge_info *sge_info,
                                         struct hns_roce_context *ctx)
{
    if (likely(nreq) && qp->gdr_enabled == HNS_ROCE_QP_MODE_NOR) {
            qp->sq.head += nreq;
            hns_roce_update_sq_db(ctx, qp->ibv_qp.qp_num, qp->sl,
                                  qp->sq.head & ((qp->sq.wqe_cnt << 1) - 1),
                                  qp->gdr_enabled);

            qp->next_sge = sge_info->start_idx;
            *(qp->sdb) = qp->sq.head & 0xffff;
    } else if (likely(nreq)) {
        qp->sq.head += nreq;
    }
}

STATIC void hns_roce_u_v2_handle_err_qp(struct ibv_qp *ibvqp)
{
    int attr_mask;
    struct ibv_qp_attr attr;

    if (ibvqp->state == IBV_QPS_ERR) {
        attr_mask = IBV_QP_STATE;
        attr.qp_state = IBV_QPS_ERR;

        /* Exception handling here, no need to return val */
        if (hns_roce_u_v2_modify_qp(ibvqp, &attr, attr_mask)) {
            roce_err("modify qp failed!");
        }
    }
}

STATIC void hns_roce_u_v2_gdr_exp_rsp(const struct hns_roce_qp *qp, unsigned int gdr_wqe_idx,
                                      struct wr_exp_rsp *exp_rsp)
{
    unsigned int pi = qp->sq.head & ((qp->sq.wqe_cnt << 1) - 1);
    if (qp->gdr_enabled == HNS_ROCE_QP_MODE_GDR) {
        exp_rsp->wqe_index = gdr_wqe_idx;
    } else if (qp->gdr_enabled == HNS_ROCE_QP_MODE_OP) {
        exp_rsp->db_info = ((unsigned long)pi << DB_PI_OFFSET) |
                            (0 << DB_QPN_OFFSET) | qp->ibv_qp.qp_num;
    }
}

STATIC int hns_roce_u_v2_set_wqe(void *wqe, struct hns_roce_qp *qp,
                                 struct ibv_send_wr *wr, int nreq,
                                 struct hns_roce_sge_info *sge_info)
{
    int ret = 0;

    switch (qp->ibv_qp.qp_type) {
        case IBV_QPT_UD:
            ret = hns_roce_u_v2_set_ud_wqe(wqe, qp, wr, nreq, sge_info);
            break;
        case IBV_QPT_RC:
            ret = hns_roce_u_v2_set_rc_wqe(wqe, qp, wr, nreq, sge_info);
            break;
        default:
            roce_err("Not supported qp type %d", qp->ibv_qp.qp_type);
            return -EINVAL;
    }

    return ret;
}

STATIC int _hns_roce_u_v2_post_send(struct ibv_qp *ibvqp, struct ibv_send_wr *wr,
                                    struct ibv_send_wr **bad_wr, struct wr_exp_rsp *exp_rsp)
{
    int nreq = 0;
    int ret = 0;
    unsigned int wqe_idx = 0;
    unsigned int gdr_wqe_idx = 0;
    void *wqe = NULL;
    struct hns_roce_qp *qp = to_hr_qp(ibvqp);
    struct hns_roce_context *ctx = to_hr_ctx(ibvqp->context);
    struct hns_roce_sge_info sge_info = {0};

    /* check qp state is OK to post send */
    if (hns_roce_u_v2_check_qp_state(ibvqp)) {
        *bad_wr = wr;
        return -EINVAL;
    }

    pthread_spin_lock(&qp->sq.lock);
    sge_info.start_idx = qp->next_sge; /* start index of extend sge */

    if (qp->gdr_enabled == HNS_ROCE_QP_MODE_GDR) {
        ret = hns_roce_u_v2_gdr_post_send(qp, &gdr_wqe_idx, wr, ctx, &sge_info);
        if (ret) {
            *bad_wr = wr;
            goto gdr_out;
        }
    } else {
        for (nreq = 0; wr; ++nreq, wr = wr->next) {
            ret = hns_roce_u_v2_check_wr(wr, nreq, qp);
            if (ret) {
                *bad_wr = wr;
                goto out;
            }

            wqe_idx = (qp->sq.head + (unsigned int)nreq) & (qp->sq.wqe_cnt - 1);
            wqe = get_send_wqe(qp, wqe_idx);
            qp->sq.wrid[wqe_idx] = wr->wr_id;

            ret = hns_roce_u_v2_set_wqe(wqe, qp, wr, nreq, &sge_info);
            if (ret) {
                *bad_wr = wr;
                goto out;
            }
        }
    }

out:
    hns_roce_u_v2_notify_send_qp(qp, nreq, &sge_info, ctx);

gdr_out:
    pthread_spin_unlock(&qp->sq.lock);

    hns_roce_u_v2_handle_err_qp(ibvqp);

    hns_roce_u_v2_gdr_exp_rsp(qp, gdr_wqe_idx, exp_rsp);

    return ret;
}

int hns_roce_u_v2_post_send(struct ibv_qp *ibvqp, struct ibv_send_wr *wr,
                            struct ibv_send_wr **bad_wr)
{
    struct wr_exp_rsp exp_rsp = {0};

    if (ibvqp == NULL) {
        roce_err("Input parameter error: ibv_qp is NULL!");
        return -EINVAL;
    }

    return _hns_roce_u_v2_post_send(ibvqp, wr, bad_wr, &exp_rsp);
}

// gdr post send
int hns_roce_u_v2_exp_post_send(struct ibv_qp *ibvqp, struct ibv_send_wr *wr,
                                struct ibv_send_wr **bad_wr, struct wr_exp_rsp *exp_rsp)
{
    if (ibvqp == NULL || exp_rsp == NULL) {
        roce_err("Input parameter error: ibv_qp or exp_rsp is NULL!");
        return -EINVAL;
    }

    return _hns_roce_u_v2_post_send(ibvqp, wr, bad_wr, exp_rsp);
}
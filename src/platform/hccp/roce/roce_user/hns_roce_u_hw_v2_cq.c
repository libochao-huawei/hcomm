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
#include <malloc.h>
#include "securec.h"
#include "hns_roce_u.h"
#include "hns_roce_u_db.h"
#include "hns_roce_u_sec.h"
#include "hns_roce_u_abi.h"
#include "hns_roce_u_hw_v2.h"
#include "hns_roce_u_hw_v2_qp.h"
#include "hns_roce_u_hw_v2_opreation.h"
#include "hns_roce_u_cmd.h"
#include "hns_roce_u_hw_v2_cq.h"

#ifdef DEFINE_HNS_LLT
#undef static
#define static
#endif

struct cqe_wc_status g_cqe_wc_array[] = {
    {HNS_ROCE_V2_CQE_LOCAL_LENGTH_ERR, IBV_WC_LOC_LEN_ERR},
    {HNS_ROCE_V2_CQE_LOCAL_QP_OP_ERR, IBV_WC_LOC_QP_OP_ERR},
    {HNS_ROCE_V2_CQE_LOCAL_PROT_ERR, IBV_WC_LOC_PROT_ERR},
    {HNS_ROCE_V2_CQE_WR_FLUSH_ERR, IBV_WC_WR_FLUSH_ERR},
    {HNS_ROCE_V2_CQE_MEM_MANAGERENT_OP_ERR, IBV_WC_MW_BIND_ERR},
    {HNS_ROCE_V2_CQE_BAD_RESP_ERR, IBV_WC_BAD_RESP_ERR},
    {HNS_ROCE_V2_CQE_LOCAL_ACCESS_ERR, IBV_WC_LOC_ACCESS_ERR},
    {HNS_ROCE_V2_CQE_REMOTE_INVAL_REQ_ERR, IBV_WC_REM_INV_REQ_ERR},
    {HNS_ROCE_V2_CQE_REMOTE_ACCESS_ERR, IBV_WC_REM_ACCESS_ERR},
    {HNS_ROCE_V2_CQE_REMOTE_OP_ERR, IBV_WC_REM_OP_ERR},
    {HNS_ROCE_V2_CQE_TRANSPORT_RETRY_EXC_ERR, IBV_WC_RETRY_EXC_ERR},
    {HNS_ROCE_V2_CQE_RNR_RETRY_EXC_ERR, IBV_WC_RNR_RETRY_EXC_ERR},
    {HNS_ROCE_V2_CQE_REMOTE_ABORTED_ERR, IBV_WC_REM_ABORT_ERR},
};

static void hns_roce_v2_handle_error_cqe(struct hns_roce_v2_cqe *cqe,
    struct ibv_wc *wc)
{
    int i;
    unsigned int status = roce_get_field(cqe->byte_4, CQE_BYTE_4_STATUS_M,
                                         CQE_BYTE_4_STATUS_S);
    unsigned int cqe_status = status & HNS_ROCE_V2_CQE_STATUS_MASK;
    int array_num = sizeof(g_cqe_wc_array) / sizeof(struct cqe_wc_status);

    for (i = 0; i < array_num; i++) {
        if (cqe_status == g_cqe_wc_array[i].cqe_status) {
            wc->status = g_cqe_wc_array[i].wc_status;
            break;
        }
    }

    if (i == array_num) {
        wc->status = IBV_WC_GENERAL_ERR;
    }

    if (wc->status != IBV_WC_WR_FLUSH_ERR) {
        roce_err("error cqe status: 0x%x", cqe_status);
    }
}

static void hns_roce_free_srq_wqe(struct hns_roce_srq *srq, unsigned int ind)
{
    uint32_t bitmap_num;
    uint32_t bit_num;

    pthread_spin_lock(&srq->lock);
    bitmap_num = ind / (sizeof(uint64_t) * BIT_CNT_PER_BYTE);
    bit_num = ind % (sizeof(uint64_t) * BIT_CNT_PER_BYTE);
    if (bitmap_num < (unsigned int)srq->idx_que.bitmap_len) {
        srq->idx_que.bitmap[bitmap_num] |= (1ULL << bit_num);
        srq->tail++;
    } else {
        roce_err("get invalid bitmap num [%u], the bitmap len is [%d]", bitmap_num, srq->idx_que.bitmap_len);
    }

    pthread_spin_unlock(&srq->lock);
}

STATIC void dump_err_cqe_v2(struct hns_roce_qp *qp, const struct hns_roce_cq *cq, const uint32_t *cqe,
    uint32_t wc_status)
{
#define CQE_ELEMENT_EIGHT 8
    int index;
    if (wc_status != IBV_WC_WR_FLUSH_ERR) {
        for (index = 0; index < CQE_ELEMENT_EIGHT; index++) {
            roce_err("CQ(0x%lx) CQE(0x%x) INDEX(0x%08x): 0x%08x", cq->cqn, cq->cons_index, index, *(cqe +
                index));
        }
        hns_roce_u_v2_printf_dfx(qp, HNS_ROCE_USER_POLL_CQE);
    }
}

static int hns_roce_flush_cqe(struct hns_roce_qp **cur_qp, const struct ibv_wc *wc)
{
    struct ibv_qp_attr attr;
    int attr_mask;
    int ret;

    if ((wc->status != IBV_WC_SUCCESS) &&
            (wc->status != IBV_WC_WR_FLUSH_ERR)) {
        attr_mask = IBV_QP_STATE;
        attr.qp_state = IBV_QPS_ERR;
        ret = hns_roce_u_v2_modify_qp(&(*cur_qp)->ibv_qp,
                                      &attr, attr_mask);
        if (ret) {
            roce_err("failed to modify qp, ret [%d], expect 0", ret);
        }
        (*cur_qp)->ibv_qp.state = IBV_QPS_ERR;
    }

    return V2_CQ_OK;
}

static void hns_roce_v2_get_opcode_from_sender(struct hns_roce_v2_cqe *cqe, struct ibv_wc *wc)
{
    /* Get opcode and flag before update the tail point for send */
    switch (roce_get_field(cqe->byte_4, CQE_BYTE_4_OPCODE_M, CQE_BYTE_4_OPCODE_S) & HNS_ROCE_V2_CQE_OPCODE_MASK) {
        case HNS_ROCE_SQ_OP_SEND:
            wc->opcode = IBV_WC_SEND;
            goto wc_handle_and_out;
        case HNS_ROCE_SQ_OP_SEND_WITH_IMM:
            wc->opcode = IBV_WC_SEND;
            wc->wc_flags = IBV_WC_WITH_IMM;
            break;
        case HNS_ROCE_SQ_OP_SEND_WITH_INV:
            wc->opcode = IBV_WC_SEND;
            break;
        case HNS_ROCE_SQ_OP_RDMA_READ:
            wc->opcode = IBV_WC_RDMA_READ;
            wc->byte_len = le32toh(cqe->byte_cnt);
            goto wc_handle_and_out;
        case HNS_ROCE_SQ_OP_RDMA_WRITE:
            wc->opcode = IBV_WC_RDMA_WRITE;
            goto wc_handle_and_out;
        case HNS_ROCE_SQ_OP_RDMA_WRITE_WITH_IMM:
            wc->opcode = IBV_WC_RDMA_WRITE;
            wc->wc_flags = IBV_WC_WITH_IMM;
            break;
        case HNS_ROCE_SQ_OP_LOCAL_INV:
            wc->opcode = IBV_WC_LOCAL_INV;
            wc->wc_flags = IBV_WC_WITH_INV;
            break;
        case HNS_ROCE_SQ_OP_ATOMIC_COMP_AND_SWAP:
            wc->opcode = IBV_WC_COMP_SWAP;
            wc->byte_len  = BYTE_LEN;
            goto wc_handle_and_out;
        case HNS_ROCE_SQ_OP_ATOMIC_FETCH_AND_ADD:
            wc->opcode = IBV_WC_FETCH_ADD;
            wc->byte_len  = BYTE_LEN;
            goto wc_handle_and_out;
        case HNS_ROCE_SQ_OP_BIND_MW:
            wc->opcode = IBV_WC_BIND_MW;
            goto wc_handle_and_out;
        default:
            wc->status = IBV_WC_GENERAL_ERR;
            goto wc_handle_and_out;
    }
wc_handle_and_out:
    wc->wc_flags = 0;
    return;
}

static void hns_roce_v2_get_opcode_from_responder(struct hns_roce_v2_cqe *cqe,
    struct ibv_wc *wc,
    uint32_t opcode)
{
    switch (opcode) {
        case HNS_ROCE_RECV_OP_RDMA_WRITE_IMM:
            wc->opcode = IBV_WC_RECV_RDMA_WITH_IMM;
            wc->wc_flags = IBV_WC_WITH_IMM;
            wc->imm_data = htobe32(le32toh(cqe->immtdata));
            break;
        case HNS_ROCE_RECV_OP_SEND:
            wc->opcode = IBV_WC_RECV;
            wc->wc_flags = 0;
            break;
        case HNS_ROCE_RECV_OP_SEND_WITH_IMM:
            wc->opcode = IBV_WC_RECV;
            wc->wc_flags = IBV_WC_WITH_IMM;
            wc->imm_data = htobe32(le32toh(cqe->immtdata));
            break;
        case HNS_ROCE_RECV_OP_SEND_WITH_INV:
            wc->opcode = IBV_WC_RECV;
            wc->wc_flags = IBV_WC_WITH_INV;
            wc->invalidated_rkey = le32toh(cqe->rkey);
            break;
        default:
            wc->status = IBV_WC_GENERAL_ERR;
            break;
    }
}

static int hns_roce_handle_recv_inl_wqe(struct hns_roce_v2_cqe *cqe,
    struct hns_roce_qp **cur_qp,
    struct ibv_wc *wc, uint32_t opcode)
{
    int ret;
    if (((*cur_qp)->ibv_qp.qp_type == IBV_QPT_RC ||
        (*cur_qp)->ibv_qp.qp_type == IBV_QPT_UC) &&
        (opcode == HNS_ROCE_RECV_OP_SEND ||
        opcode == HNS_ROCE_RECV_OP_SEND_WITH_IMM ||
        opcode == HNS_ROCE_RECV_OP_SEND_WITH_INV) &&
            (roce_get_bit(cqe->byte_4, CQE_BYTE_4_RQ_INLINE_S))) {
        struct hns_roce_rinl_sge *sge_list;
        uint32_t wr_num, wr_cnt, sge_num, data_len;
        uint8_t *wqe_buf;
        uint32_t sge_cnt, size;

        wr_num = (uint16_t)roce_get_field(cqe->byte_4,
            CQE_BYTE_4_WQE_IDX_M,
            CQE_BYTE_4_WQE_IDX_S) & 0xffff;
        wr_cnt = wr_num & ((*cur_qp)->rq.wqe_cnt - 1);

        sge_list = (*cur_qp)->rq_rinl_buf.wqe_list[wr_cnt].sg_list;
        sge_num = (*cur_qp)->rq_rinl_buf.wqe_list[wr_cnt].sge_cnt;
        wqe_buf = (uint8_t *)get_recv_wqe_v2(*cur_qp, wr_cnt);
        if (wqe_buf == NULL) {
            roce_err("can't get recv inl wqe !");
            return -EINVAL;
        }

        data_len = wc->byte_len;

        for (sge_cnt = 0; (sge_cnt < sge_num) && (data_len); sge_cnt++) {
            size = sge_list[sge_cnt].len < data_len ? sge_list[sge_cnt].len : data_len;

            ret = memcpy_s((void *)sge_list[sge_cnt].addr, sge_list[sge_cnt].len,
                (void *)wqe_buf, size);
            HNS_ROCE_U_SEC_CHECK_RET_INT(ret);

            data_len -= size;
            wqe_buf += size;
        }

        if (data_len) {
            wc->status = IBV_WC_LOC_LEN_ERR;
            return V2_CQ_POLL_ERR;
        }
    }

    return V2_CQ_OK;
}

static struct hns_roce_qp *hns_roce_v2_find_qp(struct hns_roce_context *ctx,
    uint32_t qpn)
{
    int tind = (qpn & ((unsigned int)ctx->num_qps - 1)) >> (unsigned int)ctx->qp_table_shift;

    if (tind < HNS_ROCE_QP_TABLE_SIZE) {
        if (ctx->qp_table[tind].refcnt) {
            return ctx->qp_table[tind].table[qpn & (uint32_t)(ctx->qp_table_mask)];
        } else {
            return NULL;
        }
    }
    return NULL;
}

STATIC int hns_roce_v2_poll_one_mark_opcode(int is_send, struct hns_roce_v2_cqe *cqe, struct ibv_wc *wc,
    struct hns_roce_qp **cur_qp)
{
    int ret;
    uint32_t opcode;
    if (is_send) {
        hns_roce_v2_get_opcode_from_sender(cqe, wc);
    } else {
        /* Get opcode and flag in rq&srq */
        wc->byte_len = le32toh(cqe->byte_cnt);
        opcode = roce_get_field(cqe->byte_4, CQE_BYTE_4_OPCODE_M,
            CQE_BYTE_4_OPCODE_S) & HNS_ROCE_V2_CQE_OPCODE_MASK;
        hns_roce_v2_get_opcode_from_responder(cqe, wc, opcode);

        ret = hns_roce_handle_recv_inl_wqe(cqe, cur_qp, wc, opcode);
        if (ret) {
            roce_err("failed to handle recv inline wqe!, ret [%d], expect 0", ret);
            return ret;
        }
    }
    return 0;
}

STATIC void hns_roce_v2_poll_one_set_wc(struct ibv_wc *wc, uint32_t qpn, struct hns_roce_qp **cur_qp,
    int is_send, struct hns_roce_v2_cqe *cqe)
{
    uint16_t wqe_ctr;
    struct hns_roce_srq *srq = NULL;
    struct hns_roce_wq *wq = NULL;

    wc->qp_num = qpn & 0xffffff;

    srq = (*cur_qp)->ibv_qp.srq ? to_hr_srq((*cur_qp)->ibv_qp.srq) : NULL;
    if (is_send) {
        wq = &(*cur_qp)->sq;
        /*
         * if sq_signal_bits is 1, the tail pointer first update to
         * the wqe corresponding the current cqe
         */
        if ((*cur_qp)->sq_signal_bits) {
            wqe_ctr = (uint16_t)(roce_get_field(cqe->byte_4,
                                                CQE_BYTE_4_WQE_IDX_M,
                                                CQE_BYTE_4_WQE_IDX_S));
            /*
             * wq->tail will plus a positive number every time,
             * when wq->tail exceeds 32b, it is 0 and acc
             */
            wq->tail += (wqe_ctr - (uint16_t) wq->tail) &
                        (wq->wqe_cnt - 1);
        }
        /* write the wr_id of wq into the wc */
        wc->wr_id = wq->wrid[wq->tail & (wq->wqe_cnt - 1)];
        ++wq->tail;
    } else if (srq != NULL) {
        wqe_ctr = (uint16_t)(roce_get_field(cqe->byte_4,
                                            CQE_BYTE_4_WQE_IDX_M,
                                            CQE_BYTE_4_WQE_IDX_S));
        if (wqe_ctr < srq->max) {
            wc->wr_id = srq->wrid[wqe_ctr];
        }

        hns_roce_free_srq_wqe(srq, wqe_ctr);
    } else {
        wq = &(*cur_qp)->rq;
        wc->wr_id = wq->wrid[wq->tail & (wq->wqe_cnt - 1)];
        ++wq->tail;
    }
}

STATIC void dump_err_qpc_v2(const struct ibv_context *ibv_ctx, uint32_t wc_status, uint32_t qpn)
{
    struct hns_roce_qpc_stat qpc_stat = {0};
    char *qpc_info_ptr = NULL;
    char *next_token = NULL;
    int ret;

    if (wc_status == IBV_WC_WR_FLUSH_ERR) {
        return;
    }

    ret = snprintf_s(qpc_stat.index, INDEX_LEN, INDEX_LEN - 1, "%u", qpn);
    if (ret <= 0) {
        roce_err("sprintf context err %d, expect bigger than 0", ret);
        return;
    }

    ret = hns_roce_u_get_roce_qpc_stat(ibv_ctx, &qpc_stat);
    if (ret) {
        roce_err("hns roce get roce qpc stat failed, ret = %d", ret);
        return;
    }

    qpc_info_ptr = strtok_s(qpc_stat.info, "\n", &next_token);
    while (qpc_info_ptr) {
        roce_err("%s", qpc_info_ptr);
        qpc_info_ptr = strtok_s(NULL, "\n", &next_token);
    }

    return;
}

static int hns_roce_v2_poll_one(struct hns_roce_cq *cq, struct hns_roce_qp **cur_qp, struct ibv_wc *wc)
{
    uint32_t qpn;
    int is_send;
    uint32_t local_qpn;
    struct hns_roce_v2_cqe *cqe = NULL;
    int ret;

    /* According to CI, find the relative cqe */
    cqe = next_cqe_sw_v2(cq);
    if (cqe == NULL) {
        /* This is normal, don't need record log. */
        return V2_CQ_EMPTY;
    }

    /* Get the next cqe, CI will be added gradually */
    ++cq->cons_index;

    udma_from_device_barrier();

    qpn = roce_get_field(cqe->byte_16, CQE_BYTE_16_LCL_QPN_M,
                         CQE_BYTE_16_LCL_QPN_S);

    is_send = (roce_get_bit(cqe->byte_4, CQE_BYTE_4_S_R_S) ==
               HNS_ROCE_V2_CQE_IS_SQ);

    local_qpn = roce_get_field(cqe->byte_16, CQE_BYTE_16_LCL_QPN_M,
                               CQE_BYTE_16_LCL_QPN_S);
    /* if qp is zero, it will not get the correct qpn */
    if (*cur_qp == NULL ||
            (local_qpn & HNS_ROCE_V2_CQE_QPN_MASK) != (*cur_qp)->ibv_qp.qp_num) {
        *cur_qp = hns_roce_v2_find_qp(to_hr_ctx(cq->verbs_cq.cq.context),
                                      qpn & 0xffffff);
        if (*cur_qp == NULL) {
            roce_err("can't find qp!");
            return V2_CQ_POLL_ERR;
        }
    }

    hns_roce_v2_poll_one_set_wc(wc, qpn, cur_qp, is_send, cqe);

    /*
     * HW maintains wc status, set the err type and directly return, after
     * generated the incorrect CQE
     */
    if (roce_get_field(cqe->byte_4, CQE_BYTE_4_STATUS_M,
                       CQE_BYTE_4_STATUS_S) != HNS_ROCE_V2_CQE_SUCCESS) {
        hns_roce_v2_handle_error_cqe(cqe, wc);
        dump_err_cqe_v2(*cur_qp, cq, (uint32_t *)cqe, wc->status);
        dump_err_qpc_v2(cq->verbs_cq.cq.context, wc->status, qpn);
        return hns_roce_flush_cqe(cur_qp, wc);
    }

    wc->status = IBV_WC_SUCCESS;

    /*
     * According to the opcode type of cqe, mark the opcode and other
     * information of wc
     */
    ret = hns_roce_v2_poll_one_mark_opcode(is_send, cqe, wc, cur_qp);
    if (ret) {
        return ret;
    }

    return V2_CQ_OK;
}

int hns_roce_u_v2_poll_cq(struct ibv_cq *ibvcq, int ne, struct ibv_wc *wc)
{
    int npolled;
    int err = V2_CQ_OK;
    struct hns_roce_qp *qp = NULL;
    struct hns_roce_cq *cq = NULL;
    struct hns_roce_context *ctx = NULL;

    HNS_ROCE_U_NULL_POINT_RETURN_ERR(ibvcq);
    HNS_ROCE_U_NULL_POINT_RETURN_ERR(wc);
    cq = to_hr_cq(ibvcq);
    ctx = to_hr_ctx(ibvcq->context);
    pthread_spin_lock(&cq->lock);

    for (npolled = 0; npolled < ne; ++npolled) {
        err = hns_roce_v2_poll_one(cq, &qp, wc + npolled);
        if (err != V2_CQ_OK) {
            break;
        }
    }

    if (npolled) {
        mmio_ordered_writes_hack();

        if (cq->flags & HNS_ROCE_SUPPORT_CQ_RECORD_DB) {
            *cq->set_ci_db = (cq->cons_index &
                              ((cq->cq_depth << 1U) - 1));
        } else {
            hns_roce_v2_update_cq_cons_index(ctx, cq);
        }
    }

    pthread_spin_unlock(&cq->lock);

    return err == V2_CQ_POLL_ERR ? err : npolled;
}

int hns_roce_u_v2_arm_cq(struct ibv_cq *ibvcq, int solicited)
{
    uint32_t ci;
    uint32_t cmd_sn;
    uint32_t solicited_flag;
    struct hns_roce_db cq_db;
    struct hns_roce_cq *cq = NULL;

    HNS_ROCE_U_NULL_POINT_RETURN_ERR(ibvcq);
    cq = to_hr_cq(ibvcq);
    ci  = cq->cons_index & ((cq->cq_depth << 1U) - 1);
    cmd_sn = ((unsigned int)(cq->arm_sn)) & HNS_ROCE_CMDSN_MASK;
    solicited_flag = solicited ? HNS_ROCE_V2_CQ_DB_REQ_SOL :
                     HNS_ROCE_V2_CQ_DB_REQ_NEXT;

    cq_db.byte_4 = 0;
    cq_db.parameter = 0;

    roce_set_field(cq_db.byte_4, DB_BYTE_4_TAG_M, DB_BYTE_4_TAG_S, cq->cqn);
    roce_set_field(cq_db.byte_4, DB_BYTE_4_CMD_M, DB_BYTE_4_CMD_S,
                   HNS_ROCE_V2_CQ_DB_NTR);

    roce_set_field(cq_db.parameter, DB_PARAM_CQ_CONSUMER_IDX_M,
                   DB_PARAM_CQ_CONSUMER_IDX_S, ci);

    roce_set_field(cq_db.parameter, DB_PARAM_CQ_CMD_SN_M,
                   DB_PARAM_CQ_CMD_SN_S, cmd_sn);
    roce_set_bit(cq_db.parameter, DB_PARAM_CQ_NOTIFY_S, solicited_flag);

    hns_roce_write64((uint32_t *)&cq_db, to_hr_ctx(ibvcq->context),
                     ROCEE_VF_DB_CFG0_OFFSET);
    return 0;
}

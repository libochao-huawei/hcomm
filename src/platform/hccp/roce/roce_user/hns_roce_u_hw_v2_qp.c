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
#include "hns_roce_u.h"
#include "hns_roce_u_abi.h"
#include "hns_roce_u_db.h"
#include "hns_roce_u_hw_v2.h"
#include "hns_roce_u_sec.h"
#include "securec.h"
#include "hns_roce_u_hw_v2_opreation.h"
#include "hns_roce_u_hw_v2_qp.h"

#ifdef DEFINE_HNS_LLT
#undef static
#define static
#endif

static void __hns_roce_v2_cq_clean(struct hns_roce_cq *cq, uint32_t qpn,
    struct hns_roce_srq *srq)
{
    int ret;
    int nfreed = 0;
    uint32_t prod_index;
    uint8_t owner_bit = 0;
    struct hns_roce_v2_cqe *cqe = NULL;
    struct hns_roce_v2_cqe *dest = NULL;
    struct hns_roce_context *ctx = to_hr_ctx(cq->verbs_cq.cq.context);

    for (prod_index = cq->cons_index; get_sw_cqe_v2(cq, prod_index);
            ++prod_index) {
        if (prod_index == cq->cons_index + cq->verbs_cq.cq.cqe) {
            break;
        }
    }

    while ((int) --prod_index - (int) cq->cons_index >= 0) {
        cqe = get_cqe_v2(cq, prod_index & (unsigned int)cq->verbs_cq.cq.cqe);
        if ((roce_get_field(cqe->byte_16, CQE_BYTE_16_LCL_QPN_M,
                            CQE_BYTE_16_LCL_QPN_S) & 0xffffff) == qpn) {
            ++nfreed;
        } else if (nfreed) {
#ifndef DEFINE_HNS_LLT
            dest = get_cqe_v2(cq,
                              (prod_index + (unsigned int)nfreed) & (unsigned int)cq->verbs_cq.cq.cqe);
#endif

            owner_bit = roce_get_bit(dest->byte_4,
                                     CQE_BYTE_4_OWNER_S);
#ifndef DEFINE_HNS_LLT
            ret = memcpy_s(dest, sizeof(*dest), cqe, sizeof(*cqe));
            HNS_ROCE_U_SEC_CHECK_RET_VOID(ret);
#endif
            roce_set_bit(dest->byte_4, CQE_BYTE_4_OWNER_S,
                         owner_bit);
        }
    }

    if (nfreed) {
        cq->cons_index += nfreed;
        udma_to_device_barrier();
        hns_roce_v2_update_cq_cons_index(ctx, cq);
    }
}

static void hns_roce_v2_cq_clean(struct hns_roce_cq *cq, unsigned int qpn,
    struct hns_roce_srq *srq)
{
    pthread_spin_lock(&cq->lock);
    __hns_roce_v2_cq_clean(cq, qpn, srq);
    pthread_spin_unlock(&cq->lock);
}

int hns_roce_u_v2_modify_qp(struct ibv_qp *qp, struct ibv_qp_attr *attr,
    int attr_mask)
{
    int ret;
    struct hns_roce_modify_qp cmd;
    struct hns_roce_qp *hr_qp = NULL;
    bool flag = false; /* modify qp to error */

    HNS_ROCE_U_NULL_POINT_RETURN_ERR(qp);
    HNS_ROCE_U_NULL_POINT_RETURN_ERR(attr);
    hr_qp = to_hr_qp(qp);

    if (((unsigned int)attr_mask & IBV_QP_STATE) && (attr->qp_state == IBV_QPS_ERR)) {
        pthread_spin_lock(&hr_qp->sq.lock);
        pthread_spin_lock(&hr_qp->rq.lock);
        cmd.sq_head = hr_qp->sq.head;
        cmd.rq_head = hr_qp->rq.head;
        flag = true;
    }

    ret = ibv_cmd_modify_qp(&hr_qp->ibv_qp, attr, attr_mask,
                            (struct ibv_modify_qp *)&cmd, sizeof(cmd));

    if (flag) {
        pthread_spin_unlock(&hr_qp->rq.lock);
        pthread_spin_unlock(&hr_qp->sq.lock);
    }

    if (!ret && (((unsigned)attr_mask) & IBV_QP_STATE) &&
            attr->qp_state == IBV_QPS_RESET) {
        hns_roce_v2_cq_clean(to_hr_cq(qp->recv_cq), qp->qp_num,
                             qp->srq ? to_hr_srq(qp->srq) : NULL);
        if (qp->send_cq != qp->recv_cq) {
            hns_roce_v2_cq_clean(to_hr_cq(qp->send_cq), qp->qp_num, NULL);
        }

        hns_roce_init_qp_indices(to_hr_qp(qp));
    }

    if (!ret && (((uint32_t)attr_mask) & IBV_QP_PORT)) {
        hr_qp->port_num = attr->port_num;
    }

    hr_qp->sl = attr->ah_attr.sl;

    return ret;
}

static void hns_roce_lock_cqs(struct ibv_qp *qp)
{
    struct hns_roce_cq *send_cq = to_hr_cq(qp->send_cq);
    struct hns_roce_cq *recv_cq = to_hr_cq(qp->recv_cq);

    if (send_cq == recv_cq) {
        pthread_spin_lock(&send_cq->lock);
    } else if (send_cq->cqn < recv_cq->cqn) {
        pthread_spin_lock(&send_cq->lock);
        pthread_spin_lock(&recv_cq->lock);
    } else {
        pthread_spin_lock(&recv_cq->lock);
        pthread_spin_lock(&send_cq->lock);
    }
}

static void hns_roce_unlock_cqs(struct ibv_qp *qp)
{
    struct hns_roce_cq *send_cq = to_hr_cq(qp->send_cq);
    struct hns_roce_cq *recv_cq = to_hr_cq(qp->recv_cq);

    if (send_cq == recv_cq) {
        pthread_spin_unlock(&send_cq->lock);
    } else if (send_cq->cqn < recv_cq->cqn) {
        pthread_spin_unlock(&recv_cq->lock);
        pthread_spin_unlock(&send_cq->lock);
    } else {
        pthread_spin_unlock(&send_cq->lock);
        pthread_spin_unlock(&recv_cq->lock);
    }
}

STATIC void hns_roce_u_v2_out_wq(unsigned int opcode, const unsigned long *pshareSq, const unsigned long *pshareWq)
{
    unsigned int icout = 0;
    if (opcode == HNS_ROCE_USER_CANCLE_QP) {
        for (icout = 0; icout < (IBV_EXP_WQE_ENTRY_LENGTH) / sizeof(unsigned long); icout++) {
            roce_warn("hns_roce_u_v2_printf_dfx cancel qp error share_sq Cancel QP(0x%llx) :0x%llx",
                      icout, *(pshareSq + icout));
        }

        for (icout = 0; icout < (IBV_EXP_TEMP_WQE_ENTRY_LENGTH) / sizeof(unsigned long); icout++) {
            roce_warn("hns_roce_u_v2_printf_dfx cancel qp error share_wq (0x%llx) :0x%llx",
                      icout, *(pshareWq + icout));
        }
    } else if (opcode == HNS_ROCE_USER_POLL_CQE) {
        for (icout = 0; icout < (IBV_EXP_WQE_ENTRY_LENGTH) / sizeof(unsigned long); icout++) {
            roce_err("hns_roce_u_v2_printf_dfx poll cqe error share_sq(0x%llx) :0x%llx",
                     icout, *(pshareSq + icout));
        }

        for (icout = 0; icout < (IBV_EXP_TEMP_WQE_ENTRY_LENGTH) / sizeof(unsigned long); icout++) {
            roce_err("hns_roce_u_v2_printf_dfx poll cqe error share_wq(0x%llx) :0x%llx",
                     icout, *(pshareWq + icout));
        }
    } else {
        for (icout = 0; icout < (IBV_EXP_WQE_ENTRY_LENGTH) / sizeof(unsigned long); icout++) {
            roce_dbg("hns_roce_u_v2_printf_dfx normal share_sq(0x%llx) :0x%llx",
                     icout, *(pshareSq + icout));
        }

        for (icout = 0; icout < (IBV_EXP_TEMP_WQE_ENTRY_LENGTH) / sizeof(unsigned long); icout++) {
            roce_dbg("hns_roce_u_v2_printf_dfx normal share_wq(0x%llx) :0x%llx",
                     icout, *(pshareWq + icout));
        }
    }
}

static void hns_roce_u_v2_dfx_by_opcode(unsigned int opcode, const struct hns_roce_dfx_context *dfxContext,
    const struct hns_roce_wq *wq, unsigned long *pshareSq, unsigned long *pshareWq)
{
    unsigned int utail = ((wq->tail & IBV_EXP_WQE_DEPTH_MASK) > 0) ? (wq->tail & IBV_EXP_WQE_DEPTH_MASK) : 0;

    switch (opcode) {
        case HNS_ROCE_USER_CANCLE_QP:
            if (utail != ((dfxContext->sq_pi + 1) & IBV_EXP_WQE_DEPTH_MASK)) {
                roce_warn("cancel qp, sq_idx:%u sq_pi:0x%llx tmp_idx:0x%llx stream_id 0x%llx db_val 0x%llx utail 0x%u",
                    dfxContext->sq_index, dfxContext->sq_pi, dfxContext->wq_temp_index, dfxContext->sream_id,
                    dfxContext->db_value, utail);
                roce_warn("wq info:cnt:0x%llx head 0x%llx tail 0x%llx maxpost 0x%llx maxgas 0x%llx offset 0x%llx",
                    wq->wqe_cnt, wq->head, wq->tail, wq->max_post, wq->max_gs, wq->offset);
                hns_roce_u_v2_out_wq(HNS_ROCE_USER_CANCLE_QP, pshareSq, pshareWq);
            } else {
                roce_dbg("normal cancel qp, sq_idx:%u sq_pi:0x%llx tmp_idx:0x%llx stream_id 0x%llx db_val 0x%llx",
                    dfxContext->sq_index, dfxContext->sq_pi, dfxContext->wq_temp_index, dfxContext->sream_id,
                    dfxContext->db_value);
                roce_dbg("wq info:cnt:0x%llx head 0x%llx tail 0x%llx maxpost 0x%llx maxgas 0x%llx offset 0x%llx",
                    wq->wqe_cnt, wq->head, wq->tail, wq->max_post, wq->max_gs, wq->offset);

                hns_roce_u_v2_out_wq(HNS_ROCE_USER_RESERVER, pshareSq, pshareWq);
            }
            break;
        case HNS_ROCE_USER_POLL_CQE:
            roce_err("poll cqe, dfxContext:SQ_idx 0x%llx sq_pi 0x%llx tmp_idx 0x%llx stream_id 0x%llx db_val 0x%llx",
                dfxContext->sq_index, dfxContext->sq_pi, dfxContext->wq_temp_index, dfxContext->sream_id,
                dfxContext->db_value);
            roce_err("wq info:Wq_cnt 0x%llx head 0x%llx tail 0x%llx maxpost 0x%llx maxgas 0x%llx offset 0x%llx",
                wq->wqe_cnt, wq->head, wq->tail, wq->max_post, wq->max_gs, wq->offset);
            hns_roce_u_v2_out_wq(HNS_ROCE_USER_POLL_CQE, pshareSq, pshareWq);

            break;
        default:
            roce_err("hns_roce_u_v2_printf_dfx Invalid opcode[%d]!", opcode);
            break;
    }
}

void hns_roce_u_v2_printf_dfx(struct hns_roce_qp *qp, unsigned int opcode)
{
    struct hns_roce_dfx_context *dfxContext = NULL;
    struct hns_roce_wq *wq = &(qp->sq);
    unsigned long *pshareSq = NULL;
    unsigned long *pshareWq = NULL;

    if (qp->gdr_enabled != HNS_ROCE_QP_MODE_GDR) {
        roce_warn("hns_roce_u_v2_printf_dfx not enable gdr !");
        return;
    }

    dfxContext = (struct hns_roce_dfx_context *)((char *)(to_hr_ctx(qp->ibv_qp.context)->share_buffer_base) +
                                                 qp->gdr_share_sq.dfx_offset);
    if (dfxContext == NULL) {
        roce_err("hns_roce_u_v2_printf_dfx NULL Context error!");
        return;
    }

    if (dfxContext->sq_pi > IBV_EXP_SHARED_SQ_DEPTH ||
            dfxContext->wq_temp_index > IBV_EXP_SHARED_TEMP_DEPTH) {
        roce_warn("hns_roce_u_v2_printf_dfx dfx invalid index sq_pi %u wq_temp_index 0x%llx !",
                 dfxContext->sq_pi, dfxContext->wq_temp_index);
        return;
    }

    pshareSq = (unsigned long *)((char *)(to_hr_ctx(qp->ibv_qp.context)->share_buffer_base) +
               (unsigned int)(qp->gdr_share_sq.sq_offset + dfxContext->sq_pi * IBV_EXP_WQE_ENTRY_LENGTH));

    pshareWq = (unsigned long *)((char *)(to_hr_ctx(qp->ibv_qp.context)->share_buffer_base) +
                                 (unsigned int)(qp->gdr_share_sq.temp_offset +
                                                dfxContext->wq_temp_index * IBV_EXP_TEMP_WQE_ENTRY_LENGTH));
    hns_roce_u_v2_dfx_by_opcode(opcode, dfxContext, wq, pshareSq, pshareWq);

    return ;
}

static void hns_roce_v2_clear_qp(struct hns_roce_context *ctx, uint32_t qpn)
{
    int tind = (qpn & ((unsigned int)(ctx->num_qps) - 1)) >> (unsigned int)ctx->qp_table_shift;

    if (!--ctx->qp_table[tind].refcnt) {
        free(ctx->qp_table[tind].table);
        ctx->qp_table[tind].table = NULL;
    } else {
        ctx->qp_table[tind].table[qpn & (uint32_t)(ctx->qp_table_mask)] = NULL;
    }
}

static void hns_roce_u_v2_free_qp_content(struct ibv_qp *ibqp, struct hns_roce_qp *qp)
{
    if (qp->rq.max_gs) {
        hns_roce_free_db(to_hr_ctx(ibqp->context), qp->rdb, HNS_ROCE_QP_TYPE_DB);
    }

    if (qp->sq.max_gs) {
        hns_roce_free_db(to_hr_ctx(ibqp->context), qp->sdb, HNS_ROCE_QP_TYPE_DB);
    }

    hns_roce_free_buf(&qp->buf);
    if (qp->rq_rinl_buf.wqe_list) {
        if (qp->rq_rinl_buf.wqe_list[0].sg_list) {
            free(qp->rq_rinl_buf.wqe_list[0].sg_list);
            qp->rq_rinl_buf.wqe_list[0].sg_list = NULL;
        }

        free(qp->rq_rinl_buf.wqe_list);
        qp->rq_rinl_buf.wqe_list = NULL;
    }

    free(qp->sq.wrid);
    qp->sq.wrid = NULL;

    if (qp->rq.wqe_cnt) {
        free(qp->rq.wrid);
        qp->rq.wrid = NULL;
    }

    if (qp->gdr_enabled == HNS_ROCE_CREATE_EXP) {
        if (qp->gdr_temp_wqe.bitmap) {
            free(qp->gdr_temp_wqe.bitmap);
            qp->gdr_temp_wqe.bitmap = NULL;
        }
    }
}

int hns_roce_u_v2_destroy_qp(struct ibv_qp *ibqp)
{
    int ret;
    struct hns_roce_qp *qp = NULL;

    HNS_ROCE_U_NULL_POINT_RETURN_ERR(ibqp);
    qp = to_hr_qp(ibqp);
    hns_roce_u_v2_printf_dfx(qp, HNS_ROCE_USER_CANCLE_QP);
    (void)pthread_mutex_lock(&to_hr_ctx(ibqp->context)->qp_table_mutex);
    ret = ibv_cmd_destroy_qp(ibqp);
    if (ret) {
        (void)pthread_mutex_unlock(&to_hr_ctx(ibqp->context)->qp_table_mutex);
        roce_err("ibv_cmd_destroy_qp failed ret %d, expect 0", ret);
        return ret;
    }

    hns_roce_lock_cqs(ibqp);
    __hns_roce_v2_cq_clean(to_hr_cq(ibqp->recv_cq), ibqp->qp_num, ibqp->srq ? to_hr_srq(ibqp->srq) : NULL);
    if (ibqp->send_cq != ibqp->recv_cq) {
        __hns_roce_v2_cq_clean(to_hr_cq(ibqp->send_cq), ibqp->qp_num, NULL);
    }
    hns_roce_v2_clear_qp(to_hr_ctx(ibqp->context), ibqp->qp_num);
    hns_roce_unlock_cqs(ibqp);
    (void)pthread_mutex_unlock(&to_hr_ctx(ibqp->context)->qp_table_mutex);

    hns_roce_u_v2_free_qp_content(ibqp, qp);

    pthread_spin_destroy(&qp->sq.lock);
    pthread_spin_destroy(&qp->rq.lock);
    free(qp);
    qp = NULL;

    return ret;
}

int hns_roce_u_v2_exp_peer_commit_qp(struct ibv_qp *ibvqp, struct ibv_exp_peer_commit *commit_ctx)
{
    struct peer_op_wr *wr = NULL;
    struct hns_roce_qp *qp = NULL;

    HNS_ROCE_U_NULL_POINT_RETURN_ERR(ibvqp);
    HNS_ROCE_U_NULL_POINT_RETURN_ERR(commit_ctx);
    qp = to_hr_qp(ibvqp);
    wr = commit_ctx->storage;
    commit_ctx->entries = 1;
    wr->type = IBV_EXP_PEER_OP_DB;
    wr->wr.db.db_addr = qp->peer_va_ids[HNS_QP_PEER_VA_ID_DBR];
    wr->wr.db.db_val = qp->peer_ctrl_db;
    qp->peer_ctrl_db = 0;

    return 0;
}

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
#include "hns_roce_u_db.h"
#include "hns_roce_u_abi.h"
#include "hns_roce_u_sec.h"
#include "hns_roce_u_hw_v2_qp.h"
#include "hns_roce_u_hw_v2_send.h"
#include "hns_roce_u_hw_v2_cq.h"
#include "hns_roce_u_hw_v2_opreation.h"
#include "securec.h"
#include "hns_roce_u_hw_v2.h"

#ifdef DEFINE_HNS_LLT
#undef static
#define static
#endif

static void hns_roce_u_v2_post_recv_inline_wqe(struct hns_roce_qp *qp, int *ind, struct ibv_recv_wr *wr)
{
    int i;
    struct hns_roce_rinl_sge *sge_list = NULL;

    /* QP support receive inline wqe */
    sge_list = qp->rq_rinl_buf.wqe_list[*ind].sg_list;
    qp->rq_rinl_buf.wqe_list[*ind].sge_cnt = (unsigned int)wr->num_sge;
/*lint -e409*/
    for (i = 0; i < wr->num_sge; i++) {
        sge_list[i].addr = (void *)(uintptr_t)wr->sg_list[i].addr;
        sge_list[i].len = wr->sg_list[i].length;
    }

    qp->rq.wrid[*ind] = wr->wr_id;

    *ind = ((unsigned int)*ind + 1) & (qp->rq.wqe_cnt - 1);
}

static void hns_roce_u_v2_post_recv_get_bad_wr(struct ibv_qp *ibvqp, struct ibv_recv_wr **bad_wr,
    struct ibv_recv_wr *wr)
{
    int attr_mask;
    struct ibv_qp_attr attr;
    int flag = 0;

    if (ibvqp->state == IBV_QPS_ERR) {
        attr_mask = IBV_QP_STATE;
        attr.qp_state = IBV_QPS_ERR;

        flag = hns_roce_u_v2_modify_qp(ibvqp, &attr, attr_mask);
        if (flag) {
            *bad_wr = wr;
            roce_err("hns_roce_u_v2_modify_qp failed ret %d, expect 0!", flag);
        }
    }
}

static void hns_roce_u_v2_post_recv_out(int nreq, struct hns_roce_qp *qp, struct hns_roce_context *ctx)
{
    if (nreq) {
        qp->rq.head += nreq;

        if (qp->flags & HNS_ROCE_SUPPORT_RQ_RECORD_DB) {
            udma_to_device_barrier();
            *qp->rdb = qp->rq.head & 0xffff;
        } else {
            hns_roce_update_rq_db(ctx, qp->ibv_qp.qp_num,
                                  qp->rq.head & ((qp->rq.wqe_cnt << 1) - 1));
        }
    }
}

static int hns_roce_u_v2_post_recv_check_out(struct hns_roce_qp *qp, int nreq,
    struct ibv_recv_wr **bad_wr, struct ibv_recv_wr *wr)
{
    int ret;
    if (hns_roce_v2_wq_overflow(&qp->rq, nreq, to_hr_cq(qp->ibv_qp.recv_cq))) {
        ret = -ENOMEM;
        *bad_wr = wr;
        roce_err("hns_roce_v2_wq_overflow");
        return ret;
    }

    if ((unsigned int)wr->num_sge > qp->rq.max_gs) {
        ret = -EINVAL;
        *bad_wr = wr;
        roce_err("num_sge [%d] is bigger than max_gs [%d]", wr->num_sge, wr->num_sge);
        return ret;
    }
    return 0;
}

static void hns_roce_u_v2_post_recv_fill_dseg(struct ibv_recv_wr *wr,
    struct hns_roce_v2_wqe_data_seg *dseg, const struct hns_roce_qp *qp)
{
    int i;
    for (i = 0; i < wr->num_sge; i++) {
        if (!wr->sg_list[i].length) {
            continue;
        }
        set_data_seg_v2(dseg, wr->sg_list + i);
        dseg++;
    }

    if (i < (int)qp->rq.max_gs) {
        dseg->len = 0;
        dseg->lkey = htole32(HNS_ROCE_INVALID_SGE_KEY);
        dseg->addr = 0;
    }
}

static int hns_roce_u_v2_post_recv(struct ibv_qp *ibvqp, struct ibv_recv_wr *wr, struct ibv_recv_wr **bad_wr)
{
    int ret = 0;
    int nreq;
    int ind;
    struct hns_roce_qp *qp = NULL;
    struct hns_roce_context *ctx = NULL;
    struct hns_roce_v2_wqe_data_seg *dseg = NULL;

    void *wqe = NULL;

    HNS_ROCE_U_NULL_POINT_RETURN_ERR(ibvqp);
    HNS_ROCE_U_NULL_POINT_RETURN_ERR(wr);
    HNS_ROCE_U_NULL_POINT_RETURN_ERR(bad_wr);
    qp = to_hr_qp(ibvqp);
    ctx = to_hr_ctx(ibvqp->context);

    pthread_spin_lock(&qp->rq.lock);

    /* check that state is OK to post receive */
    ind = qp->rq.head & (qp->rq.wqe_cnt - 1);

    if (ibvqp->state == IBV_QPS_RESET) {
        pthread_spin_unlock(&qp->rq.lock);
        *bad_wr = wr;
        roce_err("ibvqp->state is IBV_QPS_RESET");
        return -1;
    }

    for (nreq = 0; wr; ++nreq, wr = wr->next) {
        ret = hns_roce_u_v2_post_recv_check_out(qp, nreq, bad_wr, wr);
        if (ret) {
            roce_err("hns_roce_u_v2_post_recv_check_out fail, ret(%d)!", ret);
            goto out;
        }
        wqe = get_recv_wqe_v2(qp, ind);
        if (wqe == NULL) {
            roce_err("can't get recv wqe !");
            ret = -EINVAL;
            goto out;
        }

        dseg = (struct hns_roce_v2_wqe_data_seg *)wqe;

        hns_roce_u_v2_post_recv_fill_dseg(wr, dseg, qp);

        hns_roce_u_v2_post_recv_inline_wqe(qp, &ind, wr);
    }

out:
    hns_roce_u_v2_post_recv_out(nreq, qp, ctx);

    pthread_spin_unlock(&qp->rq.lock);

    hns_roce_u_v2_post_recv_get_bad_wr(ibvqp, bad_wr, wr);

    return ret;
}

static void fill_idx_que(struct hns_roce_idx_que *idx_que,
    int cur_idx, int wqe_idx)
{
    unsigned int *addr = NULL;

    addr = (unsigned int *)((char *)idx_que->buf.buf + (unsigned int)(cur_idx * idx_que->entry_sz));
    *addr = wqe_idx;
}

static int find_empty_entry(struct hns_roce_idx_que *idx_que)
{
    int i;
    int bit_num;

    /* bitmap[i] is set zero if all bits are allocated */
    for (i = 0; (i < idx_que->bitmap_len) && (idx_que->bitmap[i] == 0); ++i) {
        ;
    }

    if (i >= idx_que->bitmap_len) {
        roce_err("the idx que wqe bitmap space is used up!");
        return -ENOMEM;
    }

    bit_num = ffsl(idx_que->bitmap[i]);
    idx_que->bitmap[i] &= ~(1ULL << ((uint32_t)bit_num - 1));

    return (int)(i * sizeof(uint64_t) * BIT_CNT_PER_BYTE + (bit_num - 1));
}

STATIC int hns_roce_u_v2_post_srq_recv_list_for_wr(int *nreq, struct ibv_recv_wr *wr,
    struct hns_roce_srq *srq, int ind, struct ibv_recv_wr **bad_wr)
{
    int wqe_idx, i;
    int ret = 0;
    void *wqe = NULL;
    struct hns_roce_v2_wqe_data_seg *dseg = NULL;

    for (*nreq = 0; wr; ++(*nreq), wr = wr->next) {
        if ((unsigned int)wr->num_sge > srq->max_gs) {
            ret = -1;
            *bad_wr = wr;
            roce_err("num_sge [%d] bigger than max_gs [%d]", wr->num_sge, srq->max_gs);
            break;
        }

        if (srq->head == srq->tail) {
            /* SRQ is full */
            ret = -1;
            *bad_wr = wr;
            roce_err("SRQ is full");
            break;
        }

        wqe_idx = find_empty_entry(&srq->idx_que);
        if (wqe_idx < 0) {
            ret = -1;
            *bad_wr = wr;
            roce_err("srq idx que space is used up!");
            break;
        }
        fill_idx_que(&srq->idx_que, ind, wqe_idx);

        wqe = get_srq_wqe(srq, wqe_idx);
        dseg = (struct hns_roce_v2_wqe_data_seg *)wqe;

        for (i = 0; i < wr->num_sge; ++i) {
            dseg[i].len = htole32(wr->sg_list[i].length);
            dseg[i].lkey = htole32(wr->sg_list[i].lkey);
            dseg[i].addr = htole64(wr->sg_list[i].addr);
        }
/*lint +e409*/
        if (i < (int)srq->max_gs) {
            dseg[i].len = 0;
            dseg[i].lkey = htole32(HNS_ROCE_INVALID_SGE_KEY);
            dseg[i].addr = 0;
        }

        srq->wrid[wqe_idx] = wr->wr_id;
        ind = ((unsigned int)ind + 1) & ((unsigned int)srq->max - 1);
    }
    return ret;
}
static int hns_roce_u_v2_post_srq_recv(struct ibv_srq *ib_srq,
    struct ibv_recv_wr *wr,
    struct ibv_recv_wr **bad_wr)
{
    int ret;
    int nreq;
    int ind;

    HNS_ROCE_U_NULL_POINT_RETURN_ERR(ib_srq);
    HNS_ROCE_U_NULL_POINT_RETURN_ERR(wr);
    HNS_ROCE_U_NULL_POINT_RETURN_ERR(bad_wr);
    struct hns_roce_context *ctx = to_hr_ctx(ib_srq->context);
    struct hns_roce_srq *srq = to_hr_srq(ib_srq);
    struct hns_roce_db srq_db;

    pthread_spin_lock(&srq->lock);

    /* current idx of srqwq */
    ind = (unsigned int)srq->head & ((unsigned int)srq->max - 1U);

    ret = hns_roce_u_v2_post_srq_recv_list_for_wr(&nreq, wr, srq, ind, bad_wr);

    if (nreq) {
        srq->head += nreq;
        srq->idx_que.use_cnt += nreq;

        /*
         * Make sure that descriptors are written before
         * we write doorbell record.
         */
        udma_to_device_barrier();

        srq_db.byte_4 = HNS_ROCE_V2_SRQ_DB << DB_BYTE_4_CMD_S | srq->srqn;
        srq_db.parameter = srq->head;

        hns_roce_write64((uint32_t *)&srq_db, ctx, ROCEE_VF_DB_CFG0_OFFSET);
    }

    pthread_spin_unlock(&srq->lock);

    return ret;
}

struct hns_roce_u_hw g_hns_roce_u_hw_v2 = {
    .hw_version = HNS_ROCE_HW_VER2,
    .hw_ops = {
        .poll_cq = hns_roce_u_v2_poll_cq,
        .req_notify_cq = hns_roce_u_v2_arm_cq,
        .post_send = hns_roce_u_v2_post_send,
        .post_recv = hns_roce_u_v2_post_recv,
        .modify_qp = hns_roce_u_v2_modify_qp,
        .destroy_qp = hns_roce_u_v2_destroy_qp,
        .post_srq_recv = hns_roce_u_v2_post_srq_recv,
    },
    .exp_peer_commit_qp = hns_roce_u_v2_exp_peer_commit_qp,
    .exp_post_send = hns_roce_u_v2_exp_post_send,
};

struct hns_roce_u_hw g_hns_roce_u_hw_v3 = {
    .hw_version = HNS_ROCE_HW_VER3,
    .hw_ops = {
        .poll_cq = hns_roce_u_v2_poll_cq,
        .req_notify_cq = hns_roce_u_v2_arm_cq,
        .post_send = hns_roce_u_v2_post_send,
        .post_recv = hns_roce_u_v2_post_recv,
        .modify_qp = hns_roce_u_v2_modify_qp,
        .destroy_qp = hns_roce_u_v2_destroy_qp,
        .post_srq_recv = hns_roce_u_v2_post_srq_recv,
    },
    .exp_peer_commit_qp = hns_roce_u_v2_exp_peer_commit_qp,
    .exp_post_send = hns_roce_u_v2_exp_post_send,
};

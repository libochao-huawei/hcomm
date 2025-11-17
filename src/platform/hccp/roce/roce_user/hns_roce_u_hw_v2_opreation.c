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
#include "hns_roce_u_abi.h"
#include "hns_roce_u_hw_v2.h"
#include "hns_roce_u_hw_v2_opreation.h"

#ifdef DEFINE_HNS_LLT
#undef static
#define static
#endif

char *get_send_wqe(struct hns_roce_qp *qp, int n)
{
    return (char *)qp->buf.buf + (unsigned int)(qp->sq.offset + ((unsigned int)n << (unsigned int)(qp->sq.wqe_shift)));
}

void *get_send_sge_ex(struct hns_roce_qp *qp, int n)
{
    return (void *)((char *)qp->buf.buf + (unsigned int)(qp->sge.offset + ((unsigned int)n << 
             (unsigned int)(qp->sge.sge_shift))));
}

void set_data_seg_v2(struct hns_roce_v2_wqe_data_seg *dseg,
    struct ibv_sge *sg)
{
    dseg->lkey = htole32(sg->lkey);
    dseg->addr = htole64(sg->addr);
    dseg->len = htole32(sg->length);
}

void *get_recv_wqe_v2(struct hns_roce_qp *qp, int n)
{
    if ((n < 0) || (qp && ((unsigned int)n > qp->rq.wqe_cnt)) || qp == NULL) {
        roce_info("rq wqe index:%d,rq wqe cnt:%u", n, qp ? qp->rq.wqe_cnt : 0xffffffff);
        return NULL;
    }

    return (void *)((char *)qp->buf.buf + (unsigned int)(qp->rq.offset + ((unsigned int)n << 
             (unsigned int)(qp->rq.wqe_shift))));
}

void *get_srq_wqe(struct hns_roce_srq *srq, int n)
{
    return (void *)((char *)srq->buf.buf + (unsigned int)((unsigned int)n << (unsigned int)srq->wqe_shift));
}

struct hns_roce_v2_cqe *get_cqe_v2(struct hns_roce_cq *cq, int entry)
{
    return (struct hns_roce_v2_cqe *)((char *)cq->buf.buf + (unsigned int)(entry * HNS_ROCE_CQE_ENTRY_SIZE));
}

void *get_sw_cqe_v2(struct hns_roce_cq *cq, int n)
{
    struct hns_roce_v2_cqe *cqe = get_cqe_v2(cq, (unsigned int)n & (unsigned int)cq->verbs_cq.cq.cqe);

    return ((!!(roce_get_bit(cqe->byte_4, CQE_BYTE_4_OWNER_S)) !=
             !!((unsigned int)n & ((unsigned int)cq->verbs_cq.cq.cqe + 1U)))) ? cqe : NULL;
}

struct hns_roce_v2_cqe *next_cqe_sw_v2(struct hns_roce_cq *cq)
{
    return get_sw_cqe_v2(cq, cq->cons_index);
}

void hns_roce_update_rq_db(struct hns_roce_context *ctx,
    unsigned int qpn, unsigned int rq_head)
{
    struct hns_roce_db rq_db;

    rq_db.byte_4 = 0;
    rq_db.parameter = 0;

    roce_set_field(rq_db.byte_4, DB_BYTE_4_TAG_M, DB_BYTE_4_TAG_S, qpn);
    roce_set_field(rq_db.byte_4, DB_BYTE_4_CMD_M, DB_BYTE_4_CMD_S,
                   HNS_ROCE_V2_RQ_DB);
    roce_set_field(rq_db.parameter, DB_PARAM_RQ_PRODUCER_IDX_M,
                   DB_PARAM_RQ_PRODUCER_IDX_S, rq_head);

    udma_to_device_barrier();

    hns_roce_write64((uint32_t *)&rq_db, ctx, ROCEE_VF_DB_CFG0_OFFSET);
}

void hns_roce_update_sq_db(struct hns_roce_context *ctx,
    unsigned int qpn, unsigned int sl,
    unsigned int sq_head, int gdr_mode)
{
    struct hns_roce_db sq_db;

    sq_db.byte_4 = 0;

    /* In fact, the sq_head bits should be 15bit */
    sq_db.parameter = 0;

    /* cmd: 0 sq db; 1 rq db; 2; 2 srq db; 3 cq db ptr; 4 cq db ntr */
    roce_set_field(sq_db.byte_4, DB_BYTE_4_CMD_M, DB_BYTE_4_CMD_S,
                   HNS_ROCE_V2_SQ_DB);
    roce_set_field(sq_db.byte_4, DB_BYTE_4_TAG_M, DB_BYTE_4_TAG_S, qpn);

    roce_set_field(sq_db.parameter, DB_PARAM_SQ_PRODUCER_IDX_M,
                   DB_PARAM_SQ_PRODUCER_IDX_S, sq_head);
    roce_set_field(sq_db.parameter, DB_PARAM_SL_M, DB_PARAM_SL_S, sl);

    if (gdr_mode == HNS_ROCE_QP_MODE_NOR) {
        udma_to_device_barrier();
        hns_roce_write64((uint32_t *)&sq_db, ctx, ROCEE_VF_DB_CFG0_OFFSET);
    }
}

void hns_roce_get_sq_db(unsigned int qpn, unsigned int sl,
    unsigned int sq_head, uint64_t *value)
{
    struct hns_roce_db sq_db;
    uint32_t val[VAL_SIZE];

    sq_db.byte_4 = 0;

    /* In fact, the sq_head bits should be 15bit */
    sq_db.parameter = 0;

    /* cmd: 0 sq db; 1 rq db; 2; 2 srq db; 3 cq db ptr; 4 cq db ntr */
    roce_set_field(sq_db.byte_4, DB_BYTE_4_CMD_M, DB_BYTE_4_CMD_S, 0);
    roce_set_field(sq_db.byte_4, DB_BYTE_4_TAG_M, DB_BYTE_4_TAG_S, qpn);

    roce_set_field(sq_db.parameter, DB_PARAM_SQ_PRODUCER_IDX_M,
                   DB_PARAM_SQ_PRODUCER_IDX_S, sq_head);
    roce_set_field(sq_db.parameter, DB_PARAM_SL_M, DB_PARAM_SL_S, sl);

    val[0] = sq_db.byte_4;
    val[1] = sq_db.parameter;
    *value = HNS_ROCE_PAIR_TO_64(val);
    roce_info("val0[0x%x] val1[0x%x]", sq_db.byte_4, sq_db.parameter);
    roce_info("val0[0x%x] val1[0x%x]", val[0], val[1]);
}

int hns_roce_v2_wq_overflow(struct hns_roce_wq *wq, int nreq,
    struct hns_roce_cq *cq)
{
    unsigned int cur;

    cur = wq->head - wq->tail;
    if ((int)cur + nreq < wq->max_post) {
        return 0;
    }

    /* While the num of wqe exceeds cap of the device, cq will be locked */
    pthread_spin_lock(&cq->lock);
    cur = wq->head - wq->tail;
    pthread_spin_unlock(&cq->lock);

    return (int)cur + nreq >= wq->max_post;
}

void hns_roce_v2_update_cq_cons_index(struct hns_roce_context *ctx,
    const struct hns_roce_cq *cq)
{
    struct hns_roce_db cq_db;

    cq_db.byte_4 = 0;
    cq_db.parameter = 0;

    roce_set_field(cq_db.byte_4, DB_BYTE_4_TAG_M, DB_BYTE_4_TAG_S, cq->cqn);
    roce_set_field(cq_db.byte_4, DB_BYTE_4_CMD_M, DB_BYTE_4_CMD_S,
                   HNS_ROCE_V2_CQ_DB_PTR);

    roce_set_field(cq_db.parameter, DB_PARAM_CQ_CONSUMER_IDX_M,
                   DB_PARAM_CQ_CONSUMER_IDX_S,
                   cq->cons_index & ((cq->cq_depth << 1U) - 1));
    roce_set_field(cq_db.parameter, DB_PARAM_CQ_CMD_SN_M,
                   DB_PARAM_CQ_CMD_SN_S, 1);
    roce_set_bit(cq_db.parameter, DB_PARAM_CQ_NOTIFY_S, 0);

    udma_to_device_barrier();
    hns_roce_write64((uint32_t *)&cq_db, ctx, ROCEE_VF_DB_CFG0_OFFSET);
}

void hns_roce_dealloc_gdr_template_wqe(struct hns_roce_qp *qp, unsigned int wqe_index)
{
    int i;
    uint32_t bit_num;

    (void)pthread_mutex_lock((pthread_mutex_t *)&qp->gdr_temp_wqe.wqe_mutex);

    i = wqe_index / (BIT_CNT_PER_BYTE * sizeof(unsigned long));
    bit_num = (wqe_index % BIT_CNT_PER_BYTE) + 1;

    qp->gdr_temp_wqe.bitmap[i] |= (1ULL << (bit_num - 1));
    --qp->gdr_temp_wqe.use_cnt;

    (void)pthread_mutex_unlock((pthread_mutex_t *)&qp->gdr_temp_wqe.wqe_mutex);
}

int hns_roce_alloc_gdr_template_wqe(struct hns_roce_qp *qp, unsigned int *wqe_index)
{
    int i;
    uint32_t bit_num;

    if (qp->gdr_temp_wqe.use_cnt >= qp->gdr_temp_wqe.wqe_num) {
        roce_err("template wqe use_cnt:%d is more than wqe_num:%d!",
                 qp->gdr_temp_wqe.use_cnt, qp->gdr_temp_wqe.wqe_num);
        return -EFAULT;
    }

    (void)pthread_mutex_lock((pthread_mutex_t *)&qp->gdr_temp_wqe.wqe_mutex);

    /* if the bitmap is allocated, the bitmap[i] is set zero */
    for (i = 0; i < qp->gdr_temp_wqe.bitmap_len && qp->gdr_temp_wqe.bitmap[i] == 0; ++i) {
        ;
    }

    if (i >= qp->gdr_temp_wqe.bitmap_len) {
        roce_err("the gdr template wqe bitmap space is used up!");
        (void)pthread_mutex_unlock((pthread_mutex_t *)&qp->gdr_temp_wqe.wqe_mutex);
        return -ENOMEM;
    }

    roce_info("qp->gdr_temp_wqe.bitmap[%d]:0x%lx!", i, qp->gdr_temp_wqe.bitmap[i]);

    /* ffsl() return the position of the first bit set, or 0 if no bits are set in i. */
    bit_num = (uint32_t)ffsl(qp->gdr_temp_wqe.bitmap[i]);
    qp->gdr_temp_wqe.bitmap[i] &= ~(1ULL << (bit_num - 1));

    *wqe_index = (i * sizeof(unsigned long) * BIT_CNT_PER_BYTE + (bit_num - 1));

    ++qp->gdr_temp_wqe.use_cnt;

    roce_info("hns_roce_alloc_gdr_template_wqe index:%u bitmap[%d]:0x%lx!",
              *wqe_index, i, qp->gdr_temp_wqe.bitmap[i]);

    (void)pthread_mutex_unlock((pthread_mutex_t *)&qp->gdr_temp_wqe.wqe_mutex);

    return 0;
}

STATIC void hns_roce_u_v2_rdma_read(struct ibv_send_wr *wr,
    struct hns_roce_rc_sq_wqe *rc_sq_wqe)
{
    roce_set_field(rc_sq_wqe->byte_4,
                   RC_SQ_WQE_BYTE_4_OPCODE_M, RC_SQ_WQE_BYTE_4_OPCODE_S,
                   HNS_ROCE_WQE_OP_RDMA_READ);

    rc_sq_wqe->va = htole64(wr->wr.rdma.remote_addr);
    rc_sq_wqe->rkey = htole32(wr->wr.rdma.rkey);
}

STATIC void hns_roce_u_v2_rdma_write(struct ibv_send_wr *wr,
    struct hns_roce_rc_sq_wqe *rc_sq_wqe)
{
    roce_set_field(rc_sq_wqe->byte_4,
                   RC_SQ_WQE_BYTE_4_OPCODE_M, RC_SQ_WQE_BYTE_4_OPCODE_S,
                   HNS_ROCE_WQE_OP_RDMA_WRITE);
    rc_sq_wqe->va = htole64(wr->wr.rdma.remote_addr);
    rc_sq_wqe->rkey = htole32(wr->wr.rdma.rkey);
}

STATIC void hns_roce_u_v2_write_imm(struct ibv_send_wr *wr,
    struct hns_roce_rc_sq_wqe *rc_sq_wqe)
{
    roce_set_field(rc_sq_wqe->byte_4,
                   RC_SQ_WQE_BYTE_4_OPCODE_M, RC_SQ_WQE_BYTE_4_OPCODE_S,
                   HNS_ROCE_WQE_OP_RDMA_WRITE_WITH_IMM);
    rc_sq_wqe->va = htole64(wr->wr.rdma.remote_addr);
    rc_sq_wqe->rkey = htole32(wr->wr.rdma.rkey);
    rc_sq_wqe->immtdata = htole32(be32toh(wr->imm_data));
}

STATIC void hns_roce_u_v2_send(struct ibv_send_wr *wr,
    struct hns_roce_rc_sq_wqe *rc_sq_wqe)
{
    roce_set_field(rc_sq_wqe->byte_4,
                   RC_SQ_WQE_BYTE_4_OPCODE_M, RC_SQ_WQE_BYTE_4_OPCODE_S,
                   HNS_ROCE_WQE_OP_SEND);
}

STATIC void hns_roce_u_v2_send_inv(struct ibv_send_wr *wr,
    struct hns_roce_rc_sq_wqe *rc_sq_wqe)
{
    roce_set_field(rc_sq_wqe->byte_4,
                   RC_SQ_WQE_BYTE_4_OPCODE_M, RC_SQ_WQE_BYTE_4_OPCODE_S,
                   HNS_ROCE_WQE_OP_SEND_WITH_INV);
    rc_sq_wqe->inv_key = htole32(wr->invalidate_rkey);
}

STATIC void hns_roce_u_v2_send_imm(struct ibv_send_wr *wr,
    struct hns_roce_rc_sq_wqe *rc_sq_wqe)
{
    rc_sq_wqe->immtdata = htole32(be32toh(wr->imm_data));
    roce_set_field(rc_sq_wqe->byte_4,
                   RC_SQ_WQE_BYTE_4_OPCODE_M, RC_SQ_WQE_BYTE_4_OPCODE_S,
                   HNS_ROCE_WQE_OP_SEND_WITH_IMM);
}

STATIC void hns_roce_u_v2_local_inv(struct ibv_send_wr *wr,
    struct hns_roce_rc_sq_wqe *rc_sq_wqe)
{
    roce_set_field(rc_sq_wqe->byte_4,
                   RC_SQ_WQE_BYTE_4_OPCODE_M, RC_SQ_WQE_BYTE_4_OPCODE_S,
                   HNS_ROCE_WQE_OP_LOCAL_INV);
    roce_set_bit(rc_sq_wqe->byte_4, RC_SQ_WQE_BYTE_4_SO_S, 1);
    rc_sq_wqe->inv_key = htole32(wr->invalidate_rkey);
}

STATIC void hns_roce_u_v2_bind_mw(struct ibv_send_wr *wr,
    struct hns_roce_rc_sq_wqe *rc_sq_wqe)
{
    roce_set_field(rc_sq_wqe->byte_4,
                   RC_SQ_WQE_BYTE_4_OPCODE_M, RC_SQ_WQE_BYTE_4_OPCODE_S,
                   HNS_ROCE_WQE_OP_BIND_MW_TYPE);

    roce_set_bit(rc_sq_wqe->byte_4,
                 RC_SQ_WQE_BYTE_4_MW_TYPE_S, (unsigned int)wr->bind_mw.mw->type - 1U);

    roce_set_bit(rc_sq_wqe->byte_4, RC_SQ_WQE_BYTE_4_ATOMIC_S,
                 (wr->bind_mw.bind_info.mw_access_flags &
                  IBV_ACCESS_REMOTE_ATOMIC) ? 1 : 0);

    roce_set_bit(rc_sq_wqe->byte_4, RC_SQ_WQE_BYTE_4_RDMA_READ_S,
                 (wr->bind_mw.bind_info.mw_access_flags &
                  IBV_ACCESS_REMOTE_READ) ? 1 : 0);

    roce_set_bit(rc_sq_wqe->byte_4, RC_SQ_WQE_BYTE_4_RDMA_WRITE_S,
                 (wr->bind_mw.bind_info.mw_access_flags &
                  IBV_ACCESS_REMOTE_WRITE) ? 1 : 0);

    rc_sq_wqe->new_rkey = htole32(wr->bind_mw.rkey);
    rc_sq_wqe->byte_16 = htole32(wr->bind_mw.bind_info.length & 0xffffffff);
    rc_sq_wqe->byte_20 = htole32(wr->bind_mw.bind_info.length >> BIND_INFO_SHIFT_THIRTY_TWO); //lint !e572
    rc_sq_wqe->rkey = htole32(wr->bind_mw.bind_info.mr->rkey);

    rc_sq_wqe->va = htole64(wr->bind_mw.bind_info.addr);
}

STATIC void hns_roce_u_v2_write_notify(struct ibv_send_wr *wr,
    struct hns_roce_rc_sq_wqe *rc_sq_wqe)
{
    roce_set_field(rc_sq_wqe->byte_4, RC_SQ_WQE_BYTE_4_OPCODE_M, RC_SQ_WQE_BYTE_4_OPCODE_S,
                   HNS_ROCE_WQE_OP_WRITE_WITH_NOTIFY);
    rc_sq_wqe->va = htole64(wr->wr.rdma.remote_addr);
    rc_sq_wqe->rkey = htole32(wr->wr.rdma.rkey);
    rc_sq_wqe->immtdata = htole32(be32toh(wr->imm_data));
}

struct hns_roce_u_table g_rdmaops_array[] = {
    {IBV_WR_RDMA_READ, hns_roce_u_v2_rdma_read},
    {IBV_WR_RDMA_WRITE, hns_roce_u_v2_rdma_write},
    {IBV_WR_RDMA_WRITE_WITH_IMM, hns_roce_u_v2_write_imm},
    {IBV_WR_SEND, hns_roce_u_v2_send},
    {IBV_WR_SEND_WITH_INV, hns_roce_u_v2_send_inv},
    {IBV_WR_SEND_WITH_IMM, hns_roce_u_v2_send_imm},
    {IBV_WR_LOCAL_INV, hns_roce_u_v2_local_inv},
    {IBV_WR_BIND_MW, hns_roce_u_v2_bind_mw},
    {IBV_WR_RDMA_WRITE_WITH_NOTIFY, hns_roce_u_v2_write_notify}
};

int hns_roce_u_v2_operation(struct ibv_send_wr *wr,
    struct hns_roce_rc_sq_wqe *rc_sq_wqe)
{
    int i;
    int array_num = sizeof(g_rdmaops_array) / sizeof(struct hns_roce_u_table);

    for (i = 0; i < array_num; i++) {
        if (wr->opcode == g_rdmaops_array[i].wr_opcode) {
            g_rdmaops_array[i].rdma_operation(wr, rc_sq_wqe);
            return 0;
        }
    }

    roce_set_field(rc_sq_wqe->byte_4, RC_SQ_WQE_BYTE_4_OPCODE_M,
                   RC_SQ_WQE_BYTE_4_OPCODE_S, HNS_ROCE_WQE_OP_MASK);
    roce_err("Not supported opcode %d", wr->opcode);

    return -EINVAL;
}
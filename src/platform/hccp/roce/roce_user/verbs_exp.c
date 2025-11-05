/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "verbs_exp.h"

struct ibv_qp *ibv_exp_create_qp(struct ibv_pd *pd, struct ibv_exp_qp_init_attr *qp_init_attr,
                                 struct rdma_lite_device_qp_attr *qp_resp)
{
    struct verbs_context_exp *vctx = NULL;
    struct ibv_qp *qp = NULL;

    VERBS_NULL_POINT_RETURN_NULL(pd);
    VERBS_NULL_POINT_RETURN_NULL(qp_init_attr);
    VERBS_NULL_POINT_RETURN_NULL(qp_resp);

    vctx = verbs_get_exp_ctx_op(pd->context, drv_exp_create_qp);  //lint !e574
    if (vctx == NULL) {
        errno = ENOSYS;
        return NULL;
    }

    qp = vctx->drv_exp_create_qp(pd, qp_init_attr, qp_resp);
    if (qp != NULL) {
        qp->context          = pd->context;
        qp->qp_context       = qp_init_attr->attr.qp_context;
        qp->pd               = pd;
        qp->send_cq          = qp_init_attr->attr.send_cq;
        qp->recv_cq          = qp_init_attr->attr.recv_cq;
        qp->srq              = qp_init_attr->attr.srq;
        qp->qp_type          = qp_init_attr->attr.qp_type;
        qp->state            = IBV_QPS_RESET;
        qp->events_completed = 0;
        pthread_mutex_init(&qp->mutex, NULL);
        pthread_cond_init(&qp->cond, NULL);
    }

    return qp;
}

struct ibv_cq *ibv_exp_create_cq(struct ibv_context *context, int cqe, void *cq_context,
    struct ibv_comp_channel *channel, int vector,
    struct rdma_lite_device_cq_init_attr *attr, struct rdma_lite_device_cq_attr *cq_resp)
{
    struct verbs_context_exp *vctx = NULL;
    struct ibv_cq *cq = NULL;

    VERBS_NULL_POINT_RETURN_NULL(context);
    VERBS_NULL_POINT_RETURN_NULL(attr);
    VERBS_NULL_POINT_RETURN_NULL(cq_resp);

    vctx = verbs_get_exp_ctx_op(context, drv_exp_create_cq);
    if (vctx == NULL) {
        errno = ENOSYS;
        return NULL;
    }

    cq = vctx->drv_exp_create_cq(context, cqe, channel, vector, attr, cq_resp);
    if (cq != NULL) {
        cq->context = context;
        cq->channel = channel;
        cq->async_events_completed = 0;
        cq->comp_events_completed = 0;
        pthread_mutex_init(&cq->mutex, NULL);
        pthread_cond_init(&cq->cond, NULL);
    }

    return cq;
}

struct ibv_mr *ibv_exp_reg_mr(struct ibv_pd *pd, void *addr, size_t length, int access,
    struct roce_process_sign roce_sign)
{
    struct ibv_mr *mr = NULL;
    struct verbs_context_exp *vctx = NULL;

    VERBS_NULL_POINT_RETURN_NULL(pd);
    VERBS_NULL_POINT_RETURN_NULL(addr);

    vctx = verbs_get_exp_ctx_op(pd->context, drv_exp_reg_mr);  //lint !e574
    if (vctx == NULL) {
        return NULL;
    }

    if (ibv_dontfork_range(addr, length)) {
        return NULL;
    }

    mr = vctx->drv_exp_reg_mr(pd, addr, length, access, roce_sign);
    if (mr != NULL) {
        mr->context = pd->context;
        mr->pd      = pd;
        mr->addr    = addr;
        mr->length  = length;
    } else {
        ibv_dofork_range(addr, length);
    }

    return mr;
}

struct verbs_context_exp *verbs_get_exp_ctx(struct ibv_context *ctx)
{
    struct verbs_context *app_ex_ctx = NULL;

    VERBS_NULL_POINT_RETURN_NULL(ctx);

    app_ex_ctx = verbs_get_ctx(ctx);
    if (app_ex_ctx == NULL) {
        return NULL;
    }
    return (struct verbs_context_exp *)((char *)app_ex_ctx + sizeof(struct verbs_context));
}

int ibv_exp_query_get_ctx(struct ibv_context *context, unsigned long long *notify_va,
    unsigned long long *size)
{
    struct verbs_context_exp *vctx = NULL;

    VERBS_NULL_POINT_RETURN_ERR(context);
    VERBS_NULL_POINT_RETURN_ERR(notify_va);
    VERBS_NULL_POINT_RETURN_ERR(size);

    vctx = verbs_get_exp_ctx_op(context, drv_exp_query_notify);   //lint !e574
    if (vctx == NULL) {
        return -ENOSYS;
    }

    return vctx->drv_exp_query_notify(context, notify_va, size);
}

int ibv_exp_query_notify(struct ibv_context *context, unsigned long long *notify_va,
    unsigned long long *size)
{
    if ((context == NULL) || (notify_va == NULL) || (size == NULL)) {
        return -ENOSYS;
    }

    return ibv_exp_query_get_ctx(context, notify_va, size);
}

int ibv_exp_post_send(struct ibv_qp *qp, struct ibv_send_wr *wr, struct ibv_send_wr **bad_wr,
    struct wr_exp_rsp *exp_rsp)
{
    struct verbs_context_exp *vctx = NULL;

    VERBS_NULL_POINT_RETURN_ERR(qp);
    VERBS_NULL_POINT_RETURN_ERR(wr);
    VERBS_NULL_POINT_RETURN_ERR(bad_wr);
    VERBS_NULL_POINT_RETURN_ERR(exp_rsp);

    vctx = verbs_get_exp_ctx_op(qp->context, drv_exp_post_send);  //lint !e574
    if (vctx == NULL) {
        return -ENOSYS;
    }

    return vctx->drv_exp_post_send(qp, wr, bad_wr, exp_rsp);
}

int ibv_exp_query_device(struct ibv_context *context, struct dev_cap_info *cap)
{
    struct verbs_context_exp *vctx = NULL;

    VERBS_NULL_POINT_RETURN_ERR(context);
    VERBS_NULL_POINT_RETURN_ERR(cap);

    vctx = verbs_get_exp_ctx_op(context, drv_exp_query_device);
    if (vctx == NULL) {
        return -ENOSYS;
    }

    return vctx->drv_exp_query_device(context, cap);
}

struct ibv_ah *ibv_exp_create_ah(struct ibv_pd *pd, struct ibv_exp_ah_attr *attrx)
{
    errno = EOPNOTSUPP;

    return NULL;
}

/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <errno.h>
#include <infiniband/verbs.h>
#include "securec.h"
#include "dl_hal_function.h"
#include "dl_ibverbs_function.h"
#include "hccp_common.h"
#include "rs.h"
#include "ra_rs_err.h"
#include "rs_inner.h"
#include "rs_epoll.h"
#include "rs_socket.h"
#include "rs_drv_rdma.h"
#include "rs_ping_inner.h"
#ifndef HNS_ROCE_LLT
#include <dlog_pub.h>
#endif
#include "rs_ping_roce.h"

#define RS_PING_ROCE_RECV_WC_NUM 16

struct ibv_wc gPingQpRecvWc[RS_PING_ROCE_RECV_WC_NUM] = { 0 };
struct ibv_wc gPongQpRecvWc[RS_PING_ROCE_RECV_WC_NUM] = { 0 };

STATIC bool RsPingRoceCheckFd(struct rs_ping_ctx_cb *pingCb, int fd)
{
    if (pingCb->ping_qp.channel != NULL && pingCb->ping_qp.channel->fd == fd) {
        hccp_dbg("ping_qp rq, channel->fd:%d poll cq", fd);
        return true;
    }
    return false;
}

STATIC bool RsPongRoceCheckFd(struct rs_ping_ctx_cb *pingCb, int fd)
{
    if (pingCb->pong_qp.channel != NULL && pingCb->pong_qp.channel->fd == fd) {
        hccp_dbg("pong_qp rq, channel->fd:%d poll cq", fd);
        return true;
    }
    return false;
}

STATIC int RsPingCbGetDevRdevIndex(struct rs_ping_ctx_cb *pingCb, int index)
{
#ifdef CUSTOM_INTERFACE
    struct roce_dev_data rdevData = { 0 };
    int ret;

    RS_PTHREAD_MUTEX_LOCK(&pingCb->ping_mutex);
    pingCb->rdev_cb.dev_name = RsIbvGetDeviceName(pingCb->rdev_cb.dev_list[index]);
    ret = RsRoceGetRoceDevData(pingCb->rdev_cb.dev_name, &rdevData);
    if (ret != 0) {
        hccp_err("rs_roce_get_roce_dev_data failed, ret:%d, dev_name:%s", ret, pingCb->rdev_cb.dev_name);
        RS_PTHREAD_MUTEX_ULOCK(&pingCb->ping_mutex);
        return ret;
    }
    pingCb->dev_index = rdevData.rdev_index; // rdev_index is same to port_id
    RS_PTHREAD_MUTEX_ULOCK(&pingCb->ping_mutex);
#endif
    return 0;
}

STATIC int RsPingCbGetIbCtxAndIndex(struct rdev *rdevInfo, struct rs_ping_ctx_cb *pingCb)
{
    struct ibv_context *ibCtx = NULL;
    int ret;
    int i;

    for (i = 0; (i < pingCb->rdev_cb.dev_num) && (pingCb->rdev_cb.dev_list[i] != NULL); ++i) {
        ibCtx = RsIbvOpenDevice(pingCb->rdev_cb.dev_list[i]);
        CHK_PRT_RETURN(ibCtx == NULL, hccp_err("ibv_open_device failed!"), -ENODEV);
        ret = RsQueryGid(*rdevInfo, ibCtx, pingCb->rdev_cb.ib_port, &pingCb->rdev_cb.gid_idx);
        if (ret == 0) {
            ret = RsPingCbGetDevRdevIndex(pingCb, i);
            if (ret != 0) {
                hccp_err("rs_ping_cb_get_dev_rdev_index failed, ret:%d", ret);
                RsIbvCloseDevice(ibCtx);
                return ret;
            }
            pingCb->rdev_cb.ib_ctx = ibCtx;
            ret = RsIbvQueryGid(ibCtx, pingCb->rdev_cb.ib_port, pingCb->rdev_cb.gid_idx, &pingCb->rdev_cb.gid);
            if (ret != 0) {
                RsIbvCloseDevice(ibCtx);
                hccp_err("query gid failed gid_idx %d, ret %d", pingCb->rdev_cb.gid_idx, ret);
                return -EOPENSRC;
            }
            return 0;
        } else if (ret == -EEXIST) {
            RsIbvCloseDevice(ibCtx);
        } else {
            hccp_err("rs_query_gid failed, ret:%d", ret);
            RsIbvCloseDevice(ibCtx);
            return ret;
        }
    }

    CHK_PRT_RETURN(i == pingCb->rdev_cb.dev_num, hccp_err("can not find ib_ctx for phy_id[%u] local_ip[0x%x] "
        "in dev_list!", rdevInfo->phy_id, rdevInfo->local_ip.addr.s_addr), -ENODEV);
    return 0;
}

STATIC int RsPingCommonModifyLocalQp(struct rs_ping_ctx_cb *pingCb, struct rs_ping_local_qp_cb *qpCb)
{
    struct ibv_qp_init_attr initAttr;
    struct ibv_qp_attr attr = { 0 };
    int ret;

    ret = RsIbvQueryQp(qpCb->ib_qp, &attr, IBV_QP_STATE, &initAttr);
    CHK_PRT_RETURN(ret != 0 || attr.qp_state != IBV_QPS_RESET,
        hccp_err("rs_ibv_query_qp qpn:%u fail, ret:%d attr.qp_state:%d != %d",
        qpCb->ib_qp->qp_num, ret, attr.qp_state, IBV_QPS_RESET), -EOPENSRC);

    attr.qp_state = IBV_QPS_INIT;
    attr.pkey_index = 0;
    attr.port_num = pingCb->rdev_cb.ib_port;
    attr.qkey = qpCb->qkey;
    ret = RsIbvModifyQp(qpCb->ib_qp, &attr, IBV_QP_STATE | IBV_QP_PKEY_INDEX | IBV_QP_PORT | IBV_QP_QKEY);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_ibv_modify_qp qpn:%u to init fail, ret:%d, errno:%d",
        qpCb->ib_qp->qp_num, ret, errno), -EOPENSRC);

    attr.qp_state = IBV_QPS_RTR;
    ret = RsIbvModifyQp(qpCb->ib_qp, &attr, IBV_QP_STATE);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_ibv_modify_qp qpn:%u to rtr fail, ret:%d, errno:%d",
        qpCb->ib_qp->qp_num, ret, errno), -EOPENSRC);

    attr.qp_state = IBV_QPS_RTS;
    attr.sq_psn = 0;
    ret = RsIbvModifyQp(qpCb->ib_qp, &attr, IBV_QP_STATE | IBV_QP_SQ_PSN);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_ibv_modify_qp qpn:%u to rts fail, ret:%d, errno:%d",
        qpCb->ib_qp->qp_num, ret, errno), -EOPENSRC);

    return 0;
}

STATIC int RsPingCommonInitLocalQp(struct rs_cb *rscb, struct rs_ping_ctx_cb *pingCb, union ping_qp_attr *attr,
    struct rs_ping_local_qp_cb *qpCb)
{
    struct ibv_exp_qp_init_attr qpInitAttr = { 0 };
    struct rdma_lite_device_qp_attr qpResp = { 0 };
    int randNum;
    int ret;

    hccp_info("cq_attr{%d %d, %d %d}", attr->rdma.cq_attr.send_cq_depth, attr->rdma.cq_attr.send_cq_comp_vector,
        attr->rdma.cq_attr.recv_cq_depth, attr->rdma.cq_attr.recv_cq_comp_vector);

    // create send cq with attr
    qpCb->send_cq.depth = attr->rdma.cq_attr.send_cq_depth;
    qpCb->send_cq.comp_vector = attr->rdma.cq_attr.send_cq_comp_vector;
    qpCb->send_cq.ib_cq = RsIbvCreateCq(pingCb->rdev_cb.ib_ctx, qpCb->send_cq.depth, NULL, NULL,
        qpCb->send_cq.comp_vector);
    qpCb->send_cq.max_recv_wc_num = RS_PING_ROCE_RECV_WC_NUM;
    ret = -errno;
    CHK_PRT_RETURN(qpCb->send_cq.ib_cq == NULL, hccp_err("rs_ibv_create_cq send cq fail, ret:%d", ret), ret);

    // create channel & create recv cq with attr
    qpCb->channel = RsIbvCreateCompChannel(pingCb->rdev_cb.ib_ctx);
    if (qpCb->channel == NULL) {
        ret = -errno;
        hccp_err("rs_ibv_create_comp_channel failed! ret:%d", ret);
        goto create_channel_fail;
    }
    ret = RsEpollCtl(rscb->conn_cb.epollfd, EPOLL_CTL_ADD, qpCb->channel->fd, EPOLLIN | EPOLLRDHUP);
    if (ret != 0) {
        hccp_err("rs_epoll_ctl failed! epollfd:%d fd:%d ret:%d", rscb->conn_cb.epollfd, qpCb->channel->fd, ret);
        goto epoll_ctl_fail;
    }
    qpCb->recv_cq.depth = attr->rdma.cq_attr.recv_cq_depth;
    qpCb->recv_cq.comp_vector = attr->rdma.cq_attr.recv_cq_comp_vector;
    qpCb->recv_cq.ib_cq = RsIbvCreateCq(pingCb->rdev_cb.ib_ctx, qpCb->recv_cq.depth, NULL, qpCb->channel,
        qpCb->recv_cq.comp_vector);
    qpCb->recv_cq.max_recv_wc_num = RS_PING_ROCE_RECV_WC_NUM;
    if (qpCb->recv_cq.ib_cq == NULL) {
        ret = -errno;
        hccp_err("rs_ibv_create_cq recv cq fail, ret:%d", ret);
        goto create_rcq_fail;
    }

    // create qp with attr
    (void)RsDrvGetRandomNum(&randNum);
    // clear bit IB_QP_SET_QKEY to avoid modify_qp to INIT failed
    qpCb->qkey = (uint32_t)(((uint32_t)randNum) & (~(1U << 31U)));
    (void)memcpy_s(&qpCb->qp_cap, sizeof(struct ibv_qp_cap), &attr->rdma.qp_attr.cap, sizeof(struct ibv_qp_cap));
    qpCb->udp_sport = attr->rdma.qp_attr.udp_sport;
    qpInitAttr.attr.send_cq = qpCb->send_cq.ib_cq;
    qpInitAttr.attr.recv_cq = qpCb->recv_cq.ib_cq;
    (void)memcpy_s(&qpInitAttr.attr.cap, sizeof(struct ibv_qp_cap), &qpCb->qp_cap, sizeof(struct ibv_qp_cap));
    qpInitAttr.attr.qp_type = IBV_QPT_UD;
    qpInitAttr.udp_sport = attr->rdma.qp_attr.udp_sport;

    hccp_info("qkey:%u udp_sport:%u qp_cap{%u %u %u %u %u}", qpCb->qkey, qpCb->udp_sport,
        attr->rdma.qp_attr.cap.max_send_wr, attr->rdma.qp_attr.cap.max_recv_wr, attr->rdma.qp_attr.cap.max_send_sge,
        attr->rdma.qp_attr.cap.max_recv_sge, attr->rdma.qp_attr.cap.max_inline_data);
    qpCb->ib_qp = RsIbvExpCreateQp(pingCb->rdev_cb.ib_pd, &qpInitAttr, &qpResp);
    if (qpCb->ib_qp == NULL) {
        ret = -errno;
        hccp_err("rs_ibv_exp_create_qp qp fail, ret:%d", ret);
        goto create_qp_fail;
    }

    ret = RsPingCommonModifyLocalQp(pingCb, qpCb);
    if (ret != 0) {
        hccp_err("rs_ping_common_modify_local_qp failed, ret:%d", ret);
        goto modify_qp_fail;
    }

    ret = RsIbvReqNotifyCq(qpCb->recv_cq.ib_cq, 0);
    if (ret != 0) {
        hccp_err("rs_ibv_req_notify_cq failed, ret:%d", ret);
        goto modify_qp_fail;
    }

    hccp_run_info("qpn:%u create success, cq_attr{%d %d, %d %d} qkey:%u udp_sport:%u qp_cap{%u %u %u %u %u}",
        qpCb->ib_qp->qp_num, attr->rdma.cq_attr.send_cq_depth, attr->rdma.cq_attr.send_cq_comp_vector,
        attr->rdma.cq_attr.recv_cq_depth, attr->rdma.cq_attr.recv_cq_comp_vector, qpCb->qkey, qpCb->udp_sport,
        attr->rdma.qp_attr.cap.max_send_wr, attr->rdma.qp_attr.cap.max_recv_wr, attr->rdma.qp_attr.cap.max_send_sge,
        attr->rdma.qp_attr.cap.max_recv_sge, attr->rdma.qp_attr.cap.max_inline_data);

    return 0;

modify_qp_fail:
    (void)RsIbvDestroyQp(qpCb->ib_qp);
create_qp_fail:
    (void)RsIbvDestroyCq(qpCb->recv_cq.ib_cq);
create_rcq_fail:
    (void)RsEpollCtl(rscb->conn_cb.epollfd, EPOLL_CTL_DEL, qpCb->channel->fd, EPOLLIN | EPOLLRDHUP);
epoll_ctl_fail:
    (void)RsIbvDestroyCompChannel(qpCb->channel);
create_channel_fail:
    (void)RsIbvDestroyCq(qpCb->send_cq.ib_cq);
    return ret;
}

STATIC int RsPingCommonInitMrCb(struct rs_cb *rscb, struct rs_ping_ctx_cb *pingCb, struct rs_ping_mr_cb *mrCb)
{
    unsigned long flag = 0;
    uint32_t idx = 0;
    int ret;

    hccp_info("payload_offset:%u len:0x%llx sge_num:%u grp_id:%u",
        mrCb->payload_offset, mrCb->len, mrCb->sge_num, rscb->grp_id);

    ret = pthread_mutex_init(&mrCb->mutex, NULL);
    CHK_PRT_RETURN(ret != 0, hccp_err("pthread_mutex_init mr_cb mutex failed, ret:%d", ret), ret);

    flag = ((unsigned long)pingCb->logic_devid << BUFF_FLAGS_DEVID_OFFSET) | BUFF_SP_SVM;
    ret = DlHalBuffAllocAlignEx(mrCb->len, RA_RS_PING_BUFFER_ALIGN_4K_PAGE_SIZE, flag,
        (int)rscb->grp_id, (void **)&mrCb->addr);
    if (ret != 0) {
        hccp_err("dl_hal_buff_alloc_align_ex failed, length:0x%llx, dev_id:0x%x, flag:0x%lx, grp_id:%u, ret:%d",
            mrCb->len, pingCb->logic_devid, flag, rscb->grp_id, ret);
        goto alloc_fail;
    }

    mrCb->ib_mr = RsDrvMrReg(pingCb->rdev_cb.ib_pd, (char *)(uintptr_t)mrCb->addr, mrCb->len,
        IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ);
    if (mrCb->ib_mr == NULL) {
        ret = -errno;
        hccp_err("rs_ibv_reg_mr fail, ret:%d addr:0x%llx len:0x%llx", ret, mrCb->addr, mrCb->len);
        goto mr_reg_fail;
    }

    // init sge list
    mrCb->sge_list = calloc(mrCb->sge_num, sizeof(struct ibv_sge));
    if (mrCb->sge_list == NULL) {
        ret = -errno;
        hccp_err("calloc fail, ret:%d sge_num:%u", ret, mrCb->sge_num);
        goto calloc_fail;
    }
    for (idx = 0; idx < mrCb->sge_num; idx++) {
        mrCb->sge_list[idx].lkey = mrCb->ib_mr->lkey;
        mrCb->sge_list[idx].length = mrCb->payload_offset;
        if (idx == 0) {
            mrCb->sge_list[idx].addr = mrCb->addr;
        } else {
            mrCb->sge_list[idx].addr = mrCb->sge_list[idx - 1].addr + mrCb->payload_offset;
        }
    }
    mrCb->sge_idx = 0;

    hccp_info("addr:0x%llx lkey:%u ", mrCb->addr, mrCb->ib_mr->lkey);

    return 0;

calloc_fail:
    (void)RsDrvMrDereg(mrCb->ib_mr);
mr_reg_fail:
    (void)DlHalBuffFree((void *)(uintptr_t)mrCb->addr);
alloc_fail:
    (void)pthread_mutex_destroy(&mrCb->mutex);
    return ret;
}

STATIC void RsPingCommonDeinitMrCb(struct rs_ping_mr_cb *mrCb)
{
    hccp_dbg("addr:0x%llx len:%llu", mrCb->addr, mrCb->len);
    free(mrCb->sge_list);
    mrCb->sge_list = NULL;
    (void)RsDrvMrDereg(mrCb->ib_mr);
    (void)DlHalBuffFree((void *)(uintptr_t)mrCb->addr);
    (void)pthread_mutex_destroy(&mrCb->mutex);
}

STATIC int RsPingPongInitLocalBuffer(struct rs_cb *rscb, struct ping_init_attr *attr, struct ping_init_info *info,
    struct rs_ping_ctx_cb *pingCb)
{
    int ret;

    // prepare ping_qp send mr
    pingCb->ping_qp.send_mr_cb.payload_offset = PING_TOTAL_PAYLOAD_MAX_SIZE;
    pingCb->ping_qp.send_mr_cb.len = pingCb->ping_qp.qp_cap.max_send_wr * pingCb->ping_qp.send_mr_cb.payload_offset;
    pingCb->ping_qp.send_mr_cb.sge_num = pingCb->ping_qp.qp_cap.max_send_wr;
    ret = RsPingCommonInitMrCb(rscb, pingCb, &pingCb->ping_qp.send_mr_cb);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_ping_common_init_mr_cb ping_qp send_mr_cb failed, ret %d", ret), ret);
    // prepare ping_qp recv mr
    pingCb->ping_qp.recv_mr_cb.payload_offset = PING_TOTAL_PAYLOAD_MAX_SIZE;
    pingCb->ping_qp.recv_mr_cb.len = pingCb->ping_qp.qp_cap.max_recv_wr * pingCb->ping_qp.recv_mr_cb.payload_offset;
    pingCb->ping_qp.recv_mr_cb.sge_num = pingCb->ping_qp.qp_cap.max_recv_wr;
    ret = RsPingCommonInitMrCb(rscb, pingCb, &pingCb->ping_qp.recv_mr_cb);
    if (ret != 0) {
        hccp_err("rs_ping_common_init_mr_cb ping_qp recv_mr_cb failed, ret %d", ret);
        goto init_ping_qp_recv_mr_fail;
    }

    // prepare pong_qp send mr
    pingCb->pong_qp.send_mr_cb.payload_offset = PING_TOTAL_PAYLOAD_MAX_SIZE;
    pingCb->pong_qp.send_mr_cb.len = pingCb->pong_qp.qp_cap.max_send_wr * pingCb->pong_qp.send_mr_cb.payload_offset;
    pingCb->pong_qp.send_mr_cb.sge_num = pingCb->pong_qp.qp_cap.max_send_wr;
    ret = RsPingCommonInitMrCb(rscb, pingCb, &pingCb->pong_qp.send_mr_cb);
    if (ret != 0) {
        hccp_err("rs_ping_common_init_mr_cb pong_qp send_mr_cb failed, ret %d", ret);
        goto init_pong_qp_send_mr_fail;
    }
    // prepare pong_qp recv mr
    pingCb->pong_qp.recv_mr_cb.payload_offset = PING_TOTAL_PAYLOAD_MAX_SIZE;
    pingCb->pong_qp.recv_mr_cb.len = attr->buffer_size;
    pingCb->pong_qp.recv_mr_cb.sge_num = attr->buffer_size / pingCb->pong_qp.recv_mr_cb.payload_offset;
    ret = RsPingCommonInitMrCb(rscb, pingCb, &pingCb->pong_qp.recv_mr_cb);
    if (ret != 0) {
        hccp_err("rs_ping_common_init_mr_cb pong_qp recv_mr_cb failed, ret %d", ret);
        goto init_pong_qp_recv_mr_fail;
    }
    info->result.buffer_va = pingCb->pong_qp.recv_mr_cb.addr;
    info->result.buffer_size = attr->buffer_size;
    info->result.payload_offset = pingCb->pong_qp.recv_mr_cb.payload_offset;
    info->result.header_size = RS_PING_PAYLOAD_HEADER_RESV_GRH + RS_PING_PAYLOAD_HEADER_RESV_CUSTOM;

    return 0;

init_pong_qp_recv_mr_fail:
    RsPingCommonDeinitMrCb(&pingCb->pong_qp.send_mr_cb);
init_pong_qp_send_mr_fail:
    RsPingCommonDeinitMrCb(&pingCb->ping_qp.recv_mr_cb);
init_ping_qp_recv_mr_fail:
    RsPingCommonDeinitMrCb(&pingCb->ping_qp.send_mr_cb);
    return ret;
}

STATIC int RsPingCommonPostRecv(struct rs_ping_local_qp_cb *qpCb)
{
    struct ibv_recv_wr *badWr = NULL;
    struct ibv_recv_wr wr = { 0 };
    struct ibv_sge list = { 0 };
    uint32_t sgeIdx;
    int ret;

    RS_PTHREAD_MUTEX_LOCK(&qpCb->recv_mr_cb.mutex);
    sgeIdx = qpCb->recv_mr_cb.sge_idx;
    (void)memcpy_s(&list, sizeof(struct ibv_sge), &qpCb->recv_mr_cb.sge_list[sgeIdx], sizeof(struct ibv_sge));
    qpCb->recv_mr_cb.sge_idx = (sgeIdx + 1) % qpCb->recv_mr_cb.sge_num;
    RS_PTHREAD_MUTEX_ULOCK(&qpCb->recv_mr_cb.mutex);

    wr.wr_id = (uint64_t)sgeIdx;
    wr.next = NULL;
    wr.sg_list = &list;
    wr.num_sge = 1;

    ret = RsIbvPostRecv(qpCb->ib_qp, &wr, &badWr);
    if (ret != 0) {
        hccp_err("rs_ibv_post_recv failed, ret:%d", ret);
        return ret;
    }

    return 0;
}

STATIC int RsPingCommonInitPostRecvAll(struct rs_ping_local_qp_cb *qpCb)
{
    int ret = 0;
    uint32_t i;

    // prepare RQ wqe
    for (i = qpCb->recv_mr_cb.sge_idx; i < qpCb->recv_mr_cb.sge_num && i < qpCb->qp_cap.max_recv_wr; i++) {
        ret = RsPingCommonPostRecv(qpCb);
        if (ret != 0) {
            hccp_err("rs_ping_common_post_recv %u-th rqe failed, ret:%d", i, ret);
            break;
        }
    }

    return ret;
}

STATIC void RsPingCommonDeinitLocalBuffer(struct rs_ping_ctx_cb *pingCb)
{
    RsPingCommonDeinitMrCb(&pingCb->pong_qp.recv_mr_cb);
    RsPingCommonDeinitMrCb(&pingCb->pong_qp.send_mr_cb);
    RsPingCommonDeinitMrCb(&pingCb->ping_qp.recv_mr_cb);
    RsPingCommonDeinitMrCb(&pingCb->ping_qp.send_mr_cb);
}

STATIC void RsPingCommonDeinitLocalQp(struct rs_cb *rscb, struct rs_ping_ctx_cb *pingCb,
    struct rs_ping_local_qp_cb *qpCb)
{
    if (qpCb == NULL || qpCb->channel == NULL) {
        hccp_err("qp_cb is NULL or qp_cb->channel is NULL");
        return;
    }

    (void)RsIbvDestroyQp(qpCb->ib_qp);
    RsIbvAckCqEvents(qpCb->recv_cq.ib_cq, qpCb->recv_cq.num_events);
    qpCb->recv_cq.num_events = 0;
    (void)RsIbvDestroyCq(qpCb->recv_cq.ib_cq);
    (void)RsEpollCtl(rscb->conn_cb.epollfd, EPOLL_CTL_DEL, qpCb->channel->fd, EPOLLIN | EPOLLRDHUP);
    (void)RsIbvDestroyCompChannel(qpCb->channel);
    qpCb->channel = NULL;
    (void)RsIbvDestroyCq(qpCb->send_cq.ib_cq);
}

STATIC int RsPingPongInitLocalInfo(struct rs_cb *rscb, struct ping_init_attr *attr, struct ping_init_info *info,
    struct rs_ping_ctx_cb *pingCb)
{
    int ret;

    ret = RsPingCommonInitLocalQp(rscb, pingCb, &attr->client, &pingCb->ping_qp);
    CHK_PRT_RETURN(ret != 0, hccp_err("init ping_qp failed, ret:%d", ret), ret);
    info->client.version = 0;
    (void)memcpy_s(&info->client.rdma.gid, sizeof(union hccp_gid), &pingCb->rdev_cb.gid, sizeof(union ibv_gid));
    info->client.rdma.qpn = pingCb->ping_qp.ib_qp->qp_num;
    info->client.rdma.qkey = pingCb->ping_qp.qkey;

    ret = RsPingCommonInitLocalQp(rscb, pingCb, &attr->server, &pingCb->pong_qp);
    if (ret != 0) {
        hccp_err("init pong_qp failed, ret:%d", ret);
        goto init_pong_qp_fail;
    }
    info->server.version = 0;
    (void)memcpy_s(&info->server.rdma.gid, sizeof(union hccp_gid), &pingCb->rdev_cb.gid, sizeof(union ibv_gid));
    info->server.rdma.qpn = pingCb->pong_qp.ib_qp->qp_num;
    info->server.rdma.qkey = pingCb->pong_qp.qkey;

    ret = RsPingPongInitLocalBuffer(rscb, attr, info, pingCb);
    if (ret != 0) {
        hccp_err("init buffer failed, ret:%d", ret);
        goto init_buffer_fail;
    }

    ret = RsPingCommonInitPostRecvAll(&pingCb->ping_qp);
    if (ret != 0) {
        hccp_err("ping_qp post recv failed, ret:%d", ret);
        goto post_recv_fail;
    }
    ret = RsPingCommonInitPostRecvAll(&pingCb->pong_qp);
    if (ret != 0) {
        hccp_err("pong_qp post recv failed, ret:%d", ret);
        goto post_recv_fail;
    }

    return 0;

post_recv_fail:
    RsPingCommonDeinitLocalBuffer(pingCb);
init_buffer_fail:
    RsPingCommonDeinitLocalQp(rscb, pingCb, &pingCb->pong_qp);
init_pong_qp_fail:
    RsPingCommonDeinitLocalQp(rscb, pingCb, &pingCb->ping_qp);
    return ret;
}

STATIC int RsPingRocePingCbInit(unsigned int phyId, struct ping_init_attr *attr, struct ping_init_info *info,
    unsigned int *devIndex, struct rs_ping_ctx_cb *pingCb)
{
    struct rdev *rdevInfo = &attr->dev.rdma;
    struct rs_cb *rscb = NULL;
    int ret;

    ret = RsGetRsCb(phyId, &rscb);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_get_rs_cb failed, phyId[%u] invalid, ret %d", phyId, ret), ret);

    // prepare input attr
    pingCb->rdev_cb.ip.family = (uint32_t)rdevInfo->family;
    pingCb->rdev_cb.ip.bin_addr = rdevInfo->local_ip;
    ret = RsInetNtop(rdevInfo->family, &rdevInfo->local_ip, pingCb->rdev_cb.ip.read_addr, RS_MAX_IP_LEN);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_inet_ntop failed, ret %d", ret), -EINVAL);
    (void)memcpy_s(&pingCb->comm_info, sizeof(struct ping_local_comm_info), &attr->comm_info,
        sizeof(struct ping_local_comm_info));

    // open device & alloc global pd
    pingCb->rdev_cb.dev_list = RsIbvGetDeviceList(&pingCb->rdev_cb.dev_num);
    if (pingCb->rdev_cb.dev_list == NULL || pingCb->rdev_cb.dev_num == 0) {
        hccp_err("dev_list is NULL or dev_num[%d] is 0", pingCb->rdev_cb.dev_num);
        ret = -ENODEV;
        goto get_device_list_fail;
    }

    pingCb->rdev_cb.ib_port = RS_PORT_DEF;
    ret = RsPingCbGetIbCtxAndIndex(rdevInfo, pingCb);
    if (ret != 0) {
        hccp_err("rs_ping_cb_get_ib_ctx_and_index failed, ret:%d", ret);
        goto get_ib_ctx_and_index_fail;
    }

    pingCb->rdev_cb.ib_pd = RsIbvAllocPd(pingCb->rdev_cb.ib_ctx);
    if (pingCb->rdev_cb.ib_pd == NULL) {
        hccp_err("rs_ibv_alloc_pd failed, errno:%d", errno);
        ret = -ENOMEM;
        goto alloc_pd_fail;
    }

    // init cq & qp & mr info, prepare output info
    info->version = 0;
    ret = RsPingPongInitLocalInfo(rscb, attr, info, pingCb);
    if (ret != 0) {
        hccp_err("rs_ping_pong_init_local_info failed, ret=%d phyId:%u", ret, rdevInfo->phy_id);
        goto init_local_info_fail;
    }

    *devIndex = pingCb->dev_index;
    return 0;

init_local_info_fail:
    (void)RsIbvDeallocPd(pingCb->rdev_cb.ib_pd);
alloc_pd_fail:
    (void)RsIbvCloseDevice(pingCb->rdev_cb.ib_ctx);
get_ib_ctx_and_index_fail:
    RsIbvFreeDeviceList(pingCb->rdev_cb.dev_list);
get_device_list_fail:
    (void)pthread_mutex_destroy(&pingCb->ping_mutex);
    (void)pthread_mutex_destroy(&pingCb->pong_mutex);
    return ret;
}

STATIC bool RsPingCommonCompareRdmaInfo(struct ping_qp_info *a, struct ping_qp_info *b)
{
    if (a->rdma.qpn != b->rdma.qpn) {
        return false;
    }
    if (a->rdma.qkey != b->rdma.qkey) {
        return false;
    }
    if (memcmp(&a->rdma.gid, &b->rdma.gid, sizeof(union hccp_gid)) != 0) {
        return false;
    }
    return true;
}

STATIC int RsPingRoceFindTargetNode(struct rs_ping_ctx_cb *pingCb, struct ping_qp_info *target,
    struct rs_ping_target_info **node)
{
    struct rs_ping_target_info *targetNext = NULL;
    struct rs_ping_target_info *targetCurr = NULL;

    RS_PTHREAD_MUTEX_LOCK(&pingCb->ping_mutex);
    RS_LIST_GET_HEAD_ENTRY(targetCurr, targetNext, &pingCb->ping_list, list, struct rs_ping_target_info);
    for (; (&targetCurr->list) != &pingCb->ping_list;
        targetCurr = targetNext, targetNext = list_entry(targetNext->list.next, struct rs_ping_target_info, list)) {
        if (RsPingCommonCompareRdmaInfo(&targetCurr->qp_info, target)) {
            *node = targetCurr;
            RS_PTHREAD_MUTEX_ULOCK(&pingCb->ping_mutex);
            return 0;
        }
    }
    RS_PTHREAD_MUTEX_ULOCK(&pingCb->ping_mutex);

    hccp_info("ping target node for qpn:%u gid:%016llx:%016llx not found", target->rdma.qpn,
        target->rdma.gid.global.subnet_prefix, target->rdma.gid.global.interface_id);
    return -ENODEV;
}

STATIC int RsPingCommonCreateAh(struct rs_ping_ctx_cb *pingCb, struct ping_local_comm_info *localInfo,
    struct ping_qp_info *remoteInfo, struct ibv_ah **ah)
{
    struct ibv_exp_ah_attr attrx = { 0 };
    struct ibv_global_route grh = { 0 };
    struct ibv_ah_attr attr = { 0 };
    struct ibv_ah *ahTmp = NULL;
    int ret = 0;

    (void)memcpy_s(&grh.dgid, sizeof(union ibv_gid), &remoteInfo->rdma.gid, sizeof(union hccp_gid));
    grh.flow_label = localInfo->rdma.flow_label;
    grh.sgid_index = (uint8_t)pingCb->rdev_cb.gid_idx;
    grh.hop_limit = localInfo->rdma.hop_limit;
    grh.traffic_class = localInfo->rdma.qos_attr.tc;

    attr.grh = grh;
    attr.sl = localInfo->rdma.qos_attr.sl;
    attr.is_global = 1;
    attr.port_num = pingCb->rdev_cb.ib_port;
    attrx.attr = attr;
    attrx.udp_sport = localInfo->rdma.udp_sport;

    hccp_dbg("remote_qpn:%u flow_label:%u sgid_index:%u hop_limit:%u traffic_class:%u sl:%u is_global:%u "
        "port_num:%u udp_sport:%u", remoteInfo->rdma.qpn, grh.flow_label, grh.sgid_index, grh.hop_limit,
        grh.traffic_class, attr.sl, attr.is_global, attr.port_num, attrx.udp_sport);

    ahTmp = RsIbvExpCreateAh(pingCb->rdev_cb.ib_pd, &attrx);
    if (ahTmp == NULL) {
        ret = -errno;
        hccp_err("rs_ibv_exp_create_ah failed, errno:%d", ret);
        return ret;
    }

    *ah = ahTmp;
    return ret;
}

STATIC int RsPingRoceAllocTargetNode(struct rs_ping_ctx_cb *pingCb, struct ping_target_info *target,
    struct rs_ping_target_info **node)
{
    struct rs_ping_target_info *targetInfo = NULL;
    int ret;

    targetInfo = (struct rs_ping_target_info *)calloc(1, sizeof(struct rs_ping_target_info));
    CHK_PRT_RETURN(targetInfo == NULL, hccp_err("calloc target_info fail! errno:%d", errno), -ENOMEM);

    ret = pthread_mutex_init(&targetInfo->trip_mutex, NULL);
    if (ret != 0) {
        hccp_err("pthread_mutex_init trip_mutex failed, ret:%d", ret);
        goto free_target_info;
    }

    targetInfo->payload_size = target->payload.size;
    if (target->payload.size > 0) {
        targetInfo->payload_buffer = (char *)calloc(1, target->payload.size);
        if (targetInfo->payload_buffer == NULL) {
            hccp_err("calloc payload_buffer fail! size:%u errno:%d", target->payload.size, errno);
            ret = -ENOMEM;
            goto free_trip_mutex;
        }
        (void)memcpy_s(targetInfo->payload_buffer, target->payload.size, target->payload.buffer, target->payload.size);
    }

    (void)memcpy_s(&targetInfo->qp_info, sizeof(struct ping_qp_info),
        &target->remote_info.qp_info, sizeof(struct ping_qp_info));
    ret = RsPingCommonCreateAh(pingCb, &target->local_info, &target->remote_info.qp_info, &targetInfo->ah);
    if (ret != 0) {
        hccp_err("rs_ping_common_create_ah fail! ret:%d", ret);
        goto free_payload_buffer;
    }

    targetInfo->result_summary.rtt_min = ~0;
    targetInfo->state = RS_PING_PONG_TARGET_READY;
    *node = targetInfo;

    return 0;
free_payload_buffer:
    if (target->payload.size > 0 && targetInfo->payload_buffer != NULL) {
        free(targetInfo->payload_buffer);
        targetInfo->payload_buffer = NULL;
    }
free_trip_mutex:
    (void)pthread_mutex_destroy(&targetInfo->trip_mutex);
free_target_info:
    free(targetInfo);
    targetInfo = NULL;
    return ret;
}

STATIC void RsPingRoceResetRecvBuffer(struct rs_ping_ctx_cb *pingCb)
{
    RS_PTHREAD_MUTEX_LOCK(&pingCb->pong_qp.recv_mr_cb.mutex);
    (void)memset_s((void *)(uintptr_t)pingCb->pong_qp.recv_mr_cb.addr, pingCb->pong_qp.recv_mr_cb.len,
        0, pingCb->pong_qp.recv_mr_cb.len);
    RS_PTHREAD_MUTEX_ULOCK(&pingCb->pong_qp.recv_mr_cb.mutex);
}

STATIC void RsPingQpBuildUpWr(struct rs_ping_target_info *target, struct ibv_sge *list, struct ibv_send_wr *wr)
{
    wr->wr_id = target->uuid;
    wr->next = NULL;
    wr->sg_list = list;
    wr->num_sge = 1;
    wr->opcode = IBV_WR_SEND;
    wr->send_flags = IBV_SEND_SIGNALED;
    wr->wr.ud.ah = target->ah;
    wr->wr.ud.remote_qpn = target->qp_info.rdma.qpn;
    wr->wr.ud.remote_qkey = target->qp_info.rdma.qkey;
}

STATIC int RsPingRocePostSend(struct rs_ping_ctx_cb *pingCb, struct rs_ping_target_info *target)
{
    struct rs_ping_payload_header *header = NULL;
    struct ibv_send_wr *badWr = NULL;
    struct timeval timestamp = { 0 };
    struct ibv_send_wr wr = { 0 };
    struct ibv_sge list = { 0 };
    uint32_t sgeIdx;
    int ret = 0;

    hccp_dbg("target uuid:0x%llx state:%d payload_size:%u qpn:%u gid:%016llx:%016llx",
        target->uuid, target->state, target->payload_size, target->qp_info.rdma.qpn,
        target->qp_info.rdma.gid.global.subnet_prefix, target->qp_info.rdma.gid.global.interface_id);

    RS_PTHREAD_MUTEX_LOCK(&pingCb->ping_qp.send_mr_cb.mutex);
    sgeIdx = pingCb->ping_qp.send_mr_cb.sge_idx;
    (void)memcpy_s(&list, sizeof(struct ibv_sge),
        &pingCb->ping_qp.send_mr_cb.sge_list[sgeIdx], sizeof(struct ibv_sge));
    pingCb->ping_qp.send_mr_cb.sge_idx = (sgeIdx + 1) % pingCb->ping_qp.send_mr_cb.sge_num;
    RS_PTHREAD_MUTEX_ULOCK(&pingCb->ping_qp.send_mr_cb.mutex);

    // prepare ping_qp send buffer
    (void)memset_s((void *)(uintptr_t)list.addr, list.length, 0, list.length);
    header = (struct rs_ping_payload_header *)(uintptr_t)list.addr;
    header->type = RS_PING_TYPE_ROCE_DETECT;
    (void)memcpy_s(&header->server.rdma.gid, sizeof(union hccp_gid), &pingCb->rdev_cb.gid, sizeof(union ibv_gid));
    header->server.rdma.qpn = pingCb->pong_qp.ib_qp->qp_num;
    header->server.rdma.qkey = pingCb->pong_qp.qkey;
    (void)memcpy_s(&header->target, sizeof(struct ping_qp_info), &target->qp_info, sizeof(struct ping_qp_info));

    if (target->payload_size > 0) {
        ret = memcpy_s((void *)(uintptr_t)(list.addr + RS_PING_PAYLOAD_HEADER_RESV_CUSTOM),
            (list.length - RS_PING_PAYLOAD_HEADER_RESV_CUSTOM),
            (void *)target->payload_buffer, target->payload_size);
        CHK_PRT_RETURN(ret != 0, hccp_err("memcpy_s buffer payload_size:%u list.length:%u failed, ret:%d",
            target->payload_size, (list.length - RS_PING_PAYLOAD_HEADER_RESV_CUSTOM), ret), -ESAFEFUNC);
    }
    list.length = RS_PING_PAYLOAD_HEADER_RESV_CUSTOM + target->payload_size;

    RsPingQpBuildUpWr(target, &list, &wr);

    // record timestamp t1
    (void)gettimeofday(&timestamp, NULL);
    header->timestamp.tv_sec1 = (uint64_t)timestamp.tv_sec;
    header->timestamp.tv_usec1 = (uint64_t)timestamp.tv_usec;
    header->task_id = pingCb->task_id;
    header->magic = 0x55AA;

    ret = RsIbvPostSend(pingCb->ping_qp.ib_qp, &wr, &badWr);
    if (ret != 0) {
        hccp_err("rs_ibv_post_send qpn:%u failed, ret:%d", pingCb->ping_qp.ib_qp->qp_num, ret);
        RS_PTHREAD_MUTEX_LOCK(&target->trip_mutex);
        target->state = RS_PING_PONG_TARGET_ERROR;
        RS_PTHREAD_MUTEX_ULOCK(&target->trip_mutex);
    }
    return ret;
}

STATIC int RsPingRocePollScq(struct rs_ping_ctx_cb *pingCb, struct rs_ping_target_info *target)
{
    struct ibv_wc wc = { 0 };
    int polledCnt;

    polledCnt = RsIbvPollCq(pingCb->ping_qp.send_cq.ib_cq, 1, &wc);
    if (polledCnt != 1) {
        hccp_err("uuid:0x%llx rs_ibv_poll_cq polled_cnt:%d", target->uuid, polledCnt);
        target->state = RS_PING_PONG_TARGET_ERROR;
        return -ENODATA;
    }
    if (wc.status != IBV_WC_SUCCESS) {
        target->state = RS_PING_PONG_TARGET_ERROR;
        hccp_err("wr_id:0x%llx error cqe %s(%d)", wc.wr_id, RsIbvWcStatusStr(wc.status), wc.status);
        return -EOPENSRC;
    }
    return 0;
}

STATIC int RsPingRocePollRcq(struct rs_ping_ctx_cb *pingCb, int *polledCnt, struct timeval *timestamp2)
{
    struct ibv_cq *evCq = NULL;
    void *evCtx = NULL;
    int ret;

    // record timestamp t2
    (void)gettimeofday(timestamp2, NULL);

    ret = RsIbvGetCqEvent(pingCb->ping_qp.channel, &evCq, &evCtx);
    if (ret != 0) {
        hccp_err("rs_ibv_get_cq_event ping_qp.channel failed, ret:%d", ret);
        return -EOPENSRC;
    }

    if (evCq != pingCb->ping_qp.recv_cq.ib_cq) {
        hccp_err("CQ event for unknown CQ");
        return -EOPENSRC;
    }
    pingCb->ping_qp.recv_cq.num_events++;

    *polledCnt = RsIbvPollCq(evCq, pingCb->ping_qp.recv_cq.max_recv_wc_num, gPingQpRecvWc);
    CHK_PRT_RETURN(*polledCnt > pingCb->ping_qp.recv_cq.max_recv_wc_num || *polledCnt < 0,
        hccp_err("ping_poll_rcq failed, ret:%d", *polledCnt), -EOPENSRC);

    return 0;
}

STATIC int RsPingCommonPollScq(struct rs_ping_local_qp_cb *qpCb)
{
    struct ibv_wc wc = { 0 };
    int polledCnt;

    polledCnt = RsIbvPollCq(qpCb->send_cq.ib_cq, 1, &wc);
    if (polledCnt < 0) {
        hccp_warn("rs_ibv_poll_cq unsuccessful, polledCnt:%d", polledCnt);
    } else if (polledCnt > 0) {
        if (wc.status != IBV_WC_SUCCESS) {
            hccp_err("wr_id:0x%llx error cqe %s(%d)", wc.wr_id, RsIbvWcStatusStr(wc.status), wc.status);
            return -EOPENSRC;
        }
    }

    return 0;
}

STATIC int RsPongFindTargetNode(struct rs_ping_ctx_cb *pingCb, struct ping_qp_info *target,
    struct rs_pong_target_info **node)
{
    struct rs_pong_target_info *targetNext = NULL;
    struct rs_pong_target_info *targetCurr = NULL;

    RS_CHECK_POINTER_NULL_WITH_RET(pingCb);
    RS_PTHREAD_MUTEX_LOCK(&pingCb->pong_mutex);
    RS_LIST_GET_HEAD_ENTRY(targetCurr, targetNext, &pingCb->pong_list, list, struct rs_pong_target_info);
    for (; (&targetCurr->list) != &pingCb->pong_list;
        targetCurr = targetNext, targetNext = list_entry(targetNext->list.next, struct rs_pong_target_info, list)) {
        if (RsPingCommonCompareRdmaInfo(&targetCurr->qp_info, target)) {
            *node = targetCurr;
            RS_PTHREAD_MUTEX_ULOCK(&pingCb->pong_mutex);
            return 0;
        }
    }
    RS_PTHREAD_MUTEX_ULOCK(&pingCb->pong_mutex);

    hccp_info("pong target node for qpn:%u gid:%016llx:%016llx not found", target->rdma.qpn,
        target->rdma.gid.global.subnet_prefix, target->rdma.gid.global.interface_id);
    return -ENODEV;
}

STATIC int RsPongFindAllocTargetNode(struct rs_ping_ctx_cb *pingCb, struct ping_qp_info *target,
    struct rs_pong_target_info **node)
{
    struct rs_pong_target_info *targetInfo = NULL;
    int ret;

    ret = RsPongFindTargetNode(pingCb, target, node);
    if (ret == 0 && (*node)->state == RS_PING_PONG_TARGET_READY) {
        return 0;
    } else if (ret == 0) {
        targetInfo = *node;
        hccp_info("delete pong target uuid:0x%llx state:%d, realloc again", targetInfo->uuid, targetInfo->state);
        RsListDel(&targetInfo->list);
        if (targetInfo->ah) {
            (void)RsIbvDestroyAh(targetInfo->ah);
        }
        free(targetInfo);
        targetInfo = NULL;
    }

    targetInfo = (struct rs_pong_target_info *)calloc(1, sizeof(struct rs_pong_target_info));
    CHK_PRT_RETURN(targetInfo == NULL, hccp_err("calloc target_info fail! errno:%d", errno), -ENOMEM);

    (void)memcpy_s(&targetInfo->qp_info, sizeof(struct ping_qp_info), target, sizeof(struct ping_qp_info));
    ret = RsPingCommonCreateAh(pingCb, &pingCb->comm_info, target, &targetInfo->ah);
    if (ret != 0) {
        hccp_err("rs_ping_common_create_ah fail! ret:%d", ret);
        goto free_target_info;
    }

    targetInfo->state = RS_PING_PONG_TARGET_READY;
    *node = targetInfo;

    RS_PTHREAD_MUTEX_LOCK(&pingCb->pong_mutex);
    targetInfo->uuid = (uint64_t)pingCb->pong_num << 32U;
    RsListAddTail(&targetInfo->list, &pingCb->pong_list);
    pingCb->pong_num++;
    RS_PTHREAD_MUTEX_ULOCK(&pingCb->pong_mutex);

    return 0;

free_target_info:
    free(targetInfo);
    return ret;
}

STATIC int RsPongPostSend(struct rs_ping_ctx_cb *pingCb, struct ibv_wc *wc, struct timeval *timestamp2)
{
    struct rs_pong_target_info *targetInfo = NULL;
    struct rs_ping_payload_header *header = NULL;
    struct ibv_send_wr *badWr = NULL;
    struct timeval timestamp3 = { 0 };
    struct ibv_sge recvList = { 0 };
    struct ibv_sge sendList = { 0 };
    struct ibv_send_wr wr = { 0 };
    uint32_t recvSgeIdx;
    uint32_t sendSgeIdx;
    int ret = 0;

    // poll send cq
    (void)RsPingCommonPollScq(&pingCb->pong_qp);

    // handle detect packet & send response packet
    recvSgeIdx = (uint32_t)wc->wr_id;
    if (recvSgeIdx > pingCb->ping_qp.recv_mr_cb.sge_num) {
        hccp_err("param err recv_sge_idx:%u > sge_num:%u", recvSgeIdx, pingCb->ping_qp.recv_mr_cb.sge_num);
        return -EIO;
    }
    (void)memcpy_s(&recvList, sizeof(struct ibv_sge),
        &pingCb->ping_qp.recv_mr_cb.sge_list[recvSgeIdx], sizeof(struct ibv_sge));

    RS_PTHREAD_MUTEX_LOCK(&pingCb->pong_qp.send_mr_cb.mutex);
    sendSgeIdx = pingCb->pong_qp.send_mr_cb.sge_idx;
    (void)memcpy_s(&sendList, sizeof(struct ibv_sge),
        &pingCb->pong_qp.send_mr_cb.sge_list[sendSgeIdx], sizeof(struct ibv_sge));
    pingCb->pong_qp.send_mr_cb.sge_idx = (sendSgeIdx + 1) % pingCb->pong_qp.send_mr_cb.sge_num;
    RS_PTHREAD_MUTEX_ULOCK(&pingCb->pong_qp.send_mr_cb.mutex);

    // UD consume 40 Bytes for GRH
    if (wc->byte_len < RS_PING_PAYLOAD_HEADER_RESV_GRH || wc->byte_len > PING_TOTAL_PAYLOAD_MAX_SIZE) {
        hccp_err("param err wc->byte_len:%u < %u or wc->byte_len:%u > %u", wc->byte_len,
            RS_PING_PAYLOAD_HEADER_RESV_GRH, wc->byte_len, PING_TOTAL_PAYLOAD_MAX_SIZE);
        return -EIO;
    }
    ret = memcpy_s((void *)(uintptr_t)sendList.addr, sendList.length,
        (void *)(uintptr_t)(recvList.addr + RS_PING_PAYLOAD_HEADER_RESV_GRH),
        wc->byte_len - RS_PING_PAYLOAD_HEADER_RESV_GRH);
    CHK_PRT_RETURN(ret != 0, hccp_err("memcpy_s buffer wc->byte_len:%u send_list.length:%u failed, ret:%d",
        wc->byte_len, sendList.length, ret), -ESAFEFUNC);
    sendList.length = wc->byte_len - RS_PING_PAYLOAD_HEADER_RESV_GRH;
    header = (struct rs_ping_payload_header *)(uintptr_t)sendList.addr;
    header->type = RS_PING_TYPE_ROCE_RESPONSE;

    ret = RsPongFindAllocTargetNode(pingCb, &header->server, &targetInfo);
    if (ret != 0) {
        hccp_err("rs_pong_find_alloc_target_node failed, ret:%d", ret);
        return ret;
    }

    wr.wr_id = targetInfo->uuid;
    wr.next = NULL;
    wr.sg_list = &sendList;
    wr.num_sge = 1;
    wr.opcode = IBV_WR_SEND;
    wr.send_flags = IBV_SEND_SIGNALED;
    wr.wr.ud.ah = targetInfo->ah;
    wr.wr.ud.remote_qpn = targetInfo->qp_info.rdma.qpn;
    wr.wr.ud.remote_qkey = targetInfo->qp_info.rdma.qkey;

    // record timestamp t3
    (void)gettimeofday(&timestamp3, NULL);
    header->timestamp.tv_sec2 = (uint64_t)timestamp2->tv_sec;
    header->timestamp.tv_usec2 = (uint64_t)timestamp2->tv_usec;
    header->timestamp.tv_sec3 = (uint64_t)timestamp3.tv_sec;
    header->timestamp.tv_usec3 = (uint64_t)timestamp3.tv_usec;
    header->magic = 0xAA55;

    ret = RsIbvPostSend(pingCb->pong_qp.ib_qp, &wr, &badWr);
    if (ret != 0) {
        targetInfo->state = RS_PING_PONG_TARGET_ERROR;
        hccp_err("rs_ibv_post_send failed, ret:%d", ret);
        return ret;
    }

    return ret;
}

STATIC void RsPongRoceHandleSend(struct rs_ping_ctx_cb *pingCb, int polledCnt, struct timeval *timestamp2)
{
    struct ibv_wc *wc = NULL;
    int ret, i;

    wc = gPingQpRecvWc;
    for (i = 0; i < polledCnt; i++) {
        if (wc[i].status != IBV_WC_SUCCESS) {
            hccp_err("wr_id:0x%llx error cqe %s(%d)", wc[i].wr_id, RsIbvWcStatusStr(wc[i].status), wc[i].status);
            continue;
        }

        ret = RsPongPostSend(pingCb, &wc[i], timestamp2);
        if (ret != 0) {
            hccp_err("rs_pong_post_send failed, wr_id:0x%llx", wc[i].wr_id);
            continue;
        }

        ret = RsPingCommonPostRecv(&pingCb->ping_qp);
        if (ret != 0) {
            hccp_err("rs_ping_common_post_recv failed, ret:%d", ret);
            continue;
        }
    }

    ret = RsIbvReqNotifyCq(pingCb->ping_qp.recv_cq.ib_cq, 0);
    if (ret != 0) {
        hccp_err("rs_ibv_req_notify_cq failed, ret:%d", ret);
    }

    return;
}

STATIC int RsPongResolveResponsePacket(struct rs_ping_ctx_cb *pingCb, uint32_t sgeIdx, struct timeval *timestamp4)
{
    struct rs_ping_target_info *targetInfo = NULL;
    struct rs_ping_payload_header *header = NULL;
    struct ibv_sge *recvList = NULL;
    uint32_t rtt;
    int ret;

    recvList = &pingCb->pong_qp.recv_mr_cb.sge_list[sgeIdx];
    // UD consume 40 Bytes for GRH
    header = (struct rs_ping_payload_header *)(uintptr_t)(recvList->addr + RS_PING_PAYLOAD_HEADER_RESV_GRH);
    if (header->task_id != pingCb->task_id) {
        hccp_warn("drop received packet, recv_task_id:%u, curr_task_id:%u", header->task_id, pingCb->task_id);
        return 0;
    }

    header->timestamp.tv_sec4 = (uint64_t)timestamp4->tv_sec;
    header->timestamp.tv_usec4 = (uint64_t)timestamp4->tv_usec;
    rtt = RsPingGetTripTime(&header->timestamp);
    ret = RsPingRoceFindTargetNode(pingCb, &header->target, &targetInfo);
    if (ret != 0) {
        hccp_err("rs_ping_roce_find_target_node failed, ret:%d qpn:%u gid:%016llx:%016llx rtt:%u", ret,
            header->target.rdma.qpn, header->target.rdma.gid.global.subnet_prefix,
            header->target.rdma.gid.global.interface_id, rtt);
        return ret;
    }

    (void)memset_s((void *)header, RS_PING_PAYLOAD_HEADER_MASK_SIZE, 0, RS_PING_PAYLOAD_HEADER_MASK_SIZE);
    RS_PTHREAD_MUTEX_LOCK(&targetInfo->trip_mutex);
    targetInfo->result_summary.recv_cnt++;
    targetInfo->result_summary.task_id = header->task_id;
    // rtt timeout, increase timeout_cnt
    if ((targetInfo->result_summary.task_attr.timeout_interval * RS_PING_MSEC_TO_USEC) < rtt) {
        targetInfo->result_summary.timeout_cnt++;
        hccp_dbg("recv_cnt:%u timeout_interval:%u rtt:%u timeout_cnt:%u", targetInfo->result_summary.recv_cnt,
            targetInfo->result_summary.task_attr.timeout_interval, rtt, targetInfo->result_summary.timeout_cnt);
        RS_PTHREAD_MUTEX_ULOCK(&targetInfo->trip_mutex);
        return 0;
    }

    // handle rtt_min, rtt_max, rtt_avg
    if (targetInfo->result_summary.rtt_min > rtt) {
        targetInfo->result_summary.rtt_min = rtt;
    }
    if (targetInfo->result_summary.rtt_max < rtt) {
        targetInfo->result_summary.rtt_max = rtt;
    }
    if (targetInfo->result_summary.rtt_avg == 0) {
        targetInfo->result_summary.rtt_avg = rtt;
    }
    targetInfo->result_summary.rtt_avg = (targetInfo->result_summary.rtt_avg + rtt) / 2U;
    RS_PTHREAD_MUTEX_ULOCK(&targetInfo->trip_mutex);
    return 0;
}

STATIC void RsPongRocePollRcq(struct rs_ping_ctx_cb *pingCb)
{
    struct timeval timestamp = { 0 };
    struct ibv_cq *evCq = NULL;
    struct ibv_wc *wc = NULL;
    uint32_t recvSgeIdx;
    void *evCtx = NULL;
    int polledCnt, i;
    int ret;

    // record timestamp t4
    (void)gettimeofday(&timestamp, NULL);

    ret = RsIbvGetCqEvent(pingCb->pong_qp.channel, &evCq, &evCtx);
    if (ret != 0) {
        hccp_err("rs_ibv_get_cq_event pong_qp.channel failed, ret:%d", ret);
        return;
    }

    if (evCq != pingCb->pong_qp.recv_cq.ib_cq) {
        hccp_err("CQ event for unknown CQ");
        return;
    }
    pingCb->pong_qp.recv_cq.num_events++;

    polledCnt = RsIbvPollCq(evCq, pingCb->pong_qp.recv_cq.max_recv_wc_num, gPongQpRecvWc);
    if (polledCnt > pingCb->pong_qp.recv_cq.max_recv_wc_num || polledCnt < 0) {
        hccp_err("rs_ibv_poll_cq failed, ret:%d", polledCnt);
        return;
    }

    wc = gPongQpRecvWc;
    for (i = 0; i < polledCnt; i++) {
        if (wc[i].status != IBV_WC_SUCCESS) {
            hccp_err("wr_id:0x%llx error cqe %s(%d)", wc[i].wr_id, RsIbvWcStatusStr(wc[i].status), wc[i].status);
            continue;
        }
        recvSgeIdx = (uint32_t)wc[i].wr_id;
        if (recvSgeIdx >= pingCb->pong_qp.recv_mr_cb.sge_num) {
            hccp_err("param err recv_sge_idx:%u > sge_num:%u", recvSgeIdx, pingCb->pong_qp.recv_mr_cb.sge_num);
            continue;
        }

        // handle response packet result
        ret = RsPongResolveResponsePacket(pingCb, recvSgeIdx, &timestamp);
        if (ret != 0) {
            continue;
        }

        ret = RsPingCommonPostRecv(&pingCb->pong_qp);
        if (ret != 0) {
            continue;
        }
    }

    ret = RsIbvReqNotifyCq(evCq, 0);
    if (ret != 0) {
        hccp_err("rs_ibv_req_notify_cq failed, ret:%d", ret);
    }

    return;
}

STATIC int RsPingRoceGetTargetResult(struct rs_ping_ctx_cb *pingCb, struct ping_target_comm_info *target,
    struct ping_result_info *result)
{
    struct rs_ping_target_info *targetInfo = NULL;
    int ret;

    ret = RsPingRoceFindTargetNode(pingCb, &target->qp_info, &targetInfo);
    if (ret != 0) {
        hccp_err("rs_ping_roce_find_target_node failed, ret:%d qpn:%u gid:%016llx:%016llx", ret,
            target->qp_info.rdma.qpn, target->qp_info.rdma.gid.global.subnet_prefix,
            target->qp_info.rdma.gid.global.interface_id);
        return ret;
    }

    (void)memcpy_s(&result->summary, sizeof(struct ping_result_summary), &targetInfo->result_summary,
        sizeof(struct ping_result_summary));
    if (targetInfo->state == RS_PING_PONG_TARGET_FINISH) {
        result->state = PING_RESULT_STATE_VALID;
    } else {
        result->state = PING_RESULT_STATE_INVALID;
    }

    hccp_dbg("ip:0x%llx qpn:%u, state:%d send_cnt:%u recv_cnt:%u timeout_cnt:%u rtt_min:%u rtt_max:%u rtt_avg:%u",
        target->ip.addr.s_addr, target->qp_info.rdma.qpn, result->state, result->summary.send_cnt,
        result->summary.recv_cnt, result->summary.timeout_cnt, result->summary.rtt_min, result->summary.rtt_max,
        result->summary.rtt_avg);

    return 0;
}

STATIC void RsPingRoceFreeTargetNode(struct rs_ping_ctx_cb *pingCb, struct rs_ping_target_info *targetInfo)
{
    RS_PTHREAD_MUTEX_LOCK(&pingCb->ping_mutex);
    RsListDel(&targetInfo->list);
    RS_PTHREAD_MUTEX_ULOCK(&pingCb->ping_mutex);

    if (targetInfo->payload_size > 0 && targetInfo->payload_buffer != NULL) {
        free(targetInfo->payload_buffer);
        targetInfo->payload_buffer = NULL;
    }

    if (targetInfo->ah) {
        (void)RsIbvDestroyAh(targetInfo->ah);
    }
    return;
}

STATIC void RsPingPongDelTargetList(struct rs_ping_ctx_cb *pingCb)
{
    struct rs_pong_target_info *pongNext = NULL;
    struct rs_ping_target_info *pingNext = NULL;
    struct rs_pong_target_info *pongCurr = NULL;
    struct rs_ping_target_info *pingCurr = NULL;

    // del ping_list
    RS_PTHREAD_MUTEX_LOCK(&pingCb->ping_mutex);
    RS_LIST_GET_HEAD_ENTRY(pingCurr, pingNext, &pingCb->ping_list, list, struct rs_ping_target_info);
    for (; (&pingCurr->list) != &pingCb->ping_list;
        pingCurr = pingNext, pingNext = list_entry(pingNext->list.next, struct rs_ping_target_info, list)) {
        RsListDel(&pingCurr->list);
        if (pingCurr->payload_size > 0 && pingCurr->payload_buffer != NULL) {
            free(pingCurr->payload_buffer);
            pingCurr->payload_buffer = NULL;
        }
        if (pingCurr->ah) {
            (void)RsIbvDestroyAh(pingCurr->ah);
        }
        (void)pthread_mutex_destroy(&pingCurr->trip_mutex);
        free(pingCurr);
        pingCurr = NULL;
    }
    RS_PTHREAD_MUTEX_ULOCK(&pingCb->ping_mutex);

    // del pong_list
    RS_PTHREAD_MUTEX_LOCK(&pingCb->pong_mutex);
    RS_LIST_GET_HEAD_ENTRY(pongCurr, pongNext, &pingCb->pong_list, list, struct rs_pong_target_info);
    for (; (&pongCurr->list) != &pingCb->pong_list;
        pongCurr = pongNext, pongNext = list_entry(pongNext->list.next, struct rs_pong_target_info, list)) {
        RsListDel(&pongCurr->list);
        if (pongCurr->ah) {
            (void)RsIbvDestroyAh(pongCurr->ah);
        }
        free(pongCurr);
        pongCurr = NULL;
    }
    RS_PTHREAD_MUTEX_ULOCK(&pingCb->pong_mutex);
}

STATIC void RsPingRocePingCbDeinit(unsigned int phyId, struct rs_ping_ctx_cb *pingCb)
{
    struct rs_cb *rscb = NULL;
    int ret;

    ret = RsGetRsCb(phyId, &rscb);
    if (ret != 0) {
        hccp_err("rs_get_rs_cb failed, phyId[%u] invalid, ret %d", phyId, ret);
        return;
    }

    RS_PTHREAD_MUTEX_LOCK(&pingCb->ping_mutex);
    pingCb->task_status = RS_PING_TASK_RESET;
    RS_PTHREAD_MUTEX_ULOCK(&pingCb->ping_mutex);

    RsPingPongDelTargetList(pingCb);

    RsPingCommonDeinitLocalQp(rscb, pingCb, &pingCb->pong_qp);
    RsPingCommonDeinitLocalQp(rscb, pingCb, &pingCb->ping_qp);
    RsPingCommonDeinitLocalBuffer(pingCb);
    (void)RsIbvDeallocPd(pingCb->rdev_cb.ib_pd);
    (void)RsIbvCloseDevice(pingCb->rdev_cb.ib_ctx);
    RsIbvFreeDeviceList(pingCb->rdev_cb.dev_list);
}

STATIC void RsPingRoceAddTargetSuccess(struct ping_target_info *target, struct rs_ping_target_info *targetInfo)
{
    hccp_info("target ip:0x%llx payload_size:%u add success, qpn:%u uuid:0x%llx",
        target->remote_info.ip.addr.s_addr, target->payload.size, targetInfo->qp_info.rdma.qpn, targetInfo->uuid);
}

STATIC void RsPingRocePingCbInitSuccess(unsigned int phyId, struct ping_init_attr *attr, unsigned int devIndex)
{
    hccp_run_info("ping_cb init success, phyId:%u, local_ip:0x%x, devIndex:%u",
        phyId, attr->dev.rdma.local_ip.addr.s_addr, devIndex);
}

STATIC void RsPingRoceCannotFindTargetNode(unsigned int i, int ret, struct ping_target_comm_info target,
    unsigned int phyId)
{
    hccp_err("rs_ping_roce_find_target_node i:%u failed, ret:%d ip:0x%llx qpn:%u phyId:%u",i, ret,
        target.ip.addr.s_addr, target.qp_info.rdma.qpn, phyId);
}

struct rs_ping_pong_ops gRsPingRoceOps = {
    .check_ping_fd          = RsPingRoceCheckFd,
    .check_pong_fd          = RsPongRoceCheckFd,
    .init_ping_cb           = RsPingRocePingCbInit,
    .ping_find_target_node  = RsPingRoceFindTargetNode,
    .ping_alloc_target_node = RsPingRoceAllocTargetNode,
    .reset_recv_buffer      = RsPingRoceResetRecvBuffer,
    .ping_post_send         = RsPingRocePostSend,
    .ping_poll_scq          = RsPingRocePollScq,
    .ping_poll_rcq          = RsPingRocePollRcq,
    .pong_handle_send       = RsPongRoceHandleSend,
    .pong_poll_rcq          = RsPongRocePollRcq,
    .get_target_result      = RsPingRoceGetTargetResult,
    .ping_free_target_node  = RsPingRoceFreeTargetNode,
    .deinit_ping_cb         = RsPingRocePingCbDeinit,
};

struct rs_ping_pong_dfx gRsPingRoceDfx = {
    .add_target_success           = RsPingRoceAddTargetSuccess,
    .init_ping_cb_success         = RsPingRocePingCbInitSuccess,
    .ping_cannot_find_target_node = RsPingRoceCannotFindTargetNode,
};

struct rs_ping_pong_ops *RsPingRoceGetOps(void) {
    return &gRsPingRoceOps;
}

struct rs_ping_pong_dfx *RsPingRoceGetDfx(void) {
    return &gRsPingRoceDfx;
}

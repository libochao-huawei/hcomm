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

struct ibv_wc g_ping_qp_recv_wc[RS_PING_ROCE_RECV_WC_NUM] = { 0 };
struct ibv_wc g_pong_qp_recv_wc[RS_PING_ROCE_RECV_WC_NUM] = { 0 };

STATIC bool rs_ping_roce_check_fd(struct rs_ping_ctx_cb *ping_cb, int fd)
{
    if (ping_cb->ping_qp.channel != NULL && ping_cb->ping_qp.channel->fd == fd) {
        hccp_dbg("ping_qp rq, channel->fd:%d poll cq", fd);
        return true;
    }
    return false;
}

STATIC bool rs_pong_roce_check_fd(struct rs_ping_ctx_cb *ping_cb, int fd)
{
    if (ping_cb->pong_qp.channel != NULL && ping_cb->pong_qp.channel->fd == fd) {
        hccp_dbg("pong_qp rq, channel->fd:%d poll cq", fd);
        return true;
    }
    return false;
}

STATIC int rs_ping_cb_get_dev_rdev_index(struct rs_ping_ctx_cb *ping_cb, int index)
{
#ifdef CUSTOM_INTERFACE
    struct roce_dev_data rdev_data = { 0 };
    int ret;

    RS_PTHREAD_MUTEX_LOCK(&ping_cb->ping_mutex);
    ping_cb->rdev_cb.dev_name = rs_ibv_get_device_name(ping_cb->rdev_cb.dev_list[index]);
    ret = rs_roce_get_roce_dev_data(ping_cb->rdev_cb.dev_name, &rdev_data);
    if (ret != 0) {
        hccp_err("rs_roce_get_roce_dev_data failed, ret:%d, dev_name:%s", ret, ping_cb->rdev_cb.dev_name);
        RS_PTHREAD_MUTEX_ULOCK(&ping_cb->ping_mutex);
        return ret;
    }
    ping_cb->dev_index = rdev_data.rdev_index; // rdev_index is same to port_id
    RS_PTHREAD_MUTEX_ULOCK(&ping_cb->ping_mutex);
#endif
    return 0;
}

STATIC int rs_ping_cb_get_ib_ctx_and_index(struct rdev *rdev_info, struct rs_ping_ctx_cb *ping_cb)
{
    struct ibv_context *ib_ctx = NULL;
    int ret;
    int i;

    for (i = 0; (i < ping_cb->rdev_cb.dev_num) && (ping_cb->rdev_cb.dev_list[i] != NULL); ++i) {
        ib_ctx = rs_ibv_open_device(ping_cb->rdev_cb.dev_list[i]);
        CHK_PRT_RETURN(ib_ctx == NULL, hccp_err("ibv_open_device failed!"), -ENODEV);
        ret = rs_query_gid(*rdev_info, ib_ctx, ping_cb->rdev_cb.ib_port, &ping_cb->rdev_cb.gid_idx);
        if (ret == 0) {
            ret = rs_ping_cb_get_dev_rdev_index(ping_cb, i);
            if (ret != 0) {
                hccp_err("rs_ping_cb_get_dev_rdev_index failed, ret:%d", ret);
                rs_ibv_close_device(ib_ctx);
                return ret;
            }
            ping_cb->rdev_cb.ib_ctx = ib_ctx;
            ret = rs_ibv_query_gid(ib_ctx, ping_cb->rdev_cb.ib_port, ping_cb->rdev_cb.gid_idx, &ping_cb->rdev_cb.gid);
            if (ret != 0) {
                rs_ibv_close_device(ib_ctx);
                hccp_err("query gid failed gid_idx %d, ret %d", ping_cb->rdev_cb.gid_idx, ret);
                return -EOPENSRC;
            }
            return 0;
        } else if (ret == -EEXIST) {
            rs_ibv_close_device(ib_ctx);
        } else {
            hccp_err("rs_query_gid failed, ret:%d", ret);
            rs_ibv_close_device(ib_ctx);
            return ret;
        }
    }

    CHK_PRT_RETURN(i == ping_cb->rdev_cb.dev_num, hccp_err("can not find ib_ctx for phy_id[%u] local_ip[0x%x] "
        "in dev_list!", rdev_info->phy_id, rdev_info->local_ip.addr.s_addr), -ENODEV);
    return 0;
}

STATIC int rs_ping_common_modify_local_qp(struct rs_ping_ctx_cb *ping_cb, struct rs_ping_local_qp_cb *qp_cb)
{
    struct ibv_qp_init_attr init_attr;
    struct ibv_qp_attr attr = { 0 };
    int ret;

    ret = rs_ibv_query_qp(qp_cb->ib_qp, &attr, IBV_QP_STATE, &init_attr);
    CHK_PRT_RETURN(ret != 0 || attr.qp_state != IBV_QPS_RESET,
        hccp_err("rs_ibv_query_qp qpn:%u fail, ret:%d attr.qp_state:%d != %d",
        qp_cb->ib_qp->qp_num, ret, attr.qp_state, IBV_QPS_RESET), -EOPENSRC);

    attr.qp_state = IBV_QPS_INIT;
    attr.pkey_index = 0;
    attr.port_num = ping_cb->rdev_cb.ib_port;
    attr.qkey = qp_cb->qkey;
    ret = rs_ibv_modify_qp(qp_cb->ib_qp, &attr, IBV_QP_STATE | IBV_QP_PKEY_INDEX | IBV_QP_PORT | IBV_QP_QKEY);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_ibv_modify_qp qpn:%u to init fail, ret:%d, errno:%d",
        qp_cb->ib_qp->qp_num, ret, errno), -EOPENSRC);

    attr.qp_state = IBV_QPS_RTR;
    ret = rs_ibv_modify_qp(qp_cb->ib_qp, &attr, IBV_QP_STATE);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_ibv_modify_qp qpn:%u to rtr fail, ret:%d, errno:%d",
        qp_cb->ib_qp->qp_num, ret, errno), -EOPENSRC);

    attr.qp_state = IBV_QPS_RTS;
    attr.sq_psn = 0;
    ret = rs_ibv_modify_qp(qp_cb->ib_qp, &attr, IBV_QP_STATE | IBV_QP_SQ_PSN);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_ibv_modify_qp qpn:%u to rts fail, ret:%d, errno:%d",
        qp_cb->ib_qp->qp_num, ret, errno), -EOPENSRC);

    return 0;
}

STATIC int rs_ping_common_init_local_qp(struct rs_cb *rscb, struct rs_ping_ctx_cb *ping_cb, union ping_qp_attr *attr,
    struct rs_ping_local_qp_cb *qp_cb)
{
    struct ibv_exp_qp_init_attr qp_init_attr = { 0 };
    struct rdma_lite_device_qp_attr qp_resp = { 0 };
    int rand_num;
    int ret;

    hccp_info("cq_attr{%d %d, %d %d}", attr->rdma.cq_attr.send_cq_depth, attr->rdma.cq_attr.send_cq_comp_vector,
        attr->rdma.cq_attr.recv_cq_depth, attr->rdma.cq_attr.recv_cq_comp_vector);

    // create send cq with attr
    qp_cb->send_cq.depth = attr->rdma.cq_attr.send_cq_depth;
    qp_cb->send_cq.comp_vector = attr->rdma.cq_attr.send_cq_comp_vector;
    qp_cb->send_cq.ib_cq = rs_ibv_create_cq(ping_cb->rdev_cb.ib_ctx, qp_cb->send_cq.depth, NULL, NULL,
        qp_cb->send_cq.comp_vector);
    qp_cb->send_cq.max_recv_wc_num = RS_PING_ROCE_RECV_WC_NUM;
    ret = -errno;
    CHK_PRT_RETURN(qp_cb->send_cq.ib_cq == NULL, hccp_err("rs_ibv_create_cq send cq fail, ret:%d", ret), ret);

    // create channel & create recv cq with attr
    qp_cb->channel = rs_ibv_create_comp_channel(ping_cb->rdev_cb.ib_ctx);
    if (qp_cb->channel == NULL) {
        ret = -errno;
        hccp_err("rs_ibv_create_comp_channel failed! ret:%d", ret);
        goto create_channel_fail;
    }
    ret = rs_epoll_ctl(rscb->conn_cb.epollfd, EPOLL_CTL_ADD, qp_cb->channel->fd, EPOLLIN | EPOLLRDHUP);
    if (ret != 0) {
        hccp_err("rs_epoll_ctl failed! epollfd:%d fd:%d ret:%d", rscb->conn_cb.epollfd, qp_cb->channel->fd, ret);
        goto epoll_ctl_fail;
    }
    qp_cb->recv_cq.depth = attr->rdma.cq_attr.recv_cq_depth;
    qp_cb->recv_cq.comp_vector = attr->rdma.cq_attr.recv_cq_comp_vector;
    qp_cb->recv_cq.ib_cq = rs_ibv_create_cq(ping_cb->rdev_cb.ib_ctx, qp_cb->recv_cq.depth, NULL, qp_cb->channel,
        qp_cb->recv_cq.comp_vector);
    qp_cb->recv_cq.max_recv_wc_num = RS_PING_ROCE_RECV_WC_NUM;
    if (qp_cb->recv_cq.ib_cq == NULL) {
        ret = -errno;
        hccp_err("rs_ibv_create_cq recv cq fail, ret:%d", ret);
        goto create_rcq_fail;
    }

    // create qp with attr
    (void)rs_drv_get_random_num(&rand_num);
    // clear bit IB_QP_SET_QKEY to avoid modify_qp to INIT failed
    qp_cb->qkey = (uint32_t)(((uint32_t)rand_num) & (~(1U << 31U)));
    (void)memcpy_s(&qp_cb->qp_cap, sizeof(struct ibv_qp_cap), &attr->rdma.qp_attr.cap, sizeof(struct ibv_qp_cap));
    qp_cb->udp_sport = attr->rdma.qp_attr.udp_sport;
    qp_init_attr.attr.send_cq = qp_cb->send_cq.ib_cq;
    qp_init_attr.attr.recv_cq = qp_cb->recv_cq.ib_cq;
    (void)memcpy_s(&qp_init_attr.attr.cap, sizeof(struct ibv_qp_cap), &qp_cb->qp_cap, sizeof(struct ibv_qp_cap));
    qp_init_attr.attr.qp_type = IBV_QPT_UD;
    qp_init_attr.udp_sport = attr->rdma.qp_attr.udp_sport;

    hccp_info("qkey:%u udp_sport:%u qp_cap{%u %u %u %u %u}", qp_cb->qkey, qp_cb->udp_sport,
        attr->rdma.qp_attr.cap.max_send_wr, attr->rdma.qp_attr.cap.max_recv_wr, attr->rdma.qp_attr.cap.max_send_sge,
        attr->rdma.qp_attr.cap.max_recv_sge, attr->rdma.qp_attr.cap.max_inline_data);
    qp_cb->ib_qp = rs_ibv_exp_create_qp(ping_cb->rdev_cb.ib_pd, &qp_init_attr, &qp_resp);
    if (qp_cb->ib_qp == NULL) {
        ret = -errno;
        hccp_err("rs_ibv_exp_create_qp qp fail, ret:%d", ret);
        goto create_qp_fail;
    }

    ret = rs_ping_common_modify_local_qp(ping_cb, qp_cb);
    if (ret != 0) {
        hccp_err("rs_ping_common_modify_local_qp failed, ret:%d", ret);
        goto modify_qp_fail;
    }

    ret = rs_ibv_req_notify_cq(qp_cb->recv_cq.ib_cq, 0);
    if (ret != 0) {
        hccp_err("rs_ibv_req_notify_cq failed, ret:%d", ret);
        goto modify_qp_fail;
    }

    hccp_run_info("qpn:%u create success, cq_attr{%d %d, %d %d} qkey:%u udp_sport:%u qp_cap{%u %u %u %u %u}",
        qp_cb->ib_qp->qp_num, attr->rdma.cq_attr.send_cq_depth, attr->rdma.cq_attr.send_cq_comp_vector,
        attr->rdma.cq_attr.recv_cq_depth, attr->rdma.cq_attr.recv_cq_comp_vector, qp_cb->qkey, qp_cb->udp_sport,
        attr->rdma.qp_attr.cap.max_send_wr, attr->rdma.qp_attr.cap.max_recv_wr, attr->rdma.qp_attr.cap.max_send_sge,
        attr->rdma.qp_attr.cap.max_recv_sge, attr->rdma.qp_attr.cap.max_inline_data);

    return 0;

modify_qp_fail:
    (void)rs_ibv_destroy_qp(qp_cb->ib_qp);
create_qp_fail:
    (void)rs_ibv_destroy_cq(qp_cb->recv_cq.ib_cq);
create_rcq_fail:
    (void)rs_epoll_ctl(rscb->conn_cb.epollfd, EPOLL_CTL_DEL, qp_cb->channel->fd, EPOLLIN | EPOLLRDHUP);
epoll_ctl_fail:
    (void)rs_ibv_destroy_comp_channel(qp_cb->channel);
create_channel_fail:
    (void)rs_ibv_destroy_cq(qp_cb->send_cq.ib_cq);
    return ret;
}

STATIC int rs_ping_common_init_mr_cb(struct rs_cb *rscb, struct rs_ping_ctx_cb *ping_cb, struct rs_ping_mr_cb *mr_cb)
{
    unsigned long flag = 0;
    uint32_t idx = 0;
    int ret;

    hccp_info("payload_offset:%u len:0x%llx sge_num:%u grp_id:%u",
        mr_cb->payload_offset, mr_cb->len, mr_cb->sge_num, rscb->grp_id);

    ret = pthread_mutex_init(&mr_cb->mutex, NULL);
    CHK_PRT_RETURN(ret != 0, hccp_err("pthread_mutex_init mr_cb mutex failed, ret:%d", ret), ret);

    flag = ((unsigned long)ping_cb->logic_devid << BUFF_FLAGS_DEVID_OFFSET) | BUFF_SP_SVM;
    ret = dl_hal_buff_alloc_align_ex(mr_cb->len, RA_RS_PING_BUFFER_ALIGN_4K_PAGE_SIZE, flag,
        (int)rscb->grp_id, (void **)&mr_cb->addr);
    if (ret != 0) {
        hccp_err("dl_hal_buff_alloc_align_ex failed, length:0x%llx, dev_id:0x%x, flag:0x%lx, grp_id:%u, ret:%d",
            mr_cb->len, ping_cb->logic_devid, flag, rscb->grp_id, ret);
        goto alloc_fail;
    }

    mr_cb->ib_mr = rs_drv_mr_reg(ping_cb->rdev_cb.ib_pd, (char *)(uintptr_t)mr_cb->addr, mr_cb->len,
        IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ);
    if (mr_cb->ib_mr == NULL) {
        ret = -errno;
        hccp_err("rs_ibv_reg_mr fail, ret:%d addr:0x%llx len:0x%llx", ret, mr_cb->addr, mr_cb->len);
        goto mr_reg_fail;
    }

    // init sge list
    mr_cb->sge_list = calloc(mr_cb->sge_num, sizeof(struct ibv_sge));
    if (mr_cb->sge_list == NULL) {
        ret = -errno;
        hccp_err("calloc fail, ret:%d sge_num:%u", ret, mr_cb->sge_num);
        goto calloc_fail;
    }
    for (idx = 0; idx < mr_cb->sge_num; idx++) {
        mr_cb->sge_list[idx].lkey = mr_cb->ib_mr->lkey;
        mr_cb->sge_list[idx].length = mr_cb->payload_offset;
        if (idx == 0) {
            mr_cb->sge_list[idx].addr = mr_cb->addr;
        } else {
            mr_cb->sge_list[idx].addr = mr_cb->sge_list[idx - 1].addr + mr_cb->payload_offset;
        }
    }
    mr_cb->sge_idx = 0;

    hccp_info("addr:0x%llx lkey:%u ", mr_cb->addr, mr_cb->ib_mr->lkey);

    return 0;

calloc_fail:
    (void)rs_drv_mr_dereg(mr_cb->ib_mr);
mr_reg_fail:
    (void)dl_hal_buff_free((void *)(uintptr_t)mr_cb->addr);
alloc_fail:
    (void)pthread_mutex_destroy(&mr_cb->mutex);
    return ret;
}

STATIC void rs_ping_common_deinit_mr_cb(struct rs_ping_mr_cb *mr_cb)
{
    hccp_dbg("addr:0x%llx len:%llu", mr_cb->addr, mr_cb->len);
    free(mr_cb->sge_list);
    mr_cb->sge_list = NULL;
    (void)rs_drv_mr_dereg(mr_cb->ib_mr);
    (void)dl_hal_buff_free((void *)(uintptr_t)mr_cb->addr);
    (void)pthread_mutex_destroy(&mr_cb->mutex);
}

STATIC int rs_ping_pong_init_local_buffer(struct rs_cb *rscb, struct ping_init_attr *attr, struct ping_init_info *info,
    struct rs_ping_ctx_cb *ping_cb)
{
    int ret;

    // prepare ping_qp send mr
    ping_cb->ping_qp.send_mr_cb.payload_offset = PING_TOTAL_PAYLOAD_MAX_SIZE;
    ping_cb->ping_qp.send_mr_cb.len = ping_cb->ping_qp.qp_cap.max_send_wr * ping_cb->ping_qp.send_mr_cb.payload_offset;
    ping_cb->ping_qp.send_mr_cb.sge_num = ping_cb->ping_qp.qp_cap.max_send_wr;
    ret = rs_ping_common_init_mr_cb(rscb, ping_cb, &ping_cb->ping_qp.send_mr_cb);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_ping_common_init_mr_cb ping_qp send_mr_cb failed, ret %d", ret), ret);
    // prepare ping_qp recv mr
    ping_cb->ping_qp.recv_mr_cb.payload_offset = PING_TOTAL_PAYLOAD_MAX_SIZE;
    ping_cb->ping_qp.recv_mr_cb.len = ping_cb->ping_qp.qp_cap.max_recv_wr * ping_cb->ping_qp.recv_mr_cb.payload_offset;
    ping_cb->ping_qp.recv_mr_cb.sge_num = ping_cb->ping_qp.qp_cap.max_recv_wr;
    ret = rs_ping_common_init_mr_cb(rscb, ping_cb, &ping_cb->ping_qp.recv_mr_cb);
    if (ret != 0) {
        hccp_err("rs_ping_common_init_mr_cb ping_qp recv_mr_cb failed, ret %d", ret);
        goto init_ping_qp_recv_mr_fail;
    }

    // prepare pong_qp send mr
    ping_cb->pong_qp.send_mr_cb.payload_offset = PING_TOTAL_PAYLOAD_MAX_SIZE;
    ping_cb->pong_qp.send_mr_cb.len = ping_cb->pong_qp.qp_cap.max_send_wr * ping_cb->pong_qp.send_mr_cb.payload_offset;
    ping_cb->pong_qp.send_mr_cb.sge_num = ping_cb->pong_qp.qp_cap.max_send_wr;
    ret = rs_ping_common_init_mr_cb(rscb, ping_cb, &ping_cb->pong_qp.send_mr_cb);
    if (ret != 0) {
        hccp_err("rs_ping_common_init_mr_cb pong_qp send_mr_cb failed, ret %d", ret);
        goto init_pong_qp_send_mr_fail;
    }
    // prepare pong_qp recv mr
    ping_cb->pong_qp.recv_mr_cb.payload_offset = PING_TOTAL_PAYLOAD_MAX_SIZE;
    ping_cb->pong_qp.recv_mr_cb.len = attr->buffer_size;
    ping_cb->pong_qp.recv_mr_cb.sge_num = attr->buffer_size / ping_cb->pong_qp.recv_mr_cb.payload_offset;
    ret = rs_ping_common_init_mr_cb(rscb, ping_cb, &ping_cb->pong_qp.recv_mr_cb);
    if (ret != 0) {
        hccp_err("rs_ping_common_init_mr_cb pong_qp recv_mr_cb failed, ret %d", ret);
        goto init_pong_qp_recv_mr_fail;
    }
    info->result.buffer_va = ping_cb->pong_qp.recv_mr_cb.addr;
    info->result.buffer_size = attr->buffer_size;
    info->result.payload_offset = ping_cb->pong_qp.recv_mr_cb.payload_offset;
    info->result.header_size = RS_PING_PAYLOAD_HEADER_RESV_GRH + RS_PING_PAYLOAD_HEADER_RESV_CUSTOM;

    return 0;

init_pong_qp_recv_mr_fail:
    rs_ping_common_deinit_mr_cb(&ping_cb->pong_qp.send_mr_cb);
init_pong_qp_send_mr_fail:
    rs_ping_common_deinit_mr_cb(&ping_cb->ping_qp.recv_mr_cb);
init_ping_qp_recv_mr_fail:
    rs_ping_common_deinit_mr_cb(&ping_cb->ping_qp.send_mr_cb);
    return ret;
}

STATIC int rs_ping_common_post_recv(struct rs_ping_local_qp_cb *qp_cb)
{
    struct ibv_recv_wr *bad_wr = NULL;
    struct ibv_recv_wr wr = { 0 };
    struct ibv_sge list = { 0 };
    uint32_t sge_idx;
    int ret;

    RS_PTHREAD_MUTEX_LOCK(&qp_cb->recv_mr_cb.mutex);
    sge_idx = qp_cb->recv_mr_cb.sge_idx;
    (void)memcpy_s(&list, sizeof(struct ibv_sge), &qp_cb->recv_mr_cb.sge_list[sge_idx], sizeof(struct ibv_sge));
    qp_cb->recv_mr_cb.sge_idx = (sge_idx + 1) % qp_cb->recv_mr_cb.sge_num;
    RS_PTHREAD_MUTEX_ULOCK(&qp_cb->recv_mr_cb.mutex);

    wr.wr_id = (uint64_t)sge_idx;
    wr.next = NULL;
    wr.sg_list = &list;
    wr.num_sge = 1;

    ret = rs_ibv_post_recv(qp_cb->ib_qp, &wr, &bad_wr);
    if (ret != 0) {
        hccp_err("rs_ibv_post_recv failed, ret:%d", ret);
        return ret;
    }

    return 0;
}

STATIC int rs_ping_common_init_post_recv_all(struct rs_ping_local_qp_cb *qp_cb)
{
    int ret = 0;
    uint32_t i;

    // prepare RQ wqe
    for (i = qp_cb->recv_mr_cb.sge_idx; i < qp_cb->recv_mr_cb.sge_num && i < qp_cb->qp_cap.max_recv_wr; i++) {
        ret = rs_ping_common_post_recv(qp_cb);
        if (ret != 0) {
            hccp_err("rs_ping_common_post_recv %u-th rqe failed, ret:%d", i, ret);
            break;
        }
    }

    return ret;
}

STATIC void rs_ping_common_deinit_local_buffer(struct rs_ping_ctx_cb *ping_cb)
{
    rs_ping_common_deinit_mr_cb(&ping_cb->pong_qp.recv_mr_cb);
    rs_ping_common_deinit_mr_cb(&ping_cb->pong_qp.send_mr_cb);
    rs_ping_common_deinit_mr_cb(&ping_cb->ping_qp.recv_mr_cb);
    rs_ping_common_deinit_mr_cb(&ping_cb->ping_qp.send_mr_cb);
}

STATIC void rs_ping_common_deinit_local_qp(struct rs_cb *rscb, struct rs_ping_ctx_cb *ping_cb,
    struct rs_ping_local_qp_cb *qp_cb)
{
    if (qp_cb == NULL || qp_cb->channel == NULL) {
        hccp_err("qp_cb is NULL or qp_cb->channel is NULL");
        return;
    }

    (void)rs_ibv_destroy_qp(qp_cb->ib_qp);
    rs_ibv_ack_cq_events(qp_cb->recv_cq.ib_cq, qp_cb->recv_cq.num_events);
    qp_cb->recv_cq.num_events = 0;
    (void)rs_ibv_destroy_cq(qp_cb->recv_cq.ib_cq);
    (void)rs_epoll_ctl(rscb->conn_cb.epollfd, EPOLL_CTL_DEL, qp_cb->channel->fd, EPOLLIN | EPOLLRDHUP);
    (void)rs_ibv_destroy_comp_channel(qp_cb->channel);
    qp_cb->channel = NULL;
    (void)rs_ibv_destroy_cq(qp_cb->send_cq.ib_cq);
}

STATIC int rs_ping_pong_init_local_info(struct rs_cb *rscb, struct ping_init_attr *attr, struct ping_init_info *info,
    struct rs_ping_ctx_cb *ping_cb)
{
    int ret;

    ret = rs_ping_common_init_local_qp(rscb, ping_cb, &attr->client, &ping_cb->ping_qp);
    CHK_PRT_RETURN(ret != 0, hccp_err("init ping_qp failed, ret:%d", ret), ret);
    info->client.version = 0;
    (void)memcpy_s(&info->client.rdma.gid, sizeof(union hccp_gid), &ping_cb->rdev_cb.gid, sizeof(union ibv_gid));
    info->client.rdma.qpn = ping_cb->ping_qp.ib_qp->qp_num;
    info->client.rdma.qkey = ping_cb->ping_qp.qkey;

    ret = rs_ping_common_init_local_qp(rscb, ping_cb, &attr->server, &ping_cb->pong_qp);
    if (ret != 0) {
        hccp_err("init pong_qp failed, ret:%d", ret);
        goto init_pong_qp_fail;
    }
    info->server.version = 0;
    (void)memcpy_s(&info->server.rdma.gid, sizeof(union hccp_gid), &ping_cb->rdev_cb.gid, sizeof(union ibv_gid));
    info->server.rdma.qpn = ping_cb->pong_qp.ib_qp->qp_num;
    info->server.rdma.qkey = ping_cb->pong_qp.qkey;

    ret = rs_ping_pong_init_local_buffer(rscb, attr, info, ping_cb);
    if (ret != 0) {
        hccp_err("init buffer failed, ret:%d", ret);
        goto init_buffer_fail;
    }

    ret = rs_ping_common_init_post_recv_all(&ping_cb->ping_qp);
    if (ret != 0) {
        hccp_err("ping_qp post recv failed, ret:%d", ret);
        goto post_recv_fail;
    }
    ret = rs_ping_common_init_post_recv_all(&ping_cb->pong_qp);
    if (ret != 0) {
        hccp_err("pong_qp post recv failed, ret:%d", ret);
        goto post_recv_fail;
    }

    return 0;

post_recv_fail:
    rs_ping_common_deinit_local_buffer(ping_cb);
init_buffer_fail:
    rs_ping_common_deinit_local_qp(rscb, ping_cb, &ping_cb->pong_qp);
init_pong_qp_fail:
    rs_ping_common_deinit_local_qp(rscb, ping_cb, &ping_cb->ping_qp);
    return ret;
}

STATIC int rs_ping_roce_ping_cb_init(unsigned int phy_id, struct ping_init_attr *attr, struct ping_init_info *info,
    unsigned int *dev_index, struct rs_ping_ctx_cb *ping_cb)
{
    struct rdev *rdev_info = &attr->dev.rdma;
    struct rs_cb *rscb = NULL;
    int ret;

    ret = rs_get_rs_cb(phy_id, &rscb);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_get_rs_cb failed, phy_id[%u] invalid, ret %d", phy_id, ret), ret);

    // prepare input attr
    ping_cb->rdev_cb.ip.family = (uint32_t)rdev_info->family;
    ping_cb->rdev_cb.ip.bin_addr = rdev_info->local_ip;
    ret = rs_inet_ntop(rdev_info->family, &rdev_info->local_ip, ping_cb->rdev_cb.ip.read_addr, RS_MAX_IP_LEN);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_inet_ntop failed, ret %d", ret), -EINVAL);
    (void)memcpy_s(&ping_cb->comm_info, sizeof(struct ping_local_comm_info), &attr->comm_info,
        sizeof(struct ping_local_comm_info));

    // open device & alloc global pd
    ping_cb->rdev_cb.dev_list = rs_ibv_get_device_list(&ping_cb->rdev_cb.dev_num);
    if (ping_cb->rdev_cb.dev_list == NULL || ping_cb->rdev_cb.dev_num == 0) {
        hccp_err("dev_list is NULL or dev_num[%d] is 0", ping_cb->rdev_cb.dev_num);
        ret = -ENODEV;
        goto get_device_list_fail;
    }

    ping_cb->rdev_cb.ib_port = RS_PORT_DEF;
    ret = rs_ping_cb_get_ib_ctx_and_index(rdev_info, ping_cb);
    if (ret != 0) {
        hccp_err("rs_ping_cb_get_ib_ctx_and_index failed, ret:%d", ret);
        goto get_ib_ctx_and_index_fail;
    }

    ping_cb->rdev_cb.ib_pd = rs_ibv_alloc_pd(ping_cb->rdev_cb.ib_ctx);
    if (ping_cb->rdev_cb.ib_pd == NULL) {
        hccp_err("rs_ibv_alloc_pd failed, errno:%d", errno);
        ret = -ENOMEM;
        goto alloc_pd_fail;
    }

    // init cq & qp & mr info, prepare output info
    info->version = 0;
    ret = rs_ping_pong_init_local_info(rscb, attr, info, ping_cb);
    if (ret != 0) {
        hccp_err("rs_ping_pong_init_local_info failed, ret=%d phy_id:%u", ret, rdev_info->phy_id);
        goto init_local_info_fail;
    }

    *dev_index = ping_cb->dev_index;
    return 0;

init_local_info_fail:
    (void)rs_ibv_dealloc_pd(ping_cb->rdev_cb.ib_pd);
alloc_pd_fail:
    (void)rs_ibv_close_device(ping_cb->rdev_cb.ib_ctx);
get_ib_ctx_and_index_fail:
    rs_ibv_free_device_list(ping_cb->rdev_cb.dev_list);
get_device_list_fail:
    (void)pthread_mutex_destroy(&ping_cb->ping_mutex);
    (void)pthread_mutex_destroy(&ping_cb->pong_mutex);
    return ret;
}

STATIC bool rs_ping_common_compare_rdma_info(struct ping_qp_info *a, struct ping_qp_info *b)
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

STATIC int rs_ping_roce_find_target_node(struct rs_ping_ctx_cb *ping_cb, struct ping_qp_info *target,
    struct rs_ping_target_info **node)
{
    struct rs_ping_target_info *target_next = NULL;
    struct rs_ping_target_info *target_curr = NULL;

    RS_PTHREAD_MUTEX_LOCK(&ping_cb->ping_mutex);
    RS_LIST_GET_HEAD_ENTRY(target_curr, target_next, &ping_cb->ping_list, list, struct rs_ping_target_info);
    for (; (&target_curr->list) != &ping_cb->ping_list;
        target_curr = target_next, target_next = list_entry(target_next->list.next, struct rs_ping_target_info, list)) {
        if (rs_ping_common_compare_rdma_info(&target_curr->qp_info, target)) {
            *node = target_curr;
            RS_PTHREAD_MUTEX_ULOCK(&ping_cb->ping_mutex);
            return 0;
        }
    }
    RS_PTHREAD_MUTEX_ULOCK(&ping_cb->ping_mutex);

    hccp_info("ping target node for qpn:%u gid:%016llx:%016llx not found", target->rdma.qpn,
        target->rdma.gid.global.subnet_prefix, target->rdma.gid.global.interface_id);
    return -ENODEV;
}

STATIC int rs_ping_common_create_ah(struct rs_ping_ctx_cb *ping_cb, struct ping_local_comm_info *local_info,
    struct ping_qp_info *remote_info, struct ibv_ah **ah)
{
    struct ibv_exp_ah_attr attrx = { 0 };
    struct ibv_global_route grh = { 0 };
    struct ibv_ah_attr attr = { 0 };
    struct ibv_ah *ah_tmp = NULL;
    int ret = 0;

    (void)memcpy_s(&grh.dgid, sizeof(union ibv_gid), &remote_info->rdma.gid, sizeof(union hccp_gid));
    grh.flow_label = local_info->rdma.flow_label;
    grh.sgid_index = (uint8_t)ping_cb->rdev_cb.gid_idx;
    grh.hop_limit = local_info->rdma.hop_limit;
    grh.traffic_class = local_info->rdma.qos_attr.tc;

    attr.grh = grh;
    attr.sl = local_info->rdma.qos_attr.sl;
    attr.is_global = 1;
    attr.port_num = ping_cb->rdev_cb.ib_port;
    attrx.attr = attr;
    attrx.udp_sport = local_info->rdma.udp_sport;

    hccp_dbg("remote_qpn:%u flow_label:%u sgid_index:%u hop_limit:%u traffic_class:%u sl:%u is_global:%u "
        "port_num:%u udp_sport:%u", remote_info->rdma.qpn, grh.flow_label, grh.sgid_index, grh.hop_limit,
        grh.traffic_class, attr.sl, attr.is_global, attr.port_num, attrx.udp_sport);

    ah_tmp = rs_ibv_exp_create_ah(ping_cb->rdev_cb.ib_pd, &attrx);
    if (ah_tmp == NULL) {
        ret = -errno;
        hccp_err("rs_ibv_exp_create_ah failed, errno:%d", ret);
        return ret;
    }

    *ah = ah_tmp;
    return ret;
}

STATIC int rs_ping_roce_alloc_target_node(struct rs_ping_ctx_cb *ping_cb, struct ping_target_info *target,
    struct rs_ping_target_info **node)
{
    struct rs_ping_target_info *target_info = NULL;
    int ret;

    target_info = (struct rs_ping_target_info *)calloc(1, sizeof(struct rs_ping_target_info));
    CHK_PRT_RETURN(target_info == NULL, hccp_err("calloc target_info fail! errno:%d", errno), -ENOMEM);

    ret = pthread_mutex_init(&target_info->trip_mutex, NULL);
    if (ret != 0) {
        hccp_err("pthread_mutex_init trip_mutex failed, ret:%d", ret);
        goto free_target_info;
    }

    target_info->payload_size = target->payload.size;
    if (target->payload.size > 0) {
        target_info->payload_buffer = (char *)calloc(1, target->payload.size);
        if (target_info->payload_buffer == NULL) {
            hccp_err("calloc payload_buffer fail! size:%u errno:%d", target->payload.size, errno);
            ret = -ENOMEM;
            goto free_trip_mutex;
        }
        (void)memcpy_s(target_info->payload_buffer, target->payload.size, target->payload.buffer, target->payload.size);
    }

    (void)memcpy_s(&target_info->qp_info, sizeof(struct ping_qp_info),
        &target->remote_info.qp_info, sizeof(struct ping_qp_info));
    ret = rs_ping_common_create_ah(ping_cb, &target->local_info, &target->remote_info.qp_info, &target_info->ah);
    if (ret != 0) {
        hccp_err("rs_ping_common_create_ah fail! ret:%d", ret);
        goto free_payload_buffer;
    }

    target_info->result_summary.rtt_min = ~0;
    target_info->state = RS_PING_PONG_TARGET_READY;
    *node = target_info;

    return 0;
free_payload_buffer:
    if (target->payload.size > 0 && target_info->payload_buffer != NULL) {
        free(target_info->payload_buffer);
        target_info->payload_buffer = NULL;
    }
free_trip_mutex:
    (void)pthread_mutex_destroy(&target_info->trip_mutex);
free_target_info:
    free(target_info);
    target_info = NULL;
    return ret;
}

STATIC void rs_ping_roce_reset_recv_buffer(struct rs_ping_ctx_cb *ping_cb)
{
    RS_PTHREAD_MUTEX_LOCK(&ping_cb->pong_qp.recv_mr_cb.mutex);
    (void)memset_s((void *)(uintptr_t)ping_cb->pong_qp.recv_mr_cb.addr, ping_cb->pong_qp.recv_mr_cb.len,
        0, ping_cb->pong_qp.recv_mr_cb.len);
    RS_PTHREAD_MUTEX_ULOCK(&ping_cb->pong_qp.recv_mr_cb.mutex);
}

STATIC void rs_ping_qp_build_up_wr(struct rs_ping_target_info *target, struct ibv_sge *list, struct ibv_send_wr *wr)
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

STATIC int rs_ping_roce_post_send(struct rs_ping_ctx_cb *ping_cb, struct rs_ping_target_info *target)
{
    struct rs_ping_payload_header *header = NULL;
    struct ibv_send_wr *bad_wr = NULL;
    struct timeval timestamp = { 0 };
    struct ibv_send_wr wr = { 0 };
    struct ibv_sge list = { 0 };
    uint32_t sge_idx;
    int ret = 0;

    hccp_dbg("target uuid:0x%llx state:%d payload_size:%u qpn:%u gid:%016llx:%016llx",
        target->uuid, target->state, target->payload_size, target->qp_info.rdma.qpn,
        target->qp_info.rdma.gid.global.subnet_prefix, target->qp_info.rdma.gid.global.interface_id);

    RS_PTHREAD_MUTEX_LOCK(&ping_cb->ping_qp.send_mr_cb.mutex);
    sge_idx = ping_cb->ping_qp.send_mr_cb.sge_idx;
    (void)memcpy_s(&list, sizeof(struct ibv_sge),
        &ping_cb->ping_qp.send_mr_cb.sge_list[sge_idx], sizeof(struct ibv_sge));
    ping_cb->ping_qp.send_mr_cb.sge_idx = (sge_idx + 1) % ping_cb->ping_qp.send_mr_cb.sge_num;
    RS_PTHREAD_MUTEX_ULOCK(&ping_cb->ping_qp.send_mr_cb.mutex);

    // prepare ping_qp send buffer
    (void)memset_s((void *)(uintptr_t)list.addr, list.length, 0, list.length);
    header = (struct rs_ping_payload_header *)(uintptr_t)list.addr;
    header->type = RS_PING_TYPE_ROCE_DETECT;
    (void)memcpy_s(&header->server.rdma.gid, sizeof(union hccp_gid), &ping_cb->rdev_cb.gid, sizeof(union ibv_gid));
    header->server.rdma.qpn = ping_cb->pong_qp.ib_qp->qp_num;
    header->server.rdma.qkey = ping_cb->pong_qp.qkey;
    (void)memcpy_s(&header->target, sizeof(struct ping_qp_info), &target->qp_info, sizeof(struct ping_qp_info));

    if (target->payload_size > 0) {
        ret = memcpy_s((void *)(uintptr_t)(list.addr + RS_PING_PAYLOAD_HEADER_RESV_CUSTOM),
            (list.length - RS_PING_PAYLOAD_HEADER_RESV_CUSTOM),
            (void *)target->payload_buffer, target->payload_size);
        CHK_PRT_RETURN(ret != 0, hccp_err("memcpy_s buffer payload_size:%u list.length:%u failed, ret:%d",
            target->payload_size, (list.length - RS_PING_PAYLOAD_HEADER_RESV_CUSTOM), ret), -ESAFEFUNC);
    }
    list.length = RS_PING_PAYLOAD_HEADER_RESV_CUSTOM + target->payload_size;

    rs_ping_qp_build_up_wr(target, &list, &wr);

    // record timestamp t1
    (void)gettimeofday(&timestamp, NULL);
    header->timestamp.tv_sec1 = (uint64_t)timestamp.tv_sec;
    header->timestamp.tv_usec1 = (uint64_t)timestamp.tv_usec;
    header->task_id = ping_cb->task_id;
    header->magic = 0x55AA;

    ret = rs_ibv_post_send(ping_cb->ping_qp.ib_qp, &wr, &bad_wr);
    if (ret != 0) {
        hccp_err("rs_ibv_post_send qpn:%u failed, ret:%d", ping_cb->ping_qp.ib_qp->qp_num, ret);
        RS_PTHREAD_MUTEX_LOCK(&target->trip_mutex);
        target->state = RS_PING_PONG_TARGET_ERROR;
        RS_PTHREAD_MUTEX_ULOCK(&target->trip_mutex);
    }
    return ret;
}

STATIC int rs_ping_roce_poll_scq(struct rs_ping_ctx_cb *ping_cb, struct rs_ping_target_info *target)
{
    struct ibv_wc wc = { 0 };
    int polled_cnt;

    polled_cnt = rs_ibv_poll_cq(ping_cb->ping_qp.send_cq.ib_cq, 1, &wc);
    if (polled_cnt != 1) {
        hccp_err("uuid:0x%llx rs_ibv_poll_cq polled_cnt:%d", target->uuid, polled_cnt);
        target->state = RS_PING_PONG_TARGET_ERROR;
        return -ENODATA;
    }
    if (wc.status != IBV_WC_SUCCESS) {
        target->state = RS_PING_PONG_TARGET_ERROR;
        hccp_err("wr_id:0x%llx error cqe %s(%d)", wc.wr_id, rs_ibv_wc_status_str(wc.status), wc.status);
        return -EOPENSRC;
    }
    return 0;
}

STATIC int rs_ping_roce_poll_rcq(struct rs_ping_ctx_cb *ping_cb, int *polled_cnt, struct timeval *timestamp2)
{
    struct ibv_cq *ev_cq = NULL;
    void *ev_ctx = NULL;
    int ret;

    // record timestamp t2
    (void)gettimeofday(timestamp2, NULL);

    ret = rs_ibv_get_cq_event(ping_cb->ping_qp.channel, &ev_cq, &ev_ctx);
    if (ret != 0) {
        hccp_err("rs_ibv_get_cq_event ping_qp.channel failed, ret:%d", ret);
        return -EOPENSRC;
    }

    if (ev_cq != ping_cb->ping_qp.recv_cq.ib_cq) {
        hccp_err("CQ event for unknown CQ");
        return -EOPENSRC;
    }
    ping_cb->ping_qp.recv_cq.num_events++;

    *polled_cnt = rs_ibv_poll_cq(ev_cq, ping_cb->ping_qp.recv_cq.max_recv_wc_num, g_ping_qp_recv_wc);
    CHK_PRT_RETURN(*polled_cnt > ping_cb->ping_qp.recv_cq.max_recv_wc_num || *polled_cnt < 0,
        hccp_err("ping_poll_rcq failed, ret:%d", *polled_cnt), -EOPENSRC);

    return 0;
}

STATIC int rs_ping_common_poll_scq(struct rs_ping_local_qp_cb *qp_cb)
{
    struct ibv_wc wc = { 0 };
    int polled_cnt;

    polled_cnt = rs_ibv_poll_cq(qp_cb->send_cq.ib_cq, 1, &wc);
    if (polled_cnt < 0) {
        hccp_warn("rs_ibv_poll_cq unsuccessful, polled_cnt:%d", polled_cnt);
    } else if (polled_cnt > 0) {
        if (wc.status != IBV_WC_SUCCESS) {
            hccp_err("wr_id:0x%llx error cqe %s(%d)", wc.wr_id, rs_ibv_wc_status_str(wc.status), wc.status);
            return -EOPENSRC;
        }
    }

    return 0;
}

STATIC int rs_pong_find_target_node(struct rs_ping_ctx_cb *ping_cb, struct ping_qp_info *target,
    struct rs_pong_target_info **node)
{
    struct rs_pong_target_info *target_next = NULL;
    struct rs_pong_target_info *target_curr = NULL;

    RS_CHECK_POINTER_NULL_WITH_RET(ping_cb);
    RS_PTHREAD_MUTEX_LOCK(&ping_cb->pong_mutex);
    RS_LIST_GET_HEAD_ENTRY(target_curr, target_next, &ping_cb->pong_list, list, struct rs_pong_target_info);
    for (; (&target_curr->list) != &ping_cb->pong_list;
        target_curr = target_next, target_next = list_entry(target_next->list.next, struct rs_pong_target_info, list)) {
        if (rs_ping_common_compare_rdma_info(&target_curr->qp_info, target)) {
            *node = target_curr;
            RS_PTHREAD_MUTEX_ULOCK(&ping_cb->pong_mutex);
            return 0;
        }
    }
    RS_PTHREAD_MUTEX_ULOCK(&ping_cb->pong_mutex);

    hccp_info("pong target node for qpn:%u gid:%016llx:%016llx not found", target->rdma.qpn,
        target->rdma.gid.global.subnet_prefix, target->rdma.gid.global.interface_id);
    return -ENODEV;
}

STATIC int rs_pong_find_alloc_target_node(struct rs_ping_ctx_cb *ping_cb, struct ping_qp_info *target,
    struct rs_pong_target_info **node)
{
    struct rs_pong_target_info *target_info = NULL;
    int ret;

    ret = rs_pong_find_target_node(ping_cb, target, node);
    if (ret == 0 && (*node)->state == RS_PING_PONG_TARGET_READY) {
        return 0;
    } else if (ret == 0) {
        target_info = *node;
        hccp_info("delete pong target uuid:0x%llx state:%d, realloc again", target_info->uuid, target_info->state);
        rs_list_del(&target_info->list);
        if (target_info->ah) {
            (void)rs_ibv_destroy_ah(target_info->ah);
        }
        free(target_info);
        target_info = NULL;
    }

    target_info = (struct rs_pong_target_info *)calloc(1, sizeof(struct rs_pong_target_info));
    CHK_PRT_RETURN(target_info == NULL, hccp_err("calloc target_info fail! errno:%d", errno), -ENOMEM);

    (void)memcpy_s(&target_info->qp_info, sizeof(struct ping_qp_info), target, sizeof(struct ping_qp_info));
    ret = rs_ping_common_create_ah(ping_cb, &ping_cb->comm_info, target, &target_info->ah);
    if (ret != 0) {
        hccp_err("rs_ping_common_create_ah fail! ret:%d", ret);
        goto free_target_info;
    }

    target_info->state = RS_PING_PONG_TARGET_READY;
    *node = target_info;

    RS_PTHREAD_MUTEX_LOCK(&ping_cb->pong_mutex);
    target_info->uuid = (uint64_t)ping_cb->pong_num << 32U;
    rs_list_add_tail(&target_info->list, &ping_cb->pong_list);
    ping_cb->pong_num++;
    RS_PTHREAD_MUTEX_ULOCK(&ping_cb->pong_mutex);

    return 0;

free_target_info:
    free(target_info);
    return ret;
}

STATIC int rs_pong_post_send(struct rs_ping_ctx_cb *ping_cb, struct ibv_wc *wc, struct timeval *timestamp2)
{
    struct rs_pong_target_info *target_info = NULL;
    struct rs_ping_payload_header *header = NULL;
    struct ibv_send_wr *bad_wr = NULL;
    struct timeval timestamp3 = { 0 };
    struct ibv_sge recv_list = { 0 };
    struct ibv_sge send_list = { 0 };
    struct ibv_send_wr wr = { 0 };
    uint32_t recv_sge_idx;
    uint32_t send_sge_idx;
    int ret = 0;

    // poll send cq
    (void)rs_ping_common_poll_scq(&ping_cb->pong_qp);

    // handle detect packet & send response packet
    recv_sge_idx = (uint32_t)wc->wr_id;
    if (recv_sge_idx > ping_cb->ping_qp.recv_mr_cb.sge_num) {
        hccp_err("param err recv_sge_idx:%u > sge_num:%u", recv_sge_idx, ping_cb->ping_qp.recv_mr_cb.sge_num);
        return -EIO;
    }
    (void)memcpy_s(&recv_list, sizeof(struct ibv_sge),
        &ping_cb->ping_qp.recv_mr_cb.sge_list[recv_sge_idx], sizeof(struct ibv_sge));

    RS_PTHREAD_MUTEX_LOCK(&ping_cb->pong_qp.send_mr_cb.mutex);
    send_sge_idx = ping_cb->pong_qp.send_mr_cb.sge_idx;
    (void)memcpy_s(&send_list, sizeof(struct ibv_sge),
        &ping_cb->pong_qp.send_mr_cb.sge_list[send_sge_idx], sizeof(struct ibv_sge));
    ping_cb->pong_qp.send_mr_cb.sge_idx = (send_sge_idx + 1) % ping_cb->pong_qp.send_mr_cb.sge_num;
    RS_PTHREAD_MUTEX_ULOCK(&ping_cb->pong_qp.send_mr_cb.mutex);

    // UD consume 40 Bytes for GRH
    if (wc->byte_len < RS_PING_PAYLOAD_HEADER_RESV_GRH || wc->byte_len > PING_TOTAL_PAYLOAD_MAX_SIZE) {
        hccp_err("param err wc->byte_len:%u < %u or wc->byte_len:%u > %u", wc->byte_len,
            RS_PING_PAYLOAD_HEADER_RESV_GRH, wc->byte_len, PING_TOTAL_PAYLOAD_MAX_SIZE);
        return -EIO;
    }
    ret = memcpy_s((void *)(uintptr_t)send_list.addr, send_list.length,
        (void *)(uintptr_t)(recv_list.addr + RS_PING_PAYLOAD_HEADER_RESV_GRH),
        wc->byte_len - RS_PING_PAYLOAD_HEADER_RESV_GRH);
    CHK_PRT_RETURN(ret != 0, hccp_err("memcpy_s buffer wc->byte_len:%u send_list.length:%u failed, ret:%d",
        wc->byte_len, send_list.length, ret), -ESAFEFUNC);
    send_list.length = wc->byte_len - RS_PING_PAYLOAD_HEADER_RESV_GRH;
    header = (struct rs_ping_payload_header *)(uintptr_t)send_list.addr;
    header->type = RS_PING_TYPE_ROCE_RESPONSE;

    ret = rs_pong_find_alloc_target_node(ping_cb, &header->server, &target_info);
    if (ret != 0) {
        hccp_err("rs_pong_find_alloc_target_node failed, ret:%d", ret);
        return ret;
    }

    wr.wr_id = target_info->uuid;
    wr.next = NULL;
    wr.sg_list = &send_list;
    wr.num_sge = 1;
    wr.opcode = IBV_WR_SEND;
    wr.send_flags = IBV_SEND_SIGNALED;
    wr.wr.ud.ah = target_info->ah;
    wr.wr.ud.remote_qpn = target_info->qp_info.rdma.qpn;
    wr.wr.ud.remote_qkey = target_info->qp_info.rdma.qkey;

    // record timestamp t3
    (void)gettimeofday(&timestamp3, NULL);
    header->timestamp.tv_sec2 = (uint64_t)timestamp2->tv_sec;
    header->timestamp.tv_usec2 = (uint64_t)timestamp2->tv_usec;
    header->timestamp.tv_sec3 = (uint64_t)timestamp3.tv_sec;
    header->timestamp.tv_usec3 = (uint64_t)timestamp3.tv_usec;
    header->magic = 0xAA55;

    ret = rs_ibv_post_send(ping_cb->pong_qp.ib_qp, &wr, &bad_wr);
    if (ret != 0) {
        target_info->state = RS_PING_PONG_TARGET_ERROR;
        hccp_err("rs_ibv_post_send failed, ret:%d", ret);
        return ret;
    }

    return ret;
}

STATIC void rs_pong_roce_handle_send(struct rs_ping_ctx_cb *ping_cb, int polled_cnt, struct timeval *timestamp2)
{
    struct ibv_wc *wc = NULL;
    int ret, i;

    wc = g_ping_qp_recv_wc;
    for (i = 0; i < polled_cnt; i++) {
        if (wc[i].status != IBV_WC_SUCCESS) {
            hccp_err("wr_id:0x%llx error cqe %s(%d)", wc[i].wr_id, rs_ibv_wc_status_str(wc[i].status), wc[i].status);
            continue;
        }

        ret = rs_pong_post_send(ping_cb, &wc[i], timestamp2);
        if (ret != 0) {
            hccp_err("rs_pong_post_send failed, wr_id:0x%llx", wc[i].wr_id);
            continue;
        }

        ret = rs_ping_common_post_recv(&ping_cb->ping_qp);
        if (ret != 0) {
            hccp_err("rs_ping_common_post_recv failed, ret:%d", ret);
            continue;
        }
    }

    ret = rs_ibv_req_notify_cq(ping_cb->ping_qp.recv_cq.ib_cq, 0);
    if (ret != 0) {
        hccp_err("rs_ibv_req_notify_cq failed, ret:%d", ret);
    }

    return;
}

STATIC int rs_pong_resolve_response_packet(struct rs_ping_ctx_cb *ping_cb, uint32_t sge_idx, struct timeval *timestamp4)
{
    struct rs_ping_target_info *target_info = NULL;
    struct rs_ping_payload_header *header = NULL;
    struct ibv_sge *recv_list = NULL;
    uint32_t rtt;
    int ret;

    recv_list = &ping_cb->pong_qp.recv_mr_cb.sge_list[sge_idx];
    // UD consume 40 Bytes for GRH
    header = (struct rs_ping_payload_header *)(uintptr_t)(recv_list->addr + RS_PING_PAYLOAD_HEADER_RESV_GRH);
    if (header->task_id != ping_cb->task_id) {
        hccp_warn("drop received packet, recv_task_id:%u, curr_task_id:%u", header->task_id, ping_cb->task_id);
        return 0;
    }

    header->timestamp.tv_sec4 = (uint64_t)timestamp4->tv_sec;
    header->timestamp.tv_usec4 = (uint64_t)timestamp4->tv_usec;
    rtt = rs_ping_get_trip_time(&header->timestamp);
    ret = rs_ping_roce_find_target_node(ping_cb, &header->target, &target_info);
    if (ret != 0) {
        hccp_err("rs_ping_roce_find_target_node failed, ret:%d qpn:%u gid:%016llx:%016llx rtt:%u", ret,
            header->target.rdma.qpn, header->target.rdma.gid.global.subnet_prefix,
            header->target.rdma.gid.global.interface_id, rtt);
        return ret;
    }

    (void)memset_s((void *)header, RS_PING_PAYLOAD_HEADER_MASK_SIZE, 0, RS_PING_PAYLOAD_HEADER_MASK_SIZE);
    RS_PTHREAD_MUTEX_LOCK(&target_info->trip_mutex);
    target_info->result_summary.recv_cnt++;
    target_info->result_summary.task_id = header->task_id;
    // rtt timeout, increase timeout_cnt
    if ((target_info->result_summary.task_attr.timeout_interval * RS_PING_MSEC_TO_USEC) < rtt) {
        target_info->result_summary.timeout_cnt++;
        hccp_dbg("recv_cnt:%u timeout_interval:%u rtt:%u timeout_cnt:%u", target_info->result_summary.recv_cnt,
            target_info->result_summary.task_attr.timeout_interval, rtt, target_info->result_summary.timeout_cnt);
        RS_PTHREAD_MUTEX_ULOCK(&target_info->trip_mutex);
        return 0;
    }

    // handle rtt_min, rtt_max, rtt_avg
    if (target_info->result_summary.rtt_min > rtt) {
        target_info->result_summary.rtt_min = rtt;
    }
    if (target_info->result_summary.rtt_max < rtt) {
        target_info->result_summary.rtt_max = rtt;
    }
    if (target_info->result_summary.rtt_avg == 0) {
        target_info->result_summary.rtt_avg = rtt;
    }
    target_info->result_summary.rtt_avg = (target_info->result_summary.rtt_avg + rtt) / 2U;
    RS_PTHREAD_MUTEX_ULOCK(&target_info->trip_mutex);
    return 0;
}

STATIC void rs_pong_roce_poll_rcq(struct rs_ping_ctx_cb *ping_cb)
{
    struct timeval timestamp = { 0 };
    struct ibv_cq *ev_cq = NULL;
    struct ibv_wc *wc = NULL;
    uint32_t recv_sge_idx;
    void *ev_ctx = NULL;
    int polled_cnt, i;
    int ret;

    // record timestamp t4
    (void)gettimeofday(&timestamp, NULL);

    ret = rs_ibv_get_cq_event(ping_cb->pong_qp.channel, &ev_cq, &ev_ctx);
    if (ret != 0) {
        hccp_err("rs_ibv_get_cq_event pong_qp.channel failed, ret:%d", ret);
        return;
    }

    if (ev_cq != ping_cb->pong_qp.recv_cq.ib_cq) {
        hccp_err("CQ event for unknown CQ");
        return;
    }
    ping_cb->pong_qp.recv_cq.num_events++;

    polled_cnt = rs_ibv_poll_cq(ev_cq, ping_cb->pong_qp.recv_cq.max_recv_wc_num, g_pong_qp_recv_wc);
    if (polled_cnt > ping_cb->pong_qp.recv_cq.max_recv_wc_num || polled_cnt < 0) {
        hccp_err("rs_ibv_poll_cq failed, ret:%d", polled_cnt);
        return;
    }

    wc = g_pong_qp_recv_wc;
    for (i = 0; i < polled_cnt; i++) {
        if (wc[i].status != IBV_WC_SUCCESS) {
            hccp_err("wr_id:0x%llx error cqe %s(%d)", wc[i].wr_id, rs_ibv_wc_status_str(wc[i].status), wc[i].status);
            continue;
        }
        recv_sge_idx = (uint32_t)wc[i].wr_id;
        if (recv_sge_idx >= ping_cb->pong_qp.recv_mr_cb.sge_num) {
            hccp_err("param err recv_sge_idx:%u > sge_num:%u", recv_sge_idx, ping_cb->pong_qp.recv_mr_cb.sge_num);
            continue;
        }

        // handle response packet result
        ret = rs_pong_resolve_response_packet(ping_cb, recv_sge_idx, &timestamp);
        if (ret != 0) {
            continue;
        }

        ret = rs_ping_common_post_recv(&ping_cb->pong_qp);
        if (ret != 0) {
            continue;
        }
    }

    ret = rs_ibv_req_notify_cq(ev_cq, 0);
    if (ret != 0) {
        hccp_err("rs_ibv_req_notify_cq failed, ret:%d", ret);
    }

    return;
}

STATIC int rs_ping_roce_get_target_result(struct rs_ping_ctx_cb *ping_cb, struct ping_target_comm_info *target,
    struct ping_result_info *result)
{
    struct rs_ping_target_info *target_info = NULL;
    int ret;

    ret = rs_ping_roce_find_target_node(ping_cb, &target->qp_info, &target_info);
    if (ret != 0) {
        hccp_err("rs_ping_roce_find_target_node failed, ret:%d qpn:%u gid:%016llx:%016llx", ret,
            target->qp_info.rdma.qpn, target->qp_info.rdma.gid.global.subnet_prefix,
            target->qp_info.rdma.gid.global.interface_id);
        return ret;
    }

    (void)memcpy_s(&result->summary, sizeof(struct ping_result_summary), &target_info->result_summary,
        sizeof(struct ping_result_summary));
    if (target_info->state == RS_PING_PONG_TARGET_FINISH) {
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

STATIC void rs_ping_roce_free_target_node(struct rs_ping_ctx_cb *ping_cb, struct rs_ping_target_info *target_info)
{
    RS_PTHREAD_MUTEX_LOCK(&ping_cb->ping_mutex);
    rs_list_del(&target_info->list);
    RS_PTHREAD_MUTEX_ULOCK(&ping_cb->ping_mutex);

    if (target_info->payload_size > 0 && target_info->payload_buffer != NULL) {
        free(target_info->payload_buffer);
        target_info->payload_buffer = NULL;
    }

    if (target_info->ah) {
        (void)rs_ibv_destroy_ah(target_info->ah);
    }
    return;
}

STATIC void rs_ping_pong_del_target_list(struct rs_ping_ctx_cb *ping_cb)
{
    struct rs_pong_target_info *pong_next = NULL;
    struct rs_ping_target_info *ping_next = NULL;
    struct rs_pong_target_info *pong_curr = NULL;
    struct rs_ping_target_info *ping_curr = NULL;

    // del ping_list
    RS_PTHREAD_MUTEX_LOCK(&ping_cb->ping_mutex);
    RS_LIST_GET_HEAD_ENTRY(ping_curr, ping_next, &ping_cb->ping_list, list, struct rs_ping_target_info);
    for (; (&ping_curr->list) != &ping_cb->ping_list;
        ping_curr = ping_next, ping_next = list_entry(ping_next->list.next, struct rs_ping_target_info, list)) {
        rs_list_del(&ping_curr->list);
        if (ping_curr->payload_size > 0 && ping_curr->payload_buffer != NULL) {
            free(ping_curr->payload_buffer);
            ping_curr->payload_buffer = NULL;
        }
        if (ping_curr->ah) {
            (void)rs_ibv_destroy_ah(ping_curr->ah);
        }
        (void)pthread_mutex_destroy(&ping_curr->trip_mutex);
        free(ping_curr);
        ping_curr = NULL;
    }
    RS_PTHREAD_MUTEX_ULOCK(&ping_cb->ping_mutex);

    // del pong_list
    RS_PTHREAD_MUTEX_LOCK(&ping_cb->pong_mutex);
    RS_LIST_GET_HEAD_ENTRY(pong_curr, pong_next, &ping_cb->pong_list, list, struct rs_pong_target_info);
    for (; (&pong_curr->list) != &ping_cb->pong_list;
        pong_curr = pong_next, pong_next = list_entry(pong_next->list.next, struct rs_pong_target_info, list)) {
        rs_list_del(&pong_curr->list);
        if (pong_curr->ah) {
            (void)rs_ibv_destroy_ah(pong_curr->ah);
        }
        free(pong_curr);
        pong_curr = NULL;
    }
    RS_PTHREAD_MUTEX_ULOCK(&ping_cb->pong_mutex);
}

STATIC void rs_ping_roce_ping_cb_deinit(unsigned int phy_id, struct rs_ping_ctx_cb *ping_cb)
{
    struct rs_cb *rscb = NULL;
    int ret;

    ret = rs_get_rs_cb(phy_id, &rscb);
    if (ret != 0) {
        hccp_err("rs_get_rs_cb failed, phy_id[%u] invalid, ret %d", phy_id, ret);
        return;
    }

    RS_PTHREAD_MUTEX_LOCK(&ping_cb->ping_mutex);
    ping_cb->task_status = RS_PING_TASK_RESET;
    RS_PTHREAD_MUTEX_ULOCK(&ping_cb->ping_mutex);

    rs_ping_pong_del_target_list(ping_cb);

    rs_ping_common_deinit_local_qp(rscb, ping_cb, &ping_cb->pong_qp);
    rs_ping_common_deinit_local_qp(rscb, ping_cb, &ping_cb->ping_qp);
    rs_ping_common_deinit_local_buffer(ping_cb);
    (void)rs_ibv_dealloc_pd(ping_cb->rdev_cb.ib_pd);
    (void)rs_ibv_close_device(ping_cb->rdev_cb.ib_ctx);
    rs_ibv_free_device_list(ping_cb->rdev_cb.dev_list);
}

STATIC void rs_ping_roce_add_target_success(struct ping_target_info *target, struct rs_ping_target_info *target_info)
{
    hccp_info("target ip:0x%llx payload_size:%u add success, qpn:%u uuid:0x%llx",
        target->remote_info.ip.addr.s_addr, target->payload.size, target_info->qp_info.rdma.qpn, target_info->uuid);
}

STATIC void rs_ping_roce_ping_cb_init_success(unsigned int phy_id, struct ping_init_attr *attr, unsigned int dev_index)
{
    hccp_run_info("ping_cb init success, phy_id:%u, local_ip:0x%x, dev_index:%u",
        phy_id, attr->dev.rdma.local_ip.addr.s_addr, dev_index);
}

STATIC void rs_ping_roce_cannot_find_target_node(unsigned int i, int ret, struct ping_target_comm_info target,
    unsigned int phy_id)
{
    hccp_err("rs_ping_roce_find_target_node i:%u failed, ret:%d ip:0x%llx qpn:%u phy_id:%u",i, ret,
        target.ip.addr.s_addr, target.qp_info.rdma.qpn, phy_id);
}

struct rs_ping_pong_ops g_rs_ping_roce_ops = {
    .check_ping_fd          = rs_ping_roce_check_fd,
    .check_pong_fd          = rs_pong_roce_check_fd,
    .init_ping_cb           = rs_ping_roce_ping_cb_init,
    .ping_find_target_node  = rs_ping_roce_find_target_node,
    .ping_alloc_target_node = rs_ping_roce_alloc_target_node,
    .reset_recv_buffer      = rs_ping_roce_reset_recv_buffer,
    .ping_post_send         = rs_ping_roce_post_send,
    .ping_poll_scq          = rs_ping_roce_poll_scq,
    .ping_poll_rcq          = rs_ping_roce_poll_rcq,
    .pong_handle_send       = rs_pong_roce_handle_send,
    .pong_poll_rcq          = rs_pong_roce_poll_rcq,
    .get_target_result      = rs_ping_roce_get_target_result,
    .ping_free_target_node  = rs_ping_roce_free_target_node,
    .deinit_ping_cb         = rs_ping_roce_ping_cb_deinit,
};

struct rs_ping_pong_dfx g_rs_ping_roce_dfx = {
    .add_target_success           = rs_ping_roce_add_target_success,
    .init_ping_cb_success         = rs_ping_roce_ping_cb_init_success,
    .ping_cannot_find_target_node = rs_ping_roce_cannot_find_target_node,
};

struct rs_ping_pong_ops *rs_ping_roce_get_ops(void) {
    return &g_rs_ping_roce_ops;
}

struct rs_ping_pong_dfx *rs_ping_roce_get_dfx(void) {
    return &g_rs_ping_roce_dfx;
}

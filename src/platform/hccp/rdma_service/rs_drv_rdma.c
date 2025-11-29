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
#include <unistd.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <fcntl.h>

#include "securec.h"
#include "rs.h"
#include "ra_rs_err.h"
#include "rs_inner.h"
#include "rs_socket.h"
#include "verbs_exp.h"
#include "dl_hal_function.h"
#include "dl_ibverbs_function.h"
#include "rs_drv_rdma.h"

int gMaxCqeNum = 0;
#define INVALID_EVENT -1

struct ibv_wc gWcBuf[RS_WC_NUM];

struct rs_cqe_err_info gRsCqeErr;

int RsDrvInitCqeErrInfo(void)
{
    struct rs_cqe_err_info *errInfo = &gRsCqeErr;
    int ret;

    ret = pthread_mutex_init(&errInfo->mutex, NULL);
    CHK_PRT_RETURN(ret, hccp_err("rscb mutex_init failed ret %d!, normal ret 0", ret), -ESYSFUNC);
    ret = memset_s(&errInfo->info, sizeof(struct cqe_err_info), 0, sizeof(struct cqe_err_info));
    if (ret != 0) {
        pthread_mutex_destroy(&errInfo->mutex);
    }
    CHK_PRT_RETURN(ret, hccp_err("memset_s failed ret[%d]", ret), -ESAFEFUNC);
    return 0;
}

void RsDrvDeinitCqeErrInfo(void)
{
    struct rs_cqe_err_info *errInfo = &gRsCqeErr;

    pthread_mutex_destroy(&errInfo->mutex);
    return;
}

STATIC void RsDrvSaveCqeErrInfo(uint32_t status, struct rs_qp_cb *qpCb)
{
    struct rs_cqe_err_info *errInfo = &gRsCqeErr;
    struct cqe_err_info *tempInfo = &errInfo->info;

    RS_PTHREAD_MUTEX_LOCK(&errInfo->mutex);
    if (tempInfo->status != 0) {
        RS_PTHREAD_MUTEX_ULOCK(&errInfo->mutex);
        hccp_run_info("over status=[0x%x], drop qpn[0x%x] err cqe status=[0x%x]",
            tempInfo->status, qpCb->qp_info_lo.qpn, status);
        return;
    }
    tempInfo->status = status;
    tempInfo->qpn = (uint32_t)qpCb->qp_info_lo.qpn;
    RsGetCurTime(&tempInfo->time);
    RS_PTHREAD_MUTEX_ULOCK(&errInfo->mutex);

    return;
}

STATIC void RsDrvSaveQpCqeErrInfo(uint32_t status, struct rs_qp_cb *qpCb)
{
    RS_PTHREAD_MUTEX_LOCK(&qpCb->cqe_err_info.mutex);
    if (qpCb->cqe_err_info.info.status != 0) {
        RS_PTHREAD_MUTEX_ULOCK(&qpCb->cqe_err_info.mutex);
        return;
    }
    qpCb->cqe_err_info.info.status = status;
    qpCb->cqe_err_info.info.qpn = (uint32_t)qpCb->qp_info_lo.qpn;
    RsGetCurTime(&qpCb->cqe_err_info.info.time);
    RS_PTHREAD_MUTEX_ULOCK(&qpCb->cqe_err_info.mutex);

    RS_PTHREAD_MUTEX_LOCK(&qpCb->rdev_cb->cqe_err_cnt_mutex);
    qpCb->rdev_cb->cqe_err_cnt++;
    RS_PTHREAD_MUTEX_ULOCK(&qpCb->rdev_cb->cqe_err_cnt_mutex);

    return;
}

int RsDrvGetCqeErrInfo(struct cqe_err_info *info)
{
    struct rs_cqe_err_info *errInfo = &gRsCqeErr;
    struct cqe_err_info *tempInfo = &errInfo->info;
    int ret;

    CHK_PRT_RETURN(info == NULL, hccp_err("info is NULL"), -EINVAL);

    RS_PTHREAD_MUTEX_LOCK(&errInfo->mutex);
    if (tempInfo->status == 0) {
        RS_PTHREAD_MUTEX_ULOCK(&errInfo->mutex);
        return 0;
    }
    hccp_run_info("status=[%u]", tempInfo->status);
    ret = memcpy_s(info, sizeof(struct cqe_err_info), &errInfo->info, sizeof(struct cqe_err_info));
    if (ret) {
        hccp_err("memcpy_s  failed");
        RS_PTHREAD_MUTEX_ULOCK(&errInfo->mutex);
        return -ESAFEFUNC;
    }

    ret = memset_s(&errInfo->info, sizeof(struct cqe_err_info), 0, sizeof(struct cqe_err_info));
    RS_PTHREAD_MUTEX_ULOCK(&errInfo->mutex);
    CHK_PRT_RETURN(ret, hccp_err("memset_s failed ret[%d], buf len:%u", ret, sizeof(struct cqe_err_info)), -ESAFEFUNC);
    return 0;
}

STATIC void RsRetryTimeoutExceptionCheck(struct rs_rdev_cb *rdevCb, struct ibv_wc *wc)
{
    int ret = 0;

    /* sensor may not support, handle is 0 */
    if (rdevCb->sensor_handle == 0) {
        return;
    }

    if (wc->status != IBV_WC_RETRY_EXC_ERR) {
        return;
    }

    /* The notification alarm framework does not filter alarms. In this example, only one notification
        alarm is reported by a single process, which does not need to be accurate. Therefore, no lock is used. */
    if (rdevCb->sensor_update_cnt == 0) {
        ret = DlHalSensorNodeUpdateState(rdevCb->logic_devid, rdevCb->sensor_handle,
            RDMA_CQE_ERR_RETRY_TIMEOUT_EVENT_TYPE, GENERAL_EVENT_TYPE_ONE_TIME);
        if (ret == 0) {
            rdevCb->sensor_update_cnt++;
        }
    }

    hccp_warn("update sensor state logic_devid(%u), qpn(%u), sensor_update_cnt(%d),ret(%d)\n",
        rdevCb->logic_devid, wc->qp_num, rdevCb->sensor_update_cnt, ret);
}

STATIC void RsCqeCallbackProcess(struct rs_qp_cb *qpCb, struct ibv_wc *wc, struct ibv_cq *evCq)
{
    if (wc->status != IBV_WC_SUCCESS && wc->status != IBV_WC_WR_FLUSH_ERR) {
        hccp_err("Failed status [%s] [%u], wr[%llu]",
            RsIbvWcStatusStr(wc->status), wc->status, wc->wr_id);
        RsDrvSaveCqeErrInfo(wc->status, qpCb);
        RsDrvSaveQpCqeErrInfo(wc->status, qpCb);
        RsRetryTimeoutExceptionCheck(qpCb->rdev_cb, wc);
    }

    return;
}

void RsDrvPollSrqCqHandle(struct rs_qp_cb *qpCb)
{
    struct ibv_cq *evCq = NULL;
    void *evCtx = NULL;

    if (RsIbvGetCqEvent(qpCb->srq_context->channel, &evCq, &evCtx)) {
        hccp_err("Failed to get cq_event");
        return;
    }

    if (evCq != qpCb->ib_recv_cq || evCtx == NULL) {
        hccp_err("CQ event for unknown CQ");
        return;
    }

    ++qpCb->srq_context->num_recv_cq_events;

    struct event_summary *evCtxTmp = (struct event_summary *)evCtx;
    if ((int)evCtxTmp->event_id != INVALID_EVENT) {
        hccp_info("SubmitEvent: event id:%d, pid:%d, grp id:%u, dev id:%u",
            evCtxTmp->event_id, evCtxTmp->pid, evCtxTmp->grp_id, qpCb->rdev_cb->rs_cb->chip_id);
        int ret = DlHalEschedSubmitEvent(qpCb->rdev_cb->rs_cb->chip_id, evCtx);
        if (ret) {
            hccp_warn("halEschedSubmitEvent unsuccessful, ret:%d", ret);
        }
    }

    return;
}

void RsDrvPollCqHandle(struct rs_qp_cb *qpCb)
{
    struct ibv_cq *evCq = NULL;
    void *evCtx = NULL;
    int ne, i;

    if (RsIbvGetCqEvent(qpCb->channel, &evCq, &evCtx)) {
        hccp_err("Failed to get cq_event");
        return;
    }

    if (evCq != qpCb->ib_send_cq && evCq != qpCb->ib_recv_cq) {
        hccp_err("CQ event for unknown CQ");
        return;
    }

    if (evCq == qpCb->ib_recv_cq) {
        ++qpCb->num_recv_cq_events;
    } else {
        ++qpCb->num_send_cq_events;
    }

    if (evCtx != NULL) {
        struct event_summary *evCtxTmp = (struct event_summary *)evCtx;
        if ((int)(evCtxTmp->event_id) != INVALID_EVENT) {
            hccp_info("SubmitEvent: event id:%d, pid:%d, grp id:%u, dev id:%u",
                evCtxTmp->event_id, evCtxTmp->pid, evCtxTmp->grp_id, qpCb->rdev_cb->rs_cb->chip_id);
            int ret = DlHalEschedSubmitEvent(qpCb->rdev_cb->rs_cb->chip_id, evCtx);
            if (ret) {
                hccp_warn("halEschedSubmitEvent unsuccessful, ret:%d", ret);
            }
        }
        return;
    }

    ne = ibv_poll_cq(evCq, RS_WC_NUM, gWcBuf);
    if (ne > RS_WC_NUM || ne < 0) {
        hccp_err("poll CQ failed %d", ne);
        return;
    }
    if (gMaxCqeNum < ne) {
        gMaxCqeNum = ne;
        hccp_run_info("rs_drv_poll_cq_handle: max_cqe_num=[%d]", gMaxCqeNum);
    }

    if (ibv_req_notify_cq(evCq, 0)) {
        hccp_err("Couldn't request CQ notification");
        return;
    }
    qpCb->rdev_cb->poll_cqe_num += ne;
    for (i = 0; i < ne; ++i) {
        RsCqeCallbackProcess(qpCb, &(gWcBuf[i]), evCq);
    }
    return;
}

int RsDrvCompareIpGid(int family, union hccp_ip_addr localIp, union ibv_gid *gid)
{
    unsigned int gidV4[RS_GID_SEQ_NUM];

    if (family == AF_INET6) {
        if (memcmp(gid, &(localIp.addr6), sizeof(union ibv_gid)) == 0) {
            return 0;
        }
    } else {
        gidV4[RS_GID_SEQ_ZERO] = 0;
        gidV4[RS_GID_SEQ_ONE] = 0;
        /* The gid format generated by ipv4 is filled with 0xFFFF in [33, 48] */
        gidV4[RS_GID_SEQ_TWO]   = htonl(0x0000FFFF);
        gidV4[RS_GID_SEQ_THREE] = localIp.addr.s_addr;
        if (memcmp(gid, &(gidV4), sizeof(union ibv_gid)) == 0) {
            return 0;
        }
    }

    return -ENODEV;
}

int RsDrvGetGidIndex(struct rs_rdev_cb *rdevCb, struct ibv_port_attr *attr, int *idx)
{
    static const char *portStates[] = {"Nop", "Down", "Init", "Armed", "", "Active Defer"};
    enum ibv_gid_type_sysfs type;
    union ibv_gid gidTmp;
    int gidIdx = -1;
    int ret;
    int i;

    ret = RsIbvQueryPort(rdevCb->ib_ctx, rdevCb->ib_port, attr);
    CHK_PRT_RETURN(ret, hccp_err("ibv_query_port failed ret[%d]", ret), -EOPENSRC);

    // link maybe suffer from intermittent disconnection, should continue process
    if (attr->state != IBV_PORT_ACTIVE) {
        hccp_warn("port number %u state is %s", rdevCb->ib_port, portStates[attr->state]);
    }

    for (i = 0; i < attr->gid_tbl_len; i++) {
        ret = RsIbvQueryGidType(rdevCb->ib_ctx, rdevCb->ib_port, (unsigned int)i, &type);
        CHK_PRT_RETURN(ret, hccp_err("query gid type failed i %d, ret %d", i, ret), -EOPENSRC);
        if (type != IBV_GID_TYPE_SYSFS_ROCE_V2) {
            continue;
        }

        ret = RsIbvQueryGid(rdevCb->ib_ctx, rdevCb->ib_port, i, &gidTmp);
        CHK_PRT_RETURN(ret, hccp_err("query gid failed i %d, ret %d", i, ret), -EOPENSRC);

        ret = RsDrvCompareIpGid((int)rdevCb->local_ip.family, rdevCb->local_ip.bin_addr, &gidTmp);
        if (ret == 0) {
            gidIdx = i;
            break;
        }
    }

    if (gidIdx == -1) {
        hccp_err("get idx failed, attr->gid_tbl_len:%d", attr->gid_tbl_len);
        return -ENODEV;
    }

    hccp_dbg("GID index is %d", gidIdx);
    *idx = gidIdx;
    return 0;
}

STATIC int RsDrvGetSupportLite(struct rs_rdev_cb *rdevCb, int qpMode, unsigned int aiOpSupport)
{
    // bypass rdma lite when ai_op_support was set
    if (aiOpSupport == 1) {
        return 0;
    }

    if (qpMode == RA_RS_OP_QP_MODE ||
        qpMode == RA_RS_OP_QP_MODE_EXT) {
        return rdevCb->support_lite;
    }

    return 0;
}

int RsDrvCreateCqWithAttrs(struct rs_qp_cb *qpCb, int isExt, struct cq_ext_attr *cqAttr)
{
    int sendEqNum = cqAttr->send_cq_comp_vector;
    int recvEqNum = cqAttr->recv_cq_comp_vector;
    struct rdma_lite_device_cq_init_attr attr = {
        .cq_type = qpCb->qp_mode,
        .part_id = 0,
        .lite_op_supported = 0,
        .mem_align = qpCb->mem_align,
        .mem_idx = qpCb->mem_resp.mem_data.mem_idx,
        .ai_op_support = qpCb->ai_op_support,
        .grp_id = qpCb->grp_id,
        .cq_cstm_flag = qpCb->cq_cstm_flag,
    };
    struct ibv_comp_channel *channel = qpCb->channel;

    attr.lite_op_supported = RsDrvGetSupportLite(qpCb->rdev_cb, qpCb->qp_mode, qpCb->ai_op_support);
    // caller poll cq
    if (attr.lite_op_supported != 0 || attr.cq_cstm_flag != 0) {
        channel = NULL;
        sendEqNum = 0;
        recvEqNum = 0;
    }

    hccp_dbg("create cq start");
    if (isExt == 1) {
        qpCb->ib_send_cq = RsIbvExpCreateCq(qpCb->rdev_cb->ib_ctx, cqAttr->send_cq_depth,
            NULL, channel, sendEqNum, &attr, &qpCb->qp_resp.send_cq_data);
        hccp_info("rs_ibv_exp_create_cq");
    } else {
        qpCb->ib_send_cq = RsIbvCreateCq(qpCb->rdev_cb->ib_ctx, cqAttr->send_cq_depth,
            NULL, channel, sendEqNum);
    }

    /* A return value of NULL indicates an OutOfMemoryError(OOM) has occurred */
    CHK_PRT_RETURN(qpCb->ib_send_cq == NULL, hccp_err("ibv create send cq failed"), -ENOMEM);

    if (isExt == 1) {
        qpCb->ib_recv_cq = RsIbvExpCreateCq(qpCb->rdev_cb->ib_ctx, cqAttr->recv_cq_depth,
            NULL, channel, recvEqNum, &attr, &qpCb->qp_resp.recv_cq_data);
        hccp_info("rs_ibv_exp_create_cq");
    } else {
        qpCb->ib_recv_cq = RsIbvCreateCq(qpCb->rdev_cb->ib_ctx, cqAttr->recv_cq_depth,
            NULL, channel, recvEqNum);
    }

    /* A return value of NULL indicates an OutOfMemoryError(OOM) has occurred */
    if (qpCb->ib_recv_cq == NULL) {
        hccp_err("ibv create recv cq failed");
        (void)RsIbvDestroyCq(qpCb->ib_send_cq);
        return -ENOMEM;
    }

    hccp_info("create cq success");
    return 0;
}

#define RS_DRV_CQ_DEPTH         16384
#define RS_DRV_CQ_128_DEPTH     128
#define RS_DRV_CQ_32K_DEPTH     32768

int RsDrvCreateCq(struct rs_qp_cb *qpCb, int isExt)
{
    struct cq_ext_attr cqAttr = {0};

    cqAttr.send_cq_depth = qpCb->send_cq_depth;
    cqAttr.send_cq_comp_vector = qpCb->eq_num;
    cqAttr.recv_cq_depth = qpCb->recv_cq_depth;
    cqAttr.recv_cq_comp_vector = qpCb->eq_num;

    return RsDrvCreateCqWithAttrs(qpCb, isExt, &cqAttr);
}

void RsDrvEventDestroy(struct event_summary *event)
{
    if (event != NULL) {
        free(event);
        event = NULL;
    }
}

void RsDrvDestroyCq(struct rs_qp_cb *qpCb)
{
    (void)RsIbvDestroyCq(qpCb->ib_recv_cq);
    (void)RsIbvDestroyCq(qpCb->ib_send_cq);
    (void)RsDrvEventDestroy(qpCb->recv_event);
    (void)RsDrvEventDestroy(qpCb->send_event);
}

int RsDrvQpStateModifytoReset(struct rs_qp_cb *qpCb)
{
    struct ibv_qp_init_attr qpInitAttr = {0};
    struct ibv_qp_attr attr = {0};
    int ret;

    attr.qp_state = IBV_QPS_RESET;
    ret = RsIbvModifyQp(qpCb->ib_qp, &attr, IBV_QP_STATE);
    CHK_PRT_RETURN(ret, hccp_err("[modify]qpn[%d] modify to reset failed, ret %d", qpCb->qp_info_lo.qpn, ret), ret);

    (void)memset_s(&attr, sizeof(struct ibv_qp_attr), 0, sizeof(struct ibv_qp_attr));

    ret = RsIbvQueryQp(qpCb->ib_qp, &attr, IBV_QP_STATE, &qpInitAttr);
    if ((ret != 0) || attr.qp_state != IBV_QPS_RESET) {
        hccp_err("query qpn[%d] attr failed, ret %d or qp state %d is not RESET",
            qpCb->qp_info_lo.qpn, ret, attr.qp_state);
        return ret;
    }

    hccp_info("qpn[%d] modify to reset success", qpCb->qp_info_lo.qpn);
    return 0;
}

int RsDrvQpStateModifytoInit(struct rs_qp_cb *qpCb, struct ibv_qp_attr *attr)
{
    int ret;

    attr->qp_state = IBV_QPS_INIT;
    attr->pkey_index = 0;
    attr->port_num = qpCb->rdev_cb->ib_port;
    attr->qp_access_flags = DEFAULT_ACCESS_FLAG;
    ret = RsIbvModifyQp(qpCb->ib_qp, attr, IBV_QP_STATE |
                        IBV_QP_PKEY_INDEX | IBV_QP_PORT | IBV_QP_ACCESS_FLAGS);
    CHK_PRT_RETURN(ret, hccp_err("[modify]qpn[%d] modify to init failed, ret %d", qpCb->qp_info_lo.qpn, ret), ret);

    hccp_info("qpn[%d] modify to init success", qpCb->qp_info_lo.qpn);
    return 0;
}

enum ibv_mtu RsDrvSetMtu(struct rs_qp_cb *qpCb)
{
    int ret;
    struct ibv_port_attr portAttr = {0};
    enum ibv_mtu currMtu;

    ret = RsIbvQueryPort(qpCb->rdev_cb->ib_ctx, qpCb->rdev_cb->ib_port, &portAttr);
    CHK_PRT_RETURN(ret, hccp_err("Error when trying to query port, ret[%d]", ret), -EOPENSRC);
#ifndef CA_CONFIG_LLT
    currMtu = portAttr.active_mtu;
#else
    curr_mtu = IBV_MTU_1024;
#endif

    return currMtu;
}

int RsDrvQpStateModifytoRtr(struct rs_qp_cb *qpCb, struct ibv_qp_attr *attr)
{
    struct ibv_port_attr portAttr = { 0 };
    int ret;

    attr->qp_state          = IBV_QPS_RTR;
    attr->dest_qp_num       = (uint32_t)qpCb->qp_info_rem.qpn;
    attr->rq_psn            = (uint32_t)qpCb->qp_info_rem.psn;
    attr->min_rnr_timer     = RS_QP_ATTR_MIN_RNR_TIMER;
    (attr->ah_attr).is_global   = 0;
    (attr->ah_attr).dlid        = qpCb->qp_info_rem.lid;
    (attr->ah_attr).sl      = qpCb->qos_attr.sl;
    (attr->ah_attr).src_path_bits   = 0;
    (attr->ah_attr).port_num    = qpCb->rdev_cb->ib_port;

    attr->path_mtu = RsDrvSetMtu(qpCb);
    if (qpCb->rdev_cb->rs_cb->hccp_mode == NETWORK_PEER_ONLINE) {
        attr->max_dest_rd_atomic = RS_MAX_RD_ATOMIC_NUM_PEER_ONLINE;
    } else {
        attr->max_dest_rd_atomic = RS_MAX_RD_ATOMIC_NUM;
        CHK_PRT_RETURN(attr->path_mtu < IBV_MTU_1024, hccp_err("qpn[%d] failed to set mtu, mtu[%d] < [%d]",
            qpCb->qp_info_lo.qpn, attr->path_mtu, IBV_MTU_1024), -EPERM);
    }

    // get gid_idx dynamically
    ret = RsDrvGetGidIndex(qpCb->rdev_cb, &portAttr, &qpCb->qp_info_lo.gid_idx);
    CHK_PRT_RETURN(ret, hccp_err("rs_drv_get_gid_index failed ret[%d], qpn[%d] gidIdx[%d]",
        ret, qpCb->qp_info_lo.qpn, qpCb->qp_info_lo.gid_idx), ret);

    if (qpCb->qp_info_rem.gid.global.interface_id) {
        attr->ah_attr.is_global = 1;
        attr->ah_attr.grh.hop_limit = 1;
        attr->ah_attr.grh.dgid = qpCb->qp_info_rem.gid;
        attr->ah_attr.grh.sgid_index = qpCb->qp_info_lo.gid_idx;
    }

    (attr->ah_attr).grh.traffic_class = qpCb->qos_attr.tc;
    ret = RsIbvModifyQp(qpCb->ib_qp, attr,
                           IBV_QP_STATE | IBV_QP_AV |
                           IBV_QP_PATH_MTU | IBV_QP_DEST_QPN |
                           IBV_QP_RQ_PSN | IBV_QP_MAX_DEST_RD_ATOMIC |
                           IBV_QP_MIN_RNR_TIMER);
    CHK_PRT_RETURN(ret, hccp_err("qpn[%d] failed to modify QP to RTR, ibv_modify_qp failed ret[%d], errno[%d]",
        qpCb->qp_info_lo.qpn, ret, errno), -EOPENSRC);
    hccp_info("qp qos attr: qpn[%d] tc[%u] sl[%u]", qpCb->qp_info_lo.qpn, qpCb->qos_attr.tc, qpCb->qos_attr.sl);
    return 0;
}

int RsDrvQpStateModifytoRts(struct rs_qp_cb *qpCb, struct ibv_qp_attr *attr)
{
    int ret;
    attr->qp_state      = IBV_QPS_RTS;
    attr->timeout       = (uint8_t)qpCb->timeout;
    attr->retry_cnt     = (uint8_t)qpCb->retry_cnt;
    attr->rnr_retry     = RS_QP_ATTR_RNR_RETRY;
    attr->sq_psn        = (uint32_t)qpCb->qp_info_lo.psn;
    if (qpCb->rdev_cb->rs_cb->hccp_mode == NETWORK_PEER_ONLINE) {
        attr->max_rd_atomic = RS_MAX_RD_ATOMIC_NUM_PEER_ONLINE;
    } else {
        attr->max_rd_atomic = RS_MAX_RD_ATOMIC_NUM;
    }
    ret = RsIbvModifyQp(qpCb->ib_qp, attr,
                           IBV_QP_STATE | IBV_QP_TIMEOUT |
                           IBV_QP_RETRY_CNT | IBV_QP_RNR_RETRY |
                           IBV_QP_SQ_PSN | IBV_QP_MAX_QP_RD_ATOMIC);
    CHK_PRT_RETURN(ret, hccp_err("qpn[%d] failed to modify QP to RTS, ibv_modify_qp failed ret[%d]",
        qpCb->qp_info_lo.qpn, ret), -EOPENSRC);
    hccp_info("qp rdma attr: qpn[%d] timeout[%u] retrycnt[%u]", qpCb->qp_info_lo.qpn, qpCb->timeout,
        qpCb->retry_cnt);
    return 0;
}

struct ibv_mr* RsDrvMrReg(struct ibv_pd *pd, char *addr, size_t length, int access)
{
    return RsIbvRegMr(pd, addr, length, access);
}

struct ibv_mr* RsDrvExpMrReg(struct ibv_pd *pd, char *addr, size_t length,
    int access, struct roce_process_sign roceSign)
{
    return RsIbvExpRegMr(pd, addr, length, access, roceSign);
}

int RsDrvMrDereg(struct ibv_mr *ibMr)
{
    return RsIbvDeregMr(ibMr);
}

#ifdef CUSTOM_INTERFACE
STATIC int RsOpenBackupIbCtx(struct rs_rdev_cb *rdevCb)
{
    struct ibv_context *ibCtxTmp = NULL;
    struct rdev rdevInfo = { 0 };
    int gidIndex = -1;
    int ret;
    int i;

    (void)memcpy_s(&rdevInfo, sizeof(struct rdev), &rdevCb->backup_info.rdev_info, sizeof(struct rdev));
    for (i = 0; (i < rdevCb->dev_num) && (rdevCb->dev_list[i] != NULL); ++i) {
        ibCtxTmp = RsIbvOpenDevice(rdevCb->dev_list[i]);
        CHK_PRT_RETURN(ibCtxTmp == NULL, hccp_err("ibv_open_device with backup failed, errno:%d", errno), -ENODEV);
        ret = RsQueryGid(rdevInfo, ibCtxTmp, rdevCb->ib_port, &gidIndex);
        if (ret == 0) {
            rdevCb->backup_info.ib_ctx = ibCtxTmp;
            return 0;
        } else if (ret == -EEXIST) {
            RsIbvCloseDevice(ibCtxTmp);
        } else {
            RsIbvCloseDevice(ibCtxTmp);
            hccp_err("rs_query_gid failed, ret:%d", ret);
            return ret;
        }
    }

    CHK_PRT_RETURN(i == rdevCb->dev_num, hccp_err("can not find ib_ctx for phy_id[%u] local_ip[0x%x] in dev_list!",
        rdevInfo.phy_id, rdevInfo.local_ip.addr.s_addr), -EEXIST);
    return 0;
}

void RsCloseBackupIbCtx(struct rs_rdev_cb *rdevCb)
{
    // no need to close backup device
    if (!rdevCb->backup_info.backup_flag || rdevCb->backup_info.ib_ctx == NULL) {
        return;
    }

    RsIbvCloseDevice(rdevCb->backup_info.ib_ctx);
}
#endif

STATIC int RsDrvQueryNotify(struct rs_rdev_cb *rdevCb)
{
    int ret = 0;

    if (rdevCb->notify_type == NO_USE) {
        return 0;
    }

#ifdef CUSTOM_INTERFACE
    if (rdevCb->backup_info.backup_flag) {
        // open backup device to get ib_ctx to get backup notify va and size
        ret = RsOpenBackupIbCtx(rdevCb);
        CHK_PRT_RETURN(ret, hccp_err("rs_open_backup_ib_ctx failed, ret:%d", ret), ret);

        ret = RsIbvExpQueryNotify(rdevCb->backup_info.ib_ctx, &rdevCb->notify_va_base, &rdevCb->notify_size);
        if (ret != 0) {
            RsCloseBackupIbCtx(rdevCb);
            hccp_err("rs_ibv_exp_query_notify with backup ctx failed, ret:%d", ret);
            return ret;
        }
    } else {
        ret = RsIbvExpQueryNotify(rdevCb->ib_ctx, &rdevCb->notify_va_base, &rdevCb->notify_size);
        CHK_PRT_RETURN(ret, hccp_err("rs_ibv_exp_query_notify failed, ret:%d", ret), ret);
    }
#endif
    hccp_info("chip_id:%u, RsDrvQueryNotify ok, notify va:0x%llx, size:%llu", rdevCb->rs_cb->chip_id,
        rdevCb->notify_va_base, rdevCb->notify_size);
    return ret;
}

int RsDrvQueryNotifyAndAllocPd(struct rs_rdev_cb *rdevCb)
{
    int ret = 0;

    ret = RsDrvQueryNotify(rdevCb);
    CHK_PRT_RETURN(ret, hccp_err("rs_drv_query_notify failed, ret %d", ret), ret);

    rdevCb->ib_pd = RsIbvAllocPd(rdevCb->ib_ctx);
    if (rdevCb->ib_pd == NULL) {
#ifdef CUSTOM_INTERFACE
        RsCloseBackupIbCtx(rdevCb);
#endif
        hccp_err("rs_ibv_alloc_pd failed, errno:%d", errno);
        return -ENOMEM;
    }

    return 0;
}

int RsDrvRegNotifyMr(struct rs_rdev_cb *rdevCb)
{
#ifdef CUSTOM_INTERFACE
    struct roce_process_sign roceSign = {0};
#endif
    int access = DEFAULT_ACCESS_FLAG;

    rdevCb->notify_access = access;
    switch (rdevCb->notify_type) {
        case NO_USE: return 0;
        case NOTIFY: {
            rdevCb->notify_mr = RsIbvRegMr(rdevCb->ib_pd, (void *)(uintptr_t)rdevCb->notify_va_base,
                rdevCb->notify_size, access);
            break;
        }
        case EVENTID: {
#ifdef CUSTOM_INTERFACE
            rdevCb->notify_mr = RsIbvExpRegMr(rdevCb->ib_pd, (void *)(uintptr_t)rdevCb->notify_va_base,
                rdevCb->notify_size, access, roceSign);
            break;
#else
            return 0;
#endif
        }
        default: {
            hccp_err("rs_drv_reg_notify_mr failed! notify_type = %u", rdevCb->notify_type);
            return -EINVAL;
        }
    }

    CHK_PRT_RETURN(rdevCb->notify_mr == NULL, hccp_err("ibv_reg_mr addr[0x%llx] len[%llu] errno[%d]failed",
        rdevCb->notify_va_base, rdevCb->notify_size, errno), -EACCES);

    hccp_info("ibv_reg_mr ok");
    return 0;
}

STATIC void RsBuildRecvWr(struct recv_wrlist_data *wr, struct ibv_sge *list, struct ibv_recv_wr *ibWr)
{
    list->addr = (uintptr_t)wr->mem_list.addr;
    list->length = wr->mem_list.len;
    list->lkey = wr->mem_list.lkey;

    ibWr->sg_list = list;
    ibWr->wr_id = wr->wr_id;
    ibWr->num_sge = 1; /* only support one sge */
    return;
}

int RsDrvPostRecv(struct rs_qp_cb *qpCb, struct recv_wrlist_data *wr, unsigned int recvNum,
    unsigned int *completeNum)
{
    int ret = 0;
    unsigned int i, index;
    struct ibv_recv_wr *badWr = NULL;

    CHK_PRT_RETURN(recvNum == 0, hccp_err("recv_num[%u] is invalid!", recvNum), -EINVAL);

    struct ibv_recv_wr *recvWr = (struct ibv_recv_wr *)calloc(recvNum, sizeof(struct ibv_recv_wr));
    CHK_PRT_RETURN(recvWr == NULL, hccp_err("calloc recv_wr failed!"), -ENOSPC);

    struct ibv_sge *list = (struct ibv_sge *)calloc(recvNum, sizeof(struct ibv_sge));
    if (list == NULL) {
        hccp_err("calloc list failed!");
        ret = -ENOSPC;
        goto alloc_sge_fail;
    }

    for (i = 0; i < recvNum; i++) {
        RsBuildRecvWr(&wr[i], &list[i], &recvWr[i]);
        index = i + 1; // for fix pclint warning
        recvWr[i].next = (i < recvNum - 1) ? &(recvWr[index]) : NULL;
    }

    ret = RsIbvPostRecv(qpCb->ib_qp, recvWr, &badWr);
    if (ret == 0) {
        *completeNum = recvNum;
    } else if (ret == -ENOMEM) {
        *completeNum = (unsigned int)((void *)badWr - (void *)recvWr) / sizeof(struct ibv_recv_wr);
        hccp_dbg("post recv wqe overflow, completeNum[%d]", *completeNum);
    } else {
        hccp_err("ibv_post_recv failed, ret[%d]", ret);
        *completeNum = 0;
    }
    qpCb->recv_wr_num = qpCb->recv_wr_num + (*completeNum);

    free(list);
    list = NULL;

alloc_sge_fail:
    free(recvWr);
    recvWr = NULL;
    return (ret == -ENOMEM) ? 0 : ret;
}

int RsDrvSendExp(struct rs_qp_cb *qpCb, struct rs_mr_cb *mrCb,
                    struct rs_mr_cb *remMrCb, struct send_wr *wr, struct send_wr_rsp *wrRsp)
{
    int i;
    int ret;
    struct ibv_sge list[RS_SGLIST_MAX];
    struct ibv_send_wr ibWr = {
        .sg_list    = list,
        .opcode     = wr->op,
        .send_flags = wr->send_flag,
    };
    struct ibv_send_wr *badWr = NULL;
    struct wr_exp_rsp expRsp = {0};

    for (i = 0; i < wr->buf_num && i < RS_SGLIST_MAX; i++) {
        list[i].addr = (uintptr_t)wr->buf_list[i].addr;
        list[i].length = wr->buf_list[i].len;
        list[i].lkey = mrCb->ib_mr->lkey;
    }

    ibWr.num_sge = i;
    ibWr.wr_id = mrCb->wr_id;
    // send op has no rem_mr, no need to assign
    if (wr->op != RA_WR_SEND && wr->op != RA_WR_SEND_WITH_IMM) {
        ibWr.wr.rdma.rkey = remMrCb->mr_info.rkey;
        ibWr.wr.rdma.remote_addr = wr->dst_addr;
    }

    ret = RsIbvExpPostSend(qpCb->ib_qp, &ibWr, &badWr, &expRsp);
    if (ret) {
        hccp_err("rs_ibv_exp_post_send failed ret %d", ret);
    }

    if (qpCb->qp_mode == RA_RS_GDR_TMPL_QP_MODE) {
        wrRsp->wqe_tmp.sq_index = (unsigned int)qpCb->sq_index;
        wrRsp->wqe_tmp.wqe_index = expRsp.wqe_index;
    } else if (qpCb->qp_mode == RA_RS_OP_QP_MODE ||
               qpCb->qp_mode == RA_RS_OP_QP_MODE_EXT ||
               qpCb->qp_mode == RA_RS_GDR_ASYN_QP_MODE) {
        wrRsp->db.db_index = (unsigned int)qpCb->db_index;
        wrRsp->db.db_info = expRsp.db_info;
    }

    return ret;
}

STATIC int RsDrvQpQueryInfo(struct rs_qp_cb *qpCb, struct rs_rdev_cb *rdevCb,
                                struct ibv_port_attr *attr, struct ibv_qp_attr *qpAttr, union ibv_gid *gidTmp)
{
    int ret;
    /* modify qp state */
    ret = memset_s(qpAttr, sizeof(struct ibv_qp_attr), 0, sizeof(struct ibv_qp_attr));
    CHK_PRT_RETURN(ret, hccp_err("memset_s failed ret[%d], buf len:%u", ret, sizeof(struct ibv_qp_attr)), -ESAFEFUNC);
    qpAttr->qp_state = IBV_QPS_INIT;
    qpAttr->pkey_index = 0;
    qpAttr->port_num = rdevCb->ib_port;
    qpAttr->qp_access_flags = DEFAULT_ACCESS_FLAG;
    ret = RsIbvModifyQp(qpCb->ib_qp, qpAttr, IBV_QP_STATE |
                        IBV_QP_PKEY_INDEX | IBV_QP_PORT | IBV_QP_ACCESS_FLAGS);
    CHK_PRT_RETURN(ret, hccp_err("ibv_modify_qp failed ret[%d]", ret), -EOPENSRC);

    /* prepare qp info for exchange */
    ret = RsDrvGetGidIndex(rdevCb, attr, &qpCb->qp_info_lo.gid_idx);
    CHK_PRT_RETURN(ret, hccp_err("rs_drv_get_gid_index failed ret[%d] qpn[%u] gid_idx[%d]",
        ret, qpCb->ib_qp->qp_num, qpCb->qp_info_lo.gid_idx), ret);

    ret = RsIbvQueryGid(rdevCb->ib_ctx, rdevCb->ib_port, qpCb->qp_info_lo.gid_idx, gidTmp);
    CHK_PRT_RETURN(ret, hccp_err("ibv_query_gid failed ret[%d]", ret), -EOPENSRC);
    return 0;
}

RS_ATTRI_VISI_DEF int RsDrvGetRandomNum(int *randNum)
{
    int randFd;
    int ret = 0;
    int retClose = 0;

    randFd = open("/dev/urandom", O_RDONLY, S_IRUSR);
    CHK_PRT_RETURN(randFd < 0, hccp_err("open random failed ret[%d] rand_fd[%d]", errno, randFd), -EFILEOPER);
    do {
        ret = read(randFd, randNum, sizeof(int));
    } while ((ret < 0) && (errno == EINTR));

    if (ret < 0) {
        hccp_err("get random failed ret[%d]", ret);
        RS_CLOSE_RETRY_FOR_EINTR(retClose, randFd);
        return -EFILEOPER;
    }

    RS_CLOSE_RETRY_FOR_EINTR(retClose, randFd);
    return 0;
}

int RsDrvQpInfoRelated(struct rs_qp_cb *qpCb, struct rs_rdev_cb *rdevCb,
                           struct ibv_port_attr *attr, struct ibv_qp_attr *qpAttr)
{
    int ret;
    union ibv_gid gidTmp;
    int randNum;

    ret = RsDrvQpQueryInfo(qpCb, rdevCb,
                               attr, qpAttr, &gidTmp);
    CHK_PRT_RETURN(ret, hccp_err("query qp info failed, ret %d", ret), ret);

    ret = RsDrvGetRandomNum(&randNum);
    CHK_PRT_RETURN(ret, hccp_err("get random num failed, ret %d", ret), ret);

    qpCb->qp_info_lo.cmd = (unsigned int)RS_CMD_QP_INFO;
    qpCb->qp_info_lo.lid = attr->lid;
    qpCb->qp_info_lo.psn = (unsigned int)randNum & 0xffffff;
    qpCb->qp_info_lo.qpn = (int)qpCb->ib_qp->qp_num;
    if (rdevCb->notify_type != NO_USE && rdevCb->notify_mr != NULL) {
        qpCb->qp_info_lo.notify_mr.addr = rdevCb->notify_va_base;
        qpCb->qp_info_lo.notify_mr.len = rdevCb->notify_size;
        qpCb->qp_info_lo.notify_mr.rkey = rdevCb->notify_mr->rkey;
    } else {
        qpCb->qp_info_lo.notify_mr.addr = 0;
        qpCb->qp_info_lo.notify_mr.len = 0;
        qpCb->qp_info_lo.notify_mr.rkey = 0;
    }

    ret = memcpy_s(qpCb->qp_info_lo.gid.raw, sizeof(qpCb->qp_info_lo.gid.raw), gidTmp.raw, sizeof(gidTmp.raw));
    CHK_PRT_RETURN(ret, hccp_err("memcpy_s raw failed, ret:%d, dst_len:%u, src_len:%d", ret,
        sizeof(qpCb->qp_info_lo.gid.raw), RS_QP_ATTR_GID_LEN), -ESAFEFUNC);
    return 0;
}

STATIC int RsDrvExpQpCreateInit(struct ibv_exp_qp_init_attr *qpInitAttr,
    struct rs_qp_cb *qpCb, struct ibv_port_attr *attr)
{
    int ret;
    ret = memset_s(attr, sizeof(struct ibv_port_attr), 0, sizeof(struct ibv_port_attr));
    CHK_PRT_RETURN(ret, hccp_err("memset_s for attr failed, ret:%d", ret), -ESAFEFUNC);
    ret = memset_s(qpInitAttr, sizeof(struct ibv_exp_qp_init_attr), 0, sizeof(struct ibv_exp_qp_init_attr));
    CHK_PRT_RETURN(ret, hccp_err("memset_s for qp_init_attr failed, ret:%d", ret), -ESAFEFUNC);

    qpInitAttr->attr.qp_type = IBV_QPT_RC;
    qpInitAttr->attr.send_cq = qpCb->ib_send_cq;
    qpInitAttr->attr.recv_cq = qpCb->ib_recv_cq;
    qpInitAttr->attr.cap.max_inline_data = QP_DEFAULT_MAX_CAP_INLINE_DATA;
    qpInitAttr->attr.cap.max_send_wr = qpCb->tx_depth;
    qpInitAttr->attr.cap.max_send_sge = qpCb->send_sge_num;
    qpInitAttr->attr.cap.max_recv_wr = qpCb->rx_depth;
    qpInitAttr->attr.cap.max_recv_sge = qpCb->recv_sge_num;

    return 0;
}

STATIC int RsDrvExpQpCreate(struct rs_qp_cb *qpCb, int qpMode)
{
    int ret;
    struct ibv_qp_attr qpAttr;
    struct ibv_exp_qp_init_attr qpInitAttr;
    struct rs_rdev_cb *rdevCb = NULL;
    struct ibv_port_attr attr;

    hccp_dbg("qp exp create begin..");
    rdevCb = qpCb->rdev_cb;
    qpCb->qp_mode = qpMode;
    ret = RsDrvExpQpCreateInit(&qpInitAttr, qpCb, &attr);
    CHK_PRT_RETURN(ret, hccp_err("rs_drv_qp_create_init failed, ret:%d", ret), ret);

    qpInitAttr.gdr_enable = qpMode;
    qpInitAttr.lite_op_support = RsDrvGetSupportLite(rdevCb, qpCb->qp_mode, qpCb->ai_op_support);
    qpInitAttr.mem_align = qpCb->mem_align;
    qpInitAttr.mem_idx = qpCb->mem_resp.mem_data.mem_idx;
    /* A return value of NULL indicates an OutOfMemoryError(OOM) has occurred */
    qpCb->ib_qp = RsIbvExpCreateQp(qpCb->ib_pd, &qpInitAttr, &qpCb->qp_resp.qp_data);
    CHK_PRT_RETURN(qpCb->ib_qp == NULL, hccp_err("rs_ibv_exp_create_qp failed"), -ENOMEM);

    qpCb->db_index = (qpMode == RA_RS_OP_QP_MODE ||
                       qpMode == RA_RS_GDR_ASYN_QP_MODE) ? qpCb->qp_resp.qp_data.qp_info : 0;
    qpCb->sq_index = (qpMode == RA_RS_GDR_TMPL_QP_MODE) ? qpCb->qp_resp.qp_data.qp_info : 0;
    hccp_info("db index is [%d], sq index is [%d]", qpCb->db_index, qpCb->sq_index);

    /* query qp attr */
    ret = RsIbvQueryQp(qpCb->ib_qp, &qpAttr, IBV_QP_CAP, &qpInitAttr.attr);
    if (ret) {
        hccp_err("query qp attr failed ret %d", ret);
        ret = -EOPENSRC;
        goto exp_init_qp_err;
    }

    ret = RsDrvQpInfoRelated(qpCb, rdevCb, &attr, &qpAttr);
    if (ret) {
        hccp_err("qp info related failed %d", ret);
        goto exp_init_qp_err;
    }

    hccp_info("chip_id %u, rdev_index:%u, qp[%d] create succ.", qpCb->rdev_cb->rs_cb->chip_id,
        qpCb->rdev_cb->rdev_index, qpCb->qp_info_lo.qpn);

    return 0;

exp_init_qp_err:
    (void)RsIbvDestroyQp(qpCb->ib_qp);
    return ret;
}

STATIC int RsDrvNormalQpCreateInit(struct ibv_qp_init_attr *qpInitAttr,
    struct rs_qp_cb *qpCb, struct ibv_port_attr *attr)
{
    int ret;
    ret = memset_s(attr, sizeof(struct ibv_port_attr), 0, sizeof(struct ibv_port_attr));
    CHK_PRT_RETURN(ret, hccp_err("memset_s for attr failed, ret:%d", ret), -ESAFEFUNC);
    ret = memset_s(qpInitAttr, sizeof(struct ibv_qp_init_attr), 0, sizeof(struct ibv_qp_init_attr));
    CHK_PRT_RETURN(ret, hccp_err("memset_s for qp_init_attr failed, ret:%d", ret), -ESAFEFUNC);

    qpInitAttr->qp_type = IBV_QPT_RC;
    qpInitAttr->send_cq = qpCb->ib_send_cq;
    qpInitAttr->recv_cq = qpCb->ib_recv_cq;
    qpInitAttr->cap.max_inline_data = QP_DEFAULT_MAX_CAP_INLINE_DATA;
    qpInitAttr->cap.max_send_wr = qpCb->tx_depth;
    qpInitAttr->cap.max_send_sge = qpCb->send_sge_num;
    qpInitAttr->cap.max_recv_wr = qpCb->rx_depth;
    qpInitAttr->cap.max_recv_sge = qpCb->recv_sge_num;
    return 0;
}

STATIC int RsDrvQpNormal(struct rs_qp_cb *qpCb, int qpMode)
{
    int ret;
    struct ibv_qp_attr qpAttr;
    struct ibv_qp_init_attr qpInitAttr;
    struct rs_rdev_cb *rdevCb = NULL;
    struct ibv_port_attr attr;

    hccp_dbg("qp normal create begin..");
    rdevCb = qpCb->rdev_cb;
    qpCb->qp_mode = qpMode;
    ret = RsDrvNormalQpCreateInit(&qpInitAttr, qpCb, &attr);
    CHK_PRT_RETURN(ret, hccp_err("rs_drv_normal_qp_create_init failed, ret:%d", ret), ret);

    /* A return value of NULL indicates an OutOfMemoryError(OOM) has occurred */
    qpCb->ib_qp = RsIbvCreateQp(qpCb->ib_pd, &qpInitAttr);
    CHK_PRT_RETURN(qpCb->ib_qp == NULL, hccp_err("rs_ibv_create_qp failed, errno=%d", errno), -ENOMEM);

    /* query qp attr */
    ret = RsIbvQueryQp(qpCb->ib_qp, &qpAttr, IBV_QP_CAP, &qpInitAttr);
    if (ret) {
        hccp_err("query qp attr failed ret %d", ret);
        ret = -EOPENSRC;
        goto normal_init_qp_err;
    }

    ret = RsDrvQpInfoRelated(qpCb, rdevCb, &attr, &qpAttr);
    if (ret) {
        hccp_err("qp info related failed %d", ret);
        goto normal_init_qp_err;
    }

    hccp_info("chip_id %u, rdev_index:%u, qp[%d] create succ.", qpCb->rdev_cb->rs_cb->chip_id,
        qpCb->rdev_cb->rdev_index, qpCb->qp_info_lo.qpn);

    return 0;

normal_init_qp_err:
    (void)RsIbvDestroyQp(qpCb->ib_qp);
    return ret;
}

int RsDrvQpCreate(struct rs_qp_cb *qpCb, struct rs_qp_norm *qpNorm)
{
    int ret;
    if (qpNorm->is_exp != 0 && qpNorm->qp_mode != RA_RS_NOR_QP_MODE) {
        ret = RsDrvExpQpCreate(qpCb, qpNorm->qp_mode);
    } else {
        ret = RsDrvQpNormal(qpCb, qpNorm->qp_mode);
    }

    return ret;
}

STATIC int RsDrvExpQpCreateInitWithAttrs(struct ibv_exp_qp_init_attr *qpInitAttr, struct rs_qp_cb *qpCb,
    struct ibv_port_attr *attr, struct rs_qp_norm_with_attrs *qpNorm)
{
    int ret;

    ret = memset_s(attr, sizeof(struct ibv_port_attr), 0, sizeof(struct ibv_port_attr));
    CHK_PRT_RETURN(ret, hccp_err("memset_s for attr failed, ret:%d", ret), -ESAFEFUNC);
    ret = memset_s(qpInitAttr, sizeof(struct ibv_exp_qp_init_attr), 0, sizeof(struct ibv_exp_qp_init_attr));
    CHK_PRT_RETURN(ret, hccp_err("memset_s for qp_init_attr failed, ret:%d", ret), -ESAFEFUNC);

    qpInitAttr->attr.send_cq = qpCb->ib_send_cq;
    qpInitAttr->attr.recv_cq = qpCb->ib_recv_cq;

    ret = memcpy_s(&qpInitAttr->attr.cap, sizeof(struct ibv_qp_cap), &qpNorm->ext_attrs.qp_attr.cap,
        sizeof(struct ibv_qp_cap));
    CHK_PRT_RETURN(ret, hccp_err("memset_s for qp_init_attr failed, ret:%d", ret), -ENOMEM);
    qpInitAttr->attr.qp_type = qpNorm->ext_attrs.qp_attr.qp_type;
    qpInitAttr->attr.sq_sig_all = qpNorm->ext_attrs.qp_attr.sq_sig_all;

    qpInitAttr->udp_sport = qpCb->udp_sport;

    return 0;
}

STATIC int RsDrvExpQpCreateWithAttrs(struct rs_qp_cb *qpCb, struct rs_qp_norm_with_attrs *qpNorm)
{
    int ret;
    struct ibv_port_attr attr;
    struct ibv_qp_attr qpAttr;
    struct rs_rdev_cb *rdevCb = NULL;
    struct ibv_exp_qp_init_attr qpInitAttr;

    hccp_dbg("qp exp create begin..");
    rdevCb = qpCb->rdev_cb;
    ret = RsDrvExpQpCreateInitWithAttrs(&qpInitAttr, qpCb, &attr, qpNorm);
    CHK_PRT_RETURN(ret, hccp_err("rs_drv_qp_create_init failed, ret:%d", ret), ret);

    qpInitAttr.gdr_enable = qpCb->qp_mode;
    qpInitAttr.lite_op_support = RsDrvGetSupportLite(rdevCb, qpCb->qp_mode, qpCb->ai_op_support);
    qpInitAttr.mem_align = qpCb->mem_align;
    qpInitAttr.mem_idx = qpCb->mem_resp.mem_data.mem_idx;
    qpInitAttr.ai_op_support = qpCb->ai_op_support;
    qpInitAttr.grp_id = qpCb->grp_id;
    qpInitAttr.qp_cstm_flag = qpCb->ai_op_support;
    /* A return value of NULL indicates an OutOfMemoryError(OOM) has occurred */
    qpCb->ib_qp = RsIbvExpCreateQp(qpCb->ib_pd, &qpInitAttr, &qpCb->qp_resp.qp_data);
    CHK_PRT_RETURN(qpCb->ib_qp == NULL, hccp_err("rs_ibv_exp_create_qp failed"), -ENOMEM);

    qpCb->db_index = (qpCb->qp_mode == RA_RS_OP_QP_MODE ||
                       qpCb->qp_mode == RA_RS_GDR_ASYN_QP_MODE) ? qpCb->qp_resp.qp_data.qp_info : 0;
    qpCb->sq_index = (qpCb->qp_mode == RA_RS_GDR_TMPL_QP_MODE) ? qpCb->qp_resp.qp_data.qp_info : 0;
    hccp_info("db index is [%d], sq index is [%d]", qpCb->db_index, qpCb->sq_index);

    /* query qp attr */
    ret = RsIbvQueryQp(qpCb->ib_qp, &qpAttr, IBV_QP_CAP, &qpInitAttr.attr);
    if (ret) {
        hccp_err("query qp attr failed ret %d", ret);
        ret = -EOPENSRC;
        goto exp_init_qp_err;
    }

    ret = RsDrvQpInfoRelated(qpCb, rdevCb, &attr, &qpAttr);
    if (ret) {
        hccp_err("qp info related failed %d", ret);
        goto exp_init_qp_err;
    }

    hccp_info("chip_id %u, rdev_index:%u, qp[%d] create succ.", qpCb->rdev_cb->rs_cb->chip_id,
        qpCb->rdev_cb->rdev_index, qpCb->qp_info_lo.qpn);

    return 0;

exp_init_qp_err:
    (void)RsIbvDestroyQp(qpCb->ib_qp);
    return ret;
}

STATIC int RsDrvNormalQpCreateInitWithAttrs(struct ibv_qp_init_attr *qpInitAttr,
    struct rs_qp_cb *qpCb, struct ibv_port_attr *attr, struct rs_qp_norm_with_attrs *qpNorm)
{
    int ret;

    ret = memset_s(attr, sizeof(struct ibv_port_attr), 0, sizeof(struct ibv_port_attr));
    CHK_PRT_RETURN(ret, hccp_err("memset_s for attr failed, ret:%d", ret), -ESAFEFUNC);
    ret = memset_s(qpInitAttr, sizeof(struct ibv_qp_init_attr), 0, sizeof(struct ibv_qp_init_attr));
    CHK_PRT_RETURN(ret, hccp_err("memset_s for qp_init_attr failed, ret:%d", ret), -ESAFEFUNC);

    qpInitAttr->send_cq = qpCb->ib_send_cq;
    qpInitAttr->recv_cq = qpCb->ib_recv_cq;

    ret = memcpy_s(&qpInitAttr->cap, sizeof(struct ibv_qp_cap), &qpNorm->ext_attrs.qp_attr.cap,
        sizeof(struct ibv_qp_cap));
    CHK_PRT_RETURN(ret, hccp_err("memcpy_s for qp_init_attr failed, ret:%d", ret), -ENOMEM);
    qpInitAttr->qp_type = qpNorm->ext_attrs.qp_attr.qp_type;
    qpInitAttr->sq_sig_all = qpNorm->ext_attrs.qp_attr.sq_sig_all;

    return ret;
}

STATIC int RsDrvQpNormalWithAttrs(struct rs_qp_cb *qpCb, struct rs_qp_norm_with_attrs *qpNorm)
{
    int ret;
    struct ibv_port_attr attr;
    struct ibv_qp_attr qpAttr;
    struct ibv_qp_init_attr qpInitAttr;
    struct rs_rdev_cb *rdevCb = NULL;

    hccp_dbg("qp normal create begin..");
    rdevCb = qpCb->rdev_cb;
    ret = RsDrvNormalQpCreateInitWithAttrs(&qpInitAttr, qpCb, &attr, qpNorm);
    CHK_PRT_RETURN(ret, hccp_err("rs_drv_normal_qp_create_init failed, ret:%d", ret), ret);

    /* A return value of NULL indicates an OutOfMemoryError(OOM) has occurred */
    qpCb->ib_qp = RsIbvCreateQp(qpCb->ib_pd, &qpInitAttr);
    CHK_PRT_RETURN(qpCb->ib_qp == NULL, hccp_err("rs_ibv_create_qp failed, errno=%d", errno), -ENOMEM);

    /* query qp attr */
    ret = RsIbvQueryQp(qpCb->ib_qp, &qpAttr, IBV_QP_CAP, &qpInitAttr);
    if (ret) {
        hccp_err("query qp attr failed ret %d", ret);
        ret = -EOPENSRC;
        goto normal_init_qp_err;
    }

    ret = RsDrvQpInfoRelated(qpCb, rdevCb, &attr, &qpAttr);
    if (ret) {
        hccp_err("qp info related failed ret %d", ret);
        goto normal_init_qp_err;
    }

    hccp_info("chip_id %u, rdev_index:%u, qp[%d] create succ.", qpCb->rdev_cb->rs_cb->chip_id,
        qpCb->rdev_cb->rdev_index, qpCb->qp_info_lo.qpn);

    return 0;

normal_init_qp_err:
    (void)RsIbvDestroyQp(qpCb->ib_qp);
    return ret;
}

int RsDrvQpCreateWithAttrs(struct rs_qp_cb *qpCb, struct rs_qp_norm_with_attrs *qpNorm)
{
    int ret;

    // ignore qp_mode when ai_op_support set
    if ((qpNorm->is_exp != 0 && qpNorm->ext_attrs.qp_mode != RA_RS_NOR_QP_MODE) || (qpNorm->ai_op_support != 0)) {
        ret = RsDrvExpQpCreateWithAttrs(qpCb, qpNorm);
    } else {
        ret = RsDrvQpNormalWithAttrs(qpCb, qpNorm);
    }

    return ret;
}

void RsDrvQpDestroy(struct rs_qp_cb *qpCb)
{
    (void)RsIbvDestroyQp(qpCb->ib_qp);
}

int RsQueryEvent(int cqEventId, struct event_summary **event)
{
   *event = calloc(1, sizeof(struct event_summary));
    if ((*event) == NULL) {
        return -ENOMEM;
    }

    (*event)->pid = getpid();
    (*event)->grp_id = 6; // 6: 通信库使用的事件调度group
    (*event)->event_id = cqEventId;
    (*event)->subevent_id = 0;
    (*event)->msg_len = 0;
    (*event)->msg = NULL;
    (*event)->dst_engine = ACPU_DEVICE;
    (*event)->policy = ONLY;
    hccp_info("pid:%d, cqEventId:%d", (*event)->pid, cqEventId);
    unsigned int i;
    for (i = 0; i < EVENT_SUMMARY_RSV; i++) {
        (*event)->rsv[i] = 0;
    }

    return 0;
}

int RsDrvCreateCqEvent(struct rs_cq_context *cqContext, struct cq_attr *attr)
{
    int ret;

    hccp_info("create cq event start cq_create_mode [%d]", cqContext->cq_create_mode);

    if (cqContext->cq_create_mode == RS_NORMAL_CQ_CREATE || cqContext->cq_create_mode == RS_SQ_CQ_CREATE) {
        ret = RsQueryEvent(attr->send_cq_event_id, &(cqContext->send_event));
        CHK_PRT_RETURN(ret, hccp_err("rs_query_event send_event failed! ret:%d", ret), ret);

        cqContext->ib_send_cq = RsIbvCreateCq(cqContext->rdev_cb->ib_ctx, attr->send_cq_depth,
            cqContext->send_event, cqContext->channel, cqContext->eq_num);
        if (cqContext->ib_send_cq == NULL) {
            hccp_err("rs_drv_create_cq_event ibv create send cq failed, send_cq_event_id:%d", attr->send_cq_event_id);
            goto create_cq_even_err;
        }

        ret = ibv_req_notify_cq(cqContext->ib_send_cq, 0);
        if (ret) {
            hccp_err("Couldn't request send CQ notification, ret:%d", ret);
            goto create_cq_even_err;
        }
        *attr->ib_send_cq = cqContext->ib_send_cq;
    }

    if (cqContext->cq_create_mode == RS_NORMAL_CQ_CREATE || cqContext->cq_create_mode == RS_SRQ_CQ_CREATE) {
        ret = RsQueryEvent(attr->recv_cq_event_id, &(cqContext->recv_event));
        if (ret) {
            hccp_err("rs_query_event send_event failed! ret:%d", ret);
            goto create_cq_even_err;
        }

        cqContext->ib_recv_cq = RsIbvCreateCq(cqContext->rdev_cb->ib_ctx, attr->recv_cq_depth,
            cqContext->recv_event, cqContext->channel, cqContext->eq_num);
        if (cqContext->ib_recv_cq == NULL) {
            hccp_err("rs_drv_create_cq_event ibv create recv cq failed, recv_cq_event_id:%d", attr->recv_cq_event_id);
            goto create_cq_even_err;
        }

        ret = ibv_req_notify_cq(cqContext->ib_recv_cq, 0);
        if (ret) {
            hccp_err("Couldn't request recv CQ notification, ret:%d", ret);
            goto create_cq_even_err;
        }
        *attr->ib_recv_cq = cqContext->ib_recv_cq;
    }

    hccp_info("create cq event success");
    return 0;

create_cq_even_err:
    if (cqContext->ib_recv_cq != NULL) {
        (void)RsIbvDestroyCq(cqContext->ib_recv_cq);
    }

    if (cqContext->ib_send_cq != NULL) {
        (void)RsIbvDestroyCq(cqContext->ib_send_cq);
    }

    (void)RsDrvEventDestroy(cqContext->recv_event);

    (void)RsDrvEventDestroy(cqContext->send_event);

    return -EOPENSRC;
}

int RsDrvCreateCqWithChannel(struct rs_cq_context *cqContext, struct cq_attr *attr)
{
    hccp_dbg("create cq with channel start");

    cqContext->ib_send_cq = RsIbvCreateCq(cqContext->rdev_cb->ib_ctx, attr->send_cq_depth,
        NULL, attr->send_channel, 1);
    if (cqContext->ib_send_cq == NULL) {
        hccp_err("rs_drv_create_cq_with_channel ibv create send cq failed.");
        return -EOPENSRC;
    }

    cqContext->ib_recv_cq = RsIbvCreateCq(cqContext->rdev_cb->ib_ctx, attr->recv_cq_depth,
        NULL, attr->recv_channel, 1);
    if (cqContext->ib_recv_cq == NULL) {
        hccp_err("rs_drv_create_cq_with_channel ibv create serecvnd cq failed.");
        goto create_recv_cq_err;
    }

    *attr->ib_send_cq = cqContext->ib_send_cq;
    *attr->ib_recv_cq = cqContext->ib_recv_cq;
    hccp_info("create cq with channel success");
    return 0;

create_recv_cq_err:
    (void)RsIbvDestroyCq(cqContext->ib_send_cq);
    return -EOPENSRC;
}

int RsDrvDestroyCqEvent(struct rs_cq_context *cqContext)
{
    int ret;

    if (cqContext->cq_create_mode == RS_NORMAL_CQ_CREATE || cqContext->cq_create_mode == RS_SRQ_CQ_CREATE) {
        ret = RsIbvDestroyCq(cqContext->ib_recv_cq);
        if (ret) {
            hccp_err("rs_ibv_destroy_cq(recv) failed, ret %d", ret);
        }
        (void)RsDrvEventDestroy(cqContext->recv_event);
    }

    if (cqContext->cq_create_mode == RS_NORMAL_CQ_CREATE || cqContext->cq_create_mode == RS_SQ_CQ_CREATE) {
        ret = RsIbvDestroyCq(cqContext->ib_send_cq);
        if (ret) {
            hccp_err("rs_ibv_destroy_cq(send) failed, ret %d", ret);
        }
        (void)RsDrvEventDestroy(cqContext->send_event);
    }

    return 0;
}

int RsDrvNormalQpCreate(struct rs_qp_cb *qpCb, struct ibv_qp_init_attr *qpInitAttr)
{
    int ret;
    struct ibv_qp_attr qpAttr;
    struct rs_rdev_cb *rdevCb = NULL;
    struct ibv_port_attr attr;

    hccp_dbg("rs_drv_normal_qp_create begin..");
    rdevCb = qpCb->rdev_cb;

    /* A return value of NULL indicates an OutOfMemoryError(OOM) has occurred */
    qpCb->ib_qp = RsIbvCreateQp(qpCb->ib_pd, qpInitAttr);
    CHK_PRT_RETURN(qpCb->ib_qp == NULL, hccp_err("rs_ibv_create_qp failed, errno=%d", errno), -ENOMEM);

    /* query qp attr */
    ret = RsIbvQueryQp(qpCb->ib_qp, &qpAttr, IBV_QP_CAP, qpInitAttr);
    if (ret) {
        hccp_err("query qp attr failed ret %d", ret);
        ret = -EOPENSRC;
        goto normal_init_qp_err;
    }

    ret = RsDrvQpInfoRelated(qpCb, rdevCb, &attr, &qpAttr);
    if (ret) {
        hccp_err("qp info related failed %d", ret);
        goto normal_init_qp_err;
    }

    hccp_info("chip_id %u, rdev_index:%u, qp[%d] create succ.", qpCb->rdev_cb->rs_cb->chip_id,
        qpCb->rdev_cb->rdev_index, qpCb->qp_info_lo.qpn);

    return 0;

normal_init_qp_err:
    (void)RsIbvDestroyQp(qpCb->ib_qp);
    return ret;
}

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

int g_max_cqe_num = 0;
#define INVALID_EVENT -1

struct ibv_wc g_wc_buf[RS_WC_NUM];

struct rs_cqe_err_info g_rs_cqe_err;

int rs_drv_init_cqe_err_info(void)
{
    struct rs_cqe_err_info *err_info = &g_rs_cqe_err;
    int ret;

    ret = pthread_mutex_init(&err_info->mutex, NULL);
    CHK_PRT_RETURN(ret, hccp_err("rscb mutex_init failed ret %d!, normal ret 0", ret), -ESYSFUNC);
    ret = memset_s(&err_info->info, sizeof(struct cqe_err_info), 0, sizeof(struct cqe_err_info));
    if (ret != 0) {
        pthread_mutex_destroy(&err_info->mutex);
    }
    CHK_PRT_RETURN(ret, hccp_err("memset_s fail ret[%d]", ret), -ESAFEFUNC);
    return 0;
}

void rs_drv_deinit_cqe_err_info(void)
{
    struct rs_cqe_err_info *err_info = &g_rs_cqe_err;

    pthread_mutex_destroy(&err_info->mutex);
    return;
}

STATIC void rs_drv_save_cqe_err_info(uint32_t status, struct rs_qp_cb *qp_cb)
{
    struct rs_cqe_err_info *err_info = &g_rs_cqe_err;
    struct cqe_err_info *temp_info = &err_info->info;

    RS_PTHREAD_MUTEX_LOCK(&err_info->mutex);
    if (temp_info->status != 0) {
        RS_PTHREAD_MUTEX_ULOCK(&err_info->mutex);
        hccp_run_info("over status=[0x%x], drop qpn[0x%x] err cqe status=[0x%x]",
            temp_info->status, qp_cb->qp_info_lo.qpn, status);
        return;
    }
    temp_info->status = status;
    temp_info->qpn = (uint32_t)qp_cb->qp_info_lo.qpn;
    rs_get_cur_time(&temp_info->time);
    RS_PTHREAD_MUTEX_ULOCK(&err_info->mutex);

    return;
}

STATIC void rs_drv_save_qp_cqe_err_info(uint32_t status, struct rs_qp_cb *qp_cb)
{
    RS_PTHREAD_MUTEX_LOCK(&qp_cb->cqe_err_info.mutex);
    if (qp_cb->cqe_err_info.info.status != 0) {
        RS_PTHREAD_MUTEX_ULOCK(&qp_cb->cqe_err_info.mutex);
        return;
    }
    qp_cb->cqe_err_info.info.status = status;
    qp_cb->cqe_err_info.info.qpn = (uint32_t)qp_cb->qp_info_lo.qpn;
    rs_get_cur_time(&qp_cb->cqe_err_info.info.time);
    RS_PTHREAD_MUTEX_ULOCK(&qp_cb->cqe_err_info.mutex);

    RS_PTHREAD_MUTEX_LOCK(&qp_cb->rdev_cb->cqe_err_cnt_mutex);
    qp_cb->rdev_cb->cqe_err_cnt++;
    RS_PTHREAD_MUTEX_ULOCK(&qp_cb->rdev_cb->cqe_err_cnt_mutex);

    return;
}

int rs_drv_get_cqe_err_info(struct cqe_err_info *info)
{
    struct rs_cqe_err_info *err_info = &g_rs_cqe_err;
    struct cqe_err_info *temp_info = &err_info->info;
    int ret;

    CHK_PRT_RETURN(info == NULL, hccp_err("info is NULL"), -EINVAL);

    RS_PTHREAD_MUTEX_LOCK(&err_info->mutex);
    if (temp_info->status == 0) {
        RS_PTHREAD_MUTEX_ULOCK(&err_info->mutex);
        return 0;
    }
    hccp_run_info("status=[%u]", temp_info->status);
    ret = memcpy_s(info, sizeof(struct cqe_err_info), &err_info->info, sizeof(struct cqe_err_info));
    if (ret) {
        hccp_err("memcpy_s  failed");
        RS_PTHREAD_MUTEX_ULOCK(&err_info->mutex);
        return -ESAFEFUNC;
    }

    ret = memset_s(&err_info->info, sizeof(struct cqe_err_info), 0, sizeof(struct cqe_err_info));
    RS_PTHREAD_MUTEX_ULOCK(&err_info->mutex);
    CHK_PRT_RETURN(ret, hccp_err("memset_s fail ret[%d], buf len:%u", ret, sizeof(struct cqe_err_info)), -ESAFEFUNC);
    return 0;
}

STATIC void rs_retry_timeout_exception_check(struct rs_rdev_cb *rdev_cb, struct ibv_wc *wc)
{
    int ret = 0;

    /* sensor may not support, handle is 0 */
    if (rdev_cb->sensor_handle == 0) {
        return;
    }

    if (wc->status != IBV_WC_RETRY_EXC_ERR) {
        return;
    }

    /* The notification alarm framework does not filter alarms. In this example, only one notification
        alarm is reported by a single process, which does not need to be accurate. Therefore, no lock is used. */
    if (rdev_cb->sensor_update_cnt == 0) {
        ret = dl_hal_sensor_node_update_state(rdev_cb->logic_devid, rdev_cb->sensor_handle,
            RDMA_CQE_ERR_RETRY_TIMEOUT_EVENT_TYPE, GENERAL_EVENT_TYPE_ONE_TIME);
        if (ret == 0) {
            rdev_cb->sensor_update_cnt++;
        }
    }

    hccp_warn("update sensor state logic_devid(%u), qpn(%u), sensor_update_cnt(%d),ret(%d)\n",
        rdev_cb->logic_devid, wc->qp_num, rdev_cb->sensor_update_cnt, ret);
}

STATIC void rs_cqe_callback_process(struct rs_qp_cb *qp_cb, struct ibv_wc *wc, struct ibv_cq *ev_cq)
{
    if (wc->status != IBV_WC_SUCCESS && wc->status != IBV_WC_WR_FLUSH_ERR) {
        hccp_err("Failed status [%s] [%u], wr[%llu]",
            rs_ibv_wc_status_str(wc->status), wc->status, wc->wr_id);
        rs_drv_save_cqe_err_info(wc->status, qp_cb);
        rs_drv_save_qp_cqe_err_info(wc->status, qp_cb);
        rs_retry_timeout_exception_check(qp_cb->rdev_cb, wc);
    }

    return;
}

void rs_drv_poll_srq_cq_handle(struct rs_qp_cb *qp_cb)
{
    struct ibv_cq *ev_cq = NULL;
    void *ev_ctx = NULL;

    if (rs_ibv_get_cq_event(qp_cb->srq_context->channel, &ev_cq, &ev_ctx)) {
        hccp_err("Failed to get cq_event");
        return;
    }

    if (ev_cq != qp_cb->ib_recv_cq || ev_ctx == NULL) {
        hccp_err("CQ event for unknown CQ");
        return;
    }

    ++qp_cb->srq_context->num_recv_cq_events;

    struct event_summary *ev_ctx_tmp = (struct event_summary *)ev_ctx;
    if ((int)ev_ctx_tmp->event_id != INVALID_EVENT) {
        hccp_info("SubmitEvent: event id:%d, pid:%d, grp id:%u, dev id:%u",
            ev_ctx_tmp->event_id, ev_ctx_tmp->pid, ev_ctx_tmp->grp_id, qp_cb->rdev_cb->rs_cb->chip_id);
        int ret = dl_hal_esched_submit_event(qp_cb->rdev_cb->rs_cb->chip_id, ev_ctx);
        if (ret) {
            hccp_warn("halEschedSubmitEvent unsuccessful, ret:%d", ret);
        }
    }

    return;
}

void rs_drv_poll_cq_handle(struct rs_qp_cb *qp_cb)
{
    struct ibv_cq *ev_cq = NULL;
    void *ev_ctx = NULL;
    int ne, i;

    if (rs_ibv_get_cq_event(qp_cb->channel, &ev_cq, &ev_ctx)) {
        hccp_err("Failed to get cq_event");
        return;
    }

    if (ev_cq != qp_cb->ib_send_cq && ev_cq != qp_cb->ib_recv_cq) {
        hccp_err("CQ event for unknown CQ");
        return;
    }

    if (ev_cq == qp_cb->ib_recv_cq) {
        ++qp_cb->num_recv_cq_events;
    } else {
        ++qp_cb->num_send_cq_events;
    }

    if (ev_ctx != NULL) {
        struct event_summary *ev_ctx_tmp = (struct event_summary *)ev_ctx;
        if ((int)(ev_ctx_tmp->event_id) != INVALID_EVENT) {
            hccp_info("SubmitEvent: event id:%d, pid:%d, grp id:%u, dev id:%u",
                ev_ctx_tmp->event_id, ev_ctx_tmp->pid, ev_ctx_tmp->grp_id, qp_cb->rdev_cb->rs_cb->chip_id);
            int ret = dl_hal_esched_submit_event(qp_cb->rdev_cb->rs_cb->chip_id, ev_ctx);
            if (ret) {
                hccp_warn("halEschedSubmitEvent unsuccessful, ret:%d", ret);
            }
        }
        return;
    }

    ne = ibv_poll_cq(ev_cq, RS_WC_NUM, g_wc_buf);
    if (ne > RS_WC_NUM || ne < 0) {
        hccp_err("poll CQ failed %d", ne);
        return;
    }
    if (g_max_cqe_num < ne) {
        g_max_cqe_num = ne;
        hccp_run_info("rs_drv_poll_cq_handle: max_cqe_num=[%d]", g_max_cqe_num);
    }

    if (ibv_req_notify_cq(ev_cq, 0)) {
        hccp_err("Couldn't request CQ notification");
        return;
    }
    qp_cb->rdev_cb->poll_cqe_num += ne;
    for (i = 0; i < ne; ++i) {
        rs_cqe_callback_process(qp_cb, &(g_wc_buf[i]), ev_cq);
    }
    return;
}

int rs_drv_compare_ip_gid(int family, union hccp_ip_addr local_ip, union ibv_gid *gid)
{
    unsigned int gid_v4[RS_GID_SEQ_NUM];

    if (family == AF_INET6) {
        if (memcmp(gid, &(local_ip.addr6), sizeof(union ibv_gid)) == 0) {
            return 0;
        }
    } else {
        gid_v4[RS_GID_SEQ_ZERO] = 0;
        gid_v4[RS_GID_SEQ_ONE] = 0;
        /* The gid format generated by ipv4 is filled with 0xFFFF in [33, 48] */
        gid_v4[RS_GID_SEQ_TWO]   = htonl(0x0000FFFF);
        gid_v4[RS_GID_SEQ_THREE] = local_ip.addr.s_addr;
        if (memcmp(gid, &(gid_v4), sizeof(union ibv_gid)) == 0) {
            return 0;
        }
    }

    return -ENODEV;
}

int rs_drv_get_gid_index(struct rs_rdev_cb *rdev_cb, struct ibv_port_attr *attr, int *idx)
{
    static const char *portStates[] = {"Nop", "Down", "Init", "Armed", "", "Active Defer"};
    enum ibv_gid_type_sysfs type;
    union ibv_gid gid_tmp;
    int gid_idx = -1;
    int ret;
    int i;

    ret = rs_ibv_query_port(rdev_cb->ib_ctx, rdev_cb->ib_port, attr);
    CHK_PRT_RETURN(ret, hccp_err("ibv_query_port fail ret[%d]", ret), -EOPENSRC);

    // link maybe suffer from intermittent disconnection, should continue process
    if (attr->state != IBV_PORT_ACTIVE) {
        hccp_warn("port number %u state is %s", rdev_cb->ib_port, portStates[attr->state]);
    }

    for (i = 0; i < attr->gid_tbl_len; i++) {
        ret = rs_ibv_query_gid_type(rdev_cb->ib_ctx, rdev_cb->ib_port, (unsigned int)i, &type);
        CHK_PRT_RETURN(ret, hccp_err("query gid type failed i %d, ret %d", i, ret), -EOPENSRC);
        if (type != IBV_GID_TYPE_SYSFS_ROCE_V2) {
            continue;
        }

        ret = rs_ibv_query_gid(rdev_cb->ib_ctx, rdev_cb->ib_port, i, &gid_tmp);
        CHK_PRT_RETURN(ret, hccp_err("query gid failed i %d, ret %d", i, ret), -EOPENSRC);

        ret = rs_drv_compare_ip_gid((int)rdev_cb->local_ip.family, rdev_cb->local_ip.bin_addr, &gid_tmp);
        if (ret == 0) {
            gid_idx = i;
            break;
        }
    }

    if (gid_idx == -1) {
        hccp_err("get idx failed, attr->gid_tbl_len:%d", attr->gid_tbl_len);
        return -ENODEV;
    }

    hccp_dbg("GID index is %d", gid_idx);
    *idx = gid_idx;
    return 0;
}

STATIC int rs_drv_get_support_lite(struct rs_rdev_cb *rdev_cb, int qp_mode, unsigned int ai_op_support)
{
    // bypass rdma lite when ai_op_support was set
    if (ai_op_support == 1) {
        return 0;
    }

    if (qp_mode == RA_RS_OP_QP_MODE ||
        qp_mode == RA_RS_OP_QP_MODE_EXT) {
        return rdev_cb->support_lite;
    }

    return 0;
}

int rs_drv_create_cq_with_attrs(struct rs_qp_cb *qp_cb, int is_ext, struct cq_ext_attr *cq_attr)
{
    int send_eq_num = cq_attr->send_cq_comp_vector;
    int recv_eq_num = cq_attr->recv_cq_comp_vector;
    struct rdma_lite_device_cq_init_attr attr = {
        .cq_type = qp_cb->qp_mode,
        .part_id = 0,
        .lite_op_supported = 0,
        .mem_align = qp_cb->mem_align,
        .mem_idx = qp_cb->mem_resp.mem_data.mem_idx,
        .ai_op_support = qp_cb->ai_op_support,
        .grp_id = qp_cb->grp_id,
        .cq_cstm_flag = qp_cb->cq_cstm_flag,
    };
    struct ibv_comp_channel *channel = qp_cb->channel;

    attr.lite_op_supported = rs_drv_get_support_lite(qp_cb->rdev_cb, qp_cb->qp_mode, qp_cb->ai_op_support);
    // caller poll cq
    if (attr.lite_op_supported != 0 || attr.cq_cstm_flag != 0) {
        channel = NULL;
        send_eq_num = 0;
        recv_eq_num = 0;
    }

    hccp_dbg("create cq start");
    if (is_ext == 1) {
        qp_cb->ib_send_cq = rs_ibv_exp_create_cq(qp_cb->rdev_cb->ib_ctx, cq_attr->send_cq_depth,
            NULL, channel, send_eq_num, &attr, &qp_cb->qp_resp.send_cq_data);
        hccp_info("rs_ibv_exp_create_cq");
    } else {
        qp_cb->ib_send_cq = rs_ibv_create_cq(qp_cb->rdev_cb->ib_ctx, cq_attr->send_cq_depth,
            NULL, channel, send_eq_num);
    }

    CHK_PRT_RETURN(qp_cb->ib_send_cq == NULL, hccp_err("ibv create send cq fail "), -EOPENSRC);

    if (is_ext == 1) {
        qp_cb->ib_recv_cq = rs_ibv_exp_create_cq(qp_cb->rdev_cb->ib_ctx, cq_attr->recv_cq_depth,
            NULL, channel, recv_eq_num, &attr, &qp_cb->qp_resp.recv_cq_data);
        hccp_info("rs_ibv_exp_create_cq");
    } else {
        qp_cb->ib_recv_cq = rs_ibv_create_cq(qp_cb->rdev_cb->ib_ctx, cq_attr->recv_cq_depth,
            NULL, channel, recv_eq_num);
    }

    if (qp_cb->ib_recv_cq == NULL) {
        hccp_err("ibv create recv cq fail ");
        (void)rs_ibv_destroy_cq(qp_cb->ib_send_cq);
        return -EOPENSRC;
    }

    hccp_info("create cq success");
    return 0;
}

#define RS_DRV_CQ_DEPTH         16384
#define RS_DRV_CQ_128_DEPTH     128
#define RS_DRV_CQ_32K_DEPTH     32768

int rs_drv_create_cq(struct rs_qp_cb *qp_cb, int is_ext)
{
    struct cq_ext_attr cq_attr = {0};

    cq_attr.send_cq_depth = qp_cb->send_cq_depth;
    cq_attr.send_cq_comp_vector = qp_cb->eq_num;
    cq_attr.recv_cq_depth = qp_cb->recv_cq_depth;
    cq_attr.recv_cq_comp_vector = qp_cb->eq_num;

    return rs_drv_create_cq_with_attrs(qp_cb, is_ext, &cq_attr);
}

void rs_drv_event_destroy(struct event_summary *event)
{
    if (event != NULL) {
        free(event);
        event = NULL;
    }
}

void rs_drv_destroy_cq(struct rs_qp_cb *qp_cb)
{
    (void)rs_ibv_destroy_cq(qp_cb->ib_recv_cq);
    (void)rs_ibv_destroy_cq(qp_cb->ib_send_cq);
    (void)rs_drv_event_destroy(qp_cb->recv_event);
    (void)rs_drv_event_destroy(qp_cb->send_event);
}

int rs_drv_qp_state_modifyto_reset(struct rs_qp_cb *qp_cb)
{
    struct ibv_qp_init_attr qp_init_attr = {0};
    struct ibv_qp_attr attr = {0};
    int ret;

    attr.qp_state = IBV_QPS_RESET;
    ret = rs_ibv_modify_qp(qp_cb->ib_qp, &attr, IBV_QP_STATE);
    CHK_PRT_RETURN(ret, hccp_err("[modify]qpn[%d] modify to reset fail, ret %d", qp_cb->qp_info_lo.qpn, ret), ret);

    (void)memset_s(&attr, sizeof(struct ibv_qp_attr), 0, sizeof(struct ibv_qp_attr));

    ret = rs_ibv_query_qp(qp_cb->ib_qp, &attr, IBV_QP_STATE, &qp_init_attr);
    if ((ret != 0) || attr.qp_state != IBV_QPS_RESET) {
        hccp_err("query qpn[%d] attr failed, ret %d or qp state %d is not RESET",
            qp_cb->qp_info_lo.qpn, ret, attr.qp_state);
        return ret;
    }

    hccp_info("qpn[%d] modify to reset success", qp_cb->qp_info_lo.qpn);
    return 0;
}

int rs_drv_qp_state_modifyto_init(struct rs_qp_cb *qp_cb, struct ibv_qp_attr *attr)
{
    int ret;

    attr->qp_state = IBV_QPS_INIT;
    attr->pkey_index = 0;
    attr->port_num = qp_cb->rdev_cb->ib_port;
    attr->qp_access_flags = DEFAULT_ACCESS_FLAG;
    ret = rs_ibv_modify_qp(qp_cb->ib_qp, attr, IBV_QP_STATE |
                        IBV_QP_PKEY_INDEX | IBV_QP_PORT | IBV_QP_ACCESS_FLAGS);
    CHK_PRT_RETURN(ret, hccp_err("[modify]qpn[%d] modify to init fail, ret %d", qp_cb->qp_info_lo.qpn, ret), ret);

    hccp_info("qpn[%d] modify to init success", qp_cb->qp_info_lo.qpn);
    return 0;
}

enum ibv_mtu rs_drv_set_mtu(struct rs_qp_cb *qp_cb)
{
    int ret;
    struct ibv_port_attr port_attr = {0};
    enum ibv_mtu curr_mtu;

    ret = rs_ibv_query_port(qp_cb->rdev_cb->ib_ctx, qp_cb->rdev_cb->ib_port, &port_attr);
    CHK_PRT_RETURN(ret, hccp_err("Error when trying to query port, ret[%d]", ret), -EOPENSRC);
#ifndef CA_CONFIG_LLT
    curr_mtu = port_attr.active_mtu;
#else
    curr_mtu = IBV_MTU_1024;
#endif

    return curr_mtu;
}

int rs_drv_qp_state_modifyto_rtr(struct rs_qp_cb *qp_cb, struct ibv_qp_attr *attr)
{
    struct ibv_port_attr port_attr = { 0 };
    int ret;

    attr->qp_state          = IBV_QPS_RTR;
    attr->dest_qp_num       = (uint32_t)qp_cb->qp_info_rem.qpn;
    attr->rq_psn            = (uint32_t)qp_cb->qp_info_rem.psn;
    attr->min_rnr_timer     = RS_QP_ATTR_MIN_RNR_TIMER;
    (attr->ah_attr).is_global   = 0;
    (attr->ah_attr).dlid        = qp_cb->qp_info_rem.lid;
    (attr->ah_attr).sl      = qp_cb->qos_attr.sl;
    (attr->ah_attr).src_path_bits   = 0;
    (attr->ah_attr).port_num    = qp_cb->rdev_cb->ib_port;

    attr->path_mtu = rs_drv_set_mtu(qp_cb);
    if (qp_cb->rdev_cb->rs_cb->hccp_mode == NETWORK_PEER_ONLINE) {
        attr->max_dest_rd_atomic = RS_MAX_RD_ATOMIC_NUM_PEER_ONLINE;
    } else {
        attr->max_dest_rd_atomic = RS_MAX_RD_ATOMIC_NUM;
        CHK_PRT_RETURN(attr->path_mtu < IBV_MTU_1024, hccp_err("qpn[%d] failed to set mtu, mtu[%d] < [%d]",
            qp_cb->qp_info_lo.qpn, attr->path_mtu, IBV_MTU_1024), -EPERM);
    }

    // get gid_idx dynamically
    ret = rs_drv_get_gid_index(qp_cb->rdev_cb, &port_attr, &qp_cb->qp_info_lo.gid_idx);
    CHK_PRT_RETURN(ret, hccp_err("rs_drv_get_gid_index fail ret[%d], qpn[%d] gid_idx[%d]",
        ret, qp_cb->qp_info_lo.qpn, qp_cb->qp_info_lo.gid_idx), ret);

    if (qp_cb->qp_info_rem.gid.global.interface_id) {
        attr->ah_attr.is_global = 1;
        attr->ah_attr.grh.hop_limit = 1;
        attr->ah_attr.grh.dgid = qp_cb->qp_info_rem.gid;
        attr->ah_attr.grh.sgid_index = qp_cb->qp_info_lo.gid_idx;
    }

    (attr->ah_attr).grh.traffic_class = qp_cb->qos_attr.tc;
    ret = rs_ibv_modify_qp(qp_cb->ib_qp, attr,
                           IBV_QP_STATE | IBV_QP_AV |
                           IBV_QP_PATH_MTU | IBV_QP_DEST_QPN |
                           IBV_QP_RQ_PSN | IBV_QP_MAX_DEST_RD_ATOMIC |
                           IBV_QP_MIN_RNR_TIMER);
    CHK_PRT_RETURN(ret, hccp_err("qpn[%d] failed to modify QP to RTR, ibv_modify_qp fail ret[%d], errno[%d]",
        qp_cb->qp_info_lo.qpn, ret, errno), -EOPENSRC);
    hccp_info("qp qos attr: qpn[%d] tc[%u] sl[%u]", qp_cb->qp_info_lo.qpn, qp_cb->qos_attr.tc, qp_cb->qos_attr.sl);
    return 0;
}

int rs_drv_qp_state_modifyto_rts(struct rs_qp_cb *qp_cb, struct ibv_qp_attr *attr)
{
    int ret;
    attr->qp_state      = IBV_QPS_RTS;
    attr->timeout       = (uint8_t)qp_cb->timeout;
    attr->retry_cnt     = (uint8_t)qp_cb->retry_cnt;
    attr->rnr_retry     = RS_QP_ATTR_RNR_RETRY;
    attr->sq_psn        = (uint32_t)qp_cb->qp_info_lo.psn;
    if (qp_cb->rdev_cb->rs_cb->hccp_mode == NETWORK_PEER_ONLINE) {
        attr->max_rd_atomic = RS_MAX_RD_ATOMIC_NUM_PEER_ONLINE;
    } else {
        attr->max_rd_atomic = RS_MAX_RD_ATOMIC_NUM;
    }
    ret = rs_ibv_modify_qp(qp_cb->ib_qp, attr,
                           IBV_QP_STATE | IBV_QP_TIMEOUT |
                           IBV_QP_RETRY_CNT | IBV_QP_RNR_RETRY |
                           IBV_QP_SQ_PSN | IBV_QP_MAX_QP_RD_ATOMIC);
    CHK_PRT_RETURN(ret, hccp_err("qpn[%d] failed to modify QP to RTS, ibv_modify_qp fail ret[%d]",
        qp_cb->qp_info_lo.qpn, ret), -EOPENSRC);
    hccp_info("qp rdma attr: qpn[%d] timeout[%u] retrycnt[%u]", qp_cb->qp_info_lo.qpn, qp_cb->timeout,
        qp_cb->retry_cnt);
    return 0;
}

struct ibv_mr* rs_drv_mr_reg(struct ibv_pd *pd, char *addr, size_t length, int access)
{
    return rs_ibv_reg_mr(pd, addr, length, access);
}

struct ibv_mr* rs_drv_exp_mr_reg(struct ibv_pd *pd, char *addr, size_t length,
    int access, struct roce_process_sign roce_sign)
{
    return rs_ibv_exp_reg_mr(pd, addr, length, access, roce_sign);
}

int rs_drv_mr_dereg(struct ibv_mr *ib_mr)
{
    return rs_ibv_dereg_mr(ib_mr);
}

#ifdef CUSTOM_INTERFACE
STATIC int rs_open_backup_ib_ctx(struct rs_rdev_cb *rdev_cb)
{
    struct ibv_context *ib_ctx_tmp = NULL;
    struct rdev rdev_info = { 0 };
    int gid_index = -1;
    int ret;
    int i;

    (void)memcpy_s(&rdev_info, sizeof(struct rdev), &rdev_cb->backup_info.rdev_info, sizeof(struct rdev));
    for (i = 0; (i < rdev_cb->dev_num) && (rdev_cb->dev_list[i] != NULL); ++i) {
        ib_ctx_tmp = rs_ibv_open_device(rdev_cb->dev_list[i]);
        CHK_PRT_RETURN(ib_ctx_tmp == NULL, hccp_err("ibv_open_device with backup failed, errno:%d", errno), -ENODEV);
        ret = rs_query_gid(rdev_info, ib_ctx_tmp, rdev_cb->ib_port, &gid_index);
        if (ret == 0) {
            rdev_cb->backup_info.ib_ctx = ib_ctx_tmp;
            return 0;
        } else if (ret == -EEXIST) {
            rs_ibv_close_device(ib_ctx_tmp);
        } else {
            rs_ibv_close_device(ib_ctx_tmp);
            hccp_err("rs_query_gid failed, ret:%d", ret);
            return ret;
        }
    }

    CHK_PRT_RETURN(i == rdev_cb->dev_num, hccp_err("can not find ib_ctx for phy_id[%u] local_ip[0x%x] in dev_list!",
        rdev_info.phy_id, rdev_info.local_ip.addr.s_addr), -EEXIST);
    return 0;
}

void rs_close_backup_ib_ctx(struct rs_rdev_cb *rdev_cb)
{
    // no need to close backup device
    if (!rdev_cb->backup_info.backup_flag || rdev_cb->backup_info.ib_ctx == NULL) {
        return;
    }

    rs_ibv_close_device(rdev_cb->backup_info.ib_ctx);
}
#endif

STATIC int rs_drv_query_notify(struct rs_rdev_cb *rdev_cb)
{
    int ret = 0;

    if (rdev_cb->notify_type == NO_USE) {
        return 0;
    }

#ifdef CUSTOM_INTERFACE
    if (rdev_cb->backup_info.backup_flag) {
        // open backup device to get ib_ctx to get backup notify va and size
        ret = rs_open_backup_ib_ctx(rdev_cb);
        CHK_PRT_RETURN(ret, hccp_err("rs_open_backup_ib_ctx fail, ret:%d", ret), ret);

        ret = rs_ibv_exp_query_notify(rdev_cb->backup_info.ib_ctx, &rdev_cb->notify_va_base, &rdev_cb->notify_size);
        if (ret != 0) {
            rs_close_backup_ib_ctx(rdev_cb);
            hccp_err("rs_ibv_exp_query_notify with backup ctx failed, ret:%d", ret);
            return ret;
        }
    } else {
        ret = rs_ibv_exp_query_notify(rdev_cb->ib_ctx, &rdev_cb->notify_va_base, &rdev_cb->notify_size);
        CHK_PRT_RETURN(ret, hccp_err("rs_ibv_exp_query_notify failed, ret:%d", ret), ret);
    }
#endif
    hccp_info("chip_id:%u, rs_drv_query_notify ok, notify va:0x%llx, size:%llu", rdev_cb->rs_cb->chip_id,
        rdev_cb->notify_va_base, rdev_cb->notify_size);
    return ret;
}

int rs_drv_query_notify_and_alloc_pd(struct rs_rdev_cb *rdev_cb)
{
    int ret = 0;

    ret = rs_drv_query_notify(rdev_cb);
    CHK_PRT_RETURN(ret, hccp_err("rs_drv_query_notify failed, ret %d", ret), ret);

    rdev_cb->ib_pd = rs_ibv_alloc_pd(rdev_cb->ib_ctx);
    if (rdev_cb->ib_pd == NULL) {
#ifdef CUSTOM_INTERFACE
        rs_close_backup_ib_ctx(rdev_cb);
#endif
        hccp_err("rs_ibv_alloc_pd fail, errno:%d", errno);
        return -ENOMEM;
    }

    return 0;
}

int rs_drv_reg_notify_mr(struct rs_rdev_cb *rdev_cb)
{
#ifdef CUSTOM_INTERFACE
    struct roce_process_sign roce_sign = {0};
#endif
    int access = DEFAULT_ACCESS_FLAG;

    rdev_cb->notify_access = access;
    switch (rdev_cb->notify_type) {
        case NO_USE: return 0;
        case NOTIFY: {
            rdev_cb->notify_mr = rs_ibv_reg_mr(rdev_cb->ib_pd, (void *)(uintptr_t)rdev_cb->notify_va_base,
                rdev_cb->notify_size, access);
            break;
        }
        case EVENTID: {
#ifdef CUSTOM_INTERFACE
            rdev_cb->notify_mr = rs_ibv_exp_reg_mr(rdev_cb->ib_pd, (void *)(uintptr_t)rdev_cb->notify_va_base,
                rdev_cb->notify_size, access, roce_sign);
            break;
#else
            return 0;
#endif
        }
        default: {
            hccp_err("rs_drv_reg_notify_mr fail! notify_type = %u", rdev_cb->notify_type);
            return -EINVAL;
        }
    }

    CHK_PRT_RETURN(rdev_cb->notify_mr == NULL, hccp_err("ibv_reg_mr addr[0x%llx] len[%llu] errno[%d]fail",
        rdev_cb->notify_va_base, rdev_cb->notify_size, errno), -EACCES);

    hccp_info("ibv_reg_mr ok");
    return 0;
}

STATIC void rs_build_recv_wr(struct recv_wrlist_data *wr, struct ibv_sge *list, struct ibv_recv_wr *ib_wr)
{
    list->addr = (uintptr_t)wr->mem_list.addr;
    list->length = wr->mem_list.len;
    list->lkey = wr->mem_list.lkey;

    ib_wr->sg_list = list;
    ib_wr->wr_id = wr->wr_id;
    ib_wr->num_sge = 1; /* only support one sge */
    return;
}

int rs_drv_post_recv(struct rs_qp_cb *qp_cb, struct recv_wrlist_data *wr, unsigned int recv_num,
    unsigned int *complete_num)
{
    int ret = 0;
    unsigned int i, index;
    struct ibv_recv_wr *bad_wr = NULL;

    CHK_PRT_RETURN(recv_num == 0, hccp_err("recv_num[%u] is invalid!", recv_num), -EINVAL);

    struct ibv_recv_wr *recv_wr = (struct ibv_recv_wr *)calloc(recv_num, sizeof(struct ibv_recv_wr));
    CHK_PRT_RETURN(recv_wr == NULL, hccp_err("calloc recv_wr failed!"), -ENOSPC);

    struct ibv_sge *list = (struct ibv_sge *)calloc(recv_num, sizeof(struct ibv_sge));
    if (list == NULL) {
        hccp_err("calloc list failed!");
        ret = -ENOSPC;
        goto alloc_sge_fail;
    }

    for (i = 0; i < recv_num; i++) {
        rs_build_recv_wr(&wr[i], &list[i], &recv_wr[i]);
        index = i + 1; // for fix pclint warning
        recv_wr[i].next = (i < recv_num - 1) ? &(recv_wr[index]) : NULL;
    }

    ret = rs_ibv_post_recv(qp_cb->ib_qp, recv_wr, &bad_wr);
    if (ret == 0) {
        *complete_num = recv_num;
    } else if (ret == -ENOMEM) {
        *complete_num = (unsigned int)((void *)bad_wr - (void *)recv_wr) / sizeof(struct ibv_recv_wr);
        hccp_dbg("post recv wqe overflow, complete_num[%d]", *complete_num);
    } else {
        hccp_err("ibv_post_recv failed, ret[%d]", ret);
        *complete_num = 0;
    }
    qp_cb->recv_wr_num = qp_cb->recv_wr_num + (*complete_num);

    free(list);
    list = NULL;

alloc_sge_fail:
    free(recv_wr);
    recv_wr = NULL;
    return (ret == -ENOMEM) ? 0 : ret;
}

int rs_drv_send_exp(struct rs_qp_cb *qp_cb, struct rs_mr_cb *mr_cb,
                    struct rs_mr_cb *rem_mr_cb, struct send_wr *wr, struct send_wr_rsp *wr_rsp)
{
    int i;
    int ret;
    struct ibv_sge list[RS_SGLIST_MAX];
    struct ibv_send_wr ib_wr = {
        .sg_list    = list,
        .opcode     = wr->op,
        .send_flags = wr->send_flag,
    };
    struct ibv_send_wr *bad_wr = NULL;
    struct wr_exp_rsp exp_rsp = {0};

    for (i = 0; i < wr->buf_num && i < RS_SGLIST_MAX; i++) {
        list[i].addr = (uintptr_t)wr->buf_list[i].addr;
        list[i].length = wr->buf_list[i].len;
        list[i].lkey = mr_cb->ib_mr->lkey;
    }

    ib_wr.num_sge = i;
    ib_wr.wr_id = mr_cb->wr_id;
    // send op has no rem_mr, no need to assign
    if (wr->op != RA_WR_SEND && wr->op != RA_WR_SEND_WITH_IMM) {
        ib_wr.wr.rdma.rkey = rem_mr_cb->mr_info.rkey;
        ib_wr.wr.rdma.remote_addr = wr->dst_addr;
    }

    ret = rs_ibv_exp_post_send(qp_cb->ib_qp, &ib_wr, &bad_wr, &exp_rsp);
    if (ret) {
        hccp_err("rs_ibv_exp_post_send failed ret %d", ret);
    }

    if (qp_cb->qp_mode == RA_RS_GDR_TMPL_QP_MODE) {
        wr_rsp->wqe_tmp.sq_index = (unsigned int)qp_cb->sq_index;
        wr_rsp->wqe_tmp.wqe_index = exp_rsp.wqe_index;
    } else if (qp_cb->qp_mode == RA_RS_OP_QP_MODE ||
               qp_cb->qp_mode == RA_RS_OP_QP_MODE_EXT ||
               qp_cb->qp_mode == RA_RS_GDR_ASYN_QP_MODE) {
        wr_rsp->db.db_index = (unsigned int)qp_cb->db_index;
        wr_rsp->db.db_info = exp_rsp.db_info;
    }

    return ret;
}

STATIC int rs_drv_qp_query_info(struct rs_qp_cb *qp_cb, struct rs_rdev_cb *rdev_cb,
                                struct ibv_port_attr *attr, struct ibv_qp_attr *qp_attr, union ibv_gid *gid_tmp)
{
    int ret;
    /* modify qp state */
    ret = memset_s(qp_attr, sizeof(struct ibv_qp_attr), 0, sizeof(struct ibv_qp_attr));
    CHK_PRT_RETURN(ret, hccp_err("memset_s fail ret[%d], buf len:%u", ret, sizeof(struct ibv_qp_attr)), -ESAFEFUNC);
    qp_attr->qp_state = IBV_QPS_INIT;
    qp_attr->pkey_index = 0;
    qp_attr->port_num = rdev_cb->ib_port;
    qp_attr->qp_access_flags = DEFAULT_ACCESS_FLAG;
    ret = rs_ibv_modify_qp(qp_cb->ib_qp, qp_attr, IBV_QP_STATE |
                        IBV_QP_PKEY_INDEX | IBV_QP_PORT | IBV_QP_ACCESS_FLAGS);
    CHK_PRT_RETURN(ret, hccp_err("ibv_modify_qp fail ret[%d]", ret), -EOPENSRC);

    /* prepare qp info for exchange */
    ret = rs_drv_get_gid_index(rdev_cb, attr, &qp_cb->qp_info_lo.gid_idx);
    CHK_PRT_RETURN(ret, hccp_err("rs_drv_get_gid_index fail ret[%d] qpn[%u] gid_idx[%d]",
        ret, qp_cb->ib_qp->qp_num, qp_cb->qp_info_lo.gid_idx), ret);

    ret = rs_ibv_query_gid(rdev_cb->ib_ctx, rdev_cb->ib_port, qp_cb->qp_info_lo.gid_idx, gid_tmp);
    CHK_PRT_RETURN(ret, hccp_err("ibv_query_gid fail ret[%d]", ret), -EOPENSRC);
    return 0;
}

RS_ATTRI_VISI_DEF int rs_drv_get_random_num(int *rand_num)
{
    int rand_fd;
    int ret = 0;
    int ret_close = 0;

    rand_fd = open("/dev/urandom", O_RDONLY, S_IRUSR);
    CHK_PRT_RETURN(rand_fd < 0, hccp_err("open random fail ret[%d] rand_fd[%d]", errno, rand_fd), -EFILEOPER);
    do {
        ret = read(rand_fd, rand_num, sizeof(int));
    } while ((ret < 0) && (errno == EINTR));

    if (ret < 0) {
        hccp_err("get random fail ret[%d]", ret);
        RS_CLOSE_RETRY_FOR_EINTR(ret_close, rand_fd);
        return -EFILEOPER;
    }

    RS_CLOSE_RETRY_FOR_EINTR(ret_close, rand_fd);
    return 0;
}

int rs_drv_qp_info_related(struct rs_qp_cb *qp_cb, struct rs_rdev_cb *rdev_cb,
                           struct ibv_port_attr *attr, struct ibv_qp_attr *qp_attr)
{
    int ret;
    union ibv_gid gid_tmp;
    int rand_num;

    ret = rs_drv_qp_query_info(qp_cb, rdev_cb,
                               attr, qp_attr, &gid_tmp);
    CHK_PRT_RETURN(ret, hccp_err("query qp info failed, ret %d", ret), ret);

    ret = rs_drv_get_random_num(&rand_num);
    CHK_PRT_RETURN(ret, hccp_err("get random num failed, ret %d", ret), ret);

    qp_cb->qp_info_lo.cmd = (unsigned int)RS_CMD_QP_INFO;
    qp_cb->qp_info_lo.lid = attr->lid;
    qp_cb->qp_info_lo.psn = (unsigned int)rand_num & 0xffffff;
    qp_cb->qp_info_lo.qpn = (int)qp_cb->ib_qp->qp_num;
    if (rdev_cb->notify_type != NO_USE && rdev_cb->notify_mr != NULL) {
        qp_cb->qp_info_lo.notify_mr.addr = rdev_cb->notify_va_base;
        qp_cb->qp_info_lo.notify_mr.len = rdev_cb->notify_size;
        qp_cb->qp_info_lo.notify_mr.rkey = rdev_cb->notify_mr->rkey;
    } else {
        qp_cb->qp_info_lo.notify_mr.addr = 0;
        qp_cb->qp_info_lo.notify_mr.len = 0;
        qp_cb->qp_info_lo.notify_mr.rkey = 0;
    }

    ret = memcpy_s(qp_cb->qp_info_lo.gid.raw, sizeof(qp_cb->qp_info_lo.gid.raw), gid_tmp.raw, sizeof(gid_tmp.raw));
    CHK_PRT_RETURN(ret, hccp_err("memcpy_s raw failed, ret:%d, dst_len:%u, src_len:%d", ret,
        sizeof(qp_cb->qp_info_lo.gid.raw), RS_QP_ATTR_GID_LEN), -ESAFEFUNC);
    return 0;
}

STATIC int rs_drv_exp_qp_create_init(struct ibv_exp_qp_init_attr *qp_init_attr,
    struct rs_qp_cb *qp_cb, struct ibv_port_attr *attr)
{
    int ret;
    ret = memset_s(attr, sizeof(struct ibv_port_attr), 0, sizeof(struct ibv_port_attr));
    CHK_PRT_RETURN(ret, hccp_err("memset_s for attr fail, ret:%d", ret), -ESAFEFUNC);
    ret = memset_s(qp_init_attr, sizeof(struct ibv_exp_qp_init_attr), 0, sizeof(struct ibv_exp_qp_init_attr));
    CHK_PRT_RETURN(ret, hccp_err("memset_s for qp_init_attr fail, ret:%d", ret), -ESAFEFUNC);

    qp_init_attr->attr.qp_type = IBV_QPT_RC;
    qp_init_attr->attr.send_cq = qp_cb->ib_send_cq;
    qp_init_attr->attr.recv_cq = qp_cb->ib_recv_cq;
    qp_init_attr->attr.cap.max_inline_data = QP_DEFAULT_MAX_CAP_INLINE_DATA;
    qp_init_attr->attr.cap.max_send_wr = qp_cb->tx_depth;
    qp_init_attr->attr.cap.max_send_sge = qp_cb->send_sge_num;
    qp_init_attr->attr.cap.max_recv_wr = qp_cb->rx_depth;
    qp_init_attr->attr.cap.max_recv_sge = qp_cb->recv_sge_num;

    return 0;
}

STATIC int rs_drv_exp_qp_create(struct rs_qp_cb *qp_cb, int qp_mode)
{
    int ret;
    struct ibv_qp_attr qp_attr;
    struct ibv_exp_qp_init_attr qp_init_attr;
    struct rs_rdev_cb *rdev_cb = NULL;
    struct ibv_port_attr attr;

    hccp_dbg("qp exp create begin..");
    rdev_cb = qp_cb->rdev_cb;
    qp_cb->qp_mode = qp_mode;
    ret = rs_drv_exp_qp_create_init(&qp_init_attr, qp_cb, &attr);
    CHK_PRT_RETURN(ret, hccp_err("rs_drv_qp_create_init fail, ret:%d", ret), ret);

    qp_init_attr.gdr_enable = qp_mode;
    qp_init_attr.lite_op_support = rs_drv_get_support_lite(rdev_cb, qp_cb->qp_mode, qp_cb->ai_op_support);
    qp_init_attr.mem_align = qp_cb->mem_align;
    qp_init_attr.mem_idx = qp_cb->mem_resp.mem_data.mem_idx;
    qp_cb->ib_qp = rs_ibv_exp_create_qp(qp_cb->ib_pd, &qp_init_attr, &qp_cb->qp_resp.qp_data);
    CHK_PRT_RETURN(qp_cb->ib_qp == NULL, hccp_err("rs_ibv_exp_create_qp fail"), -ENOMEM);

    qp_cb->db_index = (qp_mode == RA_RS_OP_QP_MODE ||
                       qp_mode == RA_RS_GDR_ASYN_QP_MODE) ? qp_cb->qp_resp.qp_data.qp_info : 0;
    qp_cb->sq_index = (qp_mode == RA_RS_GDR_TMPL_QP_MODE) ? qp_cb->qp_resp.qp_data.qp_info : 0;
    hccp_info("db index is [%d], sq index is [%d]", qp_cb->db_index, qp_cb->sq_index);

    /* query qp attr */
    ret = rs_ibv_query_qp(qp_cb->ib_qp, &qp_attr, IBV_QP_CAP, &qp_init_attr.attr);
    if (ret) {
        hccp_err("query qp attr failed ret %d", ret);
        ret = -EOPENSRC;
        goto exp_init_qp_err;
    }

    ret = rs_drv_qp_info_related(qp_cb, rdev_cb, &attr, &qp_attr);
    if (ret) {
        hccp_err("qp info related failed %d", ret);
        goto exp_init_qp_err;
    }

    hccp_info("chip_id %u, rdev_index:%u, qp[%d] create succ.", qp_cb->rdev_cb->rs_cb->chip_id,
        qp_cb->rdev_cb->rdev_index, qp_cb->qp_info_lo.qpn);

    return 0;

exp_init_qp_err:
    (void)rs_ibv_destroy_qp(qp_cb->ib_qp);
    return ret;
}

STATIC int rs_drv_normal_qp_create_init(struct ibv_qp_init_attr *qp_init_attr,
    struct rs_qp_cb *qp_cb, struct ibv_port_attr *attr)
{
    int ret;
    ret = memset_s(attr, sizeof(struct ibv_port_attr), 0, sizeof(struct ibv_port_attr));
    CHK_PRT_RETURN(ret, hccp_err("memset_s for attr fail, ret:%d", ret), -ESAFEFUNC);
    ret = memset_s(qp_init_attr, sizeof(struct ibv_qp_init_attr), 0, sizeof(struct ibv_qp_init_attr));
    CHK_PRT_RETURN(ret, hccp_err("memset_s for qp_init_attr fail, ret:%d", ret), -ESAFEFUNC);

    qp_init_attr->qp_type = IBV_QPT_RC;
    qp_init_attr->send_cq = qp_cb->ib_send_cq;
    qp_init_attr->recv_cq = qp_cb->ib_recv_cq;
    qp_init_attr->cap.max_inline_data = QP_DEFAULT_MAX_CAP_INLINE_DATA;
    qp_init_attr->cap.max_send_wr = qp_cb->tx_depth;
    qp_init_attr->cap.max_send_sge = qp_cb->send_sge_num;
    qp_init_attr->cap.max_recv_wr = qp_cb->rx_depth;
    qp_init_attr->cap.max_recv_sge = qp_cb->recv_sge_num;
    return 0;
}

STATIC int rs_drv_qp_normal(struct rs_qp_cb *qp_cb, int qp_mode)
{
    int ret;
    struct ibv_qp_attr qp_attr;
    struct ibv_qp_init_attr qp_init_attr;
    struct rs_rdev_cb *rdev_cb = NULL;
    struct ibv_port_attr attr;

    hccp_dbg("qp normal create begin..");
    rdev_cb = qp_cb->rdev_cb;
    qp_cb->qp_mode = qp_mode;
    ret = rs_drv_normal_qp_create_init(&qp_init_attr, qp_cb, &attr);
    CHK_PRT_RETURN(ret, hccp_err("rs_drv_normal_qp_create_init fail, ret:%d", ret), ret);

    qp_cb->ib_qp = rs_ibv_create_qp(qp_cb->ib_pd, &qp_init_attr);
    CHK_PRT_RETURN(qp_cb->ib_qp == NULL, hccp_err("rs_ibv_create_qp fail, errno=%d", errno), -ENOMEM);

    /* query qp attr */
    ret = rs_ibv_query_qp(qp_cb->ib_qp, &qp_attr, IBV_QP_CAP, &qp_init_attr);
    if (ret) {
        hccp_err("query qp attr failed ret %d", ret);
        ret = -EOPENSRC;
        goto normal_init_qp_err;
    }

    ret = rs_drv_qp_info_related(qp_cb, rdev_cb, &attr, &qp_attr);
    if (ret) {
        hccp_err("qp info related failed %d", ret);
        goto normal_init_qp_err;
    }

    hccp_info("chip_id %u, rdev_index:%u, qp[%d] create succ.", qp_cb->rdev_cb->rs_cb->chip_id,
        qp_cb->rdev_cb->rdev_index, qp_cb->qp_info_lo.qpn);

    return 0;

normal_init_qp_err:
    (void)rs_ibv_destroy_qp(qp_cb->ib_qp);
    return ret;
}

int rs_drv_qp_create(struct rs_qp_cb *qp_cb, struct rs_qp_norm *qp_norm)
{
    int ret;
    if (qp_norm->is_exp != 0 && qp_norm->qp_mode != RA_RS_NOR_QP_MODE) {
        ret = rs_drv_exp_qp_create(qp_cb, qp_norm->qp_mode);
    } else {
        ret = rs_drv_qp_normal(qp_cb, qp_norm->qp_mode);
    }

    return ret;
}

STATIC int rs_drv_exp_qp_create_init_with_attrs(struct ibv_exp_qp_init_attr *qp_init_attr, struct rs_qp_cb *qp_cb,
    struct ibv_port_attr *attr, struct rs_qp_norm_with_attrs *qp_norm)
{
    int ret;

    ret = memset_s(attr, sizeof(struct ibv_port_attr), 0, sizeof(struct ibv_port_attr));
    CHK_PRT_RETURN(ret, hccp_err("memset_s for attr fail, ret:%d", ret), -ESAFEFUNC);
    ret = memset_s(qp_init_attr, sizeof(struct ibv_exp_qp_init_attr), 0, sizeof(struct ibv_exp_qp_init_attr));
    CHK_PRT_RETURN(ret, hccp_err("memset_s for qp_init_attr fail, ret:%d", ret), -ESAFEFUNC);

    qp_init_attr->attr.send_cq = qp_cb->ib_send_cq;
    qp_init_attr->attr.recv_cq = qp_cb->ib_recv_cq;

    ret = memcpy_s(&qp_init_attr->attr.cap, sizeof(struct ibv_qp_cap), &qp_norm->ext_attrs.qp_attr.cap,
        sizeof(struct ibv_qp_cap));
    CHK_PRT_RETURN(ret, hccp_err("memset_s for qp_init_attr fail, ret:%d", ret), -ENOMEM);
    qp_init_attr->attr.qp_type = qp_norm->ext_attrs.qp_attr.qp_type;
    qp_init_attr->attr.sq_sig_all = qp_norm->ext_attrs.qp_attr.sq_sig_all;

    qp_init_attr->udp_sport = qp_cb->udp_sport;

    return 0;
}

STATIC int rs_drv_exp_qp_create_with_attrs(struct rs_qp_cb *qp_cb, struct rs_qp_norm_with_attrs *qp_norm)
{
    int ret;
    struct ibv_port_attr attr;
    struct ibv_qp_attr qp_attr;
    struct rs_rdev_cb *rdev_cb = NULL;
    struct ibv_exp_qp_init_attr qp_init_attr;

    hccp_dbg("qp exp create begin..");
    rdev_cb = qp_cb->rdev_cb;
    ret = rs_drv_exp_qp_create_init_with_attrs(&qp_init_attr, qp_cb, &attr, qp_norm);
    CHK_PRT_RETURN(ret, hccp_err("rs_drv_qp_create_init fail, ret:%d", ret), ret);

    qp_init_attr.gdr_enable = qp_cb->qp_mode;
    qp_init_attr.lite_op_support = rs_drv_get_support_lite(rdev_cb, qp_cb->qp_mode, qp_cb->ai_op_support);
    qp_init_attr.mem_align = qp_cb->mem_align;
    qp_init_attr.mem_idx = qp_cb->mem_resp.mem_data.mem_idx;
    qp_init_attr.ai_op_support = qp_cb->ai_op_support;
    qp_init_attr.grp_id = qp_cb->grp_id;
    qp_init_attr.qp_cstm_flag = qp_cb->ai_op_support;
    qp_cb->ib_qp = rs_ibv_exp_create_qp(qp_cb->ib_pd, &qp_init_attr, &qp_cb->qp_resp.qp_data);
    CHK_PRT_RETURN(qp_cb->ib_qp == NULL, hccp_err("rs_ibv_exp_create_qp fail"), -ENOMEM);

    qp_cb->db_index = (qp_cb->qp_mode == RA_RS_OP_QP_MODE ||
                       qp_cb->qp_mode == RA_RS_GDR_ASYN_QP_MODE) ? qp_cb->qp_resp.qp_data.qp_info : 0;
    qp_cb->sq_index = (qp_cb->qp_mode == RA_RS_GDR_TMPL_QP_MODE) ? qp_cb->qp_resp.qp_data.qp_info : 0;
    hccp_info("db index is [%d], sq index is [%d]", qp_cb->db_index, qp_cb->sq_index);

    /* query qp attr */
    ret = rs_ibv_query_qp(qp_cb->ib_qp, &qp_attr, IBV_QP_CAP, &qp_init_attr.attr);
    if (ret) {
        hccp_err("query qp attr failed ret %d", ret);
        ret = -EOPENSRC;
        goto exp_init_qp_err;
    }

    ret = rs_drv_qp_info_related(qp_cb, rdev_cb, &attr, &qp_attr);
    if (ret) {
        hccp_err("qp info related failed %d", ret);
        goto exp_init_qp_err;
    }

    hccp_info("chip_id %u, rdev_index:%u, qp[%d] create succ.", qp_cb->rdev_cb->rs_cb->chip_id,
        qp_cb->rdev_cb->rdev_index, qp_cb->qp_info_lo.qpn);

    return 0;

exp_init_qp_err:
    (void)rs_ibv_destroy_qp(qp_cb->ib_qp);
    return ret;
}

STATIC int rs_drv_normal_qp_create_init_with_attrs(struct ibv_qp_init_attr *qp_init_attr,
    struct rs_qp_cb *qp_cb, struct ibv_port_attr *attr, struct rs_qp_norm_with_attrs *qp_norm)
{
    int ret;

    ret = memset_s(attr, sizeof(struct ibv_port_attr), 0, sizeof(struct ibv_port_attr));
    CHK_PRT_RETURN(ret, hccp_err("memset_s for attr fail, ret:%d", ret), -ESAFEFUNC);
    ret = memset_s(qp_init_attr, sizeof(struct ibv_qp_init_attr), 0, sizeof(struct ibv_qp_init_attr));
    CHK_PRT_RETURN(ret, hccp_err("memset_s for qp_init_attr fail, ret:%d", ret), -ESAFEFUNC);

    qp_init_attr->send_cq = qp_cb->ib_send_cq;
    qp_init_attr->recv_cq = qp_cb->ib_recv_cq;

    ret = memcpy_s(&qp_init_attr->cap, sizeof(struct ibv_qp_cap), &qp_norm->ext_attrs.qp_attr.cap,
        sizeof(struct ibv_qp_cap));
    CHK_PRT_RETURN(ret, hccp_err("memcpy_s for qp_init_attr fail, ret:%d", ret), -ENOMEM);
    qp_init_attr->qp_type = qp_norm->ext_attrs.qp_attr.qp_type;
    qp_init_attr->sq_sig_all = qp_norm->ext_attrs.qp_attr.sq_sig_all;

    return ret;
}

STATIC int rs_drv_qp_normal_with_attrs(struct rs_qp_cb *qp_cb, struct rs_qp_norm_with_attrs *qp_norm)
{
    int ret;
    struct ibv_port_attr attr;
    struct ibv_qp_attr qp_attr;
    struct ibv_qp_init_attr qp_init_attr;
    struct rs_rdev_cb *rdev_cb = NULL;

    hccp_dbg("qp normal create begin..");
    rdev_cb = qp_cb->rdev_cb;
    ret = rs_drv_normal_qp_create_init_with_attrs(&qp_init_attr, qp_cb, &attr, qp_norm);
    CHK_PRT_RETURN(ret, hccp_err("rs_drv_normal_qp_create_init fail, ret:%d", ret), ret);

    qp_cb->ib_qp = rs_ibv_create_qp(qp_cb->ib_pd, &qp_init_attr);
    CHK_PRT_RETURN(qp_cb->ib_qp == NULL, hccp_err("rs_ibv_create_qp fail, errno=%d", errno), -ENOMEM);

    /* query qp attr */
    ret = rs_ibv_query_qp(qp_cb->ib_qp, &qp_attr, IBV_QP_CAP, &qp_init_attr);
    if (ret) {
        hccp_err("query qp attr failed ret %d", ret);
        ret = -EOPENSRC;
        goto normal_init_qp_err;
    }

    ret = rs_drv_qp_info_related(qp_cb, rdev_cb, &attr, &qp_attr);
    if (ret) {
        hccp_err("qp info related failed %d", ret);
        goto normal_init_qp_err;
    }

    hccp_info("chip_id %u, rdev_index:%u, qp[%d] create succ.", qp_cb->rdev_cb->rs_cb->chip_id,
        qp_cb->rdev_cb->rdev_index, qp_cb->qp_info_lo.qpn);

    return 0;

normal_init_qp_err:
    (void)rs_ibv_destroy_qp(qp_cb->ib_qp);
    return ret;
}

int rs_drv_qp_create_with_attrs(struct rs_qp_cb *qp_cb, struct rs_qp_norm_with_attrs *qp_norm)
{
    int ret;

    // ignore qp_mode when ai_op_support set
    if ((qp_norm->is_exp != 0 && qp_norm->ext_attrs.qp_mode != RA_RS_NOR_QP_MODE) || (qp_norm->ai_op_support != 0)) {
        ret = rs_drv_exp_qp_create_with_attrs(qp_cb, qp_norm);
    } else {
        ret = rs_drv_qp_normal_with_attrs(qp_cb, qp_norm);
    }

    return ret;
}

void rs_drv_qp_destroy(struct rs_qp_cb *qp_cb)
{
    (void)rs_ibv_destroy_qp(qp_cb->ib_qp);
}

int rs_query_event(int cq_event_id, struct event_summary **event)
{
   *event = calloc(1, sizeof(struct event_summary));
    if ((*event) == NULL) {
        return -ENOMEM;
    }

    (*event)->pid = getpid();
    (*event)->grp_id = 6; // 6: 通信库使用的事件调度group
    (*event)->event_id = cq_event_id;
    (*event)->subevent_id = 0;
    (*event)->msg_len = 0;
    (*event)->msg = NULL;
    (*event)->dst_engine = ACPU_DEVICE;
    (*event)->policy = ONLY;
    hccp_info("pid:%d, cq_event_id:%d", (*event)->pid, cq_event_id);
    unsigned int i;
    for (i = 0; i < EVENT_SUMMARY_RSV; i++) {
        (*event)->rsv[i] = 0;
    }

    return 0;
}

int rs_drv_create_cq_event(struct rs_cq_context *cq_context, struct cq_attr *attr)
{
    int ret;

    hccp_info("create cq event start cq_create_mode [%d]", cq_context->cq_create_mode);

    if (cq_context->cq_create_mode == RS_NORMAL_CQ_CREATE || cq_context->cq_create_mode == RS_SQ_CQ_CREATE) {
        ret = rs_query_event(attr->send_cq_event_id, &(cq_context->send_event));
        CHK_PRT_RETURN(ret, hccp_err("rs_query_event send_event failed!ret:%d", ret), ret);

        cq_context->ib_send_cq = rs_ibv_create_cq(cq_context->rdev_cb->ib_ctx, attr->send_cq_depth,
            cq_context->send_event, cq_context->channel, cq_context->eq_num);
        if (cq_context->ib_send_cq == NULL) {
            hccp_err("rs_drv_create_cq_event ibv create send cq fail,send_cq_event_id:%d", attr->send_cq_event_id);
            goto create_cq_even_err;
        }

        ret = ibv_req_notify_cq(cq_context->ib_send_cq, 0);
        if (ret) {
            hccp_err("Couldn't request send CQ notification, ret:%d", ret);
            goto create_cq_even_err;
        }
        *attr->ib_send_cq = cq_context->ib_send_cq;
    }

    if (cq_context->cq_create_mode == RS_NORMAL_CQ_CREATE || cq_context->cq_create_mode == RS_SRQ_CQ_CREATE) {
        ret = rs_query_event(attr->recv_cq_event_id, &(cq_context->recv_event));
        if (ret) {
            hccp_err("rs_query_event send_event failed!ret:%d", ret);
            goto create_cq_even_err;
        }

        cq_context->ib_recv_cq = rs_ibv_create_cq(cq_context->rdev_cb->ib_ctx, attr->recv_cq_depth,
            cq_context->recv_event, cq_context->channel, cq_context->eq_num);
        if (cq_context->ib_recv_cq == NULL) {
            hccp_err("rs_drv_create_cq_event ibv create recv cq fail,recv_cq_event_id:%d", attr->recv_cq_event_id);
            goto create_cq_even_err;
        }

        ret = ibv_req_notify_cq(cq_context->ib_recv_cq, 0);
        if (ret) {
            hccp_err("Couldn't request recv CQ notification, ret:%d", ret);
            goto create_cq_even_err;
        }
        *attr->ib_recv_cq = cq_context->ib_recv_cq;
    }

    hccp_info("create cq event success");
    return 0;

create_cq_even_err:
    if (cq_context->ib_recv_cq != NULL) {
        (void)rs_ibv_destroy_cq(cq_context->ib_recv_cq);
    }

    if (cq_context->ib_send_cq != NULL) {
        (void)rs_ibv_destroy_cq(cq_context->ib_send_cq);
    }

    (void)rs_drv_event_destroy(cq_context->recv_event);

    (void)rs_drv_event_destroy(cq_context->send_event);

    return -EOPENSRC;
}

int rs_drv_create_cq_with_channel(struct rs_cq_context *cq_context, struct cq_attr *attr)
{
    hccp_dbg("create cq with channel start");

    cq_context->ib_send_cq = rs_ibv_create_cq(cq_context->rdev_cb->ib_ctx, attr->send_cq_depth,
        NULL, attr->send_channel, 1);
    if (cq_context->ib_send_cq == NULL) {
        hccp_err("rs_drv_create_cq_with_channel ibv create send cq fail.");
        return -EOPENSRC;
    }

    cq_context->ib_recv_cq = rs_ibv_create_cq(cq_context->rdev_cb->ib_ctx, attr->recv_cq_depth,
        NULL, attr->recv_channel, 1);
    if (cq_context->ib_recv_cq == NULL) {
        hccp_err("rs_drv_create_cq_with_channel ibv create serecvnd cq fail.");
        goto create_recv_cq_err;
    }

    *attr->ib_send_cq = cq_context->ib_send_cq;
    *attr->ib_recv_cq = cq_context->ib_recv_cq;
    hccp_info("create cq with channel success");
    return 0;

create_recv_cq_err:
    (void)rs_ibv_destroy_cq(cq_context->ib_send_cq);
    return -EOPENSRC;
}

int rs_drv_destroy_cq_event(struct rs_cq_context *cq_context)
{
    int ret;

    if (cq_context->cq_create_mode == RS_NORMAL_CQ_CREATE || cq_context->cq_create_mode == RS_SRQ_CQ_CREATE) {
        ret = rs_ibv_destroy_cq(cq_context->ib_recv_cq);
        if (ret) {
            hccp_err("rs_ibv_destroy_cq(recv) failed, ret %d", ret);
        }
        (void)rs_drv_event_destroy(cq_context->recv_event);
    }

    if (cq_context->cq_create_mode == RS_NORMAL_CQ_CREATE || cq_context->cq_create_mode == RS_SQ_CQ_CREATE) {
        ret = rs_ibv_destroy_cq(cq_context->ib_send_cq);
        if (ret) {
            hccp_err("rs_ibv_destroy_cq(send) failed, ret %d", ret);
        }
        (void)rs_drv_event_destroy(cq_context->send_event);
    }

    return 0;
}

int rs_drv_normal_qp_create(struct rs_qp_cb *qp_cb, struct ibv_qp_init_attr *qp_init_attr)
{
    int ret;
    struct ibv_qp_attr qp_attr;
    struct rs_rdev_cb *rdev_cb = NULL;
    struct ibv_port_attr attr;

    hccp_dbg("rs_drv_normal_qp_create begin..");
    rdev_cb = qp_cb->rdev_cb;

    qp_cb->ib_qp = rs_ibv_create_qp(qp_cb->ib_pd, qp_init_attr);
    CHK_PRT_RETURN(qp_cb->ib_qp == NULL, hccp_err("rs_ibv_create_qp fail, errno=%d", errno), -ENOMEM);

    /* query qp attr */
    ret = rs_ibv_query_qp(qp_cb->ib_qp, &qp_attr, IBV_QP_CAP, qp_init_attr);
    if (ret) {
        hccp_err("query qp attr failed ret %d", ret);
        ret = -EOPENSRC;
        goto normal_init_qp_err;
    }

    ret = rs_drv_qp_info_related(qp_cb, rdev_cb, &attr, &qp_attr);
    if (ret) {
        hccp_err("qp info related failed %d", ret);
        goto normal_init_qp_err;
    }

    hccp_info("chip_id %u, rdev_index:%u, qp[%d] create succ.", qp_cb->rdev_cb->rs_cb->chip_id,
        qp_cb->rdev_cb->rdev_index, qp_cb->qp_info_lo.qpn);

    return 0;

normal_init_qp_err:
    (void)rs_ibv_destroy_qp(qp_cb->ib_qp);
    return ret;
}

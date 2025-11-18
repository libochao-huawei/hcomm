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
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dlfcn.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <errno.h>
#include "securec.h"
#include "rs.h"
#include "ra_rs_err.h"
#include "rs_inner.h"
#include "rs_drv_socket.h"
#include "rs_epoll.h"
#include "dl_ibverbs_function.h"
#include "rs_drv_rdma.h"
#include "rs_rdma.h"

unsigned int g_rs_send_wr_num = 0;

STATIC void rs_buf_print(char *addr, int len)
{
    int i;

    for (i = 0; i < len; i++) {
        hccp_info("0x%02x ", *(addr + i));
    }
}

STATIC int rs_get_qpcb(struct rs_rdev_cb *rdev_cb, uint32_t qpn, struct rs_qp_cb **qp_cb)
{
    struct rs_qp_cb *qp_cb_tmp = NULL;
    struct rs_qp_cb *qp_cb_tmp2 = NULL;

    RS_LIST_GET_HEAD_ENTRY(qp_cb_tmp, qp_cb_tmp2, &rdev_cb->qp_list, list, struct rs_qp_cb);
    for (; (&qp_cb_tmp->list) != &rdev_cb->qp_list;
        qp_cb_tmp = qp_cb_tmp2, qp_cb_tmp2 = list_entry(qp_cb_tmp2->list.next, struct rs_qp_cb, list)) {
        if (qp_cb_tmp->ib_qp->qp_num == qpn) {
            *qp_cb = qp_cb_tmp;
            return 0;
        }
    }

    *qp_cb = NULL;
    hccp_err("qp_cb for qp %u do not available!", qpn);

    return -ENODEV;
}

int rs_qpn2qpcb(unsigned int phy_id, unsigned int rdev_index, uint32_t qpn, struct rs_qp_cb **qp_cb)
{
    int ret;
    unsigned int chip_id;
    struct rs_cb *rs_cb = NULL;
    struct rs_rdev_cb *rdev_cb = NULL;

    CHK_PRT_RETURN(phy_id >= RS_MAX_DEV_NUM, hccp_err("rs set param error! phy_id:%u", phy_id), -EINVAL);

    ret = rsGetLocalDevIDByHostDevID(phy_id, &chip_id);
    CHK_PRT_RETURN(ret, hccp_err("rs_qpn2qpcb rsGetLocalDevIDByHostDevID phy_id[%u] invalid, ret:%d",
        phy_id, ret), ret);

    ret = rs_dev2rscb(chip_id, &rs_cb, false);
    CHK_PRT_RETURN(ret, hccp_err("rs_qpn2qpcb get rs_cb failed, ret:%d", ret), -ENODEV);

    ret = rs_get_rdev_cb(rs_cb, rdev_index, &rdev_cb);
    CHK_PRT_RETURN(ret, hccp_err("rs_get_rdev_cb failed! ret:%d, rdev_index:%u", ret, rdev_index), ret);

    ret = rs_get_qpcb(rdev_cb, qpn, qp_cb);
    CHK_PRT_RETURN(ret, hccp_err("rs_get_qpcb failed! ret:%d, qpn:%u", ret, qpn), ret);

    return 0;
}

STATIC int rs_get_mrcb(struct rs_qp_cb *qp_cb, uint64_t addr, struct rs_mr_cb **mr_cb,
    struct rs_list_head *mr_list)
{
    struct rs_mr_cb *mr_tmp = NULL;
    struct rs_mr_cb *mr_tmp2 = NULL;

    RS_PTHREAD_MUTEX_LOCK(&qp_cb->qp_mutex);
    RS_LIST_GET_HEAD_ENTRY(mr_tmp, mr_tmp2, mr_list, list, struct rs_mr_cb);
    for (; (&mr_tmp->list) != mr_list;
        mr_tmp = mr_tmp2, mr_tmp2 = list_entry(mr_tmp2->list.next, struct rs_mr_cb, list)) {
        if ((mr_tmp->mr_info.addr <= addr) && (addr < mr_tmp->mr_info.addr + mr_tmp->mr_info.len)) {
            *mr_cb = mr_tmp;
            RS_PTHREAD_MUTEX_ULOCK(&qp_cb->qp_mutex);
            return 0;
        }
    }

    *mr_cb = NULL;
    RS_PTHREAD_MUTEX_ULOCK(&qp_cb->qp_mutex);

    hccp_info("cannot find mrcb for addr@0x%lx !", addr);

    return -ENODEV;
}

STATIC void *rs_notify_mr_list_add(struct rs_qp_cb *qp_cb, const char *buf)
{
    int ret;
    struct rs_mr_cb *notify_mr_cb;

    notify_mr_cb = calloc(1, sizeof(struct rs_mr_cb));
    CHK_PRT_RETURN(notify_mr_cb == NULL, hccp_err("notify_mr_cb calloc failed"), NULL);
    ret = memcpy_s(&notify_mr_cb->mr_info, sizeof(struct rs_mr_info),
                   &((const struct rs_qp_info *)buf)->notify_mr, sizeof(struct rs_mr_info));
    if (ret) {
        hccp_err("memcpy_s failed, ret:%d, src_len:%u, dst_len:%u",
            ret, sizeof(struct rs_mr_info), sizeof(struct rs_mr_info));
        free(notify_mr_cb);
        notify_mr_cb = NULL;
        return NULL;
    }

    hccp_info("qpn is %d, rdev_index:%u, chip_id %u, recv notify va is 0x%llx, notify size is %llu",
        qp_cb->qp_info_lo.qpn, qp_cb->rdev_cb->rdev_index, qp_cb->rdev_cb->rs_cb->chip_id,
        notify_mr_cb->mr_info.addr, notify_mr_cb->mr_info.len);

    rs_list_add_tail(&notify_mr_cb->list, &qp_cb->rem_mr_list);

    return notify_mr_cb;
}

STATIC int rs_qp_state_modify(struct rs_qp_cb *qp_cb)
{
    struct ibv_qp_init_attr init_attr = { 0 };
    struct ibv_qp_attr attr = { 0 };
    enum ibv_qp_state state;
    int ret;

    // see ib_modify_qp_is_ok for status modify, only support modify qp from INIT to RTR
    ret = rs_ibv_query_qp(qp_cb->ib_qp, &attr, IBV_QP_STATE, &init_attr);
    if (ret != 0) {
        hccp_warn("rs_ibv_query_qp qpn:%d unsuccessful, ret:%d", qp_cb->qp_info_lo.qpn, ret);
        state = IBV_QPS_UNKNOWN;
    } else {
        state = attr.qp_state;
    }

    // disallow modify qp from IBV_QPS_RTS to IBV_QPS_RTS
    if (state == IBV_QPS_RTS) {
        hccp_err("qpn:%d disallow modify from %d", qp_cb->qp_info_lo.qpn, state);
        return -EINVAL;
    }

    hccp_info("qpn:%d state:%d start modify", qp_cb->qp_info_lo.qpn, state);

    // modify qp from others to RESET
    if (state != IBV_QPS_RESET && state != IBV_QPS_INIT && state != IBV_QPS_RTR) {
        ret = rs_drv_qp_state_modifyto_reset(qp_cb);
        CHK_PRT_RETURN(ret, hccp_err("qpn:%d modify %d to reset failed, ret:%d", qp_cb->qp_info_lo.qpn, state, ret),
            ret);
        state = IBV_QPS_RESET;
    }

    // modify qp from RESET to INIT
    if (state == IBV_QPS_RESET) {
        ret = rs_drv_qp_state_modifyto_init(qp_cb, &attr);
        CHK_PRT_RETURN(ret, hccp_err("qpn:%d modify %d to init failed, ret %d", qp_cb->qp_info_lo.qpn, state, ret),
            ret);
        state = IBV_QPS_INIT;
    }

    // modify qp from INIT to RTR
    if (state == IBV_QPS_INIT) {
        ret = rs_drv_qp_state_modifyto_rtr(qp_cb, &attr);
        CHK_PRT_RETURN(ret, hccp_err("qpn:%d modify %d to rtr failed, ret %d", qp_cb->qp_info_lo.qpn, state, ret), ret);
        state = IBV_QPS_RTR;
    }

    // modify qp from RTR to RTS
    if (state == IBV_QPS_RTR) {
        ret = rs_drv_qp_state_modifyto_rts(qp_cb, &attr);
        CHK_PRT_RETURN(ret, hccp_err("qpn:%d modify %d to rts failed, ret %d", qp_cb->qp_info_lo.qpn, state, ret), ret);
    }

    hccp_info("local qpn[%d] remote qpn[%d] modify succ", qp_cb->qp_info_lo.qpn, qp_cb->qp_info_rem.qpn);

    return 0;
}

STATIC int rs_epoll_recv_qp_handle(struct rs_qp_cb *qp_cb, const char *buf_tmp)
{
    int ret;
    float time_cost = 0.0;

    ret = memcpy_s(&qp_cb->qp_info_rem, sizeof(struct rs_qp_info),
                   buf_tmp, sizeof(struct rs_qp_info));
    CHK_PRT_RETURN(ret, hccp_err("memcpy_s failed[%d], dest size:%d, src size:%d", ret, sizeof(struct rs_qp_info),
        sizeof(struct rs_qp_info)), -ENOMEM);

    /* modify qp state to RTR/RTS */
    ret = rs_qp_state_modify(qp_cb);
    CHK_PRT_RETURN(ret, hccp_err("rs_qp_state_modify local qpn[%d] remote qpn[%d] failed ret[%d]",
        qp_cb->qp_info_lo.qpn, qp_cb->qp_info_rem.qpn, ret), ret);

    rs_get_cur_time(&qp_cb->end_time);
    hccp_time_interval(&qp_cb->end_time, &qp_cb->start_time, &time_cost);
    if (time_cost > RS_EXPECT_TIME_MAX) {
        hccp_warn("local qpn[%d] remote qpn [%d] connect success cost[%f] more than[%f]ms!", qp_cb->qp_info_lo.qpn,
            qp_cb->qp_info_rem.qpn, time_cost, RS_EXPECT_TIME_MAX);
    } else {
        hccp_info("local qpn[%d] remote qpn [%d] connect success! cost [%f] ms", qp_cb->qp_info_lo.qpn,
            qp_cb->qp_info_rem.qpn, time_cost);
    }

    hccp_info("qp [%d] state has been migrate to RTS!, qp_cb state is %d", qp_cb->qp_info_lo.qpn, qp_cb->state);

    return 0;
}

STATIC void *rs_epoll_recv_mr_handle(struct rs_qp_cb *qp_cb, const char *buf_tmp)
{
    int ret;
    struct rs_mr_cb *mr_cb;

    mr_cb = calloc(1, sizeof(struct rs_mr_cb));
    CHK_PRT_RETURN(mr_cb == NULL, hccp_err("mr_cb calloc failed"), NULL);
    ret = memcpy_s(&mr_cb->mr_info, sizeof(struct rs_mr_info), buf_tmp, sizeof(struct rs_mr_info));
    if (ret) {
        hccp_err("memcpy_s failed[%d], dest size:%u, src size:%u", ret, sizeof(struct rs_mr_info),
            sizeof(struct rs_mr_info));
        free(mr_cb);
        mr_cb = NULL;
        return NULL;
    }

    rs_list_add_tail(&mr_cb->list, &qp_cb->rem_mr_list);

    hccp_info("recv mr addr is 0x%llx", mr_cb->mr_info.addr);
    hccp_info("recv mr len is %llu", mr_cb->mr_info.len);

    return mr_cb;
}

STATIC int rs_cmd_qp_info_handle(struct rs_qp_cb *qp_cb, unsigned int total_size,
    const char *buf_tmp, unsigned int cur_size, bool *flag)
{
    int ret;
    CHK_PRT_RETURN((total_size - cur_size) < sizeof(struct rs_qp_info), hccp_info("qp_info remain size"
        "[%u] < size [%u], wait for next recv", total_size - cur_size, sizeof(struct rs_qp_info)), -EINVAL);

    ret = rs_epoll_recv_qp_handle(qp_cb, buf_tmp);
    CHK_PRT_RETURN(ret, hccp_err("rs_epoll_recv_qp_handle failed! ret[%d]", ret), ret);

    rs_notify_mr_list_add(qp_cb, buf_tmp);
    hccp_info("rs_notify_mr_list_add");

    *flag = true;
    hccp_info("qp_info cur_size(%u) len(%u) !", cur_size, sizeof(struct rs_qp_info));

    return 0;
}

STATIC int rs_cmd_mr_info_handle(struct rs_qp_cb *qp_cb, unsigned int total_size, const char *buf_tmp,
    unsigned int cur_size, bool *flag)
{
    CHK_PRT_RETURN((total_size - cur_size) < sizeof(struct rs_mr_info), hccp_info("mr_info remain size"
        "[%u] < size [%u], wait for next recv", total_size - cur_size, sizeof(struct rs_mr_info)), -EINVAL);

    (void)rs_epoll_recv_mr_handle(qp_cb, buf_tmp);

    *flag = true;

    hccp_info("mr_info cur_size(%u) len(%u) !", cur_size, sizeof(struct rs_mr_info));

    return 0;
}

STATIC int rs_cmd_len_info_handle(struct rs_qp_cb *qp_cb, unsigned int total_size, const char *buf_tmp,
    unsigned int cur_size, bool *flag)
{
    CHK_PRT_RETURN((total_size - cur_size) < sizeof(struct rs_qp_len_info), hccp_info("len_info remain size"
        "[%u] < size [%u], wait for next recv", total_size - cur_size, sizeof(struct rs_qp_len_info)), -EINVAL);

    qp_cb->expect_len = *((const uint32_t*)(buf_tmp + sizeof(uint32_t)));

    *flag = true;

    return 0;
}

STATIC void rs_epoll_recv_handle_remain(struct rs_qp_cb *qp_cb, unsigned int total_size,
    unsigned int cur_size, bool flag, const char *buf_tmp)
{
    int ret = 0;

    qp_cb->remain_size = total_size - cur_size;
    if ((qp_cb->remain_size > 0) && (flag == true)) {
        ret = memcpy_s(qp_cb->qp_mr_buf, RS_BUF_SIZE, buf_tmp, qp_cb->remain_size);
        if (ret) {
            hccp_err("memcpy_s failed, ret:%d, remain_size:%u", ret, qp_cb->remain_size);
            return;
        }
    }

    return;
}

STATIC void rs_epoll_recv_handle(struct rs_qp_cb *qp_cb, char *buf, int size)
{
    unsigned int total_size = qp_cb->remain_size + (unsigned int)size;
    char *buf_tmp = (char *)qp_cb->qp_mr_buf;
    unsigned int cur_size = 0;
    bool flag = false;
    uint32_t cmd;
    int ret;

    hccp_info("Message for qp:%d, qp_cb->remain_size:%u, size:%d", qp_cb->qp_info_lo.qpn, qp_cb->remain_size, size);
    ret = memcpy_s(qp_cb->qp_mr_buf + qp_cb->remain_size, RS_BUF_SIZE - qp_cb->remain_size, buf, size);
    if (ret) {
        hccp_err("memcpy_s failed, ret:%d, remain_size:%u, size:%d", ret, qp_cb->remain_size, size);
        return;
    }

    do {
        cmd = *((uint32_t *)buf_tmp);
        switch (cmd) {
            case RS_CMD_QP_INFO:
                ret = rs_cmd_qp_info_handle(qp_cb, total_size, buf_tmp, cur_size, &flag);
                if (ret) {
                    goto out;
                }

                cur_size += sizeof(struct rs_qp_info);
                buf_tmp = qp_cb->qp_mr_buf + cur_size;
                break;
            case RS_CMD_MR_INFO:
                ret = rs_cmd_mr_info_handle(qp_cb, total_size, buf_tmp, cur_size, &flag);
                if (ret) {
                    goto out;
                }

                cur_size += sizeof(struct rs_mr_info);
                buf_tmp = qp_cb->qp_mr_buf + cur_size;
                break;
            case RS_CMD_LEN_INFO:
                ret = rs_cmd_len_info_handle(qp_cb, total_size, buf_tmp, cur_size, &flag);
                if (ret) {
                    goto out;
                }
                cur_size += sizeof(struct rs_qp_len_info);
                buf_tmp = qp_cb->qp_mr_buf + cur_size;
                break;
            default:
                hccp_warn("qp %d, unknown cmd(0x%x)!", qp_cb->qp_info_lo.qpn, cmd);
                rs_buf_print(buf, size);
                return;
        }
    } while (cur_size < total_size);

out:
    rs_epoll_recv_handle_remain(qp_cb, total_size, cur_size, flag, buf_tmp);
}

STATIC void rs_qp_mr_recv_handle(int fd, struct rs_qp_cb *qp_cb)
{
    char buf[RS_BUF_SIZE];
    int size;
    int ret;

    RS_PTHREAD_MUTEX_LOCK(&qp_cb->qp_mutex);

    size = rs_socket_recv(fd, buf, RS_BUF_SIZE - qp_cb->remain_size);
    hccp_dbg("fd %d qpn %d read size = %d, qp_cb->remain_size:%u", fd, qp_cb->qp_info_lo.qpn, size, qp_cb->remain_size);

    if (size > 0) {
        qp_cb->recv_len += (uint32_t)size;
        rs_epoll_recv_handle(qp_cb, buf, size);
    } else if (size == 0) {
        hccp_dbg("fd %d read size = %d, remote fd has been closed, fd cannot use !", fd, size);
#ifdef CA_CONFIG_LLT
        qp_cb->state = RS_QP_STATUS_REM_FD_CLOSE;
#endif
    } else {
        ret = errno;
        hccp_dbg("no data available, errno:%d", ret);
    }
    RS_PTHREAD_MUTEX_ULOCK(&qp_cb->qp_mutex);

    return;
}

STATIC int rs_handle_qp_mr_epoll_event(struct rs_rdev_cb *rdev_cb, int fd)
{
    struct rs_qp_cb *qp_cb;
    struct rs_qp_cb *qp_cb2 = NULL;

    /* QP event, QP info exchange */
    RS_LIST_GET_HEAD_ENTRY(qp_cb, qp_cb2, &rdev_cb->qp_list, list, struct rs_qp_cb);
    for (; (&qp_cb->list) != &rdev_cb->qp_list;
        qp_cb = qp_cb2, qp_cb2 = list_entry(qp_cb2->list.next, struct rs_qp_cb, list)) {
        if (qp_cb->channel == NULL) {
            continue;
        }
        if (qp_cb->srq_context != NULL && qp_cb->srq_context->channel->fd == fd) {
            hccp_dbg("fd %d poll cq!", fd);
            rs_drv_poll_srq_cq_handle(qp_cb);
            return 0;
        }
        if (fd == qp_cb->channel->fd) {
            hccp_dbg("fd %d poll cq!", fd);
            rs_drv_poll_cq_handle(qp_cb);
            return 0;
        }
    }
    return -ENODEV;
}

int rs_epoll_event_qp_mr_in_handle(struct rs_cb *rs_cb, int fd)
{
    int ret;
    struct rs_rdev_cb *rdev_cb_tmp = NULL;
    struct rs_rdev_cb *rdev_cb_tmp2 = NULL;

    RS_LIST_GET_HEAD_ENTRY(rdev_cb_tmp, rdev_cb_tmp2, &rs_cb->rdev_list, list, struct rs_rdev_cb);
    for (; (&rdev_cb_tmp->list) != &rs_cb->rdev_list;
        rdev_cb_tmp = rdev_cb_tmp2, rdev_cb_tmp2 = list_entry(rdev_cb_tmp2->list.next, struct rs_rdev_cb, list)) {
            RS_PTHREAD_MUTEX_LOCK(&rdev_cb_tmp->rdev_mutex);
            ret = rs_handle_qp_mr_epoll_event(rdev_cb_tmp, fd);
            RS_PTHREAD_MUTEX_ULOCK(&rdev_cb_tmp->rdev_mutex);
            if (ret == 0) {
                return 0;
            }
    }
    return -ENODEV;
}

STATIC int rs_mr_info_sync(struct rs_mr_cb *mr_cb)
{
    int ret;

    hccp_info("mr state:%d, addr:0x%lx", mr_cb->state, mr_cb->mr_info.addr);

    CHK_PRT_RETURN(mr_cb->state & RS_MR_STATE_SYNCED, hccp_warn("mr synced ! mr_cb->flag[%d] & [%d] != 0",
        mr_cb->state, RS_MR_STATE_SYNCED), 0);

    /*
     * no socket available for MR_INFO exchange if allowed
     * need exchange when socket available!!!
     */
    CHK_PRT_RETURN(mr_cb->qp_cb->conn_info == NULL, hccp_warn("no conn available !"), 0);

    CHK_PRT_RETURN(mr_cb->qp_cb->state == RS_QP_STATUS_REM_FD_CLOSE, hccp_warn("remote qp fd closed,"
        "cann not use it anymore! status[%d](RS_QP_STATUS_REM_FD_CLOSE)", mr_cb->qp_cb->state), -EFAULT);

    CHK_PRT_RETURN(mr_cb->qp_cb->conn_info->connfd == RS_FD_INVALID, hccp_warn("rm info sync failed! fd not ready!"
        "connfd[%d](RS_FD_INVALID)", mr_cb->qp_cb->conn_info->connfd), -ENETUNREACH);

    mr_cb->mr_info.cmd = (unsigned int)RS_CMD_MR_INFO;
    ret = rs_socket_send(mr_cb->qp_cb->conn_info->connfd, &mr_cb->mr_info,
        sizeof(struct rs_mr_info));
    CHK_PRT_RETURN(ret != sizeof(struct rs_mr_info), hccp_err("mr_info send %d/%ld incomplete",
        ret, sizeof(struct rs_mr_info)), -EAGAIN);

    mr_cb->qp_cb->send_len += (uint32_t)ret;
    mr_cb->state |= RS_MR_STATE_SYNCED;
    hccp_info("after send mr state:%d, addr:0x%lx", mr_cb->state, mr_cb->mr_info.addr);

    return 0;
}

STATIC int rs_mr_pre_reg(unsigned int phy_id, struct rs_qp_cb *qp_cb, struct rs_mr_cb *mr_cb,
    struct rdma_mr_reg_info *mr_reg_info)
{
    struct roce_process_sign roceSign;
    int ret;
    unsigned int chip_id;
    char *addr = mr_reg_info->addr;
    unsigned long long len = mr_reg_info->len;
    int access = mr_reg_info->access;

    if (qp_cb->rdev_cb->rs_cb->hccp_mode == NETWORK_PEER_ONLINE || qp_cb->rdev_cb->rs_cb->hccp_mode == NETWORK_ONLINE ||
        qp_cb->is_exp == RS_NOT_EXP) {
        mr_cb->ib_mr = rs_drv_mr_reg(qp_cb->ib_pd, addr, len, access);
        CHK_PRT_RETURN(mr_cb->ib_mr == NULL, hccp_err("rs_drv_mr_reg addr is NULL len[%lld] failed ",
            len), -EACCES);
    } else {
        // reg mr with backup phy_id
        if (qp_cb->rdev_cb->backup_info.backup_flag) {
            phy_id = qp_cb->rdev_cb->backup_info.rdev_info.phy_id;
        }
        ret = rsGetLocalDevIDByHostDevID(phy_id, &chip_id);
        CHK_PRT_RETURN(ret, hccp_err("rsGetLocalDevIDByHostDevID failed, ret %d, phyid[%u]", ret, phy_id), -EACCES);
        roceSign.tgid = qp_cb->rdev_cb->rs_cb->p_rs_sign.tgid;
        roceSign.devid = chip_id;
        roceSign.vfid = 0;
        ret = strcpy_s(roceSign.sign, PROCESS_RS_SIGN_LENGTH, qp_cb->rdev_cb->rs_cb->p_rs_sign.sign);
        CHK_PRT_RETURN(ret, hccp_err("Invalid pid sign, ret(%d)", ret), -ESAFEFUNC);
        mr_cb->ib_mr = rs_drv_exp_mr_reg(qp_cb->ib_pd, addr, len, access, roceSign);
        CHK_PRT_RETURN(mr_cb->ib_mr == NULL, hccp_err("rs_drv_exp_mr_reg addr is NULL len[%lld] failed ",
            len), -EACCES);
    }

    mr_cb->mr_info.cmd = (unsigned int)RS_CMD_MR_INFO;
    mr_cb->mr_info.addr = (uintptr_t)addr;
    mr_cb->mr_info.len = len;
    mr_cb->mr_info.rkey = mr_cb->ib_mr->rkey;

    RS_PTHREAD_MUTEX_LOCK(&qp_cb->qp_mutex);
    rs_list_add_tail(&mr_cb->list, &qp_cb->mr_list);
    RS_PTHREAD_MUTEX_ULOCK(&qp_cb->qp_mutex);

    qp_cb->mr_num++;
    return 0;
}

STATIC int rs_calloc_mr(int num, struct rs_mr_cb **mr_cb)
{
    CHK_PRT_RETURN(num <= 0, hccp_err("invalid num for mr calloc"), -EINVAL);

    *mr_cb = calloc(num, sizeof(struct rs_mr_cb));
    CHK_PRT_RETURN((*mr_cb) == NULL, hccp_err("calloc mr_cb failed"), -ENOMEM);
    return 0;
}

STATIC int rs_calloc_qpcb(int num, struct rs_qp_cb **qp_cb)
{
    if (num <= 0) {
        return -EINVAL;
    }

    *qp_cb = calloc(num, sizeof(struct rs_qp_cb));
    if ((*qp_cb) == NULL) {
        return -ENOMEM;
    }

    return 0;
}

RS_ATTRI_VISI_DEF int rs_mr_reg(unsigned int phy_id, unsigned int rdev_index, unsigned int qpn,
    struct rdma_mr_reg_info *mr_reg_info)
{
    int ret;
    struct rs_qp_cb *qp_cb = NULL;
    struct rs_mr_cb *mr_cb = NULL;

    CHK_PRT_RETURN(mr_reg_info == NULL || mr_reg_info->addr == NULL || mr_reg_info->len == 0 ||
        phy_id >= RS_MAX_DEV_NUM, hccp_err("param err, NULL pointer or phy_id:%u >= [%d]", phy_id, RS_MAX_DEV_NUM),
        -EINVAL);

    hccp_info("qpn[%u], len[0x%llx], access[%d]",
        qpn, mr_reg_info->len, mr_reg_info->access);

    ret = rs_qpn2qpcb(phy_id, rdev_index, qpn, &qp_cb);
    CHK_PRT_RETURN(ret, hccp_err("rs_qpn2qpcb qpn[%d] ret[%d] failed ", qpn, ret), ret);

    CHK_PRT_RETURN(qp_cb->mr_num >= RS_MR_NUM_MAX, hccp_err("Exceeded the maximum MR limit %d",
        qp_cb->mr_num), -EINVAL);

    ret = rs_get_mrcb(qp_cb, (uintptr_t)mr_reg_info->addr, &mr_cb, &qp_cb->mr_list);
    if (!ret) {
        hccp_warn("mr already registered");
        goto found;
    }

    ret = rs_calloc_mr(1, &mr_cb);
    CHK_PRT_RETURN(ret, hccp_err("calloc mr failed"), ret);

    mr_cb->qp_cb = qp_cb;

    ret = rs_mr_pre_reg(phy_id, qp_cb, mr_cb, mr_reg_info);
    if (ret) {
        hccp_err("pre reg mr failed, qpn %u, ret %d", qpn, ret);
        goto reg_err;
    }

found:
    mr_reg_info->lkey = mr_cb->ib_mr->lkey;
    mr_reg_info->rkey = mr_cb->ib_mr->rkey;

    hccp_info("rs_mr_reg succ, state:%u", mr_cb->state);
    return 0;

reg_err:
    free(mr_cb);
    mr_cb = NULL;

    return ret;
}

RS_ATTRI_VISI_DEF int rs_mr_dereg(unsigned int phy_id, unsigned int rdev_index, unsigned int qpn, char *addr)
{
    int ret;
    struct rs_qp_cb *qp_cb = NULL;
    struct rs_mr_cb *mr_cb = NULL;

    hccp_dbg("start rs_mr_dereg");
    RS_CHECK_POINTER_NULL_RETURN_INT(addr);
    CHK_PRT_RETURN(phy_id >= RS_MAX_DEV_NUM, hccp_err("phy_id:%u >= [%d], is invalid", phy_id, RS_MAX_DEV_NUM),
        -EINVAL);

    ret = rs_qpn2qpcb(phy_id, rdev_index, qpn, &qp_cb);
    CHK_PRT_RETURN(ret, hccp_err("rs_qpn2qpcb failed ret[%d]", ret), ret);

    CHK_PRT_RETURN(rs_get_mrcb(qp_cb, (uintptr_t)addr, &mr_cb, &qp_cb->mr_list), hccp_err("rs_get_mrcb failed "\
        "g_rs_send_wr_num[%u]", g_rs_send_wr_num), -EFAULT);

    ret = rs_drv_mr_dereg(mr_cb->ib_mr);
    CHK_PRT_RETURN(ret, hccp_err("rs_drv_mr_dereg failed ret[%d] ", ret), -EACCES);

    RS_PTHREAD_MUTEX_LOCK(&qp_cb->qp_mutex);
    rs_list_del(&mr_cb->list);
    free(mr_cb);
    mr_cb = NULL;
    RS_PTHREAD_MUTEX_ULOCK(&qp_cb->qp_mutex);
    qp_cb->mr_num--;

    hccp_dbg("qpn[%u] succ", qpn);

    return 0;
}

RS_ATTRI_VISI_DEF int rs_register_mr(unsigned int phy_id, unsigned int rdev_index, struct rdma_mr_reg_info *mr_reg_info,
    void **mr_handle)
{
    RS_CHECK_POINTER_NULL_RETURN_INT(mr_handle);

    int ret;
    unsigned int chip_id;
    struct rs_rdev_cb *rdev_cb = NULL;
    struct ibv_mr *rs_mr_handle = NULL;

    CHK_PRT_RETURN(mr_reg_info == NULL || mr_reg_info->addr == NULL || mr_reg_info->len == 0 ||
        phy_id >= RS_MAX_DEV_NUM, hccp_err("param err, NULL pointer or phy_id:%u >= [%d]", phy_id,
        RS_MAX_DEV_NUM), -EINVAL);

    hccp_info("[rs_register_mr] len[0x%llx], access[%d]",
        mr_reg_info->len, mr_reg_info->access);

    ret = rsGetLocalDevIDByHostDevID(phy_id, &chip_id);
    CHK_PRT_RETURN(ret, hccp_err("rs_register_mr rsGetLocalDevIDByHostDevID phy_id[%u] invalid, ret %d",
        phy_id, ret), ret);

    ret = rs_rdev2rdev_cb(chip_id, rdev_index, &rdev_cb);
    CHK_PRT_RETURN(ret || rdev_cb == NULL, hccp_err("rs_rdev2rdev_cb for chip_id[%u] failed, ret %d",
        chip_id, ret), ret);

    *mr_handle = (void *)rs_drv_mr_reg(rdev_cb->ib_pd, mr_reg_info->addr, mr_reg_info->len, mr_reg_info->access);
    if (*mr_handle == NULL) {
        hccp_warn("rs_drv_mr_reg addr is NULL len[%lld] access[%d] unsuccessful ", mr_reg_info->len,
            mr_reg_info->access);
        goto reg_err;
    }

    rs_mr_handle = (struct ibv_mr *)*mr_handle;
    mr_reg_info->lkey = rs_mr_handle->lkey;
    mr_reg_info->rkey = rs_mr_handle->rkey;

    hccp_info("rs_register_mr succ");
    return ret;
reg_err:
    mr_reg_info->lkey = 0;

    return 0;
}

STATIC int rs_init_typical_mr_cb(unsigned int phy_id, struct rdma_mr_reg_info *mr_reg_info, struct rs_rdev_cb *dev_cb,
                                 struct rs_mr_cb *mr_cb)
{
    unsigned long long len = mr_reg_info->len;
    char *addr = (char *)mr_reg_info->addr;
    int access = mr_reg_info->access;
    struct roce_process_sign roceSign;
    unsigned int chip_id;
    int ret;

    if (dev_cb->rs_cb->hccp_mode == NETWORK_PEER_ONLINE || dev_cb->rs_cb->hccp_mode == NETWORK_ONLINE) {
        mr_cb->ib_mr = rs_drv_mr_reg(dev_cb->ib_pd, addr, len, access);
        CHK_PRT_RETURN(mr_cb->ib_mr == NULL, hccp_err("rs_drv_mr_reg addr is NULL len[%lld] failed", len), -EACCES);
    } else {
        // reg mr with backup phy_id
        if (dev_cb->backup_info.backup_flag) {
            phy_id = dev_cb->backup_info.rdev_info.phy_id;
        }
        ret = rsGetLocalDevIDByHostDevID(phy_id, &chip_id);
        CHK_PRT_RETURN(ret, hccp_err("rsGetLocalDevIDByHostDevID failed, ret %d, phyid[%u]", ret, phy_id), -EACCES);
        roceSign.tgid = dev_cb->rs_cb->p_rs_sign.tgid;
        roceSign.devid = chip_id;
        roceSign.vfid = 0;
        ret = strcpy_s(roceSign.sign, PROCESS_RS_SIGN_LENGTH, dev_cb->rs_cb->p_rs_sign.sign);
        CHK_PRT_RETURN(ret, hccp_err("Invalid pid sign, ret(%d)", ret), -ESAFEFUNC);
        mr_cb->ib_mr = rs_drv_exp_mr_reg(dev_cb->ib_pd, addr, len, access, roceSign);
        CHK_PRT_RETURN(mr_cb->ib_mr == NULL, hccp_err("rs_drv_exp_mr_reg addr is NULL len[%lld] failed", len), -EACCES);
    }

    mr_cb->mr_info.addr = (uintptr_t)addr;
    mr_cb->mr_info.len = len;
    mr_cb->mr_info.rkey = mr_cb->ib_mr->rkey;

    RS_PTHREAD_MUTEX_LOCK(&dev_cb->rdev_mutex);
    rs_list_add_tail(&mr_cb->list, &dev_cb->typical_mr_list);
    RS_PTHREAD_MUTEX_ULOCK(&dev_cb->rdev_mutex);

    return 0;
}

RS_ATTRI_VISI_DEF int rs_typical_register_mr_v1(unsigned int phy_id, unsigned int rdev_index,
    struct rdma_mr_reg_info *mr_reg_info, void **mr_handle)
{
    RS_CHECK_POINTER_NULL_RETURN_INT(mr_handle);

    struct rs_mr_cb *typical_mr_cb = NULL;
    struct rs_rdev_cb *rdev_cb = NULL;
    unsigned int chip_id;
    int ret;

    CHK_PRT_RETURN(mr_reg_info == NULL || mr_reg_info->addr == NULL || mr_reg_info->len == 0 ||
        phy_id >= RS_MAX_DEV_NUM, hccp_err("param err, NULL pointer or phy_id:%u >= [%d]", phy_id,
        RS_MAX_DEV_NUM), -EINVAL);

    hccp_info("[rs_typical_register_mr] len[0x%llx], access[%d]",
        mr_reg_info->len, mr_reg_info->access);

    ret = rsGetLocalDevIDByHostDevID(phy_id, &chip_id);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_typical_register_mr rsGetLocalDevIDByHostDevID phy_id[%u] invalid, ret %d",
        phy_id, ret), ret);

    ret = rs_rdev2rdev_cb(chip_id, rdev_index, &rdev_cb);
    CHK_PRT_RETURN(ret != 0 || rdev_cb == NULL, hccp_err("rs_rdev2rdev_cb for chip_id[%u] failed, ret %d",
        chip_id, ret), ret);

    ret = rs_query_mr_cb(rdev_cb, (uint64_t)(uintptr_t)mr_reg_info->addr, &typical_mr_cb, &rdev_cb->typical_mr_list);
    if (ret == 0) {
        hccp_warn("typical mr already registered");
        goto found;
    }

    typical_mr_cb = calloc(1, sizeof(struct rs_mr_cb));
    CHK_PRT_RETURN(typical_mr_cb == NULL, hccp_err("calloc typical_mr_cb failed"), -ENOMEM);
    typical_mr_cb->dev_cb = rdev_cb;

    ret = rs_init_typical_mr_cb(phy_id, mr_reg_info, rdev_cb, typical_mr_cb);
    if (ret != 0) {
        hccp_err("rs_init_typical_mr_cb failed, dev_index[%u], ret[%d]", rdev_index, ret);
        goto reg_err;
    }

found:
    *mr_handle = typical_mr_cb->ib_mr;
    mr_reg_info->lkey = typical_mr_cb->ib_mr->lkey;
    mr_reg_info->rkey = typical_mr_cb->ib_mr->rkey;
    hccp_info("rs_typical_register_mr succ, state:%d", typical_mr_cb->state);
    return 0;

reg_err:
    free(typical_mr_cb);
    typical_mr_cb = NULL;
    return ret;
}

RS_ATTRI_VISI_DEF int rs_typical_register_mr(unsigned int phy_id, unsigned int rdev_index,
    struct rdma_mr_reg_info *mr_reg_info, void **mr_handle)
{
    RS_CHECK_POINTER_NULL_RETURN_INT(mr_handle);

    struct rs_mr_cb *typical_mr_cb = NULL;
    struct rs_rdev_cb *rdev_cb = NULL;
    unsigned int chip_id;
    int ret;

    CHK_PRT_RETURN(mr_reg_info == NULL || mr_reg_info->addr == NULL || mr_reg_info->len == 0 ||
        phy_id >= RS_MAX_DEV_NUM, hccp_err("param err, NULL pointer or phy_id:%u >= [%d]", phy_id,
        RS_MAX_DEV_NUM), -EINVAL);

    hccp_info("start register len[0x%llx], access[%d]", mr_reg_info->len, mr_reg_info->access);

    ret = rsGetLocalDevIDByHostDevID(phy_id, &chip_id);
    CHK_PRT_RETURN(ret != 0, hccp_err("rsGetLocalDevIDByHostDevID phy_id[%u] invalid, ret %d", phy_id, ret), ret);

    ret = rs_rdev2rdev_cb(chip_id, rdev_index, &rdev_cb);
    CHK_PRT_RETURN(ret != 0 || rdev_cb == NULL, hccp_err("rs_rdev2rdev_cb for chip_id[%u] failed, ret %d",
        chip_id, ret), ret);

    typical_mr_cb = calloc(1, sizeof(struct rs_mr_cb));
    CHK_PRT_RETURN(typical_mr_cb == NULL, hccp_err("calloc typical_mr_cb failed"), -ENOMEM);
    typical_mr_cb->dev_cb = rdev_cb;

    ret = rs_init_typical_mr_cb(phy_id, mr_reg_info, rdev_cb, typical_mr_cb);
    if (ret != 0) {
        hccp_err("rs_init_typical_mr_cb failed, dev_index[%u], ret[%d]", rdev_index, ret);
        goto reg_err;
    }

    // resv len as 1 to save addr for later unreg to query
    typical_mr_cb->mr_info.addr = (uint64_t)(uintptr_t)typical_mr_cb->ib_mr;
    typical_mr_cb->mr_info.len = 1U;
    *mr_handle = typical_mr_cb->ib_mr;
    mr_reg_info->lkey = typical_mr_cb->ib_mr->lkey;
    mr_reg_info->rkey = typical_mr_cb->ib_mr->rkey;
    hccp_info("register succ, state:%d", typical_mr_cb->state);
    return 0;

reg_err:
    free(typical_mr_cb);
    typical_mr_cb = NULL;
    return ret;
}

RS_ATTRI_VISI_DEF int rs_remap_mr(unsigned int phy_id, unsigned int rdev_index, struct mem_remap_info mem_list[],
    unsigned int mem_num)
{
    struct rs_rdev_cb *dev_cb = NULL;
    struct rs_mr_cb *mr_curr = NULL;
    struct rs_mr_cb *mr_next = NULL;
    unsigned long long addr = 0;
    bool is_mem_matched = false;
    unsigned int chip_id;
    unsigned int i;
    int ret;

    CHK_PRT_RETURN(phy_id >= RS_MAX_DEV_NUM, hccp_err("phy_id:%u >= %d, is invalid", phy_id, RS_MAX_DEV_NUM), -EINVAL);

    ret = rsGetLocalDevIDByHostDevID(phy_id, &chip_id);
    CHK_PRT_RETURN(ret, hccp_err("rsGetLocalDevIDByHostDevID failed, phy_id:%u invalid, ret:%d", phy_id, ret), ret);

    ret = rs_rdev2rdev_cb(chip_id, rdev_index, &dev_cb);
    CHK_PRT_RETURN(dev_cb == NULL, hccp_err("rs_rdev2rdev_cb failed, chip_id:%u, ret:%d", chip_id, ret), -ENODEV);

    for (i = 0; i < mem_num; i++) {
        is_mem_matched = false;
        addr = (uint64_t)(uintptr_t)mem_list[i].addr;
        RS_PTHREAD_MUTEX_LOCK(&dev_cb->rdev_mutex);
        RS_LIST_GET_HEAD_ENTRY(mr_curr, mr_next, &dev_cb->typical_mr_list, list, struct rs_mr_cb);
        for (; (&mr_curr->list) != &dev_cb->typical_mr_list;
            mr_curr = mr_next, mr_next = list_entry(mr_next->list.next, struct rs_mr_cb, list)) {
            // mem is out range of mr, continue to find next matching mr
            if ((addr < (uint64_t)(uintptr_t)mr_curr->ib_mr->addr) ||
                (mem_list[i].size > mr_curr->ib_mr->length) ||
                (addr + mem_list[i].size < addr) ||
                (addr + mem_list[i].size > (uint64_t)(uintptr_t)mr_curr->ib_mr->addr + mr_curr->ib_mr->length)) {
                continue;
            }

            // each mr remap each corresponding mem
            ret = rs_roce_remap_mr(mr_curr->ib_mr, (struct hns_roce_mr_remap_info *)&mem_list[i], 1);
            if (ret != 0) {
                hccp_err("remap %u-th mem failed, ret:%d addr:0x%llx size:0x%llx", i, ret, addr, mem_list[i].size);
                RS_PTHREAD_MUTEX_ULOCK(&dev_cb->rdev_mutex);
                return ret;
            }
            is_mem_matched = true;
        }
        RS_PTHREAD_MUTEX_ULOCK(&dev_cb->rdev_mutex);

        if (!is_mem_matched) {
            hccp_err("find %u-th mem failed, addr:0x%llx size:0x%llx", i, addr, mem_list[i].size);
            return -ENODEV;
        }
        hccp_dbg("remap %u-th mem success, addr:0x%llx size:0x%llx", i, addr, mem_list[i].size);
    }

    return 0;
}

RS_ATTRI_VISI_DEF int rs_typical_deregister_mr(unsigned int phy_id, unsigned int dev_index, unsigned long long addr)
{
    struct rs_mr_cb *typical_mr_cb = NULL;
    struct rs_rdev_cb *dev_cb = NULL;
    unsigned int chip_id;
    int ret;

    hccp_info("typical mr unreg start, addr[%llu]", addr);
    CHK_PRT_RETURN(phy_id >= RS_MAX_DEV_NUM, hccp_err("phy_id:%u >= %d, is invalid", phy_id, RS_MAX_DEV_NUM),
        -EINVAL);

    ret = rsGetLocalDevIDByHostDevID(phy_id, &chip_id);
    CHK_PRT_RETURN(ret, hccp_err("phy_id[%u] invalid, ret %d", phy_id, ret), ret);

    ret = rs_rdev2rdev_cb(chip_id, dev_index, &dev_cb);
    CHK_PRT_RETURN(ret != 0 || dev_cb == NULL, hccp_err("rs_rdev2rdev_cb get dev_cb failed for chip_id[%u], ret[%d]",
        chip_id, ret), -ENODEV);

    ret = rs_query_mr_cb(dev_cb, addr, &typical_mr_cb, &dev_cb->typical_mr_list);
    CHK_PRT_RETURN(ret, hccp_err("rs_query_mr_cb failed ret[%d]", ret), ret);

    ret = rs_drv_mr_dereg(typical_mr_cb->ib_mr);
    CHK_PRT_RETURN(ret, hccp_err("rs_drv_mr_dereg failed ret[%d]", ret), -EACCES);

    RS_PTHREAD_MUTEX_LOCK(&dev_cb->rdev_mutex);
    rs_list_del(&typical_mr_cb->list);
    free(typical_mr_cb);
    typical_mr_cb = NULL;
    RS_PTHREAD_MUTEX_ULOCK(&dev_cb->rdev_mutex);

    hccp_info("dev_index[%u] succ", dev_index);

    return 0;
}

RS_ATTRI_VISI_DEF int rs_deregister_mr(void *mr_handle)
{
    RS_CHECK_POINTER_NULL_RETURN_INT(mr_handle);

    int ret;
    struct ibv_mr *rs_mr_handle = (struct ibv_mr *)mr_handle;

    ret = rs_drv_mr_dereg(rs_mr_handle);
    CHK_PRT_RETURN(ret, hccp_err("rs_drv_mr_dereg failed ret[%d] ", ret), -EACCES);

    hccp_info("rs_deregister_mr succ");
    return 0;
}

RS_ATTRI_VISI_DEF int rs_send_wr(unsigned int phy_id, unsigned int rdev_index, uint32_t qpn, struct send_wr *wr,
    struct send_wr_rsp *wr_rsp)
{
    int ret;
    struct rs_qp_cb *qp_cb = NULL;
    struct rs_mr_cb *mr_cb = NULL;
    struct rs_mr_cb *rem_mr_cb = NULL;

    RS_CHECK_POINTER_NULL_RETURN_INT(wr);
    RS_CHECK_POINTER_NULL_RETURN_INT(wr->buf_list);
    RS_CHECK_POINTER_NULL_RETURN_INT(wr_rsp);

    CHK_PRT_RETURN(phy_id >= RS_MAX_DEV_NUM, hccp_err("phy_id:%u >= [%d], is invalid",
        phy_id, RS_MAX_DEV_NUM), -EINVAL);

    CHK_PRT_RETURN(wr->buf_num > MAX_SGE_NUM || wr->buf_num == 0, hccp_err("invalid buf_num[%u]!",
        wr->buf_num), -EINVAL);

    CHK_PRT_RETURN(wr->buf_list->len > RS_SGLIST_LEN_MAX || wr->buf_list->len == 0, hccp_err("sg list"
        "len is more than 2G, len[%u]", wr->buf_list->len), -EINVAL);

    if (rs_qpn2qpcb(phy_id, rdev_index, qpn, &qp_cb)) {
        return -EACCES;
    }

    qp_cb->send_wr_num++;

    hccp_info("qpn %d, buf_list[0].addr is 0x%llx", qpn, wr->buf_list[0].addr);
    if (rs_get_mrcb(qp_cb, wr->buf_list[0].addr, &mr_cb, &qp_cb->mr_list)) {
        hccp_err("qpn %d, buf_list[0].addr[0x%llx] len[0x%x] is invalid.", qpn, wr->buf_list[0].addr,
            wr->buf_list[0].len);
        return -EFAULT;
    }

    // send op no need to check & get remote mr
    if (wr->op != RA_WR_SEND && wr->op != RA_WR_SEND_WITH_IMM) {
        hccp_info("remote wr dst addr is 0x%llx", wr->dst_addr);
        if (rs_get_mrcb(qp_cb, wr->dst_addr, &rem_mr_cb, &qp_cb->rem_mr_list)) {
            hccp_err("qpn %d, remote wr dst addr[0x%llx] len[0x%x] is invalid.", qpn, wr->dst_addr,
                wr->buf_list[0].len);
            return -ENOENT;
        }
    }

    ret = rs_drv_send_exp(qp_cb, mr_cb, rem_mr_cb, wr, wr_rsp);
    if (ret) {
        hccp_err("send exp failed qpn %u, ret %d", qpn, ret);
    }
    g_rs_send_wr_num++;
    return ret;
}

STATIC void build_up_wr_with_key(struct wr_info *wr, struct ibv_sge *list, struct ibv_send_wr *ib_wr)
{
    list->addr = (uintptr_t)wr->mem_list.addr;
    list->length = wr->mem_list.len;
    list->lkey = wr->mem_list.lkey;

    ib_wr->sg_list = list;
    ib_wr->opcode = wr->op;
    ib_wr->send_flags = (unsigned int)wr->send_flags;
    ib_wr->imm_data = htobe32(wr->imm_data);

    ib_wr->num_sge = 1; /* only support one sge */
    ib_wr->wr_id = wr->wr_id;
    if (wr->op != IBV_WR_SEND && wr->op != IBV_WR_SEND_WITH_IMM) {
        ib_wr->wr.rdma.rkey = wr->rkey;
        ib_wr->wr.rdma.remote_addr = wr->dst_addr;
    }
}

STATIC void rs_send_build_up_wr(struct rs_mr_cb *mr_cb, struct wr_info *wr, struct ibv_sge *list,
    struct ibv_send_wr *ib_wr)
{
    list->addr = (uintptr_t)wr->mem_list.addr;
    list->lkey =  mr_cb->ib_mr->lkey;
    list->length = wr->mem_list.len;

    ib_wr->sg_list = list;
    ib_wr->opcode = wr->op;
    ib_wr->imm_data = htobe32(wr->imm_data);
    ib_wr->send_flags = (unsigned int)wr->send_flags;

    ib_wr->num_sge = 1; /* only support one sge */
    ib_wr->wr_id = wr->wr_id;
}

STATIC void rs_wirte_and_read_build_up_wr(struct rs_mr_cb *mr_cb, struct rs_mr_cb *rem_mr_cb,
    struct wr_info *wr, struct ibv_sge *list, struct ibv_send_wr *ib_wr)
{
    list->addr = (uintptr_t)wr->mem_list.addr;
    list->length = wr->mem_list.len;
    list->lkey =  mr_cb->ib_mr->lkey;

    ib_wr->sg_list = list;
    ib_wr->opcode = wr->op;
    ib_wr->send_flags = (unsigned int)wr->send_flags;
    ib_wr->imm_data = htobe32(wr->imm_data);

    ib_wr->num_sge = 1; /* only support one sge */
    ib_wr->wr_id = wr->wr_id;
    ib_wr->wr.rdma.rkey = rem_mr_cb->mr_info.rkey;
    ib_wr->wr.rdma.remote_addr = wr->dst_addr;
}

STATIC int rs_build_up_wr_list(struct wr_info *wr_list, struct rs_qp_cb *qp_cb, struct ibv_sge *list,
    struct ibv_send_wr *ib_wr, unsigned int i)
{
    struct rs_mr_cb *mr_cb = NULL;
    struct rs_mr_cb *rem_mr_cb = NULL;
    CHK_PRT_RETURN(wr_list[i].mem_list.len > RS_SGLIST_LEN_MAX, hccp_err("sg list len is more than 2G, len[%u]",
        wr_list[i].mem_list.len), -EINVAL);

    hccp_dbg("qpn %d, buf_list[0].addr is 0x%llx", qp_cb->ib_qp->qp_num, wr_list[i].mem_list.addr);
    if (rs_get_mrcb(qp_cb, wr_list[i].mem_list.addr, &mr_cb, &qp_cb->mr_list)) {
        hccp_err("qpn %d, buf_list[0].addr[0x%llx] len[0x%x] is invalid.", qp_cb->ib_qp->qp_num,
            wr_list[i].mem_list.addr, wr_list[i].mem_list.len);
        return -EFAULT;
    }

    // send op no need to check & get remote mr
    if (wr_list[i].op != IBV_WR_SEND && wr_list[i].op != IBV_WR_SEND_WITH_IMM) {
        hccp_dbg("remote wr dst addr is 0x%llx", wr_list[i].dst_addr);
        if (rs_get_mrcb(qp_cb, wr_list[i].dst_addr, &rem_mr_cb, &qp_cb->rem_mr_list)) {
            hccp_err("qpn %d, remote wr dst addr[0x%llx] len[0x%x] is invalid.", qp_cb->ib_qp->qp_num,
                wr_list[i].dst_addr, wr_list[i].mem_list.len);
            return -ENOENT;
        }
        rs_wirte_and_read_build_up_wr(mr_cb, rem_mr_cb, &wr_list[i], &list[i], &ib_wr[i]);
    } else {
        rs_send_build_up_wr(mr_cb, &wr_list[i], &list[i], &ib_wr[i]);
    }

    return 0;
}

STATIC int rs_build_up_wr_list_with_key(struct wr_info *wr_list, struct ibv_sge *list,
    struct ibv_send_wr *ib_wr, unsigned int i)
{
    CHK_PRT_RETURN(wr_list[i].mem_list.len > RS_SGLIST_LEN_MAX, hccp_err("sg list len is more than 2G, len[%u]",
        wr_list[i].mem_list.len), -EINVAL);

    build_up_wr_with_key(&wr_list[i], &list[i], &ib_wr[i]);
    return 0;
}

STATIC int rs_send_normal_wrlist(struct rs_qp_cb *qp_cb, struct wr_info *wr_list,
    unsigned int send_num, unsigned int *complete_num, unsigned int key_flag)
{
    int ret;
    unsigned int i, j;

    struct ibv_send_wr *bad_wr = NULL;
    CHK_PRT_RETURN(send_num > MAX_WR_NUM || send_num == 0, hccp_err("send num[%u] is invalid!", send_num), -EINVAL);
    struct ibv_send_wr *ib_wr = (struct ibv_send_wr *)calloc(send_num, sizeof(struct ibv_send_wr));
    CHK_PRT_RETURN(ib_wr == NULL, hccp_err("calloc ib_wr failed!"), -ENOSPC);

    struct ibv_sge *list = (struct ibv_sge *)calloc(send_num, sizeof(struct ibv_sge));
    if (list == NULL) {
        hccp_err("calloc list failed!");
        ret = -ENOSPC;
        goto alloc_fail;
    }

    for (i = 0; i < send_num; i++) {
        ret = (key_flag == 0) ? rs_build_up_wr_list(wr_list, qp_cb, list, ib_wr, i) :
            rs_build_up_wr_list_with_key(wr_list, list, ib_wr, i);
        if (ret) {
            goto input_err;
        }
        j = i + 1;
        ib_wr[i].next = (i < send_num - 1) ? &ib_wr[j] : NULL;
    }

    ret = rs_ibv_post_send(qp_cb->ib_qp, &ib_wr[0], &bad_wr);
    if (ret == 0) {
        *complete_num = send_num;
    } else if (ret == -ENOMEM) {
        *complete_num = (unsigned int)((void *)bad_wr - (void *)ib_wr) / sizeof(struct ibv_send_wr);
        hccp_dbg("post send wqe overflow, complete_num[%d]", *complete_num);
    } else {
        hccp_err("ibv_post_send failed, ret[%d]", ret);
        *complete_num = 0;
    }
    qp_cb->send_wr_num = qp_cb->send_wr_num + (*complete_num);

input_err:
    free(list);
    list = NULL;
alloc_fail:
    free(ib_wr);
    ib_wr = NULL;
    return (ret == -ENOMEM) ? 0 : ret;
}

STATIC int rs_send_exp_wrlist(struct rs_qp_cb *qp_cb, struct wr_info *wr_list, unsigned int send_num,
    struct send_wr_rsp *wr_rsp, unsigned int *complete_num, unsigned int key_flag)
{
    struct ibv_post_send_ext_attr ext_attr = {0};
    struct ibv_post_send_ext_resp ext_rsp = {0};
    struct ibv_send_wr *bad_wr = NULL;
    struct wr_exp_rsp exp_rsp = {0};
    struct ibv_send_wr ib_wr = {0};
    struct ibv_sge list = {0};
    unsigned int i;
    int ret = 0;

    for (i = 0; i < send_num; i++) {
        // reuse code: only need to build up one wr once a time
        ret = (key_flag == 0) ? rs_build_up_wr_list(&wr_list[i], qp_cb, &list, &ib_wr, 0) :
            rs_build_up_wr_list_with_key(&wr_list[i], &list, &ib_wr, 0);
        if (ret != 0) {
            hccp_err("qpn:%u key_flag:%u build_up_wr i:%u failed, ret:%d", qp_cb->ib_qp->qp_num, key_flag, i, ret);
            break;
        }

        if (wr_list[i].op == RA_WR_RDMA_WRITE_WITH_NOTIFY ||
            wr_list[i].op == RA_WR_RDMA_REDUCE_WRITE ||
            wr_list[i].op == RA_WR_RDMA_REDUCE_WRITE_WITH_NOTIFY) {
            ib_wr.imm_data = htobe32((wr_list[i].aux.notify_offset & WRITE_NOTIFY_OFFSET_MASK) |
                WRITE_NOTIFY_VALUE_RECORD);
            ext_attr.reduce_op = wr_list[i].aux.reduce_type;
            ext_attr.reduce_type = wr_list[i].aux.data_type;
            ret = rs_ibv_ext_post_send(qp_cb->ib_qp, &ib_wr, &bad_wr, &ext_attr, &ext_rsp);
            exp_rsp.wqe_index = ext_rsp.wqe_index;
            exp_rsp.db_info = ext_rsp.db_info;
            hccp_dbg("rs_ibv_ext_post_send, op = [%x], imm_data = [0x%lx], reduce_op = [%d],reduce_type = [%d]",
                     ib_wr.opcode, ib_wr.imm_data, ext_attr.reduce_op, ext_attr.reduce_type);
        } else {
            ret = rs_ibv_exp_post_send(qp_cb->ib_qp, &ib_wr, &bad_wr, &exp_rsp);
            hccp_dbg("rs_ibv_exp_post_send, op = [%x], remote_addr = [0x%llx], size = [%d]",
                     ib_wr.opcode, ib_wr.wr.rdma.remote_addr, ib_wr.sg_list->length);
        }

        if (ret != 0) {
            if (ret == -ENOMEM) {
                hccp_warn("qpn:%u rs_ibv_exp_post_send i:%u unsuccessful, ret %d", qp_cb->ib_qp->qp_num, i, ret);
            } else {
                hccp_err("qpn:%u rs_ibv_exp_post_send i:%u failed, ret %d", qp_cb->ib_qp->qp_num, i, ret);
            }
            break;
        }

        qp_cb->send_wr_num++;

        if (qp_cb->qp_mode == RA_RS_GDR_TMPL_QP_MODE) {
            wr_rsp[i].wqe_tmp.sq_index = (unsigned int)qp_cb->sq_index;
            wr_rsp[i].wqe_tmp.wqe_index = exp_rsp.wqe_index;
        } else if (qp_cb->qp_mode == RA_RS_OP_QP_MODE ||
                   qp_cb->qp_mode == RA_RS_GDR_ASYN_QP_MODE) {
            wr_rsp[i].db.db_index = (unsigned int)qp_cb->db_index;
            wr_rsp[i].db.db_info = exp_rsp.db_info;
        }
    }

    hccp_dbg("complete_num[%d], ret[%d]", i, ret);
    *complete_num = i;
    return (ret == -ENOMEM) ? 0 : ret;
}

RS_ATTRI_VISI_DEF int rs_send_wrlist(struct rs_wrlist_base_info base_info, struct wr_info *wr_list,
    unsigned int send_num, struct send_wr_rsp *wr_rsp, unsigned int *complete_num)
{
    int ret;
    unsigned int phy_id, rdev_index, qpn;
    struct rs_qp_cb *qp_cb = NULL;

    RS_CHECK_POINTER_NULL_RETURN_INT(wr_list);
    RS_CHECK_POINTER_NULL_RETURN_INT(wr_rsp);
    CHK_PRT_RETURN(send_num > MAX_WR_NUM || send_num == 0 || base_info.phy_id >= RS_MAX_DEV_NUM,
        hccp_err("send_num[%u] or phy_id:%u >= [%d], is invalid", send_num, base_info.phy_id, RS_MAX_DEV_NUM),
        -EINVAL);

    phy_id = base_info.phy_id;
    rdev_index = base_info.rdev_index;
    qpn = base_info.qpn;

    CHK_PRT_RETURN(rs_qpn2qpcb(phy_id, rdev_index, qpn, &qp_cb), hccp_err("rs_qpn2qpcb failed, physical id[%u]",
        phy_id), -EACCES);

    // only allow normal qp to call this func when ai_op_support not set
    if (qp_cb->qp_mode == RA_RS_NOR_QP_MODE && qp_cb->ai_op_support == 0) {
        ret = rs_send_normal_wrlist(qp_cb, wr_list, send_num, complete_num, base_info.key_flag);
    } else {
        ret = rs_send_exp_wrlist(qp_cb, wr_list, send_num, wr_rsp, complete_num, base_info.key_flag);
    }
    return ret;
}

RS_ATTRI_VISI_DEF int rs_recv_wrlist(struct rs_wrlist_base_info base_info, struct recv_wrlist_data *wr,
    unsigned int recv_num, unsigned int *complete_num)
{
    struct rs_qp_cb *qp_cb = NULL;

    RS_CHECK_POINTER_NULL_RETURN_INT(wr);
    CHK_PRT_RETURN(recv_num > MAX_WR_NUM || recv_num == 0 || base_info.phy_id >= RS_MAX_DEV_NUM,
        hccp_err("recv_num[%u] or phy_id:%u >= [%d], is invalid", recv_num, base_info.phy_id, RS_MAX_DEV_NUM),
        -EINVAL);

    CHK_PRT_RETURN(rs_qpn2qpcb(base_info.phy_id, base_info.rdev_index, base_info.qpn, &qp_cb),
        hccp_err("rs_qpn2qpcb failed, physical id[%u]",  base_info.phy_id), -EACCES);

    return rs_drv_post_recv(qp_cb, wr, recv_num, complete_num);
}

RS_ATTRI_VISI_DEF int rs_set_host_pid(uint32_t phy_id, pid_t host_pid, const char *pid_sign)
{
    int ret;
    unsigned int chip_id;
    struct rs_cb *rs_cb = NULL;

    RS_CHECK_POINTER_NULL_RETURN_INT(pid_sign);
    CHK_PRT_RETURN(phy_id >= RS_MAX_DEV_NUM, hccp_err("rs_set_host_pid rs set param error ! phy_id:%u",
        phy_id), -EINVAL);

    ret = rsGetLocalDevIDByHostDevID(phy_id, &chip_id);
    CHK_PRT_RETURN(ret, hccp_err("rs_set_host_pid rsGetLocalDevIDByHostDevID phy_id invalid, ret %d", ret), ret);

    hccp_info("phy_id[%u] host_pid[%d]", chip_id, host_pid);

    ret = rs_dev2rscb(chip_id, &rs_cb, false);
    CHK_PRT_RETURN(ret, hccp_err("get rs cb failed, chip_id:%u", chip_id), ret);

    rs_cb->p_rs_sign.tgid = host_pid;
    ret = strcpy_s(rs_cb->p_rs_sign.sign, PROCESS_RS_SIGN_LENGTH, pid_sign);
    CHK_PRT_RETURN(ret, hccp_err("copy sign failed, ret %d", ret), -ESAFEFUNC);

    return 0;
}

RS_ATTRI_VISI_DEF int rs_rdev_get_port_status(unsigned int phy_id, unsigned int rdev_index, enum port_status *status)
{
    struct ibv_port_attr port_attr = { 0 };
    struct rs_rdev_cb *rdev_cb = NULL;
    unsigned int chip_id;
    int ret;

    CHK_PRT_RETURN(phy_id >= RS_MAX_DEV_NUM, hccp_err("phy_id:%u >= [%d], is invalid",
        phy_id, RS_MAX_DEV_NUM), -EINVAL);
    CHK_PRT_RETURN(status == NULL, hccp_err("param err! status is NULL"), -EINVAL);

    ret = rsGetLocalDevIDByHostDevID(phy_id, &chip_id);
    CHK_PRT_RETURN(ret, hccp_err("rsGetLocalDevIDByHostDevID failed, phy_id[%u] invalid, ret %d", phy_id, ret), ret);

    ret = rs_rdev2rdev_cb(chip_id, rdev_index, &rdev_cb);
    CHK_PRT_RETURN(ret != 0 || rdev_cb == NULL, hccp_err("rs_rdev2rdev_cb for chip_id[%u] failed, ret %d",
        chip_id, ret), ret);

    ret = rs_ibv_query_port(rdev_cb->ib_ctx, rdev_cb->ib_port, &port_attr);
    CHK_PRT_RETURN(ret, hccp_err("ibv_query_port failed ret[%d]", ret), -EOPENSRC);

    *status = port_attr.state == IBV_PORT_ACTIVE ? PORT_STATUS_ACTIVE : PORT_STATUS_DOWN;

    hccp_dbg("phy_id:%u port_attr.state:%u status:%u", phy_id, port_attr.state, *status);
    return 0;
}

RS_ATTRI_VISI_DEF int rs_get_notify_mr_info(unsigned int phy_id, unsigned int rdev_index, struct mr_info *info)
{
    struct rs_rdev_cb *rdev_cb = NULL;
    struct rs_cb *rs_cb = NULL;
    unsigned int chip_id;
    int ret;

    CHK_PRT_RETURN(phy_id >= RS_MAX_DEV_NUM, hccp_err("phy_id:%u >= [%d], is invalid",
        phy_id, RS_MAX_DEV_NUM), -EINVAL);

    CHK_PRT_RETURN(info == NULL, hccp_err("param err! info is NULL"), -EINVAL);

    ret = rsGetLocalDevIDByHostDevID(phy_id, &chip_id);
    CHK_PRT_RETURN(ret, hccp_err("phy_id[%u] invalid, ret:%d", phy_id, ret), ret);

    ret = rs_dev2rscb(chip_id, &rs_cb, false);
    CHK_PRT_RETURN(ret, hccp_err("get rs_cb failed, ret:%d", ret), -ENODEV);

    ret = rs_get_rdev_cb(rs_cb, rdev_index, &rdev_cb);
    CHK_PRT_RETURN(ret, hccp_err("rs_get_rdev_cb failed!, ret:%d, rdev_index:%u", ret, rdev_index), ret);

    info->addr = (void *)(uintptr_t)rdev_cb->notify_va_base;
    info->size = rdev_cb->notify_size;
    info->access = rdev_cb->notify_access;
    info->lkey = rdev_cb->notify_mr->lkey;

    return 0;
}

RS_ATTRI_VISI_DEF int rs_notify_cfg_set(unsigned int phy_id, unsigned long long va, unsigned long long size)
{
    int ret;
    unsigned int chip_id;
    struct rs_cb *rs_cb = NULL;

    RS_CHECK_POINTER_NULL_RETURN_INT((void *)(uintptr_t)va);

    CHK_PRT_RETURN(phy_id >= RS_MAX_DEV_NUM ||
        (size != MAX_NOTIFY_SIZE_CLOUD && size != NOTIFY_NUM_MAX_V2 && size != NOTIFY_NUM_MAX_V3),
        hccp_err("rs_notify_cfg_set rs set param error ! phy_id[%u] size[%llu]", phy_id, size), -EINVAL);

    ret = rsGetLocalDevIDByHostDevID(phy_id, &chip_id);
    CHK_PRT_RETURN(ret, hccp_err("rs_notify_cfg_set phy_id invalid, ret %d, phy_id:%u", ret, phy_id), ret);

    ret = rs_dev2rscb(chip_id, &rs_cb, false);
    CHK_PRT_RETURN(ret, hccp_err("get rs cb failed, chip_id:%u", chip_id), ret);

    rs_cb->notify_va_base = va;
    rs_cb->notify_size = size;

    return 0;
}

RS_ATTRI_VISI_DEF int rs_notify_cfg_get(unsigned int phy_id, unsigned long long *va, unsigned long long *size)
{
    int ret;
    unsigned int chip_id;
    struct rs_cb *rs_cb = NULL;

    RS_CHECK_POINTER_NULL_RETURN_INT(va);
    RS_CHECK_POINTER_NULL_RETURN_INT(size);

    CHK_PRT_RETURN(phy_id >= RS_MAX_DEV_NUM, hccp_err("rs_notify_cfg_get rs set param error ! phy_id:%u",
        phy_id), -EINVAL);

    ret = rsGetLocalDevIDByHostDevID(phy_id, &chip_id);
    CHK_PRT_RETURN(ret, hccp_err("rs_notify_cfg_get phy_id invalid, ret %d, phy_id:%u", ret, phy_id), ret);

    ret = rs_dev2rscb(chip_id, &rs_cb, false);
    CHK_PRT_RETURN(ret, hccp_err("get rs cb failed, chip_id:%u", chip_id), ret);

    *va = rs_cb->notify_va_base;
    *size = rs_cb->notify_size;

    return 0;
}

RS_ATTRI_VISI_DEF int rs_set_tsqp_depth(unsigned int phy_id, unsigned int rdev_index, unsigned int temp_depth,
    unsigned int *qp_num)
{
#ifdef CUSTOM_INTERFACE
    int ret;
    unsigned int chip_id = 0;
    unsigned int sq_depth = 0;
    struct rs_rdev_cb *rdev_cb = NULL;

    CHK_PRT_RETURN(phy_id >= RS_MAX_DEV_NUM, hccp_err("rs_set_tsqp_depth param error ! phy_id:%d", phy_id), -EINVAL);

    CHK_PRT_RETURN(qp_num == NULL, hccp_err("rs_set_tsqp_depth qp_num is NULL, param error!"), -EINVAL);

    CHK_PRT_RETURN(temp_depth < RS_MIN_TEMPTH_DEPTH || temp_depth > RS_MAX_TEMPTH_DEPTH, hccp_err("param error!"
        "temp_depth[%u] can not smaller than [%d] or bigerr than [%d]", temp_depth, RS_MIN_TEMPTH_DEPTH,
        RS_MAX_TEMPTH_DEPTH), -EINVAL);

    ret = rsGetLocalDevIDByHostDevID(phy_id, &chip_id);
    CHK_PRT_RETURN(ret, hccp_err("phy_id[%u] invalid, ret %d", phy_id, ret), ret);

    ret = rs_rdev2rdev_cb(chip_id, rdev_index, &rdev_cb);
    CHK_PRT_RETURN(ret || rdev_cb == NULL, hccp_err("rs_set_tsqp_depth rs_rdev2rdev_cb for chip_id[%u]"
        "failed, ret %d", chip_id, ret), ret);

    ret = rs_roce_set_tsqp_depth(rdev_cb->dev_name, rdev_index, temp_depth, qp_num, &sq_depth);
    CHK_PRT_RETURN(ret, hccp_err("rs_roce_set_tsqp_depth failed, ret %d, dev_name[%s]", ret, rdev_cb->dev_name), ret);

    rdev_cb->tx_depth = sq_depth;
    rdev_cb->rx_depth = sq_depth;
    rdev_cb->qp_max_num = *qp_num;
#endif
    return 0;
}

RS_ATTRI_VISI_DEF int rs_get_tsqp_depth(unsigned int phy_id, unsigned int rdev_index, unsigned int *temp_depth,
    unsigned int *qp_num)
{
#ifdef CUSTOM_INTERFACE
    int ret;
    unsigned int chip_id = 0;
    unsigned int sq_depth = 0;
    struct rs_rdev_cb *rdev_cb = NULL;

    CHK_PRT_RETURN(phy_id >= RS_MAX_DEV_NUM, hccp_err("param error ! phy_id:%d", phy_id), -EINVAL);

    CHK_PRT_RETURN(temp_depth == NULL || qp_num == NULL, hccp_err("temp_depth or qp_num is NULL,"
        "param error!"), -EINVAL);

    ret = rsGetLocalDevIDByHostDevID(phy_id, &chip_id);
    CHK_PRT_RETURN(ret, hccp_err("phy_id[%u] invalid, ret %d", phy_id, ret), ret);

    ret = rs_rdev2rdev_cb(chip_id, rdev_index, &rdev_cb);
    CHK_PRT_RETURN(ret || rdev_cb == NULL, hccp_err("rs_get_tsqp_depth rs_rdev2rdev_cb for chip_id[%u]"
        "failed, ret %d", chip_id, ret), ret);

    ret = rs_roce_get_tsqp_depth(rdev_cb->dev_name, rdev_index, temp_depth, qp_num, &sq_depth);
    CHK_PRT_RETURN(ret, hccp_err("rs_roce_get_tsqp_depth failed, ret %d, dev_name[%s]", ret, rdev_cb->dev_name), ret);
#endif
    return 0;
}

STATIC void rs_set_qp_depth_attr(struct rs_rdev_cb *rdev_cb, struct rs_qp_cb *qp_cb, struct rs_qp_norm *qp_norm)
{
    if (qp_cb->qp_mode == RA_RS_GDR_TMPL_QP_MODE) {
        qp_cb->tx_depth = rdev_cb->tx_depth;
        qp_cb->rx_depth = rdev_cb->rx_depth;
    } else {
        if (rdev_cb->rs_cb->hccp_mode == NETWORK_OFFLINE) {
            qp_cb->tx_depth = RS_QP_TX_DEPTH_OFFLINE;
            qp_cb->rx_depth = RS_QP_RX_DEPTH_OFFLINE;
        } else {
            qp_cb->tx_depth = RS_QP_TX_DEPTH_ONLINE;
            qp_cb->rx_depth = RS_QP_RX_DEPTH_ONLINE;
        }
    }

    if (qp_norm->is_exp != 0 && qp_norm->qp_mode != RA_RS_NOR_QP_MODE) {
        if (rdev_cb->rs_cb->hccp_mode == NETWORK_PEER_ONLINE) {
            qp_cb->tx_depth = (qp_cb->qp_mode != RA_RS_GDR_TMPL_QP_MODE) ? RS_QP_TX_DEPTH_PEER_ONLINE : qp_cb->tx_depth;
            qp_cb->rx_depth = (qp_cb->qp_mode != RA_RS_GDR_TMPL_QP_MODE) ? RS_QP_TX_DEPTH_PEER_ONLINE : qp_cb->rx_depth;
        } else {
            qp_cb->tx_depth = (qp_cb->qp_mode != RA_RS_GDR_TMPL_QP_MODE && qp_cb->qp_mode != RA_RS_GDR_ASYN_QP_MODE)
                                  ? RS_QP_32K_DEPTH
                                  : qp_cb->tx_depth;
        }
        qp_cb->send_sge_num = 1;
        qp_cb->recv_sge_num = 1;
    } else {
        if (rdev_cb->rs_cb->hccp_mode == NETWORK_PEER_ONLINE) {
            qp_cb->tx_depth = (qp_cb->qp_mode != RA_RS_GDR_TMPL_QP_MODE) ? RS_QP_TX_DEPTH_PEER_ONLINE : qp_cb->tx_depth;
            qp_cb->rx_depth = (qp_cb->qp_mode != RA_RS_GDR_TMPL_QP_MODE) ? RS_QP_TX_DEPTH_PEER_ONLINE : qp_cb->rx_depth;
        } else {
            qp_cb->tx_depth = (qp_cb->qp_mode != RA_RS_GDR_TMPL_QP_MODE) ? RS_QP_TX_DEPTH : qp_cb->tx_depth;
            qp_cb->rx_depth = (qp_cb->qp_mode != RA_RS_GDR_TMPL_QP_MODE) ? RS_QP_TX_DEPTH : qp_cb->rx_depth;
        }
        qp_cb->send_sge_num = RS_QP_ATTR_MAX_SEND_SGE;
        qp_cb->recv_sge_num = 1;
    }
}

STATIC int rs_qpcb_init(struct rs_rdev_cb *rdev_cb, struct rs_qp_cb *qp_cb, struct rs_qp_norm *qp_norm)
{
#define RS_DRV_CQ_DEPTH         16384
#define RS_DRV_CQ_128_DEPTH     128
#define RS_DRV_CQ_8K_DEPTH      8192
#define RS_DRV_CQ_32K_DEPTH     32768
    int qp_mode = qp_norm->qp_mode;
    int ret;

    qp_cb->rdev_cb = rdev_cb;
    RS_INIT_LIST_HEAD(&qp_cb->mr_list);
    RS_INIT_LIST_HEAD(&qp_cb->rem_mr_list);

    qp_cb->qp_mode = qp_mode;
    qp_cb->eq_num = 0;
    qp_cb->num_recv_cq_events = 0;
    qp_cb->num_send_cq_events = 0;
    qp_cb->state = RS_QP_STATUS_DISCONNECT;
    qp_cb->ib_pd = rdev_cb->ib_pd;

    // cq attr
    if (qp_norm->is_ext == 1) {
        // update TEMP & ASYN mode cq depth from 32K to 8K due to memory issue
        qp_cb->send_cq_depth = (qp_mode != RA_RS_GDR_TMPL_QP_MODE && qp_mode != RA_RS_GDR_ASYN_QP_MODE)
            ? RS_DRV_CQ_32K_DEPTH : RS_DRV_CQ_8K_DEPTH;
        qp_cb->recv_cq_depth = RS_DRV_CQ_128_DEPTH;
    } else {
        qp_cb->send_cq_depth = RS_DRV_CQ_DEPTH;
        qp_cb->recv_cq_depth = RS_DRV_CQ_DEPTH;
    }

    // qp attr
    rs_set_qp_depth_attr(rdev_cb, qp_cb, qp_norm);

    qp_cb->mem_align = qp_norm->mem_align;

    qp_cb->channel = rs_ibv_create_comp_channel(rdev_cb->ib_ctx);
    CHK_PRT_RETURN(qp_cb->channel == NULL, hccp_err("ibv_create_comp_channel failed! errno(%d)", errno), -EINVAL);
    qp_cb->qos_attr.tc = (RS_ROCE_DSCP_33 & RS_DSCP_MASK) << RS_DSCP_OFF;
    qp_cb->qos_attr.sl = RS_ROCE_4_SL;
    qp_cb->timeout = RS_QP_ATTR_TIMEOUT;
    qp_cb->retry_cnt = RS_QP_ATTR_RETRY_CNT;

    ret = rs_epoll_ctl(rdev_cb->rs_cb->conn_cb.epollfd, EPOLL_CTL_ADD, qp_cb->channel->fd, EPOLLIN | EPOLLRDHUP);
#ifndef CA_CONFIG_LLT
    if (ret) {
        rs_ibv_destroy_comp_channel(qp_cb->channel);
        hccp_err("add channel fd failed ret %d", ret);
        return ret;
    }
#endif
    return 0;
}

STATIC int rs_qpcb_deinit(struct rs_rdev_cb *rdev_cb, struct rs_qp_cb *qp_cb)
{
    int ret;

    if (qp_cb == NULL || qp_cb->channel == NULL) {
        hccp_err("qp_cb or qp_cb->channel is NULL!");
        return -EINVAL;
    }

    ret = rs_epoll_ctl(rdev_cb->rs_cb->conn_cb.epollfd, EPOLL_CTL_DEL, qp_cb->channel->fd, EPOLLIN | EPOLLRDHUP);
#ifndef CA_CONFIG_LLT
    if (ret) {
        hccp_err("del channel fd failed ret %d", ret);
    }
#endif

    if (qp_cb->channel != NULL) {
        rs_ibv_destroy_comp_channel(qp_cb->channel);
        qp_cb->channel = NULL;
    }
#ifndef CA_CONFIG_LLT
    return ret;
#else
    return 0;
#endif
}

STATIC int rs_qp_notify_mr(struct rs_rdev_cb *rdev_cb, struct rs_qp_cb *qp_cb, uint32_t *qpn)
{
    int ret;
    struct rs_mr_cb *notify_mr_node = NULL;

    ret = rs_calloc_mr(1, &notify_mr_node);
    CHK_PRT_RETURN(ret, hccp_err("notify_mr_cb malloc failed"), ret);

    RS_PTHREAD_MUTEX_LOCK(&rdev_cb->rdev_mutex);
    rs_list_add_tail(&qp_cb->list, &rdev_cb->qp_list);
    RS_PTHREAD_MUTEX_ULOCK(&rdev_cb->rdev_mutex);

    if (rdev_cb->notify_type != NO_USE) {
        notify_mr_node->qp_cb = qp_cb;
        notify_mr_node->ib_mr = rdev_cb->notify_mr;
        notify_mr_node->mr_info.addr = rdev_cb->notify_va_base;
        notify_mr_node->mr_info.len = rdev_cb->notify_size;
        notify_mr_node->mr_info.rkey = notify_mr_node->ib_mr->rkey;
    } else {
        notify_mr_node->qp_cb = qp_cb;
        notify_mr_node->ib_mr = NULL;
    }

    RS_PTHREAD_MUTEX_LOCK(&qp_cb->qp_mutex);
    rs_list_add_tail(&notify_mr_node->list, &qp_cb->mr_list);
    RS_PTHREAD_MUTEX_ULOCK(&qp_cb->qp_mutex);
    rdev_cb->qp_cnt++;
    *qpn = qp_cb->ib_qp->qp_num;

    hccp_info("rs qp %d create OK!", *qpn);

    return 0;
}

STATIC int rs_qp_query_info(unsigned int phy_id, unsigned int rdev_index, struct rs_rdev_cb **rdev_cb, int qp_mode)
{
    int ret;
    unsigned int chip_id;
    struct rs_cb *rs_cb = NULL;

    CHK_PRT_RETURN(phy_id >= RS_MAX_DEV_NUM, hccp_err("rs_qp_query_info rs set param error! phy_id:%u",
        phy_id), -EINVAL);

    ret = rsGetLocalDevIDByHostDevID(phy_id, &chip_id);
    CHK_PRT_RETURN(ret, hccp_err("rs_qp_query_info phy_id[%u] invalid, ret:%d", phy_id, ret), ret);

    ret = rs_dev2rscb(chip_id, &rs_cb, false);
    CHK_PRT_RETURN(ret, hccp_err("rs_qp_query_info get rs_cb failed, ret:%d", ret), -ENODEV);

    ret = rs_get_rdev_cb(rs_cb, rdev_index, rdev_cb);
    CHK_PRT_RETURN(ret, hccp_err("rs_get_rdev_cb failed! ret:%d, rdev_index:%u", ret, rdev_index), ret);

    if (qp_mode == RA_RS_GDR_TMPL_QP_MODE) {
        CHK_PRT_RETURN((*rdev_cb)->qp_cnt >= (*rdev_cb)->qp_max_num, hccp_err("Exceeded the maximum QP limit(%u)",
            (*rdev_cb)->qp_max_num), -EINVAL);
    } else {
        CHK_PRT_RETURN((*rdev_cb)->qp_cnt >= RS_QP_NUM_MAX, hccp_err("Exceeded the maximum QP limit(%u)",
            (*rdev_cb)->qp_cnt), -EINVAL);
    }

    return 0;
}

STATIC int rs_init_mem_pool(struct rs_qp_cb *qp_cb)
{
    struct roce_mem_cq_qp_attr mem_attr = {0};
    int ret;

    if ((qp_cb->qp_mode != RA_RS_OP_QP_MODE && qp_cb->qp_mode != RA_RS_OP_QP_MODE_EXT) ||
        qp_cb->mem_align != LITE_ALIGN_2MB) {
        return 0;
    }

    // init mem_pool and store mem_data in mem_resp
    mem_attr.mem_align = qp_cb->mem_align;
    mem_attr.send_qp_depth = qp_cb->tx_depth;
    mem_attr.send_cq_depth = (unsigned int)qp_cb->send_cq_depth;
    mem_attr.send_sge_num = qp_cb->send_sge_num;
    mem_attr.recv_qp_depth = qp_cb->rx_depth;
    mem_attr.recv_cq_depth = (unsigned int)qp_cb->recv_cq_depth;
    mem_attr.recv_sge_num = qp_cb->recv_sge_num;

    ret = rs_roce_init_mem_pool(&mem_attr, &qp_cb->mem_resp.mem_data, qp_cb->rdev_cb->rs_cb->chip_id);
    if (ret != 0) {
        hccp_err("rs_roce_init_mem_pool failed, ret=%d, chip_id=%u", ret, qp_cb->rdev_cb->rs_cb->chip_id);
    }
    return ret;
}

STATIC void rs_deinit_mem_pool(struct rs_qp_cb *qp_cb)
{
    if ((qp_cb->qp_mode != RA_RS_OP_QP_MODE && qp_cb->qp_mode != RA_RS_OP_QP_MODE_EXT) ||
        qp_cb->mem_align != LITE_ALIGN_2MB) {
        return;
    }

    (void)rs_roce_deinit_mem_pool(qp_cb->mem_resp.mem_data.mem_idx);
}

STATIC int rs_alloc_qpcb(struct rs_rdev_cb *rdev_cb, struct rs_qp_cb **qp_cb, struct rs_qp_norm *qp_norm)
{
    int ret;

    ret = rs_calloc_qpcb(1, qp_cb);
    CHK_PRT_RETURN(ret, hccp_err("alloc mem for qp_cb failed, ret:%d errno:%d", ret, errno), -ENOMEM);

    ret = pthread_mutex_init(&(*qp_cb)->qp_mutex, NULL);
    if (ret) {
        hccp_err("pthread_mutex_init failed, ret %d", ret);
        goto qp_mutex_init_err;
    }

    ret = pthread_mutex_init(&(*qp_cb)->cqe_err_info.mutex, NULL);
    if (ret) {
        hccp_err("pthread_mutex_init failed, ret %d", ret);
        goto cqe_mutex_init_err;
    }

    ret = rs_qpcb_init(rdev_cb, *qp_cb, qp_norm);
    if (ret) {
        hccp_err("create qp tx rx failed ret %d", ret);
        goto rs_qpcb_init_err;
    }

    ret = rs_init_mem_pool(*qp_cb);
    if (ret) {
        hccp_err("init mem pool failed ret %d", ret);
        goto rs_init_mem_err;
    }

    ret = rs_drv_create_cq(*qp_cb, qp_norm->is_ext);
    if (ret) {
        hccp_err("create cq failed ret %d", ret);
        goto create_cq_err;
    }

    return 0;

create_cq_err:
    rs_deinit_mem_pool(*qp_cb);

rs_init_mem_err:
    rs_qpcb_deinit(rdev_cb, *qp_cb);

rs_qpcb_init_err:
    pthread_mutex_destroy(&(*qp_cb)->cqe_err_info.mutex);

cqe_mutex_init_err:
    pthread_mutex_destroy(&(*qp_cb)->qp_mutex);

qp_mutex_init_err:
    free(*qp_cb);
    *qp_cb = NULL;

    return ret;
}

STATIC void rs_free_qpcb(struct rs_rdev_cb *rdev_cb, struct rs_qp_cb *qp_cb)
{
    rs_drv_destroy_cq(qp_cb);
    rs_deinit_mem_pool(qp_cb);
    (void)rs_qpcb_deinit(rdev_cb, qp_cb);
    pthread_mutex_destroy(&qp_cb->cqe_err_info.mutex);
    pthread_mutex_destroy(&qp_cb->qp_mutex);
    free(qp_cb);
    qp_cb = NULL;
}

RS_ATTRI_VISI_DEF int rs_qp_create(unsigned int phy_id, unsigned int rdev_index, struct rs_qp_norm qp_norm,
    struct rs_qp_resp *qp_resp)
{
    struct rs_rdev_cb *rdev_cb = NULL;
    struct rs_qp_cb *qp_cb = NULL;
    int ret;

    RS_QP_PARA_CHECK(phy_id);
    CHK_PRT_RETURN(qp_resp == NULL, hccp_err("qp_resp is NULL!"), -EINVAL);

    ret = rs_qp_query_info(phy_id, rdev_index, &rdev_cb, qp_norm.qp_mode);
    CHK_PRT_RETURN(ret, hccp_err("query qp info failed:%d", ret), ret);

    ret = rs_alloc_qpcb(rdev_cb, &qp_cb, &qp_norm);
    CHK_PRT_RETURN(ret, hccp_err("alloc mem for qp_cb failed, ret:%d", ret), ret);

    ret = rs_drv_qp_create(qp_cb, &qp_norm);
    if (ret) {
        hccp_err("create drv qp create failed:%d", ret);
        goto create_qp_err;
    }

    ret = ibv_req_notify_cq(qp_cb->ib_send_cq, 0);
    if (ret) {
        hccp_err("Couldn't request send CQ notification, ret:%d", ret);
        ret = -EOPENSRC;
        goto ret_noritfy_cq;
    }

    ret = ibv_req_notify_cq(qp_cb->ib_recv_cq, 0);
    if (ret) {
        hccp_err("Couldn't request recv CQ notification, ret:%d", ret);
        ret = -EOPENSRC;
        goto ret_noritfy_cq;
    }

    ret = rs_qp_notify_mr(rdev_cb, qp_cb, &qp_resp->qpn);   // alloc mr
    if (ret) {
        hccp_err("store qp notify mr failed:%d", ret);
        goto ret_noritfy_cq;
    }

    if (qp_norm.is_exp) {
        qp_cb->is_exp = RS_IS_EXP;
    } else {
        qp_cb->is_exp = RS_NOT_EXP;
    }

    qp_resp->qpn = (unsigned int)qp_cb->qp_info_lo.qpn;
    qp_resp->gid_idx = (unsigned int)qp_cb->qp_info_lo.gid_idx;
    qp_resp->psn = (unsigned int)qp_cb->qp_info_lo.psn;
    qp_resp->gid = qp_cb->qp_info_lo.gid;

    return 0;

ret_noritfy_cq:
    rs_drv_qp_destroy(qp_cb);

create_qp_err:
    rs_free_qpcb(rdev_cb, qp_cb);
    return ret;
}

STATIC int rs_qpcb_init_with_attrs(struct rs_rdev_cb *rdev_cb, struct rs_qp_cb *qp_cb,
    struct rs_qp_norm_with_attrs *qp_norm)
{
    int ret;

    qp_cb->rdev_cb = rdev_cb;
    RS_INIT_LIST_HEAD(&qp_cb->mr_list);
    RS_INIT_LIST_HEAD(&qp_cb->rem_mr_list);

    qp_cb->qp_mode = qp_norm->ext_attrs.qp_mode;
    qp_cb->num_recv_cq_events = 0;
    qp_cb->num_send_cq_events = 0;
    qp_cb->state = RS_QP_STATUS_DISCONNECT;
    qp_cb->ib_pd = rdev_cb->ib_pd;

    qp_cb->tx_depth = qp_norm->ext_attrs.qp_attr.cap.max_send_wr;
    qp_cb->rx_depth = qp_norm->ext_attrs.qp_attr.cap.max_send_wr;
    qp_cb->send_sge_num = qp_norm->ext_attrs.qp_attr.cap.max_send_sge;
    qp_cb->recv_sge_num = qp_norm->ext_attrs.qp_attr.cap.max_recv_sge;
    qp_cb->send_cq_depth = qp_norm->ext_attrs.cq_attr.send_cq_depth;
    qp_cb->recv_cq_depth = qp_norm->ext_attrs.cq_attr.recv_cq_depth;
    qp_cb->mem_align = qp_norm->ext_attrs.mem_align;

    qp_cb->channel = rs_ibv_create_comp_channel(rdev_cb->ib_ctx);
    CHK_PRT_RETURN(qp_cb->channel == NULL, hccp_err("ibv_create_comp_channel failed! errno(%d)", errno), -EINVAL);
    qp_cb->qos_attr.tc = (RS_ROCE_DSCP_33 & RS_DSCP_MASK) << RS_DSCP_OFF;
    qp_cb->qos_attr.sl = RS_ROCE_4_SL;
    qp_cb->timeout = RS_QP_ATTR_TIMEOUT;
    qp_cb->retry_cnt = RS_QP_ATTR_RETRY_CNT;

    qp_cb->udp_sport = qp_norm->ext_attrs.udp_sport;

    qp_cb->ai_op_support = qp_norm->ai_op_support;
    qp_cb->grp_id = rdev_cb->rs_cb->grp_id;
    qp_cb->cq_cstm_flag = qp_norm->ext_attrs.data_plane_flag.bs.cq_cstm;

    ret = rs_epoll_ctl(rdev_cb->rs_cb->conn_cb.epollfd, EPOLL_CTL_ADD, qp_cb->channel->fd, EPOLLIN | EPOLLRDHUP);
#ifndef CA_CONFIG_LLT
    if (ret) {
        rs_ibv_destroy_comp_channel(qp_cb->channel);
        hccp_err("add channel fd failed ret %d", ret);
        return ret;
    }
#endif
    return 0;
}

STATIC int rs_alloc_qpcb_with_attrs(struct rs_rdev_cb *rdev_cb, struct rs_qp_cb **qp_cb,
    struct rs_qp_norm_with_attrs *qp_norm)
{
    int ret;

    ret = rs_calloc_qpcb(1, qp_cb);
    CHK_PRT_RETURN(ret, hccp_err("alloc mem for qp_cb failed, ret:%d errno:%d", ret, errno), -ENOMEM);

    ret = pthread_mutex_init(&(*qp_cb)->qp_mutex, NULL);
    if (ret) {
        hccp_err("pthread_mutex_init failed, ret %d", ret);
        goto qp_mutex_init_err;
    }

    ret = pthread_mutex_init(&(*qp_cb)->cqe_err_info.mutex, NULL);
    if (ret) {
        hccp_err("pthread_mutex_init failed, ret %d", ret);
        goto cqe_mutex_init_err;
    }

    ret = rs_qpcb_init_with_attrs(rdev_cb, *qp_cb, qp_norm);
    if (ret) {
        hccp_err("create qp tx rx failed ret %d", ret);
        goto rs_qpcb_init_err;
    }

    ret = rs_init_mem_pool(*qp_cb);
    if (ret) {
        hccp_err("init mem pool failed ret %d", ret);
        goto rs_init_mem_err;
    }

    ret = rs_drv_create_cq_with_attrs(*qp_cb, qp_norm->is_ext, &qp_norm->ext_attrs.cq_attr);
    if (ret) {
        hccp_err("create cq failed ret %d", ret);
        goto create_cq_err;
    }

    return 0;

create_cq_err:
    rs_deinit_mem_pool(*qp_cb);

rs_init_mem_err:
    rs_qpcb_deinit(rdev_cb, *qp_cb);

rs_qpcb_init_err:
    pthread_mutex_destroy(&(*qp_cb)->cqe_err_info.mutex);

cqe_mutex_init_err:
    pthread_mutex_destroy(&(*qp_cb)->qp_mutex);

qp_mutex_init_err:
    free(*qp_cb);
    *qp_cb = NULL;

    return ret;
}

STATIC int rs_qp_check_qp_norm(struct rs_qp_norm_with_attrs *qp_norm, int *qp_mode)
{
    CHK_PRT_RETURN(qp_norm == NULL, hccp_err("qp_norm is NULL!"), -EINVAL);
    CHK_PRT_RETURN(qp_norm->ext_attrs.version != QP_CREATE_WITH_ATTR_VERSION,
        hccp_err("attr version[%d] mismatch, expect [%d]", qp_norm->ext_attrs.version, QP_CREATE_WITH_ATTR_VERSION),
        -EINVAL);

    *qp_mode = qp_norm->ext_attrs.qp_mode;
    if (*qp_mode < 0 || *qp_mode >= RA_RS_ERR_QP_MODE) {
        hccp_err("qp_mode[%d] must greater or equal to 0 and less than %d", *qp_mode, RA_RS_ERR_QP_MODE);
        return -EINVAL;
    }

    if (*qp_mode == RA_RS_OP_QP_MODE_EXT) {
        *qp_mode = RA_RS_OP_QP_MODE;
    }

    qp_norm->ext_attrs.qp_mode = *qp_mode;
    return 0;
}

#ifdef CUSTOM_INTERFACE
STATIC void rs_qp_prepare_cq_data_plane_info(struct ibv_cq *ib_cq, struct ai_data_plane_cq *data_plane_cq)
{
    struct hns_roce_cq_data_plane_info cq_info = {0};

    (void)rs_roce_get_cq_data_plane_info(ib_cq, &cq_info);
    data_plane_cq->cqn = cq_info.cqn;
    data_plane_cq->buf_addr = cq_info.buf_addr;
    data_plane_cq->cqe_size = cq_info.cqe_size;
    data_plane_cq->depth = cq_info.depth;
    data_plane_cq->head_addr = cq_info.head_addr;
    data_plane_cq->tail_addr = cq_info.tail_addr;
    data_plane_cq->swdb_addr = cq_info.swdb_addr;
    data_plane_cq->db_reg = cq_info.db_reg;
    hccp_info("cqn:%u buf_addr:0x%llx cqe_size:%u depth:%u head_addr:0x%llx tail_addr:0x%llx swdb_addr:0x%llx",
        data_plane_cq->cqn, data_plane_cq->buf_addr, data_plane_cq->cqe_size, data_plane_cq->depth,
        data_plane_cq->head_addr, data_plane_cq->tail_addr, data_plane_cq->swdb_addr);
}

STATIC void rs_qp_prepare_wq_data_plane_info(struct hns_roce_wq_data_plane_info *wq_info,
    struct ai_data_plane_wq *data_plane_wq)
{
    data_plane_wq->wqn = wq_info->wqn;
    data_plane_wq->buf_addr = wq_info->buf_addr;
    data_plane_wq->wqebb_size = wq_info->wqebb_size;
    data_plane_wq->depth = wq_info->depth;
    data_plane_wq->head_addr = wq_info->head_addr;
    data_plane_wq->tail_addr = wq_info->tail_addr;
    data_plane_wq->swdb_addr = wq_info->swdb_addr;
    data_plane_wq->db_reg = wq_info->db_reg;
    hccp_info("wqn:%u buf_addr:0x%llx wqebb_size:%u depth:%u head_addr:%u tail_addr:%u swdb_addr:0x%llx",
        data_plane_wq->wqn, data_plane_wq->buf_addr, data_plane_wq->wqebb_size, data_plane_wq->depth,
        data_plane_wq->head_addr, data_plane_wq->tail_addr, data_plane_wq->swdb_addr);
}

STATIC void rs_qp_prepare_qp_data_plane_info(struct ibv_qp *ib_qp, struct ai_data_plane_wq *data_plane_sq,
    struct ai_data_plane_wq *data_plane_rq)
{
    struct hns_roce_qp_data_plane_info qp_info = {0};

    (void)rs_roce_get_qp_data_plane_info(ib_qp, &qp_info);
    rs_qp_prepare_wq_data_plane_info(&qp_info.sq, data_plane_sq);
    rs_qp_prepare_wq_data_plane_info(&qp_info.rq, data_plane_rq);
}

STATIC void rs_qp_prepare_data_plane_info(struct rs_qp_norm_with_attrs *qp_norm, struct rs_qp_cb *qp_cb,
    struct rs_qp_resp_with_attrs *qp_resp)
{
    // skip to prepare cq data plane info
    if (qp_norm->ext_attrs.data_plane_flag.bs.cq_cstm != 0) {
        qp_resp->ai_scq_addr = (unsigned long long)(uintptr_t)qp_cb->ib_send_cq;
        qp_resp->ai_rcq_addr = (unsigned long long)(uintptr_t)qp_cb->ib_recv_cq;
        rs_qp_prepare_cq_data_plane_info(qp_cb->ib_send_cq, &qp_resp->data_plane_info.scq);
        rs_qp_prepare_cq_data_plane_info(qp_cb->ib_recv_cq, &qp_resp->data_plane_info.rcq);
    }

    // skip to prepare qp data plane info
    if (qp_norm->ai_op_support != 0) {
        rs_qp_prepare_qp_data_plane_info(qp_cb->ib_qp, &qp_resp->data_plane_info.sq, &qp_resp->data_plane_info.rq);
    }
}
#endif

STATIC void rs_qp_prepare_qp_resp(struct rs_qp_norm_with_attrs *qp_norm, struct rs_qp_cb *qp_cb,
    struct rs_qp_resp_with_attrs *qp_resp)
{
    if (qp_norm->is_exp != 0) {
        qp_cb->is_exp = RS_IS_EXP;
    } else {
        qp_cb->is_exp = RS_NOT_EXP;
    }

    qp_resp->ai_qp_addr = (unsigned long long)(uintptr_t)qp_cb->ib_qp;
    qp_resp->sq_index = (unsigned int)qp_cb->sq_index;
    qp_resp->db_index = (unsigned int)qp_cb->db_index;
    qp_resp->gid_idx = (unsigned int)qp_cb->qp_info_lo.gid_idx;
    qp_resp->psn = (unsigned int)qp_cb->qp_info_lo.psn;

#ifdef CUSTOM_INTERFACE
    rs_qp_prepare_data_plane_info(qp_norm, qp_cb, qp_resp);
#endif

    return;
}

RS_ATTRI_VISI_DEF int rs_qp_create_with_attrs(unsigned int phy_id, unsigned int rdev_index,
    struct rs_qp_norm_with_attrs *qp_norm, struct rs_qp_resp_with_attrs *qp_resp)
{
    struct rs_rdev_cb *rdev_cb = NULL;
    struct rs_qp_cb *qp_cb = NULL;
    int qp_mode;
    int ret;

    RS_QP_PARA_CHECK(phy_id);
    CHK_PRT_RETURN(qp_resp == NULL, hccp_err("qp_resp is NULL!"), -EINVAL);

    ret = rs_qp_check_qp_norm(qp_norm, &qp_mode);
    CHK_PRT_RETURN(ret != 0, hccp_err("check qp mode failed, ret:%d", ret), ret);

    ret = rs_qp_query_info(phy_id, rdev_index, &rdev_cb, qp_mode);
    CHK_PRT_RETURN(ret, hccp_err("query qp info failed:%d", ret), ret);

    ret = rs_alloc_qpcb_with_attrs(rdev_cb, &qp_cb, qp_norm);
    CHK_PRT_RETURN(ret, hccp_err("alloc mem for qp_cb failed, ret:%d", ret), ret);

    ret = rs_drv_qp_create_with_attrs(qp_cb, qp_norm);
    if (ret) {
        hccp_err("create drv qp create failed:%d", ret);
        goto create_qp_err;
    }

    ret = ibv_req_notify_cq(qp_cb->ib_send_cq, 0);
    if (ret) {
        hccp_err("Couldn't request send CQ notification, ret:%d", ret);
        ret = -EOPENSRC;
        goto ret_noritfy_cq;
    }

    ret = ibv_req_notify_cq(qp_cb->ib_recv_cq, 0);
    if (ret) {
        hccp_err("Couldn't request recv CQ notification, ret:%d", ret);
        ret = -EOPENSRC;
        goto ret_noritfy_cq;
    }

    ret = rs_qp_notify_mr(rdev_cb, qp_cb, &qp_resp->qpn);   // alloc mr
    if (ret) {
        hccp_err("store qp notify mr failed:%d", ret);
        goto ret_noritfy_cq;
    }

    rs_qp_prepare_qp_resp(qp_norm, qp_cb, qp_resp);

    return 0;

ret_noritfy_cq:
    rs_drv_qp_destroy(qp_cb);

create_qp_err:
    rs_free_qpcb(rdev_cb, qp_cb);
    return ret;
}

RS_ATTRI_VISI_DEF int rs_qp_destroy(unsigned int phy_id, unsigned int rdev_index, unsigned int qpn)
{
    int ret;
    struct rs_qp_cb *qp_cb = NULL;
    struct rs_mr_cb *mr_tmp = NULL;
    struct rs_mr_cb *mr_tmp2 = NULL;

    RS_QP_PARA_CHECK(phy_id);
    ret = rs_qpn2qpcb(phy_id, rdev_index, qpn, &qp_cb);
    CHK_PRT_RETURN(ret || qp_cb == NULL, hccp_err("get qp cb failed qpn %u, ret %d", qpn, ret), ret);

    RS_PTHREAD_MUTEX_LOCK(&qp_cb->rdev_cb->rdev_mutex);
    rs_list_del(&qp_cb->list);
    RS_PTHREAD_MUTEX_ULOCK(&qp_cb->rdev_cb->rdev_mutex);
    rs_ibv_ack_cq_events(qp_cb->ib_send_cq, qp_cb->num_send_cq_events);
    rs_ibv_ack_cq_events(qp_cb->ib_recv_cq, qp_cb->num_recv_cq_events);

    // dereg mr
    RS_PTHREAD_MUTEX_LOCK(&qp_cb->qp_mutex);
    RS_LIST_GET_HEAD_ENTRY(mr_tmp, mr_tmp2, &qp_cb->mr_list, list, struct rs_mr_cb);
    for (; (&mr_tmp->list) != &qp_cb->mr_list;
        mr_tmp = mr_tmp2, mr_tmp2 = list_entry(mr_tmp2->list.next, struct rs_mr_cb, list)) {
        if (mr_tmp->ib_mr != qp_cb->rdev_cb->notify_mr) {
            (void)rs_drv_mr_dereg(mr_tmp->ib_mr);
        }
        rs_list_del(&mr_tmp->list);
        free(mr_tmp);
        mr_tmp = NULL;
    }

    RS_LIST_GET_HEAD_ENTRY(mr_tmp, mr_tmp2, &qp_cb->rem_mr_list, list, struct rs_mr_cb);
    for (; (&mr_tmp->list) != &qp_cb->rem_mr_list;
        mr_tmp = mr_tmp2, mr_tmp2 = list_entry(mr_tmp2->list.next, struct rs_mr_cb, list)) {
        rs_list_del(&mr_tmp->list);
        free(mr_tmp);
        mr_tmp = NULL;
    }
    RS_PTHREAD_MUTEX_ULOCK(&qp_cb->qp_mutex);

    // destroy qp
    rs_drv_qp_destroy(qp_cb);
    rs_drv_destroy_cq(qp_cb);
    rs_deinit_mem_pool(qp_cb);

    qp_cb->rdev_cb->qp_cnt--;
    ret = rs_qpcb_deinit(qp_cb->rdev_cb, qp_cb);
    if (ret) {
        hccp_err("rs_qpcb_deinit failed! ret[%d]", ret);
    }

    pthread_mutex_destroy(&qp_cb->cqe_err_info.mutex);
    pthread_mutex_destroy(&qp_cb->qp_mutex);
    hccp_info("qp %d destroy qp, send wr[%u].", qpn, qp_cb->send_wr_num);

    free(qp_cb);
    qp_cb = NULL;
    return ret;
}

static void rs_qp_connect_async_mr(const struct rs_qp_cb *qp_cb)
{
    int ret;
    struct rs_mr_cb *mr_cb = NULL;
    struct rs_mr_cb *mr_cb2 = NULL;

    RS_LIST_GET_HEAD_ENTRY(mr_cb, mr_cb2, &qp_cb->mr_list, list, struct rs_mr_cb);
    for (; (&mr_cb->list) != &qp_cb->mr_list;
        mr_cb = mr_cb2, mr_cb2 = list_entry(mr_cb2->list.next, struct rs_mr_cb, list)) {
        ret = rs_mr_info_sync(mr_cb);
        if (ret) {
            hccp_warn("rs_mr_info_sync unsuccessful, ret:%d", ret);
        }
    }
}

STATIC void rs_qp_connect_async_qpcb_set(int fd, struct rs_qp_cb *qp_cb)
{
    int ret;
    ret = rs_socket_send(fd, &qp_cb->qp_info_lo, sizeof(struct rs_qp_info));
    if (ret == sizeof(struct rs_qp_info)) {
        qp_cb->send_len += (uint32_t)ret;
        qp_cb->state = RS_QP_STATUS_CONNECTING;
    } else {
        qp_cb->state = RS_QP_STATUS_TIMEOUT;
    }
}

STATIC void rs_qp_connect_async_length(int fd, struct rs_qp_cb *qp_cb)
{
    int ret;
    struct rs_qp_len_info msg;

    msg.cmd = RS_CMD_LEN_INFO;
    msg.len = qp_cb->send_len;

    ret = rs_socket_send(fd, &msg, sizeof(struct rs_qp_len_info));
    if (ret != sizeof(struct rs_qp_len_info)) {
        qp_cb->state = RS_QP_STATUS_TIMEOUT;
    }
}

static int rs_qp_connect_async_init_para(struct rs_qp_conn_para qp_conn_para, int fd,
    struct rs_qp_cb **qp_cb, struct rs_conn_info **conn)
{
    int ret;

    CHK_PRT_RETURN(qp_conn_para.phy_id >= RS_MAX_DEV_NUM, hccp_err("param error ! phy_id:%u",
        qp_conn_para.phy_id), -EINVAL);

    CHK_PRT_RETURN(fd < 0, hccp_err("param error ! fd:%d must bigger than 0", fd), -EINVAL);

    ret = rs_qpn2qpcb(qp_conn_para.phy_id, qp_conn_para.rdev_index, qp_conn_para.qpn, qp_cb);
    CHK_PRT_RETURN(ret, hccp_err("get qpcb failed, qpn %u, ret %d", qp_conn_para.qpn, ret), ret);

    ret = rs_fd2conn(fd, conn);
    CHK_PRT_RETURN(ret, hccp_err("get conn failed, fd %d, ret %d", fd, ret), ret);

    rs_get_cur_time(&((*qp_cb)->start_time));
    (*qp_cb)->send_len = 0;
    (*qp_cb)->recv_len = 0;
    (*qp_cb)->expect_len = 0;
    (*qp_cb)->conn_info = *conn;

    return 0;
}

STATIC int rs_typical_qp_state_modifyto_rtr(struct rs_qp_cb *qp_cb, struct typical_qp *local_qp_info,
    struct typical_qp *remote_qp_info)
{
    struct ibv_port_attr port_attr = { 0 };
    union ibv_gid remote_info_gid = { 0 };
    struct ibv_qp_attr attr = { 0 };
    int ret;

    attr.qp_state                  = IBV_QPS_RTR;
    attr.dest_qp_num               = remote_qp_info->qpn;
    attr.rq_psn                    = remote_qp_info->psn;
    attr.min_rnr_timer             = RS_QP_ATTR_MIN_RNR_TIMER;
    (attr.ah_attr).is_global       = 0;
    (attr.ah_attr).sl              = local_qp_info->sl;
    (attr.ah_attr).src_path_bits   = 0;
    (attr.ah_attr).port_num        = qp_cb->rdev_cb->ib_port;

    attr.path_mtu = rs_drv_set_mtu(qp_cb);
    CHK_PRT_RETURN(attr.path_mtu < IBV_MTU_1024, hccp_err("qpn[%u] failed to set mtu, mtu[%d] < [%d]",
        local_qp_info->qpn, attr.path_mtu, IBV_MTU_1024), -EPERM);
    if (qp_cb->rdev_cb->rs_cb->hccp_mode == NETWORK_PEER_ONLINE) {
        attr.max_dest_rd_atomic = RS_MAX_RD_ATOMIC_NUM_PEER_ONLINE;
    } else {
        attr.max_dest_rd_atomic = RS_MAX_RD_ATOMIC_NUM;
    }
    (attr.ah_attr).grh.traffic_class = local_qp_info->tc;
    // get gid_idx dynamically to avoid gid_idx changed issue: refresh gid_idx when it changed
    ret = rs_drv_get_gid_index(qp_cb->rdev_cb, &port_attr, &qp_cb->qp_info_lo.gid_idx);
    if (ret == 0 && local_qp_info->gid_idx != (uint32_t)qp_cb->qp_info_lo.gid_idx) {
        hccp_warn("qpn[%u] qp_mode[%d] refresh gid_idx[%u] to [%d]", local_qp_info->qpn, qp_cb->qp_mode,
            local_qp_info->gid_idx, qp_cb->qp_info_lo.gid_idx);
        local_qp_info->gid_idx = (uint32_t)qp_cb->qp_info_lo.gid_idx;
    }

    (void)memcpy_s(remote_info_gid.raw, HCCP_GID_RAW_LEN, remote_qp_info->gid, HCCP_GID_RAW_LEN);
    if (remote_info_gid.global.interface_id) {
        attr.ah_attr.is_global = 1;
        attr.ah_attr.grh.hop_limit = 1;
        attr.ah_attr.grh.dgid = remote_info_gid;
        attr.ah_attr.grh.sgid_index = local_qp_info->gid_idx;
    }

    ret = rs_ibv_modify_qp(qp_cb->ib_qp, &attr,
                           IBV_QP_STATE | IBV_QP_AV |
                           IBV_QP_PATH_MTU | IBV_QP_DEST_QPN |
                           IBV_QP_RQ_PSN | IBV_QP_MAX_DEST_RD_ATOMIC |
                           IBV_QP_MIN_RNR_TIMER);
    CHK_PRT_RETURN(ret, hccp_err("[modifyto_rtr]local_qpn[%u] remote_qpn[%u] ibv_modify_qp failed ret[%d], errno[%d]",
        local_qp_info->qpn, remote_qp_info->qpn, ret, errno), -EOPENSRC);
    hccp_info("qp qos attr: qpn[%u] tc[%u] sl[%u]", local_qp_info->qpn, local_qp_info->tc, local_qp_info->sl);
    return 0;
}

STATIC int rs_typical_qp_state_modifyto_rts(struct rs_qp_cb *qp_cb, struct typical_qp *local_qp_info)
{
    struct ibv_qp_attr attr = {0};
    int ret;

    attr.qp_state      = IBV_QPS_RTS;
    attr.timeout       = (uint8_t)local_qp_info->retry_time;
    attr.retry_cnt     = (uint8_t)local_qp_info->retry_cnt;
    attr.rnr_retry     = RS_QP_ATTR_RNR_RETRY;
    attr.sq_psn        = local_qp_info->psn;
    if (qp_cb->rdev_cb->rs_cb->hccp_mode == NETWORK_PEER_ONLINE) {
        attr.max_rd_atomic = RS_MAX_RD_ATOMIC_NUM_PEER_ONLINE;
    } else {
        attr.max_rd_atomic = RS_MAX_RD_ATOMIC_NUM;
    }

    ret = rs_ibv_modify_qp(qp_cb->ib_qp, &attr,
                           IBV_QP_STATE | IBV_QP_TIMEOUT |
                           IBV_QP_RETRY_CNT | IBV_QP_RNR_RETRY |
                           IBV_QP_SQ_PSN | IBV_QP_MAX_QP_RD_ATOMIC);
    CHK_PRT_RETURN(ret != 0, hccp_err("[modifyto_rts]local_qpn[%u] ibv_modify_qp failed ret[%d], errno[%d]",
        local_qp_info->qpn, ret, errno), -EOPENSRC);

    hccp_info("qp rdma attr: qpn[%u] timeout[%u] retrycnt[%u]", local_qp_info->qpn, local_qp_info->retry_time,
        local_qp_info->retry_cnt);
    return 0;
}

STATIC void rs_typical_qp_modify_info_related(struct rs_qp_cb *qp_cb, struct typical_qp *local_qp_info,
    struct typical_qp *remote_qp_info)
{
    qp_cb->state = RS_QP_STATUS_CONNECTED;
    // local qp info related: no need to relate qpn, psn, gid_idx, gid
    qp_cb->qos_attr.tc = (unsigned char)local_qp_info->tc;
    qp_cb->qos_attr.sl = (unsigned char)local_qp_info->sl;
    qp_cb->retry_cnt = local_qp_info->retry_cnt;
    qp_cb->timeout = local_qp_info->retry_time;
    // remote qp info related
    qp_cb->qp_info_rem.qpn = (int)remote_qp_info->qpn;
    qp_cb->qp_info_rem.psn = (int)remote_qp_info->psn;
    qp_cb->qp_info_rem.gid_idx = (int)remote_qp_info->gid_idx;
    (void)memcpy_s(qp_cb->qp_info_rem.gid.raw, HCCP_GID_RAW_LEN, remote_qp_info->gid, HCCP_GID_RAW_LEN);
}

RS_ATTRI_VISI_DEF int rs_typical_qp_modify(unsigned int phy_id, unsigned int rdev_index,
    struct typical_qp local_qp_info, struct typical_qp remote_qp_info, unsigned int *udp_sport)
{
#ifdef CUSTOM_INTERFACE
    unsigned int qp_attr_mask = HNS_ROCE_AI_QPC_UDPSPN;
    struct hns_roce_qpc_attr_val qp_attr_val = { 0 };
#endif
    struct ibv_qp_init_attr init_attr = { 0 };
    struct ibv_qp_attr attr = { 0 };
    struct rs_qp_cb *qp_cb = NULL;
    int ret;

    CHK_PRT_RETURN(phy_id >= RS_MAX_DEV_NUM, hccp_err("[modify]phy_id:%u >= [%d], is invalid", phy_id, RS_MAX_DEV_NUM),
        -EINVAL);

    CHK_PRT_RETURN(rs_qpn2qpcb(phy_id, rdev_index, local_qp_info.qpn, &qp_cb),
        hccp_err("[modify]rs_qpn2qpcb qpn:%u failed, phy_id[%u]", local_qp_info.qpn, phy_id), -EACCES);

    CHK_PRT_RETURN(qp_cb->state == RS_QP_STATUS_CONNECTED,
        hccp_info("local_qpn:%u remote_qpn:%u already been connected, no need to modify again",
        local_qp_info.qpn, remote_qp_info.qpn), 0);

    // see ib_modify_qp_is_ok for status modify, only support modify qp from INIT to RTR
    ret = rs_ibv_query_qp(qp_cb->ib_qp, &attr, IBV_QP_STATE, &init_attr);
    CHK_PRT_RETURN(ret != 0 || attr.qp_state != IBV_QPS_INIT, hccp_err("query qpn:%u failed, ret:%d or state:%d != %d",
        local_qp_info.qpn, ret, attr.qp_state, IBV_QPS_INIT), -EOPENSRC);

    ret = rs_typical_qp_state_modifyto_rtr(qp_cb, &local_qp_info, &remote_qp_info);
    CHK_PRT_RETURN(ret != 0, hccp_err("[modify]local_qpn:%u remote_qpn:%u modify to rtr failed, ret %d",
        local_qp_info.qpn, remote_qp_info.qpn, ret), ret);

    ret = rs_typical_qp_state_modifyto_rts(qp_cb, &local_qp_info);
    CHK_PRT_RETURN(ret != 0, hccp_err("[modify]local_qpn:%u remote_qpn:%u modify to rts failed, ret %d",
        local_qp_info.qpn, remote_qp_info.qpn, ret), ret);

#ifdef CUSTOM_INTERFACE
    ret = rs_roce_query_qpc(qp_cb->ib_qp, &qp_attr_val, qp_attr_mask);
    if (ret != 0) {
        hccp_warn("qpn:%d query qpc unsuccessful, ret %d", local_qp_info.qpn, ret);
    } else {
        qp_cb->udp_sport = qp_attr_val.udp_sport;
    }
#endif
    *udp_sport = qp_cb->udp_sport;
    rs_typical_qp_modify_info_related(qp_cb, &local_qp_info, &remote_qp_info);

    hccp_info("local_qpn:%u remote_qpn:%u modify succ, udp_sport:%u",
        local_qp_info.qpn, remote_qp_info.qpn, qp_cb->udp_sport);

    return 0;
}

STATIC int rs_qp_state_batch_modifyto_pause(struct rs_qp_cb *qp_cb)
{
    int ret;

    ret = rs_drv_qp_state_modifyto_reset(qp_cb);
    CHK_PRT_RETURN(ret, hccp_err("qp modify to reset failed, ret %d", ret), ret);

    hccp_info("local qpn[%d] remote qpn[%d] modify to pause succ", qp_cb->qp_info_lo.qpn, qp_cb->qp_info_rem.qpn);
    return 0;
}

STATIC int rs_qp_state_batch_modifyto_connected(struct rs_qp_cb *qp_cb)
{
    struct ibv_qp_attr attr;
    int ret;

    ret = memset_s(&attr, sizeof(struct ibv_qp_attr), 0, sizeof(struct ibv_qp_attr));
    CHK_PRT_RETURN(ret, hccp_err("memset_s attr failed ret %d", ret), -ESAFEFUNC);

    ret = rs_drv_qp_state_modifyto_init(qp_cb, &attr);
    CHK_PRT_RETURN(ret, hccp_err("qp modify to init failed, ret %d", ret), ret);
    ret = rs_drv_qp_state_modifyto_rtr(qp_cb, &attr);
    CHK_PRT_RETURN(ret, hccp_err("qp modify to rtr failed, ret %d", ret), ret);
    ret = rs_drv_qp_state_modifyto_rts(qp_cb, &attr);
    CHK_PRT_RETURN(ret, hccp_err("qp modify to rts failed, ret %d", ret), ret);

    hccp_info("local qpn[%d] remote qpn[%d] modify to rts succ", qp_cb->qp_info_lo.qpn, qp_cb->qp_info_rem.qpn);
    return 0;
}

RS_ATTRI_VISI_DEF int rs_qp_batch_modify(unsigned int phy_id, unsigned int rdev_index,
    int status, int qpn[], int qpn_num)
{
    struct rs_qp_cb *qp_cb = NULL;
    int ret;
    int i;

    CHK_PRT_RETURN(phy_id >= RS_MAX_DEV_NUM, hccp_err("[modify]phy_id:%u >= [%d], is invalid", phy_id, RS_MAX_DEV_NUM),
        -EINVAL);

    for (i = 0; i < qpn_num; i++) {
        CHK_PRT_RETURN(rs_qpn2qpcb(phy_id, rdev_index, (uint32_t)qpn[i], &qp_cb),
            hccp_err("[modify]rs_qpn2qpcb failed, phy_id[%u]", phy_id), -EACCES);

        /*
         * see ib_modify_qp_is_ok for status modify
         * only support modify qp from STATUS_PAUSE(RESET) to STATUS_CONNECTED(INIT)
         */
        if (status == RS_QP_STATUS_CONNECTED && qp_cb->state == RS_QP_STATUS_PAUSE) {
            ret = rs_qp_state_batch_modifyto_connected(qp_cb);
            CHK_PRT_RETURN(ret, hccp_err("modify_qp qpn[%d]:%d to connected failed, ret[%d] phy_id[%u]",
                i, qpn[i], ret, phy_id), ret);
        } else if (status == RS_QP_STATUS_PAUSE) {
            ret = rs_qp_state_batch_modifyto_pause(qp_cb);
            CHK_PRT_RETURN(ret, hccp_err("modify_qp qpn[%d]:%d to pause failed, ret[%d] phy_id[%u]",
                i, qpn[i], ret, phy_id), ret);
        } else {
            hccp_err("modify_qp qpn[%d]:%d failed, not support to modify status[%d] to status[%d], phy_id[%u]",
                i, qpn[i], qp_cb->state, status, phy_id);
            return -EINVAL;
        }

        qp_cb->state = status;
    }

    return 0;
}

RS_ATTRI_VISI_DEF int rs_qp_connect_async(unsigned int phy_id, unsigned int rdev_index, unsigned int qpn, int fd)
{
    int ret;
    struct rs_qp_cb *qp_cb = NULL;
    struct rs_conn_info *conn = NULL;
    struct rs_qp_conn_para qp_conn_para;
    hccp_info("qp:%d, fd:%d", qpn, fd);

    qp_conn_para.phy_id = phy_id;
    qp_conn_para.rdev_index = rdev_index;
    qp_conn_para.qpn = qpn;
    ret = rs_qp_connect_async_init_para(qp_conn_para, fd, &qp_cb, &conn);
    CHK_PRT_RETURN(ret, hccp_err("rs_qp_connect_async_init_para failed, qpn %u, ret %d", qpn, ret), ret);

    RS_PTHREAD_MUTEX_LOCK(&qp_cb->qp_mutex);

    if (qp_cb->state == RS_QP_STATUS_REM_FD_CLOSE) {
        hccp_warn("remote qp fd close, can not use it anymore!");
        RS_PTHREAD_MUTEX_ULOCK(&qp_cb->qp_mutex);
        return -EFAULT;
    }

    if ((qp_cb->state == RS_QP_STATUS_CONNECTED) || (qp_cb->state == RS_QP_STATUS_CONNECTING)) {
        hccp_warn("qp %d has already sync! state[%d]", qp_cb->qp_info_lo.qpn, qp_cb->state);
        RS_PTHREAD_MUTEX_ULOCK(&qp_cb->qp_mutex);
        return -EEXIST;
    }

    rs_qp_connect_async_qpcb_set(fd, qp_cb);

    hccp_info("after socket fd %d send QP %u, chip_id %u, state:%d!",
        fd, qpn, qp_cb->rdev_cb->rs_cb->chip_id, qp_cb->state);

    RS_PTHREAD_MUTEX_ULOCK(&qp_cb->qp_mutex);

    rs_qp_mr_recv_handle(fd, qp_cb);

    rs_qp_connect_async_mr(qp_cb);

    rs_qp_connect_async_length(fd, qp_cb);

    hccp_info("QP %d async done, state:%d!", qpn, qp_cb->state);

    return 0;
}

RS_ATTRI_VISI_DEF int rs_get_qp_status(unsigned int phy_id, unsigned int rdev_index, unsigned int qpn,
    struct rs_qp_status_info *qp_info)
{
#ifdef CUSTOM_INTERFACE
    unsigned int qp_attr_mask = HNS_ROCE_AI_QPC_UDPSPN;
    struct hns_roce_qpc_attr_val qp_attr_val = { 0 };
#endif
    struct rs_qp_cb *qp_cb = NULL;
    int ret;

    CHK_PRT_RETURN(qp_info == NULL, hccp_err("param error, qp_info is NULL"), -EINVAL);

    CHK_PRT_RETURN(phy_id >= RS_MAX_DEV_NUM, hccp_err("phy_id:%u >= [%d], is invalid",
        phy_id, RS_MAX_DEV_NUM), -EINVAL);

    ret = rs_qpn2qpcb(phy_id, rdev_index, qpn, &qp_cb);
    CHK_PRT_RETURN(ret, hccp_err("get qp cb failed, qpn:%u, ret %d", qpn, ret), ret);

    // qp state is CONNECTED, no need to handle
    if (qp_cb->state == RS_QP_STATUS_CONNECTED) {
        goto update_qp_cb;
    }

    // modify state to CONNECTED
    if (qp_cb->expect_len == qp_cb->recv_len - sizeof(struct rs_qp_len_info)) {
        qp_cb->state = RS_QP_STATUS_CONNECTED;
    } else {
        rs_qp_mr_recv_handle(qp_cb->conn_info->connfd, qp_cb);
        goto out;
    }

update_qp_cb:
#ifdef CUSTOM_INTERFACE
    ret = rs_roce_query_qpc(qp_cb->ib_qp, &qp_attr_val, qp_attr_mask);
    if (ret != 0) {
        hccp_warn("qpn:%d query qpc unsuccessful, ret %d", qp_cb->qp_info_lo.qpn, ret);
    } else {
        qp_cb->udp_sport = qp_attr_val.udp_sport;
    }
#endif
out:
    hccp_dbg("qp:%u, state:%d, udp_sport:%u", qpn, qp_cb->state, qp_cb->udp_sport);
    qp_info->status = qp_cb->state;
    qp_info->udp_sport = qp_cb->udp_sport;

    return 0;
}

RS_ATTRI_VISI_DEF int rs_get_qp_context(unsigned int phy_id, unsigned int rdev_index, unsigned int qpn, void** qp,
    void** send_cq, void** recv_cq)
{
    int ret;
    struct rs_qp_cb *qp_cb = NULL;

    CHK_PRT_RETURN(phy_id >= RS_MAX_DEV_NUM, hccp_err("phy_id:%u >= [%d], is invalid", phy_id, RS_MAX_DEV_NUM),
        -EINVAL);

    ret = rs_qpn2qpcb(phy_id, rdev_index, qpn, &qp_cb);
    CHK_PRT_RETURN(ret, hccp_err("rs_qpn2qpcb failed ret[%d]", ret), ret);

    *qp = qp_cb->ib_qp;
    *send_cq = qp_cb->ib_send_cq;
    *recv_cq = qp_cb->ib_recv_cq;

    hccp_dbg("qpn[%u] succ", qpn);

    return 0;
}

STATIC int rs_query_rdev_cb(unsigned int phy_id, unsigned int rdev_index, struct rs_rdev_cb **rdev_cb)
{
    int ret;
    unsigned int chip_id;
    struct rs_cb *rs_cb = NULL;

    RS_QP_PARA_CHECK(phy_id);

    ret = rsGetLocalDevIDByHostDevID(phy_id, &chip_id);
    CHK_PRT_RETURN(ret, hccp_err("rs_query_rdev_cb phy_id[%u] invalid, ret:%d", phy_id, ret), ret);

    ret = rs_dev2rscb(chip_id, &rs_cb, false);
    CHK_PRT_RETURN(ret, hccp_err("rs_query_rdev_cb get rs_cb failed, ret:%d", ret), -ENODEV);

    ret = rs_get_rdev_cb(rs_cb, rdev_index, rdev_cb);
    CHK_PRT_RETURN(ret, hccp_err("rs_get_rdev_cb failed! ret:%d, rdev_index:%u", ret, rdev_index), ret);

    return 0;
}

STATIC int rs_build_up_qpcb(struct rs_cq_context *cq_context, struct ibv_qp_init_attr *qp_init_attr,
    struct rs_qp_cb **qp_cb)
{
    int ret;

    ret = rs_calloc_qpcb(1, qp_cb);
    CHK_PRT_RETURN(ret, hccp_err("alloc mem for qp_cb failed, ret:%d errno:%d", ret, errno), -ENOMEM);

    ret = pthread_mutex_init(&(*qp_cb)->qp_mutex, NULL);
    if (ret) {
        hccp_err("pthread_mutex_init failed, ret %d", ret);
        goto pthread_mutex_init_err;
    }

    (*qp_cb)->rdev_cb = cq_context->rdev_cb;
    RS_INIT_LIST_HEAD(&(*qp_cb)->mr_list);
    RS_INIT_LIST_HEAD(&(*qp_cb)->rem_mr_list);

    (*qp_cb)->eq_num = cq_context->eq_num;
    (*qp_cb)->channel = cq_context->channel;
    (*qp_cb)->ib_send_cq = cq_context->ib_send_cq;
    (*qp_cb)->ib_recv_cq = cq_context->ib_recv_cq;
    (*qp_cb)->send_event = cq_context->send_event;
    (*qp_cb)->recv_event = cq_context->recv_event;
    (*qp_cb)->num_recv_cq_events = 0;
    (*qp_cb)->num_send_cq_events = 0;
    (*qp_cb)->srq_context = cq_context->srq_context;
    (*qp_cb)->state = RS_QP_STATUS_DISCONNECT;
    (*qp_cb)->ib_pd = cq_context->rdev_cb->ib_pd;
    (*qp_cb)->tx_depth = qp_init_attr->cap.max_send_wr;
    (*qp_cb)->rx_depth = qp_init_attr->cap.max_recv_wr;
    (*qp_cb)->qos_attr.tc = (RS_ROCE_DSCP_33 & RS_DSCP_MASK) << RS_DSCP_OFF;
    (*qp_cb)->qos_attr.sl = RS_ROCE_4_SL;
    (*qp_cb)->timeout = RS_QP_ATTR_TIMEOUT;
    (*qp_cb)->retry_cnt = RS_QP_ATTR_RETRY_CNT;

    return 0;

pthread_mutex_init_err:
    free(*qp_cb);
    (*qp_cb) = NULL;
    return ret;
}

RS_ATTRI_VISI_DEF int rs_create_cq_event(struct rs_cq_context *cq_context, struct cq_attr *attr)
{
    int ret;
    cq_context->channel = rs_ibv_create_comp_channel(cq_context->rdev_cb->ib_ctx);

    if (cq_context->channel == NULL) {
        hccp_err("ibv_create_comp_channel failed, ret %d, errno(%d)", -EINVAL, errno);
        return -EINVAL;
    }

    hccp_info("comp channel fd[%d].", cq_context->channel->fd);
    ret = rs_epoll_ctl(cq_context->rdev_cb->rs_cb->conn_cb.epollfd, EPOLL_CTL_ADD,
        cq_context->channel->fd, EPOLLIN | EPOLLRDHUP);
#ifndef CA_CONFIG_LLT
    if (ret) {
        hccp_err("add channel fd failed ret %d", ret);
        goto rs_cq_epoll_ctl_err;
    }
#endif

    ret = rs_drv_create_cq_event(cq_context, attr);
    if (ret) {
        hccp_err("create drv cq event failed:%d", ret);
        goto rs_cq_create_err;
    }

    return ret;
rs_cq_create_err:
    ret = rs_epoll_ctl(cq_context->rdev_cb->rs_cb->conn_cb.epollfd, EPOLL_CTL_DEL, cq_context->channel->fd,
        EPOLLIN | EPOLLRDHUP);
#ifndef CA_CONFIG_LLT
    if (ret) {
        hccp_err("del channel fd failed ret %d", ret);
    }
#endif
rs_cq_epoll_ctl_err:
    if (cq_context->channel != NULL) {
        rs_ibv_destroy_comp_channel(cq_context->channel);
        cq_context->channel = NULL;
    }
    return ret;
}

RS_ATTRI_VISI_DEF int rs_cq_create(unsigned int phy_id, unsigned int rdev_index, struct cq_attr *attr)
{
    int ret;
    struct rs_rdev_cb *rdev_cb = NULL;
    struct rs_cq_context *cq_context = NULL;

    ret = rs_query_rdev_cb(phy_id, rdev_index, &rdev_cb);
    if (ret) {
        hccp_err("rs_query_rdev_cb phy_id[%u] rdev_index[%u], ret %d", phy_id, rdev_index, ret);
        return ret;
    }

    cq_context = calloc(1, sizeof(struct rs_cq_context));
    if (cq_context == NULL) {
        return -ENOMEM;
    }
    cq_context->rdev_cb = rdev_cb;
    cq_context->eq_num = 0;
    if (attr->send_channel == NULL && attr->recv_channel == NULL) {
        if (*attr->ib_send_cq == NULL && *attr->ib_recv_cq != NULL) {
            // 只创建sq cq
            cq_context->cq_create_mode = RS_SQ_CQ_CREATE;
            cq_context->ib_recv_cq = *attr->ib_recv_cq;
            cq_context->srq_context = attr->srq_context;
        } else {
            // 创建sq&rq cq
            cq_context->cq_create_mode = RS_NORMAL_CQ_CREATE;
        }
        ret = rs_create_cq_event(cq_context, attr);
        if (ret) {
            hccp_err("create cq event failed:%d", ret);
            goto rs_cq_create_err;
        }
    } else if (attr->send_channel != NULL && attr->recv_channel != NULL) {
        // 使用输入comp channel创建sq&rq
        ret = rs_drv_create_cq_with_channel(cq_context, attr);
        if (ret) {
            hccp_err("create drv cq with channel failed:%d", ret);
            goto rs_cq_create_err;
        }
    } else {
        hccp_err("rs create cq failed, send_channel or recv_channel is NULL.");
        ret = -EPERM;
        goto rs_cq_create_err;
    }

    *attr->qp_context = cq_context;
    return 0;

rs_cq_create_err:
    free(cq_context);
    cq_context = NULL;

    return ret;
}

RS_ATTRI_VISI_DEF int rs_cq_destroy(unsigned int phy_id, unsigned int rdev_index, struct cq_attr *attr)
{
    int ret;
    struct rs_rdev_cb *rdev_cb = NULL;
    struct rs_cq_context *cq_context = NULL;

    ret = rs_query_rdev_cb(phy_id, rdev_index, &rdev_cb);
    CHK_PRT_RETURN(ret, hccp_err("rs_query_rdev_cb phy_id[%u] rdev_index[%u], ret %d", phy_id, rdev_index, ret), ret);

    cq_context = *attr->qp_context;

    ret = rs_drv_destroy_cq_event(cq_context);
    if (ret) {
        hccp_err("rs_drv_destroy_cq_event failed ret %d", ret);
    }

    if (cq_context->channel != NULL) {
        ret = rs_epoll_ctl(rdev_cb->rs_cb->conn_cb.epollfd, EPOLL_CTL_DEL, cq_context->channel->fd,
            EPOLLIN | EPOLLRDHUP);
#ifndef CA_CONFIG_LLT
            if (ret) {
                hccp_err("del channel fd failed ret %d", ret);
            }
#endif
        rs_ibv_destroy_comp_channel(cq_context->channel);
        cq_context->channel = NULL;
    }

    free(cq_context);
    cq_context = NULL;

    return ret;
}

RS_ATTRI_VISI_DEF int rs_normal_qp_create(unsigned int phy_id, unsigned int rdev_index,
    struct ibv_qp_init_attr *qp_init_attr, struct rs_qp_resp *qp_resp, void **qp)
{
    struct rs_cq_context *cq_context = NULL;
    struct rs_rdev_cb *rdev_cb = NULL;
    struct rs_qp_cb *qp_cb = NULL;
    int ret;

    CHK_PRT_RETURN(qp_resp == NULL, hccp_err("qp_resp is NULL!"), -EINVAL);
    ret = rs_query_rdev_cb(phy_id, rdev_index, &rdev_cb);
    CHK_PRT_RETURN(ret, hccp_err("rs_query_rdev_cb phy_id[%u] rdev_index[%u], ret %d",
        phy_id, rdev_index, ret), ret);

    CHK_PRT_RETURN(qp_init_attr == NULL, hccp_err("qp_init_attr is NULL!"), -EINVAL);

    cq_context = qp_init_attr->qp_context;
    CHK_PRT_RETURN(cq_context == NULL, hccp_err("cq_context is NULL!"), -EINVAL);
    CHK_PRT_RETURN(rdev_cb != cq_context->rdev_cb, hccp_err("rs_query_rdev_cb phy_id[%u] rdev_index[%u],"
        "rdev_cb is invalid.", phy_id, rdev_index), -EINVAL);

    ret = rs_build_up_qpcb(cq_context, qp_init_attr, &qp_cb);
    CHK_PRT_RETURN(ret, hccp_err("rs_build_up_qpcb failed, ret:%d", ret), ret);

    ret = rs_drv_normal_qp_create(qp_cb, qp_init_attr);
    if (ret) {
        hccp_err("create drv qp create failed:%d", ret);
        goto create_qp_err;
    }

    RS_PTHREAD_MUTEX_LOCK(&rdev_cb->rdev_mutex);
    rs_list_add_tail(&qp_cb->list, &rdev_cb->qp_list);
    RS_PTHREAD_MUTEX_ULOCK(&rdev_cb->rdev_mutex);
    rdev_cb->qp_cnt++;
    *qp = qp_cb->ib_qp;
    qp_resp->qpn = (unsigned int)qp_cb->qp_info_lo.qpn;
    qp_resp->gid_idx = (unsigned int)qp_cb->qp_info_lo.gid_idx;
    qp_resp->psn = (unsigned int)qp_cb->qp_info_lo.psn;
    qp_resp->gid = qp_cb->qp_info_lo.gid;

    hccp_info("qp %d create qp.", qp_resp->qpn);

    return 0;

create_qp_err:
    pthread_mutex_destroy(&qp_cb->qp_mutex);
    free(qp_cb);
    qp_cb = NULL;
    return ret;
}

RS_ATTRI_VISI_DEF int rs_normal_qp_destroy(unsigned int phy_id, unsigned int rdev_index, unsigned int qpn)
{
    int ret;
    struct rs_qp_cb *qp_cb = NULL;
    struct rs_mr_cb *mr_tmp = NULL;
    struct rs_mr_cb *mr_tmp2 = NULL;

    RS_QP_PARA_CHECK(phy_id);
    ret = rs_qpn2qpcb(phy_id, rdev_index, qpn, &qp_cb);
    CHK_PRT_RETURN(ret || qp_cb == NULL, hccp_err("get qp cb failed qpn %u, ret %d", qpn, ret), ret);

    RS_PTHREAD_MUTEX_LOCK(&qp_cb->rdev_cb->rdev_mutex);
    rs_list_del(&qp_cb->list);
    RS_PTHREAD_MUTEX_ULOCK(&qp_cb->rdev_cb->rdev_mutex);
    rs_ibv_ack_cq_events(qp_cb->ib_send_cq, qp_cb->num_send_cq_events);
    rs_ibv_ack_cq_events(qp_cb->ib_recv_cq, qp_cb->num_recv_cq_events);

    RS_PTHREAD_MUTEX_LOCK(&qp_cb->qp_mutex);
    RS_LIST_GET_HEAD_ENTRY(mr_tmp, mr_tmp2, &qp_cb->mr_list, list, struct rs_mr_cb);
    for (; (&mr_tmp->list) != &qp_cb->mr_list;
        mr_tmp = mr_tmp2, mr_tmp2 = list_entry(mr_tmp2->list.next, struct rs_mr_cb, list)) {
        if (mr_tmp->ib_mr != qp_cb->rdev_cb->notify_mr) {
            (void)rs_drv_mr_dereg(mr_tmp->ib_mr);
        }
        rs_list_del(&mr_tmp->list);
        free(mr_tmp);
        mr_tmp = NULL;
    }

    RS_LIST_GET_HEAD_ENTRY(mr_tmp, mr_tmp2, &qp_cb->rem_mr_list, list, struct rs_mr_cb);
    for (; (&mr_tmp->list) != &qp_cb->rem_mr_list;
        mr_tmp = mr_tmp2, mr_tmp2 = list_entry(mr_tmp2->list.next, struct rs_mr_cb, list)) {
        rs_list_del(&mr_tmp->list);
        free(mr_tmp);
        mr_tmp = NULL;
    }
    RS_PTHREAD_MUTEX_ULOCK(&qp_cb->qp_mutex);

    // destroy qp
    rs_drv_qp_destroy(qp_cb);

    qp_cb->rdev_cb->qp_cnt--;

    pthread_mutex_destroy(&qp_cb->qp_mutex);
    hccp_info("qp %d destroy qp, send wr[%u].", qpn, qp_cb->send_wr_num);

    free(qp_cb);
    qp_cb = NULL;
    return ret;
}

RS_ATTRI_VISI_DEF int rs_create_comp_channel(unsigned int phy_id, unsigned int rdev_index, void** comp_channel)
{
    int ret;
    unsigned int chip_id;

    struct rs_rdev_cb *rdev_cb = NULL;

    CHK_PRT_RETURN(comp_channel == NULL || phy_id >= RS_MAX_DEV_NUM,
        hccp_err("param err, NULL pointer or phy_id:%u >= [%d]", phy_id, RS_MAX_DEV_NUM), -EINVAL);

    ret = rsGetLocalDevIDByHostDevID(phy_id, &chip_id);
    CHK_PRT_RETURN(ret,
        hccp_err("rs_create_comp_channel rsGetLocalDevIDByHostDevID phy_id[%u] invalid, ret %d", phy_id, ret), ret);

    ret = rs_rdev2rdev_cb(chip_id, rdev_index, &rdev_cb);
    CHK_PRT_RETURN(ret || rdev_cb == NULL, hccp_err("rs_rdev2rdev_cb for chip_id[%u] failed, ret %d",
        chip_id, ret), ret);

    *comp_channel = (void *)rs_ibv_create_comp_channel(rdev_cb->ib_ctx);
    if (*comp_channel == NULL) {
        hccp_err("rs_ibv_create_comp_channel failed, errno(%d)", errno);
        return -EOPENSRC;
    }
    hccp_info("create comp channel success!");
    return 0;
}

RS_ATTRI_VISI_DEF int rs_destroy_comp_channel(void* comp_channel)
{
    int ret;
    struct ibv_comp_channel *rs_comp_channel = (struct ibv_comp_channel *)comp_channel;

    ret = rs_ibv_destroy_comp_channel(rs_comp_channel);
    CHK_PRT_RETURN(ret, hccp_err("rs_destroy_comp_channel failed."), ret);
    hccp_info("destroy comp channel success!");

    return 0;
}

RS_ATTRI_VISI_DEF int rs_create_srq(unsigned int phy_id, unsigned int rdev_index, struct srq_attr *attr)
{
    int ret;
    struct rs_rdev_cb *rdev_cb = NULL;
    struct rs_cq_context *cq_context = NULL;

    CHK_PRT_RETURN(attr == NULL || phy_id >= RS_MAX_DEV_NUM,
        hccp_err("param err, NULL pointer or phy_id:%u >= [%d]", phy_id, RS_MAX_DEV_NUM), -EINVAL);

    ret = rs_query_rdev_cb(phy_id, rdev_index, &rdev_cb);
    CHK_PRT_RETURN(ret, hccp_err("rs_query_rdev_cb phy_id[%u] rdev_index[%u], ret %d", phy_id, rdev_index, ret), ret);

    cq_context = calloc(1, sizeof(struct rs_cq_context));
    if (cq_context == NULL) {
        return -ENOMEM;
    }

    cq_context->rdev_cb = rdev_cb;
    cq_context->eq_num = 0;
    cq_context->cq_create_mode = RS_SRQ_CQ_CREATE;
    *attr->context = cq_context;

    struct cq_attr cq_attr = {0};
    cq_attr.recv_cq_depth = attr->cq_depth;
    cq_attr.recv_cq_event_id = attr->srq_event_id;
    cq_attr.ib_recv_cq = attr->ib_recv_cq;
    // 创建srq cq
    ret = rs_create_cq_event(cq_context, &cq_attr);
    if (ret) {
        hccp_err("rs_create_cq_event create cq failed! ret:%d", ret);
        goto create_cq_event_err;
    }
    cq_context->ib_srq_cq = *attr->ib_recv_cq;


    struct ibv_srq_init_attr srq_init_attr = {
        .attr = {
            .max_wr  = attr->srq_depth,
            .max_sge = attr->max_sge
        }
    };
    hccp_info("max_wr [%u], max_sge[%u]", srq_init_attr.attr.max_wr, srq_init_attr.attr.max_sge);

    // 创建srq
    *attr->ib_srq = rs_ibv_create_srq(rdev_cb->ib_pd, &srq_init_attr);
    if (*attr->ib_srq == NULL) {
        hccp_err("rs_ibv_create_srq failed.");
        ret = -EOPENSRC;
        goto create_srq_err;
    }
    hccp_info("create srq success!");

    return 0;
create_cq_event_err:
create_srq_err:
    cq_attr.qp_context = attr->context;
    rs_cq_destroy(phy_id, rdev_index, &cq_attr);

    return ret;
}

RS_ATTRI_VISI_DEF int rs_destroy_srq(unsigned int phy_id, unsigned int rdev_index, struct srq_attr *attr)
{
    int ret;

    CHK_PRT_RETURN(*attr->context == NULL || *attr->ib_srq == NULL|| phy_id >= RS_MAX_DEV_NUM,
        hccp_err("param err, NULL pointer or phy_id:%u >= [%d]", phy_id, RS_MAX_DEV_NUM), -EINVAL);

    struct cq_attr cq_attr = {0};
    struct rs_cq_context *cq_context = *attr->context;
    cq_attr.qp_context = attr->context;

    rs_ibv_ack_cq_events(cq_context->ib_srq_cq, cq_context->num_recv_cq_events);

    // 销毁srq cq
    ret = rs_cq_destroy(phy_id, rdev_index, &cq_attr);
    CHK_PRT_RETURN(ret, hccp_err("rs_cq_destroy destroy cq failed! ret:%d", ret), ret);

    ret = rs_ibv_destroy_srq(*attr->ib_srq);
    CHK_PRT_RETURN(ret, hccp_err("rs_ibv_destroy_srq failed."), ret);

    return 0;
}

RS_ATTRI_VISI_DEF int rs_get_lite_support(unsigned int phy_id, unsigned int rdev_index, int *support_lite)
{
    int ret;
    unsigned int chip_id;
    struct rs_rdev_cb *rdev_cb = NULL;

    RS_CHECK_POINTER_NULL_RETURN_INT(support_lite);

    CHK_PRT_RETURN(phy_id >= RS_MAX_DEV_NUM, hccp_err("rs set param error ! phy_id:%u", phy_id), -EINVAL);
    ret = rsGetLocalDevIDByHostDevID(phy_id, &chip_id);
    CHK_PRT_RETURN(ret, hccp_err("phy_id[%u] invalid, ret %d", phy_id, ret), ret);

    ret = rs_rdev2rdev_cb(chip_id, rdev_index, &rdev_cb);
    CHK_PRT_RETURN(ret || rdev_cb == NULL, hccp_err("rs_rdev2rdev_cb for chip_id[%u] failed, ret %d",
        chip_id, ret), ret);

    rdev_cb->support_lite = 1;
    *support_lite = rdev_cb->support_lite;

    return 0;
}

RS_ATTRI_VISI_DEF int rs_get_lite_rdev_cap(
    unsigned int phy_id, unsigned int rdev_index, struct lite_rdev_cap_resp *resp)
{
    int ret;
    unsigned int chip_id;
    struct rs_rdev_cb *rdev_cb = NULL;

    RS_CHECK_POINTER_NULL_RETURN_INT(resp);

    CHK_PRT_RETURN(phy_id >= RS_MAX_DEV_NUM, hccp_err("rs set param error ! phy_id:%u", phy_id), -EINVAL);
    ret = rsGetLocalDevIDByHostDevID(phy_id, &chip_id);
    CHK_PRT_RETURN(ret, hccp_err("phy_id[%u] invalid, ret %d", phy_id, ret), ret);

    ret = rs_rdev2rdev_cb(chip_id, rdev_index, &rdev_cb);
    CHK_PRT_RETURN(ret || rdev_cb == NULL, hccp_err("rs_rdev2rdev_cb for chip_id[%u] failed, ret %d",
        chip_id, ret), ret);

    ret = rs_ibv_exp_query_device(rdev_cb->ib_ctx, &resp->cap);
    CHK_PRT_RETURN(ret, hccp_err("rs_ibv_exp_query_device for phy_id[%u] failed, ret %d", phy_id, ret), ret);

    ret = memcpy_s(resp, sizeof(struct dev_cap_info), (void *)&resp->cap, sizeof(resp->cap));
    if (ret) {
        hccp_err("memcpy_s failed, ret:%d, src_len:%u, dst_len:%u",
            ret,
            (unsigned int)sizeof(resp->cap),
            (unsigned int)sizeof(struct dev_cap_info));
        return ret;
    }

    return 0;
}

RS_ATTRI_VISI_DEF int rs_get_lite_qp_cq_attr(
    unsigned int phy_id, unsigned int rdev_index, unsigned int qpn, struct lite_qp_cq_attr_resp *resp)
{
    int ret;
    struct rs_qp_cb *qp_cb = NULL;

    RS_CHECK_POINTER_NULL_RETURN_INT(resp);

    RS_QP_PARA_CHECK(phy_id);
    ret = rs_qpn2qpcb(phy_id, rdev_index, qpn, &qp_cb);
    CHK_PRT_RETURN(ret || qp_cb == NULL, hccp_err("get qp cb failed qpn %u, ret %d", qpn, ret), ret);

    ret = memcpy_s(resp, sizeof(struct lite_qp_cq_attr_resp), (void *)&qp_cb->qp_resp, sizeof(qp_cb->qp_resp));
    if (ret) {
        hccp_err("memcpy_s failed, ret:%d, src_len:%u, dst_len:%u",
            ret,
            (unsigned int)sizeof(qp_cb->qp_resp),
            (unsigned int)sizeof(struct lite_qp_cq_attr_resp));
        return ret;
    }

    return 0;
}

RS_ATTRI_VISI_DEF int rs_get_lite_mem_attr(
    unsigned int phy_id, unsigned int rdev_index, unsigned int qpn, struct lite_mem_attr_resp *resp)
{
    int ret;
    struct rs_qp_cb *qp_cb = NULL;

    RS_CHECK_POINTER_NULL_RETURN_INT(resp);

    RS_QP_PARA_CHECK(phy_id);
    ret = rs_qpn2qpcb(phy_id, rdev_index, qpn, &qp_cb);
    CHK_PRT_RETURN(ret != 0 || qp_cb == NULL, hccp_err("get qp cb failed qpn %u, ret %d", qpn, ret), ret);

    ret = memcpy_s(resp, sizeof(struct lite_mem_attr_resp), (void *)&qp_cb->mem_resp, sizeof(qp_cb->mem_resp));
    if (ret) {
        hccp_err("memcpy_s failed, ret:%d, src_len:%u, dst_len:%u",
            ret,
            (unsigned int)sizeof(qp_cb->mem_resp),
            (unsigned int)sizeof(struct lite_mem_attr_resp));
        return ret;
    }

    return 0;
}

STATIC void rs_get_mr_info(
    struct rs_qp_cb *qp_cb, struct lite_mr_info *mr, uint32_t max_mr_num, struct rs_list_head *mr_list)
{
    struct rs_mr_cb *mr_tmp = NULL;
    struct rs_mr_cb *mr_tmp2 = NULL;
    uint32_t i = 0;

    RS_PTHREAD_MUTEX_LOCK(&qp_cb->qp_mutex);
    RS_LIST_GET_HEAD_ENTRY(mr_tmp, mr_tmp2, mr_list, list, struct rs_mr_cb);
    for (; (&mr_tmp->list) != mr_list;
        mr_tmp = mr_tmp2, mr_tmp2 = list_entry(mr_tmp2->list.next, struct rs_mr_cb, list)) {
        if (i < max_mr_num) {
            mr[i].key = mr_tmp->mr_info.rkey;
            mr[i].addr = mr_tmp->mr_info.addr;
            mr[i].len = mr_tmp->mr_info.len;
            i++;
        } else {
            break;
        }
    }

    RS_PTHREAD_MUTEX_ULOCK(&qp_cb->qp_mutex);
}

RS_ATTRI_VISI_DEF int rs_get_lite_connected_info(
    unsigned int phy_id, unsigned int rdev_index, unsigned int qpn, struct lite_connected_info_resp *resp)
{
    int ret;
    struct rs_qp_cb *qp_cb = NULL;

    RS_CHECK_POINTER_NULL_RETURN_INT(resp);
    RS_QP_PARA_CHECK(phy_id);
    ret = rs_qpn2qpcb(phy_id, rdev_index, qpn, &qp_cb);
    CHK_PRT_RETURN(ret || qp_cb == NULL, hccp_err("get qp cb failed qpn %u, ret %d", qpn, ret), ret);

    resp->state = (unsigned int)qp_cb->state;
    if (resp->state == RS_QP_STATUS_CONNECTED) {
        rs_get_mr_info(qp_cb, &resp->local_mr[0], RA_MR_MAX_NUM, &qp_cb->mr_list);
        rs_get_mr_info(qp_cb, &resp->rem_mr[0], RA_MR_MAX_NUM, &qp_cb->rem_mr_list);
        resp->qos_attr.sl = qp_cb->qos_attr.sl;
        resp->qos_attr.tc = qp_cb->qos_attr.tc;
    }

    return 0;
}

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

unsigned int gRsSendWrNum = 0;

STATIC void RsBufPrint(char *addr, int len)
{
    int i;

    for (i = 0; i < len; i++) {
        hccp_info("0x%02x ", *(addr + i));
    }
}

STATIC int RsGetQpcb(struct rs_rdev_cb *rdevCb, uint32_t qpn, struct rs_qp_cb **qpCb)
{
    struct rs_qp_cb *qpCbTmp = NULL;
    struct rs_qp_cb *qpCbTmp2 = NULL;

    RS_LIST_GET_HEAD_ENTRY(qpCbTmp, qpCbTmp2, &rdevCb->qp_list, list, struct rs_qp_cb);
    for (; (&qpCbTmp->list) != &rdevCb->qp_list;
        qpCbTmp = qpCbTmp2, qpCbTmp2 = list_entry(qpCbTmp2->list.next, struct rs_qp_cb, list)) {
        if (qpCbTmp->ib_qp->qp_num == qpn) {
            *qpCb = qpCbTmp;
            return 0;
        }
    }

    *qpCb = NULL;
    hccp_err("qp_cb for qp %u do not available!", qpn);

    return -ENODEV;
}

int RsQpn2qpcb(unsigned int phyId, unsigned int rdevIndex, uint32_t qpn, struct rs_qp_cb **qpCb)
{
    int ret;
    unsigned int chipId;
    struct rs_cb *rsCb = NULL;
    struct rs_rdev_cb *rdevCb = NULL;

    CHK_PRT_RETURN(phyId >= RS_MAX_DEV_NUM, hccp_err("rs set param error! phy_id:%u", phyId), -EINVAL);

    ret = rsGetLocalDevIDByHostDevID(phyId, &chipId);
    CHK_PRT_RETURN(ret, hccp_err("rs_qpn2qpcb rsGetLocalDevIDByHostDevID phy_id[%u] invalid, ret:%d",
        phyId, ret), ret);

    ret = RsDev2rscb(chipId, &rsCb, false);
    CHK_PRT_RETURN(ret, hccp_err("rs_qpn2qpcb get rs_cb failed, ret:%d", ret), -ENODEV);

    ret = RsGetRdevCb(rsCb, rdevIndex, &rdevCb);
    CHK_PRT_RETURN(ret, hccp_err("rs_get_rdev_cb failed! ret:%d, rdevIndex:%u", ret, rdevIndex), ret);

    ret = RsGetQpcb(rdevCb, qpn, qpCb);
    CHK_PRT_RETURN(ret, hccp_err("rs_get_qpcb failed! ret:%d, qpn:%u", ret, qpn), ret);

    return 0;
}

STATIC int RsGetMrcb(struct rs_qp_cb *qpCb, uint64_t addr, struct rs_mr_cb **mrCb,
    struct rs_list_head *mrList)
{
    struct rs_mr_cb *mrTmp = NULL;
    struct rs_mr_cb *mrTmp2 = NULL;

    RS_PTHREAD_MUTEX_LOCK(&qpCb->qp_mutex);
    RS_LIST_GET_HEAD_ENTRY(mrTmp, mrTmp2, mrList, list, struct rs_mr_cb);
    for (; (&mrTmp->list) != mrList;
        mrTmp = mrTmp2, mrTmp2 = list_entry(mrTmp2->list.next, struct rs_mr_cb, list)) {
        if ((mrTmp->mr_info.addr <= addr) && (addr < mrTmp->mr_info.addr + mrTmp->mr_info.len)) {
            *mrCb = mrTmp;
            RS_PTHREAD_MUTEX_ULOCK(&qpCb->qp_mutex);
            return 0;
        }
    }

    *mrCb = NULL;
    RS_PTHREAD_MUTEX_ULOCK(&qpCb->qp_mutex);

    hccp_info("cannot find mrcb for addr@0x%lx !", addr);

    return -ENODEV;
}

STATIC void *RsNotifyMrListAdd(struct rs_qp_cb *qpCb, const char *buf)
{
    int ret;
    struct rs_mr_cb *notifyMrCb;

    notifyMrCb = calloc(1, sizeof(struct rs_mr_cb));
    CHK_PRT_RETURN(notifyMrCb == NULL, hccp_err("notify_mr_cb calloc failed"), NULL);
    ret = memcpy_s(&notifyMrCb->mr_info, sizeof(struct rs_mr_info),
                   &((const struct rs_qp_info *)buf)->notify_mr, sizeof(struct rs_mr_info));
    if (ret) {
        hccp_err("memcpy_s failed, ret:%d, src_len:%u, dst_len:%u",
            ret, sizeof(struct rs_mr_info), sizeof(struct rs_mr_info));
        free(notifyMrCb);
        notifyMrCb = NULL;
        return NULL;
    }

    hccp_info("qpn is %d, rdevIndex:%u, chipId %u, recv notify va is 0x%llx, notify size is %llu",
        qpCb->qp_info_lo.qpn, qpCb->rdev_cb->rdev_index, qpCb->rdev_cb->rs_cb->chip_id,
        notifyMrCb->mr_info.addr, notifyMrCb->mr_info.len);

    RsListAddTail(&notifyMrCb->list, &qpCb->rem_mr_list);

    return notifyMrCb;
}

STATIC int RsQpStateModify(struct rs_qp_cb *qpCb)
{
    struct ibv_qp_init_attr initAttr = { 0 };
    struct ibv_qp_attr attr = { 0 };
    enum ibv_qp_state state;
    int ret;

    // see ib_modify_qp_is_ok for status modify, only support modify qp from INIT to RTR
    ret = RsIbvQueryQp(qpCb->ib_qp, &attr, IBV_QP_STATE, &initAttr);
    if (ret != 0) {
        hccp_warn("rs_ibv_query_qp qpn:%d unsuccessful, ret:%d", qpCb->qp_info_lo.qpn, ret);
        state = IBV_QPS_UNKNOWN;
    } else {
        state = attr.qp_state;
    }

    // disallow modify qp from IBV_QPS_RTS to IBV_QPS_RTS
    if (state == IBV_QPS_RTS) {
        hccp_err("qpn:%d disallow modify from %d", qpCb->qp_info_lo.qpn, state);
        return -EINVAL;
    }

    hccp_info("qpn:%d state:%d start modify", qpCb->qp_info_lo.qpn, state);

    // modify qp from others to RESET
    if (state != IBV_QPS_RESET && state != IBV_QPS_INIT && state != IBV_QPS_RTR) {
        ret = RsDrvQpStateModifytoReset(qpCb);
        CHK_PRT_RETURN(ret, hccp_err("qpn:%d modify %d to reset failed, ret:%d", qpCb->qp_info_lo.qpn, state, ret),
            ret);
        state = IBV_QPS_RESET;
    }

    // modify qp from RESET to INIT
    if (state == IBV_QPS_RESET) {
        ret = RsDrvQpStateModifytoInit(qpCb, &attr);
        CHK_PRT_RETURN(ret, hccp_err("qpn:%d modify %d to init failed, ret %d", qpCb->qp_info_lo.qpn, state, ret),
            ret);
        state = IBV_QPS_INIT;
    }

    // modify qp from INIT to RTR
    if (state == IBV_QPS_INIT) {
        ret = RsDrvQpStateModifytoRtr(qpCb, &attr);
        CHK_PRT_RETURN(ret, hccp_err("qpn:%d modify %d to rtr failed, ret %d", qpCb->qp_info_lo.qpn, state, ret), ret);
        state = IBV_QPS_RTR;
    }

    // modify qp from RTR to RTS
    if (state == IBV_QPS_RTR) {
        ret = RsDrvQpStateModifytoRts(qpCb, &attr);
        CHK_PRT_RETURN(ret, hccp_err("qpn:%d modify %d to rts failed, ret %d", qpCb->qp_info_lo.qpn, state, ret), ret);
    }

    hccp_info("local qpn[%d] remote qpn[%d] modify succ", qpCb->qp_info_lo.qpn, qpCb->qp_info_rem.qpn);

    return 0;
}

STATIC int RsEpollRecvQpHandle(struct rs_qp_cb *qpCb, const char *bufTmp)
{
    int ret;
    float timeCost = 0.0;

    ret = memcpy_s(&qpCb->qp_info_rem, sizeof(struct rs_qp_info),
                   bufTmp, sizeof(struct rs_qp_info));
    CHK_PRT_RETURN(ret, hccp_err("memcpy_s failed[%d], dest size:%d, src size:%d", ret, sizeof(struct rs_qp_info),
        sizeof(struct rs_qp_info)), -ENOMEM);

    /* modify qp state to RTR/RTS */
    ret = RsQpStateModify(qpCb);
    CHK_PRT_RETURN(ret, hccp_err("rs_qp_state_modify local qpn[%d] remote qpn[%d] failed ret[%d]",
        qpCb->qp_info_lo.qpn, qpCb->qp_info_rem.qpn, ret), ret);

    RsGetCurTime(&qpCb->end_time);
    HccpTimeInterval(&qpCb->end_time, &qpCb->start_time, &timeCost);
    if (timeCost > RS_EXPECT_TIME_MAX) {
        hccp_warn("local qpn[%d] remote qpn [%d] connect success cost[%f] more than[%f]ms!", qpCb->qp_info_lo.qpn,
            qpCb->qp_info_rem.qpn, timeCost, RS_EXPECT_TIME_MAX);
    } else {
        hccp_info("local qpn[%d] remote qpn [%d] connect success! cost [%f] ms", qpCb->qp_info_lo.qpn,
            qpCb->qp_info_rem.qpn, timeCost);
    }

    hccp_info("qp [%d] state has been migrate to RTS!, qpCb state is %d", qpCb->qp_info_lo.qpn, qpCb->state);

    return 0;
}

STATIC void *RsEpollRecvMrHandle(struct rs_qp_cb *qpCb, const char *bufTmp)
{
    int ret;
    struct rs_mr_cb *mrCb;

    mrCb = calloc(1, sizeof(struct rs_mr_cb));
    CHK_PRT_RETURN(mrCb == NULL, hccp_err("mr_cb calloc failed"), NULL);
    ret = memcpy_s(&mrCb->mr_info, sizeof(struct rs_mr_info), bufTmp, sizeof(struct rs_mr_info));
    if (ret) {
        hccp_err("memcpy_s failed[%d], dest size:%u, src size:%u", ret, sizeof(struct rs_mr_info),
            sizeof(struct rs_mr_info));
        free(mrCb);
        mrCb = NULL;
        return NULL;
    }

    RsListAddTail(&mrCb->list, &qpCb->rem_mr_list);

    hccp_info("recv mr addr is 0x%llx", mrCb->mr_info.addr);
    hccp_info("recv mr len is %llu", mrCb->mr_info.len);

    return mrCb;
}

STATIC int RsCmdQpInfoHandle(struct rs_qp_cb *qpCb, unsigned int totalSize,
    const char *bufTmp, unsigned int curSize, bool *flag)
{
    int ret;
    CHK_PRT_RETURN((totalSize - curSize) < sizeof(struct rs_qp_info), hccp_info("qp_info remain size"
        "[%u] < size [%u], wait for next recv", totalSize - curSize, sizeof(struct rs_qp_info)), -EINVAL);

    ret = RsEpollRecvQpHandle(qpCb, bufTmp);
    CHK_PRT_RETURN(ret, hccp_err("rs_epoll_recv_qp_handle failed! ret[%d]", ret), ret);

    RsNotifyMrListAdd(qpCb, bufTmp);
    hccp_info("rs_notify_mr_list_add");

    *flag = true;
    hccp_info("qp_info cur_size(%u) len(%u) !", curSize, sizeof(struct rs_qp_info));

    return 0;
}

STATIC int RsCmdMrInfoHandle(struct rs_qp_cb *qpCb, unsigned int totalSize, const char *bufTmp,
    unsigned int curSize, bool *flag)
{
    CHK_PRT_RETURN((totalSize - curSize) < sizeof(struct rs_mr_info), hccp_info("mr_info remain size"
        "[%u] < size [%u], wait for next recv", totalSize - curSize, sizeof(struct rs_mr_info)), -EINVAL);

    (void)RsEpollRecvMrHandle(qpCb, bufTmp);

    *flag = true;

    hccp_info("mr_info cur_size(%u) len(%u) !", curSize, sizeof(struct rs_mr_info));

    return 0;
}

STATIC int RsCmdLenInfoHandle(struct rs_qp_cb *qpCb, unsigned int totalSize, const char *bufTmp,
    unsigned int curSize, bool *flag)
{
    CHK_PRT_RETURN((totalSize - curSize) < sizeof(struct rs_qp_len_info), hccp_info("len_info remain size"
        "[%u] < size [%u], wait for next recv", totalSize - curSize, sizeof(struct rs_qp_len_info)), -EINVAL);

    qpCb->expect_len = *((const uint32_t*)(bufTmp + sizeof(uint32_t)));

    *flag = true;

    return 0;
}

STATIC void RsEpollRecvHandleRemain(struct rs_qp_cb *qpCb, unsigned int totalSize,
    unsigned int curSize, bool flag, const char *bufTmp)
{
    int ret = 0;

    qpCb->remain_size = totalSize - curSize;
    if ((qpCb->remain_size > 0) && (flag == true)) {
        ret = memcpy_s(qpCb->qp_mr_buf, RS_BUF_SIZE, bufTmp, qpCb->remain_size);
        if (ret) {
            hccp_err("memcpy_s failed, ret:%d, remain_size:%u", ret, qpCb->remain_size);
            return;
        }
    }

    return;
}

STATIC void RsEpollRecvHandle(struct rs_qp_cb *qpCb, char *buf, int size)
{
    unsigned int totalSize = qpCb->remain_size + (unsigned int)size;
    char *bufTmp = (char *)qpCb->qp_mr_buf;
    unsigned int curSize = 0;
    bool flag = false;
    uint32_t cmd;
    int ret;

    hccp_info("Message for qp:%d, qpCb->remain_size:%u, size:%d", qpCb->qp_info_lo.qpn, qpCb->remain_size, size);
    ret = memcpy_s(qpCb->qp_mr_buf + qpCb->remain_size, RS_BUF_SIZE - qpCb->remain_size, buf, size);
    if (ret) {
        hccp_err("memcpy_s failed, ret:%d, remain_size:%u, size:%d", ret, qpCb->remain_size, size);
        return;
    }

    do {
        cmd = *((uint32_t *)bufTmp);
        switch (cmd) {
            case RS_CMD_QP_INFO:
                ret = RsCmdQpInfoHandle(qpCb, totalSize, bufTmp, curSize, &flag);
                if (ret) {
                    goto out;
                }

                curSize += sizeof(struct rs_qp_info);
                bufTmp = qpCb->qp_mr_buf + curSize;
                break;
            case RS_CMD_MR_INFO:
                ret = RsCmdMrInfoHandle(qpCb, totalSize, bufTmp, curSize, &flag);
                if (ret) {
                    goto out;
                }

                curSize += sizeof(struct rs_mr_info);
                bufTmp = qpCb->qp_mr_buf + curSize;
                break;
            case RS_CMD_LEN_INFO:
                ret = RsCmdLenInfoHandle(qpCb, totalSize, bufTmp, curSize, &flag);
                if (ret) {
                    goto out;
                }
                curSize += sizeof(struct rs_qp_len_info);
                bufTmp = qpCb->qp_mr_buf + curSize;
                break;
            default:
                hccp_warn("qp %d, unknown cmd(0x%x)!", qpCb->qp_info_lo.qpn, cmd);
                RsBufPrint(buf, size);
                return;
        }
    } while (curSize < totalSize);

out:
    RsEpollRecvHandleRemain(qpCb, totalSize, curSize, flag, bufTmp);
}

STATIC void RsQpMrRecvHandle(int fd, struct rs_qp_cb *qpCb)
{
    char buf[RS_BUF_SIZE];
    int size;
    int ret;

    RS_PTHREAD_MUTEX_LOCK(&qpCb->qp_mutex);

    size = RsSocketRecv(fd, buf, RS_BUF_SIZE - qpCb->remain_size);
    hccp_dbg("fd %d qpn %d read size = %d, qpCb->remain_size:%u", fd, qpCb->qp_info_lo.qpn, size, qpCb->remain_size);

    if (size > 0) {
        qpCb->recv_len += (uint32_t)size;
        RsEpollRecvHandle(qpCb, buf, size);
    } else if (size == 0) {
        hccp_dbg("fd %d read size = %d, remote fd has been closed, fd cannot use !", fd, size);
#ifdef CA_CONFIG_LLT
        qp_cb->state = RS_QP_STATUS_REM_FD_CLOSE;
#endif
    } else {
        ret = errno;
        hccp_dbg("no data available, errno:%d", ret);
    }
    RS_PTHREAD_MUTEX_ULOCK(&qpCb->qp_mutex);

    return;
}

STATIC int RsHandleQpMrEpollEvent(struct rs_rdev_cb *rdevCb, int fd)
{
    struct rs_qp_cb *qpCb;
    struct rs_qp_cb *qpCb2 = NULL;

    /* QP event, QP info exchange */
    RS_LIST_GET_HEAD_ENTRY(qpCb, qpCb2, &rdevCb->qp_list, list, struct rs_qp_cb);
    for (; (&qpCb->list) != &rdevCb->qp_list;
        qpCb = qpCb2, qpCb2 = list_entry(qpCb2->list.next, struct rs_qp_cb, list)) {
        if (qpCb->channel == NULL) {
            continue;
        }
        if (qpCb->srq_context != NULL && qpCb->srq_context->channel->fd == fd) {
            hccp_dbg("fd %d poll cq!", fd);
            RsDrvPollSrqCqHandle(qpCb);
            return 0;
        }
        if (fd == qpCb->channel->fd) {
            hccp_dbg("fd %d poll cq!", fd);
            RsDrvPollCqHandle(qpCb);
            return 0;
        }
    }
    return -ENODEV;
}

int RsEpollEventQpMrInHandle(struct rs_cb *rsCb, int fd)
{
    int ret;
    struct rs_rdev_cb *rdevCbTmp = NULL;
    struct rs_rdev_cb *rdevCbTmp2 = NULL;

    RS_LIST_GET_HEAD_ENTRY(rdevCbTmp, rdevCbTmp2, &rsCb->rdev_list, list, struct rs_rdev_cb);
    for (; (&rdevCbTmp->list) != &rsCb->rdev_list;
        rdevCbTmp = rdevCbTmp2, rdevCbTmp2 = list_entry(rdevCbTmp2->list.next, struct rs_rdev_cb, list)) {
            RS_PTHREAD_MUTEX_LOCK(&rdevCbTmp->rdev_mutex);
            ret = RsHandleQpMrEpollEvent(rdevCbTmp, fd);
            RS_PTHREAD_MUTEX_ULOCK(&rdevCbTmp->rdev_mutex);
            if (ret == 0) {
                return 0;
            }
    }
    return -ENODEV;
}

STATIC int RsMrInfoSync(struct rs_mr_cb *mrCb)
{
    int ret;

    hccp_info("mr state:%d, addr:0x%lx", mrCb->state, mrCb->mr_info.addr);

    CHK_PRT_RETURN(mrCb->state & RS_MR_STATE_SYNCED, hccp_warn("mr synced ! mr_cb->flag[%d] & [%d] != 0",
        mrCb->state, RS_MR_STATE_SYNCED), 0);

    /*
     * no socket available for MR_INFO exchange if allowed
     * need exchange when socket available!!!
     */
    CHK_PRT_RETURN(mrCb->qp_cb->conn_info == NULL, hccp_warn("no conn available !"), 0);

    CHK_PRT_RETURN(mrCb->qp_cb->state == RS_QP_STATUS_REM_FD_CLOSE, hccp_warn("remote qp fd closed,"
        "cann not use it anymore! status[%d](RS_QP_STATUS_REM_FD_CLOSE)", mrCb->qp_cb->state), -EFAULT);

    CHK_PRT_RETURN(mrCb->qp_cb->conn_info->connfd == RS_FD_INVALID, hccp_warn("rm info sync failed! fd not ready!"
        "connfd[%d](RS_FD_INVALID)", mrCb->qp_cb->conn_info->connfd), -ENETUNREACH);

    mrCb->mr_info.cmd = (unsigned int)RS_CMD_MR_INFO;
    ret = RsSocketSend(mrCb->qp_cb->conn_info->connfd, &mrCb->mr_info,
        sizeof(struct rs_mr_info));
    CHK_PRT_RETURN(ret != sizeof(struct rs_mr_info), hccp_err("mr_info send %d/%ld incomplete",
        ret, sizeof(struct rs_mr_info)), -EAGAIN);

    mrCb->qp_cb->send_len += (uint32_t)ret;
    mrCb->state |= RS_MR_STATE_SYNCED;
    hccp_info("after send mr state:%d, addr:0x%lx", mrCb->state, mrCb->mr_info.addr);

    return 0;
}

STATIC int RsMrPreReg(unsigned int phyId, struct rs_qp_cb *qpCb, struct rs_mr_cb *mrCb,
    struct rdma_mr_reg_info *mrRegInfo)
{
    struct roce_process_sign roceSign;
    int ret;
    unsigned int chipId;
    char *addr = mrRegInfo->addr;
    unsigned long long len = mrRegInfo->len;
    int access = mrRegInfo->access;

    if (qpCb->rdev_cb->rs_cb->hccp_mode == NETWORK_PEER_ONLINE || qpCb->rdev_cb->rs_cb->hccp_mode == NETWORK_ONLINE ||
        qpCb->is_exp == RS_NOT_EXP) {
        mrCb->ib_mr = RsDrvMrReg(qpCb->ib_pd, addr, len, access);
        CHK_PRT_RETURN(mrCb->ib_mr == NULL, hccp_err("rs_drv_mr_reg addr is NULL len[%lld] failed ",
            len), -EACCES);
    } else {
        // reg mr with backup phy_id
        if (qpCb->rdev_cb->backup_info.backup_flag) {
            phyId = qpCb->rdev_cb->backup_info.rdev_info.phy_id;
        }
        ret = rsGetLocalDevIDByHostDevID(phyId, &chipId);
        CHK_PRT_RETURN(ret, hccp_err("rsGetLocalDevIDByHostDevID failed, ret %d, phyid[%u]", ret, phyId), -EACCES);
        roceSign.tgid = qpCb->rdev_cb->rs_cb->p_rs_sign.tgid;
        roceSign.devid = chipId;
        roceSign.vfid = 0;
        ret = strcpy_s(roceSign.sign, PROCESS_RS_SIGN_LENGTH, qpCb->rdev_cb->rs_cb->p_rs_sign.sign);
        CHK_PRT_RETURN(ret, hccp_err("Invalid pid sign, ret(%d)", ret), -ESAFEFUNC);
        mrCb->ib_mr = RsDrvExpMrReg(qpCb->ib_pd, addr, len, access, roceSign);
        CHK_PRT_RETURN(mrCb->ib_mr == NULL, hccp_err("rs_drv_exp_mr_reg addr is NULL len[%lld] failed ",
            len), -EACCES);
    }

    mrCb->mr_info.cmd = (unsigned int)RS_CMD_MR_INFO;
    mrCb->mr_info.addr = (uintptr_t)addr;
    mrCb->mr_info.len = len;
    mrCb->mr_info.rkey = mrCb->ib_mr->rkey;

    RS_PTHREAD_MUTEX_LOCK(&qpCb->qp_mutex);
    RsListAddTail(&mrCb->list, &qpCb->mr_list);
    RS_PTHREAD_MUTEX_ULOCK(&qpCb->qp_mutex);

    qpCb->mr_num++;
    return 0;
}

STATIC int RsCallocMr(int num, struct rs_mr_cb **mrCb)
{
    CHK_PRT_RETURN(num <= 0, hccp_err("invalid num for mr calloc"), -EINVAL);

    *mrCb = calloc(num, sizeof(struct rs_mr_cb));
    CHK_PRT_RETURN((*mrCb) == NULL, hccp_err("calloc mr_cb failed"), -ENOMEM);
    return 0;
}

STATIC int RsCallocQpcb(int num, struct rs_qp_cb **qpCb)
{
    if (num <= 0) {
        return -EINVAL;
    }

    *qpCb = calloc(num, sizeof(struct rs_qp_cb));
    if ((*qpCb) == NULL) {
        return -ENOMEM;
    }

    return 0;
}

RS_ATTRI_VISI_DEF int RsMrReg(unsigned int phyId, unsigned int rdevIndex, unsigned int qpn,
    struct rdma_mr_reg_info *mrRegInfo)
{
    int ret;
    struct rs_qp_cb *qpCb = NULL;
    struct rs_mr_cb *mrCb = NULL;

    CHK_PRT_RETURN(mrRegInfo == NULL || mrRegInfo->addr == NULL || mrRegInfo->len == 0 ||
        phyId >= RS_MAX_DEV_NUM, hccp_err("param err, NULL pointer or phyId:%u >= [%d]", phyId, RS_MAX_DEV_NUM),
        -EINVAL);

    hccp_info("qpn[%u], len[0x%llx], access[%d]",
        qpn, mrRegInfo->len, mrRegInfo->access);

    ret = RsQpn2qpcb(phyId, rdevIndex, qpn, &qpCb);
    CHK_PRT_RETURN(ret, hccp_err("rs_qpn2qpcb qpn[%d] ret[%d] failed ", qpn, ret), ret);

    CHK_PRT_RETURN(qpCb->mr_num >= RS_MR_NUM_MAX, hccp_err("Exceeded the maximum MR limit %d",
        qpCb->mr_num), -EINVAL);

    ret = RsGetMrcb(qpCb, (uintptr_t)mrRegInfo->addr, &mrCb, &qpCb->mr_list);
    if (!ret) {
        hccp_warn("mr already registered");
        goto found;
    }

    ret = RsCallocMr(1, &mrCb);
    CHK_PRT_RETURN(ret, hccp_err("calloc mr failed"), ret);

    mrCb->qp_cb = qpCb;

    ret = RsMrPreReg(phyId, qpCb, mrCb, mrRegInfo);
    if (ret) {
        hccp_err("pre reg mr failed, qpn %u, ret %d", qpn, ret);
        goto reg_err;
    }

found:
    mrRegInfo->lkey = mrCb->ib_mr->lkey;
    mrRegInfo->rkey = mrCb->ib_mr->rkey;

    hccp_info("rs_mr_reg succ, state:%u", mrCb->state);
    return 0;

reg_err:
    free(mrCb);
    mrCb = NULL;

    return ret;
}

RS_ATTRI_VISI_DEF int RsMrDereg(unsigned int phyId, unsigned int rdevIndex, unsigned int qpn, char *addr)
{
    int ret;
    struct rs_qp_cb *qpCb = NULL;
    struct rs_mr_cb *mrCb = NULL;

    hccp_dbg("start rs_mr_dereg");
    RS_CHECK_POINTER_NULL_RETURN_INT(addr);
    CHK_PRT_RETURN(phyId >= RS_MAX_DEV_NUM, hccp_err("phy_id:%u >= [%d], is invalid", phyId, RS_MAX_DEV_NUM),
        -EINVAL);

    ret = RsQpn2qpcb(phyId, rdevIndex, qpn, &qpCb);
    CHK_PRT_RETURN(ret, hccp_err("rs_qpn2qpcb failed ret[%d]", ret), ret);

    CHK_PRT_RETURN(RsGetMrcb(qpCb, (uintptr_t)addr, &mrCb, &qpCb->mr_list), hccp_err("rs_get_mrcb failed "\
        "g_rs_send_wr_num[%u]", gRsSendWrNum), -EFAULT);

    ret = RsDrvMrDereg(mrCb->ib_mr);
    CHK_PRT_RETURN(ret, hccp_err("rs_drv_mr_dereg failed ret[%d] ", ret), -EACCES);

    RS_PTHREAD_MUTEX_LOCK(&qpCb->qp_mutex);
    RsListDel(&mrCb->list);
    free(mrCb);
    mrCb = NULL;
    RS_PTHREAD_MUTEX_ULOCK(&qpCb->qp_mutex);
    qpCb->mr_num--;

    hccp_dbg("qpn[%u] succ", qpn);

    return 0;
}

RS_ATTRI_VISI_DEF int RsRegisterMr(unsigned int phyId, unsigned int rdevIndex, struct rdma_mr_reg_info *mrRegInfo,
    void **mrHandle)
{
    RS_CHECK_POINTER_NULL_RETURN_INT(mrHandle);

    int ret;
    unsigned int chipId;
    struct rs_rdev_cb *rdevCb = NULL;
    struct ibv_mr *rsMrHandle = NULL;

    CHK_PRT_RETURN(mrRegInfo == NULL || mrRegInfo->addr == NULL || mrRegInfo->len == 0 ||
        phyId >= RS_MAX_DEV_NUM, hccp_err("param err, NULL pointer or phyId:%u >= [%d]", phyId,
        RS_MAX_DEV_NUM), -EINVAL);

    hccp_info("[rs_register_mr] len[0x%llx], access[%d]",
        mrRegInfo->len, mrRegInfo->access);

    ret = rsGetLocalDevIDByHostDevID(phyId, &chipId);
    CHK_PRT_RETURN(ret, hccp_err("rs_register_mr rsGetLocalDevIDByHostDevID phy_id[%u] invalid, ret %d",
        phyId, ret), ret);

    ret = RsRdev2rdevCb(chipId, rdevIndex, &rdevCb);
    CHK_PRT_RETURN(ret || rdevCb == NULL, hccp_err("rs_rdev2rdev_cb for chip_id[%u] failed, ret %d",
        chipId, ret), ret);

    *mrHandle = (void *)RsDrvMrReg(rdevCb->ib_pd, mrRegInfo->addr, mrRegInfo->len, mrRegInfo->access);
    if (*mrHandle == NULL) {
        hccp_warn("rs_drv_mr_reg addr is NULL len[%lld] access[%d] unsuccessful ", mrRegInfo->len,
            mrRegInfo->access);
        goto reg_err;
    }

    rsMrHandle = (struct ibv_mr *)*mrHandle;
    mrRegInfo->lkey = rsMrHandle->lkey;
    mrRegInfo->rkey = rsMrHandle->rkey;

    hccp_info("rs_register_mr succ");
    return ret;
reg_err:
    mrRegInfo->lkey = 0;

    return 0;
}

STATIC int RsInitTypicalMrCb(unsigned int phyId, struct rdma_mr_reg_info *mrRegInfo, struct rs_rdev_cb *devCb,
                                 struct rs_mr_cb *mrCb)
{
    unsigned long long len = mrRegInfo->len;
    char *addr = (char *)mrRegInfo->addr;
    int access = mrRegInfo->access;
    struct roce_process_sign roceSign;
    unsigned int chipId;
    int ret;

    if (devCb->rs_cb->hccp_mode == NETWORK_PEER_ONLINE || devCb->rs_cb->hccp_mode == NETWORK_ONLINE) {
        mrCb->ib_mr = RsDrvMrReg(devCb->ib_pd, addr, len, access);
        CHK_PRT_RETURN(mrCb->ib_mr == NULL, hccp_err("rs_drv_mr_reg addr is NULL len[%lld] failed", len), -EACCES);
    } else {
        // reg mr with backup phy_id
        if (devCb->backup_info.backup_flag) {
            phyId = devCb->backup_info.rdev_info.phy_id;
        }
        ret = rsGetLocalDevIDByHostDevID(phyId, &chipId);
        CHK_PRT_RETURN(ret, hccp_err("rsGetLocalDevIDByHostDevID failed, ret %d, phyid[%u]", ret, phyId), -EACCES);
        roceSign.tgid = devCb->rs_cb->p_rs_sign.tgid;
        roceSign.devid = chipId;
        roceSign.vfid = 0;
        ret = strcpy_s(roceSign.sign, PROCESS_RS_SIGN_LENGTH, devCb->rs_cb->p_rs_sign.sign);
        CHK_PRT_RETURN(ret, hccp_err("Invalid pid sign, ret(%d)", ret), -ESAFEFUNC);
        mrCb->ib_mr = RsDrvExpMrReg(devCb->ib_pd, addr, len, access, roceSign);
        CHK_PRT_RETURN(mrCb->ib_mr == NULL, hccp_err("rs_drv_exp_mr_reg addr is NULL len[%lld] failed", len), -EACCES);
    }

    mrCb->mr_info.addr = (uintptr_t)addr;
    mrCb->mr_info.len = len;
    mrCb->mr_info.rkey = mrCb->ib_mr->rkey;

    RS_PTHREAD_MUTEX_LOCK(&devCb->rdev_mutex);
    RsListAddTail(&mrCb->list, &devCb->typical_mr_list);
    RS_PTHREAD_MUTEX_ULOCK(&devCb->rdev_mutex);

    return 0;
}

RS_ATTRI_VISI_DEF int RsTypicalRegisterMrV1(unsigned int phyId, unsigned int rdevIndex,
    struct rdma_mr_reg_info *mrRegInfo, void **mrHandle)
{
    RS_CHECK_POINTER_NULL_RETURN_INT(mrHandle);

    struct rs_mr_cb *typicalMrCb = NULL;
    struct rs_rdev_cb *rdevCb = NULL;
    unsigned int chipId;
    int ret;

    CHK_PRT_RETURN(mrRegInfo == NULL || mrRegInfo->addr == NULL || mrRegInfo->len == 0 ||
        phyId >= RS_MAX_DEV_NUM, hccp_err("param err, NULL pointer or phyId:%u >= [%d]", phyId,
        RS_MAX_DEV_NUM), -EINVAL);

    hccp_info("[rs_typical_register_mr] len[0x%llx], access[%d]",
        mrRegInfo->len, mrRegInfo->access);

    ret = rsGetLocalDevIDByHostDevID(phyId, &chipId);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_typical_register_mr rsGetLocalDevIDByHostDevID phy_id[%u] invalid, ret %d",
        phyId, ret), ret);

    ret = RsRdev2rdevCb(chipId, rdevIndex, &rdevCb);
    CHK_PRT_RETURN(ret != 0 || rdevCb == NULL, hccp_err("rs_rdev2rdev_cb for chip_id[%u] failed, ret %d",
        chipId, ret), ret);

    ret = RsQueryMrCb(rdevCb, (uint64_t)(uintptr_t)mrRegInfo->addr, &typicalMrCb, &rdevCb->typical_mr_list);
    if (ret == 0) {
        hccp_warn("typical mr already registered");
        goto found;
    }

    typicalMrCb = calloc(1, sizeof(struct rs_mr_cb));
    CHK_PRT_RETURN(typicalMrCb == NULL, hccp_err("calloc typical_mr_cb failed"), -ENOMEM);
    typicalMrCb->dev_cb = rdevCb;

    ret = RsInitTypicalMrCb(phyId, mrRegInfo, rdevCb, typicalMrCb);
    if (ret != 0) {
        hccp_err("rs_init_typical_mr_cb failed, devIndex[%u], ret[%d]", rdevIndex, ret);
        goto reg_err;
    }

found:
    *mrHandle = typicalMrCb->ib_mr;
    mrRegInfo->lkey = typicalMrCb->ib_mr->lkey;
    mrRegInfo->rkey = typicalMrCb->ib_mr->rkey;
    hccp_info("rs_typical_register_mr succ, state:%d", typicalMrCb->state);
    return 0;

reg_err:
    free(typicalMrCb);
    typicalMrCb = NULL;
    return ret;
}

RS_ATTRI_VISI_DEF int RsTypicalRegisterMr(unsigned int phyId, unsigned int rdevIndex,
    struct rdma_mr_reg_info *mrRegInfo, void **mrHandle)
{
    RS_CHECK_POINTER_NULL_RETURN_INT(mrHandle);

    struct rs_mr_cb *typicalMrCb = NULL;
    struct rs_rdev_cb *rdevCb = NULL;
    unsigned int chipId;
    int ret;

    CHK_PRT_RETURN(mrRegInfo == NULL || mrRegInfo->addr == NULL || mrRegInfo->len == 0 ||
        phyId >= RS_MAX_DEV_NUM, hccp_err("param err, NULL pointer or phyId:%u >= [%d]", phyId,
        RS_MAX_DEV_NUM), -EINVAL);

    hccp_info("start register len[0x%llx], access[%d]", mrRegInfo->len, mrRegInfo->access);

    ret = rsGetLocalDevIDByHostDevID(phyId, &chipId);
    CHK_PRT_RETURN(ret != 0, hccp_err("rsGetLocalDevIDByHostDevID phy_id[%u] invalid, ret %d", phyId, ret), ret);

    ret = RsRdev2rdevCb(chipId, rdevIndex, &rdevCb);
    CHK_PRT_RETURN(ret != 0 || rdevCb == NULL, hccp_err("rs_rdev2rdev_cb for chip_id[%u] failed, ret %d",
        chipId, ret), ret);

    typicalMrCb = calloc(1, sizeof(struct rs_mr_cb));
    CHK_PRT_RETURN(typicalMrCb == NULL, hccp_err("calloc typical_mr_cb failed"), -ENOMEM);
    typicalMrCb->dev_cb = rdevCb;

    ret = RsInitTypicalMrCb(phyId, mrRegInfo, rdevCb, typicalMrCb);
    if (ret != 0) {
        hccp_err("rs_init_typical_mr_cb failed, devIndex[%u], ret[%d]", rdevIndex, ret);
        goto reg_err;
    }

    // resv len as 1 to save addr for later unreg to query
    typicalMrCb->mr_info.addr = (uint64_t)(uintptr_t)typicalMrCb->ib_mr;
    typicalMrCb->mr_info.len = 1U;
    *mrHandle = typicalMrCb->ib_mr;
    mrRegInfo->lkey = typicalMrCb->ib_mr->lkey;
    mrRegInfo->rkey = typicalMrCb->ib_mr->rkey;
    hccp_info("register succ, state:%d", typicalMrCb->state);
    return 0;

reg_err:
    free(typicalMrCb);
    typicalMrCb = NULL;
    return ret;
}

RS_ATTRI_VISI_DEF int RsRemapMr(unsigned int phyId, unsigned int rdevIndex, struct mem_remap_info memList[],
    unsigned int memNum)
{
    struct rs_rdev_cb *devCb = NULL;
    struct rs_mr_cb *mrCurr = NULL;
    struct rs_mr_cb *mrNext = NULL;
    unsigned long long addr = 0;
    bool isMemMatched = false;
    unsigned int chipId;
    unsigned int i;
    int ret;

    CHK_PRT_RETURN(phyId >= RS_MAX_DEV_NUM, hccp_err("phy_id:%u >= %d, is invalid", phyId, RS_MAX_DEV_NUM), -EINVAL);

    ret = rsGetLocalDevIDByHostDevID(phyId, &chipId);
    CHK_PRT_RETURN(ret, hccp_err("rsGetLocalDevIDByHostDevID failed, phyId:%u invalid, ret:%d", phyId, ret), ret);

    ret = RsRdev2rdevCb(chipId, rdevIndex, &devCb);
    CHK_PRT_RETURN(devCb == NULL, hccp_err("rs_rdev2rdev_cb failed, chipId:%u, ret:%d", chipId, ret), -ENODEV);

    for (i = 0; i < memNum; i++) {
        isMemMatched = false;
        addr = (uint64_t)(uintptr_t)memList[i].addr;
        RS_PTHREAD_MUTEX_LOCK(&devCb->rdev_mutex);
        RS_LIST_GET_HEAD_ENTRY(mrCurr, mrNext, &devCb->typical_mr_list, list, struct rs_mr_cb);
        for (; (&mrCurr->list) != &devCb->typical_mr_list;
            mrCurr = mrNext, mrNext = list_entry(mrNext->list.next, struct rs_mr_cb, list)) {
            // mem is out range of mr, continue to find next matching mr
            if ((addr < (uint64_t)(uintptr_t)mrCurr->ib_mr->addr) ||
                (memList[i].size > mrCurr->ib_mr->length) ||
                (addr + memList[i].size < addr) ||
                (addr + memList[i].size > (uint64_t)(uintptr_t)mrCurr->ib_mr->addr + mrCurr->ib_mr->length)) {
                continue;
            }

            // each mr remap each corresponding mem
            ret = RsRoceRemapMr(mrCurr->ib_mr, (struct hns_roce_mr_remap_info *)&memList[i], 1);
            if (ret != 0) {
                hccp_err("remap %u-th mem failed, ret:%d addr:0x%llx size:0x%llx", i, ret, addr, memList[i].size);
                RS_PTHREAD_MUTEX_ULOCK(&devCb->rdev_mutex);
                return ret;
            }
            isMemMatched = true;
        }
        RS_PTHREAD_MUTEX_ULOCK(&devCb->rdev_mutex);

        if (!isMemMatched) {
            hccp_err("find %u-th mem failed, addr:0x%llx size:0x%llx", i, addr, memList[i].size);
            return -ENODEV;
        }
        hccp_dbg("remap %u-th mem success, addr:0x%llx size:0x%llx", i, addr, memList[i].size);
    }

    return 0;
}

RS_ATTRI_VISI_DEF int RsTypicalDeregisterMr(unsigned int phyId, unsigned int devIndex, unsigned long long addr)
{
    struct rs_mr_cb *typicalMrCb = NULL;
    struct rs_rdev_cb *devCb = NULL;
    unsigned int chipId;
    int ret;

    hccp_info("typical mr unreg start, addr[%llu]", addr);
    CHK_PRT_RETURN(phyId >= RS_MAX_DEV_NUM, hccp_err("phy_id:%u >= %d, is invalid", phyId, RS_MAX_DEV_NUM),
        -EINVAL);

    ret = rsGetLocalDevIDByHostDevID(phyId, &chipId);
    CHK_PRT_RETURN(ret, hccp_err("phy_id[%u] invalid, ret %d", phyId, ret), ret);

    ret = RsRdev2rdevCb(chipId, devIndex, &devCb);
    CHK_PRT_RETURN(ret != 0 || devCb == NULL, hccp_err("rs_rdev2rdev_cb get dev_cb failed for chip_id[%u], ret[%d]",
        chipId, ret), -ENODEV);

    ret = RsQueryMrCb(devCb, addr, &typicalMrCb, &devCb->typical_mr_list);
    CHK_PRT_RETURN(ret, hccp_err("rs_query_mr_cb failed ret[%d]", ret), ret);

    ret = RsDrvMrDereg(typicalMrCb->ib_mr);
    CHK_PRT_RETURN(ret, hccp_err("rs_drv_mr_dereg failed ret[%d]", ret), -EACCES);

    RS_PTHREAD_MUTEX_LOCK(&devCb->rdev_mutex);
    RsListDel(&typicalMrCb->list);
    free(typicalMrCb);
    typicalMrCb = NULL;
    RS_PTHREAD_MUTEX_ULOCK(&devCb->rdev_mutex);

    hccp_info("dev_index[%u] succ", devIndex);

    return 0;
}

RS_ATTRI_VISI_DEF int RsDeregisterMr(void *mrHandle)
{
    RS_CHECK_POINTER_NULL_RETURN_INT(mrHandle);

    int ret;
    struct ibv_mr *rsMrHandle = (struct ibv_mr *)mrHandle;

    ret = RsDrvMrDereg(rsMrHandle);
    CHK_PRT_RETURN(ret, hccp_err("rs_drv_mr_dereg failed ret[%d] ", ret), -EACCES);

    hccp_info("rs_deregister_mr succ");
    return 0;
}

RS_ATTRI_VISI_DEF int RsSendWr(unsigned int phyId, unsigned int rdevIndex, uint32_t qpn, struct send_wr *wr,
    struct send_wr_rsp *wrRsp)
{
    int ret;
    struct rs_qp_cb *qpCb = NULL;
    struct rs_mr_cb *mrCb = NULL;
    struct rs_mr_cb *remMrCb = NULL;

    RS_CHECK_POINTER_NULL_RETURN_INT(wr);
    RS_CHECK_POINTER_NULL_RETURN_INT(wr->buf_list);
    RS_CHECK_POINTER_NULL_RETURN_INT(wrRsp);

    CHK_PRT_RETURN(phyId >= RS_MAX_DEV_NUM, hccp_err("phy_id:%u >= [%d], is invalid",
        phyId, RS_MAX_DEV_NUM), -EINVAL);

    CHK_PRT_RETURN(wr->buf_num > MAX_SGE_NUM || wr->buf_num == 0, hccp_err("invalid buf_num[%u]!",
        wr->buf_num), -EINVAL);

    CHK_PRT_RETURN(wr->buf_list->len > RS_SGLIST_LEN_MAX || wr->buf_list->len == 0, hccp_err("sg list"
        "len is more than 2G, len[%u]", wr->buf_list->len), -EINVAL);

    if (RsQpn2qpcb(phyId, rdevIndex, qpn, &qpCb)) {
        return -EACCES;
    }

    qpCb->send_wr_num++;

    hccp_info("qpn %d, buf_list[0].addr is 0x%llx", qpn, wr->buf_list[0].addr);
    if (RsGetMrcb(qpCb, wr->buf_list[0].addr, &mrCb, &qpCb->mr_list)) {
        hccp_err("qpn %d, buf_list[0].addr[0x%llx] len[0x%x] is invalid.", qpn, wr->buf_list[0].addr,
            wr->buf_list[0].len);
        return -EFAULT;
    }

    // send op no need to check & get remote mr
    if (wr->op != RA_WR_SEND && wr->op != RA_WR_SEND_WITH_IMM) {
        hccp_info("remote wr dst addr is 0x%llx", wr->dst_addr);
        if (RsGetMrcb(qpCb, wr->dst_addr, &remMrCb, &qpCb->rem_mr_list)) {
            hccp_err("qpn %d, remote wr dst addr[0x%llx] len[0x%x] is invalid.", qpn, wr->dst_addr,
                wr->buf_list[0].len);
            return -ENOENT;
        }
    }

    ret = RsDrvSendExp(qpCb, mrCb, remMrCb, wr, wrRsp);
    if (ret) {
        hccp_err("send exp failed qpn %u, ret %d", qpn, ret);
    }
    gRsSendWrNum++;
    return ret;
}

STATIC void BuildUpWrWithKey(struct wr_info *wr, struct ibv_sge *list, struct ibv_send_wr *ibWr)
{
    list->addr = (uintptr_t)wr->mem_list.addr;
    list->length = wr->mem_list.len;
    list->lkey = wr->mem_list.lkey;

    ibWr->sg_list = list;
    ibWr->opcode = wr->op;
    ibWr->send_flags = (unsigned int)wr->send_flags;
    ibWr->imm_data = htobe32(wr->imm_data);

    ibWr->num_sge = 1; /* only support one sge */
    ibWr->wr_id = wr->wr_id;
    if (wr->op != IBV_WR_SEND && wr->op != IBV_WR_SEND_WITH_IMM) {
        ibWr->wr.rdma.rkey = wr->rkey;
        ibWr->wr.rdma.remote_addr = wr->dst_addr;
    }
}

STATIC void RsSendBuildUpWr(struct rs_mr_cb *mrCb, struct wr_info *wr, struct ibv_sge *list,
    struct ibv_send_wr *ibWr)
{
    list->addr = (uintptr_t)wr->mem_list.addr;
    list->lkey =  mrCb->ib_mr->lkey;
    list->length = wr->mem_list.len;

    ibWr->sg_list = list;
    ibWr->opcode = wr->op;
    ibWr->imm_data = htobe32(wr->imm_data);
    ibWr->send_flags = (unsigned int)wr->send_flags;

    ibWr->num_sge = 1; /* only support one sge */
    ibWr->wr_id = wr->wr_id;
}

STATIC void RsWirteAndReadBuildUpWr(struct rs_mr_cb *mrCb, struct rs_mr_cb *remMrCb,
    struct wr_info *wr, struct ibv_sge *list, struct ibv_send_wr *ibWr)
{
    list->addr = (uintptr_t)wr->mem_list.addr;
    list->length = wr->mem_list.len;
    list->lkey =  mrCb->ib_mr->lkey;

    ibWr->sg_list = list;
    ibWr->opcode = wr->op;
    ibWr->send_flags = (unsigned int)wr->send_flags;
    ibWr->imm_data = htobe32(wr->imm_data);

    ibWr->num_sge = 1; /* only support one sge */
    ibWr->wr_id = wr->wr_id;
    ibWr->wr.rdma.rkey = remMrCb->mr_info.rkey;
    ibWr->wr.rdma.remote_addr = wr->dst_addr;
}

STATIC int RsBuildUpWrList(struct wr_info *wrList, struct rs_qp_cb *qpCb, struct ibv_sge *list,
    struct ibv_send_wr *ibWr, unsigned int i)
{
    struct rs_mr_cb *mrCb = NULL;
    struct rs_mr_cb *remMrCb = NULL;
    CHK_PRT_RETURN(wrList[i].mem_list.len > RS_SGLIST_LEN_MAX, hccp_err("sg list len is more than 2G, len[%u]",
        wrList[i].mem_list.len), -EINVAL);

    hccp_dbg("qpn %d, buf_list[0].addr is 0x%llx", qpCb->ib_qp->qp_num, wrList[i].mem_list.addr);
    if (RsGetMrcb(qpCb, wrList[i].mem_list.addr, &mrCb, &qpCb->mr_list)) {
        hccp_err("qpn %d, buf_list[0].addr[0x%llx] len[0x%x] is invalid.", qpCb->ib_qp->qp_num,
            wrList[i].mem_list.addr, wrList[i].mem_list.len);
        return -EFAULT;
    }

    // send op no need to check & get remote mr
    if (wrList[i].op != IBV_WR_SEND && wrList[i].op != IBV_WR_SEND_WITH_IMM) {
        hccp_dbg("remote wr dst addr is 0x%llx", wrList[i].dst_addr);
        if (RsGetMrcb(qpCb, wrList[i].dst_addr, &remMrCb, &qpCb->rem_mr_list)) {
            hccp_err("qpn %d, remote wr dst addr[0x%llx] len[0x%x] is invalid.", qpCb->ib_qp->qp_num,
                wrList[i].dst_addr, wrList[i].mem_list.len);
            return -ENOENT;
        }
        RsWirteAndReadBuildUpWr(mrCb, remMrCb, &wrList[i], &list[i], &ibWr[i]);
    } else {
        RsSendBuildUpWr(mrCb, &wrList[i], &list[i], &ibWr[i]);
    }

    return 0;
}

STATIC int RsBuildUpWrListWithKey(struct wr_info *wrList, struct ibv_sge *list,
    struct ibv_send_wr *ibWr, unsigned int i)
{
    CHK_PRT_RETURN(wrList[i].mem_list.len > RS_SGLIST_LEN_MAX, hccp_err("sg list len is more than 2G, len[%u]",
        wrList[i].mem_list.len), -EINVAL);

    BuildUpWrWithKey(&wrList[i], &list[i], &ibWr[i]);
    return 0;
}

STATIC int RsSendNormalWrlist(struct rs_qp_cb *qpCb, struct wr_info *wrList,
    unsigned int sendNum, unsigned int *completeNum, unsigned int keyFlag)
{
    int ret;
    unsigned int i, j;

    struct ibv_send_wr *badWr = NULL;
    CHK_PRT_RETURN(sendNum > MAX_WR_NUM || sendNum == 0, hccp_err("send num[%u] is invalid!", sendNum), -EINVAL);
    struct ibv_send_wr *ibWr = (struct ibv_send_wr *)calloc(sendNum, sizeof(struct ibv_send_wr));
    CHK_PRT_RETURN(ibWr == NULL, hccp_err("calloc ib_wr failed!"), -ENOSPC);

    struct ibv_sge *list = (struct ibv_sge *)calloc(sendNum, sizeof(struct ibv_sge));
    if (list == NULL) {
        hccp_err("calloc list failed!");
        ret = -ENOSPC;
        goto alloc_fail;
    }

    for (i = 0; i < sendNum; i++) {
        ret = (keyFlag == 0) ? RsBuildUpWrList(wrList, qpCb, list, ibWr, i) :
            RsBuildUpWrListWithKey(wrList, list, ibWr, i);
        if (ret) {
            goto input_err;
        }
        j = i + 1;
        ibWr[i].next = (i < sendNum - 1) ? &ibWr[j] : NULL;
    }

    ret = RsIbvPostSend(qpCb->ib_qp, &ibWr[0], &badWr);
    if (ret == 0) {
        *completeNum = sendNum;
    } else if (ret == -ENOMEM) {
        *completeNum = (unsigned int)((void *)badWr - (void *)ibWr) / sizeof(struct ibv_send_wr);
        hccp_dbg("post send wqe overflow, completeNum[%d]", *completeNum);
    } else {
        hccp_err("ibv_post_send failed, ret[%d]", ret);
        *completeNum = 0;
    }
    qpCb->send_wr_num = qpCb->send_wr_num + (*completeNum);

input_err:
    free(list);
    list = NULL;
alloc_fail:
    free(ibWr);
    ibWr = NULL;
    return (ret == -ENOMEM) ? 0 : ret;
}

STATIC int RsSendExpWrlist(struct rs_qp_cb *qpCb, struct wr_info *wrList, unsigned int sendNum,
    struct send_wr_rsp *wrRsp, unsigned int *completeNum, unsigned int keyFlag)
{
    struct ibv_post_send_ext_attr extAttr = {0};
    struct ibv_post_send_ext_resp extRsp = {0};
    struct ibv_send_wr *badWr = NULL;
    struct wr_exp_rsp expRsp = {0};
    struct ibv_send_wr ibWr = {0};
    struct ibv_sge list = {0};
    unsigned int i;
    int ret = 0;

    for (i = 0; i < sendNum; i++) {
        // reuse code: only need to build up one wr once a time
        ret = (keyFlag == 0) ? RsBuildUpWrList(&wrList[i], qpCb, &list, &ibWr, 0) :
            RsBuildUpWrListWithKey(&wrList[i], &list, &ibWr, 0);
        if (ret != 0) {
            hccp_err("qpn:%u key_flag:%u build_up_wr i:%u failed, ret:%d", qpCb->ib_qp->qp_num, keyFlag, i, ret);
            break;
        }

        if (wrList[i].op == RA_WR_RDMA_WRITE_WITH_NOTIFY ||
            wrList[i].op == RA_WR_RDMA_REDUCE_WRITE ||
            wrList[i].op == RA_WR_RDMA_REDUCE_WRITE_WITH_NOTIFY) {
            ibWr.imm_data = htobe32((wrList[i].aux.notify_offset & WRITE_NOTIFY_OFFSET_MASK) |
                WRITE_NOTIFY_VALUE_RECORD);
            extAttr.reduce_op = wrList[i].aux.reduce_type;
            extAttr.reduce_type = wrList[i].aux.data_type;
            ret = RsIbvExtPostSend(qpCb->ib_qp, &ibWr, &badWr, &extAttr, &extRsp);
            expRsp.wqe_index = extRsp.wqe_index;
            expRsp.db_info = extRsp.db_info;
            hccp_dbg("rs_ibv_ext_post_send, op = [%x], imm_data = [0x%lx], reduce_op = [%d],reduce_type = [%d]",
                     ibWr.opcode, ibWr.imm_data, extAttr.reduce_op, extAttr.reduce_type);
        } else {
            ret = RsIbvExpPostSend(qpCb->ib_qp, &ibWr, &badWr, &expRsp);
            hccp_dbg("rs_ibv_exp_post_send, op = [%x], remote_addr = [0x%llx], size = [%d]",
                     ibWr.opcode, ibWr.wr.rdma.remote_addr, ibWr.sg_list->length);
        }

        if (ret != 0) {
            if (ret == -ENOMEM) {
                hccp_warn("qpn:%u rs_ibv_exp_post_send i:%u unsuccessful, ret %d", qpCb->ib_qp->qp_num, i, ret);
            } else {
                hccp_err("qpn:%u rs_ibv_exp_post_send i:%u failed, ret %d", qpCb->ib_qp->qp_num, i, ret);
            }
            break;
        }

        qpCb->send_wr_num++;

        if (qpCb->qp_mode == RA_RS_GDR_TMPL_QP_MODE) {
            wrRsp[i].wqe_tmp.sq_index = (unsigned int)qpCb->sq_index;
            wrRsp[i].wqe_tmp.wqe_index = expRsp.wqe_index;
        } else if (qpCb->qp_mode == RA_RS_OP_QP_MODE ||
                   qpCb->qp_mode == RA_RS_GDR_ASYN_QP_MODE) {
            wrRsp[i].db.db_index = (unsigned int)qpCb->db_index;
            wrRsp[i].db.db_info = expRsp.db_info;
        }
    }

    hccp_dbg("complete_num[%d], ret[%d]", i, ret);
    *completeNum = i;
    return (ret == -ENOMEM) ? 0 : ret;
}

RS_ATTRI_VISI_DEF int RsSendWrlist(struct rs_wrlist_base_info baseInfo, struct wr_info *wrList,
    unsigned int sendNum, struct send_wr_rsp *wrRsp, unsigned int *completeNum)
{
    int ret;
    unsigned int phyId, rdevIndex, qpn;
    struct rs_qp_cb *qpCb = NULL;

    RS_CHECK_POINTER_NULL_RETURN_INT(wrList);
    RS_CHECK_POINTER_NULL_RETURN_INT(wrRsp);
    CHK_PRT_RETURN(sendNum > MAX_WR_NUM || sendNum == 0 || baseInfo.phy_id >= RS_MAX_DEV_NUM,
        hccp_err("send_num[%u] or phy_id:%u >= [%d], is invalid", sendNum, baseInfo.phy_id, RS_MAX_DEV_NUM),
        -EINVAL);

    phyId = baseInfo.phy_id;
    rdevIndex = baseInfo.rdev_index;
    qpn = baseInfo.qpn;

    CHK_PRT_RETURN(RsQpn2qpcb(phyId, rdevIndex, qpn, &qpCb), hccp_err("rs_qpn2qpcb failed, physical id[%u]",
        phyId), -EACCES);

    // only allow normal qp to call this func when ai_op_support not set
    if (qpCb->qp_mode == RA_RS_NOR_QP_MODE && qpCb->ai_op_support == 0) {
        ret = RsSendNormalWrlist(qpCb, wrList, sendNum, completeNum, baseInfo.key_flag);
    } else {
        ret = RsSendExpWrlist(qpCb, wrList, sendNum, wrRsp, completeNum, baseInfo.key_flag);
    }
    return ret;
}

RS_ATTRI_VISI_DEF int RsRecvWrlist(struct rs_wrlist_base_info baseInfo, struct recv_wrlist_data *wr,
    unsigned int recvNum, unsigned int *completeNum)
{
    struct rs_qp_cb *qpCb = NULL;

    RS_CHECK_POINTER_NULL_RETURN_INT(wr);
    CHK_PRT_RETURN(recvNum > MAX_WR_NUM || recvNum == 0 || baseInfo.phy_id >= RS_MAX_DEV_NUM,
        hccp_err("recv_num[%u] or phy_id:%u >= [%d], is invalid", recvNum, baseInfo.phy_id, RS_MAX_DEV_NUM),
        -EINVAL);

    CHK_PRT_RETURN(RsQpn2qpcb(baseInfo.phy_id, baseInfo.rdev_index, baseInfo.qpn, &qpCb),
        hccp_err("rs_qpn2qpcb failed, physical id[%u]",  baseInfo.phy_id), -EACCES);

    return RsDrvPostRecv(qpCb, wr, recvNum, completeNum);
}

RS_ATTRI_VISI_DEF int RsSetHostPid(uint32_t phyId, pid_t hostPid, const char *pidSign)
{
    int ret;
    unsigned int chipId;
    struct rs_cb *rsCb = NULL;

    RS_CHECK_POINTER_NULL_RETURN_INT(pidSign);
    CHK_PRT_RETURN(phyId >= RS_MAX_DEV_NUM, hccp_err("rs_set_host_pid rs set param error ! phy_id:%u",
        phyId), -EINVAL);

    ret = rsGetLocalDevIDByHostDevID(phyId, &chipId);
    CHK_PRT_RETURN(ret, hccp_err("rs_set_host_pid rsGetLocalDevIDByHostDevID phy_id invalid, ret %d", ret), ret);

    hccp_info("phy_id[%u] host_pid[%d]", chipId, hostPid);

    ret = RsDev2rscb(chipId, &rsCb, false);
    CHK_PRT_RETURN(ret, hccp_err("get rs cb failed, chipId:%u", chipId), ret);

    rsCb->p_rs_sign.tgid = hostPid;
    ret = strcpy_s(rsCb->p_rs_sign.sign, PROCESS_RS_SIGN_LENGTH, pidSign);
    CHK_PRT_RETURN(ret, hccp_err("copy sign failed, ret %d", ret), -ESAFEFUNC);

    return 0;
}

RS_ATTRI_VISI_DEF int RsRdevGetPortStatus(unsigned int phyId, unsigned int rdevIndex, enum port_status *status)
{
    struct ibv_port_attr portAttr = { 0 };
    struct rs_rdev_cb *rdevCb = NULL;
    unsigned int chipId;
    int ret;

    CHK_PRT_RETURN(phyId >= RS_MAX_DEV_NUM, hccp_err("phy_id:%u >= [%d], is invalid",
        phyId, RS_MAX_DEV_NUM), -EINVAL);
    CHK_PRT_RETURN(status == NULL, hccp_err("param err! status is NULL"), -EINVAL);

    ret = rsGetLocalDevIDByHostDevID(phyId, &chipId);
    CHK_PRT_RETURN(ret, hccp_err("rsGetLocalDevIDByHostDevID failed, phyId[%u] invalid, ret %d", phyId, ret), ret);

    ret = RsRdev2rdevCb(chipId, rdevIndex, &rdevCb);
    CHK_PRT_RETURN(ret != 0 || rdevCb == NULL, hccp_err("rs_rdev2rdev_cb for chip_id[%u] failed, ret %d",
        chipId, ret), ret);

    ret = RsIbvQueryPort(rdevCb->ib_ctx, rdevCb->ib_port, &portAttr);
    CHK_PRT_RETURN(ret, hccp_err("ibv_query_port failed ret[%d]", ret), -EOPENSRC);

    *status = portAttr.state == IBV_PORT_ACTIVE ? PORT_STATUS_ACTIVE : PORT_STATUS_DOWN;

    hccp_dbg("phy_id:%u port_attr.state:%u status:%u", phyId, portAttr.state, *status);
    return 0;
}

RS_ATTRI_VISI_DEF int RsGetNotifyMrInfo(unsigned int phyId, unsigned int rdevIndex, struct mr_info *info)
{
    struct rs_rdev_cb *rdevCb = NULL;
    struct rs_cb *rsCb = NULL;
    unsigned int chipId;
    int ret;

    CHK_PRT_RETURN(phyId >= RS_MAX_DEV_NUM, hccp_err("phy_id:%u >= [%d], is invalid",
        phyId, RS_MAX_DEV_NUM), -EINVAL);

    CHK_PRT_RETURN(info == NULL, hccp_err("param err! info is NULL"), -EINVAL);

    ret = rsGetLocalDevIDByHostDevID(phyId, &chipId);
    CHK_PRT_RETURN(ret, hccp_err("phy_id[%u] invalid, ret:%d", phyId, ret), ret);

    ret = RsDev2rscb(chipId, &rsCb, false);
    CHK_PRT_RETURN(ret, hccp_err("get rs_cb failed, ret:%d", ret), -ENODEV);

    ret = RsGetRdevCb(rsCb, rdevIndex, &rdevCb);
    CHK_PRT_RETURN(ret, hccp_err("rs_get_rdev_cb failed!, ret:%d, rdevIndex:%u", ret, rdevIndex), ret);

    info->addr = (void *)(uintptr_t)rdevCb->notify_va_base;
    info->size = rdevCb->notify_size;
    info->access = rdevCb->notify_access;
    info->lkey = rdevCb->notify_mr->lkey;

    return 0;
}

RS_ATTRI_VISI_DEF int RsNotifyCfgSet(unsigned int phyId, unsigned long long va, unsigned long long size)
{
    int ret;
    unsigned int chipId;
    struct rs_cb *rsCb = NULL;

    RS_CHECK_POINTER_NULL_RETURN_INT((void *)(uintptr_t)va);

    CHK_PRT_RETURN(phyId >= RS_MAX_DEV_NUM ||
        (size != MAX_NOTIFY_SIZE_CLOUD && size != NOTIFY_NUM_MAX_V2 && size != NOTIFY_NUM_MAX_V3),
        hccp_err("rs_notify_cfg_set rs set param error ! phy_id[%u] size[%llu]", phyId, size), -EINVAL);

    ret = rsGetLocalDevIDByHostDevID(phyId, &chipId);
    CHK_PRT_RETURN(ret, hccp_err("rs_notify_cfg_set phy_id invalid, ret %d, phyId:%u", ret, phyId), ret);

    ret = RsDev2rscb(chipId, &rsCb, false);
    CHK_PRT_RETURN(ret, hccp_err("get rs cb failed, chipId:%u", chipId), ret);

    rsCb->notify_va_base = va;
    rsCb->notify_size = size;

    return 0;
}

RS_ATTRI_VISI_DEF int RsNotifyCfgGet(unsigned int phyId, unsigned long long *va, unsigned long long *size)
{
    int ret;
    unsigned int chipId;
    struct rs_cb *rsCb = NULL;

    RS_CHECK_POINTER_NULL_RETURN_INT(va);
    RS_CHECK_POINTER_NULL_RETURN_INT(size);

    CHK_PRT_RETURN(phyId >= RS_MAX_DEV_NUM, hccp_err("rs_notify_cfg_get rs set param error ! phy_id:%u",
        phyId), -EINVAL);

    ret = rsGetLocalDevIDByHostDevID(phyId, &chipId);
    CHK_PRT_RETURN(ret, hccp_err("rs_notify_cfg_get phy_id invalid, ret %d, phyId:%u", ret, phyId), ret);

    ret = RsDev2rscb(chipId, &rsCb, false);
    CHK_PRT_RETURN(ret, hccp_err("get rs cb failed, chipId:%u", chipId), ret);

    *va = rsCb->notify_va_base;
    *size = rsCb->notify_size;

    return 0;
}

RS_ATTRI_VISI_DEF int RsSetTsqpDepth(unsigned int phyId, unsigned int rdevIndex, unsigned int tempDepth,
    unsigned int *qpNum)
{
#ifdef CUSTOM_INTERFACE
    int ret;
    unsigned int chipId = 0;
    unsigned int sqDepth = 0;
    struct rs_rdev_cb *rdevCb = NULL;

    CHK_PRT_RETURN(phyId >= RS_MAX_DEV_NUM, hccp_err("rs_set_tsqp_depth param error ! phy_id:%d", phyId), -EINVAL);

    CHK_PRT_RETURN(qpNum == NULL, hccp_err("rs_set_tsqp_depth qp_num is NULL, param error!"), -EINVAL);

    CHK_PRT_RETURN(tempDepth < RS_MIN_TEMPTH_DEPTH || tempDepth > RS_MAX_TEMPTH_DEPTH, hccp_err("param error!"
        "temp_depth[%u] can not smaller than [%d] or bigerr than [%d]", tempDepth, RS_MIN_TEMPTH_DEPTH,
        RS_MAX_TEMPTH_DEPTH), -EINVAL);

    ret = rsGetLocalDevIDByHostDevID(phyId, &chipId);
    CHK_PRT_RETURN(ret, hccp_err("phy_id[%u] invalid, ret %d", phyId, ret), ret);

    ret = RsRdev2rdevCb(chipId, rdevIndex, &rdevCb);
    CHK_PRT_RETURN(ret || rdevCb == NULL, hccp_err("rs_set_tsqp_depth rs_rdev2rdev_cb for chip_id[%u]"
        "failed, ret %d", chipId, ret), ret);

    ret = RsRoceSetTsqpDepth(rdevCb->dev_name, rdevIndex, tempDepth, qpNum, &sqDepth);
    CHK_PRT_RETURN(ret, hccp_err("rs_roce_set_tsqp_depth failed, ret %d, dev_name[%s]", ret, rdevCb->dev_name), ret);

    rdevCb->tx_depth = sqDepth;
    rdevCb->rx_depth = sqDepth;
    rdevCb->qp_max_num = *qpNum;
#endif
    return 0;
}

RS_ATTRI_VISI_DEF int RsGetTsqpDepth(unsigned int phyId, unsigned int rdevIndex, unsigned int *tempDepth,
    unsigned int *qpNum)
{
#ifdef CUSTOM_INTERFACE
    int ret;
    unsigned int chipId = 0;
    unsigned int sqDepth = 0;
    struct rs_rdev_cb *rdevCb = NULL;

    CHK_PRT_RETURN(phyId >= RS_MAX_DEV_NUM, hccp_err("param error ! phy_id:%d", phyId), -EINVAL);

    CHK_PRT_RETURN(tempDepth == NULL || qpNum == NULL, hccp_err("temp_depth or qp_num is NULL,"
        "param error!"), -EINVAL);

    ret = rsGetLocalDevIDByHostDevID(phyId, &chipId);
    CHK_PRT_RETURN(ret, hccp_err("phy_id[%u] invalid, ret %d", phyId, ret), ret);

    ret = RsRdev2rdevCb(chipId, rdevIndex, &rdevCb);
    CHK_PRT_RETURN(ret || rdevCb == NULL, hccp_err("rs_get_tsqp_depth rs_rdev2rdev_cb for chip_id[%u]"
        "failed, ret %d", chipId, ret), ret);

    ret = RsRoceGetTsqpDepth(rdevCb->dev_name, rdevIndex, tempDepth, qpNum, &sqDepth);
    CHK_PRT_RETURN(ret, hccp_err("rs_roce_get_tsqp_depth failed, ret %d, dev_name[%s]", ret, rdevCb->dev_name), ret);
#endif
    return 0;
}

STATIC void RsSetQpDepthAttr(struct rs_rdev_cb *rdevCb, struct rs_qp_cb *qpCb, struct rs_qp_norm *qpNorm)
{
    if (qpCb->qp_mode == RA_RS_GDR_TMPL_QP_MODE) {
        qpCb->tx_depth = rdevCb->tx_depth;
        qpCb->rx_depth = rdevCb->rx_depth;
    } else {
        if (rdevCb->rs_cb->hccp_mode == NETWORK_OFFLINE) {
            qpCb->tx_depth = RS_QP_TX_DEPTH_OFFLINE;
            qpCb->rx_depth = RS_QP_RX_DEPTH_OFFLINE;
        } else {
            qpCb->tx_depth = RS_QP_TX_DEPTH_ONLINE;
            qpCb->rx_depth = RS_QP_RX_DEPTH_ONLINE;
        }
    }

    if (qpNorm->is_exp != 0 && qpNorm->qp_mode != RA_RS_NOR_QP_MODE) {
        if (rdevCb->rs_cb->hccp_mode == NETWORK_PEER_ONLINE) {
            qpCb->tx_depth = (qpCb->qp_mode != RA_RS_GDR_TMPL_QP_MODE) ? RS_QP_TX_DEPTH_PEER_ONLINE : qpCb->tx_depth;
            qpCb->rx_depth = (qpCb->qp_mode != RA_RS_GDR_TMPL_QP_MODE) ? RS_QP_TX_DEPTH_PEER_ONLINE : qpCb->rx_depth;
        } else {
            qpCb->tx_depth = (qpCb->qp_mode != RA_RS_GDR_TMPL_QP_MODE && qpCb->qp_mode != RA_RS_GDR_ASYN_QP_MODE)
                                  ? RS_QP_32K_DEPTH
                                  : qpCb->tx_depth;
        }
        qpCb->send_sge_num = 1;
        qpCb->recv_sge_num = 1;
    } else {
        if (rdevCb->rs_cb->hccp_mode == NETWORK_PEER_ONLINE) {
            qpCb->tx_depth = (qpCb->qp_mode != RA_RS_GDR_TMPL_QP_MODE) ? RS_QP_TX_DEPTH_PEER_ONLINE : qpCb->tx_depth;
            qpCb->rx_depth = (qpCb->qp_mode != RA_RS_GDR_TMPL_QP_MODE) ? RS_QP_TX_DEPTH_PEER_ONLINE : qpCb->rx_depth;
        } else {
            qpCb->tx_depth = (qpCb->qp_mode != RA_RS_GDR_TMPL_QP_MODE) ? RS_QP_TX_DEPTH : qpCb->tx_depth;
            qpCb->rx_depth = (qpCb->qp_mode != RA_RS_GDR_TMPL_QP_MODE) ? RS_QP_TX_DEPTH : qpCb->rx_depth;
        }
        qpCb->send_sge_num = RS_QP_ATTR_MAX_SEND_SGE;
        qpCb->recv_sge_num = 1;
    }
}

STATIC int RsQpcbInit(struct rs_rdev_cb *rdevCb, struct rs_qp_cb *qpCb, struct rs_qp_norm *qpNorm)
{
#define RS_DRV_CQ_DEPTH         16384
#define RS_DRV_CQ_128_DEPTH     128
#define RS_DRV_CQ_8K_DEPTH      8192
#define RS_DRV_CQ_32K_DEPTH     32768
    int qpMode = qpNorm->qp_mode;
    int ret;

    qpCb->rdev_cb = rdevCb;
    RS_INIT_LIST_HEAD(&qpCb->mr_list);
    RS_INIT_LIST_HEAD(&qpCb->rem_mr_list);

    qpCb->qp_mode = qpMode;
    qpCb->eq_num = 0;
    qpCb->num_recv_cq_events = 0;
    qpCb->num_send_cq_events = 0;
    qpCb->state = RS_QP_STATUS_DISCONNECT;
    qpCb->ib_pd = rdevCb->ib_pd;

    // cq attr
    if (qpNorm->is_ext == 1) {
        // update TEMP & ASYN mode cq depth from 32K to 8K due to memory issue
        qpCb->send_cq_depth = (qpMode != RA_RS_GDR_TMPL_QP_MODE && qpMode != RA_RS_GDR_ASYN_QP_MODE)
            ? RS_DRV_CQ_32K_DEPTH : RS_DRV_CQ_8K_DEPTH;
        qpCb->recv_cq_depth = RS_DRV_CQ_128_DEPTH;
    } else {
        qpCb->send_cq_depth = RS_DRV_CQ_DEPTH;
        qpCb->recv_cq_depth = RS_DRV_CQ_DEPTH;
    }

    // qp attr
    RsSetQpDepthAttr(rdevCb, qpCb, qpNorm);

    qpCb->mem_align = qpNorm->mem_align;

    qpCb->channel = RsIbvCreateCompChannel(rdevCb->ib_ctx);
    CHK_PRT_RETURN(qpCb->channel == NULL, hccp_err("ibv_create_comp_channel failed! errno(%d)", errno), -EINVAL);
    qpCb->qos_attr.tc = (RS_ROCE_DSCP_33 & RS_DSCP_MASK) << RS_DSCP_OFF;
    qpCb->qos_attr.sl = RS_ROCE_4_SL;
    qpCb->timeout = RS_QP_ATTR_TIMEOUT;
    qpCb->retry_cnt = RS_QP_ATTR_RETRY_CNT;

    ret = RsEpollCtl(rdevCb->rs_cb->conn_cb.epollfd, EPOLL_CTL_ADD, qpCb->channel->fd, EPOLLIN | EPOLLRDHUP);
#ifndef CA_CONFIG_LLT
    if (ret) {
        RsIbvDestroyCompChannel(qpCb->channel);
        hccp_err("add channel fd failed ret %d", ret);
        return ret;
    }
#endif
    return 0;
}

STATIC int RsQpcbDeinit(struct rs_rdev_cb *rdevCb, struct rs_qp_cb *qpCb)
{
    int ret;

    if (qpCb == NULL || qpCb->channel == NULL) {
        hccp_err("qp_cb or qp_cb->channel is NULL!");
        return -EINVAL;
    }

    ret = RsEpollCtl(rdevCb->rs_cb->conn_cb.epollfd, EPOLL_CTL_DEL, qpCb->channel->fd, EPOLLIN | EPOLLRDHUP);
#ifndef CA_CONFIG_LLT
    if (ret) {
        hccp_err("del channel fd failed ret %d", ret);
    }
#endif

    if (qpCb->channel != NULL) {
        RsIbvDestroyCompChannel(qpCb->channel);
        qpCb->channel = NULL;
    }
#ifndef CA_CONFIG_LLT
    return ret;
#else
    return 0;
#endif
}

STATIC int RsQpNotifyMr(struct rs_rdev_cb *rdevCb, struct rs_qp_cb *qpCb, uint32_t *qpn)
{
    int ret;
    struct rs_mr_cb *notifyMrNode = NULL;

    ret = RsCallocMr(1, &notifyMrNode);
    CHK_PRT_RETURN(ret, hccp_err("notify_mr_cb malloc failed"), ret);

    RS_PTHREAD_MUTEX_LOCK(&rdevCb->rdev_mutex);
    RsListAddTail(&qpCb->list, &rdevCb->qp_list);
    RS_PTHREAD_MUTEX_ULOCK(&rdevCb->rdev_mutex);

    if (rdevCb->notify_type != NO_USE) {
        notifyMrNode->qp_cb = qpCb;
        notifyMrNode->ib_mr = rdevCb->notify_mr;
        notifyMrNode->mr_info.addr = rdevCb->notify_va_base;
        notifyMrNode->mr_info.len = rdevCb->notify_size;
        notifyMrNode->mr_info.rkey = notifyMrNode->ib_mr->rkey;
    } else {
        notifyMrNode->qp_cb = qpCb;
        notifyMrNode->ib_mr = NULL;
    }

    RS_PTHREAD_MUTEX_LOCK(&qpCb->qp_mutex);
    RsListAddTail(&notifyMrNode->list, &qpCb->mr_list);
    RS_PTHREAD_MUTEX_ULOCK(&qpCb->qp_mutex);
    rdevCb->qp_cnt++;
    *qpn = qpCb->ib_qp->qp_num;

    hccp_info("rs qp %d create OK!", *qpn);

    return 0;
}

STATIC int RsQpQueryInfo(unsigned int phyId, unsigned int rdevIndex, struct rs_rdev_cb **rdevCb, int qpMode)
{
    int ret;
    unsigned int chipId;
    struct rs_cb *rsCb = NULL;

    CHK_PRT_RETURN(phyId >= RS_MAX_DEV_NUM, hccp_err("rs_qp_query_info rs set param error! phy_id:%u",
        phyId), -EINVAL);

    ret = rsGetLocalDevIDByHostDevID(phyId, &chipId);
    CHK_PRT_RETURN(ret, hccp_err("rs_qp_query_info phy_id[%u] invalid, ret:%d", phyId, ret), ret);

    ret = RsDev2rscb(chipId, &rsCb, false);
    CHK_PRT_RETURN(ret, hccp_err("rs_qp_query_info get rs_cb failed, ret:%d", ret), -ENODEV);

    ret = RsGetRdevCb(rsCb, rdevIndex, rdevCb);
    CHK_PRT_RETURN(ret, hccp_err("rs_get_rdev_cb failed! ret:%d, rdevIndex:%u", ret, rdevIndex), ret);

    if (qpMode == RA_RS_GDR_TMPL_QP_MODE) {
        CHK_PRT_RETURN((*rdevCb)->qp_cnt >= (*rdevCb)->qp_max_num, hccp_err("Exceeded the maximum QP limit(%u)",
            (*rdevCb)->qp_max_num), -EINVAL);
    } else {
        CHK_PRT_RETURN((*rdevCb)->qp_cnt >= RS_QP_NUM_MAX, hccp_err("Exceeded the maximum QP limit(%u)",
            (*rdevCb)->qp_cnt), -EINVAL);
    }

    return 0;
}

STATIC int RsInitMemPool(struct rs_qp_cb *qpCb)
{
    struct roce_mem_cq_qp_attr memAttr = {0};
    int ret;

    if ((qpCb->qp_mode != RA_RS_OP_QP_MODE && qpCb->qp_mode != RA_RS_OP_QP_MODE_EXT) ||
        qpCb->mem_align != LITE_ALIGN_2MB) {
        return 0;
    }

    // init mem_pool and store mem_data in mem_resp
    memAttr.mem_align = qpCb->mem_align;
    memAttr.send_qp_depth = qpCb->tx_depth;
    memAttr.send_cq_depth = (unsigned int)qpCb->send_cq_depth;
    memAttr.send_sge_num = qpCb->send_sge_num;
    memAttr.recv_qp_depth = qpCb->rx_depth;
    memAttr.recv_cq_depth = (unsigned int)qpCb->recv_cq_depth;
    memAttr.recv_sge_num = qpCb->recv_sge_num;

    ret = RsRoceInitMemPool(&memAttr, &qpCb->mem_resp.mem_data, qpCb->rdev_cb->rs_cb->chip_id);
    if (ret != 0) {
        hccp_err("rs_roce_init_mem_pool failed, ret=%d, chipId=%u", ret, qpCb->rdev_cb->rs_cb->chip_id);
    }
    return ret;
}

STATIC void RsDeinitMemPool(struct rs_qp_cb *qpCb)
{
    if ((qpCb->qp_mode != RA_RS_OP_QP_MODE && qpCb->qp_mode != RA_RS_OP_QP_MODE_EXT) ||
        qpCb->mem_align != LITE_ALIGN_2MB) {
        return;
    }

    (void)RsRoceDeinitMemPool(qpCb->mem_resp.mem_data.mem_idx);
}

STATIC int RsAllocQpcb(struct rs_rdev_cb *rdevCb, struct rs_qp_cb **qpCb, struct rs_qp_norm *qpNorm)
{
    int ret;

    ret = RsCallocQpcb(1, qpCb);
    CHK_PRT_RETURN(ret, hccp_err("alloc mem for qp_cb failed, ret:%d errno:%d", ret, errno), -ENOMEM);

    ret = pthread_mutex_init(&(*qpCb)->qp_mutex, NULL);
    if (ret) {
        hccp_err("pthread_mutex_init failed, ret %d", ret);
        goto qp_mutex_init_err;
    }

    ret = pthread_mutex_init(&(*qpCb)->cqe_err_info.mutex, NULL);
    if (ret) {
        hccp_err("pthread_mutex_init failed, ret %d", ret);
        goto cqe_mutex_init_err;
    }

    ret = RsQpcbInit(rdevCb, *qpCb, qpNorm);
    if (ret) {
        hccp_err("create qp tx rx failed ret %d", ret);
        goto rs_qpcb_init_err;
    }

    ret = RsInitMemPool(*qpCb);
    if (ret) {
        hccp_err("init mem pool failed ret %d", ret);
        goto rs_init_mem_err;
    }

    ret = RsDrvCreateCq(*qpCb, qpNorm->is_ext);
    if (ret) {
        hccp_err("create cq failed ret %d", ret);
        goto create_cq_err;
    }

    return 0;

create_cq_err:
    RsDeinitMemPool(*qpCb);

rs_init_mem_err:
    RsQpcbDeinit(rdevCb, *qpCb);

rs_qpcb_init_err:
    pthread_mutex_destroy(&(*qpCb)->cqe_err_info.mutex);

cqe_mutex_init_err:
    pthread_mutex_destroy(&(*qpCb)->qp_mutex);

qp_mutex_init_err:
    free(*qpCb);
    *qpCb = NULL;

    return ret;
}

STATIC void RsFreeQpcb(struct rs_rdev_cb *rdevCb, struct rs_qp_cb *qpCb)
{
    RsDrvDestroyCq(qpCb);
    RsDeinitMemPool(qpCb);
    (void)RsQpcbDeinit(rdevCb, qpCb);
    pthread_mutex_destroy(&qpCb->cqe_err_info.mutex);
    pthread_mutex_destroy(&qpCb->qp_mutex);
    free(qpCb);
    qpCb = NULL;
}

RS_ATTRI_VISI_DEF int RsQpCreate(unsigned int phyId, unsigned int rdevIndex, struct rs_qp_norm qpNorm,
    struct rs_qp_resp *qpResp)
{
    struct rs_rdev_cb *rdevCb = NULL;
    struct rs_qp_cb *qpCb = NULL;
    int ret;

    RS_QP_PARA_CHECK(phyId);
    CHK_PRT_RETURN(qpResp == NULL, hccp_err("qp_resp is NULL!"), -EINVAL);

    ret = RsQpQueryInfo(phyId, rdevIndex, &rdevCb, qpNorm.qp_mode);
    CHK_PRT_RETURN(ret, hccp_err("query qp info failed:%d", ret), ret);

    ret = RsAllocQpcb(rdevCb, &qpCb, &qpNorm);
    CHK_PRT_RETURN(ret, hccp_err("alloc mem for qp_cb failed, ret:%d", ret), ret);

    ret = RsDrvQpCreate(qpCb, &qpNorm);
    if (ret) {
        hccp_err("create drv qp create failed:%d", ret);
        goto create_qp_err;
    }

    ret = ibv_req_notify_cq(qpCb->ib_send_cq, 0);
    if (ret) {
        hccp_err("Couldn't request send CQ notification, ret:%d", ret);
        ret = -EOPENSRC;
        goto ret_noritfy_cq;
    }

    ret = ibv_req_notify_cq(qpCb->ib_recv_cq, 0);
    if (ret) {
        hccp_err("Couldn't request recv CQ notification, ret:%d", ret);
        ret = -EOPENSRC;
        goto ret_noritfy_cq;
    }

    ret = RsQpNotifyMr(rdevCb, qpCb, &qpResp->qpn);   // alloc mr
    if (ret) {
        hccp_err("store qp notify mr failed:%d", ret);
        goto ret_noritfy_cq;
    }

    if (qpNorm.is_exp) {
        qpCb->is_exp = RS_IS_EXP;
    } else {
        qpCb->is_exp = RS_NOT_EXP;
    }

    qpResp->qpn = (unsigned int)qpCb->qp_info_lo.qpn;
    qpResp->gid_idx = (unsigned int)qpCb->qp_info_lo.gid_idx;
    qpResp->psn = (unsigned int)qpCb->qp_info_lo.psn;
    qpResp->gid = qpCb->qp_info_lo.gid;

    return 0;

ret_noritfy_cq:
    RsDrvQpDestroy(qpCb);

create_qp_err:
    RsFreeQpcb(rdevCb, qpCb);
    return ret;
}

STATIC int RsQpcbInitWithAttrs(struct rs_rdev_cb *rdevCb, struct rs_qp_cb *qpCb,
    struct rs_qp_norm_with_attrs *qpNorm)
{
    int ret;

    qpCb->rdev_cb = rdevCb;
    RS_INIT_LIST_HEAD(&qpCb->mr_list);
    RS_INIT_LIST_HEAD(&qpCb->rem_mr_list);

    qpCb->qp_mode = qpNorm->ext_attrs.qp_mode;
    qpCb->num_recv_cq_events = 0;
    qpCb->num_send_cq_events = 0;
    qpCb->state = RS_QP_STATUS_DISCONNECT;
    qpCb->ib_pd = rdevCb->ib_pd;

    qpCb->tx_depth = qpNorm->ext_attrs.qp_attr.cap.max_send_wr;
    qpCb->rx_depth = qpNorm->ext_attrs.qp_attr.cap.max_send_wr;
    qpCb->send_sge_num = qpNorm->ext_attrs.qp_attr.cap.max_send_sge;
    qpCb->recv_sge_num = qpNorm->ext_attrs.qp_attr.cap.max_recv_sge;
    qpCb->send_cq_depth = qpNorm->ext_attrs.cq_attr.send_cq_depth;
    qpCb->recv_cq_depth = qpNorm->ext_attrs.cq_attr.recv_cq_depth;
    qpCb->mem_align = qpNorm->ext_attrs.mem_align;

    qpCb->channel = RsIbvCreateCompChannel(rdevCb->ib_ctx);
    CHK_PRT_RETURN(qpCb->channel == NULL, hccp_err("ibv_create_comp_channel failed! errno(%d)", errno), -EINVAL);
    qpCb->qos_attr.tc = (RS_ROCE_DSCP_33 & RS_DSCP_MASK) << RS_DSCP_OFF;
    qpCb->qos_attr.sl = RS_ROCE_4_SL;
    qpCb->timeout = RS_QP_ATTR_TIMEOUT;
    qpCb->retry_cnt = RS_QP_ATTR_RETRY_CNT;

    qpCb->udp_sport = qpNorm->ext_attrs.udp_sport;

    qpCb->ai_op_support = qpNorm->ai_op_support;
    qpCb->grp_id = rdevCb->rs_cb->grp_id;
    qpCb->cq_cstm_flag = qpNorm->ext_attrs.data_plane_flag.bs.cq_cstm;

    ret = RsEpollCtl(rdevCb->rs_cb->conn_cb.epollfd, EPOLL_CTL_ADD, qpCb->channel->fd, EPOLLIN | EPOLLRDHUP);
#ifndef CA_CONFIG_LLT
    if (ret) {
        RsIbvDestroyCompChannel(qpCb->channel);
        hccp_err("add channel fd failed ret %d", ret);
        return ret;
    }
#endif
    return 0;
}

STATIC int RsAllocQpcbWithAttrs(struct rs_rdev_cb *rdevCb, struct rs_qp_cb **qpCb,
    struct rs_qp_norm_with_attrs *qpNorm)
{
    int ret;

    ret = RsCallocQpcb(1, qpCb);
    CHK_PRT_RETURN(ret, hccp_err("alloc mem for qp_cb failed, ret:%d errno:%d", ret, errno), -ENOMEM);

    ret = pthread_mutex_init(&(*qpCb)->qp_mutex, NULL);
    if (ret) {
        hccp_err("pthread_mutex_init failed, ret %d", ret);
        goto qp_mutex_init_err;
    }

    ret = pthread_mutex_init(&(*qpCb)->cqe_err_info.mutex, NULL);
    if (ret) {
        hccp_err("pthread_mutex_init failed, ret %d", ret);
        goto cqe_mutex_init_err;
    }

    ret = RsQpcbInitWithAttrs(rdevCb, *qpCb, qpNorm);
    if (ret) {
        hccp_err("create qp tx rx failed ret %d", ret);
        goto rs_qpcb_init_err;
    }

    ret = RsInitMemPool(*qpCb);
    if (ret) {
        hccp_err("init mem pool failed ret %d", ret);
        goto rs_init_mem_err;
    }

    ret = RsDrvCreateCqWithAttrs(*qpCb, qpNorm->is_ext, &qpNorm->ext_attrs.cq_attr);
    if (ret) {
        hccp_err("create cq failed ret %d", ret);
        goto create_cq_err;
    }

    return 0;

create_cq_err:
    RsDeinitMemPool(*qpCb);

rs_init_mem_err:
    RsQpcbDeinit(rdevCb, *qpCb);

rs_qpcb_init_err:
    pthread_mutex_destroy(&(*qpCb)->cqe_err_info.mutex);

cqe_mutex_init_err:
    pthread_mutex_destroy(&(*qpCb)->qp_mutex);

qp_mutex_init_err:
    free(*qpCb);
    *qpCb = NULL;

    return ret;
}

STATIC int RsQpCheckQpNorm(struct rs_qp_norm_with_attrs *qpNorm, int *qpMode)
{
    CHK_PRT_RETURN(qpNorm == NULL, hccp_err("qp_norm is NULL!"), -EINVAL);
    CHK_PRT_RETURN(qpNorm->ext_attrs.version != QP_CREATE_WITH_ATTR_VERSION,
        hccp_err("attr version[%d] mismatch, expect [%d]", qpNorm->ext_attrs.version, QP_CREATE_WITH_ATTR_VERSION),
        -EINVAL);

    *qpMode = qpNorm->ext_attrs.qp_mode;
    if (*qpMode < 0 || *qpMode >= RA_RS_ERR_QP_MODE) {
        hccp_err("qp_mode[%d] must greater or equal to 0 and less than %d", *qpMode, RA_RS_ERR_QP_MODE);
        return -EINVAL;
    }

    if (*qpMode == RA_RS_OP_QP_MODE_EXT) {
        *qpMode = RA_RS_OP_QP_MODE;
    }

    qpNorm->ext_attrs.qp_mode = *qpMode;
    return 0;
}

#ifdef CUSTOM_INTERFACE
STATIC void RsQpPrepareCqDataPlaneInfo(struct ibv_cq *ibCq, struct ai_data_plane_cq *dataPlaneCq)
{
    struct hns_roce_cq_data_plane_info cqInfo = {0};

    (void)RsRoceGetCqDataPlaneInfo(ibCq, &cqInfo);
    dataPlaneCq->cqn = cqInfo.cqn;
    dataPlaneCq->buf_addr = cqInfo.buf_addr;
    dataPlaneCq->cqe_size = cqInfo.cqe_size;
    dataPlaneCq->depth = cqInfo.depth;
    dataPlaneCq->head_addr = cqInfo.head_addr;
    dataPlaneCq->tail_addr = cqInfo.tail_addr;
    dataPlaneCq->swdb_addr = cqInfo.swdb_addr;
    dataPlaneCq->db_reg = cqInfo.db_reg;
    hccp_info("cqn:%u buf_addr:0x%llx cqe_size:%u depth:%u head_addr:0x%llx tail_addr:0x%llx swdb_addr:0x%llx",
        dataPlaneCq->cqn, dataPlaneCq->buf_addr, dataPlaneCq->cqe_size, dataPlaneCq->depth,
        dataPlaneCq->head_addr, dataPlaneCq->tail_addr, dataPlaneCq->swdb_addr);
}

STATIC void RsQpPrepareWqDataPlaneInfo(struct hns_roce_wq_data_plane_info *wqInfo,
    struct ai_data_plane_wq *dataPlaneWq)
{
    dataPlaneWq->wqn = wqInfo->wqn;
    dataPlaneWq->buf_addr = wqInfo->buf_addr;
    dataPlaneWq->wqebb_size = wqInfo->wqebb_size;
    dataPlaneWq->depth = wqInfo->depth;
    dataPlaneWq->head_addr = wqInfo->head_addr;
    dataPlaneWq->tail_addr = wqInfo->tail_addr;
    dataPlaneWq->swdb_addr = wqInfo->swdb_addr;
    dataPlaneWq->db_reg = wqInfo->db_reg;
    hccp_info("wqn:%u buf_addr:0x%llx wqebb_size:%u depth:%u head_addr:%u tail_addr:%u swdb_addr:0x%llx",
        dataPlaneWq->wqn, dataPlaneWq->buf_addr, dataPlaneWq->wqebb_size, dataPlaneWq->depth,
        dataPlaneWq->head_addr, dataPlaneWq->tail_addr, dataPlaneWq->swdb_addr);
}

STATIC void RsQpPrepareQpDataPlaneInfo(struct ibv_qp *ibQp, struct ai_data_plane_wq *dataPlaneSq,
    struct ai_data_plane_wq *dataPlaneRq)
{
    struct hns_roce_qp_data_plane_info qpInfo = {0};

    (void)RsRoceGetQpDataPlaneInfo(ibQp, &qpInfo);
    RsQpPrepareWqDataPlaneInfo(&qpInfo.sq, dataPlaneSq);
    RsQpPrepareWqDataPlaneInfo(&qpInfo.rq, dataPlaneRq);
}

STATIC void RsQpPrepareDataPlaneInfo(struct rs_qp_norm_with_attrs *qpNorm, struct rs_qp_cb *qpCb,
    struct rs_qp_resp_with_attrs *qpResp)
{
    // skip to prepare cq data plane info
    if (qpNorm->ext_attrs.data_plane_flag.bs.cq_cstm != 0) {
        qpResp->ai_scq_addr = (unsigned long long)(uintptr_t)qpCb->ib_send_cq;
        qpResp->ai_rcq_addr = (unsigned long long)(uintptr_t)qpCb->ib_recv_cq;
        RsQpPrepareCqDataPlaneInfo(qpCb->ib_send_cq, &qpResp->data_plane_info.scq);
        RsQpPrepareCqDataPlaneInfo(qpCb->ib_recv_cq, &qpResp->data_plane_info.rcq);
    }

    // skip to prepare qp data plane info
    if (qpNorm->ai_op_support != 0) {
        RsQpPrepareQpDataPlaneInfo(qpCb->ib_qp, &qpResp->data_plane_info.sq, &qpResp->data_plane_info.rq);
    }
}
#endif

STATIC void RsQpPrepareQpResp(struct rs_qp_norm_with_attrs *qpNorm, struct rs_qp_cb *qpCb,
    struct rs_qp_resp_with_attrs *qpResp)
{
    if (qpNorm->is_exp != 0) {
        qpCb->is_exp = RS_IS_EXP;
    } else {
        qpCb->is_exp = RS_NOT_EXP;
    }

    qpResp->ai_qp_addr = (unsigned long long)(uintptr_t)qpCb->ib_qp;
    qpResp->sq_index = (unsigned int)qpCb->sq_index;
    qpResp->db_index = (unsigned int)qpCb->db_index;
    qpResp->gid_idx = (unsigned int)qpCb->qp_info_lo.gid_idx;
    qpResp->psn = (unsigned int)qpCb->qp_info_lo.psn;

#ifdef CUSTOM_INTERFACE
    RsQpPrepareDataPlaneInfo(qpNorm, qpCb, qpResp);
#endif

    return;
}

RS_ATTRI_VISI_DEF int RsQpCreateWithAttrs(unsigned int phyId, unsigned int rdevIndex,
    struct rs_qp_norm_with_attrs *qpNorm, struct rs_qp_resp_with_attrs *qpResp)
{
    struct rs_rdev_cb *rdevCb = NULL;
    struct rs_qp_cb *qpCb = NULL;
    int qpMode;
    int ret;

    RS_QP_PARA_CHECK(phyId);
    CHK_PRT_RETURN(qpResp == NULL, hccp_err("qp_resp is NULL!"), -EINVAL);

    ret = RsQpCheckQpNorm(qpNorm, &qpMode);
    CHK_PRT_RETURN(ret != 0, hccp_err("check qp mode failed, ret:%d", ret), ret);

    ret = RsQpQueryInfo(phyId, rdevIndex, &rdevCb, qpMode);
    CHK_PRT_RETURN(ret, hccp_err("query qp info failed:%d", ret), ret);

    ret = RsAllocQpcbWithAttrs(rdevCb, &qpCb, qpNorm);
    CHK_PRT_RETURN(ret, hccp_err("alloc mem for qp_cb failed, ret:%d", ret), ret);

    ret = RsDrvQpCreateWithAttrs(qpCb, qpNorm);
    if (ret) {
        hccp_err("create drv qp create failed:%d", ret);
        goto create_qp_err;
    }

    ret = ibv_req_notify_cq(qpCb->ib_send_cq, 0);
    if (ret) {
        hccp_err("Couldn't request send CQ notification, ret:%d", ret);
        ret = -EOPENSRC;
        goto ret_noritfy_cq;
    }

    ret = ibv_req_notify_cq(qpCb->ib_recv_cq, 0);
    if (ret) {
        hccp_err("Couldn't request recv CQ notification, ret:%d", ret);
        ret = -EOPENSRC;
        goto ret_noritfy_cq;
    }

    ret = RsQpNotifyMr(rdevCb, qpCb, &qpResp->qpn);   // alloc mr
    if (ret) {
        hccp_err("store qp notify mr failed:%d", ret);
        goto ret_noritfy_cq;
    }

    RsQpPrepareQpResp(qpNorm, qpCb, qpResp);

    return 0;

ret_noritfy_cq:
    RsDrvQpDestroy(qpCb);

create_qp_err:
    RsFreeQpcb(rdevCb, qpCb);
    return ret;
}

RS_ATTRI_VISI_DEF int RsQpDestroy(unsigned int phyId, unsigned int rdevIndex, unsigned int qpn)
{
    int ret;
    struct rs_qp_cb *qpCb = NULL;
    struct rs_mr_cb *mrTmp = NULL;
    struct rs_mr_cb *mrTmp2 = NULL;

    RS_QP_PARA_CHECK(phyId);
    ret = RsQpn2qpcb(phyId, rdevIndex, qpn, &qpCb);
    CHK_PRT_RETURN(ret || qpCb == NULL, hccp_err("get qp cb failed qpn %u, ret %d", qpn, ret), ret);

    RS_PTHREAD_MUTEX_LOCK(&qpCb->rdev_cb->rdev_mutex);
    RsListDel(&qpCb->list);
    RS_PTHREAD_MUTEX_ULOCK(&qpCb->rdev_cb->rdev_mutex);
    RsIbvAckCqEvents(qpCb->ib_send_cq, qpCb->num_send_cq_events);
    RsIbvAckCqEvents(qpCb->ib_recv_cq, qpCb->num_recv_cq_events);

    // dereg mr
    RS_PTHREAD_MUTEX_LOCK(&qpCb->qp_mutex);
    RS_LIST_GET_HEAD_ENTRY(mrTmp, mrTmp2, &qpCb->mr_list, list, struct rs_mr_cb);
    for (; (&mrTmp->list) != &qpCb->mr_list;
        mrTmp = mrTmp2, mrTmp2 = list_entry(mrTmp2->list.next, struct rs_mr_cb, list)) {
        if (mrTmp->ib_mr != qpCb->rdev_cb->notify_mr) {
            (void)RsDrvMrDereg(mrTmp->ib_mr);
        }
        RsListDel(&mrTmp->list);
        free(mrTmp);
        mrTmp = NULL;
    }

    RS_LIST_GET_HEAD_ENTRY(mrTmp, mrTmp2, &qpCb->rem_mr_list, list, struct rs_mr_cb);
    for (; (&mrTmp->list) != &qpCb->rem_mr_list;
        mrTmp = mrTmp2, mrTmp2 = list_entry(mrTmp2->list.next, struct rs_mr_cb, list)) {
        RsListDel(&mrTmp->list);
        free(mrTmp);
        mrTmp = NULL;
    }
    RS_PTHREAD_MUTEX_ULOCK(&qpCb->qp_mutex);

    // destroy qp
    RsDrvQpDestroy(qpCb);
    RsDrvDestroyCq(qpCb);
    RsDeinitMemPool(qpCb);

    qpCb->rdev_cb->qp_cnt--;
    ret = RsQpcbDeinit(qpCb->rdev_cb, qpCb);
    if (ret) {
        hccp_err("rs_qpcb_deinit failed! ret[%d]", ret);
    }

    pthread_mutex_destroy(&qpCb->cqe_err_info.mutex);
    pthread_mutex_destroy(&qpCb->qp_mutex);
    hccp_info("qp %d destroy qp, send wr[%u].", qpn, qpCb->send_wr_num);

    free(qpCb);
    qpCb = NULL;
    return ret;
}

static void RsQpConnectAsyncMr(const struct rs_qp_cb *qpCb)
{
    int ret;
    struct rs_mr_cb *mrCb = NULL;
    struct rs_mr_cb *mrCb2 = NULL;

    RS_LIST_GET_HEAD_ENTRY(mrCb, mrCb2, &qpCb->mr_list, list, struct rs_mr_cb);
    for (; (&mrCb->list) != &qpCb->mr_list;
        mrCb = mrCb2, mrCb2 = list_entry(mrCb2->list.next, struct rs_mr_cb, list)) {
        ret = RsMrInfoSync(mrCb);
        if (ret) {
            hccp_warn("rs_mr_info_sync unsuccessful, ret:%d", ret);
        }
    }
}

STATIC void RsQpConnectAsyncQpcbSet(int fd, struct rs_qp_cb *qpCb)
{
    int ret;
    ret = RsSocketSend(fd, &qpCb->qp_info_lo, sizeof(struct rs_qp_info));
    if (ret == sizeof(struct rs_qp_info)) {
        qpCb->send_len += (uint32_t)ret;
        qpCb->state = RS_QP_STATUS_CONNECTING;
    } else {
        qpCb->state = RS_QP_STATUS_TIMEOUT;
    }
}

STATIC void RsQpConnectAsyncLength(int fd, struct rs_qp_cb *qpCb)
{
    int ret;
    struct rs_qp_len_info msg;

    msg.cmd = RS_CMD_LEN_INFO;
    msg.len = qpCb->send_len;

    ret = RsSocketSend(fd, &msg, sizeof(struct rs_qp_len_info));
    if (ret != sizeof(struct rs_qp_len_info)) {
        qpCb->state = RS_QP_STATUS_TIMEOUT;
    }
}

static int RsQpConnectAsyncInitPara(struct rs_qp_conn_para qpConnPara, int fd,
    struct rs_qp_cb **qpCb, struct rs_conn_info **conn)
{
    int ret;

    CHK_PRT_RETURN(qpConnPara.phy_id >= RS_MAX_DEV_NUM, hccp_err("param error ! phy_id:%u",
        qpConnPara.phy_id), -EINVAL);

    CHK_PRT_RETURN(fd < 0, hccp_err("param error ! fd:%d must bigger than 0", fd), -EINVAL);

    ret = RsQpn2qpcb(qpConnPara.phy_id, qpConnPara.rdev_index, qpConnPara.qpn, qpCb);
    CHK_PRT_RETURN(ret, hccp_err("get qpcb failed, qpn %u, ret %d", qpConnPara.qpn, ret), ret);

    ret = RsFd2conn(fd, conn);
    CHK_PRT_RETURN(ret, hccp_err("get conn failed, fd %d, ret %d", fd, ret), ret);

    RsGetCurTime(&((*qpCb)->start_time));
    (*qpCb)->send_len = 0;
    (*qpCb)->recv_len = 0;
    (*qpCb)->expect_len = 0;
    (*qpCb)->conn_info = *conn;

    return 0;
}

STATIC int RsTypicalQpStateModifytoRtr(struct rs_qp_cb *qpCb, struct typical_qp *localQpInfo,
    struct typical_qp *remoteQpInfo)
{
    struct ibv_port_attr portAttr = { 0 };
    union ibv_gid remoteInfoGid = { 0 };
    struct ibv_qp_attr attr = { 0 };
    int ret;

    attr.qp_state                  = IBV_QPS_RTR;
    attr.dest_qp_num               = remoteQpInfo->qpn;
    attr.rq_psn                    = remoteQpInfo->psn;
    attr.min_rnr_timer             = RS_QP_ATTR_MIN_RNR_TIMER;
    (attr.ah_attr).is_global       = 0;
    (attr.ah_attr).sl              = localQpInfo->sl;
    (attr.ah_attr).src_path_bits   = 0;
    (attr.ah_attr).port_num        = qpCb->rdev_cb->ib_port;

    attr.path_mtu = RsDrvSetMtu(qpCb);
    CHK_PRT_RETURN(attr.path_mtu < IBV_MTU_1024, hccp_err("qpn[%u] failed to set mtu, mtu[%d] < [%d]",
        localQpInfo->qpn, attr.path_mtu, IBV_MTU_1024), -EPERM);
    if (qpCb->rdev_cb->rs_cb->hccp_mode == NETWORK_PEER_ONLINE) {
        attr.max_dest_rd_atomic = RS_MAX_RD_ATOMIC_NUM_PEER_ONLINE;
    } else {
        attr.max_dest_rd_atomic = RS_MAX_RD_ATOMIC_NUM;
    }
    (attr.ah_attr).grh.traffic_class = localQpInfo->tc;
    // get gid_idx dynamically to avoid gid_idx changed issue: refresh gid_idx when it changed
    ret = RsDrvGetGidIndex(qpCb->rdev_cb, &portAttr, &qpCb->qp_info_lo.gid_idx);
    if (ret == 0 && localQpInfo->gid_idx != (uint32_t)qpCb->qp_info_lo.gid_idx) {
        hccp_warn("qpn[%u] qp_mode[%d] refresh gid_idx[%u] to [%d]", localQpInfo->qpn, qpCb->qp_mode,
            localQpInfo->gid_idx, qpCb->qp_info_lo.gid_idx);
        localQpInfo->gid_idx = (uint32_t)qpCb->qp_info_lo.gid_idx;
    }

    (void)memcpy_s(remoteInfoGid.raw, HCCP_GID_RAW_LEN, remoteQpInfo->gid, HCCP_GID_RAW_LEN);
    if (remoteInfoGid.global.interface_id) {
        attr.ah_attr.is_global = 1;
        attr.ah_attr.grh.hop_limit = 1;
        attr.ah_attr.grh.dgid = remoteInfoGid;
        attr.ah_attr.grh.sgid_index = localQpInfo->gid_idx;
    }

    ret = RsIbvModifyQp(qpCb->ib_qp, &attr,
                           IBV_QP_STATE | IBV_QP_AV |
                           IBV_QP_PATH_MTU | IBV_QP_DEST_QPN |
                           IBV_QP_RQ_PSN | IBV_QP_MAX_DEST_RD_ATOMIC |
                           IBV_QP_MIN_RNR_TIMER);
    CHK_PRT_RETURN(ret, hccp_err("[modifyto_rtr]local_qpn[%u] remote_qpn[%u] ibv_modify_qp failed ret[%d], errno[%d]",
        localQpInfo->qpn, remoteQpInfo->qpn, ret, errno), -EOPENSRC);
    hccp_info("qp qos attr: qpn[%u] tc[%u] sl[%u]", localQpInfo->qpn, localQpInfo->tc, localQpInfo->sl);
    return 0;
}

STATIC int RsTypicalQpStateModifytoRts(struct rs_qp_cb *qpCb, struct typical_qp *localQpInfo)
{
    struct ibv_qp_attr attr = {0};
    int ret;

    attr.qp_state      = IBV_QPS_RTS;
    attr.timeout       = (uint8_t)localQpInfo->retry_time;
    attr.retry_cnt     = (uint8_t)localQpInfo->retry_cnt;
    attr.rnr_retry     = RS_QP_ATTR_RNR_RETRY;
    attr.sq_psn        = localQpInfo->psn;
    if (qpCb->rdev_cb->rs_cb->hccp_mode == NETWORK_PEER_ONLINE) {
        attr.max_rd_atomic = RS_MAX_RD_ATOMIC_NUM_PEER_ONLINE;
    } else {
        attr.max_rd_atomic = RS_MAX_RD_ATOMIC_NUM;
    }

    ret = RsIbvModifyQp(qpCb->ib_qp, &attr,
                           IBV_QP_STATE | IBV_QP_TIMEOUT |
                           IBV_QP_RETRY_CNT | IBV_QP_RNR_RETRY |
                           IBV_QP_SQ_PSN | IBV_QP_MAX_QP_RD_ATOMIC);
    CHK_PRT_RETURN(ret != 0, hccp_err("[modifyto_rts]local_qpn[%u] ibv_modify_qp failed ret[%d], errno[%d]",
        localQpInfo->qpn, ret, errno), -EOPENSRC);

    hccp_info("qp rdma attr: qpn[%u] timeout[%u] retrycnt[%u]", localQpInfo->qpn, localQpInfo->retry_time,
        localQpInfo->retry_cnt);
    return 0;
}

STATIC void RsTypicalQpModifyInfoRelated(struct rs_qp_cb *qpCb, struct typical_qp *localQpInfo,
    struct typical_qp *remoteQpInfo)
{
    qpCb->state = RS_QP_STATUS_CONNECTED;
    // local qp info related: no need to relate qpn, psn, gid_idx, gid
    qpCb->qos_attr.tc = (unsigned char)localQpInfo->tc;
    qpCb->qos_attr.sl = (unsigned char)localQpInfo->sl;
    qpCb->retry_cnt = localQpInfo->retry_cnt;
    qpCb->timeout = localQpInfo->retry_time;
    // remote qp info related
    qpCb->qp_info_rem.qpn = (int)remoteQpInfo->qpn;
    qpCb->qp_info_rem.psn = (int)remoteQpInfo->psn;
    qpCb->qp_info_rem.gid_idx = (int)remoteQpInfo->gid_idx;
    (void)memcpy_s(qpCb->qp_info_rem.gid.raw, HCCP_GID_RAW_LEN, remoteQpInfo->gid, HCCP_GID_RAW_LEN);
}

RS_ATTRI_VISI_DEF int RsTypicalQpModify(unsigned int phyId, unsigned int rdevIndex,
    struct typical_qp localQpInfo, struct typical_qp remoteQpInfo, unsigned int *udpSport)
{
#ifdef CUSTOM_INTERFACE
    unsigned int qpAttrMask = HNS_ROCE_AI_QPC_UDPSPN;
    struct hns_roce_qpc_attr_val qpAttrVal = { 0 };
#endif
    struct ibv_qp_init_attr initAttr = { 0 };
    struct ibv_qp_attr attr = { 0 };
    struct rs_qp_cb *qpCb = NULL;
    int ret;

    CHK_PRT_RETURN(phyId >= RS_MAX_DEV_NUM, hccp_err("[modify]phy_id:%u >= [%d], is invalid", phyId, RS_MAX_DEV_NUM),
        -EINVAL);

    CHK_PRT_RETURN(RsQpn2qpcb(phyId, rdevIndex, localQpInfo.qpn, &qpCb),
        hccp_err("[modify]rs_qpn2qpcb qpn:%u failed, phyId[%u]", localQpInfo.qpn, phyId), -EACCES);

    CHK_PRT_RETURN(qpCb->state == RS_QP_STATUS_CONNECTED,
        hccp_info("local_qpn:%u remote_qpn:%u already been connected, no need to modify again",
        localQpInfo.qpn, remoteQpInfo.qpn), 0);

    // see ib_modify_qp_is_ok for status modify, only support modify qp from INIT to RTR
    ret = RsIbvQueryQp(qpCb->ib_qp, &attr, IBV_QP_STATE, &initAttr);
    CHK_PRT_RETURN(ret != 0 || attr.qp_state != IBV_QPS_INIT, hccp_err("query qpn:%u failed, ret:%d or state:%d != %d",
        localQpInfo.qpn, ret, attr.qp_state, IBV_QPS_INIT), -EOPENSRC);

    ret = RsTypicalQpStateModifytoRtr(qpCb, &localQpInfo, &remoteQpInfo);
    CHK_PRT_RETURN(ret != 0, hccp_err("[modify]local_qpn:%u remote_qpn:%u modify to rtr failed, ret %d",
        localQpInfo.qpn, remoteQpInfo.qpn, ret), ret);

    ret = RsTypicalQpStateModifytoRts(qpCb, &localQpInfo);
    CHK_PRT_RETURN(ret != 0, hccp_err("[modify]local_qpn:%u remote_qpn:%u modify to rts failed, ret %d",
        localQpInfo.qpn, remoteQpInfo.qpn, ret), ret);

#ifdef CUSTOM_INTERFACE
    ret = RsRoceQueryQpc(qpCb->ib_qp, &qpAttrVal, qpAttrMask);
    if (ret != 0) {
        hccp_warn("qpn:%d query qpc unsuccessful, ret %d", localQpInfo.qpn, ret);
    } else {
        qpCb->udp_sport = qpAttrVal.udp_sport;
    }
#endif
    *udpSport = qpCb->udp_sport;
    RsTypicalQpModifyInfoRelated(qpCb, &localQpInfo, &remoteQpInfo);

    hccp_info("local_qpn:%u remote_qpn:%u modify succ, udpSport:%u",
        localQpInfo.qpn, remoteQpInfo.qpn, qpCb->udp_sport);

    return 0;
}

STATIC int RsQpStateBatchModifytoPause(struct rs_qp_cb *qpCb)
{
    int ret;

    ret = RsDrvQpStateModifytoReset(qpCb);
    CHK_PRT_RETURN(ret, hccp_err("qp modify to reset failed, ret %d", ret), ret);

    hccp_info("local qpn[%d] remote qpn[%d] modify to pause succ", qpCb->qp_info_lo.qpn, qpCb->qp_info_rem.qpn);
    return 0;
}

STATIC int RsQpStateBatchModifytoConnected(struct rs_qp_cb *qpCb)
{
    struct ibv_qp_attr attr;
    int ret;

    ret = memset_s(&attr, sizeof(struct ibv_qp_attr), 0, sizeof(struct ibv_qp_attr));
    CHK_PRT_RETURN(ret, hccp_err("memset_s attr failed ret %d", ret), -ESAFEFUNC);

    ret = RsDrvQpStateModifytoInit(qpCb, &attr);
    CHK_PRT_RETURN(ret, hccp_err("qp modify to init failed, ret %d", ret), ret);
    ret = RsDrvQpStateModifytoRtr(qpCb, &attr);
    CHK_PRT_RETURN(ret, hccp_err("qp modify to rtr failed, ret %d", ret), ret);
    ret = RsDrvQpStateModifytoRts(qpCb, &attr);
    CHK_PRT_RETURN(ret, hccp_err("qp modify to rts failed, ret %d", ret), ret);

    hccp_info("local qpn[%d] remote qpn[%d] modify to rts succ", qpCb->qp_info_lo.qpn, qpCb->qp_info_rem.qpn);
    return 0;
}

RS_ATTRI_VISI_DEF int RsQpBatchModify(unsigned int phyId, unsigned int rdevIndex,
    int status, int qpn[], int qpnNum)
{
    struct rs_qp_cb *qpCb = NULL;
    int ret;
    int i;

    CHK_PRT_RETURN(phyId >= RS_MAX_DEV_NUM, hccp_err("[modify]phy_id:%u >= [%d], is invalid", phyId, RS_MAX_DEV_NUM),
        -EINVAL);

    for (i = 0; i < qpnNum; i++) {
        CHK_PRT_RETURN(RsQpn2qpcb(phyId, rdevIndex, (uint32_t)qpn[i], &qpCb),
            hccp_err("[modify]rs_qpn2qpcb failed, phyId[%u]", phyId), -EACCES);

        /*
         * see ib_modify_qp_is_ok for status modify
         * only support modify qp from STATUS_PAUSE(RESET) to STATUS_CONNECTED(INIT)
         */
        if (status == RS_QP_STATUS_CONNECTED && qpCb->state == RS_QP_STATUS_PAUSE) {
            ret = RsQpStateBatchModifytoConnected(qpCb);
            CHK_PRT_RETURN(ret, hccp_err("modify_qp qpn[%d]:%d to connected failed, ret[%d] phyId[%u]",
                i, qpn[i], ret, phyId), ret);
        } else if (status == RS_QP_STATUS_PAUSE) {
            ret = RsQpStateBatchModifytoPause(qpCb);
            CHK_PRT_RETURN(ret, hccp_err("modify_qp qpn[%d]:%d to pause failed, ret[%d] phyId[%u]",
                i, qpn[i], ret, phyId), ret);
        } else {
            hccp_err("modify_qp qpn[%d]:%d failed, not support to modify status[%d] to status[%d], phyId[%u]",
                i, qpn[i], qpCb->state, status, phyId);
            return -EINVAL;
        }

        qpCb->state = status;
    }

    return 0;
}

RS_ATTRI_VISI_DEF int RsQpConnectAsync(unsigned int phyId, unsigned int rdevIndex, unsigned int qpn, int fd)
{
    int ret;
    struct rs_qp_cb *qpCb = NULL;
    struct rs_conn_info *conn = NULL;
    struct rs_qp_conn_para qpConnPara;
    hccp_info("qp:%d, fd:%d", qpn, fd);

    qpConnPara.phy_id = phyId;
    qpConnPara.rdev_index = rdevIndex;
    qpConnPara.qpn = qpn;
    ret = RsQpConnectAsyncInitPara(qpConnPara, fd, &qpCb, &conn);
    CHK_PRT_RETURN(ret, hccp_err("rs_qp_connect_async_init_para failed, qpn %u, ret %d", qpn, ret), ret);

    RS_PTHREAD_MUTEX_LOCK(&qpCb->qp_mutex);

    if (qpCb->state == RS_QP_STATUS_REM_FD_CLOSE) {
        hccp_warn("remote qp fd close, can not use it anymore!");
        RS_PTHREAD_MUTEX_ULOCK(&qpCb->qp_mutex);
        return -EFAULT;
    }

    if ((qpCb->state == RS_QP_STATUS_CONNECTED) || (qpCb->state == RS_QP_STATUS_CONNECTING)) {
        hccp_warn("qp %d has already sync! state[%d]", qpCb->qp_info_lo.qpn, qpCb->state);
        RS_PTHREAD_MUTEX_ULOCK(&qpCb->qp_mutex);
        return -EEXIST;
    }

    RsQpConnectAsyncQpcbSet(fd, qpCb);

    hccp_info("after socket fd %d send QP %u, chipId %u, state:%d!",
        fd, qpn, qpCb->rdev_cb->rs_cb->chip_id, qpCb->state);

    RS_PTHREAD_MUTEX_ULOCK(&qpCb->qp_mutex);

    RsQpMrRecvHandle(fd, qpCb);

    RsQpConnectAsyncMr(qpCb);

    RsQpConnectAsyncLength(fd, qpCb);

    hccp_info("QP %d async done, state:%d!", qpn, qpCb->state);

    return 0;
}

RS_ATTRI_VISI_DEF int RsGetQpStatus(unsigned int phyId, unsigned int rdevIndex, unsigned int qpn,
    struct rs_qp_status_info *qpInfo)
{
#ifdef CUSTOM_INTERFACE
    unsigned int qpAttrMask = HNS_ROCE_AI_QPC_UDPSPN;
    struct hns_roce_qpc_attr_val qpAttrVal = { 0 };
#endif
    struct rs_qp_cb *qpCb = NULL;
    int ret;

    CHK_PRT_RETURN(qpInfo == NULL, hccp_err("param error, qpInfo is NULL"), -EINVAL);

    CHK_PRT_RETURN(phyId >= RS_MAX_DEV_NUM, hccp_err("phy_id:%u >= [%d], is invalid",
        phyId, RS_MAX_DEV_NUM), -EINVAL);

    ret = RsQpn2qpcb(phyId, rdevIndex, qpn, &qpCb);
    CHK_PRT_RETURN(ret, hccp_err("get qp cb failed, qpn:%u, ret %d", qpn, ret), ret);

    // qp state is CONNECTED, no need to handle
    if (qpCb->state == RS_QP_STATUS_CONNECTED) {
        goto update_qp_cb;
    }

    // modify state to CONNECTED
    if (qpCb->expect_len == qpCb->recv_len - sizeof(struct rs_qp_len_info)) {
        qpCb->state = RS_QP_STATUS_CONNECTED;
    } else {
        RsQpMrRecvHandle(qpCb->conn_info->connfd, qpCb);
        goto out;
    }

update_qp_cb:
#ifdef CUSTOM_INTERFACE
    ret = RsRoceQueryQpc(qpCb->ib_qp, &qpAttrVal, qpAttrMask);
    if (ret != 0) {
        hccp_warn("qpn:%d query qpc unsuccessful, ret %d", qpCb->qp_info_lo.qpn, ret);
    } else {
        qpCb->udp_sport = qpAttrVal.udp_sport;
    }
#endif
out:
    hccp_dbg("qp:%u, state:%d, udpSport:%u", qpn, qpCb->state, qpCb->udp_sport);
    qpInfo->status = qpCb->state;
    qpInfo->udp_sport = qpCb->udp_sport;

    return 0;
}

RS_ATTRI_VISI_DEF int RsGetQpContext(unsigned int phyId, unsigned int rdevIndex, unsigned int qpn, void** qp,
    void** sendCq, void** recvCq)
{
    int ret;
    struct rs_qp_cb *qpCb = NULL;

    CHK_PRT_RETURN(phyId >= RS_MAX_DEV_NUM, hccp_err("phy_id:%u >= [%d], is invalid", phyId, RS_MAX_DEV_NUM),
        -EINVAL);

    ret = RsQpn2qpcb(phyId, rdevIndex, qpn, &qpCb);
    CHK_PRT_RETURN(ret, hccp_err("rs_qpn2qpcb failed ret[%d]", ret), ret);

    *qp = qpCb->ib_qp;
    *sendCq = qpCb->ib_send_cq;
    *recvCq = qpCb->ib_recv_cq;

    hccp_dbg("qpn[%u] succ", qpn);

    return 0;
}

STATIC int RsQueryRdevCb(unsigned int phyId, unsigned int rdevIndex, struct rs_rdev_cb **rdevCb)
{
    int ret;
    unsigned int chipId;
    struct rs_cb *rsCb = NULL;

    RS_QP_PARA_CHECK(phyId);

    ret = rsGetLocalDevIDByHostDevID(phyId, &chipId);
    CHK_PRT_RETURN(ret, hccp_err("rs_query_rdev_cb phy_id[%u] invalid, ret:%d", phyId, ret), ret);

    ret = RsDev2rscb(chipId, &rsCb, false);
    CHK_PRT_RETURN(ret, hccp_err("rs_query_rdev_cb get rs_cb failed, ret:%d", ret), -ENODEV);

    ret = RsGetRdevCb(rsCb, rdevIndex, rdevCb);
    CHK_PRT_RETURN(ret, hccp_err("rs_get_rdev_cb failed! ret:%d, rdevIndex:%u", ret, rdevIndex), ret);

    return 0;
}

STATIC int RsBuildUpQpcb(struct rs_cq_context *cqContext, struct ibv_qp_init_attr *qpInitAttr,
    struct rs_qp_cb **qpCb)
{
    int ret;

    ret = RsCallocQpcb(1, qpCb);
    CHK_PRT_RETURN(ret, hccp_err("alloc mem for qp_cb failed, ret:%d errno:%d", ret, errno), -ENOMEM);

    ret = pthread_mutex_init(&(*qpCb)->qp_mutex, NULL);
    if (ret) {
        hccp_err("pthread_mutex_init failed, ret %d", ret);
        goto pthread_mutex_init_err;
    }

    (*qpCb)->rdev_cb = cqContext->rdev_cb;
    RS_INIT_LIST_HEAD(&(*qpCb)->mr_list);
    RS_INIT_LIST_HEAD(&(*qpCb)->rem_mr_list);

    (*qpCb)->eq_num = cqContext->eq_num;
    (*qpCb)->channel = cqContext->channel;
    (*qpCb)->ib_send_cq = cqContext->ib_send_cq;
    (*qpCb)->ib_recv_cq = cqContext->ib_recv_cq;
    (*qpCb)->send_event = cqContext->send_event;
    (*qpCb)->recv_event = cqContext->recv_event;
    (*qpCb)->num_recv_cq_events = 0;
    (*qpCb)->num_send_cq_events = 0;
    (*qpCb)->srq_context = cqContext->srq_context;
    (*qpCb)->state = RS_QP_STATUS_DISCONNECT;
    (*qpCb)->ib_pd = cqContext->rdev_cb->ib_pd;
    (*qpCb)->tx_depth = qpInitAttr->cap.max_send_wr;
    (*qpCb)->rx_depth = qpInitAttr->cap.max_recv_wr;
    (*qpCb)->qos_attr.tc = (RS_ROCE_DSCP_33 & RS_DSCP_MASK) << RS_DSCP_OFF;
    (*qpCb)->qos_attr.sl = RS_ROCE_4_SL;
    (*qpCb)->timeout = RS_QP_ATTR_TIMEOUT;
    (*qpCb)->retry_cnt = RS_QP_ATTR_RETRY_CNT;

    return 0;

pthread_mutex_init_err:
    free(*qpCb);
    (*qpCb) = NULL;
    return ret;
}

RS_ATTRI_VISI_DEF int RsCreateCqEvent(struct rs_cq_context *cqContext, struct cq_attr *attr)
{
    int ret;
    cqContext->channel = RsIbvCreateCompChannel(cqContext->rdev_cb->ib_ctx);

    if (cqContext->channel == NULL) {
        hccp_err("ibv_create_comp_channel failed, ret %d, errno(%d)", -EINVAL, errno);
        return -EINVAL;
    }

    hccp_info("comp channel fd[%d].", cqContext->channel->fd);
    ret = RsEpollCtl(cqContext->rdev_cb->rs_cb->conn_cb.epollfd, EPOLL_CTL_ADD,
        cqContext->channel->fd, EPOLLIN | EPOLLRDHUP);
#ifndef CA_CONFIG_LLT
    if (ret) {
        hccp_err("add channel fd failed ret %d", ret);
        goto rs_cq_epoll_ctl_err;
    }
#endif

    ret = RsDrvCreateCqEvent(cqContext, attr);
    if (ret) {
        hccp_err("create drv cq event failed:%d", ret);
        goto rs_cq_create_err;
    }

    return ret;
rs_cq_create_err:
    ret = RsEpollCtl(cqContext->rdev_cb->rs_cb->conn_cb.epollfd, EPOLL_CTL_DEL, cqContext->channel->fd,
        EPOLLIN | EPOLLRDHUP);
#ifndef CA_CONFIG_LLT
    if (ret) {
        hccp_err("del channel fd failed ret %d", ret);
    }
#endif
rs_cq_epoll_ctl_err:
    if (cqContext->channel != NULL) {
        RsIbvDestroyCompChannel(cqContext->channel);
        cqContext->channel = NULL;
    }
    return ret;
}

RS_ATTRI_VISI_DEF int RsCqCreate(unsigned int phyId, unsigned int rdevIndex, struct cq_attr *attr)
{
    int ret;
    struct rs_rdev_cb *rdevCb = NULL;
    struct rs_cq_context *cqContext = NULL;

    ret = RsQueryRdevCb(phyId, rdevIndex, &rdevCb);
    if (ret) {
        hccp_err("rs_query_rdev_cb phy_id[%u] rdev_index[%u], ret %d", phyId, rdevIndex, ret);
        return ret;
    }

    cqContext = calloc(1, sizeof(struct rs_cq_context));
    if (cqContext == NULL) {
        return -ENOMEM;
    }
    cqContext->rdev_cb = rdevCb;
    cqContext->eq_num = 0;
    if (attr->send_channel == NULL && attr->recv_channel == NULL) {
        if (*attr->ib_send_cq == NULL && *attr->ib_recv_cq != NULL) {
            // 只创建sq cq
            cqContext->cq_create_mode = RS_SQ_CQ_CREATE;
            cqContext->ib_recv_cq = *attr->ib_recv_cq;
            cqContext->srq_context = attr->srq_context;
        } else {
            // 创建sq&rq cq
            cqContext->cq_create_mode = RS_NORMAL_CQ_CREATE;
        }
        ret = RsCreateCqEvent(cqContext, attr);
        if (ret) {
            hccp_err("create cq event failed:%d", ret);
            goto rs_cq_create_err;
        }
    } else if (attr->send_channel != NULL && attr->recv_channel != NULL) {
        // 使用输入comp channel创建sq&rq
        ret = RsDrvCreateCqWithChannel(cqContext, attr);
        if (ret) {
            hccp_err("create drv cq with channel failed:%d", ret);
            goto rs_cq_create_err;
        }
    } else {
        hccp_err("rs create cq failed, send_channel or recv_channel is NULL.");
        ret = -EPERM;
        goto rs_cq_create_err;
    }

    *attr->qp_context = cqContext;
    return 0;

rs_cq_create_err:
    free(cqContext);
    cqContext = NULL;

    return ret;
}

RS_ATTRI_VISI_DEF int RsCqDestroy(unsigned int phyId, unsigned int rdevIndex, struct cq_attr *attr)
{
    int ret;
    struct rs_rdev_cb *rdevCb = NULL;
    struct rs_cq_context *cqContext = NULL;

    ret = RsQueryRdevCb(phyId, rdevIndex, &rdevCb);
    CHK_PRT_RETURN(ret, hccp_err("rs_query_rdev_cb phy_id[%u] rdev_index[%u], ret %d", phyId, rdevIndex, ret), ret);

    cqContext = *attr->qp_context;

    ret = RsDrvDestroyCqEvent(cqContext);
    if (ret) {
        hccp_err("rs_drv_destroy_cq_event failed ret %d", ret);
    }

    if (cqContext->channel != NULL) {
        ret = RsEpollCtl(rdevCb->rs_cb->conn_cb.epollfd, EPOLL_CTL_DEL, cqContext->channel->fd,
            EPOLLIN | EPOLLRDHUP);
#ifndef CA_CONFIG_LLT
            if (ret) {
                hccp_err("del channel fd failed ret %d", ret);
            }
#endif
        RsIbvDestroyCompChannel(cqContext->channel);
        cqContext->channel = NULL;
    }

    free(cqContext);
    cqContext = NULL;

    return ret;
}

RS_ATTRI_VISI_DEF int RsNormalQpCreate(unsigned int phyId, unsigned int rdevIndex,
    struct ibv_qp_init_attr *qpInitAttr, struct rs_qp_resp *qpResp, void **qp)
{
    struct rs_cq_context *cqContext = NULL;
    struct rs_rdev_cb *rdevCb = NULL;
    struct rs_qp_cb *qpCb = NULL;
    int ret;

    CHK_PRT_RETURN(qpResp == NULL, hccp_err("qp_resp is NULL!"), -EINVAL);
    ret = RsQueryRdevCb(phyId, rdevIndex, &rdevCb);
    CHK_PRT_RETURN(ret, hccp_err("rs_query_rdev_cb phy_id[%u] rdev_index[%u], ret %d",
        phyId, rdevIndex, ret), ret);

    CHK_PRT_RETURN(qpInitAttr == NULL, hccp_err("qp_init_attr is NULL!"), -EINVAL);

    cqContext = qpInitAttr->qp_context;
    CHK_PRT_RETURN(cqContext == NULL, hccp_err("cq_context is NULL!"), -EINVAL);
    CHK_PRT_RETURN(rdevCb != cqContext->rdev_cb, hccp_err("rs_query_rdev_cb phy_id[%u] rdev_index[%u],"
        "rdevCb is invalid.", phyId, rdevIndex), -EINVAL);

    ret = RsBuildUpQpcb(cqContext, qpInitAttr, &qpCb);
    CHK_PRT_RETURN(ret, hccp_err("rs_build_up_qpcb failed, ret:%d", ret), ret);

    ret = RsDrvNormalQpCreate(qpCb, qpInitAttr);
    if (ret) {
        hccp_err("create drv qp create failed:%d", ret);
        goto create_qp_err;
    }

    RS_PTHREAD_MUTEX_LOCK(&rdevCb->rdev_mutex);
    RsListAddTail(&qpCb->list, &rdevCb->qp_list);
    RS_PTHREAD_MUTEX_ULOCK(&rdevCb->rdev_mutex);
    rdevCb->qp_cnt++;
    *qp = qpCb->ib_qp;
    qpResp->qpn = (unsigned int)qpCb->qp_info_lo.qpn;
    qpResp->gid_idx = (unsigned int)qpCb->qp_info_lo.gid_idx;
    qpResp->psn = (unsigned int)qpCb->qp_info_lo.psn;
    qpResp->gid = qpCb->qp_info_lo.gid;

    hccp_info("qp %d create qp.", qpResp->qpn);

    return 0;

create_qp_err:
    pthread_mutex_destroy(&qpCb->qp_mutex);
    free(qpCb);
    qpCb = NULL;
    return ret;
}

RS_ATTRI_VISI_DEF int RsNormalQpDestroy(unsigned int phyId, unsigned int rdevIndex, unsigned int qpn)
{
    int ret;
    struct rs_qp_cb *qpCb = NULL;
    struct rs_mr_cb *mrTmp = NULL;
    struct rs_mr_cb *mrTmp2 = NULL;

    RS_QP_PARA_CHECK(phyId);
    ret = RsQpn2qpcb(phyId, rdevIndex, qpn, &qpCb);
    CHK_PRT_RETURN(ret || qpCb == NULL, hccp_err("get qp cb failed qpn %u, ret %d", qpn, ret), ret);

    RS_PTHREAD_MUTEX_LOCK(&qpCb->rdev_cb->rdev_mutex);
    RsListDel(&qpCb->list);
    RS_PTHREAD_MUTEX_ULOCK(&qpCb->rdev_cb->rdev_mutex);
    RsIbvAckCqEvents(qpCb->ib_send_cq, qpCb->num_send_cq_events);
    RsIbvAckCqEvents(qpCb->ib_recv_cq, qpCb->num_recv_cq_events);

    RS_PTHREAD_MUTEX_LOCK(&qpCb->qp_mutex);
    RS_LIST_GET_HEAD_ENTRY(mrTmp, mrTmp2, &qpCb->mr_list, list, struct rs_mr_cb);
    for (; (&mrTmp->list) != &qpCb->mr_list;
        mrTmp = mrTmp2, mrTmp2 = list_entry(mrTmp2->list.next, struct rs_mr_cb, list)) {
        if (mrTmp->ib_mr != qpCb->rdev_cb->notify_mr) {
            (void)RsDrvMrDereg(mrTmp->ib_mr);
        }
        RsListDel(&mrTmp->list);
        free(mrTmp);
        mrTmp = NULL;
    }

    RS_LIST_GET_HEAD_ENTRY(mrTmp, mrTmp2, &qpCb->rem_mr_list, list, struct rs_mr_cb);
    for (; (&mrTmp->list) != &qpCb->rem_mr_list;
        mrTmp = mrTmp2, mrTmp2 = list_entry(mrTmp2->list.next, struct rs_mr_cb, list)) {
        RsListDel(&mrTmp->list);
        free(mrTmp);
        mrTmp = NULL;
    }
    RS_PTHREAD_MUTEX_ULOCK(&qpCb->qp_mutex);

    // destroy qp
    RsDrvQpDestroy(qpCb);

    qpCb->rdev_cb->qp_cnt--;

    pthread_mutex_destroy(&qpCb->qp_mutex);
    hccp_info("qp %d destroy qp, send wr[%u].", qpn, qpCb->send_wr_num);

    free(qpCb);
    qpCb = NULL;
    return ret;
}

RS_ATTRI_VISI_DEF int RsCreateCompChannel(unsigned int phyId, unsigned int rdevIndex, void** compChannel)
{
    int ret;
    unsigned int chipId;

    struct rs_rdev_cb *rdevCb = NULL;

    CHK_PRT_RETURN(compChannel == NULL || phyId >= RS_MAX_DEV_NUM,
        hccp_err("param err, NULL pointer or phyId:%u >= [%d]", phyId, RS_MAX_DEV_NUM), -EINVAL);

    ret = rsGetLocalDevIDByHostDevID(phyId, &chipId);
    CHK_PRT_RETURN(ret,
        hccp_err("rs_create_comp_channel rsGetLocalDevIDByHostDevID phy_id[%u] invalid, ret %d", phyId, ret), ret);

    ret = RsRdev2rdevCb(chipId, rdevIndex, &rdevCb);
    CHK_PRT_RETURN(ret || rdevCb == NULL, hccp_err("rs_rdev2rdev_cb for chip_id[%u] failed, ret %d",
        chipId, ret), ret);

    *compChannel = (void *)RsIbvCreateCompChannel(rdevCb->ib_ctx);
    if (*compChannel == NULL) {
        hccp_err("rs_ibv_create_comp_channel failed, errno(%d)", errno);
        return -EOPENSRC;
    }
    hccp_info("create comp channel success!");
    return 0;
}

RS_ATTRI_VISI_DEF int RsDestroyCompChannel(void* compChannel)
{
    int ret;
    struct ibv_comp_channel *rsCompChannel = (struct ibv_comp_channel *)compChannel;

    ret = RsIbvDestroyCompChannel(rsCompChannel);
    CHK_PRT_RETURN(ret, hccp_err("rs_destroy_comp_channel failed."), ret);
    hccp_info("destroy comp channel success!");

    return 0;
}

RS_ATTRI_VISI_DEF int RsCreateSrq(unsigned int phyId, unsigned int rdevIndex, struct srq_attr *attr)
{
    int ret;
    struct rs_rdev_cb *rdevCb = NULL;
    struct rs_cq_context *cqContext = NULL;

    CHK_PRT_RETURN(attr == NULL || phyId >= RS_MAX_DEV_NUM,
        hccp_err("param err, NULL pointer or phyId:%u >= [%d]", phyId, RS_MAX_DEV_NUM), -EINVAL);

    ret = RsQueryRdevCb(phyId, rdevIndex, &rdevCb);
    CHK_PRT_RETURN(ret, hccp_err("rs_query_rdev_cb phy_id[%u] rdev_index[%u], ret %d", phyId, rdevIndex, ret), ret);

    cqContext = calloc(1, sizeof(struct rs_cq_context));
    if (cqContext == NULL) {
        return -ENOMEM;
    }

    cqContext->rdev_cb = rdevCb;
    cqContext->eq_num = 0;
    cqContext->cq_create_mode = RS_SRQ_CQ_CREATE;
    *attr->context = cqContext;

    struct cq_attr cqAttr = {0};
    cqAttr.recv_cq_depth = attr->cq_depth;
    cqAttr.recv_cq_event_id = attr->srq_event_id;
    cqAttr.ib_recv_cq = attr->ib_recv_cq;
    // 创建srq cq
    ret = RsCreateCqEvent(cqContext, &cqAttr);
    if (ret) {
        hccp_err("rs_create_cq_event create cq failed! ret:%d", ret);
        goto create_cq_event_err;
    }
    cqContext->ib_srq_cq = *attr->ib_recv_cq;

    struct ibv_srq_init_attr srqInitAttr = {
        .attr = {
            .max_wr  = attr->srq_depth,
            .max_sge = attr->max_sge
        }
    };
    hccp_info("max_wr [%u], max_sge[%u]", srqInitAttr.attr.max_wr, srqInitAttr.attr.max_sge);

    // 创建srq
    *attr->ib_srq = RsIbvCreateSrq(rdevCb->ib_pd, &srqInitAttr);
    if (*attr->ib_srq == NULL) {
        hccp_err("rs_ibv_create_srq failed.");
        ret = -EOPENSRC;
        goto create_srq_err;
    }
    hccp_info("create srq success!");

    return 0;
create_cq_event_err:
create_srq_err:
    cqAttr.qp_context = attr->context;
    RsCqDestroy(phyId, rdevIndex, &cqAttr);

    return ret;
}

RS_ATTRI_VISI_DEF int RsDestroySrq(unsigned int phyId, unsigned int rdevIndex, struct srq_attr *attr)
{
    int ret;

    CHK_PRT_RETURN(*attr->context == NULL || *attr->ib_srq == NULL|| phyId >= RS_MAX_DEV_NUM,
        hccp_err("param err, NULL pointer or phyId:%u >= [%d]", phyId, RS_MAX_DEV_NUM), -EINVAL);

    struct cq_attr cqAttr = {0};
    struct rs_cq_context *cqContext = *attr->context;
    cqAttr.qp_context = attr->context;

    RsIbvAckCqEvents(cqContext->ib_srq_cq, cqContext->num_recv_cq_events);

    // 销毁srq cq
    ret = RsCqDestroy(phyId, rdevIndex, &cqAttr);
    CHK_PRT_RETURN(ret, hccp_err("rs_cq_destroy destroy cq failed! ret:%d", ret), ret);

    ret = RsIbvDestroySrq(*attr->ib_srq);
    CHK_PRT_RETURN(ret, hccp_err("rs_ibv_destroy_srq failed."), ret);

    return 0;
}

RS_ATTRI_VISI_DEF int RsGetLiteSupport(unsigned int phyId, unsigned int rdevIndex, int *supportLite)
{
    int ret;
    unsigned int chipId;
    struct rs_rdev_cb *rdevCb = NULL;

    RS_CHECK_POINTER_NULL_RETURN_INT(supportLite);

    CHK_PRT_RETURN(phyId >= RS_MAX_DEV_NUM, hccp_err("rs set param error ! phy_id:%u", phyId), -EINVAL);
    ret = rsGetLocalDevIDByHostDevID(phyId, &chipId);
    CHK_PRT_RETURN(ret, hccp_err("phy_id[%u] invalid, ret %d", phyId, ret), ret);

    ret = RsRdev2rdevCb(chipId, rdevIndex, &rdevCb);
    CHK_PRT_RETURN(ret || rdevCb == NULL, hccp_err("rs_rdev2rdev_cb for chip_id[%u] failed, ret %d",
        chipId, ret), ret);

    rdevCb->support_lite = 1;
    *supportLite = rdevCb->support_lite;

    return 0;
}

RS_ATTRI_VISI_DEF int RsGetLiteRdevCap(
    unsigned int phyId, unsigned int rdevIndex, struct lite_rdev_cap_resp *resp)
{
    int ret;
    unsigned int chipId;
    struct rs_rdev_cb *rdevCb = NULL;

    RS_CHECK_POINTER_NULL_RETURN_INT(resp);

    CHK_PRT_RETURN(phyId >= RS_MAX_DEV_NUM, hccp_err("rs set param error ! phy_id:%u", phyId), -EINVAL);
    ret = rsGetLocalDevIDByHostDevID(phyId, &chipId);
    CHK_PRT_RETURN(ret, hccp_err("phy_id[%u] invalid, ret %d", phyId, ret), ret);

    ret = RsRdev2rdevCb(chipId, rdevIndex, &rdevCb);
    CHK_PRT_RETURN(ret || rdevCb == NULL, hccp_err("rs_rdev2rdev_cb for chip_id[%u] failed, ret %d",
        chipId, ret), ret);

    ret = RsIbvExpQueryDevice(rdevCb->ib_ctx, &resp->cap);
    CHK_PRT_RETURN(ret, hccp_err("rs_ibv_exp_query_device for phy_id[%u] failed, ret %d", phyId, ret), ret);

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

RS_ATTRI_VISI_DEF int RsGetLiteQpCqAttr(
    unsigned int phyId, unsigned int rdevIndex, unsigned int qpn, struct lite_qp_cq_attr_resp *resp)
{
    int ret;
    struct rs_qp_cb *qpCb = NULL;

    RS_CHECK_POINTER_NULL_RETURN_INT(resp);

    RS_QP_PARA_CHECK(phyId);
    ret = RsQpn2qpcb(phyId, rdevIndex, qpn, &qpCb);
    CHK_PRT_RETURN(ret || qpCb == NULL, hccp_err("get qp cb failed qpn %u, ret %d", qpn, ret), ret);

    ret = memcpy_s(resp, sizeof(struct lite_qp_cq_attr_resp), (void *)&qpCb->qp_resp, sizeof(qpCb->qp_resp));
    if (ret) {
        hccp_err("memcpy_s failed, ret:%d, src_len:%u, dst_len:%u",
            ret,
            (unsigned int)sizeof(qpCb->qp_resp),
            (unsigned int)sizeof(struct lite_qp_cq_attr_resp));
        return ret;
    }

    return 0;
}

RS_ATTRI_VISI_DEF int RsGetLiteMemAttr(
    unsigned int phyId, unsigned int rdevIndex, unsigned int qpn, struct lite_mem_attr_resp *resp)
{
    int ret;
    struct rs_qp_cb *qpCb = NULL;

    RS_CHECK_POINTER_NULL_RETURN_INT(resp);

    RS_QP_PARA_CHECK(phyId);
    ret = RsQpn2qpcb(phyId, rdevIndex, qpn, &qpCb);
    CHK_PRT_RETURN(ret != 0 || qpCb == NULL, hccp_err("get qp cb failed qpn %u, ret %d", qpn, ret), ret);

    ret = memcpy_s(resp, sizeof(struct lite_mem_attr_resp), (void *)&qpCb->mem_resp, sizeof(qpCb->mem_resp));
    if (ret) {
        hccp_err("memcpy_s failed, ret:%d, src_len:%u, dst_len:%u",
            ret,
            (unsigned int)sizeof(qpCb->mem_resp),
            (unsigned int)sizeof(struct lite_mem_attr_resp));
        return ret;
    }

    return 0;
}

STATIC void RsGetMrInfo(
    struct rs_qp_cb *qpCb, struct lite_mr_info *mr, uint32_t maxMrNum, struct rs_list_head *mrList)
{
    struct rs_mr_cb *mrTmp = NULL;
    struct rs_mr_cb *mrTmp2 = NULL;
    uint32_t i = 0;

    RS_PTHREAD_MUTEX_LOCK(&qpCb->qp_mutex);
    RS_LIST_GET_HEAD_ENTRY(mrTmp, mrTmp2, mrList, list, struct rs_mr_cb);
    for (; (&mrTmp->list) != mrList;
        mrTmp = mrTmp2, mrTmp2 = list_entry(mrTmp2->list.next, struct rs_mr_cb, list)) {
        if (i < maxMrNum) {
            mr[i].key = mrTmp->mr_info.rkey;
            mr[i].addr = mrTmp->mr_info.addr;
            mr[i].len = mrTmp->mr_info.len;
            i++;
        } else {
            break;
        }
    }

    RS_PTHREAD_MUTEX_ULOCK(&qpCb->qp_mutex);
}

RS_ATTRI_VISI_DEF int RsGetLiteConnectedInfo(
    unsigned int phyId, unsigned int rdevIndex, unsigned int qpn, struct lite_connected_info_resp *resp)
{
    int ret;
    struct rs_qp_cb *qpCb = NULL;

    RS_CHECK_POINTER_NULL_RETURN_INT(resp);
    RS_QP_PARA_CHECK(phyId);
    ret = RsQpn2qpcb(phyId, rdevIndex, qpn, &qpCb);
    CHK_PRT_RETURN(ret || qpCb == NULL, hccp_err("get qp cb failed qpn %u, ret %d", qpn, ret), ret);

    resp->state = (unsigned int)qpCb->state;
    if (resp->state == RS_QP_STATUS_CONNECTED) {
        RsGetMrInfo(qpCb, &resp->local_mr[0], RA_MR_MAX_NUM, &qpCb->mr_list);
        RsGetMrInfo(qpCb, &resp->rem_mr[0], RA_MR_MAX_NUM, &qpCb->rem_mr_list);
        resp->qos_attr.sl = qpCb->qos_attr.sl;
        resp->qos_attr.tc = qpCb->qos_attr.tc;
    }

    return 0;
}

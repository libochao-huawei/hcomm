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
#include <sys/types.h>
#include <sys/eventfd.h>
#include <sys/epoll.h>
#include <sys/syscall.h>
#include <sys/prctl.h>

#include "rs_tls.h"
#include "ssl_adp.h"
#include "securec.h"
#include "rs.h"
#include "rs_inner.h"
#include "ra_rs_comm.h"
#include "ra_rs_err.h"
#include "rs_drv_rdma.h"
#include "dl_hal_function.h"
#include "rs_drv_rdma.h"
#include "rs_socket.h"
#ifdef CONFIG_TLV
#include "rs_adp_nslb.h"
#endif
#include "rs_epoll.h"

#define BIND_MIN_DCPU_NUM 1
#define COMMAND_LENGTH 100
#define ADD_TID_SHELL_PATH 50
#define TID_LENGTH 20
#define HOST ((uint32_t)1)

struct rs_pthread_info gEpollThreadInfo = {0};  //lint !e17
struct rs_pthread_info gConnectThreadInfo = {0};

/*
 * opcode:
 *  ADD: EPOLL_CTL_ADD
 *  MOD: EPOLL_CTL_MOD
 *  DEL: EPOLL_CTL_DEL
 */
int RsEpollCtl(int epollfd, int op, int fd, unsigned int state)
{
    int ret;
    struct epoll_event ev;

    ev.events = state;
    ev.data.fd = fd;
    ret = epoll_ctl(epollfd, op, fd, &ev);
    if (ret) {
        hccp_warn("epoll_ctl for fd %d unsuccessful! ret:%d errno:%d op:%d state:%u",
            fd, ret, errno, op, state);
    }
    return ret;
}

int RsEpollCtlFdHandle(int epollfd, int op, int fd, unsigned int state, void *fdHandle)
{
    int ret;
    struct epoll_event ev;

    ev.events = state;
    ev.data.ptr = fdHandle;
    ret = epoll_ctl(epollfd, op, fd, &ev);
    if (ret) {
        hccp_warn("epoll_ctl for fd %d unsuccessful! ret:%d errno:%d op:%d state:%u",
            fd, ret, errno, op, state);
    }
    return ret;
}

int RsWlistCheckConnAdd(struct rs_cb *rsCb, struct rs_conn_info* connTmp)
{
    int ret;
    int retClose;
    struct rs_conn_info *conn = NULL;

    if (rsCb->conn_cb.wlist_enable == 1) {
        ret = RsWhiteListCheckValid(rsCb->chip_id, &rsCb->conn_cb, connTmp);
        if (ret) {
            hccp_info("invalid client node found: chip_id %u, fd %d accept, server ip:%s, client ip:%s, port:%u, "
                "state:%d, tag:%s", rsCb->chip_id, connTmp->connfd, connTmp->server_ip.read_addr,
                connTmp->client_ip.read_addr, connTmp->port, connTmp->state, connTmp->tag);
            return ret;
        }
    }

    /* add conn node to server list */
    ret = RsAllocConnNode(&conn, connTmp->port);
    if (ret) {
        hccp_err("server IP:0x%s add conn info to list failed, fd:%d, ret:%d",
            connTmp->server_ip.read_addr, connTmp->connfd, ret);
        goto alloc_err;
    }

    ret = RsSocketCopyConnInfo(connTmp, conn);
    if (ret) {
        hccp_err("rs_socket_copy_conn_info failed, ret[%d]", ret);
        goto out;
    }

    if (rsCb->ssl_enable == RS_SSL_ENABLE) {
        ret = RsEpollCtl(rsCb->conn_cb.epollfd, EPOLL_CTL_DEL, connTmp->connfd, EPOLLIN);
        if (ret) {
            hccp_err("rs epoll ctl failed, ret %d", ret);
            goto out;
        }
    }

    RS_PTHREAD_MUTEX_LOCK(&rsCb->conn_cb.conn_mutex);
    RsListAddTail(&conn->list, &rsCb->conn_cb.server_conn_list);
    RS_PTHREAD_MUTEX_ULOCK(&rsCb->conn_cb.conn_mutex);

    hccp_info("[Server]chip_id %u, fd %d accept, server ip:%s, client ip:%s, state:%d, tag:%s",
        rsCb->chip_id, connTmp->connfd, connTmp->server_ip.read_addr, connTmp->client_ip.read_addr,
        connTmp->state, connTmp->tag);
    ShowConnNode(&(rsCb->conn_cb.server_conn_list));
    return 0;

out:
    free(conn);
    conn = NULL;
alloc_err:
    RS_CLOSE_RETRY_FOR_EINTR(retClose, connTmp->connfd);
    return ret;
}

STATIC int RsSslRecvTagInHandle(struct rs_accept_info *acceptInfo, struct rs_conn_info *connTmp)
{
    int expSize = SOCK_CONN_TAG_SIZE + SOCK_CONN_DEV_ID_SIZE;
    char *recvBuff = connTmp->tag;
    struct timeval startTime, now;
    float timeCost = 0.0;
    int size = expSize;

    RsGetCurTime(&startTime);
    while (expSize > 0 && size != 0) {
        connTmp->tag_sync_time++;
        size = ssl_adp_read(acceptInfo->ssl, recvBuff, expSize);
        if ((size < 0) && (errno == EINTR)) {
            connTmp->tag_eintr_time++;
            continue;
        }
        expSize -= size;
        recvBuff += size;
        RsGetCurTime(&now);
        HccpTimeInterval(&now, &startTime, &timeCost);
        if (timeCost >= RS_RECV_MAX_TIME) {
            hccp_run_info("recv tag time out, server:{%s:%u} client:%s tag_sync_time:%u tag_eintr_time:%u",
                acceptInfo->server_ip_addr.read_addr, acceptInfo->sock_port, acceptInfo->client_ip_addr.read_addr,
                connTmp->tag_sync_time, connTmp->tag_eintr_time);
            return -ETIME;
        }

        if (timeCost <= 0) {
            RsGetCurTime(&startTime);
        }
    }

    connTmp->server_ip = acceptInfo->server_ip_addr;
    connTmp->client_ip = acceptInfo->client_ip_addr;
    connTmp->connfd = acceptInfo->conn_fd;
    connTmp->state = RS_CONN_STATE_TAG_SYNC;
    connTmp->port = acceptInfo->sock_port;
    connTmp->ssl = acceptInfo->ssl;

    hccp_info("recv tag success, server:{%s:%u} client:%s timeCost:%fms tag_sync_time:%u tag_eintr_time:%u",
        acceptInfo->server_ip_addr.read_addr, acceptInfo->sock_port, acceptInfo->client_ip_addr.read_addr, timeCost,
        connTmp->tag_sync_time, connTmp->tag_eintr_time);
    return 0;
}

STATIC void RsEpollEventSslRecvTagInHandle(struct rs_cb *rsCb, struct rs_accept_info *acceptInfo)
{
    struct rs_conn_info connTmp = {0};
    int retClose;
    int ret;

    ret = RsSslRecvTagInHandle(acceptInfo, &connTmp);
    if (ret != 0) {
        RS_CLOSE_RETRY_FOR_EINTR(retClose, acceptInfo->conn_fd);
        goto out;
    }

    ret = RsWlistCheckConnAdd(rsCb, &connTmp);
out:
    if (ret != 0) {
        hccp_warn("recv tag or add conn unsuccessful ret:%d", ret);
        ssl_adp_shutdown(acceptInfo->ssl);
        ssl_adp_free(acceptInfo->ssl);
        acceptInfo->ssl = NULL;
    }

    /* do not shutdown ssl */
    RS_PTHREAD_MUTEX_LOCK(&rsCb->conn_cb.conn_mutex);
    RsListDel(&acceptInfo->list);
    free(acceptInfo);
    acceptInfo = NULL;
    RS_PTHREAD_MUTEX_ULOCK(&rsCb->conn_cb.conn_mutex);

    return;
}

STATIC void RsDoSslHandshake(struct rs_accept_info *acceptInfo, struct rs_cb *rscb)
{
    int ret;
    int err;

    ret = ssl_adp_do_handshake(acceptInfo->ssl);
    if (ret == 1) {
#ifdef CONFIG_SSL
        ret = rs_tls_peer_cert_verify(acceptInfo->ssl, rscb);
        if (ret) {
            hccp_err("tls verify peer cert failed");
            return ;
        }
#endif
        acceptInfo->state = RS_CONN_STATE_SSL_CONNECTED;
    } else {
        err = ssl_adp_get_error(acceptInfo->ssl, ret);
        if (err == SSL_ERROR_WANT_WRITE) {
            hccp_info("return want write");
            return ;
        } else if (err == SSL_ERROR_WANT_READ) {
            hccp_info("return want read");
            return ;
        } else {
#ifdef CONFIG_SSL
            rs_ssl_err_string(acceptInfo->conn_fd, err);
#endif
            return ;
        }
    }

    return ;
}

STATIC int RsEpollEventSslAcceptInHandle(struct rs_cb *rsCb, int fd)
{
    int ret;

    struct rs_accept_info *acceptInfo = NULL;
    struct rs_accept_info *acceptInfo2 = NULL;

    /* Server event: ssl accept */
    RS_LIST_GET_HEAD_ENTRY(acceptInfo, acceptInfo2, &rsCb->conn_cb.server_accept_list, list, struct rs_accept_info);
    for (; (&acceptInfo->list) != &rsCb->conn_cb.server_accept_list;
        acceptInfo = acceptInfo2, acceptInfo2 = list_entry(acceptInfo2->list.next, struct rs_accept_info, list)) {
        /* connection request for Server */
        if (fd == acceptInfo->conn_fd) {
            if (acceptInfo->ssl == NULL) {
                acceptInfo->ssl = ssl_adp_new(rsCb->server_ssl_ctx);
                CHK_PRT_RETURN(acceptInfo->ssl == NULL, hccp_err("server ssl ctx alloc failed"), -ENOMEM);

                ret = ssl_adp_set_fd(acceptInfo->ssl, acceptInfo->conn_fd);
                if (ret != 1) {
                    hccp_err("bind connfd and ssl failed, ret %d", ret);
                    ssl_adp_shutdown(acceptInfo->ssl);
                    ssl_adp_free(acceptInfo->ssl);
                    acceptInfo->ssl = NULL;
                    return -EINVAL;
                }

                ssl_adp_set_mode(acceptInfo->ssl, SSL_MODE_AUTO_RETRY);
                ssl_adp_set_accept_state(acceptInfo->ssl);
            }

            if (acceptInfo->state == RS_CONN_STATE_RESET) {
                RsDoSslHandshake(acceptInfo, rsCb);
                return 0;
            }

            if (acceptInfo->state == RS_CONN_STATE_SSL_CONNECTED) {
                RsEpollEventSslRecvTagInHandle(rsCb, acceptInfo);
                return 0;
            }
        }
    }

    return -ENODEV;
}

STATIC int RsEpollTcpRecv(struct rs_cb *rsCb, int fd)
{
    if (rsCb->tcp_recv_callback != NULL) {
        rsCb->tcp_recv_callback(rsCb->fd_map[fd]);
    } else {
        hccp_err("[rs_epoll_tcp_recv]tcp_recv_callback is null.");
        return -EINVAL;
    }
    return 0;
}

STATIC int RsEpollEventHeterogTcpRecvInHandle(struct rs_cb *rsCb, int fd)
{
    int ret;
    struct rs_heterog_tcp_fd_info *fdNode = NULL;
    struct rs_heterog_tcp_fd_info *fdNode1 = NULL;

    RS_LIST_GET_HEAD_ENTRY(fdNode, fdNode1, &rsCb->heterog_tcp_fd_list, list, struct rs_heterog_tcp_fd_info);
    for (; (&fdNode->list) != &rsCb->heterog_tcp_fd_list;
        fdNode = fdNode1, fdNode1 = list_entry(fdNode1->list.next, struct rs_heterog_tcp_fd_info, list)) {
        if (fdNode->fd == fd) {
            // 处理tcp recv
            ret = RsEpollTcpRecv(rsCb, fd);
            return ret;
        }
    }
    return -ENODEV;
}

STATIC void RsEpollEventInHandle(struct rs_cb *rsCb, struct epoll_event *events)
{
    int fd = events->data.fd;
    int ret;

    ret = RsEpollEventPingHandle(rsCb, fd);
    if (ret != -ENODEV) {
        hccp_info("the fd:%d is for ping, no need to go on, ret:%d", fd, ret);
        return;
    }

    ret = RsEpollEventListenInHandle(rsCb, fd);
    if (ret != -ENODEV) {
        hccp_info("the fd:%d is tcp listened, no need to go on, ret:%d", fd, ret);
        return;
    }

    if (rsCb->ssl_enable == RS_SSL_ENABLE) {
        ret = RsEpollEventSslAcceptInHandle(rsCb, fd);
        if (ret != -ENODEV) {
            hccp_info("the fd:%d is ssl accept, no need to go on, ret:%d", fd, ret);
            return;
        }
    }

    ret = RsEpollEventQpMrInHandle(rsCb, fd);
    if (ret != -ENODEV) {
        hccp_info("the fd:%d is for qp mr, no need to go on, ret:%d", fd, ret);
        return;
    }

    ret = RsEpollEventHeterogTcpRecvInHandle(rsCb, fd);
    if (ret != -ENODEV) {
        hccp_info("the fd:%d is for tcp recv, no need to go on, ret:%d", fd, ret);
        return;
    }

    return;
}

STATIC void RsEpollEventHandleOne(struct rs_cb *rsCb, struct epoll_event *events)
{
    RS_CHECK_POINTER_NULL_RETURN_VOID(events);
    RS_CHECK_POINTER_NULL_RETURN_VOID(rsCb);

#ifdef CONFIG_TLV
    int ret;
    ret = RsEpollNslbEventHandle(&rsCb->nslb_cb, events->data.fd, events->events);
    if (ret != -ENODEV) {
        hccp_info("the fd:%d is nslb event, no need to go on, ret:%d", events->data.fd, ret);
        return;
    }
#endif

    if (events->events & EPOLLIN) {
        if (events->events & EPOLLRDHUP) {
            hccp_dbg("Peer socket has been closed!");
            return;
        }
        RsEpollEventInHandle(rsCb, events);
    } else {
        hccp_warn("unknown event(0x%x)!", events->events);
    }

    return;
}

STATIC int SetAffinity(unsigned int chipId, unsigned int cpuId)
{
    cpu_set_t mask;
    cpu_set_t get;
    int ret;

    CPU_ZERO(&mask);

    // 设置线程CPU亲和力
    CPU_SET(cpuId, &mask);
    ret = pthread_setaffinity_np(pthread_self(), sizeof(mask), &mask);
    CHK_PRT_RETURN(ret < 0, hccp_err("could not set CPU affinity"), ret);

    // 获取线程cpu亲和力
    CPU_ZERO(&get);
    ret = pthread_getaffinity_np(pthread_self(), sizeof(get), &get);
    CHK_PRT_RETURN(ret, hccp_err("could not get CPU affinity"), ret);

    if (CPU_ISSET(cpuId, &get)) { // 检查cpuid是否在这个集合中
        hccp_info("dev is %d thread %llu is running in processor %d.", chipId, pthread_self(), cpuId);
    } else {
        hccp_warn("dev is %d thread %llu is not running in processor %d.", chipId, pthread_self(), cpuId);
    }

    return ret;
}

STATIC int BindDataCpu(unsigned int chipId)
{
    int ret;
    unsigned int cpuId;
    uint32_t info = 0;
    uint32_t devNum = 0;
    int64_t ccpuNum;
    int64_t dcpuNum;
    int64_t acpuNum;
    int64_t cpuNum;

    // 判断当前处于host or device侧，如果在host侧，无需进行绑核操作
    ret = DlDrvGetPlatformInfo(&info);
    CHK_PRT_RETURN(ret, hccp_err("get PlatformInfo failed, ret[%d]", ret), ret);
    if (info == HOST) {
        hccp_info("host not need bind cpu, info[%u]", info);
        return 0;
    }

    // 获取当前os的device数量
    ret = DlDrvGetDevNum(&devNum);
    CHK_PRT_RETURN(ret, hccp_err("get device_num failed, ret[%d]", ret), ret);
    if (devNum == 0) {
        hccp_info("no device need bind cpu, device num %u", devNum);
        return 0;
    }

    // 获取data cpu数量
    ret = DlHalGetDeviceInfo(chipId, MODULE_TYPE_DCPU, INFO_TYPE_CORE_NUM, &dcpuNum);
    CHK_PRT_RETURN(ret, hccp_err("get dcpu_num failed, ret(%d)", ret), ret);

    // 如果dcpu_num < 1，无需绑核直接返回
    if (dcpuNum < BIND_MIN_DCPU_NUM) {
        hccp_info("data cpu num %d, device not need bind cpu", dcpuNum);
        return 0;
    }

    // 获取ctrl cpu数量
    ret = DlHalGetDeviceInfo(chipId, MODULE_TYPE_CCPU, INFO_TYPE_CORE_NUM, &ccpuNum);
    CHK_PRT_RETURN(ret, hccp_err("get ccpu_num failed, ret(%d)", ret), ret);

    // 获取ai cpu数量
    ret = DlHalGetDeviceInfo(chipId, MODULE_TYPE_AICPU, INFO_TYPE_CORE_NUM, &acpuNum);
    CHK_PRT_RETURN(ret, hccp_err("get acpu_num failed, ret(%d)", ret), ret);

    // 计算单个device上的核数
    cpuNum = ccpuNum + dcpuNum + acpuNum;
    hccp_info("halGetDeviceInf chip = %u dev_num = %u, ccpu = %lld dcpu = %lld acpu = %lld cpuNum = %lld",
        chipId, devNum, ccpuNum, dcpuNum, acpuNum, cpuNum);

    cpuId = (unsigned int)((int64_t)(chipId % devNum) * cpuNum + ccpuNum);

    // 进行绑核
    ret = DlHalBindCgroup(BIND_DATACPU_CGROUP);
    CHK_PRT_RETURN(ret, hccp_err("bind cgroup failed, ret[%d], strerror[%s]",
        ret, strerror(errno)), ret);
    hccp_info("bind cgroup success!");

    ret = SetAffinity(chipId, cpuId);
    CHK_PRT_RETURN(ret, hccp_err("set affinity with cpu[%u] failed", cpuId), ret);

    hccp_info("chip_id[%u] bind data cpu[%u] success", chipId, cpuId);

    return ret;
}

STATIC void RsSocketSaveEpollWaitErrInfo(int eventNum, int errNo, struct socket_err_info *epollErrInfo)
{
    if (eventNum > 0) {
        return;
    }

    RsSocketSaveErrInfo(RS_CONN_STATE_RESET, errNo, epollErrInfo);
}

STATIC void *RsEpollHandle(void *arg)
{
    struct rs_conn_cb *connCb = NULL;
    struct rs_cb *rsCb = NULL;
    eventfd_t val;
    int ret;
    int num;
    int i;

    RS_CHECK_POINTER_NULL_RETURN_NULL(arg);

    hccp_info("<EPOLL> thread begin! thread_id:%lu, pid:%d, ppid:%d",
        pthread_self(), getpid(), getppid());

    struct epoll_event events[RS_EPOLL_EVENT];

    CHK_PRT_RETURN(pthread_detach(pthread_self()), hccp_err("pthread_detach failed! thread_id:%lu, errno:%d",
        pthread_self(), errno), NULL);

    (void)prctl(PR_SET_NAME, (unsigned long)"hccp_epoll");

    rsCb = (struct rs_cb *)arg;
    gRsCb = rsCb;
    connCb = &rsCb->conn_cb;

    ret = BindDataCpu(rsCb->chip_id);
    if (ret) {
        hccp_warn("bind data cpu unsuccessful! thread_id:%lu, errno:%d", pthread_self(), errno);
    }

    rsCb->state &= ~RS_STATE_HALT;
    RsGetCurTime(&gEpollThreadInfo.last_check_time);
    ret = strncpy_s((char *)gEpollThreadInfo.pthread_name, sizeof(gEpollThreadInfo.pthread_name),
        "epoll_pthread", strlen("epoll_pthread"));
    CHK_PRT_RETURN(ret, hccp_err("strncpy_s pthread name failed, ret[%d]", ret), NULL);

    hccp_run_info("pthread[%s] is alive!", gEpollThreadInfo.pthread_name);

    ret = RsDrvInitCqeErrInfo();
    CHK_PRT_RETURN(ret, hccp_err("rs_drv_init_cqe_err_info failed, ret[%d]", ret), NULL);
    while (1) {
        do {
            num = epoll_wait(connCb->epollfd, events, RS_EPOLL_EVENT, -1);
        } while ((num < 0) && (errno == EINTR));
        RsSocketSaveEpollWaitErrInfo(num, -errno, &connCb->epoll_err_info);

        /* eventfd is for wake up epoll wait, value is ignored */
        if (events[0].data.fd == connCb->eventfd) {
            hccp_warn("<EPOLL> SHUT DOWN event!!! eventfd:%d", connCb->eventfd);
            do {
                num = read(connCb->eventfd, &val, sizeof(eventfd_t));
            } while ((num < 0) && (errno == EINTR));
            RS_PTHREAD_MUTEX_LOCK(&rsCb->mutex);
            rsCb->state |= RS_STATE_HALT;
            RS_PTHREAD_MUTEX_ULOCK(&rsCb->mutex);

            break;
        }

        RsHeartbeatAlivePrint(&gEpollThreadInfo);
        RS_PTHREAD_MUTEX_LOCK(&rsCb->mutex);
        /* events handle one by one */
        for (i = 0; i < num; i++) {
            RsEpollEventHandleOne(rsCb, &events[i]);
        }

        RS_PTHREAD_MUTEX_ULOCK(&rsCb->mutex);
    }
    RsDrvDeinitCqeErrInfo();
    hccp_info("<EPOLL> QUIT thread_id:%lu, pid:%d, read num:%d", pthread_self(), getpid(), num);

    return NULL;
}

STATIC int RsCreateEpoll(struct rs_cb *rsCb)
{
    int ret, retFd;
    struct rs_conn_cb *connCb = NULL;

    connCb = &rsCb->conn_cb;

    connCb->epollfd = epoll_create(1);
    CHK_PRT_RETURN(connCb->epollfd < 0, hccp_err("epollfd[%d] failed, errno[%d]", connCb->epollfd, errno), -EINVAL);

    connCb->eventfd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    if (connCb->eventfd == RS_FD_INVALID) {
        RS_CLOSE_RETRY_FOR_EINTR(retFd, connCb->epollfd);
        connCb->epollfd = RS_FD_INVALID;
        hccp_err("create eventfd for rs cb failed !");
        return -EINVAL;
    }

    ret = RsEpollCtl(connCb->epollfd, EPOLL_CTL_ADD, connCb->eventfd, EPOLLIN);
    if (ret) {
        hccp_err("re epoll ctl failed, epollfd[%d], eventfd[%d]", connCb->epollfd, connCb->eventfd);
        RS_CLOSE_RETRY_FOR_EINTR(retFd, connCb->eventfd);
        connCb->eventfd = RS_FD_INVALID;
        RS_CLOSE_RETRY_FOR_EINTR(retFd, connCb->epollfd);
        connCb->epollfd = RS_FD_INVALID;
        return ret;
    }

    return 0;
}

void RsDestroyEpoll(struct rs_cb *rsCb)
{
    int ret;
    struct rs_conn_cb *connCb = NULL;

    connCb = &rsCb->conn_cb;

    ret = RsEpollCtl(connCb->epollfd, EPOLL_CTL_DEL, connCb->eventfd, EPOLLIN);
    if (ret) {
        hccp_warn("re epoll ctl unsuccessful, epollfd[%d], eventfd[%d]", connCb->epollfd, connCb->eventfd);
    }

    RS_CLOSE_RETRY_FOR_EINTR(ret, connCb->eventfd);
    connCb->eventfd = RS_FD_INVALID;

    RS_CLOSE_RETRY_FOR_EINTR(ret, connCb->epollfd);
    connCb->epollfd = RS_FD_INVALID;

    return;
}

STATIC void RsUsleepWaitConn(sem_t* sem, uint32_t timeoutUs)
{
    uint32_t sleepUs = 1000; // 每 1000us 查看sem是否为零
    uint32_t elapsedUs = 0;
    do {
        if (sem_trywait(sem) == 0) {
            return;
        }
        usleep(sleepUs);
        elapsedUs += sleepUs;
    } while (elapsedUs < timeoutUs);
    return;
}

STATIC void *RsConnectHandle(void *arg)
{
    struct rs_conn_info *connTmp2 = NULL;
    struct rs_conn_info *connTmp = NULL;
    bool promoteConnect = false;
    int ret;

    hccp_info("<SOCKET> thread begin! thread_id:%lu, pid:%d, ppid:%d",
        pthread_self(), getpid(), getppid());
    CHK_PRT_RETURN(pthread_detach(pthread_self()), hccp_err("pthread_detach failed! thread_id:%lu, errno:%d",
        pthread_self(), errno), NULL);

    (void)prctl(PR_SET_NAME, (unsigned long)"hccp_connect");

    RsGetCurTime(&gConnectThreadInfo.last_check_time);
    ret = strncpy_s((char *)gConnectThreadInfo.pthread_name, sizeof(gConnectThreadInfo.pthread_name),
        "connect_pthread", strlen("connect_pthread"));
    CHK_PRT_RETURN(ret, hccp_err("strncpy_s pthread name failed, ret[%d]", ret), NULL);
    struct rs_cb *rsCb = (struct rs_cb *)arg;
    if (rsCb == NULL) {
        return NULL;
    }

    gRsCb = rsCb;
    sem_init(&rsCb->connect_trig_sem, 0, 0);

    hccp_run_info("pthread[%s] is alive!", gConnectThreadInfo.pthread_name);
    while (1) {
        if (rsCb == NULL) {
            return NULL;
        }

        if (rsCb->conn_flag == 0) {
            break;
        }

        promoteConnect = false;
        RsHeartbeatAlivePrint(&gConnectThreadInfo);
        RS_PTHREAD_MUTEX_LOCK(&rsCb->mutex);
        RS_LIST_GET_HEAD_ENTRY(connTmp, connTmp2, &rsCb->conn_cb.client_conn_list, list, struct rs_conn_info);
        for (; (&connTmp->list) != &rsCb->conn_cb.client_conn_list;
            connTmp = connTmp2, connTmp2 = list_entry(connTmp2->list.next, struct rs_conn_info, list)) {
            ret = RsSocketConnectAsync(connTmp, rsCb);
            if (ret != 0 && connTmp->state == RS_CONN_STATE_RESET) {
                connTmp->state = RS_CONN_STATE_ERR;
                hccp_err("[client]rs_socket_connect_async failed at RS_CONN_STATE_RESET state, ret:%d, client_ip:%s "
                    "server_ip:%s server_port:%u tag:%s", ret, connTmp->client_ip.read_addr,
                    connTmp->server_ip.read_addr, connTmp->port, connTmp->tag);
                continue;
            }
            if ((promoteConnect == false) && (RsGetSocketConnectState(connTmp) == 0)) {
                promoteConnect = true;
            }
        }
        RS_PTHREAD_MUTEX_ULOCK(&rsCb->mutex);

        if (promoteConnect) {
            usleep(RS_PROMOTE_CONN_USLEEP_TIME);
        } else {
            RsUsleepWaitConn(&rsCb->connect_trig_sem, RS_CONN_USLEEP_TIME);
        }
    }
    sem_destroy(&rsCb->connect_trig_sem);
    rsCb->conn_flag = RS_CONN_EXIT_FLAG;
    hccp_info("<SOCKET> QUIT thread_id:%lu, pid:%d", pthread_self(), getpid());

    return NULL;
}

int RsEpollConnectHandleInit(struct rs_cb *rscb)
{
    int ret;
    pthread_t ntid;

    ret = RsCreateEpoll(rscb);
    CHK_PRT_RETURN(ret, hccp_err("rs_create_epoll failed ! ret:%d", ret), ret);

    hccp_info("rs_create_epoll ok");
    gRsCb = rscb;

    ret = pthread_create(&ntid, NULL, RsEpollHandle, (void *)rscb);
    if (ret != 0) {
        gRsCb = NULL;
        RsDestroyEpoll(rscb);
        hccp_err("pthread_create failed ! ret:%d, errno:%d", ret, errno);
        return -ESYSFUNC;
    }

    pthread_t tidp;
    rscb->conn_flag = 1;
    ret = pthread_create(&tidp, NULL, RsConnectHandle, (void *)rscb);
    if (ret != 0) {
        gRsCb = NULL;
        RsDestroyEpoll(rscb);
        hccp_err("conn pthread_create failed ! ret:%d, errno:%d", ret, errno);
        return -ESYSFUNC;
    }

    hccp_info("RS INIT OK!");

    return ret;
}

int RsEpollCreateEpollfd(int *epollfd)
{
    RS_CHECK_POINTER_NULL_WITH_RET(epollfd);

    // 1024 specify the max fd num, this arg will be ignored since Linux 2.6.8
    *epollfd = epoll_create(1024);
    CHK_PRT_RETURN(*epollfd < 0, hccp_err("create epollfd[%d] failed, errno[%d]", *epollfd, errno), -EINVAL);

    return 0;
}

int RsEpollDestroyFd(int *fd)
{
    int ret = 0;

    RS_CHECK_POINTER_NULL_WITH_RET(fd);

    RS_CLOSE_RETRY_FOR_EINTR(ret, *fd);
    *fd = RS_FD_INVALID;

    return ret;
}

int RsEpollWaitHandle(int eventHandle, struct epoll_event *events, int timeout, unsigned int maxevents,
    unsigned int *eventsNum)
{
    int eventCount;

    RS_CHECK_POINTER_NULL_WITH_RET(events);
    RS_CHECK_POINTER_NULL_WITH_RET(eventsNum);

    eventCount = epoll_wait(eventHandle, events, (int)maxevents, timeout);
    if (eventCount < 0) {
        hccp_err("[rs_epoll_wait_handle]epoll_wait failed, strerror[%s]", strerror(errno));
        return -EIO;
    }

    *eventsNum = (unsigned int)eventCount;
    return 0;
}

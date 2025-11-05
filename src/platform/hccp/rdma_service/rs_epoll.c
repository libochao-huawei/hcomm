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

struct rs_pthread_info g_epoll_thread_info = {0};  //lint !e17
struct rs_pthread_info g_connect_thread_info = {0};

/*
 * opcode:
 *  ADD: EPOLL_CTL_ADD
 *  MOD: EPOLL_CTL_MOD
 *  DEL: EPOLL_CTL_DEL
 */
int rs_epoll_ctl(int epollfd, int op, int fd, unsigned int state)
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

int rs_epoll_ctl_fd_handle(int epollfd, int op, int fd, unsigned int state, void *fd_handle)
{
    int ret;
    struct epoll_event ev;

    ev.events = state;
    ev.data.ptr = fd_handle;
    ret = epoll_ctl(epollfd, op, fd, &ev);
    if (ret) {
        hccp_warn("epoll_ctl for fd %d unsuccessful! ret:%d errno:%d op:%d state:%u",
            fd, ret, errno, op, state);
    }
    return ret;
}

int rs_wlist_check_conn_add(struct rs_cb *rs_cb, struct rs_conn_info* conn_tmp)
{
    int ret;
    int ret_close;
    struct rs_conn_info *conn = NULL;

    if (rs_cb->conn_cb.wlist_enable == 1) {
        ret = rs_white_list_check_valid(rs_cb->chip_id, &rs_cb->conn_cb, conn_tmp);
        if (ret) {
            hccp_info("invalid client node found: chip_id %u, fd %d accept, server ip:%s, client ip:%s, port:%u, "
                "state:%d, tag:%s", rs_cb->chip_id, conn_tmp->connfd, conn_tmp->server_ip.read_addr,
                conn_tmp->client_ip.read_addr, conn_tmp->port, conn_tmp->state, conn_tmp->tag);
            return ret;
        }
    }

    /* add conn node to server list */
    ret = rs_alloc_conn_node(&conn, conn_tmp->port);
    if (ret) {
        hccp_err("server IP:0x%s add conn info to list fail, fd:%d, ret:%d",
            conn_tmp->server_ip.read_addr, conn_tmp->connfd, ret);
        goto alloc_err;
    }

    ret = rs_socket_copy_conn_info(conn_tmp, conn);
    if (ret) {
        hccp_err("rs_socket_copy_conn_info failed, ret[%d]", ret);
        goto out;
    }

    if (rs_cb->ssl_enable == RS_SSL_ENABLE) {
        ret = rs_epoll_ctl(rs_cb->conn_cb.epollfd, EPOLL_CTL_DEL, conn_tmp->connfd, EPOLLIN);
        if (ret) {
            hccp_err("rs epoll ctl failed, ret %d", ret);
            goto out;
        }
    }

    RS_PTHREAD_MUTEX_LOCK(&rs_cb->conn_cb.conn_mutex);
    rs_list_add_tail(&conn->list, &rs_cb->conn_cb.server_conn_list);
    RS_PTHREAD_MUTEX_ULOCK(&rs_cb->conn_cb.conn_mutex);

    hccp_info("[Server]chip_id %u, fd %d accept, server ip:%s, client ip:%s, state:%d, tag:%s",
        rs_cb->chip_id, conn_tmp->connfd, conn_tmp->server_ip.read_addr, conn_tmp->client_ip.read_addr,
        conn_tmp->state, conn_tmp->tag);
    show_conn_node(&(rs_cb->conn_cb.server_conn_list));
    return 0;

out:
    free(conn);
    conn = NULL;
alloc_err:
    RS_CLOSE_RETRY_FOR_EINTR(ret_close, conn_tmp->connfd);
    return ret;
}

STATIC int rs_ssl_recv_tag_in_handle(struct rs_accept_info *accept_info, struct rs_conn_info *conn_tmp)
{
    int exp_size = SOCK_CONN_TAG_SIZE + SOCK_CONN_DEV_ID_SIZE;
    char *recv_buff = conn_tmp->tag;
    struct timeval start_time, now;
    float time_cost = 0.0;
    int size = exp_size;

    rs_get_cur_time(&start_time);
    while (exp_size > 0 && size != 0) {
        conn_tmp->tag_sync_time++;
        size = ssl_adp_read(accept_info->ssl, recv_buff, exp_size);
        if ((size < 0) && (errno == EINTR)) {
            conn_tmp->tag_eintr_time++;
            continue;
        }
        exp_size -= size;
        recv_buff += size;
        rs_get_cur_time(&now);
        hccp_time_interval(&now, &start_time, &time_cost);
        if (time_cost >= RS_RECV_MAX_TIME) {
            hccp_run_info("recv tag time out, server:{%s:%u} client:%s tag_sync_time:%u tag_eintr_time:%u",
                accept_info->server_ip_addr.read_addr, accept_info->sock_port, accept_info->client_ip_addr.read_addr,
                conn_tmp->tag_sync_time, conn_tmp->tag_eintr_time);
            return -ETIME;
        }

        if (time_cost <= 0) {
            rs_get_cur_time(&start_time);
        }
    }

    conn_tmp->server_ip = accept_info->server_ip_addr;
    conn_tmp->client_ip = accept_info->client_ip_addr;
    conn_tmp->connfd = accept_info->conn_fd;
    conn_tmp->state = RS_CONN_STATE_TAG_SYNC;
    conn_tmp->port = accept_info->sock_port;
    conn_tmp->ssl = accept_info->ssl;

    hccp_info("recv tag success, server:{%s:%u} client:%s time_cost:%fms tag_sync_time:%u tag_eintr_time:%u",
        accept_info->server_ip_addr.read_addr, accept_info->sock_port, accept_info->client_ip_addr.read_addr, time_cost,
        conn_tmp->tag_sync_time, conn_tmp->tag_eintr_time);
    return 0;
}

STATIC void rs_epoll_event_ssl_recv_tag_in_handle(struct rs_cb *rs_cb, struct rs_accept_info *accept_info)
{
    struct rs_conn_info conn_tmp = {0};
    int ret_close;
    int ret;

    ret = rs_ssl_recv_tag_in_handle(accept_info, &conn_tmp);
    if (ret != 0) {
        RS_CLOSE_RETRY_FOR_EINTR(ret_close, accept_info->conn_fd);
        goto out;
    }

    ret = rs_wlist_check_conn_add(rs_cb, &conn_tmp);
out:
    if (ret != 0) {
        hccp_warn("recv tag or add conn unsuccessful ret:%d", ret);
        ssl_adp_shutdown(accept_info->ssl);
        ssl_adp_free(accept_info->ssl);
        accept_info->ssl = NULL;
    }

    /* do not shutdown ssl */
    RS_PTHREAD_MUTEX_LOCK(&rs_cb->conn_cb.conn_mutex);
    rs_list_del(&accept_info->list);
    free(accept_info);
    accept_info = NULL;
    RS_PTHREAD_MUTEX_ULOCK(&rs_cb->conn_cb.conn_mutex);

    return;
}

STATIC void rs_do_ssl_handshake(struct rs_accept_info *accept_info, struct rs_cb *rscb)
{
    int ret;
    int err;

    ret = ssl_adp_do_handshake(accept_info->ssl);
    if (ret == 1) {
#ifdef CONFIG_SSL
        ret = rs_tls_peer_cert_verify(accept_info->ssl, rscb);
        if (ret) {
            hccp_err("tls verify peer cert failed");
            return ;
        }
#endif
        accept_info->state = RS_CONN_STATE_SSL_CONNECTED;
    } else {
        err = ssl_adp_get_error(accept_info->ssl, ret);
        if (err == SSL_ERROR_WANT_WRITE) {
            hccp_info("return want write");
            return ;
        } else if (err == SSL_ERROR_WANT_READ) {
            hccp_info("return want read");
            return ;
        } else {
#ifdef CONFIG_SSL
            rs_ssl_err_string(accept_info->conn_fd, err);
#endif
            return ;
        }
    }

    return ;
}

STATIC int rs_epoll_event_ssl_accept_in_handle(struct rs_cb *rs_cb, int fd)
{
    int ret;

    struct rs_accept_info *accept_info = NULL;
    struct rs_accept_info *accept_info2 = NULL;

    /* Server event: ssl accept */
    RS_LIST_GET_HEAD_ENTRY(accept_info, accept_info2, &rs_cb->conn_cb.server_accept_list, list, struct rs_accept_info);
    for (; (&accept_info->list) != &rs_cb->conn_cb.server_accept_list;
        accept_info = accept_info2, accept_info2 = list_entry(accept_info2->list.next, struct rs_accept_info, list)) {
        /* connection request for Server */
        if (fd == accept_info->conn_fd) {
            if (accept_info->ssl == NULL) {
                accept_info->ssl = ssl_adp_new(rs_cb->server_ssl_ctx);
                CHK_PRT_RETURN(accept_info->ssl == NULL, hccp_err("server ssl ctx alloc failed"), -ENOMEM);

                ret = ssl_adp_set_fd(accept_info->ssl, accept_info->conn_fd);
                if (ret != 1) {
                    hccp_err("bind connfd and ssl fail, ret %d", ret);
                    ssl_adp_shutdown(accept_info->ssl);
                    ssl_adp_free(accept_info->ssl);
                    accept_info->ssl = NULL;
                    return -EINVAL;
                }

                ssl_adp_set_mode(accept_info->ssl, SSL_MODE_AUTO_RETRY);
                ssl_adp_set_accept_state(accept_info->ssl);
            }

            if (accept_info->state == RS_CONN_STATE_RESET) {
                rs_do_ssl_handshake(accept_info, rs_cb);
                return 0;
            }

            if (accept_info->state == RS_CONN_STATE_SSL_CONNECTED) {
                rs_epoll_event_ssl_recv_tag_in_handle(rs_cb, accept_info);
                return 0;
            }
        }
    }

    return -ENODEV;
}

STATIC int rs_epoll_tcp_recv(struct rs_cb *rs_cb, int fd)
{
    if (rs_cb->tcp_recv_callback != NULL) {
        rs_cb->tcp_recv_callback(rs_cb->fd_map[fd]);
    } else {
        hccp_err("[rs_epoll_tcp_recv]tcp_recv_callback is null.");
        return -EINVAL;
    }
    return 0;
}

STATIC int rs_epoll_event_heterog_tcp_recv_in_handle(struct rs_cb *rs_cb, int fd)
{
    int ret;
    struct rs_heterog_tcp_fd_info *fd_node = NULL;
    struct rs_heterog_tcp_fd_info *fd_node1 = NULL;

    RS_LIST_GET_HEAD_ENTRY(fd_node, fd_node1, &rs_cb->heterog_tcp_fd_list, list, struct rs_heterog_tcp_fd_info);
    for (; (&fd_node->list) != &rs_cb->heterog_tcp_fd_list;
        fd_node = fd_node1, fd_node1 = list_entry(fd_node1->list.next, struct rs_heterog_tcp_fd_info, list)) {
        if (fd_node->fd == fd) {
            // 处理tcp recv
            ret = rs_epoll_tcp_recv(rs_cb, fd);
            return ret;
        }
    }
    return -ENODEV;
}

STATIC void rs_epoll_event_in_handle(struct rs_cb *rs_cb, struct epoll_event *events)
{
    int fd = events->data.fd;
    int ret;

    ret = rs_epoll_event_ping_handle(rs_cb, fd);
    if (ret != -ENODEV) {
        hccp_info("the fd:%d is for ping, no need to go on, ret:%d", fd, ret);
        return;
    }

    ret = rs_epoll_event_listen_in_handle(rs_cb, fd);
    if (ret != -ENODEV) {
        hccp_info("the fd:%d is tcp listened, no need to go on, ret:%d", fd, ret);
        return;
    }

    if (rs_cb->ssl_enable == RS_SSL_ENABLE) {
        ret = rs_epoll_event_ssl_accept_in_handle(rs_cb, fd);
        if (ret != -ENODEV) {
            hccp_info("the fd:%d is ssl accept, no need to go on, ret:%d", fd, ret);
            return;
        }
    }

    ret = rs_epoll_event_qp_mr_in_handle(rs_cb, fd);
    if (ret != -ENODEV) {
        hccp_info("the fd:%d is for qp mr, no need to go on, ret:%d", fd, ret);
        return;
    }

    ret = rs_epoll_event_heterog_tcp_recv_in_handle(rs_cb, fd);
    if (ret != -ENODEV) {
        hccp_info("the fd:%d is for tcp recv, no need to go on, ret:%d", fd, ret);
        return;
    }

    return;
}

STATIC void rs_epoll_event_handle_one(struct rs_cb *rs_cb, struct epoll_event *events)
{
    RS_CHECK_POINTER_NULL_RETURN_VOID(events);
    RS_CHECK_POINTER_NULL_RETURN_VOID(rs_cb);

#ifdef CONFIG_TLV
    int ret;
    ret = rs_epoll_nslb_event_handle(&rs_cb->nslb_cb, events->data.fd, events->events);
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
        rs_epoll_event_in_handle(rs_cb, events);
    } else {
        hccp_warn("unknown event(0x%x)!", events->events);
    }

    return;
}

STATIC int set_affinity(unsigned int chip_id, unsigned int cpu_id)
{
    cpu_set_t mask;
    cpu_set_t get;
    int ret;

    CPU_ZERO(&mask);

    // 设置线程CPU亲和力
    CPU_SET(cpu_id, &mask);
    ret = pthread_setaffinity_np(pthread_self(), sizeof(mask), &mask);
    CHK_PRT_RETURN(ret < 0, hccp_err("could not set CPU affinity"), ret);

    // 获取线程cpu亲和力
    CPU_ZERO(&get);
    ret = pthread_getaffinity_np(pthread_self(), sizeof(get), &get);
    CHK_PRT_RETURN(ret, hccp_err("could not get CPU affinity"), ret);

    if (CPU_ISSET(cpu_id, &get)) { // 检查cpuid是否在这个集合中
        hccp_info("dev is %d thread %llu is running in processor %d.", chip_id, pthread_self(), cpu_id);
    } else {
        hccp_warn("dev is %d thread %llu is not running in processor %d.", chip_id, pthread_self(), cpu_id);
    }

    return ret;
}

STATIC int bind_data_cpu(unsigned int chip_id)
{
    int ret;
    unsigned int cpu_id;
    uint32_t info = 0;
    uint32_t dev_num = 0;
    int64_t ccpu_num;
    int64_t dcpu_num;
    int64_t acpu_num;
    int64_t cpu_num;

    // 判断当前处于host or device侧，如果在host侧，无需进行绑核操作
    ret = dl_drv_get_platform_info(&info);
    CHK_PRT_RETURN(ret, hccp_err("get PlatformInfo failed, ret[%d]", ret), ret);
    if (info == HOST) {
        hccp_info("host not need bind cpu, info[%u]", info);
        return 0;
    }

    // 获取当前os的device数量
    ret = dl_drv_get_dev_num(&dev_num);
    CHK_PRT_RETURN(ret, hccp_err("get device_num failed, ret[%d]", ret), ret);
    if (dev_num == 0) {
        hccp_info("no device need bind cpu, device num %u", dev_num);
        return 0;
    }

    // 获取data cpu数量
    ret = dl_hal_get_device_info(chip_id, MODULE_TYPE_DCPU, INFO_TYPE_CORE_NUM, &dcpu_num);
    CHK_PRT_RETURN(ret, hccp_err("get dcpu_num failed, ret(%d)", ret), ret);

    // 如果dcpu_num < 1，无需绑核直接返回
    if (dcpu_num < BIND_MIN_DCPU_NUM) {
        hccp_info("data cpu num %d, device not need bind cpu", dcpu_num);
        return 0;
    }

    // 获取ctrl cpu数量
    ret = dl_hal_get_device_info(chip_id, MODULE_TYPE_CCPU, INFO_TYPE_CORE_NUM, &ccpu_num);
    CHK_PRT_RETURN(ret, hccp_err("get ccpu_num failed, ret(%d)", ret), ret);

    // 获取ai cpu数量
    ret = dl_hal_get_device_info(chip_id, MODULE_TYPE_AICPU, INFO_TYPE_CORE_NUM, &acpu_num);
    CHK_PRT_RETURN(ret, hccp_err("get acpu_num failed, ret(%d)", ret), ret);

    // 计算单个device上的核数
    cpu_num = ccpu_num + dcpu_num + acpu_num;
    hccp_info("halGetDeviceInf chip = %u dev_num = %u, ccpu = %lld dcpu = %lld acpu = %lld cpu_num = %lld",
        chip_id, dev_num, ccpu_num, dcpu_num, acpu_num, cpu_num);

    cpu_id = (unsigned int)((int64_t)(chip_id % dev_num) * cpu_num + ccpu_num);

    // 进行绑核
    ret = dl_hal_bind_cgroup(BIND_DATACPU_CGROUP);
    CHK_PRT_RETURN(ret, hccp_err("bind cgroup failed, ret[%d], strerror[%s]",
        ret, strerror(errno)), ret);
    hccp_info("bind cgroup success!");

    ret = set_affinity(chip_id, cpu_id);
    CHK_PRT_RETURN(ret, hccp_err("set affinity with cpu[%u] failed", cpu_id), ret);

    hccp_info("chip_id[%u] bind data cpu[%u] success", chip_id, cpu_id);

    return ret;
}

STATIC void rs_socket_save_epoll_wait_err_info(int event_num, int err_no, struct socket_err_info *epoll_err_info)
{
    if (event_num > 0) {
        return;
    }

    rs_socket_save_err_info(RS_CONN_STATE_RESET, err_no, epoll_err_info);
}

STATIC void *rs_epoll_handle(void *arg)
{
    struct rs_conn_cb *conn_cb = NULL;
    struct rs_cb *rs_cb = NULL;
    eventfd_t val;
    int ret;
    int num;
    int i;

    RS_CHECK_POINTER_NULL_RETURN_NULL(arg);

    hccp_info("<EPOLL> thread begin! thread_id:%lu, pid:%d, ppid:%d",
        pthread_self(), getpid(), getppid());

    struct epoll_event events[RS_EPOLL_EVENT];

    CHK_PRT_RETURN(pthread_detach(pthread_self()), hccp_err("pthread_detach fail! thread_id:%lu, errno:%d",
        pthread_self(), errno), NULL);

    (void)prctl(PR_SET_NAME, (unsigned long)"hccp_epoll");

    rs_cb = (struct rs_cb *)arg;
    g_rs_cb = rs_cb;
    conn_cb = &rs_cb->conn_cb;

    ret = bind_data_cpu(rs_cb->chip_id);
    if (ret) {
        hccp_warn("bind data cpu unsuccessful! thread_id:%lu, errno:%d", pthread_self(), errno);
    }

    rs_cb->state &= ~RS_STATE_HALT;
    rs_get_cur_time(&g_epoll_thread_info.last_check_time);
    ret = strncpy_s((char *)g_epoll_thread_info.pthread_name, sizeof(g_epoll_thread_info.pthread_name),
        "epoll_pthread", strlen("epoll_pthread"));
    CHK_PRT_RETURN(ret, hccp_err("strncpy_s pthread name failed, ret[%d]", ret), NULL);

    hccp_run_info("pthread[%s] is alive!", g_epoll_thread_info.pthread_name);

    ret = rs_drv_init_cqe_err_info();
    CHK_PRT_RETURN(ret, hccp_err("rs_drv_init_cqe_err_info failed, ret[%d]", ret), NULL);
    while (1) {
        do {
            num = epoll_wait(conn_cb->epollfd, events, RS_EPOLL_EVENT, -1);
        } while ((num < 0) && (errno == EINTR));
        rs_socket_save_epoll_wait_err_info(num, -errno, &conn_cb->epoll_err_info);

        /* eventfd is for wake up epoll wait, value is ignored */
        if (events[0].data.fd == conn_cb->eventfd) {
            hccp_warn("<EPOLL> SHUT DOWN event!!! eventfd:%d", conn_cb->eventfd);
            do {
                num = read(conn_cb->eventfd, &val, sizeof(eventfd_t));
            } while ((num < 0) && (errno == EINTR));
            RS_PTHREAD_MUTEX_LOCK(&rs_cb->mutex);
            rs_cb->state |= RS_STATE_HALT;
            RS_PTHREAD_MUTEX_ULOCK(&rs_cb->mutex);

            break;
        }

        rs_heartbeat_alive_print(&g_epoll_thread_info);
        RS_PTHREAD_MUTEX_LOCK(&rs_cb->mutex);
        /* events handle one by one */
        for (i = 0; i < num; i++) {
            rs_epoll_event_handle_one(rs_cb, &events[i]);
        }

        RS_PTHREAD_MUTEX_ULOCK(&rs_cb->mutex);
    }
    rs_drv_deinit_cqe_err_info();
    hccp_info("<EPOLL> QUIT thread_id:%lu, pid:%d, read num:%d", pthread_self(), getpid(), num);

    return NULL;
}

STATIC int rs_create_epoll(struct rs_cb *rs_cb)
{
    int ret, ret_fd;
    struct rs_conn_cb *conn_cb = NULL;

    conn_cb = &rs_cb->conn_cb;

    conn_cb->epollfd = epoll_create(1);
    CHK_PRT_RETURN(conn_cb->epollfd < 0, hccp_err("epollfd[%d] failed, errno[%d]", conn_cb->epollfd, errno), -EINVAL);

    conn_cb->eventfd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    if (conn_cb->eventfd == RS_FD_INVALID) {
        RS_CLOSE_RETRY_FOR_EINTR(ret_fd, conn_cb->epollfd);
        conn_cb->epollfd = RS_FD_INVALID;
        hccp_err("create eventfd for rs cb failed !");
        return -EINVAL;
    }

    ret = rs_epoll_ctl(conn_cb->epollfd, EPOLL_CTL_ADD, conn_cb->eventfd, EPOLLIN);
    if (ret) {
        hccp_err("re epoll ctl failed, epollfd[%d], eventfd[%d]", conn_cb->epollfd, conn_cb->eventfd);
        RS_CLOSE_RETRY_FOR_EINTR(ret_fd, conn_cb->eventfd);
        conn_cb->eventfd = RS_FD_INVALID;
        RS_CLOSE_RETRY_FOR_EINTR(ret_fd, conn_cb->epollfd);
        conn_cb->epollfd = RS_FD_INVALID;
        return ret;
    }

    return 0;
}

void rs_destroy_epoll(struct rs_cb *rs_cb)
{
    int ret;
    struct rs_conn_cb *conn_cb = NULL;

    conn_cb = &rs_cb->conn_cb;

    ret = rs_epoll_ctl(conn_cb->epollfd, EPOLL_CTL_DEL, conn_cb->eventfd, EPOLLIN);
    if (ret) {
        hccp_warn("re epoll ctl unsuccessful, epollfd[%d], eventfd[%d]", conn_cb->epollfd, conn_cb->eventfd);
    }

    RS_CLOSE_RETRY_FOR_EINTR(ret, conn_cb->eventfd);
    conn_cb->eventfd = RS_FD_INVALID;

    RS_CLOSE_RETRY_FOR_EINTR(ret, conn_cb->epollfd);
    conn_cb->epollfd = RS_FD_INVALID;

    return;
}

STATIC void rs_usleep_wait_conn(sem_t* sem, uint32_t timeout_us)
{
    uint32_t sleep_us = 1000; // 每 1000us 查看sem是否为零
    uint32_t elapsed_us = 0;
    do {
        if (sem_trywait(sem) == 0) {
            return;
        }
        usleep(sleep_us);
        elapsed_us += sleep_us;
    } while (elapsed_us < timeout_us);
    return;
}

STATIC void *rs_connect_handle(void *arg)
{
    struct rs_conn_info *conn_tmp2 = NULL;
    struct rs_conn_info *conn_tmp = NULL;
    bool promote_connect = false;
    int ret;

    hccp_info("<SOCKET> thread begin! thread_id:%lu, pid:%d, ppid:%d",
        pthread_self(), getpid(), getppid());
    CHK_PRT_RETURN(pthread_detach(pthread_self()), hccp_err("pthread_detach fail! thread_id:%lu, errno:%d",
        pthread_self(), errno), NULL);

    (void)prctl(PR_SET_NAME, (unsigned long)"hccp_connect");

    rs_get_cur_time(&g_connect_thread_info.last_check_time);
    ret = strncpy_s((char *)g_connect_thread_info.pthread_name, sizeof(g_connect_thread_info.pthread_name),
        "connect_pthread", strlen("connect_pthread"));
    CHK_PRT_RETURN(ret, hccp_err("strncpy_s pthread name failed, ret[%d]", ret), NULL);
    struct rs_cb *rs_cb = (struct rs_cb *)arg;
    if (rs_cb == NULL) {
        return NULL;
    }

    g_rs_cb = rs_cb;
    sem_init(&rs_cb->connect_trig_sem, 0, 0);

    hccp_run_info("pthread[%s] is alive!", g_connect_thread_info.pthread_name);
    while (1) {
        if (rs_cb == NULL) {
            return NULL;
        }

        if (rs_cb->conn_flag == 0) {
            break;
        }

        promote_connect = false;
        rs_heartbeat_alive_print(&g_connect_thread_info);
        RS_PTHREAD_MUTEX_LOCK(&rs_cb->mutex);
        RS_LIST_GET_HEAD_ENTRY(conn_tmp, conn_tmp2, &rs_cb->conn_cb.client_conn_list, list, struct rs_conn_info);
        for (; (&conn_tmp->list) != &rs_cb->conn_cb.client_conn_list;
            conn_tmp = conn_tmp2, conn_tmp2 = list_entry(conn_tmp2->list.next, struct rs_conn_info, list)) {
            ret = rs_socket_connect_async(conn_tmp, rs_cb);
            if (ret != 0 && conn_tmp->state == RS_CONN_STATE_RESET) {
                conn_tmp->state = RS_CONN_STATE_ERR;
                hccp_err("[client]rs_socket_connect_async failed at RS_CONN_STATE_RESET state, ret:%d, client_ip:%s "
                    "server_ip:%s server_port:%u tag:%s", ret, conn_tmp->client_ip.read_addr,
                    conn_tmp->server_ip.read_addr, conn_tmp->port, conn_tmp->tag);
                continue;
            }
            if ((promote_connect == false) && (rs_get_socket_connect_state(conn_tmp) == 0)) {
                promote_connect = true;
            }
        }
        RS_PTHREAD_MUTEX_ULOCK(&rs_cb->mutex);

        if (promote_connect) {
            usleep(RS_PROMOTE_CONN_USLEEP_TIME);
        } else {
            rs_usleep_wait_conn(&rs_cb->connect_trig_sem, RS_CONN_USLEEP_TIME);
        }
    }
    sem_destroy(&rs_cb->connect_trig_sem);
    rs_cb->conn_flag = RS_CONN_EXIT_FLAG;
    hccp_info("<SOCKET> QUIT thread_id:%lu, pid:%d", pthread_self(), getpid());

    return NULL;
}

int rs_epoll_connect_handle_init(struct rs_cb *rscb)
{
    int ret;
    pthread_t ntid;

    ret = rs_create_epoll(rscb);
    CHK_PRT_RETURN(ret, hccp_err("rs_create_epoll failed ! ret:%d", ret), ret);

    hccp_info("rs_create_epoll ok");
    g_rs_cb = rscb;

    ret = pthread_create(&ntid, NULL, rs_epoll_handle, (void *)rscb);
    if (ret != 0) {
        g_rs_cb = NULL;
        rs_destroy_epoll(rscb);
        hccp_err("pthread_create failed ! ret:%d, errno:%d", ret, errno);
        return -ESYSFUNC;
    }

    pthread_t tidp;
    rscb->conn_flag = 1;
    ret = pthread_create(&tidp, NULL, rs_connect_handle, (void *)rscb);
    if (ret != 0) {
        g_rs_cb = NULL;
        rs_destroy_epoll(rscb);
        hccp_err("conn pthread_create failed ! ret:%d, errno:%d", ret, errno);
        return -ESYSFUNC;
    }

    hccp_info("RS INIT OK!");

    return ret;
}

int rs_epoll_create_epollfd(int *epollfd)
{
    RS_CHECK_POINTER_NULL_WITH_RET(epollfd);

    // 1024 specify the max fd num, this arg will be ignored since Linux 2.6.8
    *epollfd = epoll_create(1024);
    CHK_PRT_RETURN(*epollfd < 0, hccp_err("create epollfd[%d] failed, errno[%d]", *epollfd, errno), -EINVAL);

    return 0;
}

int rs_epoll_destroy_fd(int *fd)
{
    int ret = 0;

    RS_CHECK_POINTER_NULL_WITH_RET(fd);

    RS_CLOSE_RETRY_FOR_EINTR(ret, *fd);
    *fd = RS_FD_INVALID;

    return ret;
}

int rs_epoll_wait_handle(int event_handle, struct epoll_event *events, int timeout, unsigned int maxevents,
    unsigned int *events_num)
{
    int event_count;

    RS_CHECK_POINTER_NULL_WITH_RET(events);
    RS_CHECK_POINTER_NULL_WITH_RET(events_num);

    event_count = epoll_wait(event_handle, events, (int)maxevents, timeout);
    if (event_count < 0) {
        hccp_err("[rs_epoll_wait_handle]epoll_wait failed, strerror[%s]", strerror(errno));
        return -EIO;
    }

    *events_num = (unsigned int)event_count;
    return 0;
}

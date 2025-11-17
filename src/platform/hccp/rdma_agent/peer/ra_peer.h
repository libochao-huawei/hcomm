/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef RA_PEER_H
#define RA_PEER_H

#include <pthread.h>
#include <infiniband/verbs.h>
#include "hccp_common.h"
#include "ra_rs_comm.h"
#include "ra_comm.h"

#define HCCP_CLOSE_RETRY_FOR_EINTR(fd) do { \
    int ret_; \
    do { \
        ret_ = close(fd); \
        if (ret_ < 0) { \
            hccp_warn("close filedscp[%d] unsuccessful, errno:%d", fd, errno); \
        } \
    } while ((ret_ < 0) && (errno == EINTR)); \
    fd = -1; \
} while (0)

#define PEER_PTHREAD_MUTEX_LOCK(mutex) do { \
    int ret_lock = pthread_mutex_lock(mutex); \
    if (ret_lock) { \
        hccp_warn("pthread_mutex_lock unsuccessful, ret[%d]", ret_lock); \
    }\
} while (0)

#define PEER_PTHREAD_MUTEX_UNLOCK(mutex) do { \
    int ret_ulock = pthread_mutex_unlock(mutex); \
    if (ret_ulock) { \
        hccp_warn("pthread_mutex_unlock unsuccessful, ret[%d]", ret_ulock); \
    } \
} while (0)

#define RA_SSL_DISABLE 0

struct host_roce_notify_info {
    unsigned int logic_id;
    unsigned long long va;
    unsigned long long sz;
};

#define HOST_DEVICE_NAME     "/dev/host_rdma"
#define RA_NOTIFY_TYPE_TOTAL_SIZE   1

#define HOST_CDEV_IOC_MAGIC  '%'

#define HOST_CDEV_IOC_FREE_NOTIFY _IOWR(HOST_CDEV_IOC_MAGIC, 1, struct host_roce_notify_info)

int ra_peer_socket_batch_connect(unsigned int dev_id, struct socket_connect_info_t conn[], unsigned int num);

int ra_peer_socket_batch_close(unsigned int dev_id, struct socket_close_info_t conn[], unsigned int num);

int ra_peer_socket_batch_abort(unsigned int dev_id, struct socket_connect_info_t conn[], unsigned int num);

int ra_peer_socket_listen_start(unsigned int dev_id, struct socket_listen_info_t conn[], unsigned int num);

int ra_peer_socket_listen_stop(unsigned int dev_id, struct socket_listen_info_t conn[], unsigned int num);

int ra_peer_get_sockets(unsigned int phy_id, unsigned int role, struct socket_info_t conn[], unsigned int num);

int ra_peer_socket_send(unsigned int dev_id, const void *handle, const void *data, unsigned long long size);

int ra_peer_socket_recv(unsigned int dev_id, const void *handle, void *data, unsigned long long size);

int ra_peer_epoll_ctl_add(const void *fd_handle, enum RaEpollEvent event);

int ra_peer_epoll_ctl_mod(const void *fd_handle, enum RaEpollEvent event);

int ra_peer_epoll_ctl_del(const void *fd_handle);

void ra_peer_set_tcp_recv_callback(unsigned int phy_id, const void *callback);

int ra_peer_socket_white_list_add(struct rdev rdev_info, struct socket_wlist_info_t white_list[], unsigned int num);

int ra_peer_socket_white_list_del(struct rdev rdev_info, struct socket_wlist_info_t white_list[], unsigned int num);

int ra_peer_socket_deinit(struct rdev rdev_info);

int ra_peer_qp_create(struct ra_rdma_handle *rdma_handle, int flag, int qp_mode, void **qp_handle);

int ra_peer_qp_create_with_attrs(struct ra_rdma_handle *rdma_handle, struct qp_ext_attrs *ext_attrs, void **qp_handle);

int ra_peer_loopback_qp_create(struct ra_rdma_handle *rdma_handle, struct loopback_qp_pair *qp_pair, void **qp_handle);

int ra_peer_qp_destroy(struct ra_qp_handle *qp_peer);

int ra_peer_typical_qp_modify(struct ra_qp_handle *qp_peer, struct typical_qp *local_qp_info,
    struct typical_qp *remote_qp_info);

int ra_peer_qp_connect_async(struct ra_qp_handle *qp_peer, const void *sock_handle);

int ra_peer_get_qp_status(struct ra_qp_handle *qp_peer, int *status);

int ra_peer_mr_reg(struct ra_qp_handle *qp_peer, struct mr_info *info);

int ra_peer_mr_dereg(struct ra_qp_handle *qp_peer, struct mr_info *info);

int ra_peer_register_mr(struct ra_rdma_handle *rdma_peer, struct mr_info *info, void **mr_handle);

int ra_peer_deregister_mr(struct ra_rdma_handle *rdma_peer, void *mr_handle);

int ra_peer_send_wr(struct ra_qp_handle *qp_peer, struct send_wr *wr, struct send_wr_rsp *op_rsp);

int ra_peer_send_wrlist(struct ra_qp_handle *qp_handle, struct send_wrlist_data wr[], struct send_wr_rsp op_rsp[],
    struct wrlist_send_complete_num wrlist_num);

int ra_peer_recv_wrlist(struct ra_qp_handle *qp_handle, struct recv_wrlist_data *wr, unsigned int recv_num,
    unsigned int *complete_num);

int ra_peer_get_notify_base_addr(struct ra_rdma_handle *handle, unsigned long long *va, unsigned long long *size);

int ra_peer_init(struct ra_init_config *cfg, unsigned int white_list_status);

int ra_peer_get_tls_enable(unsigned int phy_id, bool *tls_enable);

int ra_peer_get_sec_random(unsigned int *value);

int ra_peer_deinit(struct ra_init_config *cfg);

int ra_peer_get_ifnum(unsigned int phy_id, unsigned int *num);

int ra_peer_get_ifaddrs(unsigned int phy_id, struct interface_info interface_infos[], unsigned int *num);

int ra_peer_rdev_init(
    struct ra_rdma_handle *rdma_handle, unsigned int notify_type, struct rdev rdev_info, unsigned int *rdev_index);

int ra_peer_rdev_deinit(struct ra_rdma_handle *rdma_handle, unsigned int notify_type);

int host_notify_base_addr_init(unsigned int phy_id);

int ra_peer_notify_base_addr_init(unsigned int notify_type, unsigned int phy_id);

int host_notify_base_addr_uninit(unsigned int phy_id);

int notify_base_addr_uninit(unsigned int notify_type, unsigned int phy_id);

int ra_peer_set_tsqp_depth(struct ra_rdma_handle *rdma_handle, unsigned int temp_depth, unsigned int *qp_num);

int ra_peer_get_tsqp_depth(struct ra_rdma_handle *rdma_handle, unsigned int *temp_depth, unsigned int *qp_num);

int ra_peer_get_qp_context(struct ra_qp_handle *qp_peer, void** qp, void** send_cq, void** recv_cq);

int ra_peer_normal_qp_create(struct ra_rdma_handle *rdma_handle, struct ibv_qp_init_attr *qp_init_attr,
    void **qp_handle, void** qp);

int ra_peer_normal_qp_destroy(struct ra_qp_handle *qp_peer);

int ra_peer_cq_create(struct ra_rdma_handle *rdma_handle, struct cq_attr *attr);

int ra_peer_cq_destroy(struct ra_rdma_handle *rdma_handle, struct cq_attr *attr);

int ra_peer_set_qp_attr_qos(struct ra_qp_handle *qp_peer, struct qos_attr *attr);

int ra_peer_set_qp_attr_timeout(struct ra_qp_handle *qp_peer, unsigned int *timeout);

int ra_peer_set_qp_attr_retry_cnt(struct ra_qp_handle *qp_peer, unsigned int *retry_cnt);

int ra_peer_create_comp_channel(struct ra_rdma_handle *rdma_handle, void** comp_channel);

int ra_peer_destroy_comp_channel(void* comp_channel);

int ra_peer_create_srq(struct ra_rdma_handle *rdma_handle, struct srq_attr *attr);

int ra_peer_destroy_srq(struct ra_rdma_handle *rdma_handle, struct srq_attr *attr);

int ra_peer_create_event_handle(int *event_handle);

int ra_peer_ctl_event_handle(int event_handle, const void *fd_handle, int opcode, enum RaEpollEvent event);

int ra_peer_wait_event_handle(int event_handle, struct socket_event_info *event_infos, int timeout,
    unsigned int maxevents, unsigned int *events_num);

int ra_peer_destroy_event_handle(int *event_handle);

void ra_peer_mutex_lock(unsigned int phy_id);

void ra_peer_mutex_unlock(unsigned int phy_id);
#endif // RA_PEER_H

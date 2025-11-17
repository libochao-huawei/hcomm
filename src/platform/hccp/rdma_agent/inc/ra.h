/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef RA_H
#define RA_H

#include <dlfcn.h>
#include <sched.h>
#include "stdio.h"
#include "hccp_common.h"
#ifndef HNS_ROCE_LLT
#include "dlog_pub.h"
#endif
#include "user_log.h"
#include "ra_rs_comm.h"
#include "rdma_lite.h"

#if defined(HNS_ROCE_LLT) || defined(DEFINE_HNS_LLT)
#define STATIC
#else
#define STATIC static
#endif

#define RA_MAX_PHY_ID_NUM 64

#define MAX_SUPPORT_IFNUM 65536
#define SOCKET_SEND_MAXLEN 2048
#define MAX_HDC_DATA 65536
#define MAX_SOCKET_NUM 16

#define MAX_WLIST_NUM 16
#define MAX_WLIST_NUM_V1 32
#define MAX_SG_LIST_LEN_MAX     2147483648

#define ra_conn_para_check(conn, num) \
    ((num) == 0 || (num) > MAX_SOCKET_NUM)

#define PROCESS_RA_SIGN_LENGTH 49
#define PROCESS_RA_RESV_LENGTH 4

#define MAX_IP_LEN 64

#define MAX_POLL_CQE_NUM 100

struct process_ra_sign {
    pid_t tgid;
    char sign[PROCESS_RA_SIGN_LENGTH];
};

#define container_of(ptr, type, member)                    \
    ({                                                     \
        const typeof(((type *)0)->member) *__mptr = (ptr); \
        (type *)((char *)__mptr - offsetof(type, member)); \
    })

#define list_entry(n, type, member) container_of(n, type, member)

struct ra_list_head {
    struct ra_list_head *next, *prev;
};

static inline void RA_INIT_LIST_HEAD(struct ra_list_head *list)
{
    list->next = list;
    list->prev = list;
}

#define RA_LIST_GET_HEAD_ENTRY(pos, n, head, member, type) do { \
    (pos) = list_entry((head)->next, type, member);       \
    (n) = list_entry((pos)->member.next, type, member);     \
} while (0)

static inline bool ra_list_empty(struct ra_list_head *head)
{
    return head->next == head;
}

static inline void ra_list_add_(struct ra_list_head *xnew, struct ra_list_head *prev, struct ra_list_head *next)
{
    next->prev = xnew;
    xnew->next = next;
    xnew->prev = prev;
    prev->next = xnew;
}

static inline void ra_list_add_tail(struct ra_list_head *xnew, struct ra_list_head *head)
{
    ra_list_add_(xnew, head->prev, head);
}

static inline void ra_list_del_(struct ra_list_head *prev, struct ra_list_head *next)
{
    next->prev = prev;
    prev->next = next;
}

static inline void ra_list_del(struct ra_list_head *entry)
{
    ra_list_del_(entry->prev, entry->next);
}

struct ra_backup_info {
    bool backup_flag;
    struct rdev rdev_info;
};

struct ra_cqe_err_info {
    pthread_mutex_t mutex;
    struct cqe_err_info info;
};

enum rdma_lite_thread_status {
    LITE_THREAD_STATUS_DESTROY = 0,
    LITE_THREAD_STATUS_RUNNING = 1,
    LITE_THREAD_STATUS_FINISH_RUNNING = 2,
    LITE_THREAD_STATUS_SUSPEND = 3,
};

struct ra_rdma_handle {
    unsigned int rdev_index;
    struct rdev rdev_info;
    struct ra_backup_info backup_info;
    struct ra_rdma_ops *rdma_ops;
    int support_lite;
    struct rdma_lite_context *lite_ctx;
    struct ra_list_head qp_list;
    pthread_mutex_t rdev_mutex;
    pthread_mutex_t cqe_err_cnt_mutex;
    unsigned int cqe_err_cnt;
    pthread_t tid;
    enum rdma_lite_thread_status thread_status;
    bool disabled_lite_thread;
    bool enabled_910a_lite;
    unsigned int logic_devid;
    int sensor_update_cnt;
    uint64_t sensor_handle;
    uint64_t qp_cnt;  // record the number of ra_qp_create_with_attrs function calls
    bool enabled_2mb_lite;
    uint8_t gid[HCCP_GID_RAW_LEN];
    uint64_t notify_va;
    uint64_t notify_size;
};

struct ra_socket_handle {
    int scope_id;
    struct rdev rdev_info;
    struct ra_socket_ops *socket_ops;
    uint64_t close_cnt;      // record the number of ra_socket_batch_close function calls
    uint64_t connect_cnt;    // record the number of ra_socket_batch_connect function calls
    uint64_t abort_cnt;      // record the number of ra_socket_batch_abort function calls
};

struct ra_loopback_info {
    struct ibv_cq *ib_send_cq;
    struct ibv_cq *ib_recv_cq;
    void *cq_context;
};

struct ra_qp_handle {
    unsigned int qpn;
    int qp_mode;
    int flag;
    unsigned int phy_id;
    unsigned int rdev_index;
    struct ra_rdma_ops *rdma_ops; // only ra use
    int support_lite;
    struct rdma_lite_cq *send_lite_cq;
    struct rdma_lite_cq *recv_lite_cq;
    struct rdma_lite_qp *lite_qp;
    struct lite_mr_info local_mr[RA_MR_MAX_NUM];
    struct lite_mr_info rem_mr[RA_MR_MAX_NUM];
    pthread_mutex_t qp_mutex;
    struct ra_cqe_err_info cqe_err_info;
    int db_index;
    unsigned int send_wr_num;
    unsigned int poll_cqe_num;
    unsigned int recv_wr_num;
    unsigned int poll_recv_cqe_num;
    struct ra_list_head list;
    struct ra_rdma_handle *rdma_handle;
    struct rdma_lite_wc *lite_wc;
    unsigned int mem_idx;
    int sq_sig_all;
    unsigned int udp_sport;
    unsigned int psn;
    unsigned int gid_idx;
    unsigned int sq_depth; // only valid in RDMA Lite scenario
    struct ra_loopback_info *loopback_info;
    struct ra_qp_handle *loopback_qp_handle;
};

struct ra_mr_handle {
    uint64_t addr;
};

enum get_ifaddrs_version {
    GET_IFADDRS_VERSION_1 = 1,
    GET_IFADDRS_VERSION_2,
    GET_IFADDRS_VERSION_3,
    GET_IFADDRS_MAX_VERSION,
};

enum ra_qp_status {
    RA_QP_STATUS_DISCONNECT = 0,
    RA_QP_STATUS_CONNECTED = 1,
    RA_QP_STATUS_TIMEOUT = 2,
    RA_QP_STATUS_CONNECTING = 3,
    RA_QP_STATUS_REM_FD_CLOSE = 4,
    RA_QP_STATUS_PAUSE = 5,
};
#endif // RA_H

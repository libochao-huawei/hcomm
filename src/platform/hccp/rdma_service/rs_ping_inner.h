/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef RS_PING_INNER_H
#define RS_PING_INNER_H

#include <pthread.h>
#include <infiniband/verbs.h>
#include "hccp_ping.h"
#include "rs_list.h"
#include "rs_common_inner.h"

#define RS_PING_SEC_TO_USEC 1000000
#define RS_PING_MSEC_TO_USEC 1000
#define RS_PING_PERIOD_TIME_USEC 10000
#define RS_PING_PAYLOAD_HEADER_RESV_GRH 40U
#define RS_PING_PAYLOAD_HEADER_MASK_SIZE 104U
#define RS_PING_PAYLOAD_HEADER_RESV_CUSTOM sizeof(struct rs_ping_payload_header)

enum rs_ping_thread_status {
    RS_PING_THREAD_RESET = 0,
    RS_PING_THREAD_RUNNING = 1,
    RS_PING_THREAD_FINISH = 2
};

enum rs_ping_task_status {
    RS_PING_TASK_RESET = 0,
    RS_PING_TASK_RUNNING = 1
};

struct rs_ping_mr_cb {
    uint32_t payload_offset;
    uint64_t len;

    pthread_mutex_t mutex;
    uint64_t addr;

    struct ibv_mr *ib_mr;
    uint32_t sge_num;
    struct ibv_sge *sge_list;
    uint32_t sge_idx;
};

struct rs_ping_cq_info {
    int depth;
    int comp_vector;
    struct ibv_cq *ib_cq;
    uint32_t num_events;
    int max_recv_wc_num;
};

struct rs_ping_local_qp_cb {
    struct ibv_comp_channel *channel;
    struct rs_ping_cq_info send_cq;
    struct rs_ping_cq_info recv_cq;

    uint32_t qkey;
    struct ibv_qp_cap qp_cap;
    uint32_t udp_sport;
    struct ibv_qp *ib_qp;

    struct rs_ping_mr_cb send_mr_cb;
    struct rs_ping_mr_cb recv_mr_cb;
};

enum rs_ping_pong_target_state {
    RS_PING_PONG_TARGET_RESET = 0,
    RS_PING_PONG_TARGET_READY = 1,
    RS_PING_PONG_TARGET_FINISH = 2,
    RS_PING_PONG_TARGET_ERROR = 3,
    RS_PING_PONG_TARGET_MAX
};

enum rs_ping_type {
    RS_PING_TYPE_ROCE_DETECT = 1,
    RS_PING_TYPE_ROCE_RESPONSE = 2,
    RS_PING_TYPE_URMA_DETECT = 3,
    RS_PING_TYPE_URMA_RESPONSE = 4
};

struct rs_ping_timestamp {
    uint64_t tv_sec1;
    uint64_t tv_usec1;
    uint64_t tv_sec2;
    uint64_t tv_usec2;
    uint64_t tv_sec3;
    uint64_t tv_usec3;
    uint64_t tv_sec4;
    uint64_t tv_usec4;
};

struct rs_ping_payload_header {
    int version;
    enum rs_ping_type type;
    struct ping_qp_info server;
    struct ping_qp_info target;
    struct rs_ping_timestamp timestamp;
    uint32_t task_id;
    uint8_t reserved[42U];
    uint16_t magic;
};

struct rs_pong_target_info {
    struct ping_qp_info qp_info;
    union {
        struct ibv_ah *ah;
    };

    enum rs_ping_pong_target_state state;

    struct rs_list_head list;
    uint64_t uuid;
};

struct rs_ping_target_info {
    char *payload_buffer;
    uint32_t payload_size;

    struct ping_qp_info qp_info;
    union {
        struct ibv_ah *ah;
    };

    enum rs_ping_pong_target_state state;

    pthread_mutex_t trip_mutex;
    struct ping_result_summary result_summary;

    struct rs_list_head list;
    uint64_t uuid;
};

struct rs_ping_rdev_cb {
    struct rs_ip_addr_info ip;
    const char *dev_name;
    unsigned char ib_port;
    union ibv_gid gid;

    int dev_num;
    struct ibv_device **dev_list;
    int gid_idx;
    struct ibv_context *ib_ctx;
    struct ibv_pd *ib_pd;
};

struct rs_ping_ctx_cb {
    enum protocol_type protocol;
    struct rs_ping_pong_ops *ping_pong_ops;
    struct rs_ping_pong_dfx *ping_pong_dfx;
    pthread_t tid;
    int thread_status;

    pthread_mutex_t ping_mutex;
    struct rs_list_head ping_list;
    unsigned int ping_num;

    pthread_mutex_t pong_mutex;
    struct rs_list_head pong_list;
    unsigned int pong_num;

    struct ping_local_comm_info comm_info;
    unsigned int logic_devid;
    unsigned int init_cnt;

    unsigned int dev_index;
    pthread_mutex_t dev_mutex;
    union {
        struct rs_ping_rdev_cb rdev_cb;
    };

    union {
        struct rs_ping_local_qp_cb ping_qp;
    };

    union {
        struct rs_ping_local_qp_cb pong_qp;
    };

    int task_status;
    struct ping_task_attr task_attr;
    unsigned int task_id;
};

struct rs_ping_pong_ops {
    bool (*check_ping_fd)(struct rs_ping_ctx_cb *ping_cb, int fd);
    bool (*check_pong_fd)(struct rs_ping_ctx_cb *ping_cb, int fd);
    int (*init_ping_cb)(unsigned int phy_id, struct ping_init_attr *attr, struct ping_init_info *info,
        unsigned int *dev_index, struct rs_ping_ctx_cb *ping_cb);
    int (*ping_find_target_node)(struct rs_ping_ctx_cb *ping_cb, struct ping_qp_info *target,
        struct rs_ping_target_info **node);
    int (*ping_alloc_target_node)(struct rs_ping_ctx_cb *ping_cb, struct ping_target_info *target,
        struct rs_ping_target_info **node);
    void (*reset_recv_buffer)(struct rs_ping_ctx_cb *ping_cb);
    int (*ping_post_send)(struct rs_ping_ctx_cb *ping_cb, struct rs_ping_target_info *target);
    int (*ping_poll_scq)(struct rs_ping_ctx_cb *ping_cb, struct rs_ping_target_info *target);
    int (*ping_poll_rcq)(struct rs_ping_ctx_cb *ping_cb, int *polled_cnt, struct timeval *timestamp2);
    void (*pong_handle_send)(struct rs_ping_ctx_cb *ping_cb, int polled_cnt, struct timeval *timestamp2);
    void (*pong_poll_rcq)(struct rs_ping_ctx_cb *ping_cb);
    int (*get_target_result)(struct rs_ping_ctx_cb *ping_cb, struct ping_target_comm_info *target,
        struct ping_result_info *result);
    void (*ping_free_target_node)(struct rs_ping_ctx_cb *ping_cb, struct rs_ping_target_info *target_info);
    void (*deinit_ping_cb)(unsigned int phy_id, struct rs_ping_ctx_cb *ping_cb);
};

struct rs_ping_pong_dfx {
    void (*add_target_success)(struct ping_target_info *target, struct rs_ping_target_info *target_info);
    void (*init_ping_cb_success)(unsigned int phy_id, struct ping_init_attr *attr, unsigned int dev_index);
    void (*ping_cannot_find_target_node)(unsigned int i, int ret, struct ping_target_comm_info target,
        unsigned int phy_id);
};

uint32_t rs_ping_get_trip_time(struct rs_ping_timestamp *timestamp);

#endif // RS_PING_INNER_H

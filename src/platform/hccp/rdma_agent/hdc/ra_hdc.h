/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef RA_HDC_H
#define RA_HDC_H

#include <pthread.h>
#include <sys/types.h>
#include "ascend_hal.h"
#include "hccp.h"
#include "hccp_common.h"
#include "ra.h"
#include "ra_async.h"
#include "ra_rs_comm.h"

struct socket_hdc_info {
    unsigned int phy_id;
    int fd;
    void *socket_handle;
    uint32_t rsv; // consistent with socket_peer_info
};

struct hdc_info {
    HDC_CLIENT client;
    HDC_SESSION session;
    HDC_SESSION snapshot_session;
    int start_deinit;
    int last_recv_status;
    pthread_mutex_t lock;
    unsigned int restore_flag;
};

struct hdc_async_info {
    pid_t host_tgid;
    HDC_SESSION session;
    HDC_SESSION snapshot_session;
    pthread_t tid;
    unsigned int connect_status; // hdc session connect status
    unsigned int thread_status; // recv thread status

    int last_recv_status;
    pthread_mutex_t send_mutex;
    pthread_mutex_t recv_mutex;

    unsigned int req_id;
    pthread_mutex_t req_mutex;
    struct ra_list_head req_list;
    pthread_mutex_t rsp_mutex;
    struct ra_list_head rsp_list;
    unsigned int restore_flag;
};

struct msg_head {
    unsigned int opcode;
    int ret;
    union {
        unsigned int rsvd;
        unsigned int async_req_id;
    };
    unsigned int msg_data_len;
    pid_t host_tgid;
    char pid_sign[PROCESS_RA_SIGN_LENGTH];
};

enum ra_hdc_recv_mode {
    RA_HDC_WAIT_FOREVER,
    RA_HDC_NOWAIT,
    RA_HDC_WAIT_TIMEOUT,
};

#define RA_RSVD_NUM_2 2
#define RA_RSVD_NUM_3 3
#define RA_RSVD_NUM_4 4
#define RA_RSVD_NUM_5 5
#define RA_RSVD_NUM_6 6
#define RA_RSVD_NUM_8 8
#define RA_RSVD_NUM_16 16
#define RA_RSVD_NUM_33 33
#define RA_RSVD_NUM_50 50
#define RA_RSVD_NUM_53 53
#define RA_RSVD_NUM_61 61
#define RA_RSVD_NUM_63 63
#define RA_RSVD_NUM_64 64
#define RA_RSVD_NUM_801 801
#define MAX_HDC_MSG_DATA (4096 - 16)

union op_set_pid_data {
    struct {
        unsigned int phy_id;
        pid_t pid;
        unsigned int rsvd[RA_RSVD_NUM_2];
        char pid_sign[PROCESS_RA_SIGN_LENGTH];
        char resv[PROCESS_RA_RESV_LENGTH];
    } tx_data;

    struct {
        unsigned int rsvd[RA_RSVD_NUM_4];
    } rx_data;
};

union op_get_cqe_err_info_data {
    struct {
    } tx_data;

    struct {
        struct cqe_err_info info;
    } rx_data;
};

union op_get_tls_enable_data {
    struct {
        unsigned int phy_id;
        unsigned int rsvd[RA_RSVD_NUM_4];
    } tx_data;
    struct {
        bool tls_enable;
        unsigned int rsvd[RA_RSVD_NUM_4];
    } rx_data;
};

union op_get_sec_random_data {
    struct {
        unsigned int rsvd;
    } tx_data;
    struct {
        unsigned int value;
    } rx_data;
};

union op_hdc_close_data {
    struct {
        unsigned int phy_id;
        char resv[PROCESS_RA_RESV_LENGTH];
    } tx_data;

    struct {
        unsigned int rsvd[RA_RSVD_NUM_4];
    } rx_data;
};

union op_ifnum_data {
    struct {
        unsigned int phy_id;
        unsigned int num; /* resv bit 31 for is_all */
    } tx_data;

    struct {
        unsigned int num;
    } rx_data;
};

union op_get_version_data {
    struct {
        unsigned int opcode;
    } tx_data;

    struct {
        unsigned int version;
    } rx_data;
};

struct hdc_ops {
    DLLEXPORT hdcError_t (*get_capacity)(struct drvHdcCapacity *capacity);
    DLLEXPORT hdcError_t (*client_create)(HDC_CLIENT *client, int maxSessionNum, int serviceType, int flag);
    DLLEXPORT hdcError_t (*client_destroy)(HDC_CLIENT client);
    DLLEXPORT hdcError_t (*session_connect)(int peer_node, int peer_logicid, HDC_CLIENT client, HDC_SESSION *session);
    DLLEXPORT hdcError_t (*session_connect_ex)(int peer_node, int peer_devid, int peer_pid, HDC_CLIENT client,
        HDC_SESSION *pSession);
    DLLEXPORT hdcError_t (*server_create)(int chip_id, int serviceType, HDC_SERVER *server);
    DLLEXPORT hdcError_t (*server_destroy)(HDC_SERVER server);
    DLLEXPORT hdcError_t (*session_accept)(HDC_SERVER server, HDC_SESSION *session);
    DLLEXPORT hdcError_t (*session_close)(HDC_SESSION session);
    DLLEXPORT hdcError_t (*alloc_msg)(HDC_SESSION session, struct drvHdcMsg **ppMsg, int count);
    DLLEXPORT hdcError_t (*free_msg)(struct drvHdcMsg *msg);
    DLLEXPORT hdcError_t (*reuse_msg)(struct drvHdcMsg *msg);
    DLLEXPORT hdcError_t (*add_msg_buffer)(struct drvHdcMsg *msg, char *pBuf, int len);
    DLLEXPORT hdcError_t (*get_msg_buffer)(struct drvHdcMsg *msg, int index, char **pBuf, int *pLen);
    DLLEXPORT hdcError_t (*recv)(HDC_SESSION session, struct drvHdcMsg *msg, int bufLen, unsigned long long flag,
                          int *recvBufCount, unsigned int timeout);
    DLLEXPORT hdcError_t (*send)(HDC_SESSION session, struct drvHdcMsg *msg, unsigned long long flag,
                          unsigned int timeout);
    DLLEXPORT hdcError_t (*set_session_reference)(HDC_SESSION session) ;
};

#define RA_PTHREAD_MUTEX_LOCK(mutex) do { \
    int ret_lock = pthread_mutex_lock(mutex); \
    if (ret_lock) { \
        hccp_warn("pthread_mutex_lock unsuccessful, ret[%d]", ret_lock); \
    } \
} while (0)

#define RA_PTHREAD_MUTEX_UNLOCK(mutex) do { \
    int ret_ulock = pthread_mutex_unlock(mutex); \
    if (ret_ulock) { \
        hccp_warn("pthread_mutex_unlock unsuccessful, ret[%d]", ret_ulock); \
    } \
} while (0)

static inline bool ra_hdc_is_broken(int last_recv_status)
{
    return last_recv_status == DRV_ERROR_SOCKET_CLOSE;
}

int ra_hdc_init(struct ra_init_config *cfg, struct process_ra_sign p_ra_sign);
int ra_hdc_get_tls_enable(unsigned int phy_id, bool *tls_enable);
int ra_hdc_deinit(struct ra_init_config *cfg);
int ra_hdc_get_interface_version(unsigned int phy_id, unsigned int interface_opcode, unsigned int *interface_version);
void ra_hdc_get_all_opcode_version(unsigned int phy_id);
int ra_hdc_get_cqe_err_info(unsigned int phy_id, struct cqe_err_info *info);
int ra_hdc_process_msg(unsigned int opcode, unsigned int phy_id, char *data, unsigned int data_size);
int ra_hdc_init_session(int peer_node, int peer_devid, unsigned int phy_id, int hdc_type, HDC_SESSION *session);
void ra_hdc_deinit_session(HDC_SESSION *session);
int ra_hdc_set_session_reference(HDC_SESSION *session);
void msg_head_build_up(struct msg_head *p_send_rcv_head, unsigned int opcode, unsigned int req_id,
    unsigned int msg_data_len, pid_t host_tgid);
int hdc_async_send_pkt(struct hdc_async_info *async_info, unsigned int phy_id, void *send_buf, unsigned int send_len,
    struct ra_request_handle *req_handle);
int hdc_async_recv_pkt(struct hdc_async_info *async_info, unsigned int phy_id, void *recv_buf, unsigned int *recv_len);
int ra_hdc_save_snapshot(unsigned int phy_id, enum save_snapshot_action action);
int ra_hdc_restore_snapshot(unsigned int phy_id);
int ra_hdc_get_sec_random(unsigned int phy_id, unsigned int *value);
#endif // RA_HDC_H

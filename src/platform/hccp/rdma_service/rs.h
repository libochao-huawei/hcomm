/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef RS_H
#define RS_H

#include <unistd.h>
#include <stdbool.h>
#include <sys/time.h>
#include <infiniband/verbs.h>
#include <infiniband/driver.h>
#include "hccp_common.h"
#include "user_log.h"
#include "ra_rs_comm.h"

#define PROCESS_RS_SIGN_LENGTH 49
#define PROCESS_RS_RESV_LENGTH 4
#define EXP_DEVNUM             2

struct rs_mr_reg_info {
    unsigned int phy_id;
    char *addr;
    unsigned long long len;
    int access;
};

struct process_rs_sign {
    int tgid;
    char sign[PROCESS_RS_SIGN_LENGTH];
    char resv[PROCESS_RS_RESV_LENGTH];
};

struct rs_qp_norm {
    int flag;
    int qp_mode;
    int is_exp;
    int is_ext;
    int mem_align; // 0,1:4KB, 2:2MB
};

struct rs_qp_status_info {
    int status;
    unsigned int udp_sport;
};

struct rs_wrlist_base_info {
    unsigned int phy_id;
    unsigned int rdev_index;
    unsigned int qpn;
    unsigned int key_flag;
};

struct rs_linux_version_info {
    int major;
    int minor;
    int patch;
};

#if defined(HNS_ROCE_LLT) || defined(DEFINE_HNS_LLT)
#define STATIC
#else
#define STATIC static
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ++++++++++++++++++++++++++++++RDMA API for RA++++++++++++++++++++++++++++++++++ */
/*
 * rs_open
 * flag: bit0: 0 = RC, 1= UD
 */
#define MS_PER_SECOND_F   1000.0
#define US_PER_MS_F   1000.0
#define MS_PER_SECOND_I   1000
#define RS_EXPECT_TIME_MAX 200.0 // ms

#define RS_GID_SEQ_NUM             4
#define RS_GID_SEQ_ZERO            0
#define RS_GID_SEQ_ONE             1
#define RS_GID_SEQ_TWO             2
#define RS_GID_SEQ_THREE           3

#define RS_ATTRI_VISI_DEF __attribute__ ((visibility ("default")))

RS_ATTRI_VISI_DEF int rs_set_tsqp_depth(unsigned int phy_id, unsigned int rdev_index, unsigned int temp_depth,
    unsigned int *qp_num);
RS_ATTRI_VISI_DEF int rs_get_tsqp_depth(unsigned int phy_id, unsigned int rdev_index, unsigned int *temp_depth,
    unsigned int *qp_num);
RS_ATTRI_VISI_DEF int rs_qp_create(unsigned int phy_id, unsigned int rdev_index, struct rs_qp_norm qp_norm,
    struct rs_qp_resp *qp_resp);
RS_ATTRI_VISI_DEF int rs_qp_create_with_attrs(unsigned int phy_id, unsigned int rdev_index,
    struct rs_qp_norm_with_attrs *qp_norm, struct rs_qp_resp_with_attrs *qp_resp);
RS_ATTRI_VISI_DEF int rs_qp_destroy(unsigned int phy_id, unsigned int rdev_index, unsigned int qpn);
RS_ATTRI_VISI_DEF int rs_typical_qp_modify(unsigned int phy_id, unsigned int rdev_index,
    struct typical_qp local_qp_info, struct typical_qp remote_qp_info, unsigned int *udp_sport);
RS_ATTRI_VISI_DEF int rs_qp_batch_modify(unsigned int phy_id, unsigned int rdev_index,
    int status, int qpn[], int qpn_num);
RS_ATTRI_VISI_DEF int rs_qp_connect_async(unsigned int phy_id, unsigned int rdev_index, unsigned int qpn, int fd);

enum rs_qp_status {
    RS_QP_STATUS_DISCONNECT = 0,
    RS_QP_STATUS_CONNECTED = 1,
    RS_QP_STATUS_TIMEOUT = 2,
    RS_QP_STATUS_CONNECTING = 3,
    RS_QP_STATUS_REM_FD_CLOSE = 4,
    RS_QP_STATUS_PAUSE = 5,
};

#define RS_IS_EXP       0
#define RS_NOT_EXP      1

enum rs_access_flags {
    RS_ACCESS_LOCAL_WRITE  = 1,
    RS_ACCESS_REMOTE_WRITE = (1 << 1),
    RS_ACCESS_REMOTE_READ  = (1 << 2UL),
    RS_ACCESS_REDUCE       = (1 << 8),
};

struct rs_init_config {
    unsigned int chip_id;
    unsigned int hccp_mode;
    unsigned int white_list_status;
};

struct rs_qp_conn_para {
    unsigned int phy_id;
    unsigned int rdev_index;
    uint32_t qpn;
};

struct rs_backup_info {
    bool backup_flag;
    struct rdev rdev_info;
};

#define RS_HEARTBEAT_TIME (1000.0 * 60) // ms
#define PTHREAD_NAME_LEN 32

struct rs_pthread_info {
    char pthread_name[PTHREAD_NAME_LEN];
    struct timeval last_check_time;
};

RS_ATTRI_VISI_DEF int rs_mr_reg(unsigned int phy_id, unsigned int rdev_index, unsigned int qpn,
    struct rdma_mr_reg_info *mr_reg_info);
RS_ATTRI_VISI_DEF int rs_mr_dereg(unsigned int phy_id, unsigned int rdev_index, unsigned int qpn, char *addr);

RS_ATTRI_VISI_DEF int rs_register_mr(unsigned int phy_id, unsigned int rdev_index,
    struct rdma_mr_reg_info *mr_reg_info, void **mr_handle);
RS_ATTRI_VISI_DEF int rs_typical_register_mr(unsigned int phy_id, unsigned int rdev_index,
    struct rdma_mr_reg_info *mr_reg_info, void **mr_handle);
RS_ATTRI_VISI_DEF int rs_remap_mr(unsigned int phy_id, unsigned int rdev_index, struct mem_remap_info mem_list[],
    unsigned int mem_num);
RS_ATTRI_VISI_DEF int rs_typical_deregister_mr(unsigned int phy_id, unsigned int dev_index, unsigned long long addr);
RS_ATTRI_VISI_DEF int rs_deregister_mr(void *mr_handle);

enum rs_send_flags {
    RS_SEND_FENCE  = 1 << 0,
    RS_SEND_SIGNALED = 1 << 1,
    RS_SEND_SOLICITED = 1 << 2,
    RS_SEND_INLINE  = 1 << 3,
};
RS_ATTRI_VISI_DEF int rs_send_wr(unsigned int phy_id, unsigned int rdev_index, uint32_t qpn, struct send_wr *wr,
    struct send_wr_rsp *wr_rsp);
RS_ATTRI_VISI_DEF int rs_send_wrlist(struct rs_wrlist_base_info base_info, struct wr_info *wr_list,
    unsigned int send_num, struct send_wr_rsp *wr_rsp, unsigned int *complete_num);

RS_ATTRI_VISI_DEF int rs_recv_wrlist(struct rs_wrlist_base_info base_info, struct recv_wrlist_data *wr,
    unsigned int recv_num, unsigned int *complete_num);

RS_ATTRI_VISI_DEF int rs_get_notify_mr_info(unsigned int phy_id, unsigned int rdev_index, struct mr_info *info);
RS_ATTRI_VISI_DEF int rs_set_host_pid(uint32_t phy_id, pid_t host_pid, const char *pid_sign);

RS_ATTRI_VISI_DEF int rs_init(struct rs_init_config *cfg);
RS_ATTRI_VISI_DEF int rs_get_tls_enable(unsigned int phy_id, bool *tls_enable);
RS_ATTRI_VISI_DEF int rs_bind_hostpid(unsigned int chip_id, pid_t pid);
RS_ATTRI_VISI_DEF int rs_deinit(struct rs_init_config *cfg);

RS_ATTRI_VISI_DEF int rs_socket_init(const unsigned int *vnic_ip, unsigned int num);
RS_ATTRI_VISI_DEF int rs_socket_deinit(struct rdev rdev_info);

RS_ATTRI_VISI_DEF int rs_rdev_init(struct rdev rdev_info, unsigned int notify_type, unsigned int *rdev_index);
RS_ATTRI_VISI_DEF int rs_rdev_init_with_backup(struct rdev rdev_info, struct rdev backup_rdev_info,
    unsigned int notify_type, unsigned int *rdev_index);
RS_ATTRI_VISI_DEF int rs_rdev_get_port_status(unsigned int phy_id, unsigned int rdev_index, enum port_status *status);
RS_ATTRI_VISI_DEF int rs_rdev_deinit(unsigned int phy_id, unsigned int notify_type, unsigned int rdev_index);

/* ++++++++++++++++++++++++++++++Epoll API start++++++++++++++++++++++++++++++++++ */
RS_ATTRI_VISI_DEF int rs_epoll_ctl_add(const void *fd_handle, enum RaEpollEvent event);
RS_ATTRI_VISI_DEF int rs_epoll_ctl_mod(const void *fd_handle, enum RaEpollEvent event);
RS_ATTRI_VISI_DEF int rs_epoll_ctl_del(int fd);
RS_ATTRI_VISI_DEF void rs_set_tcp_recv_callback(const void *callback);
RS_ATTRI_VISI_DEF int rs_create_event_handle(int *event_handle);
RS_ATTRI_VISI_DEF int rs_ctl_event_handle(int event_handle, const void *fd_handle, int opcode,
    enum RaEpollEvent event);
RS_ATTRI_VISI_DEF int rs_wait_event_handle(int event_handle, struct socket_event_info *event_infos,
    int timeout, unsigned int maxevents, unsigned int *events_num);
RS_ATTRI_VISI_DEF int rs_destroy_event_handle(int *event_handle);
/* ++++++++++++++++++++++++++++++Epoll API end++++++++++++++++++++++++++++++++++ */

/* ++++++++++++++++++++++++++++++Socket API++++++++++++++++++++++++++++++++++ */
#define RS_SOCK_PORT_DEF 16666
RS_ATTRI_VISI_DEF int rs_socket_listen_start(struct socket_listen_info conn[], uint32_t num);
RS_ATTRI_VISI_DEF int rs_socket_listen_stop(struct socket_listen_info conn[], uint32_t num);

RS_ATTRI_VISI_DEF int rs_socket_batch_connect(struct socket_connect_info conn[], uint32_t num);
RS_ATTRI_VISI_DEF int rs_socket_batch_abort(struct socket_connect_info conn[], uint32_t num);

RS_ATTRI_VISI_DEF int rs_socket_get_client_socket_err_info(struct socket_connect_info conn[],
    struct socket_err_info err[], unsigned int num);
RS_ATTRI_VISI_DEF int rs_socket_get_server_socket_err_info(struct socket_listen_info conn[],
    struct server_socket_err_info err[], unsigned int num);

RS_ATTRI_VISI_DEF void rs_get_cur_time(struct timeval *time);
RS_ATTRI_VISI_DEF void hccp_time_interval(struct timeval *end_time, struct timeval *start_time, float *msec);
RS_ATTRI_VISI_DEF int rs_socket_white_list_switch(unsigned int phy_id, unsigned int enable);
RS_ATTRI_VISI_DEF int rs_socket_white_list_add(struct rdev rdev_info, struct socket_wlist_info_t white_list[],
    unsigned int num);
RS_ATTRI_VISI_DEF int rs_socket_white_list_del(struct rdev rdev_info, struct socket_wlist_info_t white_list[],
    unsigned int num);
RS_ATTRI_VISI_DEF int rs_socket_accept_credit_add(struct socket_listen_info conn[], uint32_t num,
    unsigned int credit_limit);
RS_ATTRI_VISI_DEF int rs_get_ifnum(unsigned int phy_id, bool is_all, unsigned int *num);
RS_ATTRI_VISI_DEF int rs_peer_get_ifnum(unsigned int phy_id, unsigned int *num);
RS_ATTRI_VISI_DEF int rs_get_ifaddrs(struct ifaddr_info ifaddr_infos[], unsigned int *num, unsigned int phy_id);
RS_ATTRI_VISI_DEF int rs_get_ifaddrs_v2(struct interface_info interface_infos[], unsigned int *num,
    unsigned int phy_id, bool is_all);
RS_ATTRI_VISI_DEF int rs_peer_get_ifaddrs(struct interface_info interface_infos[], unsigned int *num,
    unsigned int phy_id);
RS_ATTRI_VISI_DEF int rs_get_vnic_ip(unsigned int phy_id, unsigned int *vnic_ip);
RS_ATTRI_VISI_DEF int rs_get_interface_version(unsigned int opcode, unsigned int *version);
RS_ATTRI_VISI_DEF int rs_get_vnic_ip_infos(unsigned int phy_id, enum id_type type, unsigned int ids[], unsigned int num,
    struct ip_info infos[]);
RS_ATTRI_VISI_DEF int rs_socket_set_scope_id(unsigned int dev_id, int scope_id);
struct rs_socket_close_info_t {
    int fd;
};
RS_ATTRI_VISI_DEF int rs_socket_batch_close(int disuse_linger, struct rs_socket_close_info_t conn[], uint32_t num);

enum rs_conn_role {
    RS_CONN_ROLE_SERVER = 0,
    RS_CONN_ROLE_CLIENT = 1,
};
enum rs_socket_status {
    RS_SOCK_STATUS_NA = 0,
    RS_SOCK_STATUS_OK = 1,
    RS_SOCK_STATUS_TIMEOUT = 2,
    RS_SOCK_STATUS_ING = 3,
};

RS_ATTRI_VISI_DEF int rs_get_sockets(uint32_t role, struct socket_fd_data conn[], uint32_t num);
RS_ATTRI_VISI_DEF int rs_get_ssl_enable(uint32_t *ssl_enable);
RS_ATTRI_VISI_DEF int rs_socket_send(int fd, const void *data, uint64_t size);
RS_ATTRI_VISI_DEF int rs_peer_socket_send(uint32_t ssl_enable, int fd, const void *data, uint64_t size);
RS_ATTRI_VISI_DEF int rs_socket_recv(int fd, void *data, uint64_t size);
RS_ATTRI_VISI_DEF int rs_peer_socket_recv(uint32_t ssl_enable, int fd, void *data, uint64_t size);
RS_ATTRI_VISI_DEF int rs_notify_cfg_set(unsigned int phy_id, unsigned long long va, unsigned long long size);
RS_ATTRI_VISI_DEF int rs_notify_cfg_get(unsigned int phy_id, unsigned long long *va, unsigned long long *size);
RS_ATTRI_VISI_DEF void rs_heartbeat_alive_print(struct rs_pthread_info *pthread_info);

RS_ATTRI_VISI_DEF int rs_get_qp_context(unsigned int phy_id, unsigned int rdev_index, unsigned int qpn, void** qp,
                                        void** send_cq, void** recv_cq);
RS_ATTRI_VISI_DEF int rs_get_qp_status(unsigned int phy_id, unsigned int rdev_index, unsigned int qpn,
    struct rs_qp_status_info *qp_info);

int rsGetLocalDevIDByHostDevID(unsigned int phy_id, unsigned int *chip_id);
int rsGetDevIDByLocalDevID(unsigned int chip_id, unsigned int *phy_id);

RS_ATTRI_VISI_DEF int rs_cq_create(unsigned int phy_id, unsigned int rdev_index, struct cq_attr *attr);
RS_ATTRI_VISI_DEF int rs_cq_destroy(unsigned int phy_id, unsigned int rdev_index, struct cq_attr *attr);
RS_ATTRI_VISI_DEF int rs_normal_qp_create(unsigned int phy_id, unsigned int rdev_index,
    struct ibv_qp_init_attr *qp_init_attr, struct rs_qp_resp *qp_resp, void **qp);
RS_ATTRI_VISI_DEF int rs_normal_qp_destroy(unsigned int phy_id, unsigned int rdev_index, unsigned int qpn);
RS_ATTRI_VISI_DEF int rs_set_qp_attr_qos(unsigned int phy_id, unsigned int rdev_index, unsigned int qpn,
    struct qos_attr *attr);
RS_ATTRI_VISI_DEF int rs_set_qp_attr_timeout(unsigned int phy_id, unsigned int rdev_index, unsigned int qpn,
    unsigned int *timeout);
RS_ATTRI_VISI_DEF int rs_set_qp_attr_retry_cnt(unsigned int phy_id, unsigned int rdev_index, unsigned int qpn,
    unsigned int *retry_cnt);
RS_ATTRI_VISI_DEF int rs_create_comp_channel(unsigned int phy_id, unsigned int rdev_index, void** comp_channel);
RS_ATTRI_VISI_DEF int rs_destroy_comp_channel(void* comp_channel);
RS_ATTRI_VISI_DEF int rs_get_cqe_err_info(struct cqe_err_info *info);
RS_ATTRI_VISI_DEF int rs_create_srq(unsigned int phy_id, unsigned int rdev_index, struct srq_attr *attr);
RS_ATTRI_VISI_DEF int rs_destroy_srq(unsigned int phy_id, unsigned int rdev_index, struct srq_attr *attr);
RS_ATTRI_VISI_DEF int rs_get_lite_support(unsigned int phy_id, unsigned int rdev_index, int *support_lite);
RS_ATTRI_VISI_DEF int rs_get_lite_rdev_cap(
    unsigned int phy_id, unsigned int rdev_index, struct lite_rdev_cap_resp *resp);
RS_ATTRI_VISI_DEF int rs_get_lite_qp_cq_attr(
    unsigned int phy_id, unsigned int rdev_index, unsigned int qpn, struct lite_qp_cq_attr_resp *resp);
RS_ATTRI_VISI_DEF int rs_get_lite_connected_info(
    unsigned int phy_id, unsigned int rdev_index, unsigned int qpn, struct lite_connected_info_resp *resp);
RS_ATTRI_VISI_DEF int rs_get_lite_mem_attr(
    unsigned int phy_id, unsigned int rdev_index, unsigned int qpn, struct lite_mem_attr_resp *resp);
RS_ATTRI_VISI_DEF void rs_set_ctx(unsigned int phy_id);
RS_ATTRI_VISI_DEF int rs_get_cqe_err_info_num(unsigned int phy_id, unsigned int rdev_idx, unsigned int *num);
RS_ATTRI_VISI_DEF int rs_get_cqe_err_info_list(unsigned int phy_id, unsigned int rdev_idx, struct cqe_err_info *info,
    unsigned int *num);
RS_ATTRI_VISI_DEF int rs_drv_get_random_num(int *rand_num);
RS_ATTRI_VISI_DEF int rs_get_sec_random(unsigned int *value);
#ifdef __cplusplus
}
#endif
#endif // RS_H

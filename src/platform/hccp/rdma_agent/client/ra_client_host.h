/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef RA_CLIENT_HOST_H
#define RA_CLIENT_HOST_H

#include <errno.h>
#include <stdbool.h>
#include <infiniband/verbs.h>
#include "ra_rs_comm.h"
#include "ra.h"

struct ra_socket_ops {
    int (*ra_socket_init)(struct rdev rdev_info);
    int (*ra_socket_deinit)(struct rdev rdev_info);
    int (*ra_socket_batch_connect)(unsigned int phy_id, struct socket_connect_info_t conn[], unsigned int num);
    int (*ra_socket_batch_close)(unsigned int phy_id, struct socket_close_info_t conn[], unsigned int num);
    int (*ra_socket_batch_abort)(unsigned int phy_id, struct socket_connect_info_t conn[], unsigned int num);
    int (*ra_socket_listen_start)(unsigned int phy_id, struct socket_listen_info_t conn[], unsigned int num);
    int (*ra_socket_listen_stop)(unsigned int phy_id, struct socket_listen_info_t conn[], unsigned int num);
    int (*ra_get_sockets)(unsigned int phy_id, unsigned int role, struct socket_info_t conn[], unsigned int num);
    int (*ra_socket_send)(unsigned int phy_id, const void *handle,
            const void *data, unsigned long long size);
    int (*ra_socket_recv)(unsigned int phy_id, const void *handle, void *data, unsigned long long size);
    int (*ra_get_client_socket_err_info)(unsigned int phy_id, struct socket_connect_info_t conn[],
        struct socket_err_info err[], unsigned int num);
    int (*ra_get_server_socket_err_info)(unsigned int phy_id, struct socket_listen_info_t conn[],
        struct server_socket_err_info err[], unsigned int num);
    int (*ra_socket_set_white_list_status)(unsigned int enable);
    int (*ra_socket_get_white_list_status)(unsigned int *enable);
    int (*ra_socket_white_list_add)(struct rdev rdev_info,
        struct socket_wlist_info_t white_list[], unsigned int num);
    int (*ra_socket_white_list_del)(struct rdev rdev_info,
        struct socket_wlist_info_t white_list[], unsigned int num);
    int (*ra_socket_accept_credit_add)(unsigned int phy_id, struct socket_listen_info_t conn[], unsigned int num,
        unsigned int credit_limit);
};

struct ra_rdma_ops {
    int (*ra_rdev_init)(
        struct ra_rdma_handle *rdma_handle, unsigned int notify_type, struct rdev rdev_info, unsigned int *rdev_index);
    int (*ra_rdev_get_port_status)(struct ra_rdma_handle *rdma_handle, enum port_status *status);
    int (*ra_rdev_deinit)(struct ra_rdma_handle *rdma_handle, unsigned int notify_type);
    int (*ra_set_tsqp_depth) (struct ra_rdma_handle *rdma_handle, unsigned int temp_depth, unsigned int *qp_num);
    int (*ra_get_tsqp_depth) (struct ra_rdma_handle *rdma_handle, unsigned int *temp_depth, unsigned int *qp_num);
    int (*ra_qp_create)(struct ra_rdma_handle *rdma_handle, int flag, int qp_mode, void **qp_handle);
    int (*ra_qp_create_with_attrs)(struct ra_rdma_handle *rdma_handle, struct qp_ext_attrs *ext_attrs,
        void **qp_handle);
    int (*ra_ai_qp_create)(struct ra_rdma_handle *rdma_handle, struct qp_ext_attrs *ext_attrs,
        struct ai_qp_info *info, void **qp_handle);
    int (*ra_ai_qp_create_with_attrs)(struct ra_rdma_handle *rdma_handle, struct qp_ext_attrs *ext_attrs,
        struct ai_qp_info *info, void **qp_handle);
    int (*ra_typical_qp_create)(struct ra_rdma_handle *rdma_handle, int flag, int qp_mode, struct typical_qp *qp_info,
        void **qp_handle);
    int (*ra_loopback_qp_create)(struct ra_rdma_handle *rdev_handle, struct loopback_qp_pair *qp_pair, void **qp_handle);
    int (*ra_qp_destroy)(struct ra_qp_handle *handle);
    int (*ra_typical_qp_modify)(struct ra_qp_handle *handle, struct typical_qp *local_qp_info,
        struct typical_qp *remote_qp_info);
    int (*ra_qp_batch_modify)(struct ra_rdma_handle *handle, void *qp_hdc[],
        unsigned int num, int expect_status);
    int (*ra_qp_connect_async)(struct ra_qp_handle *handle, const void *sock_handle);
    int (*ra_get_qp_status)(struct ra_qp_handle *handle, int *status);
    int (*ra_mr_reg)(struct ra_qp_handle *handle, struct mr_info *info);
    int (*ra_mr_dereg)(struct ra_qp_handle *handle, struct mr_info *info);
    int (*ra_register_mr)(struct ra_rdma_handle *handle, struct mr_info *info, void **mr_handle);
    int (*ra_remap_mr)(struct ra_rdma_handle *handle, struct mem_remap_info info[], unsigned int num);
    int (*ra_deregister_mr)(struct ra_rdma_handle *handle, void *mr_handle);
    int (*ra_send_wr)(struct ra_qp_handle *handle, struct send_wr *wr, struct send_wr_rsp *wr_rsp);
    int (*ra_send_wr_v2)(struct ra_qp_handle *handle, struct send_wr_v2 *wr, struct send_wr_rsp *wr_rsp);
    int (*ra_typical_send_wr)(struct ra_qp_handle *handle, struct send_wr *wr, struct send_wr_rsp *wr_rsp);
    int (*ra_send_wrlist)(struct ra_qp_handle *handle, struct send_wrlist_data wr[], struct send_wr_rsp op_rsp[],
        struct wrlist_send_complete_num wrlist_num);
    int (*ra_send_wrlist_ext)(struct ra_qp_handle *handle, struct send_wrlist_data_ext wr[],
        struct send_wr_rsp op_rsp[], struct wrlist_send_complete_num wrlist_num);
    int (*ra_send_normal_wrlist)(struct ra_qp_handle *handle, struct wr_info wr[], struct send_wr_rsp op_rsp[],
        struct wrlist_send_complete_num wrlist_num);
    int (*ra_get_notify_base_addr)(struct ra_rdma_handle *handle,
            unsigned long long *va, unsigned long long *size);
    int (*ra_get_notify_mr_info)(struct ra_rdma_handle *handle, struct mr_info *info);
    int (*ra_recv_wrlist)(struct ra_qp_handle *handle, struct recv_wrlist_data *wr, unsigned int recv_num,
        unsigned int *complete_num);
    int (*ra_poll_cq)(struct ra_qp_handle *handle, bool is_send_cq, unsigned int num_entries, void *wc);
    int (*ra_get_qp_context)(struct ra_qp_handle *handle, void** qp, void** send_cq, void** recv_cq);
    int (*ra_normal_qp_create)(struct ra_rdma_handle *rdma_handle, struct ibv_qp_init_attr *qp_init_attr,
        void **qp_handle, void **qp);
    int (*ra_normal_qp_destroy)(struct ra_qp_handle *handle);
    int (*ra_cq_create)(struct ra_rdma_handle *rdma_handle, struct cq_attr *attr);
    int (*ra_cq_destroy)(struct ra_rdma_handle *rdma_handle, struct cq_attr *attr);
    int (*ra_set_qp_attr_qos)(struct ra_qp_handle *handle, struct qos_attr *info);
    int (*ra_set_qp_attr_timeout)(struct ra_qp_handle *handle, unsigned int *timeout);
    int (*ra_set_qp_attr_retry_cnt)(struct ra_qp_handle *handle, unsigned int *retry_cnt);
    int (*ra_create_comp_channel)(struct ra_rdma_handle *handle, void **comp_channel);
    int (*ra_destroy_comp_channel)(void *comp_channel);
    int (*ra_create_srq)(struct ra_rdma_handle *handle, struct srq_attr *attr);
    int (*ra_destroy_srq)(struct ra_rdma_handle *handle, struct srq_attr *attr);
};

struct errcode_info {
    int orig_errcode;
    int err_type;
    int module_errcode;
};

#define RA_VNIC_MAX 128
#define RA_NOTIFY_TYPE_NUM          2
#define RA_MIN_TEMPTH_DEPTH         8
#define RA_MAX_TEMPTH_DEPTH         4096

#define DEFAULT_ERRCODE_TYPE    3
#define DEFAULT_MODULE_ERRCODE  7
#define HCCP_MODULE_ID          28
#define ACL_ERRCODE_DIGIT       100000

#define CONVER_ERROR_CODE(module, err_type, module_errcode) \
    (err_type * 100000 + HCCP_MODULE_ID * 1000 + module * 100 + module_errcode)  /* Combine a 6-digit ACL error code. */

int ra_rdev_init_check(int mode, struct rdev rdev_info, char local_ip[], unsigned int num, void *rdma_handle);
int ra_inet_pton(int family, union hccp_ip_addr ip, char net_addr[], unsigned int len);
#endif // RA_CLIENT_HOST_H

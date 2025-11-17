/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef DL_IBVERBS_FUNCTION_H
#define DL_IBVERBS_FUNCTION_H

#include <ccan/list.h>
#include <infiniband/driver.h>
#include <infiniband/verbs.h>
#include "hccp_dl.h"
#include "hns_roce_u_cmd.h"
#include "verbs_exp.h"
#include "rdma_lite_common.h"

enum so_type {
    SO_TYPE_EXP,
    SO_TYPE_EXT,
    SO_TYPE_INVALID,
};

struct rs_ibverbs_ops {
    void (*rs_ibv_free_device_list)(struct ibv_device **list);
    void (*rs_ibv_ack_cq_events)(struct ibv_cq *cq, unsigned int nevents);
    const char *(*rs_ibv_get_device_name)(struct ibv_device *device);
    const char *(*rs_ibv_wc_status_str)(enum ibv_wc_status status);
    int (*rs_ibv_query_gid_type)(struct ibv_context *context, uint8_t port_num, unsigned int index,
        enum ibv_gid_type_sysfs *type);
    int (*rs_ibv_dereg_mr)(struct ibv_mr *mr);
    int (*rs_ibv_query_qp)(struct ibv_qp *qp, struct ibv_qp_attr *attr, int attr_mask,
        struct ibv_qp_init_attr *init_attr);
    int (*rs_ibv_destroy_qp)(struct ibv_qp *qp);
    int (*rs_ibv_get_cq_event)(struct ibv_comp_channel *channel, struct ibv_cq **cq, void **cq_context);
    int (*rs_ibv_destroy_cq)(struct ibv_cq *cq);
    int (*rs_ibv_modify_qp)(struct ibv_qp *qp, struct ibv_qp_attr *attr, int attr_mask);
    int (*rs_ibv_query_port)(struct ibv_context *context, uint8_t port_num, struct ibv_port_attr *port_attr);
    int (*rs_ibv_query_gid)(struct ibv_context *context, uint8_t port_num, int index, union ibv_gid *gid);
    int (*rs_ibv_close_device)(struct ibv_context *context);
    int (*rs_ibv_dealloc_pd)(struct ibv_pd *pd);
    int (*rs_ibv_destroy_comp_channel)(struct ibv_comp_channel *channel);
    struct ibv_context *(*rs_ibv_open_device)(struct ibv_device *device);
    struct ibv_pd *(*rs_ibv_alloc_pd)(struct ibv_context *context);
    struct ibv_device **(*rs_ibv_get_device_list)(int *num_devices);
    struct ibv_cq *(*rs_ibv_create_cq)(struct ibv_context *context, int cqe, void *cq_context,
        struct ibv_comp_channel *channel, int comp_vector);
    struct ibv_mr *(*rs_ibv_reg_mr)(struct ibv_pd *pd, void *addr, size_t length, int access);
    struct ibv_qp *(*rs_ibv_create_qp)(struct ibv_pd *pd, struct ibv_qp_init_attr *qp_init_attr);
    struct ibv_comp_channel *(*rs_ibv_create_comp_channel)(struct ibv_context *context);
    struct ibv_srq *(*rs_ibv_create_srq)(struct ibv_pd *pd, struct ibv_srq_init_attr *srq_init_attr);
    int (*rs_ibv_destroy_srq)(struct ibv_srq *);
    struct ibv_ah *(*rs_ibv_create_ah)(struct ibv_pd *pd, struct ibv_ah_attr *attr);
    int (*rs_ibv_destroy_ah)(struct ibv_ah *ah);
};

struct rs_roce_user_ops {
    int (*rs_roce_set_tsqp_depth)(const char *dev_name, unsigned int rdev_index, unsigned int temp_depth,
        unsigned int *qp_num, unsigned int *sq_depth);
    int (*rs_roce_get_tsqp_depth)(const char *dev_name, unsigned int rdev_index, unsigned int *temp_depth,
        unsigned int *qp_num, unsigned int *sq_depth);
    int (*rs_roce_get_roce_dev_data)(const char *dev_name, struct roce_dev_data *rdev_data);
    int (*rs_ibv_exp_query_notify)(struct ibv_context *context, unsigned long long *notify_va,
        unsigned long long *size);
    int (*rs_ibv_exp_post_send)(struct ibv_qp *qp, struct ibv_send_wr *wr, struct ibv_send_wr **bad_wr,
        struct wr_exp_rsp *exp_rsp);
    int (*rs_ibv_ext_post_send)(struct ibv_qp *qp, struct ibv_send_wr *wr, struct ibv_send_wr **bad_wr,
        struct ibv_post_send_ext_attr *ext_attr, struct ibv_post_send_ext_resp *ext_resp);
    struct ibv_mr *(*rs_ibv_exp_reg_mr)(struct ibv_pd *pd, void *addr, size_t length, int access,
        struct roce_process_sign roce_sign);
    struct ibv_qp *(*rs_ibv_exp_create_qp)(struct ibv_pd *pd, struct ibv_exp_qp_init_attr *qp_init_attr,
        struct rdma_lite_device_qp_attr *qp_resp);
    struct ibv_cq *(*rs_ibv_exp_create_cq)(struct ibv_context *context, int cqe, void *cq_context,
        struct ibv_comp_channel *channel, int comp_vector, struct rdma_lite_device_cq_init_attr *attr,
        struct rdma_lite_device_cq_attr *cq_resp);
    int (*rs_ibv_exp_query_device)(struct ibv_context *context, struct dev_cap_info *cap);
    int (*rs_roce_init_mem_pool)(const struct roce_mem_cq_qp_attr *mem_attr,
        struct rdma_lite_device_mem_attr *mem_data, unsigned int dev_id);
    int (*rs_roce_deinit_mem_pool)(unsigned int mem_idx);
    int (*rs_roce_query_qpc)(struct ibv_qp *qp, struct hns_roce_qpc_attr_val *attr_val, unsigned int attr_mask);
    struct ibv_ah *(*rs_ibv_exp_create_ah)(struct ibv_pd *pd, struct ibv_exp_ah_attr *attrx);
    int (*rs_roce_mmap_ai_db_reg)(struct ibv_context *context, unsigned int tgid);
    int (*rs_roce_unmmap_ai_db_reg)(struct ibv_context *context);
    int (*rs_roce_get_cq_data_plane_info)(struct ibv_cq *cq, struct hns_roce_cq_data_plane_info *info);
    int (*rs_roce_get_qp_data_plane_info)(struct ibv_qp *qp, struct hns_roce_qp_data_plane_info *info);
    int (*rs_roce_remap_mr)(struct ibv_mr *mr, struct hns_roce_mr_remap_info info[], unsigned int num);
    unsigned int (*rs_roce_get_api_version)(void);
};

struct ibv_mr *rs_ibv_exp_reg_mr(struct ibv_pd *pd, void *addr, size_t length, int access,
    struct roce_process_sign roce_sign);
int rs_ibv_exp_query_notify(struct ibv_context *context, unsigned long long *notify_va, unsigned long long *size);
int rs_ibv_exp_post_send(struct ibv_qp *qp, struct ibv_send_wr *wr, struct ibv_send_wr **bad_wr,
    struct wr_exp_rsp *exp_rsp);
int rs_ibv_ext_post_send(struct ibv_qp *qp, struct ibv_send_wr *wr, struct ibv_send_wr **bad_wr,
    struct ibv_post_send_ext_attr *ext_attr, struct ibv_post_send_ext_resp *ext_resp);
struct ibv_qp *rs_ibv_exp_create_qp(
    struct ibv_pd *pd, struct ibv_exp_qp_init_attr *qp_init_attr, struct rdma_lite_device_qp_attr *qp_resp);
int rs_roce_set_tsqp_depth(const char *dev_name, unsigned int rdev_index, unsigned int temp_depth,
    unsigned int *qp_num, unsigned int *sq_depth);
int rs_roce_get_tsqp_depth(const char *dev_name, unsigned int rdev_index, unsigned int *temp_depth,
    unsigned int *qp_num, unsigned int *sq_depth);
int rs_roce_get_roce_dev_data(const char *dev_name, struct roce_dev_data *rdev_data);

DL_ATTRI_VISI_DEF void rs_api_deinit(void);
DL_ATTRI_VISI_DEF int rs_api_init(void);
void rs_ibv_free_device_list(struct ibv_device **list);
void rs_ibv_ack_cq_events(struct ibv_cq *cq, unsigned int nevents);
const char *rs_ibv_get_device_name(struct ibv_device *device);
const char *rs_ibv_wc_status_str(enum ibv_wc_status status);
int rs_ibv_query_gid_type(struct ibv_context *context, uint8_t port_num, unsigned int index,
    enum ibv_gid_type_sysfs *type);
int rs_ibv_dereg_mr(struct ibv_mr *mr);
int rs_ibv_post_send(struct ibv_qp *qp, struct ibv_send_wr *wr, struct ibv_send_wr **bad_wr);
int rs_ibv_post_recv(struct ibv_qp *qp, struct ibv_recv_wr *wr, struct ibv_recv_wr **bad_wr);
int rs_ibv_query_qp(struct ibv_qp *qp, struct ibv_qp_attr *attr, int attr_mask, struct ibv_qp_init_attr *init_attr);
int rs_ibv_destroy_qp(struct ibv_qp *qp);
int rs_ibv_get_cq_event(struct ibv_comp_channel *channel, struct ibv_cq **cq, void **cq_context);
int rs_ibv_poll_cq(struct ibv_cq *cq, int num_entries, struct ibv_wc *wc);
int rs_ibv_req_notify_cq(struct ibv_cq *cq, int solicited_only);
int rs_ibv_destroy_cq(struct ibv_cq *cq);
int rs_ibv_modify_qp(struct ibv_qp *qp, struct ibv_qp_attr *attr, int attr_mask);
int rs_ibv_query_port(struct ibv_context *context, uint8_t port_num, struct ibv_port_attr *port_attr);
int rs_ibv_query_gid(struct ibv_context *context, uint8_t port_num, int index, union ibv_gid *gid);
int rs_ibv_close_device(struct ibv_context *context);
int rs_ibv_dealloc_pd(struct ibv_pd *pd);
int rs_ibv_destroy_comp_channel(struct ibv_comp_channel *channel);
struct ibv_context *rs_ibv_open_device(struct ibv_device *device);
struct ibv_pd *rs_ibv_alloc_pd(struct ibv_context *context);
struct ibv_device **rs_ibv_get_device_list(int *num_devices);
struct ibv_cq *rs_ibv_create_cq(struct ibv_context *context, int cqe, void *cq_context,
    struct ibv_comp_channel *channel, int comp_vector);
struct ibv_mr *rs_ibv_reg_mr(struct ibv_pd *pd, void *addr, size_t length, int access);
struct ibv_qp *rs_ibv_create_qp(struct ibv_pd *pd, struct ibv_qp_init_attr *qp_init_attr);
struct ibv_comp_channel *rs_ibv_create_comp_channel(struct ibv_context *context);
struct ibv_srq *rs_ibv_create_srq(struct ibv_pd *pd, struct ibv_srq_init_attr *srq_init_attr);
int rs_ibv_destroy_srq(struct ibv_srq *srq);
struct ibv_ah *rs_ibv_create_ah(struct ibv_pd *pd, struct ibv_ah_attr *attr);
int rs_ibv_destroy_ah(struct ibv_ah *ah);
struct ibv_cq *rs_ibv_exp_create_cq(struct ibv_context *context, int cqe, void *cq_context,
    struct ibv_comp_channel *channel, int comp_vector, struct rdma_lite_device_cq_init_attr *attr,
    struct rdma_lite_device_cq_attr *cq_resp);
int rs_ibv_exp_query_device(struct ibv_context *context, struct dev_cap_info *cap);
int rs_roce_init_mem_pool(const struct roce_mem_cq_qp_attr *mem_attr, struct rdma_lite_device_mem_attr *mem_data,
    unsigned int dev_id);
int rs_roce_deinit_mem_pool(unsigned int mem_idx);
int rs_roce_query_qpc(struct ibv_qp *qp, struct hns_roce_qpc_attr_val *attr_val, unsigned int attr_mask);
struct ibv_ah *rs_ibv_exp_create_ah(struct ibv_pd *pd, struct ibv_exp_ah_attr *attrx);
int rs_roce_mmap_ai_db_reg(struct ibv_context *context, unsigned int tgid);
int rs_roce_unmmap_ai_db_reg(struct ibv_context *context);
int rs_roce_get_cq_data_plane_info(struct ibv_cq *cq, struct hns_roce_cq_data_plane_info *info);
int rs_roce_get_qp_data_plane_info(struct ibv_qp *qp, struct hns_roce_qp_data_plane_info *info);
int rs_roce_remap_mr(struct ibv_mr *mr, struct hns_roce_mr_remap_info info[], unsigned int num);
unsigned int rs_roce_get_api_version(void);
#endif // DL_IBVERBS_FUNCTION_H

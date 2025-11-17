/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef RS_DRV_RDMA_H
#define RS_DRV_RDMA_H

#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <infiniband/verbs.h>

#include "securec.h"
#include "rs.h"
#include "rs_inner.h"
#include "verbs_exp.h"
#include "hccp_common.h"
#include "ascend_hal_external.h"

#define DEFAULT_ACCESS_FLAG (IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ | \
    IBV_ACCESS_REMOTE_ATOMIC)

void rs_drv_poll_cq_handle(struct rs_qp_cb *qp_cb);
void rs_drv_poll_srq_cq_handle(struct rs_qp_cb *qp_cb);
int rs_drv_get_gid_index(struct rs_rdev_cb *rdev_cb, struct ibv_port_attr *attr, int *idx);
int rs_drv_create_cq(struct rs_qp_cb *qp_cb, int is_ext);
int rs_drv_create_cq_with_attrs(struct rs_qp_cb *qp_cb, int is_ext, struct cq_ext_attr *cq_attr);
int rs_drv_qp_state_modifyto_reset(struct rs_qp_cb *qp_cb);
int rs_drv_qp_state_modifyto_init(struct rs_qp_cb *qp_cb, struct ibv_qp_attr *attr);
int rs_drv_qp_state_modifyto_rtr(struct rs_qp_cb *qp_cb, struct ibv_qp_attr *attr);
int rs_drv_qp_state_modifyto_rts(struct rs_qp_cb *qp_cb, struct ibv_qp_attr *attr);
struct ibv_mr* rs_drv_mr_reg(struct ibv_pd *pd, char *addr, size_t length, int access);
struct ibv_mr* rs_drv_exp_mr_reg(struct ibv_pd *pd, char *addr, size_t length,
    int access, struct roce_process_sign roce_sign);
int rs_drv_mr_dereg(struct ibv_mr *ib_mr);
void rs_drv_destroy_cq(struct rs_qp_cb *qp_cb);
int rs_drv_open_device(struct rs_cb *rscb, struct ibv_device *ib_dev);
int rs_drv_reg_notify_mr(struct rs_rdev_cb *rdev_cb);
int rs_drv_query_notify_and_alloc_pd(struct rs_rdev_cb *rdev_cb);
int rs_drv_post_recv(struct rs_qp_cb *qp_cb, struct recv_wrlist_data *wr, unsigned int recv_num,
    unsigned int *complete_num);
int rs_drv_send_exp(struct rs_qp_cb *qp_cb, struct rs_mr_cb *mr_cb,
                    struct rs_mr_cb *rem_mr_cb, struct send_wr *wr, struct send_wr_rsp *wr_rsp);
int rs_drv_send_ibv(struct rs_qp_cb *qp_cb, struct rs_mr_cb *mr_cb,
                    struct rs_mr_cb *rem_mr_cb, struct send_wr *wr, int imm_data);

int rs_drv_qp_info_related(struct rs_qp_cb *qp_cb, struct rs_rdev_cb *rdev_cb,
                           struct ibv_port_attr *attr, struct ibv_qp_attr *qp_attr);
int rs_drv_qp_create(struct rs_qp_cb *qp_cb, struct rs_qp_norm *qp_norm);
int rs_drv_qp_create_with_attrs(struct rs_qp_cb *qp_cb, struct rs_qp_norm_with_attrs *qp_norm);
void rs_drv_qp_destroy(struct rs_qp_cb *qp_cb);
int rs_drv_create_cq_event(struct rs_cq_context *cq_context, struct cq_attr *attr);
int rs_drv_create_cq_with_channel(struct rs_cq_context *cq_context, struct cq_attr *attr);
int rs_drv_destroy_cq_event(struct rs_cq_context *cq_context);
int rs_drv_normal_qp_create(struct rs_qp_cb *qp_cb, struct ibv_qp_init_attr *qp_init_attr);
int rs_drv_init_cqe_err_info(void);
void rs_drv_deinit_cqe_err_info(void);
int rs_drv_get_cqe_err_info(struct cqe_err_info *info);
int rs_query_event(int cq_event_id, struct event_summary **event);
void rs_drv_event_destroy(struct event_summary *event);
int rs_drv_compare_ip_gid(int family, union hccp_ip_addr local_ip, union ibv_gid *gid);
#ifdef CUSTOM_INTERFACE
void rs_close_backup_ib_ctx(struct rs_rdev_cb *rdev_cb);
#endif
#endif // RS_DRV_RDMA_H
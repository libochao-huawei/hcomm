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

void RsDrvPollCqHandle(struct rs_qp_cb *qpCb);
void RsDrvPollSrqCqHandle(struct rs_qp_cb *qpCb);
int RsDrvGetGidIndex(struct rs_rdev_cb *rdevCb, struct ibv_port_attr *attr, int *idx);
int RsDrvCreateCq(struct rs_qp_cb *qpCb, int isExt);
int RsDrvCreateCqWithAttrs(struct rs_qp_cb *qpCb, int isExt, struct cq_ext_attr *cqAttr);
int RsDrvQpStateModifytoReset(struct rs_qp_cb *qpCb);
int RsDrvQpStateModifytoInit(struct rs_qp_cb *qpCb, struct ibv_qp_attr *attr);
int RsDrvQpStateModifytoRtr(struct rs_qp_cb *qpCb, struct ibv_qp_attr *attr);
int RsDrvQpStateModifytoRts(struct rs_qp_cb *qpCb, struct ibv_qp_attr *attr);
struct ibv_mr* RsDrvMrReg(struct ibv_pd *pd, char *addr, size_t length, int access);
struct ibv_mr* RsDrvExpMrReg(struct ibv_pd *pd, char *addr, size_t length,
    int access, struct roce_process_sign roceSign);
int RsDrvMrDereg(struct ibv_mr *ibMr);
void RsDrvDestroyCq(struct rs_qp_cb *qpCb);
int RsDrvOpenDevice(struct rs_cb *rscb, struct ibv_device *ibDev);
int RsDrvRegNotifyMr(struct rs_rdev_cb *rdevCb);
int RsDrvQueryNotifyAndAllocPd(struct rs_rdev_cb *rdevCb);
int RsDrvPostRecv(struct rs_qp_cb *qpCb, struct recv_wrlist_data *wr, unsigned int recvNum,
    unsigned int *completeNum);
int RsDrvSendExp(struct rs_qp_cb *qpCb, struct rs_mr_cb *mrCb,
                    struct rs_mr_cb *remMrCb, struct send_wr *wr, struct send_wr_rsp *wrRsp);
int RsDrvSendIbv(struct rs_qp_cb *qpCb, struct rs_mr_cb *mrCb,
                    struct rs_mr_cb *remMrCb, struct send_wr *wr, int immData);

int RsDrvQpInfoRelated(struct rs_qp_cb *qpCb, struct rs_rdev_cb *rdevCb,
                           struct ibv_port_attr *attr, struct ibv_qp_attr *qpAttr);
int RsDrvQpCreate(struct rs_qp_cb *qpCb, struct rs_qp_norm *qpNorm);
int RsDrvQpCreateWithAttrs(struct rs_qp_cb *qpCb, struct rs_qp_norm_with_attrs *qpNorm);
void RsDrvQpDestroy(struct rs_qp_cb *qpCb);
int RsDrvCreateCqEvent(struct rs_cq_context *cqContext, struct cq_attr *attr);
int RsDrvCreateCqWithChannel(struct rs_cq_context *cqContext, struct cq_attr *attr);
int RsDrvDestroyCqEvent(struct rs_cq_context *cqContext);
int RsDrvNormalQpCreate(struct rs_qp_cb *qpCb, struct ibv_qp_init_attr *qpInitAttr);
int RsDrvInitCqeErrInfo(void);
void RsDrvDeinitCqeErrInfo(void);
int RsDrvGetCqeErrInfo(struct cqe_err_info *info);
int RsQueryEvent(int cqEventId, struct event_summary **event);
void RsDrvEventDestroy(struct event_summary *event);
int RsDrvCompareIpGid(int family, union hccp_ip_addr localIp, union ibv_gid *gid);
#ifdef CUSTOM_INTERFACE
void RsCloseBackupIbCtx(struct rs_rdev_cb *rdevCb);
#endif
#endif // RS_DRV_RDMA_H
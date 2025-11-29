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
    int (*rs_ibv_query_gid_type)(struct ibv_context *context, uint8_t portNum, unsigned int index,
        enum ibv_gid_type_sysfs *type);
    int (*rs_ibv_dereg_mr)(struct ibv_mr *mr);
    int (*rs_ibv_query_qp)(struct ibv_qp *qp, struct ibv_qp_attr *attr, int attrMask,
        struct ibv_qp_init_attr *initAttr);
    int (*rs_ibv_destroy_qp)(struct ibv_qp *qp);
    int (*rs_ibv_get_cq_event)(struct ibv_comp_channel *channel, struct ibv_cq **cq, void **cqContext);
    int (*rs_ibv_destroy_cq)(struct ibv_cq *cq);
    int (*rs_ibv_modify_qp)(struct ibv_qp *qp, struct ibv_qp_attr *attr, int attrMask);
    int (*rs_ibv_query_port)(struct ibv_context *context, uint8_t portNum, struct ibv_port_attr *portAttr);
    int (*rs_ibv_query_gid)(struct ibv_context *context, uint8_t portNum, int index, union ibv_gid *gid);
    int (*rs_ibv_close_device)(struct ibv_context *context);
    int (*rs_ibv_dealloc_pd)(struct ibv_pd *pd);
    int (*rs_ibv_destroy_comp_channel)(struct ibv_comp_channel *channel);
    struct ibv_context *(*rs_ibv_open_device)(struct ibv_device *device);
    struct ibv_pd *(*rs_ibv_alloc_pd)(struct ibv_context *context);
    struct ibv_device **(*rs_ibv_get_device_list)(int *numDevices);
    struct ibv_cq *(*rs_ibv_create_cq)(struct ibv_context *context, int cqe, void *cqContext,
        struct ibv_comp_channel *channel, int compVector);
    struct ibv_mr *(*rs_ibv_reg_mr)(struct ibv_pd *pd, void *addr, size_t length, int access);
    struct ibv_qp *(*rs_ibv_create_qp)(struct ibv_pd *pd, struct ibv_qp_init_attr *qpInitAttr);
    struct ibv_comp_channel *(*rs_ibv_create_comp_channel)(struct ibv_context *context);
    struct ibv_srq *(*rs_ibv_create_srq)(struct ibv_pd *pd, struct ibv_srq_init_attr *srqInitAttr);
    int (*rs_ibv_destroy_srq)(struct ibv_srq *);
    struct ibv_ah *(*rs_ibv_create_ah)(struct ibv_pd *pd, struct ibv_ah_attr *attr);
    int (*rs_ibv_destroy_ah)(struct ibv_ah *ah);
};

struct rs_roce_user_ops {
    int (*rs_roce_set_tsqp_depth)(const char *devName, unsigned int rdevIndex, unsigned int tempDepth,
        unsigned int *qpNum, unsigned int *sqDepth);
    int (*rs_roce_get_tsqp_depth)(const char *devName, unsigned int rdevIndex, unsigned int *tempDepth,
        unsigned int *qpNum, unsigned int *sqDepth);
    int (*rs_roce_get_roce_dev_data)(const char *devName, struct roce_dev_data *rdevData);
    int (*rs_ibv_exp_query_notify)(struct ibv_context *context, unsigned long long *notifyVa,
        unsigned long long *size);
    int (*rs_ibv_exp_post_send)(struct ibv_qp *qp, struct ibv_send_wr *wr, struct ibv_send_wr **badWr,
        struct wr_exp_rsp *expRsp);
    int (*rs_ibv_ext_post_send)(struct ibv_qp *qp, struct ibv_send_wr *wr, struct ibv_send_wr **badWr,
        struct ibv_post_send_ext_attr *extAttr, struct ibv_post_send_ext_resp *extResp);
    struct ibv_mr *(*rs_ibv_exp_reg_mr)(struct ibv_pd *pd, void *addr, size_t length, int access,
        struct roce_process_sign roceSign);
    struct ibv_qp *(*rs_ibv_exp_create_qp)(struct ibv_pd *pd, struct ibv_exp_qp_init_attr *qpInitAttr,
        struct rdma_lite_device_qp_attr *qpResp);
    struct ibv_cq *(*rs_ibv_exp_create_cq)(struct ibv_context *context, int cqe, void *cqContext,
        struct ibv_comp_channel *channel, int compVector, struct rdma_lite_device_cq_init_attr *attr,
        struct rdma_lite_device_cq_attr *cqResp);
    int (*rs_ibv_exp_query_device)(struct ibv_context *context, struct dev_cap_info *cap);
    int (*rs_roce_init_mem_pool)(const struct roce_mem_cq_qp_attr *memAttr,
        struct rdma_lite_device_mem_attr *memData, unsigned int devId);
    int (*rs_roce_deinit_mem_pool)(unsigned int memIdx);
    int (*rs_roce_query_qpc)(struct ibv_qp *qp, struct hns_roce_qpc_attr_val *attrVal, unsigned int attrMask);
    struct ibv_ah *(*rs_ibv_exp_create_ah)(struct ibv_pd *pd, struct ibv_exp_ah_attr *attrx);
    int (*rs_roce_mmap_ai_db_reg)(struct ibv_context *context, unsigned int tgid);
    int (*rs_roce_unmmap_ai_db_reg)(struct ibv_context *context);
    int (*rs_roce_get_cq_data_plane_info)(struct ibv_cq *cq, struct hns_roce_cq_data_plane_info *info);
    int (*rs_roce_get_qp_data_plane_info)(struct ibv_qp *qp, struct hns_roce_qp_data_plane_info *info);
    int (*rs_roce_remap_mr)(struct ibv_mr *mr, struct hns_roce_mr_remap_info info[], unsigned int num);
    unsigned int (*rs_roce_get_api_version)(void);
};

struct ibv_mr *RsIbvExpRegMr(struct ibv_pd *pd, void *addr, size_t length, int access,
    struct roce_process_sign roceSign);
int RsIbvExpQueryNotify(struct ibv_context *context, unsigned long long *notifyVa, unsigned long long *size);
int RsIbvExpPostSend(struct ibv_qp *qp, struct ibv_send_wr *wr, struct ibv_send_wr **badWr,
    struct wr_exp_rsp *expRsp);
int RsIbvExtPostSend(struct ibv_qp *qp, struct ibv_send_wr *wr, struct ibv_send_wr **badWr,
    struct ibv_post_send_ext_attr *extAttr, struct ibv_post_send_ext_resp *extResp);
struct ibv_qp *RsIbvExpCreateQp(
    struct ibv_pd *pd, struct ibv_exp_qp_init_attr *qpInitAttr, struct rdma_lite_device_qp_attr *qpResp);
int RsRoceSetTsqpDepth(const char *devName, unsigned int rdevIndex, unsigned int tempDepth,
    unsigned int *qpNum, unsigned int *sqDepth);
int RsRoceGetTsqpDepth(const char *devName, unsigned int rdevIndex, unsigned int *tempDepth,
    unsigned int *qpNum, unsigned int *sqDepth);
int RsRoceGetRoceDevData(const char *devName, struct roce_dev_data *rdevData);

DL_ATTRI_VISI_DEF void RsApiDeinit(void);
DL_ATTRI_VISI_DEF int RsApiInit(void);
void RsIbvFreeDeviceList(struct ibv_device **list);
void RsIbvAckCqEvents(struct ibv_cq *cq, unsigned int nevents);
const char *RsIbvGetDeviceName(struct ibv_device *device);
const char *RsIbvWcStatusStr(enum ibv_wc_status status);
int RsIbvQueryGidType(struct ibv_context *context, uint8_t portNum, unsigned int index,
    enum ibv_gid_type_sysfs *type);
int RsIbvDeregMr(struct ibv_mr *mr);
int RsIbvPostSend(struct ibv_qp *qp, struct ibv_send_wr *wr, struct ibv_send_wr **badWr);
int RsIbvPostRecv(struct ibv_qp *qp, struct ibv_recv_wr *wr, struct ibv_recv_wr **badWr);
int RsIbvQueryQp(struct ibv_qp *qp, struct ibv_qp_attr *attr, int attrMask, struct ibv_qp_init_attr *initAttr);
int RsIbvDestroyQp(struct ibv_qp *qp);
int RsIbvGetCqEvent(struct ibv_comp_channel *channel, struct ibv_cq **cq, void **cqContext);
int RsIbvPollCq(struct ibv_cq *cq, int numEntries, struct ibv_wc *wc);
int RsIbvReqNotifyCq(struct ibv_cq *cq, int solicitedOnly);
int RsIbvDestroyCq(struct ibv_cq *cq);
int RsIbvModifyQp(struct ibv_qp *qp, struct ibv_qp_attr *attr, int attrMask);
int RsIbvQueryPort(struct ibv_context *context, uint8_t portNum, struct ibv_port_attr *portAttr);
int RsIbvQueryGid(struct ibv_context *context, uint8_t portNum, int index, union ibv_gid *gid);
int RsIbvCloseDevice(struct ibv_context *context);
int RsIbvDeallocPd(struct ibv_pd *pd);
int RsIbvDestroyCompChannel(struct ibv_comp_channel *channel);
struct ibv_context *RsIbvOpenDevice(struct ibv_device *device);
struct ibv_pd *RsIbvAllocPd(struct ibv_context *context);
struct ibv_device **RsIbvGetDeviceList(int *numDevices);
struct ibv_cq *RsIbvCreateCq(struct ibv_context *context, int cqe, void *cqContext,
    struct ibv_comp_channel *channel, int compVector);
struct ibv_mr *RsIbvRegMr(struct ibv_pd *pd, void *addr, size_t length, int access);
struct ibv_qp *RsIbvCreateQp(struct ibv_pd *pd, struct ibv_qp_init_attr *qpInitAttr);
struct ibv_comp_channel *RsIbvCreateCompChannel(struct ibv_context *context);
struct ibv_srq *RsIbvCreateSrq(struct ibv_pd *pd, struct ibv_srq_init_attr *srqInitAttr);
int RsIbvDestroySrq(struct ibv_srq *srq);
struct ibv_ah *RsIbvCreateAh(struct ibv_pd *pd, struct ibv_ah_attr *attr);
int RsIbvDestroyAh(struct ibv_ah *ah);
struct ibv_cq *RsIbvExpCreateCq(struct ibv_context *context, int cqe, void *cqContext,
    struct ibv_comp_channel *channel, int compVector, struct rdma_lite_device_cq_init_attr *attr,
    struct rdma_lite_device_cq_attr *cqResp);
int RsIbvExpQueryDevice(struct ibv_context *context, struct dev_cap_info *cap);
int RsRoceInitMemPool(const struct roce_mem_cq_qp_attr *memAttr, struct rdma_lite_device_mem_attr *memData,
    unsigned int devId);
int RsRoceDeinitMemPool(unsigned int memIdx);
int RsRoceQueryQpc(struct ibv_qp *qp, struct hns_roce_qpc_attr_val *attrVal, unsigned int attrMask);
struct ibv_ah *RsIbvExpCreateAh(struct ibv_pd *pd, struct ibv_exp_ah_attr *attrx);
int RsRoceMmapAiDbReg(struct ibv_context *context, unsigned int tgid);
int RsRoceUnmmapAiDbReg(struct ibv_context *context);
int RsRoceGetCqDataPlaneInfo(struct ibv_cq *cq, struct hns_roce_cq_data_plane_info *info);
int RsRoceGetQpDataPlaneInfo(struct ibv_qp *qp, struct hns_roce_qp_data_plane_info *info);
int RsRoceRemapMr(struct ibv_mr *mr, struct hns_roce_mr_remap_info info[], unsigned int num);
unsigned int RsRoceGetApiVersion(void);
#endif // DL_IBVERBS_FUNCTION_H

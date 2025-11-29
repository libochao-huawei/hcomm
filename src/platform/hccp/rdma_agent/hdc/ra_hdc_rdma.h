/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef RA_HDC_RDMA_H
#define RA_HDC_RDMA_H
#include "ascend_hal.h"
#include "hccp_common.h"
#include "ra.h"
#include "ra_hdc.h"
#include "ra_rs_comm.h"

#define RA_MAX_BATCH_QP_MODIFY_NUM  768

union op_rdev_init_data {
    struct {
        struct rdev rdev_info;
        unsigned int rsvd;
    } tx_data;

    struct {
        unsigned int rdev_index;
        unsigned int rsvd;
    } rx_data;
};

union op_rdev_init_with_backup_data {
    struct {
        struct rdev rdev_info;
        struct rdev backup_rdev_info;
        unsigned int rsvd;
    } tx_data;

    struct {
        unsigned int rdev_index;
        unsigned int rsvd;
    } rx_data;
};

union op_rdev_deinit_data {
    struct {
        unsigned int phy_id;
        unsigned int rdev_index;
        unsigned int rsvd;
    } tx_data;

    struct {
        unsigned int rsvd;
    } rx_data;
};

union op_set_tsqp_depth_data {
    struct {
        unsigned int phy_id;
        unsigned int rdev_index;
        unsigned int temp_depth;
        unsigned int rsvd;
    } tx_data;

    struct {
        unsigned int qp_num;
        unsigned int rsvd;
    } rx_data;
};

union op_get_tsqp_depth_data {
    struct {
        unsigned int phy_id;
        unsigned int rdev_index;
        unsigned int rsvd;
    } tx_data;

    struct {
        unsigned int temp_depth;
        unsigned int qp_num;
        unsigned int rsvd;
    } rx_data;
};

union op_rdev_get_port_status_data {
    struct {
        unsigned int phy_id;
        unsigned int rdev_index;
        unsigned int rsvd;
    } tx_data;

    struct {
        enum port_status status;
        unsigned int rsvd;
    } rx_data;
};

union op_qp_create_data {
    struct {
        unsigned int phy_id;
        unsigned int rdev_index;
        int qp_mode;
        int flag;
        int mem_align;  // 0,1:4KB, 2:2MB
        unsigned int rsvd;
    } tx_data;

    struct {
        unsigned int qpn;
        unsigned int psn;
        unsigned int gid_idx;
        unsigned int rsvd;
    } rx_data;
};

union op_qp_create_with_attrs_data {
    struct {
        unsigned int phy_id;
        unsigned int rdev_index;
        struct qp_ext_attrs ext_attrs;
    } tx_data;

    struct {
        unsigned int qpn;
        unsigned int psn;
        unsigned int gid_idx;
        unsigned int rsvd;
    } rx_data;
};

union op_ai_qp_create_data {
    struct {
        unsigned int phy_id;
        unsigned int rdev_index;
        struct qp_ext_attrs ext_attrs;
    } tx_data;

    struct {
        unsigned int qpn;
        unsigned long long ai_qp_addr;  // refer to struct ibv_qp *
        unsigned int sq_index;          // index of sq
        unsigned int db_index;          // index of db
        unsigned int psn;
    } rx_data;
};

union op_ai_qp_create_with_attrs_data {
    struct {
        unsigned int phy_id;
        unsigned int rdev_index;
        struct qp_ext_attrs ext_attrs;
    } tx_data;

    struct {
        unsigned int qpn;
        unsigned int gid_idx;
        unsigned int psn;
        unsigned long long ai_qp_addr;  // refer to struct ibv_qp *
        unsigned int sq_index;          // index of sq
        unsigned int db_index;          // index of db
        unsigned long long ai_scq_addr; // refer to struct ibv_cq *scq
        unsigned long long ai_rcq_addr; // refer to struct ibv_cq *rcq
        struct ai_data_plane_info data_plane_info;
        unsigned int rsvd[32U];
    } rx_data;
};

union op_typical_qp_create_data {
    struct {
        unsigned int phy_id;
        unsigned int rdev_index;
        int qp_mode;
        int flag;
        int mem_align;  // 0,1:4KB, 2:2MB
        unsigned int rsvd;
    } tx_data;

    struct {
        unsigned int qpn;
        unsigned int gid_idx;
        unsigned int psn;
        union ibv_gid gid;
    } rx_data;
};

union op_qp_destroy_data {
    struct {
        unsigned int phy_id;
        unsigned int rdev_index;
        unsigned int qpn;
    } tx_data;

    struct {
        unsigned int rsvd;
    } rx_data;
};

union op_qp_status_data {
    struct {
        unsigned int phy_id;
        unsigned int rdev_index;
        unsigned int qpn;
    } tx_data;

    struct {
        int status;
    } rx_data;
};

union op_qp_info_data {
    struct {
        unsigned int phy_id;
        unsigned int rdev_index;
        unsigned int qpn;
        unsigned int rsvd[32U];
    } tx_data;
 
    struct {
        int status;
        unsigned int udp_sport;
        unsigned int rsvd[64U];
    } rx_data;
};

union op_typical_qp_modify_data {
    struct {
        unsigned int phy_id;
        unsigned int rdev_index;
        struct typical_qp local_qp_info;
        struct typical_qp remote_qp_info;
        unsigned int rsvd[RA_RSVD_NUM_4];
    } tx_data;

    struct {
        unsigned int udp_sport;
        unsigned int rsvd[RA_RSVD_NUM_3];
    } rx_data;
};

union op_qp_batch_modify_data {
    struct {
        unsigned int phy_id;
        unsigned int rdev_index;
        int status;
        int qpn[RA_MAX_BATCH_QP_MODIFY_NUM];
        int qpn_num;
        unsigned int rsvd[RA_RSVD_NUM_4];
    } tx_data;

    struct {
        unsigned int rsvd[RA_RSVD_NUM_4];
    } rx_data;
};

union op_qp_connect_data {
    struct {
        unsigned int phy_id;
        unsigned int rdev_index;
        unsigned int qpn;
        unsigned int fd;
    } tx_data;

    struct {
        unsigned int rsvd[RA_RSVD_NUM_2];
    } rx_data;
};

union op_mr_reg_data {
    struct {
        unsigned int phy_id;
        unsigned int rdev_index;
        unsigned int qpn;
        struct rdma_mr_reg_attr mr_reg_attr;
    } tx_data;

    struct {
        unsigned int lkey;
        unsigned int rkey;
        unsigned int rsvd[RA_RSVD_NUM_4];
    } rx_data;
};

union op_mr_dereg_data {
    struct {
        unsigned int phy_id;
        unsigned int rdev_index;
        unsigned int qpn;
        unsigned int rsvd;
        char *addr;
    } tx_data;

    struct {
        unsigned int rsvd[RA_RSVD_NUM_4];
    } rx_data;
};

union op_typical_mr_reg_data {
    struct {
        unsigned int phy_id;
        unsigned int rdev_index;
        struct rdma_mr_reg_attr mr_reg_attr;
    } tx_data;

    struct {
        unsigned int lkey;
        unsigned int rkey;
        uint64_t addr;
        unsigned int rsvd[RA_RSVD_NUM_2];
    } rx_data;
};

union op_remap_mr_data {
    struct {
        struct mem_remap_info mem_list[REMAP_MR_MAX_NUM];
        unsigned int mem_num;
        unsigned int phy_id;
        unsigned int rdev_index;
        unsigned int rsvd[RA_RSVD_NUM_4];
    } tx_data;
    struct {
        unsigned int rsvd[RA_RSVD_NUM_4];
    } rx_data;
};

union op_typical_mr_dereg_data {
    struct {
        unsigned int phy_id;
        unsigned int rdev_index;
        uint64_t addr;
    } tx_data;

    struct {
        unsigned int rsvd[RA_RSVD_NUM_4];
    } rx_data;
};

union op_send_wr_data {
    struct {
        unsigned int phy_id;
        unsigned int rdev_index;
        unsigned int qpn;
        unsigned short buf_num;
        unsigned short rsvd;
        unsigned long long dst_addr;
        unsigned int op;
        int send_flags;
        struct sg_list mem_list[MAX_SGE_NUM];
    } tx_data;

    struct {
        struct send_wr_rsp wr_rsp;
        unsigned int rsvd[RA_RSVD_NUM_50];
    } rx_data;
};

union op_send_wrlist_data {
    struct {
        unsigned int phy_id;
        unsigned int rdev_index;
        unsigned int qpn;
        unsigned int send_num;
        struct send_wrlist_data wrlist[MAX_WR_NUM_V1];
    } tx_data;

    struct {
        unsigned int complete_num;
        struct send_wr_rsp wr_rsp[MAX_WR_NUM_V1];
        unsigned int rsvd[RA_RSVD_NUM_50];
    } rx_data;
};

union op_send_wrlist_data_ext {
    struct {
        unsigned int phy_id;
        unsigned int rdev_index;
        unsigned int qpn;
        unsigned int send_num;
        struct send_wrlist_data_ext wrlist[MAX_WR_NUM_V1];
    } tx_data;

    struct {
        unsigned int complete_num;
        struct send_wr_rsp wr_rsp[MAX_WR_NUM_V1];
        unsigned int rsvd[RA_RSVD_NUM_50];
    } rx_data;
};

union op_send_wrlist_data_v2 {
    struct {
        unsigned int phy_id;
        unsigned int rdev_index;
        unsigned int qpn;
        unsigned int send_num;
        struct send_wrlist_data wrlist[MAX_WR_NUM];
    } tx_data;

    struct {
        unsigned int complete_num;
        struct send_wr_rsp wr_rsp[MAX_WR_NUM];
        unsigned int rsvd[RA_RSVD_NUM_50];
    } rx_data;
};

union op_send_wrlist_data_ext_v2 {
    struct {
        unsigned int phy_id;
        unsigned int rdev_index;
        unsigned int qpn;
        unsigned int send_num;
        struct send_wrlist_data_ext wrlist[MAX_WR_NUM];
    } tx_data;

    struct {
        unsigned int complete_num;
        struct send_wr_rsp wr_rsp[MAX_WR_NUM];
        unsigned int rsvd[RA_RSVD_NUM_50];
    } rx_data;
};

union op_send_normal_wrlist_data {
    struct {
        unsigned int phy_id;
        unsigned int rdev_index;
        unsigned int qpn;
        unsigned int send_num;
        struct wr_info wrlist[MAX_WR_NUM];
        unsigned int rsvd[RA_RSVD_NUM_50];
    } tx_data;
 
    struct {
        unsigned int complete_num;
        struct send_wr_rsp wr_rsp[MAX_WR_NUM];
        unsigned int rsvd[RA_RSVD_NUM_50];
    } rx_data;
};

union op_set_qp_attr_qos_data {
    struct {
        unsigned int phy_id;
        unsigned int rdev_index;
        unsigned int qpn;
        struct qos_attr qos_attr;
    } tx_data;

    struct {
        unsigned int rsvd[RA_RSVD_NUM_6];
    } rx_data;
};

union op_set_qp_attr_timeout_data {
    struct {
        unsigned int phy_id;
        unsigned int rdev_index;
        unsigned int qpn;
        unsigned int timeout;  // Rdma超时时间
    } tx_data;

    struct {
        unsigned int rsvd[RA_RSVD_NUM_4];
    } rx_data;
};

union op_set_qp_attr_retry_cnt_data {
    struct {
        unsigned int phy_id;
        unsigned int rdev_index;
        unsigned int qpn;
        unsigned int retry_cnt;  // Rdma重传次数
    } tx_data;

    struct {
        unsigned int rsvd[RA_RSVD_NUM_4];
    } rx_data;
};

union op_get_cqe_err_info_num_data {
    struct {
        unsigned int phy_id;
        unsigned int rdev_index;
        unsigned int rsvd[RA_RSVD_NUM_4];
    } tx_data;
    struct {
        unsigned int num;
        unsigned int rsvd[RA_RSVD_NUM_4];
    } rx_data;
};

union op_get_cqe_err_info_list_data {
    struct {
        unsigned int phy_id;
        unsigned int rdev_index;
        unsigned int num;
        unsigned int rsvd[RA_RSVD_NUM_4];
    } tx_data;
    struct {
        struct cqe_err_info info_list[CQE_ERR_INFO_MAX_NUM];
        unsigned int num;
        unsigned int rsvd[RA_RSVD_NUM_4];
    } rx_data;
};

int RaHdcQpCreate(struct ra_rdma_handle *rdmaHandle, int flag, int qpMode, void **qpHandle);
int RaHdcQpCreateWithAttrs(struct ra_rdma_handle *rdmaHandle, struct qp_ext_attrs *extAttrs, void **qpHandle);
int RaHdcAiQpCreate(struct ra_rdma_handle *rdmaHandle, struct qp_ext_attrs *extAttrs,
    struct ai_qp_info *info, void **qpHandle);
int RaHdcAiQpCreateWithAttrs(struct ra_rdma_handle *rdmaHandle, struct qp_ext_attrs *extAttrs,
    struct ai_qp_info *info, void **qpHandle);
int RaHdcTypicalQpCreate(struct ra_rdma_handle *rdmaHandle, int flag, int qpMode, struct typical_qp *qpInfo,
    void **qpHandle);
int RaHdcPollCq(struct ra_qp_handle *qpHdc, bool isSendCq, unsigned int numEntries, void *wc);
int RaHdcQpDestroy(struct ra_qp_handle *qpHdc);
int RaHdcTypicalQpModify(struct ra_qp_handle *qpHdc, struct typical_qp *localQpInfo,
    struct typical_qp *remoteQpInfo);
int RaHdcQpConnectAsync(struct ra_qp_handle *qpHdc, const void *sockHandle);
int RaHdcGetQpStatus(struct ra_qp_handle *qpHdc, int *status);
int RaHdcMrReg(struct ra_qp_handle *qpHdc, struct mr_info *info);
int RaHdcMrDereg(struct ra_qp_handle *qpHdc, struct mr_info *info);
int RaHdcTypicalMrReg(struct ra_rdma_handle *rdmaHandle, struct mr_info *info, void **mrHandle);
int RaHdcRemapMr(struct ra_rdma_handle *rdmaHandle, struct mem_remap_info info[], unsigned int num);
int RaHdcTypicalMrDereg(struct ra_rdma_handle *rdmaHandle, void *mrHandle);
int RaHdcSendWr(struct ra_qp_handle *qpHdc, struct send_wr *wr, struct send_wr_rsp *opRsp);
int RaHdcTypicalSendWr(struct ra_qp_handle *qpHdc, struct send_wr *wr, struct send_wr_rsp *opRsp);
int RaHdcSendWrV2(struct ra_qp_handle *qpHdc, struct send_wr_v2 *wr, struct send_wr_rsp *opRsp);
int RaHdcSendWrlist(struct ra_qp_handle *qpHdc, struct send_wrlist_data wr[], struct send_wr_rsp opRsp[],
    struct wrlist_send_complete_num wrlistNum);
int RaHdcSendWrlistExt(struct ra_qp_handle *qpHdc, struct send_wrlist_data_ext wr[], struct send_wr_rsp opRsp[],
    struct wrlist_send_complete_num wrlistNum);
int RaHdcSendNormalWrlist(struct ra_qp_handle *qpHdc, struct wr_info wr[], struct send_wr_rsp opRsp[],
    struct wrlist_send_complete_num wrlistNum);
int RaHdcRecvWrlist(struct ra_qp_handle *qpHdc, struct recv_wrlist_data *wr, unsigned int recvNum,
    unsigned int *completeNum);
int RaHdcRdevInit(struct ra_rdma_handle *rdmaHandle, unsigned int notifyType, struct rdev rdevInfo,
    unsigned int *rdevIndex);
int RaHdcRdevGetPortStatus(struct ra_rdma_handle *rdmaHandle, enum port_status *status);
int RaHdcRdevDeinit(struct ra_rdma_handle *rdmaHandle, unsigned int notifyType);
int RaHdcRdevRestoreDeinit(struct ra_rdma_handle *rdmaHandle, unsigned int notifyType);
int RaHdcSetTsqpDepth(struct ra_rdma_handle *rdmaHandle, unsigned int tempDepth, unsigned int *qpNum);
int RaHdcGetTsqpDepth(struct ra_rdma_handle *rdmaHandle, unsigned int *tempDepth, unsigned int *qpNum);
int RaHdcSetQpAttrQos(struct ra_qp_handle *qpHdc, struct qos_attr *attr);
int RaHdcSetQpAttrTimeout(struct ra_qp_handle *qpHdc, unsigned int *timeout);
int RaHdcSetQpAttrRetryCnt(struct ra_qp_handle *qpHdc, unsigned int *retryCnt);
int RaHdcGetCqeErrInfoList(struct ra_rdma_handle *rdmaHandle, struct cqe_err_info *infoList, unsigned int *num);
int RaHdcQpBatchModify(struct ra_rdma_handle *rdmaHandle, void *qpHdc[], unsigned int num, int expectStatus);
int RaHdcRdmaSetOps(struct ra_rdma_handle *rdmaHandle, struct ra_rdma_ops *rdmaOps);
int RaHdcRdmaSaveSnapshot(struct ra_rdma_handle *rdmaHandle, enum save_snapshot_action action);
int RaHdcRdmaRestoreSnapshot(struct ra_rdma_handle *rdmaHandle, struct ra_rdma_ops *rdmaOps);
#endif // RA_HDC_RDMA_H

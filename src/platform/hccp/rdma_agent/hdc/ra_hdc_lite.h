/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef RA_HDC_LITE_H
#define RA_HDC_LITE_H

#include "ascend_hal.h"
#include "stdio.h"
#include "hccp.h"
#include "hccp_common.h"
#include "ra.h"
#include "ra_rs_comm.h"

#define RA_SGLIST_MAX       16
#define RA_QP_32K_DEPTH         32767
#define RA_QP_128_DEPTH         128

#define WRITE_NOTIFY_OFFSET_MASK  0xffffff
#define WRITE_NOTIFY_VALUE_RECORD 0x1000000

#define RA_LITE_POLL_CQE_PERIOD_TIME 10000 // 10ms

#define HDC_LITE_DEFAULT_WR_ID 0

struct lite_send_wr {
    struct send_wr wr;
    union {
        struct wr_aux_info aux;
        struct wr_ext_info ext;
    };
};

union op_lite_support_data {
    struct {
        unsigned int phy_id;
        unsigned int rdev_index;
        unsigned int rsvd;
    } tx_data;

    struct {
        int support_lite;
    } rx_data;
};

union op_lite_rdev_cap_data {
    struct {
        unsigned int phy_id;
        unsigned int rdev_index;
        unsigned int rsvd;
    } tx_data;

    struct {
        struct lite_rdev_cap_resp resp;
    } rx_data;
};

union op_lite_qp_cq_attr_data {
    struct {
        unsigned int phy_id;
        unsigned int rdev_index;
        unsigned int qpn;
    } tx_data;

    struct {
        struct lite_qp_cq_attr_resp resp;
    } rx_data;
};

union op_lite_mem_attr_data {
    struct {
        unsigned int phy_id;
        unsigned int rdev_index;
        unsigned int qpn;
    } tx_data;

    struct {
        struct lite_mem_attr_resp resp;
    } rx_data;
};

union op_lite_connected_info_data {
    struct {
        unsigned int phy_id;
        unsigned int rdev_index;
        unsigned int qpn;
    } tx_data;

    struct {
        struct lite_connected_info_resp resp;
    } rx_data;
};

int RaHdcLiteQpCreate(struct ra_rdma_handle *rdmaHandle, struct ra_qp_handle *qpHdc,
    struct rdma_lite_qp_cap *cap);
void RaHdcLiteQpDestroy(struct ra_qp_handle *qpHdc);
int RaHdcLiteInit(struct ra_rdma_handle *rdmaHandle, unsigned int phyId, unsigned int rdevIndex);
void RaHdcLiteDeinit(struct ra_rdma_handle *rdmaHandle);
int RaHdcLiteSendWr(struct ra_qp_handle *qpHdc, struct lite_send_wr *wr, struct send_wr_rsp *opRsp,
    unsigned long long wrId);
int RaHdcLiteTypicalSendWr(struct ra_qp_handle *qpHdc, struct lite_send_wr *wr, struct send_wr_rsp *opRsp,
    unsigned long long wrId);
int RaHdcLiteGetConnectedInfo(struct ra_qp_handle *qpHdc);
int RaHdcLiteSendWrlist(struct ra_qp_handle *qpHdc, struct send_wrlist_data wr[], struct send_wr_rsp opRsp[],
    struct wrlist_send_complete_num wrlistNum);
int RaHdcLiteSendWrlistExt(struct ra_qp_handle *qpHdc, struct send_wrlist_data_ext wr[],
    struct send_wr_rsp opRsp[], struct wrlist_send_complete_num wrlistNum);
int RaHdcLiteSendNormalWrlist(struct ra_qp_handle *qpHdc, struct wr_info wr[], struct send_wr_rsp opRsp[],
    struct wrlist_send_complete_num wrlistNum);
int RaHdcLiteRecvWrlist(struct ra_qp_handle *qpHdc, struct recv_wrlist_data *wr, unsigned int recvNum,
    unsigned int *completeNum);
int RaHdcLitePollCq(struct ra_qp_handle *qpHdc, bool isSendCq, unsigned int numEntries,
    struct rdma_lite_wc_v2 *liteWc);
int RaHdcLiteInitCqeErrInfo(unsigned int phyId);
void RaHdcLiteDeinitCqeErrInfo(unsigned int phyId);
void RaHdcLiteGetCqeErrInfo(unsigned int phyId, struct cqe_err_info *info);
int RaHdcLiteGetCqeErrInfoList(struct ra_rdma_handle *rdmaHandle, struct cqe_err_info *infoList,
    unsigned int *num);
#endif // RA_HDC_LITE_H

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

int ra_hdc_lite_qp_create(struct ra_rdma_handle *rdma_handle, struct ra_qp_handle *qp_hdc,
    struct rdma_lite_qp_cap *cap);
void ra_hdc_lite_qp_destroy(struct ra_qp_handle *qp_hdc);
int ra_hdc_lite_init(struct ra_rdma_handle *rdma_handle, unsigned int phy_id, unsigned int rdev_index);
void ra_hdc_lite_deinit(struct ra_rdma_handle *rdma_handle);
int ra_hdc_lite_send_wr(struct ra_qp_handle *qp_hdc, struct lite_send_wr *wr, struct send_wr_rsp *op_rsp,
    unsigned long long wr_id);
int ra_hdc_lite_typical_send_wr(struct ra_qp_handle *qp_hdc, struct lite_send_wr *wr, struct send_wr_rsp *op_rsp,
    unsigned long long wr_id);
int ra_hdc_lite_get_connected_info(struct ra_qp_handle *qp_hdc);
int ra_hdc_lite_send_wrlist(struct ra_qp_handle *qp_hdc, struct send_wrlist_data wr[], struct send_wr_rsp op_rsp[],
    struct wrlist_send_complete_num wrlist_num);
int ra_hdc_lite_send_wrlist_ext(struct ra_qp_handle *qp_hdc, struct send_wrlist_data_ext wr[],
    struct send_wr_rsp op_rsp[], struct wrlist_send_complete_num wrlist_num);
int ra_hdc_lite_send_normal_wrlist(struct ra_qp_handle *qp_hdc, struct wr_info wr[], struct send_wr_rsp op_rsp[],
    struct wrlist_send_complete_num wrlist_num);
int ra_hdc_lite_recv_wrlist(struct ra_qp_handle *qp_hdc, struct recv_wrlist_data *wr, unsigned int recv_num,
    unsigned int *complete_num);
int ra_hdc_lite_poll_cq(struct ra_qp_handle *qp_hdc, bool is_send_cq, unsigned int num_entries,
    struct rdma_lite_wc_v2 *lite_wc);
int ra_hdc_lite_init_cqe_err_info(unsigned int phy_id);
void ra_hdc_lite_deinit_cqe_err_info(unsigned int phy_id);
void ra_hdc_lite_get_cqe_err_info(unsigned int phy_id, struct cqe_err_info *info);
int ra_hdc_lite_get_cqe_err_info_list(struct ra_rdma_handle *rdma_handle, struct cqe_err_info *info_list,
    unsigned int *num);
#endif // RA_HDC_LITE_H

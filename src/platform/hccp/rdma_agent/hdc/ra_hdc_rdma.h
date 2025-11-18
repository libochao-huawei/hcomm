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

int ra_hdc_qp_create(struct ra_rdma_handle *rdma_handle, int flag, int qp_mode, void **qp_handle);
int ra_hdc_qp_create_with_attrs(struct ra_rdma_handle *rdma_handle, struct qp_ext_attrs *ext_attrs, void **qp_handle);
int ra_hdc_ai_qp_create(struct ra_rdma_handle *rdma_handle, struct qp_ext_attrs *ext_attrs,
    struct ai_qp_info *info, void **qp_handle);
int ra_hdc_ai_qp_create_with_attrs(struct ra_rdma_handle *rdma_handle, struct qp_ext_attrs *ext_attrs,
    struct ai_qp_info *info, void **qp_handle);
int ra_hdc_typical_qp_create(struct ra_rdma_handle *rdma_handle, int flag, int qp_mode, struct typical_qp *qp_info,
    void **qp_handle);
int ra_hdc_poll_cq(struct ra_qp_handle *qp_hdc, bool is_send_cq, unsigned int num_entries, void *wc);
int ra_hdc_qp_destroy(struct ra_qp_handle *qp_hdc);
int ra_hdc_typical_qp_modify(struct ra_qp_handle *qp_hdc, struct typical_qp *local_qp_info,
    struct typical_qp *remote_qp_info);
int ra_hdc_qp_connect_async(struct ra_qp_handle *qp_hdc, const void *sock_handle);
int ra_hdc_get_qp_status(struct ra_qp_handle *qp_hdc, int *status);
int ra_hdc_mr_reg(struct ra_qp_handle *qp_hdc, struct mr_info *info);
int ra_hdc_mr_dereg(struct ra_qp_handle *qp_hdc, struct mr_info *info);
int ra_hdc_typical_mr_reg(struct ra_rdma_handle *rdma_handle, struct mr_info *info, void **mr_handle);
int ra_hdc_remap_mr(struct ra_rdma_handle *rdma_handle, struct mem_remap_info info[], unsigned int num);
int ra_hdc_typical_mr_dereg(struct ra_rdma_handle *rdma_handle, void *mr_handle);
int ra_hdc_send_wr(struct ra_qp_handle *qp_hdc, struct send_wr *wr, struct send_wr_rsp *op_rsp);
int ra_hdc_typical_send_wr(struct ra_qp_handle *qp_hdc, struct send_wr *wr, struct send_wr_rsp *op_rsp);
int ra_hdc_send_wr_v2(struct ra_qp_handle *qp_hdc, struct send_wr_v2 *wr, struct send_wr_rsp *op_rsp);
int ra_hdc_send_wrlist(struct ra_qp_handle *qp_hdc, struct send_wrlist_data wr[], struct send_wr_rsp op_rsp[],
    struct wrlist_send_complete_num wrlist_num);
int ra_hdc_send_wrlist_ext(struct ra_qp_handle *qp_hdc, struct send_wrlist_data_ext wr[], struct send_wr_rsp op_rsp[],
    struct wrlist_send_complete_num wrlist_num);
int ra_hdc_send_normal_wrlist(struct ra_qp_handle *qp_hdc, struct wr_info wr[], struct send_wr_rsp op_rsp[],
    struct wrlist_send_complete_num wrlist_num);
int ra_hdc_recv_wrlist(struct ra_qp_handle *qp_hdc, struct recv_wrlist_data *wr, unsigned int recv_num,
    unsigned int *complete_num);
int ra_hdc_rdev_init(struct ra_rdma_handle *rdma_handle, unsigned int notify_type, struct rdev rdev_info,
    unsigned int *rdev_index);
int ra_hdc_rdev_get_port_status(struct ra_rdma_handle *rdma_handle, enum port_status *status);
int ra_hdc_rdev_deinit(struct ra_rdma_handle *rdma_handle, unsigned int notify_type);
int ra_hdc_rdev_restore_deinit(struct ra_rdma_handle *rdma_handle, unsigned int notify_type);
int ra_hdc_set_tsqp_depth(struct ra_rdma_handle *rdma_handle, unsigned int temp_depth, unsigned int *qp_num);
int ra_hdc_get_tsqp_depth(struct ra_rdma_handle *rdma_handle, unsigned int *temp_depth, unsigned int *qp_num);
int ra_hdc_set_qp_attr_qos(struct ra_qp_handle *qp_hdc, struct qos_attr *attr);
int ra_hdc_set_qp_attr_timeout(struct ra_qp_handle *qp_hdc, unsigned int *timeout);
int ra_hdc_set_qp_attr_retry_cnt(struct ra_qp_handle *qp_hdc, unsigned int *retry_cnt);
int ra_hdc_get_cqe_err_info_list(struct ra_rdma_handle *rdma_handle, struct cqe_err_info *info_list, unsigned int *num);
int ra_hdc_qp_batch_modify(struct ra_rdma_handle *rdma_handle, void *qp_hdc[], unsigned int num, int expect_status);
int ra_hdc_rdma_set_ops(struct ra_rdma_handle *rdma_handle, struct ra_rdma_ops *rdma_ops);
int ra_hdc_rdma_save_snapshot(struct ra_rdma_handle *rdma_handle, enum save_snapshot_action action);
int ra_hdc_rdma_restore_snapshot(struct ra_rdma_handle *rdma_handle, struct ra_rdma_ops *rdma_ops);
#endif // RA_HDC_RDMA_H

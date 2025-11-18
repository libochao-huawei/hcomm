/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef _HNS_ROCE_U_HW_V2_OPREATION_H
#define _HNS_ROCE_U_HW_V2_OPREATION_H

#include "hns_roce_u.h"
#include "hns_roce_u_hw_v2.h"

struct hns_roce_u_table {
    enum ibv_wr_opcode wr_opcode;
    func_point rdma_operation;
};

char *get_send_wqe(struct hns_roce_qp *qp, int n);
void *get_send_sge_ex(struct hns_roce_qp *qp, int n);
void set_data_seg_v2(struct hns_roce_v2_wqe_data_seg *dseg,
    struct ibv_sge *sg);
void *get_sw_cqe_v2(struct hns_roce_cq *cq, int n);
void *get_srq_wqe(struct hns_roce_srq *srq, int n);
struct hns_roce_v2_cqe *get_cqe_v2(struct hns_roce_cq *cq, int entry);
struct hns_roce_v2_cqe *next_cqe_sw_v2(struct hns_roce_cq *cq);
void *get_recv_wqe_v2(struct hns_roce_qp *qp, int n);
int hns_roce_v2_wq_overflow(struct hns_roce_wq *wq, int nreq,
    struct hns_roce_cq *cq);
void hns_roce_update_rq_db(struct hns_roce_context *ctx,
    unsigned int qpn, unsigned int rq_head);
void hns_roce_update_sq_db(struct hns_roce_context *ctx,
    unsigned int qpn, unsigned int sl,
    unsigned int sq_head,
    int gdr_mode);
void hns_roce_get_sq_db(unsigned int qpn, unsigned int sl,
    unsigned int sq_head, uint64_t *value);
void hns_roce_v2_update_cq_cons_index(struct hns_roce_context *ctx,
    const struct hns_roce_cq *cq);
int hns_roce_alloc_gdr_template_wqe(struct hns_roce_qp *qp, unsigned int *wqe_index);
int hns_roce_u_v2_operation(struct ibv_send_wr *wr,
    struct hns_roce_rc_sq_wqe *rc_sq_wqe);
void hns_roce_dealloc_gdr_template_wqe(struct hns_roce_qp *qp, unsigned int wqe_index);

#endif /* _HNS_ROCE_U_HW_V2_OPREATION_H */

/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef _HNS_ROCE_U_STDIO_H
#define _HNS_ROCE_U_STDIO_H
#include <linux/types.h>

#include "hns_roce_u.h"

int align_queue_size(int req);
int align_qp_size(int req);
void hns_roce_calc_sq_wqe_size(struct ibv_qp_cap *cap, struct hns_roce_qp *qp);
void hns_roce_set_qp_params(struct ibv_pd *pd, struct ibv_qp_init_attr *attr, struct hns_roce_qp *qp);
int hns_roce_verify_qp(struct ibv_qp_init_attr *attr, struct hns_roce_context *context);
int hns_roce_alloc_qp_buf(struct ibv_pd *pd, struct ibv_qp_cap *cap,
    enum ibv_qp_type type, struct hns_roce_qp *qp);
int hns_roce_store_qp(struct hns_roce_context *ctx, uint32_t qpn, struct hns_roce_qp *qp);
void hns_roce_free_qp_buf(struct hns_roce_qp *qp);

#endif /* _HNS_ROCE_U_STDIO_H */

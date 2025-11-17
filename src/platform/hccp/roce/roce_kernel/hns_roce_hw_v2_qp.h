/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef _HNS_ROCE_HW_V2_QP_H_
#define _HNS_ROCE_HW_V2_QP_H_

#include "hns_roce_device.h"

int hns_roce_v2_query_qp(struct ib_qp *ibqp, struct ib_qp_attr *qp_attr,
    int qp_attr_mask, struct ib_qp_init_attr *qp_init_attr);
int hns_roce_v2_destroy_qp(struct ib_qp *ibqp, struct ib_udata *udata);
int hns_roce_v2_qp_flow_control_init(struct hns_roce_dev *hr_dev,
    struct hns_roce_qp *hr_qp);
int hns_roce_v2_modify_qp(struct ib_qp *ibqp,
    const struct ib_qp_attr *attr,
    int attr_mask, enum ib_qp_state cur_state,
    enum ib_qp_state new_state);
void hns_roce_v2_cq_set_ci(struct hns_roce_cq *hr_cq, u32 cons_index);

#endif


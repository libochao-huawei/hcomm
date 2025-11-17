/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef _HNS_ROCE_U_HW_V2_SEND_H
#define _HNS_ROCE_U_HW_V2_SEND_H

#include "verbs_exp.h"
int hns_roce_u_v2_post_send(struct ibv_qp *ibvqp, struct ibv_send_wr *wr,
    struct ibv_send_wr **bad_wr);
int hns_roce_u_v2_exp_post_send(struct ibv_qp *ibvqp, struct ibv_send_wr *wr,
    struct ibv_send_wr **bad_wr, struct wr_exp_rsp *exp_rsp);

#endif /* _HNS_ROCE_U_HW_V2_SEND_H */

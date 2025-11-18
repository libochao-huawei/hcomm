/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef _HNS_ROCE_U_HW_V2_CQ_H
#define _HNS_ROCE_U_HW_V2_CQ_H

struct cqe_wc_status {
    unsigned int cqe_status;
    unsigned int wc_status;
};
int hns_roce_u_v2_poll_cq(struct ibv_cq *ibvcq, int ne,
    struct ibv_wc *wc);
int hns_roce_u_v2_arm_cq(struct ibv_cq *ibvcq, int solicited);

#endif /* _HNS_ROCE_U_HW_V2_CQ_H */

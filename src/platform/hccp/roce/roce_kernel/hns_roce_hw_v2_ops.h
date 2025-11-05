/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef _HNS_ROCE_HW_V2_OPS_H_
#define _HNS_ROCE_HW_V2_OPS_H_

#include "hns_roce_device.h"

int hns_roce_v2_post_send(struct ib_qp *ibqp, const struct ib_send_wr *wr,
    const struct ib_send_wr **bad_wr);

int hns_roce_v2_post_recv(struct ib_qp *ibqp,
    const struct ib_recv_wr *wr,
    const struct ib_recv_wr **bad_wr);
int hns_roce_v2_cmq_init(struct hns_roce_dev *hr_dev);
void hns_roce_v2_cmq_exit(struct hns_roce_dev *hr_dev);
void hns_roce_function_clear(struct hns_roce_dev *hr_dev);
int hns_roce_v2_profile(struct hns_roce_dev *hr_dev);
void hns_roce_v2_cmq_exit(struct hns_roce_dev *hr_dev);
int hns_roce_v2_rst_process_cmd(struct hns_roce_dev *hr_dev);
void hns_roce_query_func_info(struct hns_roce_dev *hr_dev);

#endif


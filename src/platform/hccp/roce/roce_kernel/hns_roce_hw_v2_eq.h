/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef _HNS_ROCE_HW_V2_EQ_H_
#define _HNS_ROCE_HW_V2_EQ_H_

#include "hns_roce_device.h"
#include "hns_roce_hw_v2_wqe.h"

void hns_roce_v2_cleanup_eq_table(struct hns_roce_dev *hr_dev);
int hns_roce_v2_reset_atu_table(struct hns_roce_dev *hr_dev);
int hns_roce_init_link_table(struct hns_roce_dev *hr_dev, enum hns_roce_link_table_type type);
void hns_roce_free_link_table(struct hns_roce_dev *hr_dev,
    struct hns_roce_link_table *link_tbl);
int hns_roce_init_gdr(struct hns_roce_dev *hr_dev);
int hns_roce_init_so_reg(struct hns_roce_dev *hr_dev);
int hns_roce_v2_init_eq_table(struct hns_roce_dev *hr_dev);
int hns_roce_v2_uar_init(struct hns_roce_dev *hr_dev);
void hns_roce_v2_uar_free(struct hns_roce_dev *hr_dev);
int create_flush_workqueue(struct hns_roce_dev *hr_dev);
int hns_roce_v2_init_timer_table(struct hns_roce_dev *hr_dev);

#endif


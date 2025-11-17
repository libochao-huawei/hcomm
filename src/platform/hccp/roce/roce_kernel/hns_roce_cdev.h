/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef HNS_ROCE_KERNEL_CDEV_H
#define HNS_ROCE_KERNEL_CDEV_H

#include "hns_roce_device.h"

struct roce_set_tsqp_depth_data {
    unsigned int rdev_index;
    unsigned int temp_depth;
    unsigned int sq_depth;
    unsigned int qp_num;
};

struct roce_get_tsqp_depth_data {
    unsigned int rdev_index;
    unsigned int temp_depth;
    unsigned int sq_depth;
    unsigned int qp_num;
};

struct roce_dev_data {
    unsigned int rdev_index;
    unsigned int reserved;
};

#define INDEX_LEN 11
#define INFO_PAYLOAD_LEN 1000
struct hns_roce_qpc_stat {
    char index[INDEX_LEN];
    char info[INFO_PAYLOAD_LEN];
};

#define ROCE_IOCTL_MAGIC   'R'
#define ROCE_CMD_SET_TSQP_DEPTH    _IO(ROCE_IOCTL_MAGIC, 1)
#define ROCE_CMD_GET_TSQP_DEPTH    _IO(ROCE_IOCTL_MAGIC, 2)
#define ROCE_CMD_GET_ROCE_DEV_INFO _IO(ROCE_IOCTL_MAGIC, 3)
#define ROCE_CMD_GET_ROCE_QPC_STAT _IO(ROCE_IOCTL_MAGIC, 4)

int roce_init_cdev(struct hns_roce_dev *hr_dev);
void roce_remove_cdev(struct hns_roce_dev *hr_dev);

#endif
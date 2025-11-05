/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef RS_PING_H
#define RS_PING_H

#include "ra_rs_comm.h"
#include "hccp_ping.h"

RS_ATTRI_VISI_DEF int rs_ping_handle_init(unsigned int chip_id, int hdc_type, unsigned int white_list_status);
RS_ATTRI_VISI_DEF int rs_ping_handle_deinit(unsigned int chip_id);
RS_ATTRI_VISI_DEF int rs_ping_init(struct ping_init_attr *attr, struct ping_init_info *info,
    unsigned int *dev_index);
RS_ATTRI_VISI_DEF int rs_ping_target_add(struct ra_rs_dev_info *rdev, struct ping_target_info *target);
RS_ATTRI_VISI_DEF int rs_ping_task_start(struct ra_rs_dev_info *rdev, struct ping_task_attr *attr);
RS_ATTRI_VISI_DEF int rs_ping_get_results(struct ra_rs_dev_info *rdev, struct ping_target_comm_info target[],
    unsigned int *num, struct ping_result_info result[]);
RS_ATTRI_VISI_DEF int rs_ping_task_stop(struct ra_rs_dev_info *rdev);
RS_ATTRI_VISI_DEF int rs_ping_target_del(struct ra_rs_dev_info *rdev, struct ping_target_comm_info target[],
    unsigned int *num);
RS_ATTRI_VISI_DEF int rs_ping_deinit(struct ra_rs_dev_info *rdev);
#endif // RS_PING_H

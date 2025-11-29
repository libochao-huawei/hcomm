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

RS_ATTRI_VISI_DEF int RsPingHandleInit(unsigned int chipId, int hdcType, unsigned int whiteListStatus);
RS_ATTRI_VISI_DEF int RsPingHandleDeinit(unsigned int chipId);
RS_ATTRI_VISI_DEF int RsPingInit(struct ping_init_attr *attr, struct ping_init_info *info,
    unsigned int *devIndex);
RS_ATTRI_VISI_DEF int RsPingTargetAdd(struct ra_rs_dev_info *rdev, struct ping_target_info *target);
RS_ATTRI_VISI_DEF int RsPingTaskStart(struct ra_rs_dev_info *rdev, struct ping_task_attr *attr);
RS_ATTRI_VISI_DEF int RsPingGetResults(struct ra_rs_dev_info *rdev, struct ping_target_comm_info target[],
    unsigned int *num, struct ping_result_info result[]);
RS_ATTRI_VISI_DEF int RsPingTaskStop(struct ra_rs_dev_info *rdev);
RS_ATTRI_VISI_DEF int RsPingTargetDel(struct ra_rs_dev_info *rdev, struct ping_target_comm_info target[],
    unsigned int *num);
RS_ATTRI_VISI_DEF int RsPingDeinit(struct ra_rs_dev_info *rdev);
#endif // RS_PING_H

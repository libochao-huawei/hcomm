/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef DEV_COLL_COMM_C_ADPT_H
#define DEV_COLL_COMM_C_ADPT_H

#include "hccl/hccl_comm.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @note 职责：设备侧集合通信的通信域管理的C接口声明
 */

/**
 * @brief 获取通信域状态
 * @param commId 通信域ID
 * @param status 通信域状态
 * @return HCCL_SUCCESS 成功
 * @return 其他值 失败
 */
HcclResult HcclCommGetStatus(const char* commId, HcclCommStatus *status);

#ifdef __cplusplus
}
#endif

#endif // DEV_COLL_COMM_C_ADPT_H
/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef CCU_DATA_API_H
#define CCU_DATA_API_H

#include <stdint.h>

#include "hccl_types.h"
#include "hcomm_primitives.h"

#include "ccu_types.h"
#include "ccu_data_resource.h"

#ifdef __cplusplus
#include "ccu_loop_macro.h"
class CcuVariable;
#endif

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/**
 * @defgroup ccu数据面编程接口
 * @{
 */




/**
 * @brief 远端同步操作
 * @param[in] channel 链路句柄
 * 
 * @return HcclResult 执行结果状态码
 * @note x
 * @warning
 */
// extern CcuResult CcuNotifyRecord(ChannelHandle channel, uint32_t remoteNotifyIdx, uint32_t mask);

/**
 * @brief 远端同步操作
 * @param[in] channel 链路句柄
 * 
 * @return HcclResult 执行结果状态码
 * @note x
 * @warning
 */
// extern CcuResult CcuNotifyWait(ChannelHandle channel, uint32_t localNotifyIdx, uint32_t mask);


#ifdef __cplusplus
}
#endif // __cplusplus

#endif // CCU_DATA_API_H
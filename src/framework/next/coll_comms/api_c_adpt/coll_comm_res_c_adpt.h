/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef COLL_COMM_RES_C_ADPT_H
#define COLL_COMM_RES_C_ADPT_H

#include <cstdint>
#include "hccl/hccl_res.h"

constexpr uint32_t HCCL_CHANNEL_VERSION_ONE = 1;
constexpr uint32_t MULTIPLE = 4;               // 用于A5判断TC是否为4的倍数
constexpr uint32_t TC_MAX = 255;               // TC的最大值（不区分芯片类型）
constexpr uint32_t RETRY_INTERVAL_MIN = 5u;    // retryInterval范围的最小值（不区分芯片类型）
constexpr uint32_t A5_RETRY_INTERVAL_MAX = 24u;// A5的retryInterval范围的最大值
constexpr uint32_t RETRY_CNT_MIN = 1u;         // retryCnt范围的最小值（不区分芯片类型）
constexpr uint32_t RETRY_CNT_MAX = 7u;         // retryCnt范围的最大值（不区分芯片类型）
constexpr uint32_t SL_MAX = 7u;                // sl范围的最大值，sl即serviceLevel（不区分芯片类型）
constexpr uint32_t TC_DEFAULT = 0xFFFFFFFFu;   // TC的默认值（不区分芯片类型）
constexpr uint32_t SL_DEFAULT = 0xFFFFFFFFu;   // SL的默认值（不区分芯片类型）

#ifdef __cplusplus
extern "C" {
#endif

HcclResult ProcessHcclResPackReq(const HcclChannelDesc &channelDesc, HcclChannelDesc &channelDescFinal, hccl::hcclComm *hcclComm)

/**
 * @note 职责：集合通信的通信域资源管理的C接口声明（暂未对外的接口）
 */

/**
 * @note 非对外接口声明示例
 * @code {.c}
 * extern HcclResult HcclThreadAcquire(HcclComm comm, CommEngine engine, uint32_t threadNum,
 *     uint32_t notifyNumPerThread, ThreadHandle *threads);
 * @endcode
 */

#ifdef __cplusplus
}
#endif

#endif // COLL_COMM_RES_C_ADPT_H

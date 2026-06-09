/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef TP_QOS_H
#define TP_QOS_H

#include <cstdint>

#include "hccp_tp.h"

namespace hcomm {

/// QoS→TP→SL 策略输入；与 GetTpInfoParam / RaUbGetTpInfoParam 中策略相关字段对齐。
struct TpQosInput {
    /// 与 TpProtocol 枚举序一致：0=CTP，1=RTP/TP，2=UBOE
    uint32_t tpProtocol{0U};
    uint32_t qos{0U};
    uint32_t slLevelCount{0U};
    bool loopback{false};
};

namespace TpQos {

constexpr uint32_t kProtoUboe = 2U;

/// 从 GetTpInfoParam / RaUbGetTpInfoParam 策略字段构造输入。
inline TpQosInput MakeInput(uint32_t tpProtocol, uint32_t qos, uint32_t slLevelCount, bool loopback)
{
    return TpQosInput{tpProtocol, qos, slLevelCount, loopback};
}

uint32_t CountSlTiers(uint32_t slMask);
uint16_t ReadSlMask(const struct TpAttr &attr);

/// hcclQos + slBitmap → tpListIndex + mappedSl
bool Map(uint32_t nTp, uint16_t slMask, const TpQosInput &input, uint32_t &tpListIndexOut, uint32_t &mappedSlOut);

/// UBOE SetTpAttr 查 DSCP 时使用的 qos（8-TP 一对一用请求 qos；共 TP 用组内首个 qos）
uint8_t DscpLookupQos(uint32_t nTp, uint16_t slMask, const TpQosInput &input);

} // namespace TpQos
} // namespace hcomm

#endif // TP_QOS_H

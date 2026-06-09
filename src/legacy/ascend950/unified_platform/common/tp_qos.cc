/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "tp_qos.h"

#include <algorithm>

#include "hccl_common.h"

namespace hcomm {
namespace TpQos {
namespace {

constexpr uint32_t kUboeEightTpCount = 8U;

uint32_t SlAtRank(uint32_t mask, uint32_t rank)
{
    uint32_t seen = 0;
    for (uint32_t bit = 0; bit < 16U; ++bit) {
        if ((mask & (1U << bit)) != 0U) {
            if (seen == rank) {
                return bit;
            }
            ++seen;
        }
    }
    return 0;
}

uint32_t EffectiveSlTierCount(uint16_t slMask, uint32_t slLevelCount)
{
    uint32_t cnt = CountSlTiers(slMask);
    if (slLevelCount != 0U) {
        cnt = std::min(slLevelCount, cnt);
    }
    return cnt;
}

/// UBOE 8-TP：hcclQos 高→slBitmap 低档；slAvailableCnt>=3 时 3:2:3 分档。
uint32_t MapUboe8TpSl(uint32_t qos, uint16_t slMask, uint32_t slTierCnt)
{
    const uint32_t q = qos & 7U;
    if (slTierCnt == 0U) {
        return 0U;
    }
    if (slTierCnt == 1U) {
        return SlAtRank(slMask, 0U);
    }
    if (slTierCnt == 2U) {
        const uint32_t slRank = (q >= 4U) ? 0U : 1U;
        return SlAtRank(slMask, slRank);
    }
    uint32_t slRank = 0U;
    if (q >= 5U) {
        slRank = 0U;
    } else if (q >= 3U) {
        slRank = (slTierCnt - 1U) / 2U;
    } else {
        slRank = slTierCnt - 1U;
    }
    if (slRank >= slTierCnt) {
        slRank = slTierCnt - 1U;
    }
    return SlAtRank(slMask, slRank);
}

bool MapLoopback(uint32_t nTp, uint16_t slMask, uint32_t slRawCnt, uint32_t slTierCnt, uint32_t &tpListIndexOut,
    uint32_t &mappedSlOut)
{
    (void)nTp;
    (void)slRawCnt;
    (void)slTierCnt;
    tpListIndexOut = 0U;
    mappedSlOut = SlAtRank(slMask, 0U);
    HCCL_INFO("[TpQos][MapLoopback] slMask[0x%x] tpListIdx[0] mappedSl[%u].", static_cast<unsigned>(slMask),
        static_cast<unsigned>(mappedSlOut & 0xFU));
    return true;
}

bool MapGrouped(const TpQosInput &input, uint32_t nTp, uint16_t slMask, uint32_t slRawCnt, uint32_t slTierCnt,
    uint32_t &tpListIndexOut, uint32_t &mappedSlOut)
{
    if (nTp == 0U || slTierCnt == 0U) {
        HCCL_ERROR("[TpQos][MapGrouped] nTp or slTierCnt zero: nTp[%u] slTierCnt[%u] slMask[0x%x] qos[%u].", nTp,
            slTierCnt, static_cast<unsigned>(slMask), input.qos & 7U);
        return false;
    }
    const uint32_t k = std::min(nTp, slTierCnt);
    if (k == 0U) {
        HCCL_ERROR("[TpQos][MapGrouped] k is zero: nTp[%u] slTierCnt[%u] slMask[0x%x] qos[%u].", nTp, slTierCnt,
            static_cast<unsigned>(slMask), input.qos & 7U);
        return false;
    }
    const uint32_t numGroups = std::min(8U, k);
    const uint32_t qos = input.qos & 7U;
    const uint32_t groupIdx =
        (k == 3U) ? (qos < 3U ? 0U : (qos < 5U ? 1U : 2U)) : ((qos * numGroups) / 8U);
    const uint32_t slotIdx = (groupIdx * k) / numGroups;
    if (slotIdx >= k || slotIdx >= nTp) {
        HCCL_ERROR("[TpQos][MapGrouped] slotIdx out of range: nTp[%u] slRawCnt[%u] slTierCnt[%u] k[%u] "
                   "numGroups[%u] qos[%u] groupIdx[%u] slotIdx[%u] slMask[0x%x].",
            nTp, slRawCnt, slTierCnt, k, numGroups, qos, groupIdx, slotIdx, static_cast<unsigned>(slMask));
        return false;
    }
    const uint32_t slRank = (slTierCnt - 1U) - slotIdx;
    if (slRank >= slTierCnt) {
        HCCL_ERROR("[TpQos][MapGrouped] slRank out of range: nTp[%u] slTierCnt[%u] k[%u] slRank[%u] slMask[0x%x].",
            nTp, slTierCnt, k, slRank, static_cast<unsigned>(slMask));
        return false;
    }
    tpListIndexOut = (k - 1U) - slotIdx;
    mappedSlOut = SlAtRank(slMask, slRank);
    return true;
}

bool MapGeneral(const TpQosInput &input, uint32_t nTp, uint16_t slMask, uint32_t &tpListIndexOut,
    uint32_t &mappedSlOut)
{
    const uint32_t slRawCnt = CountSlTiers(slMask);
    uint32_t slTierCnt = slRawCnt;
    if (slTierCnt == 0U) {
        HCCL_ERROR("[TpQos][MapGeneral] slMask empty: nTp[%u] slMask[0x%x] qos[%u].", nTp,
            static_cast<unsigned>(slMask), input.qos & 7U);
        return false;
    }
    if (input.slLevelCount != 0U) {
        slTierCnt = std::min(input.slLevelCount, slTierCnt);
    }
    if (input.loopback) {
        return MapLoopback(nTp, slMask, slRawCnt, slTierCnt, tpListIndexOut, mappedSlOut);
    }
    if (slTierCnt == 1U) {
        if (nTp == 0U) {
            HCCL_ERROR("[TpQos][MapGeneral] nTp zero with single SL tier: slMask[0x%x] qos[%u].",
                static_cast<unsigned>(slMask), input.qos & 7U);
            return false;
        }
        tpListIndexOut = 0U;
        mappedSlOut = SlAtRank(slMask, 0U);
        return true;
    }
    return MapGrouped(input, nTp, slMask, slRawCnt, slTierCnt, tpListIndexOut, mappedSlOut);
}

bool TryMapUboe8Tp(const TpQosInput &input, uint32_t nTp, uint16_t slMask, uint32_t &tpListIndexOut,
    uint32_t &mappedSlOut)
{
    if (input.tpProtocol != kProtoUboe || input.loopback) {
        return false;
    }
    const uint32_t slTierCnt = EffectiveSlTierCount(slMask, input.slLevelCount);
    if (nTp != kUboeEightTpCount || slTierCnt == 0U) {
        return false;
    }
    const uint32_t qos = input.qos & 7U;
    static constexpr uint8_t kTpIndexByQos[8] = {7U, 6U, 5U, 4U, 3U, 2U, 1U, 0U};
    tpListIndexOut = kTpIndexByQos[qos];
    mappedSlOut = MapUboe8TpSl(qos, slMask, slTierCnt);
    HCCL_INFO("[TpQos][TryMapUboe8Tp] qos[%u] tpListIndex[%u] mappedSl[%u] slMask[0x%x] slTierCnt[%u].", qos,
        tpListIndexOut, mappedSlOut, static_cast<unsigned>(slMask), slTierCnt);
    return true;
}

uint32_t GroupFirstQos(uint32_t qos, uint32_t nTp, uint32_t slTierCnt)
{
    const uint32_t q = qos & 7U;
    if (nTp == 0U || slTierCnt == 0U) {
        return q;
    }
    const uint32_t k = std::min(nTp, slTierCnt);
    const uint32_t numGroups = std::min(8U, k);
    const uint32_t groupIdx =
        (k == 3U) ? (q < 3U ? 0U : (q < 5U ? 1U : 2U)) : ((q * numGroups) / 8U);
    if (k == 3U) {
        static constexpr uint8_t kGroupFirstQos[3] = {0U, 3U, 5U};
        return (groupIdx < 3U) ? static_cast<uint32_t>(kGroupFirstQos[groupIdx]) : 0U;
    }
    for (uint32_t candidate = 0U; candidate <= 7U; ++candidate) {
        if (((candidate * numGroups) / 8U) == groupIdx) {
            return candidate;
        }
    }
    return q;
}

} // namespace

uint32_t CountSlTiers(uint32_t slMask)
{
    uint32_t c = 0;
    for (uint32_t i = 0; i < 16U; ++i) {
        if ((slMask & (1U << i)) != 0U) {
            ++c;
        }
    }
    return c;
}

uint16_t ReadSlMask(const struct TpAttr &attr)
{
    return static_cast<uint16_t>(attr.slBitmap);
}

bool Map(uint32_t nTp, uint16_t slMask, const TpQosInput &input, uint32_t &tpListIndexOut, uint32_t &mappedSlOut)
{
    if (TryMapUboe8Tp(input, nTp, slMask, tpListIndexOut, mappedSlOut)) {
        return true;
    }
    return MapGeneral(input, nTp, slMask, tpListIndexOut, mappedSlOut);
}

uint8_t DscpLookupQos(uint32_t nTp, uint16_t slMask, const TpQosInput &input)
{
    const uint8_t requestQos = static_cast<uint8_t>(input.qos & 0xFFU);
    uint32_t dummyTpIdx = 0U;
    uint32_t dummySl = 0U;
    if (TryMapUboe8Tp(input, nTp, slMask, dummyTpIdx, dummySl)) {
        return requestQos;
    }
    if (input.loopback) {
        return 0U;
    }
    const uint32_t slTierCnt = EffectiveSlTierCount(slMask, input.slLevelCount);
    return static_cast<uint8_t>(GroupFirstQos(input.qos, nTp, slTierCnt));
}

} // namespace TpQos
} // namespace hcomm

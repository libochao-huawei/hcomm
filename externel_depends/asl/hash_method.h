/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef HASH_METHOD_H
#define HASH_METHOD_H

#include "hss_types.h"

namespace Hss {

constexpr u64 HASH_MURMUR_NUM1 = 33;
constexpr u64 HASH_MURMUR_NUM2 = 0xff51afd7ed558ccd;
constexpr u64 HASH_MURMUR_NUM3 = 0xc4ceb9fe1a85ec53;
constexpr u8 ONE_BIT_SHIFT = 1;
constexpr u8 TWO_BIT_SHIFT = 2;
constexpr u8 FOUR_BIT_SHIFT = 4;
constexpr u8 EIGHT_BIT_SHIFT = 8;
constexpr u8 SIXTEEN_BIT_SHIFT = 16;
constexpr u8 THIRTY_TWO_BIT_SHIFT = 32;
constexpr u64 POWER_BASE_NUM_TWO = 2;

inline bool IsPowerOfTwo(u64 n)
{
    if (n < ONE_BIT_SHIFT) {
        return false;
    } else if ((n & (n - ONE_BIT_SHIFT)) == 0) {
        return true;
    }
    return false;
}

inline u64 RoundUpPowerOf2(u64 tableSize)
{
    tableSize--;
    tableSize |= tableSize >> ONE_BIT_SHIFT;
    tableSize |= tableSize >> TWO_BIT_SHIFT;
    tableSize |= tableSize >> FOUR_BIT_SHIFT;
    tableSize |= tableSize >> EIGHT_BIT_SHIFT;
    tableSize |= tableSize >> SIXTEEN_BIT_SHIFT;
    tableSize |= tableSize >> THIRTY_TWO_BIT_SHIFT;
    tableSize++;

    return tableSize;
}

inline u64 DichotoIndex(u64 threadSeq, u64 totalSize)
{
    if (threadSeq == 0) {
        return 0;
    }

    if (IsPowerOfTwo(threadSeq)) {
        return totalSize / threadSeq;
    }
    u64 upPowerOf2 = RoundUpPowerOf2(threadSeq);
    return totalSize * (POWER_BASE_NUM_TWO * threadSeq - upPowerOf2 + 1) / upPowerOf2;
}

}
#endif
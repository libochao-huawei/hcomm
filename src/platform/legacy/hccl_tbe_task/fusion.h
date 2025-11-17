/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

/* !
 * \file fusion.h
 * \brief
 */
#ifndef OPS_BUILT_IN_OP_TILING_FUSION_H
#define OPS_BUILT_IN_OP_TILING_FUSION_H

#include <vector>
#include <array>
#include <utility>
#include "auto_tiling_context.h"

namespace TbeReduce {
constexpr size_t B_MAX_DIM_LEN = 16;
constexpr size_t B_MAX_INPUT_NUMS = 70;
constexpr size_t UINT64_BIT_NUM = 64;

template <size_t MAX_SIZE> class BitArray {
public:
    BitArray()
    {
        // std::memset(masks, 0, sizeof(masks));
        masks.fill(0ULL);
    };
    ~BitArray() = default;
    void clear()
    {
        masks.fill(0ULL);
        // std::memset(masks, 0, sizeof(masks));
    };
    bool SetOn(const size_t index)
    {
        if (index >= MAX_SIZE) {
            return false;
        }
        masks[index / UINT64_BIT_NUM] |= (1ULL << (index % UINT64_BIT_NUM));
        return true;
    };
    bool IsOn(const size_t index) const
    {
        if ((masks[index / UINT64_BIT_NUM] & (1ULL << (index % UINT64_BIT_NUM))) == 0ULL) {
            return false;
        }
        return true;
    };
    bool IsAllOn(size_t len) const
    {
        size_t i = 0;
        while (len >= UINT64_BIT_NUM) {
            if (masks[i] != 0xFFFFFFFFFFFFFFFFULL) {
                return false;
            }
            i++;
            len -= UINT64_BIT_NUM;
        }
        if (len > 0) {
            if (masks[i] != ((1ULL << len) - 1)) {
                return false;
            }
        }
        return true;
    };
    bool IsAllOff(size_t len) const
    {
        size_t i = 0;
        while (len > 0) {
            if (masks[i] != 0ULL) {
                return false;
            }
            i++;
            len = (len > UINT64_BIT_NUM) ? (len - UINT64_BIT_NUM) : 0;
        }
        return true;
    };
    bool IsBroadcast(size_t len) const
    {
        return !IsAllOn(len) && !IsAllOff(len);
    };
    bool operator == (const BitArray<MAX_SIZE> &other) const
    {
        size_t len = (MAX_SIZE + UINT64_BIT_NUM - 1) / UINT64_BIT_NUM;
        while (len > 0) {
            len--;
            if (masks[len] != other.masks[len]) {
                return false;
            }
        }
        return true;
    };
    bool operator != (const BitArray<MAX_SIZE> &other) const
    {
        size_t len = (MAX_SIZE + UINT64_BIT_NUM - 1) / UINT64_BIT_NUM;
        while (len > 0) {
            len--;
            if (masks[len] != other.masks[len]) {
                return true;
            }
        }
        return false;
    };
    BitArray<MAX_SIZE> &operator |= (const BitArray<MAX_SIZE> &other)
    {
        size_t len = (MAX_SIZE + UINT64_BIT_NUM - 1) / UINT64_BIT_NUM;
        while (len > 0) {
            len--;
            masks[len] |= other.masks[len];
        }
        return *this;
    };

private:
    std::array<uint64_t, (MAX_SIZE + UINT64_BIT_NUM - 1) / UINT64_BIT_NUM> masks;
};

struct BroadcastAxis {
    int64_t dim;
    BitArray<B_MAX_INPUT_NUMS> mask;
};

class Fusion {
public:
    Fusion()
    {
        dim_len = 0;
        input_num = 0;
        fusion_len = 0;
        max_dims.fill(1LL);
        dim_masks.fill(BitArray<B_MAX_INPUT_NUMS>());
    };
    ~Fusion() = default;
    bool AddShape(const std::vector<int64_t> &shape);
    bool AddShape(const OpShape &shape, const size_t len);
    bool AddShape(const gert::Shape &shape, const size_t len);
    bool ExpandAllShapes(const size_t left);
    bool RefreshShapes(const std::pair<size_t, std::array<BroadcastAxis, B_MAX_DIM_LEN>> &shape_info);
    void FusionShapes(size_t max_len, std::pair<size_t, std::array<BroadcastAxis, B_MAX_DIM_LEN>> &fusion_info,
        std::pair<size_t, std::array<size_t, B_MAX_DIM_LEN>> &out_indexs);
    void GetAllShapes(std::vector<gert::Shape> &shapes) const;
    void GetAllCompleteShapes(std::vector<gert::Shape> &shapes) const;
    size_t FusionByIndex(const std::pair<size_t, std::array<size_t, B_MAX_DIM_LEN>> &indexes);
    size_t FusionByLen(size_t max_len);
    int64_t Elewise();
    int64_t GetShapeSize() const;
    int64_t GetInputDim(const size_t input_index, const size_t dim_index) const;
    void GetOutShape(std::pair<size_t, std::array<int64_t, B_MAX_DIM_LEN>> &shape, size_t len) const;
    bool IsBroadcast(const size_t dim_index) const;
    const BitArray<B_MAX_INPUT_NUMS> &GetBitArray(const size_t dim_index) const
    {
        size_t index = B_MAX_DIM_LEN + dim_index - dim_len;
        return dim_masks[index];
    }

private:
    size_t dim_len{};
    size_t input_num{};
    size_t fusion_len{};
    std::array<int64_t, B_MAX_DIM_LEN> max_dims {};
    std::array<BitArray<B_MAX_INPUT_NUMS>, B_MAX_DIM_LEN> dim_masks {};
};
} // namespace optiling
#endif // OPS_BUILT_IN_OP_TILING_FUSION_H
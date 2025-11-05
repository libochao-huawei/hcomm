/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef CONCURRENT_UNIQUE_MAPPER
#define CONCURRENT_UNIQUE_MAPPER

#include <vector>
#include "unique_mapper.h"
#include "hccl_types.h"

namespace hccl {

template <typename T, typename TM>
class ConcurrentUniqueMapper {
public:
    using MtxBuffer = typename UniqueMapper<T, TM>::MtxBuffer;
    using SplitDataEle = typename UniqueMapper<T, TM>::SplitDataEle;
    using ConUniqueMappersInfo = typename UniqueMapper<T, TM>::ConUniqueMappersInfo;

    ConcurrentUniqueMapper()
    {
    }

    ~ConcurrentUniqueMapper()
    {
    }

    HcclResult Cfg(u32 parallelNum, const Buffer<T> &srcData, const Buffer<T> &uniqueData,
        std::function<bool(const u32&)> filtered, const ConUniqueMappersInfo &info)
    {
        if (parallelNum == 0) {
            HCCL_ERROR("parallelNum is 0");
            return HCCL_E_PARA;
        }

        parallelNum_ = parallelNum;
        filtered_ = filtered;
        mappers_.resize(info.maxParallelNum);

        uniqueIdsOffsets_.resize(info.maxParallelNum);

        for (u32 i = 0; i < parallelNum_; i++) {
            auto newFiltered = [i, this] (const u32 &index, const T &data) -> bool {
                u32 val = static_cast<u32>(static_cast<u64>(data) % parallelNum_);

                return filtered_(index) || (i != val);
            };

            mappers_[i].CfgInfo(srcData, uniqueData, newFiltered, info);
        }

        return HCCL_SUCCESS;
    }

    HcclResult CollectUniqueSizeInfo()
    {
        u32 offset = 0;
        for (u32 i = 0; i < parallelNum_; i++) {
            uniqueIdsOffsets_[i] = offset;
            offset += mappers_[i].GetUniqueDataCount();
        }

        uniuqedCount_ = offset;

        return HCCL_SUCCESS;
    }

    u32 GetUniuqedCount()
    {
        u32 cnt = 0;
        for (u32 i = 0; i < parallelNum_; i++) {
            cnt += mappers_[i].GetUniqueDataCount();
        }

        uniuqedCount_ = cnt;

        return uniuqedCount_;
    }

    HcclResult GetMappingMatrixs(MtxBuffer &mappingMatrix)
    {
        for (u32 i = 0; i < parallelNum_; i++) {
            CHK_RET(mappers_[i].GetMappingMatrix(mappingMatrix, uniqueIdsOffsets_[i]));
        }

        return HCCL_SUCCESS;
    }

    UniqueMapper<T, TM> &GetUniqueMapper(u32 idx)
    {
        return mappers_[idx];
    }

    u32 GetUniqueIdOffset(u32 idx)
    {
        return uniqueIdsOffsets_[idx];
    }

    void ClearData()
    {
        uniqueIdsOffsets_.clear();
    }

private:
    static constexpr u32 PARALLEL_NUM_ONE = 1;

    std::vector<UniqueMapper<T, TM>> mappers_{};
    std::vector<u32> uniqueIdsOffsets_{};
    u32 uniuqedCount_{};
    u32 parallelNum_{ PARALLEL_NUM_ONE };
    std::function<bool(const u32&)> filtered_{};
};
}
#endif // CONCURRENT_UNIQUE_MAPPER
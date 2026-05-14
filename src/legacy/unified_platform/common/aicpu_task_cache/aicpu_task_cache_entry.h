/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef HCCLV2_AICPU_TASK_CACHE_ENTRY_H
#define HCCLV2_AICPU_TASK_CACHE_ENTRY_H

#include <cstdint>
#include <vector>

#include "stream_lite.h"

namespace Hccl {

struct AicpuTaskMemRange {
    explicit AicpuTaskMemRange();
    explicit AicpuTaskMemRange(const uint64_t curBaseAddr, const uint64_t curMemSize);
    explicit AicpuTaskMemRange(const AicpuTaskMemRange& other);
    ~AicpuTaskMemRange();

    const AicpuTaskMemRange& operator=(const AicpuTaskMemRange& other);

    HcclResult GetEndAddr(uint64_t& endAddr) const;
    HcclResult InRange(const uint64_t addr, bool& isInRange) const;

    bool isValid;
    uint64_t baseAddr;
    uint64_t memSize;
};

class AicpuTaskCacheEntry {
public:
    AicpuTaskCacheEntry() = delete;
    explicit AicpuTaskCacheEntry(const AicpuTaskMemRange& localUserInputMemRange,
        const AicpuTaskMemRange& localUserOutputMemRange);
    ~AicpuTaskCacheEntry();

    HcclResult GetSqeArrayCount(size_t& sqeArrayCount) const;

    HcclResult AllocSqeArray(const size_t sqeCount, const int32_t streamId, size_t& arrayIdx);
    HcclResult MemcpySqeArray(const size_t arrayIdx, const size_t sqeStartIdx, const size_t sqeCount,
        const uint8_t* sqeArray, const uint8_t* sqeTypeArray, const AicpuDfxInfo* sqeDfxInfoArray);

    HcclResult CalcStreamSeqIdxes(Stream& mainStream, std::vector<Stream>& slaveStreams);

    HcclResult UpdateAndGetSqeArray(const size_t arrayIdx,
        const std::vector<AicpuTaskMemRange>& curUserInputMemRanges,
        const std::vector<AicpuTaskMemRange>& curUserOutputMemRanges, Stream& mainStream,
        std::vector<Stream>& slaveStreams, const uint32_t opRingBufferIdx, size_t& sqeCount,
        uint8_t** sqeArrayPtr, uint8_t** sqeTypeArrayPtr, AicpuDfxInfo** sqeDfxInfoArrayPtr,
        Stream** streamPtrPtr, const bool profL1Enable, std::vector<uint64_t>& profTimestamps);

    HcclResult SetInputOutputMemRanges(const std::vector<AicpuTaskMemRange>& curUserInputMemRanges,
        const std::vector<AicpuTaskMemRange>& curUserOutputMemRanges);

private:
    inline void CombineUint32ToUint64(uint64_t& addr, const uint32_t high, const uint32_t low) const
    {
        constexpr uint64_t uintBitWidth = 32;
        addr = (static_cast<uint64_t>(high) << uintBitWidth) | static_cast<uint64_t>(low);
        return;
    }

    inline void SplitUint64ToUint32(const uint64_t addr, uint32_t& high, uint32_t& low) const
    {
        constexpr uint64_t uintBitWidth = 32;
        high = static_cast<uint32_t>(addr >> uintBitWidth);
        low = static_cast<uint32_t>(addr & 0xFFFFFFFFULL);
        return;
    }

    HcclResult RefreshSqeAddr(uint64_t& sqeAddr, const uint32_t rankId,
        const std::vector<AicpuTaskMemRange>& cachedMemRanges,
        const std::vector<AicpuTaskMemRange>& curMemRanges) const;

    // Current rank's user input/output memory range
    AicpuTaskMemRange localUserInputMemRange_;
    AicpuTaskMemRange localUserOutputMemRange_;

    // 多段SQE数组: 每段SQE数组对应一次LaunchTask, 以及相应的RtsqA5指针
    std::vector<uint8_t*> sqeArrays_;
    std::vector<RtsqA5*> rtsqPtrs_;

    // TODOSSY: === END HERE ===
};

} // namespace Hccl

#endif // HCCLV2_AICPU_TASK_CACHE_ENTRY_H

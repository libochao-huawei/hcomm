/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <cstdint>
#include <cstdlib>

// TODOSSY: === END HERE ===

#include "aicpu_task_cache_entry.h"

#include "aicpu_hccl_sqcq.h"
#include "dispatcher_pub.h"
#include "log.h"
#include "sal.h"

namespace Hccl {

AicpuTaskMemRange::AicpuTaskMemRange() : isValid(false), baseAddr(0), memSize(0)
{
}

AicpuTaskMemRange::AicpuTaskMemRange(const uint64_t curBaseAddr, const uint64_t curMemSize)
    : isValid(true), baseAddr(curBaseAddr), memSize(curMemSize)
{
    CHK_PRT_CONT(curBaseAddr == 0,
        HCCL_ERROR("[AicpuTaskMemRange][AicpuTaskMemRange] curBaseAddr is 0"));
}

AicpuTaskMemRange::AicpuTaskMemRange(const AicpuTaskMemRange& other)
    : isValid(other.isValid), baseAddr(other.baseAddr), memSize(other.memSize)
{
}

AicpuTaskMemRange::~AicpuTaskMemRange()
{
}

const AicpuTaskMemRange& AicpuTaskMemRange::operator=(const AicpuTaskMemRange& other)
{
    if (this != &other) {
        isValid = other.isValid;
        baseAddr = other.baseAddr;
        memSize = other.memSize;
    }
    return *this;
}

HcclResult AicpuTaskMemRange::GetEndAddr(uint64_t& endAddr) const
{
    CHK_PRT_RET(baseAddr + memSize < baseAddr,
        HCCL_ERROR("[AicpuTaskMemRange][GetEndAddr] baseAddr[0x%016llx] + "
            "memSize[%llu] overflows", baseAddr, memSize),
        HCCL_E_INTERNAL);

    endAddr = baseAddr + memSize;
    return HCCL_SUCCESS;
}

HcclResult AicpuTaskMemRange::InRange(const uint64_t addr, bool& isInRange) const
{
    uint64_t endAddr = 0;
    CHK_RET(GetEndAddr(endAddr));

    isInRange = (isValid && addr >= baseAddr && addr < endAddr);
    return HCCL_SUCCESS;
}

AicpuTaskCacheEntry::AicpuTaskCacheEntry(
    const std::vector<AicpuTaskMemRange>& userInputMemRanges,
    const std::vector<AicpuTaskMemRange>& userOutputMemRanges)
    : userInputMemRanges_(userInputMemRanges), userOutputMemRanges_(userOutputMemRanges)
{
    HCCL_INFO("[AicpuTaskCacheEntry][AicpuTaskCacheEntry] create a cache entry with "
        "%llu userInputMemRanges and %llu userOutputMemRanges",
        userInputMemRanges_.size(), userOutputMemRanges_.size());
}

AicpuTaskCacheEntry::~AicpuTaskCacheEntry()
{
    size_t sqeArrayCount = sqeArrays_.size();
    size_t totalSqeCount = 0;
    for (size_t arrayIdx = 0; arrayIdx < sqeArrayCount; ++arrayIdx) {
        totalSqeCount += sqeTypeArrays_[arrayIdx].size();

        uint8_t* curSqeArray = sqeArrays_[arrayIdx];
        if (UNLIKELY(curSqeArray == nullptr)) {
            HCCL_ERROR("[AicpuTaskCacheEntry][~AicpuTaskCacheEntry] curSqeArray is nullptr");
        } else {
            free(curSqeArray);
            curSqeArray = nullptr;
        }

        uint8_t* curSqeTypeArray = sqeTypeArrays_[arrayIdx];
        if (UNLIKELY(curSqeTypeArray == nullptr)) {
            HCCL_ERROR("[AicpuTaskCacheEntry][~AicpuTaskCacheEntry] curSqeTypeArray is nullptr");
        } else {
            free(curSqeTypeArray);
            curSqeTypeArray = nullptr;
        }

        AicpuDfxInfo* curSqeDfxInfoArray = sqeDfxInfoArrays_[arrayIdx];
        if (UNLIKELY(curSqeDfxInfoArray == nullptr)) {
            HCCL_ERROR("[AicpuTaskCacheEntry][~AicpuTaskCacheEntry] curSqeDfxInfoArray is nullptr");
        } else {
            free(curSqeDfxInfoArray);
            curSqeDfxInfoArray = nullptr;
        }
    }

    HCCL_INFO("[AicpuTaskCacheEntry][~AicpuTaskCacheEntry] release %u SQE arrays "
        "(%u SQEs in total) from the cache entry", sqeArrayCount, totalSqeCount);
}

HcclResult AicpuTaskCacheEntry::GetSqeArrayCount(size_t& sqeArrayCount) const
{
    sqeArrayCount = sqeArrays_.size();
    CHK_PRT_RET(sqeArrayCount == 0,
        HCCL_ERROR("[AicpuTaskCacheEntry][GetSqeArrayCount] sqeArrayCount is 0"),
        HCCL_E_INTERNAL);
    return HCCL_SUCCESS;
}

HcclResult AicpuTaskCacheEntry::AllocSqeArray(const size_t sqeCount, const int32_t streamId,
    size_t& arrayIdx)
{
    const size_t sqeBytes = sqeCount * HCCL_SQE_SIZE;
    uint8_t* newSqeArray = reinterpret_cast<uint8_t*>(malloc(sqeBytes));
    CHK_PTR_NULL(newSqeArray);
    sqeArrays_.emplace_back(newSqeArray);

    const size_t sqeTypeBytes = sqeCount * sizeof(uint8_t);
    uint8_t* newSqeTypeArray = reinterpret_cast<uint8_t*>(malloc(sqeTypeBytes));
    CHK_PTR_NULL(newSqeTypeArray);
    sqeTypeArrays_.emplace_back(newSqeTypeArray);

    const size_t sqeDfxInfoBytes = sqeCount * sizeof(AicpuDfxInfo);
    AicpuDfxInfo* newSqeDfxInfoArray = reinterpret_cast<AicpuDfxInfo*>(malloc(sqeDfxInfoBytes));
    CHK_PTR_NULL(newSqeDfxInfoArray);
    sqeDfxInfoArrays_.emplace_back(newSqeDfxInfoArray);

    CHK_PRT_RET(streamId < 0,
        HCCL_ERROR("[AicpuTaskCacheEntry][AllocSqeArray] streamId %d < 0", streamId),
        HCCL_E_INTERNAL);
    streamIds_.emplace_back(streamId);

    arrayIdx = sqeArrays_.size() - 1;

    HCCL_INFO("[AicpuTaskCacheEntry][AllocSqeArray] allocate %uth sqe array with "
        "sqeCount of %u and streamId of %d", arrayIdx, sqeCount, streamId);

    return HCCL_SUCCESS;
}

HcclResult AicpuTaskCacheEntry::MemcpySqeArray(const size_t arrayIdx, const size_t sqeStartIdx,
    const size_t sqeCount, const uint8_t* sqeArray, const uint8_t* sqeTypeArray,
    const AicpuDfxInfo* sqeDfxInfoArray)
{
    CHK_PRT_RET(arrayIdx >= sqeArrays_.size(),
        HCCL_ERROR("[AicpuTaskCacheEntry][MemcpySqeArray] arrayIdx %u is out of range [0, %u)",
            arrayIdx, sqeArrays_.size()),
        HCCL_E_INTERNAL);
    const size_t totalSqeCount = sqeTypeArrays_[arrayIdx].size();
    CHK_PRT_RET(sqeStartIdx + sqeCount - 1 >= totalSqeCount,
        HCCL_ERROR("[AicpuTaskCacheEntry][MemcpySqeArray] sqeStartIdx %u + sqeCount %u - 1 "
            "is out of range [0, %u)", sqeStartIdx, sqeCount, totalSqeCount),
        HCCL_E_INTERNAL);
    CHK_PTR_NULL(sqeArray);
    CHK_PTR_NULL(sqeTypeArray);
    CHK_PTR_NULL(sqeDfxInfoArray);

    HCCL_INFO("[AicpuTaskCacheEntry][MemcpySqeArray] memcpy %uth sqe array[%u:%u]",
        arrayIdx, sqeStartIdx, sqeStartIdx + sqeCount - 1);

    const size_t sqeBytes = sqeCount * HCCL_SQE_SIZE;
    uint8_t* dstSqeArray = sqeArrays_[arrayIdx];
    CHK_PTR_NULL(dstSqeArray);
    CHK_SAFETY_FUNC_RET(memcpy_s(dstSqeArray + sqeStartIdx * HCCL_SQE_SIZE,
        (totalSqeCount - sqeStartIdx) * HCCL_SQE_SIZE, sqeArray, sqeBytes));

    const size_t sqeTypeBytes = sqeCount * sizeof(uint8_t);
    uint8_t* dstSqeTypeArray = sqeTypeArrays_[arrayIdx];
    CHK_PTR_NULL(dstSqeTypeArray);
    CHK_SAFETY_FUNC_RET(memcpy_s(dstSqeTypeArray + sqeStartIdx,
        (totalSqeCount - sqeStartIdx) * sizeof(uint8_t), sqeTypeArray, sqeTypeBytes));

    const size_t sqeDfxInfoBytes = sqeCount * sizeof(AicpuDfxInfo);
    AicpuDfxInfo* dstSqeDfxInfoArray = sqeDfxInfoArrays_[arrayIdx];
    CHK_PTR_NULL(dstSqeDfxInfoArray);
    CHK_SAFETY_FUNC_RET(memcpy_s(dstSqeDfxInfoArray + sqeStartIdx,
        (totalSqeCount - sqeStartIdx) * sizeof(AicpuDfxInfo), sqeDfxInfoArray, sqeDfxInfoBytes));

    return HCCL_SUCCESS;
}

HcclResult AicpuTaskCacheEntry::CalcStreamSeqIdxes(Stream& mainStream,
    std::vector<Stream>& slaveStreams)
{
    const size_t streamIdCount = streamIds_.size();
    const int32_t mainStreamId = mainStream.id();
    const size_t slaveStreamCount = slaveStreams.size();

    streamSeqIdxes_.resize(streamIdCount);

    for (size_t arrayIdx = 0; arrayIdx < streamIdCount; ++arrayIdx) {
        const int32_t curStreamId = streamIds_[arrayIdx];
        if (curStreamId == mainStreamId) {
            streamSeqIdxes_[arrayIdx] = 0;
        } else {
            bool found = false;
            for (size_t slaveIdx = 0; slaveIdx < slaveStreamCount; ++slaveIdx) {
                if (slaveStreams[slaveIdx].id() == curStreamId) {
                    streamSeqIdxes_[arrayIdx] = static_cast<uint32_t>(slaveIdx + 1);
                    found = true;
                    break;
                }
            }
            CHK_PRT_RET(!found,
                HCCL_ERROR("[AicpuTaskCacheEntry][CalcStreamSeqIdxes] streamId %d not found",
                    curStreamId),
                HCCL_E_INTERNAL);
        }
        HCCL_INFO("[AicpuTaskCacheEntry][CalcStreamSeqIdxes] set streamSeqIdx of %uth "
            "sqe array as %u", arrayIdx, streamSeqIdxes_[arrayIdx]);
    }

    return HCCL_SUCCESS;
}

HcclResult AicpuTaskCacheEntry::UpdateAndGetSqeArray(const size_t arrayIdx,
    const std::vector<AicpuTaskMemRange>& curUserInputMemRanges,
    const std::vector<AicpuTaskMemRange>& curUserOutputMemRanges, Stream& mainStream,
    std::vector<Stream>& slaveStreams, const uint32_t opRingBufferIdx, size_t& sqeCount,
    uint8_t** sqeArrayPtr, uint8_t** sqeTypeArrayPtr, AicpuDfxInfo** sqeDfxInfoArrayPtr,
    Stream** streamPtrPtr, const bool profL1Enable, std::vector<uint64_t>& profTimestamps)
{
    CHK_PRT_RET(arrayIdx >= sqeArrays_.size(),
        HCCL_ERROR("[AicpuTaskCacheEntry][UpdateAndGetSqeArray] arrayIdx %u is out of range",
            arrayIdx),
        HCCL_E_INTERNAL);

    const uint32_t streamSeqIdx = streamSeqIdxes_[arrayIdx];
    if (streamSeqIdx == 0) {
        *streamPtrPtr = &mainStream;
    } else {
        CHK_PRT_RET(streamSeqIdx > slaveStreams.size(),
            HCCL_ERROR("[AicpuTaskCacheEntry][UpdateAndGetSqeArray] streamSeqIdx %u is out of range",
                streamSeqIdx),
            HCCL_E_INTERNAL);
        *streamPtrPtr = &(slaveStreams[streamSeqIdx - 1]);
    }

    *sqeArrayPtr = sqeArrays_[arrayIdx];
    *sqeTypeArrayPtr = sqeTypeArrays_[arrayIdx];
    *sqeDfxInfoArrayPtr = sqeDfxInfoArrays_[arrayIdx];
    sqeCount = sqeTypeArrays_[arrayIdx].size();

    CHK_PTR_NULL(*sqeArrayPtr);
    CHK_PTR_NULL(*sqeTypeArrayPtr);
    CHK_PTR_NULL(*sqeDfxInfoArrayPtr);

    HCCL_INFO("[AicpuTaskCacheEntry][UpdateAndGetSqeArray] get %uth sqe array with "
        "sqeCount of %u and streamId of %d", arrayIdx, sqeCount, streamIds_[arrayIdx]);

    return HCCL_SUCCESS;
}

HcclResult AicpuTaskCacheEntry::SetInputOutputMemRanges(
    const std::vector<AicpuTaskMemRange>& curUserInputMemRanges,
    const std::vector<AicpuTaskMemRange>& curUserOutputMemRanges)
{
    userInputMemRanges_ = curUserInputMemRanges;
    userOutputMemRanges_ = curUserOutputMemRanges;

    HCCL_INFO("[AicpuTaskCacheEntry][SetInputOutputMemRanges] set %llu userInputMemRanges "
        "and %llu userOutputMemRanges", userInputMemRanges_.size(), userOutputMemRanges_.size());

    return HCCL_SUCCESS;
}

HcclResult AicpuTaskCacheEntry::RefreshSqeAddr(uint64_t& sqeAddr, const uint32_t rankId,
    const std::vector<AicpuTaskMemRange>& cachedMemRanges,
    const std::vector<AicpuTaskMemRange>& curMemRanges) const
{
    return HCCL_SUCCESS;
}

} // namespace Hccl

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

#include "aicpu_task_cache_entry.h"

#include "log.h"

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

AddrRefreshInfo::AddrRefreshInfo() : rankId(0), memType(CCL_MEMTYPE)
{
}

AddrRefreshInfo::AddrRefreshInfo(const uint32_t curRankId, const uint8_t curMemType)
    : rankId(curRankId), memType(curMemType)
{
}

AddrRefreshInfo::AddrRefreshInfo(const AddrRefreshInfo& other)
    : rankId(other.rankId), memType(other.memType)
{
}

AddrRefreshInfo::~AddrRefreshInfo()
{
}

const AddrRefreshInfo& AddrRefreshInfo::operator=(const AddrRefreshInfo& other)
{
    if (this != &other) {
        rankId = other.rankId;
        memType = other.memType;
    }
    return *this;
}

AicpuTaskCacheEntry::AicpuTaskCacheEntry(const AicpuTaskMemRange& localUserInputMemRange,
    const AicpuTaskMemRange& localUserOutputMemRange)
    : localUserInputMemRange_(localUserInputMemRange),
      localUserOutputMemRange_(localUserOutputMemRange)
{
    HCCL_INFO("[AicpuTaskCacheEntry][AicpuTaskCacheEntry] create a cache entry with "
        "localUserInputMemRange[0x%016llx, 0x%016llx) and localUserOutputMemRange[0x%016llx, 0x%016llx)",
        localUserInputMemRange_.baseAddr, localUserInputMemRange_.baseAddr + localUserInputMemRange_.memSize,
        localUserOutputMemRange_.baseAddr, localUserOutputMemRange_.baseAddr + localUserOutputMemRange_.memSize);
}

AicpuTaskCacheEntry::~AicpuTaskCacheEntry()
{
    size_t sqeArrayCount = sqeArrays_.size();
    size_t totalSqeCount = 0;
    for (size_t arrayIdx = 0; arrayIdx < sqeArrayCount; ++arrayIdx) {
        totalSqeCount += sqeSrcInfoArrays_[arrayIdx].size();

        uint8_t* curSqeArray = sqeArrays_[arrayIdx];
        if (UNLIKELY(curSqeArray == nullptr)) {
            HCCL_ERROR("[AicpuTaskCacheEntry][~AicpuTaskCacheEntry] curSqeArray is nullptr");
        } else {
            free(curSqeArray);
            curSqeArray = nullptr;
        }
    }

    size_t wqeArrayCount = wqeTaskArrays_.size();
    size_t totalWqeCount = 0;
    for (size_t arrayIdx = 0; arrayIdx < wqeArrayCount; ++arrayIdx) {
        totalWqeCount += wqeTaskArrays_[arrayIdx].size();
    }

    HCCL_INFO("[AicpuTaskCacheEntry][~AicpuTaskCacheEntry] release %u SQE arrays "
        "(%u SQEs in total) and %u WQE arrays (%u WQEs in total) from the cache entry",
        sqeArrayCount, totalSqeCount, wqeArrayCount, totalWqeCount);
}

// TODOSSY: === END HERE ===

HcclResult AicpuTaskCacheEntry::AddSqeArray(RtsqA5* rtsqPtr, const size_t sqeCount,
    const uint8_t* sqeArray)
{
    CHK_PTR_NULL(rtsqPtr);
    CHK_PTR_NULL(sqeArray);
    CHK_PRT_RET(sqeCount == 0,
        HCCL_ERROR("[AicpuTaskCacheEntry][AddSqeArray] sqeCount is 0"),
        HCCL_E_INTERNAL);

    const size_t sqeBytes = sqeCount * sizeof(Rt91095StarsSqe);
    uint8_t* newSqeArray = reinterpret_cast<uint8_t*>(malloc(sqeBytes));
    CHK_PTR_NULL(newSqeArray);
    CHK_SAFETY_FUNC_RET(memcpy_s(newSqeArray, sqeBytes, sqeArray, sqeBytes));

    sqeArrays_.emplace_back(newSqeArray);
    rtsqPtrs_.emplace_back(rtsqPtr);
    sqeSrcInfoArrays_.emplace_back(sqeCount);
    sqeDstInfoArrays_.emplace_back(sqeCount);
    launchOrder_.emplace_back(TaskArrayType::kTaskArrayTypeSqe);

    HCCL_INFO("[AicpuTaskCacheEntry][AddSqeArray] add %uth sqe array with sqeCount of %u",
        sqeArrays_.size() - 1, sqeCount);

    return HCCL_SUCCESS;
}

HcclResult AicpuTaskCacheEntry::AddWqeArray(UbConnLite* ubConnLitePtr, const UdmaSqeRead& wqeRead,
    const uint32_t dbSqeIdx)
{
    CHK_PTR_NULL(ubConnLitePtr);

    std::vector<WqeTask> wqeTaskArray(1);
    wqeTaskArray[0].wqeRead = wqeRead;
    wqeTaskArrays_.emplace_back(wqeTaskArray);
    ubConnLitePtrs_.emplace_back(ubConnLitePtr);

    DbSqeLocation dbSqeLocation;
    dbSqeLocation.sqeArrayIdx = static_cast<uint32_t>(sqeArrays_.size() - 1);
    dbSqeLocation.dbSqeIdx = dbSqeIdx;
    dbSqeLocations_.emplace_back(dbSqeLocation);

    AddrRefreshInfo srcInfo(0, AddrRefreshInfo::CCL_MEMTYPE);
    AddrRefreshInfo dstInfo(0, AddrRefreshInfo::CCL_MEMTYPE);
    wqeSrcInfoArrays_.emplace_back(srcInfo);
    wqeDstInfoArrays_.emplace_back(dstInfo);

    launchOrder_.emplace_back(TaskArrayType::kTaskArrayTypeWqe);

    HCCL_INFO("[AicpuTaskCacheEntry][AddWqeArray] add %uth wqe array (UdmaSqeRead)",
        wqeTaskArrays_.size() - 1);

    return HCCL_SUCCESS;
}

HcclResult AicpuTaskCacheEntry::AddWqeArray(UbConnLite* ubConnLitePtr, const UdmaSqeWrite& wqeWrite,
    const uint32_t dbSqeIdx)
{
    CHK_PTR_NULL(ubConnLitePtr);

    std::vector<WqeTask> wqeTaskArray(1);
    wqeTaskArray[0].wqeWrite = wqeWrite;
    wqeTaskArrays_.emplace_back(wqeTaskArray);
    ubConnLitePtrs_.emplace_back(ubConnLitePtr);

    DbSqeLocation dbSqeLocation;
    dbSqeLocation.sqeArrayIdx = static_cast<uint32_t>(sqeArrays_.size() - 1);
    dbSqeLocation.dbSqeIdx = dbSqeIdx;
    dbSqeLocations_.emplace_back(dbSqeLocation);

    AddrRefreshInfo srcInfo(0, AddrRefreshInfo::CCL_MEMTYPE);
    AddrRefreshInfo dstInfo(0, AddrRefreshInfo::CCL_MEMTYPE);
    wqeSrcInfoArrays_.emplace_back(srcInfo);
    wqeDstInfoArrays_.emplace_back(dstInfo);

    launchOrder_.emplace_back(TaskArrayType::kTaskArrayTypeWqe);

    HCCL_INFO("[AicpuTaskCacheEntry][AddWqeArray] add %uth wqe array (UdmaSqeWrite)",
        wqeTaskArrays_.size() - 1);

    return HCCL_SUCCESS;
}

HcclResult AicpuTaskCacheEntry::AddWqeArray(UbConnLite* ubConnLitePtr,
    const UdmaSqeWriteWithNotify& wqeWriteWithNotify, const uint32_t dbSqeIdx)
{
    CHK_PTR_NULL(ubConnLitePtr);

    std::vector<WqeTask> wqeTaskArray(1);
    wqeTaskArray[0].wqeWriteWithNotify = wqeWriteWithNotify;
    wqeTaskArrays_.emplace_back(wqeTaskArray);
    ubConnLitePtrs_.emplace_back(ubConnLitePtr);

    DbSqeLocation dbSqeLocation;
    dbSqeLocation.sqeArrayIdx = static_cast<uint32_t>(sqeArrays_.size() - 1);
    dbSqeLocation.dbSqeIdx = dbSqeIdx;
    dbSqeLocations_.emplace_back(dbSqeLocation);

    AddrRefreshInfo srcInfo(0, AddrRefreshInfo::CCL_MEMTYPE);
    AddrRefreshInfo dstInfo(0, AddrRefreshInfo::CCL_MEMTYPE);
    wqeSrcInfoArrays_.emplace_back(srcInfo);
    wqeDstInfoArrays_.emplace_back(dstInfo);

    launchOrder_.emplace_back(TaskArrayType::kTaskArrayTypeWqe);

    HCCL_INFO("[AicpuTaskCacheEntry][AddWqeArray] add %uth wqe array (UdmaSqeWriteWithNotify)",
        wqeTaskArrays_.size() - 1);

    return HCCL_SUCCESS;
}

HcclResult AicpuTaskCacheEntry::RefreshAndLaunch(const AicpuTaskMemRange& localUserInputMemRange,
    const AicpuTaskMemRange& localUserOutputMemRange, const bool profilingEnable)
{
    size_t sqeArrayIdx = 0;
    size_t wqeArrayIdx = 0;

    for (const TaskArrayType& taskType : launchOrder_) {
        if (taskType == TaskArrayType::kTaskArrayTypeSqe) {
            HCCL_INFO("[AicpuTaskCacheEntry][RefreshAndLaunch] launch sqe array[%u]", sqeArrayIdx);
            ++sqeArrayIdx;
        } else if (taskType == TaskArrayType::kTaskArrayTypeWqe) {
            HCCL_INFO("[AicpuTaskCacheEntry][RefreshAndLaunch] launch wqe array[%u]", wqeArrayIdx);
            ++wqeArrayIdx;
        } else {
            HCCL_ERROR("[AicpuTaskCacheEntry][RefreshAndLaunch] invalid task array type");
            return HCCL_E_INTERNAL;
        }
    }

    localUserInputMemRange_ = localUserInputMemRange;
    localUserOutputMemRange_ = localUserOutputMemRange;

    return HCCL_SUCCESS;
}

HcclResult AicpuTaskCacheEntry::RefreshTaskAddr(uint32_t& addrLow, uint32_t& addrHigh,
    AddrRefreshInfo& addrRefreshInfo, const AicpuTaskMemRange& localUserInputMemRange,
    const AicpuTaskMemRange& localUserOutputMemRange) const
{
    return HCCL_SUCCESS;
}

} // namespace Hccl

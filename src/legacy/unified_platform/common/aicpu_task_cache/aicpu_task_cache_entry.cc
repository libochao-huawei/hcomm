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
#include <string.h>

#include "aicpu_task_cache_entry.h"

#include "log.h"
#include "sqe.h"
#include "sqe_v82.h"

namespace Hccl {

// 注意: 与ub_conn_lite.cc保持一致
constexpr uint32_t WRITE_WITH_NOTIFY_OPCODE   = 0x5;

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
        totalSqeCount += sqeSrcAddrRefreshInfoArrays_[arrayIdx].size();

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
        "(%u SQEs in total) and %u WQE arrays (%u WQEs in total) from a cache entry",
        sqeArrayCount, totalSqeCount, wqeArrayCount, totalWqeCount);
}

HcclResult AicpuTaskCacheEntry::AddSqeArray(RtsqA5* rtsqPtr, const size_t sqeCount, const uint8_t* sqeArray)
{
    CHK_PTR_NULL(rtsqPtr);
    CHK_PTR_NULL(sqeArray);
    CHK_PRT_RET(sqeCount == 0,
        HCCL_ERROR("[AicpuTaskCacheEntry][AddSqeArray] sqeCount is 0"),
        HCCL_E_INTERNAL);

    const size_t sqeBytes = sqeCount * AC_SQE_SIZE;
    uint8_t* newSqeArray = reinterpret_cast<uint8_t*>(malloc(sqeBytes));
    CHK_PTR_NULL(newSqeArray);
    CHK_SAFETY_FUNC_RET(memcpy_s(newSqeArray, sqeBytes, sqeArray, sqeBytes));

    // 更新SQE数组
    sqeArrays_.emplace_back(newSqeArray);

    // 更新RtsqA5指针
    rtsqPtrs_.emplace_back(rtsqPtr);

    // 更新AddrRefreshInfo
    sqeSrcAddrRefreshInfoArrays_.emplace_back(sqeCount);
    sqeDstAddrRefreshInfoArrays_.emplace_back(sqeCount);
    const uint8_t* sqePtr = sqeArray;
    for (size_t sqeIdx = 0; sqeIdx < sqeCount; sqeIdx++) {
        CHK_RET(GetSqeAddrRefreshInfo_(sqePtr, sqeSrcAddrRefreshInfoArrays_.back()[sqeIdx],
            sqeDstAddrRefreshInfoArrays_.back()[sqeIdx]));

        sqePtr += AC_SQE_SIZE;
    }

    // 更新下发顺序
    launchOrder_.emplace_back(TaskArrayType::kTaskArrayTypeSqe);

    HCCL_INFO("[AicpuTaskCacheEntry][AddSqeArray] add %uth sqe array with sqeCount of %u",
        sqeArrays_.size() - 1, sqeCount);

    return HCCL_SUCCESS;
}

HcclResult AicpuTaskCacheEntry::AddWqeArray(UbConnLite* ubConnLitePtr, const std::vector<WqeTask>& wqeTasks,
    const uint32_t dbSqeIdx)
{
    const uint32_t wqeCount = wqeTasks.size();
    CHK_PRT_RET(wqeCount == 0,
        HCCL_ERROR("[AicpuTaskCacheEntry][AddWqeArray] wqeCount is 0"),
        HCCL_E_INTERNAL);

    // 更新WQE数组
    wqeTaskArrays_.push_back(wqeTasks);
    
    // 更新UbConnLite指针
    CHK_PTR_NULL(ubConnLitePtr);
    ubConnLitePtrs_.emplace_back(ubConnLitePtr);

    // 更新DbSqeLocation
    // 注意: 假设每段WQE数组对应 **一个** DbSqe
    // 注意: WQE下发时, 对应的DbSqe一定还未下发, 因此一定是在下一个待缓存的SQE数组中
    dbSqeLocations_.emplace_back();
    dbSqeLocations_.back().sqeArrayIdx = static_cast<uint32_t>(sqeArrays_.size());
    dbSqeLocations_.back().dbSqeIdx = dbSqeIdx;

    // 更新AddrRefreshInfo
    wqeSrcAddrRefreshInfoArrays_.emplace_back(wqeCount);
    wqeDstAddrRefreshInfoArrays_.emplace_back(wqeCount);
    for (size_t wqeIdx = 0; wqeIdx < wqeCount; wqeIdx++) {
        CHK_RET(GetWqeAddrRefreshInfo_(wqeTasks[wqeIdx], wqeSrcAddrRefreshInfoArrays_.back()[wqeIdx],
            wqeDstAddrRefreshInfoArrays_.back()[wqeIdx]));
    }

    // 更新下发顺序
    launchOrder_.emplace_back(TaskArrayType::kTaskArrayTypeWqe);

    HCCL_INFO("[AicpuTaskCacheEntry][AddWqeArray] add UdmaSqeRead into %uth wqe array with DbSqeLocation[%u, %u]",
        wqeTaskArrays_.size() - 1, dbSqeLocations_.back()[0].sqeArrayIdx, dbSqeLocations_.back()[0].dbSqeIdx);

    return HCCL_SUCCESS;
}

HcclResult AicpuTaskCacheEntry::RefreshAndLaunch(const AicpuTaskMemRange& localUserInputMemRange,
    const AicpuTaskMemRange& localUserOutputMemRange, const bool profilingEnable)
{
    // TODOSSY: 实现刷新下发逻辑

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

    // 更新缓存的user input/output, 用于下次cache hit刷新缓存
    localUserInputMemRange_ = localUserInputMemRange;
    localUserOutputMemRange_ = localUserOutputMemRange;

    return HCCL_SUCCESS;
}

HcclResult AicpuTaskCacheEntry::GetSqeAddrRefreshInfo_(const uint8_t* sqePtr, AddrRefreshInfo& srcAddrRefreshInfo,
    AddrRefreshInfo& dstAddrRefreshInfo) const
{
    // 参考sqe_build_a5.h, 提取给定SQE的AddrRefreshInfo

    // 提取sqeType
    CHK_PTR_NULL(sqePtr);
    Rt91095StarsSqeHeader* sqeHeaderPtr = (Rt91095StarsSqeHeader*)sqePtr;
    const Rt91095StarsSqeType sqeType = static_cast<Rt91095StarsSqeType>(sqeHeaderPtr->type);
    
    // 根据sqeType提取AddrRefreshInfo
    switch (sqeType) {
        case Rt91095StarsSqeType::RT_91095_SQE_TYPE_NOTIFY_WAIT:
        case Rt91095StarsSqeType::RT_91095_SQE_TYPE_NOTIFY_RECORD:
        case Rt91095StarsSqeType::RT_91095_SQE_TYPE_UBDMA:
            // 无地址字段, 直接跳过
            break;
        case Rt91095StarsSqeType::RT_91095_SQE_TYPE_SDMA:
            Rt91095StarsMemcpySqe *memcpySqePtr = (Rt91095StarsMemcpySqe *)sqePtr;
            CHK_RET(GetAddrRefreshInfo_(memcpySqePtr->u.strideMode0.srcAddrLow, memcpySqePtr->u.strideMode0.srcAddrHigh,
                srcAddrRefreshInfo));
            CHK_RET(GetAddrRefreshInfo_(memcpySqePtr->u.strideMode0.dstAddrLow, memcpySqePtr->u.strideMode0.dstAddrHigh,
                dstAddrRefreshInfo));
            break;
        case Rt91095StarsSqeType::RT_91095_SQE_TYPE_WRITE_VALUE:
            Rt91095StarsWriteValueSqe *writeValueSqePtr = (Rt91095StarsWriteValueSqe *)sqePtr;
            CHK_RET(GetAddrRefreshInfo_(writeValueSqePtr->writeAddrLow, writeValueSqePtr->writeAddrHigh,
                dstAddrRefreshInfo));
            break;
        default:
            // sqe_build_a5.h中未使用的SQE类型, 告警后跳过
            HCCL_WARNING("[AicpuTaskCacheEntry][GetSqeAddrRefreshInfo_] unexpected sqeType[%u]", sqeType);
            break;
    }

    return HCCL_SUCCESS;
}

HcclResult AicpuTaskCacheEntry::GetWqeAddrRefreshInfo_(const WqeTask& wqeTask, AddrRefreshInfo& srcAddrRefreshInfo,
    AddrRefreshInfo& dstAddrRefreshInfo)
{
    // 参考ub_conn_lite.cc, 提取给定WQE的AddrRefreshInfo

    // 提取wqeCode
    // 注意: BatchOneSidedRead/Write只是对multi-slice封装的接口, 最终还是规约到normal read/write
    UdmaSqeCommon* wqeCommonPtr = (UdmaSqeCommon*)(&wqeTask);
    const uint8_t wqeCode = static_cast<uint8_t>(wqeCommonPtr->opcode); // opcode来自于UdmaSqOpcode, 一定在uint8范围内
    switch (wqeCode) {
        case UdmaSqOpcode::UDMA_OPC_READ: // UdmaSqeWrite
            // normal read
            CHK_RET(GetAddrRefreshInfo_(wqeTask.wqeWrite.u.sge.dataAddrLow, wqeTask.wqeWrite.u.sge.dataAddrHigh,
                srcAddrRefreshInfo));
            CHK_RET(GetAddrRefreshInfo_(wqeTask.wqeWrite.comm.rmtAddrLow, wqeTask.wqeWrite.comm.rmtAddrHigh,
                dstAddrRefreshInfo));
            break;
        case UdmaSqOpcode::UDMA_OPC_WRITE: // UdmaSqeWrite
            const uint32_t inlineEn = wqeTask.wqeWrite.comm.inlineEn;
            if (inlineEn) { // inline write
                CHK_RET(GetAddrRefreshInfo_(wqeTask.wqeWrite.comm.rmtAddrLow, wqeTask.wqeWrite.comm.rmtAddrHigh,
                    dstAddrRefreshInfo));
            } else { // normal write or write reduce
                CHK_RET(GetAddrRefreshInfo_(wqeTask.wqeWrite.u.sge.dataAddrLow, wqeTask.wqeWrite.u.sge.dataAddrHigh,
                    srcAddrRefreshInfo));
                CHK_RET(GetAddrRefreshInfo_(wqeTask.wqeWrite.comm.rmtAddrLow, wqeTask.wqeWrite.comm.rmtAddrHigh,
                    dstAddrRefreshInfo));
            }
            break;
        case WRITE_WITH_NOTIFY_OPCODE: // UdmaSqeWriteWithNotify
            // write with notify (对于给定slice的最后一个UB chunk)
            CHK_RET(GetAddrRefreshInfo_(wqeTask.wqeWriteWithNotify.localU.sge.dataAddrLow,
                wqeTask.wqeWriteWithNotify.localU.sge.dataAddrHigh, srcAddrRefreshInfo));
            CHK_RET(GetAddrRefreshInfo_(wqeTask.wqeWriteWithNotify.comm.rmtAddrLow,
                wqeTask.wqeWriteWithNotify.comm.rmtAddrHigh, dstAddrRefreshInfo));
            break;
        default:
            // ub_conn_lite.cc中未使用的WQE类型, 告警后跳过
            HCCL_WARNING("[AicpuTaskCacheEntry][GetWqeAddrRefreshInfo_] unexpected wqeCode[%u]", wqeCode);
            break;
    }
}

HcclResult AicpuTaskCacheEntry::GetAddrRefreshInfo_(const uint32_t addrLow, const uint32_t addrHigh,
    AddrRefreshInfo& addrRefreshInfo)
{
    // 注意: 当前只考虑单算子场景, remote rank一定使用ccl buffer, 无需刷新; 因此rankId字段暂不使用
    addrRefreshInfo.rankId = 0;

    // 拼接地址
    uint64_t addr = 0;
    CombineUint32ToUint64(addr, addrHigh, addrLow);

    // 检查是否为user input
    bool isUserInput = false;
    CHK_RET(localUserInputMemRange_.InRange(addr, isUserInput));
    if (isUserInput) {
        addrRefreshInfo.memType = AddrRefreshInfo::USER_INPUT_MEMTYPE;
        return HCCL_SUCCESS;
    }

    // 检查是否为user output
    bool isUserOutput = false;
    CHK_RET(localUserOutputMemRange_.InRange(addr, isUserOutput));
    if (isUserOutput) {
        addrRefreshInfo.memType = AddrRefreshInfo::USER_OUTPUT_MEMTYPE;
        return HCCL_SUCCESS;
    }

    // 不是user input/ouput, 认为无需刷新
    addrRefreshInfo.memType = AddrRefreshInfo::FIXED_MEMTYPE;
    return HCCL_SUCCESS;
}

HcclResult AicpuTaskCacheEntry::RefreshTaskAddr_(uint32_t& addrLow, uint32_t& addrHigh,
    AddrRefreshInfo& addrRefreshInfo, const AicpuTaskMemRange& localUserInputMemRange,
    const AicpuTaskMemRange& localUserOutputMemRange) const
{
    return HCCL_SUCCESS;
}

} // namespace Hccl

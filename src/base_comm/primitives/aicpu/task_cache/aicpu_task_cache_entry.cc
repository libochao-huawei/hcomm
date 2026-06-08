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
#include "rma_buffer_lite.h"
#include "buffer.h"
#include "rmt_rma_buf_slice_lite.h"
#include "sqe.h"
#include "sqe_v82.h"

namespace hcomm {

// 注意: 与ub_conn_lite.cc保持一致
constexpr uint32_t WRITE_WITH_NOTIFY_OPCODE   = 0x5;

AddrRefreshInfo::AddrRefreshInfo() : needRefresh(false), memIdx(0)
{
}

AddrRefreshInfo::AddrRefreshInfo(const uint32_t curMemIdx)
    : needRefresh(true), memIdx(curMemIdx)
{
}

AddrRefreshInfo::AddrRefreshInfo(const AddrRefreshInfo& other)
    : needRefresh(other.needRefresh), memIdx(other.memIdx)
{
}

AddrRefreshInfo::~AddrRefreshInfo()
{
}

const AddrRefreshInfo& AddrRefreshInfo::operator=(const AddrRefreshInfo& other)
{
    if (this != &other) {
        needRefresh = other.needRefresh;
        memIdx = other.memIdx;
    }
    return *this;
}

AicpuTaskCacheEntry::AicpuTaskCacheEntry() {}

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

HcclResult AicpuTaskCacheEntry::AddSqeArray(RtsqA5* rtsqPtr, AicpuTsThread* aicpuTsThreadPtr, const uint64_t sqeCount,
    const uint8_t* sqeArray)
{
    CHK_PTR_NULL(rtsqPtr);
    CHK_PTR_NULL(aicpuTsThreadPtr);
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
    entryBytes_ += sqeBytes + sizeof(uint8_t*);

    // 更新RtsqA5指针
    rtsqPtrs_.emplace_back(rtsqPtr);
    entryBytes_ += sizeof(RtsqA5*);

    // 更新AicpuTsThread指针
    aicpuTsThreadPtrs_.emplace_back(aicpuTsThreadPtr);
    entryBytes_ += sizeof(AicpuTsThread*);

    // 更新sqeCount
    sqeCounts_.emplace_back(sqeCount);
    entryBytes_ += sizeof(uint64_t);

    // 更新下发顺序
    launchOrder_.emplace_back(TaskArrayType::kTaskArrayTypeSqe);
    entryBytes_ += sizeof(TaskArrayType);

    HCCL_INFO("[AicpuTaskCacheEntry][AddSqeArray] add %uth sqe array with sqeCount[%llu]",
        sqeArrays_.size() - 1, sqeCount);

    return HCCL_SUCCESS;
}

HcclResult AicpuTaskCacheEntry::AddWqeArray(UbConnLite* ubConnLitePtr, UbTransportLiteImpl* ubTransportLiteImplPtr, 
    const std::vector<WqeTask>& wqeTasks, const uint32_t dbSqeIdx)
{
    const uint32_t wqeCount = wqeTasks.size();
    CHK_PRT_RET(wqeCount == 0,
        HCCL_ERROR("[AicpuTaskCacheEntry][AddWqeArray] wqeCount is 0"),
        HCCL_E_INTERNAL);

    // 更新WQE数组
    wqeTaskArrays_.push_back(wqeTasks);
    entryBytes_ += wqeCount * sizeof(WqeTask);
    
    // 更新UbConnLite指针
    CHK_PTR_NULL(ubConnLitePtr);
    ubConnLitePtrs_.emplace_back(ubConnLitePtr);
    entryBytes_ += sizeof(UbConnLite*);

    // 更新UbTransportLiteImpl指针
    CHK_PTR_NULL(ubTransportLiteImplPtr);
    ubTransportLiteImplPtrs_.emplace_back(ubTransportLiteImplPtr);
    entryBytes_ += sizeof(UbTransportLiteImpl*);

    // 更新DbSqeLocation
    // 注意: 假设每段WQE数组对应 **一个** DbSqe
    // 注意: WQE下发时, 对应的DbSqe一定还未下发, 因此一定是在下一个待缓存的SQE数组中
    dbSqeLocations_.emplace_back();
    dbSqeLocations_.back().sqeArrayIdx = static_cast<uint32_t>(sqeArrays_.size());
    dbSqeLocations_.back().dbSqeIdx = dbSqeIdx;
    entryBytes_ += sizeof(DbSqeLocation);

    // 更新下发顺序
    launchOrder_.emplace_back(TaskArrayType::kTaskArrayTypeWqe);
    entryBytes_ += sizeof(TaskArrayType);

    HCCL_INFO("[AicpuTaskCacheEntry][AddWqeArray] add %uth wqe array with wqeCount[%u] DbSqeLocation[%u, %u]",
        wqeTaskArrays_.size() - 1, wqeCount, dbSqeLocations_.back()[0].sqeArrayIdx, dbSqeLocations_.back()[0].dbSqeIdx);

    return HCCL_SUCCESS;
}

HcclResult AicpuTaskCacheEntry::SubmitCacheEntry(const uint64_t* baseAddrs, const uint64_t* memSizes, const uint32_t count)
{
    // 校验count (当前rank的userIn和userOut)
    constexpr uint32_t ADDRS_COUNT = 2;
    CHK_PRT_RET(count != ADDRS_COUNT,
        HCCL_ERROR("[AicpuTaskCacheEntry][SubmitCacheEntry] count[%u] != ADDRS_COUNT[%u]", count, ADDRS_COUNT),
        HCCL_E_INTERNAL);

    // 缓存baseAddrs
    CHK_PRT_RET(cachedBaseAddrs_.size() != 0,
        HCCL_ERROR("[AicpuTaskCacheEntry][SubmitCacheEntry] cachedBaseAddrs_.size[%u] != 0", cachedBaseAddrs_.size()),
        HCCL_E_INTERNAL);
    cachedBaseAddrs_.resize(count);
    entryBytes_ += count * sizeof(uint64_t);
    CHK_SAFETY_FUNC_RET(memcpy_s(cachedBaseAddrs_.data(), count * sizeof(uint64_t), baseAddrs, count * sizeof(uint64_t)));

    // 缓存sizes
    CHK_PRT_RET(cachedMemSizes__.size() != 0,
        HCCL_ERROR("[AicpuTaskCacheEntry][SubmitCacheEntry] cachedMemSizes__.size[%u] != 0", cachedMemSizes__.size()),
        HCCL_E_INTERNAL);
    cachedMemSizes__.resize(count);
    entryBytes_ += count * sizeof(uint64_t);
    CHK_SAFETY_FUNC_RET(memcpy_s(cachedMemSizes__.data(), count * sizeof(uint64_t), memSizes, count * sizeof(uint64_t)));

    // 打印dynamic memory ranges
    if (UNLIKELY(HcclCheckLogLevel(HCCL_LOG_INFO))) {
        for (size_t memIdx = 0; memIdx < count; memIdx++) {
            HCCL_INFO("[AicpuTaskCacheEntry][SubmitCacheEntry] memRanges[%u]: [0x%016llx, 0x%016llx)",
                memIdx, cachedBaseAddrs_[memIdx], cachedBaseAddrs_[memIdx] + cachedMemSizes_[memIdx]);
        }
    }

    // 更新每段SQE数组的AddrRefreshInfo
    CHK_PRT_RET(sqeSrcAddrRefreshInfoArrays_.size() != 0,
        HCCL_ERROR("[AicpuTaskCacheEntry][SubmitCacheEntry] sqeSrcAddrRefreshInfoArrays_.size[%u] != 0",
            sqeSrcAddrRefreshInfoArrays_.size()),
        HCCL_E_INTERNAL);
    CHK_PRT_RET(sqeDstAddrRefreshInfoArrays_.size() != 0,
        HCCL_ERROR("[AicpuTaskCacheEntry][SubmitCacheEntry] sqeDstAddrRefreshInfoArrays_.size[%u] != 0",
            sqeDstAddrRefreshInfoArrays_.size()),
        HCCL_E_INTERNAL);
    for (size_t arrayIdx = 0; arrayIdx < sqeArrays_.size(); arrayIdx++) {
        const uint64_t sqeCount = sqeCounts_[arrayIdx];
        sqeSrcAddrRefreshInfoArrays_.emplace_back(sqeCount);
        entryBytes_ += sqeCount * sizeof(AddrRefreshInfo);
        sqeDstAddrRefreshInfoArrays_.emplace_back(sqeCount);
        entryBytes_ += sqeCount * sizeof(AddrRefreshInfo);
        const uint8_t* sqePtr = sqeArrays_[arrayIdx];
        for (size_t sqeIdx = 0; sqeIdx < sqeCount; sqeIdx++) {
            CHK_RET(UpdateSqeAddrRefreshInfo_(sqePtr, sqeSrcAddrRefreshInfoArrays_.back()[sqeIdx],
                sqeDstAddrRefreshInfoArrays_.back()[sqeIdx]));
            
            // 打印SQE AddrRefreshInfo
            HCCL_INFO("[AicpuTaskCacheEntry][SubmitCacheEntry] sqeArrays_[%u][%u]: "
                "srcAddrRefreshInfo[needRefresh-%d memIdx-%u] "
                "dstAddrRefreshInfo[needRefresh-%d memIdx-%u]",
                arrayIdx, sqeIdx, sqeSrcAddrRefreshInfoArrays_.back()[sqeIdx].needRefresh,
                sqeSrcAddrRefreshInfoArrays_.back()[sqeIdx].memIdx,
                sqeDstAddrRefreshInfoArrays_.back()[sqeIdx].needRefresh,
                sqeDstAddrRefreshInfoArrays_.back()[sqeIdx].memIdx);
            
            sqePtr += AC_SQE_SIZE;
        }
    }

    // 更新每段WQE数组的AddrRefreshInfo
    for (size_t arrayIdx = 0; arrayIdx < wqeTaskArrays_.size(); arrayIdx++) {
        const uint32_t wqeCount = wqeTaskArrays_[arrayIdx].size();
        wqeLocAddrRefreshInfoArrays_.emplace_back(wqeCount);
        entryBytes_ += wqeCount * sizeof(AddrRefreshInfo);
        wqeRmtAddrRefreshInfoArrays_.emplace_back(wqeCount);
        entryBytes_ += wqeCount * sizeof(AddrRefreshInfo);
        std::vector<WqeTask>& wqeTasks = wqeTaskArrays_[arrayIdx];
        for (size_t wqeIdx = 0; wqeIdx < wqeCount; wqeIdx++) {
            CHK_RET(UpdateWqeAddrRefreshInfo_(wqeTasks[wqeIdx], wqeLocAddrRefreshInfoArrays_.back()[wqeIdx],
                wqeRmtAddrRefreshInfoArrays_.back()[wqeIdx]));

            // 打印WQE AddrRefreshInfo
            HCCL_INFO("[AicpuTaskCacheEntry][SubmitCacheEntry] wqeTaskArrays_[%u][%u]: "
                "srcAddrRefreshInfo[needRefresh-%d memIdx-%u] "
                "dstAddrRefreshInfo[needRefresh-%d memIdx-%u]",
                arrayIdx, wqeIdx, wqeLocAddrRefreshInfoArrays_.back()[wqeIdx].needRefresh,
                wqeLocAddrRefreshInfoArrays_.back()[wqeIdx].memIdx,
                wqeRmtAddrRefreshInfoArrays_.back()[wqeIdx].needRefresh,
                wqeRmtAddrRefreshInfoArrays_.back()[wqeIdx].memIdx);
        }
    }

    return HCCL_SUCCESS;
}

HcclResult AicpuTaskCacheEntry::RefreshAndLaunch(const uint64_t* baseAddrs, const uint64_t* memSizes, const uint32_t count)
{
    CHK_PRT_RET(cachedBaseAddrs_.size() != count || cachedMemSizes__.size() != count,
        HCCL_ERROR("[AicpuTaskCacheEntry][RefreshAndLaunch] cachedBaseAddrs_.size[%u] / cachedMemSizes__.size[%u] "
            "!= count[%u]", cachedBaseAddrs_.size(), cachedMemSizes__.size(), count),
        HCCL_E_INTERNAL);

    // 按照缓存时的下发顺序, 依次刷新并下发task
    size_t sqeArrayIdx = 0;
    size_t wqeArrayIdx = 0;
    for (size_t launchIdx = 0; launchIdx < launchOrder_.size(); launchIdx++) {
        const TaskArrayType taskType = launchOrder_[launchIdx];
        if (taskType == TaskArrayType::kTaskArrayTypeSqe) {
            HCCL_INFO("[AicpuTaskCacheEntry][RefreshAndLaunch] refresh sqe array[%u]", sqeArrayIdx);
            // TODO: AR3 调用RefreshSqeTasks_刷新SQE
            // TODO: AR3 RefreshSqeTasks_中, 调用AicpuTsThread中的ReprotXXX, 按需构造TaskParam上报profiling (DB SQE不需要上报?)

            HCCL_INFO("[AicpuTaskCacheEntry][RefreshAndLaunch] launch sqe array[%u]", sqeArrayIdx);
            // TODO: AR3 下发SQE
            
            ++sqeArrayIdx;
        } else if (taskType == TaskArrayType::kTaskArrayTypeWqe) {
            // 刷新WQE数组
            HCCL_INFO("[AicpuTaskCacheEntry][RefreshAndLaunch] refresh wqe array[%u]", wqeArrayIdx);
            CHK_RET(RefreshWqeTasks_(wqeArrayIdx, baseAddrs, memSizes, count));

            // 下发WQE数组
            HCCL_INFO("[AicpuTaskCacheEntry][RefreshAndLaunch] launch wqe array[%u]", wqeArrayIdx);
            CHK_RET(LaunchWqeTasks_(wqeArrayIdx));

            // 刷新对应DbSqe
            HCCL_INFO("[AicpuTaskCacheEntry][RefreshAndLaunch] refresh DbSqe for wqe array[%u]", wqeArrayIdx);
            CHK_RET(RefreshDbSqe_(wqeArrayIdx));

            ++wqeArrayIdx;
        } else {
            HCCL_ERROR("[AicpuTaskCacheEntry][RefreshAndLaunch] invalid task array type");
            return HCCL_E_INTERNAL;
        }
    }

    // 更新缓存的dynamic memory ranges, 用于下次cache hit刷新缓存
    CHK_SAFETY_FUNC_RET(memcpy_s(cachedBaseAddrs_.data(), count * sizeof(uint64_t), baseAddrs, count * sizeof(uint64_t)));
    CHK_SAFETY_FUNC_RET(memcpy_s(cachedMemSizes__.data(), count * sizeof(uint64_t), memSizes, count * sizeof(uint64_t)));

    // 打印更新后的dynamic memory ranges
    if (UNLIKELY(HcclCheckLogLevel(HCCL_LOG_INFO))) {
        for (size_t memIdx = 0; memIdx < count; memIdx++) {
            HCCL_INFO("[AicpuTaskCacheEntry][RefreshAndLaunch] memRanges[%u]: [0x%016llx, 0x%016llx)",
                memIdx, cachedBaseAddrs_[memIdx], cachedBaseAddrs_[memIdx] + cachedMemSizes_[memIdx]);
        }
    }

    return HCCL_SUCCESS;
}

HcclResult AicpuTaskCacheEntry::UpdateSqeAddrRefreshInfo_(const uint8_t* sqePtr, AddrRefreshInfo& srcAddrRefreshInfo,
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
            CHK_RET(UpdateAddrRefreshInfo_(memcpySqePtr->u.strideMode0.srcAddrLow, memcpySqePtr->u.strideMode0.srcAddrHigh,
                srcAddrRefreshInfo));
            CHK_RET(UpdateAddrRefreshInfo_(memcpySqePtr->u.strideMode0.dstAddrLow, memcpySqePtr->u.strideMode0.dstAddrHigh,
                dstAddrRefreshInfo));
            break;
        case Rt91095StarsSqeType::RT_91095_SQE_TYPE_WRITE_VALUE:
            Rt91095StarsWriteValueSqe *writeValueSqePtr = (Rt91095StarsWriteValueSqe *)sqePtr;
            CHK_RET(UpdateAddrRefreshInfo_(writeValueSqePtr->writeAddrLow, writeValueSqePtr->writeAddrHigh,
                dstAddrRefreshInfo));
            break;
        default:
            // sqe_build_a5.h中未使用的SQE类型, 告警后跳过
            HCCL_WARNING("[AicpuTaskCacheEntry][UpdateSqeAddrRefreshInfo_] unexpected sqeType[%u]", sqeType);
            break;
    }

    return HCCL_SUCCESS;
}

HcclResult AicpuTaskCacheEntry::UpdateWqeAddrRefreshInfo_(const WqeTask& wqeTask, AddrRefreshInfo& locAddrRefreshInfo,
    AddrRefreshInfo& rmtAddrRefreshInfo)
{
    // 参考ub_conn_lite.cc, 提取给定WQE的AddrRefreshInfo

    // 提取wqeCode
    // 注意: BatchOneSidedRead/Write只是对multi-slice封装的接口, 最终还是规约到normal read/write
    UdmaSqeCommon* wqeCommonPtr = (UdmaSqeCommon*)(&wqeTask);
    const uint8_t wqeCode = static_cast<uint8_t>(wqeCommonPtr->opcode); // opcode来自于UdmaSqOpcode, 一定在uint8范围内
    switch (wqeCode) {
        case UdmaSqOpcode::UDMA_OPC_READ: // UdmaSqeWrite
            // normal read
            CHK_RET(UpdateAddrRefreshInfo_(wqeTask.wqeWrite.u.sge.dataAddrLow, wqeTask.wqeWrite.u.sge.dataAddrHigh,
                locAddrRefreshInfo));
            CHK_RET(UpdateAddrRefreshInfo_(wqeTask.wqeWrite.comm.rmtAddrLow, wqeTask.wqeWrite.comm.rmtAddrHigh,
                rmtAddrRefreshInfo));
            break;
        case UdmaSqOpcode::UDMA_OPC_WRITE: // UdmaSqeWrite
            const uint32_t inlineEn = wqeTask.wqeWrite.comm.inlineEn;
            if (inlineEn) { // inline write
                CHK_RET(UpdateAddrRefreshInfo_(wqeTask.wqeWrite.comm.rmtAddrLow, wqeTask.wqeWrite.comm.rmtAddrHigh,
                    rmtAddrRefreshInfo));
            } else { // normal write or write reduce
                CHK_RET(UpdateAddrRefreshInfo_(wqeTask.wqeWrite.u.sge.dataAddrLow, wqeTask.wqeWrite.u.sge.dataAddrHigh,
                    locAddrRefreshInfo));
                CHK_RET(UpdateAddrRefreshInfo_(wqeTask.wqeWrite.comm.rmtAddrLow, wqeTask.wqeWrite.comm.rmtAddrHigh,
                    rmtAddrRefreshInfo));
            }
            break;
        case WRITE_WITH_NOTIFY_OPCODE: // UdmaSqeWriteWithNotify
            // write with notify (对于给定slice的最后一个UB chunk)
            CHK_RET(UpdateAddrRefreshInfo_(wqeTask.wqeWriteWithNotify.localU.sge.dataAddrLow,
                wqeTask.wqeWriteWithNotify.localU.sge.dataAddrHigh, locAddrRefreshInfo));
            CHK_RET(UpdateAddrRefreshInfo_(wqeTask.wqeWriteWithNotify.comm.rmtAddrLow,
                wqeTask.wqeWriteWithNotify.comm.rmtAddrHigh, rmtAddrRefreshInfo));
            break;
        default:
            // ub_conn_lite.cc中未使用的WQE类型, 告警后跳过
            HCCL_WARNING("[AicpuTaskCacheEntry][UpdateWqeAddrRefreshInfo_] unexpected wqeCode[%u]", wqeCode);
            break;
    }
}

HcclResult AicpuTaskCacheEntry::UpdateAddrRefreshInfo_(const uint32_t addrLow, const uint32_t addrHigh,
    AddrRefreshInfo& addrRefreshInfo)
{
    // 拼接地址
    uint64_t addr = 0;
    AicpuTaskCacheEntry::CombineUint32ToUint64(addr, addrHigh, addrLow);

    // 检查是否为任意一段dynamic memory
    for (uint32_t memIdx = 0; memIdx < cachedBaseAddrs_.size(); memIdx++) {
        // Memory range: [baseAddr, baseAddr + memSize)
        const uint64_t baseAddr = cachedBaseAddrs_[memIdx];
        const uint64_t memSize = cachedMemSizes__[memIdx];
        CHK_PRT_RET(baseAddr == 0,
            HCCL_ERROR("[AicpuTaskCacheEntry][UpdateAddrRefreshInfo_] cachedBaseAddrs_[%u] is 0, memSize[%llu]",
                memIdx, memSize),
            HCCL_E_INTERNAL);

        // 检查是否在当前dynamic memory range
        bool isInRange = false;
        CHK_RET(AicpuTaskCacheEntry::InRange(baseAddr, memSize, addr, isInRange));
        if (isInRange) {
            addrRefreshInfo.needRefresh = true;
            addrRefreshInfo.memIdx = memIdx;
            return HCCL_SUCCESS;
        }
    }

    // 不是user input/ouput, 认为无需刷新
    addrRefreshInfo.needRefresh = false;
    return HCCL_SUCCESS;
}

HcclResult AicpuTaskCacheEntry::RefreshWqeTasks_(const size_t arrayIdx, const uint64_t* baseAddrs,
    const uint64_t* memSizes, const uint32_t count)
{
    // 校验arrayIdx
    CHK_PRT_RET(arrayIdx >= wqeTaskArrays_.size() || arrayIdx >= wqeLocAddrRefreshInfoArrays_.size() ||
        arrayIdx >= wqeRmtAddrRefreshInfoArrays_.size() || arrayIdx >= ubTransportLiteImplPtrs_.size(),
        HCCL_ERROR("[AicpuTaskCacheEntry][RefreshWqeTasks_] arrayIdx[%u] >= wqeTaskArrays_.size[%u] / "
            "wqeLocAddrRefreshInfoArrays_.size[%u] / wqeRmtAddrRefreshInfoArrays_.size[%u] / "
            "ubTransportLiteImplPtrs_.size[%u]",
            arrayIdx, wqeTaskArrays_.size(), wqeLocAddrRefreshInfoArrays_.size(), wqeRmtAddrRefreshInfoArrays_.size(),
            ubTransportLiteImplPtrs_.size()),
        HCCL_E_INVALID_PARAM);
    
    // 逐个刷新地址
    std::vector<WqeTask>& wqeTasks = wqeTaskArrays_[arrayIdx];
    const std::vector<AddrRefreshInfo>& wqeLocAddrRefreshInfoArray = wqeLocAddrRefreshInfoArrays_[arrayIdx];
    const std::vector<AddrRefreshInfo>& wqeRmtAddrRefreshInfoArray = wqeRmtAddrRefreshInfoArrays_[arrayIdx];
    UbTransportLiteImpl* ubTransportLitePtr = ubTransportLiteImplPtrs_[arrayIdx];
    CHK_PTR_NULL(ubTransportLitePtr);
    for (size_t wqeIdx = 0; wqeIdx < wqeTasks.size(); wqeIdx++) {
        WqeTask& wqeTask = wqeTasks[wqeIdx];
        const AddrRefreshInfo& locAddrRefreshInfo = wqeLocAddrRefreshInfoArray[wqeIdx];
        const AddrRefreshInfo& rmtAddrRefreshInfo = wqeRmtAddrRefreshInfoArray[wqeIdx];

        // 根据WQE类型刷新对应地址字段及token id/value
        UdmaSqeCommon* wqeCommonPtr = (UdmaSqeCommon*)(&wqeTask);
        const uint8_t wqeCode = static_cast<uint8_t>(wqeCommonPtr->opcode); // opcode来自于UdmaSqOpcode, 一定在uint8范围内
        switch (wqeCode) {
            case UdmaSqOpcode::UDMA_OPC_READ: // UdmaSqeWrite
                // normal read

                // 如果需要刷新loc地址信息
                if (locAddrRefreshInfo.needRefresh) {
                    // 刷新loc addr
                    uint64_t newLocAddr = 0;
                    CHK_RET(RefreshTaskAddr_(wqeTask.wqeWrite.u.sge.dataAddrLow, wqeTask.wqeWrite.u.sge.dataAddrHigh,
                        locAddrRefreshInfo, baseAddrs, memSizes, count, &newLocAddr));
                    
                    // 根据刷新后的loc addr, 刷新loc token id (注意: loc不需要刷新token value)
                    CHK_RET(RefreshWqeLocTokenId_(wqeTask.wqeWrite.u.sge.tokenId, locAddrRefreshInfo, newLocAddr,
                        wqeTask.wqeWrite.u.sge.length, ubTransportLitePtr));
                }

                // 如果需要刷新rmt地址信息
                if (rmtAddrRefreshInfo.needRefresh) {
                    // 刷新rmt addr
                    uint64_t newRmtAddr = 0;
                    CHK_RET(RefreshTaskAddr_(wqeTask.wqeWrite.comm.rmtAddrLow, wqeTask.wqeWrite.comm.rmtAddrHigh,
                        rmtAddrRefreshInfo, baseAddrs, memSizes, count, &newRmtAddr));

                    // 根据刷新后的rmt addr, 刷新rmt token id/value
                    CHK_RET(RefreshWqeRmtTokenIdAndValue_(wqeTask.wqeWrite.comm.rmtObjId, wqeTask.wqeWrite.comm.rmtTokenValue,
                        rmtAddrRefreshInfo, newRmtAddr, wqeTask.wqeWrite.u.sge.length, ubTransportLitePtr));
                }

                break;
            case UdmaSqOpcode::UDMA_OPC_WRITE: // UdmaSqeWrite
                const uint32_t inlineEn = wqeTask.wqeWrite.comm.inlineEn;
                if (inlineEn) { // inline write
                    // 注意: inline write使用WriteWqe实现notify功能 (类似A3使用WriteValue实现notify功能)
                    // 因为notify token id/value以及notify addr不会改变, 因此inline write无需刷新WQE
                    if (UNLIKELY(rmtAddrRefreshInfo.needRefresh)) {
                        // 打印地址刷新信息
                        HCCL_ERROR("[AicpuTaskCacheEntry][RefreshWqeTasks_] inline write should not refresh rmt addr: "
                            "rmtAddrRefreshInfo.needRefresh[%d] rmtAddrRefreshInfo.memIdx[%u]",
                            rmtAddrRefreshInfo.needRefresh, rmtAddrRefreshInfo.memIdx);

                        // 校验memIdx
                        CHK_PRT_RET(rmtAddrRefreshInfo.memIdx >= cachedBaseAddrs_.size() ||
                            rmtAddrRefreshInfo.memIdx >= cachedMemSizes_.size(),
                            HCCL_ERROR("[AicpuTaskCacheEntry][RefreshWqeTasks_] invalid memIdx[%u] >= "
                                "cachedBaseAddrs_.size[%u] or cachedMemSizes_.size[%u]",
                                rmtAddrRefreshInfo.memIdx, cachedBaseAddrs_.size(), cachedMemSizes_.size()),
                            HCCL_E_INTERNAL);

                        // 打印地址信息
                        uint64_t rmtAddr = 0;
                        AicpuTaskCacheEntry::CombineUint32ToUint64(rmtAddr, wqeTask.wqeWrite.comm.rmtAddrHigh,
                            wqeTask.wqeWrite.comm.rmtAddrLow);
                        const uint64_t cachedBaseAddr = cachedBaseAddrs_[rmtAddrRefreshInfo.memIdx];
                        const uint64_t cachedMemSize = cachedMemSizes_[rmtAddrRefreshInfo.memIdx];
                        HCCL_ERROR("[AicpuTaskCacheEntry][RefreshWqeTasks_] rmtAddr[0x%016llx] "
                            "cachedBaseAddr[0x%016llx] endAddr[0x%016llx] memSize[%llu]",
                            rmtAddr, cachedBaseAddr, cachedBaseAddr + cachedMemSize, cachedMemSize);
                        
                        return HCCL_E_INTERNAL;
                    }
                } else { // normal write or write reduce
                    // 如果需要刷新loc地址信息
                    if (locAddrRefreshInfo.needRefresh) {
                        // 刷新loc addr
                        uint64_t newLocAddr = 0;
                        CHK_RET(RefreshTaskAddr_(wqeTask.wqeWrite.u.sge.dataAddrLow, wqeTask.wqeWrite.u.sge.dataAddrHigh,
                            locAddrRefreshInfo, baseAddrs, memSizes, count, &newLocAddr));

                        // 根据刷新后的loc addr, 刷新loc token id (注意: loc不需要刷新token value)
                        CHK_RET(RefreshWqeLocTokenId_(wqeTask.wqeWrite.u.sge.tokenId, locAddrRefreshInfo, newLocAddr,
                            wqeTask.wqeWrite.u.sge.length, ubTransportLitePtr));
                    }

                    // 如果需要刷新rmt地址信息
                    if (rmtAddrRefreshInfo.needRefresh) {
                        // 刷新rmt addr
                        uint64_t newRmtAddr = 0;
                        CHK_RET(RefreshTaskAddr_(wqeTask.wqeWrite.comm.rmtAddrLow, wqeTask.wqeWrite.comm.rmtAddrHigh,
                            rmtAddrRefreshInfo, baseAddrs, memSizes, count, &newRmtAddr));

                        // 根据刷新后的rmt addr, 刷新rmt token id/value
                        CHK_RET(RefreshWqeRmtTokenIdAndValue_(wqeTask.wqeWrite.comm.rmtObjId, wqeTask.wqeWrite.comm.rmtTokenValue,
                            rmtAddrRefreshInfo, newRmtAddr, wqeTask.wqeWrite.u.sge.length, ubTransportLitePtr));
                    }
                }

                break;
            case WRITE_WITH_NOTIFY_OPCODE: // UdmaSqeWriteWithNotify
                // write with notify (对于给定slice的最后一个UB chunk)

                // 如果需要刷新loc地址信息
                if (locAddrRefreshInfo.needRefresh) {
                    // 刷新loc addr
                    uint64_t newLocAddr = 0;
                    CHK_RET(RefreshTaskAddr_(wqeTask.wqeWriteWithNotify.localU.sge.dataAddrLow,
                        wqeTask.wqeWriteWithNotify.localU.sge.dataAddrHigh,
                        locAddrRefreshInfo, baseAddrs, memSizes, count, &newLocAddr));
                    
                    // 根据刷新后的loc addr, 刷新loc token id (注意: loc不需要刷新token value)
                    CHK_RET(RefreshWqeLocTokenId_(wqeTask.wqeWriteWithNotify.localU.sge.tokenId, locAddrRefreshInfo,
                        newLocAddr, wqeTask.wqeWriteWithNotify.localU.sge.length, ubTransportLitePtr));
                }

                // 如果需要刷新rmt地址信息
                if (rmtAddrRefreshInfo.needRefresh) {
                    // 刷新rmt addr
                    uint64_t newRmtAddr = 0;
                    CHK_RET(RefreshTaskAddr_(wqeTask.wqeWriteWithNotify.comm.rmtAddrLow,
                        wqeTask.wqeWriteWithNotify.comm.rmtAddrHigh,
                        rmtAddrRefreshInfo, baseAddrs, memSizes, count, &newRmtAddr));

                    // 根据刷新后的rmt addr, 刷新rmt token id/value
                    CHK_RET(RefreshWqeRmtTokenIdAndValue_(wqeTask.wqeWriteWithNotify.comm.rmtObjId,
                        wqeTask.wqeWriteWithNotify.comm.rmtTokenValue,
                        rmtAddrRefreshInfo, newRmtAddr, wqeTask.wqeWriteWithNotify.localU.sge.length, ubTransportLitePtr));
                }

                break;
            default:
                // ub_conn_lite.cc中未使用的WQE类型, 告警后跳过
                HCCL_WARNING("[AicpuTaskCacheEntry][UpdateWqeAddrRefreshInfo_] unexpected wqeCode[%u]", wqeCode);
                break;
        } // switch(wqeCode)

        // TODO: AR6 在UbTransportLiteImpl中封装ReportXXX, 按需构造TaskParam上报profiling
    }

    return HCCL_SUCCESS;
}

HcclResult AicpuTaskCacheEntry::LaunchWqeTasks_(const size_t arrayIdx)
{
    // 校验arrayIdx
    CHK_PRT_RET(arrayIdx >= wqeTaskArrays_.size() || arrayIdx >= ubConnLitePtrs_.size(),
        HCCL_ERROR("[AicpuTaskCacheEntry][RefreshWqeTasks_] arrayIdx[%u] >= wqeTaskArrays_.size[%u] / "
            "ubConnLitePtrs_.size[%u]",
            arrayIdx, wqeTaskArrays_.size(), ubConnLitePtrs_.size()),
        HCCL_E_INVALID_PARAM);

    // 逐个下发WQE
    std::vector<WqeTask>& wqeTasks = wqeTaskArrays_[arrayIdx];
    UbConnLite* ubConnLitePtr = ubConnLitePtrs_[arrayIdx];
    for (size_t wqeIdx = 0; wqeIdx < wqeTasks.size(); wqeIdx++) {
        WqeTask& wqeTask = wqeTasks[wqeIdx];

        // 根据WQE类型下发 (下发过程中会更新ubConnLitePtr中的pi)
        UdmaSqeCommon* wqeCommonPtr = (UdmaSqeCommon*)(&wqeTask);
        const uint8_t wqeCode = static_cast<uint8_t>(wqeCommonPtr->opcode); // opcode来自于UdmaSqOpcode, 一定在uint8范围内
        switch (wqeCode) {
            case UdmaSqOpcode::UDMA_OPC_READ: // UdmaSqeWrite
                // normal read
                // ubConnLitePtr->ProcessOneWqe(&wqeTask.wqeWrite, UdmaSqOpcode::UDMA_OPC_READ); // TODOSSY
                break;
            case UdmaSqOpcode::UDMA_OPC_WRITE: // UdmaSqeWrite
                // inline write, normal write, or write reduce
                // ubConnLitePtr->ProcessOneWqe(&wqeTask.wqeWrite, UdmaSqOpcode::UDMA_OPC_WRITE); // TODOSSY
                break;
            case WRITE_WITH_NOTIFY_OPCODE: // UdmaSqeWriteWithNotify
                // write with notify (对于给定slice的最后一个UB chunk)
                // ubConnLitePtr->ProcessOneWqeWithNotify(&wqeTask.wqeWriteWithNotify, WRITE_WITH_NOTIFY_OPCODE); // TODOSSY
                break;
        }
    }

    return HCCL_SUCCESS;
}

HcclResult AicpuTaskCacheEntry::RefreshDbSqe_(const size_t arrayIdx)
{
    // 校验arrayIdx
    CHK_PRT_RET(arrayIdx >= wqeTaskArrays_.size() || arrayIdx >= dbSqeLocations_.size(),
        HCCL_ERROR("[AicpuTaskCacheEntry][RefreshWqeTasks_] arrayIdx[%u] >= wqeTaskArrays_.size[%u] / "
            "dbSqeLocations_.size[%u]",
            arrayIdx, wqeTaskArrays_.size(), dbSqeLocations_.size()),
        HCCL_E_INVALID_PARAM);

    // 获取pi
    UbConnLite* ubConnLitePtr = ubConnLitePtrs_[arrayIdx];
    uint16_t pi = 0;
    // pi = ubConnLitePtr->GetPi(); // TODOSSY

    // 校验dbSqeLocation
    const DbSqeLocation& dbSqeLocation = dbSqeLocations_[arrayIdx];
    CHK_PRT_RET(dbSqeLocation.sqeArrayIdx >= sqeArrays_.size(),
        HCCL_ERROR("[AicpuTaskCacheEntry][RefreshDbSqe_] dbSqeLocation.sqeArrayIdx[%u] >= sqeArrays_.size[%u]",
            dbSqeLocation.sqeArrayIdx, sqeArrays_.size()),
        HCCL_E_INVALID_PARAM);
    const uint64_t sqeCount = sqeCounts_[dbSqeLocation.sqeArrayIdx];
    CHK_PRT_RET(dbSqeLocation.dbSqeIdx >= sqeCount,
        HCCL_ERROR("[AicpuTaskCacheEntry][RefreshDbSqe_] dbSqeLocation.dbSqeIdx[%u] >= sqeCount[%u]",
            dbSqeLocation.dbSqeIdx, sqeCount),
        HCCL_E_INVALID_PARAM);
    
    // 校验SQE type
    uint8_t* sqePtr = sqeArrays_[dbSqeLocation.sqeArrayIdx] + dbSqeLocation.dbSqeIdx * AC_SQE_SIZE;
    CHK_PTR_NULL(sqePtr);
    Rt91095StarsSqeHeader* sqeHeaderPtr = (Rt91095StarsSqeHeader*)sqePtr;
    const Rt91095StarsSqeType sqeType = static_cast<Rt91095StarsSqeType>(sqeHeaderPtr->type);
    CHK_PRT_RET(sqeType != Rt91095StarsSqeType::RT_91095_SQE_TYPE_UBDMA,
        HCCL_ERROR("[AicpuTaskCacheEntry][RefreshDbSqe_] sqeType[%u] != RT_91095_SQE_TYPE_UBDMA",
            static_cast<uint8_t>(sqeType)),
        HCCL_E_INVALID_PARAM);

    // 更新SQE pi value
    Rt91095StarsUbdmaDBmodeSqe *dbSqePtr = (Rt91095StarsUbdmaDBmodeSqe *)sqePtr;
    dbSqePtr->piValue1 = pi;

    // 注意: UbTransportLiteImpl只针对WQE按需构造TaskParam上报profiling, DB SQE无需上报profiling

    return HCCL_SUCCESS;
}

HcclResult AicpuTaskCacheEntry::RefreshTaskAddr_(uint32_t& addrLow, uint32_t& addrHigh,
    const AddrRefreshInfo& addrRefreshInfo, const uint64_t* baseAddrs, const uint64_t* memSizes, const uint32_t count,
    uint64_t* refreshedAddr) const
{
    CHK_PRT_RET(!addrRefreshInfo.needRefresh,
        HCCL_ERROR("[AicpuTaskCacheEntry][RefreshTaskAddr_] addrRefreshInfo.needRefresh[%u] should be true",
            addrRefreshInfo.needRefresh),
        HCCL_E_PARA);

    // 拼接地址
    uint64_t addr = 0;
    AicpuTaskCacheEntry::CombineUint32ToUint64(addr, addrHigh, addrLow);

    // 校验memIdx
    const uint32_t memIdx = addrRefreshInfo.memIdx;
    CHK_PRT_RET(memIdx >= cachedBaseAddrs_.size() || memIdx >= cachedMemSizes_.size() || memIdx >= count,
        HCCL_ERROR("[AicpuTaskCacheEntry][RefreshTaskAddr_] memIdx[%u] >= cachedBaseAddrs_.size[%u] / "
            "cachedMemSizes_.size[%u] / count[%u]",
            memIdx, cachedBaseAddrs_.size(), cachedMemSizes_.size(), count),
        HCCL_E_INTERNAL);

    // 校验addr
    CHK_PRT_RET(addr < cachedBaseAddrs_[memIdx],
        HCCL_ERROR("[AicpuTaskCacheEntry][RefreshTaskAddr_] addr[0x%016llx] < cachedBaseAddr[0x%016llx]",
            addr, cachedBaseAddrs_[memIdx]),
        HCCL_E_INTERNAL);
    CHK_PRT_RET(addr >= cachedBaseAddrs_[memIdx] + cachedMemSizes_[memIdx],
        HCCL_ERROR("[AicpuTaskCacheEntry][RefreshTaskAddr_] addr[0x%016llx] >= cachedEndAddr[0x%016llx]",
            addr, cachedBaseAddrs_[memIdx] + cachedMemSizes_[memIdx]),
        HCCL_E_INTERNAL);

    // 校验memSize
    CHK_PRT_RET(cachedMemSizes_[memIdx] != memSizes[memIdx],
        HCCL_ERROR("[AicpuTaskCacheEntry][RefreshTaskAddr_] cachedMemSize[%u] != memSize[%u]",
            cachedMemSizes_[memIdx], memSizes[memIdx]),
        HCCL_E_INTERNAL);

    // 计算newAddr
    const uint64_t newAddr = (addr - cachedBaseAddrs_[memIdx]) + baseAddrs[memIdx];
    AicpuTaskCacheEntry::SplitUint64ToUint32(newAddr, addrHigh, addrLow);

    HCCL_INFO("[AicpuTaskCacheEntry][RefreshTaskAddr_] refresh addr[0x%016llx] in memRange[0x%016llx, 0x%016llx] "
        "to newAddr[0x%016llx] in newMemRange[0x%016llx, 0x%016llx]",
        addr, cachedBaseAddrs_[memIdx], cachedBaseAddrs_[memIdx] + cachedMemSizes_[memIdx],
        newAddr, baseAddrs[memIdx], baseAddrs[memIdx] + memSizes[memIdx]);

    // 注意: 如果需要刷新地址, 刷新后的地址一定不是0
    CHK_PRT_RET(newAddr == 0,
        HCCL_ERROR("[AicpuTaskCacheEntry][RefreshTaskAddr_] newAddr[0x%016llx] is 0", newAddr),
        HCCL_E_INTERNAL);

    // 按需返回newAddr
    if (refreshedAddr != nullptr) {
        *refreshedAddr = newAddr;
        HCCL_INFO("[AicpuTaskCacheEntry][RefreshTaskAddr_] set *refreshedAddr as newAddr[0x%016llx]", newAddr);
    }

    return HCCL_SUCCESS;
}

HcclResult AicpuTaskCacheEntry::RefreshWqeLocTokenId_(uint32_t& tokenId, const AddrRefreshInfo& addrRefreshInfo,
    const uint64_t newLocAddr, const uint32_t len, UbTransportLiteImpl* ubTransportLitePtr) const
{
    CHK_PRT_RET(!addrRefreshInfo.needRefresh,
        HCCL_ERROR("[AicpuTaskCacheEntry][RefreshWqeLocTokenId_] addrRefreshInfo.needRefresh[%u] should be true",
            addrRefreshInfo.needRefresh),
        HCCL_E_PARA);

    CHK_PTR_NULL(ubTransportLitePtr);

    // 注意: RefreshTaskAddr_保证, 如果需要刷新, 刷新后的newLocAddr一定不是0
    CHK_PRT_RET(newLocAddr == 0,
        HCCL_ERROR("[AicpuTaskCacheEntry][RefreshWqeLocTokenId_] newLocAddr[0x%016llx] is 0", newLocAddr),
        HCCL_E_INTERNAL);

    // 参考hccl_api_data_aicpu_ts.cc (例如HcommWriteOnThread), 获取新loc token id
    // 注意: 无需通过ubTransportLitePtr->GetRmaBufSlicelite(locRmaBuf)构造Hccl::RmaBufSliceLite locRmaBufSlicelite,
    //     再调用locRmaBufSlicelite.GetTokenId()获取token id (一定与Hccl::RmaBufferLite locRmaBuf的token id相同)
    Hccl::RmaBufferLite locRmaBuf;
    CHK_RET(ubTransportLitePtr->BuildLocRmaBufferLite(reinterpret_cast<uintptr_t>(newLocAddr), len, locRmaBuf));
    const uint32_t newSrcTokenId = locRmaBuf.GetTokenId();

    // 刷新loc token id
    HCCL_INFO("[AicpuTaskCacheEntry][RefreshWqeLocTokenId_] refresh loc tokenId from %u to %u for newLocAddr[0x%016llx]",
        tokenId, newSrcTokenId, newLocAddr);
    tokenId = newSrcTokenId;

    return HCCL_SUCCESS;
}

HcclResult AicpuTaskCacheEntry::RefreshWqeRmtTokenIdAndValue_(uint32_t& tokenId, uint32_t& tokenValue,
    const AddrRefreshInfo& addrRefreshInfo, const uint64_t newRmtAddr, const uint32_t len,
    UbTransportLiteImpl* ubTransportLitePtr) const
{
    CHK_PRT_RET(!addrRefreshInfo.needRefresh,
        HCCL_ERROR("[AicpuTaskCacheEntry][RefreshWqeRmtTokenIdAndValue_] addrRefreshInfo.needRefresh[%u] should be true",
            addrRefreshInfo.needRefresh),
        HCCL_E_PARA);

    // 参考hccl_api_data_aicpu_ts.cc (例如HcommRead/Write/WriteReduce/WriteWithNotifyOnThread), 获取新rmt token id/value
    // 注意: Hccl::Buffer本身不含token id/value, 必须通过调用ubTransportLitePtr->GetRmtRmaBufSliceLite,
    //     构造Hccl::RmtRmaBufSliceLite, 再调用GetTokenId/Value获取token id/value
    const Hccl::Buffer rmtBuf{reinterpret_cast<uintptr_t>(newRmtAddr), len};
    Hccl::RmtRmaBufSliceLite rmtRmaBufSlicelite = ubTransportLitePtr->GetRmtRmaBufSliceLite(rmtBuf);
    const uint32_t newDstTokenId = rmtRmaBufSlicelite.GetTokenId();
    const uint32_t newDstTokenValue = rmtRmaBufSlicelite.GetTokenValue();

    // 刷新remote token id/value
    HCCL_INFO("[AicpuTaskCacheEntry][RefreshWqeRmtTokenIdAndValue_] refresh rmt tokenId from %u to %u, "
        "rmt tokenValue from %u to %u, for newRmtAddr[0x%016llx]",
        tokenId, newDstTokenId, tokenValue, newDstTokenValue, newRmtAddr);
    tokenId = newDstTokenId;
    tokenValue = newDstTokenValue;

    return HCCL_SUCCESS;
}

} // namespace hcomm

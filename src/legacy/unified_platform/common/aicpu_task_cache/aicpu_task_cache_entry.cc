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

HcclResult AicpuTaskCacheEntry::AddSqeArray(RtsqA5* rtsqPtr, const uint64_t sqeCount, const uint8_t* sqeArray)
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

    // 更新sqeCount
    sqeCounts_.emplace_back(sqeCount);

    // 更新下发顺序
    launchOrder_.emplace_back(TaskArrayType::kTaskArrayTypeSqe);

    HCCL_INFO("[AicpuTaskCacheEntry][AddSqeArray] add %uth sqe array with sqeCount[%llu]",
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

    // 更新下发顺序
    launchOrder_.emplace_back(TaskArrayType::kTaskArrayTypeWqe);

    HCCL_INFO("[AicpuTaskCacheEntry][AddWqeArray] add %uth wqe array with wqeCount[%u] DbSqeLocation[%u, %u]",
        wqeTaskArrays_.size() - 1, wqeCount, dbSqeLocations_.back()[0].sqeArrayIdx, dbSqeLocations_.back()[0].dbSqeIdx);

    return HCCL_SUCCESS;
}

HcclResult AicpuTaskCacheEntry::SubmitCacheEntry(const uint64_t* baseAddrs, const uint64_t* memSizes, const uint64_t count)
{
    // 缓存baseAddrs
    CHK_PRT_RET(cachedBaseAddrs_.size() != 0,
        HCCL_ERROR("[AicpuTaskCacheEntry][SubmitCacheEntry] cachedBaseAddrs_.size[%u] != 0", cachedBaseAddrs_.size()),
        HCCL_E_INTERNAL);
    cachedBaseAddrs_.resize(count);
    CHK_SAFETY_FUNC_RET(memcpy_s(cachedBaseAddrs_.data(), count * sizeof(uint64_t), baseAddrs, count * sizeof(uint64_t)));

    // 缓存sizes
    CHK_PRT_RET(cachedMemSizes__.size() != 0,
        HCCL_ERROR("[AicpuTaskCacheEntry][SubmitCacheEntry] cachedMemSizes__.size[%u] != 0", cachedMemSizes__.size()),
        HCCL_E_INTERNAL);
    cachedMemSizes__.resize(count);
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
        sqeDstAddrRefreshInfoArrays_.emplace_back(sqeCount);
        const uint8_t* sqePtr = sqeArrays_[arrayIdx];
        for (size_t sqeIdx = 0; sqeIdx < sqeCount; sqeIdx++) {
            CHK_RET(GetSqeAddrRefreshInfo_(sqePtr, sqeSrcAddrRefreshInfoArrays_.back()[sqeIdx],
                sqeDstAddrRefreshInfoArrays_.back()[sqeIdx]));
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
        wqeSrcAddrRefreshInfoArrays_.emplace_back(wqeCount);
        wqeDstAddrRefreshInfoArrays_.emplace_back(wqeCount);
        std::vector<WqeTask>& wqeTasks = wqeTaskArrays_[arrayIdx];
        for (size_t wqeIdx = 0; wqeIdx < wqeCount; wqeIdx++) {
            CHK_RET(GetWqeAddrRefreshInfo_(wqeTasks[wqeIdx], wqeSrcAddrRefreshInfoArrays_.back()[wqeIdx],
                wqeDstAddrRefreshInfoArrays_.back()[wqeIdx]));
        }
    }

    return HCCL_SUCCESS;
}

HcclResult AicpuTaskCacheEntry::RefreshAndLaunch(const uint64_t* baseAddrs, const uint64_t* memSizes,
    const uint64_t count, const bool profilingEnable)
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
            // TODOSSY: 刷新SQE

            HCCL_INFO("[AicpuTaskCacheEntry][RefreshAndLaunch] launch sqe array[%u]", sqeArrayIdx);
            // TODOSSY: 下发SQE
            
            ++sqeArrayIdx;
        } else if (taskType == TaskArrayType::kTaskArrayTypeWqe) {
            HCCL_INFO("[AicpuTaskCacheEntry][RefreshAndLaunch] refresh wqe array[%u]", wqeArrayIdx);
            CHK_RET(RefreshWqeTasks_(wqeArrayIdx, baseAddrs, memSizes, count));

            HCCL_INFO("[AicpuTaskCacheEntry][RefreshAndLaunch] launch wqe array[%u]", wqeArrayIdx);
            // TODOSSY: 下发WQE

            // TODOSSY: 刷新对应DbSqe

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
    // 拼接地址
    uint64_t addr = 0;
    AicpuTaskCacheEntry::CombineUint32ToUint64(addr, addrHigh, addrLow);

    // 检查是否为任意一段dynamic memory
    for (uint32_t memIdx = 0; memIdx < cachedBaseAddrs_.size(); memIdx++) {
        // Memory range: [baseAddr, baseAddr + memSize)
        const uint64_t baseAddr = cachedBaseAddrs_[memIdx];
        const uint64_t memSize = cachedMemSizes__[memIdx];
        CHK_PRT_RET(baseAddr == 0,
            HCCL_ERROR("[AicpuTaskCacheEntry][GetAddrRefreshInfo_] cachedBaseAddrs_[%u] is 0, memSize[%llu]",
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
    const uint64_t* memSizes, const uint64_t count)
{
    CHK_PRT_RET(arrayIdx >= wqeTaskArrays_.size(),
        HCCL_ERROR("[AicpuTaskCacheEntry][RefreshWqeTasks_] arrayIdx[%u] >= wqeTaskArrays_.size[%u]",
            arrayIdx, wqeTaskArrays_.size()),
        HCCL_E_INVALID_PARAM);
    
    // 逐个刷新地址
    std::vector<WqeTask>& wqeTasks = wqeTaskArrays_[arrayIdx];
    for (size_t wqeIdx = 0; wqeIdx < wqeTasks.size(); wqeIdx++) {
        WqeTask& wqeTask = wqeTasks[wqeIdx];
        AddrRefreshInfo& srcAddrRefreshInfo = sqeSrcAddrRefreshInfoArrays_[arrayIdx][wqeIdx];
        AddrRefreshInfo& dstAddrRefreshInfo = sqeDstAddrRefreshInfoArrays_[arrayIdx][wqeIdx];

        // 根据WQE类型刷新对应地址字段及token id/value
        UdmaSqeCommon* wqeCommonPtr = (UdmaSqeCommon*)(&wqeTask);
        const uint8_t wqeCode = static_cast<uint8_t>(wqeCommonPtr->opcode); // opcode来自于UdmaSqOpcode, 一定在uint8范围内
        switch (wqeCode) {
            case UdmaSqOpcode::UDMA_OPC_READ: // UdmaSqeWrite
                // normal read

                // 刷新src addr
                uint64_t srcAddr = 0;
                AicpuTaskCacheEntry::CombineUint32ToUint64(srcAddr, wqeTask.wqeWrite.u.sge.dataAddrHigh,
                    wqeTask.wqeWrite.u.sge.dataAddrLow);
                CHK_RET(RefreshTaskAddr_(wqeTask.wqeWrite.u.sge.dataAddrLow, wqeTask.wqeWrite.u.sge.dataAddrHigh,
                    srcAddr, srcAddrRefreshInfo, baseAddrs, memSizes, count));

                // 刷新dst addr
                uint64_t dstAddr = 0;
                AicpuTaskCacheEntry::CombineUint32ToUint64(dstAddr, wqeTask.wqeWrite.comm.rmtAddrHigh,
                    wqeTask.wqeWrite.comm.rmtAddrLow);
                CHK_RET(RefreshTaskAddr_(wqeTask.wqeWrite.comm.rmtAddrLow, wqeTask.wqeWrite.comm.rmtAddrHigh,
                    dstAddr, dstAddrRefreshInfo, baseAddrs, memSizes, count));
                
                // 刷新src token id
                const uint32_t len = wqeTask.wqeWrite.u.sge.length;
                Hccl::RmaBufferLite locRmaBuf;
                // CHK_RET(ubTransportLitePtr->BuildLocRmaBufferLite(reinterpret_cast<uintptr_t>(srcAddr), len, locRmaBuf)); // TODOSSY
                RmaBufSliceLite locRmaBufSlicelite;
                // locRmaBufSlicelite = ubTransportLitePtr->GetRmaBufSlicelite(locRmaBuf); // TODOSSY
                wqeTask.wqeWrite.u.sge.tokenId =  locRmaBufSlicelite.GetTokenId();

                // 刷新dst token id/value
                const Hccl::Buffer rmtBuf{reinterpret_cast<uintptr_t>(dstAddr), len};
                RmtRmaBufSliceLite rmtRmaBufSlicelite;
                // rmtRmaBufSlicelite = ubTransportLitePtr->GetRmtRmaBufSliceLite(rmtBuf); // TODOSSY
                wqeTask.wqeWrite.comm.rmtObjId = rmtRmaBufSlicelite.GetTokenId();
                wqeTask.wqeWrite.comm.rmtTokenValue = rmtRmaBufSlicelite.GetTokenValue();

                break;
            case UdmaSqOpcode::UDMA_OPC_WRITE: // UdmaSqeWrite
                const uint32_t inlineEn = wqeTask.wqeWrite.comm.inlineEn;
                if (inlineEn) { // inline write
                    // 刷新dst addr
                    uint64_t dstAddr = 0;
                    AicpuTaskCacheEntry::CombineUint32ToUint64(dstAddr, wqeTask.wqeWrite.comm.rmtAddrHigh,
                        wqeTask.wqeWrite.comm.rmtAddrLow);
                    CHK_RET(RefreshTaskAddr_(wqeTask.wqeWrite.comm.rmtAddrLow, wqeTask.wqeWrite.comm.rmtAddrHigh,
                        dstAddr, dstAddrRefreshInfo, baseAddrs, memSizes, count));
                    
                    // 刷新dst token id/value
                    const Hccl::Buffer rmtBuf{reinterpret_cast<uintptr_t>(dstAddr), len};
                    RmtRmaBufSliceLite rmtRmaBufSlicelite;
                    // rmtRmaBufSlicelite = ubTransportLitePtr->GetRmtRmaBufSliceLite(rmtBuf); // TODOSSY
                    wqeTask.wqeWrite.comm.rmtObjId = rmtRmaBufSlicelite.GetTokenId();
                    wqeTask.wqeWrite.comm.rmtTokenValue = rmtRmaBufSlicelite.GetTokenValue();
                } else { // normal write or write reduce
                    // 刷新src addr
                    uint64_t srcAddr = 0;
                    AicpuTaskCacheEntry::CombineUint32ToUint64(srcAddr, wqeTask.wqeWrite.u.sge.dataAddrHigh,
                        wqeTask.wqeWrite.u.sge.dataAddrLow);
                    CHK_RET(RefreshTaskAddr_(wqeTask.wqeWrite.u.sge.dataAddrLow, wqeTask.wqeWrite.u.sge.dataAddrHigh,
                        srcAddr, srcAddrRefreshInfo, baseAddrs, memSizes, count));

                    // 刷新dst addr
                    uint64_t dstAddr = 0;
                    AicpuTaskCacheEntry::CombineUint32ToUint64(dstAddr, wqeTask.wqeWrite.comm.rmtAddrHigh,
                        wqeTask.wqeWrite.comm.rmtAddrLow);
                    CHK_RET(RefreshTaskAddr_(wqeTask.wqeWrite.comm.rmtAddrLow, wqeTask.wqeWrite.comm.rmtAddrHigh,
                        dstAddr, dstAddrRefreshInfo, baseAddrs, memSizes, count));
                    
                    // 刷新src token id
                    const uint32_t len = wqeTask.wqeWrite.u.sge.length;
                    Hccl::RmaBufferLite locRmaBuf;
                    // CHK_RET(ubTransportLitePtr->BuildLocRmaBufferLite(reinterpret_cast<uintptr_t>(srcAddr), len, locRmaBuf)); // TODOSSY
                    RmaBufSliceLite locRmaBufSlicelite;
                    // locRmaBufSlicelite = ubTransportLitePtr->GetRmaBufSlicelite(locRmaBuf); // TODOSSY
                    wqeTask.wqeWrite.u.sge.tokenId =  locRmaBufSlicelite.GetTokenId();

                    // 刷新dst token id/value
                    const Hccl::Buffer rmtBuf{reinterpret_cast<uintptr_t>(dstAddr), len};
                    RmtRmaBufSliceLite rmtRmaBufSlicelite;
                    // rmtRmaBufSlicelite = ubTransportLitePtr->GetRmtRmaBufSliceLite(rmtBuf); // TODOSSY
                    wqeTask.wqeWrite.comm.rmtObjId = rmtRmaBufSlicelite.GetTokenId();
                    wqeTask.wqeWrite.comm.rmtTokenValue = rmtRmaBufSlicelite.GetTokenValue();
                }
                break;
            case WRITE_WITH_NOTIFY_OPCODE: // UdmaSqeWriteWithNotify
                // write with notify (对于给定slice的最后一个UB chunk)

                // 刷新src addr
                uint64_t srcAddr = 0;
                AicpuTaskCacheEntry::CombineUint32ToUint64(srcAddr, wqeTask.wqeWriteWithNotify.localU.sge.dataAddrHigh,
                    wqeTask.wqeWriteWithNotify.localU.sge.dataAddrLow);
                CHK_RET(RefreshTaskAddr_(wqeTask.wqeWriteWithNotify.localU.sge.dataAddrLow,
                    wqeTask.wqeWriteWithNotify.localU.sge.dataAddrHigh,
                    srcAddr, srcAddrRefreshInfo, baseAddrs, memSizes, count));

                // 刷新dst addr
                uint64_t dstAddr = 0;
                AicpuTaskCacheEntry::CombineUint32ToUint64(dstAddr, wqeTask.wqeWriteWithNotify.comm.rmtAddrHigh,
                    wqeTask.wqeWriteWithNotify.comm.rmtAddrLow);
                CHK_RET(RefreshTaskAddr_(wqeTask.wqeWriteWithNotify.comm.rmtAddrLow,
                    wqeTask.wqeWriteWithNotify.comm.rmtAddrHigh,
                    dstAddr, dstAddrRefreshInfo, baseAddrs, memSizes, count));

                // 刷新src token id
                const uint32_t len = wqeTask.wqeWriteWithNotify.localU.sge.length;
                Hccl::RmaBufferLite locRmaBuf;
                // CHK_RET(ubTransportLitePtr->BuildLocRmaBufferLite(reinterpret_cast<uintptr_t>(srcAddr), len, locRmaBuf)); // TODOSSY
                RmaBufSliceLite locRmaBufSlicelite;
                // locRmaBufSlicelite = ubTransportLitePtr->GetRmaBufSlicelite(locRmaBuf); // TODOSSY
                wqeTask.wqeWriteWithNotify.localU.sge.tokenId =  locRmaBufSlicelite.GetTokenId();

                // 刷新dst token id/value
                const Hccl::Buffer rmtBuf{reinterpret_cast<uintptr_t>(dstAddr), len};
                RmtRmaBufSliceLite rmtRmaBufSlicelite;
                // rmtRmaBufSlicelite = ubTransportLitePtr->GetRmtRmaBufSliceLite(rmtBuf); // TODOSSY
                wqeTask.wqeWriteWithNotify.comm.rmtObjId = rmtRmaBufSlicelite.GetTokenId();
                wqeTask.wqeWriteWithNotify.comm.rmtTokenValue = rmtRmaBufSlicelite.GetTokenValue();

                break;
            default:
                // ub_conn_lite.cc中未使用的WQE类型, 告警后跳过
                HCCL_WARNING("[AicpuTaskCacheEntry][GetWqeAddrRefreshInfo_] unexpected wqeCode[%u]", wqeCode);
                break;
        }
    }

    return HCCL_SUCCESS;
}

HcclResult AicpuTaskCacheEntry::RefreshTaskAddr_(uint32_t& addrLow, uint32_t& addrHigh, const uint64_t addr,
    AddrRefreshInfo& addrRefreshInfo, const uint64_t* baseAddrs, const uint64_t* memSizes, const uint64_t count) const
{
    if (!addrRefreshInfo.needRefresh) {
        return HCCL_SUCCESS;
    }

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

    return HCCL_SUCCESS;
}

} // namespace Hccl

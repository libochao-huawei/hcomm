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
#include <unordered_map>
#include <unordered_set>

#include "op_unfold_cache_entry.h"

#include "aicpu_hccl_sqcq.h"
#include "aicpu_hccl_sqcqv1.h"
#include "aicpu_hccl_sqcqv2.h"
#include "dispatcher_pub.h" // HCCL_SDMA_MAX_COUNT_4GB
#include "log.h"
#include "sal.h"

namespace hccl {

    // struct OpUnfoldMemRange

    OpUnfoldMemRange::OpUnfoldMemRange() : isValid(false), baseAddr(0), memSize(0) {
    }

    OpUnfoldMemRange::OpUnfoldMemRange(const uint64_t curBaseAddr, const uint64_t curMemSize) : isValid(true), baseAddr(curBaseAddr), memSize(curMemSize) {
        // 检查地址有效性
        CHK_PRT_CONT(curBaseAddr == 0, HCCL_ERROR("[OpUnfoldMemRange][OpUnfoldMemRange] curBaseAddr is 0"));
    }

    OpUnfoldMemRange::OpUnfoldMemRange(const OpUnfoldMemRange& other)
        : isValid(other.isValid), baseAddr(other.baseAddr), memSize(other.memSize) {
    }

    OpUnfoldMemRange::~OpUnfoldMemRange() {}

    const OpUnfoldMemRange& OpUnfoldMemRange::operator=(const OpUnfoldMemRange& other) {
        if (this != &other) {
            isValid = other.isValid;
            baseAddr = other.baseAddr;
            memSize = other.memSize;
        }
        return *this;
    }

    HcclResult OpUnfoldMemRange::GetEndAddr(uint64_t& endAddr) const {
        // 检查地址是否溢出
        CHK_PRT_RET(baseAddr + memSize < baseAddr,
            HCCL_ERROR("[OpUnfoldMemRange][InRange] baseAddr[0x%016llx] + memSize[%llu] overflows", baseAddr, memSize),
            HCCL_E_INTERNAL);
        
        endAddr = baseAddr + memSize;
        return HCCL_SUCCESS;
    }

    HcclResult OpUnfoldMemRange::InRange(const uint64_t addr, bool& isInRange) const {
        uint64_t endAddr = 0;
        CHK_RET(GetEndAddr(endAddr));

        if (isValid && addr >= baseAddr && addr < endAddr) {
            isInRange = true;
        } else {
            isInRange = false;
        }

        return HCCL_SUCCESS;
    }

    // struct RefreshAddrInfo

    RefreshAddrInfo::RefreshAddrInfo() : rankId(INVALID_VALUE_RANKID), memType(RefreshAddrInfo::INVALID_MEMTYPE) {
    }

    RefreshAddrInfo::RefreshAddrInfo(const uint32_t curRankId, const uint8_t curMemType) : rankId(curRankId), memType(curMemType) {
        // 注意: rankId可以为INVALID_VALUE_RANKID, 表示访问本地rank
        CHK_PRT_CONT(memType == INVALID_MEMTYPE, HCCL_ERROR("[RefreshAddrInfo][RefreshAddrInfo] invalid memType"));
    }

    RefreshAddrInfo::RefreshAddrInfo(const RefreshAddrInfo& other)
        : rankId(other.rankId), memType(other.memType) {
    }

    RefreshAddrInfo::~RefreshAddrInfo() {}

    const RefreshAddrInfo& RefreshAddrInfo::operator=(const RefreshAddrInfo& other) {
        if (this != &other) {
            rankId = other.rankId;
            memType = other.memType;
        }
        return *this;
    }

    // struct AlltoallvMetadata

    AlltoallvMetadata::AlltoallvMetadata() {
        sdmaDataBlockSize = 0;
        hcclInputMemRanges.clear();
        notifyIdRankRflagMap.clear();
        signalAddrRankRflagMap.clear();

        hcclOffsetDstRanksIdxMap.clear();
    }
    
    void AlltoallvMetadata::Clear() {
        sdmaDataBlockSize = 0;
        hcclInputMemRanges.clear();
        notifyIdRankRflagMap.clear();
        signalAddrRankRflagMap.clear();
        hcclOffsetDstRanksIdxMap.clear();
        return;
    }

    HcclResult AlltoallvMetadata::Check(const bool afterFirstOrch) const {
        CHK_PRT_RET(sdmaDataBlockSize == 0, HCCL_ERROR("[AlltoallvMetadata][Check] sdmaDataBlockSize is zero"), HCCL_E_INTERNAL);

        const uint32_t rankSize = hcclInputMemRanges.size();
        CHK_PRT_RET(rankSize == 0, HCCL_ERROR("[AlltoallvMetadata][Check] empty hcclInputMemRanges"), HCCL_E_INTERNAL);
        // 注意: 每个remote rank各有两个NotifyId/SignalAddr分别用于send/recv count对应的Wait/Record同步
        CHK_PRT_RET(notifyIdRankRflagMap.size() != 2*(rankSize - 1),
            HCCL_ERROR("[AlltoallvMetadata][Check] notifyIdRankRflagMap.size[%u] != rankSize-1[%u]",
                notifyIdRankRflagMap.size(), 2*(rankSize - 1)),
            HCCL_E_INTERNAL);
        CHK_PRT_RET(signalAddrRankRflagMap.size() != 2*(rankSize - 1),
            HCCL_ERROR("[AlltoallvMetadata][Check] signalAddrRankRflagMap.size[%u] != rankSize-1[%u]",
                signalAddrRankRflagMap.size(), 2*(rankSize - 1)),
            HCCL_E_INTERNAL);

        // 注意: 只有在第一次cache miss的executor->Orchestrate之后, 相关mapping才会被初始化
        if (afterFirstOrch) {
            CHK_PRT_RET(((rankSize > 1) && (hcclOffsetDstRanksIdxMap.size() == 0)),
                HCCL_ERROR("[AlltoallvMetadata][Check] empty hcclOffsetDstRanksIdxMap for rankSize[%u]", rankSize),
                HCCL_E_INTERNAL); // 注意: 只有当rankSize为1时, hcclOffsetDstRanksIdxMap的size才可以是0
        }

        return HCCL_SUCCESS;
    }

    // struct AlltoallvSendRecvInfo

    AlltoallvSendRecvInfo::AlltoallvSendRecvInfo() {
        sendType = HcclDataType::HCCL_DATA_TYPE_RESERVED;
        recvType = HcclDataType::HCCL_DATA_TYPE_RESERVED;
        sendCounts.clear();
        recvCounts.clear();
        sendOffsets.clear();
        recvOffsets.clear();

        return;
    }

    HcclResult AlltoallvSendRecvInfo::Check() const {
        CHK_PRT_RET(sendType == HcclDataType::HCCL_DATA_TYPE_RESERVED, HCCL_ERROR("[AlltoallvSendRecvInfo][Check] sendType is reserved"), HCCL_E_INTERNAL);
        CHK_PRT_RET(recvType == HcclDataType::HCCL_DATA_TYPE_RESERVED, HCCL_ERROR("[AlltoallvSendRecvInfo][Check] recvType is reserved"), HCCL_E_INTERNAL);
        
        const uint32_t rankSize = sendCounts.size();
        CHK_PRT_RET(rankSize == 0, HCCL_ERROR("[AlltoallvSendRecvInfo][Check] empty sendCounts"), HCCL_E_INTERNAL);
        CHK_PRT_RET(recvCounts.size() != rankSize, HCCL_ERROR("[AlltoallvSendRecvInfo][Check] recvCounts.size[%u] != rankSize[%u]", recvCounts.size(), rankSize), HCCL_E_INTERNAL);
        CHK_PRT_RET(sendOffsets.size() != rankSize, HCCL_ERROR("[AlltoallvSendRecvInfo][Check] sendOffsets.size[%u] != rankSize[%u]", sendOffsets.size(), rankSize), HCCL_E_INTERNAL);
        CHK_PRT_RET(recvOffsets.size() != rankSize, HCCL_ERROR("[AlltoallvSendRecvInfo][Check] recvOffsets.size[%u] != rankSize[%u]", recvOffsets.size(), rankSize), HCCL_E_INTERNAL);
        
        return HCCL_SUCCESS;
    }

    // class OpUnfoldCacheEntry

    OpUnfoldCacheEntry::OpUnfoldCacheEntry(const std::vector<OpUnfoldMemRange>& userInputMemRanges, const std::vector<OpUnfoldMemRange>& userOutputMemRanges)
        : userInputMemRanges_(userInputMemRanges), userOutputMemRanges_(userOutputMemRanges) {
        HCCL_INFO("[OpUnfoldCacheEntry][OpUnfoldCacheEntry] create a cache entry with %llu userInputMemRanges and %llu userOutputMemRanges",
            userInputMemRanges_.size(), userOutputMemRanges_.size());
    }

    OpUnfoldCacheEntry::~OpUnfoldCacheEntry() {
        // TODOSSY: 性能打点
        std::unordered_map<uint8_t, uint32_t> sqeTypeSqeCountMap;
        std::unordered_set<uint32_t> streamSqeIdxSet;

        size_t sqeArrayCount = sqeArrays_.size();
        size_t totalSqeCount = 0;
        for (size_t arrayIdx = 0; arrayIdx < sqeArrayCount; ++arrayIdx) {
            const uint32_t curSqeCount = srcRefreshAddrInfoArrays_[arrayIdx].size();
            totalSqeCount += curSqeCount;

            // 如果存在当前这段连续的SQE数组，则指向内容必不为空
            // 因为SQE数量为0时, DispatcherAicpu::LaunchTask()会直接返回, 不会添加SQE到OpUnfoldCache中
            uint8_t *curSqeArray = sqeArrays_[arrayIdx];

            // 释放当前SQE数组
            if (UNLIKELY(curSqeArray == nullptr)) { // 不能使用CHK_PTR_NULL，因为会return HcclResult
                HCCL_ERROR("[OpUnfoldCacheEntry][~OpUnfoldCacheEntry] curSqeArray is nullptr");
            } else {
                free(curSqeArray);
                curSqeArray = nullptr;
            }

            // 同理释放其他空间

            // TODOSSY: 性能打点
            for (size_t i = 0; i < curSqeCount; ++i) {
                const uint8_t sqeType = sqeTypeArrays_[arrayIdx][i];
                if (sqeTypeSqeCountMap.find(sqeType) == sqeTypeSqeCountMap.end()) {
                    sqeTypeSqeCountMap[sqeType] = 0;
                }
                sqeTypeSqeCountMap[sqeType]++;
            }

            // 释放当前SQE type数组
            uint8_t *curSqeTypeArray = sqeTypeArrays_[arrayIdx];
            if (UNLIKELY(curSqeTypeArray == nullptr)) {
                HCCL_ERROR("[OpUnfoldCacheEntry][~OpUnfoldCacheEntry] curSqeTypeArray is nullptr");
            } else {
                free(curSqeTypeArray);
                curSqeTypeArray = nullptr;
            }

            // 释放当前SQE DfxInfo数组
            AicpuDfxInfo *curSqeDfxInfoArray = sqeDfxInfoArrays_[arrayIdx];
            if (UNLIKELY(curSqeDfxInfoArray == nullptr)) {
                HCCL_ERROR("[OpUnfoldCacheEntry][~OpUnfoldCacheEntry] curSqeDfxInfoArray is nullptr");
            } else {
                free(curSqeDfxInfoArray);
                curSqeDfxInfoArray = nullptr;
            }

            // TODOSSY: 性能打点
            const uint32_t streamSqeIdx = streamSeqIdxes_[arrayIdx];
            streamSqeIdxSet.insert(streamSqeIdx);
        }

        HCCL_ERROR("[OpUnfoldCacheEntry][~OpUnfoldCacheEntry] release %u SQE arrays (%u SQEs and %u streams in total) from the cache entry", sqeArrayCount, totalSqeCount, streamSqeIdxSet.size()); // TODOSSY: 性能打点

        // TODOSSY: 性能打点
        for (std::unordered_map<uint8_t, uint32_t>::const_iterator it = sqeTypeSqeCountMap.begin(); it != sqeTypeSqeCountMap.end(); ++it) {
            HCCL_ERROR("[OpUnfoldCacheEntry][~OpUnfoldCacheEntry] sqeType %u has %u SQEs", it->first, it->second); // TODOSSY: 性能打点
        }
    }

    HcclResult OpUnfoldCacheEntry::GetSqeArrayCount(size_t& sqeArrayCount) const {
        sqeArrayCount = sqeArrays_.size();
        CHK_PRT_RET(sqeArrayCount == 0, HCCL_ERROR("[OpUnfoldCacheEntry][OpUnfoldCacheEntry] sqeArrayCount is 0"), HCCL_E_INTERNAL);
        return HCCL_SUCCESS;
    }

    HcclResult OpUnfoldCacheEntry::AllocSqeArray(const size_t sqeCount, const int32_t streamId, size_t& arrayIdx) {
        // Allocate a new SQE array
        const size_t sqeBytes = sqeCount * HCCL_SQE_SIZE;
        uint8_t *newSqeArray = reinterpret_cast<uint8_t *>(malloc(sqeBytes));
        CHK_PTR_NULL(newSqeArray);
        sqeArrays_.emplace_back(newSqeArray);

        // Allocate a new SQE type array
        const size_t sqeTypeBytes = sqeCount * sizeof(uint8_t);
        uint8_t *newSqeTypeArray = reinterpret_cast<uint8_t *>(malloc(sqeTypeBytes));
        CHK_PTR_NULL(newSqeTypeArray);
        sqeTypeArrays_.emplace_back(newSqeTypeArray);

        // Allocate a new SQE DFX info array
        const size_t sqeDfxInfoBytes = sqeCount * sizeof(AicpuDfxInfo);
        AicpuDfxInfo *newSqeDfxInfoArray = reinterpret_cast<AicpuDfxInfo *>(malloc(sqeDfxInfoBytes));
        CHK_PTR_NULL(newSqeDfxInfoArray);
        sqeDfxInfoArrays_.emplace_back(newSqeDfxInfoArray);

        // Copy stream pointer
        CHK_PRT_RET(streamId < 0, HCCL_ERROR("[OpUnfoldCacheEntry][AllocSqeArray] streamId %d < 0", streamId), HCCL_E_INTERNAL);
        streamIds_.emplace_back(streamId);

        // 注意: streamSeqIdxes_在cache miss LaunchTask()结束后, HcclCommAicpu通过CalcStreamSeqIdxes更新

        // 初始化src/dst RefreshAddrInfo
        srcRefreshAddrInfoArrays_.emplace_back(sqeCount);
        dstRefreshAddrInfoArrays_.emplace_back(sqeCount);

        // Set index of allocated array
        arrayIdx = sqeArrays_.size() - 1;

        HCCL_INFO("[OpUnfoldCacheEntry][AllocSqeArray] allocate %uth sqe array with sqeCount of %u and streamId of %d", arrayIdx, sqeCount, streamId);

        return HCCL_SUCCESS;
    }

    HcclResult OpUnfoldCacheEntry::MemcpySqeArray(const size_t arrayIdx, const size_t sqeStartIdx, const size_t sqeCount, const uint8_t *sqeArray, const uint8_t *sqeTypeArray, const AicpuDfxInfo *sqeDfxInfoArray, const bool isAlltoallv, const AlltoallvMetadata* alltoallvMetadataPtr) {
        // Copy sqeArray[0:sqeCount) -> sqeArrays_[arrayIdx][sqeStartIdx:sqeStartIdx+sqeCount)

        // 检验入参
        CHK_PRT_RET(arrayIdx >= sqeArrays_.size(), HCCL_ERROR("[OpUnfoldCacheEntry][MemcpySqeArray] arrayIdx %u is out of range [0, %u)", arrayIdx, sqeArrays_.size()), HCCL_E_INTERNAL);
        const size_t totalSqeCount = srcRefreshAddrInfoArrays_[arrayIdx].size();
        CHK_PRT_RET(sqeStartIdx + sqeCount - 1 >= totalSqeCount, HCCL_ERROR("[OpUnfoldCacheEntry][MemcpySqeArray] sqeStartIdx %u + sqeCount %u - 1 is out of range [0, %u)", sqeStartIdx, sqeCount, totalSqeCount), HCCL_E_INTERNAL);
        CHK_PTR_NULL(sqeArray);
        CHK_PTR_NULL(sqeTypeArray);
        CHK_PTR_NULL(sqeDfxInfoArray);
        if (isAlltoallv) {
            CHK_PTR_NULL(alltoallvMetadataPtr);
            CHK_RET(alltoallvMetadataPtr->Check(false));
        }

        HCCL_INFO("[OpUnfoldCacheEntry][MemcpySqeArray] memcpy %uth sqe array[%u:%u]; isAlltoallv[%u]", arrayIdx, sqeStartIdx, sqeStartIdx + sqeCount - 1, isAlltoallv);

        // Copy SQE content
        const size_t sqeBytes = sqeCount * HCCL_SQE_SIZE;
        uint8_t *dstSqeArray = sqeArrays_[arrayIdx];
        CHK_PTR_NULL(dstSqeArray);
        CHK_SAFETY_FUNC_RET(memcpy_s(dstSqeArray + sqeStartIdx * HCCL_SQE_SIZE, (totalSqeCount - sqeStartIdx) * HCCL_SQE_SIZE, sqeArray, sqeBytes));

        // Copy SQE type
        const size_t sqeTypeBytes = sqeCount * sizeof(uint8_t);
        uint8_t *dstSqeTypeArray = sqeTypeArrays_[arrayIdx];
        CHK_PTR_NULL(dstSqeTypeArray);
        CHK_SAFETY_FUNC_RET(memcpy_s(dstSqeTypeArray + sqeStartIdx, (totalSqeCount - sqeStartIdx) * sizeof(uint8_t), sqeTypeArray, sqeTypeBytes));

        // Copy SQE DFX info
        const size_t sqeDfxInfoBytes = sqeCount * sizeof(AicpuDfxInfo);
        AicpuDfxInfo *dstSqeDfxInfoArray = sqeDfxInfoArrays_[arrayIdx];
        CHK_PTR_NULL(dstSqeDfxInfoArray);
        CHK_SAFETY_FUNC_RET(memcpy_s(dstSqeDfxInfoArray + sqeStartIdx, (totalSqeCount - sqeStartIdx) * sizeof(AicpuDfxInfo), sqeDfxInfoArray, sqeDfxInfoBytes));

        // 遍历SQE, 根据type更新src/dst RefreshAddrInfo
        std::vector<RefreshAddrInfo>& srcRefreshAddrInfoArray = srcRefreshAddrInfoArrays_[arrayIdx];
        std::vector<RefreshAddrInfo>& dstRefreshAddrInfoArray = dstRefreshAddrInfoArrays_[arrayIdx];
        uint64_t sqeSrcAddr = 0;
        uint64_t sqeDstAddr = 0;
        const uint8_t *sqePtr = sqeArray;
        for (size_t tmpSqeIdx = 0; tmpSqeIdx < sqeCount; tmpSqeIdx++) {
            const size_t cacheSqeIdx = sqeStartIdx + tmpSqeIdx;

            // 获得当前SQE的信息
            // 注意: 不使用sqeDfxInfoArray[tmpSqeIdx].remoteRank来准备RefreshAddrInfo, 因为DfxInfo.remoteRank某些整网用例下存在维护异常
            const uint8_t sqeType = sqeTypeArray[tmpSqeIdx];

            // 根据SQE type更新RefreshAddrInfo
            switch(sqeType) {
                case SqeType::NOTIFY_SQE:
                case SqeType::EVENT_SQE: {
                    // No need to update src/dst RefreshAddrInfo due to no addr fields
                    break;
                }
                case SqeType::WRITE_VALUE_SQE:
                case SqeType::RDMA_DB_SEND_SQE: {
                    const rtStarsWriteValueSqe_t *writeValueSqePtr = reinterpret_cast<const rtStarsWriteValueSqe_t *>(sqePtr);
                    
                    CombineUint32ToUint64(sqeDstAddr, writeValueSqePtr->write_addr_high, writeValueSqePtr->write_addr_low);
                    CHK_RET(CheckAndPrepareRefreshAddrInfo(sqeDstAddr, dstRefreshAddrInfoArray[cacheSqeIdx], false, nullptr)); // 注意: alltoallv算子有WRITE_VALUE_SQE, 但不会存在对于HCCL input buffer的访问, 无需确认HCCL input buffer对应的rank id进行地址刷新

                    break;
                }
                case SqeType::MEMCPY_ASYNC_SQE: {
                    const rtStarsMemcpyAsyncSqe_t *memcpyAsyncSqePtr = reinterpret_cast<const rtStarsMemcpyAsyncSqe_t *>(sqePtr);

                    CombineUint32ToUint64(sqeSrcAddr, memcpyAsyncSqePtr->src_addr_high, memcpyAsyncSqePtr->src_addr_low);
                    CHK_RET(CheckAndPrepareRefreshAddrInfo(sqeSrcAddr, srcRefreshAddrInfoArray[cacheSqeIdx], isAlltoallv, alltoallvMetadataPtr));

                    CombineUint32ToUint64(sqeDstAddr, memcpyAsyncSqePtr->dst_addr_high, memcpyAsyncSqePtr->dst_addr_low);
                    CHK_RET(CheckAndPrepareRefreshAddrInfo(sqeDstAddr, dstRefreshAddrInfoArray[cacheSqeIdx], isAlltoallv, alltoallvMetadataPtr));

                    break;
                }
                case SqeType::CCORE_WAIT_START_SQE: {
                    HCCL_ERROR("[OpUnfoldCacheEntry][MemcpySqeArray] SqeType::CCORE_WAIT_START_SQE is not supported in A3");
                    return HCCL_E_NOT_SUPPORT;
                }
                case SqeType::CCORE_WRITE_VALUE_SQE: {
                    HCCL_ERROR("[OpUnfoldCacheEntry][MemcpySqeArray] SqeType::CCORE_WRITE_VALUE_SQE is not supported in A3");
                    return HCCL_E_NOT_SUPPORT;
                }
                case SqeType::NOTIFY_SQE_V2: {
                    HCCL_ERROR("[OpUnfoldCacheEntry][MemcpySqeArray] SqeType::NOTIFY_SQE_V2 is not supported in A3");
                    return HCCL_E_NOT_SUPPORT;
                }
                case SqeType::WRITE_VALUE_SQE_V2: {
                    HCCL_ERROR("[OpUnfoldCacheEntry][MemcpySqeArray] SqeType::WRITE_VALUE_SQE_V2 is not supported in A3");
                    return HCCL_E_NOT_SUPPORT;
                }
                case SqeType::EVENT_SQE_V2: {
                    HCCL_ERROR("[OpUnfoldCacheEntry][MemcpySqeArray] SqeType::EVENT_SQE_V2 is not supported in A3");
                    return HCCL_E_NOT_SUPPORT;
                }
                case SqeType::MEMCPY_ASYNC_SQE_V2: {
                    HCCL_ERROR("[OpUnfoldCacheEntry][MemcpySqeArray] SqeType::MEMCPY_ASYNC_SQE_V2 is not supported in A3");
                    return HCCL_E_NOT_SUPPORT;
                }
                case SqeType::FLIP_PLACEHOLDER_SQE: {
                    HCCL_ERROR("[OpUnfoldCacheEntry][MemcpySqeArray] placeholder should not be cached, sqeType[%u] tmpSqeIdx[%u] cacheSqeIdx[%u]", sqeType, tmpSqeIdx, cacheSqeIdx);
                    return HCCL_E_INTERNAL;
                }
                case SqeType::CACHE_MEMCPY_PLACEHOLDER_SQE: {
                    // 注意: 只有alltoallv算子在cache时才会有此类SQE
                    CHK_PRT_RET(!isAlltoallv, HCCL_ERROR("[OpUnfoldCacheEntry][MemcpySqeArray] non-alltoallv op should not dispatch CACHE_MEMCPY_PLACEHOLDER_SQE"), HCCL_E_INTERNAL);

                    const rtStarsPlaceHolderSqe_t *placeholderSqePtr = reinterpret_cast<const rtStarsPlaceHolderSqe_t *>(sqePtr);
                    const struct rtCacheMemcpyTaskTag_t& unfoldCacheTaskTag = placeholderSqePtr->u.cache_memcpy_task_info;

                    CombineUint32ToUint64(sqeSrcAddr, unfoldCacheTaskTag.src_addr_high, unfoldCacheTaskTag.src_addr_low);
                    CHK_RET(CheckAndPrepareRefreshAddrInfo(sqeSrcAddr, srcRefreshAddrInfoArray[cacheSqeIdx], isAlltoallv, alltoallvMetadataPtr));

                    CombineUint32ToUint64(sqeDstAddr, unfoldCacheTaskTag.dst_addr_high, unfoldCacheTaskTag.dst_addr_low);
                    CHK_RET(CheckAndPrepareRefreshAddrInfo(sqeDstAddr, dstRefreshAddrInfoArray[cacheSqeIdx], isAlltoallv, alltoallvMetadataPtr));

                    break;
                }
                case SqeType::CACHE_NOTIFY_PLACEHOLDER_SQE:
                case SqeType::CACHE_WRITE_VALUE_PLACEHOLDER_SQE:
                case SqeType::CACHE_MEMCPY_RECORD_PLACEHOLDER_SQE: {
                    // Cache-notify placeholder: 无地址字段, 不需要更新src/dst RefreshAddrInfo
                    // Cache-write placeholder: 对应WriteValueRecord SQE, 地址为硬件映射的固定内存, 无需更新RefreshAddrInfo
                    // Cache-memcpy-record placeholder: 对应MemcpyRecord SQE, 地址为硬件映射的固定内存, 无需更新RefreshAddrInfo
                    break;
                }
                default: {
                    HCCL_WARNING("[OpUnfoldCacheEntry][MemcpySqeArray] sqeType %u is unsupported", sqeType);
                    return HCCL_E_NOT_SUPPORT;
                }
            }

            sqePtr += HCCL_SQE_SIZE;
        }

        return HCCL_SUCCESS;
    }

    HcclResult OpUnfoldCacheEntry::CalcStreamSeqIdxes(Stream& mainStream, std::vector<Stream>& slaveStreams) {
        const size_t streamIdCount = streamIds_.size();
        HCCL_INFO("[OpUnfoldCacheEntry][CalcStreamSeqIdxes] calculate stream sequential indexes for %u stream ids", streamIdCount);

        // 对每个stream id找到对应的sequential stream index
        streamSeqIdxes_.resize(streamIdCount);
        for (size_t i = 0; i < streamIdCount; ++i) {
            const int32_t curStreamId = streamIds_[i];

            if (curStreamId == mainStream.GetHcclStreamInfo().actualStreamId) { // 主流
                streamSeqIdxes_[i] = 0;
            } else { // 遍历从流
                bool isFound = false;
                for (size_t j = 0; j < slaveStreams.size(); ++j) {
                    if (curStreamId == slaveStreams[j].GetHcclStreamInfo().actualStreamId) { // 匹配某个从流
                        streamSeqIdxes_[i] = j + 1;
                        isFound = true;
                        break;
                    }
                }

                // No stream can match the stream id
                if (!isFound) {
                    HCCL_ERROR("[OpUnfoldCacheEntry][CalcStreamSeqIdxes] cannot find any stream to match streamId %u", curStreamId);
                    return HCCL_E_INTERNAL;
                }
            }
        }

        return HCCL_SUCCESS;
    }

    HcclResult OpUnfoldCacheEntry::UpdateRefreshAddrInfoForAlltoallv(const uint32_t curRank, AlltoallvMetadata& alltoallvMetadata) {
        // 校验入参
        CHK_RET(alltoallvMetadata.Check(true));

        // 获得rankSize
        const uint32_t rankSize = alltoallvMetadata.hcclInputMemRanges.size();
        CHK_PRT_RET(curRank >= rankSize,
            HCCL_ERROR("[OpUnfoldCacheEntry][UpdateRefreshAddrInfoForAlltoallv] curRank[%u] >= rankSize[%u]", curRank, rankSize),
            HCCL_E_INTERNAL);
        
        HCCL_RUN_INFO("[OpUnfoldCacheEntry][UpdateRefreshAddrInfoForAlltoallv] curRank[%u] rankSize[%u]", curRank, rankSize);

        // 遍历每一段SQE数组
        const size_t sqeArrayCnt = dstRefreshAddrInfoArrays_.size();
        for (size_t arrayIdx = 0; arrayIdx < sqeArrayCnt; ++arrayIdx) {
            // 准备当前SQE数组的相关metadata
            const uint8_t *sqeArray = sqeArrays_[arrayIdx];
            CHK_PTR_NULL(sqeArray);
            const uint8_t *sqeTypeArray = sqeTypeArrays_[arrayIdx];
            CHK_PTR_NULL(sqeTypeArray);
            std::vector<RefreshAddrInfo>& srcRefreshAddrInfoArray = srcRefreshAddrInfoArrays_[arrayIdx];
            std::vector<RefreshAddrInfo>& dstRefreshAddrInfoArray = dstRefreshAddrInfoArrays_[arrayIdx];

            // 遍历数组中的每个SQE
            const size_t sqeCount = dstRefreshAddrInfoArray.size();
            HCCL_INFO("[OpUnfoldCacheEntry][UpdateRefreshAddrInfoForAlltoallv] sqeArrayIdx[%u] sqeCount[%u]", arrayIdx, sqeCount);
            for (size_t sqeIdx = 0; sqeIdx < sqeCount; ++sqeIdx) {
                // 准备当前SQE的相关metadata
                const uint8_t *sqePtr = sqeArray + sqeIdx * HCCL_SQE_SIZE;
                const uint8_t sqeType = sqeTypeArray[sqeIdx];
                RefreshAddrInfo& srcRefreshAddrInfo = srcRefreshAddrInfoArray[sqeIdx];
                RefreshAddrInfo& dstRefreshAddrInfo = dstRefreshAddrInfoArray[sqeIdx];

                // 只有memcpy / cache-memcpy placeholder SQE可能需要更新RefreshAddrInfo
                if (sqeType != SqeType::MEMCPY_ASYNC_SQE && sqeType != SqeType::CACHE_MEMCPY_PLACEHOLDER_SQE) {
                    continue;
                }

                // 跳过memcpy record SQE (src/dst memType为invalid)
                const uint8_t srcMemType = srcRefreshAddrInfo.memType;
                const uint8_t dstMemType = dstRefreshAddrInfo.memType;
                if (sqeType == SqeType::MEMCPY_ASYNC_SQE && srcMemType == RefreshAddrInfo::INVALID_MEMTYPE &&
                    dstMemType == RefreshAddrInfo::INVALID_MEMTYPE) {
                    const rtStarsMemcpyAsyncSqe_t *memcpySqePtr = reinterpret_cast<const rtStarsMemcpyAsyncSqe_t *>(sqePtr);
                    HCCL_DEBUG("[OpUnfoldCacheEntry][UpdateRefreshAddrInfoForAlltoallv] skip memcpy-record SQE: "\
                        "streamId[%u] taskId[%u] srcMemType[%u] dstMemType[%u]",
                            memcpySqePtr->header.rtStreamId, memcpySqePtr->header.taskId, srcMemType, dstMemType);
                    continue;
                }

                // (i) 更新invalid memType

                // 只有cache-memcpy placeholder才有可能存在user input/output endAddr导致的src/dst invalid memType
                if (sqeType == SqeType::CACHE_MEMCPY_PLACEHOLDER_SQE) {
                    // 校验src/dst memType
                    const rtStarsPlaceHolderSqe_t *placeholderSqePtr = reinterpret_cast<const rtStarsPlaceHolderSqe_t *>(sqePtr);
                    const uint16_t placeholderStreamId = placeholderSqePtr->header.rtStreamId;
                    const uint16_t placeholderTaskId = placeholderSqePtr->header.taskId;
                    if (srcMemType == RefreshAddrInfo::INVALID_MEMTYPE && dstMemType != RefreshAddrInfo::INVALID_MEMTYPE) {
                        // LocalCopy/PrepareIntraData: dst一定是local user output
                        // PrepareIntraData: 虽然dst RefreshAddrInfo的rank尚未更新 (see当前函数的第(ii)部分), memType一定是(local) hccl input
                        CHK_PRT_RET(dstMemType != RefreshAddrInfo::USER_OUTPUT_MEMTYPE && dstMemType != RefreshAddrInfo::HCCL_INPUT_MEMTYPE,
                            HCCL_ERROR("[OpUnfoldCacheEntry][UpdateRefreshAddrInfoForAlltoallv] cache-memcpy placeholder: "\
                                "streamId[%u] taskId[%u] dstMemType[%u] != RefreshAddrInfo::HCCL_INPUT_MEMTYPE[%u]",
                                placeholderStreamId, placeholderTaskId, dstMemType, RefreshAddrInfo::HCCL_INPUT_MEMTYPE),
                            HCCL_E_INTERNAL);
                    } else if (srcMemType != RefreshAddrInfo::INVALID_MEMTYPE && dstMemType == RefreshAddrInfo::INVALID_MEMTYPE) {
                        // LocalCopy/RemoteCopy: src一定是local user input
                        // RemoteCopy: src memType一定是(remote) hccl input
                        CHK_PRT_RET(srcMemType != RefreshAddrInfo::USER_INPUT_MEMTYPE && srcMemType != RefreshAddrInfo::HCCL_INPUT_MEMTYPE,
                            HCCL_ERROR("[OpUnfoldCacheEntry][UpdateRefreshAddrInfoForAlltoallv] cache-memcpy placeholder: "\
                                "streamId[%u] taskId[%u] srcMemType[%u] != RefreshAddrInfo::HCCL_INPUT_MEMTYPE[%u]",
                                placeholderStreamId, placeholderTaskId, srcMemType, RefreshAddrInfo::HCCL_INPUT_MEMTYPE),
                            HCCL_E_INTERNAL);
                    }

                    // 更新src/dst memType
                    const struct rtCacheMemcpyTaskTag_t& cacheMemcpyTaskInfo = placeholderSqePtr->u.cache_memcpy_task_info;
                    if (srcMemType == RefreshAddrInfo::INVALID_MEMTYPE) { // LocalCopy/PrepareIntraData
                        // srcAddr一定是local user input的end addr
                        uint64_t sqeSrcAddr = 0;
                        CombineUint32ToUint64(sqeSrcAddr, cacheMemcpyTaskInfo.src_addr_high, cacheMemcpyTaskInfo.src_addr_low);
                        uint64_t localUserInputEndAddr = 0;
                        CHK_RET(userInputMemRanges_[curRank].GetEndAddr(localUserInputEndAddr));
                        CHK_PRT_RET(sqeSrcAddr != localUserInputEndAddr,
                            HCCL_ERROR("[OpUnfoldCacheEntry][UpdateRefreshAddrInfoForAlltoallv] sqeSrcAddr[0x%016llx]"\
                                "!= localUserInputEndAddr[0x%016llx]", sqeSrcAddr, localUserInputEndAddr),
                            HCCL_E_INTERNAL);
                        
                        // 更新src memType和rank, 即local user input
                        srcRefreshAddrInfo.memType = RefreshAddrInfo::USER_INPUT_MEMTYPE;
                        srcRefreshAddrInfo.rankId = curRank;
                        HCCL_INFO("[OpUnfoldCacheEntry][UpdateRefreshAddrInfoForAlltoallv] update invalid src "\
                            "RefreshAddrInfo for cache-memcpy placeholder: streamId[%u] taskId[%u] memType[%u] rankId[%u]",
                            placeholderStreamId, placeholderTaskId, srcRefreshAddrInfo.memType, srcRefreshAddrInfo.rankId);
                    }
                    if (dstMemType == RefreshAddrInfo::INVALID_MEMTYPE) { // LocalCopy/RemoteCopy
                        // dstAddr一定是local user output的end addr
                        uint64_t sqeDstAddr = 0;
                        CombineUint32ToUint64(sqeDstAddr, cacheMemcpyTaskInfo.dst_addr_high, cacheMemcpyTaskInfo.dst_addr_low);
                        uint64_t localUserOutputEndAddr = 0;
                        CHK_RET(userOutputMemRanges_[curRank].GetEndAddr(localUserOutputEndAddr));
                        CHK_PRT_RET(sqeDstAddr != localUserOutputEndAddr,
                            HCCL_ERROR("[OpUnfoldCacheEntry][UpdateRefreshAddrInfoForAlltoallv] sqeDstAddr[0x%016llx]"\
                                "!= localUserOutputEndAddr[0x%016llx]", sqeDstAddr, localUserOutputEndAddr),
                            HCCL_E_INTERNAL);
                        
                        // 更新dst memType和rank, 即local user output
                        dstRefreshAddrInfo.memType = RefreshAddrInfo::USER_OUTPUT_MEMTYPE;
                        dstRefreshAddrInfo.rankId = curRank;
                        HCCL_INFO("[OpUnfoldCacheEntry][UpdateRefreshAddrInfoForAlltoallv] update invalid dst "\
                            "RefreshAddrInfo for cache-memcpy placeholder: streamId[%u] taskId[%u] memType[%u] rankId[%u]",
                            placeholderStreamId, placeholderTaskId, dstRefreshAddrInfo.memType, dstRefreshAddrInfo.rankId);
                    }
                }

                // (ii) 更新local dst rank (如果dst memType是local hccl input)

                // PrepareIntraData: local user input -> local hccl input
                // 注意: invalid memType已经在当前函数的第(i)部分被解决
                if (srcRefreshAddrInfo.memType == RefreshAddrInfo::USER_INPUT_MEMTYPE &&
                    dstRefreshAddrInfo.memType == RefreshAddrInfo::HCCL_INPUT_MEMTYPE) {
                    // srcRank一定是current rank
                    const uint32_t srcRank = srcRefreshAddrInfo.rankId;
                    CHK_PRT_RET(srcRank != curRank,
                        HCCL_ERROR("[OpUnfoldCacheEntry][UpdateRefreshAddrInfoForAlltoallv] srcRank[%u] != curRank[%u], rankSize[%u]",
                            srcRank, curRank, rankSize),
                        HCCL_E_INTERNAL);

                    // 注意: 对于alltoallv算子, 原始dstRefreshAddrInfo.rankId在MemcpySqeArray时被刷新
                    // 因为alltoallvMetadata中的hcclOffsetDstRanksIdxMap要等第一次算子Orchestrate结束后才会被设置
                    // 而根据范围dstAddr落在local hccl input范围内, 因此当时刷新的rankId一定为current rank
                    CHK_PRT_RET(dstRefreshAddrInfo.rankId != curRank,
                        HCCL_ERROR("[OpUnfoldCacheEntry][UpdateRefreshAddrInfoForAlltoallv] original dstRank[%u] != curRank[%u]",
                            dstRefreshAddrInfo.rankId, curRank),
                        HCCL_E_INTERNAL);

                    // 根据SQE类型获得dst addr
                    uint64_t sqeDstAddr = 0;
                    if (sqeType == SqeType::MEMCPY_ASYNC_SQE) {
                        const rtStarsMemcpyAsyncSqe_t *memcpyAsyncSqePtr = reinterpret_cast<const rtStarsMemcpyAsyncSqe_t *>(sqePtr);
                        CombineUint32ToUint64(sqeDstAddr, memcpyAsyncSqePtr->dst_addr_high, memcpyAsyncSqePtr->dst_addr_low);
                    } else { // cache-memcpy placeholder
                        const rtStarsPlaceHolderSqe_t *placeholderSqePtr = reinterpret_cast<const rtStarsPlaceHolderSqe_t *>(sqePtr);
                        const struct rtCacheMemcpyTaskTag_t& cacheMemcpyTaskInfo = placeholderSqePtr->u.cache_memcpy_task_info;
                        CombineUint32ToUint64(sqeDstAddr, cacheMemcpyTaskInfo.dst_addr_high, cacheMemcpyTaskInfo.dst_addr_low);
                    }

                    // 本函数只会在第一次cache miss后处理时调用, dst addr一定落在local hccl input范围内
                    bool isInRange = false;
                    const OpUnfoldMemRange& localHcclInputMemRange = alltoallvMetadata.hcclInputMemRanges[curRank];
                    CHK_RET(localHcclInputMemRange.InRange(sqeDstAddr, isInRange));
                    if (UNLIKELY(!isInRange)) {
                        uint64_t localHcclInputEndAddr = 0;
                        CHK_RET(localHcclInputMemRange.GetEndAddr(localHcclInputEndAddr));
                        HCCL_ERROR("[OpUnfoldCacheEntry][UpdateRefreshAddrInfoForAlltoallv] sqeDstAddr[0x%016llx] not "\
                            "in localHcclInputMemRange[0x%016llx -- 0x%016llx]",
                            sqeDstAddr, localHcclInputMemRange.baseAddr, localHcclInputEndAddr);
                        return HCCL_E_INTERNAL;
                    }

                    // 参考alltoallv_direct_fullmesh.cc中的UpdateCurrRankSendInfo

                    // 计算local hccl input buffer下的offset
                    // 注意: 前面isInRange已经校验过必定为true, 即sqeDstAddr一定 >= localHcclInputBaseAddr
                    uint64_t localHcclInputBaseAddr = localHcclInputMemRange.baseAddr;
                    uint64_t hcclOffset = sqeDstAddr - localHcclInputBaseAddr;

                    // 根据hcclOffset-dstRanksInfo mapping获得对应的dst rank
                    std::unordered_map<uint64_t, RanksIdx>::iterator mapIter = alltoallvMetadata.hcclOffsetDstRanksIdxMap.find(hcclOffset);
                    if (mapIter == alltoallvMetadata.hcclOffsetDstRanksIdxMap.end()) {
                        HCCL_ERROR("[OpUnfoldCacheEntry][UpdateRefreshAddrInfoForAlltoallv] hcclOffset[%u] is not found", hcclOffset);
                        return HCCL_E_INTERNAL;
                    }
                    const std::vector<uint32_t>& dstRanks = mapIter->second.first;
                    uint32_t& curIdx = mapIter->second.second;
                    const uint32_t dstRank = dstRanks[curIdx % dstRanks.size()];
                    HCCL_INFO("[OpUnfoldCacheEntry][UpdateRefreshAddrInfoForAlltoallv] hcclOffset[%llu] dstRanks.size[%u] "\
                        "curIdx[%u] dstRank[%u]", hcclOffset, dstRanks.size(), curIdx, dstRank);

                    // 新的dstRank一定是某个remoteRank, 即不等于curRank
                    CHK_PRT_RET(dstRank == curRank,
                        HCCL_ERROR("[OpUnfoldCacheEntry][UpdateRefreshAddrInfoForAlltoallv] dstRank[%u] = curRank[%u]",
                            dstRank, curRank),
                        HCCL_E_INTERNAL);

                    // Prepare for next-round dstRank (if any)
                    curIdx = (curIdx + 1) % dstRanks.size();

                    // 更新dst rank
                    dstRefreshAddrInfo.rankId = dstRank;
                    HCCL_INFO("[OpUnfoldCacheEntry][UpdateRefreshAddrInfoForAlltoallv]"\
                        "dstRefreshAddrInfoArrays_[%u][%u].rankId[%u -> %u]",
                        arrayIdx, sqeIdx, curRank, dstRefreshAddrInfo.rankId);
                } // PrepareIntraData in alltoallv

                // 刷新memcpy / cache-memcpy placeholder SQE后, 校验src/dst地址字段对应的memType
                CHK_RET(CheckMemTypeForAlltoallv(sqePtr, sqeType, srcRefreshAddrInfo, dstRefreshAddrInfo));
            } // sqeIdx
        } // arrayIdx

        return HCCL_SUCCESS;
    }

    HcclResult OpUnfoldCacheEntry::UpdateAndGetSqeArray(const size_t arrayIdx, const std::vector<OpUnfoldMemRange>& curUserInputMemRanges,
        const std::vector<OpUnfoldMemRange>& curUserOutputMemRanges, Stream& mainStream, std::vector<Stream> &slaveStreams,
        const uint32_t opRingBufferIdx, size_t& sqeCount, uint8_t **sqeArrayPtr, uint8_t **sqeTypeArrayPtr,
        AicpuDfxInfo **sqeDfxInfoArrayPtr, Stream **streamPtrPtr, std::vector<FlipInfo>& flipInfos,
        const bool profL1Enable, std::vector<uint64_t>& profTimestamps, const bool isAlltoallv,
        const AlltoallvMetadata& alltoallvMetadata, const AlltoallvSendRecvInfo& alltoallvSendRecvInfo) {
        FUNCTION_TRACE; // TODOSSY: 性能打点
            
        HCCL_INFO("[OpUnfoldCacheEntry][UpdateAndGetSqeArray] update and get SQEs from %uth SQE array; isAlltoallv[%u]", arrayIdx, isAlltoallv);

        // 检验入参
        CHK_PRT_RET(arrayIdx >= sqeArrays_.size(), HCCL_ERROR("[OpUnfoldCacheEntry][MemcpySqeArray] arrayIdx %u is out of range [0, %u)", arrayIdx, sqeArrays_.size()), HCCL_E_INTERNAL);
        CHK_PRT_RET(arrayIdx >= streamSeqIdxes_.size(), HCCL_ERROR("[OpUnfoldCacheEntry][MemcpySqeArray] arrayIdx %u is out of range [0, %u)", arrayIdx, streamSeqIdxes_.size()), HCCL_E_INTERNAL);
        // 检查指针, arrayPtr不应该是null, 但*arrayPtr应该是null
        CHK_PTRPTR_NULL(sqeArrayPtr);
        CHK_PTRPTR_NULL(sqeTypeArrayPtr);
        CHK_PTRPTR_NULL(sqeDfxInfoArrayPtr);
        CHK_PTRPTR_NULL(streamPtrPtr);
        // Double-check alltoallv相关入参
        if (isAlltoallv) {
            CHK_RET(alltoallvMetadata.Check(true));
            CHK_RET(alltoallvSendRecvInfo.Check());
        }

        // 设置入参
        sqeCount = srcRefreshAddrInfoArrays_[arrayIdx].size();
        *sqeArrayPtr = sqeArrays_[arrayIdx];
        *sqeTypeArrayPtr = sqeTypeArrays_[arrayIdx];
        *sqeDfxInfoArrayPtr = sqeDfxInfoArrays_[arrayIdx];
        flipInfos.clear();
        if (profL1Enable) {
            profTimestamps.clear();
            profTimestamps.reserve(sqeCount); // 需要额外flip placeholder是小概率事件, 所以只reserve cached SQE个数 (即非flip placeholder类SQE)
        }

        // 设置入参的stream pointer
        const uint32_t streamSeqIdx = streamSeqIdxes_[arrayIdx];
        if (streamSeqIdx == 0) {
            *streamPtrPtr = &mainStream;
        } else {
            CHK_PRT_RET(streamSeqIdx > slaveStreams.size(), HCCL_ERROR("[OpUnfoldCacheEntry][UpdateAndGetSqeArray] invalid streamSeqIdx %u > slaveStreams.size() %u", streamSeqIdx, slaveStreams.size()), HCCL_E_MEMORY);
            *streamPtrPtr = &(slaveStreams[streamSeqIdx - 1]); // 0 < streamSeqIdx <= slaveStreams.size())
        }

        // 从stream中获取SQE刷新需要的当前task id
        HcclSqeContext *sqeContext = (*streamPtrPtr)->GetSqeContextPtr();
        CHK_PTR_NULL(sqeContext);
        SqeRingBuffer *sqeContextBuffer = &(sqeContext->buffer);
        CHK_PTR_NULL(sqeContextBuffer);
        uint16_t& curTaskId = sqeContextBuffer->tailSqeTaskId;
        uint16_t& curFlipNum = sqeContextBuffer->filpNum;
        HCCL_INFO("[OpUnfoldCacheEntry][UpdateAndGetSqeArray] curTaskId[%u] curFlipNum[%u] from streamId %u for %u cached SQEs",
            curTaskId, curFlipNum, (*streamPtrPtr)->GetHcclStreamInfo().actualStreamId, sqeCount);

        // 执行SQE刷新
        // 注意: curUserInputMemRanges/curUserOutputMemRanges为当前算子执行时各rank输入输出的user memory range, userInputMemRanges_/userOutputMemRanges_为算子缓存时各rank输入输出的user memory range
        const std::vector<RefreshAddrInfo>& srcRefreshAddrInfoArray = srcRefreshAddrInfoArrays_[arrayIdx];
        const std::vector<RefreshAddrInfo>& dstRefreshAddrInfoArray = dstRefreshAddrInfoArrays_[arrayIdx];
        uint64_t sqeSrcAddr = 0;
        uint64_t sqeDstAddr = 0;
        uint8_t *sqePtr = (*sqeArrayPtr);
        for (size_t sqeIdx = 0 ; sqeIdx < sqeCount; ++sqeIdx){
            // 获取当前SQE的信息
            uint8_t& sqeType = (*sqeTypeArrayPtr)[sqeIdx];
            const RefreshAddrInfo& srcRefreshAddrInfo = srcRefreshAddrInfoArray[sqeIdx];
            const RefreshAddrInfo& dstRefreshAddrInfo = dstRefreshAddrInfoArray[sqeIdx];
            HCCL_INFO("[OpUnfoldCacheEntry][UpdateAndGetSqeArray] update %uth cached SQE with sqeType[%u] srcRefreshAddrInfo[rankid[%u], memType[%u]] dstRefreshAddrInfo[rankid[%u], memType[%u]] curTaskId[%u]",
                sqeIdx, sqeType, srcRefreshAddrInfo.rankId, srcRefreshAddrInfo.memType, dstRefreshAddrInfo.rankId, dstRefreshAddrInfo.memType, curTaskId);

            // 根据SQE type进行对应刷新 (task id始终要刷新; addr相关字段有条件刷新)
            switch(sqeType) {
                case SqeType::NOTIFY_SQE: {
                    rtStarsNotifySqeV1_t *notifySqePtr = reinterpret_cast<rtStarsNotifySqeV1_t *>(sqePtr);
                    if (isAlltoallv && notifySqePtr->header.type == RT_STARS_SQE_TYPE_NOTIFY_WAIT) {
                        // 针对alltoallv算子, 动态刷新Notify SQE / 将Notify SQE生成为CacheNotifyPlaceholder SQE
                        // 注意: NotifyRecord SQE不需要针对alltoallv类算子做特殊刷新
                        CHK_RET(UpdateSyncSqeForAlltoallv(sqePtr, &sqeType, curTaskId, srcRefreshAddrInfo, dstRefreshAddrInfo, alltoallvMetadata, alltoallvSendRecvInfo));
                    } else {
                        notifySqePtr->header.taskId = curTaskId;
                    }
                    break;
                }
                case SqeType::WRITE_VALUE_SQE:
                case SqeType::RDMA_DB_SEND_SQE: {
                    if (isAlltoallv) { // 针对alltoallv算子, 动态刷新WriteValue SQE / 将WriteValue SQE生成为CacheWriteValuePlaceholder SQE
                        CHK_RET(UpdateSyncSqeForAlltoallv(sqePtr, &sqeType, curTaskId, srcRefreshAddrInfo, dstRefreshAddrInfo, alltoallvMetadata, alltoallvSendRecvInfo));
                    } else {
                        rtStarsWriteValueSqe_t *writeValueSqePtr = reinterpret_cast<rtStarsWriteValueSqe_t *>(sqePtr);
                        writeValueSqePtr->header.taskId = curTaskId;

                        if (dstRefreshAddrInfo.memType != RefreshAddrInfo::INVALID_MEMTYPE) { // 需要刷新地址
                            CombineUint32ToUint64(sqeDstAddr, writeValueSqePtr->write_addr_high, writeValueSqePtr->write_addr_low);
                            if (dstRefreshAddrInfo.memType == RefreshAddrInfo::USER_OUTPUT_MEMTYPE) { // user output
                                CHK_RET(RefreshSqeAddr(sqeDstAddr, dstRefreshAddrInfo.rankId, userOutputMemRanges_, curUserOutputMemRanges, false, 0));
                            } else if (dstRefreshAddrInfo.memType == RefreshAddrInfo::USER_INPUT_MEMTYPE) { // user input
                                CHK_RET(RefreshSqeAddr(sqeDstAddr, dstRefreshAddrInfo.rankId, userInputMemRanges_, curUserInputMemRanges, false, 0));
                            } else { // hccl input (alltoallv算子不需要对WRITE_VALUE_SQE刷新hccl地址)
                                HCCL_ERROR("[OpUnfoldCacheEntry][UpdateAndGetSqeArray] isAlltoallv[%u] sqeType[%u]; memType should be user input/output", isAlltoallv, sqeType);
                                HCCL_ERROR("[OpUnfoldCacheEntry][UpdateAndGetSqeArray] dstRefreshAddrInfo: rankId[%u] memType[%u]", dstRefreshAddrInfo.rankId, dstRefreshAddrInfo.memType);
                                return HCCL_E_INTERNAL;
                            }

                            // Bit-field member不能直接传引用
                            uint32_t tmpHighAddr = 0;
                            SplitUint64ToUint32(sqeDstAddr, tmpHighAddr, writeValueSqePtr->write_addr_low);
                            writeValueSqePtr->write_addr_high = tmpHighAddr;
                        }
                    }
                    break;
                }
                case SqeType::EVENT_SQE: {
                    rtStarsEventSqe_t *eventSqePtr = reinterpret_cast<rtStarsEventSqe_t *>(sqePtr);
                    eventSqePtr->header.taskId = curTaskId;
                    break;
                }
                case SqeType::MEMCPY_ASYNC_SQE: {
                    if (isAlltoallv) { // 针对alltoallv算子
                        if (srcRefreshAddrInfo.memType == RefreshAddrInfo::INVALID_MEMTYPE &&
                            dstRefreshAddrInfo.memType == RefreshAddrInfo::INVALID_MEMTYPE) { // MemcpyRecord SQE
                            // 动态刷新MemcpyRecord SQE / 将MemcpyRecord SQE生成为CacheMemcpyRecordPlaceholder SQE
                            CHK_RET(UpdateSyncSqeForAlltoallv(sqePtr, &sqeType, curTaskId, srcRefreshAddrInfo, dstRefreshAddrInfo, alltoallvMetadata, alltoallvSendRecvInfo));
                        } else { // Memcpy SQE
                            // 动态刷新Memcpy SQE / 将Memcpy SQE生成为CacheMemcpyPlaceholder SQE
                            CHK_RET(UpdateTransferSqeForAlltoallv(sqePtr, &sqeType, curTaskId, srcRefreshAddrInfo, dstRefreshAddrInfo, curUserInputMemRanges, curUserOutputMemRanges, alltoallvMetadata, alltoallvSendRecvInfo));
                        }
                    } else { // 非V类算子, 动态刷新Memcpy SQE
                        rtStarsMemcpyAsyncSqe_t *memcpyAsyncSqePtr = reinterpret_cast<rtStarsMemcpyAsyncSqe_t *>(sqePtr);
                        memcpyAsyncSqePtr->header.taskId = curTaskId;

                        if (srcRefreshAddrInfo.memType != RefreshAddrInfo::INVALID_MEMTYPE) { // 需要刷新src addr
                            CombineUint32ToUint64(sqeSrcAddr, memcpyAsyncSqePtr->src_addr_high, memcpyAsyncSqePtr->src_addr_low);
                            if (srcRefreshAddrInfo.memType == RefreshAddrInfo::USER_OUTPUT_MEMTYPE) { // user output
                                CHK_RET(RefreshSqeAddr(sqeSrcAddr, srcRefreshAddrInfo.rankId, userOutputMemRanges_, curUserOutputMemRanges, false, 0));
                            } else if (srcRefreshAddrInfo.memType == RefreshAddrInfo::USER_INPUT_MEMTYPE) { // user input
                                CHK_RET(RefreshSqeAddr(sqeSrcAddr, srcRefreshAddrInfo.rankId, userInputMemRanges_, curUserInputMemRanges, false, 0));
                            } else { // hccl input (非V类算子不需要对MEMCPY_ASYNC_SQE刷新hccl地址)
                                HCCL_ERROR("[OpUnfoldCacheEntry][UpdateAndGetSqeArray] isAlltoallv[%u] sqeType[%u]; memType should be user input/output", isAlltoallv, sqeType);
                                HCCL_ERROR("[OpUnfoldCacheEntry][UpdateAndGetSqeArray] srcRefreshAddrInfo: rankId[%u] memType[%u]", srcRefreshAddrInfo.rankId, srcRefreshAddrInfo.memType);
                                return HCCL_E_INTERNAL;
                            }
                            SplitUint64ToUint32(sqeSrcAddr, memcpyAsyncSqePtr->src_addr_high, memcpyAsyncSqePtr->src_addr_low);
                        }

                        if (dstRefreshAddrInfo.memType != RefreshAddrInfo::INVALID_MEMTYPE) { // 需要刷新地址
                            CombineUint32ToUint64(sqeDstAddr, memcpyAsyncSqePtr->dst_addr_high, memcpyAsyncSqePtr->dst_addr_low);
                            if (dstRefreshAddrInfo.memType == RefreshAddrInfo::USER_OUTPUT_MEMTYPE) { // user output
                                CHK_RET(RefreshSqeAddr(sqeDstAddr, dstRefreshAddrInfo.rankId, userOutputMemRanges_, curUserOutputMemRanges, false, 0));
                            } else if (dstRefreshAddrInfo.memType == RefreshAddrInfo::USER_INPUT_MEMTYPE) { // user input
                                CHK_RET(RefreshSqeAddr(sqeDstAddr, dstRefreshAddrInfo.rankId, userInputMemRanges_, curUserInputMemRanges, false, 0));
                            } else { // hccl input (非V类算子不需要对MEMCPY_ASYNC_SQE刷新hccl地址)
                                HCCL_ERROR("[OpUnfoldCacheEntry][UpdateAndGetSqeArray] isAlltoallv[%u] sqeType[%u]; memType should be user input/output", isAlltoallv, sqeType);
                                HCCL_ERROR("[OpUnfoldCacheEntry][UpdateAndGetSqeArray] dstRefreshAddrInfo: rankId[%u] memType[%u]", dstRefreshAddrInfo.rankId, dstRefreshAddrInfo.memType);
                                return HCCL_E_INTERNAL;
                            }
                            SplitUint64ToUint32(sqeDstAddr, memcpyAsyncSqePtr->dst_addr_high, memcpyAsyncSqePtr->dst_addr_low);
                        }
                    }

                    break;
                }
                case SqeType::CCORE_WAIT_START_SQE: {
                    HCCL_ERROR("[OpUnfoldCacheEntry][UpdateAndGetSqeArray] SqeType::CCORE_WAIT_START_SQE is not supported in A3");
                    return HCCL_E_NOT_SUPPORT;
                }
                case SqeType::CCORE_WRITE_VALUE_SQE: {
                    HCCL_ERROR("[OpUnfoldCacheEntry][UpdateAndGetSqeArray] SqeType::CCORE_WRITE_VALUE_SQE is not supported in A3");
                    return HCCL_E_NOT_SUPPORT;
                }
                case SqeType::NOTIFY_SQE_V2: {
                    HCCL_ERROR("[OpUnfoldCacheEntry][UpdateAndGetSqeArray] SqeType::NOTIFY_SQE_V2 is not supported in A3");
                    return HCCL_E_NOT_SUPPORT;
                }
                case SqeType::WRITE_VALUE_SQE_V2: {
                    HCCL_ERROR("[OpUnfoldCacheEntry][UpdateAndGetSqeArray] SqeType::WRITE_VALUE_SQE_V2 is not supported in A3");
                    return HCCL_E_NOT_SUPPORT;
                }
                case SqeType::EVENT_SQE_V2: {
                    HCCL_ERROR("[OpUnfoldCacheEntry][UpdateAndGetSqeArray] SqeType::EVENT_SQE_V2 is not supported in A3");
                    return HCCL_E_NOT_SUPPORT;
                }
                case SqeType::MEMCPY_ASYNC_SQE_V2: {
                    HCCL_ERROR("[OpUnfoldCacheEntry][UpdateAndGetSqeArray] SqeType::MEMCPY_ASYNC_SQE_V2 is not supported in A3");
                    return HCCL_E_NOT_SUPPORT;
                }
                case SqeType::FLIP_PLACEHOLDER_SQE: {
                    HCCL_ERROR("[OpUnfoldCacheEntry][UpdateAndGetSqeArray] placeholder should not be cached, sqeType[%u] sqeIdx[%u]", sqeType, sqeIdx);
                    return HCCL_E_INTERNAL;
                }
                case SqeType::CACHE_MEMCPY_PLACEHOLDER_SQE: {
                    // 非V类算子, 不应该出现CacheMemcpyPlaceholder SQE
                    CHK_PRT_RET(!isAlltoallv,
                        HCCL_ERROR("[OpUnfoldCacheEntry][UpdateAndGetSqeArray] cache-memcpy placeholder"
                            "should be not cached for non-alltoallv op, sqeType[%u] sqeIdx[%u]", sqeType, sqeIdx),
                        HCCL_E_INTERNAL);

                    // 针对alltoallv类算子, 动态刷新CacheMemcpyPlaceholder SQE / 将CacheMemcpyPlaceholder SQE生成为Memcpy SQE
                    CHK_RET(UpdateTransferSqeForAlltoallv(sqePtr, &sqeType, curTaskId, srcRefreshAddrInfo, dstRefreshAddrInfo, curUserInputMemRanges, curUserOutputMemRanges, alltoallvMetadata, alltoallvSendRecvInfo));

                    break;
                }
                case SqeType::CACHE_NOTIFY_PLACEHOLDER_SQE:
                case SqeType::CACHE_WRITE_VALUE_PLACEHOLDER_SQE:
                case SqeType::CACHE_MEMCPY_RECORD_PLACEHOLDER_SQE: {
                    // 非V类算子, 不应该出现CacheNotify/Write/MemcpyRecordPlaceholder SQE
                    CHK_PRT_RET(!isAlltoallv,
                        HCCL_ERROR("[OpUnfoldCacheEntry][UpdateAndGetSqeArray] cache-notify/write/memcpy-record placeholder"
                            "should be not cached for non-alltoallv op, sqeType[%u] sqeIdx[%u]", sqeType, sqeIdx),
                        HCCL_E_INTERNAL);
                    
                    // 针对alltoallv类算子, 动态刷新CacheNotify/WritePlaceholder SQE / 将CacheNotify/WritePlaceholder SQE生成为Notify/Write SQE
                    CHK_RET(UpdateSyncSqeForAlltoallv(sqePtr, &sqeType, curTaskId, srcRefreshAddrInfo, dstRefreshAddrInfo, alltoallvMetadata, alltoallvSendRecvInfo));

                    break;
                }
                default: {
                    HCCL_WARNING("[OpUnfoldCacheEntry][UpdateAndGetSqeArray] sqeType %u is unsupported (sqeIdx[%u] curTaskId[%u])",
                        sqeType, sqeIdx, curTaskId);
                    return HCCL_E_NOT_SUPPORT;
                }
            }

            // 记录SQE刷新时间用于profiling
            if (profL1Enable) {
                const uint64_t curTime = ProfGetCurCpuTimestamp();
                profTimestamps.push_back(curTime);
            }

            // 刷新taskId和flipNum
            if (curTaskId == UINT16_MAX) { // 更新flipNum和taskId
                // 参考stream.cc中的GetNextSqeBufferAddr
                curFlipNum += 1;
                curTaskId = 0;
            } else if (curTaskId == 0 && curFlipNum != 0) { // 更新flipInfos和taskId
                // 参考dispatcher_aicpu.cc中的GetStreamSqeBufferAddr
                flipInfos.push_back(FlipInfo(sqeIdx, curFlipNum));

                // 为placeholder SQE预留task id = 0
                curTaskId = 1;

                // Flip placeholder SQE在外侧dispatcher aicpu中生成, 这里记录当前时间作为flip placeholder SQE的生成时间
                if (profL1Enable) {
                    const uint64_t curTime = ProfGetCurCpuTimestamp();
                    profTimestamps.push_back(curTime);
                }
            } else { // 只更新taskid
                curTaskId += 1;
            }

            sqePtr += HCCL_SQE_SIZE;
        }

        // 更新每个SQE的DfxInfo中的opRingBufferIdx
        HCCL_INFO("[OpUnfoldCacheEntry][UpdateAndGetSqeArray] update opRingBufferIndx in DfxInfoArray as %u", opRingBufferIdx);
        for (size_t sqeIdx = 0; sqeIdx < sqeCount; ++sqeIdx) {
            (*sqeDfxInfoArrayPtr)[sqeIdx].opRingBufferIdx = opRingBufferIdx;
        }

        HCCL_INFO("[OpUnfoldCacheEntry][UpdateAndGetSqeArray] update and get %uth SQE array with %u SQEs, streamId[%u] and %u flipInfos",
            arrayIdx, sqeCount, (*streamPtrPtr)->GetHcclStreamInfo().actualStreamId, flipInfos.size());

        return HCCL_SUCCESS;
    }

    HcclResult OpUnfoldCacheEntry::SetInputOutputMemRanges(const std::vector<OpUnfoldMemRange>& curUserInputMemRanges, const std::vector<OpUnfoldMemRange>& curUserOutputMemRanges) {
        CHK_PRT_RET(userInputMemRanges_.size() != curUserInputMemRanges.size(), HCCL_ERROR("[OpUnfoldCacheEntry][SetInputOutputMemRanges] original rankSize %u != new rankSize %u", userInputMemRanges_.size(), curUserInputMemRanges.size()), HCCL_E_INTERNAL);

        userInputMemRanges_ = curUserInputMemRanges;
        userOutputMemRanges_ = curUserOutputMemRanges;

        return HCCL_SUCCESS;
    }

    HcclResult OpUnfoldCacheEntry::CheckAndPrepareRefreshAddrInfo(const uint64_t sqeAddr, RefreshAddrInfo& refreshAddrInfo, const bool isAlltoallv, const AlltoallvMetadata* alltoallvMetadataPtr) {
        // 遍历per-rank user input memory range
        for (size_t rankId = 0; rankId < userInputMemRanges_.size(); ++rankId) {
            bool isInRange = false;
            CHK_RET(userInputMemRanges_[rankId].InRange(sqeAddr, isInRange));
            if (isInRange) {
                refreshAddrInfo.rankId = rankId;
                refreshAddrInfo.memType = RefreshAddrInfo::USER_INPUT_MEMTYPE;
                return HCCL_SUCCESS; // 确实是某rank下的user input mem, 则无需继续搜索output mem
            }
        }

        // 遍历per-rank user input memory range
        for (size_t rankId = 0; rankId < userOutputMemRanges_.size(); ++rankId) {
            bool isInRange = false;
            CHK_RET(userOutputMemRanges_[rankId].InRange(sqeAddr, isInRange));
            if (isInRange) {
                refreshAddrInfo.rankId = rankId;
                refreshAddrInfo.memType = RefreshAddrInfo::USER_OUTPUT_MEMTYPE;
                return HCCL_SUCCESS;
            }
        }

        // 针对alltoallv算子, 遍历HCCL input buffer, 确定rank id
        if (isAlltoallv) {
            CHK_PTR_NULL(alltoallvMetadataPtr);
            CHK_RET(alltoallvMetadataPtr->Check(false));

            const std::vector<OpUnfoldMemRange>& hcclInputMemRanges = alltoallvMetadataPtr->hcclInputMemRanges;
            for (size_t rankId = 0; rankId < hcclInputMemRanges.size(); ++rankId) {
                bool isInRange = false;
                CHK_RET(hcclInputMemRanges[rankId].InRange(sqeAddr, isInRange));
                if (isInRange) {
                    refreshAddrInfo.rankId = rankId;
                    refreshAddrInfo.memType = RefreshAddrInfo::HCCL_INPUT_MEMTYPE;
                    return HCCL_SUCCESS;
                }
            }
        }

        return HCCL_SUCCESS;
    }

    HcclResult OpUnfoldCacheEntry::CheckMemTypeForAlltoallv(const uint8_t *sqePtr, const uint8_t sqeType,
        const RefreshAddrInfo& srcRefreshAddrInfo, const RefreshAddrInfo& dstRefreshAddrInfo) const {
        // 对于所有算子, SQE第一次admit时, 在MemcpySqeArray中, 遍历memory ranges获取每个地址字段对应的rankId和memType
        // 对于alltoallv算子, cache miss后处理时, 在UpdateRefreshAddrInfoForAlltoallv中, 进一步更新memType和dstRank

        // 如果src在user memory内, 说明是LocalCopy或者PrepareIntraData, 一定是local user input -> local user output / local hccl input
        // 如果dst在user memory内, 说明是LocalCopy或者RemoteCopy, 一定是local user input / remote hccl input -> local user output

        // 获取task id和stream id
        uint16_t taskId = 0;
        uint16_t streamId = 0;
        if (sqeType == SqeType::MEMCPY_ASYNC_SQE) {
            const rtStarsMemcpyAsyncSqe_t *memcpyAsyncSqePtr = reinterpret_cast<const rtStarsMemcpyAsyncSqe_t *>(sqePtr);
            taskId = memcpyAsyncSqePtr->header.taskId;
            streamId = memcpyAsyncSqePtr->header.rtStreamId;
        } else if (sqeType == SqeType::CACHE_MEMCPY_PLACEHOLDER_SQE) {
            const rtStarsPlaceHolderSqe_t *placeholderSqePtr = reinterpret_cast<const rtStarsPlaceHolderSqe_t *>(sqePtr);
            taskId = placeholderSqePtr->header.taskId;
            streamId = placeholderSqePtr->header.rtStreamId;
        } else {
            HCCL_ERROR("[OpUnfoldCacheEntry][CheckMemTypeForAlltoallv] invalid sqeType[%u]", sqeType);
            return HCCL_E_INTERNAL;
        }

        // memcpy / cache-memcpy placeholder SQE的src/dst memType一定不是invalid
        // 注意: memcpy-record SQE的src/dst一定是invalid, 但不会进入本函数
        const uint8_t srcMemType = srcRefreshAddrInfo.memType;
        const uint8_t dstMemType = dstRefreshAddrInfo.memType;
        CHK_PRT_RET(srcMemType == RefreshAddrInfo::INVALID_MEMTYPE || dstMemType == RefreshAddrInfo::INVALID_MEMTYPE,
            HCCL_ERROR("[OpUnfoldCacheEntry][CheckMemTypeForAlltoallv] sqeType[%u] streamId[%u] taskId[%u] srcMemType[%u] dstMemType[%u]",
                sqeType, streamId, taskId, srcMemType, dstMemType),
            HCCL_E_INTERNAL);

        // src一定不是user output
        CHK_PRT_RET(srcMemType == RefreshAddrInfo::USER_OUTPUT_MEMTYPE,
            HCCL_ERROR("[OpUnfoldCacheEntry][CheckMemTypeForAlltoallv] sqeType[%u] streamId[%u] taskId[%u] srcMemType[%u]",
                sqeType, streamId, taskId, srcMemType),
            HCCL_E_INTERNAL);

        // src是user input时, dst一定是user output / hccl input
        // src是hccl input时, dst一定是user output
        if (srcMemType == RefreshAddrInfo::USER_INPUT_MEMTYPE) {
            CHK_PRT_RET(dstMemType != RefreshAddrInfo::USER_OUTPUT_MEMTYPE && dstMemType != RefreshAddrInfo::HCCL_INPUT_MEMTYPE,
                HCCL_ERROR("[OpUnfoldCacheEntry][CheckMemTypeForAlltoallv] sqeType[%u] streamId[%u] taskId[%u] dstMemType[%u]",
                sqeType, streamId, taskId, dstMemType),
            HCCL_E_INTERNAL);
        } else if (srcMemType == RefreshAddrInfo::HCCL_INPUT_MEMTYPE) {
            CHK_PRT_RET(dstMemType != RefreshAddrInfo::USER_OUTPUT_MEMTYPE,
                HCCL_ERROR("[OpUnfoldCacheEntry][CheckMemTypeForAlltoallv] sqeType[%u] streamId[%u] taskId[%u] dstMemType[%u]",
                sqeType, streamId, taskId, dstMemType),
            HCCL_E_INTERNAL);
        }

        // dst一定不是user input
        CHK_PRT_RET(dstMemType == RefreshAddrInfo::USER_INPUT_MEMTYPE,
            HCCL_ERROR("[OpUnfoldCacheEntry][CheckMemTypeForAlltoallv] sqeType[%u] streamId[%u] taskId[%u] dstMemType[%u]",
                sqeType, streamId, taskId, dstMemType),
            HCCL_E_INTERNAL);

        // dst是user output时, src一定是user input / hccl input
        // dst是hccl input时, src一定是user input
        if (dstMemType == RefreshAddrInfo::USER_OUTPUT_MEMTYPE) {
            CHK_PRT_RET(srcMemType != RefreshAddrInfo::USER_INPUT_MEMTYPE && srcMemType != RefreshAddrInfo::HCCL_INPUT_MEMTYPE,
                HCCL_ERROR("[OpUnfoldCacheEntry][CheckMemTypeForAlltoallv] sqeType[%u] streamId[%u] taskId[%u] srcMemType[%u]",
                sqeType, streamId, taskId, srcMemType),
            HCCL_E_INTERNAL);
        } else if (dstMemType == RefreshAddrInfo::HCCL_INPUT_MEMTYPE) {
            CHK_PRT_RET(srcMemType != RefreshAddrInfo::USER_INPUT_MEMTYPE,
                HCCL_ERROR("[OpUnfoldCacheEntry][CheckMemTypeForAlltoallv] sqeType[%u] streamId[%u] taskId[%u] srcMemType[%u]",
                sqeType, streamId, taskId, srcMemType),
            HCCL_E_INTERNAL);
        }

        return HCCL_SUCCESS;
    }

    HcclResult OpUnfoldCacheEntry::UpdateTransferSqeForAlltoallv(uint8_t *sqePtr, uint8_t *sqeTypePtr, const uint16_t curTaskId, const RefreshAddrInfo& srcRefreshAddrInfo, const RefreshAddrInfo& dstRefreshAddrInfo, const std::vector<OpUnfoldMemRange>& curUserInputMemRanges, const std::vector<OpUnfoldMemRange>& curUserOutputMemRanges, const AlltoallvMetadata& alltoallvMetadata, const AlltoallvSendRecvInfo& alltoallvSendRecvInfo) {
        HCCL_INFO("[OpUnfoldCacheEntry][UpdateTransferSqeForAlltoallv] curTaskId[%u] srcMemType[%u] dstMemType[%u]",
            curTaskId, srcRefreshAddrInfo.memType, dstRefreshAddrInfo.memType);
        
        // 先判断是否需要刷新此memcpy SQE
        if (srcRefreshAddrInfo.memType != RefreshAddrInfo::USER_INPUT_MEMTYPE && dstRefreshAddrInfo.memType != RefreshAddrInfo::USER_OUTPUT_MEMTYPE) {
            // 注意: 由于alltoallv direct fullmesh不会调用InlineReduceAsync和基于memcpy的SignalRecord
            // 理论上所有的memcpy / cache-memcpy SQE都需要地址刷新, 不会进入当前code block

            // 更新task id
            if ((*sqeTypePtr) == SqeType::MEMCPY_ASYNC_SQE) {
                rtStarsMemcpyAsyncSqe_t *memcpyAsyncSqePtr = reinterpret_cast<rtStarsMemcpyAsyncSqe_t *>(sqePtr);
                memcpyAsyncSqePtr->header.taskId = curTaskId;
            } else if ((*sqeTypePtr) == SqeType::CACHE_MEMCPY_PLACEHOLDER_SQE) {
                rtStarsPlaceHolderSqe_t *placeholderSqePtr = reinterpret_cast<rtStarsPlaceHolderSqe_t *>(sqePtr);
                placeholderSqePtr->header.taskId = curTaskId;
            } else {
                HCCL_ERROR("[OpUnfoldCacheEntry][UpdateTransferSqeForAlltoallv] invalid sqeType[%u]", *sqeTypePtr);
                return HCCL_E_INTERNAL;
            }

            return HCCL_SUCCESS;
        }
        // 注意: 从这里开始, srcAddr为userInput和dstAddr为userOutput至少有一个条件满足

        // 校验alltoallv相关参数
        CHK_RET(alltoallvMetadata.Check(true));
        CHK_RET(alltoallvSendRecvInfo.Check());
        const uint32_t rankSize = alltoallvSendRecvInfo.sendOffsets.size();
        CHK_PRT_RET(rankSize != alltoallvMetadata.hcclInputMemRanges.size(),
            HCCL_ERROR("[OpUnfoldCacheEntry][UpdateTransferSqeForAlltoallv] hcclInputMemRanges.size[%u] != rankSize[%u]",
                alltoallvMetadata.hcclInputMemRanges.size(), rankSize),
            HCCL_E_INTERNAL);

        // 获取当前memcpy类SQE对应的count和size
        uint64_t count = 0;
        uint64_t size = 0; // send/recv bytes
        CHK_RET(GetTransferCountForAlltoallv(count, size, srcRefreshAddrInfo, dstRefreshAddrInfo, alltoallvMetadata, alltoallvSendRecvInfo));

        // 更新/生成相应SQE
        if ((*sqeTypePtr) == SqeType::MEMCPY_ASYNC_SQE) {
            CHK_RET(UpdateMemcpySqeForAlltoallv(sqePtr, sqeTypePtr, curTaskId, srcRefreshAddrInfo, dstRefreshAddrInfo, curUserInputMemRanges, curUserOutputMemRanges, alltoallvMetadata, alltoallvSendRecvInfo, count, size));
        } else if ((*sqeTypePtr) == SqeType::CACHE_MEMCPY_PLACEHOLDER_SQE) {
            CHK_RET(UpdateMemcpyPlaceholderSqeForAlltoallv(sqePtr, sqeTypePtr, curTaskId, srcRefreshAddrInfo, dstRefreshAddrInfo, curUserInputMemRanges, curUserOutputMemRanges, alltoallvMetadata, alltoallvSendRecvInfo, count, size));
        } else {
            HCCL_ERROR("[OpUnfoldCacheEntry][UpdateTransferSqeForAlltoallv] invalid sqeType[%u]", *sqeTypePtr);
            return HCCL_E_INTERNAL;
        }

        return HCCL_SUCCESS;
    }

    HcclResult OpUnfoldCacheEntry::GetTransferCountForAlltoallv(uint64_t& count, uint64_t& size, const RefreshAddrInfo& srcRefreshAddrInfo, const RefreshAddrInfo& dstRefreshAddrInfo, const AlltoallvMetadata& alltoallvMetadata, const AlltoallvSendRecvInfo& alltoallvSendRecvInfo) const {
        // 注意: 如果对应send count, 一定是LocalCopy或者PrepareIntraData, 即local user input -> local user output / hccl input
        // 注意: 如果对应recv count, 一定是RemoteCopy, 即remote hccl input -> local user output
        const uint32_t rankSize = alltoallvSendRecvInfo.sendOffsets.size();
        if (srcRefreshAddrInfo.memType == RefreshAddrInfo::USER_INPUT_MEMTYPE) { // LocalCopy/PrepareIntraData
            // 获得local rank
            uint32_t localRank = srcRefreshAddrInfo.rankId;
            CHK_PRT_RET(localRank >= rankSize, HCCL_ERROR("[OpUnfoldCacheEntry][GetTransferCountForAlltoallv] localRank[%u] >= rankSize[%u]", localRank, rankSize), HCCL_E_INTERNAL);

            // 获得dst rank
            uint32_t dstRank = 0;
            if (dstRefreshAddrInfo.memType == RefreshAddrInfo::USER_OUTPUT_MEMTYPE) { // LocalCopy
                dstRank = dstRefreshAddrInfo.rankId; // dstRank = localRank
                CHK_PRT_RET(dstRank != localRank, HCCL_ERROR("[OpUnfoldCacheEntry][GetTransferCountForAlltoallv] dstRank[%u] != localRank[%u]", dstRank, localRank), HCCL_E_INTERNAL);
            } else if (dstRefreshAddrInfo.memType == RefreshAddrInfo::HCCL_INPUT_MEMTYPE) { // PrepareIntraData
                // dstRank在第一次cache miss后处理时, 被UpdateRefreshAddrInfoForAlltoallv更新, 一定不等于localRank
                dstRank = dstRefreshAddrInfo.rankId;
                CHK_PRT_RET(dstRank == localRank, HCCL_ERROR("[OpUnfoldCacheEntry][GetTransferCountForAlltoallv] dstRank[%u] = localRank[%u]", dstRank, localRank), HCCL_E_INTERNAL);
            } else {
                HCCL_ERROR("[OpUnfoldCacheEntry][GetTransferCountForAlltoallv] invalid dstMemType[%u]", dstRefreshAddrInfo.memType);
                return HCCL_E_INTERNAL;
            }

            // 获得dst rank对应的send count/size
            CHK_PRT_RET(dstRank >= rankSize, HCCL_ERROR("[OpUnfoldCacheEntry][GetTransferCountForAlltoallv] dstRank[%u] >= rankSize[%u]", dstRank, rankSize), HCCL_E_INTERNAL);
            count = alltoallvSendRecvInfo.sendCounts[dstRank];
            size = count * SIZE_TABLE[alltoallvSendRecvInfo.sendType];

            HCCL_INFO("[OpUnfoldCacheEntry][GetTransferCountForAlltoallv] sendCount[%llu] sendSize[%llu] dstRank[%u]", count, size, dstRank);
        } else if (dstRefreshAddrInfo.memType == RefreshAddrInfo::USER_OUTPUT_MEMTYPE) { // srcAddr不是user input, 但dstAddr是user output (remote copy: remote hccl input -> local user output)
            CHK_PRT_RET(srcRefreshAddrInfo.memType != RefreshAddrInfo::HCCL_INPUT_MEMTYPE,
                HCCL_ERROR("[OpUnfoldCacheEntry][GetTransferCountForAlltoallv] invalid srcMemType[%u] for remote copy", srcRefreshAddrInfo.memType),
                HCCL_E_INTERNAL);

            // 获得src rank对应的recv count/size
            uint32_t srcRank = srcRefreshAddrInfo.rankId;
            CHK_PRT_RET(srcRank >= rankSize, HCCL_ERROR("[OpUnfoldCacheEntry][GetTransferCountForAlltoallv] srcRank[%u] >= rankSize[%u]", srcRank, rankSize), HCCL_E_INTERNAL);
            count = alltoallvSendRecvInfo.recvCounts[srcRank];
            size = count * SIZE_TABLE[alltoallvSendRecvInfo.recvType];

            HCCL_INFO("[OpUnfoldCacheEntry][GetTransferCountForAlltoallv] recvCount[%llu] recvSize[%llu] srcRank[%u]", count, size, srcRank);
        } else {
            HCCL_ERROR("[OpUnfoldCacheEntry][GetTransferCountForAlltoallv] invalid srcMemType[%u] and dstMemType[%u]", srcRefreshAddrInfo.memType, dstRefreshAddrInfo.memType);
            return HCCL_E_INTERNAL;
        }

        // 一定不是大数据量的alltoallv, 否则会在aicpu communicator侧被拦截, 不会进入cache
        CHK_PRT_RET(size > alltoallvMetadata.sdmaDataBlockSize || size > HCCL_SDMA_MAX_COUNT_4GB,
            HCCL_ERROR("[OpUnfoldCacheEntry][GetTransferCountForAlltoallv] invalid size[%u] sdmaDataBlockSize[%u] 4GB[%u]",
                size, alltoallvMetadata.sdmaDataBlockSize, HCCL_SDMA_MAX_COUNT_4GB),
            HCCL_E_INTERNAL);

        return HCCL_SUCCESS;
    }

    HcclResult OpUnfoldCacheEntry::UpdateMemcpySqeForAlltoallv(uint8_t *sqePtr, uint8_t *sqeTypePtr, const uint16_t curTaskId, const RefreshAddrInfo& srcRefreshAddrInfo, const RefreshAddrInfo& dstRefreshAddrInfo, const std::vector<OpUnfoldMemRange>& curUserInputMemRanges, const std::vector<OpUnfoldMemRange>& curUserOutputMemRanges, const AlltoallvMetadata& alltoallvMetadata, const AlltoallvSendRecvInfo& alltoallvSendRecvInfo, const uint64_t count, const uint64_t size) {
        // 获取src/dst memType
        const uint8_t srcMemType = srcRefreshAddrInfo.memType;
        const uint8_t dstMemType = dstRefreshAddrInfo.memType;

        // 更新SQE (count > 0)或者生成SQE (count = 0)
        const uint32_t rankSize = alltoallvSendRecvInfo.sendOffsets.size();
        rtStarsMemcpyAsyncSqe_t *memcpyAsyncSqePtr = reinterpret_cast<rtStarsMemcpyAsyncSqe_t *>(sqePtr);
        if (count > 0) { // Case 1: memcpy SQE -> memcpy SQE
            HCCL_DEBUG("[OpUnfoldCacheEntry][UpdateMemcpySqeForAlltoallv] case 1: memcpy -> memcpy; curTaskId[%u]", curTaskId);

            // 校验length
            CHK_PRT_RET(size == 0, HCCL_ERROR("[OpUnfoldCacheEntry][UpdateMemcpySqeForAlltoallv] size[%u] for positive count", size), HCCL_E_INTERNAL);

            // 更新task id
            memcpyAsyncSqePtr->header.taskId = curTaskId;

            // 更新length
            memcpyAsyncSqePtr->length = static_cast<uint32_t>(size);

            // 更新src/dst addr
            if (srcMemType == RefreshAddrInfo::USER_INPUT_MEMTYPE) { // LocalCopy/PrepareIntraData
                // 获取send offset
                const uint32_t dstRank = dstRefreshAddrInfo.rankId; // LocalCopy下是localRank, PrepareIntraData下是remoteRank
                CHK_PRT_RET(dstRank >= rankSize, HCCL_ERROR("[OpUnfoldCacheEntry][UpdateMemcpySqeForAlltoallv] dstRank[%u] >= rankSize[%u]", dstRank, rankSize), HCCL_E_INTERNAL);
                const uint64_t sendOffset = alltoallvSendRecvInfo.sendOffsets[dstRank];

                // 更新src addr (local user input)
                uint64_t sqeSrcAddr = 0;
                CombineUint32ToUint64(sqeSrcAddr, memcpyAsyncSqePtr->src_addr_high, memcpyAsyncSqePtr->src_addr_low);
                CHK_RET(RefreshSqeAddr(sqeSrcAddr, srcRefreshAddrInfo.rankId, userInputMemRanges_, curUserInputMemRanges, true, sendOffset));
                SplitUint64ToUint32(sqeSrcAddr, memcpyAsyncSqePtr->src_addr_high, memcpyAsyncSqePtr->src_addr_low);

                // 只有LocalCopy才需要更新dst addr (hccl addr不用刷新)
                if (dstMemType == RefreshAddrInfo::USER_OUTPUT_MEMTYPE) { // LocalCopy (local user input -> local user output)
                    // 获取recv offset
                    const uint64_t recvOffset = alltoallvSendRecvInfo.recvOffsets[dstRank];

                    // 更新dst addr (local user output)
                    uint64_t sqeDstAddr = 0;
                    CombineUint32ToUint64(sqeDstAddr, memcpyAsyncSqePtr->dst_addr_high, memcpyAsyncSqePtr->dst_addr_low);
                    CHK_RET(RefreshSqeAddr(sqeDstAddr, dstRank, userOutputMemRanges_, curUserOutputMemRanges, true, recvOffset));
                    SplitUint64ToUint32(sqeDstAddr, memcpyAsyncSqePtr->dst_addr_high, memcpyAsyncSqePtr->dst_addr_low);
                }
            } else { // RemoteCopy
                // 获取recv offset
                uint32_t srcRank = srcRefreshAddrInfo.rankId;
                CHK_PRT_RET(srcRank >= rankSize,
                    HCCL_ERROR("[OpUnfoldCacheEntry][UpdateMemcpySqeForAlltoallv] srcRank[%u] >= rankSize[%u]",
                        srcRank, rankSize),
                    HCCL_E_INTERNAL);
                const uint64_t recvOffset = alltoallvSendRecvInfo.recvOffsets[srcRank];

                // 更新dst addr (local user output)
                uint64_t sqeDstAddr = 0;
                CombineUint32ToUint64(sqeDstAddr, memcpyAsyncSqePtr->dst_addr_high, memcpyAsyncSqePtr->dst_addr_low);
                CHK_RET(RefreshSqeAddr(sqeDstAddr, dstRefreshAddrInfo.rankId, userOutputMemRanges_, curUserOutputMemRanges, true, recvOffset));
                SplitUint64ToUint32(sqeDstAddr, memcpyAsyncSqePtr->dst_addr_high, memcpyAsyncSqePtr->dst_addr_low);
            }
        } else { // Case 2: memcpy SQE -> placeholder SQE
            HCCL_DEBUG("[OpUnfoldCacheEntry][UpdateMemcpySqeForAlltoallv] case 2: memcpy -> placeholder; curTaskId[%u]", curTaskId);

            // 校验length
            CHK_PRT_RET(size != 0,
                HCCL_ERROR("[OpUnfoldCacheEntry][UpdateMemcpySqeForAlltoallv] size[%u] for zero count", size),
                HCCL_E_INTERNAL);

            // 保留original SQE中的相关信息
            const uint16_t streamId = memcpyAsyncSqePtr->header.rtStreamId;
            const uint8_t kernelCredit = memcpyAsyncSqePtr->kernel_credit;
            const uint8_t linkType = memcpyAsyncSqePtr->linkType;
            const uint32_t qos = memcpyAsyncSqePtr->qos;
            uint32_t dstAddrHigh = 0;
            uint32_t dstAddrLow = 0;
            bool saveDstAddr = false;
            if (srcMemType == RefreshAddrInfo::USER_INPUT_MEMTYPE && dstMemType == RefreshAddrInfo::HCCL_INPUT_MEMTYPE) { // PrepareIntraData
                // 保留dst addr (local hccl input)
                // 注意: 非PrepraeIntraData case下, dst addr为local user output, 会动态计算, 无需保留在placeholder中
                dstAddrHigh = memcpyAsyncSqePtr->dst_addr_high;
                dstAddrLow = memcpyAsyncSqePtr->dst_addr_low;
                saveDstAddr = true;
            }
            uint32_t srcAddrHigh = 0;
            uint32_t srcAddrLow = 0;
            bool saveSrcAddr = false;
            if (srcMemType == RefreshAddrInfo::HCCL_INPUT_MEMTYPE && dstMemType == RefreshAddrInfo::USER_OUTPUT_MEMTYPE) { // RemoteCopy
                // 保留src addr (remote hccl intput)
                // 注意: 非RemoteCopy case下, src addr为local user input, 会动态计算, 无需保留在placeholder中
                srcAddrHigh = memcpyAsyncSqePtr->src_addr_high;
                srcAddrLow = memcpyAsyncSqePtr->src_addr_low;
                saveSrcAddr = true;
            }

            // 清空original SQE
            CHK_SAFETY_FUNC_RET(memset_s(static_cast<void *>(sqePtr), HCCL_SQE_SIZE, 0, HCCL_SQE_SIZE));

            // 生成placeholder SQE
            // 参考aicpu_hccl_sqcqv1.cc中的AddOneCacheMemcpyPlaceholderSqeV1
            *sqeTypePtr = SqeType::CACHE_MEMCPY_PLACEHOLDER_SQE;
            SetCachePlaceholderHeaderForAlltoallv(streamId, curTaskId, sqePtr);

            // 注意: 正常情况下无需设置placeholder SQE中的src/dst addr
            // (只用于第一次算子执行发生cache miss时, 根据memory range获得对应的RefreshAddrInfo, 后续不再使用)
            rtStarsPlaceHolderSqe_t * const placeholderSqePtr = (rtStarsPlaceHolderSqe_t * const)sqePtr;
            if (saveDstAddr) { // PrepareIntraData case下需要保留dst addr (local hccl input)
                placeholderSqePtr->u.cache_memcpy_task_info.dst_addr_high = dstAddrHigh;
                placeholderSqePtr->u.cache_memcpy_task_info.dst_addr_low = dstAddrLow;
            }
            if (saveSrcAddr) { // RemoteCopy case下需要保留src addr (remote hccl input)
                placeholderSqePtr->u.cache_memcpy_task_info.src_addr_high = srcAddrHigh;
                placeholderSqePtr->u.cache_memcpy_task_info.src_addr_low = srcAddrLow;
            }
            placeholderSqePtr->u.cache_memcpy_task_info.kernel_credit = kernelCredit;
            placeholderSqePtr->u.cache_memcpy_task_info.linkType = linkType;
            placeholderSqePtr->u.cache_memcpy_task_info.qos = qos;
        }

        return HCCL_SUCCESS;
    }

    HcclResult OpUnfoldCacheEntry::UpdateMemcpyPlaceholderSqeForAlltoallv(uint8_t *sqePtr, uint8_t *sqeTypePtr, const uint16_t curTaskId, const RefreshAddrInfo& srcRefreshAddrInfo, const RefreshAddrInfo& dstRefreshAddrInfo, const std::vector<OpUnfoldMemRange>& curUserInputMemRanges, const std::vector<OpUnfoldMemRange>& curUserOutputMemRanges, const AlltoallvMetadata& alltoallvMetadata, const AlltoallvSendRecvInfo& alltoallvSendRecvInfo, const uint64_t count, const uint64_t size) {
        // 更新SQE (count = 0)或者生成SQE (count > 0)
        const uint32_t rankSize = alltoallvSendRecvInfo.sendOffsets.size();
        rtStarsPlaceHolderSqe_t *placeholderSqePtr = reinterpret_cast<rtStarsPlaceHolderSqe_t *>(sqePtr);
        if (count == 0) { // Case 3: placeholder SQE -> placeholder SQE
            HCCL_DEBUG("[OpUnfoldCacheEntry][UpdateMemcpyPlaceholderSqeForAlltoallv] case 3: placeholder -> placeholder; curTaskId[%u]", curTaskId);

            // 校验length
            CHK_PRT_RET(size != 0, HCCL_ERROR("[OpUnfoldCacheEntry][UpdateMemcpyPlaceholderSqeForAlltoallv] size[%u] for zero count", size), HCCL_E_INTERNAL);

            // 更新task id
            placeholderSqePtr->header.taskId = curTaskId;

            // 注意: 无需更新placeholder SQE中的src/dst addr (只用于第一次算子执行发生cache miss时, 根据memory range获得对应的RefreshAddrInfo, 后续不再使用)
        } else { // Case 4: placeholder SQE -> memcpy SQE
            HCCL_DEBUG("[OpUnfoldCacheEntry][UpdateMemcpyPlaceholderSqeForAlltoallv] case 4: placeholder -> memcpy; curTaskId[%u]", curTaskId);

            // 校验length
            CHK_PRT_RET(size == 0, HCCL_ERROR("[OpUnfoldCacheEntry][UpdateMemcpyPlaceholderSqeForAlltoallv] size[%u] for positive count", size), HCCL_E_INTERNAL);

            // 保留original SQE中的相关信息
            const uint16_t streamId = placeholderSqePtr->header.rtStreamId;
            const uint8_t kernelCredit = placeholderSqePtr->u.cache_memcpy_task_info.kernel_credit;
            const uint8_t linkType = placeholderSqePtr->u.cache_memcpy_task_info.linkType;
            const uint32_t qos = placeholderSqePtr->u.cache_memcpy_task_info.qos;

            // 准备src/dst addr
            uint64_t sqeSrcAddr = 0;
            uint64_t sqeDstAddr = 0;
            if (srcRefreshAddrInfo.memType == RefreshAddrInfo::USER_INPUT_MEMTYPE) { // LocalCopy/PrepareIntraData
                // 获取send offset
                const uint32_t dstRank = dstRefreshAddrInfo.rankId; // LocalCopy下是localRank, PrepareIntraData下是remoteRank
                CHK_PRT_RET(dstRank >= rankSize, HCCL_ERROR("[OpUnfoldCacheEntry][UpdateMemcpyPlaceholderSqeForAlltoallv] dstRank[%u] >= rankSize[%u]", dstRank, rankSize), HCCL_E_INTERNAL);
                const uint64_t sendOffset = alltoallvSendRecvInfo.sendOffsets[dstRank];

                // 获得src addr (local user input)
                const uint32_t localRank = srcRefreshAddrInfo.rankId;
                CHK_PRT_RET(localRank >= rankSize, HCCL_ERROR("[OpUnfoldCacheEntry][UpdateMemcpyPlaceholderSqeForAlltoallv] localRank[%u] >= rankSize[%u]", localRank, rankSize), HCCL_E_INTERNAL);
                const uint64_t localUserInputBaseAddr = curUserInputMemRanges[localRank].baseAddr;
                sqeSrcAddr = localUserInputBaseAddr + sendOffset;

                if (dstRefreshAddrInfo.memType == RefreshAddrInfo::USER_OUTPUT_MEMTYPE) { // LocalCopy
                    // 获取recv offset
                    const uint64_t recvOffset = alltoallvSendRecvInfo.recvOffsets[dstRank]; // dstRank = localRank

                    // 获得dst addr (local user output)
                    const uint64_t localUserOutputBaseAddr = curUserOutputMemRanges[dstRank].baseAddr;
                    sqeDstAddr = localUserOutputBaseAddr + recvOffset;
                } else if (dstRefreshAddrInfo.memType == RefreshAddrInfo::HCCL_INPUT_MEMTYPE) { // PrepareIntraData
                    // 从placeholder中获取dst addr (local hccl input)
                    const uint32_t dstAddrHigh = placeholderSqePtr->u.cache_memcpy_task_info.dst_addr_high;
                    const uint32_t dstAddrLow = placeholderSqePtr->u.cache_memcpy_task_info.dst_addr_low;
                    CombineUint32ToUint64(sqeDstAddr, dstAddrHigh, dstAddrLow);
                } else {
                    HCCL_ERROR("[OpUnfoldCacheEntry][UpdateMemcpyPlaceholderSqeForAlltoallv] invalid srcMemType[%u] dstMemType[%u]",\
                        srcRefreshAddrInfo.memType, dstRefreshAddrInfo.memType);
                    return HCCL_E_INTERNAL;
                }
            } else { // RemoteCopy
                // 从placeholder中获取src addr (remote hccl input)
                const uint32_t srcAddrHigh = placeholderSqePtr->u.cache_memcpy_task_info.src_addr_high;
                const uint32_t srcAddrLow = placeholderSqePtr->u.cache_memcpy_task_info.src_addr_low;
                CombineUint32ToUint64(sqeSrcAddr, srcAddrHigh, srcAddrLow);

                // 获得remote src rank
                const uint32_t srcRank = srcRefreshAddrInfo.rankId;
                CHK_PRT_RET(srcRank >= rankSize,
                    HCCL_ERROR("[OpUnfoldCacheEntry][UpdateMemcpyPlaceholderSqeForAlltoallv] srcRank[%u] >= rankSize[%u]",
                        srcRank, rankSize),
                    HCCL_E_INTERNAL);

                // 获得recv offset
                const uint64_t recvOffset = alltoallvSendRecvInfo.recvOffsets[srcRank]; // srcRank = remoteRank

                // 获得dst addr
                const uint32_t localRank = dstRefreshAddrInfo.rankId;
                CHK_PRT_RET(localRank >= rankSize,
                    HCCL_ERROR("[OpUnfoldCacheEntry][UpdateMemcpyPlaceholderSqeForAlltoallv] localRank[%u] >= rankSize[%u]",
                        localRank, rankSize),
                    HCCL_E_INTERNAL);
                const uint64_t localUserOutputBaseAddr = curUserOutputMemRanges[localRank].baseAddr;
                sqeDstAddr = localUserOutputBaseAddr + recvOffset;
            }

            // 清空original SQE
            CHK_SAFETY_FUNC_RET(memset_s(static_cast<void *>(sqePtr), HCCL_SQE_SIZE, 0, HCCL_SQE_SIZE));

            // 生成memcpy SQE
            // 参考aicpu_hccl_sqcqv1.cc中的AddOneMemcpySqeV1
            *sqeTypePtr = SqeType::MEMCPY_ASYNC_SQE;
            rtStarsMemcpyAsyncSqe_t *memcpySqePtr = reinterpret_cast<rtStarsMemcpyAsyncSqe_t *>(sqePtr);
            memcpySqePtr->header.type = RT_STARS_SQE_TYPE_SDMA;
            memcpySqePtr->header.rtStreamId = streamId;
            memcpySqePtr->header.taskId = curTaskId;
            memcpySqePtr->kernel_credit = kernelCredit;
            memcpySqePtr->opcode = 0U;
            memcpySqePtr->length = static_cast<uint32_t>(size);
            SplitUint64ToUint32(sqeSrcAddr, memcpySqePtr->src_addr_high, memcpySqePtr->src_addr_low);
            SplitUint64ToUint32(sqeDstAddr, memcpySqePtr->dst_addr_high, memcpySqePtr->dst_addr_low);
            memcpySqePtr->sssv = 1U;
            memcpySqePtr->dssv = 1U;
            memcpySqePtr->sns = 1U;
            memcpySqePtr->dns = 1U;
            memcpySqePtr->qos = 6; // 6 is HCCL QoS
            const uint32_t partId = 0; // 参考dispatcher_aicpu.cc中addOneMemcpySqe_的partId传参始终为0
            memcpySqePtr->partid = partId;
            memcpySqePtr->linkType = linkType;
            memcpySqePtr->qos = qos;
        }

        return HCCL_SUCCESS;
    }

    HcclResult OpUnfoldCacheEntry::RefreshSqeAddr(uint64_t &sqeAddr, const uint32_t rankId, const std::vector<OpUnfoldMemRange>& cachedMemRanges, const std::vector<OpUnfoldMemRange>& curMemRanges, const bool isAlltoallv, const uint64_t offset) const {
        CHK_PRT_RET(rankId == INVALID_VALUE_RANKID, HCCL_ERROR("[OpUnfoldCacheEntry][RefreshSqeAddr] invalid rankId"), HCCL_E_INTERNAL);
        CHK_PRT_RET(rankId >= cachedMemRanges.size(), HCCL_ERROR("[OpUnfoldCacheEntry][RefreshSqeAddr] rankId %u exceeds rankSize %u", rankId, cachedMemRanges.size()), HCCL_E_INTERNAL);

        // 获取缓存的和当前的memory ranges
        const OpUnfoldMemRange& cachedMemRange = cachedMemRanges[rankId];
        const OpUnfoldMemRange& curMemRange = curMemRanges[rankId];
        HCCL_DEBUG("[OpUnfoldCacheEntry][RefreshSqeAddr] cachedMemRange: isValid[%u] baseAddr[0x%016llx] memSize[%llu]; curMemRange: isValid[%u] baseAddr[0x%016llx] memSize[%llu] isAlltoallv[%u]",
            cachedMemRange.isValid, cachedMemRange.baseAddr, cachedMemRange.memSize, curMemRange.isValid, curMemRange.baseAddr, curMemRange.memSize, isAlltoallv);

        // 刷新前地址校验
        // (i) user memory: 非V类/alltoallv, SQE addr字段一定在user memory range内, 才需要调用本函数刷新addr
        // (ii) hccl memory: alltoallv, SQE addr一定在hccl memory range内, 才需要调用本函数生成addr
        bool isInRange = false;
        CHK_RET(cachedMemRange.InRange(sqeAddr, isInRange));
        CHK_PRT_RET(!isInRange,
            HCCL_ERROR("[OpUnfoldCacheEntry][RefreshSqeAddr] sqeAddr[0x%016llx] not in the range of cachedMemRange[0x%016llx, 0x%016llx)",
                sqeAddr, cachedMemRange.baseAddr, cachedMemRange.baseAddr + cachedMemRange.memSize),
            HCCL_E_INTERNAL);

        // 刷新SQE addr
        uint64_t curOffset = 0;
        if (isAlltoallv) { // alltoallv
            curOffset = offset; // 使用给定的offset
        } else { // 非V类算子
            curOffset = sqeAddr - cachedMemRange.baseAddr; // 计算缓存的sqe addr相对于缓存的base addr的offset
        }
        sqeAddr = curMemRange.baseAddr + curOffset; // 用当前的base addr更新sqe addr

        // 刷新后地址校验: 一定在当前的memory range内
        isInRange = false;
        CHK_RET(curMemRange.InRange(sqeAddr, isInRange));
        CHK_PRT_RET(!isInRange,
            HCCL_ERROR("[OpUnfoldCacheEntry][RefreshSqeAddr] sqeAddr[0x%016llx] not in the range of curMemRange[0x%016llx, 0x%016llx)",
                sqeAddr, curMemRange.baseAddr, curMemRange.baseAddr + curMemRange.memSize),
            HCCL_E_INTERNAL);

        return HCCL_SUCCESS;
    }

    HcclResult OpUnfoldCacheEntry::UpdateSyncSqeForAlltoallv(uint8_t *sqePtr, uint8_t *sqeTypePtr, const uint16_t curTaskId,
        const RefreshAddrInfo& srcRefreshAddrInfo, const RefreshAddrInfo& dstRefreshAddrInfo,
        const AlltoallvMetadata& alltoallvMetadata, const AlltoallvSendRecvInfo& alltoallvSendRecvInfo) {
        HCCL_INFO("[OpUnfoldCacheEntry][UpdateSyncSqeForAlltoallv] curTaskId[%u]", curTaskId);
        
        // 校验alltoallv相关参数
        CHK_RET(alltoallvMetadata.Check(true));
        CHK_RET(alltoallvSendRecvInfo.Check());
        const uint32_t rankSize = alltoallvSendRecvInfo.sendOffsets.size();
        CHK_PRT_RET(rankSize != alltoallvMetadata.hcclInputMemRanges.size(),
            HCCL_ERROR("[OpUnfoldCacheEntry][UpdateSyncSqeForAlltoallv] hcclInputMemRanges.size[%u] != rankSize[%u]",
                alltoallvMetadata.hcclInputMemRanges.size(), rankSize),
            HCCL_E_INTERNAL);

        // 对于NotifyWait, 如果不在notifyid-rank map中, 则无需根据count进行处理, 只需要更新taskid
        // 对于WriteValue/MemcpyRecord, 如果不在signaladdr-rank map中, 则无需根据count进行处理, 只需要更新taskid
        if ((*sqeTypePtr) == SqeType::NOTIFY_SQE) {
            rtStarsNotifySqeV1_t *notifySqePtr = reinterpret_cast<rtStarsNotifySqeV1_t *>(sqePtr);
            const uint32_t notifyId = notifySqePtr->notify_id;
            if (alltoallvMetadata.notifyIdRankRflagMap.find(notifyId) == alltoallvMetadata.notifyIdRankRflagMap.cend()) {
                notifySqePtr->header.taskId = curTaskId;
                return HCCL_SUCCESS;
            }
        } else if ((*sqeTypePtr) == SqeType::WRITE_VALUE_SQE) {
            rtStarsWriteValueSqe_t *writeValueSqePtr = reinterpret_cast<rtStarsWriteValueSqe_t *>(sqePtr);
            uint64_t signalAddr = 0;
            const uint32_t lowAddr = writeValueSqePtr->write_addr_low;
            const uint32_t highAddr = writeValueSqePtr->write_addr_high;
            CombineUint32ToUint64(signalAddr, highAddr, lowAddr);
            if (alltoallvMetadata.signalAddrRankRflagMap.find(signalAddr) == alltoallvMetadata.signalAddrRankRflagMap.cend()) {
                writeValueSqePtr->header.taskId = curTaskId;
                return HCCL_SUCCESS;
            }
        } else if ((*sqeTypePtr) == SqeType::MEMCPY_ASYNC_SQE) {
            // Memcpy-record SQE的src/dst memtype一定是invalid
            rtStarsMemcpyAsyncSqe_t *memcpySqePtr = reinterpret_cast<rtStarsMemcpyAsyncSqe_t *>(sqePtr);
            CHK_PRT_RET(srcRefreshAddrInfo.memType != RefreshAddrInfo::INVALID_MEMTYPE ||
                dstRefreshAddrInfo.memType != RefreshAddrInfo::INVALID_MEMTYPE,
                HCCL_ERROR("[OpUnfoldCacheEntry][UpdateTransferSqeForAlltoallv] memcpy-record SQE: "\
                    "streamId[%u] taskId[%u] curTaskId[%u] srcMemType[%u] dstMemType[%u]",
                    memcpySqePtr->header.rtStreamId, memcpySqePtr->header.taskId, curTaskId,
                    srcRefreshAddrInfo.memType, dstRefreshAddrInfo.memType),
                HCCL_E_INTERNAL);

            uint64_t dstSignalAddr = 0;
            const uint32_t lowAddr = memcpySqePtr->dst_addr_low;
            const uint32_t highAddr = memcpySqePtr->dst_addr_high;
            CombineUint32ToUint64(dstSignalAddr, highAddr, lowAddr);
            if (alltoallvMetadata.signalAddrRankRflagMap.find(dstSignalAddr) == alltoallvMetadata.signalAddrRankRflagMap.cend()) {
                memcpySqePtr->header.taskId = curTaskId;
                return HCCL_SUCCESS;
            }
        }

        // 获取当前sync类SQE对应的count和size
        uint64_t count = 0;
        uint64_t size = 0; // send/recv bytes
        CHK_RET(GetTransferCountForAlltoallv(count, size, sqePtr, sqeTypePtr, alltoallvMetadata, alltoallvSendRecvInfo));
        
        // 更新/生成相应SQE
        if ((*sqeTypePtr) == SqeType::CACHE_NOTIFY_PLACEHOLDER_SQE) {
            CHK_RET(UpdateNotifyPlaceholderSqeForAlltoallv(sqePtr, sqeTypePtr, curTaskId, alltoallvMetadata, alltoallvSendRecvInfo, count, size));
        } else if ((*sqeTypePtr) == SqeType::CACHE_WRITE_VALUE_PLACEHOLDER_SQE) {
            CHK_RET(UpdateWritePlaceholderSqeForAlltoallv(sqePtr, sqeTypePtr, curTaskId, alltoallvMetadata, alltoallvSendRecvInfo, count, size));
        } else if ((*sqeTypePtr) == SqeType::NOTIFY_SQE) {
            CHK_RET(UpdateNotifySqeForAlltoallv(sqePtr, sqeTypePtr, curTaskId, alltoallvMetadata, alltoallvSendRecvInfo, count, size));
        } else if ((*sqeTypePtr) == SqeType::WRITE_VALUE_SQE) {
            CHK_RET(UpdateWriteValueSqeForAlltoallv(sqePtr, sqeTypePtr, curTaskId, alltoallvMetadata, alltoallvSendRecvInfo, count, size));
        } else if ((*sqeTypePtr) == SqeType::MEMCPY_ASYNC_SQE) { // MemcpyRecord SQE
            CHK_RET(UpdateMemcpyRecordSqeForAlltoallv(sqePtr, sqeTypePtr, curTaskId, alltoallvMetadata, alltoallvSendRecvInfo, count, size));
        } else if ((*sqeTypePtr) == SqeType::CACHE_MEMCPY_RECORD_PLACEHOLDER_SQE) {
            CHK_RET(UpdateMemcpyRecordPlaceholderSqeForAlltoallv(sqePtr, sqeTypePtr, curTaskId, alltoallvMetadata, alltoallvSendRecvInfo, count, size));
        } else {
            HCCL_ERROR("[OpUnfoldCacheEntry][UpdateTransferSqeForAlltoallv] invalid sqeType[%u]", *sqeTypePtr);
            return HCCL_E_INTERNAL;
        }

        return HCCL_SUCCESS;
    }

    HcclResult OpUnfoldCacheEntry::GetTransferCountForAlltoallv(uint64_t& count, uint64_t& size, const uint8_t *sqePtr, const uint8_t *sqeTypePtr, const AlltoallvMetadata& alltoallvMetadata, const AlltoallvSendRecvInfo& alltoallvSendRecvInfo) const {
        // 获得notifyId或者signalAddr
        bool isNotify = false;
        uint32_t notifyId = 0;
        uint64_t signalAddr = 0;
        if ((*sqeTypePtr) == SqeType::NOTIFY_SQE) {
            // 注意: 目前只有NotifyWait可能会生成cache-notify placeholder
            const rtStarsNotifySqeV1_t *notifySqePtr = reinterpret_cast<const rtStarsNotifySqeV1_t *>(sqePtr);
            CHK_PRT_RET(notifySqePtr->header.type != RT_STARS_SQE_TYPE_NOTIFY_WAIT,
                HCCL_ERROR("[OpUnfoldCacheEntry][GetTransferCountForAlltoallv] header.type[%u]", notifySqePtr->header.type),
                HCCL_E_INTERNAL);

            notifyId = notifySqePtr->notify_id;
            isNotify = true;
        } else if ((*sqeTypePtr) == SqeType::CACHE_NOTIFY_PLACEHOLDER_SQE) {
            // 注意: 目前只会存在对应NotifyWait的placeholder
            const rtStarsPlaceHolderSqe_t *placeholderSqePtr = reinterpret_cast<const rtStarsPlaceHolderSqe_t *>(sqePtr);
            CHK_PRT_RET(placeholderSqePtr->u.cache_notify_task_info.is_wait != 1,
                HCCL_ERROR("[OpUnfoldCacheEntry][GetTransferCountForAlltoallv] is_wait[%u]",
                    placeholderSqePtr->u.cache_notify_task_info.is_wait),
                HCCL_E_INTERNAL);

            notifyId = placeholderSqePtr->u.cache_notify_task_info.notify_id;
            isNotify = true;
        } else if ((*sqeTypePtr) == SqeType::WRITE_VALUE_SQE) {
            const rtStarsWriteValueSqe_t *writeValueSqePtr = reinterpret_cast<const rtStarsWriteValueSqe_t *>(sqePtr);
            const uint32_t lowAddr = writeValueSqePtr->write_addr_low;
            const uint32_t highAddr = writeValueSqePtr->write_addr_high;
            CombineUint32ToUint64(signalAddr, highAddr, lowAddr);
            isNotify = false;
        } else if ((*sqeTypePtr) == SqeType::CACHE_WRITE_VALUE_PLACEHOLDER_SQE) {
            const rtStarsPlaceHolderSqe_t *placeholderSqePtr = reinterpret_cast<const rtStarsPlaceHolderSqe_t *>(sqePtr);
            const uint32_t lowAddr = placeholderSqePtr->u.cache_write_value_task_info.write_addr_low;
            const uint32_t highAddr = placeholderSqePtr->u.cache_write_value_task_info.write_addr_high;
            CombineUint32ToUint64(signalAddr, highAddr, lowAddr);
            isNotify = false;
        } else if ((*sqeTypePtr) == SqeType::MEMCPY_ASYNC_SQE) {  // MemcpyRecord SQE
            const rtStarsMemcpyAsyncSqe_t *memcpySqePtr = reinterpret_cast<const rtStarsMemcpyAsyncSqe_t *>(sqePtr);
            const uint32_t lowAddr = memcpySqePtr->dst_addr_low;
            const uint32_t highAddr = memcpySqePtr->dst_addr_high;
            CombineUint32ToUint64(signalAddr, highAddr, lowAddr);
            isNotify = false;
        } else if ((*sqeTypePtr) == SqeType::CACHE_MEMCPY_RECORD_PLACEHOLDER_SQE) {
            const rtStarsPlaceHolderSqe_t *placeholderSqePtr = reinterpret_cast<const rtStarsPlaceHolderSqe_t *>(sqePtr);
            const uint32_t lowAddr = placeholderSqePtr->u.cache_memcpy_record_task_info.dst_addr_low;
            const uint32_t highAddr = placeholderSqePtr->u.cache_memcpy_record_task_info.dst_addr_high;
            CombineUint32ToUint64(signalAddr, highAddr, lowAddr);
            isNotify = false;
        }
        else {
            HCCL_ERROR("[OpUnfoldCacheEntry][GetTransferCountForAlltoallv] invalid sqeType[%u]", *sqeTypePtr);
            return HCCL_E_INTERNAL;
        }
        HCCL_INFO("[OpUnfoldCacheEntry][GetTransferCountForAlltoallv] notifyId[%u] signalAddr[0x%016llx]", notifyId, signalAddr);

        // 获得remoteRank
        uint32_t remoteRank = 0;
        bool recvFlag = false;
        if (isNotify) {
            // 校验notifyId一定在notifyid-rank map中
            std::unordered_map<uint32_t, RankRflag>::const_iterator mapIter = alltoallvMetadata.notifyIdRankRflagMap.find(notifyId);
            CHK_PRT_RET(mapIter == alltoallvMetadata.notifyIdRankRflagMap.cend(),
                HCCL_ERROR("[OpUnfoldCacheEntry][GetTransferCountForAlltoallv] notifyId[%u] not in alltoallvMetadata", notifyId),
                HCCL_E_INTERNAL);

            // 根据notifyId获得remoteRank
            remoteRank = mapIter->second.first;
            recvFlag = mapIter->second.second;
        } else {
            // 校验signalAddr一定在signalAddr-rank map中
            std::unordered_map<uint64_t, RankRflag>::const_iterator mapIter = alltoallvMetadata.signalAddrRankRflagMap.find(signalAddr);
            CHK_PRT_RET(mapIter == alltoallvMetadata.signalAddrRankRflagMap.cend(),
                HCCL_ERROR("[OpUnfoldCacheEntry][GetTransferCountForAlltoallv] signalAddr[%llu] not in alltoallvMetadata", signalAddr),
                HCCL_E_INTERNAL);

            // 根据signalAddr获得remoteRank
            remoteRank = mapIter->second.first;
            recvFlag = mapIter->second.second;
        }
        HCCL_INFO("[OpUnfoldCacheEntry][GetTransferCountForAlltoallv] remoteRank[%u] recvFlag[%u]", remoteRank, recvFlag);

        // 校验remoteRank (理论上一定不是localRank)
        const uint32_t rankSize = alltoallvSendRecvInfo.sendOffsets.size();
        CHK_PRT_RET(remoteRank >= rankSize,
            HCCL_ERROR("[OpUnfoldCacheEntry][GetTransferCountForAlltoallv] remoteRank[%u] >= rankSize[%u]",
                remoteRank, rankSize),
            HCCL_E_INTERNAL);

        // 根据remoteRank获得count和size
        if (recvFlag) {
            count = alltoallvSendRecvInfo.recvCounts[remoteRank];
            size = count * SIZE_TABLE[alltoallvSendRecvInfo.recvType];
            HCCL_INFO("[OpUnfoldCacheEntry][GetTransferCountForAlltoallv] recvCount[%llu] recvSize[%llu]", count, size);
        } else {
            count = alltoallvSendRecvInfo.sendCounts[remoteRank];
            size = count * SIZE_TABLE[alltoallvSendRecvInfo.sendType];
            HCCL_INFO("[OpUnfoldCacheEntry][GetTransferCountForAlltoallv] sendCount[%llu] sendSize[%llu]", count, size);
        }

        return HCCL_SUCCESS;
    }

    HcclResult OpUnfoldCacheEntry::UpdateNotifyPlaceholderSqeForAlltoallv(uint8_t *sqePtr, uint8_t *sqeTypePtr, const uint16_t curTaskId, const AlltoallvMetadata& alltoallvMetadata, const AlltoallvSendRecvInfo& alltoallvSendRecvInfo, const uint64_t count, const uint64_t size) {
        UNUSED_PARAM(size);
        
        rtStarsPlaceHolderSqe_t *placeholderSqePtr = reinterpret_cast<rtStarsPlaceHolderSqe_t *>(sqePtr);
        if (count == 0) { // 只需要更新task id
            placeholderSqePtr->header.taskId = curTaskId;
        } else { // 需要将cache-notify placeholder转变成NotifyWait
            // 保留original SQE中的相关信息
            const uint16_t streamId = placeholderSqePtr->header.rtStreamId;
            const uint8_t kernel_credit = placeholderSqePtr->u.cache_notify_task_info.kernel_credit;
            const uint32_t timeout = placeholderSqePtr->u.cache_notify_task_info.timeout;
            const uint32_t notifyId = placeholderSqePtr->u.cache_notify_task_info.notify_id;

            // 清空original SQE
            CHK_SAFETY_FUNC_RET(memset_s(static_cast<void *>(sqePtr), HCCL_SQE_SIZE, 0, HCCL_SQE_SIZE));

            // 生成NotifyWait
            // 参考aicpu_hccl_sqcqv1.cc中的AddOneNotifyWaitSqeV1
            *sqeTypePtr = SqeType::NOTIFY_SQE;
            rtStarsNotifySqeV1_t * const notifySqePtr = (rtStarsNotifySqeV1_t * const)sqePtr;
            notifySqePtr->header.type = RT_STARS_SQE_TYPE_NOTIFY_WAIT;
            notifySqePtr->kernel_credit = kernel_credit;
            notifySqePtr->timeout = timeout;
            notifySqePtr->header.rtStreamId = streamId;
            notifySqePtr->notify_id = notifyId;
            notifySqePtr->header.taskId = curTaskId;
        }

        return HCCL_SUCCESS;
    }

    HcclResult OpUnfoldCacheEntry::UpdateWritePlaceholderSqeForAlltoallv(uint8_t *sqePtr, uint8_t *sqeTypePtr, const uint16_t curTaskId, const AlltoallvMetadata& alltoallvMetadata, const AlltoallvSendRecvInfo& alltoallvSendRecvInfo, const uint64_t count, const uint64_t size) {
        UNUSED_PARAM(size);
        
        rtStarsPlaceHolderSqe_t *placeholderSqePtr = reinterpret_cast<rtStarsPlaceHolderSqe_t *>(sqePtr);
        if (count == 0) { // 只需要更新task id
            placeholderSqePtr->header.taskId = curTaskId;
        } else { // 需要将cache-write placeholder转变成WriteValue
            // 保留original SQE中的相关信息
            const uint16_t streamId = placeholderSqePtr->header.rtStreamId;
            const uint32_t lowAddr = placeholderSqePtr->u.cache_write_value_task_info.write_addr_low;
            const uint32_t highAddr = placeholderSqePtr->u.cache_write_value_task_info.write_addr_high;

            // 清空original SQE
            CHK_SAFETY_FUNC_RET(memset_s(static_cast<void *>(sqePtr), HCCL_SQE_SIZE, 0, HCCL_SQE_SIZE));

            // 生成WriteValue
            // 参考aicpu_hccl_sqcqv1.cc中的AddOneWriteValueRecordSqeV1
            *sqeTypePtr = SqeType::WRITE_VALUE_SQE;
            rtStarsWriteValueSqe_t * const writeValueSqePtr = (rtStarsWriteValueSqe_t * const)sqePtr;
            writeValueSqePtr->header.type = RT_STARS_SQE_TYPE_WRITE_VALUE;
            writeValueSqePtr->header.rtStreamId = streamId;
            writeValueSqePtr->header.taskId = curTaskId;
            writeValueSqePtr->kernel_credit = RT_STARS_DEFAULT_KERNEL_CREDIT;
            writeValueSqePtr->awsize = RT_STARS_WRITE_VALUE_SIZE_TYPE_32BIT;
            writeValueSqePtr->write_value_part0 = 1U;
            writeValueSqePtr->sub_type = RT_STARS_WRITE_VALUE_SUB_TYPE_NOTIFY_RECORD_IPC_NO_PCIE;
            writeValueSqePtr->write_addr_low = lowAddr;
            writeValueSqePtr->write_addr_high = highAddr;
        }

        return HCCL_SUCCESS;
    }

    HcclResult OpUnfoldCacheEntry::UpdateNotifySqeForAlltoallv(uint8_t *sqePtr, uint8_t *sqeTypePtr, const uint16_t curTaskId, const AlltoallvMetadata& alltoallvMetadata, const AlltoallvSendRecvInfo& alltoallvSendRecvInfo, const uint64_t count, const uint64_t size) {
        UNUSED_PARAM(size);

        rtStarsNotifySqeV1_t *notifySqePtr = reinterpret_cast<rtStarsNotifySqeV1_t *>(sqePtr);
        if (count > 0) { // 只需要更新task id
            notifySqePtr->header.taskId = curTaskId;
        } else { // 需要将NotifyWait转变成cache-notify placeholder
            // 保留original SQE中的相关信息
            const uint16_t streamId = notifySqePtr->header.rtStreamId;
            const uint8_t kernel_credit = notifySqePtr->kernel_credit;
            const uint32_t timeout = notifySqePtr->timeout;
            const uint32_t notifyId = notifySqePtr->notify_id;

            // 清空original SQE
            CHK_SAFETY_FUNC_RET(memset_s(static_cast<void *>(sqePtr), HCCL_SQE_SIZE, 0, HCCL_SQE_SIZE));

            // 生成cache-notify placeholder
            // 参考aicpu_hccl_sqcqv1.cc中的AddOneCacheNotifyWaitPlaceholderSqeV1
            *sqeTypePtr = SqeType::CACHE_NOTIFY_PLACEHOLDER_SQE;
            SetCachePlaceholderHeaderForAlltoallv(streamId, curTaskId, sqePtr);
            rtStarsPlaceHolderSqe_t * const placeholderSqePtr = (rtStarsPlaceHolderSqe_t * const)sqePtr;
            placeholderSqePtr->u.cache_notify_task_info.is_wait = 1; // NotifyWait
            placeholderSqePtr->u.cache_notify_task_info.kernel_credit = kernel_credit;
            placeholderSqePtr->u.cache_notify_task_info.timeout = timeout;
            placeholderSqePtr->u.cache_notify_task_info.notify_id = notifyId;
        }

        return HCCL_SUCCESS;
    }

    HcclResult OpUnfoldCacheEntry::UpdateWriteValueSqeForAlltoallv(uint8_t *sqePtr, uint8_t *sqeTypePtr, const uint16_t curTaskId, const AlltoallvMetadata& alltoallvMetadata, const AlltoallvSendRecvInfo& alltoallvSendRecvInfo, const uint64_t count, const uint64_t size) {
        UNUSED_PARAM(size);

        rtStarsWriteValueSqe_t *writeValueSqePtr = reinterpret_cast<rtStarsWriteValueSqe_t *>(sqePtr);
        if (count > 0) { // 只需要更新task id
            writeValueSqePtr->header.taskId = curTaskId;
        } else { // 需要将WriteValue转变成cache-write placeholder
            // 保留original SQE中的相关信息
            const uint16_t streamId = writeValueSqePtr->header.rtStreamId;
            const uint32_t lowAddr = writeValueSqePtr->write_addr_low;
            const uint32_t highAddr = writeValueSqePtr->write_addr_high;

            // 清空original SQE
            CHK_SAFETY_FUNC_RET(memset_s(static_cast<void *>(sqePtr), HCCL_SQE_SIZE, 0, HCCL_SQE_SIZE));

            // 生成cache-write placeholder
            // 参考aicpu_hccl_sqcqv1.cc中的AddOneCacheWriteValuePlaceholderSqeV1
            *sqeTypePtr = SqeType::CACHE_WRITE_VALUE_PLACEHOLDER_SQE;
            SetCachePlaceholderHeaderForAlltoallv(streamId, curTaskId, sqePtr);
            rtStarsPlaceHolderSqe_t * const placeholderSqePtr = (rtStarsPlaceHolderSqe_t * const)sqePtr;
            placeholderSqePtr->u.cache_write_value_task_info.write_addr_low = lowAddr;
            placeholderSqePtr->u.cache_write_value_task_info.write_addr_high = highAddr;
        }

        return HCCL_SUCCESS;
    }

    HcclResult OpUnfoldCacheEntry::UpdateMemcpyRecordSqeForAlltoallv(uint8_t *sqePtr, uint8_t *sqeTypePtr, const uint16_t curTaskId, const AlltoallvMetadata& alltoallvMetadata, const AlltoallvSendRecvInfo& alltoallvSendRecvInfo, const uint64_t count, const uint64_t size) {
        UNUSED_PARAM(size);

        rtStarsMemcpyAsyncSqe_t *memcpySqePtr = reinterpret_cast<rtStarsMemcpyAsyncSqe_t *>(sqePtr);
        if (count > 0) { // 只需要更新task id
            memcpySqePtr->header.taskId = curTaskId;
        } else { // 需要将MemcpyRecord转变成cache-memcpy-record placeholder
            // 保留original SQE中的相关信息
            const uint16_t streamId = memcpySqePtr->header.rtStreamId;
            const uint8_t kernelCredit = memcpySqePtr->kernel_credit;
            const uint32_t opCode = memcpySqePtr->opcode;
            const uint32_t length = memcpySqePtr->length;
            const uint32_t srcAddrLow = memcpySqePtr->src_addr_low;
            const uint32_t srcAddrHigh = memcpySqePtr->src_addr_high;
            const uint32_t dstAddrLow = memcpySqePtr->dst_addr_low;
            const uint32_t dstAddrHigh = memcpySqePtr->dst_addr_high;
            const uint32_t partId = memcpySqePtr->partid;
            const uint8_t linkType = memcpySqePtr->linkType;
            const uint32_t qos = memcpySqePtr->qos;

            // 清空original SQE
            CHK_SAFETY_FUNC_RET(memset_s(static_cast<void *>(sqePtr), HCCL_SQE_SIZE, 0, HCCL_SQE_SIZE));

            // 生成cache-memcpy-record placeholder
            // 参考aicpu_hccl_sqcqv1.cc中的AddOneCacheMemcpyRecordPlaceholderSqeV1
            *sqeTypePtr = SqeType::CACHE_MEMCPY_RECORD_PLACEHOLDER_SQE;
            SetCachePlaceholderHeaderForAlltoallv(streamId, curTaskId, sqePtr);
            rtStarsPlaceHolderSqe_t * const placeholderSqePtr = (rtStarsPlaceHolderSqe_t * const)sqePtr;
            placeholderSqePtr->u.cache_memcpy_record_task_info.kernel_credit = kernelCredit;
            placeholderSqePtr->u.cache_memcpy_record_task_info.opcode = opCode;
            placeholderSqePtr->u.cache_memcpy_record_task_info.length = length;
            placeholderSqePtr->u.cache_memcpy_record_task_info.src_addr_low = srcAddrLow;
            placeholderSqePtr->u.cache_memcpy_record_task_info.src_addr_high = srcAddrHigh;
            placeholderSqePtr->u.cache_memcpy_record_task_info.dst_addr_low = dstAddrLow;
            placeholderSqePtr->u.cache_memcpy_record_task_info.dst_addr_high = dstAddrHigh;
            placeholderSqePtr->u.cache_memcpy_record_task_info.partid = partId;
            placeholderSqePtr->u.cache_memcpy_record_task_info.linkType = linkType;
            placeholderSqePtr->u.cache_memcpy_record_task_info.qos = qos;
        }

        return HCCL_SUCCESS;
    }

    HcclResult OpUnfoldCacheEntry::UpdateMemcpyRecordPlaceholderSqeForAlltoallv(uint8_t *sqePtr, uint8_t *sqeTypePtr, const uint16_t curTaskId, const AlltoallvMetadata& alltoallvMetadata, const AlltoallvSendRecvInfo& alltoallvSendRecvInfo, const uint64_t count, const uint64_t size) {
        UNUSED_PARAM(size);
        
        rtStarsPlaceHolderSqe_t *placeholderSqePtr = reinterpret_cast<rtStarsPlaceHolderSqe_t *>(sqePtr);
        if (count == 0) { // 只需要更新task id
            placeholderSqePtr->header.taskId = curTaskId;
        } else { // 需要将cache-memcpy-record placeholder转变成MemcpyRecord
            // 保留original SQE中的相关信息
            const uint16_t streamId = placeholderSqePtr->header.rtStreamId;
            const uint8_t kernelCredit = placeholderSqePtr->u.cache_memcpy_record_task_info.kernel_credit;
            const uint32_t opCode = placeholderSqePtr->u.cache_memcpy_record_task_info.opcode;
            const uint32_t length = placeholderSqePtr->u.cache_memcpy_record_task_info.length;
            const uint32_t srcAddrLow = placeholderSqePtr->u.cache_memcpy_record_task_info.src_addr_low;
            const uint32_t srcAddrHigh = placeholderSqePtr->u.cache_memcpy_record_task_info.src_addr_high;
            const uint32_t dstAddrLow = placeholderSqePtr->u.cache_memcpy_record_task_info.dst_addr_low;
            const uint32_t dstAddrHigh = placeholderSqePtr->u.cache_memcpy_record_task_info.dst_addr_high;
            const uint32_t partId = placeholderSqePtr->u.cache_memcpy_record_task_info.partid;
            const uint32_t linkType = placeholderSqePtr->u.cache_memcpy_record_task_info.linkType;
            const uint32_t qos = placeholderSqePtr->u.cache_memcpy_record_task_info.qos;

            // 清空original SQE
            CHK_SAFETY_FUNC_RET(memset_s(static_cast<void *>(sqePtr), HCCL_SQE_SIZE, 0, HCCL_SQE_SIZE));

            // 生成MemcpyRecord
            // 参考aicpu_hccl_sqcqv1.cc中的AddOneMemcpySqeV1
            *sqeTypePtr = SqeType::MEMCPY_ASYNC_SQE;
            rtStarsMemcpyAsyncSqe_t * const memcpySqePtr = (rtStarsMemcpyAsyncSqe_t * const)sqePtr;
            memcpySqePtr->header.type = RT_STARS_SQE_TYPE_SDMA;
            memcpySqePtr->header.rtStreamId = streamId;
            memcpySqePtr->header.taskId = curTaskId;
            memcpySqePtr->kernel_credit = kernelCredit;
            memcpySqePtr->opcode = opCode;
            memcpySqePtr->length = length;
            memcpySqePtr->src_addr_low = srcAddrLow;
            memcpySqePtr->src_addr_high = srcAddrHigh;
            memcpySqePtr->dst_addr_low = dstAddrLow;
            memcpySqePtr->dst_addr_high = dstAddrHigh;
            memcpySqePtr->sssv = 1U;
            memcpySqePtr->dssv = 1U;
            memcpySqePtr->sns = 1U;
            memcpySqePtr->dns = 1U;
            memcpySqePtr->qos = 6; // 6 is HCCL QoS
            memcpySqePtr->partid = partId;
            memcpySqePtr->linkType = linkType;
            memcpySqePtr->qos = qos;
        }

        return HCCL_SUCCESS;
    }

    void OpUnfoldCacheEntry::SetCachePlaceholderHeaderForAlltoallv(const uint16_t streamId, const uint16_t taskId, uint8_t *sqePtr) {
        // 参考aicpu_hccl_sqcqv1.cc中的SetCachePlaceholderHeaderV1
        // 注意: 不直接调用SetCachePlaceholderHeaderV1, 避免libhccl_plf对platform/task/rtsq_interact产生依赖
        // 目前rtsq_interact只编译到ccl_kernel_plf与ccl_kernel_plf_a中
        rtStarsPlaceHolderSqe_t *placeholderSqePtr = (rtStarsPlaceHolderSqe_t*)sqePtr;
        placeholderSqePtr->header.type = RT_STARS_SQE_TYPE_PLACE_HOLDER;
        placeholderSqePtr->header.ie = 0U;
        placeholderSqePtr->header.preP = 0U; // 不需要STARS_FW参与任何预处理
        placeholderSqePtr->header.postP = 0U;
        placeholderSqePtr->header.wrCqe = 0U;
        placeholderSqePtr->header.reserved = 0U;
        // NOTE: task type在preP阶段被TASK_FW使用, 而此placeholder无preP阶段, 设置为RT_TASK_TYPE_FLIP不影响功能
        placeholderSqePtr->header.blockDim = RT_TASK_TYPE_FLIP;
        placeholderSqePtr->header.rtStreamId = streamId;
        placeholderSqePtr->header.taskId = taskId;
        placeholderSqePtr->kernel_credit = RT_STARS_DEFAULT_KERNEL_CREDIT;
        return;
    }

}; // namespace hccl
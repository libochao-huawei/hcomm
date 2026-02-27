/*
 * This program is free software, you can redistribute it and/or modify it.
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <cstdint>
#include <cstdlib>
#include <limits>

#include "async_unfold_cache_entry.h"

#include "aicpu_cache_utils.h"
#include "log.h"

namespace hccl {

    // class AsyncUnfoldCacheEntry

    AsyncUnfoldCacheEntry::AsyncUnfoldCacheEntry() {}

    AsyncUnfoldCacheEntry::~AsyncUnfoldCacheEntry() {
        size_t sqeArrayCount = asyncUnfoldSqeArrays_.size();
        size_t totalSqeCount = 0;
        for (size_t arrayIdx = 0; arrayIdx < sqeArrayCount; ++arrayIdx) {
            totalSqeCount += asyncUnfoldSqeCnts_[arrayIdx];

            // 如果存在当前这段连续的SQE数组，则指向内容必不为空
            // 因为异步展开时, 如果SQE数量为0, DispatcherAicpu会直接返回, 不会添加SQE到AsyncUnfoldCache中
            uint8_t *curSqeArray = asyncUnfoldSqeArrays_[arrayIdx];

            // 释放当前SQE数组
            if (UNLIKELY(curSqeArray == nullptr)) { // 不能使用CHK_PTR_NULL，因为会return HcclResult
                HCCL_ERROR("[AsyncUnfoldCacheEntry][~AsyncUnfoldCacheEntry] asyncUnfoldSqeArrays_[%u] is nullptr", arrayIdx);
            } else {
                free(curSqeArray);
                curSqeArray = nullptr;
            }

            // 同理释放其他空间

            // 释放当前SQE type数组
            uint8_t *curSqeTypeArray = asyncUnfoldSqeTypeArrays_[arrayIdx];
            if (UNLIKELY(curSqeTypeArray == nullptr)) {
                HCCL_ERROR("[AsyncUnfoldCacheEntry][~AsyncUnfoldCacheEntry] asyncUnfoldSqeTypeArrays_[%u] is nullptr", arrayIdx);
            } else {
                free(curSqeTypeArray);
                curSqeTypeArray = nullptr;
            }

            // 释放当前SQE DfxInfo数组
            AicpuDfxInfo *curSqeDfxInfoArray = asyncUnfoldSqeDfxInfoArrays_[arrayIdx];
            if (UNLIKELY(curSqeDfxInfoArray == nullptr)) {
                HCCL_ERROR("[AsyncUnfoldCacheEntry][~AsyncUnfoldCacheEntry] asyncUnfoldSqeDfxInfoArrays_[%u] is nullptr", arrayIdx);
            } else {
                free(curSqeDfxInfoArray);
                curSqeDfxInfoArray = nullptr;
            }

            // 释放当前SQE profTimestamp数组
            if (arrayIdx < asyncUnfoldProfTimestampArrays_.size()) {
                if (UNLIKELY(asyncUnfoldProfTimestampArrays_[arrayIdx] == nullptr)) {
                    HCCL_ERROR("[AsyncUnfoldCacheEntry][~AsyncUnfoldCacheEntry] asyncUnfoldProfTimestampArrays_[%u] is nullptr", arrayIdx);
                } else {
                    free(asyncUnfoldProfTimestampArrays_[arrayIdx]);
                    asyncUnfoldProfTimestampArrays_[arrayIdx] = nullptr;
                }
            }
        }

        HCCL_INFO("[AsyncUnfoldCacheEntry][~AsyncUnfoldCacheEntry] release %u SQE arrays (%u SQEs in total) from the cache entry",
            sqeArrayCount, totalSqeCount);
    }

    HcclResult AsyncUnfoldCacheEntry::GetAsyncUnfoldSqeArrayCount(size_t& sqeArrayCount) const {
        sqeArrayCount = asyncUnfoldSqeArrays_.size();
        CHK_PRT_RET(sqeArrayCount == 0,
            HCCL_ERROR("[AsyncUnfoldCacheEntry][GetAsyncUnfoldSqeArrayCount] sqeArrayCount is 0"),
            HCCL_E_INTERNAL);
        return HCCL_SUCCESS;
    }

    HcclResult AsyncUnfoldCacheEntry::AllocAsyncUnfoldSqeArray(const size_t sqeCount, const int32_t streamId, size_t& arrayIdx,
        const bool profL1Enable) {
        // 注意: sqeCount不会超过HCCL_PER_LAUNCH_SQE_CNT (默认128), 则也不会超过uint16_t的最大值
        CHK_PRT_RET(sqeCount > HCCL_PER_LAUNCH_SQE_CNT,
            HCCL_ERROR("[AsyncUnfoldCacheEntry][AllocAsyncUnfoldSqeArray] sqeCount %u > HCCL_PER_LAUNCH_SQE_CNT %u",
                sqeCount, HCCL_PER_LAUNCH_SQE_CNT),
            HCCL_E_INTERNAL);
        CHK_PRT_RET(sqeCount > std::numeric_limits<uint16_t>::max(),
            HCCL_ERROR("[AsyncUnfoldCacheEntry][AllocAsyncUnfoldSqeArray] sqeCount %u > max uint16_t %u",
                sqeCount, std::numeric_limits<uint16_t>::max()),
            HCCL_E_INTERNAL);
        asyncUnfoldSqeCnts_.emplace_back(static_cast<uint16_t>(sqeCount));

        // Allocate a new SQE array
        const size_t sqeBytes = sqeCount * HCCL_SQE_SIZE;
        uint8_t *newSqeArray = reinterpret_cast<uint8_t *>(malloc(sqeBytes));
        CHK_PTR_NULL(newSqeArray);
        asyncUnfoldSqeArrays_.emplace_back(newSqeArray);

        // Allocate a new SQE type array
        const size_t sqeTypeBytes = sqeCount * sizeof(uint8_t);
        uint8_t *newSqeTypeArray = reinterpret_cast<uint8_t *>(malloc(sqeTypeBytes));
        CHK_PTR_NULL(newSqeTypeArray);
        asyncUnfoldSqeTypeArrays_.emplace_back(newSqeTypeArray);

        // Allocate a new SQE DFX info array
        const size_t sqeDfxInfoBytes = sqeCount * sizeof(AicpuDfxInfo);
        AicpuDfxInfo *newSqeDfxInfoArray = reinterpret_cast<AicpuDfxInfo *>(malloc(sqeDfxInfoBytes));
        CHK_PTR_NULL(newSqeDfxInfoArray);
        asyncUnfoldSqeDfxInfoArrays_.emplace_back(newSqeDfxInfoArray);

        // Allocate a new profTimestamp array
        if (profL1Enable) {
            const size_t profTimestampBytes = sqeCount * sizeof(uint64_t);
            uint64_t *newProfTimestampArray = reinterpret_cast<uint64_t *>(malloc(profTimestampBytes));
            CHK_PTR_NULL(newProfTimestampArray);
            asyncUnfoldProfTimestampArrays_.emplace_back(newProfTimestampArray);
        }

        // Copy stream ID
        CHK_PRT_RET(streamId < 0,
            HCCL_ERROR("[AsyncUnfoldCacheEntry][AllocAsyncUnfoldSqeArray] streamId %d < 0", streamId),
            HCCL_E_INTERNAL);
        asyncUnfoldStreamIds_.emplace_back(streamId);

        // 注意: streamSeqIdxes_在异步展开结束后, HcclCommAicpu通过CalcAsyncUnfoldStreamSeqIdxes更新

        // Set index of allocated array
        arrayIdx = asyncUnfoldSqeArrays_.size() - 1;

        HCCL_INFO("[AsyncUnfoldCacheEntry][AllocAsyncUnfoldSqeArray] allocate %uth sqe array with sqeCount of %u and streamId of %d",
            arrayIdx, sqeCount, streamId);

        return HCCL_SUCCESS;
    }

    HcclResult AsyncUnfoldCacheEntry::MemcpyAsyncUnfoldSqeArray(const size_t arrayIdx, const size_t sqeStartIdx,
        const size_t sqeCount, const uint8_t *sqeArray, const uint8_t *sqeTypeArray, const AicpuDfxInfo *sqeDfxInfoArray,
        const bool profL1Enable, const std::vector<uint64_t>& profTimestamps, const size_t profTimestampStartIdx) {
        // Copy sqeArray[0:sqeCount) -> asyncUnfoldSqeArrays_[arrayIdx][sqeStartIdx:sqeStartIdx+sqeCount-1]

        // 检验入参
        CHK_PRT_RET(arrayIdx >= asyncUnfoldSqeArrays_.size(),
            HCCL_ERROR("[AsyncUnfoldCacheEntry][MemcpyAsyncUnfoldSqeArray] arrayIdx %u is out of range [0, %u)",
                arrayIdx, asyncUnfoldSqeArrays_.size()),
            HCCL_E_INTERNAL);
        const size_t totalSqeCount = asyncUnfoldSqeCnts_[arrayIdx];
        CHK_PRT_RET(sqeStartIdx + sqeCount - 1 >= totalSqeCount,
            HCCL_ERROR("[AsyncUnfoldCacheEntry][MemcpyAsyncUnfoldSqeArray] sqeStartIdx[%u] + sqeCount[%u] - 1 >= totalSqeCount[%u]",
                sqeStartIdx, sqeCount, totalSqeCount),
            HCCL_E_INTERNAL);
        CHK_PTR_NULL(sqeArray);
        CHK_PTR_NULL(sqeTypeArray);
        CHK_PTR_NULL(sqeDfxInfoArray);
        if (profL1Enable) {
            CHK_PRT_RET(arrayIdx >= asyncUnfoldProfTimestampArrays_.size(),
                HCCL_ERROR("[AsyncUnfoldCacheEntry][MemcpyAsyncUnfoldSqeArray] arrayIdx %u is out of range [0, %u)",
                    arrayIdx, asyncUnfoldProfTimestampArrays_.size()),
                HCCL_E_INTERNAL);

            // 会访问profTimestamps[profTimestampStartIdx, profTimestampStartIdx + sqeCount - 1]
            CHK_PRT_RET(profTimestamps.size() == 0,
                HCCL_ERROR("[AsyncUnfoldCacheEntry][MemcpyAsyncUnfoldSqeArray] empty profTimestamps"),
                HCCL_E_INTERNAL);
            CHK_PRT_RET(profTimestampStartIdx >= profTimestamps.size(),
                HCCL_ERROR("[AsyncUnfoldCacheEntry][MemcpyAsyncUnfoldSqeArray] "
                    "profTimestampStartIdx[%u] >= profTimestamps.size[%u]",
                    profTimestampStartIdx, profTimestamps.size()),
                HCCL_E_INTERNAL);
            CHK_PRT_RET((profTimestampStartIdx + sqeCount - 1) >= profTimestamps.size(),
                HCCL_ERROR("[AsyncUnfoldCacheEntry][MemcpyAsyncUnfoldSqeArray] profTimestampStartIdx[%u] + "
                    "sqeCount[%u] - 1 >= profTimestamps.size[%u]",
                    profTimestampStartIdx, sqeCount, profTimestamps.size()),
                HCCL_E_INTERNAL);
        }

        HCCL_INFO("[AsyncUnfoldCacheEntry][MemcpyAsyncUnfoldSqeArray] memcpy %uth sqe array[%u:%u]",
            arrayIdx, sqeStartIdx, sqeStartIdx + sqeCount - 1);

        // Copy SQE content
        const size_t sqeBytes = sqeCount * HCCL_SQE_SIZE;
        uint8_t *dstSqeArray = asyncUnfoldSqeArrays_[arrayIdx];
        CHK_PTR_NULL(dstSqeArray);
        CHK_SAFETY_FUNC_RET(memcpy_s(dstSqeArray + sqeStartIdx * HCCL_SQE_SIZE,
            (totalSqeCount - sqeStartIdx) * HCCL_SQE_SIZE, sqeArray, sqeBytes));

        // Copy SQE type
        const size_t sqeTypeBytes = sqeCount * sizeof(uint8_t);
        uint8_t *dstSqeTypeArray = asyncUnfoldSqeTypeArrays_[arrayIdx];
        CHK_PTR_NULL(dstSqeTypeArray);
        CHK_SAFETY_FUNC_RET(memcpy_s(dstSqeTypeArray + sqeStartIdx, (totalSqeCount - sqeStartIdx) * sizeof(uint8_t),
            sqeTypeArray, sqeTypeBytes));

        // Copy SQE DFX info
        const size_t sqeDfxInfoBytes = sqeCount * sizeof(AicpuDfxInfo);
        AicpuDfxInfo *dstSqeDfxInfoArray = asyncUnfoldSqeDfxInfoArrays_[arrayIdx];
        CHK_PTR_NULL(dstSqeDfxInfoArray);
        CHK_SAFETY_FUNC_RET(memcpy_s(dstSqeDfxInfoArray + sqeStartIdx,
            (totalSqeCount - sqeStartIdx) * sizeof(AicpuDfxInfo), sqeDfxInfoArray, sqeDfxInfoBytes));

        // Copy profTimestamp
        if (profL1Enable) {
            const size_t profTimestampBytes = sqeCount * sizeof(uint64_t);
            uint64_t *dstProfTimestampArray = asyncUnfoldProfTimestampArrays_[arrayIdx];
            CHK_PTR_NULL(dstProfTimestampArray);
            CHK_SAFETY_FUNC_RET(memcpy_s(dstProfTimestampArray + sqeStartIdx,
                (totalSqeCount - sqeStartIdx) * sizeof(uint64_t),
                profTimestamps.data() + profTimestampStartIdx,
                profTimestampBytes));
        }

        return HCCL_SUCCESS;
    }

    HcclResult AsyncUnfoldCacheEntry::CalcAsyncUnfoldStreamSeqIdxes(Stream& mainStream, std::vector<Stream>& slaveStreams) {
        const size_t streamIdCount = asyncUnfoldStreamIds_.size();
        HCCL_INFO("[AsyncUnfoldCacheEntry][CalcAsyncUnfoldStreamSeqIdxes] calculate stream sequential indexes for %u stream ids",
            streamIdCount);

        // 对每个stream id找到对应的sequential stream index
        asyncUnfoldStreamSeqIdxes_.resize(streamIdCount);
        for (size_t i = 0; i < streamIdCount; ++i) {
            const int32_t curStreamId = asyncUnfoldStreamIds_[i];

            if (curStreamId == mainStream.GetHcclStreamInfo().actualStreamId) { // 主流
                asyncUnfoldStreamSeqIdxes_[i] = 0;
            } else { // 遍历从流
                bool isFound = false;
                for (size_t j = 0; j < slaveStreams.size(); ++j) {
                    if (curStreamId == slaveStreams[j].GetHcclStreamInfo().actualStreamId) { // 匹配某个从流
                        asyncUnfoldStreamSeqIdxes_[i] = j + 1;
                        isFound = true;
                        break;
                    }
                }

                // No stream can match the stream id
                if (!isFound) {
                    HCCL_ERROR("[AsyncUnfoldCacheEntry][CalcAsyncUnfoldStreamSeqIdxes] cannot find any stream to match streamId %u",
                        curStreamId);
                    return HCCL_E_INTERNAL;
                }
            }
        }

        return HCCL_SUCCESS;
    }

    HcclResult AsyncUnfoldCacheEntry::GetAsyncUnfoldSqeArray(const size_t arrayIdx,
        Stream& mainStream, std::vector<Stream> &slaveStreams,
        size_t& sqeCount, uint8_t **sqeArrayPtr, uint8_t **sqeTypeArrayPtr,
        AicpuDfxInfo **sqeDfxInfoArrayPtr, Stream **streamPtrPtr,
        const bool profL1Enable, uint64_t **profTimestampArrayPtr) {
        HCCL_INFO("[AsyncUnfoldCacheEntry][GetAsyncUnfoldSqeArray] get SQEs from %uth SQE array", arrayIdx);

        // 检验入参
        CHK_PRT_RET(arrayIdx >= asyncUnfoldSqeArrays_.size(),
            HCCL_ERROR("[AsyncUnfoldCacheEntry][GetAsyncUnfoldSqeArray] arrayIdx %u is out of range [0, %u)",
                arrayIdx, asyncUnfoldSqeArrays_.size()),
            HCCL_E_INTERNAL);
        CHK_PRT_RET(arrayIdx >= asyncUnfoldStreamSeqIdxes_.size(),
            HCCL_ERROR("[AsyncUnfoldCacheEntry][GetAsyncUnfoldSqeArray] arrayIdx %u is out of range [0, %u)",
                arrayIdx, asyncUnfoldStreamSeqIdxes_.size()),
            HCCL_E_INTERNAL);
        // 检查指针, arrayPtr不应该是null, 但*arrayPtr应该是null
        CHK_PTRPTR_NULL(sqeArrayPtr);
        CHK_PTRPTR_NULL(sqeTypeArrayPtr);
        CHK_PTRPTR_NULL(sqeDfxInfoArrayPtr);
        CHK_PTRPTR_NULL(streamPtrPtr);
        if (profL1Enable) {
            CHK_PTRPTR_NULL(profTimestampArrayPtr);
        }

        // 设置入参
        sqeCount = asyncUnfoldSqeCnts_[arrayIdx];
        *sqeArrayPtr = asyncUnfoldSqeArrays_[arrayIdx];
        *sqeTypeArrayPtr = asyncUnfoldSqeTypeArrays_[arrayIdx];
        *sqeDfxInfoArrayPtr = asyncUnfoldSqeDfxInfoArrays_[arrayIdx];
        if (profL1Enable) {
            *profTimestampArrayPtr = asyncUnfoldProfTimestampArrays_[arrayIdx];
        }

        // 设置入参的stream pointer
        const uint32_t streamSeqIdx = asyncUnfoldStreamSeqIdxes_[arrayIdx];
        if (streamSeqIdx == 0) {
            *streamPtrPtr = &mainStream;
        } else {
            CHK_PRT_RET(streamSeqIdx > slaveStreams.size(),
                HCCL_ERROR("[AsyncUnfoldCacheEntry][GetAsyncUnfoldSqeArray] invalid streamSeqIdx %u > slaveStreams.size() %u",
                    streamSeqIdx, slaveStreams.size()),
                HCCL_E_MEMORY);
            *streamPtrPtr = &(slaveStreams[streamSeqIdx - 1]); // 0 < streamSeqIdx <= slaveStreams.size())
        }

        HCCL_INFO("[AsyncUnfoldCacheEntry][GetAsyncUnfoldSqeArray] get %uth SQE array with %u SQEs, streamId[%u]",
            arrayIdx, sqeCount, (*streamPtrPtr)->GetHcclStreamInfo().actualStreamId);

        return HCCL_SUCCESS;
    }

}; // namespace hccl
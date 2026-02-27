/*
 * This program is free software, you can redistribute it and/or modify it.
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef __ASYNC_UNFOLD_CACHE_ENTRY_H__
#define __ASYNC_UNFOLD_CACHE_ENTRY_H__

#include <cstdint>
#include <vector>

#include "stream_pub.h"

namespace hccl {

// 注意: 与OpUnfoldCacheEntry不同, AsyncUnfoldCacheEntry用于AICPU异步展开后续算子的静态缓存条目, 需要缓存所有展开结果 (包括placeholder)
// 而OpUnfoldCacheEntry用于AICPU同步展开当前算子的动态缓存条目, 需要对后续相同类型算子支持动态刷新, 但不需要缓存placeholder
// 注意: 每个AsyncUnfoldKey对应最多一个缓存条目
class AsyncUnfoldCacheEntry {
public:
    explicit AsyncUnfoldCacheEntry();
    ~AsyncUnfoldCacheEntry();

    // 获得当前缓存条目中, 异步展开后续算子生成的SQE数组数量
    HcclResult GetAsyncUnfoldSqeArrayCount(size_t& sqeArrayCount) const;

    // 当前算子异步展开后续算子的函数

    // 异步展开过程中, 为当前生成的SQE数组分配缓存空间
    // 注意: 分配成功会将arrayIdx设置为分配的SQE数组在sqeArrays_当中的索引
    HcclResult AllocAsyncUnfoldSqeArray(const size_t sqeCount, const int32_t streamId, size_t& arrayIdx,
        const bool profL1Enable);

    // 异步展开过程中, 将当前生成的SQE数组memcpy到缓存中
    // 注意: 将sqeArray memcpy到asyncUnfoldSqeArrays_[arrayIdx][sqeStartIdx:sqeStartIdx+sqeCount-1]
    HcclResult MemcpyAsyncUnfoldSqeArray(const size_t arrayIdx, const size_t sqeStartIdx, const size_t sqeCount,
        const uint8_t *sqeArray, const uint8_t *sqeTypeArray, const AicpuDfxInfo *sqeDfxInfoArray,
        const bool profL1Enable, const uint64_t *profTimestamps);

    // 根据streamId计算streamSeqIdx
    HcclResult CalcAsyncUnfoldStreamSeqIdxes(Stream& mainStream, std::vector<Stream>& slaveStreams);

    // 后续算子命中并加载异步展开缓存的函数

    // 加载指定的一段连续SQE, 并将相关信息设置给对应指针, 用于直接下发task到RTSQ
    HcclResult GetAsyncUnfoldSqeArray(const size_t arrayIdx,
        Stream& mainStream, std::vector<Stream> &slaveStreams,
        size_t& sqeCount, uint8_t **sqeArrayPtr, uint8_t **sqeTypeArrayPtr,
        AicpuDfxInfo **sqeDfxInfoArrayPtr, Stream **streamPtrPtr,
        const bool profL1Enable, uint64_t **profTimestampArrayPtr);

private:
    // 分段保存异步展开生成的SQE
    std::vector<uint16_t> asyncUnfoldSqeCnts_; // 每段SQE数组的SQE数量 (每段不会超过HCCL_PER_LAUNCH_SQE_CNT, 即128个SQE)
    std::vector<uint8_t *> asyncUnfoldSqeArrays_; // 多段连续的SQE数组 (每段连续的SQE不超过HCCL_SQE_SIZE * HCCL_PER_LAUNCH_SQE_CNT bytes)
    std::vector<uint8_t *> asyncUnfoldSqeTypeArrays_; // 每段每个SQE的type
    std::vector<AicpuDfxInfo *>  asyncUnfoldSqeDfxInfoArrays_; // 每段每个SQE的DfxInfo
    std::vector<uint64_t *> asyncUnfoldProfTimestampArrays_; // 每段每个SQE的profTimestamp (即生成时间)

    // 每段SQE对应的actual stream ID
    std::vector<int32_t> asyncUnfoldStreamIds_;
    // 每段SQE对应的sequential stream index (后续算子命中并加载异步展开缓存时, 用于索引对应stream实例)
    // 注意: sequential是指将mainStream + slaveStreams顺序起来看, 0代表mainStream, 1代表slaveStreams[0]
    std::vector<uint32_t> asyncUnfoldStreamSeqIdxes_;
};

} // namespace hccl

#endif // __ASYNC_UNFOLD_CACHE_ENTRY_H__
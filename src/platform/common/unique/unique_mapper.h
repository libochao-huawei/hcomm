/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef UNIQUE_MAPPER
#define UNIQUE_MAPPER

#include <vector>
#include <unordered_map>
#include <functional>
#include "hccl_types.h"
#include "unsafe_table.h"

namespace hccl {

template <typename T>
struct Buffer {
    T *eles{};
    u32 count{};
    u32 elesCapacity{};
};

template <typename T, typename TC = u32>
struct KeyAndCounter {
    T key{};
    TC counter{};
};

template <typename T, typename TM>
class UniqueMapper {
public:

    struct ConUniqueMappersInfo {
        u32 conMapperNum{}; // conMapper的数量，当前可对应psSize
        u32 parallelNum{}; // 每个conMapper的并行数量
        u32 maxParallelNum{};
        Buffer<T> srcData{};
        Buffer<T> uniqueData{};
        PsBufferInfo *preSlicingOffsetAndNum{};
        u32 *preSlicingInfo{};
        u64 mapReserveNum{};
        bool splitData{};
        bool enableKeyCounter{};
    };

    struct KeyInfo {
        TM index{};
        u32 counter{};
    };

    using MtxBuffer = Buffer<TM>;
    using USTable = Hss::UnsafeTable<T, TM>;
    using InsertResult = typename USTable::InsertResult;

    using CounterUSTable = Hss::UnsafeTable<T, KeyInfo>;
    using CounterInsertResult = typename CounterUSTable::InsertResult;

    struct SplitDataEle {
        SplitDataEle(const T &d, u32 i)
        {
            data = d;
            idx = i;
        }

        T data{};
        u32 idx{};
    };

    UniqueMapper()
    {
        unsafeTable_ = new(std::nothrow) USTable();
        counterUnsafeTable_ = new(std::nothrow) CounterUSTable();
    }

    ~UniqueMapper()
    {
        if (unsafeTable_ != nullptr) {
            delete unsafeTable_;
        }

        if (counterUnsafeTable_ != nullptr) {
            delete counterUnsafeTable_;
        }
    }

    // 当前未对外提供使用，为了保证性能，参数校验由调用者保证
    HcclResult CfgInfo(const Buffer<T> &srcData, const Buffer<T> &uniqueData,
        std::function<bool(const u32&, const T&)> filtered, const ConUniqueMappersInfo &info)
    {
        if (srcData.eles == nullptr || uniqueData.eles == nullptr) {
            HCCL_ERROR("srcData.eles or uniqueData.eles is nullptr");
            return HCCL_E_PARA;
        }

        HCCL_DEBUG("srcData.count is [%u], uniqueData.count is [%u], mapReserveNum[%llu], enableKeyCounter[%d]",
            srcData.count, uniqueData.count, info.mapReserveNum, info.enableKeyCounter);

        srcData_ = srcData;
        uniqueData_ = uniqueData;
        filtered_ = filtered;
        uniqueDataBasePos_ = uniqueData.count;

        enableKeyCounter_ = info.enableKeyCounter;
        if (enableKeyCounter_) {
            counterUniqueData_.eles = reinterpret_cast<KeyAndCounter<T> *>(uniqueData.eles);
            counterUniqueData_.count = uniqueData.count;
            counterUniqueData_.elesCapacity = uniqueData.elesCapacity;
        }

        if (enableKeyCounter_) {
            CHK_PRT_RET(counterUnsafeTable_->Reserve(info.mapReserveNum * MAP_RESERVE_FACTOR) != 0,
                HCCL_ERROR("counterUnsafeTable_ Reserve failed ret"), HCCL_E_INTERNAL);
        } else {
            CHK_PRT_RET(unsafeTable_->Reserve(info.mapReserveNum * MAP_RESERVE_FACTOR) != 0,
                HCCL_ERROR("unsafeTable_ Reserve failed ret"), HCCL_E_INTERNAL);
        }

        splitSrcDataIdx_.reserve(info.mapReserveNum * VEC_RESERVE_FACTOR);

        return HCCL_SUCCESS;
    }
    // 方案A.1
    HcclResult SplitDataAndGenerateMappingTableAndMatrix(MtxBuffer &mappingMatrix)
    {
        for (u32 i = 0; i < srcData_.count; i++) {
            T &data = srcData_.eles[i];
            if (filtered_(i, data)) {
                continue;
            }

            TM uniqueDataIdx = static_cast<TM>(unsafeTable_->Size() + uniqueDataBasePos_);
            auto result = unsafeTable_->InsertOrFind(data);
            if (result.second == InsertResult::NEW_INSERTED) {
                *(result.first) = uniqueDataIdx;
            }

            mappingMatrix.eles[i] = *(result.first);
            splitSrcDataIdx_.emplace_back(i);
        }

        return HCCL_SUCCESS;
    }

    // 方案A.2
    HcclResult SplitAddOffsetToMappingMatrix(MtxBuffer &mappingMatrix, u32 uniqueIdsOffset) const
    {
        for (u32 cnt = 0; cnt < splitSrcDataIdx_.size(); cnt++) {
            u32 i = splitSrcDataIdx_[cnt];
            mappingMatrix.eles[i] += static_cast<TM>(uniqueIdsOffset);
        }

        return HCCL_SUCCESS;
    }

    // keyCounter表方案A.1
    HcclResult SplitDataAndGenerateMappingTableAndMatrixWithCounter(MtxBuffer &mappingMatrix)
    {
        for (u32 i = 0; i < srcData_.count; i++) {
            T &data = srcData_.eles[i];
            if (filtered_(i, data)) {
                continue;
            }

            TM uniqueDataIdx = static_cast<TM>(counterUnsafeTable_->Size() + uniqueDataBasePos_);
            auto result = counterUnsafeTable_->InsertOrFind(data);
            if (result.second == CounterInsertResult::NEW_INSERTED) {
                result.first->index = uniqueDataIdx;
                result.first->counter = INIT_KEY_COUNTER;
            } else {
                if (UNLIKELY(result.first->counter == UINT32_MAX)) {
                    HCCL_WARNING("key counter reach UINT32_MAX!!!");
                } else {
                    result.first->counter++;
                }
            }

            mappingMatrix.eles[i] = result.first->index;
            splitSrcDataIdx_.emplace_back(i);
        }

        return HCCL_SUCCESS;
    }

    // 方案B.1
    HcclResult SplitDataAndGenerateMappingTable()
    {
        for (u32 i = 0; i < srcData_.count; i++) {
            T &data = srcData_.eles[i];
            if (filtered_(i, data)) {
                continue;
            }

            TM uniqueDataIdx = static_cast<TM>(unsafeTable_->Size() + uniqueDataBasePos_);
            auto result = unsafeTable_->InsertOrFind(data);
            if (result.second == InsertResult::NEW_INSERTED) {
                *(result.first) = uniqueDataIdx;
            }

            splitSrcDataIdx_.emplace_back(i);
        }

        return HCCL_SUCCESS;
    }

    // 方案B.2
    HcclResult SplitDataGetMappingMatrix(MtxBuffer &mappingMatrix, u32 uniqueIdsOffset) const
    {
        for (u32 cnt = 0; cnt < splitSrcDataIdx_.size(); cnt++) {
            u32 i = splitSrcDataIdx_[cnt];
            T &data = srcData_.eles[i];

            auto result = unsafeTable_->Find(data);
#ifdef UNIQUE_SEC_CHECK
            if (result == nullptr) {
                HCCL_ERROR("Cannot find ele in map");
                return HCCL_E_INTERNAL;
            }
#endif
            mappingMatrix.eles[i] = *result + static_cast<TM>(uniqueIdsOffset);
        }

        return HCCL_SUCCESS;
    }

    // 仅供单线程去重时使用
    HcclResult UniqueAndGetMappingMatrix(MtxBuffer &mappingMatrix)
    {
        for (u32 i = 0; i < srcData_.count; i++) {
            T &data = srcData_.eles[i];
            if (filtered_(i, data)) {
                continue;
            }

            TM uniqueDataIdx = static_cast<TM>(unsafeTable_->Size() + uniqueDataBasePos_);
            auto result = unsafeTable_->InsertOrFind(data);
            if (result.second == InsertResult::NEW_INSERTED) {
                *(result.first) = uniqueDataIdx;
            }

#ifdef UNIQUE_SEC_CHECK // 去重性能较为敏感，为了高性能，不做检查。当有功能问题时候可打开检查
            CHK_PRT_RET(i >= mappingMatrix.elesCapacity,
                HCCL_ERROR("idx[%u] >= elesCapacity[%u] in mappingMatrix",
                i, mappingMatrix.elesCapacity), HCCL_E_NOT_FOUND);
#endif

            mappingMatrix.eles[i] = *(result.first);

            // 已经重复
            if (result.second == InsertResult::FOUND) {
#ifdef UNIQUE_SEC_CHECK
                if (*(result.first) < uniqueDataIdx) {
                    continue;
                }

                HCCL_ERROR("uniqueDataIdx[%x] in map >= current uniqueDataIdx[%x]", *(result.first),
                    uniqueDataIdx);
                return HCCL_E_INTERNAL;
#else
                continue;
#endif
            }
            uniqueData_.eles[uniqueDataIdx] = data;
        }

        return HCCL_SUCCESS;
    }

    // 方案C.1
    HcclResult SplitGenerateMappingTable()
    {
        for (u32 cnt = 0; cnt < splitSrcData_.size(); cnt++) {
            T &data = splitSrcData_[cnt].data;

            TM uniqueDataIdx = static_cast<TM>(unsafeTable_->Size() + uniqueDataBasePos_);
            auto result = unsafeTable_->InsertOrFind(data);
            if (result.second == InsertResult::NEW_INSERTED) {
                *(result.first) = uniqueDataIdx;
            }
        }

        return HCCL_SUCCESS;
    }

    // 方案C.2
    HcclResult SplitGetMappingMatrix(MtxBuffer &mappingMatrix, u32 uniqueIdsOffset) const
    {
        for (u32 cnt = 0; cnt < splitSrcData_.size(); cnt++) {
            u32 i = splitSrcData_[cnt].idx;
            const T &data = splitSrcData_[cnt].data;

            auto result = unsafeTable_->Find(data);
#ifdef UNIQUE_SEC_CHECK
            if (result == nullptr) {
                HCCL_ERROR("Cannot find ele in map");
                return HCCL_E_INTERNAL;
            }

            CHK_PRT_RET(i >= mappingMatrix.elesCapacity,
                HCCL_ERROR("idx[%u] >= elesCapacity[%u] in mappingMatrix",
                i, mappingMatrix.elesCapacity), HCCL_E_NOT_FOUND);
#endif

            mappingMatrix.eles[i] = *result + static_cast<TM>(uniqueIdsOffset);
        }

        return HCCL_SUCCESS;
    }

    // 方案C.3 mappingMatrix的count是0
    HcclResult SplitReduceByMappingMatrix(const MtxBuffer &mappingMatrix)
    {
        for (u32 cnt = 0; cnt < splitSrcDataIdx_.size(); cnt++) {
            u32 i = splitSrcDataIdx_[cnt];
            T &data = srcData_.eles[i];

            HcclResult ret = ReduceData(i, data, mappingMatrix);
#ifdef UNIQUE_SEC_CHECK
            if (ret != HCCL_SUCCESS) { // 性能敏感，不打印错误码
                HCCL_ERROR("ReduceData Failed");
                return ret;
            }
#else
            static_cast<void>(ret);
#endif
        }

        return HCCL_SUCCESS;
    }

    // keyCounter方案C.3 mappingMatrix的count是0
    HcclResult SplitReduceByMappingMatrixWithCounter(const MtxBuffer &mappingMatrix)
    {
        for (u32 cnt = 0; cnt < splitSrcDataIdx_.size(); cnt++) {
            u32 i = splitSrcDataIdx_[cnt];
            T &data = srcData_.eles[i];
            TM &uniqueIdx = mappingMatrix.eles[i];
#ifdef UNIQUE_SEC_CHECK
            CHK_PRT_RET(uniqueIdx >= uniqueData_.elesCapacity,
                HCCL_ERROR("uniqueIdx[%x] in mappingMatrix[%u] >= elesCapacity[%u] in uniqueData",
                uniqueIdx, idx, uniqueData_.elesCapacity), HCCL_E_NOT_FOUND);
#endif
            // 由于重复率较高，且内存读性能大于写性能，为了更高的性能，故先判断满足条件再写内存
            if (counterUniqueData_.eles[uniqueIdx].key != data) {
                counterUniqueData_.eles[uniqueIdx].key = data;
            }

            auto result = counterUnsafeTable_->Find(data);
#ifdef UNIQUE_SEC_CHECK
            CHK_PTR_NULL(result);
#endif

            if (counterUniqueData_.eles[uniqueIdx].counter != result->counter) {
                counterUniqueData_.eles[uniqueIdx].counter = result->counter;
            }
        }

        return HCCL_SUCCESS;
    }

    // 非spilt时使用，mappingMatrix的count是0
    HcclResult ReduceByMappingMatrix(const MtxBuffer &mappingMatrix)
    {
        for (u32 i = 0; i < srcData_.count; i++) {
            T &data = srcData_.eles[i];
            if (filtered_(i, data)) { // 不需要根据data切分时，filtered_内部忽略即可
                continue;
            }

            HcclResult ret = ReduceData(i, data, mappingMatrix);
#ifdef UNIQUE_SEC_CHECK
            if (ret != HCCL_SUCCESS) { // 性能敏感，不打印错误码
                HCCL_ERROR("ReduceData Failed");
                return ret;
            }
#else
            static_cast<void>(ret);
#endif
        }

        return HCCL_SUCCESS;
    }

    u32 GetUniqueDataCount() const
    {
        if (enableKeyCounter_) {
            return counterUnsafeTable_->Size();
        }

        return unsafeTable_->Size();
    }

    std::vector<SplitDataEle> &GetSplitSrcData()
    {
        return splitSrcData_;
    }

    void ClearData()
    {
        splitSrcData_.clear();
        splitSrcDataIdx_.clear();

        if (enableKeyCounter_) {
            counterUnsafeTable_->Clear();
        } else {
            unsafeTable_->Clear();
        }
    }

    std::vector<SplitDataEle> splitSrcData_{};
    std::vector<u32> splitSrcDataIdx_{};

private:
    static constexpr u32 MAP_RESERVE_FACTOR = 2;
    static constexpr u32 VEC_RESERVE_FACTOR = 4;
    static constexpr u32 INIT_KEY_COUNTER = 1;

    Buffer<T> uniqueData_{};
    u32 uniqueDataBasePos_{};
    Buffer<T> srcData_{};
    USTable *unsafeTable_{ nullptr };

    std::function<bool(const u32&, const T&)> filtered_{};

    CounterUSTable *counterUnsafeTable_{ nullptr };
    Buffer<KeyAndCounter<T>> counterUniqueData_{};
    bool enableKeyCounter_{};

    HcclResult ReduceData(u32 idx, const T &data, const MtxBuffer &mappingMatrix)
    {
#ifdef UNIQUE_SEC_CHECK
        CHK_PRT_RET(idx >= mappingMatrix.elesCapacity,
            HCCL_ERROR("idx[%u] >= elesCapacity[%u] in mappingMatrix",
            idx, mappingMatrix.elesCapacity), HCCL_E_NOT_FOUND);
#endif

        TM &uniqueIdx = mappingMatrix.eles[idx];
#ifdef UNIQUE_SEC_CHECK
        CHK_PRT_RET(uniqueIdx >= uniqueData_.elesCapacity,
            HCCL_ERROR("uniqueIdx[%x] in mappingMatrix[%u] >= elesCapacity[%u] in uniqueData",
            uniqueIdx, idx, uniqueData_.elesCapacity), HCCL_E_NOT_FOUND);
#endif

        // 由于重复率较高，且内存读性能大于写性能，为了更高的性能，故先判断满足条件再写内存
        if (uniqueData_.eles[uniqueIdx] != data) {
            uniqueData_.eles[uniqueIdx] = data;
        }

        return HCCL_SUCCESS;
    }
};
}
#endif // UNIQUE_MAPPER

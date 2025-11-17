/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef CON_UNIQUE_MAPPERS_MGR
#define CON_UNIQUE_MAPPERS_MGR

#include <vector>
#include "concurrent_unique_mapper.h"
#include "aicpu_sharder.h"
#include "hccl_types.h"

namespace hccl {

template <typename T, typename TM>
class ConUniqueMappersMgr {
public:
    using MtxBuffer = typename ConcurrentUniqueMapper<T, TM>::MtxBuffer;
    using SplitDataEle = typename ConcurrentUniqueMapper<T, TM>::SplitDataEle;
    using ConUniqueMappersInfo = typename ConcurrentUniqueMapper<T, TM>::ConUniqueMappersInfo;

    ConUniqueMappersMgr()
    {
    }

    ~ConUniqueMappersMgr()
    {
    }

    HcclResult Cfg(const ConUniqueMappersInfo &info)
    {
        // preSlicingInfo可以为空，如在conMapperNum为1时
        if (info.conMapperNum == 0 || info.parallelNum == 0 || info.preSlicingOffsetAndNum == nullptr) {
            HCCL_ERROR("conMapperNum[%u] or parallelNum[%u] is 0, preSlicingOffsetAndNum is nullptr",
                info.conMapperNum, info.parallelNum);
            return HCCL_E_PARA;
        }

        PRINT_ARRAY(info.srcData.eles, info.srcData.count, "Keys");
        PRINT_ARRAY(reinterpret_cast<s64 *>(info.preSlicingOffsetAndNum), info.conMapperNum *
            PS_BUFF_INFO_ELE_NUM, "PsBufferInfo");
        PRINT_ARRAY(info.preSlicingInfo, info.srcData.count, "preSlicingInfo");

        parallelNum_ = info.parallelNum;
        conMapperNum_ = info.conMapperNum;
        mappers_.resize(info.conMapperNum);
        splitData_ = info.splitData;
        enableKeyCounter_ = info.enableKeyCounter;

        Buffer<T> uniqData = info.uniqueData;
        std::function<bool(const u32&)> filtered;
        // 当前业务中，每个PSId对应parallelNum个线程
        for (u32 i = 0; i < conMapperNum_; i++) {
            // 为了偏于获取绝对偏移量，count不一定从0开始，最终获取去重后count时需注意
            uniqData.count = info.preSlicingOffsetAndNum[i].offset;

            if (conMapperNum_ > CON_MAPPER_NUM_ONE) {
                filtered = [i, &info] (const u32& index) -> bool {
                    u32 psId = info.preSlicingInfo[index];

                    return (psId != i);
                };
            } else {
                filtered = [] (const u32& index) -> bool {
                    return false;
                };
            }

            auto &mappers = this->GetMappers();
            EXECEPTION_CATCH(CHK_RET(mappers.at(i).Cfg(parallelNum_, info.srcData, uniqData, filtered, info)),
                return HCCL_E_NOT_FOUND);
        }

        return HCCL_SUCCESS;
    }

    HcclResult UniqueDataAndGetMappingMatrix(MtxBuffer &mappingMatrix)
    {
        // 方案A
        if (enableKeyCounter_) {
            TIME_PRINT(CHK_RET(ParallelSplitDataAndGenerateMappingTableAndMatrixWithCounter(mappingMatrix)));
        } else {
            TIME_PRINT(CHK_RET(ParallelSplitDataAndGenerateMappingTableAndMatrix(mappingMatrix)));
        }

        TIME_PRINT(CHK_RET(ParallelCollectUniqueSizeInfo()));
        TIME_PRINT(CHK_RET(ParallelSplitAddOffsetToMappingMatrix(mappingMatrix)));

        if (enableKeyCounter_) {
            TIME_PRINT(CHK_RET(ParallelReduceByMappingMatrixWithCounter(mappingMatrix)));
        } else {
            TIME_PRINT(CHK_RET(ParallelReduceByMappingMatrix(mappingMatrix)));
        }

        PRINT_ARRAY(mappingMatrix.eles, mappingMatrix.elesCapacity, "MappingMatrix");

        return HCCL_SUCCESS;
    }

    // 需在调用时候捕获异常
    void SplitData(const ConUniqueMappersInfo &info)
    {
        for (u32 i = 0; i < info.srcData.count; i++) {
            T &data = info.srcData.eles[i];

            u32 rowIdx = info.preSlicingInfo[i];
            u32 colIdx = static_cast<u32>(data % parallelNum_);

            mappers_.at(rowIdx).GetUniqueMapper(colIdx).GetSplitSrcData().emplace_back(data, i);
        }
    }

    // 直接完成去重，并输出映射矩阵。限parallelNum_为1时调用
    HcclResult ParallelUniqueAndGetMappingMatrix(MtxBuffer &mappingMatrix)
    {
#ifdef CCL_KERNEL
        auto uniqueAndGetMappingMatrixWork = [&mappingMatrix, this] (int64_t start, int64_t end) -> HcclResult {
            for (int64_t i = start; i < end; i++) {
                u32 idx = static_cast<u32>(i);

                EXECEPTION_CATCH(CHK_RET(mappers_.at(idx).GetUniqueMapper(0).UniqueAndGetMappingMatrix(mappingMatrix)),
                    return HCCL_E_NOT_FOUND);
            }

            return HCCL_SUCCESS;
        };

        ParallelFor(conMapperNum_, PARALLEL_UNIT_SIZE, uniqueAndGetMappingMatrixWork);
#endif
        return HCCL_SUCCESS;
    }

    // 只生成映射表
    HcclResult ParallelGenMappingTables()
    {
#ifdef CCL_KERNEL
        auto generateMappingTableWork = [this] (int64_t start, int64_t end) -> HcclResult {
            for (int64_t i = start; i < end; i++) {
                u32 tidIdx = static_cast<u32>(i);

                u32 rowIdx = tidIdx % conMapperNum_;
                u32 colIdx = tidIdx / conMapperNum_;

                EXECEPTION_CATCH(CHK_RET(mappers_.at(rowIdx).GetUniqueMapper(colIdx).SplitGenerateMappingTable()),
                    return HCCL_E_NOT_FOUND);
            }

            return HCCL_SUCCESS;
        };

        ParallelFor(conMapperNum_ * parallelNum_, PARALLEL_UNIT_SIZE, generateMappingTableWork);
#endif
        return HCCL_SUCCESS;
    }

    // 方案B.1
    HcclResult ParallelSplitDataAndGenerateMappingTable()
    {
#ifdef CCL_KERNEL
        auto generateMappingTableWork = [this] (int64_t start, int64_t end) -> HcclResult {
            for (int64_t i = start; i < end; i++) {
                u32 tidIdx = static_cast<u32>(i);

                u32 rowIdx = tidIdx % conMapperNum_;
                u32 colIdx = tidIdx / conMapperNum_;

                EXECEPTION_CATCH(CHK_RET(mappers_.at(rowIdx).GetUniqueMapper(colIdx). \
                    SplitDataAndGenerateMappingTable()), return HCCL_E_NOT_FOUND);
            }

            return HCCL_SUCCESS;
        };

        ParallelFor(conMapperNum_ * parallelNum_, PARALLEL_UNIT_SIZE, generateMappingTableWork);
#endif
        return HCCL_SUCCESS;
    }

    // 方案B.2
    HcclResult ParallelSplitDataGetMappingMatrixs(MtxBuffer &mappingMatrix)
    {
#ifdef CCL_KERNEL
        auto GetMappingMatrixsWork = [&mappingMatrix, this] (int64_t start, int64_t end) -> HcclResult {
            for (int64_t i = start; i < end; i++) {
                u32 tidIdx = static_cast<u32>(i);
                u32 rowIdx = tidIdx % conMapperNum_;
                u32 colIdx = tidIdx / conMapperNum_;

                u32 offset = 0;
                EXECEPTION_CATCH(offset = mappers_.at(rowIdx).GetUniqueIdOffset(colIdx), return HCCL_E_NOT_FOUND);

                EXECEPTION_CATCH(CHK_RET(mappers_.at(rowIdx).GetUniqueMapper(colIdx).SplitDataGetMappingMatrix(
                    mappingMatrix, offset)), return HCCL_E_NOT_FOUND);
            }

            return HCCL_SUCCESS;
        };

        ParallelFor(conMapperNum_ * parallelNum_, PARALLEL_UNIT_SIZE, GetMappingMatrixsWork);
#endif
        return HCCL_SUCCESS;
    }

    // 方案A.1
    HcclResult ParallelSplitDataAndGenerateMappingTableAndMatrix(MtxBuffer &mappingMatrix)
    {
#ifdef CCL_KERNEL
        auto generateMappingTableWork = [&mappingMatrix, this] (int64_t start, int64_t end) -> HcclResult {
            for (int64_t i = start; i < end; i++) {
                u32 tidIdx = static_cast<u32>(i);

                u32 rowIdx = tidIdx % conMapperNum_;
                u32 colIdx = tidIdx / conMapperNum_;

                EXECEPTION_CATCH(CHK_RET(mappers_.at(rowIdx).GetUniqueMapper(colIdx). \
                    SplitDataAndGenerateMappingTableAndMatrix(mappingMatrix)), return HCCL_E_NOT_FOUND);
            }

            return HCCL_SUCCESS;
        };

        ParallelFor(conMapperNum_ * parallelNum_, PARALLEL_UNIT_SIZE, generateMappingTableWork);
#endif
        return HCCL_SUCCESS;
    }

    // 方案A.2
    HcclResult ParallelSplitAddOffsetToMappingMatrix(MtxBuffer &mappingMatrix)
    {
#ifdef CCL_KERNEL
        auto GetMappingMatrixsWork = [&mappingMatrix, this] (int64_t start, int64_t end) -> HcclResult {
            for (int64_t i = start; i < end; i++) {
                u32 tidIdx = static_cast<u32>(i);

                u32 rowIdx = tidIdx % conMapperNum_;
                u32 colIdx = tidIdx / conMapperNum_;

                u32 offset = 0;
                EXECEPTION_CATCH(offset = mappers_.at(rowIdx).GetUniqueIdOffset(colIdx), return HCCL_E_NOT_FOUND);

                EXECEPTION_CATCH(CHK_RET(mappers_.at(rowIdx).GetUniqueMapper(colIdx). \
                    SplitAddOffsetToMappingMatrix(mappingMatrix, \
                    offset)), return HCCL_E_NOT_FOUND);
            }

            return HCCL_SUCCESS;
        };

        ParallelFor(conMapperNum_ * parallelNum_, PARALLEL_UNIT_SIZE, GetMappingMatrixsWork);
#endif
        return HCCL_SUCCESS;
    }

    // keyCounter方案A.1
    HcclResult ParallelSplitDataAndGenerateMappingTableAndMatrixWithCounter(MtxBuffer &mappingMatrix)
    {
#ifdef CCL_KERNEL
        auto generateMappingTableWork = [&mappingMatrix, this] (int64_t start, int64_t end) -> HcclResult {
            for (int64_t i = start; i < end; i++) {
                u32 tidIdx = static_cast<u32>(i);

                u32 rowIdx = tidIdx % conMapperNum_;
                u32 colIdx = tidIdx / conMapperNum_;

                EXECEPTION_CATCH(CHK_RET(mappers_.at(rowIdx).GetUniqueMapper(colIdx). \
                    SplitDataAndGenerateMappingTableAndMatrixWithCounter(mappingMatrix)), \
                    return HCCL_E_NOT_FOUND);
            }

            return HCCL_SUCCESS;
        };

        ParallelFor(conMapperNum_ * parallelNum_, PARALLEL_UNIT_SIZE, generateMappingTableWork);
#endif
        return HCCL_SUCCESS;
    }

    // 在ParallelGenMappingTables后且parallelNum_大于PARALLEL_UNIT_SIZE时调用
    HcclResult ParallelCollectUniqueSizeInfo()
    {
#ifdef CCL_KERNEL
        auto CollectUniqueSizeInfoWork = [this] (int64_t start, int64_t end) -> HcclResult {
            for (int64_t i = start; i < end; i++) {
                u32 idx = static_cast<u32>(i);

                EXECEPTION_CATCH(CHK_RET(mappers_.at(idx).CollectUniqueSizeInfo()), return HCCL_E_NOT_FOUND);
            }

            return HCCL_SUCCESS;
        };

        ParallelFor(conMapperNum_, PARALLEL_UNIT_SIZE, CollectUniqueSizeInfoWork);
#endif
        return HCCL_SUCCESS;
    }

    HcclResult ParallelGetMappingMatrixs(MtxBuffer &mappingMatrix)
    {
#ifdef CCL_KERNEL
        auto GetMappingMatrixsWork = [&mappingMatrix, this] (int64_t start, int64_t end) -> HcclResult {
            for (int64_t i = start; i < end; i++) {
                u32 tidIdx = static_cast<u32>(i);

                u32 rowIdx = tidIdx % conMapperNum_;
                u32 colIdx = tidIdx / conMapperNum_;

                u32 offset = 0;
                EXECEPTION_CATCH(offset = mappers_.at(rowIdx).GetUniqueIdOffset(colIdx), return HCCL_E_NOT_FOUND);

                EXECEPTION_CATCH(CHK_RET(mappers_.at(rowIdx).GetUniqueMapper(colIdx). \
                    SplitGetMappingMatrix(mappingMatrix, offset)), return HCCL_E_NOT_FOUND);
            }

            return HCCL_SUCCESS;
        };

        ParallelFor(conMapperNum_ * parallelNum_, PARALLEL_UNIT_SIZE, GetMappingMatrixsWork);
#endif
        return HCCL_SUCCESS;
    }

    HcclResult ParallelReduceByMappingMatrix(const MtxBuffer &mappingMatrix)
    {
#ifdef CCL_KERNEL
        auto reduceByMappingMatrixWork = [&mappingMatrix, this] (int64_t start, int64_t end) -> HcclResult {
            for (int64_t i = start; i < end; i++) {
                u32 tidIdx = static_cast<u32>(i);
                // conMapperNum_大于等于1，故无除0风险
                u32 rowIdx = tidIdx % conMapperNum_;
                u32 colIdx = tidIdx / conMapperNum_;

                EXECEPTION_CATCH(CHK_RET(mappers_.at(rowIdx).GetUniqueMapper(colIdx).
                    SplitReduceByMappingMatrix(mappingMatrix)), return HCCL_E_NOT_FOUND);
            }

            return HCCL_SUCCESS;
        };

        ParallelFor(conMapperNum_ * parallelNum_, PARALLEL_UNIT_SIZE, reduceByMappingMatrixWork);
#endif
        return HCCL_SUCCESS;
    }

    // keyCounter
    HcclResult ParallelReduceByMappingMatrixWithCounter(const MtxBuffer &mappingMatrix)
    {
#ifdef CCL_KERNEL
        auto reduceByMappingMatrixWork = [&mappingMatrix, this] (int64_t start, int64_t end) -> HcclResult {
            for (int64_t i = start; i < end; i++) {
                u32 tidIdx = static_cast<u32>(i);
                // conMapperNum_大于等于1，故无除0风险
                u32 rowIdx = tidIdx % conMapperNum_;
                u32 colIdx = tidIdx / conMapperNum_;

                EXECEPTION_CATCH(CHK_RET(mappers_.at(rowIdx).GetUniqueMapper(colIdx).
                    SplitReduceByMappingMatrixWithCounter(mappingMatrix)), return HCCL_E_NOT_FOUND);
            }

            return HCCL_SUCCESS;
        };

        ParallelFor(conMapperNum_ * parallelNum_, PARALLEL_UNIT_SIZE, reduceByMappingMatrixWork);
#endif
        return HCCL_SUCCESS;
    }

    HcclResult ParallelReduceByMappingMatrixWithNoSplit(const MtxBuffer &mappingMatrix)
    {
#ifdef CCL_KERNEL
        auto reduceByMappingMatrixWork = [&mappingMatrix, this] (int64_t start, int64_t end) -> HcclResult {
            for (int64_t i = start; i < end; i++) {
                u32 tidIdx = static_cast<u32>(i);
                // conMapperNum_大于等于1，故无除0风险
                u32 rowIdx = tidIdx % conMapperNum_;
                u32 colIdx = tidIdx / conMapperNum_;

                EXECEPTION_CATCH(CHK_RET(mappers_.at(rowIdx).GetUniqueMapper(colIdx).
                    ReduceByMappingMatrix(mappingMatrix)), return HCCL_E_NOT_FOUND);
            }

            return HCCL_SUCCESS;
        };

        ParallelFor(conMapperNum_ * parallelNum_, PARALLEL_UNIT_SIZE, reduceByMappingMatrixWork);
#endif
        return HCCL_SUCCESS;
    }

    HcclResult ClearData()
    {
#ifdef CCL_KERNEL
        auto clearDataWork = [this] (int64_t start, int64_t end) -> HcclResult {
            for (int64_t i = start; i < end; i++) {
                u32 tidIdx = static_cast<u32>(i);
                // conMapperNum_大于等于1，故无除0风险
                u32 rowIdx = tidIdx % conMapperNum_;
                u32 colIdx = tidIdx / conMapperNum_;

                EXECEPTION_CATCH(mappers_.at(rowIdx).GetUniqueMapper(colIdx).ClearData(),
                    return HCCL_E_NOT_FOUND);
            }

            return HCCL_SUCCESS;
        };

        // 当前无溢出风险
        u32 total = conMapperNum_ * parallelNum_;
        if (total > 0) {
            ParallelFor(total, PARALLEL_UNIT_SIZE, clearDataWork);
            HCCL_DEBUG("conMapperNum_[%u], parallelNum_[%u]", conMapperNum_, parallelNum_);
        } else {
            HCCL_DEBUG("conMapperNum[%u] * parallelNum[%u] is 0, no need ParallelFor", conMapperNum_, parallelNum_);
        }
#endif

        for (u32 i = 0; i < conMapperNum_; i++) {
            EXECEPTION_CATCH(mappers_.at(i).ClearData(), return HCCL_E_NOT_FOUND);
        }

        return HCCL_SUCCESS;
    }

    // 均分方式并行Reduce，仅限T为基本类型时候使用
    static HcclResult ParallelReduceByMappingMatrixEqually(T *dst, T *src, TM *mapMtx, u64 size, u32 parallelNum)
    {
#ifdef CCL_KERNEL
        CHK_PRT_RET(UNLIKELY(size == 0), HCCL_WARNING("size is 0"), HCCL_SUCCESS);
        CHK_PRT_RET(UNLIKELY((size >= INT64_MAX) || (parallelNum == 0)),
            HCCL_ERROR("size[%llu] >= INT64_MAX or parallelNum[%u] == 0, does not support", size, parallelNum),
            HCCL_E_PARA);

        int64_t totalSize = static_cast<int64_t>(size);
        int64_t unitSize = static_cast<int64_t>(size / parallelNum);
        if ((size % parallelNum) != 0) {
            unitSize++;
        }

        ParallelFor(totalSize, unitSize, [&dst, &src, &mapMtx] (int64_t start, int64_t end) -> HcclResult {
            for (int64_t idx = start; idx < end; idx++) {
                TM &uniqueIdx = mapMtx[idx];
                // 由于重复率较高，且内存读性能大于写性能，为了更高的性能，故先判断满足条件再写内存。同时避免缓存乒乓
                if (dst[uniqueIdx] != src[idx]) {
                    dst[uniqueIdx] = src[idx];
                }
            }

            return HCCL_SUCCESS;
        });
#endif

        return HCCL_SUCCESS;
    }

    // 均分方式并行Reduce，仅限T为基本类型时候使用
    static HcclResult ParallelConvertMappingMatrixEqually(TM *secondSrcMapMtx, s64 secondSrcMapMtxNum,
        TM *srcMapMtx, s64 size, TM *dstMapMtx, u32 parallelNum)
    {
#ifdef CCL_KERNEL
        CHK_PRT_RET((size == 0), HCCL_WARNING("size is 0"), HCCL_SUCCESS);
        CHK_PRT_RET((parallelNum == 0),
            HCCL_ERROR("parallelNum[%u] == 0, does not support", parallelNum), HCCL_E_PARA);

        int64_t totalSize = size;
        int64_t unitSize = size / parallelNum;
        if ((size % parallelNum) != 0) {
            unitSize++;
        }

        ParallelFor(totalSize, unitSize,
            [&secondSrcMapMtxNum, &secondSrcMapMtx, &srcMapMtx, &dstMapMtx] (int64_t start, int64_t end) -> HcclResult {
            for (int64_t idx = start; idx < end; idx++) {
                TM &uniqueIdx = srcMapMtx[idx];
                CHK_PRT_RET((uniqueIdx >= static_cast<TM>(secondSrcMapMtxNum)),
                    HCCL_ERROR("uniqueIdx[%d] >= secondSrcMapMtxNum[%lld]", uniqueIdx, secondSrcMapMtxNum),
                    HCCL_E_PARA);
                dstMapMtx[idx] = secondSrcMapMtx[uniqueIdx];
            }

            return HCCL_SUCCESS;
        });
#endif

        return HCCL_SUCCESS;
    }

    std::vector<ConcurrentUniqueMapper<T, TM>> &GetMappers()
    {
        return mappers_;
    }

    std::vector<ConcurrentUniqueMapper<T, TM>> mappers_{};

private:
    static constexpr u32 CON_MAPPER_NUM_ONE = 1;
    static constexpr u32 PARALLEL_NUM_ONE = 1;
    static constexpr u32 PARALLEL_UNIT_SIZE = 1;
    static constexpr u32 PS_BUFF_INFO_ELE_NUM = sizeof(PsBufferInfo) / sizeof(s64);

    u32 conMapperNum_{};
    u32 parallelNum_{};
    bool splitData_{};
    bool enableKeyCounter_{};
};
}
#endif // CON_UNIQUE_MAPPERS_MGR
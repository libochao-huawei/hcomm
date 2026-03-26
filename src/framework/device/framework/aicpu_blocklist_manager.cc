/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "aicpu_blocklist_manager.h"

#include "driver/ascend_hal_define.h"
#include "hccl_common.h"
#include "log.h"

namespace hccl {
    // struct StreamDispatchInfo

    AicpuBlocklistManager::StreamDispatchInfo::StreamDispatchInfo(const uint32_t curSqDepth)
        : sqDepth(curSqDepth), beginSqTail(0), totalSqeCount(0), firstFlipPlaceholderSqIdx(-1) {}
    
    AicpuBlocklistManager::StreamDispatchInfo::StreamDispatchInfo(const StreamDispatchInfo& other)
        : sqDepth(other.sqDepth), beginSqTail(other.beginSqTail), totalSqeCount(other.totalSqeCount),
        firstFlipPlaceholderSqIdx(other.firstFlipPlaceholderSqIdx) {}

    AicpuBlocklistManager::StreamDispatchInfo::~StreamDispatchInfo() {}

    // class AicpuBlocklistManager

    AicpuBlocklistManager::AicpuBlocklistManager() {}

    AicpuBlocklistManager::~AicpuBlocklistManager() {}

    HcclResult AicpuBlocklistManager::InitBlocklistManager(Stream& mainStream, std::vector<Stream>& slaveStreams)
    {
        exceedSqDepth_ = false;

        // 初始化aicpu main/slave stream的下发信息
        perStreamDispatchInfoMap_.clear();
        const HcclComStreamInfo& mainStreamInfo = mainStream.GetHcclStreamInfo();
        perStreamDispatchInfoMap_.emplace(mainStreamInfo.actualStreamId, StreamDispatchInfo(mainStreamInfo.sqDepth));
        for (size_t i = 0; i < slaveStreams.size(); i++) {
            const HcclComStreamInfo& slaveStreamInfo = slaveStreams[i].GetHcclStreamInfo();
            perStreamDispatchInfoMap_.emplace(slaveStreamInfo.actualStreamId, StreamDispatchInfo(slaveStreamInfo.sqDepth));
        }

        HCCL_INFO("[AicpuBlocklistManager][InitBlocklistManager] perStreamDispatchInfoMap.size[%d]",
            perStreamDispatchInfoMap.size());

        return HCCL_SUCCESS;
    }

    HcclResult AicpuBlocklistManager::EnablePartialOpretry(const std::string& algName, const OpParam &param,
        const HcclTopoInfo& topoinfo, bool& isEnablePartialOpretry) {
        // Part 1: 黑名单过滤inplace场景, inline reduce算子, 以及跨超场景

        // 过滤inplace算子
        bool isInplace = false;
        CHK_RET(IsInplace(param, topoinfo, isInplace));
        if (isInplace) {
            HCCL_INFO("[AicpuBlocklistManager][EnablePartialOpretry] inplace: not support partial opretry");
            isEnablePartialOpretry = false;
            return HCCL_SUCCESS;
        }
        
        // 过滤inline reduce算子
        bool isInlineReduce = IsReduce(param);
        if (isInlineReduce) {
            HCCL_INFO("[AicpuBlocklistManager][EnablePartialOpretry] opType[%u] w/ inline reduce: not support "
                "partial opretry", param.opType);
            isEnablePartialOpretry = false;
            return HCCL_SUCCESS;
        }

        // 过滤跨超场景 (即是否使用RDMA)
        const std::unordered_map<u32, bool>& isUsedRdmaMap = topoinfo.isUsedRdmaMap;
        for (std::unordered_map<u32, bool>::const_iterator map_iter = isUsedRdmaMap.cbegin();
            map_iter != isUsedRdmaMap.end(); ++map_iter) {
            if (map_iter->second) {
                HCCL_INFO("[AicpuBlocklistManager][EnablePartialOpretry] rank[%u] uses RDMA: not support partial opretry",
                    map_iter->first);
                return HCCL_SUCCESS;
            }
        }
        
        // Part 2: 白名单控制影响范围 (暂时只支持alltoall类DirectFullmesh算法, 其他算子算法后续再考虑)

        // 校验alltoall类算子
        const HcclCMDType opType = param.opType;
        bool isValidOp = false;
        if (opType == HcclCMDType::HCCL_CMD_ALLTOALL || opType == HcclCMDType::HCCL_CMD_ALLTOALLV ||
            opType == HcclCMDType::HCCL_CMD_ALLTOALLVC) {
            isValidOp = true;
        }
        if (!isValidOp) {
            HCCL_INFO("[AicpuBlocklistManager][EnablePartialOpretry] opType[%u]: not support partial opretry",
                opType);
            isEnablePartialOpretry = false;
            return HCCL_SUCCESS;
        }

        // 校验DirectFullmesh算法
        bool isValidAlg = false;
        if (algName == "RunAlltoAllDirectFullmesh") {
            isValidAlg = true;
        }
        if (!isValidAlg) {
            HCCL_INFO("[AicpuBlocklistManager][EnablePartialOpretry] algName[%s]: not support partial opretry",
                algName.c_str());
            isEnablePartialOpretry = false;
            return HCCL_SUCCESS;
        }

        // Part 3: 基于当前算子各stream下发的SQE数量, 不考虑超过RTSQ size的情况 (否则RTSQ head/tail无法推导SQE执行情况)
        if (exceedSqDepth_) {
            HCCL_INFO("[AicpuBlocklistManager][EnablePartialOpretry] exceedSqDepth_[%u]: not support partial opretry",
                exceedSqDepth_);
            isEnablePartialOpretry = false;
            return HCCL_SUCCESS;
        }

        isEnablePartialOpretry = true;
        return HCCL_SUCCESS;
    }

    HcclResult AicpuBlocklistManager::ResetAndBackupBeforeUnfold(const uint32_t devId,
        Stream& mainStream, std::vector<Stream>& slaveStreams)
    {
        // Part 1: 重置exceedSqDepth_
        exceedSqDepth_ = false;

        // Part 2: 针对aicpu main stream, 重置下发信息

        // aicpu main stream在InitBlocklistManager时, 一定已经初始化了StreamDispatchInfo
        const int32_t mainStreamId = mainStream.GetHcclStreamInfo().actualStreamId;
        std::unordered_map<int32_t, StreamDispatchInfo>::iterator mainIter = perStreamDispatchInfoMap_.find(mainStreamId);
        CHK_PRT_RET(mainIter == perStreamDispatchInfoMap_.end(),
            HCCL_ERROR("[AicpuBlocklistManager][ResetAndBackupBeforeUnfold] mainStreamId[%d] "
                "not found in perStreamDispatchInfoMap", mainStreamId),
            HCCL_E_INTERNAL);
        StreamDispatchInfo& mainDispatchInfo = mainIter->second;
        
        // 备份当前算子展开前, aicpu main stream的RTSQ tail
        CHK_RET(QuerySqStatusByType(devId, mainStream.sqId(), DRV_SQCQ_PROP_SQ_TAIL, mainDispatchInfo.beginSqTail));

        // 重置totalSqeCount, 用于后续展开时, 统计当前算子在aicpu main stream实际下发的SQE count (含flip placeholder)
        mainDispatchInfo.totalSqeCount = 0;

        // 重置firstFlipPlaceholderSqIdx_
        mainDispatchInfo.firstFlipPlaceholderSqIdx = -1;

        HCCL_INFO("[AicpuBlocklistManager][ResetAndBackupBeforeUnfold] mainStreamId[%d] beginSqTail[%u] "
            "totalSqeCount[%llu] firstFlipPlaceholderSqIdx[%d]",
            mainStreamId, mainDispatchInfo.beginSqTail, mainDispatchInfo.totalSqeCount,
            mainDispatchInfo.firstFlipPlaceholderSqIdx);

        // Part 3: 针对aicpu slave streams, 重置下发信息
        for (size_t i = 0; i < slaveStreams.size(); i++) {
            Stream& slaveStream = slaveStreams[i];

            // aicpu slave stream在InitBlocklistManager时, 一定已经初始化了StreamDispatchInfo
            const int32_t slaveStreamId = slaveStream.GetHcclStreamInfo().actualStreamId;
            std::unordered_map<int32_t, StreamDispatchInfo>::iterator slaveIter = perStreamDispatchInfoMap_.find(slaveStreamId);
            CHK_PRT_RET(slaveIter == perStreamDispatchInfoMap_.end(),
                HCCL_ERROR("[AicpuBlocklistManager][ResetAndBackupBeforeUnfold] slaveStreamId[%d] "
                    "not found in perStreamDispatchInfoMap", slaveStreamId),
                HCCL_E_INTERNAL);
            StreamDispatchInfo& slaveDispatchInfo = slaveIter->second;
        
            // 备份当前算子展开前的各stream的RTSQ tail
            CHK_RET(QuerySqStatusByType(devId, slaveStream.sqId(), DRV_SQCQ_PROP_SQ_TAIL, slaveDispatchInfo.beginSqTail));

            // 重置totalSqeCount, 用于后续展开时, 统计当前算子实际下发的SQE count (含flip placeholder)
            slaveDispatchInfo.totalSqeCount = 0;

            // 重置firstFlipPlaceholderSqIdx_
            slaveDispatchInfo.firstFlipPlaceholderSqIdx = -1;

            HCCL_INFO("[AicpuBlocklistManager][ResetAndBackupBeforeUnfold] slaveStreamId[%d] beginSqTail[%u] "
                "totalSqeCount[%llu] firstFlipPlaceholderSqIdx[%d]",
                slaveStreamId, slaveDispatchInfo.beginSqTail, slaveDispatchInfo.totalSqeCount,
                slaveDispatchInfo.firstFlipPlaceholderSqIdx);
        }

        return HCCL_SUCCESS;
    }

    HcclResult AicpuBlocklistManager::UpdateTotalSqeCount(const int32_t streamId, const uint64_t sqeCount)
    {
        // 判断exceedSqDepth_, 如果为true (即某条流下发的SQE count超过sqDepth), 则一定不会使用局部重执行, 没有必要再更新下发信息
        if (exceedSqDepth_) {
            HCCL_DEBUG("[AicpuBlocklistManager][UpdateTotalSqeCount] exceedSqDepth_[%u]: no need to "
                "update totalSqeCount for streamId[%d] sqeCount[%llu]",
                exceedSqDepth_, streamId, sqeCount);
            return HCCL_SUCCESS;
        }

        // 非aicpu main/slave stream (例如aicpu order stream)
        std::unordered_map<int32_t, StreamDispatchInfo>::iterator mapIter = perStreamDispatchInfoMap_.find(streamId);
        if (mapIter == perStreamDispatchInfoMap_.end()) {
            HCCL_INFO("[AicpuBlocklistManager][UpdateTotalSqeCount] streamId[%d] not found; sqeCount[%llu]",
                streamId, sqeCount);
            return HCCL_SUCCESS;
        }

        // 判断totalSqeCount integer overflow
        if (UNLIKELY(mapIter->second.totalSqeCount + sqeCount < mapIter->second.totalSqeCount)) {
            HCCL_ERROR("[AicpuBlocklistManager][UpdateTotalSqeCount] existing sqeCount[%llu] + current sqeCount[%llu]"
                "overflow, streamId[%d]", mapIter->second.totalSqeCount, sqeCount, streamId);
            return HCCL_E_INTERNAL;
        }

        // 更新totalSqeCount
        mapIter->second.totalSqeCount += sqeCount;

        // 更新exceedSqDepth_ if necessary
        if (mapIter->second.totalSqeCount > mapIter->second.sqDepth) {
            exceedSqDepth_ = true;
            HCCL_INFO("[AicpuBlocklistManager][UpdateTotalSqeCount] totalSqeCount[%llu] exceeds sqDepth[%llu] after "
                "updating totalSqeCount for streamId[%d] sqeCount[%llu]",
                mapIter->second.totalSqeCount, mapIter->second.sqDepth, streamId, sqeCount);
        }

        return HCCL_SUCCESS;
    }

    HcclResult AicpuBlocklistManager::IsInplace(const OpParam &param, const HcclTopoInfo& topoinfo, bool& isInplace)
    {
        // 准备input/output size
        uint64_t inputSize = 0;
        uint64_t outputSize = 0;
        CHK_RET(ParseOpParam(param, topoinfo, inputSize, outputSize));

        // 注意: alltoall/alltoallv/alltoallvc可能存在inputSize/outputSize为0的情况, 导致不分配user input/output
        // 但会使用tinySendRecvMem_更新algResource.paramInput/OutputMem用于建链, 相当于param input/output为同一块内存
        // 参考aicpu_communicator.cc中的SetAlltoAllInputAndOutPutMem
        if (inputSize == 0 && outputSize == 0) {
            isInplace = true;
            HCCL_INFO("[AicpuCacheManager][IsInplace] inputSize[%u] is overlapping with outputSize[%u]",
                inputSize, outputSize);
            return HCCL_SUCCESS;
        }

        if (inputSize == 0 || outputSize == 0) {
            isInplace = false;
            HCCL_INFO("[AicpuCacheManager][IsInplace] inputSize[%u] is not overlapping with outputSize[%u]",
                inputSize, outputSize);
            return HCCL_SUCCESS;
        }

        const uint64_t inputStart = reinterpret_cast<uint64_t>(param.inputPtr);
        const uint64_t inputEnd = inputStart + inputSize - 1;
        const uint64_t outputStart = reinterpret_cast<uint64_t>(param.outputPtr);
        const uint64_t outputEnd = outputStart + outputSize - 1;

        if (inputStart <= outputEnd && outputStart <= inputEnd) {
            isInplace = true;
            HCCL_INFO("[AicpuCacheManager][IsInplace] input[0x%016llx, 0x%016llx] is overlapping with output[0x%016llx, 0x%016llx]",
                inputStart, inputEnd, outputStart, outputEnd);
        } else {
            isInplace = false;
            HCCL_INFO("[AicpuCacheManager][IsInplace] input[0x%016llx, 0x%016llx] is not overlapping with output[0x%016llx, 0x%016llx]",
                inputStart, inputEnd, outputStart, outputEnd);
        }

        return HCCL_SUCCESS;
    }

    bool AicpuBlocklistManager::IsReduce(const OpParam& param)
    {
        const HcclCMDType opType = param.opType;
        return opType == HcclCMDType::HCCL_CMD_ALLREDUCE || opType == HcclCMDType::HCCL_CMD_REDUCE ||
            opType == HcclCMDType::HCCL_CMD_REDUCE_SCATTER || opType == HcclCMDType::HCCL_CMD_REDUCE_SCATTER_V;
    }

    HcclResult AicpuBlocklistManager::ParseOpParam(const OpParam &param, const HcclTopoInfo& topoinfo,
        uint64_t& inputSize, uint64_t& outputSize)
    {
        const HcclCMDType opType = param.opType;
        const uint32_t rankSize = topoinfo.userRankSize;

        // 准备input/output size
        // NOTE: 非V类算子 (DataRes), V类算子 (VDataDes), All2All类算子 (All2AllDataDes), batch类算子 (BatchSendRecvDataDes/BatchWriteDataDes)
        if (opType == HcclCMDType::HCCL_CMD_ALLTOALL) { // alltoall算子
            // 注意: sendType和recvType一定相同
            const HcclDataType sendType = param.All2AllDataDes.sendType;

            // 注意: 对于alltoall算子, inputSize和outputSize一定相同 (但不能直接使用param.input/outputSize, alltoall算子不会设置这两个字段)
            // 注意: outputSize的计算不能使用param.All2AllDataDes.recvCount * rankSize * SIZE_TABLE[recvType]
            // 因为alltoall使用sendCount来表示send/recvCount, 而recvCount本身为0
            inputSize = param.All2AllDataDes.sendCount * rankSize * SIZE_TABLE[sendType];
            outputSize = inputSize;
        } else if (opType == HcclCMDType::HCCL_CMD_ALLTOALLV) { // alltoallv算子
            // 注意: sendType和recvType一定相同
            const HcclDataType sendType = param.All2AllDataDes.sendType;
            const HcclDataType recvType = param.All2AllDataDes.recvType;

            // 注意: 对于alltoallv算子, inputSize和outputSize不一定相同 (但不能直接使用param.input/outputSize, alltoallv算子不会设置这两个字段)
            // 参考coll_all_to_all_v_direct_fullmesh_executor.cc下的CollRunAlltoAllDirectFullmesh::GetLocalSendRecvInfoforAlltoallV
            HCCL_DEBUG("[AicpuBlocklistManager][ParseOpParam] sum %u send/recv counts for input/output size", rankSize);
            for (uint32_t tmpRank = 0; tmpRank < rankSize; ++tmpRank) {
                // curRank发送到tmpRank的数据量
                const uint64_t curSendCounts = *(static_cast<const u64 *>(param.All2AllDataDes.sendCounts) + tmpRank);
                const uint64_t curSendLength = curSendCounts * SIZE_TABLE[sendType];
                inputSize += curSendLength;

                // curRank从tmpRank接收的数据量
                const uint64_t curRecvCounts = *(static_cast<const u64 *>(param.All2AllDataDes.recvCounts) + tmpRank);
                const uint64_t curRecvLength = curRecvCounts * SIZE_TABLE[recvType];
                outputSize += curRecvLength;
            }
        } else if (opType == HcclCMDType::HCCL_CMD_ALLTOALLVC) { // alltoallvc算子
            // 注意: sendType和recvType一定相同
            const HcclDataType sendType = param.All2AllDataDes.sendType;
            const HcclDataType recvType = param.All2AllDataDes.recvType;

            // 注意: 对于alltoallvc算子, inputSize和outputSize不一定相同 (但不能直接使用param.input/outputSize, alltoallvc算子不会设置这两个字段)
            // 参考coll_all_to_all_v_direct_fullmesh_executor.cc下的CollRunAlltoAllDirectFullmesh::GetLocalSendRecvInfoforAlltoallV
            const uint32_t curRank = topoinfo.userRank;
            HCCL_DEBUG("[AicpuBlocklistManager][ParseOpParam] sum %u-size sendCountMatrix for input/output size", rankSize);
            for (uint32_t tmpRank = 0; tmpRank < rankSize; ++tmpRank) {
                // curRank发送到tmpRank的数据量
                const uint64_t curSendCounts = *(static_cast<const u64 *>(param.All2AllDataDes.sendCountMatrix)
                    + curRank * rankSize + tmpRank); // sendCountMatrix[curRank][tmpRank]
                const uint64_t curSendLength = curSendCounts * SIZE_TABLE[sendType];
                inputSize += curSendLength;

                // curRank从tmpRank接收到的数据量
                const uint64_t curRecvCounts = *(static_cast<const u64 *>(param.All2AllDataDes.sendCountMatrix)
                    + tmpRank * topoinfo.userRankSize + curRank); // sendCountMatrix[tmpRank][curRank]
                const uint64_t curRecvLength = curRecvCounts * SIZE_TABLE[recvType];
                outputSize += curRecvLength;
            }
        } else { // 非V类算子
            inputSize = param.inputSize;
            outputSize = param.outputSize;
        }

        HCCL_INFO("[AicpuBlocklistManager][ParseOpParam] opType[%u] rankSize[%u] inputSize[%llu] outputSize[%llu]",
            opType, rankSize, inputSize, outputSize);

        return HCCL_SUCCESS;
    }
}
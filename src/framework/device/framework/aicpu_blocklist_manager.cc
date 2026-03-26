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

#include "log.h"

namespace hccl {
    HcclResult AicpuBlocklistManager::EnablePartialOpretry(const std::string& algName, const OpParam &param,
        const HcclTopoInfo& topoinfo, bool& isEnablePartialOpretry) {
        // Part 1: 黑名单过滤inplace和inline reduce算子

        // 过滤inplace算子
        bool isInplace = false;
        CHK_RET(IsInplace(param, topoinfo, isInplace));
        if (isInplace) {
            HCCL_INFO("[AicpuBlocklistManager][EnablePartialOpretry] inplace is not supported for partial opretry");
            isEnablePartialOpretry = false;
            return HCCL_SUCCESS;
        }
        
        // 过滤inline reduce算子
        bool isInlineReduce = IsReduce(param);
        if (isInlineReduce) {
            HCCL_INFO("[AicpuBlocklistManager][EnablePartialOpretry] opType[%u] w/ inline reduce is not supported "
                "for partial opretry", param.opType);
            isEnablePartialOpretry = false;
            return HCCL_SUCCESS;
        }
        
        // Part 2: 白名单控制影响范围 (暂时只支持alltoall类DirectFullmesh算法, 其他算子算法后续再考虑)

        // 过滤alltoall类算子
        const HcclCMDType opType = param.opType;
        bool isValidOp = false;
        if (opType == HcclCMDType::HCCL_CMD_ALLTOALL || opType == HcclCMDType::HCCL_CMD_ALLTOALLV ||
            opType == HcclCMDType::HCCL_CMD_ALLTOALLVC) {
            isValidOp = true;
        }
        if (!isValidOp) {
            HCCL_INFO("[AicpuBlocklistManager][EnablePartialOpretry] opType[%u] is not supported for partial opretry",
                opType);
            isEnablePartialOpretry = false;
            return HCCL_SUCCESS;
        }

        // 过滤DirectFullmesh算法
        bool isValidAlg = false;
        if (algName == "RunAlltoAllDirectFullmesh") {
            isValidAlg = true;
        }
        if (!isValidAlg) {
            HCCL_INFO("[AicpuBlocklistManager][EnablePartialOpretry] algName[%s] is not supported for partial opretry",
                algName.c_str());
            isEnablePartialOpretry = false;
            return HCCL_SUCCESS;
        }

        isEnablePartialOpretry = true;
        return HCCL_SUCCESS;
    }

    AicpuBlocklistManager::AicpuBlocklistManager() {}

    AicpuBlocklistManager::~AicpuBlocklistManager() {}

    HcclResult AicpuBlocklistManager::InitBlocklistManager()
    {
        streamIdSqeCountMap_.clear();
        streamIdSqeCountMapBackup_.clear();
        return HCCL_SUCCESS;
    }

    HcclResult AicpuBlocklistManager::BackupSqeCounts()
    {
        std::lock_guard<std::mutex> lock(backupMutex_); // 修改时加锁保证原子性
        streamIdSqeCountMapBackup_ = streamIdSqeCountMap_; // Deep copy
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
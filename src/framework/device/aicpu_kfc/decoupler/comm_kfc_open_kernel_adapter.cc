/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "comm_kfc_open_kernel_adapter.h"

#include <algorithm>
#include <limits>
#include <string>
#include <vector>

#include "log.h"
#include "alg_param.h"

extern HcclResult LaunchOpenOpParamDataImpl(std::vector<uint8_t> &opParam);

HcclResult LoadOpenOpParamData(uint64_t opParamKey, std::string &commName, std::vector<uint8_t> &opParam)
{
    if (opParamKey == 0U) {
        HCCL_ERROR("Invalid opParamKey %#llx.", opParamKey);
        return HCCL_E_PARA;
    }

    const auto *param = reinterpret_cast<const ops_hccl::OpParam *>(opParamKey);
    if (param == nullptr) {
        HCCL_ERROR("Invalid opParamKey %#llx.", opParamKey);
        return HCCL_E_PARA;
    }

    const size_t opParamSize = sizeof(ops_hccl::OpParam) + param->varMemSize;
    const auto *begin = reinterpret_cast<const uint8_t *>(param);
    opParam.assign(begin, begin + opParamSize);
    commName.assign(param->commName);
    HCCL_INFO("[MC2_OPEN_DIAG][Load] key %#llx, opParamSize %zu, commName[%s], opType %u, algName[%s], "
              "inputSize %llu, outputSize %llu, varMemSize %llu, count %llu, dataType %u, outputType %u, "
              "strideCount %llu, resCtx %p, ctxSize %llu.",
              static_cast<unsigned long long>(opParamKey), opParamSize, commName.c_str(),
              static_cast<u32>(param->opType), param->algName, static_cast<unsigned long long>(param->inputSize),
              static_cast<unsigned long long>(param->outputSize), static_cast<unsigned long long>(param->varMemSize),
              static_cast<unsigned long long>(param->DataDes.count), static_cast<u32>(param->DataDes.dataType),
              static_cast<u32>(param->DataDes.outputType), static_cast<unsigned long long>(param->DataDes.strideCount),
              param->resCtx, static_cast<unsigned long long>(param->ctxSize));
    return HCCL_SUCCESS;
}

void CreateOpParamByBaseOpParam(const std::vector<uint8_t> &baseOpParam, const HcclApi::HcclMsg &msg,
    HcclApi::HcclMsgExt &extMsg, uint32_t rankNum, void *stream, std::vector<uint8_t> &runOpParam)
{
    const auto *baseParam = reinterpret_cast<const ops_hccl::OpParam *>(baseOpParam.data());
    const size_t opParamSize = sizeof(ops_hccl::OpParam) + baseParam->varMemSize;
    runOpParam.resize(opParamSize);

    auto *param = reinterpret_cast<ops_hccl::OpParam *>(runOpParam.data());
    memcpy_s(param, sizeof(ops_hccl::OpParam), baseParam, sizeof(ops_hccl::OpParam));

    if (baseParam->varMemSize > 0) {
        memcpy_s(param->varData, baseParam->varMemSize, baseParam->varData, baseParam->varMemSize);
    }
    if (param->opType == HcclCMDType::HCCL_CMD_ALLTOALL && msg.strideCount > 0UL) {
        for (uint32_t i = 0U; i < rankNum; ++i) {
            extMsg.sendCounts[i] = extMsg.recvCounts[i] = msg.dataCnt;
            extMsg.sendOffset[i] = extMsg.recvOffset[i] = msg.strideCount * i;
        }
        param->opType = static_cast<HcclCMDType>(HcclCMDType::HCCL_CMD_ALLTOALLV);
    }
    if (param->opType == HcclCMDType::HCCL_CMD_ALLTOALLV) {
        param->all2AllVDataDes.sendType = param->all2AllVDataDes.recvType = static_cast<HcclDataType>(msg.addMsg.v1Msg.hcclDataType);
        param->all2AllVDataDes.sendCounts = reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(extMsg.sendCounts));
        param->all2AllVDataDes.recvCounts = reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(extMsg.recvCounts));
        param->all2AllVDataDes.sdispls = reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(extMsg.sendOffset));
        param->all2AllVDataDes.rdispls = reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(extMsg.recvOffset));
    } else if (param->opType == HcclCMDType::HCCL_CMD_ALLTOALL) {
        param->all2AllDataDes.sendType = param->all2AllDataDes.recvType = static_cast<HcclDataType>(msg.addMsg.v1Msg.hcclDataType);
        param->all2AllDataDes.sendCount = param->all2AllDataDes.recvCount = msg.dataCnt;
    } else {
        param->DataDes.dataType = param->DataDes.outputType = static_cast<HcclDataType>(msg.addMsg.v1Msg.hcclDataType);
        param->DataDes.count = msg.dataCnt;
        param->DataDes.strideCount = msg.strideCount;
    }

    param->opType = static_cast<HcclCMDType>(msg.commType.prepareType);
    param->reduceType = static_cast<HcclReduceOp>(msg.opType);
    param->stream = stream;
}

HcclResult FormatOpenOpParamDataFromMsg(const std::vector<uint8_t> &baseOpParam, const HcclApi::HcclMsg &msg,
    HcclApi::HcclMsgExt &extMsg, uint32_t rankNum, uint32_t repeatIdx, void *stream, std::vector<uint8_t> &runOpParam)
{
    if (baseOpParam.empty()) {
        HCCL_ERROR("Base op param is empty.");
        return HCCL_E_PARA;
    }
    const auto *baseParam = reinterpret_cast<const ops_hccl::OpParam *>(baseOpParam.data());
    if (repeatIdx == 0U) {
        CreateOpParamByBaseOpParam(baseOpParam, msg, extMsg, rankNum, stream, runOpParam);
    }
    auto *param = reinterpret_cast<ops_hccl::OpParam *>(runOpParam.data());

    if ((repeatIdx != 0) && (param->opType == HCCL_CMD_ALLTOALLV)) {
        for (u32 i = 0U; i < rankNum; ++i) {
            extMsg.sendOffset[i] += extMsg.sendCounts[i];
            extMsg.recvOffset[i] += extMsg.recvCounts[i];
            HCCL_INFO("Formatted alltoallv info: repeat %u, rank id %u, send offset %llu, recv offset %llu.", repeatIdx, i,
                    static_cast<u64 *>(param->all2AllVDataDes.sdispls)[i],
                    static_cast<u64 *>(param->all2AllVDataDes.rdispls)[i]);
        }
    }

    // offset 的计算方法？( baseParam->inputSize * repeatIdx ) or ( msg.dataCnt * DataUnitSize(msg.addMsg.v1Msg.hcclDataType) * repeatIdx ) ?
    const u64 offset = param->inputSize * repeatIdx;
    param->inputPtr = reinterpret_cast<void *>(msg.sendBuffer + offset);
    param->outputPtr = reinterpret_cast<void *>(msg.recvBuffer + offset);
    const HcclCMDType opType = static_cast<HcclCMDType>(msg.commType.prepareType);

    HCCL_INFO("[MC2_OPEN_DIAG][FormatMsg] repeatIdx %u, rankNum %u, msgOpType %u, msgReduceType %u, "
              "msgSendBuffer %#llx, msgRecvBuffer %#llx, msgDataCnt %llu, msgDataType %u, msgRepeatCnt %u, "
              "msgStrideCount %llu, ccOpTilingData %#llx.",
              repeatIdx, rankNum, static_cast<u32>(opType), static_cast<u32>(msg.opType),
              static_cast<unsigned long long>(msg.sendBuffer), static_cast<unsigned long long>(msg.recvBuffer),
              static_cast<unsigned long long>(msg.dataCnt), static_cast<u32>(msg.addMsg.v1Msg.hcclDataType),
              static_cast<u32>(msg.addMsg.v1Msg.repeatCnt), static_cast<unsigned long long>(msg.strideCount),
              static_cast<unsigned long long>(msg.addMsg.v1Msg.ccOpTilingData));
    HCCL_INFO("[MC2_OPEN_DIAG][FormatBase] baseOpType %u, baseAlgName[%s], baseInputSize %llu, "
              "baseOutputSize %llu, baseCount %llu, baseDataType %u, baseOutputType %u, baseStrideCount %llu, "
              "baseInputPtr %p, baseOutputPtr %p.",
              static_cast<u32>(baseParam->opType), baseParam->algName,
              static_cast<unsigned long long>(baseParam->inputSize),
              static_cast<unsigned long long>(baseParam->outputSize),
              static_cast<unsigned long long>(baseParam->DataDes.count), static_cast<u32>(baseParam->DataDes.dataType),
              static_cast<u32>(baseParam->DataDes.outputType),
              static_cast<unsigned long long>(baseParam->DataDes.strideCount), baseParam->inputPtr,
              baseParam->outputPtr);
    HCCL_INFO("[MC2_OPEN_DIAG][FormatRun] runOpType %u, runAlgName[%s], offset %llu, runInputPtr %p, "
              "runOutputPtr %p, runCount %llu, runDataType %u, runOutputType %u, runStrideCount %llu, stream %p.",
              static_cast<u32>(param->opType), param->algName, static_cast<unsigned long long>(offset),
              param->inputPtr, param->outputPtr, static_cast<unsigned long long>(param->DataDes.count),
              static_cast<u32>(param->DataDes.dataType), static_cast<u32>(param->DataDes.outputType),
              static_cast<unsigned long long>(param->DataDes.strideCount), param->stream);
    HCCL_INFO("Formatted open op param: repeat index %u, op type %u, input addr %#llx, output addr %#llx.", repeatIdx,
        static_cast<u32>(opType), param->inputPtr, param->outputPtr);

    return HCCL_SUCCESS;
}

HcclResult LaunchOpenOpParamData(std::vector<uint8_t> &opParam)
{
    if (opParam.empty()) {
        HCCL_ERROR("Op param is empty.");
        return HCCL_E_PARA;
    }
    return LaunchOpenOpParamDataImpl(opParam);
}

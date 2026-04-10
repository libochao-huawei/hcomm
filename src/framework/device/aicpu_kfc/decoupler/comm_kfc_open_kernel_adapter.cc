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
    return HCCL_SUCCESS;
}

HcclResult FormatOpenOpParamDataFromMsg(const std::vector<uint8_t> &baseOpParam, const HcclApi::HcclMsg &msg,
    HcclApi::HcclMsgExt &extMsg, uint32_t rankNum, uint32_t repeatIdx, void *stream, std::vector<uint8_t> &runOpParam)
{
    if (baseOpParam.empty()) {
        HCCL_ERROR("Base op param is empty.");
        return HCCL_E_PARA;
    }

    const auto *baseParam = reinterpret_cast<const ops_hccl::OpParam *>(baseOpParam.data());
    const size_t opParamSize = sizeof(ops_hccl::OpParam) + baseParam->varMemSize;
    runOpParam.resize(opParamSize);

    auto *param = reinterpret_cast<ops_hccl::OpParam *>(runOpParam.data());
    memcpy_s(param, sizeof(ops_hccl::OpParam), baseParam, sizeof(ops_hccl::OpParam));

    if (baseParam->varMemSize > 0) {
        memcpy_s(param->varData, baseParam->varMemSize, baseParam->varData, baseParam->varMemSize);
    }

    param->stream = stream;

    const HcclCMDType opType = static_cast<HcclCMDType>(msg.commType.prepareType);
    const u64 offset = baseParam->inputSize * repeatIdx;
    param->inputPtr = reinterpret_cast<void *>(msg.sendBuffer + offset);
    param->outputPtr = reinterpret_cast<void *>(msg.recvBuffer + offset);

    if (opType == HCCL_CMD_ALLTOALLV && repeatIdx > 0) {
        for (u32 i = 0U; i < rankNum; ++i) {
            param->all2AllVDataDes.sdispls
                = reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(param->all2AllVDataDes.sdispls)
                                           + reinterpret_cast<u64 *>(extMsg.sendCounts)[i] * repeatIdx);
            param->all2AllVDataDes.rdispls
                = reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(param->all2AllVDataDes.rdispls)
                                           + reinterpret_cast<u64 *>(extMsg.recvCounts)[i] * repeatIdx);
        }
    }

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

/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef HCCL_API_DATA_AICPU_TS_H
#define HCCL_API_DATA_AICPU_TS_H

bool IsBatchLaunchMode();
HcclResult HandleDispatchAllStreams();

// Convert hccl::HcommDataType => Hccl::DataType, hccl::HcommReduceOp => Hccl::ReduceOp
static const std::unordered_map<HcommReduceOp, Hccl::ReduceOp> mapHcommReduceOpToA5 = {
    {HcommReduceOp::HCOMM_REDUCE_SUM,  Hccl::ReduceOp::SUM},
    {HcommReduceOp::HCOMM_REDUCE_PROD, Hccl::ReduceOp::PROD},
    {HcommReduceOp::HCOMM_REDUCE_MAX,  Hccl::ReduceOp::MAX},
    {HcommReduceOp::HCOMM_REDUCE_MIN,  Hccl::ReduceOp::MIN},
    {HcommReduceOp::HCOMM_REDUCE_RESERVED, Hccl::ReduceOp::INVALID}
};

static const std::unordered_map<HcommDataType, Hccl::DataType> mapHcommDataTypeToA5 = {
    {HcommDataType::HCOMM_DATA_TYPE_INT8,    Hccl::DataType::INT8},
    {HcommDataType::HCOMM_DATA_TYPE_INT16,   Hccl::DataType::INT16},
    {HcommDataType::HCOMM_DATA_TYPE_INT32,   Hccl::DataType::INT32},
    {HcommDataType::HCOMM_DATA_TYPE_FP16,    Hccl::DataType::FP16},
    {HcommDataType::HCOMM_DATA_TYPE_FP32,    Hccl::DataType::FP32},
    {HcommDataType::HCOMM_DATA_TYPE_INT64,   Hccl::DataType::INT64},
    {HcommDataType::HCOMM_DATA_TYPE_UINT64,  Hccl::DataType::UINT64},
    {HcommDataType::HCOMM_DATA_TYPE_UINT8,   Hccl::DataType::UINT8},
    {HcommDataType::HCOMM_DATA_TYPE_UINT16,  Hccl::DataType::UINT16},
    {HcommDataType::HCOMM_DATA_TYPE_UINT32,  Hccl::DataType::UINT32},
    {HcommDataType::HCOMM_DATA_TYPE_FP64,    Hccl::DataType::FP64},
    {HcommDataType::HCOMM_DATA_TYPE_BFP16,   Hccl::DataType::BFP16},
    {HcommDataType::HCOMM_DATA_TYPE_INT128,  Hccl::DataType::INT128},
#ifndef OPEN_BUILD_PROJECT
    {HcommDataType::HCOMM_DATA_TYPE_HIF8,    Hccl::DataType::HIF8},
    {HcommDataType::HCOMM_DATA_TYPE_FP8E4M3, Hccl::DataType::FP8E4M3},
    {HcommDataType::HCOMM_DATA_TYPE_FP8E5M2, Hccl::DataType::FP8E5M2},
    {HcommDataType::HCOMM_DATA_TYPE_FP8E8M0, Hccl::DataType::FP8E8M0},
#endif
    {HcommDataType::HCOMM_DATA_TYPE_RESERVED, Hccl::DataType::INVALID}
};

inline HcclResult CheckDataTypeAndReduceOp(HcommDataType dataType, HcommReduceOp reduceOp)
{
    if (mapHcommDataTypeToA5.find(dataType) == mapHcommDataTypeToA5.end()) {
        HCCL_ERROR("[%s] type[%u] is not supported.", __func__, dataType);
        return HCCL_E_PARA;
    }

    if (mapHcommReduceOpToA5.find(reduceOp) == mapHcommReduceOpToA5.end()) {
        HCCL_ERROR("[%s] op[%u] is not supported.", __func__, reduceOp);
        return HCCL_E_PARA;
    }

    return HCCL_SUCCESS;
}

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus
/**
 * @brief NPU上查询 rtsq任务执行完成的接口（阻塞）
 * @param[in] thread NPU上执行的线程句柄
 * @param[in] timeout 超时时间(秒)
 * @return int32_t 执行结果状态码
 * 
 * WARNING: experimental API, No compatibility is currently guaranteed for this API
 */
extern int32_t HcommThreadJoin(ThreadHandle thread, uint32_t timeout);
#ifdef __cplusplus
}
#endif  // __cplusplus

#endif // HCCL_API_DATA_AICPU_TS_H

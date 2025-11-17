/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */


#ifndef LEGACY_COMMON_H
#define LEGACY_COMMON_H

#include <unordered_map>
#include <string>
#include <mutex>
#include <map>
#include <hccl/hccl_types.h>
#include <hccl/base.h>
#include "acl/acl_rt.h"
#include "rt_external.h"

// 2 is sizeof(float16), 8 is sizeof(float64), 2 is sizeof(bfloat16)..
constexpr u32 LEGACY_SIZE_TABLE[HCCL_DATA_TYPE_RESERVED] = {sizeof(s8), sizeof(s16), sizeof(s32),
    2, sizeof(float), sizeof(s64), sizeof(u64), sizeof(u8), sizeof(u16), sizeof(u32), 8, 2, 16};

// 对内芯片类型
enum class LegacyDevType {
    DEV_TYPE_910 = 0,
    DEV_TYPE_310P3 = 1, // PG
    DEV_TYPE_910B = 2,
    DEV_TYPE_310P1 = 3, // AG
    DEV_TYPE_910_93 = 4,
    DEV_TYPE_NOSOC = 5,
    DEV_TYPE_910_95 = 6,
    DEV_TYPE_COUNT = 7
};

const std::unordered_map<std::string, LegacyDevType> LEGACY_SOC_VER_CONVERT{
    {"Ascend310P1", LegacyDevType::DEV_TYPE_310P3},
    {"Ascend310P3", LegacyDevType::DEV_TYPE_310P3},
    {"Ascend310P5", LegacyDevType::DEV_TYPE_310P3},
    {"Ascend310P7", LegacyDevType::DEV_TYPE_310P3},
    {"Ascend310B1", LegacyDevType::DEV_TYPE_310P3},  // 临时映射，临时当前Ascend310B1 torch_npu未与hccl的so解耦；计划20250630完成解耦，解耦后删除
    {"Ascend910", LegacyDevType::DEV_TYPE_910},
    {"Ascend910A", LegacyDevType::DEV_TYPE_910},
    {"Ascend910B", LegacyDevType::DEV_TYPE_910},
    {"Ascend910ProA", LegacyDevType::DEV_TYPE_910},
    {"Ascend910ProB", LegacyDevType::DEV_TYPE_910},
    {"Ascend910PremiumA", LegacyDevType::DEV_TYPE_910},
    {"Ascend910B1", LegacyDevType::DEV_TYPE_910B},
    {"Ascend910B2", LegacyDevType::DEV_TYPE_910B},
    {"Ascend910B2C", LegacyDevType::DEV_TYPE_910B},
    {"Ascend910B3", LegacyDevType::DEV_TYPE_910B},
    {"Ascend910B4", LegacyDevType::DEV_TYPE_910B},
    {"Ascend910B4-1", LegacyDevType::DEV_TYPE_910B},
    {"Ascend910_9391", LegacyDevType::DEV_TYPE_910_93},
    {"Ascend910_9381", LegacyDevType::DEV_TYPE_910_93},
    {"Ascend910_9392", LegacyDevType::DEV_TYPE_910_93},   // Ascend910_9392、Ascend910_9382为预留类型，当前版本暂不支持，待跟随后续版本节奏交付
    {"Ascend910_9382", LegacyDevType::DEV_TYPE_910_93},
    {"Ascend910_9372", LegacyDevType::DEV_TYPE_910_93},
    {"Ascend910_9362", LegacyDevType::DEV_TYPE_910_93},
    {"nosoc", LegacyDevType::DEV_TYPE_NOSOC}};

const std::map<HcclDataType, std::string> LEGACY_DATA_TYPE_STR_MAP{
    {HcclDataType::HCCL_DATA_TYPE_INT8, "int8"},
    {HcclDataType::HCCL_DATA_TYPE_INT16, "int16"},
    {HcclDataType::HCCL_DATA_TYPE_INT32, "int32"},
    {HcclDataType::HCCL_DATA_TYPE_INT64, "int64"},
    {HcclDataType::HCCL_DATA_TYPE_UINT64, "uint64"},
    {HcclDataType::HCCL_DATA_TYPE_FP16, "float16"},
    {HcclDataType::HCCL_DATA_TYPE_FP32, "float32"},
    {HcclDataType::HCCL_DATA_TYPE_UINT8, "uint8"},
    {HcclDataType::HCCL_DATA_TYPE_UINT16, "uint16"},
    {HcclDataType::HCCL_DATA_TYPE_UINT32, "uint32"},
    {HcclDataType::HCCL_DATA_TYPE_FP64, "float64"},
    {HcclDataType::HCCL_DATA_TYPE_BFP16, "bfloat16"},
    {HcclDataType::HCCL_DATA_TYPE_INT128, "int128"},
    {HcclDataType::HCCL_DATA_TYPE_RESERVED, "reserved"}
};

const std::map<HcclReduceOp, std::string> LEGACY_REDUCE_OP_STR_MAP{
    {HcclReduceOp::HCCL_REDUCE_SUM, "sum"},
    {HcclReduceOp::HCCL_REDUCE_PROD, "prod"},
    {HcclReduceOp::HCCL_REDUCE_MAX, "max"},
    {HcclReduceOp::HCCL_REDUCE_MIN, "min"},
    {HcclReduceOp::HCCL_REDUCE_RESERVED, "reserved"}
};

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

extern const char *GetSocVer();
extern HcclResult GetDeviceType(LegacyDevType &deviceType);
extern HcclResult Malloc(void **devPtr, u64 size);
extern HcclResult Free(void *devPtr);
extern HcclResult TbeMemcpy(void *dst, uint64_t destMax, const void *src, uint64_t count, aclrtMemcpyKind kind);
extern HcclResult GetDevice(s32 *deviceLogicId);
extern HcclResult GetDeviceInfo(u32 deviceId, aclrtDevAttr attr, int64_t *value);
extern HcclResult GetDevArgsAddr(rtStream_t stream, rtArgsEx_t *argsInfo, void **devArgsAddr, void **argsHandle);
extern HcclResult BinaryGetFunction(const aclrtBinHandle binHandle, const char *kernelName, aclrtFuncHandle *funcHandle);
extern HcclResult GetFunctionAddr(aclrtFuncHandle funcHandle, void **aicAddr, void **aivAddr);
extern HcclResult BinaryUnLoad(aclrtBinHandle binHandle);
extern HcclResult BinaryLoadFromData(const void *data, size_t length, const aclrtBinaryLoadOptions *options,
    aclrtBinHandle *binHandle);
extern HcclResult KernelArgsInit(aclrtFuncHandle funcHandle, aclrtArgsHandle *argsHandle);
extern HcclResult KernelArgsFinalize(aclrtArgsHandle argsHandle);
extern HcclResult KernelArgsAppend(aclrtArgsHandle argsHandle, void *param, size_t paramSize, aclrtParamHandle *paramHandle);
extern HcclResult LaunchKernelWithConfig(aclrtFuncHandle funcHandle, uint32_t blockDim,
        aclrtStream stream, aclrtLaunchKernelCfg *cfg, aclrtArgsHandle argsHandle, void *reserve);
extern HcclResult NotifyGetAddr(void *signal, u64 *notifyAddr);
extern HcclResult GetNotifyID(void *signal, u32 *notifyID);
extern HcclResult FftsPlusTaskLaunchWithFlag(rtFftsPlusTaskInfo_t *fftsPlusTaskInfo, rtStream_t stm, u32 flag);
extern HcclResult LegacyParseDebugConfig();
extern const u64& GetExterInputDebugConfigLegacy();
extern std::string LegacyGetDataTypeEnumStr(HcclDataType dataType);
extern std::string LegacyGetReduceOpEnumStr(HcclReduceOp reduceOp);
extern HcclResult GetRdmaDoorbellAddr(u32 dbIndex, u64 &dbAddr);

#ifdef __cplusplus
}
#endif // __cplusplus
#endif  // LEGACY_COMMON_H
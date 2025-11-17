/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef TBE_VECTOR_REDUCE_H
#define TBE_VECTOR_REDUCE_H

#include <string>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include <hccl/hccl_types.h>
#include <mutex>
#include <map>
#include <list>
#include <hccl/base.h>
#include "op_tiling.h"
#include "exe_graph/runtime/tiling_context.h"
#include "exe_graph/runtime/tiling_parse_context.h"
#include "acl/acl_rt.h"
#include "legacy_common.h"

namespace TbeReduce {
constexpr u32 TILING_DATA_MAX_SZIE = 32;
constexpr u32 CHIP_VERSION_MAX_LEN = 32;
constexpr size_t INPUT_NUM = 2;
constexpr size_t OUTPUT_NUM = 1;

using BinHandle = void *;
using HcclRtStream = void *;

struct TilingInputInfo {
    HcclDataType dataType;
    HcclReduceOp redOp;
    u64 count;
    bool operator == (const TilingInputInfo& other) const
    {
        return dataType == other.dataType && redOp == other.redOp && count && other.count;
    }
};

struct TilingHashFuc {
    std::size_t operator()(const TilingInputInfo& key) const
    {
        return std::hash<u64>()(key.count) ^ std::hash<s32>()(key.dataType) ^ std::hash<s32>()(key.redOp);
    }
};

const std::map<std::string, std::string> OPINFO_INDEX_SOC_STRING_MAP = {
    {"Ascend910B1", "_910B1"},
    {"Ascend910B2", "_910B2"},
    {"Ascend910B2C", "_910B2"},
    {"Ascend910B3", "_910B3"},
    {"Ascend910B4", "_910B4"},
    {"Ascend910B4-1", "_910B4-1"},
    {"Ascend910_9391", "_910B1"},
    {"Ascend910_9381", "_910B2"},
    {"Ascend910_9392", "_910B1"},   // Ascend910_9392、Ascend910_9382为预留类型，当前版本暂不支持，待跟随后续版本节奏交付
    {"Ascend910_9382", "_910B2"},
    {"Ascend910_9372", "_910B3"},
    {"Ascend910_9362", "_910B4-1"}
};

class TbeVectorReduce {
public:
    explicit TbeVectorReduce();
    virtual ~TbeVectorReduce();
    HcclResult Init();
    HcclResult DeInit();
    HcclResult Run(const void *src1, const void *src2, u64 count, const HcclDataType dataType,
                               HcclReduceOp redOp, void *stream, const void *dst);
    HcclResult SetGlobalWorkSpace(const std::vector<void *> &globalWorkSpaceAddr);
    HcclResult PrepareVectorReduce(const void *src1, const void *src2, u64 count, const HcclDataType dataType,
        HcclReduceOp redOp, const void *dst, void *stream, void *&addrListDevMemPtr, void *&argsHandle,
        void *&stubFuncAddr, uint32_t &blockDim);
    HcclResult GetVectorBlockSize(u32& blockSize);
    HcclResult GetOpProperty(nlohmann::json &opInfo, const std::string propName, std::string &propValue);
    HcclResult GetOpProperty(nlohmann::json &opInfo, const std::string propName, u32 &propValue);
    HcclResult LoadOpBinary();
    HcclResult GetTilingDataDevMem(OpRunInfo &runInfo, void **tilingDataDevPtr,
        const u32 tilingDataMaxSize);
    HcclResult GetKernelFunctionAndArgs(std::vector<const void *> &deviceAddrs, std::string &kernelName,
        aclrtBinHandle binHandle, aclrtFuncHandle &funcHandle, aclrtArgsHandle &argsHandle);

protected:
    char *tilingDataHostPtr_;
    std::vector<char *> binaryDataPtrVec_;
    std::map<std::string, aclrtBinHandle> binaryLoadMap_;
    std::mutex stubFuncMutex_;
    std::mutex tilingDataMutex_;
    std::map<std::string, s8 *> opNameStubFuncsMap_;
    LegacyDevType deviceType_;
    std::atomic<bool> isInit_{false};
    std::atomic<bool> isDeInit_{false};

private:
    void InitOpInfoMap310P3();
    void InitOpInfoMap910A();
    void InitOpInfoMap910B();
    HcclResult GetOpBinaryPath(std::string &binaryPath);
    HcclResult LoadOpBinaryWithParams(std::string &loadBinaryFile, const void *&binaryData, uint64_t &binaryLength);
    HcclResult GenerateStubFunc(s8 *&stubFunc, std::string &kernelName);
    HcclResult VectorReduce(const void *src1, const void *src2, u64 count, const HcclDataType dataType,
                            HcclReduceOp redOp, void *stream, const void *dst);
    HcclResult VectorReduceLoop(const void *src1, const void *src2, u64 count, const HcclDataType dataType,
                            HcclReduceOp redOp, void *stream, const void *dst);
    HcclResult GetTilingRunInfo(nlohmann::json &opDescInfo, nlohmann::json &opTilingInfo, u64 count,
                                HcclDataType dataType, OpRunInfo &runInfo, HcclReduceOp redOp = HCCL_REDUCE_RESERVED);
    HcclResult RegisterMetadata(nlohmann::json &opInfo);
    bool IsSupportOperatorOverflowDevType910A(HcclDataType dataType, HcclReduceOp redOp) const;
    bool IsSupportOperatorDevType910A(HcclDataType dataType, HcclReduceOp redOp) const;
    HcclResult GetKernelName(nlohmann::json &opInfo, HcclDataType dataType, std::string &kernelName,
                             OpRunInfo& runInfo, HcclReduceOp redOp = HCCL_REDUCE_RESERVED);
    HcclResult GetOpInfo(HcclDataType dataType, HcclReduceOp redOp, nlohmann::json &opDescInfo,
                         nlohmann::json &opTilingInfo);
    HcclResult GetOpInfoIndex(std::string &opInfoIndex, HcclDataType dataType);
    HcclResult GetOpInfoIndex(std::string &opInfoIndex, HcclReduceOp redOp);
    HcclResult GetOpInfoIndex(std::string &opInfoIndex, bool isAddSocName = true);
    HcclResult GetOpInfoIndex(std::string &opInfoIndex, HcclDataType dataType, HcclReduceOp redOp);
    HcclResult GetTilingDataInfo(const u64 count, const HcclDataType dataType,
        const HcclReduceOp redOp, void*& tilingDataDevPtr, nlohmann::json& opDescInfo, OpRunInfo& runInfo);
    HcclResult ConcatMem(const void *src1, const void *src2, const void *dst, const void *tilingDataDevPtr,
        std::vector<const void *> &deviceAddrs);
    HcclResult GetTilingRunInfo(nlohmann::json &opDescInfo, u64 count,
        HcclDataType dataType, OpRunInfo &runInfo, HcclReduceOp redOp = HCCL_REDUCE_RESERVED);
    HcclResult GetOpInfo(HcclDataType dataType, HcclReduceOp redOp, nlohmann::json &opDescInfo);
    HcclResult GetCoreNum();
    HcclResult ConvertToOpRunInfo(gert::TilingContext *context, OpRunInfo &runInfo);
    HcclResult GetBinaryName(std::string &binaryName, HcclDataType dataType, HcclReduceOp redOp);
    std::map<std::string, nlohmann::json> opInfoMap_;
    std::unordered_map<TilingInputInfo, void *, TilingHashFuc> tilingDataMap_;
    std::mutex globalWorkSpaceAddrMutex_;
    std::vector<void *> globalWorkSpaceAddr_;
};
} // namespace TbeReduce

#endif

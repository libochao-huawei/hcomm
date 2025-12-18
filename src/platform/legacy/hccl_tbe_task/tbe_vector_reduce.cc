/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "tbe_vector_reduce.h"
#include <fstream>
#include <dlfcn.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <iomanip>
#include "ascend_hal.h"
#include "op_json_info.h"
#include "vector_tiling.h"
#include "mmpa_api.h"
#include "vector_tiling_rt2.h"
#include "hccl_tbe_task.h"
#include "base/context_builder/op_tiling_parse_context_builder.h"
#include "base/context_builder/op_tiling_context_builder.h"
#include "exe_graph/runtime/storage_shape.h"

#if 0
namespace ge {
REG_OP(Add)
    .INPUT(x1, TensorType({ DT_INT64 }))
    .INPUT(x2, TensorType({ DT_INT64 }))
    .OUTPUT(y, TensorType({ DT_INT64 }))
    .OP_END_FACTORY_REG(Add);

REG_OP(Mul)
    .INPUT(x1, TensorType({ DT_INT64 }))
    .INPUT(x2, TensorType({ DT_INT64 }))
    .OUTPUT(y, TensorType({ DT_INT64 }))
    .OP_END_FACTORY_REG(Mul);

REG_OP(Maximum)
    .INPUT(x1, TensorType({ DT_INT64 }))
    .INPUT(x2, TensorType({ DT_INT64 }))
    .OUTPUT(y, TensorType({ DT_INT64 }))
    .OP_END_FACTORY_REG(Maximum);

REG_OP(Minimum)
    .INPUT(x1, TensorType({ DT_INT64 }))
    .INPUT(x2, TensorType({ DT_INT64 }))
    .OUTPUT(y, TensorType({ DT_INT64 }))
    .OP_END_FACTORY_REG(Minimum);
}
#endif

namespace TbeReduce {
using namespace std;

constexpr u32 UB_BLOCK_SIZE = 32;
constexpr u64 TBE_REDUCE_MAX_COUNT = INT32_MAX;
constexpr u32 kMAX_TILING_DATA_SIZE = 16UL * 1024UL;
constexpr u32 kMAX_WORKSPACE_COUNT = 16;
fe::PlatFormInfos g_platFormInfos;
std::mutex g_accessFilesMutex;
std::map<HcclReduceOp, std::string> OPERATOR_MAP = { { HCCL_REDUCE_SUM, "Add" },
                                                     { HCCL_REDUCE_PROD, "Mul" },
                                                     { HCCL_REDUCE_MAX, "Maximum" },
                                                     { HCCL_REDUCE_MIN, "Minimum" } };

std::map<HcclDataType, ge::DataType> CONVERT_DATATYPE_MAP = {
    { HCCL_DATA_TYPE_INT8, ge::DataType::DT_INT8 },     { HCCL_DATA_TYPE_INT16, ge::DataType::DT_INT16 },
    { HCCL_DATA_TYPE_INT32, ge::DataType::DT_INT32 },   { HCCL_DATA_TYPE_FP16, ge::DataType::DT_FLOAT16 },
    { HCCL_DATA_TYPE_INT64, ge::DataType::DT_INT64 },   { HCCL_DATA_TYPE_UINT64, ge::DataType::DT_UINT64 },
    { HCCL_DATA_TYPE_UINT8, ge::DataType::DT_UINT8 },   { HCCL_DATA_TYPE_UINT16, ge::DataType::DT_UINT16 },
    { HCCL_DATA_TYPE_UINT32, ge::DataType::DT_UINT32 }, { HCCL_DATA_TYPE_BFP16, ge::DataType::DT_BF16 }
};

TbeVectorReduce::TbeVectorReduce()
    : tilingDataHostPtr_(nullptr),
      deviceType_(LegacyDevType::DEV_TYPE_910)
{}

TbeVectorReduce::~TbeVectorReduce()
{
    if (!isDeInit_) {
        DeInit();
    }
}

HcclResult TbeVectorReduce::Init()
{
    std::unique_lock<std::mutex> lock(initMutex_);
    CHK_PRT_RET(isInit_, HCCL_INFO("TbeVectorReduce had been initialized"), HCCL_SUCCESS);

    CHK_RET(GetDeviceType(deviceType_));
    HCCL_INFO("TbeVectorReduce Init get device type[%u]", deviceType_);

    // 初始化op信息
    switch (deviceType_) {
        case LegacyDevType::DEV_TYPE_910:
            InitOpInfoMap910A();
            CHK_RET(GetCoreNum());
            break;
        case LegacyDevType::DEV_TYPE_310P3:
            InitOpInfoMap310P3();
            break;
        case LegacyDevType::DEV_TYPE_910B:
        case LegacyDevType::DEV_TYPE_910_93:
            InitOpInfoMap910B();
            CHK_RET(GetCoreNum());
            break;
        default:
            HCCL_ERROR("[TbeVectorReduce][Init]devType[%u] is not supported", deviceType_);
            return HCCL_E_NOT_SUPPORT;
    }

    // 使用内存申请
    tilingDataHostPtr_ = new (std::nothrow) char[TILING_DATA_MAX_SZIE];
    CHK_PTR_NULL(tilingDataHostPtr_);

    CHK_RET(LoadOpBinary());

    isInit_ = true;
    isDeInit_ = false;
    return HCCL_SUCCESS;
}

HcclResult TbeVectorReduce::DeInit()
{
    std::unique_lock<std::mutex> lock(deInitMutex_);
    HCCL_INFO("TbeVectorReduce Deinit");
    if (isDeInit_) {
        HCCL_INFO("tbe vector reduce has deinit");
        return HCCL_SUCCESS;
    }
    for (auto iter : binaryLoadMap_) {
        if (iter.second != nullptr) {
            HcclResult ret = BinaryUnLoad(iter.second);
            if (ret != HCCL_SUCCESS) {
                HCCL_ERROR("[Destroy][TbeVectorReduce]dev binary unregister failed");
            }
            iter.second = nullptr;
        }
    }
    binaryLoadMap_.clear();

    for (auto &dataPtr : binaryDataPtrVec_) {
        if (dataPtr != nullptr) {
            delete[] dataPtr;
            dataPtr = nullptr;
        }
    }
    binaryDataPtrVec_.clear();

    if (tilingDataHostPtr_ != nullptr) {
        delete[] tilingDataHostPtr_;
        tilingDataHostPtr_ = nullptr;
    }

    for (auto iter = tilingDataMap_.begin(); iter != tilingDataMap_.end(); iter++) {
        if (iter->second != nullptr) {
            if (Free(iter->second) != HCCL_SUCCESS) {
                HCCL_WARNING("free tilingdata memory failed");
            }
            iter->second = nullptr;
        }
    }
    tilingDataMap_.clear();

    for (auto iter = opNameStubFuncsMap_.begin(); iter != opNameStubFuncsMap_.end(); iter++) {
        if (iter->second == nullptr) {
            continue;
        }
        free(iter->second);
        iter->second = nullptr;
    }
    opNameStubFuncsMap_.clear();

    isDeInit_ = true;
    isInit_ = false;
    return HCCL_SUCCESS;
}

HcclResult TbeVectorReduce::LoadOpBinary()
{
    aclrtBinaryLoadOption binaryLoadOption;
    aclrtBinaryLoadOptions binaryLoadOptions;

    binaryLoadOption.type = ACL_RT_BINARY_LOAD_OPT_LAZY_MAGIC;
    if (deviceType_ == LegacyDevType::DEV_TYPE_910 || deviceType_ == LegacyDevType::DEV_TYPE_310P3) {
        binaryLoadOption.value.magic = ACL_RT_BINARY_MAGIC_ELF_AICORE;
    } else if (deviceType_ == LegacyDevType::DEV_TYPE_910B || deviceType_ == LegacyDevType::DEV_TYPE_910_93) {
        binaryLoadOption.value.magic = ACL_RT_BINARY_MAGIC_ELF_VECTOR_CORE;
    }

    // 注册op二进制文件到device
    binaryLoadOptions.numOpt = 1;
    binaryLoadOptions.options = &binaryLoadOption;
    std::string binaryPath;
    CHK_RET(GetOpBinaryPath(binaryPath));
    for (auto &iter : binaryLoadMap_) {
        // 读取op二进制文件
        const void *data  = nullptr;
        size_t length = 0;
        aclrtBinHandle binHandle = nullptr;
        std::string loadBinaryFile = binaryPath + "/" + iter.first + ".o";
        CHK_RET(LoadOpBinaryWithParams(loadBinaryFile, data, length));
        HcclResult ret = BinaryLoadFromData(data, length, &binaryLoadOptions, &binHandle);
        CHK_PRT_RET(ret != HCCL_SUCCESS,
            HCCL_ERROR("[Init][TbeVectorReduce]register opBinary failed binaryLenth[%lld] ret[%d]",
            length, ret), HCCL_E_RUNTIME);
        iter.second = binHandle;
        HCCL_DEBUG("op name [%s] second[%p] binHandle[%p]", iter.first.c_str(), iter.second, binHandle);
    }
    return HCCL_SUCCESS;
}

HcclResult TbeVectorReduce::Run(const void *src1, const void *src2, u64 count, const HcclDataType dataType,
    HcclReduceOp redOp, void *stream, const void *dst)
{
    CHK_PRT_RET(!isInit_, HCCL_ERROR("[Run][Reduce]TbeVectorReduce had not been initialized"), HCCL_E_UNAVAIL);

    CHK_PTR_NULL(src1);
    CHK_PTR_NULL(src2);
    CHK_PTR_NULL(dst);
    CHK_PTR_NULL(stream);
    CHK_PRT_RET(count == 0, HCCL_WARNING("input parameter 'count' is 0"), HCCL_SUCCESS);
    CHK_PRT_RET(dataType >= HCCL_DATA_TYPE_RESERVED, HCCL_ERROR("[Run][Reduce]unsupported data type[%s]",
        LegacyGetDataTypeEnumStr(dataType).c_str()), HCCL_E_PARA);

    CHK_RET(VectorReduceLoop(src1, src2, count, dataType, redOp, stream, dst));
    return HCCL_SUCCESS;
}

HcclResult TbeVectorReduce::GetBinaryName(std::string &binaryName, HcclDataType dataType, HcclReduceOp redOp)
{
    // 规约类型适配
    CHK_RET(GetOpInfoIndex(binaryName, redOp));
    // 数据类型适配
    CHK_RET(GetOpInfoIndex(binaryName, dataType));
    // 芯片类型适配
    if (dataType == HCCL_DATA_TYPE_INT64) {
        CHK_RET(GetOpInfoIndex(binaryName, false));
    } else {
        CHK_RET(GetOpInfoIndex(binaryName, true));
    }
    return HCCL_SUCCESS;
}

HcclResult TbeVectorReduce::GetKernelFunctionAndArgs(std::vector<const void *> &deviceAddrs, std::string &kernelName,
    aclrtBinHandle binHandle, aclrtFuncHandle &funcHandle, aclrtArgsHandle &argsHandle)
{
    CHK_PTR_NULL(binHandle);
    std::unique_lock<std::mutex> stubFuncLock(stubFuncMutex_);
    HCCL_INFO("[GetKernelFunctionAndArgs]kernelName[%s]", kernelName.c_str());
    HcclResult ret = BinaryGetFunction(binHandle, kernelName.c_str(), &funcHandle);
    CHK_PRT_RET(ret != HCCL_SUCCESS, HCCL_ERROR("[GetKernelFunctionAndArgs]binary get function failed"), ret);

    ret = KernelArgsInit(funcHandle, &argsHandle);
    CHK_PRT_RET(ret != HCCL_SUCCESS, HCCL_ERROR("[GetKernelFunctionAndArgs]args init failed"), ret);
 
    aclrtParamHandle paraHandle;
    ret = KernelArgsAppend(argsHandle, deviceAddrs.data(), deviceAddrs.size() * sizeof(void *), &paraHandle);
    CHK_PRT_RET(ret != HCCL_SUCCESS, HCCL_ERROR("[GetKernelFunctionAndArgs]args append failed"), ret);
 
    ret = KernelArgsFinalize(argsHandle);
    CHK_PRT_RET(ret != HCCL_SUCCESS, HCCL_ERROR("[GetKernelFunctionAndArgs]args finalize failed"), ret);
    stubFuncLock.unlock();
    return HCCL_SUCCESS;
}

HcclResult TbeVectorReduce::GetTilingDataInfo(const u64 count, const HcclDataType dataType, const HcclReduceOp redOp,
    void *&tilingDataDevPtr, nlohmann::json &opDescInfo, OpRunInfo &runInfo)
{
    runInfo.blockDim = 0;
    runInfo.workspaces = { 0 };
    runInfo.clearAtomic = 0;
    HCCL_INFO("VectorReduce count[%llu], dataType[%s]", count, LegacyGetDataTypeEnumStr(dataType).c_str());
    if ((IsSupportOperatorDevType910A(dataType, redOp))) {
        CHK_RET(GetOpInfo(dataType, redOp, opDescInfo));
        // 获取tiling的执行信息
        HCCL_INFO("opDescInfo[%s]", opDescInfo.dump().c_str());
        CHK_RET(GetTilingRunInfo(opDescInfo, count, dataType, runInfo, redOp));
    } else {
        nlohmann::json opTilingInfo;
        CHK_RET(GetOpInfo(dataType, redOp, opDescInfo, opTilingInfo));
        // 获取tiling的执行信息
        HCCL_INFO("opDescInfo[%s]", opDescInfo.dump().c_str());
        CHK_RET(GetTilingRunInfo(opDescInfo, opTilingInfo, count, dataType, runInfo, redOp));
    }

    HCCL_INFO("runInfo blockDim[%u], clearAtomic[%d], tilingKey[%u]",
        runInfo.blockDim, runInfo.clearAtomic, runInfo.tilingKey);

    // 如果tilingDataMap_里没有，用GetTilingDataDevMem获取tilingDataDevPtr；如果tilingDataMap_里有，直接取出来给tilingDataDevPtr
    TilingInputInfo tilingDataInfo = { dataType, redOp, count };
    std::unique_lock<std::mutex> tilingDataLock(tilingDataMutex_);
    if (tilingDataMap_.find(tilingDataInfo) == tilingDataMap_.end()) {
        CHK_RET(GetTilingDataDevMem(runInfo, &tilingDataDevPtr, TILING_DATA_MAX_SZIE));
        tilingDataMap_[tilingDataInfo] = tilingDataDevPtr;
    } else {
        tilingDataDevPtr = tilingDataMap_[tilingDataInfo];
    }
    tilingDataLock.unlock();
    return HCCL_SUCCESS;
}

HcclResult TbeVectorReduce::VectorReduce(const void *src1, const void *src2, u64 count, const HcclDataType dataType,
    HcclReduceOp redOp, void *stream, const void *dst)
{
    // 获取op关联的描述信息和tiling的执行信息
    nlohmann::json opDescInfo;
    OpRunInfo runInfo;
    void *tilingDataDevPtr = nullptr;
    CHK_RET(GetTilingDataInfo(count, dataType, redOp, tilingDataDevPtr, opDescInfo, runInfo));

    std::string kernelName = "";
    CHK_RET(GetKernelName(opDescInfo, dataType, kernelName, runInfo, redOp));
 
    std::string binaryName = "";
    CHK_RET(GetBinaryName(binaryName, dataType, redOp));
 
    aclrtBinHandle binHandle = nullptr;
    auto it = binaryLoadMap_.find(binaryName);
    if (it == binaryLoadMap_.end()) {
        HCCL_ERROR("GenerateStubFunc find binaryName[%s] fail.", binaryName.c_str());
        return HCCL_E_NOT_FOUND;
    }
    binHandle = it->second;
    aclrtFuncHandle funcHandle = nullptr;
    aclrtArgsHandle argsHandle = nullptr;

    std::vector<const void *> deviceAddrs;
    CHK_RET(ConcatMem(src1, src2, dst, tilingDataDevPtr, deviceAddrs));
    CHK_RET(GetKernelFunctionAndArgs(deviceAddrs, kernelName, binHandle, funcHandle, argsHandle));
 
    // 执行OP
    aclrtLaunchKernelCfg cfg;
    aclrtLaunchKernelAttr attr;
    attr.id = ACL_RT_LAUNCH_KERNEL_ATTR_TIMEOUT;
    attr.value.timeout = 0;
    cfg.numAttrs = 1;
    cfg.attrs = &attr;
    CHK_RET(LaunchKernelWithConfig(funcHandle, runInfo.blockDim, stream, &cfg, argsHandle, nullptr));
    return HCCL_SUCCESS;
}

HcclResult TbeVectorReduce::VectorReduceLoop(const void *src1, const void *src2, u64 count,
    const HcclDataType dataType, HcclReduceOp redOp, void *stream, const void *dst)
{
    const u32 unitSize = LEGACY_SIZE_TABLE[dataType];
    void *currentSrc1 = const_cast<void *>(src1);
    void *currentSrc2 = const_cast<void *>(src2);
    void *currentDst = const_cast<void *>(dst);
 
    // 计算出字节数为32字节整倍数的最大count
    const u64 maxCountPerLoop = ((TBE_REDUCE_MAX_COUNT * unitSize) / UB_BLOCK_SIZE * UB_BLOCK_SIZE) / unitSize;
    u64 countLeft = count;
 
    while (countLeft > 0) {
        u64 currentCount = countLeft > maxCountPerLoop ? maxCountPerLoop : countLeft;
        HCCL_DEBUG(
            "[VectorReduceLoop] currentCount[%llu], countLeft[%llu], currentSrc1[%p], currentSrc2[%p], currentDst[%p]",
            currentCount, countLeft, currentSrc1, currentSrc2, currentDst);
 
        CHK_RET(VectorReduce(currentSrc1, currentSrc2, currentCount, dataType, redOp, stream, currentDst));
 
        currentSrc1 = static_cast<void *>(static_cast<s8 *>(currentSrc1) + currentCount * unitSize);
        currentSrc2 = static_cast<void *>(static_cast<s8 *>(currentSrc2) + currentCount * unitSize);
        currentDst = static_cast<void *>(static_cast<s8 *>(currentDst) + currentCount * unitSize);
        countLeft -= currentCount;
    }
    return HCCL_SUCCESS;
}

bool TbeVectorReduce::IsSupportOperatorOverflowDevType910A(HcclDataType dataType, HcclReduceOp redOp) const
{
    bool checkDataType = (dataType == HCCL_DATA_TYPE_FP32 || dataType == HCCL_DATA_TYPE_FP16);
    bool checkDeviceType = (deviceType_ == LegacyDevType::DEV_TYPE_910);
    bool checkReduceType = (redOp == HCCL_REDUCE_SUM || redOp == HCCL_REDUCE_PROD);
    return checkDataType && checkDeviceType && checkReduceType;
}

bool TbeVectorReduce::IsSupportOperatorDevType910A(HcclDataType dataType, HcclReduceOp redOp) const
{
    bool checkDataType = (dataType == HCCL_DATA_TYPE_INT64);
    bool checkDeviceType = (deviceType_ == LegacyDevType::DEV_TYPE_910 || deviceType_ == LegacyDevType::DEV_TYPE_910B ||
        deviceType_ == LegacyDevType::DEV_TYPE_910_93);
    bool checkReduceType =
        (redOp == HCCL_REDUCE_SUM || redOp == HCCL_REDUCE_PROD || redOp == HCCL_REDUCE_MAX || redOp == HCCL_REDUCE_MIN);

    return checkDataType && checkDeviceType && checkReduceType;
}

HcclResult TbeVectorReduce::GetKernelName(nlohmann::json &opInfo, HcclDataType dataType, std::string &kernelName,
    OpRunInfo &runInfo, HcclReduceOp redOp)
{
    CHK_RET(GetOpProperty(opInfo, "kernelName", kernelName));

    if ((dataType == HCCL_DATA_TYPE_INT8) || IsSupportOperatorOverflowDevType910A(dataType, redOp) ||
        deviceType_ == LegacyDevType::DEV_TYPE_910B || deviceType_ == LegacyDevType::DEV_TYPE_910_93 ||
        IsSupportOperatorDevType910A(dataType, redOp)) {
        kernelName += '_';
        kernelName += to_string(runInfo.tilingKey);
    }
    return HCCL_SUCCESS;
}

HcclResult TbeVectorReduce::LoadOpBinaryWithParams(std::string &loadBinaryFile, const void *&binaryData, uint64_t &binaryLength)
{
    // 将二进制文件读取到内存中
    std::lock_guard<std::mutex> lock(g_accessFilesMutex);
    std::ifstream infile(loadBinaryFile, std::ios::binary);
    CHK_PRT_RET(!infile.is_open(),
        HCCL_ERROR("[Load][OpBinary]open op binary file[%s] failed,"
        "please check LD_LIBRARY_PATH and ensure that the file is exist",
        loadBinaryFile.c_str()),
        HCCL_E_UNAVAIL);
 
    infile.seekg(0, infile.end);
    binaryLength = infile.tellg(); // 获取二进制文件大小
    infile.seekg(0, infile.beg);
 
    char *binaryDataPtr = nullptr;
    CHK_PRT_RET(binaryLength == 0, HCCL_ERROR("[Load][OpBinary]op binary file size is 0"), HCCL_E_PARA);
    binaryDataPtr = new (std::nothrow) char[binaryLength]; // op二进制文件由类保存，在析构时销毁
    if (binaryDataPtr == nullptr) {
        HCCL_ERROR("[Load][OpBinary]malloc memory for binary data failed");
        infile.close();
        return HCCL_E_UNAVAIL;
    }
    infile.read(binaryDataPtr, binaryLength);
    binaryData = binaryDataPtr;
 
    infile.close();
    binaryDataPtrVec_.push_back(binaryDataPtr);
    return HCCL_SUCCESS;
}

HcclResult TbeVectorReduce::GetOpBinaryPath(std::string &binaryPath)
{
    // 获取二进制文件路径
    std::string libPath;
    char *getPath = nullptr;
    MM_SYS_GET_ENV(MM_ENV_ASCEND_HOME_PATH, getPath);
    if (getPath != nullptr) {
        libPath = getPath;
    } else {
        libPath = "/usr/local/Ascend/cann";
        HCCL_WARNING("[GetOpBinaryPath]ENV:ASCEND_HOME_PATH is not set");
    }
    binaryPath = libPath + "/lib64";
    HCCL_INFO("op binary file path[%s]", binaryPath.c_str());
    return HCCL_SUCCESS;
}

template <typename T, bool IsPointer = std::is_pointer<T>::value>
void PrintHex(const T *p, size_t num, std::stringstream &ss)
{
    for (size_t i = 0; i < num; ++i) {
        if (!IsPointer) {
            // 通过std::setw设置输出位宽为2倍的sizeof(T)
            ss << "0x" << std::setfill('0') << std::setw(static_cast<int32_t>(sizeof(T)) * 2) << std::hex << +p[i] <<
                ' ';
        } else {
            ss << p[i] << ' ';
        }
    }
}

HcclResult TbeVectorReduce::GetTilingDataDevMem(OpRunInfo &runInfo, void **tilingDataDevPtr,
    const u32 tilingDataMaxSize)
{
    // 处理tiling data
    runInfo.tilingData.seekg(0, std::ios::end);
    u64 tilingDataLength = runInfo.tilingData.tellg();
    runInfo.tilingData.seekg(0, std::ios::beg);

    CHK_PRT_RET(tilingDataLength > tilingDataMaxSize,
        HCCL_ERROR("[Get][TilingData]tiling data length[%llu] > max size[%u]", tilingDataLength, tilingDataMaxSize),
        HCCL_E_INTERNAL);

    {
        runInfo.tilingData.read(tilingDataHostPtr_, tilingDataLength);
        std::stringstream ss;
        PrintHex(reinterpret_cast<u8 *>(tilingDataHostPtr_), tilingDataLength, ss);
        HCCL_INFO("[GetTilingDataDevMem]tilingData[%s]", ss.str().c_str());

        CHK_RET(Malloc(tilingDataDevPtr, tilingDataLength));
        HcclResult ret = TbeMemcpy(*tilingDataDevPtr, tilingDataLength, tilingDataHostPtr_, tilingDataLength,
            ACL_MEMCPY_HOST_TO_DEVICE);
        CHK_PRT_RET(ret != HCCL_SUCCESS,
            HCCL_ERROR("[Get][TilingData]tiling data memcpy host to device failed,"
            "size[%llu]",
            tilingDataLength),
            HCCL_E_RUNTIME);
    }
    return HCCL_SUCCESS;
}

HcclResult TbeVectorReduce::SetGlobalWorkSpace(const std::vector<void *> &globalWorkSpaceAddr)
{
    CHK_PRT_RET(!isInit_, HCCL_ERROR("[Run][Reduce]TbeVectorReduce had not been initialized"), HCCL_E_UNAVAIL);
    std::unique_lock<std::mutex> lock(globalWorkSpaceAddrMutex_);
    if (globalWorkSpaceAddr.size() != 0) {
        HCCL_INFO("enable tbe_vector_reduce overflow detection");
        globalWorkSpaceAddr_.assign(globalWorkSpaceAddr.begin(), globalWorkSpaceAddr.end());
    }
    return HCCL_SUCCESS;
}

HcclResult TbeVectorReduce::ConcatMem(const void *src1, const void *src2, const void *dst, const void *tilingDataDevPtr,
    std::vector<const void *> &deviceAddrs)
{
    deviceAddrs.push_back(src2);
    deviceAddrs.push_back(src1);
    deviceAddrs.push_back(dst);
    deviceAddrs.push_back(tilingDataDevPtr);

    // 导入代表溢出检测的内存
    if (globalWorkSpaceAddr_.size() != 0) {
        for (auto &dumpAttr : globalWorkSpaceAddr_) {
            deviceAddrs.push_back(dumpAttr);
        }
    }
    return HCCL_SUCCESS;
}

HcclResult TbeVectorReduce::GetTilingRunInfo(nlohmann::json &opDescInfo, nlohmann::json &opTilingInfo, u64 count,
    HcclDataType dataType, OpRunInfo &runInfo, HcclReduceOp redOp)
{
    // 构造tiling入参，进行tiling操作
    TeOpTensor inputTensor;
    switch (dataType) {
        case HCCL_DATA_TYPE_INT32:
            inputTensor.dtype = "int32";
            break;
        case HCCL_DATA_TYPE_FP32:
            inputTensor.dtype = "float32";
            break;
        case HCCL_DATA_TYPE_FP16:
            inputTensor.dtype = "float16";
            break;
        case HCCL_DATA_TYPE_INT8:
            inputTensor.dtype = "int8";
            break;
        case HCCL_DATA_TYPE_INT16:
            inputTensor.dtype = "int16";
            break;
        default:
            CHK_PRT_RET(true, HCCL_ERROR("[Get][TilingRunInfo]unsupported data type[%s]",
                LegacyGetDataTypeEnumStr(dataType).c_str()), HCCL_E_PARA);
            break;
    }

    inputTensor.shape = { 1, static_cast<s64>(count) };
    inputTensor.oriShape = { 1, static_cast<s64>(count) };
    inputTensor.format = "ND";
    inputTensor.oriFormat = "ND";

    TeOpTensorArg tilingArg;
    tilingArg.tensor.push_back(inputTensor);
    tilingArg.argType = TensorArgType::TA_SINGLE;

    TeOpParas paras;
    paras.inputs.push_back(tilingArg);
    paras.inputs.push_back(tilingArg);
    paras.outputs.push_back(tilingArg);

    std::string opName;
    CHK_RET(GetOpProperty(opDescInfo, "binFileName", opName));

    if (IsSupportOperatorOverflowDevType910A(dataType, redOp) || deviceType_ == LegacyDevType::DEV_TYPE_910B ||
        deviceType_ == LegacyDevType::DEV_TYPE_910_93) {
        bool isTiling = EletwiseTilingV3(opName, paras, opTilingInfo, runInfo);
        CHK_PRT_RET(!isTiling, HCCL_ERROR("[Get][TilingRunInfo]tbe op tiling failed"), HCCL_E_CCE);
    } else if (dataType != HCCL_DATA_TYPE_INT8) {
        bool isTiling = EletwiseTilingV1(opName, paras, opTilingInfo, runInfo);
        CHK_PRT_RET(!isTiling, HCCL_ERROR("[Get][TilingRunInfo]tbe op tiling failed"), HCCL_E_CCE);
    } else {
        bool isTiling = EletwiseTilingV2(opName, paras, opTilingInfo, runInfo);
        CHK_PRT_RET(!isTiling, HCCL_ERROR("[Get][TilingRunInfo]tbe op tiling failed"), HCCL_E_CCE);
    }

    return HCCL_SUCCESS;
}

HcclResult TbeVectorReduce::GetOpInfo(HcclDataType dataType, HcclReduceOp redOp, nlohmann::json &opDescInfo)
{
    std::string opInfoIndex;
    CHK_RET(GetOpInfoIndex(opInfoIndex, dataType, redOp));

    if (opInfoMap_.find(opInfoIndex) == opInfoMap_.end()) {
        HCCL_ERROR("[Get][OpInfo]can't find tbereduce info,opInfoIndex[%s]", opInfoIndex.c_str());
        return HCCL_E_UNAVAIL;
    }
    opDescInfo = opInfoMap_[opInfoIndex];
    return HCCL_SUCCESS;
}

HcclResult TbeVectorReduce::GetTilingRunInfo(nlohmann::json &opDescInfo, u64 count, HcclDataType dataType,
    OpRunInfo &runInfo, HcclReduceOp redOp)
{
    if (CONVERT_DATATYPE_MAP.count(dataType) == 0) {
        HCCL_ERROR("[Get][dataType]data type[%s] is not supported", LegacyGetDataTypeEnumStr(dataType).c_str());
        return HCCL_E_PARA;
    }

    // 解析
    std::string compileInfoJson = opDescInfo.at("compileInfo").dump();
    HCCL_INFO("compileInfoJson[%s]", compileInfoJson.c_str());
    gert::OpTilingParseContextBuilder parserContextBuilder;
    gert::StorageShape x1({1, static_cast<s64>(count)}, {1, static_cast<s64>(count)});
    gert::StorageShape x2({1, static_cast<s64>(count)}, {1, static_cast<s64>(count)});
    gert::StorageShape y({1, static_cast<s64>(count)}, {1, static_cast<s64>(count)});

    AutoTilingCompileInfo compileInfo;
    auto tilingParseContext = parserContextBuilder.OpName("tmp_op")
        .OpType(OPERATOR_MAP[redOp].c_str())
        .IONum(INPUT_NUM, OUTPUT_NUM)
        .InputTensorDesc(0, CONVERT_DATATYPE_MAP[dataType], ge::FORMAT_ND, ge::FORMAT_ND)
        .InputTensorDesc(1, CONVERT_DATATYPE_MAP[dataType], ge::FORMAT_ND, ge::FORMAT_ND)
        .OutputTensorDesc(0, CONVERT_DATATYPE_MAP[dataType], ge::FORMAT_ND, ge::FORMAT_ND)
        .CompiledJson(compileInfoJson.c_str())
        .CompiledInfo(&compileInfo)
        .PlatformInfo(reinterpret_cast<void *>(const_cast<fe::PlatFormInfos *>(&g_platFormInfos)))
        .Build();

    auto parseContext = reinterpret_cast<gert::TilingParseContext *>(tilingParseContext.GetContext());
    CHK_PTR_NULL(parseContext);

    ge::graphStatus gRet = AutoTilingParser(parseContext);
    CHK_PRT_RET(gRet != ge::GRAPH_SUCCESS, HCCL_ERROR("[GetTilingRunInfo] Auto TilingParser failed"), HCCL_E_PARA);

    auto compiledInfo = parseContext->GetCompiledInfo<AutoTilingCompileInfo>();
    CHK_PTR_NULL(compiledInfo);

    // 构造tiling入参，进行tiling操作
    uint32_t maxSize = -1;
    if (GetOpProperty(opDescInfo, "opParaSize", maxSize) != HCCL_SUCCESS) {
        HCCL_INFO("No max tiling size in opdesc.");
        maxSize = kMAX_TILING_DATA_SIZE;
    }
    const auto alignedMaxSize = RoundUp(static_cast<uint64_t>(maxSize), sizeof(uintptr_t));
    const auto tilingData = gert::TilingData::CreateCap(alignedMaxSize);
    const auto workspaceSize = gert::ContinuousVector::Create<size_t>(kMAX_WORKSPACE_COUNT);
    gert::OpTilingContextBuilder contextBuilder;
    auto tilingContext = contextBuilder.OpName("tmp_op")
        .OpType(OPERATOR_MAP[redOp].c_str())
        .IONum(INPUT_NUM, OUTPUT_NUM)
        .TilingData((gert::TilingData *)tilingData.get())
        .Workspace(reinterpret_cast<gert::ContinuousVector *>(workspaceSize.get()))
        .CompileInfo(reinterpret_cast<void *>(compiledInfo))
        .PlatformInfo(reinterpret_cast<void *>(const_cast<fe::PlatFormInfos *>(&g_platFormInfos)))
        .InputTensors({reinterpret_cast<gert::Tensor *>(&x1), reinterpret_cast<gert::Tensor *>(&x2)})
        .OutputTensors({reinterpret_cast<gert::Tensor *>(&y)})
        .Build();

    gert::TilingContext *context = reinterpret_cast<gert::TilingContext *>(tilingContext.GetContext());
    CHK_PTR_NULL(context);

    auto rawTilingData = context->GetRawTilingData();
    if (rawTilingData != nullptr) {
        rawTilingData->SetDataSize(0);
    }

    gRet = AutoTiling(context);
    CHK_PRT_RET(gRet != ge::GRAPH_SUCCESS, HCCL_ERROR("[GetTilingRunInfo] Auto Tiling failed"), HCCL_E_PARA);
    CHK_RET(ConvertToOpRunInfo(context, runInfo));
    return HCCL_SUCCESS;
}

HcclResult TbeVectorReduce::GetOpInfo(HcclDataType dataType, HcclReduceOp redOp, nlohmann::json &opDescInfo,
    nlohmann::json &opTilingInfo)
{
    std::string opInfoIndex;
    CHK_RET(GetOpInfoIndex(opInfoIndex, dataType, redOp));

    if (opInfoMap_.find(opInfoIndex) == opInfoMap_.end()) {
        HCCL_ERROR("[GetOpInfo]cur redOp[%d] dataType[%d] not support, opInfoIndex[%s]",
            redOp, dataType, opInfoIndex.c_str());
        return HCCL_E_NOT_SUPPORT;
    }
    opDescInfo = opInfoMap_[opInfoIndex];

    opInfoIndex += "_tiling_info";
    if (opInfoMap_.find(opInfoIndex) == opInfoMap_.end()) {
        HCCL_ERROR("[Get][OpInfo]can't find tbereduce info,opInfoTilingIndex[%s]", opInfoIndex.c_str());
        return HCCL_E_UNAVAIL;
    }
    opTilingInfo = opInfoMap_[opInfoIndex];

    return HCCL_SUCCESS;
}

HcclResult TbeVectorReduce::GetOpInfoIndex(std::string &opInfoIndex, HcclReduceOp redOp)
{
    switch (redOp) {
        case HCCL_REDUCE_SUM:
            opInfoIndex += "dynamic_add_";
            break;
        case HCCL_REDUCE_PROD:
            opInfoIndex += "dynamic_mul_";
            break;
        case HCCL_REDUCE_MAX:
            opInfoIndex += "dynamic_maximum_";
            break;
        case HCCL_REDUCE_MIN:
            opInfoIndex += "dynamic_minimum_";
            break;
        default:
            CHK_PRT_RET(true, HCCL_ERROR("[Get][OpInfoIndex]calculation type is not supported"), HCCL_E_PARA);
            break;
    }

    return HCCL_SUCCESS;
}

HcclResult TbeVectorReduce::GetOpInfoIndex(std::string &opInfoIndex, HcclDataType dataType)
{
    switch (dataType) {
        case HCCL_DATA_TYPE_INT32:
            opInfoIndex += "int32_";
            break;
        case HCCL_DATA_TYPE_FP32:
            opInfoIndex += "float32_";
            break;
        case HCCL_DATA_TYPE_FP16:
            opInfoIndex += "float16_";
            break;
        case HCCL_DATA_TYPE_INT8:
            opInfoIndex += "int8_";
            break;
        case HCCL_DATA_TYPE_INT16:
            opInfoIndex += "int16_";
            break;
        case HCCL_DATA_TYPE_INT64:
            opInfoIndex += "int64_";
            break;
        default:
            CHK_PRT_RET(true, HCCL_ERROR("[Get][OpInfoIndex]data type is not supported"), HCCL_E_PARA);
            break;
    }

    return HCCL_SUCCESS;
}

HcclResult TbeVectorReduce::GetOpInfoIndex(std::string &opInfoIndex, bool isAddSocName)
{
    switch (deviceType_) {
        default:
        case LegacyDevType::DEV_TYPE_910:
            opInfoIndex += "v80";
            break;
        case LegacyDevType::DEV_TYPE_910B:
        case LegacyDevType::DEV_TYPE_910_93:
            opInfoIndex += "v81";
            break;
        case LegacyDevType::DEV_TYPE_310P3:
            opInfoIndex += "v51";
            break;
        case LegacyDevType::DEV_TYPE_310P1:
            HCCL_ERROR("[Get][OpBinaryPath]devType[%u] is not supported", deviceType_);
            return HCCL_E_NOT_SUPPORT;
    }

    if (isAddSocName) {
        if (deviceType_ == LegacyDevType::DEV_TYPE_910B || deviceType_ == LegacyDevType::DEV_TYPE_910_93) {
            const char *socName = GetSocVer();
            CHK_PRT_RET(socName == nullptr, HCCL_ERROR("[GetDeviceType]get soc name is nullptr"), HCCL_E_RUNTIME);
            HCCL_INFO("get soc name[%s]", socName);
            auto iter = OPINFO_INDEX_SOC_STRING_MAP.find(socName);
            CHK_PRT_RET(iter == OPINFO_INDEX_SOC_STRING_MAP.end(),
                HCCL_ERROR("[Get][OpInfoIndex]socName[%s] is not supported", socName), HCCL_E_NOT_SUPPORT);
            opInfoIndex += iter->second;
        }
    }

    return HCCL_SUCCESS;
}

HcclResult TbeVectorReduce::GetOpInfoIndex(std::string &opInfoIndex, HcclDataType dataType, HcclReduceOp redOp)
{
    // 规约类型适配
    CHK_RET(GetOpInfoIndex(opInfoIndex, redOp));
    // 数据类型适配
    CHK_RET(GetOpInfoIndex(opInfoIndex, dataType));
    // 芯片类型适配
    CHK_RET(GetOpInfoIndex(opInfoIndex));

    return HCCL_SUCCESS;
}

HcclResult TbeVectorReduce::GetOpProperty(nlohmann::json &opInfo, const std::string propName, std::string &propValue)
{
    /* 查找json对象中是否有该属性, 不存在的属性不能直接访问 */
    if (opInfo.find(propName) == opInfo.end()) {
        HCCL_ERROR("[Get][OpProperty]property does not exist");
        return HCCL_E_PARA;
    }
    /* 判断是否是字符串 */
    if (opInfo[propName].is_string()) {
        propValue = static_cast<std::string>(opInfo[propName]);
        return HCCL_SUCCESS;
    } else {
        HCCL_ERROR("[Get][OpProperty]data type is not supported");
        return HCCL_E_PARA;
    }
}

HcclResult TbeVectorReduce::GetOpProperty(nlohmann::json &opInfo, const std::string propName, u32 &propValue)
{
    /* 查找json对象中是否有该属性, 不存在的属性不能直接访问 */
    if (opInfo.find(propName) == opInfo.end()) {
        HCCL_ERROR("[Get][OpProperty]property does not exist");
        return HCCL_E_PARA;
    }
    /* 判断是否是数字 */
    if (opInfo[propName].is_number()) {
        propValue = static_cast<u32>(opInfo[propName]);
        return HCCL_SUCCESS;
    } else {
        HCCL_ERROR("[Get][OpProperty]data type is not supported");
        return HCCL_E_PARA;
    }
}

HcclResult TbeVectorReduce::ConvertToOpRunInfo(gert::TilingContext *context, OpRunInfo &runInfo)
{
    runInfo.blockDim = context->GetBlockDim();
    runInfo.workspaces.push_back(reinterpret_cast<int64_t>(context->GetWorkspaceSizes(context->GetWorkspaceNum())));

    auto rawTilingData = context->GetRawTilingData();
    CHK_PTR_NULL(rawTilingData);

    std::string stringTilingData(reinterpret_cast<char *>(rawTilingData->GetData()), rawTilingData->GetDataSize());
    runInfo.tilingData.str(stringTilingData);
    runInfo.clearAtomic = context->NeedAtomic();
    runInfo.tilingKey = static_cast<uint32_t>(context->GetTilingKey());

    return HCCL_SUCCESS;
}

HcclResult TbeVectorReduce::GetCoreNum()
{
    aclrtDevAttr attr;
    s32 deviceLogicId = -1;
    int64_t coreNum = -1;
    CHK_RET(GetDevice(&deviceLogicId));

    if (deviceType_ == LegacyDevType::DEV_TYPE_910) {
        attr = ACL_DEV_ATTR_AICORE_CORE_NUM;
    } else if (deviceType_ == LegacyDevType::DEV_TYPE_910B || deviceType_ == LegacyDevType::DEV_TYPE_910_93) {
        attr = ACL_DEV_ATTR_VECTOR_CORE_NUM;
    } else {
        HCCL_ERROR("devType[%d] does not need tbe.", deviceType_);
        return HCCL_E_INTERNAL;
    }

    CHK_RET(GetDeviceInfo(deviceLogicId, attr, &coreNum));

    g_platFormInfos.SetCoreNum(static_cast<uint32_t>(coreNum));
    return HCCL_SUCCESS;
}

void TbeVectorReduce::InitOpInfoMap310P3()
{
    opInfoMap_["dynamic_add_float16_v51"] = dynamic_add_float16_v51;
    opInfoMap_["dynamic_add_float16_v51_tiling_info"] = dynamic_add_float16_v51_tiling_info;
    opInfoMap_["dynamic_add_float32_v51"] = dynamic_add_float32_v51;
    opInfoMap_["dynamic_add_float32_v51_tiling_info"] = dynamic_add_float32_v51_tiling_info;
    opInfoMap_["dynamic_add_int32_v51"] = dynamic_add_int32_v51;
    opInfoMap_["dynamic_add_int32_v51_tiling_info"] = dynamic_add_int32_v51_tiling_info;

    opInfoMap_["dynamic_mul_float16_v51"] = dynamic_mul_float16_v51;
    opInfoMap_["dynamic_mul_float16_v51_tiling_info"] = dynamic_mul_float16_v51_tiling_info;
    opInfoMap_["dynamic_mul_float32_v51"] = dynamic_mul_float32_v51;
    opInfoMap_["dynamic_mul_float32_v51_tiling_info"] = dynamic_mul_float32_v51_tiling_info;
    opInfoMap_["dynamic_mul_int32_v51"] = dynamic_mul_int32_v51;
    opInfoMap_["dynamic_mul_int32_v51_tiling_info"] = dynamic_mul_int32_v51_tiling_info;

    opInfoMap_["dynamic_maximum_float16_v51"] = dynamic_maximum_float16_v51;
    opInfoMap_["dynamic_maximum_float16_v51_tiling_info"] = dynamic_maximum_float16_v51_tiling_info;
    opInfoMap_["dynamic_maximum_float32_v51"] = dynamic_maximum_float32_v51;
    opInfoMap_["dynamic_maximum_float32_v51_tiling_info"] = dynamic_maximum_float32_v51_tiling_info;
    opInfoMap_["dynamic_maximum_int32_v51"] = dynamic_maximum_int32_v51;
    opInfoMap_["dynamic_maximum_int32_v51_tiling_info"] = dynamic_maximum_int32_v51_tiling_info;

    opInfoMap_["dynamic_minimum_float16_v51"] = dynamic_minimum_float16_v51;
    opInfoMap_["dynamic_minimum_float16_v51_tiling_info"] = dynamic_minimum_float16_v51_tiling_info;
    opInfoMap_["dynamic_minimum_float32_v51"] = dynamic_minimum_float32_v51;
    opInfoMap_["dynamic_minimum_float32_v51_tiling_info"] = dynamic_minimum_float32_v51_tiling_info;
    opInfoMap_["dynamic_minimum_int32_v51"] = dynamic_minimum_int32_v51;
    opInfoMap_["dynamic_minimum_int32_v51_tiling_info"] = dynamic_minimum_int32_v51_tiling_info;

    opInfoMap_["dynamic_add_int8_v51"] = dynamic_add_int8_v51;
    opInfoMap_["dynamic_add_int8_v51_tiling_info"] = dynamic_add_int8_v51_tiling_info;
    opInfoMap_["dynamic_mul_int8_v51"] = dynamic_mul_int8_v51;
    opInfoMap_["dynamic_mul_int8_v51_tiling_info"] = dynamic_mul_int8_v51_tiling_info;
    opInfoMap_["dynamic_maximum_int8_v51"] = dynamic_maximum_int8_v51;
    opInfoMap_["dynamic_maximum_int8_v51_tiling_info"] = dynamic_maximum_int8_v51_tiling_info;
    opInfoMap_["dynamic_minimum_int8_v51"] = dynamic_minimum_int8_v51;
    opInfoMap_["dynamic_minimum_int8_v51_tiling_info"] = dynamic_minimum_int8_v51_tiling_info;

    binaryLoadMap_["dynamic_mul_int8_v51"] = nullptr;
    binaryLoadMap_["dynamic_add_int8_v51"] = nullptr;
    binaryLoadMap_["dynamic_minimum_int8_v51"] = nullptr;
    binaryLoadMap_["dynamic_maximum_int8_v51"] = nullptr;
    binaryLoadMap_["dynamic_mul_int32_v51"] = nullptr;
    binaryLoadMap_["dynamic_minimum_int32_v51"] = nullptr;
    binaryLoadMap_["dynamic_maximum_int32_v51"] = nullptr;
    binaryLoadMap_["dynamic_add_int32_v51"] = nullptr;
    binaryLoadMap_["dynamic_mul_float32_v51"] = nullptr;
    binaryLoadMap_["dynamic_minimum_float32_v51"] = nullptr;
    binaryLoadMap_["dynamic_maximum_float32_v51"] = nullptr;
    binaryLoadMap_["dynamic_add_float32_v51"] = nullptr;
    binaryLoadMap_["dynamic_mul_float16_v51"] = nullptr;
    binaryLoadMap_["dynamic_minimum_float16_v51"] = nullptr;
    binaryLoadMap_["dynamic_maximum_float16_v51"] = nullptr;
    binaryLoadMap_["dynamic_add_float16_v51"] = nullptr;
}

void TbeVectorReduce::InitOpInfoMap910A()
{
    opInfoMap_["dynamic_add_float16_v80"] = dynamic_add_float16_v80;
    opInfoMap_["dynamic_add_float16_v80_tiling_info"] = dynamic_add_float16_v80_tiling_info;
    opInfoMap_["dynamic_add_float32_v80"] = dynamic_add_float32_v80;
    opInfoMap_["dynamic_add_float32_v80_tiling_info"] = dynamic_add_float32_v80_tiling_info;
    opInfoMap_["dynamic_add_int32_v80"] = dynamic_add_int32_v80;
    opInfoMap_["dynamic_add_int32_v80_tiling_info"] = dynamic_add_int32_v80_tiling_info;

    opInfoMap_["dynamic_mul_float16_v80"] = dynamic_mul_float16_v80;
    opInfoMap_["dynamic_mul_float16_v80_tiling_info"] = dynamic_mul_float16_v80_tiling_info;
    opInfoMap_["dynamic_mul_float32_v80"] = dynamic_mul_float32_v80;
    opInfoMap_["dynamic_mul_float32_v80_tiling_info"] = dynamic_mul_float32_v80_tiling_info;
    opInfoMap_["dynamic_mul_int32_v80"] = dynamic_mul_int32_v80;
    opInfoMap_["dynamic_mul_int32_v80_tiling_info"] = dynamic_mul_int32_v80_tiling_info;

    opInfoMap_["dynamic_maximum_float16_v80"] = dynamic_maximum_float16_v80;
    opInfoMap_["dynamic_maximum_float16_v80_tiling_info"] = dynamic_maximum_float16_v80_tiling_info;
    opInfoMap_["dynamic_maximum_float32_v80"] = dynamic_maximum_float32_v80;
    opInfoMap_["dynamic_maximum_float32_v80_tiling_info"] = dynamic_maximum_float32_v80_tiling_info;
    opInfoMap_["dynamic_maximum_int32_v80"] = dynamic_maximum_int32_v80;
    opInfoMap_["dynamic_maximum_int32_v80_tiling_info"] = dynamic_maximum_int32_v80_tiling_info;

    opInfoMap_["dynamic_minimum_float16_v80"] = dynamic_minimum_float16_v80;
    opInfoMap_["dynamic_minimum_float16_v80_tiling_info"] = dynamic_minimum_float16_v80_tiling_info;
    opInfoMap_["dynamic_minimum_float32_v80"] = dynamic_minimum_float32_v80;
    opInfoMap_["dynamic_minimum_float32_v80_tiling_info"] = dynamic_minimum_float32_v80_tiling_info;
    opInfoMap_["dynamic_minimum_int32_v80"] = dynamic_minimum_int32_v80;
    opInfoMap_["dynamic_minimum_int32_v80_tiling_info"] = dynamic_minimum_int32_v80_tiling_info;

    opInfoMap_["dynamic_add_int8_v80"] = dynamic_add_int8_v80;
    opInfoMap_["dynamic_add_int8_v80_tiling_info"] = dynamic_add_int8_v80_tiling_info;
    opInfoMap_["dynamic_mul_int8_v80"] = dynamic_mul_int8_v80;
    opInfoMap_["dynamic_mul_int8_v80_tiling_info"] = dynamic_mul_int8_v80_tiling_info;
    opInfoMap_["dynamic_maximum_int8_v80"] = dynamic_maximum_int8_v80;
    opInfoMap_["dynamic_maximum_int8_v80_tiling_info"] = dynamic_maximum_int8_v80_tiling_info;
    opInfoMap_["dynamic_minimum_int8_v80"] = dynamic_minimum_int8_v80;
    opInfoMap_["dynamic_minimum_int8_v80_tiling_info"] = dynamic_minimum_int8_v80_tiling_info;

    opInfoMap_["dynamic_add_int64_v80"] = dynamic_add_int64_v80;
    opInfoMap_["dynamic_mul_int64_v80"] = dynamic_mul_int64_v80;
    opInfoMap_["dynamic_maximum_int64_v80"] = dynamic_maximum_int64_v80;
    opInfoMap_["dynamic_minimum_int64_v80"] = dynamic_minimum_int64_v80;

    binaryLoadMap_["dynamic_mul_int8_v80"] = nullptr;
    binaryLoadMap_["dynamic_minimum_int8_v80"] = nullptr;
    binaryLoadMap_["dynamic_maximum_int8_v80"] = nullptr;
    binaryLoadMap_["dynamic_add_int8_v80"] = nullptr;
    binaryLoadMap_["dynamic_mul_int64_v80"] = nullptr;
    binaryLoadMap_["dynamic_minimum_int64_v80"] = nullptr;
    binaryLoadMap_["dynamic_maximum_int64_v80"] = nullptr;
    binaryLoadMap_["dynamic_add_int64_v80"] = nullptr;
    binaryLoadMap_["dynamic_mul_float32_v80"] = nullptr;
    binaryLoadMap_["dynamic_minimum_float32_v80"] = nullptr;
    binaryLoadMap_["dynamic_maximum_float32_v80"] = nullptr;
    binaryLoadMap_["dynamic_mul_float16_v80"] = nullptr;
    binaryLoadMap_["dynamic_add_float16_v80"] = nullptr;
    binaryLoadMap_["dynamic_minimum_float16_v80"] = nullptr;
    binaryLoadMap_["dynamic_maximum_float16_v80"] = nullptr;
    binaryLoadMap_["dynamic_mul_int32_v80"] = nullptr;
    binaryLoadMap_["dynamic_minimum_int32_v80"] = nullptr;
    binaryLoadMap_["dynamic_maximum_int32_v80"] = nullptr;
    binaryLoadMap_["dynamic_add_int32_v80"] = nullptr;
}

void TbeVectorReduce::InitOpInfoMap910B()
{
    opInfoMap_["dynamic_mul_int8_v81_910B1"] = dynamic_mul_int8_v81_910B1;
    opInfoMap_["dynamic_mul_int8_v81_910B1_tiling_info"] = dynamic_mul_int8_v81_910B1_tiling_info;
    opInfoMap_["dynamic_mul_int32_v81_910B1"] = dynamic_mul_int32_v81_910B1;
    opInfoMap_["dynamic_mul_int32_v81_910B1_tiling_info"] = dynamic_mul_int32_v81_910B1_tiling_info;
    opInfoMap_["dynamic_mul_float16_v81_910B1"] = dynamic_mul_float16_v81_910B1;
    opInfoMap_["dynamic_mul_float16_v81_910B1_tiling_info"] = dynamic_mul_float16_v81_910B1_tiling_info;
    opInfoMap_["dynamic_mul_float32_v81_910B1"] = dynamic_mul_float32_v81_910B1;
    opInfoMap_["dynamic_mul_float32_v81_910B1_tiling_info"] = dynamic_mul_float32_v81_910B1_tiling_info;
    opInfoMap_["dynamic_mul_int8_v81_910B2"] = dynamic_mul_int8_v81_910B2;
    opInfoMap_["dynamic_mul_int8_v81_910B2_tiling_info"] = dynamic_mul_int8_v81_910B2_tiling_info;
    opInfoMap_["dynamic_mul_int32_v81_910B2"] = dynamic_mul_int32_v81_910B2;
    opInfoMap_["dynamic_mul_int32_v81_910B2_tiling_info"] = dynamic_mul_int32_v81_910B2_tiling_info;
    opInfoMap_["dynamic_mul_float16_v81_910B2"] = dynamic_mul_float16_v81_910B2;
    opInfoMap_["dynamic_mul_float16_v81_910B2_tiling_info"] = dynamic_mul_float16_v81_910B2_tiling_info;
    opInfoMap_["dynamic_mul_float32_v81_910B2"] = dynamic_mul_float32_v81_910B2;
    opInfoMap_["dynamic_mul_float32_v81_910B2_tiling_info"] = dynamic_mul_float32_v81_910B2_tiling_info;
    opInfoMap_["dynamic_mul_int8_v81_910B3"] = dynamic_mul_int8_v81_910B3;
    opInfoMap_["dynamic_mul_int8_v81_910B3_tiling_info"] = dynamic_mul_int8_v81_910B3_tiling_info;
    opInfoMap_["dynamic_mul_int32_v81_910B3"] = dynamic_mul_int32_v81_910B3;
    opInfoMap_["dynamic_mul_int32_v81_910B3_tiling_info"] = dynamic_mul_int32_v81_910B3_tiling_info;
    opInfoMap_["dynamic_mul_float16_v81_910B3"] = dynamic_mul_float16_v81_910B3;
    opInfoMap_["dynamic_mul_float16_v81_910B3_tiling_info"] = dynamic_mul_float16_v81_910B3_tiling_info;
    opInfoMap_["dynamic_mul_float32_v81_910B3"] = dynamic_mul_float32_v81_910B3;
    opInfoMap_["dynamic_mul_float32_v81_910B3_tiling_info"] = dynamic_mul_float32_v81_910B3_tiling_info;
    opInfoMap_["dynamic_mul_int8_v81_910B4"] = dynamic_mul_int8_v81_910B4;
    opInfoMap_["dynamic_mul_int8_v81_910B4_tiling_info"] = dynamic_mul_int8_v81_910B4_tiling_info;
    opInfoMap_["dynamic_mul_int32_v81_910B4"] = dynamic_mul_int32_v81_910B4;
    opInfoMap_["dynamic_mul_int32_v81_910B4_tiling_info"] = dynamic_mul_int32_v81_910B4_tiling_info;
    opInfoMap_["dynamic_mul_float16_v81_910B4"] = dynamic_mul_float16_v81_910B4;
    opInfoMap_["dynamic_mul_float16_v81_910B4_tiling_info"] = dynamic_mul_float16_v81_910B4_tiling_info;
    opInfoMap_["dynamic_mul_float32_v81_910B4"] = dynamic_mul_float32_v81_910B4;
    opInfoMap_["dynamic_mul_float32_v81_910B4_tiling_info"] = dynamic_mul_float32_v81_910B4_tiling_info;

    opInfoMap_["dynamic_mul_int8_v81_910B4-1"] = dynamic_mul_int8_v81_910B4;
    opInfoMap_["dynamic_mul_int8_v81_910B4-1_tiling_info"] = dynamic_mul_int8_v81_910B4_tiling_info;
    opInfoMap_["dynamic_mul_int32_v81_910B4-1"] = dynamic_mul_int32_v81_910B4;
    opInfoMap_["dynamic_mul_int32_v81_910B4-1_tiling_info"] = dynamic_mul_int32_v81_910B4_tiling_info;
    opInfoMap_["dynamic_mul_float16_v81_910B4-1"] = dynamic_mul_float16_v81_910B4;
    opInfoMap_["dynamic_mul_float16_v81_910B4-1_tiling_info"] = dynamic_mul_float16_v81_910B4_tiling_info;
    opInfoMap_["dynamic_mul_float32_v81_910B4-1"] = dynamic_mul_float32_v81_910B4;
    opInfoMap_["dynamic_mul_float32_v81_910B4-1_tiling_info"] = dynamic_mul_float32_v81_910B4_tiling_info;

    opInfoMap_["dynamic_add_int64_v81"] = dynamic_add_int64_v81;
    opInfoMap_["dynamic_mul_int64_v81"] = dynamic_mul_int64_v81;
    opInfoMap_["dynamic_maximum_int64_v81"] = dynamic_maximum_int64_v81;
    opInfoMap_["dynamic_minimum_int64_v81"] = dynamic_minimum_int64_v81;

    opInfoMap_["dynamic_add_int64_v81_910B1"] = dynamic_add_int64_v81;
    opInfoMap_["dynamic_mul_int64_v81_910B1"] = dynamic_mul_int64_v81;
    opInfoMap_["dynamic_maximum_int64_v81_910B1"] = dynamic_maximum_int64_v81;
    opInfoMap_["dynamic_minimum_int64_v81_910B1"] = dynamic_minimum_int64_v81;

    opInfoMap_["dynamic_add_int64_v81_910B2"] = dynamic_add_int64_v81;
    opInfoMap_["dynamic_mul_int64_v81_910B2"] = dynamic_mul_int64_v81;
    opInfoMap_["dynamic_maximum_int64_v81_910B2"] = dynamic_maximum_int64_v81;
    opInfoMap_["dynamic_minimum_int64_v81_910B2"] = dynamic_minimum_int64_v81;

    opInfoMap_["dynamic_add_int64_v81_910B3"] = dynamic_add_int64_v81;
    opInfoMap_["dynamic_mul_int64_v81_910B3"] = dynamic_mul_int64_v81;
    opInfoMap_["dynamic_maximum_int64_v81_910B3"] = dynamic_maximum_int64_v81;
    opInfoMap_["dynamic_minimum_int64_v81_910B3"] = dynamic_minimum_int64_v81;

    opInfoMap_["dynamic_add_int64_v81_910B4"] = dynamic_add_int64_v81;
    opInfoMap_["dynamic_mul_int64_v81_910B4"] = dynamic_mul_int64_v81;
    opInfoMap_["dynamic_maximum_int64_v81_910B4"] = dynamic_maximum_int64_v81;
    opInfoMap_["dynamic_minimum_int64_v81_910B4"] = dynamic_minimum_int64_v81;

    opInfoMap_["dynamic_add_int64_v81_910B4-1"] = dynamic_add_int64_v81;
    opInfoMap_["dynamic_mul_int64_v81_910B4-1"] = dynamic_mul_int64_v81;
    opInfoMap_["dynamic_maximum_int64_v81_910B4-1"] = dynamic_maximum_int64_v81;
    opInfoMap_["dynamic_minimum_int64_v81_910B4-1"] = dynamic_minimum_int64_v81;

    binaryLoadMap_["dynamic_mul_int32_v81_910B4"] = nullptr;
    binaryLoadMap_["dynamic_mul_int8_v81_910B4"] = nullptr;
    binaryLoadMap_["dynamic_mul_float16_v81_910B4"] = nullptr;
    binaryLoadMap_["dynamic_mul_float32_v81_910B4"] = nullptr;
    binaryLoadMap_["dynamic_mul_int32_v81_910B3"] = nullptr;
    binaryLoadMap_["dynamic_mul_int8_v81_910B3"] = nullptr;
    binaryLoadMap_["dynamic_mul_float16_v81_910B3"] = nullptr;
    binaryLoadMap_["dynamic_mul_float32_v81_910B3"] = nullptr;
    binaryLoadMap_["dynamic_mul_int32_v81_910B2"] = nullptr;
    binaryLoadMap_["dynamic_mul_int8_v81_910B2"] = nullptr;
    binaryLoadMap_["dynamic_mul_float16_v81_910B2"] = nullptr;
    binaryLoadMap_["dynamic_mul_float32_v81_910B2"] = nullptr;
    binaryLoadMap_["dynamic_mul_int32_v81_910B1"] = nullptr;
    binaryLoadMap_["dynamic_mul_int8_v81_910B1"] = nullptr;
    binaryLoadMap_["dynamic_mul_float16_v81_910B1"] = nullptr;
    binaryLoadMap_["dynamic_mul_float32_v81_910B1"] = nullptr;
    binaryLoadMap_["dynamic_mul_int64_v81"] = nullptr;
    binaryLoadMap_["dynamic_minimum_int64_v81"] = nullptr;
    binaryLoadMap_["dynamic_maximum_int64_v81"] = nullptr;
    binaryLoadMap_["dynamic_add_int64_v81"] = nullptr;
}

HcclResult TbeVectorReduce::PrepareVectorReduce(const void *src1, const void *src2, u64 count,
    const HcclDataType dataType, HcclReduceOp redOp, const void *dst, void *stream, void *&addrListDevMemPtr,
    void *&argsHandle, void *&stubFuncAddr, uint32_t &blockDim)
{
    CHK_PRT_RET(!isInit_, HCCL_ERROR("[Run][Reduce]TbeVectorReduce had not been initialized"), HCCL_E_UNAVAIL);
    CHK_PTR_NULL(src1);
    CHK_PTR_NULL(src2);
    CHK_PTR_NULL(dst);
    nlohmann::json opDescInfo;
    void *tilingDataDevPtr = nullptr;
    // 获取tiling的执行信息
    TbeReduce::OpRunInfo runInfo;
    CHK_RET(GetTilingDataInfo(count, dataType, redOp, tilingDataDevPtr, opDescInfo, runInfo));

    std::vector<const void *> deviceAddrs;
    CHK_RET(ConcatMem(src1, src2, dst, tilingDataDevPtr, deviceAddrs));
    rtArgsEx_t argsInfo;
    s32 sRet = memset_s(&argsInfo, sizeof(rtArgsEx_t), 0, sizeof(rtArgsEx_t));
    CHK_PRT_RET(sRet != EOK, HCCL_ERROR("[PrepareVectorReduce]memory set error. return[%d].", sRet), HCCL_E_PARA);
    argsInfo.args = deviceAddrs.data();
    argsInfo.argsSize = deviceAddrs.size() * sizeof(void *);
    CHK_RET(GetDevArgsAddr(stream, &argsInfo, &addrListDevMemPtr, &argsHandle));

    std::string kernelName = "";
    CHK_RET(GetKernelName(opDescInfo, dataType, kernelName, runInfo, redOp));
 
    std::string binaryName = "";
    CHK_RET(GetBinaryName(binaryName, dataType, redOp));
 
    aclrtBinHandle binHandle = nullptr;
    auto it = binaryLoadMap_.find(binaryName);
    if (it == binaryLoadMap_.end()) {
        HCCL_ERROR("GenerateStubFunc find binaryName[%s] fail.", binaryName.c_str());
        return HCCL_E_NOT_FOUND;
    }
    binHandle = it->second;
    aclrtFuncHandle funcHandle = nullptr;
    std::unique_lock<std::mutex> stubFuncLock(stubFuncMutex_);
    HcclResult ret = BinaryGetFunction(binHandle, kernelName.c_str(), &funcHandle);
    CHK_PRT_RET(ret != HCCL_SUCCESS, HCCL_ERROR("[GetKernelFunctionAndArgs]binary get function failed"), ret);
    stubFuncLock.unlock();
    void *aicAddr = nullptr;
    CHK_RET(GetFunctionAddr(funcHandle, &aicAddr, &stubFuncAddr));

    blockDim = runInfo.blockDim;
    return HCCL_SUCCESS;
}

HcclResult TbeVectorReduce::GetVectorBlockSize(u32& blockSize)
{
    blockSize = UB_BLOCK_SIZE;
    return HCCL_SUCCESS;
}
} // namespace TbeReduce

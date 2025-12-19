/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "tbe_crack_cleared.h"
#include <fstream>
#include <unistd.h>
#include <nlohmann/json.hpp>
#include <hccl/hccl_types.h>
#include "tbe_vector_reduce.h"
#include "elewise_memset.h"
#include "op_json_info.h"
#include "op_tiling.h"
#include "vector_tiling.h"
#include "mmpa_api.h"

namespace TbeReduce {
using namespace std;

TbeCrackCleard::TbeCrackCleard() {}

TbeCrackCleard::~TbeCrackCleard()
{
    if (!isDeInit_) {
        CrackDeInit();
    }
}

HcclResult TbeCrackCleard::CrackInit()
{
    std::unique_lock<std::mutex> lock(initMutex_);
    CHK_PRT_RET(isInit_, HCCL_INFO("TbeCrackCleard had been initialized"), HCCL_SUCCESS);

    CHK_RET(GetDeviceType(deviceType_));
    HCCL_INFO("TbeCrackCleard Init get device type[%u]", deviceType_);

    switch (deviceType_) {
        case LegacyDevType::DEV_TYPE_910:
            CrackInitOpInfoMap910A();
            break;
        case LegacyDevType::DEV_TYPE_310P3:
            CrackInitOpInfoMap310P3();
            break;
        case LegacyDevType::DEV_TYPE_910B:
            CrackInitOpInfoMap910B();
            break;
        case LegacyDevType::DEV_TYPE_910_93:
            break;
        default:
            HCCL_ERROR("[TbeCrackCleard][Init]devType[%u] is not supported", deviceType_);
            return HCCL_E_NOT_SUPPORT;
    }

    CHK_RET(LoadOpBinary());

    isInit_ = true;
    isDeInit_ = false;
    return HCCL_SUCCESS;
}

HcclResult TbeCrackCleard::CrackDeInit()
{
    std::unique_lock<std::mutex> lock(deInitMutex_);
    HCCL_INFO("TbeCrackCleard Deinit");
    if (isDeInit_) {
        HCCL_INFO("tbe crack cleard has deinit");
        return HCCL_SUCCESS;
    }
    if (addrListDevPtr_ != nullptr) {
        if (Free(addrListDevPtr_) != HCCL_SUCCESS) {
            HCCL_WARNING("free device memory failed");
        }
        addrListDevPtr_ = nullptr;
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

HcclResult TbeCrackCleard::GetTilingRunInfo(nlohmann::json &opDescInfo, nlohmann::json &opTilingInfo,\
    OpRunInfo &runInfo, const std::vector<std::int64_t> &crackSize)
{
    MemSetCompileInfo CompileInfo;
    CompileInfo.workspace_num = opTilingInfo["vars"]["workspace_num"];
    CompileInfo.ub_size = opTilingInfo["vars"]["ub_size"];
    CompileInfo.core_num = opTilingInfo["vars"]["core_num"];
    CompileInfo.max_repeat_time = opTilingInfo["vars"]["max_repeat_time"];

    std::string opName;
    CHK_RET(GetOpProperty(opDescInfo, "binFileName", opName));

    bool isTiling = MemSetTiling(opName, crackSize, CompileInfo, runInfo);
    CHK_PRT_RET(!isTiling, HCCL_ERROR("[Get][TilingRunInfo]crack cleared tiling failed"), HCCL_E_CCE);

    return HCCL_SUCCESS;
}

HcclResult TbeCrackCleard::Run(const std::vector<std::int64_t> &crackAddr, const std::vector<std::int64_t> &crackSize,
    HcclRtStream stream)
{
    CHK_PRT_RET(!isInit_, HCCL_ERROR("[Run][Reduce]TbeVectorReduce had not been initialized"), HCCL_E_UNAVAIL);
    if (deviceType_ == LegacyDevType::DEV_TYPE_910_93) { // 当前tensor间缝隙清零支持910 910B 310p3
            HCCL_WARNING("[TbeCrackCleard][Run] not support 910_93");
            return HCCL_SUCCESS;
        }

    uint64_t requiredSize = (crackAddr.size() + 1) * MEMSET_CRACK_TILING_DATA_MAX_SZIE;
    if (tilingDataHostSize_ < requiredSize) {
        if (tilingDataHostPtr_ != nullptr) {
            delete[] tilingDataHostPtr_;
        }
        tilingDataHostPtr_ = new (std::nothrow) char[requiredSize];
        tilingDataHostSize_ = requiredSize;
        CHK_PTR_NULL(tilingDataHostPtr_);
    }

    // 获取op关联的描述信息和tiling信息
    nlohmann::json opDescInfo;
    nlohmann::json opTilingInfo;
    CHK_RET(GetOpInfo(opDescInfo, opTilingInfo));

    // 获取tiling data
    OpRunInfo runInfo;
    runInfo.blockDim = 0;
    runInfo.workspaces = { 0 };
    runInfo.clearAtomic = 0;
    CHK_RET(GetTilingRunInfo(opDescInfo, opTilingInfo, runInfo, crackSize));

    // 将tilingdata拷贝device侧
    std::unique_lock<std::mutex> tilingDataLock(tilingDataMutex_);
    void *tilingDataDevPtr = nullptr;
    CHK_RET(GetTilingDataDevMem(runInfo, &tilingDataDevPtr,\
        (crackAddr.size() + 1) * MEMSET_CRACK_TILING_DATA_MAX_SZIE));
    tilingDataLock.unlock();

    string kernelName = "";
    CHK_RET(GetOpProperty(opDescInfo, "kernelName", kernelName));
 
    aclrtBinHandle binHandle = nullptr;
    auto it = binaryLoadMap_.find(binaryName_);
    if (it == binaryLoadMap_.end()) {
        HCCL_ERROR("GenerateStubFunc find binaryName[%s] fail.", binaryName_.c_str());
        return HCCL_E_NOT_FOUND;
    }
    binHandle = it->second;

    // 执行OP
    CHK_RET(ExecuteKernelLaunch(crackAddr, stream, binHandle, kernelName, runInfo, tilingDataDevPtr));
    return HCCL_SUCCESS;
}

HcclResult TbeCrackCleard::ExecuteKernelLaunch(const std::vector<std::int64_t> &crackAddr, void *stream,
    aclrtBinHandle &binHandle, std::string &kernelName, OpRunInfo &runInfo, void *tilingDataDevPtr)
{
    std::unique_lock<std::mutex> crackDataLock(crackDataMutex_);

    u64 addrListDevSize = ((crackAddr.size()) * sizeof(std::int64_t) + MEMSET_ALIGNMENT_CRACK_LIST_MAX_SZIE - 1) \
        * MEMSET_ALIGNMENT_CRACK_LIST_MAX_SZIE / MEMSET_ALIGNMENT_CRACK_LIST_MAX_SZIE;

    CHK_RET(Malloc(&addrListDevPtr_, addrListDevSize));
    CHK_PTR_NULL(addrListDevPtr_);
    HcclResult ret = TbeMemcpy(reinterpret_cast<void *>(static_cast<s8 *>(addrListDevPtr_)),\
        crackAddr.size() * sizeof(std::int64_t), crackAddr.data(), crackAddr.size() * sizeof(std::int64_t),\
        ACL_MEMCPY_HOST_TO_DEVICE);
    CHK_PRT_RET(ret != HCCL_SUCCESS,
        HCCL_ERROR("[ExecuteKernelLaunch]crackAddr data memcpy host to device failed"), HCCL_E_RUNTIME);
    crackDataLock.unlock();

    std::vector<const void *> deviceAddrs;
    deviceAddrs.push_back(addrListDevPtr_);
    deviceAddrs.push_back(tilingDataDevPtr);

    aclrtFuncHandle funcHandle = nullptr;
    aclrtArgsHandle argsHandle = nullptr;
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

HcclResult TbeCrackCleard::GetopInfoIndex(std::string &opInfoIndex)
{
    // 芯片类型适配
    switch (deviceType_) {
        default:
        case LegacyDevType::DEV_TYPE_910:
            opInfoIndex += "910";
            break;
        case LegacyDevType::DEV_TYPE_910B:
            opInfoIndex += "910b";
            break;
        case LegacyDevType::DEV_TYPE_310P3:
            opInfoIndex += "310p3";
            break;
        case LegacyDevType::DEV_TYPE_910_93:
            break;
        case LegacyDevType::DEV_TYPE_310P1:
            HCCL_ERROR("[Get][OpBinaryPath]devType[%u] is not supported", deviceType_);
            return HCCL_E_NOT_SUPPORT;
    }

    return HCCL_SUCCESS;
}

HcclResult TbeCrackCleard::GetOpInfo(nlohmann::json &opDescInfo, nlohmann::json &opTilingInfo)
{
    std::string opInfoIndex = "MemSet_dynamic_AtomicAddrClean_1_ascend"; // TBE支持下发清零算子名称
    CHK_RET(GetopInfoIndex(opInfoIndex));
    if (opInfoMap_.find(opInfoIndex) == opInfoMap_.end()) {
        HCCL_ERROR("[Get][OpInfo]can't find tbereduce info,opInfoIndex[%s]", opInfoIndex.c_str());
        return HCCL_E_UNAVAIL;
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

void TbeCrackCleard::CrackInitOpInfoMap910A()
{
    opInfoMap_["MemSet_dynamic_AtomicAddrClean_1_ascend910"] = MemSet_dynamic_AtomicAddrClean_1_ascend910;
    opInfoMap_["MemSet_dynamic_AtomicAddrClean_1_ascend910_tiling_info"] =\
        MemSet_dynamic_AtomicAddrClean_1_ascend910_tiling_info;
    binaryLoadMap_["MemSet_dynamic_AtomicAddrClean_1_ascend910"] = nullptr;
    binaryName_ = "MemSet_dynamic_AtomicAddrClean_1_ascend910";
}

void TbeCrackCleard::CrackInitOpInfoMap310P3()
{
    opInfoMap_["MemSet_dynamic_AtomicAddrClean_1_ascend310p3"] = MemSet_dynamic_AtomicAddrClean_1_ascend310p3;
    opInfoMap_["MemSet_dynamic_AtomicAddrClean_1_ascend310p3_tiling_info"] =\
        MemSet_dynamic_AtomicAddrClean_1_ascend310p3_tiling_info;
    binaryLoadMap_["MemSet_dynamic_AtomicAddrClean_1_ascend310p3"] = nullptr;
    binaryName_ = "MemSet_dynamic_AtomicAddrClean_1_ascend310p3";
}

void TbeCrackCleard::CrackInitOpInfoMap910B()
{
    opInfoMap_["MemSet_dynamic_AtomicAddrClean_1_ascend910b"] = MemSet_dynamic_AtomicAddrClean_1_ascend910b;
    opInfoMap_["MemSet_dynamic_AtomicAddrClean_1_ascend910b_tiling_info"] =\
        MemSet_dynamic_AtomicAddrClean_1_ascend910b_tiling_info;
    binaryLoadMap_["MemSet_dynamic_AtomicAddrClean_1_ascend910b"] = nullptr;
    binaryName_ = "MemSet_dynamic_AtomicAddrClean_1_ascend910b";
}
} // namespace TbeReduce
/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <iostream>
#include <fstream>
#include <string>
#include "launch_aicpu.h"
#include "log.h"
#include "env_config.h"
#include "mmpa_api.h"


using namespace std;

namespace hccl {
HcclResult InitKernelArgsPrepare(aclrtBinHandle binHandle, const std::string &kernelName,
    void *initTaskAddr, u32 initTaskSize, aclrtFuncHandle &funcHandle, aclrtArgsHandle &argsHandle)
{
    aclError ret = aclrtBinaryGetFunction(binHandle, kernelName.c_str(), &funcHandle);
    CHK_PRT_RET(ret != ACL_SUCCESS,
                HCCL_ERROR("[aclrtBinaryGetFunction]errNo[0x%016llx] get func handle failed, kernelName:%s",
                            ret, kernelName.c_str()),
                HCCL_E_RUNTIME);

    ret = aclrtKernelArgsInit(funcHandle, &argsHandle);
    CHK_PRT_RET(ret != ACL_SUCCESS,
                HCCL_ERROR("[aclrtKernelArgsInit]errNo[0x%016llx] args init failed, kernelName:%s", ret, kernelName.c_str()),
                HCCL_E_RUNTIME);

    aclrtParamHandle paraHandle;
    ret = aclrtKernelArgsAppend(argsHandle, initTaskAddr, initTaskSize, &paraHandle);
    CHK_PRT_RET(ret != ACL_SUCCESS,
                HCCL_ERROR("[aclrtKernelArgsAppend]errNo[0x%016llx] args append failed, append size %u, kernelName:%s", ret,
                            initTaskSize, kernelName.c_str()),
                HCCL_E_RUNTIME);

    ret = aclrtKernelArgsFinalize(argsHandle);
    CHK_PRT_RET(ret != ACL_SUCCESS,
                HCCL_ERROR("[aclrtKernelArgsFinalize]errNo[0x%016llx] args finalize failed, kernelName:%s", ret,
                            kernelName.c_str()),
                HCCL_E_RUNTIME);
    return HCCL_SUCCESS;
}

HcclResult TaskCommKernelArgsPrepare(aclrtBinHandle binHandle, const std::string &kernelName,
    void *contextAddr, u32 contextSize, void *tilingDataPtr, u32 tilingDataSize, aclrtFuncHandle &funcHandle,
    aclrtArgsHandle &argsHandle)
{
    CHK_PRT_RET((tilingDataPtr == nullptr || tilingDataSize == 0),
        HCCL_ERROR("[TaskCommKernelArgsPrepare]param is invalid,tilingDataPtr[%p], tilingDataSize[%llu], kernelName:%s",
        tilingDataPtr, tilingDataSize, kernelName.c_str()), HCCL_E_PARA);

    aclError aclRet = aclrtBinaryGetFunction(binHandle, kernelName.c_str(), &funcHandle);
    CHK_PRT_RET(aclRet != ACL_SUCCESS,
                HCCL_ERROR("[aclrtBinaryGetFunction]errNo[0x%016llx] get func handle failed, kernelName:%s",
                            aclRet, kernelName.c_str()),
                HCCL_E_RUNTIME);

    aclRet = aclrtKernelArgsInit(funcHandle, &argsHandle);
    CHK_PRT_RET(aclRet != ACL_SUCCESS,
                HCCL_ERROR("[aclrtKernelArgsInit]errNo[0x%016llx] args init failed, kernelName:%s", aclRet,
                kernelName.c_str()), HCCL_E_RUNTIME);
    // 拼凑aicpu侧KFCTaskComm结构体
    // 1、先存放HcclOpResParam的context指针
    aclrtParamHandle paraHandle;
    aclRet = aclrtKernelArgsAppend(argsHandle, contextAddr, contextSize, &paraHandle);
    CHK_PRT_RET(aclRet != ACL_SUCCESS,
                HCCL_ERROR("[aclrtKernelArgsAppend]errNo[0x%016llx] args append failed, append size %u, kernelName:%s",
                aclRet, contextSize, kernelName.c_str()), HCCL_E_RUNTIME);
    // 2、再将OpTilingData的tilingData指针拼凑，并将tilingData的hostMem给rts进行H2D
    void *dataAddr;
    aclRet = aclrtKernelArgsAppendPlaceHolder(argsHandle, &paraHandle);
    CHK_PRT_RET(aclRet != ACL_SUCCESS,
                HCCL_ERROR("[aclrtKernelArgsAppendPlaceHolder]errNo[0x%016llx] args append place holder failed",
                aclRet), HCCL_E_RUNTIME);

    aclRet = aclrtKernelArgsGetPlaceHolderBuffer(argsHandle, paraHandle, tilingDataSize, &dataAddr);
    CHK_PRT_RET(aclRet != ACL_SUCCESS,
                HCCL_ERROR("[aclrtKernelArgsGetPlaceHolderBuffer]errNo[0x%016llx] args get place holder buffer failed,"
                "tilingDataSize %u", aclRet, tilingDataSize), HCCL_E_RUNTIME);
    CHK_SAFETY_FUNC_RET(memcpy_s(dataAddr, tilingDataSize, tilingDataPtr, tilingDataSize));

    aclRet = aclrtKernelArgsFinalize(argsHandle);
    CHK_PRT_RET(aclRet != ACL_SUCCESS,
                HCCL_ERROR("[aclrtKernelArgsFinalize]errNo[0x%016llx] args finalize failed, kernelName:%s", aclRet,
                            kernelName.c_str()),
                HCCL_E_RUNTIME);
    return HCCL_SUCCESS;
}

HcclResult AicpuAclKernelLaunch(const rtStream_t stm, void *addr, u32 size,
    aclrtBinHandle binHandle, const std::string &kernelName, bool isInitTask, u16 timeOut,
    void *tilingDataPtr, u32 tilingDataSize)
{
    if (binHandle == nullptr) {
        HCCL_ERROR("binHandle is nullptr, no need to launch aicpu kernel, binHandle[%p]", binHandle);
        return HCCL_E_PTR;
    }
    CHK_PRT_RET((addr == nullptr || size == 0),
        HCCL_ERROR("[AicpuAclKernelLaunch]param is invalid,contextAddr[%p], size[%llu], kernelName:%s",
        addr, size, kernelName.c_str()), HCCL_E_PARA);

    aclrtFuncHandle funcHandle;
    aclrtArgsHandle argsHandle;
    HcclResult ret;
    if (isInitTask) {
        ret = InitKernelArgsPrepare(binHandle, kernelName, addr, size, funcHandle, argsHandle);
        CHK_PRT_RET(ret != HCCL_SUCCESS,
                    HCCL_ERROR("[InitKernelArgsPrepare]errNo[0x%016llx]init args prepare failed, kernelName:%s", ret,
                                kernelName.c_str()),
                    HCCL_E_RUNTIME);
    } else {
        ret = TaskCommKernelArgsPrepare(binHandle, kernelName, addr, size, tilingDataPtr, tilingDataSize, funcHandle,
                                        argsHandle);
        CHK_PRT_RET(ret != HCCL_SUCCESS,
                    HCCL_ERROR("[TaskCommKernelArgsPrepare]errNo[0x%016llx]taskComm args prepare failed, kernelName:%s",
                    ret, kernelName.c_str()), HCCL_E_RUNTIME);
    }

    aclrtLaunchKernelCfg cfg;
    aclrtLaunchKernelAttr attr;
    attr.id = ACL_RT_LAUNCH_KERNEL_ATTR_TIMEOUT;
    attr.value.timeout = timeOut;
    cfg.numAttrs = 1;
    cfg.attrs = &attr;
    constexpr u32 blockDim = 1;
    aclError aclRet = aclrtLaunchKernelWithConfig(funcHandle, blockDim, stm, &cfg, argsHandle, nullptr);
    CHK_PRT_RET(aclRet != ACL_SUCCESS,
                HCCL_ERROR("[aclrtLaunchKernelWithConfig]errNo[0x%016llx] launch kernel failed", ret), HCCL_E_OPEN_FILE_FAILURE);
    return HCCL_SUCCESS;
}

HcclResult GetKernelFilePath(std::string &binaryPath)
{
    // 获取二进制文件路径
    std::string libPath;
    char *getPath = getenv("ASCEND_HOME_PATH");
    MM_SYS_GET_ENV(MM_ENV_ASCEND_HOME_PATH, getPath);
    if (getPath != nullptr) {
        libPath = getPath;
    } else {
        libPath = "/usr/local/Ascend/cann/";
        HCCL_WARNING("[GetKernelFilePath]ENV:ASCEND_HOME_PATH is not set");
    }

    libPath += "/opp/built-in/op_impl/aicpu/config/";
    binaryPath = libPath;
    HCCL_DEBUG("[GetKernelFilePath]kernel folder path[%s]", binaryPath.c_str());

    return HCCL_SUCCESS;
}

}   // ~~ namespace hccl
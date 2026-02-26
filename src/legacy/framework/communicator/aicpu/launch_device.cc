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
#include "launch_device.h"
#include "log.h"
#include "mmpa_api.h"
#include "sal.h"
#include "exception_util.h"
#include "invalid_params_exception.h"
#include "runtime_api_exception.h"

using namespace std;

namespace Hccl {
void GetKernelFilePath(std::string &binaryPath)
{
    std::string libPath = SalGetEnv("ASCEND_HOME_PATH");
    if (libPath.empty() || libPath == "EmptyString") {
        HCCL_WARNING("[GetKernelFilePath]ENV:ASCEND_HOME_PATH is not set, use default:/usr/local/Ascend/cann/");
        libPath = "/usr/local/Ascend/cann/";
    }
    libPath += "/opp/built-in/op_impl/aicpu/config/";
    binaryPath = libPath;
    HCCL_DEBUG("[GetKernelFilePath]kernel folder path[%s]", binaryPath.c_str());
}

void LoadBinaryFromFile(const char *binPath, aclrtBinaryLoadOptionType optionType, uint32_t cpuKernelMode,
                        aclrtBinHandle &binHandle)
{
    if(binPath == nullptr)
    {
        THROW<InvalidParamsException>(StringFormat("[LoadBinaryFromFile]binary path is nullptr", binPath));
    }
    char realPath[PATH_MAX] = {0};
    if(realpath(binPath, realPath) == nullptr)
    {
        THROW<InvalidParamsException>(StringFormat("[LoadBinaryFromFile]binPath:%s is not a valid real path,"
                                                   "err[%d]", binPath,errno));
    }
    HCCL_INFO("[LoadBinaryFromFile]realPath: %s", realPath);

    aclrtBinaryLoadOptions loadOptions = {0};
    aclrtBinaryLoadOption option;
    loadOptions.numOpt = 1;
    loadOptions.options = &option;
    option.type = optionType;
    option.value.cpuKernelMode = cpuKernelMode;
    // ACL_RT_BINARY_LOAD_OPT_CPU_KERNEL_MODE
    aclError aclRet = aclrtBinaryLoadFromFile(realPath, &loadOptions, &binHandle);
    if(aclRet != ACL_SUCCESS)
    {
        THROW<RuntimeApiException>(StringFormat("[LoadBinaryFromFile]:errNo[0x%016llx]load binary from file error.",
                                                aclRet));
    }
}
}   // namespace Hccl
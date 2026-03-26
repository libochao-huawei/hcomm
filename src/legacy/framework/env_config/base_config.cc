/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "base_config.h"
#include <fstream>
#include "sal.h"
#include "log.h"
#include "orion_adapter_rts.h"

namespace Hccl {

// EnvHostNicConfig

void EnvHostNicConfig::Parse()
{
    hcclIfIp.Parse();
    hcclIfBasePort.Parse();
    hcclSocketIfName.Parse();
    whitelistDisable.Parse();
    if (!whitelistDisable.Get()) {
        hcclWhiteListFile.Parse();
        HCCL_RUN_INFO("[Init][EnvVarParam]Env config hcclWhiteListFile[%s]", GetWhiteListFile().c_str());
    }
    hcclHostSocketPortRange.Parse();
    hcclDeviceSocketPortRange.Parse();
    std::ostringstream hosrPortRangeOss;
    std::ostringstream devicePortRangeOss;
    for (auto range : GetHostSocketPortRange()) {
        hosrPortRangeOss << " [" << std::to_string(range.min) << ", " << std::to_string(range.max) << "]";
    }
    for (auto range : GetDeviceSocketPortRange()) {
        devicePortRangeOss << " [" << std::to_string(range.min) << ", " << std::to_string(range.max) << "]";
    }

    HCCL_RUN_INFO("[Init][EnvVarParam]Env config hcclIfIp[%s], hcclIfBasePort[%u], hcclSocketIfName[%s], whitelistDisable[%d], "
                  "hcclHostSocketPortRange[%s], devicePortRangeOss[%s]",
                  GetControlIfIp().Describe().c_str(), GetIfBasePort(), GetSocketIfName().configIfNameStr.c_str(),
                  whitelistDisable.Get(), hosrPortRangeOss.str().c_str(), devicePortRangeOss.str().c_str());
}

const IpAddress &EnvHostNicConfig::GetControlIfIp() const
{
    return hcclIfIp.Get();
}

u32 EnvHostNicConfig::GetIfBasePort() const
{
    return hcclIfBasePort.Get();
}

const SocketIfName &EnvHostNicConfig::GetSocketIfName() const
{
    return hcclSocketIfName.Get();
}

bool EnvHostNicConfig::GetWhitelistDisable() const
{
    return whitelistDisable.Get();
}

const std::string &EnvHostNicConfig::GetWhiteListFile() const
{
    return hcclWhiteListFile.Get();
}

const std::vector<SocketPortRange> &EnvHostNicConfig::GetHostSocketPortRange() const
{
    return hcclHostSocketPortRange.Get();
}

const std::vector<SocketPortRange> &EnvHostNicConfig::GetDeviceSocketPortRange() const
{
    return hcclDeviceSocketPortRange.Get();
}

// EnvSocketConfig

void EnvSocketConfig::Parse()
{
    hcclSocketFamily.Parse();
    linkTimeOut.Parse();
    HCCL_RUN_INFO("[Init][EnvVarParam]Env config hcclSocketFamily[%d], linkTimeOut[%d]",
                  GetSocketFamily(), GetLinkTimeOut());
}

s32 EnvSocketConfig::GetSocketFamily() const
{
    return hcclSocketFamily.Get();
}

s32 EnvSocketConfig::GetLinkTimeOut() const
{
    return linkTimeOut.Get();
}

// EnvRtsConfig

void EnvRtsConfig::Parse()
{
    execTimeOut.Parse();
    aivExecTimeOut.Parse();
    HCCL_RUN_INFO("[Init][EnvVarParam]Env config execTimeOut[%u], aivExecTimeOut[%f]", GetExecTimeOut(), GetAivExecTimeOut());
}

u32 EnvRtsConfig::GetExecTimeOut() const
{
    return execTimeOut.Get();
}

double EnvRtsConfig::GetAivExecTimeOut() const
{
    return aivExecTimeOut.Get();
}

// EnvRdmaConfig

void EnvRdmaConfig::Parse()
{
    rdmaTrafficClass.Parse();
    rdmaServerLevel.Parse();
    rdmaTimeOut.Parse();
    rdmaRetryCnt.Parse();
    HCCL_RUN_INFO("[Init][EnvVarParam]Env config rdmaTrafficClass[%u], rdmaServerLevel[%u], rdmaTimeOut[%u], rdmaRetryCnt[%u]",
                  GetRdmaTrafficClass(), GetRdmaServerLevel(), GetRdmaTimeOut(), GetRdmaRetryCnt());
}

u32 EnvRdmaConfig::GetRdmaTrafficClass() const
{
    return rdmaTrafficClass.Get();
}

u32 EnvRdmaConfig::GetRdmaServerLevel() const
{
    return rdmaServerLevel.Get();
}

u32 EnvRdmaConfig::GetRdmaTimeOut() const
{
    return rdmaTimeOut.Get();
}

u32 EnvRdmaConfig::GetRdmaRetryCnt() const
{
    return rdmaRetryCnt.Get();
}

// EnvAlgoConfig

void EnvAlgoConfig::Parse()
{
    primQueueGenName.Parse();
    hcclAlgoConfig.Parse();
    bufferSize.Parse();
    hcclAccelerator_.Parse();
    std::ostringstream hcclAlgoConfigOss;
    for (auto algoConfig : GetAlgoConfig()) {
        OpType opType = algoConfig.first;
        hcclAlgoConfigOss << " [" << opType.Describe().c_str() << ", ";
        std::vector<HcclAlgoType> algoTypes = algoConfig.second;
        for (auto algoType : algoTypes) {
           hcclAlgoConfigOss << algoType.Describe().c_str() << " ";
        }
        hcclAlgoConfigOss << "]";
    }
    HCCL_RUN_INFO("[Init][EnvVarParam]Env config primQueueGenName[%s], hcclAlgoConfig[%s], bufferSize[%llu], hcclAccelerator[%s]",
                  GetPrimQueueGenName().c_str(), hcclAlgoConfigOss.str().c_str(), GetBuffSize(), GetHcclAccelerator().Describe().c_str());
}

const std::string &EnvAlgoConfig::GetPrimQueueGenName() const
{
    return primQueueGenName.Get();
}

const std::map<OpType, std::vector<HcclAlgoType>> &EnvAlgoConfig::GetAlgoConfig() const
{
    return hcclAlgoConfig.Get();
}

u64 EnvAlgoConfig::GetBuffSize() const
{
    return bufferSize.Get();
}

HcclAccelerator EnvAlgoConfig::GetHcclAccelerator() const
{
    return hcclAccelerator_.Get();
}
 
// EnvLogConfig
void EnvLogConfig::Parse()
{
    entryLogEnable.Parse();
    cannVersion.Parse();
    dfsConfig.Parse();
    HCCL_RUN_INFO("[Init][EnvVarParam]Env config entryLogEnable[%d], cannVersion[%s], dfsConfig[%d]",
                  GetEntryLogEnable(), GetCannVersion().c_str(), GetDfsConfig().taskExceptionEnable);
}

bool EnvLogConfig::GetEntryLogEnable() const
{
    return entryLogEnable.Get();
}

const std::string &EnvLogConfig::GetCannVersion() const
{
    return cannVersion.Get();
}

const DfsConfig &EnvLogConfig::GetDfsConfig() const
{
    return dfsConfig.Get();
}

// EnvDetourConfig

void EnvDetourConfig::Parse()
{
    detourType.Parse();
    HCCL_RUN_INFO("[Init][EnvVarParam]Env config detourType[%s]", GetDetourType().Describe().c_str());
}

HcclDetourType EnvDetourConfig::GetDetourType() const
{
    return detourType.Get();
}

} // namespace Hccl

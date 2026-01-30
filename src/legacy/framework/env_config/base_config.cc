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
    }
    hcclSocketPortRange.Parse();
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

const std::vector<SocketPortRange> &EnvHostNicConfig::GetSocketPortRange() const
{
    return hcclSocketPortRange.Get();
}

// EnvSocketConfig

void EnvSocketConfig::Parse()
{
    hcclSocketFamily.Parse();
    linkTimeOut.Parse();
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
    diagnoseEnable.Parse();
    entryLogEnable.Parse();
    cannVersion.Parse();
    dfsConfig.Parse();
}

bool EnvLogConfig::GetDiagnoseEnable() const
{
    return diagnoseEnable.Get();
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
}

HcclDetourType EnvDetourConfig::GetDetourType() const
{
    return detourType.Get();
}

} // namespace Hccl

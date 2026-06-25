/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "tp_qos.h"

#include <cctype>
#include <string>
#include <vector>

#include "log.h"
#include "hccp.h"

namespace Hccl {

namespace {

static bool ParseDscpFromCfgByQos(const std::string &cfg, uint8_t qos, uint8_t &dscpOut)
{
    constexpr size_t initialReserveSize = 32;
    std::vector<uint32_t> nums;
    nums.reserve(initialReserveSize);
    uint32_t cur = 0;
    bool inNum = false;
    for (char ch : cfg) {
        if (std::isdigit(static_cast<unsigned char>(ch)) != 0) {
            cur = cur * 10U + static_cast<uint32_t>(ch - '0');
            inNum = true;
            continue;
        }
        if (inNum) {
            nums.push_back(cur);
            cur = 0;
            inNum = false;
        }
    }
    if (inNum) {
        nums.push_back(cur);
    }

    if (nums.empty()) {
        return false;
    }

    if (nums.size() > static_cast<size_t>(qos)) {
        const uint32_t dscp = nums[qos];
        if (dscp <= 63U) {
            dscpOut = static_cast<uint8_t>(dscp);
            return true;
        }
    }

    constexpr size_t pairStep = 2;
    for (size_t i = 0; i + 1 < nums.size(); i += pairStep) {
        if (nums[i] == qos && nums[i + 1] <= 63U) {
            dscpOut = static_cast<uint8_t>(nums[i + 1]);
            return true;
        }
    }
    return false;
}

} // namespace

bool TpQosGetDscpByQosFromHccnCfg(const uint32_t devPhyId, uint8_t qos, uint8_t &dscpOut)
{
    struct RaInfo info {};
    info.mode = NETWORK_OFFLINE;
    info.phyId = devPhyId;

    constexpr unsigned int kCfgBufLen = 2048U;
    std::vector<char> value(kCfgBufLen, 0);
    unsigned int valueLen = kCfgBufLen;
    const int ret = RaGetHccnCfg(&info, HCCN_CFG_QOS_DSCP, value.data(), &valueLen);
    unsigned int logLen = valueLen;
    if (logLen > kCfgBufLen) {
        logLen = kCfgBufLen;
    }
    const std::string cfgLog(value.data(), logLen);
    HCCL_INFO("[TpQos][%s] RaGetHccnCfg ret[%d] phyId[%u] valueLen[%u] qos_dscp[%s].", __func__, ret, devPhyId,
        valueLen, cfgLog.c_str());
    if (ret != 0 || valueLen == 0U) {
        return false;
    }
    if (valueLen > kCfgBufLen) {
        valueLen = kCfgBufLen;
    }
    const std::string cfg(value.data(), valueLen);
    return ParseDscpFromCfgByQos(cfg, qos, dscpOut);
}

} // namespace Hccl

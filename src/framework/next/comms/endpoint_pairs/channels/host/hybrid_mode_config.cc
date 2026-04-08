/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "hybrid_mode_config.h"
#include <cstdlib>
#include "log.h"

namespace hcomm {

HybridModeConfig& HybridModeConfig::GetInstance()
{
    static HybridModeConfig instance;
    return instance;
}

HybridModeConfig::HybridModeConfig()
    : hybridModeEnabled_(true),  // 默认启用混合模式
      qpConnectMode_("auto"),    // 默认自动选择
      debugEnabled_(false),
      pollTimeoutMs_(30000),     // 默认30秒
      pollIntervalMs_(1)         // 默认1毫秒
{
    LoadConfigFromEnv();
}

void HybridModeConfig::LoadConfigFromEnv()
{
    // HCCL_HYBRID_MODE: 是否启用混合模式（0/1, 默认1）
    const char* hybridMode = std::getenv(ENV_HCCL_HYBRID_MODE);
    if (hybridMode != nullptr) {
        hybridModeEnabled_ = (std::string(hybridMode) == "1" || std::string(hybridMode) == "true");
    }
    
    // HCCL_HYBRID_QP_CONNECT: QP 连接模式（auto/hccp/ibv, 默认auto）
    const char* qpConnectMode = std::getenv(ENV_HCCL_HYBRID_QP_CONNECT);
    if (qpConnectMode != nullptr) {
        qpConnectMode_ = qpConnectMode;
    }
    
    // HCCL_HYBRID_DEBUG: 是否启用调试日志（0/1, 默认0）
    const char* debugEnabled = std::getenv(ENV_HCCL_HYBRID_DEBUG);
    if (debugEnabled != nullptr) {
        debugEnabled_ = (std::string(debugEnabled) == "1" || std::string(debugEnabled) == "true");
    }
    
    // HCCL_HYBRID_POLL_TIMEOUT: 轮询超时时间（毫秒，默认30000）
    const char* pollTimeout = std::getenv(ENV_HCCL_HYBRID_POLL_TIMEOUT);
    if (pollTimeout != nullptr) {
        pollTimeoutMs_ = static_cast<uint32_t>(std::atoi(pollTimeout));
        if (pollTimeoutMs_ == 0) {
            pollTimeoutMs_ = 30000;  // 最小值保护
        }
    }
    
    // HCCL_HYBRID_POLL_INTERVAL: 轮询间隔（毫秒，默认1）
    const char* pollInterval = std::getenv(ENV_HCCL_HYBRID_POLL_INTERVAL);
    if (pollInterval != nullptr) {
        pollIntervalMs_ = static_cast<uint32_t>(std::atoi(pollInterval));
        if (pollIntervalMs_ == 0) {
            pollIntervalMs_ = 1;  // 最小值保护
        }
    }
    
    HCCL_INFO("[HybridModeConfig] Loaded config: enabled=%d, qpMode=%s, debug=%d, timeout=%u, interval=%u",
        hybridModeEnabled_, qpConnectMode_.c_str(), debugEnabled_, pollTimeoutMs_, pollIntervalMs_);
}

bool HybridModeConfig::IsHybridModeEnabled() const
{
    return hybridModeEnabled_;
}

std::string HybridModeConfig::GetQpConnectMode() const
{
    return qpConnectMode_;
}

bool HybridModeConfig::IsDebugEnabled() const
{
    return debugEnabled_;
}

uint32_t HybridModeConfig::GetPollTimeoutMs() const
{
    return pollTimeoutMs_;
}

uint32_t HybridModeConfig::GetPollIntervalMs() const
{
    return pollIntervalMs_;
}

} // namespace hcomm

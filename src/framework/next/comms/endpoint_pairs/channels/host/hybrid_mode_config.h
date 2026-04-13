/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef HYBRID_MODE_CONFIG_H
#define HYBRID_MODE_CONFIG_H

#include <string>
#include "hccl_types.h"

namespace hcomm {

// 混合模式配置类
class HybridModeConfig {
public:
    // 获取单例实例
    static HybridModeConfig& GetInstance();
    
    // 检查是否启用混合模式
    bool IsHybridModeEnabled() const;
    
    // 获取 QP 连接模式（auto/hccp/ibv）
    std::string GetQpConnectMode() const;
    
    // 检查是否启用调试日志
    bool IsDebugEnabled() const;
    
    // 获取轮询超时时间（毫秒）
    uint32_t GetPollTimeoutMs() const;
    
    // 获取轮询间隔（毫秒）
    uint32_t GetPollIntervalMs() const;

private:
    HybridModeConfig();
    ~HybridModeConfig() = default;
    
    // 从环境变量读取配置
    void LoadConfigFromEnv();
    
    // 配置项
    bool hybridModeEnabled_;
    std::string qpConnectMode_;
    bool debugEnabled_;
    uint32_t pollTimeoutMs_;
    uint32_t pollIntervalMs_;
};

// 环境变量名称常量
constexpr const char* ENV_HCCL_HYBRID_MODE = "HCCL_HYBRID_MODE";
constexpr const char* ENV_HCCL_HYBRID_QP_CONNECT = "HCCL_HYBRID_QP_CONNECT";
constexpr const char* ENV_HCCL_HYBRID_DEBUG = "HCCL_HYBRID_DEBUG";
constexpr const char* ENV_HCCL_HYBRID_POLL_TIMEOUT = "HCCL_HYBRID_POLL_TIMEOUT";
constexpr const char* ENV_HCCL_HYBRID_POLL_INTERVAL = "HCCL_HYBRID_POLL_INTERVAL";

} // namespace hcomm

#endif // HYBRID_MODE_CONFIG_H

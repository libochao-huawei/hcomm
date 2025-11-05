/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef INDEPENDENT_OP_RESOURCE_MANAGER_H
#define INDEPENDENT_OP_RESOURCE_MANAGER_H

#include <memory>
#include <string>
#include <unordered_set>
#include <atomic>
#include "hccl_api.h"
#include "reg_mem_manager.h"
#include "comm_mem_manager.h"
#include "comm_engine_res_manager.h"
#include "rank_graph.h"
#include "comm_config_pub.h"

namespace hccl {

class IndependentOp {
public:
    IndependentOp();
    ~IndependentOp() = default;

    // 初始化资源管理器
    HcclResult SetIndependentOpConfig(const CommConfig &commConfig, const RankTable_t &rankTable,
        const HcclTopoAttr &topoAttr, const aclrtBinHandle binHandle);

    // 获取配置信息
    u32 GetThreadNum() const { return threadNum_; }
    u32 GetNotifyNumPerThread() const { return notifyNumPerThread_; }

    inline CommMemMgr& GetCommMemMgr() {
        return commMemMgr_;
    }

    inline CommEngineResMgr& GetCommEngineResMgr() {
        return engineResMgr_;
    }

    inline RankGraph& GetRankGraph() {
        return rankgraph_;
    }

private:
    // config内容
    int32_t commEngine_ = -1;
    u32 threadNum_ = 0;
    u32 notifyNumPerThread_ = 0;
    u64 cclBufferSize_;
    std::string commId_;

    // 内存注册管理器
    RegMemMgr regMemMgr_;
    CommMemMgr commMemMgr_;
    CommEngineResMgr engineResMgr_;
    RankGraph rankgraph_;
};

}  // namespace hccl
#endif
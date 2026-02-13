/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */


#ifndef __COLL_COMM_AICPU_H__
#define __COLL_COMM_AICPU_H__

#include "common.h"
#include "aicpu_init_param.h"
#include "topo_matcher.h"
#include "hcomm_primitives.h"
#include "transport_pub.h"
#include "thread.h"
#include "local_notify.h"
#include "ub_transport_lite_impl.h"
#include "task_exception.h"
class CollCommAicpu {
public:
    HcclResult InitAicpuIndOp(CommAicpuParam *commAicpuParam);

    uint32_t notifySize_;
    u32 devId_ = 0;
    u32 localUserRank_ = 0;

    std::string identifier_;
    bool indOpCommInitialized_{ false }; // 独立算子流程通信域是否初始化
    HcclTopoInfo topoInfo_;
    std::unordered_map<std::string, ChannelHandle> channelHandleMap_;
    std::unordered_map<ChannelHandle, std::shared_ptr<Transport>> linkMap_;
    std::vector<std::shared_ptr<Thread>> threads_;
    std::vector<std::unique_ptr<LocalNotify>> notifys_;
    TaskException taskExecption_;
    // A5 独立算子
    std::unordered_map<ChannelHandle, std::unique_ptr<Hccl::UbTransportLiteImpl>> ubTransportMap_;

    // 维护aicpu算子展开的索引, 方便定位当前展开的算子信息
    size_t opUnfoldIdx_ = 0;
};


#endif // __COLL_COMM_AICPU_MGR_H__
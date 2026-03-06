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
#include "aicpu_launch_manager.h"
#include "channel_param.h"
#include "hdc_pub.h"

using namespace hccl;
class CollCommAicpu {
public:
    HcclResult InitAicpuIndOp(CommAicpuParam *commAicpuParam);
    HcclResult InitThreads(ThreadMgrAicpuParam *param);
    HcclResult AllocChannelResource(HcclChannelUrmaRes *commParam);
    HcclResult NotifyFree(NotifyMgrAicpuParam *param);
    HcclResult NotifyAlloc(NotifyMgrAicpuParam *param);

private:
    HcclResult InitUrmaChannel(HcclChannelUrmaRes *commParam);
    HcclResult ParsePackData(std::vector<char> &data, ChannelHandle &handle);
    u32 devId_{0};
    //通用的通道
    std::shared_ptr<hccl::HDCommunicate> kfcControlTransferH2D_{nullptr};
    std::shared_ptr<hccl::HDCommunicate> kfcStatusTransferD2H_{nullptr};

    std::string identifier_;
    bool indOpCommInitialized_{ false }; // 独立算子流程通信域是否初始化
    HcclTopoInfo topoInfo_;
    std::vector<std::shared_ptr<Thread>> threads_;
    std::vector<std::unique_ptr<LocalNotify>> notifys_;
    std::unordered_map<s32, Thread*> streamIdToThreadMap_;
    // A5 独立算子
    std::unordered_map<ChannelHandle, std::unique_ptr<Hccl::UbTransportLiteImpl>> ubTransportMap_;
};

#endif // __COLL_COMM_AICPU_H__

/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef ALL_GATHER_NHR_PIPELINE_FOR_910_93_H
#define ALL_GATHER_NHR_PIPELINE_FOR_910_93_H

#include <vector>
#include <memory>
#include <list>
#include <hccl/hccl_types.h>
#include "hccl/base.h"
#include "externalinput_pub.h"
#include "mem_device_pub.h"
#include "stream_pub.h"
#include "alg_template_base_pub.h"

namespace hccl {

class AllGatherNHRPipelineFor91093 : public AlgTemplateBase {
public:
    explicit AllGatherNHRPipelineFor91093(const HcclDispatcher dispatcher);
    ~AllGatherNHRPipelineFor91093() override;

    // 重写基类接口
    HcclResult RunAsync() override;

    // 扩展的Prepare方法（支持三级通信域）
    HcclResult Prepare(
        HcomCollOpInfo *opInfo,
        u32 userRank,
        u64 &count,
        DeviceMem &cclBufferPartOne,
        DeviceMem &cclBufferPartTwo,
        SubCommInfo &level0CommInfo,
        SubCommInfo &level1CommInfo,
        SubCommInfo &level2CommInfo,  // 新增L2参数
        Stream &mainStream,
        std::vector<Stream> &subStream,
        std::vector<std::shared_ptr<LocalNotify>> &notifyMain,
        std::vector<std::shared_ptr<LocalNotify>> &notifySub
    );

    // 三级流水线核心方法
    HcclResult RunThreeLevelPipeline();
    HcclResult SyncLevels(u32 step, u32 bufferIndex);  // 层级同步

private:
    // 同步方法
    HcclResult MainRecordSub(u32 bufferIndex);
    HcclResult SubWaitMain(u32 bufferIndex);
    HcclResult MainWaitSub(u32 bufferIndex);
    HcclResult SubRecordMain(u32 bufferIndex);

    // 三级通信域信息
    SubCommInfo level0CommInfo_;
    SubCommInfo level1CommInfo_;
    SubCommInfo level2CommInfo_;

    // 双缓冲内存
    DeviceMem dmaBuffers_[2];

    // 流资源
    Stream mainStream_;
    std::vector<Stream> subStreams_;

    // 同步信号
    std::vector<std::shared_ptr<LocalNotify>> notifyMain_;
    std::vector<std::shared_ptr<LocalNotify>> notifySub_;

    // 其他成员
    HcomCollOpInfo *opInfo_;
    u64 memSliceCount_;
    u32 userRank_;
    void* usrInMemAddr_;
    void* usrOutMemAddr_;

    // 链路信息
    std::vector<LINK> level0Links_;
    std::vector<LINK> level1Links_;
    std::vector<LINK> level2Links_;
};

} // namespace hccl

#endif /* ALL_GATHER_NHR_PIPELINE_FOR_910_93_H */
/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef __AICPU_ASYNC_UNFOLDER_H__
#define __AICPU_ASYNC_UNFOLDER_H__

#include <memory>
#include <stdint.h>
#include <vector>

#include "hdc_pub.h"
#include "op_async_unfold_info.h"
#include "stream_pub.h"
#include "workflow.h"

namespace hccl {

class AicpuAsyncUnfolder {
public:
    AicpuAsyncUnfolder();
    ~AicpuAsyncUnfolder();

    // 初始化: 根据HcclOpResParam反序列化HDC资源
    HcclResult Init(const hccl::HDCommunicateParams& tailH2DParams,
        const hccl::HDCommunicateParams& headD2HParams, const hccl::HDCommunicateParams* opH2DRingBufferParams,
        const size_t opH2DRingBufferSize);
    
    // 检查是否需要关闭异步展开 (注意: 需要与hccl_communicator_host.cc::UpdateAsyncUnfoldFlag保持一致)
    HcclResult UpdateAsyncUnfoldFlag(const HcclWorkflowMode workflowMode, const OpParam &param,
        const HcclTopoInfo& topoinfo);

    inline bool IsAsyncUnfoldEnable() const { return asyncUnfoldEnable_; }
    
    // 获取第一个未异步展开的OpAsyncUnfoldInfo (if any), 即head位置的OpAsyncUnfoldInfo
    HcclResult GetOpAsyncUnfoldInfo(bool& isValid, OpAsyncUnfoldInfo& opAsyncUnfoldInfo);

    // 消费一个OpAsyncUnfoldInfo, 即head + 1
    HcclResult ConsumeOpAsyncUnfoldInfo();

    // 备份各stream的tailSqeTaskId
    HcclResult BackupTailSqeTaskIds(Stream &mainStream, std::vector<Stream> &slaveStreams);
private:
    // OpH2DRingBuffer: 以ring buffer形式组织OpAsyncUnfoldInfo用于异步展开
    // 注意: 能够进入AICPU则至少是A3, hccl_communicator.cc中的GetSupportHDCommunicate一定返回true, 即HDC一定可用

    // 是否使能异步展开 -> 在以下场景更新:
    // (1) 如果host侧初始化通信域时不使能异步展开, 则不会分配并序列化HDC资源, commParam中的HDC params为默认值 (即buffLen为0),
    //     -> 可以据此在Init时初始化device侧异步展开开关 (注意: hccl_communicator_host.cc::BuildAsyncUnfoldParam已校验默认值);
    // (2) device侧检测到当前/后续算子为图模式/图下沉, 关闭开关
    bool asyncUnfoldEnable_ = true;
    
    // 注意: 对每个OpAsyncUnfoldInfo单独维护一个opH2D HDC;
    // 如果用一个HDC维护整个opRingBuffer, 则单个OpAsyncUnfoldInfo更新会导致ReadCache全量刷新
    std::shared_ptr<hccl::HDCommunicate> tailH2D_{nullptr}; // uint64_t
    std::shared_ptr<hccl::HDCommunicate> headD2H_{nullptr}; // uint64_t
    std::vector<std::shared_ptr<hccl::HDCommunicate>> opH2DRingBuffer_; // std::vector<OpAsyncUnfoldInfo>

    // 由于HDC为单项通道, 维护head_作为headD2H_在device侧的镜像值用于读取
    // 注意: 不涉及并行修改导致head_和headD2H_不一致的问题, 因为每个通信域的HcclCommAicpu独享一个AicpuAsyncUnfolder实例
    // 而每个HcclCommAicpu又是串行执行, 且异步展开过程中不会调用CheckOpExecStatus, 即不会被error CQE / stop KfcCommand打断
    // 因此, head_和headD2H_在device侧一定是同步的
    uint64_t head_ = 0;

    // 最近一次异步展开前备份的per-stream tailSqeTaskId
    uint16_t mainTailSqeTaskId_ = 0;
    std::vector<uint16_t> slaveTailSqeTaskIds_;
}; // class AicpuAsyncUnfolder

} // namespace hccl

#endif // __AICPU_ASYNC_UNFOLDER_H__
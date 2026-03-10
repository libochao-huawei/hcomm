/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
//TODO:以后搬走
#ifndef HCCL_COMM_OP_H
#define HCCL_COMM_OP_H

#include <memory>
#include "hccl_common.h"
#include <unordered_map>
#include "buffer.h"
#include "common.h"
#include "op_mode.h"
#include "task_info.h"

namespace hccl {

struct HcclDfxOpInfo {
    //DfxOpInfo_base
    char                tag_[256];
    u32                 index_{0};
    u64                 beginTime_{0};
    u64                 endTime_{0};
    u64                 ranksize_{0};
    //CollOperator
    char                opTag[256];
    bool                staticAddr{false};
    bool                staticShape{false};
    u32                 myRank;
    //baseCollOperator
    u32                 opMode{0};
    u32                 opType{0};//通过map找dfxopinfo
    u32                 reduceOp{0};
    u32                 dataType{0};
    u32                 outputType{0};
    u64                 dataCount{0};
    u32                 root = INVALID_VALUE_RANKID;
    void*               inputMemPtr{nullptr};
    u64                 inputMemSize{0};
    void*               outputMemPtr{nullptr};
    u64                 outputMemSize{0};
    void*               scratchMemPtr{nullptr};
    u64                 scratchMemSize{0};
    //task_exception
    u64                 cpuTsThread{0}; // host侧算子主流的threadhandle
    u32                 cpuWaitAicpuNotifyIdx{0}; // host wait device notifyIdx
    u32                 cpuWaitAicpuNotifyId{0}; // host wait device notifyId
};

std::shared_ptr<Hccl::DfxOpInfo> ConvertToDfxOpInfo(const HcclDfxOpInfo& dfxOpInfo);

}
int32_t HcommThreadRegisterDfx(ThreadHandle thread, std::function<HcclResult(u32, u32, const Hccl::TaskParam&, u64)> callback);
#endif
/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef HCOM_LAUNCH_KERNEL_H
#define HCOM_LAUNCH_KERNEL_H

#include "hccl/base.h"
#include "hcom_build_graph.h"
#include "register/kernel_registry.h"
#include "register/kernel_registry_impl.h"
#include "exe_graph/lowering/shape_utils.h"
#include "exe_graph/runtime/tensor_data_utils.h"

namespace hccl {
constexpr int INPUT_INDEX_2 = 2;
constexpr int INPUT_INDEX_3 = 3;
constexpr int INPUT_INDEX_4 = 4;
constexpr int OUTPUT_INDEX_0 = 0;
constexpr int OUTPUT_INDEX_1 = 1;
constexpr int INDEX_PARA_2 = 2;
constexpr int INDEX_PARA_3 = 3;

constexpr u64 SHAPE_INFO_COUNT = 100; /* 动态send recv协商的shape info数据长度 */

struct HcomOpLaunchArgs {
    struct HcomOpAttr opAttr;
    void* stream;
    uint32_t inputNum;
    uint32_t outputNum;
    std::vector<void*> inputAddrs;
    std::vector<void*> outputAddrs;
    std::vector<gert::Shape> inputShapes;
    std::vector<gert::Shape> outputShapes;
};

std::vector<std::string> PrintLaunchArgs(const gert::KernelContext *context);
ge::graphStatus LaunchHcomKernel(gert::KernelContext *context);
#ifndef OPEN_BUILD_PROJECT
ge::graphStatus LaunchHcomKernelV2(gert::KernelContext *context);
#endif
HcclResult GetHcomOpLaunchArgs(gert::KernelContext *context, HcomOpLaunchArgs& args);
ge::graphStatus LaunchHcomKernelInitComm(gert::KernelContext *context);

ge::graphStatus BuildHcclOutputShapeOutputs(const ge::FastNode *node, gert::KernelContext *context);
HcclResult GetRecvOpLaunchArgs(gert::KernelContext *context, HcomOpLaunchArgs& args);
ge::graphStatus HcomGetRecvBeforeKernel(HcomOpLaunchArgs& args, std::vector<int64_t>& recvShape);
ge::graphStatus LaunchRecvKernel(gert::KernelContext *context);
HcclResult HcomReduceScatterVKernel(HcomOpLaunchArgs& launchArgs);
HcclResult HcomAllGatherVKernel(HcomOpLaunchArgs& launchArgs);
HcclResult HcomAllToAllVKernel(HcomOpLaunchArgs& launchArgs);
#ifndef OPEN_BUILD_PROJECT
HcclResult HcomAllToAllVKernelV2(HcomOpLaunchArgs& launchArgs);
#endif
HcclResult HcomAllToAllVCKernel(HcomOpLaunchArgs& launchArgs);
HcclResult HcomAllToAllKernel(HcomOpLaunchArgs& launchArgs);
HcclResult HcomGatherAllToAllVKernel(HcomOpLaunchArgs& launchArgs);
HcclResult HcomAllGatherKernel(HcomOpLaunchArgs& launchArgs);
#ifndef OPEN_BUILD_PROJECT
HcclResult HcomAllGatherKernelV2(HcomOpLaunchArgs& launchArgs);
#endif
HcclResult HcomAllReduceKernel(HcomOpLaunchArgs& launchArgs);
#ifndef OPEN_BUILD_PROJECT
HcclResult HcomAllReduceKernelV2(HcomOpLaunchArgs& launchArgs);
HcclResult HcomAllToAllVCKernelV2(HcomOpLaunchArgs& launchArgs);
#endif
HcclResult HcomBroadcastKernel(HcomOpLaunchArgs& launchArgs);
#ifndef OPEN_BUILD_PROJECT
HcclResult HcomBroadcastKernelV2(HcomOpLaunchArgs& launchArgs);
#endif
HcclResult HcomReduceScatterKernel(HcomOpLaunchArgs& launchArgs);
#ifndef OPEN_BUILD_PROJECT
HcclResult HcomReduceScatterKernelV2(HcomOpLaunchArgs& launchArgs);
#endif
HcclResult HcomReduceKernel(HcomOpLaunchArgs& launchArgs);
#ifndef OPEN_BUILD_PROJECT
HcclResult HcomReduceKernelV2(HcomOpLaunchArgs& launchArgs);
#endif
HcclResult RemoteScatterWriteKernel(HcomOpLaunchArgs& launchArgs);
HcclResult HcomCollRemotePairedParaCheck(const HcomRemoteOperationParams &params);
HcclResult HcomSendKernel(HcomOpLaunchArgs& launchArgs);
HcclResult HcomRemoteReadKernel(HcomOpLaunchArgs& launchArgs);
HcclResult HcomRemoteWriteKernel(HcomOpLaunchArgs& launchArgs);
HcclResult HcomRemoteRefReadKernel(HcomOpLaunchArgs& launchArgs);
HcclResult HcomGetBatchAllReduceInfo(const HcomOpLaunchArgs &launchArgs, uint64_t maxCommCount,
    std::vector<uint32_t> &sliceIdxs, std::vector<uint64_t> &inputsCount);
HcclResult HcomCopyInputsToCCLbuff(const HcomOpLaunchArgs &launchArgs, uint32_t inputsNum, uint32_t inputsOffset,
    std::vector<uint64_t> &inputsCount, void *cclBuff, uint64_t cclBuffSize, uint64_t &commCount);
HcclResult GetCountByShape(const gert::Shape& shape, HcclDataType dataType, uint64_t& count);
HcclResult GetSendCountByShapeAlign(const gert::Shape& shape, HcclDataType dataType, uint64_t& count);
uint64_t RoundUp(const uint64_t originValue, const uint64_t multiple);
bool IsRemoteOpType(HcomOpType opType);
} // namespace hccl
#endif // HCOM_LAUNCH_KERNEL_H

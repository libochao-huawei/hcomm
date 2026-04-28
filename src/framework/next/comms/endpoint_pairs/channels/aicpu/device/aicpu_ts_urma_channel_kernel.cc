/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <vector>
#include "aicpu_ts_urma_channel_kernel.h"
#include "framework/aicpu_hccl_process.h"
#include "channel_param.h"
#include "aicpu_indop_process.h"
#include "aicpu_channel_process.h"

extern "C" {
__attribute__((visibility("default"))) uint32_t RunAicpuIndOpChannelInitV2(void *args)
{
    HCCL_RUN_INFO("YYYYYY hcomm RunAicpuIndOpChannelInitV2 start, args[%p]", args);
    CHK_PTR_NULL(args);
    uint64_t devAddr = *reinterpret_cast<uint64_t*>(args);
    HcclChannelUrmaRes *commParam = reinterpret_cast<HcclChannelUrmaRes *>(devAddr);
    HCCL_RUN_INFO("YYYYYY hcomm RunAicpuIndOpChannelInitV2 devAddr[0x%llx], commParam[%p], "
        "sizeof(HcclChannelUrmaRes)[%llu]", static_cast<unsigned long long>(devAddr), commParam,
        static_cast<unsigned long long>(sizeof(HcclChannelUrmaRes)));
    if (commParam != nullptr) {
        HCCL_RUN_INFO("YYYYYY hcomm RunAicpuIndOpChannelInitV2 param hcomId[%s], listNum[%u], channelList[%p], "
            "uniqueIdAddr[%p], uniqueIdSize[%u], channelSizeAddr[%p], remoteRankList[%p], remoteRankId[%p], "
            "deviceLogicId[%d], deviceType[%u]", commParam->hcomId, commParam->listNum, commParam->channelList,
            commParam->uniqueIdAddr, commParam->uniqueIdSize, commParam->channelSizeAddr, commParam->remoteRankList,
            commParam->remoteRankId, commParam->deviceLogicId, commParam->deviceType);
    }
    CHK_PTR_NULL(commParam);
    HCCL_RUN_INFO("YYYYYY hcomm RunAicpuIndOpChannelInitV2 call AicpuIndOpChannelInit begin");
    uint32_t ret = AicpuIndopProcess::AicpuIndOpChannelInit(commParam);
    HCCL_RUN_INFO("YYYYYY hcomm RunAicpuIndOpChannelInitV2 call AicpuIndOpChannelInit end, ret[%u]", ret);
    return ret;
}

__attribute__((visibility("default"))) uint32_t RunAicpuChannelInitV2(void *args)
{
    HCCL_RUN_INFO("RunAicpuIndOpChannelInitV2Internal start.");
    CHK_PTR_NULL(args);
    uint64_t devAddr = *reinterpret_cast<uint64_t*>(args);
    HcclChannelUrmaRes *commParam = reinterpret_cast<HcclChannelUrmaRes *>(devAddr);
    CHK_PTR_NULL(commParam);
    return AicpuChannelProcess::AicpuChannelInit(commParam);
}

__attribute__((visibility("default"))) uint32_t RunAicpuChannelDestroyV2(void *args)
{
    HCCL_RUN_INFO("RunAicpuIndOpChannelDestroyV2Internal start.");
    CHK_PTR_NULL(args);
    uint64_t devAddr = *reinterpret_cast<uint64_t*>(args);
    HcclChannelUrmaRes *commParam = reinterpret_cast<HcclChannelUrmaRes *>(devAddr);
    CHK_PTR_NULL(commParam);
    return AicpuChannelProcess::AicpuChannelDestroy(commParam);
}

__attribute__((visibility("default"))) uint32_t RunAicpuIndOpChannelUpdateV2(void *args)
{
    HCCL_RUN_INFO("RunAicpuIndOpChannelUpdateV2 start.");
    CHK_PTR_NULL(args);
    uint64_t devAddr = *reinterpret_cast<uint64_t*>(args);
    HcclChannelUrmaRes *commParam = reinterpret_cast<HcclChannelUrmaRes *>(devAddr);
    CHK_PTR_NULL(commParam);
    return AicpuIndopProcess::AicpuIndOpChannelUpdate(commParam);
}
}

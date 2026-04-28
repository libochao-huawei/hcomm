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

extern "C" {
__attribute__((visibility("default"))) uint32_t RunAicpuDfxOpInfoInitV2(void *args)
{
    HCCL_RUN_INFO("YYYYYY hcomm dfx [RunAicpuDfxOpInfoInitV2] start, args[%p]", args);
    CHK_PRT_RET(args == nullptr, HCCL_ERROR("YYYYYY hcomm dfx [%s]args is null.", __func__), HCCL_E_PARA);
    struct InitTask {
        u64 context;
        char commTag[256];
    };
    InitTask *ctxArgs = reinterpret_cast<InitTask *>(args);
    CHK_PRT_RET(ctxArgs == nullptr, HCCL_ERROR("YYYYYY hcomm dfx [%s]ctxArgs is null.", __func__), HCCL_E_PARA);
    HcclDfxOpInfo *dfxOpInfo = reinterpret_cast<HcclDfxOpInfo *>(ctxArgs->context);
    HCCL_RUN_INFO("YYYYYY hcomm dfx [RunAicpuDfxOpInfoInitV2] decoded, ctxArgs[%p], context[0x%llx], "
        "commTag[%s], dfxOpInfo[%p]", ctxArgs, static_cast<unsigned long long>(ctxArgs->context),
        ctxArgs->commTag, dfxOpInfo);
    uint32_t ret = AicpuIndopProcess::AicpuDfxOpInfoInit(dfxOpInfo, ctxArgs->commTag);
    HCCL_RUN_INFO("YYYYYY hcomm dfx [RunAicpuDfxOpInfoInitV2] end, ret[%u]", ret);
    return ret;
}
}

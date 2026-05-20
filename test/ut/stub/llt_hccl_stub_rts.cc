/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "acl/acl_rt.h"
#include "rt_external.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum tagRtClearStep {
    RT_STREAM_STOP = 0,
    RT_STREAM_CLEAR,
} rtClearStep_t;

RTS_API rtError_t rtStreamClear(rtStream_t stm, rtClearStep_t step);

RTS_API rtError_t rtGetServerIDBySDID(uint32_t sdid, uint32_t *srvId);

#ifdef __cplusplus
}
#endif

rtError_t rtStreamClear(rtStream_t stm, rtClearStep_t step)
{
    return 0;
}

rtError_t rtGetServerIDBySDID(uint32_t sdid, uint32_t *srvId)
{
    *srvId = 0;
    return 0;
}

RTS_API rtError_t rtUbDevQueryInfo(rtUbDevQueryCmd cmd, void *devInfo)
{
    return 0;
}
 
rtError_t rtCCULaunch(rtCcuTaskInfo_t *taskInfo,  rtStream_t const stm)
{
    return 0;
}

rtError_t rtReleaseDevResAddress(rtDevResInfo * const resInfo)
{
    return RT_ERROR_NONE;
}

rtError_t rtGetDevResAddress(rtDevResInfo * const resInfo, rtDevResAddrInfo * const addrInfo)
{
    return RT_ERROR_NONE;
}

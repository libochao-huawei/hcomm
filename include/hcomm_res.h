/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
 
#ifndef HCOMM_RES_H
#define HCOMM_RES_H
 
#include <hcomm_res_defs.h>
 
#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

// experimental API, No compatibility is currently guaranteed for this API
extern HcclResult HcommEndpointCreate(const EndpointDesc *endPoint, EndpointHandle *endPointHandle);

// experimental API, No compatibility is currently guaranteed for this API
extern HcclResult HcommEndpointDestroy(EndpointHandle endPointHandle);

// experimental API, No compatibility is currently guaranteed for this API
extern HcclResult HcommMemReg(EndpointHandle endPointHandle, const char *memTag, HcommMem mem, void **memHandle);

// experimental API, No compatibility is currently guaranteed for this API
extern HcclResult HcommMemUnreg(EndpointHandle endPointHandle, void *memHandle);

// experimental API, No compatibility is currently guaranteed for this API
extern HcclResult HcommMemExport(
    EndpointHandle endPointHandle, const void *memHandle, void **memDesc, uint32_t *memDescLen);

// experimental API, No compatibility is currently guaranteed for this API
extern HcclResult HcommMemImport(
    EndpointHandle endPointHandle, const void *memDesc, uint32_t descLen, HcommBuf *outBuf);

// experimental API, No compatibility is currently guaranteed for this API
extern HcclResult HcommMemUnimport(EndpointHandle endPointHandle, const HcommBuf *buf);

// experimental API, No compatibility is currently guaranteed for this API
extern HcclResult HcommChannelCreate(EndpointHandle endPointHandle, CommEngine engine, HcommChannelDesc *channelDescs,
    uint32_t channelNum, ChannelHandle *channels);

// experimental API, No compatibility is currently guaranteed for this API
extern HcclResult HcommChannelGetStatus(const ChannelHandle *channelList, uint32_t listNum, int32_t *statusList);

// experimental API, No compatibility is currently guaranteed for this API
extern HcclResult HcommChannelGetNotifyNum(ChannelHandle channel, uint32_t *notifyNum);

// experimental API, No compatibility is currently guaranteed for this API
extern HcclResult HcommChannelDestroy(const ChannelHandle *channels, uint32_t channelNum);

// experimental API, No compatibility is currently guaranteed for this API
extern HcclResult HcommChannelGetRemoteMem(ChannelHandle channel, HcommMem **remoteMem, uint32_t *memNum, char **memTags);

// experimental API, No compatibility is currently guaranteed for this API
extern HcclResult HcommThreadAlloc(CommEngine engine, uint32_t threadNum, uint32_t notifyNumPerThread, ThreadHandle *threads);

// experimental API, No compatibility is currently guaranteed for this API
extern HcclResult HcommThreadFree(const ThreadHandle *threads, uint32_t threadNum);

#ifdef __cplusplus
}
#endif // __cplusplus
#endif // HCOMM_RES_H_
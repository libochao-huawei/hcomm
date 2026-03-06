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

// WARNING: experimental API, No compatibility is currently guaranteed for this API
extern HcclResult HcommEndpointCreate(const EndpointDesc *endpoint, EndpointHandle *endpointHandle);

// WARNING: experimental API, No compatibility is currently guaranteed for this API
extern HcclResult HcommEndpointDestroy(EndpointHandle endpointHandle);

// WARNING: experimental API, No compatibility is currently guaranteed for this API
extern HcclResult HcommMemReg(EndpointHandle endpointHandle, const char *memTag, HcommMem mem, void **memHandle);

// WARNING: experimental API, No compatibility is currently guaranteed for this API
extern HcclResult HcommMemUnreg(EndpointHandle endpointHandle, void *memHandle);

// WARNING: experimental API, No compatibility is currently guaranteed for this API
extern HcclResult HcommMemExport(EndpointHandle endpointHandle, void *memHandle, void **memDesc, uint32_t *memDescLen);

// WARNING: experimental API, No compatibility is currently guaranteed for this API
extern HcclResult HcommMemImport(EndpointHandle endpointHandle, const void *memDesc, uint32_t descLen, HcommMem *outMem);

// WARNING: experimental API, No compatibility is currently guaranteed for this API
extern HcclResult HcommMemUnimport(EndpointHandle endpointHandle, const void *memDesc, uint32_t descLen);

// WARNING: experimental API, No compatibility is currently guaranteed for this API
extern HcclResult HcommChannelCreate(EndpointHandle endpointHandle, CommEngine engine, HcommChannelDesc *channelDescs,
    uint32_t channelNum, ChannelHandle *channels);

// WARNING: experimental API, No compatibility is currently guaranteed for this API
extern HcclResult HcommChannelGetStatus(const ChannelHandle *channelList, uint32_t listNum, int32_t *statusList);

// WARNING: experimental API, No compatibility is currently guaranteed for this API
extern HcclResult HcommChannelGetNotifyNum(ChannelHandle channelHandle, uint32_t *notifyNum);

// WARNING: experimental API, No compatibility is currently guaranteed for this API
extern HcclResult HcommChannelDestroy(const ChannelHandle *channels, uint32_t channelNum);

// WARNING: experimental API, No compatibility is currently guaranteed for this API
extern HcclResult HcommChannelGetRemoteMem(ChannelHandle channel, HcommMem **remoteMem, uint32_t *memNum, char **memTags);

// WARNING: experimental API, No compatibility is currently guaranteed for this API
extern HcclResult HcommThreadAlloc(CommEngine engine, uint32_t threadNum, uint32_t notifyNumPerThread, ThreadHandle *threads);

// WARNING: experimental API, No compatibility is currently guaranteed for this API
extern HcclResult HcommThreadFree(const ThreadHandle *threads, uint32_t threadNum);

#ifdef __cplusplus
}
#endif // __cplusplus
#endif // HCOMM_RES_H_
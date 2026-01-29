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
 
extern HcclResult HcommEndpointGet(HcommDevId deviceId, EndpointDesc **endpointDescs, uint32_t *num);
 
extern HcclResult HcommEndpointCreate(const EndpointDesc *endPoint, EndpointHandle *endPointHandle);
 
extern HcclResult HcommEndpointDestroy(EndpointHandle endPointHandle);
 
extern HcclResult HcommMemReg(EndpointHandle endPointHandle, const char *memTag, HcommMem mem, void **memHandle);
 
extern HcclResult HcommMemUnreg(EndpointHandle endPointHandle, void *memHandle);
 
extern HcclResult HcommMemExport(
    EndpointHandle endPointHandle, const void *memHandle, void **memDesc, uint32_t *memDescLen);
 
extern HcclResult HcommMemImport(
    EndpointHandle endpointHandle, const void *memDesc, uint32_t descLen, HcommMem *outMem);

extern HcclResult HcommMemUnimport(EndpointHandle endpointHandle, const void *memDesc, uint32_t descLen);
 
/* 暂未实现 */
extern HcclResult HcommMemGrant(EndpointHandle endPointHandle, const HcommMemGrantInfo *remoteGrantInfo);
 
/* 暂未实现 */
extern HcclResult HcommMemRemap(const EndpointHandle endPointHandle, const HcommMem *memArray, uint64_t arraySize);
 
extern HcclResult HcommMemGetAllMemHandles(EndpointHandle endPointHandle, void **memHandles, uint32_t *memHandleNum);
 
extern HcclResult HcommChannelCreate(EndpointHandle endPointHandle, CommEngine engine, HcommChannelDesc *channelDescs,
    uint32_t channelNum, ChannelHandle *channels);
 
extern HcclResult HcommChannelGetStatus(const ChannelHandle *channelList, uint32_t listNum, int32_t *statusList);
 
extern HcclResult HcommChannelGetNotifyNum(ChannelHandle channel, uint32_t *notifyNum);
 
extern HcclResult HcommChannelDestroy(const ChannelHandle *channels, uint32_t channelNum);
 
extern HcclResult HcommChannelGetRemoteMem(ChannelHandle channel, HcommMem **remoteMem, uint32_t *memNum, char **memTags);
 
extern HcclResult HcommThreadAlloc(CommEngine engine, uint32_t threadNum, uint32_t notifyNumPerThread, ThreadHandle *threads);

extern HcclResult HcommThreadFree(const ThreadHandle *threads, uint32_t threadNum);

extern HcclResult HcommThreadAllocWithStream(CommEngine engine, void *stream, uint32_t notifyNum, ThreadHandle *thread);
#ifdef __cplusplus
}
#endif // __cplusplus
#endif // HCOMM_RES_H_
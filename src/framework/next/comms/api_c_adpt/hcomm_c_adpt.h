/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
 
#ifndef HCOMM_C_ADPT_H
#define HCOMM_C_ADPT_H
 
#include "hcomm_res_defs.h"
#include "hccl/hccl_res.h"
#include "mem_host_pub.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

HcclResult HcommEndpointCreate(const EndpointDesc *endpoint, EndpointHandle *endpointHandle);

HcclResult HcommEndpointDestroy(EndpointHandle endPointHandle);

HcclResult HcommMemReg(EndpointHandle endpointHandle, const char *memTag, HcommMem mem, void **memHandle);

HcclResult HcommMemUnreg(EndpointHandle endPointHandle, void *memHandle);

HcclResult HcommMemExport(
    EndpointHandle endPointHandle, const void *memHandle, void **memDesc, uint32_t *memDescLen);

HcclResult HcommMemImport(
    EndpointHandle endPointHandle, const void *memDesc, uint32_t descLen, HcommBuf *outBuf);

HcclResult HcommMemUnimport(EndpointHandle endPointHandle, const HcommBuf *buf);

HcclResult HcommChannelCreate(EndpointHandle endpointHandle, CommEngine engine, HcommChannelDesc *channelDescs,
    uint32_t channelNum, ChannelHandle *channels);

HcclResult HcommChannelGetStatus(const ChannelHandle *channelList, uint32_t listNum,  int32_t* statusList);

HcclResult HcommChannelDestroy(const ChannelHandle *channels, uint32_t channelNum);

HcclResult HcommChannelKernelLaunch(ChannelHandle *channelHandles, ChannelHandle *hostChannelHandles, uint32_t listNum,
    const std::string &commTag, aclrtBinHandle binHandle);

HcclResult HcclChannelGetHcclBufferA5(HcclComm comm, ChannelHandle channel, void **buffer, uint64_t *size);

HcclResult HcommThreadAlloc(CommEngine engine, uint32_t threadNum, uint32_t notifyNumPerThread, ThreadHandle *threads);

HcclResult HcommThreadFree(const ThreadHandle *threads, uint32_t threadNum);

#ifdef __cplusplus
}
#endif // __cplusplus


#endif
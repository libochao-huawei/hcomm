/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "hcomm_nic_plugin.h"

#include <memory>
#include <new>
#include "cpu_roce_endpoint.h"
#include "hccl_mem_defs.h"
#include "host_cpu_roce_channel.h"
#include "log.h"
#include "param_check_pub.h"

namespace {
using hcomm_host_nic::Channel;
using hcomm_host_nic::CpuRoceEndpoint;
using hcomm_host_nic::HostCpuRoceChannel;

int32_t InitEndpoint(void *endpointCtx)
{
    CHK_PTR_NULL(endpointCtx);
    return static_cast<CpuRoceEndpoint *>(endpointCtx)->Init();
}

HcommResult CreateEndpoint(const EndpointDesc *endpoint, void **endpointCtx)
{
    CHK_PTR_NULL(endpoint);
    CHK_PTR_NULL(endpointCtx);
    auto ep = std::unique_ptr<CpuRoceEndpoint>(new (std::nothrow) CpuRoceEndpoint(*endpoint));
    CHK_SMART_PTR_NULL(ep);
    *endpointCtx = ep.release();
    return HCCL_SUCCESS;
}

void DestroyEndpoint(void *endpointCtx)
{
    delete static_cast<CpuRoceEndpoint *>(endpointCtx);
}

HcommResult StartListen(void *endpointCtx, uint32_t port, HcommEndpointListenConfig *config)
{
    (void)config;
    CHK_PTR_NULL(endpointCtx);
    return static_cast<CpuRoceEndpoint *>(endpointCtx)->ServerSocketListen(port);
}

HcommResult StopListen(void *endpointCtx, uint32_t port)
{
    CHK_PTR_NULL(endpointCtx);
    return static_cast<CpuRoceEndpoint *>(endpointCtx)->ServerSocketStopListen(port);
}

HcommResult GetListenPort(void *endpointCtx, uint32_t *port)
{
    CHK_PTR_NULL(endpointCtx);
    return static_cast<CpuRoceEndpoint *>(endpointCtx)->ServerSocketGetListenPort(port);
}

HcommResult MemReg(void *endpointCtx, const CommMem *mem, const char *memTag, void **memHandle)
{
    CHK_PTR_NULL(endpointCtx);
    CHK_PTR_NULL(mem);
    CHK_PTR_NULL(memHandle);
    return static_cast<CpuRoceEndpoint *>(endpointCtx)->RegisterMemory(*mem, memTag, memHandle);
}

HcommResult MemUnreg(void *endpointCtx, void *memHandle)
{
    CHK_PTR_NULL(endpointCtx);
    return static_cast<CpuRoceEndpoint *>(endpointCtx)->UnregisterMemory(memHandle);
}

HcommResult MemExport(void *endpointCtx, void *memHandle, void **memDesc, uint32_t *memDescLen)
{
    CHK_PTR_NULL(endpointCtx);
    return static_cast<CpuRoceEndpoint *>(endpointCtx)->MemoryExport(memHandle, memDesc, memDescLen);
}

HcommResult MemImport(void *endpointCtx, const void *memDesc, uint32_t descLen, CommMem *outMem)
{
    CHK_PTR_NULL(endpointCtx);
    return static_cast<CpuRoceEndpoint *>(endpointCtx)->MemoryImport(memDesc, descLen, outMem);
}

HcommResult MemUnimport(void *endpointCtx, const void *memDesc, uint32_t descLen)
{
    CHK_PTR_NULL(endpointCtx);
    return static_cast<CpuRoceEndpoint *>(endpointCtx)->MemoryUnimport(memDesc, descLen);
}

HcommResult MemGetAllHandles(void *endpointCtx, void **memHandles, uint32_t *memHandleNum)
{
    CHK_PTR_NULL(endpointCtx);
    return static_cast<CpuRoceEndpoint *>(endpointCtx)->GetAllMemHandles(memHandles, memHandleNum);
}

HcommResult MemGrant(void *endpointCtx, const HcommMemGrantInfo *remoteGrantInfo)
{
    (void)endpointCtx;
    (void)remoteGrantInfo;
    return HCCL_SUCCESS;
}

int32_t InitChannel(void *channelCtx)
{
    CHK_PTR_NULL(channelCtx);
    return static_cast<HostCpuRoceChannel *>(channelCtx)->Init();
}

HcommResult CreateChannel(void *endpointCtx, const HcommChannelDesc *channelDesc, void **channelCtx)
{
    CHK_PTR_NULL(endpointCtx);
    CHK_PTR_NULL(channelDesc);
    CHK_PTR_NULL(channelCtx);
    auto ch = std::unique_ptr<HostCpuRoceChannel>(
        new (std::nothrow) HostCpuRoceChannel(reinterpret_cast<EndpointHandle>(endpointCtx), *channelDesc));
    CHK_SMART_PTR_NULL(ch);
    *channelCtx = ch.release();
    return HCCL_SUCCESS;
}

void DestroyChannel(void *channelCtx)
{
    delete static_cast<HostCpuRoceChannel *>(channelCtx);
}

HcommResult GetStatus(void *channelCtx, int32_t *status)
{
    CHK_PTR_NULL(channelCtx);
    CHK_PTR_NULL(status);
    auto channelStatus = static_cast<HostCpuRoceChannel *>(channelCtx)->GetStatus();
    *status = (channelStatus == hcomm_host_nic::ChannelStatus::READY) ? 0 : 1;
    if (channelStatus == hcomm_host_nic::ChannelStatus::FAILED || channelStatus == hcomm_host_nic::ChannelStatus::SOCKET_TIMEOUT) {
        return HCCL_E_NETWORK;
    }
    return HCCL_SUCCESS;
}

HcommResult GetNotifyNum(void *channelCtx, uint32_t *notifyNum)
{
    CHK_PTR_NULL(channelCtx);
    return static_cast<HostCpuRoceChannel *>(channelCtx)->GetNotifyNum(notifyNum);
}

HcommResult GetRemoteMem(void *channelCtx, CommMem **remoteMem, uint32_t *memNum, char **memTags)
{
    CHK_PTR_NULL(channelCtx);
    return static_cast<HostCpuRoceChannel *>(channelCtx)->GetRemoteMem(reinterpret_cast<HcclMem **>(remoteMem),
        memNum, memTags);
}

HcommResult CleanChannel(void *channelCtx)
{
    (void)channelCtx;
    return HCCL_SUCCESS;
}

HcommResult GetUserRemoteMem(void *channelCtx, CommMem **remoteMem, char ***memTags, uint32_t *memNum)
{
    CHK_PTR_NULL(channelCtx);
    return static_cast<HostCpuRoceChannel *>(channelCtx)->GetUserRemoteMem(remoteMem, memTags, memNum);
}

HcommResult UpdateMemInfo(void *channelCtx, HcommMemHandle *memHandles, uint32_t memHandleNum)
{
    CHK_PTR_NULL(channelCtx);
    return static_cast<HostCpuRoceChannel *>(channelCtx)->UpdateMemInfo(memHandles, memHandleNum);
}

HcommResult NotifyRecord(void *channelCtx, ThreadHandle thread, uint32_t remoteNotifyIdx)
{
    (void)thread;
    CHK_PTR_NULL(channelCtx);
    return static_cast<HostCpuRoceChannel *>(channelCtx)->NotifyRecord(remoteNotifyIdx);
}

HcommResult NotifyWait(void *channelCtx, ThreadHandle thread, uint32_t localNotifyIdx, uint32_t timeout)
{
    (void)thread;
    CHK_PTR_NULL(channelCtx);
    return static_cast<HostCpuRoceChannel *>(channelCtx)->NotifyWait(localNotifyIdx, timeout);
}

HcommResult WriteWithNotify(void *channelCtx, ThreadHandle thread, void *dst, const void *src, uint64_t len,
    uint32_t remoteNotifyIdx)
{
    (void)thread;
    CHK_PTR_NULL(channelCtx);
    return static_cast<HostCpuRoceChannel *>(channelCtx)->WriteWithNotify(dst, src, len, remoteNotifyIdx);
}

HcommResult Write(void *channelCtx, ThreadHandle thread, void *dst, const void *src, uint64_t len)
{
    (void)thread;
    CHK_PTR_NULL(channelCtx);
    return static_cast<HostCpuRoceChannel *>(channelCtx)->Write(dst, src, len);
}

HcommResult Read(void *channelCtx, ThreadHandle thread, void *dst, const void *src, uint64_t len)
{
    (void)thread;
    CHK_PTR_NULL(channelCtx);
    return static_cast<HostCpuRoceChannel *>(channelCtx)->Read(dst, src, len);
}

HcommResult Fence(void *channelCtx, ThreadHandle thread)
{
    (void)thread;
    CHK_PTR_NULL(channelCtx);
    return static_cast<HostCpuRoceChannel *>(channelCtx)->ChannelFence();
}

HcommResult UnsupportedWriteReduce(void *, ThreadHandle, void *, const void *, uint64_t, int32_t, int32_t)
{
    return HCCL_E_NOT_SUPPORT;
}

HcommResult UnsupportedWriteReduceWithNotify(void *, ThreadHandle, void *, const void *, uint64_t, int32_t, int32_t,
    uint32_t)
{
    return HCCL_E_NOT_SUPPORT;
}

HcommResult UnsupportedReadReduce(void *, ThreadHandle, void *, const void *, uint64_t, int32_t, int32_t)
{
    return HCCL_E_NOT_SUPPORT;
}

HcommResult UnsupportedBatch(void *, ThreadHandle, const HcommBatchTransferDesc *, uint32_t)
{
    return HCCL_E_NOT_SUPPORT;
}

HcommResult WriteNbiNt(void *channelCtx, void *dst, const void *src, uint64_t len)
{
    return Write(channelCtx, 0, dst, src, len);
}

HcommResult WriteWithNotifyNbiNt(void *channelCtx, void *dst, const void *src, uint64_t len,
    uint32_t remoteNotifyIdx)
{
    return WriteWithNotify(channelCtx, 0, dst, src, len, remoteNotifyIdx);
}

HcommResult ReadNbiNt(void *channelCtx, void *dst, const void *src, uint64_t len)
{
    return Read(channelCtx, 0, dst, src, len);
}

HcommResult NotifyRecordNt(void *channelCtx, uint32_t remoteNotifyIdx)
{
    return NotifyRecord(channelCtx, 0, remoteNotifyIdx);
}

HcommResult NotifyWaitNt(void *channelCtx, uint32_t localNotifyIdx, uint32_t timeout)
{
    return NotifyWait(channelCtx, 0, localNotifyIdx, timeout);
}

HcommResult FenceNt(void *channelCtx)
{
    return Fence(channelCtx, 0);
}

const HcommNicPluginInfo kPluginInfo = {
    HCOMM_NIC_PLUGIN_ABI_VERSION,
    "hcomm_host_roce",
    1U,
    {HCOMM_NIC_PROTO_ROCE, HCOMM_NIC_PROTO_HCCS, HCOMM_NIC_PROTO_HCCS, HCOMM_NIC_PROTO_HCCS}
};

HcommNicEndpointOps kEndpointOps = {
    InitEndpoint,
    MemReg,
    MemUnreg,
    MemExport,
    MemImport,
    MemUnimport,
    DestroyEndpoint,
    StartListen,
    StopListen,
    GetListenPort,
    MemGetAllHandles,
    MemGrant
};

HcommNicChannelOps kChannelOps = {
    InitChannel,
    GetNotifyNum,
    GetRemoteMem,
    CleanChannel,
    Write,
    UnsupportedWriteReduce,
    WriteWithNotify,
    UnsupportedWriteReduceWithNotify,
    Read,
    UnsupportedReadReduce,
    Write,
    WriteWithNotify,
    Read,
    UnsupportedBatch,
    NotifyRecord,
    NotifyWait,
    Fence,
    WriteNbiNt,
    WriteWithNotifyNbiNt,
    ReadNbiNt,
    NotifyRecordNt,
    NotifyWaitNt,
    FenceNt,
    DestroyChannel,
    GetStatus,
    UpdateMemInfo,
    GetUserRemoteMem
};
}

extern "C" const HcommNicPluginInfo *HcommNicPluginGetInfo(void)
{
    return &kPluginInfo;
}

extern "C" int32_t HcommNicPluginCreateEndpoint(const void *endpointDescRaw, uint32_t epDescLen, void **outCtx,
    HcommNicEndpointOps **outOps)
{
    CHK_PTR_NULL(endpointDescRaw);
    CHK_PTR_NULL(outCtx);
    CHK_PTR_NULL(outOps);
    CHK_PRT_RET(epDescLen < sizeof(EndpointDesc), HCCL_ERROR("[HostRocePlugin] invalid endpoint desc len[%u].",
        epDescLen), HCCL_E_PARA);
    CHK_RET(static_cast<HcclResult>(CreateEndpoint(static_cast<const EndpointDesc *>(endpointDescRaw), outCtx)));
    *outOps = &kEndpointOps;
    return HCCL_SUCCESS;
}

extern "C" int32_t HcommNicPluginCreateChannel(void *epCtx, const void *channelDescRaw, uint32_t chDescLen,
    void **outCtx, HcommNicChannelOps **outOps)
{
    CHK_PTR_NULL(channelDescRaw);
    CHK_PTR_NULL(outCtx);
    CHK_PTR_NULL(outOps);
    CHK_PRT_RET(chDescLen < sizeof(HcommChannelDesc), HCCL_ERROR("[HostRocePlugin] invalid channel desc len[%u].",
        chDescLen), HCCL_E_PARA);
    CHK_RET(static_cast<HcclResult>(CreateChannel(epCtx, static_cast<const HcommChannelDesc *>(channelDescRaw), outCtx)));
    *outOps = &kChannelOps;
    return HCCL_SUCCESS;
}

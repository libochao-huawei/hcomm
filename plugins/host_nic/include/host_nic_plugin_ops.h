/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef HCOMM_HOST_NIC_PLUGIN_OPS_H
#define HCOMM_HOST_NIC_PLUGIN_OPS_H

#include "hcomm_nic_plugin.h"

#include <memory>
#include <new>
#include "channel.h"
#include "hccl_mem_defs.h"
#include "log.h"
#include "param_check_pub.h"

namespace hcomm_host_nic {

template <typename Traits>
class HostNicPluginOps {
public:
    using EndpointT = typename Traits::EndpointT;
    using ChannelT = typename Traits::ChannelT;

    static int32_t InitEndpoint(void *endpointCtx)
    {
        CHK_PTR_NULL(endpointCtx);
        return static_cast<EndpointT *>(endpointCtx)->Init();
    }

    static HcommResult CreateEndpoint(const EndpointDesc *endpoint, void **endpointCtx)
    {
        CHK_PTR_NULL(endpoint);
        CHK_PTR_NULL(endpointCtx);
        auto ep = std::unique_ptr<EndpointT>(new (std::nothrow) EndpointT(*endpoint));
        CHK_SMART_PTR_NULL(ep);
        *endpointCtx = ep.release();
        return HCCL_SUCCESS;
    }

    static void DestroyEndpoint(void *endpointCtx)
    {
        delete static_cast<EndpointT *>(endpointCtx);
    }

    static HcommResult StartListen(void *endpointCtx, uint32_t port, HcommEndpointListenConfig *config)
    {
        (void)config;
        CHK_PTR_NULL(endpointCtx);
        return static_cast<EndpointT *>(endpointCtx)->ServerSocketListen(port);
    }

    static HcommResult StopListen(void *endpointCtx, uint32_t port)
    {
        CHK_PTR_NULL(endpointCtx);
        return static_cast<EndpointT *>(endpointCtx)->ServerSocketStopListen(port);
    }

    static HcommResult GetListenPort(void *endpointCtx, uint32_t *port)
    {
        CHK_PTR_NULL(endpointCtx);
        return static_cast<EndpointT *>(endpointCtx)->ServerSocketGetListenPort(port);
    }

    static HcommResult MemReg(void *endpointCtx, const CommMem *mem, const char *memTag, void **memHandle)
    {
        CHK_PTR_NULL(endpointCtx);
        CHK_PTR_NULL(mem);
        CHK_PTR_NULL(memHandle);
        return static_cast<EndpointT *>(endpointCtx)->RegisterMemory(*mem, memTag, memHandle);
    }

    static HcommResult MemUnreg(void *endpointCtx, void *memHandle)
    {
        CHK_PTR_NULL(endpointCtx);
        return static_cast<EndpointT *>(endpointCtx)->UnregisterMemory(memHandle);
    }

    static HcommResult MemExport(void *endpointCtx, void *memHandle, void **memDesc, uint32_t *memDescLen)
    {
        CHK_PTR_NULL(endpointCtx);
        return static_cast<EndpointT *>(endpointCtx)->MemoryExport(memHandle, memDesc, memDescLen);
    }

    static HcommResult MemImport(void *endpointCtx, const void *memDesc, uint32_t descLen, CommMem *outMem)
    {
        CHK_PTR_NULL(endpointCtx);
        return static_cast<EndpointT *>(endpointCtx)->MemoryImport(memDesc, descLen, outMem);
    }

    static HcommResult MemUnimport(void *endpointCtx, const void *memDesc, uint32_t descLen)
    {
        CHK_PTR_NULL(endpointCtx);
        return static_cast<EndpointT *>(endpointCtx)->MemoryUnimport(memDesc, descLen);
    }

    static HcommResult MemGetAllHandles(void *endpointCtx, void **memHandles, uint32_t *memHandleNum)
    {
        CHK_PTR_NULL(endpointCtx);
        return static_cast<EndpointT *>(endpointCtx)->GetAllMemHandles(memHandles, memHandleNum);
    }

    static HcommResult MemGrant(void *endpointCtx, const HcommMemGrantInfo *remoteGrantInfo)
    {
        (void)endpointCtx;
        (void)remoteGrantInfo;
        return HCCL_SUCCESS;
    }

    static int32_t InitChannel(void *channelCtx)
    {
        CHK_PTR_NULL(channelCtx);
        return static_cast<ChannelT *>(channelCtx)->Init();
    }

    static HcommResult CreateChannel(void *endpointCtx, const HcommChannelDesc *channelDesc, void **channelCtx)
    {
        CHK_PTR_NULL(endpointCtx);
        CHK_PTR_NULL(channelDesc);
        CHK_PTR_NULL(channelCtx);
        auto ch = std::unique_ptr<ChannelT>(
            new (std::nothrow) ChannelT(reinterpret_cast<EndpointHandle>(endpointCtx), *channelDesc));
        CHK_SMART_PTR_NULL(ch);
        *channelCtx = ch.release();
        return HCCL_SUCCESS;
    }

    static void DestroyChannel(void *channelCtx)
    {
        delete static_cast<ChannelT *>(channelCtx);
    }

    static HcommResult GetStatus(void *channelCtx, int32_t *status)
    {
        CHK_PTR_NULL(channelCtx);
        CHK_PTR_NULL(status);
        auto channelStatus = static_cast<ChannelT *>(channelCtx)->GetStatus();
        *status = (channelStatus == ChannelStatus::READY) ? 0 : 1;
        if (channelStatus == ChannelStatus::FAILED || channelStatus == ChannelStatus::SOCKET_TIMEOUT) {
            return HCCL_E_NETWORK;
        }
        return HCCL_SUCCESS;
    }

    static HcommResult GetNotifyNum(void *channelCtx, uint32_t *notifyNum)
    {
        CHK_PTR_NULL(channelCtx);
        return static_cast<ChannelT *>(channelCtx)->GetNotifyNum(notifyNum);
    }

    static HcommResult GetRemoteMem(void *channelCtx, CommMem **remoteMem, uint32_t *memNum, char **memTags)
    {
        CHK_PTR_NULL(channelCtx);
        return static_cast<ChannelT *>(channelCtx)->GetRemoteMem(reinterpret_cast<HcclMem **>(remoteMem),
            memNum, memTags);
    }

    static HcommResult CleanChannel(void *channelCtx)
    {
        (void)channelCtx;
        return HCCL_SUCCESS;
    }

    static HcommResult GetUserRemoteMem(void *channelCtx, CommMem **remoteMem, char ***memTags, uint32_t *memNum)
    {
        return Traits::GetUserRemoteMem(static_cast<ChannelT *>(channelCtx), remoteMem, memTags, memNum);
    }

    static HcommResult UpdateMemInfo(void *channelCtx, HcommMemHandle *memHandles, uint32_t memHandleNum)
    {
        CHK_PTR_NULL(channelCtx);
        return static_cast<ChannelT *>(channelCtx)->UpdateMemInfo(memHandles, memHandleNum);
    }

    static HcommResult NotifyRecord(void *channelCtx, ThreadHandle thread, uint32_t remoteNotifyIdx)
    {
        (void)thread;
        CHK_PTR_NULL(channelCtx);
        return static_cast<ChannelT *>(channelCtx)->NotifyRecord(remoteNotifyIdx);
    }

    static HcommResult NotifyWait(void *channelCtx, ThreadHandle thread, uint32_t localNotifyIdx, uint32_t timeout)
    {
        (void)thread;
        CHK_PTR_NULL(channelCtx);
        return static_cast<ChannelT *>(channelCtx)->NotifyWait(localNotifyIdx, timeout);
    }

    static HcommResult WriteWithNotify(void *channelCtx, ThreadHandle thread, void *dst, const void *src, uint64_t len,
        uint32_t remoteNotifyIdx)
    {
        (void)thread;
        CHK_PTR_NULL(channelCtx);
        return static_cast<ChannelT *>(channelCtx)->WriteWithNotify(dst, src, len, remoteNotifyIdx);
    }

    static HcommResult Write(void *channelCtx, ThreadHandle thread, void *dst, const void *src, uint64_t len)
    {
        (void)thread;
        CHK_PTR_NULL(channelCtx);
        return static_cast<ChannelT *>(channelCtx)->Write(dst, src, len);
    }

    static HcommResult Read(void *channelCtx, ThreadHandle thread, void *dst, const void *src, uint64_t len)
    {
        (void)thread;
        CHK_PTR_NULL(channelCtx);
        return static_cast<ChannelT *>(channelCtx)->Read(dst, src, len);
    }

    static HcommResult Fence(void *channelCtx, ThreadHandle thread)
    {
        (void)thread;
        CHK_PTR_NULL(channelCtx);
        return static_cast<ChannelT *>(channelCtx)->ChannelFence();
    }

    static HcommResult UnsupportedWriteReduce(void *, ThreadHandle, void *, const void *, uint64_t, int32_t, int32_t)
    {
        return HCCL_E_NOT_SUPPORT;
    }

    static HcommResult UnsupportedWriteReduceWithNotify(void *, ThreadHandle, void *, const void *, uint64_t, int32_t,
        int32_t, uint32_t)
    {
        return HCCL_E_NOT_SUPPORT;
    }

    static HcommResult UnsupportedReadReduce(void *, ThreadHandle, void *, const void *, uint64_t, int32_t, int32_t)
    {
        return HCCL_E_NOT_SUPPORT;
    }

    static HcommResult UnsupportedBatch(void *, ThreadHandle, const HcommBatchTransferDesc *, uint32_t)
    {
        return HCCL_E_NOT_SUPPORT;
    }

    static HcommResult WriteNbiNt(void *channelCtx, void *dst, const void *src, uint64_t len)
    {
        return Write(channelCtx, 0, dst, src, len);
    }

    static HcommResult WriteWithNotifyNbiNt(void *channelCtx, void *dst, const void *src, uint64_t len,
        uint32_t remoteNotifyIdx)
    {
        return WriteWithNotify(channelCtx, 0, dst, src, len, remoteNotifyIdx);
    }

    static HcommResult ReadNbiNt(void *channelCtx, void *dst, const void *src, uint64_t len)
    {
        return Read(channelCtx, 0, dst, src, len);
    }

    static HcommResult NotifyRecordNt(void *channelCtx, uint32_t remoteNotifyIdx)
    {
        return NotifyRecord(channelCtx, 0, remoteNotifyIdx);
    }

    static HcommResult NotifyWaitNt(void *channelCtx, uint32_t localNotifyIdx, uint32_t timeout)
    {
        return NotifyWait(channelCtx, 0, localNotifyIdx, timeout);
    }

    static HcommResult FenceNt(void *channelCtx)
    {
        return Fence(channelCtx, 0);
    }

    static HcommNicEndpointOps EndpointOps()
    {
        return {
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
    }

    static HcommNicChannelOps ChannelOps()
    {
        return {
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

    static int32_t CreateEndpointExport(const void *endpointDescRaw, uint32_t epDescLen, void **outCtx,
        HcommNicEndpointOps **outOps, HcommNicEndpointOps *endpointOps)
    {
        CHK_PTR_NULL(endpointDescRaw);
        CHK_PTR_NULL(outCtx);
        CHK_PTR_NULL(outOps);
        CHK_PRT_RET(epDescLen < sizeof(EndpointDesc),
            HCCL_ERROR("[%s] invalid endpoint desc len[%u].", Traits::LogTag(), epDescLen), HCCL_E_PARA);
        CHK_RET(static_cast<HcclResult>(CreateEndpoint(static_cast<const EndpointDesc *>(endpointDescRaw), outCtx)));
        *outOps = endpointOps;
        return HCCL_SUCCESS;
    }

    static int32_t CreateChannelExport(void *epCtx, const void *channelDescRaw, uint32_t chDescLen, void **outCtx,
        HcommNicChannelOps **outOps, HcommNicChannelOps *channelOps)
    {
        CHK_PTR_NULL(channelDescRaw);
        CHK_PTR_NULL(outCtx);
        CHK_PTR_NULL(outOps);
        CHK_PRT_RET(chDescLen < sizeof(HcommChannelDesc),
            HCCL_ERROR("[%s] invalid channel desc len[%u].", Traits::LogTag(), chDescLen), HCCL_E_PARA);
        CHK_RET(static_cast<HcclResult>(
            CreateChannel(epCtx, static_cast<const HcommChannelDesc *>(channelDescRaw), outCtx)));
        *outOps = channelOps;
        return HCCL_SUCCESS;
    }
};

} // namespace hcomm_host_nic

#endif // HCOMM_HOST_NIC_PLUGIN_OPS_H

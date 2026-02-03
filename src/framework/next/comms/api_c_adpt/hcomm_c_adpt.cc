/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <mutex>

#include "hccl_api.h"
#include "hcomm_res.h"
#include "hcomm_res_defs.h"
#include "log.h"
#include "hcomm_c_adpt.h"
#include "../endpoints/endpoint.h"
#include "../endpoint_pairs/channels/channel.h"
#include "thread.h"
#include "aicpu_ts_thread.h"
#include "cpu_ts_thread.h"
#include "aicpu_ts_urma_channel.h"
#include "mem_device_pub.h"
#include "channel_param.h"
#include "launch_aicpu.h"
#include "comm_configer.h"
#include "endpoint_map.h"
#include "endpoint_map.h"

namespace hcomm {
static std::unordered_map<ChannelHandle, std::unique_ptr<Channel>> g_ChannelMap;
static std::unordered_map<ChannelHandle, ChannelHandle> g_ChannelD2HMap;
static std::unordered_map<ThreadHandle, std::shared_ptr<hccl::Thread>> g_ThreadMap;

static std::mutex g_ChannelMapMtx;
}  // namespace hcomm

namespace hcomm {

/**
 * @brief 单锁版本：输入任意 channel handle（可能是 device/host），先通过 g_ChannelD2HMap 映射到
 *        真实的 handle，再通过 g_ChannelMap 找到 Channel，并在持锁状态下执行 func。
 *
 * @tparam Func 形如：HcclResult func(Channel& ch)
 */
template <typename Func>
static inline HcclResult WithChannelByHandleLocked(ChannelHandle inHandle, Func &&func)
{
    // 单锁：该锁同时保护 g_ChannelD2HMap 和 g_ChannelMap
    std::lock_guard<std::mutex> lk(hcomm::g_ChannelMapMtx);

    // 1) D2H 映射
    auto itH = hcomm::g_ChannelD2HMap.find(inHandle);
    if (itH == hcomm::g_ChannelD2HMap.end()) {
        HCCL_ERROR("[%s] handle not found in g_ChannelD2HMap, inHandle[0x%llx].", __func__, inHandle);
        return HcclResult::HCCL_E_NOT_FOUND;
    }
    const ChannelHandle mappedHandle = itH->second;

    // 2) ChannelMap 查找
    auto itC = hcomm::g_ChannelMap.find(mappedHandle);
    if (itC == hcomm::g_ChannelMap.end() || !itC->second) {
        HCCL_ERROR("[%s] channel not found in g_ChannelMap, inHandle[0x%llx], mappedHandle[0x%llx].",
            __func__,
            inHandle,
            mappedHandle);
        return HcclResult::HCCL_E_INTERNAL;
    }

    Channel *ch = itC->second.get();
    if (ch == nullptr) {
        HCCL_ERROR(
            "[%s] null channel pointer, inHandle[0x%llx], mappedHandle[0x%llx].", __func__, inHandle, mappedHandle);
        return HcclResult::HCCL_E_INTERNAL;
    }

    // 3) 锁内执行用户逻辑（注意：func 内不要做长耗时/阻塞操作）
    return std::forward<Func>(func)(*ch);
}

}  // namespace hcomm

using namespace hcomm;
static HcommEndpointMap g_EndpointMap;

HcclResult HcommEndpointGet_(EndpointHandle endpointHandle, void **endpoint)  // 根据endpointHandle返回Endpoint对象指针
{
    auto it = g_EndpointMap.GetEndpoint(endpointHandle);
    CHK_PRT_RET(it == nullptr, HCCL_ERROR("[%s] endpoint not found in g_EndpointMap, endpointHandle[%p]",
        __func__, endpointHandle), HCCL_E_PARA);

    *endpoint = static_cast<void *>(it);
    return HCCL_SUCCESS;
}

HcclResult HcommEndpointCreate(const EndpointDesc *endpoint, EndpointHandle *endpointHandle)
{
    if (endpoint->loc.locType != ENDPOINT_LOC_TYPE_DEVICE && endpoint->loc.locType != ENDPOINT_LOC_TYPE_HOST) {
        HCCL_ERROR("[%s] Only support END_POINT_LOCATION_DEVICE AND END_POINT_LOCATION_HOST, but "
                   "endpoint->loc.locType is %d",
            __func__,
            endpoint->loc.locType);
        return HCCL_E_PARA;
    }

    std::unique_ptr<Endpoint> endpointPtr = nullptr;

    CHK_RET(Endpoint::CreateEndpoint(*endpoint, endpointPtr));
    CHK_PTR_NULL(endpointPtr);
    CHK_RET(endpointPtr->Init());

    const EndpointHandle handle = reinterpret_cast<EndpointHandle>(endpointPtr.get());
    CHK_PTR_NULL(handle);
    EXECEPTION_CATCH(g_EndpointMap.AddEndpoint(handle, std::move(endpointPtr)), return HCCL_E_INTERNAL);
    *endpointHandle = handle;

    return HCCL_SUCCESS;
}

HcclResult HcommEndpointDestroy(EndpointHandle endpointHandle)
{
    CHK_PTR_NULL(endpointHandle);

    auto ret = g_EndpointMap.RemoveEndpoint(endpointHandle);
    if (ret == false) {
        HCCL_ERROR("[%s] endpoint not found in g_EndpointMap, endpointHandle[0x%llx]", __func__, endpointHandle);
        return HCCL_E_INTERNAL;
    }
    endpointHandle = nullptr;

    return HCCL_SUCCESS;
}

HcclResult HcommMemReg(EndpointHandle endpointHandle, const char *memTag, HcommMem mem, void **memHandle)
{
    auto endpoint = g_EndpointMap.GetEndpoint(endpointHandle);
    CHK_RET(endpoint->RegisterMemory(mem, memTag, memHandle));
    return HCCL_SUCCESS;
}

HcclResult HcommMemUnreg(EndpointHandle endpointHandle, void *memHandle)
{
    auto endpoint = g_EndpointMap.GetEndpoint(endpointHandle);
    CHK_RET(endpoint->UnregisterMemory(memHandle));
    return HCCL_SUCCESS;
}

HcclResult HcommMemExport(EndpointHandle endpointHandle, void *memHandle, void **memDesc, uint32_t *memDescLen)
{
    auto endpoint = g_EndpointMap.GetEndpoint(endpointHandle);
    CHK_RET(endpoint->MemoryExport(memHandle, memDesc, memDescLen));
    return HCCL_SUCCESS;
}

HcclResult HcommMemImport(EndpointHandle endpointHandle, const void *memDesc, uint32_t descLen, HcommMem *outMem)
{
    auto endpoint = g_EndpointMap.GetEndpoint(endpointHandle);
    CHK_RET(endpoint->MemoryImport(memDesc, descLen, outMem));
    return HCCL_SUCCESS;
}

HcclResult HcommMemUnimport(EndpointHandle endpointHandle, const void *memDesc, uint32_t descLen)
{
    auto endpoint = g_EndpointMap.GetEndpoint(endpointHandle);
    CHK_RET(endpoint->MemoryUnimport(memDesc, descLen));
    return HCCL_SUCCESS;
}

/* 暂未实现 */
HcclResult HcommMemGrant(EndpointHandle endpointHandle, const HcommMemGrantInfo *remoteGrantInfo)
{
    return HCCL_SUCCESS;
}

/* 暂未实现 */
HcclResult HcommMemRemap(const EndpointHandle endpointHandle, const HcommMem *memArray, uint64_t arraySize)
{
    return HCCL_SUCCESS;
}

HcclResult HcommMemGetAllMemHandles(EndpointHandle endpointHandle, void **memHandles, uint32_t *memHandleNum)
{
    return HCCL_SUCCESS;
}

HcclResult HcommChannelCreate(EndpointHandle endpointHandle, CommEngine engine, HcommChannelDesc *channelDescs,
    uint32_t channelNum, ChannelHandle *channels)
{
    CHK_PTR_NULL(channelDescs);
    CHK_PTR_NULL(channels);

    for (uint32_t i = 0; i < channelNum; ++i) {
        // 1) 创建对象：不持全局表锁，避免扩大临界区
        std::unique_ptr<Channel> tmpPtr = nullptr;
        CHK_RET(Channel::CreateChannel(endpointHandle, engine, channelDescs[i], tmpPtr));
        CHK_SMART_PTR_NULL(tmpPtr);

        ChannelHandle handle = reinterpret_cast<ChannelHandle>(tmpPtr.get());
        channels[i] = handle;
        HCCL_INFO("%s handle[0x%llx], ptr[%p]", __func__, handle, tmpPtr.get());

        // 2) 仅在修改全局表时持锁（该锁同时保护两张表）
        {
            std::lock_guard<std::mutex> lk(hcomm::g_ChannelMapMtx);

            if (hcomm::g_ChannelMap.find(handle) != hcomm::g_ChannelMap.end()) {
                HCCL_ERROR("[%s] channel handle already exists [0x%llx] in ChannelMap", __func__, handle);
                return HCCL_E_INTERNAL;
            }
            if (hcomm::g_ChannelD2HMap.find(handle) != hcomm::g_ChannelD2HMap.end()) {
                HCCL_ERROR("[%s] channel handle already exists [0x%llx] in g_ChannelD2HMap", __func__, handle);
                return HCCL_E_INTERNAL;
            }

            hcomm::g_ChannelMap.emplace(handle, std::move(tmpPtr));
            hcomm::g_ChannelD2HMap.emplace(handle, handle);
        }
    }

    return HCCL_SUCCESS;
}

static HcclResult CombineHostMemory(const std::vector<std::vector<char>> &hostPackBuffers, hccl::HostMem &hostPackBuf)
{
    if (hostPackBuffers.empty()) {
        HCCL_ERROR("[%s] hostPackBuffers is empty, please check.", __func__);
        return HCCL_E_PARA;
    }

    // 将离散数据复制到连续内存中
    u8 *dstPtr = static_cast<u8 *>(hostPackBuf.ptr());  // 目标内存起始地址
    u64 dstMax = hostPackBuf.size();
    u64 packSize = 0;

    for (const auto &mem : hostPackBuffers) {
        packSize += mem.size();
        CHK_PRT_RET(packSize > dstMax,
            HCCL_ERROR("[%s] fail, packSize[%llu] is bigger than dstMax[%llu]", __func__, packSize, dstMax),
            HCCL_E_PARA);

        CHK_SAFETY_FUNC_RET(memcpy_s(dstPtr, mem.size(), mem.data(), mem.size()));
        dstPtr += mem.size();  // 移动目标指针
    }

    HCCL_INFO("[%s] end of merging host memory, hostPackBuf.addr[%p], hostPackBuf.size[%zu]",
        __func__,
        hostPackBuf.ptr(),
        hostPackBuf.size());

    return HCCL_SUCCESS;
}

static HcclResult FillChannelD2HMap(ChannelHandle *deviceChannelHandles,
    ChannelHandle *hostChannelHandles, uint32_t listNum)
{
    std::lock_guard<std::mutex> lk(hcomm::g_ChannelMapMtx);
    for (uint32_t idx = 0; idx < listNum; idx++) {
        auto deviceChannelHandle = deviceChannelHandles[idx];
        auto hostChannelHandle = hostChannelHandles[idx];
        HCCL_INFO("%s deviceChannelHandle[0x%llx], hostChannelHandle[0x%llx]",
            __func__,
            deviceChannelHandle,
            hostChannelHandle);
        g_ChannelD2HMap.emplace(deviceChannelHandle, hostChannelHandle);
    }

    return HCCL_SUCCESS;
}

HcclResult HcommChannelKernelLaunch(ChannelHandle *channelHandles, ChannelHandle *hostChannelHandles, uint32_t listNum,
    const std::string &commTag, aclrtBinHandle binHandle)
{
    HCCL_RUN_INFO("[%s] listNum[%u], commTag[%s]", __func__, listNum, commTag.c_str());
    std::vector<std::vector<char>> hostPackBuffers(listNum);
    HcclChannelUrmaRes channelParam{};
    CHK_SAFETY_FUNC_RET(memset_s(&channelParam, sizeof(channelParam), 0, sizeof(channelParam)));

    // 获取到host侧序列化的地址
    uint32_t totalListNum = 0;
    for (uint32_t index = 0; index < listNum; index++) {
        auto aicpuTsUrmaChannel = reinterpret_cast<hcomm::AicpuTsUrmaChannel *>(hostChannelHandles[index]);
        CHK_PRT(aicpuTsUrmaChannel->H2DResPack(hostPackBuffers[index]));
        totalListNum += hostPackBuffers[index].size();
    }
    HCCL_INFO("[%s] totalListNum[%llu]", __func__, totalListNum);

    // 分配连续的host内存，将序列化的地址放入其中
    hccl::HostMem hostPackBuf = hccl::HostMem::alloc(totalListNum);
    CHK_PTR_NULL(hostPackBuf.ptr());
    CHK_RET(CombineHostMemory(hostPackBuffers, hostPackBuf));
    hccl::DeviceMem devicePackBuf = hccl::DeviceMem::alloc(totalListNum);
    CHK_PTR_NULL(devicePackBuf.ptr());

    // 将host侧序列化内容拷贝到device侧内存中
    CHK_RET(hrtMemSyncCopy(devicePackBuf.ptr(),
        totalListNum,
        hostPackBuf.ptr(),
        totalListNum,
        HcclRtMemcpyKind::HCCL_RT_MEMCPY_KIND_HOST_TO_DEVICE));

    // 为device侧的channelList分配内存
    hccl::DeviceMem deviceChannelList = hccl::DeviceMem::alloc(listNum * sizeof(ChannelHandle));
    CHK_PTR_NULL(deviceChannelList.ptr());

    // channelParam资源参数填充
    s32 sRet = strncpy_s(channelParam.hcomId, HCOMID_MAX_LENGTH, commTag.c_str(), HCOMID_MAX_LENGTH - 1);
    CHK_PRT_RET(sRet != EOK, HCCL_ERROR("[%s] str copy fail. return[%d]", __func__, sRet), HCCL_E_INTERNAL);

    channelParam.channelList = static_cast<void *>(deviceChannelList.ptr());
    channelParam.listNum = listNum;
    channelParam.uniqueIdAddr = static_cast<void *>(devicePackBuf.ptr());
    channelParam.uniqueIdSize = totalListNum;
    channelParam.singleUniqueIdSize = totalListNum / hostPackBuffers.size();

    // 创建局部流
    hccl::Stream localStream(hccl::StreamType::STREAM_TYPE_ONLINE);
    constexpr u32 aicpuStreamMode = 1;
    CHK_RET(hrtStreamSetMode(localStream.ptr(), aicpuStreamMode));

    // 下kernel
    std::string kernelName = "RunAicpuIndOpChannelInitV2";
    struct InitTask {
        u64 context;
        bool isCustom;
    };
    // 拷贝channelParam到device
    hccl::DeviceMem addr = hccl::DeviceMem::alloc(sizeof(channelParam));
    CHK_PTR_NULL(addr.ptr());

    CHK_RET(hrtMemSyncCopy(addr.ptr(),
        sizeof(channelParam),
        &channelParam,
        sizeof(channelParam),
        HcclRtMemcpyKind::HCCL_RT_MEMCPY_KIND_HOST_TO_DEVICE));

    InitTask customInitTask = {0};
    customInitTask.context = reinterpret_cast<u64>(addr.ptr());
    customInitTask.isCustom = false;

    CHK_RET(hccl::AicpuAclKernelLaunch(localStream.ptr(),
        reinterpret_cast<void *>(&customInitTask),
        sizeof(customInitTask),
        binHandle,
        kernelName,
        true,
        NOTIFY_DEFAULT_WAIT_TIME));

    CHK_RET(
        hcclStreamSynchronize(localStream.ptr(), hccl::CommConfiger::GetInstance().GetCommConfigExecTimeOut(commTag)));

    // 将device侧的channelList拷贝回host侧的channelList
    CHK_RET(hrtMemSyncCopy(channelHandles,
        listNum * sizeof(ChannelHandle),
        deviceChannelList.ptr(),
        listNum * sizeof(ChannelHandle),
        HcclRtMemcpyKind::HCCL_RT_MEMCPY_KIND_DEVICE_TO_HOST));

    CHK_RET(FillChannelD2HMap(channelHandles, hostChannelHandles, listNum));

    HCCL_INFO("[%s] channel kernel launch success.", __func__);

    return HCCL_SUCCESS;
}

HcclResult HcommChannelGet(const ChannelHandle channelHandle, void **channel)
{
    CHK_PTR_NULL(channel);
 
    const auto &D2HhandleIter = hcomm::g_ChannelD2HMap.find(channelHandle);
    if (D2HhandleIter == hcomm::g_ChannelD2HMap.end()) {
        HCCL_ERROR("[Hcomm][%s] channel[%llx] not found.", __func__, channelHandle);
        return HcclResult::HCCL_E_NOT_FOUND;
    }
 
    const auto handle = D2HhandleIter->second;
    const auto &handleIter = hcomm::g_ChannelMap.find(handle);
    if (handleIter == hcomm::g_ChannelMap.end()) {
        HCCL_ERROR("[Hcomm][%s] channel[%llx] not found.", __func__, handle);
        return HcclResult::HCCL_E_NOT_FOUND;
    }
    *channel = reinterpret_cast<void*>(handleIter->second.get());
    return HcclResult::HCCL_SUCCESS;
}

HcclResult HcommChannelGetStatus(const ChannelHandle *channelList, uint32_t listNum, int32_t *statusList)
{
    // 不得随意添加无效日志，可能造成刷屏
    CHK_PTR_NULL(channelList);
    CHK_PTR_NULL(statusList);

    u32 readyCount = 0;

    for (uint32_t i = 0; i < listNum; ++i) {
        const ChannelHandle inHandle = channelList[i];
        int32_t status = 0;

        // 单锁：D2H 映射 + 查 map + 锁内调用 GetStatus()
        HcclResult ret = hcomm::WithChannelByHandleLocked(inHandle, [&](Channel &channel) -> HcclResult {
            status = channel.GetStatus();  // 锁内调用，防止 destroy 并发释放
            return HcclResult::HCCL_SUCCESS;
        });

        if (ret != HcclResult::HCCL_SUCCESS) {
            HCCL_ERROR("[%s] Get ChannelHandle failed.", __func__);
            return ret;
        }
        CHK_PRT_RET(
            status == ChannelStatus::FAILED, HCCL_ERROR("%s failed, status[%d]", __func__, status), HCCL_E_NETWORK);

        CHK_PRT_RET(status == ChannelStatus::SOCKET_TIMEOUT,
            HCCL_ERROR("%s timeout, status[%d]", __func__, status),
            HCCL_E_TIMEOUT);

        readyCount += (status == ChannelStatus::READY) ? 1 : 0;
        statusList[i] = status;
    }

    HcclResult finalRet = (readyCount == listNum) ? HCCL_SUCCESS : HCCL_E_AGAIN;
    return finalRet;
}

HcclResult HcommChannelGetNotifyNum(ChannelHandle channelHandle, uint32_t *notifyNum)
{
    return hcomm::WithChannelByHandleLocked(channelHandle, [&](Channel &channel) -> HcclResult {
        // 锁内调用，避免 destroy 并发释放
        channel.GetNotifyNum(notifyNum);
        return HcclResult::HCCL_SUCCESS;
    });
}

HcclResult HcommChannelDestroy(const ChannelHandle *channels, uint32_t channelNum)
{
    CHK_PTR_NULL(channels);

    // 单锁：g_ChannelMapMtx 同时保护 g_ChannelMap + g_ChannelD2HMap
    std::lock_guard<std::mutex> lk(hcomm::g_ChannelMapMtx);

    for (uint32_t i = 0; i < channelNum; ++i) {
        const ChannelHandle inHandle = channels[i];

        // 1) 先做 D2H 映射（统一销毁入口 handle）
        auto itH = hcomm::g_ChannelD2HMap.find(inHandle);
        if (itH == hcomm::g_ChannelD2HMap.end()) {
            HCCL_ERROR(
                "[Hcomm][%s] failed to find handle mapping in g_ChannelD2HMap, inHandle[0x%llx].", __func__, inHandle);
            return HcclResult::HCCL_E_NOT_FOUND;
        }
        const ChannelHandle mappedHandle = itH->second;

        // 2) 从 ChannelMap 删除 channel（以 mappedHandle 为准）
        auto itC = hcomm::g_ChannelMap.find(mappedHandle);
        if (itC == hcomm::g_ChannelMap.end()) {
            HCCL_ERROR("[Hcomm][%s] failed to find channel in g_ChannelMap, inHandle[0x%llx], mappedHandle[0x%llx].",
                __func__,
                inHandle,
                mappedHandle);
            return HcclResult::HCCL_E_NOT_FOUND;
        }

        HCCL_INFO("[Hcomm][%s] erase channel: inHandle[0x%llx], mappedHandle[0x%llx], ptr[%p]",
            __func__,
            inHandle,
            mappedHandle,
            itC->second.get());

        // 3) 先 erase ChannelMap（unique_ptr 释放对象）
        hcomm::g_ChannelMap.erase(itC);

        // 4) 清理 D2HMap 中所有指向 mappedHandle 的映射，避免残留导致后续查到“已销毁”
        for (auto it = hcomm::g_ChannelD2HMap.begin(); it != hcomm::g_ChannelD2HMap.end();) {
            if (it->second == mappedHandle) {
                it = hcomm::g_ChannelD2HMap.erase(it);
            } else {
                ++it;
            }
        }
    }

    return HcclResult::HCCL_SUCCESS;
}

HcclResult HcommChannelGetRemoteMem(ChannelHandle channelHandle, HcommMem **remoteMem, uint32_t *memNum, char **memTags)
{
    HcclMem **remoteMemConverted = reinterpret_cast<HcclMem **>(remoteMem);

    return hcomm::WithChannelByHandleLocked(channelHandle, [&](Channel &channel) -> HcclResult {
        // 锁内调用，避免 destroy 并发释放
        channel.GetRemoteMem(remoteMemConverted, memNum, memTags);
        return HcclResult::HCCL_SUCCESS;
    });
}

HcclResult HcommChannelGetUserRemoteMem(ChannelHandle channelHandle, CommMem **remoteMem, char ***memTag, uint32_t *memNum)
{
    return hcomm::WithChannelByHandleLocked(channelHandle, [&](Channel &channel) -> HcclResult {
        // 锁内调用，避免 destroy 并发释放
        channel.GetUserRemoteMem(remoteMem, memTag, memNum);
        return HcclResult::HCCL_SUCCESS;
    });
}

HcclResult HcommThreadAlloc(CommEngine engine, uint32_t threadNum, uint32_t notifyNumPerThread, ThreadHandle *threads)
{
    CHK_PTR_NULL(threads);

    HCCL_INFO("[%s]HcclThreadAcquire begin. need threadNum[%u], notifyPerThread[%u]",
        __func__,
        threadNum,
        notifyNumPerThread);

    if (threadNum == 0) {
        HCCL_ERROR("[HcommThreadAlloc]threadNum is 0");
        return HCCL_E_PARA;
    }

    hccl::NotifyLoadType notifyLoadType;
    hccl::StreamType streamType;
    CHK_RET(CommEngineToNotifyLoadType(engine, notifyLoadType));
    CHK_RET(CommEngineToStreamType(engine, streamType));

    HcclResult ret = HCCL_E_INTERNAL;

    for (uint32_t i = 0; i < threadNum; ++i) {
        std::shared_ptr<hccl::Thread> handle;
        HCCL_INFO("[%s] AicpuTsThread notifyLoadType[%u], streamType[%u]",
            __func__,
            static_cast<int32_t>(notifyLoadType),
            static_cast<int32_t>(streamType));
        CHK_RET(CreateThread(engine, streamType, notifyNumPerThread, notifyLoadType, handle));
        ret = handle->Init();
        if (ret != HCCL_SUCCESS) {
            HCCL_ERROR("[HcommThreadAlloc] Failed to init thread index %u", i);
            return ret;
        }
        // newThreads.emplace_back(std::move(handle));
        threads[i] = reinterpret_cast<ThreadHandle>(handle.get());
        hcomm::g_ThreadMap.emplace(threads[i], handle);
        HCCL_INFO("[%s] host threadArray[%u] = [%lu]",__func__, i, threads[i]);
    }

    HCCL_INFO("[HcommThreadAlloc] HcclThreadAcquire done: engine[%d] threadNum[%u],"
              "notifyPerThread[%u]%s",
        engine,
        threadNum,
        notifyNumPerThread,
        (engine == COMM_ENGINE_AICPU || engine == COMM_ENGINE_AICPU_TS) ? " (AICPU token ready)" : "");
    return HCCL_SUCCESS;
}

HcclResult HcommThreadFree(const ThreadHandle *threads, uint32_t threadNum)
{
    if (threads == nullptr) {
        HCCL_ERROR("[HcommThreadfree] threads is null.");
        return HCCL_E_PARA;
    }

    if (threadNum == 0) {
        HCCL_ERROR("[HcommThreadfree] threadNum is 0, nothing to free.");
        return HCCL_E_PARA;
    }

    HCCL_INFO("[HcommThreadfree] begin to free %u threads", threadNum);

    for (uint32_t i = 0; i < threadNum; ++i) {
        auto handleIter = hcomm::g_ThreadMap.find(threads[i]);
        if (handleIter == hcomm::g_ThreadMap.end()) {
            HCCL_ERROR("[%s] failed to find thread[0x%llx].", __func__, threads[i]);
            return HcclResult::HCCL_E_NOT_FOUND;
        }
        hcomm::g_ThreadMap.erase(threads[i]);
    }

    HCCL_INFO("[HcommThreadfree] all threads freed successfully");
    return HCCL_SUCCESS;
}

HcclResult HcommThreadAllocWithStream(CommEngine engine,
    rtStream_t stream, uint32_t notifyNum, ThreadHandle *thread)
{
    CHK_PTR_NULL(thread);
    hccl::NotifyLoadType notifyLoadType;
    CHK_RET(CommEngineToNotifyLoadType(engine, notifyLoadType));
    std::shared_ptr<hccl::Thread> handle;
    EXECEPTION_CATCH(handle = std::make_shared<hccl::CpuTsThread>(stream, notifyNum, notifyLoadType), return HCCL_E_PTR);
    CHK_RET(handle->Init());
 
    // 返回第一个句柄
    *thread = reinterpret_cast<ThreadHandle>(handle.get());
    hcomm::g_ThreadMap.emplace(*thread , handle);
 
    HCCL_INFO("[ThreadMgr]  HcclThreadAcquireWithStream done: engine[%d] stream[%p],"
        "notifyNum[%u]",  engine, stream, notifyNum);
    return HCCL_SUCCESS;
}
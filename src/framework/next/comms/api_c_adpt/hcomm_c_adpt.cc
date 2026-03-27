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
#include "hcomm_result_defs.h"
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

namespace {
HcommResult ProcessHcommResPackReq(const HcommChannelDesc &channelDesc, HcommChannelDesc &channelDescFinal)
{
    if (channelDesc.header.size < sizeof(CommAbiHeader)) {
        HCCL_ERROR("[%s] invalid channelDesc.header.size[%u].", __func__, channelDesc.header.size);
        return HCCL_E_PARA;
    }

    if (channelDesc.header.magicWord != channelDescFinal.header.magicWord) {
        HCCL_ERROR("[%s] channelDesc.header.magicWord[0x%08x] is invalid, expected[0x%08x].",
            __func__, channelDesc.header.magicWord, channelDescFinal.header.magicWord);
        return HCCL_E_PARA;
    }

    const uint32_t copySize = (channelDescFinal.header.size < channelDesc.header.size ?
        channelDescFinal.header.size : channelDesc.header.size) - sizeof(CommAbiHeader);
    CHK_SAFETY_FUNC_RET(memcpy_s(reinterpret_cast<uint8_t *>(&channelDescFinal) + sizeof(CommAbiHeader), copySize,
        reinterpret_cast<const uint8_t *>(&channelDesc) + sizeof(CommAbiHeader), copySize));

    if (channelDesc.header.version >= HCOMM_CHANNEL_VERSION_ONE) {
        channelDescFinal.remoteEndpoint = channelDesc.remoteEndpoint;
        channelDescFinal.notifyNum = channelDesc.notifyNum;
        channelDescFinal.exchangeAllMems = channelDesc.exchangeAllMems;
        channelDescFinal.memHandles = channelDesc.memHandles;
        channelDescFinal.memHandleNum = channelDesc.memHandleNum;
        channelDescFinal.socket = channelDesc.socket;
        channelDescFinal.role = channelDesc.role;
        channelDescFinal.port = channelDesc.port;
    }

    if (channelDesc.header.version > HCOMM_CHANNEL_VERSION) {
        HCCL_RUN_WARNING("The version of provided [%u] is higher than the current version[%u], "
            "unsupported configuration will be ignored.",
            channelDesc.header.version, HCOMM_CHANNEL_VERSION);
    } else if (channelDesc.header.version < HCOMM_CHANNEL_VERSION) {
        HCCL_RUN_WARNING("The version of provided [%u] is lower than the current version[%u], "
            "configurations supported by later versions will be ignored.",
            channelDesc.header.version, HCOMM_CHANNEL_VERSION);
    }

    return HCOMM_SUCCESS;
}

HcommResult NormalizeHcommChannelDescs(HcommChannelDesc *channelDescs, uint32_t channelNum,
    std::vector<HcommChannelDesc> &channelDescFinals)
{
    channelDescFinals.clear();
    channelDescFinals.reserve(channelNum);
    for (uint32_t idx = 0; idx < channelNum; ++idx) {
        HcommChannelDesc channelDescFinal{};
        HcommResult ret = HcommChannelDescInit(&channelDescFinal, 1);
        if (ret != HCOMM_SUCCESS) {
            return ret;
        }
        ret = ProcessHcommResPackReq(channelDescs[idx], channelDescFinal);
        if (ret != HCOMM_SUCCESS) {
            HCCL_ERROR("[%s] failed to normalize channelDesc[%u], ret[%d].", __func__, idx, ret);
            return ret;
        }
        channelDescFinals.push_back(channelDescFinal);
    }
    return HCOMM_SUCCESS;
}
} // namespace

HcommResult HcommResMgrInit(uint32_t devPhyId)
{
    // 临时方案：触发统一平台层单例触发静态对象声明
    // 内部流程触发各种单例声明，保证时序
    EXCEPTION_HANDLE_BEGIN
    HCCLV2_FUNC_RUN([&]() -> HcclResult {
        (void)HcommResMgr::GetInstance(devPhyId);
        return HcclResult::HCCL_SUCCESS;
    }());
    EXCEPTION_HANDLE_END
    return HCCL_SUCCESS;
}

static HcclResult EnsureKernelBinLoaded(CommEngine engine) {
    if (engine != COMM_ENGINE_AICPU && engine != COMM_ENGINE_AICPU_TS) {
        HCCL_INFO("[%s] engine[%d] kernel loading not required", __func__, engine);
        return HCCL_SUCCESS;
    }
    std::lock_guard<std::mutex> lock(hcomm::g_BinHandleMtx);
    if (g_BinHandle != nullptr) {
        return HCCL_SUCCESS;
    }
    std::string jsonPath;
    CHK_RET(hccl::GetKernelFilePath(jsonPath));
    jsonPath += "ccl_kernel.json";

    HcclResult ret = hccl::LoadBinaryFromFile(jsonPath.c_str(), ACL_RT_BINARY_LOAD_OPT_CPU_KERNEL_MODE, 0, g_BinHandle);
    CHK_PRT_RET(ret != HCCL_SUCCESS,
                HCCL_ERROR("[EnsureKernelBinLoaded] load aicpu file fail, path[%s]", jsonPath.c_str()),
                ret);
    return HCCL_SUCCESS;
}

HcommResult HcommEndpointGet(EndpointHandle endpointHandle, void **endpoint)  // 根据endpointHandle返回Endpoint对象指针
{
    auto it = g_EndpointMap.GetEndpoint(endpointHandle);
    CHK_PRT_RET(it == nullptr, HCCL_ERROR("[%s] endpoint not found in g_EndpointMap, endpointHandle[%p]",
        __func__, endpointHandle), HCCL_E_PARA);

    *endpoint = static_cast<void *>(it);
    return HCCL_SUCCESS;
}

HcommResult HcommEndpointCreate(const EndpointDesc *endpoint, EndpointHandle *endpointHandle)
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

HcommResult HcommEndpointDestroy(EndpointHandle endpointHandle)
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


HcommResult HcommEndpointStartListen(EndpointHandle endpointHandle, uint32_t port, HcommEndpointListenConfig* config)
{
    (void)config;
    CHK_PTR_NULL(endpointHandle);
    Endpoint* endpoint = g_EndpointMap.GetEndpoint(endpointHandle);
    CHK_PTR_NULL(endpoint);
    CHK_RET(endpoint->ServerSocketListen(port));
    return HCCL_SUCCESS;
}

HcommResult HcommEndpointStopListen(EndpointHandle endpointHandle, uint32_t port)
{
    CHK_PTR_NULL(endpointHandle);
    Endpoint* endpoint = g_EndpointMap.GetEndpoint(endpointHandle);
    CHK_PTR_NULL(endpoint);
    CHK_RET(endpoint->ServerSocketStopListen(port));
    return HCCL_SUCCESS;
}

HcommResult HcommMemReg(EndpointHandle endpointHandle, const char *memTag, const CommMem *mem,
    HcommMemHandle *memHandle)
{
    EXCEPTION_HANDLE_BEGIN
    CHK_PTR_NULL(mem);
    CHK_PTR_NULL(memHandle);
    auto endpoint = g_EndpointMap.GetEndpoint(endpointHandle);
    CHK_PTR_NULL(endpoint);
    CHK_RET(endpoint->RegisterMemory(*mem, memTag, reinterpret_cast<void **>(memHandle)));
    EXCEPTION_HANDLE_END
    return HCCL_SUCCESS;
}

HcommResult HcommMemUnreg(EndpointHandle endpointHandle, HcommMemHandle memHandle)
{
    auto endpoint = g_EndpointMap.GetEndpoint(endpointHandle);
    CHK_PTR_NULL(endpoint);
    CHK_RET(endpoint->UnregisterMemory(memHandle));
    return HCCL_SUCCESS;
}

HcommResult HcommMemExport(EndpointHandle endpointHandle, HcommMemHandle memHandle, void **memDesc,
    uint32_t *memDescLen)
{
    auto endpoint = g_EndpointMap.GetEndpoint(endpointHandle);
    CHK_PTR_NULL(endpoint);
    CHK_RET(endpoint->MemoryExport(memHandle, memDesc, memDescLen));
    return HCCL_SUCCESS;
}

HcommResult HcommMemImport(EndpointHandle endpointHandle, const void *memDesc, uint32_t descLen, CommMem *outMem)
{
    auto endpoint = g_EndpointMap.GetEndpoint(endpointHandle);
    CHK_PTR_NULL(endpoint);
    CHK_PTR_NULL(outMem);
    CommMem importedMem{};
    CHK_RET(endpoint->MemoryImport(memDesc, descLen, &importedMem));
    *outMem = importedMem;
    return HCCL_SUCCESS;
}

HcommResult HcommMemUnimport(EndpointHandle endpointHandle, const void *memDesc, uint32_t descLen)
{
    auto endpoint = g_EndpointMap.GetEndpoint(endpointHandle);
    CHK_PTR_NULL(endpoint);
    CHK_RET(endpoint->MemoryUnimport(memDesc, descLen));
    return HCCL_SUCCESS;
}

/* 暂未实现 */
HcommResult HcommMemGrant(EndpointHandle endpointHandle, const HcommMemGrantInfo *remoteGrantInfo)
{
    return HCCL_SUCCESS;
}

/* 暂未实现 */
HcommResult HcommMemRemap(const EndpointHandle endpointHandle, const CommMem *memArray, uint64_t arraySize)
{
    return HCCL_SUCCESS;
}

HcommResult HcommMemGetAllMemHandles(EndpointHandle endpointHandle, void **memHandles, uint32_t *memHandleNum)
{
    auto endpoint = g_EndpointMap.GetEndpoint(endpointHandle);
    CHK_RET(endpoint->GetAllMemHandles(memHandles, memHandleNum));
    return HCCL_SUCCESS;
}

// 集合通信使用，待归一到HcommChannelCreate
HcommResult HcommCollectiveChannelCreate(EndpointHandle endpointHandle, CommEngine engine,
    HcommChannelDesc *channelDescs, uint32_t channelNum, ChannelHandle *channels)
{
    CHK_PTR_NULL(channelDescs);
    CHK_PTR_NULL(channels);

    std::vector<HcommChannelDesc> channelDescFinals;
    CHK_RET(static_cast<HcclResult>(NormalizeHcommChannelDescs(channelDescs, channelNum, channelDescFinals)));
    return ChannelProcess::CreateChannelsLoop(endpointHandle, engine, channelDescFinals.data(), channelNum, channels);
}

HcommResult HcommChannelCreate(EndpointHandle endpointHandle, CommEngine engine,
    HcommChannelDesc *channelDescs, uint32_t channelNum, ChannelHandle *channels)
{
    if (hostPackBuffers.empty()) {
        HCCL_ERROR("[%s] hostPackBuffers is empty, please check.", __func__);
        return HCCL_E_PARA;
    }

    std::vector<HcommChannelDesc> channelDescFinals;
    CHK_RET(static_cast<HcclResult>(NormalizeHcommChannelDescs(channelDescs, channelNum, channelDescFinals)));

    std::vector<ChannelHandle> hostChannelHandles(channelNum);
    ChannelHandle* targetChannels = hostChannelHandles.data();

    CHK_RET(ChannelProcess::CreateChannelsLoop(endpointHandle, engine, channelDescFinals.data(), channelNum,
        targetChannels));
    CHK_RET(ChannelProcess::ConnectChannels(targetChannels, channelNum, engine));
    CHK_RET(EnsureKernelBinLoaded(engine));
    CHK_RET(ChannelProcess::SaveChannels(targetChannels, channels, channelNum, engine, g_BinHandle));

    return HCCL_SUCCESS;
}

HcommResult HcommChannelGet(ChannelHandle channelHandle, void **channel)
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

HcommResult HcommChannelGetStatus(const ChannelHandle *channelList, uint32_t listNum,  int32_t* statusList)
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

HcommResult HcommChannelGetNotifyNum(ChannelHandle channelHandle, uint32_t *notifyNum)
{
    return hcomm::WithChannelByHandleLocked(channelHandle, [&](Channel &channel) -> HcclResult {
        // 锁内调用，避免 destroy 并发释放
        channel.GetNotifyNum(notifyNum);
        return HcclResult::HCCL_SUCCESS;
    });
}

HcommResult HcommChannelDestroy(const ChannelHandle *channels, uint32_t channelNum)
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

HcommResult HcommChannelGetRemoteMem(ChannelHandle channelHandle, CommMem **remoteMem, uint32_t *memNum, char **memTags)
{
    HcclMem **remoteMemConverted = reinterpret_cast<HcclMem **>(remoteMem);

    return hcomm::WithChannelByHandleLocked(channelHandle, [&](Channel &channel) -> HcclResult {
        // 锁内调用，避免 destroy 并发释放
        channel.GetRemoteMem(remoteMemConverted, memNum, memTags);
        return HcclResult::HCCL_SUCCESS;
    });
}

HcommResult HcommChannelGetRemoteMems(ChannelHandle channelHandle, uint32_t *memNum, CommMem **remoteMems,
    char ***memTags)
{
    return ChannelProcess::ChannelGetUserRemoteMem(channelHandle, remoteMems, memTags, memNum);
}

HcommResult HcommThreadAlloc(CommEngine engine, uint32_t threadNum, const uint32_t *notifyNumPerThread,
    ThreadHandle *threads) {
    CHK_PTR_NULL(threads);
    CHK_PTR_NULL(notifyNumPerThread);
    const uint32_t notifyNum = notifyNumPerThread[0];
    if (threadNum > 1U) {
        HCCL_RUN_WARNING("[%s] only notifyNumPerThread[0] is used currently, threadNum[%u], notifyNum[0][%u].",
            __func__, threadNum, notifyNum);
    }
    HCCL_INFO("[%s] ThreadAcquire begin. engine[%d], threadNum[%u], notifyPerThread[%u], threads[%p]",
        __func__, engine, threadNum, notifyNum, threads);

    // 1. 参数校验
    CHK_RET(hccl::ValidateThreadParams(threadNum, notifyNum));

    hccl::NotifyLoadType notifyLoadType;
    hccl::StreamType streamType;
    CHK_RET(CommEngineToNotifyLoadType(engine, notifyLoadType));
    CHK_RET(CommEngineToStreamType(engine, streamType));

    // 3. 创建线程
    std::vector<std::shared_ptr<hccl::Thread>> newThreads;
    hccl::ThreadCreateParams params(engine, threadNum, notifyNum, notifyLoadType, streamType);
    CHK_RET(hccl::CreateAndInitThreads(params, newThreads));

    // 4. 插入全局映射表
    CHK_RET(hccl::SaveThreads(newThreads));

    // 5. 储存线程句柄
    CHK_RET(EnsureKernelBinLoaded(engine));
    CHK_RET(hccl::StoreThreadHandles(newThreads, threads, engine, g_BinHandle));

    HCCL_INFO("[HcommThreadAlloc] ThreadAcquire done: engine[%d] threadNum[%u], notifyPerThread[%u]",
              engine, threadNum, notifyNum);
    return HCCL_SUCCESS;
}

HcommResult HcommThreadAlloc(CommEngine engine, uint32_t threadNum, uint32_t notifyNumPerThread,
    ThreadHandle *threads)
{
    return ::HcommThreadAlloc(engine, threadNum, &notifyNumPerThread, threads);
}

HcommResult HcommThreadFree(const ThreadHandle *threads, uint32_t threadNum)
{
    if (threads == nullptr) {
        HCCL_ERROR("[HcommThreadfree] threads is null.");
        return HCCL_E_PARA;
    }

    // 不允许空释放
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

HcommResult HcommThreadAllocWithStream(CommEngine engine,
    rtStream_t stream, uint32_t notifyNum, ThreadHandle *thread)
{
    CHK_PTR_NULL(thread);
    hccl::NotifyLoadType notifyLoadType;
    CHK_RET(CommHostEngineToNotifyLoadType(engine, notifyLoadType));
    std::shared_ptr<hccl::Thread> handle;
    EXECEPTION_CATCH(handle = std::make_shared<hccl::CpuTsThread>(stream, notifyNum, notifyLoadType), return HCCL_E_PTR);
    CHK_RET(handle->Init());
 
    // 返回第一个句柄
    *thread = reinterpret_cast<ThreadHandle>(handle.get());
    hcomm::g_ThreadMap.emplace(*thread , handle);
 
    HCCL_INFO("[ThreadMgr]  ThreadAcquireWithStream done: engine[%d] stream[%p],"
        "notifyNum[%u]",  engine, stream, notifyNum);
    return HCCL_SUCCESS;
}

HcommResult HcommEngineCtxCreate(CommEngine engine, uint64_t size, void **ctx)
{
    if (engine == COMM_ENGINE_CPU || engine == COMM_ENGINE_CPU_TS
        || engine == COMM_ENGINE_CCU) {
        *ctx = malloc(size);
        CHK_PTR_NULL(*ctx);
        CHK_SAFETY_FUNC_RET(memset_s(*ctx, size, 0, size));
    } else if (engine == COMM_ENGINE_AICPU || engine == COMM_ENGINE_AICPU_TS
        || engine == COMM_ENGINE_AIV) {
        CHK_RET(hrtMalloc(ctx, size));
    } else {
        HCCL_ERROR("[%s] not support engine type[%d]", __func__, engine);
        return HCCL_E_PARA;
    }
    return HCCL_SUCCESS;
}

HcommResult HcommEngineCtxDestroy(CommEngine engine, void *ctx)
{
    CHK_PTR_NULL(ctx);
    if (engine == COMM_ENGINE_CPU || engine == COMM_ENGINE_CPU_TS
        || engine == COMM_ENGINE_CCU) {
        free(ctx);
    } else if (engine == COMM_ENGINE_AICPU || engine == COMM_ENGINE_AICPU_TS
        || engine == COMM_ENGINE_AIV) {
        CHK_RET(hrtFree(ctx));
    } else {
        HCCL_ERROR("[%s] invalid engine[%d]", __func__, engine);
        return HCCL_E_PARA;
    }
    return HCCL_SUCCESS;
}

HcommResult HcommEngineCtxCopy(CommEngine engine, void *dstCtx, const void *srcCtx, uint64_t size)
{
    if (engine == COMM_ENGINE_AICPU_TS || engine == COMM_ENGINE_AICPU
        || engine == COMM_ENGINE_AIV) {
        // 从Host内存拷贝到Device Context内存上
        CHK_RET(hrtMemSyncCopy(reinterpret_cast<uint8_t*>(dstCtx), size, srcCtx, size,
            HcclRtMemcpyKind::HCCL_RT_MEMCPY_KIND_HOST_TO_DEVICE));
    } else if (engine == COMM_ENGINE_CPU || engine == COMM_ENGINE_CPU_TS
        || engine == COMM_ENGINE_CCU) {
        CHK_SAFETY_FUNC_RET(memcpy_s(reinterpret_cast<uint8_t*>(dstCtx), size, srcCtx, size));
    } else {
        HCCL_ERROR("[%s]copy engine ctx failed, Unsupported engine[%d]", __func__, engine);
        return HCCL_E_PARA;
    }
    HCCL_INFO("[%s]copy engine ctx success, engine[%d]", __func__, engine);
    return HCCL_SUCCESS;
}

HcommResult HcommDfxKernelLaunch(const std::string &commTag, aclrtBinHandle binHandle, HcclDfxOpInfo dfxOpInfo)
{
    // 申请device侧内存
    hccl::DeviceMem devicePackBuf = hccl::DeviceMem::alloc(sizeof(dfxOpInfo));
    CHK_PTR_NULL(devicePackBuf.ptr());
    
    // 将dfxOpInfo信息传递给device侧
    CHK_RET(hrtMemSyncCopy(devicePackBuf.ptr(),
        sizeof(dfxOpInfo),
        &dfxOpInfo,
        sizeof(dfxOpInfo),
        HcclRtMemcpyKind::HCCL_RT_MEMCPY_KIND_HOST_TO_DEVICE));

    // 创建局部流
    hccl::Stream localStream(hccl::StreamType::STREAM_TYPE_ONLINE);
    constexpr u32 aicpuStreamMode = 1;
    CHK_RET(hrtStreamSetMode(localStream.ptr(), aicpuStreamMode));

    // 下kernel
    std::string kernelName = "RunAicpuDfxOpInfoInitV2";

    struct InitTask {
        u64 context;
        char commTag[256];
    };

    InitTask customInitTask = {0, ""};
    customInitTask.context = reinterpret_cast<u64>(devicePackBuf.ptr());
    s32 sRet = strncpy_s(customInitTask.commTag, TAG_MAX_LENGTH, commTag.c_str(), TAG_MAX_LENGTH - 1);
    CHK_PRT_RET(sRet != EOK, HCCL_ERROR("[%s] str copy fail. return[%d]", __func__, sRet), HCCL_E_INTERNAL);

    CHK_RET(hccl::AicpuAclKernelLaunch(localStream.ptr(),
        reinterpret_cast<void *>(&customInitTask),  
        sizeof(customInitTask),  
        binHandle,            
        kernelName,
        true,
        NOTIFY_DEFAULT_WAIT_TIME));

    CHK_RET(
        hcclStreamSynchronize(localStream.ptr(), hccl::CommConfiger::GetInstance().GetCommConfigExecTimeOut(commTag)));

    HCCL_INFO("[%s] channel kernel launch success.", __func__);

    return HCCL_SUCCESS;
}

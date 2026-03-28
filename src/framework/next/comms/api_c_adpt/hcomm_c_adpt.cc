/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "hcomm_c_adpt.h"

#include <mutex>
#include <cstring>

#include "hccl/hccl_res.h"
#include "hcomm_res.h"
#include "hcomm_res_defs.h"
#include "hcomm_result_defs.h"
#include "log.h"
#include "../endpoints/endpoint.h"
#include "../endpoint_pairs/channels/channel.h"
#include "thread.h"
#include "aicpu_ts_thread.h"
#include "cpu_ts_thread.h"
#include "cpu_thread.h"
#include "aicpu_ts_urma_channel.h"
#include "mem_device_pub.h"
#include "channel_param.h"
#include "launch_aicpu.h"
#include "comm_configer.h"
#include "endpoint_map.h"

#include "../hcomm_res_mgr.h"

#include "param_check_pub.h"
#include "exception_handler.h"
#include "hcclCommDfx.h"
#include "hcclCommOp.h"
#include "exception_handler.h"
#include "param_check_pub.h"
#include "channel_process.h"
#include "launch_device.h"
#include "resource_entities.h"
#include "orion_adapter_rts.h"

// specify namespaces for the macro TRY_CATCH_*
using string = std::string;
using exception = std::exception;
using HcclException = Hccl::HcclException;


namespace hcomm {
static std::unordered_map<ThreadHandle, std::shared_ptr<hccl::Thread>> g_ThreadMap;
static std::mutex g_ThreadMapMtx;
static aclrtBinHandle g_BinHandle;
static std::mutex g_BinHandleMtx;
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
    CHK_PTR_NULL(endpoint);
    auto it = g_EndpointMap.GetEndpoint(endpointHandle);
    CHK_PRT_RET(it == nullptr, HCCL_ERROR("[%s] endpoint not found, endpointHandle[%p]",
        __func__, endpointHandle), HCCL_E_NOT_FOUND);

    *endpoint = static_cast<void *>(it);
    return HCCL_SUCCESS;
}

HcommResult HcommEndpointCreate(const EndpointDesc *endpoint, EndpointHandle *endpointHandle)
{
    EXCEPTION_HANDLE_BEGIN
    CHK_PTR_NULL(endpoint);
    CHK_PTR_NULL(endpointHandle);
    if (endpoint->loc.locType != ENDPOINT_LOC_TYPE_DEVICE && endpoint->loc.locType != ENDPOINT_LOC_TYPE_HOST) {
        HCCL_ERROR("[%s] Only support END_POINT_LOCATION_DEVICE AND END_POINT_LOCATION_HOST, but "
                   "endpoint->loc.locType is %d",
            __func__,
            endpoint->loc.locType);
        return HCCL_E_PARA;
    }

    std::unique_ptr<Endpoint> endpointPtr = nullptr;

    HcclResult ret = Endpoint::CreateEndpoint(*endpoint, endpointPtr);
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("call Endpoint::CreateEndpoint failed");
        return ret;
    }
    CHK_PTR_NULL(endpointPtr);
    ret = endpointPtr->Init();
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("call endpointPtr->Init failed");
        return ret;
    }

    const EndpointHandle handle = reinterpret_cast<EndpointHandle>(endpointPtr.get());
    CHK_PTR_NULL(handle);
    EXECEPTION_CATCH(g_EndpointMap.AddEndpoint(handle, std::move(endpointPtr)), return HCCL_E_INTERNAL);
    *endpointHandle = handle;

    EXCEPTION_HANDLE_END
    return HCCL_SUCCESS;
}

HcommResult HcommEndpointDestroy(EndpointHandle endpointHandle)
{
    auto ret = g_EndpointMap.RemoveEndpoint(endpointHandle);
    CHK_PRT_RET(ret == false, HCCL_ERROR("[%s] endpoint not found, endpointHandle[0x%llx]",
        __func__, endpointHandle), HCCL_E_NOT_FOUND);
    endpointHandle = nullptr;

    return HCCL_SUCCESS;
}


HcommResult HcommEndpointStartListen(EndpointHandle endpointHandle, uint32_t port, HcommEndpointListenConfig* config)
{
    (void)config;
    auto endpoint = g_EndpointMap.GetEndpoint(endpointHandle);
    CHK_PRT_RET(endpoint == nullptr, HCCL_ERROR("[%s] endpoint not found, endpointHandle[%p]",
        __func__, endpointHandle), HCCL_E_NOT_FOUND);
    CHK_RET(endpoint->ServerSocketListen(port));
    return HCCL_SUCCESS;
}

HcommResult HcommEndpointStopListen(EndpointHandle endpointHandle, uint32_t port)
{
    auto endpoint = g_EndpointMap.GetEndpoint(endpointHandle);
    CHK_PRT_RET(endpoint == nullptr, HCCL_ERROR("[%s] endpoint not found, endpointHandle[%p]",
        __func__, endpointHandle), HCCL_E_NOT_FOUND);
    CHK_RET(endpoint->ServerSocketStopListen(port));
    return HCCL_SUCCESS;
}

HcommResult HcommMemReg(EndpointHandle endpointHandle, const char *memTag, const CommMem *mem,
    HcommMemHandle *memHandle)
{
    CHK_PTR_NULL(memHandle);
    EXCEPTION_HANDLE_BEGIN
    CHK_PTR_NULL(mem);
    CHK_PTR_NULL(memHandle);
    auto endpoint = g_EndpointMap.GetEndpoint(endpointHandle);
    CHK_PRT_RET(endpoint == nullptr, HCCL_ERROR("[%s] endpoint not found, endpointHandle[0x%llx]",
        __func__, endpointHandle), HCCL_E_NOT_FOUND);
    CHK_RET(endpoint->RegisterMemory(*mem, memTag, reinterpret_cast<void **>(memHandle)));
    EXCEPTION_HANDLE_END
    return HCCL_SUCCESS;
}

HcommResult HcommMemUnreg(EndpointHandle endpointHandle, HcommMemHandle memHandle)
{
    CHK_PTR_NULL(memHandle);
    EXCEPTION_HANDLE_BEGIN
    auto endpoint = g_EndpointMap.GetEndpoint(endpointHandle);
    CHK_PRT_RET(endpoint == nullptr, HCCL_ERROR("[%s] endpoint not found, endpointHandle[0x%llx]",
        __func__, endpointHandle), HCCL_E_NOT_FOUND);
    CHK_RET(endpoint->UnregisterMemory(memHandle));
    EXCEPTION_HANDLE_END
    return HCCL_SUCCESS;
}

HcommResult HcommMemExport(EndpointHandle endpointHandle, HcommMemHandle memHandle, void **memDesc,
    uint32_t *memDescLen)
{
    CHK_PTR_NULL(memHandle);
    CHK_PTR_NULL(memDesc);
    CHK_PTR_NULL(memDescLen);
    auto endpoint = g_EndpointMap.GetEndpoint(endpointHandle);
    CHK_PRT_RET(endpoint == nullptr, HCCL_ERROR("[%s] endpoint not found, endpointHandle[0x%llx]",
        __func__, endpointHandle), HCCL_E_NOT_FOUND);
    CHK_RET(endpoint->MemoryExport(memHandle, memDesc, memDescLen));
    return HCCL_SUCCESS;
}

HcommResult HcommMemImport(EndpointHandle endpointHandle, const void *memDesc, uint32_t descLen, CommMem *outMem)
{
    CHK_PTR_NULL(memDesc);
    CHK_PTR_NULL(outMem);
    auto endpoint = g_EndpointMap.GetEndpoint(endpointHandle);
    CHK_PRT_RET(endpoint == nullptr, HCCL_ERROR("[%s] endpoint not found, endpointHandle[0x%llx]",
        __func__, endpointHandle), HCCL_E_NOT_FOUND);
    CHK_PTR_NULL(outMem);
    CommMem importedMem{};
    CHK_RET(endpoint->MemoryImport(memDesc, descLen, &importedMem));
    *outMem = importedMem;
    return HCCL_SUCCESS;
}

HcommResult HcommMemUnimport(EndpointHandle endpointHandle, const void *memDesc, uint32_t descLen)
{
    CHK_PTR_NULL(memDesc);
    auto endpoint = g_EndpointMap.GetEndpoint(endpointHandle);
    CHK_PRT_RET(endpoint == nullptr, HCCL_ERROR("[%s] endpoint not found, endpointHandle[0x%llx]",
        __func__, endpointHandle), HCCL_E_NOT_FOUND);
    CHK_RET(endpoint->MemoryUnimport(memDesc, descLen));
    return HCCL_SUCCESS;
}

/* 暂未实现 */
HcommResult HcommMemGrant(EndpointHandle endpointHandle, const HcommMemGrantInfo *remoteGrantInfo)
{
    return HCCL_E_NOT_SUPPORT;
}

/* 暂未实现 */
HcommResult HcommMemRemap(const EndpointHandle endpointHandle, const CommMem *memArray, uint64_t arraySize)
{
    return HCCL_E_NOT_SUPPORT;
}

HcommResult HcommMemGetAllMemHandles(EndpointHandle endpointHandle, void **memHandles, uint32_t *memHandleNum)
{
    CHK_PTR_NULL(memHandles);
    CHK_PTR_NULL(memHandleNum);
    auto endpoint = g_EndpointMap.GetEndpoint(endpointHandle);
    CHK_PRT_RET(endpoint == nullptr, HCCL_ERROR("[%s] endpoint not found, endpointHandle[0x%llx]",
        __func__, endpointHandle), HCCL_E_NOT_FOUND);
    CHK_RET(endpoint->GetAllMemHandles(memHandles, memHandleNum));
    return HCCL_SUCCESS;
}

// 集合通信使用，待归一到HcommChannelCreate
HcommResult HcommCollectiveChannelCreate(EndpointHandle endpointHandle, CommEngine engine,
    HcommChannelDesc *channelDescs, uint32_t channelNum, ChannelHandle *channels)
{
    CHK_PTR_NULL(channelDescs);
    CHK_PTR_NULL(channels);
    CHK_PRT_RET((channelNum == 0), HCCL_ERROR("[%s]Invalid channelNum, channelNum[%u]",
        __func__, channelNum), HCCL_E_PARA);
    HCCL_INFO("[%s] START. endpointHandle[0x%llx], engine[%d], channelNum[%u].",
        __func__, endpointHandle, engine, channelNum);

    std::vector<HcommChannelDesc> channelDescFinals;
    CHK_RET(static_cast<HcclResult>(NormalizeHcommChannelDescs(channelDescs, channelNum, channelDescFinals)));
    return ChannelProcess::CreateChannelsLoop(endpointHandle, engine, channelDescFinals.data(), channelNum, channels);
}

HcommResult HcommChannelUpdateMemInfo(HcommMemHandle *memHandles, uint32_t memHandleNum, ChannelHandle channelHandle)
{
    CHK_PTR_NULL(memHandles);
    CHK_PRT_RET((memHandleNum == 0), HCCL_ERROR("[%s]Invalid memHandleNum, memHandleNum is 0.", __func__),
        HCCL_E_PARA);
    return ChannelProcess::ChannelUpdateMemInfo(memHandles, memHandleNum, channelHandle);
}

HcommResult HcommChannelCreate(EndpointHandle endpointHandle, CommEngine engine,
    HcommChannelDesc *channelDescs, uint32_t channelNum, ChannelHandle *channels)
{
    CHK_PTR_NULL(channelDescs);
    CHK_PTR_NULL(channels);
    CHK_PRT_RET((channelNum == 0), HCCL_ERROR("[%s]Invalid channelNum, channelNum[%u]",
        __func__, channelNum), HCCL_E_PARA);
    HCCL_INFO("[%s] START. endpointHandle[0x%llx], engine[%d], channelNum[%u].",
        __func__, endpointHandle, engine, channelNum);

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
    return ChannelProcess::ChannelGet(channelHandle, channel);
}

HcommResult HcommChannelGetStatus(const ChannelHandle *channelList, uint32_t listNum,  int32_t* statusList)
{
    // 当前为非阻塞式建链，直接返回成功
    // 参数校验
    CHK_PTR_NULL(channelList);
    CHK_PTR_NULL(statusList);
    CHK_PRT_RET((listNum == 0), HCCL_ERROR("[%s]Invalid listNum, listNum[%u]",
        __func__, listNum), HCCL_E_PARA);
    // 为每个通道设置成功状态
    for (uint32_t i = 0; i < listNum; i++) {
        statusList[i] = 0;
    }
    return HCCL_SUCCESS;
}

HcommResult HcommChannelGetNotifyNum(ChannelHandle channelHandle, uint32_t *notifyNum)
{
    CHK_PTR_NULL(notifyNum);
    return ChannelProcess::ChannelGetNotifyNum(channelHandle, notifyNum);
}

HcommResult HcommChannelDestroy(const ChannelHandle *channels, uint32_t channelNum)
{
    CHK_PTR_NULL(channels);
    return ChannelProcess::ChannelDestroy(channels, channelNum, g_BinHandle);
}

HcommResult HcommChannelGetRemoteMem(ChannelHandle channelHandle, CommMem **remoteMem, uint32_t *memNum, char **memTags)
{
    CHK_PTR_NULL(remoteMem);
    CHK_PTR_NULL(memNum);
    CHK_PTR_NULL(memTags);
    return ChannelProcess::ChannelGetRemoteMem(channelHandle, remoteMem, memNum, memTags);
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

    // 2. 获取引擎对应的类型
    hccl::NotifyLoadType notifyLoadType;
    hccl::StreamType streamType;
    CHK_RET(hccl::CommEngineToNotifyLoadType(engine, notifyLoadType));
    CHK_RET(hccl::CommEngineToStreamType(engine, streamType));

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
    CHK_PTR_NULL(threads);
    return hccl::FreeThreads(threads, threadNum, g_BinHandle);
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
    {
        std::lock_guard<std::mutex> lock(hcomm::g_ThreadMapMtx);
        hcomm::g_ThreadMap.emplace(*thread, handle);
    }
 
    HCCL_INFO("[ThreadMgr]  ThreadAcquireWithStream done: engine[%d] stream[%p],"
        "notifyNum[%u]",  engine, stream, notifyNum);
    return HCCL_SUCCESS;
}

HcclResult HcommThreadAllocWithConfig(CommEngine engine, uint32_t threadNum, const ThreadConfig config, const ThreadType type, ThreadHandle *threads) {
    CHK_PTR_NULL(threads);

    HCCL_INFO("[%s]ThreadAcquire begin. need threadNum[%u], notifyPerThread[%u]",
        __func__,
        threadNum,
        config.notifyNumPerThread);
    if (threadNum == 0 || threadNum > hccl::HCOMM_THREADNUM_MAX_NUM) {
        HCCL_ERROR("[HcommThreadAlloc]ThreadAlloc failed.ThreadNum %u.threadNum range (0 , %u]", threadNum, hccl::HCOMM_THREADNUM_MAX_NUM);
        return HCCL_E_PARA;
    }

    if (config.notifyNumPerThread > hccl::HCOMM_NOTIFY_MAX_NUM) {
        HCCL_ERROR("[HcommThreadAlloc]ThreadAlloc failed.notifyNumPerThread is %u,notifyNumPerThread range [0 , %u]", config.notifyNumPerThread, hccl::HCOMM_NOTIFY_MAX_NUM);
        return HCCL_E_PARA;
    }

    hccl::NotifyLoadType notifyLoadType;
    hccl::StreamType streamType;
    CHK_RET(CommEngineToNotifyLoadType(engine, notifyLoadType));
    CHK_RET(CommEngineToStreamType(engine, streamType));

    HcclResult ret = HCCL_SUCCESS;
    for (uint32_t i = 0; i < threadNum; ++i) {
        std::shared_ptr<hccl::Thread> hostHandle;
        HCCL_INFO("[%s] Thread notifyLoadType[%d], streamType[%d], threadType[%d]",
            __func__,
            static_cast<int32_t>(notifyLoadType),
            static_cast<int32_t>(streamType),
            type);
        ret = CreateThread(engine, streamType, config.notifyNumPerThread, notifyLoadType, type, hostHandle);
        if (ret != HCCL_SUCCESS ) {
            HCCL_ERROR("[HcommThreadAlloc] Failed to create thread index %u", i);
            if (i != 0) {
                CHK_RET(HcommThreadFree(threads, i));
            }
            return ret;
        }
        ret = hostHandle->Init();
        if (ret != HCCL_SUCCESS ) {
            HCCL_ERROR("[HcommThreadAlloc] Failed to init thread index %u", i);
            if (i != 0) {
                CHK_RET(HcommThreadFree(threads, i));
            }
            return ret;
        }

        if (engine == COMM_ENGINE_AICPU && type == THREAD_TYPE_CPU) {
            void* deviceHandle{};
            hccl::CpuThread* cpuThread = dynamic_cast<hccl::CpuThread*>(hostHandle.get());
            cpuThread->GetThreadEntity(deviceHandle);
            threads[i] = reinterpret_cast<ThreadHandle>(deviceHandle);
        } else if (engine == COMM_ENGINE_AICPU_TS && type == THREAD_TYPE_TS) {
            void* deviceHandle{};
            hccl::AicpuTsThread* aicpuTsThread = dynamic_cast<hccl::AicpuTsThread*>(hostHandle.get());
            aicpuTsThread->GetThreadEntity(deviceHandle);
            threads[i] = reinterpret_cast<ThreadHandle>(deviceHandle);
        } else {
            threads[i] = reinterpret_cast<ThreadHandle>(hostHandle.get());
        }
        {
            std::lock_guard<std::mutex> lock(hcomm::g_ThreadMapMtx);
            hcomm::g_ThreadMap.emplace(threads[i], hostHandle);
        }
    }

    HCCL_INFO("[HcommThreadAlloc] ThreadAcquire done: engine[%d] threadNum[%u],"
              "notifyPerThread[%u]", engine, threadNum, config.notifyNumPerThread);
    return HCCL_SUCCESS;
}

HcclResult HcommThreadServiceRegister(ThreadHandle threadHandle, ThreadService service, ThreadServiceHandle *serviceHandle)
{
    std::shared_ptr<hccl::Thread> hostThread;
    {
        std::lock_guard<std::mutex> lock(hcomm::g_ThreadMapMtx);
        auto it = hcomm::g_ThreadMap.find(threadHandle);
        if (it == hcomm::g_ThreadMap.end()) {
            HCCL_ERROR("Unknown ThreadHandle");
            return HCCL_E_NOT_FOUND;
        }
        hostThread = it->second;
    }
    hccl::CpuThread* cpuThread = dynamic_cast<hccl::CpuThread*>(hostThread.get());
    CHK_PTR_NULL(cpuThread);
    ThreadServiceHandle deviceServiceHandle{};
    CHK_RET(cpuThread->ServiceRegister(service, &deviceServiceHandle));

    *serviceHandle = deviceServiceHandle;
    return HCCL_SUCCESS;
}

HcclResult HcommThreadServiceUnregister(ThreadHandle threadHandle, ThreadServiceHandle serviceHandle)
{
    std::shared_ptr<hccl::Thread> hostThread;
    {
        std::lock_guard<std::mutex> lock(hcomm::g_ThreadMapMtx);
        auto it = hcomm::g_ThreadMap.find(threadHandle);
        if (it == hcomm::g_ThreadMap.end()) {
            HCCL_ERROR("Unknown ThreadHandle");
            return HCCL_E_NOT_FOUND;
        }
        hostThread = it->second;
    }
    hccl::CpuThread* cpuThread = dynamic_cast<hccl::CpuThread*>(hostThread.get());
    CHK_PTR_NULL(cpuThread);
    CHK_RET(cpuThread->ServiceUnregister(serviceHandle));
    return HCCL_SUCCESS;
}

HcommResult HcommEngineCtxCreate(CommEngine engine, uint64_t size, void **ctx)
{
    CHK_PTR_NULL(ctx);
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
    CHK_PTR_NULL(dstCtx);
    CHK_PTR_NULL(srcCtx);
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

/* Built-in services for AICPU engine + CPU thread type */
HcclResult RecordService(void *args, uint64_t argsSizeByte)
{
    CHK_PTR_NULL(args);
    if (argsSizeByte != sizeof(hccl::RecordServiceArgs)) {
        HCCL_ERROR("[%s] invalid argsSizeByte[%llu], expected sizeof(RecordServiceArgs)[%zu]", __func__, argsSizeByte, sizeof(hccl::RecordServiceArgs));
        return HCCL_E_PARA;
    }
    const auto &serviceArgs = *reinterpret_cast<hccl::RecordServiceArgs *>(args);
    const ThreadHandle srcThreadHdl = serviceArgs.threadHandle;
    const ThreadHandle dstThreadHdl = serviceArgs.dstThreadHandle;
    const uint32_t     dstNotifyIdx = serviceArgs.dstNotifyIdx;
    HCCL_INFO("[%s] START. srcThreadHandle[0x%llx], dstThreadHandle[0x%llx], dstNotifyIdx[%u]",
        __func__, srcThreadHdl, dstThreadHdl, dstNotifyIdx);

    std::shared_ptr<hccl::Thread> dstThreadHolder;
    {
        std::lock_guard<std::mutex> lock(hcomm::g_ThreadMapMtx);
        auto it = hcomm::g_ThreadMap.find(dstThreadHdl);
        if (it == hcomm::g_ThreadMap.end()) {
            HCCL_ERROR("[%s] FAIL. dstThreadHandle[0x%llx] not found in g_ThreadMap.", __func__, dstThreadHdl);
            return HCCL_E_NOT_FOUND;
        }
        dstThreadHolder = it->second;
    }
    hccl::Thread *const dstThread = dstThreadHolder.get();
    auto *const dstAicpuTsThread = dynamic_cast<hccl::AicpuTsThread *>(dstThread);
    if (dstAicpuTsThread == nullptr) {
        HCCL_ERROR("[%s] FAIL. g_ThreadMap.at(dstThreadHandle) is not AicpuTsThread.", __func__);
        return HCCL_E_PARA;
    }

    hccl::LocalNotify *const dstNotifyPtr = dstAicpuTsThread->GetNotify(dstNotifyIdx);
    if (dstNotifyPtr == nullptr) {
        HCCL_ERROR("[%s] FAIL. dstNotifyIdx[%u] is nullptr.", __func__, dstNotifyIdx);
        return HCCL_E_PTR;
    }
    HcclSignalInfo notifyInfo{};
    CHK_RET(dstNotifyPtr->GetNotifyData(notifyInfo));
    void *const notifyAddrOnDev = reinterpret_cast<void *>(notifyInfo.addr);
    const uint64_t recordValue = 1;
    const uint64_t notifySize = sizeof(uint8_t);  // on 91095, notify size is 8 bit.
    TRY_CATCH_RETURN(Hccl::HrtMemcpy(notifyAddrOnDev, notifySize, &recordValue, notifySize, Hccl::rtMemcpyKind_t::RT_MEMCPY_HOST_TO_DEVICE));
    return HCCL_SUCCESS;
}

HcclResult WaitService(void *args, uint64_t argsSizeByte)
{
    CHK_PTR_NULL(args);
    if (argsSizeByte != sizeof(hccl::WaitServiceArgs)) {
        HCCL_ERROR("[%s] invalid argsSizeByte[%llu], expected sizeof(WaitServiceArgs)[%zu]", __func__, argsSizeByte, sizeof(hccl::WaitServiceArgs));
        return HCCL_E_PARA;
    }
    const auto &serviceArgs = *reinterpret_cast<hccl::WaitServiceArgs *>(args);
    const ThreadHandle threadHdl = serviceArgs.threadHandle;
    const uint32_t     notifyIdx = serviceArgs.notifyIdx;
    HCCL_INFO("[%s] START. threadHandle[0x%llx], notifyIdx[%u]", __func__, threadHdl, notifyIdx);

    std::shared_ptr<hccl::Thread> threadHolder;
    {
        std::lock_guard<std::mutex> lock(hcomm::g_ThreadMapMtx);
        auto it = hcomm::g_ThreadMap.find(threadHdl);
        if (it == hcomm::g_ThreadMap.end()) {
            HCCL_ERROR("[%s] FAIL. threadHandle[0x%llx] not found in g_ThreadMap.", __func__, threadHdl);
            return HCCL_E_NOT_FOUND;
        }
        threadHolder = it->second;
    }
    hccl::Thread *const thread = threadHolder.get();
    hccl::CpuThread *const cpuThread = dynamic_cast<hccl::CpuThread *>(thread);
    if (cpuThread == nullptr) {
        HCCL_ERROR("[%s] FAIL. thread[0x%llx] is not CpuThread.", __func__, threadHdl);
        return HCCL_E_PARA;
    }

    hccl::MemNotify *const memNotify = cpuThread->GetMemNotify(notifyIdx);
    if (memNotify == nullptr) {
        HCCL_ERROR("[%s] FAIL. notifyIdx[%u] is nullptr", __func__, notifyIdx);
        return HCCL_E_PTR;
    }
    CHK_RET(memNotify->Wait());

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

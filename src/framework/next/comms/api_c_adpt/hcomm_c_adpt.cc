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
#include <cstring>

#include "hccl/hccl_res.h"
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

#include "../hcomm_res_mgr.h"

#include "param_check_pub.h"
#include "exception_handler.h"
#include "hcclCommDfx.h"
#include "hcclCommOp.h"
#include "exception_handler.h"
#include "param_check_pub.h"
#include "channel_process.h"
#include "launch_device.h"


namespace hcomm {
static std::unordered_map<ThreadHandle, std::shared_ptr<hccl::Thread>> g_ThreadMap;
static aclrtBinHandle g_BinHandle;
static std::mutex g_BinHandleMtx;
}  // namespace hcomm

using namespace hcomm;
static HcommEndpointMap g_EndpointMap;

HcclResult HcommResMgrInit(uint32_t devPhyId)
{
    // 临时方案：触发统一平台层单例触发静态对象声明
    // 内部流程触发各种单例声明，保证时序
    EXCEPTION_HANDLE_BEGIN
    HCCLV2_FUNC_RUN([&]() -> HcclResult {
        (void)HcommResMgr::GetInstance(devPhyId);
        return HcclResult::HCCL_SUCCESS;
    }());
    EXCEPTION_HANDLE_END
    return HcclResult::HCCL_SUCCESS;
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

HcclResult HcommEndpointGet(const EndpointHandle endpointHandle, void **endpoint)  // 根据endpointHandle返回Endpoint对象指针
{
    CHK_PTR_NULL(endpoint);
    auto it = g_EndpointMap.GetEndpoint(endpointHandle);
    CHK_PRT_RET(it == nullptr, HCCL_ERROR("[%s] endpoint not found, endpointHandle[%p]",
        __func__, endpointHandle), HCCL_E_NOT_FOUND);

    *endpoint = static_cast<void *>(it);
    return HCCL_SUCCESS;
}

HcclResult HcommEndpointCreate(const EndpointDesc *endpoint, EndpointHandle *endpointHandle)
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

HcclResult HcommEndpointDestroy(EndpointHandle endpointHandle)
{
    auto ret = g_EndpointMap.RemoveEndpoint(endpointHandle);
    CHK_PRT_RET(ret == false, HCCL_ERROR("[%s] endpoint not found, endpointHandle[0x%llx]",
        __func__, endpointHandle), HCCL_E_NOT_FOUND);
    endpointHandle = nullptr;

    return HCCL_SUCCESS;
}


HcclResult HcommEndpointStartListen(EndpointHandle endpointHandle, uint32_t port, HcommEndpointListenConfig* config)
{
    (void)config;
    auto endpoint = g_EndpointMap.GetEndpoint(endpointHandle);
    CHK_PRT_RET(endpoint == nullptr, HCCL_ERROR("[%s] endpoint not found, endpointHandle[%p]",
        __func__, endpointHandle), HCCL_E_NOT_FOUND);
    CHK_RET(endpoint->ServerSocketListen(port));
    return HCCL_SUCCESS;
}

HcclResult HcommEndpointStopListen(EndpointHandle endpointHandle, uint32_t port)
{
    auto endpoint = g_EndpointMap.GetEndpoint(endpointHandle);
    CHK_PRT_RET(endpoint == nullptr, HCCL_ERROR("[%s] endpoint not found, endpointHandle[%p]",
        __func__, endpointHandle), HCCL_E_NOT_FOUND);
    CHK_RET(endpoint->ServerSocketStopListen(port));
    return HCCL_SUCCESS;
}

HcclResult HcommMemReg(EndpointHandle endpointHandle, const char *memTag, HcommMem mem, void **memHandle)
{
    CHK_PTR_NULL(memHandle);
    EXCEPTION_HANDLE_BEGIN
    auto endpoint = g_EndpointMap.GetEndpoint(endpointHandle);
    CHK_PRT_RET(endpoint == nullptr, HCCL_ERROR("[%s] endpoint not found, endpointHandle[0x%llx]",
        __func__, endpointHandle), HCCL_E_NOT_FOUND);
    CHK_RET(endpoint->RegisterMemory(mem, memTag, memHandle));
    EXCEPTION_HANDLE_END
    return HCCL_SUCCESS;
}

HcclResult HcommMemUnreg(EndpointHandle endpointHandle, void *memHandle)
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

HcclResult HcommMemExport(EndpointHandle endpointHandle, void *memHandle, void **memDesc, uint32_t *memDescLen)
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

HcclResult HcommMemImport(EndpointHandle endpointHandle, const void *memDesc, uint32_t descLen, HcommMem *outMem)
{
    CHK_PTR_NULL(memDesc);
    CHK_PTR_NULL(outMem);
    auto endpoint = g_EndpointMap.GetEndpoint(endpointHandle);
    CHK_PRT_RET(endpoint == nullptr, HCCL_ERROR("[%s] endpoint not found, endpointHandle[0x%llx]",
        __func__, endpointHandle), HCCL_E_NOT_FOUND);
    CHK_RET(endpoint->MemoryImport(memDesc, descLen, outMem));
    return HCCL_SUCCESS;
}

HcclResult HcommMemUnimport(EndpointHandle endpointHandle, const void *memDesc, uint32_t descLen)
{
    CHK_PTR_NULL(memDesc);
    auto endpoint = g_EndpointMap.GetEndpoint(endpointHandle);
    CHK_PRT_RET(endpoint == nullptr, HCCL_ERROR("[%s] endpoint not found, endpointHandle[0x%llx]",
        __func__, endpointHandle), HCCL_E_NOT_FOUND);
    CHK_RET(endpoint->MemoryUnimport(memDesc, descLen));
    return HCCL_SUCCESS;
}

/* 暂未实现 */
HcclResult HcommMemGrant(EndpointHandle endpointHandle, const HcommMemGrantInfo *remoteGrantInfo)
{
    return HCCL_E_NOT_SUPPORT;
}

/* 暂未实现 */
HcclResult HcommMemRemap(const EndpointHandle endpointHandle, const HcommMem *memArray, uint64_t arraySize)
{
    return HCCL_E_NOT_SUPPORT;
}

HcclResult HcommMemGetAllMemHandles(EndpointHandle endpointHandle, void **memHandles, uint32_t *memHandleNum)
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
HcclResult HcommCollectiveChannelCreate(EndpointHandle endpointHandle, CommEngine engine,
    HcommChannelDesc *channelDescs, uint32_t channelNum, ChannelHandle *channels)
{
    CHK_PTR_NULL(channelDescs);
    CHK_PTR_NULL(channels);
    CHK_PRT_RET((channelNum == 0), HCCL_ERROR("[%s]Invalid channelNum, channelNum[%u]",
        __func__, channelNum), HCCL_E_PARA);
    HCCL_INFO("[%s] START. endpointHandle[0x%llx], engine[%d], channelNum[%u].",
        __func__, endpointHandle, engine, channelNum);

    return ChannelProcess::CreateChannelsLoop(endpointHandle, engine, channelDescs, channelNum, channels);
}

HcclResult HcommChannelUpdateMemInfo(void **memHandles, uint32_t memHandleNum, ChannelHandle channelHandle)
{
    return ChannelProcess::ChannelUpdateMemInfo(memHandles, memHandleNum, channelHandle);
}

HcclResult HcommChannelCreate(EndpointHandle endpointHandle, CommEngine engine,
    HcommChannelDesc *channelDescs, uint32_t channelNum, ChannelHandle *channels)
{
    CHK_PTR_NULL(channelDescs);
    CHK_PTR_NULL(channels);
    CHK_PRT_RET((channelNum == 0), HCCL_ERROR("[%s]Invalid channelNum, channelNum[%u]",
        __func__, channelNum), HCCL_E_PARA);
    HCCL_INFO("[%s] START. endpointHandle[0x%llx], engine[%d], channelNum[%u].",
        __func__, endpointHandle, engine, channelNum);

    std::vector<ChannelHandle> hostChannelHandles(channelNum);
    ChannelHandle* targetChannels = hostChannelHandles.data();

    CHK_RET(ChannelProcess::CreateChannelsLoop(endpointHandle, engine, channelDescs, channelNum, targetChannels));
    CHK_RET(ChannelProcess::ConnectChannels(targetChannels, channelNum, engine));
    CHK_RET(EnsureKernelBinLoaded(engine));
    CHK_RET(ChannelProcess::SaveChannels(targetChannels, channels, channelNum, engine, g_BinHandle));

    return HCCL_SUCCESS;
}

HcclResult HcommChannelGet(const ChannelHandle channelHandle, void **channel)
{
    CHK_PTR_NULL(channel);
    return ChannelProcess::ChannelGet(channelHandle, channel);
}

HcclResult HcommChannelGetStatus(const ChannelHandle *channelList, uint32_t listNum,  int32_t* statusList)
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

HcclResult HcommChannelGetNotifyNum(ChannelHandle channelHandle, uint32_t *notifyNum)
{
    CHK_PTR_NULL(notifyNum);
    return ChannelProcess::ChannelGetNotifyNum(channelHandle, notifyNum);
}

HcclResult HcommChannelDestroy(const ChannelHandle *channels, uint32_t channelNum)
{
    CHK_PTR_NULL(channels);
    return ChannelProcess::ChannelDestroy(channels, channelNum, g_BinHandle);
}

HcclResult HcommChannelGetRemoteMem(ChannelHandle channelHandle, HcommMem **remoteMem, uint32_t *memNum, char **memTags)
{
    CHK_PTR_NULL(remoteMem);
    CHK_PTR_NULL(memNum);
    CHK_PTR_NULL(memTags);
    return ChannelProcess::ChannelGetRemoteMem(channelHandle, remoteMem, memNum, memTags);
}

HcclResult HcommThreadAlloc(CommEngine engine, uint32_t threadNum, uint32_t notifyNumPerThread, ThreadHandle *threads) {
    CHK_PTR_NULL(threads);
    HCCL_INFO("[%s] ThreadAcquire begin. engine[%d], threadNum[%u], notifyPerThread[%u], threads[%p]",
        __func__, engine, threadNum, notifyNumPerThread, threads);

    // 1. 参数校验
    CHK_RET(hccl::ValidateThreadParams(threadNum, notifyNumPerThread));

    // 2. 获取引擎对应的类型
    hccl::NotifyLoadType notifyLoadType;
    hccl::StreamType streamType;
    CHK_RET(hccl::CommEngineToNotifyLoadType(engine, notifyLoadType));
    CHK_RET(hccl::CommEngineToStreamType(engine, streamType));

    // 3. 创建线程
    std::vector<std::shared_ptr<hccl::Thread>> newThreads;
    hccl::ThreadCreateParams params(engine, threadNum, notifyNumPerThread, notifyLoadType, streamType);
    CHK_RET(hccl::CreateAndInitThreads(params, newThreads));

    // 4. 插入全局映射表
    CHK_RET(hccl::SaveThreads(newThreads));

    // 5. 储存线程句柄
    CHK_RET(EnsureKernelBinLoaded(engine));
    CHK_RET(hccl::StoreThreadHandles(newThreads, threads, engine, g_BinHandle));

    HCCL_INFO("[HcommThreadAlloc] ThreadAcquire done: engine[%d] threadNum[%u], notifyPerThread[%u]",
              engine, threadNum, notifyNumPerThread);
    return HCCL_SUCCESS;
}

HcclResult HcommThreadFree(const ThreadHandle *threads, uint32_t threadNum)
{
    CHK_PTR_NULL(threads);
    return hccl::FreeThreads(threads, threadNum, g_BinHandle);
}

HcclResult HcommThreadAllocWithStream(CommEngine engine,
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

HcclResult HcommEngineCtxCreate(CommEngine engine, uint64_t size, void **ctx)
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

HcclResult HcommEngineCtxDestroy(CommEngine engine, void *ctx)
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

HcclResult HcommEngineCtxCopy(CommEngine engine, void *dstCtx, const void *srcCtx, uint64_t size)
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

HcclResult HcommDfxKernelLaunch(const std::string &commTag, aclrtBinHandle binHandle, HcclDfxOpInfo dfxOpInfo)
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
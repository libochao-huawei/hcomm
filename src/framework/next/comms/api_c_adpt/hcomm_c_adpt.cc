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
#include "hcclCommDfx.h"
#include "hcclCommOp.h"
#include "exception_handler.h"
#include "param_check_pub.h"
#include "launch_device.h"


namespace hcomm {
static std::unordered_map<ChannelHandle, std::unique_ptr<Channel>> g_ChannelMap;
static std::unordered_map<ChannelHandle, ChannelHandle> g_ChannelD2HMap;
static std::unordered_map<ThreadHandle, std::shared_ptr<hccl::Thread>> g_ThreadMap;
static aclrtBinHandle g_BinHandle;
static std::mutex g_ChannelMapMtx;
static std::mutex g_BinHandleMtx;
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

HcclResult HcommEndpointGet(EndpointHandle endpointHandle, void **endpoint)  // 根据endpointHandle返回Endpoint对象指针
{
    auto it = g_EndpointMap.GetEndpoint(endpointHandle);
    CHK_PRT_RET(it == nullptr, HCCL_ERROR("[%s] endpoint not found in g_EndpointMap, endpointHandle[%p]",
        __func__, endpointHandle), HCCL_E_PARA);

    *endpoint = static_cast<void *>(it);
    return HCCL_SUCCESS;
}

HcclResult HcommEndpointCreate(const EndpointDesc *endpoint, EndpointHandle *endpointHandle)
{
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
    auto endpoint = g_EndpointMap.GetEndpoint(endpointHandle);
    CHK_RET(endpoint->GetAllMemHandles(memHandles, memHandleNum));
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
        // 先检查是否会溢出，再执行复制操作
        CHK_PRT_RET((packSize + mem.size()) > dstMax,
            HCCL_ERROR("[%s] fail, packSize[%llu] + mem.size[%zu] is bigger than dstMax[%llu]", 
                __func__, packSize, mem.size(), dstMax),
            HCCL_E_PARA);

        CHK_SAFETY_FUNC_RET(memcpy_s(dstPtr, mem.size(), mem.data(), mem.size()));
        dstPtr += mem.size();  // 移动目标指针
        packSize += mem.size();
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
    hccl::DeviceMem remoteRankList = hccl::DeviceMem::alloc(listNum * sizeof(u32));
    CHK_PTR_NULL(remoteRankList.ptr());
    std::vector<u32> remoteRankIdList(listNum);
    for ( u32 i = 0; i < listNum; ++i) {
        CHK_RET(hccl::HcclCommDfx::GetChannelRemoteRankId(commTag, hostChannelHandles[i], remoteRankIdList[i]));
    }
    // 通过安全的内存拷贝将主机内存数据传输到设备内存
    CHK_RET(hrtMemSyncCopy(remoteRankList.ptr(), listNum * sizeof(u32), remoteRankIdList.data(), 
            listNum * sizeof(u32), HcclRtMemcpyKind::HCCL_RT_MEMCPY_KIND_HOST_TO_DEVICE));
    channelParam.remoteRankList = static_cast<u32 *>(remoteRankList.ptr());
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
    u16 timeOut = NOTIFY_DEFAULT_WAIT_TIME > std::numeric_limits<uint16_t>::max() ? 
                    std::numeric_limits<uint16_t>::max() : NOTIFY_DEFAULT_WAIT_TIME;
    CHK_RET(hccl::AicpuAclKernelLaunch(localStream.ptr(),
        reinterpret_cast<void *>(&customInitTask),
        sizeof(customInitTask),
        binHandle,
        kernelName,
        true,
        timeOut));

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
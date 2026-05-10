/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "aicpu_launch_manager.h"
#include "adapter_rts_common.h"
#include "mem_device_pub.h"
#include "notify_manager.h"
#include "launch_aicpu.h"
#include "comm_configer.h"
#include <iomanip>
#include "hcom_host_profiling.h"
#include "adapter_prof.h"

namespace hccl {
template <typename OpParam, typename ApiParam>
HcclResult AicpuLaunchMgr::KernelLaunch(OpParam &opParam, ApiParam &apiParam, rtStream_t aicpuInitStream)
{
    return HCCL_SUCCESS;
}

HcclResult AicpuLaunchMgr::KernelLaunchAicpuCustom(uint64_t context, std::string kernelName, rtStream_t aicpuInitStream,
    aclrtBinHandle binCustomHandle)
{
    u16 timeOut = NOTIFY_DEFAULT_WAIT_TIME > std::numeric_limits<uint16_t>::max() ? 
                    std::numeric_limits<uint16_t>::max() : NOTIFY_DEFAULT_WAIT_TIME;
    CHK_RET(AicpuAclKernelLaunch(aicpuInitStream, &context,
        sizeof(context), binCustomHandle, kernelName, true, timeOut));
    return HCCL_SUCCESS;
}

HcclResult AicpuLaunchMgr::AiCpuStreamAllocAndGet(rtStream_t &aiCpuStream)
{
    if (opStream_.ptr() != nullptr) {
        HCCL_INFO("%s already alloc, stream id:%u", __func__, opStream_.id());
        aiCpuStream = opStream_.ptr();
        return HCCL_SUCCESS;
    }

    constexpr u32 aicpuStreamMode = 1; // 单独申请的kernel流，使能遇错即停，避免出错后流卡住不退
    opStream_ = Stream(StreamType::STREAM_TYPE_ONLINE);
    CHK_RET(hrtStreamSetMode(opStream_.ptr(), aicpuStreamMode));
    aiCpuStream = opStream_.ptr();
    HCCL_RUN_INFO("[AicpuLaunchMgr][%s] alloc success, stream id:%u, aicpuStreamMode:%u",
                    __func__, opStream_.id(), aicpuStreamMode);
    return HCCL_SUCCESS;
}

static HcclResult CreateLocalStream(Stream &localStream)
{
    HCCL_INFO("YYYYYY hcomm resource [CreateLocalStream] enter file[%s], localStreamPtrBefore[%p].",
        __FILE__, localStream.ptr());
    localStream = Stream(StreamType::STREAM_TYPE_ONLINE);
    constexpr u32 aicpuStreamMode = 1;
    HcclResult ret = hrtStreamSetMode(localStream.ptr(), aicpuStreamMode);
    HCCL_INFO("YYYYYY hcomm resource [CreateLocalStream] exit file[%s], streamPtr[%p], streamId[%u], "
        "streamMode[%u], ret[%u].", __FILE__, localStream.ptr(), localStream.id(), aicpuStreamMode,
        static_cast<u32>(ret));
    CHK_RET(ret);
    return HCCL_SUCCESS;
}

static HcclResult PrepareThreadMgrParam(const std::vector<std::shared_ptr<Thread>> &newThreads,
                                        const ThreadKernelLaunchConfig &config,
                                        ThreadMgrAicpuParam &opParam,
                                        DeviceMem &deviceHandle)
{
    HCCL_INFO("[%s] fill opParam", __func__);
    (void)memset_s(&opParam, sizeof(opParam), 0, sizeof(opParam));
    opParam.threadNum = newThreads.size();

    // 拷贝 commId
    errno_t sRet = strncpy_s(opParam.hcomId, HCOMID_MAX_SIZE, config.commId.c_str(), config.commId.length());
    CHK_PRT_RET(sRet != EOK,
        HCCL_ERROR("[%s] strncpy_s failed, return [%d].", __func__, sRet),
        HCCL_E_MEMORY);
    opParam.hcomId[HCOMID_MAX_SIZE - 1] = '\0';

    // 拷贝每个线程的 unique id
    for (u32 i = 0; i < opParam.threadNum; ++i) {
        const std::string &uid = newThreads[i]->GetUniqueId();
        size_t copyLen = std::min(uid.size(), static_cast<size_t>(THREAD_UNIQUE_ID_MAX_SIZE));
        sRet = memcpy_s(opParam.threadParam[i], THREAD_UNIQUE_ID_MAX_SIZE, uid.c_str(), copyLen);
        CHK_PRT_RET(sRet != EOK,
            HCCL_ERROR("[%s] memcpy_s failed, return [%d].", __func__, sRet),
            HCCL_E_MEMORY);
        opParam.threadParam[i][THREAD_UNIQUE_ID_MAX_SIZE - 1] = '\0';

        if (UNLIKELY(HcclCheckLogLevel(HCCL_LOG_INFO))) {
            std::ostringstream oss;
            oss << "threadParam[" << i << "] raw bytes: ";
            for (u32 j = 0; j < THREAD_UNIQUE_ID_MAX_SIZE; ++j) {
                oss << std::hex << std::setw(2) << std::setfill('0')
                    << static_cast<unsigned int>(static_cast<unsigned char>(opParam.threadParam[i][j])) << " ";
            }
            HCCL_INFO("[%s] %s", __func__, oss.str().c_str());
        }
    }

    // 分配设备内存
    size_t handleLen = sizeof(ThreadHandle) * newThreads.size();
    deviceHandle = DeviceMem::alloc(handleLen);
    CHK_SMART_PTR_NULL(deviceHandle);
    opParam.deviceHandle = deviceHandle.ptr();

    // 基础通信需要设备信息
    if (config.needDeviceInfo) {
        CHK_RET(hrtGetDevice(&opParam.deviceLogicId));
        DevType devType;
        CHK_RET(hrtGetDeviceType(devType));
        opParam.deviceType = static_cast<u32>(devType);
    }
    return HCCL_SUCCESS;
}

HcclResult AicpuLaunchMgr::ThreadKernelLaunchImpl(
    std::vector<std::shared_ptr<Thread>> &newThreads,
    std::unique_ptr<ThreadHandle[]> &aicpuHandle,
    const ThreadKernelLaunchConfig &config)
{
    HCCL_INFO("YYYYYY hcomm resource [ThreadKernelLaunchImpl] enter file[%s], commId[%s], kernelName[%s], "
        "threadNum[%zu], timeoutSec[%u], needDeviceInfo[%d], needProfiling[%d], isSupplementNotify[%d], "
        "binHandle[%p].", __FILE__, config.commId.c_str(), config.kernelName.c_str(), newThreads.size(),
        config.timeoutSec, config.needDeviceInfo, config.needProfiling, config.isSupplementNotify,
        config.binHandle);
    // 参数检查
    CHK_PRT_RET(newThreads.size() > SIGNAL_DEV_STREAM_MAX_NUM,
        HCCL_ERROR("[AicpuLaunchMgr][%s] streamNum[%zu] > SIGNAL_DEV_STREAM_MAX_NUM[%u]", __func__,
        newThreads.size(), SIGNAL_DEV_STREAM_MAX_NUM), HCCL_E_PARA);

    uint64_t beginTime = (config.needProfiling ? hrtMsprofSysCycleTime() : 0);

    // Step 1. 创建局部 stream
    Stream localStream;
    HcclResult ret = CreateLocalStream(localStream);
    HCCL_INFO("YYYYYY hcomm resource [ThreadKernelLaunchImpl] CreateLocalStream ret[%u], commId[%s], "
        "kernelName[%s], streamPtr[%p], streamId[%u].", static_cast<u32>(ret), config.commId.c_str(),
        config.kernelName.c_str(), localStream.ptr(), localStream.id());
    CHK_RET(ret);

    // Step 2. 填写 opParam 并分配设备内存
    ThreadMgrAicpuParam opParam{};
    DeviceMem deviceHandle;
    ret = PrepareThreadMgrParam(newThreads, config, opParam, deviceHandle);
    HCCL_INFO("YYYYYY hcomm resource [ThreadKernelLaunchImpl] PrepareThreadMgrParam ret[%u], commId[%s], "
        "kernelName[%s], threadNum[%u], deviceLogicId[%u], deviceType[%u], deviceHandle[%p], "
        "deviceHandleMem[%p].", static_cast<u32>(ret), config.commId.c_str(), config.kernelName.c_str(),
        opParam.threadNum, opParam.deviceLogicId, opParam.deviceType, opParam.deviceHandle, deviceHandle.ptr());
    CHK_RET(ret);

    size_t handleLen = sizeof(ThreadHandle) * newThreads.size();
    // Step 3. 补充notify，将threadHanle拷到device侧
    if (config.isSupplementNotify) {
        ret = hrtMemSyncCopy(opParam.deviceHandle, handleLen, aicpuHandle.get(), handleLen,
            HcclRtMemcpyKind::HCCL_RT_MEMCPY_KIND_HOST_TO_DEVICE);
        HCCL_INFO("YYYYYY hcomm resource [ThreadKernelLaunchImpl] supplement H2D ret[%u], commId[%s], "
            "kernelName[%s], handleLen[%zu], deviceHandle[%p], hostHandles[%p].", static_cast<u32>(ret),
            config.commId.c_str(), config.kernelName.c_str(), handleLen, opParam.deviceHandle, aicpuHandle.get());
        CHK_RET(ret);
    }

    // Step 4. 调用 KernelLaunch
    DeviceMem addr = DeviceMem::alloc(sizeof(opParam));
    ret = hrtMemSyncCopy(addr.ptr(), sizeof(opParam), &opParam, sizeof(opParam),
        HcclRtMemcpyKind::HCCL_RT_MEMCPY_KIND_HOST_TO_DEVICE);
    HCCL_INFO("YYYYYY hcomm resource [ThreadKernelLaunchImpl] opParam H2D ret[%u], commId[%s], "
        "kernelName[%s], opParamDevAddr[%p], opParamSize[%zu].", static_cast<u32>(ret),
        config.commId.c_str(), config.kernelName.c_str(), addr.ptr(), sizeof(opParam));
    CHK_RET(ret);
    uint64_t context = reinterpret_cast<uint64_t>(addr.ptr());
    ret = KernelLaunchAicpuCustom(context, config.kernelName.c_str(), localStream.ptr(), config.binHandle);
    HCCL_INFO("YYYYYY hcomm resource [ThreadKernelLaunchImpl] KernelLaunchAicpuCustom ret[%u], "
        "commId[%s], kernelName[%s], context[0x%llx], streamPtr[%p], streamId[%u], timeoutSec[%u], "
        "deviceLogicId[%u], deviceType[%u], deviceHandle[%p].", static_cast<u32>(ret), config.commId.c_str(),
        config.kernelName.c_str(), context, localStream.ptr(), localStream.id(), config.timeoutSec,
        opParam.deviceLogicId, opParam.deviceType, opParam.deviceHandle);
    CHK_PRT_RET(ret != HCCL_SUCCESS,
        HCCL_ERROR("[AicpuLaunchMgr][%s] KernelLaunch failed, return [%d].", __func__, ret), ret);

    // Step 5. 等待流完成
    ret = hcclStreamSynchronize(localStream.ptr(), config.timeoutSec);
    HCCL_INFO("YYYYYY hcomm resource [ThreadKernelLaunchImpl] hcclStreamSynchronize ret[%u], "
        "commId[%s], kernelName[%s], streamPtr[%p], streamId[%u], timeoutSec[%u].",
        static_cast<u32>(ret), config.commId.c_str(), config.kernelName.c_str(), localStream.ptr(),
        localStream.id(), config.timeoutSec);
    CHK_RET(ret);

    // Step 6. 非补充notify,返回 device 侧句柄
    if (!config.isSupplementNotify) {
        ret = hrtMemSyncCopy(aicpuHandle.get(), handleLen, opParam.deviceHandle, handleLen,
            HcclRtMemcpyKind::HCCL_RT_MEMCPY_KIND_DEVICE_TO_HOST);
        HCCL_INFO("YYYYYY hcomm resource [ThreadKernelLaunchImpl] D2H ret[%u], commId[%s], "
            "kernelName[%s], handleLen[%zu], deviceHandle[%p], hostHandles[%p], firstHandle[0x%llx].",
            static_cast<u32>(ret), config.commId.c_str(), config.kernelName.c_str(), handleLen,
            opParam.deviceHandle, aicpuHandle.get(),
            ((ret == HCCL_SUCCESS && newThreads.size() > 0 && aicpuHandle != nullptr) ? aicpuHandle[0] : 0ULL));
        CHK_RET(ret);
    }

    // 性能分析上报
    if (config.needProfiling) {
        HcommProfilingReportKernel(beginTime, config.kernelName.c_str());
    }
    HCCL_INFO("YYYYYY hcomm resource [ThreadKernelLaunchImpl] exit file[%s], commId[%s], kernelName[%s], "
        "threadNum[%zu], firstHandle[0x%llx], ret[%u].", __FILE__, config.commId.c_str(),
        config.kernelName.c_str(), newThreads.size(),
        ((newThreads.empty() || aicpuHandle == nullptr) ? 0ULL : aicpuHandle[0]), static_cast<u32>(HCCL_SUCCESS));
    return HCCL_SUCCESS;
}

// 集合通信使用，待归一到基础通信接口
HcclResult AicpuLaunchMgr::ThreadKernelLaunchForComm(std::vector<std::shared_ptr<Thread>> &newThreads,
    const std::string &commId, std::unique_ptr<ThreadHandle[]> &aicpuHandle, aclrtBinHandle binHandle)
{
    ThreadKernelLaunchConfig config(
        commId,
        binHandle,
        "RunAicpuIndOpThreadInit",
        false,
        CommConfiger::GetInstance().GetCommConfigExecTimeOut(commId),
        true,
        false
    );
    HCCL_INFO("YYYYYY hcomm resource [ThreadKernelLaunchForComm] enter file[%s], commId[%s], "
        "kernelName[%s], threadNum[%zu], timeoutSec[%u], binHandle[%p].", __FILE__, commId.c_str(),
        config.kernelName.c_str(), newThreads.size(), config.timeoutSec, binHandle);
    HcclResult ret = ThreadKernelLaunchImpl(newThreads, aicpuHandle, config);
    HCCL_INFO("YYYYYY hcomm resource [ThreadKernelLaunchForComm] exit file[%s], commId[%s], "
        "kernelName[%s], threadNum[%zu], firstHandle[0x%llx], ret[%u].", __FILE__, commId.c_str(),
        config.kernelName.c_str(), newThreads.size(),
        ((ret == HCCL_SUCCESS && !newThreads.empty() && aicpuHandle != nullptr) ? aicpuHandle[0] : 0ULL),
        static_cast<u32>(ret));
    return ret;
}

// 基础通信使用
HcclResult AicpuLaunchMgr::ThreadKernelLaunchForBase(std::vector<std::shared_ptr<Thread>> &newThreads,
    std::unique_ptr<ThreadHandle[]> &aicpuHandle, aclrtBinHandle binHandle)
{
    constexpr uint32_t defaultTimeOutSec = 120;
    ThreadKernelLaunchConfig config(
        "",      
        binHandle,
        "RunAicpuThreadInit",
        true,    
        defaultTimeOutSec,     
        false,
        false
    );
    return ThreadKernelLaunchImpl(newThreads, aicpuHandle, config);
}

// 补充notify使用
HcclResult AicpuLaunchMgr::SupplementNotifyKernelLaunch(std::vector<std::shared_ptr<Thread>> &newThreads,
    const std::string &commId, std::unique_ptr<ThreadHandle[]> &aicpuHandle, aclrtBinHandle binHandle)
{
    constexpr uint32_t defaultTimeOutSec = 120;
    ThreadKernelLaunchConfig config(
        commId,      
        binHandle,
        "RunAicpuThreadSupplementNotify",
        true,    
        defaultTimeOutSec,     
        false,
        true
    );
    return ThreadKernelLaunchImpl(newThreads, aicpuHandle, config);
}

HcclResult AicpuLaunchMgr::ThreadKernelLaunchDestroy(ThreadHandle *threadHandles, uint32_t listNum, 
    aclrtBinHandle binHandle)
{
    HCCL_INFO("YYYYYY hcomm resource [ThreadKernelLaunchDestroy] enter file[%s], listNum[%u], "
        "threadHandles[%p], firstThreadHandle[0x%llx], binHandle[%p].", __FILE__, listNum, threadHandles,
        ((threadHandles != nullptr && listNum > 0) ? threadHandles[0] : 0ULL), binHandle);
    // Step 1. 创建局部 stream
    Stream localStream;
    HcclResult ret = CreateLocalStream(localStream);
    HCCL_INFO("YYYYYY hcomm resource [ThreadKernelLaunchDestroy] CreateLocalStream ret[%u], "
        "streamPtr[%p], streamId[%u].", static_cast<u32>(ret), localStream.ptr(), localStream.id());
    CHK_RET(ret);

    // Step 2. 填写 opParam
    ThreadMgrAicpuParam opParam{};
    (void)memset_s(&opParam, sizeof(opParam), 0, sizeof(opParam));
    opParam.threadNum = listNum;
    size_t handleLen = sizeof(ThreadHandle) * listNum;
    DeviceMem deviceHandle = DeviceMem::alloc(handleLen);
    CHK_SMART_PTR_NULL(deviceHandle);
    opParam.deviceHandle = deviceHandle.ptr();
    ret = hrtMemSyncCopy(deviceHandle.ptr(), handleLen, threadHandles, handleLen,
        HcclRtMemcpyKind::HCCL_RT_MEMCPY_KIND_HOST_TO_DEVICE);
    HCCL_INFO("YYYYYY hcomm resource [ThreadKernelLaunchDestroy] handles H2D ret[%u], listNum[%u], "
        "handleLen[%zu], deviceHandle[%p], firstThreadHandle[0x%llx].", static_cast<u32>(ret), listNum,
        handleLen, deviceHandle.ptr(), ((threadHandles != nullptr && listNum > 0) ? threadHandles[0] : 0ULL));
    CHK_RET(ret);

    // Step 3. 调用 KernelLaunch
    std::string kernelName = "RunAicpuThreadDestroy";
    DeviceMem addr = DeviceMem::alloc(sizeof(opParam));
    ret = hrtMemSyncCopy(addr.ptr(), sizeof(opParam), &opParam, sizeof(opParam),
        HcclRtMemcpyKind::HCCL_RT_MEMCPY_KIND_HOST_TO_DEVICE);
    HCCL_INFO("YYYYYY hcomm resource [ThreadKernelLaunchDestroy] opParam H2D ret[%u], kernelName[%s], "
        "opParamDevAddr[%p], opParamSize[%zu].", static_cast<u32>(ret), kernelName.c_str(), addr.ptr(),
        sizeof(opParam));
    CHK_RET(ret);
    uint64_t context = reinterpret_cast<uint64_t>(addr.ptr());
    ret = KernelLaunchAicpuCustom(context, kernelName.c_str(), localStream.ptr(), binHandle);
    HCCL_INFO("YYYYYY hcomm resource [ThreadKernelLaunchDestroy] KernelLaunchAicpuCustom ret[%u], "
        "kernelName[%s], context[0x%llx], streamPtr[%p], streamId[%u], threadNum[%u], deviceHandle[%p].",
        static_cast<u32>(ret), kernelName.c_str(), context, localStream.ptr(), localStream.id(),
        opParam.threadNum, opParam.deviceHandle);
    CHK_PRT_RET(ret != HCCL_SUCCESS,
        HCCL_ERROR("[AicpuLaunchMgr][%s] KernelLaunch failed, return [%d].", __func__, ret), ret);

    // Step 4. 等待流完成
    constexpr uint32_t defaultTimeOutSec = 120;
    ret = hcclStreamSynchronize(localStream.ptr(), defaultTimeOutSec);
    HCCL_INFO("YYYYYY hcomm resource [ThreadKernelLaunchDestroy] hcclStreamSynchronize ret[%u], "
        "kernelName[%s], streamPtr[%p], streamId[%u], timeoutSec[%u].", static_cast<u32>(ret),
        kernelName.c_str(), localStream.ptr(), localStream.id(), defaultTimeOutSec);
    CHK_RET(ret);
    HCCL_INFO("YYYYYY hcomm resource [ThreadKernelLaunchDestroy] exit file[%s], listNum[%u], "
        "firstThreadHandle[0x%llx], ret[%u].", __FILE__, listNum,
        ((threadHandles != nullptr && listNum > 0) ? threadHandles[0] : 0ULL), static_cast<u32>(ret));
    return HCCL_SUCCESS;
}

// 准备 opParam
HcclResult AicpuLaunchMgr::PrepareAicpuNotifyParam(NotifyMgrAicpuParam &opParam,
    const std::string &commId, size_t notifyNum, bool freeFlag, void *deviceHandle)
{
    (void)memset_s(&opParam, sizeof(opParam), 0, sizeof(opParam));

    opParam.notifyNum = notifyNum;
    opParam.freeFlag = freeFlag;
    opParam.deviceHandle = deviceHandle;

    errno_t sRet = strncpy_s(opParam.hcomId, HCOMID_MAX_SIZE, commId.c_str(), commId.length());
    CHK_PRT_RET(sRet != EOK,
        HCCL_ERROR("[AicpuLaunchMgr][PrepareAicpuNotifyParam] strncpy_s failed, ret[%d]", sRet),
        HCCL_E_MEMORY);
    opParam.hcomId[HCOMID_MAX_SIZE - 1] = '\0';
    return HCCL_SUCCESS;
}

HcclResult AicpuLaunchMgr::LaunchNotifyKernel(NotifyMgrAicpuParam &opParam, aclrtBinHandle binCustomHandle)
{
    Stream localStream(StreamType::STREAM_TYPE_ONLINE);
    constexpr u32 aicpuStreamMode = 1;
    CHK_RET(hrtStreamSetMode(localStream.ptr(), aicpuStreamMode));

    DeviceMem addr = DeviceMem::alloc(sizeof(opParam));
    CHK_RET(hrtMemSyncCopy(addr.ptr(), sizeof(opParam), &opParam, sizeof(opParam),
        HcclRtMemcpyKind::HCCL_RT_MEMCPY_KIND_HOST_TO_DEVICE));
    uint64_t context = reinterpret_cast<uint64_t>(addr.ptr());
    HcclResult ret = KernelLaunchAicpuCustom(context, "RunAicpuIndOpNotify", localStream.ptr(), binCustomHandle);
    CHK_PRT_RET(ret != HCCL_SUCCESS,
        HCCL_ERROR("[AicpuLaunchMgr][LaunchNotifyKernel] KernelLaunch failed, ret[%d]", ret), ret);
    CHK_RET(hcclStreamSynchronize(localStream.ptr(), CommConfiger::GetInstance().GetCommConfigExecTimeOut(opParam.hcomId)));
    return HCCL_SUCCESS;
}

HcclResult AicpuLaunchMgr::NotifyKernelLaunchAlloc(std::vector<std::unique_ptr<LocalNotify>> &newNotifys,
    const std::string &commId, std::unique_ptr<NotifyHandle[]> &hostHandle, aclrtBinHandle binCustomHandle)
{
    size_t handleLen = sizeof(NotifyHandle) * newNotifys.size();
    DeviceMem deviceHandle = DeviceMem::alloc(handleLen);
    CHK_SMART_PTR_NULL(deviceHandle);

    NotifyMgrAicpuParam opParam;
    CHK_RET(PrepareAicpuNotifyParam(opParam, commId, newNotifys.size(), false, deviceHandle.ptr()));
    std::string uid = NotifyManager::GetBinNotifys(newNotifys, NotifyLoadType::DEVICE_NOTIFY);
    if (UNLIKELY(uid.empty())) {
        HCCL_ERROR("[AicpuLaunchMgr][%s] uid is empty.", __func__, HCCL_E_MEMORY);
        return HCCL_E_MEMORY;
    }
    size_t copyLen = std::min(uid.size(), static_cast<size_t>(NOTIFY_UNIQUE_ID_MAX_SIZE));
    errno_t sRet = memcpy_s(opParam.notifyParam, NOTIFY_UNIQUE_ID_MAX_SIZE, uid.c_str(), copyLen);
    CHK_PRT_RET(sRet != EOK, HCCL_ERROR("[AicpuLaunchMgr][%s] call memcpy_s failed, return [%d].", __func__, sRet),
        HCCL_E_MEMORY);
    // 打印每个字节
    if (UNLIKELY(HcclCheckLogLevel(HCCL_LOG_INFO))) {
        std::ostringstream oss;
        oss << "notifyParam" << " raw bytes: ";
        for (u32 i = 0; i < NOTIFY_UNIQUE_ID_MAX_SIZE; ++i) {
            oss << std::hex << std::setw(2) << std::setfill('0')
                << static_cast<unsigned int>(static_cast<unsigned char>(opParam.notifyParam[i])) << " ";
        }
        HCCL_INFO("[AicpuLaunchMgr][%s] %s", __func__, oss.str().c_str());
    }

    CHK_RET(LaunchNotifyKernel(opParam, binCustomHandle));

    CHK_RET(hrtMemSyncCopy(hostHandle.get(), handleLen, opParam.deviceHandle,
        handleLen, HcclRtMemcpyKind::HCCL_RT_MEMCPY_KIND_DEVICE_TO_HOST));
    HCCL_RUN_INFO("[AicpuLaunchMgr][%s] notify alloc success, commid[%s], notifyNum[%u]",
        __func__, commId.c_str(), newNotifys.size());
    return HCCL_SUCCESS;
}

HcclResult AicpuLaunchMgr::NotifyKernelLaunchFree(std::vector<NotifyHandle> &aicpuNotifys, uint32_t notifyNum,
    const std::string &commId, aclrtBinHandle binCustomHandle)
{
    size_t handleLen = sizeof(NotifyHandle) * notifyNum;
    DeviceMem deviceHandle = DeviceMem::alloc(handleLen);
    CHK_SMART_PTR_NULL(deviceHandle);

    CHK_RET(hrtMemSyncCopy(deviceHandle.ptr(), handleLen, aicpuNotifys.data(),
        handleLen, HcclRtMemcpyKind::HCCL_RT_MEMCPY_KIND_HOST_TO_DEVICE));

    NotifyMgrAicpuParam opParam;
    CHK_RET(PrepareAicpuNotifyParam(opParam, commId, notifyNum, true, deviceHandle.ptr()));

    CHK_RET(LaunchNotifyKernel(opParam, binCustomHandle));
    HCCL_RUN_INFO("[AicpuLaunchMgr][%s] notify free kernalLaunch success, commid[%s], notifyNum[%u]",
        __func__, commId.c_str(), aicpuNotifys.size());
    return HCCL_SUCCESS;
}
}

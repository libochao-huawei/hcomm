/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "../rank/my_rank.h"
#include "hccl_comm_pub.h"
#include "exception_handler.h"
#include "env_config.h"
#include "../common/loggers/channel_logger.h"  // 日志记录器

#include "hcom_common.h"
#include "ccu_kernel.h"
#include "../comms/ccu/ccu_kernel/ccu_kernel_mgr.h"
#include "rt_external.h"
#include "hccl_ccu_res.h"

using namespace hccl;
/**
 * @note 职责：集合通信的通信域资源管理的C接口的C到C++适配
 */

/**
 * @note C接口适配参考示例
 * @code {.c}
 * HcclResult HcclThreadAcquire(HcclComm comm, CommEngine engine, uint32_t threadNum,
 *     uint32_t notifyNumPerThread, ThreadHandle *threads) {
 *     return HCCL_SUCCESS;
 * }
 * @endcode
 */

const uint32_t HCCL_CHANNEL_VERSION_ONE = 1;
HcclResult ProcessHcclResPackReq(const HcclChannelDesc &channelDesc, HcclChannelDesc &channelDescFinal)
{
    if (channelDesc.header.size < channelDescFinal.header.size) {
        // 需要前向兼容HcclChannelDesc，末尾部分字段不支持处理
    } else if (channelDesc.header.size > channelDescFinal.header.size) {
        // 需要后向向兼容HcclChannelDesc，末尾部分字段会被忽略
    }
 
    if (channelDesc.header.magicWord != channelDescFinal.header.magicWord) {
        HCCL_ERROR("[%s]channelDescFinal.header.magicWord[%u] not equal to channelDesc.header.magicWord[%u]",
            __func__, channelDescFinal.header.magicWord, channelDesc.header.magicWord);
        return HCCL_E_PARA;
    }
 
    uint32_t copySize = (channelDescFinal.header.size < channelDesc.header.size ?
        channelDescFinal.header.size : channelDesc.header.size) - sizeof(CommAbiHeader);
    CHK_SAFETY_FUNC_RET(memcpy_s(reinterpret_cast<uint8_t *>(&channelDescFinal) + sizeof(CommAbiHeader), copySize,
        reinterpret_cast<const uint8_t *>(&channelDesc) + sizeof(CommAbiHeader), copySize));
 
    if (channelDesc.header.version >= HCCL_CHANNEL_VERSION_ONE) {
        channelDescFinal.remoteRank = channelDesc.remoteRank;
        channelDescFinal.channelProtocol   = channelDesc.channelProtocol;
        channelDescFinal.localEndpoint  = channelDesc.localEndpoint;
        channelDescFinal.remoteEndpoint  = channelDesc.remoteEndpoint;
        channelDescFinal.notifyNum  = channelDesc.notifyNum;
        channelDescFinal.memHandles  = channelDesc.memHandles;
        channelDescFinal.memHandleNum  = channelDesc.memHandleNum;
 
        // 根据协议类型拷贝union中的相应成员
        switch (channelDesc.channelProtocol) {
            case COMM_PROTOCOL_HCCS:
            case COMM_PROTOCOL_PCIE:
            case COMM_PROTOCOL_SIO:
            case COMM_PROTOCOL_UBC_CTP:
            case COMM_PROTOCOL_UB_MEM:
                break;
            case COMM_PROTOCOL_ROCE:
                channelDescFinal.roceAttr.queueNum = (channelDesc.roceAttr.queueNum == INVALID_UINT) ? GetExternalInputQpsPerConnection() : channelDesc.roceAttr.queueNum;
                channelDescFinal.roceAttr.retryCnt = (channelDesc.roceAttr.retryCnt == INVALID_UINT) ? EnvConfig::GetExternalInputRdmaRetryCnt() : channelDesc.roceAttr.retryCnt;
                channelDescFinal.roceAttr.retryInterval = (channelDesc.roceAttr.retryInterval == INVALID_UINT) ? EnvConfig::GetExternalInputRdmaTimeOut() : channelDesc.roceAttr.retryInterval;
                channelDescFinal.roceAttr.tc = (channelDesc.roceAttr.tc == 0xFF) ? EnvConfig::GetExternalInputRdmaTrafficClass() : channelDesc.roceAttr.tc;
                channelDescFinal.roceAttr.sl = (channelDesc.roceAttr.sl == 0xFF) ? EnvConfig::GetExternalInputRdmaServerLevel() : channelDesc.roceAttr.sl;
                HCCL_INFO("[%s]queueNum[%u], retryCnt[%u], retryInterval[%u], tc[%u], sl[%u]", __func__,
                    channelDescFinal.roceAttr.queueNum, channelDescFinal.roceAttr.retryCnt, channelDescFinal.roceAttr.retryInterval,
                    channelDescFinal.roceAttr.tc, channelDescFinal.roceAttr.sl);
                break;
            default:
                HCCL_ERROR("[%s]Unsupported protocol[%d] found in HcclChannelDesc.", __func__, channelDesc.channelProtocol);
                return HCCL_E_PARA;
        }
    }
 
    if (channelDesc.header.version > HCCL_CHANNEL_VERSION) {
        // 传入的版本高于当前版本，警告不支持的配置项将被忽略
        HCCL_WARNING("The version of provided [%u] is higher than the current version[%u], "
            "unsupported configuration will be ignored.",
            channelDesc.header.version, HCCL_CHANNEL_VERSION);
    } else if (channelDesc.header.version < HCCL_CHANNEL_VERSION) {
        // 传入的版本低于当前版本，警告高版本支持的配置项将被忽略
        HCCL_WARNING("The version of provided [%u] is lower than the current version[%u], "
            "configurations supported by later versions will be ignored.",
            channelDesc.header.version, HCCL_CHANNEL_VERSION);
    }
 
    // 如果扩展到version=2后
    // 1) 在底层为新的结构体和版本（version为2）上，会正常执行下面的判断处理逻辑；
    // 2) 在底层为旧的结构体和版本（version为1）上，下面的逻辑没有，version的2 > 1的部分会被忽略掉；
    if (channelDesc.header.version >= 2) {
    }
 
    return HCCL_SUCCESS;
}

bool CheckCommEngine(const CommEngine engine, const uint32_t opExpansionMode)
{
    constexpr uint32_t DEFAULT_MODE = 0;
    constexpr uint32_t CCU_MS_MODE = 5;
    constexpr uint32_t CCU_SCHE_MODE = 6;
    if (engine == CommEngine::COMM_ENGINE_CCU) {
        return opExpansionMode == DEFAULT_MODE
            || opExpansionMode == CCU_MS_MODE
            || opExpansionMode == CCU_SCHE_MODE;
    }

    return true;
}

constexpr uint32_t CHANNEL_NUM_MAX = 1024 * 1024;  // channel的默认限制最大为1024 * 1024

HcclResult HcclChannelAcquire(HcclComm comm, CommEngine engine, 
    const HcclChannelDesc* channelDescs, uint32_t channelNum, ChannelHandle* channels)
{
    HCCL_RUN_INFO("Entry-%s", __func__);
    HcclUs startut = TIME_NOW();
    EXCEPTION_HANDLE_BEGIN
    HCCL_INFO("[%s] ChannelAcquire begin, channelNum[%u], engine[%d]", __func__, channelNum, engine);

    // 入参校验
    CHK_PTR_NULL(comm);
    CHK_PTR_NULL(channelDescs);
    CHK_PTR_NULL(channels);
    CHK_PRT_RET(
        (channelNum == 0 || channelNum > CHANNEL_NUM_MAX), 
        HCCL_ERROR("[%s]Invalid channelNum, channelNum[%u], max channel num[%u]",
        __func__, channelNum, CHANNEL_NUM_MAX), HCCL_E_PARA
    );
 
    HcclResult ret = HCCL_SUCCESS;
    hccl::hcclComm *hcclComm = static_cast<hccl::hcclComm *>(comm);
    std::vector<HcclChannelDesc> channelDescFinals;
    for (uint32_t idx = 0; idx < channelNum; idx++) {
        HcclChannelDesc channelDescFinal;
        HcclChannelDescInit(&channelDescFinal, 1);
        ret = ProcessHcclResPackReq(channelDescs[idx], channelDescFinal);
        if (ret != HCCL_SUCCESS) {
            HCCL_ERROR("[%s] Failed check channelDesc, channelDesc idx[%u], group[%s], engine[%d], "
                "channelNum[%llu], ret[%d]", __func__, idx, hcclComm->GetIdentifier().c_str(),
                engine, channelNum, ret);
            return ret;
        }
        channelDescFinals.push_back(channelDescFinal);
    }
 
    if (hcclComm->IsCommunicatorV2()) {  // A5
        hccl::CollComm* collComm = hcclComm->GetCollComm();
        CHK_PTR_NULL(collComm);
        const std::string &commTag = hcclComm->GetIdentifier();
        hccl::MyRank* myRank = collComm->GetMyRank();
        CHK_PTR_NULL(myRank);
 
        const uint32_t opExpansionMode = myRank->GetOpExpansionMode();
        if (!CheckCommEngine(engine, opExpansionMode)) {
            HCCL_ERROR("[%s] failed, coll comm[%p] is not enable ccu feature[%d], "
                "but commEngine is [%d].", __func__, hcclComm, opExpansionMode, engine);
            return HcclResult::HCCL_E_PARA;
        }

        CHK_RET(myRank->CreateChannels(engine, commTag, channelDescFinals.data(), channelNum, channels));
    } else {
        auto& channelMgr = hcclComm->GetIndependentOp().GetChannelManager();
        ret = channelMgr.ChannelCommCreate(hcclComm->GetIdentifier(), engine,
            channelDescFinals.data(), channelNum, channels);
    }
 
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("[%s] Failed to acquire channel, group[%s], engine[%d], channelNum[%llu], ret[%d]",
           __func__, hcclComm->GetIdentifier().c_str(), engine, channelNum, ret);
        return ret;
    }
 
    HCCL_RUN_INFO("[%s] acquire channel success, group[%s], engine[%d], channelNum[%llu], ret[%d]", 
        __func__, hcclComm->GetIdentifier().c_str(), engine, channelNum, ret);
    HCCL_INFO("[%s] success, take time [%lld]us.",
        __func__, DURATION_US(TIME_NOW() - startut));
    EXCEPTION_HANDLE_END
    return HCCL_SUCCESS;
}

HcclResult HcclCcuKernelRegister(HcclComm comm,
    CcuKernelHandle *kernelHandle, void *kernelCreator, void *kernelArg)
{
    HCCL_RUN_INFO("Entry-%s", __func__);
    HcclUs startut = TIME_NOW();

    CHK_PTR_NULL(comm);
    CHK_PTR_NULL(kernelHandle);
    CHK_PTR_NULL(kernelCreator);
    CHK_PTR_NULL(kernelArg);

    auto *hcclComm = static_cast<hccl::hcclComm *>(comm);
    auto *collComm = hcclComm->GetCollComm();
    CHK_PTR_NULL(collComm);
    auto* myRank = collComm->GetMyRank();
    CHK_PTR_NULL(myRank);

    auto *ccuContainer = myRank->GetCcuResContainer();
    CHK_PTR_NULL(ccuContainer);

    auto *resPack = ccuContainer->GetResPack();
    CHK_PTR_NULL(resPack);

    hcomm::KernelCreator creator = *static_cast<hcomm::KernelCreator*>(kernelCreator);
    const auto& arg = *static_cast<const hcomm::CcuKernelArg*>(kernelArg);
    std::unique_ptr<hcomm::CcuKernel> kernel = creator(arg);

    const uint32_t devLogicId = HcclGetThreadDeviceId();
    auto &kernelMgr = hcomm::CcuKernelMgr::GetInstance(devLogicId);
    CcuKernelHandle newHandle{0};
    // 当前翻译内部流程可能抛异常
    EXCEPTION_HANDLE_BEGIN
    CHK_RET(kernelMgr.Register(std::move(kernel), *resPack, newHandle));
    EXCEPTION_HANDLE_END
    CHK_RET(ccuContainer->SaveCcuKernel(newHandle));
    *kernelHandle = newHandle;
    HCCL_INFO("[%s] success, take time [%lld]us.",
        __func__, DURATION_US(TIME_NOW() - startut));
    return HcclResult::HCCL_SUCCESS;
}

HcclResult HcclCcuKernelRegisterFinish(HcclComm comm)
{
    HCCL_RUN_INFO("Entry-%s", __func__);
    CHK_PTR_NULL(comm);

    auto *hcclComm = static_cast<hccl::hcclComm *>(comm);
    auto *collComm = hcclComm->GetCollComm();
    CHK_PTR_NULL(collComm);
    auto* myRank = collComm->GetMyRank();
    CHK_PTR_NULL(myRank);

    auto *ccuContainer = myRank->GetCcuResContainer();
    CHK_PTR_NULL(ccuContainer);
    CHK_RET(ccuContainer->ResetResPack());
    return HcclResult::HCCL_SUCCESS;
}

static HcclResult LaunchCcuTasks(const std::vector<hcomm::CcuTaskParam> &params, const aclrtStream stream)
{
    constexpr uint32_t defaultTimeOutSec = 120; // 当前未支持从环境变量配置
    for (auto it = params.begin(); it != params.end(); ++it) {
        rtCcuTaskInfo_t taskInfo{};
        taskInfo.dieId       = it->dieId;
        taskInfo.missionId   = it->missionId;
        taskInfo.instStartId = it->instStartId;
        taskInfo.instCnt     = it->instCnt;
        taskInfo.key         = it->key;
        taskInfo.argSize     = it->argSize;
        taskInfo.timeout     = defaultTimeOutSec;
        std::copy(std::begin(it->args), std::end(it->args), std::begin(taskInfo.args));
        
        HCCL_INFO("[%s] start ccu task, dieId[%u] missionId[%u] instStartId[%u] instCnt[%u], "
            "argSize[%u], timeout[%u]s", __func__, taskInfo.dieId, taskInfo.missionId,
            taskInfo.instStartId, taskInfo.instCnt, taskInfo.argSize, taskInfo.timeout);
 
        for (std::size_t i = 0; i < taskInfo.argSize; i++) { // args 大小为 13
            constexpr std::size_t TOKEN_VALUE_INDEX = 2; // 与算法约束token index为 2
            if (i == TOKEN_VALUE_INDEX) { continue; }
            HCCL_INFO("[%s] arg[%lu] = %lu", __func__, i, taskInfo.args[i]);
        }

        auto ret = rtCCULaunch(&taskInfo, stream);
        if (ret != RT_ERROR_NONE) {
            HCCL_ERROR("[%s] failed to launch ccu, ret[%d]", __func__, ret);
            return HcclResult::HCCL_E_RUNTIME;
        }
    }

    return HcclResult::HCCL_SUCCESS;
}

HcclResult HcclCcuKernelLaunch(HcclComm comm, const ThreadHandle threadHandle,
    const CcuKernelHandle KernelHandle, void *taskArgs)
{
    HCCL_RUN_INFO("Entry-%s", __func__);
    HcclUs startut = TIME_NOW();
    (void)comm;
    CHK_PTR_NULL(taskArgs);

    CHK_PRT_RET(threadHandle == 0,
        HCCL_ERROR("[%s] failed, thread handle is empty.", __func__),
        HcclResult::HCCL_E_PARA);

    const Thread *rtsThread = reinterpret_cast<Thread *>(threadHandle);
    const auto *threadStream = rtsThread->GetStream();
    CHK_PTR_NULL(threadStream);
    auto *streamPtr = threadStream->ptr();
    CHK_PTR_NULL(streamPtr);

    const uint32_t devLogicId = HcclGetThreadDeviceId();
    auto &kernelMgr = hcomm::CcuKernelMgr::GetInstance(devLogicId);
    auto *kernel = kernelMgr.GetKernel(KernelHandle);
    CHK_PTR_NULL(kernel);

    EXCEPTION_HANDLE_BEGIN
    const hcomm::CcuTaskArg *ccuTaskArgs =
        reinterpret_cast<hcomm::CcuTaskArg *>(taskArgs);
    std::vector<hcomm::CcuTaskParam> ccuParams{};
    CHK_RET(kernel->GeneTaskParam(*ccuTaskArgs, ccuParams));
    if (ccuParams.empty()) {
        HCCL_INFO("[%s] passed, ccu params are empty.", __func__);
        return HcclResult::HCCL_SUCCESS;
    }
    CHK_RET(LaunchCcuTasks(ccuParams, streamPtr));
    EXCEPTION_HANDLE_END
    HCCL_INFO("[%s] success, take time [%lld]us.",
        __func__, DURATION_US(TIME_NOW() - startut));
    return HcclResult::HCCL_SUCCESS;
}
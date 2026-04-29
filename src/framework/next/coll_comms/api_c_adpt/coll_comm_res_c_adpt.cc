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
#include "config/env_config.h"
#include "env_config/env_config.h"
#include "../common/loggers/channel_logger.h"  // 日志记录器

#include "hcom_common.h"
#include "ccu_kernel.h"
#include "../comms/ccu/ccu_kernel/ccu_kernel_mgr.h"
#include "rt_external.h"

#include "ccu_types.h"
#include "ccu_log.h"

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
HcclResult ProcessRoceChannelDesc(const HcclChannelDesc &channelDesc, HcclChannelDesc &channelDescFinal, hccl::hcclComm *hcclComm)
{
    hccl::CollComm* collComm = hcclComm->GetCollComm();
    CHK_PTR_NULL(collComm);
    hccl::CommConfig commConfig = collComm->GetCommConfig();
    channelDescFinal.roceAttr.queueNum = (channelDesc.roceAttr.queueNum == INVALID_UINT) ? GetExternalInputQpsPerConnection() : channelDesc.roceAttr.queueNum;
    channelDescFinal.roceAttr.retryCnt = (channelDesc.roceAttr.retryCnt == INVALID_UINT) ? EnvConfig::GetExternalInputRdmaRetryCnt() : channelDesc.roceAttr.retryCnt;
    channelDescFinal.roceAttr.retryInterval = (channelDesc.roceAttr.retryInterval == INVALID_UINT) ? EnvConfig::GetExternalInputRdmaTimeOut() : channelDesc.roceAttr.retryInterval;
    channelDescFinal.roceAttr.tc = (commConfig.GetConfigTrafficClass() == INVALID_UINT) ? EnvConfig::GetExternalInputRdmaTrafficClass() : commConfig.GetConfigTrafficClass();
    channelDescFinal.roceAttr.sl = (commConfig.GetConfigServiceLevel() == INVALID_UINT) ? EnvConfig::GetExternalInputRdmaServerLevel() : commConfig.GetConfigServiceLevel();
    HCCL_INFO("[%s]queueNum[%u], retryCnt[%u], retryInterval[%u], tc[%u], sl[%u]", __func__,
        channelDescFinal.roceAttr.queueNum, channelDescFinal.roceAttr.retryCnt, channelDescFinal.roceAttr.retryInterval,
        channelDescFinal.roceAttr.tc, channelDescFinal.roceAttr.sl);
    return HCCL_SUCCESS;
}

HcclResult ProcessHcclChannelDesc(const HcclChannelDesc &channelDesc, HcclChannelDesc &channelDescFinal, hccl::hcclComm *hcclComm)
{
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
            return ProcessRoceChannelDesc(channelDesc, channelDescFinal, hcclComm);
        default: {
            auto ProtocolToString = [](const CommProtocol proto) -> const char* {
                switch (proto) {
                    case COMM_PROTOCOL_HCCS:    return "COMM_PROTOCOL_HCCS";
                    case COMM_PROTOCOL_PCIE:    return "COMM_PROTOCOL_PCIE";
                    case COMM_PROTOCOL_SIO:     return "COMM_PROTOCOL_SIO";
                    case COMM_PROTOCOL_UBC_CTP: return "COMM_PROTOCOL_UBC_CTP";
                    case COMM_PROTOCOL_UB_MEM:  return "COMM_PROTOCOL_UB_MEM";
                    case COMM_PROTOCOL_ROCE:    return "COMM_PROTOCOL_ROCE";
                    default:                    return "UNKNOWN_PROTOCOL";
                }
            };
            HCCL_ERROR("[%s] Unsupported protocol[%s] found in HcclChannelDesc.",
                       __func__, ProtocolToString(channelDesc.channelProtocol));
            return HCCL_E_PARA;
        }
    }
    return HCCL_SUCCESS;
}

HcclResult ProcessHcclResPackReq(const HcclChannelDesc &channelDesc, HcclChannelDesc &channelDescFinal, hccl::hcclComm *hcclComm)
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
        ProcessHcclChannelDesc(channelDesc, channelDescFinal, hcclComm);
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
    u64 beginTime =  Hccl::DlProfFunction::GetInstance().dlMsprofSysCycleTime();
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
        ret = ProcessHcclResPackReq(channelDescs[idx], channelDescFinal, hcclComm);
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
        
        CHK_RET_UNAVAIL(myRank->CreateChannels(engine, commTag, channelDescFinals.data(), channelNum, channels));
        if (engine == COMM_ENGINE_AICPU || engine == COMM_ENGINE_AICPU_TS) {
            HCCL_INFO("[HcclChannelAcquire] ReportChannelAicpuKernel start");
            HcclCommDfx* hcclCommDfx = collComm->GetHcclCommDfx();
            CHK_PTR_NULL(hcclCommDfx);
            std::string kernelName = "RunAicpuIndOpChannelInitV2";
            CHK_RET(hcclCommDfx->ReportKernel(beginTime, commTag, kernelName, SalGetTid()));
            HCCL_INFO("[HcclChannelAcquire] ReportChannelAicpuKernel success");
        }
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
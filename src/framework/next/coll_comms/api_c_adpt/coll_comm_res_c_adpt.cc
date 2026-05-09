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

constexpr uint32_t HCCL_CHANNEL_VERSION_ONE = 1;
constexpr uint32_t MULTIPLE = 4;               // 用于A5判断TC是否为4的倍数
constexpr uint32_t TC_MAX = 255;               // TC的最大值（不区分芯片类型）
constexpr uint32_t RETRY_INTERVAL_MIN = 5u;    // retryInterval范围的最小值（不区分芯片类型）
constexpr uint32_t A5_RETRY_INTERVAL_MAX = 24u;// A5的retryInterval范围的最大值
constexpr uint32_t RETRY_CNT_MIN = 1u;         // retryCnt范围的最小值（不区分芯片类型）
constexpr uint32_t RETRY_CNT_MAX = 7u;         // retryCnt范围的最大值（不区分芯片类型）
constexpr uint32_t SL_MAX = 7u;                // sl范围的最大值，sl即serviceLevel（不区分芯片类型）
constexpr uint32_t TC_DEFAULT = 0xFFFFFFFFu;   // TC的默认值（不区分芯片类型）
constexpr uint32_t SL_DEFAULT = 0xFFFFFFFFu;   // SL的默认值（不区分芯片类型）

static void FillChannelDescFinal(hccl::CommConfig commConfig, const HcclChannelDesc &channelDesc, HcclChannelDesc &channelDescFinal, bool isCommunicatorV2)
{
    channelDescFinal.roceAttr.queueNum = (channelDesc.roceAttr.queueNum == INVALID_UINT) ? GetExternalInputQpsPerConnection() : channelDesc.roceAttr.queueNum;
    if (isCommunicatorV2) { // A5
        auto& rdmaConfig = Hccl::EnvConfig::GetInstance().GetRdmaConfig();
        channelDescFinal.roceAttr.retryCnt = (channelDesc.roceAttr.retryCnt == INVALID_UINT) ? rdmaConfig.GetRdmaRetryCnt() : channelDesc.roceAttr.retryCnt;
        channelDescFinal.roceAttr.retryInterval = (channelDesc.roceAttr.retryInterval == INVALID_UINT) ? rdmaConfig.GetRdmaTimeOut() : channelDesc.roceAttr.retryInterval;
        channelDescFinal.roceAttr.tc = (commConfig.GetConfigTrafficClass() == INVALID_UINT) ? rdmaConfig.GetRdmaTrafficClass() : commConfig.GetConfigTrafficClass();
        channelDescFinal.roceAttr.sl = (commConfig.GetConfigServiceLevel() == INVALID_UINT) ? rdmaConfig.GetRdmaServerLevel() : commConfig.GetConfigServiceLevel();
    } else {
        channelDescFinal.roceAttr.retryCnt = (channelDesc.roceAttr.retryCnt == INVALID_UINT) ? EnvConfig::GetExternalInputRdmaRetryCnt() : channelDesc.roceAttr.retryCnt;
        channelDescFinal.roceAttr.retryInterval = (channelDesc.roceAttr.retryInterval == INVALID_UINT) ? EnvConfig::GetExternalInputRdmaTimeOut() : channelDesc.roceAttr.retryInterval;
        channelDescFinal.roceAttr.tc = (channelDesc.roceAttr.tc == 0xFF) ? EnvConfig::GetExternalInputRdmaTrafficClass() : channelDesc.roceAttr.tc;
        channelDescFinal.roceAttr.sl = (channelDesc.roceAttr.sl == 0xFF) ? EnvConfig::GetExternalInputRdmaServerLevel() : channelDesc.roceAttr.sl;
    }
}

static HcclResult CheckA5Config(hccl::CommConfig commConfig, const HcclChannelDesc &channelDesc)
{
    u32 tc = commConfig.GetConfigTrafficClass();
    CHK_PRT_RET((tc != TC_DEFAULT) && (tc > TC_MAX || (tc % MULTIPLE != 0)),
        HCCL_ERROR("[ProcessRoceChannelDesc]errNo[0x%016llx] invalid hcclRdmaTrafficClass[%u], must be 0xFFFFFFFF or in [0,255] and a multiple of 4",
            HCCL_ERROR_CODE(HCCL_E_PARA), tc),
        HCCL_E_PARA);

    u32 sl = commConfig.GetConfigServiceLevel();
    CHK_PRT_RET((sl != SL_DEFAULT) && (sl > SL_MAX),
        HCCL_ERROR("[ProcessRoceChannelDesc]errNo[0x%016llx] invalid hcclRdmaServiceLevel[%u], must be 0xFFFFFFFF or in [0,7]",
            HCCL_ERROR_CODE(HCCL_E_PARA), sl),
        HCCL_E_PARA);

    u32 retryInterval = channelDesc.roceAttr.retryInterval;
    CHK_PRT_RET((retryInterval != INVALID_UINT) && (retryInterval < RETRY_INTERVAL_MIN || retryInterval > A5_RETRY_INTERVAL_MAX),
        HCCL_ERROR("[ProcessRoceChannelDesc]errNo[0x%016llx] invalid hcclRdmaRetryInterval[%u], must be 0xFFFFFFFF or in [5,24]",
        HCCL_ERROR_CODE(HCCL_E_PARA), retryInterval),
        HCCL_E_PARA);

    u32 retryCnt = channelDesc.roceAttr.retryCnt;
    CHK_PRT_RET((retryCnt != INVALID_UINT) && (retryCnt < RETRY_CNT_MIN || retryCnt > RETRY_CNT_MAX),
        HCCL_ERROR("[ProcessRoceChannelDesc]errNo[0x%016llx] invalid hcclRdmaRetryCnt[%u], must be 0xFFFFFFFF or in [1,7]",
        HCCL_ERROR_CODE(HCCL_E_PARA), retryCnt),
        HCCL_E_PARA);
    return HCCL_SUCCESS;
}

HcclResult ProcessRoceChannelDesc(const HcclChannelDesc &channelDesc, HcclChannelDesc &channelDescFinal, hccl::hcclComm *hcclComm)
{
    bool isCommunicatorV2 = hcclComm->IsCommunicatorV2();
    hccl::CommConfig commConfig{}; // A5使用
    if (isCommunicatorV2) { // A5
        hccl::CollComm* collComm = hcclComm->GetCollComm();
        CHK_PTR_NULL(collComm);
        commConfig = collComm->GetCommConfig();
        CHK_RET(CheckA5Config(commConfig, channelDesc));
    }
    FillChannelDescFinal(commConfig, channelDesc, channelDescFinal, isCommunicatorV2);
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
        case COMM_PROTOCOL_UBC_TP:
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
                    case COMM_PROTOCOL_UBC_TP:  return "COMM_PROTOCOL_UBC_TP";
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
        CHK_RET(ProcessHcclChannelDesc(channelDesc, channelDescFinal, hcclComm));
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
            HCCL_ERROR("[%s] failed, coll comm[%p] opExpansionMode[%d] is not supported by CCU engine[%d].", 
                __func__, hcclComm, opExpansionMode, engine);
            return HcclResult::HCCL_E_PARA;
        }
        
        CHK_RET_UNAVAIL(myRank->CreateChannels(engine, commTag, channelDescFinals.data(), channelNum, channels));
        if (engine == COMM_ENGINE_CPU) {
            HcclCommDfx* hcclCommDfx = collComm->GetHcclCommDfx();
            CHK_PTR_NULL(hcclCommDfx);
            auto callback = hcclCommDfx->GetDpuCallback();
            for (uint32_t idx = 0; idx < channelNum; idx++) {
                int32_t ret = HcommDpuChannelRegisterDfx(channels[idx], callback);
                CHK_PRT_RET(ret != HCCL_SUCCESS,
                    HCCL_ERROR("[HcclChannelAcquire] Failed to register DFX callback for channel[%u], ret[%d]", idx, ret),
                    static_cast<HcclResult>(ret));
            }
            HCCL_INFO("[HcclChannelAcquire] channelNum[%u] Register DFX callback for CPU channels success", channelNum);
        }
        if (engine == COMM_ENGINE_AICPU || engine == COMM_ENGINE_AICPU_TS) {
            HCCL_INFO("[HcclChannelAcquire] ReportChannelAicpuKernel start");
            HcclCommDfx* hcclCommDfx = collComm->GetHcclCommDfx();
            CHK_PTR_NULL(hcclCommDfx);
            std::string kernelName = "RunAicpuIndOpChannelInitV2";
            CHK_RET(hcclCommDfx->ReportKernel(beginTime, commTag, kernelName, SalGetTid()));
            HCCL_INFO("[HcclChannelAcquire] ReportChannelAicpuKernel success");
        }
    } else {
        hccl::MyRank *myRank = (hccl::MyRank *)hcclComm->GetMyRank();
        if (hcclComm->GetConnectMode() && engine == COMM_ENGINE_CPU && myRank != nullptr) {
            const std::string &commTag = hcclComm->GetIdentifier();
            ret = myRank->CreateChannels(engine, commTag, channelDescFinals.data(), channelNum, channels);
        } else {
            auto& channelMgr = hcclComm->GetIndependentOp().GetChannelManager();
            ret = channelMgr.ChannelCommCreate(hcclComm->GetIdentifier(), engine,
                channelDescFinals.data(), channelNum, channels);
        }
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
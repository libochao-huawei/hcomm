/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "ccu_conn.h"

#include <random>
#include <sstream>

#include "hcom_common.h"
#include "exception_handler.h"
#include "eid_info_mgr.h"
#include "adapter_rts.h"

#include "hccp_ctx.h"

#include "buffer.h"
#include "local_ub_rma_buffer.h"
#include "rdma_handle_manager.h"
#include "orion_adapter_rts.h"
#include "orion_adapter_hccp.h"
#include "env_config.h"

namespace hcomm {

CcuConnection::CcuConnection(const CommAddr &locAddr, const CommAddr &rmtAddr,
    const CcuChannelInfo &channelInfo, const std::vector<CcuJetty *> &ccuJettys)
    : locAddr_(locAddr), rmtAddr_(rmtAddr), channelInfo_(channelInfo), ccuJettys_(ccuJettys)
{
}

CcuRtpConnection::CcuRtpConnection(const CommAddr &locAddr, const CommAddr &rmtAddr,
    const CcuChannelInfo &channelInfo, const std::vector<CcuJetty *> &ccuJettys)
    : CcuConnection(locAddr, rmtAddr, channelInfo, ccuJettys)
{
    tpProtocol_ = TpProtocol::RTP;
}

CcuCtpConnection::CcuCtpConnection(const CommAddr &locAddr, const CommAddr &rmtAddr,
    const CcuChannelInfo &channelInfo, const std::vector<CcuJetty *> &ccuJettys)
    : CcuConnection(locAddr, rmtAddr, channelInfo, ccuJettys)
{
    tpProtocol_ = TpProtocol::CTP;
}

HcclResult CcuConnection::Init()
{
    devLogicId_ = HcclGetThreadDeviceId();
    CHK_RET(hrtGetDevicePhyIdByIndex(static_cast<uint32_t>(devLogicId_), devPhyId_));

    EXCEPTION_HANDLE_BEGIN
    auto &rdmaHandleMgr = Hccl::RdmaHandleManager::GetInstance();
    Hccl::IpAddress ipAddr{};
    CHK_RET(CommAddrToIpAddress(locAddr_, ipAddr));
    ctxHandle_ = rdmaHandleMgr.GetByIp(devPhyId_, ipAddr);

    DevEidInfo eidInfo{};
    CHK_RET(EidInfoMgr::GetInstance(devPhyId_).GetEidInfoByAddr(locAddr_, eidInfo));
    dieId_ = static_cast<uint8_t>(eidInfo.dieId);
    funcId_ = eidInfo.funcId;

    EXCEPTION_HANDLE_END

    CHK_RET(GetLocalCcuRmaBufferInfo());

    jettyNum_ = channelInfo_.jettyInfos.size();
    CHK_PRT_RET(jettyNum_ == 0,
        HCCL_ERROR("[CcuConnection][%s] failed, jetty num[0] is unexpected.", __func__),
        HcclResult::HCCL_E_PARA);

    GenerateLocalPsn();
    status_ = CcuConnStatus::INIT;
    innerStatus_ = InnerStatus::INIT;
    return HcclResult::HCCL_SUCCESS;
}

CcuConnStatus CcuConnection::GetStatus()
{
    if (status_ == CcuConnStatus::CONNECTED
        || status_ == CcuConnStatus::CONN_INVALID) {
        return status_;
    }

    if (StatusMachine() != HcclResult::HCCL_SUCCESS) {
        status_ = CcuConnStatus::CONN_INVALID;
        innerStatus_ = InnerStatus::CONN_INVALID;
    }

    return status_;
}

HcclResult CcuConnection::GetLocalCcuRmaBufferInfo()
{
    uint64_t ccuBufSize = 0; // 暂未使用
    CHK_RET(CcuDevMgrImp::GetCcuResourceSpaceBufInfo(
        devLogicId_, dieId_, ccuBufAddr_, ccuBufSize));

    uint64_t tokenId = 0;
    uint64_t tokenValue = 0;
    CHK_RET(CcuDevMgrImp::GetCcuResourceSpaceTokenInfo(
        devLogicId_, dieId_, tokenId, tokenValue));
    ccuBufTokenId_ = static_cast<uint32_t>(tokenId);
    ccuBufTokenValue_ = static_cast<uint32_t>(tokenValue);
    return HcclResult::HCCL_SUCCESS;
}

HcclResult CcuConnection::StatusMachine()
{
    if (status_ == CcuConnStatus::INIT) {
        CHK_RET(UpdateInitStatus());
        return HcclResult::HCCL_SUCCESS;
    }

    if (innerStatus_ == InnerStatus::JETTY_IMPORTING) {
        CHK_RET(UpdateExchangeStatus());
        return HcclResult::HCCL_SUCCESS;
    }

    return HcclResult::HCCL_SUCCESS;
}

// ==================== TA 档位映射表 ====================
// TA 芯片挡位 = hw_value / 8
// 挡位 0 (0-7):   512ms
// 挡位 1 (8-15):  1000ms (1s)
// 挡位 2 (16-23): 8000ms (8s)
// 挡位 3 (24-31): 32000ms (32s)
// 各挡位对应的最小硬件配置值
constexpr uint8_t TA_HW_GEAR0_BASE = 0;
constexpr uint8_t TA_HW_GEAR1_BASE = 8;
constexpr uint8_t TA_HW_GEAR2_BASE = 16;
constexpr uint8_t TA_HW_GEAR3_BASE = 24;
static constexpr uint32_t TA_TIMEOUT_MS_GEAR0 = 512;
static constexpr uint32_t TA_TIMEOUT_MS_GEAR1 = 1000;
static constexpr uint32_t TA_TIMEOUT_MS_GEAR2 = 8000;
static constexpr uint32_t TA_TIMEOUT_MS_GEAR3 = 32000;
 
// AT 档位 -> 单轮超时时间(ms) 的映射表
// at=0 -> 16ms
// at=1 -> 128ms
// at=2 -> 1000ms (1s)
// at=3 -> 4000ms (4s)
static constexpr uint32_t AT_TIMEOUT_MAP[4] = {16, 128, 1000, 4000};
 
// 合法的 AT 档位范围
static constexpr uint8_t AT_GEAR_MIN = 0;
static constexpr uint8_t AT_GEAR_MAX = 3;
 
// 默认 AT 档位（异常降级用）
static constexpr uint8_t AT_GEAR_DEFAULT = 2; // 默认 1s
 
HcclResult GetTpAttrAsync(RdmaHandle handle, uint64_t tpHandle, uint32_t& attrBitmap, TpAttr& attr, RequestHandle& reqHandle)
{
    HCCL_INFO("[GetTpAttrAsync] begain, reqHandle[%llu]", reqHandle);

    void *raReqHandle = nullptr;
    ret = RaGetTpAttrAsync(handle, tpHandle, &attrBitmap, &attr, &raReqHandle);
    if (ret != 0) {
        string msg = StringFormat("call RaGetTpAttrAsync failed, error code =%d.", ret);
        THROW<NetworkApiException>(msg);
    }

    CHK_RET(WaitRequestResult(raReqHandle, reqHandle));
    HCCL_INFO("[GetTpAttrAsync] success, reqHandle[%llu]", reqHandle);
    return HCCL_SUCCESS;
}

 
HcclResult CalcTotalTimeout(CtxHandle ctxHandle, TpHandle tpHandle, uint32_t &outTotalTimeoutMs)
{
    uint32_t attrBitmap = 0;
    struct TpAttr tpAttr = {0};
    RequestHandle reqHandle{0};
    CHK_RET(GetTpAttrAsync(ctxHandle, tpHandle, attrBitmap, tpAttr, reqHandle));

    uint8_t rawAtGear = tpAttr.at;
    uint8_t rawRetryTimes = tpAttr.retryTimesInit;

    uint8_t finalAtGear = rawAtGear;
    if (rawAtGear < AT_GEAR_MIN || rawAtGear > AT_GEAR_MAX) {
        finalAtGear = AT_GEAR_DEFAULT;
        HCCL_WARNING("[TpTimeoutCalculator] Invalid at gear[%u], expect [%u, %u], use default gear[%u].", rawAtGear,
            AT_GEAR_MIN, AT_GEAR_MAX, finalAtGear);
    }

    uint32_t singleAtTimeoutMs = AT_TIMEOUT_MAP[finalAtGear];
    uint32_t totalTimeoutMs = singleAtTimeoutMs * static_cast<uint32_t>(rawRetryTimes);
    outTotalTimeoutMs = totalTimeoutMs;

    HCCL_INFO("[TpTimeoutCalculator] TP timeout calc success: "
              "raw_at_gear[%u], final_at_gear[%u], single_timeout[%ums], "
              "retry_times_init[%u], total_timeout[%ums].",
        rawAtGear, finalAtGear, singleAtTimeoutMs, rawRetryTimes, totalTimeoutMs);

    return HCCL_SUCCESS;
}
 
uint32_t TaHwValueToMs(uint8_t hwValue)
{
    uint8_t gear = hwValue / 8; // 芯片挡位 = 配置值 / 8
    switch (gear) {
        case 0:
            return TA_TIMEOUT_MS_GEAR0; // 512ms
        case 1:
            return TA_TIMEOUT_MS_GEAR1; // 1s
        case 2:
            return TA_TIMEOUT_MS_GEAR2; // 8s
        case 3:
            return TA_TIMEOUT_MS_GEAR3; // 32s
        default:
            return TA_TIMEOUT_MS_GEAR2; // 异常兜底 8s
    }
}
 
uint8_t FindMinTaHwValueGreaterThan(uint32_t tpTotalTimeoutMs)
{
    uint8_t finalHwValue = 0;
 
    // 按挡位从小到大依次判断，找到第一个大于目标时间的挡位
    if (tpTotalTimeoutMs < TA_TIMEOUT_MS_GEAR0) {
        finalHwValue = TA_HW_GEAR0_BASE;
    } else if (tpTotalTimeoutMs < TA_TIMEOUT_MS_GEAR1) {
        finalHwValue = TA_HW_GEAR1_BASE;
    } else if (tpTotalTimeoutMs < TA_TIMEOUT_MS_GEAR2) {
        finalHwValue = TA_HW_GEAR2_BASE;
    } else {
        // 超过8s或等于8s，直接使用最大挡位32s
        finalHwValue = TA_HW_GEAR3_BASE;
    }
 
    // 打印调试日志，方便线上定位
    HCCL_DEBUG("[FindMinTaHwValue] tp_total_timeout[%ums], selected_gear_hw_value[%u], corresponding_timeout[%ums]",
        tpTotalTimeoutMs, finalHwValue,
        (finalHwValue == TA_HW_GEAR0_BASE)   ? TA_TIMEOUT_MS_GEAR0
        : (finalHwValue == TA_HW_GEAR1_BASE) ? TA_TIMEOUT_MS_GEAR1
        : (finalHwValue == TA_HW_GEAR2_BASE) ? TA_TIMEOUT_MS_GEAR2
                                             : TA_TIMEOUT_MS_GEAR3);
 
    return finalHwValue;
}
 
HcclResult CcuConnection::GetTaTimeOut()
{
    u8 envValue = EnvConfig::GetExternalInputUBTimeOut();
    
    if (tpProtocol_ == TpProtocol::CTP) {
        errTimeout_ = envValue;
        return HCCL_SUCCESS;
    }

    TpHandle tpHandle = tpInfo_.tpHandle;
    uint32_t envTimeoutMs = TaHwValueToMs(envValue);
    uint32_t tpTimeOutMs = 0;
    CHK_RET(CalcTotalTimeout(ctxHandle_, tpHandle, tpTimeOutMs));

    if (envTimeoutMs < tpTimeOutMs) {
        errTimeout_ = FindMinTaHwValueGreaterThan(tpTimeOutMs);
        HCCL_WARNING("[TaTimeoutFinalDecider] Env timeout [%ums] < TP timeout [%ums]. Auto upgrade TA to "
                     "hw_val[%u] (%ums).",
            envTimeoutMs, tpTimeOutMs, errTimeout_, TaHwValueToMs(errTimeout_));
    } else {
        // 规则: 否则，直接使用环境变量对应的挡位 (对齐到 0/8/16/24)
        // 注意：这里我们取环境变量所在挡位的基准值 (例如 env=10 -> 取 8)
        errTimeout_ = envValue;
        HCCL_INFO("[TaTimeoutFinalDecider] Env timeout [%ums] >= TP timeout [%ums]. Use env gear base "
                  "hw_val[%u] (%ums).",
            envTimeoutMs, tpTimeOutMs, envValue, envTimeoutMs);
    }
    return HCCL_SUCCESS;
}

HcclResult CcuConnection::UpdateInitStatus()
{
    switch (innerStatus_) {
        case InnerStatus::INIT:
        case InnerStatus::TP_INFO_GETTING: {
            auto ret = GetTpInfo();
            if (ret == HcclResult::HCCL_E_AGAIN) {
                innerStatus_ = InnerStatus::TP_INFO_GETTING;
                break; // 状态不改变退出，下轮状态机进入继续执行
            }
            CHK_RET(ret);

            GetTaTimeOut();
            ret = CreateJetty(); // 不退出继续调用下个异步接口
            if (ret == HcclResult::HCCL_E_AGAIN) {
                innerStatus_ = InnerStatus::JETTY_CREATING;
                break;
            }
            CHK_RET(ret);

            innerStatus_ = InnerStatus::EXCHANGEABLE;
            status_ = CcuConnStatus::EXCHANGEABLE;
            break;
        }
        case InnerStatus::JETTY_CREATING: {
            GetTaTimeOut();
            auto ret = CreateJetty();
            if (ret == HcclResult::HCCL_E_AGAIN) {
                innerStatus_ = InnerStatus::JETTY_CREATING;
                break; // 状态不改变退出，下轮状态机进入继续执行
            }
            CHK_RET(ret);

            innerStatus_ = InnerStatus::EXCHANGEABLE;
            status_      = CcuConnStatus::EXCHANGEABLE;
            break;
        }
        default:
            return ReturnErrorStatus(std::string(__func__));
    }

    return HcclResult::HCCL_SUCCESS;
}

HcclResult CcuConnection::CreateJetty()
{
    if (isJettyCreated_) {
        return HcclResult::HCCL_SUCCESS;
    }

    isJettyCreated_ = true;
    for (size_t i = 0; i < jettyNum_; i++) {
        auto ret = ccuJettys_[i]->CreateJetty(errTimeout_);
        if (ret == HcclResult::HCCL_E_AGAIN) {
            // 不提供日志避免刷屏
            isJettyCreated_ = isJettyCreated_ && false;
            continue;
        }

        if (ret != HcclResult::HCCL_SUCCESS) {
            isJettyCreated_ = true;
            HCCL_ERROR("[CcuConnection][%s] failed, hccl result[%d]", __func__, ret);
            return HcclResult::HCCL_E_NETWORK;
        }
    }

    return isJettyCreated_ ?
        HcclResult::HCCL_SUCCESS: HcclResult::HCCL_E_AGAIN;
}

inline uint32_t GetRandomNum()
{
    uint32_t randNum = std::rand();
    return randNum;
}

void CcuConnection::GenerateLocalPsn()
{
    jettyImportCfg_.localPsn = GetRandomNum();
}

HcclResult CcuConnection::GetTpInfo()
{
    if (tpProtocol_ == TpProtocol::INVALID) { // 不感知tp建链，当前默认不支持
        HCCL_ERROR("[CcuConnection][%s] failed, tpProtocol[%s] is not expected.",
            __func__, tpProtocol_.Describe().c_str());
        return HcclResult::HCCL_E_PARA;
    }

    HcclResult ret = TpMgr::GetInstance(devPhyId_)
        .GetTpInfo({locAddr_, rmtAddr_, tpProtocol_}, tpInfo_);
    if (ret == HcclResult::HCCL_E_AGAIN) {
        // 此处可能刷屏，非必要勿加日志
        return ret;
    }

    if (ret != HcclResult::HCCL_SUCCESS) {
        HCCL_ERROR("[CcuConnection][%s] failed, hccl result[%d]", __func__, ret);
        return HcclResult::HCCL_E_NETWORK;
    }

    jettyImportCfg_.localTpHandle = tpInfo_.tpHandle;
    return HcclResult::HCCL_SUCCESS;
}

HcclResult CcuConnection::Serialize(std::vector<char> &dtoData)
{
    if (status_ != CcuConnStatus::EXCHANGEABLE) {
        HCCL_ERROR("[CcuConnection][%s] failed, not init completed yet, "
            "status[%s].", __func__, status_.Describe().c_str());
        return HcclResult::HCCL_E_INTERNAL;
    }

    Hccl::BinaryStream dtoStream;
    dtoStream << ccuBufAddr_;
    dtoStream << ccuBufTokenId_;
    dtoStream << ccuBufTokenValue_;
    HCCL_INFO("[CcuConnection][%s], ccuBufAddr[%llx]", __func__, ccuBufAddr_);

    dtoStream << jettyNum_;
    HCCL_INFO("[CcuConnection][%s], jettyNum[%u]", __func__, jettyNum_);
    for (const auto &ccuJetty : ccuJettys_) {
        dtoStream << ccuJetty->GetCreateJettyParam().tokenValue; 
        const auto &outParam = ccuJetty->GetJettyedOutParam();
        dtoStream << outParam.key; // 此处的qpKey是数组
        dtoStream << outParam.keySize;
    }

    if (tpProtocol_ != TpProtocol::INVALID) {
        dtoStream << jettyImportCfg_.localTpHandle;
        dtoStream << jettyImportCfg_.localPsn;
        HCCL_INFO("[CcuConnection][%s] tpProtocol[%s], localTpHandle[0x%llx], localPsn[%u].",
            __func__, tpProtocol_.Describe().c_str(), jettyImportCfg_.localTpHandle,
            jettyImportCfg_.localPsn);
    }

    dtoData.clear();
    dtoStream.Dump(dtoData);
    return HcclResult::HCCL_SUCCESS;
}

HcclResult CcuConnection::Deserialize(const std::vector<char> &dtoData)
{
    if (status_ != CcuConnStatus::EXCHANGEABLE) {
        HCCL_ERROR("[CcuConnection][%s] failed, not init completed yet, "
            "status[%s].", __func__, status_.Describe().c_str());
        return HcclResult::HCCL_E_INTERNAL;
    }

    std::vector<char> rmtDtoData = dtoData;
    Hccl::BinaryStream dtoStream(rmtDtoData);
    dtoStream >> rmtCcuBufAddr_;
    dtoStream >> rmtCcuBufTokenId_;
    dtoStream >> rmtCcuBufTokenValue_;
    HCCL_INFO("[CcuConnection][%s], rmtCcuBufAddr[%llx].", __func__, rmtCcuBufAddr_);

    uint32_t remoteJettySize{0};
    dtoStream >> remoteJettySize;

    importJettyCtxs_.clear();
    importJettyCtxs_.resize(remoteJettySize);
    HCCL_INFO("[CcuConnection][%s], remoteJettySize[%u].", __func__, remoteJettySize);

    for (auto &importCtx : importJettyCtxs_) {
        dtoStream >> importCtx.inParam.tokenValue;
        dtoStream >> importCtx.remoteQpKey; // 保存key数组
        importCtx.inParam.key = importCtx.remoteQpKey; // 保存指针用于接口调用
        dtoStream >> importCtx.inParam.keyLen;
    }

    if (tpProtocol_ != TpProtocol::INVALID) {
        dtoStream >> jettyImportCfg_.remoteTpHandle;
        dtoStream >> jettyImportCfg_.remotePsn;

        HCCL_INFO("[CcuConnection][%s] tpEnable, remoteTpHandle[0x%llx], remotePsn[%u].",
            __func__, jettyImportCfg_.remoteTpHandle, jettyImportCfg_.remotePsn);
    }

    return HcclResult::HCCL_SUCCESS;
}

HcclResult CcuConnection::ImportJetty()
{
    if (isJettyImported_) {
        HCCL_INFO("[CcuConnection][%s] taJettys has been imported already.", __func__);
        return HcclResult::HCCL_SUCCESS;
    }

    if (innerStatus_ != InnerStatus::EXCHANGEABLE) {
        return ReturnErrorStatus(std::string(__func__));
    }

    // importJettyCtxs_.resize(jettyNum_);
    if (jettyNum_ != importJettyCtxs_.size()) {
        HCCL_ERROR("[CcuConnection][%s] failed to ImportJetty, "
            "jettyNum[%u] is not equal to importJettyCtxs.size[%u].",
            __func__, jettyNum_, importJettyCtxs_.size());
        return ReturnErrorStatus(std::string(__func__));
    }

    ResetRequestCtxs();
    for (size_t i = 0; i < jettyNum_; i++) {
        if (StartImportJettyRequest(i, reqHandles_[i]) != HcclResult::HCCL_SUCCESS) {
            return ReturnErrorStatus(std::string(__func__));
        }
    }

    innerStatus_ = InnerStatus::JETTY_IMPORTING;
    return HcclResult::HCCL_SUCCESS;
}

void CcuConnection::ResetRequestCtxs()
{
    reqHandles_.clear();
    reqHandles_.resize(jettyNum_);

    reqDataBuffers_.clear();
    reqDataBuffers_.resize(jettyNum_);

    remoteJettyHandlePtrs_.clear();
    remoteJettyHandlePtrs_.resize(jettyNum_);
}

HcclResult CcuConnection::StartImportJettyRequest(uint32_t jettyIndex, RequestHandle &reqHandle)
{
    if (tpProtocol_ == TpProtocol::INVALID) {
        return ReturnErrorStatus(std::string(__func__));
    }

    auto &importCtxInParam = importJettyCtxs_[jettyIndex].inParam;
    importCtxInParam.jettyImportCfg = jettyImportCfg_;
    importCtxInParam.jettyImportCfg.protocol = tpProtocol_;
    CHK_RET(HccpUbTpImportJettyAsync(ctxHandle_, importCtxInParam, reqDataBuffers_[jettyIndex],
        remoteJettyHandlePtrs_[jettyIndex], reqHandle));

    return HcclResult::HCCL_SUCCESS;
}

HcclResult CcuConnection::CheckRequestResults()
{
    if (reqHandles_.size() == 0) {
        return HcclResult::HCCL_SUCCESS;
    }

    // 检查所有下发异步请求是否完成
    std::vector<size_t> completedReqs;
    const uint32_t reqSize = reqHandles_.size();
    for (size_t i = 0; i < reqSize; i++) {
        RequestResult result = HccpGetAsyncReqResult(reqHandles_[i]);
        if (result == RequestResult::NOT_COMPLETED) {
            continue;
        }

        if (result != RequestResult::COMPLETED) {
            HCCL_ERROR("[CcuConnection][%s] failed, result[%s] is unexpected.",
                __func__, result.Describe().c_str());
            return HcclResult::HCCL_E_NETWORK;
        }

        // 记录已完成的reqHandles
        completedReqs.push_back(i);
    }

    // 删除已完成的reqHandles，避免重复查询
    for (int i = completedReqs.size() - 1; i >= 0; --i) {
        reqHandles_.erase(reqHandles_.begin() + completedReqs[i]);
    }

    // 检查是否有剩余reqHandles
    return reqHandles_.size() == 0 ?
        HcclResult::HCCL_SUCCESS : HcclResult::HCCL_E_AGAIN;
}

HcclResult CcuConnection::UpdateExchangeStatus()
{
    // 状态机保证为 InnerStatus::JETTY_IMPORTING
    auto ret = CheckRequestResults();
    if (ret == HcclResult::HCCL_E_AGAIN) {
        return HcclResult::HCCL_SUCCESS; // 操作成功，保持当前状态
    }
    CHK_RET(ret);

    for (size_t i = 0; i < jettyNum_; i++) {
        auto &outParam = importJettyCtxs_[i].outParam;
        struct QpImportInfoT *infoPtr =
            reinterpret_cast<QpImportInfoT *>(reqDataBuffers_[i].data());
        outParam.handle        =
            reinterpret_cast<TargetJettyHandle>(remoteJettyHandlePtrs_[i]);
        outParam.targetJettyVa = infoPtr->out.ub.tjettyHandle; // 该信息当前未使用
        outParam.tpn           = infoPtr->out.ub.tpn;
    }
    isJettyImported_ = true;

    CHK_RET(ConfigChannel());
    status_ = CcuConnStatus::CONNECTED;
    innerStatus_ = InnerStatus::CONNECTED;
    return HcclResult::HCCL_SUCCESS;
}

HcclResult CcuConnection::ConfigChannel()
{
    if (jettyNum_ != importJettyCtxs_.size()) {
        HCCL_ERROR("[CcuConnection][%s] failed, jettyNum[%u] is not equal to "
            "importJettyCtxs.size[%u].", __func__, jettyNum_, importJettyCtxs_.size());
        return HcclResult::HCCL_E_INTERNAL;
    }

    ChannelCfg cfg{};
    cfg.channelId = channelInfo_.channelId;
    Hccl::IpAddress rmtAddr{};
    CHK_RET(CommAddrToIpAddress(rmtAddr_, rmtAddr));
    CHK_RET(IpAddressToReverseHcclEid(rmtAddr, cfg.remoteEid)); // 配置ccu硬件需要使用反向eid
    cfg.tpn       = importJettyCtxs_[0].outParam.tpn; // tp handle复用所以tpn一致
    cfg.remoteCcuVa   = rmtCcuBufAddr_;
    cfg.memTokenId    = rmtCcuBufTokenId_;
    cfg.memTokenValue = rmtCcuBufTokenValue_;

    for (size_t i = 0; i < jettyNum_; i++) {
        const auto &ccuJetty = ccuJettys_[i];
        const auto &inParam = ccuJetty->GetCreateJettyParam();
        const auto &outParam = ccuJetty->GetJettyedOutParam();
        const auto &jettyInfo = channelInfo_.jettyInfos[i];
        cfg.jettyCfgs.emplace_back(JettyCfg{
            jettyInfo.jettyCtxId,
            outParam.dbVa,
            outParam.dbTokenId,
            inParam.tokenValue}); // 安全问题，禁止打印token相关信息
    }

    CHK_RET(CcuDevMgrImp::ConfigChannel(devLogicId_, dieId_, cfg));
    return HcclResult::HCCL_SUCCESS;
}

CcuConnection::~CcuConnection()
{
    (void)ReleaseConnRes();
}

HcclResult CcuConnection::ReleaseConnRes()
{
    for (auto &item : importJettyCtxs_) {
        if (item.outParam.handle != 0) {
            int32_t ret = RaCtxQpUnimport(ctxHandle_, item.outParam.handle);
            item.outParam.handle = 0;
            if (ret != 0) {
                HCCL_ERROR("[CcuComponent][%s] failed but passed, ctxHandle[%p] "
                    "remoteJettyHandle[%p], devLogicId[%d].", __func__,
                    ctxHandle_, item.outParam.handle, devLogicId_);
                status_ = CcuConnStatus::CONN_INVALID;
                innerStatus_ = InnerStatus::CONN_INVALID;
            }
        }
    }
    importJettyCtxs_.clear();

    if (tpInfo_.tpHandle != 0) { // tp handle 复用，只释放一次
        (void)TpMgr::GetInstance(devPhyId_)
            .ReleaseTpInfo({locAddr_, rmtAddr_, tpProtocol_}, tpInfo_);
        tpInfo_.tpHandle = 0;
    }

    // CcuJetty 生命周期跟随通信域CcuJettyMgr
    // 不需要connection主动销毁
    return HcclResult::HCCL_SUCCESS;
}

HcclResult CcuConnection::ReturnErrorStatus(const std::string &funcName)
{
    std::string errMsg = Hccl::StringFormat("[CcuConnection][%s] failed, [%s].",
        funcName.c_str(), Describe().c_str());
    status_ = CcuConnStatus::CONN_INVALID;
    innerStatus_ = InnerStatus::CONN_INVALID;
    HCCL_ERROR("%s", errMsg.c_str());
    return HcclResult::HCCL_E_INTERNAL;
}

std::string CcuConnection::Describe()
{
    Hccl::IpAddress locAddr{}, rmtAddr{};
    (void)CommAddrToIpAddress(locAddr_, locAddr);
    (void)CommAddrToIpAddress(rmtAddr_, rmtAddr);
    return Hccl::StringFormat("[CcuConnection[locAddr=%s, rmtAddr=%s, protocol=%s, "
        "status=%s, innerStatus=%s, [dieId=%u, channelId=%u, jettyNum=%u]]]",
        locAddr.Describe().c_str(), rmtAddr.Describe().c_str(), tpProtocol_.Describe().c_str(),
        status_.Describe().c_str(), innerStatus_.Describe().c_str(), dieId_, channelInfo_.channelId,
        jettyNum_);
}

HcclResult CcuConnection::Describe(std::string &dfxMsg)
{
    uint16_t udpSport = 0xFFFF; // 无法获取实际的udpSport，使用0xFFFF表示未知
    if (tpProtocol_ == TpProtocol::RTP) {
        struct TpAttr tpAttr {0};
        uint32_t attrBitmap = 1 << 13; // 13对应dataUdpSrcport
        EXCEPTION_HANDLE_BEGIN
        HcclResult ret = Hccl::HrtRaGetTpAttrAsync(devPhyId_, ctxHandle_, tpInfo_.tpHandle, attrBitmap, tpAttr, reqHandles_[0]);
        if (ret == HCCL_E_NOT_SUPPORT) {
            HCCL_ERROR("[DevUbConnection::%s] failed, this package does not support RaGetTpAttrAsync for device,"
                " please change new package. devPhyId[%u]", __func__, devPhyId_);
            return ret;
        } else if (ret != HCCL_SUCCESS) {
            HCCL_ERROR("[DevUbConnection::%s] failed, hccl result[%d]", __func__, ret);
            return ret;
        }
        EXCEPTION_HANDLE_END
        udpSport = tpAttr.dataUdpSrcport;
    }
    udpSport = udpSport & 0xFF;

    std::ostringstream oss;
    for (size_t i = 0; i < ccuJettys_.size(); ++i) {
        uint16_t jettyId = ccuJettys_[i]->GetJettyedOutParam().id;
        if (i != 0) {
            oss << ", ";
        }
        oss << jettyId;
    }
    std::string jettyIds = oss.str();

    Hccl::IpAddress locAddr{}, rmtAddr{};
    CHK_RET(CommAddrToIpAddress(locAddr_, locAddr));
    CHK_RET(CommAddrToIpAddress(rmtAddr_, rmtAddr));
    Hccl::Eid locEid = locAddr.GetReverseEid();
    Hccl::Eid rmtEid = rmtAddr.GetReverseEid();

    std::string dfxStr = Hccl::StringFormat("chip id[%u] die id[%u] func_id[%u] jetty id[%s] "
        "local %s remote %s udp sport[%u]",
        devLogicId_, dieId_, funcId_, jettyIds.c_str(), locEid.Describe().c_str(), rmtEid.Describe().c_str(), udpSport);
    dfxMsg += dfxStr;
    HCCL_INFO("[CcuConnection::%s] %s", __func__, dfxStr.c_str());
    return HcclResult::HCCL_SUCCESS;
}

uint32_t CcuConnection::GetDieId() const
{
    return dieId_;
}

uint32_t CcuConnection::GetChannelId() const
{
    return channelInfo_.channelId;
}

int32_t CcuConnection::GetDevLogicId() const
{
    return devLogicId_;
}

HcclResult CcuConnection::Clean()
{
    status_ = CcuConnStatus::INIT;
    innerStatus_ = InnerStatus::INIT;
    isJettyCreated_ = false;
    isJettyImported_ = false;
    CHK_RET(ReleaseConnRes());
    GenerateLocalPsn();

    // 销毁jetty要在ReleaseConnRes之后
    for (auto &ccuJetty : ccuJettys_) {
        ccuJetty->Clean();
    }
    return HcclResult::HCCL_SUCCESS;
}

} // namespace hcomm
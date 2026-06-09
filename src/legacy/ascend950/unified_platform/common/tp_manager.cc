/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "tp_manager.h"

#include "tp_qos.h"

#include <algorithm>
#include <cctype>
#include <string>

#include "exception_util.h"
#include "hccl_common_v2.h"
#include "invalid_params_exception.h"
#include "env_config/env_config.h"
#include "network_api_exception.h"

#include "hccp.h"
#include "hccp_async_ctx.h"
#include "hccp_ctx.h"
#include "orion_adapter_rts.h"
#include "rdma_handle_manager.h"

namespace Hccl {

// RaGetTpAttrAsync 属性位图常量；匿名命名空间内工具与 TpManager 成员函数共用
constexpr uint32_t kTpAttrSlAvailableBit = 17U;
constexpr uint32_t kTpAttrBitmapSl = (1U << 10U);
constexpr uint32_t kTpAttrBitmapDscp = (1U << 8U);
constexpr uint32_t kTpAttrDscpConfigModeBit = 18U;

namespace {

constexpr uint32_t kGetTpAttrOpcode = 106U;
constexpr uint32_t kGetTpAttrVersion = 2U;

static constexpr QosKey QosMapKey(uint32_t qos) noexcept
{
    return static_cast<QosKey>(qos & 0xFFU);
}

static bool DeviceSupportsRaGetTpAttrAsync(uint32_t phyId)
{
    u32 tpAttrVersion = 0;
    const s32 ret = RaGetInterfaceVersion(phyId, kGetTpAttrOpcode, &tpAttrVersion);
    return (ret == 0 && tpAttrVersion >= kGetTpAttrVersion);
}

static uint32_t BuildBootstrapAttrBitmap(TpProtocol tpProtocol)
{
    uint32_t bitmap = (1U << kTpAttrSlAvailableBit) | kTpAttrBitmapSl | 0x3U;
    if (tpProtocol == TpProtocol::UBOE) {
        bitmap |= kTpAttrBitmapDscp | (1U << kTpAttrDscpConfigModeBit);
    }
    return bitmap;
}

static RdmaHandle ResolveUbRdmaHandle(const uint32_t phyId, const Hccl::IpAddress &locAddr, const bool isSync)
{
    if (isSync) {
        Hccl::IpAddress addr = locAddr;
        return RdmaHandleManager::GetInstance().GetByAddr(phyId, LinkProtoType::UB, addr,
            Hccl::PortDeploymentType::HOST_NET);
    }
    return RdmaHandleManager::GetInstance().GetByIp(phyId, locAddr);
}

static bool ParseDscpFromCfgByQos(const std::string &cfg, uint8_t qos, uint8_t &dscpOut)
{
    std::vector<uint32_t> nums;
    nums.reserve(32);
    uint32_t cur = 0;
    bool inNum = false;
    for (char ch : cfg) {
        if (std::isdigit(static_cast<unsigned char>(ch)) != 0) {
            cur = cur * 10U + static_cast<uint32_t>(ch - '0');
            inNum = true;
            continue;
        }
        if (inNum) {
            nums.push_back(cur);
            cur = 0;
            inNum = false;
        }
    }
    if (inNum) {
        nums.push_back(cur);
    }
    if (nums.empty()) {
        return false;
    }
    if (nums.size() > static_cast<size_t>(qos)) {
        const uint32_t dscp = nums[qos];
        if (dscp <= 63U) {
            dscpOut = static_cast<uint8_t>(dscp);
            return true;
        }
    }
    for (size_t i = 0; i + 1 < nums.size(); i += 2) {
        if (nums[i] == qos && nums[i + 1] <= 63U) {
            dscpOut = static_cast<uint8_t>(nums[i + 1]);
            return true;
        }
    }
    return false;
}

static bool GetDscpByQosFromHccnCfg(const uint32_t devPhyId, uint8_t qos, uint8_t &dscpOut)
{
    struct RaInfo info {};
    info.mode = NETWORK_OFFLINE;
    info.phyId = devPhyId;
    constexpr unsigned int kCfgBufLen = 2048U;
    std::vector<char> value(kCfgBufLen, 0);
    unsigned int valueLen = kCfgBufLen;
    const int ret = RaGetHccnCfg(&info, HCCN_CFG_QOS_DSCP, value.data(), &valueLen);
    if (ret != 0) {
        HCCL_WARNING("[TpManager][%s] RaGetHccnCfg failed ret[%d] phyId[%u] qos[%u].", __func__, ret, devPhyId,
            static_cast<unsigned>(qos));
        return false;
    }
    if (valueLen == 0U) {
        HCCL_WARNING("[TpManager][%s] RaGetHccnCfg empty cfg phyId[%u] qos[%u].", __func__, devPhyId,
            static_cast<unsigned>(qos));
        return false;
    }
    if (valueLen > kCfgBufLen) {
        valueLen = kCfgBufLen;
    }
    const std::string cfg(value.data(), valueLen);
    HCCL_INFO("[TpManager][%s] RaGetHccnCfg ok phyId[%u] qos[%u] valueLen[%u] qos_dscp[%s].", __func__, devPhyId,
        static_cast<unsigned>(qos), valueLen, cfg.c_str());
    if (!ParseDscpFromCfgByQos(cfg, qos, dscpOut)) {
        HCCL_WARNING("[TpManager][%s] parse qos_dscp failed phyId[%u] qos[%u] cfg[%s].", __func__, devPhyId,
            static_cast<unsigned>(qos), cfg.c_str());
        return false;
    }
    return true;
}

static void BuildCommitTpAttr(const RaUbGetTpInfoParam &param, uint32_t devPhyId, uint32_t mappedSl,
    const struct TpAttr &linkTpAttr, uint32_t tpInfoNum, struct TpAttr &tpAttr, uint32_t &attrBitmap)
{
    tpAttr = {};
    tpAttr.sl = static_cast<uint8_t>(mappedSl & 0xFU);
    attrBitmap = kTpAttrBitmapSl;
    if (param.tpProtocol == TpProtocol::UBOE && (linkTpAttr.dscpConfigMode & 1U) == 0U) {
        const uint16_t slMask = hcomm::TpQos::ReadSlMask(linkTpAttr);
        const uint8_t dscpLookupQos = hcomm::TpQos::DscpLookupQos(tpInfoNum, slMask,
            hcomm::TpQos::MakeInput(static_cast<uint32_t>(param.tpProtocol), param.qos, param.slLevelCount,
                param.loopFirstTpLowestSl));
        uint8_t dscp = 33U;
        (void)GetDscpByQosFromHccnCfg(devPhyId, dscpLookupQos, dscp);
        tpAttr.dscp = static_cast<uint8_t>(dscp & 0x3FU);
        attrBitmap |= kTpAttrBitmapDscp;
    }
}

} // namespace

inline TpInfo ParseTpInfo(const struct HccpTpInfo *infoPtr)
{
    TpInfo tpInfo;
    tpInfo.tpHandle = infoPtr->tpHandle;
    return tpInfo;
}

TpManager& TpManager::GetInstance(const int32_t deviceLogicId)
{
    static TpManager tpManager[MAX_MODULE_DEVICE_NUM + 1];

    if (deviceLogicId < 0 ||
        static_cast<uint32_t>(deviceLogicId) > MAX_MODULE_DEVICE_NUM) {
        THROW<InvalidParamsException>("[TpManager][%s] failed to get instance, "
            "devLogicId[%d] should be less than %u.", __func__,
            deviceLogicId, MAX_MODULE_DEVICE_NUM);
    }

    tpManager[deviceLogicId].devLogicId = deviceLogicId;

    return tpManager[deviceLogicId];
}

void TpManager::Init()
{
    if (initFlag) {
        return;
    }

    devPhyId = HrtGetDevicePhyIdByIndex(devLogicId);
    initFlag = true;
}

bool TpManager::CheckRequestResult(RequestHandle &reqHandle) const
{
    if (reqHandle == 0) {
        return true;
    }

    ReqHandleResult result = HrtRaGetAsyncReqResult(reqHandle);
    if (result == ReqHandleResult::NOT_COMPLETED) {
        return false;
    }

    if (result != ReqHandleResult::COMPLETED) {
        THROW<InternalException>("[TpManager][%s] failed, result[%s] is unexpected.",
            __func__, result.Describe().c_str());
    }

    return true;
}

HcclResult CheckTpProtocol(const TpProtocol tpProtocol) {
    if (tpProtocol != TpProtocol::CTP && tpProtocol != TpProtocol::TP && tpProtocol != TpProtocol::UBOE) {
        HCCL_WARNING("[TpManager][%s] failed, tpProtocol[%d] is not supported.",
            __func__, tpProtocol);
        return HcclResult::HCCL_E_NOT_SUPPORT;
    }

    return HcclResult::HCCL_SUCCESS;
}

bool TpManager::TryGetLinkAttrCache(const RaUbGetTpInfoParam &param, TpAttr &outAttr)
{
    std::lock_guard<std::mutex> lock(GetInfoCtxMutex(param.tpProtocol));
    const auto &linkMap = GetLinkAttrMap(param.tpProtocol);
    const auto lit = linkMap.find(param.locAddr);
    if (lit == linkMap.end()) {
        return false;
    }
    const auto rit = lit->second.find(param.rmtAddr);
    if (rit == lit->second.end() || !rit->second.valid) {
        return false;
    }
    outAttr = rit->second.tpAttr;
    return true;
}

void TpManager::PutLinkAttrCache(const RaUbGetTpInfoParam &param, const TpAttr &attr)
{
    std::lock_guard<std::mutex> lock(GetInfoCtxMutex(param.tpProtocol));
    auto &entry = GetLinkAttrMap(param.tpProtocol)[param.locAddr][param.rmtAddr];
    entry.tpAttr = attr;
    entry.valid = true;
}

void TpManager::ClearLinkAttrCacheLocked(const TpProtocol tpProtocol, const IpAddress &locAddr, const IpAddress &rmtAddr)
{
    auto &linkMap = GetLinkAttrMap(tpProtocol);
    const auto lit = linkMap.find(locAddr);
    if (lit == linkMap.end()) {
        return;
    }
    const auto rit = lit->second.find(rmtAddr);
    if (rit == lit->second.end()) {
        return;
    }
    lit->second.erase(rit);
    if (lit->second.empty()) {
        linkMap.erase(lit);
    }
}

HcclResult TpManager::StoreGetTpInfoResult(const RaUbGetTpInfoParam &param, const RequestCtx &reqCtx, TpInfo &tpInfo)
{
    TpInfo tmpTpInfo{};
    tmpTpInfo.tpHandle = reqCtx.selectedTpHandle;
    tmpTpInfo.mappedJettyPriority = reqCtx.mappedSl & 0xFU;
    tmpTpInfo.hasMappedJettyPriority = true;
    if (param.tpProtocol != TpProtocol::CTP) {
        tmpTpInfo.at = reqCtx.linkTpAttr.at;
        tmpTpInfo.retryTimesInit = reqCtx.linkTpAttr.retryTimesInit;
        tmpTpInfo.hasLinkAtRetry = true;
    }
    HCCL_INFO("[TpManager][%s] tp qos mapping ok: tpHandle[%llu] tpListIndex[%u] mappedSl[%u] qos[%u] param[%s].",
        __func__, tmpTpInfo.tpHandle, reqCtx.tpListIndex, static_cast<unsigned>(reqCtx.mappedSl & 0xFU),
        param.qos & 0xFFU, param.Describe().c_str());

    const QosKey qosKey = QosMapKey(param.qos);
    {
        std::lock_guard<std::mutex> lock(GetInfoCtxMutex(param.tpProtocol));
        auto &infoMap = GetInfoCtxMap(param.tpProtocol);
        auto &qosInfoMap = infoMap[param.locAddr][param.rmtAddr];
        auto qIt = qosInfoMap.find(qosKey);
        if (qIt != qosInfoMap.end() && qIt->second.tpInfo.tpHandle == tmpTpInfo.tpHandle) {
            qIt->second.useCnt += 1;
            tpInfo = qIt->second.tpInfo;
        } else {
            qosInfoMap[qosKey] = TpInfoCtx{tmpTpInfo, 1};
            tpInfo = qosInfoMap[qosKey].tpInfo;
        }
    }

    if (reqCtx.selectedTpHandle != 0U) {
        std::lock_guard<std::mutex> lock(tpAttrCtxMutex);
        tpAttrCtxMap[reqCtx.selectedTpHandle] = TpAttrCtx{TpAttrInfo(reqCtx.linkTpAttr), 1};
    }
    return HcclResult::HCCL_SUCCESS;
}

HcclResult TpManager::FinalizeGetTpInfo(RequestCtx reqCtx, const RaUbGetTpInfoParam &param, ReqQosMap &qosMap,
    const ReqQosMap::iterator it, std::unique_lock<std::mutex> &reqCtxLock, TpInfo &tpInfo)
{
    const uint32_t tpInfoNum = reqCtx.tpInfoNum;
    if (tpInfoNum == 0U) {
        qosMap.erase(it);
        reqCtxLock.unlock();
        HCCL_WARNING("[TpManager][%s] failed to find tp info, tpInfoNum is 0, param[%s].", __func__,
            param.Describe().c_str());
        return HcclResult::HCCL_E_NOT_FOUND;
    }

    const HcclResult ret = StoreGetTpInfoResult(param, reqCtx, tpInfo);
    qosMap.erase(it);
    reqCtxLock.unlock();
    return ret;
}

HcclResult TpManager::SelectTpByQosSl(const RaUbGetTpInfoParam &param, RequestCtx &reqCtx)
{
    const uint32_t tpInfoNum = reqCtx.tpInfoNum;
    const struct HccpTpInfo *baseInfoPtr =
        reinterpret_cast<const struct HccpTpInfo *>(reqCtx.dataBuffer.data());
    const uint16_t slMask = hcomm::TpQos::ReadSlMask(reqCtx.linkTpAttr);
    const uint32_t slAvailableCnt = hcomm::TpQos::CountSlTiers(slMask);
    HCCL_INFO("[TpManager][%s] bootstrap attr ok: slMask[0x%04x] slAvailableCnt[%u] param[%s].", __func__,
        static_cast<unsigned>(slMask), slAvailableCnt, param.Describe().c_str());
    if (slAvailableCnt == 0U) {
        HCCL_ERROR("[TpManager][%s] sl_available mask empty, param[%s].", __func__, param.Describe().c_str());
        return HcclResult::HCCL_E_INTERNAL;
    }

    uint32_t tpListIndex = 0U;
    uint32_t mappedSl = 0U;
    const hcomm::TpQosInput qosInput = hcomm::TpQos::MakeInput(static_cast<uint32_t>(param.tpProtocol), param.qos,
        param.slLevelCount, param.loopFirstTpLowestSl);
    if (!hcomm::TpQos::Map(tpInfoNum, slMask, qosInput, tpListIndex, mappedSl)) {
        HCCL_ERROR("[TpManager][%s] TpQos::Map failed, param[%s].", __func__, param.Describe().c_str());
        return HcclResult::HCCL_E_INTERNAL;
    }
    if (tpListIndex >= tpInfoNum) {
        HCCL_ERROR("[TpManager][%s] tpListIndex out of range: tpListIndex[%u] tpInfoNum[%u] param[%s].", __func__,
            tpListIndex, tpInfoNum, param.Describe().c_str());
        return HcclResult::HCCL_E_INTERNAL;
    }

    reqCtx.tpListIndex = tpListIndex;
    reqCtx.mappedSl = mappedSl;
    reqCtx.selectedTpHandle = baseInfoPtr[tpListIndex].tpHandle;
    return HcclResult::HCCL_SUCCESS;
}

HcclResult TpManager::SelectTpAndContinueAsync(const RaUbGetTpInfoParam &param, RequestCtx &reqCtx, ReqQosMap &qosMap,
    const ReqQosMap::iterator it, std::unique_lock<std::mutex> &reqCtxLock, TpInfo &tpInfo)
{
    CHK_RET(SelectTpByQosSl(param, reqCtx));

    if (param.tpProtocol == TpProtocol::CTP) {
        return FinalizeGetTpInfo(std::move(reqCtx), param, qosMap, it, reqCtxLock, tpInfo);
        // CTP 直接 finalize 不参与setTpAttr
    }

    try {
        CHK_RET(StartCommitTpAttr(param, reqCtx));
    } catch (...) {
        qosMap.erase(it);
        throw;
    }
    reqCtx.phase = ReqPhase::WAIT_COMMIT;
    return HcclResult::HCCL_E_AGAIN;
}

HcclResult TpManager::AdvanceFromWaitList(const RaUbGetTpInfoParam &param, RequestCtx &reqCtx, ReqQosMap &qosMap,
    const ReqQosMap::iterator it, std::unique_lock<std::mutex> &reqCtxLock, TpInfo &tpInfo)
{
    if (reqCtx.tpInfoNum == 0U) {
        qosMap.erase(it);
        reqCtxLock.unlock();
        HCCL_WARNING("[TpManager][%s] failed to find tp info, tpInfoNum is 0, param[%s].", __func__,
            param.Describe().c_str());
        return HcclResult::HCCL_E_NOT_FOUND;
    }

    if (!DeviceSupportsRaGetTpAttrAsync(devPhyId)) {
        qosMap.erase(it);
        reqCtxLock.unlock();
        HCCL_ERROR("[TpManager][%s] device does not support GetTpAttr, devPhyId[%u] param[%s].", __func__, devPhyId,
            param.Describe().c_str());
        return HcclResult::HCCL_E_NOT_SUPPORT;
    }

    const struct HccpTpInfo *list = reinterpret_cast<const struct HccpTpInfo *>(reqCtx.dataBuffer.data());
    HCCL_INFO("[TpManager][GetTpInfo] list stage ok, devPhyId[%u] tpInfoNum[%u] firstTpHandle[%llu] param[%s].",
        devPhyId, reqCtx.tpInfoNum, static_cast<unsigned long long>(list[0].tpHandle), param.Describe().c_str());

    if (TryGetLinkAttrCache(param, reqCtx.linkTpAttr)) {
        HCCL_INFO("[TpManager][GetTpInfo] linkAttr cache hit, skip bootstrap get, param[%s].", param.Describe().c_str());
        return SelectTpAndContinueAsync(param, reqCtx, qosMap, it, reqCtxLock, tpInfo); // 缓存命中不需要去getTpAttr
    }

    try {
        CHK_RET(StartBootstrapLinkAttr(param, reqCtx));
    } catch (...) {
        qosMap.erase(it);
        throw;
    }
    HCCL_INFO("[TpManager][GetTpInfo] RaGetTpAttrAsync submitted, devPhyId[%u] reqHandle[%llu] phase[WAIT_ATTR] "
              "bootstrapBitmap[0x%x] param[%s].",
        devPhyId, static_cast<unsigned long long>(reqCtx.handle), reqCtx.bootstrapAttrBitmap, param.Describe().c_str());
    return HcclResult::HCCL_E_AGAIN;
}

HcclResult TpManager::AdvanceFromWaitAttr(const RaUbGetTpInfoParam &param, RequestCtx &reqCtx, ReqQosMap &qosMap,
    const ReqQosMap::iterator it, std::unique_lock<std::mutex> &reqCtxLock, TpInfo &tpInfo)
{
    PutLinkAttrCache(param, reqCtx.linkTpAttr);
    const uint16_t slMask = hcomm::TpQos::ReadSlMask(reqCtx.linkTpAttr);
    HCCL_INFO("[TpManager][GetTpInfo] bootstrap attr ok, slMask[0x%04x] slBitmap[0x%x] param[%s].",
        static_cast<unsigned>(slMask), static_cast<unsigned>(reqCtx.linkTpAttr.slBitmap), param.Describe().c_str());
    return SelectTpAndContinueAsync(param, reqCtx, qosMap, it, reqCtxLock, tpInfo);
}

HcclResult TpManager::AdvanceFromWaitCommit(const RaUbGetTpInfoParam &param, RequestCtx &reqCtx, ReqQosMap &qosMap,
    const ReqQosMap::iterator it, std::unique_lock<std::mutex> &reqCtxLock, TpInfo &tpInfo)
{
    return FinalizeGetTpInfo(std::move(reqCtx), param, qosMap, it, reqCtxLock, tpInfo);
}

HcclResult TpManager::RunHostSyncGetTpInfo(const RaUbGetTpInfoParam &param, TpInfo &tpInfo)
{
    std::unique_lock<std::mutex> reqCtxLock(GetReqCtxMutex(param.tpProtocol));

    if (!DeviceSupportsRaGetTpAttrAsync(devPhyId)) {
        HCCL_ERROR("[TpManager][%s] device does not support GetTpAttr, devPhyId[%u] param[%s].", __func__, devPhyId,
            param.Describe().c_str());
        return HcclResult::HCCL_E_NOT_SUPPORT;
    }

    RequestCtx reqCtx{};
    StartGetTpInfoListRequest(param, reqCtx, true);

    if (reqCtx.tpInfoNum == 0U) {
        HCCL_WARNING("[TpManager][%s] failed to find tp info, tpInfoNum is 0, param[%s].", __func__,
            param.Describe().c_str());
        return HcclResult::HCCL_E_NOT_FOUND;
    }

    const struct HccpTpInfo *list = reinterpret_cast<const struct HccpTpInfo *>(reqCtx.dataBuffer.data());
    HCCL_INFO("[TpManager][RunHostSyncGetTpInfo] list ok, devPhyId[%u] tpInfoNum[%u] firstTpHandle[%llu] param[%s].",
        devPhyId, reqCtx.tpInfoNum, static_cast<unsigned long long>(list[0].tpHandle), param.Describe().c_str());

    if (!TryGetLinkAttrCache(param, reqCtx.linkTpAttr)) {
        const RdmaHandle rdmaHandle = ResolveUbRdmaHandle(devPhyId, param.locAddr, true);
        if (!rdmaHandle) {
            THROW<InternalException>("[TpManager][%s] can not find host rdmaHandle, devPhyId[%u] locAddr[%s].",
                __func__, devPhyId, param.locAddr.Describe().c_str());
        }
        reqCtx.bootstrapAttrBitmap = BuildBootstrapAttrBitmap(param.tpProtocol);
        RaUbGetTpAttr(rdmaHandle, list[0].tpHandle, reqCtx.bootstrapAttrBitmap, reqCtx.linkTpAttr);
        PutLinkAttrCache(param, reqCtx.linkTpAttr);
    }

    CHK_RET(SelectTpByQosSl(param, reqCtx)); // 由通信域qos和GetTpAttr获得的SL进行映射计算

    if (param.tpProtocol == TpProtocol::TP || param.tpProtocol == TpProtocol::UBOE) {
        SyncCommitTpAttr(param, reqCtx);
    }

    CHK_RET(StoreGetTpInfoResult(param, reqCtx, tpInfo));
    return HcclResult::HCCL_SUCCESS;
}

HcclResult TpManager::RunAsyncGetTpInfo(const RaUbGetTpInfoParam &param, TpInfo &tpInfo)
{
    std::unique_lock<std::mutex> reqCtxLock(GetReqCtxMutex(param.tpProtocol));
    auto &reqCtxMap = GetReqCtxMap(param.tpProtocol);
    const QosKey qosKey = QosMapKey(param.qos);

    auto &qosReqMap = reqCtxMap[param.locAddr][param.rmtAddr];
    auto it = qosReqMap.find(qosKey);
    if (it == qosReqMap.end()) {
        RequestCtx &reqCtx = qosReqMap[qosKey];
        StartGetTpInfoListRequest(param, reqCtx, false);
        return HcclResult::HCCL_E_AGAIN;
    }

    RequestCtx &reqCtx = it->second;

    if (reqCtx.handle != 0U) {
        if (!CheckRequestResult(reqCtx.handle)) {
            return HcclResult::HCCL_E_AGAIN;
        }
        reqCtx.handle = 0U;
    }

    switch (reqCtx.phase) {
    case ReqPhase::WAIT_LIST:
        // List 已完成：取 linkAttr（或命中缓存）后进入 Mapping / WAIT_ATTR
        return AdvanceFromWaitList(param, reqCtx, qosReqMap, it, reqCtxLock, tpInfo);
    case ReqPhase::WAIT_ATTR:
        // bootstrap GetTpAttr 已完成：落 linkCache，执行 QoS Mapping
        return AdvanceFromWaitAttr(param, reqCtx, qosReqMap, it, reqCtxLock, tpInfo);
    case ReqPhase::WAIT_COMMIT:
        // SetTpAttr 已完成：写 tpInfo 缓存并返回 SUCCESS
        return AdvanceFromWaitCommit(param, reqCtx, qosReqMap, it, reqCtxLock, tpInfo);
    default:
        qosReqMap.erase(it);
        reqCtxLock.unlock();
        HCCL_ERROR("[TpManager][%s] unexpected phase[%u] param[%s].", __func__,
            static_cast<unsigned>(reqCtx.phase), param.Describe().c_str());
        return HcclResult::HCCL_E_INTERNAL;
    }
}

HcclResult TpManager::GetTpInfo(const RaUbGetTpInfoParam &param, TpInfo &tpInfo, bool isSync)
{
    CHK_RET(CheckTpProtocol(param.tpProtocol));
    if (FindAndGetTpInfo(param, tpInfo) == HcclResult::HCCL_SUCCESS) {
        return HcclResult::HCCL_SUCCESS;
    }

    if (isSync) {
        return RunHostSyncGetTpInfo(param, tpInfo);
    }
    return RunAsyncGetTpInfo(param, tpInfo);
}

HcclResult TpManager::FindAndGetTpAttr(const TpHandle tpHandle, TpAttrInfo &tpAttrInfo)
{
    std::lock_guard<std::mutex> lock(tpAttrCtxMutex);
    auto attrIter = tpAttrCtxMap.find(tpHandle);
    if (attrIter != tpAttrCtxMap.end()) {
        attrIter->second.useCnt += 1;
        tpAttrInfo = attrIter->second.tpAttrInfo;
        return HcclResult::HCCL_SUCCESS;
    }

    return HcclResult::HCCL_E_NOT_FOUND;
}

HcclResult TpManager::GetTpAttr(const GetTpAttrParam &param, TpAttrInfo &tpAttrInfo, RdmaHandle rdmaHandle)
{
    const TpHandle tpHandle = param.tpHandle;
    if (FindAndGetTpAttr(tpHandle, tpAttrInfo) == HcclResult::HCCL_SUCCESS) {
        return HcclResult::HCCL_SUCCESS;
    }

    std::unique_lock<std::mutex> reqCtxLock(tpAttrReqMutex);
    auto reqCtxIter = tpAttrReqCtxMap.find(tpHandle);
    if (reqCtxIter == tpAttrReqCtxMap.end()) {
        HCCL_INFO("[TpManager][%s] get new tpAttr, param[%s].", __func__,
            param.Describe().c_str());

        TpAttrRequestCtx &reqCtx = tpAttrReqCtxMap[tpHandle];
        CHK_RET(StartGetTpAttrRequest(param, reqCtx, rdmaHandle));
        return HcclResult::HCCL_E_AGAIN;
    }

    auto &reqCtx = reqCtxIter->second;
    if (!CheckRequestResult(reqCtx.handle)) {
        return HcclResult::HCCL_E_AGAIN;
    }

    TpAttrRequestCtx completedReqCtx = reqCtxIter->second;
    tpAttrReqCtxMap.erase(reqCtxIter);
    reqCtxLock.unlock();

    return HandleCompletedTpAttrRequest(std::move(completedReqCtx), tpHandle, tpAttrInfo);
}

HcclResult TpManager::StartGetTpAttrRequest(const GetTpAttrParam &param,
    TpManager::TpAttrRequestCtx &reqCtx, RdmaHandle rdmaHandle) const
{
    void *raReqHandle = nullptr;
    s32 ret = RaGetTpAttrAsync(rdmaHandle, param.tpHandle,
        const_cast<uint32_t*>(&param.attrBitmap), &reqCtx.tpAttr, &raReqHandle);
    if (ret != 0 || !raReqHandle) {
        HCCL_ERROR("[TpManager][%s] failed, call RaGetTpAttrAsync error[%d] raReqHandle[%p], "
            "tpHandle[0x%llx] attrBitmap[0x%x].", __func__, ret, raReqHandle,
            param.tpHandle, param.attrBitmap);
        return HcclResult::HCCL_E_NETWORK;
    }

    reqCtx.handle = reinterpret_cast<RequestHandle>(raReqHandle);
    HCCL_INFO("[TpManager][%s] success, tpHandle[0x%llx] reqHandle[%llu].",
        __func__, param.tpHandle, reqCtx.handle);
    return HcclResult::HCCL_SUCCESS;
}

HcclResult TpManager::HandleCompletedTpAttrRequest(const TpManager::TpAttrRequestCtx reqCtx,
    const TpHandle tpHandle, TpAttrInfo &tpAttrInfo)
{
    TpAttrInfo tmpTpAttrInfo(reqCtx.tpAttr);

    std::lock_guard<std::mutex> lock(tpAttrCtxMutex);
    tpAttrCtxMap[tpHandle] = {std::move(tmpTpAttrInfo), 1};
    
    tpAttrInfo = tpAttrCtxMap[tpHandle].tpAttrInfo;
    return HcclResult::HCCL_SUCCESS;
}

HcclResult TpManager::ReleaseTpInfo(const RaUbGetTpInfoParam &param, const TpInfo &tpInfo)
{
    const QosKey qosKey = QosMapKey(param.qos);
    std::lock_guard<std::mutex> lock(GetInfoCtxMutex(param.tpProtocol));
    auto &infoMap = GetInfoCtxMap(param.tpProtocol);
    auto lit = infoMap.find(param.locAddr);
    if (lit == infoMap.end()) {
        HCCL_ERROR("[TpManager][%s] failed, tp info is not found, param[%s].", __func__, param.Describe().c_str());
        return HcclResult::HCCL_E_NOT_FOUND;
    }
    auto rit = lit->second.find(param.rmtAddr);
    if (rit == lit->second.end()) {
        HCCL_ERROR("[TpManager][%s] failed, tp info is not found, param[%s].", __func__, param.Describe().c_str());
        return HcclResult::HCCL_E_NOT_FOUND;
    }
    auto qit = rit->second.find(qosKey);
    if (qit == rit->second.end()) {
        HCCL_ERROR("[TpManager][%s] failed, tp info is not found for qosKey[%u], param[%s].", __func__,
            static_cast<unsigned>(qosKey), param.Describe().c_str());
        return HcclResult::HCCL_E_NOT_FOUND;
    }

    if (tpInfo.tpHandle != qit->second.tpInfo.tpHandle) {
        HCCL_ERROR("[TpManager][%s] failed, tp info[%llu] is not expected[%llu].",
            __func__, tpInfo.tpHandle, qit->second.tpInfo.tpHandle);
        return HcclResult::HCCL_E_PARA;
    }

    if (qit->second.useCnt > 1) {
        qit->second.useCnt -= 1;
        return HcclResult::HCCL_SUCCESS;
    }

    rit->second.erase(qit);
    if (rit->second.empty()) {
        const IpAddress locAddr = param.locAddr;
        const IpAddress rmtAddr = param.rmtAddr;
        lit->second.erase(rit);
        ClearLinkAttrCacheLocked(param.tpProtocol, locAddr, rmtAddr); // linkAttr 随 tpInfo 生命周期释放
    }
    if (lit->second.empty()) {
        infoMap.erase(lit);
    }
    return HcclResult::HCCL_SUCCESS;
}

HcclResult TpManager::ReleaseTpAttr(const TpHandle tpHandle, const TpAttrInfo &tpAttrInfo)
{
    std::lock_guard<std::mutex> lock(tpAttrCtxMutex);
    auto attrIter = tpAttrCtxMap.find(tpHandle);
    if (attrIter == tpAttrCtxMap.end()) {
        HCCL_ERROR("[TpManager][%s] failed, tp attr is not found, "
            "tpHandle[0x%llx].", __func__, tpHandle);
        return HcclResult::HCCL_E_NOT_FOUND;
    }

    if (attrIter->second.useCnt > 1) {
        attrIter->second.useCnt -= 1;
        return HcclResult::HCCL_SUCCESS;
    }

    tpAttrCtxMap.erase(attrIter);
    return HcclResult::HCCL_SUCCESS;
}

HcclResult TpManager::GetTpTotalTimeout(const TpAttrInfo &tpAttrInfo, uint32_t &tpTimeOutMs)
{
    uint8_t rawAtGear = tpAttrInfo.tpAttr.at;
    uint8_t rawRetryTimes = tpAttrInfo.tpAttr.retryTimesInit;

    uint8_t finalAtGear = rawAtGear;
    if (rawAtGear < AT_GEAR_MIN || rawAtGear > AT_GEAR_MAX) {
        finalAtGear = AT_GEAR_DEFAULT;
        HCCL_WARNING("%s Invalid at gear[%u], expect [%u, %u], use default gear[%u].",
            __func__, rawAtGear, AT_GEAR_MIN, AT_GEAR_MAX, finalAtGear);
    }

    uint32_t singleAtTimeoutMs = AT_TIMEOUT_MAP[finalAtGear];
    tpTimeOutMs = singleAtTimeoutMs * static_cast<uint32_t>(rawRetryTimes + 1);

    HCCL_INFO("%s TP timeout calc success: raw_at_gear[%u], final_at_gear[%u], "
        "single_timeout[%ums], retry_times[%u], total_timeout[%ums].",
        __func__, rawAtGear, finalAtGear, singleAtTimeoutMs, rawRetryTimes, tpTimeOutMs);

    return HcclResult::HCCL_SUCCESS;
}

uint32_t TpManager::TaHwValueToMs(uint8_t hwValue)
{
    uint8_t gear = hwValue / 8;
    switch (gear) {
        case TA_GEAR_INDEX_0: return TA_TIMEOUT_MS_GEAR0;
        case TA_GEAR_INDEX_1: return TA_TIMEOUT_MS_GEAR1;
        case TA_GEAR_INDEX_2: return TA_TIMEOUT_MS_GEAR2;
        case TA_GEAR_INDEX_3: return TA_TIMEOUT_MS_GEAR3;
        default: return TA_TIMEOUT_MS_GEAR2;
    }
}

uint8_t TpManager::FindMinTaHwValue(uint32_t tpTotalTimeoutMs)
{
    if (tpTotalTimeoutMs < TA_TIMEOUT_MS_GEAR0) {
        return TA_HW_GEAR0_BASE;
    }
    if (tpTotalTimeoutMs < TA_TIMEOUT_MS_GEAR1) {
        return TA_HW_GEAR1_BASE;
    }
    if (tpTotalTimeoutMs < TA_TIMEOUT_MS_GEAR2) {
        return TA_HW_GEAR2_BASE;
    }
    return TA_HW_GEAR3_BASE;
}

uint8_t TpManager::CalcTaTimeout(const TpAttrInfo &tpAttrInfo)
{
    constexpr uint8_t UB_TIMEOUT_DEFAULT = 8;
    uint8_t envValue = static_cast<uint8_t>(EnvConfig::GetInstance().GetRdmaConfig().GetUbTimeOut());
    uint32_t envTimeoutMs = TaHwValueToMs(envValue);
    
    uint32_t tpTimeOutMs = 0;
    (void)GetTpTotalTimeout(tpAttrInfo, tpTimeOutMs);
    
    uint8_t errTimeout = UB_TIMEOUT_DEFAULT;
    if (envTimeoutMs < tpTimeOutMs) {
        errTimeout = FindMinTaHwValue(tpTimeOutMs);
        HCCL_WARNING("[TpManager][%s] Env timeout [%ums] < TP timeout [%ums]. Auto upgrade TA to hw_val[%u] (%ums).",
            __func__, envTimeoutMs, tpTimeOutMs, errTimeout, TaHwValueToMs(errTimeout));
    } else {
        errTimeout = envValue;
        HCCL_INFO("[TpManager][%s] Env timeout [%ums] >= TP timeout [%ums]. Use env gear base hw_val[%u] (%ums).",
            __func__, envTimeoutMs, tpTimeOutMs, envValue, envTimeoutMs);
    }
    
    return errTimeout;
}

HcclResult TpManager::FindAndGetTpInfo(const RaUbGetTpInfoParam &param, TpInfo &tpInfo)
{
    const QosKey qosKey = QosMapKey(param.qos);
    std::lock_guard<std::mutex> lock(GetInfoCtxMutex(param.tpProtocol));
    auto &infoMap = GetInfoCtxMap(param.tpProtocol);
    auto lit = infoMap.find(param.locAddr);
    if (lit == infoMap.end()) {
        return HcclResult::HCCL_E_NOT_FOUND;
    }
    auto rit = lit->second.find(param.rmtAddr);
    if (rit == lit->second.end()) {
        return HcclResult::HCCL_E_NOT_FOUND;
    }
    auto qit = rit->second.find(qosKey);
    if (qit == rit->second.end()) {
        return HcclResult::HCCL_E_NOT_FOUND;
    }
    qit->second.useCnt += 1;
    tpInfo = qit->second.tpInfo;
    return HcclResult::HCCL_SUCCESS;
}

void TpManager::StartGetTpInfoListRequest(const RaUbGetTpInfoParam &param,
    TpManager::RequestCtx &reqCtx, bool isSync) const
{
    reqCtx.phase = ReqPhase::WAIT_LIST;
    reqCtx.bootstrapAttrBitmap = 0U;
    reqCtx.tpListIndex = 0U;
    reqCtx.mappedSl = 0U;
    reqCtx.selectedTpHandle = 0U;
    (void)memset_s(&reqCtx.linkTpAttr, sizeof(reqCtx.linkTpAttr), 0, sizeof(reqCtx.linkTpAttr));

    const RdmaHandle rdmaHandle = ResolveUbRdmaHandle(devPhyId, param.locAddr, isSync);
    // 按 Host / Device 路径，取对的 UB RDMA 上下文 handle
    if (!rdmaHandle) {
        THROW<InternalException>("[TpManager][%s] can not find rdmaHandle, "
            "devPhyId[%u] locAddr[%s].", __func__, devPhyId,
            param.locAddr.Describe().c_str());
    }
    if (isSync) {
        RaUbGetTpInfo(rdmaHandle, param, reqCtx.dataBuffer, reqCtx.tpInfoNum);
        return;
    }
    reqCtx.handle = RaUbGetTpInfoAsync(rdmaHandle, param, reqCtx.dataBuffer, reqCtx.tpInfoNum);
}

HcclResult TpManager::StartBootstrapLinkAttr(const RaUbGetTpInfoParam &param, RequestCtx &reqCtx) const
{
    (void)memset_s(&reqCtx.linkTpAttr, sizeof(reqCtx.linkTpAttr), 0, sizeof(reqCtx.linkTpAttr));
    reqCtx.bootstrapAttrBitmap = BuildBootstrapAttrBitmap(param.tpProtocol);

    const struct HccpTpInfo *list = reinterpret_cast<const struct HccpTpInfo *>(reqCtx.dataBuffer.data());
    const uint64_t firstTpHandle = list[0].tpHandle;
    const RdmaHandle rdmaHandle = ResolveUbRdmaHandle(devPhyId, param.locAddr, false);
    if (!rdmaHandle) {
        THROW<InternalException>("[TpManager][%s] can not find rdmaHandle for RaGetTpAttrAsync, devPhyId[%u].",
            __func__, devPhyId);
    }
    void *raReqHandle = nullptr;
    const s32 ret =
        RaGetTpAttrAsync(rdmaHandle, firstTpHandle, &reqCtx.bootstrapAttrBitmap, &reqCtx.linkTpAttr, &raReqHandle);
    if (ret != 0 || !raReqHandle) {
        THROW<NetworkApiException>(StringFormat("[TpManager][StartBootstrapLinkAttr] RaGetTpAttrAsync failed "
                                                  "ret[%d] raReqHandle[%p] tpHandle[%llu].",
            ret, raReqHandle, firstTpHandle));
    }
    reqCtx.handle = reinterpret_cast<RequestHandle>(raReqHandle);
    reqCtx.phase = ReqPhase::WAIT_ATTR;
    return HcclResult::HCCL_SUCCESS;
}

void TpManager::SyncCommitTpAttr(const RaUbGetTpInfoParam &param, const RequestCtx &reqCtx) const
{
    const RdmaHandle rdmaHandle = ResolveUbRdmaHandle(devPhyId, param.locAddr, true);
    if (!rdmaHandle) {
        THROW<InternalException>("[TpManager][%s] can not find host rdmaHandle, devPhyId[%u].", __func__, devPhyId);
    }

    struct TpAttr tpAttr {};
    uint32_t attrBitmap = 0;
    BuildCommitTpAttr(param, devPhyId, reqCtx.mappedSl, reqCtx.linkTpAttr, reqCtx.tpInfoNum, tpAttr, attrBitmap);
    RaUbSetTpAttr(rdmaHandle, reqCtx.selectedTpHandle, attrBitmap, tpAttr);
}

HcclResult TpManager::StartCommitTpAttr(const RaUbGetTpInfoParam &param, RequestCtx &reqCtx) const
{
    const RdmaHandle rdmaHandle = ResolveUbRdmaHandle(devPhyId, param.locAddr, false);
    if (!rdmaHandle) {
        return HcclResult::HCCL_E_INTERNAL;
    }

    struct TpAttr tpAttr {};
    uint32_t attrBitmap = 0;
    BuildCommitTpAttr(param, devPhyId, reqCtx.mappedSl, reqCtx.linkTpAttr, reqCtx.tpInfoNum, tpAttr, attrBitmap);

    RequestHandle reqHandle = 0;
    HcclResult hret = HcclResult::HCCL_SUCCESS;
    TRY_CATCH_RETURN(
        hret = HrtRaSetTpAttrAsync(rdmaHandle, reqCtx.selectedTpHandle, attrBitmap, tpAttr, reqHandle);
    );
    if (hret != HcclResult::HCCL_SUCCESS) {
        HCCL_ERROR("[TpManager][StartCommitTpAttr] HrtRaSetTpAttrAsync failed hcclRet[%d] tpHandle[%llu].",
            static_cast<int>(hret), reqCtx.selectedTpHandle);
        return hret;
    }
    reqCtx.handle = reqHandle;
    return HcclResult::HCCL_SUCCESS;
}

TpManager::InfoCtxMap& TpManager::GetInfoCtxMap(const TpProtocol tpProtocol)
{
    switch (tpProtocol) {
        case TpProtocol::CTP:
            return ctpInfoMap;
        case TpProtocol::TP:
            return tpInfoMap;
        case TpProtocol::UBOE:
            return uboeInfoMap;
        default:
            return tpInfoMap;
    }
}

TpManager::ReqCtxMap& TpManager::GetReqCtxMap(const TpProtocol tpProtocol)
{
    switch (tpProtocol) {
        case TpProtocol::CTP:
            return ctpReqMap;
        case TpProtocol::TP:
            return tpReqMap;
        case TpProtocol::UBOE:
            return uboeReqMap;
        default:
            return tpReqMap;
    }
}

TpManager::LinkAttrMap& TpManager::GetLinkAttrMap(const TpProtocol tpProtocol)
{
    switch (tpProtocol) {
        case TpProtocol::CTP:
            return ctpLinkAttrMap;
        case TpProtocol::TP:
            return tpLinkAttrMap;
        case TpProtocol::UBOE:
            return uboeLinkAttrMap;
        default:
            return tpLinkAttrMap;
    }
}

std::mutex& TpManager::GetInfoCtxMutex(const TpProtocol tpProtocol)
{
    switch (tpProtocol) {
        case TpProtocol::CTP:
            return ctpInfoMutex;
        case TpProtocol::TP:
            return tpInfoMutex;
        case TpProtocol::UBOE:
            return uboeInfoMutex;
        default:
            return tpInfoMutex;
    }
}

std::mutex& TpManager::GetReqCtxMutex(const TpProtocol tpProtocol)
{
    switch (tpProtocol) {
        case TpProtocol::CTP:
            return ctpReqMutex;
        case TpProtocol::TP:
            return tpReqMutex;
        case TpProtocol::UBOE:
            return uboeReqMutex;
        default:
            return tpReqMutex;
    }
}

} // namespace Hccl

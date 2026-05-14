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

#include <algorithm>
#include <cctype>
#include <string>

#include "exception_util.h"
#include "hccl_common_v2.h"
#include "invalid_params_exception.h"
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

constexpr uint32_t TpCacheQosKey(uint32_t qos) noexcept
{
    return qos & 7U;
}

static uint32_t CalSlAvailableCnt(uint32_t mask)
{
    uint32_t c = 0;
    for (uint32_t i = 0; i < 16U; ++i) {
        if ((mask & (1U << i)) != 0U) {
            ++c;
        }
    }
    return c;
}

static uint32_t SlValueAtRankInMask16(uint32_t mask, uint32_t rank)
{
    uint32_t seen = 0;
    for (uint32_t bit = 0; bit < 16U; ++bit) {
        if ((mask & (1U << bit)) != 0U) {
            if (seen == rank) {
                return bit;
            }
            ++seen;
        }
    }
    return 0;
}

static uint16_t ReadSlAvailableMask16(const struct TpAttr &attr)
{
    return static_cast<uint16_t>(attr.slBitmap);
}

static bool ApplyUbcQosTpSlPolicy(const RaUbGetTpInfoParam &param, uint32_t nTp, uint16_t slMask,
    uint32_t &tpListIndexOut, uint32_t &mappedSlOut)
{
    const uint32_t slRawCnt = CalSlAvailableCnt(slMask);
    uint32_t slAvailableCnt = slRawCnt;
    if (slAvailableCnt == 0U) {
        HCCL_WARNING("[TpManager][ApplyUbcQosTpSlPolicy] slMask empty: nTp[%u] slMask[0x%x] param[%s].", nTp,
            static_cast<unsigned>(slMask), param.Describe().c_str());
        return false;
    }
    if (param.slLevelCount != 0U) {
        slAvailableCnt = std::min(param.slLevelCount, slAvailableCnt);
    }
    if (param.loopFirstTpLowestSl) {
        tpListIndexOut = 0;
        mappedSlOut = SlValueAtRankInMask16(slMask, 0);
        HCCL_INFO(
            "[TpManager][ApplyUbcQosTpSlPolicy] loopFirstTpLowestSl: nTp[%u] slRawCnt[%u] slAvailableCnt[%u(after cap)] "
            "slMask[0x%x] tpListIdx[0] mappedSl[%u] param[%s].",
            nTp, slRawCnt, slAvailableCnt, static_cast<unsigned>(slMask),
            static_cast<unsigned>(mappedSlOut & 0xFU), param.Describe().c_str());
        return true;
    }
    if (nTp == 0U || slAvailableCnt == 0U) {
        HCCL_WARNING("[TpManager][ApplyUbcQosTpSlPolicy] nTp or slAvailableCnt zero: nTp[%u] slAvailableCnt[%u] "
                     "slMask[0x%x] param[%s].",
            nTp, slAvailableCnt, static_cast<unsigned>(slMask), param.Describe().c_str());
        return false;
    }
    const uint32_t k = std::min(nTp, slAvailableCnt);
    if (k == 0U) {
        return false;
    }
    const uint32_t numGroups = std::min(8U, k);
    const uint32_t qos = param.qos & 7U;
    const uint32_t groupIdx =
        (k == 3U) ? (qos < 3U ? 0U : (qos < 5U ? 1U : 2U)) : ((qos * numGroups) / 8U);
    const uint32_t slotIdx = (groupIdx * k) / numGroups;
    if (slotIdx >= k || slotIdx >= nTp) {
        HCCL_WARNING(
            "[TpManager][ApplyUbcQosTpSlPolicy] slotIdx out of range: nTp[%u] slRawCnt[%u] slAvailableCnt[%u] k[%u] "
            "numGroups[%u] qos[%u] groupIdx[%u] slotIdx[%u] slMask[0x%x] param[%s].",
            nTp, slRawCnt, slAvailableCnt, k, numGroups, qos, groupIdx, slotIdx, static_cast<unsigned>(slMask),
            param.Describe().c_str());
        return false;
    }
    const uint32_t slRank = slotIdx;
    if (slRank >= slAvailableCnt) {
        HCCL_WARNING(
            "[TpManager][ApplyUbcQosTpSlPolicy] slRank out of range: nTp[%u] slAvailableCnt[%u] k[%u] slRank[%u] "
            "slMask[0x%x] param[%s].",
            nTp, slAvailableCnt, k, slRank, static_cast<unsigned>(slMask), param.Describe().c_str());
        return false;
    }
    tpListIndexOut = slotIdx;
    mappedSlOut = SlValueAtRankInMask16(slMask, slRank);
    return true;
}

static bool DeviceSupportsRaGetTpAttrAsync(uint32_t phyId)
{
    u32 tpAttrVersion = 0;
    const s32 ret = RaGetInterfaceVersion(phyId, kGetTpAttrOpcode, &tpAttrVersion);
    return (ret == 0 && tpAttrVersion >= kGetTpAttrVersion);
}

/// 设备侧与 `RaGetTpAttrAsync` 一致走 HDC；`RaCtxSetTpAttr` 易触发 Rs 路径 phyId 无效，故统一用异步封装并同步等待完成。
static HcclResult HrtRaSetTpAttrAsyncSync(RdmaHandle ctxHandle, uint64_t tpHandle, uint32_t attrBitmap,
    struct TpAttr &attr, const char *logTag)
{
    RequestHandle reqHandle = 0;
    try {
        const HcclResult hret = HrtRaSetTpAttrAsync(ctxHandle, tpHandle, attrBitmap, attr, reqHandle);
        if (hret != HcclResult::HCCL_SUCCESS) {
            HCCL_ERROR("[TpManager][%s] HrtRaSetTpAttrAsync failed hcclRet[%d] tpHandle[%llu].", logTag,
                static_cast<int>(hret), tpHandle);
        }
        return hret;
    } catch (const NetworkApiException &ex) {
        HCCL_ERROR("[TpManager][%s] HrtRaSetTpAttrAsync exception: %s tpHandle[%llu].", logTag, ex.what(), tpHandle);
        return HcclResult::HCCL_E_NETWORK;
    }
}

static HcclResult CommitMappedSlToTpAttr(const uint32_t devPhyId, const IpAddress &locAddr, uint64_t tpHandle,
    uint32_t mappedSl)
{
    if (tpHandle == 0U) {
        HCCL_ERROR("[TpManager][CommitMappedSlToTpAttr] tpHandle is 0");
        return HcclResult::HCCL_E_INTERNAL;
    }
    const RdmaHandle ctxHandle = RdmaHandleManager::GetInstance().GetByIp(devPhyId, locAddr);
    if (!ctxHandle) {
        HCCL_ERROR("[TpManager][CommitMappedSlToTpAttr] ctxHandle null devPhyId[%u] loc[%s]", devPhyId,
            locAddr.Describe().c_str());
        return HcclResult::HCCL_E_INTERNAL;
    }
    struct TpAttr tpSlAttr {};
    tpSlAttr.sl = static_cast<uint8_t>(mappedSl & 0xFU);
    return HrtRaSetTpAttrAsyncSync(ctxHandle, tpHandle, kTpAttrBitmapSl, tpSlAttr, "CommitMappedSlToTpAttr");
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
    if (ret != 0 || valueLen == 0U) {
        return false;
    }
    if (valueLen > kCfgBufLen) {
        valueLen = kCfgBufLen;
    }
    const std::string cfg(value.data(), valueLen);
    return ParseDscpFromCfgByQos(cfg, qos, dscpOut);
}

static HcclResult CommitUboeDscpToTpAttr(const uint32_t devPhyId, const IpAddress &locAddr, uint64_t tpHandle,
    uint8_t dscp)
{
    if (tpHandle == 0U) {
        return HcclResult::HCCL_E_INTERNAL;
    }
    const RdmaHandle ctxHandle = RdmaHandleManager::GetInstance().GetByIp(devPhyId, locAddr);
    if (!ctxHandle) {
        return HcclResult::HCCL_E_INTERNAL;
    }
    struct TpAttr tpDscpAttr {};
    tpDscpAttr.dscp = static_cast<uint8_t>(dscp & 0x3FU);
    return HrtRaSetTpAttrAsyncSync(ctxHandle, tpHandle, kTpAttrBitmapDscp, tpDscpAttr, "CommitUboeDscpToTpAttr");
}

static HcclResult CommitTpAttrsAfterSlMapping(const uint32_t devPhyId, const RaUbGetTpInfoParam &param,
    const TpAttr &tpAttr, uint64_t tpHandle, uint32_t mappedSl)
{
    // TP / UBOE：将 ApplyUbcQosTpSlPolicy 得到的 mapped SL 写回 TP；CTP 不向 TP 写 SL（与 Next TpMgr 一致）
    if (param.tpProtocol == TpProtocol::TP || param.tpProtocol == TpProtocol::UBOE) {
        CHK_RET(CommitMappedSlToTpAttr(devPhyId, param.locAddr, tpHandle, mappedSl));
    }
    if (param.tpProtocol == TpProtocol::UBOE && tpAttr.dscpConfigMode == 0) {
        const uint8_t dscpBefore = static_cast<uint8_t>(tpAttr.dscp & 0x3FU);
        const uint8_t qos = static_cast<uint8_t>(param.qos & 0xFFU);
        uint8_t dscp = 33U;
        (void)GetDscpByQosFromHccnCfg(devPhyId, qos, dscp);
        CHK_RET(CommitUboeDscpToTpAttr(devPhyId, param.locAddr, tpHandle, dscp));
        HCCL_INFO("[TpManager][%s] UBOE dscp updated: tpHandle[%llu] qos[%u] dscpBefore[%u] dscpAfter[%u].", __func__,
            tpHandle, static_cast<unsigned>(qos), static_cast<unsigned>(dscpBefore), static_cast<unsigned>(dscp));
    }
    return HcclResult::HCCL_SUCCESS;
}

} // namespace

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

void TpManager::SetIsHost()
{
    isHost = true;
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

HcclResult TpManager::RunHandleCompletedGetTpEraseReq(ReqCtxMap &reqCtxMap, const IpAddress &locAddr,
    const IpAddress &rmtAddr, const uint32_t qosKey, RequestCtx &&completedReqCtx,
    std::unique_lock<std::mutex> &reqCtxLock, const RaUbGetTpInfoParam &param, TpInfo &tpInfo, const bool withSlPolicy)
{
    EraseReqCtxAtQos(reqCtxMap, locAddr, rmtAddr, qosKey);
    // 先完成缓存写入再释放 req 互斥量，避免其它线程在同一 qosKey 上再次插入 in-flight RequestCtx 与本次提交竞态
    const HcclResult ret = HandleCompletedRequest(std::move(completedReqCtx), param, tpInfo, withSlPolicy);
    reqCtxLock.unlock();
    return ret;
}

HcclResult TpManager::GetTpInfoOnDeviceWaitListPhase(const RaUbGetTpInfoParam &param, ReqCtxMap &reqCtxMap,
    const IpAddress &locAddr, const IpAddress &rmtAddr, const uint32_t qosKey, RequestCtx &reqCtx,
    std::unique_lock<std::mutex> &reqCtxLock, TpInfo &tpInfo)
{
    if (reqCtx.tpInfoNum == 0U) {
        EraseReqCtxAtQos(reqCtxMap, locAddr, rmtAddr, qosKey);
        reqCtxLock.unlock();
        HCCL_WARNING("[TpManager][%s] failed to find tp info, tpInfoNum is 0, param[%s].", __func__,
            param.Describe().c_str());
        return HcclResult::HCCL_E_NOT_FOUND;
    }
    if (DeviceSupportsRaGetTpAttrAsync(devPhyId)) {
        try {
            StartGetTpAttrForFirstTpDevice(param, reqCtx);
        } catch (...) {
            EraseReqCtxAtQos(reqCtxMap, locAddr, rmtAddr, qosKey);
            throw;
        }
        return HcclResult::HCCL_E_AGAIN;
    }
    return RunHandleCompletedGetTpEraseReq(reqCtxMap, locAddr, rmtAddr, qosKey, std::move(reqCtx), reqCtxLock, param,
        tpInfo, false);
}

HcclResult TpManager::GetTpInfoOnDeviceWaitTpAttrPhase(const RaUbGetTpInfoParam &param, ReqCtxMap &reqCtxMap,
    const IpAddress &locAddr, const IpAddress &rmtAddr, const uint32_t qosKey, RequestCtx &reqCtx,
    std::unique_lock<std::mutex> &reqCtxLock, TpInfo &tpInfo)
{
    return RunHandleCompletedGetTpEraseReq(reqCtxMap, locAddr, rmtAddr, qosKey, std::move(reqCtx), reqCtxLock, param,
        tpInfo, true);
}

HcclResult TpManager::GetTpInfo(const RaUbGetTpInfoParam &param, TpInfo &tpInfo, bool isSync)
{
    const auto &tpProtocol = param.tpProtocol;
    CHK_RET(CheckTpProtocol(tpProtocol));
    if (FindAndGetTpInfo(param, tpInfo)) {
        return HcclResult::HCCL_SUCCESS;
    }

    std::unique_lock<std::mutex> reqCtxLock(GetReqCtxMutex(tpProtocol));

    auto &reqCtxMap = GetReqCtxMap(tpProtocol);
    const auto &locAddr = param.locAddr;
    const auto &rmtAddr = param.rmtAddr;
    const uint32_t qosKey = TpCacheQosKey(param.qos);

    auto &rmtReqMap = reqCtxMap[locAddr];
    auto &qosReqMap = rmtReqMap[rmtAddr];
    auto qosReqIter = qosReqMap.find(qosKey);
    if (qosReqIter == qosReqMap.end()) {
        HCCL_INFO("[TpManager][%s] get new tpInfo, param[%s].", __func__, param.Describe().c_str());

        RequestCtx &reqCtx = qosReqMap[qosKey];
        reqCtx.isSync = isSync;
        StartGetTpInfoListRequest(param, reqCtx, isSync);
        return HcclResult::HCCL_E_AGAIN;
    }

    RequestCtx &reqCtx = qosReqIter->second;

    if (!reqCtx.isSync && reqCtx.handle != 0U && !CheckRequestResult(reqCtx.handle)) {
        return HcclResult::HCCL_E_AGAIN;
    }

    if (!isHost) {
        if (reqCtx.phase == RequestCtx::ReqPhase::WAIT_LIST) {
            return GetTpInfoOnDeviceWaitListPhase(param, reqCtxMap, locAddr, rmtAddr, qosKey, reqCtx, reqCtxLock, tpInfo);
        }
        if (reqCtx.phase == RequestCtx::ReqPhase::WAIT_TP_ATTR) {
            return GetTpInfoOnDeviceWaitTpAttrPhase(param, reqCtxMap, locAddr, rmtAddr, qosKey, reqCtx, reqCtxLock,
                tpInfo);
        }
    }

    return RunHandleCompletedGetTpEraseReq(reqCtxMap, locAddr, rmtAddr, qosKey, std::move(qosReqIter->second),
        reqCtxLock, param, tpInfo, false);
}

HcclResult TpManager::ReleaseTpInfo(const RaUbGetTpInfoParam &param, const TpInfo &tpInfo)
{
    const uint32_t qosKey = TpCacheQosKey(param.qos);
    std::lock_guard<std::mutex> lock(GetInfoCtxMutex(param.tpProtocol));
    auto &infoMap = GetInfoCtxMap(param.tpProtocol);
    auto lit = infoMap.find(param.locAddr);
    if (lit == infoMap.end()) {
        HCCL_ERROR("[TpManager][%s] failed, tp info is not found, "
            "param[%s].", __func__, param.Describe().c_str());
        return HcclResult::HCCL_E_NOT_FOUND;
    }
    auto rit = lit->second.find(param.rmtAddr);
    if (rit == lit->second.end()) {
        HCCL_ERROR("[TpManager][%s] failed, tp info is not found, "
            "param[%s].", __func__, param.Describe().c_str());
        return HcclResult::HCCL_E_NOT_FOUND;
    }
    auto qit = rit->second.find(qosKey);
    if (qit == rit->second.end()) {
        HCCL_ERROR("[TpManager][%s] failed, tp info is not found, "
            "param[%s].", __func__, param.Describe().c_str());
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
        lit->second.erase(rit);
    }
    if (lit->second.empty()) {
        infoMap.erase(lit);
    }
    // 暂时不能主动释放tp handle，跟随unimport jetty释放
    return HcclResult::HCCL_SUCCESS;
}

void TpManager::EraseReqCtxAtQos(ReqCtxMap &reqCtxMap, const IpAddress &loc, const IpAddress &rmt, uint32_t qosKey)
{
    auto lit = reqCtxMap.find(loc);
    if (lit == reqCtxMap.end()) {
        return;
    }
    auto rit = lit->second.find(rmt);
    if (rit == lit->second.end()) {
        return;
    }
    auto qit = rit->second.find(qosKey);
    if (qit == rit->second.end()) {
        return;
    }
    rit->second.erase(qit);
    if (rit->second.empty()) {
        lit->second.erase(rit);
    }
    if (lit->second.empty()) {
        reqCtxMap.erase(lit);
    }
}

bool TpManager::FindAndGetTpInfo(const RaUbGetTpInfoParam &param, TpInfo &tpInfo)
{
    const uint32_t qosKey = TpCacheQosKey(param.qos);
    std::lock_guard<std::mutex> lock(GetInfoCtxMutex(param.tpProtocol));
    auto &infoMap = GetInfoCtxMap(param.tpProtocol);
    auto lit = infoMap.find(param.locAddr);
    if (lit == infoMap.end()) {
        return false;
    }
    auto rit = lit->second.find(param.rmtAddr);
    if (rit == lit->second.end()) {
        return false;
    }
    auto qit = rit->second.find(qosKey);
    if (qit == rit->second.end()) {
        return false;
    }
    qit->second.useCnt += 1;
    tpInfo = qit->second.tpInfo;
    return true;
}

void TpManager::StartGetTpInfoListRequest(const RaUbGetTpInfoParam &param,
    TpManager::RequestCtx &reqCtx, bool isSync) const
{
    reqCtx.phase = RequestCtx::ReqPhase::WAIT_LIST;
    (void)memset_s(&reqCtx.tpAttr, sizeof(reqCtx.tpAttr), 0, sizeof(reqCtx.tpAttr));
    reqCtx.tpAttrBitmap = 0;

    Hccl::IpAddress localIp = param.locAddr;
    RdmaHandle rdmaHandle = isSync
        ? RdmaHandleManager::GetInstance().GetByAddr(devPhyId, LinkProtoType::UB, localIp,
              Hccl::PortDeploymentType::HOST_NET)
        : RdmaHandleManager::GetInstance().GetByIp(devPhyId, param.locAddr);
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

void TpManager::StartGetTpAttrForFirstTpDevice(const RaUbGetTpInfoParam &param, RequestCtx &reqCtx)
{
    (void)memset_s(&reqCtx.tpAttr, sizeof(reqCtx.tpAttr), 0, sizeof(reqCtx.tpAttr));
    reqCtx.tpAttrBitmap = (1U << kTpAttrSlAvailableBit) | kTpAttrBitmapSl;
    if (param.tpProtocol == TpProtocol::UBOE) {
        reqCtx.tpAttrBitmap |= kTpAttrBitmapDscp | (1U << kTpAttrDscpConfigModeBit);
    }
    const struct HccpTpInfo *list = reinterpret_cast<const struct HccpTpInfo *>(reqCtx.dataBuffer.data());
    const uint64_t firstTpHandle = list[0].tpHandle;
    const RdmaHandle rdmaHandle = RdmaHandleManager::GetInstance().GetByIp(devPhyId, param.locAddr);
    if (!rdmaHandle) {
        THROW<InternalException>("[TpManager][%s] can not find rdmaHandle for RaGetTpAttrAsync, devPhyId[%u].",
            __func__, devPhyId);
    }
    void *raReqHandle = nullptr;
    const s32 ret =
        RaGetTpAttrAsync(rdmaHandle, firstTpHandle, &reqCtx.tpAttrBitmap, &reqCtx.tpAttr, &raReqHandle);
    if (ret != 0 || !raReqHandle) {
        THROW<NetworkApiException>(StringFormat("[TpManager][StartGetTpAttrForFirstTpDevice] RaGetTpAttrAsync failed "
                                                  "ret[%d] raReqHandle[%p] tpHandle[%llu].",
            ret, raReqHandle, firstTpHandle));
    }
    reqCtx.handle = reinterpret_cast<RequestHandle>(raReqHandle);
    reqCtx.phase = RequestCtx::ReqPhase::WAIT_TP_ATTR;
}

inline TpInfo ParseTpInfo(const struct HccpTpInfo *infoPtr)
{
    TpInfo tpInfo;
    tpInfo.tpHandle = infoPtr->tpHandle;
    return tpInfo;
}

HcclResult TpManager::MapTpInfoFromTpAttr(const RaUbGetTpInfoParam &param, const RequestCtx &reqCtx, TpInfo &outTpInfo)
{
    const uint32_t tpInfoNum = reqCtx.tpInfoNum;
    const struct HccpTpInfo *baseInfoPtr =
        reinterpret_cast<const struct HccpTpInfo *>(reqCtx.dataBuffer.data());
    const uint16_t slMask = ReadSlAvailableMask16(reqCtx.tpAttr);
    const uint32_t slAvailableCnt = CalSlAvailableCnt(slMask);
    HCCL_INFO("[TpManager][%s] after get_tp_attr: slMask[0x%04x] slAvailableCnt[%u] slBitmap[0x%x] dscp[%u] "
              "dscpConfigMode[%u] tpAttrBitmap[0x%x] param[%s].",
        __func__, static_cast<unsigned>(slMask), slAvailableCnt,
        static_cast<unsigned>(reqCtx.tpAttr.slBitmap), static_cast<unsigned>(reqCtx.tpAttr.dscp & 0x3FU),
        static_cast<unsigned>(reqCtx.tpAttr.dscpConfigMode & 1U), reqCtx.tpAttrBitmap, param.Describe().c_str());
    if (slAvailableCnt == 0U) {
        HCCL_ERROR("[TpManager][%s] sl_available mask empty after get_tp_attr, param[%s].", __func__,
            param.Describe().c_str());
        return HcclResult::HCCL_E_INTERNAL;
    }
    uint32_t tpListIndex = 0;
    uint32_t mappedSl = 0;
    if (!ApplyUbcQosTpSlPolicy(param, tpInfoNum, slMask, tpListIndex, mappedSl)) {
        HCCL_ERROR("[TpManager][%s] ApplyUbcQosTpSlPolicy failed, param[%s] nTp[%u] slAvailableCnt[%u] mask[%u].",
            __func__, param.Describe().c_str(), tpInfoNum, slAvailableCnt, static_cast<unsigned>(slMask));
        return HcclResult::HCCL_E_INTERNAL;
    }
    if (tpListIndex >= tpInfoNum) {
        return HcclResult::HCCL_E_INTERNAL;
    }

    outTpInfo = ParseTpInfo(baseInfoPtr + tpListIndex);
    outTpInfo.mappedJettyPriority = mappedSl & 0xFU;
    outTpInfo.hasMappedJettyPriority = true;

    CHK_RET(CommitTpAttrsAfterSlMapping(devPhyId, param, reqCtx.tpAttr, outTpInfo.tpHandle, mappedSl));

    HCCL_INFO("[TpManager][%s] tp qos mapping ok: tpInfoNum[%u] tpHandle[%llu] tpListIndex[%u] "
              "mappedJettyPriority[%u] qos[%u] param[%s].",
        __func__, tpInfoNum, outTpInfo.tpHandle, tpListIndex, outTpInfo.mappedJettyPriority,
        param.qos & 0xFFU, param.Describe().c_str());
    return HcclResult::HCCL_SUCCESS;
}

HcclResult TpManager::HandleCompletedRequest(const TpManager::RequestCtx reqCtx, const RaUbGetTpInfoParam &param,
    TpInfo &tpInfo, bool withSlPolicy)
{
    const uint32_t tpInfoNum = reqCtx.tpInfoNum;
    if (tpInfoNum == 0) {
        HCCL_WARNING("[TpManager][%s] failed to find tp info, tpInfoNum is 0, "
            "param[%s].", __func__, param.Describe().c_str());
        return HcclResult::HCCL_E_NOT_FOUND;
    }

    HCCL_INFO("[TpManager][%s] RaGetTpInfoList completed: tpInfoNum[%u] withSlPolicy[%d] devPhyId[%u] param[%s].",
        __func__, tpInfoNum, static_cast<int>(withSlPolicy), devPhyId, param.Describe().c_str());

    TpInfo tmpTpInfo{};
    if (withSlPolicy) {
        CHK_RET(MapTpInfoFromTpAttr(param, reqCtx, tmpTpInfo));
    } else {
        const struct HccpTpInfo *baseInfoPtr =
            reinterpret_cast<const struct HccpTpInfo *>(reqCtx.dataBuffer.data());
        tmpTpInfo = ParseTpInfo(baseInfoPtr);
        tmpTpInfo.hasMappedJettyPriority = false;
    }

    const uint32_t qosKey = TpCacheQosKey(param.qos);

    std::lock_guard<std::mutex> lock(GetInfoCtxMutex(param.tpProtocol));
    auto &infoMap = GetInfoCtxMap(param.tpProtocol);
    auto &qosMap = infoMap[param.locAddr][param.rmtAddr];
    auto qIt = qosMap.find(qosKey);
    if (qIt != qosMap.end() && qIt->second.tpInfo.tpHandle == tmpTpInfo.tpHandle) {
        qIt->second.useCnt += 1;
        tpInfo = qIt->second.tpInfo;
    } else {
        qosMap[qosKey] = TpInfoCtx{tmpTpInfo, 1};
        tpInfo = qosMap[qosKey].tpInfo;
    }
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

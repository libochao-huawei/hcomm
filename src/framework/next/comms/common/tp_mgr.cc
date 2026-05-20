/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "tp_mgr.h"

#include <algorithm>
#include <cctype>
#include <vector>

#include "hccp_ctx.h"
#include "hccp_async_ctx.h"
#include "hccp.h"

#include "hccl_common.h"
#include "exception_handler.h"
#include "network_api_exception.h"
#include "orion_adapter_hccp.h"
#include "rdma_handle_manager.h"
#include "orion_adapter_hccp.h"
#include "env_config/env_config.h"

namespace hcomm {

namespace {
constexpr uint32_t kTpAttrSlAvailableBit = 17U;
static constexpr uint32_t kTpAttrBitmapSl = (1U << 10U);
static constexpr uint32_t kTpAttrBitmapDscp = (1U << 8U);
static constexpr uint32_t kTpAttrDscpConfigModeBit = 18U;

static constexpr QosKey QosMapKey(uint32_t qos) noexcept
{
    return static_cast<QosKey>(qos & 0xFFU);
}

struct TpInfoAddrKey {
    Hccl::IpAddress locAddr{};
    Hccl::IpAddress rmtAddr{};
    QosKey qosKey{0};
};

static HcclResult ResolveTpInfoAddrKey(const GetTpInfoParam &param, TpInfoAddrKey &out)
{
    CHK_RET(CommAddrToIpAddress(param.locAddr, out.locAddr));
    CHK_RET(CommAddrToIpAddress(param.rmtAddr, out.rmtAddr));
    out.qosKey = QosMapKey(param.qos);
    return HcclResult::HCCL_SUCCESS;
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

constexpr uint16_t kUboeSl789Mask = static_cast<uint16_t>((1U << 7U) | (1U << 8U) | (1U << 9U));
constexpr uint32_t kUboeEightTpPolicyCount = 8U;

static bool ValidateUboeSl789Mask(uint16_t slMask)
{
    if (slMask == kUboeSl789Mask) {
        return true;
    }
    if (CalSlAvailableCnt(slMask) != 3U) {
        return false;
    }
    return SlValueAtRankInMask16(slMask, 0U) == 7U && SlValueAtRankInMask16(slMask, 1U) == 8U &&
           SlValueAtRankInMask16(slMask, 2U) == 9U;
}

static bool ApplyUbcQosTpSlPolicy(const GetTpInfoParam &param, uint32_t nTp, uint16_t slMask,
    uint32_t &tpListIndexOut, uint32_t &mappedSlOut);

/// UBOE 专用：8 个 TP + SL 7/8/9，hcclQos 0–7 与 TP 一一对应（3:2:3 分段，低 qos 选高 index / 高 SL）。
static bool TryApplyUboeEightTpQosPolicy(const GetTpInfoParam &param, uint32_t nTp, uint16_t slMask,
    uint32_t &tpListIndexOut, uint32_t &mappedSlOut)
{
    if (param.tpProtocol != TpProtocol::UBOE || param.loopFirstTpLowestSl || param.slLevelCount != 0U) {
        return false;
    }
    if (nTp != kUboeEightTpPolicyCount || !ValidateUboeSl789Mask(slMask)) {
        return false;
    }
    const uint32_t qos = param.qos & 7U;
    static constexpr uint8_t kUboeEightTpIndexByQos[8] = {7U, 6U, 5U, 4U, 3U, 2U, 1U, 0U};
    static constexpr uint8_t kUboeEightTpSlByQos[8] = {9U, 9U, 9U, 8U, 8U, 7U, 7U, 7U};
    tpListIndexOut = kUboeEightTpIndexByQos[qos];
    mappedSlOut = kUboeEightTpSlByQos[qos];
    HCCL_INFO("[TpMgr][TryApplyUboeEightTpQosPolicy] qos[%u] tpListIndex[%u] mappedSl[%u] param[%s].", qos,
        tpListIndexOut, mappedSlOut, param.Describe().c_str());
    return true;
}

static bool ApplyTpQosSlPolicy(const GetTpInfoParam &param, uint32_t nTp, uint16_t slMask,
    uint32_t &tpListIndexOut, uint32_t &mappedSlOut)
{
    if (TryApplyUboeEightTpQosPolicy(param, nTp, slMask, tpListIndexOut, mappedSlOut)) {
        return true;
    }
    return ApplyUbcQosTpSlPolicy(param, nTp, slMask, tpListIndexOut, mappedSlOut);
}

static bool ApplyUbcQosTpSlPolicy(const GetTpInfoParam &param, uint32_t nTp, uint16_t slMask,
    uint32_t &tpListIndexOut, uint32_t &mappedSlOut)
{
    uint32_t slAvailableCnt = CalSlAvailableCnt(slMask); // slMask 里为 1 的 bit 个数 = 可用 SL 档位数
    if (slAvailableCnt == 0U) {
        return false;
    }
    if (param.slLevelCount != 0U) {
        slAvailableCnt = std::min(param.slLevelCount, slAvailableCnt);
    }
    if (param.loopFirstTpLowestSl) {
        tpListIndexOut = 0;
        mappedSlOut = SlValueAtRankInMask16(slMask, 0);
        return true;
    }
    if (nTp == 0U || slAvailableCnt == 0U) {
        return false;
    }
    const uint32_t k = std::min(nTp, slAvailableCnt);
    if (k == 0U) {
        return false;
    }
    const uint32_t numGroups = std::min(8U, k);
    const uint32_t qos = param.qos & 7U;
    // K=3：8 档 QoS 按 3:2:3 → [0–2]/[3–4]/[5–7]；其余 k 仍用均匀分段
    const uint32_t groupIdx =
        (k == 3U) ? (qos < 3U ? 0U : (qos < 5U ? 1U : 2U)) : ((qos * numGroups) / 8U);
    const uint32_t slotIdx = (groupIdx * k) / numGroups;
    if (slotIdx >= k || slotIdx >= nTp) {
        return false;
    }
    // hcclQos 越大优先级越高；UB SL 数值越小优先级越高，对档位取反
    const uint32_t slRank = (slAvailableCnt - 1U) - slotIdx;
    if (slRank >= slAvailableCnt) {
        return false;
    }
    tpListIndexOut = (k - 1U) - slotIdx;
    mappedSlOut = SlValueAtRankInMask16(slMask, slRank);
    return true;
}

/// 与 `ApplyUbcQosTpSlPolicy` 分组一致：取该组最小 hcclQos，供共 TP 时统一查 DSCP。
static uint32_t ResolveUbcGroupFirstHcclQos(uint32_t qos, uint32_t nTp, uint32_t slAvailableCnt)
{
    const uint32_t q = qos & 7U;
    if (nTp == 0U || slAvailableCnt == 0U) {
        return q;
    }
    const uint32_t k = std::min(nTp, slAvailableCnt);
    const uint32_t numGroups = std::min(8U, k);
    const uint32_t groupIdx =
        (k == 3U) ? (q < 3U ? 0U : (q < 5U ? 1U : 2U)) : ((q * numGroups) / 8U);
    if (k == 3U) {
        static constexpr uint8_t kUboeGroupFirstQos[3] = {0U, 3U, 5U};
        return (groupIdx < 3U) ? static_cast<uint32_t>(kUboeGroupFirstQos[groupIdx]) : 0U;
    }
    for (uint32_t candidate = 0U; candidate <= 7U; ++candidate) {
        if (((candidate * numGroups) / 8U) == groupIdx) {
            return candidate;
        }
    }
    return q;
}

/// 8-TP 新策略用请求 qos；旧策略多 QoS 共 TP 时用组内首个 qos 查 DSCP，避免后写覆盖。
static uint8_t ResolveUboeDscpLookupQos(const GetTpInfoParam &param, uint32_t nTp, uint16_t slMask)
{
    const uint8_t requestQos = static_cast<uint8_t>(param.qos & 0xFFU);
    uint32_t dummyTpIdx = 0U;
    uint32_t dummySl = 0U;
    if (TryApplyUboeEightTpQosPolicy(param, nTp, slMask, dummyTpIdx, dummySl)) {
        return requestQos;
    }
    if (param.loopFirstTpLowestSl) {
        return 0U;
    }
    uint32_t slAvailableCnt = CalSlAvailableCnt(slMask);
    if (param.slLevelCount != 0U) {
        slAvailableCnt = std::min(param.slLevelCount, slAvailableCnt);
    }
    return static_cast<uint8_t>(ResolveUbcGroupFirstHcclQos(param.qos, nTp, slAvailableCnt));
}

/// 与 Legacy `TpManager` 一致：`RaGetTpAttrAsync` 走 HDC，写回 SL/DSCP 用 `HrtRaSetTpAttrAsync`（同步等到完成），避免
/// `RaCtxSetTpAttr` 经 Rs 路径在设备上出现 phyId 无效等问题。
static HcclResult HrtRaSetTpAttrAsyncSync(const Hccl::RdmaHandle rdmaHandle, uint64_t tpHandle, uint32_t attrBitmap,
    struct TpAttr &attr, const char *logTag)
{
    Hccl::RequestHandle reqHandle = 0;
    try {
        const HcclResult hret =
            Hccl::HrtRaSetTpAttrAsync(rdmaHandle, tpHandle, attrBitmap, attr, reqHandle);
        if (hret != HcclResult::HCCL_SUCCESS) {
            HCCL_ERROR("[TpMgr][%s] HrtRaSetTpAttrAsync failed hcclRet[%d] tpHandle[%llu].", logTag,
                static_cast<int>(hret), tpHandle);
        }
        return hret;
    } catch (const Hccl::NetworkApiException &ex) {
        HCCL_ERROR("[TpMgr][%s] HrtRaSetTpAttrAsync exception: %s tpHandle[%llu].", logTag, ex.what(), tpHandle);
        return HcclResult::HCCL_E_NETWORK;
    }
}

static HcclResult CommitMappedSlToTpAttr(const uint32_t devPhyId, const CommAddr &locCommAddr, uint64_t tpHandle,
    uint32_t mappedSl)
{
    if (tpHandle == 0U) {
        HCCL_ERROR("[TpMgr][CommitMappedSlToTpAttr] tpHandle is 0");
        return HcclResult::HCCL_E_INTERNAL;
    }
    Hccl::IpAddress locAddr{};
    CHK_RET(CommAddrToIpAddress(locCommAddr, locAddr));
    const Hccl::RdmaHandle rdmaHandle = Hccl::RdmaHandleManager::GetInstance().GetByIp(devPhyId, locAddr);
    CHK_PTR_NULL(rdmaHandle);

    struct TpAttr tpSlAttr {};
    tpSlAttr.sl = static_cast<uint8_t>(mappedSl & 0xFU);
    const HcclResult hret =
        HrtRaSetTpAttrAsyncSync(rdmaHandle, tpHandle, kTpAttrBitmapSl, tpSlAttr, "CommitMappedSlToTpAttr");
    if (hret == HcclResult::HCCL_SUCCESS) {
        HCCL_INFO("[TpMgr][CommitMappedSlToTpAttr] ok tpHandle[%llu] sl[%u].", tpHandle,
            static_cast<unsigned>(mappedSl & 0xFU));
    }
    return hret;
}

static HcclResult CommitUboeDscpToTpAttr(const uint32_t devPhyId, const CommAddr &locCommAddr, uint64_t tpHandle,
    uint8_t dscp)
{
    if (tpHandle == 0U) {
        HCCL_ERROR("[TpMgr][CommitUboeDscpToTpAttr] tpHandle is 0");
        return HcclResult::HCCL_E_INTERNAL;
    }
    Hccl::IpAddress locAddr{};
    CHK_RET(CommAddrToIpAddress(locCommAddr, locAddr));
    const Hccl::RdmaHandle rdmaHandle = Hccl::RdmaHandleManager::GetInstance().GetByIp(devPhyId, locAddr);
    CHK_PTR_NULL(rdmaHandle);

    struct TpAttr tpDscpAttr {};
    tpDscpAttr.dscp = static_cast<uint8_t>(dscp & 0x3FU);
    const HcclResult hret =
        HrtRaSetTpAttrAsyncSync(rdmaHandle, tpHandle, kTpAttrBitmapDscp, tpDscpAttr, "CommitUboeDscpToTpAttr");
    if (hret == HcclResult::HCCL_SUCCESS) {
        HCCL_INFO("[TpMgr][CommitUboeDscpToTpAttr] ok tpHandle[%llu] dscp[%u].", tpHandle,
            static_cast<unsigned>(tpDscpAttr.dscp));
    }
    return hret;
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
    HCCL_INFO("[TpMgr][%s] RaGetHccnCfg ret[%d] phyId[%u] valueLen[%u] qos_dscp[%s].", __func__, ret, devPhyId,
        valueLen, value.data());
    if (ret != 0 || valueLen == 0U) {
        return false;
    }
    if (valueLen > kCfgBufLen) {
        valueLen = kCfgBufLen;
    }
    std::string cfg(value.data(), valueLen);
    return ParseDscpFromCfgByQos(cfg, qos, dscpOut);
}

} // namespace

TpMgr &TpMgr::GetInstance(const uint32_t devicePhyId)
{
    static TpMgr tpMgr[MAX_MODULE_DEVICE_NUM + 1];

    uint32_t devPhyId = devicePhyId;
    if (devPhyId >= MAX_MODULE_DEVICE_NUM) {
        HCCL_WARNING("[TpMgr][%s] use the backup device, devPhyId[%u] should be "
            "less than %u.",
            __func__, devPhyId, MAX_MODULE_DEVICE_NUM);
        devPhyId = MAX_MODULE_DEVICE_NUM;
    }

    tpMgr[devPhyId].devPhyId_ = devPhyId;

    return tpMgr[devPhyId];
}

static HcclResult CheckRequestResult(RequestHandle &reqHandle)
{
    if (reqHandle == 0) {
        return HcclResult::HCCL_SUCCESS;
    }

    RequestResult result = HccpGetAsyncReqResult(reqHandle);
    if (result == RequestResult::NOT_COMPLETED) {
        return HcclResult::HCCL_E_AGAIN;
    }

    if (result != RequestResult::COMPLETED) {
        HCCL_ERROR("[TpMgr][%s] failed, result[%s] is unexpected.", __func__, result.Describe().c_str());
        return HcclResult::HCCL_E_NETWORK;
    }

    return HcclResult::HCCL_SUCCESS;
}

HcclResult CheckTpProtocol(const TpProtocol tpProtocol)
{
    if (tpProtocol != TpProtocol::CTP && tpProtocol != TpProtocol::RTP && tpProtocol != TpProtocol::UBOE) {
        HCCL_ERROR("[TpMgr][%s] failed, tpProtocol[%d] is not supported.", __func__, tpProtocol);
        return HcclResult::HCCL_E_NOT_SUPPORT;
    }

    return HcclResult::HCCL_SUCCESS;
}

HcclResult TpMgr::LookupInfoCtxEntry(InfoCtxMap &infoMap, const Hccl::IpAddress &locAddr,
    const Hccl::IpAddress &rmtAddr, const QosKey qosKey, InfoCtxMap::iterator &lit, InfoRmtMap::iterator &rit,
    InfoQosMap::iterator &qosIt) const
{
    lit = infoMap.find(locAddr);
    if (lit == infoMap.end()) {
        return HcclResult::HCCL_E_NOT_FOUND;
    }
    rit = lit->second.find(rmtAddr);
    if (rit == lit->second.end()) {
        return HcclResult::HCCL_E_NOT_FOUND;
    }
    qosIt = rit->second.find(qosKey);
    if (qosIt == rit->second.end()) {
        return HcclResult::HCCL_E_NOT_FOUND;
    }
    return HcclResult::HCCL_SUCCESS;
}

HcclResult TpMgr::FindAndGetTpInfo(const GetTpInfoParam &param, TpInfo &tpInfo)
{
    TpInfoAddrKey key{};
    CHK_RET(ResolveTpInfoAddrKey(param, key));
    std::lock_guard<std::mutex> lock(GetInfoCtxMutex(param.tpProtocol));
    auto &infoMap = GetInfoCtxMap(param.tpProtocol);
    InfoCtxMap::iterator lit;
    InfoRmtMap::iterator rit;
    InfoQosMap::iterator qosIt;
    const auto lookupRet = LookupInfoCtxEntry(infoMap, key.locAddr, key.rmtAddr, key.qosKey, lit, rit, qosIt);
    if (lookupRet != HcclResult::HCCL_SUCCESS) {
        return lookupRet;
    }
    qosIt->second.useCnt += 1;
    tpInfo = qosIt->second.tpInfo;
    return HcclResult::HCCL_SUCCESS;
}

HcclResult TpMgr::BeginGetTpInfoListRequest(const GetTpInfoParam &param, ReqQosMap &qosMap, const QosKey qosKey)
{
    RequestCtx &reqCtx = qosMap[qosKey];
    CHK_RET(StartGetTpInfoListRequest(param, reqCtx));
    HCCL_INFO("[TpMgr][GetTpInfo] RaGetTpInfoListAsync submitted, devPhyId[%u] reqHandle[%llu] phase[WAIT_LIST] "
              "param[%s].",
        devPhyId_, static_cast<unsigned long long>(reqCtx.handle), param.Describe().c_str());
    return HcclResult::HCCL_E_AGAIN;
}

HcclResult TpMgr::AdvanceGetTpInfoWaitList(const GetTpInfoParam &param, RequestCtx &reqCtx, ReqQosMap &qosMap,
    const ReqQosMap::iterator it, std::unique_lock<std::mutex> &reqCtxLock)
{
    if (reqCtx.tpInfoNum == 0U) {
        qosMap.erase(it);
        reqCtxLock.unlock();
        HCCL_WARNING("[TpMgr][%s] failed to find tp info, tpInfoNum is 0, param[%s].", __func__, param.Describe().c_str());
        return HcclResult::HCCL_E_NOT_FOUND;
    }
    const struct HccpTpInfo *list = reinterpret_cast<const struct HccpTpInfo *>(reqCtx.dataBuffer.data());
    HCCL_INFO("[TpMgr][GetTpInfo] list stage ok, devPhyId[%u] tpInfoNum[%u] firstTpHandle[%llu] param[%s].",
        devPhyId_, reqCtx.tpInfoNum, static_cast<unsigned long long>(list[0].tpHandle), param.Describe().c_str());
    try {
        CHK_RET(StartGetTpAttrForFirstTp(param, reqCtx));
    } catch (...) {
        qosMap.erase(it);
        throw;
    }
    HCCL_INFO("[TpMgr][GetTpInfo] RaGetTpAttrAsync submitted, devPhyId[%u] reqHandle[%llu] phase[WAIT_TP_ATTR] "
              "tpAttrBitmap[0x%x] param[%s].",
        devPhyId_, static_cast<unsigned long long>(reqCtx.handle), reqCtx.tpAttrBitmap, param.Describe().c_str());
    return HcclResult::HCCL_E_AGAIN;
}

HcclResult TpMgr::PollGetTpInfoReqCtx(std::unique_lock<std::mutex> &reqCtxLock, const GetTpInfoParam &param,
    TpInfo &tpInfo)
{
    auto &reqCtxMap = GetReqCtxMap(param.tpProtocol);
    TpInfoAddrKey key{};
    CHK_RET(ResolveTpInfoAddrKey(param, key));
    auto &qosMap = reqCtxMap[key.locAddr][key.rmtAddr];
    auto it = qosMap.find(key.qosKey);
    if (it == qosMap.end()) {
        return BeginGetTpInfoListRequest(param, qosMap, key.qosKey);
    }

    RequestCtx &reqCtx = it->second;
    const auto ret = CheckRequestResult(reqCtx.handle);
    if (ret == HcclResult::HCCL_E_AGAIN) {
        return ret;
    }
    CHK_RET(ret);

    if (reqCtx.phase == ReqPhase::WAIT_LIST) {
        return AdvanceGetTpInfoWaitList(param, reqCtx, qosMap, it, reqCtxLock);
    }

    // 先 move 出槽位再 erase，避免 erase 析构槽内对象后再 move（UB / double free）
    RequestCtx completedReqCtx = std::move(it->second);
    qosMap.erase(it);
    const HcclResult ret = HandleCompletedRequest(std::move(completedReqCtx), param, tpInfo);
    reqCtxLock.unlock();
    return ret;
}

HcclResult TpMgr::GetTpInfo(const GetTpInfoParam &param, TpInfo &tpInfo)
{
    CHK_RET(CheckTpProtocol(param.tpProtocol));
    if (FindAndGetTpInfo(param, tpInfo) == HcclResult::HCCL_SUCCESS) {
        return HcclResult::HCCL_SUCCESS;
    }

    std::unique_lock<std::mutex> reqCtxLock(GetReqCtxMutex(param.tpProtocol));
    return PollGetTpInfoReqCtx(reqCtxLock, param, tpInfo);
}

HcclResult TpMgr::ReleaseTpInfo(const GetTpInfoParam &param, const TpInfo &tpInfo)
{
    TpInfoAddrKey key{};
    CHK_RET(ResolveTpInfoAddrKey(param, key));
    std::lock_guard<std::mutex> lock(GetInfoCtxMutex(param.tpProtocol));
    auto &infoMap = GetInfoCtxMap(param.tpProtocol);
    InfoCtxMap::iterator lit;
    InfoRmtMap::iterator rmtIt;
    InfoQosMap::iterator qosIt;
    const auto lookupRet = LookupInfoCtxEntry(infoMap, key.locAddr, key.rmtAddr, key.qosKey, lit, rmtIt, qosIt);
    if (lookupRet != HcclResult::HCCL_SUCCESS) {
        if (lit == infoMap.end()) {
            HCCL_ERROR("[TpMgr][%s] failed, tp info is not found, param[%s].", __func__, param.Describe().c_str());
        } else if (rmtIt == lit->second.end()) {
            HCCL_ERROR("[TpMgr][%s] failed, tp info is not found, param[%s].", __func__, param.Describe().c_str());
        } else {
            HCCL_ERROR("[TpMgr][%s] failed, tp info is not found for qosKey[%u], param[%s].", __func__,
                static_cast<unsigned>(key.qosKey), param.Describe().c_str());
        }
        return HcclResult::HCCL_E_NOT_FOUND;
    }

    if (tpInfo.tpHandle != qosIt->second.tpInfo.tpHandle) {
        HCCL_ERROR("[TpMgr][%s] failed, tp info[%llu] is not expected[%llu].", __func__, tpInfo.tpHandle,
            qosIt->second.tpInfo.tpHandle);
        return HcclResult::HCCL_E_PARA;
    }

    if (qosIt->second.useCnt > 1) {
        qosIt->second.useCnt -= 1;
        return HcclResult::HCCL_SUCCESS;
    }

    rmtIt->second.erase(qosIt);
    if (rmtIt->second.empty()) {
        lit->second.erase(rmtIt);
    }
    if (lit->second.empty()) {
        infoMap.erase(lit);
    }
    return HcclResult::HCCL_SUCCESS;
}

static HcclResult GetTpInfoListAsync(const CtxHandle ctxHandle, const GetTpInfoParam &param,
    std::vector<char> &out, uint32_t &num, RequestHandle &reqHandle)
{
    Hccl::IpAddress locAddr{};
    Hccl::IpAddress rmtAddr{};
    CHK_RET(CommAddrToIpAddress(param.locAddr, locAddr));
    CHK_RET(CommAddrToIpAddress(param.rmtAddr, rmtAddr));
    const auto &tpProtocol = param.tpProtocol;

    struct GetTpCfg cfg {};
    cfg.flag.bs.rtp = tpProtocol == TpProtocol::RTP ? 1 : 0;
    cfg.flag.bs.ctp = tpProtocol == TpProtocol::CTP ? 1 : 0;
    cfg.flag.bs.uboe = tpProtocol == TpProtocol::UBOE ? 1 : 0;
    cfg.transMode = TransportModeT::CONN_RM;
    CHK_RET(IpAddressToHccpEid(locAddr, cfg.localEid));
    HCCL_INFO("RaUbGetTpInfoAsync cfg.local_eid[subnetPrefix[%016llx], interfaceId[%016llx]]",
        cfg.localEid.in6.subnetPrefix, cfg.localEid.in6.interfaceId);
    CHK_RET(IpAddressToHccpEid(rmtAddr, cfg.peerEid));
    HCCL_INFO("RaUbGetTpInfoAsync cfg.peer_eid[subnetPrefix[%016llx], interfaceId[%016llx]]",
        cfg.peerEid.in6.subnetPrefix, cfg.peerEid.in6.interfaceId);

    // buffer 须至少容纳本次请求的个数，避免 RS 按 num 写多条 HccpTpInfo 时越界破坏堆
    out.resize(static_cast<size_t>(Hccl::TP_HANDLE_REQUEST_NUM) * sizeof(struct HccpTpInfo));
    struct HccpTpInfo *info = reinterpret_cast<struct HccpTpInfo *>(out.data());

    void *raReqHandle = nullptr;
    num = Hccl::TP_HANDLE_REQUEST_NUM; // 指定需要从管控面申请 tp handle 的上限；完成后 num 为实际个数
    const s32 ret = RaGetTpInfoListAsync(ctxHandle, &cfg, info, &num, &raReqHandle);
    if (ret != 0 || !raReqHandle) {
        HCCL_ERROR("[%s] failed, call interface error[%d] raReqHandle[%p], ctxHandle[%p] locAddr[%s] rmtAddr[%s].",
            __func__, ret, raReqHandle, ctxHandle, locAddr.Describe().c_str(), rmtAddr.Describe().c_str());
        return HcclResult::HCCL_E_NETWORK;
    }

    reqHandle = reinterpret_cast<RequestHandle>(raReqHandle);
    HCCL_INFO("[%s] get request handle[%llu].", __func__, reqHandle);
    return HcclResult::HCCL_SUCCESS;
}

HcclResult TpMgr::StartGetTpInfoListRequest(const GetTpInfoParam &param, RequestCtx &reqCtx) const
{
    EXCEPTION_HANDLE_BEGIN
    reqCtx.phase = ReqPhase::WAIT_LIST;
    reqCtx.tpAttrBitmap = 0;
    (void)memset_s(&reqCtx.tpAttr, sizeof(reqCtx.tpAttr), 0, sizeof(reqCtx.tpAttr));

    Hccl::IpAddress ipAddr{};
    CHK_RET(CommAddrToIpAddress(param.locAddr, ipAddr));
    const CtxHandle ctxHandle =
        static_cast<CtxHandle>(Hccl::RdmaHandleManager::GetInstance().GetByIp(devPhyId_, ipAddr));
    CHK_PTR_NULL(ctxHandle);

    CHK_RET(GetTpInfoListAsync(ctxHandle, param, reqCtx.dataBuffer, reqCtx.tpInfoNum, reqCtx.handle));
    EXCEPTION_HANDLE_END
    return HcclResult::HCCL_SUCCESS;
}

HcclResult TpMgr::StartGetTpAttrForFirstTp(const GetTpInfoParam &param, RequestCtx &reqCtx) const
{
    EXCEPTION_HANDLE_BEGIN
    (void)memset_s(&reqCtx.tpAttr, sizeof(reqCtx.tpAttr), 0, sizeof(reqCtx.tpAttr));
    reqCtx.tpAttrBitmap = (1U << kTpAttrSlAvailableBit) | kTpAttrBitmapSl;
    if (param.tpProtocol == TpProtocol::UBOE) {
        reqCtx.tpAttrBitmap |= kTpAttrBitmapDscp | (1U << kTpAttrDscpConfigModeBit);
    }

    const struct HccpTpInfo *list = reinterpret_cast<const struct HccpTpInfo *>(reqCtx.dataBuffer.data());
    const uint64_t firstTpHandle = list[0].tpHandle;

    Hccl::IpAddress ipAddr{};
    CHK_RET(CommAddrToIpAddress(param.locAddr, ipAddr));
    const CtxHandle ctxHandle =
        static_cast<CtxHandle>(Hccl::RdmaHandleManager::GetInstance().GetByIp(devPhyId_, ipAddr));
    CHK_PTR_NULL(ctxHandle);

    void *raReqHandle = nullptr;
    const s32 ret =
        RaGetTpAttrAsync(ctxHandle, firstTpHandle, &reqCtx.tpAttrBitmap, &reqCtx.tpAttr, &raReqHandle);
    if (ret != 0 || !raReqHandle) {
        HCCL_ERROR("[TpMgr][%s] RaGetTpAttrAsync failed ret[%d] raReqHandle[%p] ctx[%p] tpHandle[%llu].", __func__,
            ret, raReqHandle, ctxHandle, firstTpHandle);
        return HcclResult::HCCL_E_NETWORK;
    }
    reqCtx.handle = reinterpret_cast<RequestHandle>(raReqHandle);
    reqCtx.phase = ReqPhase::WAIT_TP_ATTR;
    EXCEPTION_HANDLE_END
    return HcclResult::HCCL_SUCCESS;
}

HcclResult TpMgr::FindAndGetTpAttr(const TpHandle tpHandle, TpAttrInfo &tpAttrInfo)
{
    std::lock_guard<std::mutex> lock(tpAttrCtxMutex_);
    auto attrIter = tpAttrCtxMap_.find(tpHandle);
    if (attrIter != tpAttrCtxMap_.end()) {
        attrIter->second.useCnt += 1;
        tpAttrInfo = attrIter->second.tpAttrInfo;
        return HcclResult::HCCL_SUCCESS;
    }

    return HcclResult::HCCL_E_NOT_FOUND;
}

HcclResult TpMgr::GetTpAttr(const GetTpAttrParam &param, TpAttrInfo &tpAttrInfo, CtxHandle ctxHandle)
{
    const TpHandle tpHandle = param.tpHandle;
    if (FindAndGetTpAttr(tpHandle, tpAttrInfo) == HcclResult::HCCL_SUCCESS) {
        return HcclResult::HCCL_SUCCESS;
    }

    std::unique_lock<std::mutex> reqCtxLock(tpAttrReqMutex_);
    auto reqCtxIter = tpAttrReqCtxMap_.find(tpHandle);
    if (reqCtxIter == tpAttrReqCtxMap_.end()) {
        HCCL_INFO("[TpMgr][%s] get new tpAttr, param[%s].", __func__,
            param.Describe().c_str());

        TpAttrRequestCtx &reqCtx = tpAttrReqCtxMap_[tpHandle];
        CHK_RET(StartGetTpAttrRequest(param, reqCtx, ctxHandle));
        return HcclResult::HCCL_E_AGAIN;
    }

    auto &reqCtx = reqCtxIter->second;
    auto ret = CheckRequestResult(reqCtx.handle);
    if (ret == HcclResult::HCCL_E_AGAIN) {
        return ret;
    }
    CHK_RET(ret);

    TpAttrRequestCtx completedReqCtx = reqCtxIter->second;
    tpAttrReqCtxMap_.erase(reqCtxIter);
    reqCtxLock.unlock();

    return HandleCompletedTpAttrRequest(std::move(completedReqCtx), tpHandle, tpAttrInfo);
}

HcclResult TpMgr::StartGetTpAttrRequest(const GetTpAttrParam &param,
    TpMgr::TpAttrRequestCtx &reqCtx, CtxHandle ctxHandle) const
{
    void *raReqHandle = nullptr;
    s32 ret = RaGetTpAttrAsync(ctxHandle, param.tpHandle, 
        const_cast<uint32_t*>(&param.attrBitmap), &reqCtx.tpAttr, &raReqHandle);
    if (ret != 0 || !raReqHandle) {
        HCCL_ERROR("[TpMgr][%s] failed, call RaGetTpAttrAsync error[%d] raReqHandle[%p], "
            "tpHandle[0x%llx] attrBitmap[0x%x].", __func__, ret, raReqHandle,
            param.tpHandle, param.attrBitmap);
        return HcclResult::HCCL_E_NETWORK;
    }

    reqCtx.handle = reinterpret_cast<RequestHandle>(raReqHandle);
    HCCL_INFO("[TpMgr][%s] success, tpHandle[0x%llx] reqHandle[%llu].",
        __func__, param.tpHandle, reqCtx.handle);
    return HcclResult::HCCL_SUCCESS;
}

HcclResult TpMgr::HandleCompletedTpAttrRequest(const TpMgr::TpAttrRequestCtx reqCtx,
    const TpHandle tpHandle, TpAttrInfo &tpAttrInfo)
{
    TpAttrInfo tmpTpAttrInfo(reqCtx.tpAttr);

    std::lock_guard<std::mutex> lock(tpAttrCtxMutex_);
    tpAttrCtxMap_[tpHandle] = {std::move(tmpTpAttrInfo), 1};
    
    tpAttrInfo = tpAttrCtxMap_[tpHandle].tpAttrInfo;
    return HcclResult::HCCL_SUCCESS;
}

HcclResult TpMgr::ReleaseTpAttr(const TpHandle tpHandle, const TpAttrInfo &tpAttrInfo)
{
    std::lock_guard<std::mutex> lock(tpAttrCtxMutex_);
    auto attrIter = tpAttrCtxMap_.find(tpHandle);
    if (attrIter == tpAttrCtxMap_.end()) {
        HCCL_ERROR("[TpMgr][%s] failed, tp attr is not found, "
            "tpHandle[0x%llx].", __func__, tpHandle);
        return HcclResult::HCCL_E_NOT_FOUND;
    }

    if (attrIter->second.useCnt > 1) {
        attrIter->second.useCnt -= 1;
        return HcclResult::HCCL_SUCCESS;
    }

    tpAttrCtxMap_.erase(attrIter);
    return HcclResult::HCCL_SUCCESS;
}

HcclResult TpMgr::GetTpTotalTimeout(const TpAttrInfo &tpAttrInfo, uint32_t &tpTimeOutMs)
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

static uint32_t TaHwValueToMs(uint8_t hwValue)
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

static uint8_t FindMinTaHwValue(uint32_t tpTotalTimeoutMs)
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

uint8_t TpMgr::CalcTaTimeout(const TpAttrInfo &tpAttrInfo)
{
    constexpr uint8_t UB_TIMEOUT_DEFAULT = 8; // 默认 UBC_CTP 和 UBC_TP 超时配置为8
    uint8_t envValue = static_cast<uint8_t>(Hccl::EnvConfig::GetInstance().GetRdmaConfig().GetUbTimeOut());
    uint32_t envTimeoutMs = TaHwValueToMs(envValue);
    
    uint32_t tpTimeOutMs = 0;
    (void)GetTpTotalTimeout(tpAttrInfo, tpTimeOutMs);
    
    uint8_t errTimeout = UB_TIMEOUT_DEFAULT;
    if (envTimeoutMs < tpTimeOutMs) {
        errTimeout = FindMinTaHwValue(tpTimeOutMs);
        HCCL_WARNING("[TpMgr][%s] Env timeout [%ums] < TP timeout [%ums]. Auto upgrade TA to hw_val[%u] (%ums).",
            __func__, envTimeoutMs, tpTimeOutMs, errTimeout, TaHwValueToMs(errTimeout));
    } else {
        errTimeout = envValue;
        HCCL_INFO("[TpMgr][%s] Env timeout [%ums] >= TP timeout [%ums]. Use env gear base hw_val[%u] (%ums).",
            __func__, envTimeoutMs, tpTimeOutMs, envValue, envTimeoutMs);
    }
    
    return errTimeout;
}

HcclResult TpMgr::BuildTpInfoAndCommitQosAttr(const GetTpInfoParam &param, const RequestCtx &reqCtx,
    const struct HccpTpInfo *baseInfoPtr, const uint32_t tpListIndex, const uint32_t mappedSl, TpInfo &tmpTpInfo)
{
    tmpTpInfo = TpInfo{};
    tmpTpInfo.tpHandle = baseInfoPtr[tpListIndex].tpHandle;
    tmpTpInfo.mappedJettyPriority = mappedSl & 0xFU;
    tmpTpInfo.hasMappedJettyPriority = true;

    if (param.tpProtocol == TpProtocol::RTP || param.tpProtocol == TpProtocol::UBOE) {
        CHK_RET(CommitMappedSlToTpAttr(devPhyId_, param.locAddr, tmpTpInfo.tpHandle, mappedSl));
    }
    if (param.tpProtocol == TpProtocol::UBOE && reqCtx.tpAttr.dscpConfigMode == 0) {
        const uint8_t dscpBefore = static_cast<uint8_t>(reqCtx.tpAttr.dscp & 0x3FU);
        const uint8_t requestQos = static_cast<uint8_t>(param.qos & 0xFFU);
        const uint16_t slMask = ReadSlAvailableMask16(reqCtx.tpAttr);
        const uint8_t dscpLookupQos = ResolveUboeDscpLookupQos(param, reqCtx.tpInfoNum, slMask);
        uint8_t dscp = 33U;
        (void)GetDscpByQosFromHccnCfg(devPhyId_, dscpLookupQos, dscp);
        CHK_RET(CommitUboeDscpToTpAttr(devPhyId_, param.locAddr, tmpTpInfo.tpHandle, dscp));
        HCCL_INFO("[TpMgr][%s] UBOE dscp updated: tpHandle[%llu] requestQos[%u] dscpLookupQos[%u] dscpBefore[%u] "
                  "dscpAfter[%u].",
            __func__, tmpTpInfo.tpHandle, static_cast<unsigned>(requestQos), static_cast<unsigned>(dscpLookupQos),
            static_cast<unsigned>(dscpBefore), static_cast<unsigned>(dscp));
    }
    HCCL_INFO("[TpMgr][%s] tp qos mapping ok: tpHandle[%llu] tpListIndex[%u] mappedSl[%u] jettyPriority[%u] qos[%u] param[%s].",
        __func__, tmpTpInfo.tpHandle, tpListIndex, static_cast<unsigned>(mappedSl & 0xFU), tmpTpInfo.mappedJettyPriority,
        param.qos & 0xFFU, param.Describe().c_str());
    return HcclResult::HCCL_SUCCESS;
}

HcclResult TpMgr::HandleCompletedRequest(RequestCtx reqCtx, const GetTpInfoParam &param, TpInfo &tpInfo)
{
    const uint32_t tpInfoNum = reqCtx.tpInfoNum;
    if (tpInfoNum == 0U) {
        HCCL_WARNING("[TpMgr][%s] failed to find tp info, tpInfoNum is 0, param[%s].", __func__,
            param.Describe().c_str());
        return HcclResult::HCCL_E_NOT_FOUND;
    }

    const struct HccpTpInfo *baseInfoPtr = reinterpret_cast<const struct HccpTpInfo *>(reqCtx.dataBuffer.data());
    const uint16_t slMask = ReadSlAvailableMask16(reqCtx.tpAttr);
    const uint32_t slAvailableCnt = CalSlAvailableCnt(slMask);
    HCCL_INFO("[TpMgr][%s] after get_tp_attr: slMask[0x%04x] slAvailableCnt[%u] slBitmap[0x%x] dscp[%u] dscpConfigMode[%u] "
              "tpAttrBitmap[0x%x] param[%s].",
        __func__, static_cast<unsigned>(slMask), slAvailableCnt, static_cast<unsigned>(reqCtx.tpAttr.slBitmap),
        static_cast<unsigned>(reqCtx.tpAttr.dscp & 0x3FU),
        static_cast<unsigned>(reqCtx.tpAttr.dscpConfigMode & 1U), reqCtx.tpAttrBitmap, param.Describe().c_str());
    if (slAvailableCnt == 0U) {
        HCCL_ERROR("[TpMgr][%s] sl_available mask empty after get_tp_attr, param[%s].", __func__,
            param.Describe().c_str());
        return HcclResult::HCCL_E_INTERNAL;
    }
    uint32_t tpListIndex = 0;
    uint32_t mappedSl = 0;
    if (!ApplyTpQosSlPolicy(param, tpInfoNum, slMask, tpListIndex, mappedSl)) {
        HCCL_ERROR("[TpMgr][%s] ApplyTpQosSlPolicy failed, param[%s] nTp[%u] slAvailableCnt[%u] mask[%u].",
            __func__, param.Describe().c_str(), tpInfoNum, slAvailableCnt, static_cast<unsigned>(slMask));
        return HcclResult::HCCL_E_INTERNAL;
    }
    if (tpListIndex >= tpInfoNum) {
        return HcclResult::HCCL_E_INTERNAL;
    }

    TpInfo tmpTpInfo{};
    CHK_RET(BuildTpInfoAndCommitQosAttr(param, reqCtx, baseInfoPtr, tpListIndex, mappedSl, tmpTpInfo));

    Hccl::IpAddress locAddr{};
    Hccl::IpAddress rmtAddr{};
    CHK_RET(CommAddrToIpAddress(param.locAddr, locAddr));
    CHK_RET(CommAddrToIpAddress(param.rmtAddr, rmtAddr));
    const QosKey qosKey = QosMapKey(param.qos);

    std::lock_guard<std::mutex> lock(GetInfoCtxMutex(param.tpProtocol));
    auto &infoMap = GetInfoCtxMap(param.tpProtocol);
    auto &rmtMap = infoMap[locAddr][rmtAddr];
    rmtMap[qosKey] = TpInfoCtx{std::move(tmpTpInfo), 1U};

    tpInfo = rmtMap[qosKey].tpInfo;
    return HcclResult::HCCL_SUCCESS;
}

TpMgr::InfoCtxMap &TpMgr::GetInfoCtxMap(const TpProtocol tpProtocol)
{
    switch (tpProtocol) {
        case TpProtocol::CTP:
            return ctpInfoMap_;
        case TpProtocol::RTP:
            return rtpInfoMap_;
        case TpProtocol::UBOE:
            return uboeInfoMap_;
        default:
            return rtpInfoMap_;
    }
}

TpMgr::ReqCtxMap &TpMgr::GetReqCtxMap(const TpProtocol tpProtocol)
{
    switch (tpProtocol) {
        case TpProtocol::CTP:
            return ctpReqMap_;
        case TpProtocol::RTP:
            return rtpReqMap_;
        case TpProtocol::UBOE:
            return uboeReqMap_;
        default:
            return rtpReqMap_;
    }
}

std::mutex &TpMgr::GetInfoCtxMutex(const TpProtocol tpProtocol)
{
    switch (tpProtocol) {
        case TpProtocol::CTP:
            return ctpInfoMutex_;
        case TpProtocol::RTP:
            return rtpInfoMutex_;
        case TpProtocol::UBOE:
            return uboeInfoMutex_;
        default:
            return rtpInfoMutex_;
    }
}

std::mutex &TpMgr::GetReqCtxMutex(const TpProtocol tpProtocol)
{
    switch (tpProtocol) {
        case TpProtocol::CTP:
            return ctpReqMutex_;
        case TpProtocol::RTP:
            return rtpReqMutex_;
        case TpProtocol::UBOE:
            return uboeReqMutex_;
        default:
            return rtpReqMutex_;
    }
}

} // namespace hcomm

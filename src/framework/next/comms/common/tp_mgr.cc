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

namespace hcomm {

namespace {
constexpr uint32_t kTpAttrSlAvailableBit = 17U;
static constexpr uint32_t kTpAttrBitmapSl = (1U << 10U);
static constexpr uint32_t kTpAttrBitmapDscp = (1U << 8U);
static constexpr uint32_t kTpAttrDscpConfigModeBit = 18U;

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
    const uint32_t slRank = slotIdx;
    if (slRank >= slAvailableCnt) {
        return false;
    }
    tpListIndexOut = slotIdx;
    mappedSlOut = SlValueAtRankInMask16(slMask, slRank);
    return true;
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

void TpMgr::EraseReqCtxAtQos(ReqCtxMap &reqCtxMap, const Hccl::IpAddress &loc, const Hccl::IpAddress &rmt,
    uint32_t qosKey)
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

bool TpMgr::FindAndGetTpInfo(const TpProtocol tpProtocol, const Hccl::IpAddress &locAddr, const Hccl::IpAddress &rmtAddr,
    uint32_t qosKey, TpInfo &tpInfo)
{
    std::lock_guard<std::mutex> lock(GetInfoCtxMutex(tpProtocol));
    auto &infoMap = GetInfoCtxMap(tpProtocol);
    auto lit = infoMap.find(locAddr);
    if (lit == infoMap.end()) {
        return false;
    }
    auto rit = lit->second.find(rmtAddr);
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

HcclResult TpMgr::WaitForInFlightGetTpReqResult(const GetTpInfoParam &param, const TpProtocol tpProtocol,
    RequestCtx &reqCtx) const
{
    const auto ret = CheckRequestResult(reqCtx.handle);
    if (ret == HcclResult::HCCL_E_AGAIN) {
        return ret;
    }
    if (ret != HcclResult::HCCL_SUCCESS) {
        const char *phaseStr = (reqCtx.phase == ReqPhase::WAIT_LIST)
            ? "WAIT_LIST(RaGetTpInfoListAsync)"
            : "WAIT_TP_ATTR(RaGetTpAttrAsync)";
        HCCL_ERROR(
            "[TpMgr][GetTpInfo] async completed with failure, phase[%s] devPhyId[%u] tpProtocol[%s] qos[%u] "
            "reqHandle[%llu] tpInfoNum[%u] param[%s] hcclRet[%d]; see prior [HccpGetAsyncReqResult] line for "
            "async reqResult code.",
            phaseStr, devPhyId_, tpProtocol.Describe().c_str(), param.qos & 0xFFU,
            static_cast<unsigned long long>(reqCtx.handle), reqCtx.tpInfoNum, param.Describe().c_str(),
            static_cast<int>(ret));
        return ret;
    }
    return HcclResult::HCCL_SUCCESS;
}

HcclResult TpMgr::OnGetTpInfoListAsyncDoneThenSubmitTpAttr(const GetTpInfoParam &param, ReqCtxMap &reqCtxMap,
    const Hccl::IpAddress &locAddr, const Hccl::IpAddress &rmtAddr, const uint32_t qosKey, RequestCtx &reqCtx,
    std::unique_lock<std::mutex> &reqCtxLock) const
{
    if (reqCtx.tpInfoNum == 0U) {
        EraseReqCtxAtQos(reqCtxMap, locAddr, rmtAddr, qosKey);
        reqCtxLock.unlock();
        HCCL_WARNING("[TpMgr][%s] failed to find tp info, tpInfoNum is 0, param[%s].", __func__,
            param.Describe().c_str());
        return HcclResult::HCCL_E_NOT_FOUND;
    }
    const struct HccpTpInfo *list = reinterpret_cast<const struct HccpTpInfo *>(reqCtx.dataBuffer.data());
    HCCL_INFO("[TpMgr][GetTpInfo] list stage ok, devPhyId[%u] tpInfoNum[%u] firstTpHandle[%llu] param[%s].",
        devPhyId_, reqCtx.tpInfoNum, static_cast<unsigned long long>(list[0].tpHandle), param.Describe().c_str());
    CHK_RET(StartGetTpAttrForFirstTp(param, reqCtx));
    HCCL_INFO("[TpMgr][GetTpInfo] RaGetTpAttrAsync submitted, devPhyId[%u] reqHandle[%llu] phase[WAIT_TP_ATTR] "
              "tpAttrBitmap[0x%x] param[%s].",
        devPhyId_, static_cast<unsigned long long>(reqCtx.handle), reqCtx.tpAttrBitmap, param.Describe().c_str());
    return HcclResult::HCCL_E_AGAIN;
}

HcclResult TpMgr::RunHandleCompletedGetTpEraseReq(ReqCtxMap &reqCtxMap, const Hccl::IpAddress &locAddr,
    const Hccl::IpAddress &rmtAddr, const uint32_t qosKey, RequestCtx &&completedReqCtx,
    std::unique_lock<std::mutex> &reqCtxLock, const GetTpInfoParam &param, TpInfo &tpInfo)
{
    // `completedReqCtx` 常绑定在 map 槽位；须先移出再 Erase，避免先析构再 move（UB / tcache double free）
    RequestCtx doneCtx(std::move(completedReqCtx));
    EraseReqCtxAtQos(reqCtxMap, locAddr, rmtAddr, qosKey);
    // 与 Legacy TpManager 一致：先写入 info 缓存再释放 req 互斥量，避免同 qosKey 上并发插入 in-flight 与本次提交竞态
    const HcclResult ret = HandleCompletedRequest(std::move(doneCtx), param, tpInfo);
    reqCtxLock.unlock();
    return ret;
}

HcclResult TpMgr::GetTpInfo(const GetTpInfoParam &param, TpInfo &tpInfo)
{
    const auto &tpProtocol = param.tpProtocol;
    CHK_RET(CheckTpProtocol(tpProtocol));

    Hccl::IpAddress locAddr{};
    Hccl::IpAddress rmtAddr{};
    CHK_RET(CommAddrToIpAddress(param.locAddr, locAddr));
    CHK_RET(CommAddrToIpAddress(param.rmtAddr, rmtAddr));
    const uint32_t qosKey = TpCacheQosKey(param.qos);
    if (FindAndGetTpInfo(tpProtocol, locAddr, rmtAddr, qosKey, tpInfo)) {
        return HcclResult::HCCL_SUCCESS;
    }

    std::unique_lock<std::mutex> reqCtxLock(GetReqCtxMutex(tpProtocol));

    auto &reqCtxMap = GetReqCtxMap(tpProtocol);

    auto &rmtReqMap = reqCtxMap[locAddr];
    auto &qosReqMap = rmtReqMap[rmtAddr];
    auto qosReqIter = qosReqMap.find(qosKey);
    if (qosReqIter == qosReqMap.end()) {
        HCCL_INFO("[TpMgr][%s] get new tpInfo, param[%s].", __func__, param.Describe().c_str());

        RequestCtx &reqCtx = qosReqMap[qosKey];
        CHK_RET(StartGetTpInfoListRequest(param, reqCtx));
        HCCL_INFO("[TpMgr][GetTpInfo] RaGetTpInfoListAsync submitted, devPhyId[%u] reqHandle[%llu] phase[WAIT_LIST] "
                  "param[%s].",
            devPhyId_, static_cast<unsigned long long>(reqCtx.handle), param.Describe().c_str());
        return HcclResult::HCCL_E_AGAIN; // 首次触发异步接口调用，动作一定未完成
    }

    RequestCtx &reqCtx = qosReqIter->second;
    CHK_RET(WaitForInFlightGetTpReqResult(param, tpProtocol, reqCtx));

    if (reqCtx.phase == ReqPhase::WAIT_LIST) {
        return OnGetTpInfoListAsyncDoneThenSubmitTpAttr(param, reqCtxMap, locAddr, rmtAddr, qosKey, reqCtx, reqCtxLock);
    }

    return RunHandleCompletedGetTpEraseReq(reqCtxMap, locAddr, rmtAddr, qosKey, std::move(qosReqIter->second),
        reqCtxLock, param, tpInfo);
}

HcclResult TpMgr::ReleaseTpInfo(const GetTpInfoParam &param, const TpInfo &tpInfo)
{
    Hccl::IpAddress locAddr{}, rmtAddr{};
    CHK_RET(CommAddrToIpAddress(param.locAddr, locAddr));
    CHK_RET(CommAddrToIpAddress(param.rmtAddr, rmtAddr));
    const uint32_t qosKey = TpCacheQosKey(param.qos);

    std::lock_guard<std::mutex> lock(GetInfoCtxMutex(param.tpProtocol));
    auto &infoMap = GetInfoCtxMap(param.tpProtocol);
    auto lit = infoMap.find(locAddr);
    if (lit == infoMap.end()) {
        HCCL_ERROR("[TpMgr][%s] failed, tp info is not found, "
            "param[%s].", __func__, param.Describe().c_str());
        return HcclResult::HCCL_E_NOT_FOUND;
    }
    auto rit = lit->second.find(rmtAddr);
    if (rit == lit->second.end()) {
        HCCL_ERROR("[TpMgr][%s] failed, tp info is not found, "
            "param[%s].", __func__, param.Describe().c_str());
        return HcclResult::HCCL_E_NOT_FOUND;
    }
    auto qit = rit->second.find(qosKey);
    if (qit == rit->second.end()) {
        HCCL_ERROR("[TpMgr][%s] failed, tp info is not found, "
            "param[%s].", __func__, param.Describe().c_str());
        return HcclResult::HCCL_E_NOT_FOUND;
    }

    if (tpInfo.tpHandle != qit->second.tpInfo.tpHandle) {
        HCCL_ERROR("[TpMgr][%s] failed, tp info[%llu] is not expected[%llu].",
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
    // 当前 ub 在 unimport jetty 时通过引用计数管理释放 tp handle
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

HcclResult TpMgr::CommitTpAttrsAfterSlMapping(const GetTpInfoParam &param, const TpAttr &tpAttr, uint64_t tpHandle,
    uint32_t mappedSl)
{
    // RTP / UBOE：将 ApplyUbcQosTpSlPolicy 得到的 mapped SL 写回 TP；CTP 不向 TP 写 SL（沿用既有约定）
    if (param.tpProtocol == TpProtocol::RTP || param.tpProtocol == TpProtocol::UBOE) {
        CHK_RET(CommitMappedSlToTpAttr(devPhyId_, param.locAddr, tpHandle, mappedSl));
    }
    if (param.tpProtocol == TpProtocol::UBOE && tpAttr.dscpConfigMode == 0) {
        const uint8_t dscpBefore = static_cast<uint8_t>(tpAttr.dscp & 0x3FU);
        const uint8_t qos = static_cast<uint8_t>(param.qos & 0xFFU);
        uint8_t dscp = 33U;
        (void)GetDscpByQosFromHccnCfg(devPhyId_, qos, dscp);
        CHK_RET(CommitUboeDscpToTpAttr(devPhyId_, param.locAddr, tpHandle, dscp));
        HCCL_INFO("[TpMgr][%s] UBOE dscp updated: tpHandle[%llu] qos[%u] dscpBefore[%u] dscpAfter[%u].", __func__,
            tpHandle, static_cast<unsigned>(qos), static_cast<unsigned>(dscpBefore), static_cast<unsigned>(dscp));
    }
    return HcclResult::HCCL_SUCCESS;
}

HcclResult TpMgr::MapTpInfoFromTpAttr(const GetTpInfoParam &param, const RequestCtx &reqCtx, TpInfo &outTpInfo)
{
    const uint32_t tpInfoNum = reqCtx.tpInfoNum;
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
    if (!ApplyUbcQosTpSlPolicy(param, tpInfoNum, slMask, tpListIndex, mappedSl)) {
        HCCL_ERROR("[TpMgr][%s] ApplyUbcQosTpSlPolicy failed, param[%s] nTp[%u] slAvailableCnt[%u] mask[%u].",
            __func__, param.Describe().c_str(), tpInfoNum, slAvailableCnt, static_cast<unsigned>(slMask));
        return HcclResult::HCCL_E_INTERNAL;
    }
    if (tpListIndex >= tpInfoNum) {
        return HcclResult::HCCL_E_INTERNAL;
    }

    outTpInfo = TpInfo{};
    outTpInfo.tpHandle = baseInfoPtr[tpListIndex].tpHandle;
    outTpInfo.mappedJettyPriority = mappedSl & 0xFU;
    outTpInfo.hasMappedJettyPriority = true;

    CHK_RET(CommitTpAttrsAfterSlMapping(param, reqCtx.tpAttr, outTpInfo.tpHandle, mappedSl));

    HCCL_INFO("[TpMgr][%s] tp qos mapping ok: tpInfoNum[%u] tpHandle[%llu] tpListIndex[%u] mappedJettyPriority[%u] qos[%u] param[%s].",
        __func__, tpInfoNum, outTpInfo.tpHandle, tpListIndex, outTpInfo.mappedJettyPriority,
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

    TpInfo tmpTpInfo{};
    CHK_RET(MapTpInfoFromTpAttr(param, reqCtx, tmpTpInfo));

    Hccl::IpAddress locAddr{};
    Hccl::IpAddress rmtAddr{};
    CHK_RET(CommAddrToIpAddress(param.locAddr, locAddr));
    CHK_RET(CommAddrToIpAddress(param.rmtAddr, rmtAddr));
    const uint32_t qosKey = TpCacheQosKey(param.qos);

    std::lock_guard<std::mutex> lock(GetInfoCtxMutex(param.tpProtocol));
    auto &infoMap = GetInfoCtxMap(param.tpProtocol);
    auto &qosMap = infoMap[locAddr][rmtAddr];
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

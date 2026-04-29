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

#include "hccp_ctx.h"
#include "hccp_async_ctx.h"

#include "hccl_common.h"
#include "exception_handler.h"
#include "rdma_handle_manager.h"

namespace hcomm {

namespace {

// urma_api.h 公开 tp_attr_bitmap 仅 0–11（至 ttl）。若扩展位按 ttl 后「每个成员各占 1 位」递增（与 RS 一致）：
// 12=ack_udp_srcport，13=data_udp_srcport，14=udp_srcport_range，15=spray_en，16=udp_global_en，17=reserve_0，18=slBitmap。
// 若 Get 后 slBitmap 仍为 0，请以 RS/URMA 文档为准改本常量（例如合并位域为一扩展位时编号会前移）。
constexpr uint32_t kTpAttrSlAvailableBit = 18U;
static constexpr bool kSkipRaGetTpAttrStubSlAvailable = false;
// hccp_tp.h：TpAttr.sl 对应 bitmap bit 10；经 RaCtxSetTpAttr → urma_set_tp_attr 写回当前 tpHandle（与 mappedJettyPriority 独立）
static constexpr uint32_t kTpAttrBitmapSl = (1U << 10U);
// urma_api：bit 8 = dscp；扩展位在 slBitmap(18) 之后递增至 dscpConfigMode（与 urma_tp_attr_value_t 成员顺序一致）
static constexpr uint32_t kTpAttrBitmapDscp = (1U << 8U);
static constexpr uint32_t kTpAttrDscpConfigModeBit = 19U;

static uint32_t PopCount16(uint32_t mask)
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
    const uint32_t mPop = PopCount16(slMask); //slMask 里为 1 的 bit 个数 = 可用 SL 档位数
    if (mPop == 0U) {
        return false;
    }
    uint32_t mSl = mPop;
    if (param.slLevelCount != 0U) {
        mSl = std::min(param.slLevelCount, mPop);
    }
    if (param.loopFirstTpLowestSl) {
        tpListIndexOut = 0;
        mappedSlOut = SlValueAtRankInMask16(slMask, 0);
        return true;
    }
    if (nTp == 0U || mSl == 0U) {
        return false;
    }
    const uint32_t k = std::min(nTp, mSl);
    if (k == 0U) {
        return false;
    }
    const uint32_t numGroups = std::min(8U, k);
    const uint32_t qos = param.qos & 7U;
    const uint32_t groupIdx = (qos * numGroups) / 8U;
    const uint32_t slotIdx = (groupIdx * k) / numGroups;
    if (slotIdx >= k || slotIdx >= nTp) {
        return false;
    }
    const uint32_t slRank = slotIdx;
    if (slRank >= mPop) {
        return false;
    }
    tpListIndexOut = slotIdx;
    mappedSlOut = SlValueAtRankInMask16(slMask, slRank);
    HCCL_INFO("[TpMgr][%s] nTp[%u] mPop[%u] mSl[%u] k[%u] numGroups[%u] qos[%u] groupIdx[%u] slotIdx[%u] slMask[0x%x] "
              "tpListIdx[%u] mappedSl[%u] slLevelCount[%u].",
        __func__, nTp, mPop, mSl, k, numGroups, qos, groupIdx, slotIdx, static_cast<unsigned>(slMask), tpListIndexOut,
        mappedSlOut, param.slLevelCount);
    return true;
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
    const CtxHandle ctxHandle =
        static_cast<CtxHandle>(Hccl::RdmaHandleManager::GetInstance().GetByIp(devPhyId, locAddr));
    CHK_PTR_NULL(ctxHandle);

    struct TpAttr tpSlAttr {};
    tpSlAttr.sl = static_cast<uint8_t>(mappedSl & 0xFU);

    const s32 ret = RaCtxSetTpAttr(ctxHandle, tpHandle, kTpAttrBitmapSl, &tpSlAttr);
    if (ret != 0) {
        HCCL_ERROR("[TpMgr][CommitMappedSlToTpAttr] RaCtxSetTpAttr failed ret[%d] tpHandle[%llu] sl[%u].", ret,
            tpHandle, static_cast<unsigned>(mappedSl & 0xFU));
        return HcclResult::HCCL_E_NETWORK;
    }
    HCCL_INFO("[TpMgr][CommitMappedSlToTpAttr] RaCtxSetTpAttr ok tpHandle[%llu] sl[%u].", tpHandle,
        static_cast<unsigned>(mappedSl & 0xFU));
    return HcclResult::HCCL_SUCCESS;
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
    const CtxHandle ctxHandle =
        static_cast<CtxHandle>(Hccl::RdmaHandleManager::GetInstance().GetByIp(devPhyId, locAddr));
    CHK_PTR_NULL(ctxHandle);

    struct TpAttr tpDscpAttr {};
    tpDscpAttr.dscp = static_cast<uint8_t>(dscp & 0x3FU);

    const s32 ret = RaCtxSetTpAttr(ctxHandle, tpHandle, kTpAttrBitmapDscp, &tpDscpAttr);
    if (ret != 0) {
        HCCL_ERROR("[TpMgr][CommitUboeDscpToTpAttr] RaCtxSetTpAttr failed ret[%d] tpHandle[%llu] dscp[%u].", ret,
            tpHandle, static_cast<unsigned>(tpDscpAttr.dscp));
        return HcclResult::HCCL_E_NETWORK;
    }
    HCCL_INFO("[TpMgr][CommitUboeDscpToTpAttr] RaCtxSetTpAttr ok tpHandle[%llu] dscp[%u].", tpHandle,
        static_cast<unsigned>(tpDscpAttr.dscp));
    return HcclResult::HCCL_SUCCESS;
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

HcclResult TpMgr::GetTpInfo(const GetTpInfoParam &param, TpInfo &tpInfo)
{
    const auto &tpProtocol = param.tpProtocol;
    CHK_RET(CheckTpProtocol(tpProtocol));
    if (FindAndGetTpInfo(param, tpInfo) == HcclResult::HCCL_SUCCESS) {
        return HcclResult::HCCL_SUCCESS;
    }

    std::unique_lock<std::mutex> reqCtxLock(GetReqCtxMutex(tpProtocol));

    auto &reqCtxMap = GetReqCtxMap(tpProtocol);
    Hccl::IpAddress locAddr{};
    Hccl::IpAddress rmtAddr{};
    CHK_RET(CommAddrToIpAddress(param.locAddr, locAddr));
    CHK_RET(CommAddrToIpAddress(param.rmtAddr, rmtAddr));

    const QosKey qosKey = param.qos & 0xFFU;
    auto &rmtMap = reqCtxMap[locAddr];
    auto &qosMap = rmtMap[rmtAddr];
    auto it = qosMap.find(qosKey);
    if (it == qosMap.end()) {
        HCCL_INFO("[TpMgr][%s] get new tpInfo, param[%s].", __func__, param.Describe().c_str());

        RequestCtx &reqCtx = qosMap[qosKey];
        CHK_RET(StartGetTpInfoListRequest(param, reqCtx));
        return HcclResult::HCCL_E_AGAIN;
    }

    RequestCtx &reqCtx = it->second;
    auto ret = CheckRequestResult(reqCtx.handle);
    if (ret == HcclResult::HCCL_E_AGAIN) {
        return ret;
    }
    CHK_RET(ret);

    if (reqCtx.phase == ReqPhase::WAIT_LIST) {
        if (reqCtx.tpInfoNum == 0U) {
            qosMap.erase(it);
            reqCtxLock.unlock();
            HCCL_WARNING("[TpMgr][%s] failed to find tp info, tpInfoNum is 0, param[%s].", __func__,
                param.Describe().c_str());
            return HcclResult::HCCL_E_NOT_FOUND;
        }
        CHK_RET(StartGetTpAttrForFirstTp(param, reqCtx));
        return HcclResult::HCCL_E_AGAIN;
    }

    RequestCtx completedReqCtx = std::move(it->second);
    qosMap.erase(it);
    reqCtxLock.unlock();

    return HandleCompletedRequest(std::move(completedReqCtx), param, tpInfo);
}

HcclResult TpMgr::ReleaseTpInfo(const GetTpInfoParam &param, const TpInfo &tpInfo)
{
    Hccl::IpAddress locAddr{};
    Hccl::IpAddress rmtAddr{};
    CHK_RET(CommAddrToIpAddress(param.locAddr, locAddr));
    CHK_RET(CommAddrToIpAddress(param.rmtAddr, rmtAddr));

    const QosKey qosKey = param.qos & 0xFFU;
    std::lock_guard<std::mutex> lock(GetInfoCtxMutex(param.tpProtocol));
    auto &infoMap = GetInfoCtxMap(param.tpProtocol);
    auto &locInfoMap = infoMap[locAddr];
    auto rmtIt = locInfoMap.find(rmtAddr);
    if (rmtIt == locInfoMap.end()) {
        HCCL_ERROR("[TpMgr][%s] failed, tp info is not found, param[%s].", __func__, param.Describe().c_str());
        return HcclResult::HCCL_E_NOT_FOUND;
    }
    auto qosIt = rmtIt->second.find(qosKey);
    if (qosIt == rmtIt->second.end()) {
        HCCL_ERROR("[TpMgr][%s] failed, tp info is not found for qosKey[%u], param[%s].", __func__, qosKey,
            param.Describe().c_str());
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
        locInfoMap.erase(rmtIt);
    }
    return HcclResult::HCCL_SUCCESS;
}

HcclResult TpMgr::FindAndGetTpInfo(const GetTpInfoParam &param, TpInfo &tpInfo)
{
    Hccl::IpAddress locAddr{};
    Hccl::IpAddress rmtAddr{};
    CHK_RET(CommAddrToIpAddress(param.locAddr, locAddr));
    CHK_RET(CommAddrToIpAddress(param.rmtAddr, rmtAddr));

    const QosKey qosKey = param.qos & 0xFFU;
    std::lock_guard<std::mutex> lock(GetInfoCtxMutex(param.tpProtocol));
    auto &infoMap = GetInfoCtxMap(param.tpProtocol);
    auto &locInfoMap = infoMap[locAddr];
    auto rmtIt = locInfoMap.find(rmtAddr);
    if (rmtIt == locInfoMap.end()) {
        return HcclResult::HCCL_E_NOT_FOUND;
    }
    auto qosIt = rmtIt->second.find(qosKey);
    if (qosIt == rmtIt->second.end()) {
        return HcclResult::HCCL_E_NOT_FOUND;
    }
    qosIt->second.useCnt += 1;
    tpInfo = qosIt->second.tpInfo;
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

    out.resize(static_cast<size_t>(HCCP_MAX_TPID_INFO_NUM) * sizeof(struct HccpTpInfo));
    struct HccpTpInfo *info = reinterpret_cast<struct HccpTpInfo *>(out.data());

    void *raReqHandle = nullptr;
    num = static_cast<uint32_t>(HCCP_MAX_TPID_INFO_NUM);
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

    if (kSkipRaGetTpAttrStubSlAvailable) {
        // slBitmap 低 16bit：仅 bit1–3 置 1 → 0x000E，表示 SL 1/2/3 可用
        reqCtx.tpAttr.slBitmap = 0x000EU;
        reqCtx.tpAttr.dscpConfigMode = 1; // stub 下跳过 UBOE 业务面写 dscp
        reqCtx.handle = 0;
        reqCtx.phase = ReqPhase::WAIT_TP_ATTR;
        return HcclResult::HCCL_SUCCESS;
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

HcclResult TpMgr::HandleCompletedRequest(RequestCtx reqCtx, const GetTpInfoParam &param, TpInfo &tpInfo)
{
    const uint32_t tpInfoNum = reqCtx.tpInfoNum;
    if (tpInfoNum == 0U) {
        HCCL_WARNING("[TpMgr][%s] failed to find tp info, tpInfoNum is 0, param[%s].", __func__,
            param.Describe().c_str());
        return HcclResult::HCCL_E_NOT_FOUND;
    }

    const struct HccpTpInfo *baseInfoPtr = reinterpret_cast<const struct HccpTpInfo *>(reqCtx.dataBuffer.data());
    uint16_t slMask = ReadSlAvailableMask16(reqCtx.tpAttr);
    uint32_t mPop = PopCount16(slMask);
    HCCL_INFO("[TpMgr][%s] after get_tp_attr: slMask[0x%04x] mPop[%u] slBitmap[0x%x] dscp[%u] dscpConfigMode[%u] "
              "tpAttrBitmap[0x%x] param[%s].",
        __func__, static_cast<unsigned>(slMask), mPop, static_cast<unsigned>(reqCtx.tpAttr.slBitmap),
        static_cast<unsigned>(reqCtx.tpAttr.dscp & 0x3FU),
        static_cast<unsigned>(reqCtx.tpAttr.dscpConfigMode & 1U), reqCtx.tpAttrBitmap, param.Describe().c_str());
    if (mPop == 0U) {
        HCCL_ERROR("[TpMgr][%s] sl_available mask empty after get_tp_attr, param[%s].", __func__,
            param.Describe().c_str());
        return HcclResult::HCCL_E_INTERNAL;
    }
    uint32_t mSl = mPop;
    if (param.slLevelCount != 0U) {
        mSl = std::min(param.slLevelCount, mPop);
    }

    uint32_t tpListIndex = 0;
    uint32_t mappedSl = 0;
    if (!ApplyUbcQosTpSlPolicy(param, tpInfoNum, slMask, tpListIndex, mappedSl)) {
        HCCL_ERROR("[TpMgr][%s] ApplyUbcQosTpSlPolicy failed, param[%s] nTp[%u] mSl[%u] mask[%u].", __func__,
            param.Describe().c_str(), tpInfoNum, mSl, static_cast<unsigned>(slMask));
        return HcclResult::HCCL_E_INTERNAL;
    }
    if (tpListIndex >= tpInfoNum) {
        return HcclResult::HCCL_E_INTERNAL;
    }

    TpInfo tmpTpInfo{};
    tmpTpInfo.tpHandle = baseInfoPtr[tpListIndex].tpHandle;
    tmpTpInfo.mappedJettyPriority = mappedSl & 0xFU;
    tmpTpInfo.hasMappedJettyPriority = true;

    CHK_RET(CommitMappedSlToTpAttr(devPhyId_, param.locAddr, tmpTpInfo.tpHandle, mappedSl));
    if (param.tpProtocol == TpProtocol::UBOE && reqCtx.tpAttr.dscpConfigMode == 0) {
        const uint8_t dscpBefore = static_cast<uint8_t>(reqCtx.tpAttr.dscp & 0x3FU);
        uint8_t dscp = 33U;
        CHK_RET(CommitUboeDscpToTpAttr(devPhyId_, param.locAddr, tmpTpInfo.tpHandle, dscp));
        HCCL_INFO("[TpMgr][%s] UBOE dscp updated: tpHandle[%llu] dscpBefore[%u] dscpAfter[%u].", __func__,
            tmpTpInfo.tpHandle, static_cast<unsigned>(dscpBefore), static_cast<unsigned>(dscp));
    }
    HCCL_INFO("[TpMgr][%s] tp qos mapping ok: tpHandle[%llu] tpListIndex[%u] mappedSl[%u] jettyPriority[%u] qos[%u] param[%s].",
        __func__, tmpTpInfo.tpHandle, tpListIndex, static_cast<unsigned>(mappedSl & 0xFU), tmpTpInfo.mappedJettyPriority,
        param.qos & 0xFFU, param.Describe().c_str());

    Hccl::IpAddress locAddr{};
    Hccl::IpAddress rmtAddr{};
    CHK_RET(CommAddrToIpAddress(param.locAddr, locAddr));
    CHK_RET(CommAddrToIpAddress(param.rmtAddr, rmtAddr));
    const QosKey qosKey = param.qos & 0xFFU;

    std::lock_guard<std::mutex> lock(GetInfoCtxMutex(param.tpProtocol));
    auto &infoMap = GetInfoCtxMap(param.tpProtocol);
    auto &rmtMap = infoMap[locAddr][rmtAddr];
    rmtMap[qosKey] = {std::move(tmpTpInfo), 1};

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

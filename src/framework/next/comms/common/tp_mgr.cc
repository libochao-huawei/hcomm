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

constexpr uint32_t kTpAttrSlAvailableBit = 12U;
// URMA 头文件仅定义 tp_attr bitmap 0–11；bit12 由驱动扩展。为 false 时走 RaGetTpAttrAsync。
static constexpr bool kSkipRaGetTpAttrStubSlAvailable = true;
// hccp_tp.h：TpAttr.sl 对应 bitmap bit 10；经 RaCtxSetTpAttr → urma_set_tp_attr 写回当前 tpHandle（与 mappedJettyPriority 独立）
static constexpr uint32_t kTpAttrBitmapSl = (1U << 10U);

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
    return static_cast<uint16_t>(static_cast<uint8_t>(attr.reserved[0])) |
        (static_cast<uint16_t>(static_cast<uint8_t>(attr.reserved[1])) << 8);
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
/*
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
*/
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

TpMgr::QosKey TpMgr::TpInfoCacheQos(const GetTpInfoParam &param)
{
    return param.qos & 0xFFU;
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
    if (tpProtocol != TpProtocol::CTP && tpProtocol != TpProtocol::RTP) {
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

    auto &reqMap = GetReqCtxMap(tpProtocol);
    const QosKey qosKey = TpInfoCacheQos(param);
    auto it = reqMap.find(qosKey);
    if (it == reqMap.end()) {
        HCCL_INFO("[TpMgr][%s] get new tpInfo, param[%s].", __func__, param.Describe().c_str());

        RequestCtx &reqCtx = reqMap[qosKey];
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
            reqMap.erase(it);
            reqCtxLock.unlock();
            HCCL_WARNING("[TpMgr][%s] failed to find tp info, tpInfoNum is 0, param[%s].", __func__,
                param.Describe().c_str());
            return HcclResult::HCCL_E_NOT_FOUND;
        }
        CHK_RET(StartGetTpAttrForFirstTp(param, reqCtx));
        return HcclResult::HCCL_E_AGAIN;
    }

    RequestCtx completedReqCtx = std::move(it->second);
    reqMap.erase(it);
    reqCtxLock.unlock();

    return HandleCompletedRequest(std::move(completedReqCtx), param, tpInfo);
}

HcclResult TpMgr::ReleaseTpInfo(const GetTpInfoParam &param, const TpInfo &tpInfo)
{
    const QosKey qosKey = TpInfoCacheQos(param);
    std::lock_guard<std::mutex> lock(GetInfoCtxMutex(param.tpProtocol));
    auto &infoMap = GetInfoCtxMap(param.tpProtocol);
    auto qosIt = infoMap.find(qosKey);
    if (qosIt == infoMap.end()) {
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

    infoMap.erase(qosIt);
    return HcclResult::HCCL_SUCCESS;
}

HcclResult TpMgr::FindAndGetTpInfo(const GetTpInfoParam &param, TpInfo &tpInfo)
{
    const QosKey qosKey = TpInfoCacheQos(param);
    std::lock_guard<std::mutex> lock(GetInfoCtxMutex(param.tpProtocol));
    auto &infoMap = GetInfoCtxMap(param.tpProtocol);
    auto qosIt = infoMap.find(qosKey);
    if (qosIt == infoMap.end()) {
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
    reqCtx.tpAttrBitmap = (1U << kTpAttrSlAvailableBit);

    if (kSkipRaGetTpAttrStubSlAvailable) {
        // sl_available 低 16bit：仅 bit1–3 置 1 → 0x000E，表示 SL 1/2/3 可用
        reqCtx.tpAttr.reserved[0] = 0x0EU;
        reqCtx.tpAttr.reserved[1] = 0x00U;
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
    HCCL_INFO("[TpMgr][%s] after get_tp_attr: slMask[0x%04x] mPop[%u] reserved[0/1][%u,%u] tpAttrBitmap[0x%x] param[%s].",
        __func__, static_cast<unsigned>(slMask), mPop,
        static_cast<unsigned>(static_cast<uint8_t>(reqCtx.tpAttr.reserved[0])),
        static_cast<unsigned>(static_cast<uint8_t>(reqCtx.tpAttr.reserved[1])), reqCtx.tpAttrBitmap,
        param.Describe().c_str());
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

    // CHK_RET(CommitMappedSlToTpAttr(devPhyId_, param.locAddr, tmpTpInfo.tpHandle, mappedSl)); set tp attr 等待接口
    HCCL_INFO("[TpMgr][%s] tp qos mapping ok: tpHandle[%llu] tpListIndex[%u] mappedSl[%u] jettyPriority[%u] qos[%u] param[%s].",
        __func__, tmpTpInfo.tpHandle, tpListIndex, static_cast<unsigned>(mappedSl & 0xFU), tmpTpInfo.mappedJettyPriority,
        param.qos & 0xFFU, param.Describe().c_str());

    const QosKey qosKey = TpInfoCacheQos(param);

    std::lock_guard<std::mutex> lock(GetInfoCtxMutex(param.tpProtocol));
    auto &infoMap = GetInfoCtxMap(param.tpProtocol);
    infoMap[qosKey] = {std::move(tmpTpInfo), 1};

    tpInfo = infoMap[qosKey].tpInfo;
    return HcclResult::HCCL_SUCCESS;
}

TpMgr::InfoCtxMap &TpMgr::GetInfoCtxMap(const TpProtocol tpProtocol)
{
    return tpProtocol == TpProtocol::CTP ? ctpInfoMap_ : rtpInfoMap_;
}

TpMgr::ReqCtxMap &TpMgr::GetReqCtxMap(const TpProtocol tpProtocol)
{
    return tpProtocol == TpProtocol::CTP ? ctpReqMap_ : rtpReqMap_;
}

std::mutex &TpMgr::GetInfoCtxMutex(const TpProtocol tpProtocol)
{
    return tpProtocol == TpProtocol::CTP ? ctpInfoMutex_ : rtpInfoMutex_;
}

std::mutex &TpMgr::GetReqCtxMutex(const TpProtocol tpProtocol)
{
    return tpProtocol == TpProtocol::CTP ? ctpReqMutex_ : rtpReqMutex_;
}

} // namespace hcomm

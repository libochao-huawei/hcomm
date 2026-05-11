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
#include "hccp.h"

#include "hccl_common.h"
#include "exception_handler.h"

namespace hcomm {

namespace {

constexpr uint32_t kTpAttrSlAvailableBit = 18U;
static constexpr uint32_t kTpAttrBitmapSl = (1U << 10U);
static constexpr uint32_t kTpAttrBitmapDscp = (1U << 8U);
static constexpr uint32_t kTpAttrDscpConfigModeBit = 19U;

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
    HCCL_INFO("[TpMgr][%s] nTp[%u] slAvailableCnt[%u] k[%u] numGroups[%u] qos[%u] groupIdx[%u] slotIdx[%u] slMask[0x%x] "
              "tpListIdx[%u] mappedSl[%u] slLevelCount[%u].",
        __func__, nTp, slAvailableCnt, k, numGroups, qos, groupIdx, slotIdx, static_cast<unsigned>(slMask),
        tpListIndexOut,
        mappedSlOut, param.slLevelCount);
    return true;
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
    // 不在入口处命中 infoMap：list/attr 打桩为同步完成；完成后写入 infoMap 供 ReleaseTpInfo 计数。

    std::unique_lock<std::mutex> reqCtxLock(GetReqCtxMutex(tpProtocol));

    auto &reqCtxMap = GetReqCtxMap(tpProtocol);
    Hccl::IpAddress locAddr{};
    Hccl::IpAddress rmtAddr{};
    CHK_RET(CommAddrToIpAddress(param.locAddr, locAddr));
    CHK_RET(CommAddrToIpAddress(param.rmtAddr, rmtAddr));

    auto &locReqCtxMap = reqCtxMap[locAddr];
    auto locReqCtxIter = locReqCtxMap.find(rmtAddr);
    if (locReqCtxIter == locReqCtxMap.end()) {
        HCCL_INFO("[TpMgr][%s] get new tpInfo, param[%s].", __func__, param.Describe().c_str());

        RequestCtx &reqCtx = locReqCtxMap[rmtAddr];
        CHK_RET(StartGetTpInfoListRequest(param, reqCtx));
        return HcclResult::HCCL_E_AGAIN; // 首次触发异步接口调用，动作一定未完成
    }

    auto &reqCtx = locReqCtxIter->second;
    auto ret = CheckRequestResult(reqCtx.handle);
    if (ret == HcclResult::HCCL_E_AGAIN) {
        return ret;
    }
    CHK_RET(ret);

    if (reqCtx.phase == ReqPhase::WAIT_LIST) {
        if (reqCtx.tpInfoNum == 0U) {
            locReqCtxMap.erase(locReqCtxIter);
            reqCtxLock.unlock();
            HCCL_WARNING("[TpMgr][%s] failed to find tp info, tpInfoNum is 0, param[%s].", __func__,
                param.Describe().c_str());
            return HcclResult::HCCL_E_NOT_FOUND;
        }
        CHK_RET(StartGetTpAttrForFirstTp(param, reqCtx));
        return HcclResult::HCCL_E_AGAIN;
    }

    RequestCtx completedReqCtx = std::move(locReqCtxIter->second);
    locReqCtxMap.erase(locReqCtxIter); // 删除已经完成的请求，避免下次申请错误复用
    reqCtxLock.unlock();

    return HandleCompletedRequest(std::move(completedReqCtx), param, tpInfo);
}

HcclResult TpMgr::ReleaseTpInfo(const GetTpInfoParam &param, const TpInfo &tpInfo)
{
    Hccl::IpAddress locAddr{}, rmtAddr{};
    CHK_RET(CommAddrToIpAddress(param.locAddr, locAddr));
    CHK_RET(CommAddrToIpAddress(param.rmtAddr, rmtAddr));

    std::lock_guard<std::mutex> lock(GetInfoCtxMutex(param.tpProtocol));
    auto &infoMap = GetInfoCtxMap(param.tpProtocol);
    auto &locInfoMap = infoMap[locAddr];
    auto locInfoIter = locInfoMap.find(rmtAddr);
    if (locInfoIter == locInfoMap.end()) {
        HCCL_ERROR("[TpMgr][%s] failed, tp info is not found, "
            "param[%s].", __func__, param.Describe().c_str());
        return HcclResult::HCCL_E_NOT_FOUND;
    }

    if (tpInfo.tpHandle != locInfoIter->second.tpInfo.tpHandle) {
        HCCL_ERROR("[TpMgr][%s] failed, tp info[%llu] is not expected[%llu].",
            __func__, tpInfo.tpHandle, locInfoIter->second.tpInfo.tpHandle);
        return HcclResult::HCCL_E_PARA;
    }

    if (locInfoIter->second.useCnt > 1) {
        locInfoIter->second.useCnt -= 1;
        return HcclResult::HCCL_SUCCESS;
    }

    locInfoMap.erase(locInfoIter);
    // 当前 ub 在 unimport jetty 时通过引用计数管理释放 tp handle
    return HcclResult::HCCL_SUCCESS;
}

HcclResult TpMgr::StartGetTpInfoListRequest(const GetTpInfoParam &param, RequestCtx &reqCtx) const
{
    EXCEPTION_HANDLE_BEGIN
    reqCtx.phase = ReqPhase::WAIT_LIST;
    reqCtx.tpAttrBitmap = 0;
    (void)memset_s(&reqCtx.tpAttr, sizeof(reqCtx.tpAttr), 0, sizeof(reqCtx.tpAttr));

    /* 打桩：删 RaGetTpInfoListAsync，直接填一条 list；handle=0 同步完成 */
    constexpr uint32_t kTpBufSlots = 8U;
    reqCtx.dataBuffer.resize(static_cast<size_t>(kTpBufSlots) * sizeof(struct HccpTpInfo));
    (void)memset_s(reqCtx.dataBuffer.data(), reqCtx.dataBuffer.size(), 0, reqCtx.dataBuffer.size());
    auto *infos = reinterpret_cast<struct HccpTpInfo *>(reqCtx.dataBuffer.data());
    infos[0].tpHandle = 1ULL;
    reqCtx.tpInfoNum = 1;
    reqCtx.handle = 0;
    (void)param;
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

    /* 打桩：删 RaGetTpAttrAsync；slBitmap=0x000E、dscpConfigMode=0；handle=0 同步完成 */
    reqCtx.tpAttr.slBitmap = 0x000EU;
    reqCtx.tpAttr.dscpConfigMode = 0;
    reqCtx.handle = 0;
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
    uint16_t slMask = ReadSlAvailableMask16(reqCtx.tpAttr); //得到slBitmap
    uint32_t slAvailableCnt = CalSlAvailableCnt(slMask); //得到sl的数量
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

    TpInfo tmpTpInfo{};
    tmpTpInfo.tpHandle = baseInfoPtr[tpListIndex].tpHandle;
    tmpTpInfo.mappedJettyPriority = mappedSl & 0xFU;
    tmpTpInfo.hasMappedJettyPriority = true;

    HCCL_INFO("[TpMgr][%s] tp qos mapping ok: tpHandle[%llu] tpListIndex[%u] mappedSl[%u] jettyPriority[%u] qos[%u] param[%s].",
        __func__, tmpTpInfo.tpHandle, tpListIndex, static_cast<unsigned>(mappedSl & 0xFU), tmpTpInfo.mappedJettyPriority,
        param.qos & 0xFFU, param.Describe().c_str());

    Hccl::IpAddress locAddr{};
    Hccl::IpAddress rmtAddr{};
    CHK_RET(CommAddrToIpAddress(param.locAddr, locAddr));
    CHK_RET(CommAddrToIpAddress(param.rmtAddr, rmtAddr));

    std::lock_guard<std::mutex> lock(GetInfoCtxMutex(param.tpProtocol));
    auto &infoMap = GetInfoCtxMap(param.tpProtocol);
    auto &locInfoMap = infoMap[locAddr];
    auto rmtIt = locInfoMap.find(rmtAddr);
    if (rmtIt != locInfoMap.end() && rmtIt->second.tpInfo.tpHandle == tmpTpInfo.tpHandle) {
        rmtIt->second.useCnt += 1;
        tpInfo = rmtIt->second.tpInfo;
    } else {
        locInfoMap[rmtAddr] = TpInfoCtx{tmpTpInfo, 1};
        tpInfo = locInfoMap[rmtAddr].tpInfo;
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

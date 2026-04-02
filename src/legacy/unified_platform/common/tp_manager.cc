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
#include <cstdint>

#include "exception_util.h"
#include "hccl_common_v2.h"
#include "invalid_params_exception.h"

#include "hccp_ctx.h"
#include "hccp_async_ctx.h"
#include "orion_adapter_rts.h"
#include "rdma_handle_manager.h"

namespace Hccl {

constexpr uint32_t kTpAttrSlAvailableBit = 12U;

namespace {
uint32_t TpInfoCacheKey(const RaUbGetTpInfoParam &param)
{
    return param.qos & 0xFFU;
}

struct UbcQosTpSlPolicyInput {
    /** 通信域 QoS（0–7），决定 slotIdx 从而在 sl_available 允许集合中选 SL */
    uint32_t qos{0};
    uint32_t nTp{0};
    uint32_t mSlLevels{8};
    uint16_t slAvailableMask{0};
};

struct UbcQosTpSlPolicyOutput {
    uint8_t sl{0};
    uint32_t tpListIndex{0};
    bool valid{false};
};

uint32_t NumGroupsForUsableSlotCount(uint32_t usableSlotCount)
{
    if (usableSlotCount <= 1U) {
        return 1U;
    }
    if (usableSlotCount == 2U) {
        return 2U;
    }
    if (usableSlotCount == 3U) {
        return 3U;
    }
    if (usableSlotCount >= 4U && usableSlotCount <= 7U) {
        return 4U;
    }
    return 8U;
}

uint32_t QoSGroupIndex(uint32_t qos0To7, uint32_t numGroups)
{
    const uint32_t q = std::min(qos0To7, 7U);
    switch (numGroups) {
        case 1U:
            return 0U;
        case 2U:
            return q <= 3U ? 0U : 1U;
        case 3U:
            if (q <= 2U) {
                return 0U;
            }
            if (q <= 4U) {
                return 1U;
            }
            return 2U;
        case 4U:
            return q / 2U;
        case 8U:
            return q;
        default:
            return std::min(q, numGroups - 1U);
    }
}

uint32_t PopCount32(uint32_t v)
{
    uint32_t c = 0;
    while (v != 0U) {
        v &= v - 1U;
        ++c;
    }
    return c;
}

static uint8_t SlValueAtRankInMask16(uint16_t mask, uint32_t rank)
{
    uint32_t n = 0U;
    for (uint32_t b = 0U; b < 16U; ++b) {
        if ((mask & (1U << b)) != 0U) {
            if (n == rank) {
                return static_cast<uint8_t>(b);
            }
            ++n;
        }
    }
    return 0U;
}

/** 16bit：bit i==1 表示可选用 SL=i（0–15）作 priority */
static uint16_t ReadSlAvailableMask16(const struct TpAttr &attr, uint32_t rxAttrBitmap)
{
    if ((rxAttrBitmap & (1U << kTpAttrSlAvailableBit)) == 0U) {
        return 0U;
    }
    return static_cast<uint16_t>(static_cast<uint32_t>(attr.slAvailable[0]) |
        (static_cast<uint32_t>(attr.slAvailable[1]) << 8));
}

static uint32_t SlLevelCountFromSlAvailableField(const struct TpAttr &attr, uint32_t rxAttrBitmap)
{
    const uint32_t mask = static_cast<uint32_t>(ReadSlAvailableMask16(attr, rxAttrBitmap));
    const uint32_t pc = PopCount32(mask);
    if (pc == 0U) {
        return 0U;
    }
    return std::min(pc, 16U);
}

/**
 * sl_available 划定允许 SL；qos 经分组得到 slotIdx，再取掩码中第 slotIdx 个置位（SL 升序）为结果。
 * 例：仅 bit3、bit14→slotIdx 0 为 3、1 为 14（slotIdx 由 qos 等推出）。
 */
void ApplyUbcQosTpSlPolicy(const UbcQosTpSlPolicyInput &in, UbcQosTpSlPolicyOutput &out)
{
    out.valid = false;
    if (in.nTp == 0U || in.mSlLevels == 0U || in.slAvailableMask == 0U) {
        return;
    }

    const uint32_t pc = PopCount32(static_cast<uint32_t>(in.slAvailableMask));
    if (pc == 0U) {
        return;
    }

    const uint32_t usableSlotCount = std::min(in.nTp, in.mSlLevels);
    const uint32_t numGroups = NumGroupsForUsableSlotCount(usableSlotCount);
    const uint32_t qosGroupIndex = QoSGroupIndex(in.qos, numGroups);
    const uint32_t priority = (numGroups > 1U) ? (numGroups - 1U - qosGroupIndex) : 0U;
    const uint32_t slotIdx = std::min(priority, usableSlotCount - 1U);
    const uint32_t tpIdx = std::min(slotIdx, in.nTp - 1U);

    if (slotIdx >= pc) {
        return;
    }

    out.tpListIndex = tpIdx;
    out.sl = SlValueAtRankInMask16(in.slAvailableMask, slotIdx);
    out.valid = true;
}
} // namespace

TpManager &TpManager::GetInstance(const int32_t deviceLogicId)
{
    static TpManager tpManager[MAX_MODULE_DEVICE_NUM];

    if (deviceLogicId < 0 ||
        static_cast<uint32_t>(deviceLogicId) >= MAX_MODULE_DEVICE_NUM) {
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

    devPhyId = HrtGetDevicePhyIdByIndex(static_cast<uint32_t>(devLogicId));
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

static HcclResult CheckTpProtocol(const TpProtocol tpProtocol)
{
    if (tpProtocol != TpProtocol::CTP && tpProtocol != TpProtocol::TP && tpProtocol != TpProtocol::UBOE) {
        HCCL_WARNING("[TpManager][%s] failed, tpProtocol[%d] is not supported.",
            __func__, tpProtocol);
        return HcclResult::HCCL_E_NOT_SUPPORT;
    }

    return HcclResult::HCCL_SUCCESS;
}

HcclResult TpManager::GetTpInfo(const RaUbGetTpInfoParam &param, TpInfo &tpInfo)
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
    const uint32_t qosKey = TpInfoCacheKey(param);
    auto &locReqCtxMap = reqCtxMap[locAddr];
    auto &rmtReqMap = locReqCtxMap[rmtAddr];
    auto locReqCtxIter = rmtReqMap.find(qosKey);
    if (locReqCtxIter == rmtReqMap.end()) {
        HCCL_INFO("[TpManager][%s] get new tpInfo, param[%s].", __func__,
            param.Describe().c_str());

        RequestCtx &reqCtx = rmtReqMap[qosKey];
        StartGetTpInfoListRequest(param, reqCtx);
        return HcclResult::HCCL_E_AGAIN;
    }

    auto &reqCtx = locReqCtxIter->second;
    if (!CheckRequestResult(reqCtx.handle)) {
        return HcclResult::HCCL_E_AGAIN;
    }

    HcclResult pipelineRet = HcclResult::HCCL_SUCCESS;

    if (reqCtx.reqPhase == TpInfoReqPhase::kWaitList) {
        if (reqCtx.tpInfoNum > 0U) {
            const HcclResult attrRet = StartGetTpAttrForFirstTp(param, reqCtx);
            if (attrRet == HcclResult::HCCL_SUCCESS) {
                return HcclResult::HCCL_E_AGAIN;
            }
            if (attrRet != HcclResult::HCCL_SUCCESS) {
                pipelineRet = attrRet;
            }
        }
    } else if (reqCtx.reqPhase == TpInfoReqPhase::kWaitTpAttr) {
        uint32_t mFromAttr = 0U;
        if (reqCtx.tpAttrBuf.size() >= sizeof(struct TpAttr)) {
            const struct TpAttr *tpAttr =
                reinterpret_cast<const struct TpAttr *>(reqCtx.tpAttrBuf.data());
            mFromAttr = SlLevelCountFromSlAvailableField(*tpAttr, reqCtx.tpAttrBitmap);
        }
        if (mFromAttr == 0U) {
            HCCL_ERROR("[TpManager][%s] sl_available (attr 12) missing or all-zero, param[%s].", __func__,
                param.Describe().c_str());
            pipelineRet = HcclResult::HCCL_E_INTERNAL;
        } else {
            if (param.slLevelCount != 0U && mFromAttr > param.slLevelCount) {
                mFromAttr = param.slLevelCount;
            }
            reqCtx.resolvedSlLevelCount = mFromAttr;
        }
    }

    RequestCtx completedReqCtx = locReqCtxIter->second;
    rmtReqMap.erase(locReqCtxIter);
    if (rmtReqMap.empty()) {
        locReqCtxMap.erase(rmtAddr);
    }
    if (locReqCtxMap.empty()) {
        reqCtxMap.erase(locAddr);
    }
    reqCtxLock.unlock();

    if (pipelineRet != HcclResult::HCCL_SUCCESS) {
        return pipelineRet;
    }
    return HandleCompletedRequest(std::move(completedReqCtx), param, tpInfo);
}

HcclResult TpManager::ReleaseTpInfo(const RaUbGetTpInfoParam &param, const TpInfo &tpInfo)
{
    const uint32_t qosKey = TpInfoCacheKey(param);
    std::lock_guard<std::mutex> lock(GetInfoCtxMutex(param.tpProtocol));
    auto &infoMap = GetInfoCtxMap(param.tpProtocol);
    auto locInfoIter = infoMap.find(param.locAddr);
    if (locInfoIter == infoMap.end()) {
        HCCL_ERROR("[TpManager][%s] failed, tp info is not found, "
            "param[%s].", __func__, param.Describe().c_str());
        return HcclResult::HCCL_E_NOT_FOUND;
    }
    auto rmtInfoIter = locInfoIter->second.find(param.rmtAddr);
    if (rmtInfoIter == locInfoIter->second.end()) {
        HCCL_ERROR("[TpManager][%s] failed, tp info is not found, "
            "param[%s].", __func__, param.Describe().c_str());
        return HcclResult::HCCL_E_NOT_FOUND;
    }
    auto qosInfoIter = rmtInfoIter->second.find(qosKey);
    if (qosInfoIter == rmtInfoIter->second.end()) {
        HCCL_ERROR("[TpManager][%s] failed, tp info is not found, "
            "param[%s].", __func__, param.Describe().c_str());
        return HcclResult::HCCL_E_NOT_FOUND;
    }

    if (tpInfo.tpHandle != qosInfoIter->second.tpInfo.tpHandle) {
        HCCL_ERROR("[TpManager][%s] failed, tp info[%llu] is not expected[%llu].",
            __func__, tpInfo.tpHandle, qosInfoIter->second.tpInfo.tpHandle);
        return HcclResult::HCCL_E_PARA;
    }

    if (qosInfoIter->second.useCnt > 1) {
        qosInfoIter->second.useCnt -= 1;
        return HcclResult::HCCL_SUCCESS;
    }

    rmtInfoIter->second.erase(qosInfoIter);
    if (rmtInfoIter->second.empty()) {
        locInfoIter->second.erase(rmtInfoIter);
    }
    if (locInfoIter->second.empty()) {
        infoMap.erase(locInfoIter);
    }
    return HcclResult::HCCL_SUCCESS;
}

bool TpManager::FindAndGetTpInfo(const RaUbGetTpInfoParam &param, TpInfo &tpInfo)
{
    const uint32_t qosKey = TpInfoCacheKey(param);
    std::lock_guard<std::mutex> lock(GetInfoCtxMutex(param.tpProtocol));
    auto &infoMap = GetInfoCtxMap(param.tpProtocol);
    auto locInfoIter = infoMap.find(param.locAddr);
    if (locInfoIter == infoMap.end()) {
        return false;
    }
    auto rmtInfoIter = locInfoIter->second.find(param.rmtAddr);
    if (rmtInfoIter == locInfoIter->second.end()) {
        return false;
    }
    auto qosInfoIter = rmtInfoIter->second.find(qosKey);
    if (qosInfoIter != rmtInfoIter->second.end()) {
        qosInfoIter->second.useCnt += 1;
        tpInfo = qosInfoIter->second.tpInfo;
        return true;
    }

    return false;
}

void TpManager::StartGetTpInfoListRequest(const RaUbGetTpInfoParam &param,
    TpManager::RequestCtx &reqCtx) const
{
    RdmaHandle rdmaHandle =
        RdmaHandleManager::GetInstance().GetByIp(devPhyId, param.locAddr);
    if (!rdmaHandle) {
        THROW<InternalException>("[TpManager][%s] can not find rdmaHandle, "
            "devPhyId[%u] locAddr[%s].", __func__, devPhyId,
            param.locAddr.Describe().c_str());
    }
    reqCtx.reqPhase = TpInfoReqPhase::kWaitList;
    reqCtx.resolvedSlLevelCount = 0U;
    reqCtx.tpAttrBitmap = 0U;
    reqCtx.tpAttrBuf.clear();
    reqCtx.tpInfoNum = HCCP_MAX_TPID_INFO_NUM;
    reqCtx.handle = RaUbGetTpInfoAsync(rdmaHandle, param, reqCtx.dataBuffer,
        reqCtx.tpInfoNum);
}

HcclResult TpManager::StartGetTpAttrForFirstTp(const RaUbGetTpInfoParam &param, RequestCtx &reqCtx)
{
    RdmaHandle rdmaHandle =
        RdmaHandleManager::GetInstance().GetByIp(devPhyId, param.locAddr);
    if (!rdmaHandle) {
        HCCL_WARNING("[TpManager][%s] no rdmaHandle for get_tp_attr.", __func__);
        return HcclResult::HCCL_E_NETWORK;
    }

    const struct HccpTpInfo *infos = reinterpret_cast<const struct HccpTpInfo *>(reqCtx.dataBuffer.data());
    const uint64_t firstHandle = infos[0].tpHandle;

    reqCtx.tpAttrBitmap = (1U << kTpAttrSlAvailableBit);
    reqCtx.tpAttrBuf.assign(sizeof(struct TpAttr), 0);
    auto *tpAttrPtr = reinterpret_cast<struct TpAttr *>(reqCtx.tpAttrBuf.data());

    void *raReqHandle = nullptr;
    const s32 r = RaGetTpAttrAsync(rdmaHandle, firstHandle, &reqCtx.tpAttrBitmap, tpAttrPtr, &raReqHandle);
    if (r != 0 || raReqHandle == nullptr) {
        HCCL_WARNING("[TpManager][%s] RaGetTpAttrAsync failed ret[%d] firstTpHandle[%llu].",
            __func__, r, static_cast<unsigned long long>(firstHandle));
        return HcclResult::HCCL_E_NETWORK;
    }

    reqCtx.handle = reinterpret_cast<RequestHandle>(raReqHandle);
    reqCtx.reqPhase = TpInfoReqPhase::kWaitTpAttr;
    HCCL_INFO("[TpManager][%s] get_tp_attr pending for first tp_handle[%llu].", __func__,
        static_cast<unsigned long long>(firstHandle));
    return HcclResult::HCCL_SUCCESS;
}

inline TpInfo ParseTpInfo(const struct HccpTpInfo *infoPtr)
{
    TpInfo tpInfo;
    tpInfo.tpHandle = infoPtr->tpHandle;
    return tpInfo;
}

HcclResult TpManager::HandleCompletedRequest(TpManager::RequestCtx reqCtx,
    const RaUbGetTpInfoParam &param, TpInfo &tpInfo)
{
    const uint32_t tpInfoNum = reqCtx.tpInfoNum;
    if (tpInfoNum == 0) {
        HCCL_WARNING("[TpManager][%s] failed to find tp info, tpInfoNum is 0, "
            "param[%s].", __func__, param.Describe().c_str());
        return HcclResult::HCCL_E_NOT_FOUND;
    }

    const struct HccpTpInfo *baseInfoPtr =
        reinterpret_cast<const struct HccpTpInfo *>(reqCtx.dataBuffer.data());

    TpInfo tmpTpInfo = ParseTpInfo(baseInfoPtr);
    if (param.loopFirstTpLowestSl) {
        tmpTpInfo.tpHandle = baseInfoPtr[0].tpHandle;
        if (reqCtx.tpAttrBuf.size() < sizeof(struct TpAttr)) {
            HCCL_ERROR("[TpManager][%s] loop path requires TpAttr with sl_available, param[%s].", __func__,
                param.Describe().c_str());
            return HcclResult::HCCL_E_INTERNAL;
        }
        const struct TpAttr *tpAttr =
            reinterpret_cast<const struct TpAttr *>(reqCtx.tpAttrBuf.data());
        const uint16_t slMask = ReadSlAvailableMask16(*tpAttr, reqCtx.tpAttrBitmap);
        if (slMask == 0U) {
            HCCL_ERROR("[TpManager][%s] loop path requires non-zero sl_available, param[%s].", __func__,
                param.Describe().c_str());
            return HcclResult::HCCL_E_INTERNAL;
        }
        tmpTpInfo.mappedJettyPriority = SlValueAtRankInMask16(slMask, 0U);
        tmpTpInfo.hasMappedJettyPriority = true;
    } else {
        const uint32_t mSlLevels = reqCtx.resolvedSlLevelCount;
        if (mSlLevels == 0U) {
            HCCL_ERROR("[TpManager][%s] resolvedSlLevelCount is 0 (sl_available), param[%s].", __func__,
                param.Describe().c_str());
            return HcclResult::HCCL_E_INTERNAL;
        }
        uint32_t mCap = mSlLevels;
        if (param.slLevelCount != 0U && mCap > param.slLevelCount) {
            mCap = param.slLevelCount;
        }
        if (reqCtx.tpAttrBuf.size() < sizeof(struct TpAttr)) {
            HCCL_ERROR("[TpManager][%s] missing TpAttr buffer for sl_available, param[%s].", __func__,
                param.Describe().c_str());
            return HcclResult::HCCL_E_INTERNAL;
        }
        const struct TpAttr *tpAttr =
            reinterpret_cast<const struct TpAttr *>(reqCtx.tpAttrBuf.data());
        const uint16_t slMask = ReadSlAvailableMask16(*tpAttr, reqCtx.tpAttrBitmap);
        if (slMask == 0U) {
            HCCL_ERROR("[TpManager][%s] sl_available mask is zero, param[%s].", __func__,
                param.Describe().c_str());
            return HcclResult::HCCL_E_INTERNAL;
        }
        UbcQosTpSlPolicyInput pin{};
        pin.qos = param.qos;
        pin.nTp = tpInfoNum;
        pin.mSlLevels = mCap;
        pin.slAvailableMask = slMask;
        UbcQosTpSlPolicyOutput pout{};
        ApplyUbcQosTpSlPolicy(pin, pout);
        if (!pout.valid || pout.tpListIndex >= tpInfoNum) {
            HCCL_ERROR("[TpManager][%s] UBC QoS→SL/TP mapping required but policy invalid, "
                "param[%s] tpInfoNum[%u] valid[%d] tpListIndex[%u].", __func__,
                param.Describe().c_str(), tpInfoNum, static_cast<int>(pout.valid),
                pout.tpListIndex);
            return HcclResult::HCCL_E_INTERNAL;
        }
        tmpTpInfo.tpHandle = baseInfoPtr[pout.tpListIndex].tpHandle;
        tmpTpInfo.mappedJettyPriority = pout.sl;
        tmpTpInfo.hasMappedJettyPriority = true;
    }

    const uint32_t qosKey = TpInfoCacheKey(param);
    std::lock_guard<std::mutex> lock(GetInfoCtxMutex(param.tpProtocol));
    auto &infoMap = GetInfoCtxMap(param.tpProtocol);
    infoMap[param.locAddr][param.rmtAddr][qosKey] = {std::move(tmpTpInfo), 1};

    tpInfo = infoMap[param.locAddr][param.rmtAddr][qosKey].tpInfo;
    return HcclResult::HCCL_SUCCESS;
}

TpManager::InfoCtxMap &TpManager::GetInfoCtxMap(const TpProtocol tpProtocol)
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

TpManager::ReqCtxMap &TpManager::GetReqCtxMap(const TpProtocol tpProtocol)
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

std::mutex &TpManager::GetInfoCtxMutex(const TpProtocol tpProtocol)
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

std::mutex &TpManager::GetReqCtxMutex(const TpProtocol tpProtocol)
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

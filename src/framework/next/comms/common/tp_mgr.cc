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
#include <cstdint>

#include "hccp_ctx.h"
#include "hccp_async_ctx.h"

#include "hccl_common.h"
#include "exception_handler.h"
#include "rdma_handle_manager.h"

namespace hcomm {

namespace {
constexpr uint32_t kTpInfoLegacyQosKey = UINT32_MAX;

/** 映射路径：commHcclQos → 同 EID 对、同 QoS 档复用同一 TPID/SL；legacy：全挤在单一键 */
uint32_t TpInfoCacheKey(const GetTpInfoParam &param)
{
    if (!param.useUbTpSlMapping) {
        return kTpInfoLegacyQosKey;
    }
    return param.commHcclQos & 0xFFU;
}

struct UbcQosTpSlPolicyInput {
    uint32_t commHcclQos{0};
    uint32_t nTp{0};
    uint32_t mSlLevels{8};
};

struct UbcQosTpSlPolicyOutput {
    uint8_t sl{0};
    uint32_t tpListIndex{0};
    bool valid{false};
};

/** 按「可同时用到的 TP/SL 档位数」决定 hcclQoS 0–7 分成几组 */
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

constexpr uint32_t kTpAttrSlBitmapBit = 10U;
constexpr uint32_t kTpAttrStandardBitsMask = (1U << 12U) - 1U;

uint32_t PopCount32(uint32_t v)
{
    uint32_t c = 0;
    while (v != 0U) {
        v &= v - 1U;
        ++c;
    }
    return c;
}

/** 由 urma_get_tp_attr 回填的 attrBitmap（低 12 bit 与 URMA 文档一致）推导 SL 维度的 M */
uint32_t SlLevelCountFromTpAttrRxBitmap(uint32_t rxAttrBitmap)
{
    const uint32_t bits = rxAttrBitmap & kTpAttrStandardBitsMask;
    const uint32_t pc = PopCount32(bits);
    if (pc == 0U) {
        return 8U;
    }
    if (pc == 1U && ((bits & (1U << kTpAttrSlBitmapBit)) != 0U)) {
        return 16U;
    }
    return std::min(pc, 16U);
}

/**
 * 假定 urma_get_tp_list 在同 (EID 对, CTP|RTP) 下多次调用结果稳定，且列表顺序为 SL 从小到大
 * （下标 i 对应第 i 低 SL 槽位的 TP）；slotIdx 与列表下标、策略 sl 槽位一致。
 */
void ApplyUbcQosTpSlPolicy(const UbcQosTpSlPolicyInput &in, UbcQosTpSlPolicyOutput &out)
{
    out.valid = false;
    if (in.nTp == 0U || in.mSlLevels == 0U) {
        return;
    }

    const uint32_t usableSlotCount = std::min(in.nTp, in.mSlLevels);
    const uint32_t numGroups = NumGroupsForUsableSlotCount(usableSlotCount);
    const uint32_t hcclQosGroupIndex = QoSGroupIndex(in.commHcclQos, numGroups);
    const uint32_t priority = (numGroups > 1U) ? (numGroups - 1U - hcclQosGroupIndex) : 0U;
    const uint32_t slotIdx = std::min(priority, usableSlotCount - 1U);
    const uint32_t tpIdx = std::min(slotIdx, in.nTp - 1U);

    out.tpListIndex = tpIdx;
    out.sl = static_cast<uint8_t>(std::min(slotIdx, 15U));
    out.valid = true;
}
} // namespace

TpMgr& TpMgr::GetInstance(const uint32_t devicePhyId)
{
    static TpMgr tpMgr[MAX_MODULE_DEVICE_NUM + 1];

    uint32_t devPhyId = devicePhyId;
    if (devPhyId >= MAX_MODULE_DEVICE_NUM) {
        HCCL_WARNING("[TpMgr][%s] use the backup device, devPhyId[%u] should be "
            "less than %u.", __func__, devPhyId, MAX_MODULE_DEVICE_NUM);
        devPhyId = MAX_MODULE_DEVICE_NUM; // 使用备份设备
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
        // 不提供日志避免刷屏
        return HcclResult::HCCL_E_AGAIN;
    }

    if (result != RequestResult::COMPLETED) {
        HCCL_ERROR("[TpMgr][%s] failed, result[%s] is unexpected.",
            __func__, result.Describe().c_str());
        return HcclResult::HCCL_E_NETWORK;
    }

    return HcclResult::HCCL_SUCCESS;
}

HcclResult CheckTpProtocol(const TpProtocol tpProtocol) {
    if (tpProtocol != TpProtocol::CTP && tpProtocol != TpProtocol::RTP) {
        HCCL_ERROR("[TpMgr][%s] failed, tpProtocol[%d] is not supported.",
            __func__, tpProtocol);
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
    Hccl::IpAddress locAddr{}, rmtAddr{};
    CHK_RET(CommAddrToIpAddress(param.locAddr, locAddr));
    CHK_RET(CommAddrToIpAddress(param.rmtAddr, rmtAddr));

    const uint32_t qosKey = TpInfoCacheKey(param);
    auto &locReqCtxMap = reqCtxMap[locAddr];
    auto &rmtReqMap = locReqCtxMap[rmtAddr];
    auto locReqCtxIter = rmtReqMap.find(qosKey);
    if (locReqCtxIter == rmtReqMap.end()) {
        HCCL_INFO("[TpMgr][%s] get new tpInfo, param[%s].", __func__,
            param.Describe().c_str());

        RequestCtx &reqCtx = rmtReqMap[qosKey];
        CHK_RET(StartGetTpInfoListRequest(param, reqCtx));
        return HcclResult::HCCL_E_AGAIN; // 首次触发异步接口调用，动作一定未完成
    }

    auto &reqCtx = locReqCtxIter->second;
    auto ret = CheckRequestResult(reqCtx.handle);
    if (ret == HcclResult::HCCL_E_AGAIN) {
        return ret;
    }
    CHK_RET(ret);

    if (reqCtx.reqPhase == TpInfoReqPhase::kWaitList) {
        if (param.useUbTpSlMapping && reqCtx.tpInfoNum > 0U) {
            const HcclResult attrRet = StartGetTpAttrForFirstTp(devPhyId_, param, reqCtx);
            if (attrRet == HcclResult::HCCL_SUCCESS) {
                return HcclResult::HCCL_E_AGAIN;
            }
            reqCtx.resolvedSlLevelCount = (param.slLevelCount != 0U) ? param.slLevelCount : 8U;
        }
    } else if (reqCtx.reqPhase == TpInfoReqPhase::kWaitTpAttr) {
        uint32_t mFromAttr = SlLevelCountFromTpAttrRxBitmap(reqCtx.tpAttrBitmap);
        if (mFromAttr == 0U) {
            mFromAttr = 8U;
        }
        if (param.slLevelCount != 0U && mFromAttr > param.slLevelCount) {
            mFromAttr = param.slLevelCount;
        }
        reqCtx.resolvedSlLevelCount = mFromAttr;
    }

    RequestCtx completedReqCtx = locReqCtxIter->second; // 深拷贝构造对象，与map解耦
    rmtReqMap.erase(locReqCtxIter); // 删除已经完成的请求，避免下次申请错误复用
    if (rmtReqMap.empty()) {
        locReqCtxMap.erase(rmtAddr);
    }
    if (locReqCtxMap.empty()) {
        reqCtxMap.erase(locAddr);
    }
    reqCtxLock.unlock();

    return HandleCompletedRequest(std::move(completedReqCtx), param, tpInfo);
}

HcclResult TpMgr::ReleaseTpInfo(const GetTpInfoParam &param, const TpInfo &tpInfo)
{
    Hccl::IpAddress locAddr{}, rmtAddr{};
    CHK_RET(CommAddrToIpAddress(param.locAddr, locAddr));
    CHK_RET(CommAddrToIpAddress(param.rmtAddr, rmtAddr));

    const uint32_t qosKey = TpInfoCacheKey(param);
    std::lock_guard<std::mutex> lock(GetInfoCtxMutex(param.tpProtocol));
    auto &infoMap = GetInfoCtxMap(param.tpProtocol);
    auto locInfoIter = infoMap.find(locAddr);
    if (locInfoIter == infoMap.end()) {
        HCCL_ERROR("[TpMgr][%s] failed, tp info is not found, "
            "param[%s].", __func__, param.Describe().c_str());
        return HcclResult::HCCL_E_NOT_FOUND;
    }
    auto rmtInfoIter = locInfoIter->second.find(rmtAddr);
    if (rmtInfoIter == locInfoIter->second.end()) {
        HCCL_ERROR("[TpMgr][%s] failed, tp info is not found, "
            "param[%s].", __func__, param.Describe().c_str());
        return HcclResult::HCCL_E_NOT_FOUND;
    }
    auto qosInfoIter = rmtInfoIter->second.find(qosKey);
    if (qosInfoIter == rmtInfoIter->second.end()) {
        HCCL_ERROR("[TpMgr][%s] failed, tp info is not found, "
            "param[%s].", __func__, param.Describe().c_str());
        return HcclResult::HCCL_E_NOT_FOUND;
    }

    if (tpInfo.tpHandle != qosInfoIter->second.tpInfo.tpHandle) {
        HCCL_ERROR("[TpMgr][%s] failed, tp info[%llu] is not expected[%llu].",
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
    // 当前ub在unimport jetty时通过引用计数管理释放tp handle
    return HcclResult::HCCL_SUCCESS;
}

HcclResult TpMgr::FindAndGetTpInfo(const GetTpInfoParam &param, TpInfo &tpInfo)
{
    Hccl::IpAddress locAddr{}, rmtAddr{};
    CHK_RET(CommAddrToIpAddress(param.locAddr, locAddr));
    CHK_RET(CommAddrToIpAddress(param.rmtAddr, rmtAddr));

    const uint32_t qosKey = TpInfoCacheKey(param);
    std::lock_guard<std::mutex> lock(GetInfoCtxMutex(param.tpProtocol));
    auto &infoMap = GetInfoCtxMap(param.tpProtocol);
    auto locInfoIter = infoMap.find(locAddr);
    if (locInfoIter == infoMap.end()) {
        return HcclResult::HCCL_E_NOT_FOUND;
    }
    auto rmtInfoIter = locInfoIter->second.find(rmtAddr);
    if (rmtInfoIter == locInfoIter->second.end()) {
        return HcclResult::HCCL_E_NOT_FOUND;
    }
    auto qosInfoIter = rmtInfoIter->second.find(qosKey);
    if (qosInfoIter != rmtInfoIter->second.end()) {
        qosInfoIter->second.useCnt += 1;
        tpInfo = qosInfoIter->second.tpInfo;
        return HcclResult::HCCL_SUCCESS;
    }

    return HcclResult::HCCL_E_NOT_FOUND;
}

static HcclResult GetTpInfoAsync(const CtxHandle ctxHandle, const GetTpInfoParam &param,
    std::vector<char> &out, uint32_t &num, RequestHandle &reqHandle)
{
    Hccl::IpAddress locAddr{}, rmtAddr{};
    CHK_RET(CommAddrToIpAddress(param.locAddr, locAddr));
    CHK_RET(CommAddrToIpAddress(param.rmtAddr, rmtAddr));
    const auto &tpProtocol = param.tpProtocol;

    struct GetTpCfg cfg{};
    cfg.flag.bs.rtp = tpProtocol == TpProtocol::RTP ? 1 : 0;
    cfg.flag.bs.ctp = tpProtocol == TpProtocol::CTP ? 1 : 0;
    cfg.transMode = TransportModeT::CONN_RM; // 当前只使用RM Jetty
    CHK_RET(IpAddressToHccpEid(locAddr, cfg.localEid)); // 当前复用orion ip address
    HCCL_INFO("RaUbGetTpInfoAsync cfg.local_eid[subnetPrefix[%016llx], interfaceId[%016llx]]",
              cfg.localEid.in6.subnetPrefix, cfg.localEid.in6.interfaceId);
    CHK_RET(IpAddressToHccpEid(rmtAddr, cfg.peerEid));
    HCCL_INFO("RaUbGetTpInfoAsync cfg.peer_eid[subnetPrefix[%016llx], interfaceId[%016llx]]",
              cfg.peerEid.in6.subnetPrefix, cfg.peerEid.in6.interfaceId);

    out.resize(static_cast<size_t>(HCCP_MAX_TPID_INFO_NUM) * sizeof(HccpTpInfo));
    struct HccpTpInfo *info = reinterpret_cast<struct HccpTpInfo *>(out.data());

    void *raReqHandle = nullptr;
    num = HCCP_MAX_TPID_INFO_NUM; // IN: 缓冲容量；OUT: 实际返回条数
    s32 ret = RaGetTpInfoListAsync(ctxHandle, &cfg, info, &num, &raReqHandle);
    if (ret != 0 || !raReqHandle) {
        HCCL_ERROR("[%s] failed, call interface error[%d] raReqHandle[%p], "
            "ctxHandle[%p] locAddr[%s] rmtAddr[%s].", __func__, ret, raReqHandle, ctxHandle,
            locAddr.Describe().c_str(), rmtAddr.Describe().c_str());
        return HcclResult::HCCL_E_NETWORK;
    }

    reqHandle = reinterpret_cast<RequestHandle>(raReqHandle);
    HCCL_INFO("[%s] get request handle[%llu].", __func__, reqHandle);
    return HcclResult::HCCL_SUCCESS;
}

HcclResult TpMgr::StartGetTpInfoListRequest(const GetTpInfoParam &param,
    TpMgr::RequestCtx &reqCtx) const
{
    EXCEPTION_HANDLE_BEGIN
    reqCtx.reqPhase = TpInfoReqPhase::kWaitList;
    reqCtx.resolvedSlLevelCount = 0U;
    reqCtx.tpAttrBitmap = 0U;
    reqCtx.tpAttrBuf.clear();

    Hccl::IpAddress ipAddr{};
    CHK_RET(CommAddrToIpAddress(param.locAddr, ipAddr));
    const CtxHandle ctxHandle = static_cast<CtxHandle>(
        Hccl::RdmaHandleManager::GetInstance().GetByIp(devPhyId_, ipAddr));
    CHK_PTR_NULL(ctxHandle);

    CHK_RET(GetTpInfoAsync(ctxHandle, param, reqCtx.dataBuffer,
        reqCtx.tpInfoNum, reqCtx.handle));
    EXCEPTION_HANDLE_END
    return HcclResult::HCCL_SUCCESS;
}

HcclResult TpMgr::StartGetTpAttrForFirstTp(uint32_t devPhyId, const GetTpInfoParam &param,
    TpMgr::RequestCtx &reqCtx)
{
    EXCEPTION_HANDLE_BEGIN
    Hccl::IpAddress ipAddr{};
    CHK_RET(CommAddrToIpAddress(param.locAddr, ipAddr));
    const CtxHandle ctxHandle = static_cast<CtxHandle>(
        Hccl::RdmaHandleManager::GetInstance().GetByIp(devPhyId, ipAddr));
    CHK_PTR_NULL(ctxHandle);

    const struct HccpTpInfo *infos = reinterpret_cast<const struct HccpTpInfo *>(reqCtx.dataBuffer.data());
    const uint64_t firstHandle = infos[0].tpHandle;

    reqCtx.tpAttrBitmap = (1U << 10U);
    reqCtx.tpAttrBuf.assign(sizeof(struct TpAttr), 0);
    auto *tpAttrPtr = reinterpret_cast<struct TpAttr *>(reqCtx.tpAttrBuf.data());

    void *raReqHandle = nullptr;
    const s32 r = RaGetTpAttrAsync(ctxHandle, firstHandle, &reqCtx.tpAttrBitmap, tpAttrPtr, &raReqHandle);
    if (r != 0 || raReqHandle == nullptr) {
        HCCL_WARNING("[TpMgr][%s] RaGetTpAttrAsync failed ret[%d] firstTpHandle[%llu], use fallback M.",
            __func__, r, static_cast<unsigned long long>(firstHandle));
        return HcclResult::HCCL_E_NETWORK;
    }

    reqCtx.handle = reinterpret_cast<RequestHandle>(raReqHandle);
    reqCtx.reqPhase = TpInfoReqPhase::kWaitTpAttr;
    HCCL_INFO("[TpMgr][%s] get_tp_attr pending for first tp_handle[%llu].", __func__,
        static_cast<unsigned long long>(firstHandle));
    EXCEPTION_HANDLE_END
    return HcclResult::HCCL_SUCCESS;
}

inline TpInfo ParseTpInfo(const struct HccpTpInfo *infoPtr)
{
    TpInfo tpInfo;
    tpInfo.tpHandle = infoPtr->tpHandle;

    return tpInfo;
}

HcclResult TpMgr::HandleCompletedRequest(const TpMgr::RequestCtx reqCtx,
    const GetTpInfoParam &param, TpInfo &tpInfo)
{
    const uint32_t tpInfoNum = reqCtx.tpInfoNum;
    if (tpInfoNum == 0) {
        HCCL_WARNING("[TpMgr][%s] failed to find tp info, tpInfoNum is 0, "
            "param[%s].", __func__, param.Describe().c_str());
        return HcclResult::HCCL_E_NOT_FOUND;
    }

    const struct HccpTpInfo *baseInfoPtr = // 类的私有变量vector指向的堆内存，不会为空
        reinterpret_cast<const struct HccpTpInfo *>(reqCtx.dataBuffer.data());

    TpInfo tmpTpInfo = ParseTpInfo(baseInfoPtr);
    if (param.useUbTpSlMapping) {
        if (param.loopFirstTpLowestSl) {
            tmpTpInfo.tpHandle = baseInfoPtr[0].tpHandle;
            tmpTpInfo.mappedJettyPriority = 0U;
            tmpTpInfo.hasMappedJettyPriority = true;
        } else {
            uint32_t mSlLevels = reqCtx.resolvedSlLevelCount;
            if (mSlLevels == 0U) {
                mSlLevels = param.slLevelCount != 0U ? param.slLevelCount : 8U;
            } else if (param.slLevelCount != 0U && mSlLevels > param.slLevelCount) {
                mSlLevels = param.slLevelCount;
            }
            UbcQosTpSlPolicyInput pin{};
            pin.commHcclQos = param.commHcclQos;
            pin.nTp = tpInfoNum;
            pin.mSlLevels = mSlLevels;
            UbcQosTpSlPolicyOutput pout{};
            ApplyUbcQosTpSlPolicy(pin, pout);
            if (!pout.valid || pout.tpListIndex >= tpInfoNum) {
                HCCL_ERROR("[TpMgr][%s] UBC QoS→SL/TP mapping required but policy invalid, "
                    "param[%s] tpInfoNum[%u] valid[%d] tpListIndex[%u].", __func__,
                    param.Describe().c_str(), tpInfoNum, static_cast<int>(pout.valid),
                    pout.tpListIndex);
                return HcclResult::HCCL_E_INTERNAL;
            }
            tmpTpInfo.tpHandle = baseInfoPtr[pout.tpListIndex].tpHandle;
            tmpTpInfo.mappedJettyPriority = pout.sl;
            tmpTpInfo.hasMappedJettyPriority = true;
        }
    }

    Hccl::IpAddress locAddr{}, rmtAddr{};
    (void)CommAddrToIpAddress(param.locAddr, locAddr);
    (void)CommAddrToIpAddress(param.rmtAddr, rmtAddr);

    const uint32_t qosKey = TpInfoCacheKey(param);
    std::lock_guard<std::mutex> lock(GetInfoCtxMutex(param.tpProtocol));
    auto &infoMap = GetInfoCtxMap(param.tpProtocol);
    infoMap[locAddr][rmtAddr][qosKey] = {std::move(tmpTpInfo), 1};

    tpInfo = infoMap[locAddr][rmtAddr][qosKey].tpInfo;
    return HcclResult::HCCL_SUCCESS;
}

TpMgr::InfoCtxMap& TpMgr::GetInfoCtxMap(const TpProtocol tpProtocol)
{
    return tpProtocol == TpProtocol::CTP ? ctpInfoMap_ : rtpInfoMap_;
}

TpMgr::ReqCtxMap& TpMgr::GetReqCtxMap(const TpProtocol tpProtocol)
{
    return tpProtocol == TpProtocol::CTP ? ctpReqMap_ : rtpReqMap_;
}

std::mutex& TpMgr::GetInfoCtxMutex(const TpProtocol tpProtocol)
{
    return tpProtocol == TpProtocol::CTP ? ctpInfoMutex_ : rtpInfoMutex_;
}

std::mutex& TpMgr::GetReqCtxMutex(const TpProtocol tpProtocol)
{
    return tpProtocol == TpProtocol::CTP ? ctpReqMutex_ : rtpReqMutex_;
}

} // namespace hcomm

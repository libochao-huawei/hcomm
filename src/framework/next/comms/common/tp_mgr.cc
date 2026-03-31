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

/** urma/hccp tp_attr_bitmap：属性 12 = sl_available（与 urma_api.h 一致） */
constexpr uint32_t kTpAttrSlAvailableBit = 12U;

namespace {
constexpr uint32_t kTpInfoLegacyQosKey = UINT32_MAX;

/** 映射路径：qos → 同 EID 对、同 QoS 档复用同一 TPID/SL；legacy：全挤在单一键 */
uint32_t TpInfoCacheKey(const GetTpInfoParam &param)
{
    if (!param.useUbTpSlMapping) {
        return kTpInfoLegacyQosKey;
    }
    return param.qos & 0xFFU;
}

struct UbcQosTpSlPolicyInput {
    /** 通信域 QoS（0–7），与 NumGroups/QoSGroupIndex 一起决定 slotIdx，从而在 sl_available 允许集合里选具体 SL */
    uint32_t qos{0};
    uint32_t nTp{0};
    uint32_t mSlLevels{8};
    /** get_tp_attr 属性 12：可用 SL 位图（低 16 bit） */
    uint16_t slAvailableMask{0};
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

uint32_t PopCount32(uint32_t v)
{
    uint32_t c = 0;
    while (v != 0U) {
        v &= v - 1U;
        ++c;
    }
    return c;
}

/** mask 低 16 位：第 rank 个被置位的 SL 编号（从低到高，rank 从 0 开始） */
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

/** 返回 16bit：bit i==1 表示可选用 SL=i（0–15）作 Jetty/UB priority */
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
 * sl_available 只划定「允许哪些 SL」（bit i=1 即可用 SL=i）。
 * 具体落到哪一个 SL 由 qos 决定：先据可用档位数 K 将 0–7 映射到组索引，再得到 slotIdx，最后
 * SlValueAtRankInMask16(mask, slotIdx) = 允许集合中第 slotIdx 个 SL（升序）。例：仅 bit3、bit14 置位时，
 * slotIdx==0→3，slotIdx==1→14；slotIdx 随 qos 而变。
 * 假定 get_tp_list 下标与上述 SL 升序档位一一对应。
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

    HcclResult pipelineRet = HcclResult::HCCL_SUCCESS;

    if (reqCtx.reqPhase == TpInfoReqPhase::kWaitList) {
        if (param.useUbTpSlMapping && reqCtx.tpInfoNum > 0U) {
            const HcclResult attrRet = StartGetTpAttrForFirstTp(devPhyId_, param, reqCtx);
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
            HCCL_ERROR("[TpMgr][%s] sl_available (attr 12) missing or all-zero, param[%s].", __func__,
                param.Describe().c_str());
            pipelineRet = HcclResult::HCCL_E_INTERNAL;
        } else {
            if (param.slLevelCount != 0U && mFromAttr > param.slLevelCount) {
                mFromAttr = param.slLevelCount;
            }
            reqCtx.resolvedSlLevelCount = mFromAttr;
        }
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

    if (pipelineRet != HcclResult::HCCL_SUCCESS) {
        return pipelineRet;
    }
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

    reqCtx.tpAttrBitmap = (1U << kTpAttrSlAvailableBit);
    reqCtx.tpAttrBuf.assign(sizeof(struct TpAttr), 0);
    auto *tpAttrPtr = reinterpret_cast<struct TpAttr *>(reqCtx.tpAttrBuf.data());

    void *raReqHandle = nullptr;
    const s32 r = RaGetTpAttrAsync(ctxHandle, firstHandle, &reqCtx.tpAttrBitmap, tpAttrPtr, &raReqHandle);
    if (r != 0 || raReqHandle == nullptr) {
        HCCL_WARNING("[TpMgr][%s] RaGetTpAttrAsync failed ret[%d] firstTpHandle[%llu].",
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
            if (reqCtx.tpAttrBuf.size() < sizeof(struct TpAttr)) {
                HCCL_ERROR("[TpMgr][%s] loop path requires TpAttr with sl_available, param[%s].", __func__,
                    param.Describe().c_str());
                return HcclResult::HCCL_E_INTERNAL;
            }
            const struct TpAttr *tpAttr =
                reinterpret_cast<const struct TpAttr *>(reqCtx.tpAttrBuf.data());
            const uint16_t slMask = ReadSlAvailableMask16(*tpAttr, reqCtx.tpAttrBitmap);
            if (slMask == 0U) {
                HCCL_ERROR("[TpMgr][%s] loop path requires non-zero sl_available, param[%s].", __func__,
                    param.Describe().c_str());
                return HcclResult::HCCL_E_INTERNAL;
            }
            tmpTpInfo.mappedJettyPriority = SlValueAtRankInMask16(slMask, 0U);
            tmpTpInfo.hasMappedJettyPriority = true;
        } else {
            const uint32_t mSlLevels = reqCtx.resolvedSlLevelCount;
            if (mSlLevels == 0U) {
                HCCL_ERROR("[TpMgr][%s] resolvedSlLevelCount is 0 (sl_available), param[%s].", __func__,
                    param.Describe().c_str());
                return HcclResult::HCCL_E_INTERNAL;
            }
            uint32_t mCap = mSlLevels;
            if (param.slLevelCount != 0U && mCap > param.slLevelCount) {
                mCap = param.slLevelCount;
            }
            if (reqCtx.tpAttrBuf.size() < sizeof(struct TpAttr)) {
                HCCL_ERROR("[TpMgr][%s] missing TpAttr buffer for sl_available, param[%s].", __func__,
                    param.Describe().c_str());
                return HcclResult::HCCL_E_INTERNAL;
            }
            const struct TpAttr *tpAttr =
                reinterpret_cast<const struct TpAttr *>(reqCtx.tpAttrBuf.data());
            const uint16_t slMask = ReadSlAvailableMask16(*tpAttr, reqCtx.tpAttrBitmap);
            if (slMask == 0U) {
                HCCL_ERROR("[TpMgr][%s] sl_available mask is zero, param[%s].", __func__,
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

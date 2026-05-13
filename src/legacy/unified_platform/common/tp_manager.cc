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
constexpr uint32_t kTpAttrSlAvailableBit = 18U;
constexpr uint32_t kTpAttrBitmapSl = (1U << 10U);
constexpr uint32_t kTpAttrBitmapDscp = (1U << 8U);
constexpr uint32_t kTpAttrDscpConfigModeBit = 19U;

namespace {

constexpr uint32_t kGetTpAttrOpcode = 106U;
constexpr uint32_t kGetTpAttrVersion = 2U;

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
    HCCL_INFO(
        "[TpManager][ApplyUbcQosTpSlPolicy] nTp[%u] slRawCnt[%u] slAvailableCnt[%u] k[%u] numGroups[%u] qos[%u] "
        "groupIdx[%u] slotIdx[%u] slMask[0x%x] tpListIdx[%u] mappedSl[%u] slLevelCount[%u] param[%s].",
        nTp, slRawCnt, slAvailableCnt, k, numGroups, qos, groupIdx, slotIdx, static_cast<unsigned>(slMask),
        tpListIndexOut, static_cast<unsigned>(mappedSlOut & 0xFU), param.slLevelCount, param.Describe().c_str());
    return true;
}

static bool DeviceSupportsRaGetTpAttrAsync(uint32_t phyId)
{
    u32 tpAttrVersion = 0;
    const s32 ret = RaGetInterfaceVersion(phyId, kGetTpAttrOpcode, &tpAttrVersion);
    return (ret == 0 && tpAttrVersion >= kGetTpAttrVersion);
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
    const s32 ret = RaCtxSetTpAttr(ctxHandle, tpHandle, kTpAttrBitmapSl, &tpSlAttr);
    if (ret == 0) {
        return HcclResult::HCCL_SUCCESS;
    }
    HCCL_WARNING(
        "[TpManager][CommitMappedSlToTpAttr] RaCtxSetTpAttr failed ret[%d] (e.g. Rs path phyId invalid on HDC); "
        "retry HrtRaSetTpAttrAsync tpHandle[%llu] sl[%u] devPhyId[%u].",
        ret, tpHandle, static_cast<unsigned>(mappedSl & 0xFU), devPhyId);
    RequestHandle reqHandle = 0;
    try {
        const HcclResult hret = HrtRaSetTpAttrAsync(ctxHandle, tpHandle, kTpAttrBitmapSl, tpSlAttr, reqHandle);
        if (hret != HcclResult::HCCL_SUCCESS) {
            HCCL_ERROR("[TpManager][CommitMappedSlToTpAttr] HrtRaSetTpAttrAsync failed hcclRet[%d] tpHandle[%llu] "
                       "sl[%u] devPhyId[%u].",
                static_cast<int>(hret), tpHandle, static_cast<unsigned>(mappedSl & 0xFU), devPhyId);
            return hret;
        }
    } catch (const NetworkApiException &ex) {
        HCCL_ERROR("[TpManager][CommitMappedSlToTpAttr] HrtRaSetTpAttrAsync exception: %s tpHandle[%llu] sl[%u] "
                   "devPhyId[%u].",
            ex.what(), tpHandle, static_cast<unsigned>(mappedSl & 0xFU), devPhyId);
        return HcclResult::HCCL_E_NETWORK;
    }
    return HcclResult::HCCL_SUCCESS;
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
    const s32 ret = RaCtxSetTpAttr(ctxHandle, tpHandle, kTpAttrBitmapDscp, &tpDscpAttr);
    if (ret == 0) {
        return HcclResult::HCCL_SUCCESS;
    }
    HCCL_WARNING(
        "[TpManager][CommitUboeDscpToTpAttr] RaCtxSetTpAttr failed ret[%d]; retry HrtRaSetTpAttrAsync tpHandle[%llu] "
        "dscp[%u] devPhyId[%u].",
        ret, tpHandle, static_cast<unsigned>(tpDscpAttr.dscp), devPhyId);
    RequestHandle reqHandle = 0;
    try {
        const HcclResult hret = HrtRaSetTpAttrAsync(ctxHandle, tpHandle, kTpAttrBitmapDscp, tpDscpAttr, reqHandle);
        if (hret != HcclResult::HCCL_SUCCESS) {
            return hret;
        }
    } catch (const NetworkApiException &) {
        return HcclResult::HCCL_E_NETWORK;
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
    auto &locReqCtxMap = reqCtxMap[locAddr];
    auto locReqCtxIter = locReqCtxMap.find(rmtAddr);
    if (locReqCtxIter == locReqCtxMap.end()) {
        HCCL_INFO("[TpManager][%s] get new tpInfo, param[%s].", __func__, param.Describe().c_str());

        RequestCtx &reqCtx = locReqCtxMap[rmtAddr];
        StartGetTpInfoListRequest(param, reqCtx);
        return HcclResult::HCCL_E_AGAIN;
    }

    RequestCtx &reqCtx = locReqCtxIter->second;

    if (!isHost) {
        if (reqCtx.phase == RequestCtx::ReqPhase::WAIT_LIST) {
            if (reqCtx.handle != 0U) {
                if (!CheckRequestResult(reqCtx.handle)) {
                    return HcclResult::HCCL_E_AGAIN;
                }
            }
            if (reqCtx.tpInfoNum == 0U) {
                locReqCtxMap.erase(locReqCtxIter);
                reqCtxLock.unlock();
                HCCL_WARNING("[TpManager][%s] failed to find tp info, tpInfoNum is 0, param[%s].", __func__,
                    param.Describe().c_str());
                return HcclResult::HCCL_E_NOT_FOUND;
            }
            if (DeviceSupportsRaGetTpAttrAsync(devPhyId)) {
                try {
                    StartGetTpAttrForFirstTpDevice(param, reqCtx);
                } catch (...) {
                    locReqCtxMap.erase(locReqCtxIter);
                    throw;
                }
                return HcclResult::HCCL_E_AGAIN;
            }
            RequestCtx completedReqCtx = std::move(locReqCtxIter->second);
            locReqCtxMap.erase(locReqCtxIter);
            reqCtxLock.unlock();
            return HandleCompletedRequest(std::move(completedReqCtx), param, tpInfo, false);
        }
        if (reqCtx.phase == RequestCtx::ReqPhase::WAIT_TP_ATTR) {
            if (reqCtx.handle != 0U) {
                if (!CheckRequestResult(reqCtx.handle)) {
                    return HcclResult::HCCL_E_AGAIN;
                }
            }
            RequestCtx completedReqCtx = std::move(locReqCtxIter->second);
            locReqCtxMap.erase(locReqCtxIter);
            reqCtxLock.unlock();
            return HandleCompletedRequest(std::move(completedReqCtx), param, tpInfo, true);
        }
    }

    RequestCtx completedReqCtx = std::move(locReqCtxIter->second);
    locReqCtxMap.erase(locReqCtxIter);
    reqCtxLock.unlock();

    return HandleCompletedRequest(std::move(completedReqCtx), param, tpInfo, false);
}

HcclResult TpManager::ReleaseTpInfo(const RaUbGetTpInfoParam &param, const TpInfo &tpInfo)
{
    std::lock_guard<std::mutex> lock(GetInfoCtxMutex(param.tpProtocol));
    auto &infoMap = GetInfoCtxMap(param.tpProtocol);
    auto &locInfoMap = infoMap[param.locAddr];
    auto locInfoIter = locInfoMap.find(param.rmtAddr);
    if (locInfoIter == locInfoMap.end()) {
        HCCL_ERROR("[TpManager][%s] failed, tp info is not found, "
            "param[%s].", __func__, param.Describe().c_str());
        return HcclResult::HCCL_E_NOT_FOUND;
    }

    if (tpInfo.tpHandle != locInfoIter->second.tpInfo.tpHandle) {
        HCCL_ERROR("[TpManager][%s] failed, tp info[%llu] is not expected[%llu].",
            __func__, tpInfo.tpHandle, locInfoIter->second.tpInfo.tpHandle);
        return HcclResult::HCCL_E_PARA;
    }

    if (locInfoIter->second.useCnt > 1) {
        locInfoIter->second.useCnt -= 1;
        return HcclResult::HCCL_SUCCESS;
    }

    locInfoMap.erase(locInfoIter);
    // 暂时不能主动释放tp handle，跟随unimport jetty释放
    return HcclResult::HCCL_SUCCESS;
}

bool TpManager::FindAndGetTpInfo(const RaUbGetTpInfoParam &param, TpInfo &tpInfo)
{
    std::lock_guard<std::mutex> lock(GetInfoCtxMutex(param.tpProtocol));
    auto &infoMap = GetInfoCtxMap(param.tpProtocol);
    auto &locInfoMap = infoMap[param.locAddr];
    auto locInfoIter = locInfoMap.find(param.rmtAddr);
    if (locInfoIter != locInfoMap.end()) {
        locInfoIter->second.useCnt += 1;
        tpInfo = locInfoIter->second.tpInfo;
        return true;
    }

    return false;
}

void TpManager::StartGetTpInfoListRequest(const RaUbGetTpInfoParam &param,
    TpManager::RequestCtx &reqCtx) const
{
    reqCtx.phase = RequestCtx::ReqPhase::WAIT_LIST;
    (void)memset_s(&reqCtx.tpAttr, sizeof(reqCtx.tpAttr), 0, sizeof(reqCtx.tpAttr));
    reqCtx.tpAttrBitmap = 0;

    Hccl::IpAddress localIp = param.locAddr;
    RdmaHandle rdmaHandle = isHost
        ? RdmaHandleManager::GetInstance().GetByAddr(devPhyId, LinkProtoType::UB, localIp,
              Hccl::PortDeploymentType::HOST_NET)
        : RdmaHandleManager::GetInstance().GetByIp(devPhyId, param.locAddr);
    if (!rdmaHandle) {
        THROW<InternalException>("[TpManager][%s] can not find rdmaHandle, "
            "devPhyId[%u] locAddr[%s].", __func__, devPhyId,
            param.locAddr.Describe().c_str());
    }
    if (isHost) {
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

    const struct HccpTpInfo *baseInfoPtr =
        reinterpret_cast<const struct HccpTpInfo *>(reqCtx.dataBuffer.data());

    TpInfo tmpTpInfo = ParseTpInfo(baseInfoPtr);

    if (withSlPolicy) {
        const uint16_t slMask = ReadSlAvailableMask16(reqCtx.tpAttr);
        const uint32_t slAvailableCnt = CalSlAvailableCnt(slMask);
        HCCL_INFO("[TpManager][%s] after get_tp_attr: slMask[0x%04x] slAvailableCnt[%u] slBitmap[0x%x] param[%s].",
            __func__, static_cast<unsigned>(slMask), slAvailableCnt,
            static_cast<unsigned>(reqCtx.tpAttr.slBitmap), param.Describe().c_str());
        if (slAvailableCnt == 0U) {
            HCCL_ERROR("[TpManager][%s] sl_available mask empty after get_tp_attr, param[%s].", __func__,
                param.Describe().c_str());
            return HcclResult::HCCL_E_INTERNAL;
        }
        uint32_t tpListIndex = 0;
        uint32_t mappedSl = 0;
        if (!ApplyUbcQosTpSlPolicy(param, tpInfoNum, slMask, tpListIndex, mappedSl)) {
            HCCL_ERROR("[TpManager][%s] ApplyUbcQosTpSlPolicy failed, param[%s] nTp[%u].", __func__,
                param.Describe().c_str(), tpInfoNum);
            return HcclResult::HCCL_E_INTERNAL;
        }
        if (tpListIndex >= tpInfoNum) {
            return HcclResult::HCCL_E_INTERNAL;
        }
        tmpTpInfo = ParseTpInfo(baseInfoPtr + tpListIndex);
        tmpTpInfo.mappedJettyPriority = mappedSl & 0xFU;
        tmpTpInfo.hasMappedJettyPriority = true;

        if (param.tpProtocol == TpProtocol::TP || param.tpProtocol == TpProtocol::UBOE) {
            CHK_RET(CommitMappedSlToTpAttr(devPhyId, param.locAddr, tmpTpInfo.tpHandle, mappedSl));
        }
        if (param.tpProtocol == TpProtocol::UBOE && reqCtx.tpAttr.dscpConfigMode == 0) {
            const uint8_t qos = static_cast<uint8_t>(param.qos & 0xFFU);
            uint8_t dscp = 33U;
            if (GetDscpByQosFromHccnCfg(devPhyId, qos, dscp)) {
                CHK_RET(CommitUboeDscpToTpAttr(devPhyId, param.locAddr, tmpTpInfo.tpHandle, dscp));
            }
        }
    } else {
        tmpTpInfo.hasMappedJettyPriority = false;
    }

    auto &locAddr = param.locAddr;
    auto &rmtAddr = param.rmtAddr;

    std::lock_guard<std::mutex> lock(GetInfoCtxMutex(param.tpProtocol));
    auto &infoMap = GetInfoCtxMap(param.tpProtocol);
    infoMap[locAddr][rmtAddr] = {std::move(tmpTpInfo), 1};

    tpInfo = infoMap[locAddr][rmtAddr].tpInfo;
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

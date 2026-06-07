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
#include <string>
#include <vector>

#include "hccp_ctx.h"
#include "hccp_async_ctx.h"
#include "hccp.h"

#include "hccl_common.h"
#include "exception_handler.h"
#include "hccl_exception.h"
#include "exception_util.h"
#include "orion_adapter_hccp.h"
#include "rdma_handle_manager.h"
#include "env_config/env_config.h"

using Hccl::HcclException;
using std::exception;
using std::string;

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

/// 将 GetTpInfoParam 解析为缓存键 loc/rmt/qos。
static HcclResult ResolveTpInfoAddrKey(const GetTpInfoParam &param, TpInfoAddrKey &out)
{
    CHK_RET(CommAddrToIpAddress(param.locAddr, out.locAddr));
    CHK_RET(CommAddrToIpAddress(param.rmtAddr, out.rmtAddr));
    out.qosKey = QosMapKey(param.qos);
    return HcclResult::HCCL_SUCCESS;
}

/// 统计 16 位 slBitmap 中置位个数（可用 SL 档位数）。
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

/// 取 slBitmap 中第 rank 个可用 SL 的 bit 值（0–15）。
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

/// 从 linkTpAttr 读出 slBitmap，供 Mapping/Commit 使用。
static uint16_t ReadSlAvailableMask16(const struct TpAttr &attr)
{
    return static_cast<uint16_t>(attr.slBitmap);
}

constexpr uint32_t kUboeEightTpPolicyCount = 8U;

/// 策略用 SL 档位数：掩码计数与 slLevelCount 取 min。
static uint32_t ResolveSlAvailableCntForPolicy(uint16_t slMask, uint32_t slLevelCount)
{
    uint32_t slAvailableCnt = CalSlAvailableCnt(slMask);
    if (slLevelCount != 0U) {
        slAvailableCnt = std::min(slLevelCount, slAvailableCnt);
    }
    return slAvailableCnt;
}

/// UBOE 8-TP：hcclQos 高→slBitmap 低档（高优先级），低→高档。
/// slAvailableCnt>=3 时按 3:2:3 分档（左闭）：qos∈[5,7]→rank0，qos∈[3,4]→中档 rank，qos∈[0,2]→rank(slAvailableCnt-1)。
static uint32_t MapUboeEightTpSlFromMask(uint32_t qos, uint16_t slMask, uint32_t slAvailableCnt)
{
    const uint32_t q = qos & 7U;
    if (slAvailableCnt == 0U) {
        return 0U;
    }
    if (slAvailableCnt == 1U) {
        // 仅一档 SL，QoS 分组无意义，固定取掩码内 rank 0
        return SlValueAtRankInMask16(slMask, 0U);
    }
    if (slAvailableCnt == 2U) {
        const uint32_t slRank = (q >= 4U) ? 0U : 1U;
        return SlValueAtRankInMask16(slMask, slRank);
    }
    uint32_t slRank = 0U;
    if (q >= 5U) {
        slRank = 0U;
    } else if (q >= 3U) {
        slRank = (slAvailableCnt - 1U) / 2U;
    } else {
        slRank = slAvailableCnt - 1U;
    }
    if (slRank >= slAvailableCnt) {
        slRank = slAvailableCnt - 1U;
    }
    return SlValueAtRankInMask16(slMask, slRank);
}

/// 环回场景：固定 list[0] + slBitmap 最低档 SL。
static bool ApplyLoopFirstTpLowestSl(const GetTpInfoParam &param, uint32_t nTp, uint16_t slMask,
    uint32_t slRawCnt, uint32_t slAvailableCnt, uint32_t &tpListIndexOut, uint32_t &mappedSlOut)
{
    tpListIndexOut = 0U;
    mappedSlOut = SlValueAtRankInMask16(slMask, 0U);
    HCCL_INFO("[TpMgr][ApplyLoopFirstTpLowestSl] nTp[%u] slRawCnt[%u] slAvailableCnt[%u] slMask[0x%x] "
              "tpListIdx[0] mappedSl[%u] param[%s].",
        nTp, slRawCnt, slAvailableCnt, static_cast<unsigned>(slMask),
        static_cast<unsigned>(mappedSlOut & 0xFU), param.Describe().c_str());
    return true;
}

/// CTP/RTP 分组策略：8 档 QoS 映射到 min(nTp, slAvailableCnt) 个 TP×SL 槽位。
static bool ApplyUbcQosTpSlPolicyGrouped(const GetTpInfoParam &param, uint32_t nTp, uint16_t slMask,
    uint32_t slRawCnt, uint32_t slAvailableCnt, uint32_t &tpListIndexOut, uint32_t &mappedSlOut)
{
    if (nTp == 0U || slAvailableCnt == 0U) {
        HCCL_ERROR("[TpMgr][ApplyUbcQosTpSlPolicyGrouped] nTp or slAvailableCnt zero: nTp[%u] slAvailableCnt[%u] "
                   "slMask[0x%x] param[%s].",
            nTp, slAvailableCnt, static_cast<unsigned>(slMask), param.Describe().c_str());
        return false;
    }
    const uint32_t k = std::min(nTp, slAvailableCnt);
    if (k == 0U) {
        HCCL_ERROR("[TpMgr][ApplyUbcQosTpSlPolicyGrouped] k is zero: nTp[%u] slAvailableCnt[%u] slMask[0x%x] "
                   "param[%s].",
            nTp, slAvailableCnt, static_cast<unsigned>(slMask), param.Describe().c_str());
        return false;
    }
    const uint32_t numGroups = std::min(8U, k);
    const uint32_t qos = param.qos & 7U;
    // K=3：8 档 QoS 按 3:2:3 → [0–2]/[3–4]/[5–7]；其余 k 仍用均匀分段
    const uint32_t groupIdx =
        (k == 3U) ? (qos < 3U ? 0U : (qos < 5U ? 1U : 2U)) : ((qos * numGroups) / 8U);
    const uint32_t slotIdx = (groupIdx * k) / numGroups;
    if (slotIdx >= k || slotIdx >= nTp) {
        HCCL_ERROR("[TpMgr][ApplyUbcQosTpSlPolicyGrouped] slotIdx out of range: nTp[%u] slRawCnt[%u] "
                   "slAvailableCnt[%u] k[%u] numGroups[%u] qos[%u] groupIdx[%u] slotIdx[%u] slMask[0x%x] param[%s].",
            nTp, slRawCnt, slAvailableCnt, k, numGroups, qos, groupIdx, slotIdx, static_cast<unsigned>(slMask),
            param.Describe().c_str());
        return false;
    }
    // hcclQos 越大优先级越高；UB SL 数值越小优先级越高，对档位取反
    const uint32_t slRank = (slAvailableCnt - 1U) - slotIdx;
    if (slRank >= slAvailableCnt) {
        HCCL_ERROR("[TpMgr][ApplyUbcQosTpSlPolicyGrouped] slRank out of range: nTp[%u] slAvailableCnt[%u] k[%u] "
                   "slRank[%u] slMask[0x%x] param[%s].",
            nTp, slAvailableCnt, k, slRank, static_cast<unsigned>(slMask), param.Describe().c_str());
        return false;
    }
    tpListIndexOut = (k - 1U) - slotIdx;
    mappedSlOut = SlValueAtRankInMask16(slMask, slRank);
    return true;
}

/// CTP/RTP Mapping：环回/单档 SL 特例后委托分组策略。
static bool ApplyUbcQosTpSlPolicy(const GetTpInfoParam &param, uint32_t nTp, uint16_t slMask,
    uint32_t &tpListIndexOut, uint32_t &mappedSlOut)
{
    const uint32_t slRawCnt = CalSlAvailableCnt(slMask);
    uint32_t slAvailableCnt = slRawCnt;
    if (slAvailableCnt == 0U) {
        HCCL_ERROR("[TpMgr][ApplyUbcQosTpSlPolicy] slMask empty: nTp[%u] slMask[0x%x] param[%s].", nTp,
            static_cast<unsigned>(slMask), param.Describe().c_str());
        return false;
    }
    if (param.slLevelCount != 0U) {
        slAvailableCnt = std::min(param.slLevelCount, slAvailableCnt);
    }
    if (param.loopFirstTpLowestSl) {
        return ApplyLoopFirstTpLowestSl(param, nTp, slMask, slRawCnt, slAvailableCnt, tpListIndexOut, mappedSlOut);
    }
    // slAvailableCnt==1：仅一档 SL，QoS 无法分组；固定 TP 槽位 0 + 掩码 rank 0，避免分组公式在 k=1 时误判
    if (slAvailableCnt == 1U) {
        if (nTp == 0U) {
            HCCL_ERROR("[TpMgr][ApplyUbcQosTpSlPolicy] nTp zero with single SL tier: slMask[0x%x] param[%s].",
                static_cast<unsigned>(slMask), param.Describe().c_str());
            return false;
        }
        tpListIndexOut = 0U;
        mappedSlOut = SlValueAtRankInMask16(slMask, 0U);
        return true;
    }
    return ApplyUbcQosTpSlPolicyGrouped(param, nTp, slMask, slRawCnt, slAvailableCnt, tpListIndexOut, mappedSlOut);
}

/// UBOE 专用：8 个 TP 与 hcclQos 一一对应；SL 由 GetTpAttr.slBitmap 可用档位按 3:2:3 动态选取。
static bool TryApplyUboeEightTpQosPolicy(const GetTpInfoParam &param, uint32_t nTp, uint16_t slMask,
    uint32_t &tpListIndexOut, uint32_t &mappedSlOut)
{
    if (param.tpProtocol != TpProtocol::UBOE || param.loopFirstTpLowestSl) {
        return false;
    }
    const uint32_t slAvailableCnt = ResolveSlAvailableCntForPolicy(slMask, param.slLevelCount);
    if (nTp != kUboeEightTpPolicyCount || slAvailableCnt == 0U) {
        return false;
    }
    const uint32_t qos = param.qos & 7U;
    static constexpr uint8_t kUboeEightTpIndexByQos[8] = {7U, 6U, 5U, 4U, 3U, 2U, 1U, 0U};
    tpListIndexOut = kUboeEightTpIndexByQos[qos];
    mappedSlOut = MapUboeEightTpSlFromMask(qos, slMask, slAvailableCnt);
    HCCL_INFO("[TpMgr][TryApplyUboeEightTpQosPolicy] qos[%u] tpListIndex[%u] mappedSl[%u] slMask[0x%x] "
              "slAvailableCnt[%u] param[%s].",
        qos, tpListIndexOut, mappedSlOut, static_cast<unsigned>(slMask), slAvailableCnt, param.Describe().c_str());
    return true;
}

/// QoS Mapping 入口：qos + slBitmap → tpListIndex + mappedSl（供 RunMapping 调用）。
static bool ApplyTpQosSlPolicy(const GetTpInfoParam &param, uint32_t nTp, uint16_t slMask,
    uint32_t &tpListIndexOut, uint32_t &mappedSlOut)
{
    // UBOE 8 TP 特化：qos 0..7 一一对应 TP 7..0，SL 走专用映射；条件不满足则返回 false 走通用策略。
    if (TryApplyUboeEightTpQosPolicy(param, nTp, slMask, tpListIndexOut, mappedSlOut)) {
        return true;
    }
    // 通用 QoS→TP→SL：按 hcclQos 分组选 TP 槽位，再按 SL 档位（数值越小优先级越高）取 mappedSl。
    return ApplyUbcQosTpSlPolicy(param, nTp, slMask, tpListIndexOut, mappedSlOut);
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

/// 组装 list[0] bootstrap 用的 attrBitmap（slBitmap + at/retry；UBOE 含 dscp）。
static uint32_t BuildBootstrapAttrBitmap(TpProtocol tpProtocol)
{
    uint32_t bitmap = (1U << kTpAttrSlAvailableBit) | kTpAttrBitmapSl | 0x3U;
    if (tpProtocol == TpProtocol::UBOE) {
        bitmap |= kTpAttrBitmapDscp | (1U << kTpAttrDscpConfigModeBit);
    }
    return bitmap;
}

/// RTP/UBOE 需在 selectedTp 上 SetTpAttr；CTP 跳过 Commit。
static bool NeedsCommitPhase(TpProtocol tpProtocol)
{
    return tpProtocol == TpProtocol::RTP || tpProtocol == TpProtocol::UBOE;
}

/// 由本端 CommAddr 解析 RDMA ctx，供 Ra*Async 使用。
static HcclResult ResolveCtxHandle(const uint32_t devPhyId, const CommAddr &locCommAddr, CtxHandle &ctxHandle)
{
    Hccl::IpAddress locAddr{};
    CHK_RET(CommAddrToIpAddress(locCommAddr, locAddr));
    ctxHandle = static_cast<CtxHandle>(Hccl::RdmaHandleManager::GetInstance().GetByIp(devPhyId, locAddr));
    CHK_PTR_NULL(ctxHandle);
    return HcclResult::HCCL_SUCCESS;
}

/// 提交 RaSetTpAttrAsync，由调用方 poll CheckRequestResult。
static HcclResult SubmitSetTpAttr(CtxHandle ctxHandle, uint64_t tpHandle, uint32_t attrBitmap, TpAttr &attr,
    RequestHandle &outHandle)
{
    if (tpHandle == 0U) {
        HCCL_ERROR("[TpMgr][SubmitSetTpAttr] tpHandle is 0");
        return HcclResult::HCCL_E_INTERNAL;
    }
    void *raReqHandle = nullptr;
    const s32 ret = RaSetTpAttrAsync(ctxHandle, tpHandle, attrBitmap, &attr, &raReqHandle);
    if (ret != 0 || raReqHandle == nullptr) {
        HCCL_ERROR("[TpMgr][SubmitSetTpAttr] RaSetTpAttrAsync failed ret[%d] tpHandle[%llu] bitmap[0x%x].", ret,
            tpHandle, attrBitmap);
        return HcclResult::HCCL_E_NETWORK;
    }
    outHandle = reinterpret_cast<RequestHandle>(raReqHandle);
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
    if (ret != 0) {
        HCCL_WARNING("[TpMgr][%s] RaGetHccnCfg failed ret[%d] phyId[%u] qos[%u].", __func__, ret, devPhyId,
            static_cast<unsigned>(qos));
        return false;
    }
    if (valueLen == 0U) {
        HCCL_WARNING("[TpMgr][%s] RaGetHccnCfg empty cfg phyId[%u] qos[%u].", __func__, devPhyId,
            static_cast<unsigned>(qos));
        return false;
    }
    if (valueLen > kCfgBufLen) {
        valueLen = kCfgBufLen;
    }
    const std::string cfg(value.data(), valueLen);
    HCCL_INFO("[TpMgr][%s] RaGetHccnCfg ok phyId[%u] qos[%u] valueLen[%u] qos_dscp[%s].", __func__, devPhyId,
        static_cast<unsigned>(qos), valueLen, cfg.c_str());
    if (!ParseDscpFromCfgByQos(cfg, qos, dscpOut)) {
        HCCL_WARNING("[TpMgr][%s] parse qos_dscp failed phyId[%u] qos[%u] cfg[%s].", __func__, devPhyId,
            static_cast<unsigned>(qos), cfg.c_str());
        return false;
    }
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

/// 轮询 Hccp 异步请求：未完成返回 HCCL_E_AGAIN。
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

/// 校验 tpProtocol 是否为 CTP/RTP/UBOE。
HcclResult CheckTpProtocol(const TpProtocol tpProtocol)
{
    if (tpProtocol != TpProtocol::CTP && tpProtocol != TpProtocol::RTP && tpProtocol != TpProtocol::UBOE) {
        HCCL_ERROR("[TpMgr][%s] failed, tpProtocol[%d] is not supported.", __func__, tpProtocol);
        return HcclResult::HCCL_E_NOT_SUPPORT;
    }

    return HcclResult::HCCL_SUCCESS;
}

/// 在 infoMap 三级索引中查找 loc/rmt/qos 条目。
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

/// GetTpInfo 快路径：在 tpInfoCache[loc×rmt×qos] 中查找已完成结果。
/// 命中则 useCnt++ 并拷贝 tpInfo；未命中返回 HCCL_E_NOT_FOUND，由调用方走异步状态机。
HcclResult TpMgr::FindAndGetTpInfo(const GetTpInfoParam &param, TpInfo &tpInfo)
{
    TpInfoAddrKey key{};
    CHK_RET(ResolveTpInfoAddrKey(param, key));

    std::lock_guard<std::mutex> lock(GetInfoCtxMutex(param.tpProtocol));
    auto &infoMap = GetInfoCtxMap(param.tpProtocol);

    InfoCtxMap::iterator lit = infoMap.find(key.locAddr);
    if (lit == infoMap.end()) {
        return HcclResult::HCCL_E_NOT_FOUND;
    }
    InfoRmtMap::iterator rit = lit->second.find(key.rmtAddr);
    if (rit == lit->second.end()) {
        return HcclResult::HCCL_E_NOT_FOUND;
    }
    InfoQosMap::iterator qosIt = rit->second.find(key.qosKey);
    if (qosIt == rit->second.end()) {
        return HcclResult::HCCL_E_NOT_FOUND;
    }

    qosIt->second.useCnt += 1;
    tpInfo = qosIt->second.tpInfo;
    return HcclResult::HCCL_SUCCESS;
}

/// 同链路已 bootstrap 过则复用 linkAttr，免二次 GetTpAttr。
bool TpMgr::TryGetLinkAttrCache(const GetTpInfoParam &param, TpAttr &outAttr)
{
    Hccl::IpAddress locAddr{};
    Hccl::IpAddress rmtAddr{};
    if (CommAddrToIpAddress(param.locAddr, locAddr) != HcclResult::HCCL_SUCCESS) {
        return false;
    }
    if (CommAddrToIpAddress(param.rmtAddr, rmtAddr) != HcclResult::HCCL_SUCCESS) {
        return false;
    }
    std::lock_guard<std::mutex> lock(GetInfoCtxMutex(param.tpProtocol));
    const auto &linkMap = GetLinkAttrMap(param.tpProtocol);
    const auto lit = linkMap.find(locAddr);
    if (lit == linkMap.end()) {
        return false;
    }
    const auto rit = lit->second.find(rmtAddr);
    if (rit == lit->second.end() || !rit->second.valid) {
        return false;
    }
    outAttr = rit->second.tpAttr;
    return true;
}

/// 缓存 list[0] 链路级 TpAttr，供同 loc×rmt 后续 qos 复用。
void TpMgr::PutLinkAttrCache(const GetTpInfoParam &param, const TpAttr &attr)
{
    Hccl::IpAddress locAddr{};
    Hccl::IpAddress rmtAddr{};
    (void)CommAddrToIpAddress(param.locAddr, locAddr);
    (void)CommAddrToIpAddress(param.rmtAddr, rmtAddr);
    std::lock_guard<std::mutex> lock(GetInfoCtxMutex(param.tpProtocol));
    auto &entry = GetLinkAttrMap(param.tpProtocol)[locAddr][rmtAddr];
    entry.tpAttr = attr;
    entry.valid = true;
}

/// WAIT_LIST 阶段收尾：校验 tp 列表，触发 bootstrap 或直走 Mapping。
HcclResult TpMgr::AdvanceFromWaitList(const GetTpInfoParam &param, RequestCtx &reqCtx, ReqQosMap &qosMap,
    const ReqQosMap::iterator it, std::unique_lock<std::mutex> &reqCtxLock, TpInfo &tpInfo)
{
    if (reqCtx.tpInfoNum == 0U) {
        qosMap.erase(it);
        reqCtxLock.unlock();
        HCCL_ERROR("[TpMgr][%s] failed to find tp info, tpInfoNum is 0, param[%s].", __func__, param.Describe().c_str());
        return HcclResult::HCCL_E_NOT_FOUND;
    }
    const struct HccpTpInfo *list = reinterpret_cast<const struct HccpTpInfo *>(reqCtx.dataBuffer.data());
    HCCL_INFO("[TpMgr][GetTpInfo] list stage ok, devPhyId[%u] tpInfoNum[%u] firstTpHandle[%llu] param[%s].",
        devPhyId_, reqCtx.tpInfoNum, static_cast<unsigned long long>(list[0].tpHandle), param.Describe().c_str());

    if (TryGetLinkAttrCache(param, reqCtx.linkTpAttr)) {
        HCCL_INFO("[TpMgr][GetTpInfo] linkAttr cache hit, skip bootstrap get, param[%s].", param.Describe().c_str());
        return RunMappingAndContinue(param, reqCtx, qosMap, it, reqCtxLock, tpInfo);
    }

    try {
        CHK_RET(StartBootstrapLinkAttr(param, reqCtx));
    } catch (...) {
        qosMap.erase(it);
        throw;
    }
    HCCL_INFO("[TpMgr][GetTpInfo] RaGetTpAttrAsync submitted, devPhyId[%u] reqHandle[%llu] phase[WAIT_ATTR] "
              "bootstrapBitmap[0x%x] param[%s].",
        devPhyId_, static_cast<unsigned long long>(reqCtx.handle), reqCtx.bootstrapAttrBitmap, param.Describe().c_str());
    return HcclResult::HCCL_E_AGAIN;
}

/// WAIT_ATTR 阶段收尾：落 linkCache 后进入 Mapping。
HcclResult TpMgr::AdvanceFromWaitAttr(const GetTpInfoParam &param, RequestCtx &reqCtx, ReqQosMap &qosMap,
    const ReqQosMap::iterator it, std::unique_lock<std::mutex> &reqCtxLock, TpInfo &tpInfo)
{
    PutLinkAttrCache(param, reqCtx.linkTpAttr);
    const uint16_t slMask = ReadSlAvailableMask16(reqCtx.linkTpAttr);
    HCCL_INFO("[TpMgr][GetTpInfo] bootstrap attr ok, slMask[0x%04x] slBitmap[0x%x] param[%s].",
        static_cast<unsigned>(slMask), static_cast<unsigned>(reqCtx.linkTpAttr.slBitmap), param.Describe().c_str());
    return RunMappingAndContinue(param, reqCtx, qosMap, it, reqCtxLock, tpInfo);
}

/// WAIT_COMMIT 阶段收尾：SetTpAttr 完成后 Finalize。
HcclResult TpMgr::AdvanceFromWaitCommit(const GetTpInfoParam &param, RequestCtx &reqCtx, ReqQosMap &qosMap,
    const ReqQosMap::iterator it, std::unique_lock<std::mutex> &reqCtxLock, TpInfo &tpInfo)
{
    return FinalizeGetTpInfo(std::move(reqCtx), param, qosMap, it, reqCtxLock, tpInfo);
}

/// ApplyTpQosSlPolicy 选 selectedTp；CTP 结束，RTP/UBOE 进入 Commit。
HcclResult TpMgr::RunMappingAndContinue(const GetTpInfoParam &param, RequestCtx &reqCtx, ReqQosMap &qosMap,
    const ReqQosMap::iterator it, std::unique_lock<std::mutex> &reqCtxLock, TpInfo &tpInfo)
{
    const uint32_t tpInfoNum = reqCtx.tpInfoNum;
    const struct HccpTpInfo *baseInfoPtr =
        reinterpret_cast<const struct HccpTpInfo *>(reqCtx.dataBuffer.data());
    const uint16_t slMask = ReadSlAvailableMask16(reqCtx.linkTpAttr);
    const uint32_t slAvailableCnt = CalSlAvailableCnt(slMask);
    if (slAvailableCnt == 0U) {
        qosMap.erase(it);
        reqCtxLock.unlock();
        HCCL_ERROR("[TpMgr][%s] sl_available mask empty, param[%s].", __func__, param.Describe().c_str());
        return HcclResult::HCCL_E_INTERNAL;
    }
    uint32_t tpListIndex = 0U;
    uint32_t mappedSl = 0U;
    if (!ApplyTpQosSlPolicy(param, tpInfoNum, slMask, tpListIndex, mappedSl)) {
        qosMap.erase(it);
        reqCtxLock.unlock();
        HCCL_ERROR("[TpMgr][%s] ApplyTpQosSlPolicy failed, param[%s].", __func__, param.Describe().c_str());
        return HcclResult::HCCL_E_INTERNAL;
    }
    if (tpListIndex >= tpInfoNum) {
        qosMap.erase(it);
        reqCtxLock.unlock();
        HCCL_ERROR("[TpMgr][%s] tpListIndex out of range: tpListIndex[%u] tpInfoNum[%u] param[%s].", __func__,
            tpListIndex, tpInfoNum, param.Describe().c_str());
        return HcclResult::HCCL_E_INTERNAL;
    }
    reqCtx.tpListIndex = tpListIndex;
    reqCtx.mappedSl = mappedSl;
    reqCtx.selectedTpHandle = baseInfoPtr[tpListIndex].tpHandle;
    HCCL_INFO("[TpMgr][GetTpInfo] mapping ok: tpListIndex[%u] selectedTpHandle[%llu] mappedSl[%u] qos[%u] param[%s].",
        tpListIndex, reqCtx.selectedTpHandle, static_cast<unsigned>(mappedSl & 0xFU), param.qos & 0xFFU,
        param.Describe().c_str());

    if (!NeedsCommitPhase(param.tpProtocol)) {
        return FinalizeGetTpInfo(std::move(reqCtx), param, qosMap, it, reqCtxLock, tpInfo);
    }

    try {
        CHK_RET(StartCommitTpAttr(param, reqCtx));
    } catch (...) {
        qosMap.erase(it);
        throw;
    }
    reqCtx.phase = ReqPhase::WAIT_COMMIT;
    HCCL_INFO("[TpMgr][GetTpInfo] RaSetTpAttrAsync commit submitted, reqHandle[%llu] tpHandle[%llu] sl[%u] "
              "protocol[%d] param[%s].",
        static_cast<unsigned long long>(reqCtx.handle), reqCtx.selectedTpHandle,
        static_cast<unsigned>(mappedSl & 0xFU), static_cast<int>(param.tpProtocol), param.Describe().c_str());
    return HcclResult::HCCL_E_AGAIN;
}

/// 全部异步阶段完成：写缓存、填充 tpInfo，返回 SUCCESS。
HcclResult TpMgr::FinalizeGetTpInfo(RequestCtx reqCtx, const GetTpInfoParam &param, ReqQosMap &qosMap,
    const ReqQosMap::iterator it, std::unique_lock<std::mutex> &reqCtxLock, TpInfo &tpInfo)
{
    TpInfo tmpTpInfo{};
    tmpTpInfo.tpHandle = reqCtx.selectedTpHandle;
    tmpTpInfo.mappedJettyPriority = reqCtx.mappedSl & 0xFU;
    tmpTpInfo.hasMappedJettyPriority = true;
    if (param.tpProtocol != TpProtocol::CTP) {
        tmpTpInfo.jettyErrTimeout = CalcTaTimeout(TpAttrInfo(reqCtx.linkTpAttr));
        tmpTpInfo.hasJettyErrTimeout = true;
    }

    HCCL_INFO("[TpMgr][%s] tp qos mapping ok: tpHandle[%llu] tpListIndex[%u] mappedSl[%u] jettyPriority[%u] qos[%u] "
              "param[%s].",
        __func__, tmpTpInfo.tpHandle, reqCtx.tpListIndex, static_cast<unsigned>(reqCtx.mappedSl & 0xFU),
        tmpTpInfo.mappedJettyPriority, param.qos & 0xFFU, param.Describe().c_str());

    Hccl::IpAddress locAddr{};
    Hccl::IpAddress rmtAddr{};
    CHK_RET(CommAddrToIpAddress(param.locAddr, locAddr));
    CHK_RET(CommAddrToIpAddress(param.rmtAddr, rmtAddr));
    const QosKey qosKey = QosMapKey(param.qos);

    {
        std::lock_guard<std::mutex> infoLock(GetInfoCtxMutex(param.tpProtocol));
        auto &infoMap = GetInfoCtxMap(param.tpProtocol);
        auto &rmtMap = infoMap[locAddr][rmtAddr];
        rmtMap[qosKey] = TpInfoCtx{tmpTpInfo, 1U};
        tpInfo = rmtMap[qosKey].tpInfo;
    }

    {
        std::lock_guard<std::mutex> attrLock(tpAttrCtxMutex_);
        tpAttrCtxMap_[reqCtx.selectedTpHandle] = TpAttrCtx{TpAttrInfo(reqCtx.linkTpAttr), 1U};
    }

    qosMap.erase(it);
    reqCtxLock.unlock();
    return HcclResult::HCCL_SUCCESS;
}

/// 异步 GetTpInfo：在 reqCtxMap 上按 phase 推进，未完成返回 HCCL_E_AGAIN。
HcclResult TpMgr::RunAsyncGetTpInfo(const GetTpInfoParam &param, TpInfo &tpInfo)
{
    std::unique_lock<std::mutex> reqCtxLock(GetReqCtxMutex(param.tpProtocol));
    auto &reqCtxMap = GetReqCtxMap(param.tpProtocol);
    TpInfoAddrKey key{};
    CHK_RET(ResolveTpInfoAddrKey(param, key));
    auto &qosMap = reqCtxMap[key.locAddr][key.rmtAddr];
    auto it = qosMap.find(key.qosKey);
    if (it == qosMap.end()) {
        RequestCtx &reqCtx = qosMap[key.qosKey];
        CHK_RET(StartGetTpInfoListRequest(param, reqCtx));
        HCCL_INFO("[TpMgr][GetTpInfo] RaGetTpInfoListAsync submitted, devPhyId[%u] reqHandle[%llu] phase[WAIT_LIST] "
                  "param[%s].",
            devPhyId_, static_cast<unsigned long long>(reqCtx.handle), param.Describe().c_str());
        return HcclResult::HCCL_E_AGAIN;
    }

    RequestCtx &reqCtx = it->second;
    if (reqCtx.handle != 0U) {
        const auto ret = CheckRequestResult(reqCtx.handle);
        if (ret == HcclResult::HCCL_E_AGAIN) {
            return ret;
        }
        CHK_RET(ret);
        reqCtx.handle = 0U;
    }

    switch (reqCtx.phase) {
    case ReqPhase::WAIT_LIST:
        // List 已完成：取 linkAttr（或命中缓存）后进入 Mapping / WAIT_ATTR
        return AdvanceFromWaitList(param, reqCtx, qosMap, it, reqCtxLock, tpInfo);
    case ReqPhase::WAIT_ATTR:
        // bootstrap GetTpAttr 已完成：落 linkCache，执行 QoS Mapping
        return AdvanceFromWaitAttr(param, reqCtx, qosMap, it, reqCtxLock, tpInfo);
    case ReqPhase::WAIT_COMMIT:
        // SetTpAttr 已完成：Finalize
        return AdvanceFromWaitCommit(param, reqCtx, qosMap, it, reqCtxLock, tpInfo);
    default:
        qosMap.erase(it);
        reqCtxLock.unlock();
        HCCL_ERROR("[TpMgr][%s] unexpected phase[%u] param[%s].", __func__, static_cast<unsigned>(reqCtx.phase),
            param.Describe().c_str());
        return HcclResult::HCCL_E_INTERNAL;
    }
}

/// 入口：缓存命中即 SUCCESS，否则在 req 锁内 poll 状态机（进行中返回 HCCL_E_AGAIN）。
HcclResult TpMgr::GetTpInfo(const GetTpInfoParam &param, TpInfo &tpInfo)
{
    CHK_RET(CheckTpProtocol(param.tpProtocol));
    if (FindAndGetTpInfo(param, tpInfo) == HcclResult::HCCL_SUCCESS) {
        return HcclResult::HCCL_SUCCESS;
    }

    return RunAsyncGetTpInfo(param, tpInfo);
}

/// 释放 GetTpInfo 缓存引用；useCnt 归零时删除 loc×rmt×qos 条目。
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

/// 封装 RaGetTpInfoListAsync，填充 EID/协议并预分配 tp 列表缓冲区。
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

/// 重置 reqCtx，提交 List 查询，phase=WAIT_LIST。
HcclResult TpMgr::StartGetTpInfoListRequest(const GetTpInfoParam &param, RequestCtx &reqCtx) const
{
    EXCEPTION_HANDLE_BEGIN
    reqCtx.phase = ReqPhase::WAIT_LIST;
    reqCtx.bootstrapAttrBitmap = 0U;
    reqCtx.tpListIndex = 0U;
    reqCtx.mappedSl = 0U;
    reqCtx.selectedTpHandle = 0U;
    (void)memset_s(&reqCtx.linkTpAttr, sizeof(reqCtx.linkTpAttr), 0, sizeof(reqCtx.linkTpAttr));

    CtxHandle ctxHandle = nullptr;
    CHK_RET(ResolveCtxHandle(devPhyId_, param.locAddr, ctxHandle));
    CHK_RET(GetTpInfoListAsync(ctxHandle, param, reqCtx.dataBuffer, reqCtx.tpInfoNum, reqCtx.handle));
    EXCEPTION_HANDLE_END
    return HcclResult::HCCL_SUCCESS;
}

/// 对 list[0] 提交 bootstrap GetTpAttr，phase=WAIT_ATTR。
HcclResult TpMgr::StartBootstrapLinkAttr(const GetTpInfoParam &param, RequestCtx &reqCtx) const
{
    EXCEPTION_HANDLE_BEGIN
    (void)memset_s(&reqCtx.linkTpAttr, sizeof(reqCtx.linkTpAttr), 0, sizeof(reqCtx.linkTpAttr));
    reqCtx.bootstrapAttrBitmap = BuildBootstrapAttrBitmap(param.tpProtocol);

    const struct HccpTpInfo *list = reinterpret_cast<const struct HccpTpInfo *>(reqCtx.dataBuffer.data());
    const uint64_t firstTpHandle = list[0].tpHandle;

    CtxHandle ctxHandle = nullptr;
    CHK_RET(ResolveCtxHandle(devPhyId_, param.locAddr, ctxHandle));

    void *raReqHandle = nullptr;
    const s32 ret = RaGetTpAttrAsync(ctxHandle, firstTpHandle, &reqCtx.bootstrapAttrBitmap, &reqCtx.linkTpAttr,
        &raReqHandle);
    if (ret != 0 || raReqHandle == nullptr) {
        HCCL_ERROR("[TpMgr][%s] RaGetTpAttrAsync failed ret[%d] raReqHandle[%p] ctx[%p] tpHandle[%llu].", __func__,
            ret, raReqHandle, ctxHandle, firstTpHandle);
        return HcclResult::HCCL_E_NETWORK;
    }
    reqCtx.handle = reinterpret_cast<RequestHandle>(raReqHandle);
    reqCtx.phase = ReqPhase::WAIT_ATTR;
    EXCEPTION_HANDLE_END
    return HcclResult::HCCL_SUCCESS;
}

/// RTP 异步写 SL；UBOE 在同一次 SetTpAttr 中写 SL，且管控未配 dscp 时一并写 DSCP。
HcclResult TpMgr::StartCommitTpAttr(const GetTpInfoParam &param, RequestCtx &reqCtx) const
{
    CtxHandle ctxHandle = nullptr;
    CHK_RET(ResolveCtxHandle(devPhyId_, param.locAddr, ctxHandle));

    TpAttr tpAttr {};
    tpAttr.sl = static_cast<uint8_t>(reqCtx.mappedSl & 0xFU);
    uint32_t attrBitmap = kTpAttrBitmapSl;

    if (param.tpProtocol == TpProtocol::UBOE && (reqCtx.linkTpAttr.dscpConfigMode & 1U) == 0U) {
        const uint16_t slMask = ReadSlAvailableMask16(reqCtx.linkTpAttr);
        const uint8_t dscpLookupQos = ResolveUboeDscpLookupQos(param, reqCtx.tpInfoNum, slMask);
        // 决定用哪个 qos 档去 HCCN 配置里查 dscp
        uint8_t dscp = 33U;
        (void)GetDscpByQosFromHccnCfg(devPhyId_, dscpLookupQos, dscp);
        tpAttr.dscp = static_cast<uint8_t>(dscp & 0x3FU);
        attrBitmap |= kTpAttrBitmapDscp;
        const uint8_t dscpBefore = static_cast<uint8_t>(reqCtx.linkTpAttr.dscp & 0x3FU);
        HCCL_INFO("[TpMgr][%s] UBOE commit sl+dscp: tpHandle[%llu] sl[%u] dscpLookupQos[%u] dscpBefore[%u] "
                  "dscpAfter[%u].",
            __func__, reqCtx.selectedTpHandle, static_cast<unsigned>(tpAttr.sl & 0xFU),
            static_cast<unsigned>(dscpLookupQos), static_cast<unsigned>(dscpBefore),
            static_cast<unsigned>(tpAttr.dscp));
    }

    return SubmitSetTpAttr(ctxHandle, reqCtx.selectedTpHandle, attrBitmap, tpAttr, reqCtx.handle);
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

/// 按协议返回 tpInfo 完成缓存 map。
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

/// 按协议返回 GetTpInfo 进行中 reqCtx map。
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

/// 按协议返回 linkAttr 链路缓存 map。
TpMgr::LinkAttrMap &TpMgr::GetLinkAttrMap(const TpProtocol tpProtocol)
{
    switch (tpProtocol) {
        case TpProtocol::CTP:
            return ctpLinkAttrMap_;
        case TpProtocol::RTP:
            return rtpLinkAttrMap_;
        case TpProtocol::UBOE:
            return uboeLinkAttrMap_;
        default:
            return rtpLinkAttrMap_;
    }
}

/// 按协议返回 tpInfo 缓存互斥锁。
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

/// 按协议返回 GetTpInfo reqCtx 互斥锁。
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

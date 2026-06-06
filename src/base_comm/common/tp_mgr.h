/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef TP_MGR_H
#define TP_MGR_H

#include <cstdint>
#include <functional>
#include <mutex>
#include <vector>
#include <unordered_map>

#include "hccl_types.h"
#include "hcomm_adapter_hccp.h"
#include "hccp_tp.h"
#include "orion_adpt_utils.h"

namespace hcomm {

// ==================== TA 档位映射表 ====================
// TA 芯片挡位 = hw_value / 8
// 挡位 0 (0-7):   512ms
// 挡位 1 (8-15):  1000ms (1s)
// 挡位 2 (16-23): 8000ms (8s)
// 挡位 3 (24-31): 32000ms (32s)
// 各挡位对应的最小硬件配置值
constexpr uint8_t TA_HW_GEAR0_BASE = 0;
constexpr uint8_t TA_HW_GEAR1_BASE = 8;
constexpr uint8_t TA_HW_GEAR2_BASE = 16;
constexpr uint8_t TA_HW_GEAR3_BASE = 24;

constexpr uint8_t TA_GEAR_INDEX_0 = 0;
constexpr uint8_t TA_GEAR_INDEX_1 = 1;
constexpr uint8_t TA_GEAR_INDEX_2 = 2;
constexpr uint8_t TA_GEAR_INDEX_3 = 3;

static constexpr uint32_t TA_TIMEOUT_MS_GEAR0 = 512;
static constexpr uint32_t TA_TIMEOUT_MS_GEAR1 = 1000;
static constexpr uint32_t TA_TIMEOUT_MS_GEAR2 = 8000;
static constexpr uint32_t TA_TIMEOUT_MS_GEAR3 = 32000;

constexpr uint8_t AT_GEAR_MIN = 0;
constexpr uint8_t AT_GEAR_MAX = 3;
constexpr uint8_t AT_GEAR_DEFAULT = 2;
constexpr uint32_t AT_TIMEOUT_MAP[4] = {16, 128, 1000, 4000};

/// 与 GetTpInfo / ReleaseTpInfo 中 info、req 两级 map 的 qos 键一致（param.qos 低 8 位）
using QosKey = uint32_t;

using GetTpInfoParam = struct GetTpInfoParamDef {
    CommAddr locAddr{};
    CommAddr rmtAddr{};
    TpProtocol tpProtocol{TpProtocol::CTP};
    /// 参与 TP/SL 分组与缓存键（0–7），连接侧已归一化
    uint32_t qos{Hccl::UB_QOS_DEFAULT};
    /// 非 0 时与 slBitmap 可用档位数取 min，作为 SL 分组上限（见 TpMgr::ResolveSlAvailableCntForPolicy）。
    /// 预留：当前连接/CCU 等调用方均传 0；待管控面或连接侧按需注入非 0 后生效。
    uint32_t slLevelCount{0};
    /// 环回等场景：首 TPID + 掩码内最小 SL（TpMgr 策略唯一依据；CCU 环回亦靠此字段表达）
    bool loopFirstTpLowestSl{false};
    /// CCU 环回调用方标记，仅用于 Describe/排查；TpMgr 不读取，行为由 loopFirstTpLowestSl 决定
    bool ccuLoopbackGetTpInfo{false};

    explicit GetTpInfoParamDef() = default;
    GetTpInfoParamDef(const CommAddr &locAddr, const CommAddr &rmtAddr, TpProtocol tpProtocol)
        : locAddr(locAddr), rmtAddr(rmtAddr), tpProtocol(tpProtocol) {}

    std::string Describe() const {
        Hccl::IpAddress locIpAddr{}, rmtIpAddr{};
        (void)CommAddrToIpAddress(locAddr, locIpAddr);
        (void)CommAddrToIpAddress(rmtAddr, rmtIpAddr);
        return Hccl::StringFormat(
            "RaUbGetTpInfoParam[locAddr=%s, rmtAddr=%s, tpProtocol=%s, qos=%u, loopFirstTpLowestSl=%d, ccuLoop=%d]",
            locIpAddr.Describe().c_str(), rmtIpAddr.Describe().c_str(), tpProtocol.Describe().c_str(), qos,
            static_cast<int>(loopFirstTpLowestSl), static_cast<int>(ccuLoopbackGetTpInfo));
    }
};

/*
 * TP信息，当前申请TpHandle，不感知具体TP信息，当前仅支持TP与CTP
 * tpHandle: 对应管控面的TPID与相关资源，URMA通过引用计数管理申请和销毁TP
 */
using TpHandle = uint64_t;
struct TpInfo {
    TpHandle tpHandle{0};
    uint32_t mappedJettyPriority{0};
    bool hasMappedJettyPriority{false};
    /// CalcTaTimeout 结果；RTP/UBOE 在 GetTpInfo Finalize 时填充，CTP 由调用方按环境配置处理。
    uint8_t jettyErrTimeout{0};
    bool hasJettyErrTimeout{false};
    TpInfo() = default;
    explicit TpInfo(const TpHandle handle)
        : tpHandle(handle) {}
};

struct TpAttrInfo {
    struct TpAttr tpAttr{0};

    TpAttrInfo() = default;
    TpAttrInfo(const struct TpAttr &attr)
        : tpAttr(attr) {}
};

using GetTpAttrParam = struct GetTpAttrParamDef {
    TpHandle tpHandle{0};
    uint32_t attrBitmap{0};

    explicit GetTpAttrParamDef() = default;
    GetTpAttrParamDef(const TpHandle tpHandle, const uint32_t attrBitmap)
        : tpHandle(tpHandle), attrBitmap(attrBitmap){};

    std::string Describe() const {
        return Hccl::StringFormat("GetTpAttrParam[tpHandle=0x%llx, attrBitmap=0x%x]",
            tpHandle, attrBitmap);
    }
};

class TpMgr {
public:
    static TpMgr &GetInstance(const uint32_t devicePhyId);
    /// 异步获取 TP：缓存命中立即 SUCCESS，否则轮询直至 List/Attr/Mapping/Commit 完成。
    HcclResult GetTpInfo(const GetTpInfoParam &param, TpInfo &tpInfo);
    HcclResult GetTpAttr(const GetTpAttrParam &param, TpAttrInfo &tpAttrInfo, CtxHandle ctxHandle);
    // unimport jetty 会 URMA 销毁 tp 资源，hccl 配套删除记录
    /// 递减 tpInfoCache 引用计数，归零时移除缓存项。
    HcclResult ReleaseTpInfo(const GetTpInfoParam &param, const TpInfo &tpInfo);
    HcclResult ReleaseTpAttr(const TpHandle tpHandle, const TpAttrInfo &tpAttrInfo);
    static HcclResult GetTpTotalTimeout(const TpAttrInfo &tpAttrInfo, uint32_t &tpTimeOutMs);
    static uint8_t CalcTaTimeout(const TpAttrInfo &tpAttrInfo);

private:
     struct TpInfoCtx {
        TpInfo tpInfo{};
        uint32_t useCnt{0};

        TpInfoCtx() = default;
        TpInfoCtx(const TpInfo &info, const uint32_t cnt)
            : tpInfo(info), useCnt(cnt) {}
    };

    struct TpAttrCtx {
        TpAttrInfo tpAttrInfo{};
        uint32_t useCnt{0};

        TpAttrCtx() = default;
        TpAttrCtx(const TpAttrInfo &info, const uint32_t cnt)
            : tpAttrInfo(info), useCnt(cnt) {}
    };

    struct LinkAttrCtx {
        TpAttr tpAttr{};
        bool valid{false};
    };

    /*
     * GetTpInfo 异步状态机：
     *   WAIT_LIST   → RaGetTpInfoListAsync
     *   WAIT_ATTR   → RaGetTpAttrAsync(list[0]) + Mapping；linkCache 命中则跳过 Get
     *   WAIT_COMMIT → RaSetTpAttrAsync(selectedTp, SL；UBOE 且 dscpConfigMode==0 时 SL+DSCP 同次)；CTP 跳过
     * timeout(at/retry) 仅 bootstrap 读取，供 CalcTaTimeout；不写 SetTpAttr。
     */
    enum class ReqPhase : uint8_t { WAIT_LIST = 0, WAIT_ATTR = 1, WAIT_COMMIT = 2 };
    struct RequestCtx {
        ReqPhase phase{ReqPhase::WAIT_LIST};
        RequestHandle handle{0};
        uint32_t tpInfoNum{0};
        std::vector<char> dataBuffer;
        /// list[0] bootstrap 或 linkCache 中的链路级属性（slBitmap / at / retry / UBOE dscp 等）
        TpAttr linkTpAttr{};
        uint32_t bootstrapAttrBitmap{0};
        uint32_t tpListIndex{0};
        uint32_t mappedSl{0};
        TpHandle selectedTpHandle{0};
    };

    struct TpAttrRequestCtx {
        RequestHandle handle{0};
        struct TpAttr tpAttr{0};
    };

    using TpAttrCtxMap = std::unordered_map<TpHandle, TpAttrCtx>;
    using TpAttrReqCtxMap = std::unordered_map<TpHandle, TpAttrRequestCtx>;

    /// 三级索引：先按本端 IP，再按对端 IP，最后按 QoS 档（0–7，与 GetTpInfo/TP-SL 策略里用的档位一致）。
    using InfoQosMap = std::unordered_map<uint32_t, TpInfoCtx>;
    using InfoRmtMap = std::unordered_map<Hccl::IpAddress, InfoQosMap>;
    using InfoCtxMap = std::unordered_map<Hccl::IpAddress, InfoRmtMap>;
    using ReqQosMap = std::unordered_map<uint32_t, RequestCtx>;
    using ReqRmtMap = std::unordered_map<Hccl::IpAddress, ReqQosMap>;
    using ReqCtxMap = std::unordered_map<Hccl::IpAddress, ReqRmtMap>;
    using LinkAttrRmtMap = std::unordered_map<Hccl::IpAddress, LinkAttrCtx>;
    using LinkAttrMap = std::unordered_map<Hccl::IpAddress, LinkAttrRmtMap>;

private:
    TpMgr() = default;
    ~TpMgr() = default;
    TpMgr(const TpMgr &that) = delete;
    TpMgr &operator=(const TpMgr &that) = delete;

    /// GetTpInfo 快路径：查 tpInfoCache[loc×rmt×qos]；命中 useCnt++，未命中返回 NOT_FOUND。
    HcclResult FindAndGetTpInfo(const GetTpInfoParam &param, TpInfo &tpInfo);
    HcclResult FindAndGetTpAttr(const TpHandle tpHandle, TpAttrInfo &tpAttrInfo);
    /// 在三级 infoMap 中定位 loc/rmt/qos 条目迭代器（供 ReleaseTpInfo 级联删除用）。
    HcclResult LookupInfoCtxEntry(InfoCtxMap &infoMap, const Hccl::IpAddress &locAddr, const Hccl::IpAddress &rmtAddr,
        QosKey qosKey, InfoCtxMap::iterator &lit, InfoRmtMap::iterator &rit, InfoQosMap::iterator &qosIt) const;
    /// GetTpInfo 状态机驱动：poll 异步结果并按 phase 推进。
    HcclResult PollGetTpInfoReqCtx(std::unique_lock<std::mutex> &reqCtxLock, const GetTpInfoParam &param, TpInfo &tpInfo);
    /// 首次请求：提交 RaGetTpInfoListAsync，进入 WAIT_LIST。
    HcclResult BeginGetTpInfoListRequest(const GetTpInfoParam &param, ReqQosMap &qosMap, QosKey qosKey);
    /// WAIT_LIST 完成：拉 linkAttr 或走缓存，再 Mapping/Commit。
    HcclResult AdvanceFromWaitList(const GetTpInfoParam &param, RequestCtx &reqCtx, ReqQosMap &qosMap,
        ReqQosMap::iterator it, std::unique_lock<std::mutex> &reqCtxLock, TpInfo &tpInfo);
    /// WAIT_ATTR 完成：写入 linkCache，执行 Mapping/Commit。
    HcclResult AdvanceFromWaitAttr(const GetTpInfoParam &param, RequestCtx &reqCtx, ReqQosMap &qosMap,
        ReqQosMap::iterator it, std::unique_lock<std::mutex> &reqCtxLock, TpInfo &tpInfo);
    /// WAIT_COMMIT 完成：Finalize。
    HcclResult AdvanceFromWaitCommit(const GetTpInfoParam &param, RequestCtx &reqCtx, ReqQosMap &qosMap,
        ReqQosMap::iterator it, std::unique_lock<std::mutex> &reqCtxLock, TpInfo &tpInfo);
    /// 同步 QoS Mapping，CTP 直接完成，RTP/UBOE 提交 SetTpAttr Commit。
    HcclResult RunMappingAndContinue(const GetTpInfoParam &param, RequestCtx &reqCtx, ReqQosMap &qosMap,
        ReqQosMap::iterator it, std::unique_lock<std::mutex> &reqCtxLock, TpInfo &tpInfo);
    /// 写入 tpInfoCache/tpAttrCache，清理 reqCtx，返回 SUCCESS。
    HcclResult FinalizeGetTpInfo(RequestCtx reqCtx, const GetTpInfoParam &param, ReqQosMap &qosMap,
        ReqQosMap::iterator it, std::unique_lock<std::mutex> &reqCtxLock, TpInfo &tpInfo);

    /// 查 linkAttrCache[loc×rmt×protocol]，命中返回 list[0] /bootstrap 属性。
    bool TryGetLinkAttrCache(const GetTpInfoParam &param, TpAttr &outAttr);
    /// 将 list[0] 拉到的链路级 TpAttr 写入 linkAttrCache。
    void PutLinkAttrCache(const GetTpInfoParam &param, const TpAttr &attr);

    /// 初始化 reqCtx 并提交 RaGetTpInfoListAsync。
    HcclResult StartGetTpInfoListRequest(const GetTpInfoParam &param, RequestCtx &reqCtx) const;
    /// 对 list[0] 提交 RaGetTpAttrAsync（slBitmap/at/retry 等），进入 WAIT_ATTR。
    HcclResult StartBootstrapLinkAttr(const GetTpInfoParam &param, RequestCtx &reqCtx) const;
    /// RTP 写 SL；UBOE 且需写 dscp 时在同一次 SetTpAttr 中写 SL + DSCP。
    HcclResult StartCommitTpAttr(const GetTpInfoParam &param, RequestCtx &reqCtx) const;
    HcclResult StartGetTpAttrRequest(const GetTpAttrParam &param, TpAttrRequestCtx &reqCtx, CtxHandle ctxHandle) const;
    HcclResult HandleCompletedTpAttrRequest(const TpAttrRequestCtx reqCtx, const TpHandle tpHandle,
        TpAttrInfo &tpAttrInfo);

    /// 按协议取 tpInfo 结果缓存表。
    InfoCtxMap &GetInfoCtxMap(const TpProtocol tpProtocol);
    /// 按协议取 GetTpInfo 进行中的 reqCtx 表。
    ReqCtxMap  &GetReqCtxMap(const TpProtocol tpProtocol);
    /// 按协议取 linkAttr 链路缓存表。
    LinkAttrMap &GetLinkAttrMap(const TpProtocol tpProtocol);
    /// 按协议取 tpInfo 缓存互斥锁。
    std::mutex &GetInfoCtxMutex(const TpProtocol tpProtocol);
    /// 按协议取 GetTpInfo reqCtx 互斥锁。
    std::mutex &GetReqCtxMutex(const TpProtocol tpProtocol);

private:
    bool initFlag_{false};
    uint32_t devPhyId_{0};

    InfoCtxMap ctpInfoMap_;
    ReqCtxMap  ctpReqMap_;
    LinkAttrMap ctpLinkAttrMap_;

    InfoCtxMap rtpInfoMap_;
    ReqCtxMap  rtpReqMap_;
    LinkAttrMap rtpLinkAttrMap_;

    InfoCtxMap uboeInfoMap_;
    ReqCtxMap  uboeReqMap_;
    LinkAttrMap uboeLinkAttrMap_;

    TpAttrCtxMap tpAttrCtxMap_;
    TpAttrReqCtxMap tpAttrReqCtxMap_;

    std::mutex ctpInfoMutex_;
    std::mutex ctpReqMutex_;

    std::mutex rtpInfoMutex_;
    std::mutex rtpReqMutex_;

    std::mutex uboeInfoMutex_;
    std::mutex uboeReqMutex_;

    std::mutex tpAttrCtxMutex_;
    std::mutex tpAttrReqMutex_;
};

} // namespace hcomm

#endif // TP_MGR_H

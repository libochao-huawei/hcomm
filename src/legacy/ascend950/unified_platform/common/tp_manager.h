/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef HCCLV2_TP_MANAGER_H
#define HCCLV2_TP_MANAGER_H

#include <cstdint>
#include <functional>
#include <mutex>
#include <vector>
#include <unordered_map>

#include "hccl_types.h"
#include "ip_address.h"
#include "orion_adapter_hccp.h"

namespace Hccl {

constexpr uint8_t TA_GEAR_INDEX_0 = 0;
constexpr uint8_t TA_GEAR_INDEX_1 = 1;
constexpr uint8_t TA_GEAR_INDEX_2 = 2;
constexpr uint8_t TA_GEAR_INDEX_3 = 3;

constexpr uint8_t TA_HW_GEAR0_BASE = 0;
constexpr uint8_t TA_HW_GEAR1_BASE = 8;
constexpr uint8_t TA_HW_GEAR2_BASE = 16;
constexpr uint8_t TA_HW_GEAR3_BASE = 24;

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

/*
 * TP信息，当前申请TpHandle，不感知具体TP信息，当前仅支持TP与CTP
 * tpHandle: 对应管控面的TPID与相关资源，URMA通过引用计数管理申请和销毁TP
 */
using TpHandle = uint64_t;
struct TpInfo {
    TpHandle tpHandle{0};
    uint32_t mappedJettyPriority{0};
    bool hasMappedJettyPriority{false};
    /// CalcTaTimeout 结果；TP/UBOE 在 GetTpInfo Finalize 时填充，CTP 由调用方按环境配置处理。
    uint8_t jettyErrTimeout{0};
    bool hasJettyErrTimeout{false};

    TpInfo() = default;
    TpInfo(const TpHandle handle)
        : tpHandle(handle) {}
};

struct TpAttrInfo {
    struct TpAttr tpAttr{0};

    TpAttrInfo() = default;
    TpAttrInfo(const struct TpAttr &attr)
        : tpAttr(attr) {}
};

struct GetTpAttrParam {
    TpHandle tpHandle{0};
    uint32_t attrBitmap{0};

    GetTpAttrParam() = default;
    GetTpAttrParam(const TpHandle handle, const uint32_t bitmap)
        : tpHandle(handle), attrBitmap(bitmap) {}

    std::string Describe() const {
        return Hccl::StringFormat("GetTpAttrParam[tpHandle=0x%llx, attrBitmap=0x%x]",
            tpHandle, attrBitmap);
    }
};

class TpManager {
public:
    static TpManager &GetInstance(const int32_t deviceLogicId);
    void Init();
    /// `isSync==true`：HostUb 一次同步完成 list→attr→Mapping→commit；false 走 Device 异步状态机。
    HcclResult GetTpInfo(const RaUbGetTpInfoParam &param, TpInfo &tpInfo, bool isSync = false);
    // unimport jetty 会 URMA 销毁 tp 资源，hccl 配套删除记录
    HcclResult ReleaseTpInfo(const RaUbGetTpInfoParam &param, const TpInfo &tpInfo);
    HcclResult GetTpAttr(const GetTpAttrParam &param, TpAttrInfo &tpAttrInfo, RdmaHandle rdmaHandle);
    HcclResult ReleaseTpAttr(const TpHandle tpHandle, const TpAttrInfo &tpAttrInfo);

    static HcclResult GetTpTotalTimeout(const TpAttrInfo &tpAttrInfo, uint32_t &tpTimeOutMs);
    static uint8_t CalcTaTimeout(const TpAttrInfo &tpAttrInfo);
    static uint32_t TaHwValueToMs(uint8_t hwValue);
    static uint8_t FindMinTaHwValue(uint32_t tpTotalTimeoutMs);

private:
    bool initFlag{false};
    uint32_t devLogicId{0};
    uint32_t devPhyId{0};

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
     * GetTpInfo 异步状态机（与 TpMgr 对齐）：
     *   WAIT_LIST   → RaUbGetTpInfo(Async)
     *   WAIT_ATTR   → RaGetTpAttrAsync(list[0])；linkCache 命中则跳过 Get
     *   WAIT_COMMIT → HrtRaSetTpAttrAsync(selectedTp, SL；UBOE 且 dscpConfigMode==0 时 SL+DSCP 同次)
     */
    enum class ReqPhase : uint8_t { WAIT_LIST = 0, WAIT_ATTR = 1, WAIT_COMMIT = 2 };
    struct RequestCtx {
        ReqPhase phase{ReqPhase::WAIT_LIST};
        RequestHandle handle{0};
        uint32_t tpInfoNum{0};
        std::vector<char_t> dataBuffer;
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

    /// 三级索引：先按本端 IP，再按对端 IP，最后按 QoS 键（`QosKey`：`param.qos & 0xFF`）。
    using InfoQosMap = std::unordered_map<uint32_t, TpInfoCtx>;
    using InfoRmtMap = std::unordered_map<IpAddress, InfoQosMap>;
    using InfoCtxMap = std::unordered_map<IpAddress, InfoRmtMap>;
    using ReqQosMap = std::unordered_map<uint32_t, RequestCtx>;
    using ReqRmtMap = std::unordered_map<IpAddress, ReqQosMap>;
    using ReqCtxMap = std::unordered_map<IpAddress, ReqRmtMap>;
    using LinkAttrRmtMap = std::unordered_map<IpAddress, LinkAttrCtx>;
    using LinkAttrMap = std::unordered_map<IpAddress, LinkAttrRmtMap>;

    using TpAttrCtxMap = std::unordered_map<TpHandle, TpAttrCtx>;
    using TpAttrReqCtxMap = std::unordered_map<TpHandle, TpAttrRequestCtx>;

    InfoCtxMap ctpInfoMap;
    ReqCtxMap  ctpReqMap;
    LinkAttrMap ctpLinkAttrMap;

    InfoCtxMap tpInfoMap;
    ReqCtxMap  tpReqMap;
    LinkAttrMap tpLinkAttrMap;

    InfoCtxMap uboeInfoMap;
    ReqCtxMap  uboeReqMap;
    LinkAttrMap uboeLinkAttrMap;

    TpAttrCtxMap tpAttrCtxMap;
    TpAttrReqCtxMap tpAttrReqCtxMap;

    std::mutex ctpInfoMutex;
    std::mutex ctpReqMutex;

    std::mutex tpInfoMutex;
    std::mutex tpReqMutex;

    std::mutex uboeInfoMutex;
    std::mutex uboeReqMutex;

    std::mutex tpAttrCtxMutex;
    std::mutex tpAttrReqMutex;

    TpManager() = default;
    ~TpManager() = default;
    TpManager(const TpManager &that) = delete;
    TpManager &operator=(const TpManager &that) = delete;

    HcclResult FindAndGetTpInfo(const RaUbGetTpInfoParam &param, TpInfo &tpInfo);
    HcclResult RunHostSyncGetTpInfo(const RaUbGetTpInfoParam &param, TpInfo &tpInfo);
    HcclResult RunAsyncGetTpInfo(const RaUbGetTpInfoParam &param, TpInfo &tpInfo);
    HcclResult SelectTpByQosSl(const RaUbGetTpInfoParam &param, RequestCtx &reqCtx);
    HcclResult StoreGetTpInfoResult(const RaUbGetTpInfoParam &param, const RequestCtx &reqCtx, TpInfo &tpInfo);
    void SyncCommitTpAttr(const RaUbGetTpInfoParam &param, const RequestCtx &reqCtx) const;
    HcclResult SelectTpAndContinueAsync(const RaUbGetTpInfoParam &param, RequestCtx &reqCtx, ReqQosMap &qosMap,
        ReqQosMap::iterator it, std::unique_lock<std::mutex> &reqCtxLock, TpInfo &tpInfo);
    /// WAIT_LIST 完成：拉 linkAttr 或走缓存，再 Mapping/Commit。
    HcclResult AdvanceFromWaitList(const RaUbGetTpInfoParam &param, RequestCtx &reqCtx, ReqQosMap &qosMap,
        ReqQosMap::iterator it, std::unique_lock<std::mutex> &reqCtxLock, TpInfo &tpInfo);
    /// WAIT_ATTR 完成：写入 linkCache，执行 Mapping/Commit。
    HcclResult AdvanceFromWaitAttr(const RaUbGetTpInfoParam &param, RequestCtx &reqCtx, ReqQosMap &qosMap,
        ReqQosMap::iterator it, std::unique_lock<std::mutex> &reqCtxLock, TpInfo &tpInfo);
    HcclResult AdvanceFromWaitCommit(const RaUbGetTpInfoParam &param, RequestCtx &reqCtx, ReqQosMap &qosMap,
        ReqQosMap::iterator it, std::unique_lock<std::mutex> &reqCtxLock, TpInfo &tpInfo);
    HcclResult FinalizeGetTpInfo(RequestCtx reqCtx, const RaUbGetTpInfoParam &param, ReqQosMap &qosMap,
        ReqQosMap::iterator it, std::unique_lock<std::mutex> &reqCtxLock, TpInfo &tpInfo);
    void StartGetTpInfoListRequest(const RaUbGetTpInfoParam &param, RequestCtx &reqCtx, bool isSync) const;
    HcclResult StartBootstrapLinkAttr(const RaUbGetTpInfoParam &param, RequestCtx &reqCtx) const;
    HcclResult StartCommitTpAttr(const RaUbGetTpInfoParam &param, RequestCtx &reqCtx) const;
    bool TryGetLinkAttrCache(const RaUbGetTpInfoParam &param, TpAttr &outAttr);
    void PutLinkAttrCache(const RaUbGetTpInfoParam &param, const TpAttr &attr);
    HcclResult FindAndGetTpAttr(const TpHandle tpHandle, TpAttrInfo &tpAttrInfo);
    HcclResult StartGetTpAttrRequest(const GetTpAttrParam &param, TpAttrRequestCtx &reqCtx, RdmaHandle rdmaHandle) const;
    HcclResult HandleCompletedTpAttrRequest(const TpAttrRequestCtx reqCtx, const TpHandle tpHandle,
        TpAttrInfo &tpAttrInfo);

    bool CheckRequestResult(RequestHandle &reqHandle) const;
    InfoCtxMap &GetInfoCtxMap(const TpProtocol tpProtocol);
    ReqCtxMap  &GetReqCtxMap(const TpProtocol tpProtocol);
    LinkAttrMap &GetLinkAttrMap(const TpProtocol tpProtocol);
    std::mutex &GetInfoCtxMutex(const TpProtocol tpProtocol);
    std::mutex &GetReqCtxMutex(const TpProtocol tpProtocol);
};

} // namespace Hccl

#endif // HCCLV2_TP_MANAGER_H

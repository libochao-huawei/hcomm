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

    TpInfo() = default;
    TpInfo(const TpHandle handle)
        : tpHandle(handle) {}
};

class TpManager {
public:
    static TpManager &GetInstance(const int32_t deviceLogicId);
    void Init();
    /// `isSync==true`：主仓 HostUb 同步路径（GetByAddr + `RaUbGetTpInfo`）；默认 `false` 走异步 + QoS/SL 逻辑。
    HcclResult GetTpInfo(const RaUbGetTpInfoParam &param, TpInfo &tpInfo, bool isSync = false);
    // unimport jetty 会 URMA 销毁 tp 资源，hccl 配套删除记录
    HcclResult ReleaseTpInfo(const RaUbGetTpInfoParam &param, const TpInfo &tpInfo);
    void SetIsHost();

private:
    bool initFlag{false};
    bool isHost{false};
    uint32_t devLogicId{0};
    uint32_t devPhyId{0};

    struct TpInfoCtx {
        TpInfo tpInfo{};
        uint32_t useCnt{0};

        TpInfoCtx() = default;
        TpInfoCtx(const TpInfo &info, const uint32_t cnt)
            : tpInfo(info), useCnt(cnt) {}
    };

    /*
    * Request上下文，保存查询TP信息相关调用异步接口出参
    * handle: 异步接口调用handle，用于查询处理结果
    * tpInfoNum: 查询到的TP信息个数，当前为复用TP，只会申请1个
    * isSync: 与主仓一致；true 时走同步 `RaUbGetTpInfo`，不参与异步 handle 轮询。
    * dataBuffer: 查询到的TP信息数据，原始数据保留缓冲区
    */
    struct RequestCtx {
        enum class ReqPhase : uint8_t { WAIT_LIST = 0, WAIT_TP_ATTR = 1 };
        ReqPhase phase{ReqPhase::WAIT_LIST};
        RequestHandle handle{0};
        uint32_t tpInfoNum{0};
        bool isSync{false};
        std::vector<char_t> dataBuffer;
        TpAttr tpAttr{};
        uint32_t tpAttrBitmap{0};
    };

    /// 三级索引：先按本端 IP，再按对端 IP，最后按 QoS 键（`QosKey`：`param.qos & 0xFF`，与 next `TpMgr` 一致）。
    /// 每一层的键要么是 IpAddress，要么是 uint32_t，都可直接放进 unordered_map，不必把 (本端,对端) 拼成 pair 再写自定义哈希。
    using InfoQosMap = std::unordered_map<uint32_t, TpInfoCtx>;
    using InfoRmtMap = std::unordered_map<IpAddress, InfoQosMap>;
    using InfoCtxMap = std::unordered_map<IpAddress, InfoRmtMap>;
    using ReqQosMap = std::unordered_map<uint32_t, RequestCtx>;
    using ReqRmtMap = std::unordered_map<IpAddress, ReqQosMap>;
    using ReqCtxMap = std::unordered_map<IpAddress, ReqRmtMap>;

    InfoCtxMap ctpInfoMap;
    ReqCtxMap  ctpReqMap;

    InfoCtxMap tpInfoMap;
    ReqCtxMap  tpReqMap;

    InfoCtxMap uboeInfoMap;
    ReqCtxMap  uboeReqMap;

    std::mutex ctpInfoMutex;
    std::mutex ctpReqMutex;

    std::mutex tpInfoMutex;
    std::mutex tpReqMutex;

    std::mutex uboeInfoMutex;
    std::mutex uboeReqMutex;

    TpManager() = default;
    ~TpManager() = default;
    TpManager(const TpManager &that) = delete;
    TpManager &operator=(const TpManager &that) = delete;

    HcclResult FindAndGetTpInfo(const RaUbGetTpInfoParam &param, TpInfo &tpInfo);
    void StartGetTpInfoListRequest(const RaUbGetTpInfoParam &param, RequestCtx &reqCtx, bool isSync) const;
    void StartGetTpAttrForFirstTpDevice(const RaUbGetTpInfoParam &param, RequestCtx &reqCtx);
    HcclResult HandleCompletedRequest(RequestCtx reqCtx, const RaUbGetTpInfoParam &param, TpInfo &tpInfo,
        bool withSlPolicy);
    HcclResult MapTpInfoFromTpAttr(const RaUbGetTpInfoParam &param, const RequestCtx &reqCtx, TpInfo &outTpInfo);

    bool CheckRequestResult(RequestHandle &reqHandle) const;
    InfoCtxMap &GetInfoCtxMap(const TpProtocol tpProtocol);
    ReqCtxMap  &GetReqCtxMap(const TpProtocol tpProtocol);
    std::mutex &GetInfoCtxMutex(const TpProtocol tpProtocol);
    std::mutex &GetReqCtxMutex(const TpProtocol tpProtocol);
};

} // namespace Hccl

#endif // HCCLV2_TP_MANAGER_H

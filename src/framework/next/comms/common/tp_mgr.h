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

#include <mutex>
#include <vector>
#include <unordered_map>

#include "hccl_types.h"
#include "hcomm_adapter_hccp.h"
#include "hccp_tp.h"
#include "orion_adpt_utils.h"

namespace hcomm {

using GetTpInfoParam = struct GetTpInfoParamDef {
    CommAddr locAddr{};
    CommAddr rmtAddr{};
    TpProtocol tpProtocol{TpProtocol::CTP};
    /// 参与 TP/SL 分组与缓存键（0–7），连接侧已归一化
    uint32_t qos{EnvConfig::UB_QOS_DEFAULT};
    /// 非 0 时与 sl_available 推导的 M 取 min 作为可用档位数上限
    uint32_t slLevelCount{0};
    /// 环回等场景：首 TPID + 掩码内最小 SL
    bool loopFirstTpLowestSl{false};

    explicit GetTpInfoParamDef() = default;
    GetTpInfoParamDef(const CommAddr &locAddr, const CommAddr &rmtAddr, TpProtocol tpProtocol)
        : locAddr(locAddr), rmtAddr(rmtAddr), tpProtocol(tpProtocol) {}

    std::string Describe() const {
        Hccl::IpAddress locIpAddr{}, rmtIpAddr{};
        (void)CommAddrToIpAddress(locAddr, locIpAddr);
        (void)CommAddrToIpAddress(rmtAddr, rmtIpAddr);
        return Hccl::StringFormat(
            "RaUbGetTpInfoParam[locAddr=%s, rmtAddr=%s, tpProtocol=%s, qos=%u, loopFirstTpLowestSl=%d]",
            locIpAddr.Describe().c_str(), rmtIpAddr.Describe().c_str(), tpProtocol.Describe().c_str(), qos,
            static_cast<int>(loopFirstTpLowestSl));
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

    TpInfo() = default;
    explicit TpInfo(const TpHandle handle)
        : tpHandle(handle) {}
};

class TpMgr {
public:
    static TpMgr &GetInstance(const uint32_t devicePhyId);
    HcclResult GetTpInfo(const GetTpInfoParam &param, TpInfo &tpInfo);
    // unimport jetty 会 URMA 销毁 tp 资源，hccl 配套删除记录
    HcclResult ReleaseTpInfo(const GetTpInfoParam &param, const TpInfo &tpInfo);

private:
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
    * dataBuffer: 查询到的TP信息数据，原始数据保留缓冲区
    */
    enum class ReqPhase : uint8_t { WAIT_LIST = 0, WAIT_TP_ATTR = 1 };

    struct RequestCtx {
        ReqPhase phase{ReqPhase::WAIT_LIST};
        RequestHandle handle{0};
        uint32_t tpInfoNum{0};
        std::vector<char> dataBuffer;
        TpAttr tpAttr{};
        uint32_t tpAttrBitmap{0};
    };

    using QosKey = uint32_t;
    /// 本/对端地址键（与 GetTpCfg 中 EID 同源）+ qos：同一 EID 对、同协议、同 qos 的多个通信域复用同一 TpInfo；不同 EID 对互不共享。
    using InfoCtxMap = std::unordered_map<Hccl::IpAddress,
        std::unordered_map<Hccl::IpAddress, std::unordered_map<QosKey, TpInfoCtx>>>;
    using ReqCtxMap = std::unordered_map<Hccl::IpAddress,
        std::unordered_map<Hccl::IpAddress, std::unordered_map<QosKey, RequestCtx>>>;

private:
    TpMgr() = default;
    ~TpMgr() = default;
    TpMgr(const TpMgr &that) = delete;
    TpMgr &operator=(const TpMgr &that) = delete;

    HcclResult FindAndGetTpInfo(const GetTpInfoParam &param, TpInfo &tpInfo);
    HcclResult StartGetTpInfoListRequest(const GetTpInfoParam &param, RequestCtx &reqCtx) const;
    HcclResult StartGetTpAttrForFirstTp(const GetTpInfoParam &param, RequestCtx &reqCtx) const;
    HcclResult HandleCompletedRequest(RequestCtx reqCtx, const GetTpInfoParam &param, TpInfo &tpInfo);

    static QosKey TpInfoCacheQos(const GetTpInfoParam &param);

    InfoCtxMap &GetInfoCtxMap(const TpProtocol tpProtocol);
    ReqCtxMap  &GetReqCtxMap(const TpProtocol tpProtocol);
    std::mutex &GetInfoCtxMutex(const TpProtocol tpProtocol);
    std::mutex &GetReqCtxMutex(const TpProtocol tpProtocol);

private:
    bool initFlag_{false};
    uint32_t devPhyId_{0};

    InfoCtxMap ctpInfoMap_;
    ReqCtxMap  ctpReqMap_;

    InfoCtxMap rtpInfoMap_;
    ReqCtxMap  rtpReqMap_;

    std::mutex ctpInfoMutex_;
    std::mutex ctpReqMutex_;

    std::mutex rtpInfoMutex_;
    std::mutex rtpReqMutex_;
};

} // namespace hcomm

#endif // TP_MGR_H

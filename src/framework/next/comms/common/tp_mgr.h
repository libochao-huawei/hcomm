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
#include "orion_adpt_utils.h"

namespace hcomm {

/**
 * 同一对 EID 上可有 1～N 个通信域（N 无固定上限）。映射路径下：相同 (locAddr,rmtAddr,tpProtocol)
 * 且相同 commHcclQos → TpMgr 缓存命中，复用同一 tpHandle（TPID）与 mappedJettyPriority（SL）；
 * TpInfoCtx::useCnt 引用计数，ReleaseTpInfo 须与 GetTpInfo 使用一致的 GetTpInfoParam。
 */
using GetTpInfoParam = struct GetTpInfoParamDef {
    CommAddr locAddr{};
    CommAddr rmtAddr{};
    TpProtocol tpProtocol{TpProtocol::CTP};

    /** EID + UBC CTP/RTP：按 commHcclQoS 与 TP/SL 能力做映射；否则保持 false 走旧路径 */
    bool useUbTpSlMapping{false};
    /** 通信域 QoS（0–7），映射路径下参与 TpInfoCacheKey 分桶；同键多连接共享 TPID/SL */
    uint32_t commHcclQos{0};
    /** SL 档位数 M：0 表示映射路径下由首个 TP 的 get_tp_attr 推导；非 0 为显式覆盖/兜底上限 */
    uint32_t slLevelCount{0};
    /**
     * 环回（loc==rmt）：在 useUbTpSlMapping 且完成 list+get_tp_attr 后，固定使用列表第 0 个 TPID
     * 与最低 SL 槽位（策略顺序下标 0，mappedJettyPriority=0），不走 ApplyUbcQosTpSlPolicy。
     */
    bool loopFirstTpLowestSl{false};

    explicit GetTpInfoParamDef() = default;
    GetTpInfoParamDef(const CommAddr &locAddr, const CommAddr &rmtAddr, TpProtocol tpProtocol)
        : locAddr(locAddr), rmtAddr(rmtAddr), tpProtocol(tpProtocol){};

    std::string Describe() const {
        Hccl::IpAddress locIpAddr{}, rmtIpAddr{};
        (void)CommAddrToIpAddress(locAddr, locIpAddr);
        (void)CommAddrToIpAddress(rmtAddr, rmtIpAddr);
        return Hccl::StringFormat(
            "RaUbGetTpInfoParam[locAddr=%s, rmtAddr=%s, tpProtocol=%s, ubMap=%u qos=%u mSl=%u loop1st=%u]",
            locIpAddr.Describe().c_str(), rmtIpAddr.Describe().c_str(),
            tpProtocol.Describe().c_str(), static_cast<unsigned>(useUbTpSlMapping),
            commHcclQos, slLevelCount, static_cast<unsigned>(loopFirstTpLowestSl));
    }
};

/*
 * TP信息，当前申请TpHandle，不感知具体TP信息，当前仅支持TP与CTP
 * tpHandle: 对应管控面的TPID与相关资源，URMA通过引用计数管理申请和销毁TP
 */
using TpHandle = uint64_t;
struct TpInfo {
    TpHandle tpHandle{0};
    /** 与 set_tp_attr 应对齐的 SL，经 Jetty priority 下发；非映射路径不设置 */
    uint8_t mappedJettyPriority{0};
    bool hasMappedJettyPriority{false};

    TpInfo() = default;
    TpInfo(const TpHandle handle)
        : tpHandle(handle) {}
};

class TpMgr {
public:
    static TpMgr &GetInstance(const uint32_t devicePhyId);
    HcclResult GetTpInfo(const GetTpInfoParam &param, TpInfo &tpInfo);
    // unimport jetty 会 URMA 销毁 tp 资源，hccl 配套删除记录
    HcclResult ReleaseTpInfo(const GetTpInfoParam &param, const TpInfo &tpInfo);

private:
    enum class TpInfoReqPhase : uint8_t {
        kWaitList = 0,
        kWaitTpAttr = 1,
    };

     /** 同一 (loc,rmt,qosKey) 多路 GetTpInfo 共用条目，useCnt 递减至 0 才 erase */
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
    * tpInfoNum: 查询到的TP信息个数（RaGetTpInfoListAsync 回填）
    * dataBuffer: 查询到的TP信息数据，原始数据保留缓冲区
    */
    struct RequestCtx {
        RequestHandle handle{0};
        uint32_t tpInfoNum{0};
        std::vector<char> dataBuffer;
        TpInfoReqPhase reqPhase{TpInfoReqPhase::kWaitList};
        uint32_t tpAttrBitmap{0};
        std::vector<char> tpAttrBuf;
        /** 非 0：来自首个 TP 的 get_tp_attr 或失败兜底，供 K=min(N,M) */
        uint32_t resolvedSlLevelCount{0};
    };

    using TpInfoByQosKey    = std::unordered_map<uint32_t, TpInfoCtx>;
    using TpInfoByRmt       = std::unordered_map<Hccl::IpAddress, TpInfoByQosKey>;
    using InfoCtxMap        = std::unordered_map<Hccl::IpAddress, TpInfoByRmt>;
    using RequestByQosKey   = std::unordered_map<uint32_t, RequestCtx>;
    using RequestByRmt      = std::unordered_map<Hccl::IpAddress, RequestByQosKey>;
    using ReqCtxMap         = std::unordered_map<Hccl::IpAddress, RequestByRmt>;

private:
    TpMgr() = default;
    ~TpMgr() = default;
    TpMgr(const TpMgr &that) = delete;
    TpMgr &operator=(const TpMgr &that) = delete;

    HcclResult FindAndGetTpInfo(const GetTpInfoParam &param, TpInfo &tpInfo);
    HcclResult StartGetTpInfoListRequest(const GetTpInfoParam &param, RequestCtx &reqCtx) const;
    static HcclResult StartGetTpAttrForFirstTp(uint32_t devPhyId, const GetTpInfoParam &param,
        RequestCtx &reqCtx);
    HcclResult HandleCompletedRequest(const RequestCtx reqCtx, const GetTpInfoParam &param,
        TpInfo &tpInfo);

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

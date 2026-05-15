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

using GetTpInfoParam = struct GetTpInfoParamDef {
    CommAddr locAddr{};
    CommAddr rmtAddr{};
    TpProtocol tpProtocol{TpProtocol::CTP};

    explicit GetTpInfoParamDef() = default;
    GetTpInfoParamDef(const CommAddr &locAddr, const CommAddr &rmtAddr, TpProtocol tpProtocol)
        : locAddr(locAddr), rmtAddr(rmtAddr), tpProtocol(tpProtocol){};

    std::string Describe() const {
        Hccl::IpAddress locIpAddr{}, rmtIpAddr{};
        (void)CommAddrToIpAddress(locAddr, locIpAddr);
        (void)CommAddrToIpAddress(rmtAddr, rmtIpAddr);
        return Hccl::StringFormat("RaUbGetTpInfoParam[locAddr=%s, rmtAddr=%s, tpProtocol=%s]",
            locIpAddr.Describe().c_str(), rmtIpAddr.Describe().c_str(),
            tpProtocol.Describe().c_str());
    }
};

/*
 * TP信息，当前申请TpHandle，不感知具体TP信息，当前仅支持TP与CTP
 * tpHandle: 对应管控面的TPID与相关资源，URMA通过引用计数管理申请和销毁TP
 */
using TpHandle = uint64_t;
struct TpInfo {
    TpHandle tpHandle{0};

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
    struct RequestCtx {
        RequestHandle handle{0};
        uint32_t tpInfoNum{0};
        std::vector<char> dataBuffer;
    };

    using InfoCtxMap = std::unordered_map<Hccl::IpAddress,
        std::unordered_map<Hccl::IpAddress, TpInfoCtx>>;
    using ReqCtxMap  = std::unordered_map<Hccl::IpAddress,
        std::unordered_map<Hccl::IpAddress, RequestCtx>>;

private:
    TpMgr() = default;
    ~TpMgr() = default;
    TpMgr(const TpMgr &that) = delete;
    TpMgr &operator=(const TpMgr &that) = delete;

    HcclResult FindAndGetTpInfo(const GetTpInfoParam &param, TpInfo &tpInfo);
    HcclResult StartGetTpInfoListRequest(const GetTpInfoParam &param, RequestCtx &reqCtx) const;
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

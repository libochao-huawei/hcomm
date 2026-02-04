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

#include <mutex>
#include <vector>
#include <unordered_map>

#include "hccl_types.h"
#include "ip_address.h"
#include "orion_adapter_hccp.h"

namespace Hccl {

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

class TpManager {
public:
    static TpManager &GetInstance(const int32_t deviceLogicId);
    void Init();
    HcclResult GetTpInfo(const RaUbGetTpInfoParam &param, TpInfo &tpInfo);
    // unimport jetty 会 URMA 销毁 tp 资源，hccl 配套删除记录
    HcclResult ReleaseTpInfo(const RaUbGetTpInfoParam &param, const TpInfo &tpInfo);

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

    /*
    * Request上下文，保存查询TP信息相关调用异步接口出参
    * handle: 异步接口调用handle，用于查询处理结果
    * tpInfoNum: 查询到的TP信息个数，当前为复用TP，只会申请1个
    * dataBuffer: 查询到的TP信息数据，原始数据保留缓冲区
    */
    struct RequestCtx {
        RequestHandle handle{0};
        uint32_t tpInfoNum{0};
        std::vector<char_t> dataBuffer;
    };

    using InfoCtxMap = std::unordered_map<IpAddress, std::unordered_map<IpAddress, TpInfoCtx>>;
    using ReqCtxMap  = std::unordered_map<IpAddress, std::unordered_map<IpAddress, RequestCtx>>;

    InfoCtxMap ctpInfoMap;
    ReqCtxMap  ctpReqMap;

    InfoCtxMap tpInfoMap;
    ReqCtxMap  tpReqMap;

    std::mutex ctpInfoMutex;
    std::mutex ctpReqMutex;

    std::mutex tpInfoMutex;
    std::mutex tpReqMutex;

    TpManager() = default;
    ~TpManager() = default;
    TpManager(const TpManager &that) = delete;
    TpManager &operator=(const TpManager &that) = delete;

    bool FindAndGetTpInfo(const RaUbGetTpInfoParam &param, TpInfo &tpInfo);
    void StartGetTpInfoListRequest(const RaUbGetTpInfoParam &param, RequestCtx &reqCtx) const;
    HcclResult HandleCompletedRequest(const RequestCtx reqCtx, const RaUbGetTpInfoParam &param,
        TpInfo &tpInfo);

    bool CheckRequestResult(RequestHandle &reqHandle) const;
    InfoCtxMap &GetInfoCtxMap(const TpProtocol tpProtocol);
    ReqCtxMap  &GetReqCtxMap(const TpProtocol tpProtocol);
    std::mutex &GetInfoCtxMutex(const TpProtocol tpProtocol);
    std::mutex &GetReqCtxMutex(const TpProtocol tpProtocol);
};

} // namespace Hccl

#endif // HCCLV2_TP_MANAGER_H

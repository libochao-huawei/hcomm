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
    HcclResult GetTpInfo(const GetTpInfoParam &param, TpInfo &tpInfo);
    HcclResult GetTpAttr(const GetTpAttrParam &param, TpAttrInfo &tpAttrInfo, CtxHandle ctxHandle);
    // unimport jetty 会 URMA 销毁 tp 资源，hccl 配套删除记录
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

    struct TpAttrRequestCtx {
        RequestHandle handle{0};
        struct TpAttr tpAttr{0};
    };

    using TpAttrCtxMap = std::unordered_map<TpHandle, TpAttrCtx>;
    using TpAttrReqCtxMap  = std::unordered_map<TpHandle, TpAttrRequestCtx>;

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
    HcclResult FindAndGetTpAttr(const TpHandle tpHandle, TpAttrInfo &tpAttrInfo);
    HcclResult StartGetTpInfoListRequest(const GetTpInfoParam &param, RequestCtx &reqCtx) const;
    HcclResult StartGetTpAttrRequest(const GetTpAttrParam &param, TpAttrRequestCtx &reqCtx, CtxHandle ctxHandle) const;
    HcclResult HandleCompletedRequest(const RequestCtx reqCtx, const GetTpInfoParam &param,
        TpInfo &tpInfo);
    HcclResult HandleCompletedTpAttrRequest(const TpAttrRequestCtx reqCtx, const TpHandle tpHandle,
        TpAttrInfo &tpAttrInfo);

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

    TpAttrCtxMap tpAttrCtxMap_;
    TpAttrReqCtxMap tpAttrReqCtxMap_;

    std::mutex ctpInfoMutex_;
    std::mutex ctpReqMutex_;

    std::mutex rtpInfoMutex_;
    std::mutex rtpReqMutex_;

    std::mutex tpAttrCtxMutex_;
    std::mutex tpAttrReqMutex_;
};

} // namespace hcomm

#endif // TP_MGR_H

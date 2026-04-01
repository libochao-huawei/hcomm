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

using TpHandle = uint64_t;
struct TpInfo {
    TpHandle tpHandle{0};
    /** 映射路径：来自 sl_available 位图与策略的真实 SL，经 Jetty priority 下发 */
    uint8_t mappedJettyPriority{0};
    bool hasMappedJettyPriority{false};

    TpInfo() = default;
    explicit TpInfo(const TpHandle handle)
        : tpHandle(handle)
    {
    }
};

class TpManager {
public:
    static TpManager &GetInstance(const int32_t deviceLogicId);
    void Init();
    HcclResult GetTpInfo(const RaUbGetTpInfoParam &param, TpInfo &tpInfo);
    HcclResult ReleaseTpInfo(const RaUbGetTpInfoParam &param, const TpInfo &tpInfo);

private:
    bool initFlag{false};
    int32_t devLogicId{0};
    uint32_t devPhyId{0};

    struct TpInfoCtx {
        TpInfo tpInfo{};
        uint32_t useCnt{0};

        TpInfoCtx() = default;
        TpInfoCtx(const TpInfo &info, const uint32_t cnt)
            : tpInfo(info), useCnt(cnt)
        {
        }
    };

    enum class TpInfoReqPhase : uint8_t {
        kWaitList = 0,
        kWaitTpAttr = 1,
    };

    struct RequestCtx {
        RequestHandle handle{0};
        uint32_t tpInfoNum{0};
        std::vector<char_t> dataBuffer;
        TpInfoReqPhase reqPhase{TpInfoReqPhase::kWaitList};
        uint32_t tpAttrBitmap{0};
        std::vector<char_t> tpAttrBuf;
        /** sl_available popcount 得到的 M（可被 slLevelCount 收束） */
        uint32_t resolvedSlLevelCount{0};
    };

    using TpInfoByQosKey = std::unordered_map<uint32_t, TpInfoCtx>;
    using TpInfoByRmt = std::unordered_map<IpAddress, TpInfoByQosKey>;
    using InfoCtxMap = std::unordered_map<IpAddress, TpInfoByRmt>;

    using RequestByQosKey = std::unordered_map<uint32_t, RequestCtx>;
    using RequestByRmt = std::unordered_map<IpAddress, RequestByQosKey>;
    using ReqCtxMap = std::unordered_map<IpAddress, RequestByRmt>;

    InfoCtxMap ctpInfoMap;
    ReqCtxMap ctpReqMap;

    InfoCtxMap tpInfoMap;
    ReqCtxMap tpReqMap;

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

    bool FindAndGetTpInfo(const RaUbGetTpInfoParam &param, TpInfo &tpInfo);
    void StartGetTpInfoListRequest(const RaUbGetTpInfoParam &param, RequestCtx &reqCtx) const;
    HcclResult StartGetTpAttrForFirstTp(const RaUbGetTpInfoParam &param, RequestCtx &reqCtx);
    HcclResult HandleCompletedRequest(RequestCtx reqCtx, const RaUbGetTpInfoParam &param, TpInfo &tpInfo);

    bool CheckRequestResult(RequestHandle &reqHandle) const;
    InfoCtxMap &GetInfoCtxMap(const TpProtocol tpProtocol);
    ReqCtxMap &GetReqCtxMap(const TpProtocol tpProtocol);
    std::mutex &GetInfoCtxMutex(const TpProtocol tpProtocol);
    std::mutex &GetReqCtxMutex(const TpProtocol tpProtocol);
};

} // namespace Hccl

#endif // HCCLV2_TP_MANAGER_H

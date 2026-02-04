/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef HCCL_CCU_JETTY_H
#define HCCL_CCU_JETTY_H

#include "ip_address.h"
#include "ccu_dev_mgr.h"
#include "orion_adapter_hccp.h"
#include "rdma_handle_manager.h"

namespace Hccl {

class CcuJetty final {
public:
    CcuJetty(const IpAddress &ipAddr, const CcuJettyInfo &jettyInfo)
        : ipAddr_(ipAddr), jettyInfo_(jettyInfo){
        HCCL_DEBUG("CcuJetty: constructed with ipAdde [%s] and jettyInfo [%s]",
                ipAddr_.GetIpStr().c_str(), jettyInfo_.ToString().c_str());
    }
    ~CcuJetty();
    CcuJetty(const CcuJetty &that) = delete;
    CcuJetty &operator=(const CcuJetty &that) = delete;
    CcuJetty(CcuJetty &&that) = delete;
    CcuJetty &operator=(CcuJetty &&that) = delete;

    HcclResult CreateJetty();

    HrtRaUbCreateJettyParam GetCreateJettyParam() const;
    HrtRaUbJettyCreatedOutParam GetJettyedOutParam() const;
      
    JettyHandle GetJettyHandle() const
    {
        return reinterpret_cast<JettyHandle>(jettyHandlePtr_);
    }
    RdmaHandle GetRdmaHandle() const
    {
        return rdmaHandle_;
    }
    uint16_t GetJettyId() const
    {
        return jettyInfo_.taJettyId;
    }
    void Clean();

    static HcclResult Create(const IpAddress &ipAddr, const CcuJettyInfo &jettyInfo,
                            std::unique_ptr<CcuJetty> &ccuJetty){
                                ccuJetty = std::make_unique<CcuJetty> (ipAddr, jettyInfo);
                                TRY_CATCH_RETURN(ccuJetty->Initialize);
                                return HcclResult::HCCL_SUCCESS;
                            }
private:
    void Initialize(){
        devLogicId_ = HrtGetDevice();
        uint32_t devPhyId = HrtGetDevicePhyIdByIndex(devLogicId_);
        auto &rdmaHandleMgr = RdmaHandleManager::GetInstance();
        rdmaHandle_ = rdmaHandleMgr.GetByIp(devPhyId, ipAddr);
        const auto jfcHandle = rdmaHandleMgr.GetJfcHandle(rdmaHandle_, HrtUbJfcMode::CCU_POLL);
        const auto &tokenInfo = rdmaHandleMgr.GetTokenIdInfo(rdmaHandle_);
        const auto tokenIdHandle = tokenInfo.first;
        const auto tokenValue = GetUbToken();
        const auto jettyMode = HrtJettyMode::CCU_CCUM_CACHE; // 当前仅支持该模式
        inParam_ = HrtRaUbCreateJettyParam{jfcHandle, jfcHandle, tokenValue,
        tokenIdHandle, jettyMode, jettyInfo.taJettyId, jettyInfo.sqBufVa,
        jettyInfo.sqBufSize, jettyInfo.wqeBBStartId, jettyInfo.sqDepth};
    }
    
    int32_t devLogicId_{0};
    IpAddress ipAddr_{};
    CcuJettyInfo jettyInfo_{};

    RdmaHandle rdmaHandle_{nullptr};
    bool isCreated_{false};
    bool isError_{false};

    RequestHandle reqHandle_{0};
    std::vector<char> reqDataBuffer_;
    void *jettyHandlePtr_{nullptr};

    HrtRaUbCreateJettyParam inParam_{};
    HrtRaUbJettyCreatedOutParam outParam_{};

    HcclResult HandleAsyncRequest();
};

HcclResult CcuCreateJetty(const IpAddress &ipAddr, const CcuJettyInfo &jettyInfo,
    std::unique_ptr<CcuJetty> &ccuJetty);

} // namespace Hccl
#endif // HCCL_CCU_JETTY_H
/**
* Copyright (c) 2026 Huawei Technologies Co., Ltd.
* This program is free software, you can redistribute it and/or modify it under the terms and conditions of
* CANN Open Software License Agreement Version 2.0 (the "License").
* Please refer to the License for details. You may not use this file except in compliance with the License.
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
* INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
* See LICENSE in the root of the software repository for the full text of the License.
*/
#ifndef HOST_CPU_UBOE_CONNECTION_H
#define HOST_CPU_UBOE_CONNECTION_H

#include "hccp_common.h"
#include "enum_factory.h"
#include "hccl_common.h"

// Orion
#include "../../../../../../legacy/unified_platform/resource/socket/socket.h"
#include "local_ub_rma_buffer.h"
#include "exchange_ub_conn_dto.h"
#include "orion_adapter_hccp.h"

namespace hcomm {

class HostCpuUboeConnection {
public:

    MAKE_ENUM(UrmaConnStatus, CLOSED, INIT, JETTY_CREATED, READY, CONN_INVALID)
    HostCpuUboeConnection(Hccl::Socket *socket, RdmaHandle rdmaHandle);

    HcclResult Init();
    HcclResult CreateJetty();
    HcclResult GetExchangeDto(std::unique_ptr<Hccl::Serializable> &serial);
    void ParseRmtExchangeDto(const Hccl::ExchangeUbConnDto &rmtDto); // 解析收到的远端序列化数据
    HcclResult ImportJetty(const Hccl::ExchangeUbConnDto &rmtDto);

    ~HostCpuUboeConnection();

    std::string Describe() const;
    u64 GetJettyVa();
    u64 GetTJettyVa();

protected:
    Hccl::TpProtocol     tpProtocol_{Hccl::TpProtocol::INVALID};

private:
    Hccl::Socket        *socket_{nullptr};
    Hccl::RdmaHandle    rdmaHandle_{nullptr};

    u8                  remoteQpKey_[Hccl::HRT_UB_QP_KEY_MAX_LEN] = {0};
    u32                 sqDepth_{0};
    u32                 keySize_{0};
    u32                 remoteTokenValue_{0};
    u32                 tokenValue_{Hccl::GetUbToken()};

    u64                 jfcHandle_{0};
    u64                 jettyHandle_{0};
    u64                 jettyVa_{0};
    u64                 remoteJettyVa_{0};
    u64                 remoteJettyHandle_{0};
    Hccl::HrtUbJfcMode  jfcMode_{Hccl::HrtUbJfcMode::NORMAL};

    Hccl::JettyImportCfg jettyImportCfg_{};
    Hccl::HrtRaUbJettyCreatedOutParam repJetty_{};
    Hccl::HrtRaUbJettyImportedOutParam remOutParam_{};

    Hccl::CqCreateInfo cqInfo_{0};
    UrmaConnStatus urmaConnStatus_{UrmaConnStatus::CLOSED};

    void ThrowAbnormalStatus(std::string funcName);
    void ReleaseResource();
};

} // namespace hcomm

#endif // HOST_CPU_UBOE_CONNECTION_H

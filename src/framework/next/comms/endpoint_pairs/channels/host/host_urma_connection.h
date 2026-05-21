/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef HOST_URMA_CONNECTION_H
#define HOST_URMA_CONNECTION_H

#include "hccp_common.h"
#include "enum_factory.h"
#include "hccl_common.h"
#include "exchange_rdma_conn_dto.h"

// Orion
#include "../../../../../../legacy/unified_platform/resource/socket/socket.h"
#include "orion_adapter_hccp.h"

namespace hcomm {

class HostUrmaConnection {
public:
    struct JettyAttrDto {

    };
    struct UboeAttr {

    };
    MAKE_ENUM(RdmaConnStatus, CLOSED, INIT, QP_CREATED, QP_MODIFIED, SOCKET_TIMEOUT)
    HostUrmaConnection(Hccl::Socket *socket, RdmaHandle rdmaHandle);

    HcclResult Init();
    HcclResult CreateJetty();
    // HcclResult GetLocQpAttr(std::unique_ptr<Hccl::Serializable> &locQpAttrserial);
    // HcclResult ParseRmtQpAttr(const Hccl::Serializable &rmtQpAttrSerial);
    HcclResult GetExchangeDto(std::unique_ptr<Hccl::Serializable> &serial);
    HcclResult ParseRmtExchangeDto(const Hccl::Serializable &rmtDto); // 解析收到的远端序列化数据
    HcclResult ImportJetty(const Hccl::Serializable &rmtDto);
    RdmaConnStatus GetRdmaStatus();

    ~HostUrmaConnection();

    std::string Describe() const ;


private:
    Hccl::Socket        *socket_{nullptr};
};

} // namespace hcomm

#endif // HOST_URMA_CONNECTION_H

/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef IRDMA_CONNECTION_H
#define IRDMA_CONNECTION_H

#include "hccp.h"
#include "orion_adapter_hccp.h"
#include "../endpoint_pairs/channels/host/exchange_rdma_conn_dto.h"
#include "hccl/hccl.h"

namespace hcomm {

class IRdmaConnection {
public:
    virtual ~IRdmaConnection() = default;
    
    virtual HcclResult GetExchangeDto(std::unique_ptr<Hccl::Serializable> &serial) = 0;
    
protected:
    static HcclResult BuildExchangeDto(RdmaHandle rdmaHandle, QpHandle qpHandle,
        std::unique_ptr<Hccl::Serializable> &serial);
};

} // namespace hcomm

#endif // IRDMA_CONNECTION_H

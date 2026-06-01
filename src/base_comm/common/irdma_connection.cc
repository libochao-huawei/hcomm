/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "irdma_connection.h"
#include "log.h"
#include "../endpoint_pairs/channels/host/exchange_rdma_conn_dto.h"

namespace hcomm {

HcclResult IRdmaConnection::BuildExchangeDto(Hccl::RdmaHandle rdmaHandle, Hccl::QpHandle qpHandle,
    std::unique_ptr<Hccl::Serializable> &serial)
{
    struct QpAttr localQpAttr;
    s32 ret = RaGetQpAttr(qpHandle, &localQpAttr);
    if (ret != 0) {
        HCCL_ERROR("[IRdmaConnection::BuildExchangeDto]RaGetQpAttr failed, ret[%d]", ret);
        return HCCL_E_ROCE_CONNECT;
    }
    std::unique_ptr<hcomm::ExchangeRdmaConnDto> dto = nullptr;
    EXCEPTION_CATCH(
        dto = std::make_unique<hcomm::ExchangeRdmaConnDto>(localQpAttr.qpn, localQpAttr.psn, localQpAttr.gidIdx),
        return HCCL_E_PTR
    );
    CHK_SAFETY_FUNC_RET(memcpy_s(dto->gid_, HCCP_GID_RAW_LEN, localQpAttr.gid, HCCP_GID_RAW_LEN));
    serial = std::unique_ptr<Hccl::Serializable>(std::move(dto));
    return HCCL_SUCCESS;
}

} // namespace hcomm

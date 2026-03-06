/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "endpoint_pair_mgr.h"

namespace hcomm {

HcclResult EndpointPairMgr::Get(CommEngine engine, const EndpointDescPair &endpointDescPair, EndpointPair*& out)
{
    if (endpointPairMap_.find(engine) != endpointPairMap_.end() &&
        endpointPairMap_[engine].find(endpointDescPair) != endpointPairMap_[engine].end()) {
        out = endpointPairMap_[engine][endpointDescPair].get();
        return HCCL_SUCCESS;
    }
 
    std::unique_ptr<EndpointPair> endpointPair = nullptr;
    EXECEPTION_CATCH(
        (endpointPair = std::make_unique<EndpointPair>(endpointDescPair.first, endpointDescPair.second)), 
        return HCCL_E_PTR
    );
    CHK_SMART_PTR_NULL(endpointPair);
    CHK_RET(endpointPair->Init());
 
    out = endpointPair.get();
    endpointPairMap_[engine].emplace(endpointDescPair, std::move(endpointPair));

    return HCCL_SUCCESS;
}

} // namespace hcomm
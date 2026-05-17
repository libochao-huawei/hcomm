/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for the details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef ENDPOINT_MAP_H
#define ENDPOINT_MAP_H

#include "hcomm_res_defs.h"
#include "../endpoints/endpoint.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

namespace hcomm{

class HcommEndpointMap{
public:
    HcclResult AddEndpoint(EndpointHandle handle, std::unique_ptr<Endpoint> endpoint);

    HcclResult RemoveEndpoint(EndpointHandle handle);

    HcclResult UpdateEndpoint(EndpointHandle handle, std::unique_ptr<Endpoint> newEndpoint);

    HcclResult GetEndpoint(EndpointHandle handle, Endpoint*& outEndpoint);
};
}



#ifdef __cplusplus
}
#endif // __cplusplus


#endif

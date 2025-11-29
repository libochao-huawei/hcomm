/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "endpoint_pair.h"

namespace hcomm {
EndPointPair::EndPointPair(EndPointHandle srcEndPoint, EndPointInfo dstEndPoint)
    : srcEndPoint_(srcEndPoint), dstEndPoint_(dstEndPoint) {
}

EndPointPair::~EndPointPair()
{
    channels_.clear();
}

HcclResult EndPointPair::CreateChannel(CommEngineType engineType, const std::vector<MemRegion>& memRegions,
    ChannelHandle* channel) {
    return HCCL_SUCCESS;
}

HcclResult EndPointPair::GetRemoteMemoryInfo(std::vector<MemRegion>& remoteMemories) {
    remoteMemories = remoteMemories_;
    return HCCL_SUCCESS;
}
}
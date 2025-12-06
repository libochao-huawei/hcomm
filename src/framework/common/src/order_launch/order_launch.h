/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef HCCL_ORDER_LAUNCH_H
#define HCCL_ORDER_LAUNCH_H

#include <unordered_set>
#include <map>
#include "hccl/hccl_types.h"
#include "stream_pub.h"
#include "hccl_common.h"
#include "local_notify.h"

namespace hccl {
class OrderLaunch {
public:
    static OrderLaunch& GetInstance(s32 deviceLogicID);
    HcclResult RegisterOrderLaunch(const std::string &group);
    HcclResult UnRegisterOrderLaunch(const std::string &group);

    HcclResult AclgraphLaunchInOrder(std::string &group, const Stream& kernelStream, u64 modelId, rtModel_t rtModel,
        std::shared_ptr<LocalNotify> notify0, std::shared_ptr<LocalNotify> notify1, u32 timeOut);
    HcclResult OpbaseLaunchInOrder(std::string &group, const Stream& kernelStream,
        std::shared_ptr<LocalNotify> notify0, std::shared_ptr<LocalNotify> notify1, u32 timeOut);
    HcclResult HcomLaunchInOrder(std::string &group, const Stream& kernelStream, u32 graphId,
        std::shared_ptr<LocalNotify> notify0, std::shared_ptr<LocalNotify> notify1, u32 timeOut);
    HcclResult SetHcomStream(u32 graphId, const Stream& hcomAttachedStream);

private:
    OrderLaunch();
    ~OrderLaunch();

    std::mutex streamMutex_;
    std::unordered_set<std::string> groupSet_; // 记录已经初始化过的group
    std::unique_ptr<Stream> opbaseStream_;
    std::unordered_map<u64, Stream> aclgraphStreamMap_;
    std::unordered_map<u32, Stream> hcomStreamMap_;
    bool initialized_ = false;
    HcclResult LaunchInOrder(std::string &group, const Stream &kernelStream, const Stream &hostOrderStream,
        std::shared_ptr<LocalNotify> notify0, std::shared_ptr<LocalNotify> notify1, u32 timeOut);
};
}
#endif
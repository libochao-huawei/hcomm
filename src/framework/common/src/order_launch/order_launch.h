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
#include "adapter_rts.h"

namespace hccl {
struct OrderLaunchResMgr
{
    std::unordered_set<std::string> usedGroup; // 使用当前资源的group
    Stream opbaseStream; // 单算子模式使用
    Stream aclgraphStream; // aclgraph模式使用

    OrderLaunchResMgr() : usedGroup(), opbaseStream(nullptr), aclgraphStream(nullptr) {}
};

class OrderLaunch {
public:
    static OrderLaunch& GetInstance(s32 deviceLogicID);
    HcclResult RegisterOrderLaunch(const std::string &group);
    HcclResult UnRegisterOrderLaunch(const std::string &group);

    HcclResult AclgraphLaunchInOrderToOrderStream(std::string &group, const Stream& kernelStream,
        std::shared_ptr<LocalNotify> notify0, std::shared_ptr<LocalNotify> notify1, u32 timeOut, HcclRtEvent event);
    HcclResult AclgraphLaunchInOrderToKernelStream(std::string &group, const Stream& kernelStream, HcclRtEvent event);

    HcclResult OpbaseLaunchInOrder(std::string &group, const Stream& kernelStream,
        std::shared_ptr<LocalNotify> notify0, std::shared_ptr<LocalNotify> notify1, u32 timeOut);

    HcclResult HcomLaunchInOrder(std::string &group, const Stream& kernelStream, u32 graphId,
        std::shared_ptr<LocalNotify> notify0, std::shared_ptr<LocalNotify> notify1, u32 timeOut);

    HcclResult SetHcomStream(u32 graphId, const Stream& hcomAttachedStream);

private:
    OrderLaunch();
    ~OrderLaunch();

    HcclResult LaunchInOrder(std::string &group, const Stream &kernelStream, const Stream &hostOrderStream,
        std::shared_ptr<LocalNotify> notify0, std::shared_ptr<LocalNotify> notify1, u32 timeOut);
    void DestoryRes();
    HcclResult InitGroupCtx(const std::string &group);

    std::mutex streamMutex_;
    std::unordered_map<std::string, u64> groupCtxMap_; // 记录已经初始化过的group, 并在其申请流时记录aclrtContext
    std::unordered_map<u64, OrderLaunchResMgr> orderLaunchCtxResMgrMap_; // key:aclrtContext, value:OrderLaunchResMgr, 管理申请的流资源

    std::unordered_map<u32, Stream> hcomStreamMap_; // 图模式使用
    bool initialized_ = false;
};
}
#endif
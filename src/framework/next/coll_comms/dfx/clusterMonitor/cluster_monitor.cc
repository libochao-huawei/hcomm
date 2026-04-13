/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "cluster_monitor.h"

namespace hccl {

ClusterMonitor &ClusterMonitor::GetInstance()
{
    static ClusterMonitor clusterMonitor[MAX_MODULE_DEVICE_NUMS];
    // TODO 获取deviceLogicId
    return clusterMonitor[deviceLogicId];
}

void ClusterMonitor::Register(CollComm *comm)
{
    std::lock_guard<std::mutex> lock(vecMutex);
    // 从comm中拿到所有需要的东西
}

void ClusterMonitor::UnRegister(CollComm *comm)
{
    std::lock_guard<std::mutex> lock(vecMutex);
    // 从comm中解注册所有的私有成员变量
}

HcclResult ClusterMonitor::RegisterToClusterMonitor()
{
    // 主流程
}
}// namespace hccl
#endif // CLUSTER_MONITOR_H
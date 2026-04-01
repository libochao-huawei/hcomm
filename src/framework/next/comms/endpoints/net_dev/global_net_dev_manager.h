/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef GLOBAL_NET_DEV_MANAGER_H
#define GLOBAL_NET_DEV_MANAGER_H

#include <set>
#include <map>
#include <mutex>
#include <memory>
#include <unordered_set>

#include <hccl/hccl_comm.h>
#include <hccl/hccl_inner.h>
#include "hccl_network_pub.h"
#include "hccl_socket_manager.h"
#include "hccl_net_dev.h"

namespace hccl {

// 进程粒度的endpoint的netdev管理单例
class GlobalNetDevMgr {
public:
    static GlobalNetDevMgr& GetInstance(u32 devicePhyId); // 获取单例
    ~GlobalNetDevMgr();
    HcclResult RefNetDevCtx(NicType nicType, const HcclIpAddress& ipAddr, u32 port, HcclNetDevCtx& netDevCtx);
    HcclResult UnRefNetDevCtx(NicType nicType, const HcclIpAddress& ipAddr, u32 port);
    HcclResult ServerInit(const HcclNetDevCtx netDevCtx, u32 port);
    HcclResult ServerDeInit(const HcclNetDevCtx netDevCtx, u32 port);
    HcclResult GetDeviceIP(u32 devicePhyId, std::vector<hccl::HcclIpAddress> &ipAddr);
    std::shared_ptr<HcclSocketManager> GetSocketManager() {return socketManager_;};

private:
    HcclResult Init(u32 devicePhyId, u32 deviceLogicID);
    HcclResult DeInit();

private:
    u32 devicePhyId_{INVALID_UINT};
    s32 deviceLogicId_{INVALID_INT};

    bool isInited_{false};
    std::shared_ptr<HcclSocketManager> socketManager_;

    static std::map<PortInfo, std::pair<NicType, HcclNetDevCtx>> netDevCtxMap_;
    static std::map<PortInfo, Referenced> netDevCtxRefMap_;
    static std::mutex netDevCtxMtx_;
};

} // namespace hccl
#endif // GLOBAL_NET_DEV_MANAGER_H
/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef SOCKET_MGR_H
#define SOCKET_MGR_H

#include <mutex>

#include "hccl/hccl_res.h"
#include "../../../../../legacy/unified_platform/resource/socket/socket.h"
#include "virtual_topo.h"
#include "socket_config.h"
#include "orion_adapter_rts.h"

namespace hcomm {

class SocketMgr {
public:
    SocketMgr() {};
    ~SocketMgr() {};

    HcclResult GetSocket(const Hccl::SocketConfig &socketConfig, Hccl::Socket*& socket);

private:
    HcclResult Init();
    HcclResult GetSocketHandle(const Hccl::SocketConfig &socketConfig, Hccl::SocketHandle &socketHandle);
    HcclResult AddWhiteList(const Hccl::SocketConfig &socketConfig, const Hccl::SocketHandle &socketHandle);
    HcclResult CreateSocket(const Hccl::SocketConfig &socketConfig, const Hccl::SocketHandle &socketHandle);

private:
    bool isLoaded_{false};
    uint32_t devicePhyId_{};
    uint32_t serverListenPort_{};
    std::unordered_map<Hccl::SocketConfig, std::unique_ptr<Hccl::Socket>> socketMap_{};
    std::mutex mutex_{};
};

} // namespace hcomm

#endif // SOCKET_MGR_H

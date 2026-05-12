/**
* Copyright (c) 2026 Huawei Technologies Co., Ltd.
* This program is free software, you can redistribute it and/or modify it under the terms and conditions of
* CANN Open Software License Agreement Version 2.0 (the "License").
* Please refer to the License for details. You may not use this file except in compliance with the License.
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
* INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
* See LICENSE in the root of the software repository for the full text of the License.
*/
#ifndef HOST_CPU_URMA_CHANNEL_H
#define HOST_CPU_URMA_CHANNEL_H

#include "host_cpu_channel.h"
#include <mutex>
#include "urma_types.h"

// Orion
#include "../../../../../../legacy/unified_platform/pub_inc/buffer_key.h"
#include "rma_connection.h"
#include "ub_mem_transport.h"
#include "host_ub_connection.h"
#include "ub_local_notify.h"
#include "aicpu_res_package_helper.h"

namespace hcomm {

constexpr uint64_t MAX_JETTY_WR_DATA_LEN = 256 * 1024 * 1024;  // 256MB

class HostCpuUrmaChannel : public HostCpuChannel {
public:
    HostCpuUrmaChannel(EndpointHandle endpointHandle, const HcommChannelDesc &channelDesc);

    HcclResult Init() override;
    HcclResult GetRemoteMem(HcclMem **remoteMem, uint32_t *memNum, char **memTags) override;
    ChannelStatus GetStatus() override;

    // 数据面接口
    HcclResult NotifyRecord(const uint32_t remoteNotifyIdx) override;
    HcclResult NotifyWait(const uint32_t localNotifyIdx, const uint32_t timeout) override;
    HcclResult WriteWithNotify(void *dst, const void *src, const uint64_t len, uint32_t remoteNotifyIdx) override;
    HcclResult Write(void *dst, const void *src, uint64_t len) override;
    HcclResult Read(void *dst, const void *src, uint64_t len) override;
    HcclResult ChannelFence() override;

    virtual HcclResult Clean() override;
    virtual HcclResult Resume() override;

private:
    HcclResult ParseInputParam();
    Hccl::SocketConfig MakeSocketConfig(Hccl::LinkData &linkData, uint16_t port) override;
    HcclResult BuildConnection();
    HcclResult BuildBuffer();
    HcclResult BuildUbMemTransport();
    HcclResult GetLocSeg(const void *addr, const size_t size, u64 *seg);
    HcclResult UrmaPostJettySendWr(urma_opcode_t opcode, void *dst, const void *src, uint64_t len);
    HcclResult GetSplitNum(uint64_t len, uint64_t &splitNum);
    HcclResult GetLocalAndRemoteSeg(urma_opcode_t opcode, void *dst, const void *src, uint64_t len, u64 &localSeg, u64 &remoteSeg);

private:
    // --------------------- 入参 (endpointHandle_, channelDesc_ 继承自 HostCpuChannel) ---------------------

    // --------------------- 转换参数 (localEp_, remoteEp_, socket_, rdmaHandle_ 继承自 HostCpuChannel) ---------------------
    std::vector<std::shared_ptr<Hccl::Buffer>>                  bufs_{};

    // --------------------- 具体成员 ---------------------
    std::unique_ptr<Hccl::UbMemTransport>                       memTransport_{nullptr};
    Hccl::BaseMemTransport::Attribution                         attr_{};
    Hccl::BaseMemTransport::CommonLocRes                        commonRes_{};
    std::vector<std::unique_ptr<Hccl::HostUbConnection>>        connections_{};
    std::vector<std::unique_ptr<Hccl::LocalUbRmaBuffer>>        localRmaBuffers_{};
    std::unique_ptr<Hccl::Socket>                               serverSocket_{nullptr};

    urma_jfc_t jfc_{};
    urma_jetty_t jetty_{};
    urma_target_jetty_t tjetty_{};

    std::mutex jfcMutex_;
    std::mutex fenceMutex_;
    bool Onetime_{false};
};

} // namespace hcomm

#endif // HOST_CPU_URMA_CHANNEL_H
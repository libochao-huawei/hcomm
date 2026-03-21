/**
* Copyright (c) 2025 Huawei Technologies Co., Ltd.
* This program is free software, you can redistribute it and/or modify it under the terms and conditions of
* CANN Open Software License Agreement Version 2.0 (the "License").
* Please refer to the License for details. You may not use this file except in compliance with the License.
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
* INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
* See LICENSE in the root of the software repository for the full text of the License.
*/
#ifndef HOST_CPU_URMA_CHANNEL_H
#define HOST_CPU_URMA_CHANNEL_H

#include "../channel.h"
#include <mutex>
#include "urma_types.h"

// Orion
#include "../../../../../../legacy/unified_platform/resource/socket/socket.h"
#include "../../../../../../legacy/unified_platform/pub_inc/buffer_key.h"
#include "rma_connection.h"
#include "ub_mem_transport.h"
#include "host_ub_connection.h"
#include "ub_local_notify.h"
#include "aicpu_res_package_helper.h"

namespace hcomm {

class HostCpuUrmaChannel : public Channel {
public:
    HostCpuUrmaChannel(EndpointHandle endpointHandle, const HcommChannelDesc &channelDesc);

    HcclResult Init() override;
    HcclResult GetNotifyNum(uint32_t *notifyNum) const override;
    HcclResult GetRemoteMem(HcclMem **remoteMem, uint32_t *memNum, char **memTags) override;
    ChannelStatus GetStatus() override;

    // 数据面接口
    HcclResult NotifyRecord(const uint32_t remoteNotifyIdx) override;
    HcclResult NotifyWait(const uint32_t localNotifyIdx, const uint32_t timeout) override;
    HcclResult WriteWithNotify(void *dst, const void *src, const uint64_t len, uint32_t remoteNotifyIdx) override;
    HcclResult Write(void *dst, const void *src, uint64_t len) override;
    HcclResult Read(void *dst, const void *src, uint64_t len) override;
    HcclResult ChannelFence() override;

private:
    HcclResult ParseInputParam();
    HcclResult BuildAttr();
    HcclResult BuildConnection();
    HcclResult BuildNotify();
    HcclResult BuildBuffer();
    HcclResult BuildUbMemTransport();

private:
    // --------------------- 入参 ---------------------
    EndpointHandle                                              endpointHandle_;
    HcommChannelDesc                                            channelDesc_;

    // TODO: 成员变量全部初始化
    // --------------------- 转换参数 ---------------------
    EndpointDesc                                                localEp_{};
    EndpointDesc                                                remoteEp_{};
    uint32_t                                                    notifyNum_{0};
    std::vector<std::shared_ptr<Hccl::Buffer>>                  bufs_{};

    // --------------------- 具体成员 ---------------------
    Hccl::Socket*                                               socket_{nullptr};
    RdmaHandle                                                  rdmaHandle_{nullptr};
    std::unique_ptr<Hccl::UbMemTransport>                       memTransport_{nullptr};
    Hccl::BaseMemTransport::Attribution                         attr_{};
    Hccl::BaseMemTransport::CommonLocRes                        commonRes_{};
    std::vector<std::unique_ptr<Hccl::HostUbConnection>>        connections_{};
    std::vector<std::unique_ptr<Hccl::LocalUbRmaBuffer>>        localRmaBuffers_{};
    std::vector<std::unique_ptr<Hccl::UbLocalNotify>>           localNotifies_{};

    HcclResult UrmaPostJfr();
    HcclResult PrepareNotifyWrResource(const uint32_t remoteNotifyIdx, urma_jfs_wr_t &notifyRecordWr);
    HcclResult PrepareWriteWrResource(const void *dst, const void *src, const uint64_t len, const uint32_t remoteNotifyIdx,
                                    urma_jfs_wr_t &writeWithNotifyWr);

    urma_jfc_t jfc_;
    urma_jetty_t jetty_;
    urma_target_jetty_t tjetty_;
    uint32_t wqeNum_{0};
    bool fenceFlag_{false};

    std::mutex jfcMutex_;
    std::mutex fenceMutex_;
};

} // namespace hcomm

#endif // HOST_CPU_URMA_CHANNEL_H
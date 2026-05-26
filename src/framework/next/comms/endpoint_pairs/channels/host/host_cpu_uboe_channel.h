/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef HOST_CPU_UBOE_CHANNEL_H
#define HOST_CPU_UBOE_CHANNEL_H

#include <mutex>

#include "../channel.h"
#include "enum_factory.h"
#include "hccl_common.h"
#include "../../sockets/socket_mgr.h"

#include "host_urma_connection.h"
#include "local_ub_rma_buffer.h"
#include "remote_rma_buffer.h"


namespace hcomm {

class HostCpuUboeChannel final : public Channel {
public:
    
    MAKE_ENUM(RdmaStatus, INIT, SOCKET_OK, JETTY_CREATED,  DATA_EXCHANGE, JETTY_IMPORTED, CONN_OK)

    HostCpuUboeChannel(EndpointHandle endpointHandle, HcommChannelDesc channelDesc);
    ~HostCpuUboeChannel();

    HcclResult Init() override;
    HcclResult GetNotifyNum(uint32_t *notifyNum) const override;
    HcclResult GetRemoteMem(HcclMem **remoteMem, uint32_t *memNum, char** memTags) override;
    ChannelStatus GetStatus() override;
    HcclResult GetStatus(ChannelStatus &status);

    std::string Describe() const;

    // 数据面接口
    HcclResult NotifyRecord(const uint32_t remoteNotifyIdx) override;
    HcclResult NotifyWait(const uint32_t localNotifyIdx, const uint32_t timeout) override;
    HcclResult WriteWithNotify(void *dst, const void *src, const uint64_t len, uint32_t remoteNotifyIdx) override;
    HcclResult Write(void *dst, const void *src, uint64_t len) override;
    HcclResult Read(void *dst, const void *src, uint64_t len) override;
    HcclResult ChannelFence() override;

    HcclResult Clean() override 
    {
        return HCCL_SUCCESS;
    }
    HcclResult Resume() override
    {
        return HCCL_SUCCESS;
    }

private:
    HcclResult ParseInputParam();
    HcclResult StartListen();
    HcclResult BuildSocket();
    HcclResult BuildConnection();
    HcclResult BuildBuffer();

    HcclResult CheckSocketStatus();
    HcclResult CreateJetty();
    HcclResult ExchangeData();
    HcclResult ImportJetty();

    HcclResult BufferVecPack(Hccl::BinaryStream &binaryStream);
    HcclResult ConnVecPack(Hccl::BinaryStream &binaryStream);

    HcclResult RmtBufferVecUnpackProc(Hccl::BinaryStream &binaryStream);
    HcclResult ConnVecUnpackProc(Hccl::BinaryStream &binaryStream);

    HcclResult FindLocalBuffer(const uint64_t addr, const uint64_t len, size_t &targetIdx) const;
    HcclResult FindRemoteBuffer(const uint64_t addr, const uint64_t len, size_t &targetIdx) const;

    // 入参
    EndpointHandle endpointHandle_;
    HcommChannelDesc channelDesc_;

    // 状态
    ChannelStatus channelStatus_{ChannelStatus::INIT};
    RdmaStatus rdmaStatus_{RdmaStatus::INIT};
    
    // 转换参数
    EndpointDesc localEpDesc_;
    EndpointDesc remoteEpDesc_;
    Hccl::Socket *socket_{nullptr};
    const Hccl::SocketConfig *socketConfig_{nullptr};

    struct PortAggregationCtx
    {
        bool fenceFlag{false};
        uint32_t wqeNum{0};

        RdmaHandle rdmaHandle_{};
        std::unique_ptr<HostUrmaConnection> connection_;
        ExchangeRdmaConnDto rmtConnDto_;
    };
    
    std::vector<PortAggregationCtx> portCtxs_;
    std::vector<std::shared_ptr<Hccl::LocalUbAggregatedRmaBuffer>> localRmaBuffers_;
    std::vector<std::unique_ptr<Hccl::RemoteUbAggregatedRmaBuffer>> remoteRmaBuffers_;
    uint32_t devicePhyId_{0};
};

} // namespace hcomm

#endif // HOST_CPU_UBOE_CHANNEL_H

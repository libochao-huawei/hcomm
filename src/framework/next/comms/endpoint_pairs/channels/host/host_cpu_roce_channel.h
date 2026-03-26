/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef HOST_CPU_ROCE_CHANNEL_H
#define HOST_CPU_ROCE_CHANNEL_H

#include <mutex>

#include "../channel.h"
#include "enum_factory.h"
#include "hccl_common.h"
#include "../../sockets/socket_mgr.h"

// Orion
#include "../../../../../../legacy/unified_platform/resource/socket/socket.h"
#include "../../../../../../legacy/unified_platform/resource/buffer/local_rdma_rma_buffer.h"
#include "remote_rma_buffer.h"
#include "host_rdma_connection.h"

namespace hcomm {

class HostCpuRoceChannel final : public Channel {
public:
    MAKE_ENUM(RdmaStatus, INIT, SOCKET_OK, QP_CREATED,  DATA_EXCHANGE, QP_MODIFIED, CONN_OK)

    HostCpuRoceChannel(EndpointHandle endpointHandle, HcommChannelDesc channelDesc);
    ~HostCpuRoceChannel();

    HcclResult Init() override;
    HcclResult GetNotifyNum(uint32_t *notifyNum) const override;
    HcclResult GetRemoteMem(HcclMem **remoteMem, uint32_t *memNum, char** memTags) override;
    ChannelStatus GetStatus() override;
    HcclResult GetStatus(ChannelStatus &status);

    std::string Describe() const;

    // 数据面调用verbs接口
    HcclResult NotifyRecord(const uint32_t remoteNotifyIdx);
    HcclResult NotifyWait(const uint32_t localNotifyIdx, const uint32_t timeout);
    HcclResult WriteWithNotify(void *dst, const void *src, const uint64_t len, uint32_t remoteNotifyIdx);
    HcclResult Write(void *dst, const void *src, uint64_t len);
    HcclResult Read(void *dst, const void *src, uint64_t len);
    HcclResult ChannelFence();
    HcclResult GetHcclBuffer(void*& addr, uint64_t& size);

private:
    HcclResult ParseInputParam();
    HcclResult StartListen();
    HcclResult BuildSocket();
    HcclResult BuildConnection();
    HcclResult BuildNotify();
    HcclResult BuildBuffer();

    HcclResult CheckSocketStatus();
    HcclResult CreateQp();
    HcclResult ExchangeData();
    HcclResult ModifyQp();

    void NotifyVecPack(Hccl::BinaryStream &binaryStream);
    HcclResult BufferVecPack(Hccl::BinaryStream &binaryStream);
    HcclResult ConnVecPack(Hccl::BinaryStream &binaryStream);
    // void HandshakeMsgPack(Hccl::BinaryStream &binaryStream);

    // HcclResult HandshakeMsgUnpack(Hccl::BinaryStream &binaryStream);
    HcclResult NotifyVecUnpack(Hccl::BinaryStream &binaryStream);
    HcclResult RmtBufferVecUnpackProc(Hccl::BinaryStream &binaryStream);
    HcclResult ConnVecUnpackProc(Hccl::BinaryStream &binaryStream);

    std::vector<Hccl::QpInfo> GetQpInfos() const; // in Connection

    HcclResult IbvPostRecv() const;
    HcclResult PrepareNotifyWrResource(const uint64_t len, const uint32_t remoteNotifyIdx, struct ibv_send_wr &notifyRecordWr) const;
    HcclResult PrepareWriteWrResource(const void *dst, const void *src, const uint64_t len, const uint32_t remoteNotifyIdx,
                                      struct ibv_send_wr &writeWithNotifyWr) const;

    HcclResult PostRdmaOp(const char *caller, ibv_wr_opcode opcode, void *localAddr, const void *remoteAddr, uint64_t len);
    void BuildRdmaWr(const char *caller, ibv_wr_opcode opcode, void *localAddr, const void *remoteAddr, uint64_t len,
                     size_t localIdx, size_t rmtIdx, struct ibv_send_wr &wr, struct ibv_sge &sg) const;
    HcclResult PostAndCheckSend(const char *caller, struct ibv_send_wr &wr);
    HcclResult FindLocalBuffer(const uint64_t addr, const uint64_t len, size_t &targetIdx) const;
    HcclResult FindRemoteBuffer(const uint64_t addr, const uint64_t len, size_t &targetIdx) const;

    // 入参
    EndpointHandle endpointHandle_;
    HcommChannelDesc channelDesc_;

    // 转换参数
    EndpointDesc localEp_;
    EndpointDesc remoteEp_;
    uint32_t notifyNum_{0};
    Hccl::Socket *socket_{nullptr};
    RdmaHandle rdmaHandle_{nullptr};

    std::vector<std::unique_ptr<HostRdmaConnection>> connections_{};
    std::vector<Hccl::LocalRdmaRmaBuffer *> localRmaBuffers_{};
    std::vector<uint32_t> localDpuNotifyIds_{};
    uint32_t bufferNum_{0};
    uint32_t connNum_{0};
    // Hccl::BaseMemTransport::Attribution attr_;
    ChannelStatus channelStatus_{ChannelStatus::INIT};
    RdmaStatus rdmaStatus_{RdmaStatus::INIT};
    std::vector<uint32_t> remoteDpuNotifyIds_;
    std::vector<std::unique_ptr<Hccl::RemoteRdmaRmaBuffer>> rmtRmaBuffers_{};
    ExchangeRdmaConnDto rmtConnDto_;
    std::vector<std::unique_ptr<HcclMem>> remoteMems{};
    uint32_t wqeNum_{0};
    std::unique_ptr<SocketMgr> socketMgr_{nullptr};
    bool fenceFlag_{false};

    std::mutex cq_mutex;
    std::mutex sendCq_mutex;
};

} // namespace hcomm

#endif // HOST_CPU_ROCE_CHANNEL_H
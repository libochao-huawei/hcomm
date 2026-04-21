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
#include "infiniband/verbs.h"

// Orion
#include "../../../../../../legacy/unified_platform/resource/socket/socket.h"
#include "../../../../../../legacy/unified_platform/resource/buffer/local_rdma_rma_buffer.h"
#include "remote_rma_buffer.h"
#include "host_rdma_connection.h"

namespace hcomm {

class HostCpuUboeChannel final : public Channel {
public:
    MAKE_ENUM(RdmaStatus, INIT, SOCKET_OK, QP_CREATED,  DATA_EXCHANGE, QP_MODIFIED, CONN_OK)

    HostCpuUboeChannel(EndpointHandle endpointHandle, HcommChannelDesc channelDesc);
    ~HostCpuUboeChannel();

    HcclResult Init() override;
    // 查询notify数量
    HcclResult GetNotifyNum(uint32_t *notifyNum) const override;
    // 查询remoteMem
    HcclResult GetRemoteMem(HcclMem **remoteMem, uint32_t *memNum, char** memTags) override;
    // 创建channel前查询channel状态
    ChannelStatus GetStatus() override;
    // GetStatus内部实现
    HcclResult GetStatus(ChannelStatus &status);

    // channel描述
    std::string Describe() const;

    // 数据面调用verbs接口
    // host dpu实现同步机制
    HcclResult NotifyRecord(const uint32_t remoteNotifyIdx);
    // host dpu实现同步机制
    HcclResult NotifyWait(const uint32_t localNotifyIdx, const uint32_t timeout);
    
    HcclResult WriteWithNotify(void *dst, const void *src, const uint64_t len, uint32_t remoteNotifyIdx);
    HcclResult Write(void *dst, const void *src, uint64_t len);
    HcclResult Read(void *dst, const void *src, uint64_t len);
    HcclResult ChannelFence();
    HcclResult GetHcclBuffer(void*& addr, uint64_t& size);

    virtual HcclResult Clean() override;
    virtual HcclResult Resume() override;

private:
    HcclResult ParseInputParam();
    HcclResult StartListen();
    HcclResult BuildSocket();
    HcclResult BuildConnection();
    HcclResult BuildNotify();
    HcclResult BuildBuffer();

    HcclResult CheckSocketStatus();
    HcclResult CreateJetty();
    HcclResult ExchangeData();
    HcclResult ImportJetty();

    void NotifyVecPack(Hccl::BinaryStream &binaryStream);
    HcclResult BufferVecPack(Hccl::BinaryStream &binaryStream);
    HcclResult ConnVecPack(Hccl::BinaryStream &binaryStream);
    // void HandshakeMsgPack(Hccl::BinaryStream &binaryStream);

    // HcclResult HandshakeMsgUnpack(Hccl::BinaryStream &binaryStream);
    HcclResult NotifyVecUnpack(Hccl::BinaryStream &binaryStream);
    HcclResult RmtBufferVecUnpackProc(Hccl::BinaryStream &binaryStream);
    HcclResult ConnVecUnpackProc(Hccl::BinaryStream &binaryStream);

    std::vector<Hccl::JettyInfo> GetJettyInfos() const; // in Connection

    HcclResult PrepareNotifyWrResource(const uint64_t len, const uint32_t remoteNotifyIdx, struct urma_jfs_wr &notifyRecordWr) const;
    HcclResult PrepareWriteWrResource(const void *dst, const void *src, const uint64_t len, const uint32_t remoteNotifyIdx,
                                      struct urma_jfs_wr &writeWithNotifyWr) const;

    HcclResult PostUrmaOp(const char *caller, urma_opcode opcode, void *localAddr, const void *remoteAddr, uint64_t len);
    void BuildUrmaWr(const char *caller, urma_opcode opcode, void *localAddr, const void *remoteAddr, uint64_t len,
                     size_t localIdx, size_t rmtIdx, struct urma_jfs_wr &wr, struct urma_sge &sg) const;
    HcclResult PostAndCheckSend(const char *caller, struct urma_jfs_wr &wr);
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

    std::vector<std::unique_ptr<HostUrmaConnection>> connections_{};
    std::vector<Hccl::LocalUbRmaBuffer *> localRmaBuffers_{};
    std::vector<uint32_t> localDpuNotifyIds_{};
    uint32_t bufferNum_{0};
    uint32_t connNum_{0};
    // Hccl::BaseMemTransport::Attribution attr_;
    ChannelStatus channelStatus_{ChannelStatus::INIT};
    RdmaStatus rdmaStatus_{RdmaStatus::INIT};
    std::vector<uint32_t> remoteDpuNotifyIds_;
    std::vector<std::unique_ptr<Hccl::RemoteUbRmaBuffer>> rmtRmaBuffers_{};
    ExchangeRdmaConnDto rmtConnDto_;
    std::vector<std::unique_ptr<HcclMem>> remoteMems{};
    uint32_t wqeNum_{0};
    std::unique_ptr<SocketMgr> socketMgr_{nullptr};
    bool fenceFlag_{false};

    uint64_t maxMsgSize_{0};

    std::mutex cq_mutex;
    std::mutex sendCq_mutex;
};

} // namespace hcomm

#endif // HOST_CPU_UBOE_CHANNEL_H
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

#include "../channel.h"
#include "enum_factory.h"
#include "hccl_common.h"

// Orion
#include "../../../../../../legacy/unified_platform/resource/socket/socket.h"
#include "../../../../../../legacy/unified_platform/resource/buffer/local_rdma_rma_buffer.h"
#include "remote_rma_buffer.h"
#include "host_rdma_connection.h"
// #include "base_mem_transport.h"

// For hybrid mode
#include "../../../../../../platform/resource/notify/local_ipc_notify.h"
#include "hybrid_mode_types.h"

// Forward declaration
class NotifyPoolImpl;

namespace hcomm {

// 混合模式专用错误码（保留用于向后兼容，主要定义在 hybrid_mode_types.h）
enum class HybridModeErrorCode {
    HYBRID_SUCCESS = 0,
    HYBRID_E_INVALID_QP_STATE = 1,
    HYBRID_E_MEMORY_NOT_REGISTERED = 2,
    HYBRID_E_POST_SEND_FAILED = 3,
    HYBRID_E_NOTIFY_TIMEOUT = 4,
    HYBRID_E_VERSION_MISMATCH = 5,
    HYBRID_E_EXCHANGE_DATA_INVALID = 6,
};

// 无效 Notify ID 常量
static constexpr uint32_t INVALID_DPU_NOTIFY_ID = 0xFFFFFFFF;

class HostCpuRoceChannel final : public Channel {
public:
    MAKE_ENUM(RdmaStatus, INIT, SOCKET_OK, CAP_EXCHANGED, QP_CREATED, DATA_EXCHANGE, QP_MODIFIED, CONN_OK)

    HostCpuRoceChannel(EndpointHandle endpointHandle, HcommChannelDesc channelDesc);
    ~HostCpuRoceChannel();

    HcclResult Init() override;
    HcclResult GetNotifyNum(uint32_t *notifyNum) const override;
    HcclResult GetRemoteMem(HcclMem **remoteMem, uint32_t *memNum, char** memTags) override;
    ChannelStatus GetStatus() override;
    HcclResult GetStatus(ChannelStatus &status);

    std::string Describe() const;

    // 数据面调用verbs接口
    HcclResult NotifyRecord(const uint32_t remoteNotifyIdx) const;
    HcclResult NotifyWait(const uint32_t localNotifyIdx, const uint32_t timeout);
    HcclResult WriteWithNotify(void *dst, const void *src, const uint64_t len, uint32_t remoteNotifyIdx) const;
    HcclResult Write(void *dst, const void *src, uint64_t len) const;
    HcclResult Read(void *dst, const void *src, uint64_t len) const;
    HcclResult ChannelFence() const;
    HcclResult GetHcclBuffer(void*& addr, uint64_t& size);

    // ========== 混合模式（RoCE Cross-Mode）接口 ==========
    // 能力协商
    HcclResult ExchangeCapability();
    HcclResult NegotiateMode();
    
    // 混合模式数据交换
    HcclResult ExchangeDataHybrid();
    HcclResult InitHybridModeResources();
    
    // 混合模式同步接口
    HcclResult WriteWithNotifyHybrid(void *dst, const void *src, uint64_t len, uint32_t remoteNotifyIdx) const;
    HcclResult NotifyWaitHybrid(uint32_t localNotifyIdx, uint32_t timeout);

private:
    HcclResult ParseInputParam();
    // HcclResult BuildAttr();
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

    std::mutex cq_mutex;
    
    // ========== 混合模式（RoCE Cross-Mode）成员变量 ==========
    // 1. 能力协商结果
    RoCECapability remoteCap_;            // 对端能力
    bool isHybridMode_ = false;           // 是否为混合模式
    SyncMode negotiatedSyncMode_ = SyncMode::SYNC_MODE_WRITE_IMM;
    
    // 2. 发送端使用：预分配的 Notify 信号值缓冲区
    uint64_t notifySignalValue_ = 1;
    
    // 3. 发送端使用：对端 TransportIbverbs 的 Notify 地址信息
    // 非对称设计：HostCpuRoceChannel 发送时写入此地址
    uint64_t remoteHostNotifyAddr_ = 0;   // TransportIbverbs 的 Notify 内存地址
    uint32_t remoteHostNotifyRkey_ = 0;   // 该内存的 rkey
    
    // 4. 接收端使用：本地 DPU Notify ID（TransportIbverbs 作为立即数发送的目标）
    uint32_t localDpuNotifyId_ = INVALID_DPU_NOTIFY_ID;  // 本地 Notify ID（供对端作为立即数）
    
    // 5. 本地 Notify 内存基地址（用于混合模式轮询）
    uint64_t localNotifyBaseAddr_ = 0;
    uint32_t localNotifyRkey_ = 0;
    
    // 6. 混合模式使用的 LocalIpcNotify
    std::shared_ptr<hccl::LocalIpcNotify> hybridNotify_;
    uint64_t hybridNotifyOffset_ = 0;
    
    // 7. Notify Pool（用于创建 LocalIpcNotify）
    std::unique_ptr<NotifyPoolImpl> notifyPool_;
    
    // 8. 析构标志（用于避免重复释放资源）
    bool resourcesCleaned_ = false;
};

} // namespace hcomm

#endif // HOST_CPU_ROCE_CHANNEL_H
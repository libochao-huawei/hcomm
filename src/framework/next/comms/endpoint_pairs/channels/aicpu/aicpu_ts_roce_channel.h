/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef AICPU_TS_ROCE_CHANNEL_H
#define AICPU_TS_ROCE_CHANNEL_H

#include "../channel.h"
#include "enum_factory.h"
#include "hccl_common.h"
#include "../sockets/socket_mgr.h"

// Orion
#include "../../../../../../legacy/unified_platform/resource/socket/socket.h"
#include "../../../../../../legacy/unified_platform/resource/buffer/local_rdma_rma_buffer.h"
#include "remote_rma_buffer.h"
#include "dev_rdma_connection.h"
#include "rdma_local_notify.h"
#include "dev_buffer.h"

namespace hcomm {
/**
 * @note 职责：Channel的AicpuTs通信引擎、RoCE协议的类派生
 */
constexpr u32 RDMA_NOTIFY_NUM = 3;

typedef decltype(EndpointLoc::device) EndpointDeviceLoc;

class AicpuTsRoceChannel final : public Channel {
public:
    MAKE_ENUM(RdmaStatus, INIT, SOCKET_OK, QP_CREATED,  DATA_EXCHANGE, QP_MODIFIED, CONN_OK)

    AicpuTsRoceChannel(EndpointHandle endpointHandle, HcommChannelDesc channelDesc, CommEngine engine);
    ~AicpuTsRoceChannel();

    HcclResult Init() override;
    HcclResult GetNotifyNum(uint32_t *notifyNum) const override;
    HcclResult GetBufferNum(uint32_t *bufferNum) const;
    HcclResult GetQpNum(uint32_t *qpNum) const;
    HcclResult GetRemoteMem(HcclMem **remoteMem, uint32_t *memNum, char** memTags) override;
    ChannelStatus GetStatus() override;
    HcclResult GetStatus(ChannelStatus &status);

    std::string Describe() const;

    CommEngine GetCommEngine() const {
        return engine_;
    }
    CommProtocol GetCommProtocol() const {
        return channelDesc_.remoteEndpoint.protocol;
    }
    HcclResult BuildAndGetLocNotifyInfo(Notify** notify);
    HcclResult BuildAndGetRmtNotifyInfo(Notify** notify);
    HcclResult BuildAndGetRmtBufInfo(ProtectionInfo** protectionInfoPtr);
    HcclResult BuildAndGetLocBufInfo(ProtectionInfo** protectionInfoPtr);
    HcclResult BuildAndGetSqContext(SqContext** sqContextPtr);
    HcclResult BuildAndGetCqContext(CqContext** cqContextPtr);

    HcclResult H2DResPack(std::vector<char>& buffer);

    HcclResult Clean() override;
    HcclResult Resume() override;

private:
    HcclResult ParseInputParam();
    HcclResult BuildConnection();
    HcclResult BuildNotify();
    HcclResult BuildBuffer();
    HcclResult BuildNotifyValueBuffer();
    HcclResult BuildSocket();
    HcclResult StartListen();

    HcclResult CheckSocketStatus();
    HcclResult CreateQp();
    HcclResult ExchangeData();
    HcclResult ModifyQp();

    void NotifyVecPack(Hccl::BinaryStream &binaryStream);
    HcclResult BufferVecPack(Hccl::BinaryStream &binaryStream);
    HcclResult ConnVecPack(Hccl::BinaryStream &binaryStream);

    HcclResult NotifyVecUnpack(Hccl::BinaryStream &binaryStream);
    HcclResult RmtBufferVecUnpackProc(Hccl::BinaryStream &binaryStream);
    HcclResult ConnVecUnpackProc(Hccl::BinaryStream &binaryStream);

    std::vector<char> GetLocalNotifyUniqueIds() const;
    std::vector<char> GetRemoteNotifyUniqueIds() const;
    std::vector<char> GetLocBufferUniqueIds() const;
    std::vector<char> GetRmtBufferUniqueIds() const;
    std::vector<char> GetNotifyValueBufferUniqueIds() const;
    std::vector<char> GetSingleRmaBufferUniqueId(u64 addr, u64 size, u32 key) const;
    std::vector<char> GetConnUniqueIds() const;
    std::vector<char> GetUniqueId() const;
    HcclResult PackOpData(std::vector<char> &data) const;

    // 入参
    EndpointHandle endpointHandle_;
    HcommChannelDesc channelDesc_;
    CommEngine engine_;

    // 转换参数
    EndpointDesc localEp_;
    EndpointDesc remoteEp_;
    uint32_t notifyNum_{0};
    std::unique_ptr<SocketMgr>                              socketMgr_{nullptr};
    Hccl::Socket*                                           socket_{nullptr};
    RdmaHandle                                              rdmaHandle_{nullptr};

    std::vector<std::unique_ptr<DevRdmaConnection>>         connections_{};
    std::vector<Hccl::LocalRdmaRmaBuffer *>                 localRmaBuffers_{};
    std::vector<std::unique_ptr<Hccl::RdmaLocalNotify>>     localNotifies_{};
    std::shared_ptr<Hccl::DevBuffer>                        notifyValueMem_{nullptr};
    std::unique_ptr<Hccl::LocalRdmaRmaBuffer>               notifyValueBuffer_{nullptr};
    uint32_t                                                bufferNum_{0};
    uint32_t                                                connNum_{0};
    ChannelStatus                                           channelStatus_{ChannelStatus::INIT};
    RdmaStatus                                              rdmaStatus_{RdmaStatus::INIT};
    std::vector<std::unique_ptr<Hccl::RemoteRdmaRmaBuffer>> remoteNotifies_{};
    std::vector<std::unique_ptr<Hccl::RemoteRdmaRmaBuffer>> rmtRmaBuffers_{};
    ExchangeRdmaConnDto                                     rmtConnDto_;
    std::vector<std::unique_ptr<HcclMem>>                   remoteMems{};

    std::vector<SqContext>                                  sqContextList_{};
    std::vector<CqContext>                                  cqContextList_{};
    std::vector<ProtectionInfo>                             locBufProtecInfoList_{};
    std::vector<ProtectionInfo>                             rmtBufProtecInfoList_{};
};

} // namespace hcomm

#endif // AICPU_TS_ROCE_CHANNEL_H
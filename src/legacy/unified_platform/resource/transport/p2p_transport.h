/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef P2P_TRANSPORT_H
#define P2P_TRANSPORT_H

#include "base_mem_transport.h"
#include "virtual_topo.h"

namespace Hccl {
class P2PTransport : public BaseMemTransport {
public:
    P2PTransport(CommonLocRes &commonLocRes, Attribution &attr, const LinkData &linkData, const Socket &socket);

    P2PTransport(CommonLocRes &commonLocRes, Attribution &attr, const LinkData &linkData, const Socket &socket, std::function<void(u32 streamId, u32 taskId, TaskParam taskParam)> callback);

    std::string Describe() const override;

    TransportStatus GetStatus() override;

    // RemoteRmaBuffer *GetRmtRmaBuffer(u32 index) override // transport完成交换后该方法挪到父类中

    void Post(u32 index, const Stream &stream) override;

    void Read(const RmaBufferSlice &locSlice, const RmtRmaBufferSlice &rmtSlice, const Stream &stream) override;

    void ReadReduce(const RmaBufferSlice &locSlice, const RmtRmaBufferSlice &rmtSlice, const ReduceIn &reduceIn,
                    const Stream &stream) override;

    void Write(const RmaBufferSlice &locSlice, const RmtRmaBufferSlice &rmtSlice, const Stream &stream) override;

    void WriteReduce(const RmaBufferSlice &locSlice, const RmtRmaBufferSlice &rmtSlice, const ReduceIn &reduceIn,
                     const Stream &stream) override;

private:
    MemoryBuffer GetLocMemBuffer(const RmaBufferSlice &locSlice) const;
    MemoryBuffer GetRmtMemBuffer(const RmtRmaBufferSlice &rmtSlice) const;

    MAKE_ENUM(P2PStatus, INIT, SOCKET_OK, SEND_PID, RECV_PID, GRANT, SEND_DATA, RECV_DATA)
    P2PStatus p2pStatus{P2PStatus::INIT};

    u32 pidMsgSize{0};
    u32 myPid{0};
    u32 rmtPid{0};

    bool rmtPidValid{false};

    std::vector<std::unique_ptr<IpcRemoteNotify>>    rmtNotifyVec;
    std::vector<std::unique_ptr<RemoteIpcRmaBuffer>> rmtBufferVec;

    bool IsRmtPidValid() const;
    void SendPid();
    void RecvPid();
    void Grant();
    void SendExchangeData();
    void RecvExchangeData();

    void BufferVecPack(BinaryStream &binaryStream);

    void RmtNotifyVecUnpackProc(BinaryStream &binaryStream);
    void RmtBufferVecUnpackProc(BinaryStream &binaryStream);
};

} // namespace Hccl

#endif
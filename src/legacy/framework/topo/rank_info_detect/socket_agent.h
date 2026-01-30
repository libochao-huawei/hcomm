/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: socket agent header file
 * Create: 2024-02-29
 */

#ifndef HCCLV2_SOCKET_AGENT_H
#define HCCLV2_SOCKET_AGENT_H

#include "socket.h"

namespace Hccl {

class SocketAgent {
public:
    explicit SocketAgent(Socket *socket) : socket_(socket)
    {
    }

    void SendMsg(const void *data, u64 dataLen);
    bool RecvMsg(void *msg, u64 &revMsgLen);

private:
    Socket *socket_{nullptr};
};

} // namespace Hccl
#endif // HCCLV2_SOCKET_AGENT_H

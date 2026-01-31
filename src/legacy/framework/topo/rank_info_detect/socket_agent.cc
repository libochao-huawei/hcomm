/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: socket agent
 * Create: 2024-02-29
 */

#include "socket_agent.h"
#include <climits>
#include "socket_exception.h"
#include "root_handle_v2.h"
#include "null_ptr_exception.h"
#include "socket_agent.h"

namespace Hccl {

void SocketAgent::SendMsg(const void *data, u64 dataLen)
{
    HCCL_INFO("[SocketAgent::%s] start, dataLen[%llu].", __func__, dataLen);

    // 发送数据长度
    CHK_PRT_THROW(!socket_->Send(&dataLen, sizeof(dataLen)), HCCL_ERROR("[SocketAgent::%s] Send data len failed", __func__),
                  SocketException, "Unable to send data");
    
    // 发送数据
    CHK_PRT_THROW(!socket_->Send(data, dataLen), HCCL_ERROR("[SocketAgent::%s] Send data failed", __func__),
                  SocketException, "Unable to send data");

    HCCL_INFO("[SocketAgent::%s] end.", __func__);
}

bool SocketAgent::RecvMsg(void *msg, u64 &revMsgLen)
{
    HCCL_INFO("[SocketAgent::%s] start.", __func__);

    // 检查 msg 是否为 nullptr
    CHK_PRT_THROW(msg == nullptr, 
        HCCL_ERROR("[RankInfoDetectService::%s] msg is nullptr", __func__), 
        NullPtrException, "RecvMsg fail");

    // 先接收长度
    EXECEPTION_CATCH(socket_->Recv(&revMsgLen, sizeof(revMsgLen)), return false);
    CHK_PRT_RET(revMsgLen == 0 || revMsgLen > MAX_BUFFER_LEN,
        HCCL_ERROR("[SocketAgent::%s] Invalid length[%llu]", __func__, revMsgLen), false);

    // 再接收内容
    EXECEPTION_CATCH(socket_->Recv(msg, revMsgLen), return false);

    HCCL_INFO("[SocketAgent::%s] end, revMsgLen[%llu].", __func__, revMsgLen);
    return true;
}

} // namespace Hccl

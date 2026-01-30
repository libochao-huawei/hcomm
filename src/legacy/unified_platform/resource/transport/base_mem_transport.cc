/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "base_mem_transport.h"
#include "internal_exception.h"
#include "timeout_exception.h"
#include "socket_exception.h"
#include "coll_operator_check.h"

namespace Hccl {
BaseMemTransport::BaseMemTransport(CommonLocRes &commonLocRes, Attribution &attr, const LinkData &linkData,
                                   const Socket &socket, TransportType type)
    : commonLocRes(commonLocRes), attr(attr), linkData(linkData), socket(const_cast<Socket *>(&socket)),
      transportType(type)
{
    CheckCommonLocRes(commonLocRes);
}

BaseMemTransport::BaseMemTransport(CommonLocRes &commonLocRes, Attribution &attr, const LinkData &linkData,
                                   const Socket &socket, TransportType type, std::function<void(u32 streamId, u32 taskId, TaskParam taskParam)> callback)
    : commonLocRes(commonLocRes), attr(attr), linkData(linkData), socket(const_cast<Socket *>(&socket)),
      transportType(type), callback(callback)
{
    CheckCommonLocRes(commonLocRes);
}

void BaseMemTransport::Establish()
{
    baseStatus = TransportStatus::INIT;
    rmtRmaBufferVec.clear();
}

void BaseMemTransport::SetBaseStatusReady()
{
    baseStatus = TransportStatus::READY;
}

bool BaseMemTransport::IsSocketReady()
{
    if (socket == nullptr) {
        MACRO_THROW(InternalException, StringFormat("%s socket is nullptr, please check", GetLinkDescInfo().c_str()));
    }

    SocketStatus socketStatus = socket->GetAsyncStatus();
    if (socketStatus == SocketStatus::OK) {
        baseStatus = TransportStatus::SOCKET_OK;
        return true;
    } else if (socketStatus == SocketStatus::TIMEOUT) {
        baseStatus = TransportStatus::SOCKET_TIMEOUT;
        return false;
    }

    return false;
}

void BaseMemTransport::NotifyVecPack(BinaryStream &binaryStream)
{
    binaryStream << notifyNum;
    HCCL_INFO("start pack %s notifyVec", transportType.Describe().c_str());
    u32 pos = 0;
    for (auto &it : commonLocRes.notifyVec) {
        binaryStream << pos;
        std::unique_ptr<Serializable> dto = it->GetExchangeDto();
        dto->Serialize(binaryStream);
        HCCL_INFO("pack notify pos=%u, dto %s", pos, dto->Describe().c_str());
        pos++;
    }
}

void BaseMemTransport::ConnVecPack(BinaryStream &binaryStream)
{
    binaryStream << connNum;
    HCCL_INFO("start pack %s connVec", transportType.Describe().c_str());
    u32 pos = 0;
    for (auto &it : commonLocRes.connVec) {
        binaryStream << pos;
        std::unique_ptr<Serializable> dto = it->GetExchangeDto();
        dto->Serialize(binaryStream);
        HCCL_INFO("pack connection pos=%u, dto %s", pos, dto->Describe().c_str());
        pos++;
    }
}

void BaseMemTransport::HandshakeMsgPack(BinaryStream &binaryStream)
{
    HCCL_INFO("start pack %s handshakeMsg, size=%u", transportType.Describe().c_str(), attr.handshakeMsg.size());
    binaryStream << static_cast<u32>(attr.opAcceState);
    binaryStream << attr.handshakeMsg;
}

void BaseMemTransport::HandshakeMsgUnpack(BinaryStream &binaryStream)
{
    u32 rmtAccelerator{0};
    binaryStream >> rmtAccelerator;
    HCCL_INFO("[BaseMemTransport::HandshakeMsgUnpack], rmtAccelerator[%u]", rmtAccelerator);
    rmtOpAcceState = static_cast<AcceleratorState::Value>(rmtAccelerator);

    if (rmtOpAcceState != attr.opAcceState) {
        THROW<InvalidParamsException>(
            StringFormat("[BaseMemTransport::HandshakeMsgUnpack] Accelerator information check fail. "
                         "locOpAccelerator[%s], rmtOpAccelerator[%s]",
                         attr.opAcceState.Describe().c_str(), rmtOpAcceState.Describe().c_str()));
    }

    rmtHandshakeMsg.clear();
    binaryStream >> rmtHandshakeMsg;

    if (attr.handshakeMsg.size() != rmtHandshakeMsg.size()) {
        MACRO_THROW(InvalidParamsException, StringFormat("handshakeMsg size=%u is not equal to rmt=%u",
                                                         attr.handshakeMsg.size(), rmtHandshakeMsg.size()));
    }

    //单边通信情况下，handshakeMsg的size为0
    if (attr.handshakeMsg.size() == 0) {
        return;
    }
    auto localCollOperator = CollOperator::GetPackedData(attr.handshakeMsg);
    auto remoteCollOperator = CollOperator::GetPackedData(rmtHandshakeMsg);
    CheckCollOperator(localCollOperator, remoteCollOperator); // 两端算子参数一致性校验
}

string BaseMemTransport::GetLinkDescInfo()
{
    return StringFormat("rank[%u], rmtRank[%u] linkData=%s, type=%s", linkData.GetLocalRankId(),
                        linkData.GetRemoteRankId(), linkData.Describe().c_str(), transportType.Describe().c_str());
}

string BaseMemTransport::DescribeSocket() const
{
    return StringFormat("BaseMemTransport socket=[%s]", socket->Describe().c_str());
}

void BaseMemTransport::CheckLocNotify(CommonLocRes &res)
{
    HCCL_INFO("%s notify check start, notifyNum=%u", GetLinkDescInfo().c_str(), res.notifyVec.size());
    // notify 不允许出现空指针情况
    for (auto &it : res.notifyVec) {
        if (it == nullptr) {
            string msg = StringFormat("%s notify is nullptr", GetLinkDescInfo().c_str());
            MACRO_THROW(InvalidParamsException, msg);
        }
        HCCL_INFO("locNotify=%s", it->Describe().c_str());
    }
    HCCL_INFO("%s notify check ok, notifyNum=%u", GetLinkDescInfo().c_str(), res.notifyVec.size());
}

void BaseMemTransport::CheckLocBuffer(CommonLocRes &res)
{
    HCCL_INFO("%s buffer check start, bufferNum=%u", GetLinkDescInfo().c_str(), res.bufferVec.size());
    u32 bufIndex = 0;
    for (auto &it : res.bufferVec) {
        if (it == nullptr) {
            HCCL_INFO("bufIndex=%u is nullptr", bufIndex);
        } else {
            HCCL_INFO("bufIndex=%u, buf=%s", bufIndex, it->Describe().c_str());
        }
        bufIndex++;
    }

    HCCL_INFO("%s buffer check ok, bufferNum=%u", GetLinkDescInfo().c_str(), res.bufferVec.size());
}

void BaseMemTransport::CheckLocConn(CommonLocRes &res)
{
    HCCL_INFO("%s connection check start, connNum=%u", GetLinkDescInfo().c_str(), res.connVec.size());
    for (auto &it : res.connVec) {
        if (it == nullptr) {
            string msg = StringFormat("%s conn is nullptr", GetLinkDescInfo().c_str());
            MACRO_THROW(InvalidParamsException, msg);
        }
        HCCL_INFO("conn=%s", it->Describe().c_str());
    }
    HCCL_INFO("%s connection check ok, connNum=%u", GetLinkDescInfo().c_str(), res.connVec.size());
}

void BaseMemTransport::CheckCommonLocRes(CommonLocRes &res)
{
    CheckLocNotify(res);
    CheckLocBuffer(res);
    CheckLocConn(res);
}

} // namespace Hccl
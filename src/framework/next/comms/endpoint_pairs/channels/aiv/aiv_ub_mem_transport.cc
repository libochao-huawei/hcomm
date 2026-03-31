/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "aiv_ub_mem_transport.h"
#include "exception_handler.h"
#include "../../../../../../legacy/unified_platform/resource/socket/socket.h"
#include "../../../../../../legacy/unified_platform/resource/buffer/exchange_ipc_buffer_dto.h"
#include "../../../../../../legacy/common/utils/string_util.h"
#include "../../../../../../legacy/unified_platform/resource/mem/user_remote_mem_getter.h"
#include "comm_mems.h"

namespace hcomm {

AivUbMemTransport::AivUbMemTransport(Hccl::Socket *socket, HcommChannelDesc &channelDesc) : socket_(socket), 
    channelDesc_(channelDesc) {}

HcclResult AivUbMemTransport::Init()
{
    // 拷贝memTag信息
    uint32_t bufferNum = channelDesc_.memHandleNum;
    localUserMemTag_.reserve(bufferNum);
    if (bufferNum == 0) {
        HCCL_ERROR("[AivUbMemTransport][Init] bufferNum is 0.");
        return HCCL_E_PARA;
    }
    HCCL_INFO("channelDesc_.memHandleNum: %u", bufferNum);
    for (uint32_t i = 0; i < bufferNum; ++i) {
        auto locMemInfo = reinterpret_cast<hccl::CommMemHandle *>(channelDesc_.memHandles[i]);
        auto localIpcRmaBuffer = reinterpret_cast<Hccl::LocalIpcRmaBuffer *>(locMemInfo->bufferHandle);
        localRmaBufferVec_.push_back(localIpcRmaBuffer);
        std::array<char, HCCL_RES_TAG_MAX_LEN> memTag{};
        std::string tag = locMemInfo->memTag;
        if (UNLIKELY(tag.size() >= HCCL_RES_TAG_MAX_LEN)) {
            HCCL_ERROR("[AivUbMemTransport][Init] tagSize exceeds limit[%u]", HCCL_RES_TAG_MAX_LEN);
            return HCCL_E_PARA;
        }
        CHK_SAFETY_FUNC_RET(memcpy_s(memTag.data(), memTag.size(), tag.c_str(), tag.size()));
        HCCL_INFO("[AivUbMemTransport][Init] memHandleNum[%u] memTag[%s]", i, memTag.data());
        localUserMemTag_.push_back(memTag);
    }

    baseStatus_ = Hccl::TransportStatus::INIT;
    return HCCL_SUCCESS;
}

HcclResult AivUbMemTransport::IsSocketReady(bool &isReady)
{
    CHK_PTR_NULL(socket_);

    Hccl::SocketStatus socketStatus = socket_->GetAsyncStatus();
    if (socketStatus == Hccl::SocketStatus::OK) {
        baseStatus_ = Hccl::TransportStatus::SOCKET_OK;
        isReady = true;
    } else if (socketStatus == Hccl::SocketStatus::TIMEOUT) {
        baseStatus_ = Hccl::TransportStatus::SOCKET_TIMEOUT;
        isReady = false;
    }

    return HCCL_SUCCESS;
}

Hccl::TransportStatus AivUbMemTransport::GetStatus()
{
    if (baseStatus_ == Hccl::TransportStatus::READY) {
        return baseStatus_;
    } else if (baseStatus_ == Hccl::TransportStatus::INIT) {
        aivUbStatus_ = AivUbMemTransportStatus::INIT;
    }

    bool isReady = false;
    IsSocketReady(isReady);
    if (!isReady) {
        return baseStatus_;
    }
    HCCL_INFO("%s aivUbStatus_[%d], baseStatus_[%d] start, aivUbStatus_::SOCKET_OK[%d]", 
        __func__, aivUbStatus_, baseStatus_, AivUbMemTransportStatus::SOCKET_OK);
    switch (aivUbStatus_) {
        case AivUbMemTransportStatus::INIT:
            aivUbStatus_ = AivUbMemTransportStatus::SOCKET_OK;
            baseStatus_ = Hccl::TransportStatus::SOCKET_OK;
            break;
        case AivUbMemTransportStatus::SOCKET_OK:
            SendMemInfo();
            aivUbStatus_ = AivUbMemTransportStatus::SEND_MEM_INFO;
            break;
        case AivUbMemTransportStatus::SEND_MEM_INFO:
            RecvMemInfo();
            aivUbStatus_ = AivUbMemTransportStatus::RECV_MEM_INFO;
            break;
        case AivUbMemTransportStatus::RECV_MEM_INFO:
            RecvDataProcess();
            aivUbStatus_ = AivUbMemTransportStatus::RECV_MEM_FIN;
            break;
        case AivUbMemTransportStatus::RECV_MEM_FIN:
            aivUbStatus_ = AivUbMemTransportStatus::READY;
            baseStatus_ = Hccl::TransportStatus::READY;
            break;
        default:
            break;
    }
    HCCL_INFO("%s aivUbStatus_[%d], baseStatus_[%d]", __func__, aivUbStatus_, baseStatus_);
    return baseStatus_;
}

HcclResult AivUbMemTransport::SendMemInfo()
{
    HCCL_INFO("[%s] start", __func__);

    Hccl::BinaryStream binaryStream;
    BufferPack(binaryStream); // 名字？
    
    binaryStream.Dump(sendData_);
    EXCEPTION_HANDLE_BEGIN
    socket_->SendAsync(reinterpret_cast<u8 *>(&sendData_[0]), sendData_.size());
    EXCEPTION_HANDLE_END
    exchangeDataSize_ = sendData_.size();
    HCCL_INFO("[%s] finished", __func__);
    return HCCL_SUCCESS;
}

void AivUbMemTransport::BufferPack(Hccl::BinaryStream &binaryStream)
{
    u32 vecSize = localRmaBufferVec_.size();
    binaryStream << vecSize;
    HCCL_RUN_INFO("BufferPack vecSize=%u", vecSize);

    for (uint32_t i = 0; i < vecSize; ++i) {
        std::unique_ptr<Hccl::Serializable> dto = localRmaBufferVec_[i]->GetExchangeDto();
        HCCL_INFO("[%s] dto[%s]", __func__, dto->Describe().c_str());
        dto->Serialize(binaryStream);
    }

    for (const auto& tag : localUserMemTag_) {
        // 逐个字节传输
        for (uint32_t i = 0; i < HCCL_RES_TAG_MAX_LEN; i++) {
            binaryStream << static_cast<u8>(tag[i]);
        }
    }
}

HcclResult AivUbMemTransport::RecvMemInfo()
{
    recvData_.resize(exchangeDataSize_);
    EXCEPTION_HANDLE_BEGIN
    socket_->RecvAsync(reinterpret_cast<u8 *>(&recvData_[0]), recvData_.size());
    EXCEPTION_HANDLE_END
    // HCCL_INFO("recv data, size=%llu, data=%s", data.size(), Hccl::Bytes2hex(data.data(), data.size()).c_str());
    return HCCL_SUCCESS;
}

HcclResult AivUbMemTransport::RecvDataProcess()
{
    Hccl::BinaryStream binaryStream(recvData_);
    EXCEPTION_HANDLE_BEGIN
    RmtBufferUnpackProc(binaryStream);
    EXCEPTION_HANDLE_END
    return HCCL_SUCCESS;
}

void AivUbMemTransport::RmtBufferUnpackProc(Hccl::BinaryStream &binaryStream)
{
    rmtBufferVec_.clear(); // 这两个可以归一吗？
    rmtRmaBufferVec_.clear();
    u32 vecSize{0};
    binaryStream >> vecSize;
    HCCL_RUN_INFO("vecSize=%u", vecSize);
    if (UNLIKELY(vecSize > MAX_BUFFER_NUM)) {
        EXCEPTION_THROW_IF_ERR(HCCL_E_PARA, "[AivUbMemTransport][RmtBufferUnpackProc] vecSize exceeds limit.");
    }
    for (u32 pos = 0; pos < vecSize; ++pos) {
        Hccl::ExchangeIpcBufferDto dto;
        dto.Deserialize(binaryStream);
        HCCL_INFO("[%s] dto[%s]", __func__, dto.Describe().c_str());
        if (dto.size == 0) { // size为0，则为 remote 空buffer
            HCCL_INFO("unpack nullptr, pos=%u", pos);
            rmtBufferVec_.push_back(nullptr);
            rmtRmaBufferVec_.push_back(nullptr);
        } else { // size非0，则构造一个remote buffer
            rmtBufferVec_.push_back(std::make_unique<Hccl::RemoteIpcRmaBuffer>(dto, "UbMemory"));
            rmtRmaBufferVec_.push_back(rmtBufferVec_.back().get());
        }
    }
    
    remoteUserMemTag_.resize(vecSize);
    for (auto& tag : remoteUserMemTag_) {
        for (uint32_t i = 0; i < HCCL_RES_TAG_MAX_LEN; i++) {
            u8 byte;
            binaryStream >> byte;
            tag[i] = static_cast<char>(byte);
        }
    }
}

HcclResult AivUbMemTransport::GetRemoteMem(HcclMem **remoteMem, uint32_t *memNum, char **memTags) 
{
    CHK_PRT_RET(!remoteMem, HCCL_ERROR("[GetRemoteMem] remoteMem is nullptr"), HCCL_E_PARA);
    CHK_PRT_RET(!memNum, HCCL_ERROR("[GetRemoteMem] memNum is nullptr"), HCCL_E_PARA);
    CHK_PRT_RET(!memTags, HCCL_ERROR("[GetRemoteMem] memTags is nullptr"), HCCL_E_PARA);

    std::lock_guard<std::mutex> lock(remoteMemsMutex_);

    if (*memNum == 0) {
        // 只传cclbuffer
        uint32_t cclbufferNum = 1;
        *memNum = cclbufferNum;
        remoteMems_.resize(cclbufferNum);
        auto& rmtBuffer = rmtBufferVec_[0];
        remoteMems_[0].type = rmtBuffer->GetMemType();
        remoteMems_[0].addr = reinterpret_cast<void *>(rmtBuffer->GetAddr());
        remoteMems_[0].size = rmtBuffer->GetSize();
        remoteMem[0] = &remoteMems_[0];
        CHK_RET(GetMemTag(memTags, cclbufferNum));
    } else {
        // 只传用户注册内存
        uint32_t totalCount = rmtBufferVec_.size();
        CHK_PRT_RET((totalCount > *memNum), 
            HCCL_ERROR("[GetRemoteMem] real remote memNum is greater than input memNum"), HCCL_E_PARA);
        *memNum = totalCount;
        if (totalCount == 1) {
            HCCL_INFO("[GetRemoteMem] No remote memory regions available");
            return HCCL_SUCCESS;
        }
        remoteMems_.resize(totalCount);
        for (uint32_t i = 0; i < totalCount; i++) {
            auto& rmtBuffer = rmtBufferVec_[i];
            remoteMems_[i].type = rmtBuffer->GetMemType();
            remoteMems_[i].addr = reinterpret_cast<void *>(rmtBuffer->GetAddr());
            remoteMems_[i].size = rmtBuffer->GetSize();
            remoteMem[i] = &remoteMems_[i];
        }
        CHK_RET(GetMemTag(memTags, totalCount));
    }
    return HCCL_SUCCESS;
}

HcclResult AivUbMemTransport::GetMemTag(char **memTag, uint32_t memNum)
{
    for (uint32_t i = 0; i < memNum; i++) {
        memTag[i] = const_cast<char*>(remoteUserMemTag_[i].data());
        if (strlen(memTag[i]) >= HCCL_RES_TAG_MAX_LEN) {
            memTag[i][HCCL_RES_TAG_MAX_LEN - 1] = '\0';
        }
        HCCL_INFO("[%s] memTag[%s]", __func__, memTag[i]);
    }
    return HCCL_SUCCESS;
}

HcclResult AivUbMemTransport::GetUserRemoteMem(CommMem **remoteMem, char ***memTags, uint32_t *memNum)
{
    std::lock_guard<std::mutex> lock(remoteMemsMutex_);
    uint32_t userMemCount = rmtBufferVec_.size() - 1; // 默认 cclBuffer 数量为1，后续出现1的含义也是 cclBufferNum
    auto cacheBuilder = [](Hccl::RemoteMemCtx<std::unique_ptr<Hccl::RemoteIpcRmaBuffer>> &remoteMemCtx, uint32_t index) {
        auto &rmtBuffer = remoteMemCtx.rmtBufferVec[index + 1];
        if (rmtBuffer == nullptr) {
            return;
        }
        remoteMemCtx.remoteUserMems[index].type = hccl::ConvertHcclToCommMemType(rmtBuffer->GetMemType());
        remoteMemCtx.remoteUserMems[index].addr = reinterpret_cast<void *>(rmtBuffer->GetAddr());
        remoteMemCtx.remoteUserMems[index].size = rmtBuffer->GetSize();
    };
    Hccl::RemoteMemCtx<std::unique_ptr<Hccl::RemoteIpcRmaBuffer>> remoteMemCtx{
        userMemCount, cacheValid_, rmtBufferVec_, remoteUserMemTag_, remoteUserMems_, tagCopies_, tagPointers_,
        cacheBuilder, remoteMem, memTags, memNum};
    CHK_RET(Hccl::GetRemoteUserMem(remoteMemCtx));
    return HCCL_SUCCESS;
}
}

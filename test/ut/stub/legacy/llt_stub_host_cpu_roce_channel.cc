/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "host_cpu_roce_channel.h"

namespace hcomm {

HostCpuRoceChannel::HostCpuRoceChannel(EndpointHandle endpointHandle, HcommChannelDesc channelDesc)
    : endpointHandle_(endpointHandle), channelDesc_(channelDesc)
{
}

HostCpuRoceChannel::~HostCpuRoceChannel()
{
}

HcclResult HostCpuRoceChannel::ParseInputParam()
{
    return HCCL_SUCCESS;
}

HcclResult HostCpuRoceChannel::BuildConnection()
{
    return HCCL_SUCCESS;
}

HcclResult HostCpuRoceChannel::BuildNotify()
{
    return HCCL_SUCCESS;
}

HcclResult HostCpuRoceChannel::BuildBuffer()
{
    return HCCL_SUCCESS;
}

HcclResult HostCpuRoceChannel::Init()
{
    return HCCL_SUCCESS;
}

ChannelStatus HostCpuRoceChannel::GetStatus()
{
    return ChannelStatus::INIT;
}

HcclResult HostCpuRoceChannel::GetStatus(ChannelStatus &status)
{
    (void)status;
    return HCCL_SUCCESS;
}

HcclResult HostCpuRoceChannel::CheckSocketStatus()
{
    return HCCL_SUCCESS;
}

HcclResult HostCpuRoceChannel::CreateQp()
{
    return HCCL_SUCCESS;
}

HcclResult HostCpuRoceChannel::ExchangeData()
{
    return HCCL_SUCCESS;
}

void HostCpuRoceChannel::NotifyVecPack(Hccl::BinaryStream &binaryStream)
{
    (void)binaryStream;
}

HcclResult HostCpuRoceChannel::BufferVecPack(Hccl::BinaryStream &binaryStream)
{
    (void)binaryStream;
    return HCCL_SUCCESS;
}

HcclResult HostCpuRoceChannel::ConnVecPack(Hccl::BinaryStream &binaryStream)
{
    (void)binaryStream;
    return HCCL_SUCCESS;
}

HcclResult HostCpuRoceChannel::RmtBufferVecUnpackProc(Hccl::BinaryStream &binaryStream)
{
    (void)binaryStream;
    return HCCL_SUCCESS;
}

HcclResult HostCpuRoceChannel::NotifyVecUnpack(Hccl::BinaryStream &binaryStream)
{
    (void)binaryStream;
    return HCCL_SUCCESS;
}

HcclResult HostCpuRoceChannel::ConnVecUnpackProc(Hccl::BinaryStream &binaryStream)
{
    (void)binaryStream;
    return HCCL_SUCCESS;
}

HcclResult HostCpuRoceChannel::ModifyQp()
{
    return HCCL_SUCCESS;
}

HcclResult HostCpuRoceChannel::GetRemoteMem(HcclMem **remoteMem, uint32_t *memNum, char **memTags)
{
    (void)remoteMem;
    (void)memNum;
    (void)memTags;
    return HCCL_SUCCESS;
}

std::vector<Hccl::QpInfo> HostCpuRoceChannel::GetQpInfos() const
{
    return {};
}

std::string HostCpuRoceChannel::Describe() const
{
    return "";
}

HcclResult HostCpuRoceChannel::IbvPostRecv() const
{
    return HCCL_SUCCESS;
}

HcclResult HostCpuRoceChannel::PrepareNotifyWrResource(
    const uint64_t len, const uint32_t remoteNotifyIdx, struct ibv_send_wr &notifyRecordWr) const
{
    (void)len;
    (void)remoteNotifyIdx;
    (void)notifyRecordWr;
    return HCCL_SUCCESS;
}

HcclResult HostCpuRoceChannel::NotifyRecord(const uint32_t remoteNotifyIdx) const
{
    (void)remoteNotifyIdx;
    return HCCL_SUCCESS;
}

HcclResult HostCpuRoceChannel::NotifyWait(const uint32_t localNotifyIdx, const uint32_t timeout)
{
    (void)localNotifyIdx;
    (void)timeout;
    return HCCL_SUCCESS;
}

HcclResult HostCpuRoceChannel::PrepareWriteWrResource(const void *dst, const void *src, const uint64_t len,
    const uint32_t remoteNotifyIdx, struct ibv_send_wr &writeWithNotifyWr) const
{
    (void)dst;
    (void)src;
    (void)len;
    (void)remoteNotifyIdx;
    (void)writeWithNotifyWr;
    return HCCL_SUCCESS;
}

HcclResult HostCpuRoceChannel::WriteWithNotify(
    void *dst, const void *src, const uint64_t len, const uint32_t remoteNotifyIdx) const
{
    (void)dst;
    (void)src;
    (void)len;
    (void)remoteNotifyIdx;
    return HCCL_SUCCESS;
}

HcclResult HostCpuRoceChannel::Write(void *dst, const void *src, const uint64_t len) const
{
    (void)dst;
    (void)src;
    (void)len;
    return HCCL_SUCCESS;
}

HcclResult HostCpuRoceChannel::Read(void *dst, const void *src, const uint64_t len) const
{
    (void)dst;
    (void)src;
    (void)len;
    return HCCL_SUCCESS;
}

HcclResult HostCpuRoceChannel::ChannelFence() const
{
    return HCCL_SUCCESS;
}

HcclResult HostCpuRoceChannel::GetNotifyNum(uint32_t *notifyNum) const
{
    (void)notifyNum;
    return HCCL_SUCCESS;
}

HcclResult HostCpuRoceChannel::GetHcclBuffer(void *&addr, uint64_t &size)
{
    (void)addr;
    (void)size;
    return HCCL_SUCCESS;
}

} // namespace hcomm

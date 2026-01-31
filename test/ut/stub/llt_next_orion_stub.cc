/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <driver/ascend_hal.h>
#include "rt_external.h"
#include <hccl/base.h>
#include <hccl/hccl_types.h>
#include <securec.h>
#include <unistd.h>
#include <signal.h>
#include <syscall.h>
#include <sys/prctl.h>
#include <syslog.h>
#include <sys/mman.h>
#include <sys/stat.h>        /* For mode constants */
#include <fcntl.h>           /* For O_* constants */
#include <arpa/inet.h>
#include <time.h>
#include <map>

#include <stdlib.h>
#include <set>
#include <mutex>

#include "coll_comm.h"
#include "host_cpu_roce_channel.h"
#include "stream_lite.h"
#include "op_base_v2.h"
#include "host_rdma_connection.h"
#include "aicpu_res_package_helper.h"
#include "hcomm_c_adpt.h"

HcclResult HcclCommDestroyV2(HcclComm comm)
{
    return HCCL_SUCCESS;
}

HcclResult HcclChannelGetHcclBufferA5(HcclComm comm, ChannelHandle channel, void **buffer, uint64_t *size)
{
    return HCCL_SUCCESS;
}

HcclResult HcommChannelGetNotifyNum(ChannelHandle channelHandle, uint32_t *notifyNum)
{
    return HCCL_SUCCESS;
}

namespace hccl {
HcclResult MyRank::ChannelGetHcclBuffer(ChannelHandle channel, void **buffer, uint64_t *size) {
    return HCCL_SUCCESS;
}

HcclResult CommMems::GetHcclBuffer(void *&addr, uint64_t &len)
{
    return HCCL_SUCCESS;
} 
}

namespace hccl {

CollComm::CollComm(void * comm, uint32_t rankId, const std::string &commName, const ManagerCallbacks& callbacks)
    : comm_(comm), rankId_(rankId), commId_ (commName), callbacks_(callbacks)
{}

CollComm::~CollComm()
{}

HcclResult CollComm::Init(void * rankGraph, aclrtBinHandle binHandle, HcclMem cclBuffer)
{
    return HCCL_SUCCESS;
}

uint32_t CollComm::GetMyRankId() const
{
    return rankId_;
}

}  // namespace hccl

namespace Hccl {

StreamLite::StreamLite(std::vector<char> &uniqueId)
{
}

StreamLite::StreamLite(u32 id, u32 sqIds, u32 phyId, u32 cqIds) : id(id), sqId(sqIds), devPhyId(phyId), cqId(cqIds)
{
}

u32 StreamLite::GetId() const
{
    return id;
}

u32 StreamLite::GetSqId() const
{
    return sqId;
}

RtsqBase *StreamLite::GetRtsq() const
{
    return rtsq.get();
}

std::vector<ModuleData> AicpuResPackageHelper::ParsePackedData(std::vector<char> &data) const
{
    std::vector<ModuleData> result;
    return result;
}

}  // namespace Hccl

namespace hcomm {
HostRdmaConnection::HostRdmaConnection(Hccl::Socket *socket, RdmaHandle rdmaHandle):
    socket_(socket), rdmaHandle_(rdmaHandle)
{
}

HcclResult HostRdmaConnection::Init()
{
    return HCCL_SUCCESS;
}

HostRdmaConnection::~HostRdmaConnection()
{
}

HostCpuRoceChannel::HostCpuRoceChannel(EndpointHandle endpointHandle, HcommChannelDesc channelDesc)
    : endpointHandle_(endpointHandle), channelDesc_(channelDesc) {}

HostCpuRoceChannel::~HostCpuRoceChannel() 
{
}

HcclResult HostCpuRoceChannel::Init()
{
    return HCCL_SUCCESS;
}

HcclResult HostCpuRoceChannel::GetRemoteMem(HcclMem **remoteMem, uint32_t *memNum, char** memTags)
{
    return HCCL_SUCCESS;
}

HcclResult HostCpuRoceChannel::GetNotifyNum(uint32_t *notifyNum) const
{
    return HCCL_SUCCESS;
}

ChannelStatus HostCpuRoceChannel::GetStatus()
{
    return ChannelStatus::READY;;
}

HcclResult HostCpuRoceChannel::NotifyRecord(const uint32_t remoteNotifyIdx) const
{
    return HCCL_SUCCESS;
}

HcclResult HostCpuRoceChannel::NotifyWait(const uint32_t localNotifyIdx, const uint32_t timeout)
{
    return HCCL_SUCCESS;
}

HcclResult HostCpuRoceChannel::WriteWithNotify(
    void *dst, const void *src, const uint64_t len, const uint32_t remoteNotifyIdx) const
{
    return HCCL_SUCCESS;
}

HcclResult HostCpuRoceChannel::Write(void *dst, const void *src, const uint64_t len) const
{
    return HCCL_SUCCESS;
}

HcclResult HostCpuRoceChannel::Read(void *dst, const void *src, const uint64_t len) const 
{
    return HCCL_SUCCESS;
}

HcclResult HostCpuRoceChannel::ChannelFence() const
{
    return HCCL_SUCCESS;
}

}  // namespace hcomm
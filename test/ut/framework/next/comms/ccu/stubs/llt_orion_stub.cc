/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
 
#include "rdma_handle_manager.h"
#include "../../../../../../../src/orion/unified_platform/resource/socket/socket.h"
#include "../../../../../../../src/orion/common/sal.h"
 
#include "buffer.h"
#include "dev_buffer.h"
#include "local_ub_rma_buffer.h"
#include "socket_handle_manager.h"
#include "base_config.h"
#include "../../../../../../../src/orion/framework/env_config/env_config.h"
#include <initializer_list>
#include "../../../../../../../src/whole/hccl_next/framework/topo/new_topo_builder/topo_info/topo_info.h"
#include "rank_graph_builder.h"
#include "../../../../../../../src/orion/unified_platform/external_system/orion_adapter_rts.h"
// #include "../../../../../../src/whole/hccl_next/framework/topo/new_topo_builder/rank_graph/net_instance.h"
#include "../../../../../../../src/orion/framework/resource_manager/socket/host_socket_handle_manager.h"
 
namespace Hccl {

void *HrtMalloc(u64 size, aclrtMemType_t memType)
{
    return (void*)0x12345678;
}
 
RdmaHandleManager::RdmaHandleManager()
{
}
 
RdmaHandleManager::~RdmaHandleManager()
{
}
 
RdmaHandle RdmaHandleManager::GetByIp(u32 devPhyId, const IpAddress &localIp)
{
    return (void*)0x12345678;
}
 
RdmaHandleManager &RdmaHandleManager::GetInstance()
{
    static RdmaHandleManager rdmaHandleManager;
    return rdmaHandleManager;
}
 
JfcHandle RdmaHandleManager::GetJfcHandle(RdmaHandle rdmaHandle, HrtUbJfcMode jfcMode)
{
    return 0x12345678;
}
 
std::pair<TokenIdHandle, uint32_t> RdmaHandleManager::GetTokenIdInfo(RdmaHandle rdmaHandle, const BufferKey<uintptr_t, u64> &bufKey)
{
    return {0x12345678, 12345678};
}
 
SocketStatus Socket::GetAsyncStatus()
{
    return SocketStatus::OK;
}
 
void Socket::ConnectAsync()
{
    if (role == SocketRole::SERVER) {
        std::cout << "Socket Server, connect async passed." << std::endl;
        return;
    }
 
    std::cout << "Socket client, connect async do." << std::endl;
    return;
}
 
void Socket::SendAsync(const u8 *sendBuf, u32 size)
{
    return;
}
 
void Socket::RecvAsync(u8 *recvBuf, u32 size)
{
    return;
}
 
void Socket::Listen()
{
    std::cout << "Socket Server, listen." << std::endl;
}
 
void Socket::Connect()
{
}
 
Socket::~Socket()
{
}
 
std::size_t HashCombine(std::initializer_list<std::size_t> hashItem)
{
    std::size_t res     = 17;
    std::size_t padding = 31;
    for (auto begin = hashItem.begin(); begin != hashItem.end(); ++begin) {
        res = padding * res + (*begin);
    }
    return res;
}
 
Buffer::Buffer(std::size_t size) : size_(size)
{
    addr_ = 0x12345678;
}
 
Buffer::Buffer(uintptr_t addr, std::size_t size) : addr_(addr), size_(size)
{
}
 
std::string Buffer::Describe() const
{
    return "";
}
 
uintptr_t Buffer::GetAddr() const
{
    return addr_;
}
 
size_t Buffer::GetSize() const
{
    return size_;
}
 
DevBuffer::DevBuffer(uintptr_t devAddr, std::size_t devSize) : Buffer(devSize), selfOwned(false)
{
    addr_ = devAddr;
    size_ = devSize;
}
 
DevBuffer::DevBuffer(std::size_t allocSize) : Buffer(allocSize), selfOwned(true)
{
    addr_ = (uintptr_t)(0x12345678);
}
 
std::shared_ptr<DevBuffer> DevBuffer::Create(uintptr_t devAddr, std::size_t devSize)
{
    if (devAddr == 0 || devSize == 0) {
        return nullptr;
    }
    return std::shared_ptr<DevBuffer>(new (std::nothrow) DevBuffer(devAddr, devSize));
}
 
DevBuffer::DevBuffer(std::size_t allocSize, std::uint32_t policy, PolicyTag /*tag*/) : Buffer(allocSize), selfOwned(true)
{
    addr_ = 0x12345678;
}
 
DevBuffer::~DevBuffer()
{
}
 
std::string DevBuffer::Describe() const
{
    return "";
}
 
LocalUbRmaBuffer::LocalUbRmaBuffer(std::shared_ptr<Buffer> buf) : LocalRmaBuffer(buf, RmaType::UB)
{
}
 
LocalUbRmaBuffer::LocalUbRmaBuffer(std::shared_ptr<Buffer> buf, RdmaHandle rdmaHandle) : LocalRmaBuffer(buf, RmaType::UB)
{
}
 
LocalUbRmaBuffer::~LocalUbRmaBuffer()
{
}
 
std::unique_ptr<Serializable> LocalUbRmaBuffer::GetExchangeDto()
{
    return nullptr;
}
 
std::string LocalUbRmaBuffer::Describe() const {
    return "";
}
 
u32 LocalUbRmaBuffer::GetTokenId() const
{
    return 0;
}
 
u32 LocalUbRmaBuffer::GetTokenValue() const
{
    return 0;
}
 
TokenIdHandle LocalUbRmaBuffer::GetTokenIdHandle() const
{
    return 0x12345678;
}
 
u32 GetUbToken()
{
    return 0;
}
 
void SaluSleep(uint32_t usec)
{
    return;
}
 
SocketHandleManager::SocketHandleManager()
{
}
 
SocketHandleManager::~SocketHandleManager()
{
}
 
SocketHandleManager& SocketHandleManager::GetInstance()
{
    static SocketHandleManager mgr;
    return mgr;
}
 
SocketHandle SocketHandleManager::Create(DevId devicePhyId, const PortData &localPort)
{
    int a = 0x12345678;
    return (void*)&a;
}
 
std::string EnvTopoFilePathConfig::GetTopoFilePath() const
{
    return "fake topo path";
}
 
u32 RankGraph::GetInnerRankSize() const
{
    return 2;
}
 
u32 RankGraph::GetRankSize() const
{
    return 2;
}
 
EnvConfig &EnvConfig::GetInstance()
{
    // static EnvConfig envConfig;
    return *((EnvConfig *)0x12345678);
}
 
std::shared_ptr<TopoInfo> RankGraphBuilder::GetTopoInfo()
{
    return nullptr;
}
 
const EnvTopoFilePathConfig &EnvConfig::GetTopoFilePathConfig()
{
    // return topoFilePathCfg;
    return *((EnvTopoFilePathConfig *)0x12345678);
}
 
unique_ptr<RankGraph> RankGraphBuilder::Build(const string &ranktableM, const string &topoPath, RankId myRank)
{
    return nullptr;
}
 
std::unique_ptr<RankTableInfo> RankGraphBuilder::GetRankTableInfo()
{
    return nullptr;
}
 
shared_ptr<NetInstance::ConnInterface> NetInstance::Link::GetSourceIface() const
{
    return nullptr;
}
 
std::set<LinkProtocol> NetInstance::Link::GetLinkProtocols() const
{
    return {};
}
 
shared_ptr<NetInstance::ConnInterface> NetInstance::Link::GetTargetIface() const
{
    return nullptr;
}
 
IpAddress NetInstance::ConnInterface::GetAddr() const
{
    return {};
}
 
vector<NetInstance::Path> RankGraph::GetPaths(u32 netLayer, RankId sRankId, RankId dRankId) const
{
    return {};
}

}
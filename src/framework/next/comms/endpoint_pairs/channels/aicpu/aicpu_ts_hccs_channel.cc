/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "aicpu_ts_hccs_channel.h"
#include "../../../endpoints/endpoint.h"
#include "../../../endpoints/aicputs_hccs_endpoint.h"
#include "../../../endpoints/net_dev/global_net_dev_manager.h"
#include "channel_param.h"
#include "inner/remote_ipc_rma_buffer.h"
#include "inner/local_ipc_rma_buffer.h"
#include "hcomm_c_adpt.h"
#include "hccl_socket_manager.h"
#include "externalinput_pub.h"
// for hccl_network.h
#include "inner/local_rdma_rma_buffer.h"
#include "inner/remote_rdma_rma_buffer.h"
#include "hccl_network.h"

using LocalIpcRmaBufferMgr = hccl::RmaBufferMgr<hccl::BufferKey<uintptr_t, u64>, std::shared_ptr<hccl::LocalIpcRmaBuffer>>;
using RemoteIpcRmaBufferMgr = hccl::RmaBufferMgr<hccl::BufferKey<uintptr_t, u64>, std::shared_ptr<hccl::RemoteIpcRmaBuffer>>;

using namespace hccl;

namespace hcomm {
AicpuTsHccsChannel::AicpuTsHccsChannel(EndpointHandle endpointHandle, const HcommChannelDesc &channelDesc):
    endpointHandle_(endpointHandle), channelDesc_(channelDesc) 
{
    myId_ = ++allId_;
    HCCL_INFO("[AicpuTsHccsChannel][AicpuTsHccsChannel] myID[%lu]", myId_);
}

AicpuTsHccsChannel::~AicpuTsHccsChannel()
{
    if (dispatcher_ != nullptr) {
        (void)HcclDispatcherDestroy(dispatcher_);
        dispatcher_ = NULL;
    }
    GlobalNetDevMgr::GetInstance(localEp_.loc.device.devPhyId).CloseSocket(socket_);
}

HcclResult AicpuTsHccsChannel::ParseInputParam()
{
    // 1. 从 endpointHandle_，获得 localEp_     
    CHK_RET(static_cast<HcclResult>(HcommEndpointGet(endpointHandle_, reinterpret_cast<void**>(&localEpPtr_))));
    CHK_PTR_NULL(localEpPtr_);
    
    netDevCtx_ = localEpPtr_->GetNetDevCtx();
    localEp_ = localEpPtr_->GetEndpointDesc();

    // 2. 从 channelDesc_，获得 remoteEp_, socket_ 和 notifyNum
    remoteEp_ = channelDesc_.remoteEndpoint;
    notifyNum_ = channelDesc_.notifyNum;

    CHK_RET(GetFirstIpByPhyId(localEp_.loc.device.devPhyId, localEp_.loc.device.superDevId, localIp_));
    CHK_RET(GetFirstIpByPhyId(remoteEp_.loc.device.devPhyId, remoteEp_.loc.device.superDevId, remoteIp_));
    std::string localReadableAddress = localIp_.GetReadableAddress();
    std::string remoteReadableAddress = remoteIp_.GetReadableAddress();
    if (localReadableAddress < remoteReadableAddress) {
        isSocketServer_ = true;
    }
    HCCL_INFO("[AicpuTsHccsChannel][ParseInputParam] local devPhyId [%u] ip[%u] remote devPhyId[%u] ip[%s], isSocketServer_[%u], myID[%lu]",
        localEp_.loc.device.devPhyId, localReadableAddress.c_str(),
        remoteEp_.loc.device.devPhyId, remoteReadableAddress.c_str(),
        static_cast<u32>(isSocketServer_), localEpPtr_->GetMyId());

    return HCCL_SUCCESS;
}

HcclResult AicpuTsHccsChannel::GetFirstIpByPhyId(u32 devicePhyId, u32 superDevId, HcclIpAddress &ip)
{
    CHK_RET(GlobalNetDevMgr::GetDeviceVnicIP(devicePhyId, superDevId, ip));
    HCCL_INFO("[AicpuTsHccsChannel][GetFirstIpByPhyId]devicePhyId[%u] superDevId[%u] linkInfo.ip[%s]",
        devicePhyId, superDevId, ip.GetReadableAddress());
    return HCCL_SUCCESS;
}

HcclResult AicpuTsHccsChannel::BuildConnection()
{
    std::string localReadableAddress = localIp_.GetReadableAddress();
    std::string remoteReadableAddress = remoteIp_.GetReadableAddress();

    HCCL_INFO("[AicpuTsHccsChannel][BuildConnection] local devPhyId [%u] ip[%u] remote devPhyId[%u] ip[%s]",
        localEp_.loc.device.devPhyId, localReadableAddress.c_str(),
        remoteEp_.loc.device.devPhyId, remoteReadableAddress.c_str());

    u32 server_port = channelDesc_.port != 0 ?  channelDesc_.port : AICPU_CHANNEL_DEFUALT_PORT;
    if (isSocketServer_) {
        GlobalNetDevMgr::MakeSocketTag(localIp_, server_port, remoteIp_, socketTag_, GetMyId());
        CHK_RET(GlobalNetDevMgr::GetInstance(localEp_.loc.device.devPhyId).AcceptClient(
            netDevCtx_, remoteIp_, socketTag_, socket_));
    } else {
        GlobalNetDevMgr::MakeSocketTag(remoteIp_, server_port, localIp_, socketTag_, GetMyId());
        CHK_RET(GlobalNetDevMgr::GetInstance(localEp_.loc.device.devPhyId).ConnectToServer(
            netDevCtx_, remoteIp_, server_port, socketTag_, socket_));
    }
    HCCL_INFO("[AicpuTsHccsChannel][BuildConnection] local devPhyId [%u] ip[%u] remote devPhyId[%u] ip[%s] socketTag_[%s]",
        localEp_.loc.device.devPhyId, localReadableAddress.c_str(),
        remoteEp_.loc.device.devPhyId, remoteReadableAddress.c_str(), socketTag_.c_str());
    return HCCL_SUCCESS;
}

HcclResult AicpuTsHccsChannel::SetMachinePara(hccl::MachinePara &machinePara)
{
    CHK_RET(hrtGetDeviceType(machinePara.deviceType));

    u32 deviceLogicId;
    CHK_RET(hrtGetDeviceIndexByPhyId(localEp_.loc.device.devPhyId, deviceLogicId));
    machinePara.deviceLogicId = static_cast<s32>(deviceLogicId);
    machinePara.tag = socketTag_;
    machinePara.notifyNum = channelDesc_.notifyNum;
    machinePara.linkMode = hccl::LinkMode::LINK_DUPLEX_MODE;;
    machinePara.specifyLink = LinkTypeInServer::RESERVED_LINK_TYPE;
    machinePara.machineType = isSocketServer_ ? hccl::MachineType::MACHINE_SERVER_TYPE : hccl::MachineType::MACHINE_CLIENT_TYPE;
    machinePara.serverId = localEp_.loc.device.serverIdx;
    machinePara.localDeviceId = localEp_.loc.device.devPhyId;
    machinePara.remoteDeviceId = remoteEp_.loc.device.devPhyId;
    machinePara.localIpAddr = socket_->GetLocalIp();
    machinePara.remoteIpAddr = socket_->GetRemoteIp();
    machinePara.localSocketPort = socket_->GetLocalPort();
    machinePara.remoteSocketPort = socket_->GetRemotePort();
    machinePara.srcPorts = std::vector<std::uint16_t>(1, 0); /* 默认填充一个元素，0代表默认不配置 */
    machinePara.mem.clear();
    machinePara.linkAttribute = 0x03; /* 0x03同时支持目的端和源端发起 */
    machinePara.sockets.push_back(socket_);
    machinePara.exchangeInfo.resize(sizeof(HccsExchangeInfo));
    machinePara.isNewOndeSide = true;
    return HCCL_SUCCESS;
}

void AicpuTsHccsChannel::SetTransportParam(hccl::TransportPara &para)
{
    std::chrono::milliseconds kdefaultTimeout = std::chrono::seconds(GetExternalInputHcclLinkTimeOut());
    para.timeout = kdefaultTimeout;
    para.transportResourceInfoAddr = nullptr;
    para.transportResourceInfoSize = 0;
    para.virtualFlag = false;
}

HcclResult AicpuTsHccsChannel::TransportInit()
{
    hccl::MachinePara machinePara;
    CHK_RET(SetMachinePara(machinePara));

    hccl::TransportPara para{};
    SetTransportParam(para);

    CHK_RET(HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, localEp_.loc.device.devPhyId, &dispatcher_));
    CHK_SMART_PTR_NULL(dispatcher_);

    if (!FindDispatcherByCommId(&dispatcherCtx_, DEFAULT_DISPATCH_NAME)) {
        CHK_RET(CreateDispatcherCtx(&dispatcherCtx_, localEp_.loc.device.devPhyId, DEFAULT_DISPATCH_NAME));
    }
    CHK_PTR_NULL(dispatcherCtx_);

    notifyPool_.reset(new (std::nothrow) hccl::NotifyPool());
    CHK_SMART_PTR_NULL(notifyPool_);
    CHK_RET(notifyPool_->Init(localEp_.loc.device.devPhyId));
    CHK_RET(notifyPool_->RegisterOp(machinePara.tag));

    // 实例化TransportBase
    transport_.reset(new (std::nothrow) Transport(TransportType::TRANS_TYPE_P2P, para,
        dispatcher_, notifyPool_, machinePara));

    CHK_RET(transport_->Init());

    return HCCL_SUCCESS;
}

HcclResult AicpuTsHccsChannel::Init()
{  
    CHK_RET(ParseInputParam());
    CHK_RET(BuildConnection());
    CHK_RET(TransportInit());
    return HCCL_SUCCESS;
}

HcclResult AicpuTsHccsChannel::GetRemoteMem(HcclMem **remoteMem, uint32_t *memNum, char **memTags)
{
    remoteIpcRmaBufferVec_.clear();
    CHK_RET(localEpPtr_->GetRemoteIpcRmaBuffer(remoteIpcRmaBufferVec_));
    *remoteMem = remoteIpcRmaBufferVec_.data();
    *memNum = remoteIpcRmaBufferVec_.size();
    return HCCL_SUCCESS;
}

ChannelStatus AicpuTsHccsChannel::GetStatus()
{
    ChannelStatus out = ChannelStatus::READY;
    return out;
}

HcclResult AicpuTsHccsChannel::GetNotifyNum(uint32_t *notifyNum) const
{
    *notifyNum = notifyNum_;
    return HCCL_SUCCESS;
}

HcclResult AicpuTsHccsChannel::BuildHcclChannelHccsRes(HcclChannelHccsRes &channelHccsRes)
{
    HcclChannelP2p &linkp2p = channelHccsRes.channelP2p;

    CHK_SAFETY_FUNC_RET(memcpy_s(channelHccsRes.channelTag, sizeof(channelHccsRes.channelTag) - 1,
        socketTag_.c_str(), socketTag_.length()));
    HCCL_DEBUG("[AicpuTsHccsChannel][%s] channelHccsRes.channelTag[%s]", __func__, channelHccsRes.channelTag);

    linkp2p.remoteHcclbuffer.addr = nullptr;
    linkp2p.remoteHcclbuffer.size = 0;
    linkp2p.remoteUserMem = nullptr;
    linkp2p.remoteUserMemCount = 0;

    HCCL_DEBUG("[AicpuTsHccsChannel][%s] finish set remoteMem info", __func__);

    // localnotify & remotenotify
    u64 notifyNum = 0;
    channelHccsRes.p2pNotifyNum = transport_->GetNotifyNum();
    HCCL_DEBUG("[AicpuTsHccsChannel][%s] finish set localnotify & remotenotify info, notifyNum[%llu], p2pNotifyNum[%llu]",
        __func__, notifyNum, channelHccsRes.p2pNotifyNum);
    // transportAttr
    CHK_RET(transport_->GetTransportAttr(linkp2p.transportAttr));

    // for device init
    DevType devType;
    CHK_RET(hrtGetDeviceType(devType));
    channelHccsRes.deviceType = static_cast<u32>(devType);
    channelHccsRes.remoteDevicePhyId = remoteEp_.loc.device.devPhyId;
    channelHccsRes.localDevicePhyId = localEp_.loc.device.devPhyId;
    channelHccsRes.machineType = isSocketServer_ ? hccl::MachineType::MACHINE_SERVER_TYPE : hccl::MachineType::MACHINE_CLIENT_TYPE;
    u32 deviceLogicId;
    CHK_RET(hrtGetDeviceIndexByPhyId(localEp_.loc.device.devPhyId, deviceLogicId));
    channelHccsRes.localDeviceLogicId = static_cast<s32>(deviceLogicId);

    remoteIpcRmaBufferVecEx_.clear();
    CHK_RET(localEpPtr_->GetRemoteIpcRmaBufferEx(remoteIpcRmaBufferVecEx_));
    channelHccsRes.remoteBufSize = remoteIpcRmaBufferVecEx_.size();
    channelHccsRes.remoteBufMem = remoteIpcRmaBufferVecEx_.data();

    localIpcRmaBufferVecEx_.clear();
    CHK_RET(localEpPtr_->GetLocalIpcRmaBufferEx(localIpcRmaBufferVecEx_));
    channelHccsRes.localBufSize = localIpcRmaBufferVecEx_.size();
    channelHccsRes.localBufMem = localIpcRmaBufferVecEx_.data();

    HCCL_DEBUG("[AicpuTsHccsChannel][%s] finish set RemoteChannelP2pResParam info", __func__);
    return HCCL_SUCCESS;
}

HcclResult AicpuTsHccsChannel::PackToHccsRes(HcclChannelHccsRes &channelHccsRes)
{
    CHK_RET(BuildHcclChannelHccsRes(channelHccsRes));
    return HCCL_SUCCESS;
}

HcclResult AicpuTsHccsChannel::Clean()
{
    return HCCL_SUCCESS;
}

HcclResult AicpuTsHccsChannel::Resume()
{
    return HCCL_SUCCESS;
}
}
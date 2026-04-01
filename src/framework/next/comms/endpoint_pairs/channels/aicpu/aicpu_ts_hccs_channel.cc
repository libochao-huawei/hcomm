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

using namespace hccl;

namespace hcomm {
AicpuTsHccsChannel::AicpuTsHccsChannel(EndpointHandle endpointHandle, const HcommChannelDesc &channelDesc):
    endpointHandle_(endpointHandle), channelDesc_(channelDesc) {}

AicpuTsHccsChannel::~AicpuTsHccsChannel()
{
    if (dispatcher_ != nullptr) {
        (void)HcclDispatcherDestroy(dispatcher_);
        dispatcher_ = NULL;
    }
}

void AicpuTsHccsChannel::ConstructTransTag(std::string& transTag)
{
    transTag = "_HccsChannel_" +
        std::to_string(localEp_.loc.device.superDevId) + "_" + std::to_string(localEp_.loc.device.devPhyId) + "_" +
        std::to_string(remoteEp_.loc.device.superDevId) + "_" + std::to_string(remoteEp_.loc.device.devPhyId);
}

void AicpuTsHccsChannel::GetUserRank(bool beLocal, uint32_t &userRank)
{
    uint32_t localUserRank = 0;
    uint32_t remoteUserRank = 0;

    // because socketmanager.userRank_ default is 0, and socketmanager.userRank_ < remoteRank, socket role will be server
    if (localEp_.loc.device.superDevId == 0 && remoteEp_.loc.device.superDevId == 0) {
        if (localEp_.loc.device.devPhyId < remoteEp_.loc.device.devPhyId) {
            localUserRank = 0;
            remoteUserRank = 1;
        } else {
            localUserRank = 1;
            remoteUserRank = 0;
        }
    }
    if (localEp_.loc.device.superDevId == 0) {
        localUserRank = 0;
        remoteUserRank = 1;
    } else {
        localUserRank = 1;
        remoteUserRank = 0;
    }
    userRank = beLocal ? localUserRank : remoteUserRank;
}

HcclResult AicpuTsHccsChannel::ParseInputParam()
{
    // 1. 从 endpointHandle_，获得 localEp_     
    AicpuTsHccsEndPoint* localEpPtr{nullptr};
    CHK_RET(HcommEndpointGet(endpointHandle_, (void **)&localEpPtr));
    CHK_PTR_NULL(localEpPtr);
    netDevCtx_ = localEpPtr->GetNetDevCtx();
    localEp_ = localEpPtr->GetEndpointDesc();

    // 2. 从 channelDesc_，获得 remoteEp_, socket_ 和 notifyNum
    remoteEp_ = channelDesc_.remoteEndpoint;

    notifyNum_ = channelDesc_.notifyNum;

    ConstructTransTag(connTag_);

    if (channelDesc_.exchangeAllMems) {
        // 3. Get memHandles from endpoint
        std::shared_ptr<hccl::LocalIpcRmaBuffer> *memHandles = nullptr;
        uint32_t memHandleNum = 0;
        HCCL_INFO("[AicpuTsHccsChannel][%s] exchangeAllMems == True. Get memHandles from endpoint.", __func__);
        CHK_RET(HcommMemGetAllMemHandles(endpointHandle_, reinterpret_cast<void**>(&memHandles), &memHandleNum));
        for (uint32_t i = 0; i < memHandleNum; ++i) {
            std::shared_ptr<hccl::LocalIpcRmaBuffer> &localIpcRmaBuffer = memHandles[i];
            HCCL_INFO("[AicpuTsHccsChannel][%s] Got memHandle No.%u: addr[0x%llx], size[0x%llx].",
                __func__, i, localIpcRmaBuffer->GetAddr(), localIpcRmaBuffer->GetSize());
            bufs_.emplace_back(std::move(std::make_shared<Hccl::Buffer>(
                reinterpret_cast<uintptr_t>(localIpcRmaBuffer->GetAddr()), localIpcRmaBuffer->GetSize(), "")
            ));
        }
    } else {
        // 3. 从 channelDesc 的 memHandle，获得 bufs_
        HCCL_INFO("[AicpuTsHccsChannel][%s] exchangeAllMems == false. Get memHandles from channelDesc.", __func__);
        for (uint32_t i = 0; i < channelDesc_.memHandleNum; ++i) {
            auto *localIpcRmaBuffer = reinterpret_cast<hccl::LocalIpcRmaBuffer *>(channelDesc_.memHandles[i]);
            HCCL_INFO("[AicpuTsHccsChannel][%s] Got memHandle No.%u: addr[0x%llx], size[0x%llx].",
                __func__, i, localIpcRmaBuffer->GetAddr(), localIpcRmaBuffer->GetSize());
            bufs_.emplace_back(std::move(std::make_shared<Hccl::Buffer>(
                reinterpret_cast<uintptr_t>(localIpcRmaBuffer->GetAddr()), localIpcRmaBuffer->GetSize(), "")
            ));
        }
    }

    return HCCL_SUCCESS;
}

#define AICPU_CHANNEL_DEFUALT_PORT 16666
HcclResult AicpuTsHccsChannel::MakeLinkInfo(bool beLocal, EndpointDesc &endpointDesc, hccl::HcclRankLinkInfo &linkInfo)
{
    GetUserRank(beLocal, linkInfo.userRank);

    linkInfo.devicePhyId = endpointDesc.loc.device.devPhyId;
/*
    if (endpointDesc.loc.device.superDevId != 0) {
        CHK_RET(hrtRaGetSingleSocketVnicIpInfo(endpointDesc.loc.device.devPhyId,
            DeviceIdType::DEVICE_ID_TYPE_SDID,
            endpointDesc.loc.device.superDevId,
            linkInfo.ip));
    } else {
        CHK_RET(hrtRaGetSingleSocketVnicIpInfo(endpointDesc.loc.device.devPhyId,
            DeviceIdType::DEVICE_ID_TYPE_PHY_ID,
            endpointDesc.loc.device.devPhyId,
            linkInfo.ip));
    }
*/

    std::vector<HcclIpAddress> deviceIp;
    CHK_RET(GlobalNetDevMgr::GetInstance(linkInfo.devicePhyId).GetDeviceIP(linkInfo.devicePhyId, deviceIp));
    linkInfo.ip = deviceIp[0];
    HCCL_INFO("[AicpuTsHccsChannel][MakeLinkInfo]devicePhysicID[%u] linkInfo.ip[%s]",
        linkInfo.devicePhyId, linkInfo.ip.GetReadableAddress());

    if (!beLocal) {
        linkInfo.port = channelDesc_.port != 0 ? channelDesc_.port : AICPU_CHANNEL_DEFUALT_PORT;
    } else {
        linkInfo.port = 0;
    }
    linkInfo.socketsPerLink = 1;

    HCCL_INFO("[AicpuTsHccsChannel][MakeLinkInfo]link info:userRank[%u], devPhyId[%u], ip[%s], port[%u], socketsPerLink[%u]",
        linkInfo.userRank, linkInfo.devicePhyId, linkInfo.ip.GetReadableAddress(), linkInfo.port, linkInfo.socketsPerLink);
    return HCCL_SUCCESS;
}

HcclResult AicpuTsHccsChannel::BuildConnection()
{
    // 创建socket用于交换数据
    // just for reuse socketManager->CreateSingleLinkSocket
    hccl::HcclRankLinkInfo remoteRankLinkInfo;
    CHK_RET(MakeLinkInfo(false, remoteEp_, remoteRankLinkInfo));

    std::shared_ptr<hccl::HcclSocketManager> socketManager =
        GlobalNetDevMgr::GetInstance(localEp_.loc.device.devPhyId).GetSocketManager();
    CHK_PTR_NULL(socketManager);
    CHK_RET(socketManager->CreateSingleLinkSocket(connTag_, netDevCtx_, remoteRankLinkInfo, socket_, false, true));
    return HCCL_SUCCESS;
}

#define HCCS_CHANNEL_MAX_EXCHANGE_DATA_LEN (1024U * 4)
HcclResult AicpuTsHccsChannel::SetMachinePara(hccl::HcclRankLinkInfo &localRankLinkInfo,
    hccl::HcclRankLinkInfo &remoteRankLinkInfo, hccl::MachinePara &machinePara)
{
    CHK_RET(hrtGetDeviceType(machinePara.deviceType));

    u32 deviceLogicId;
    CHK_RET(hrtGetDeviceIndexByPhyId(localEp_.loc.device.devPhyId, deviceLogicId));
    machinePara.deviceLogicId = static_cast<s32>(deviceLogicId);
    machinePara.tag = connTag_;
    machinePara.notifyNum = channelDesc_.notifyNum;
    machinePara.linkMode = hccl::LinkMode::LINK_DUPLEX_MODE;;
    machinePara.machineType = hccl::MachineType::MACHINE_CLIENT_TYPE;
    machinePara.serverId = localEp_.loc.device.serverIdx;

    machinePara.localSocketPort = 0;
    machinePara.remoteSocketPort = 0;

    machinePara.srcPorts = std::vector<std::uint16_t>(1, 0); /* 默认填充一个元素，0代表默认不配置 */

    machinePara.mem.clear();
    machinePara.linkAttribute = 0x03; /* 0x03同时支持目的端和源端发起 */

    machinePara.localIpAddr = localRankLinkInfo.ip;
    machinePara.remoteIpAddr = remoteRankLinkInfo.ip;
    machinePara.localDeviceId = localRankLinkInfo.devicePhyId;
    machinePara.remoteDeviceId = remoteRankLinkInfo.devicePhyId;

    // 把原来的两层vector变成一层, 方便后继调用
    if (socket_.size() > 0) {
        std::map<u32, std::vector<std::shared_ptr<HcclSocket>>> socketsMap;
        socketsMap[remoteRankLinkInfo.userRank] = socket_;

        std::map<u32, u32> dstRankToUserRank;
        hccl::RankInfo localRank;
        hccl::RankInfo remoteRank;
        std::shared_ptr<hccl::HcclSocketManager> socketManager = GlobalNetDevMgr::GetInstance().GetSocketManager();
        CHK_PTR_NULL(socketManager);
        CHK_RET(socketManager->WaitLinksEstablishCompleted(socket_[0]->GetLocalRole(),
            socketsMap, dstRankToUserRank, localRank, remoteRank, netDevCtx_));
        machinePara.sockets = socket_;
    }

    machinePara.exchangeInfo.resize(HCCS_CHANNEL_MAX_EXCHANGE_DATA_LEN);
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
    hccl::HcclRankLinkInfo localRankLinkInfo{};
    hccl::HcclRankLinkInfo remoteRankLinkInfo{};
    CHK_RET(MakeLinkInfo(true, localEp_, localRankLinkInfo));
    CHK_RET(MakeLinkInfo(false, remoteEp_, remoteRankLinkInfo));

    hccl::MachinePara machinePara;
    CHK_RET(SetMachinePara(localRankLinkInfo, remoteRankLinkInfo, machinePara));

    hccl::TransportPara para{};
    SetTransportParam(para);

    CHK_RET(HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, localEp_.loc.device.devPhyId, &dispatcher_));
    CHK_SMART_PTR_NULL(dispatcher_);

    notifyPool_.reset(new (std::nothrow) hccl::NotifyPool());
    CHK_SMART_PTR_NULL(notifyPool_);
    CHK_RET(notifyPool_->Init(localEp_.loc.device.devPhyId));

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
    *memNum = 0;
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

HcclResult AicpuTsHccsChannel::CheckNotifyOrQPMaxNum(u64 &existNum, const u64 &MaxNum, const bool &isNotifyRes)
{
    std::string resType = isNotifyRes ? "Notify" : "QP";
    if (existNum + 1 > MaxNum) {
        HCCL_ERROR("[%s]%s resources are insufficient, existNum[%llu], MaxNum is [%llu]",
            __func__, resType.c_str(), existNum, MaxNum);
        return HCCL_E_INTERNAL;
    }
    HCCL_DEBUG("[%s]%s resources are sufficient, existNum[%llu], MaxNum is [%llu]",
            __func__, resType.c_str(), existNum, MaxNum);
    return HCCL_SUCCESS;
}

HcclResult AicpuTsHccsChannel::BuildHcclChannelHccsRes(HcclChannelHccsRes &channelHccsRes)
{
    HcclChannelP2p &linkp2p = channelHccsRes.channelP2p;
    // remoteMem, 独立算子localmem是否需要传待确认
    void *bufferPtr = nullptr;
    CHK_RET(transport_->GetRemoteMem(UserMemType::INPUT_MEM, &bufferPtr));
    linkp2p.remoteHcclbuffer.addr = reinterpret_cast<void*>(bufferPtr);
    u64 remotebufferSize;
    CHK_RET(transport_->GetRemoteMemSize(UserMemType::INPUT_MEM, remotebufferSize));
    linkp2p.remoteHcclbuffer.size = remotebufferSize;
    // 独立算子远端用户内存，linkp2p.remoteUserMem需要手动释放内存
    CHK_RET(transport_->GetIndOpRemoteMem(&linkp2p.remoteUserMem, &linkp2p.remoteUserMemCount));
    HCCL_DEBUG("[AicpuTsHccsChannel][%s] finish set remoteMem info", __func__);

    // localnotify & remotenotify
    u64 notifyNum = 0;
    std::vector<HcclSignalInfo> locIpcSignals;
    std::vector<HcclSignalInfo> rmtIpcSignals;
    CHK_RET(transport_->GetLocalNotify(locIpcSignals));
    CHK_RET(transport_->GetRemoteNotify(rmtIpcSignals));

    for (size_t i = 0; i < locIpcSignals.size(); i++) {
        CHK_RET(CheckNotifyOrQPMaxNum(notifyNum, LINK_P2P_MAX_NUM, true));
        linkp2p.localIpcSignal[notifyNum] = locIpcSignals[i];
        linkp2p.remoteIpcSignal[notifyNum] = rmtIpcSignals[i];
        notifyNum++;
    }
    channelHccsRes.p2pNotifyNum = transport_->GetNotifyNum();
    HCCL_DEBUG("[AicpuTsHccsChannel][%s] finish set localnotify & remotenotify info, notifyNum[%llu], p2pNotifyNum[%llu]",
        __func__, notifyNum, channelHccsRes.p2pNotifyNum);
    // transportAttr
    CHK_RET(transport_->GetTransportAttr(linkp2p.transportAttr));

    // for device init
    DevType devType;
    CHK_RET(hrtGetDeviceType(devType));
    channelHccsRes.deviceType = static_cast<u32>(devType);

    GetUserRank(true, channelHccsRes.localRank);
    GetUserRank(false, channelHccsRes.remoteRank);

    channelHccsRes.remoteDevicePhyId = remoteEp_.loc.device.devPhyId;
    channelHccsRes.localDevicePhyId = localEp_.loc.device.devPhyId;

    u32 deviceLogicId;
    CHK_RET(hrtGetDeviceIndexByPhyId(localEp_.loc.device.devPhyId, deviceLogicId));
    channelHccsRes.localDeviceLogicId = static_cast<s32>(deviceLogicId);
    HCCL_DEBUG("[AicpuTsHccsChannel][%s] finish set RemoteChannelP2pResParam info", __func__);
    return HCCL_SUCCESS;
}

HcclResult AicpuTsHccsChannel::H2DResPack(HcclChannelHccsRes &channelHccsRes)
{
    CHK_RET(BuildHcclChannelHccsRes(channelHccsRes));
    return HCCL_SUCCESS;
}
}
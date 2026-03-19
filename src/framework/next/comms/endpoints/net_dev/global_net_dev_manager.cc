/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
 
#include "global_net_dev_manager.h"
#include <string>
#include "adapter_pub.h"
#include "hccl_mem.h"
// for hccl_network.h
#include "inner/remote_ipc_rma_buffer.h"
#include "inner/local_ipc_rma_buffer.h"
#include "inner/local_rdma_rma_buffer.h"
#include "inner/remote_rdma_rma_buffer.h"
#include "hccl_network.h"
#include "network_manager_pub.h"
#include "dlhal_function.h"
#include "dlra_function.h"

using namespace hccl;

namespace hccl {
std::map<PortInfo, std::pair<NicType, HcclNetDevCtx>> GlobalNetDevMgr::netDevCtxMap_;
std::map<PortInfo, Referenced> GlobalNetDevMgr::netDevCtxRefMap_;
std::mutex GlobalNetDevMgr::netDevCtxMtx_;
bool GlobalNetDevMgr::isDlRaInited_{false};
std::unordered_map<uint32_t, std::shared_ptr<hccl::HcclSocket>> GlobalNetDevMgr::serverSocketMap_;

static GlobalNetDevMgr netDevMgrInstance[MAX_MODULE_DEVICE_NUM + 1];

GlobalNetDevMgr::~GlobalNetDevMgr()
{
    HCCL_INFO("[GlobalNetDevMgr][%s] start.", __func__);
    if (isInited_) {
        UnInit();
    }
    HCCL_INFO("[GlobalNetDevMgr][%s] end.", __func__);
}

GlobalNetDevMgr& GlobalNetDevMgr::GetInstance(u32 devicePhyId)
{
    u32 deviceLogicId = 0;
    HcclResult hcclRet = hrtGetDeviceIndexByPhyId(devicePhyId, deviceLogicId);
    if (hcclRet != HCCL_SUCCESS) {
        HCCL_RUN_WARNING("GlobalNetDevMgr::GetInstance hrtGetDeviceRefresh failed, ret[%d], "
            "return reserve instance", hcclRet);
        return netDevMgrInstance[MAX_MODULE_DEVICE_NUM];
    }

    if (static_cast<u32>(deviceLogicId) >= MAX_MODULE_DEVICE_NUM) {
        HCCL_RUN_WARNING("[Get][Instance]deviceLogicId[%d] is invalid, return reserve instance", deviceLogicId);
        return netDevMgrInstance[MAX_MODULE_DEVICE_NUM];
    }

    if (!netDevMgrInstance[deviceLogicId].isInited_) {
        hcclRet = Init(devicePhyId, deviceLogicId);
        if (hcclRet != HCCL_SUCCESS) {
            HCCL_RUN_WARNING("[Get][Instance]Init deviceLogicId[%d]fail, return reserve instance", deviceLogicId);
            return netDevMgrInstance[MAX_MODULE_DEVICE_NUM];
        }
    }

    HCCL_INFO("GlobalNetDevMgr::GetInstance deviceLogicId[%u].", deviceLogicId);
    return netDevMgrInstance[deviceLogicId];
}

HcclResult GlobalNetDevMgr::Init(u32 devicePhyId, u32 deviceLogicId)
{
    if (netDevMgrInstance[deviceLogicId].isInited_) {
        return HCCL_SUCCESS;
    }

    // init after get the lock
    std::unique_lock<std::mutex> lock(netDevCtxMtx_);

    // need to check again
    if (netDevMgrInstance[deviceLogicId].isInited_) {
        HCCL_INFO("[GlobalNetDevMgr][Init]Has been inited. devicePhyId[%u], deviceLogicId[%d]", 
            devicePhyId, deviceLogicId);
        return HCCL_SUCCESS;
    }

    netDevMgrInstance[deviceLogicId].devicePhyId_ = devicePhyId;
    netDevMgrInstance[deviceLogicId].deviceLogicId_ = deviceLogicId;

    if (!isDlRaInited_) {
        CHK_RET(hccl::DlRaFunction::GetInstance().DlRaFunctionInit());
        CHK_RET(hccl::DlHalFunction::GetInstance().DlHalFunctionInit());
        isDlRaInited_ = true;
    }

    bool isHostUseDevNic;
    CHK_RET(IsHostUseDevNic(isHostUseDevNic));
    HCCL_DEBUG("[%s]HcclNetDevGetBusAddr, deviceLogicId[%u], devicePhyId[%u], nicDeploy[%d], hasBackup[%d],",
        __func__, deviceLogicId, devicePhyId, NICDeployment::NIC_DEPLOYMENT_DEVICE, false);
    CHK_RET(hccl::NetworkManager::GetInstance(deviceLogicId)
                .InitV2(NICDeployment::NIC_DEPLOYMENT_DEVICE, false, devicePhyId, isHostUseDevNic));

    netDevMgrInstance[deviceLogicId].isInited_ = true;

    HCCL_INFO("[GlobalNetDevMgr][Init]Init success, devicePhyId[%u], deviceLogicId[%d]",
        devicePhyId, deviceLogicId);
    return HCCL_SUCCESS;
}

void GlobalNetDevMgr::UnInit()
{
    if (!isInited_) {
        HCCL_INFO(
            "[GlobalNetDevMgr][UnInit]has been deinited. devicePhyId[%u], deviceLogicId[%d]", devicePhyId_, deviceLogicId_);
        return;
    }

    std::unique_lock<std::mutex> lock(netDevCtxMtx_);

    (void)hccl::NetworkManager::GetInstance(deviceLogicId_).DeInitV2(NICDeployment::NIC_DEPLOYMENT_DEVICE, false, false);

    isInited_ = false;
    HCCL_INFO("[GlobalNetDevMgr][UnInit]UnInit success. devicePhyId[%u], deviceLogicId[%d]", devicePhyId_, deviceLogicId_);
}

HcclResult GlobalNetDevMgr::GetDeviceVnicIP(u32 devicePhyId, u32 superDeviceId, hccl::HcclIpAddress &ipAddr)
{
    bool useSuperPodMode;
    CHK_RET(IsSuperPodMode(useSuperPodMode));
    if (useSuperPodMode) {
        CHK_RET(hrtRaGetSingleSocketVnicIpInfo(devicePhyId,
            DeviceIdType::DEVICE_ID_TYPE_SDID,
            superDeviceId,
            ipAddr));
    } else {
        CHK_RET(hrtRaGetSingleSocketVnicIpInfo(devicePhyId,
            DeviceIdType::DEVICE_ID_TYPE_PHY_ID,
            devicePhyId,
            ipAddr));
    }
    return HCCL_SUCCESS;
}

HcclResult GlobalNetDevMgr::RefNetDevCtx(NicType nicType, const HcclIpAddress &ipAddr, u32 port,
    HcclNetDevCtx &netDevCtx)
{
    HCCL_INFO("[GlobalNetDevMgr][RefNetDevCtx] nicType[%d], ip[%s]", nicType, ipAddr.GetReadableAddress());

    if (devicePhyId_ == INVALID_UINT || deviceLogicId_ == INVALID_INT) {
        CHK_RET(hrtGetDevice(&deviceLogicId_));
        CHK_RET(hrtGetDevicePhyIdByIndex(static_cast<u32>(deviceLogicId_), devicePhyId_));
    }

    std::lock_guard<std::mutex> lock(netDevCtxMtx_);

    // 进程粒度open dev，如果已open，直接复用
    PortInfo portInfo(ipAddr, port);
    if (netDevCtxMap_.find(portInfo) != netDevCtxMap_.end()) {
        netDevCtx = netDevCtxMap_[portInfo].second;
        CHK_PTR_NULL(netDevCtx);
 
        auto &netDevCtxRef = netDevCtxRefMap_[portInfo];
        netDevCtxRef.Ref();

        HCCL_INFO(
            "[GlobalNetDevMgr][RefNetDevCtx] nicType[%d] ip[%s] has been Ref.",
            nicType, ipAddr.GetReadableAddress());
        return HCCL_SUCCESS;
    }

    HcclNetDevCtx tempNetDevCtx;
    CHK_RET(HcclNetOpenDev(&tempNetDevCtx, nicType, devicePhyId_, deviceLogicId_, ipAddr));
    CHK_PTR_NULL(tempNetDevCtx);

    if (port != 0) {
        HcclResult ret = ServerSocketListenInner(tempNetDevCtx, port);
        if (ret != HCCL_SUCCESS) {
            HcclNetCloseDev(tempNetDevCtx);
            return ret;
        }
    }

    netDevCtxMap_.insert(std::make_pair(portInfo, std::make_pair(nicType, tempNetDevCtx)));

    Referenced ref;
    ref.Ref();
    netDevCtxRefMap_.insert(std::make_pair(portInfo, ref));

    netDevCtx = tempNetDevCtx;
    HCCL_INFO(
        "[GlobalNetDevMgr][RefNetDevCtx] nicType[%d] ip[%s] has been Init.", nicType, ipAddr.GetReadableAddress());
    return HCCL_SUCCESS;
}

HcclResult GlobalNetDevMgr::UnRefNetDevCtx(NicType nicType, const HcclIpAddress &ipAddr, u32 port)
{
    HCCL_INFO("[GlobalNetDevMgr][UnRefNetDevCtx] nicType[%d], ip[%s]", nicType, ipAddr.GetReadableAddress());

    if (devicePhyId_ == INVALID_UINT || deviceLogicId_ == INVALID_INT) {
        CHK_RET(hrtGetDevice(&deviceLogicId_));
        CHK_RET(hrtGetDevicePhyIdByIndex(static_cast<u32>(deviceLogicId_), devicePhyId_));
    }

    std::lock_guard<std::mutex> lock(netDevCtxMtx_);
    HcclNetDevCtx netDevCtx;
    PortInfo portInfo(ipAddr, port);
    if (netDevCtxMap_.find(portInfo) != netDevCtxMap_.end()) {
        netDevCtx = netDevCtxMap_[portInfo].second;
        CHK_PTR_NULL(netDevCtx);

        auto &netDevCtxRef = netDevCtxRefMap_[portInfo];
        netDevCtxRef.Unref();
        HCCL_INFO(
            "[GlobalNetDevMgr][RefNetDevCtx] nicType[%d] ip[%s] has been UnRef.",
            nicType, ipAddr.GetReadableAddress());

        if (netDevCtxRef.Count() == 0) {
            (void)ServerSocketStopInner(port);
            netDevCtxMap_.erase(portInfo);
            netDevCtxRefMap_.erase(portInfo);
            HCCL_INFO(
                "[GlobalNetDevMgr][RefNetDevCtx] nicType[%d] ip[%s] has been Deinit.",
                nicType, ipAddr.GetReadableAddress());
        }
    }
    return HCCL_SUCCESS;
}

HcclResult GlobalNetDevMgr::ServerSocketListen(const HcclNetDevCtx netDevCtx, u32 port)
{
    HCCL_INFO("[GlobalNetDevMgr][%s] start.", __func__);
    HcclResult ret = ServerSocketListenInner(netDevCtx, port);
    if (ret != HCCL_SUCCESS) {
        return ret;
    }

    HCCL_INFO("[GlobalNetDevMgr][%s] end.", __func__);
    return HCCL_SUCCESS;
}

HcclResult GlobalNetDevMgr::ServerSocketStopListen(const HcclNetDevCtx netDevCtx, u32 port)
{
    HCCL_INFO("[GlobalNetDevMgr][%s] start.", __func__);
    (void)ServerSocketStopInner(port);
    HCCL_INFO("[GlobalNetDevMgr][%s] end.", __func__);
    return HCCL_SUCCESS;
}

HcclResult GlobalNetDevMgr::ServerSocketListenInner(const HcclNetDevCtx netDevCtx, const uint32_t port)
{
    uint32_t listenPort = port;
    if (serverSocketMap_.find(port) != serverSocketMap_.end()) {
        HCCL_INFO("[GlobalNetDevMgr::%s] reuse serverSocket for port[%u]", __func__, port);
        return HCCL_SUCCESS;
    }

    EXECEPTION_CATCH(
        serverSocket_ = std::make_shared<hccl::HcclSocket>(static_cast<HcclNetDevCtx>(netDevCtx), listenPort),
        return HCCL_E_PTR);
    CHK_PTR_NULL(serverSocket_);

    HcclResult ret = serverSocket_->Init();
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("[GlobalNetDevMgr][%s] HcclSocket Init failed, ret[%d]", __func__, ret);
        return ret;
    }

    ret = serverSocket_->Listen();
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("[GlobalNetDevMgr][%s] HcclSocket Listen failed, ret[%d]", __func__, ret);
        return ret;
    }

    serverSocketMap_[listenPort] = serverSocket_;
    serverPort_ = port;
    HCCL_INFO("[GlobalNetDevMgr][%s] listen on port[%u] success", __func__, serverPort_);
    return HCCL_SUCCESS;
}

HcclResult GlobalNetDevMgr::ServerSocketStopInner(const uint32_t port)
{
    uint32_t listenPort = port;
    auto it = serverSocketMap_.find(port);
    if (it == serverSocketMap_.end() || it->second == nullptr) {
        HCCL_INFO("[GlobalNetDevMgr::%s] reuse serverSocket for port[%u]", __func__, port);
        return HCCL_SUCCESS;
    }

    serverSocketMap_.erase(it);
    HCCL_INFO("[GlobalNetDevMgr][%s] stop listen on port[%u] success", __func__, listenPort);
    return HCCL_SUCCESS;
}

HcclResult GlobalNetDevMgr::AddListenSocketWhiteList(uint32_t port, const std::vector<SocketWlistInfo> &wlistInfos)
{
    if (wlistInfos.empty()) {
        HCCL_ERROR("[GlobalNetDevMgr][%s] empty whitelist", __func__);
        return HCCL_E_PARA;
    }

    uint32_t listenPort = port;
    auto it = serverSocketMap_.find(listenPort);
    if (it == serverSocketMap_.end() || it->second == nullptr) {
        HCCL_ERROR("[GlobalNetDevMgr][%s] no listen socket for port[%u]", __func__, listenPort);
        return HCCL_E_NOT_FOUND;
    }
    std::vector<SocketWlistInfo> mutableCopy = wlistInfos;
    return it->second->AddWhiteList(mutableCopy);
}

HcclResult GlobalNetDevMgr::AcceptDataSocket(uint32_t port, const std::string &tag,
    std::shared_ptr<hccl::HcclSocket> &outConnected, uint32_t acceptTimeoutMs)
{
    uint32_t listenPort = port;
    auto it = serverSocketMap_.find(listenPort);
    if (it == serverSocketMap_.end() || it->second == nullptr) {
        HCCL_ERROR("[GlobalNetDevMgr][%s] no listen socket for port[%u]", __func__, listenPort);
        return HCCL_E_NOT_FOUND;
    }
    return it->second->Accept(tag, outConnected, acceptTimeoutMs);
}

HcclResult GlobalNetDevMgr::WaitClientSocketLinkEstablished(const std::shared_ptr<hccl::HcclSocket> &socket, s32 timeoutSec)
{
    CHK_SMART_PTR_NULL(socket);
    u32 pollCount = 0;
    const auto startTime = std::chrono::steady_clock::now();
    const auto timeout = std::chrono::seconds(timeoutSec > 0 ? timeoutSec : GetExternalInputHcclLinkTimeOut());
    HCCL_DEBUG("[GlobalNetDevMgr][client][WaitLink] waiting for socket link up...");
    while (true) {
        if ((std::chrono::steady_clock::now() - startTime) >= timeout) {
            HCCL_ERROR("[GlobalNetDevMgr][client][WaitLink] wait socket establish timeout, timeout[%lld s]",
                static_cast<long long>(timeout.count()));
            socket->SetStatus(hccl::HcclSocketStatus::SOCKET_TIMEOUT);
            return HCCL_E_TIMEOUT;
        }
        const hccl::HcclSocketStatus status = socket->GetStatus();
        if (status == hccl::HcclSocketStatus::SOCKET_OK) {
            HCCL_DEBUG("[GlobalNetDevMgr][client][WaitLink] socket established. localIp[%s], remoteIp[%s]",
                socket->GetLocalIp().GetReadableIP(), socket->GetRemoteIp().GetReadableIP());
            return HCCL_SUCCESS;
        }
        if (status == hccl::HcclSocketStatus::SOCKET_CONNECTING) {
            SaluSleep(ONE_MILLISECOND_OF_USLEEP);
            if (pollCount % 50U == 0U) {
                HCCL_DEBUG("[GlobalNetDevMgr][client][WaitLink] socket is connecting");
            }
            ++pollCount;
            continue;
        }
        if (status == hccl::HcclSocketStatus::SOCKET_TIMEOUT) {
            return HCCL_E_TIMEOUT;
        }
        socket->SetStatus(hccl::HcclSocketStatus::SOCKET_ERROR);
        return HCCL_E_TCP_CONNECT;
    }
}

void GlobalNetDevMgr::MakeSocketTag(hccl::HcclIpAddress tagServerIp, uint32_t tagServerPort,
    hccl::HcclIpAddress tagClientIp, std::string &socketTag, u64 id)
{
    socketTag = tagServerIp.GetReadableIP() + std::string(":") + std::to_string(tagServerPort) +
        std::string(":") + tagClientIp.GetReadableIP() + std::string(":") + std::to_string(id);
}

HcclResult GlobalNetDevMgr::ConnectToServer(const HcclNetDevCtx netDevCtx, hccl::HcclIpAddress remoteIp,
    uint32_t remotePort, std::string &socketTag, std::shared_ptr<hccl::HcclSocket> &socket)
{
    HCCL_INFO("[GlobalNetDevMgr]ConnectToServer start");

    auto *netDevCtxPtr = static_cast<hccl::NetDevContext *>(netDevCtx);
    hccl::HcclIpAddress localIpAddr = netDevCtxPtr->GetLocalIp();
    uint32_t localPort = serverPort_;

    HCCL_INFO("[GlobalNetDevMgr]ConnectToServer localIp[%s] localPort[%u] remoteIp[%s] remotePort[%u] socketTag[%s]",
        localIpAddr.GetReadableIP(), localPort, remoteIp.GetReadableIP(), remotePort, socketTag.c_str());

    HCCL_INFO("[GlobalNetDevMgr][client] ConnectToServer connect to server");
    std::shared_ptr<hccl::HcclSocket> socketTemp = nullptr;
    EXECEPTION_CATCH(socketTemp = std::make_shared<hccl::HcclSocket>(socketTag, netDevCtx, remoteIp, remotePort,
                        hccl::HcclSocketRole::SOCKET_ROLE_CLIENT),
        return HCCL_E_PTR);
    CHK_SMART_PTR_NULL(socketTemp);
    CHK_RET(socketTemp->Init());
    CHK_RET(socketTemp->Connect());
    HcclResult waitRet = WaitClientSocketLinkEstablished(socketTemp, 0);
    if (waitRet != HCCL_SUCCESS) {
        socketTemp->Close();
        return waitRet;
    }

    socket = socketTemp;
    HCCL_INFO("[GlobalNetDevMgr]ConnectToServer done localPort[%u] remotePort[%u]",
         socket->GetLocalPort(), socket->GetRemotePort());

    return HCCL_SUCCESS;
}

HcclResult GlobalNetDevMgr::AcceptClient(const HcclNetDevCtx netDevCtx, hccl::HcclIpAddress remoteIp,
   std::string &socketTag, std::shared_ptr<hccl::HcclSocket> &socket)
{
    HCCL_INFO("[GlobalNetDevMgr]AcceptClient start");

    auto *netDevCtxPtr = static_cast<hccl::NetDevContext *>(netDevCtx);
    hccl::HcclIpAddress localIpAddr = netDevCtxPtr->GetLocalIp();
    uint32_t localPort = serverPort_;

    HCCL_INFO("[GlobalNetDevMgr]AcceptClient localIp[%s] localPort[%u] remoteIp[%s] socketTag[%s]",
        localIpAddr.GetReadableIP(), localPort, remoteIp.GetReadableIP(), socketTag.c_str());

    HCCL_INFO("[GlobalNetDevMgr][server] AcceptClient listen and accept");
    SocketWlistInfo wlistEntry{};
    wlistEntry.connLimit = 1U;
    const auto bin = remoteIp.GetBinaryAddress();
    wlistEntry.remoteIp.addr = bin.addr;
    wlistEntry.remoteIp.addr6 = bin.addr6;
    s32 mw = memcpy_s(wlistEntry.tag, sizeof(wlistEntry.tag), socketTag.c_str(), socketTag.size() + 1U);
    CHK_PRT_RET(mw != EOK, HCCL_ERROR("[GlobalNetDevMgr]memcpy_s whitelist tag failed"),
        HCCL_E_MEMORY);
    const std::vector<SocketWlistInfo> wlistVec = { wlistEntry };
    CHK_RET(AddListenSocketWhiteList(localPort, wlistVec));

    std::shared_ptr<hccl::HcclSocket> socketTemp = nullptr;
    CHK_RET(AcceptDataSocket(localPort, socketTag, socketTemp, 0));
    CHK_SMART_PTR_NULL(socketTemp);

    socket = socketTemp;
    HCCL_INFO("[GlobalNetDevMgr]AcceptClient done localPort[%u] remotePort[%u]",
         socket->GetLocalPort(), socket->GetRemotePort());

    return HCCL_SUCCESS;
}

void GlobalNetDevMgr::CloseSocket(std::shared_ptr<hccl::HcclSocket> &socket)
{
    if (socket != nullptr) {
        socket->Close();
        socket = nullptr;
    }
}
} // namespace hccl
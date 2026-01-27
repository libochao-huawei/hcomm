/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "socket_mgr.h"
#include "../channels/channel.h"
#include "orion_adpt_utils.h"
#include "host_socket_handle_manager.h"
#include "exception_handler.h"
#include "adapter_rts.h"

namespace hcomm {

constexpr uint32_t TempServerListenPort = 60001;    // 临时固定监听端口，用于功能验证

HcclResult SocketMgr::Init()
{
    if (isLoaded_) {
        return HCCL_SUCCESS;
    }
    isLoaded_ = true;
    s32 devLogicId;
    CHK_RET(hrtGetDevice(&devLogicId));
    CHK_RET(hrtGetDevicePhyIdByIndex(static_cast<u32>(devLogicId), devicePhyId_));
    serverListenPort_ = TempServerListenPort;
    return HCCL_SUCCESS;
}

HcclResult SocketMgr::AddWhiteList(const Hccl::SocketConfig &socketConfig, const Hccl::SocketHandle &socketHandle)
{
    EXCEPTION_HANDLE_BEGIN

    // 1. 创建 wlistInfo 对象
    Hccl::RaSocketWhitelist wlistInfo{};;
    wlistInfo.connLimit = 1;
    wlistInfo.remoteIp = socketConfig.link.GetRemoteAddr();
    wlistInfo.tag = socketConfig.GetHccpTag();

    std::vector<Hccl::RaSocketWhitelist> wlistInfoVec;
    wlistInfoVec.clear();
    wlistInfoVec.push_back(wlistInfo);

     // 2. 加入白名单
    Hccl::HrtRaSocketWhiteListAdd(socketHandle, wlistInfoVec);

    EXCEPTION_HANDLE_END
    return HCCL_SUCCESS;
}

HcclResult SocketMgr::GetSocketHandle(const Hccl::SocketConfig &socketConfig, Hccl::SocketHandle &socketHandle)
{
    EXCEPTION_HANDLE_BEGIN

    // 加异常捕获
    auto localPort = socketConfig.link.GetLocalPort();
    if (localPort.GetType() == Hccl::PortDeploymentType::DEV_NET) { 
        socketHandle = Hccl::SocketHandleManager::GetInstance().Get(devicePhyId_, localPort);
        if (socketHandle == nullptr) {
            socketHandle = Hccl::SocketHandleManager::GetInstance().Create(devicePhyId_, localPort);
        }
    } else if (localPort.GetType() == Hccl::PortDeploymentType::HOST_NET){
        socketHandle = Hccl::HostSocketHandleManager::GetInstance().Get(devicePhyId_, localPort.GetAddr());
        if (socketHandle == nullptr) {
            socketHandle = Hccl::HostSocketHandleManager::GetInstance().Create(devicePhyId_, localPort.GetAddr());
        }
    } else {
        HCCL_ERROR(
            "[SocketMgr] PortDeploymentType = %d, not support create socket.", localPort.GetType().Describe().c_str());
        return HCCL_E_NOT_SUPPORT;
    }
    if (socketHandle == nullptr) {
        HCCL_ERROR("[SocketMgr] socketHandle is nullptr, devicePhyId=%d, localPort[%s]",
            devicePhyId_, localPort.Describe().c_str());
        return HCCL_E_INTERNAL;
    }
    HCCL_INFO("[SocketMgr][%s] socketHandle[%p] devicePhyId[%u] localPort[%s]",
        __func__, socketHandle, devicePhyId_, localPort.Describe().c_str());

    EXCEPTION_HANDLE_END
    return HCCL_SUCCESS;
}

HcclResult SocketMgr::CreateSocket(const Hccl::SocketConfig &socketConfig, const Hccl::SocketHandle &socketHandle)
{
    EXCEPTION_HANDLE_BEGIN

    Hccl::IpAddress  localIpAddress  = socketConfig.link.GetLocalAddr();
    Hccl::IpAddress  remoteIpAddress = socketConfig.link.GetRemoteAddr();
    Hccl::SocketRole socketRole      = socketConfig.GetRole();
    std::string     hccpSocketTag   = socketConfig.GetHccpTag();
    
    std::unique_ptr<Hccl::Socket> tmpSocket = nullptr;
    if (socketConfig.link.GetType() == Hccl::PortDeploymentType::DEV_NET) {
        EXECEPTION_CATCH(
            tmpSocket = std::make_unique<Hccl::Socket>(
                socketHandle, localIpAddress, serverListenPort_,
                remoteIpAddress, hccpSocketTag,
                socketRole, Hccl::NicType::DEVICE_NIC_TYPE
            ),
            return HCCL_E_PTR
        );
        HCCL_INFO("[SocketMgr][%s] client_socket_info[%s]", __func__, tmpSocket->Describe().c_str());
        tmpSocket->ConnectAsync();
    } else if (socketConfig.link.GetType() == Hccl::PortDeploymentType::HOST_NET) {
        EXECEPTION_CATCH(
            tmpSocket = std::make_unique<Hccl::Socket>(socketHandle,
            localIpAddress,
            serverListenPort_,
            remoteIpAddress,
            hccpSocketTag,
            socketRole,
            Hccl::NicType::HOST_NIC_TYPE),
            return HCCL_E_PTR
        );
        HCCL_INFO("[SocketMgr][%s] client_socket_info[%s]", __func__, tmpSocket->Describe().c_str());
        tmpSocket->Connect();
    } else {
        HCCL_ERROR(
            "[SocketMgr] PortDeploymentType = %d, not support create socket.", socketConfig.link.GetType().Describe().c_str());
        return HCCL_E_NOT_SUPPORT;
    }

    socketMap_[socketConfig] = std::move(tmpSocket);

    EXCEPTION_HANDLE_END
    return HCCL_SUCCESS;
}

HcclResult SocketMgr::GetSocket(const Hccl::SocketConfig &socketConfig, Hccl::Socket*& socket)
{
    CHK_RET(Init());
    // 1. 先查找
    auto it = socketMap_.find(socketConfig);
    if (it != socketMap_.end()) {
        socket = it->second.get();
        return HCCL_SUCCESS;
    }

    // 2. 不存在则创建
    Hccl::SocketHandle socketHandle;
    CHK_RET(GetSocketHandle(socketConfig, socketHandle));
    CHK_RET(AddWhiteList(socketConfig, socketHandle));
    CHK_RET(CreateSocket(socketConfig, socketHandle));
 
    // 3. 再次查找
    it = socketMap_.find(socketConfig);
    if (it == socketMap_.end()) {
        HCCL_ERROR("[SocketMgr][%s] CreateSocket succeeded but socket not found",
                   __func__);
        return HCCL_E_INTERNAL;
    }
 
    socket = it->second.get();
    return HCCL_SUCCESS;
}

} // namespace hcomm

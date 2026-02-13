/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "server_socket_manager.h"
#include "../channels/channel.h"
#include "orion_adpt_utils.h"
#include "host_socket_handle_manager.h"
#include "exception_handler.h"
#include "adapter_rts.h"

namespace hcomm {
HcclResult ServerSocketManager::ServerSocketStartListen(const Hccl::PortData& localPort, const Hccl::NicType nicType, const uint32_t devPhyId, const uint32_t port)
{
    if (nicType == Hccl::NicType::HOST_NIC_TYPE) {
        CHK_RET(HostSocketListen(localPort, devPhyId, port));
    } else if (nicType == Hccl::NicType::DEVICE_NIC_TYPE) {  
        CHK_RET(DeviceSocketListen(localPort, devPhyId, port));
    }
    
    return HCCL_SUCCESS;
}

HcclResult ServerSocketManager::HostSocketListen(const Hccl::PortData& localPort, const uint32_t devPhyId, const uint32_t port)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (hostServerSocketMap_.find(localPort) != hostServerSocketMap_.end() &&
        hostServerSocketMap_[localPort].find(port) != hostServerSocketMap_[localPort].end()){
        hostServerSocketMap_[localPort][port].second = hostServerSocketMap_[localPort][port].second + 1; // 计数+1
        HCCL_INFO("[ServerSocketManager][%s] reuse serverSocket", __func__);
        return HCCL_SUCCESS;
    }

    Hccl::SocketHandle socketHandle{};
    EXECEPTION_CATCH(
            socketHandle = Hccl::HostSocketHandleManager::GetInstance().Create(devPhyId, localPort.GetAddr()), return HCCL_E_PARA);

    std::unique_ptr<Hccl::Socket> serverSocket{};
    EXECEPTION_CATCH(serverSocket = std::make_unique<Hccl::Socket>(
        socketHandle, localPort.GetAddr(), port, localPort.GetAddr(), "server", 
        Hccl::SocketRole::SERVER, Hccl::NicType::HOST_NIC_TYPE), return HCCL_E_PARA); //端口号可能冲突，需要SE做决定
    HCCL_INFO("[ServerSocketManager][%s] listen_socket_info[%s]", __func__, serverSocket->Describe().c_str());
    EXECEPTION_CATCH(serverSocket->Listen(), return HCCL_E_INTERNAL);
 
    hostServerSocketMap_[localPort][port] = std::make_pair(std::move(serverSocket), 1);
    
    return HCCL_SUCCESS;
}

HcclResult ServerSocketManager::DeviceSocketListen(const Hccl::PortData& localPort, const uint32_t devPhyId, const uint32_t port)
{
    std::lock_guard<std::mutex> lock(mutex2_);
    if (deviceServerSocketMap_.find(localPort) != deviceServerSocketMap_.end() &&
        deviceServerSocketMap_[localPort].find(port) != deviceServerSocketMap_[localPort].end()){
        deviceServerSocketMap_[localPort][port].second = deviceServerSocketMap_[localPort][port].second + 1; // 计数+1
        HCCL_INFO("[ServerSocketManager][%s] reuse serverSocket", __func__);
        return HCCL_SUCCESS;
    }

    Hccl::SocketHandle socketHandle{};
    EXECEPTION_CATCH(
            socketHandle = Hccl::SocketHandleManager::GetInstance().Create(devPhyId, localPort), return HCCL_E_PARA);

    std::unique_ptr<Hccl::Socket> serverSocket;
    EXECEPTION_CATCH(serverSocket = std::make_unique<Hccl::Socket>(
        socketHandle, localPort.GetAddr(), port, localPort.GetAddr(), "server", 
        Hccl::SocketRole::SERVER, Hccl::NicType::DEVICE_NIC_TYPE), return HCCL_E_PARA); //端口号可能冲突，需要SE做决定
    HCCL_INFO("[ServerSocketManager][%s] listen_socket_info[%s]", __func__, serverSocket->Describe().c_str());
    EXECEPTION_CATCH(serverSocket->Listen(), return HCCL_E_INTERNAL);
    
    deviceServerSocketMap_[localPort][port] = std::make_pair(std::move(serverSocket), 1);

    return HCCL_SUCCESS;
}

HcclResult ServerSocketManager::ServerSocketStopListen(const Hccl::PortData& localPort, const Hccl::NicType nicType, const uint32_t port)
{
    if (nicType == Hccl::NicType::DEVICE_NIC_TYPE) {
        std::lock_guard<std::mutex> lock(mutex2_);
        if (deviceServerSocketMap_.find(localPort) != deviceServerSocketMap_.end() && 
            deviceServerSocketMap_[localPort].find(port) != deviceServerSocketMap_[localPort].end()) {
            deviceServerSocketMap_[localPort][port].second = deviceServerSocketMap_[localPort][port].second - 1; // 计数-1
            if (deviceServerSocketMap_[localPort][port].second == 0) {
                deviceServerSocketMap_[localPort].erase(port);
            }
            if (deviceServerSocketMap_[localPort].empty()) {
                deviceServerSocketMap_.erase(localPort);
            }
            return HCCL_SUCCESS;
        }
    } else if (nicType == Hccl::NicType::HOST_NIC_TYPE) {  
        std::lock_guard<std::mutex> lock(mutex_);
        if (hostServerSocketMap_.find(localPort) != hostServerSocketMap_.end() && 
            hostServerSocketMap_[localPort].find(port) != hostServerSocketMap_[localPort].end()) {
            hostServerSocketMap_[localPort][port].second = hostServerSocketMap_[localPort][port].second - 1; // 计数-1
            if (hostServerSocketMap_[localPort][port].second == 0) {
                hostServerSocketMap_[localPort].erase(port);
            }
            if (hostServerSocketMap_[localPort].empty()) {
                hostServerSocketMap_.erase(localPort);
            }
            return HCCL_SUCCESS;
        }
    }
    
    return HCCL_SUCCESS;
}

} // namespace hcomm

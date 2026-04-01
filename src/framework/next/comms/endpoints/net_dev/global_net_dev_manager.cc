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

using namespace hccl;

namespace hccl {
std::map<PortInfo, std::pair<NicType, HcclNetDevCtx>> GlobalNetDevMgr::netDevCtxMap_;
std::map<PortInfo, Referenced> GlobalNetDevMgr::netDevCtxRefMap_;
std::mutex GlobalNetDevMgr::netDevCtxMtx_;

GlobalNetDevMgr::~GlobalNetDevMgr()
{
    Destroy();
}

GlobalNetDevMgr& GlobalNetDevMgr::GetInstance(u32 devicePhyId)
{
    // reserve 1 instance for invalid deviceid and host
    static GlobalNetDevMgr instance[MAX_MODULE_DEVICE_NUM + 1];
    u32 deviceLogicID = 0;
    HcclResult hcclRet = hrtGetDeviceIndexByPhyId(devicePhyId, deviceLogicID);
    if (hcclRet != HCCL_SUCCESS) {
        HCCL_RUN_WARNING("GlobalNetDevMgr::GetInstance hrtGetDeviceIndexByPhyId failed, ret[%d], "
            "return reserve instance", hcclRet);
        return instance[MAX_MODULE_DEVICE_NUM];
    }

    if (static_cast<u32>(deviceLogicID) >= MAX_MODULE_DEVICE_NUM || deviceLogicID <= HOST_DEVICE_ID) {
        HCCL_RUN_WARNING("[Get][Instance]deviceLogicID[%u] is invalid, return reserve instance", deviceLogicID);
        return instance[MAX_MODULE_DEVICE_NUM];
    }

    HCCL_INFO("GlobalNetDevMgr::GetInstance deviceLogicID[%u].", deviceLogicID);
    return instance[deviceLogicID];
}

HcclResult GlobalNetDevMgr::Init()
{
    HCCL_INFO("[GlobalNetDevMgr][%s] start.", __func__);
    std::unique_lock<std::mutex> lock(netDevCtxMtx_);

    CHK_RET(InitNic());

    lock.unlock();

    HCCL_INFO("[GlobalNetDevMgr][%s] end.", __func__);
    return HCCL_SUCCESS;
}

HcclResult GlobalNetDevMgr::Destroy()
{
    HCCL_INFO("[GlobalNetDevMgr][%s] start.", __func__);

    if (devicePhyId_ == INVALID_UINT || deviceLogicId_ == INVALID_INT) {
        CHK_RET(hrtGetDevice(&deviceLogicId_));
        CHK_RET(hrtGetDevicePhyIdByIndex(static_cast<u32>(deviceLogicId_), devicePhyId_));
    }

    std::unique_lock<std::mutex> lock(netDevCtxMtx_);

    for (auto& pair : netDevCtxMap_) {
        if (pair.second.first == NicType::DEVICE_NIC_TYPE) {
            socketManager_->ServerDeInit(pair.first.ip, pair.first.listenPort);
        }
        HcclNetCloseDev(pair.second.second);
        HCCL_INFO("[GlobalNetDevMgr][%s] Close netdev[%p].", __func__, pair.second.second);
    }

    netDevCtxRefMap_.clear();
    netDevCtxMap_.clear();
 
    CHK_RET(DeInitNic());

    lock.unlock();

    HCCL_INFO("[GlobalNetDevMgr][%s] end.", __func__);
    return HCCL_SUCCESS;
}

HcclResult GlobalNetDevMgr::ServerInit(const HcclNetDevCtx netDevCtx, u32 port)
{
    HCCL_INFO("[GlobalNetDevMgr][%s] start.", __func__);

    NetDevContext *netDevCtxTmp = static_cast<NetDevContext *>(netDevCtx);
    if (netDevCtxTmp->GetNicType() == NicType::DEVICE_NIC_TYPE) {
        HcclResult ret = socketManager_->ServerInit(netDevCtx, port);
        if (ret != HCCL_SUCCESS) {
            HcclNetCloseDev(netDevCtx);
            return ret; 
        }
    }

    HCCL_INFO("[GlobalNetDevMgr][%s] end.", __func__);
    return HCCL_SUCCESS;
}

HcclResult GlobalNetDevMgr::ServerDeInit(const HcclNetDevCtx netDevCtx, u32 port)
{
    HCCL_INFO("[GlobalNetDevMgr][%s] start.", __func__);
    NetDevContext *netDevCtxTmp = static_cast<NetDevContext *>(netDevCtx);
    if (netDevCtxTmp->GetNicType() == NicType::DEVICE_NIC_TYPE) {
        socketManager_->ServerDeInit(netDevCtxTmp->GetLocalIp(), port);
    }
    HCCL_INFO("[GlobalNetDevMgr][%s] end.", __func__);
    return HCCL_SUCCESS;
}

HcclResult GlobalNetDevMgr::InitNic()
{
    if (nicInited_) {
        return HCCL_SUCCESS;
    }
    // init after get the lock
    std::unique_lock<std::mutex> lock(netDevCtxMtx_);

    // need to check again
    if (nicInited_) {
        HCCL_INFO("[InitNic] Nic has been inited. devicePhyId[%u], deviceLogicId[%d]", devicePhyId_, deviceLogicId_);
        return HCCL_SUCCESS;
    }

    if (devicePhyId_ == INVALID_UINT || deviceLogicId_ == INVALID_INT) {
        CHK_RET(hrtGetDevice(&deviceLogicId_));
        CHK_RET(hrtGetDevicePhyIdByIndex(static_cast<u32>(deviceLogicId_), devicePhyId_));
    }

    CHK_RET(HcclNetInit(NICDeployment::NIC_DEPLOYMENT_DEVICE, devicePhyId_, static_cast<u32>(deviceLogicId_), false));
    nicInited_ = true;
    socketManager_.reset(new (std::nothrow) HcclSocketManager(NICDeployment::NIC_DEPLOYMENT_DEVICE, deviceLogicId_, devicePhyId_, 0));
    CHK_PTR_NULL(socketManager_);

    HCCL_INFO("[InitNic] Nic init success, devicePhyId[%u], deviceLogicId[%d]", devicePhyId_, deviceLogicId_);
    return HCCL_SUCCESS;
}

HcclResult GlobalNetDevMgr::DeInitNic()
{
    if (!nicInited_) {
        HCCL_INFO(
            "[DeInitNic] Nic has been deinited. devicePhyId[%u], deviceLogicId[%d]", devicePhyId_, deviceLogicId_);
        return HCCL_SUCCESS;
    }

    if (devicePhyId_ == INVALID_UINT || deviceLogicId_ == INVALID_INT) {
        CHK_RET(hrtGetDevice(&deviceLogicId_));
        CHK_RET(hrtGetDevicePhyIdByIndex(static_cast<u32>(deviceLogicId_), devicePhyId_));
    }

    CHK_RET(HcclNetDeInit(NICDeployment::NIC_DEPLOYMENT_DEVICE, devicePhyId_, static_cast<u32>(deviceLogicId_)));
    nicInited_ = false;
    HCCL_INFO("[DeInitNic] Nic deinit success. devicePhyId[%u], deviceLogicId[%d]", devicePhyId_, deviceLogicId_);
    return HCCL_SUCCESS;
}

HcclResult GlobalNetDevMgr::HcclIpAddressConvertHcclAddr(HcclAddress *hccladdr, HcclIpAddress *hcclIP) {
    CHK_PTR_NULL(hcclIP);
    CHK_PTR_NULL(hccladdr);
    if (hcclIP->GetFamily() == AF_INET) {
        hccladdr->type = HCCL_ADDR_TYPE_IP_V4;
        hccladdr->addr = hcclIP->GetBinaryAddress().addr;
    } else if (hcclIP->GetFamily() == AF_INET6) {
        hccladdr->type = HCCL_ADDR_TYPE_IP_V6;
        hccladdr->addr6 = hcclIP->GetBinaryAddress().addr6;
    } else {
        HCCL_ERROR("[HcclIpAddressConvertingHcclAddr]ERROR IP type!");
        return HCCL_E_PARA;
    }
    return HCCL_SUCCESS;
}

HcclResult GlobalNetDevMgr::GetNicAddr(int32_t devicePhyId, HcclAddress **addr, uint32_t *addrNum)
{
    CHK_PTR_NULL(addrNum);
    CHK_PTR_NULL(addr);
 
    u32 deviceLogicId;
    CHK_RET(hrtGetDeviceIndexByPhyId(devicePhyId, deviceLogicId));
 
    // 先创建进程
    bool isHostUseDevNic;
    CHK_RET(IsHostUseDevNic(isHostUseDevNic));
    HCCL_DEBUG("[%s]HcclNetDevGetBusAddr, deviceLogicId[%u], devicePhyId[%u], nicDeploy[%d], hasBackup[%d],",
        __func__,
        deviceLogicId,
        devicePhyId,
        NICDeployment::NIC_DEPLOYMENT_DEVICE,
        false);
    CHK_RET(hccl::NetworkManager::GetInstance(deviceLogicId)
                .InitV2(NICDeployment::NIC_DEPLOYMENT_DEVICE, false, devicePhyId, isHostUseDevNic));
    CHK_RET(hccl::NetworkManager::GetInstance(deviceLogicId).GetNicIp(devicePhyId, addr, addrNum));

    // 销毁进程
    CHK_RET(hccl::NetworkManager::GetInstance(deviceLogicId)
                .DeInitV2(NICDeployment::NIC_DEPLOYMENT_DEVICE, false, false));
    return HCCL_SUCCESS;
}

HcclResult GlobalNetDevMgr::GetDeviceIP(u32 devicePhyId, std::vector<hccl::HcclIpAddress> &ipAddr)
{
    HcclAddress *hcclAddress = nullptr;
    uint32_t addrNum = 0;
    CHK_RET(GetNicAddr(devicePhyId, &hcclAddress, &addrNum));
    CHK_PTR_NULL(hcclAddress);

    HcclIpAddress hcclIpAddress;
    for (uint32_t i = 0; i < addrNum; i++) {
        CHK_RET(HcclIpAddressConvertHcclAddr(&hcclAddress[0], &hcclIpAddress));
        ipAddr.push_back(hcclIpAddress);
    }

    return HCCL_SUCCESS;
}

HcclResult GlobalNetDevMgr::RefNetDevCtx(NicType nicType, const HcclIpAddress &ipAddr, u32 port,
    HcclNetDevCtx &netDevCtx)
{
    HCCL_INFO("[GlobalNetDevMgr][RefNetDevCtx] nicType[%d], ip[%s]", nicType, ipAddr.GetReadableAddress());

    // auto init nic when ref
    if (!nicInited_) {
        InitNic();
    }

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

    if (nicType == NicType::DEVICE_NIC_TYPE && port != 0) {
        HcclResult ret = socketManager_->ServerInit(tempNetDevCtx, port);
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
            if (nicType == NicType::DEVICE_NIC_TYPE && port != 0) {
                (void)socketManager_->ServerDeInit(netDevCtx, port);
            }
            netDevCtxMap_.erase(portInfo);
            netDevCtxRefMap_.erase(portInfo);
            HCCL_INFO(
                "[GlobalNetDevMgr][RefNetDevCtx] nicType[%d] ip[%s] has been Deinit.",
                nicType, ipAddr.GetReadableAddress());
        }
    }
    return HCCL_SUCCESS;
}
} // namespace hccl
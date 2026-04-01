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
    u32 deviceLogicID = 0;
    HcclResult hcclRet = hrtGetDeviceIndexByPhyId(devicePhyId, deviceLogicID);
    if (hcclRet != HCCL_SUCCESS) {
        HCCL_RUN_WARNING("GlobalNetDevMgr::GetInstance hrtGetDeviceRefresh failed, ret[%d], "
            "return reserve instance", hcclRet);
        return instance[MAX_MODULE_DEVICE_NUM];
    }

    if (static_cast<u32>(deviceLogicID) >= MAX_MODULE_DEVICE_NUM) {
        HCCL_RUN_WARNING("[Get][Instance]deviceLogicID[%d] is invalid, return reserve instance", deviceLogicID);
        return instance[MAX_MODULE_DEVICE_NUM];
    }

    std::unique_lock<std::mutex> lock(netDevCtxMtx_);
    if (!netDevMgrInstance[deviceLogicID].isInited_) {
        hcclRet = Init(devicePhyId, deviceLogicID);
        if (hcclRet != HCCL_SUCCESS) {
            HCCL_RUN_WARNING("[Get][Instance]Init deviceLogicID[%d]fail, return reserve instance", deviceLogicID);
            return instance[MAX_MODULE_DEVICE_NUM];
        }
    }

    HCCL_INFO("GlobalNetDevMgr::GetInstance deviceLogicID[%u].", deviceLogicID);
    return netDevMgrInstance[deviceLogicID];
}

HcclResult GlobalNetDevMgr::Init(u32 devicePhyId, u32 deviceLogicID)
{
    if (netDevMgrInstance[deviceLogicID].isInited_) {
        return HCCL_SUCCESS;
    }

    // init after get the lock
    std::unique_lock<std::mutex> lock(netDevCtxMtx_);

    // need to check again
    if (netDevMgrInstance[deviceLogicID].isInited_) {
        HCCL_INFO("[GlobalNetDevMgr][Init]Has been inited. devicePhyId[%u], deviceLogicId[%d]", 
            devicePhyId, deviceLogicID);
        return HCCL_SUCCESS;
    }

    netDevMgrInstance[deviceLogicID].devicePhyId_ = devicePhyId;
    netDevMgrInstance[deviceLogicID].deviceLogicId_ = deviceLogicID;

    CHK_RET(HcclNetInit(NICDeployment::NIC_DEPLOYMENT_DEVICE, devicePhyId, static_cast<u32>(deviceLogicID), false));
    netDevMgrInstance[deviceLogicID].isInited_ = true;

    HCCL_INFO("[GlobalNetDevMgr][Init]Init success, devicePhyId[%u], deviceLogicId[%d]",
        devicePhyId, deviceLogicID);
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

    for (auto& pair : netDevCtxMap_) {
        if (pair.second.first == NicType::DEVICE_NIC_TYPE && pair.first.listenPort != 0 && socketManager_ != nullptr) {
            socketManager_->ServerDeInit(pair.first.ip, pair.first.listenPort);
        }
        HcclNetCloseDev(pair.second.second);
        HCCL_INFO("[GlobalNetDevMgr][UnInit][%s] Close netdev[%p].", __func__, pair.second.second);
    }

    netDevCtxRefMap_.clear();
    netDevCtxMap_.clear();
 
    (void)HcclNetDeInit(NICDeployment::NIC_DEPLOYMENT_DEVICE, devicePhyId_, static_cast<u32>(deviceLogicId_));

    isInited_ = false;
    HCCL_INFO("[GlobalNetDevMgr][UnInit]UnInit success. devicePhyId[%u], deviceLogicId[%d]", devicePhyId_, deviceLogicId_);
}

HcclResult GlobalNetDevMgr::ServerInit(const HcclNetDevCtx netDevCtx, u32 port)
{
    HCCL_INFO("[GlobalNetDevMgr][%s] start.", __func__);

    NetDevContext *netDevCtxTmp = static_cast<NetDevContext *>(netDevCtx);
    if (netDevCtxTmp->GetNicType() == NicType::DEVICE_NIC_TYPE) {
        if (socketManager_ == nullptr) {
            socketManager_.reset(new (std::nothrow) HcclSocketManager(
                NICDeployment::NIC_DEPLOYMENT_DEVICE, deviceLogicId_, devicePhyId_, 0));
            CHK_PTR_NULL(socketManager_);
        }
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
        if (socketManager_ != nullptr) {
            socketManager_->ServerDeInit(netDevCtxTmp->GetLocalIp(), port);
        }
    }
    HCCL_INFO("[GlobalNetDevMgr][%s] end.", __func__);
    return HCCL_SUCCESS;
}

HcclResult GlobalNetDevMgr::GetDeviceIP(u32 devicePhyId, std::vector<hccl::HcclIpAddress> &ipAddr)
{
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

    CHK_RET(hrtRaGetDeviceIP(devicePhyId, ipAddr));

    // 销毁进程
    CHK_RET(hccl::NetworkManager::GetInstance(deviceLogicId)
                .DeInitV2(NICDeployment::NIC_DEPLOYMENT_DEVICE, false, false));
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

    if (nicType == NicType::DEVICE_NIC_TYPE && port != 0) {
        socketManager_.reset(new (std::nothrow) HcclSocketManager(
            NICDeployment::NIC_DEPLOYMENT_DEVICE, deviceLogicId_, devicePhyId_, 0));
        CHK_PTR_NULL(socketManager_);

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
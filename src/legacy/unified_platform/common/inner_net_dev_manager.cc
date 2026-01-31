/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "inner_net_dev_manager.h"
#include "log.h"
#include "hccl_net_dev_v2.h"

namespace Hccl {
HcclResult InnerNetDevManager::AddDevice(const NetDevInfo &info, HcclNetDevice *&device)
{
    device = new HcclNetDevice(info);  
    if(device == nullptr) {
        HCCL_ERROR("new HcclNetDevice fail, devId[%u]", info.devId);
        return HCCL_E_PTR;
    }
    pltNetDevVec_.push_back(device);

    InnerNetDev* innerNetDev = nullptr;
    auto it = netDevMap_.find(info);
    if (it == netDevMap_.end()) {
        innerNetDev = new(nothrow) InnerNetDev(info);
        if(innerNetDev == nullptr || !innerNetDev->GetIsValid()) {
            HCCL_ERROR("new InnerNetDev fail, devId[%u]", info.devId);
            return HCCL_E_PARA;
        }
        netDevMap_.insert(std::make_pair(info, std::unique_ptr<InnerNetDev>(innerNetDev)));
        netDevCnt_[info] = 1;
    } else {
        innerNetDev = it->second.get();
        netDevCnt_[info]++;
    }
    device->SetInnerNetDev(innerNetDev);
    return HCCL_SUCCESS;
}

HcclResult InnerNetDevManager::DeleteDevice(Hccl::HcclNetDevice *device)
{
    if(device == nullptr) {
        return HCCL_SUCCESS;
    }
    HcclResult ret = RemoveDevice(device->GetNetDevInfo());
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("delete netDev fail, devId[%u]", device->GetNetDevInfo().devId);
        return HCCL_E_PARA;
    }
    delete device;
    device = nullptr;
    return HCCL_SUCCESS;
}

HcclResult InnerNetDevManager::RemoveDevice(const NetDevInfo &info)
{
    auto cntIt = netDevCnt_.find(info);
    if (cntIt == netDevCnt_.end()) {
        HCCL_ERROR("find HcclNetDevice fail, devId[%u]", info.devId);
        return HCCL_E_PTR;
    }

    cntIt->second--;
    if (cntIt->second == 0) {          
        netDevMap_.erase(info);
        netDevCnt_.erase(cntIt);
    }
    return HCCL_SUCCESS;
}

InnerNetDev *InnerNetDevManager::GetDevice(const NetDevInfo &info) 
{
    auto it = netDevMap_.find(info);
    if(it == netDevMap_.end()) {
        auto innerNetDev = new InnerNetDev(info);
        netDevMap_.insert(std::make_pair(info, std::unique_ptr<InnerNetDev>(innerNetDev)));
        netDevCnt_[info] = 1;
        return innerNetDev;
    }
    return it->second.get();
}

uint32_t InnerNetDevManager::GetDeviceCount(const NetDevInfo &info) const
{
    auto it = netDevCnt_.find(info);
    return (it != netDevCnt_.end()) ? it->second : 0;
}
bool InnerNetDevManager::ReplaceDevice(const NetDevInfo &info, std::unique_ptr<InnerNetDev> newDevice)
{
    auto it = netDevMap_.find(info);
    if (it == netDevMap_.end())
        return false;

    it->second = std::move(newDevice);
    return true;
}

RdmaHandle InnerNetDevManager::GetRdmaHandleByIP(uint32_t devPhyId, const IpAddress &ip)
{
    NetDevInfo info;
    info.devId = devPhyId;
    info.addr = ip;
    info.protoType = LinkProtoType::UB;
    return GetDevice(info)->getRdmaHandle();
}

void InnerNetDevManager::Cleanup()
{
    // 1. 清理netDevMap_：unique_ptr会自动释放所管理的InnerNetDev对象
    netDevMap_.clear();

    // 2. 清理netDevCnt_：直接清空计数映射
    netDevCnt_.clear();

    // 3. 清理pltNetDevVec_：释放所有HcclNetDev*指向的动态内存
    for (HcclNetDevice* dev : pltNetDevVec_) {
        if (dev != nullptr) {
            delete dev;  // 释放堆上分配的HcclNetDev对象
            dev = nullptr;  // 避免悬空指针
        }
    }
    pltNetDevVec_.clear();  // 清空容器
}

InnerNetDevManager &InnerNetDevManager::GetInstance()
{
    static InnerNetDevManager instance;
    return instance;
}
} // namespace Hccl

/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "mem_mapping_manager.h"
#include "private_types.h"
#include "adapter_hal.h"
#include "adapter_rts.h"
#include "dlhal_function.h"

namespace hccl {
MemMappingManager &MemMappingManager::GetInstance(s32 deviceLogicID)
{
    static MemMappingManager instance[MAX_DEV_NUM];
    if (deviceLogicID == HOST_DEVICE_ID) {
        return instance[DEFAULT_DEVICE_LOGIC_ID];
    }

    if (static_cast<u32>(deviceLogicID) >= MAX_DEV_NUM || deviceLogicID <= HOST_DEVICE_ID) {
        HCCL_WARNING("[Get][Instance]deviceLogicID[%d] is invalid", deviceLogicID);
        return instance[DEFAULT_DEVICE_LOGIC_ID];
    }
    return instance[deviceLogicID];
}
MemMappingManager::~MemMappingManager()
{
}
// 获取映射后的devVa，先去map找，找不到则新建映射关系
HcclResult MemMappingManager::GetDevVA(s32 deviceLogicID, void *addr, u64 size, void *&devVA)
{
    std::unique_lock<std::mutex> lockMapping(mappedHostToDevMutex_);
    if (!DlHalFunction::GetInstance().DlHalFunctionIsInit()) {
        CHK_RET(DlHalFunction::GetInstance().DlHalFunctionInit());
        HCCL_INFO("[MemMappingManager] hal function init success.");
    }

    CHK_RET(MapMem(deviceLogicID, addr, size, devVA));
    HCCL_INFO("[MemMappingManager][GetDevVA]addr[%p] size[%llu] mapping success, devVa[%p]",
        addr, size, &devVA);
    return HCCL_SUCCESS;
}

HcclResult MemMappingManager::MapMem(s32 deviceLogicID, void *addr, u64 size, void *&devVA)
{
    if (IsRequireMapping(addr, size, devVA)) {
        DevType devType;
        CHK_RET(hrtHalGetDeviceType(deviceLogicID, devType));
        drvRegisterTpye registerTpye = HOST_MEM_MAP_DEV;
        if ((devType == DevType::DEV_TYPE_910B) || (devType == DevType::DEV_TYPE_910_93)) {
            // 910B环境传参要特殊处理
            registerTpye = HOST_MEM_MAP_DEV_PCIE_TH;
            HCCL_INFO("[MemMappingManager][MapMem]hrtHalHostRegister addr[%p], size[%llu], flag[%u], devId[%u]",
                addr, size, HOST_MEM_MAP_DEV_PCIE_TH, deviceLogicID);
            CHK_RET(hrtHalHostRegister(addr, size, HOST_MEM_MAP_DEV_PCIE_TH, deviceLogicID, devVA));
        } else {
            CHK_RET(hrtHalHostRegister(addr, size, HOST_MEM_MAP_DEV, deviceLogicID, devVA));
        }
        HostMappingKey hostMappingKey(reinterpret_cast<u64>(addr), size);
        mappedHostToDevMap_[hostMappingKey].devVA = devVA;
        mappedHostToDevMap_[hostMappingKey].ref.Ref();
        mappedHostToDevMap_[hostMappingKey].registerTpye = registerTpye;
    }
    return HCCL_SUCCESS;
}

bool MemMappingManager::IsRequireMapping(void *addr, u64 size, void *&devVA)
{
    u64 userAddr = reinterpret_cast<u64>(addr);
    u64 userSize = size;
    if (mappedHostToDevMap_.size() == 0) {
        return true;
    }

    auto iter = SearchMappingMap(userAddr, userSize);
    if (iter != mappedHostToDevMap_.end()) {
        u64 tmpDva = reinterpret_cast<u64>(iter->second.devVA) + userAddr - iter->first.addr;
        devVA = reinterpret_cast<void*>(static_cast<uintptr_t>(tmpDva));
        iter->second.ref.Ref();
        return false;
    }

    return true;
}

MemMappingManager::HostMappingIter MemMappingManager::SearchMappingMap(u64 userAddr, u64 userSize)
{
    for (auto iter = mappedHostToDevMap_.begin(); iter != mappedHostToDevMap_.end(); ++iter) {
        if ((userAddr >= iter->first.addr) &&
            (userAddr + userSize <= iter->first.size + iter->first.addr)) {
            return iter;
        }
    }
    return mappedHostToDevMap_.end();
}

// 先去map找内存，找到后引用计数--，减到0后做解映射，从map移除
HcclResult MemMappingManager::ReleaseDevVA(s32 deviceLogicID, void *addr, u64 size)
{
    std::unique_lock<std::mutex> lockMapping(mappedHostToDevMutex_);
    u64 userAddr = reinterpret_cast<u64>(addr);
    auto iter = SearchMappingMap(userAddr, size);
    CHK_PRT_RET((iter == mappedHostToDevMap_.end()),
        HCCL_ERROR("[MemMappingManager][ReleaseDevVA]the memory dereged isn't been reged"), HCCL_E_PARA);
    if (iter->second.ref.Unref() == 0) {
        // 解除内存映射，注册与解注册的 flag 保持一致
        CHK_RET(hrtHalHostUnregisterEx(addr, deviceLogicID, iter->second.registerTpye));
        mappedHostToDevMap_.erase(iter->first);
        HCCL_INFO("[MemMappingManager][ReleaseDevVA]addr[%p], size[%llu] unregister success.", addr, size);
    }
    return HCCL_SUCCESS;
}

}
/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "hcomm_res_mgr.h"
#include "base_comm_res_mgr.h"
#include "hccl_common.h"

// orion 通用平台层单例
#include "hccp_hdc_manager.h"
#include "hccp_peer_manager.h"
#include "hccp_tlv_hdc_manager.h"
#include "rdma_handle_manager.h"
#include "inner_net_dev_manager.h"
#include "socket_handle_manager.h"
#include "host_socket_handle_manager.h"
#include "tp_manager.h"
// orion ccu单例
#include "ccu_component.h"
#include "../../../legacy/unified_platform/ccu/ccu_device/ccu_res_batch_allocator.h"
#include "../../../legacy/unified_platform/ccu/ccu_context/ccu_context_mgr_imp.h"
// 开源开放架构下CCU通信域相关单例
#include "ccu_comp.h"
<<<<<<< HEAD
#include "ccu/ccu_device/ccu_res_batch_allocator.h"
=======
>>>>>>> 2c573c6b ([feature] V2单例架构+删除全局变量逻辑+仅考虑新流程中某些单例统一)
#include "ccu_kernel_mgr.h"

namespace hcomm {

HcommResMgr& HcommResMgr::GetInstance(const uint32_t devicePhyId)
{
    uint32_t devPhyId = devicePhyId;
    if (devPhyId >= MAX_MODULE_DEVICE_NUM) {
        HCCL_WARNING("[HcommResMgr][%s] use the backup device, devPhyId[%u] should be "
            "less than %u.", __func__, devPhyId, MAX_MODULE_DEVICE_NUM);
        devPhyId = MAX_MODULE_DEVICE_NUM; // 使用备份设备
    }

    // 触发legacy Hccl单例初始化，确保生命周期正确
    // legacy单例初始化，暂时无法归入统一单例架构，后续再考虑调整
    Hccl::HccpHdcManager::GetInstance();
    Hccl::HccpPeerManager::GetInstance();
    Hccl::HccpTlvHdcManager::GetInstance();
    Hccl::RdmaHandleManager::GetInstance();
    Hccl::InnerNetDevManager::GetInstance();
    Hccl::SocketHandleManager::GetInstance();
    Hccl::HostSocketHandleManager::GetInstance();
    Hccl::TpManager::GetInstance(devicePhyId);

    Hccl::CcuComponent::GetInstance(devicePhyId);
    Hccl::CcuResBatchAllocator::GetInstance(devicePhyId);
    Hccl::CtxMgrImp::GetInstance(devicePhyId);

    // 开源开放架构下CCU通信域相关单例初始化
    CcuComponent::GetInstance(devPhyId);
    CcuKernelMgr::GetInstance(devPhyId);

    static HcommResMgr hcommResMgrs[MAX_MODULE_DEVICE_NUM + 1];
    HcommResMgr& mgr = hcommResMgrs[devPhyId];

    {
        // 写baseCommRes_过程确保线程安全
        std::lock_guard<std::mutex> lock(mgr.mutex_);
        if (mgr.baseCommRes_ == nullptr) {
        BaseCommResMgr& baseCommResMgr = BaseCommResMgr::Instance();
        mgr.baseCommRes_ = baseCommResMgr.GetOrCreate(devPhyId);
        if (mgr.baseCommRes_ == nullptr) {
            HCCL_ERROR("[HcommResMgr][%s] GetOrCreate BaseCommRes failed for devPhyId=%u",
                __func__, devPhyId);
        }
    }
    }

    return mgr;
}

HcommResMgr::HcommResMgr()
{
}

HcommResMgr::~HcommResMgr()
{
}

} // namespace hcomm
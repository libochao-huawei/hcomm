/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "ccu_drv_handle.h"

#include "log.h"

#include "hccp_tlv.h"
#include "hccp_tlv_hdc_mgr.h"

#include "ccu_res_specs.h"
#include "ccu_pfe_cfg_mgr.h"
#include "ccu_comp.h"
#include "ccu_res_batch_allocator.h"
#include "ccu_kernel_mgr.h"

#include "adapter_rts.h"

namespace hcomm {

static HcclResult HccpRaTlvRequest(const TlvHandle tlvHandle,
    const u32 tlvModuleType, const u32 tlvCcuMsgType)
{
    struct TlvMsg sendMsg {};
    struct TlvMsg recvMsg {};
    sendMsg.type = tlvCcuMsgType;

    HCCL_INFO("[%s] tlvHandle[%p].", __func__, tlvHandle);
    int32_t ret = RaTlvRequest(tlvHandle, tlvModuleType, &sendMsg, &recvMsg);
    if (ret != 0) {
        HCCL_ERROR("[Request][RaTlv]errNo[0x%016llx] ra tlv request fail. "
            "return: ret[%d], module type[%u], message type[%u]",
             HCCL_ERROR_CODE(HcclResult::HCCL_E_NETWORK), tlvModuleType, tlvCcuMsgType);
        return HcclResult::HCCL_E_NETWORK;
    }

    HCCL_INFO("tlv request success, tlv module type[%u], "
        "message type[%u]", tlvModuleType, tlvCcuMsgType);
    return HcclResult::HCCL_SUCCESS;
}

HcclResult CcuDrvHandle::Init()
{
    HCCL_RUN_INFO("[CcuDrvHandle][%s], deviceLogicId: %d", __func__, devLogicId_);
    CHK_RET(hrtGetDevicePhyIdByIndex(static_cast<uint32_t>(devLogicId_), devPhyId_));
    // 初始化CCU平台层能力，有时序要求
    // 当前走进A5通信域，暂时不需要主动拉起
    auto &tlvHdcMgr = HccpTlvHdcMgr::GetInstance(devPhyId_);
    CHK_RET(tlvHdcMgr.Init());
    tlvHandle_ = tlvHdcMgr.GetHandle();
    CHK_PTR_NULL(tlvHandle_);

    CHK_RET(HccpRaTlvRequest(tlvHandle_, TLV_MODULE_TYPE_CCU, MSG_TYPE_CCU_INIT));
    CHK_RET(CcuResSpecifications::GetInstance(devLogicId_).Init());
    CHK_RET(CcuPfeCfgMgr::GetInstance(devLogicId_).Init());
    CHK_RET(CcuComponent::GetInstance(devLogicId_).Init());
    CHK_RET(CcuResBatchAllocator::GetInstance(devLogicId_).Init());
    // CHK_RET(CcuKernelMgr::GetInstance(devLogicId_).Init());

    return HcclResult::HCCL_SUCCESS;
}

HcclResult CcuDrvHandle::Deinit()
{
    // 释放流程不打断，不抛异常，尽量尝试释放所有资源
    HCCL_RUN_INFO("[CcuDrvHandle] start to deinit ccu driver, deviceLogicId[%d].", devLogicId_);
    (void)CcuKernelMgr::GetInstance(devLogicId_).Deinit();
    (void)CcuResBatchAllocator::GetInstance(devLogicId_).Deinit();
    (void)CcuComponent::GetInstance(devLogicId_).Deinit();
    (void)CcuPfeCfgMgr::GetInstance(devLogicId_).Deinit();
    (void)CcuResSpecifications::GetInstance(devLogicId_).Deinit();

    (void)HccpRaTlvRequest(tlvHandle_, TLV_MODULE_TYPE_CCU, MSG_TYPE_CCU_UNINIT);
    return HcclResult::HCCL_SUCCESS;
}

CcuDrvHandle::~CcuDrvHandle()
{
    (void)Deinit();
}

} // namespace hcomm
/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "typical_qp_manager.h"
#include <map>
#include <memory>
#include "externalinput.h"
#include "adapter_rts_common.h"
#include "adapter_hccp_common.h"
#include "network_manager_pub.h"
#include "rdma_resource_manager.h"

namespace hccl {
TypicalQpManager::TypicalQpManager()
{
}

TypicalQpManager::~TypicalQpManager()
{
    std::unique_lock<std::mutex> lock(qpMutex_);
    for (auto& item : qpMap_) {
        if (item.second.second == nullptr) {
            continue;
        }
        HrtRaQpDestroy(item.second.second);
    }
    qpMap_.clear();
    lock.unlock();
}

TypicalQpManager& TypicalQpManager::GetInstance()
{
    static TypicalQpManager qpInstance[MAX_MODULE_DEVICE_NUM + 1];
    s32 deviceLogicId = INVALID_INT;
    HcclResult ret = hrtGetDevice(&deviceLogicId);
    if (ret == HCCL_SUCCESS && (static_cast<u32>(deviceLogicId) < MAX_MODULE_DEVICE_NUM)) {
        HCCL_INFO("[TypicalQpManager::GetInstance]deviceLogicID[%d]", deviceLogicId);
        return qpInstance[deviceLogicId];
    }
    HCCL_WARNING("[TypicalQpManager::GetInstance]deviceLogicID[%d] is invalid, ret[%d]", deviceLogicId, ret);
    return qpInstance[MAX_MODULE_DEVICE_NUM];
}

HcclResult TypicalQpManager::CreateQp(struct TypicalQp& qpInfo)
{
    HcclResult ret = HCCL_SUCCESS;
    QpHandle qpHandle = nullptr;
    CHK_RET(RdmaResourceManager::GetInstance().GetRdmaHandle(rdmaHandle_));
    CHK_PTR_NULL(rdmaHandle_);
    std::unique_lock<std::mutex> lock(qpMutex_);
    ret = hrtRaTypicalQpCreate(rdmaHandle_, QP_FLAG_RC, OPBASE_QP_MODE, &qpInfo, qpHandle);
    CHK_PRT_RET(ret != HCCL_SUCCESS, HCCL_ERROR("[TypicalQpManager][CreateQp] Create qp failed."), HCCL_E_INTERNAL);
    qpMap_.insert(std::make_pair(qpInfo.qpn, std::make_pair(qpInfo, qpHandle)));
    return HCCL_SUCCESS;
}

HcclResult TypicalQpManager::CreateQp(struct TypicalQp& qpInfo, const QpConfigInfo& qpConfig)
{
    HcclResult ret = HCCL_SUCCESS;
    QpHandle qpHandle = nullptr;
    CHK_RET(RdmaResourceManager::GetInstance().GetRdmaHandle(rdmaHandle_));
    CHK_PTR_NULL(rdmaHandle_);
    std::unique_lock<std::mutex> lock(qpMutex_);
    ret = CreateQpWithDepthConfig(rdmaHandle_, OPBASE_QP_MODE, qpConfig, qpHandle, qpInfo);
    CHK_PRT_RET(ret != HCCL_SUCCESS, HCCL_ERROR("[TypicalQpManager][CreateQp] Create qp failed."), HCCL_E_INTERNAL);
    qpMap_.insert(std::make_pair(qpInfo.qpn, std::make_pair(qpInfo, qpHandle)));
    return HCCL_SUCCESS;
}

HcclResult TypicalQpManager::ModifyQp(struct TypicalQp& localQpInfo, struct TypicalQp& remoteQpInfo)
{
    CHK_PRT_RET((localQpInfo.qpn == 0 || remoteQpInfo.qpn == 0),
        HCCL_ERROR("[TypicalQpManager][ModifyQp] the qpinfo is wrong, qpn is 0."), HCCL_E_PARA);
    QpHandle qpHandle = nullptr;
    CHK_RET(GetQpHandleByQpn(localQpInfo.qpn, qpHandle));
    CHK_PTR_NULL(qpHandle);
    CHK_RET(SetQpRdmaRetryCfg(localQpInfo));
    std::unique_lock<std::mutex> lock(qpMutex_);
    HcclResult ret = hrtRaTypicalQpModify(qpHandle, &localQpInfo, &remoteQpInfo);
    CHK_PRT_RET(ret != HCCL_SUCCESS, HCCL_ERROR("[TypicalQpManager][ModifyQp] Modify qp failed."), HCCL_E_INTERNAL);

    return HCCL_SUCCESS;
}

HcclResult TypicalQpManager::DestroyQp(struct TypicalQp& qpInfo)
{
    CHK_PRT_RET((qpInfo.qpn == 0), HCCL_ERROR("[TypicalQpManager][DestroyQp] The qpinfo is wrong, qpn is 0."),
        HCCL_E_PARA);
    QpHandle qpHandle;
    CHK_RET(GetQpHandleByQpn(qpInfo.qpn, qpHandle));
    CHK_PTR_NULL(qpHandle);
    std::unique_lock<std::mutex> lock(qpMutex_);
    HcclResult ret = HrtRaQpDestroy(qpHandle);
    CHK_PRT_RET(ret != HCCL_SUCCESS, HCCL_ERROR("[TypicalQpManager][DestroyQp] Destroy qp failed."), HCCL_E_INTERNAL);
    qpMap_.erase(qpInfo.qpn);
    return HCCL_SUCCESS;
}

HcclResult TypicalQpManager::SetQpRdmaRetryCfg(struct TypicalQp& qpInfo)
{
    qpInfo.retryCnt = GetExternalInputRdmaRetryCnt();
    qpInfo.retryTime = GetExternalInputRdmaTimeOut();
    HCCL_INFO("[TypicalQpManager][SetQpCreateBaseInfo] Qpinfo is set, tc is %u, sl is %u, retry cnt is %u, "\
        "retry time is %u", qpInfo.tc, qpInfo.sl, qpInfo.retryCnt, qpInfo.retryTime);
    return HCCL_SUCCESS;
}

HcclResult TypicalQpManager::GetQpHandleByQpn(u32 qpn, QpHandle& qpHandle)
{
    HCCL_DEBUG("[TypicalQpManager][GetQpHandleByQpn] Get qpHandle by qpn[%u]", qpn);
    CHK_RET(RdmaResourceManager::GetInstance().GetRdmaHandle(rdmaHandle_));
    CHK_PTR_NULL(rdmaHandle_);
    std::unique_lock<std::mutex> lock(qpMutex_);
    auto it = qpMap_.find(qpn);
    CHK_PRT_RET((it == qpMap_.end()),
        HCCL_ERROR("[TypicalQpManager][GetQpHandleByQpn] Qpn is not found"), HCCL_E_NOT_FOUND);
    qpHandle = it->second.second;
    CHK_PRT_RET((qpHandle == nullptr),
        HCCL_ERROR("[TypicalQpManager][GetQpHandleByQpn] Get Qphandle failed, qphandle is nullptr. qpn is %u", qpn),
        HCCL_E_NOT_FOUND);
    return HCCL_SUCCESS;
}
} // namespace hccl

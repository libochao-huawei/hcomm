/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "aclgraph_callback.h"
#include "stream_utils.h"

namespace hccl {

void AclgraphDestroyCallback(void *fnData)
{
    AclgraphDestroyCallbackParam *callbackParam = static_cast<AclgraphDestroyCallbackParam *>(fnData);
    if (callbackParam == nullptr) {
        HCCL_ERROR("[%s] callbackParam ptr is NULL", __func__);
        return;
    }

    HCCL_INFO("[%s] Entry modelID[%llu] CleanCaptureRes", __func__, callbackParam->modelId);
    HcclResult ret = AclgraphCallback::GetInstance().CleanCaptureRes(callbackParam->modelId);
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("[%s] modelID[%llu] CleanCaptureRes failed", __func__, callbackParam->modelId);
    }
}

AclgraphCallback &AclgraphCallback::GetInstance()
{
    static AclgraphCallback aclgraphCallback;
    return aclgraphCallback;
}

AclgraphCallback::~AclgraphCallback()
{
    std::lock_guard<std::mutex> lock(resMutex_);
    captureResMap_.clear();
    captureCallbackParamMap_.clear();
}

void AclgraphCallback::CleanCaptureRes(HcclCommunicator *communicator)
{
    if (communicator == nullptr) {
        return;
    }

    std::lock_guard<std::mutex> lock(resMutex_);
    for (auto &modelIt : captureResMap_) {
        if (modelIt.second.find(communicator) != modelIt.second.end()) {
            modelIt.second.erase(communicator);
        }
    }

    HCCL_INFO("[%s] communicator[%p] resource release success", __func__, communicator);
}

HcclResult AclgraphCallback::CleanCaptureRes(u64 modelId)
{
    HcclResult ret;

    std::lock_guard<std::mutex> lock(resMutex_);
    auto modelIt  = captureResMap_.find(modelId);
    if (modelIt  == captureResMap_.end()) {
        HCCL_ERROR("[%s] modelID[%llu] is not record", __func__, modelId);
        return HCCL_E_NOT_FOUND;
    }

    bool isResourceReleaseFailed = false;
    for (auto &commIt : modelIt->second) {
        // host 端逐 tag 清自己 resMap_（操作 host 进程内状态，无 launch 开销）
        for (auto &newTag : commIt.second) {
            ret = commIt.first->ClearOpResource(newTag);
            if (ret != HCCL_SUCCESS) {
                HCCL_ERROR("[%s] modelID[%llu] tag[%s] resource release fail, ret[%d]",
                    __func__, modelId, newTag.c_str(), ret);
                isResourceReleaseFailed = true;
            }
            HCCL_DEBUG("[%s] modelID[%llu] tag[%s] host resource release finish", __func__, modelId, newTag.c_str());
        }
        // aicpu 端整批一次 launch（同 communicator 内所有 tag 打包到一个 payload，由 host 侧分批限流），
        // 避免逐 tag launch+sync 把 destroy 路径延迟放大 N 倍。
        // 失败不阻断流程：aicpu 端清理失败时下一轮 destroy callback 仍可重试。
        HcclResult aicpuRet = commIt.first->AicpuKfcClearOpResLaunch(commIt.second);
        if (aicpuRet != HCCL_SUCCESS) {
            HCCL_RUN_WARNING("[%s] modelID[%llu] aicpu batch cleanup dispatch fail, tagCount[%zu] ret[%d]",
                __func__, modelId, commIt.second.size(), aicpuRet);
        }
    }

    captureResMap_.erase(modelId);
    captureCallbackParamMap_.erase(modelId);
    if (isResourceReleaseFailed) {
        HCCL_RUN_WARNING("[%s] modelID[%llu] resource release partially failed", __func__, modelId);
    } else {
        HCCL_INFO("[%s] modelID[%llu] resource release success", __func__, modelId);
    }

    return isResourceReleaseFailed ? HCCL_E_INTERNAL : HCCL_SUCCESS;
}

// 记录aclgraph下发的所有tag, 首次记录时注册aclgraph销毁回调
HcclResult AclgraphCallback::InsertNewTagToCaptureResMap(HcclCommunicator *communicator,
    const std::string &newTag, const OpParam &opParam)
{
    CHK_PTR_NULL(communicator);
    aclmdlRI rtModel = nullptr;
    bool isCapture = false;
    u64 modelId = 0;

    CHK_RET(GetStreamCaptureInfo(opParam.stream.ptr(), rtModel, isCapture));
    CHK_PTR_NULL(rtModel);
    CHK_RET(GetModelId(rtModel, modelId));

    std::lock_guard<std::mutex> lock(resMutex_);
    if (captureResMap_.find(modelId) == captureResMap_.end()) {
        captureCallbackParamMap_[modelId].modelId = modelId;
        aclError aclRet = aclmdlRIDestroyRegisterCallback(rtModel, AclgraphDestroyCallback,
            static_cast<void *>(&captureCallbackParamMap_[modelId]));
        CHK_PRT_RET(aclRet != ACL_SUCCESS, HCCL_ERROR("[%s] aclmdlRIDestroyRegisterCallback fail, modelId[%llu]",
            __func__, modelId), HCCL_E_RUNTIME);
        HCCL_INFO("[%s] aclmdlRIDestroyRegisterCallback success modelID[%llu]", __func__, modelId);
    }
    captureResMap_[modelId][communicator].insert(newTag);
    HCCL_DEBUG("[%s] captureResMap insert tag[%s] to modelID[%llu]", __func__, newTag.c_str(), modelId);

    return HCCL_SUCCESS;
}
} // namespace hccl
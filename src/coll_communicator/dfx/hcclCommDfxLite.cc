/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "hcclCommDfxLite.h"
#include "hccl_common.h"

namespace hccl {
// HcclCommDfxLite构造函数实现
HcclCommDfxLite::HcclCommDfxLite() {
}

// HcclCommDfxLite初始化流程 - 修改为返回HcclResult类型
HcclResult HcclCommDfxLite::Init(u32 deviceId, const std::string& commTag) {
    HCCL_INFO("[HcclCommDfxLite][Init] Init begin deviceId[%u], commTag[%s]", deviceId, commTag.c_str());
    deviceId_ = deviceId;
    commTag_ = commTag;
    /*1. 如果mirrorTaskManagerLite_为空，则创建新的MirrorTaskManager
    注意：实际实现中应该避免这种情况，CommunicatorImplLite应该传入已经存在的MirrorTaskManager*/
    EXCEPTION_CATCH(mirrorTaskManagerLite_ = std::make_unique<Hccl::MirrorTaskManagerLite>(), return HCCL_E_PTR);
    auto getChannelRemoteRankId = [this](u64 handle) { return this->GetChannelRemoteRankId(handle); };
    mirrorTaskManagerLite_->RegGetRemoteRankCallBack(getChannelRemoteRankId);

    // 2. 创建Profiling管理类
    EXCEPTION_CATCH(profilingImpl_ = std::make_unique<HcclCommProfilingLite>(deviceId_, mirrorTaskManagerLite_.get()), return HCCL_E_PTR);

    // 3. 注册回调到单例
    addTaskCallback_ = [this](u32 streamId, u32 taskId, const Hccl::TaskParam &taskParam, u64 handle) {
        return this->mirrorTaskManagerLite_->AddTaskInfo(streamId, taskId, taskParam, handle);
    };
    return HCCL_SUCCESS; // 初始化成功返回成功码
}

// HcclCommDfxLite接口实现 - 修改为返回HcclResult类型
HcclResult HcclCommDfxLite::SetCurrDfxOpInfo(std::shared_ptr<Hccl::DfxOpInfo> dfxOpInfo)
{
    CHK_PTR_NULL(dfxOpInfo);
    CHK_RET(mirrorTaskManagerLite_->SetCurrDfxOpInfo(dfxOpInfo));
    if (Hccl::ProfilingHandlerLite::GetInstance().GetProfL1State() 
        || Hccl::ProfilingHandlerLite::GetInstance().GetProfL0State()) {
        Hccl::ProfilingHandlerLite::GetInstance().SetCachedGroupName(*dfxOpInfo);
    }
    return HCCL_SUCCESS;
}

HcclResult HcclCommDfxLite::ReportAllTasks() {
    profilingImpl_->ReportAllTasks();
    return HCCL_SUCCESS;
}

HcclResult HcclCommDfxLite::UpdateProfStat() {
    profilingImpl_->UpdateProfStat();
    return HCCL_SUCCESS;
}

void HcclCommDfxLite::AddChannelRemoteRankId(u64 handle, u32 remoteRankId) {
    HCCL_INFO("[%s] commTag[%s], handle[%lu], remoteRankId[%u]", __func__, commTag_.c_str(), handle, remoteRankId);
    channelRemoteRankIdLite_[handle] = remoteRankId;
}

u32 HcclCommDfxLite::GetChannelRemoteRankId(u64 handle) {
    auto it = channelRemoteRankIdLite_.find(handle);
    if (UNLIKELY(it == channelRemoteRankIdLite_.end())) {
        HCCL_ERROR("[%s]handle[%lu] not found, commTag[%s]", __func__, handle, commTag_.c_str());
        return INVALID_UINT;
    }
    return it->second;
}

Hccl::MirrorTaskManagerLite* HcclCommDfxLite::GetMirrorTaskManagerLite() const {
    return mirrorTaskManagerLite_.get();
}
}
/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "base_comm_res.h"
#include "hccl_common.h"
#include "sub_resource_mgrs.h"

namespace hcomm {

BaseCommRes::BaseCommRes(uint32_t devPhyId)
    : devPhyId_(devPhyId),
      state_(State::kReady)
{
    HCCL_INFO("[BaseCommRes][%s] created for devPhyId=%u", __func__, devPhyId);
}

BaseCommRes::~BaseCommRes()
{
    DestroyAll();
}

bool BaseCommRes::TransitionState(State from, State to)
{
    State expected = from;
    if (state_.compare_exchange_strong(expected, to)) {
        HCCL_INFO("[BaseCommRes][%s] state transition %d -> %d for devPhyId=%u",
            __func__, static_cast<int>(from), static_cast<int>(to), devPhyId_);
        return true;
    }
    HCCL_ERROR("[BaseCommRes][%s] state transition failed: expected %d, actual %d for devPhyId=%u",
        __func__, static_cast<int>(from), static_cast<int>(state_.load()), devPhyId_);
    return false;
}

HcclResult BaseCommRes::DestroyAll()
{
    std::lock_guard<std::mutex> lock(mutex_);

    const State current = state_.load();
    if (current == State::kDestroyed) {
        return HCCL_SUCCESS;
    }
    if (current == State::kDestroying) {
        return HCCL_E_BUSY;
    }

    if (!TransitionState(State::kReady, State::kDestroying)) {
        HCCL_ERROR("[BaseCommRes][%s] failed to enter destroying from ready", __func__);
        return HCCL_E_BUSY;
    }

    HcclResult ret = DestroyInOrder();

    if (!TransitionState(State::kDestroying, State::kDestroyed)) {
        HCCL_ERROR("[BaseCommRes][%s] failed kDestroying -> kDestroyed", __func__);
        state_.store(State::kDestroyed);
    }
    return ret;
}

HcclResult BaseCommRes::DestroyInOrder()
{
    HcclResult finalResult = HCCL_SUCCESS;

    if (channelsMgr_) {
        auto ret = channelsMgr_->DestroyAll();
        if (ret != HCCL_SUCCESS) finalResult = ret;
    }

    if (threadsMgr_) {
        auto ret = threadsMgr_->DestroyAll();
        if (ret != HCCL_SUCCESS) finalResult = ret;
    }

    if (ccuInstancesMgr_) {
        auto ret = ccuInstancesMgr_->DestroyAll();
        if (ret != HCCL_SUCCESS) finalResult = ret;
    }

    if (endpointsMgr_) {
        auto ret = endpointsMgr_->DestroyAllMem();
        if (ret != HCCL_SUCCESS) finalResult = ret;
    }

    if (hdcProcess_) {
        auto ret = hdcProcess_->Deinit();
        if (ret != HCCL_SUCCESS) finalResult = ret;
    }

    if (endpointsMgr_) {
        auto ret = endpointsMgr_->DestroyAll();
        if (ret != HCCL_SUCCESS) finalResult = ret;
    }

    return finalResult;
}

EndpointsMgr* BaseCommRes::GetEndpointsMgr()
{
    std::lock_guard<std::mutex> lock(mutex_);

    const State s = state_.load();
    if (s == State::kDestroyed || s == State::kDestroying) {
        return nullptr;
    }

    if (endpointsMgr_ == nullptr) {
        auto ret = LazyInitEndpointsMgr();
        if (ret != HCCL_SUCCESS) {
            return nullptr;
        }
    }
    return endpointsMgr_.get();
}

ChannelsMgr* BaseCommRes::GetChannelsMgr()
{
    std::lock_guard<std::mutex> lock(mutex_);

    const State s = state_.load();
    if (s == State::kDestroyed || s == State::kDestroying) {
        return nullptr;
    }

    if (channelsMgr_ == nullptr) {
        auto ret = LazyInitChannelsMgr();
        if (ret != HCCL_SUCCESS) {
            return nullptr;
        }
    }
    return channelsMgr_.get();
}

ThreadsMgr* BaseCommRes::GetThreadsMgr()
{
    std::lock_guard<std::mutex> lock(mutex_);

    const State s = state_.load();
    if (s == State::kDestroyed || s == State::kDestroying) {
        return nullptr;
    }

    if (threadsMgr_ == nullptr) {
        auto ret = LazyInitThreadsMgr();
        if (ret != HCCL_SUCCESS) {
            return nullptr;
        }
    }
    return threadsMgr_.get();
}

CcuInstancesMgr* BaseCommRes::GetCcuInstancesMgr()
{
    std::lock_guard<std::mutex> lock(mutex_);

    const State s = state_.load();
    if (s == State::kDestroyed || s == State::kDestroying) {
        return nullptr;
    }

    if (ccuInstancesMgr_ == nullptr) {
        auto ret = LazyInitCcuInstancesMgr();
        if (ret != HCCL_SUCCESS) {
            return nullptr;
        }
    }
    return ccuInstancesMgr_.get();
}

HdcProcess* BaseCommRes::GetHdcProcess()
{
    std::lock_guard<std::mutex> lock(mutex_);

    const State s = state_.load();
    if (s == State::kDestroyed || s == State::kDestroying) {
        return nullptr;
    }

    if (hdcProcess_ == nullptr) {
        auto ret = LazyInitHdcProcess();
        if (ret != HCCL_SUCCESS) {
            return nullptr;
        }
    }
    return hdcProcess_.get();
}

HcclResult BaseCommRes::LazyInitEndpointsMgr()
{
    if (endpointsMgr_ != nullptr) {
        return HCCL_SUCCESS;
    }
    endpointsMgr_ = std::make_unique<EndpointsMgr>();
    if (endpointsMgr_ == nullptr) {
        HCCL_ERROR("[BaseCommRes][%s] make_unique EndpointsMgr failed", __func__);
        return HCCL_E_MEMORY;
    }
    HCCL_INFO("[BaseCommRes][%s] EndpointsMgr created for devPhyId=%u", __func__, devPhyId_);
    return HCCL_SUCCESS;
}

HcclResult BaseCommRes::LazyInitChannelsMgr()
{
    if (channelsMgr_ != nullptr) {
        return HCCL_SUCCESS;
    }
    channelsMgr_ = std::make_unique<ChannelsMgr>();
    if (channelsMgr_ == nullptr) {
        HCCL_ERROR("[BaseCommRes][%s] make_unique ChannelsMgr failed", __func__);
        return HCCL_E_MEMORY;
    }
    HCCL_INFO("[BaseCommRes][%s] ChannelsMgr created for devPhyId=%u", __func__, devPhyId_);
    return HCCL_SUCCESS;
}

HcclResult BaseCommRes::LazyInitThreadsMgr()
{
    if (threadsMgr_ != nullptr) {
        return HCCL_SUCCESS;
    }
    threadsMgr_ = std::make_unique<ThreadsMgr>();
    if (threadsMgr_ == nullptr) {
        HCCL_ERROR("[BaseCommRes][%s] make_unique ThreadsMgr failed", __func__);
        return HCCL_E_MEMORY;
    }
    HCCL_INFO("[BaseCommRes][%s] ThreadsMgr created for devPhyId=%u", __func__, devPhyId_);
    return HCCL_SUCCESS;
}

HcclResult BaseCommRes::LazyInitCcuInstancesMgr()
{
    if (ccuInstancesMgr_ != nullptr) {
        return HCCL_SUCCESS;
    }
    ccuInstancesMgr_ = std::make_unique<CcuInstancesMgr>();
    if (ccuInstancesMgr_ == nullptr) {
        HCCL_ERROR("[BaseCommRes][%s] make_unique CcuInstancesMgr failed", __func__);
        return HCCL_E_MEMORY;
    }
    HCCL_INFO("[BaseCommRes][%s] CcuInstancesMgr created for devPhyId=%u", __func__, devPhyId_);
    return HCCL_SUCCESS;
}

HcclResult BaseCommRes::LazyInitHdcProcess()
{
    if (hdcProcess_ != nullptr) {
        return HCCL_SUCCESS;
    }
    hdcProcess_ = std::make_unique<HdcProcess>();
    if (hdcProcess_ == nullptr) {
        HCCL_ERROR("[BaseCommRes][%s] make_unique HdcProcess failed", __func__);
        return HCCL_E_MEMORY;
    }
    HCCL_INFO("[BaseCommRes][%s] HdcProcess created for devPhyId=%u", __func__, devPhyId_);
    return HCCL_SUCCESS;
}

BaseCommRes::ResourceCount BaseCommRes::GetResourceCount() const
{
    ResourceCount count;
    if (endpointsMgr_ != nullptr) {
        count.endpointCount = endpointsMgr_->GetEndpointCount();
    }
    if (channelsMgr_ != nullptr) {
        count.channelCount = channelsMgr_->GetChannelCount();
    }
    if (threadsMgr_ != nullptr) {
        count.threadCount = threadsMgr_->GetThreadCount();
    }
    if (ccuInstancesMgr_ != nullptr) {
        count.ccuInstanceCount = ccuInstancesMgr_->GetCcuInstanceCount();
    }
    return count;
}

} // namespace hcomm
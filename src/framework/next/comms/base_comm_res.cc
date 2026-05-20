/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for the details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "base_comm_res.h"
#include "hccl_common.h"
#include "sub_resource_mgrs.h"
#include "endpoint_pairs/sockets/socket_process.h"
#include "common/tp_mgr.h"
#include "common/hccp_tlv_hdc_mgr.h"
#include "endpoints/dfx/endpoint_monitor.h"

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

    if (endpointMonitor_) {
        endpointMonitor_.reset();
    }

    if (socketProcess_) {
        socketProcess_.reset();
    }

    if (tpMgr_) {
        tpMgr_.reset();
    }

    if (hccpTlvHdcMgr_) {
        hccpTlvHdcMgr_.reset();
    }

    if (endpointsMgr_) {
        auto ret = endpointsMgr_->DestroyAllMem();
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

SocketProcess* BaseCommRes::GetSocketProcess()
{
    std::lock_guard<std::mutex> lock(mutex_);

    const State s = state_.load();
    if (s == State::kDestroyed || s == State::kDestroying) {
        return nullptr;
    }

    if (socketProcess_ == nullptr) {
        auto ret = LazyInitSocketProcess();
        if (ret != HCCL_SUCCESS) {
            return nullptr;
        }
    }
    return socketProcess_.get();
}

TpMgr* BaseCommRes::GetTpMgr()
{
    std::lock_guard<std::mutex> lock(mutex_);

    const State s = state_.load();
    if (s == State::kDestroyed || s == State::kDestroying) {
        return nullptr;
    }

    if (tpMgr_ == nullptr) {
        auto ret = LazyInitTpMgr();
        if (ret != HCCL_SUCCESS) {
            return nullptr;
        }
    }
    return tpMgr_.get();
}

HccpTlvHdcMgr* BaseCommRes::GetHccpTlvHdcMgr()
{
    std::lock_guard<std::mutex> lock(mutex_);

    const State s = state_.load();
    if (s == State::kDestroyed || s == State::kDestroying) {
        return nullptr;
    }

    if (hccpTlvHdcMgr_ == nullptr) {
        auto ret = LazyInitHccpTlvHdcMgr();
        if (ret != HCCL_SUCCESS) {
            return nullptr;
        }
    }
    return hccpTlvHdcMgr_.get();
}

EndpointMonitor* BaseCommRes::GetEndpointMonitor()
{
    std::lock_guard<std::mutex> lock(mutex_);

    const State s = state_.load();
    if (s == State::kDestroyed || s == State::kDestroying) {
        return nullptr;
    }

    if (endpointMonitor_ == nullptr) {
        auto ret = LazyInitEndpointMonitor();
        if (ret != HCCL_SUCCESS) {
            return nullptr;
        }
    }
    return endpointMonitor_.get();
}

HcclResult BaseCommRes::LazyInitEndpointsMgr()
{
    if (endpointsMgr_ != nullptr) {
        return HCCL_SUCCESS;
    }
    EXECEPTION_CATCH(endpointsMgr_ = std::make_unique<EndpointsMgr>());
    if (endpointsMgr_ == nullptr) {
        HCCL_ERROR("[BaseCommRes][%s] make_unique EndpointsMgr failed", __func__);
        return HCCL_E_INTERNAL;
    }
    HCCL_INFO("[BaseCommRes][%s] EndpointsMgr created for devPhyId=%u", __func__, devPhyId_);
    return HCCL_SUCCESS;
}

HcclResult BaseCommRes::LazyInitChannelsMgr()
{
    if (channelsMgr_ != nullptr) {
        return HCCL_SUCCESS;
    }
    EXECEPTION_CATCH(channelsMgr_ = std::make_unique<ChannelsMgr>());
    if (channelsMgr_ == nullptr) {
        HCCL_ERROR("[BaseCommRes][%s] make_unique ChannelsMgr failed", __func__);
        return HCCL_E_INTERNAL;
    }
    HCCL_INFO("[BaseCommRes][%s] ChannelsMgr created for devPhyId=%u", __func__, devPhyId_);
    return HCCL_SUCCESS;
}

HcclResult BaseCommRes::LazyInitThreadsMgr()
{
    if (threadsMgr_ != nullptr) {
        return HCCL_SUCCESS;
    }
    EXECEPTION_CATCH(threadsMgr_ = std::make_unique<ThreadsMgr>());
    if (threadsMgr_ == nullptr) {
        HCCL_ERROR("[BaseCommRes][%s] make_unique ThreadsMgr failed", __func__);
        return HCCL_E_INTERNAL;
    }
    HCCL_INFO("[BaseCommRes][%s] ThreadsMgr created for devPhyId=%u", __func__, devPhyId_);
    return HCCL_SUCCESS;
}

HcclResult BaseCommRes::LazyInitSocketProcess()
{
    if (socketProcess_ != nullptr) {
        return HCCL_SUCCESS;
    }
    EXECEPTION_CATCH(socketProcess_ = std::make_unique<SocketProcess>());
    if (socketProcess_ == nullptr) {
        HCCL_ERROR("[BaseCommRes][%s] make_unique SocketProcess failed", __func__);
        return HCCL_E_INTERNAL;
    }
    auto ret = socketProcess_->Init(devPhyId_);
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("[BaseCommRes][%s] SocketProcess::Init failed", __func__);
        return ret;
    }
    HCCL_INFO("[BaseCommRes][%s] SocketProcess created and initialized for devPhyId=%u", __func__, devPhyId_);
    return HCCL_SUCCESS;
}

HcclResult BaseCommRes::LazyInitTpMgr()
{
    if (tpMgr_ != nullptr) {
        return HCCL_SUCCESS;
    }
    EXECEPTION_CATCH(tpMgr_ = std::make_unique<TpMgr>());
    if (tpMgr_ == nullptr) {
        HCCL_ERROR("[BaseCommRes][%s] make_unique TpMgr failed", __func__);
        return HCCL_E_INTERNAL;
    }
    HCCL_INFO("[BaseCommRes][%s] TpMgr created for devPhyId=%u", __func__, devPhyId_);
    return HCCL_SUCCESS;
}

HcclResult BaseCommRes::LazyInitHccpTlvHdcMgr()
{
    if (hccpTlvHdcMgr_ != nullptr) {
        return HCCL_SUCCESS;
    }
    EXECEPTION_CATCH(hccpTlvHdcMgr_ = std::make_unique<HccpTlvHdcMgr>());
    if (hccpTlvHdcMgr_ == nullptr) {
        HCCL_ERROR("[BaseCommRes][%s] make_unique HccpTlvHdcMgr failed", __func__);
        return HCCL_E_INTERNAL;
    }
    auto ret = hccpTlvHdcMgr_->Init(devPhyId_);
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("[BaseCommRes][%s] HccpTlvHdcMgr::Init failed", __func__);
        return ret;
    }
    HCCL_INFO("[BaseCommRes][%s] HccpTlvHdcMgr created and initialized for devPhyId=%u", __func__, devPhyId_);
    return HCCL_SUCCESS;
}

HcclResult BaseCommRes::LazyInitEndpointMonitor()
{
    if (endpointMonitor_ != nullptr) {
        return HCCL_SUCCESS;
    }
    EXECEPTION_CATCH(endpointMonitor_ = std::make_unique<EndpointMonitor>());
    if (endpointMonitor_ == nullptr) {
        HCCL_ERROR("[BaseCommRes][%s] make_unique EndpointMonitor failed", __func__);
        return HCCL_E_INTERNAL;
    }
    HCCL_INFO("[BaseCommRes][%s] EndpointMonitor created for devPhyId=%u", __func__, devPhyId_);
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
    return count;
}

} // namespace hcomm
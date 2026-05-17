/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef BASE_COMM_RES_H
#define BASE_COMM_RES_H

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "hccl_types.h"
#include "sub_resource_mgrs.h"

namespace hcomm {

class BaseCommRes {
public:
    enum class State {
        kCreated,
        kInitializing,
        kReady,
        kDestroying,
        kDestroyed
    };

    struct ResourceCount {
        size_t endpointCount = 0;
        size_t channelCount = 0;
        size_t threadCount = 0;
        size_t ccuInstanceCount = 0;
    };

    explicit BaseCommRes(uint32_t devPhyId);
    ~BaseCommRes();

    EndpointsMgr* GetEndpointsMgr();
    ChannelsMgr* GetChannelsMgr();
    ThreadsMgr* GetThreadsMgr();
    CcuInstancesMgr* GetCcuInstancesMgr();
    HdcProcess* GetHdcProcess();

    HcclResult DestroyAll();

    uint32_t GetDevicePhyId() const { return devPhyId_; }

    State GetState() const { return state_.load(); }
    bool IsReady() const { return state_.load() == State::kReady; }
    bool IsDestroyed() const { return state_.load() == State::kDestroyed; }

    ResourceCount GetResourceCount() const;

private:
    friend class BaseCommResMgr;
    friend class std::unique_ptr<BaseCommRes>;

    HcclResult DestroyInOrder();

    HcclResult LazyInitEndpointsMgr();
    HcclResult LazyInitChannelsMgr();
    HcclResult LazyInitThreadsMgr();
    HcclResult LazyInitCcuInstancesMgr();
    HcclResult LazyInitHdcProcess();

    bool TransitionState(State from, State to);

private:
    uint32_t devPhyId_;
    std::atomic<State> state_{State::kCreated};

    std::unique_ptr<EndpointsMgr> endpointsMgr_;
    std::unique_ptr<ChannelsMgr> channelsMgr_;
    std::unique_ptr<ThreadsMgr> threadsMgr_;
    std::unique_ptr<CcuInstancesMgr> ccuInstancesMgr_;
    std::unique_ptr<HdcProcess> hdcProcess_;

    std::mutex mutex_;
    std::atomic<bool> destroyed_{false};

    std::atomic<size_t> endpointCount_{0};
    std::atomic<size_t> channelCount_{0};
    std::atomic<size_t> threadCount_{0};
    std::atomic<size_t> ccuInstanceCount_{0};
};

} // namespace hcomm

#endif // BASE_COMM_RES_H

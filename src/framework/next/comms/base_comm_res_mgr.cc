/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "base_comm_res_mgr.h"

#include "base_comm_res.h"
#include "log.h"

namespace hcomm {

BaseCommResMgr& BaseCommResMgr::Instance()
{
    static BaseCommResMgr instance;
    return instance;
}

BaseCommResMgr::BaseCommResMgr()
{
    HCCL_INFO("[BaseCommResMgr][%s] constructor called", __func__);
}

BaseCommResMgr::~BaseCommResMgr()
{
    HCCL_INFO("[BaseCommResMgr][%s] destructor called, destroying all resources", __func__);
    DestroyAll();
}

BaseCommRes* BaseCommResMgr::GetOrCreate(uint32_t devPhyId)
{
    std::lock_guard<std::mutex> lock(mutex_);
    // 先检查是否存在, 如果存在则直接返回
    auto it = baseCommResMap_.find(devPhyId);
    if (it != baseCommResMap_.end()) {
        return it->second.get();
    }

    auto res = std::make_unique<BaseCommRes>(devPhyId);
    if (res == nullptr) {
        HCCL_ERROR("[BaseCommResMgr][%s] make_unique BaseCommRes failed for devPhyId=%u", __func__, devPhyId);
        return nullptr;
    }

    BaseCommRes* ptr = res.get();
    baseCommResMap_[devPhyId] = std::move(res);
    HCCL_INFO("[BaseCommResMgr][%s] created BaseCommRes for devPhyId=%u", __func__, devPhyId);
    return ptr;
}

BaseCommRes* BaseCommResMgr::GetBaseCommRes(uint32_t devPhyId)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = baseCommResMap_.find(devPhyId);
    if (it != baseCommResMap_.end()) {
        return it->second.get();
    }
    return nullptr;
}

HcclResult BaseCommResMgr::DestroyBaseCommRes(uint32_t devPhyId)
{
    std::unique_ptr<BaseCommRes> resToDestroy;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = baseCommResMap_.find(devPhyId);
        if (it == baseCommResMap_.end()) {
            HCCL_WARNING("[BaseCommResMgr][%s] BaseCommRes not found for devPhyId=%u", __func__, devPhyId);
            return HCCL_SUCCESS;
        }
        resToDestroy = std::move(it->second);
        baseCommResMap_.erase(it);
    }

    if (resToDestroy != nullptr) {
        auto ret = resToDestroy->DestroyAll();
        if (ret != HCCL_SUCCESS) {
            HCCL_ERROR("[BaseCommResMgr][%s] DestroyAll failed for devPhyId=%u, ret=%d", __func__, devPhyId, ret);
            return ret;
        }
    }

    HCCL_INFO("[BaseCommResMgr][%s] destroyed BaseCommRes for devPhyId=%u", __func__, devPhyId);
    return HCCL_SUCCESS;
}

HcclResult BaseCommResMgr::DestroyAll()
{
    HCCL_INFO("[BaseCommResMgr][%s] DestroyAll called", __func__);

    std::vector<std::unique_ptr<BaseCommRes>> toDestroy;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        toDestroy.reserve(baseCommResMap_.size());
        for (auto& entry : baseCommResMap_) {
            if (entry.second != nullptr) {
                toDestroy.push_back(std::move(entry.second));
            }
        }
        baseCommResMap_.clear();
    }

    for (auto& res : toDestroy) {
        if (res != nullptr) {
            (void)res->DestroyAll();
        }
    }
    HCCL_INFO("[BaseCommResMgr][%s] DestroyAll completed", __func__);
    return HCCL_SUCCESS;
}

HcclResult BaseCommResMgr::VisitBaseCommRes(const std::function<HcclResult(BaseCommRes*)>& visitor)
{
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& entry : baseCommResMap_) {
        if (entry.second != nullptr) {
            HcclResult ret = visitor(entry.second.get());
            if (ret != HCCL_SUCCESS) {
                return ret;
            }
        }
    }
    return HCCL_SUCCESS;
}

HcclResult BaseCommResMgr::GetDeviceCount(uint32_t& deviceCount) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    deviceCount = static_cast<uint32_t>(baseCommResMap_.size());

    return HCCL_SUCCESS;
}

} // namespace hcomm

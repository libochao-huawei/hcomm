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

#include <algorithm>

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

    DestroySingletons();

    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [devPhyId, res] : baseCommResMap_) {
        if (res != nullptr) {
            (void)res->DestroyAll();
        }
    }
    baseCommResMap_.clear();
    HCCL_INFO("[BaseCommResMgr][%s] DestroyAll completed", __func__);
    return HCCL_SUCCESS;
}

uint32_t BaseCommResMgr::GetDeviceCount() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return static_cast<uint32_t>(baseCommResMap_.size());
}

HcclResult BaseCommResMgr::RegisterSingleton(const std::string& name, SingletonInitFunc init, SingletonDestroyFunc destroy)
{
    std::lock_guard<std::mutex> lock(singletonMutex_);
    if (singletonRegistry_.find(name) != singletonRegistry_.end()) {
        HCCL_WARNING("[BaseCommResMgr][%s] singleton [%s] already registered", __func__, name.c_str());
        return HCCL_E_EXIST;
    }
    singletonRegistry_[name] = {init, destroy, false};
    HCCL_INFO("[BaseCommResMgr][%s] registered singleton [%s]", __func__, name.c_str());
    return HCCL_SUCCESS;
}

HcclResult BaseCommResMgr::UnregisterSingleton(const std::string& name)
{
    std::lock_guard<std::mutex> lock(singletonMutex_);
    auto it = singletonRegistry_.find(name);
    if (it == singletonRegistry_.end()) {
        HCCL_WARNING("[BaseCommResMgr][%s] singleton [%s] not found", __func__, name.c_str());
        return HCCL_E_NOT_FOUND;
    }

    if (it->second.initialized) {
        auto ret = it->second.destroy();
        if (ret != HCCL_SUCCESS) {
            HCCL_ERROR("[BaseCommResMgr][%s] destroy singleton [%s] failed, ret=%d", __func__, name.c_str(), ret);
        }
    }

    singletonRegistry_.erase(it);
    HCCL_INFO("[BaseCommResMgr][%s] unregistered singleton [%s]", __func__, name.c_str());
    return HCCL_SUCCESS;
}

HcclResult BaseCommResMgr::InitSingletons()
{
    std::lock_guard<std::mutex> lock(singletonMutex_);
    for (auto& [name, record] : singletonRegistry_) {
        if (!record.initialized) {
            auto ret = record.init();
            if (ret != HCCL_SUCCESS) {
                HCCL_ERROR("[BaseCommResMgr][%s] init singleton [%s] failed", __func__, name.c_str());
                return ret;
            }
            record.initialized = true;
            HCCL_INFO("[BaseCommResMgr][%s] initialized singleton [%s]", __func__, name.c_str());
        }
    }
    return HCCL_SUCCESS;
}

HcclResult BaseCommResMgr::DestroySingletons()
{
    std::lock_guard<std::mutex> lock(singletonMutex_);
    for (auto it = singletonRegistry_.rbegin(); it != singletonRegistry_.rend(); ++it) {
        auto& [name, record] = *it;
        if (record.initialized) {
            auto ret = record.destroy();
            if (ret != HCCL_SUCCESS) {
                HCCL_ERROR("[BaseCommResMgr][%s] destroy singleton [%s] failed", __func__, name.c_str());
            } else {
                HCCL_INFO("[BaseCommResMgr][%s] destroyed singleton [%s]", __func__, name.c_str());
            }
            record.initialized = false;
        }
    }
    return HCCL_SUCCESS;
}

} // namespace hcomm

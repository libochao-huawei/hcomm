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
#include "hccl_common.h"

namespace hcomm {

/**
 * @brief 获取单例实例
 */
BaseCommResMgr& BaseCommResMgr::Instance()
{
    static BaseCommResMgr instance;
    return instance;
}

/**
 * @brief 构造函数
 */
BaseCommResMgr::BaseCommResMgr()
{
    HCCL_INFO("[BaseCommResMgr][%s] constructed", __func__);
}

/**
 * @brief 析构函数，销毁所有资源
 */
BaseCommResMgr::~BaseCommResMgr()
{
    HCCL_INFO("[BaseCommResMgr][%s] destructing", __func__);
    DestroyAll();
}

/**
 * @brief 获取指定设备的BaseCommRes，不存在则创建
 * @param devPhyId 设备物理ID
 * @return BaseCommRes指针，已存在返回现有实例，不存在则创建新实例
 */
BaseCommRes* BaseCommResMgr::GetOrCreate(uint32_t devPhyId)
{
    if (devPhyId >= MAX_MODULE_DEVICE_NUM) {
        HCCL_WARNING("[BaseCommResMgr][%s] devPhyId[%u] >= MAX_MODULE_DEVICE_NUM[%u], using backup slot",
            __func__, devPhyId, MAX_MODULE_DEVICE_NUM);
        devPhyId = MAX_MODULE_DEVICE_NUM;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    auto it = baseCommResMap_.find(devPhyId);
    if (it != baseCommResMap_.end()) {
        return it->second.get();
    }

    auto baseCommRes = std::make_unique<BaseCommRes>(devPhyId);
    if (baseCommRes == nullptr) {
        HCCL_ERROR("[BaseCommResMgr][%s] make_unique BaseCommRes failed for devPhyId=%u",
            __func__, devPhyId);
        return nullptr;
    }

    BaseCommRes* ptr = baseCommRes.get();
    baseCommResMap_.emplace(devPhyId, std::move(baseCommRes));
    HCCL_INFO("[BaseCommResMgr][%s] created BaseCommRes for devPhyId=%u", __func__, devPhyId);
    return ptr;
}

/**
 * @brief 获取指定设备的BaseCommRes，仅返回已存在的实例
 * @param devPhyId 设备物理ID
 * @return BaseCommRes指针，不存在则返回nullptr
 */
BaseCommRes* BaseCommResMgr::GetBaseCommRes(uint32_t devPhyId)
{
    if (devPhyId >= MAX_MODULE_DEVICE_NUM) {
        devPhyId = MAX_MODULE_DEVICE_NUM;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    auto it = baseCommResMap_.find(devPhyId);
    if (it != baseCommResMap_.end()) {
        return it->second.get();
    }
    return nullptr;
}

/**
 * @brief 销毁指定设备的BaseCommRes
 * @param devPhyId 设备物理ID
 * @return HcclResult 成功返回HCCL_SUCCESS
 */
HcclResult BaseCommResMgr::DestroyBaseCommRes(uint32_t devPhyId)
{
    if (devPhyId >= MAX_MODULE_DEVICE_NUM) {
        devPhyId = MAX_MODULE_DEVICE_NUM;
    }

    std::unique_ptr<BaseCommRes> toDestroy;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = baseCommResMap_.find(devPhyId);
        if (it == baseCommResMap_.end()) {
            HCCL_INFO("[BaseCommResMgr][%s] BaseCommRes for devPhyId=%u not found", __func__, devPhyId);
            return HCCL_SUCCESS;
        }
        toDestroy = std::move(it->second);
        baseCommResMap_.erase(it);
    }

    if (toDestroy != nullptr) {
        HCCL_INFO("[BaseCommResMgr][%s] destroying BaseCommRes for devPhyId=%u", __func__, devPhyId);
        (void)toDestroy->DestroyAll();
    }
    return HCCL_SUCCESS;
}

/**
 * @brief 销毁所有设备的BaseCommRes
 * @return HcclResult 成功返回HCCL_SUCCESS
 */
HcclResult BaseCommResMgr::DestroyAll()
{
    std::vector<std::unique_ptr<BaseCommRes>> toDestroy;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        HCCL_INFO("[BaseCommResMgr][%s] destroying all BaseCommRes, count=%zu",
            __func__, baseCommResMap_.size());
        for (auto& entry : baseCommResMap_) {
            toDestroy.push_back(std::move(entry.second));
        }
        baseCommResMap_.clear();
    }

    for (auto& res : toDestroy) {
        if (res != nullptr) {
            (void)res->DestroyAll();
        }
    }
    return HCCL_SUCCESS;
}

/**
 * @brief 获取已注册的设备数量
 * @return 设备数量
 */
uint32_t BaseCommResMgr::GetDeviceCount() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return static_cast<uint32_t>(baseCommResMap_.size());
}

/**
 * @brief 遍历所有BaseCommRes，调用visitor回调
 * @param visitor 回调函数，接收BaseCommRes指针
 * @note 遍历过程中持有锁，visitor执行在锁外
 */
void BaseCommResMgr::VisitBaseCommRes(const std::function<void(BaseCommRes*)>& visitor)
{
    std::vector<BaseCommRes*> results;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& entry : baseCommResMap_) {
            BaseCommRes* res = entry.second.get();
            if (res == nullptr) {
                continue;
            }
            EndpointsMgr* mgr = res->TryGetEndpointsMgr();
            if (mgr == nullptr) {
                continue;
            }
            results.push_back(res);
        }
    }

    for (auto* res : results) {
        visitor(res);
    }
}

} // namespace hcomm

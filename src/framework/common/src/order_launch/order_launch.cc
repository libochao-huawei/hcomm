/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "order_launch.h"
#include "log.h"

namespace hccl {
OrderLaunch &OrderLaunch::GetInstance(s32 deviceLogicID)
{
    static OrderLaunch orderLaunch[MAX_MODULE_DEVICE_NUM];
    if (static_cast<u32>(deviceLogicID) >= MAX_MODULE_DEVICE_NUM) {
        HCCL_WARNING("[OrderLaunch][GetInstance]Invalid deviceLogicID[%d]", deviceLogicID);
        return orderLaunch[0];
    }
    HCCL_DEBUG("[OrderLaunch][GetInstance]Valid deviceLogicID[%d]", deviceLogicID);
    return orderLaunch[deviceLogicID];
}

OrderLaunch::OrderLaunch() : initialized_(true) {}

OrderLaunch::~OrderLaunch()
{
    std::unique_lock<std::mutex> mapLock(streamMutex_);
    initialized_ = false;
    groupSet_.clear();
    streamMap_.clear();
}

HcclResult OrderLaunch::RegisterOrderLaunch(const std::string &group)
{
    std::unique_lock<std::mutex> mapLock(streamMutex_);
    if (groupSet_.find(group) != groupSet_.end()) {
        HCCL_WARNING("%s skip, group[%s] has already been registered", __func__, group.c_str());
        return HCCL_SUCCESS;
    }
    groupSet_.insert(group);
    HCCL_INFO("%s success, group[%s]", __func__, group.c_str());
    return HCCL_SUCCESS;
}

HcclResult OrderLaunch::UnRegisterOrderLaunch(const std::string &group)
{
    CHK_PRT_RET(initialized_ == false, HCCL_WARNING("OrderLaunch has been destroyed"), HCCL_SUCCESS);
    std::unique_lock<std::mutex> mapLock(streamMutex_);
    if (groupSet_.find(group) == groupSet_.end()) {
        HCCL_WARNING("%s skip, group[%s] has not been registered", __func__, group.c_str());
        return HCCL_SUCCESS;
    }

    groupSet_.erase(group);
    if (groupSet_.empty()) { // 没有注册的通信域，销毁全局的流
        streamMap_.clear();
    }
    HCCL_INFO("%s success, group[%s]", __func__, group.c_str());
    return HCCL_SUCCESS;
}

HcclResult OrderLaunch::GetOrderStream(const std::string &group, StreamType streamType, Stream &stream)
{
    std::unique_lock<std::mutex> mapLock(streamMutex_);
    if (groupSet_.find(group) == groupSet_.end()) {
        HCCL_ERROR("%s fail, group[%s] has not been registered", __func__, group.c_str());
        return HCCL_E_PARA;
    }

    if (streamMap_.find(streamType) == streamMap_.end()) {
        streamMap_[streamType] = Stream(streamType);
        HCCL_RUN_INFO("%s alloc streamId[%u], streamType[%d]", __func__, streamMap_[streamType].id(), streamType);
    }

    stream = streamMap_[streamType];
    CHK_PTR_NULL(stream.ptr());
    HCCL_DEBUG("%s group[%s], streamId[%u], streamType[%d]", __func__, group.c_str(), stream.id(), streamType);
    return HCCL_SUCCESS;
}

void OrderLaunch::Lock()
{
    streamMutex_.lock();
}

void OrderLaunch::UnLock()
{
    streamMutex_.unlock();
}
}
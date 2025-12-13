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
#include "acl/acl_base.h"
#include "acl/acl_rt.h"
#include "config_log.h"
#include "hccl_communicator.h"
#include "stream_utils.h"
#include "hccl_types.h"

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
    opbaseStream_.reset();
    aclgraphStreamMap_.clear();
    hcomStreamMap_.clear();
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
        opbaseStream_.reset();
        aclgraphStreamMap_.clear();
        hcomStreamMap_.clear();
    }
    HCCL_INFO("%s success, group[%s]", __func__, group.c_str());
    return HCCL_SUCCESS;
}

HcclResult OrderLaunch::SetHcomStream(u32 graphId, const Stream& hcomAttachedStream)
{
    std::unique_lock<std::mutex> mapLock(streamMutex_);
    hcomStreamMap_[graphId] = hcomAttachedStream;
    return HCCL_SUCCESS;
}

HcclResult OrderLaunch::AclgraphLaunchInOrder(std::string &group, const Stream& kernelStream, u64 modelId,
    rtModel_t rtModel, std::shared_ptr<LocalNotify> notify0, std::shared_ptr<LocalNotify> notify1, u32 timeOut)
{
    HCCL_INFO("AclgraphLaunchInOrder skip");
    return HCCL_SUCCESS;
    std::unique_lock<std::mutex> mapLock(streamMutex_);
    if (groupSet_.find(group) == groupSet_.end()) {
        HCCL_ERROR("[%s] fail, group[%s] has not been registered", __func__, group.c_str());
        return HCCL_E_PARA;
    }

    Stream* hostOrderStream = nullptr;
    for (auto it = aclgraphStreamMap_.begin(); it != aclgraphStreamMap_.end(); ++it) {
        if (it->first == modelId) {
            hostOrderStream = &(it->second);
            CHK_PTR_NULL(hostOrderStream);
            HCCL_INFO("[%s] reuse existing modelId[%llu] streamId[%u]",
                        __func__, modelId, hostOrderStream->id());
            break;
        }
    }

    if (hostOrderStream == nullptr) {
        aclgraphStreamMap_[modelId] = Stream(StreamType::STREAM_TYPE_ONLINE);
        hostOrderStream = &(aclgraphStreamMap_[modelId]);
        HCCL_INFO("[%s] modelId[%llu] group[%s] alloc streamId[%u]",
                    __func__, modelId, group.c_str(), hostOrderStream->id());
        CHK_PTR_NULL(hostOrderStream);
        CHK_RET(AddStreamToModel(hostOrderStream->ptr(), rtModel));
    }
    HCCL_INFO("[%s] group[%s], modelId[%llu], streamId[%u]", __func__, group.c_str(), modelId, hostOrderStream->id());
    CHK_RET(LaunchInOrder(group, kernelStream, *hostOrderStream, notify0, notify1, timeOut));
    return HCCL_SUCCESS;
}

HcclResult OrderLaunch::OpbaseLaunchInOrder(std::string &group, const Stream& kernelStream,
    std::shared_ptr<LocalNotify> notify0, std::shared_ptr<LocalNotify> notify1, u32 timeOut)
{
    std::unique_lock<std::mutex> mapLock(streamMutex_);
    if (groupSet_.find(group) == groupSet_.end()) {
        HCCL_ERROR("[%s] fail, group[%s] has not been registered", __func__, group.c_str());
        return HCCL_E_PARA;
    }
    // 申请控制流
    Stream hostOrderStream;
    if (opbaseStream_ == nullptr) {
        EXECEPTION_CATCH(opbaseStream_ = std::make_unique<Stream>(StreamType::STREAM_TYPE_ONLINE), return HCCL_E_PTR);
        HCCL_INFO("[%s] group[%s] alloc streamId[%u]", __func__, group.c_str(), opbaseStream_->id());
    }
    hostOrderStream = *opbaseStream_;
    CHK_PTR_NULL(hostOrderStream.ptr());
    HCCL_INFO("[%s] group[%s], streamId[%u]", __func__, group.c_str(), hostOrderStream.id());
    CHK_RET(LaunchInOrder(group, kernelStream, hostOrderStream, notify0, notify1, timeOut));
    return HCCL_SUCCESS;
}

HcclResult OrderLaunch::HcomLaunchInOrder(std::string &group, const Stream& kernelStream, u32 graphId,
    std::shared_ptr<LocalNotify> notify0, std::shared_ptr<LocalNotify> notify1, u32 timeOut)
{
    std::unique_lock<std::mutex> mapLock(streamMutex_);
    if (groupSet_.find(group) == groupSet_.end()) {
        HCCL_ERROR("[%s] fail, group[%s] has not been registered", __func__, group.c_str());
        return HCCL_E_PARA;
    }
    Stream hostOrderStream;
    if (hcomStreamMap_.find(graphId) == hcomStreamMap_.end()) {
        HCCL_ERROR("[%s] graphId[%u] group[%s] stream not found", __func__, graphId, group.c_str());
        return HCCL_E_NOT_FOUND;
    }
    hostOrderStream = hcomStreamMap_[graphId];
    CHK_PTR_NULL(hostOrderStream.ptr());
    HCCL_INFO("[%s] group[%s], graphId[%u], streamId[%u]", __func__, group.c_str(), graphId, hostOrderStream.id());
    CHK_RET(LaunchInOrder(group, kernelStream, hostOrderStream, notify0, notify1, timeOut));
    return HCCL_SUCCESS;
}

HcclResult OrderLaunch::LaunchInOrder(std::string &group, const Stream &kernelStream, const Stream &hostOrderStream,
    std::shared_ptr<LocalNotify> notify0, std::shared_ptr<LocalNotify> notify1, u32 timeOut) 
{
#ifndef CCL_KERNEL_AICPU
    aclError ret = ACL_SUCCESS;
    ret = aclrtWaitAndResetNotify(notify0->ptr(), kernelStream.ptr(), timeOut);
    CHK_PRT_RET(ret != ACL_SUCCESS,
        HCCL_ERROR("[%s] aclrtWaitAndResetNotify failed, ret[%d], notifyId[%u], streamId[%d], timeOut[%d s]",
        __func__, ret, notify0->notifyId_, kernelStream.id(), timeOut), HCCL_E_RUNTIME);
    HCCL_CONFIG_INFO(HCCL_TASK, "[%s] aclrtWaitAndResetNotify para: notifyId[%u], streamId[%d], timeOut[%d s]",
        __func__, notify0->notifyId_, kernelStream.id(), timeOut);

    ret = aclrtRecordNotify(notify0->ptr(), hostOrderStream.ptr());
    CHK_PRT_RET(ret != ACL_SUCCESS, HCCL_ERROR("[%s] aclrtRecordNotify failed, ret[%d], notifyId[%u], streamId[%d]",
        __func__, ret, notify0->notifyId_, hostOrderStream.id()), HCCL_E_RUNTIME);
    HCCL_CONFIG_INFO(HCCL_TASK, "[%s] aclrtRecordNotify para: notifyId[%u], streamId[%d]",
        __func__, notify0->notifyId_, hostOrderStream.id());

    ret = aclrtWaitAndResetNotify(notify1->ptr(), hostOrderStream.ptr(), timeOut);
    CHK_PRT_RET(ret != ACL_SUCCESS,
        HCCL_ERROR("[%s] aclrtWaitAndResetNotify failed, ret[%d], notifyId[%u], streamId[%d], timeOut[%d s]",
        __func__, ret, notify1->notifyId_, hostOrderStream.id(), timeOut), HCCL_E_RUNTIME);
    HCCL_CONFIG_INFO(HCCL_TASK, "[%s] aclrtWaitAndResetNotify para: notifyId[%u], streamId[%d], timeOut[%d s]",
        __func__, notify1->notifyId_, hostOrderStream.id(), timeOut);
#endif
    return HCCL_SUCCESS;
}
}
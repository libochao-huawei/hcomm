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
#include "adapter_rts_common.h"

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
    groupCtxMap_.clear();
    DestroyRes();
}

void OrderLaunch::DestroyRes()
{
    for (auto &entry : contextResMgrMap_) {
        entry.second.DestroyResources();
    }
    contextResMgrMap_.clear();
    hcomStreamMap_.clear();
}

HcclResult OrderLaunch::RegisterOrderLaunch(const std::string &group)
{
    std::unique_lock<std::mutex> mapLock(streamMutex_);
    if (groupCtxMap_.find(group) != groupCtxMap_.end()) {
        HCCL_WARNING("%s skip, group[%s] has already been registered", __func__, group.c_str());
        return HCCL_SUCCESS;
    }
    groupCtxMap_.insert({group, INVALID_U64}); // 只记录group，context暂不赋值，只在算子下发阶段对context赋值
    HCCL_INFO("%s success, group[%s]", __func__, group.c_str());
    return HCCL_SUCCESS;
}

HcclResult OrderLaunch::UnRegisterOrderLaunch(const std::string &group)
{
    CHK_PRT_RET(initialized_ == false, HCCL_WARNING("OrderLaunch has been destroyed"), HCCL_SUCCESS);
    std::unique_lock<std::mutex> mapLock(streamMutex_);
    auto it = groupCtxMap_.find(group);
    if (it == groupCtxMap_.end()) {
        HCCL_WARNING("%s skip, group[%s] has not been registered", __func__, group.c_str());
        return HCCL_SUCCESS;
    }

    u64 context = it->second;
    if (contextGroupsMap_.find(context) != contextGroupsMap_.end()) {
        contextGroupsMap_[context].erase(group);
        if (contextGroupsMap_[context].empty()) {
            contextGroupsMap_.erase(context);
            if (contextResMgrMap_.find(context) != contextResMgrMap_.end()) {
                contextResMgrMap_[context].DestroyResources();
                contextResMgrMap_.erase(context);
            }
            HCCL_INFO("%s contextGroupsMap_ erase context[0x%llx]", __func__, context);
        }
    }

    groupCtxMap_.erase(it);
    HCCL_INFO("%s success, group[%s]", __func__, group.c_str());
    return HCCL_SUCCESS;
}

HcclResult OrderLaunch::SetHcomStream(u32 graphId, const Stream& hcomAttachedStream)
{
    std::unique_lock<std::mutex> mapLock(streamMutex_);
    hcomStreamMap_[graphId] = hcomAttachedStream;
    return HCCL_SUCCESS;
}

HcclResult OrderLaunch::InitGroupCtx(const std::string &group)
{
    u64 currentContext = GetCurrentContext();

    if (contextResMgrMap_.find(currentContext) == contextResMgrMap_.end()) {
        contextResMgrMap_[currentContext] = OrderLaunchResMgr();
        contextResMgrMap_[currentContext].context = currentContext;
    }

    groupCtxMap_[group] = currentContext;
    contextGroupsMap_[currentContext].insert(group);

    HCCL_RUN_INFO("[%s]group[%s] init or update context[0x%llx]", __func__, group.c_str(), currentContext);
    return HCCL_SUCCESS;
}

// aclgraph模式下，先在kernel stream上写record，再在上order stream写wait；解order stream的wait
HcclResult OrderLaunch::AclgraphLaunchInOrderToOrderStream(std::string &group, const Stream& kernelStream,
    std::shared_ptr<LocalNotify> notify0, std::shared_ptr<LocalNotify> notify1, u32 timeOut, HcclRtEvent event)
{
    std::unique_lock<std::mutex> mapLock(streamMutex_);
    // group未注册过，或者未记录过算子下发阶段的线程context
    if (groupCtxMap_.find(group) == groupCtxMap_.end() || groupCtxMap_[group] == INVALID_U64) {
        CHK_RET(InitGroupCtx(group));
    }

    u64 context = groupCtxMap_[group];
    std::unique_ptr<Stream>& aclgraphStream = contextResMgrMap_[context].aclgraphStream;
    EnsureOrderStreamForGroup(group, aclgraphStream); // aclgraph控制流

    aclError ret = ACL_SUCCESS;
    // kernelStream -> aclgraphStream
    ret = aclrtRecordEvent(event, kernelStream.ptr());
    CHK_PRT_RET(ret != ACL_SUCCESS, HCCL_ERROR("[%s]aclrtRecordEvent failed, ret[%d]", __func__, ret), HCCL_E_RUNTIME);
    HCCL_CONFIG_INFO(HCCL_TASK, "[%s]aclrtRecordEvent para: kernelStreamId[%d]", __func__, kernelStream.id());

    ret = aclrtStreamWaitEvent(aclgraphStream->ptr(), event);
    CHK_PRT_RET(ret != ACL_SUCCESS, HCCL_ERROR("[%s]aclrtStreamWaitEvent failed, ret[%d]", __func__, ret), HCCL_E_RUNTIME);
    HCCL_CONFIG_INFO(HCCL_TASK, "[%s]aclrtStreamWaitEvent para: orderStreamId[%d]",  __func__, aclgraphStream->id());

    HCCL_INFO("[%s] group[%s], kernelStreamId[%u], orderStreamId[%u], context[0x%llx]",
        __func__, group.c_str(), kernelStream.id(), aclgraphStream->id(), context);
    CHK_RET(LaunchInOrder(group, kernelStream, *aclgraphStream, notify0, notify1, timeOut));
    return HCCL_SUCCESS;
}

// aclgraph模式下，接着在order stream上做record，再在kernel stream上做wait；解kernel stream的wait
HcclResult OrderLaunch::AclgraphLaunchInOrderToKernelStream(std::string &group, const Stream& kernelStream,
    HcclRtEvent event)
{
    std::unique_lock<std::mutex> mapLock(streamMutex_);

    auto ctxIt = groupCtxMap_.find(group);
    CHK_PRT_RET(ctxIt == groupCtxMap_.end(), HCCL_ERROR("[%s]fail, group[%s] is not in groupCtxMap_",
        __func__, group.c_str()), HCCL_E_NOT_FOUND);

    u64 context = ctxIt->second;
    if (contextResMgrMap_.find(context) == contextResMgrMap_.end()) {
        HCCL_ERROR("[%s]fail, context[0x%llx] is not in contextResMgrMap_", __func__, context);
        return HCCL_E_NOT_FOUND;
    }

    std::unique_ptr<Stream>& aclgraphStream = contextResMgrMap_[context].aclgraphStream;

    aclError ret = ACL_SUCCESS;
    ret = aclrtRecordEvent(event, aclgraphStream->ptr());
    CHK_PRT_RET(ret != ACL_SUCCESS, HCCL_ERROR("[%s]aclrtRecordEvent failed, ret[%d]", __func__, ret), HCCL_E_RUNTIME);
    HCCL_CONFIG_INFO(HCCL_TASK, "[%s]aclrtRecordEvent para: orderStreamId[%d]", __func__, aclgraphStream->id());

    ret = aclrtStreamWaitEvent(kernelStream.ptr(), event);
    CHK_PRT_RET(ret != ACL_SUCCESS, HCCL_ERROR("[%s]aclrtStreamWaitEvent failed, ret[%d]", __func__, ret), HCCL_E_RUNTIME);
    HCCL_CONFIG_INFO(HCCL_TASK, "[%s]aclrtStreamWaitEvent para: kernelStreamId[%d]", __func__, kernelStream.id());

    HCCL_INFO("[%s] group[%s], kernelStreamId[%u], orderStreamId[%u], context[0x%llx]",
        __func__, group.c_str(), kernelStream.id(), aclgraphStream->id(), ctxIt->second);
    return HCCL_SUCCESS;
}

HcclResult OrderLaunch::OpbaseLaunchInOrder(std::string &group, const Stream& kernelStream,
    std::shared_ptr<LocalNotify> notify0, std::shared_ptr<LocalNotify> notify1, u32 timeOut)
{
    std::unique_lock<std::mutex> mapLock(streamMutex_);
    // group未注册过，或者未记录过算子下发阶段的线程context
    if (groupCtxMap_.find(group) == groupCtxMap_.end() || groupCtxMap_[group] == INVALID_U64) {
        CHK_RET(InitGroupCtx(group));
    }

    u64 context = groupCtxMap_[group];
    std::unique_ptr<Stream>& opbaseStream = contextResMgrMap_[context].opbaseStream;
    EnsureOrderStreamForGroup(group, opbaseStream); // 单算子控制流

    HCCL_INFO("[%s] group[%s], kernelStreamId[%u], orderStreamId[%u], context[0x%llx]",
        __func__, group.c_str(), kernelStream.id(), opbaseStream->id(), context);
    CHK_RET(LaunchInOrder(group, kernelStream, *opbaseStream, notify0, notify1, timeOut));
    return HCCL_SUCCESS;
}

HcclResult OrderLaunch::HcomLaunchInOrder(std::string &group, const Stream& kernelStream, u32 graphId,
    std::shared_ptr<LocalNotify> notify0, std::shared_ptr<LocalNotify> notify1, u32 timeOut)
{
    std::unique_lock<std::mutex> mapLock(streamMutex_);
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
    return HCCL_SUCCESS;
}

HcclResult OrderLaunch::EnsureOrderStreamForGroup(std::string &group, std::unique_ptr<Stream> &orderStream) {
    auto it = groupCtxMap_.find(group);
    if (it == groupCtxMap_.end()) {
        HCCL_ERROR("[%s] group[%s] not found", __func__, group.c_str());
        return HCCL_E_PARA;
    }

    u64 context = it->second;
    if (contextResMgrMap_.find(context) == contextResMgrMap_.end()) {
        HCCL_ERROR("[%s] context[0x%llx] not found for group[%s]", __func__, context, group.c_str());
        return HCCL_E_PARA;
    }

    auto& groupCtxRes = contextResMgrMap_[context];
    u64 currentContext = GetCurrentContext();

    if (!groupCtxRes.IsCtxMatch(currentContext) || orderStream == nullptr || orderStream->ptr() == nullptr) {
        if (orderStream != nullptr && orderStream->ptr() != nullptr) {
            HCCL_INFO("[OrderLaunch] Destroying existing order stream with mismatched context.");
            orderStream.reset();
        }
        EXECEPTION_CATCH(orderStream = std::make_unique<Stream>(StreamType::STREAM_TYPE_ONLINE), return HCCL_E_PTR);
        constexpr u32 streamMode = 1; // 使能遇错即停，避免出错后流卡住不退
        CHK_RET(hrtStreamSetMode(orderStream->ptr(), streamMode));
        HCCL_INFO("[OrderLaunch] Created new order stream with id[%d] with context [0x%llx]", orderStream->id(), currentContext);

        // 对group->contextResource的映射关系进行更新
        groupCtxRes.UpdateContext(currentContext);
        // 对context->group的映射关系进行更新
        contextGroupsMap_[currentContext].insert(group);
    }

    return HCCL_SUCCESS;
}

u64 OrderLaunch::GetCurrentContext() {
    std::lock_guard<std::mutex> lock(streamMutex_);
    // 获取当前上下文
    HcclRtContext rtCtx = nullptr;
    CHK_RET(hrtCtxGetCurrent(&rtCtx));
    u64 currentContext = reinterpret_cast<u64>(rtCtx);

    if (currentContext == INVALID_U64)
    {
        HCCL_ERROR("[%s] GetCurrentContext failed", __func__);
        return HCCL_E_RUNTIME;
    }
    
    return currentContext;
}
}
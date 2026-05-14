/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "endpoint_monitor.h"
#include "hcom_common.h"

namespace hcomm {

EndpointMonitor::~EndpointMonitor()
{
    DeInit();
}

EndpointMonitor &EndpointMonitor::GetInstance(u32 deviceId)
{
    static std::array<EndpointMonitor, MAX_MODULE_DEVICE_NUM> instances;
    if (deviceId >= MAX_MODULE_DEVICE_NUM) {
        HCCL_WARNING("[EndpointMonitor][%s] deviceId[%u] >= %u, invalid", __func__, deviceId, MAX_MODULE_DEVICE_NUM);
        return instances[0];
    }
    return instances[deviceId];
}

HcclResult EndpointMonitor::RegisterToEndpointMonitor(u32 deviceId, EndpointHandle epHandle)
{
    HCCL_INFO("[EndpointMonitor] deviceId[%u] RegisterToEndpointMonitor begin.",deviceId);
    CHK_PRT_RET(deviceId >= MAX_MODULE_DEVICE_NUM,
        HCCL_ERROR("[EndpointMonitor][%s] deviceId[%u] >= %u, invalid", __func__, deviceId, MAX_MODULE_DEVICE_NUM),
        HCCL_E_PARA);
    CHK_PRT_RET(epHandle == nullptr, HCCL_ERROR("[EndpointMonitor][%s] epHandle is null", __func__), HCCL_E_PTR);

    u32 devPhyId{0};
    CHK_RET(hrtGetDevicePhyIdByIndex(deviceId, devPhyId));

    {
        std::lock_guard<std::mutex> lock(threadLock_);
        epHandleSet_.emplace(reinterpret_cast<u64>(epHandle));
        deviceLogicId_ = static_cast<s32>(deviceId);
        devPhyId_ = devPhyId;
        if (!initialized_) {
            CHK_RET(RunMonitorThread());
        }
    }

    HCCL_INFO("[EndpointMonitor] deviceId[%u] RegisterToEndpointMonitor Completed.",deviceId);
    return HCCL_SUCCESS;
}

HcclResult EndpointMonitor::RunMonitorThread()
{
    HCCL_INFO("[EndpointMonitor][%s] Start Thread.", __func__);
    endpointMonitorThreadFlag_ = true;
    endpointMonitorThread_.reset(new (std::nothrow) std::thread(&EndpointMonitor::MonitorThread, this));
    CHK_SMART_PTR_NULL(endpointMonitorThread_);
    initialized_ = true;
    return HCCL_SUCCESS;
}

void EndpointMonitor::MonitorThread()
{
    SetThreadName("Hccl_Ub_Event_Monitor");
    hrtSetDevice(deviceLogicId_);

    while (endpointMonitorThreadFlag_) {
        if (!ProcessUbAsyncEvents()) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(MONITOR_INTERVAL));
    }

    hrtResetDevice(deviceLogicId_);
}

HcclResult EndpointMonitor::UnRegisterToEndpointMonitor()
{
    HCCL_INFO("[EndpointMonitor] deviceId[%d] UnRegisterToEndpointMonitor begin.", deviceLogicId_);
    {
        std::lock_guard<std::mutex> lock(threadLock_);
        CHK_PRT_RET(!initialized_,
            HCCL_WARNING("[EndpointMonitor][%s] hcclUbEventMonitor has been destroyed, or not initialized", __func__),
            HCCL_SUCCESS);

        epHandleSet_.clear();
        devPhyId_ = 0;
        deviceLogicId_ = 0;
    }

    CHK_RET(DeInit());
    initialized_ = false;
    HCCL_RUN_INFO("[EndpointMonitor] deviceId[%d] UnRegisterToEndpointMonitor completed.", deviceLogicId_);
    return HCCL_SUCCESS;
}

HcclResult EndpointMonitor::DeInit()
{
    HCCL_INFO("[EndpointMonitor] deinit begin.");
    endpointMonitorThreadFlag_ = false;

    if (endpointMonitorThread_) {
        if (endpointMonitorThread_->joinable()) {
            try {
                endpointMonitorThread_->join();
            } catch (const std::exception &e) {
                HCCL_ERROR("[EndpointMonitor][%s] join failed: %s"__func__, e.what());
                return HCCL_E_INTERNAL;
            }
        }
    }

    HCCL_INFO("[EndpointMonitor] deinit end.");
    return HCCL_SUCCESS;
}

bool EndpointMonitor::ProcessUbAsyncEvents()
{
    std::vector<EndpointHandle> handles;
    {
        std::lock_guard<std::mutex> lock(threadLock_);
        for (const auto &value : epHandleSet_) {
            handles.push_back(reinterpret_cast<EndpointHandle>(value));
        }
    }

    HCCL_DEBUG("[EndpointMonitor][%s] devPhyId[%u] handles[%zu]", __func__, devPhyId_, handles.size());

    for (const auto &epHandle : handles) {
        u32 num = ASYNC_EVENT_MAX_NUM;
        auto events = std::unique_ptr<struct AsyncEvent[]>(new (std::nothrow) struct AsyncEvent[ASYNC_EVENT_MAX_NUM]);
        CHK_PRT_RET(events == nullptr,
            HCCL_ERROR("[EndpointMonitor][%s] devPhyId[%u] events is null", __func__, devPhyId_), false);

        HcommResult ret = HcommGetAsyncEvents(devPhyId_, epHandle, events.get(), num);

        if (ret == HCCL_E_NOT_SUPPORT) {
            HCCL_ERROR("[EndpointMonitor][%s] devPhyId[%u] version not support", __func__, devPhyId_);
            return false;
        } else if (ret == HCCL_SUCCESS) {
            HCCL_DEBUG("[EndpointMonitor][%s] devPhyId[%u] fetched %u events", __func__, devPhyId_, num);
            for (unsigned int i = 0; i < num; ++i) {
                PrintUbAsyncEventsContext(devPhyId_, events[i]);
            }
        } else {
            HCCL_ERROR("[EndpointMonitor][%s] devPhyId[%u] HcommGetAsyncEvents failed, ret[%d]",
                __func__, devPhyId_, ret);
            return false;
        }
    }
    return true;
}

void EndpointMonitor::PrintUbAsyncEventsContext(u32 devPhyId, const struct AsyncEvent &event)
{
    u32 contextLen = event.len;
    if (contextLen > CONTEXT_MAX_LEN) {
        HCCL_ERROR("[EndpointMonitor][%s] devPhyId[%u] context len[%u] exceed max[%u]", __func__, devPhyId, contextLen,
            CONTEXT_MAX_LEN);
        return;
    }

    HCCL_ERROR("************* devPhyId[%u] ub async event, resId[%u], eventType[%u], contextLen[%u]*************",
        devPhyId, event.resId, event.eventType, event.len);

    constexpr u32 bytesPerLine = 4;
    for (u32 i = 0; i < contextLen; i += bytesPerLine) {
        u32 endIndex = std::min(i + bytesPerLine, contextLen);
        HCCL_ERROR("context[byte %u]: %02x%02x%02x%02x", endIndex,
            (i + 3 < contextLen) ? event.context[i + 3] : 0,
            (i + 2 < contextLen) ? event.context[i + 2] : 0,
            (i + 1 < contextLen) ? event.context[i + 1] : 0,
            event.context[i]);
    }
    HCCL_ERROR("********************************************************************************");
}

} // namespace hcomm
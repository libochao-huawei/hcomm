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

EndpointMonitor &EndpointMonitor::GetInstance(s32 deviceLogicId)
{
    static std::array<EndpointMonitor, MAX_MODULE_DEVICE_NUM + 1> instances;
    uint32_t deviceId;
    if (deviceLogicId < 0 || deviceLogicId >= MAX_MODULE_DEVICE_NUM) {
        HCCL_ERROR("[EndpointMonitor][%s] deviceLogicId[%d] not in range [0,%u)",
            __func__, deviceLogicId, MAX_MODULE_DEVICE_NUM);
        deviceId = MAX_MODULE_DEVICE_NUM;
    } else {
        deviceId = static_cast<u32> deviceLogicId;
    }
    return instances[deviceId];
}

HcclResult EndpointMonitor::RegisterToEndpointMonitor(s32 deviceLogicId, EndpointHandle epHandle)
{
    CHK_PRT_RET(deviceLogicId < 0 || deviceLogicId >= MAX_MODULE_DEVICE_NUM,
        HCCL_ERROR("[EndpointMonitor][%s] deviceLogicId[%d] not in range [0,%u)", 
            __func__, deviceLogicId, MAX_MODULE_DEVICE_NUM), HCCL_E_PARA);
    CHK_PRT_RET(epHandle == nullptr, HCCL_ERROR("[EndpointMonitor][%s] epHandle is null", __func__), HCCL_E_PTR);

    HCCL_INFO("[EndpointMonitor] deviceLogicId[%d] RegisterToEndpointMonitor begin.", deviceLogicId);
    u32 devPhyId{0};
    CHK_RET(hrtGetDevicePhyIdByIndex(static_cast<u32>(deviceLogicId), devPhyId));

    {
        std::lock_guard<std::mutex> lock(threadLock_);
        epHandleSet_.emplace(reinterpret_cast<u64>(epHandle));
        deviceLogicId_ = deviceLogicId;
        devPhyId_ = devPhyId;
        if (!initialized_) {
            CHK_RET(RunMonitorThread());
        }
    }

    HCCL_INFO("[EndpointMonitor] deviceLogicId[%d] RegisterToEndpointMonitor Completed.", deviceLogicId);
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

    HcclResult ret = hrtSetDevice(deviceLogicId_);
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR(
            "[EndpointMonitor][%s] hrtSetDevice failed, deviceLogicId[%d], ret[%d]", __func__, deviceLogicId_, ret);
        return;
    }

    while (endpointMonitorThreadFlag_) {
        ProcessUbAsyncEvents();
        std::this_thread::sleep_for(std::chrono::milliseconds(MONITOR_INTERVAL));
    }

    ret = hrtResetDevice(deviceLogicId_);
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR(
            "[EndpointMonitor][%s] hrtResetDevice failed, deviceLogicId[%d], ret[%d]", __func__, deviceLogicId_, ret);
    }
}

HcclResult EndpointMonitor::UnRegisterToEndpointMonitor()
{
    s32 deviceLogicId = deviceLogicId_;
    HCCL_INFO("[EndpointMonitor] deviceId[%d] UnRegisterToEndpointMonitor begin.", deviceLogicId);
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
    HCCL_INFO("[EndpointMonitor] deviceId[%d] UnRegisterToEndpointMonitor completed.", deviceLogicId);
    return HCCL_SUCCESS;
}

HcclResult EndpointMonitor::DeInit()
{
    HCCL_INFO("[EndpointMonitor] deinit begin.");
    endpointMonitorThreadFlag_ = false;
    if (endpointMonitorThread_)
    {
        if (endpointMonitorThread_->joinable()) {
            try {
                endpointMonitorThread_->join();
                endpointMonitorThread_.reset();
            } catch (const std::exception &e) {
                HCCL_ERROR("[EndpointMonitor][%s] join failed: %s", __func__, e.what());
                return HCCL_E_INTERNAL;
            }
        }
    }
    HCCL_INFO("[EndpointMonitor] deinit end.");
    return HCCL_SUCCESS;
}

void EndpointMonitor::ProcessUbAsyncEvents()
{
    std::vector<EndpointHandle> handles;
    {
        std::lock_guard<std::mutex> lock(threadLock_);
        for (const auto &value : epHandleSet_) {
            handles.push_back(reinterpret_cast<EndpointHandle>(value));
        }
    }

    HCCL_INFO("[EndpointMonitor][%s] devPhyId[%u] handles[%zu]", __func__, devPhyId_, handles.size());

    for (const auto &epHandle : handles) {
        u32 num = ASYNC_EVENT_MAX_NUM;
        Endpoint *localEpPtr = reinterpret_cast<Endpoint *>(epHandle);
        HcommResult ret = localEpPtr->GetAsyncEventsContext(devPhyId_, events_, num);

        if (ret != HCCL_SUCCESS) {
            {
                std::lock_guard<std::mutex> lock(threadLock_);
                epHandleSet_.erase(reinterpret_cast<u64>(epHandle));
            }
            HCCL_ERROR("[EndpointMonitor][%s] devPhyId[%u] HcommGetAsyncEvents failed ret[%d], "
                       "epHandle[%p] removed from monitor", __func__, devPhyId_, ret, epHandle);
            return;
        }

        HCCL_INFO("[EndpointMonitor][%s] devPhyId[%u] fetched %u events", __func__, devPhyId_, num);
        for (unsigned int i = 0; i < num; ++i) {
            PrintUbAsyncEventsContext(devPhyId_, events_[i]);
        }
    }
}

void EndpointMonitor::PrintUbAsyncEventsContext(u32 devPhyId, const struct AsyncEvent &event)
{
    u32 contextLen = event.len;
    if (contextLen > CONTEXT_MAX_LEN) {
        HCCL_ERROR("[EndpointMonitor][%s] devPhyId[%u] context len[%u] exceed max[%u]", __func__, devPhyId, contextLen,
            CONTEXT_MAX_LEN);
        return;
    }

    HCCL_ERROR("*************************** ub async event ***************************");
    HCCL_ERROR("devPhyId[%u] resId[%u] eventType[%u] contextLen[%u]",
        devPhyId, event.resId, event.eventType, event.len);
    HCCL_ERROR("context bytes order: high -> low");
    constexpr u32 bytesPerLine = 4;
    for (u32 i = 0; i < contextLen; i += bytesPerLine) {
        u32 endIndex = std::min(i + bytesPerLine, contextLen);
        HCCL_ERROR("context[byte %u]: %02x%02x%02x%02x", endIndex,
            (i + 3 < contextLen) ? event.context[i + 3] : 0,
            (i + 2 < contextLen) ? event.context[i + 2] : 0,
            (i + 1 < contextLen) ? event.context[i + 1] : 0,
            event.context[i]);
    }
    HCCL_ERROR("**********************************************************************");
}

} // namespace hcomm
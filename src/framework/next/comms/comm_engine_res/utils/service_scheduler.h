/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef SERVICE_SCHEDULER_H
#define SERVICE_SCHEDULER_H

#include "hccl_res.h"
#include "hccl_independent_common.h"
#include "resource_entities.h"
#include "msg_queue.h"
#include <memory>
#include <unordered_map>
#include <mutex>

namespace hccl {

class ServiceScheduler {
public:
    ServiceScheduler() = default;
    ~ServiceScheduler() = default;
    HcclResult Init() {
        // 初始化sq和cq
        EXECEPTION_CATCH(
        sendQueue_ = std::make_shared<MsgQueue>(MSG_QUEUE_CAPACITY, sizeof(ThreadMsgEntity)), return HCCL_E_PTR);
        EXECEPTION_CATCH(
        completeQueue_ = std::make_shared<MsgQueue>(MSG_QUEUE_CAPACITY, sizeof(ThreadMsgEntity)), return HCCL_E_PTR);
        return HCCL_SUCCESS;
    };
    HcclResult ServiceRun() {
        bool isEmpty = false;
        while (true) {
            CHK_RET(sendQueue_->Empty(isEmpty));
            if (!isEmpty) {
                ThreadMsgEntity entity{};
                CHK_RET(sendQueue_->Pop(entity));
                CHK_RET(executeService(entity.serviceHandle, entity.args, entity.argsSizeByte));
            }
            // 退出机制 ？注册一个特殊的退出服务，收到这个服务时退出循环
            if (stop_) {
                HCCL_INFO("ServiceScheduler stopping...");
                break;
            }
        }
        return HCCL_SUCCESS;
    }
    HcclResult executeService(ThreadServiceHandle service, void* args, uint64_t argsSize) {
        if (handle2ServiceMap_.find(service) == handle2ServiceMap_.end()) {
            HCCL_ERROR("service not found");
            return HCCL_E_NOT_FOUND;
        }
        auto cb = handle2ServiceMap_[service];
        CHK_RET(cb(args, argsSize));
        return HCCL_SUCCESS;
    }

    HcclResult ServiceRegister(ThreadService serviceCb, ThreadServiceHandle* serviceHandle) {
        CHK_PTR_NULL(serviceHandle);
        std::lock_guard<std::mutex> lock(mtx_);
        if (service2HandleMap_.find(serviceCb) != service2HandleMap_.end()) {
            HCCL_ERROR("[ServiceScheduler::%s] service already registered", __func__);
            return HCCL_E_AGAIN;
        }
        *serviceHandle = reinterpret_cast<uint64_t>(serviceCb);
        // static std::atomic<uint64_t> handleCounter{1};
        // *serviceHandle = handleCounter.fetch_add(1, std::memory_order_relaxed);
        service2HandleMap_.insert(std::make_pair(serviceCb, *serviceHandle));
        handle2ServiceMap_.insert(std::make_pair(*serviceHandle, serviceCb));
        return HCCL_SUCCESS;
    };
    HcclResult ServiceUnregister(ThreadServiceHandle serviceHandle) {
        std::lock_guard<std::mutex> lock(mtx_);
        if (handle2ServiceMap_.find(serviceHandle) == handle2ServiceMap_.end()) {
            HCCL_ERROR("[ServiceScheduler::%s] service handle not found", __func__);
            return HCCL_E_NOT_FOUND;
        }
        auto cb = handle2ServiceMap_[serviceHandle];
        service2HandleMap_.erase(cb);
        handle2ServiceMap_.erase(serviceHandle);
        return HCCL_SUCCESS;
    };
    std::shared_ptr<MsgQueue>& GetSendQueue() {
        return sendQueue_;
    };

    void StopService() {
        stop_ = true;
    };
private:
    std::mutex mtx_;
    std::unordered_map<ThreadService*, ThreadServiceHandle> service2HandleMap_;
    std::unordered_map<ThreadServiceHandle, ThreadService*> handle2ServiceMap_;
    std::shared_ptr<MsgQueue> sendQueue_;
    std::shared_ptr<MsgQueue> completeQueue_; // RDV - not used yet
    bool stop_{false};
};

} // namespace hccl

#endif // SERVICE_SCHEDULER_H

/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef RESOURCE_ENTITIES_H
#define RESOURCE_ENTITIES_H

#include "hccl_res.h"
#include "hccl_api.h"

#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include <mutex>

namespace hccl {

typedef struct {
    NotifyType type;
    uint64_t identifier;  // AicpuTs -> Cpu 场景下, type == DEVICE_MEM，为一块 device 能直接访问的地址
} NotifyEntity;

typedef struct {
    uint64_t addr;         // on HBM
    uint64_t headIdxAddr;  // on HBM, 指向 head 索引的地址
    uint64_t tailIdxAddr;  // on HBM, 指向 tail 索引的地址
    uint32_t msgSize;
    uint32_t capacity;
} QueueInfo;

/**
 * @note 申请内存大小: sizeof(ThreadEntity) + notifyNum * sizeof(NotifyEntity)
 */
typedef struct {
    ThreadType type;
    CommEngine engine;
    union {
        struct {
            QueueInfo sendQueue;
            // QueueInfo completionQueue;  // TODO: for DFX, not planned for now.
            ThreadServiceHandle waitService;
            ThreadServiceHandle recordService;
        } cpuRes;                // 如果是 AICPU Engine + CPU Type, 选择这个成员
        uint64_t threadObjAddr;  // 如果是 AICPU Engine + TS Type, 为 Device 侧的 AicpuTsThread 对象地址
                                 // 如果是 CPU Engine + TS Type, 为 Host 侧的 CpuTsThread 对象地址
        uint32_t raws[128];
    };
    uint32_t notifyNum;
    NotifyEntity notifies[0];  // 变长数据区

    std::string DescribeAttr() {
        return "engine=" + std::to_string(engine) + ", type=" + std::to_string(type);
    }
} ThreadEntity;

typedef struct {
    ThreadHandle threadHandle;
    ThreadHandle dstThreadHandle;
    uint32_t dstNotifyIdx;
} RecordServiceArgs;

typedef struct {
    ThreadHandle threadHandle;
    uint32_t notifyIdx;
} WaitServiceArgs;

typedef struct {
    uint32_t msgId;
    uint64_t serviceHandle;
    void *args;  // -> RecordServiceArgs, WaitServiceArgs, or other custom args struct
    uint64_t argsSizeByte;
} ThreadMsgEntity;

constexpr uint32_t MSG_QUEUE_CAPACITY = 256;
// 循环队列
class MsgQueue {
public:
    MsgQueue() = default;
    MsgQueue(uint64_t capacity, uint64_t msgSize) : capacity_(capacity), msgSize_(msgSize){};
    ~MsgQueue() {
        if (!initialized_) {
            return;
        }
        if (addr_) {
            aclrtFree(reinterpret_cast<void*>(addr_));
        }
        if (head_) {
            aclrtFree(reinterpret_cast<void*>(head_));
        }
        if (tail_) {
            aclrtFree(reinterpret_cast<void*>(tail_));
        }
    }
    HcclResult Init() {
        // 申请内存，初始化队列
        void* mem = nullptr;
        int32_t ret = aclrtMalloc(&mem, capacity_ * msgSize_, ACL_MEM_MALLOC_HUGE_ONLY);
        if (ret != 0) {
            HCCL_ERROR("aclrtMalloc for queue memory failed");
            return HCCL_E_RUNTIME;
        }
        addr_ = reinterpret_cast<uint64_t>(mem);
        void* headPtr{};
        void* tailPtr{};
        ret = aclrtMalloc(&headPtr, sizeof(uint64_t), ACL_MEM_MALLOC_HUGE_ONLY);
        if (ret != 0) {
            HCCL_ERROR("aclrtMalloc for head failed");
            aclrtFree(mem);
            return HCCL_E_RUNTIME;
        }
        ret = aclrtMalloc(&tailPtr, sizeof(uint64_t), ACL_MEM_MALLOC_HUGE_ONLY);
        if (ret != 0) {
            HCCL_ERROR("aclrtMalloc for tail failed");
            aclrtFree(mem);
            aclrtFree(headPtr);
            return HCCL_E_RUNTIME;
        }
        uint64_t zero = 0;
        ret = aclrtMemcpy(headPtr, sizeof(uint64_t), &zero, sizeof(uint64_t), ACL_MEMCPY_HOST_TO_DEVICE);
        if (ret != 0) {
            HCCL_ERROR("aclrtMemcpy for head init failed");
            aclrtFree(mem);
            aclrtFree(headPtr);
            aclrtFree(tailPtr);
            return HCCL_E_RUNTIME;
        }
        ret = aclrtMemcpy(tailPtr, sizeof(uint64_t), &zero, sizeof(uint64_t), ACL_MEMCPY_HOST_TO_DEVICE);
        if (ret != 0) {
            HCCL_ERROR("aclrtMemcpy for tail init failed");
            aclrtFree(mem);
            aclrtFree(headPtr);
            aclrtFree(tailPtr);
            return HCCL_E_RUNTIME;
        }
        head_ = reinterpret_cast<uint64_t>(headPtr);
        tail_ = reinterpret_cast<uint64_t>(tailPtr);
        initialized_ = true;
        return HCCL_SUCCESS;
    };

    HcclResult pop(ThreadMsgEntity &entity) {
        if (!initialized_) {
            return HCCL_E_PARA;
        }

        // 检查队列是否为空
        bool isEmpty = false;
        CHK_RET(empty(isEmpty));
        if (isEmpty) {
            return HCCL_E_AGAIN;  // 队列为空
        }

        // 1. 获取当前 head 值
        uint64_t head;
        aclError err = aclrtMemcpy(&head, sizeof(uint64_t), 
                                   reinterpret_cast<void*>(head_), sizeof(uint64_t), 
                                   ACL_MEMCPY_DEVICE_TO_HOST);
        if (err != ACL_ERROR_NONE) {
            return HCCL_E_RUNTIME;
        }

        // 2. 获取当前 tail 值（用于计算队列长度）
        uint64_t tail;
        err = aclrtMemcpy(&tail, sizeof(uint64_t), 
                          reinterpret_cast<void*>(tail_), sizeof(uint64_t), 
                          ACL_MEMCPY_DEVICE_TO_HOST);
        if (err != ACL_ERROR_NONE) {
            return HCCL_E_RUNTIME;
        }

        // 3. 计算消息在队列中的偏移位置
        uint64_t offset = (head % capacity_) * msgSize_;
        void* msgPtr = reinterpret_cast<void*>(addr_ + offset);

        // 4. 从设备端队列拷贝消息到主机端
        err = aclrtMemcpy(&entity, msgSize_, msgPtr, msgSize_, ACL_MEMCPY_DEVICE_TO_HOST);
        if (err != ACL_ERROR_NONE) {
            return HCCL_E_RUNTIME;
        }

        // 5. 更新 head 指针
        head++;
        err = aclrtMemcpy(reinterpret_cast<void*>(head_), sizeof(uint64_t), 
                          &head, sizeof(uint64_t), ACL_MEMCPY_HOST_TO_DEVICE);
        if (err != ACL_ERROR_NONE) {
            return HCCL_E_RUNTIME;
        }
        return HCCL_SUCCESS;
    }

    HcclResult push(const ThreadMsgEntity &entity) {
        if (!initialized_) {
            HCCL_ERROR("[MsgQueue::%s] MsgQueue not initialized", __func__);
            return HCCL_E_PARA;
        }

        // 检查队列是否为满
        bool isFull = false;
        CHK_RET(full(isFull));
        if (isFull) {
            HCCL_ERROR("[MsgQueue::%s] MsgQueue is full", __func__);
            return HCCL_E_AGAIN;  // 队列为满
        }

        // 1. 获取当前 head 值（用于检查满）
        uint64_t head;
        aclError err = aclrtMemcpy(&head, sizeof(uint64_t), 
                                   reinterpret_cast<void*>(head_), sizeof(uint64_t), 
                                   ACL_MEMCPY_DEVICE_TO_HOST);
        if (err != ACL_ERROR_NONE) {
            return HCCL_E_RUNTIME;
        }

        // 2. 获取当前 tail 值
        uint64_t tail;
        err = aclrtMemcpy(&tail, sizeof(uint64_t), 
                          reinterpret_cast<void*>(tail_), sizeof(uint64_t), 
                          ACL_MEMCPY_DEVICE_TO_HOST);
        if (err != ACL_ERROR_NONE) {
            return HCCL_E_RUNTIME;
        }

        // 3. 计算消息在队列中的偏移位置
        uint64_t offset = (tail % capacity_) * msgSize_;
        void* msgPtr = reinterpret_cast<void*>(addr_ + offset);

        // 4. 从主机端拷贝消息到设备端队列
        err = aclrtMemcpy(msgPtr, msgSize_, &entity, msgSize_, ACL_MEMCPY_HOST_TO_DEVICE);
        if (err != ACL_ERROR_NONE) {
            return HCCL_E_RUNTIME;
        }

        // 5. 更新 tail 指针
        tail++;
        err = aclrtMemcpy(reinterpret_cast<void*>(tail_), sizeof(uint64_t), 
                          &tail, sizeof(uint64_t), ACL_MEMCPY_HOST_TO_DEVICE);
        if (err != ACL_ERROR_NONE) {
            return HCCL_E_RUNTIME;
        }
        return HCCL_SUCCESS;
    };
    HcclResult empty(bool &isEmpty) {
        uint64_t head, tail;
        aclError ret = aclrtMemcpy(&head, sizeof(uint64_t), reinterpret_cast<void*>(head_), sizeof(uint64_t), ACL_MEMCPY_DEVICE_TO_HOST);
        if (ret != ACL_ERROR_NONE) {
            HCCL_ERROR("[MsgQueue::%s] copy head from device failed: %d", __func__, ret);
            isEmpty = true;
            return HCCL_E_RUNTIME;
        }
        ret = aclrtMemcpy(&tail, sizeof(uint64_t), reinterpret_cast<void*>(tail_), sizeof(uint64_t), ACL_MEMCPY_DEVICE_TO_HOST);
        if (ret != ACL_ERROR_NONE) {
            HCCL_ERROR("[MsgQueue::%s] copy tail from device failed: %d", __func__, ret);
            isEmpty = true;
            return HCCL_E_RUNTIME;
        }
        head = head % capacity_;
        tail = tail % capacity_;
        isEmpty = (head == tail);
        return HCCL_SUCCESS;
    };

    HcclResult full(bool &isFull) {
        uint64_t head, tail;
        aclError ret = aclrtMemcpy(&head, sizeof(uint64_t), reinterpret_cast<void*>(head_), sizeof(uint64_t), ACL_MEMCPY_DEVICE_TO_HOST);
        if (ret != ACL_ERROR_NONE) {
            HCCL_ERROR("[MsgQueue::%s] copy head from device failed: %d", __func__, ret);
            isFull = true;
            return HCCL_E_RUNTIME;
        }
        ret = aclrtMemcpy(&tail, sizeof(uint64_t), reinterpret_cast<void*>(tail_), sizeof(uint64_t), ACL_MEMCPY_DEVICE_TO_HOST);
        if (ret != ACL_ERROR_NONE) {
            HCCL_ERROR("[MsgQueue::%s] copy tail from device failed: %d", __func__, ret);
            isFull = true;
            return HCCL_E_RUNTIME;
        }
        head = head % capacity_;
        tail = tail % capacity_;
        isFull = ((tail + 1) % capacity_ == head);
        return HCCL_SUCCESS;
    };

    QueueInfo GetQueueInfo() {
        QueueInfo queueInfo;
        queueInfo.addr = addr_;
        queueInfo.headIdxAddr = head_;
        queueInfo.tailIdxAddr = tail_;
        queueInfo.msgSize = msgSize_;
        queueInfo.capacity = capacity_;
        return queueInfo;
    };
private:
    bool initialized_{false};
    uint64_t addr_{0};
    uint64_t capacity_{0};
    uint64_t msgSize_{0};
    uint64_t head_{0};
    uint64_t tail_{0};
    // 一条消息包含：ThreadMsgEntity
};

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
            CHK_RET(sendQueue_->empty(isEmpty));
            if (!isEmpty) {
                ThreadMsgEntity entity{};
                CHK_RET(sendQueue_->pop(entity));
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

#endif // RESOURCE_ENTITIES_H

/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef MSG_QUEUE_H
#define MSG_QUEUE_H

#include "hccl_res.h"
#include "resource_entities.h"
#include <cstdint>

namespace hccl {

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

    HcclResult Pop(ThreadMsgEntity &entity) {
        if (!initialized_) {
            return HCCL_E_PARA;
        }

        // 检查队列是否为空
        bool isEmpty = false;
        CHK_RET(Empty(isEmpty));
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

    HcclResult Push(const ThreadMsgEntity &entity) {
        if (!initialized_) {
            HCCL_ERROR("[MsgQueue::%s] MsgQueue not initialized", __func__);
            return HCCL_E_PARA;
        }

        // 检查队列是否为满
        bool isFull = false;
        CHK_RET(Full(isFull));
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
    HcclResult Empty(bool &isEmpty) {
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

    HcclResult Full(bool &isFull) {
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

} // namespace hccl

#endif // MSG_QUEUE_H

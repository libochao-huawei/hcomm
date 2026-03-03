/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef HCCL_CPU_THREAD_H
#define HCCL_CPU_THREAD_H
#include <memory>
#include <vector>
#include <unordered_map>
#include "thread.h"
#include "resource_entities.h"
#include "acl/acl_rt.h"
#include "ascend_hal.h"
#define SVM_REGISTER_FLAG_WITH_ACCESS_VA (1ULL << 0ULL)
#define SVM_MEM_READ (0x1 << 0)
#define SVM_MEM_WRITE (0x1 << 1)
extern drvError_t __attribute__((weak)) halSvmRegister(uint32_t dev_id, uint64_t va, uint64_t size, uint64_t flag, uint64_t *access_va);
extern drvError_t __attribute__((weak)) halSvmAccess(uint32_t dev_id, uint64_t dst, uint64_t src, uint64_t size, uint64_t flag);
extern drvError_t __attribute__((weak)) halSvmUnregister(uint32_t dev_id, uint64_t va, uint64_t size, uint64_t flag);

//-----------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    ThreadHandle thread;
    ThreadHandle dstThread;
    uint32_t notifyIdx;
} RecordServiceArgs;

int32_t RecordService (void* args, uint64_t argsSize)
{
    // TODO: 补充流程
    return 0;
}
typedef struct {
    ThreadHandle thread;
    uint32_t notifyIdx;
} WaitServiceArgs;

int32_t WaitService (void* args, uint64_t argsSize)
{
    // TODO: 补充流程
    return 0;
}

namespace hccl {

// 循环队列
class MsgQueue {
public:
    MsgQueue() = default;
    MsgQueue(uint64_t capacity, uint64_t msgSize) : capacity_(capacity), msgSize_(msgSize){};
    ~MsgQueue() = default;
    HcclResult Init() {
        // 申请内存，初始化队列
        void* mem;
        int32_t ret = aclrtMalloc(&mem, capacity_ * msgSize_, ACL_MEM_MALLOC_HUGE_ONLY);
        if (ret != 0) {
            HCCL_ERROR("err");
            return HCCL_E_RUNTIME;
        }
        addr_ = reinterpret_cast<uint64_t>(mem);
        void* headPtr;
        void* tailPtr;
        ret = aclrtMalloc(&headPtr, sizeof(uint64_t), ACL_MEM_MALLOC_HUGE_ONLY);
        if (ret != 0) {
            HCCL_ERROR("err");
            return HCCL_E_RUNTIME;
        }
        ret = aclrtMalloc(&tailPtr, sizeof(uint64_t), ACL_MEM_MALLOC_HUGE_ONLY);
        if (ret != 0) {
            HCCL_ERROR("err");
            return HCCL_E_RUNTIME;
        }
        ret = aclrtMemcpy(headPtr, sizeof(uint64_t), 0, sizeof(uint64_t), ACL_MEMCPY_HOST_TO_DEVICE);
        if (ret != 0) {
            HCCL_ERROR("err");
            return HCCL_E_RUNTIME;
        }
        ret = aclrtMemcpy(tailPtr, sizeof(uint64_t), 0, sizeof(uint64_t), ACL_MEMCPY_HOST_TO_DEVICE);
        if (ret != 0) {
            HCCL_ERROR("err");
            return HCCL_E_RUNTIME;
        }
        head_ = reinterpret_cast<uint64_t>(headPtr);
        tail_ = reinterpret_cast<uint64_t>(tailPtr);
        return HCCL_SUCCESS;
    };

    HcclResult pop(ThreadMsgEntity &entity) {
        // TODO: 补充流程
        return HCCL_SUCCESS;
    };
    HcclResult push(const ThreadMsgEntity &entity) {
        // TODO: 补充流程
        return HCCL_SUCCESS;
    };
    bool empty() {
        uint64_t head, tail;
        aclrtMemcpy(&head, sizeof(uint64_t), reinterpret_cast<void*>(head_), sizeof(uint64_t), ACL_MEMCPY_DEVICE_TO_HOST);
        aclrtMemcpy(&tail, sizeof(uint64_t), reinterpret_cast<void*>(tail_), sizeof(uint64_t), ACL_MEMCPY_DEVICE_TO_HOST);
        return (head == tail);
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
    uint64_t addr_;
    uint64_t capacity_;
    uint64_t msgSize_;
    uint64_t head_;
    uint64_t tail_;
    // 一条消息包含：ThreadServiceHandle + ThreadServiceArgs
};

class ServiceScheduler {
public:
    ServiceScheduler() = default;
    ~ServiceScheduler();
    HcclResult Init() {
        // 初始化sq和cq
        sendQueue_ = std::make_shared<MsgQueue>(1024, 128);
        completeQueue_ = std::make_shared<MsgQueue>(1024, 128);
        return HCCL_SUCCESS;
    };
    HcclResult ServiceRun() {
        while (true) {
            if (!sendQueue_->empty()) {
                ThreadMsgEntity entity;
                sendQueue_->pop(entity);
                executeService(entity.serviceHandle, entity.args, entity.argsSizeByte);
            }
        }
        return HCCL_SUCCESS;
    }
    HcclResult executeService(ThreadServiceHandle service, void* args, uint64_t argsSize) {
        //
        if (handle2ServiceMap_.find(service) == handle2ServiceMap_.end()) {
            HCCL_ERROR("service not found");
            return HCCL_E_NOT_FOUND;
        }
        auto cb = handle2ServiceMap_[service];
        cb(args, argsSize);
        return HCCL_SUCCESS;
    }

    HcclResult ServiceRegister(ThreadService serviceCb, ThreadServiceHandle* serviceHandle) {
        CHK_PTR_NULL(serviceHandle);
        if (service2HandleMap_.find(serviceCb) != service2HandleMap_.end()) {
            HCCL_ERROR("xxxxxxxxxxxxxxxxxxxxxxxxxxxx");
            return HCCL_E_AGAIN;
        }
        *serviceHandle = reinterpret_cast<uint64_t>(serviceCb);
        service2HandleMap_.insert(std::make_pair(serviceCb, *serviceHandle));
        handle2ServiceMap_.insert(std::make_pair(*serviceHandle, serviceCb));
        return HCCL_SUCCESS;
    };
    HcclResult ServiceUnregister(ThreadServiceHandle serviceHandle) {
        if (handle2ServiceMap_.find(serviceHandle) == handle2ServiceMap_.end()) {
            HCCL_ERROR("not found");
            return HCCL_E_NOT_FOUND;
        }
        auto cb = handle2ServiceMap_[serviceHandle];
        service2HandleMap_.erase(cb);
        handle2ServiceMap_.erase(serviceHandle);
        return HCCL_SUCCESS;
    };
    std::shared_ptr<MsgQueue> GetSendQueue() {
        return sendQueue_;
    };
private:
    std::mutex mtx_;
    std::unordered_map<ThreadService*, ThreadServiceHandle> service2HandleMap_;
    std::unordered_map<ThreadServiceHandle, ThreadService*> handle2ServiceMap_;
    std::shared_ptr<MsgQueue> sendQueue_;
    std::shared_ptr<MsgQueue> completeQueue_; // RDV
};

class MemNotify {
public:
    ~MemNotify() {
        //TODO: 内存销毁
    };
    HcclResult NotifyRecord() {
        return HCCL_SUCCESS;
    };
    HcclResult NotifyWait() {
        return HCCL_SUCCESS;
    };
    HcclResult Alloc(uint32_t notifySize) {
        s32 devId = 0;
        CHK_RET(hrtGetDevice(&devId));
        int64_t connectType = 0;
        hrtHalGetDeviceInfo(devId, MODULE_TYPE_SYSTEM, INFO_TYPE_HD_CONNECT_TYPE, &connectType); // ?
        if (connectType == 1) { // PCIe
            // 申请notify
            hrtMalloc(&notifyDeviceVa_, notifySize);
            // 对齐
            uint64_t va = static_cast<uint64_t>(notifySize + 4096ULL);
            uint64_t registerVa = (va + (4096ULL - 1ULL)) & ~(4096ULL - 1ULL);
            uint64_t accessVa;
            int32_t ret = halSvmRegister(devId, registerVa, notifySize, SVM_REGISTER_FLAG_WITH_ACCESS_VA, &accessVa);
            if (ret != 0) {
                HCCL_ERROR("halSvmRegister failed, ret = %d", ret);
                return HCCL_E_INTERNAL;
            }
            notifyDeviceVa_ = reinterpret_cast<void*>(accessVa);
        } else {
            //
            notifyHostVa_ = malloc(notifySize);
            aclError aclRet = aclrtHostRegister(notifyHostVa_, notifySize, ACL_HOST_REGISTER_MAPPED, &notifyDeviceVa_);
            if (aclRet != ACL_SUCCESS) {
                HCCL_ERROR("aclrtHostRegister failed, ret = %d", aclRet);
                free(notifyHostVa_);
                return HCCL_E_INTERNAL;
            }
        }
        return HCCL_SUCCESS;
    };
    uint64_t GetIdentifier() {
        return reinterpret_cast<uint64_t>(notifyDeviceVa_);
    };
private:
    void* notifyHostVa_{};
    void* notifyDeviceVa_{};
};

class CpuThread : public Thread {
public:
    CpuThread(aclrtStream rtStream, uint32_t notifyNum, const NotifyLoadType notifyLoadType)
        : dpuStream_(rtStream), notifyNum_(notifyNum), notifyLoadType_(notifyLoadType) {};
    CpuThread(StreamType streamType, uint32_t notifyNum, const NotifyLoadType notifyLoadType)
        : streamType_(streamType), notifyNum_(notifyNum), notifyLoadType_(notifyLoadType) {};

    HcclResult Init() override;
    HcclResult DeInit() override;

    std::string &GetUniqueId() override {
        static std::string uniqueId = "CpuThread";
        return uniqueId;
    };
    LocalNotify *GetNotify(uint32_t index) const override {
        return nullptr;
    };

    // A3 Stream & A5 Stream
    bool IsDeviceA5() const {
        return true;
    };
    Stream *GetStream() const {
        return nullptr;
    };
    void *GetStreamLitePtr() const override {
        return nullptr;
    };
    void LaunchTask() const override {
        // Do nothing
    };

    // Local Data Plane Functions
    HcclResult LocalNotifyRecord(uint32_t notifyId) const override {
        return HCCL_E_NOT_SUPPORT;
    };
    HcclResult LocalNotifyWait(uint32_t notifyId) const override {
        return HCCL_E_NOT_SUPPORT;
    };
    HcclResult LocalCopy(void *dst, const void *src, uint64_t sizeByte) const override {
        return HCCL_E_NOT_SUPPORT;
    };
    HcclResult LocalReduce(
        void *dst, const void *src, uint64_t sizeByte, HcommDataType dataType, HcommReduceOp reduceOp) const override {
        return HCCL_E_NOT_SUPPORT;
    };

    ~CpuThread();

    HcclResult ServiceRegister(ThreadService serviceCb, ThreadServiceHandle* serviceHandle);
    HcclResult ServiceUnregister(ThreadServiceHandle service);
    HcclResult KernelRun();
    HcclResult GetThreadEntity(ThreadEntity* threadEntity);
    MemNotify* GetNotify(uint32_t notifyIndex);
    uint32_t GetNotifyNum() const override;

private:
    HcclResult PrepareDpuKernelResource(aclrtFuncHandle &funcHandle);
    HcclResult LaunchDpuKernel(aclrtFuncHandle &funcHandle);

    StreamType streamType_ = StreamType::STREAM_TYPE_RESERVED;
    aclrtStream dpuStream_{};
    uint32_t notifyNum_ = 0;
    NotifyLoadType notifyLoadType_ = NotifyLoadType::HOST_NOTIFY;
    
    aclrtContext dpuContext_;
    aclrtContext npuContext_;
    std::unique_ptr<ServiceScheduler> serviceScheduler_{};
    std::vector<std::unique_ptr<MemNotify>> notifys_;
    ThreadServiceHandle recordServiceHandle_;
    ThreadServiceHandle waitServiceHandle_;
};

__attribute__((visibility("default"))) uint32_t RunDpuRpcSrvLaunch(const uint64_t args)
{
    struct DpuKernelLaunchParam {
        void*       cpuThread;
        int32_t    deviceId;
    };

    HCCL_INFO("[%s] Launch Dpu Kernel: 0x%lx", __func__, args);
    if (args == 0) {
        HCCL_ERROR("[%s] args is null.", __func__);
        return HCCL_E_PARA;
    }

    // 解析参数信息
    DpuKernelLaunchParam *params = reinterpret_cast<DpuKernelLaunchParam *>(args);

    // 实例化
    CpuThread* cpuThread = static_cast<CpuThread*>(params->cpuThread);

    aclError ret = aclrtSetDevice(params->deviceId);
    if (ret != ACL_SUCCESS) {
        HCCL_ERROR("[%s] set device fail. DeviceId: %d.", __func__, params->deviceId);
        return HCCL_E_RUNTIME;
    }

    // Run
    HCCL_INFO("[%s] start to TaskRun", __func__);
    CHK_RET(cpuThread->KernelRun());
    return HCCL_SUCCESS;
}

}  // namespace hccl

#endif  // HCCL_CPU_THREAD_H

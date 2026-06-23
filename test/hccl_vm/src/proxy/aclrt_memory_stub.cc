/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <atomic>
#include <cstdint>
#include <iostream>
#include <unistd.h>
#include <vector>

#include "acl/acl_base.h"
#include "acl/acl_rt.h"
#include "hccl_proxy_common.h"
#include "store_sim_store_pub.h"
#include "sim_log.h"
#include "rt_external_mem.h"
#include "store_sim_device_memory_manager.h"
#include "sim_models.h"
#include "db_sim_runner_ops.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

std::string GenDevMemName(int deviceId)
{
    std::ostringstream oss;
    oss << "DEV" << deviceId << "_" << getpid() << "_" << std::this_thread::get_id() << "_" << std::chrono::steady_clock::now().time_since_epoch().count();
    return oss.str();
}

bool GetPtrNameByVirPtr(const void* virPtr, uint32_t& offset, sim::PhyMemBlock &phyMem)
{
    uint64_t devPtr = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(virPtr));
    auto virMemRes = RunnerDB::GetOneByPred<sim::VirtualMemBlock>(
        [devPtr](const sim::VirtualMemBlock &virMem)
        { return ((virMem.start_ptr <= devPtr) &&
                    (devPtr < (virMem.start_ptr + virMem.size)) &&
                    (virMem.src_type == (uint8_t)sim::VIR_MEM_TYPE_DEV)); });
    if (!virMemRes.second) {
        HCCL_VM_ERROR("can not find this buff offset ptr:{:p}", virPtr);
        return false;
    }

    auto phyMemId = virMemRes.first.phy_mem_id;
    auto phyMemRes = RunnerDB::GetById<sim::PhyMemBlock>(phyMemId);
    if (!phyMemRes.has_value()) {
        HCCL_VM_ERROR("can not find phy Mem id:{:d}", phyMemId);
        return false;
    };
    offset = (uint32_t)(devPtr - virMemRes.first.start_ptr);
    phyMem = *phyMemRes;
    return true;
}

uint32_t GetRankIdByVirAddr(const void* virAddr)
{
    sim::PhyMemBlock phyMem{};
    uint32_t offset = 0;
    if (!GetPtrNameByVirPtr(virAddr, offset, phyMem)) {
        return 0;
    }

    uint32_t deviceId = phyMem.device_id;
    auto rank = RunnerDB::GetOneByPred<sim::Rank>([deviceId](const sim::Rank &rank) { return rank.device_id == deviceId; });
    if (!rank.second) {
        HCCL_VM_ERROR("cannot find rank by device id[{}]", deviceId);
        return 0;
    }

    return rank.first.rank_id;
}

void* GetRealPtrByAddr(const void* virPtr)
{
    uint64_t devPtr = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(virPtr));
    auto virMemRes = RunnerDB::GetOneByPred<sim::VirtualMemBlock>(
        [devPtr](const sim::VirtualMemBlock &virMem)
        { return ((virMem.start_ptr <= devPtr) &&
                    (devPtr < (virMem.start_ptr + virMem.size)) &&
                    (virMem.src_type == (uint8_t)sim::VIR_MEM_TYPE_DEV)); });
    if (!virMemRes.second) {
        HCCL_VM_ERROR("can not find this buff offset ptr:{:p}", virPtr);
        return nullptr;
    }

    void* devStartPtr = (void*)(uintptr_t)virMemRes.first.start_ptr;
    auto hostPtr = sim::DeviceMemoryManager::GetInstance().GetHostPtrByDevPtr(devStartPtr);
    if (hostPtr == nullptr) {
        HCCL_VM_ERROR("can not find host ptr by dev ptr:{:p}", devStartPtr);
        return nullptr;
    }

    hostPtr = ((char*)hostPtr + (devPtr - (uint64_t)(uintptr_t)devStartPtr));
    return hostPtr;
}

aclError aclrtMallocHost(void **hostPtr, size_t size)
{
    *hostPtr = malloc(size);
    HCCL_VM_INFO("[MEM] malloc host addr:{:p}, size:{:d}", *hostPtr, size);
    return ACL_SUCCESS;
}

aclError aclrtMallocHostWithCfg(void **ptr, uint64_t size, aclrtMallocConfig *cfg)
{
    (void) cfg;
    return aclrtMallocHost(ptr, size);
}

aclError aclrtFreeHost(void *hostPtr)
{
    HCCL_VM_INFO("[MEM] free host addr:{:p}", hostPtr);
    free(hostPtr);
    return ACL_SUCCESS;
}

aclError aclrtMalloc(void **devPtr, size_t size, aclrtMemMallocPolicy policy)
{
    sim::Runner runner;
    if (!sim::GetCurrRunnerTls(0, runner)) {
        return ACL_ERROR_INVALID_PARAM;
    }
    auto currCtx = RunnerDB::GetById<sim::Context>(runner.current_ctx_id);
    if (!currCtx.has_value()) {
        HCCL_VM_ERROR("can not find current context:{:d}", runner.current_ctx_id);
        return ACL_ERROR_INTERNAL_ERROR;
    }

    auto deviceId = currCtx->device_id;

    auto dev = RunnerDB::GetById<sim::Device>(deviceId);
    if (!dev.has_value()) {
        HCCL_VM_ERROR("can not find device id:{:d}", deviceId);
        return ACL_ERROR_INTERNAL_ERROR;
    }

    auto rankId = (uint32_t)sim::GetCurrRankId();

    auto virPtr = sim::DeviceMemoryManager::GetInstance().AllocVirMem(rankId, size);
    if (virPtr == nullptr) {
        HCCL_VM_ERROR("can not alloc vir mem deviceId:{:d}, phyMemId:{:d}, size:{:d}", deviceId, rankId, size);
        return ACL_ERROR_INTERNAL_ERROR;
    }

    std::string memName = GenDevMemName(deviceId);
    auto hostPtr = sim::DeviceMemoryManager::GetInstance().AllocPhyMem(memName.c_str(), deviceId, size);
    if (hostPtr == nullptr) {
        sim::DeviceMemoryManager::GetInstance().FreeVirMem(rankId, devPtr);
        HCCL_VM_ERROR("can not alloc phy mem deviceId:{:d}, size:{:d}", deviceId, size);
        return ACL_ERROR_INTERNAL_ERROR;
    }

    // 记录物理内存信息到数据库
    sim::PhyMemBlock phyMem{};
    phyMem.device_id = deviceId;
    memcpy(phyMem.name, memName.c_str(), memName.size());
    phyMem.size = size;
    phyMem.ref_count = 1;
    auto phyMemId = RunnerDB::Add<sim::PhyMemBlock>(phyMem);

    // 记录虚拟内存信息到数据库
    sim::VirtualMemBlock virMem{};
    virMem.start_ptr = (uint64_t)(uintptr_t)virPtr;
    virMem.size = size;
    virMem.ctx_id = runner.current_ctx_id;
    virMem.phy_mem_id = phyMemId;
    virMem.owner_pid = runner.pid;
    virMem.src_type = (uint8_t)sim::VIR_MEM_TYPE_DEV;
    virMem.policy = (uint8_t)policy;
    RunnerDB::Add<sim::VirtualMemBlock>(virMem);

    *devPtr = virPtr;
    // 缓存设备地址到host地址
    sim::DeviceMemoryManager::GetInstance().MapDevPtrHostPtr(virPtr, hostPtr);
    HCCL_VM_INFO("malloc dev addr:{:p}, memName:{}, size:{:d}", virPtr, memName, size);
    return ACL_SUCCESS;
}

aclError aclrtMallocAlign32(void **devPtr, size_t size, aclrtMemMallocPolicy policy)
{
    size_t realSize = ((size + 31) / 32) * 32;
    return aclrtMalloc(devPtr, realSize, policy);
}

aclError aclrtMallocCached(void **devPtr, size_t size, aclrtMemMallocPolicy policy)
{
    return aclrtMalloc(devPtr, size, policy);
}

aclError aclrtMemFlush(void *devPtr, size_t size)
{
    (void) devPtr;
    (void) size;
    return ACL_SUCCESS;
}

aclError aclrtMemInvalidate(void *devPtr, size_t size)
{
    (void) devPtr;
    (void) size;
    return ACL_SUCCESS;
}

aclError aclrtMallocWithCfg(void **devPtr, size_t size, aclrtMemMallocPolicy policy, aclrtMallocConfig *cfg)
{
    (void) cfg;
    return aclrtMalloc(devPtr, size, policy);
}

aclError aclrtMallocForTaskScheduler(void **devPtr, size_t size, aclrtMemMallocPolicy policy, aclrtMallocConfig *cfg)
{
    (void) cfg;
    return aclrtMalloc(devPtr, size, policy);
}

aclError aclrtFree(void* devPtr)
{
    uint64_t startPtr = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(devPtr));
    auto virMemRes = RunnerDB::GetOneByPred<sim::VirtualMemBlock>(
        [startPtr](const sim::VirtualMemBlock &virMem)
        { return virMem.start_ptr == startPtr && virMem.src_type == (uint8_t)sim::VIR_MEM_TYPE_DEV; });
    if (!virMemRes.second) {
        HCCL_VM_ERROR("can not find this buff offset ptr:0x{:x}", startPtr);
        return ACL_ERROR_INTERNAL_ERROR;
    }

    auto phyMemId = virMemRes.first.phy_mem_id;
    auto phyMemRes = RunnerDB::GetById<sim::PhyMemBlock>(phyMemId);
    if (!phyMemRes.has_value()) {
        HCCL_VM_ERROR("can not find phy Mem id:{:d}", phyMemId);
        return ACL_ERROR_INTERNAL_ERROR;
    };

    auto deviceId = phyMemRes->device_id;
    auto dev = RunnerDB::GetById<sim::Device>(deviceId);
    if (!dev.has_value()) {
        HCCL_VM_ERROR("can not find device id:{:d}", deviceId);
        return ACL_ERROR_INTERNAL_ERROR;
    }

    auto devPhyId = dev->physical_id;

    // 去缓存设备地址到host地址
    sim::DeviceMemoryManager::GetInstance().UnmapDevPtrHostPtr(devPtr);

    // 删除内存
    sim::DeviceMemoryManager::GetInstance().FreeVirMem(devPhyId, devPtr);
    sim::DeviceMemoryManager::GetInstance().FreePhyMem(phyMemRes->name, deviceId);

    // 更新数据库记录，标记为已释放
    RunnerDB::Update<sim::PhyMemBlock>(phyMemId, [](sim::PhyMemBlock &memBlock) { memBlock.is_freed = 1; });
    RunnerDB::Update<sim::VirtualMemBlock>(virMemRes.first.id, [](sim::VirtualMemBlock &virtMemBlock) { virtMemBlock.is_freed = 1; });

    HCCL_VM_INFO("free dev addr:{:p}, memName:{}, size:{:d}", devPtr, phyMemRes->name, virMemRes.first.size);
    return ACL_SUCCESS;
}

aclError aclrtMemset(void *devPtr, size_t maxCount, int32_t value, size_t count)
{
    sim::PhyMemBlock phyMem{};
    uint32_t offset = 0;
    if (!GetPtrNameByVirPtr(devPtr, offset, phyMem)) {
        memset(devPtr, value, count);
        HCCL_VM_INFO("[MEM] sys mem memset ptr:{:p}, maxCount: {:d}, value: {:d}, count: {:d}", devPtr, maxCount, value, count);
        return ACL_SUCCESS;
    }

    char* hostPtr = (char*)sim::DeviceMemoryManager::GetInstance().AcquirePhyMem(phyMem.name, phyMem.device_id, phyMem.size);
    if (hostPtr == nullptr) {
        HCCL_VM_INFO("[MEM] dev mem memset ptr:{:p} name:{} can not get host ptr", devPtr, phyMem.name);
        return ACL_ERROR_INTERNAL_ERROR;
    }

    memset(hostPtr + offset, value, count);
    sim::DeviceMemoryManager::GetInstance().ReleasePhyMem(phyMem.name, phyMem.device_id);
    HCCL_VM_INFO("[MEM] dev mem memset ptr:{:p}, memName:{}, count: {:d}", devPtr, phyMem.name, count);
    return ACL_SUCCESS;
}

aclError aclrtMemsetAsync(void *devPtr, size_t maxCount, int32_t value, size_t count, aclrtStream stream)
{
    (void) stream;
    // 当前先不生成任务，后续有需要再根据实际情况生成任务
    return aclrtMemset(devPtr, maxCount, value, count);
}

aclError aclrtMemcpy(void *dst, size_t destMax, const void *src, size_t count, aclrtMemcpyKind kind)
{
    (void) destMax;
    void* hostPtr = nullptr;
    void* devPtr = nullptr;
    if (kind == ACL_MEMCPY_HOST_TO_HOST) {
        memcpy(dst, src, count);
        return ACL_SUCCESS;
    } else if (kind == ACL_MEMCPY_HOST_TO_DEVICE) {
        hostPtr = const_cast<void *>(src);
        devPtr = dst;
    } else if (kind == ACL_MEMCPY_DEVICE_TO_HOST) {
        hostPtr = dst;
        devPtr = const_cast<void *>(src);
    } else if (kind == ACL_MEMCPY_DEVICE_TO_DEVICE) {
        auto dstAddr =  GetRealPtrByAddr(dst);
        if (dstAddr == nullptr) {
            HCCL_VM_WARN("[aclrtMemcpy D2D] only support self D2D memcpy");
            return ACL_ERROR_RT_FEATURE_NOT_SUPPORT;
        }
        auto srcAddr =  GetRealPtrByAddr(src);
        if (srcAddr == nullptr) {
            HCCL_VM_WARN("[aclrtMemcpy D2D] only support self D2D memcpy");
            return ACL_ERROR_RT_FEATURE_NOT_SUPPORT;
        }
        memcpy(dstAddr, srcAddr, count);
        return ACL_SUCCESS;
    } else {
        HCCL_VM_INFO("src:{:p} to dst:{:p}, size:{:d} type: {:d} not support", src, dst, count, (int)kind);
        return ACL_ERROR_RT_FEATURE_NOT_SUPPORT;
    }

    sim::PhyMemBlock phyMem{};
    uint32_t offset = 0;
    if (!GetPtrNameByVirPtr(devPtr, offset, phyMem)) {  
        HCCL_VM_INFO("[MEM] dev mem memcpy ptr:{:p}, count: {:d}", devPtr, count);
        return ACL_ERROR_INTERNAL_ERROR;
    }

    HCCL_VM_INFO("host:{:p} dev:{:p} memName:{} size:{:d} type:{:d}", hostPtr, devPtr, phyMem.name, count, (int)kind);
    char* hostDevPtr = (char*)sim::DeviceMemoryManager::GetInstance().AcquirePhyMem(phyMem.name, phyMem.device_id, phyMem.size);
    if (hostDevPtr == nullptr) {
        HCCL_VM_INFO("[MEM] dev mem memcpy ptr:{:p} name:{} can not get host ptr", devPtr, phyMem.name);
        return ACL_ERROR_INTERNAL_ERROR;
    }

    if (kind == ACL_MEMCPY_HOST_TO_DEVICE) {
        memcpy(hostDevPtr + offset, hostPtr, count);
    } else if (kind == ACL_MEMCPY_DEVICE_TO_HOST) {
        memcpy(hostPtr, hostDevPtr + offset, count);
    }

    sim::DeviceMemoryManager::GetInstance().ReleasePhyMem(phyMem.name, phyMem.device_id);

    return ACL_SUCCESS;
}

aclError aclrtMemcpyAsync(void *dst, size_t destMax, const void *src, size_t count, aclrtMemcpyKind kind, aclrtStream stream)
{
    (void) destMax;
    void* hostPtr = nullptr;
    void* devPtr = nullptr;
    if (kind == ACL_MEMCPY_HOST_TO_HOST) {
        memcpy(dst, src, count);
        return ACL_SUCCESS;
    } else if (kind == ACL_MEMCPY_HOST_TO_DEVICE) {
        hostPtr = const_cast<void *>(src);
        devPtr = dst;
    } else if (kind == ACL_MEMCPY_DEVICE_TO_HOST) {
        hostPtr = dst;
        devPtr = const_cast<void *>(src);
    } else if (kind == ACL_MEMCPY_DEVICE_TO_DEVICE) {
        auto curRank = (uint32_t)sim::GetCurrRankId();
        HcclTaskMetaData taskMetaData{};
        taskMetaData.taskType = HccLTaskMetaType::MEM_CPY;
        taskMetaData.commId   = 0;
        taskMetaData.rankId   = curRank;
        taskMetaData.streamId = sim::GetCurrentStreamId((uint64_t)(uintptr_t)stream);
        taskMetaData.taskData.transMem.srcRankId = GetRankIdByVirAddr(src);
        taskMetaData.taskData.transMem.srcOffset = (uint64_t)(uintptr_t)src;
        taskMetaData.taskData.transMem.dstRankId = GetRankIdByVirAddr(dst);
        taskMetaData.taskData.transMem.dstOffset = (uint64_t)(uintptr_t)dst;
        taskMetaData.taskData.transMem.len       = count;

        uint32_t index{0};
        auto ret = InsertTaskToCollection(&taskMetaData, &index);
        if (ret != HcclSim::HcclVmResult::HCCL_SIM_SUCCESS) {
            HCCL_VM_ERROR("InsertTaskToCollection fail");
            return ACL_ERROR_INTERNAL_ERROR;
        }

        // 下发cid
        HcclTaskCid taskCid{0, curRank, index};
        sim::Task task{};
        task.stream_id  = taskMetaData.streamId;
        task.cid        = taskCid.value;
        task.type       = (uint8_t)HccLTaskMetaType::MEM_CPY;

        auto taskId = RunnerDB::Add<sim::Task>(task);
        return ACL_SUCCESS;
    } else {
        HCCL_VM_INFO("src:{:p} to dst:{:p}, size:{:d} type: {:d} not support", src, dst, count, (int)kind);
        return ACL_ERROR_RT_FEATURE_NOT_SUPPORT;
    }

    sim::PhyMemBlock phyMem{};
    uint32_t offset = 0;
    if (!GetPtrNameByVirPtr(devPtr, offset, phyMem)) {
        HCCL_VM_INFO("[MEM] dev mem memcpy ptr:{:p}, count: {:d}", devPtr, count);
        return ACL_ERROR_INTERNAL_ERROR;
    }

    HCCL_VM_INFO("host:{:p} dev:{:p} memName:{} size:{:d} type:{:d}", hostPtr, devPtr, phyMem.name, count, (int)kind);

    char* hostDevPtr = (char*)sim::DeviceMemoryManager::GetInstance().AcquirePhyMem(phyMem.name, phyMem.device_id, phyMem.size);
    if (hostDevPtr == nullptr) {
        HCCL_VM_INFO("[MEM] dev mem memcpy ptr:{:p} name:{} can not get host ptr", devPtr, phyMem.name);
        return ACL_ERROR_INTERNAL_ERROR;
    }

    if (kind == ACL_MEMCPY_HOST_TO_DEVICE) {
        memcpy(hostDevPtr + offset, hostPtr, count);
    } else if (kind == ACL_MEMCPY_DEVICE_TO_HOST) {
        memcpy(hostPtr, hostDevPtr + offset, count);
    }

    sim::DeviceMemoryManager::GetInstance().ReleasePhyMem(phyMem.name, phyMem.device_id);

    return ACL_SUCCESS;
}

aclError aclrtMemcpyAsyncWithCondition(void *dst, size_t destMax, const void *src, size_t count, aclrtMemcpyKind kind, aclrtStream stream)
{
    return aclrtMemcpyAsync(dst, destMax, src, count, kind, stream);
}

aclError aclrtMemcpyBatch(void **dsts, size_t *destMaxs, void **srcs, size_t *sizes, size_t numBatches, aclrtMemcpyBatchAttr *attrs, size_t *attrsIndexes, size_t numAttrs, size_t *failIndex)
{
    (void) attrs;
    (void) attrsIndexes;
    (void) numAttrs;
    (void) failIndex;
    for (size_t i = 0; i < numBatches; i++) {
        aclrtMemcpy(dsts[i], destMaxs[i], srcs[i], sizes[i], ACL_MEMCPY_DEFAULT);
    }

    return ACL_SUCCESS;
}

aclError aclrtMemcpyBatchAsync(void **dsts, size_t *destMaxs, void **srcs, size_t *sizes, size_t numBatches, aclrtMemcpyBatchAttr *attrs, size_t *attrsIndexes, size_t numAttrs, size_t *failIndex, aclrtStream stream)
{
    (void) attrs;
    (void) attrsIndexes;
    (void) numAttrs;
    (void) failIndex;
    for (size_t i = 0; i < numBatches; i++) {
        aclrtMemcpyAsync(dsts[i], destMaxs[i], srcs[i], sizes[i], ACL_MEMCPY_DEFAULT, stream);
    }

    return ACL_SUCCESS;
}

aclError aclrtMemcpy2d(void *dst, size_t dpitch, const void *src, size_t spitch, size_t width, size_t height, aclrtMemcpyKind kind)
{
    (void) dst;
    (void) dpitch;
    (void) src;
    (void) spitch;
    (void) width;
    (void) height;
    if (kind != ACL_MEMCPY_HOST_TO_DEVICE && kind != ACL_MEMCPY_DEVICE_TO_HOST) {
        return ACL_ERROR_RT_FEATURE_NOT_SUPPORT;
    }

    return ACL_SUCCESS;
}

aclError aclrtMemcpy2dAsync(void *dst, size_t dpitch, const void *src, size_t spitch, size_t width, size_t height, aclrtMemcpyKind kind, aclrtStream stream)
{
    (void) dst;
    (void) dpitch;
    (void) src;
    (void) spitch;
    (void) width;
    (void) height;
    (void) kind;
    (void) stream;
    return ACL_SUCCESS;
}

aclError aclrtGetMemcpyDescSize(aclrtMemcpyKind kind, size_t *descSize)
{
    (void) kind;
    (void) descSize;
    return ACL_SUCCESS;
}

aclError aclrtSetMemcpyDesc(void *desc, aclrtMemcpyKind kind, void *srcAddr, void *dstAddr, size_t count, void *config)
{
    (void) desc;
    (void) kind;
    (void) srcAddr;
    (void) dstAddr;
    (void) count;
    (void) config;
    return ACL_SUCCESS;
}

aclError aclrtMemcpyAsyncWithDesc(void *desc, aclrtMemcpyKind kind, aclrtStream stream)
{
    (void) desc;
    (void) kind;
    (void) stream;
    return ACL_SUCCESS;
}

aclError aclrtMallocPhysical(aclrtDrvMemHandle *handle, size_t size, const aclrtPhysicalMemProp *prop, uint64_t flags)
{
    (void) prop;
    (void) flags;
    sim::Runner runner;
    if (!sim::GetCurrRunnerTls(0, runner)) {
        return ACL_ERROR_INVALID_PARAM;
    }
    auto currCtx = RunnerDB::GetById<sim::Context>(runner.current_ctx_id);
    if (!currCtx.has_value()) {
        HCCL_VM_ERROR("can not find current context:{:d}", runner.current_ctx_id);
        return ACL_ERROR_INTERNAL_ERROR;
    }

    auto deviceId = currCtx->device_id;

    std::string memName = GenDevMemName(deviceId);
    auto hostPtr = sim::DeviceMemoryManager::GetInstance().AllocPhyMem(memName.c_str(), deviceId, size);
    if (hostPtr == nullptr) {
        HCCL_VM_ERROR("can not alloc phy mem deviceId:{:d}, size:{:d}", deviceId, size);
        return ACL_ERROR_INTERNAL_ERROR;
    }

    // 记录物理内存信息到数据库
    sim::PhyMemBlock phyMem{};
    phyMem.device_id = deviceId;
    memcpy(phyMem.name, memName.c_str(), memName.size());
    phyMem.size = size;
    phyMem.ref_count = 1;
    auto phyMemId = RunnerDB::Add<sim::PhyMemBlock>(phyMem);

    HCCL_VM_INFO("malloc phy mem name:{}, size:{:d}, id:{:d}", memName, size, phyMemId);
    *handle = (aclrtDrvMemHandle)phyMemId;
    return ACL_SUCCESS;
}

aclError aclrtFreePhysical(aclrtDrvMemHandle handle)
{
    auto phyMemId = (uint64_t)(uintptr_t)handle;
    auto phyMemRes = RunnerDB::GetById<sim::PhyMemBlock>(phyMemId);
    if (!phyMemRes.has_value()) {
        HCCL_VM_ERROR("can not find phy Mem id:{:d}", phyMemId);
        return ACL_ERROR_INTERNAL_ERROR;
    };

    // 删除内存
    sim::DeviceMemoryManager::GetInstance().FreePhyMem(phyMemRes->name, phyMemRes->device_id);
    // 删除数据库记录
    RunnerDB::Delete<sim::PhyMemBlock>(phyMemId);
    HCCL_VM_INFO("free phy mem, id:{:d}", phyMemId);
    
    return ACL_SUCCESS;
}

aclError aclrtReserveMemAddress(void **virPtr, size_t size, size_t alignment, void *expectPtr, uint64_t flags)
{
    (void) alignment;
    (void) expectPtr;
    (void) flags;
    sim::Runner runner;
    if (!sim::GetCurrRunnerTls(0, runner)) {
        return ACL_ERROR_INVALID_PARAM;
    }
    auto currCtx = RunnerDB::GetById<sim::Context>(runner.current_ctx_id);
    if (!currCtx.has_value()) {
        HCCL_VM_ERROR("can not find current context:{:d}", runner.current_ctx_id);
        return ACL_ERROR_INTERNAL_ERROR;
    }

    auto deviceId = currCtx->device_id;

    auto dev = RunnerDB::GetById<sim::Device>(deviceId);
    if (!dev.has_value()) {
        HCCL_VM_ERROR("can not find device id:{:d}", deviceId);
        return ACL_ERROR_INTERNAL_ERROR;
    }

    auto devPhyId = dev->physical_id;

    std::string memName = GenDevMemName(deviceId);
    auto devPtr = sim::DeviceMemoryManager::GetInstance().AllocVirMem( devPhyId, size);
    if (devPtr == nullptr) {
        HCCL_VM_ERROR("can not alloc vir mem deviceId:{:d}, phyMemId:{:d}, size:{:d}", deviceId, devPhyId, size);
        return ACL_ERROR_INTERNAL_ERROR;
    }

    // 记录虚拟内存信息到数据库
    sim::VirtualMemBlock virMem{};
    virMem.start_ptr = (uint64_t)(uintptr_t)devPtr;
    virMem.size = size;
    virMem.ctx_id = runner.current_ctx_id;
    virMem.phy_mem_id = 0;
    virMem.owner_pid = runner.pid;
    virMem.src_type = (uint8_t)sim::VIR_MEM_TYPE_DEV;
    virMem.policy = 0;
    RunnerDB::Add<sim::VirtualMemBlock>(virMem);
    HCCL_VM_INFO("reserve vir mem, size:{:d}, devPtr:{:p}", size, devPtr);
    *virPtr = devPtr;
    return ACL_SUCCESS;
}

aclError aclrtReleaseMemAddress(void *virPtr)
{
    uint64_t devPtr  = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(virPtr));
    auto virMemRes = RunnerDB::GetOneByPred<sim::VirtualMemBlock>(
        [devPtr](const sim::VirtualMemBlock &virMem)
        { return virMem.start_ptr == devPtr && virMem.src_type == (uint8_t)sim::VIR_MEM_TYPE_DEV; });
    if (!virMemRes.second) {
        HCCL_VM_ERROR("can not find this buff offset ptr:{:p}", devPtr);
        return ACL_ERROR_INTERNAL_ERROR;
    }
    auto ctxId = virMemRes.first.ctx_id;
    auto currCtx = RunnerDB::GetById<sim::Context>(ctxId);
    if (!currCtx.has_value()) {
        HCCL_VM_ERROR("can not find current context:{:d}", ctxId);
        return ACL_ERROR_INTERNAL_ERROR;
    }

    auto deviceId = currCtx->device_id;

    auto dev = RunnerDB::GetById<sim::Device>(deviceId);
    if (!dev.has_value()) {
        HCCL_VM_ERROR("can not find device id:{:d}", deviceId);
        return ACL_ERROR_INTERNAL_ERROR;
    }

    auto devPhyId = dev->physical_id;

    sim::DeviceMemoryManager::GetInstance().FreeVirMem(devPhyId, virPtr);
    RunnerDB::Delete<sim::VirtualMemBlock>(virMemRes.first.id);
    HCCL_VM_INFO("release vir mem, devPtr:{:p}", virPtr);
    
    return ACL_SUCCESS;
}

aclError aclrtMapMem(void *virPtr, size_t size, size_t offset, aclrtDrvMemHandle handle, uint64_t flags)
{
    (void) size;
    (void) offset;
    (void) flags;
    uint64_t phyMemId = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(handle));
    uint64_t devPtr  = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(virPtr));
    auto virMemRes = RunnerDB::GetOneByPred<sim::VirtualMemBlock>(
        [devPtr](const sim::VirtualMemBlock &virMem)
        { return virMem.start_ptr == devPtr && virMem.src_type == (uint8_t)sim::VIR_MEM_TYPE_DEV; });
    if (!virMemRes.second) {
        HCCL_VM_ERROR("can not find this buff offset ptr:{:p}", devPtr);
        return ACL_ERROR_INTERNAL_ERROR;
    }

    auto phyMemRes = RunnerDB::GetById<sim::PhyMemBlock>(phyMemId);
    if (!phyMemRes.has_value()) {
        HCCL_VM_ERROR("can not find phy Mem id:{:d}", phyMemId);
        return ACL_ERROR_INTERNAL_ERROR;
    };

    // 更新虚拟内存记录 
    RunnerDB::Update<sim::VirtualMemBlock>(virMemRes.first.id, [phyMemId](sim::VirtualMemBlock &virMem) {
        virMem.phy_mem_id = phyMemId;
    });

    return ACL_SUCCESS;
}

aclError aclrtUnmapMem(void *virPtr)
{
    uint64_t devPtr  = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(virPtr));
    auto virMemRes = RunnerDB::GetOneByPred<sim::VirtualMemBlock>(
        [devPtr](const sim::VirtualMemBlock &virMem)
        { return virMem.start_ptr == devPtr && virMem.src_type == (uint8_t)sim::VIR_MEM_TYPE_DEV; });
    if (!virMemRes.second) {
        HCCL_VM_ERROR("can not find this buff offset ptr:{:p}", devPtr);
        return ACL_ERROR_INTERNAL_ERROR;
    }

    auto phyMemId = virMemRes.first.phy_mem_id;
    auto phyMemRes = RunnerDB::GetById<sim::PhyMemBlock>(phyMemId);
    if (!phyMemRes.has_value()) {
        HCCL_VM_ERROR("can not find phy Mem id:{:d}", phyMemId);
        return ACL_ERROR_INTERNAL_ERROR;
    };

    // 更新虚拟内存记录 
    RunnerDB::Update<sim::VirtualMemBlock>(virMemRes.first.id, [phyMemId](sim::VirtualMemBlock &virMem) {
        virMem.phy_mem_id = 0;
    });
    return ACL_SUCCESS;
}

aclError aclrtMemExportToShareableHandle(aclrtDrvMemHandle handle, aclrtMemHandleType handleType, uint64_t flags, uint64_t *shareableHandle)
{
    (void) handleType;
    (void) flags;
    uint64_t phyMemId = (uint64_t)(uintptr_t)handle;

    auto phyMemRes = RunnerDB::GetById<sim::PhyMemBlock>(phyMemId);
    if (!phyMemRes.has_value()) {
        HCCL_VM_ERROR("[aclrtMemExportToShareableHandle] cannot find phy Mem offset: {:d}", phyMemId);
        return ACL_ERROR_INVALID_PARAM;
    }

    *shareableHandle = phyMemRes->id;
    return ACL_SUCCESS;
}

aclError aclrtDeviceGetBareTgid(int32_t *pid)
{
    sim::Runner runner;
    if (!sim::GetCurrRunnerTls(0, runner)) {
        return ACL_ERROR_INVALID_PARAM;
    }
    *pid = runner.pid;
    return ACL_SUCCESS;
}

aclError aclrtMemSetPidToShareableHandle(uint64_t shareableHandle, int32_t *pid, size_t pidNum)
{
    sim::Runner runner;
    if (!sim::GetCurrRunnerTls(0, runner)) {
        return ACL_ERROR_INVALID_PARAM;
    }
    for (size_t i = 0; i < pidNum; i++) {
        sim::FdMemWhiteList tmp{};
        tmp.name_or_key = shareableHandle;
        tmp.pid = pid[i];
        tmp.create_pid = runner.pid;
        RunnerDB::Add<sim::FdMemWhiteList>(tmp);
    }

    return ACL_SUCCESS;
}

aclError aclrtMemImportFromShareableHandle(uint64_t shareableHandle, int32_t deviceId, aclrtDrvMemHandle *handle)
{
    (void) deviceId;
    uint64_t phyMemId = shareableHandle;

    auto phyMemRes = RunnerDB::GetById<sim::PhyMemBlock>(phyMemId);
    if (!phyMemRes.has_value()) {
        HCCL_VM_ERROR("[aclrtMemImportFromShareableHandle] cannot find phy Mem offset: {:d}", phyMemId);
        return ACL_ERROR_INVALID_PARAM;
    }

    *handle = (aclrtDrvMemHandle)phyMemId;
    return ACL_SUCCESS;
}

aclError aclrtMemGetAllocationGranularity(aclrtPhysicalMemProp *prop, aclrtMemGranularityOptions option, size_t *granularity)
{
    (void) prop;
    (void) option;
    // 1字节对齐
    *granularity = 1;
    return ACL_SUCCESS;
}

aclError aclrtCmoAsync(void *src, size_t size, aclrtCmoType cmoType, aclrtStream stream)
{
    (void) src;
    (void) size;
    (void) cmoType;
    (void) stream;
    return ACL_SUCCESS;
}

aclError aclrtCmoAsyncWithBarrier(void *src, size_t size, aclrtCmoType cmoType, uint32_t barrierId, aclrtStream stream)
{
    (void) src;
    (void) size;
    (void) cmoType;
    (void) barrierId;
    (void) stream;
    return ACL_SUCCESS;
}

aclError aclrtCmoWaitBarrier(aclrtBarrierTaskInfo *taskInfo, aclrtStream stream, uint32_t flag)
{
    (void) taskInfo;
    (void) stream;
    (void) flag;
    return ACL_SUCCESS;
}

aclError aclrtPointerGetAttributes(const void *ptr, aclrtPtrAttributes *attributes)
{
    uint64_t startPtr = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ptr));
    auto virMemRes = RunnerDB::GetOneByPred<sim::VirtualMemBlock>(
        [startPtr](const sim::VirtualMemBlock &virMem) { return virMem.start_ptr ==  startPtr && virMem.src_type == (uint8_t)sim::VIR_MEM_TYPE_DEV;});
    if (!virMemRes.second) {
        HCCL_VM_ERROR("[aclrtPointerGetAttributes] can not find this buff offset ptr: 0x{:x}", startPtr);
        return ACL_ERROR_INVALID_PARAM;
    }

    auto phyMemId = virMemRes.first.phy_mem_id;
    auto phyMemRes = RunnerDB::GetById<sim::PhyMemBlock>(phyMemId);
    if (!phyMemRes.has_value()) {
        HCCL_VM_ERROR("[aclrtPointerGetAttributes] cannot find phy Mem offset: {:d}", phyMemId);
        return ACL_ERROR_INVALID_PARAM;
    }

    auto devRes = RunnerDB::GetById<sim::Device>(phyMemRes->device_id);
    if (!devRes.has_value()) {
        HCCL_VM_ERROR("[aclrtPointerGetAttributes] cannot find phy Mem device: {:d}", phyMemRes->device_id);
        return ACL_ERROR_INVALID_PARAM;
    }

    attributes->location.id = devRes->logic_id;
    attributes->location.type = (aclrtMemLocationType)(virMemRes.first.src_type);
    return ACL_SUCCESS;
}

aclError aclrtHostRegister(void *ptr, uint64_t size, aclrtHostRegisterType type, void **devPtr)
{
    (void) size;
    (void) type;
    uint64_t startPtr = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ptr));
    auto virMemRes = RunnerDB::GetOneByPred<sim::VirtualMemBlock>(
        [startPtr](const sim::VirtualMemBlock &virMem) { return virMem.start_ptr ==  startPtr && virMem.src_type == (uint8_t)sim::VIR_MEM_TYPE_HOST;});
    if (!virMemRes.second) {
        HCCL_VM_ERROR("[aclrtHostRegister] can not find this buff offset ptr: 0x{:x}", startPtr);
        return ACL_ERROR_INVALID_PARAM;
    }
    *devPtr = ptr;
    return ACL_SUCCESS;
}

aclError aclrtHostUnregister(void *ptr)
{
    uint64_t startPtr = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ptr));
    auto virMemRes = RunnerDB::GetOneByPred<sim::VirtualMemBlock>(
        [startPtr](const sim::VirtualMemBlock &virMem) { return virMem.start_ptr ==  startPtr && virMem.src_type == (uint8_t)sim::VIR_MEM_TYPE_HOST;});
    if (!virMemRes.second) {
        HCCL_VM_ERROR("[aclrtHostUnregister] can not find this buff offset ptr: 0x{:x}", startPtr);
        return ACL_ERROR_INVALID_PARAM;
    }
    return ACL_SUCCESS;
}

aclError aclrtValueWrite(void* devAddr, uint64_t value, uint32_t flag, aclrtStream stream)
{
    (void) devAddr;
    (void) value;
    (void) flag;
    (void) stream;
    return ACL_SUCCESS;
}

aclError aclrtValueWait(void* devAddr, uint64_t value, uint32_t flag, aclrtStream stream)
{
    (void) devAddr;
    (void) value;
    (void) flag;
    (void) stream;
    return ACL_SUCCESS;
}

aclError aclrtIpcMemGetExportKey(void *devPtr, size_t size, char *key, size_t len, uint64_t flags)
{
    (void) size;
    (void) len;
    (void) flags;
    uint64_t startPtr = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(devPtr));
    auto virMemRes = RunnerDB::GetOneByPred<sim::VirtualMemBlock>(
        [startPtr](const sim::VirtualMemBlock &virMem) { return virMem.start_ptr ==  startPtr;});
    if (!virMemRes.second) {
        HCCL_VM_ERROR("[aclrtIpcMemGetExportKey] can not find this buff offset ptr: 0x{:x}", startPtr);
        return ACL_ERROR_INVALID_PARAM;
    }
    auto phyMemId = virMemRes.first.phy_mem_id;
    auto phyMemRes = RunnerDB::GetById<sim::PhyMemBlock>(phyMemId);
    if (!phyMemRes.has_value()) {
        HCCL_VM_ERROR("[aclrtIpcMemGetExportKey] cannot find phy Mem offset: {:d}", phyMemId);
        return ACL_ERROR_INVALID_PARAM;
    }

    sim::Runner runner;
    if (!sim::GetCurrRunnerTls(0, runner)) {
        return ACL_ERROR_INVALID_PARAM;
    }
    sim::IpcMemRecord memRecord{};
    memRecord.vir_mem_id = virMemRes.first.id;
    memRecord.create_pid = runner.pid;
    auto recordIdx = RunnerDB::Add<sim::IpcMemRecord>(memRecord);

    *(uint64_t *)key = recordIdx;

    return ACL_SUCCESS;
}

aclError aclrtIpcMemSetImportPid(const char *key, int32_t *pid, size_t num)
{
    sim::Runner runner;
    if (!sim::GetCurrRunnerTls(0, runner)) {
        return ACL_ERROR_INVALID_PARAM;
    }
    for (int32_t i = 0; i < num; i++) {
        sim::IpcMemWhiteList tmp{};
        tmp.name_or_key = *(uint64_t*)key;
        tmp.pid = pid[i];
        tmp.create_pid = runner.pid;
        RunnerDB::Add<sim::IpcMemWhiteList>(tmp);
    }
    return ACL_SUCCESS;
}

aclError aclrtIpcMemImportByKey(void **devPtr, const char *key, uint64_t flags)
{
    (void) flags;
    uint64_t ipcRecordIdx = *(const uint64_t *)key;
    auto recordRes = RunnerDB::GetById<sim::IpcMemRecord>(ipcRecordIdx);
    if (!recordRes.has_value()) {
        HCCL_VM_ERROR("[aclrtIpcMemImportByKey] cannot find ipc record: {:d}", ipcRecordIdx);
        return ACL_ERROR_INVALID_PARAM;
    }

    auto virMemRes = RunnerDB::GetById<sim::VirtualMemBlock>(recordRes->vir_mem_id);
    if (!virMemRes.has_value()) {
        HCCL_VM_ERROR("[aclrtIpcMemImportByKey] cannot find vir mem: {:d}", recordRes->vir_mem_id);
        return ACL_ERROR_INVALID_PARAM;
    }
    *devPtr = (void *)virMemRes->start_ptr;

    return ACL_SUCCESS;
}

aclError aclrtIpcMemClose(const char *key)
{
    uint64_t ipcRecordIdx = *reinterpret_cast<const uint64_t*>(key);
    auto recordRes = RunnerDB::GetById<sim::IpcMemRecord>(ipcRecordIdx);
    if (!recordRes.has_value()) {
        HCCL_VM_ERROR("[aclrtIpcMemClose] cannot find ipc record: {:d}", ipcRecordIdx);
        return ACL_ERROR_INVALID_PARAM;
    }

    sim::Runner runner;
    if (!sim::GetCurrRunnerTls(0, runner)) {
        return ACL_ERROR_INVALID_PARAM;
    }
    if (runner.pid == recordRes->create_pid) {
        RunnerDB::Delete<sim::IpcMemRecord>(recordRes->id);
    }

    return ACL_SUCCESS;
}

aclError aclrtGetMemInfo(aclrtMemAttr attr, size_t *free, size_t *total)
{
    (void) attr;
    (void) free;
    (void) total;
    return ACL_SUCCESS;
}

aclError aclrtAllocatorRegister(aclrtStream stream, aclrtAllocatorDesc allocatorDesc)
{
    (void) stream;
    (void) allocatorDesc;
    return ACL_SUCCESS;
}

aclError aclrtAllocatorGetByStream(aclrtStream stream, aclrtAllocatorDesc *allocatorDesc, aclrtAllocator *allocator, aclrtAllocatorAllocFunc *allocFunc, aclrtAllocatorFreeFunc *freeFunc, aclrtAllocatorAllocAdviseFunc *allocAdviseFunc, aclrtAllocatorGetAddrFromBlockFunc *getAddrFromBlockFunc)
{
    (void) stream;
    (void) allocatorDesc;
    (void) allocator;
    (void) allocFunc;
    (void) freeFunc;
    (void) allocAdviseFunc;
    (void) getAddrFromBlockFunc;
    return ACL_SUCCESS;
}

aclError aclrtAllocatorUnregister(aclrtStream stream)
{
    (void) stream;
    return ACL_SUCCESS;
}

aclError aclrtMemcpyAsyncWithOffsetImpl(void **dst, size_t destMax, uint64_t dstDataOffset, const void **src,
    size_t count, size_t srcDataOffset, aclrtMemcpyKind kind, aclrtStream stream)
{
    (void) dst;
    (void) destMax;
    (void) dstDataOffset;
    (void) src;
    (void) count;
    (void) srcDataOffset;
    (void) kind;
    (void) stream;
    return ACL_SUCCESS;
}

aclError aclrtIpcMemSetAttr(const char *key, aclrtIpcMemAttrType type, uint64_t attr)
{
    (void) key;
    (void) type;
    (void) attr;
    return ACL_SUCCESS;
}

aclError aclrtIpcMemImportPidInterServer(const char *name, aclrtServerPid *serverPids, size_t num)
{
    const aclrtServerPid &rtServerPid = *serverPids;
    return aclrtIpcMemSetImportPid(name, rtServerPid.pid, rtServerPid.num);
}

rtError_t rtMalloc(void **devPtr, uint64_t size, rtMemType_t type, const uint16_t moduleId)
{
    (void) type;
    (void) moduleId;
    int ret = aclrtMalloc(devPtr, size, aclrtMemMallocPolicy::ACL_MEM_MALLOC_HUGE_FIRST);
    HCCL_VM_INFO("enter into stub rtMalloc.... addr:{:p}", *devPtr);
    return ret;
}

aclError rtFree(void* devPtr)
{
    (void) devPtr;
    HCCL_VM_ERROR("[aclstub] enter into rtFree not support");
    return ACL_ERROR_INTERNAL_ERROR;
}

std::map<uint8_t, HcclDataType> rtDataType2CheckerDataType = {
    { aclDataType::ACL_FLOAT, HcclDataType::HCCL_DATA_TYPE_FP32 },
    { aclDataType::ACL_FLOAT16, HcclDataType::HCCL_DATA_TYPE_FP16 },
    { aclDataType::ACL_INT8,  HcclDataType::HCCL_DATA_TYPE_INT8 },
    { aclDataType::ACL_INT32, HcclDataType::HCCL_DATA_TYPE_INT32 },
    { aclDataType::ACL_UINT8, HcclDataType::HCCL_DATA_TYPE_UINT8 },
    { aclDataType::ACL_INT16, HcclDataType::HCCL_DATA_TYPE_INT16 },
    { aclDataType::ACL_UINT16, HcclDataType::HCCL_DATA_TYPE_UINT16 },
    { aclDataType::ACL_UINT32, HcclDataType::HCCL_DATA_TYPE_UINT32 },
    { aclDataType::ACL_INT64,  HcclDataType::HCCL_DATA_TYPE_INT64 },
    { aclDataType::ACL_BF16,  HcclDataType::HCCL_DATA_TYPE_BFP16 }
};

aclError aclrtReduceAsync(void *dst, const void *src, uint64_t count, aclrtReduceKind kind, aclDataType type, aclrtStream stream, void *reserve)
{
    (void) reserve;
    uint64_t srcOffset = 0;
    uint64_t dstOffset = 0;
    uint64_t srcCtxId = 0;
    uint64_t dstCtxId = 0;

    auto iter = rtDataType2CheckerDataType.find(type);
    if (iter == rtDataType2CheckerDataType.end()) {
        HCCL_VM_ERROR("[ERROR][aclrtReduceAsync] not support data type: {}", static_cast<uint32_t>(type));
        return ACL_ERROR_RT_FEATURE_NOT_SUPPORT;
    }

    auto dataType = iter->second;
    uint32_t dataSize = 0;
    if (sim::GetDataTypeSize(dataType, dataSize) != HcclResult::HCCL_SUCCESS) {
        return HcclResult::HCCL_E_NOT_SUPPORT;
    }

    uint64_t streamId = sim::GetCurrentStreamId((uint64_t)(uintptr_t)stream);

    uint32_t curRank = (uint32_t)sim::GetCurrRankId();

    HcclTaskMetaData taskMetaData;
    taskMetaData.taskType = HccLTaskMetaType::REDUCE;
    taskMetaData.commId   = 0;
    taskMetaData.rankId   = curRank;
    taskMetaData.streamId = streamId;
    taskMetaData.taskData.reduce.srcRankId = sim::GetRankIdByCtxId(srcCtxId);
    taskMetaData.taskData.reduce.dstRankId = sim::GetRankIdByCtxId(dstCtxId);
    taskMetaData.taskData.reduce.srcOffset = (uint64_t)(uintptr_t)src;
    taskMetaData.taskData.reduce.dstOffset = (uint64_t)(uintptr_t)dst;
    taskMetaData.taskData.reduce.dataType  = static_cast<uint8_t>(dataType);
    taskMetaData.taskData.reduce.dataCount = count / dataSize;
    taskMetaData.taskData.reduce.reduceOp  = static_cast<uint8_t>(kind);

    uint32_t index{0};
    HCCL_VM_DEBUG("[aclstub][aclrtReduceAsync] Get reduce task, streamId={:d}", streamId);
    auto ret = InsertTaskToCollection(&taskMetaData, &index);
    if (ret != HcclSim::HcclVmResult::HCCL_SIM_SUCCESS) {
        HCCL_VM_ERROR("[{}] InsertTaskToCollection fail", __func__);
        return ACL_ERROR_INTERNAL_ERROR;
    }

    return ACL_SUCCESS;
}

#ifdef __cplusplus
}
#endif  // __cplusplus

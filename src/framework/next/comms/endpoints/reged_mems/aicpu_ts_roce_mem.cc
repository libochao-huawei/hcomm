/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "aicpu_ts_roce_mem.h"
#include <algorithm>
#include <mutex>
#include <unordered_map>
#include "securec.h"
#include "adapter_hccp.h"
#include "endpoint_pair.h"
#include "exception_handler.h"
#include "hccl_network.h"
#include "hccl_net_dev_defs.h"
#include "log.h"
#include "rma_buffer.h"

namespace {
using LocalRdmaRmaBufferMgr = hccl::NetDevContext::LocalRdmaRmaBufferMgr;
struct LocalBufferMgrCtx {
    std::shared_ptr<std::mutex> mu;
    std::shared_ptr<LocalRdmaRmaBufferMgr> mgr;
};

std::mutex g_phyLocalRdmaBundleMapMu;
std::unordered_map<s32, std::shared_ptr<LocalBufferMgrCtx>> g_phyIdToLocalBufferMgrCtx;

std::shared_ptr<LocalBufferMgrCtx> GetOrCreateLocalBufferMgr(s32 devicePhyId)
{
    std::lock_guard<std::mutex> mapLock(g_phyLocalRdmaBundleMapMu);
    std::shared_ptr<LocalBufferMgrCtx> &slot = g_phyIdToLocalBufferMgrCtx[devicePhyId];
    if (slot == nullptr) {
        std::shared_ptr<LocalRdmaRmaBufferMgr> mgr;
        EXECEPTION_CATCH((mgr = std::make_shared<LocalRdmaRmaBufferMgr>()), return nullptr);
        slot = std::make_shared<LocalBufferMgrCtx>();
        slot->mu = std::make_shared<std::mutex>();
        slot->mgr = std::move(mgr);
    }
    return slot;
}
}  // namespace

namespace hcomm {
AicpuTsRoceRegedMemMgr::AicpuTsRoceRegedMemMgr(HcclNetDev netDev, RdmaHandle rdmaHandle)
    : netDev_(netDev)
{
    rdmaHandle_ = rdmaHandle;
    if (netDev_ != nullptr) {
        auto *netDevCtx = static_cast<hccl::NetDevContext *>(netDev_);
        const s32 phyId = netDevCtx->GetPhyId();
        std::shared_ptr<LocalBufferMgrCtx> ctx = GetOrCreateLocalBufferMgr(phyId);
        if (ctx != nullptr) {
            localRdmaRmaBufferMgr_ = ctx->mgr;
        }
        HCCL_INFO(
            "[AicpuTsRoceRegedMemMgr] ctor netDev[%p] phyId[%d] process-local localRdmaRmaBufferMgr[%p] (not NetDev embed)",
            static_cast<void *>(netDev_), static_cast<int>(phyId), static_cast<void *>(localRdmaRmaBufferMgr_.get()));
    } else {
        HCCL_INFO("[AicpuTsRoceRegedMemMgr] ctor netDev is null, local mgr unset");
    }
}

void AicpuTsRoceRegedMemMgr::TrackRegisteredBuffer(const std::shared_ptr<hccl::LocalRdmaRmaBuffer> &localBuffer)
{
    void *const handlePtr = static_cast<void *>(localBuffer.get());
    const bool alreadyListed = std::any_of(allRegisteredBuffers_.begin(), allRegisteredBuffers_.end(),
        [handlePtr](const std::shared_ptr<hccl::LocalRdmaRmaBuffer> &p) { return static_cast<void *>(p.get()) == handlePtr; });
    if (alreadyListed) {
        return;
    }
    allRegisteredBuffers_.push_back(localBuffer);
    HcclBuf rec{};
    rec.addr = localBuffer->GetAddr();
    rec.len = localBuffer->GetSize();
    auto *rma = dynamic_cast<hccl::RmaBuffer *>(localBuffer.get());
    if (rma == nullptr) {
        HCCL_ERROR("[AicpuTsRoceRegedMemMgr][TrackRegisteredBuffer] rma is nullptr");
        return;
    }
    rec.handle = static_cast<void *>(rma);
    hcclBufRecords_.push_back(rec);
}

HcclResult AicpuTsRoceRegedMemMgr::GetOrCreateLocalRdmaRmaBuffer(hccl::NetDevContext *netDevCtx, HcommMem mem,
    hccl::BufferKey<uintptr_t, u64> tempKey, std::shared_ptr<hccl::LocalRdmaRmaBuffer> &localRdmaRmaBuffer)
{
    hccl::RmaMemType memType = static_cast<hccl::RmaMemType>(mem.type);
    auto findPair = localRdmaRmaBufferMgr_->Find(tempKey);
    if (findPair.first) {
        localRdmaRmaBuffer = findPair.second;
        HCCL_INFO("[AicpuTsRoceRegedMemMgr][RegisterMemory] Find hit, reuse buffer mgr entry key {%p, %llu}",
            mem.addr, mem.size);
        return HCCL_SUCCESS;
    }
    HCCL_INFO("[AicpuTsRoceRegedMemMgr][RegisterMemory] Find miss, construct LocalRdmaRmaBuffer key {%p, %llu} type[%u]",
        mem.addr, mem.size, static_cast<unsigned int>(mem.type));
    EXECEPTION_CATCH((localRdmaRmaBuffer = std::make_shared<hccl::LocalRdmaRmaBuffer>(netDevCtx, mem.addr,
        static_cast<u64>(mem.size), memType)), return HCCL_E_PTR);
    return HCCL_SUCCESS;
}

HcclResult AicpuTsRoceRegedMemMgr::RegisterMemory(HcommMem mem, const char *memTag, void **memHandle)
{
    (void)memTag;
    HCCL_INFO("[%s] Begin", __FUNCTION__);
    CHK_PTR_NULL(netDev_);
    CHK_PTR_NULL(memHandle);
    CHK_PTR_NULL(localRdmaRmaBufferMgr_);

    hccl::BufferKey<uintptr_t, u64> tempKey(reinterpret_cast<uintptr_t>(mem.addr), static_cast<u64>(mem.size));
    auto *netDevCtx = static_cast<hccl::NetDevContext *>(netDev_);

    std::shared_ptr<LocalBufferMgrCtx> ctx = GetOrCreateLocalBufferMgr(netDevCtx->GetPhyId());
    CHK_PTR_NULL(ctx);
    std::lock_guard<std::mutex> phyLocalLock(*ctx->mu);

    std::shared_ptr<hccl::LocalRdmaRmaBuffer> localRdmaRmaBuffer;

    // 检查是否已注册（精确匹配或子集）
    auto findPair = localRdmaRmaBufferMgr_->Find(tempKey);
    if (findPair.first) {
        localRdmaRmaBuffer = findPair.second;
        CHK_SMART_PTR_NULL(localRdmaRmaBuffer);

        auto parentAddr = reinterpret_cast<uintptr_t>(localRdmaRmaBuffer->GetAddr());
        auto parentSize = localRdmaRmaBuffer->GetSize();
        hccl::BufferKey<uintptr_t, u64> parentKey(parentAddr, parentSize);

        if (tempKey == parentKey) {
            // CASE 1: 精确匹配，增加引用计数
            HCCL_INFO("[AicpuTsRoceRegedMemMgr][RegisterMemory]Memory {addr[%p], size[%llu]} already registered, "
                "increase ref count.", mem.addr, mem.size);
            localRdmaRmaBufferMgr_->Add(tempKey, localRdmaRmaBuffer);
            TrackRegisteredBuffer(localRdmaRmaBuffer);
            *memHandle = static_cast<void *>(localRdmaRmaBuffer.get());
            return HCCL_SUCCESS;
        }

        // CASE 2: 传入内存被包含于更大的已注册区域，构造虚拟注册
        HCCL_INFO("[AicpuTsRoceRegedMemMgr][RegisterMemory]Memory {addr[%p], size[%llu]} is within registered region "
            "{addr[%p], size[%llu]}, create virtual entry.",
            mem.addr, mem.size, reinterpret_cast<void *>(parentAddr), parentSize);

        localRdmaRmaBufferMgr_->AddWithoutCheck(tempKey, localRdmaRmaBuffer);
        localRdmaRmaBufferMgr_->AddWithoutCheck(parentKey, localRdmaRmaBuffer);

        virtualRegs_.push_back(
            {localRdmaRmaBuffer, reinterpret_cast<uintptr_t>(mem.addr), mem.size, {}});
        *memHandle = static_cast<void *>(&virtualRegs_.back());
        return HCCL_SUCCESS;
    }

    // CASE 3: 未注册，构造LocalRdmaRmaBuffer
    hccl::RmaMemType memType = static_cast<hccl::RmaMemType>(mem.type);
    HCCL_INFO("[AicpuTsRoceRegedMemMgr][RegisterMemory] Find miss, construct LocalRdmaRmaBuffer key {%p, %llu} type[%u]",
        mem.addr, mem.size, static_cast<unsigned int>(mem.type));
    EXECEPTION_CATCH((localRdmaRmaBuffer = std::make_shared<hccl::LocalRdmaRmaBuffer>(netDevCtx, mem.addr,
        static_cast<u64>(mem.size), memType)), return HCCL_E_PTR);

    auto resultPair = localRdmaRmaBufferMgr_->Add(tempKey, localRdmaRmaBuffer);
    if (resultPair.first == localRdmaRmaBufferMgr_->End()) {
        HCCL_ERROR("[AicpuTsRoceRegedMemMgr][RegisterMemory] memory overlaps with registered memory");
        return HCCL_E_INTERNAL;
    }

    std::shared_ptr<hccl::LocalRdmaRmaBuffer> &localBuffer = resultPair.first->second.buffer;
    CHK_SMART_PTR_NULL(localBuffer);
    *memHandle = static_cast<void *>(localBuffer.get());

    if (resultPair.second) {
        HcclResult ret = localBuffer->Init();
        if (ret != HCCL_SUCCESS) {
            (void)localRdmaRmaBufferMgr_->Del(tempKey);
            HCCL_ERROR("[AicpuTsRoceRegedMemMgr][RegisterMemory] Init failed, ret[%d]", ret);
            return ret;
        }
        HCCL_INFO("[AicpuTsRoceRegedMemMgr][RegisterMemory] success, key {%p, %llu}", mem.addr, mem.size);
    }

    TrackRegisteredBuffer(localBuffer);

    if (resultPair.second) {
        return HCCL_SUCCESS;
    }
    HCCL_INFO("[AicpuTsRoceRegedMemMgr][RegisterMemory] already registered, ref++, key {%p, %llu}", mem.addr, mem.size);
    return HCCL_SUCCESS;
}

HcclResult AicpuTsRoceRegedMemMgr::UnregisterMemory(void *memHandle)
{
    HCCL_INFO("[%s] Begin", __FUNCTION__);
    CHK_PTR_NULL(netDev_);
    CHK_PTR_NULL(memHandle);
    CHK_PTR_NULL(localRdmaRmaBufferMgr_);

    auto *netDevCtx = static_cast<hccl::NetDevContext *>(netDev_);
    std::shared_ptr<LocalBufferMgrCtx> ctx = GetOrCreateLocalBufferMgr(netDevCtx->GetPhyId());
    CHK_PTR_NULL(ctx);
    std::lock_guard<std::mutex> phyLocalLock(*ctx->mu);

    // 检查是否为虚拟注册句柄
    for (auto it = virtualRegs_.begin(); it != virtualRegs_.end(); ++it) {
        if (static_cast<void *>(&(*it)) == memHandle) {
            HCCL_INFO("[AicpuTsRoceRegedMemMgr][UnregisterMemory] Unregister virtual memory {addr[%p], size[%llu]}.",
                reinterpret_cast<void *>(it->childAddr), it->childSize);

            hccl::BufferKey<uintptr_t, u64> childKey(it->childAddr, it->childSize);
            bool childDeleted = false;
            EXECEPTION_CATCH(childDeleted = localRdmaRmaBufferMgr_->Del(childKey), return HCCL_E_NOT_FOUND);

            hccl::BufferKey<uintptr_t, u64> parentKey(
                reinterpret_cast<uintptr_t>(it->parentBuffer->GetAddr()),
                it->parentBuffer->GetSize());
            bool parentDeleted = false;
            EXECEPTION_CATCH(parentDeleted = localRdmaRmaBufferMgr_->Del(parentKey), return HCCL_E_NOT_FOUND);

            if (parentDeleted) {
                allRegisteredBuffers_.erase(std::remove_if(allRegisteredBuffers_.begin(), allRegisteredBuffers_.end(),
                    [parent = it->parentBuffer.get()](const std::shared_ptr<hccl::LocalRdmaRmaBuffer> &p) {
                        return p.get() == parent;
                    }), allRegisteredBuffers_.end());

                hcclBufRecords_.erase(std::remove_if(hcclBufRecords_.begin(), hcclBufRecords_.end(),
                    [parent = it->parentBuffer.get()](const HcclBuf &b) {
                        return b.handle == static_cast<void *>(parent);
                    }), hcclBufRecords_.end());
            }

            virtualRegs_.erase(it);
            HCCL_INFO("[AicpuTsRoceRegedMemMgr][UnregisterMemory] virtual unregister success");
            return HCCL_SUCCESS;
        }
    }

    auto *buffer = static_cast<hccl::LocalRdmaRmaBuffer *>(memHandle);
    hccl::BufferKey<uintptr_t, u64> tempKey(reinterpret_cast<uintptr_t>(buffer->GetAddr()), buffer->GetSize());

    bool delOk = false;
    EXECEPTION_CATCH(delOk = localRdmaRmaBufferMgr_->Del(tempKey), return HCCL_E_NOT_FOUND);
    if (!delOk) {
        HCCL_INFO("[AicpuTsRoceRegedMemMgr][UnregisterMemory] ref count > 0");
    }

    exportDescByBuffer_.erase(buffer);
    allRegisteredBuffers_.erase(std::remove_if(allRegisteredBuffers_.begin(), allRegisteredBuffers_.end(),
        [memHandle](const std::shared_ptr<hccl::LocalRdmaRmaBuffer> &p) { return p.get() == memHandle; }),
        allRegisteredBuffers_.end());

    hcclBufRecords_.erase(std::remove_if(hcclBufRecords_.begin(), hcclBufRecords_.end(),
        [memHandle](const HcclBuf &b) { return b.handle == memHandle; }),
        hcclBufRecords_.end());
    HCCL_INFO("[AicpuTsRoceRegedMemMgr][UnregisterMemory] success, memHandle[%p] key {%p, %llu}", memHandle,
        buffer->GetAddr(), static_cast<unsigned long long>(buffer->GetSize()));
    return HCCL_SUCCESS;
}

HcclResult AicpuTsRoceRegedMemMgr::MemoryExport(const EndpointDesc endpointDesc, void *memHandle, void **memDesc,
    uint32_t *memDescLen)
{
    HCCL_INFO("[%s] Begin", __FUNCTION__);
    CHK_PTR_NULL(memHandle);
    CHK_PTR_NULL(memDesc);
    CHK_PTR_NULL(memDescLen);

    // 检查是否为虚拟注册句柄
    for (auto &virt : virtualRegs_) {
        if (static_cast<void *>(&virt) == memHandle) {
            HCCL_INFO("[AicpuTsRoceRegedMemMgr][MemoryExport] Export virtual memory {addr[%p], size[%llu]}.",
                reinterpret_cast<void *>(virt.childAddr), virt.childSize);

            auto *parentBuf = virt.parentBuffer.get();
            CHK_PTR_NULL(parentBuf);
            std::string &ser = parentBuf->Serialize();
            if (ser.empty()) {
                HCCL_ERROR("[AicpuTsRoceRegedMemMgr][MemoryExport] parent Serialize empty");
                return HCCL_E_INTERNAL;
            }

            virt.exportDesc.clear();
            virt.exportDesc.reserve(ser.size() + sizeof(EndpointDesc));
            virt.exportDesc.insert(virt.exportDesc.end(), ser.begin(), ser.end());

            std::vector<char> ep(sizeof(EndpointDesc));
            if (memcpy_s(ep.data(), sizeof(EndpointDesc), &endpointDesc, sizeof(EndpointDesc)) != EOK) {
                HCCL_ERROR("[AicpuTsRoceRegedMemMgr][MemoryExport] endpointDesc memcpy_s failed");
                return HCCL_E_INTERNAL;
            }
            virt.exportDesc.insert(virt.exportDesc.end(), ep.begin(), ep.end());

            *memDesc = static_cast<void *>(virt.exportDesc.data());
            *memDescLen = static_cast<uint32_t>(virt.exportDesc.size());
            HCCL_INFO("[AicpuTsRoceRegedMemMgr][MemoryExport] virtual export success memHandle[%p]", memHandle);
            return HCCL_SUCCESS;
        }
    }

    auto *buf = reinterpret_cast<hccl::LocalRdmaRmaBuffer *>(memHandle);
    std::string &ser = buf->Serialize();
    if (ser.empty()) {
        HCCL_ERROR("[AicpuTsRoceRegedMemMgr][MemoryExport] Serialize empty");
        return HCCL_E_INTERNAL;
    }

    std::vector<char> &blob = exportDescByBuffer_[buf];
    blob.clear();
    blob.reserve(ser.size() + sizeof(EndpointDesc));
    blob.insert(blob.end(), ser.begin(), ser.end());

    std::vector<char> ep(sizeof(EndpointDesc));
    if (memcpy_s(ep.data(), sizeof(EndpointDesc), &endpointDesc, sizeof(EndpointDesc)) != EOK) {
        HCCL_ERROR("[AicpuTsRoceRegedMemMgr][MemoryExport] endpointDesc memcpy_s failed");
        return HCCL_E_INTERNAL;
    }
    blob.insert(blob.end(), ep.begin(), ep.end());

    *memDesc = static_cast<void *>(blob.data());
    *memDescLen = static_cast<uint32_t>(blob.size());
    HCCL_INFO("[AicpuTsRoceRegedMemMgr][MemoryExport] success memHandle[%p] memDescLen[%u] rdmaSerLen[%zu]", memHandle,
        *memDescLen, ser.size());
    return HCCL_SUCCESS;
}

HcclResult AicpuTsRoceRegedMemMgr::GetParamsFromMemDesc(const void *memDesc, uint32_t descLen,
    EndpointDesc &endpointDesc, std::string &rdmaBlob)
{
    CHK_PTR_NULL(memDesc);
    if (descLen < sizeof(EndpointDesc)) {
        HCCL_ERROR("[AicpuTsRoceRegedMemMgr][GetParamsFromMemDesc] descLen[%u] too small", descLen);
        return HCCL_E_PARA;
    }
    const auto *base = static_cast<const char *>(memDesc);
    if (memcpy_s(&endpointDesc, sizeof(EndpointDesc), base + descLen - sizeof(EndpointDesc), sizeof(EndpointDesc)) != EOK) {
        HCCL_ERROR("[AicpuTsRoceRegedMemMgr][GetParamsFromMemDesc] endpointDesc copy failed");
        return HCCL_E_INTERNAL;
    }
    rdmaBlob.assign(base, base + descLen - sizeof(EndpointDesc));
    return HCCL_SUCCESS;
}

HcclResult AicpuTsRoceRegedMemMgr::MemoryImport(const void *memDesc, uint32_t descLen, HcommMem *outMem)
{
    HCCL_INFO("[%s] Begin", __FUNCTION__);
    CHK_PTR_NULL(outMem);

    EndpointDesc endpointDesc{};
    std::string rdmaBlob;
    CHK_RET(GetParamsFromMemDesc(memDesc, descLen, endpointDesc, rdmaBlob));

    std::shared_ptr<hccl::RemoteRdmaRmaBuffer> remoteBuf;
    EXECEPTION_CATCH(remoteBuf = std::make_shared<hccl::RemoteRdmaRmaBuffer>(), return HCCL_E_PTR);
    CHK_RET(remoteBuf->Deserialize(rdmaBlob));

    if (remoteRdmaRmaBufferMgrs_.find(endpointDesc) == remoteRdmaRmaBufferMgrs_.end()) {
        std::unique_ptr<RemoteRdmaRmaBufferMgr> mgr;
        EXECEPTION_CATCH(mgr = std::make_unique<RemoteRdmaRmaBufferMgr>(), return HCCL_E_PTR);
        CHK_SMART_PTR_NULL(mgr);
        remoteRdmaRmaBufferMgrs_[endpointDesc] = std::move(mgr);
        HCCL_INFO("[AicpuTsRoceRegedMemMgr][MemoryImport] created RemoteRdmaRmaBufferMgr for new endpoint, mgrCnt[%zu]",
            remoteRdmaRmaBufferMgrs_.size());
    }

    hccl::BufferKey<uintptr_t, u64> tempKey(reinterpret_cast<uintptr_t>(remoteBuf->GetAddr()), remoteBuf->GetSize());
    auto resultPair = remoteRdmaRmaBufferMgrs_[endpointDesc]->Add(tempKey, remoteBuf);
    if (!resultPair.second) {
        HCCL_ERROR("[AicpuTsRoceRegedMemMgr][MemoryImport] memDesc already imported");
        return HCCL_E_AGAIN;
    }

    outMem->addr = remoteBuf->GetAddr();
    outMem->size = remoteBuf->GetSize();
    outMem->type = COMM_MEM_TYPE_DEVICE;
    HCCL_INFO("[AicpuTsRoceRegedMemMgr][MemoryImport] success descLen[%u] outMem addr[%p] size[%llu], mgrCnt[%zu]",
        descLen, outMem->addr, static_cast<unsigned long long>(outMem->size), remoteRdmaRmaBufferMgrs_.size());
    return HCCL_SUCCESS;
}

HcclResult AicpuTsRoceRegedMemMgr::MemoryUnimport(const void *memDesc, uint32_t descLen)
{
    HCCL_INFO("[%s] Begin", __FUNCTION__);

    EndpointDesc endpointDesc{};
    std::string rdmaBlob;
    CHK_RET(GetParamsFromMemDesc(memDesc, descLen, endpointDesc, rdmaBlob));

    auto mgrIt = remoteRdmaRmaBufferMgrs_.find(endpointDesc);
    if (mgrIt == remoteRdmaRmaBufferMgrs_.end()) {
        HCCL_ERROR("[AicpuTsRoceRegedMemMgr][MemoryUnimport] remote mgr not found");
        return HCCL_E_NOT_FOUND;
    }

    std::shared_ptr<hccl::RemoteRdmaRmaBuffer> probe;
    EXECEPTION_CATCH(probe = std::make_shared<hccl::RemoteRdmaRmaBuffer>(), return HCCL_E_PTR);
    CHK_RET(probe->Deserialize(rdmaBlob));

    hccl::BufferKey<uintptr_t, u64> tempKey(reinterpret_cast<uintptr_t>(probe->GetAddr()), probe->GetSize());
    bool delOk = false;
    EXECEPTION_CATCH(delOk = mgrIt->second->Del(tempKey), return HCCL_E_NOT_FOUND);
    if (!delOk) {
        HCCL_INFO("[AicpuTsRoceRegedMemMgr][MemoryUnimport] ref count > 0");
        return HCCL_E_AGAIN;
    }
    if (mgrIt->second->size() == 0) {
        remoteRdmaRmaBufferMgrs_.erase(mgrIt);
        HCCL_INFO("[AicpuTsRoceRegedMemMgr][MemoryUnimport] erased empty remote mgr, descLen[%u] remainingMgrCnt[%zu]",
            descLen, remoteRdmaRmaBufferMgrs_.size());
    }
    HCCL_INFO("[AicpuTsRoceRegedMemMgr][MemoryUnimport] success descLen[%u] key {%p, %llu}", descLen, probe->GetAddr(),
        static_cast<unsigned long long>(probe->GetSize()));
    return HCCL_SUCCESS;
}

HcclResult AicpuTsRoceRegedMemMgr::GetAllMemHandles(void **memHandles, uint32_t *memHandleNum)
{
    HCCL_INFO("[%s] Begin", __FUNCTION__);
    CHK_PTR_NULL(memHandleNum);

    *memHandleNum = static_cast<uint32_t>(hcclBufRecords_.size());
    if (*memHandleNum == 0U) {
        *memHandles = nullptr;
        HCCL_INFO("[AicpuTsRoceRegedMemMgr][GetAllMemHandles] no records, memHandleNum[0]");
        return HCCL_SUCCESS;
    }
    *memHandles = static_cast<void *>(hcclBufRecords_.data());
    HCCL_INFO("[AicpuTsRoceRegedMemMgr][GetAllMemHandles] memHandleNum[%u] hcclBufRecords[%p]", *memHandleNum,
        static_cast<void *>(hcclBufRecords_.data()));
    return HCCL_SUCCESS;
}

HcclResult AicpuTsRoceRegedMemMgr::GatherLocalMemDetails(std::vector<RoceMemDetails> &localOut) const
{
    localOut.reserve(allRegisteredBuffers_.size());
    for (const auto &buf : allRegisteredBuffers_) {
        if (buf == nullptr) {
            continue;
        }
        auto *rma = static_cast<hccl::RmaBuffer *>(buf.get());
        CHK_PTR_NULL(rma->GetDevAddr());
        RoceMemDetails r{};
        r.addr = static_cast<u64>(reinterpret_cast<uintptr_t>(rma->GetAddr()));
        r.devAddr = static_cast<u64>(reinterpret_cast<uintptr_t>(rma->GetDevAddr()));
        r.size = buf->GetSize();
        r.key = buf->GetKey();
        localOut.push_back(r);
        HCCL_INFO("[AicpuTsRoceRegedMemMgr][GetAllMemDetails][local][%zu] addr[0x%llx] devAddr[0x%llx] size[%llu] key[%u]",
            localOut.size() - 1U, static_cast<unsigned long long>(r.addr), static_cast<unsigned long long>(r.devAddr),
            static_cast<unsigned long long>(r.size), r.key);
    }
    return HCCL_SUCCESS;
}

HcclResult AicpuTsRoceRegedMemMgr::AppendLocalNotifyMemDetails(std::vector<RoceMemDetails> &localOut) const
{
    CHK_PTR_NULL(rdmaHandle_);
    CHK_PTR_NULL(netDev_);
    auto *netCtx = static_cast<hccl::NetDevContext *>(netDev_);
    struct MrInfoT mrInfo{};
    CHK_RET(HrtRaGetNotifyMrInfo(static_cast<u32>(netCtx->GetPhyId()), rdmaHandle_, &mrInfo));
    CHK_PTR_NULL(mrInfo.addr);
    RoceMemDetails notifyMd{};
    notifyMd.addr = static_cast<u64>(reinterpret_cast<uintptr_t>(mrInfo.addr));
    notifyMd.devAddr = notifyMd.addr;
    notifyMd.size = static_cast<u64>(mrInfo.size);
    notifyMd.key = mrInfo.lkey;
    localOut.push_back(notifyMd);
    HCCL_INFO("[AicpuTsRoceRegedMemMgr][GetAllMemDetails][local][notify] addr[0x%llx] devAddr[0x%llx] size[%llu] key[%u]",
        static_cast<unsigned long long>(notifyMd.addr), static_cast<unsigned long long>(notifyMd.devAddr),
        static_cast<unsigned long long>(notifyMd.size), notifyMd.key);
    return HCCL_SUCCESS;
}

void AicpuTsRoceRegedMemMgr::GatherRemoteMemDetails(std::vector<RoceMemDetails> &remoteOut) const
{
    for (const auto &epMgr : remoteRdmaRmaBufferMgrs_) {
        const auto &mgr = epMgr.second;
        if (mgr == nullptr) {
            continue;
        }
        mgr->ForEach([&remoteOut](const hccl::BufferKey<uintptr_t, u64> &, const std::shared_ptr<hccl::RemoteRdmaRmaBuffer> &rb) {
            if (rb == nullptr) {
                return;
            }
            auto *rma = static_cast<hccl::RmaBuffer *>(rb.get());
            if (rma->GetDevAddr() == nullptr) {
                return;
            }
            RoceMemDetails r{};
            r.addr = static_cast<u64>(reinterpret_cast<uintptr_t>(rma->GetAddr()));
            r.devAddr = static_cast<u64>(reinterpret_cast<uintptr_t>(rma->GetDevAddr()));
            r.size = rb->GetSize();
            r.key = rb->GetKey();
            remoteOut.push_back(r);
            HCCL_INFO("[AicpuTsRoceRegedMemMgr][GetAllMemDetails][remote][%zu] addr[0x%llx] devAddr[0x%llx] size[%llu] key[%u]",
                remoteOut.size() - 1U, static_cast<unsigned long long>(r.addr), static_cast<unsigned long long>(r.devAddr),
                static_cast<unsigned long long>(r.size), r.key);
        });
    }
}

HcclResult AicpuTsRoceRegedMemMgr::GetAllMemDetails(std::vector<RoceMemDetails> &localOut,
    std::vector<RoceMemDetails> &remoteOut) const
{
    localOut.clear();
    remoteOut.clear();
    CHK_RET(GatherLocalMemDetails(localOut));
    CHK_RET(AppendLocalNotifyMemDetails(localOut));
    GatherRemoteMemDetails(remoteOut);
    HCCL_INFO("[AicpuTsRoceRegedMemMgr][GetAllMemDetails] summary localCnt[%zu] remoteCnt[%zu] remoteMgrCnt[%zu]",
        localOut.size(), remoteOut.size(), remoteRdmaRmaBufferMgrs_.size());
    return HCCL_SUCCESS;
}

}  // namespace hcomm

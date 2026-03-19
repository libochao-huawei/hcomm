/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "endpoint_pair.h"
#include "hccl_common.h"
#include "hccl/hccl_res.h"
#include "log.h"
#include "endpoint.h"
#include "hccl_mem.h"
#include "hccl_one_sided_data.h"
#include "hccs_mem.h"
// for hccl_network.h
#include "inner/local_rdma_rma_buffer.h"
#include "inner/remote_rdma_rma_buffer.h"
#include "hccl_network.h"

using namespace hccl;
namespace hcomm {

HccsRegedMemMgr::HccsRegedMemMgr(HcclNetDevCtx netDevCtx)
{
    netDevCtx_ = netDevCtx;
}

HccsRegedMemMgr::~HccsRegedMemMgr()
{
    allRegisteredBuffers_.clear();
    remoteIpcRmaBufferMgrs_.clear();
}

HcclResult HccsRegedMemMgr::RegisterMemory(HcommMem mem, const char *memTag, void **memHandle)
{
    HCCL_INFO("[%s] Begin", __FUNCTION__);
    CHK_PTR_NULL(memHandle);

    NetDevContext *netDevCtx = static_cast<NetDevContext *>(netDevCtx_);
    std::shared_ptr<LocalIpcRmaBufferMgr> localIpcRmaBufferMgr = netDevCtx->GetlocalIpcRmaBufferMgr();
    CHK_PTR_NULL(localIpcRmaBufferMgr);

    std::shared_ptr<hccl::LocalIpcRmaBuffer> localIpcRmaBuffer = nullptr;
 
    // LocalIpcRmaBuffer构造函数存在注册动作，在调用该构造函数前需检查是否注册过
    hccl::BufferKey<uintptr_t, u64> tempKey(reinterpret_cast<uintptr_t>(mem.addr), mem.size);
    auto findPair = localIpcRmaBufferMgr->Find(tempKey);
    if(findPair.first) {
        localIpcRmaBuffer = findPair.second;
    } else {
        // 构造LocalIpcRmaBuffer
        EXECEPTION_CATCH((localIpcRmaBuffer = std::make_shared<hccl::LocalIpcRmaBuffer>(
            netDevCtx_, mem.addr, mem.size, static_cast<RmaMemType>(mem.type))),
            return HCCL_E_PTR);
    }
    
    // 注册到LocalIpcRmaBuffer计数器
    auto resultPair = localIpcRmaBufferMgr->Add(tempKey, localIpcRmaBuffer);
    if (resultPair.first == localIpcRmaBufferMgr->End()) {
        // 若已注册内存有交叉，返回HCCL_E_INTERNAL
        HCCL_ERROR(
            "[HccsRegedMemMgr][RegisterMemory] [%s]The memory overlaps with the memory that has been registered.",
            __FUNCTION__);
        return HCCL_E_INTERNAL;
    }

    std::shared_ptr<hccl::LocalIpcRmaBuffer> &localBuffer = resultPair.first->second.buffer;
    CHK_SMART_PTR_NULL(localBuffer);
    *memHandle = static_cast<void *>(localBuffer.get());

    // 已注册：输入key是表中某一最相近key的全集。 返回添加该key的迭代器，及false
    // 未注册：输入key是表中某一最相近key的空集。 返回添加成功的迭代器，及true
    if (resultPair.second) {
        HcclResult ret = localBuffer->Init();
        if (ret != HCCL_SUCCESS) {
            // 此分支中一定删除成功
            localIpcRmaBufferMgr->Del(tempKey);
            HCCL_ERROR("[HccsRegedMemMgr][RegisterMemory]localbuffer init failed %d.", ret);
            return ret;
        }
        HCCL_INFO("[HccsRegedMemMgr][RegisterMemory]Register memory success! Add key {%p, %llu}", mem.addr, mem.size);
    } else {  
        HCCL_INFO(
                "[HccsRegedMemMgr][RegisterMemory]Memory is already registered, just increase the reference count. Add key "
                "{%p, %llu}", mem.addr, mem.size);;
        return HCCL_E_AGAIN;
    }

    this->allRegisteredBuffers_.push_back(localBuffer);
    return HCCL_SUCCESS;
}

HcclResult HccsRegedMemMgr::UnregisterMemory(void* memHandle)
{
    HCCL_INFO("[%s] Begin", __FUNCTION__);
    CHK_PTR_NULL(memHandle);
   
    NetDevContext *netDevCtx = static_cast<NetDevContext *>(netDevCtx_);
    std::shared_ptr<LocalIpcRmaBufferMgr> localIpcRmaBufferMgr = netDevCtx->GetlocalIpcRmaBufferMgr();
    CHK_PTR_NULL(localIpcRmaBufferMgr);

    hccl::LocalIpcRmaBuffer* localIpcRmaBuffer = static_cast<hccl::LocalIpcRmaBuffer*>(memHandle);

    // 从LocalRamBuffer计数器删除
    hccl::BufferKey<uintptr_t, u64> tempKey(reinterpret_cast<uintptr_t>(localIpcRmaBuffer->GetAddr()), localIpcRmaBuffer->GetSize());
    bool resultPair = false;
    EXECEPTION_CATCH(resultPair = localIpcRmaBufferMgr->Del(tempKey), return HCCL_E_NOT_FOUND);
    // 计数器大于1时，返回false，说明框架层有其它设备在使用这段内存，返回HCCL_E_AGAIN
    if (!resultPair) {
        HCCL_INFO("[HccsRegedMemMgr][[UnregisterMemory]Memory reference count is larger than 0"
                  "(used by other RemoteRank), do not deregister memory.");
        return HCCL_E_AGAIN;
    }

    // 删除vector中的LocalIpcRmaBuffer
    auto it = std::find_if(allRegisteredBuffers_.begin(), allRegisteredBuffers_.end(),
            [localIpcRmaBuffer](const std::shared_ptr<hccl::LocalIpcRmaBuffer>& ptr) {
                return ptr.get() == localIpcRmaBuffer;
            });

    if (it == allRegisteredBuffers_.end()) {
        HCCL_ERROR("[HccsRegedMemMgr][HccsRegedMemMgr] Memory not found in vector!");
        return HCCL_E_NOT_FOUND;
    }

    allRegisteredBuffers_.erase(it);
    return HCCL_SUCCESS;
}

HcclResult HccsRegedMemMgr::SerializeToMemDesc(const EndpointDesc &endpointDesc, std::string &ipcRmaBufferDesc,
    void **memDesc, uint32_t *descLen)
{
    HCCL_INFO("[%s] Begin", __FUNCTION__);
    CHK_PTR_NULL(memDesc);
    CHK_PTR_NULL(descLen);
 
    if (ipcRmaBufferDesc.empty()) {
        HCCL_ERROR("[HccsRegedMemMgr][[SerializeToMemDesc]ipcRmaBufferDesc is empty.");
        return HCCL_E_INTERNAL;
    }

    ipcRmaBufferDesc.resize(ipcRmaBufferDesc.length() + sizeof(EndpointDesc));
    // put the EndpointDesc at the end of the Serialize-ed buf
    if(memcpy_s(const_cast<char *>(ipcRmaBufferDesc.c_str()) + (ipcRmaBufferDesc.length() - sizeof(EndpointDesc)),
        sizeof(EndpointDesc), &endpointDesc, sizeof(EndpointDesc)) != EOK) {
        HCCL_ERROR("[RoceRegedMemMgr][SerializeToMemDesc] [%s] endpointDesc memcpy_s failed.", __func__);
        return HCCL_E_INTERNAL;
    }

    *descLen = static_cast<uint32_t>(ipcRmaBufferDesc.length());
    *memDesc = const_cast<char *>(ipcRmaBufferDesc.c_str());
    return HCCL_SUCCESS;
}

HcclResult HccsRegedMemMgr::DeSerializeFromMemDesc(const void *memDesc, uint32_t descLen,
    EndpointDesc &endpointDesc, std::string &ipcRmaBufferDesc)
{
    HCCL_INFO("[%s] Begin", __FUNCTION__);
    CHK_PTR_NULL(memDesc);

    const char *description = static_cast<const char *>(memDesc);
    HCCL_INFO("[%s] descLen[%u] memDesc[%s]", __FUNCTION__, descLen, description);

    if (descLen <= sizeof(EndpointDesc)) {
        HCCL_ERROR("[HccsRegedMemMgr][DeSerializeFromMemDesc] [%s] descLen :%u too small error. need more than size:[%llu]",
            __func__, sizeof(EndpointDesc));
        return HCCL_E_INTERNAL;
    }

    uint32_t ipcRmaBufferDescLen = descLen - sizeof(EndpointDesc);
    if (memcpy_s(&endpointDesc, sizeof(EndpointDesc), description + ipcRmaBufferDescLen, sizeof(EndpointDesc)) != EOK) {
        HCCL_ERROR("[HccsRegedMemMgr][DeSerializeFromMemDesc] [%s] endpointDesc copy error. aim size:[%llu]",
            __func__, sizeof(EndpointDesc));
        return HCCL_E_INTERNAL;
    }

    ipcRmaBufferDesc.resize(ipcRmaBufferDescLen);
    if (memcpy_s(const_cast<char *>(ipcRmaBufferDesc.c_str()), ipcRmaBufferDescLen,
                description, ipcRmaBufferDescLen) != EOK) {
        HCCL_ERROR("[HccsRegedMemMgr][DeSerializeFromMemDesc] [%s] ipcRmaBufferDesc copy error. aim size:[%llu]",
            __func__, ipcRmaBufferDescLen);
        return HCCL_E_INTERNAL;
    }

    return HCCL_SUCCESS;    
}

HcclResult HccsRegedMemMgr::MemoryExport(
    const EndpointDesc endpointDesc, void *memHandle, void **memDesc, uint32_t *memDescLen)
{
    HCCL_INFO("[%s] Begin", __FUNCTION__);
    CHK_PTR_NULL(memHandle);
    CHK_PTR_NULL(memDesc);
    CHK_PTR_NULL(memDescLen);

    hccl::LocalIpcRmaBuffer *localIpcRmaBuffer = reinterpret_cast<hccl::LocalIpcRmaBuffer *>(memHandle);
    std::string &ipcRmaBufferDesc = localIpcRmaBuffer->Serialize();
    HCCL_INFO("[%s] ipcRmaBufferDesc.len[%u]", __FUNCTION__, ipcRmaBufferDesc.length());

    CHK_RET(SerializeToMemDesc(endpointDesc, ipcRmaBufferDesc, memDesc, memDescLen));
    HCCL_INFO("[%s] descLen[%u]", __FUNCTION__, *memDescLen);
    return HCCL_SUCCESS;
}

HcclResult HccsRegedMemMgr::AddMemDesc(EndpointDesc &endpointDesc, const void *memDesc, uint32_t descLen,
    std::shared_ptr<hccl::RemoteIpcRmaBuffer> &remoteIpcRmaBuffer)
{
    HCCL_INFO("[%s] Begin", __FUNCTION__);
    CHK_PTR_NULL(remoteIpcRmaBuffer);

    HCCL_INFO("[%s] addr[%p] size[%u] start", __FUNCTION__, memDesc, descLen);

    // 放到 remoteIpcRmaBufferMgrs_
    hccl::BufferKey<uintptr_t, u64> tempKey(reinterpret_cast<uintptr_t>(memDesc), descLen);
    if(remoteIpcRmaBufferMgrs_.find(endpointDesc) == remoteIpcRmaBufferMgrs_.end()) {
        std::unique_ptr<RemoteIpcRmaBufferMgr> remoteIpcRmaBufferMgr;
        EXECEPTION_CATCH((remoteIpcRmaBufferMgr = std::make_unique<RemoteIpcRmaBufferMgr>()),
            return HCCL_E_PTR);
        CHK_SMART_PTR_NULL(remoteIpcRmaBufferMgr);
        remoteIpcRmaBufferMgrs_[endpointDesc] = std::move(remoteIpcRmaBufferMgr);
        HCCL_INFO("[HccsRegedMemMgr][AddMemDesc]add remoteIpcRmaBufferMgr successfully!");
    }

    auto resultPair = remoteIpcRmaBufferMgrs_[endpointDesc]->Add(tempKey, remoteIpcRmaBuffer);
    if(!resultPair.second) {
        HCCL_ERROR("[HccsRegedMemMgr][AddMemDesc]This memDesc has already been imported!");
        return HCCL_E_AGAIN;
    }

    HCCL_INFO("[%s] addr[%p] size[%u] done", __FUNCTION__, memDesc, descLen);
    return HCCL_SUCCESS;
}

HcclResult HccsRegedMemMgr::DeleteMemDesc(EndpointDesc &endpointDesc, const void *memDesc, uint32_t descLen,
    std::shared_ptr<hccl::RemoteIpcRmaBuffer> &remoteIpcRmaBuffer)
{
    HCCL_INFO("[%s] Begin", __FUNCTION__);

    HCCL_INFO("[%s] addr[%p] size[%u] start", __FUNCTION__, memDesc, descLen);

    if(remoteIpcRmaBufferMgrs_.find(endpointDesc) == remoteIpcRmaBufferMgrs_.end()) {
        HCCL_ERROR("[HccsRegedMemMgr][DeleteMemDesc] Remote buffer manager Not Found.");
        return HCCL_E_NOT_FOUND;
    }

    // 删除 remoteIpcRmaBufferMgrs_
    hccl::BufferKey<uintptr_t, u64> tempKey(reinterpret_cast<uintptr_t>(memDesc),descLen);
    auto remoteIpcRmaBufferPair = remoteIpcRmaBufferMgrs_[endpointDesc]->Find(tempKey);
    if(!remoteIpcRmaBufferPair.first) {
        HCCL_ERROR("[HccsRegedMemMgr][DeleteMemDesc] Remote buffer manager Not Found!");
        return HCCL_E_AGAIN;
    }

    remoteIpcRmaBuffer = remoteIpcRmaBufferPair.second;

    bool delResultPair = false;
    EXECEPTION_CATCH(delResultPair = remoteIpcRmaBufferMgrs_[endpointDesc]->Del(tempKey), return HCCL_E_NOT_FOUND);
    // 计数器大于1时，返回false，说明框架层有其它设备在使用这段内存，返回HCCL_E_AGAIN
    if (!delResultPair) {
        HCCL_INFO("[HccsRegedMemMgr][[DeleteMemDesc] Memory reference count is larger than 0"
                    "(used by other RemoteRank).");
        return HCCL_E_AGAIN;
    }

    if (!remoteIpcRmaBufferMgrs_[endpointDesc]->size()) {
        remoteIpcRmaBufferMgrs_.erase(endpointDesc);
    }

    HCCL_INFO("[%s] addr[%p] size[%u] done", __FUNCTION__, memDesc, descLen);

    return HCCL_SUCCESS;
}

HcclResult HccsRegedMemMgr::MemoryImport(const void *memDesc, uint32_t descLen, HcommMem *outMem)
{
    HCCL_INFO("[%s] Begin", __FUNCTION__);
    CHK_PTR_NULL(memDesc);
    CHK_PTR_NULL(outMem);

    HCCL_INFO("[%s] descLen[%u] memDesc[%s]", __FUNCTION__, descLen, static_cast<const char *>(memDesc));

    EndpointDesc endpointDesc;
    std::string ipcRmaBufferDesc;
    CHK_RET(DeSerializeFromMemDesc(memDesc, descLen, endpointDesc, ipcRmaBufferDesc));
    HCCL_INFO("[%s] ipcRmaBufferDesc.len[%u]", __FUNCTION__, ipcRmaBufferDesc.length());

    std::shared_ptr<hccl::RemoteIpcRmaBuffer> remoteIpcRmaBuffer;
    EXECEPTION_CATCH(
        remoteIpcRmaBuffer = std::make_shared<hccl::RemoteIpcRmaBuffer>(netDevCtx_),
        return HCCL_E_PTR;
    );

    CHK_PTR_NULL(remoteIpcRmaBuffer);
    HcclResult deRet = remoteIpcRmaBuffer->Deserialize(ipcRmaBufferDesc);
    HcclResult openRet = remoteIpcRmaBuffer->Open();
    if (deRet != HCCL_SUCCESS || openRet != HCCL_SUCCESS) {
        CHK_PRT_RET(deRet != HCCL_SUCCESS,
            HCCL_ERROR("[HccsRegedMemMgr][[MemoryImport]RemoteIpcRmaBuffer Deserialize failed."), deRet);
        CHK_PRT_RET(openRet != HCCL_SUCCESS,
            HCCL_ERROR("[HccsRegedMemMgr][[MemoryImport]RemoteIpcRmaBuffer Open failed."), openRet);
    }

    CHK_RET(AddMemDesc(endpointDesc, memDesc, descLen, remoteIpcRmaBuffer));

    outMem->addr = reinterpret_cast<void *>(remoteIpcRmaBuffer->GetAddr());
    outMem->size = remoteIpcRmaBuffer->GetSize();
    outMem->type = static_cast<HcclMemType>(remoteIpcRmaBuffer->GetRmaType());

    HCCL_INFO("[%s]addr[%p] size[%lu], type[%u] done", __FUNCTION__, outMem->addr, outMem->size, outMem->type);

    return HCCL_SUCCESS;
}

HcclResult HccsRegedMemMgr::MemoryUnimport(const void *memDesc, uint32_t descLen)
{
    HCCL_INFO("[%s] Begin", __FUNCTION__);
    CHK_PTR_NULL(memDesc);

    std::string ipcRmaBufferDesc;
    EndpointDesc endpointDesc;
    CHK_RET(DeSerializeFromMemDesc(memDesc, descLen, endpointDesc, ipcRmaBufferDesc));

    std::shared_ptr<hccl::RemoteIpcRmaBuffer> remoteIpcRmaBuffer;
    CHK_RET(DeleteMemDesc(endpointDesc, memDesc, descLen, remoteIpcRmaBuffer));
    CHK_PTR_NULL(remoteIpcRmaBuffer);
    HcclResult ret = remoteIpcRmaBuffer->Close();
    CHK_PRT_RET(ret != HCCL_SUCCESS,
        HCCL_ERROR("[HccsRegedMemMgr][[MemoryUnimport]RemoteIpcRmaBuffer Close failed"), ret);

    return HCCL_SUCCESS;
}

HcclResult HccsRegedMemMgr::GetAllMemHandles(void **memHandles, uint32_t *memHandleNum)
{
    HCCL_INFO("[%s] Begin", __FUNCTION__);
    CHK_PTR_NULL(memHandles);
    CHK_PTR_NULL(memHandleNum);

    uint32_t bufferCount = static_cast<uint32_t>(allRegisteredBuffers_.size());
    *memHandleNum = bufferCount;

    HCCL_INFO("[HccsRegedMemMgr][[GetAllMemHandles]memHandleNum is [%d]", bufferCount);

    *memHandles = (bufferCount == 0) ? nullptr : reinterpret_cast<void *>(allRegisteredBuffers_.data());

    return HCCL_SUCCESS;
}

HcclResult HccsRegedMemMgr::EnableMemAccess(const HcommMemGrantInfo *remoteGrantInfo)
{
    HCCL_INFO("[%s] Begin", __FUNCTION__);
    CHK_PTR_NULL(remoteGrantInfo);

    HCCL_INFO("[HccsRegedMemMgr][[EnableMemAccess]Grant remotePid:%d, remoteSdid:%u",
        remoteGrantInfo->pid, remoteGrantInfo->sdid);
    for (auto it = allRegisteredBuffers_.begin(); it != allRegisteredBuffers_.end(); it++) {
        std::shared_ptr<hccl::LocalIpcRmaBuffer> localIpcRmaBuffer = *it;
        CHK_PTR_NULL(localIpcRmaBuffer);

        HcclResult ret = localIpcRmaBuffer->Grant(remoteGrantInfo->pid, remoteGrantInfo->sdid);
        CHK_PRT_RET((ret != HCCL_SUCCESS),
            HCCL_ERROR("[HccsRegedMemMgr][[EnableMemAccess]Grant remotePid:%d, remoteSdid:%u error", 
                remoteGrantInfo->pid, remoteGrantInfo->sdid), ret);
    }

    HCCL_INFO("[HccsRegedMemMgr][[EnableMemAccess]Grant remotePid:%d, remoteSdid:%u done",
        remoteGrantInfo->pid, remoteGrantInfo->sdid);
    return HCCL_SUCCESS;
}

HcclResult HccsRegedMemMgr::DisableMemAccess(const HcommMemGrantInfo *remoteGrantInfo)
{
    HCCL_INFO("[%s] Begin", __FUNCTION__);
    CHK_PTR_NULL(remoteGrantInfo);
    HCCL_INFO("[HccsRegedMemMgr][[DisableMemAccess]Grant remotePid:%d, remoteSdid:%u done",
        remoteGrantInfo->pid, remoteGrantInfo->sdid);
    return HCCL_SUCCESS;
}
}
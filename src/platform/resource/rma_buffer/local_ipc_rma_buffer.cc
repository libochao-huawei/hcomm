/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "local_ipc_rma_buffer.h"
#include "local_ipc_rma_buffer_impl.h"
#include <sstream>

namespace hccl {
LocalIpcRmaBuffer::LocalIpcRmaBuffer(const HcclNetDevCtx netDevCtx, void* addr, u64 size, const RmaMemType memType)
    : RmaBuffer(netDevCtx, addr, size, memType, RmaType::IPC_RMA)
{
    pimpl_ = std::make_unique<LocalIpcRmaBufferImpl>(netDevCtx, addr, size, memType);
}

LocalIpcRmaBuffer::LocalIpcRmaBuffer(const HcclNetDevCtx netDevCtx, void* addr, u64 size,
    const RmaMemType memType, std::shared_ptr<LocalIpcRmaBuffer> parent)
    : RmaBuffer(netDevCtx, addr, size, memType, RmaType::IPC_RMA)
{
    isAlias_ = true;
    parentBuffer_ = parent;
    parentTyped_ = parent;
    if (parent != nullptr) {
        uintptr_t parentAddr = reinterpret_cast<uintptr_t>(parent->GetAddr());
        uintptr_t aliasAddr = reinterpret_cast<uintptr_t>(addr);
        uintptr_t parentDevAddr = reinterpret_cast<uintptr_t>(parent->GetDevAddr());
        devAddr = reinterpret_cast<void *>(parentDevAddr + (aliasAddr - parentAddr));
    }
}

LocalIpcRmaBuffer::~LocalIpcRmaBuffer()
{
    HcclResult res = Destroy();
    if (res != HCCL_SUCCESS) {
        HCCL_ERROR("[LocalIpcRmaBuffer][~LocalIpcRmaBuffer]failed, ret[%d]", res);
    }
}

HcclResult LocalIpcRmaBuffer::Init()
{
    if (isAlias_) {
        devAddr = addr;
        return HCCL_SUCCESS;
    }
    CHK_PTR_NULL(addr);
    CHK_PRT_RET((memType >= RmaMemType::TYPE_NUM),
        HCCL_ERROR("[LocalIpcRmaBuffer][Init]RmaMemType[%d] is invalid.", static_cast<int>(memType)), HCCL_E_PARA);
    CHK_PRT_RET((size == 0 || (memType == RmaMemType::HOST && size >= HOST_MEM_MAX_COUNT) ||
        (memType == RmaMemType::DEVICE && size >= DEVICE_MEM_MAX_COUNT)),
        HCCL_ERROR("[LocalIpcRmaBuffer][Init]memory size[%llu] should be greater than 0 and less than [%llu].",
        size, (memType == RmaMemType::DEVICE ? HOST_MEM_MAX_COUNT : DEVICE_MEM_MAX_COUNT)), HCCL_E_PARA);

    CHK_SMART_PTR_NULL(pimpl_);
    HcclResult ret = pimpl_->Init();
    if (ret != HCCL_SUCCESS) {
        pimpl_ = nullptr;
        HCCL_ERROR("[LocalIpcRmaBuffer][Init]Init failed, ret[%d]", ret);
        return ret;
    }

    this->devAddr   = pimpl_->GetDevAddr();

    return HCCL_SUCCESS;
}

HcclResult LocalIpcRmaBuffer::Destroy()
{
    if (isAlias_) {
        parentTyped_ = nullptr;
        parentBuffer_ = nullptr;
        return HCCL_SUCCESS;
    }
    if (pimpl_ != nullptr) {
        HcclResult ret = pimpl_->Destroy();
        if (ret != HCCL_SUCCESS) {
            HCCL_ERROR("[LocalIpcRmaBuffer][Destroy]Destroy failed, ret[%d]", ret);
        }
        pimpl_  = nullptr;
        addr    = nullptr;
        size    = 0;
        devAddr = nullptr;
        return ret;
    }
    return HCCL_SUCCESS;
}

std::string &LocalIpcRmaBuffer::Serialize()
{
    if (isAlias_) {
        if (!aliasSerializeStr_.empty()) {
            return aliasSerializeStr_;
        }
        // For alias, serialize with own addr/size but parent's IPC memName
        std::string &parentSer = parentTyped_->Serialize();
        // parentSer contains serialized data with parent's addr/size/memName
        // We need to replace addr and size with alias's values
        // memName and memOffset from parent are reused
        std::ostringstream oss;
        u8 type{static_cast<u8>(rmaType)};
        oss.write(reinterpret_cast<const char_t *>(&type), sizeof(type));
        oss.write(reinterpret_cast<const char_t *>(&addr), sizeof(addr));
        oss.write(reinterpret_cast<const char_t *>(&size), sizeof(size));
        oss.write(reinterpret_cast<const char_t *>(&devAddr), sizeof(devAddr));
        oss.write(reinterpret_cast<const char_t *>(&memType), sizeof(memType));
        // Use parent's IPC memName and offset (adjusted)
        // The parent serialization has memName at offset after type+addr+size+devAddr+memType
        const char *parentData = parentSer.data();
        size_t headerSize = sizeof(type) + sizeof(addr) + sizeof(size) + sizeof(devAddr) + sizeof(memType);
        if (parentSer.size() > headerSize) {
            oss.write(parentData + headerSize, parentSer.size() - headerSize);
        }
        aliasSerializeStr_ = oss.str();
        return aliasSerializeStr_;
    }
    return pimpl_->Serialize();
}

HcclResult LocalIpcRmaBuffer::Grant(u32 remotePid, u32 remoteSdid)
{
    if (isAlias_) {
        return (parentTyped_ != nullptr) ? parentTyped_->Grant(remotePid, remoteSdid) : HCCL_E_PTR;
    }
    return pimpl_->Grant(remotePid, remoteSdid);
}

}
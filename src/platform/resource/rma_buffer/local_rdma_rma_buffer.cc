/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "local_rdma_rma_buffer.h"
#include "local_rdma_rma_buffer_impl.h"
#include <sstream>

namespace hccl {
LocalRdmaRmaBuffer::LocalRdmaRmaBuffer(const HcclNetDevCtx netDevCtx, void* addr, u64 size, const RmaMemType memType)
    : RmaBuffer(netDevCtx, addr, size, memType, RmaType::RDMA_RMA)
{
    pimpl_ = std::make_unique<LocalRdmaRmaBufferImpl>(netDevCtx, addr, size, memType);
}

LocalRdmaRmaBuffer::LocalRdmaRmaBuffer(const HcclNetDevCtx netDevCtx, void* addr, u64 size,
    const RmaMemType memType, std::shared_ptr<LocalRdmaRmaBuffer> parent)
    : RmaBuffer(netDevCtx, addr, size, memType, RmaType::RDMA_RMA)
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

LocalRdmaRmaBuffer::~LocalRdmaRmaBuffer()
{
    HcclResult res = Destroy();
    if (res != HCCL_SUCCESS) {
        HCCL_ERROR("[LocalRdmaRmaBuffer][~LocalRdmaRmaBuffer]failed, ret[%d]", res);
    }
}

HcclResult LocalRdmaRmaBuffer::Init()
{
    if (isAlias_) {
        devAddr = addr;
        return HCCL_SUCCESS;
    }
    CHK_PTR_NULL(addr);
    CHK_PRT_RET((memType >= RmaMemType::TYPE_NUM),
        HCCL_ERROR("[LocalRdmaRmaBuffer][Init]RmaMemType[%d] is invalid.", static_cast<int>(memType)), HCCL_E_PARA);
    CHK_PRT_RET((size == 0 || (memType == RmaMemType::HOST && size >= HOST_MEM_MAX_COUNT) ||
        (memType == RmaMemType::DEVICE && size >= DEVICE_MEM_MAX_COUNT)),
        HCCL_ERROR("[LocalRdmaRmaBuffer][Init]memory size[%llu] should be greater than 0 and less than [%llu].",
        size, (memType == RmaMemType::DEVICE ? HOST_MEM_MAX_COUNT : DEVICE_MEM_MAX_COUNT)), HCCL_E_PARA);

    CHK_SMART_PTR_NULL(pimpl_);
    HcclResult ret = pimpl_->Init();
    if (ret != HCCL_SUCCESS) {
        pimpl_ = nullptr;
        HCCL_ERROR("[LocalRdmaRmaBuffer][Init]Init failed, ret[%d]", ret);
        return ret;
    }

    this->devAddr   = pimpl_->GetDevAddr();

    return HCCL_SUCCESS;
}

HcclResult LocalRdmaRmaBuffer::Destroy()
{
    if (isAlias_) {
        parentTyped_ = nullptr;
        parentBuffer_ = nullptr;
        return HCCL_SUCCESS;
    }
    if (pimpl_ != nullptr) {
        HcclResult ret = pimpl_->Destroy();
        if (ret != HCCL_SUCCESS) {
            HCCL_ERROR("[LocalRdmaRmaBuffer][Destroy]Destroy failed, ret[%d]", ret);
        }
        pimpl_  = nullptr;
        addr    = nullptr;
        size    = 0;
        devAddr = nullptr;
        return ret;
    }
    return HCCL_SUCCESS;
}

std::string &LocalRdmaRmaBuffer::Serialize()
{
    if (isAlias_) {
        if (!aliasSerializeStr_.empty()) {
            return aliasSerializeStr_;
        }
        std::ostringstream oss;
        u8 type{static_cast<u8>(rmaType)};
        oss.write(reinterpret_cast<const char_t *>(&type), sizeof(type));
        oss.write(reinterpret_cast<const char_t *>(&addr), sizeof(addr));
        oss.write(reinterpret_cast<const char_t *>(&size), sizeof(size));
        oss.write(reinterpret_cast<const char_t *>(&devAddr), sizeof(devAddr));
        oss.write(reinterpret_cast<const char_t *>(&memType), sizeof(memType));
        u32 parentLkey = (parentTyped_ != nullptr) ? parentTyped_->GetKey() : 0;
        oss.write(reinterpret_cast<const char_t *>(&parentLkey), sizeof(parentLkey));
        aliasSerializeStr_ = oss.str();
        return aliasSerializeStr_;
    }
    return pimpl_->Serialize();
}

u32 LocalRdmaRmaBuffer::GetKey() const
{
    if (isAlias_) {
        return (parentTyped_ != nullptr) ? parentTyped_->GetKey() : 0;
    }
    return pimpl_->GetKey();
}

HcclResult LocalRdmaRmaBuffer::Remap(void* addr, u64 length)
{
    if (isAlias_) {
        return HCCL_SUCCESS;
    }
    return pimpl_->Remap(addr, length);
}

}
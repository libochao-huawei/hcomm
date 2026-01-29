/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "comm_mems.h"
#include <cstdlib>
#include <algorithm>

namespace hccl {
CommMems::CommMems(uint64_t bufferSize)
    : bufferSize_(bufferSize)
{
}

HcclResult CommMems::Add(void *addr, uint64_t len)
{
    return HCCL_SUCCESS;
}

HcclResult CommMems::GetHcclBuffer(void *&addr, uint64_t &len)
{
    addr = reinterpret_cast<void*>(addr_);
    len = static_cast<uint64_t>(size_);
    return HCCL_SUCCESS;
}

HcclResult CommMems::Init(HcclMem cclBuffer)
{
    addr_ = cclBuffer.addr;
    size_ = cclBuffer.size;
    memType_ = cclBuffer.type;
    HCCL_INFO("[CommMems][Init] addr[%p] size[%u] memType[%u]", cclBuffer.addr, cclBuffer.size, cclBuffer.type);
    return HCCL_SUCCESS;
}

HcclResult CommMems::GetMemoryHandles(std::vector<HcclMem> &mem)
{
    HcclMem memTemp;
    memTemp.size = size_;
    memTemp.type = memType_;
    memTemp.addr = addr_;;
    mem.push_back(memTemp);

    HCCL_INFO("[CommMems][%s] HcclMem: size[%u], addr[%p], type[%d]", 
        __func__, memTemp.size, memTemp.addr, (int)memTemp.type
    );

    return HCCL_SUCCESS;
}

}
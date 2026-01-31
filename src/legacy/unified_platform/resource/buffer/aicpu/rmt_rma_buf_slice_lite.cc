/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "rmt_rma_buf_slice_lite.h"
#include "log.h"
#include "string_util.h"
namespace Hccl {
RmtRmaBufSliceLite::RmtRmaBufSliceLite(u64 addr, u32 size, u32 rkey, u32 tokenId, u32 tokenValue)
    : addr_(addr), size_(size), rkey_(rkey), tokenId_(tokenId), tokenValue_(tokenValue)
{
    HCCL_INFO("RmtRmaBufSliceLite::RmtRmaBufSliceLite:%s", Describe().c_str());
}
u64 RmtRmaBufSliceLite::GetAddr() const
{
    return addr_;
}

u32 RmtRmaBufSliceLite::GetSize() const
{
    return size_;
}

u32 RmtRmaBufSliceLite::GetRkey() const
{
    return rkey_;
}

u32 RmtRmaBufSliceLite::GetTokenId() const
{
    return tokenId_;
}

u32 RmtRmaBufSliceLite::GetTokenValue() const
{
    return tokenValue_;
}

std::string RmtRmaBufSliceLite::Describe() const
{
    return StringFormat("RmtRmaBufSliceLite[addr=0x%llx, size=0x%x, rkey=%u]", addr_, size_,
                        rkey_);
}

} // namespace Hccl
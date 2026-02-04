/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "stream.h"
#include "runtime_api_exception.h"
#include "log.h"
#include "binary_stream.h"
namespace Hccl {

Stream::Stream(aclrtStream ptr) : ptr(ptr), selfOwned(false)
{
    id = static_cast<u32>(HrtGetStreamId(ptr));
    InitDevPhyId();
}

Stream::Stream(bool deviceUsed) : selfOwned(true), devUsed(deviceUsed)
{
    try {
        if (deviceUsed) {
            ptr  = HrtStreamCreateWithFlags(HCCL_STREAM_PRIORITY_HIGH, RT_STREAM_CP_PROCESS_USE);
            sqId = HrtStreamGetSqId(ptr);
            cqId = HrtStreamGetCqId(ptr);
        } else {
            ptr = HrtStreamCreateWithFlags(HCCL_STREAM_PRIORITY_HIGH, RT_STREAM_FAST_LAUNCH  | RT_STREAM_FAST_SYNC);
        }
        id = static_cast<u32>(HrtGetStreamId(ptr));
        InitDevPhyId();
    } catch (RuntimeApiException &e) {
        HCCL_ERROR("HrtGetStreamId failed: %s", e.what());
        HrtStreamDestroy(ptr);
        throw;
    }
}

Stream::~Stream()
{
    try {
        if (selfOwned) {
            HrtStreamDestroy(ptr);
        }
    } catch (HcclException &e) {
        HCCL_ERROR(e.what());
    } catch (std::exception &e) {
        HCCL_ERROR(e.what());
    } catch (...) {
        HCCL_ERROR("Unknow Error occurs when destruct stream %d", id);
    }
}

void Stream::SetStmMode(u64 stmMode)
{
    mode = stmMode;
}

aclrtStream Stream::GetPtr() const
{
    return ptr;
}

u32 Stream::GetId() const
{
    return id;
}

bool Stream::IsSelfOwned() const
{
    return selfOwned;
}

u64 Stream::GetMode() const
{
    return mode;
}

std::vector<char> Stream::GetUniqueId() const
{
    std::vector<char> result;

    BinaryStream binaryStream;
    binaryStream << id;
    binaryStream << sqId;
    binaryStream << devPhyId;    
    binaryStream << cqId;
    binaryStream.Dump(result);
    HCCL_INFO("Stream::GetUniqueId:%s:data=%s", Describe().c_str(), Bytes2hex(result.data(), result.size()).c_str());
    return result;
}

std::string Stream::Describe() const
{
    return StringFormat("Stream[ptr=%p, id=%u, sqId=%u, selfOwned=%u, devUsed=%d, devPhyId=%u]", ptr, id, sqId,
                        selfOwned, devUsed, devPhyId);
}

void Stream::InitDevPhyId()
{
    devPhyId = HrtGetDevicePhyIdByIndex(HrtGetDevice());
}

} // namespace Hccl
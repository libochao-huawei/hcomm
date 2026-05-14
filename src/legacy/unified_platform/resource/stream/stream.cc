/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FIT FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "stream.h"
#include "log.h"
#include "binary_stream.h"
#include "exception_util.h"

namespace Hccl {

Stream::Stream(void* ptr)
    : ptr(static_cast<aclrtStream>(ptr)), id(0), selfOwned(false), mode(0), devUsed(false), sqId(0), cqId(0), devPhyId(0), isMaster_(true)
{
}

Stream::Stream(bool devUsed)
    : ptr(nullptr), id(0), selfOwned(false), mode(0), devUsed(devUsed), sqId(0), cqId(0), devPhyId(0), isMaster_(true)
{
}

HcclResult Stream::Create(bool deviceUsed, bool isMaster, std::unique_ptr<Stream>& stream)
{
    stream = std::unique_ptr<Stream>(new Stream());
    stream->selfOwned = true;
    stream->devUsed = deviceUsed;
    stream->isMaster_ = isMaster;

    if (deviceUsed) {
        CHK_RET(HrtStreamCreateWithFlags(HCCL_STREAM_PRIORITY_HIGH, ACL_STREAM_DEVICE_USE_ONLY, stream->ptr));
        CHK_RET(HrtStreamGetSqId(stream->ptr, stream->sqId));
        CHK_RET(HrtStreamGetCqId(stream->ptr, stream->cqId));
    } else {
        CHK_RET(HrtStreamCreateWithFlags(HCCL_STREAM_PRIORITY_HIGH,
            ACL_STREAM_FAST_LAUNCH | ACL_STREAM_FAST_SYNC, stream->ptr));
        CHK_RET(HrtStreamSetMode(stream->ptr, STREAM_MODE_STOP_ON_FAILURE));
    }

    s32 streamId;
    CHK_RET(HrtGetStreamId(stream->ptr, streamId));
    stream->id = static_cast<u32>(streamId);

    CHK_RET(Stream::InitDevPhyId(stream->devPhyId));

    stream->mode = 0;
    return HCCL_SUCCESS;
}

HcclResult Stream::CreateFromPtr(aclrtStream ptr, bool isMaster, std::unique_ptr<Stream>& stream)
{
    stream = std::unique_ptr<Stream>(new Stream());
    stream->ptr = ptr;
    stream->selfOwned = false;
    stream->isMaster_ = isMaster;

    s32 streamId;
    CHK_RET(HrtGetStreamId(ptr, streamId));
    stream->id = static_cast<u32>(streamId);

    CHK_RET(Stream::InitDevPhyId(stream->devPhyId));

    return HCCL_SUCCESS;
}

HcclResult Stream::InitDevPhyId(u32& devPhyId)
{
    s32 deviceId;
    CHK_RET(HrtGetDevice(deviceId));
    DevId phyId;
    CHK_RET(HrtGetDevicePhyIdByIndex(deviceId, phyId));
    devPhyId = phyId;
    return HCCL_SUCCESS;
}

Stream::Stream() : ptr(nullptr), id(0), selfOwned(false), mode(0), devUsed(false), sqId(0), cqId(0), devPhyId(0),
                    isMaster_(true) {}

Stream::Stream(aclrtStream ptr, bool selfOwned, bool devUsed, bool isMaster, u32 id, u32 sqId, u32 cqId, u64 mode,
               u32 devPhyId)
    : ptr(ptr), id(id), selfOwned(selfOwned), mode(mode), devUsed(devUsed), sqId(sqId), cqId(cqId),
      devPhyId(devPhyId), isMaster_(isMaster)
{
}

Stream::~Stream()
{
    if (selfOwned && ptr != nullptr) {
        HcclResult ret = HrtStreamDestroy(ptr);
        if (ret != HCCL_SUCCESS) {
            HCCL_ERROR("HrtStreamDestroy failed in destructor, ret=%d", ret);
        }
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

u32 Stream::GetSqId() const
{
    return sqId;
}

bool Stream::IsMaster() const
{
    return isMaster_;
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

} // namespace Hccl

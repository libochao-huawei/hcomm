/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "rts_notify.h"
#include "log.h"
#include "dev_capability.h"
#include "not_support_exception.h"
#include "binary_stream.h"
#include "exception_util.h"
#include "runtime_api_exception.h"
namespace Hccl {

HcclResult RtsNotify::Create(bool devUsed, std::unique_ptr<RtsNotify>& notify)
{
    notify = std::make_unique<RtsNotify>();

    s32 deviceId;
    CHK_RET(HrtGetDevice(deviceId));

    DevId phyId;
    CHK_RET(HrtGetDevicePhyIdByIndex(deviceId, phyId));
    notify->devPhyId = phyId;

    if (devUsed) {
        CHK_RET(HrtNotifyCreateWithFlag(deviceId, ACL_NOTIFY_DEVICE_USE_ONLY, notify->handle));
    } else {
        CHK_RET(HrtNotifyCreate(deviceId, notify->handle));
    }

    CHK_RET(HrtGetNotifyID(notify->handle, notify->id));
    notify->devUsed = devUsed;
    return HCCL_SUCCESS;
}

RtsNotify::RtsNotify() : devPhyId(0), devUsed(false), handle(nullptr), id(0) {}

RtsNotify::RtsNotify(bool devUsed) : devPhyId(0), devUsed(devUsed), handle(nullptr), id(0)
{
    s32 deviceId;
    HcclResult ret = HrtGetDevice(deviceId);
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("RtsNotify: HrtGetDevice failed, ret=%d", ret);
        throw RuntimeApiException(StringFormat("RtsNotify: HrtGetDevice failed, ret=%d", ret));
    }

    DevId phyId;
    ret = HrtGetDevicePhyIdByIndex(deviceId, phyId);
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("RtsNotify: HrtGetDevicePhyIdByIndex failed, ret=%d", ret);
        throw RuntimeApiException(StringFormat("RtsNotify: HrtGetDevicePhyIdByIndex failed, ret=%d", ret));
    }
    devPhyId = phyId;

    if (devUsed) {
        ret = HrtNotifyCreateWithFlag(deviceId, ACL_NOTIFY_DEVICE_USE_ONLY, handle);
    } else {
        ret = HrtNotifyCreate(deviceId, handle);
    }
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("RtsNotify: HrtNotifyCreate failed, ret=%d", ret);
        throw RuntimeApiException(StringFormat("RtsNotify: HrtNotifyCreate failed, ret=%d", ret));
    }

    ret = HrtGetNotifyID(handle, id);
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("RtsNotify: HrtGetNotifyID failed, ret=%d", ret);
        throw RuntimeApiException(StringFormat("RtsNotify: HrtGetNotifyID failed, ret=%d", ret));
    }
}

RtsNotify::~RtsNotify()
{
    DECTOR_TRY_CATCH("RtsNotify", HrtNotifyDestroy(handle));
}

std::string RtsNotify::SetIpcName() const
{
    char ipcName[RTS_IPC_MEM_NAME_LEN] = {0};
    HrtIpcSetNotifyName(handle, ipcName, RTS_IPC_MEM_NAME_LEN);
    return ipcName;
}


void RtsNotify::SetIpcPid(s32 pid) const
{
    HrtSetIpcNotifyPid(handle, pid);
}

u32 RtsNotify::GetId() const
{
    return id;
}

u64 RtsNotify::GetOffset() const
{
    u32 offset;
    HcclResult ret = HrtNotifyGetOffset(handle, offset);
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("[RtsNotify] HrtNotifyGetOffset failed, ret=%d", ret);
        return 0;
    }
    return offset;
}

u64 RtsNotify::GetHandleAddr() const
{
    return reinterpret_cast<u64>(handle);
}

u32 RtsNotify::GetSize() const
{
    return DevCapability::GetInstance().GetNotifySize();
}

bool RtsNotify::IsDevUsed() const
{
    return devUsed;
}

u32 RtsNotify::GetDevPhyId() const
{
    return devPhyId;
}

std::vector<char> RtsNotify::GetUniqueId() const
{
    BinaryStream binaryStream;
    binaryStream << id;
    binaryStream << devPhyId;
    HCCL_INFO("[RtsNotify][GetUniqueId]id[%u] devPhyId[%u]", id, devPhyId);
    std::vector<char> result;
    binaryStream.Dump(result);
    return result;
}

void RtsNotify::Wait(const Stream &stream, u32 timeout) const
{
    HrtNotifyWaitWithTimeOut(handle, stream.GetPtr(), timeout);
}

void RtsNotify::Post(const Stream &stream) const
{
    HrtNotifyRecord(handle, stream.GetPtr());
}

string RtsNotify::Describe() const
{
    return StringFormat("RtsNotify[devUsed=%d, id=%u, handle=%p]", devUsed, id, handle);
}

} // namespace Hccl

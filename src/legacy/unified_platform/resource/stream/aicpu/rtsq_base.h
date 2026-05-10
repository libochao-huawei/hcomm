/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef HCCLV2_RTSQ_BASE_H
#define HCCLV2_RTSQ_BASE_H
#include <vector>
#include <functional>
#include <queue>
#include "types.h"
#include "buffer.h"
#include "notify_lite.h"
#include "reduce_op.h"
#include "data_type.h"
#include "reduce_in.h"
#include "ub_jetty_lite.h"

#include "ascend_hal.h"
namespace aicpu {
void __attribute__((weak)) __attribute__((visibility("default"))) GetSqeId(const uint32_t num, uint32_t &start, uint32_t &end);
}

namespace Hccl {
class RtsqBase {
public:
    RtsqBase(u32 devPhyId, u32 streamId, u32 sqId);

    virtual ~RtsqBase() = default;

    virtual void Reset();

    inline u32 GetSqDepth()
    {
        return sqDepth_;
    }

    inline u32 GetHead()
    {
        return sqHead_;
    }

    inline u32 GetTail()
    {
        return sqTail_;
    }

    inline u32 GetTaskId()
    {
        return taskId_;
    }

    void SetOpExecStatusCallback(std::function<void()> callback)
    {
        checkOpExecStatusCallback_ = callback;
    }

    virtual HcclResult LaunchTask()
    {
        HCCL_ERROR("LaunchTask not supported");
        return HCCL_E_NOT_SUPPORT;
    }

    virtual HcclResult NotifyWait(u32 notifyId)
    {
        (void)notifyId;
        HCCL_ERROR("NotifyWait not supported");
        return HCCL_E_NOT_SUPPORT;
    }

    virtual HcclResult NotifyWait(u32 notifyId, u32 timeout)
    {
        (void)notifyId;
        (void)timeout;
        HCCL_ERROR("NotifyWait not supported");
        return HCCL_E_NOT_SUPPORT;
    }

    virtual HcclResult Cnt1toNNotifyWait(u32 notifyId, u32 value)
    {
        (void)notifyId;
        (void)value;
        HCCL_ERROR("Cnt1toNNotifyWait not supported");
        return HCCL_E_NOT_SUPPORT;
    }

    virtual HcclResult Cnt1toNNotifyRecord(u32 notifyId, u32 value)
    {
        (void)notifyId;
        (void)value;
        HCCL_ERROR("Cnt1toNNotifyRecord not supported");
        return HCCL_E_NOT_SUPPORT;
    }

    virtual HcclResult CntNto1NotifyWait(u32 notifyId, u32 value)
    {
        (void)notifyId;
        (void)value;
        HCCL_ERROR("CntNto1NotifyWait not supported");
        return HCCL_E_NOT_SUPPORT;
    }

    virtual HcclResult CntNto1NotifyRecord(u32 notifyId, u32 value)
    {
        (void)notifyId;
        (void)value;
        HCCL_ERROR("CntNto1NotifyRecord not supported");
        return HCCL_E_NOT_SUPPORT;
    }

    virtual HcclResult NotifyRecordLoc(u32 notifyId)
    {
        (void)notifyId;
        HCCL_ERROR("NotifyRecordLoc not supported");
        return HCCL_E_NOT_SUPPORT;
    }

    virtual HcclResult NotifyRecordRmt(u32 rmtDevPhyId, u32 notifyId)
    {
        (void)rmtDevPhyId;
        (void)notifyId;
        HCCL_ERROR("NotifyRecordRmt not supported");
        return HCCL_E_NOT_SUPPORT;
    }

    virtual HcclResult SdmaCopy(u64 srcAddr, u64 dstAddr, u32 size, u32 partId)
    {
        (void)srcAddr;
        (void)dstAddr;
        (void)size;
        (void)partId;
        HCCL_ERROR("SdmaCopy not supported");
        return HCCL_E_NOT_SUPPORT;
    }

    virtual HcclResult SdmaReduce(u64 srcAddr, u64 dstAddr, u32 size, u32 partId, const ReduceIn &reduceIn)
    {
        (void)srcAddr;
        (void)dstAddr;
        (void)size;
        (void)partId;
        (void)reduceIn;
        HCCL_ERROR("SdmaReduce not supported");
        return HCCL_E_NOT_SUPPORT;
    }

    virtual HcclResult P2PWriteValue(u64 remoteAddr, u32 writeValue)
    {
        (void)remoteAddr;
        (void)writeValue;
        HCCL_ERROR("P2PWriteValue not supported");
        return HCCL_E_NOT_SUPPORT;
    }

    virtual HcclResult UbDbSend(const UbJettyLiteId &jettyLiteId, u16 piValue)
    {
        (void)jettyLiteId;
        (void)piValue;
        HCCL_ERROR("UbDbSend not supported");
        return HCCL_E_NOT_SUPPORT;
    }

    virtual HcclResult UbDirectSend(const UbJettyLiteId &jettyLiteId, u32 dwqeSize, const u8 *wqe)
    {
        (void)jettyLiteId;
        (void)dwqeSize;
        (void)wqe;
        HCCL_ERROR("UbDirectSend not supported");
        return HCCL_E_NOT_SUPPORT;
    }

    virtual HcclResult UbWriteValue(u64 dbAddr, u32 piValue)
    {
        (void)dbAddr;
        (void)piValue;
        HCCL_ERROR("UbWriteValue not supported");
        return HCCL_E_NOT_SUPPORT;
    }

    virtual HcclResult CCoreNotifyWait(u64 waitAddr, u64 curTurnCntAddr, bool last)
    {
        (void)waitAddr;
        (void)curTurnCntAddr;
        (void)last;
        HCCL_ERROR("CCoreNotifyWait not supported");
        return HCCL_E_NOT_SUPPORT;
    }

    virtual HcclResult CCoreNotifyRecord(u64 recordAddr, u64 curTurnCntAddr)
    {
        (void)recordAddr;
        (void)curTurnCntAddr;
        HCCL_ERROR("CCoreNotifyRecord not supported");
        return HCCL_E_NOT_SUPPORT;
    }

    u32 QuerySqHead();
    u32 QuerySqTail();

    virtual bool IsRtsqQueueSpaceSufficient()
    {
        return true;
    }

    virtual HcclResult SetPreStreamSyncReady()
    {
        HCCL_ERROR("SetPreStreamSyncReady not supported");
        return HCCL_E_NOT_SUPPORT;
    }

    virtual HcclResult SetPreStreamSyncFin()
    {
        HCCL_ERROR("SetPreStreamSyncFin not supported");
        return HCCL_E_NOT_SUPPORT;
    }

    virtual bool GetPreStreamSyncStatus()
    {
        return false;
    }

protected:
    u32 devPhyId_{0};
    u32 localDevId_{0};
    u32 streamId_{0}; // 填写到SQE中的streamId
    u32 sqId_{0};

    u32 sqHead_{0};
    u32 sqTail_{0};
    u32 sqDepth_{0};
    u64 sqBaseAddr_{0};

    u32 taskId_{0}; // 填写到SQE中的taskId，现改为由AICPU组件提供的sqeId维护

    std::function<void()> checkOpExecStatusCallback_{nullptr};

    u32 QuerySqDepth();

    std::string GetHwSqDescribe();

    void ConfigSqTail(u32 value);
    void ConfigDisableToEnable(u32 value);

    HcclResult SetTaskIdBySqeId();

private:
    u64 QuerySqBaseAddr();
    u32 QueryCqeStatus();

    u32 QuerySqStatusByType(drvSqCqPropType_t givenType);
    void ConfigSqStatusByType(drvSqCqPropType_t givenType, u32 value);
};

} // namespace Hccl

#endif
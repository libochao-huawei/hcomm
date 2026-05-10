/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef HCCLV2_RTSQ_A5_H
#define HCCLV2_RTSQ_A5_H
#include "rtsq_base.h"
namespace Hccl {

class RtsqA5 : public RtsqBase {
public:
    RtsqA5(u32 devPhyId, u32 streamId, u32 sqId);

    RtsqA5(u32 devPhyId, u32 streamId, u32 sqId, bool launchFlag);

    void Reset() override;

    HcclResult LaunchTask() override;

    HcclResult NotifyWait(u32 notifyId) override;

    HcclResult NotifyWait(u32 notifyId, u32 timeout);

    HcclResult NotifyRecordLoc(u32 notifyId) override;

    HcclResult Cnt1toNNotifyWait(u32 notifyId, u32 value) override;

    HcclResult Cnt1toNNotifyRecord(u32 notifyId, u32 value) override;

    HcclResult CntNto1NotifyWait(u32 notifyId, u32 value) override;

    HcclResult CntNto1NotifyRecord(u32 notifyId, u32 value) override;

    HcclResult SdmaCopy(u64 srcAddr, u64 dstAddr, u32 size, u32 partId) override;

    HcclResult SdmaReduce(u64 srcAddr, u64 dstAddr, u32 size, u32 partId, const ReduceIn &reduceIn) override;

    HcclResult P2PWriteValue(u64 remoteAddr, u32 writeValue) override;

    HcclResult UbDbSend(const UbJettyLiteId &jettyLiteId, u16 piValue) override;

    HcclResult UbDirectSend(const UbJettyLiteId &jettyLiteId, u32 dwqeSize, const u8 *wqe) override
    {
        (void)jettyLiteId;
        (void)dwqeSize;
        (void)wqe;
        return HCCL_SUCCESS;
    }

    HcclResult UbWriteValue(u64 dbAddr, u32 piValue) override
    {
        (void)dbAddr;
        (void)piValue;
        return HCCL_SUCCESS;
    }

    bool IsRtsqQueueSpaceSufficient() override;

    HcclResult CCoreNotifyWait(u64 waitAddr, u64 curTurnCntAddr, bool last) override;

    HcclResult CCoreNotifyRecord(u64 recordAddr, u64 curTurnCntAddr) override;

    HcclResult SetPreStreamSyncReady() override;

    HcclResult SetPreStreamSyncFin() override;

    bool GetPreStreamSyncStatus() override;

private:
    u32 pendingSqeCnt{0};

    bool isPreStreamSync = false;

    bool launchFlag_ = false;

    static constexpr u32 rtsqSqeSize     = 64;
    static constexpr u32 perLaunchSqeCnt = 128; // 最大launch 128个SQE

    u8 locBuf[rtsqSqeSize * perLaunchSqeCnt]{0};

    u8 *GetCurrSqeBuffer();

    void RefreshInfo();

    void CopyLocBufToSq();

    void MakeSureAvailableSpace();

    u32 GetTailToHeadDist() const;
};

} // namespace Hccl

#endif
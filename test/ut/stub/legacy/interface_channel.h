/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: interface channel header file
 * Create: 2025-11-20
 */

#ifndef INTERFACE_CHANNEL_H
#define INTERFACE_CHANNEL_H
#include "hccl_api.h"
#include "hccl/hccl_res.h"
#include "hccl_types.h"

namespace Hccl {
class IChannel {
public:
    IChannel();
    virtual ~IChannel();

    // 数据面调用verbs接口
    virtual HcclResult NotifyRecord(uint32_t remoteNotifyIdx) = 0;
    virtual HcclResult NotifyWait(uint32_t localNotifyIdx, uint32_t timeout) = 0;
    virtual HcclResult WriteWithNotify(void *dst, const void *src, uint64_t len, uint32_t remoteNotifyIdx) = 0;
    virtual HcclResult ChannelFence() = 0;
    virtual HcclResult GetNotifyNum(uint32_t *notifyNum) = 0;
    virtual HcclResult GetHcclBuffer(void*& addr, uint64_t& size) = 0;
    virtual HcclResult Init() = 0;
    virtual HcclResult DeInit() = 0;
    virtual HcclResult GetDpuRemoteMem(HcclMem **remoteMem, uint32_t *memNum) = 0;
    virtual uint32_t GetStatus() = 0;
};

}
#endif // INTERFACE_CHANNEL_H
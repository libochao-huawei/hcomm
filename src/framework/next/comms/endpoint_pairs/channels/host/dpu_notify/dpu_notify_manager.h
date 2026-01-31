/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: dpu notify manager
 * Create: 2025-10-31
 */
#ifndef HCCL_DPU_NOTIFY_MANAGER_H
#define HCCL_DPU_NOTIFY_MANAGER_H

#include <memory>
#include <vector>
#include <mutex>
#include "hccl_types.h"

namespace hcomm {

class DpuNotifyManager {
public:
    static DpuNotifyManager& GetInstance();
    ~DpuNotifyManager();

    HcclResult AllocNotifyIds(uint32_t notifyNum, std::vector<uint32_t> &notifyIds);
    HcclResult FreeNotifyIds(uint32_t notifyNum, std::vector<uint32_t> &notifyIds);

private:
    DpuNotifyManager(int numEntries);

    std::vector<unsigned char> notifyIdBitMap;  // 存放notify的位图
    std::vector<uint32_t> freeBit;              // 存放每个字节第一个非0的位
    uint32_t byteSize;                          // 字节为单位，即8192的bit对应1024字节

    void UpdateFreeBit(uint32_t index);

    int AllocSingleNotifyId();                   // 申请单个notify
    void FreeSingleNotifyId(uint32_t notifyId);  // 回收单个notify

    std::mutex mtxAlloc_;
    std::mutex mtxFree_;
};
}  // namespace Hccl

#endif  // HCCL_DPU_NOTIFY_MANAGER_H

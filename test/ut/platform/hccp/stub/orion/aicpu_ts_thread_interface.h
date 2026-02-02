#ifndef AICPU_TS_THREAD_INTERFACE_H
#define AICPU_TS_THREAD_INTERFACE_H

#include "hccl_types.h"

namespace Hccl {

class IAicpuTsThread {
public:
    IAicpuTsThread() {}

    ~IAicpuTsThread() {}

    void StreamLiteInit(uint32_t id, uint32_t sqIds, uint32_t phyId, uint32_t cqIds)
    {
        streamLiteVoidPtr_  = reinterpret_cast<void *>(0x3344);
    }

    HcclResult NotifyWait(uint32_t notifyId) const
    {
        return HCCL_SUCCESS;
    }

    HcclResult NotifyRecordLoc(uint32_t notifyId) const
    {
        return HCCL_SUCCESS;
    }

    HcclResult SdmaCopy(uint64_t dstAddr, uint64_t srcAddr, uint64_t sizeByte) const
    {
        return HCCL_SUCCESS;
    }

    HcclResult SdmaReduce(
        uint64_t dstAddr, uint64_t srcAddr, uint64_t sizeByte, uint32_t dataTypeRaw, uint32_t reduceOpRaw) const
    {
        return HCCL_SUCCESS;
    }

    HcclResult GetStreamLitePtr(void **streamLitePtrPtr)
    {
        *streamLitePtrPtr = streamLiteVoidPtr_;
        return HCCL_SUCCESS;
    }

    void LaunchTask() {}

private:
    void *streamLiteVoidPtr_ = nullptr;
};

}  // namespace Hccl

#endif
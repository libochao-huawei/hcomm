
#ifndef NDA_RDMA_BASE_VENDOR_OPS_H
#define NDA_RDMA_BASE_VENDOR_OPS_H

#include "hccp_common.h"
#include "hccl_common.h"
#include "hccp_nda.h"

namespace hcomm {

static constexpr uint32_t kMaxWqeSize = 256;    // Assume no wqe will exceed 256 Byte

struct WqeEntry {
    alignas(64) uint8_t data[kMaxWqeSize];  // Wqe内容填充
    uint32_t            wqeSize;            // 实际字节数
};


// PI、CI的入队出队在基类中解决，不知可否这样实现
class NdaBaseVendorOps {
public:
    NdaBaseVendorOps(SqContext sqContext, CqContext cqContext): sqContext_(sqContext), cqContext_(cqContext_) {}
    ~NdaBaseVendorOps() {}

    // 下发wqe，通用操作
    int ProcessOneWqe(const WqeEntry *wqe, uint32_t opCode) const
    {
        HCCL_INFO("[NdaRdmaConnection::%s] start, opCode[%d]", __func__, opCode);

        auto wqeSize = wqe->wqeSize;
        auto sqDepth = sqContext_.depth;
        auto sqBaseAddr = sqContext_.sqVa;

        // sqOffset用于计算wqe位置的偏移，小于sqDepth
        u32 sqHead = pi % sqDepth;
        uint32_t sqPIMask = sqDepth - 1;
        if (sqOffset < sqDepth && (sqOffset + 1) >= sqDepth) {
            piDetourCount++;
        }
        // pi维护用于传入DB Send用于Rtsq 敲door bell，要求u16数据结构并且自然增长
        pi = pi + 1;

        // 写wqe到va
        u8 *va = reinterpret_cast<u8 *>(sqBaseAddr + (sqOffset & sqPIMask) * wqeSize);
        auto ret = memcpy_s(va, wqeSize, wqe, wqeSize);
        if (UNLIKELY(ret != 0)) {
            THROW<InternalException>(StringFormat("[NdaRdmaConnection::%s] memcpy_s failed, ret = %d", __func__, ret));
        }

        HCCL_INFO("[NdaRdmaConnection::%s] end, pi[%u], ci[%u]", __func__, pi, ci);
        return HCCL_SUCCESS;
    }

protected:
    u16  pi{0};
    u16  ci{0}; 
    u32  piDetourCount{0};
    u32  ciDetourCount{0};

    SqContext *sqContext_;
    CqContext *cqContext_;

    virtual int BuildWriteWqe(const RmaBufSliceLite &loc, const RmtRmaBufSliceLite &rmt, WqeEntry *wqe_buf) = 0;
    virtual int BuildWriteReduceWqe(const RmaBufSliceLite &loc, const RmtRmaBufSliceLite &rmt, WqeEntry *wqe_buf) = 0;
    virtual int BuildWriteWithNotifyWqe(const RmaBufSliceLite &loc, const RmtRmaBufSliceLite &rmt, WqeEntry *wqe_buf) = 0;
    virtual int BuildWriteReduceWithNotifyWqe(const RmaBufSliceLite &loc, const RmtRmaBufSliceLite &rmt, WqeEntry *wqe_buf) = 0;

    virtual int BuildOneWqe(const RmaBufSliceLite &loc, const RmtRmaBufSliceLite &rmt, WqeEntry *wqe_buf, uint32_t opCode) = 0;
    virtual int BuildDoorbell(u64 &dbValue) = 0;
};

}
#endif  // NDA_RDMA_BASE_VENDOR_OPS_H
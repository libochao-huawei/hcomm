
#ifndef NDA_RDMA_BASE_VENDOR_OPS_H
#define NDA_RDMA_BASE_VENDOR_OPS_H

#include "hccp_common.h"
#include "hccl_common.h"
#include "hccp_nda.h"

// 厂商共用的wqe创建入参
struct WqeDesc {
    uint32_t opCode = 0;

    const RmaBufSliceLite           *loc = nullptr;
    const RmtRmaBufSliceLite        *rmt = nullptr;

    // Reduce Releated
    HcommDataType dataType;
    HcommReduceOp reduceOp;

    // Notify Related
    uint32_t NotifyId           = 0;
    uint32_t timeout            = 0;
};

// 厂商共用的Cqe接收结构
struct CqeResult {
    uint32_t    immData = 0;
    int         status = 0;     // zero = success, non-zero = error.
}

namespace hcomm {

// PI、CI的入队出队在基类中解决，不知可否这样实现
class NdaBaseVendorOps {
public:
    NdaBaseVendorOps(SqContext *sqContext, CqContext *cqContext): sqContext_(sqContext), cqContext_(cqContext) {}
    ~NdaBaseVendorOps() {}

    int Write(const RmaBufSliceLite &loc, const RmtRmaBufSliceLite &rmt)
    {
        WqeDesc desc{};
        // Write Need Params
        desc.opCode = static_cast<uint32_t>(HCOMM_OP_TYPE::WRITE);
        desc.loc = loc;
        desc.rmt = rmt;

        return PostWqe(desc);
    }

    int WriteWithNotify(const RmaBufSliceLite &loc, const RmtRmaBufSliceLite &rmt, const uint32_t notifyId)
    {
        WqeDesc desc{};
        // WriteWithNotify Need Params
        desc.opCode = static_cast<uint32_t>(HCOMM_OP_TYPE::WRITE_WITH_NOTIFY);
        desc.loc = loc;
        desc.rmt = rmt;
        desc.NotifyId = notifyId;

        return PostWqe(desc);
    }

    int NotifyWait(const uint32_t notifyId, const uint32_t timeout)
    {
        // TODO 补充rqe
        PostRecv();

        auto startTime = std::chrono::steady_clock::now();
        auto waitTime = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::milliseconds(timeout));
        while (true) {
            HCCL_INFO("[NdaBaseVendorOps::NotifyWait] start to poll cq");

            CqeResult result{};
            int ret = PollCq(&result);
            if (ret < 0) {
                HCCL_ERROR("[NdaBaseVendorOps][%s] PollCq internal error, ret = %d", __func__, ret);
            }

            HCCL_INFO("[NdaBaseVendorOps::%s] polled 1 CQE, imm_data = %u", __func__, result.immData);
            if (result.status != 0) {
                HCCL_ERROR("[NdaBaseVendorOps][%s] CQE error, status = %d", __func__, result.status);
                return HCCL_E_NETWORK;
            }

            if (result.immData == notifyId) {
                HCCL_INFO("[NdaBaseVendorOps::%s] notify matched, notifyid = %u", __func__, notifyId);
                return HCCL_SUCCESS;
            }

            if ((std::chrono::steady_clock::now() - startTime) >= waitTime) {
                HCCL_ERROR("[NdaBaseVendorOps][%s] call PollCq timeout.", __func__);
                return HCCL_E_TIMEOUT;
            }
        }
        return HCCL_SUCCESS;
    }

protected:
    u16  pi{0};
    u16  ci{0}; 
    u32  piDetourCount{0};
    u32  ciDetourCount{0};

    SqContext *sqContext_;
    CqContext *cqContext_;
    // TODO Rq存在否？控制面好像没这一层
    RqContext *rqContext_;

    virtual int Write(const RmaBufSliceLite &loc, const RmtRmaBufSliceLite &rmt) = 0;
    virtual int Notify(const RmaBufSliceLite &loc, const RmtRmaBufSliceLite &rmt) = 0;
    virtual int WriteReduce(const RmaBufSliceLite &loc, const RmtRmaBufSliceLite &rmt) = 0;
    virtual int WriteWithNotify(const RmaBufSliceLite &loc, const RmtRmaBufSliceLite &rmt) = 0;
    virtual int WriteReduceWithNotify(const RmaBufSliceLite &loc, const RmtRmaBufSliceLite &rmt) = 0;
    virtual int WriteReduceWithNotify(const RmaBufSliceLite &loc, const RmtRmaBufSliceLite &rmt) = 0;

    // 准备wqe(厂商实现) + 下发wqe(厂商实现)
    virtual int PostWqe(const WqeDesc &desc) = 0;
    // TODO 待实现
    virtual int PostRecv() = 0;
    virtual int PollCq(const uint32_t notifyId) = 0;

    // 准备Doorbell(厂商实现)
    virtual int BuildDoorbell(u64 &dbValue) = 0;

    // 搬运wqe(通用实现)
    int ProcessOneWqe(const void *wqe, uint32_t wqeSize, uint32_t opCode) const
    {
        HCCL_INFO("[NdaBaseVendorOps::%s] Memcpy wqe start, opCode[%d]", __func__, opCode);

        auto sqDepth = sqContext_.depth;
        auto sqBaseAddr = sqContext_.sqVa;

        // sqHead用于计算wqe位置的偏移，小于sqDepth
        u32 sqHead = pi % sqDepth;
        uint32_t sqPIMask = sqDepth - 1;
        if (sqHead < sqDepth && (sqHead + 1) >= sqDepth) {
            piDetourCount++;
        }
        // pi维护用于传入DB Send用于Rtsq 敲door bell，要求u16数据结构并且自然增长
        pi = pi + 1;

        // 写wqe到va
        u8 *va = reinterpret_cast<u8 *>(sqBaseAddr + (sqHead & sqPIMask) * wqeSize);
        auto ret = memcpy_s(va, wqeSize, wqe, wqeSize);
        if (UNLIKELY(ret != 0)) {
            THROW<InternalException>(StringFormat("[NdaBaseVendorOps::%s] memcpy_s failed, ret = %d", __func__, ret));
        }

        HCCL_INFO("[NdaBaseVendorOps::%s] Memcpy wqe end, pi[%u], ci[%u]", __func__, pi, ci);
        return HCCL_SUCCESS;
    }
};

}
#endif  // NDA_RDMA_BASE_VENDOR_OPS_H
#ifndef NDA_RDMA_BASE_VENDOR_OPS_H
#define NDA_RDMA_BASE_VENDOR_OPS_H

#include "hccl/base.h"
#include "enum_factory.h"
#include "hccp_common.h"
#include "hccl_common.h"
#include "hccp_nda.h"

// 上层只分发操作类型，具体opCode在子类指定
MAKE_ENUM(NdaOpType,
        WRITE = 0,
        NOTIFY,
        WRITE_WITH_NOTIFY,
        WRITE_REDUCE,
        WRITE_REDUCE_WITH_NOTIFY,
        )

// 厂商共用的wqe创建入参
struct WqeDesc {
    NdaOpType opCode = NdaOpType::WRITE;

    const RmaBufSliceLite           *loc        = nullptr;
    const RmtRmaBufSliceLite        *rmt        = nullptr;
    const RmaBufSliceLite           *locNotify  = nullptr;
    const RmtRmaBufSliceLite        *notify     = nullptr;

    // Reduce Releated
    HcommDataType dataType;
    HcommReduceOp reduceOp;
};

namespace hcomm {

// PI、CI的入队出队在基类中解决，不知可否这样实现
class NdaBaseVendorOps {
public:
    NdaBaseVendorOps(SqContext *sqContext, CqContext *cqContext): sqContext_(sqContext), cqContext_(cqContext) {}
    ~NdaBaseVendorOps() {}

    int AddWqeList(NdaOpType descType, 
        const RmaBufSliceLite &loc, const RmtRmaBufSliceLite &rmt, 
        const RmaBufSliceLite &locNotify, const RmtRmaBufSliceLite &notify, std::vector<WqeDesc> &descList)
    {
        WqeDesc descTmp{};

        switch (descType) {
            case NdaOpType::WRITE:
                descTmp.opCode         = NdaOpType::WRITE;
                descTmp.loc            = &loc;
                descTmp.rmt            = &rmt;
            case NdaOpType::NOTIFY:
                descTmp.opCode         = NdaOpType::NOTIFY;
                descTmp.locNotify      = &locNotify;
                descTmp.notify         = &notify;
            default:
                HCCL_ERROR("error wqeType[%d]", descType);
                return HCCL_E_INTERNAL;
        }

        descList.push_back(descTmp);
        return HCCL_SUCCESS;
    }

    int Write(const RmaBufSliceLite &loc, const RmtRmaBufSliceLite &rmt)
    {
        std::vector<WqeDesc> descList = {};
        // Add Write
        AddWqeList(NdaOpType::WRITE, loc, rmt, nullptr, nullptr, descList);

        return PostWqeList(descList);
    }

    int NotifyRecord(const RmaBufSliceLite &locNotify, const RmtRmaBufSliceLite &notify)
    {
        std::vector<WqeDesc> descList = {};
        // Add Notify
        AddWqeList(NdaOpType::NOTIFY, nullptr, nullptr, locNotify, notify, descList);

        return PostWqeList(descList);
    }

    int WriteWithNotify(
        const RmaBufSliceLite &loc, const RmtRmaBufSliceLite &rmt, 
        const RmaBufSliceLite &locNotify, const RmtRmaBufSliceLite &notify)
    {
        std::vector<WqeDesc> descList = {};
        // Add Write
        AddWqeList(NdaOpType::WRITE, loc, rmt, nullptr, nullptr, descList);

        // Add Notify
        AddWqeList(NdaOpType::NOTIFY, nullptr, nullptr, locNotify, notify, descList);

        return PostWqeList(descList);
    }

    int WriteReduce(const RmaBufSliceLite &loc, const RmtRmaBufSliceLite &rmt, DataType dataType, ReduceOp reduceOp)
    {
        return HCCL_SUCCESS;
    }

    int WriteReduceWithNotify(
        const RmaBufSliceLite &loc, const RmtRmaBufSliceLite &rmt, DataType dataType, ReduceOp reduceOp, const uint32_t remoteNotifyId)
    {
        return HCCL_SUCCESS;
    }

protected:
    u16  pi{0};
    u16  ci{0}; 
    u32  piDetourCount{0};
    u32  ciDetourCount{0};

    SqContext *sqContext_;
    CqContext *cqContext_;

    // 根据descList准备wqe(厂商实现) + 下发wqe(厂商实现)
    virtual int PostWqeList(std::vector<WqeDesc> &descList) = 0;

    // 准备Doorbell(厂商实现)
    virtual int BuildDoorbell(u64 &dbAddr, u64 &dbValue) = 0;

    // 搬运wqe(通用实现)
    int ProcessOneWqe(const void *wqe, uint32_t wqeSize, uint32_t opCode)
    {
        HCCL_INFO("[NdaBaseVendorOps::%s] Memcpy wqe start, opCode[%d]", __func__, opCode);

        auto sqDepth = sqContext_->depth;
        auto sqBaseAddr = sqContext_->sqVa;

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
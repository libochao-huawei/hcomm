#ifndef RDMA_BASE_VENDOR_OPS_H
#define RDMA_BASE_VENDOR_OPS_H

#include "rma_conn_lite.h"

namespace Hccl {

// 上层只分发操作类型，具体opCode在子类指定
enum RdmaOpType {
    WRITE = 0,
    NOTIFY,
    WRITE_WITH_NOTIFY,
    WRITE_REDUCE,
    WRITE_REDUCE_WITH_NOTIFY,
};

// 厂商共用的wqe创建入参
struct WqeDesc {
    RdmaOpType opCode = RdmaOpType::WRITE;

    const RmaBufSliceLite           *loc        = nullptr;
    const RmtRmaBufSliceLite        *rmt        = nullptr;
    const RmaBufSliceLite           *locNotify  = nullptr;
    const RmtRmaBufSliceLite        *notify     = nullptr;

    // TODO Hccl与Hcomm的ReduceType不一样？
    // HcommDataType dataType;
    // HcommReduceOp reduceOp;

    // HcclType
    HcclDataType dataType;
    HcclReduceOp reduceOp;
};

class RdmaBaseOps {
public:
    RdmaBaseOps(RdmaSqContextLite *sqContext, RdmaCqContextLite *cqContext): sqContext_(sqContext), cqContext_(cqContext) {}
    virtual ~RdmaBaseOps() {}

    int Write(const RmaBufSliceLite &loc, const RmtRmaBufSliceLite &rmt)
    {
        std::vector<WqeDesc> descList = {};
        // Add Write
        AddWqeList(RdmaOpType::WRITE, loc, rmt, descList);

        return PostWqeList(descList);
    }

    int NotifyRecord(const RmaBufSliceLite &locNotify, const RmtRmaBufSliceLite &notify)
    {
        std::vector<WqeDesc> descList = {};
        // Add Notify
        AddWqeList(RdmaOpType::NOTIFY, locNotify, notify, descList);

        return PostWqeList(descList);
    }

    int WriteWithNotify(
        const RmaBufSliceLite &loc, const RmtRmaBufSliceLite &rmt, 
        const RmaBufSliceLite &locNotify, const RmtRmaBufSliceLite &notify)
    {
        std::vector<WqeDesc> descList = {};
        // Add Write
        AddWqeList(RdmaOpType::WRITE, loc, rmt, descList);

        // Add Notify
        AddWqeList(RdmaOpType::NOTIFY, locNotify, notify, descList);

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

    // 准备Doorbell(厂商实现)
    virtual int BuildDoorbell(u64 &dbAddr, u64 &dbValue) = 0;

protected:
    // 软件侧只维护Sq PI，Sq CI由硬件维护
    u32  sqHead_{0};
    u32  sqTail_{0};

    RdmaSqContextLite *sqContext_;
    RdmaCqContextLite *cqContext_;

    int AddWqeList(RdmaOpType descType, 
        const RmaBufSliceLite &loc, const RmtRmaBufSliceLite &rmt, std::vector<WqeDesc> &descList)
    {
        WqeDesc descTmp{};

        switch (descType) {
            case RdmaOpType::WRITE:
                descTmp.opCode         = RdmaOpType::WRITE;
                descTmp.loc            = &loc;
                descTmp.rmt            = &rmt;
                break;
            case RdmaOpType::NOTIFY:
                descTmp.opCode         = RdmaOpType::NOTIFY;
                descTmp.locNotify      = &loc;
                descTmp.notify         = &rmt;
                break;
            default:
                HCCL_ERROR("error wqeType[%d]", descType);
                return HCCL_E_INTERNAL;
        }

        descList.push_back(descTmp);
        return HCCL_SUCCESS;
    }

    // 根据descList准备wqe(厂商实现) + 下发wqe(厂商实现)
    virtual int PostWqeList(std::vector<WqeDesc> &descList) = 0;

    // 搬运wqe(通用实现)
    int ProcessOneWqe(const void *wqe, uint32_t wqeSize, uint32_t opCode)
    {
        HCCL_INFO("[RdmaBaseOps::%s] Memcpy wqe start, opCode[%d]", __func__, opCode);

        // pi维护用于传入DB Send用于Rtsq 敲door bell
        sqHead_ = sqHead_ + 1;

        // 写wqe到va
        auto sqDepth = sqContext_->depth;
        uint32_t sqPIMask = sqDepth - 1;
        u8 *va = reinterpret_cast<u8 *>(sqContext_->sqVa + ((sqHead_ % sqDepth) & sqPIMask) * wqeSize);
        auto ret = memcpy_s(va, wqeSize, wqe, wqeSize);
        if (UNLIKELY(ret != 0)) {
            THROW<InternalException>(StringFormat("[RdmaBaseOps::%s] memcpy_s failed, ret = %d", __func__, ret));
        }

        HCCL_INFO("[RdmaBaseOps::%s] Memcpy wqe end, pi[%u]", __func__, sqHead_);
        return HCCL_SUCCESS;
    }
};

}
#endif  // RDMA_BASE_VENDOR_OPS_H
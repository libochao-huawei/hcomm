#include "nda_rdma_connection.h"
#include "hccp.h"

HcclResult NdaRdmaConnection::BuildSqContext(SqContext *context)
{
    struct QpAttr localQpAttr;
    s32 ret = RaGetQpAttr(qpHandle_, &localQpAttr);
    if (ret != 0) {
        return HCCL_E_ROCE_CONNECT;
    }

    context->type = 1;  // 0-jfs 1-rdma
    context->RdmaSqContext.qpn = localQpAttr.qpn;
    context->RdmaSqContext.sqVa = qpInfo_.sqInfo.qBuf.base;


    return HCCL_SUCCESS;
}

HcclResult NdaRdmaConnection::BuildCqContext(CqContext *context)
{
    return HCCL_SUCCESS;
}

void NdaRdmaConnection::GetVendorOps()
{
    if (wqeBuilder_ != nullptr) {
        return;
    }
    switch (directFlag_) {
        case DIRECT_FLAG_PCIE: {
            vendorOps_ = std::make_unique<NdaPcieOps>(sqContext_, cqContext_);
            break;
        }
        case DIRECT_FLAG_UB: {
            vendorOps_ = std::make_unique<NdaUbOps>(sqContext_, cqContext_);
            break;
        }
    }
}

void NdaRdmaConnection::Write(
    const RmaBufSliceLite &loc, const RmtRmaBufSliceLite &rmt, const StreamLite &stream, u64 &dbValue)
{
    HCCL_INFO("[NdaRdmaConnection::%s] start, loc size = %u", __func__, loc.GetSize());
    int ret;
    // 和UDMA不同，RDMA似乎不需要考虑Slice切分
    WqeEntry wqe{};
    // 构造wqe, vendorOps_用来屏蔽厂商区别
    ret = vendorOps_.BuildWriteWqe(loc, rmt, &wqe);
    // 下发wqe
    ret = vendorOps_.ProcessOneWqe(&wqe);
    // 构造Doorbell
    ret = vendorOps_.BuildDoorbell(dbValue);

    HCCL_INFO("[NdaRdmaConnection::%s] end, out.dbValue = %llu", __func__, dbValue);
}
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
    if (vendorOps_ != nullptr) {
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

void NdaRdmaConnection::Write(const RmaBufSliceLite &loc, const RmtRmaBufSliceLite &rmt, u64 &dbVa, u64 &dbValue)
{
    HCCL_INFO("[NdaRdmaConnection::%s] Write start, loc size = %u", __func__, loc.GetSize());
    int ret;

    // Post wqe, vendorOps_用来屏蔽厂商区别; 和UDMA不同，RDMA似乎不需要考虑Slice切分
    ret = vendorOps_->Write(loc, rmt);

    // 构造Doorbell并返回
    ret = vendorOps_->BuildDoorbell(dbVa, dbValue);

    // TODO 增加错误处理

    HCCL_INFO("[NdaRdmaConnection::%s] Write end, out.dbValue = %llu", __func__, dbValue);
}

void NdaRdmaConnection::WriteWithNotify(
    const RmaBufSliceLite &loc, const RmtRmaBufSliceLite &rmt, uint32_t remoteNotifyId, u64 &dbVa, u64 &dbValue)
{
    HCCL_INFO("[NdaRdmaConnection::%s] WriteWithNotify start, loc size = %u, remoteNotifyId = %u", __func__, loc.GetSize(), remoteNotifyId);
    int ret;

    // Post wqe, vendorOps_用来屏蔽厂商区别; 和UDMA不同，RDMA似乎不需要考虑Slice切分
    ret = vendorOps_->WriteWithNotify(loc, rmt);

    // 构造Doorbell并返回
    ret = vendorOps_->BuildDoorbell(dbVa, dbValue);

    HCCL_INFO("[NdaRdmaConnection::%s] WriteWithNotify end, out.dbValue = %llu", __func__, dbValue);
}

void NdaRdmaConnection::NotifyWait(uint32_t localNotifyId, uint32_t timeout)
{
    HCCL_INFO("[NdaRdmaConnection::%s] NotifyWait start, localNotifyId = %u, timeout = %u", __func__, localNotifyId, timeout);
    int ret;

    // poll_cq, vendorOps_用来屏蔽厂商区别
    ret = vendorOps_->NotifyWait(localNotifyId, timeout);

    HCCL_INFO("[NdaRdmaConnection::%s] NotifyWait end, localNotifyId = %u, timeout = %u", __func__, localNotifyId, timeout);
}
#include "nda_rdma_connection.h"
#include "hccp.h"

void NdaRdmaConnection::GetVendorOps()
{
    if (vendorOps_ != nullptr) {
        return;
    }
    switch (directFlag_) {
        case DIRECT_FLAG_PCIE: {
            vendorOps_ = std::make_unique<NdaXscdvOps>(sqContext_, cqContext_);
            break;
        }
        case DIRECT_FLAG_UB: {
            vendorOps_ = std::make_unique<Nda1825Ops>(sqContext_, cqContext_);
            break;
        }
    }
}

void NdaRdmaConnection::Write(const RmaBufSliceLite &loc, const RmtRmaBufSliceLite &rmt, u64 &dbAddr, u64 &dbValue)
{
    HCCL_INFO("[NdaRdmaConnection::%s] Write start, loc size = %u", __func__, loc.GetSize());
    int ret;

    // Post wqe, vendorOps_用来屏蔽厂商区别; 和UDMA不同，RDMA似乎不需要考虑Slice切分
    ret = vendorOps_->Write(loc, rmt);

    // 构造Doorbell并返回
    ret = vendorOps_->BuildDoorbell(dbAddr, dbValue);

    // TODO 增加错误处理
    HCCL_INFO("[NdaRdmaConnection::%s] Write end, out.dbValue = %llu", __func__, dbValue);
}

void NdaRdmaConnection::WriteWithNotify(
    const RmaBufSliceLite &loc, const RmtRmaBufSliceLite &rmt,
    const RmaBufSliceLite &locNotify, const RmtRmaBufSliceLite &notify, u64 &dbAddr, u64 &dbValue)
{
    HCCL_INFO("[NdaRdmaConnection::%s] WriteWithNotify start, loc size = %u, remoteNotifyId = %u", __func__, loc.GetSize(), remoteNotifyId);
    int ret;

    // Post wqe, vendorOps_用来屏蔽厂商区别; 和UDMA不同，RDMA似乎不需要考虑Slice切分
    ret = vendorOps_->WriteWithNotify(loc, rmt, locNotify, notify);

    // 构造Doorbell并返回
    ret = vendorOps_->BuildDoorbell(dbAddr, dbValue);

    HCCL_INFO("[NdaRdmaConnection::%s] WriteWithNotify end, out.dbValue = %llu", __func__, dbValue);
}
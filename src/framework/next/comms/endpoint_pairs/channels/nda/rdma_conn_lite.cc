#include "nda_rdma_connection.h"
#include "hccp.h"

void RdmaConnLite::GetVendorOps()
{
    if (vendorOps_ != nullptr) {
        return;
    }
    switch (directFlag_) {
        case DIRECT_FLAG_PCIE: {
            vendorOps_ = std::make_unique<NdaXscdvOps>(&sqContext, &cqContext);
            break;
        }
        case DIRECT_FLAG_UB: {
            vendorOps_ = std::make_unique<Nda1825Ops>(&sqContext, &cqContext);
            break;
        }
    }
}

void RdmaConnLite::Write(const RmaBufSliceLite &loc, const RmtRmaBufSliceLite &rmt, u64 &dbAddr, u64 &dbValue)
{
    HCCL_INFO("[RdmaConnLite::%s] Write start, loc size = %u", __func__, loc.GetSize());

    // Post wqe, vendorOps_用来屏蔽厂商区别; 和UDMA不同，RDMA似乎不需要考虑Slice切分
    vendorOps_->Write(loc, rmt);

    // 构造Doorbell并返回
    vendorOps_->BuildDoorbell(dbAddr, dbValue);

    // TODO 增加错误处理
    HCCL_INFO("[RdmaConnLite::%s] end, dbAddr = %llu, dbValue = %llu, conn[%s]", __func__, dbAddr, dbValue, Describe().c_str());
}

void RdmaConnLite::WriteWithNotify(
    const RmaBufSliceLite &loc, const RmtRmaBufSliceLite &rmt,
    const RmaBufSliceLite &locNotify, const RmtRmaBufSliceLite &notify, u64 &dbAddr, u64 &dbValue)
{
    HCCL_INFO("[RdmaConnLite::%s] start", __func__);

    // Post wqe, vendorOps_用来屏蔽厂商区别; 和UDMA不同，RDMA似乎不需要考虑Slice切分
    vendorOps_->WriteWithNotify(loc, rmt, locNotify, notify);

    // 构造Doorbell并返回
    vendorOps_->BuildDoorbell(dbAddr, dbValue);

    // TODO 增加错误处理
    HCCL_INFO("[RdmaConnLite::%s] end, dbAddr = %llu, dbValue = %llu, conn[%s]", __func__, dbAddr, dbValue, Describe().c_str());
}
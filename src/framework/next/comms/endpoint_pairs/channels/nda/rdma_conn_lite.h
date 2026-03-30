#ifndef NDA_RDMA_CONNECTION_H
#define NDA_RDMA_CONNECTION_H

#include "hccp_common.h"
#include "hccl_common.h"
#include "hccp_nda.h"

#include "../../../../../../legacy/unified_platform/resource/socket/socket.h"

#include "nda_vendor_1825_ops.h"

namespace hcomm {

class RdmaConnLite {
public:
    // 数据面
    // TODO 其他数据面接口
    void Write(const RmaBufSliceLite &loc, const RmtRmaBufSliceLite &rmt, u64 &dbAddr, u64 &dbValue);
    void WriteWithNotify(
        const RmaBufSliceLite &loc, const RmtRmaBufSliceLite &rmt,
        const RmaBufSliceLite &locNotify, const RmtRmaBufSliceLite &notify, u64 &dbAddr, u64 &dbValue);

private:
    // 工厂模式，负责具体的厂商ops创建
    std::unique_ptr<NdaBaseVendorOps> vendorOps_ = nullptr;
};

}   // namespace hcomm


#endif  // NDA_RDMA_CONNECTION_H
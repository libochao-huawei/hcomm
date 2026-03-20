#ifndef NDA_RDMA_CONNECTION_H
#define NDA_RDMA_CONNECTION_H

#include "hccp_common.h"
#include "hccl_common.h"
#include "hccp_nda.h"

#include "../../../../../../legacy/unified_platform/resource/socket/socket.h"

#include "nda_vendor_1825_ops.h"
#include "nda_vendor_xscdv_ops.h"

namespace hcomm {
class NdaRdmaConnection {
public:
    HcclResult BuildSqContext(SqContext *context);
    HcclResult BuildCqContext(CqContext *context);

    // 数据面
    void Write(const RmaBufSliceLite &loc, const RmtRmaBufSliceLite &rmt, u64 &dbAddr, u64 &dbValue);
    void WriteWithNotify(const RmaBufSliceLite &loc, const RmtRmaBufSliceLite &rmt, uint32_t remoteNotifyId, u64 &dbAddr, u64 &dbValue);

    void NotifyWait(uint32_t localNotifyId, uint32_t timeout);

private:
    u16  pi{0};
    u16  ci{0}; 
    u32  piDetourCount{0};
    u32  ciDetourCount{0};

    SqContext *sqContext_;
    CqContext *cqContext_;

    Hccl::RdmaHandle     rdmaHandle_{nullptr};
    uint32_t            directFlag_{0};
    uint32_t            dmaMode_{0};

    Hccl::QpHandle      qpHandle_;
    struct NdaQpInfo    qpInfo_;
    Hccl::CpHandle      cqHandle_;
    struct NdaCqInfo    cqInfo_;

    // 工厂模式，负责具体的厂商ops创建
    std::unique_ptr<NdaBaseVendorOps> vendorOps_ = nullptr;
};

}   // namespace hcomm


#endif  // NDA_RDMA_CONNECTION_H
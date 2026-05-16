#ifndef RDMA_VENDOR_XSCDV_OPS_H
#define RDMA_VENDOR_XSCDV_OPS_H

#include "rdma_vendor_base_ops.h"
// #include "infiniband/xscdv.h"

namespace Hccl {

class RdmaXscdvOps : public RdmaBaseOps {
public:
    RdmaXscdvOps(RdmaSqContextLite *sqContext, RdmaCqContextLite *cqContext)
        : RdmaBaseOps(sqContext, cqContext) {}

    ~RdmaXscdvOps() override {}

protected:
    // 创建xscdv对应Wqe, 并下发
    int PostWqeList(std::vector<WqeDesc> &descList)
    {
        return HCCL_SUCCESS;
    }

    int BuildDoorbell(u64 &dbAddr, u64 &dbValue)
    {
        return HCCL_SUCCESS;
    }

private:

};

}   // namespace Hccl


#endif   // RDMA_VENDOR_XSCDV_OPS_H
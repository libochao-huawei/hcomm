#ifndef NDA_RDMA_XSCDV_OPS_H
#define NDA_RDMA_XSCDV_OPS_H

#include "nda_vendor_base_ops.h"
// TODO 云脉头文件参与编译
// #include "infiniband/xscdv.h"

namespace Hccl {

class NdaXscdvOps : public NdaBaseVendorOps {
public:
    NdaXscdvOps(RdmaSqContextLite *sqContext, RdmaCqContextLite *cqContext)
        : NdaBaseVendorOps(sqContext, cqContext) {}

    ~NdaXscdvOps() override {}

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


#endif   // NDA_RDMA_XSCDV_OPS_H
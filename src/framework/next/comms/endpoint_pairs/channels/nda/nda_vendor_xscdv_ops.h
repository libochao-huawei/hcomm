#ifndef NDA_RDMA_XSCDV_OPS_H
#define NDA_RDMA_XSCDV_OPS_H

#include "nda_vendor_base_ops.h"
#include "infiniband/xscdv.h"

namespace hcomm {

class NdaXscdvOps : public NdaBaseVendorOps {
public:

protected:
    // 创建xscdv对应Wqe, 并下发
    int PostWqe(const WqeDesc &desc)
    {
        return HCCL_SUCCESS;
    }

    int BuildDoorbell(u64 &dbVa, u64 &dbValue)
    {
        return HCCL_SUCCESS;
    }

private:
    int BuildOneWqe()
    {
        return HCCL_SUCCESS;
    }

    int BuildWrite()
    {
        return HCCL_SUCCESS;
    }

};

}   // namespace hcomm


#endif   // NDA_RDMA_XSCDV_OPS_H
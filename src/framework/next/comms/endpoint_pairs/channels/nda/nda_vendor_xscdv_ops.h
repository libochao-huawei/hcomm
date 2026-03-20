#ifndef NDA_RDMA_XSCDV_OPS_H
#define NDA_RDMA_XSCDV_OPS_H

#include "nda_vendor_base_ops.h"
#include "infiniband/xscdv.h"

namespace hcomm {

class NdaXscdvOps : public NdaBaseVendorOps {
public:

protected:
    // 创建xscdv对应Wqe, 并下发
    int PostWqeList(std::vector<WqeDesc> &descList)
    {
        return HCCL_SUCCESS;
    }

    int int BuildDoorbell(u64 &dbAddr, u64 &dbValue)
    {
        return HCCL_SUCCESS;
    }

private:

};

}   // namespace hcomm


#endif   // NDA_RDMA_XSCDV_OPS_H
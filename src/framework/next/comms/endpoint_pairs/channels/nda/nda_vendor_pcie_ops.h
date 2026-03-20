#ifndef NDA_RDMA_PCIE_OPS_H
#define NDA_RDMA_PCIE_OPS_H

#include "nda_vendor_base_ops.h"
#include "infiniband/xscdv.h"

namespace hcomm {

class NdaPcieOps : public NdaBaseVendorOps {
public:

protected:
    // 创建xscdv对应Wqe, 并下发
    int PostWqe(const WqeDesc &desc)
    {
        return HCCL_SUCCESS;
    }

    int PostRecv()
    {
        return HCCL_SUCCESS;
    }

    int PollCq(CqeResult &result)
    {
        return 0;
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


#endif   // NDA_RDMA_PCIE_OPS_H
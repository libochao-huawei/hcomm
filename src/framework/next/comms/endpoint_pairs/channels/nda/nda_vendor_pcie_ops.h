#ifndef NDA_RDMA_PCIE_OPS_H
#define NDA_RDMA_PCIE_OPS_H

#include "nda_vendor_base_ops.h"
#include "infiniband/xscdv.h"

namespace hcomm {

class NdaPcieOps : public NdaBaseVendorOps {
public:

protected:
    int BuildOneWqe(const RmaBufSliceLite &loc, const RmtRmaBufSliceLite &rmt, WqeEntry *wqe_buf, uint32_t opCode)
    {
        HCCL_INFO("[NdaPcieOps::%s] start, loc size[%u]", __func__, loc.GetSize());

        HCCL_INFO("[NdaPcieOps::%s] Hcomm PCIe Build wqe OK", __func__);
        return HCCL_SUCCESS;
    }

    int BuildDoorbell(u64 &dbValue)
    {
        HCCL_INFO("[NdaPcieOps::%s] start BuildDoorbell", __func__);

        HCCL_INFO("[NdaPcieOps::%s] Hcomm PCIe Build Doorbell OK", __func__);
        return HCCL_SUCCESS;
    }

};

}   // namespace hcomm


#endif   // NDA_RDMA_PCIE_OPS_H
#ifndef NDA_RDMA_PCIE_OPS_H
#define NDA_RDMA_PCIE_OPS_H

#include "nda_vendor_base_ops.h"
#include "infiniband/xscdv.h"

namespace hcomm {

class NdaPcieOps : public NdaBaseVendorOps {
public:

protected:
    int Write(const RmaBufSliceLite &loc, const RmtRmaBufSliceLite &rmt)
    {
        HCCL_INFO("[NdaPcieOps::%s] start, loc size[%u]", __func__, loc.GetSize());

        uint16_t wqe_ds_idx = wqe_idx * 8;
        struct xscdv_wqe_ctrl_seg *ctrl_seg = reinterpret_cast<ibgda_ctrl_seg_t*>(out_wqes[0]); // wqe起始
        struct xscdv_diamond_data_seg *raddr_seg = reinterpret_cast<xscdv_diamond_data_seg*>(reinterpret_cast<uintptr_t>(ctrl_seg) + sizeof(*ctrl_seg));
        struct xscdv_diamond_data_seg *ldata_seg = reinterpret_cast<xscdv_diamond_data_seg*>(reinterpret_cast<uintptr_t>(raddr_seg) + sizeof(*raddr_seg));

        BuildOneWqe(loc, rmt, (void *)(ctrl_seg), static_cast<uint32_t>(HCOMM_OP_TYPE::WRITE));

        ProcessOneWqe(ctrl_seg, sizeof(xscdv_wqe_ctrl_seg), static_cast<uint32_t>(HCOMM_OP_TYPE::WRITE));

        HCCL_INFO("[NdaPcieOps::%s] Hcomm PCIe Build wqe OK", __func__);
        return HCCL_SUCCESS;
    }

    int BuildOneWqe(const RmaBufSliceLite &loc, const RmtRmaBufSliceLite &rmt, void *wqe_buf, uint32_t opCode)
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
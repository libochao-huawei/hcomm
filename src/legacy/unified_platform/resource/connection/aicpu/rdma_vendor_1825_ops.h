#ifndef RDMA_VENDOR_1825_OPS_H
#define RDMA_VENDOR_1825_OPS_H

#include "rdma_vendor_base_ops.h"

namespace Hccl {

typedef struct {
    // Control Segment
    union {
        struct {
            uint32_t o : 1;     // Owner
            uint32_t ctrlSl : 2;
            uint32_t csl : 2;
            uint32_t difSl : 3;
            uint32_t cr : 1;
            uint32_t df : 1;
            uint32_t va : 1;
            uint32_t tsl : 5;
            uint32_t cf : 1;
            uint32_t wf : 1;
            uint32_t rsvd0 : 4;
            uint32_t rrvSl : 2;
            uint32_t bdsLen : 8;
        } bs;
        uint32_t value;
    } dw0;
    union {
        struct {
            uint32_t cl : 4;
            uint32_t rsvd1 : 8;
            uint32_t maskPi : 20;
        } bs;
        uint32_t value;
    } dw1;
} RoceWqeCtrlSeg;

typedef struct {
    // Task Segment
    union {
        struct {
            uint32_t se : 1;
            uint32_t f : 1;
            uint32_t c : 1;
            uint32_t opType : 5;
            uint32_t so : 1;
            uint32_t rsvd0 : 3;
            uint32_t dif : 1;
            uint32_t ext : 1;
            uint32_t xrcSrqn : 18;
        } bs;
        uint32_t value;
    } dw0;
} RoceWqeTaskSeg;

typedef struct {
    uint32_t bufAddrHigh32;
    uint32_t bufAddrLow32;
    uint32_t rLen;
    uint32_t leKey;
} RoceWqeDataSeg;

typedef struct {
    RoceWqeCtrlSeg ctrl;
    uint64_t doorbell;
    RoceWqeTaskSeg task;
    uint32_t dataLen;
    uint32_t immeData;
    uint32_t firstLast  : 1;
    uint32_t nxtEthHdr  : 7;
    uint32_t cmdLen     : 8;
    uint32_t rsvd0      : 8;
    uint32_t lastExtLen : 8;
    uint32_t vaHigh32;
    uint32_t vaLow32;
    uint32_t rKey;
    uint32_t rsvd1;
    RoceWqeDataSeg data;
} RoceWqeEntry;

typedef struct {
    uint32_t cqe0;
    uint32_t cqe1;
    uint32_t cqe2;
    uint32_t cqe3;
    uint32_t cqe4;
    uint32_t cqe5;
    uint32_t cqe6;
    uint32_t cqe7;
} RoceCqeEntry;

typedef struct {
    union {
        struct {
            uint32_t pi         : 8;
            uint32_t resv       : 8;
            uint32_t xrcvld     : 1;
            uint32_t vxlan      : 1;
            uint32_t mtuShift   : 3;
            uint32_t sgidIndex  : 7;
            uint32_t queueId    : 4;
            uint32_t qpn        : 20;
            uint32_t cntxSize   : 2;
            uint32_t n          : 1;
            uint32_t c          : 1;
            uint32_t cos        : 3;
            uint32_t type       : 5;
        } bs;
        uint64_t value;
    } dw0;
} RoceDbEntry;

enum class HCOMM_OP_TYPE : uint32_t {
    WRITE = 4U,
    READ = 8U
};

// Wqe ctrl seg
#define HCOMM_WQE_BDSL_OFFSET 0
#define HCOMM_WQE_TSL_OFFSET 16
#define HCOMM_WQE_VA_OFFSET 21
#define HCOMM_WQE_CR_OFFSET 23
#define HCOMM_WQE_CTRLSL_OFFSET 29
#define HCOMM_WQE_CL_OFFSET 28
#define HCOMM_WQE_OWNER_OFFSET 31
#define HCOMM_WQE_OP_TYPE_OFFSET 24
#define HCOMM_WQE_C_OFFSET 29

// Doorbell related
constexpr uint32_t ROCE_SQ_DOORBELL_TYPE = 2;
constexpr uint32_t ROCE_INIT_SQ_DB_SGIT_IDX = 1;

class Rdma1825Ops : public RdmaBaseOps {
public:
    Rdma1825Ops(RdmaSqContextLite *sqContext, RdmaCqContextLite *cqContext)
        : RdmaBaseOps(sqContext, cqContext) {}

    ~Rdma1825Ops() override {}

    int BuildDoorbell(u64 &dbAddr, u64 &dbValue)
    {
        HCCL_INFO("[Rdma1825Ops::%s] start BuildDoorbell", __func__);

        RoceDbEntry dbEntry = {0};
        dbEntry.dw0.bs.type = ROCE_SQ_DOORBELL_TYPE;
        dbEntry.dw0.bs.c = 0;
        dbEntry.dw0.bs.n = 0;
        dbEntry.dw0.bs.cntxSize = 1;
        dbEntry.dw0.bs.qpn = sqContext_->qpn;                   // Doorbell QPN
        dbEntry.dw0.bs.mtuShift = 0;
        dbEntry.dw0.bs.resv = 0;
        dbEntry.dw0.bs.sgidIndex = ROCE_INIT_SQ_DB_SGIT_IDX;
        dbEntry.dw0.bs.cos = 0x7;
        dbEntry.dw0.bs.pi = sqHead_;                                // Doorbell Sq PI

        dbAddr  = sqContext_->dbVa + ((sqHead_ >> 8) & 0xff);
        dbValue = dbEntry.dw0.value;

        HCCL_INFO("[Rdma1825Ops::%s] Hcomm UB Build Doorbell OK", __func__);
        return HCCL_SUCCESS;
    }

protected:
    int BuildWriteWqe(const RmaBufSliceLite &loc, const RmtRmaBufSliceLite &rmt)
    {
        int ret = HCCL_SUCCESS;

        RoceWqeEntry wqe{};
        CHK_RET(FillCommonWqe(loc, rmt, &wqe, static_cast<uint32_t>(HCOMM_OP_TYPE::WRITE)));

        CHK_RET(CommitWqe(&wqe, sizeof(RoceWqeEntry)));
        return HCCL_SUCCESS;
    }

    int BuildNotifyWqe(const RmaBufSliceLite &locNotify, const RmtRmaBufSliceLite &notify)
    {
        int ret = HCCL_SUCCESS;

        RoceWqeEntry wqe{};
        CHK_RET(FillCommonWqe(locNotify, notify, &wqe, static_cast<uint32_t>(HCOMM_OP_TYPE::WRITE)));

        CHK_RET(CommitWqe(&wqe, sizeof(RoceWqeEntry)));
        return HCCL_SUCCESS;
    }

private:
    int FillCommonWqe(const RmaBufSliceLite &loc, const RmtRmaBufSliceLite &rmt, RoceWqeEntry *wqe, uint32_t opCode)
    {
        HCCL_INFO("[Rdma1825Ops::%s] BuildWrite start, loc size[%u]", __func__, loc.GetSize());

        uint8_t owner = (sqHead_ & (sqContext_->depth)) == 0 ? 0 : 1;

        // ----- Ctrl Seg 1 -----
        wqe->ctrl.dw0.value =
            htonl(owner << HCOMM_WQE_OWNER_OFFSET | 2U << HCOMM_WQE_CTRLSL_OFFSET | 1U << HCOMM_WQE_CR_OFFSET
                | 1U << HCOMM_WQE_VA_OFFSET | 4U << HCOMM_WQE_TSL_OFFSET | 2U << HCOMM_WQE_BDSL_OFFSET);
        wqe->ctrl.dw1.value = htonl(1U << HCOMM_WQE_CL_OFFSET);
        // ----- Doorbell -----
        wqe->doorbell = 0;
        // ----- Task Seg -----
        wqe->task.dw0.value = htonl(opCode << HCOMM_WQE_OP_TYPE_OFFSET | 1U << HCOMM_WQE_C_OFFSET);
        wqe->dataLen = htonl(loc.GetSize());
        wqe->immeData = 0;
        wqe->firstLast = 0;
        wqe->nxtEthHdr = 0;
        wqe->cmdLen = 0;
        wqe->rsvd0 = 0;
        wqe->lastExtLen = 0;
        wqe->vaHigh32 = htonl(((uint64_t)rmt.GetAddr() + rmt.GetSize()) >> 32);
        wqe->vaLow32 = htonl(((uint64_t)rmt.GetAddr() + rmt.GetSize()) & 0xffffffff);
        // rkey通过rmt传入
        uint32_t rKey = rmt.GetRkey();
        wqe->rKey = htonl(rKey);
        wqe->rsvd1 = 0;
        // ----- Data Seg -----
        wqe->data.bufAddrHigh32 = htonl((uint64_t)loc.GetAddr() >> 32);
        wqe->data.bufAddrLow32 = htonl((uint64_t)loc.GetAddr() & 0xffffffff);
        wqe->data.rLen = (uint32_t)loc.GetSize();
        // lkey通过loc传入
        uint32_t lKey = loc.GetLkey();
        wqe->data.leKey = htonl(1 << 31 | lKey);

        HCCL_INFO("[Rdma1825Ops::%s] Hcomm UB BuildWrite wqe OK", __func__);
        return HCCL_SUCCESS;
    }
};


}
#endif // RDMA_VENDOR_1825_OPS_H
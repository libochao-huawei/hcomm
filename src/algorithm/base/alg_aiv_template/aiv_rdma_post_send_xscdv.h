#ifndef AIV_RDMA_POST_SEND_XSCDV_H
#define AIV_RDMA_POST_SEND_XSCDV_H

#include "kernel_operator.h"

struct xscdv_wqe_ctrl_seg {
    uint8_t msg_opcode;         //详⻅xscdv_msg_type
    uint8_t with_immdt:1;       //1:带⽴即数，0：不带
    uint8_t csum_en:2;
    uint8_t ds_data_num:5;      //wqe中除去ctrl段后的ds个数
    uint16_t wqe_id;            //wqe ds index
    uint32_t msg_len;           //message len
    uint32_t opcode_data;       //immdiate data
    uint8_t se:1;               //SEND_SOLICITED enable
    uint8_t ce:1;               //SEND_SIGNALEED enable
    uint8_t in_line:1;          //SEND_INLINE enable
    uint8_t fence_mode:2;
    uint8_t mask:2;
    uint32_t rsv:25;
};

struct xscdv_diamond_data_seg {
    uint32_t length;        //seg length
    uint32_t key;           //mkey
    uint64_t addr;          //mbuf virtul addr
};

union xscdv_diamond_next_send_doorbell {
    struct {
        uint64_t next_pid:17; //next produced id, need covert to ds
        uint64_t qp_id:16; //qpn
    };
    uint64_t raw;
};

struct xscdv_diamond_cqe {
    uint8_t     placeholder1;
    uint32_t    qp_id:15;
    uint8_t     :1;
    uint8_t     se:1;
    uint8_t     has_pph:1;
    uint8_t     type:1;
    uint8_t     with_immdt:1;
    uint8_t     csum_err:4;
    uint32_t    imm_data;
    uint32_t    msg_len;
    uint32_t    vni;
    uint64_t    ts:48;
    uint16_t    wqe_id;
    uint8_t     placeholder2;
    uint8_t     rsv2;
    uint16_t    rsv[2];
    uint16_t    rsv1:15;
    uint8_t     owner:1;
};

struct xscdv_cqe64 {
    xscdv_diamond_cqe   cqe;
    uint8_t             padding[32];
};

union xscdv_diamond_next_cq_doorbell {
    struct {
        uint64_t cq_next_cid:23;//next completed id
        uint64_t cq_id:16; //cqn
        uint64_t cq_sta:2;
    };
    uint64_t raw;
};

constexpr int XSCDV_BASE_WQE_SHIFT = 4;
constexpr int XSCDV_SND_WQE_SHIFT = 7;
constexpr int XSCDV_CQE_OWNER_MASK = 1;

__aicore__ inline void AivCommBase::AIVRDMAPostSendXSCDV(
    uint64_t laddr, uint32_t lkey,
    uint64_t raddr, uint32_t rkey, uint32_t bytes,
    __gm__ ChannelEntity* channelInfo)
{

    constexpr int POLL_CQ_THRESHOLD = 16;
    // 获取QP基本信息
    auto sqContext          = channelInfo->SqContextAddr[0];
    auto QP_DEPTH           = sqContext.RdmaSqContext.depth;
    auto curHardwareHead    = sqContext.RdmaSqContext.headAddr;
    auto curHardwareTail    = sqContext.RdmaSqContext.tailAddr;
    auto doorBellAddr       = sqContext.RdmaSqContext.dbVa;
    // 获取PI、CI
    uint32_t curHead = ld_dev((__gm__ uint32_t*)curHardwareHead, 0);
    uint32_t curTail = ld_dev((__gm__ uint32_t*)curHardwareTail, 0);

    if ((curHead + POLL_CQ_THRESHOLD) > (QP_DEPTH + curTail)) {
        AIVRDMAPollCqXSCDV(channelInfo, curHead);
    }

    __gm__ uint8_t* wqeAddr = 
        (__gm__ uint8_t*)((uintptr_t)sqContext.RdmaSqContext.sqVa + ((curHead % QP_DEPTH) << XSCDV_SND_WQE_SHIFT));
    __gm__ struct xscdv_wqe_ctrl_seg *ctrl_seg = 
        reinterpret_cast<__gm__ xscdv_wqe_ctrl_seg*>(wqeAddr);
    __gm__ struct xscdv_diamond_data_seg *rdata_seg = 
        reinterpret_cast<__gm__ xscdv_diamond_data_seg*>(reinterpret_cast<uintptr_t>(ctrl_seg) + sizeof(struct xscdv_wqe_ctrl_seg));
    __gm__ struct xscdv_diamond_data_seg *ldata_seg = 
        reinterpret_cast<__gm__ xscdv_diamond_data_seg*>(reinterpret_cast<uintptr_t>(rdata_seg) + sizeof(struct xscdv_diamond_data_seg));

    // PI转成网卡识别的ds_idx
    uint16_t wqe_ds_idx = curHead * 8;
    // 填充控制字段
    ctrl_seg->msg_opcode    = 1;                            // XSCALE_MSG_OPCODE_RDMA_WRITE = 1
    ctrl_seg->with_immdt    = 0;
    ctrl_seg->csum_en       = 0;
    ctrl_seg->ds_data_num   = 2;                            // 共2个data_seg, 不包括ctrl seg
    ctrl_seg->wqe_id        = wqe_ds_idx;
    ctrl_seg->msg_len       = bytes;
    ctrl_seg->opcode_data   = 0;
    ctrl_seg->se            = 0;
    ctrl_seg->ce            = 1;                            // 提供poll_cq后, 才需要产生cqe
    ctrl_seg->in_line       = 0;
    ctrl_seg->fence_mode    = 0;
    ctrl_seg->mask          = 0;
    ctrl_seg->rsv           = 0;
    // 填充数据字段
    rdata_seg->addr     = raddr;
    rdata_seg->key      = rkey;
    rdata_seg->length   = bytes;

    ldata_seg->addr     = laddr;
    ldata_seg->key      = lkey;
    ldata_seg->length   = bytes;

    // wqe cache flush
    cacheWriteThrough(wqeAddr, sizeof(struct xscdv_wqe_ctrl_seg) + 2 * sizeof(struct xscdv_diamond_data_seg));
    curHead++;
    // Fill Send DB
    union xscdv_diamond_next_send_doorbell doorBellInfo;
    uint32_t next_pid = curHead * 8;

    doorBellInfo.raw        = 0;
    doorBellInfo.qp_id      = sqContext.RdmaSqContext.qpn;
    doorBellInfo.next_pid   = next_pid;

    // Ring DB
    ubLocal.SetValue(0, doorBellInfo.raw);
    AscendC::GlobalTensor<uint64_t> dbTensor;
    dbTensor.SetGlobalBuffer((__gm__ uint64_t*)doorBellAddr);
    AscendC::DataCopyExtParams copyParams{1, 1 * sizeof(uint64_t), 0, 0, 0};
    PipeBarrier<PIPE_ALL>();
    AscendC::DataCopyPad(dbTensor, ubLocal, copyParams);
    PipeBarrier<PIPE_ALL>();
    // Update Sq PI
    st_dev(curHead, (__gm__ uint32_t*)curHardwareHead, 0);
}

__aicore__ inline bool AivCommBase::AIVParseCqe(__gm__ struct xscdv_cqe64* cqeAddr, uint32_t curTail, uint32_t cqDepth)
{
    bool cqeOwner = (cqeAddr->cqe.owner & XSCDV_CQE_OWNER_MASK);
    bool expectOwner = ((curTail & cqDepth) != 0);

    return cqeOwner == expectOwner;
}

__aicore__ inline void AivCommBase::AIVRDMAPollCqXSCDV(__gm__ ChannelEntity* channelInfo, uint32_t targetIdx)
{
    if (targetIdx == 0) {
        return;
    }

    // 获取CP基本信息
    auto sqContext          = channelInfo->SqContextAddr[0];
    auto cqContext          = channelInfo->CqContextAddr[0];
    auto CQ_DEPTH           = cqContext.RdmaCqContext.cqDepth;
    auto curHardwareTail    = cqContext.RdmaCqContext.tailAddr;
    auto cqeSize            = cqContext.RdmaCqContext.cqeSize;
    auto cqn                = cqContext.RdmaCqContext.cqn;
    auto doorBellAddr       = cqContext.RdmaCqContext.dbVa;

    // 获取CQ CI
    uint32_t curTail = ld_dev((__gm__ uint32_t*)curHardwareTail, 0);

    const int64_t timeOut = 10000; // us
    int64_t start_time = AscendC::GetSystemCycle();
    
    while (curTail != targetIdx) {
        __gm__ struct xscdv_cqe64* cqeAddr = (__gm__ struct xscdv_cqe64*)(
            (uintptr_t)cqContext.RdmaCqContext.cqVa + (curTail & (CQ_DEPTH - 1)) * cqeSize);

        // until cqeAddr->owner changed && timeout 10000us
        constexpr int64_t maxTime = 10000 * 50;
        int64_t cost_time = AscendC::GetSystemCycle() - start_time;
        while (!AIVParseCqe(cqeAddr, curTail, CQ_DEPTH) && (cost_time < maxTime)) {
            cacheWriteThrough((__gm__ uint8_t*)cqeAddr, sizeof(struct xscdv_cqe64));
            cost_time = AscendC::GetSystemCycle() - start_time;
        }

        // If TimeOut
        if (cost_time >= maxTime) {
            AIV_INFO("Poll cq timeout, curTail: %u \n", curTail);
            return;
        }

        curTail++;
    }

    // Fill CQ DB
    union xscdv_diamond_next_cq_doorbell doorBellInfo;

    doorBellInfo.raw            = 0;
    doorBellInfo.cq_id          = cqn;
    doorBellInfo.cq_next_cid    = curTail;
    doorBellInfo.cq_sta         = 0;

    // Update CQ tail
    st_dev(curTail, (__gm__ uint32_t*)(curHardwareTail), 0);

    // Update SQ tail
    st_dev(curTail, (__gm__ uint32_t*)(sqContext.RdmaSqContext.tailAddr), 0);

    // Ring CQ DB
    ubLocal.SetValue(0, doorBellInfo.raw);
    AscendC::GlobalTensor<uint64_t> dbTensor;
    dbTensor.SetGlobalBuffer((__gm__ uint64_t*)doorBellAddr);
    AscendC::DataCopyExtParams copyParams{1, 1 * sizeof(uint64_t), 0, 0, 0};
    PipeBarrier<PIPE_ALL>();
    AscendC::DataCopyPad(dbTensor, ubLocal, copyParams);
    PipeBarrier<PIPE_ALL>();
}

#endif // AIV_RDMA_POST_SEND_XSCDV_H
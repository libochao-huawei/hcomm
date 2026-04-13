#ifndef AIV_RDMA_POST_SEND_XSCDV_H
#define AIV_RDMA_POST_SEND_XSCDV_H

#include "kernel_operator.h"

__aicore__ inline void AIVRDMAPostSendXSCDV(
    uint64_t laddr, __be32 lkey,
    uint64_t raddr, __be32 rkey, uint32_t bytes,
    __gm__ ChannelEntity* channelInfo)
{
    // 获取QP基本信息
    auto sqContext          = channelInfo->SqContextAddr;
    auto QP_DEPTH           = sqContext->depth;
    auto curHardwareHead    = sqContext->headAddr;
    auto curHardwareTail    = sqContext->tailAddr;
    auto doorBellAddr       = sqContext->dbVa;

    // 获取PI、CI
    uint32_t curHead = ld_dev((__gm__ uint32_t*)curHardwareHead);
    uint32_t curTail = ld_dev((__gm__ uint32_t*)curHardwareTail);

    __gm__ uint8_t* wqeAddr = (__gm__ uint8_t*)((uintptr_t)sqContext->sqVa + ((curHead % QP_DEPTH) << XSCDV_SND_WQE_SHIFT));
    __gm__ struct xscdv_wqe_ctrl_seg *ctrl_seg = reinterpret_cast<__gm__ xscdv_wqe_ctrl_seg*>(wqeAddr);
    __gm__ struct xscdv_diamond_data_seg *rdata_seg = reinterpret_cast<__gm__ xscdv_diamond_data_seg*>(reinterpret_cast<uintptr_t>(ctrl_seg) + sizeof(struct xscdv_wqe_ctrl_seg));
    __gm__ struct xscdv_diamond_data_seg *ldata_seg = reinterpret_cast<__gm__ xscdv_diamond_data_seg*>(reinterpret_cast<uintptr_t>(rdata_seg) + sizeof(struct xscdv_diamond_data_seg));

    // PI转成网卡识别的ds_idx
    uint16_t wqe_ds_idx = curHead * 8;

    // 填充控制字段
    *ctrl_seg = {0};
    ctrl_seg->wqe_id        = wqe_ds_idx;
    ctrl_seg->ce            = 0;                            // 提供poll_cq后, 才需要产生cqe
    ctrl_seg->msg_opcode    = XSCDV_MSG_OPCODE_RDMA_WRITE;
    ctrl_seg->msg_len       = bytes;
    ctrl_seg->ds_data_num   = 2;                            // 共2个data_seg, 不包括ctrl seg
    ctrl_seg->in_line       = 0;

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
    union xscdv_diamond_send_doorbell doorBellInfo;
    uint32_t next_pid = curHead * 8;

    doorBellInfo.raw        = 0;
    doorBellInfo.qp_id      = sqContext->qpn;
    doorBellInfo.next_pid   = next_pid;

    // Ring DB
    st_dev(doorBellInfo.raw, (__gm__ uint64_t*)doorBellAddr, 0);

    // Update Sq PI
    st_dev(curHead, (__gm__ uint32_t*)curHardwareHead, 0);
}

__aicore__ inline void AIVRDMAPollCqXSCDV(__gm__ ChannelEntity* channelInfo, uint32_t targetIdx)
{
    if (targetIdx == 0) {
        return;
    }

    // 获取CP基本信息
    auto cqContext          = channelInfo->CqContextAddr;
    auto CQ_DEPTH           = cqContext->cqDepth;
    auto curHardwareTail    = cqContext->tailAddr;
    auto cqeSize            = cqContext->cqeSize;
    auto cqn                = cqContext->cqn;

    auto doorBellAddr       = cqContext->dbVa;

    // 获取CQ CI
    uint32_t curTail = ld_dev((__gm__ uint32_t*)curHardwareTail);

    // 获取当前cq tail位置
    const uint32_t maxTimes = 1000000;
    while (curTail != targetIdx) {
        __gm__ struct xscdv_cqe64* cqeAddr = (__gm__ struct xscdv_cqe64*)((uintptr_t)cqContext->cqVa + (curTail % CQ_DEPTH) * cqeSize);
        bool validOwner = (curTail / CQ_DEPTH) & 1;
        uint32_t times = 0;
        while ((validOwner ^ cqeAddr->owner) == 0 && times < maxTimes) { // util cqeAddr->owner changed
            dcci_cachelines((__gm__ uint8_t*)cqeAddr, sizeof(struct xscdv_cqe64));
            times++;
        }
        if (times >= maxTimes) {
            // Debug
            return ;
        }
        curTail++;

        // Check cqe status
        // ...
    }

    // Fill CQ DB
    union xscdv_diamond_next_cq_doorbell doorBellInfo;

    doorBellInfo.raw            = 0;
    doorBellInfo.cq_id          = cqn;
    doorBellInfo.cq_next_cid    = targetIdx;

    // Update CQ tail
    st_dev(curTail, (__gm__ uint32_t*)(curHardwareTail), 0);

    // Ring CQ DB
    st_dev(doorBellInfo.raw, (__gm__ uint64_t*)(doorBellAddr), 0);
}

#endif // AIV_RDMA_POST_SEND_XSCDV_H
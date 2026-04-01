#ifndef AIV_RDMA_POST_SEND_XSCDV_H
#define AIV_RDMA_POST_SEND_XSCDV_H

#include "kernel_operator.h"

__aicore__ inline void AIVRDMAPostSendXSCDV(
    uint64_t laddr, __be32 lkey,
    uint64_t raddr, __be32 rkey, uint32_t bytes,
    __gm__ ChannelEntity* channelInfo)
{
    // 获取QP基本信息
    auto sqContext  = channelInfo->SqContextAddr;
    auto QP_DEPTH   = sqContext->depth;

    // 获取PI
    auto curHardwareHead = sqContext->headAddr;
    cacheWriteThrough((__gm__ uint8_t*)curHardwareHead, 8);
    uint64_t curHead = *(__gm__ uint64_t*)(curHardwareHead);

    __gm__ uint8_t* wqeAddr = (__gm__ uint8_t*)((uintptr_t)sqContext->sqVa + ((curHead % QP_DEPTH) << XSCDV_SND_WQE_SHIFT));
    struct xscdv_wqe_ctrl_seg *ctrl_seg = reinterpret_cast<xscdv_wqe_ctrl_seg*>(wqeAddr);
    struct xscdv_diamond_data_seg *rdata_seg = reinterpret_cast<xscdv_diamond_data_seg*>(reinterpret_cast<uint_ptr_t>(ctrl_seg) + sizeof(struct xscdv_wqe_ctrl_seg));
    struct xscdv_diamond_data_seg *ldata_seg = reinterpret_cast<xscdv_diamond_data_seg*>(reinterpret_cast<uint_ptr_t>(rdata_seg) + sizeof(struct xscdv_diamond_data_seg));

    // PI转成网卡识别的ds_idx
    uint16_t wqe_ds_idx = curHead << (XSCDV_SND_WQE_SHIFT - XSCDV_BASE_WQE_SHIF);

    // 填充控制字段
    ctrl_seg = {0};
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
    PipeBarrier<PIPE_ALL>();
    curHead++;

    // 填充 DB
    union xscdv_diamond_send_doorbell doorBellInfo;
    uint32_t next_pid = curHead << (XSCDV_SND_WQE_SHIFT - XSCDV_BASE_WQE_SHIF);

    doorBellInfo.raw        = 0;
    doorBellInfo.qp_id      = sqContext->qpn;
    doorBellInfo.next_pid   = next_pid;

    // 注意dbVa没有偏移
    __gm__ uint64_t* doorBellAddr = (__gm__ uint64_t* )(sqContext->dbVa);
    PipeBarrier<PIPE_ALL>();

    // Ring DB
    // TODO 这里用MTE还是st_dev?
    ubLocal.SetValue(0, doorBellInfo);
    AscendC::GlobalTensor<uint64_t> DBGlobalTensor;
    DBGlobalTensor.SetGlobalBuffer(doorBellAddr);
    AscendC::DataCopyExtParams copyParams{1, 1 * sizeof(uint64_t), 0, 0, 0};
    PipeBarrier<PIPE_ALL>();
    AscendC::DataCopyPad(DBGlobalTensor, ubLocal, copyParams);
    PipeBarrier<PIPE_ALL>();

    // Update Sq PI
    ubLocalHead.SetValue(0, (uint32_t)curHead);
    AscendC::GlobalTensor<uint32_t> HeadGlobalTensor;
    HeadGlobalTensor.SetGlobalBuffer((__gm__ uint32_t*)curHardwareHead);
    AscendC::DataCopyExtParams copyParamsHead{1, 1 * sizeof(uint32_t), 0, 0, 0};
    PipeBarrier<PIPE_ALL>();
    AscendC::DataCopyPad(HeadGlobalTensor, ubLocalHead, copyParamsHead);
    PipeBarrier<PIPE_ALL>();
}

#endif // AIV_RDMA_POST_SEND_XSCDV_H
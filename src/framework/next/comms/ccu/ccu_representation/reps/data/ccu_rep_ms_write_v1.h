/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: ccu representation ms write header file
 * Create: 2025-03-23
 */

#ifndef HCOMM_CCU_REPRESENTATION_MS_WRITE_H
#define HCOMM_CCU_REPRESENTATION_MS_WRITE_H

#include "ccu_rep_base_v1.h"
#include "ccu_datatype_v1.h"

namespace hcomm {
namespace CcuRep {

// CcuRepMsWrite 对应 TransLocMSToRmtMSInstr（TRANS_TYPE=0x7）：
// 将本端 MS 数据写入远端 MS，并自动置位远端 CKE（write-with-notify）。
// rmtCkeIdx 为远端 CKE 逻辑索引（与 NotifyRecord/NotifyWait 使用同一套 per-channel 索引体系）。
// dst 由调用方（kernel 算法层）传入，为本地对等 CcuBuf，
// hcomm 内部利用对称 MS 分配（所有 rank 同一 kernel 的 MS 布局一致）从 dst.Id() 得到远端物理 msId。
class CcuRepMsWrite : public CcuRepBase {
public:
    CcuRepMsWrite(const ChannelHandle channel, CcuBuf src, CcuBuf dst, Variable len,
                  uint32_t remoteNotifyIdx, uint32_t mask);
    bool        Translate(CcuInstr *&instr, uint16_t &instrId, const TransDep &dep) override;
    std::string Describe() override;

private:
    ChannelHandle  channel;
    CcuBuf         dst;              // 远端对等 MS buffer（利用对称分配，dst.Id() = 远端物理 msId）
    CcuBuf         src;              // 本地 MS buffer（发送方）
    Variable       len;              // 数据长度寄存器
    uint32_t       remoteNotifyIdx{0}; // 远端 CKE 逻辑索引（GetRmtCkeByIndex 入参）
    uint32_t       mask{0};          // setRmtCKEMask：置位远端 CKE 的 bit mask
};

};     // namespace CcuRep
};     // namespace hcomm
#endif // HCOMM_CCU_REPRESENTATION_MS_WRITE_H

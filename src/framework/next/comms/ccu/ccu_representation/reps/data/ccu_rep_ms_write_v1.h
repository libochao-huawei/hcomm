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
// rmtMsId 由调用方（kernel 算法层）传入，穿刺版本传 0；
// 正式版本需在建链时通过 transport 层交换后传入。
class CcuRepMsWrite : public CcuRepBase {
public:
    CcuRepMsWrite(const ChannelHandle channel, CcuBuf src, Variable len,
                  uint32_t rmtCkeIdx, uint16_t rmtCkeMask, uint32_t rmtMsId);
    bool        Translate(CcuInstr *&instr, uint16_t &instrId, const TransDep &dep) override;
    std::string Describe() override;

private:
    ChannelHandle  channel;
    CcuBuf         src;           // 本地 MS buffer（发送方）
    Variable       len;           // 数据长度寄存器
    uint32_t       rmtCkeIdx{0};  // 远端 CKE 逻辑索引（GetRmtCkeByIndex 入参）
    uint16_t       rmtCkeMask{0}; // setRmtCKEMask：置位远端 CKE 的 bit mask
    uint32_t       rmtMsId{0};    // 远端 MS 物理 ID（穿刺版本为 0）
};

};     // namespace CcuRep
};     // namespace hcomm
#endif // HCOMM_CCU_REPRESENTATION_MS_WRITE_H

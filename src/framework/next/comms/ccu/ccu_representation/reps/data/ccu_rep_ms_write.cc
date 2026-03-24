/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: ccu representation ms write implementation file
 * Create: 2025-03-23
 */

#include "ccu_rep_v1.h"

#include "string_util.h"
#include "exception_util.h"
#include "ccu_api_exception.h"
#include "hcomm_c_adpt.h"

#include "../../../../endpoint_pairs/channels/ccu/ccu_urma_channel.h"

namespace hcomm {
namespace CcuRep {

CcuRepMsWrite::CcuRepMsWrite(const ChannelHandle channel, CcuBuf src, CcuBuf dst, Variable len,
                              uint32_t remoteNotifyIdx, uint32_t mask)
    : channel(channel), dst(dst), src(src), len(len), remoteNotifyIdx(remoteNotifyIdx), mask(mask)
{
    type       = CcuRepType::BUF_MS_WRITE;
    instrCount = 1;
}

bool CcuRepMsWrite::Translate(CcuInstr *&instr, uint16_t &instrId, const TransDep &dep)
{
    this->instrId = instrId;
    translated    = true;

    void *channelPtr{nullptr};
    auto ret = HcommChannelGet(channel, &channelPtr);
    if (ret != HcclResult::HCCL_SUCCESS) {
        Hccl::THROW<Hccl::CcuApiException>("failed to get ccu channel, type[%d]", type);
    }

    auto *channelImpl = dynamic_cast<CcuUrmaChannel *>(static_cast<Channel *>(channelPtr));
    if (channelImpl == nullptr) {
        Hccl::THROW<Hccl::CcuApiException>("[%s] failed to cast channel[0x%llx] to CcuUrmaChannel",
            __func__, channel);
    }

    // TransLocMSToRmtMSInstr 参数说明：
    //   rmtMSId        : 远端 MS 物理 ID（利用对称 MS 分配，取 dst.Id()）
    //   locMSId        : 本端 MS 物理 ID（src.Id() = physMsId + 0x8000 * dieId）
    //   lengthXnId     : 数据长度寄存器 ID（Xn）
    //   channelId      : 通信 channel ID
    //   setRmtCKEId    : 置位远端 CKE 的物理 ID（由 remoteNotifyIdx 逻辑索引经 GetRmtCkeByIndex 转换）
    //   setRmtCKEMask  : 置位远端 CKE 的 bit mask
    //   setCKEId=0, setCKEMask=0  : 本端无额外 CKE 置位
    //   waitCKEId=0, waitCKEMask=0: 本端无 CKE 等待（由算法层 NotifyWait 控制）
    //   clearType=1    : 标准清除模式
    //   lengthEn=1     : 使能长度字段
    uint32_t setRmtCkeId{0};
    channelImpl->GetRmtCkeByIndex(remoteNotifyIdx, setRmtCkeId); // 逻辑索引 → 物理 CKE ID

    TransLocMSToRmtMSInstr(instr++,
        static_cast<uint16_t>(dst.Id()),    // 远端 MS 物理 ID（对称分配，dst.Id() = 远端 msId）
        static_cast<uint16_t>(src.Id()),    // 本端 MS 物理 ID
        len.Id(),                           // 数据长度寄存器 ID（Xn）
        channelImpl->GetChannelId(),        // channel ID
        static_cast<uint16_t>(setRmtCkeId), // 置位远端 CKE 物理 ID
        static_cast<uint16_t>(mask),           // 置位远端 CKE bit mask
        0, 0,                               // setCKEId=0, setCKEMask=0（无本端置位）
        0, 0,                               // waitCKEId=0, waitCKEMask=0（无本端等待）
        1, 1);                              // clearType=1, lengthEn=1

    instrId += instrCount;

    return translated;
}

std::string CcuRepMsWrite::Describe()
{
    void *channelPtr{nullptr};
    auto ret = HcommChannelGet(channel, &channelPtr);
    if (ret != HcclResult::HCCL_SUCCESS) {
        Hccl::THROW<Hccl::CcuApiException>("failed to get ccu channel, type[%d]", type);
    }

    auto *channelImpl = dynamic_cast<CcuUrmaChannel *>(static_cast<Channel *>(channelPtr));
    if (channelImpl == nullptr) {
        Hccl::THROW<Hccl::CcuApiException>("[%s] failed to cast channel[0x%llx] to CcuUrmaChannel",
            __func__, channel);
    }
    return Hccl::StringFormat("MsWrite src[%u] To dst[%u], len[%u], ChannelId[%u], remoteNotifyIdx[%u], mask[%04x]",
        src.Id(), dst.Id(), len.Id(), channelImpl->GetChannelId(), remoteNotifyIdx, mask);
}

}; // namespace CcuRep
}; // namespace hcomm

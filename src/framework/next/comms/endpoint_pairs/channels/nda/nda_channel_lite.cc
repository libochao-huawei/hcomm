
#include "nda_channel.h"

// 下发Rtsq sqe, 敲DB
void NdaChannelLite::BuildRdmaDbSendTask(const StreamLite &stream, u64 remoteAddr, u64 dbValue)
{
    stream.GetRtsq()->RdmaDbSend(remoteAddr, dbValue);
}

// 下发Rtsq sqe, NotifyWait
void NdaChannelLite::BuildNotifyWaitTask(const StreamLite &stream, u32 notifyId)
{
    stream.GetRtsq()->NotifyWait(notifyId);
}

void NdaChannelLite::Write(const RmaBufferLite &loc, const RmtRmaBufferLite &rmt, const StreamLite &stream) {
    CheckConnVec("NdaChannelLite::Write");

    // Post Wqe && return dbValue
    u64 dbAddr = 0;
    u64 dbValue = 0;
    connVec_[0]->Write(
        GetRmaBufSlicelite(loc), GetRmtRmaBufSliceLite(rmt), dbAddr, dbValue);

    // Ring Doorbell
    BuildRdmaDbSendTask(stream, dbAddr, dbValue);
}

void NdaChannelLite::WriteWithNotify(const RmaBufferLite &loc, const RmtRmaBufferLite &rmt, const uint32_t remoteNotifyIdx, const StreamLite &stream) {
    CheckConnVec("NdaChannelLite::WriteWithNotify");

    // Post Wqe && return dbValue
    u64 dbAddr = 0;
    u64 dbValue = 0;

    // TODO 此处localNotify获取接口未添加？
    connVec_[0]->WriteWithNotify(
        GetRmaBufSlicelite(loc), GetRmtRmaBufSliceLite(rmt),
        GetRmaBufSlicelite(loc), GetRmtNotifySliceLite(remoteNotifyIdx), dbAddr, dbValue);

    // Ring Doorbell
    BuildRdmaDbSendTask(stream, dbAddr, dbValue);
}

void NdaChannelLite::NotifyWait(const uint32_t index, const StreamLite &stream) {
    auto notifyId = localNotifies_[index]->GetId();
    BuildNotifyWaitTask(stream, notifyId);
}
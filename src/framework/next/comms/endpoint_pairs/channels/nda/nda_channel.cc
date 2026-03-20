
#include "nda_channel.h"


NdaChannel::NdaChannel(EndpointHandle endpointHandle, HcommChannelDesc channelDesc, CommEngine engine)
    :endpointHandle_(endpointHandle), channelDesc_(channelDesc), engine_(engine) {}

NdaChannel::~NdaChannel() {}

// 下发Rtsq sqe, 敲DB
void NdaChannel::BuildRdmaDbSendTask(const StreamLite &stream, u64 remoteAddr, u64 dbValue)
{
    stream.GetRtsq()->RdmaDbSend(remoteAddr, dbValue);
}

// 下发Rtsq sqe, NotifyWait
void NdaChannel::BuildNotifyWaitTask(const StreamLite &stream, u32 notifyId)
{
    stream.GetRtsq()->NotifyWait(notifyId);
}

void NdaChannel::Write(const RmaBufferLite &loc, const Buffer &rmt, const StreamLite &stream) {
    CheckConnVec("NdaChannel::Write");

    // Post Wqe && return dbValue
    u64 dbAddr = 0;
    u64 dbValue = 0;
    connections_[0]->Write(GetRmaBufSlicelite(loc), GetRmtRmaBufSliceLite(rmt), stream, dbAddr, dbValue);
    // Ring Doorbell
    BuildRdmaDbSendTask(stream, dbAddr, dbValue);
}

void NdaChannel::WriteWithNotify(const RmaBufferLite &loc, const Buffer &rmt, const uint32_t remoteNotifyIdx, const StreamLite &stream) {
    CheckConnVec("NdaChannel::WriteWithNotify");

    // Post Wqe && return dbValue
    u64 dbAddr = 0;
    u64 dbValue = 0;

    // TODO 此处localNotify获取接口未添加？
    connections_[0]->Write(GetRmaBufSlicelite(loc), GetRmtRmaBufSliceLite(rmt), GetRmaBufSlicelite(loc), GetRmtNotifySliceLite(withNotify.index_), dbAddr, dbValue);
    // Ring Doorbell
    BuildRdmaDbSendTask(stream, dbAddr, dbValue);
}

void NdaChannel::NotifyWait(const uint32_t index, const StreamLite &stream) {
    auto notifyId = locNotifyVec[index]->GetId();
    BuildNotifyWaitTask(stream, notifyId);
}

#include "nda_channel.h"


NdaChannel::NdaChannel(EndpointHandle endpointHandle, HcommChannelDesc channelDesc, CommEngine engine)
    :endpointHandle_(endpointHandle), channelDesc_(channelDesc), engine_(engine) {}

NdaChannel::~NdaChannel() {}

// 下发Rtsq sqe, 敲DB
void NdaChannel::BuildRdmaDbSendTask(const StreamLite &stream, u64 remoteAddr, u64 dbValue)
{
    stream.GetRtsq()->RdmaDbSend(remoteAddr, dbValue);
}

void NdaChannel::Write(const RmaBufferLite &loc, const Buffer &rmt, const StreamLite &stream) {
    CheckConnVec("NdaChannel::Write");

    // Post Wqe && return dbValue
    u64 dbVa = 0;
    u64 dbValue = 0;
    connections_[0]->Write(GetRmaBufSlicelite(loc), GetRmtRmaBufSliceLite(rmt), stream, dbVa, dbValue);
    // Ring Doorbell
    BuildRdmaDbSendTask(stream, dbVa, dbValue);
}

void NdaChannel::WriteWithNotify(const RmaBufferLite &loc, const Buffer &rmt, const uint32_t remoteNotifyIdx, const StreamLite &stream) {
    CheckConnVec("NdaChannel::WriteWithNotify");

    // NotifyIdx
    uint32_t dpuNotifyId = remoteDpuNotifyIds_[remoteNotifyIdx];

    // Post Wqe && return dbValue
    u64 dbVa = 0;
    u64 dbValue = 0;
    connections_[0]->Write(GetRmaBufSlicelite(loc), GetRmtRmaBufSliceLite(rmt), dpuNotifyId, dbVa, dbValue);
    // Ring Doorbell
    BuildRdmaDbSendTask(stream, dbVa, dbValue);
}

void NdaChannel::NotifyWait(const uint32_t localNotifyIdx, const uint32_t timeout) {
    CheckConnVec("NdaChannel::NotifyWait");

    // NotifyIdx
    uint32_t dpuNotifyId = localDpuNotifyIds_[localNotifyIdx];

    // Poll_cq
    connections_[0]->NotifyWait(dpuNotifyId, timeout);
}
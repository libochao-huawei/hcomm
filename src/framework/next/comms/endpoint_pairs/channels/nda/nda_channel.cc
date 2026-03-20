
#include "nda_channel.h"


NdaChannel::NdaChannel(EndpointHandle endpointHandle, HcommChannelDesc channelDesc, CommEngine engine)
    :endpointHandle_(endpointHandle), channelDesc_(channelDesc), engine_(engine) {}

NdaChannel::~NdaChannel() {}

// 初始化connOut状态
void NdaChannel::ClearConnOut()
{
    wqeData.clear();
    wqeData.resize(64);     // MAX Wqe Size -> 64 Byte
    connOut.data     = (u8 *)wqeData.data();
    connOut.dataSize = sizeof(wqeData);
}

// 检查connection不能为空
void NdaChannel::CheckConnVec(const std::string &desc)
{
    if (UNLIKELY(connVec.size() == 0)) {
        THROW<InternalException>(StringFormat("connVec size is 0 %s", desc.c_str()));
    }

    u32 idx = 0;
    for (auto &it : connVec) {
        if (UNLIKELY(it == nullptr)) {
            THROW<InternalException>(StringFormat("connVec[%u] is null %s", idx, desc.c_str()));
        }
        idx++;
    }
}

// 下发Rtsq sqe, 敲DB
void NdaChannel::BuildRdmaDbSendTask(const StreamLite &stream, u64 dbValue)
{
    stream.GetRtsq()->RdmaDbSend(dbValue);
}

void NdaChannel::Write(const RmaBufferLite &loc, const Buffer &rmt, const StreamLite &stream) {
    CheckConnVec("NdaChannel::Write");

    // Post Wqe && return dbValue
    u64 dbValue = 0;
    connections_[0]->Write(GetRmaBufSlicelite(loc), GetRmtRmaBufSliceLite(rmt), stream, dbValue);
    // Ring Doorbell
    BuildRdmaDbSendTask(stream, dbValue);
}

void NdaChannel::WriteWithNotify(const RmaBufferLite &loc, const Buffer &rmt, const uint32_t remoteNotifyIdx, const StreamLite &stream) {
    CheckConnVec("NdaChannel::WriteWithNotify");

    // NotifyIdx
    uint32_t dpuNotifyId = remoteDpuNotifyIds_[remoteNotifyIdx];

    // Post Wqe && return dbValue
    u64 dbValue = 0;
    connections_[0]->Write(GetRmaBufSlicelite(loc), GetRmtRmaBufSliceLite(rmt), dpuNotifyId, dbValue);
    // Ring Doorbell
    BuildRdmaDbSendTask(stream, dbValue);
}

void NdaChannel::NotifyWait(const uint32_t localNotifyIdx, const uint32_t timeout) {
    CheckConnVec("NdaChannel::NotifyWait");

    // NotifyIdx
    uint32_t dpuNotifyId = localDpuNotifyIds_[localNotifyIdx];

    // Poll_cq
    connections_[0]->NotifyWait(dpuNotifyId, timeout);
}
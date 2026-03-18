
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
    ClearConnOut();
    CheckConnVec("NdaChannel::Write");
    // Post Wqe && return dbValue
    u64 dbValue = 0;
    connections_[0]->Write(GetRmaBufSlicelite(loc), GetRmtRmaBufSliceLite(rmt), stream, dbValue);
    // Ring Doorbell
    BuildRdmaDbSendTask(stream, dbValue);
}
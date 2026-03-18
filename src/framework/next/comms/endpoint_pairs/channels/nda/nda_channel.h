#ifndef NDA_CHANNEL_H
#define NDA_CHANNEL_H

#include <mutex>

#include "../channel.h"
#include "hccl_common.h"
#include "hccp_nda.h"

#include "../../../../../../legacy/unified_platform/resource/socket/socket.h"
#include "../../../../../../legacy/unified_platform/resource/buffer/local_rdma_rma_buffer.h"
#include "remote_rma_buffer.h"
#include "nda_rdma_connection.h"

namespace hcomm {

class NdaChannel final: public Channel {
public:
    NdaChannel(EndpointHandle endpointHandle, HcommChannelDesc channelDesc, CommEngine engine);
    ~NdaChannel();

    // 数据面
    void NotifyRecord(const uint32_t remoteNotifyIdx, const StreamLite &stream);

    void NotifyWait(const uint32_t localNotifyIdx, const uint32_t timeout);

    void Write(const RmaBufferLite &loc, const Buffer &rmt, const StreamLite &stream);

    void WriteReduce(const RmaBufferLite &loc, const Buffer &rmt, const ReduceIn &reduceIn,
                     const StreamLite &stream);

    void WriteWithNotify(const RmaBufferLite &loc, const Buffer &rmt, const uint32_t remoteNotifyIdx,
                         const StreamLite &stream);

    void WriteReduceWithNotify(const RmaBufferLite &loc, const Buffer &rmt, const ReduceIn &reduceIn,
                               uint32_t remoteNotifyIdx, const StreamLite &stream);

    // rtsq ring Doorbell
    void BuildRdmaDbSendTask(const StreamLite &stream, u64 dbValue);

    // Help build local buffer
    HcclResult BuildLocRmaBufferLite(const uintptr_t addr, const size_t size, RmaBufferLite &rmaBufferLite) const;

private:
    EndpointHandle endpointHandle_;
    HcommChannelDesc channelDesc_;
    CommEngine engine_;

    std::vector<std::unique_ptr<NdaRdmaConnection>> connections_{};
    std::vector<uint32_t> localDpuNotifyIds_;

    std::vector<SqContext> sqContextList_{};
    std::vector<CqContext> cqContextList_{};
};
}   // namespace hcomm

#endif
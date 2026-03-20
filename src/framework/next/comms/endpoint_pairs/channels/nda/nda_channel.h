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
    void NotifyWait(const uint32_t index, const StreamLite &stream);

    void Write(const RmaBufferLite &loc, const Buffer &rmt, const StreamLite &stream);

    void WriteWithNotify(const RmaBufferLite &loc, const Buffer &rmt, const uint32_t remoteNotifyIdx,
                         const StreamLite &stream);

    // rtsq ring Doorbell
    void BuildRdmaDbSendTask(const StreamLite &stream, u64 remoteAddr, u64 dbValue);
};
}   // namespace hcomm

#endif
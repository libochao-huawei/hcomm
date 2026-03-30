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

class NdaChannelLite final: public Channel {
public:
    // 数据面
    void NotifyWait(const uint32_t index, const StreamLite &stream);

    void Write(const RmaBufferLite &loc, const RmtRmaBufferLite &rmt, const StreamLite &stream);

    void WriteWithNotify(const RmaBufferLite &loc, const RmtRmaBufferLite &rmt, const uint32_t remoteNotifyIdx,
                         const StreamLite &stream);

    // rtsq ring Doorbell
    void BuildRdmaDbSendTask(const StreamLite &stream, u64 remoteAddr, u64 dbValue);

    // rtsq Notify Wait
    void BuildNotifyWaitTask(const StreamLite &stream, u32 notifyId);
};
}   // namespace hcomm

#endif
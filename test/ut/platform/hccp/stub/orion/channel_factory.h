#ifndef CHANNEL_FACTORY_H
#define CHANNEL_FACTORY_H
#include <memory>
#include "hccl/hccl_rank_graph.h"
#include "hccl_api.h"
#include "interface_channel.h"

namespace Hccl {

// 工厂函数声明 输出作为参数传出
HcclResult CreateChannel(CommEngine engine, HcclChannelDesc channelDesc, void *addr, uint64_t size,
                         uint32_t localRankId, std::unique_ptr<IChannel> &channel)
{return HCCL_SUCCESS;};

// 具体的创建函数声明
std::unique_ptr<IChannel> CreateDpuRoceChannel(HcclChannelDesc channelDesc, void *addr, uint64_t size,
                                               uint32_t localRankId);

}  // namespace Hccl
#endif
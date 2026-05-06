/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef CHANNEL_H
#define CHANNEL_H

#include <memory>
#include <vector>
#include <unordered_map>
#include "hccl/hccl_res.h"
#include "hccl/hccl_types.h"
#include "hcomm_res_defs.h"
#include "hccl_mem_defs.h"
#include <cstdint>
#include "mem_device_pub.h"
#include <string>
#include <unordered_map>
#include <vector>
#include "enum_factory.h"

// Orion
#include "transport_status.h"
#include "ip_address.h"
#include "topo_common_types.h"
#include "virtual_topo.h"
#include "exception_handler.h"

namespace hcomm {

MAKE_ENUM(ChannelStatus, INIT, SOCKET_OK, SOCKET_TIMEOUT, READY, FAILED)

/**
 * @brief 通道种类（与 HcommChannelRes.channelTypeList 中 u32 数值一致；由 CommEngine + CommProtocol 推导）。
 */
enum class HcommChannelKind : uint32_t {
    INVALID = 0U,
    AICPU_TS_URMA = 1U,
    AICPU_TS_ROCE = 2U,
    AICPU_TS_HCCS = 3U,
    CPU_ROCE = 4U,
    AIV_UB_MEM = 5U,
    AICPU_TS_UBOE = 6U,
};

/**
 * @note 职责：一个EndPointPair上的建立的通信通道的C++抽象接口类声明。
 * 管理该通信通道Channel对上的同步信号Notify、通信队列（如qp、jetty等）等资源管理，负责建立连接，以及注册内存、同步信号等的交换。
 */
class Channel {
public:
    Channel() {};
    virtual ~Channel() = default;

    // 禁拷贝（避免切片/资源重复释放等）
    Channel(const Channel&) = delete;
    Channel& operator=(const Channel&) = delete;

    // 视需要决定是否允许移动；很多资源类也会禁移动
    Channel(Channel&&) = default;
    Channel& operator=(Channel&&) = default;

    // ------------------ 控制面接口 ------------------
    virtual HcclResult Init() = 0;
    virtual HcclResult GetNotifyNum(uint32_t *notifyNum) const = 0;
    virtual HcclResult GetRemoteMem(HcclMem **remoteMem, uint32_t *memNum) = 0;
    virtual ChannelStatus GetStatus() = 0;
    virtual HcclResult GetUserRemoteMem(CommMem **remoteMem, uint32_t *memNum);
    virtual HcclResult UpdateMemInfo(HcommMemHandle *memHandles, uint32_t memHandleNum);

    virtual HcclResult Clean()        = 0;
    virtual HcclResult Resume()       = 0;

    virtual HcommChannelKind GetChannelKind() const;
    virtual HcclResult Serialize(std::shared_ptr<hccl::DeviceMem> &out);
    // ------------------ 数据面接口 ------------------
    virtual HcclResult NotifyRecord(const uint32_t remoteNotifyIdx) = 0;
    virtual HcclResult NotifyWait(const uint32_t localNotifyIdx, const uint32_t timeout) = 0;
    virtual HcclResult WriteWithNotify(void *dst, const void *src, const uint64_t len, uint32_t remoteNotifyIdx) = 0;
    virtual HcclResult Write(void *dst, const void *src, uint64_t len) = 0;
    virtual HcclResult Read(void *dst, const void *src, uint64_t len) = 0;
    virtual HcclResult ChannelFence() = 0;

    // ------------------ 工具方法 ------------------
    static ChannelStatus TransportStatusToChannelStatus(Hccl::TransportStatus ts);

    // ------------------ 工厂 ------------------
    static HcclResult CreateChannel(EndpointHandle endpointHandle, 
                                    CommEngine engine, 
                                    HcommChannelDesc channelDesc,
                                    std::unique_ptr<Channel>& out);
};

// 前向声明，避免include
namespace Hccl {
class RdmaHandle;
class LocalUbRmaBuffer;
class LocalIpcRmaBuffer;
class LocalRmaBuffer;
struct CommonLocRes;
}

namespace hcomm {

/**
 * @brief Channel Buffer 构建辅助类，用于消除子类中的重复代码
 */
class ChannelBufferHelper {
public:
    /**
     * @brief 构建LocalUbRmaBuffer类型的buffer (用于AicpuTsUrmaChannel和AicpuTsUboeChannel)
     * @tparam BufferPtrType LocalUbRmaBuffer的unique_ptr类型
     * @tparam BufferVecType localRmaBuffers_的vector类型
     * @param bufs 输入的buffer vector
     * @param rdmaHandle RdmaHandle句柄
     * @param bufferVecTemp bufferVecTemp_引用
     * @param commonRes commonRes_引用
     * @param localRmaBuffers localRmaBuffers_引用
     * @return HcclResult HCCL_SUCCESS成功，其他失败
     */
    template<typename BufferPtrType, typename BufferVecType>
    static HcclResult BuildUbRmaBuffers(
        const std::vector<std::shared_ptr<Hccl::Buffer>> &bufs,
        const Hccl::RdmaHandle &rdmaHandle,
        std::vector<Hccl::LocalRmaBuffer*> &bufferVecTemp,
        Hccl::CommonLocRes &commonRes,
        BufferVecType &localRmaBuffers)
    {
        bufferVecTemp.clear();
        for (size_t i = 0; i < bufs.size(); i++) {
            BufferPtrType bufferPtr = nullptr;
            EXECEPTION_CATCH(
                bufferPtr = std::make_unique<Hccl::LocalUbRmaBuffer>(bufs[i], rdmaHandle),
                return HCCL_E_PTR
            );
            bufferVecTemp.push_back(bufferPtr.get());
            commonRes.bufferVec.push_back(bufferPtr.get());
            localRmaBuffers.push_back(std::move(bufferPtr));
        }
        return HCCL_SUCCESS;
    }

    /**
     * @brief 构建LocalIpcRmaBuffer类型的buffer (用于AicpuTsP2pChannel)
     * @tparam BufferPtrType LocalIpcRmaBuffer的unique_ptr类型
     * @tparam BufferVecType localRmaBuffers_的vector类型
     * @param bufs 输入的buffer vector
     * @param bufferVecTemp bufferVecTemp_引用
     * @param commonRes commonRes_引用
     * @param localRmaBuffers localRmaBuffers_引用
     * @return HcclResult HCCL_SUCCESS成功，其他失败
     */
    template<typename BufferPtrType, typename BufferVecType>
    static HcclResult BuildIpcRmaBuffers(
        const std::vector<std::shared_ptr<Hccl::Buffer>> &bufs,
        std::vector<Hccl::LocalRmaBuffer*> &bufferVecTemp,
        Hccl::CommonLocRes &commonRes,
        BufferVecType &localRmaBuffers)
    {
        bufferVecTemp.clear();
        for (size_t i = 0; i < bufs.size(); i++) {
            BufferPtrType bufferPtr = nullptr;
            EXECEPTION_CATCH(
                bufferPtr = std::make_unique<Hccl::LocalIpcRmaBuffer>(bufs[i]),
                return HCCL_E_PTR
            );
            bufferVecTemp.push_back(bufferPtr.get());
            commonRes.bufferVec.push_back(bufferPtr.get());
            localRmaBuffers.push_back(std::move(bufferPtr));
        }
        return HCCL_SUCCESS;
    }
};

} // namespace hcomm
#endif // CHANNEL_H

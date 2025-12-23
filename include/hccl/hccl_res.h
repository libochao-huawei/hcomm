/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef HCCL_RES_H
#define HCCL_RES_H

#define HCCL_CTX_API

#include "hcomm_primitives.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 * @brief 通信域句柄类型（不透明结构）
 */
typedef void *HcclComm;

/**
 * @brief 通信引擎类型枚举
 */
typedef enum {
    COMM_ENGINE_RESERVED = -1,    ///< 保留的通信引擎
    COMM_ENGINE_HOSTCPU = 0,      ///< HOST CPU引擎
    COMM_ENGINE_HOSTCPU_TS = 1,   ///< HOST CPU TS引擎
    COMM_ENGINE_AICPU = 2,        ///< AICPU引擎
    COMM_ENGINE_AICPU_TS = 3,     ///< AICPU TS引擎
    COMM_ENGINE_AIV = 4,          ///< AIV引擎
    COMM_ENGINE_CCU = 5,          ///< CCU引擎
} CommEngine;

typedef enum {
    NOTIFY_TYPE_RESERVED = -1,
    NOTIFY_TYPE_RTS_NOTIFY = 0,
    NOTIFY_TYPE_RTS_EVENT = 1,
    NOTIFY_TYPE_DEVICE_MEM = 2,
} NotifyType;

const uint32_t HCCL_CHANNEL_MAGIC_WORD = 0x0f0f0f0f;
const uint32_t HCCL_CHANNEL_VERSION = 1;    // HcclChannelDesc更新时，HCCL_CHANNEL_VERSION + 1

/**
 * @brief 兼容Abi字段结构体
 */
typedef struct {
    uint32_t version;
    uint32_t magicWord;
    uint32_t size;
    uint32_t reserved;
} HcclAbiHeader;

/**
 * @brief 通信协议类型枚举
 * @warning
 */
typedef enum {
    COMM_PROTOCOL_RESERVED = -1,  ///< 保留协议类型
    COMM_PROTOCOL_HCCS = 0,        ///< HCCS协议
    COMM_PROTOCOL_TCP = 1,        ///< 标准TCP协议
    COMM_PROTOCOL_ROCE = 2,       ///< RDMA over Converged Ethernet
    COMM_PROTOCOL_UB_CTP = 3,    ///< 华为统一总线UB_CTP
    COMM_PROTOCOL_UB_TP = 4,     ///< 华为统一总线UB_TP
    COMM_PROTOCOL_PCIE = 5,      ///< PCIE协议
    COMM_PROTOCOL_SIO = 6,        ///< SIO协议
} CommProtocol;

typedef struct {
    // 成员定义
} HccsAttr;

/**
 * @brief RoCE协议属性
 * @warning
 */
typedef struct {
    uint32_t queueNum;        ///< QP数量
    uint32_t queueMode;       ///< QP工作模式：normal,offload...
    uint16_t *udpSport;       ///< 源端口号数组；负载均衡，基于每个QP配置源端口号
    uint8_t tc;               ///< 流量类别(QoS)
    uint8_t sl;               ///< 服务等级(QoS)
    uint32_t retryCnt;        ///< 最大重传次数
    uint32_t retryInterval;   ///< 重传间隔(ms)（对应协议计算公式）
} RoCEAttr;

/**
 * @brief Jetty属性配置
 * @warning
 */
typedef struct {
    uint32_t mode;  ///< 模式标识：normal/offload等
} JettyAttr;

/**
 * @brief 统一总线(UB)属性
 * @warning
 */
typedef struct {
    JettyAttr *jettyAttr;     ///< Jetty属性数组
    uint32_t jettyNum;        ///< Jetty数量
} UbAttr;

/**
 * @brief 通道描述参数
 * @warning
 */
typedef struct {
    HcclAbiHeader header;
    uint32_t remoteRank;    ///< 远端rankId
    CommProtocol protocol;  ///< 通信协议
    uint32_t notifyNum;  ///< channel上使用的通知消息数量
    union {
        HccsAttr hccsAttr;
        RoCEAttr roceAttr;
        UbAttr ubAttr;
    };
} HcclChannelDesc;
// 解决与Hcomm仓合入问题
#define HCCL_CHANNEL_ABI
/**
 * @brief 初始化HcclChannelDesc结构体
 *
 * @param[inout] channelDesc 返回的通道描述参数
 * @return void
 * @warning
 */
inline void HcclChannelDescInit(HcclChannelDesc *channelDesc, uint32_t descNum)
{
    for (uint32_t idx = 0; idx < descNum; idx++) {
        if (channelDesc != nullptr) {
            // Abi字段初始化
            channelDesc->header.version     = HCCL_CHANNEL_VERSION;
            channelDesc->header.magicWord   = HCCL_CHANNEL_MAGIC_WORD;
            channelDesc->header.size        = sizeof(HcclChannelDesc);
            channelDesc->header.reserved    = 0;

            // HcclChannelDesc内容初始化
            channelDesc->remoteRank = ~0U;
            channelDesc->protocol   = COMM_PROTOCOL_RESERVED;
            channelDesc->notifyNum  = 0;
            (void)memset_s(&(channelDesc->hccsAttr), sizeof(HccsAttr), 0, sizeof(HccsAttr));
            (void)memset_s(&(channelDesc->roceAttr), sizeof(RoCEAttr), 0, sizeof(RoCEAttr));
            (void)memset_s(&(channelDesc->ubAttr), sizeof(UbAttr), 0, sizeof(UbAttr));
        }
    }
    return;
}

/// HCCL算子标识最大长度（字节）
const uint32_t HCCL_OP_TAG_LEN_MAX = 255;

/**
 * @name 通信内存获取
 * @{
 */
/**
 * @brief 获取通信域中的Hccl缓存(CCL Buffer)
 * @param[in] comm 通信域句柄
 * @param[out] buffer Hccl缓存信息
 * @param[out] size 缓存区域字节数
 * @return HcclResult 执行结果状态码
 * @note 即获取通信域中的CCL Buffer
 * @warning  1、CCLBuffer是否有是Host场景？——对应返回的数据结构； 2、先不用HcclMem *mem； \n
 *          3、未来可扩展增加CommGetMemType接口
 */
extern HcclResult HcclGetHcclBuffer(HcclComm comm, void **buffer, uint64_t *size);

/**
 * @defgroup 通信引擎资源管理
 * @{
 */

/**
 * @brief 获取通信线程资源
 *
 * @param[in] comm 通信域句柄
 * @param[in] engine 通信引擎类型
 * @param[in] threadNum 线程数量
 * @param[in] notifyNumPerThread 每线程的通知数量
 * @param[out] thread 返回的线程句柄
 * @return HcclResult 执行结果状态码
 * @warning
 */
extern HcclResult HcclAllocThreadRes(HcclComm comm, CommEngine engine, uint32_t threadNum,
    uint32_t notifyNumPerThread, ThreadHandle *thread);

/**
 * @brief 基于已有流Stream申请指定notifyNum的通信线程资源
 * @param[in] comm 通信域句柄
 * @param[in] engine 通信引擎类型
 * @param[in] stream stream句柄
 * @param[in] notifyNum 通知数量
 * @param[out] thread 返回的线程句柄
 * @return HcclResult 执行结果状态码
 */
extern HcclResult HcclThreadAcquireWithStream(HcclComm comm, CommEngine engine,
    aclrtStream stream, uint32_t notifyNum, ThreadHandle *thread);

/**
 * @brief 获取线程通知数量
 * @param[in] comm 通信域句柄
 * @param[in] thread 线程句柄
 * @param[in] engine 通信引擎类型
 * @param[out] notifyNum 通知数量
 * @return HcclResult 执行结果状态码
 * @note 1.基于thread获取notify数量\n 2.后续数据面用notify idx操作
 */
extern HcclResult HcclGetNotifyNumInThread(HcclComm comm, ThreadHandle thread, CommEngine engine, uint32_t *notifyNum);

/**
 * @brief 基于通信域申请Notify
 * 
 * @param hcclComm 
 * @param commEngine 
 * @param notifyType 
 * @param notifyHandleList
 * @return HcclResult 
 * @warning  需要考虑是否带tag 2、需要确认安全校验等方案
 */
extern HcclResult HcclAllocNotify(HcclComm comm, CommEngine commEngine,
    NotifyType notifyType, uint32_t notifyNum, NotifyHandle **notifyHandleList);

/** @} */  // 通信引擎资源管理

/**
 * @defgroup 通信通道管理接口
 * @{
 */
//  * @param[in] opTag 算子标签（建议包含算子名和标识，最大字符长度为HCCL_OP_TAG_LEN_MAX）
/**
 * @brief 基于算子tag创建通信通道
 * @param[in] comm 通信域句柄
 * @param[in] engine 通信引擎类型
 * @param[in] channelDescList 通道描述列表
 * @param[in] listNum 列表数量
 * @param[out] channelList 创建的通道句柄列表
 * @return HcclResult 执行结果状态码
 * @note 非阻塞接口\n
 * 1.描述需建链的信息（如协议、远端EID等），支持批量完成与多个远端的建链\n
 * 2.CreateChannel时将交换内存信息（包含：已绑定到通信域的内存、已注册到算子TAG上的内存）\n
 * 3.注册后，通信域内以remoteRank+opName+customTag作为key存储该channel；如opName和customTag为空，则通信域内已remoteRank为key存储\n
 * 4.资源描述相同时，key相同情况下已有资源，则复用该资源返回，不重新创建；如需重复创建资源，可使用不同的tag\n
 * 5.注册后，通信域内以remoteRank+opName+customTag作为key存储该channel；如opName和customTag为空，则通信域内已remoteRank为key存储\n
 * 6.资源描述相同时，key相同情况下已有资源，则复用该资源返回，不重新创建（ps：host展开场景下 ，jetty不能复用）
 * @warning
 */
extern HcclResult HcclChannelAcquire(HcclComm comm, CommEngine engine, const HcclChannelDesc *channelDescList,
    uint32_t listNum, ChannelHandle *channelList);

/**
 * @brief 获取通道通知数量
 * @param[in] channel 通道句柄
 * @param[out] notifyNum 通知槽数量
 * @return HcclResult 执行结果状态码
 */
extern HcclResult HcclChannelGetNotifyNum(HcclComm comm, ChannelHandle channel, uint32_t *notifyNum);

/**
 * @brief 获取制定channel的Hccl通信缓存
 * @param[in] comm 通信域句柄
 * @param[in] channel 通信通道句柄
 * @param[out] buffer Hccl缓存信息
 * @param[out] size 缓存区域字节数
 * @return HcclResult 执行结果状态码
 * @note 获取远端CCL buffer
 * @warning
 */
extern HcclResult HcclChannelGetHcclBuffer(HcclComm comm, ChannelHandle channel, void **buffer, uint64_t *size);

 /**
 * @defgroup 通信引擎上下文管理接口（编程控制面可选接口）
 * @{
 */

//  * @param[in] opTag 算子标签（建议包含算子名和标识，最大字符长度为HCCL_OP_TAG_LEN_MAX）
/**
 * @brief 创建算子通信引擎上下文
 * @param[in] comm 通信域句柄
 * @param[in] ctxTag 引擎标签（最大字符长度为HCCL_OP_TAG_LEN_MAX）
 * @param[in] engine 通信引擎类型
 * @param[in] size ctx内存大小
 * @param[out] ctx 通信引擎上下文
 * @return HcclResult 执行结果状态码
 * @note 1、由使用者决定ctx的大小，并排布ctx里面的内容\n
 *       2、通信库提供ctx地址空间申请接口，并返回该地址空间的类型{host/device}，不同类型决定地址空间的访问方式\n
 *       3、通信库基于通信域+opTag为key存储ctx地址
 * @warning
 */
extern HcclResult HcclEngineCtxCreate(HcclComm comm, const char *ctxTag, CommEngine engine, uint64_t size, void **ctx);

/**
 * @brief 获取算子通信引擎上下文
 * @param[in] comm 通信域句柄
 * @param[in] ctxTag 引擎标签（最大字符长度为HCCL_OP_TAG_LEN_MAX）
 * @param[in] engine 通信引擎类型
 * @param[out] ctx 通信引擎上下文
 * @param[out] size ctx内存大小
 * @return HcclResult 执行结果状态码
 * @note 使用者可先查询ctx是否已存在，再决定是否重新申请ctx地址
 * @warning 
 */
extern HcclResult HcclEngineCtxGet(HcclComm comm, const char *ctxTag, CommEngine engine, void **ctx, uint64_t *size);

/**
 * @brief 拷贝算子通信引擎上下文
 * @param[in] comm 通信域句柄
 * @param[in] engine 通信引擎类型
 * @param[in] ctxTag 引擎标签（最大字符长度为HCCL_OP_TAG_LEN_MAX）
 * @param[in] srcCtx 拷贝的源引擎
 * @param[in] size 拷贝的ctx内存大小
 * @param[in] dstCtxOffset 拷贝的ctx地址偏移
 * @return HcclResult 执行结果状态码
 * @note 1、目标ctx通过ctxTag获取
 * @warning
 */
extern HcclResult HcclEngineCtxCopy(HcclComm comm, CommEngine engine, const char *ctxTag, const void *srcCtx,
    uint64_t size, uint64_t dstCtxOffset);

#ifdef __cplusplus
}
#endif  // __cplusplus
#endif
/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef HCOMM_PRIMITIVES_H
#define HCOMM_PRIMITIVES_H

#include <stdint.h>
#include <securec.h>
#include <arpa/inet.h>
#include "acl/acl_rt.h"
#include <hccl_types.h>

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

#ifndef CHANNEL_HANDLE_DEFINED
#define CHANNEL_HANDLE_DEFINED
/**
 * @brief 通道句柄类型
 */
typedef uint64_t ChannelHandle;
#endif

#ifndef THREAD_HANDLE_DEFINED
#define THREAD_HANDLE_DEFINED
/**
 * @brief 线程句柄类型
 */
typedef uint64_t ThreadHandle;
#endif

typedef enum {
    HCOMM_REDUCE_SUM = 0,    /**< sum */
    HCOMM_REDUCE_PROD = 1,   /**< prod */
    HCOMM_REDUCE_MAX = 2,    /**< max */
    HCOMM_REDUCE_MIN = 3,    /**< min */
    HCOMM_REDUCE_RESERVED = 255 /**< reserved */
} HcommReduceOp;

typedef enum {
    HCOMM_DATA_TYPE_INT8 = 0,    /**< int8 */
    HCOMM_DATA_TYPE_INT16 = 1,   /**< int16 */
    HCOMM_DATA_TYPE_INT32 = 2,   /**< int32 */
    HCOMM_DATA_TYPE_FP16 = 3,    /**< fp16 */
    HCOMM_DATA_TYPE_FP32 = 4,    /**< fp32 */
    HCOMM_DATA_TYPE_INT64 = 5,    /**< int64 */
    HCOMM_DATA_TYPE_UINT64 = 6,    /**< uint64 */
    HCOMM_DATA_TYPE_UINT8 = 7,    /**< uint8 */
    HCOMM_DATA_TYPE_UINT16 = 8,   /**< uint16 */
    HCOMM_DATA_TYPE_UINT32 = 9,   /**< uint32 */
    HCOMM_DATA_TYPE_FP64 = 10,    /**< fp64 */
    HCOMM_DATA_TYPE_BFP16 = 11,    /**< bfp16 */
    HCOMM_DATA_TYPE_INT128 = 12,   /**< int128 */
#ifndef OPEN_BUILD_PROJECT
    HCOMM_DATA_TYPE_HIF8 = 14,     /**< hif8 */
    HCOMM_DATA_TYPE_FP8E4M3 = 15,  /**< fp8e4m3 */
    HCOMM_DATA_TYPE_FP8E5M2 = 16,  /**< fp8e5m2 */
    HCOMM_DATA_TYPE_FP8E8M0 = 17,  /**< fp8e8m0 */
#endif
    HCOMM_DATA_TYPE_RESERVED = 255 /**< reserved */
} HcommDataType;

/**
 * @defgroup 数据面编程接口
 * @{
 */

/**
 * @name 本地拷贝和规约
 * @{
 */

/**
 * @brief 本地内存拷贝
 * @param[in] thread 线程句柄
 * @param[out] dst 目标地址
 * @param[in] src 源地址
 * @param[in] len 数据长度（字节）
 * @return int32_t 执行结果状态码
 * @note 源目内存地址要能执行引擎直接访问
 */
extern int32_t HcommLocalCopyOnThread(ThreadHandle thread, void *dst, const void *src, uint64_t len);

/**
 * @brief 本地归约操作
 * @param[in] thread 线程句柄
 * @param[out] dst 目标地址
 * @param[in] src 源地址
 * @param[in] count 元素个数
 * @param[in] dataType 数据类型
 * @param[in] reduceOp 归约操作类型
 * @return int32_t 执行结果状态码
 */
extern int32_t HcommLocalReduceOnThread(
    ThreadHandle thread, void *dst, const void *src, uint64_t count, HcommDataType dataType, HcommReduceOp reduceOp);
/** @} */  // 本地拷贝和规约

/**
 * @name 本地线程间同步通知
 * @{
 */

/**
 * @brief 本地记录通知
 * @param[in] thread 线程句柄
 * @param[in] dstThread 目标线程句柄
 * @param[in] dstNotifyIdx 目标通知索引
 * @return int32_t 执行结果状态码
 * @note 配合HcommThreadNotifyWaitOnThread使用
 */
extern int32_t HcommThreadNotifyRecordOnThread(ThreadHandle thread, ThreadHandle dstThread, uint32_t dstNotifyIdx);

/**
 * @brief 本地等待通知
 * @param[in] thread 线程句柄
 * @param[in] notifyIdx 通知索引
 * @param[in] timeout 超时时间(毫秒)
 * @return int32_t 执行结果状态码
 * @note 配合HcommThreadNotifyRecordOnThread使用
 */
extern int32_t HcommThreadNotifyWaitOnThread(ThreadHandle thread, uint32_t notifyIdx, uint32_t timeOut);
/** @} */  // 本地线程间同步通知

/**
 * @name 本地通知
 * @{
 */

/**
 * @brief 记录通知事件（生产者）
 * @param[in] streamHandle 异步流句柄
 * @param[in] dstNotifyId 通知id
 * @return 执行状态码 int32_t
 */
extern int32_t HcommAclrtNotifyRecordOnThread(ThreadHandle thread, uint64_t dstNotifyId);

/**
 * @brief 等待通知事件（消费者）
 * @param[in] streamHandle 异步流句柄
 * @param[in] notifyId 通知id
 * @param[in] timeOut 超时时间
 * @return 执行状态码 int32_t
 */
extern int32_t HcommAclrtNotifyWaitOnThread(ThreadHandle thread, uint64_t notifyId, uint32_t timeOut);
/** @} */  // 本地通知

/**
 * @name 数据读写相关
 * @{
 */

/**
 * @brief 单边写操作
 * @param[in] thread 线程句柄
 * @param[in] channel 通道句柄
 * @param[out] dst 目标内存地址
 * @param[in] src 源内存地址
 * @param[in] len 数据长度（字节）
 * @return int32_t 执行结果状态码
 */
extern int32_t HcommWriteOnThread(
    ThreadHandle thread, ChannelHandle channel, void *dst, const void *src, uint64_t len);

/**
 * @brief 归约写操作
 * @param[in] thread 线程句柄
 * @param[in] channel 通道句柄
 * @param[out] dst 目标内存地址
 * @param[in] src 源内存地址
 * @param[in] count 元素个数
 * @param[in] dataType 数据类型
 * @param[in] reduceOp 归约操作类型
 * @return int32_t 执行结果状态码
 */
extern int32_t HcommWriteReduceOnThread(ThreadHandle thread, ChannelHandle channel, void *dst, const void *src,
    uint64_t count, HcommDataType dataType, HcommReduceOp reduceOp);

/**
 * @brief 带通知的单边写操作
 * @param[in] thread 线程句柄
 * @param[in] channel 通道句柄
 * @param[out] dst 目标内存地址
 * @param[in] src 源内存地址
 * @param[in] len 数据长度（字节）
 * @param[in] notifyIdx 远端通知索引
 * @return int32_t 执行结果状态码
 * @note 当前在A5上主要支持
 * 
 * WARNING: experimental API, No compatibility is currently guaranteed for this API
 */
extern int32_t HcommWriteWithNotifyOnThread(ThreadHandle thread, ChannelHandle channel, void *dst, const void *src,
    uint64_t len, uint32_t remoteNotifyIdx);

/**
 * @brief 带通知的归约写操作
 * @param[in] thread 线程句柄
 * @param[in] channel 通道句柄
 * @param[out] dst 目标内存地址
 * @param[in] src 源内存地址
 * @param[in] count 元素个数
 * @param[in] dataType 数据类型
 * @param[in] reduceOp 归约操作类型
 * @param[in] remoteNotifyIdx 远端通知索引
 * @return int32_t 执行结果状态码
 * 
 * WARNING: experimental API, No compatibility is currently guaranteed for this API
 */
extern int32_t HcommWriteReduceWithNotifyOnThread(ThreadHandle thread, ChannelHandle channel, void *dst,
    const void *src, uint64_t count, HcommDataType dataType, HcommReduceOp reduceOp, uint32_t remoteNotifyIdx);

/**
 * @brief 单边读操作
 * @param[in] thread 线程句柄
 * @param[in] channel 通道句柄
 * @param[out] dst 目标内存地址
 * @param[in] src 源内存地址
 * @param[in] len 数据长度（字节）
 * @return int32_t 执行结果状态码
 */
extern int32_t HcommReadOnThread(
    ThreadHandle thread, ChannelHandle channel, void *dst, const void *src, uint64_t len);

/**
 * @brief 归约读操作
 * @param[in] thread 线程句柄
 * @param[in] channel 通道句柄
 * @param[out] dst 目标内存地址
 * @param[in] src 源内存地址
 * @param[in] count 元素个数
 * @param[in] dataType 数据类型
 * @param[in] reduceOp 归约操作类型
 * @return int32_t 执行结果状态码
 */
extern int32_t HcommReadReduceOnThread(ThreadHandle thread, ChannelHandle channel, void *dst, const void *src, uint64_t count,
    HcommDataType dataType, HcommReduceOp reduceOp);

/**
 * @brief 单边写操作
 * @param[in] channel 通道句柄
 * @param[out] dst 目标内存地址
 * @param[in] src 源内存地址
 * @param[in] len 数据长度（字节）
 * @return int32_t 执行结果状态码
 * 
 * WARNING: experimental API, No compatibility is currently guaranteed for this API
 */
extern int32_t HcommWriteNbi(ChannelHandle channel, void *dst, const void *src, uint64_t len);
 
/**
 * @brief 带通知的单边写操作
 * @param[in] channel 通道句柄
 * @param[out] dst 目标内存地址
 * @param[in] src 源内存地址
 * @param[in] len 数据长度（字节）
 * @param[in] notifyIdx 远端通知索引
 * @return int32_t 执行结果状态码
 * @note 当前在A5上主要支持
 * 
 * WARNING: experimental API, No compatibility is currently guaranteed for this API
 */
extern int32_t HcommWriteWithNotifyNbi(ChannelHandle channel, void *dst, const void *src,
    uint64_t len, uint32_t remoteNotifyIdx);
 
/**
 * @brief 单边读操作
 * @param[in] channel 通道句柄
 * @param[out] dst 目标内存地址
 * @param[in] src 源内存地址
 * @param[in] len 数据长度（字节）
 * @return int32_t 执行结果状态码
 * 
 * WARNING: experimental API, No compatibility is currently guaranteed for this API
 */
extern int32_t HcommReadNbi(ChannelHandle channel, void *dst, const void *src, uint64_t len);

/** @} */  // 数据读写相关

/**
 * @name 通知
 * @{
 */

/**
 * @brief 记录通知事件
 * @param[in] thread 线程句柄
 * @param[in] channel 通道句柄
 * @param[in] remoteNotifyIdx 远端通知索引
 * @return int32_t 执行结果状态码
 */
extern int32_t HcommChannelNotifyRecordOnThread(ThreadHandle thread, ChannelHandle channel, uint32_t remoteNotifyIdx);

/**
 * @brief 记录通知事件
 * @param[in] channel 通道句柄
 * @param[in] remoteNotifyIdx 远端通知索引
 * @return int32_t 执行结果状态码
 * 
 * WARNING: experimental API, No compatibility is currently guaranteed for this API
 */
extern int32_t HcommChannelNotifyRecord(ChannelHandle channel, uint32_t remoteNotifyIdx);

/**
 * @brief 等待通知事件
 * @param[in] thread 线程句柄
 * @param[in] channel 通道句柄
 * @param[in] localNotifyIdx 本地通知索引
 * @param[in] timeout 超时时间(毫秒)
 * @return int32_t 执行结果状态码
 */
extern int32_t HcommChannelNotifyWaitOnThread(ThreadHandle thread, ChannelHandle channel, uint32_t localNotifyIdx, uint32_t timeout);

/**
 * @brief 等待通知事件
 * @param[in] channel 通道句柄
 * @param[in] localNotifyIdx 本地通知索引
 * @param[in] timeout 超时时间(毫秒)
 * @return int32_t 执行结果状态码
 * 
 * WARNING: experimental API, No compatibility is currently guaranteed for this API
 */
extern int32_t HcommChannelNotifyWait(ChannelHandle channel, uint32_t localNotifyIdx, uint32_t timeout);

/** @} */  // 通知

/**
 * @defgroup 批量下发设置接口
 * @{
 */

/**
 * @brief 批量模式执行开始
 * @param batchTag 批量Id
 * @return int32_t 执行结果状态码
 * @note Start和End及中间的批量任务需要在同一个线程上执行
 */
extern int32_t HcommBatchModeStart(const char *batchTag);

/**
 * @brief 批量模式执行结束
 * @param batchTag 批量Id
 * @return int32_t 执行结果状态码
 * @note Start和End及中间的批量任务需要在同一个线程上执行
 */
extern int32_t HcommBatchModeEnd(const char *batchTag);

/** @} */  // 批量下发设置接口

/** @} */  // 数据面编程接口
/** @} */  // 算子编程接口

/**
 * @brief 获取通信域并加锁
 * @param[in] commId 通信域id
 * @return int32_t 执行结果状态码
 * @note 当前仅支持AICPU模式
 */
extern int32_t HcommAcquireComm(const char* commId);

/**
 * @brief 释放通信域
 * @param[in] commId 通信域id
 * @return int32_t 执行结果状态码
 * @note 当前仅支持AICPU模式
 */
extern int32_t HcommReleaseComm(const char* commId);

/**
 * @brief Get symmetric memory pointer.
 *
 * @param winHandle A pointer identifying the registered memory window handle.
 * @param offset A size_t identifying the offset of symmetric memory heap.
 * @param peerRank A u_integer identifying the identify for the peer rank.
 * @param ptr A pointer identifying the symmetric memory heap address.
 * @return HcclResult
 */
extern HcclResult HcommSymWinGetPeerPointer(CommSymWindow winHandle, size_t offset, uint32_t peerRank, void** ptr);

#define HCOMM_PRIMITIVES_H_MODIFIED


/**
 * @brief NPU上查询 rtsq任务执行完成的接口（阻塞）
 * @param[in] thread NPU上执行的线程句柄
 * @return int32_t 执行结果状态码
 * 
 * WARNING: experimental API, No compatibility is currently guaranteed for this API
 */
extern int32_t HcommThreadSynchronize(ThreadHandle thread);

using MsgHandle = uint64_t;

/**
 * @brief NPU 通过 HBM 共享内存向 DPU 发送同步消息（非阻塞）
 * @param[in] handle 目的地址，位于 HBM 共享内存
 * @param[in] msgTag 消息（算子任务）标签（char[256])
 * @param[in] src 附加消息源地址
 * @param[in] sizeByte 消息大小（字节）
 * @param[out] msgId 消息 Id 指针
 * @return int32_t 执行结果状态码
 * 
 * WARNING: experimental API, No compatibility is currently guaranteed for this API
 */
extern int32_t HcommSendRequest(MsgHandle handle, const char* msgTag, const void *src, size_t sizeByte, uint32_t *msgId);

/**
 * @brief NPU 通过 HBM 共享内存接收 DPU 同步消息（非阻塞）
 * @param[in] handle 源地址，位于 HBM 共享内存
 * @param[in] dst 读出数据的地址
 * @param[in] sizeByte 数据大小（字节）
 * @param[out] msgId 消息 Id 指针
 * @return int32_t 执行结果状态码
 * 
 * WARNING: experimental API, No compatibility is currently guaranteed for this API
 */
extern int32_t HcommWaitResponse(MsgHandle handle, void *dst, size_t sizeByte, uint32_t *msgId);

/**
 * @brief DPU数据面flush接口
 * @param[in] void
 * @return int32_t 执行结果状态码
 * 
 * WARNING: experimental API, No compatibility is currently guaranteed for this API
*/
extern int32_t HcommFlush();

/**
 * @brief 通信通道级内存屏障操作
 * @param[in] channel 通道句柄
 * @return int32_t 执行结果状态码
 * 
 * WARNING: experimental API, No compatibility is currently guaranteed for this API
 */
extern int32_t HcommChannelFence(ChannelHandle channel);

/** @} */  // 算子编程接口
#ifdef __cplusplus
}
#endif  // __cplusplus

#endif
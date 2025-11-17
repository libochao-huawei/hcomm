/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef HCCL_API_DATA_H
#define HCCL_API_DATA_H

#include "hccl_types.h"
#include <stdint.h>
#include <securec.h>
#include <arpa/inet.h>
#include <hccl/base.h>
#include "hccl_mem_defs.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 * @brief 通道句柄类型（不透明结构）
 * @warning
 */
typedef uint64_t ChannelHandle;


/**
 * @brief 线程句柄类型（不透明结构）
 */
typedef uint64_t ThreadHandle;

 /**
 * @brief 下发模式
 * @warning
 */
typedef enum {
    LAUNCH_MODE_RESERVED = -1, ///< 保留的下发模式
    LAUNCH_MODE_EAGER = 0,     ///< 直接下发模式（实时执行）
    LAUNCH_MODE_BATCH          ///< 批量下发模式（延迟合并执行）
} LaunchMode;


/**
 * @defgroup 数据面编程接口
 * @{
 */

/**
 * @defgroup 本地操作接口
 * @{
 */

 /**
 * @name 本地数据拷贝相关
 * @{
 */

/**
 * @brief 本地内存拷贝
 * @param[in] thread 线程句柄
 * @param[out] dst 目标地址
 * @param[in] src 源地址
 * @param[in] len 数据长度（字节）
 * @return HcclResult 执行结果状态码
 * @note 源目内存地址要能执行引擎直接访问
 * @warning
 */
extern HcclResult CommLocalCopy(ThreadHandle thread, void *dst, const void *src, uint64_t len);

/**
 * @brief 本地归约操作
 * @param[in] thread 线程句柄
 * @param[out] dst 目标地址
 * @param[in] src 源地址
 * @param[in] count 元素个数
 * @param[in] dataType 数据类型
 * @param[in] reduceOp 归约操作类型
 * @return HcclResult 执行结果状态码
 */
extern HcclResult CommLocalReduce(
    ThreadHandle thread, void *dst, const void *src, uint64_t count, HcclDataType dataType, HcclReduceOp reduceOp);

/** @} */  // 本地基本操作

/**
 * @name 本地通知
 * @{
 */

/**
 * @brief 本地记录通知
 * @param[in] thread 线程句柄
 * @param[in] dstThread 目标线程句柄
 * @param[in] dstNotifyIdx 目标通知索引
 * @return HcclResult 执行结果状态码
 * @note 配合CommLocalNotifyWait使用
 * @warning
 */
extern HcclResult CommLocalNotifyRecord(ThreadHandle thread, ThreadHandle dstThread, uint32_t dstNotifyIdx);

/**
 * @brief 本地等待通知
 * @param[in] thread 线程句柄
 * @param[in] notifyIdx 通知索引
 * @param[in] timeout 超时时间(毫秒)
 * @return HcclResult 执行结果状态码
 * @note 配合CommLocalNotifyRecord使用
 * @warning
 */
extern HcclResult CommLocalNotifyWait(ThreadHandle thread, uint32_t notifyIdx, uint32_t timeout);


/**
 * @brief 记录通知事件（生产者）
 * @param[in] streamHandle 异步流句柄
 * @param[in] dstNotifyId 通知id
 * @return 执行状态码 HcclResult
 */
extern HcclResult CommLocalBareNotifyRecord(ThreadHandle thread, uint64_t dstNotifyId);

/**
 * @brief 等待通知事件（消费者）
 * @param[in] streamHandle 异步流句柄
 * @param[in] notifyId 通知id
 * @param[in] timeOut 超时时间
 * @return 执行状态码 HcclResult
 */
extern HcclResult CommLocalBareNotifyWait(ThreadHandle thread, uint64_t notifyId, uint32_t timeOut);
/** @} */  // 本地通知

/**
 * @name 同步
 * @{
 */

/**
 * @brief 本地同步操作
 * @param[in] thread 线程句柄
 * @return HcclResult 执行结果状态码
 * @note 确保该thread上此前的所有任务都已经执行完成。在AIV、Host 网卡等场景常用
 * @warning
 */
extern HcclResult CommFence(ThreadHandle thread, ChannelHandle channel);

/** @} */  // 同步
/** @} */  // 本地操作接口

/**
 * @defgroup 通信通道操作接口（单边语义）
 * @{
 * @warning
 */

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
 * @return HcclResult 执行结果状态码
 * @warning
 */
extern HcclResult CommWrite(
    ThreadHandle thread, ChannelHandle channel, void *dst, const void *src, uint64_t len);

/**
 * @brief 带通知的单边写操作
 * @param[in] thread 线程句柄
 * @param[in] channel 通道句柄
 * @param[out] dst 目标内存地址
 * @param[in] src 源内存地址
 * @param[in] len 数据长度（字节）
 * @param[in] notifyIdx 远端通知索引
 * @return HcclResult 执行结果状态码
 * @note 当前在A5上主要支持
 */
extern HcclResult CommWriteWithNotify(ThreadHandle thread, ChannelHandle channel, void *dst, const void *src,
    uint64_t len, uint32_t remoteNotifyIdx);

/**
 * @brief 归约写操作
 * @param[in] thread 线程句柄
 * @param[in] channel 通道句柄
 * @param[out] dst 目标内存地址
 * @param[in] src 源内存地址
 * @param[in] count 元素个数
 * @param[in] dataType 数据类型
 * @param[in] reduceOp 归约操作类型
 * @return HcclResult 执行结果状态码
 */
extern HcclResult CommWriteReduce(ThreadHandle thread, ChannelHandle channel, void *dst, const void *src,
    uint64_t count, HcclDataType dataType, HcclReduceOp reduceOp);

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
 * @return HcclResult 执行结果状态码
 * @note 当前在A5上主要支持
 */
extern HcclResult CommWriteReduceWithNotify(ThreadHandle thread, ChannelHandle channel, void *dst, const void *src,
    uint64_t count, HcclDataType dataType, HcclReduceOp reduceOp, uint32_t remoteNotifyIdx);

/**
 * @brief 单边读操作
 * @param[in] thread 线程句柄
 * @param[in] channel 通道句柄
 * @param[out] dst 目标内存地址
 * @param[in] src 源内存地址
 * @param[in] len 数据长度（字节）
 * @return HcclResult 执行结果状态码
 */
extern HcclResult CommRead(
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
 * @return HcclResult 执行结果状态码
 */
extern HcclResult CommReadReduce(ThreadHandle thread, ChannelHandle channel, void *dst, const void *src, uint64_t count,
    HcclDataType dataType, HcclReduceOp reduceOp);

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
 * @return HcclResult 执行结果状态码
 */
extern HcclResult CommNotifyRecord(ThreadHandle thread, ChannelHandle channel, uint32_t remoteNotifyIdx);

/**
 * @brief 等待通知事件
 * @param[in] thread 线程句柄
 * @param[in] channel 通道句柄
 * @param[in] localNotifyIdx 本地通知索引
 * @param[in] timeout 超时时间(毫秒)
 * @return HcclResult 执行结果状态码
 */
extern HcclResult CommNotifyWait(ThreadHandle thread, ChannelHandle channel, uint32_t localNotifyIdx, uint32_t timeout);

/** @} */  // 通知

/**
 * @name channel同步
 * @{
 */

/**
 * @brief 通信通道级同步操作
 * @param[in] thread 线程句柄
 * @param[in] channel 通道句柄
 * @return HcclResult 执行结果状态码
 * @note 确保该通道上此前的所有任务都已经执行完成
 * @warning
 */
extern HcclResult CommChannelFence(ThreadHandle thread, ChannelHandle channel);

/** @} */  // channel同步
/** @} */  // 通信通道操作接口（单边语义）

/**
 * @defgroup 批量下发设置接口
 * @{
 */

/**
 * @brief 设置任务下发模式（批量或直接下发）
 * @param[in] launchId 下发Id
 * @param[in] mode 下发模式
 * @return HcclResult 执行结果状态码
 * @note 可运行在Host或Device上。
 * @warning
 */
extern HcclResult CommSetLaunchMode(int64_t launchId, LaunchMode mode);

/** @} */  // 批量下发设置接口
/** @} */  // 数据面编程接口
/** @} */  // 算子编程接口
#ifdef __cplusplus
}
#endif  // __cplusplus

#endif  // HCCL_API_DATA_H
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

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

typedef uint64_t NotifyHandle;

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
 * @name 本地拷贝和规约
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
 * @warning  是否需要将数据面接口的void *改为void，因为在较多场景存在地址不是直接访问的。
 */
extern HcclResult HcommLocalCopyOnThread(ThreadHandle thread, void *dst, const void *src, uint64_t len);

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
extern HcclResult HcommLocalReduceOnThread(
    ThreadHandle thread, void *dst, const void *src, uint64_t count, HcclDataType dataType, HcclReduceOp reduceOp);

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
 * @return HcclResult 执行结果状态码
 * @note 配合HcommInterThreadNotifyWaitOnThread使用
 * @warning
 */
extern HcclResult HcommInterThreadNotifyRecordOnThread(ThreadHandle thread, ThreadHandle dstThread, uint32_t dstNotifyIdx);

/**
 * @brief 本地等待通知
 * @param[in] thread 线程句柄
 * @param[in] notifyIdx 通知索引
 * @param[in] timeout 超时时间(毫秒)
 * @return HcclResult 执行结果状态码
 * @note 配合HcommInterThreadNotifyRecordOnThread使用
 * @warning
 */
extern HcclResult HcommInterThreadNotifyWaitOnThread(ThreadHandle thread, uint32_t notifyIdx, uint32_t timeout);
/** @} */  // 本地线程间同步通知

/**
 * @name 本地通知
 * @{
 */

/**
 * @brief 记录通知事件（生产者）
 * @param[in] streamHandle 异步流句柄
 * @param[in] dstNotifyId 通知id
 * @return 执行状态码 HcclResult
 */
extern HcclResult HcommInterOpNotifyRecordOnThread(ThreadHandle thread, uint64_t dstNotifyId);

/**
 * @brief 等待通知事件（消费者）
 * @param[in] streamHandle 异步流句柄
 * @param[in] notifyId 通知id
 * @param[in] timeOut 超时时间
 * @return 执行状态码 HcclResult
 */
extern HcclResult HcommInterOpNotifyWaitOnThread(ThreadHandle thread, uint64_t notifyId, uint32_t timeOut);
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
 * @return HcclResult 执行结果状态码
 * @warning
 */
extern HcclResult HcommWriteOnThread(
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
extern HcclResult HcommWriteWithNotifyOnThread(ThreadHandle thread, ChannelHandle channel, void *dst, const void *src,
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
extern HcclResult HcommWriteReduceOnThread(ThreadHandle thread, ChannelHandle channel, void *dst, const void *src,
    uint64_t count, HcclDataType dataType, HcclReduceOp reduceOp);

/**
 * @brief 单边读操作
 * @param[in] thread 线程句柄
 * @param[in] channel 通道句柄
 * @param[out] dst 目标内存地址
 * @param[in] src 源内存地址
 * @param[in] len 数据长度（字节）
 * @return HcclResult 执行结果状态码
 */
extern HcclResult HcommReadOnThread(
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
extern HcclResult HcommReadReduceOnThread(ThreadHandle thread, ChannelHandle channel, void *dst, const void *src, uint64_t count,
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
extern HcclResult HcommNotifyRecordOnThread(ThreadHandle thread, ChannelHandle channel, uint32_t remoteNotifyIdx);

/**
 * @brief 等待通知事件
 * @param[in] thread 线程句柄
 * @param[in] channel 通道句柄
 * @param[in] localNotifyIdx 本地通知索引
 * @param[in] timeout 超时时间(毫秒)
 * @return HcclResult 执行结果状态码
 */
extern HcclResult HcommNotifyWaitOnThread(ThreadHandle thread, ChannelHandle channel, uint32_t localNotifyIdx, uint32_t timeout);
/** @} */  // 通知

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
extern HcclResult HcommSetLaunchMode(const char *launchTag, LaunchMode mode);

/** @} */  // 批量下发设置接口

/** @} */  // 数据面编程接口
/** @} */  // 算子编程接口

/**
 * @brief 获取通信域并加锁
 * @param[in] commId 通信域id
 * @return HcclResult 执行结果状态码
 * @note 当前仅支持AICPU模式
 */
extern HcclResult HcommAcquireComm(const char* commId);

/**
 * @brief 释放通信域
 * @param[in] commId 通信域id
 * @return HcclResult 执行结果状态码
 * @note 当前仅支持AICPU模式
 */
extern HcclResult HcommReleaseComm(const char* commId);

/**
 * @brief 注册算子信息到通信域
 * @param[in] commId 通信域id
 * @param[in] opInfo 算子信息
 * @param[in] size 算子信息的数据长度
 * @return HcclResult 执行结果状态码
 * @note 当前仅支持AICPU模式
 */
extern HcclResult HcommRegisterOpInfo(const char* commId, void* opInfo, uint32_t size);

/**
 * @brief 注册taskException算子信息解析函数
 * @param[in] commId 通信域id
 * @param[in] callback 解析算子信息并输出字符数组的回调函数
 * @param[in] opInfo 算子信息存储的内存地址
 * @param[in] size 算子信息存储的内存长度
 * @return HcclResult 执行结果状态码
 * @note 当前仅支持AICPU模式
 */
typedef void (*HcommGetOpInfoCallback)(const void *opInfo, char *outPut, size_t size);
extern HcclResult HcommRegOpTaskException(const char* commId, HcommGetOpInfoCallback callback);
#ifdef __cplusplus
}
#endif  // __cplusplus

#endif
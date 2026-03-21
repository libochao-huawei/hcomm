/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
 
#ifndef HCOMM_C_ADPT_H
#define HCOMM_C_ADPT_H
 
#include "hcomm_res_defs.h"
#include "hccl/hccl_res.h"
#include "mem_host_pub.h"
#include "hccl_diag.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

typedef struct{
    int32_t devPhyId;
    uint32_t superPodId;
} HcommDevId;

/**
 * @struct HcommBuf
 * @brief 内存缓冲区描述结构体
 * @var addr   - 虚拟地址指针
 * @var len    - 内存长度（单位字节）
 */
typedef struct {
    void *addr;
    uint64_t len;
} HcommBuf;

typedef struct {
    uint32_t sdid;
    int32_t pid;
} HcommMemGrantInfo;

HcclResult HcommResMgrInit(uint32_t devPhyId);

HcclResult HcommEndpointCreate(const EndpointDesc *endpoint, EndpointHandle *endpointHandle);

HcclResult HcommEndpointGet(const EndpointHandle endpointHandle, void **endpoint);

HcclResult HcommEndpointDestroy(EndpointHandle endpointHandle);

/**
 * @brief 通信设备Endpoint监听配置结构体
 */
typedef struct {
    union {
        uint8_t raws[24]; ///< 通用数据区，用于未来扩展，如backlog, timeout等
        struct {
        };
    };
} HcommEndpointListenConfig;

/**
 * @brief 启动通信设备Endpoint监听
 * @param[in] endpointHandle Endpoint句柄
 * @param[in] port 监听端口号
 * @param[in] config 监听配置参数（可为NULL，使用默认配置）
 * @return HcclResult 执行结果状态码
 * @note 启动指定Endpoint在指定端口上的监听服务
 */
extern HcclResult HcommEndpointStartListen(EndpointHandle endpointHandle, uint32_t port, HcommEndpointListenConfig* config);

/**
 * @brief 停止通信设备Endpoint监听
 * @param[in] endpointHandle Endpoint句柄
 * @param[in] port 监听端口号
 * @return HcclResult 执行结果状态码
 * @note 停止指定Endpoint在指定端口上的监听服务
 */
extern HcclResult HcommEndpointStopListen(EndpointHandle endpointHandle, uint32_t port);

HcclResult HcommMemReg(EndpointHandle endpointHandle, const char *memTag, HcommMem mem, void **memHandle);

HcclResult HcommMemUnreg(EndpointHandle endpointHandle, void *memHandle);

HcclResult HcommMemExport(EndpointHandle endpointHandle, void *memHandle, void **memDesc, uint32_t *memDescLen);

HcclResult HcommMemImport(EndpointHandle endpointHandle, const void *memDesc, uint32_t descLen, HcommMem *outMem);

HcclResult HcommMemUnimport(EndpointHandle endpointHandle, const void *memDesc, uint32_t descLen);

HcclResult HcommChannelCreate(EndpointHandle endpointHandle, CommEngine engine, HcommChannelDesc *channelDescs,
    uint32_t channelNum, ChannelHandle *channels);

HcclResult HcommChannelGet(const ChannelHandle channelHandle, void **channel);

HcclResult HcommChannelGetStatus(const ChannelHandle *channelList, uint32_t listNum,  int32_t* statusList);

HcclResult HcommChannelGetNotifyNum(ChannelHandle channelHandle, uint32_t *notifyNum);

HcclResult HcommChannelDestroy(const ChannelHandle *channels, uint32_t channelNum);

HcclResult HcommChannelKernelLaunch(ChannelHandle *channelHandles, ChannelHandle *hostChannelHandles, uint32_t listNum,
    const std::string &commTag, aclrtBinHandle binHandle);

HcclResult HcommThreadAlloc(CommEngine engine, uint32_t threadNum, uint32_t notifyNumPerThread, ThreadHandle *threads);

HcclResult HcommThreadFree(const ThreadHandle *threads, uint32_t threadNum);

HcclResult HcommThreadAllocWithStream(CommEngine engine, rtStream_t stream, uint32_t notifyNum, ThreadHandle *thread);

HcclResult HcommEngineCtxCreate(CommEngine engine, uint64_t size, void **ctx);

HcclResult HcommEngineCtxDestroy(CommEngine engine, void *ctx);

HcclResult HcommEngineCtxCopy(CommEngine engine, void *dstCtx, const void *srcCtx, uint64_t size);


// C函数
HcclResult HcommDfxKernelLaunch(const std::string &commTag, aclrtBinHandle binHandle, HcclDfxOpInfo dfxOpInfo);
HcclResult HcommMemGetAllMemHandles(EndpointHandle endpointHandle, void **memHandles, uint32_t *memHandleNum);

HcclResult HcommCollectiveChannelCreate(EndpointHandle endpointHandle, CommEngine engine, 
    HcommChannelDesc *channelDescs, uint32_t channelNum, ChannelHandle *channels);

#ifdef __cplusplus
}
#endif // __cplusplus


#endif
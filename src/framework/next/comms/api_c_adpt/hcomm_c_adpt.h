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

#include "hcomm_res.h"
#include "hccl/hccl_res.h"
#include "mem_host_pub.h"
#include "hccl_diag.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

typedef struct {
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

/**
 * @brief 内部兼容内存描述结构体
 */
typedef struct {
    HcclMemType type;
    void *addr;
    uint64_t size;
} HcommMem;

typedef HcommMemHandle MemHandle;

typedef struct {
    uint32_t sdid;
    int32_t pid;
} HcommMemGrantInfo;

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

HcclResult HcommResMgrInit(uint32_t devPhyId);

HcommResult HcommEndpointGet(EndpointHandle endpointHandle, void **endpoint);

/**
 * @brief 启动通信设备Endpoint监听
 * @param[in] endpointHandle Endpoint句柄
 * @param[in] port 监听端口号
 * @param[in] config 监听配置参数（可为NULL，使用默认配置）
 * @return HcclResult 执行结果状态码
 * @note 启动指定Endpoint在指定端口上的监听服务
 */
extern HcclResult HcommEndpointStartListen(EndpointHandle endpointHandle, uint32_t port,
    HcommEndpointListenConfig *config);

/**
 * @brief 停止通信设备Endpoint监听
 * @param[in] endpointHandle Endpoint句柄
 * @param[in] port 监听端口号
 * @return HcclResult 执行结果状态码
 * @note 停止指定Endpoint在指定端口上的监听服务
 */
extern HcclResult HcommEndpointStopListen(EndpointHandle endpointHandle, uint32_t port);

HcommResult HcommChannelGet(ChannelHandle channelHandle, void **channel);

HcclResult HcommChannelGetRemoteMem(ChannelHandle channelHandle, HcommMem **remoteMem, uint32_t *memNum,
    char **memTags);

HcclResult HcommChannelKernelLaunch(ChannelHandle *channelHandles, ChannelHandle *hostChannelHandles, uint32_t listNum,
    const std::string &commTag, aclrtBinHandle binHandle);

HcclResult HcommThreadAllocWithStream(CommEngine engine, rtStream_t stream, uint32_t notifyNum, ThreadHandle *thread);

HcclResult HcommEngineCtxCreate(CommEngine engine, uint64_t size, void **ctx);

HcclResult HcommEngineCtxDestroy(CommEngine engine, void *ctx);

HcclResult HcommEngineCtxCopy(CommEngine engine, void *dstCtx, const void *srcCtx, uint64_t size);

HcclResult HcommDfxKernelLaunch(const std::string &commTag, aclrtBinHandle binHandle, HcclDfxOpInfo dfxOpInfo);

HcclResult HcommMemGetAllMemHandles(EndpointHandle endpointHandle, void **memHandles, uint32_t *memHandleNum);

HcommResult HcommCollectiveChannelCreate(EndpointHandle endpointHandle, CommEngine engine,
    HcommChannelDesc *channelDescs, uint32_t channelNum, ChannelHandle *channels);

#ifdef __cplusplus
}

static inline HcommResult HcommMemReg(EndpointHandle endpointHandle, const char *memTag, HcommMem mem,
    void **memHandle)
{
    CommMem commMem = {
        static_cast<CommMemType>(mem.type),
        mem.addr,
        mem.size
    };
    return HcommMemReg(endpointHandle, memTag, &commMem, reinterpret_cast<HcommMemHandle *>(memHandle));
}

static inline HcommResult HcommMemImport(EndpointHandle endpointHandle, const void *memDesc, uint32_t descLen,
    HcommMem *outMem)
{
    CommMem commMem{};
    const HcommResult ret = HcommMemImport(endpointHandle, memDesc, descLen, &commMem);
    if (ret != HCOMM_SUCCESS) {
        return ret;
    }
    if (outMem != nullptr) {
        outMem->type = static_cast<HcclMemType>(commMem.type);
        outMem->addr = commMem.addr;
        outMem->size = commMem.size;
    }
    return HCOMM_SUCCESS;
}

static inline HcommResult HcommThreadAlloc(CommEngine engine, uint32_t threadNum, uint32_t notifyNumPerThread,
    ThreadHandle *threads)
{
    return HcommThreadAlloc(engine, threadNum, &notifyNumPerThread, threads);
}

static inline HcommResult HcommThreadAlloc(CommEngine engine, uint32_t threadNum, int notifyNumPerThread,
    ThreadHandle *threads)
{
    uint32_t notifyNum = static_cast<uint32_t>(notifyNumPerThread);
    return HcommThreadAlloc(engine, threadNum, &notifyNum, threads);
}

#endif // __cplusplus

#endif // HCOMM_C_ADPT_H

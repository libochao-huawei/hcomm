/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef HCOMM_NIC_PLUGIN_H
#define HCOMM_NIC_PLUGIN_H

#include <stdint.h>

#include "hcomm_primitives.h"
#include "hcomm_res.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HCOMM_NIC_PLUGIN_ABI_VERSION 1U
#define HCOMM_NIC_PLUGIN_MAX_PROTOCOLS 4U

typedef enum {
    HCOMM_NIC_PROTO_HCCS = 0,
    HCOMM_NIC_PROTO_ROCE = 1,
    HCOMM_NIC_PROTO_PCIE = 2,
    HCOMM_NIC_PROTO_SIO = 3,
    HCOMM_NIC_PROTO_UBC_CTP = 4,
    HCOMM_NIC_PROTO_UBC_TP = 5,
    HCOMM_NIC_PROTO_UB_MEM = 6,
    HCOMM_NIC_PROTO_UBOE = 7,
    HCOMM_NIC_PROTO_HCCS_ONLY = 8,
    HCOMM_NIC_PROTO_CUSTOM_BASE = 1000,
} HcommNicProtocol;

#ifndef HCOMM_MEM_GRANT_INFO_DEFINED
#define HCOMM_MEM_GRANT_INFO_DEFINED
typedef struct {
    uint32_t sdid;
    int32_t pid;
} HcommMemGrantInfo;
#endif

#ifndef HCOMM_ENDPOINT_LISTEN_CONFIG_DEFINED
#define HCOMM_ENDPOINT_LISTEN_CONFIG_DEFINED
typedef struct {
    union {
        uint8_t raws[24];
        struct {
        };
    };
} HcommEndpointListenConfig;
#endif

typedef struct HcommNicEndpointOps {
    int32_t (*init)(void *ctx);
    int32_t (*registerMemory)(void *ctx, const CommMem *mem, const char *tag, void **handle);
    int32_t (*unregisterMemory)(void *ctx, void *handle);
    int32_t (*memoryExport)(void *ctx, void *handle, void **desc, uint32_t *descLen);
    int32_t (*memoryImport)(void *ctx, const void *desc, uint32_t descLen, CommMem *outMem);
    int32_t (*memoryUnimport)(void *ctx, const void *desc, uint32_t descLen);
    void (*destroy)(void *ctx);
    int32_t (*startListen)(void *ctx, uint32_t port, HcommEndpointListenConfig *config);
    int32_t (*stopListen)(void *ctx, uint32_t port);
    int32_t (*getListenPort)(void *ctx, uint32_t *port);
    int32_t (*getAllMemoryHandles)(void *ctx, void **memHandles, uint32_t *memHandleNum);
    int32_t (*grantMemory)(void *ctx, const HcommMemGrantInfo *remoteGrantInfo);
} HcommNicEndpointOps;

typedef struct HcommNicChannelOps {
    int32_t (*init)(void *ctx);
    int32_t (*getNotifyNum)(void *ctx, uint32_t *num);
    int32_t (*getRemoteMem)(void *ctx, CommMem **mem, uint32_t *num, char **tags);
    int32_t (*clean)(void *ctx);
    int32_t (*write)(void *ctx, ThreadHandle thread, void *dst, const void *src, uint64_t len);
    int32_t (*writeReduce)(void *ctx, ThreadHandle thread, void *dst, const void *src, uint64_t count,
        int32_t dataType, int32_t reduceOp);
    int32_t (*writeWithNotify)(void *ctx, ThreadHandle thread, void *dst, const void *src, uint64_t len,
        uint32_t remoteNotifyIdx);
    int32_t (*writeReduceWithNotify)(void *ctx, ThreadHandle thread, void *dst, const void *src, uint64_t count,
        int32_t dataType, int32_t reduceOp, uint32_t remoteNotifyIdx);
    int32_t (*read)(void *ctx, ThreadHandle thread, void *dst, const void *src, uint64_t len);
    int32_t (*readReduce)(void *ctx, ThreadHandle thread, void *dst, const void *src, uint64_t count,
        int32_t dataType, int32_t reduceOp);
    int32_t (*writeNbi)(void *ctx, ThreadHandle thread, void *dst, const void *src, uint64_t len);
    int32_t (*writeWithNotifyNbi)(void *ctx, ThreadHandle thread, void *dst, const void *src, uint64_t len,
        uint32_t remoteNotifyIdx);
    int32_t (*readNbi)(void *ctx, ThreadHandle thread, void *dst, const void *src, uint64_t len);
    int32_t (*batchTransfer)(void *ctx, ThreadHandle thread, const HcommBatchTransferDesc *descs, uint32_t num);
    int32_t (*notifyRecord)(void *ctx, ThreadHandle thread, uint32_t remoteNotifyIdx);
    int32_t (*notifyWait)(void *ctx, ThreadHandle thread, uint32_t localNotifyIdx, uint32_t timeout);
    int32_t (*fence)(void *ctx, ThreadHandle thread);
    int32_t (*writeNbiNt)(void *ctx, void *dst, const void *src, uint64_t len);
    int32_t (*writeWithNotifyNbiNt)(void *ctx, void *dst, const void *src, uint64_t len, uint32_t remoteNotifyIdx);
    int32_t (*readNbiNt)(void *ctx, void *dst, const void *src, uint64_t len);
    int32_t (*notifyRecordNt)(void *ctx, uint32_t remoteNotifyIdx);
    int32_t (*notifyWaitNt)(void *ctx, uint32_t localNotifyIdx, uint32_t timeout);
    int32_t (*fenceNt)(void *ctx);
    void (*destroy)(void *ctx);
    int32_t (*getStatus)(void *ctx, int32_t *status);
    int32_t (*updateMemInfo)(void *ctx, HcommMemHandle *memHandles, uint32_t memHandleNum);
    int32_t (*getUserRemoteMem)(void *ctx, CommMem **mem, char ***tags, uint32_t *num);
} HcommNicChannelOps;

typedef struct HcommNicPluginInfo {
    uint32_t abiVersion;
    const char *name;
    uint32_t protocolCount;
    HcommNicProtocol protocols[HCOMM_NIC_PLUGIN_MAX_PROTOCOLS];
} HcommNicPluginInfo;

typedef const HcommNicPluginInfo *(*HcommNicPluginGetInfo_t)(void);
typedef int32_t (*HcommNicPluginCreateEndpoint_t)(
    const void *endpointDescRaw, uint32_t epDescLen, void **outCtx, HcommNicEndpointOps **outOps);
typedef int32_t (*HcommNicPluginCreateChannel_t)(
    void *epCtx, const void *channelDescRaw, uint32_t chDescLen, void **outCtx, HcommNicChannelOps **outOps);

const HcommNicPluginInfo *HcommNicPluginGetInfo(void);
int32_t HcommNicPluginCreateEndpoint(
    const void *endpointDescRaw, uint32_t epDescLen, void **outCtx, HcommNicEndpointOps **outOps);
int32_t HcommNicPluginCreateChannel(
    void *epCtx, const void *channelDescRaw, uint32_t chDescLen, void **outCtx, HcommNicChannelOps **outOps);

#ifdef __cplusplus
}
#endif

#endif

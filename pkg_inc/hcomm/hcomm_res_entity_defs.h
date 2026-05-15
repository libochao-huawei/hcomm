
/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef HCOMM_RES_ENTITY_DEFS_H
#define HCOMM_RES_ENTITY_DEFS_H

#include <stdint.h>
#include "hcomm_res_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint64_t ChannelPtr;

typedef enum {
    PROTECTION_TYPE_RDMA = 0,
    PROTECTION_TYPE_URMA = 1,
} ProtectionType;

typedef enum {
    SQ_CONTEXT_TYPE_JFS = 0,
    SQ_CONTEXT_TYPE_RDMA = 1,
} SqContextType;

typedef enum {
    CQ_CONTEXT_TYPE_JFC = 0,
    CQ_CONTEXT_TYPE_RDMA = 1,
} CqContextType;

typedef enum {
    REGED_NOTIFY_IPC_RT = 0,
    REGED_NOTIFY_IPC_MEM = 1,
    REGED_NOTIFY_RMA_RT = 2,
    REGED_NOTIFY_RMA_MEM = 3,
} RegedNotifyType;

typedef enum {
    REGED_BUFFER_IPC = 0,
    REGED_BUFFER_RMA = 1,
} RegedBufferType;

typedef struct {
    ProtectionType type;
    union {
        struct {
            uint32_t lkey;
            uint32_t rkey;
        } rdmaMemInfo;
        struct {
            uint32_t tokenId;
            uint32_t tokenValue;
        } urmaMemInfo;
        int8_t reserve[24];
    } memInfo;
} ProtectionInfo; // 32B

typedef struct {
    RegedBufferType type;
    union {
        struct {
            uint64_t address;
            uint32_t size;
        } ipcBuffer;
        struct {
            uint64_t address;
            uint32_t size;
            ProtectionInfo protectionInfo;
        } rmaBuffer;
        int8_t reserve[56];
    } bufferInfo;
} RegedBufferEntity; // 64B

typedef struct {
    RegedNotifyType type;
    union {
        struct {
            uint64_t address;
            uint32_t size;
            int32_t notifyId;
        } ipcRtNotify;
        struct {
            uint64_t address;
            uint32_t size;
        } ipcMemNotify;
        struct {
            uint64_t address;
            uint32_t size;
            int32_t notifyId;
            ProtectionInfo protectionInfo;
        } rmaRtNotify;
        struct {
            uint64_t address;
            uint32_t size;
            ProtectionInfo protectionInfo;
        } rmaMemNotify;
        int8_t reserve[56];
    } notifyInfo;
} RegedNotifyEntity; // 64B

typedef struct {
    SqContextType type;
    union {
        struct {
            uint64_t sqVa;
            uint64_t headAddr;
            uint64_t tailAddr;
            uint64_t dbVa;
            uint32_t jfsID;
            uint32_t wqeSize;
            uint32_t sqDepth;
            uint32_t tpID;
            uint8_t remoteEID[16];
        } jfsContext;
        struct {
            uint64_t sqVa;
            uint64_t headAddr;
            uint64_t tailAddr;
            uint64_t dbVa;
            uint32_t qpn;
            uint32_t wqeSize;
            uint32_t depth;
            int8_t dbMode;
            uint8_t sl;
        } rdmaSqContext;
        int8_t reserve[128];
    } contextInfo;
} SqContext;

typedef struct {
    CqContextType type;
    union {
        struct {
            uint64_t scqVa;
            uint64_t headAddr;
            uint64_t tailAddr;
            uint64_t dbVa;
            uint32_t jfcID;
            uint32_t cqeSize;
            uint32_t cqDepth;
        } jfcContext;
        struct {
            uint64_t cqVa;
            uint64_t headAddr;
            uint64_t tailAddr;
            uint64_t dbVa;
            uint32_t cqn;
            uint32_t cqeSize;
            uint32_t cqDepth;
            int8_t dbMode;
        } rdmaCqContext;
        int8_t reserve[128];
    } contextInfo;
} CqContext;

typedef struct {
    CommAbiHeader abiHeader;
    CommEngine engine;
    CommProtocol protocol;
    uint32_t localNotifyNum;
    uint32_t remoteNotifyNum;
    uint32_t localBufferNum;
    uint32_t remoteBufferNum;
    uint32_t sqNum;
    uint32_t cqNum;
    RegedNotifyEntity *localNotifyAddr;
    RegedNotifyEntity *remoteNotifyAddr;
    RegedBufferEntity *localBufferAddr;
    RegedBufferEntity *remoteBufferAddr;
    SqContext *sqContextAddr;
    CqContext *cqContextAddr;
    uint8_t reserve[160];
} ChannelEntity; // 256B

#ifdef __cplusplus
}
#endif

#endif // HCOMM_RES_ENTITY_DEFS_H
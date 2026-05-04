
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

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

typedef void* ChannelEntityHandle;
typedef void* ChannelPtr;

struct ProtectionInfo {
    uint32_t type; // 0 RDMA   1 URMA
    uint64_t addr;
    uint64_t length;
    union MemInfo {
        struct RdmaMemProtectionInfo {
            uint32_t lkey;
            uint32_t rkey;
        } rdmaMemInfo;
        struct UrmaMemProtectionInfo {
            uint32_t tokenID;
            uint32_t tokenValue;
        } urmaMemInfo;
        int8_t reserve[32];
    } memInfo;
};

struct SqContext {
    uint32_t type;
    union ContextInfo {
        struct JfsContext {
            uint32_t jfsID;
            uint64_t sqVa;
            uint32_t wqeSize;
            uint32_t sqDepth;
            uint32_t tpID;
            uint64_t headAddr;
            uint64_t tailAddr;
            uint8_t remoteEID[16];
            uint64_t dbVa;
        } jfsContext; // 48+16= 64 Bytes
        struct RdmaSqContext {
            uint32_t qpn;
            uint64_t sqVa;
            uint32_t wqeSize;
            uint32_t depth;
            uint64_t headAddr;
            uint64_t tailAddr;
            uint8_t sl;
            uint64_t dbVa;
            int8_t dbMode; // 0-hw/1-sw
        } rdmaSqContext;   // 46 Bytes
        int8_t reserve[128];
    } contextInfo;
};

typedef struct {
    uint32_t type;
    union ContextInfo {
        struct JfcContext {
            uint32_t jfcID;
            uint64_t scqVa;
            uint32_t cqeSize;
            uint32_t cqDepth;
            uint64_t headAddr;
            uint64_t tailAddr;
            uint64_t dbVa;
        } jfcContext;
        struct RdmaCqContext {
            uint32_t cqn;
            uint64_t cqVa;
            uint32_t cqeSize;
            uint32_t cqDepth;
            uint64_t headAddr;
            uint64_t tailAddr;
            uint64_t dbVa;
            int8_t dbMode; // 0-hw/1-sw
        } rdmaCqContext;
        int8_t reserve[128];
    } contextInfo;
} CqContext;

struct Notify {
    uint32_t type;
    union NotifyInfo {
        struct HccsNotify {
            uint64_t address;
            int32_t notifyId;
            uint32_t size; // 默认4Byte
        } hccsNotify;
        struct RmaNotify {
            uint64_t address;
            int32_t notifyId; // remote 时不用
            uint32_t size;    // 默认4Byte
            ProtectionInfo protectionInfo;
        } rmaNotify;
        int8_t reserve[64];
    } notifyInfo;
};

struct ChannelEntity {
    CommAbiHeader abiHeader;
    CommEngine engine;
    CommProtocol protocol;
    // local notify
    uint32_t localNotifyNum;
    Notify *localNotifyAddr;
    // remote notify
    uint32_t remoteNotifyNum;
    Notify *remoteNotifyAddr;
    // Local User Reg buffer
    uint32_t localBufferNum;
    ProtectionInfo *localBufferAddr;
    // Remote User Reg buffer
    uint32_t remoteBufferNum;
    ProtectionInfo *remoteBufferAddr;
    // SQ
    uint32_t sqNum;
    SqContext *SqContextAddr;
    // CQ
    uint32_t cqNum;
    CqContext *CqContextAddr;
    uint8_t reserve[1024];
};

extern HcommResult HcommChannelGetPtrByHandle(const ChannelHandle *channelList, uint32_t listNum, ChannelPtr *channelPtr);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif

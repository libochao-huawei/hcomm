/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef HCCL_EVELUATOR_API_H
#define HCCL_EVELUATOR_API_H

#include "hccl_types.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

typedef enum {
    HCCL_OP_All_TO_ALL = 0,         /**< all to all */
    HCCL_OP_All_GATHER = 1,         /**< all gather */
    HCCL_OP_All_REDUCE = 2,         /**< all reduce */
    HCCL_OP_BROADCAST = 3,          /**< broadcast */
    HCCL_OP_REDUCE_SCATTER = 4,     /**< reduce-scatter */
    HCCL_OP_REDUCE = 5,             /**< reduce */
    HCCL_OP_NUM
} HcclOpType;

typedef enum {
    HCCL_PROTOCOL_SOCKET = 0,
    HCCL_PROTOCOL_RDMA = 1,
    HCCL_PROTOCOL_SHM_CPU = 2,      /**< share memory copyed by CPU */
    HCCL_PROTOCOL_SHM_DMA = 3,      /**< share memory copyed by DMA */
    HCCL_PROTOCOL_NUM
} HcclProtocol;

typedef struct tagHcclPort {
    HcclProtocol protocol;
    int32_t portId;
    float bandWidth;          // MB/s
    uint32_t staticDelay;        // us
    struct tagHcclPort* next;
} HcclPort;

typedef struct tagHcclConnection {
    uint32_t peerNodeId;
    uint32_t isIntraOS;          // whether peer is in the same OS or not
    HcclPort* port;
    struct tagHcclConnection* next;
} HcclConnection;

typedef struct tagHcclNode {
    uint32_t nodeId;             // unique in  input topology
    HcclConnection* connectionList;
    struct tagHcclNode* next;
} HcclNode;

typedef struct {
    HcclNode* nodeList;
    int32_t tag;
    uint32_t nodeNum;
} HcclTopology;

typedef struct tagHcclCollective {
    HcclNode* nodeList;
} HcclCollective;

typedef struct {
    HcclOpType op;
    HcclDataType dataType;   // data type of input
    unsigned long count;    // input data count of this op
    uint32_t root;               // if op need root, specified here
    uint32_t jitter;             // us, default = 0
    int32_t rsv0;
    int32_t rsv1;
} HcclOpInfo;

HcclResult HcclEvaluateTimeCostRoughly(HcclOpInfo* op, HcclTopology* topo, float* cost);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif
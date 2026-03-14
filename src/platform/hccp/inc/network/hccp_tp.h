/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef HCCP_TP_H
#define HCCP_TP_H

#include "hccp_common.h"

#ifdef __cplusplus
extern "C" {
#endif
/**
 * @ingroup libinit
 * rdma/ub gid/eid
 */
union HccpEid {
    uint8_t raw[16U]; /* Network Order */
    struct {
        uint64_t reserved; /* If IPv4 mapped to IPv6, == 0 */
        uint32_t prefix;   /* If IPv4 mapped to IPv6, == 0x0000ffff */
        uint32_t addr;     /* If IPv4 mapped to IPv6, == IPv4 addr */
    } in4;
    struct {
        uint64_t subnetPrefix;
        uint64_t interfaceId;
    } in6;
};

enum TransportModeT {
    CONN_RM = 1, /**< only for UB, Reliable Message */
    CONN_RC = 2, /**< Reliable Connection */
};

union GetTpCfgFlag {
    struct {
        uint32_t ctp : 1;
        uint32_t rtp : 1;
        uint32_t utp : 1;
        uint32_t uboe : 1;
        uint32_t preDefined : 1;
        uint32_t dynamicDefined : 1;
        uint32_t reserved : 26;
    } bs;
    uint32_t value;
};

#pragma pack(1)
struct TpAttr {
    uint8_t retryTimesInit : 3; // corresponding bitmap bit: 0
    uint8_t at : 5; // corresponding bitmap bit: 1
    uint8_t sip[16U]; // corresponding bitmap bit: 2
    uint8_t dip[16U]; // corresponding bitmap bit: 3
    uint8_t sma[6U]; // corresponding bitmap bit: 4
    uint8_t dma[6U]; // corresponding bitmap bit: 5
    uint16_t vlanId : 12; // corresponding bitmap bit: 6
    uint8_t vlanEn : 1; // corresponding bitmap bit: 7
    uint8_t dscp : 6; // corresponding bitmap bit: 8
    uint8_t atTimes : 5; // corresponding bitmap bit: 9
    uint8_t sl : 4; // corresponding bitmap bit: 10
    uint8_t ttl; // corresponding bitmap bit: 11
    uint8_t reserved[78];
};
#pragma pack()

struct GetTpCfg {
    union GetTpCfgFlag flag;
    enum TransportModeT transMode;
    union HccpEid localEid;
    union HccpEid peerEid;
};

#ifdef __cplusplus
}
#endif

#endif // HCCP_TP_H

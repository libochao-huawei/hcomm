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

#define TP_ATTR_SIP_MASK 0x4
#define TP_ATTR_DIP_MASK 0x8
#define TP_ATTR_SMAC_MASK 0x10
#define TP_ATTR_DMAC_MASK 0x20

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
        uint32_t udp : 5;
        uint32_t groupId : 15;
        uint32_t reserved : 6;
    } bs;
    uint32_t value;
};

#pragma pack(1)
/** 与 ubengine `urma_tp_attr_value_t` 布局、`#pragma pack(1)` 一致（HCCP 侧字段名为驼峰），供 RsUbGetTpAttr / RsUbSetTpAttr 与 URMA 强转。 */
struct TpAttr {
    uint8_t retry_times_init : 3;
    uint8_t at : 5;
    uint8_t sip[16U];
    uint8_t dip[16U];
    uint8_t sma[6U];
    uint8_t dma[6U];
    uint16_t vlan_id : 12;
    uint8_t vlan_en : 1;
    uint8_t dscp : 6;
    uint8_t at_times : 5;
    uint8_t sl : 4;
    uint8_t ttl;
    uint16_t ack_udp_srcport;
    uint16_t data_udp_srcport;
    uint8_t udp_srcport_range : 4;
    uint8_t spray_en : 1;
    uint8_t udp_global_en : 1;
    uint8_t reserve_0 : 2;
    uint16_t slBitmap; /* RaGetTpAttr：若扩展位按成员递增，常见为 tp_attr_bitmap bit 18（见 tp_mgr kTpAttrSlAvailableBit） */
    uint8_t dscpConfigMode : 1;
    uint8_t reserve1 : 7;
    uint8_t reserved[70];
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

/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef _HNS_ROCE_AH_H_
#define _HNS_ROCE_AH_H_

#ifndef DEFINE_HNS_LLT
#define STATIC static
#else
#define STATIC
#endif

#define HNS_ROCE_PORT_NUM_SHIFT     24
#define HNS_ROCE_VLAN_SL_BIT_MASK   7
#define HNS_ROCE_VLAN_SL_SHIFT      13
#define HNS_ROCE_MAX_VLAN_NUM       0x1000
#define HNS_ROCE_INVALID_VLAN_ID    0xffff
#define DSCP_SHIFT 2

#endif // _HNS_ROCE_AH_H_


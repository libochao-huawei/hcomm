/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef RA_TLV_H
#define RA_TLV_H

#include "hccp_tlv.h"

struct ra_tlv_handle {
    unsigned int module_type;
    struct ra_tlv_ops *tlv_ops;
    struct tlv_init_info init_info;
    unsigned int buffer_size;
};

struct ra_tlv_ops {
    int (*ra_tlv_init)(struct ra_tlv_handle *tlvHandle);
    int (*ra_tlv_deinit)(struct ra_tlv_handle *tlvHandle);
    int (*ra_tlv_request)(struct ra_tlv_handle *tlvHandle, struct tlv_msg *sendMsg, struct tlv_msg *recvMsg);
};
#endif // RA_TLV_H

/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef _HNS_ROCE_QP_H_
#define _HNS_ROCE_QP_H_
#include "hns_roce_device.h"

#ifdef DEFINE_HNS_LLT
#undef static
#define static
#endif

#define SQP_NUM             (2 * HNS_ROCE_MAX_PORTS)

struct hns_roce_ib_info {
    struct ib_udata *udata;
    struct hns_roce_ib_create_qp_resp *resp;
    struct hns_roce_ib_create_qp ucmd;
};

#endif


/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef _HNS_ROCE_MAIN_H_
#define _HNS_ROCE_MAIN_H_
#include "roce_k_compat.h"

#define PGSZ_BASE        2
#define VM_PGOFF_VAL     3
#if defined(HNS_ROCE_DEVICE) && !defined(CONFIG_PLATFORM_ESL)
#else
#define HNS_ROCE_NOTIFY_BASE_ADDR       (0xAF100000ULL + 0xE000)
#endif

#endif // _HNS_ROCE_MAIN_H_

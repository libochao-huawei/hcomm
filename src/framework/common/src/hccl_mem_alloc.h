/*
 * Copyright (c) 2025 Huawei Technologies Co., Ltd. All Rights Reserved.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef HCCL_MEM_ALLOC_H
#define HCCL_MEM_ALLOC_H

#include <hccl_comm.h>
#include "hccl_comm_pub.h"
#include "config.h"

#define ALIGN_SIZE(size, align) \
	({ \
        (size) = (((size) + (align) - 1) / (align)) * (align);\
	})

#endif // HCCL_MEM_ALLOC_H
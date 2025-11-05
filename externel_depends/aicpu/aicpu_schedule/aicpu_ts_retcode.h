/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef AICPU_TS_RETCODE_H
#define AICPU_TS_RETCODE_H

namespace aicpu {
    // ret code for aicpu to ts
    enum class AICPU_TS_RETCODE {
        // ret code start with 0x07, previous has been used.
        TASK_STATE_AICPU_PHASE2_TIMEOUT = 0x07,
    };
} // namespace aicpu

#endif // AICPU_TS_RETCODE_H
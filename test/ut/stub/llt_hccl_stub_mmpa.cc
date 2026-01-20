/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "mmpa_api.h"

INT32 mmGetEnv(const CHAR *name, CHAR *value, UINT32 len)
{
    INT32 ret;
    UINT32 envLen = 0;
    if ((name == NULL) || (value == NULL) || (len == MMPA_ZERO)) {
        return EN_INVALID_PARAM;
    }
    const CHAR *envPtr = getenv(name);
    if (envPtr == NULL) {
        return EN_ERROR;
    }

    UINT32 lenOfRet = (UINT32)strlen(envPtr);
    if (lenOfRet < (UINT32)(MMPA_MEM_MAX_LEN - 1)) {
        envLen = lenOfRet + 1U;
    }

    if ((envLen != MMPA_ZERO) && (len < envLen)) {
        return EN_INVALID_PARAM;
    } else {
        ret = memcpy_s(value, len, envPtr, envLen);
        if (ret != EN_OK) {
            return EN_ERROR;
        }
    }
    return EN_OK;
}

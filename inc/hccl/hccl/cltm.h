/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef CLTM_INC_H
#define CLTM_INC_H

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/**
 * @brief CLTM functions return value definition
 */
typedef enum tagCltmResult {
    CLTM_SUCCESS = 0,    /**< success */
    CTLM_E_PTR,          /**< empty pointer */
    CLTM_E_PARA,         /**< parameter error */
    CLTM_E_NO_RESOURCE,  /**< resource not enough error */
    CLTM_E_RESERVED      /**< reserved */
} cltmResult_t;

/**
 * @brief Generate rank table
 *
 * @param allocatedResource A string identifying the resource list allocate by the CSM.
 * @param rankTableBuf A string identifying the buffer of .
 * @param maxBufSize An integer(u32)  identifying the size of rank table buffer.
 * @param usedBufSize A pointer identifying the used size of rank table buffer.
 * @return cltmResult_t
 */
cltmResult_t cltm_generate_ranktable(const char *allocatedResource, char* rankTableBuf,
    unsigned int maxBufSize, unsigned int *usedBufSize);

#ifdef __cplusplus
}
#endif
#endif // CLTM_INC_H

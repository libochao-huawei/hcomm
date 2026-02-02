/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef _TC_UT_RS_UB_H
#define _TC_UT_RS_UB_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

void tc_rs_ub_ctx_ext_jetty_create();
void tc_rs_ub_ctx_ext_jetty_delete();
void tc_rs_ub_stars_va_munmap_batch();

void tc_rs_ub_ctx_jfc_create_ext();
void tc_rs_ub_delete_jfc_ext();

#ifdef __cplusplus
}
#endif
#endif

/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef DL_CCU_FUNCTION_H
#define DL_CCU_FUNCTION_H

#include "ccu_u_api.h"

struct rs_ccu_ops {
    int (*rs_ccu_init)(void);
    int (*rs_ccu_uninit)(void);
    int (*rs_ccu_custom_channel)(const struct channel_info_in *in, struct channel_info_out *out);
    unsigned long long (*rs_ccu_get_cqe_base_addr)(unsigned int die_id);
};

int rs_ccu_api_init(void);
void rs_ccu_api_deinit(void);

int rs_ccu_init(void);
int rs_ccu_uninit(void);
int rs_ccu_custom_channel(const struct channel_info_in *in, struct channel_info_out *out);
int rs_ccu_get_cqe_base_addr(unsigned int die_id, unsigned long long *cqe_base_addr);

#endif // DL_CCU_FUNCTION_H

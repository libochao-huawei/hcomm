/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 * Description: 获取动态库中ccu接口函数的适配接口私有头文件
 * Create: 2024-04-29
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

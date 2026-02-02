/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 * Description: 获取动态库中ccu接口函数的适配接口私有头文件
 * Create: 2024-04-29
 */

#ifndef DL_CCU_FUNCTION_H
#define DL_CCU_FUNCTION_H

#include "ccu_u_api.h"

struct RsCcuOps {
    int (*rsCcuInit)(void);
    int (*rsCcuUninit)(void);
    int (*rsCcuCustomChannel)(const struct channel_info_in *in, struct channel_info_out *out);
    unsigned long long (*rsCcuGetCqeBaseAddr)(unsigned int dieId);
};

int RsCcuApiInit(void);
void RsCcuApiDeinit(void);

int RsCcuInit(void);
int RsCcuUninit(void);
int RsCcuCustomChannel(const struct channel_info_in *in, struct channel_info_out *out);
int RsCcuGetCqeBaseAddr(unsigned int dieId, unsigned long long *cqeBaseAddr);

#endif // DL_CCU_FUNCTION_H

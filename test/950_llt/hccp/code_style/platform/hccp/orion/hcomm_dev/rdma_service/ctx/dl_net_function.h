/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * Description: 获取动态库中network 适配层接口函数的适配接口私有头文件
 * Create: 2025-12-9
 */

#ifndef DL_NET_FUNCTION_H
#define DL_NET_FUNCTION_H

struct RsNetOps {
    int (*rsNetAdaptInit)(void);
    void (*rsNetAdaptUninit)(void);
    int (*rsNetAllocJfcId)(const char *udevName, unsigned int jfcMode, unsigned int *jfcId);
    int (*rsNetFreeJfcId)(const char *udevName, unsigned int jfcMode, unsigned int jfcId);
    int (*rsNetAllocJettyId)(const char *udevName, unsigned int jettyMode, unsigned int *jettyId);
    int (*rsNetFreeJettyId)(const char *udevName, unsigned int jettyMode, unsigned int jettyId);
    unsigned long long (*rsNetGetCqeBaseAddr)(unsigned int dieId);
};

int RsNetApiInit(void);
void RsNetApiDeinit(void);

int RsNetAdaptInit(void);
void RsNetAdaptUninit(void);
int RsNetAllocJfcId(const char *udevName, unsigned int jfcMode, unsigned int *jfcId);
int RsNetFreeJfcId(const char *udevName, unsigned int jfcMode, unsigned int jfcId);
int RsNetAllocJettyId(const char *udevName, unsigned int jettyMode, unsigned int *jettyId);
int RsNetFreeJettyId(const char *udevName, unsigned int jettyMode, unsigned int jettyId);
int RsNetGetCqeBaseAddr(unsigned int dieId, unsigned long long *cqeBaseAddr);

#endif // DL_NET_FUNCTION_H

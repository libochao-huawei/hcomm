/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef _PCIE_NIC_
#define _PCIE_NIC_

#include <stdio.h>
#include "hal.h"

#ifdef __cplusplus
extern "C" {
#endif

int GetNpuRoceIp(int phy_id, char* ipaddr, size_t addrLen);

int isSamePcieSwitch(NPU *npu, HCA *nic);

/**
 * 扫描所有HCA
 * @param nics HCA数组指针
 * @param maxNicNum 最大HCA数量
 * @param nicNum 扫描到的HCA数量指针
 * @return 0 成功，-1 失败0
 */
int scanHca(HCA *nics, int maxNicNum, int *nicNum);

/**
 * 从文件中读取整数,用来读取numa
 * @param path 文件路径
 * @param value 读取的整数指针
 * @return 0 成功，-1 失败0
*/
int readIntFromFile(const char* path, int *value);

#ifdef __cplusplus
}
#endif

#endif // _PCIE_NIC_

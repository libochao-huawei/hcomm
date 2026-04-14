/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "pcie_nic.h"
#include <stdio.h>
#include <errno.h>
#include <libgen.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
#include <dlfcn.h>
#include "hal.h"
#include "securec.h"



static size_t getCommonPrefixLen(const char* a, const char* b)
{
    size_t len = 0;
    while (a[len] == b[len] && a[len] != '\0' && b[len] != '\0') {
        len++;
    }
    return len;
}

static int getAbsPath(const char* path, char* abs_path, size_t abs_path_len)
{
    char resolved_path[PATH_MAX] = {0};
    char *p = realpath(path, resolved_path);
    if (p == NULL) {
        return -1;
    }
    return strcpy_s(abs_path, abs_path_len, resolved_path);
}


int isSamePcieSwitch(NPU *npu, HCA *nic)
{
    char dev_path[MAX_NIC_PCIE_PATH] = {0};
    char abs_path[MAX_NIC_PCIE_PATH] = {0};
    size_t abs_path_len = sizeof(abs_path);
    int ret = sprintf_s(dev_path, sizeof(dev_path), "/sys/bus/pci/devices/%s", npu->bus_id);
    if (ret < 0) {
        return -1;
    }
    getAbsPath(dev_path, abs_path, abs_path_len);
    size_t common_prefix_len = getCommonPrefixLen(abs_path, nic->pciePath);
    if (common_prefix_len > PCIE_ROOT_MIN_LEN) {
        return 1;
    }
    return 0;
}

int getFileNameInDir(const char* path, char *name, size_t nameSize) 
{
    DIR *dir = opendir(path);
    if (dir == NULL) {
        return -1;
    }
    int loop = 0;
    struct dirent *entry = NULL;
    while ((entry=readdir(dir)) != NULL) {
        if (loop >= MAX_NIC_COUNT) {
            break;
        }
        if (entry->d_name[0] == '.') {
            continue;
        }
        int ret = strcpy_s(name, nameSize, entry->d_name);
        if (ret == 0) {
            break;
        }
        loop++;
    }
    return 0;
}

int GetHcaEth(HCA *hca) 
{
    char netPath[MAX_NIC_PCIE_PATH] = {0};
    sprintf_s(netPath, MAX_NIC_PCIE_PATH, "%s/net", hca->pciePath);
    return getFileNameInDir(netPath, hca->ethName, MAX_NIC_PCIE_PATH) ;
}

int GetHcaVerbs(HCA *hca) 
{
    char netPath[MAX_NIC_PCIE_PATH] = {0};
    sprintf_s(netPath, MAX_NIC_PCIE_PATH, "%s/infiniband_verbs", hca->pciePath);
    return getFileNameInDir(netPath, hca->verbsName, MAX_NIC_PCIE_PATH) ;
}

int InitHcaByName(const char* hcaName, HCA* hca)
{
    if (hca == NULL) {
        return -1;
    }
    strcpy_s(hca->hcaName, MAX_NIC_PCIE_PATH, hcaName);
    char devPath[MAX_NIC_PCIE_PATH] = {0};
    int ret = sprintf_s(devPath, MAX_NIC_PCIE_PATH, "/sys/class/infiniband/%s/device", hcaName);
    if (ret < 0) {
        return -1;
    }
    getAbsPath(devPath, hca->pciePath, MAX_NIC_PCIE_PATH);
    strcpy_s(hca->pcieBusId, MAX_NIC_PCIE_PATH, basename(hca->pciePath));
    GetHcaEth(hca);
    GetHcaVerbs(hca);

    // 检查verbs可见性，不存在说明没有挂载到容器内
    char verbsPath[MAX_NIC_PCIE_PATH] = {0};
    ret = sprintf_s(verbsPath, MAX_NIC_PCIE_PATH, "/dev/infiniband/%s", hca->verbsName);
    if (ret < 0) {
        return -1;
    }   
    if (access(verbsPath, F_OK) != 0) {
        return -1;
    }
    return 0;
}

/**
 * 获取本机所有网卡信息
 */
int scanHca(HCA *nics, int *nic_len)
{
    DIR *dir = opendir("/sys/class/infiniband");
    if (dir == NULL) {
        return -1;
    }
    const static int maxLoop = 100;  // 循环保护
    int loop = 0;  // 循环保护
    int pos = 0;
    struct dirent *entry = NULL;
    while ((entry=readdir(dir)) != NULL) {
        if (loop >= maxLoop || pos >= MAX_NIC_COUNT) {
            break;
        }
        loop++;
        if (entry->d_name[0] == '.') {
            continue;
        }
        int ret = InitHcaByName(entry->d_name, &nics[pos]);
        if (ret != 0) {
            continue;
        }
        pos++;
    }
    (*nic_len) = loop;
    closedir(dir);
    return 0;
}
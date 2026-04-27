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

/**
  * @brief 检查NPU和网卡是否在同一个PCIe交换机下
  * @note * 判断原理，比较两个两个pcie路径, 同一个PCIe交换机下的两个设备的的父节点的bdf号相同Function id不同,
  * 因此判断路径是否相同，减去自身bdf号长度和function id长度即可
  * 则判断结果为1，因为它们的父节点的bdf号相同Function id不同，减去自身bdf号长度和function id长度后，路径相同
  * 例如两个设备的pcie路径分别为
  * ../0000:01:01.0/0000:02:00.0
  * ../0000:01:02.0/0000:03:00.0
  * 则说明02和03在同一个PCIe switch 01下, 他们之间的最大不同的字符数为18
  * 另需排除掉pcie host bridge， 例如:
  * /sys/bus/pci/devices/0000:01:01.0/0000:02:00.0
  * /sys/bus/pci/devices/0000:01:01.0/0000:03:00.0
  * 这类型直接挂载在CPU host bridge的情况,这类行路径长度较短，最短为33
  * @param npu NPU指针
  * @param npu NPU指针
  * @param nic 网卡指针
  * @return int 1表示在同一个PCIe交换机下，0表示不在同一个PCIe交换机下
*/
int isSamePcieSwitch(NPU *npu, HCA *nic)
{
#define SAME_PCIE_SW_BDF_DIFF_LEN (18)
#define PCIE_HOST_BRIDGE_PATH_LEN (33)
    char dev_path[MAX_NIC_PCIE_PATH] = {0};
    char abs_path[MAX_NIC_PCIE_PATH] = {0};
    size_t abs_path_len = sizeof(abs_path);
    int ret = sprintf_s(dev_path, sizeof(dev_path), "/sys/bus/pci/devices/%s", npu->bus_id);
    if (ret < 0) {
        return -1;
    }
    getAbsPath(dev_path, abs_path, abs_path_len);
    if (strlen(abs_path) < PCIE_HOST_BRIDGE_PATH_LEN) {
        return 0;
    }
    size_t common_prefix_len = getCommonPrefixLen(abs_path, nic->pciePath);
    if (common_prefix_len >= strlen(abs_path) - SAME_PCIE_SW_BDF_DIFF_LEN
     && common_prefix_len >= PCIE_HOST_BRIDGE_PATH_LEN) {
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
    int ret = sprintf_s(netPath, MAX_NIC_PCIE_PATH, "%s/net", hca->pciePath);
    if (ret < 0) {
        return -1;
    }
    return getFileNameInDir(netPath, hca->ethName, MAX_NIC_PCIE_PATH) ;
}

/**
 * @brief 获取网卡的verbs名称
 * @param hca 网卡指针
 * @return int 0表示成功，-1表示失败
 */
int GetHcaVerbs(const char* hcaName, char* verbsName, size_t verbsNameLen) 
{
    char verbsPath[MAX_NIC_PCIE_PATH] = {0};
    int ret = sprintf_s(verbsPath, MAX_NIC_PCIE_PATH, "/sys/class/infiniband/%s/device/infiniband_verbs", hcaName);
    if (ret < 0) {
        return -1;
    }
    ret = getFileNameInDir(verbsPath, verbsName, verbsNameLen);
    if (ret != 0) {
        return -1;
    }
    // 检查verbs可见性，不存在说明没有挂载到容器内
    char devVerbsPath[MAX_NIC_PCIE_PATH] = {0};
    ret = sprintf_s(devVerbsPath, MAX_NIC_PCIE_PATH, "/dev/infiniband/%s", verbsName);
    if (ret < 0) {
        return -1;
    }   
    if (access(devVerbsPath, F_OK) != 0) {
        return -1;
    }
    return 0;
}

int readIntFromFile(const char* path, int *value)
{
    FILE *fp = fopen(path, "r");
    if (fp == NULL) {
        return -1;
    }
    int ret = fscanf(fp, "%d", value);
    if (ret != 1) {
        fclose(fp);
        return -1;
    }
    fclose(fp);
    return 0;
}

/**
 * @brief 获取网卡的numa节点
 * @param hca 网卡指针
 * @return int 0表示成功，-1表示失败
*/
static int GetHcaNuma(const char* hcaName, int *numa)
{
    char numaPathPcie[MAX_NIC_PCIE_PATH] = {0};
    char numaPathUB[MAX_NIC_PCIE_PATH] = {0};
    int ret = sprintf_s(numaPathPcie, MAX_NIC_PCIE_PATH, "/sys/class/infiniband/%s/device/numa_node", hcaName);
    if (ret < 0) {
        return -1;
    }
    ret = sprintf_s(numaPathUB, MAX_NIC_PCIE_PATH, "/sys/class/infiniband/%s/device/numa", hcaName);
    if (ret < 0) {
        return -1;
    }
    /* 先读取pcie numa_node */
    ret = readIntFromFile(numaPathPcie, numa);
    if (ret == 0) {
        return 0;
    }
    /* 再读取ub numa */
    ret = readIntFromFile(numaPathUB, numa);
    if (ret == 0) {
        return 0;
    }
    return -1;
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
    ret = GetHcaEth(hca);
    if (ret != 0) {
        return -1;
    }
    ret = GetHcaVerbs(hca->hcaName, hca->verbsName, MAX_NIC_PCIE_PATH);
    if (ret != 0) {
        return -1;
    }
    ret = GetHcaNuma(hca->hcaName, &hca->numa);
    if (ret != 0) {
        /* 读取失败，设置为-1 */
        hca->numa = -1;
    }
    return 0;
}

/**
 * 获取本机所有Host Channel Adapter信息
 */
int scanHca(HCA *nics, int maxNicNum, int *nicNum)
{
    DIR *dir = opendir("/sys/class/infiniband");
    if (dir == NULL) {
        return -1;
    }
    static const int maxLoop = 100;  // 循环保护
    int loop = 0;  // 循环保护
    int pos = 0;
    struct dirent *entry = NULL;
    while ((entry=readdir(dir)) != NULL) {
        if (loop >= maxLoop || pos >= maxNicNum) {
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
    (*nicNum) = pos;
    closedir(dir);
    return 0;
}
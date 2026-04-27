/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "host_rdma.h"
#include "securec.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <net/if.h>
#include "hal.h"
#include "rdma_hca.h"

int GetIpByIfName(const char* ifname, char* ip, size_t addrLen)
{
    struct ifaddrs *ifaddr = NULL;
    if (getifaddrs(&ifaddr) == -1) {
        return -1;
    }
    int found_interface = 0;
    for (struct ifaddrs *ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr || !ifa->ifa_name) {
            continue;
        }
        if (strcmp(ifname, ifa->ifa_name) != 0) {
            continue;
        }
        found_interface = 1;
        if (ifa->ifa_addr->sa_family == AF_INET) {
            struct sockaddr_in *sin = (struct sockaddr_in *)ifa->ifa_addr;
            if (inet_ntop(AF_INET, &sin->sin_addr, ip, addrLen)!= NULL) {
                freeifaddrs(ifaddr);
                return 0;
            }
            freeifaddrs(ifaddr);
            return -1;
        }
    }
    freeifaddrs(ifaddr);
    errno = found_interface ? ENOENT : ENODEV;
    return 0;
}



static int MatchNpuAndHca(NPU* npus, HCA* nics, int npuNum, int nicNum)
{
    if (npuNum == 0 || nicNum == 0) {
        return -1;
    }
    int cur = 0;
    for (int i = 0; i < npuNum; ++i) {
        // 网卡已经匹配后，需要从cur位置继续匹配，避免网卡足够时，匹配到同一个网卡
        // 当网卡数量不足时，循环绕一圈，继续匹配到仅有的网卡
        for (int j = cur; j < nicNum + cur; ++j) {
            size_t pos = j % nicNum;
            if (IsNpuAndNicInSamePcieSwitch(&npus[i], &nics[pos])) {
                (void)strcpy_s(npus[i].nicPciePath, sizeof(npus[i].nicPciePath), nics[pos].pciePath);
                (void)strcpy_s(npus[i].ethName, sizeof(npus[i].ethName), nics[pos].ethName);
                cur = j + 1;
                break;
            }
            if (npus[i].numa == nics[pos].numa) {
                (void)strcpy_s(npus[i].nicPciePath, sizeof(npus[i].nicPciePath), nics[pos].pciePath);
                (void)strcpy_s(npus[i].ethName, sizeof(npus[i].ethName), nics[pos].ethName);
                cur = j + 1;
                break;
            }
        }
    }
    return 0;
}

int InitNpu(NPU* npu)
{
    struct dcmi_pcie_info_all pcie_info;
    int ret = hal_get_device_pcie_info(npu->id, &pcie_info);
    if (ret == 0) {
        ret = sprintf_s(npu->bus_id, MAX_PCIE_BUS_ID_LEN, "%04x:%02x:%02x.%x", 
                                pcie_info.domain, pcie_info.bdf_busid, pcie_info.bdf_deviceid, pcie_info.bdf_funcid);
        char numaPath[MAX_NIC_PCIE_PATH] = {0};
        ret = sprintf_s(numaPath, MAX_NIC_PCIE_PATH, "/sys/bus/pci/devices/%s/numa_node", npu->bus_id);
        if (ret < 0) {
            return -1;
        }
        ret = ReadIntFromFile(numaPath, &npu->numa);
        if (ret != 0) {
            npu->numa = -1;
        }
    } else {
        if (npu->id < 4) {
            npu->numa = 0;
        } else {
            npu->numa = 1;
        }
    }
    return 0;
}


static void InitNpus(NPU *npu, size_t npu_count)
{
    for (size_t i = 0; i < npu_count; ++i) {
        npu[i].id = i;
        InitNpu(&npu[i]);
    }
}

int GetNpuHostRdmaIp(int npu_id, char* ip_addr, size_t ip_addr_len)
{
    // 获取所有NPU和HCA信息
    NPU npus[MAX_NPU_COUNT] = {0};
    HCA nics[MAX_NIC_COUNT] = {0};
    int npuNum = hal_get_npu_count();
    int nicNum = MAX_NIC_COUNT;
    InitNpus(npus, npuNum);
    if (ScanHca(nics, MAX_NIC_COUNT, &nicNum) != 0) {
        return -1;
    }
    if (nicNum == 0) {
        return -1;
    }
    // 匹配NPU和HCA
    if (MatchNpuAndHca(npus, nics, npuNum, nicNum) != 0) {
        return -1;
    }
    // 找到对于NPU的HCA IP
    for (int i = 0; i < npuNum; ++i) {
        if (npus[i].id == npu_id) {
            GetIpByIfName(npus[i].ethName, ip_addr, ip_addr_len);
        }
    }
    return 0;
}
/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "hccl_net_dev_v2.h"
#include "hccl_net_dev.h"
#include "exception_util.h"

using namespace std;

HcclResult HcclNetDevOpen(const HcclNetDevInfos *info, HcclNetDev *netDev)
{
    Hccl::CHECK_NULLPTR(netDev, "[HcclNetDevOpen] netDev is nullptr!");
    HcclResult ret = HcclNetDevOpenV2(info, netDev);
    if(ret == HCCL_SUCCESS){
        Hccl::CHECK_NULLPTR(*netDev, "[HcclNetDevOpen] *netDev is nullptr!");
        HCCL_DEBUG("HcclNetDevOpen: successfully opened netDev [%p]!", *netDev);
    }
    return ret;
}

HcclResult HcclNetDevClose(HcclNetDev netDev)
{
    HCCL_INFO("[HcclNetDevClose] netDev[%d].", netDev);
    return HcclNetDevCloseV2(netDev);
}

HcclResult HcclNetDevGetAddr(HcclNetDev netDev, HcclAddress *addr)
{
    HcclResult ret = HcclNetDevOpenV2(netDev, addr);
    if(ret == HCCL_SUCCESS){
        Hccl::CHECK_NULLPTR(addr, "[HcclNetDevGetAddr] addr is nullptr!");
        HCCL_DEBUG("HcclNetDevGetAddr: successfully got addr [%p]!", addr);
    }
    return ret;
}

HcclResult HcclNetDevGetBusAddr(HcclDeviceId dstDevId, HcclAddress *busAddr)
{
    HcclResult ret = HcclNetDevGetBusAddrV2(dstDevId, busAddr);
    if(ret != HCCL_SUCCESS){
        HCCL_ERROR("HcclNetDevGetBusAddr: failed to get bus addres for device %d, error code: %d", dstDevId, ret);
    }
    return ret;
}

HcclResult HcclNetDevGetNicAddr(int32_t devicePhyId, HcclAddress **addr, uint32_t *addrNum)
{
    HcclResult ret = HcclNetDevGetNicAddrV2(devicePhyId, addr, addrNum);
    if(ret != HCCL_SUCCESS){
        HCCL_ERROR("HcclNetDevGetBusAddr: failed to get NIC addres for device %d, error code: %d", devicePhyId, ret);
    }
    return ret;
}
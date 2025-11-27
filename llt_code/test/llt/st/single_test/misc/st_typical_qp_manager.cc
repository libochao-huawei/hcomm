/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
 
#include <stdio.h>
#include <stdlib.h>
#include "rt_external.h"
#include <assert.h>
#include <securec.h>
#include <ifaddrs.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string>
#include <sys/types.h>
#include <stddef.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "dlra_function.h"
 
#define private public
#define protected public
#include "externalinput.h"
#include "adapter_rts.h"
#include "network_manager_pub.h"
#include "adapter_rts.h"
#include "typical_qp_manager.h"
#include "rdma_resource_manager.h"
#include "externalinput_pub.h"
#undef private
 
#include <hccl/hccl.h>
#include <hccl/hccl_ex.h>
#include "llt_hccl_stub_pub.h"
#include "llt_hccl_stub_gdr.h"
#include <iostream>
#include <fstream>
 
using namespace std;
using namespace hccl;
 
 
class TypicalQPManagerTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "TypicalQPManagerTest SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "TypicalQPManagerTest TearDown" << std::endl;
    }
 
    virtual void SetUp()
    {
        std::cout << "A Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "A Test TearDown" << std::endl;
    }
};

HcclResult stub_HrtRaGetNotifyBaseAddr_1(RdmaHandle handle, u64 *va, u64 *size)
{
    HCCL_ERROR("stub_HrtRaGetNotifyBaseAddr");
    *va = 0x20000000;
    *size = 4;
    return HCCL_SUCCESS;
}
  
 
TEST_F(TypicalQPManagerTest, st_CreateQp)
{
    MOCKER(hrtRaTypicalQpCreate).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER(HrtRaGetNotifyBaseAddr).stubs().will(invoke(stub_HrtRaGetNotifyBaseAddr_1));
    MOCKER(GetExternalInputRdmaTrafficClass).stubs().will(returnValue(1));
    MOCKER(GetExternalInputRdmaServerLevel).stubs().will(returnValue(1));
    MOCKER(GetExternalInputRdmaRetryCnt).stubs().will(returnValue(1));
    MOCKER(GetExternalInputRdmaTimeOut).stubs().will(returnValue(1));
 
    s32 device_id = 0;
    HcclResult ret = hrtSetDevice(device_id);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    ret = RdmaResourceManager::GetInstance().Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
 
    struct typical_qp qpInfo;
    auto& manager = TypicalQpManager::GetInstance();
    manager.rdmaHandle_ = (void *)0x1000000;
    ret = manager.CreateQp(qpInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = hrtResetDevice(device_id);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
 
TEST_F(TypicalQPManagerTest, st_ModifyQp)
{
    HcclResult ret;
    s8* temp = (s8*)sal_malloc(sizeof(s8));
 
    MOCKER(hrtRaTypicalQpModify)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
 
    struct typical_qp localQpInfo;
    struct typical_qp remoteQpInfo;
 
    localQpInfo.qpn = 1;
    remoteQpInfo.qpn = 2;
    auto& manager = TypicalQpManager::GetInstance();
    manager.rdmaHandle_ = (void *)0x1000000;
    manager.qpMap_.insert(std::make_pair(1, std::make_pair(localQpInfo, (void *)temp)));
    ret = manager.ModifyQp(localQpInfo, remoteQpInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    sal_free(temp);
}
 
TEST_F(TypicalQPManagerTest, st_DestroyQp)
{
    HcclResult ret;
    s8* temp = (s8*)sal_malloc(sizeof(s8));
 
    MOCKER(HrtRaQpDestroy)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
 
    struct typical_qp qpInfo;
    qpInfo.qpn = 1;
    auto& manager = TypicalQpManager::GetInstance();
    manager.qpMap_.insert(std::make_pair(1, std::make_pair(qpInfo, (void *)temp)));
    manager.rdmaHandle_ = (void *)0x1000000;
    ret = manager.DestroyQp(qpInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    sal_free(temp);
}
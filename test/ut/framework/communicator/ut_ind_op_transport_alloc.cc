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

#ifndef private
#define private public
#define protected public
#endif
#include "hccl_communicator.h"
#undef private
#undef protected
#include "llt_hccl_stub_pub.h"

using namespace std;
using namespace hccl;

class IndOpTransportAllocTest : public testing::Test {
protected:
    static void SetUpTestCase() { }
    static void TearDownTestCase() { }
    virtual void SetUp() { }
    virtual void TearDown() { }
};

TEST_F(IndOpTransportAllocTest, Ut_IndOpTransportAlloc_When_AicpuModeWithUserDeviceMem_Expect_ReturnIsHCCL_E_NOT_SUPPORT) {
    std::unique_ptr<HcclCommunicator> hcclCommunicator(new (std::nothrow) HcclCommunicator());
    std::string tag = "test_tag";
    OpCommTransport opCommTransport;
    TransportIOMem transMem;
    transMem.indOpMem.userDeviceMem.push_back(DeviceMem());
    bool isAicpuModeEn = true;
    
    HcclResult ret = hcclCommunicator->IndOpTransportAlloc(tag, opCommTransport, transMem, isAicpuModeEn);
    EXPECT_EQ(ret, HCCL_E_NOT_SUPPORT);
}

TEST_F(IndOpTransportAllocTest, Ut_IndOpTransportAlloc_When_AicpuModeWithUserHostMem_Expect_ReturnIsHCCL_E_NOT_SUPPORT) {
    std::unique_ptr<HcclCommunicator> hcclCommunicator(new (std::nothrow) HcclCommunicator());
    std::string tag = "test_tag";
    OpCommTransport opCommTransport;
    TransportIOMem transMem;
    transMem.indOpMem.userHostMem.push_back(HostMem());
    bool isAicpuModeEn = true;
    
    HcclResult ret = hcclCommunicator->IndOpTransportAlloc(tag, opCommTransport, transMem, isAicpuModeEn);
    EXPECT_EQ(ret, HCCL_E_NOT_SUPPORT);
}
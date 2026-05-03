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

#include "hccl/base.h"
#include <hccl/hccl_types.h>

#ifndef private
#define private public
#define protected public
#endif

#include "socket.h"
#include "network/hccp_common.h"
#include "adapter_hccp.h"

#undef private
#undef protected

using namespace std;
using namespace hccl;

class Socket_UT : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "\033[36m--Socket_UT SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "\033[36m--Socket_UT TearDown--\033[0m" << std::endl;
    }
    virtual void SetUp()
    {
    }
    virtual void TearDown()
    {
        GlobalMockObject::verify();
    }
};

TEST_F(Socket_UT, Socket_Constructor)
{
    std::string tag = "test_socket";
    u32 role = 0;
    SocketType type = SocketType::SOCKET_VNIC;
    NICDeployment nicDeploy = NICDeployment::NIC_DEPLOYMENT_DEVICE;
    HcclIpAddress locNicIp("0.0.0.0");
    HcclIpAddress remNicIp("0.0.0.0");
    u32 locDevPhyId = 0;
    u32 remDevPhyId = 0;
    DeviceIdType deviceIdType = DeviceIdType::DEVICE_ID_TYPE_PHY_ID;
    
    Socket socket(tag, role, type, nicDeploy, locNicIp, locDevPhyId, remNicIp, remDevPhyId, deviceIdType);
}

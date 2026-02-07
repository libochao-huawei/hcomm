/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
 
#include "../../../hccl_api_base_test.h"
#include "hcomm_c_adpt.h"
#include "local_notify_impl.h"
#include "aicpu_launch_manager.h"
class TestUrmaMem : public BaseInit {
public:
    void SetUp() override {
        BaseInit::SetUp();
    }
    void TearDown() override {
        BaseInit::TearDown();
        GlobalMockObject::verify();
    }

private:
    const EndpointDesc endpointDesc = {
        .protocol = COMM_PROTOCOL_UBC_TP,
        .commAddr = {
            .type = COMM_ADDR_TYPE_IP_V4,
            .addr = {inet_addr("192.168.1.100")}
        },
        .loc = {
            .locType = ENDPOINT_LOC_TYPE_DEVICE,
            .device = {
                .devPhyId = 1,
                .superDevId = 100,
                .superIdx = 0,
                .superPodIdx = 2
            }
        },
        .raws = {0}
    };

    HcommMem mem = {
        .type = HCCL_MEM_TYPE_DEVICE,
        .addr = reinterpret_cast<void *>(0x1111),
        .size = 100
    };
};

TEST_F(TestUrmaMem, HcommMemReg_Basic_Expect_ResultIsHCCL_SUCCESS)
{
    EndpointHandle endpointHandle = nullptr;
    HcclResult res = HcommEndpointCreate(&endpointDesc, &endpointHandle);
    ASSERT_EQ(res, HCCL_SUCCESS);

    const char *memTag = "HcclBuffer";

    void *memHandle;
    res = HcommMemReg(endpointHandle, memTag, mem, &memHandle);
    EXPECT_EQ(RES, HCCL_SUCCESS);
    EXPECT_NE(memHandle, nullptr);
}
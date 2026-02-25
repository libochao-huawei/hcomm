/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "hccl_api_base_test.h"
#include "log.h"

#define private public

using namespace hccl;

class HcclGetRemoteIpcHcclBufTest : public BaseInit {
public:
    void SetUp() override
    {
        BaseInit::SetUp();
        UT_USE_RANK_TABLE_910_1SERVER_1RANK;
        UT_COMM_CREATE_DEFAULT(comm);
    }
    void TearDown() override
    {
        Ut_Comm_Destroy(comm);
        BaseInit::TearDown();
        GlobalMockObject::verify();
    }
};

TEST_F(HcclGetRemoteIpcHcclBufTest, ut_HcclGetRemoteIpcHcclBuf_When_Normal_Expect_ReturnIsHCCL_SUCCESS) {
    hccl::hcclComm fake;
    int dummy_resource = 7;
    fake.SetCommResource(&dummy_resource); // 非空
    void *expectedAddr = reinterpret_cast<void *>(0xDEADBEEF);
    uint64_t expectedSize = 4096;
    fake.SetMockBuf(expectedAddr, expectedSize);

    void *addr = nullptr;
    uint64_t size = 0;
    HcclResult ret = HcclGetRemoteIpcHcclBuf(comm, 0, &addr, &size);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(addr, expectedAddr);
    EXPECT_EQ(size, expectedSize);
}

TEST_F(HcclGetRemoteIpcHcclBufTest, ut_HcclGetRemoteIpcHcclBuf_When_ParamIsNullptr_Expect_ReturnIsHCCL_E_PTR) {
    void *addr = nullptr;
    uint64_t size = 0;
    
    EXPECT_EQ(HcclGetRemoteIpcHcclBuf(nullptr, 0, &addr, &size), HCCL_E_PARA);
    hccl::hcclComm fake;
    EXPECT_EQ(HcclGetRemoteIpcHcclBuf(static_cast<HcclComm>(nullptr), 0, nullptr, &size), HCCL_E_PARA);
    EXPECT_EQ(HcclGetRemoteIpcHcclBuf(static_cast<HcclComm>(&fake), 0, &addr, nullptr), HCCL_E_PARA);
}

TEST_F(HcclGetRemoteIpcHcclBufTest, OpResCtxNull_ReturnsError) {
    hccl::hcclComm fake;
    // 不设置 resource -> GetCommResource 返回 nullptr -> 应返回 HCCL_E_PARA
    void *addr = nullptr;
    uint64_t size = 0;
    EXPECT_EQ(HcclGetRemoteIpcHcclBuf(static_cast<HcclComm>(&fake), 1, &addr, &size), HCCL_E_PARA);
}

TEST_F(HcclGetRemoteIpcHcclBufTest, GetRemoteCCLBufFails_ReturnsError) {
    hccl::hcclComm fake;
    int dummy_resource = 42;
    fake.SetCommResource(&dummy_resource); // 非空，进入下一步
    fake.SetFailGetRemote(true); // 模拟 GetRemoteCCLBuf 返回失败
    void *addr = nullptr;
    uint64_t size = 0;
    HcclResult ret = HcclGetRemoteIpcHcclBuf(static_cast<HcclComm>(&fake), 2, &addr, &size);
    EXPECT_EQ(ret, static_cast<HcclResult>(HCCL_E_INTERNAL));
}

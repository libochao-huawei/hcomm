/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <hccl/hccl_types.h>
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

HcclResult GetRemoteCCLBufStub(HcclCommunicator *This, uint32_t remoteRank, void **addr, uint64_t *size)
{
    *addr = reinterpret_cast<void *>(0x12345678);
    *size = 0;
    return HCCL_SUCCESS;
}

TEST_F(HcclGetRemoteIpcHcclBufTest, ut_HcclGetRemoteIpcHcclBuf_When_Normal_Expect_ReturnIsHCCL_SUCCESS)
{
    void *addr = nullptr;
    uint64_t size = 0;

    MOCKER_CPP(&HcclCommunicator::GetRemoteCCLBuf).stubs().will(invoke(GetRemoteCCLBufStub));

    HcclResult ret = HcclGetRemoteIpcHcclBuf(comm, 1, &addr, &size);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(HcclGetRemoteIpcHcclBufTest, ut_HcclGetRemoteIpcHcclBuf_When_ParamIsNullptr_Expect_ReturnIsHCCL_E_PTR)
{
    void *addr = nullptr;
    uint64_t size = 0;
    
    HcclResult ret = HcclGetRemoteIpcHcclBuf(nullptr, 1, &addr, &size);
    EXPECT_EQ(ret, HCCL_E_PTR);

    ret = HcclGetRemoteIpcHcclBuf(comm, 1, nullptr, &size);
    EXPECT_EQ(ret, HCCL_E_PTR);

    ret = HcclGetRemoteIpcHcclBuf(comm, 1, &addr, nullptr);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(HcclGetRemoteIpcHcclBufTest, ut_HcclGetRemoteIpcHcclBuf_When_RemoteRankIsInvalid_Expect_ReturnIsHCCL_E_PTR)
{
    void *addr = nullptr;
    uint64_t size = 0;
    
    HcclResult ret = HcclGetRemoteIpcHcclBuf(comm, 16, &addr, &size);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

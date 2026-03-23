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
#include "data_ring.h"
#include "resource_entities.h"
#include "acl/acl_rt.h"

class TestDataRing : public BaseInit {
public:
    void SetUp() override {
        BaseInit::SetUp();
    }
    void TearDown() override {
        BaseInit::TearDown();
        GlobalMockObject::verify();
    }
};

// DataRing_Init_test_01
TEST_F(TestDataRing, Ut_DataRing_Init_When_Normal_Expect_ReturnIsHCCL_SUCCESS)
{
    MOCKER_CPP(aclrtMalloc).stubs().will(returnValue(ACL_SUCCESS));
    MOCKER_CPP(aclrtMemcpy).stubs().will(returnValue(ACL_SUCCESS));
    DataRing ring(DATA_RING_CAPACITY);
    HcclResult ret = ring.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

// DataRing_Init_test_02
TEST_F(TestDataRing, Ut_DataRing_Init_When_MallocBufferFails_Expect_ReturnIsHCCL_E_RUNTIME)
{
    MOCKER(aclrtMalloc)
        .stubs()
        .will(returnValue(ACL_ERROR_INVALID_PARAM));
    DataRing ring(DATA_RING_CAPACITY);
    EXPECT_EQ(ring.Init(), HCCL_E_RUNTIME);
}

// DataRing_Init_test_03
TEST_F(TestDataRing, Ut_DataRing_Init_When_MallocHeadFails_Expect_ReturnIsHCCL_E_RUNTIME)
{
    MOCKER(aclrtMalloc)
        .stubs()
        .will(returnValue(ACL_SUCCESS))
        .then(returnValue(ACL_ERROR_INVALID_PARAM));
    MOCKER(aclrtFree)
        .stubs()
        .will(returnValue(ACL_SUCCESS));
    DataRing ring(DATA_RING_CAPACITY);
    EXPECT_EQ(ring.Init(), HCCL_E_RUNTIME);
}

// DataRing_Init_test_04
TEST_F(TestDataRing, Ut_DataRing_Init_When_MallocTailFails_Expect_ReturnIsHCCL_E_RUNTIME)
{
    MOCKER(aclrtMalloc)
        .stubs()
        .will(returnValue(ACL_SUCCESS))
        .then(returnValue(ACL_SUCCESS))
        .then(returnValue(ACL_ERROR_INVALID_PARAM));
    MOCKER(aclrtFree)
        .stubs()
        .will(returnValue(ACL_SUCCESS));
    DataRing ring(DATA_RING_CAPACITY);
    EXPECT_EQ(ring.Init(), HCCL_E_RUNTIME);
}

// DataRing_Init_test_05
TEST_F(TestDataRing, Ut_DataRing_Init_When_MemcpyHeadInitFails_Expect_ReturnIsHCCL_E_RUNTIME)
{
    MOCKER(aclrtMalloc)
        .stubs()
        .will(returnValue(ACL_SUCCESS));
    MOCKER(aclrtMemcpy)
        .stubs()
        .will(returnValue(ACL_ERROR_INVALID_PARAM));
    MOCKER(aclrtFree)
        .stubs()
        .will(returnValue(ACL_SUCCESS));
    DataRing ring(DATA_RING_CAPACITY);
    EXPECT_EQ(ring.Init(), HCCL_E_RUNTIME);
}

// DataRing_Init_test_06
TEST_F(TestDataRing, Ut_DataRing_Init_When_MemcpyTailInitFails_Expect_ReturnIsHCCL_E_RUNTIME)
{
    MOCKER(aclrtMalloc)
        .stubs()
        .will(returnValue(ACL_SUCCESS));
    MOCKER(aclrtMemcpy)
        .stubs()
        .will(returnValue(ACL_SUCCESS))
        .then(returnValue(ACL_ERROR_INVALID_PARAM));
    MOCKER(aclrtFree)
        .stubs()
        .will(returnValue(ACL_SUCCESS));
    DataRing ring(DATA_RING_CAPACITY);
    EXPECT_EQ(ring.Init(), HCCL_E_RUNTIME);
}

// DataRing_ReadArgs_test_01
TEST_F(TestDataRing, Ut_DataRing_ReadArgs_When_Normal_Expect_ReturnIsHCCL_SUCCESS)
{
    MOCKER(aclrtMalloc)
        .stubs()
        .will(returnValue(ACL_SUCCESS));
    MOCKER(aclrtMemcpy)
        .stubs()
        .will(returnValue(ACL_SUCCESS));
    MOCKER(aclrtFree)
        .stubs()
        .will(returnValue(ACL_SUCCESS));
    DataRing ring(DATA_RING_CAPACITY);
    EXPECT_EQ(ring.Init(), HCCL_SUCCESS);

    uint8_t buf[64] = {0};
    HcclResult ret = ring.ReadArgs(0, 24, buf, sizeof(buf));
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

// DataRing_ReadArgs_test_02
TEST_F(TestDataRing, Ut_DataRing_ReadArgs_When_Uninitialized_Expect_ReturnIsHCCL_E_PARA)
{
    DataRing ring(DATA_RING_CAPACITY);
    uint8_t buf[64] = {0};
    HcclResult ret = ring.ReadArgs(0, 16, buf, sizeof(buf));
    EXPECT_EQ(ret, HCCL_E_PARA);
}

// DataRing_ReadArgs_test_03
TEST_F(TestDataRing, Ut_DataRing_ReadArgs_When_ArgsSizeIsZero_Expect_ReturnIsHCCL_E_PARA)
{
    MOCKER_CPP(aclrtMalloc).stubs().will(returnValue(ACL_SUCCESS));
    MOCKER_CPP(aclrtMemcpy).stubs().will(returnValue(ACL_SUCCESS));
    DataRing ring(DATA_RING_CAPACITY);
    EXPECT_EQ(ring.Init(), HCCL_SUCCESS);

    uint8_t buf[64] = {0};
    HcclResult ret = ring.ReadArgs(0, 0, buf, sizeof(buf));
    EXPECT_EQ(ret, HCCL_E_PARA);
}

// DataRing_ReadArgs_test_04
TEST_F(TestDataRing, Ut_DataRing_ReadArgs_When_ArgsSizeExceedsDstBuf_Expect_ReturnIsHCCL_E_PARA)
{
    MOCKER_CPP(aclrtMalloc).stubs().will(returnValue(ACL_SUCCESS));
    MOCKER_CPP(aclrtMemcpy).stubs().will(returnValue(ACL_SUCCESS));
    DataRing ring(DATA_RING_CAPACITY);
    EXPECT_EQ(ring.Init(), HCCL_SUCCESS);

    uint8_t buf[16] = {0};
    HcclResult ret = ring.ReadArgs(0, 32, buf, sizeof(buf));
    EXPECT_EQ(ret, HCCL_E_PARA);
}

// DataRing_ReadArgs_test_05
TEST_F(TestDataRing, Ut_DataRing_ReadArgs_When_MemcpyFails_Expect_ReturnIsHCCL_E_RUNTIME)
{
    MOCKER(aclrtMalloc)
        .stubs()
        .will(returnValue(ACL_SUCCESS));
    MOCKER(aclrtMemcpy)
        .stubs()
        .will(returnValue(ACL_SUCCESS));
    MOCKER(aclrtFree)
        .stubs()
        .will(returnValue(ACL_SUCCESS));
    DataRing ring(DATA_RING_CAPACITY);
    EXPECT_EQ(ring.Init(), HCCL_SUCCESS);

    // Re-stub memcpy to fail for ReadArgs
    GlobalMockObject::verify();
    MOCKER(aclrtMemcpy)
        .stubs()
        .will(returnValue(ACL_ERROR_INVALID_PARAM));
    MOCKER(aclrtFree)
        .stubs()
        .will(returnValue(ACL_SUCCESS));

    uint8_t buf[64] = {0};
    EXPECT_EQ(ring.ReadArgs(0, 24, buf, sizeof(buf)), HCCL_E_RUNTIME);
}

// DataRing_AdvanceHead_test_01
TEST_F(TestDataRing, Ut_DataRing_AdvanceHead_When_Normal_Expect_ReturnIsHCCL_SUCCESS)
{
    MOCKER(aclrtMalloc)
        .stubs()
        .will(returnValue(ACL_SUCCESS));
    MOCKER(aclrtMemcpy)
        .stubs()
        .will(returnValue(ACL_SUCCESS));
    MOCKER(aclrtFree)
        .stubs()
        .will(returnValue(ACL_SUCCESS));
    DataRing ring(DATA_RING_CAPACITY);
    EXPECT_EQ(ring.Init(), HCCL_SUCCESS);

    HcclResult ret = ring.AdvanceHead(0, 24);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

// DataRing_AdvanceHead_test_02
TEST_F(TestDataRing, Ut_DataRing_AdvanceHead_When_Uninitialized_Expect_ReturnIsHCCL_E_PARA)
{
    DataRing ring(DATA_RING_CAPACITY);
    HcclResult ret = ring.AdvanceHead(0, 16);
    EXPECT_EQ(ret, HCCL_E_PARA);
}

// DataRing_AdvanceHead_test_03
TEST_F(TestDataRing, Ut_DataRing_AdvanceHead_When_MemcpyFails_Expect_ReturnIsHCCL_E_RUNTIME)
{
    MOCKER(aclrtMalloc)
        .stubs()
        .will(returnValue(ACL_SUCCESS));
    MOCKER(aclrtMemcpy)
        .stubs()
        .will(returnValue(ACL_SUCCESS));
    MOCKER(aclrtFree)
        .stubs()
        .will(returnValue(ACL_SUCCESS));
    DataRing ring(DATA_RING_CAPACITY);
    EXPECT_EQ(ring.Init(), HCCL_SUCCESS);

    // Re-stub memcpy to fail for AdvanceHead
    GlobalMockObject::verify();
    MOCKER(aclrtMemcpy)
        .stubs()
        .will(returnValue(ACL_ERROR_INVALID_PARAM));
    MOCKER(aclrtFree)
        .stubs()
        .will(returnValue(ACL_SUCCESS));

    EXPECT_EQ(ring.AdvanceHead(0, 24), HCCL_E_RUNTIME);
}

// DataRing_GetDataRingInfo_test_01
TEST_F(TestDataRing, Ut_DataRing_GetDataRingInfo_When_Normal_Expect_CorrectInfo)
{
    MOCKER_CPP(aclrtMalloc).stubs().will(returnValue(ACL_SUCCESS));
    MOCKER_CPP(aclrtMemcpy).stubs().will(returnValue(ACL_SUCCESS));
    DataRing ring(DATA_RING_CAPACITY);
    EXPECT_EQ(ring.Init(), HCCL_SUCCESS);

    hccl::DataRingInfo info = ring.GetDataRingInfo();
    EXPECT_EQ(info.capacity, DATA_RING_CAPACITY);
}

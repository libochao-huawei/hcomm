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

class TestHcclGetHcclBuffer : public BaseInit {
public:
    void SetUp() override {
        BaseInit::SetUp();
        
    }
    void TearDown() override {
        BaseInit::TearDown();
        GlobalMockObject::verify();
    }
};

TEST_F(TestHcclGetHcclBuffer, Ut_HcclGetHcclBuffer_When_Normal_Return_HCCL_Success)
{
    MOCKER(hrtGetDeviceType)
        .stubs()
        .with(outBound(DevType::DEV_TYPE_910_95))
        .will(returnValue(HCCL_SUCCESS));
    MOCKER(IsSupportHCCLV2)
        .stubs()
        .will(returnValue(true));
    setenv("HCCL_INDEPENDENT_OP","1",1);


    void* commV2 = (void*)0x2000;
    void* rankGraph = (void*)0x2000;
    u32 rank = 1;
    HcclMem cclBuffer;
    cclBuffer.size = 2;
    cclBuffer.type = HcclMemType::HCCL_MEM_TYPE_HOST;
    cclBuffer.addr = (void*)0x1000;;
    char commName[ROOTINFO_INDENTIFIER_MAX_LENGTH] = {};
    std::shared_ptr<hccl::hcclComm> hcclCommPtr = make_shared<hccl::hcclComm>(1, 1, commName);
    HcclCommConfig config;
    config.hcclOpExpansionMode = 1; // 非CCU模式，避免拉起CCU平台层
    HcclResult ret = hcclCommPtr->InitCollComm(commV2, rankGraph, rank, cclBuffer, commName, &config);
    EXPECT_EQ(ret, 0);
    
    void* comm = static_cast<HcclComm>(hcclCommPtr.get());
    void* buffer;
    uint64_t size;
    ret =  HcclGetHcclBuffer(comm, &buffer, &size);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(size, 2);

}



TEST_F(TestHcclGetHcclBuffer, Ut_HcclGetHcclBuffer_When_CommNullptr_Return_HCCL_E_PTR)
{
    MOCKER(hrtGetDeviceType)
        .stubs()
        .with(outBound(DevType::DEV_TYPE_910_95))
        .will(returnValue(HCCL_SUCCESS));

    void* comm = nullptr;
    void* buffer;
    uint64_t size;
    HcclResult ret =  HcclGetHcclBuffer(comm, &buffer, &size);
    EXPECT_EQ(ret, HCCL_E_PTR);

}

TEST_F(TestHcclGetHcclBuffer, Ut_HcclGetHcclBuffer_When_bufferNullptr_Return_HCCL_E_PTR)
{
    MOCKER(hrtGetDeviceType)
        .stubs()
        .with(outBound(DevType::DEV_TYPE_910_95))
        .will(returnValue(HCCL_SUCCESS));

    void* comm = (void*)0x123456;
    void** buffer{nullptr};
    uint64_t size;
    HcclResult ret =  HcclGetHcclBuffer(comm, buffer, &size);
    EXPECT_EQ(ret, HCCL_E_PTR);

}

TEST_F(TestHcclGetHcclBuffer, Ut_HcclGetHcclBuffer_When_sizeNullptr_Return_HCCL_E_PTR)
{
    MOCKER(hrtGetDeviceType)
        .stubs()
        .with(outBound(DevType::DEV_TYPE_910_95))
        .will(returnValue(HCCL_SUCCESS));

    void* comm = (void*)0x123456;
    void* buffer;
    uint64_t* size{nullptr};
    HcclResult ret =  HcclGetHcclBuffer(comm, &buffer, size);
    EXPECT_EQ(ret, HCCL_E_PTR);

}

TEST_F(TestHcclGetHcclBuffer, Ut_HcclGetHcclBuffer_When_CollCommNullptr_Return_HCCL_E_PTR)
{
    MOCKER(hrtGetDeviceType)
        .stubs()
        .with(outBound(DevType::DEV_TYPE_910_95))
        .will(returnValue(HCCL_SUCCESS));
    MOCKER(IsSupportHCCLV2)
        .stubs()
        .will(returnValue(true));
    setenv("HCCL_INDEPENDENT_OP","1",1);


    void* commV2 = (void*)0x2000;
    void* rankGraph = (void*)0x2000;
    u32 rank = 1;
    HcclMem cclBuffer;
    cclBuffer.size = 1;
    cclBuffer.type = HcclMemType::HCCL_MEM_TYPE_HOST;
    cclBuffer.addr = (void*)0x1000;;
    char commName[ROOTINFO_INDENTIFIER_MAX_LENGTH] = {};
    std::shared_ptr<hccl::hcclComm> hcclCommPtr = make_shared<hccl::hcclComm>(1, 1, commName);
    
    void* comm = static_cast<HcclComm>(hcclCommPtr.get());
    void* buffer;
    uint64_t size;
    HcclResult ret =  HcclGetHcclBuffer(comm, &buffer, &size);
    EXPECT_EQ(ret,  HCCL_E_PTR);

}

TEST_F(TestHcclGetHcclBuffer, Ut_HcclGetHcclBuffer_When_MyRankNullptr_Return_HCCL_E_PTR)
{
    MOCKER(hrtGetDeviceType)
        .stubs()
        .with(outBound(DevType::DEV_TYPE_910_95))
        .will(returnValue(HCCL_SUCCESS));
     MOCKER_CPP(&CollComm::Init)
        .stubs()
        .will(returnValue(0));  
    MOCKER(IsSupportHCCLV2)
        .stubs()
        .will(returnValue(true));
    setenv("HCCL_INDEPENDENT_OP","1",1);


    void* commV2 = (void*)0x2000;
    void* rankGraph = (void*)0x2000;
    u32 rank = 1;
    HcclMem cclBuffer;
    cclBuffer.size = 1;
    cclBuffer.type = HcclMemType::HCCL_MEM_TYPE_HOST;
    cclBuffer.addr = (void*)0x1000;;
    char commName[ROOTINFO_INDENTIFIER_MAX_LENGTH] = {};
    std::shared_ptr<hccl::hcclComm> hcclCommPtr = make_shared<hccl::hcclComm>(1, 1, commName);
    HcclCommConfig config;
    config.hcclOpExpansionMode = 1; // 非CCU模式，避免拉起CCU平台层
    HcclResult ret = hcclCommPtr->InitCollComm(commV2, rankGraph, rank, cclBuffer, commName, &config);
    EXPECT_EQ(ret, 0);
    
    void* comm = static_cast<HcclComm>(hcclCommPtr.get());
    void* buffer;
    uint64_t size;
    ret =  HcclGetHcclBuffer(comm, &buffer, &size);
    EXPECT_EQ(ret,  HCCL_E_PTR);

}
TEST_F(TestHcclGetHcclBuffer, Ut_HcclGetHcclBuffer_When_CommMemsNullptr_Return_HCCL_E_PTR)
{
    MOCKER(hrtGetDeviceType)
        .stubs()
        .with(outBound(DevType::DEV_TYPE_910_95))
        .will(returnValue(HCCL_SUCCESS));
     MOCKER_CPP(&MyRank::Init)
        .stubs()
        .will(returnValue(0));
    MOCKER(IsSupportHCCLV2)
        .stubs()
        .will(returnValue(true));
    setenv("HCCL_INDEPENDENT_OP","1",1);


    void* commV2 = (void*)0x2000;
    void* rankGraph = (void*)0x2000;
    u32 rank = 1;
    HcclMem cclBuffer;
    cclBuffer.size = 1;
    cclBuffer.type = HcclMemType::HCCL_MEM_TYPE_HOST;
    cclBuffer.addr = (void*)0x1000;;
    char commName[ROOTINFO_INDENTIFIER_MAX_LENGTH] = {};
    std::shared_ptr<hccl::hcclComm> hcclCommPtr = make_shared<hccl::hcclComm>(1, 1, commName);
    HcclCommConfig config;
    config.hcclOpExpansionMode = 1; // 非CCU模式，避免拉起CCU平台层
    HcclResult ret = hcclCommPtr->InitCollComm(commV2, rankGraph, rank, cclBuffer, commName, &config);
    EXPECT_EQ(ret, 0);
    
    void* comm = static_cast<HcclComm>(hcclCommPtr.get());
    void* buffer;
    uint64_t size;
    ret =  HcclGetHcclBuffer(comm, &buffer, &size);
    EXPECT_EQ(ret,  HCCL_E_PTR);

}

TEST_F(TestHcclGetHcclBuffer, Ut_HcclGetHcclBufferA3_When_Normal_Return_HCCL_Success)
{
    char commName[ROOTINFO_INDENTIFIER_MAX_LENGTH] = {};
    std::shared_ptr<hccl::hcclComm> hcclCommPtr = make_shared<hccl::hcclComm>(1, 1, commName);

    void* comm = static_cast<HcclComm>(hcclCommPtr.get());
    void* buffer;
    uint64_t size;
    int ret =  HcclGetHcclBuffer(comm, &buffer, &size);
    EXPECT_EQ(ret, 0);

}
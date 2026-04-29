/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <gtest/gtest.h>
#include <mockcpp/mockcpp.hpp>

#include <memory>
#include <iostream>
#include <string>

#ifndef private
#define private public
#define protected public
#endif
#include "aicpu_one_side_service.h"
#include "stream_pub.h"
#undef private
#undef protected

using namespace std;
using namespace hccl;

class OneSideServiceUT : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "OneSideServiceUT SetUp" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "OneSideServiceUT TearDown" << std::endl;
    }
    virtual void SetUp()
    {
        HcclOneSideServiceAicpu::services_.clear();
        std::cout << "A Test SetUp" << std::endl;
    }
    virtual void TearDown()
    {
        HcclOneSideServiceAicpu::services_.clear();
        GlobalMockObject::verify();
        std::cout << "A Test TearDown" << std::endl;
    }

    void SetupService(std::shared_ptr<HcclOneSideServiceAicpu> &service, const std::string &identifier, bool execStreamEnable, 
        uint32_t devId, uint32_t sqId, uint32_t actualStreamId)
    {
        service->identifier_ = identifier;
        service->execStreamEnable_ = false;
        service->devId_ = 0;

        HcclComStreamInfo streamInfo{};
        streamInfo.sqId = 1;
        streamInfo.actualStreamId = 100;
        service->execStream_ = Stream(streamInfo);
    }
};

TEST_F(OneSideServiceUT, Ut_CleanStreamFunc_When_Disabled_ExecutesClean)
{   
    auto service = std::make_shared<HcclOneSideServiceAicpu>();
    SetupService(service, "test_clean", false, 0, 1, 100);

    SqCqeContext sqCqeContext{};
    service->execStream_.sqeContext_ = &sqCqeContext.sqContext;

    MOCKER(ConfigSqStatusByType)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&Stream::ClearLocalBuff)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));

    MOCKER(QuerySqStatusByType)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));

    HcclResult ret = service->CleanStreamFunc();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_TRUE(service->execStreamEnable_);
}

TEST_F(OneSideServiceUT, Ut_DisableAllStreamFunc_When_DisableStreamSuc)
{
    auto service = std::make_shared<HcclOneSideServiceAicpu>();
    SetupService(service, "test_DisableStream", false, 0, 1, 100);

    HcclResult ret = service->DisableAllStreamFunc();
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(OneSideServiceUT, Ut_CleanAllStreamFunc_When_CleanStreamFuncFail_ReturnsError)
{
    auto service = std::make_shared<HcclOneSideServiceAicpu>();
    SetupService(service, "fail_tag", false, 0, 1, 100);
    HcclOneSideServiceAicpu::services_["fail_tag"] = service;

    MOCKER(ConfigSqStatusByType)
        .stubs()
        .will(returnValue(HCCL_E_INTERNAL));

    HcclResult ret = HcclOneSideServiceAicpu::CleanAllStreamFunc();
    EXPECT_NE(ret, HCCL_SUCCESS);
}

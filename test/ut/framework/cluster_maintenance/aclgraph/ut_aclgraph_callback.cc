/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
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
#include <cstring>

#include "hccl/base.h"
#include "hccl/hccl_types.h"
#include "hcom_pub.h"
#include "hcom_common.h"
#include "comm_config_pub.h"
#include "common.h"
#include "hccl_communicator.h"
#include "aclgraph_callback.h"

using namespace hccl;

class AclgraphCallbackTest : public testing::Test {
protected:
    void SetUp() override {
        comm.reset(new (std::nothrow) hccl::hcclComm());
        if (!comm) {
            HCCL_ERROR("Failed to create hccl::hcclComm");
            return;
        }
    }

    void TearDown() override {
        GlobalMockObject::verify();
    }

    std::shared_ptr<hccl::hcclComm> comm;
    HcclCommunicator communicator_;
};

TEST_F(AclgraphCallbackTest, ut_InsertNewTagToCaptureResMap_When_Capture_Expect_SUCCESS)
{
    int mockModel = 0;
    void *pmockModel = &mockModel;
    aclmdlRICaptureStatus captureStatus = aclmdlRICaptureStatus::ACL_MODEL_RI_CAPTURE_STATUS_ACTIVE;
    MOCKER(aclmdlRICaptureGetInfo)
    .stubs()
    .with(any(), outBoundP(&captureStatus, sizeof(captureStatus)), outBoundP(&pmockModel, sizeof(pmockModel)))
    .will(returnValue(0));

    std::string newTag = "tag";
    OpParam opParam;
    HcclResult ret = AclgraphCallback::GetInstance().InsertNewTagToCaptureResMap(&communicator_, newTag, opParam);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(AclgraphCallbackTest, ut_InsertNewTagToCaptureResMap_When_Comm_Invalid_Expect_Fail)
{
    std::string newTag = "tag";
    OpParam opParam;
    HcclResult ret = AclgraphCallback::GetInstance().InsertNewTagToCaptureResMap(nullptr, newTag, opParam);
    EXPECT_EQ(ret, HCCL_E_PTR);
    GlobalMockObject::verify();
}

TEST_F(AclgraphCallbackTest, ut_CleanCaptureRes_When_modelId_Invalid_Expect_Fail)
{
    u64 modelId = 0;

    HcclResult ret = AclgraphCallback::GetInstance().CleanCaptureRes(modelId);
    EXPECT_EQ(ret, HCCL_E_NOT_FOUND);
    GlobalMockObject::verify();
}

TEST_F(AclgraphCallbackTest, ut_CleanCaptureRes_By_Communicator_With_Multiple_Comms_Expect_Correct_Comm_Removed)
{
    int mockModel = 0;
    void *pmockModel = &mockModel;
    aclmdlRICaptureStatus captureStatus = aclmdlRICaptureStatus::ACL_MODEL_RI_CAPTURE_STATUS_ACTIVE;
    MOCKER(aclmdlRICaptureGetInfo)
    .stubs()
    .with(any(), outBoundP(&captureStatus, sizeof(captureStatus)), outBoundP(&pmockModel, sizeof(pmockModel)))
    .will(returnValue(0));

    MOCKER(aclmdlRIDestroyRegisterCallback)
    .stubs()
    .will(returnValue(0));

    std::string newTag = "tag";
    OpParam opParam;

    // 创建两个不同的 communicator
    HcclCommunicator communicator1;
    HcclCommunicator communicator2;

    // 插入两个 communicator 到 captureResMap_
    HcclResult ret1 = AclgraphCallback::GetInstance().InsertNewTagToCaptureResMap(&communicator1, newTag, opParam);
    EXPECT_EQ(ret1, HCCL_SUCCESS);
    HcclResult ret2 = AclgraphCallback::GetInstance().InsertNewTagToCaptureResMap(&communicator2, newTag, opParam);
    EXPECT_EQ(ret2, HCCL_SUCCESS);

    // 验证插入成功
    bool foundComm1Before = false;
    bool foundComm2Before = false;
    for (const auto &modelEntry : AclgraphCallback::GetInstance().captureResMap_) {
        if (modelEntry.second.find(&communicator1) != modelEntry.second.end()) {
            foundComm1Before = true;
        }
        if (modelEntry.second.find(&communicator2) != modelEntry.second.end()) {
            foundComm2Before = true;
        }
    }
    EXPECT_TRUE(foundComm1Before) << "Communicator1 should be found in captureResMap_ before CleanCaptureRes";
    EXPECT_TRUE(foundComm2Before) << "Communicator2 should be found in captureResMap_ before CleanCaptureRes";

    // 只删除 communicator1
    AclgraphCallback::GetInstance().CleanCaptureRes(&communicator1);

    // 验证：communicator1 应该被删除，communicator2 应该仍然存在
    bool foundComm1After = false;
    bool foundComm2After = false;
    for (const auto &modelEntry : AclgraphCallback::GetInstance().captureResMap_) {
        if (modelEntry.second.find(&communicator1) != modelEntry.second.end()) {
            foundComm1After = true;
        }
        if (modelEntry.second.find(&communicator2) != modelEntry.second.end()) {
            foundComm2After = true;
        }
    }
    EXPECT_FALSE(foundComm1After) << "Communicator1 should NOT be found in captureResMap_ after CleanCaptureRes";
    EXPECT_TRUE(foundComm2After) << "Communicator2 should still be found in captureResMap_ after CleanCaptureRes";

    GlobalMockObject::verify();
}


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

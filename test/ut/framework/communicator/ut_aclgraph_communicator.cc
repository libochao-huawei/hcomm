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
#include <securec.h>

// -fno-access-control 已在 CMakeLists.txt 中启用，可直接访问 private 成员
#include "hccl_communicator.h"
#include "coll_alg_param.h"

using namespace hccl;

class AclgraphCommunicatorTest : public testing::Test {
protected:
    void SetUp() override
    {
        GlobalMockObject::reset();
    }

    void TearDown() override
    {
        GlobalMockObject::reset();
        // 清理静态成员，避免跨用例污染
        HcclCommunicator::linkResMap_.clear();
    }

    HcclCommunicator communicator_;
};

/**
 * @brief TC-COMM-01: ClearResMap 正常路径
 * 验证：resMap_ 中存在 tag → findTag=true，tag 被清除
 */
TEST_F(AclgraphCommunicatorTest, ClearResMap_Normal)
{
    std::string tag = "test_tag";
    bool findTag = false;

    communicator_.resMap_[tag] = AlgResourceResponse();

    HcclResult ret = communicator_.ClearResMap(tag, findTag, false);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_TRUE(findTag);
    EXPECT_EQ(communicator_.resMap_.count(tag), 0);
}

/**
 * @brief TC-COMM-02: ClearResMap tag 不存在
 * 验证：resMap_ 中不存在 tag → findTag=false
 */
TEST_F(AclgraphCommunicatorTest, ClearResMap_TagNotFound)
{
    std::string tag = "nonexistent_tag";
    bool findTag = true; // 初始为 true，验证被设为 false

    HcclResult ret = communicator_.ClearResMap(tag, findTag, false);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_FALSE(findTag);
}

/**
 * @brief TC-COMM-03: ClearResMap 传递 aclGraphDestroyCbk 参数
 * 验证：aclGraphDestroyCbk=true 时正常传递到 DestroyAlgResource
 */
TEST_F(AclgraphCommunicatorTest, ClearResMap_WithDestroyCbk)
{
    std::string tag = "test_tag";
    bool findTag = false;

    communicator_.resMap_[tag] = AlgResourceResponse();

    HcclResult ret = communicator_.ClearResMap(tag, findTag, true);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_TRUE(findTag);
    EXPECT_EQ(communicator_.resMap_.count(tag), 0);
}

/**
 * @brief TC-COMM-04: ClearOpResource 清除三种后缀
 * 验证：tag/tag_host/tag_device 三种后缀全部被清除
 */
TEST_F(AclgraphCommunicatorTest, ClearOpResource_AllSuffixes)
{
    std::string tag = "test_tag";

    communicator_.resMap_[tag] = AlgResourceResponse();
    communicator_.resMap_[tag + "_host"] = AlgResourceResponse();
    communicator_.resMap_[tag + "_device"] = AlgResourceResponse();

    HcclResult ret = communicator_.ClearOpResource(tag, false);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(communicator_.resMap_.count(tag), 0);
    EXPECT_EQ(communicator_.resMap_.count(tag + "_host"), 0);
    EXPECT_EQ(communicator_.resMap_.count(tag + "_device"), 0);
}

/**
 * @brief TC-COMM-05: ClearOpResource 部分后缀存在
 * 验证：只存在 tag/tag_host 时，不崩溃，正常清除存在的条目
 */
TEST_F(AclgraphCommunicatorTest, ClearOpResource_PartialSuffixes)
{
    std::string tag = "test_tag";

    communicator_.resMap_[tag] = AlgResourceResponse();
    communicator_.resMap_[tag + "_host"] = AlgResourceResponse();
    // 缺 _device

    HcclResult ret = communicator_.ClearOpResource(tag, false);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(communicator_.resMap_.count(tag), 0);
    EXPECT_EQ(communicator_.resMap_.count(tag + "_host"), 0);
    // _device 本来就不存在，不影响
}

/**
 * @brief TC-COMM-06: HasRoceTransportLinks 有 RDMA
 * 验证：transportRequests 中存在 isUsedRdma=true → true
 */
TEST_F(AclgraphCommunicatorTest, HasRoceTransportLinks_WithRdma)
{
    OpCommTransport tpt;
    tpt.resize(1);
    tpt[0].resize(1);
    tpt[0][0].transportRequests.resize(1);
    tpt[0][0].transportRequests[0].isUsedRdma = true;

    EXPECT_TRUE(communicator_.HasRoceTransportLinks(tpt));
}

/**
 * @brief TC-COMM-07: HasRoceTransportLinks 无 RDMA
 * 验证：transportRequests 中 isUsedRdma=false → false
 */
TEST_F(AclgraphCommunicatorTest, HasRoceTransportLinks_NoRdma)
{
    OpCommTransport tpt;
    tpt.resize(1);
    tpt[0].resize(1);
    tpt[0][0].transportRequests.resize(1);
    tpt[0][0].transportRequests[0].isUsedRdma = false;

    EXPECT_FALSE(communicator_.HasRoceTransportLinks(tpt));
}

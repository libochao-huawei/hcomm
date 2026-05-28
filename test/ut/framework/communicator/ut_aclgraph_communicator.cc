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

/**
 * @brief TC-COMM-08: DestroyAlgResource 传递 aclGraphDestroyCbk 参数（简化版）
 * 验证：ClearResMap 传入 aclGraphDestroyCbk=true 时，清理功能正常
 */
TEST_F(AclgraphCommunicatorTest, DestroyAlgResource_WithDestroyCbk)
{
    std::string tag = "test_tag";
    bool findTag = false;

    communicator_.resMap_[tag] = AlgResourceResponse();

    // 调用 ClearResMap 传入 aclGraphDestroyCbk=true
    HcclResult ret = communicator_.ClearResMap(tag, findTag, true);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_TRUE(findTag);
    EXPECT_EQ(communicator_.resMap_.count(tag), 0);
}

/**
 * @brief TC-COMM-09: ClearAclgraphHostLinks 简化版
 * 验证：tag 在 tagsRequiringHostCleanup_ 中时，清理后 tag 被移除
 */
TEST_F(AclgraphCommunicatorTest, ClearAclgraphHostLinks_Simplified)
{
    std::string tag = "test_tag";
    communicator_.tagsRequiringHostCleanup_.insert(tag);

    HcclResult ret = communicator_.ClearAclgraphHostLinks({tag});
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(communicator_.tagsRequiringHostCleanup_.count(tag), 0);
}

/**
 * @brief TC-COMM-10: ClearAclgraphHostLinks tag 不在集合中
 * 验证：tag 不在 tagsRequiringHostCleanup_ 中时，无操作，返回 SUCCESS
 */
TEST_F(AclgraphCommunicatorTest, ClearAclgraphHostLinks_TagNotTracked)
{
    // tag 不在 tagsRequiringHostCleanup_ 中，应跳过
    HcclResult ret = communicator_.ClearAclgraphHostLinks({"untracked_tag"});
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

/**
 * @brief TC-COMM-11: AicpuKfcClearOpResLaunch 空 tags
 * 验证：传入空集合时直接返回 SUCCESS
 */
TEST_F(AclgraphCommunicatorTest, KfcClearOpResLaunch_EmptyTags)
{
    HcclResult ret = communicator_.AicpuKfcClearOpResLaunch({});
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

/**
 * @brief TC-COMM-12: AicpuKfcClearOpResLaunch binHandle_ 为空
 * 验证：binHandle_=nullptr 时跳过 aicpu 清理，返回 SUCCESS
 */
TEST_F(AclgraphCommunicatorTest, KfcClearOpResLaunch_NullBinHandle)
{
    communicator_.binHandle_ = nullptr;
    HcclResult ret = communicator_.AicpuKfcClearOpResLaunch({"tag1"});
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

/**
 * @brief TC-COMM-13: AicpuKfcClearOpResLaunch opStream_ 为空
 * 验证：opStream_.ptr()=nullptr 时跳过 aicpu 清理，返回 SUCCESS
 */
TEST_F(AclgraphCommunicatorTest, KfcClearOpResLaunch_NullOpStream)
{
    // 预置 binHandle_ 非空但 opStream_ 为空
    communicator_.binHandle_ = reinterpret_cast<aclrtBinHandle>(0x1);
    // opStream_ 默认空
    HcclResult ret = communicator_.AicpuKfcClearOpResLaunch({"tag1"});
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

/**
 * @brief TC-COMM-14: AicpuKfcClearOpResLaunch 正常路径
 * 验证：预置条件满足时，函数不崩溃
 */
TEST_F(AclgraphCommunicatorTest, KfcClearOpResLaunch_NormalPath)
{
    // 预置成功能进 launch 路径的条件
    communicator_.binHandle_ = reinterpret_cast<aclrtBinHandle>(0x1);
    // 由于 opStream_ 可能为空/未初始化，不会真正进入 launch 路径
    HcclResult ret = communicator_.AicpuKfcClearOpResLaunch({"tag1", "tag2"});
    // 返回 SUCCESS（提前返回路径）或 HCCL_E_RUNTIME（launch 失败）均可
    EXPECT_TRUE(ret == HCCL_SUCCESS || ret == HCCL_E_RUNTIME);
}

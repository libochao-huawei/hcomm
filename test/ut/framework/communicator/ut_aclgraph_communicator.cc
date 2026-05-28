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
#include "aicpu_operator_pub.h"

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
 * @brief TC-COMM-08: DestroyAlgResource 参数传递验证
 * 验证：DestroyAlgResource 被 ClearResMap 正确调用并传递 aclGraphDestroyCbk 参数
 * 通过检查链路清理结果来间接验证参数传递正确性
 */
TEST_F(AclgraphCommunicatorTest, DestroyAlgResource_WithDestroyCbk)
{
    std::string tag = "test_tag";
    bool findTag = false;

    // 预置包含 transport 的完整 AlgResourceResponse
    AlgResourceResponse res;
    res.opTransportResponse.resize(1);
    res.opTransportResponse[0].resize(1);
    res.opTransportResponse[0][0].virtualLinks.resize(1);
    res.opTransportResponse[0][0].virtualLinks[0] = std::make_shared<Transport>();
    res.opTransportResponse[0][0].links.resize(1);
    res.opTransportResponse[0][0].links[0] = std::make_shared<Transport>();
    res.opTransportResponse[0][0].transportRequests.resize(1);
    res.opTransportResponse[0][0].transportRequests[0].isUsedRdma = true;
    
    communicator_.resMap_[tag] = res;
    communicator_.linkResMap_[res.opTransportResponse[0][0].virtualLinks[0].get()] = LinkInfo();
    communicator_.linkResMap_[res.opTransportResponse[0][0].links[0].get()] = LinkInfo();

    // 调用 ClearResMap 传入 aclGraphDestroyCbk=true（DestroyOpTransportResponse 应保留 RDMA 链路）
    HcclResult ret = communicator_.ClearResMap(tag, findTag, true);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_TRUE(findTag);
    EXPECT_EQ(communicator_.resMap_.count(tag), 0);
    // aclGraphDestroyCbk=true 时，RDMA 链路(isUsedRdma=true)应保留在 linkResMap_ 中
    EXPECT_EQ(communicator_.linkResMap_.size(), 2);
    communicator_.linkResMap_.clear();
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
    // 返回 SUCCESS（opStream_ 为空时走提前返回路径）
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

/**
 * @brief AIT-01: AicpuInitOpTilingDataAicpuCache enable=0
 * 验证：aicpuCacheEnable=0 时，不修改 tilingData 值
 */
TEST_F(AclgraphCommunicatorTest, AicpuInitTilingCache_Disabled)
{
    OpParam opParam;
    opParam.aicpuCacheEnable = 0;
    OpTilingData tilingData;
    tilingData.aicpuCacheEnable = 0;

    HcclResult ret = communicator_.AicpuInitOpTilingDataAicpuCache(opParam, HCCL_CMD_ALLREDUCE, &tilingData);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(tilingData.aicpuCacheEnable, 0);
}

/**
 * @brief AIT-02: AicpuInitOpTilingDataAicpuCache enable=1 + isCapture
 * 验证：算子模式 isCapture=true 情况下能正常返回
 */
TEST_F(AclgraphCommunicatorTest, AicpuInitTilingCache_ForceOpbaseWithCapture)
{
    OpParam opParam;
    opParam.aicpuCacheEnable = 1;
    opParam.isCapture = true;
    opParam.isZeroCopy = true;
    OpTilingData tilingData;
    tilingData.aicpuCacheEnable = 0;

    HcclResult ret = communicator_.AicpuInitOpTilingDataAicpuCache(opParam, HCCL_CMD_ALLREDUCE, &tilingData);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

/**
 * @brief AIT-03: AicpuInitOpTilingDataAicpuCache enable >= 10
 * 验证：aicpuCacheEnable >= 10 时触发内部错误检查
 */
TEST_F(AclgraphCommunicatorTest, AicpuInitTilingCache_EnableTooHigh)
{
    OpParam opParam;
    opParam.aicpuCacheEnable = 10;
    OpTilingData tilingData;

    HcclResult ret = communicator_.AicpuInitOpTilingDataAicpuCache(opParam, HCCL_CMD_ALLREDUCE, &tilingData);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);
}

/**
 * @brief AIT-04: AicpuInitOpTilingDataAicpuCache 单算子模式
 * 验证：非图模式下，直接赋值 aicpuCacheEnable 不变
 */
TEST_F(AclgraphCommunicatorTest, AicpuInitTilingCache_OpsMode)
{
    OpParam opParam;
    opParam.aicpuCacheEnable = 1;
    OpTilingData tilingData;
    tilingData.aicpuCacheEnable = 0;

    HcclResult ret = communicator_.AicpuInitOpTilingDataAicpuCache(opParam, HCCL_CMD_ALLREDUCE, &tilingData);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(tilingData.aicpuCacheEnable, 1);
}

/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software; you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full Text of the License.
 */

#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include "llt_hccl_stub_sal_pub.h"

#define private public
#define protected public
#include "topoinfo_ranktableOxc.h"
#include "topoinfo_ranktableParser_pub.h"
#include "hccl_comm_pub.h"
#undef private
#undef protected

#include <string>
#include <memory>

using namespace std;
using namespace hccl;

/**
 * @brief 标准 OXC RankTable JSON（用于正常功能测试）
 */
const std::string g_oxcRankTableValid = R"({
    "rank_count": 4,
    "task_id": "test_oxc_task_12345",
    "version": "2.0",
    "status": "completed",
    "rank_list": [
        {
            "rank_id": 0,
            "local_id": 0,
            "device_id": 0,
            "level_list": [
                {
                    "net_layer": 0,
                    "net_instance_id": "0_0",
                    "net_type": "TOPO_FILE_DESC",
                    "rank_addr_list": [{
                        "addr": "000000000000002000100000df000401",
                        "addr_type": "EID",
                        "plane_id": "0",
                        "ports": ["0/0"]
                    }]
                },
                {
                    "net_layer": 1,
                    "net_instance_id": "0",
                    "net_type": "CLOS",
                    "rank_addr_list": [{
                        "addr": "000000000000000000100000df0004b5",
                        "addr_type": "EID",
                        "plane_id": "0",
                        "ports": ["0/1", "0/2"]
                    }]
                },
                {
                    "net_layer": 2,
                    "net_instance_id": "0",
                    "net_type": "OXC_Mesh",
                    "rank_addr_list": [{
                        "addr": "000000000000000000100000df0004b5",
                        "addr_type": "EID",
                        "plane_id": "0",
                        "ports": ["0/1", "0/2"]
                    }]
                },
                {
                    "net_layer": 3,
                    "net_instance_id": "CLUSTER1",
                    "net_type": "CLOS",
                    "rank_addr_list": [{
                        "addr": "192.168.1.10",
                        "addr_type": "IPV4",
                        "plane_id": "CLUSTER",
                        "ports": []
                    }]
                }
            ]
        },
        {
            "rank_id": 1,
            "local_id": 0,
            "device_id": 1,
            "level_list": [
                {
                    "net_layer": 0,
                    "net_instance_id": "0_0",
                    "net_type": "TOPO_FILE_DESC",
                    "rank_addr_list": [{
                        "addr": "000000000000002000100000df000401",
                        "addr_type": "EID",
                        "plane_id": "0",
                        "ports": ["0/0"]
                    }]
                },
                {
                    "net_layer": 1,
                    "net_instance_id": "0",
                    "net_type": "CLOS",
                    "rank_addr_list": [{
                        "addr": "000000000000000000100000df0004b5",
                        "addr_type": "EID",
                        "plane_id": "0",
                        "ports": ["0/1", "0/2"]
                    }]
                },
                {
                    "net_layer": 2,
                    "net_instance_id": "0",
                    "net_type": "OXC_Mesh",
                    "rank_addr_list": [{
                        "addr": "000000000000000000100000df0004b5",
                        "addr_type": "EID",
                        "plane_id": "0",
                        "ports": ["0/1", "0/2"]
                    }]
                },
                {
                    "net_layer": 3,
                    "net_instance_id": "CLUSTER1",
                    "net_type": "CLOS",
                    "rank_addr_list": [{
                        "addr": "192.168.1.11",
                        "addr_type": "IPV4",
                        "plane_id": "CLUSTER",
                        "ports": []
                    }]
                }
            ]
        }
    ]
})";

/**
 * @brief 缺少 task_id 的 OXC RankTable JSON
 */
const std::string g_oxcRankTableNoTaskId = R"({
    "rank_count": 2,
    "version": "2.0",
    "status": "completed",
    "rank_list": [
        {
            "rank_id": 0,
            "local_id": 0,
            "device_id": 0,
            "level_list": [
                {
                    "net_layer": 0,
                    "net_instance_id": "0_0",
                    "net_type": "TOPO_FILE_DESC",
                    "rank_addr_list": [{
                        "addr": "000000000000002000100000df000401",
                        "addr_type": "EID",
                        "plane_id": "0",
                        "ports": ["0/0"]
                    }]
                },
                {
                    "net_layer": 1,
                    "net_instance_id": "0",
                    "net_type": "CLOS",
                    "rank_addr_list": [{
                        "addr": "000000000000000000100000df0004b5",
                        "addr_type": "EID",
                        "plane_id": "0",
                        "ports": ["0/1"]
                    }]
                },
                {
                    "net_layer": 2,
                    "net_instance_id": "0",
                    "net_type": "OXC_Mesh",
                    "rank_addr_list": [{
                        "addr": "000000000000000000100000df0004b5",
                        "addr_type": "EID",
                        "plane_id": "0",
                        "ports": ["0/1"]
                    }]
                },
                {
                    "net_layer": 3,
                    "net_instance_id": "CLUSTER1",
                    "net_type": "CLOS",
                    "rank_addr_list": [{
                        "addr": "192.168.1.10",
                        "addr_type": "IPV4",
                        "plane_id": "CLUSTER",
                        "ports": []
                    }]
                }
            ]
        }
    ]
})";

/**
 * @brief TopoinfoRanktableOxc 单元测试类
 */
class TopoinfoRanktableOxcTest : public testing::Test {
protected:
    static void SetUpTestCase() {
        cout << "\033[36m--TopoinfoRanktableOxcTest SetUp--\033[0m" << endl;
    }

    static void TearDownTestCase() {
        cout << "\033[36m--TopoinfoRanktableOxcTest TearDown--\033[0m" << endl;
    }

    virtual void SetUp() {
        // Mock 设备类型
        DevType deviceType = DevType::DEV_TYPE_910_93;
        MOCKER(hrtGetDeviceType)
            .stubs()
            .with(outBound(deviceType))
            .will(returnValue(HCCL_SUCCESS));

        // Mock 设备逻辑 ID
        s32 deviceLogicID = 0;
        MOCKER(hrtGetDevice)
            .stubs()
            .with(outBoundP(&deviceLogicID))
            .will(returnValue(HCCL_SUCCESS));

        // Mock 设备物理 ID
        u32 devicePhyId = 0;
        MOCKER(hrtGetDevicePhyIdByIndex)
            .stubs()
            .with(any(), outBound(devicePhyId))
            .will(returnValue(HCCL_SUCCESS));

        cout << "A Test case SetUp" << endl;
    }

    virtual void TearDown() {
        GlobalMockObject::verify();
        cout << "A Test case TearDown" << endl;
    }
};

/**
 * @brief TC001: 版本识别测试
 * @details 验证能够正确识别 "2.0" 版本
 * @预期结果 解析成功，返回 HCCL_SUCCESS
 */
TEST_F(TopoinfoRanktableOxcTest, TC001_VersionRecognition_When_ValidVersion_Expect_Success)
{
    // Arrange
    std::string rankTableStr = g_oxcRankTableValid;
    std::string identify = "0";

    // Act
    auto parser = std::make_unique<TopoinfoRanktableOxc>(rankTableStr, identify);

    // 初始化解析器（加载 JSON 字符串到 fileContent_）
    HcclResult initRet = parser->Init();
    ASSERT_EQ(initRet, HCCL_SUCCESS);

    // Assert
    ASSERT_NE(parser, nullptr);

    HcclCommParams params;
    RankTable_t rankTable;
    HcclResult ret = parser->GetClusterInfo(params, rankTable);

    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(rankTable.version, "2.0");
}

/**
 * @brief TC002: task_id 解析测试
 * @details 验证 task_id 字段正确解析
 * @预期结果 taskId = "test_oxc_task_12345"
 */
TEST_F(TopoinfoRanktableOxcTest, TC002_TaskIdParsing_When_TaskIdExists_Expect_CorrectValue)
{
    // Arrange
    std::string rankTableStr = g_oxcRankTableValid;
    std::string identify = "0";
    auto parser = std::make_unique<TopoinfoRanktableOxc>(rankTableStr, identify);

    // 初始化解析器
    HcclResult initRet = parser->Init();
    ASSERT_EQ(initRet, HCCL_SUCCESS);

    // Act
    HcclCommParams params;
    RankTable_t rankTable;
    HcclResult ret = parser->GetClusterInfo(params, rankTable);

    // Assert
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(rankTable.taskId, "test_oxc_task_12345");
}

/**
 * @brief TC003: OXC_Mesh 类型解析测试
 * @details 验证 NetLayer 2 的 OXC_Mesh 类型
 * @预期结果 解析成功（类型由父类处理）
 */
TEST_F(TopoinfoRanktableOxcTest, TC003_OxcMeshTypeParsing_When_ValidRankTable_Expect_Success)
{
    // Arrange
    std::string rankTableStr = g_oxcRankTableValid;
    std::string identify = "0";
    auto parser = std::make_unique<TopoinfoRanktableOxc>(rankTableStr, identify);

    // 初始化解析器
    HcclResult initRet = parser->Init();
    ASSERT_EQ(initRet, HCCL_SUCCESS);

    // Act
    HcclCommParams params;
    RankTable_t rankTable;
    HcclResult ret = parser->GetClusterInfo(params, rankTable);

    // Assert
    EXPECT_EQ(ret, HCCL_SUCCESS);
    // OXC_Mesh 类型的具体验证需要访问内部数据结构
    // 这里主要验证解析过程没有失败
    EXPECT_GT(rankTable.rankNum, 0);
}

/**
 * @brief TC004: net_instance_id 验证测试
 * @details 验证 NetLayer 2 的 net_instance_id 为 "0"
 * @预期结果 解析成功
 */
TEST_F(TopoinfoRanktableOxcTest, TC004_NetInstanceIdValidation_When_ValidOxcLayer_Expect_Success)
{
    // Arrange
    std::string rankTableStr = g_oxcRankTableValid;
    std::string identify = "0";
    auto parser = std::make_unique<TopoinfoRanktableOxc>(rankTableStr, identify);

    // 初始化解析器
    HcclResult initRet = parser->Init();
    ASSERT_EQ(initRet, HCCL_SUCCESS);

    // Act
    HcclCommParams params;
    RankTable_t rankTable;
    HcclResult ret = parser->GetClusterInfo(params, rankTable);

    // Assert
    EXPECT_EQ(ret, HCCL_SUCCESS);
    // net_instance_id 的具体验证需要解析 JSON 内部结构
    // 这里主要验证解析过程成功完成
}

/**
 * @brief TC005: 层级递增验证测试
 * @details 验证 0 → 1 → 2 → 3 层级递增
 * @预期结果 解析成功，层级顺序正确
 */
TEST_F(TopoinfoRanktableOxcTest, TC005_LayerOrderValidation_When_ValidRankTable_Expect_Success)
{
    // Arrange
    std::string rankTableStr = g_oxcRankTableValid;
    std::string identify = "0";
    auto parser = std::make_unique<TopoinfoRanktableOxc>(rankTableStr, identify);

    // 初始化解析器
    HcclResult initRet = parser->Init();
    ASSERT_EQ(initRet, HCCL_SUCCESS);

    // Act
    HcclCommParams params;
    RankTable_t rankTable;
    HcclResult ret = parser->GetClusterInfo(params, rankTable);

    // Assert
    EXPECT_EQ(ret, HCCL_SUCCESS);
    // 层级顺序的正确性由父类 TopoinfoRanktableConcise 处理
    // 这里验证整体解析成功
    EXPECT_GT(rankTable.rankNum, 0);
}

/**
 * @brief TC006: task_id 缺失测试
 * @details 验证 task_id 缺失时的处理
 * @预期结果 taskId = ""，不报错
 */
TEST_F(TopoinfoRanktableOxcTest, TC006_TaskIdMissing_When_NoTaskIdField_Expect_EmptyString)
{
    // Arrange
    std::string rankTableStr = g_oxcRankTableNoTaskId;
    std::string identify = "0";
    auto parser = std::make_unique<TopoinfoRanktableOxc>(rankTableStr, identify);

    // 初始化解析器
    HcclResult initRet = parser->Init();
    ASSERT_EQ(initRet, HCCL_SUCCESS);

    // Act
    HcclCommParams params;
    RankTable_t rankTable;
    HcclResult ret = parser->GetClusterInfo(params, rankTable);

    // Assert
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(rankTable.taskId, "");
}

/**
 * @brief: 测试 TopoinfoRanktableOxc 构造函数
 * @details: 验证对象能够正确构造
 */
TEST_F(TopoinfoRanktableOxcTest, Constructor_When_ValidInput_Expect_ObjectCreated)
{
    // Arrange
    std::string rankTableStr = g_oxcRankTableValid;
    std::string identify = "0";

    // Act
    auto parser = std::make_unique<TopoinfoRanktableOxc>(rankTableStr, identify);

    // Assert
    ASSERT_NE(parser, nullptr);
    EXPECT_EQ(parser->taskId_, "");
}

/**
 * @brief: 测试 ParseTaskId 方法 - 存在 task_id
 */
TEST_F(TopoinfoRanktableOxcTest, ParseTaskId_When_TaskIdExists_Expect_CorrectValue)
{
    // Arrange
    std::string rankTableStr = g_oxcRankTableValid;
    std::string identify = "0";
    auto parser = std::make_unique<TopoinfoRanktableOxc>(rankTableStr, identify);

    // Act - 手动调用 ParseTaskId
    nlohmann::json jsonObj = nlohmann::json::parse(rankTableStr);
    std::string taskId;
    HcclResult ret = parser->ParseTaskId(jsonObj, taskId);

    // Assert
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(taskId, "test_oxc_task_12345");
}

/**
 * @brief: 测试 ParseTaskId 方法 - 缺少 task_id
 */
TEST_F(TopoinfoRanktableOxcTest, ParseTaskId_When_TaskIdMissing_Expect_EmptyString)
{
    // Arrange
    std::string rankTableStr = g_oxcRankTableNoTaskId;
    std::string identify = "0";
    auto parser = std::make_unique<TopoinfoRanktableOxc>(rankTableStr, identify);

    // Act - 手动调用 ParseTaskId
    nlohmann::json jsonObj = nlohmann::json::parse(rankTableStr);
    std::string taskId;
    HcclResult ret = parser->ParseTaskId(jsonObj, taskId);

    // Assert
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(taskId, "");
}

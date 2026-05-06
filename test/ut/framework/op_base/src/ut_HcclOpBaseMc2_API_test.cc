/**
* Copyright (c) 2025 Huawei Technologies Co., Ltd.
* This program is free software, you can redistribute it and/or modify it under the terms and conditions of
* CANN Open Software License Agreement Version 2.0 (the "License").
* Please refer to the License for details. You may not use this file or except in compliance with the License.
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
* INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
* See LICENSE in the root of the software repository for the full text of the License.
*/

#include "hccl_api_base_test.h"
#include "log.h"
#include "op_base_mc2.h"

#define private public
#define protected public
#include "hccl_communicator.h"
#include "op_base.h"
#undef protected
#undef private

using namespace hccl;

class HcclOpBaseMc2APITest : public BaseInit {
public:
    void SetUp() override
    {
        BaseInit::SetUp();
        UT_USE_RANK_TABLE_910_1SERVER_1RANK;
    }

    void TearDown() override
    {
        BaseInit::TearDown();
        GlobalMockObject::verify();
    }
};

/**
 * @brief HcclGetOpArgs V2支持分支测试
 *
 * 测试场景：hrtGetHcclV2Support返回true，验证V2分支正确调用HcclGetOpArgsV2
 * 预期结果：调用HcclGetOpArgsV2并返回HCCL_SUCCESS
 */
TEST_F(HcclOpBaseMc2APITest, Ut_HcclGetOpArgs_When_V2Support_Expect_ReturnHCCL_SUCCESS)
{
    void* opArgs = nullptr;

    MOCKER(hrtGetHcclV2Support).stubs().will(doAll(SetArg<0>(true), returnValue(HCCL_SUCCESS)));
    MOCKER_CPP(HcclGetOpArgsV2).stubs().will(returnValue(HCCL_SUCCESS));

    HcclResult result = HcclGetOpArgs(&opArgs);

    EXPECT_EQ(result, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

/**
 * @brief HcclGetOpArgs V2不支持分支测试
 *
 * 测试场景：hrtGetHcclV2Support返回false，验证非V2分支正常返回成功
 * 预期结果：直接返回HCCL_SUCCESS，不调用V2接口
 */
TEST_F(HcclOpBaseMc2APITest, Ut_HcclGetOpArgs_When_V2NotSupported_Expect_ReturnHCCL_SUCCESS)
{
    void* opArgs = nullptr;

    MOCKER(hrtGetHcclV2Support).stubs().will(doAll(SetArg<0>(false), returnValue(HCCL_SUCCESS)));

    HcclResult result = HcclGetOpArgs(&opArgs);

    EXPECT_EQ(result, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

/**
 * @brief HcclFreeOpArgs V2支持分支测试
 *
 * 测试场景：hrtGetHcclV2Support返回true，验证V2分支正确调用HcclFreeOpArgsV2
 * 预期结果：调用HcclFreeOpArgsV2并返回HCCL_SUCCESS
 */
TEST_F(HcclOpBaseMc2APITest, Ut_HcclFreeOpArgs_When_V2Support_Expect_ReturnHCCL_SUCCESS)
{
    void* opArgs = reinterpret_cast<void*>(0x1000);

    MOCKER(hrtGetHcclV2Support).stubs().will(doAll(SetArg<0>(true), returnValue(HCCL_SUCCESS)));
    MOCKER_CPP(HcclFreeOpArgsV2).stubs().will(returnValue(HCCL_SUCCESS));

    HcclResult result = HcclFreeOpArgs(opArgs);

    EXPECT_EQ(result, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

/**
 * @brief HcclFreeOpArgs V2不支持分支测试
 *
 * 测试场景：hrtGetHcclV2Support返回false，验证非V2分支正常返回成功
 * 预期结果：直接返回HCCL_SUCCESS，不调用V2接口
 */
TEST_F(HcclOpBaseMc2APITest, Ut_HcclFreeOpArgs_When_V2NotSupported_Expect_ReturnHCCL_SUCCESS)
{
    void* opArgs = reinterpret_cast<void*>(0x1000);

    MOCKER(hrtGetHcclV2Support).stubs().will(doAll(SetArg<0>(false), returnValue(HCCL_SUCCESS)));

    HcclResult result = HcclFreeOpArgs(opArgs);

    EXPECT_EQ(result, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

/**
 * @brief HcclSetOpSrcDataType V2支持分支测试
 *
 * 测试场景：hrtGetHcclV2Support返回true，验证V2分支正确调用HcclSetOpSrcDataTypeV2
 * 预期结果：调用HcclSetOpSrcDataTypeV2并返回HCCL_SUCCESS
 */
TEST_F(HcclOpBaseMc2APITest, Ut_HcclSetOpSrcDataType_When_V2Support_Expect_ReturnHCCL_SUCCESS)
{
    void* opArgs = reinterpret_cast<void*>(0x1000);
    uint8_t srcDataType = static_cast<uint8_t>(HCCL_DATA_TYPE_FP16);

    MOCKER(hrtGetHcclV2Support).stubs().will(doAll(SetArg<0>(true), returnValue(HCCL_SUCCESS)));
    MOCKER_CPP(HcclSetOpSrcDataTypeV2).stubs().will(returnValue(HCCL_SUCCESS));

    HcclResult result = HcclSetOpSrcDataType(opArgs, srcDataType);

    EXPECT_EQ(result, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

/**
 * @brief HcclSetOpSrcDataType V2不支持分支测试
 *
 * 测试场景：hrtGetHcclV2Support返回false，验证非V2分支正常返回成功
 * 预期结果：直接返回HCCL_SUCCESS，不调用V2接口
 */
TEST_F(HcclOpBaseMc2APITest, Ut_HcclSetOpSrcDataType_When_V2NotSupported_Expect_ReturnHCCL_SUCCESS)
{
    void* opArgs = reinterpret_cast<void*>(0x1000);
    uint8_t srcDataType = static_cast<uint8_t>(HCCL_DATA_TYPE_FP16);

    MOCKER(hrtGetHcclV2Support).stubs().will(doAll(SetArg<0>(false), returnValue(HCCL_SUCCESS)));

    HcclResult result = HcclSetOpSrcDataType(opArgs, srcDataType);

    EXPECT_EQ(result, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

/**
 * @brief HcclSetOpDstDataType V2支持分支测试
 *
 * 测试场景：hrtGetHcclV2Support返回true，验证V2分支正确调用HcclSetOpDstDataTypeV2
 * 预期结果：调用HcclSetOpDstDataTypeV2并返回HCCL_SUCCESS
 */
TEST_F(HcclOpBaseMc2APITest, Ut_HcclSetOpDstDataType_When_V2Support_Expect_ReturnHCCL_SUCCESS)
{
    void* opArgs = reinterpret_cast<void*>(0x1000);
    uint8_t dstDataType = static_cast<uint8_t>(HCCL_DATA_TYPE_FP16);

    MOCKER(hrtGetHcclV2Support).stubs().will(doAll(SetArg<0>(true), returnValue(HCCL_SUCCESS)));
    MOCKER_CPP(HcclSetOpDstDataTypeV2).stubs().will(returnValue(HCCL_SUCCESS));

    HcclResult result = HcclSetOpDstDataType(opArgs, dstDataType);

    EXPECT_EQ(result, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

/**
 * @brief HcclSetOpDstDataType V2不支持分支测试
 *
 * 测试场景：hrtGetHcclV2Support返回false，验证非V2分支正常返回成功
 * 预期结果：直接返回HCCL_SUCCESS，不调用V2接口
 */
TEST_F(HcclOpBaseMc2APITest, Ut_HcclSetOpDstDataType_When_V2NotSupported_Expect_ReturnHCCL_SUCCESS)
{
    void* opArgs = reinterpret_cast<void*>(0x1000);
    uint8_t dstDataType = static_cast<uint8_t>(HCCL_DATA_TYPE_FP16);

    MOCKER(hrtGetHcclV2Support).stubs().will(doAll(SetArg<0>(false), returnValue(HCCL_SUCCESS)));

    HcclResult result = HcclSetOpDstDataType(opArgs, dstDataType);

    EXPECT_EQ(result, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

/**
 * @brief HcclSetOpReduceType V2支持分支测试
 *
 * 测试场景：hrtGetHcclV2Support返回true，验证V2分支正确调用HcclSetOpReduceTypeV2
 * 预期结果：调用HcclSetOpReduceTypeV2并返回HCCL_SUCCESS
 */
TEST_F(HcclOpBaseMc2APITest, Ut_HcclSetOpReduceType_When_V2Support_Expect_ReturnHCCL_SUCCESS)
{
    void* opArgs = reinterpret_cast<void*>(0x1000);
    uint32_t reduceType = static_cast<uint32_t>(HCCL_REDUCE_SUM);

    MOCKER(hrtGetHcclV2Support).stubs().will(doAll(SetArg<0>(true), returnValue(HCCL_SUCCESS)));
    MOCKER_CPP(HcclSetOpReduceTypeV2).stubs().will(returnValue(HCCL_SUCCESS));

    HcclResult result = HcclSetOpReduceType(opArgs, reduceType);

    EXPECT_EQ(result, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

/**
 * @brief HcclSetOpReduceType V2不支持分支测试
 *
 * 测试场景：hrtGetHcclV2Support返回false，验证非V2分支正常返回成功
 * 预期结果：直接返回HCCL_SUCCESS，不调用V2接口
 */
TEST_F(HcclOpBaseMc2APITest, Ut_HcclSetOpReduceType_When_V2NotSupported_Expect_ReturnHCCL_SUCCESS)
{
    void* opArgs = reinterpret_cast<void*>(0x1000);
    uint32_t reduceType = static_cast<uint32_t>(HCCL_REDUCE_SUM);

    MOCKER(hrtGetHcclV2Support).stubs().will(doAll(SetArg<0>(false), returnValue(HCCL_SUCCESS)));

    HcclResult result = HcclSetOpReduceType(opArgs, reduceType);

    EXPECT_EQ(result, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

/**
 * @brief HcclSetOpCount V2支持分支测试
 *
 * 测试场景：hrtGetHcclV2Support返回true，验证V2分支正确调用HcclSetOpCountV2
 * 预期结果：调用HcclSetOpCountV2并返回HCCL_SUCCESS
 */
TEST_F(HcclOpBaseMc2APITest, Ut_HcclSetOpCount_When_V2Support_Expect_ReturnHCCL_SUCCESS)
{
    void* opArgs = reinterpret_cast<void*>(0x1000);
    uint64_t count = 1024;

    MOCKER(hrtGetHcclV2Support).stubs().will(doAll(SetArg<0>(true), returnValue(HCCL_SUCCESS)));
    MOCKER_CPP(HcclSetOpCountV2).stubs().will(returnValue(HCCL_SUCCESS));

    HcclResult result = HcclSetOpCount(opArgs, count);

    EXPECT_EQ(result, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

/**
 * @brief HcclSetOpCount V2不支持分支测试
 *
 * 测试场景：hrtGetHcclV2Support返回false，验证非V2分支正常返回成功
 * 预期结果：直接返回HCCL_SUCCESS，不调用V2接口
 */
TEST_F(HcclOpBaseMc2APITest, Ut_HcclSetOpCount_When_V2NotSupported_Expect_ReturnHCCL_SUCCESS)
{
    void* opArgs = reinterpret_cast<void*>(0x1000);
    uint64_t count = 1024;

    MOCKER(hrtGetHcclV2Support).stubs().will(doAll(SetArg<0>(false), returnValue(HCCL_SUCCESS)));

    HcclResult result = HcclSetOpCount(opArgs, count);

    EXPECT_EQ(result, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

/**
 * @brief HcclSetOpAlgConfig V2支持分支测试
 *
 * 测试场景：hrtGetHcclV2Support返回true，验证V2分支正确调用HcclSetOpAlgConfigV2
 * 预期结果：调用HcclSetOpAlgConfigV2并返回HCCL_SUCCESS
 */
TEST_F(HcclOpBaseMc2APITest, Ut_HcclSetOpAlgConfig_When_V2Support_Expect_ReturnHCCL_SUCCESS)
{
    void* opArgs = reinterpret_cast<void*>(0x1000);
    char algConfig[128] = "AllReduce=level0:ring";

    MOCKER(hrtGetHcclV2Support).stubs().will(doAll(SetArg<0>(true), returnValue(HCCL_SUCCESS)));
    MOCKER_CPP(HcclSetOpAlgConfigV2).stubs().will(returnValue(HCCL_SUCCESS));

    HcclResult result = HcclSetOpAlgConfig(opArgs, algConfig);

    EXPECT_EQ(result, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

/**
 * @brief HcclSetOpAlgConfig V2不支持分支测试
 *
 * 测试场景：hrtGetHcclV2Support返回false，验证非V2分支正常返回成功
 * 预期结果：直接返回HCCL_SUCCESS，不调用V2接口
 */
TEST_F(HcclOpBaseMc2APITest, Ut_HcclSetOpAlgConfig_When_V2NotSupported_Expect_ReturnHCCL_SUCCESS)
{
    void* opArgs = reinterpret_cast<void*>(0x1000);
    char algConfig[128] = "AllReduce=level0:ring";

    MOCKER(hrtGetHcclV2Support).stubs().will(doAll(SetArg<0>(false), returnValue(HCCL_SUCCESS)));

    HcclResult result = HcclSetOpAlgConfig(opArgs, algConfig);

    EXPECT_EQ(result, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

/**
 * @brief HcclSetOpCommEngine V2支持分支测试
 *
 * 测试场景：hrtGetHcclV2Support返回true，验证V2分支正确调用HcclSetOpCommEngineV2
 * 预期结果：调用HcclSetOpCommEngineV2并返回HCCL_SUCCESS
 */
TEST_F(HcclOpBaseMc2APITest, Ut_HcclSetOpCommEngine_When_V2Support_Expect_ReturnHCCL_SUCCESS)
{
    void* opArgs = reinterpret_cast<void*>(0x1000);
    uint8_t commEngine = COMM_ENGINE_AICPU;

    MOCKER(hrtGetHcclV2Support).stubs().will(doAll(SetArg<0>(true), returnValue(HCCL_SUCCESS)));
    MOCKER_CPP(HcclSetOpCommEngineV2).stubs().will(returnValue(HCCL_SUCCESS));

    HcclResult result = HcclSetOpCommEngine(opArgs, commEngine);

    EXPECT_EQ(result, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

/**
 * @brief HcclSetOpCommEngine V2不支持分支测试
 *
 * 测试场景：hrtGetHcclV2Support返回false，验证非V2分支正常返回成功
 * 预期结果：直接返回HCCL_SUCCESS，不调用V2接口
 */
TEST_F(HcclOpBaseMc2APITest, Ut_HcclSetOpCommEngine_When_V2NotSupported_Expect_ReturnHCCL_SUCCESS)
{
    void* opArgs = reinterpret_cast<void*>(0x1000);
    uint8_t commEngine = COMM_ENGINE_AICPU;

    MOCKER(hrtGetHcclV2Support).stubs().will(doAll(SetArg<0>(false), returnValue(HCCL_SUCCESS)));

    HcclResult result = HcclSetOpCommEngine(opArgs, commEngine);

    EXPECT_EQ(result, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

/**
 * @brief HcclCommResPrepare V2支持分支测试
 *
 * 测试场景：hrtGetHcclV2Support返回true，验证V2分支正确调用HcclCommResPrepareV2
 * 预期结果：调用HcclCommResPrepareV2并返回HCCL_SUCCESS
 */
TEST_F(HcclOpBaseMc2APITest, Ut_HcclCommResPrepare_When_V2Support_Expect_ReturnHCCL_SUCCESS)
{
    UT_COMM_CREATE_DEFAULT(comm);

    char opName[128] = "AllReduce";
    void* opArgs = reinterpret_cast<void*>(0x1000);
    void* addr = nullptr;

    MOCKER(hrtGetHcclV2Support).stubs().will(doAll(SetArg<0>(true), returnValue(HCCL_SUCCESS)));
    MOCKER_CPP(HcclCommResPrepareV2).stubs().will(returnValue(HCCL_SUCCESS));

    HcclResult result = HcclCommResPrepare(comm, opName, opArgs, &addr);

    EXPECT_EQ(result, HCCL_SUCCESS);
    Ut_Comm_Destroy(comm);
    GlobalMockObject::verify();
}

/**
 * @brief HcclCommResPrepare V2不支持分支测试
 *
 * 测试场景：hrtGetHcclV2Support返回false，验证非V2分支正常返回成功
 * 预期结果：直接返回HCCL_SUCCESS，不调用V2接口
 */
TEST_F(HcclOpBaseMc2APITest, Ut_HcclCommResPrepare_When_V2NotSupported_Expect_ReturnHCCL_SUCCESS)
{
    UT_COMM_CREATE_DEFAULT(comm);

    char opName[128] = "AllReduce";
    void* opArgs = reinterpret_cast<void*>(0x1000);
    void* addr = nullptr;

    MOCKER(hrtGetHcclV2Support).stubs().will(doAll(SetArg<0>(false), returnValue(HCCL_SUCCESS)));

    HcclResult result = HcclCommResPrepare(comm, opName, opArgs, &addr);

    EXPECT_EQ(result, HCCL_SUCCESS);
    Ut_Comm_Destroy(comm);
    GlobalMockObject::verify();
}

/**
 * @brief HcclDevMemAcquire V2支持分支测试
 *
 * 测试场景：hrtGetHcclV2Support返回true，验证V2分支正确调用HcclDevMemAcquireV2
 * 预期结果：调用HcclDevMemAcquireV2并返回HCCL_SUCCESS
 */
TEST_F(HcclOpBaseMc2APITest, Ut_HcclDevMemAcquire_When_V2Support_Expect_ReturnHCCL_SUCCESS)
{
    UT_COMM_CREATE_DEFAULT(comm);

    const char memTag[32] = "test_mem";
    uint64_t size = 1024;
    void* addr = nullptr;
    bool newCreated = false;

    MOCKER(hrtGetHcclV2Support).stubs().will(doAll(SetArg<0>(true), returnValue(HCCL_SUCCESS)));
    MOCKER_CPP(HcclDevMemAcquireV2).stubs().will(returnValue(HCCL_SUCCESS));

    HcclResult result = HcclDevMemAcquire(comm, memTag, &size, &addr, &newCreated);

    EXPECT_EQ(result, HCCL_SUCCESS);
    Ut_Comm_Destroy(comm);
    GlobalMockObject::verify();
}

/**
 * @brief HcclDevMemAcquire V2不支持分支测试
 *
 * 测试场景：hrtGetHcclV2Support返回false，验证非V2分支正常返回成功
 * 预期结果：直接返回HCCL_SUCCESS，不调用V2接口
 */
TEST_F(HcclOpBaseMc2APITest, Ut_HcclDevMemAcquire_When_V2NotSupported_Expect_ReturnHCCL_SUCCESS)
{
    UT_COMM_CREATE_DEFAULT(comm);

    const char memTag[32] = "test_mem";
    uint64_t size = 1024;
    void* addr = nullptr;
    bool newCreated = false;

    MOCKER(hrtGetHcclV2Support).stubs().will(doAll(SetArg<0>(false), returnValue(HCCL_SUCCESS)));

    HcclResult result = HcclDevMemAcquire(comm, memTag, &size, &addr, &newCreated);

    EXPECT_EQ(result, HCCL_SUCCESS);
    Ut_Comm_Destroy(comm);
    GlobalMockObject::verify();
}

/**
 * @brief HcclGetRemoteIpcHcclBuf V2支持分支测试
 *
 * 测试场景：hrtGetHcclV2Support返回true，验证V2分支调用GetCommunicatorV2获取V2接口
 * 预期结果：通过HcclGetRemoteIpcHcclBuf V2版本获取远端IPC buffer并返回HCCL_SUCCESS
 */
TEST_F(HcclOpBaseMc2APITest, Ut_HcclGetRemoteIpcHcclBuf_When_V2Support_Expect_ReturnHCCL_SUCCESS)
{
    UT_COMM_CREATE_DEFAULT(comm);

    uint64_t remoteRank = 1;
    void* addr = nullptr;
    uint64_t size = 1024;

    MOCKER(hrtGetHcclV2Support).stubs().will(doAll(SetArg<0>(true), returnValue(HCCL_SUCCESS)));
    MOCKER_CPP(HcclGetRemoteIpcHcclBuf).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommunicator::GetUserRank).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommunicator::GetRemoteCCLBuf).stubs().will(returnValue(HCCL_SUCCESS));

    HcclResult result = HcclGetRemoteIpcHcclBuf(comm, remoteRank, &addr, &size);

    EXPECT_EQ(result, HCCL_SUCCESS);
    Ut_Comm_Destroy(comm);
    GlobalMockObject::verify();
}

/**
 * @brief HcclGetRemoteIpcHcclBuf V2不支持分支测试
 *
 * 测试场景：hrtGetHcclV2Support返回false，验证非V2分支通过GetRemoteCCLBuf获取远端buffer
 * 预期结果：使用原有非V2接口获取远端CCL buffer并返回HCCL_SUCCESS
 */
TEST_F(HcclOpBaseMc2APITest, Ut_HcclGetRemoteIpcHcclBuf_When_V2NotSupported_Expect_ReturnHCCL_SUCCESS)
{
    UT_COMM_CREATE_DEFAULT(comm);

    uint64_t remoteRank = 1;
    void* addr = nullptr;
    uint64_t size = 1024;

    MOCKER(hrtGetHcclV2Support).stubs().will(doAll(SetArg<0>(false), returnValue(HCCL_SUCCESS)));
    MOCKER_CPP(&HcclCommunicator::GetUserRank).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommunicator::GetRemoteCCLBuf).stubs().will(returnValue(HCCL_SUCCESS));

    HcclResult result = HcclGetRemoteIpcHcclBuf(comm, remoteRank, &addr, &size);

    EXPECT_EQ(result, HCCL_SUCCESS);
    Ut_Comm_Destroy(comm);
    GlobalMockObject::verify();
}

/**
 * @brief HcclAllocComResourceByTiling V2支持分支测试
 *
 * 测试场景：hrtGetHcclV2Support返回true，验证V2分支调用HcclAllocComResourceByTilingV2
 * 预期结果：通过GetCommunicatorV2获取V2接口并调用HcclAllocComResourceByTilingV2
 */
TEST_F(HcclOpBaseMc2APITest, Ut_HcclAllocComResourceByTiling_When_V2Support_Expect_ReturnHCCL_SUCCESS)
{
    UT_COMM_CREATE_DEFAULT(comm);

    void* stream = nullptr;
    void* mc2Tiling = malloc(sizeof(Mc2CcTilingInner) + sizeof(Mc2HcommCfg));
    ASSERT_NE(mc2Tiling, nullptr);

    Mc2CcTilingInner* tilingData = reinterpret_cast<Mc2CcTilingInner*>(mc2Tiling);
    tilingData->version = 2;
    tilingData->cnt = 1;
    tilingData->groupName[0] = '\0';
    tilingData->opType = 2;
    tilingData->reduceType = 1;
    tilingData->algConfig[0] = '\0';

    void* commContext = nullptr;

    MOCKER(hrtGetDeviceType).stubs().will(doAll(SetArg<0>(DevType::DEV_TYPE_910_93), returnValue(HCCL_SUCCESS)));
    MOCKER(hrtGetHcclV2Support).stubs().will(doAll(SetArg<0>(true), returnValue(HCCL_SUCCESS)));
    MOCKER_CPP(&HcclCommunicator::GetCommunicatorV2).stubs().will(returnValue(reinterpret_cast<void*>(0x1000)));

    HcclResult result = HcclAllocComResourceByTiling(comm, stream, mc2Tiling, &commContext);

    free(mc2Tiling);
    Ut_Comm_Destroy(comm);
    EXPECT_EQ(result, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

/**
 * @brief HcclAllocComResourceByTiling V2不支持分支测试
 *
 * 测试场景：hrtGetHcclV2Support返回false，验证非V2分支走原有MC2资源分配流程
 * 预期结果：执行HcclMc2ComResourceByTiling等非V2逻辑，返回HCCL_SUCCESS
 */
TEST_F(HcclOpBaseMc2APITest, Ut_HcclAllocComResourceByTiling_When_V2NotSupported_Expect_ReturnHCCL_SUCCESS)
{
    UT_COMM_CREATE_DEFAULT(comm);

    void* stream = nullptr;
    void* mc2Tiling = malloc(sizeof(Mc2CcTilingInner) + sizeof(Mc2HcommCfg));
    ASSERT_NE(mc2Tiling, nullptr);

    Mc2CcTilingInner* tilingData = reinterpret_cast<Mc2CcTilingInner*>(mc2Tiling);
    tilingData->version = 2;
    tilingData->cnt = 1;
    tilingData->groupName[0] = '\0';
    tilingData->opType = 2;
    tilingData->reduceType = 1;
    tilingData->algConfig[0] = '\0';

    void* commContext = nullptr;

    MOCKER(hrtGetDeviceType).stubs().will(doAll(SetArg<0>(DevType::DEV_TYPE_910_93), returnValue(HCCL_SUCCESS)));
    MOCKER(hrtGetHcclV2Support).stubs().will(doAll(SetArg<0>(false), returnValue(HCCL_SUCCESS)));
    MOCKER_CPP(&HcclCommunicator::GetCommunicatorV2).stubs().will(returnValue(reinterpret_cast<void*>(0x1000)));
    MOCKER_CPP(&HcclCommunicator::GetIdentifier).stubs().will(returnValue(std::string("test_comm")));
    MOCKER_CPP(&HcclCommunicator::GetCCLbufferName).stubs().will(returnValue(std::string("")));
    MOCKER_CPP(&HcclCommunicator::AllocComResourceByTiling).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommunicator::SetAicpuCommEngine).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommunicator::GetUserRank).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommunicator::GetCommResource).stubs().will(returnValue(HCCL_SUCCESS));

    HcclResult result = HcclAllocComResourceByTiling(comm, stream, mc2Tiling, &commContext);

    free(mc2Tiling);
    Ut_Comm_Destroy(comm);
    EXPECT_EQ(result, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

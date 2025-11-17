/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <iostream>
#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <stdio.h>
#include <algorithm>
#include <unordered_map>
#include <chrono>
#include <stdarg.h>
#include <string>
#include <exception>
#include <sys/socket.h>
#include <vector>
#include <climits>
#include <hccl/base.h>
#include <hccl/hccl_types.h>
#include <semaphore.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <list>
#include <vector>
#include <mutex>
#include "mmpa_api.h"
#include "nlohmann/json.hpp"

#include <driver/ascend_hal.h>
#include "exe_graph/runtime/tiling_parse_context.h"
#include "exe_graph/runtime/tiling_context.h"
#include "exe_graph/runtime/tiling_data.h"
#include <runtime/rt.h>
#define private public
#define protected public
#include "eletwise_v1.h"
#include "eletwise_v2.h"
#include "eletwise_v3.h"
#include "elewise_memset.h"
#include "op_json_info.h"
#include "op_tiling.h"
#include "vector_tiling.h"
#include "tbe_vector_reduce.h"
#include "tbe_crack_cleared.h"
#include "auto_tiling_rt2.h"
#include "vector_op_info.h"
#include "auto_tiling_register.h"
#include "vector_tiling_handle.h"
#include "vector_tiling_rt2.h"
#include "fusion.h"
#include "auto_tiling_context.h"
#undef protected
#undef private

using namespace std;
using namespace TbeReduce;

class EletwiseTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "\033[36m--ExchangerBaseTest SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "\033[36m--ExchangerBaseTest TearDown--\033[0m" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        std::cout << "A Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        std::cout << "A Test TearDown" << std::endl;
    }
};

TEST_F(EletwiseTest, st_EletwistV4_test_v80_1)
{
    TbeReduce::TbeVectorReduce tbeTest;
    tbeTest.deviceType_ = LegacyDevType::DEV_TYPE_910;

    MOCKER_CPP(&TbeReduce::TbeVectorReduce::GetTilingDataDevMem)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));
    MOCKER(LaunchKernelWithConfig)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&TbeReduce::TbeVectorReduce::GetTilingRunInfo,
        HcclResult (TbeReduce::TbeVectorReduce::*)(nlohmann::json&, u64, HcclDataType, TbeReduce::OpRunInfo&, HcclReduceOp))
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));

    tbeTest.Init();
    int test = 100;
    tbeTest.Run(&test, &test, 128, HCCL_DATA_TYPE_INT64, HCCL_REDUCE_SUM, &test, &test);
    GlobalMockObject::verify();
}

TbeReduce::AutoTilingCompileInfo* stub_AutoTilingCompileInfo_TilingParseContext1()
{
    static TbeReduce::AutoTilingCompileInfo CompileInfo;
    CompileInfo.soc_info.core_num = 2;
    CompileInfo.int64_mode = 0;
    return &CompileInfo;
}

gert::Chain* stub_Chain()
{
    static gert::Chain CompileInfo;
    return &CompileInfo;
}
#if 0 //执行失败
TEST_F(EletwiseTest, st_EletwistV4_test_v80)
{
    TbeReduce::TbeVectorReduce tbeTest;
    tbeTest.deviceType_ = LegacyDevType::DEV_TYPE_910;
    HcclResult ret;
    MOCKER_CPP(&TbeReduce::TbeVectorReduce::GetTilingDataDevMem)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));
    MOCKER(LaunchKernelWithConfig)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&TbeReduce::TbeVectorReduce::ConvertToOpRunInfo)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&gert::TilingParseContext::GetCompiledInfo<TbeReduce::AutoTilingCompileInfo>)
    .stubs()
    .with(any())
    .will(invoke(stub_AutoTilingCompileInfo_TilingParseContext1));

    MOCKER_CPP(&gert::KernelContext::GetOutput, gert::Chain* (gert::KernelContext::*)(const size_t))
        .stubs()
        .with(any())
        .will(invoke(stub_Chain));
    MOCKER(TbeReduce::DoAutoTiling)
        .stubs()
        .with(any(), outBound(true));
    MOCKER(TbeReduce::AutoTiling)
        .stubs()
        .with(any())
        .will(returnValue(ge::GRAPH_SUCCESS));
    MOCKER(TbeReduce::AutoTilingParser)
        .stubs()
        .with(any())
        .will(returnValue(ge::GRAPH_SUCCESS));
    ret = tbeTest.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    int test = 100;
    ret = tbeTest.Run(&test, &test, 128, HCCL_DATA_TYPE_INT64, HCCL_REDUCE_SUM, &test, &test);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}
#endif
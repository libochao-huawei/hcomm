#include <iostream>
#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <stdio.h>
#include <vector>
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
#include "stream_pub.h"
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
#include "tbe_unsorted_segment_sum_aicore.h"
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

#if 1
TEST_F(EletwiseTest, destructor_D1)
{
    std::vector<int64_t> fusedShapeX;
    fusedShapeX.push_back(0);
    fusedShapeX.push_back(1);
    fusedShapeX.push_back(2);
    fusedShapeX.push_back(3);
    fusedShapeX.push_back(4);
    fusedShapeX.push_back(5);
    std::vector<int64_t> fusedShapeY;
    fusedShapeY.push_back(0);
    fusedShapeY.push_back(1);
    fusedShapeY.push_back(2);
    fusedShapeY.push_back(3);
    fusedShapeY.push_back(4);
    fusedShapeY.push_back(5);
    std::vector<int64_t> inputShapeX;
    inputShapeX.push_back(1);
    inputShapeX.push_back(1);
    inputShapeX.push_back(2);
    inputShapeX.push_back(3);
    inputShapeX.push_back(4);
    inputShapeX.push_back(5);
    std::vector<int64_t> inputShapeY;
    inputShapeY.push_back(0);
    inputShapeY.push_back(2);
    inputShapeY.push_back(2);
    inputShapeY.push_back(3);
    inputShapeY.push_back(6);
    inputShapeY.push_back(5);
    bool state = 1;
    struct TeOpTensor {
        std::vector<int64_t> shape;
        std::vector<int64_t> oriShape;
        std::string format;
        std::string oriFormat;
        std::string dtype;
        std::map<std::string, std::string> attrs;
    };
    enum TensorArgType {
        TA_NONE,
        TA_SINGLE,
        TA_LIST,
    };

    struct TeOpTensorArg {
        TensorArgType argType;
        std::vector<TeOpTensor> tensor;
    };
    using ByteBuffer = std::stringstream;
    struct OpRunInfo {
        uint32_t block_dim;
        std::vector<int64_t> workspaces;
        ByteBuffer tiling_data;
        bool clear_atomic;
    };
    using TeOpAttrArgs = std::vector<std::string>;
    struct TeOpParas {
        std::vector<TeOpTensorArg> inputs;
        std::vector<TeOpTensorArg> outputs;
        TeOpAttrArgs attrs;
    };
    std::string type = "dynamic_add_float16_v51";
    TbeReduce::TeOpTensor inputTensor;
    inputTensor.dtype = "float16";
    inputTensor.shape = {1, 128};
    inputTensor.oriShape = {1, 128};
    inputTensor.format = "ND";
    inputTensor.oriFormat = "ND";
    TbeReduce::TeOpTensorArg tilingArg;
    tilingArg.tensor.push_back(inputTensor);
    tilingArg.argType =TbeReduce::TensorArgType::TA_SINGLE;
    TbeReduce::TeOpParas paras;
    paras.inputs.push_back(tilingArg);
    paras.inputs.push_back(tilingArg);
    paras.outputs.push_back(tilingArg);
    TbeReduce::EletwiseV1 elet(type, paras, dynamic_add_float16_v51_tiling_info);
    elet.GetFusedShape(fusedShapeX,fusedShapeY,inputShapeX,inputShapeY,state);
}
#endif

#if 1
TEST_F(EletwiseTest, destructor_D2)
{
    std::vector<int64_t> fusedShapeX;
    fusedShapeX.push_back(0);
    fusedShapeX.push_back(1);
    fusedShapeX.push_back(2);
    fusedShapeX.push_back(3);
    fusedShapeX.push_back(4);
    fusedShapeX.push_back(5);
    std::vector<int64_t> fusedShapeY;
    fusedShapeY.push_back(0);
    fusedShapeY.push_back(1);
    fusedShapeY.push_back(2);
    fusedShapeY.push_back(3);
    fusedShapeY.push_back(4);
    fusedShapeY.push_back(5);

    struct TeOpTensor {
        std::vector<int64_t> shape;
        std::vector<int64_t> oriShape;
        std::string format;
        std::string oriFormat;
        std::string dtype;
        std::map<std::string, std::string> attrs;
    };

    enum TensorArgType {
        TA_NONE,
        TA_SINGLE,
        TA_LIST,
    };

    struct TeOpTensorArg {
        TensorArgType argType;
        std::vector<TeOpTensor> tensor;
    };

    using TeOpAttrArgs = std::vector<std::string>;
    struct TeOpParas {
        std::vector<TeOpTensorArg> inputs;
        std::vector<TeOpTensorArg> outputs;
        TeOpAttrArgs attrs;
    };

    using ByteBuffer = std::stringstream;
    struct OpRunInfo {
        uint32_t block_dim;
        std::vector<int64_t> workspaces;
        ByteBuffer tiling_data;
        bool clear_atomic;
    };

    std::string type = "dynamic_add_float16_v51";
    TbeReduce::TeOpTensor inputTensor;
    inputTensor.dtype = "float16";
    inputTensor.shape = {1, 128};
    inputTensor.oriShape = {1, 128};
    inputTensor.format = "ND";
    inputTensor.oriFormat = "ND";
    TbeReduce::TeOpTensorArg tilingArg;
    tilingArg.tensor.push_back(inputTensor);
    tilingArg.argType = TbeReduce::TensorArgType::TA_SINGLE;
    TbeReduce::TeOpParas paras;
    paras.inputs.push_back(tilingArg);
    paras.inputs.push_back(tilingArg);
    paras.outputs.push_back(tilingArg);
    TbeReduce::EletwiseV1 elet(type, paras, dynamic_add_float16_v51_tiling_info);
    bool ret = elet.RefineShapesForBroadcast(fusedShapeX, fusedShapeY);
}
#endif

TEST_F(EletwiseTest, NewEletwist_test)
{
    std::string type = "add_Ascend910_int8";
    TbeReduce::TeOpTensor inputTensor;
    inputTensor.dtype = "float32";
    inputTensor.shape = {1, 128};
    inputTensor.oriShape = {1, 128};
    inputTensor.format = "ND";
    inputTensor.oriFormat = "ND";
    TbeReduce::TeOpTensorArg tilingArg;
    tilingArg.tensor.push_back(inputTensor);
    tilingArg.argType = TbeReduce::TensorArgType::TA_SINGLE;
    TbeReduce::TeOpParas paras;
    paras.inputs.push_back(tilingArg);
    paras.inputs.push_back(tilingArg);
    paras.outputs.push_back(tilingArg);
    TbeReduce::OpRunInfo runInfo;
    nlohmann::json dynamic_add_int8_v80_tiling_info = {
        {"_fusion_index", {{0, 1}}},
        {"_pattern", "Broadcast"},
        {"_outs_uint1", false},
        {"_flag_info", {false, true, true, true, true, false}}, 
        {"_base_info", {{"100", {261696, 4, 4, 32}},
                        {"320", {253888, 4, 4, 32}},
                        {"230", {253888, 4, 4, 32}}}},
        {"_vars", {{"210000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
                {"210010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
                {"232000000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
                {"223000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}}}},
        {"_normal_vars", {{"210000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
                        {"210010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
                        {"232000000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
                        {"223000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}}}},
        {"_attr_vars", {{"210000000", {}}, {"210010000", {}}, {"232000000", {}}, {"223000000", {}}}},
        {"_custom_vars", {{"210000000", {}}, {"210010000", {}}, {"232000000", {}}, {"223000000", {}}}},
        {"_elewise_vars", {{"210000000", {10000, 20000, 30000}},
                        {"210010000", {10000, 20000, 30000}},
                        {"232000000", {10001, 20000, 30000}},
                        {"223000000", {10000, 20000, 30000}}}}
    };

    TbeReduce::EletwiseTilingV2(type, paras, dynamic_add_int8_v80_tiling_info, runInfo);
}

TEST_F(EletwiseTest, EletwistV3_test_need_multi_core1)
{
    TbeReduce::TbeVectorReduce tbeTest;
    tbeTest.deviceType_ = LegacyDevType::DEV_TYPE_910;
    MOCKER_CPP(&TbeReduce::TbeVectorReduce::LoadOpBinary)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&TbeReduce::TbeVectorReduce::GetTilingDataDevMem)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));
    MOCKER(LaunchKernelWithConfig)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));
    MOCKER(BinaryLoadFromData)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));
    MOCKER(GetDeviceType)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));
    tbeTest.Init();
    int test = 100;

    tbeTest.Run(&test, &test, 4096, HCCL_DATA_TYPE_FP32, HCCL_REDUCE_PROD, &test, &test);
    GlobalMockObject::verify();
}

TEST_F(EletwiseTest, EletwistV3_test_need_multi_core2)
{
    TbeReduce::TbeVectorReduce tbeTest;
    tbeTest.deviceType_ = LegacyDevType::DEV_TYPE_910;
    MOCKER_CPP(&TbeReduce::TbeVectorReduce::LoadOpBinary)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&TbeReduce::TbeVectorReduce::GetTilingDataDevMem)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));
    MOCKER(LaunchKernelWithConfig)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));
    MOCKER(BinaryLoadFromData)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));
    MOCKER(GetDeviceType)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));
    tbeTest.Init();
    int test = 100;
    tbeTest.Run(&test, &test, 4096 * 4096, HCCL_DATA_TYPE_FP32, HCCL_REDUCE_PROD, &test, &test);
    GlobalMockObject::verify();
}


TEST_F(EletwiseTest, EletwistV3_test)
{
    TbeReduce::TbeVectorReduce tbeTest;
    tbeTest.deviceType_ = LegacyDevType::DEV_TYPE_910;
    MOCKER_CPP(&TbeReduce::TbeVectorReduce::LoadOpBinary)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&TbeReduce::TbeVectorReduce::GetTilingDataDevMem)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));
    MOCKER(LaunchKernelWithConfig)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));
    MOCKER(BinaryLoadFromData)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));
    MOCKER(GetDeviceType)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));
    tbeTest.Init();
    int test = 100;
    tbeTest.Run(&test, &test, 128, HCCL_DATA_TYPE_FP32, HCCL_REDUCE_PROD, &test, &test);
    GlobalMockObject::verify();
}

s32 fake_rtGetSocVersionV81TbeVectorReduceUTest(char *chipVer, const u32 maxLen)
{
    memcpy_s(chipVer, sizeof("Ascend910B1"), "Ascend910B1", sizeof("Ascend910B1"));
    return DRV_ERROR_NONE;
}

TEST_F(EletwiseTest, EletwistV3_test_need_multi_core1_v81)
{
    TbeReduce::TbeVectorReduce tbeTest;
    tbeTest.deviceType_ = LegacyDevType::DEV_TYPE_910B;
    MOCKER_CPP(&TbeReduce::TbeVectorReduce::LoadOpBinary)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&TbeReduce::TbeVectorReduce::GetTilingDataDevMem)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));
    MOCKER(LaunchKernelWithConfig)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));
    MOCKER(BinaryLoadFromData)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));
    MOCKER(GetDeviceType)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));
    MOCKER(rtGetSocVersion)
        .stubs()
        .will(invoke(fake_rtGetSocVersionV81TbeVectorReduceUTest));
    tbeTest.Init();
    int test = 100;
    tbeTest.Run(&test, &test, 4096, HCCL_DATA_TYPE_FP32, HCCL_REDUCE_PROD, &test, &test);
    GlobalMockObject::verify();
}

TEST_F(EletwiseTest, EletwistV3_test_need_multi_core2_v81)
{
    TbeReduce::TbeVectorReduce tbeTest;
    tbeTest.deviceType_ = LegacyDevType::DEV_TYPE_910B;
    MOCKER_CPP(&TbeReduce::TbeVectorReduce::LoadOpBinary)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&TbeReduce::TbeVectorReduce::GetTilingDataDevMem)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));
    MOCKER(LaunchKernelWithConfig)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));
    MOCKER(BinaryLoadFromData)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));
    MOCKER(GetDeviceType)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));
    MOCKER(rtGetSocVersion)
        .stubs()
        .will(invoke(fake_rtGetSocVersionV81TbeVectorReduceUTest));
    tbeTest.Init();
    int test = 100;
    tbeTest.Run(&test, &test, 4096 * 4096, HCCL_DATA_TYPE_FP32, HCCL_REDUCE_PROD, &test, &test);
    GlobalMockObject::verify();
}


TEST_F(EletwiseTest, EletwistV3_test_v81)
{
    TbeReduce::TbeVectorReduce tbeTest;
    tbeTest.deviceType_ = LegacyDevType::DEV_TYPE_910B;
    MOCKER_CPP(&TbeReduce::TbeVectorReduce::LoadOpBinary)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&TbeReduce::TbeVectorReduce::GetTilingDataDevMem)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));
    MOCKER(LaunchKernelWithConfig)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));
    MOCKER(BinaryLoadFromData)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));
    MOCKER(GetDeviceType)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));
    MOCKER(rtGetSocVersion)
        .stubs()
        .will(invoke(fake_rtGetSocVersionV81TbeVectorReduceUTest));
    tbeTest.Init();
    int test = 100;
    tbeTest.Run(&test, &test, 128, HCCL_DATA_TYPE_FP32, HCCL_REDUCE_PROD, &test, &test);
    GlobalMockObject::verify();
}

TEST_F(EletwiseTest, EletwistV4_test_v80_1)
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


TEST_F(EletwiseTest, EletwistV4_test_v80)
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

    MOCKER(TbeReduce::AutoTilingParser)
        .stubs()
        .with(any())
        .will(returnValue(ge::GRAPH_SUCCESS));

    // MOCKER_CPP(&gert::TilingContext::GetCompileInfo<TbeReduce::AutoTilingCompileInfo>)
    // .stubs()
    // .with(any())
    // .will(invoke(stub_AutoTilingCompileInfo_TilingParseContext));

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
    ret = tbeTest.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    int test = 100;
    ret = tbeTest.Run(&test, &test, 128, HCCL_DATA_TYPE_INT64, HCCL_REDUCE_SUM, &test, &test);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}
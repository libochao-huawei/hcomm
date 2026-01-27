#include "fwk/fwk.h"
using namespace Hccl;

TEST(ST_ALLREDUCE, st_allreduce_910A_1_server_8_device_fp32_sum_dataCount_1)
{
    Situation situation;
    situation.SetClusterType(DevType::DEV_TYPE_910A, FakeClusterType::CLUSTER_1_SERVER_8_DEV);
    situation.SetOpType(OpType::ALLREDUCE).SetDataType(DataType::FP32).SetCount(1).SetReduceOp(ReduceOp::SUM);

    HcclStTestCase testCase(situation, "st_allreduce_910A_1_server_8_device_fp32_sum_dataCount_2");

    testCase.Start();

    EXPECT_TRUE(testCase.Verify());
}

TEST(ST_ALLREDUCE, st_allreduce_910A_1_server_2_device_fp32_sum_dataCount_4)
{
    Situation situation;
    situation.SetClusterType(DevType::DEV_TYPE_910A, FakeClusterType::CLUSTER_1_SERVER_2_DEV);
    situation.SetOpType(OpType::ALLREDUCE).SetDataType(DataType::FP32).SetCount(4).SetReduceOp(ReduceOp::SUM);

    HcclStTestCase testCase(situation, "st_allreduce_910A_1_server_2_device_fp32_sum_dataCount_4");

    testCase.Start();

    EXPECT_TRUE(testCase.Verify());
}
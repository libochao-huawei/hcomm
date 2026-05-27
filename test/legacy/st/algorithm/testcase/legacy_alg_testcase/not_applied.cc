/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

TEST_F(AllReduceAICPUMesh1dNHRTest, allreduce_aicpu_case_test_Asymmetric_2pod_2mul1_2mul2)
{
    RankTable_For_LLT gen;
    TopoMeta topoMeta{{{0, 1}, {0, 1, 2, 3}}};

    setenv("HCCL_IODIE_NUM", "2", 1);

    CheckerOpParam checkerOpParam;
    checkerOpParam.opType = CheckerOpType::ALLREDUCE;
    checkerOpParam.tag = "AllReduce";
    checkerOpParam.opMode = CheckerOpMode::OPBASE;
    checkerOpParam.reduceType = CheckerReduceOp::REDUCE_SUM;
    checkerOpParam.DataDes.count = 1;
    checkerOpParam.DataDes.dataType = CheckerDataType::DATA_TYPE_INT8;
    checkerOpParam.devtype = CheckerDevType::DEV_TYPE_950;
    checkerOpParam.algName = "InsAllReduceParallelMesh1DNHR";

    Checker checker;
    HcclResult ret;
    ret = checker.CheckA5Aicpu(checkerOpParam, topoMeta);
    EXPECT_EQ(ret, HcclResult::HCCL_SUCCESS);
}

TEST_F(AllReduceAICPUMesh1dNHRTest, allreduce_aicpu_case_test_Asymmetric_2pod_2mul2_3mul2)
{
    RankTable_For_LLT gen;
    TopoMeta topoMeta{{{0, 1, 8, 9}, {0, 1, 2, 8, 9, 10}}};

    setenv("HCCL_IODIE_NUM", "2", 1);

    CheckerOpParam checkerOpParam;
    checkerOpParam.opType = CheckerOpType::ALLREDUCE;
    checkerOpParam.tag = "AllReduce";
    checkerOpParam.opMode = CheckerOpMode::OFFLOAD;
    checkerOpParam.reduceType = CheckerReduceOp::REDUCE_MIN;
    checkerOpParam.DataDes.count = 200;
    checkerOpParam.DataDes.dataType = CheckerDataType::DATA_TYPE_FP16;
    checkerOpParam.devtype = CheckerDevType::DEV_TYPE_950;
    checkerOpParam.algName = "InsAllReduceParallelMesh1DNHR";

    Checker checker;
    HcclResult ret;
    ret = checker.CheckA5Aicpu(checkerOpParam, topoMeta);
    EXPECT_EQ(ret, HcclResult::HCCL_SUCCESS);
}

TEST_F(AllReduceAICPUMesh1dNHRTest, allreduce_aicpu_case_test_2_pod_Asymmetric_3mul3_3mul2)
{
    RankTable_For_LLT gen;
    TopoMeta topoMeta{{{0, 1, 2, 8, 9, 10, 16, 17, 18}, {2, 3, 4, 10, 11, 12}}};

    setenv("HCCL_IODIE_NUM", "2", 1);

    CheckerOpParam checkerOpParam;
    checkerOpParam.opType = CheckerOpType::ALLREDUCE;
    checkerOpParam.tag = "AllReduce";
    checkerOpParam.opMode = CheckerOpMode::OPBASE;
    checkerOpParam.reduceType = CheckerReduceOp::REDUCE_MAX;
    checkerOpParam.DataDes.count = 1024;
    checkerOpParam.DataDes.dataType = CheckerDataType::DATA_TYPE_FP32;
    checkerOpParam.devtype = CheckerDevType::DEV_TYPE_950;
    checkerOpParam.algName = "InsAllReduceParallelMesh1DNHR";

    Checker checker;
    HcclResult ret;
    ret = checker.CheckA5Aicpu(checkerOpParam, topoMeta);
    EXPECT_EQ(ret, HcclResult::HCCL_SUCCESS);
}

TEST_F(AllReduceAICPUMesh1dNHRTest, AllGatherParallel_asymmetric_opbase_6n6n9)
{
    RankTable_For_LLT gen;
    TopoMeta topoMeta{{{0, 1, 2, 8, 9, 10}, {3, 4, 5, 11, 12, 13}, {4, 5, 6, 12, 13, 14, 20, 21, 22}}};

    setenv("HCCL_IODIE_NUM", "2", 1);

    CheckerOpParam checkerOpParam;
    checkerOpParam.opType = CheckerOpType::ALLREDUCE;
    checkerOpParam.tag = "AllReduce";
    checkerOpParam.opMode = CheckerOpMode::OFFLOAD;
    checkerOpParam.reduceType = CheckerReduceOp::REDUCE_SUM;
    checkerOpParam.DataDes.count = 1024;
    checkerOpParam.DataDes.dataType = CheckerDataType::DATA_TYPE_BFP16;
    checkerOpParam.devtype = CheckerDevType::DEV_TYPE_950;
    checkerOpParam.algName = "InsAllReduceParallelMesh1DNHR";

    Checker checker;
    HcclResult ret;
    ret = checker.CheckA5Aicpu(checkerOpParam, topoMeta);
    EXPECT_EQ(ret, HcclResult::HCCL_SUCCESS);
}

TEST_F(AllReduceAICPUMesh2dNHRTest, allreduce_aicpu_case_test_2_pod_Asymmetric_2mul2_2mul4)
{
    RankTable_For_LLT gen;
    TopoMeta topoMeta{{{0, 1, 8, 9}, {2, 3, 10, 11, 18, 19, 26, 27}}};

    setenv("HCCL_IODIE_NUM", "2", 1);

    CheckerOpParam checkerOpParam;
    checkerOpParam.opType = CheckerOpType::ALLREDUCE;
    checkerOpParam.tag = "AllReduce";
    checkerOpParam.opMode = CheckerOpMode::OPBASE;
    checkerOpParam.reduceType = CheckerReduceOp::REDUCE_MAX;
    checkerOpParam.DataDes.count = 100;
    checkerOpParam.DataDes.dataType = CheckerDataType::DATA_TYPE_INT32;
    checkerOpParam.devtype = CheckerDevType::DEV_TYPE_950;
    checkerOpParam.algName = "InsAllReduceParallelMesh2DNHR";

    Checker checker;
    HcclResult ret;
    ret = checker.CheckA5Aicpu(checkerOpParam, topoMeta);
    EXPECT_EQ(ret, HcclResult::HCCL_SUCCESS);
}

TEST_F(AllReduceAICPUMesh2dNHRTest, allreduce_aicpu_case_test_3_pod_Asymmetric_2mul2_2mul4_2mul6)
{
    RankTable_For_LLT gen;
    TopoMeta topoMeta{{{0, 1, 8, 9}, {2, 3, 10, 11, 18, 19, 26, 27}, {4, 5, 12, 13, 20, 21, 28, 29, 36, 37, 44, 45}}};

    setenv("HCCL_IODIE_NUM", "2", 1);

    CheckerOpParam checkerOpParam;
    checkerOpParam.opType = CheckerOpType::ALLREDUCE;
    checkerOpParam.tag = "AllReduce";
    checkerOpParam.opMode = CheckerOpMode::OPBASE;
    checkerOpParam.reduceType = CheckerReduceOp::REDUCE_MIN;
    checkerOpParam.DataDes.count = 1000;
    checkerOpParam.DataDes.dataType = CheckerDataType::DATA_TYPE_INT16;
    checkerOpParam.devtype = CheckerDevType::DEV_TYPE_950;
    checkerOpParam.algName = "InsAllReduceParallelMesh2DNHR";

    Checker checker;
    HcclResult ret;
    ret = checker.CheckA5Aicpu(checkerOpParam, topoMeta);
    EXPECT_EQ(ret, HcclResult::HCCL_SUCCESS);
}

TEST_F(AllReduceAICPUMesh2dNHRTest, allreduce_aicpu_case_test_3_pod_Asymmetric_2mul2_2mul4_2mul4)
{
    RankTable_For_LLT gen;
    TopoMeta topoMeta{{{0, 1, 8, 9}, {2, 3, 10, 11, 18, 19, 26, 27}, {4, 5, 12, 13, 20, 21, 28, 29}}};

    setenv("HCCL_IODIE_NUM", "2", 1);

    CheckerOpParam checkerOpParam;
    checkerOpParam.opType = CheckerOpType::ALLREDUCE;
    checkerOpParam.tag = "AllReduce";
    checkerOpParam.opMode = CheckerOpMode::OFFLOAD;
    checkerOpParam.reduceType = CheckerReduceOp::REDUCE_SUM;
    checkerOpParam.DataDes.count = 1024;
    checkerOpParam.DataDes.dataType = CheckerDataType::DATA_TYPE_BFP16;
    checkerOpParam.devtype = CheckerDevType::DEV_TYPE_950;
    checkerOpParam.algName = "InsAllReduceParallelMesh2DNHR";

    Checker checker;
    HcclResult ret;
    ret = checker.CheckA5Aicpu(checkerOpParam, topoMeta);
    EXPECT_EQ(ret, HcclResult::HCCL_SUCCESS);
}

TEST_F(ReduceScatterAICPUMesh2dNHRTest, reduceScatter_aicpu_case_test_2_pod_ParallelMesh2DNHR)
{
    TopoMeta topoMeta{{{0, 1, 8, 9, 24, 25, 32, 33}, {3, 5, 19, 21}}};
    setenv("HCCL_IODIE_NUM", "2", 1);

    CheckerOpParam checkerOpParam;
    checkerOpParam.opType = CheckerOpType::REDUCE_SCATTER;
    checkerOpParam.tag = "ReduceScatter";
    checkerOpParam.opMode = CheckerOpMode::OPBASE;
    checkerOpParam.reduceType = CheckerReduceOp::REDUCE_SUM;
    checkerOpParam.devtype = CheckerDevType::DEV_TYPE_950;
    checkerOpParam.DataDes.count = 400;
    checkerOpParam.DataDes.dataType = CheckerDataType::DATA_TYPE_INT32;
    checkerOpParam.algName = "InsReduceScatterParallelMesh2DNHR";

    Checker checker;
    HcclResult ret;
    ret = checker.CheckA5Aicpu(checkerOpParam, topoMeta);
    EXPECT_EQ(ret, HcclResult::HCCL_SUCCESS);
}

TEST_F(ReduceScatterAICPUMesh2dNHRTest, reduceScatter_aicpu_case_test_2_pod_Asymmetric_2mul2_2mul4)
{
    RankTable_For_LLT gen;
    TopoMeta topoMeta{{{0, 1, 8, 9}, {2, 3, 10, 11, 18, 19, 26, 27}}};

    setenv("HCCL_IODIE_NUM", "2", 1);

    CheckerOpParam checkerOpParam;
    checkerOpParam.opType = CheckerOpType::REDUCE_SCATTER;
    checkerOpParam.tag = "ReduceScatter";
    checkerOpParam.opMode = CheckerOpMode::OPBASE;
    checkerOpParam.reduceType = CheckerReduceOp::REDUCE_MAX;
    checkerOpParam.DataDes.count = 100;
    checkerOpParam.DataDes.dataType = CheckerDataType::DATA_TYPE_INT32;
    checkerOpParam.devtype = CheckerDevType::DEV_TYPE_950;
    checkerOpParam.algName = "InsReduceScatterParallelMesh2DNHR";

    Checker checker;
    HcclResult ret;
    ret = checker.CheckA5Aicpu(checkerOpParam, topoMeta);
    EXPECT_EQ(ret, HcclResult::HCCL_SUCCESS);
}

TEST_F(ReduceScatterAICPUMesh2dNHRTest, reduceScatter_aicpu_case_test_3_pod_Asymmetric_2mul2_2mul4_2mul6)
{
    RankTable_For_LLT gen;
    TopoMeta topoMeta{{{0, 1, 8, 9}, {2, 3, 10, 11, 18, 19, 26, 27}, {4, 5, 12, 13, 20, 21, 28, 29, 36, 37, 44, 45}}};

    setenv("HCCL_IODIE_NUM", "2", 1);

    CheckerOpParam checkerOpParam;
    checkerOpParam.opType = CheckerOpType::REDUCE_SCATTER;
    checkerOpParam.tag = "ReduceScatter";
    checkerOpParam.opMode = CheckerOpMode::OPBASE;
    checkerOpParam.reduceType = CheckerReduceOp::REDUCE_MIN;
    checkerOpParam.DataDes.count = 1000;
    checkerOpParam.DataDes.dataType = CheckerDataType::DATA_TYPE_INT16;
    checkerOpParam.devtype = CheckerDevType::DEV_TYPE_950;
    checkerOpParam.algName = "InsReduceScatterParallelMesh2DNHR";

    Checker checker;
    HcclResult ret;
    ret = checker.CheckA5Aicpu(checkerOpParam, topoMeta);
    EXPECT_EQ(ret, HcclResult::HCCL_SUCCESS);
}

TEST_F(ReduceScatterAICPUMesh2dNHRTest, reduceScatter_aicpu_case_test_3_pod_Asymmetric_2mul2_2mul4_2mul4)
{
    RankTable_For_LLT gen;
    TopoMeta topoMeta{{{0, 1, 8, 9}, {2, 3, 10, 11, 18, 19, 26, 27}, {4, 5, 12, 13, 20, 21, 28, 29}}};

    setenv("HCCL_IODIE_NUM", "2", 1);

    CheckerOpParam checkerOpParam;
    checkerOpParam.opType = CheckerOpType::REDUCE_SCATTER;
    checkerOpParam.tag = "ReduceScatter";
    checkerOpParam.opMode = CheckerOpMode::OFFLOAD;
    checkerOpParam.reduceType = CheckerReduceOp::REDUCE_SUM;
    checkerOpParam.DataDes.count = 1024;
    checkerOpParam.DataDes.dataType = CheckerDataType::DATA_TYPE_FP32;
    checkerOpParam.devtype = CheckerDevType::DEV_TYPE_950;
    checkerOpParam.algName = "InsReduceScatterParallelMesh2DNHR";

    Checker checker;
    HcclResult ret;
    ret = checker.CheckA5Aicpu(checkerOpParam, topoMeta);
    EXPECT_EQ(ret, HcclResult::HCCL_SUCCESS);
}

TEST_F(ReduceScatterAICPUMesh1dNHRTest, reduce_scatter_aicpu_case_test_Asymmetric_2pod_2mul1_2mul2)
{
    RankTable_For_LLT gen;
    TopoMeta topoMeta{{{0, 1}, {0, 1, 2, 3}}};

    setenv("HCCL_IODIE_NUM", "2", 1);

    CheckerOpParam checkerOpParam;
    checkerOpParam.opType = CheckerOpType::REDUCE_SCATTER;
    checkerOpParam.tag = "ReduceScatter";
    checkerOpParam.opMode = CheckerOpMode::OPBASE;
    checkerOpParam.reduceType = CheckerReduceOp::REDUCE_SUM;
    checkerOpParam.DataDes.count = 1;
    checkerOpParam.DataDes.dataType = CheckerDataType::DATA_TYPE_INT8;
    checkerOpParam.devtype = CheckerDevType::DEV_TYPE_950;
    checkerOpParam.algName = "InsReduceScatterParallelMesh1DNHR";

    Checker checker;
    HcclResult ret;
    ret = checker.CheckA5Aicpu(checkerOpParam, topoMeta);
    EXPECT_EQ(ret, HcclResult::HCCL_SUCCESS);
}

TEST_F(ReduceScatterAICPUMesh1dNHRTest, reduce_scatter_aicpu_case_test_Asymmetric_2pod_2mul2_3mul2)
{
    RankTable_For_LLT gen;
    TopoMeta topoMeta{{{0, 1, 8, 9}, {0, 1, 2, 8, 9, 10}}};

    setenv("HCCL_IODIE_NUM", "2", 1);

    CheckerOpParam checkerOpParam;
    checkerOpParam.opType = CheckerOpType::REDUCE_SCATTER;
    checkerOpParam.tag = "ReduceScatter";
    checkerOpParam.opMode = CheckerOpMode::OFFLOAD;
    checkerOpParam.reduceType = CheckerReduceOp::REDUCE_MIN;
    checkerOpParam.DataDes.count = 200;
    checkerOpParam.DataDes.dataType = CheckerDataType::DATA_TYPE_FP16;
    checkerOpParam.devtype = CheckerDevType::DEV_TYPE_950;
    checkerOpParam.algName = "InsReduceScatterParallelMesh1DNHR";

    Checker checker;
    HcclResult ret;
    ret = checker.CheckA5Aicpu(checkerOpParam, topoMeta);
    EXPECT_EQ(ret, HcclResult::HCCL_SUCCESS);
}

TEST_F(ReduceScatterAICPUMesh1dNHRTest, reduce_scatter_aicpu_case_test_2_pod_Asymmetric_3mul3_3mul2)
{
    RankTable_For_LLT gen;
    TopoMeta topoMeta{{{0, 1, 2, 8, 9, 10, 16, 17, 18}, {2, 3, 4, 10, 11, 12}}};

    setenv("HCCL_IODIE_NUM", "2", 1);

    CheckerOpParam checkerOpParam;
    checkerOpParam.opType = CheckerOpType::REDUCE_SCATTER;
    checkerOpParam.tag = "ReduceScatter";
    checkerOpParam.opMode = CheckerOpMode::OPBASE;
    checkerOpParam.reduceType = CheckerReduceOp::REDUCE_MAX;
    checkerOpParam.DataDes.count = 1024;
    checkerOpParam.DataDes.dataType = CheckerDataType::DATA_TYPE_FP32;
    checkerOpParam.devtype = CheckerDevType::DEV_TYPE_950;
    checkerOpParam.algName = "InsReduceScatterParallelMesh1DNHR";

    Checker checker;
    HcclResult ret;
    ret = checker.CheckA5Aicpu(checkerOpParam, topoMeta);
    EXPECT_EQ(ret, HcclResult::HCCL_SUCCESS);
}

TEST_F(ReduceScatterAICPUMesh1dNHRTest, AllGatherParallel_asymmetric_opbase_6n6n9)
{
    RankTable_For_LLT gen;
    TopoMeta topoMeta{{{0, 1, 2, 8, 9, 10}, {3, 4, 5, 11, 12, 13}, {4, 5, 6, 12, 13, 14, 20, 21, 22}}};

    setenv("HCCL_IODIE_NUM", "2", 1);

    CheckerOpParam checkerOpParam;
    checkerOpParam.opType = CheckerOpType::REDUCE_SCATTER;
    checkerOpParam.tag = "ReduceScatter";
    checkerOpParam.opMode = CheckerOpMode::OFFLOAD;
    checkerOpParam.reduceType = CheckerReduceOp::REDUCE_SUM;
    checkerOpParam.DataDes.count = 1024;
    checkerOpParam.DataDes.dataType = CheckerDataType::DATA_TYPE_BFP16;
    checkerOpParam.devtype = CheckerDevType::DEV_TYPE_950;
    checkerOpParam.algName = "InsReduceScatterParallelMesh1DNHR";

    Checker checker;
    HcclResult ret;
    ret = checker.CheckA5Aicpu(checkerOpParam, topoMeta);
    EXPECT_EQ(ret, HcclResult::HCCL_SUCCESS);
}

TEST_F(AllGatherTest, AllGatherParallel_asymmetric_offload_2n4)
{
    RankTable_For_LLT gen;
    TopoMeta topoMeta{{{0, 1}, {0, 1, 2, 3}}};

    setenv("HCCL_IODIE_NUM", "2", 1);

    CheckerOpParam checkerOpParam;
    checkerOpParam.opType = CheckerOpType::ALLGATHER;
    checkerOpParam.tag = "AllGather";
    checkerOpParam.opMode = CheckerOpMode::OFFLOAD;
    checkerOpParam.DataDes.count = 10;
    checkerOpParam.DataDes.dataType = CheckerDataType::DATA_TYPE_INT16;
    checkerOpParam.devtype = CheckerDevType::DEV_TYPE_950;
    checkerOpParam.algName = "InsAllGatherParallelMesh1DNHR";

    Checker checker;
    checker.EnableTaskPrint();
    HcclResult ret;
    ret = checker.CheckA5Aicpu(checkerOpParam, topoMeta);
    EXPECT_EQ(ret, HcclResult::HCCL_SUCCESS);
}

TEST_F(AllGatherTest, AllGatherParallel_nhr_offload_2n3)
{
    RankTable_For_LLT gen;
    TopoMeta topoMeta{{{0, 1}, {1, 2, 3}}};

    setenv("HCCL_IODIE_NUM", "2", 1);

    CheckerOpParam checkerOpParam;
    checkerOpParam.opType = CheckerOpType::ALLGATHER;
    checkerOpParam.tag = "AllGather";
    checkerOpParam.opMode = CheckerOpMode::OFFLOAD;
    checkerOpParam.DataDes.count = 10;
    checkerOpParam.DataDes.dataType = CheckerDataType::DATA_TYPE_INT64;
    checkerOpParam.devtype = CheckerDevType::DEV_TYPE_950;
    checkerOpParam.algName = "InsAllGatherNHR";

    Checker checker;
    checker.EnableTaskPrint();
    HcclResult ret;
    ret = checker.CheckA5Aicpu(checkerOpParam, topoMeta);
    EXPECT_EQ(ret, HcclResult::HCCL_SUCCESS);
}

TEST_F(AllGatherTest, AllGatherParallel_nhr_offload_3pods)
{
    RankTable_For_LLT gen;
    TopoMeta topoMeta{{{0, 1}, {0, 1, 2, 3}, {5, 6, 7}}};

    setenv("HCCL_IODIE_NUM", "2", 1);

    CheckerOpParam checkerOpParam;
    checkerOpParam.opType = CheckerOpType::ALLGATHER;
    checkerOpParam.tag = "AllGather";
    checkerOpParam.opMode = CheckerOpMode::OFFLOAD;
    checkerOpParam.DataDes.count = 100;
    checkerOpParam.DataDes.dataType = CheckerDataType::DATA_TYPE_INT8;
    checkerOpParam.devtype = CheckerDevType::DEV_TYPE_950;
    checkerOpParam.algName = "InsAllGatherNHR";

    Checker checker;
    checker.EnableTaskPrint();
    HcclResult ret;
    ret = checker.CheckA5Aicpu(checkerOpParam, topoMeta);
    EXPECT_EQ(ret, HcclResult::HCCL_SUCCESS);
}

TEST_F(AllGatherTest, AllGatherParallel_asymmetric_4n6)
{
    RankTable_For_LLT gen;
    TopoMeta topoMeta{{{0, 1, 8, 9}, {0, 1, 2, 8, 9, 10}}};

    setenv("HCCL_IODIE_NUM", "2", 1);

    CheckerOpParam checkerOpParam;
    checkerOpParam.opType = CheckerOpType::ALLGATHER;
    checkerOpParam.tag = "AllGather";
    checkerOpParam.opMode = CheckerOpMode::OFFLOAD;
    checkerOpParam.DataDes.count = 100;
    checkerOpParam.DataDes.dataType = CheckerDataType::DATA_TYPE_FP16;
    checkerOpParam.devtype = CheckerDevType::DEV_TYPE_950;
    checkerOpParam.algName = "InsAllGatherParallelMesh1DNHR";

    Checker checker;
    checker.EnableTaskPrint();
    HcclResult ret;
    ret = checker.CheckA5Aicpu(checkerOpParam, topoMeta);
    EXPECT_EQ(ret, HcclResult::HCCL_SUCCESS);
}

TEST_F(AllGatherTest, AllGatherParallel_asymmetric_offload_6n6n9)
{
    RankTable_For_LLT gen;
    TopoMeta topoMeta {{{0,1,2,8,9,10}, {3,4,5,11,12,13},{4,5,6,12,13,14,20,21,22}}};

    setenv("HCCL_IODIE_NUM", "2", 1);

    CheckerOpParam checkerOpParam;
    checkerOpParam.opType = CheckerOpType::ALLGATHER;
    checkerOpParam.tag = "AllGather";
    checkerOpParam.opMode = CheckerOpMode::OFFLOAD;
    checkerOpParam.DataDes.count = 1024;
    checkerOpParam.DataDes.dataType = CheckerDataType::DATA_TYPE_FP32;
    checkerOpParam.devtype = CheckerDevType::DEV_TYPE_950;
    checkerOpParam.algName = "InsAllGatherParallelMesh1DNHR";

    Checker checker;
    checker.EnableTaskPrint();
    HcclResult ret;
    ret = checker.CheckA5Aicpu(checkerOpParam, topoMeta);
    EXPECT_EQ(ret, HcclResult::HCCL_SUCCESS);
}

TEST_F(AllGatherTest, AllGatherParallel_nhr_4n5)
{
    RankTable_For_LLT gen;
    TopoMeta topoMeta{{{0, 1, 8, 9}, {0, 1, 2, 3, 4}}};

    setenv("HCCL_IODIE_NUM", "2", 1);

    CheckerOpParam checkerOpParam;
    checkerOpParam.opType = CheckerOpType::ALLGATHER;
    checkerOpParam.tag = "AllGather";
    checkerOpParam.opMode = CheckerOpMode::OFFLOAD;
    checkerOpParam.DataDes.count = 100;
    checkerOpParam.DataDes.dataType = CheckerDataType::DATA_TYPE_BFP16;
    checkerOpParam.devtype = CheckerDevType::DEV_TYPE_950;
    checkerOpParam.algName = "InsAllGatherNHR";

    Checker checker;
    checker.EnableTaskPrint();
    HcclResult ret;
    ret = checker.CheckA5Aicpu(checkerOpParam, topoMeta);
    EXPECT_EQ(ret, HcclResult::HCCL_SUCCESS);
}

TEST_F(AllGather2DTest, AllGatherParallel_opbase_4n8)
{
    RankTable_For_LLT gen;
    TopoMeta topoMeta {{{0,1,8,9}, {0,1,8,9,16,17,24,25}}};

    setenv("HCCL_IODIE_NUM", "2", 1);

    CheckerOpParam checkerOpParam;
    checkerOpParam.opType = CheckerOpType::ALLGATHER;
    checkerOpParam.tag = "AllGather";
    checkerOpParam.opMode = CheckerOpMode::OPBASE;
    checkerOpParam.DataDes.count = 100;
    checkerOpParam.DataDes.dataType = CheckerDataType::DATA_TYPE_INT32;
    checkerOpParam.devtype = CheckerDevType::DEV_TYPE_950;
    checkerOpParam.algName = "InsAllGatherParallelMesh2DNHR";

    Checker checker;
    checker.EnableTaskPrint();
    HcclResult ret;
    ret = checker.CheckA5Aicpu(checkerOpParam, topoMeta);
    EXPECT_EQ(ret, HcclResult::HCCL_SUCCESS);
}

TEST_F(AllGather2DTest, AllGatherParallel_opbase_4n8_2)
{
    RankTable_For_LLT gen;
    TopoMeta topoMeta {{{0,1,8,9}, {2,3,10,11,18,19,26,27}}};

    setenv("HCCL_IODIE_NUM", "2", 1);

    CheckerOpParam checkerOpParam;
    checkerOpParam.opType = CheckerOpType::ALLGATHER;
    checkerOpParam.tag = "AllGather";
    checkerOpParam.opMode = CheckerOpMode::OPBASE;
    checkerOpParam.DataDes.count = 1000;
    checkerOpParam.DataDes.dataType = CheckerDataType::DATA_TYPE_INT8;
    checkerOpParam.devtype = CheckerDevType::DEV_TYPE_950;
    checkerOpParam.algName = "InsAllGatherParallelMesh2DNHR";

    Checker checker;
    checker.EnableTaskPrint();
    HcclResult ret;
    ret = checker.CheckA5Aicpu(checkerOpParam, topoMeta);
    EXPECT_EQ(ret, HcclResult::HCCL_SUCCESS);
}

TEST_F(AllGather2DTest, AllGatherParallel_opbase_4n8n12)
{
    RankTable_For_LLT gen;
    TopoMeta topoMeta {{{0,1,8,9}, {2,3,10,11,18,19,26,27},{4,5,12,13,20,21,28,29,36,37,44,45}}};

    setenv("HCCL_IODIE_NUM", "2", 1);

    CheckerOpParam checkerOpParam;
    checkerOpParam.opType = CheckerOpType::ALLGATHER;
    checkerOpParam.tag = "AllGather";
    checkerOpParam.opMode = CheckerOpMode::OPBASE;
    checkerOpParam.DataDes.count = 1000;
    checkerOpParam.DataDes.dataType = CheckerDataType::DATA_TYPE_INT16;
    checkerOpParam.devtype = CheckerDevType::DEV_TYPE_950;
    checkerOpParam.algName = "InsAllGatherParallelMesh2DNHR";

    Checker checker;
    checker.EnableTaskPrint();
    HcclResult ret;
    ret = checker.CheckA5Aicpu(checkerOpParam, topoMeta);
    EXPECT_EQ(ret, HcclResult::HCCL_SUCCESS);
}

TEST_F(AllGather2DTest, AllGatherParallel_opbase_4n8n8)
{
    RankTable_For_LLT gen;
    TopoMeta topoMeta {{{0,1,8,9}, {2,3,10,11,18,19,26,27},{4,5,12,13,20,21,28,29}}};

    setenv("HCCL_IODIE_NUM", "2", 1);

    CheckerOpParam checkerOpParam;
    checkerOpParam.opType = CheckerOpType::ALLGATHER;
    checkerOpParam.tag = "AllGather";
    checkerOpParam.opMode = CheckerOpMode::OFFLOAD;
    checkerOpParam.DataDes.count = 1024;
    checkerOpParam.DataDes.dataType = CheckerDataType::DATA_TYPE_FP32;
    checkerOpParam.devtype = CheckerDevType::DEV_TYPE_950;
    checkerOpParam.algName = "InsAllGatherParallelMesh2DNHR";

    Checker checker;
    checker.EnableTaskPrint();
    HcclResult ret;
    ret = checker.CheckA5Aicpu(checkerOpParam, topoMeta);
    EXPECT_EQ(ret, HcclResult::HCCL_SUCCESS);
}

TEST_F(AllReduceTest, allreduce_aicpu_case_test_AllReduceNHR_2pod_2mul2_2mul2)
{
    RankTable_For_LLT gen;
    TopoMeta topoMeta{{{0, 1, 2}, {0, 1, 8, 9}}};

    setenv("HCCL_IODIE_NUM", "2", 1);

    CheckerOpParam checkerOpParam;
    checkerOpParam.opType = CheckerOpType::ALLREDUCE;
    checkerOpParam.tag = "AllReduce";
    checkerOpParam.opMode = CheckerOpMode::OFFLOAD;
    checkerOpParam.reduceType = CheckerReduceOp::REDUCE_SUM;
    checkerOpParam.DataDes.count = 1;
    checkerOpParam.DataDes.dataType = CheckerDataType::DATA_TYPE_INT8;
    checkerOpParam.devtype = CheckerDevType::DEV_TYPE_950;
    checkerOpParam.algName = "InsAllReduceNHR";

    Checker checker;
    HcclResult ret;
    ret = checker.CheckA5Aicpu(checkerOpParam, topoMeta);
    EXPECT_EQ(ret, HcclResult::HCCL_SUCCESS);
}

TEST_F(AllReduceTest, allreduce_aicpu_case_test_AllReduceNHR_3pod_2mul1_2mul2_3mul1)
{
    RankTable_For_LLT gen;
    TopoMeta topoMeta{{{0, 1}, {0, 1, 2, 3}, {8, 9, 10}}};

    setenv("HCCL_IODIE_NUM", "2", 1);

    CheckerOpParam checkerOpParam;
    checkerOpParam.opType = CheckerOpType::ALLREDUCE;
    checkerOpParam.tag = "AllReduce";
    checkerOpParam.opMode = CheckerOpMode::OPBASE;
    checkerOpParam.reduceType = CheckerReduceOp::REDUCE_MIN;
    checkerOpParam.DataDes.count = 100;
    checkerOpParam.DataDes.dataType = CheckerDataType::DATA_TYPE_BFP16;
    checkerOpParam.devtype = CheckerDevType::DEV_TYPE_950;
    checkerOpParam.algName = "InsAllReduceNHR";

    Checker checker;
    HcclResult ret;
    ret = checker.CheckA5Aicpu(checkerOpParam, topoMeta);
    EXPECT_EQ(ret, HcclResult::HCCL_SUCCESS);
}

TEST_F(ReduceScatterTest, reduceScatter_aicpu_case_test_ReduceScatterNHR_2pod_2mul2_2mul2)
{
    RankTable_For_LLT gen;
    TopoMeta topoMeta{{{0, 1, 2}, {0, 1, 8, 9}}};
 
    setenv("HCCL_IODIE_NUM", "2", 1);
 
    CheckerOpParam checkerOpParam;
    checkerOpParam.opType = CheckerOpType::REDUCE_SCATTER;
    checkerOpParam.tag = "ReduceScatter";
    checkerOpParam.opMode = CheckerOpMode::OFFLOAD;
    checkerOpParam.reduceType = CheckerReduceOp::REDUCE_SUM;
    checkerOpParam.DataDes.count = 1;
    checkerOpParam.DataDes.dataType = CheckerDataType::DATA_TYPE_INT8;
    checkerOpParam.devtype = CheckerDevType::DEV_TYPE_950;
    checkerOpParam.algName = "InsReduceScatterNHR";
 
    Checker checker;
    HcclResult ret;
    ret = checker.CheckA5Aicpu(checkerOpParam, topoMeta);
    EXPECT_EQ(ret, HcclResult::HCCL_SUCCESS);
}
 
TEST_F(ReduceScatterTest, reduceScatter_aicpu_case_test_ReduceScatterNHR_3pod_2mul1_2mul2_3mul1)
{
    RankTable_For_LLT gen;
    TopoMeta topoMeta{{{0, 1}, {0, 1, 2, 3}, {8, 9, 10}}};
 
    setenv("HCCL_IODIE_NUM", "2", 1);
 
    CheckerOpParam checkerOpParam;
    checkerOpParam.opType = CheckerOpType::REDUCE_SCATTER;
    checkerOpParam.tag = "ReduceScatter";
    checkerOpParam.opMode = CheckerOpMode::OPBASE;
    checkerOpParam.reduceType = CheckerReduceOp::REDUCE_MIN;
    checkerOpParam.DataDes.count = 100;
    checkerOpParam.DataDes.dataType = CheckerDataType::DATA_TYPE_BFP16;
    checkerOpParam.devtype = CheckerDevType::DEV_TYPE_950;
    checkerOpParam.algName = "InsReduceScatterNHR";
 
    Checker checker;
    HcclResult ret;
    ret = checker.CheckA5Aicpu(checkerOpParam, topoMeta);
    EXPECT_EQ(ret, HcclResult::HCCL_SUCCESS);
}

}
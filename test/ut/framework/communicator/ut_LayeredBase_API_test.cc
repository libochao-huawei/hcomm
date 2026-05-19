/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "hccl_api_base_test.h"
#include "layered_base.h"

namespace {
using namespace hccl;

class LayeredBaseProbe : public LayeredBase {
public:
    LayeredBaseProbe() : LayeredBase(nullptr)
    {
    }

    u32 OuterCommRank() const
    {
        return outerCommRank_;
    }

    u32 OuterCommSize() const
    {
        return outerCommSize_;
    }

    u32 InnerCommRank() const
    {
        return innerCommRank_;
    }

    u32 InnerCommSize() const
    {
        return innerCommSize_;
    }

    u32 CrossCommRank() const
    {
        return crossCommRank_;
    }

    u32 CrossCommSize() const
    {
        return crossCommSize_;
    }

    const SubCommInfo &OuterCommInfo() const
    {
        return outerCommInfo_;
    }

    const SubCommInfo &InnerCommInfo() const
    {
        return innerCommInfo_;
    }

    const SubCommInfo &CrossCommInfo() const
    {
        return crossCommInfo_;
    }

    HcclDataType DataType() const
    {
        return dataType_;
    }

    HcclReduceOp ReduceType() const
    {
        return reduceType_;
    }

    u32 DataUnit() const
    {
        return dataUnit_;
    }

    HcclAlgoType InterAlgType() const
    {
        return interAlgType_;
    }

    AlgType AlgTypeValue() const
    {
        return algType_;
    }

    const HcclAlgoInfo &AlgoAttr() const
    {
        return algoAttr_;
    }

    const Stream &StreamValue() const
    {
        return stream_;
    }

    u32 UserRank() const
    {
        return topoAttr_.userRank;
    }

    u32 UserRankSize() const
    {
        return topoAttr_.userRankSize;
    }

    const std::vector<Slice> &OuterDataSlices() const
    {
        return outerDataSlices_;
    }

    u64 BaseOffset() const
    {
        return baseOffset_;
    }

    const DeviceMem &InputMem() const
    {
        return inputMem_;
    }

    const DeviceMem &OutputMem() const
    {
        return outputMem_;
    }

    const DeviceMem &ScratchMem() const
    {
        return scratchMem_;
    }
};

AlgType BuildAlgType(AlgTypeLevel1 level1, AlgTypeLevel2 level2 = AlgTypeLevel2::ALG_LEVEL2_RING)
{
    return AlgType(AlgTypeLevel0::ALG_LEVEL0_WHOLE_RING, level1, level2);
}

SubCommInfo BuildSubCommInfo(u32 localRank, u32 localRankSize)
{
    SubCommInfo commInfo;
    commInfo.localRank = localRank;
    commInfo.localRankSize = localRankSize;
    return commInfo;
}

LayeredParam BuildLayeredParam()
{
    LayeredParam layeredParam;
    layeredParam.baseOffset = 64U;
    layeredParam.algType = BuildAlgType(AlgTypeLevel1::ALG_LEVEL1_RING);
    layeredParam.topoAttr.userRank = 3U;
    layeredParam.topoAttr.userRankSize = 8U;
    layeredParam.interAlgType = HcclAlgoType::HCCL_ALGO_TYPE_NB;
    layeredParam.outerCommInfo = BuildSubCommInfo(1U, 2U);
    layeredParam.innerCommInfo = BuildSubCommInfo(2U, 4U);
    layeredParam.crossCommInfo = BuildSubCommInfo(3U, 6U);
    layeredParam.outerDataSlices = {{0U, 16U}, {16U, 32U}};
    return layeredParam;
}
} // namespace

class LayeredBaseApiTest : public BaseInit {
};

TEST_F(LayeredBaseApiTest, IsSupportLayered_When_GroupSizeIsOne_Expect_False)
{
    EXPECT_FALSE(LayeredBase::IsSupportLayered(1U, BuildAlgType(AlgTypeLevel1::ALG_LEVEL1_RING)));
}

TEST_F(LayeredBaseApiTest, IsSupportLayered_When_GroupSizeGreaterThanOneAndRing_Expect_True)
{
    EXPECT_TRUE(LayeredBase::IsSupportLayered(2U, BuildAlgType(AlgTypeLevel1::ALG_LEVEL1_RING)));
}

TEST_F(LayeredBaseApiTest, IsSupportAlgType_When_Level1HdNbNhrRing_Expect_True)
{
    EXPECT_TRUE(LayeredBase::IsSupportAlgType(BuildAlgType(AlgTypeLevel1::ALG_LEVEL1_HD)));
    EXPECT_TRUE(LayeredBase::IsSupportAlgType(BuildAlgType(AlgTypeLevel1::ALG_LEVEL1_NB)));
    EXPECT_TRUE(LayeredBase::IsSupportAlgType(BuildAlgType(AlgTypeLevel1::ALG_LEVEL1_NHR)));
    EXPECT_TRUE(LayeredBase::IsSupportAlgType(BuildAlgType(AlgTypeLevel1::ALG_LEVEL1_RING)));
}

TEST_F(LayeredBaseApiTest, IsSupportAlgType_When_UnsupportedLevel1_Expect_False)
{
    EXPECT_FALSE(LayeredBase::IsSupportAlgType(BuildAlgType(AlgTypeLevel1::ALG_LEVEL1_AHC)));
    EXPECT_FALSE(LayeredBase::IsSupportAlgType(BuildAlgType(AlgTypeLevel1::ALG_LEVEL1_PIPELINE)));
}

TEST_F(LayeredBaseApiTest, GetLevel1CommType_When_HdNbNhrRing_Expect_MappedCommTypes)
{
    HcclAlgoInfo algoInfo;
    EXPECT_EQ(LayeredBase::GetLevel1CommType(algoInfo, BuildAlgType(AlgTypeLevel1::ALG_LEVEL1_HD)),
        CommType::COMM_TAG_HALVING_DOUBLING);
    EXPECT_EQ(LayeredBase::GetLevel1CommType(algoInfo, BuildAlgType(AlgTypeLevel1::ALG_LEVEL1_NB)),
        CommType::COMM_TAG_NONUNIFORM_BRUCK);
    EXPECT_EQ(LayeredBase::GetLevel1CommType(algoInfo, BuildAlgType(AlgTypeLevel1::ALG_LEVEL1_NHR)),
        CommType::COMM_TAG_NONUNIFORM_HIERARCHICAL_RING);
    EXPECT_EQ(LayeredBase::GetLevel1CommType(algoInfo, BuildAlgType(AlgTypeLevel1::ALG_LEVEL1_RING)),
        CommType::COMM_TAG_RING_INNER);
    EXPECT_EQ(LayeredBase::GetLevel1CommType(algoInfo, BuildAlgType(AlgTypeLevel1::ALG_LEVEL1_AHC)),
        CommType::COMM_TAG_RING_INNER);
}

TEST_F(LayeredBaseApiTest, GetLevel2CommType_When_InterAlgTypeProvided_Expect_CurrentHcommMapping)
{
    EXPECT_EQ(LayeredBase::GetLevel2CommType(HcclAlgoType::HCCL_ALGO_TYPE_DEFAULT),
        CommType::COMM_TAG_RING_INNER);
    EXPECT_EQ(LayeredBase::GetLevel2CommType(HcclAlgoType::HCCL_ALGO_TYPE_RING),
        CommType::COMM_TAG_RING_INNER);
    EXPECT_EQ(LayeredBase::GetLevel2CommType(HcclAlgoType::HCCL_ALGO_TYPE_HDR),
        CommType::COMM_TAG_HALVING_DOUBLING);
    EXPECT_EQ(LayeredBase::GetLevel2CommType(HcclAlgoType::HCCL_ALGO_TYPE_NB),
        CommType::COMM_TAG_NONUNIFORM_BRUCK);
    EXPECT_EQ(LayeredBase::GetLevel2CommType(HcclAlgoType::HCCL_ALGO_TYPE_NHR),
        CommType::COMM_TAG_NONUNIFORM_HIERARCHICAL_RING);
    EXPECT_EQ(LayeredBase::GetLevel2CommType(HcclAlgoType::HCCL_ALGO_TYPE_AHC),
        CommType::COMM_TAG_RING_INNER);
}

TEST_F(LayeredBaseApiTest, GetLevel2CommType_When_AlgTypeProvided_Expect_Level2FallbackMapping)
{
    HcclAlgoInfo algoInfo;
    EXPECT_EQ(LayeredBase::GetLevel2CommType(algoInfo, BuildAlgType(AlgTypeLevel1::ALG_LEVEL1_RING,
        AlgTypeLevel2::ALG_LEVEL2_HD)), CommType::COMM_TAG_HALVING_DOUBLING);
    EXPECT_EQ(LayeredBase::GetLevel2CommType(algoInfo, BuildAlgType(AlgTypeLevel1::ALG_LEVEL1_RING,
        AlgTypeLevel2::ALG_LEVEL2_NB)), CommType::COMM_TAG_NONUNIFORM_BRUCK);
    EXPECT_EQ(LayeredBase::GetLevel2CommType(algoInfo, BuildAlgType(AlgTypeLevel1::ALG_LEVEL1_RING,
        AlgTypeLevel2::ALG_LEVEL2_NHR)), CommType::COMM_TAG_NONUNIFORM_HIERARCHICAL_RING);
    EXPECT_EQ(LayeredBase::GetLevel2CommType(algoInfo, BuildAlgType(AlgTypeLevel1::ALG_LEVEL1_RING,
        AlgTypeLevel2::ALG_LEVEL2_RING)), CommType::COMM_TAG_RING_INNER);
}

TEST_F(LayeredBaseApiTest, Prepare_When_ValidLayeredParam_Expect_CopiesOuterInnerCrossCommInfo)
{
    LayeredBaseProbe layeredBase;
    OpParam opParam;
    opParam.DataDes.dataType = HCCL_DATA_TYPE_INT8;
    ExecMem execMem;
    LayeredParam layeredParam = BuildLayeredParam();

    ASSERT_EQ(layeredBase.Prepare(opParam, execMem, layeredParam), HCCL_SUCCESS);

    EXPECT_EQ(layeredBase.OuterCommRank(), layeredParam.outerCommInfo.localRank);
    EXPECT_EQ(layeredBase.OuterCommSize(), layeredParam.outerCommInfo.localRankSize);
    EXPECT_EQ(layeredBase.InnerCommRank(), layeredParam.innerCommInfo.localRank);
    EXPECT_EQ(layeredBase.InnerCommSize(), layeredParam.innerCommInfo.localRankSize);
    EXPECT_EQ(layeredBase.CrossCommRank(), layeredParam.crossCommInfo.localRank);
    EXPECT_EQ(layeredBase.CrossCommSize(), layeredParam.crossCommInfo.localRankSize);
    EXPECT_EQ(layeredBase.OuterCommInfo().localRank, layeredParam.outerCommInfo.localRank);
    EXPECT_EQ(layeredBase.InnerCommInfo().localRank, layeredParam.innerCommInfo.localRank);
    EXPECT_EQ(layeredBase.CrossCommInfo().localRank, layeredParam.crossCommInfo.localRank);
}

TEST_F(LayeredBaseApiTest, Prepare_When_ValidOpParam_Expect_CopiesStreamDataTypeReduceTypeAndDataUnit)
{
    LayeredBaseProbe layeredBase;
    OpParam opParam;
    opParam.DataDes.dataType = HCCL_DATA_TYPE_INT8;
    opParam.reduceType = HCCL_REDUCE_SUM;
    ExecMem execMem;
    LayeredParam layeredParam = BuildLayeredParam();

    ASSERT_EQ(layeredBase.Prepare(opParam, execMem, layeredParam), HCCL_SUCCESS);

    EXPECT_EQ(layeredBase.DataType(), HCCL_DATA_TYPE_INT8);
    EXPECT_EQ(layeredBase.ReduceType(), HCCL_REDUCE_SUM);
    EXPECT_EQ(layeredBase.DataUnit(), 1U);
}

TEST_F(LayeredBaseApiTest, Prepare_When_OuterDataSlicesProvided_Expect_CachedSlicesMatch)
{
    LayeredBaseProbe layeredBase;
    OpParam opParam;
    opParam.DataDes.dataType = HCCL_DATA_TYPE_INT8;
    ExecMem execMem;
    LayeredParam layeredParam = BuildLayeredParam();

    ASSERT_EQ(layeredBase.Prepare(opParam, execMem, layeredParam), HCCL_SUCCESS);

    ASSERT_EQ(layeredBase.OuterDataSlices().size(), layeredParam.outerDataSlices.size());
    EXPECT_EQ(layeredBase.OuterDataSlices()[0].offset, layeredParam.outerDataSlices[0].offset);
    EXPECT_EQ(layeredBase.OuterDataSlices()[0].size, layeredParam.outerDataSlices[0].size);
    EXPECT_EQ(layeredBase.OuterDataSlices()[1].offset, layeredParam.outerDataSlices[1].offset);
    EXPECT_EQ(layeredBase.OuterDataSlices()[1].size, layeredParam.outerDataSlices[1].size);
}

TEST_F(LayeredBaseApiTest, Prepare_When_AlgoTopoFieldsProvided_Expect_CachedLayeredParamFieldsMatch)
{
    LayeredBaseProbe layeredBase;
    HcclComStreamInfo streamInfo;
    streamInfo.actualStreamId = 7;
    streamInfo.sqId = 8;
    streamInfo.sqDepth = 16;
    streamInfo.sqBaseAddr = reinterpret_cast<void *>(0x1000);
    streamInfo.logicCqId = 9;
    OpParam opParam;
    opParam.stream = Stream(streamInfo, true);
    opParam.DataDes.dataType = HCCL_DATA_TYPE_INT8;
    ExecMem execMem;
    LayeredParam layeredParam = BuildLayeredParam();
    layeredParam.algoAttr.identifier = "layered-base-ut";

    ASSERT_EQ(layeredBase.Prepare(opParam, execMem, layeredParam), HCCL_SUCCESS);

    EXPECT_EQ(layeredBase.AlgTypeValue().algoLevel1, layeredParam.algType.algoLevel1);
    EXPECT_EQ(layeredBase.AlgoAttr().identifier, layeredParam.algoAttr.identifier);
    EXPECT_EQ(layeredBase.StreamValue().id(), opParam.stream.id());
    EXPECT_EQ(layeredBase.StreamValue().ptr(), opParam.stream.ptr());
    EXPECT_EQ(layeredBase.UserRank(), layeredParam.topoAttr.userRank);
    EXPECT_EQ(layeredBase.UserRankSize(), layeredParam.topoAttr.userRankSize);
    EXPECT_EQ(layeredBase.InterAlgType(), layeredParam.interAlgType);
    EXPECT_EQ(layeredBase.BaseOffset(), layeredParam.baseOffset);
}

TEST_F(LayeredBaseApiTest, Prepare_When_LayeredMemAbsent_Expect_FallsBackToExecMem)
{
    LayeredBaseProbe layeredBase;
    OpParam opParam;
    opParam.DataDes.dataType = HCCL_DATA_TYPE_INT8;
    int input = 0;
    int output = 0;
    int scratch = 0;
    ExecMem execMem;
    execMem.inputMem = DeviceMem::create(&input, sizeof(input));
    execMem.outputMem = DeviceMem::create(&output, sizeof(output));
    execMem.scratchMem = DeviceMem::create(&scratch, sizeof(scratch));
    LayeredParam layeredParam = BuildLayeredParam();

    ASSERT_EQ(layeredBase.Prepare(opParam, execMem, layeredParam), HCCL_SUCCESS);

    EXPECT_EQ(layeredBase.InputMem(), execMem.inputMem);
    EXPECT_EQ(layeredBase.OutputMem(), execMem.outputMem);
    EXPECT_EQ(layeredBase.ScratchMem(), execMem.scratchMem);
}

TEST_F(LayeredBaseApiTest, Prepare_When_LayeredMemProvided_Expect_UsesLayeredMem)
{
    LayeredBaseProbe layeredBase;
    OpParam opParam;
    opParam.DataDes.dataType = HCCL_DATA_TYPE_INT8;
    int execInput = 0;
    int execOutput = 0;
    int execScratch = 0;
    ExecMem execMem;
    execMem.inputMem = DeviceMem::create(&execInput, sizeof(execInput));
    execMem.outputMem = DeviceMem::create(&execOutput, sizeof(execOutput));
    execMem.scratchMem = DeviceMem::create(&execScratch, sizeof(execScratch));
    int layeredInput = 0;
    int layeredOutput = 0;
    int layeredScratch = 0;
    LayeredParam layeredParam = BuildLayeredParam();
    layeredParam.inputMem = DeviceMem::create(&layeredInput, sizeof(layeredInput));
    layeredParam.outputMem = DeviceMem::create(&layeredOutput, sizeof(layeredOutput));
    layeredParam.scratchMem = DeviceMem::create(&layeredScratch, sizeof(layeredScratch));

    ASSERT_EQ(layeredBase.Prepare(opParam, execMem, layeredParam), HCCL_SUCCESS);

    EXPECT_EQ(layeredBase.InputMem(), layeredParam.inputMem);
    EXPECT_EQ(layeredBase.OutputMem(), layeredParam.outputMem);
    EXPECT_EQ(layeredBase.ScratchMem(), layeredParam.scratchMem);
}

TEST_F(LayeredBaseApiTest, Prepare_When_LocalInterAlgTypeProvided_Expect_Level2MappingUsesLocalField)
{
    LayeredBaseProbe layeredBase;
    OpParam opParam;
    opParam.DataDes.dataType = HCCL_DATA_TYPE_INT8;
    ExecMem execMem;
    LayeredParam layeredParam = BuildLayeredParam();
    layeredParam.interAlgType = HcclAlgoType::HCCL_ALGO_TYPE_NHR;

    ASSERT_EQ(layeredBase.Prepare(opParam, execMem, layeredParam), HCCL_SUCCESS);

    EXPECT_EQ(LayeredBase::GetLevel2CommType(layeredBase.InterAlgType()),
        CommType::COMM_TAG_NONUNIFORM_HIERARCHICAL_RING);
}

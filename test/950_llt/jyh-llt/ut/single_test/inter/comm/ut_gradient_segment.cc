#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <stdio.h>
#include <fstream>

#include "sal.h"
#define private public
#include "gradient_segment.h"
#undef private
#include "externalinput.h"
#include "llt_hccl_stub_sal_pub.h"
#include "hccl_comm_pub.h"

using namespace std;
using namespace hccl;

class GradientSegmentTest : public testing::Test
{
protected:
    static void SetUpTestCase() {
        std::cout << "\033[36m--GradientSegmentTest SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase() {
        std::cout << "\033[36m--GradientSegmentTest TearDown--\033[0m" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp() {
        s32 portNum = 7;
        MOCKER(hrtGetHccsPortNum)
            .stubs()
            .with(any(), outBound(portNum))
            .will(returnValue(HCCL_SUCCESS));
        std::cout << "A Test SetUP" << std::endl;
    }
    virtual void TearDown() {
        std::cout << "A Test TearDown" << std::endl;
    }
};

TEST_F(GradientSegmentTest, ut_gradient_segment_executor_default_success)
{
    HcclResult ret;

    /* 初始化模型参数 */
    struct model_feature feature;
    std::vector<u32> segment_index;
    u32 rank_size = 1024;
    AlgType alg_type;
    alg_type.algoLevel0 = AlgTypeLevel0::ALG_LEVEL0_8P_RING;
    alg_type.algoLevel1 = AlgTypeLevel1::ALG_LEVEL1_HD;
    char model_name[] = "";
    char group[] = "group";
    feature.gradient_num = 20;
    feature.gradient_size = (float *)sal_malloc(20 * sizeof(float));
    sal_memset(feature.gradient_size, 20 * sizeof(float), 0, 20 * sizeof(float));
    feature.gradient_time = (float *)sal_malloc(20 * sizeof(float));
    sal_memset(feature.gradient_time, 20 * sizeof(float), 0, 20 * sizeof(float));
    feature.model_name = model_name;

    for (u32 i = 0; i < feature.gradient_num; i++)
    {
        feature.gradient_size[i] = 1.0;
    }

    GradientSegment gradient_segment;
    bool isConfig = true;
    ret = gradient_segment.GetGradientSegmentExecutor(group, &feature, segment_index, isConfig);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(segment_index.size(), 2);
    EXPECT_EQ(segment_index[0], 18);
    EXPECT_EQ(segment_index[1], 19);

    sal_free(feature.gradient_size);
    sal_free(feature.gradient_time);
}

TEST_F(GradientSegmentTest, ut_gradient_segment_executor_user_set_by_size)
{
    HcclResult ret;

    /* 初始化模型参数 */
    struct model_feature feature;
    std::vector<u32> segment_index;
    u32 rank_size = 1024;
    AlgType alg_type;
    alg_type.algoLevel0 = AlgTypeLevel0::ALG_LEVEL0_8P_RING;
    alg_type.algoLevel1 = AlgTypeLevel1::ALG_LEVEL1_HD;
    char model_name[] = "";
    char group[] = "group";
    feature.gradient_num = 20;
    float segSizeList[3] = {1, 59, 40};
    std::vector<float> tempList(segSizeList, segSizeList + 3);
    std::string strGroup = group;
    feature.gradient_size = (float *)sal_malloc(20 * sizeof(float));
    sal_memset(feature.gradient_size, 20 * sizeof(float), 0, 20 * sizeof(float));
    feature.gradient_time = (float *)sal_malloc(20 * sizeof(float));
    sal_memset(feature.gradient_time, 20 * sizeof(float), 0, 20 * sizeof(float));
    feature.model_name = model_name;

    for (u32 i = 0; i < feature.gradient_num; i++)
    {
        feature.gradient_size[i] = 1.0;
    }
    g_segmentIdxMap.clear();
    g_segmentSizeMap.clear();
    g_segmentSizeMap.insert(std::pair<std::string, std::vector<float>>(strGroup, tempList));
    GradientSegment gradient_segment;
    bool isConfig = true;
    ret = gradient_segment.GetGradientSegmentExecutor(group, &feature, segment_index, isConfig);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(segment_index.size(), 3);
    EXPECT_EQ(segment_index[0], 0);
    EXPECT_EQ(segment_index[1], 11);
    EXPECT_EQ(segment_index[2], 19);

    sal_free(feature.gradient_size);
    sal_free(feature.gradient_time);
}

TEST_F(GradientSegmentTest, ut_gradient_segment_executor_user_set_by_size_9segment)
{
    HcclResult ret;

    /* 初始化模型参数 */
    struct model_feature feature;
    std::vector<u32> segment_index;
    u32 rank_size = 1024;
    AlgType alg_type;
    alg_type.algoLevel0 = AlgTypeLevel0::ALG_LEVEL0_8P_RING;
    alg_type.algoLevel1 = AlgTypeLevel1::ALG_LEVEL1_HD;
    char model_name[] = "";
    char group[] = "group";
    feature.gradient_num = 20;
    float segSizeList[10] = {10, 10, 10, 10, 10, 10, 10, 10, 10, 10};
    std::vector<float> tempList(segSizeList, segSizeList + 10);
    std::string strGroup = group;
    feature.gradient_size = (float *)sal_malloc(20 * sizeof(float));
    sal_memset(feature.gradient_size, 20 * sizeof(float), 0, 20 * sizeof(float));
    feature.gradient_time = (float *)sal_malloc(20 * sizeof(float));
    sal_memset(feature.gradient_time, 20 * sizeof(float), 0, 20 * sizeof(float));
    feature.model_name = model_name;

    for (u32 i = 0; i < feature.gradient_num; i++)
    {
        feature.gradient_size[i] = 1.0;
    }
    g_segmentIdxMap.clear();
    g_segmentSizeMap.clear();
    g_segmentSizeMap.insert(std::pair<std::string, std::vector<float>>(strGroup, tempList));
    GradientSegment gradient_segment;
    bool isConfig = true;
    ret = gradient_segment.GetGradientSegmentExecutor(group, &feature, segment_index, isConfig);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    sal_free(feature.gradient_size);
    sal_free(feature.gradient_time);
}

TEST_F(GradientSegmentTest, ut_gradient_segment_executor_user_set_by_size_close)
{
    HcclResult ret;

    /* 初始化模型参数 */
    struct model_feature feature;
    u32 segment_num;
    std::vector<u32> segment_index;
    u32 rank_size = 1024;
    AlgType alg_type;
    alg_type.algoLevel0 = AlgTypeLevel0::ALG_LEVEL0_8P_RING;
    alg_type.algoLevel1 = AlgTypeLevel1::ALG_LEVEL1_HD;
    char model_name[] = "";
    char group[] = "group";
    feature.gradient_num = 20;
    float segSizeList[3] = {90, 1, 9};
    std::vector<float> tempList(segSizeList, segSizeList + 3);
    std::string strGroup = group;
    feature.gradient_size = (float *)sal_malloc(20 * sizeof(float));
    sal_memset(feature.gradient_size, 20 * sizeof(float), 0, 20 * sizeof(float));
    feature.gradient_time = (float *)sal_malloc(20 * sizeof(float));
    sal_memset(feature.gradient_time, 20 * sizeof(float), 0, 20 * sizeof(float));
    feature.model_name = model_name;

    for (u32 i = 0; i < feature.gradient_num; i++)
    {
        feature.gradient_size[i] = 1.0;
    }
    g_segmentIdxMap.clear();
    g_segmentSizeMap.clear();
    g_segmentSizeMap.insert(std::pair<std::string, std::vector<float>>(strGroup, tempList));
    GradientSegment gradient_segment;
    bool isConfig = true;
    ret = gradient_segment.GetGradientSegmentExecutor(group, &feature, segment_index, isConfig);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    for (u32 i = 0; i < segment_index.size(); i++) {
        std::cout << "segment_index" << segment_index[i] << std::endl;
    }
    EXPECT_EQ(segment_index.size(), 3);
    EXPECT_EQ(segment_index[0], 17);
    EXPECT_EQ(segment_index[1], 18);
    EXPECT_EQ(segment_index[2], 19);

    sal_free(feature.gradient_size);
    sal_free(feature.gradient_time);
}

TEST_F(GradientSegmentTest, ut_gradient_segment_executor_user_set_by_size_too_close)
{
    HcclResult ret;

    /* 初始化模型参数 */
    struct model_feature feature;
    std::vector<u32> segment_index;
    u32 rank_size = 1024;
    AlgType alg_type;
    alg_type.algoLevel0 = AlgTypeLevel0::ALG_LEVEL0_8P_RING;
    alg_type.algoLevel1 = AlgTypeLevel1::ALG_LEVEL1_HD;
    char model_name[] = "";
    char group[] = "group";
    feature.gradient_num = 20;
    float segSizeList[3] = {90, 8, 2};
    std::vector<float> tempList(segSizeList, segSizeList + 3);
    std::string strGroup = group;
    feature.gradient_size = (float *)sal_malloc(20 * sizeof(float));
    sal_memset(feature.gradient_size, 20 * sizeof(float), 0, 20 * sizeof(float));
    feature.gradient_time = (float *)sal_malloc(20 * sizeof(float));
    sal_memset(feature.gradient_time, 20 * sizeof(float), 0, 20 * sizeof(float));
    feature.model_name = model_name;

    for (u32 i = 0; i < feature.gradient_num; i++)
    {
        feature.gradient_size[i] = 1.0;
    }
    g_segmentIdxMap.clear();
    g_segmentSizeMap.clear();
    g_segmentSizeMap.insert(std::pair<std::string, std::vector<float>>(strGroup, tempList));
    GradientSegment gradient_segment;
    bool isConfig = true;
    ret = gradient_segment.GetGradientSegmentExecutor(group, &feature, segment_index, isConfig);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    sal_free(feature.gradient_size);
    sal_free(feature.gradient_time);
}


TEST_F(GradientSegmentTest, ut_gradient_segment_executor_user_set_by_idx)
{
    HcclResult ret;

    /* 初始化模型参数 */
    struct model_feature feature;
    std::vector<u32> segment_index;
    u32 rank_size = 1024;
    AlgType alg_type;
    alg_type.algoLevel0 = AlgTypeLevel0::ALG_LEVEL0_8P_RING;
    alg_type.algoLevel1 = AlgTypeLevel1::ALG_LEVEL1_HD;
    char model_name[] = "";
    char group[] = "group";
    feature.gradient_num = 20;
    u32 segIdxList[3] = {0, 15, 19};
    std::vector<u32> tempList(segIdxList, segIdxList + 3);
    std::string strGroup = group;
    feature.gradient_size = (float *)sal_malloc(20 * sizeof(float));
    sal_memset(feature.gradient_size, 20 * sizeof(float), 0, 20 * sizeof(float));
    feature.gradient_time = (float *)sal_malloc(20 * sizeof(float));
    sal_memset(feature.gradient_time, 20 * sizeof(float), 0, 20 * sizeof(float));
    feature.model_name = model_name;

    for (u32 i = 0; i < feature.gradient_num; i++)
    {
        feature.gradient_size[i] = 1.0;
    }
    g_segmentIdxMap.clear();
    g_segmentSizeMap.clear();
    g_segmentIdxMap.insert(std::pair<std::string, std::vector<u32>>(strGroup, tempList));
    GradientSegment gradient_segment;
    bool isConfig = true;
    ret = gradient_segment.GetGradientSegmentExecutor(group, &feature, segment_index, isConfig);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(segment_index.size(), 3);
    EXPECT_EQ(segment_index[0], 0);
    EXPECT_EQ(segment_index[1], 15);
    EXPECT_EQ(segment_index[2], 19);

    sal_free(feature.gradient_size);
    sal_free(feature.gradient_time);
}

TEST_F(GradientSegmentTest, ut_gradient_segment_executor_user_set_by_idx_13segment)
{
    HcclResult ret;

    /* 初始化模型参数 */
    struct model_feature feature;
    std::vector<u32> segment_index;
    u32 rank_size = 1024;
    AlgType alg_type;
    alg_type.algoLevel0 = AlgTypeLevel0::ALG_LEVEL0_8P_RING;
    alg_type.algoLevel1 = AlgTypeLevel1::ALG_LEVEL1_HD;
    char model_name[] = "";
    char group[] = "group";
    feature.gradient_num = 20;
    u32 segIdxList[13] = {0,1,2,3,4,5,6,7,8,9,10,11,19};
    std::vector<u32> tempList(segIdxList, segIdxList + 13);
    std::string strGroup = group;
    feature.gradient_size = (float *)sal_malloc(20 * sizeof(float));
    sal_memset(feature.gradient_size, 20 * sizeof(float), 0, 20 * sizeof(float));
    feature.gradient_time = (float *)sal_malloc(20 * sizeof(float));
    sal_memset(feature.gradient_time, 20 * sizeof(float), 0, 20 * sizeof(float));
    feature.model_name = model_name;

    for (u32 i = 0; i < feature.gradient_num; i++)
    {
        feature.gradient_size[i] = 1.0;
    }
    g_segmentIdxMap.clear();
    g_segmentSizeMap.clear();
    g_segmentIdxMap.insert(std::pair<std::string, std::vector<u32>>(strGroup, tempList));
    GradientSegment gradient_segment;
    bool isConfig = true;
    ret = gradient_segment.GetGradientSegmentExecutor(group, &feature, segment_index, isConfig);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    sal_free(feature.gradient_size);
    sal_free(feature.gradient_time);
}

TEST_F(GradientSegmentTest, ut_gradient_segment_executor_user_set_by_size_unknownshap)
{
    HcclResult ret;

    /* 初始化模型参数 */
    struct model_feature feature;
    std::vector<u32> segment_index;
    u32 rank_size = 1024;
    AlgType alg_type;
    alg_type.algoLevel0 = AlgTypeLevel0::ALG_LEVEL0_8P_RING;
    alg_type.algoLevel1 = AlgTypeLevel1::ALG_LEVEL1_HD;
    char model_name[] = "";
    char group[] = "group";
    feature.gradient_num = 20;
    float segSizeList[3] = {1, 59, 40};
    std::vector<float> tempList(segSizeList, segSizeList + 3);
    std::string strGroup = group;
    feature.gradient_size = (float *)sal_malloc(20 * sizeof(float));
    sal_memset(feature.gradient_size, 20 * sizeof(float), 0, 20 * sizeof(float));
    feature.gradient_time = (float *)sal_malloc(20 * sizeof(float));
    sal_memset(feature.gradient_time, 20 * sizeof(float), 0, 20 * sizeof(float));
    feature.model_name = model_name;

    for (u32 i = 0; i < feature.gradient_num; i++)
    {
        feature.gradient_size[i] = 1.0;
    }
    g_segmentIdxMap.clear();
    g_segmentSizeMap.clear();
    g_segmentSizeMap.insert(std::pair<std::string, std::vector<float>>(strGroup, tempList));
    GradientSegment gradient_segment;
    bool isConfig = true;
    ret = gradient_segment.GetGradientSegmentExecutor(group, &feature, segment_index, isConfig,
        FORCE_NONE, UNKNOWN_SHAPE);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(segment_index.size(), 3);
    EXPECT_EQ(segment_index[0], 0);
    EXPECT_EQ(segment_index[1], 11);
    EXPECT_EQ(segment_index[2], 19);

    sal_free(feature.gradient_size);
    sal_free(feature.gradient_time);
}

TEST_F(GradientSegmentTest, ut_gradient_segment_executor_user_set_by_size_unknownshap_greaterCCLbuf)
{
    HcclResult ret;

    /* 初始化模型参数 */
    struct model_feature feature;
    std::vector<u32> segment_index;
    u32 rank_size = 1024;
    AlgType alg_type;
    alg_type.algoLevel0 = AlgTypeLevel0::ALG_LEVEL0_8P_RING;
    alg_type.algoLevel1 = AlgTypeLevel1::ALG_LEVEL1_HD;
    char model_name[] = "";
    char group[] = "group";
    feature.gradient_num = 5;
    float segSizeList[2] = {60, 40};//5个梯度99,99,99,99,99分为300,200。unknownshap最终结果:198,99,198对应的index=1,2,4
    std::vector<float> tempList(segSizeList, segSizeList + 2);
    std::string strGroup = group;
    feature.gradient_size = (float *)sal_malloc(feature.gradient_num * sizeof(float));
    sal_memset(feature.gradient_size, feature.gradient_num * sizeof(float), 0, feature.gradient_num * sizeof(float));
    feature.gradient_time = (float *)sal_malloc(feature.gradient_num * sizeof(float));
    sal_memset(feature.gradient_time, feature.gradient_num * sizeof(float), 0, feature.gradient_num * sizeof(float));
    feature.model_name = model_name;

    for (u32 i = 0; i < feature.gradient_num; i++)
    {
        feature.gradient_size[i] = (GetExternalInputCCLBuffSize() - CCL_COMM_INBUFFER_UNALIGNED_RESERVE_SIZE) / 2;//每个梯度99M
    }

    g_segmentIdxMap.clear();
    g_segmentSizeMap.clear();
    g_segmentSizeMap.insert(std::pair<std::string, std::vector<float>>(strGroup, tempList));
    GradientSegment gradient_segment;
    bool isConfig = true;
    ret = gradient_segment.GetGradientSegmentExecutor(group, &feature,
        segment_index, isConfig, FORCE_NONE, UNKNOWN_SHAPE);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    for (u32 i = 0; i < segment_index.size(); i++) {
        std::cout << "segment_index" << segment_index[i] << std::endl;
    }
    sal_free(feature.gradient_size);
    EXPECT_EQ(segment_index.size(), 3);
    EXPECT_EQ(segment_index[0], 1);
    EXPECT_EQ(segment_index[1], 2);
    EXPECT_EQ(segment_index[2], 4);

    sal_free(feature.gradient_time);
}

TEST_F(GradientSegmentTest, ut_gradient_segment_executor_default_success_unknownshap_more_greaterCCLbuf)
{
    HcclResult ret;

    /* 初始化模型参数 */
    struct model_feature feature;
    std::vector<u32> segment_index;
    u32 rank_size = 1024;
    AlgType alg_type;
    alg_type.algoLevel0 = AlgTypeLevel0::ALG_LEVEL0_8P_RING;
    alg_type.algoLevel1 = AlgTypeLevel1::ALG_LEVEL1_HD;
    char model_name[] = "";
    char group[] = "group";
    feature.gradient_num = 10;
    feature.gradient_size = (float *)sal_malloc(feature.gradient_num * sizeof(float));
    sal_memset(feature.gradient_size, feature.gradient_num * sizeof(float), 0, feature.gradient_num * sizeof(float));
    feature.gradient_time = (float *)sal_malloc(feature.gradient_num * sizeof(float));
    sal_memset(feature.gradient_time, feature.gradient_num * sizeof(float), 0, feature.gradient_num * sizeof(float));
    feature.model_name = model_name;

    for (u32 i = 0; i < feature.gradient_num; i++)
    {
        feature.gradient_size[i] = 40 * (1 * 1024 * 1024);//每个梯度40M
    }

    g_segmentIdxMap.clear();
    g_segmentSizeMap.clear();
    GradientSegment gradient_segment;
    bool isConfig = true;
    ret = gradient_segment.GetGradientSegmentExecutor(group, &feature,
        segment_index, isConfig, FORCE_SIZE, UNKNOWN_SHAPE);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(segment_index.size(), 4);
    EXPECT_EQ(segment_index[0], 3);
    EXPECT_EQ(segment_index[1], 7);
    EXPECT_EQ(segment_index[2], 8);
    EXPECT_EQ(segment_index[3], 9);

    sal_free(feature.gradient_size);
    sal_free(feature.gradient_time);
}

#if 1
TEST_F(GradientSegmentTest, ut_gradient_segment_executor_default_success_unknownshap)
{
    HcclResult ret;

    /* 初始化模型参数 */
    struct model_feature feature;
    std::vector<u32> segment_index;
    u32 rank_size = 1024;
    AlgType alg_type;
    alg_type.algoLevel0 = AlgTypeLevel0::ALG_LEVEL0_8P_RING;
    alg_type.algoLevel1 = AlgTypeLevel1::ALG_LEVEL1_HD;
    char model_name[] = "";
    char group[] = "group";
    feature.gradient_num = 5;
    feature.gradient_size = (float *)sal_malloc(feature.gradient_num * sizeof(float));
    sal_memset(feature.gradient_size, feature.gradient_num * sizeof(float), 0, feature.gradient_num * sizeof(float));
    feature.gradient_time = (float *)sal_malloc(feature.gradient_num * sizeof(float));
    sal_memset(feature.gradient_time, feature.gradient_num * sizeof(float), 0, feature.gradient_num * sizeof(float));
    feature.model_name = model_name;

    for (u32 i = 0; i < feature.gradient_num; i++)
    {
        feature.gradient_size[i] = (GetExternalInputCCLBuffSize() - CCL_COMM_INBUFFER_UNALIGNED_RESERVE_SIZE) / 2;//每个梯度99M
    }

    g_segmentIdxMap.clear();
    g_segmentSizeMap.clear();
    GradientSegment gradient_segment;
    bool isConfig = true;
    ret = gradient_segment.GetGradientSegmentExecutor(group, &feature,
        segment_index, isConfig, FORCE_SIZE, UNKNOWN_SHAPE);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(segment_index.size(), 3);
    EXPECT_EQ(segment_index[0], 1);
    EXPECT_EQ(segment_index[1], 3);
    EXPECT_EQ(segment_index[2], 4);

    sal_free(feature.gradient_size);
    sal_free(feature.gradient_time);
}
#endif
TEST_F(GradientSegmentTest, ut_gradient_segment_by_size_test_boundary)
{
    HcclResult ret;
    GradientSegment gradient_segment;
    const std::vector<float> accumGradList {1, 3, 5, 11};
    const float curSizeLeft = 6;
    u32 segGradIdxLeft;
    ret = gradient_segment.GetIdxByBinarySearch(accumGradList, curSizeLeft, segGradIdxLeft);
    EXPECT_EQ(segGradIdxLeft, 2);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    const float curSizeRight = 10;
    u32 segGradIdxRight;
    ret = gradient_segment.GetIdxByBinarySearch(accumGradList, curSizeRight, segGradIdxRight);
    EXPECT_EQ(segGradIdxRight, 3);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    const float curSize = 3;
    u32 segGradIdx;
    ret = gradient_segment.GetIdxByBinarySearch(accumGradList, curSize, segGradIdx);
    EXPECT_EQ(segGradIdx, 1);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <stdio.h>
#include <fstream>

#define private public
#include "gradient_segment.h"
#undef private
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
        s32 portNum = -1;
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

TEST_F(GradientSegmentTest, st_gradient_segment_unknownshap_greaterCCLbuf)
{
    HcclResult ret;

    /* 初始化模型参数 */
    struct model_feature feature;
    std::vector<u32> segment_index;
    u32 rank_size = 1024;
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
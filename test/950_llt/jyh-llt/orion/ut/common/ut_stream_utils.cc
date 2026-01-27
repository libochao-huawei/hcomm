#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include "log.h"
#include "communicator_impl.h"
#include "internal_exception.h"
#include "invalid_params_exception.h"
#include "stream_utils.h"

using namespace Hccl;

class StreamUtilsTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "StreamUtilsTest tests set up." << std::endl;
    }

    static void TearDownTestCase()
    {
        std::cout << "StreamUtilsTest tests tear down." << std::endl;
    }

    virtual void SetUp()
    {
        std::cout << "A Test case in StreamUtilsTest SetUP" << std::endl;
    }

    virtual void TearDown()
    {
        std::cout << "A Test case in StreamUtilsTest TearDown" << std::endl;
    }
};

TEST_F(StreamUtilsTest, Ut_AddStreamToModel_When_rtStreamAddToModel_fail_Expect_HCCL_E_RUNTIME)
{
    // 前置条件
    MOCKER(&rtStreamAddToModel).stubs().will(returnValue(1));
    rtStream_t stream;
    rtModel_t rtModel;

    // 后置验证
    EXPECT_EQ(AddStreamToModel(stream, rtModel), HCCL_E_RUNTIME);
}

TEST_F(StreamUtilsTest, Ut_GetModelId_When_rtModelGetId_fail_Expect_HCCL_E_RUNTIME)
{
    // 前置条件
    MOCKER(&rtModelGetId).stubs().will(returnValue(1));
    rtModel_t rtModel;
    u32 model = 0;

    // 后置验证
    EXPECT_EQ(GetModelId(rtModel, model), HCCL_E_RUNTIME);
}

TEST_F(StreamUtilsTest, Ut_GetStreamCaptureInfo_When_aclmdlRICaptureGetInfo_success_Expect_HCCL_SUCCESS)
{
    // 前置条件
    MOCKER(&aclmdlRICaptureGetInfo).stubs().will(returnValue(207000));
    rtStream_t stream;
    rtModel_t rtModel;
    bool isCapture;

    // 后置验证
    EXPECT_EQ(GetStreamCaptureInfo(stream, rtModel, isCapture), HCCL_SUCCESS);
}
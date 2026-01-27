#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>

#ifndef private
#define private public
#define protected public
#endif
#include "stream_utils.h"
#include "env_config.h"
#include "op_base.h"
#undef private
#undef protected

using namespace std;

class Stream_Device_UT : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "Stream_Device_UT SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "Stream_Device_UT TearDown" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        std::cout << "A Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "A Test TearDown" << std::endl;
    }
};

TEST_F(Stream_Device_UT, StreamDeviceTest) {
    rtStream_t aicpuStream;
    rtModel_t rtModel = nullptr;
    bool isCapture = false;
    u32 modelId = 0;
    GetStreamCaptureInfo(aicpuStream, rtModel, isCapture);
    AddStreamToModel(aicpuStream, rtModel);
    GetModelId(rtModel, modelId);
}

TEST_F(Stream_Device_UT, EnvDeviceTest) {
    std::string opName = "llt";
    std::vector<HcclAlgoType> algType;
    ParseAlgoString(opName, opName, algType);
    SetHcclAlgoConfig(opName);
}

TEST_F(Stream_Device_UT, OpBaseDeviceTest) {
    aclrtStream stream;
    rtStreamCaptureStatus captureStatus = RT_STREAM_CAPTURE_STATUS_NONE;
    uint32_t modelId = 0;
    bool isCapture = true;
    GetCaptureInfo(stream, captureStatus, modelId, isCapture);
}
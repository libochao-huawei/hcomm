#include "gtest/gtest.h"
#include <mockcpp/mokc.h>
#include <mockcpp/mockcpp.hpp>
#include "orion_adapter_qos.h"
#include "runtime_api_exception.h"
using namespace Hccl;

class AdapterQosTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "AdapterQosTest tests set up." << std::endl;
    }

    static void TearDownTestCase()
    {
        std::cout << "AdapterQosTest tests tear down." << std::endl;
    }

    virtual void SetUp()
    {
        std::cout << "A Test case in AdapterQosTest SetUP" << std::endl;
    }

    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "A Test case in AdapterQosTest TearDown" << std::endl;
    }
};

TEST_F(AdapterQosTest, HrtGetQosConfig_return_ok)
{
    // Given
    QosConfig info;
    info.qos            = 0xFU;
    info.mpamId         = 0xFFU;

    MOCKER(QosGetStreamEngineQos)
        .stubs()
        .with(any(), any(), any(), outBoundP(&info, sizeof(info)))
        .will(returnValue(QosErrorCode::QOS_SUCCESS));

    // when
    string opType = "HCCS";
    u32 qosCfg = 1;
    DevType devType = hrtGetQosConfig(1, 1, opType, &qosCfg);
}


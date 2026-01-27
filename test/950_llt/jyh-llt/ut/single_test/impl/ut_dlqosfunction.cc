#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include "dlqos_function.h"

#include "gtest/gtest.h"
#include <stdio.h>
#include <mockcpp/mockcpp.hpp>
#include "../inc/hccl/base.h"
#include <hccl/hccl_types.h>
#include "hccl_comm_pub.h"
#include "llt_hccl_stub_pub.h"

using namespace hccl;
using namespace std;

class QosFunctionTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "\033[36m--DlOpenFunction SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "\033[36m--DlOpenFunction TearDown--\033[0m" << std::endl;
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

TEST_F(QosFunctionTest, dlqosTest)
{
    s32 ret = HCCL_SUCCESS;
    ret = DlQosFunction::GetInstance().DlQosFunctionInit();

    EXPECT_EQ(ret, HCCL_SUCCESS);
    
    QosConfig info;
    s32 deviceLogicId = 0;
    std::string opType = "HcomAllReduce";
    QosErrorCode qosRet = DlQosFunction::GetInstance().dlGetStreamEngineQos(QosStreamType::STREAM_MAX, \
        QosEngineType::HCCL, opType, deviceLogicId, &info);
    EXPECT_EQ(qosRet, QosErrorCode::QOS_SUCCESS);
}

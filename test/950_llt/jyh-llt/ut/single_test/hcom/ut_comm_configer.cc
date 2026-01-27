#include "gtest/gtest.h"
#include <hccl/hccl.h>

#define private public
#define protected public
#include "comm_config_pub.h"
#include "comm_configer.h"
#undef protected
#undef private
 
using namespace std;
using namespace hccl;

class CommConfigerTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "\033[36m--CommConfigerTest SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "\033[36m--CommConfigerTest TearDown--\033[0m" << std::endl;
    }
    virtual void SetUp()
    {
        std::cout << "A Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        std::cout << "A Test TearDown" << std::endl;
    }
};
 
TEST_F(CommConfigerTest, ut_comm_configer_execTimeOut)
{
    HcclResult ret = HCCL_SUCCESS;
    std::string identifier = "hccl_world_group";

    // commConfiger设置
    CommConfig commConfig(identifier);
    ret = CommConfiger::GetInstance().SetCommConfig(commConfig, identifier);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = CommConfiger::GetInstance().SetCommConfig(commConfig, identifier);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    // commConfig设置超时时间
    ret = CommConfiger::GetInstance().SetCommConfigExecTimeOut(1000, "test_group");
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = CommConfiger::GetInstance().SetCommConfigExecTimeOut(1000, identifier);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    // 获取超时时间
    s32 execTimeOut = CommConfiger::GetInstance().GetCommConfigExecTimeOut(identifier);
    EXPECT_EQ(execTimeOut, 1000);
    execTimeOut = CommConfiger::GetInstance().GetCommConfigExecTimeOut("test_group");
    EXPECT_EQ(execTimeOut, 1836);
}

TEST_F(CommConfigerTest, ut_comm_configer_hccl_algo)
{
    HcclResult ret = HCCL_SUCCESS;
    HcclCommConfig userConfig;
    HcclCommConfigInit(&userConfig);
    userConfig.hcclBufferSize = 300;
    userConfig.hcclDeterministic = 1;
    userConfig.hcclExecTimeOut = 2000;
    strcpy_s(userConfig.hcclCommName, COMM_NAME_MAX_LENGTH, "test_comm11");
    strcpy_s(userConfig.hcclAlgo, HCCL_COMM_ALGO_MAX_LENGTH, "level0:NA;level1:ring");
    strcpy_s(userConfig.hcclRetryEnable, HCCL_COMM_RETRY_ENABLE_MAX_LENGTH, "L1:1, L2:1");
    strcpy_s(userConfig.hcclRetryParams, HCCL_COMM_RETRY_PARAMS_MAX_LENGTH, "MaxCnt:5, HoldTime:5000, IntervalTime:5000");

    std::string identifier = "test_comm11";
    CommConfig commConfig(identifier);
    ret = commConfig.Load(&userConfig);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = CommConfiger::GetInstance().SetCommConfig(commConfig, identifier);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    EXPECT_EQ(CommConfiger::GetInstance().GetCommConfigAlgoConfig(identifier)[0], HcclAlgoType::HCCL_ALGO_TYPE_NA);
    EXPECT_EQ(CommConfiger::GetInstance().GetCommConfigAlgoConfig(identifier)[1], HcclAlgoType::HCCL_ALGO_TYPE_RING);
    EXPECT_EQ(CommConfiger::GetInstance().GetCommConfigAlgoConfig(identifier)[2], HcclAlgoType::HCCL_ALGO_TYPE_DEFAULT);
    EXPECT_EQ(CommConfiger::GetInstance().GetCommConfigAlgoConfig(identifier)[3], HcclAlgoType::HCCL_ALGO_TYPE_DEFAULT);

    EXPECT_EQ(CommConfiger::GetInstance().GetCommConfigInterServerRetryEnable(identifier), 1);
    EXPECT_EQ(CommConfiger::GetInstance().GetCommConfigInterSuperPodRetryEnable(identifier), 1);
    EXPECT_EQ(CommConfiger::GetInstance().GetCommConfigRetryMaxCnt(identifier), 5);
    EXPECT_EQ(CommConfiger::GetInstance().GetCommConfigRetryHoldTime(identifier), 5000);
    EXPECT_EQ(CommConfiger::GetInstance().GetCommConfigRetryIntervalTime(identifier), 5000);
}
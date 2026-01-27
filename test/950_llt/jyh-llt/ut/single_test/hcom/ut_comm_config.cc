#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>

#include <cstdio>
#include <cstdlib>

#include <hccl/hccl.h>
#include "externalinput_pub.h"
#include "externalinput.h"
#include "adapter_rts.h"

#define private public
#define protected public
#include "comm_config_pub.h"
#include "comm_configer.h"
#undef protected
#undef private
using namespace std;
using namespace hccl;

class CommConfigTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "\033[36m--CommConfigTest SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "\033[36m--CommConfigTest TearDown--\033[0m" << std::endl;
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

TEST_F(CommConfigTest, utCommConfig_load)
{
    MOCKER(GetExternalInputCCLBuffSize)
    .stubs()
    .will(returnValue(static_cast<u64>(200 * HCCL_CCL_COMM_FIXED_CALC_BUFFER_SIZE)));

    MOCKER(GetExternalInputHcclDeterministic)
    .stubs()
    .will(returnValue(false));

    CommConfig commConfig("comm_ID");

    EXPECT_EQ(commConfig.GetConfigBufferSize(), 200 * HCCL_CCL_COMM_FIXED_CALC_BUFFER_SIZE);
    EXPECT_EQ(commConfig.GetConfigDeterministic(), 0);

    HcclCommConfig userConfig;
    HcclCommConfigInit(&userConfig);

    userConfig.hcclBufferSize = 300;
    userConfig.hcclDeterministic = 1;
    strcpy_s(userConfig.hcclCommName, COMM_NAME_MAX_LENGTH, "Comm1");

    HcclResult ret = commConfig.Load(&userConfig);

    EXPECT_EQ(ret, HCCL_SUCCESS);

    EXPECT_EQ(commConfig.GetConfigBufferSize(), 300 * HCCL_CCL_COMM_FIXED_CALC_BUFFER_SIZE);
    EXPECT_EQ(commConfig.GetConfigDeterministic(), 1);
    EXPECT_EQ(commConfig.GetConfigCommName(), "Comm1");
    GlobalMockObject::verify();
}

TEST_F(CommConfigTest, utCommConfig_magicword_verify)
{
    MOCKER(GetExternalInputCCLBuffSize)
    .stubs()
    .will(returnValue(static_cast<u64>(200 * HCCL_CCL_COMM_FIXED_CALC_BUFFER_SIZE)));

    MOCKER(GetExternalInputHcclDeterministic)
    .stubs()
    .will(returnValue(false));

    CommConfig commConfig("comm_ID");
    CommConfigInfo configInfo = { sizeof(CommConfigHandle), COMM_CONFIG_MAGIC_WORD, 1, { 0 } };
    CommConfigHandle configHandle = { configInfo, 200, 0 };

    HcclResult ret = commConfig.CheckMagicWord(configHandle);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    configHandle.info = { sizeof(configHandle), 0, 1, { 0 } };
    ret = commConfig.CheckMagicWord(configHandle);
    EXPECT_EQ(ret, HCCL_E_PARA);
    GlobalMockObject::verify();
}

TEST_F(CommConfigTest, utCommConfig_version_compatibility_v0)
{
    MOCKER(GetExternalInputCCLBuffSize)
    .stubs()
    .will(returnValue(static_cast<u64>(200 * HCCL_CCL_COMM_FIXED_CALC_BUFFER_SIZE)));

    MOCKER(GetExternalInputHcclDeterministic)
    .stubs()
    .will(returnValue(false));

    CommConfig commConfig("comm_ID");
    CommConfigInfo configInfo = { sizeof(CommConfigHandle), COMM_CONFIG_MAGIC_WORD, 0, { 0 } };
    CommConfigHandle configHandle = { configInfo, 300, 1 };

    HcclResult ret = commConfig.SetConfigByVersion(configHandle);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(commConfig.GetConfigBufferSize(), 200 * HCCL_CCL_COMM_FIXED_CALC_BUFFER_SIZE);
    EXPECT_EQ(commConfig.GetConfigDeterministic(), 0);
    EXPECT_EQ(commConfig.GetConfigCommName(), "comm_ID");
    GlobalMockObject::verify();
}

TEST_F(CommConfigTest, utCommConfig_version_compatibility_v1)
{
    MOCKER(GetExternalInputCCLBuffSize)
    .stubs()
    .will(returnValue(static_cast<u64>(200 * HCCL_CCL_COMM_FIXED_CALC_BUFFER_SIZE)));

    MOCKER(GetExternalInputHcclDeterministic)
    .stubs()
    .will(returnValue(false));

    CommConfig commConfig("comm_ID");
    CommConfigInfo configInfo = { sizeof(CommConfigHandle), COMM_CONFIG_MAGIC_WORD, 1, { 0 } };
    CommConfigHandle configHandle = { configInfo, 300, 1, "comm_ID", "should_not_be_loaded", 0, 132, 4};

    HcclResult ret = commConfig.SetConfigByVersion(configHandle);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(commConfig.GetConfigBufferSize(), 300 * HCCL_CCL_COMM_FIXED_CALC_BUFFER_SIZE);
    EXPECT_EQ(commConfig.GetConfigDeterministic(), 1);
    EXPECT_EQ(commConfig.GetConfigCommName(), "comm_ID");
    GlobalMockObject::verify();
}

TEST_F(CommConfigTest, utCommConfig_default_env_config)
{
    MOCKER(GetExternalInputCCLBuffSize)
    .stubs()
    .will(returnValue(static_cast<u64>(200 * HCCL_CCL_COMM_FIXED_CALC_BUFFER_SIZE)));

    MOCKER(GetExternalInputHcclDeterministic)
    .stubs()
    .will(returnValue(false));

    CommConfig commConfig("comm_ID");
    CommConfigInfo configInfo = { sizeof(CommConfigHandle), COMM_CONFIG_MAGIC_WORD, 1, { 0 } };
    CommConfigHandle configHandle = { configInfo, HCCL_COMM_BUFFSIZE_CONFIG_NOT_SET, HCCL_COMM_DETERMINISTIC_CONFIG_NOT_SET };

    HcclResult ret = commConfig.SetConfigByVersion(configHandle);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(commConfig.GetConfigBufferSize(), 200 * HCCL_CCL_COMM_FIXED_CALC_BUFFER_SIZE);
    EXPECT_EQ(commConfig.GetConfigDeterministic(), 0);
    GlobalMockObject::verify();
}

ExternalInput g_externalInput;

TEST_F(CommConfigTest, utCommConfig_op_expansion)
{
    MOCKER(GetExternalInputCCLBuffSize)
    .stubs()
    .will(returnValue(static_cast<u64>(200 * HCCL_CCL_COMM_FIXED_CALC_BUFFER_SIZE)));
 
    MOCKER(GetExternalInputHcclDeterministic)
    .stubs()
    .will(returnValue(false));

    DevType deviceType = DevType::DEV_TYPE_910B;
    MOCKER(hrtGetDeviceType)
    .stubs()
    .with(outBound(deviceType))
    .will(returnValue(HCCL_SUCCESS));

    CommConfig commConfig("comm_ID");
    CommConfigInfo configInfo = { sizeof(CommConfigHandle), COMM_CONFIG_MAGIC_WORD, 4, { 0 } };
    CommConfigHandle configHandle = { configInfo, 300, 1, "comm_ID", "Unspecified", 3, 132, 4};

    HcclResult ret = commConfig.SetConfigByVersion(configHandle);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(commConfig.GetConfigAivMode(), true);
    EXPECT_EQ(commConfig.GetConfigDeterministic(), 1);
    EXPECT_EQ(configHandle.info.version, 4);
    EXPECT_EQ(configHandle.opExpansionMode, 3);

    configHandle.opExpansionMode = 1;
    ret = commConfig.SetConfigByVersion(configHandle);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    configHandle.opExpansionMode = 2;
    ret = commConfig.SetConfigByVersion(configHandle);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    configHandle.opExpansionMode = 3;
    ret = commConfig.SetConfigByVersion(configHandle);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    configHandle.opExpansionMode = 4;
    ret = commConfig.SetConfigByVersion(configHandle);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    g_externalInput.aicpuUnfold = false;
    ret = commConfig.SetConfigByVersion(configHandle);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(CommConfigTest, utCommConfig_op_expansion_v0)
{
    DevType deviceType = DevType::DEV_TYPE_910B;
    MOCKER(hrtGetDeviceType)
    .stubs()
    .with(outBound(deviceType))
    .will(returnValue(HCCL_SUCCESS));

    CommConfig commConfig("comm_ID");
    CommConfigInfo configInfo = { sizeof(CommConfigHandle), COMM_CONFIG_MAGIC_WORD, 4, { 0 } };
    CommConfigHandle configHandle = { configInfo, 300, 1, "comm_ID", "Unspecified", 3, 132, 4};
    g_externalInput.hcclDeterministic == true;
    configHandle.opExpansionMode = 3;
    configHandle.deterministic = 1;
    HcclResult ret = commConfig.SetConfigByVersion(configHandle);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(commConfig.GetConfigAivMode(), true);
    EXPECT_EQ(commConfig.GetConfigDeterministic(), 1);
    ret = commConfig.SetConfigOpExpansionMode(configHandle);
    configHandle.opExpansionMode = 0;
    ret = commConfig.SetConfigByVersion(configHandle);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    configHandle.opExpansionMode = 1;
    ret = commConfig.SetConfigByVersion(configHandle);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    configHandle.opExpansionMode = 2;
    ret = commConfig.SetConfigByVersion(configHandle);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    configHandle.opExpansionMode = 999;
    ret = commConfig.SetConfigByVersion(configHandle);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(CommConfigTest, utCommConfig_deterministic_strcit)
{
    MOCKER(GetExternalInputCCLBuffSize)
    .stubs()
    .will(returnValue(static_cast<u64>(200 * HCCL_CCL_COMM_FIXED_CALC_BUFFER_SIZE)));

    MOCKER(GetExternalInputHcclDeterministicV2).stubs().will(returnValue(0));

    DevType deviceType = DevType::DEV_TYPE_910B;
    MOCKER(hrtGetDeviceType).stubs().with(outBound(deviceType)).will(returnValue(HCCL_SUCCESS));

    CommConfig commConfig("comm_ID");
    CommConfigInfo configInfo = { sizeof(CommConfigHandle), COMM_CONFIG_MAGIC_WORD, 1, { 0 } };
    CommConfigHandle configHandle = { configInfo, 300, 2, "comm_ID", "should_not_be_loaded", 0, 132, 4};

    HcclResult ret = commConfig.SetConfigByVersion(configHandle);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(commConfig.GetConfigBufferSize(), 300 * HCCL_CCL_COMM_FIXED_CALC_BUFFER_SIZE);
    EXPECT_EQ(commConfig.GetConfigDeterministic(), 2);
    EXPECT_EQ(commConfig.GetConfigCommName(), "comm_ID");
    GlobalMockObject::verify();
}

TEST_F(CommConfigTest, utCommConfig_deterministic_strcit_fail)
{
    MOCKER(GetExternalInputCCLBuffSize)
    .stubs()
    .will(returnValue(static_cast<u64>(200 * HCCL_CCL_COMM_FIXED_CALC_BUFFER_SIZE)));

    MOCKER(GetExternalInputHcclDeterministicV2).stubs().will(returnValue(0));

    // 确定性计算配置为规约保序仅支持A2场景
    DevType deviceType = DevType::DEV_TYPE_910_93;
    MOCKER(hrtGetDeviceType).stubs().with(outBound(deviceType)).will(returnValue(HCCL_SUCCESS));

    CommConfig commConfig("comm_ID");
    CommConfigInfo configInfo = { sizeof(CommConfigHandle), COMM_CONFIG_MAGIC_WORD, 1, { 0 } };
    CommConfigHandle configHandle = { configInfo, 300, 2, "comm_ID", "should_not_be_loaded", 0, 132, 4};

    HcclResult ret = commConfig.SetConfigByVersion(configHandle);
    EXPECT_EQ(ret, HCCL_E_PARA);
    GlobalMockObject::verify();
}

TEST_F(CommConfigTest, utCommConfig_exec_timeout)
{
    MOCKER(GetExternalInputCCLBuffSize)
    .stubs()
    .will(returnValue(static_cast<u64>(200 * HCCL_CCL_COMM_FIXED_CALC_BUFFER_SIZE)));
 
    MOCKER(GetExternalInputHcclDeterministic)
    .stubs()
    .will(returnValue(false));

    DevType deviceType = DevType::DEV_TYPE_910B;
    MOCKER(hrtGetDeviceType)
    .stubs()
    .with(outBound(deviceType))
    .will(returnValue(HCCL_SUCCESS));

    CommConfig commConfig("comm_ID");
    CommConfigInfo configInfo = { sizeof(CommConfigHandle), COMM_CONFIG_MAGIC_WORD, 11, { 0 } };
    CommConfigHandle configHandle = { configInfo, 300, 1, "comm_ID", "Unspecified", 3, 132, 4, 0, 0, 0, 1000};

    HcclResult ret = commConfig.SetConfigByVersion(configHandle);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(commConfig.GetConfigExecTimeOut(), 1000);
    EXPECT_EQ(configHandle.info.version, 11);

    configHandle.execTimeOut = 2000;
    ret = commConfig.SetConfigByVersion(configHandle);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(commConfig.GetConfigExecTimeOut(), 2000);

    GlobalMockObject::verify();
}

TEST_F(CommConfigTest, utCommConfig_hccl_algo)
{
    MOCKER(GetExternalInputCCLBuffSize)
    .stubs()
    .will(returnValue(static_cast<u64>(200 * HCCL_CCL_COMM_FIXED_CALC_BUFFER_SIZE)));
 
    MOCKER(GetExternalInputHcclDeterministic)
    .stubs()
    .will(returnValue(false));

    DevType deviceType = DevType::DEV_TYPE_910B;
    MOCKER(hrtGetDeviceType)
    .stubs()
    .with(outBound(deviceType))
    .will(returnValue(HCCL_SUCCESS));

    CommConfig commConfig("comm_ID");
    CommConfigInfo configInfo = { sizeof(CommConfigHandle), COMM_CONFIG_MAGIC_WORD, 11, { 0 } };
    CommConfigHandle configHandle = { configInfo, 300, 1, "comm_ID", "Unspecified", 3, 132, 4, 0, 0, 0, 1000,
        "level0:NA;level1:ring", "L1:1, L2:1", "MaxCnt:3, HoldTime:3000, IntervalTime:3000"};

    HcclResult ret = commConfig.SetConfigByVersion(configHandle);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(commConfig.GetConfigHcclAlgo()[0], HcclAlgoType::HCCL_ALGO_TYPE_NA);
    EXPECT_EQ(commConfig.GetConfigHcclAlgo()[1], HcclAlgoType::HCCL_ALGO_TYPE_RING);
    EXPECT_EQ(commConfig.GetConfigHcclAlgo()[2], HcclAlgoType::HCCL_ALGO_TYPE_DEFAULT);
    EXPECT_EQ(commConfig.GetConfigHcclAlgo()[3], HcclAlgoType::HCCL_ALGO_TYPE_DEFAULT);

    GlobalMockObject::verify();
}

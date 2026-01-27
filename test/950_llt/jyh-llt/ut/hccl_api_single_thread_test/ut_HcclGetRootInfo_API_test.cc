#include "hccl_api_base_test.h"

class HcclGetRootInfoTest : public BaseInit {
public:
    void SetUp() override {
        BaseInit::SetUp();
        setenv("HCCL_DEBUG_CONFIG", "alg", 1);
        UT_USE_RANK_TABLE_910_1SERVER_1RANK;
    }
    void TearDown() override {
        BaseInit::TearDown();
        GlobalMockObject::verify();
    }
};

TEST_F(HcclGetRootInfoTest, Ut_HcclGetRootInfo_When_RootInfoIsNull_Expect_ReturnIsHCCL_E_PTR)
{
    HcclResult ret = hrtSetDevice(0);
    EXPECT_EQ(ret,HCCL_SUCCESS);
    ret = HcclGetRootInfo(nullptr);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(HcclGetRootInfoTest, Ut_HcclGetRootInfo_When_NotSetEnv_Expect_ReturnIsHCCL_SUCCESS)
{
    unsetenv("HCCL_IF_IP");
    unsetenv("HCCL_WHITELIST_DISABLE");
    unsetenv("HCCL_WHITELIST_FILE");
    HcclResult ret = hrtSetDevice(0);
    EXPECT_EQ(ret,HCCL_SUCCESS);
    HcclRootInfo id;
    ret = HcclGetRootInfo(&id);
    EXPECT_EQ(strcmp(id.internal,"127.0.0.1"), 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(HcclGetRootInfoTest, Ut_HcclGetRootInfo_When_NotSetWHITELISTFILE_Expect_ReturnIsHCCL_E_PARA)
{
    const char* IpConfig = "127.0.0.2";
    setenv("HCCL_IF_IP", IpConfig, 1);
    setenv("HCCL_WHITELIST_DISABLE", "0", 1);
    unsetenv("HCCL_WHITELIST_FILE");
    HcclResult ret = hrtSetDevice(0);
    EXPECT_EQ(ret,HCCL_SUCCESS);
    HcclRootInfo id;
    ret = HcclGetRootInfo(&id);
    EXPECT_NE(strcmp(id.internal,IpConfig), 0);
    EXPECT_EQ(ret, HCCL_E_PARA);
    unsetenv("HCCL_WHITELIST_DISABLE");
    unsetenv("HCCL_IF_IP");
}

TEST_F(HcclGetRootInfoTest, Ut_HcclGetRootInfo_When_IPNotInWhiteList_Expect_IdNotEqWithHCCL_IF_IP)
{
    const char* IpConfig = "127.0.0.2";
    const char* WhitelistFilePath = "./whitelist.json";
    setenv("HCCL_IF_IP", IpConfig, 1);
    setenv("HCCL_WHITELIST_DISABLE", "0", 1);
    setenv("HCCL_WHITELIST_FILE", WhitelistFilePath, 1);

    nlohmann::json WhiteList = {
        {"host_ip", {"1.1.1.1"}},
        {"device_ip", {}}
    };
    std::ofstream outfile(WhitelistFilePath, std::ios::out | std::ios::trunc | std::ios::binary);
    if (outfile.is_open()) {
        outfile << std::setw(1) << WhiteList << std::endl;
        HCCL_INFO("open %s success", WhitelistFilePath);
    }
    else {
        HCCL_ERROR("open %s failed", WhitelistFilePath);
    }
    outfile.close();

    HcclResult ret = hrtSetDevice(0);
    EXPECT_EQ(ret,HCCL_SUCCESS);
    HcclRootInfo id;
    ret = HcclGetRootInfo(&id);
    EXPECT_NE(strcmp(id.internal,IpConfig), 0);
    EXPECT_EQ(ret, HCCL_E_NOT_FOUND);
    remove(WhitelistFilePath);
    unsetenv("HCCL_WHITELIST_FILE");
    unsetenv("HCCL_WHITELIST_DISABLE");
    unsetenv("HCCL_IF_IP");
}

TEST_F(HcclGetRootInfoTest, Ut_HcclGetRootInfo_When_IPInWhiteList_Expect_IdEqWithHCCL_IF_IP)
{
    const char* IpConfig = "127.0.0.2";
    const char* WhitelistFilePath = "./whitelist.json";
    setenv("HCCL_IF_IP", IpConfig, 1);
    setenv("HCCL_WHITELIST_DISABLE", "0", 1);
    setenv("HCCL_WHITELIST_FILE", WhitelistFilePath, 1);

    nlohmann::json WhiteList = {
        {"host_ip", {IpConfig}},
        {"device_ip", {}}
    };
    std::ofstream outfile(WhitelistFilePath, std::ios::out | std::ios::trunc | std::ios::binary);
    if (outfile.is_open()) {
        outfile << std::setw(1) << WhiteList << std::endl;
        HCCL_INFO("open %s success", WhitelistFilePath);
    }
    else {
        HCCL_ERROR("open %s failed", WhitelistFilePath);
    }
    outfile.close();

    HcclResult ret = hrtSetDevice(0);
    EXPECT_EQ(ret,HCCL_SUCCESS);
    HcclRootInfo id;
    ret = HcclGetRootInfo(&id);
    EXPECT_EQ(strcmp(id.internal,IpConfig), 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    remove(WhitelistFilePath);
    unsetenv("HCCL_WHITELIST_FILE");
    unsetenv("HCCL_WHITELIST_DISABLE");
    unsetenv("HCCL_IF_IP");
}

TEST_F(HcclGetRootInfoTest, Ut_HcclGetRootInfo_When_SetWhiteListDisable_Expect_IdEqWithHCCL_IF_IP)
{
    const char* IpConfig = "127.0.0.2";
    setenv("HCCL_IF_IP", IpConfig, 1);
    setenv("HCCL_WHITELIST_DISABLE", "1", 1);
    HcclResult ret = hrtSetDevice(0);
    EXPECT_EQ(ret,HCCL_SUCCESS);
    HcclRootInfo id;
    ret = HcclGetRootInfo(&id);
    EXPECT_EQ(strcmp(id.internal,IpConfig), 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    unsetenv("HCCL_WHITELIST_DISABLE");
    unsetenv("HCCL_IF_IP");
}

TEST_F(HcclGetRootInfoTest, Ut_HcclGetRootInfo_When_SetHOSTNAME_Expect_IdEqWithHCCL_SOCKET_IFNAME)
{
    MOCKER_CPP(&TopoInfoDetect::StartRootNetwork)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));
    const char* HostnameConfig = "=eth0";
    setenv("HCCL_SOCKET_IFNAME", HostnameConfig, 1);
    setenv("HCCL_WHITELIST_DISABLE", "1", 1);
    HcclResult ret = hrtSetDevice(0);
    EXPECT_EQ(ret,HCCL_SUCCESS);
    HcclRootInfo id;
    ret = HcclGetRootInfo(&id);
    EXPECT_EQ(strcmp(id.internal,"127.0.0.1"), 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    unsetenv("HCCL_WHITELIST_DISABLE");
    unsetenv("HCCL_SOCKET_IFNAME");
    unsetenv("HCCL_IF_IP");
}

TEST_F(HcclGetRootInfoTest, Ut_HcclGetRootInfo_When_SetIPAndHOSTNAME_Expect_IdEqWithHCCL_IF_IP)
{
    const char* IpConfig1 = "127.0.0.2";
    const char* HostnameConfig2 = "=eth2";
    setenv("HCCL_IF_IP", IpConfig1, 1);
    setenv("HCCL_SOCKET_IFNAME", HostnameConfig2, 1);
    setenv("HCCL_WHITELIST_DISABLE", "1", 1);
    HcclResult ret = hrtSetDevice(0);
    EXPECT_EQ(ret,HCCL_SUCCESS);
    HcclRootInfo id;
    ret = HcclGetRootInfo(&id);
    EXPECT_EQ(strcmp(id.internal,IpConfig1), 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    unsetenv("HCCL_WHITELIST_DISABLE");
    unsetenv("HCCL_SOCKET_IFNAME");
    unsetenv("HCCL_IF_IP");
}

TEST_F(HcclGetRootInfoTest, Ut_HcclGetRootInfo_When_HOSTNAME_Expect_IdEqWithFirstIP)
{
    const char* HostnameConfig = "eth";
    unsetenv("HCCL_IF_IP");
    setenv("HCCL_SOCKET_IFNAME", HostnameConfig, 1);
    setenv("HCCL_WHITELIST_DISABLE", "1", 1);
    HcclResult ret = hrtSetDevice(0);
    EXPECT_EQ(ret,HCCL_SUCCESS);
    HcclRootInfo id;
    ret = HcclGetRootInfo(&id);
    EXPECT_EQ(strcmp(id.internal,"127.0.0.1"), 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    unsetenv("HCCL_WHITELIST_DISABLE");
    unsetenv("HCCL_SOCKET_IFNAME");
}
#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <cmath>
#include <nlohmann/json.hpp>
#include <iostream>
#include <fstream>

#define private public
#define protected public
#include "multi_qpInfo_manager.h"
#include "multi_qpInfo_manager.h"
#include "hccl_nslbdp.h"
#include "externalinput.h"
#include "dlra_function.h"
#undef private
#undef protected
#include "llt_hccl_stub_mc2.h"
#include "llt_hccl_stub.h"
#include "hccl_network.h"
using namespace std;

class MulTiQpInfoUt : public testing::Test {
protected:
    static void SetUpTestCase()
    {}
    static void TearDownTestCase()
    {
        std::cout << "MulTiQpInfoUt Test TearDownTestCase" << std::endl;
    }
    virtual void SetUp()
    {
        std::cout << "MulTiQpInfoUt Test SetUp" << std::endl;
        u32 getHccnCfgVersion = 1;
        MOCKER(hrtRaGetInterfaceVersion).stubs().with(any(), any(), outBoundP(&getHccnCfgVersion)).will(returnValue(0));
        DlRaFunction::GetInstance().DlRaFunctionInit();
        MOCKER(GetWorkflowMode).stubs().will(returnValue(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE));
        
    }
    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "MulTiQpInfoUt Test TearDown" << std::endl;
    }
};

HcclResult HrtRaGetHccnCfgDevNotConfig(s32 networkMode, u32 devicePhyId, enum HccnCfgKeyT key, std::string &value)
{
    value = "";
    return HcclResult::HCCL_SUCCESS;
}

TEST_F(MulTiQpInfoUt, Ut_Qp2048)
{
    hccl::MulQpInfo mulQpInfoManager;
    hccl::InitParams initParams(
    NICDeployment::NIC_DEPLOYMENT_HOST, 0, DevType::DEV_TYPE_910_93);
    mulQpInfoManager.Init(initParams);
    EXPECT_NE(mulQpInfoManager.Init(initParams), HcclResult::HCCL_SUCCESS);
}

TEST_F(MulTiQpInfoUt, Ut_QpFileNotExist)
{
    s32 orion_level = log_level_get_stub();
    log_level_set_stub(2); 
    unsigned int len = 0;
    MOCKER(RaGetHccnCfg).stubs().with(any(), any(), any(), outBoundP(&len)).will(returnValue(0));
    hccl::MulQpInfo mulQpInfoManager;
    hccl::InitParams initParams(
    NICDeployment::NIC_DEPLOYMENT_HOST, 0, DevType::DEV_TYPE_910_93);
    mulQpInfoManager.Init(initParams);
    EXPECT_EQ(mulQpInfoManager.Init(initParams), HcclResult::HCCL_SUCCESS);
    log_level_set_stub(orion_level); 
}

TEST_F(MulTiQpInfoUt, Ut_SingleQp)
{
    MOCKER(HrtRaGetHccnCfg).stubs().will(invoke(HrtRaGetHccnCfgDevNotConfig));
    hccl::MulQpInfo mulQpInfoManager;
    hccl::InitParams initParams(
        NICDeployment::NIC_DEPLOYMENT_HOST, 0, DevType::DEV_TYPE_910_93);
    EXPECT_EQ(mulQpInfoManager.Init(initParams), HcclResult::HCCL_SUCCESS);
    bool isEnableMulQp = true;
    EXPECT_EQ(mulQpInfoManager.IsEnableMulQp(isEnableMulQp), HcclResult::HCCL_SUCCESS);
    EXPECT_FALSE(isEnableMulQp);
    EXPECT_TRUE(mulQpInfoManager.config_ == nullptr);
    EXPECT_EQ(mulQpInfoManager.initStatus_, HcclResult::HCCL_SUCCESS);
}

HcclResult HrtRaGetHccnCfgDevMulQp(s32 networkMode, u32 devicePhyId, enum HccnCfgKeyT key, std::string &value)
{
    if (key == HccnCfgKeyT::HCCN_UDP_PORT_MODE) {
        value = "multi_qp";
        return HcclResult::HCCL_SUCCESS;
    } else if (key == HccnCfgKeyT::HCCN_MULTI_QP_COUNT) {
        value = "32";
    } else if (key == HccnCfgKeyT::HCCN_MULTI_QP_UDP_PORTS) {
        value = "1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32";
    }
    return HcclResult::HCCL_SUCCESS;
}
TEST_F(MulTiQpInfoUt, Ut_DEVMulQp)
{
    MOCKER(HrtRaGetHccnCfg).stubs().will(invoke(HrtRaGetHccnCfgDevMulQp));
    hccl::MulQpInfo mulQpInfoManager;
    hccl::InitParams initParams(
        NICDeployment::NIC_DEPLOYMENT_HOST, 0, DevType::DEV_TYPE_910_93);
    EXPECT_EQ(mulQpInfoManager.Init(initParams), HcclResult::HCCL_SUCCESS);
    bool isEnableMulQp = false;
    EXPECT_EQ(mulQpInfoManager.IsEnableMulQp(isEnableMulQp), HcclResult::HCCL_SUCCESS);
    EXPECT_TRUE(isEnableMulQp);

    MUL_QP_FROM mulQpFrom;
    EXPECT_EQ(mulQpInfoManager.GetMulQpFromType(mulQpFrom), HcclResult::HCCL_SUCCESS);
    EXPECT_TRUE(mulQpFrom == MUL_QP_FROM::MUL_QP_FROM_DEV_CFG);

    PortNum portNum = 0;
    EXPECT_EQ(mulQpInfoManager.GetPortsNumByIpPair(portNum), HcclResult::HCCL_SUCCESS);
    EXPECT_TRUE(portNum == 32);

    MulQpSourcePorts ports;
    EXPECT_EQ(mulQpInfoManager.GetSpecialSourcePortsByIpPair(ports), HcclResult::HCCL_SUCCESS);
    EXPECT_TRUE(ports.size() == 32);

    for (std::uint16_t i = 1; i <= 32; ++i) {
        EXPECT_TRUE(ports[i - 1] == i);
    }
}

HcclResult HrtRaGetHccnNSLBMulQp(s32 networkMode, u32 devicePhyId, enum HccnCfgKeyT key, std::string &value)
{
    if (key == HccnCfgKeyT::HCCN_UDP_PORT_MODE) {
        value = "nslb_dp";
        return HcclResult::HCCL_SUCCESS;
    }
    value = "";
    return HcclResult::HCCL_SUCCESS;
}
TEST_F(MulTiQpInfoUt, Ut_DEVNSLBMulQp)
{
    MOCKER(HrtRaGetHccnCfg).stubs().will(invoke(HrtRaGetHccnNSLBMulQp));
    hccl::MulQpInfo mulQpInfoManager;
    hccl::InitParams initParams(
        NICDeployment::NIC_DEPLOYMENT_HOST, 0, DevType::DEV_TYPE_910_93);
    EXPECT_EQ(mulQpInfoManager.Init(initParams), HcclResult::HCCL_SUCCESS);
    bool isEnableMulQp = false;
    EXPECT_EQ(mulQpInfoManager.IsEnableMulQp(isEnableMulQp), HcclResult::HCCL_SUCCESS);
    EXPECT_TRUE(isEnableMulQp);

    MUL_QP_FROM mulQpFrom;
    EXPECT_EQ(mulQpInfoManager.GetMulQpFromType(mulQpFrom), HcclResult::HCCL_SUCCESS);
    EXPECT_TRUE(mulQpFrom == MUL_QP_FROM::MUL_QP_FROM_DEV_NSLB);

    PortNum portNum = 0;
    EXPECT_EQ(mulQpInfoManager.GetPortsNumByIpPair(portNum), HcclResult::HCCL_SUCCESS);
    EXPECT_TRUE(portNum == 1);

    MulQpSourcePorts ports;
    EXPECT_EQ(mulQpInfoManager.GetSpecialSourcePortsByIpPair(ports), HcclResult::HCCL_SUCCESS);
    EXPECT_TRUE(ports.size() == 1);
    EXPECT_TRUE(ports[0] == static_cast<std::uint16_t>(hcclNslbDp::GetInstance().Getl4SPortId()));
}

TEST_F(MulTiQpInfoUt, Ut_ConfigPath)
{
    HcclResult ret = HCCL_SUCCESS;
    setenv("HCCL_RDMA_QP_PORT_CONFIG_PATH", "/tmp/", 1);
    ret = InitEnvVarParam();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    std::string filePath = GetExternalInputQpSrcPortConfigPath();
    std::string fileStr = filePath + "/MultiQpSrcPort.cfg";
    const int FILE_AUTHORITY = 0600;
    int fd = open(fileStr.c_str(), O_WRONLY | O_CREAT | O_TRUNC, FILE_AUTHORITY);
    if (fd < 0) {
        HCCL_ERROR("Fail to open the file: %s.", fileStr.c_str(), HCCL_E_PARA);
    }
    if (close(fd) != 0) {
        HCCL_ERROR("Fail to close the file: %s.", fileStr.c_str(), HCCL_E_PARA);
    }
    std::ofstream fileStream(fileStr.c_str(), std::ios::out | std::ios::binary);
    if (fileStream.is_open()) {
        fileStream << "192.2.100.2"
                   << ","
                   << "0.0.0.0"
                   << "="
                   << "61000"
                   << ","
                   << "61001"
                   << ","
                   << "61002" << std::endl;
        fileStream << "0.0.0.0"
                   << ","
                   << "192.2.100.1"
                   << "="
                   << "61000" << std::endl;
        fileStream << "192.2.100.2"
                   << ","
                   << "192.2.100.1"
                   << "="
                   << "61100"
                   << ","
                   << "61101"
                   << ","
                   << "61102"
                   << ","
                   << "61103"
                   << ","
                   << "61104"
                   << ","
                   << "61105"
                   << ","
                   << "61106"
                   << ","
                   << "61107"
                   << ","
                   << "61108"
                   << ","
                   << "61109"
                   << ","
                   << "61110"
                   << ","
                   << "61111"
                   << ","
                   << "61112"
                   << ","
                   << "61113"
                   << ","
                   << "61114"
                   << ","
                   << "61115"
                   << ","
                   << "61116"
                   << ","
                   << "61117"
                   << ","
                   << "61118"
                   << ","
                   << "61119"
                   << ","
                   << "61120"
                   << ","
                   << "61121"
                   << ","
                   << "61122"
                   << ","
                   << "61123"
                   << ","
                   << "61124"
                   << ","
                   << "61125"
                   << ","
                   << "61126"
                   << ","
                   << "61127"
                   << ","
                   << "61128"
                   << ","
                   << "61129"
                   << ","
                   << "61130"
                   << ","
                   << "61131";
        fileStream.close();
    } else {
        HCCL_ERROR("[Initialize][GraphOptimizer]file %s open failed!", fileStr.c_str());
    }

    MOCKER(HrtRaGetHccnCfg).stubs().will(invoke(HrtRaGetHccnCfgDevNotConfig));
    hccl::MulQpInfo mulQpInfoManager;
    hccl::InitParams initParams(
        NICDeployment::NIC_DEPLOYMENT_HOST, 0, DevType::DEV_TYPE_910_93);
    EXPECT_EQ(mulQpInfoManager.Init(initParams), HcclResult::HCCL_SUCCESS);
    bool isEnableMulQp = false;
    EXPECT_EQ(mulQpInfoManager.IsEnableMulQp(isEnableMulQp), HcclResult::HCCL_SUCCESS);
    EXPECT_TRUE(isEnableMulQp);

    MUL_QP_FROM mulQpFrom;
    EXPECT_EQ(mulQpInfoManager.GetMulQpFromType(mulQpFrom), HcclResult::HCCL_SUCCESS);
    EXPECT_TRUE(mulQpFrom == MUL_QP_FROM::MUL_QP_FROM_ENV_PORT_CONFIG_PATH);

    PortNum portNum = 0;
    MulQpSourcePorts ports;
    auto keyPair = KeyPair{HcclIpAddress("1.2.3.4"), HcclIpAddress("2.2.3.4")};
    EXPECT_EQ(
        mulQpInfoManager.GetPortsNumByIpPair(portNum, keyPair),
        HcclResult::HCCL_SUCCESS);
    EXPECT_TRUE(portNum == 0);
    EXPECT_EQ(mulQpInfoManager.GetSpecialSourcePortsByIpPair(ports, keyPair), HcclResult::HCCL_SUCCESS);
    EXPECT_TRUE(ports.size() == 0);

    auto keyPair0 = KeyPair{HcclIpAddress("192.2.100.2"), HcclIpAddress("2.2.3.4")};
    EXPECT_EQ(mulQpInfoManager.GetPortsNumByIpPair(portNum, keyPair0), HcclResult::HCCL_SUCCESS);
    EXPECT_TRUE(portNum == 3);

    EXPECT_EQ(mulQpInfoManager.GetSpecialSourcePortsByIpPair(ports, keyPair0), HcclResult::HCCL_SUCCESS);
    EXPECT_TRUE(ports.size() == 3);
    EXPECT_TRUE(ports[0] == 61000 && ports[1] == 61001 && ports[2] == 61002);

    auto keyPair1 = KeyPair{HcclIpAddress("2.2.3.4"), HcclIpAddress("192.2.100.1")};
    EXPECT_EQ(mulQpInfoManager.GetPortsNumByIpPair(portNum, keyPair1), HcclResult::HCCL_SUCCESS);
    EXPECT_TRUE(portNum == 1);

    EXPECT_EQ(mulQpInfoManager.GetSpecialSourcePortsByIpPair(ports, keyPair1), HcclResult::HCCL_SUCCESS);
    EXPECT_TRUE(ports.size() == 1);
    EXPECT_TRUE(ports[0] == 61000);

    auto keyPair2 = KeyPair{HcclIpAddress("192.2.100.2"), HcclIpAddress("192.2.100.1")};
    EXPECT_EQ(mulQpInfoManager.GetPortsNumByIpPair(portNum, keyPair2), HcclResult::HCCL_SUCCESS);
    EXPECT_TRUE(portNum == 32) << portNum;
    EXPECT_EQ(mulQpInfoManager.GetSpecialSourcePortsByIpPair(ports, keyPair2), HcclResult::HCCL_SUCCESS);
    EXPECT_TRUE(ports.size() == 32);
    for (std::uint16_t i = 0; i < 32; ++i) {
        EXPECT_TRUE(ports[i] == 61100 + i);
    }

    EXPECT_EQ(mulQpInfoManager.GetPortsNumByIpPair(portNum, KeyPair{}), HcclResult::HCCL_SUCCESS);
    EXPECT_TRUE(portNum == 0) << portNum;
    EXPECT_EQ(mulQpInfoManager.GetSpecialSourcePortsByIpPair(ports, KeyPair{}), HcclResult::HCCL_SUCCESS);
    EXPECT_TRUE(ports.empty()) << ports.size();

    unsetenv("HCCL_RDMA_QP_PORT_CONFIG_PATH");
    ResetInitState();
}

TEST_F(MulTiQpInfoUt, Ut_perconnection)
{
    HcclResult ret = HCCL_SUCCESS;
    setenv("HCCL_RDMA_QPS_PER_CONNECTION", "1", 1);
    ret = InitEnvVarParam();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    MOCKER(HrtRaGetHccnCfg).stubs().will(invoke(HrtRaGetHccnCfgDevNotConfig));
    hccl::MulQpInfo mulQpInfoManager;
    hccl::InitParams initParams(
        NICDeployment::NIC_DEPLOYMENT_HOST, 0, DevType::DEV_TYPE_910_93);
    EXPECT_EQ(mulQpInfoManager.Init(initParams), HcclResult::HCCL_SUCCESS);
    bool isEnableMulQp = false;
    EXPECT_EQ(mulQpInfoManager.IsEnableMulQp(isEnableMulQp), HcclResult::HCCL_SUCCESS);
    EXPECT_TRUE(isEnableMulQp);

    MUL_QP_FROM mulQpFrom;
    EXPECT_EQ(mulQpInfoManager.GetMulQpFromType(mulQpFrom), HcclResult::HCCL_SUCCESS);
    EXPECT_TRUE(mulQpFrom == MUL_QP_FROM::MUL_QP_FROM_ENV_PER_CONNECTION);

    PortNum portNum = 0;
    MulQpSourcePorts ports;
    EXPECT_EQ(
        mulQpInfoManager.GetPortsNumByIpPair(portNum),
        HcclResult::HCCL_SUCCESS);
    EXPECT_TRUE(portNum == 1);
    EXPECT_EQ(mulQpInfoManager.GetSpecialSourcePortsByIpPair(ports), HcclResult::HCCL_SUCCESS);
    EXPECT_TRUE(ports.size() == 1);
    EXPECT_TRUE(ports[0] == 0);
    unsetenv("HCCL_RDMA_QPS_PER_CONNECTION");
    ResetInitState();
}
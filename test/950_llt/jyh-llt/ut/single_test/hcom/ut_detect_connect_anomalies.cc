#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include<sys/time.h>
#define private public
#define protected public
#include "detect_connect_anomalies.h"
#undef private
#include "llt_hccl_stub_pub.h"
#include "llt_hccl_stub_gdr.h"
#include "hccl_comm_pub.h"
#include "network_manager_pub.h"
#include "transport_ibverbs.h"
#include "externalinput.h"
#include "dispatcher_pub.h"
#include "opexecounter_pub.h"
#include "dlra_function.h"

#include "hccl_network.h"
#include "env_config.h"
using namespace std;
using namespace hccl;



class DetectConnectionAnomaliesCheck : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "\033[36m--HcomParamCheck SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "\033[36m--HcomParamCheck TearDown--\033[0m" << std::endl;
    }
    virtual void SetUp()
    {
        s32 portNum = 7;
        setenv("HCCL_DFS_CONFIG", "connection_fault_detection_time:20", 1);
        std::cout << "A Test SetUP" << std::endl;
        MOCKER(GetExternalInputDfsConnectionFaultDetectionTime)
        .stubs()
        .will(returnValue(1));
    }
    virtual void TearDown()
    {
        unsetenv("HCCL_DFS_CONFIG");
        std::cout << "A Test TearDown" << std::endl;
        GlobalMockObject::verify();
    }
};

TEST_F(DetectConnectionAnomaliesCheck, addIpQueue1)
{
    RankInfo localRankInfo;
    RankInfo remoteRankInfo;
    NicType nicType = NicType::VNIC_TYPE;
    localRankInfo.deviceType = DevType::DEV_TYPE_910B;
    HcclIpAddress localIp("127.0.0.1");
    HcclIpAddress remoteIpIp("127.0.0.2");
    localRankInfo.deviceVnicIp = localIp;
    remoteRankInfo.deviceVnicIp = remoteIpIp;
    remoteRankInfo.nicIp.push_back(remoteIpIp);
    localRankInfo.nicIp.push_back(localIp);
    MOCKER(GetExternalInputDfsConnectionFaultDetectionTime)
    .stubs()
    .will(returnValue(1));
    DetectConnectionAnomalies::GetInstance(0).broadCastTime = 1;
    DetectConnectionAnomalies::GetInstance(0).AddIpQueue(localRankInfo, remoteRankInfo, nicType, 0);
    DetectConnectionAnomalies::GetInstance(0).Deinit();

    nicType = NicType::DEVICE_NIC_TYPE;
    localRankInfo.nicIp.push_back(localIp);
    remoteRankInfo.nicIp.push_back(remoteIpIp);
    DetectConnectionAnomalies::GetInstance(0).AddIpQueue(localRankInfo, remoteRankInfo, nicType, 0);
    DetectConnectionAnomalies::GetInstance(0).Deinit();
	GlobalMockObject::verify();
}

TEST_F(DetectConnectionAnomaliesCheck, addIpQueue2)
{
    RankInfo localRankInfo;
    RankInfo remoteRankInfo;
    NicType nicType = NicType::DEVICE_NIC_TYPE;
    localRankInfo.deviceType = DevType::DEV_TYPE_910B;
    HcclIpAddress localIp("127.0.0.1");
    HcclIpAddress remoteIpIp("127.0.0.1");
    localRankInfo.nicIp.push_back(localIp);
    remoteRankInfo.nicIp.push_back(remoteIpIp);

    localRankInfo.deviceVnicIp = localIp;
    remoteRankInfo.deviceVnicIp = remoteIpIp;
    MOCKER(GetExternalInputDfsConnectionFaultDetectionTime)
    .stubs()
    .will(returnValue(1));
    DetectConnectionAnomalies::GetInstance(1).broadCastTime = 1;
    DetectConnectionAnomalies::GetInstance(1).AddIpQueue(localRankInfo, remoteRankInfo, nicType, 1);
    DetectConnectionAnomalies::GetInstance(1).Deinit();
    GlobalMockObject::verify();
}

TEST_F(DetectConnectionAnomaliesCheck, GetStatus)
{
    MOCKER_CPP(&HcclSocket::GetStatus).stubs().with(any()).will(returnValue(HcclSocketStatus::SOCKET_OK));
    HcclIpAddress ipAddr("192.168.1.1");
    HcclIpAddress localIpAddr("192.168.1.2");
    std::shared_ptr<HcclSocket> newSocket(new (std::nothrow)HcclSocket("detect",
        nullptr, localIpAddr, 0, HcclSocketRole::SOCKET_ROLE_SERVER));
    ErrInfo errInfo;

    errInfo.localRankInfo.superPodId = 1;
    errInfo.localRankInfo.devicePhyId = 0;
    errInfo.localRankInfo.serverId = "111";
    errInfo.remoteRankInfo.serverId = "222";
    errInfo.remoteRankInfo.superPodId = 1;
    errInfo.remoteRankInfo.devicePhyId = 1;
    MOCKER(GetExternalInputDfsConnectionFaultDetectionTime)
    .stubs()
    .will(returnValue(1));
    HcclResult ret = DetectConnectionAnomalies::GetInstance(2).GetStatus(errInfo, newSocket);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    DetectConnectionAnomalies::GetInstance(2).Deinit();
    GlobalMockObject::verify();
    MOCKER(GetExternalInputDfsConnectionFaultDetectionTime)
    .stubs()
    .will(returnValue(1));
    MOCKER_CPP(&HcclSocket::GetStatus).stubs().with(any()).will(returnValue(HcclSocketStatus::SOCKET_ERROR));
    ret = DetectConnectionAnomalies::GetInstance(2).GetStatus(errInfo, newSocket);
    EXPECT_EQ(ret, HCCL_E_TIMEOUT);
    DetectConnectionAnomalies::GetInstance(2).Deinit();
    GlobalMockObject::verify();
}

TEST_F(DetectConnectionAnomaliesCheck, ConstructErrorInfo)
{
    HcclIpAddress ipAddr("192.168.1.1");
    HcclIpAddress localIpAddr("192.168.1.2");
    RankInfo localRankInfo;
    RankInfo remoteRankInfo;
    localRankInfo.devicePhyId = 0;
    localRankInfo.superPodId = "0";
    remoteRankInfo.devicePhyId = 1;
    remoteRankInfo.superPodId = "0";
    localRankInfo.serverId = "111";
    remoteRankInfo.serverId = "222";

    std::shared_ptr<HcclSocket> newSocket(new (std::nothrow)HcclSocket("detect",
        nullptr, localIpAddr, 0, HcclSocketRole::SOCKET_ROLE_SERVER));

    HcclResult ret = DetectConnectionAnomalies::GetInstance(3).ConstructErrorInfo(newSocket, localRankInfo, remoteRankInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(DetectConnectionAnomaliesCheck, CreateDetectNicLinks)
{
 
    HcclIpAddress ipAddr("192.168.1.1");
    HcclIpAddress localIpAddr("192.168.1.2");
    std::shared_ptr<HcclSocket> newSocket(new (std::nothrow)HcclSocket("detect",
        nullptr, localIpAddr, 0, HcclSocketRole::SOCKET_ROLE_SERVER));
 
    NicType nicType = NicType::DEVICE_NIC_TYPE;
    DetectConnectionAnomalies::GetInstance(7).broadCastTime = 1;
    MOCKER_CPP(&HcclSocket::Accept)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_E_TIMEOUT));
    ErrInfo errInfo;
    errInfo.localRankInfo.devicePhyId = 0;
    errInfo.deviceLogicId = 0;
    errInfo.localRankInfo.nicIp.push_back(localIpAddr);
    errInfo.localRankInfo.deviceNicPort = 16677;
    errInfo.remoteRankInfo.nicIp.push_back(ipAddr);
    MOCKER(GetExternalInputDfsConnectionFaultDetectionTime)
    .stubs()
    .will(returnValue(1));
    HcclResult ret = DetectConnectionAnomalies::GetInstance(7).CreateDetectNicLinks(errInfo);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);
    DetectConnectionAnomalies::GetInstance(7).Deinit();


    MOCKER_CPP(&HcclSocket::Listen, HcclResult(HcclSocket::*)())
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclSocket::Send, HcclResult(HcclSocket::*)(const void *, u64))
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    DetectInfo info;
    SendInfo sendInfo;
    DetectConnectionAnomalies::GetInstance(0).recvErrorInfoMap_["127.0.0.2"] = info;
    DetectConnectionAnomalies::GetInstance(0).sendErrorInfoMap_["127.0.0.2"] = sendInfo;
    DetectConnectionAnomalies::GetInstance(0).listenNicVec_.push_back(newSocket);
    DetectConnectionAnomalies::GetInstance(0).threadExit_ = true;
    ret = DetectConnectionAnomalies::GetInstance(0).CreateDetectNicLinks(errInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    DetectConnectionAnomalies::GetInstance(0).Deinit();

    GlobalMockObject::verify();
}
 
TEST_F(DetectConnectionAnomaliesCheck, CreateDetectVnicLinks)
{
    HcclIpAddress ipAddr("192.168.1.1");
    HcclIpAddress localIpAddr("192.168.1.2");
    std::shared_ptr<HcclSocket> newSocket(new (std::nothrow)HcclSocket("detect",
        nullptr, localIpAddr, 0, HcclSocketRole::SOCKET_ROLE_SERVER));

    NicType nicType = NicType::VNIC_TYPE;
    DetectConnectionAnomalies::GetInstance(4).threadExit_ = true;

    MOCKER_CPP(&HcclSocket::Listen, HcclResult(HcclSocket::*)())
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclSocket::Accept)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_E_TIMEOUT));
    MOCKER_CPP(&HcclSocket::Send, HcclResult(HcclSocket::*)(const void *, u64))
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    ErrInfo errInfo;
    errInfo.localRankInfo.devicePhyId = 1;
    errInfo.deviceLogicId = 1;
    errInfo.localRankInfo.nicIp.push_back(localIpAddr);
    errInfo.localRankInfo.deviceNicPort = 16677;
    errInfo.remoteRankInfo.nicIp.push_back(ipAddr);

    DetectInfo info;
    SendInfo sendInfo;
    DetectConnectionAnomalies::GetInstance(1).recvErrorInfoMap_["127.0.0.1"] = info;
    DetectConnectionAnomalies::GetInstance(1).sendErrorInfoMap_["127.0.0.1"] = sendInfo;
    DetectConnectionAnomalies::GetInstance(1).listenVnicVec_.push_back(newSocket);
    DetectConnectionAnomalies::GetInstance(1).broadCastTime = 1;


    MOCKER_CPP(&DetectConnectionAnomalies::AddWhiteList)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    HcclResult ret = DetectConnectionAnomalies::GetInstance(4).CreateDetectVnicLinks(errInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    DetectConnectionAnomalies::GetInstance(4).Deinit();
    GlobalMockObject::verify();
}

TEST_F(DetectConnectionAnomaliesCheck, addIpQueue3)
{
    RankInfo localRankInfo;
    RankInfo remoteRankInfo;
    NicType nicType = NicType::VNIC_TYPE;
    localRankInfo.deviceType = DevType::DEV_TYPE_910B;
    HcclIpAddress localIp("127.0.0.1");
    HcclIpAddress remoteIpIp("127.0.0.2");
    localRankInfo.deviceVnicIp = localIp;
    remoteRankInfo.deviceVnicIp = remoteIpIp;
    localRankInfo.nicIp.push_back(localIp);
    setenv("HCCL_DFS_CONFIG", "connection_fault_detection_time:0", 1);
    DetectConnectionAnomalies::GetInstance(5).broadCastTime = 1;
    DetectConnectionAnomalies::GetInstance(5).AddIpQueue(localRankInfo, remoteRankInfo, nicType, 0);
    DetectConnectionAnomalies::GetInstance(5).Deinit();
    unsetenv("HCCL_DFS_CONFIG");
    DetectConnectionAnomalies::GetInstance(33).AddIpQueue(localRankInfo, remoteRankInfo, nicType, 1);
    DetectConnectionAnomalies::GetInstance(33).Deinit();
}

TEST_F(DetectConnectionAnomaliesCheck, addIpQueue4)
{
    RankInfo localRankInfo;
    RankInfo remoteRankInfo;
    NicType nicType = NicType::DEVICE_NIC_TYPE;
    localRankInfo.deviceType = DevType::DEV_TYPE_910B;
    HcclIpAddress localIp("127.0.0.6");
    HcclIpAddress remoteIpIp("127.0.0.7");
    localRankInfo.nicIp.push_back(localIp);
    remoteRankInfo.nicIp.push_back(remoteIpIp);

    std::shared_ptr<HcclSocket> newSocket(new (std::nothrow)HcclSocket("detect",
        nullptr, localIp, 0, HcclSocketRole::SOCKET_ROLE_SERVER));


    DetectConnectionAnomalies::GetInstance(6).threadExit_ = true;
    DetectConnectionAnomalies::GetInstance(6).broadCastTime = 1;
    DetectConnectionAnomalies::GetInstance(2).ipNictypeQueue_;
    ErrInfo errInfo;

    errInfo.localRankInfo = localRankInfo;
    errInfo.remoteRankInfo = remoteRankInfo;

    localRankInfo.devicePhyId = 0;
    errInfo.deviceLogicId = 0;
    errInfo.localRankInfo.nicIp.push_back(localIp);
    errInfo.localRankInfo.deviceNicPort = 16677;
    errInfo.remoteRankInfo.nicIp.push_back(remoteIpIp);
    DetectConnectionAnomalies::GetInstance(2).AddIpQueue(localRankInfo, remoteRankInfo, nicType, 2);
    DetectConnectionAnomalies::GetInstance(2).isNeedNic_ = true;
    HcclResult  ret = DetectConnectionAnomalies::GetInstance(2).GetIpQueue();
    DetectConnectionAnomalies::GetInstance(2).Deinit();

}

TEST_F(DetectConnectionAnomaliesCheck, AddWhiteList)
{
    MOCKER_CPP(&HcclSocket::AddWhiteList).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    HcclIpAddress ipAddr("192.168.1.1");
    HcclIpAddress localIpAddr("192.168.1.2");
    NicType nicType = NicType::DEVICE_NIC_TYPE;
    std::string tag = "detect_0";

    std::shared_ptr<HcclSocket> newSocket(new (std::nothrow)HcclSocket("detect",
        nullptr, localIpAddr, 0, HcclSocketRole::SOCKET_ROLE_SERVER));

    DetectConnectionAnomalies::GetInstance(8).isNeedNic_ = true;
    DetectConnectionAnomalies::GetInstance(8).uniqueIps_.insert(ipAddr);
    HcclResult ret = DetectConnectionAnomalies::GetInstance(8).AddWhiteList(newSocket, nicType, tag);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    nicType = NicType::VNIC_TYPE;
    ret = DetectConnectionAnomalies::GetInstance(8).AddWhiteList(newSocket, nicType, tag);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    DetectConnectionAnomalies::GetInstance(8).Deinit();

    GlobalMockObject::verify();
}

TEST_F(DetectConnectionAnomaliesCheck, Connect_test)
{
    MOCKER_CPP(&HcclSocket::Connect)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&NetworkManager::StartHostNet)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS)); 

    MOCKER_CPP(&NetworkManager::StopHostNet)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS)); 

    MOCKER_CPP(&DetectConnectionAnomalies::GetStatus)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_E_TCP_CONNECT));

    struct ErrInfo errInfo;
    errInfo.nicType = NicType::VNIC_TYPE;
    errInfo.localRankInfo.devicePhyId = 0;
    errInfo.deviceLogicId = 0;

    HcclIpAddress ipAddr("192.168.1.3");
    HcclIpAddress localIpAddr("192.168.1.4");
    std::shared_ptr<HcclSocket> newSocket(new (std::nothrow)HcclSocket("detect",
        nullptr, localIpAddr, 0, HcclSocketRole::SOCKET_ROLE_SERVER));

    HcclResult ret = DetectConnectionAnomalies::GetInstance(9).CreateClient(errInfo);
    EXPECT_EQ(ret, HCCL_E_TCP_CONNECT);
    DetectConnectionAnomalies::GetInstance(9).Deinit();
    GlobalMockObject::verify();
}


TEST_F(DetectConnectionAnomaliesCheck, Connect_test_1)
{
    MOCKER_CPP(&HcclSocket::Connect)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&NetworkManager::StartHostNet)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS)); 

    MOCKER_CPP(&NetworkManager::StopHostNet)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS)); 

    MOCKER_CPP(&DetectConnectionAnomalies::GetStatus)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    struct ErrInfo errInfo;
    errInfo.nicType = NicType::VNIC_TYPE;
    errInfo.localRankInfo.devicePhyId = 0;
    errInfo.deviceLogicId = 0;

    HcclIpAddress ipAddr("192.168.1.3");
    HcclIpAddress localIpAddr("192.168.1.4");
    std::shared_ptr<HcclSocket> newSocket(new (std::nothrow)HcclSocket("detect",
        nullptr, localIpAddr, 0, HcclSocketRole::SOCKET_ROLE_SERVER));

    DetectConnectionAnomalies::GetInstance(0).threadExit_ = true;
    HcclResult ret = DetectConnectionAnomalies::GetInstance(0).CreateClient(errInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    DetectConnectionAnomalies::GetInstance(0).Deinit();
    GlobalMockObject::verify();
}

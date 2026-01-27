#include "gtest/gtest.h"
#include <mockcpp/mokc.h>
#include <mockcpp/mockcpp.hpp>
#include "conn_local_cnt_notify_manager.h"
#include "null_ptr_exception.h"
#define private public
#include "communicator_impl.h"
#undef private
using namespace Hccl;

class ConnLocalCntNotifyManagerTest : public testing::Test {
protected:
    static void SetUpTestCase() {
        std::cout << "ConnLocalCntNotifyManagerTest SetUP" << std::endl;
    }

    static void TearDownTestCase() {
        std::cout << "ConnLocalCntNotifyManagerTest TearDown" << std::endl;
    }

    virtual void SetUp() {
        std::cout << "A Test case in ConnLocalCntNotifyManagerTest SetUP" << std::endl;
    }

    virtual void TearDown () {
        GlobalMockObject::verify();
        std::cout << "A Test case in ConnLocalCntNotifyManagerTest TearDown" << std::endl;
    }
};

TEST_F(ConnLocalCntNotifyManagerTest, applyfor_and_get_success)
{
    MOCKER(HrtGetDeviceType).stubs().will(returnValue(DevType(DevType::DEV_TYPE_910A2)));

    CommunicatorImpl comm;
    comm.devPhyId = 0;
    ConnLocalCntNotifyManager connLocalCntNotifyManager(&comm);
    vector<LinkData> links;
    LinkData link(BasePortType(PortDeploymentType::DEV_NET, ConnectProtoType::UB), 0, 1, 0, 1);
    links.push_back(link);

    auto rtsCntNotifys0 = connLocalCntNotifyManager.Get(0);
    EXPECT_EQ(0, rtsCntNotifys0.size());

    connLocalCntNotifyManager.ApplyFor(0, links);
    auto rtsCntNotifys = connLocalCntNotifyManager.Get(0);

    EXPECT_EQ(rtsCntNotifys.size(), 2);

    connLocalCntNotifyManager.ApplyFor(0, links);

    LinkData link2(BasePortType(PortDeploymentType::P2P, ConnectProtoType::RDMA), 0, 1, 0, 1);
    vector<LinkData> links2;
    links2.push_back(link2);
    EXPECT_THROW(connLocalCntNotifyManager.ApplyFor(1, links2), InvalidParamsException);
}
#include "gtest/gtest.h"
#include <stdio.h>
#include <mockcpp/mockcpp.hpp>
#include "../inc/hccl/base.h"
#include <hccl/hccl_types.h>
#include "hccl_comm_pub.h"
#include "hccl_alg.h"
#include "llt_hccl_stub_pub.h"
#include "llt_hccl_stub.h"
#include "dlra_function.h"
#include "sal.h"
#include <externalinput_pub.h>
#include "tsd/tsd_client.h"
#include "dltdt_function.h"
#define private public
#define protected public
#include "network_manager_pub.h"
#undef private
#undef protected

using namespace std;
using namespace hccl;

#define SOCKET_ROLE_SERVER 0    /* server的角色 */
#define SOCKET_ROLE_CLIENT 1    /* client的角色 */

class ExchangerNetworkTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "\033[36m--ExchangerNetworkTest SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "\033[36m--ExchangerNetworkTest TearDown--\033[0m" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        static s32  call_cnt = 0;
        string name =std::to_string(call_cnt++) +"_" + __PRETTY_FUNCTION__;
        ra_set_shm_name(name .c_str());
        DlTdtFunction::GetInstance().DlTdtFunctionInit();
        TsdOpen(1,2);
        std::cout << "A Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        TsdClose(1);
        std::cout << "A Test TearDown" << std::endl;
    }
};

TEST_F(ExchangerNetworkTest, ut_NetworkManager_GetInstance)
{
    s32 device_id = 64;
    NetworkManager::GetInstance(device_id);
}

TEST_F(ExchangerNetworkTest, ut_MemNameRepository_GetInstance)
{
    s32 device_id = 64;
    MemNameRepository::GetInstance(device_id);
}

TEST_F(ExchangerNetworkTest, ut_CloseHccpSubProc)
{
    NetworkManager::GetInstance(10).isHostUseDevNic_ = true;
    NetworkManager::GetInstance(10).subPid_ = 1;

    NetworkManager::GetInstance(10).CloseHccpSubProc();

    NetworkManager::GetInstance(10).isHostUseDevNic_ = false;
    NetworkManager::GetInstance(10).subPid_ = 0;

    NetworkManager::GetInstance(10).isRaInitRepeated_ = true;
    RdmaHandle nicRdmaHandle;
    const HcclIpAddress ipAddr;
    HcclIpAddress backIpAddr;
    NetworkMode netMode;
    NotifyTypeT notifyType_;
    NetworkManager::GetInstance(10).InitRDMA(10, ipAddr, netMode, notifyType_, nicRdmaHandle, false,
                false, backIpAddr);

    //StartListenSocket
    MOCKER(hrtRaSocketListenStart).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    NetworkManager netManager;
    NetworkManager::GetInstance(10).isRaInitRepeated_ = false;
    SocketHandle socketHandle;
    u32 port = 0;
    HcclResult ret ;
    ret = NetworkManager::GetInstance(10).StartListenSocket(socketHandle, port);
    EXPECT_EQ(ret, HCCL_E_NOT_SUPPORT);
}
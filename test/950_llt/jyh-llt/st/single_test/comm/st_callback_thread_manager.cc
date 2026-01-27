#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>

#include <stdio.h>
#include <stdlib.h>
#include <runtime/rt.h>
#include <assert.h>
#include <securec.h>
#include <ifaddrs.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string>
#include <sys/types.h>
#include <stddef.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "dlra_function.h"

#define private public
#define protected public
#include "hccl_impl.h"
#include "hccl_comm_pub.h"
#include "hccl_communicator.h"
#include "transport_heterog.h"
#include "transport_heterog_roce_pub.h"
#include "transport_roce_pub.h"
#undef private

#include <hccl/hccl.h>
#include <hccl/hccl_ex.h>
#include "llt_hccl_stub_pub.h"
#include "llt_hccl_stub_gdr.h"
#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>
#include "../inc/hccl/base.h"
#include "hcom.h"
#include "../inc/hccl/hcom_executor.h"
#include <hccl/hccl_types.h>
#include "topoinfo_ranktableParser_pub.h"
#include "v80_rank_table.h"
#include "tsd/tsd_client.h"
#include "dltdt_function.h"
#include "externalinput_pub.h"
#include "rank_consistentcy_checker.h"
#include "dlhal_function.h"
#include "callback_thread_manager.h"
#include "dispatcher.h"
#include "dispatcher_pub.h"
#include "externalinput.h"
using namespace std;
using namespace hccl;

class MPI_ThreadStreamManager_Test : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "MPI_ThreadStreamManager_Test SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "MPI_ThreadStreamManager_Test TearDown" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        hrtSetDevice(0);
        ResetInitState();
        DlRaFunction::GetInstance().DlRaFunctionInit();
        ClearHalEvent();
        s32 portNum = -1;
        MOCKER(hrtGetHccsPortNum)
            .stubs()
            .with(any(), outBound(portNum))
            .will(returnValue(HCCL_SUCCESS));
        std::cout << "A TestCase SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "A TestCase TearDown" << std::endl;
    }
};

TEST_F(MPI_ThreadStreamManager_Test, st_thread_stream_manager)
{
    HCCL_INFO("st_thread_stream_manager start....");
    int i = 1;
    int j = 2;
    rtStream_t key = NULL;
    bool result = true;
    HcclResult ret = HCCL_SUCCESS;

    result = ThreadStreamManager::Instance().StreamHasBeenReged(&i);
    EXPECT_EQ(result, false);
    ret = ThreadStreamManager::Instance().RegTidAndStream(i, &i);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    result = ThreadStreamManager::Instance().StreamHasBeenReged(&i);
    EXPECT_EQ(result, true);
    ret = ThreadStreamManager::Instance().GetStreamByTid(i, key);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ThreadStreamManager::Instance().ReleaseTidAndStream(key);
    ret = ThreadStreamManager::Instance().GetStreamByTid(10, key);
    EXPECT_EQ(ret, HCCL_E_NOT_FOUND);
    GlobalMockObject::verify();
}

TEST_F(MPI_ThreadStreamManager_Test, st_CallbackUnRegStream)
{
    s32 ret = HCCL_SUCCESS;
    int i = 1;
    std::unique_ptr<HcclCommunicator> impl;
    impl.reset(new (std::nothrow) HcclCommunicator());

    impl->callbackStreamMap_[1].push_back(&i);

    MOCKER(&HcclGetCallbackResult)
    .stubs()
    .will(returnValue(HCCL_E_NETWORK));
    impl->callbackTask_.reset(new (std::nothrow) HcclCallbackTask(0, 0,
        nullptr, NICDeployment::NIC_DEPLOYMENT_HOST));
    ret = impl->callbackTask_->CallbackRegStream(&i);
    EXPECT_EQ(ret, HCCL_E_NETWORK);

    GlobalMockObject::verify();

    impl->callbackThreadId_ = 1;
    MOCKER(&HcclGetCallbackResult)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&ThreadStreamManager::StreamHasBeenReged)
    .stubs()
    .will(returnValue(true));
    ret = impl->callbackTask_->CallbackRegStream(&i);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

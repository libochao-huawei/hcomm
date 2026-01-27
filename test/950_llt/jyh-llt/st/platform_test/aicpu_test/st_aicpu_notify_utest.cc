#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <stdio.h>

#include "../inc/hccl/base.h"
#include <hccl/hccl_types.h>

#ifndef private
#define private public
#define protected public
#endif

#include "local_notify.h"
#include "local_notify_impl.h"
#include "dlhal_function.h"
#include "remote_notify.h"
#include "notify_base.h"
#include "rts_notify.h"
#include "local_ipc_notify.h"
#include "mem_host_pub.h"
#include "mem_device_pub.h"
#include "sal.h"

#include "adapter_rts.h"

#undef private
#undef protected

using namespace std;
using namespace hccl;

class NotifyAiCpu_ST : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "\033[36m--NotifyAiCpu_ST SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "\033[36m--NotifyAiCpu_ST TearDown--\033[0m" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        notifyInfo.addr = 100;
        notifyInfo.devId = 1;
        notifyInfo.rankId = 2;
        notifyInfo.resId = 3;
        notifyInfo.tsId = 4;
        s32 portNum = 7;
        MOCKER(hrtGetHccsPortNum)
            .stubs()
            .with(any(), outBound(portNum))
            .will(returnValue(HCCL_SUCCESS));
        std::cout << "A Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        std::cout << "A Test TearDown" << std::endl;
        GlobalMockObject::verify();
    }

    HcclSignalInfo notifyInfo;
};

TEST_F(NotifyAiCpu_ST, init_with_signal_info)
{
    s32 ret = HCCL_SUCCESS;
    
    // local notify
    LocalNotify localNotify;
    ret = localNotify.Init(notifyInfo, NotifyLoadType::DEVICE_NOTIFY);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_FALSE(localNotify.notifyOwner_);
    
    // remote notify
    RemoteNotify remoteNotify;
    ret = remoteNotify.Init(notifyInfo, NotifyLoadType::DEVICE_NOTIFY);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    // local ipc notify
    LocalIpcNotify localIpcNotify;
    ret = localIpcNotify.Init(notifyInfo, NotifyLoadType::DEVICE_NOTIFY);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_FALSE(localIpcNotify.notifyOwner_);
}

TEST_F(NotifyAiCpu_ST, check_signal_info)
{
    s32 ret = HCCL_SUCCESS;
    HcclSignalInfo notifyInfoGotten;
    
    // local notify
    LocalNotify localNotify;
    localNotify.Init(notifyInfo, NotifyLoadType::DEVICE_NOTIFY);
    localNotify.GetNotifyData(notifyInfoGotten);
    EXPECT_EQ(notifyInfoGotten.addr, notifyInfo.addr);
    EXPECT_EQ(notifyInfoGotten.devId, notifyInfo.devId);
    EXPECT_EQ(notifyInfoGotten.resId, notifyInfo.resId);
    EXPECT_EQ(notifyInfoGotten.tsId, notifyInfo.tsId);
    
    // remote notify
    RemoteNotify remoteNotify;
    remoteNotify.Init(notifyInfo, NotifyLoadType::DEVICE_NOTIFY);
    notifyInfo.tsId = 400;
    remoteNotify.SetNotifyData(notifyInfo);
    remoteNotify.GetNotifyData(notifyInfoGotten);
    EXPECT_EQ(notifyInfoGotten.tsId, u32(400));
}

TEST_F(NotifyAiCpu_ST, init_esched_notify)
{
    // local notify
    LocalNotifyImpl localNotifyImpl;
    localNotifyImpl.Init(HOST_DEVICE_ID, 1, NotifyLoadType::HOST_NOTIFY);

    hccl::DlHalFunction::GetInstance().DlHalFunctionInit();
    void *dispatcher = nullptr;

    HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, 0, &dispatcher);

    Stream stream(false, RT_STREAM_FAST_LAUNCH | RT_STREAM_FAST_SYNC);
    localNotifyImpl.Wait(stream, dispatcher, INVALID_VALUE_STAGE, 0xffffffff);
    HcclDispatcherDestroy(dispatcher);
    localNotifyImpl.Destroy();
}
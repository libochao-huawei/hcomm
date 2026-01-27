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
#include "network_manager_pub.h"
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
#include "externalinput.h"

using namespace std;
using namespace hccl;

class TRANSPORT_ROCE_TEST : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "TRANSPORT_ROCE_TEST SetUP" << std::endl;
        mrManager.reset(new (std::nothrow) MrManager());
        pMsgInfosMem.reset(new (std::nothrow) LocklessRingMemoryAllocate<HcclMessageInfo>(100));
        if (pMsgInfosMem == nullptr) return;
        pMsgInfosMem->Init();

        pReqInfosMem.reset(new (std::nothrow) LocklessRingMemoryAllocate<HcclRequestInfo>(100));
        if (pReqInfosMem == nullptr) return;
        pReqInfosMem->Init();

        memBlocksManager.reset(new (std::nothrow) HeterogMemBlocksManager());
        if (memBlocksManager == nullptr) return;
        memBlocksManager->Init(MEM_BLOCK_NUM);

        pRecvWrInfosMem.reset(new (std::nothrow) LocklessRingMemoryAllocate<RecvWrInfo>(100));
        if (pRecvWrInfosMem == nullptr) return;
        pRecvWrInfosMem->Init();
    }
    static void TearDownTestCase()
    {
        std::cout << "TRANSPORT_ROCE_TEST TearDown" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        s32 portNum = 7;
        MOCKER(hrtGetHccsPortNum)
            .stubs()
            .with(any(), outBound(portNum))
            .will(returnValue(HCCL_SUCCESS));
        hrtSetDevice(0);
        ResetInitState();
        DlRaFunction::GetInstance().DlRaFunctionInit();
        ClearHalEvent();

        std::cout << "A TestCase SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "A TestCase TearDown" << std::endl;
    }
    static std::unique_ptr<MrManager> mrManager;
    static std::unique_ptr<LocklessRingMemoryAllocate<HcclMessageInfo>> pMsgInfosMem;
    static std::unique_ptr<LocklessRingMemoryAllocate<HcclRequestInfo>> pReqInfosMem;
    static std::unique_ptr<HeterogMemBlocksManager> memBlocksManager;
    static std::unique_ptr<LocklessRingMemoryAllocate<RecvWrInfo>> pRecvWrInfosMem;
    TransportResourceInfo transportResourceInfo = TransportResourceInfo(mrManager, pMsgInfosMem, pReqInfosMem,
        memBlocksManager, pRecvWrInfosMem);
};
std::unique_ptr<MrManager> TRANSPORT_ROCE_TEST::mrManager = nullptr;
std::unique_ptr<LocklessRingMemoryAllocate<HcclMessageInfo>> TRANSPORT_ROCE_TEST::pMsgInfosMem = nullptr;
std::unique_ptr<LocklessRingMemoryAllocate<HcclRequestInfo>> TRANSPORT_ROCE_TEST::pReqInfosMem = nullptr;
std::unique_ptr<HeterogMemBlocksManager> TRANSPORT_ROCE_TEST::memBlocksManager = nullptr;
std::unique_ptr<LocklessRingMemoryAllocate<RecvWrInfo>> TRANSPORT_ROCE_TEST::pRecvWrInfosMem = nullptr;


TEST_F(TRANSPORT_ROCE_TEST, ut_TransportRoce_Batchsendrecv)
{
    HcclIpAddress invalidIp;

    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    TransportRoce roce(nullptr, nullptr, machinePara, timeout, invalidIp, invalidIp, 18000, 18000, transportResourceInfo);

    Stream stream;
    HcclResult ret = roce.TxPrepare(stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = roce.RxPrepare(stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = roce.TxDone(stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = roce.RxDone(stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}
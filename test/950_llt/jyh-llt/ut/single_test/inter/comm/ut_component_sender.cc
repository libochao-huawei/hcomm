#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <cstdio>

#include "../inc/hccl/base.h"
#include <hccl/hccl_types.h>

#include "sender_pub.h"
#include "transport_base_pub.h"

using namespace std;
using namespace hccl;

class SenderTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        s32 ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, 0, &dispatcherPtr);
        if (ret != HCCL_SUCCESS) return;
        if (dispatcherPtr == nullptr) return;
        dispatcher = reinterpret_cast<DispatcherPub*>(dispatcherPtr);
        std::cout << "\033[36m--SenderTest SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        if (dispatcherPtr != nullptr) {
            s32 ret = HcclDispatcherDestroy(dispatcherPtr);
            EXPECT_EQ(ret, HCCL_SUCCESS);
            dispatcherPtr = nullptr;
            dispatcher = nullptr;
        }
        std::cout << "\033[36m--SenderTest TearDown--\033[0m" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
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
    }
    static HcclDispatcher dispatcherPtr;
    static DispatcherPub *dispatcher;

};
HcclDispatcher SenderTest::dispatcherPtr = nullptr;
DispatcherPub *SenderTest::dispatcher = nullptr;
#if 1
TEST_F(SenderTest, destructor_D0)
{
    s32 ret = HCCL_SUCCESS;

    Sender* sender = new Sender(HCCL_DATA_TYPE_FP32, HCCL_REDUCE_SUM, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    delete sender;
}
#endif
#if 1
TEST_F(SenderTest, run)
{
    s32 ret = HCCL_SUCCESS;
    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;

    std::shared_ptr<Transport> link (new Transport(new (std::nothrow) TransportBase(
        dispatcher, nullptr, machinePara, timeout)));
    link->Init();

    Sender* sender = new Sender(HCCL_DATA_TYPE_FP32, HCCL_REDUCE_SUM, 1);

    DeviceMem src =  DeviceMem::alloc(10);
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    ret = sender->run(link, 0, src, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    delete sender;
}
#endif

#if 1
TEST_F(SenderTest, run_multi)
{
    s32 ret = HCCL_SUCCESS;
    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;

    std::shared_ptr<Transport> link (new Transport(new (std::nothrow) TransportBase(
        dispatcher, nullptr, machinePara, timeout)));
    link->Init();

    Sender* sender = new Sender(HCCL_DATA_TYPE_FP32, HCCL_REDUCE_SUM, 1);

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);
    DeviceMem src =  DeviceMem::alloc(10);

    std::vector<SenderMemoryInfo> senderMems;
    senderMems.emplace_back(SenderMemoryInfo{0, src});

    ret = sender->run(link, senderMems, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    delete sender;
}
#endif

#if 1
class SenderTestLink : public Transport
{
public:
    explicit SenderTestLink(HcclDispatcher dispatcher,
                        MachinePara& machine_para, std::chrono::milliseconds timeout);
    virtual ~SenderTestLink();

    bool isSupportTxReduce()
    {
        return true;
    }
};

SenderTestLink::SenderTestLink(HcclDispatcher dispatcher,
                        MachinePara& machine_para, std::chrono::milliseconds timeout)
    : Transport(new (std::nothrow) TransportBase(reinterpret_cast<DispatcherPub*>(dispatcher), nullptr, machine_para, timeout))
{

}

SenderTestLink::~SenderTestLink()
{

}

TEST_F(SenderTest, run_link_support_txWithReduce)
{
    s32 ret = HCCL_SUCCESS;
    MachinePara machinePara;
    std::chrono::milliseconds timeout;

    TransportBase transport(reinterpret_cast<DispatcherPub*>(dispatcher), nullptr, machinePara, timeout);
    MOCKER_CPP_VIRTUAL(transport, &TransportBase::TxWithReduce, HcclResult(TransportBase::*)(const std::vector<TxMemoryInfo> &, const HcclDataType, HcclReduceOp, Stream &))
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP_VIRTUAL(transport, &TransportBase::TxDataSignal)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP_VIRTUAL(transport, &TransportBase::TxAsync, HcclResult(TransportBase::*)(std::vector<TxMemoryInfo>&, Stream &))
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    std::shared_ptr<Transport> link;
    link.reset(new (std::nothrow) SenderTestLink(dispatcher, machinePara, timeout));

    Sender sender(HCCL_DATA_TYPE_FP32, HCCL_REDUCE_SUM, RDMA_REDUCE_BITMASK);

    DeviceMem src =  DeviceMem::alloc(10);
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    ret = sender.run(link, 0, src, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

#if 1
TEST_F(SenderTest, run_link_support_txWithReduce_multi)
{
    s32 ret = HCCL_SUCCESS;
    MachinePara machinePara;
    std::chrono::milliseconds timeout;

    std::shared_ptr<Transport> link;
    link.reset(new (std::nothrow) SenderTestLink(dispatcher, machinePara, timeout));

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);
    DeviceMem src =  DeviceMem::alloc(10);
    std::vector<SenderMemoryInfo> senderMems;
    senderMems.emplace_back(SenderMemoryInfo{0, src});

    Sender* sender = new Sender(HCCL_DATA_TYPE_FP32, HCCL_REDUCE_SUM, RDMA_REDUCE_BITMASK);
    ret = sender->run(link, senderMems, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    delete sender;

    Sender* sender1 = new Sender(HCCL_DATA_TYPE_FP32, HCCL_REDUCE_SUM, INLINE_REDUCE_BITMASK);
    ret = sender1->run(link, senderMems, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    delete sender1;
}
#endif

#endif

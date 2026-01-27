#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <cstdio>

#include "../inc/hccl/base.h"
#include <hccl/hccl_types.h>
#include "llt_hccl_stub_pub.h"

#include "comm_base_pub.h"

#define private public
#define protected public
#include "all_reduce_nhr_pub.h"
#include "all_reduce_nhr_oneshot_pub.h"
#undef private
#undef protected

#include "sal.h"
#include "comm_ring_pub.h"
#include "alg_template_base_pub.h"
#include "alg_template_register.h"

using namespace std;
using namespace hccl;

class StubTransportBase : public TransportBase
{
public:
    StubTransportBase(const HcclDispatcher dispatcher,
    MachinePara &machinePara,
    std::chrono::milliseconds timeout, void *remotePtr)
    : TransportBase(reinterpret_cast<DispatcherPub*>(dispatcher), nullptr, machinePara, timeout), remotePtr_(remotePtr)
    {
    }

    HcclResult GetRemoteMem(UserMemType memType, void **remotePtr) {
        *remotePtr = remotePtr_;
        return HCCL_SUCCESS;
    }
private:
    void *remotePtr_;
};

class AllReduceNHRInterTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        s32 ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, 0, &dispatcherPtr);
        if (ret != HCCL_SUCCESS) return;
        if (dispatcherPtr == nullptr) return;
        dispatcher = reinterpret_cast<DispatcherPub*>(dispatcherPtr);
        std::cout << "\033[36m--AllReduceNHRInterTest SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        if (dispatcherPtr != nullptr) {
            s32 ret = HcclDispatcherDestroy(dispatcherPtr);
            EXPECT_EQ(ret, HCCL_SUCCESS);
            dispatcherPtr = nullptr;
            dispatcher = nullptr;
        }
        std::cout << "\033[36m--AllReduceNHRInterTest TearDown--\033[0m" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        s32 portNum = -1;
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
HcclDispatcher AllReduceNHRInterTest::dispatcherPtr = nullptr;
DispatcherPub *AllReduceNHRInterTest::dispatcher = nullptr;

TEST_F(AllReduceNHRInterTest, AllReduceNHROneshot_Constructor)
{
     u64 reduceAttrBitMap = 0x01;

    // 获取模板实例
    std::unique_ptr<AlgTemplateBase> tempAlg = AlgTemplateRegistry::Instance().GetAlgTemplate(
        TemplateType::TEMPLATE_ALL_REDUCE_NHR_ONESHOT, dispatcher);
	EXPECT_NE(tempAlg, nullptr);

	// 调用新增的prepare
    s32 ret = tempAlg->Prepare(reduceAttrBitMap);
    EXPECT_EQ(ret, HCCL_SUCCESS);

	// 转换子类指针
    AllReduceNHROneshot *alg = dynamic_cast<AllReduceNHROneshot *>(tempAlg.get());
	// 校验成员变量
    EXPECT_EQ(alg->reduceAttr_, reduceAttrBitMap);
}

TEST_F(AllReduceNHRInterTest, destructor_D0)
{
     u64 reduceAttrBitMap = 0x01;

    // 获取模板实例
    std::unique_ptr<AlgTemplateBase> tempAlg = AlgTemplateRegistry::Instance().GetAlgTemplate(
        TemplateType::TEMPLATE_ALL_REDUCE_NHR, dispatcher);
	EXPECT_NE(tempAlg, nullptr);

	// 调用新增的prepare
    s32 ret = tempAlg->Prepare(reduceAttrBitMap);
    EXPECT_EQ(ret, HCCL_SUCCESS);

	// 转换子类指针
    AllReduceNHR *alg = dynamic_cast<AllReduceNHR *>(tempAlg.get());
	// 校验成员变量
    EXPECT_EQ(alg->reduceAttr_, reduceAttrBitMap);
}

void run_async_test_allreduce_nhr(DispatcherPub *dispatcher, u32 rank, u32 rankSize)
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id = "test_run_async";


    std::vector<RankInfo> para_vector(1);
    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;

    // 中大数据量
    DeviceMem output =  DeviceMem::alloc(131072*2*rankSize);
    DeviceMem input =  DeviceMem::alloc(131072*2*rankSize);
    DeviceMem scratch =  DeviceMem::alloc(131072*2*rankSize);

    s32 nLinks = -1;
    nLinks = (nLinks == -1) ? rankSize : nLinks;
    std::vector< std::shared_ptr<Transport> > link;
    link.resize(rankSize);
    for (int i = 0; i < rankSize; i++)
    {
        link[i].reset(new(std::nothrow) Transport(new (std::nothrow) StubTransportBase(
            dispatcher, machinePara, timeout, input.ptr())));
        link[i]->Init();
    }

    // add coverage
    Transport base(new (std::nothrow) TransportBase( dispatcher, nullptr, machinePara, timeout));
    Stream stream0;
    base.TxWithReduce(UserMemType::INPUT_MEM, 0, nullptr, 0, HcclDataType::HCCL_DATA_TYPE_INT32,
        HcclReduceOp::HCCL_REDUCE_SUM, stream0);

    u64 reduceAttrBitMap = 0x01;
    AllReduceNHR * tempAlg = new AllReduceNHR(dispatcher);
    tempAlg->Prepare(reduceAttrBitMap);

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    ret = tempAlg->Prepare(input, output, scratch, 131072, HCCL_DATA_TYPE_INT16, stream, HcclReduceOp::HCCL_REDUCE_SUM,
        0);
    ret = tempAlg->RunAsync(rank, rankSize, link);
    if(nLinks == rankSize) {
        EXPECT_EQ(ret, HCCL_SUCCESS);
    } else {
        EXPECT_NE(ret, HCCL_SUCCESS);
    }

    ret =rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    
    delete tempAlg;
}

void run_async_test_allreduce_nhr_smalldata(DispatcherPub *dispatcher, u32 rank, u32 rankSize)
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id = "test_run_async";

    std::vector<RankInfo> para_vector(1);
    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;

    // 小数据量
    DeviceMem output =  DeviceMem::alloc(128*2*rankSize);

    DeviceMem input =  DeviceMem::alloc(128*2*rankSize);
    DeviceMem scratch =  DeviceMem::alloc(128*2*rankSize);

    s32 nLinks = -1;
    nLinks = (nLinks == -1) ? rankSize : nLinks;
    std::vector< std::shared_ptr<Transport> > link;
    link.resize(rankSize);
    for (int i = 0; i < rankSize; i++)
    {
        link[i].reset(new(std::nothrow) Transport(new (std::nothrow) StubTransportBase(
            dispatcher, machinePara, timeout, input.ptr())));
        link[i]->Init();
    }

    // add coverage
    Transport base(new (std::nothrow) TransportBase( dispatcher, nullptr, machinePara, timeout));
    Stream stream0;
    base.TxWithReduce(UserMemType::INPUT_MEM, 0, nullptr, 0, HcclDataType::HCCL_DATA_TYPE_INT32,
        HcclReduceOp::HCCL_REDUCE_SUM, stream0);

    u64 reduceAttrBitMap = 0x01;
    AllReduceNHROneshot * tempAlg = new AllReduceNHROneshot(dispatcher);
    tempAlg->Prepare(reduceAttrBitMap);

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    ret = tempAlg->Prepare(input, output, scratch, 128, HCCL_DATA_TYPE_INT16, stream, HcclReduceOp::HCCL_REDUCE_SUM,
        0);
    ret = tempAlg->RunAsync(rank, rankSize, link);
    if(nLinks == rankSize) {
        EXPECT_EQ(ret, HCCL_SUCCESS);
    } else {
        EXPECT_NE(ret, HCCL_SUCCESS);
    }

    ret =rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    
    delete tempAlg;
}

#if 1
//对应代码中，ranksize=1
TEST_F(AllReduceNHRInterTest, run_async_00)
{
    run_async_test_allreduce_nhr(dispatcher, /*rank=*/0, /*rankSize=*/1);
}
#endif
#if 1
//对应代码中，ranksize=8
TEST_F(AllReduceNHRInterTest, run_async_01)
{
    run_async_test_allreduce_nhr(dispatcher, /*rank=*/0, /*rankSize=*/8);
}
#endif
#if 1
//对应代码中，ranksize=8
TEST_F(AllReduceNHRInterTest, run_async_02)
{
    run_async_test_allreduce_nhr(dispatcher, /*rank=*/4, /*rankSize=*/8);
}
#endif

#if 1
//对应代码中，ranksize=6
TEST_F(AllReduceNHRInterTest, run_async_03)
{
    run_async_test_allreduce_nhr(dispatcher, /*rank=*/0, /*rankSize=*/6);
}
#endif
#if 1
//对应代码中，ranksize=32
TEST_F(AllReduceNHRInterTest, run_async_04)
{
    run_async_test_allreduce_nhr(dispatcher, /*rank=*/0, /*rankSize=*/32);
}
#endif
#if 1
//对应代码中，ranksize=8
TEST_F(AllReduceNHRInterTest, run_async_05)
{
    run_async_test_allreduce_nhr_smalldata(dispatcher, /*rank=*/0, /*rankSize=*/8);
}
#endif

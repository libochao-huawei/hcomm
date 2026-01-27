#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <stdio.h>

#include "../inc/hccl/base.h"
#include <hccl/hccl_types.h>

#define private public
#define protected public
#include "alg_template_base_pub.h"
#undef private
#undef protected
#include "sal.h"
#include "transport_base_pub.h"
#include "acl/acl_rt.h"

using namespace std;
using namespace hccl;

class ExecutorBaseTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        s32 ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, 0, &dispatcherPtr);
        if (ret != HCCL_SUCCESS) return;
        if (dispatcherPtr == nullptr) return;
        dispatcher = reinterpret_cast<DispatcherPub*>(dispatcherPtr);
        std::cout << "\033[36m--ExecutorBaseTest SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        if (dispatcherPtr != nullptr) {
            s32 ret = HcclDispatcherDestroy(dispatcherPtr);
            EXPECT_EQ(ret, HCCL_SUCCESS);
            dispatcherPtr = nullptr;
            dispatcher = nullptr;
        }
        std::cout << "\033[36m--ExecutorBaseTest TearDown--\033[0m" << std::endl;
    }

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
HcclDispatcher ExecutorBaseTest::dispatcherPtr = nullptr;
DispatcherPub *ExecutorBaseTest::dispatcher = nullptr;

#if 1
TEST_F(ExecutorBaseTest, destructor_D0)
{
    s32 ret = HCCL_SUCCESS;
    s32 data_size;
    AlgTemplateBase* tempAlg = new AlgTemplateBase(dispatcher);

    data_size = tempAlg->DataUnitSize(HCCL_DATA_TYPE_INT8);
    EXPECT_EQ(data_size, 1);
    data_size = tempAlg->DataUnitSize(HCCL_DATA_TYPE_INT32);
    EXPECT_EQ(data_size, 4);
    data_size = tempAlg->DataUnitSize(HCCL_DATA_TYPE_FP16);
    EXPECT_EQ(data_size, 2);
    data_size = tempAlg->DataUnitSize(HCCL_DATA_TYPE_FP32);
    EXPECT_EQ(data_size, 4);
    data_size = tempAlg->DataUnitSize(HCCL_DATA_TYPE_RESERVED);
    EXPECT_EQ(data_size, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::vector<bool> temp_0 = tempAlg->CalcLinksRelation(0, 13);
    std::vector<bool> temp_1 = tempAlg->CalcLinksRelation(12, 13);

    delete tempAlg;
}
#endif

#if 1
//input和output不一样
TEST_F(ExecutorBaseTest, prepare01)
{
    s32 ret = HCCL_SUCCESS;
    s32 data_size;
    AlgTemplateBase* tempAlg = new AlgTemplateBase(dispatcher);

    DeviceMem input = DeviceMem::alloc(1);
    DeviceMem output = DeviceMem::alloc(1);
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);
    ret = tempAlg->Prepare(input, output, 1, HCCL_DATA_TYPE_RESERVED, stream, HCCL_REDUCE_RESERVED, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = tempAlg->RegisterProfiler(0, 0, 0, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    delete tempAlg;
}
#endif
#if 1
//  只带入一个inputmem,slice不为空
TEST_F(ExecutorBaseTest, prepare02)
{
    s32 ret = HCCL_SUCCESS;
    s32 data_size;
    AlgTemplateBase* tempAlg = new AlgTemplateBase(dispatcher);

    DeviceMem input = DeviceMem::alloc(1);
    DeviceMem output = DeviceMem::alloc(1);

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    std::vector<Slice> slice;
    Slice slice1;
    slice1.size = 0;
    slice1.offset = 0;

    slice.push_back(slice1);
    ret = tempAlg->Prepare(input, output, 1, HCCL_DATA_TYPE_RESERVED, stream, HCCL_REDUCE_RESERVED, 0, slice);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    delete tempAlg;
}
#endif

#if 1
// 带入slice vector的用例，一个rank的,2个mem，slice不为空
TEST_F(ExecutorBaseTest, prepare03)
{
    s32 ret = HCCL_SUCCESS;
    s32 data_size;
    AlgTemplateBase* tempAlg = new AlgTemplateBase(dispatcher);

    DeviceMem input = DeviceMem::alloc(1);
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);
    std::vector<Slice> slice;
    Slice slice1;
    slice1.size = 0;
    slice1.offset = 0;

    slice.push_back(slice1);
    ret = tempAlg->Prepare(input, input, 1, HCCL_DATA_TYPE_RESERVED, stream, HCCL_REDUCE_RESERVED, 0, slice);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    delete tempAlg;
}
#endif
#if 1
TEST_F(ExecutorBaseTest, run_async)
{
    s32 ret = HCCL_SUCCESS;
    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;
    AlgTemplateBase* tempAlg = new AlgTemplateBase(dispatcher);

    std::vector< std::shared_ptr<Transport> > link;
    link.resize(1);

    for (int i = 0; i < 1; i++)
    {
        link[i].reset(new(std::nothrow) Transport(new (std::nothrow) TransportBase(
            dispatcher, nullptr, machinePara, timeout)));
        link[i]->Init();
    }

    ret = tempAlg->RunAsync(0, 1, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = tempAlg->PrepareRunAsync(0, 1, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    delete tempAlg;
}
#endif
#if 1
TEST_F(ExecutorBaseTest, prepare_error)
{
    s32 ret = HCCL_SUCCESS;

    AlgTemplateBase* tempAlg = new AlgTemplateBase(dispatcher);

    DeviceMem inputMem;
    DeviceMem outputMem;
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);
    std::vector<Slice> slices;

    MOCKER(aclrtStreamGetId)
    .expects(atMost(2))
    .will(returnValue(1));

    ret = tempAlg->Prepare(inputMem, outputMem, outputMem, 0, HCCL_DATA_TYPE_FP32,
                            stream, HCCL_REDUCE_SUM, -1, slices, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = tempAlg->Prepare(inputMem, outputMem, 0, HCCL_DATA_TYPE_FP32,
                            stream, HCCL_REDUCE_SUM, -1, slices, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = tempAlg->RegisterProfiler(0, 0, 0, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    GlobalMockObject::verify();

    delete tempAlg;
}
#endif

#if 1
TEST_F(ExecutorBaseTest, calculate_links_relation_00)
{
    s32 ret = HCCL_SUCCESS;
    std::vector<bool> linkRelation;
    s32 rank = 0;
    s32 rankSize = 13;
    s32 rootRank = 0;
    HalvingDoublingType type = HalvingDoublingType::RESERVED_ALGORITHM_TYPE;
    linkRelation = AlgTemplateBase::CalcLinksRelation(rank,rankSize,rootRank,type);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
#endif

#if 1
TEST_F(ExecutorBaseTest, calculate_links_relation_01)
{
    s32 ret = HCCL_SUCCESS;
    std::vector<bool> linkRelation;
    s32 rank = 1;
    s32 rankSize = 13;
    s32 rootRank = 0;
    HalvingDoublingType type = HalvingDoublingType::RECURSIVE_HALVING_DOUBLING;
    linkRelation = AlgTemplateBase::CalcLinksRelation(rank,rankSize,rootRank,type);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
#endif

#if 1
TEST_F(ExecutorBaseTest, calculate_links_relation_02)
{
    s32 ret = HCCL_SUCCESS;
    std::vector<bool> linkRelation;
    s32 rank = 1;
    s32 rankSize = 13;
    s32 rootRank = 1;
    HalvingDoublingType type = HalvingDoublingType::RECURSIVE_HALVING_DOUBLING;
    linkRelation = AlgTemplateBase::CalcLinksRelation(rank,rankSize,rootRank,type);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
#endif

#if 1
TEST_F(ExecutorBaseTest, calculate_links_relation_03)
{
    s32 ret = HCCL_SUCCESS;
    std::vector<bool> linkRelation;
    s32 rank = 0;
    s32 rankSize = 13;
    s32 rootRank = 1;
    HalvingDoublingType type = HalvingDoublingType::RECURSIVE_HALVING_DOUBLING;
    linkRelation = AlgTemplateBase::CalcLinksRelation(rank,rankSize,rootRank,type);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
#endif

#if 1
TEST_F(ExecutorBaseTest, calculate_links_relation_04)
{
    s32 ret = HCCL_SUCCESS;
    std::vector<bool> linkRelation;
    s32 rank = 0;
    s32 rankSize = 13;
    s32 rootRank = 10;
    HalvingDoublingType type = HalvingDoublingType::RECURSIVE_HALVING_DOUBLING;
    linkRelation = AlgTemplateBase::CalcLinksRelation(rank,rankSize,rootRank,type);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
#endif

#if 1
TEST_F(ExecutorBaseTest, calculate_links_relation_05)
{
    s32 ret = HCCL_SUCCESS;
    std::vector<bool> linkRelation;
    s32 rank = 11;
    s32 rankSize = 13;
    s32 rootRank = 10;
    HalvingDoublingType type = HalvingDoublingType::RECURSIVE_HALVING_DOUBLING;
    linkRelation = AlgTemplateBase::CalcLinksRelation(rank,rankSize,rootRank,type);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
#endif

#if 1
TEST_F(ExecutorBaseTest, calculate_links_relation_06)
{
    s32 ret = HCCL_SUCCESS;
    std::vector<bool> linkRelation;
    s32 rank = 11;
    s32 rankSize = 13;
    s32 rootRank = -1;
    HalvingDoublingType type = HalvingDoublingType::RECURSIVE_HALVING_DOUBLING;
    linkRelation = AlgTemplateBase::CalcLinksRelation(rank,rankSize,rootRank,type);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
#endif


#if 1
TEST_F(ExecutorBaseTest, calculate_links_relation_07)
{
    s32 ret = HCCL_SUCCESS;
    std::vector<bool> linkRelation;
    s32 rank = 11;
    s32 rankSize = 13;
    s32 rootRank = 1;
    HalvingDoublingType type = HalvingDoublingType::RECURSIVE_HALVING_DOUBLING;
    linkRelation = AlgTemplateBase::CalcLinksRelation(rank,rankSize,rootRank,type);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
#endif


#if 1
TEST_F(ExecutorBaseTest, calculate_links_relation_08)
{
    s32 ret = HCCL_SUCCESS;
    std::vector<bool> linkRelation;
    s32 rank = 11;
    s32 rankSize = 13;
    s32 rootRank = 1;

    MOCKER(&AlgTemplateBase::CalcBinaryBlockHalvingDoubleLinkReleation)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_E_PARA));

    HalvingDoublingType type = HalvingDoublingType::BINARY_BLOCK_HALVING_DOUBLING;
    linkRelation = AlgTemplateBase::CalcLinksRelation(rank,rankSize,rootRank,type);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
#endif

#if 1
TEST_F(ExecutorBaseTest, calculate_links_relation_09)
{
    s32 ret = HCCL_SUCCESS;
    std::vector<bool> linkRelation;
    s32 rank = 11;
    s32 rankSize = 13;
    s32 rootRank = 1;

    MOCKER(&AlgTemplateBase::CalcBinaryBlockHalvingDoubleLinkReleation)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_E_PARA));

    HalvingDoublingType type = HalvingDoublingType::RESERVED_ALGORITHM_TYPE;
    linkRelation = AlgTemplateBase::CalcLinksRelation(rank,rankSize,rootRank,type);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
#endif

TEST_F(ExecutorBaseTest, ut_ExecEmptyTask)
{
    s32 ret = HCCL_SUCCESS;
    DeviceMem input = DeviceMem::alloc(1);
    DeviceMem output = DeviceMem::alloc(1);
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);
    ret = AlgTemplateBase::ExecEmptyTask(input, output, stream, dispatcher);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <stdio.h>

#include "../inc/hccl/base.h"
#include <hccl/hccl_types.h>
#include "llt_hccl_stub_pub.h"
#include "alg_configurator.h"


#define private public
#define protected public
#include "coll_alg_op_registry.h"
#undef private
#undef protected
using namespace std;
using namespace hccl;

class CollRegistryTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "\033[36m--CollRegistryTest SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "\033[36m--CollRegistryTest TearDown--\033[0m" << std::endl;
    }
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
        GlobalMockObject::verify();
        std::cout << "A Test TearDown" << std::endl;
    }
};

TEST_F(CollRegistryTest, get_op_fail)
{
    std::unique_ptr<hcclImpl> pimpl_;
    std::unique_ptr<TopoMatcher> topoMatcher_;
    AlgConfigurator* algConfigurator = nullptr;
    CCLBufferManager cclBufferManager;
    HcclDispatcher dispatcher = nullptr;

    std::unique_ptr<CollAlgOperator> algOperator =
        CollAlgOpRegistry::Instance().GetAlgOp(HcclCMDType::HCCL_CMD_INVALID, algConfigurator, cclBufferManager, dispatcher, topoMatcher_);
    EXPECT_TRUE(algOperator == nullptr);
    GlobalMockObject::verify();
}
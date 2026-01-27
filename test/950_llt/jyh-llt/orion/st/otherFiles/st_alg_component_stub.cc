#include "gtest/gtest.h"
#include <mockcpp/mokc.h>
#include <mockcpp/mockcpp.hpp>
#include "coll_alg_component.h"

#define private public
#include "communicator_impl.h"
#include "base_config.h"
#include "env_config.h"
#include "cfg_field.h"
#undef private

using namespace Hccl;

class CollAlgCompStubTest : public testing::Test {
protected:
    static void SetUpTestCase() {
        std::cout << "CollAlgCompStubTest SetUP" << std::endl;
    }

    static void TearDownTestCase() {
        std::cout << "CollAlgCompStubTest TearDown" << std::endl;
    }

    virtual void SetUp() {
        std::cout << "A Test case in CollAlgCompStubTest SetUP" << std::endl;
    }

    virtual void TearDown () {
        GlobalMockObject::verify();

        std::cout << "A Test case in CollAlgCompStubTest TearDown" << std::endl;
    }
};

TEST_F(CollAlgCompStubTest, build_intra_rank_cntNotify_test_success)
{
    CommunicatorImpl comm;
    CollAlgComponent algComponent(comm.GetRankGraph(), comm.GetDevType(), comm.GetMyRank(), comm.GetRankSize());
    CollAlgOperator op;
    CollAlgParams params;
    PrimQuePtr queue = std::make_shared<PrimQueue>();
    string algName;

    EXPECT_NO_THROW(algComponent.Orchestrate(op, params, algName, queue)); 
}

TEST_F(CollAlgCompStubTest, build_intra_rank_notify_test_success)
{
    CommunicatorImpl comm;
    CollAlgComponent algComponent(comm.GetRankGraph(), comm.GetDevType(), comm.GetMyRank(), comm.GetRankSize());
    CollAlgOperator op;
    CollAlgParams params;
    string algName;

    PrimQuePtr queue = std::make_shared<PrimQueue>();
    EXPECT_NO_THROW(algComponent.Orchestrate(op, params, algName, queue));
}
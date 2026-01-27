#include "gtest/gtest.h"
#include <mockcpp/mokc.h>
#include <mockcpp/mockcpp.hpp>
#include "mc2_context.h"
#include "ccu_ctx_arg_mc2.h"
#include "ccu_task_arg_mc2.h"
#include "ccu_transport.h"
#include "ccu_transport_group.h"

using namespace std;
using namespace Hccl;

class Mc2ContextTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "Mc2ContextTest SetUP" << std::endl;
    }

    static void TearDownTestCase()
    {
        std::cout << "Mc2ContextTest TearDown" << std::endl;
    }

    virtual void SetUp()
    {
        std::cout << "A Test case in Mc2ContextTest SetUP" << std::endl;
    }

    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "A Test case in Mc2ContextTest TearDown" << std::endl;
    }
};

TEST_F(Mc2ContextTest, should_return_success_when_single_die)
{
    Mc2Context mc2Context{};

    mc2Context.SetDieNum(1);    // 设置为单die
    mc2Context.SetCommAddr(0, 1024);    // 设置HBM内存地址
    std::map<uint64_t, uint32_t> algoTemplateInfo {{0, 1}, {1, 2}};
    mc2Context.SetAlgoTemplateInfo(algoTemplateInfo);   // algoTemplateInfo_设置为非空

    EXPECT_NO_THROW(mc2Context.Algorithm());
}

TEST_F(Mc2ContextTest, should_return_success_when_double_die_0)
{
    Mc2Context mc2Context{};

    mc2Context.SetDieNum(2);    // 设置为双die
    mc2Context.SetDieId(0);     // 设置die id
    mc2Context.SetCommAddr(0, 1024);    // 设置HBM内存地址
    std::map<uint64_t, uint32_t> algoTemplateInfo {{0, 1}, {1, 2}};
    mc2Context.SetAlgoTemplateInfo(algoTemplateInfo);   // algoTemplateInfo_设置为非空

    EXPECT_NO_THROW(mc2Context.Algorithm());
}

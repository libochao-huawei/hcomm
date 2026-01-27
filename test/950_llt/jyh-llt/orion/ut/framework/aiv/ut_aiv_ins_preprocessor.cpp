#include "gtest/gtest.h"
#include <mockcpp/mokc.h>
#include <mockcpp/mockcpp.hpp>
#define private public
#define protected public

#include "ccu_instruction_all_gather_mesh1d.h"
#include "aiv_ins_preprocessor.h"

#undef private
#undef protected

using namespace Hccl;

using namespace std;

class AivInsPreprocessorTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "AivInsPreprocessorTest SetUP" << std::endl;
    }

    static void TearDownTestCase()
    {
        std::cout << "AivInsPreprocessorTest TearDown" << std::endl;
    }

    virtual void SetUp()
    {
        std::cout << "A Test case in AivInsPreprocessorTest SetUP" << std::endl;
    }

    virtual void TearDown()
    {
        std::cout << "A Test case in AivInsPreprocessorTest TearDown" << std::endl;
    }
};

TEST_F(AivInsPreprocessorTest, should_continue_when_calling_preprocess_not_aivIns)
{
    CommunicatorImpl *comm;
    AivInsPreprocessor preprocessor(comm);
    auto insQueue = make_shared<InsQueue>();

    std::unique_ptr<CcuInstruction> ccuIns = std::make_unique<CcuInstructionAllGatherMesh1D>();
    insQueue->Append(std::move(ccuIns));
    // check
    EXPECT_NO_THROW(preprocessor.Preprocess(insQueue));
}
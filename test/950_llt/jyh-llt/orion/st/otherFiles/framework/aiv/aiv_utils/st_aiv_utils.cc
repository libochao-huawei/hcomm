#include "gtest/gtest.h"
#include <mockcpp/mokc.h>
#include <mockcpp/mockcpp.hpp>
#define private public
#define protected public
#include "hccl_aiv_utils.h"
#undef private
#undef protected

using namespace Hccl;

using namespace std;

class AivUtilsTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "AivUtilsTest SetUP" << std::endl;
    }

    static void TearDownTestCase()
    {
        std::cout << "AivUtilsTest TearDown" << std::endl;
    }

    virtual void SetUp()
    {
        std::cout << "A Test case in AivUtilsTest SetUP" << std::endl;
    }

    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "A Test case in AivUtilsTest TearDown" << std::endl;
    }
};

TEST_F(AivUtilsTest, aiv_utils_test)
{   
    char resolvePath[] = "./CMakeList.txt";  
    char* path = resolvePath;
    char  returnValueVec[] = "0";
    char* returnValueChar = returnValueVec;
    MOCKER(realpath)
    .stubs()
    .with(any(), outBound(path))
    .will(returnValue(returnValueChar));
    RegisterKernel();

    char libPath[] = "./llt/ace/comop/hccl/stub/workspace/fwkacllib/lib64";
    char* libPathPtr = libPath;

    MOCKER(getenv)
    .stubs()
    .with(any())
    .will(returnValue(libPathPtr));

    MOCKER(ReadBinFile)
    .stubs()
    .with(any())
    .will(returnValue(0));

    MOCKER(realpath)
    .stubs()
    .with(any(), outBound(path))
    .will(returnValue(nullptr));
    RegisterKernel();

    char resolvePath2[] = "./libhccl_v2.so";  
    path = resolvePath2;
    MOCKER(realpath)
    .stubs()
    .with(any(), outBound(path))
    .will(returnValue(returnValueChar));
    RegisterKernel();

    string bufferRead;
    Hccl::ReadBinFile("./llt/ace/comop/hccl/stub/workspace/fwkacllib/lib64/hccl_aiv_op_91095.o", bufferRead);
}
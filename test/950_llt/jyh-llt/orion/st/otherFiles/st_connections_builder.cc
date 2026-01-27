#include "gtest/gtest.h"
#include <mockcpp/mokc.h>
#include <mockcpp/mockcpp.hpp>
#define private public
#include "socket_manager.h"
#include "connections_builder.h"
#include "communicator_impl.h"
#undef private
 
using namespace Hccl;
using namespace std;
 
class ConnectionsBuilderTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "ConnectionsBuilderTest SetUP" << std::endl;
    }
 
    static void TearDownTestCase()
    {
        std::cout << "ConnectionsBuilderTest TearDown" << std::endl;
    }
 
    virtual void SetUp()
    {
        std::cout << "A Test case in ConnectionsBuilderTest SetUP" << std::endl;
    }
 
    virtual void TearDown()
    {
        std::cout << "A Test case in ConnectionsBuilderTest TearDown" << std::endl;
        GlobalMockObject::verify();
    }
};
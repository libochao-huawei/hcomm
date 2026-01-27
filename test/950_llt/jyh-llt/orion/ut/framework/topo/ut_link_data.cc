#include "gtest/gtest.h"
#include <mockcpp/mokc.h>
#include <mockcpp/mockcpp.hpp>
#include "virtual_topo_stub.h"
#include "virtual_topo.h"

using namespace Hccl;

class LinkDataTest : public testing::Test {
protected:
    static void SetUpTestCase() {
        std::cout << "LinkDataTest SetUP" << std::endl;
    }

    static void TearDownTestCase() {
        std::cout << "LinkDataTest TearDown" << std::endl;
    }

    virtual void SetUp() {
        std::cout << "A Test case in LinkDataTest SetUP" << std::endl;
    }

    virtual void TearDown() {
        GlobalMockObject::verify();
        std::cout << "A Test case in LinkDataTest TearDown" << std::endl;
    }
};

TEST_F(LinkDataTest, linkData_get_uniqueId)
{
    LinkData linkData(PortDeploymentType::DEV_NET, LinkProtocol::UB_TP, 0, 1, IpAddress("0.0.0.0"), IpAddress("0.0.0.0"));
    auto     data = linkData.GetUniqueId();

    LinkData linkData1(data);

    EXPECT_EQ(linkData.Describe(), linkData1.Describe());
}

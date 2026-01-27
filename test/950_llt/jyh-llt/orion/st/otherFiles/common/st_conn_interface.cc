#include "gtest/gtest.h"
#include "conn_interface.h"
#include "topo_common_types.h"

using namespace Hccl;

class ConnInterfaceTest : public ::testing::Test {
protected:
    void SetUp() override
    {}
    void TearDown() override
    {}
};

TEST_F(ConnInterfaceTest, ConnInterface_ShouldConstruct_WhenValidParameters)
{
    IpAddress addr;
    AddrPosition pos = AddrPosition::DEVICE;
    LinkType linkType = LinkType::PEER2PEER;
    LinkProtocol linkProtocol = LinkProtocol::UB_CTP;
    ConnInterface conn(addr, pos, linkType, linkProtocol);
    EXPECT_EQ(conn.GetAddr(), addr);
    EXPECT_EQ(conn.GetPos(), pos);
    EXPECT_EQ(conn.GetLinkType(), linkType);
    EXPECT_EQ(conn.GetLinkProtocol(), linkProtocol);
}

TEST_F(ConnInterfaceTest, ConnInterface_ShouldReturnAddress_WhenConstructSuccess)
{
    IpAddress addr;
    AddrPosition pos = AddrPosition::DEVICE;
    LinkType linkType = LinkType::PEER2PEER;
    LinkProtocol linkProtocol = LinkProtocol::UB_CTP;
    ConnInterface conn(addr, pos, linkType, linkProtocol);
    EXPECT_EQ(conn.GetAddr(), addr);
    EXPECT_EQ(conn.GetPos(), pos);
    EXPECT_EQ(conn.GetLinkType(), linkType);
    EXPECT_EQ(conn.GetLinkProtocol(), linkProtocol);
}

TEST_F(ConnInterfaceTest, ConnInterface_ShouldReturnPosition_WhenConstructSuccess)
{
    IpAddress addr;
    AddrPosition pos = AddrPosition::DEVICE;
    LinkType linkType = LinkType::PEER2PEER;
    LinkProtocol linkProtocol = LinkProtocol::UB_CTP;
    ConnInterface conn(addr, pos, linkType, linkProtocol);
    EXPECT_EQ(conn.GetAddr(), addr);
    EXPECT_EQ(conn.GetPos(), pos);
    EXPECT_EQ(conn.GetLinkType(), linkType);
    EXPECT_EQ(conn.GetLinkProtocol(), linkProtocol);
}

TEST_F(ConnInterfaceTest, ConnInterface_ShouldReturnLinkType_WhenConstructSuccess)
{
    IpAddress addr;
    AddrPosition pos = AddrPosition::DEVICE;
    LinkType linkType = LinkType::PEER2PEER;
    LinkProtocol linkProtocol = LinkProtocol::UB_CTP;
    ConnInterface conn(addr, pos, linkType, linkProtocol);
    EXPECT_EQ(conn.GetAddr(), addr);
    EXPECT_EQ(conn.GetPos(), pos);
    EXPECT_EQ(conn.GetLinkType(), linkType);
    EXPECT_EQ(conn.GetLinkProtocol(), linkProtocol);
}

TEST_F(ConnInterfaceTest, ConnInterface_ShouldReturnLinkProtocol_WhenConstructSuccess)
{
    IpAddress addr;
    AddrPosition pos = AddrPosition::DEVICE;
    LinkType linkType = LinkType::PEER2PEER;
    LinkProtocol linkProtocol = LinkProtocol::UB_CTP;
    ConnInterface conn(addr, pos, linkType, linkProtocol);
    EXPECT_EQ(conn.GetAddr(), addr);
    EXPECT_EQ(conn.GetPos(), pos);
    EXPECT_EQ(conn.GetLinkType(), linkType);
    EXPECT_EQ(conn.GetLinkProtocol(), linkProtocol);
}

TEST_F(ConnInterfaceTest, ConnInterface_ShouldReturnTrue_WhenOperatorEqualSuccess)
{
    IpAddress addr;
    AddrPosition pos = AddrPosition::DEVICE;
    LinkType linkType = LinkType::PEER2PEER;
    LinkProtocol linkProtocol = LinkProtocol::UB_CTP;
    ConnInterface conn(addr, pos, linkType, linkProtocol);
    ConnInterface conn1(addr, pos, linkType, linkProtocol);
    EXPECT_EQ(conn == conn1, true);
}

TEST_F(ConnInterfaceTest, ConnInterface_ShouldReturnFalse_WhenOperatorEqualFailed)
{
    IpAddress addr;
    AddrPosition pos = AddrPosition::DEVICE;
    LinkType linkType = LinkType::PEER2PEER;
    LinkProtocol linkProtocol = LinkProtocol::UB_CTP;
    ConnInterface conn(addr, pos, linkType, linkProtocol);
    ConnInterface conn1(addr, AddrPosition::HOST, linkType, linkProtocol);
    EXPECT_EQ(conn == conn1, false);
}

TEST_F(ConnInterfaceTest, ConnInterface_ShouldReturnTrue_WhenOperatorNotEqualSuccess)
{
    IpAddress addr;
    AddrPosition pos = AddrPosition::DEVICE;
    LinkType linkType = LinkType::PEER2PEER;
    LinkProtocol linkProtocol = LinkProtocol::UB_CTP;
    ConnInterface conn(addr, pos, linkType, linkProtocol);
    ConnInterface conn1(addr, AddrPosition::HOST, linkType, linkProtocol);
    EXPECT_EQ(conn != conn1, true);
}

TEST_F(ConnInterfaceTest, ConnInterface_ShouldReturnFalse_WhenOperatorNotEqualFailed)
{
    IpAddress addr;
    AddrPosition pos = AddrPosition::DEVICE;
    LinkType linkType = LinkType::PEER2PEER;
    LinkProtocol linkProtocol = LinkProtocol::UB_CTP;
    ConnInterface conn(addr, pos, linkType, linkProtocol);
    ConnInterface conn1(addr, pos, linkType, linkProtocol);
    EXPECT_EQ(conn != conn1, false);
}
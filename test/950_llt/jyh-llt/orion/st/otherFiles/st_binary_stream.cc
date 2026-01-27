#include <cstring>
#include <iostream>
#include <mockcpp/mokc.h>
#include <mockcpp/mockcpp.hpp>

#include "gtest/gtest.h"
#include "binary_stream.h"

using namespace Hccl;

class BinaryStreamTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "BinaryStream tests set up." << std::endl;
    }

    static void TearDownTestCase()
    {
        std::cout << "BinaryStream tests tear down." << std::endl;
    }

    virtual void SetUp()
    {
        std::cout << "A Test case in BinaryStream SetUP" << std::endl;
    }

    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "A Test case in BinaryStream TearDown" << std::endl;
    }
};

TEST_F(BinaryStreamTest, test_binarystream_input)
{
    BinaryStream bs;
    char varChar = 'r', varCharOut;
    unsigned varUChar = 0x78, varUCharOut;
    int varInt = 2344, varIntOut;
    unsigned int varUInt = 3427897, varUIntOut;
    char varArray[8] {"ui83jks"}, varArrayOut[8];

    bs << varChar << varUChar << varInt << varUInt << varArray;

    bs >> varCharOut >> varUCharOut >> varIntOut >> varUIntOut >> varArrayOut;

    EXPECT_EQ(varChar, varCharOut);
    EXPECT_EQ(varUChar, varUCharOut);
    EXPECT_EQ(varInt, varIntOut);
    EXPECT_EQ(varUInt, varUIntOut);
    EXPECT_EQ(strcmp(varArray, varArrayOut), 0);
}

TEST_F(BinaryStreamTest, test_binarystream_dump)
{
    BinaryStream bs;
    char bytes[10]{0x92, 0x3e, 0x8e, 0x45, 0xa7, 0xc3, 0x4d, 0xff, 0x3e, 0xa3};

    for (auto byte : bytes) {
        bs << byte;
    }
    std::vector<char> buf;
    bs.Dump(buf);
    EXPECT_EQ(memcmp(buf.data(), bytes, sizeof(bytes)), 0);
}
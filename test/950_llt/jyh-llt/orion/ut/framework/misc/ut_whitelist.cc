#include "gtest/gtest.h"
#include <mockcpp/mokc.h>
#include <mockcpp/mockcpp.hpp>
#include "whitelist.h"
#include "invalid_params_exception.h"
#include "internal_exception.h"
#include "whitelist_test.h"

using namespace Hccl;

class WhiteListTest : public testing::Test {
protected:
    static void SetUpTestCase() {
        std::cout << "WhiteListTest SetUP" << std::endl;
    }

    static void TearDownTestCase() {
        std::cout << "WhiteListTest TearDown" << std::endl;
    }

    virtual void SetUp() {
        std::cout << "A Test case in WhiteListTest SetUP" << std::endl;
    }

    virtual void TearDown() {
        GlobalMockObject::verify();
        std::cout << "A Test case in WhiteListTest TearDown" << std::endl;
    }

};

TEST_F(WhiteListTest, get_host_whitelist) {
    IpAddress ipAddress("1.0.0.0");
    std::vector<IpAddress> whiteList;
    whiteList.push_back(ipAddress);
    Whitelist::GetInstance().GetHostWhiteList(whiteList);

}

TEST_F(WhiteListTest, whitelist_load_config_file) {
    std::string name;
    EXPECT_THROW(Whitelist::GetInstance().LoadConfigFile(name), InvalidParamsException);

    name = "whitelist";
    EXPECT_THROW(Whitelist::GetInstance().LoadConfigFile(name), InternalException);

    name = "whitelist.json";
    GenWhiteListFile();
    Whitelist::GetInstance().LoadConfigFile(name);

    IpAddress ipAddress("1.0.0.0");
    std::vector<IpAddress> whiteList;
    whiteList.push_back(ipAddress);
    Whitelist::GetInstance().GetHostWhiteList(whiteList);
    DelWhiteListFile();
}
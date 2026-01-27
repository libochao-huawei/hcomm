#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include "adapter_error_manager_pub.h"
#include "adapter_rts.h"

class HcclErrManagerTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "HcclErrManagerTest SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "HcclErrManagerTest TearDown" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        std::cout << "A Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "A Test TearDown" << std::endl;
    }

};

TEST_F(HcclErrManagerTest, st_hccl_error_manager)
{
    bool ret = false;
    int *ptr = new int(10);
    RPT_INNER_ERR_PRT("remote op nic connect failed, please ensure that collective communication execution status "\
        "of each device is consistent(include network TLS configuration)");
    RPT_CALL_ERR(ret, "aclrtIpcMemImportByKey failed. return[%d], ptr[%p], name[%s]", ret, ptr, "name");
    RPT_CALL_ERR_PRT("ra socket batch connect failed. return[%d]", 1);
    delete ptr;
}
